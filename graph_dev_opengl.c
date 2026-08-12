#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <assert.h>
#include <sys/stat.h>
#include <limits.h>
#include <pthread.h>

#include <glad/gl.h>
#include <SDL.h>

#include "arraysize.h"
#include "shader.h"
#include "vertex.h"
#include "triangle.h"
#include "mtwist.h"
#include "mathutils.h"
#include "matrix.h"
#include "quat.h"
#include "mesh.h"
#include "vec4.h"
#include "snis_graph.h"
#include "graph_dev.h"
#include "material.h"
#include "entity.h"
#include "entity_private.h"
#include "star_light.h"
#include "snis_typeface.h"
#include "opengl_cap.h"
#include "png_utils.h"
#include "snis_profile.h"
#include "workqueue.h"
#include "string-utils.h"


#define OPENGL_VERSION_STRING "#version 150\n"
#define UNIVERSAL_SHADER_HEADER \
	OPENGL_VERSION_STRING

/*
 * Filmic tonemapping cribbed from oolite:
 *
 * 	gamma correction
 * 	using Jim Hejl's filmic tonemapping and gamma correction approximation.
 * 	Normally this would require HDR, but I think it works extremely well in Oolite.
 * 	Formula taken from https://www.gdcvault.com/play/1012351/Uncharted-2-HDR
 * 	jump to 27:40 in the video. Note the pow 1.0/2.2 is baked into these numbers
 *
 * Perhaps that it normally requires HDR is the reason it doesn't seem to look so
 * great in SNIS.
 */
#define FILMIC_TONEMAPPING \
	"uniform float u_FilmicTonemapping;\n" \
	"uniform float u_TonemappingGain;\n" \
	"vec4 filmic_tonemap(vec4 color) {\n" \
	"	float dont_tonemap = 1.0 - u_FilmicTonemapping;\n" \
	"	vec3 x = max(vec3(0.0), color.rgb - 0.004);\n" \
	"	x = u_TonemappingGain * (x * (6.2 * x + 0.5)) / (x * (6.2 * x + 1.7) + 0.06);\n" \
	"	return dont_tonemap * color + vec4(u_FilmicTonemapping * x, color.a);\n" \
	"}\n\n"

#define DEBUG_NORMALS 0
#define TEX_RELOAD_DELAY 1.0
#define CUBEMAP_TEX_RELOAD_DELAY 1.0
#define MAX_LOADED_TEXTURES 300

#define IMAGE_LOADER_QUEUE_DEPTH 300
#define IMAGE_LOADER_THREAD_COUNT 4
static struct work_queue *image_loader_wq = NULL; /* queue of requests to load PNG images */
static struct work_queue *loaded_images_wq = NULL; /* queue of decoded image data to upload to GPU */

struct texture_loading_status {
	GLuint texture_id;
	unsigned char finished_loading;
	unsigned char in_use;
};
static struct texture_loading_status texture_load_status[MAX_LOADED_TEXTURES] = { 0 };
pthread_mutex_t finished_loading_mutex = PTHREAD_MUTEX_INITIALIZER;

/* return true if a texture is finished loading. finished_loading_mutex must be held. */
static int texture_finished_loading(GLuint texture_name)
{
	for (int i = 0; i < MAX_LOADED_TEXTURES; i++)
		if (texture_load_status[i].in_use && texture_load_status[i].texture_id == texture_name)
			return texture_load_status[i].finished_loading;
	return 0;
}

static void set_texture_load_status(GLuint texture_name, unsigned char load_status)
{
	/* finished_loading_mutex must be held. */
	int first_unused = -1;
	for (int i = 0; i < MAX_LOADED_TEXTURES; i++) {
		if (first_unused == -1 && texture_load_status[i].in_use == 0) {
			first_unused = i;
			continue;
		}
		if (texture_load_status[i].in_use && texture_load_status[i].texture_id == texture_name) {
			texture_load_status[i].finished_loading = load_status;
			return;
		}
	}
	if (first_unused != -1) {
		texture_load_status[first_unused].texture_id = texture_name;
		texture_load_status[first_unused].in_use = 1;
		texture_load_status[first_unused].finished_loading = load_status;
		return;
	}
	fprintf(stderr, "Too many textures at %s:%d\n", __FILE__, __LINE__);
	abort();
}

static void mark_texture_load_pending(GLuint texture_name)
{
	/* finished_loading_mutex must be held. */
	set_texture_load_status(texture_name, 0);
}

static void mark_texture_load_complete(GLuint texture_name)
{
	/* finished_loading_mutex must be held. */
	set_texture_load_status(texture_name, 1);
}

static void mark_texture_load_unused(GLuint texture_name)
{
	/* finished_loading_mutex must be held. */
	for (int i = 0; i < MAX_LOADED_TEXTURES; i++) {
		if (texture_load_status[i].in_use && texture_load_status[i].texture_id == texture_name) {
			texture_load_status[i].in_use = 0;
			texture_load_status[i].texture_id = -1;
			texture_load_status[i].finished_loading = 0;
			break;
		}
	}
}

struct loaded_texture {
	GLuint texture_id;
	char *filename;
	time_t mtime;
	double last_mtime_change;
	int expired;
	int use_mipmaps;
	int linear_colorspace;
};
static int nloaded_textures = 0;
static struct loaded_texture loaded_textures[MAX_LOADED_TEXTURES];
static char *error_texture_file = NULL;
static int no_texture_mode = 0;

#define NCUBEMAP_TEXTURES 6
#define MAX_LOADED_CUBEMAP_TEXTURES 40
struct loaded_cubemap_texture {
	GLuint texture_id;
	int is_inside;
	char *filename[NCUBEMAP_TEXTURES];
	time_t mtime;
	double last_mtime_change;
	int expired;
	int linear_colorspace;
};
static int nloaded_cubemap_textures = 0;
static struct loaded_cubemap_texture loaded_cubemap_textures[MAX_LOADED_CUBEMAP_TEXTURES];

static int draw_normal_lines = 0;
static int draw_billboard_wireframe = 0;
static int draw_polygon_as_lines = 0;
static int draw_msaa_samples = 0;
static int draw_render_to_texture = 0;
static int draw_smaa = 0;
static int draw_smaa_edge = 0;
static int draw_smaa_blend = 0;
static int draw_atmospheres = 1;
/* How far a planet's shaded side is dimmed relative to everything else on the same shader.
 * u_AmbientColor is sized for hulls and asteroids, where the ambient floor is a legibility
 * affordance -- you have to see the ship you are flying and the rock you are about to hit.
 * A planet is a large airless body whose night side really does go nearly black, and it is
 * never something you need to read detail off, so it can afford the honest answer. */
#define PLANET_AMBIENT_SCALE (2.0f / 3.0f)

static int filmic_tonemapping = 1;
static float tonemapping_gain = 1.18;
int graph_dev_planet_specularity = 1;
int graph_dev_atmosphere_ring_shadows = 1;

/* Cascaded shadow mapping state (Phase 1: a single shadow map). */
int graph_dev_shadow_map_enabled = SHADOW_MAP_ENABLED;

/* graph_dev_planets_receive_csm_shadows: If 1, planets will use the CSM
 * variants of the textured cubemap shaders, if 0, they will use the
 * non-CSM variants.
 */
static int graph_dev_planets_receive_csm_shadows = 0;

static int graph_dev_shadow_map_debug; /* shadow debug visualization mode (SNIS_SHADOW_DEBUG) */
/* PCF kernel half-width used for the nearest cascade; each farther cascade steps down by
 * one (so near shadows get a wide kernel, far shadows a narrow one, keeping the penumbra
 * roughly constant in world space).  0 = single 2x2 hardware tap. */
static int graph_dev_shadow_pcf_radius = 1;
/* Cross-cascade blend band width as a fraction of each cascade's far distance (0 disables).
 * A small band hides the seam between cascades and, at the outer coverage edge, fades the
 * last cascade to lit as it runs out of shadow map.  Tuned in shadow_lab. */
static float graph_dev_shadow_blend = 0.2f;
/* Normal-offset bias, in shadow-map texels: how far the shadow lookup is lifted off the
 * surface along its normal before being projected into the map.  Unlike the depth-pass slope
 * bias this does not move the sample along the light ray, so it suppresses acne without
 * detaching shadows from their casters.  0 disables it. */
static float graph_dev_shadow_normal_offset = 1.5f;
/* Slope-scaled polygon-offset bias applied while rendering the shadow map depth pass. */
/* Depth-pass polygon offset.  The slope term is the backstop for shadow acne where the
 * normal-offset bias cannot help: that lifts the lookup along the shading normal, which on a
 * coarse hull with averaged vertex normals can point well away from the facet it stands for,
 * so part of the lift goes sideways instead of clear of the surface.  2.0 is enough margin for
 * those meshes and costs about half a screen pixel of shadow detachment at 45 degrees. */
static float shadow_polygon_offset_factor = 2.0f;
static float shadow_polygon_offset_units = 4.0f;
#define SHADOW_MAP_TEXTURE_SIZE 4096
#define MAX_SHADOW_CASCADES 6
/* Largest PCF kernel half-width the lit shaders will loop over (radius 3 -> 7x7). */
#define CSM_PCF_MAX_RADIUS 3
#define CSM_STR_(x) #x
#define CSM_STR(x) CSM_STR_(x)
/* Injected into the lit shaders so C and GLSL agree on the cascade array size, shadow map
 * resolution (for PCF texel steps), and the maximum PCF loop bound. */
#define SHADOW_CASCADES_HEADER \
	"#define MAX_SHADOW_CASCADES " CSM_STR(MAX_SHADOW_CASCADES) "\n" \
	"#define SHADOW_MAP_SIZE " CSM_STR(SHADOW_MAP_TEXTURE_SIZE) "\n" \
	"#define CSM_PCF_MAX_RADIUS " CSM_STR(CSM_PCF_MAX_RADIUS) "\n"
/* Shared cascaded-shadow-map GLSL, concatenated ahead of every shader built by
 * setup_textured_shader() and setup_textured_cubemap_shader().  Inert without USE_CSM. */
#define CSM_SHADER_FILE "csm.shader"
/* Texture unit 2 is unused by the lit shaders (0=albedo, 1=emit, 3=normalmap) and is
 * within the BIND_TEXTURE cache range (units 0-3). */
#define SHADOW_MAP_TEXTURE_UNIT GL_TEXTURE2
static GLuint shadow_map_fbo;
static GLuint shadow_map_texture; /* GL_TEXTURE_2D_ARRAY, one layer per cascade */
static int shadow_map_layers; /* layers currently allocated; grown/shrunk to the cascade count */
static int shadow_map_ready; /* 1 if a shadow map was rendered this frame */
static int shadow_map_num_cascades = MAX_SHADOW_CASCADES;
static struct mat44d shadow_cascade_w2l[MAX_SHADOW_CASCADES]; /* world -> cascade light clip */
static struct mat44d shadow_current_w2l; /* the cascade currently being rendered */
/* Per-frame cascade split far-distances (view space) for depth-based cascade selection. */
static float shadow_cascade_split_far[MAX_SHADOW_CASCADES];
static GLint saved_shadow_viewport[4];
static GLint saved_shadow_fbo;
static void upload_shadow_receive_uniforms(GLint shadow_mvp_id, GLint num_cascades_id,
	GLint shadow_map_id, GLint shadow_normal_offset_id, const struct mat44d *model);
static void ensure_shadow_map_layers(int n);

static const char *default_shader_directory = "share/snis/shader";
static char shader_directory[PATH_MAX];

struct mesh_gl_info {
	/* common buffer to hold vertex positions */
	GLuint vertex_buffer;

	int ntriangles;
	/* uses vertex_buffer for data */
	GLuint triangle_vertex_buffer;

	GLuint triangle_normal_lines_buffer;
	GLuint triangle_tangent_lines_buffer;
	GLuint triangle_bitangent_lines_buffer;

	int nwireframe_lines;
	GLuint wireframe_lines_vertex_buffer;

	int npoints;
	/* uses vertex_buffer for data */

	int nlines;
	/* uses vertex_buffer for data */
	GLuint line_vertex_buffer;

	int nparticles;
	GLuint particle_vertex_buffer;
	GLuint particle_index_buffer;
};

struct vertex_buffer_data {
	union vec3 position;
};

struct vertex_triangle_buffer_data {
	union vec3 normal;
	union vec3 tvertex0;
	union vec3 tvertex1;
	union vec3 tvertex2;
	union vec3 wireframe_edge_mask;
	union vec2 texture_coord;
	union vec3 tangent;
	union vec3 bitangent;
};

struct vertex_wireframe_line_buffer_data {
	union vec3 position;
	union vec3 normal;
};

struct vertex_line_buffer_data {
	GLubyte multi_one[4];
	union vec3 line_vertex0;
	union vec3 line_vertex1;
};

struct vertex_color_buffer_data {
	GLfloat position[2];
	GLubyte color[4];
};

struct vertex_particle_buffer_data {
	GLubyte multi_one[4];
	union vec3 start_position;
	GLubyte start_tint_color[3];
	GLubyte start_apm[2];
	union vec3 end_position;
	GLubyte end_tint_color[3];
	GLubyte end_apm[2];
};

static void graph_dev_gen_texture_maybe_lock(int count, GLuint *texture_name, int lock)
{
	PROFILE_ZONE_START("graph_dev_gen_texture_maybe_lock");
	glGenTextures(count, texture_name);

	if (lock)
		pthread_mutex_lock(&finished_loading_mutex);
	for (int i = 0; i < count; i++)
		mark_texture_load_pending(texture_name[i]);
	if (lock)
		pthread_mutex_unlock(&finished_loading_mutex);
	PROFILE_ZONE_END();
}

static void graph_dev_gen_texture(int count, GLuint *texture_name)
{
	PROFILE_ZONE_START("graph_dev_gen_texture");
	graph_dev_gen_texture_maybe_lock(count, texture_name, 1);
	PROFILE_ZONE_END();
}

static void graph_dev_gen_texture_no_lock(int count, GLuint *texture_name)
{
	PROFILE_ZONE_START("graph_dev_gen_texture_no_lock");
	graph_dev_gen_texture_maybe_lock(count, texture_name, 0);
	PROFILE_ZONE_END();
}

void mesh_graph_dev_cleanup(struct mesh *m)
{
	PROFILE_ZONE_START("mesh_graph_dev_cleanup");
	if (m->graph_ptr) {
		struct mesh_gl_info *ptr = m->graph_ptr;

		glDeleteBuffers(1, &ptr->vertex_buffer);
		glDeleteBuffers(1, &ptr->triangle_vertex_buffer);
		glDeleteBuffers(1, &ptr->wireframe_lines_vertex_buffer);
		glDeleteBuffers(1, &ptr->triangle_normal_lines_buffer);
		glDeleteBuffers(1, &ptr->triangle_tangent_lines_buffer);
		glDeleteBuffers(1, &ptr->triangle_bitangent_lines_buffer);
		glDeleteBuffers(1, &ptr->line_vertex_buffer);
		glDeleteBuffers(1, &ptr->particle_vertex_buffer);
		glDeleteBuffers(1, &ptr->particle_index_buffer);

		free(ptr);
		m->graph_ptr = 0;
	}
	PROFILE_ZONE_END();
}

/* load/reload an array buffer using stream draw if it is being overwritten */
#define LOAD_BUFFER(buffer_type, buffer_id, buffer_size, buffer_data) \
	do { \
		PROFILE_ZONE_START("LOAD_BUFFER"); \
		GLenum usage; \
		if ((buffer_id) == 0) { \
			usage = GL_STATIC_DRAW; \
			glGenBuffers(1, &(buffer_id)); \
		} else \
			usage = GL_STREAM_DRAW; \
		glBindBuffer((buffer_type), (buffer_id)); \
		glBufferData((buffer_type), (buffer_size), (buffer_data), usage); \
		PROFILE_ZONE_END(); \
	} while (0)

void mesh_graph_dev_init(struct mesh *m)
{
	PROFILE_ZONE_START("mesh_graph_dev_init");

	struct mesh_gl_info *ptr = m->graph_ptr;
	if (!ptr) {
		ptr = malloc(sizeof(struct mesh_gl_info));
		memset(ptr, 0, sizeof(*ptr));
		m->graph_ptr = ptr;
	}

	if (m->geometry_mode == MESH_GEOMETRY_TRIANGLES) {
		/* setup the triangle mesh buffers */
		int i;
		size_t v_size = sizeof(struct vertex_buffer_data) * m->ntriangles * 3;
		size_t vt_size = sizeof(struct vertex_triangle_buffer_data) * m->ntriangles * 3;
		struct vertex_buffer_data *g_v_buffer_data = malloc(v_size);
		struct vertex_triangle_buffer_data *g_vt_buffer_data = malloc(vt_size);

#if DEBUG_NORMALS
		float normal_line_length = m->radius / 20.0;
		size_t nl_size = sizeof(struct vertex_buffer_data) * m->ntriangles * 3 * 2;
		struct vertex_buffer_data *g_nl_buffer_data = malloc(nl_size * 3);
		memset(g_nl_buffer_data, 0, nl_size * 3);
		struct vertex_buffer_data *g_tl_buffer_data = &g_nl_buffer_data[m->ntriangles * 3 * 2];
		struct vertex_buffer_data *g_bl_buffer_data = &g_nl_buffer_data[m->ntriangles * 3 * 2 * 2];
#endif

		ptr->ntriangles = m->ntriangles;
		ptr->npoints = m->ntriangles * 3; /* can be rendered as a point cloud too */

		for (i = 0; i < m->ntriangles; i++) {
			int j = 0;
			for (j = 0; j < 3; j++) {
				int v_index = i * 3 + j;
				g_v_buffer_data[v_index].position.v.x = m->t[i].v[j]->x;
				g_v_buffer_data[v_index].position.v.y = m->t[i].v[j]->y;
				g_v_buffer_data[v_index].position.v.z = m->t[i].v[j]->z;

				g_vt_buffer_data[v_index].normal.v.x = m->t[i].vnormal[j].x;
				g_vt_buffer_data[v_index].normal.v.y = m->t[i].vnormal[j].y;
				g_vt_buffer_data[v_index].normal.v.z = m->t[i].vnormal[j].z;

				g_vt_buffer_data[v_index].tvertex0.v.x = m->t[i].v[0]->x;
				g_vt_buffer_data[v_index].tvertex0.v.y = m->t[i].v[0]->y;
				g_vt_buffer_data[v_index].tvertex0.v.z = m->t[i].v[0]->z;

				g_vt_buffer_data[v_index].tvertex1.v.x = m->t[i].v[1]->x;
				g_vt_buffer_data[v_index].tvertex1.v.y = m->t[i].v[1]->y;
				g_vt_buffer_data[v_index].tvertex1.v.z = m->t[i].v[1]->z;

				g_vt_buffer_data[v_index].tvertex2.v.x = m->t[i].v[2]->x;
				g_vt_buffer_data[v_index].tvertex2.v.y = m->t[i].v[2]->y;
				g_vt_buffer_data[v_index].tvertex2.v.z = m->t[i].v[2]->z;

				g_vt_buffer_data[v_index].tangent.v.x = m->t[i].vtangent[j].x;
				g_vt_buffer_data[v_index].tangent.v.y = m->t[i].vtangent[j].y;
				g_vt_buffer_data[v_index].tangent.v.z = m->t[i].vtangent[j].z;

				g_vt_buffer_data[v_index].bitangent.v.x = m->t[i].vbitangent[j].x;
				g_vt_buffer_data[v_index].bitangent.v.y = m->t[i].vbitangent[j].y;
				g_vt_buffer_data[v_index].bitangent.v.z = m->t[i].vbitangent[j].z;

				/* bias the edge distance to make the coplanar edges not draw */
				if ((j == 1 || j == 2) && (m->t[i].flag & TRIANGLE_1_2_COPLANAR))
					g_vt_buffer_data[v_index].wireframe_edge_mask.v.x = 1000;
				else
					g_vt_buffer_data[v_index].wireframe_edge_mask.v.x = 0;

				if ((j == 0 || j == 2) && (m->t[i].flag & TRIANGLE_0_2_COPLANAR))
					g_vt_buffer_data[v_index].wireframe_edge_mask.v.y = 1000;
				else
					g_vt_buffer_data[v_index].wireframe_edge_mask.v.y = 0;

				if ((j == 0 || j == 1) && (m->t[i].flag & TRIANGLE_0_1_COPLANAR))
					g_vt_buffer_data[v_index].wireframe_edge_mask.v.z = 1000;
				else
					g_vt_buffer_data[v_index].wireframe_edge_mask.v.z = 0;

				if (m->tex) {
					g_vt_buffer_data[v_index].texture_coord.v.x = m->tex[v_index].u;
					g_vt_buffer_data[v_index].texture_coord.v.y = m->tex[v_index].v;
				} else {
					g_vt_buffer_data[v_index].texture_coord.v.x = 0;
					g_vt_buffer_data[v_index].texture_coord.v.y = 0;
				}

#if DEBUG_NORMALS
				/* draw a line for each vertex normal, tangent, and bitangent */
				int nl_index = i * 6 + j * 2;

				/* normal */
				g_nl_buffer_data[nl_index].position.v.x = m->t[i].v[j]->x;
				g_nl_buffer_data[nl_index].position.v.y = m->t[i].v[j]->y;
				g_nl_buffer_data[nl_index].position.v.z = m->t[i].v[j]->z;

				g_nl_buffer_data[nl_index + 1].position.v.x =
					m->t[i].v[j]->x + normal_line_length * m->t[i].vnormal[j].x;
				g_nl_buffer_data[nl_index + 1].position.v.y =
					m->t[i].v[j]->y + normal_line_length * m->t[i].vnormal[j].y;
				g_nl_buffer_data[nl_index + 1].position.v.z =
					m->t[i].v[j]->z + normal_line_length * m->t[i].vnormal[j].z;

				/* tangent */
				g_tl_buffer_data[nl_index].position.v.x = m->t[i].v[j]->x;
				g_tl_buffer_data[nl_index].position.v.y = m->t[i].v[j]->y;
				g_tl_buffer_data[nl_index].position.v.z = m->t[i].v[j]->z;
				g_tl_buffer_data[nl_index + 1].position.v.x =
					m->t[i].v[j]->x + normal_line_length * m->t[i].vtangent[j].x;
				g_tl_buffer_data[nl_index + 1].position.v.y =
					m->t[i].v[j]->y + normal_line_length * m->t[i].vtangent[j].y;
				g_tl_buffer_data[nl_index + 1].position.v.z =
					m->t[i].v[j]->z + normal_line_length * m->t[i].vtangent[j].z;

				/* bitangent */
				g_bl_buffer_data[nl_index].position.v.x = m->t[i].v[j]->x;
				g_bl_buffer_data[nl_index].position.v.y = m->t[i].v[j]->y;
				g_bl_buffer_data[nl_index].position.v.z = m->t[i].v[j]->z;
				g_bl_buffer_data[nl_index + 1].position.v.x =
					m->t[i].v[j]->x + normal_line_length * m->t[i].vbitangent[j].x;
				g_bl_buffer_data[nl_index + 1].position.v.y =
					m->t[i].v[j]->y + normal_line_length * m->t[i].vbitangent[j].y;
				g_bl_buffer_data[nl_index + 1].position.v.z =
					m->t[i].v[j]->z + normal_line_length * m->t[i].vbitangent[j].z;
#endif
			}
		}

		LOAD_BUFFER(GL_ARRAY_BUFFER, ptr->vertex_buffer, v_size, g_v_buffer_data);
		LOAD_BUFFER(GL_ARRAY_BUFFER, ptr->triangle_vertex_buffer, vt_size, g_vt_buffer_data);
#if DEBUG_NORMALS
		LOAD_BUFFER(GL_ARRAY_BUFFER, ptr->triangle_normal_lines_buffer, nl_size, g_nl_buffer_data);
		LOAD_BUFFER(GL_ARRAY_BUFFER, ptr->triangle_tangent_lines_buffer, nl_size, g_tl_buffer_data);
		LOAD_BUFFER(GL_ARRAY_BUFFER, ptr->triangle_bitangent_lines_buffer, nl_size, g_bl_buffer_data);
#endif

		free(g_v_buffer_data);
		free(g_vt_buffer_data);
#if DEBUG_NORMALS
		free(g_nl_buffer_data);
#endif

		/* setup the line buffers used for wireframe */
		size_t wfl_size = sizeof(struct vertex_wireframe_line_buffer_data) * m->ntriangles * 3 * 2;
		struct vertex_wireframe_line_buffer_data *g_wfl_buffer_data = malloc(wfl_size);

		/* map the edge combinatinos to the triangle coplanar flag */
		static const int tri_coplaner_flags[3][3] = {
			{0, TRIANGLE_0_1_COPLANAR, TRIANGLE_0_2_COPLANAR},
			{TRIANGLE_0_1_COPLANAR, 0, TRIANGLE_1_2_COPLANAR},
			{TRIANGLE_0_2_COPLANAR, TRIANGLE_1_2_COPLANAR, 0} };

		ptr->nwireframe_lines = 0;

		for (i = 0; i < m->ntriangles; i++) {
			int j0 = 0;
			for (j0 = 0; j0 < 3; j0++) {
				int j1 = (j0 + 1) % 3;

				if (!(m->t[i].flag & tri_coplaner_flags[j0][j1])) {
					int index = 2 * ptr->nwireframe_lines;

					/* add the line from vertex j0 to j1 */
					g_wfl_buffer_data[index].position.v.x = m->t[i].v[j0]->x;
					g_wfl_buffer_data[index].position.v.y = m->t[i].v[j0]->y;
					g_wfl_buffer_data[index].position.v.z = m->t[i].v[j0]->z;

					g_wfl_buffer_data[index + 1].position.v.x = m->t[i].v[j1]->x;
					g_wfl_buffer_data[index + 1].position.v.y = m->t[i].v[j1]->y;
					g_wfl_buffer_data[index + 1].position.v.z = m->t[i].v[j1]->z;

					/* the line normal is the same as the triangle */
					g_wfl_buffer_data[index].normal.v.x = m->t[i].n.x;
					g_wfl_buffer_data[index].normal.v.y = m->t[i].n.y;
					g_wfl_buffer_data[index].normal.v.z = m->t[i].n.z;
					g_wfl_buffer_data[index + 1].normal.v.x = m->t[i].n.x;
					g_wfl_buffer_data[index + 1].normal.v.y = m->t[i].n.y;
					g_wfl_buffer_data[index + 1].normal.v.z = m->t[i].n.z;

					ptr->nwireframe_lines++;
				}
			}
		}

		LOAD_BUFFER(GL_ARRAY_BUFFER, ptr->wireframe_lines_vertex_buffer,
			sizeof(struct vertex_wireframe_line_buffer_data) * ptr->nwireframe_lines * 2,
			g_wfl_buffer_data);

		free(g_wfl_buffer_data);
	}

	if (m->geometry_mode == MESH_GEOMETRY_LINES || m->geometry_mode == MESH_GEOMETRY_PARTICLE_ANIMATION) {
		/* setup the line buffers */
		int i;
		size_t v_size = sizeof(struct vertex_buffer_data) * m->nvertices * 2;
		struct vertex_buffer_data *g_v_buffer_data = malloc(v_size);

		size_t vl_size = sizeof(struct vertex_line_buffer_data) * m->nvertices * 2;
		struct vertex_line_buffer_data *g_vl_buffer_data = malloc(vl_size);

		ptr->nlines = 0;

		for (i = 0; i < m->nlines; i++) {
			struct vertex *vstart = m->l[i].start;
			struct vertex *vend = m->l[i].end;

			if (m->l[i].flag & MESH_LINE_STRIP) {
				struct vertex *vcurr = vstart;
				struct vertex *v1;

				while (vcurr <= vend) {
					struct vertex *v2 = vcurr;

					if (v2 != vstart) {
						int index = ptr->nlines * 2;
						g_v_buffer_data[index].position.v.x = v1->x;
						g_v_buffer_data[index].position.v.y = v1->y;
						g_v_buffer_data[index].position.v.z = v1->z;

						g_v_buffer_data[index + 1].position.v.x = v2->x;
						g_v_buffer_data[index + 1].position.v.y = v2->y;
						g_v_buffer_data[index + 1].position.v.z = v2->z;

						g_vl_buffer_data[index].multi_one[0] =
							g_vl_buffer_data[index + 1].multi_one[0] = 0; /* is dotted */

						g_vl_buffer_data[index].line_vertex0.v.x =
							g_vl_buffer_data[index + 1].line_vertex0.v.x = v1->x;
						g_vl_buffer_data[index].line_vertex0.v.y =
							g_vl_buffer_data[index + 1].line_vertex0.v.y = v1->y;
						g_vl_buffer_data[index].line_vertex0.v.z =
							g_vl_buffer_data[index + 1].line_vertex0.v.z = v1->z;

						g_vl_buffer_data[index].line_vertex1.v.x =
							g_vl_buffer_data[index + 1].line_vertex1.v.x = v2->x;
						g_vl_buffer_data[index].line_vertex1.v.y =
							g_vl_buffer_data[index + 1].line_vertex1.v.y = v2->y;
						g_vl_buffer_data[index].line_vertex1.v.z =
							g_vl_buffer_data[index + 1].line_vertex1.v.z = v2->z;

						ptr->nlines++;
					}
					v1 = v2;
					++vcurr;
				}
			} else {
				int is_dotted = m->l[i].flag & MESH_LINE_DOTTED;

				int index = ptr->nlines * 2;
				g_v_buffer_data[index].position.v.x = vstart->x;
				g_v_buffer_data[index].position.v.y = vstart->y;
				g_v_buffer_data[index].position.v.z = vstart->z;

				g_v_buffer_data[index + 1].position.v.x = vend->x;
				g_v_buffer_data[index + 1].position.v.y = vend->y;
				g_v_buffer_data[index + 1].position.v.z = vend->z;

				g_vl_buffer_data[index].multi_one[0] =
					g_vl_buffer_data[index + 1].multi_one[0] = is_dotted ? 255 : 0;

				g_vl_buffer_data[index].line_vertex0.v.x =
					g_vl_buffer_data[index + 1].line_vertex0.v.x = vstart->x;
				g_vl_buffer_data[index].line_vertex0.v.y =
					g_vl_buffer_data[index + 1].line_vertex0.v.y = vstart->y;
				g_vl_buffer_data[index].line_vertex0.v.z =
					g_vl_buffer_data[index + 1].line_vertex0.v.z = vstart->z;

				g_vl_buffer_data[index].line_vertex1.v.x =
					g_vl_buffer_data[index + 1].line_vertex1.v.x = vend->x;
				g_vl_buffer_data[index].line_vertex1.v.y =
					g_vl_buffer_data[index + 1].line_vertex1.v.y = vend->y;
				g_vl_buffer_data[index].line_vertex1.v.z =
					g_vl_buffer_data[index + 1].line_vertex1.v.z = vend->z;

				ptr->nlines++;
			}
		}

		LOAD_BUFFER(GL_ARRAY_BUFFER, ptr->vertex_buffer, sizeof(struct vertex_buffer_data) * 2 * ptr->nlines,
			g_v_buffer_data);
		LOAD_BUFFER(GL_ARRAY_BUFFER, ptr->line_vertex_buffer,
			sizeof(struct vertex_line_buffer_data) * 2 * ptr->nlines, g_vl_buffer_data);

		ptr->npoints = ptr->nlines * 2; /* can be rendered as a point cloud too */

		free(g_v_buffer_data);
		free(g_vl_buffer_data);
	}

	if (m->geometry_mode == MESH_GEOMETRY_POINTS) {
		/* setup the point buffers */
		size_t v_size = sizeof(struct vertex_buffer_data) * m->nvertices;
		struct vertex_buffer_data *g_v_buffer_data = malloc(v_size);

		ptr->npoints = m->nvertices;

		int i;
		for (i = 0; i < m->nvertices; i++) {
			g_v_buffer_data[i].position.v.x = m->v[i].x;
			g_v_buffer_data[i].position.v.y = m->v[i].y;
			g_v_buffer_data[i].position.v.z = m->v[i].z;
		}

		LOAD_BUFFER(GL_ARRAY_BUFFER, ptr->vertex_buffer, v_size, g_v_buffer_data);

		free(g_v_buffer_data);
	}

	if (m->geometry_mode == MESH_GEOMETRY_PARTICLE_ANIMATION) {
		ptr->nparticles = m->nvertices / 2;

		size_t v_size = sizeof(struct vertex_particle_buffer_data) * ptr->nparticles * 4;
		struct vertex_particle_buffer_data *g_v_buffer_data = malloc(v_size);

		size_t i_size = sizeof(GLushort) * ptr->nparticles * 6;
		GLushort *g_i_buffer_data = malloc(i_size);

		/* two triangles from four vertices
		   V3 (0,1) +---+ V2 (1,1)
			    +\  +
			    + \ +
			    +  \+
		   V0 (0,0) +---+ V1 (1,0)
		*/
		int i;
		for (i = 0; i < m->nvertices; i += 2) {
			int v_index = i * 2;

			GLubyte tint_red = (int)(m->l[i / 2].tint_color.red * 255) & 255;
			GLubyte tint_green = (int)(m->l[i / 2].tint_color.green * 255) & 255;
			GLubyte tint_blue = (int)(m->l[i / 2].tint_color.blue * 255) & 255;
			GLubyte additivity = (int)(m->l[i / 2].additivity * 255) & 255;
			GLubyte opacity = (int)(m->l[i / 2].opacity * 255) & 255;
			GLubyte time_offset = (int)(m->l[i / 2].time_offset * 255) & 255;

			/* texture coord is different for all four vertices */
			g_v_buffer_data[v_index + 0].multi_one[0] = 0;
			g_v_buffer_data[v_index + 0].multi_one[1] = 0;

			g_v_buffer_data[v_index + 1].multi_one[0] = 255;
			g_v_buffer_data[v_index + 1].multi_one[1] = 0;

			g_v_buffer_data[v_index + 2].multi_one[0] = 255;
			g_v_buffer_data[v_index + 2].multi_one[1] = 255;

			g_v_buffer_data[v_index + 3].multi_one[0] = 0;
			g_v_buffer_data[v_index + 3].multi_one[1] = 255;

			g_v_buffer_data[v_index + 0].multi_one[2] =
				g_v_buffer_data[v_index + 1].multi_one[2] =
				g_v_buffer_data[v_index + 2].multi_one[2] =
				g_v_buffer_data[v_index + 3].multi_one[2] = time_offset;

			/* the rest of the attributes are the same for all four */
			g_v_buffer_data[v_index + 0].start_position.v.x =
				g_v_buffer_data[v_index + 1].start_position.v.x =
				g_v_buffer_data[v_index + 2].start_position.v.x =
				g_v_buffer_data[v_index + 3].start_position.v.x = m->v[i].x;
			g_v_buffer_data[v_index + 0].start_position.v.y =
				g_v_buffer_data[v_index + 1].start_position.v.y =
				g_v_buffer_data[v_index + 2].start_position.v.y =
				g_v_buffer_data[v_index + 3].start_position.v.y = m->v[i].y;
			g_v_buffer_data[v_index + 0].start_position.v.z =
				g_v_buffer_data[v_index + 1].start_position.v.z =
				g_v_buffer_data[v_index + 2].start_position.v.z =
				g_v_buffer_data[v_index + 3].start_position.v.z = m->v[i].z;

			g_v_buffer_data[v_index + 0].start_tint_color[0] =
				g_v_buffer_data[v_index + 1].start_tint_color[0] =
				g_v_buffer_data[v_index + 2].start_tint_color[0] =
				g_v_buffer_data[v_index + 3].start_tint_color[0] = tint_red;
			g_v_buffer_data[v_index + 0].start_tint_color[1] =
				g_v_buffer_data[v_index + 1].start_tint_color[1] =
				g_v_buffer_data[v_index + 2].start_tint_color[1] =
				g_v_buffer_data[v_index + 3].start_tint_color[1] = tint_green;
			g_v_buffer_data[v_index + 0].start_tint_color[2] =
				g_v_buffer_data[v_index + 1].start_tint_color[2] =
				g_v_buffer_data[v_index + 2].start_tint_color[2] =
				g_v_buffer_data[v_index + 3].start_tint_color[2] = tint_blue;

			g_v_buffer_data[v_index + 0].start_apm[0] =
				g_v_buffer_data[v_index + 1].start_apm[0] =
				g_v_buffer_data[v_index + 2].start_apm[0] =
				g_v_buffer_data[v_index + 3].start_apm[0] = additivity;
			g_v_buffer_data[v_index + 0].start_apm[1] =
				g_v_buffer_data[v_index + 1].start_apm[1] =
				g_v_buffer_data[v_index + 2].start_apm[1] =
				g_v_buffer_data[v_index + 3].start_apm[1] = opacity;

			g_v_buffer_data[v_index + 0].end_position.v.x =
				g_v_buffer_data[v_index + 1].end_position.v.x =
				g_v_buffer_data[v_index + 2].end_position.v.x =
				g_v_buffer_data[v_index + 3].end_position.v.x = m->v[i + 1].x;
			g_v_buffer_data[v_index + 0].end_position.v.y =
				g_v_buffer_data[v_index + 1].end_position.v.y =
				g_v_buffer_data[v_index + 2].end_position.v.y =
				g_v_buffer_data[v_index + 3].end_position.v.y = m->v[i + 1].y;
			g_v_buffer_data[v_index + 0].end_position.v.z =
				g_v_buffer_data[v_index + 1].end_position.v.z =
				g_v_buffer_data[v_index + 2].end_position.v.z =
				g_v_buffer_data[v_index + 3].end_position.v.z = m->v[i + 1].z;

			g_v_buffer_data[v_index + 0].end_tint_color[0] =
				g_v_buffer_data[v_index + 1].end_tint_color[0] =
				g_v_buffer_data[v_index + 2].end_tint_color[0] =
				g_v_buffer_data[v_index + 3].end_tint_color[0] = tint_red;
			g_v_buffer_data[v_index + 0].end_tint_color[1] =
				g_v_buffer_data[v_index + 1].end_tint_color[1] =
				g_v_buffer_data[v_index + 2].end_tint_color[1] =
				g_v_buffer_data[v_index + 3].end_tint_color[1] = tint_green;
			g_v_buffer_data[v_index + 0].end_tint_color[2] =
				g_v_buffer_data[v_index + 1].end_tint_color[2] =
				g_v_buffer_data[v_index + 2].end_tint_color[2] =
				g_v_buffer_data[v_index + 3].end_tint_color[2] = tint_blue;

			g_v_buffer_data[v_index + 0].end_apm[0] =
				g_v_buffer_data[v_index + 1].end_apm[0] =
				g_v_buffer_data[v_index + 2].end_apm[0] =
				g_v_buffer_data[v_index + 3].end_apm[0] = additivity;
			g_v_buffer_data[v_index + 0].end_apm[1] =
				g_v_buffer_data[v_index + 1].end_apm[1] =
				g_v_buffer_data[v_index + 2].end_apm[1] =
				g_v_buffer_data[v_index + 3].end_apm[1] = opacity;

			/* setup six indices for our two triangles */
			int i_index = i * 3;
			g_i_buffer_data[i_index + 0] = v_index + 0;
			g_i_buffer_data[i_index + 1] = v_index + 1;
			g_i_buffer_data[i_index + 2] = v_index + 3;
			g_i_buffer_data[i_index + 3] = v_index + 1;
			g_i_buffer_data[i_index + 4] = v_index + 2;
			g_i_buffer_data[i_index + 5] = v_index + 3;
		}

		LOAD_BUFFER(GL_ARRAY_BUFFER, ptr->particle_vertex_buffer, v_size, g_v_buffer_data);
		LOAD_BUFFER(GL_ELEMENT_ARRAY_BUFFER, ptr->particle_index_buffer, i_size, g_i_buffer_data);

		free(g_v_buffer_data);
		free(g_i_buffer_data);
	}

	PROFILE_ZONE_END();
}

struct graph_dev_gl_shader_metadata {
	GLuint *program_id;
};

static void maybe_unload_shader(struct graph_dev_gl_shader_metadata *meta, GLuint *program_id)
{
	PROFILE_ZONE_START("maybe_unload_shader");
	if (meta->program_id && *meta->program_id != (GLuint) -1) /* Shader is currently loaded? */
		glDeleteProgram(*meta->program_id); /* Unload shader */
	meta->program_id = program_id;
	*meta->program_id = -1;
	PROFILE_ZONE_END();
}

struct graph_dev_gl_shader_common {
	struct graph_dev_gl_shader_metadata meta;
	GLuint program_id;
	GLuint vao_id;
};

static GLuint drawstate_active_program = 0;

static void
activate_shader(const void *vptr)
{
	const struct graph_dev_gl_shader_common *shader = (const struct graph_dev_gl_shader_common *)vptr;
	if (drawstate_active_program == shader->program_id) {
		return;
	}
	glUseProgram(shader->program_id);
	glBindVertexArray(shader->vao_id);

	drawstate_active_program = shader->program_id;
}

struct graph_dev_gl_vertex_color_shader {
	struct graph_dev_gl_shader_metadata meta;
	GLuint program_id;
	GLuint vao_id;
	GLint mvp_matrix_id;
	GLint vertex_position_id;
	GLint vertex_color_id;
};

struct graph_dev_gl_sun_shader {
	struct graph_dev_gl_shader_metadata meta;
	GLuint program_id;
	GLuint vao_id;
	GLint mvp_matrix_id;
	GLint vertex_position_id;
	GLint texture_coord_id;
	GLint color_id;
	GLint brightness_id;
	GLint disc_radius_id;
	GLint edge_softness_id;
	GLint psf_width_id;
	GLint psf_falloff_id;
	GLint filmic_tonemapping_id;
	GLint tonemapping_gain_id;
};

struct graph_dev_gl_black_hole_shader {
	struct graph_dev_gl_shader_metadata meta;
	GLuint program_id;
	GLuint vao_id;
	GLint mvp_matrix_id;
	GLint vertex_position_id;
	GLint texture_coord_id;
	GLint disc_radius_id;
	GLint edge_softness_id;
	GLint ring_brightness_id;
	GLint ring_width_id;
	GLint einstein_radius_id;
	GLint glow_brightness_id;
	GLint glow_width_id;
	GLint ring_color_id;
};

struct graph_dev_gl_single_color_lit_shader {
	struct graph_dev_gl_shader_metadata meta;
	GLuint program_id;
	GLuint vao_id;
	GLint mvp_matrix_id;
	GLint mv_matrix_id;
	GLint normal_matrix_id;
	GLint vertex_position_id;
	GLint vertex_normal_id;
	GLint light_pos_id;
	GLint color_id;
	GLint in_shade_id;
	GLint ambient_id;
	GLint light_color_id;   /* star-tinted direct light colour (u_LightColor) */
	GLint ambient_color_id; /* absolute, complement-tinted ambient colour (u_AmbientColor) */
	GLint filmic_tonemapping_id;
	GLint tonemapping_gain_id;
	GLint shadow_mvp_id;         /* model -> light clip space, per cascade (USE_CSM only) */
	GLint shadow_map_id;         /* shadow map array sampler (USE_CSM only) */
	GLint num_cascades_id;       /* number of active cascades (USE_CSM only) */
	GLint shadow_map_enabled_id; /* 1 when a shadow map is available this frame (USE_CSM only) */
	GLint shadow_debug_id;       /* shadow debug visualization mode (USE_CSM only) */
	GLint shadow_pcf_radius_id;  /* PCF kernel half-width for the nearest cascade (USE_CSM only) */
	GLint cascade_split_far_id;  /* per-cascade view-space far distances (USE_CSM only) */
	GLint shadow_blend_id;       /* cross-cascade blend band fraction (USE_CSM only) */
	GLint shadow_normal_offset_id; /* per-cascade normal-offset lift, model units (USE_CSM) */
};

/* Depth-only shader used to render the shadow map from the light's viewpoint. */
struct graph_dev_gl_shadow_depth_shader {
	struct graph_dev_gl_shader_metadata meta;
	GLuint program_id;
	GLuint vao_id;
	GLint shadow_mvp_id;
	GLint vertex_position_id;
};

struct graph_dev_gl_atmosphere_shader {
	struct graph_dev_gl_shader_metadata meta;
	GLuint program_id;
	GLuint vao_id;
	GLint mvp_matrix_id;
	GLint mv_matrix_id;
	GLint normal_matrix_id;
	GLint vertex_position_id;
	GLint vertex_normal_id;
	GLint light_pos_id;
	GLint color_id;
	GLfloat alpha;
	GLint shadow_annulus_texture_id;
	GLint shadow_annulus_center_id;
	GLint shadow_annulus_normal_id;
	GLint shadow_annulus_radius_id;
	GLint shadow_annulus_tint_color_id;
	GLint ring_texture_v_id;
	GLint atmosphere_brightness_id;
	GLint light_color_id;   /* star-tinted direct light colour (u_LightColor) */
	GLint ambient_color_id; /* absolute, complement-tinted ambient colour (u_AmbientColor) */
	GLint filmic_tonemapping_id;
	GLint tonemapping_gain_id;
};

struct graph_dev_gl_filled_wireframe_shader {
	struct graph_dev_gl_shader_metadata meta;
	GLuint program_id;
	GLuint vao_id;
	GLint viewport_id;
	GLint mvp_matrix_id;
	GLint position_id;
	GLint tvertex0_id;
	GLint tvertex1_id;
	GLint tvertex2_id;
	GLint edge_mask_id;
	GLint line_color_id;
	GLint triangle_color_id;
};

struct graph_dev_gl_trans_wireframe_shader {
	struct graph_dev_gl_shader_metadata meta;
	GLuint program_id;
	GLuint vao_id;
	GLint mvp_matrix_id;
	GLint mv_matrix_id;
	GLint normal_matrix_id;
	GLint vertex_position_id;
	GLint vertex_normal_id;
	GLint color_id;

	GLint clip_sphere_id;
	GLint clip_sphere_radius_fade_id;
};

struct graph_dev_gl_single_color_shader {
	struct graph_dev_gl_shader_metadata meta;
	GLuint program_id;
	GLuint vao_id;
	GLint mvp_matrix_id;
	GLint vertex_position_id;
	GLint color_id;
};

struct graph_dev_gl_line_single_color_shader {
	struct graph_dev_gl_shader_metadata meta;
	GLuint program_id;
	GLuint vao_id;
	GLint mvp_matrix_id;
	GLint viewport_id;
	GLint multi_one_id;
	GLint vertex_position_id;
	GLint line_vertex0_id;
	GLint line_vertex1_id;
	GLint dot_size_id;
	GLint dot_pitch_id;
	GLint line_color_id;
};

struct graph_dev_gl_point_cloud_shader {
	struct graph_dev_gl_shader_metadata meta;
	GLuint program_id;
	GLuint vao_id;
	GLint mvp_matrix_id;
	GLint vertex_position_id;
	GLint point_size_id;
	GLint color_id;
	GLint time_id;
	GLint camera_pos_id;  /* world-space eye, for a per-point distance fade */
	GLint fade_params_id; /* (near0, near1, far0, far1); w <= 0 leaves points flat */
};

struct graph_dev_gl_skybox_shader {
	struct graph_dev_gl_shader_metadata meta;
	GLuint program_id;
	GLuint vao_id;
	GLint mvp_id;
	GLint vertex_id;
	GLint texture_id;
	GLuint cube_texture_id;
	GLint filmic_tonemapping_id;
	GLint tonemapping_gain_id;
	GLint lens_dir_id;
	GLint lens_params_id;
};

struct graph_dev_gl_color_by_w_shader {
	struct graph_dev_gl_shader_metadata meta;
	GLuint program_id;
	GLuint vao_id;
	GLint mvp_id;
	GLint position_id;
	GLint near_color_id;
	GLint near_w_id;
	GLint center_color_id;
	GLint center_w_id;
	GLint far_color_id;
	GLint far_w_id;
};

struct graph_dev_gl_textured_shader {
	struct graph_dev_gl_shader_metadata meta;
	GLuint program_id;
	GLuint vao_id;
	GLint mvp_matrix_id;
	GLint mv_matrix_id;
	GLint normal_matrix_id;
	GLint vertex_position_id;
	GLint vertex_normal_id;
	GLint vertex_tangent_id;
	GLint vertex_bitangent_id;
	GLint tint_color_id;
	GLint texture_coord_id;
	GLint texture_2d_id;
	GLint emit_texture_2d_id;
	GLint emit_intensity_id;
	GLint texture_cubemap_id;
	GLint normalmap_cubemap_id;
	GLint normalmap_id;
	GLint light_pos_id;
	GLint specular_power_id;
	GLint specular_intensity_id;
	GLint ambient_id;
	GLint light_color_id;   /* star-tinted direct light colour (u_LightColor) */
	GLint ambient_color_id; /* absolute, complement-tinted ambient colour (u_AmbientColor) */
	GLint ambient_scale_id; /* scale on the shaded floor (u_AmbientScale); planets dim it */
	GLint filmic_tonemapping_id;
	GLint tonemapping_gain_id;

	GLint shadow_sphere_id;

	GLint shadow_annulus_texture_id;
	GLint shadow_annulus_center_id;
	GLint shadow_annulus_normal_id;
	GLint shadow_annulus_radius_id;
	GLint shadow_annulus_tint_color_id;

	GLint ring_texture_v_id;
	GLint ring_inner_radius_id;
	GLint ring_outer_radius_id;
	GLint invert; /* used by alpha_by_normal shader */
	GLint in_shade;
	GLint water_color; /* Used for specular calculations by planet shader */
	GLint u1v1; /* Used by planetary lightning shader */
	GLint texture_width; /* Used by planetary lightning shader */

	/* Cascaded shadow mapping (USE_CSM variants only). */
	GLint shadow_mvp_id;         /* model -> light clip space, per cascade */
	GLint shadow_map_id;         /* shadow map array sampler */
	GLint num_cascades_id;       /* number of active cascades */
	GLint shadow_map_enabled_id; /* 1 when a shadow map is available this frame */
	GLint shadow_debug_id;       /* 1 to visualize the shadow factor */
	GLint shadow_pcf_radius_id;  /* PCF kernel half-width for the nearest cascade */
	GLint cascade_split_far_id;  /* per-cascade view-space far distances */
	GLint shadow_blend_id;       /* cross-cascade blend band fraction */
	GLint shadow_normal_offset_id; /* per-cascade normal-offset lift, in model units */
};

struct clip_sphere_data {
	union vec3 eye_pos;
	float r;
	float radius_fade;
};

struct shadow_sphere_data {
	union vec3 eye_pos;
	float r;
};

struct shadow_annulus_data {
	GLuint texture_id;
	union vec3 eye_pos;
	float r1, r2;
	struct sng_color tint_color;
	float alpha;
};

struct graph_dev_gl_textured_particle_shader {
	struct graph_dev_gl_shader_metadata meta;
	GLuint program_id;
	GLuint vao_id;
	GLint mvp_matrix_id;
	GLint camera_up_vec_id;
	GLint camera_right_vec_id;
	GLint time_id;
	GLint radius_id;
	GLint multi_one_id;
	GLint start_position_id;
	GLint start_tint_color_id;
	GLint start_apm_id;
	GLint end_position_id;
	GLint end_tint_color_id;
	GLint end_apm_id;
	GLint texture_id; /* param to vertex shader */
	GLint filmic_tonemapping_id;
	GLint tonemapping_gain_id;
};

struct graph_dev_gl_fs_effect_shader { /* For full screen effect shaders */
	struct graph_dev_gl_shader_metadata meta;
	GLuint program_id;
	GLuint vao_id;
	GLint mvp_matrix_id;
	GLint vertex_position_id;
	GLint texture_coord_id;
	GLint tint_color_id;
	GLint viewport_id;
	GLint texture0_id;
	GLint texture1_id;
	GLint texture2_id;
};

struct fbo_target {
	GLuint fbo;
	GLuint color0_texture;
	GLuint color0_buffer;
	GLuint depth_buffer;
	int samples;
	int width;
	int height;
};

struct graph_dev_smaa_effect {
	struct fbo_target edge_target;
	struct fbo_target blend_target;

	struct graph_dev_gl_fs_effect_shader edge_shader;
	struct graph_dev_gl_fs_effect_shader blend_shader;
	struct graph_dev_gl_fs_effect_shader neighborhood_shader;

	GLuint area_tex;
	GLuint search_tex;
};

/* store all the shader parameters */
static struct graph_dev_gl_single_color_lit_shader single_color_lit_shader;
static struct graph_dev_gl_single_color_lit_shader single_color_lit_shadow_shader;
static struct graph_dev_gl_shadow_depth_shader shadow_depth_shader;
static struct graph_dev_gl_atmosphere_shader atmosphere_shader;
static struct graph_dev_gl_atmosphere_shader atmosphere_with_annulus_shadow_shader;
static struct graph_dev_gl_trans_wireframe_shader trans_wireframe_shader;
static struct graph_dev_gl_trans_wireframe_shader trans_wireframe_with_clip_sphere_shader;
static struct graph_dev_gl_filled_wireframe_shader filled_wireframe_shader;
static struct graph_dev_gl_single_color_shader single_color_shader;
static struct graph_dev_gl_line_single_color_shader line_single_color_shader;
static struct graph_dev_gl_vertex_color_shader vertex_color_shader;
static struct graph_dev_gl_sun_shader sun_shader;
static struct graph_dev_gl_black_hole_shader black_hole_shader;
static struct graph_dev_gl_point_cloud_shader point_cloud_shader;
static struct graph_dev_gl_skybox_shader skybox_shader;

/* Gravitational lenses bending the skybox, packed the way the shader's uniform arrays want
 * them.  Active lenses are packed first and the remaining slots keep a zero Einstein radius,
 * which the shader treats as contributing nothing -- see graph_dev_set_gravitational_lenses()
 * below and share/snis/shader/skybox.frag. */
static GLfloat gravitational_lens_dir[MAX_GRAVITATIONAL_LENSES * 3];
static GLfloat gravitational_lens_params[MAX_GRAVITATIONAL_LENSES * 3];

void graph_dev_set_gravitational_lenses(int n, const struct graph_dev_gravitational_lens *lens)
{
	int i;

	if (n > MAX_GRAVITATIONAL_LENSES)
		n = MAX_GRAVITATIONAL_LENSES;
	memset(gravitational_lens_dir, 0, sizeof(gravitational_lens_dir));
	memset(gravitational_lens_params, 0, sizeof(gravitational_lens_params));
	for (i = 0; i < n; i++) {
		gravitational_lens_dir[i * 3 + 0] = lens[i].direction[0];
		gravitational_lens_dir[i * 3 + 1] = lens[i].direction[1];
		gravitational_lens_dir[i * 3 + 2] = lens[i].direction[2];
		gravitational_lens_params[i * 3 + 0] = lens[i].einstein_radius;
		gravitational_lens_params[i * 3 + 1] = lens[i].shadow_radius;
		gravitational_lens_params[i * 3 + 2] = lens[i].swirl;
	}
}
static struct graph_dev_gl_color_by_w_shader color_by_w_shader;
static struct graph_dev_gl_textured_shader textured_shader;
static struct graph_dev_gl_textured_shader planetary_lightning_shader;
static struct graph_dev_gl_textured_shader warp_gate_effect_shader;
static struct graph_dev_gl_textured_shader textured_with_sphere_shadow_shader;
static struct graph_dev_gl_textured_shader textured_lit_shader;
static struct graph_dev_gl_textured_shader textured_lit_emit_shader;
static struct graph_dev_gl_textured_shader textured_lit_emit_normal_shader;
static struct graph_dev_gl_textured_shader textured_lit_normal_shader;
static struct graph_dev_gl_textured_shader textured_cubemap_shield_shader;

/* 6 pairs of textured cubemap shaders, one with CSM shadows, one without, with combos of other features */
static struct graph_dev_gl_textured_shader textured_cubemap_lit_shader;
static struct graph_dev_gl_textured_shader textured_cubemap_lit_shadow_shader;
static struct graph_dev_gl_textured_shader textured_cubemap_lit_normal_map_shader;
static struct graph_dev_gl_textured_shader textured_cubemap_lit_normal_map_shadow_shader;
static struct graph_dev_gl_textured_shader textured_cubemap_lit_with_annulus_shader;
static struct graph_dev_gl_textured_shader textured_cubemap_lit_with_annulus_shadow_shader;
static struct graph_dev_gl_textured_shader textured_cubemap_normal_mapped_lit_with_annulus_shader;
static struct graph_dev_gl_textured_shader textured_cubemap_normal_mapped_lit_with_annulus_shadow_shader;
static struct graph_dev_gl_textured_shader textured_cubemap_normal_mapped_lit_with_annulus_specular_shader;
static struct graph_dev_gl_textured_shader textured_cubemap_normal_mapped_lit_with_annulus_shadow_specular_shader;
static struct graph_dev_gl_textured_shader textured_cubemap_normal_mapped_lit_specular_shader;
static struct graph_dev_gl_textured_shader textured_cubemap_normal_mapped_lit_specular_shadow_shader;

static struct graph_dev_gl_textured_particle_shader textured_particle_shader;
static struct graph_dev_gl_textured_shader alpha_by_normal_shader;
static struct graph_dev_gl_textured_shader textured_alpha_by_normal_shader;
static struct graph_dev_gl_fs_effect_shader fs_copy_shader;
static struct graph_dev_smaa_effect smaa_effect;

static struct fbo_target msaa = { 0 };
static struct fbo_target post_target0 = { 0 };
static struct fbo_target post_target1 = { 0 };
static struct fbo_target render_target_2d = { 0 };

struct graph_dev_primitive {
	int nvertices;
	GLuint vertex_buffer;
	GLuint triangle_vertex_buffer;
};

static struct graph_dev_primitive cubemap_cube;
static struct graph_dev_primitive textured_unit_quad;

#define BUFFERED_VERTICES_2D 2000
#define VERTEX_BUFFER_2D_SIZE (BUFFERED_VERTICES_2D*sizeof(struct vertex_color_buffer_data))

static struct graph_dev_gl_context {
	int screen_x, screen_y;
	float x_scale, y_scale;
	struct graph_dev_color *hue; /* current color */
	int alpha_blend;
	float alpha;
	GLuint fbo_current;

	int active_vp; /* 0=none, 1=2d, 2=3d */
	int vp_x_3d, vp_y_3d, vp_width_3d, vp_height_3d;
	GLuint fbo_2d;
	struct mat44 ortho_2d_mvp;

	int nvertex_2d;
	GLbyte vertex_type_2d[BUFFERED_VERTICES_2D];
	struct vertex_color_buffer_data vertex_data_2d[BUFFERED_VERTICES_2D];
	GLuint vertex_buffer_2d;

	struct mesh_gl_info gl_info_3d_line;
	GLuint fbo_3d;
	int texture_unit_active;
	GLuint texture_unit_bind[4];
	GLenum src_blend_func;
	GLenum dest_blend_func;
	GLint vp_x, vp_y;
	GLsizei vp_width, vp_height;
} sgc;

/* The image loader uploads finished textures with bare glBindTexture() calls, outside the
 * BIND_TEXTURE discipline above, and it does so between draws whenever an asynchronous load
 * completes.  That leaves the cache claiming one texture is on the active unit while the upload
 * has since bound a different one to that unit, and the next BIND_TEXTURE asking for the cached
 * id then skips its bind and the draw samples whatever the upload left behind.
 *
 * The visible symptom is a cubemap arriving mid-scene and briefly replacing the skybox with
 * itself -- an asteroid's rock texture wrapped around the whole sky for a frame or two, until
 * something else binds to that unit and resyncs the cache by accident.
 *
 * So: once an upload has finished with the binding, tell the cache what it actually left on the
 * active unit, which keeps the cache true rather than merely forcing the next bind.
 */
static void note_texture_bound_outside_cache(GLuint tex_id)
{
	sgc.texture_unit_bind[sgc.texture_unit_active] = tex_id;
}

#define BIND_TEXTURE(tex_unit, tex_type, tex_id) \
	do { \
		PROFILE_ZONE_START("BIND_TEXTURE"); \
		int tex_offset = tex_unit - GL_TEXTURE0; \
		if (sgc.texture_unit_active != tex_offset) { \
			glActiveTexture(tex_unit); \
			sgc.texture_unit_active = tex_offset; \
		} \
		if (sgc.texture_unit_bind[tex_offset] != tex_id) { \
			glBindTexture(tex_type, tex_id); \
			sgc.texture_unit_bind[tex_offset] = tex_id; \
		} \
		PROFILE_ZONE_END(); \
	} while (0)

#define BLEND_FUNC(src_blend, dest_blend) \
	do { \
		PROFILE_ZONE_START("BLEND_FUNC"); \
		if (sgc.src_blend_func != src_blend || sgc.dest_blend_func != dest_blend) { \
			glBlendFunc(src_blend, dest_blend); \
			sgc.src_blend_func = src_blend; \
			sgc.dest_blend_func = dest_blend; \
		} \
		PROFILE_ZONE_END(); \
	} while (0)

#define VIEWPORT(x, y, width, height) \
	do { \
		PROFILE_ZONE_START("VIEWPORT"); \
		if (sgc.vp_x != x || sgc.vp_y != y || sgc.vp_width != width || sgc.vp_height != height) { \
			glViewport(x, y, width, height); \
			sgc.vp_x = x; \
			sgc.vp_y = y; \
			sgc.vp_width = width; \
			sgc.vp_height = height; \
		} \
		PROFILE_ZONE_END(); \
	} while (0)

static void print_framebuffer_error(void)
{
	switch (glCheckFramebufferStatus(GL_FRAMEBUFFER)) {
	case GL_FRAMEBUFFER_COMPLETE:
		break;

	case GL_FRAMEBUFFER_UNSUPPORTED:
		printf("FBO Unsupported framebuffer format.\n");
		break;

	case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT:
		printf("FBO Missing attachment.\n");
		break;

	case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT:
		printf("FBO Incomplete attachment.\n");
		break;

	case GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER:
		printf("FBO Missing draw buffer.\n");
		break;

	case GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER:
		printf("FBO Missing read buffer.\n");
		break;

	case GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE:
		printf("FBO Attached images must have the same number of samples.\n");
		break;

	default:
		printf("FBO Fatal error.\n");
	}
}

static void resize_fbo_if_needed(struct fbo_target *target)
{
	PROFILE_ZONE_START("resize_fbo_if_needed");

	glBindFramebuffer(GL_FRAMEBUFFER, target->fbo);

	if (target->width != sgc.screen_x || target->height != sgc.screen_y) {
		fprintf(stderr, "Resizing FBO %d attachments to %d x %d\n", target->fbo, sgc.screen_x, sgc.screen_y);

		/* need to resize the fbo attachments */
		if (target->color0_texture > 0) {
			glBindTexture(GL_TEXTURE_2D, target->color0_texture);
			if (GLAD_GL_ARB_texture_storage) {
				glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, sgc.screen_x, sgc.screen_y);
			} else {
				glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8,
					sgc.screen_x, sgc.screen_y, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
			}
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, target->color0_texture, 0);
		}

		if (target->depth_buffer > 0) {
			glBindRenderbuffer(GL_RENDERBUFFER, target->depth_buffer);
			glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, sgc.screen_x, sgc.screen_y);
			glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, target->depth_buffer);
		}

		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, target->fbo);

		GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
		if (status != GL_FRAMEBUFFER_COMPLETE) {
			print_framebuffer_error();
		}

		target->width = sgc.screen_x;
		target->height = sgc.screen_y;
	}

	PROFILE_ZONE_END();
}

void graph_dev_set_screen_size(int width, int height)
{
	sgc.active_vp = 0;
	sgc.screen_x = width;
	sgc.screen_y = height;
}

void graph_dev_set_extent_scale(float x_scale, float y_scale)
{
	sgc.x_scale = x_scale;
	sgc.y_scale = y_scale;
}

void graph_dev_set_3d_viewport(int x_offset, int y_offset, int width, int height)
{
	sgc.active_vp = 0;
	sgc.vp_x_3d = x_offset;
	sgc.vp_y_3d = sgc.screen_y - height - y_offset;
	sgc.vp_width_3d = width;
	sgc.vp_height_3d = height;
}

static void enable_2d_viewport(void)
{
	PROFILE_ZONE_START("enable_2d_viewport");
	if (sgc.active_vp != 1) {
		/* 2d viewport is entire screen */
		VIEWPORT(0, 0, sgc.screen_x, sgc.screen_y);

		float left = 0, right = sgc.screen_x, bottom = 0, top = sgc.screen_y;
		float near = -1, far = 1;

		sgc.ortho_2d_mvp.m[0][0] = 2.0 / (right - left);
		sgc.ortho_2d_mvp.m[0][1] = 0;
		sgc.ortho_2d_mvp.m[0][2] = 0;
		sgc.ortho_2d_mvp.m[0][3] = 0;
		sgc.ortho_2d_mvp.m[1][0] = 0;
		sgc.ortho_2d_mvp.m[1][1] = 2.0 / (top - bottom);
		sgc.ortho_2d_mvp.m[1][2] = 0;
		sgc.ortho_2d_mvp.m[1][3] = 0;
		sgc.ortho_2d_mvp.m[2][0] = 0;
		sgc.ortho_2d_mvp.m[2][1] = 0;
		sgc.ortho_2d_mvp.m[2][2] = -2.0 / (far - near);
		sgc.ortho_2d_mvp.m[2][3] = 0;
		sgc.ortho_2d_mvp.m[3][0] = -(right + left) / (right - left);
		sgc.ortho_2d_mvp.m[3][1] = -(top + bottom) / (top - bottom);
		sgc.ortho_2d_mvp.m[3][2] = -(far + near) / (far - near);
		sgc.ortho_2d_mvp.m[3][3] = 1;

		if (sgc.fbo_2d > 0) {
			if (sgc.fbo_current != sgc.fbo_2d) {
				glBindFramebuffer(GL_FRAMEBUFFER, sgc.fbo_2d);
				sgc.fbo_current = sgc.fbo_2d;
			}
		} else if (sgc.fbo_current != 0) {
			static const GLenum drawBuffers[] = { GL_BACK };
			glBindFramebuffer(GL_FRAMEBUFFER, 0);
			glDrawBuffers(ARRAYSIZE(drawBuffers), drawBuffers);
			sgc.fbo_current = 0;
		}

		sgc.active_vp = 1;
	}
	PROFILE_ZONE_END();
}

static void enable_3d_viewport(void)
{
	PROFILE_ZONE_START("enable_3d_viewport");
	if (sgc.active_vp != 2) {
		VIEWPORT(sgc.vp_x_3d, sgc.vp_y_3d, sgc.vp_width_3d, sgc.vp_height_3d);

		if (sgc.fbo_3d > 0) {
			if (sgc.fbo_current != sgc.fbo_3d) {
				glBindFramebuffer(GL_FRAMEBUFFER, sgc.fbo_3d);
				sgc.fbo_current = sgc.fbo_3d;
			}
		} else if (sgc.fbo_current != 0) {
			static const GLenum drawBuffers[] = { GL_BACK };
			glBindFramebuffer(GL_FRAMEBUFFER, 0);
			glDrawBuffers(ARRAYSIZE(drawBuffers), drawBuffers);
			sgc.fbo_current = 0;
		}

		sgc.active_vp = 2;
	}
	PROFILE_ZONE_END();
}


void graph_dev_set_color(struct graph_dev_color *color, float a)
{
	sgc.hue = color;

	if (a >= 0) {
		sgc.alpha_blend = 1;
		sgc.alpha = a;
	} else {
		sgc.alpha_blend = 0;
	}
}

static void draw_vertex_buffer_2d(void)
{
	PROFILE_ZONE_START("draw_vertex_buffer_2d");
	if (sgc.nvertex_2d > 0) {
		/* printf("start draw_vertex_buffer_2d %d\n", sgc.nvertex_2d); */
		enable_2d_viewport();

		/* transfer into opengl buffer */
		glBindBuffer(GL_ARRAY_BUFFER, sgc.vertex_buffer_2d);
		glBufferSubData(GL_ARRAY_BUFFER, 0, sgc.nvertex_2d * sizeof(struct vertex_color_buffer_data),
			sgc.vertex_data_2d);

		activate_shader(&vertex_color_shader);

		glUniformMatrix4fv(vertex_color_shader.mvp_matrix_id, 1, GL_FALSE, &sgc.ortho_2d_mvp.m[0][0]);

		/* load x,y vertex position */
		glEnableVertexAttribArray(vertex_color_shader.vertex_position_id);
		glBindBuffer(GL_ARRAY_BUFFER, sgc.vertex_buffer_2d);
		glVertexAttribPointer(
			vertex_color_shader.vertex_position_id,
			2,
			GL_FLOAT,
			GL_FALSE,
			sizeof(struct vertex_color_buffer_data),
			(void *)offsetof(struct vertex_color_buffer_data, position[0])
		);

		/* load the color as as 4 bytes and let opengl normalize them to 0-1 floats */
		glEnableVertexAttribArray(vertex_color_shader.vertex_color_id);
		glBindBuffer(GL_ARRAY_BUFFER, sgc.vertex_buffer_2d);
		glVertexAttribPointer(
			vertex_color_shader.vertex_color_id,
			4,
			GL_UNSIGNED_BYTE,
			GL_TRUE,
			sizeof(struct vertex_color_buffer_data),
			(void *)offsetof(struct vertex_color_buffer_data, color[0])
		);

		int i;
		GLint start = 0;
		GLbyte mode = sgc.vertex_type_2d[0];

		for (i = 0; i < sgc.nvertex_2d; i++) {
			if (mode != sgc.vertex_type_2d[i]) {
				GLsizei count;

				/* primitive terminate */
				if (sgc.vertex_type_2d[i] == -1)
					count = i - start + 1; /* we include this vertex in draw */
				else
					count = i - start;

				assert(mode != GL_LINES || count % 2 == 0);
				assert(mode != GL_TRIANGLES || count % 3 == 0);

				glDrawArrays(mode, start, count);
				/* printf("glDrawArrays 1 mode=%d start=%d count=%d\n", mode, start, count); */

				start = start + count;
				if (start < sgc.nvertex_2d) {
					mode = sgc.vertex_type_2d[start];
				}
			}
		}
		if (start < sgc.nvertex_2d) {
			GLsizei count = sgc.nvertex_2d - start;

			assert(mode != GL_LINES || count % 2 == 0);
			assert(mode != GL_TRIANGLES || count % 3 == 0);

			glDrawArrays(mode, start, count);
			/* printf("glDrawArrays 2 mode=%d start=%d count=%d\n", mode, start, i - start); */
		}

		sgc.nvertex_2d = 0;

		/* orphan this buffer so we don't get blocked on these draw commands */
		glBufferData(GL_ARRAY_BUFFER, VERTEX_BUFFER_2D_SIZE, 0, GL_STREAM_DRAW);
	}
	PROFILE_ZONE_END();
}

static void make_room_in_vertex_buffer_2d(int nvertices)
{
	PROFILE_ZONE_START("make_room_in_vertex_buffer_2d");
	if (sgc.nvertex_2d + nvertices > BUFFERED_VERTICES_2D) {
		/* buffer needs to be emptied to fit next batch */
		draw_vertex_buffer_2d();
	}
	PROFILE_ZONE_END();
}

static void add_vertex_2d(float x, float y, struct graph_dev_color *color, GLubyte alpha, GLenum mode)
{
	PROFILE_ZONE_START("add_vertex_2d");
	struct vertex_color_buffer_data *vertex = &sgc.vertex_data_2d[sgc.nvertex_2d];

	/* setup the vertex and color */
	vertex->position[0] = x;
	vertex->position[1] = sgc.screen_y - y;

	vertex->color[0] = color->red >> 8;
	vertex->color[1] = color->green >> 8;
	vertex->color[2] = color->blue >> 8;
	vertex->color[3] = alpha;

	sgc.vertex_type_2d[sgc.nvertex_2d] = mode;

	sgc.nvertex_2d += 1;
	PROFILE_ZONE_END();
}

#if DEBUG_NORMALS
static void graph_dev_draw_normal_lines(const struct mat44 *mat_mvp, struct mesh *m, struct mesh_gl_info *ptr)
{
	glEnable(GL_DEPTH_TEST);

	activate_shader(&simple_color_shader);

	glUniformMatrix4fv(single_color_shader.mvp_matrix_id, 1, GL_FALSE, &mat_mvp->m[0][0]);

	/* normal lines */
	glUniform4f(single_color_shader.color_id, 1, 0, 0, 1);
	glEnableVertexAttribArray(single_color_shader.vertex_position_id);
	glBindBuffer(GL_ARRAY_BUFFER, ptr->triangle_normal_lines_buffer);
	glVertexAttribPointer(
		single_color_shader.vertex_position_id, /* The attribute we want to configure */
		3,                           /* size */
		GL_FLOAT,                    /* type */
		GL_FALSE,                    /* normalized? */
		sizeof(struct vertex_buffer_data), /* stride */
		(void *)offsetof(struct vertex_buffer_data, position.v.x) /* array buffer offset */
	);
	glDrawArrays(GL_LINES, 0, m->ntriangles * 3 * 2);

	/* tangent lines */
	glUniform4f(single_color_shader.color_id, 0, 1, 0, 1);
	glEnableVertexAttribArray(single_color_shader.vertex_position_id);
	glBindBuffer(GL_ARRAY_BUFFER, ptr->triangle_tangent_lines_buffer);
	glVertexAttribPointer(
		single_color_shader.vertex_position_id, /* The attribute we want to configure */
		3,                           /* size */
		GL_FLOAT,                    /* type */
		GL_FALSE,                    /* normalized? */
		sizeof(struct vertex_buffer_data), /* stride */
		(void *)offsetof(struct vertex_buffer_data, position.v.x) /* array buffer offset */
	);
	glDrawArrays(GL_LINES, 0, m->ntriangles * 3 * 2);

	/* bitangent lines */
	glUniform4f(single_color_shader.color_id, 0, 0, 1, 1);
	glEnableVertexAttribArray(single_color_shader.vertex_position_id);
	glBindBuffer(GL_ARRAY_BUFFER, ptr->triangle_bitangent_lines_buffer);
	glVertexAttribPointer(
		single_color_shader.vertex_position_id, /* The attribute we want to configure */
		3,                           /* size */
		GL_FLOAT,                    /* type */
		GL_FALSE,                    /* normalized? */
		sizeof(struct vertex_buffer_data), /* stride */
		(void *)offsetof(struct vertex_buffer_data, position.v.x) /* array buffer offset */
	);
	glDrawArrays(GL_LINES, 0, m->ntriangles * 3 * 2);

	glDisable(GL_DEPTH_TEST);
}
#endif

struct raster_texture_params {
	struct graph_dev_gl_textured_shader *shader;
	const struct mat44 *mat_mvp;			/* model view projection matrix */
	const struct mat44 *mat_mv;			/* model view matrix */
	const struct mat33 *mat_normal;			/* Used to get normal vectors into eye space */
	const struct mat44d *model;			/* model matrix (for shadow map lookup) */
	struct mesh *m;
	struct sng_color *triangle_color;
	float alpha;
	union vec3 *eye_light_pos;			/* Position of light in eye (camera) space */
	GLuint texture_number;
	GLuint emit_texture_number;
	GLuint normalmap_id;
	struct shadow_sphere_data *shadow_sphere;
	struct shadow_annulus_data *shadow_annulus;
	float light_color[3];		/* star-tinted direct light colour (u_LightColor) */
	float ambient_color[3];		/* absolute, complement-tinted ambient colour (u_AmbientColor) */
	float ambient_scale;		/* scale on the shaded floor; 1.0 unless a planet dims it */
	int do_cullface;
	int do_blend;
	float ring_texture_v;
	float ring_inner_radius;
	float ring_outer_radius;
	float specular_power;
	float specular_intensity;
	float ambient;
	float emit_intensity;
	float invert;
	float in_shade;
	float atmosphere_brightness;
	union vec3 *water_color;
	float u1, v1;
	float width;
	int textures_not_ready;
};

/* Derive this frame's star-tinted light colour and complementary ambient colour from the
 * entity context (see star_light.c).  With the default white star and zero strengths this
 * yields light = white and ambient = vec3(cx->ambient) -- i.e. the untinted look. */
static void graph_dev_compute_star_light(const struct entity_context *cx,
	float light_color[3], float ambient_color[3])
{
	star_light_colors(cx->star_color, cx->ambient, cx->star_light_tint,
		cx->star_dark_tint, cx->star_shadow_darkening, light_color, ambient_color);
}

static void graph_dev_raster_texture(struct raster_texture_params *p)
{
	PROFILE_ZONE_START("graph_dev_raster_texture");
	const struct graph_dev_gl_textured_shader *shader = p->shader;

	enable_3d_viewport();

	if (!p->m->graph_ptr)
		return;

	if (p->textures_not_ready)
		return;

	struct mesh_gl_info *ptr = p->m->graph_ptr;

	glEnable(GL_DEPTH_TEST);

	if (p->do_cullface)
		glEnable(GL_CULL_FACE);
	else
		glDisable(GL_CULL_FACE);
	if (draw_polygon_as_lines)
		glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

	if (p->do_blend) {
		/* enable depth test but don't write to depth buffer */
		glDepthMask(GL_FALSE);
		glEnable(GL_BLEND);
		BLEND_FUNC(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
	}

	activate_shader(shader);

	if (shader->texture_2d_id >= 0)
		BIND_TEXTURE(GL_TEXTURE0, GL_TEXTURE_2D, p->texture_number);
	else if (shader->texture_cubemap_id >= 0)
		BIND_TEXTURE(GL_TEXTURE0, GL_TEXTURE_CUBE_MAP, p->texture_number);

	if (shader->normalmap_cubemap_id >= 0)
		BIND_TEXTURE(GL_TEXTURE3, GL_TEXTURE_CUBE_MAP, p->normalmap_id);

	if (shader->ambient_id >= 0)
		glUniform1f(shader->ambient_id, p->ambient);
	if (shader->light_color_id >= 0)
		glUniform3f(shader->light_color_id, p->light_color[0], p->light_color[1], p->light_color[2]);
	if (shader->ambient_color_id >= 0)
		glUniform3f(shader->ambient_color_id, p->ambient_color[0], p->ambient_color[1],
			p->ambient_color[2]);
	if (shader->ambient_scale_id >= 0)
		glUniform1f(shader->ambient_scale_id, p->ambient_scale);
	if (shader->filmic_tonemapping_id >= 0)
		glUniform1f(shader->filmic_tonemapping_id, (float) filmic_tonemapping);
	if (shader->tonemapping_gain_id >= 0)
		glUniform1f(shader->tonemapping_gain_id, tonemapping_gain);

	if (shader->normalmap_id >= 0)
		BIND_TEXTURE(GL_TEXTURE3, GL_TEXTURE_2D, p->normalmap_id);

	if (shader->emit_texture_2d_id >= 0)
		BIND_TEXTURE(GL_TEXTURE1, GL_TEXTURE_2D, p->emit_texture_number);

	if (shader->emit_intensity_id >= 0)
		glUniform1f(shader->emit_intensity_id, p->emit_intensity);

	if (shader->light_pos_id >= 0)
		glUniform3f(shader->light_pos_id, p->eye_light_pos->v.x, p->eye_light_pos->v.y, p->eye_light_pos->v.z);

	glUniformMatrix4fv(shader->mvp_matrix_id, 1, GL_FALSE, &p->mat_mvp->m[0][0]);
	if (shader->mv_matrix_id >= 0)
		glUniformMatrix4fv(shader->mv_matrix_id, 1, GL_FALSE, &p->mat_mv->m[0][0]);
	if (shader->normal_matrix_id >= 0)
		glUniformMatrix3fv(shader->normal_matrix_id, 1, GL_FALSE, &p->mat_normal->m[0][0]);
	if (shader->specular_power_id >= 0)
		glUniform1f(shader->specular_power_id, p->specular_power);
	if (shader->specular_intensity_id >= 0)
		glUniform1f(shader->specular_intensity_id, p->specular_intensity);

	glUniform4f(shader->tint_color_id, p->triangle_color->red,
		p->triangle_color->green, p->triangle_color->blue, p->alpha);

	/* Ring texture v value */
	if (shader->ring_texture_v_id >= 0)
		glUniform1f(shader->ring_texture_v_id, p->ring_texture_v);
	if (shader->ring_inner_radius_id >= 0)
		glUniform1f(shader->ring_inner_radius_id, p->ring_inner_radius);
	if (shader->ring_outer_radius_id >= 0)
		glUniform1f(shader->ring_outer_radius_id, p->ring_outer_radius);
	if (shader->invert >= 0)
		glUniform1f(shader->invert, p->invert);
	if (shader->in_shade >= 0)
		glUniform1f(shader->in_shade, p->in_shade);
	if (shader->water_color >= 0 && p->water_color)
		glUniform3f(shader->water_color, p->water_color->v.x, p->water_color->v.y, p->water_color->v.z);
	if (shader->u1v1 >= 0)
		glUniform2f(shader->u1v1, p->u1, p->v1);
	if (shader->texture_width >= 0)
		glUniform1f(shader->texture_width, p->width);

	/* shadow sphere */
	if (shader->shadow_sphere_id >= 0 && p->shadow_sphere)
		glUniform4f(shader->shadow_sphere_id, p->shadow_sphere->eye_pos.v.x, p->shadow_sphere->eye_pos.v.y,
			p->shadow_sphere->eye_pos.v.z, p->shadow_sphere->r * p->shadow_sphere->r);

	/* shadow annulus */
	if (shader->shadow_annulus_texture_id > 0 && p->shadow_annulus) {
		BIND_TEXTURE(GL_TEXTURE1, GL_TEXTURE_2D, p->shadow_annulus->texture_id);

		glUniform4f(shader->shadow_annulus_tint_color_id, p->shadow_annulus->tint_color.red,
			p->shadow_annulus->tint_color.green, p->shadow_annulus->tint_color.blue,
			p->shadow_annulus->alpha);
		glUniform3f(shader->shadow_annulus_center_id, p->shadow_annulus->eye_pos.v.x,
			p->shadow_annulus->eye_pos.v.y, p->shadow_annulus->eye_pos.v.z);

		/* this only works if the ring has an identity quat for its child orientation */
		/* ring disc is in x/y plane, so z is normal */
		union vec3 annulus_normal = { { 0, 0, 1 } };
		union vec3 eye_annulus_normal;
		mat33_x_vec3(p->mat_normal, &annulus_normal, &eye_annulus_normal);
		vec3_normalize_self(&eye_annulus_normal);

		glUniform3f(shader->shadow_annulus_normal_id, eye_annulus_normal.v.x,
			eye_annulus_normal.v.y, eye_annulus_normal.v.z);

		glUniform4f(shader->shadow_annulus_radius_id,
			p->shadow_annulus->r1, p->shadow_annulus->r1 * p->shadow_annulus->r1,
			p->shadow_annulus->r2, p->shadow_annulus->r2 * p->shadow_annulus->r2);
	}

	/* Cascaded shadow map (USE_CSM shader variants). */
	if (shader->shadow_map_enabled_id >= 0) {
		int use_shadow = graph_dev_shadow_map_enabled && shadow_map_ready && p->model != NULL;
		glUniform1i(shader->shadow_map_enabled_id, use_shadow);
		if (shader->shadow_debug_id >= 0)
			glUniform1i(shader->shadow_debug_id, graph_dev_shadow_map_debug);
		if (shader->shadow_pcf_radius_id >= 0)
			glUniform1i(shader->shadow_pcf_radius_id, graph_dev_shadow_pcf_radius);
		if (shader->cascade_split_far_id >= 0)
			glUniform1fv(shader->cascade_split_far_id, shadow_map_num_cascades,
				shadow_cascade_split_far);
		if (shader->shadow_blend_id >= 0)
			glUniform1f(shader->shadow_blend_id, graph_dev_shadow_blend);
		if (use_shadow)
			upload_shadow_receive_uniforms(shader->shadow_mvp_id, shader->num_cascades_id,
				shader->shadow_map_id, shader->shadow_normal_offset_id, p->model);
	}

	glEnableVertexAttribArray(shader->vertex_position_id);
	glBindBuffer(GL_ARRAY_BUFFER, ptr->vertex_buffer);
	glVertexAttribPointer(
		shader->vertex_position_id, /* The attribute we want to configure */
		3,                           /* size */
		GL_FLOAT,                    /* type */
		GL_FALSE,                    /* normalized? */
		sizeof(struct vertex_buffer_data), /* stride */
		(void *)offsetof(struct vertex_buffer_data, position.v.x) /* array buffer offset */
	);

	if (shader->vertex_normal_id >= 0) {
		glEnableVertexAttribArray(shader->vertex_normal_id);
		glBindBuffer(GL_ARRAY_BUFFER, ptr->triangle_vertex_buffer);
		glVertexAttribPointer(
			shader->vertex_normal_id,  /* The attribute we want to configure */
			3,                            /* size */
			GL_FLOAT,                     /* type */
			GL_FALSE,                     /* normalized? */
			sizeof(struct vertex_triangle_buffer_data), /* stride */
			(void *)offsetof(struct vertex_triangle_buffer_data, normal.v.x) /* array buffer offset */
		);
	}

	if (shader->vertex_tangent_id >= 0) {
		glEnableVertexAttribArray(shader->vertex_tangent_id);
		glBindBuffer(GL_ARRAY_BUFFER, ptr->triangle_vertex_buffer);
		glVertexAttribPointer(
			shader->vertex_tangent_id,    /* The attribute we want to configure */
			3,                            /* size */
			GL_FLOAT,                     /* type */
			GL_FALSE,                     /* normalized? */
			sizeof(struct vertex_triangle_buffer_data), /* stride */
			(void *)offsetof(struct vertex_triangle_buffer_data, tangent.v.x) /* array buffer offset */
		);
	}

	if (shader->vertex_bitangent_id >= 0) {
		glEnableVertexAttribArray(shader->vertex_bitangent_id);
		glBindBuffer(GL_ARRAY_BUFFER, ptr->triangle_vertex_buffer);
		glVertexAttribPointer(
			shader->vertex_bitangent_id,  /* The attribute we want to configure */
			3,                            /* size */
			GL_FLOAT,                     /* type */
			GL_FALSE,                     /* normalized? */
			sizeof(struct vertex_triangle_buffer_data), /* stride */
			(void *)offsetof(struct vertex_triangle_buffer_data, bitangent.v.x) /* array buffer offset */
		);
	}

	if (shader->texture_coord_id >= 0) {
		glEnableVertexAttribArray(shader->texture_coord_id);
		glBindBuffer(GL_ARRAY_BUFFER, ptr->triangle_vertex_buffer);
		glVertexAttribPointer(shader->texture_coord_id, 2, GL_FLOAT, GL_TRUE,
			sizeof(struct vertex_triangle_buffer_data),
			(void *)offsetof(struct vertex_triangle_buffer_data, texture_coord.v.x));
	}

	glDrawArrays(GL_TRIANGLES, 0, p->m->ntriangles * 3);

	glDisable(GL_DEPTH_TEST);
	if (p->do_cullface)
		glDisable(GL_CULL_FACE);
	if (draw_polygon_as_lines)
		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	if (p->do_blend) {
		glDepthMask(GL_TRUE);
		glDisable(GL_BLEND);
	}

#if DEBUG_NORMALS
	if (draw_normal_lines) {
		graph_dev_draw_normal_lines(p->mat_mvp, p->m, ptr);
	}
#endif
	PROFILE_ZONE_END();
}

static void graph_dev_raster_single_color_lit(const struct mat44 *mat_mvp, const struct mat44 *mat_mv,
	const struct mat33 *mat_normal, const struct mat44d *model,
	struct mesh *m, struct sng_color *triangle_color, union vec3 *eye_light_pos,
	float in_shade, float ambient, const float light_color[3], const float ambient_color[3])
{
	PROFILE_ZONE_START("graph_dev_raster_single_color_lit");
	enable_3d_viewport();

	if (!m->graph_ptr) {
		PROFILE_ZONE_END();
		return;
	}

	struct mesh_gl_info *ptr = m->graph_ptr;

	/* Use the shadow-receiving variant only if a shadow map was rendered this frame. */
	int use_shadow = graph_dev_shadow_map_enabled && shadow_map_ready && model != NULL;
	struct graph_dev_gl_single_color_lit_shader *shader =
		use_shadow ? &single_color_lit_shadow_shader : &single_color_lit_shader;

	glEnable(GL_DEPTH_TEST);
	glEnable(GL_CULL_FACE);
	if (draw_polygon_as_lines)
		glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

	activate_shader(shader);

	glUniformMatrix4fv(shader->mv_matrix_id, 1, GL_FALSE, &mat_mv->m[0][0]);
	glUniformMatrix4fv(shader->mvp_matrix_id, 1, GL_FALSE, &mat_mvp->m[0][0]);
	glUniformMatrix3fv(shader->normal_matrix_id, 1, GL_FALSE, &mat_normal->m[0][0]);

	glUniform3f(shader->color_id, triangle_color->red,
		triangle_color->green, triangle_color->blue);
	glUniform3f(shader->light_pos_id, eye_light_pos->v.x, eye_light_pos->v.y, eye_light_pos->v.z);
	glUniform1f(shader->in_shade_id, in_shade);
	glUniform1f(shader->ambient_id, ambient);
	if (shader->light_color_id >= 0)
		glUniform3f(shader->light_color_id, light_color[0], light_color[1], light_color[2]);
	if (shader->ambient_color_id >= 0)
		glUniform3f(shader->ambient_color_id, ambient_color[0], ambient_color[1], ambient_color[2]);
	glUniform1f(shader->filmic_tonemapping_id, (float) filmic_tonemapping);
	glUniform1f(shader->tonemapping_gain_id, tonemapping_gain);

	if (use_shadow) {
		if (shader->shadow_map_enabled_id >= 0)
			glUniform1i(shader->shadow_map_enabled_id, use_shadow);
		if (shader->shadow_debug_id >= 0)
			glUniform1i(shader->shadow_debug_id, graph_dev_shadow_map_debug);
		if (shader->shadow_pcf_radius_id >= 0)
			glUniform1i(shader->shadow_pcf_radius_id, graph_dev_shadow_pcf_radius);
		if (shader->cascade_split_far_id >= 0)
			glUniform1fv(shader->cascade_split_far_id, shadow_map_num_cascades,
				shadow_cascade_split_far);
		if (shader->shadow_blend_id >= 0)
			glUniform1f(shader->shadow_blend_id, graph_dev_shadow_blend);
		upload_shadow_receive_uniforms(shader->shadow_mvp_id, shader->num_cascades_id,
			shader->shadow_map_id, shader->shadow_normal_offset_id, model);
	}

	glEnableVertexAttribArray(shader->vertex_position_id);
	glBindBuffer(GL_ARRAY_BUFFER, ptr->vertex_buffer);
	glVertexAttribPointer(
		shader->vertex_position_id, /* The attribute we want to configure */
		3,                           /* size */
		GL_FLOAT,                    /* type */
		GL_FALSE,                    /* normalized? */
		sizeof(struct vertex_buffer_data), /* stride */
		(void *)offsetof(struct vertex_buffer_data, position.v.x) /* array buffer offset */
	);

	glEnableVertexAttribArray(shader->vertex_normal_id);
	glBindBuffer(GL_ARRAY_BUFFER, ptr->triangle_vertex_buffer);
	glVertexAttribPointer(
		shader->vertex_normal_id,  /* The attribute we want to configure */
		3,                            /* size */
		GL_FLOAT,                     /* type */
		GL_FALSE,                     /* normalized? */
		sizeof(struct vertex_triangle_buffer_data), /* stride */
		(void *)offsetof(struct vertex_triangle_buffer_data, normal.v.x) /* array buffer offset */
	);

	glDrawArrays(GL_TRIANGLES, 0, m->ntriangles * 3);

	glDisable(GL_DEPTH_TEST);
	glDisable(GL_CULL_FACE);
	if (draw_polygon_as_lines)
		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

#if DEBUG_NORMALS
	if (draw_normal_lines) {
		graph_dev_draw_normal_lines(mat_mvp, m, ptr);
	}
#endif

	PROFILE_ZONE_END();
}

static void graph_dev_raster_atmosphere(const struct mat44 *mat_mvp, const struct mat44 *mat_mv,
	const struct mat33 *mat_normal,
	struct mesh *m, struct sng_color *triangle_color, union vec3 *eye_light_pos, GLfloat alpha,
	struct shadow_annulus_data *shadow_annulus, float ring_texture_v, float atmosphere_brightness,
	const float light_color[3], const float ambient_color[3])
{
	PROFILE_ZONE_START("graph_dev_raster_atmosphere");

	enable_3d_viewport();
	struct graph_dev_gl_atmosphere_shader *shader;

	if (!draw_atmospheres) {
		PROFILE_ZONE_END();
		return;
	}

	if (!m->graph_ptr) {
		PROFILE_ZONE_END();
		return;
	}

	struct mesh_gl_info *ptr = m->graph_ptr;

	/* enable depth test but don't write to depth buffer */
	glDepthMask(GL_FALSE);
	glEnable(GL_BLEND);
	BLEND_FUNC(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

	glEnable(GL_DEPTH_TEST);
	glEnable(GL_CULL_FACE);
	if (draw_polygon_as_lines)
		glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

	if (ring_texture_v >= 0.0 && graph_dev_atmosphere_ring_shadows) {
		/* Set up uniforms for ring shadow */
		shader = &atmosphere_with_annulus_shadow_shader;
		activate_shader(shader);
		if (shadow_annulus->texture_id > 0 && shader->shadow_annulus_texture_id > 0)
			BIND_TEXTURE(GL_TEXTURE0, GL_TEXTURE_2D, shadow_annulus->texture_id);

		glUniform4f(shader->shadow_annulus_tint_color_id, shadow_annulus->tint_color.red,
			shadow_annulus->tint_color.green, shadow_annulus->tint_color.blue, shadow_annulus->alpha);
		glUniform3f(shader->shadow_annulus_center_id, shadow_annulus->eye_pos.v.x,
			shadow_annulus->eye_pos.v.y, shadow_annulus->eye_pos.v.z);

		/* this only works if the ring has an identity quat for its child orientation */
		/* ring disc is in x/y plane, so z is normal */
		union vec3 annulus_normal = { { 0, 0, 1 } };
		union vec3 eye_annulus_normal;
		mat33_x_vec3(mat_normal, &annulus_normal, &eye_annulus_normal);
		vec3_normalize_self(&eye_annulus_normal);

		glUniform3f(shader->shadow_annulus_normal_id, eye_annulus_normal.v.x,
			eye_annulus_normal.v.y, eye_annulus_normal.v.z);

		glUniform4f(shader->shadow_annulus_radius_id,
			shadow_annulus->r1, shadow_annulus->r1 * shadow_annulus->r1,
			shadow_annulus->r2, shadow_annulus->r2 * shadow_annulus->r2);

	} else {
		shader = &atmosphere_shader;
		activate_shader(shader);
	}

	glUniform1f(shader->atmosphere_brightness_id, atmosphere_brightness);
	if (shader->light_color_id >= 0)
		glUniform3f(shader->light_color_id, light_color[0], light_color[1], light_color[2]);
	if (shader->ambient_color_id >= 0)
		glUniform3f(shader->ambient_color_id, ambient_color[0], ambient_color[1],
				ambient_color[2]);
	glUniformMatrix4fv(shader->mv_matrix_id, 1, GL_FALSE, &mat_mv->m[0][0]);
	glUniformMatrix4fv(shader->mvp_matrix_id, 1, GL_FALSE, &mat_mvp->m[0][0]);
	glUniformMatrix3fv(shader->normal_matrix_id, 1, GL_FALSE, &mat_normal->m[0][0]);

	glUniform3f(shader->color_id, triangle_color->red, triangle_color->green, triangle_color->blue);
	glUniform3f(shader->light_pos_id, eye_light_pos->v.x, eye_light_pos->v.y, eye_light_pos->v.z);
	glUniform1f(shader->alpha, alpha);
	glUniform1f(shader->filmic_tonemapping_id, (float) filmic_tonemapping);
	glUniform1f(shader->tonemapping_gain_id, tonemapping_gain);

	glEnableVertexAttribArray(shader->vertex_position_id);
	glBindBuffer(GL_ARRAY_BUFFER, ptr->vertex_buffer);
	glVertexAttribPointer(
		shader->vertex_position_id,  /* The attribute we want to configure */
		3,                           /* size */
		GL_FLOAT,                    /* type */
		GL_FALSE,                    /* normalized? */
		sizeof(struct vertex_buffer_data), /* stride */
		(void *)offsetof(struct vertex_buffer_data, position.v.x) /* array buffer offset */
	);

	glEnableVertexAttribArray(shader->vertex_normal_id);
	glBindBuffer(GL_ARRAY_BUFFER, ptr->triangle_vertex_buffer);
	glVertexAttribPointer(
		shader->vertex_normal_id,  /* The attribute we want to configure */
		3,                            /* size */
		GL_FLOAT,                     /* type */
		GL_FALSE,                     /* normalized? */
		sizeof(struct vertex_triangle_buffer_data), /* stride */
		(void *)offsetof(struct vertex_triangle_buffer_data, normal.v.x) /* array buffer offset */
	);

	glDrawArrays(GL_TRIANGLES, 0, m->ntriangles * 3);

	glDisable(GL_DEPTH_TEST);
	glDisable(GL_CULL_FACE);
	glDepthMask(GL_TRUE);
	if (draw_polygon_as_lines)
		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

#if DEBUG_NORMALS
	if (draw_normal_lines) {
		graph_dev_draw_normal_lines(mat_mvp, m, ptr);
	}
#endif

	PROFILE_ZONE_END();
}

static void graph_dev_raster_filled_wireframe_mesh(const struct mat44 *mat_mvp, struct mesh *m,
	struct sng_color *line_color, struct sng_color *triangle_color)
{
	PROFILE_ZONE_START("graph_dev_raster_filled_wireframe_mesh");

	enable_3d_viewport();

	if (!m->graph_ptr) {
		PROFILE_ZONE_END();
		return;
	}

	struct mesh_gl_info *ptr = m->graph_ptr;

	glEnable(GL_DEPTH_TEST);
	glEnable(GL_CULL_FACE);

	activate_shader(&filled_wireframe_shader);

	glUniform2f(filled_wireframe_shader.viewport_id, sgc.vp_width_3d, sgc.vp_height_3d);
	glUniformMatrix4fv(filled_wireframe_shader.mvp_matrix_id, 1, GL_FALSE, &mat_mvp->m[0][0]);

	glUniform3f(filled_wireframe_shader.line_color_id, line_color->red,
		line_color->green, line_color->blue);
	glUniform3f(filled_wireframe_shader.triangle_color_id, triangle_color->red,
		triangle_color->green, triangle_color->blue);

	glEnableVertexAttribArray(filled_wireframe_shader.position_id);
	glBindBuffer(GL_ARRAY_BUFFER, ptr->vertex_buffer);
	glVertexAttribPointer(
		filled_wireframe_shader.position_id,
		3,
		GL_FLOAT,
		GL_FALSE,
		sizeof(struct vertex_buffer_data),
		(void *)offsetof(struct vertex_buffer_data, position.v.x)
	);

	glEnableVertexAttribArray(filled_wireframe_shader.tvertex0_id);
	glBindBuffer(GL_ARRAY_BUFFER, ptr->triangle_vertex_buffer);
	glVertexAttribPointer(
		filled_wireframe_shader.tvertex0_id,
		3,
		GL_FLOAT,
		GL_FALSE,
		sizeof(struct vertex_triangle_buffer_data),
		(void *)offsetof(struct vertex_triangle_buffer_data, tvertex0.v.x)
	);

	glEnableVertexAttribArray(filled_wireframe_shader.tvertex1_id);
	glBindBuffer(GL_ARRAY_BUFFER, ptr->triangle_vertex_buffer);
	glVertexAttribPointer(
		filled_wireframe_shader.tvertex1_id,
		3,
		GL_FLOAT,
		GL_FALSE,
		sizeof(struct vertex_triangle_buffer_data),
		(void *)offsetof(struct vertex_triangle_buffer_data, tvertex1.v.x)
	);

	glEnableVertexAttribArray(filled_wireframe_shader.tvertex2_id);
	glBindBuffer(GL_ARRAY_BUFFER, ptr->triangle_vertex_buffer);
	glVertexAttribPointer(
		filled_wireframe_shader.tvertex2_id,
		3,
		GL_FLOAT,
		GL_FALSE,
		sizeof(struct vertex_triangle_buffer_data),
		(void *)offsetof(struct vertex_triangle_buffer_data, tvertex2.v.x)
	);

	glEnableVertexAttribArray(filled_wireframe_shader.edge_mask_id);
	glBindBuffer(GL_ARRAY_BUFFER, ptr->triangle_vertex_buffer);
	glVertexAttribPointer(
		filled_wireframe_shader.edge_mask_id,
		3,
		GL_FLOAT,
		GL_FALSE,
		sizeof(struct vertex_triangle_buffer_data),
		(void *)offsetof(struct vertex_triangle_buffer_data, wireframe_edge_mask.v.x)
	);

	glDrawArrays(GL_TRIANGLES, 0, ptr->ntriangles*3);

	glDisable(GL_DEPTH_TEST);
	glDisable(GL_CULL_FACE);

#if DEBUG_NORMALS
	if (draw_normal_lines) {
		graph_dev_draw_normal_lines(mat_mvp, m, ptr);
	}
#endif

	PROFILE_ZONE_END();
}

static void graph_dev_raster_trans_wireframe_mesh(struct graph_dev_gl_trans_wireframe_shader *shader,
		const struct mat44 *mat_mvp, const struct mat44 *mat_mv,
		const struct mat33 *mat_normal, struct mesh *m, struct sng_color *line_color,
		struct clip_sphere_data *clip_sphere, int do_cullface)
{
	PROFILE_ZONE_START("graph_dev_raster_trans_wireframe_mesh");

	enable_3d_viewport();

	if (!m->graph_ptr) {
		PROFILE_ZONE_END();
		return;
	}

	struct mesh_gl_info *ptr = m->graph_ptr;

	glEnable(GL_DEPTH_TEST);

	if (do_cullface) {
		assert(shader);
		assert(clip_sphere);

		activate_shader(shader);

		glUniformMatrix4fv(shader->mvp_matrix_id, 1, GL_FALSE, &mat_mvp->m[0][0]);
		glUniformMatrix4fv(shader->mv_matrix_id, 1, GL_FALSE, &mat_mv->m[0][0]);
		glUniformMatrix3fv(shader->normal_matrix_id, 1, GL_FALSE, &mat_normal->m[0][0]);
		glUniform3f(shader->color_id, line_color->red,
			line_color->green, line_color->blue);

		if (shader->clip_sphere_id >= 0) {
			glUniform4f(shader->clip_sphere_id, clip_sphere->eye_pos.v.x, clip_sphere->eye_pos.v.y,
				clip_sphere->eye_pos.v.z, clip_sphere->r);

			glUniform1f(shader->clip_sphere_radius_fade_id, clip_sphere->radius_fade);
		}

		glEnableVertexAttribArray(shader->vertex_position_id);
		glBindBuffer(GL_ARRAY_BUFFER, ptr->wireframe_lines_vertex_buffer);
		glVertexAttribPointer(
			shader->vertex_position_id, /* The attribute we want to configure */
			3,                           /* size */
			GL_FLOAT,                    /* type */
			GL_FALSE,                    /* normalized? */
			sizeof(struct vertex_wireframe_line_buffer_data), /* stride */
			(void *)offsetof(struct vertex_wireframe_line_buffer_data, position.v.x) /* array buffer offset */
		);

		glEnableVertexAttribArray(shader->vertex_normal_id);
		glBindBuffer(GL_ARRAY_BUFFER, ptr->wireframe_lines_vertex_buffer);
		glVertexAttribPointer(
			shader->vertex_normal_id,    /* The attribute we want to configure */
			3,                            /* size */
			GL_FLOAT,                     /* type */
			GL_FALSE,                     /* normalized? */
			sizeof(struct vertex_wireframe_line_buffer_data), /* stride */
			(void *)offsetof(struct vertex_wireframe_line_buffer_data, normal.v.x) /* array buffer offset */
		);

	} else {
		/* don't cullface so just render with single color shader */
		activate_shader(&single_color_shader);

		glUniformMatrix4fv(single_color_shader.mvp_matrix_id, 1, GL_FALSE, &mat_mvp->m[0][0]);
		glUniform4f(single_color_shader.color_id, line_color->red,
			line_color->green, line_color->blue, 1.0);

		glEnableVertexAttribArray(single_color_shader.vertex_position_id);
		glBindBuffer(GL_ARRAY_BUFFER, ptr->wireframe_lines_vertex_buffer);
		glVertexAttribPointer(
			single_color_shader.vertex_position_id, /* The attribute we want to configure */
			3,                           /* size */
			GL_FLOAT,                    /* type */
			GL_FALSE,                    /* normalized? */
			sizeof(struct vertex_wireframe_line_buffer_data), /* stride */
			(void *)offsetof(struct vertex_wireframe_line_buffer_data, position.v.x) /* array buffer offset */
		);
	}

	glDrawArrays(GL_LINES, 0, ptr->nwireframe_lines * 2);

	glDisable(GL_DEPTH_TEST);

#if DEBUG_NORMALS
	if (draw_normal_lines) {
		graph_dev_draw_normal_lines(mat_mvp, m, ptr);
	}
#endif

	PROFILE_ZONE_END();
}

static void graph_dev_raster_line_mesh(struct entity *e, const struct mat44 *mat_mvp, struct mesh *m,
					struct sng_color *line_color)
{
	PROFILE_ZONE_START("graph_dev_raster_line_mesh");

	enable_3d_viewport();

	if (!m->graph_ptr) {
		PROFILE_ZONE_END();
		return;
	}

	PROFILE_ZONE_START_CTX(p_setup, "graph_dev_raster_line_mesh:setup");

	struct mesh_gl_info *ptr = m->graph_ptr;

	glEnable(GL_DEPTH_TEST);

	GLuint vertex_position_id;

	if (e->material_ptr && e->material_ptr->type == MATERIAL_COLOR_BY_W) {
		struct material_color_by_w *mc = &e->material_ptr->color_by_w;

		activate_shader(&color_by_w_shader);

		glUniformMatrix4fv(color_by_w_shader.mvp_id, 1, GL_FALSE, &mat_mvp->m[0][0]);

		struct sng_color near_color = sng_get_color(mc->near_color);
		glUniform3f(color_by_w_shader.near_color_id, near_color.red,
			near_color.green, near_color.blue);
		glUniform1f(color_by_w_shader.near_w_id, mc->near_w);

		struct sng_color center_color = sng_get_color(mc->center_color);
		glUniform3f(color_by_w_shader.center_color_id, center_color.red,
			center_color.green, center_color.blue);
		glUniform1f(color_by_w_shader.center_w_id, mc->center_w);

		struct sng_color far_color = sng_get_color(mc->far_color);
		glUniform3f(color_by_w_shader.far_color_id, far_color.red,
			far_color.green, far_color.blue);
		glUniform1f(color_by_w_shader.far_w_id, mc->far_w);

		vertex_position_id = color_by_w_shader.position_id;
	} else {
		activate_shader(&line_single_color_shader);

		glUniformMatrix4fv(line_single_color_shader.mvp_matrix_id, 1, GL_FALSE, &mat_mvp->m[0][0]);
		glUniform2f(line_single_color_shader.viewport_id, sgc.vp_width_3d, sgc.vp_height_3d);

		glUniform1f(line_single_color_shader.dot_size_id, 2.0);
		glUniform1f(line_single_color_shader.dot_pitch_id, 5.0);
		glUniform4f(line_single_color_shader.line_color_id, line_color->red, line_color->green,
			line_color->blue, 1.0);

		vertex_position_id = line_single_color_shader.vertex_position_id;

		PROFILE_ZONE_START_CTX(p_color_arrays, "graph_dev_raster_line_mesh:upload_color_arrays");

		glEnableVertexAttribArray(line_single_color_shader.multi_one_id);
		glBindBuffer(GL_ARRAY_BUFFER, ptr->line_vertex_buffer);
		glVertexAttribPointer(
			line_single_color_shader.multi_one_id, /* The attribute we want to configure */
			4,                           /* size */
			GL_UNSIGNED_BYTE,            /* type */
			GL_TRUE,                     /* normalized? */
			sizeof(struct vertex_line_buffer_data), /* stride */
			(void *)offsetof(struct vertex_line_buffer_data, multi_one) /* array buffer offset */
		);

		glEnableVertexAttribArray(line_single_color_shader.line_vertex0_id);
		glBindBuffer(GL_ARRAY_BUFFER, ptr->line_vertex_buffer);
		glVertexAttribPointer(
			line_single_color_shader.line_vertex0_id,
			3,
			GL_FLOAT,
			GL_FALSE,
			sizeof(struct vertex_line_buffer_data),
			(void *)offsetof(struct vertex_line_buffer_data, line_vertex0.v.x)
		);

		glEnableVertexAttribArray(line_single_color_shader.line_vertex1_id);
		glBindBuffer(GL_ARRAY_BUFFER, ptr->line_vertex_buffer);
		glVertexAttribPointer(
			line_single_color_shader.line_vertex1_id,
			3,
			GL_FLOAT,
			GL_FALSE,
			sizeof(struct vertex_line_buffer_data),
			(void *)offsetof(struct vertex_line_buffer_data, line_vertex1.v.x)
	);

		PROFILE_ZONE_END_CTX(p_color_arrays);
	}

	PROFILE_ZONE_START_CTX(p_vertexbuffer, "graph_dev_raster_line_mesh:upload_vertex_buffer");

	glEnableVertexAttribArray(vertex_position_id);
	glBindBuffer(GL_ARRAY_BUFFER, ptr->vertex_buffer);
	glVertexAttribPointer(
		vertex_position_id, /* The attribute we want to configure */
		3,                           /* size */
		GL_FLOAT,                    /* type */
		GL_FALSE,                    /* normalized? */
		sizeof(struct vertex_buffer_data), /* stride */
		(void *)offsetof(struct vertex_buffer_data, position.v.x) /* array buffer offset */
	);

	PROFILE_ZONE_END_CTX(p_vertexbuffer);

	PROFILE_ZONE_END_CTX(p_setup);
	PROFILE_ZONE_START_CTX(p_render, "graph_dev_raster_line_mesh:draw");

	glDrawArrays(GL_LINES, 0, ptr->nlines * 2);

	PROFILE_ZONE_END_CTX(p_render);

	PROFILE_ZONE_START_CTX(p_cleanup, "graph_dev_raster_line_mesh:cleanup");
	glDisable(GL_DEPTH_TEST);

	PROFILE_ZONE_END_CTX(p_cleanup);

	PROFILE_ZONE_END();
}

/* camera_pos and fade_params are optional: pass NULL for both and the points come out flat,
 * the same size and the same colour, which is what every point cloud but the star field
 * wants.  no_depth_test draws without testing or writing depth, for a cloud that is meant to
 * sit behind the whole scene rather than at a depth of its own. */
void graph_dev_raster_point_cloud_mesh(struct graph_dev_gl_point_cloud_shader *shader,
	const struct mat44 *mat_mvp, struct mesh *m, struct sng_color *point_color, float alpha, float pointSize,
	int do_blend, const float camera_pos[3], const float fade_params[4], int no_depth_test)
{
	PROFILE_ZONE_START("graph_dev_raster_point_cloud_mesh");

	enable_3d_viewport();

	if (!m->graph_ptr) {
		PROFILE_ZONE_END();
		return;
	}

	struct mesh_gl_info *ptr = m->graph_ptr;

	if (!no_depth_test)
		glEnable(GL_DEPTH_TEST);
	glEnable(GL_VERTEX_PROGRAM_POINT_SIZE);

	if (do_blend) {
		/* enable depth test but don't write to depth buffer */
		glDepthMask(GL_FALSE);
		glEnable(GL_BLEND);
		BLEND_FUNC(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
	}

	activate_shader(shader);

	glUniformMatrix4fv(shader->mvp_matrix_id, 1, GL_FALSE, &mat_mvp->m[0][0]);
	glUniform1f(shader->point_size_id, pointSize);
	glUniform4f(shader->color_id, point_color->red,
		point_color->green, point_color->blue, alpha);

	if (shader->time_id >= 0) {
		float time = fmod(time_now_double(), 1.0);
		glUniform1f(shader->time_id, time);
	}

	if (shader->camera_pos_id >= 0)
		glUniform3f(shader->camera_pos_id, camera_pos ? camera_pos[0] : 0.0,
				camera_pos ? camera_pos[1] : 0.0, camera_pos ? camera_pos[2] : 0.0);
	if (shader->fade_params_id >= 0)
		glUniform4f(shader->fade_params_id, fade_params ? fade_params[0] : 0.0,
				fade_params ? fade_params[1] : 0.0, fade_params ? fade_params[2] : 0.0,
				fade_params ? fade_params[3] : 0.0);

	glEnableVertexAttribArray(shader->vertex_position_id);
	glBindBuffer(GL_ARRAY_BUFFER, ptr->vertex_buffer);
	glVertexAttribPointer(
		shader->vertex_position_id, /* The attribute we want to configure */
		3,                           /* size */
		GL_FLOAT,                    /* type */
		GL_FALSE,                    /* normalized? */
		sizeof(struct vertex_buffer_data), /* stride */
		(void *)offsetof(struct vertex_buffer_data, position.v.x) /* array buffer offset */
	);

	glDrawArrays(GL_POINTS, 0, ptr->npoints);

	glDisable(GL_DEPTH_TEST);
	glDisable(GL_VERTEX_PROGRAM_POINT_SIZE);
	if (do_blend) {
		glDepthMask(GL_TRUE);
		glDisable(GL_BLEND);
	}

	PROFILE_ZONE_END();
}

static void graph_dev_draw_nebula(const struct mat44 *mat_mvp, const struct mat44 *mat_mv,
	struct entity *e)
{
	PROFILE_ZONE_START("graph_dev_draw_nebula");

	struct material_nebula *mt = &e->material_ptr->nebula;
	struct raster_texture_params rtp = { 0 };

	/* Neutral unless a material dims it; see u_AmbientScale in the cubemap shader. */
	rtp.ambient_scale = 1.0;

	/* transform model origin into camera space */
	union vec4 ent_pos = { { 0.0, 0.0, 0.0, 1.0 } };
	union vec4 camera_ent_pos_4;
	mat44_x_vec4(mat_mv, &ent_pos, &camera_ent_pos_4);

	union vec3 camera_pos = { { 0, 0, 0 } };
	union vec3 camera_ent_pos;
	vec4_to_vec3(&camera_ent_pos_4, &camera_ent_pos);

	union vec3 camera_ent_vector;
	vec3_sub(&camera_ent_vector, &camera_pos, &camera_ent_pos);
	vec3_normalize_self(&camera_ent_vector);

	int i;
	for (i = 0; i < MATERIAL_NEBULA_NPLANES; i++) {
		struct mat44 mat_local_r;
		quat_to_rh_rot_matrix(&mt->orientation[i], &mat_local_r.m[0][0]);

		struct mat44 mat_mvp_local_r;
		mat44_product(mat_mvp, &mat_local_r, &mat_mvp_local_r);

		struct mat44 mat_mv_local_r;
		mat44_product(mat_mv, &mat_local_r, &mat_mv_local_r);

		struct mat33 mat_normal_local_r;
		struct mat33 mat_tmp33;
		mat33_inverse_transpose_ff(mat44_to_mat33_ff(&mat_mv_local_r, &mat_tmp33), &mat_normal_local_r);

		/* rotate the triangle normal into camera space */
		union vec3 *ent_normal = (union vec3 *)&e->m->t[0].n.x;
		union vec3 camera_normal;
		mat33_x_vec3(&mat_normal_local_r, ent_normal, &camera_normal);
		vec3_normalize_self(&camera_normal);

		float alpha = fabs(vec3_dot(&camera_normal, &camera_ent_vector)) * mt->alpha;

		/* Only setting parts that textured.shader actually uses, the rest zeroed above. */
		rtp.shader = &textured_shader;
		rtp.mat_mvp = &mat_mvp_local_r;
		rtp.m = e->m;
		rtp.triangle_color = &mt->tint;
		rtp.alpha = alpha;
		rtp.texture_number = mt->texture_id[i];
		rtp.textures_not_ready = !graph_dev_texture_ready(rtp.texture_number);
		rtp.do_cullface = 0;
		rtp.do_blend = 1;
		rtp.ambient = 0.1;

		graph_dev_raster_texture(&rtp);

		if (draw_billboard_wireframe) {
			struct sng_color line_color = sng_get_color(WHITE);
			graph_dev_raster_trans_wireframe_mesh(0, &mat_mvp_local_r, &mat_mv_local_r,
				&mat_normal_local_r, e->m, &line_color, 0, 0);
		}
	}

	PROFILE_ZONE_END();
}

static void graph_dev_raster_particle_animation(struct entity *e,
	const struct entity_transform *transform, GLuint texture_number,
	float particle_radius, float time_base)
{
	PROFILE_ZONE_START("graph_dev_raster_particle_animation");

	enable_3d_viewport();

	if (!e->m->graph_ptr) {
		PROFILE_ZONE_END();
		return;
	}

	struct mesh_gl_info *ptr = e->m->graph_ptr;

	/* enable depth test but don't write to depth buffer */
	glEnable(GL_DEPTH_TEST);
	glDepthMask(GL_FALSE);

	if (draw_polygon_as_lines)
		glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

	glEnable(GL_BLEND);
	BLEND_FUNC(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

	activate_shader(&textured_particle_shader);

	glUniformMatrix4fv(textured_particle_shader.mvp_matrix_id, 1, GL_FALSE, &transform->mvp.m[0][0]);

	/* need the transpose of model and view rotation */
	struct mat33d v_rotation, m_rotation;
	mat44_to_mat33_dd(transform->v, &v_rotation);
	mat44_to_mat33_dd(&transform->m_no_scale, &m_rotation);

	struct mat33 mv_rotation;
	mat33_product_ddf(&v_rotation, &m_rotation, &mv_rotation);

	struct mat33 mat_view_to_model;
	mat33_transpose(&mv_rotation, &mat_view_to_model);

	union vec3 camera_up_view = { { 0, 1, 0 } };
	union vec3 camera_up_model;
	mat33_x_vec3(&mat_view_to_model, &camera_up_view, &camera_up_model);
	vec3_normalize_self(&camera_up_model);

	union vec3 camera_right_view = { { 1, 0, 0 } };
	union vec3 camera_right_model;
	mat33_x_vec3(&mat_view_to_model, &camera_right_view, &camera_right_model);
	vec3_normalize_self(&camera_right_model);

	glUniform3f(textured_particle_shader.camera_up_vec_id, camera_up_model.v.x, camera_up_model.v.y,
		camera_up_model.v.z);
	glUniform3f(textured_particle_shader.camera_right_vec_id, camera_right_model.v.x, camera_right_model.v.y,
		camera_right_model.v.z);

	double time_now = time_now_double();
	double fmoded_time = fmod(time_now, time_base);
	float anim_time = fmoded_time / time_base;
	glUniform1f(textured_particle_shader.time_id, anim_time);

	glUniform1f(textured_particle_shader.radius_id, particle_radius);
	glUniform1f(textured_particle_shader.filmic_tonemapping_id, (float) filmic_tonemapping);
	glUniform1f(textured_particle_shader.tonemapping_gain_id, tonemapping_gain);

	BIND_TEXTURE(GL_TEXTURE0, GL_TEXTURE_2D, texture_number);

	glEnableVertexAttribArray(textured_particle_shader.multi_one_id);
	glBindBuffer(GL_ARRAY_BUFFER, ptr->particle_vertex_buffer);
	glVertexAttribPointer(
		textured_particle_shader.multi_one_id, /* The attribute we want to configure */
		4,                           /* size */
		GL_UNSIGNED_BYTE,            /* type */
		GL_TRUE,                     /* normalized? */
		sizeof(struct vertex_particle_buffer_data), /* stride */
		(void *)offsetof(struct vertex_particle_buffer_data, multi_one) /* array buffer offset */
	);

	glEnableVertexAttribArray(textured_particle_shader.start_position_id);
	glBindBuffer(GL_ARRAY_BUFFER, ptr->particle_vertex_buffer);
	glVertexAttribPointer(
		textured_particle_shader.start_position_id, /* The attribute we want to configure */
		3,                           /* size */
		GL_FLOAT,                    /* type */
		GL_FALSE,                    /* normalized? */
		sizeof(struct vertex_particle_buffer_data), /* stride */
		(void *)offsetof(struct vertex_particle_buffer_data, start_position.v.x) /* array buffer offset */
	);

	glEnableVertexAttribArray(textured_particle_shader.start_tint_color_id);
	glBindBuffer(GL_ARRAY_BUFFER, ptr->particle_vertex_buffer);
	glVertexAttribPointer(
		textured_particle_shader.start_tint_color_id, /* The attribute we want to configure */
		3,                           /* size */
		GL_UNSIGNED_BYTE,            /* type */
		GL_TRUE,                     /* normalized? */
		sizeof(struct vertex_particle_buffer_data), /* stride */
		(void *)offsetof(struct vertex_particle_buffer_data, start_tint_color) /* array buffer offset */
	);

	glEnableVertexAttribArray(textured_particle_shader.start_apm_id);
	glBindBuffer(GL_ARRAY_BUFFER, ptr->particle_vertex_buffer);
	glVertexAttribPointer(
		textured_particle_shader.start_apm_id, /* The attribute we want to configure */
		2,                           /* size */
		GL_UNSIGNED_BYTE,            /* type */
		GL_TRUE,                     /* normalized? */
		sizeof(struct vertex_particle_buffer_data), /* stride */
		(void *)offsetof(struct vertex_particle_buffer_data, start_apm) /* array buffer offset */
	);

	glEnableVertexAttribArray(textured_particle_shader.end_position_id);
	glBindBuffer(GL_ARRAY_BUFFER, ptr->particle_vertex_buffer);
	glVertexAttribPointer(
		textured_particle_shader.end_position_id, /* The attribute we want to configure */
		3,                           /* size */
		GL_FLOAT,                    /* type */
		GL_FALSE,                    /* normalized? */
		sizeof(struct vertex_particle_buffer_data), /* stride */
		(void *)offsetof(struct vertex_particle_buffer_data, end_position.v.x) /* array buffer offset */
	);

	glEnableVertexAttribArray(textured_particle_shader.end_tint_color_id);
	glBindBuffer(GL_ARRAY_BUFFER, ptr->particle_vertex_buffer);
	glVertexAttribPointer(
		textured_particle_shader.end_tint_color_id, /* The attribute we want to configure */
		3,                           /* size */
		GL_UNSIGNED_BYTE,            /* type */
		GL_TRUE,                     /* normalized? */
		sizeof(struct vertex_particle_buffer_data), /* stride */
		(void *)offsetof(struct vertex_particle_buffer_data, end_tint_color) /* array buffer offset */
	);

	glEnableVertexAttribArray(textured_particle_shader.end_apm_id);
	glBindBuffer(GL_ARRAY_BUFFER, ptr->particle_vertex_buffer);
	glVertexAttribPointer(
		textured_particle_shader.end_apm_id, /* The attribute we want to configure */
		2,                           /* size */
		GL_UNSIGNED_BYTE,            /* type */
		GL_TRUE,                     /* normalized? */
		sizeof(struct vertex_particle_buffer_data), /* stride */
		(void *)offsetof(struct vertex_particle_buffer_data, end_apm) /* array buffer offset */
	);

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ptr->particle_index_buffer);
	glDrawElements(GL_TRIANGLES, ptr->nparticles * 6, GL_UNSIGNED_SHORT, NULL);

	glDisable(GL_DEPTH_TEST);
	glDepthMask(GL_TRUE);
	if (draw_polygon_as_lines)
		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	glDisable(GL_BLEND);

	if (draw_billboard_wireframe) {
		struct sng_color white = sng_get_color(WHITE);
		graph_dev_raster_line_mesh(e, &transform->mvp, e->m, &white);

		struct sng_color red = sng_get_color(RED);
		graph_dev_raster_point_cloud_mesh(&point_cloud_shader, &transform->mvp, e->m, &red, 1.0, 3.0, 0,
			NULL, NULL, 0);
	}

	PROFILE_ZONE_END();
}

extern int graph_dev_entity_render_order(struct entity *e)
{
	int does_blending = 0;

	if (!e->material_ptr)
		return GRAPH_DEV_RENDER_NEAR_TO_FAR;

	switch (e->material_ptr->type) {
	case MATERIAL_NEBULA:
	case MATERIAL_TEXTURED_PARTICLE:
	case MATERIAL_TEXTURED_PLANET_RING:
	case MATERIAL_TEXTURED_SHIELD:
	case MATERIAL_ALPHA_BY_NORMAL:
	case MATERIAL_PLANETARY_LIGHTNING:
	case MATERIAL_WARP_GATE_EFFECT:
	case MATERIAL_SUN:
	case MATERIAL_BLACK_HOLE:
		does_blending = 1;
		break;
	case MATERIAL_TEXTURE_MAPPED_UNLIT:
		does_blending = e->material_ptr->texture_mapped_unlit.do_blend;
		break;
	case MATERIAL_TEXTURE_CUBEMAP:
		does_blending = e->material_ptr->texture_cubemap.do_blend;
		break;
	}

	if (does_blending)
		return GRAPH_DEV_RENDER_FAR_TO_NEAR;
	else
		return GRAPH_DEV_RENDER_NEAR_TO_FAR;
}

/* Draw a star billboard: one procedurally computed profile -- the star's disc already convolved
 * with the optics' point spread function -- plus its diffraction spikes.  The disc radius (in UV)
 * is taken from the material, which the caller sets per frame from the star's world radius over
 * the billboard's world size; the shader divides by it to work in star radii. */
static void graph_dev_raster_sun(const struct mat44 *mat_mvp, struct mesh *m, struct material *material)
{
	struct mesh_gl_info *ptr = m->graph_ptr;
	struct material_sun *sun = &material->sun;

	glEnable(GL_DEPTH_TEST);
	glDepthMask(GL_FALSE); /* blended: test against depth but do not write it */
	glEnable(GL_BLEND);
	BLEND_FUNC(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

	activate_shader(&sun_shader);

	glUniformMatrix4fv(sun_shader.mvp_matrix_id, 1, GL_FALSE, &mat_mvp->m[0][0]);
	glUniform3f(sun_shader.color_id, sun->color.red, sun->color.green, sun->color.blue);
	glUniform1f(sun_shader.brightness_id, sun->brightness);
	glUniform1f(sun_shader.disc_radius_id, sun->disc_radius);
	glUniform1f(sun_shader.edge_softness_id, sun->edge_softness);
	glUniform1f(sun_shader.psf_width_id, sun->psf_width);
	glUniform1f(sun_shader.psf_falloff_id, sun->psf_falloff);
	/* The star is built in linear HDR and tonemapped here, exactly as the lit shaders do it. */
	glUniform1f(sun_shader.filmic_tonemapping_id, (float) filmic_tonemapping);
	glUniform1f(sun_shader.tonemapping_gain_id, tonemapping_gain);

	glEnableVertexAttribArray(sun_shader.vertex_position_id);
	glBindBuffer(GL_ARRAY_BUFFER, ptr->vertex_buffer);
	glVertexAttribPointer(sun_shader.vertex_position_id, 3, GL_FLOAT, GL_FALSE,
		sizeof(struct vertex_buffer_data),
		(void *) offsetof(struct vertex_buffer_data, position.v.x));

	if (sun_shader.texture_coord_id >= 0) {
		glEnableVertexAttribArray(sun_shader.texture_coord_id);
		glBindBuffer(GL_ARRAY_BUFFER, ptr->triangle_vertex_buffer);
		glVertexAttribPointer(sun_shader.texture_coord_id, 2, GL_FLOAT, GL_TRUE,
			sizeof(struct vertex_triangle_buffer_data),
			(void *) offsetof(struct vertex_triangle_buffer_data, texture_coord.v.x));
	}

	glDrawArrays(GL_TRIANGLES, 0, m->ntriangles * 3);

	glDisable(GL_DEPTH_TEST);
	glDepthMask(GL_TRUE);
	glDisable(GL_BLEND);
}

/* Draw an event horizon: an opaque black disc with a thin bright rim, computed procedurally by
 * the black hole shader.  As with the sun, the disc radius (in UV) comes from the material and
 * the caller sets it per frame, so the disc stays world-scale while the billboard is sized a
 * little larger to leave the rim glow somewhere to go. */
static void graph_dev_raster_black_hole(const struct mat44 *mat_mvp, struct mesh *m,
					struct material *material)
{
	struct mesh_gl_info *ptr = m->graph_ptr;
	struct material_black_hole *bh = &material->black_hole;

	glEnable(GL_DEPTH_TEST);
	/* Blended, so no depth write -- but the disc is opaque, and the far-to-near ordering
	 * graph_dev_render_order() gives it is what keeps things behind it hidden. */
	glDepthMask(GL_FALSE);
	glEnable(GL_BLEND);
	BLEND_FUNC(GL_ONE, GL_ONE_MINUS_SRC_ALPHA); /* premultiplied; see black_hole.frag */

	activate_shader(&black_hole_shader);

	glUniformMatrix4fv(black_hole_shader.mvp_matrix_id, 1, GL_FALSE, &mat_mvp->m[0][0]);
	glUniform1f(black_hole_shader.disc_radius_id, bh->disc_radius);
	glUniform1f(black_hole_shader.edge_softness_id, bh->edge_softness);
	glUniform1f(black_hole_shader.ring_brightness_id, bh->ring_brightness);
	glUniform1f(black_hole_shader.ring_width_id, bh->ring_width);
	glUniform1f(black_hole_shader.einstein_radius_id, bh->einstein_radius);
	glUniform1f(black_hole_shader.glow_brightness_id, bh->glow_brightness);
	glUniform1f(black_hole_shader.glow_width_id, bh->glow_width);
	glUniform3f(black_hole_shader.ring_color_id, bh->ring_color.red, bh->ring_color.green,
			bh->ring_color.blue);

	glEnableVertexAttribArray(black_hole_shader.vertex_position_id);
	glBindBuffer(GL_ARRAY_BUFFER, ptr->vertex_buffer);
	glVertexAttribPointer(black_hole_shader.vertex_position_id, 3, GL_FLOAT, GL_FALSE,
		sizeof(struct vertex_buffer_data),
		(void *) offsetof(struct vertex_buffer_data, position.v.x));

	if (black_hole_shader.texture_coord_id >= 0) {
		glEnableVertexAttribArray(black_hole_shader.texture_coord_id);
		glBindBuffer(GL_ARRAY_BUFFER, ptr->triangle_vertex_buffer);
		glVertexAttribPointer(black_hole_shader.texture_coord_id, 2, GL_FLOAT, GL_TRUE,
			sizeof(struct vertex_triangle_buffer_data),
			(void *) offsetof(struct vertex_triangle_buffer_data, texture_coord.v.x));
	}

	glDrawArrays(GL_TRIANGLES, 0, m->ntriangles * 3);

	glDisable(GL_DEPTH_TEST);
	glDepthMask(GL_TRUE);
	glDisable(GL_BLEND);
}
/* See graph_dev.h.  Inert by default (floor 1.0): pulling ambient down changes how every lit
 * thing in the game looks, so nothing happens until a caller asks for it. */
static float shade_ambient_lo = 0.5;
static float shade_ambient_hi = 1.5;
static float shade_ambient_floor = 1.0;

void graph_dev_set_shade_ambient_ramp(float lo, float hi, float floor)
{
	shade_ambient_lo = lo;
	shade_ambient_hi = hi;
	shade_ambient_floor = clampf(floor, 0.0, 1.0);
}

void graph_dev_get_shade_ambient_ramp(float *lo, float *hi, float *floor)
{
	if (lo)
		*lo = shade_ambient_lo;
	if (hi)
		*hi = shade_ambient_hi;
	if (floor)
		*floor = shade_ambient_floor;
}

/* What to multiply an entity's ambient by, given its raw (unclipped) in-shade value. */
static float shade_ambient_scale(float in_shade)
{
	float t;

	if (shade_ambient_floor >= 1.0 || shade_ambient_hi <= shade_ambient_lo)
		return 1.0;
	t = clampf((in_shade - shade_ambient_lo) / (shade_ambient_hi - shade_ambient_lo), 0.0, 1.0);
	return 1.0 + t * (shade_ambient_floor - 1.0);
}


static void graph_dev_raster_triangle_mesh(struct entity_context *cx, struct entity *e,
	union vec3 *eye_light_pos, const struct entity_transform *transform, struct sng_color *line_color)
{
	PROFILE_ZONE_START("graph_dev_raster_triangle_mesh");

	struct camera_info *c = &cx->camera;
	struct raster_texture_params rtp = { 0 };

	/* Neutral unless a material dims it; see u_AmbientScale in the cubemap shader. */
	rtp.ambient_scale = 1.0;
	struct sng_color atmosphere_color = { 0 };
	union vec3 water_color;

	int filled_triangle = ((c->renderer & FLATSHADING_RENDERER) || (c->renderer & BLACK_TRIS))
				&& !(e->render_style & RENDER_NO_FILL);
	int outline_triangle = (c->renderer & WIREFRAME_RENDERER)
				|| (e->render_style & RENDER_WIREFRAME);

	int atmosphere = 0;
	int is_sun = 0;
	int is_black_hole = 0;
	float shade_scale = 1.0; /* the shadow's pull on ambient; not rtp.ambient_scale */
	struct sng_color texture_tint = { 1.0, 1.0, 1.0 };

	rtp.mat_mvp = &transform->mvp;
	rtp.mat_mv = &transform->mv;
	rtp.model = &transform->m;
	rtp.mat_normal = &transform->normal;
	rtp.ring_texture_v = 0.0f;
	rtp.ring_inner_radius = 1.0f;
	rtp.ring_outer_radius = 4.0f;
	rtp.specular_power = 512.0;
	rtp.specular_intensity = 0.2;
	rtp.eye_light_pos = eye_light_pos;
	rtp.alpha = 1.0;
	rtp.texture_number = 0;
	rtp.emit_intensity = 1.0;
	rtp.emit_texture_number = 0;

	/* for sphere shadows */
	struct shadow_sphere_data shadow_sphere;
	vec3_init(&shadow_sphere.eye_pos, 0, 0, 0);
	shadow_sphere.r = 0;

	/* for annulus shadows */
	struct shadow_annulus_data shadow_annulus;
	shadow_annulus.texture_id = 0;
	vec3_init(&shadow_annulus.eye_pos, 0, 0, 0);
	shadow_annulus.r1 = 0;
	shadow_annulus.r2 = 0;
	shadow_annulus.tint_color = sng_get_color(WHITE);
	shadow_annulus.alpha = 1.0;

	/* For planetary water specular calculations */
	water_color.v.x = 0.0; /* These will be set by planet material */
	water_color.v.y = 0.0;
	water_color.v.z = 0.0;

	struct graph_dev_gl_trans_wireframe_shader *wireframe_trans_shader = &trans_wireframe_shader;

	/* for clipping sphere (e.g. on NAV screen when sensor power is low) */
	struct clip_sphere_data clip_sphere;
	vec3_init(&clip_sphere.eye_pos, 0, 0, 0);
	clip_sphere.r = 0;
	clip_sphere.radius_fade = 0;

	if (e->material_ptr) {
		switch (e->material_ptr->type) {
		case MATERIAL_TEXTURE_MAPPED: {
			rtp.shader = &textured_lit_shader;

			struct material_texture_mapped *mt = &e->material_ptr->texture_mapped;
			rtp.texture_number = mt->texture_id;
			rtp.emit_texture_number = mt->emit_texture_id;
			rtp.specular_power = mt->specular_power;
			rtp.specular_intensity = mt->specular_intensity;
			rtp.emit_intensity = mt->emit_intensity * e->emit_intensity;
			rtp.normalmap_id = mt->normalmap_id;
			rtp.do_cullface = 1;

			rtp.textures_not_ready = !graph_dev_textures_ready(
				(int []) {rtp.texture_number, rtp.emit_texture_number,
						rtp.normalmap_id, -1 });

			if (rtp.emit_texture_number > 0 && rtp.normalmap_id > 0)
				rtp.shader = &textured_lit_emit_normal_shader;
			else if (rtp.normalmap_id > 0)
				rtp.shader = &textured_lit_normal_shader;
			else if (rtp.emit_texture_number > 0)
				rtp.shader = &textured_lit_emit_shader;
			else
				rtp.shader = &textured_lit_shader;
			}
			break;
		case MATERIAL_PLANETARY_LIGHTNING: {
			rtp.shader = &planetary_lightning_shader;

			struct material_planetary_lightning *mt = &e->material_ptr->planetary_lightning;
			rtp.u1 = mt->u1;
			rtp.v1 = mt->v1;
			rtp.width = mt->width;
			rtp.texture_number = mt->texture_id;
			rtp.do_cullface = 1;
			rtp.do_blend = 1;
			rtp.alpha = 1.0;
			rtp.emit_texture_number = 0;
			rtp.normalmap_id = 0;
			rtp.textures_not_ready = !graph_dev_textures_ready(
				(int []) { rtp.texture_number, rtp.emit_texture_number,
						rtp.normalmap_id, -1});
			}
			break;
		case MATERIAL_WARP_GATE_EFFECT: {
			rtp.shader = &warp_gate_effect_shader;

			struct material_warp_gate_effect *mt =
					&e->material_ptr->warp_gate_effect;
			rtp.texture_number = mt->texture_id;
			rtp.u1 = mt->u1;
			rtp.v1 = mt->u2;
			rtp.do_cullface = 0;
			rtp.do_blend = 1;
			rtp.alpha = 1.0;
			rtp.textures_not_ready = !graph_dev_texture_ready(rtp.texture_number);
			}
			break;
		case MATERIAL_TEXTURE_MAPPED_UNLIT: {
			rtp.shader = &textured_shader;

			struct material_texture_mapped_unlit *mt =
					&e->material_ptr->texture_mapped_unlit;
			rtp.texture_number = mt->texture_id;
			rtp.do_cullface = mt->do_cullface;
			rtp.do_blend = mt->do_blend;
			rtp.alpha = mt->alpha;
			texture_tint = mt->tint;
			rtp.textures_not_ready = !graph_dev_texture_ready(rtp.texture_number);
			}
			break;
		case MATERIAL_SUN:
			/* Handled by graph_dev_raster_sun() below; rtp.shader stays NULL. */
			is_sun = 1;
			break;
		case MATERIAL_BLACK_HOLE:
			/* Handled by graph_dev_raster_black_hole() below; rtp.shader stays NULL. */
			is_black_hole = 1;
			break;
		case MATERIAL_ATMOSPHERE: {
			rtp.textures_not_ready = 0; /* assume textures are ready until proven otherwise */
			rtp.do_blend = 1;
			rtp.alpha = entity_get_alpha(e);
			if (e->material_ptr->atmosphere.brightness)
				rtp.atmosphere_brightness =
					*e->material_ptr->atmosphere.brightness *
						e->material_ptr->atmosphere.brightness_modifier;
			else
				rtp.atmosphere_brightness = 0.5;
			atmosphere = 1;
			atmosphere_color.red = e->material_ptr->atmosphere.r;
			atmosphere_color.green = e->material_ptr->atmosphere.g;
			atmosphere_color.blue = e->material_ptr->atmosphere.b;
			struct material_atmosphere *mt = &e->material_ptr->atmosphere;
			if (mt->ring_material && mt->ring_material->type == MATERIAL_TEXTURED_PLANET_RING) {
				struct material_textured_planet_ring *ring_mt =
					&mt->ring_material->textured_planet_ring;
				rtp.ring_texture_v = ring_mt->texture_v;
				rtp.ring_inner_radius = ring_mt->inner_radius;
				rtp.ring_outer_radius = ring_mt->outer_radius;

				shadow_annulus.texture_id = ring_mt->texture_id;
				rtp.textures_not_ready = !graph_dev_texture_ready(ring_mt->texture_id);
				shadow_annulus.tint_color = ring_mt->tint;
				shadow_annulus.alpha = ring_mt->alpha;

				/* ring is at the center of our mesh */
				union vec4 sphere_pos = { { 0, 0, 0, 1 } };
				mat44_x_vec4_into_vec3(rtp.mat_mv, &sphere_pos, &shadow_annulus.eye_pos);

				/* ring is the 2x to 3x of the planet scale, world space distance
				   is the same in eye space as the view matrix does not scale */
				shadow_annulus.r1 = vec3_cwise_max(&e->scale) * rtp.ring_inner_radius;
				shadow_annulus.r2 = vec3_cwise_max(&e->scale) * rtp.ring_outer_radius;
			} else {
				/* signal absence of ring with these values */
				rtp.ring_texture_v = -1.0;
				shadow_annulus.texture_id = -1;
				shadow_annulus.tint_color.red = 0;
				shadow_annulus.tint_color.green = 0;
				shadow_annulus.tint_color.blue = 0;
				shadow_annulus.alpha = 0.0;
				shadow_annulus.r1 = 0.0;
				shadow_annulus.r2 = 0.0;
			}
			}
			break;
		case MATERIAL_ALPHA_BY_NORMAL: {
			struct material_alpha_by_normal *mt = &e->material_ptr->alpha_by_normal;
			rtp.texture_number = mt->texture_id;
			if (mt->texture_id > 0) {
				rtp.shader = &textured_alpha_by_normal_shader;
				rtp.textures_not_ready = !graph_dev_texture_ready(rtp.texture_number);
			} else {
				rtp.shader = &alpha_by_normal_shader;
				rtp.textures_not_ready = 0;
			}
			rtp.do_blend = 1;
			rtp.alpha = mt->alpha;
			rtp.do_cullface = mt->do_cullface;
			texture_tint = mt->tint;
			rtp.invert = mt->invert;
			break;
			}
		case MATERIAL_TEXTURE_CUBEMAP: {
			/* As of 2026-08-02 only asteroids in SNIS use MATERIAL_TEXTURE_CUBEMAP,
			 * and we want asteroids to be able to receive CSM shadows.
			 */
			rtp.shader = &textured_cubemap_lit_shadow_shader;

			struct material_texture_cubemap *mt = &e->material_ptr->texture_cubemap;
			rtp.texture_number = mt->texture_id;
			rtp.do_cullface = mt->do_cullface;
			rtp.do_blend = mt->do_blend;
			rtp.alpha = mt->alpha;
			texture_tint = mt->tint;
			rtp.textures_not_ready = !graph_dev_texture_ready(rtp.texture_number);
			}
			break;
		case MATERIAL_TEXTURED_SHIELD: {
			struct material_textured_shield *mt = &e->material_ptr->textured_shield;
			rtp.texture_number = mt->texture_id;
			rtp.shader = &textured_cubemap_shield_shader;
			rtp.do_blend = 1;
			rtp.alpha = entity_get_alpha(e);
			rtp.do_cullface = 0;
			rtp.textures_not_ready = !graph_dev_texture_ready(rtp.texture_number);
			}
			break;
		case MATERIAL_TEXTURED_PLANET: {
			struct material_textured_planet *mt = &e->material_ptr->textured_planet;
			rtp.ambient_scale = PLANET_AMBIENT_SCALE;
			rtp.texture_number = mt->texture_id;
			rtp.normalmap_id = mt->normalmap_id;
			water_color.v.x = mt->water_color_r;
			water_color.v.y = mt->water_color_g;
			water_color.v.z = mt->water_color_b;
			rtp.textures_not_ready = !graph_dev_textures_ready(
				(int []) {rtp.texture_number, rtp.normalmap_id, -1});

			if (mt->ring_material &&
				mt->ring_material->type == MATERIAL_TEXTURED_PLANET_RING) {
				if (rtp.normalmap_id <= 0) {
					if (graph_dev_planets_receive_csm_shadows)
						rtp.shader = &textured_cubemap_lit_with_annulus_shadow_shader;
					else
						rtp.shader = &textured_cubemap_lit_with_annulus_shader;
				} else if (graph_dev_planet_specularity)  {
					if (graph_dev_planets_receive_csm_shadows)
						rtp.shader =
							&textured_cubemap_normal_mapped_lit_with_annulus_shadow_specular_shader;
					else
						rtp.shader =
							&textured_cubemap_normal_mapped_lit_with_annulus_specular_shader;
				} else {
					if (graph_dev_planets_receive_csm_shadows)
						rtp.shader =
							&textured_cubemap_normal_mapped_lit_with_annulus_shadow_shader;
					else
						rtp.shader =
							&textured_cubemap_normal_mapped_lit_with_annulus_shader;
				}

				struct material_textured_planet_ring *ring_mt =
					&mt->ring_material->textured_planet_ring;
				rtp.ring_texture_v = ring_mt->texture_v;
				rtp.ring_inner_radius = ring_mt->inner_radius;
				rtp.ring_outer_radius = ring_mt->outer_radius;

				shadow_annulus.texture_id = ring_mt->texture_id;
				shadow_annulus.tint_color = ring_mt->tint;
				shadow_annulus.alpha = ring_mt->alpha;

				rtp.textures_not_ready |=
					!graph_dev_texture_ready(shadow_annulus.texture_id);

				/* ring is at the center of our mesh */
				union vec4 sphere_pos = { { 0, 0, 0, 1 } };
				mat44_x_vec4_into_vec3(rtp.mat_mv, &sphere_pos, &shadow_annulus.eye_pos);

				/* ring is the 2x to 3x of the planet scale, world space distance
				   is the same in eye space as the view matrix does not scale */
				shadow_annulus.r1 = vec3_cwise_max(&e->scale) *
								rtp.ring_inner_radius;
				shadow_annulus.r2 = vec3_cwise_max(&e->scale) *
								rtp.ring_outer_radius;
			} else {
				if (rtp.normalmap_id > 0) {
					if (graph_dev_planet_specularity) {
						if (graph_dev_planets_receive_csm_shadows)
							rtp.shader =
								&textured_cubemap_normal_mapped_lit_specular_shadow_shader;
						else
							rtp.shader =
								&textured_cubemap_normal_mapped_lit_specular_shader;
					} else {
						if (graph_dev_planets_receive_csm_shadows)
							rtp.shader = &textured_cubemap_lit_normal_map_shader;
						else
							rtp.shader = &textured_cubemap_lit_normal_map_shadow_shader;
					}
				} else {
					if (graph_dev_planets_receive_csm_shadows)
						rtp.shader = &textured_cubemap_lit_shadow_shader;
					else
						rtp.shader = &textured_cubemap_lit_shader;
				}
			}
			}
			break;
		case MATERIAL_TEXTURED_PLANET_RING: {
			rtp.shader = &textured_with_sphere_shadow_shader;

			struct material_textured_planet_ring *mt =
						&e->material_ptr->textured_planet_ring;
			rtp.texture_number = mt->texture_id;
			rtp.alpha = mt->alpha;
			texture_tint = mt->tint;
			rtp.do_blend = 1;
			rtp.do_cullface = 0;
			rtp.ring_texture_v = mt->texture_v;
			rtp.ring_inner_radius = mt->inner_radius;
			rtp.ring_outer_radius = mt->outer_radius;
			rtp.textures_not_ready = !graph_dev_texture_ready(rtp.texture_number);

			/* planet is at the center of our mesh */
			union vec4 sphere_pos = { { 0, 0, 0, 1 } };
			mat44_x_vec4_into_vec3(rtp.mat_mv, &sphere_pos, &shadow_sphere.eye_pos);

			/* planet is the size of the ring scale, world space distance
			   is the same in eye space as the view matrix does not scale */
			shadow_sphere.r = vec3_cwise_max(&e->scale);
			}
			break;
		case MATERIAL_WIREFRAME_SPHERE_CLIP: {
			wireframe_trans_shader = &trans_wireframe_with_clip_sphere_shader;

			struct material_wireframe_sphere_clip *mt =
				&e->material_ptr->wireframe_sphere_clip;

			union vec4 clip_sphere_pos = VEC4_INITIALIZER;
			if (mt->center)
				vec4_init_vec3(&clip_sphere_pos, &mt->center->e_pos, 1);
			mat44_x_vec4_into_vec3_dff(transform->v, &clip_sphere_pos, &clip_sphere.eye_pos);

			clip_sphere.r = e->material_ptr->wireframe_sphere_clip.radius;
			clip_sphere.radius_fade = e->material_ptr->wireframe_sphere_clip.radius_fade;
			}
			break;
		}
	}

	if (filled_triangle) {
		struct sng_color triangle_color;
		if (cx->camera.renderer & BLACK_TRIS)
			triangle_color = sng_get_color(BLACK);
		else
			triangle_color = sng_get_color(240 + GRAY + (NSHADESOFGRAY * e->shadecolor) + 10);

		/* outline and filled */
		if (outline_triangle) {
			graph_dev_raster_filled_wireframe_mesh(rtp.mat_mvp, e->m, line_color, &triangle_color);
		} else {
			if (rtp.shader) {

				rtp.m = e->m;
				rtp.triangle_color = &texture_tint;
				rtp.eye_light_pos = eye_light_pos;
				rtp.shadow_sphere = &shadow_sphere;
				rtp.shadow_annulus = &shadow_annulus;
				/* The shaders' direct term wants this clipped; the ambient ramp
				 * wants the raw figure, since a deliberately overstated shadow
				 * puts its surplus there.  See graph_dev_set_shade_ambient_ramp(). */
				rtp.in_shade = clampf(e->in_shade, 0.0, 1.0);
				rtp.water_color = &water_color;
				shade_scale = shade_ambient_scale(e->in_shade);
				rtp.ambient = cx->ambient * shade_scale;
				graph_dev_compute_star_light(cx, rtp.light_color, rtp.ambient_color);
				/* The textured shaders floor on the star-tinted ambient COLOUR rather
				 * than on the scalar above, so that has to come down with it. */
				rtp.ambient_color[0] *= shade_scale;
				rtp.ambient_color[1] *= shade_scale;
				rtp.ambient_color[2] *= shade_scale;

				graph_dev_raster_texture(&rtp);
			} else {
				if (is_sun) {
					graph_dev_raster_sun(rtp.mat_mvp, e->m, e->material_ptr);
				} else if (is_black_hole) {
					graph_dev_raster_black_hole(rtp.mat_mvp, e->m, e->material_ptr);
				} else if (atmosphere && !rtp.textures_not_ready) {
					float light_color[3], ambient_color[3];

					graph_dev_compute_star_light(cx, light_color, ambient_color);
					graph_dev_raster_atmosphere(rtp.mat_mvp, rtp.mat_mv, rtp.mat_normal,
						e->m, &atmosphere_color, eye_light_pos, rtp.alpha,
						&shadow_annulus, rtp.ring_texture_v, rtp.atmosphere_brightness,
						light_color, ambient_color);
				} else if (!rtp.textures_not_ready) {
					float light_color[3], ambient_color[3];

					graph_dev_compute_star_light(cx, light_color, ambient_color);
					shade_scale = shade_ambient_scale(e->in_shade);
					ambient_color[0] *= shade_scale;
					ambient_color[1] *= shade_scale;
					ambient_color[2] *= shade_scale;
					graph_dev_raster_single_color_lit(rtp.mat_mvp, rtp.mat_mv,
						rtp.mat_normal, &transform->m, e->m, &triangle_color, eye_light_pos,
						clampf(e->in_shade, 0.0, 1.0), cx->ambient * shade_scale,
						light_color, ambient_color);
				}
			}
		}
	} else if (outline_triangle) {
		graph_dev_raster_trans_wireframe_mesh(wireframe_trans_shader, rtp.mat_mvp, rtp.mat_mv,
			rtp.mat_normal, e->m, line_color, &clip_sphere, 1);
	}

	if (draw_billboard_wireframe && e->material_ptr &&
			e->material_ptr->billboard_type != MATERIAL_BILLBOARD_TYPE_NONE) {
		struct sng_color white_color = sng_get_color(WHITE);
		graph_dev_raster_trans_wireframe_mesh(0, rtp.mat_mvp, rtp.mat_mv,
			rtp.mat_normal, e->m, &white_color, 0, 0);
	}

	PROFILE_ZONE_END();
}

void graph_dev_draw_entity(struct entity_context *cx, struct entity *e, union vec3 *eye_light_pos,
	const struct entity_transform *transform)
{
	PROFILE_ZONE_START("graph_dev_draw_entity");

	draw_vertex_buffer_2d();

	struct sng_color line_color = sng_get_color(e->color);

	if (e->material_ptr && e->material_ptr->type == MATERIAL_NEBULA) {
		graph_dev_draw_nebula(&transform->mvp, &transform->mv, e);
		PROFILE_ZONE_END();
		return;
	}

	switch (e->m->geometry_mode) {
	case MESH_GEOMETRY_TRIANGLES:
		graph_dev_raster_triangle_mesh(cx, e, eye_light_pos, transform, &line_color);
		break;
	case MESH_GEOMETRY_LINES:
		graph_dev_raster_line_mesh(e, &transform->mvp, e->m, &line_color);
		break;
	case MESH_GEOMETRY_POINTS: {
			int do_blend = 0;
			float alpha = 1.0;
			float point_size = 1.0;
			struct graph_dev_gl_point_cloud_shader *shader;

			shader = &point_cloud_shader;

			graph_dev_raster_point_cloud_mesh(shader, &transform->mvp, e->m, &line_color, alpha, point_size,
				do_blend, NULL, NULL, 0);
		}
		break;
	case MESH_GEOMETRY_PARTICLE_ANIMATION:
		if (e->material_ptr && e->material_ptr->type == MATERIAL_TEXTURED_PARTICLE) {
			struct material_textured_particle *mt = &e->material_ptr->textured_particle;

			graph_dev_raster_particle_animation(e, transform, mt->texture_id,
				mt->radius * vec3_cwise_min(&e->scale), mt->time_base);
		}
		break;
	}
	PROFILE_ZONE_END();
}

/* This implementation is ok for drawing a few times, but the performance
   will really suck as lines per frame goes up */
void graph_dev_draw_3d_line(__attribute__((unused)) struct entity_context *cx, const struct mat44 *mat_vp,
	float x1, float y1, float z1, float x2, float y2, float z2)
{
	PROFILE_ZONE_START("graph_dev_draw_3d_line");

	draw_vertex_buffer_2d();

	enable_3d_viewport();

	/* setup fake line entity to render this */
	struct vertex_buffer_data g_v_buffer_data[2];
	g_v_buffer_data[0].position.v.x = x1;
	g_v_buffer_data[0].position.v.y = y1;
	g_v_buffer_data[0].position.v.z = z1;
	g_v_buffer_data[1].position.v.x = x2;
	g_v_buffer_data[1].position.v.y = y2;
	g_v_buffer_data[1].position.v.z = z2;

	struct vertex_line_buffer_data g_vl_buffer_data[2];

	int is_dotted = 0;

	g_vl_buffer_data[0].multi_one[0] =
		g_vl_buffer_data[1].multi_one[0] = is_dotted ? 255 : 0;

	g_vl_buffer_data[0].line_vertex0.v.x =
		g_vl_buffer_data[1].line_vertex0.v.x = x1;
	g_vl_buffer_data[0].line_vertex0.v.y =
		g_vl_buffer_data[1].line_vertex0.v.y = y1;
	g_vl_buffer_data[0].line_vertex0.v.z =
		g_vl_buffer_data[1].line_vertex0.v.z = z1;

	g_vl_buffer_data[0].line_vertex1.v.x =
		g_vl_buffer_data[1].line_vertex1.v.x = x2;
	g_vl_buffer_data[0].line_vertex1.v.y =
		g_vl_buffer_data[1].line_vertex1.v.y = y2;
	g_vl_buffer_data[0].line_vertex1.v.z =
		g_vl_buffer_data[1].line_vertex1.v.z = z2;

	sgc.gl_info_3d_line.nlines = 1;

	glBindBuffer(GL_ARRAY_BUFFER, sgc.gl_info_3d_line.vertex_buffer);
	glBufferData(GL_ARRAY_BUFFER, sizeof(g_v_buffer_data), g_v_buffer_data, GL_STREAM_DRAW);

	glBindBuffer(GL_ARRAY_BUFFER, sgc.gl_info_3d_line.line_vertex_buffer);
	glBufferData(GL_ARRAY_BUFFER, sizeof(g_vl_buffer_data), g_vl_buffer_data, GL_STREAM_DRAW);

	struct mesh m;
	m.graph_ptr = &sgc.gl_info_3d_line;

	struct entity e;
	e.material_ptr = 0;
	e.m = &m;

	struct sng_color line_color = sng_get_foreground();
	graph_dev_raster_line_mesh(&e, mat_vp, &m, &line_color);

	PROFILE_ZONE_END();
}

static void graph_dev_raster_full_screen_effect(struct graph_dev_gl_fs_effect_shader *shader, GLuint texture0_id,
	GLuint texture1_id, GLuint texture2_id, const struct sng_color *tint_color, float alpha)
{
	static const struct mat44 mat_identity = { { { 1, 0, 0, 0}, { 0, 1, 0, 0 }, { 0, 0, 1, 0}, { 0, 0, 0, 1} } };

	PROFILE_ZONE_START("graph_dev_raster_full_screen_effect");

	activate_shader(shader);

	if (texture0_id > 0 && shader->texture0_id >= 0) {
		BIND_TEXTURE(GL_TEXTURE0, GL_TEXTURE_2D, texture0_id);
	}

	if (texture1_id > 0 && shader->texture1_id >= 0) {
		BIND_TEXTURE(GL_TEXTURE1, GL_TEXTURE_2D, texture1_id);
	}

	if (texture2_id > 0 && shader->texture2_id >= 0) {
		BIND_TEXTURE(GL_TEXTURE2, GL_TEXTURE_2D, texture2_id);
	}

	glUniformMatrix4fv(shader->mvp_matrix_id, 1, GL_FALSE, &mat_identity.m[0][0]);
	glUniform4f(shader->viewport_id, 1.0 / sgc.screen_x, 1.0 / sgc.screen_y,
		sgc.screen_x, sgc.screen_y);

	if (shader->tint_color_id > 0) {
		if (tint_color)
			glUniform4f(shader->tint_color_id, tint_color->red,
				tint_color->green, tint_color->blue, alpha);
		else
			glUniform4f(shader->tint_color_id, 1, 1, 1, 1);
	}

	glEnableVertexAttribArray(shader->vertex_position_id);
	glBindBuffer(GL_ARRAY_BUFFER, textured_unit_quad.vertex_buffer);
	glVertexAttribPointer(
		shader->vertex_position_id, /* The attribute we want to configure */
		3,                           /* size */
		GL_FLOAT,                    /* type */
		GL_FALSE,                    /* normalized? */
		sizeof(struct vertex_buffer_data), /* stride */
		(void *)offsetof(struct vertex_buffer_data, position.v.x) /* array buffer offset */
	);

	glEnableVertexAttribArray(shader->texture_coord_id);
	glBindBuffer(GL_ARRAY_BUFFER, textured_unit_quad.triangle_vertex_buffer);
	glVertexAttribPointer(
		shader->texture_coord_id,/* The attribute we want to configure */
		2,                            /* size */
		GL_FLOAT,                     /* type */
		GL_TRUE,                     /* normalized? */
		sizeof(struct vertex_triangle_buffer_data), /* stride */
		(void *)offsetof(struct vertex_triangle_buffer_data, texture_coord.v.x) /* array buffer offset */
	);

	glDrawArrays(GL_TRIANGLES, 0, textured_unit_quad.nvertices);

	PROFILE_ZONE_END();
}

/* If any textures loads (PNG decoding) have completed, send them to the GPU */
static void graph_dev_send_completed_textures_to_gpu(void)
{
	PROFILE_ZONE_START("graph_dev_send_completed_textures_to_gpu");
	do {
		struct graph_dev_image_load_request *r = work_queue_dequeue(loaded_images_wq);
		if (!r) {
			PROFILE_ZONE_END();
			return;
		}

		switch (r->request_type) {
		case GRAPH_DEV_IMAGE_LOAD:
		case GRAPH_DEV_CUBEMAP_LOAD:
			(void) graph_dev_texture_to_gpu(r);
			break;
		default:
			fprintf(stderr, "Unknown graph dev image load request type %d\n",
				r->request_type);
			break;
		}
	} while (1);
	/* unreachable */
}

void graph_dev_start_frame(void)
{
	PROFILE_ZONE_START("graph_dev_start_frame");

	graph_dev_send_completed_textures_to_gpu();

	/* A fresh frame has no shadow map yet; render_entities may render one. */
	shadow_map_ready = 0;

	/* reset viewport to whole screen */
	sgc.active_vp = 0;
	VIEWPORT(0, 0, sgc.screen_x, sgc.screen_y);

	glClearColor(0.0f, 0.0f, 0.0f, 0.0f);

	if (draw_render_to_texture && render_target_2d.fbo > 0) {
		resize_fbo_if_needed(&render_target_2d);
		sgc.fbo_2d = render_target_2d.fbo;
		glBindFramebuffer(GL_FRAMEBUFFER, render_target_2d.fbo);
		glClear(GL_COLOR_BUFFER_BIT);
	} else
		sgc.fbo_2d = 0;

	if (draw_msaa_samples > 0 && msaa.fbo > 0) {

		glEnable(GL_MULTISAMPLE);

		glBindFramebuffer(GL_FRAMEBUFFER, msaa.fbo);
		sgc.fbo_3d = msaa.fbo;

		if (msaa.width != sgc.screen_x || msaa.height != sgc.screen_y || msaa.samples != draw_msaa_samples) {
			/* need to rebuild the fbo attachments */
			glBindRenderbuffer(GL_RENDERBUFFER, msaa.color0_buffer);
			glRenderbufferStorageMultisample(GL_RENDERBUFFER, draw_msaa_samples, GL_RGBA8,
				sgc.screen_x, sgc.screen_y);
			glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER,
				msaa.color0_buffer);

			glBindRenderbuffer(GL_RENDERBUFFER, msaa.depth_buffer);
			glRenderbufferStorageMultisample(GL_RENDERBUFFER, draw_msaa_samples, GL_DEPTH_COMPONENT,
				sgc.screen_x, sgc.screen_y);
			glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER,
				msaa.depth_buffer);

			msaa.width = sgc.screen_x;
			msaa.height = sgc.screen_y;
			msaa.samples = draw_msaa_samples;

			GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
			if (status != GL_FRAMEBUFFER_COMPLETE) {
				print_framebuffer_error();
			}
		}

	} else if (draw_render_to_texture && post_target0.fbo > 0) {

		resize_fbo_if_needed(&post_target0);

		glBindFramebuffer(GL_FRAMEBUFFER, post_target0.fbo);
		sgc.fbo_3d = post_target0.fbo;
	} else {
		/* render direct to back buffer */
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		sgc.fbo_3d = 0;
	}

	/* clear the bound 3d buffer */
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	sgc.fbo_current = sgc.fbo_3d;

	PROFILE_ZONE_END();
}

void graph_dev_end_frame(void)
{
	PROFILE_ZONE_START("graph_dev_end_frame");

	/* printf("end frame\n"); */
	draw_vertex_buffer_2d();

	/* reset viewport to whole screen for final effects */
	VIEWPORT(0, 0, sgc.screen_x, sgc.screen_y);

	if (msaa.fbo != 0 && sgc.fbo_3d == msaa.fbo) {
		glBindFramebuffer(GL_READ_FRAMEBUFFER, msaa.fbo);
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
		glBlitFramebuffer(0, 0, msaa.width, msaa.height, 0, 0,
			sgc.screen_x, sgc.screen_y, GL_COLOR_BUFFER_BIT, GL_NEAREST);

		glDisable(GL_MULTISAMPLE);

	} else if (post_target0.fbo != 0 && sgc.fbo_3d == post_target0.fbo) {
		GLuint result_texture;

		if (draw_smaa) {
			/* do the multi stage smaa process:
				render to texture -> edge_fbo -> blend_fbo -> screen back buffer */
			resize_fbo_if_needed(&smaa_effect.edge_target);
			resize_fbo_if_needed(&smaa_effect.blend_target);
			resize_fbo_if_needed(&post_target1);

			/* edge detect pass - render into edge_fbo */
			glBindFramebuffer(GL_FRAMEBUFFER, smaa_effect.edge_target.fbo);
			glClear(GL_COLOR_BUFFER_BIT);
			graph_dev_raster_full_screen_effect(&smaa_effect.edge_shader, post_target0.color0_texture,
				0, 0, 0, 1);

			/* blend pass - render into blend_fbo */
			glBindFramebuffer(GL_FRAMEBUFFER, smaa_effect.blend_target.fbo);
			glClear(GL_COLOR_BUFFER_BIT);
			graph_dev_raster_full_screen_effect(&smaa_effect.blend_shader,
				smaa_effect.edge_target.color0_texture,
				smaa_effect.area_tex, smaa_effect.search_tex, 0, 1);

			/* eighborhood pass - render to back buffer */
			glBindFramebuffer(GL_FRAMEBUFFER, post_target1.fbo);
			glClear(GL_COLOR_BUFFER_BIT);
			graph_dev_raster_full_screen_effect(&smaa_effect.neighborhood_shader,
				post_target0.color0_texture, smaa_effect.blend_target.color0_texture, 0, 0, 1);

			if (draw_smaa_edge)
				result_texture = smaa_effect.edge_target.color0_texture;
			else if (draw_smaa_blend)
				result_texture = smaa_effect.blend_target.color0_texture;
			else
				result_texture = post_target1.color0_texture;
		} else {
			result_texture = post_target0.color0_texture;
		}

		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		graph_dev_raster_full_screen_effect(&fs_copy_shader, result_texture, 0, 0, 0, 1);
	}

	if (render_target_2d.fbo != 0 && sgc.fbo_2d == render_target_2d.fbo) {
		/* alpha blend copy 2d fbo onto back buffer */
		glBindFramebuffer(GL_FRAMEBUFFER, 0);

		glEnable(GL_BLEND);
		BLEND_FUNC(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		graph_dev_raster_full_screen_effect(&fs_copy_shader, render_target_2d.color0_texture, 0, 0, 0, 1);
		glDisable(GL_BLEND);
	}

	PROFILE_ZONE_END();
}

void graph_dev_clear_depth_bit(void)
{
	PROFILE_ZONE_START("graph_dev_clear_depth_bit");
	glClear(GL_DEPTH_BUFFER_BIT);
	PROFILE_ZONE_END();
}

void graph_dev_set_shadow_cascades(const struct mat44d *world_to_lightclip, int n)
{
	int i;

	if (n > MAX_SHADOW_CASCADES)
		n = MAX_SHADOW_CASCADES;
	ensure_shadow_map_layers(n); /* size the texture array to the cascade count in use */
	shadow_map_num_cascades = n;
	for (i = 0; i < n; i++)
		shadow_cascade_w2l[i] = world_to_lightclip[i];
}

void graph_dev_shadow_map_begin(void)
{
	if (!graph_dev_shadow_map_enabled)
		return;

	PROFILE_ZONE_START("graph_dev_shadow_map_begin");

	/* Remember the framebuffer and viewport the caller was using so we can
	 * restore them when the shadow pass is done. */
	glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &saved_shadow_fbo);
	glGetIntegerv(GL_VIEWPORT, saved_shadow_viewport);

	glBindFramebuffer(GL_FRAMEBUFFER, shadow_map_fbo);
	glViewport(0, 0, SHADOW_MAP_TEXTURE_SIZE, SHADOW_MAP_TEXTURE_SIZE);

	glEnable(GL_DEPTH_TEST);
	glDepthMask(GL_TRUE);
	glEnable(GL_CULL_FACE);
	/* Slope-scaled depth bias to keep surfaces from shadowing themselves (acne). */
	glEnable(GL_POLYGON_OFFSET_FILL);
	glPolygonOffset(shadow_polygon_offset_factor, shadow_polygon_offset_units);

	activate_shader(&shadow_depth_shader);

	PROFILE_ZONE_END();
}

void graph_dev_shadow_map_set_cascade(int cascade)
{
	if (!graph_dev_shadow_map_enabled)
		return;
	if (cascade < 0 || cascade >= shadow_map_num_cascades)
		return;

	/* Render subsequent casters into this cascade's texture-array layer. */
	glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, shadow_map_texture, 0, cascade);
	glClear(GL_DEPTH_BUFFER_BIT);
	shadow_current_w2l = shadow_cascade_w2l[cascade];
}

void graph_dev_draw_shadow_caster(const struct mat44d *model, struct mesh *m)
{
	if (!graph_dev_shadow_map_enabled)
		return;
	if (!m->graph_ptr)
		return;

	struct mesh_gl_info *ptr = m->graph_ptr;
	struct mat44d shadow_mvp_d;
	struct mat44 shadow_mvp;

	mat44_product_ddd(&shadow_current_w2l, model, &shadow_mvp_d);
	mat44_convert_df(&shadow_mvp_d, &shadow_mvp);

	glUniformMatrix4fv(shadow_depth_shader.shadow_mvp_id, 1, GL_FALSE, &shadow_mvp.m[0][0]);

	glEnableVertexAttribArray(shadow_depth_shader.vertex_position_id);
	glBindBuffer(GL_ARRAY_BUFFER, ptr->vertex_buffer);
	glVertexAttribPointer(
		shadow_depth_shader.vertex_position_id,
		3, GL_FLOAT, GL_FALSE,
		sizeof(struct vertex_buffer_data),
		(void *)offsetof(struct vertex_buffer_data, position.v.x));

	glDrawArrays(GL_TRIANGLES, 0, m->ntriangles * 3);
}

void graph_dev_shadow_map_end(void)
{
	if (!graph_dev_shadow_map_enabled)
		return;

	glDisable(GL_POLYGON_OFFSET_FILL);
	glPolygonOffset(0.0f, 0.0f);

	/* Restore the caller's framebuffer and viewport. */
	glBindFramebuffer(GL_FRAMEBUFFER, saved_shadow_fbo);
	glViewport(saved_shadow_viewport[0], saved_shadow_viewport[1],
		saved_shadow_viewport[2], saved_shadow_viewport[3]);

	shadow_map_ready = 1;
}

void graph_dev_set_shadow_debug(int mode)
{
	graph_dev_shadow_map_debug = mode;
}

void graph_dev_set_shadow_normal_offset(float texels)
{
	if (texels < 0.0f)
		texels = 0.0f;
	graph_dev_shadow_normal_offset = texels;
}

float graph_dev_get_shadow_normal_offset(void)
{
	return graph_dev_shadow_normal_offset;
}

void graph_dev_set_shadow_pcf_radius(int radius)
{
	if (radius < 0)
		radius = 0;
	if (radius > CSM_PCF_MAX_RADIUS)
		radius = CSM_PCF_MAX_RADIUS;
	graph_dev_shadow_pcf_radius = radius;
}

int graph_dev_get_shadow_pcf_radius(void)
{
	return graph_dev_shadow_pcf_radius;
}

void graph_dev_set_shadow_cascade_splits(const float *split_far, int n)
{
	int i;

	if (n > MAX_SHADOW_CASCADES)
		n = MAX_SHADOW_CASCADES;
	for (i = 0; i < n; i++)
		shadow_cascade_split_far[i] = split_far[i];
}


void graph_dev_set_shadow_blend(float fraction)
{
	if (fraction < 0.0f)
		fraction = 0.0f;
	if (fraction > 0.5f)
		fraction = 0.5f;
	graph_dev_shadow_blend = fraction;
}

float graph_dev_get_shadow_blend(void)
{
	return graph_dev_shadow_blend;
}

void graph_dev_set_shadow_bias(float factor, float units)
{
	shadow_polygon_offset_factor = factor;
	shadow_polygon_offset_units = units;
}

void graph_dev_get_shadow_bias(float *factor, float *units)
{
	if (factor)
		*factor = shadow_polygon_offset_factor;
	if (units)
		*units = shadow_polygon_offset_units;
}

/* Upload the per-object cascade shadow matrices (world_to_lightclip[k] * model) and bind
 * the shadow map array for a lit shader that receives shadows. */
/* Length of the vector a unit model-space x axis maps to, which for the uniform scales this
 * renderer uses is the model's scale factor.  Used to express a world-space distance in the
 * model's own units. */
static double model_matrix_scale(const struct mat44d *model)
{
	double x = model->m[0][0], y = model->m[1][0], z = model->m[2][0];
	double s = sqrt(x * x + y * y + z * z);

	return s > 1e-9 ? s : 1.0;
}

/* World-space size of one shadow-map texel in a cascade, recovered from its world-to-light
 * clip matrix: the x row scales the ortho window's width down to clip space, so its length is
 * 2 / width, and the texel is that width divided by the map's resolution. */
static double cascade_texel_size(const struct mat44d *w2l)
{
	double x = w2l->m[0][0], y = w2l->m[1][0], z = w2l->m[2][0];
	double s = sqrt(x * x + y * y + z * z);

	if (s < 1e-12)
		return 0.0;
	return 2.0 / (s * (double) SHADOW_MAP_TEXTURE_SIZE);
}

static void upload_shadow_receive_uniforms(GLint shadow_mvp_id, GLint num_cascades_id,
	GLint shadow_map_id, GLint shadow_normal_offset_id, const struct mat44d *model)
{
	struct mat44 mvp[MAX_SHADOW_CASCADES];
	float normal_offset[MAX_SHADOW_CASCADES];
	double scale = model_matrix_scale(model);
	int i;

	for (i = 0; i < shadow_map_num_cascades; i++) {
		struct mat44d mvp_d;
		mat44_product_ddd(&shadow_cascade_w2l[i], model, &mvp_d);
		mat44_convert_df(&mvp_d, &mvp[i]);
		/* The shader lifts along a model-space normal, so convert the lift -- which is a
		 * number of world-space texels -- into this model's units. */
		normal_offset[i] = (float) (graph_dev_shadow_normal_offset *
			cascade_texel_size(&shadow_cascade_w2l[i]) / scale);
	}
	glUniformMatrix4fv(shadow_mvp_id, shadow_map_num_cascades, GL_FALSE, &mvp[0].m[0][0]);
	if (shadow_normal_offset_id >= 0)
		glUniform1fv(shadow_normal_offset_id, shadow_map_num_cascades, normal_offset);
	if (num_cascades_id >= 0)
		glUniform1i(num_cascades_id, shadow_map_num_cascades);
	BIND_TEXTURE(SHADOW_MAP_TEXTURE_UNIT, GL_TEXTURE_2D_ARRAY, shadow_map_texture);
	glUniform1i(shadow_map_id, SHADOW_MAP_TEXTURE_UNIT - GL_TEXTURE0);
}

void graph_dev_draw_line(float x1, float y1, float x2, float y2)
{
	PROFILE_ZONE_START("graph_dev_draw_line");
	make_room_in_vertex_buffer_2d(2);

	add_vertex_2d(x1, y1, sgc.hue, 255, GL_LINES);
	add_vertex_2d(x2, y2, sgc.hue, 255, GL_LINES);
	PROFILE_ZONE_END();
}

void graph_dev_draw_rectangle(int filled, float x, float y, float width, float height)
{
	PROFILE_ZONE_START("graph_dev_draw_rectangle");

	int x2, y2;
	GLubyte alpha = 255;

	x2 = x + width;
	y2 = y + height;

	glDisable(GL_DEPTH_TEST);
	if (sgc.alpha_blend) {
		/* must empty the vertex buffer to draw this primitive with blending */
		draw_vertex_buffer_2d();

		glEnable(GL_BLEND);
		BLEND_FUNC(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		alpha = 255 * sgc.alpha;
	}

	if (filled ) {
		/* filled rectangle with two triangles
		  0 ------- 1
		    |\  1 |
		    | \   |
		    |  \  |
		    |   \ |
		    | 2  \|
		  2 ------- 3
		*/

		make_room_in_vertex_buffer_2d(6);

		/* triangle 1 = 0, 3, 1 */
		add_vertex_2d(x, y, sgc.hue, alpha, GL_TRIANGLES);
		add_vertex_2d(x2, y2, sgc.hue, alpha, GL_TRIANGLES);
		add_vertex_2d(x2, y, sgc.hue, alpha, GL_TRIANGLES);

		/* triangle 2 = 0, 2, 3 */
		add_vertex_2d(x, y, sgc.hue, alpha, GL_TRIANGLES);
		add_vertex_2d(x, y2, sgc.hue, alpha, GL_TRIANGLES);
		add_vertex_2d(x2, y2, sgc.hue, alpha, GL_TRIANGLES);
	} else {
		/* not filled */
		make_room_in_vertex_buffer_2d(5);

		add_vertex_2d(x, y, sgc.hue, alpha, GL_LINE_STRIP);
		add_vertex_2d(x2, y, sgc.hue, alpha, GL_LINE_STRIP);
		add_vertex_2d(x2, y2, sgc.hue, alpha, GL_LINE_STRIP);
		add_vertex_2d(x, y2, sgc.hue, alpha, GL_LINE_STRIP);
		add_vertex_2d(x, y, sgc.hue, alpha, -1 /* primitive end */);
	}

	if (sgc.alpha_blend) {
		/* must draw the vertex buffer to complete the blending */
		draw_vertex_buffer_2d();

		glDisable(GL_BLEND);
	}
	PROFILE_ZONE_END();
}

void graph_dev_draw_point(float x, float y)
{
	PROFILE_ZONE_START("graph_dev_draw_point");
	make_room_in_vertex_buffer_2d(1);

	add_vertex_2d(x, y, sgc.hue, 255, GL_POINTS);
	PROFILE_ZONE_END();
}

void graph_dev_draw_arc(int filled, float x, float y, float width, float height, float angle1, float angle2)
{
	PROFILE_ZONE_START("graph_dev_draw_arc");
	float max_angle_delta = 2.0 * M_PI / 180.0; /*some ratio to height and width? */
	float rx = width/2.0;
	float ry = height/2.0;
	float cx = x + rx;
	float cy = y + ry;

	int i;

	int segments = (int)((angle2 - angle1) / max_angle_delta) + 1;
	float delta = (angle2 - angle1) / segments;

	GLubyte alpha = 255;

	if (sgc.alpha_blend) {
		/* must empty the vertex buffer to draw this primitive with blending */
		draw_vertex_buffer_2d();

		glEnable(GL_BLEND);
		BLEND_FUNC(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		alpha = 255 * sgc.alpha;
	}

	if (filled)
		make_room_in_vertex_buffer_2d(segments * 3);
	else
		make_room_in_vertex_buffer_2d(segments + 1);

	float x1 = 0, y1 = 0;
	for (i = 0; i <= segments; i++) {
		float a = angle1 + delta * (float)i;
		float x2 = cx + cos(a) * rx;
		float y2 = cy + sin(a) * ry;

		if (!filled || i > 0) {
			if (filled) {
				add_vertex_2d(x2, y2, sgc.hue, alpha, GL_TRIANGLES);
				add_vertex_2d(x1, y1, sgc.hue, alpha, GL_TRIANGLES);
				add_vertex_2d(cx, cy, sgc.hue, alpha, GL_TRIANGLES);
			} else {
				add_vertex_2d(x2, y2, sgc.hue, alpha, (i != segments ? GL_LINE_STRIP : -1));
			}
		}
		x1 = x2;
		y1 = y2;
	}

	if (sgc.alpha_blend) {
		/* must draw the vertex buffer to complete the blending */
		draw_vertex_buffer_2d();

		glDisable(GL_BLEND);
	}

	PROFILE_ZONE_END();
}

static void setup_single_color_lit_shader(struct graph_dev_gl_single_color_lit_shader *shader, int with_shadow)
{

	maybe_unload_shader(&shader->meta, &shader->program_id);

	/* Create and compile our GLSL program from the shaders. You can use
	 * single-color-lit-per-pixel.* shaders here as a drop in replacement,
	 * for the -per-vertex shaders, but it does not look significantly
	 * different and it is a bit more expensive.
	 */
	shader->program_id = load_shaders(shader_directory,
				"single-color-lit-per-vertex.vert",
				"single-color-lit-per-vertex.frag",
				with_shadow ?
				UNIVERSAL_SHADER_HEADER FILMIC_TONEMAPPING
					SHADOW_CASCADES_HEADER "#define USE_CSM 1\n" :
				UNIVERSAL_SHADER_HEADER FILMIC_TONEMAPPING);

	/* create the VAO for this shader */
	glGenVertexArrays(1, &shader->vao_id);

	/* Get a handle for our "MVP" uniform */
	shader->mvp_matrix_id = glGetUniformLocation(shader->program_id, "u_MVPMatrix");
	shader->mv_matrix_id = glGetUniformLocation(shader->program_id, "u_MVMatrix");
	shader->normal_matrix_id = glGetUniformLocation(shader->program_id, "u_NormalMatrix");
	shader->light_pos_id = glGetUniformLocation(shader->program_id, "u_LightPos");

	/* Get a handle for our buffers */
	shader->vertex_position_id = glGetAttribLocation(shader->program_id, "a_Position");
	shader->vertex_normal_id = glGetAttribLocation(shader->program_id, "a_Normal");
	shader->color_id = glGetUniformLocation(shader->program_id, "u_Color");
	shader->in_shade_id = glGetUniformLocation(shader->program_id, "u_in_shade");
	shader->ambient_id = glGetUniformLocation(shader->program_id, "u_Ambient");
	shader->light_color_id = glGetUniformLocation(shader->program_id, "u_LightColor");
	shader->ambient_color_id = glGetUniformLocation(shader->program_id, "u_AmbientColor");
	shader->filmic_tonemapping_id = glGetUniformLocation(shader->program_id, "u_FilmicTonemapping");
	shader->tonemapping_gain_id = glGetUniformLocation(shader->program_id, "u_TonemappingGain");

	shader->shadow_mvp_id = -1;
	shader->shadow_map_id = -1;
	shader->num_cascades_id = -1;
	shader->shadow_map_enabled_id = -1;
	shader->shadow_debug_id = -1;
	shader->shadow_pcf_radius_id = -1;
	if (with_shadow) {
		shader->shadow_mvp_id = glGetUniformLocation(shader->program_id, "u_ShadowMVP");
		shader->shadow_map_id = glGetUniformLocation(shader->program_id, "u_ShadowMap");
		shader->num_cascades_id = glGetUniformLocation(shader->program_id, "u_NumCascades");
		shader->shadow_map_enabled_id = glGetUniformLocation(shader->program_id, "u_ShadowMapEnabled");
		shader->shadow_debug_id = glGetUniformLocation(shader->program_id, "u_ShadowDebug");
		shader->shadow_pcf_radius_id = glGetUniformLocation(shader->program_id, "u_ShadowPcfRadius");
		shader->cascade_split_far_id = glGetUniformLocation(shader->program_id, "u_CascadeSplitFar");
		shader->shadow_blend_id = glGetUniformLocation(shader->program_id, "u_ShadowBlend");
		shader->shadow_normal_offset_id = glGetUniformLocation(shader->program_id,
								"u_ShadowNormalOffset");
	}
}

static void setup_shadow_depth_shader(struct graph_dev_gl_shadow_depth_shader *shader)
{
	maybe_unload_shader(&shader->meta, &shader->program_id);

	shader->program_id = load_shaders(shader_directory,
				"csm_depth.vert", "csm_depth.frag",
				UNIVERSAL_SHADER_HEADER);

	glGenVertexArrays(1, &shader->vao_id);

	shader->shadow_mvp_id = glGetUniformLocation(shader->program_id, "u_ShadowMVP");
	shader->vertex_position_id = glGetAttribLocation(shader->program_id, "a_Position");
}

/* Create the depth-texture framebuffer used to render the shadow map. */
/* (Re)allocate the shadow depth texture array to hold n cascade layers.  The count only
 * changes when the cascade tunable does, so this is a no-op on almost every frame; sizing to
 * the actual count keeps memory proportional to cascades used rather than paying for the max
 * (at 4096^2 x 4 bytes a layer that is ~64MB each). */
static void ensure_shadow_map_layers(int n)
{
	if (n < 1)
		n = 1;
	if (n > MAX_SHADOW_CASCADES)
		n = MAX_SHADOW_CASCADES;
	if (n == shadow_map_layers)
		return;
	glBindTexture(GL_TEXTURE_2D_ARRAY, shadow_map_texture);
	glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_DEPTH_COMPONENT24,
		SHADOW_MAP_TEXTURE_SIZE, SHADOW_MAP_TEXTURE_SIZE, n, 0,
		GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
	shadow_map_layers = n;
}

static void setup_shadow_map_fbo(void)
{
	/* One depth texture array, one layer per cascade (grown to the cascade count later). */
	glGenTextures(1, &shadow_map_texture);
	glBindTexture(GL_TEXTURE_2D_ARRAY, shadow_map_texture);
	glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_DEPTH_COMPONENT24,
		SHADOW_MAP_TEXTURE_SIZE, SHADOW_MAP_TEXTURE_SIZE, MAX_SHADOW_CASCADES, 0,
		GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
	shadow_map_layers = MAX_SHADOW_CASCADES;
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	/* Configure hardware depth comparison so the sampler2DArrayShadow returns a
	 * 0..1 lit factor (with GL_LINEAR this gives cheap 2x2 PCF). */
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);

	glGenFramebuffers(1, &shadow_map_fbo);
	glBindFramebuffer(GL_FRAMEBUFFER, shadow_map_fbo);
	/* A specific layer is attached per-cascade at render time. */
	glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, shadow_map_texture, 0, 0);
	/* Depth-only: no color attachments. */
	glDrawBuffer(GL_NONE);
	glReadBuffer(GL_NONE);

	GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
	if (status != GL_FRAMEBUFFER_COMPLETE) {
		fprintf(stderr, "Shadow map FBO incomplete, disabling shadows\n");
		print_framebuffer_error();
		graph_dev_shadow_map_enabled = 0;
	}
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

static void setup_atmosphere_shader(struct graph_dev_gl_atmosphere_shader *shader, int with_ring_shadow)
{

	maybe_unload_shader(&shader->meta, &shader->program_id);
	/* Create and compile our GLSL program from the shaders */
	shader->program_id = load_shaders(shader_directory,
				"atmosphere.vert", "atmosphere.frag",
				with_ring_shadow ?
				UNIVERSAL_SHADER_HEADER FILMIC_TONEMAPPING "\n#define USE_ANNULUS_SHADOW 1\n" :
				UNIVERSAL_SHADER_HEADER FILMIC_TONEMAPPING);

	/* create the VAO for this shader */
	glGenVertexArrays(1, &shader->vao_id);

	/* Get a handle for our "MVP" uniform */
	shader->mvp_matrix_id = glGetUniformLocation(shader->program_id, "u_MVPMatrix");
	shader->mv_matrix_id = glGetUniformLocation(shader->program_id, "u_MVMatrix");
	shader->normal_matrix_id = glGetUniformLocation(shader->program_id, "u_NormalMatrix");
	shader->light_pos_id = glGetUniformLocation(shader->program_id, "u_LightPos");
	shader->atmosphere_brightness_id = glGetUniformLocation(shader->program_id, "u_atmosphere_brightness");
	shader->light_color_id = glGetUniformLocation(shader->program_id, "u_LightColor");
	shader->ambient_color_id = glGetUniformLocation(shader->program_id, "u_AmbientColor");

	/* Get a handle for our buffers */
	shader->vertex_position_id = glGetAttribLocation(shader->program_id, "a_Position");
	shader->vertex_normal_id = glGetAttribLocation(shader->program_id, "a_Normal");
	shader->color_id = glGetUniformLocation(shader->program_id, "u_Color");
	shader->alpha = glGetUniformLocation(shader->program_id, "u_Alpha");
	shader->filmic_tonemapping_id = glGetUniformLocation(shader->program_id, "u_FilmicTonemapping");
	shader->tonemapping_gain_id = glGetUniformLocation(shader->program_id, "u_TonemappingGain");

	if (with_ring_shadow) {
		shader->shadow_annulus_texture_id = glGetUniformLocation(shader->program_id, "u_AnnulusAlbedoTex");
		shader->shadow_annulus_center_id = glGetUniformLocation(shader->program_id, "u_AnnulusCenter");
		shader->shadow_annulus_normal_id = glGetUniformLocation(shader->program_id, "u_AnnulusNormal");
		shader->shadow_annulus_radius_id = glGetUniformLocation(shader->program_id, "u_AnnulusRadius");
		shader->shadow_annulus_tint_color_id = glGetUniformLocation(shader->program_id, "u_AnnulusTintColor");
		shader->ring_texture_v_id = glGetUniformLocation(shader->program_id, "u_ring_texture_v");
	}
}

static void setup_textured_shader(const char *basename, const char *defines,
	struct graph_dev_gl_textured_shader *shader)
{
	maybe_unload_shader(&shader->meta, &shader->program_id);
	memset(shader, 0xff, sizeof(*shader)); /* set all attributes to -1 */
	shader->meta.program_id = &shader->program_id; /* Work around what that memset just did. */

	char vert_header[1024];
	snprintf(vert_header, sizeof(vert_header), "%s\n#define INCLUDE_VS 1\n", defines);
	char frag_header[1024];
	snprintf(frag_header, sizeof(frag_header), "%s\n#define INCLUDE_FS 1\n", defines);

	char shader_filename[PATH_MAX];
	snprintf(shader_filename, sizeof(shader_filename), "%s.shader", basename);

	/* csm.shader is concatenated ahead of the shader proper so its cascade helpers are
	 * declared before use.  It is entirely inside #ifdef USE_CSM, so shaders built without
	 * that define get nothing from it. */
	const char *filenames[] = { CSM_SHADER_FILE, shader_filename };

	shader->program_id = load_concat_shaders(shader_directory,
				vert_header, 2, filenames, frag_header, 2, filenames);

	/* create the VAO for this shader */
	glGenVertexArrays(1, &shader->vao_id);

	activate_shader(shader);

	shader->mvp_matrix_id = glGetUniformLocation(shader->program_id, "u_MVPMatrix");
	shader->mv_matrix_id = glGetUniformLocation(shader->program_id, "u_MVMatrix");
	shader->normal_matrix_id = glGetUniformLocation(shader->program_id, "u_NormalMatrix");
	shader->tint_color_id = glGetUniformLocation(shader->program_id, "u_TintColor");
	shader->light_pos_id = glGetUniformLocation(shader->program_id, "u_LightPos");
	shader->texture_2d_id = glGetUniformLocation(shader->program_id, "u_AlbedoTex");
	if (shader->texture_2d_id >= 0)
		glUniform1i(shader->texture_2d_id, 0);
	shader->emit_texture_2d_id = glGetUniformLocation(shader->program_id, "u_EmitTex");
	if (shader->emit_texture_2d_id >= 0)
		glUniform1i(shader->emit_texture_2d_id, 1);
	shader->normalmap_cubemap_id = -1;
	shader->texture_cubemap_id = -1;
	shader->normalmap_id = glGetUniformLocation(shader->program_id, "u_NormalMapTex");
	if (shader->normalmap_id >= 0)
		glUniform1i(shader->normalmap_id, 3);
	shader->emit_intensity_id = glGetUniformLocation(shader->program_id, "u_EmitIntensity");
	if (shader->emit_intensity_id >= 0)
		glUniform1f(shader->emit_intensity_id, 1.0);
	shader->ring_texture_v_id = glGetUniformLocation(shader->program_id, "u_ring_texture_v");
	shader->ring_inner_radius_id = glGetUniformLocation(shader->program_id, "u_ring_inner_radius");
	if (shader->ring_inner_radius_id >= 0)
		glUniform1f(shader->ring_inner_radius_id, 2.0);
	shader->ring_outer_radius_id = glGetUniformLocation(shader->program_id, "u_ring_outer_radius");
	if (shader->ring_outer_radius_id >= 0)
		glUniform1f(shader->ring_outer_radius_id, 2.0);
	shader->specular_power_id = glGetUniformLocation(shader->program_id, "u_SpecularPower");
	if (shader->specular_power_id >= 0)
		glUniform1f(shader->specular_power_id, 512.0);
	shader->specular_intensity_id = glGetUniformLocation(shader->program_id, "u_SpecularIntensity");
	if (shader->specular_power_id >= 0)
		glUniform1f(shader->specular_intensity_id, 0.2);
	shader->invert = glGetUniformLocation(shader->program_id, "u_Invert");
	shader->in_shade = glGetUniformLocation(shader->program_id, "u_in_shade");
	shader->water_color = glGetUniformLocation(shader->program_id, "u_WaterColor");
	shader->u1v1 = glGetUniformLocation(shader->program_id, "u_u1v1");
	shader->texture_width = glGetUniformLocation(shader->program_id, "u_width");

	shader->vertex_position_id = glGetAttribLocation(shader->program_id, "a_Position");
	shader->vertex_normal_id = glGetAttribLocation(shader->program_id, "a_Normal");
	shader->vertex_tangent_id = glGetAttribLocation(shader->program_id, "a_Tangent");
	shader->vertex_bitangent_id = glGetAttribLocation(shader->program_id, "a_BiTangent");
	shader->texture_coord_id = glGetAttribLocation(shader->program_id, "a_TexCoord");

	shader->shadow_sphere_id = glGetUniformLocation(shader->program_id, "u_Sphere");
	shader->ambient_id = glGetUniformLocation(shader->program_id, "u_Ambient");
	shader->light_color_id = glGetUniformLocation(shader->program_id, "u_LightColor");
	shader->ambient_color_id = glGetUniformLocation(shader->program_id, "u_AmbientColor");
	shader->ambient_scale_id = glGetUniformLocation(shader->program_id, "u_AmbientScale");
	shader->filmic_tonemapping_id = glGetUniformLocation(shader->program_id, "u_FilmicTonemapping");
	shader->tonemapping_gain_id = glGetUniformLocation(shader->program_id, "u_TonemappingGain");

	/* Cascaded shadow mapping uniforms; -1 (ignored) for non-USE_CSM variants. */
	shader->shadow_mvp_id = glGetUniformLocation(shader->program_id, "u_ShadowMVP");
	shader->shadow_map_id = glGetUniformLocation(shader->program_id, "u_ShadowMap");
	shader->num_cascades_id = glGetUniformLocation(shader->program_id, "u_NumCascades");
	shader->shadow_map_enabled_id = glGetUniformLocation(shader->program_id, "u_ShadowMapEnabled");
	shader->shadow_debug_id = glGetUniformLocation(shader->program_id, "u_ShadowDebug");
	shader->shadow_pcf_radius_id = glGetUniformLocation(shader->program_id, "u_ShadowPcfRadius");
	shader->cascade_split_far_id = glGetUniformLocation(shader->program_id, "u_CascadeSplitFar");
	shader->shadow_blend_id = glGetUniformLocation(shader->program_id, "u_ShadowBlend");
}

static void setup_textured_cubemap_shader(const char *basename, int use_normal_map,
				int use_specular, int use_annulus_shadow, int use_csm,
				struct graph_dev_gl_textured_shader *shader)
{
	maybe_unload_shader(&shader->meta, &shader->program_id);
	memset(shader, 0xff, sizeof(*shader)); /* set all attributes to -1 */
	shader->meta.program_id = &shader->program_id; /* Work around what that memset just did. */

	char vert_header[1024];
	char frag_header[1024];

	snprintf(vert_header, sizeof(vert_header), "%s\n%s\n%s\n%s\n%s\n%s\n",
		UNIVERSAL_SHADER_HEADER, "#define INCLUDE_VS 1\n",
			use_normal_map ? "#define USE_NORMAL_MAP 1\n" : "\n",
			use_specular ? "#define USE_SPECULAR 1\n" : "\n",
			use_annulus_shadow ? "#define USE_ANNULUS_SHADOW 1\n" : "\n",
			use_csm ? SHADOW_CASCADES_HEADER "#define USE_CSM 1\n" : "\n");
	snprintf(frag_header, sizeof(frag_header), "%s\n%s\n%s\n%s\n%s\n%s\n",
		UNIVERSAL_SHADER_HEADER FILMIC_TONEMAPPING, "#define INCLUDE_FS 1\n",
			use_normal_map ? "#define USE_NORMAL_MAP 1\n" : "\n",
			use_specular ? "#define USE_SPECULAR 1\n" : "\n",
			use_annulus_shadow ? "#define USE_ANNULUS_SHADOW 1\n" : "\n",
			use_csm ? SHADOW_CASCADES_HEADER "#define USE_CSM 1\n" : "\n");

	char shader_filename[PATH_MAX];
	snprintf(shader_filename, sizeof(shader_filename), "%s.shader", basename);

	const char *filenames[] = { CSM_SHADER_FILE, shader_filename };

	shader->program_id = load_concat_shaders(shader_directory,
				vert_header, 2, filenames, frag_header, 2, filenames);
	/* create the VAO for this shader */
	glGenVertexArrays(1, &shader->vao_id);

	activate_shader(shader);

	/* Get a handle for our "MVP" uniform */
	shader->mvp_matrix_id = glGetUniformLocation(shader->program_id, "u_MVPMatrix");
	shader->mv_matrix_id = glGetUniformLocation(shader->program_id, "u_MVMatrix");
	shader->normal_matrix_id = glGetUniformLocation(shader->program_id, "u_NormalMatrix");
	shader->light_pos_id = glGetUniformLocation(shader->program_id, "u_LightPos");

	shader->texture_cubemap_id = glGetUniformLocation(shader->program_id, "u_AlbedoTex");
	glUniform1i(shader->texture_cubemap_id, 0);
	if (use_normal_map) {
		shader->normalmap_cubemap_id = glGetUniformLocation(shader->program_id, "u_NormalMapTex");
		glUniform1i(shader->normalmap_cubemap_id, 3); /* GL_TEXTURE3 */
	}
	shader->texture_2d_id = -1;
	shader->normalmap_id = -1;
	if (use_csm) {
		shader->shadow_mvp_id = glGetUniformLocation(shader->program_id, "u_ShadowMVP");
		shader->shadow_map_id = glGetUniformLocation(shader->program_id, "u_ShadowMap");
		shader->num_cascades_id = glGetUniformLocation(shader->program_id, "u_NumCascades");
		shader->shadow_map_enabled_id = glGetUniformLocation(shader->program_id, "u_ShadowMapEnabled");
		shader->shadow_debug_id = glGetUniformLocation(shader->program_id, "u_ShadowDebug");
		shader->shadow_pcf_radius_id = glGetUniformLocation(shader->program_id, "u_ShadowPcfRadius");
		shader->cascade_split_far_id = glGetUniformLocation(shader->program_id, "u_CascadeSplitFar");
		shader->shadow_blend_id = glGetUniformLocation(shader->program_id, "u_ShadowBlend");
		shader->shadow_normal_offset_id = glGetUniformLocation(shader->program_id,
								"u_ShadowNormalOffset");
	} else {
		shader->shadow_map_enabled_id = -1;
	}
	shader->tint_color_id = glGetUniformLocation(shader->program_id, "u_TintColor");
	shader->ring_texture_v_id = glGetUniformLocation(shader->program_id, "u_ring_texture_v");
	if (shader->ring_texture_v_id >= 0)
		glUniform1f(shader->ring_texture_v_id, 0.25);
	shader->ring_inner_radius_id = glGetUniformLocation(shader->program_id, "u_ring_inner_radius");
	if (shader->ring_inner_radius_id >= 0)
		glUniform1f(shader->ring_inner_radius_id, 2.0);
	shader->ring_outer_radius_id = glGetUniformLocation(shader->program_id, "u_ring_outer_radius");
	if (shader->ring_outer_radius_id >= 0)
		glUniform1f(shader->ring_outer_radius_id, 2.0);
	shader->in_shade = glGetUniformLocation(shader->program_id, "u_in_shade");
	if (shader->in_shade >= 0)
		glUniform1f(shader->in_shade, 0.0);
	shader->water_color = glGetUniformLocation(shader->program_id, "u_WaterColor");
	if (shader->water_color >= 0)
		glUniform3f(shader->water_color, 0.1, 0.3, 1.0); /* mostly blue */

	/* Get a handle for our buffers */
	shader->vertex_position_id = glGetAttribLocation(shader->program_id, "a_Position");
	shader->vertex_normal_id = glGetAttribLocation(shader->program_id, "a_Normal");
	shader->vertex_tangent_id = glGetAttribLocation(shader->program_id, "a_Tangent");
	shader->vertex_bitangent_id = glGetAttribLocation(shader->program_id, "a_BiTangent");

	shader->shadow_annulus_texture_id = glGetUniformLocation(shader->program_id, "u_AnnulusAlbedoTex");
	glUniform1i(shader->shadow_annulus_texture_id, 1);
	shader->shadow_annulus_center_id = glGetUniformLocation(shader->program_id, "u_AnnulusCenter");
	shader->shadow_annulus_normal_id = glGetUniformLocation(shader->program_id, "u_AnnulusNormal");
	shader->shadow_annulus_radius_id = glGetUniformLocation(shader->program_id, "u_AnnulusRadius");
	shader->shadow_annulus_tint_color_id = glGetUniformLocation(shader->program_id, "u_AnnulusTintColor");
	shader->ambient_id = glGetUniformLocation(shader->program_id, "u_Ambient");
	shader->light_color_id = glGetUniformLocation(shader->program_id, "u_LightColor");
	shader->ambient_color_id = glGetUniformLocation(shader->program_id, "u_AmbientColor");
	shader->ambient_scale_id = glGetUniformLocation(shader->program_id, "u_AmbientScale");
	shader->filmic_tonemapping_id = glGetUniformLocation(shader->program_id, "u_FilmicTonemapping");
	shader->tonemapping_gain_id = glGetUniformLocation(shader->program_id, "u_TonemappingGain");
}

static void setup_filled_wireframe_shader(struct graph_dev_gl_filled_wireframe_shader *shader)
{
	maybe_unload_shader(&shader->meta, &shader->program_id);
	/* Create and compile our GLSL program from the shaders */
	shader->program_id = load_shaders(shader_directory,
					"wireframe_filled.vert", "wireframe_filled.frag",
					UNIVERSAL_SHADER_HEADER);
	/* create the VAO for this shader */
	glGenVertexArrays(1, &shader->vao_id);

	shader->viewport_id = glGetUniformLocation(shader->program_id, "Viewport");
	shader->mvp_matrix_id = glGetUniformLocation(shader->program_id, "ModelViewProjectionMatrix");

	shader->position_id = glGetAttribLocation(shader->program_id, "position");
	shader->tvertex0_id = glGetAttribLocation(shader->program_id, "tvertex0");
	shader->tvertex1_id = glGetAttribLocation(shader->program_id, "tvertex1");
	shader->tvertex2_id = glGetAttribLocation(shader->program_id, "tvertex2");
	shader->edge_mask_id = glGetAttribLocation(shader->program_id, "edge_mask");

	shader->line_color_id = glGetUniformLocation(shader->program_id, "line_color");
	shader->triangle_color_id = glGetUniformLocation(shader->program_id, "triangle_color");
}

static void setup_trans_wireframe_shader(const char *basename, struct graph_dev_gl_trans_wireframe_shader *shader)
{
	char vert_filename[PATH_MAX];
	char frag_filename[PATH_MAX];
	snprintf(vert_filename, sizeof(vert_filename), "%s.vert", basename);
	snprintf(frag_filename, sizeof(frag_filename), "%s.frag", basename);

	maybe_unload_shader(&shader->meta, &shader->program_id);
	/* Create and compile our GLSL program from the shaders */
	shader->program_id = load_shaders(shader_directory, vert_filename, frag_filename,
						UNIVERSAL_SHADER_HEADER);
	/* create the VAO for this shader */
	glGenVertexArrays(1, &shader->vao_id);

	shader->mvp_matrix_id = glGetUniformLocation(shader->program_id, "u_MVPMatrix");
	shader->mv_matrix_id = glGetUniformLocation(shader->program_id, "u_MVMatrix");
	shader->normal_matrix_id = glGetUniformLocation(shader->program_id, "u_NormalMatrix");
	shader->vertex_position_id = glGetAttribLocation(shader->program_id, "a_Position");
	shader->vertex_normal_id = glGetAttribLocation(shader->program_id, "a_Normal");
	shader->color_id = glGetUniformLocation(shader->program_id, "u_Color");
	shader->clip_sphere_id = glGetUniformLocation(shader->program_id, "u_ClipSphere");
	shader->clip_sphere_radius_fade_id = glGetUniformLocation(shader->program_id, "u_ClipSphereRadiusFade");
}

static void setup_single_color_shader(struct graph_dev_gl_single_color_shader *shader)
{
	maybe_unload_shader(&shader->meta, &shader->program_id);
	/* Create and compile our GLSL program from the shaders */
	shader->program_id = load_shaders(shader_directory,
				"single_color.vert", "single_color.frag",
				UNIVERSAL_SHADER_HEADER);
	/* create the VAO for this shader */
	glGenVertexArrays(1, &shader->vao_id);

	/* Get a handle for our "MVP" uniform */
	shader->mvp_matrix_id = glGetUniformLocation(shader->program_id, "u_MVPMatrix");

	/* Get a handle for our buffers */
	shader->vertex_position_id = glGetAttribLocation(shader->program_id, "a_Position");
	shader->color_id = glGetUniformLocation(shader->program_id, "u_Color");
}

static void setup_vertex_color_shader(struct graph_dev_gl_vertex_color_shader *shader)
{
	maybe_unload_shader(&shader->meta, &shader->program_id);
	shader->program_id = load_shaders(shader_directory,
				"per_vertex_color.vert", "per_vertex_color.frag",
				UNIVERSAL_SHADER_HEADER);
	/* create the VAO for this shader */
	glGenVertexArrays(1, &shader->vao_id);

	shader->mvp_matrix_id = glGetUniformLocation(shader->program_id, "u_MVPMatrix");

	shader->vertex_position_id = glGetAttribLocation(shader->program_id, "a_Position");
	shader->vertex_color_id = glGetAttribLocation(shader->program_id, "a_Color");
}

static void setup_sun_shader(struct graph_dev_gl_sun_shader *shader)
{
	maybe_unload_shader(&shader->meta, &shader->program_id);
	shader->program_id = load_shaders(shader_directory,
				"sun.vert", "sun.frag", UNIVERSAL_SHADER_HEADER FILMIC_TONEMAPPING);
	glGenVertexArrays(1, &shader->vao_id);

	shader->mvp_matrix_id = glGetUniformLocation(shader->program_id, "u_MVPMatrix");
	shader->vertex_position_id = glGetAttribLocation(shader->program_id, "a_Position");
	shader->texture_coord_id = glGetAttribLocation(shader->program_id, "a_TexCoord");
	shader->color_id = glGetUniformLocation(shader->program_id, "u_Color");
	shader->brightness_id = glGetUniformLocation(shader->program_id, "u_Brightness");
	shader->disc_radius_id = glGetUniformLocation(shader->program_id, "u_DiscRadius");
	shader->edge_softness_id = glGetUniformLocation(shader->program_id, "u_EdgeSoftness");
	shader->psf_width_id = glGetUniformLocation(shader->program_id, "u_PsfWidth");
	shader->psf_falloff_id = glGetUniformLocation(shader->program_id, "u_PsfFalloff");
	shader->filmic_tonemapping_id = glGetUniformLocation(shader->program_id, "u_FilmicTonemapping");
	shader->tonemapping_gain_id = glGetUniformLocation(shader->program_id, "u_TonemappingGain");
}

static void setup_black_hole_shader(struct graph_dev_gl_black_hole_shader *shader)
{
	maybe_unload_shader(&shader->meta, &shader->program_id);
	shader->program_id = load_shaders(shader_directory,
				"black_hole.vert", "black_hole.frag", UNIVERSAL_SHADER_HEADER);
	glGenVertexArrays(1, &shader->vao_id);

	shader->mvp_matrix_id = glGetUniformLocation(shader->program_id, "u_MVPMatrix");
	shader->vertex_position_id = glGetAttribLocation(shader->program_id, "a_Position");
	shader->texture_coord_id = glGetAttribLocation(shader->program_id, "a_TexCoord");
	shader->disc_radius_id = glGetUniformLocation(shader->program_id, "u_DiscRadius");
	shader->edge_softness_id = glGetUniformLocation(shader->program_id, "u_EdgeSoftness");
	shader->ring_brightness_id = glGetUniformLocation(shader->program_id, "u_RingBrightness");
	shader->ring_width_id = glGetUniformLocation(shader->program_id, "u_RingWidth");
	shader->einstein_radius_id = glGetUniformLocation(shader->program_id, "u_EinsteinRadius");
	shader->glow_brightness_id = glGetUniformLocation(shader->program_id, "u_GlowBrightness");
	shader->glow_width_id = glGetUniformLocation(shader->program_id, "u_GlowWidth");
	shader->ring_color_id = glGetUniformLocation(shader->program_id, "u_RingColor");
}

static void setup_line_single_color_shader(struct graph_dev_gl_line_single_color_shader *shader)
{
	maybe_unload_shader(&shader->meta, &shader->program_id);
	/* Create and compile our GLSL program from the shaders */
	shader->program_id = load_shaders(shader_directory,
				"line-single-color.vert", "line-single-color.frag",
				UNIVERSAL_SHADER_HEADER);
	/* create the VAO for this shader */
	glGenVertexArrays(1, &shader->vao_id);

	shader->mvp_matrix_id = glGetUniformLocation(shader->program_id, "u_MVPMatrix");
	shader->viewport_id = glGetUniformLocation(shader->program_id, "u_Viewport");
	shader->dot_size_id = glGetUniformLocation(shader->program_id, "u_DotSize");
	shader->dot_pitch_id = glGetUniformLocation(shader->program_id, "u_DotPitch");
	shader->line_color_id = glGetUniformLocation(shader->program_id, "u_LineColor");

	shader->multi_one_id = glGetAttribLocation(shader->program_id, "a_MultiOne");
	shader->vertex_position_id = glGetAttribLocation(shader->program_id, "a_Position");
	shader->line_vertex0_id = glGetAttribLocation(shader->program_id, "a_LineVertex0");
	shader->line_vertex1_id = glGetAttribLocation(shader->program_id, "a_LineVertex1");
}

static void setup_point_cloud_shader(const char *basename, struct graph_dev_gl_point_cloud_shader *shader)
{
	char vert_filename[PATH_MAX];
	char frag_filename[PATH_MAX];
	snprintf(vert_filename, sizeof(vert_filename), "%s.vert", basename);
	snprintf(frag_filename, sizeof(frag_filename), "%s.frag", basename);

	maybe_unload_shader(&shader->meta, &shader->program_id);
	/* Create and compile our GLSL program from the shaders */
	shader->program_id = load_shaders(shader_directory, vert_filename, frag_filename,
				UNIVERSAL_SHADER_HEADER);
	/* create the VAO for this shader */
	glGenVertexArrays(1, &shader->vao_id);

	/* Get a handle for our "MVP" uniform */
	shader->mvp_matrix_id = glGetUniformLocation(shader->program_id, "u_MVPMatrix");

	/* Get a handle for our buffers */
	shader->vertex_position_id = glGetAttribLocation(shader->program_id, "a_Position");
	shader->point_size_id = glGetUniformLocation(shader->program_id, "u_PointSize");
	shader->color_id = glGetUniformLocation(shader->program_id, "u_Color");
	shader->time_id = glGetUniformLocation(shader->program_id, "u_Time");
	shader->camera_pos_id = glGetUniformLocation(shader->program_id, "u_CameraPos");
	shader->fade_params_id = glGetUniformLocation(shader->program_id, "u_FadeParams");
}

static void setup_color_by_w_shader(struct graph_dev_gl_color_by_w_shader *shader)
{
	maybe_unload_shader(&shader->meta, &shader->program_id);
	/* Create and compile our GLSL program from the shaders */
	shader->program_id = load_shaders(shader_directory, "color_by_w.vert", "color_by_w.frag",
					UNIVERSAL_SHADER_HEADER);
	/* create the VAO for this shader */
	glGenVertexArrays(1, &shader->vao_id);

	/* Get a handle for our "MVP" uniform */
	shader->mvp_id = glGetUniformLocation(shader->program_id, "u_MVPMatrix");

	/* Get a handle for our buffers */
	shader->position_id = glGetAttribLocation(shader->program_id, "a_Position");
	shader->near_color_id = glGetUniformLocation(shader->program_id, "u_NearColor");
	shader->near_w_id = glGetUniformLocation(shader->program_id, "u_NearW");
	shader->center_color_id = glGetUniformLocation(shader->program_id, "u_CenterColor");
	shader->center_w_id = glGetUniformLocation(shader->program_id, "u_CenterW");
	shader->far_color_id = glGetUniformLocation(shader->program_id, "u_FarColor");
	shader->far_w_id = glGetUniformLocation(shader->program_id, "u_FarW");
}


static void setup_skybox_shader(struct graph_dev_gl_skybox_shader *shader)
{
	maybe_unload_shader(&shader->meta, &shader->program_id);
	/* Create and compile our GLSL program from the shaders */
	shader->program_id = load_shaders(shader_directory, "skybox.vert", "skybox.frag",
						UNIVERSAL_SHADER_HEADER FILMIC_TONEMAPPING
						GRAVITATIONAL_LENS_HEADER);
	/* create the VAO for this shader */
	glGenVertexArrays(1, &shader->vao_id);

	activate_shader(shader);

	/* Get a handle for our "MVP" uniform */
	shader->mvp_id = glGetUniformLocation(shader->program_id, "MVP");
	shader->texture_id = glGetUniformLocation(shader->program_id, "s_texture");
	shader->filmic_tonemapping_id = glGetUniformLocation(shader->program_id, "u_FilmicTonemapping");
	shader->tonemapping_gain_id = glGetUniformLocation(shader->program_id, "u_TonemappingGain");
	shader->lens_dir_id = glGetUniformLocation(shader->program_id, "u_LensDir");
	shader->lens_params_id = glGetUniformLocation(shader->program_id, "u_LensParams");
	glUniform1i(shader->texture_id, 0);

	/* Get a handle for our buffers */
	shader->vertex_id = glGetAttribLocation(shader->program_id, "vertex");
}

static void setup_cubemap_cube(struct graph_dev_primitive *obj)
{
	/* cube vertices in triangle strip for vertex buffer object */
	static const struct vertex_buffer_data cube_v_data[] = {
		{ .position = { { -10.0f,  10.0f, -10.0f } } },
		{ .position = { { -10.0f, -10.0f, -10.0f } } },
		{ .position = { { 10.0f, -10.0f, -10.0f } } },
		{ .position = { { 10.0f, -10.0f, -10.0f } } },
		{ .position = { { 10.0f,  10.0f, -10.0f } } },
		{ .position = { { -10.0f,  10.0f, -10.0f } } },

		{ .position = { { -10.0f, -10.0f,  10.0f } } },
		{ .position = { { -10.0f, -10.0f, -10.0f } } },
		{ .position = { { -10.0f,  10.0f, -10.0f } } },
		{ .position = { { -10.0f,  10.0f, -10.0f } } },
		{ .position = { { -10.0f,  10.0f,  10.0f } } },
		{ .position = { { -10.0f, -10.0f,  10.0f } } },

		{ .position = { { 10.0f, -10.0f, -10.0f } } },
		{ .position = { { 10.0f, -10.0f,  10.0f } } },
		{ .position = { { 10.0f,  10.0f,  10.0f } } },
		{ .position = { { 10.0f,  10.0f,  10.0f } } },
		{ .position = { { 10.0f,  10.0f, -10.0f } } },
		{ .position = { { 10.0f, -10.0f, -10.0f } } },

		{ .position = { { -10.0f, -10.0f,  10.0f } } },
		{ .position = { { -10.0f,  10.0f,  10.0f } } },
		{ .position = { { 10.0f,  10.0f,  10.0f } } },
		{ .position = { { 10.0f,  10.0f,  10.0f } } },
		{ .position = { { 10.0f, -10.0f,  10.0f } } },
		{ .position = { { -10.0f, -10.0f,  10.0f } } },

		{ .position = { { -10.0f,  10.0f, -10.0f } } },
		{ .position = { { 10.0f,  10.0f, -10.0f } } },
		{ .position = { { 10.0f,  10.0f,  10.0f } } },
		{ .position = { { 10.0f,  10.0f,  10.0f } } },
		{ .position = { { -10.0f,  10.0f,  10.0f } } },
		{ .position = { { -10.0f,  10.0f, -10.0f } } },

		{ .position = { { -10.0f, -10.0f, -10.0f } } },
		{ .position = { { -10.0f, -10.0f,  10.0f } } },
		{ .position = { { 10.0f, -10.0f, -10.0f } } },
		{ .position = { { 10.0f, -10.0f, -10.0f } } },
		{ .position = { { -10.0f, -10.0f,  10.0f } } },
		{ .position = { { 10.0f, -10.0f,  10.0f } } } };

	glGenBuffers(1, &obj->vertex_buffer);
	glBindBuffer(GL_ARRAY_BUFFER, obj->vertex_buffer);
	glBufferData(GL_ARRAY_BUFFER, sizeof(cube_v_data), cube_v_data, GL_STATIC_DRAW);

	obj->nvertices = sizeof(cube_v_data)/sizeof(struct vertex_buffer_data);
}

static void setup_textured_unit_quad(struct graph_dev_primitive *obj)
{
	static const struct vertex_buffer_data quad_v_data[] = {
		{ { { -1.0f, -1.0f, 0.0f } } },
		{ { { 1.0f, 1.0f, 0.0f } } },
		{ { { -1.0f, 1.0f, 0.0f } } },

		{ { { -1.0f, -1.0f, 0.0f } } },
		{ { { 1.0f, -1.0f, 0.0f } } },
		{ { { 1.0f, 1.0f, 0.0f } } } };

	static const struct vertex_triangle_buffer_data quad_vt_data[] = {
		{ .texture_coord = { { 0.0f, 0.0f } } },
		{ .texture_coord = { { 1.0f, 1.0f } } },
		{ .texture_coord = { { 0.0f, 1.0f } } },

		{ .texture_coord = { { 0.0f, 0.0f } } },
		{ .texture_coord = { { 1.0f, 0.0f } } },
		{ .texture_coord = { { 1.0f, 1.0f } } } };

	glGenBuffers(1, &obj->vertex_buffer);
	glBindBuffer(GL_ARRAY_BUFFER, obj->vertex_buffer);
	glBufferData(GL_ARRAY_BUFFER, sizeof(quad_v_data), quad_v_data, GL_STATIC_DRAW);

	glGenBuffers(1, &obj->triangle_vertex_buffer);
	glBindBuffer(GL_ARRAY_BUFFER, obj->triangle_vertex_buffer);
	glBufferData(GL_ARRAY_BUFFER, sizeof(quad_vt_data), quad_vt_data, GL_STATIC_DRAW);

	obj->nvertices = sizeof(quad_v_data)/sizeof(struct vertex_buffer_data);
}

static void setup_textured_particle_shader(struct graph_dev_gl_textured_particle_shader *shader)
{
	maybe_unload_shader(&shader->meta, &shader->program_id);
	/* Create and compile our GLSL program from the shaders */
	shader->program_id = load_shaders(shader_directory,
				"textured-particle.vert", "textured-particle.frag",
				UNIVERSAL_SHADER_HEADER FILMIC_TONEMAPPING);
	/* create the VAO for this shader */
	glGenVertexArrays(1, &shader->vao_id);

	activate_shader(shader);

	shader->mvp_matrix_id = glGetUniformLocation(shader->program_id, "u_MVPMatrix");
	shader->camera_up_vec_id = glGetUniformLocation(shader->program_id, "u_CameraUpVec");
	shader->camera_right_vec_id = glGetUniformLocation(shader->program_id, "u_CameraRightVec");
	shader->time_id = glGetUniformLocation(shader->program_id, "u_Time");
	shader->radius_id = glGetUniformLocation(shader->program_id, "u_Radius");
	shader->texture_id = glGetUniformLocation(shader->program_id, "u_AlbedoTex");
	shader->filmic_tonemapping_id = glGetUniformLocation(shader->program_id, "u_FilmicTonemapping");
	shader->tonemapping_gain_id = glGetUniformLocation(shader->program_id, "u_TonemappingGain");
	glUniform1i(shader->texture_id, 0);

	shader->multi_one_id = glGetAttribLocation(shader->program_id, "a_MultiOne");
	shader->start_position_id = glGetAttribLocation(shader->program_id, "a_StartPosition");
	shader->start_tint_color_id = glGetAttribLocation(shader->program_id, "a_StartTintColor");
	shader->start_apm_id = glGetAttribLocation(shader->program_id, "a_StartAPM");
	shader->end_position_id = glGetAttribLocation(shader->program_id, "a_EndPosition");
	shader->end_tint_color_id = glGetAttribLocation(shader->program_id, "a_EndTintColor");
	shader->end_apm_id = glGetAttribLocation(shader->program_id, "a_StartAPM");
}

static void setup_fs_effect_shader(const char *basename,
	struct graph_dev_gl_fs_effect_shader *shader)
{
	const char *vert_header =
		UNIVERSAL_SHADER_HEADER
		"#define INCLUDE_VS 1\n";
	const char *frag_header =
		UNIVERSAL_SHADER_HEADER
		"#define INCLUDE_FS 1\n";

	/* Create and compile our GLSL program from the shaders */
	char shader_filename[255];
	snprintf(shader_filename, sizeof(shader_filename), "%s.shader", basename);

	const char *filenames[] = { shader_filename };

	maybe_unload_shader(&shader->meta, &shader->program_id);
	shader->program_id = load_concat_shaders(shader_directory, vert_header, 1, filenames,
		frag_header, 1, filenames);
	/* create the VAO for this shader */
	glGenVertexArrays(1, &shader->vao_id);

	activate_shader(shader);

	shader->mvp_matrix_id = glGetUniformLocation(shader->program_id, "u_MVPMatrix");
	shader->vertex_position_id = glGetAttribLocation(shader->program_id, "a_Position");
	shader->texture_coord_id = glGetAttribLocation(shader->program_id, "a_TexCoord");
	shader->tint_color_id = glGetUniformLocation(shader->program_id, "u_TintColor");
	shader->viewport_id = glGetUniformLocation(shader->program_id, "u_Viewport");
	shader->texture0_id = glGetUniformLocation(shader->program_id, "texture0Sampler");
	if (shader->texture0_id >= 0)
		glUniform1i(shader->texture0_id, 0);
	shader->texture1_id = glGetUniformLocation(shader->program_id, "texture1Sampler");
	if (shader->texture1_id >= 0)
		glUniform1i(shader->texture1_id, 1);
	shader->texture2_id = glGetUniformLocation(shader->program_id, "texture2Sampler");
	if (shader->texture2_id >= 0)
		glUniform1i(shader->texture2_id, 2);
}

static void setup_smaa_effect_shader(const char *basename, struct graph_dev_gl_fs_effect_shader *shader)
{
	char shader_filename[255];
	snprintf(shader_filename, sizeof(shader_filename), "%s.shader", basename);

	const char *vert_header;
	const char *frag_header;
	vert_header =
		"#version 150\n"
		"#define INCLUDE_VS 1\n";
	frag_header =
		"#version 150\n"
		"#define INCLUDE_FS 1\n";

	const char *filenames[] = { "smaa-high.shader", "SMAA.hlsl", shader_filename };

	maybe_unload_shader(&shader->meta, &shader->program_id);
	shader->program_id = load_concat_shaders(shader_directory,
				vert_header, 3, filenames, frag_header, 3, filenames);
	/* create the VAO for this shader */
	glGenVertexArrays(1, &shader->vao_id);

	shader->mvp_matrix_id = glGetUniformLocation(shader->program_id, "u_MVPMatrix");
	shader->vertex_position_id = glGetAttribLocation(shader->program_id, "a_Position");
	shader->texture_coord_id = glGetAttribLocation(shader->program_id, "a_TexCoord");
	shader->tint_color_id = -1;
	shader->viewport_id = glGetUniformLocation(shader->program_id, "u_Viewport");
	shader->texture0_id = -1;
	shader->texture1_id = -1;
	shader->texture2_id = -1;
}

static void setup_smaa_effect(struct graph_dev_smaa_effect *effect)
{
	struct graph_dev_gl_fs_effect_shader *shader;
	static const GLenum drawBuffers[] = { GL_COLOR_ATTACHMENT0 };

	shader = &effect->edge_shader;
	setup_smaa_effect_shader("smaa-edge", shader);

	activate_shader(shader);
	shader->texture0_id = glGetUniformLocation(shader->program_id, "u_AlbedoTex");
	glUniform1i(shader->texture0_id, 0);

	shader = &effect->blend_shader;
	setup_smaa_effect_shader("smaa-blend", shader);

	activate_shader(shader);
	shader->texture0_id = glGetUniformLocation(shader->program_id, "u_EdgeTex");
	glUniform1i(shader->texture0_id, 0);
	shader->texture1_id = glGetUniformLocation(shader->program_id, "u_AreaTex");
	glUniform1i(shader->texture1_id, 1);
	shader->texture2_id = glGetUniformLocation(shader->program_id, "u_SearchTex");
	glUniform1i(shader->texture2_id, 2);

	shader = &effect->neighborhood_shader;
	setup_smaa_effect_shader("smaa-neighborhood", shader);

	activate_shader(shader);
	shader->texture0_id = glGetUniformLocation(shader->program_id, "u_AlbedoTex");
	glUniform1i(shader->texture0_id, 0);
	shader->texture1_id = glGetUniformLocation(shader->program_id, "u_BlendTex");
	glUniform1i(shader->texture1_id, 1);

	graph_dev_gen_texture(1, &effect->edge_target.color0_texture);
	glBindTexture(GL_TEXTURE_2D, effect->edge_target.color0_texture);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	graph_dev_gen_texture(1, &effect->blend_target.color0_texture);
	glBindTexture(GL_TEXTURE_2D, effect->blend_target.color0_texture);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	/* include file defines sizes and areaTexBytes of the area texture */
#include "smaa/gl3/AreaTex.h"

	graph_dev_gen_texture(1, &effect->area_tex);
	glBindTexture(GL_TEXTURE_2D, effect->area_tex);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RG8, (GLsizei)AREATEX_WIDTH, (GLsizei)AREATEX_HEIGHT, 0,
		GL_RG, GL_UNSIGNED_BYTE, areaTexBytes);

	/* include file defines sizes and searchTexBytes of the search texture */
#include "smaa/gl3/SearchTex.h"

	graph_dev_gen_texture(1, &effect->search_tex);
	glBindTexture(GL_TEXTURE_2D, effect->search_tex);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, (GLsizei)SEARCHTEX_WIDTH, (GLsizei)SEARCHTEX_HEIGHT, 0,
		GL_RED, GL_UNSIGNED_BYTE, searchTexBytes);

	glGenFramebuffers(1, &effect->edge_target.fbo);
	glBindFramebuffer(GL_FRAMEBUFFER, effect->edge_target.fbo);
	glDrawBuffers(ARRAYSIZE(drawBuffers), drawBuffers);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
		effect->edge_target.color0_texture, 0);

	glGenFramebuffers(1, &effect->blend_target.fbo);
	glBindFramebuffer(GL_FRAMEBUFFER, effect->blend_target.fbo);
	glDrawBuffers(ARRAYSIZE(drawBuffers), drawBuffers);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
		effect->blend_target.color0_texture, 0);
}

static void setup_2d(void)
{
	memset(&render_target_2d, 0, sizeof(render_target_2d));

	glGenBuffers(1, &sgc.vertex_buffer_2d);
	glBindBuffer(GL_ARRAY_BUFFER, sgc.vertex_buffer_2d);
	glBufferData(GL_ARRAY_BUFFER, VERTEX_BUFFER_2D_SIZE, 0, GL_STREAM_DRAW);

	sgc.nvertex_2d = 0;

	/* render 2d to seperate fbo if supported */
	if (fbo_render_to_texture_supported()) {
		static const GLenum drawBuffers[] = {GL_COLOR_ATTACHMENT0};

		graph_dev_gen_texture(1, &render_target_2d.color0_texture);
		glBindTexture(GL_TEXTURE_2D, render_target_2d.color0_texture);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

		glGenFramebuffers(1, &render_target_2d.fbo);
		glBindFramebuffer(GL_FRAMEBUFFER, render_target_2d.fbo);
		glDrawBuffers(ARRAYSIZE(drawBuffers), drawBuffers);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
			render_target_2d.color0_texture, 0);
	}
}

static void setup_3d(void)
{
	sgc.gl_info_3d_line.nlines = 0;

	glGenBuffers(1, &sgc.gl_info_3d_line.vertex_buffer);
	glBindBuffer(GL_ARRAY_BUFFER, sgc.gl_info_3d_line.vertex_buffer);
	glBufferData(GL_ARRAY_BUFFER, 0, 0, GL_STREAM_DRAW);

	glGenBuffers(1, &sgc.gl_info_3d_line.line_vertex_buffer);
	glBindBuffer(GL_ARRAY_BUFFER, sgc.gl_info_3d_line.line_vertex_buffer);
	glBufferData(GL_ARRAY_BUFFER, 0, 0, GL_STREAM_DRAW);

	sgc.texture_unit_active = 0;
	memset(sgc.texture_unit_bind, 0, sizeof(sgc.texture_unit_bind));
	sgc.src_blend_func = GL_ONE;
	sgc.dest_blend_func = GL_ZERO;
	sgc.vp_x = 0;
	sgc.vp_y = 0;
	sgc.vp_width = 0;
	sgc.vp_height = 0;
}

void graph_dev_reload_all_shaders(void)
{
	PROFILE_ZONE_START("graph_dev_reload_all_shaders");

	setup_single_color_lit_shader(&single_color_lit_shader, 0);
	setup_single_color_lit_shader(&single_color_lit_shadow_shader, 1);
	setup_shadow_depth_shader(&shadow_depth_shader);
	setup_atmosphere_shader(&atmosphere_shader, 0);
	setup_atmosphere_shader(&atmosphere_with_annulus_shadow_shader, 1);
	setup_trans_wireframe_shader("wireframe_transparent", &trans_wireframe_shader);
	setup_trans_wireframe_shader("wireframe-transparent-sphere-clip", &trans_wireframe_with_clip_sphere_shader);
	setup_filled_wireframe_shader(&filled_wireframe_shader);
	setup_single_color_shader(&single_color_shader);
	setup_vertex_color_shader(&vertex_color_shader);
	setup_sun_shader(&sun_shader);
	setup_black_hole_shader(&black_hole_shader);
	setup_line_single_color_shader(&line_single_color_shader);
	setup_point_cloud_shader("point_cloud", &point_cloud_shader);
	setup_color_by_w_shader(&color_by_w_shader);
	setup_skybox_shader(&skybox_shader);
	setup_textured_shader("textured", UNIVERSAL_SHADER_HEADER FILMIC_TONEMAPPING, &textured_shader);
	setup_textured_shader("textured-with-sphere-shadow-per-pixel", UNIVERSAL_SHADER_HEADER FILMIC_TONEMAPPING,
				&textured_with_sphere_shadow_shader);
	setup_textured_shader("textured-and-lit-per-pixel",
				UNIVERSAL_SHADER_HEADER FILMIC_TONEMAPPING SHADOW_CASCADES_HEADER
				"#define USE_CSM 1\n", &textured_lit_shader);
	setup_textured_shader("textured-and-lit-per-pixel",
				UNIVERSAL_SHADER_HEADER FILMIC_TONEMAPPING SHADOW_CASCADES_HEADER
				"#define USE_EMIT_MAP\n"
				"#define USE_CSM 1\n",
				&textured_lit_emit_shader);
	setup_textured_shader("textured-and-lit-per-pixel", UNIVERSAL_SHADER_HEADER FILMIC_TONEMAPPING
				SHADOW_CASCADES_HEADER
				"#define USE_EMIT_MAP\n"
				"#define USE_NORMAL_MAP 1\n"
				"#define USE_CSM 1\n",
				&textured_lit_emit_normal_shader);
	setup_textured_shader("textured-and-lit-per-pixel", UNIVERSAL_SHADER_HEADER FILMIC_TONEMAPPING
				SHADOW_CASCADES_HEADER
				"#define USE_NORMAL_MAP 1\n"
				"#define USE_CSM 1\n",
				&textured_lit_normal_shader);
	setup_textured_cubemap_shader("textured-cubemap-shield-per-pixel", 0, 0, 0, 0,
					&textured_cubemap_shield_shader);
	/* Set up CSM textured cubemap shaders */
	setup_textured_cubemap_shader("textured-cubemap-and-lit-with-annulus-shadow-per-pixel", 0, 0, 0, 1,
					&textured_cubemap_lit_shadow_shader);
	setup_textured_cubemap_shader("textured-cubemap-and-lit-with-annulus-shadow-per-pixel", 1, 0, 0, 1,
					&textured_cubemap_lit_normal_map_shadow_shader);
	setup_textured_cubemap_shader("textured-cubemap-and-lit-with-annulus-shadow-per-pixel", 0, 0, 1, 1,
					&textured_cubemap_lit_with_annulus_shadow_shader);
	setup_textured_cubemap_shader("textured-cubemap-and-lit-with-annulus-shadow-per-pixel", 1, 0, 1, 1,
					&textured_cubemap_normal_mapped_lit_with_annulus_shadow_shader);
	setup_textured_cubemap_shader("textured-cubemap-and-lit-with-annulus-shadow-per-pixel", 1, 1, 1, 1,
					&textured_cubemap_normal_mapped_lit_with_annulus_shadow_specular_shader);
	setup_textured_cubemap_shader("textured-cubemap-and-lit-with-annulus-shadow-per-pixel", 1, 1, 0, 1,
					&textured_cubemap_normal_mapped_lit_specular_shadow_shader);
	/* Set up non-CSM textured cubemap shaders */
	setup_textured_cubemap_shader("textured-cubemap-and-lit-with-annulus-shadow-per-pixel", 0, 0, 0, 0,
					&textured_cubemap_lit_shader);
	setup_textured_cubemap_shader("textured-cubemap-and-lit-with-annulus-shadow-per-pixel", 1, 0, 0, 0,
					&textured_cubemap_lit_normal_map_shader);
	setup_textured_cubemap_shader("textured-cubemap-and-lit-with-annulus-shadow-per-pixel", 0, 0, 1, 0,
					&textured_cubemap_lit_with_annulus_shader);
	setup_textured_cubemap_shader("textured-cubemap-and-lit-with-annulus-shadow-per-pixel", 1, 0, 1, 0,
					&textured_cubemap_normal_mapped_lit_with_annulus_shader);
	setup_textured_cubemap_shader("textured-cubemap-and-lit-with-annulus-shadow-per-pixel", 1, 1, 1, 0,
					&textured_cubemap_normal_mapped_lit_with_annulus_specular_shader);
	setup_textured_cubemap_shader("textured-cubemap-and-lit-with-annulus-shadow-per-pixel", 1, 1, 0, 0,
					&textured_cubemap_normal_mapped_lit_specular_shader);

	setup_textured_particle_shader(&textured_particle_shader);
	setup_fs_effect_shader("fs-effect-copy", &fs_copy_shader);
	setup_textured_shader("alpha_by_normal", UNIVERSAL_SHADER_HEADER, &alpha_by_normal_shader);
	setup_textured_shader("alpha_by_normal", UNIVERSAL_SHADER_HEADER "#define TEXTURED_ALPHA_BY_NORMAL",
				&textured_alpha_by_normal_shader);
	setup_textured_shader("planetary-lightning", UNIVERSAL_SHADER_HEADER, &planetary_lightning_shader);
	setup_textured_shader("warp-gate-effect", UNIVERSAL_SHADER_HEADER FILMIC_TONEMAPPING, &warp_gate_effect_shader);

	if (fbo_render_to_texture_supported())
		setup_smaa_effect(&smaa_effect);

	PROFILE_ZONE_END();
}

static void enqueue_image_load_request(struct graph_dev_image_load_request *r)
{
	PROFILE_ZONE_START("enqueue_image_load_request");
	work_queue_enqueue(image_loader_wq, r);
	PROFILE_ZONE_END();
}

static void enqueue_image_load_completion(struct graph_dev_image_load_request *r)
{
	PROFILE_ZONE_START("enqueue_image_load_completion");
	work_queue_enqueue(loaded_images_wq, r);
	PROFILE_ZONE_END();
}

static void process_image_load_request_normal(struct graph_dev_image_load_request *r)
{
	PROFILE_ZONE_START("process_image_load_request_normal");
	r->image_data[0] = png_utils_read_png_image(r->filename[0],
				r->flipVertical, r->flipHorizontal, r->pre_multiply_alpha,
				&r->w[0], &r->h[0], &r->hasAlpha[0], r->whynot, sizeof(r->whynot));
	if (!r->image_data[0]) {
		fprintf(stderr, "Failed to decode image file '%s: %s\n",
			r->filename[0], r->whynot);
		graph_dev_free_image_load_request(r);
		PROFILE_ZONE_END();
		return;
	}
	/* Put the data on the queue for the main thread to upload to the GPU */
	enqueue_image_load_completion(r);
	PROFILE_ZONE_END();
}

static void process_image_load_request_cubemap(struct graph_dev_image_load_request *r)
{
	PROFILE_ZONE_START("process_image_load_request_cubemap");
	for (int i = 0; i < 6; i++) {
		r->image_data[i] = png_utils_read_png_image(r->filename[i], 0, r->is_inside, 1,
			&r->w[i], &r->h[i], &r->hasAlpha[i], r->whynot, sizeof(r->whynot));
		if (!r->image_data[i]) {
			fprintf(stderr, "Failed to decode image file '%s: %s\n",
				r->filename[i], r->whynot);
			graph_dev_free_image_load_request(r);
			PROFILE_ZONE_END();
			return;
		}
	}
	/* Put the data on the queue for the main thread to upload to the GPU */
	enqueue_image_load_completion(r);
	PROFILE_ZONE_END();
}

/* Process a request to load an image */
static void process_image_load_request(void *work)
{
	PROFILE_ZONE_START("process_image_load_request");
	struct graph_dev_image_load_request *r = work;
	switch (r->request_type) {
	case GRAPH_DEV_IMAGE_LOAD:
		process_image_load_request_normal(r);
		break;
	case GRAPH_DEV_CUBEMAP_LOAD:
		process_image_load_request_cubemap(r);
		break;
	default:
		fprintf(stderr, "Bad image load request type %d, discarding\n", r->request_type);
		graph_dev_free_image_load_request(r);
		break;
	}
	PROFILE_ZONE_END();
}

/* Set up work queues for loading texture data concurrently with main loop */
static void graph_dev_set_up_image_loader_work_queues(void)
{
	image_loader_wq = work_queue_init("png-decode", IMAGE_LOADER_QUEUE_DEPTH,
		IMAGE_LOADER_THREAD_COUNT, process_image_load_request);
	loaded_images_wq = work_queue_init("txtr2gpu", IMAGE_LOADER_QUEUE_DEPTH, 0, NULL);
}

int graph_dev_setup(const char *asset_dir)
{
	if (!gladLoadGL((GLADloadfunc)SDL_GL_GetProcAddress)) {
		fprintf(stderr, "Got error trying to bind GL\n");
		return -1;
	}
	printf("Initialized GLAD\n");

	const char *version = (const char *)glGetString(GL_VERSION);
	const char *vendor = (const char *)glGetString(GL_VENDOR);
	const char *renderer = (const char *)glGetString(GL_RENDERER);
	const char *glslversion = (const char *)glGetString(GL_SHADING_LANGUAGE_VERSION);
	fprintf(stderr, "OpenGL: Version:  %s\n", version);
	fprintf(stderr, "        Vendor:   %s\n", vendor);
	fprintf(stderr, "        Renderer: %s\n", renderer);
	fprintf(stderr, "        Shader Language Version: %s\n", glslversion);

	if (!GLAD_GL_VERSION_3_1) {
		fprintf(stderr, "Need at least OpenGL 3.1\n");
		return -1;
	}

	if (framebuffer_srgb_supported())
		printf("sRGB framebuffer supported\n");

	if (texture_srgb_supported())
		printf("sRGB texture supported\n");

	glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);

	if (asset_dir)
		snprintf(shader_directory, sizeof(shader_directory), "%s/shader", asset_dir);
	else
		strlcpy(shader_directory, default_shader_directory, sizeof(shader_directory));

	fprintf(stderr, "shader dir = %s\n", shader_directory);

	glDepthFunc(GL_LESS);

	memset(&msaa, 0, sizeof(msaa));
	memset(&post_target0, 0, sizeof(post_target0));
	memset(&post_target1, 0, sizeof(post_target1));
	memset(&smaa_effect, 0, sizeof(smaa_effect));

	if (msaa_render_to_fbo_supported()) {
		glGenFramebuffers(1, &msaa.fbo);
		glGenRenderbuffers(1, &msaa.color0_buffer);
		glGenRenderbuffers(1, &msaa.depth_buffer);
		msaa.width = 0;
		msaa.height = 0;
		msaa.samples = 0;
	}

	if (fbo_render_to_texture_supported()) {
		static GLenum drawBuffers[] = { GL_COLOR_ATTACHMENT0 };
		graph_dev_gen_texture(1, &post_target0.color0_texture);
		glBindTexture(GL_TEXTURE_2D, post_target0.color0_texture);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

		glGenRenderbuffers(1, &post_target0.depth_buffer);
		glBindRenderbuffer(GL_RENDERBUFFER, post_target0.depth_buffer);

		glGenFramebuffers(1, &post_target0.fbo);
		glBindFramebuffer(GL_FRAMEBUFFER, post_target0.fbo);
		glDrawBuffers(ARRAYSIZE(drawBuffers), drawBuffers);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
			post_target0.color0_texture, 0);
		glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER,
			post_target0.depth_buffer);

		graph_dev_gen_texture(1, &post_target1.color0_texture);
		glBindTexture(GL_TEXTURE_2D, post_target1.color0_texture);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

		glGenFramebuffers(1, &post_target1.fbo);
		glBindFramebuffer(GL_FRAMEBUFFER, post_target1.fbo);
		glDrawBuffers(ARRAYSIZE(drawBuffers), drawBuffers);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
			post_target1.color0_texture, 0);
	}

	{
		const char *sdbg = getenv("SNIS_SHADOW_DEBUG");
		graph_dev_shadow_map_debug = sdbg ? atoi(sdbg) : 0;
	}
	if (graph_dev_shadow_map_enabled)
		setup_shadow_map_fbo();

	graph_dev_reload_all_shaders();

	/* after all the shaders are loaded */
	setup_cubemap_cube(&cubemap_cube);
	setup_textured_unit_quad(&textured_unit_quad);

	setup_2d();
	setup_3d();

	graph_dev_set_up_image_loader_work_queues();

	return 0;
}

/* returns zero on success, -1 otherwise */
static int cubemap_texture_to_gpu(struct graph_dev_image_load_request *r)
{
	PROFILE_ZONE_START("cubemap_texture_to_gpu");
	static const GLint tex_pos[] = {
		GL_TEXTURE_CUBE_MAP_POSITIVE_X, GL_TEXTURE_CUBE_MAP_NEGATIVE_X,
		GL_TEXTURE_CUBE_MAP_POSITIVE_Y, GL_TEXTURE_CUBE_MAP_NEGATIVE_Y,
		GL_TEXTURE_CUBE_MAP_POSITIVE_Z, GL_TEXTURE_CUBE_MAP_NEGATIVE_Z };
	GLint colorspace;

	glBindTexture(GL_TEXTURE_CUBE_MAP, r->texture_id);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

	int i;
	for (i = 0; i < NCUBEMAP_TEXTURES; i++) {
		/* do horizontal invert if we are projecting on the inside */
		char *image_data = r->image_data[i];

		if (r->linear_colorspace)
			colorspace = r->hasAlpha[i] ? GL_RGBA8 : GL_RGB8;
		else
			colorspace = r->hasAlpha[i] ? GL_SRGB8_ALPHA8 : GL_SRGB8;
		glTexImage2D(tex_pos[i], 0, colorspace, r->w[i], r->h[i], 0,
				(r->hasAlpha[i] ? GL_RGBA : GL_RGB), GL_UNSIGNED_BYTE, image_data);
	}
	glGenerateMipmap(GL_TEXTURE_CUBE_MAP);
	note_texture_bound_outside_cache(r->texture_id);

	pthread_mutex_lock(&finished_loading_mutex);

	/* Are we re-using a texture name or making a new one? */
	int n;
	if (r->loaded_texture_index == -1)
		n = nloaded_cubemap_textures;
	else
		n = r->loaded_texture_index;
	loaded_cubemap_textures[n].texture_id = r->texture_id;
	loaded_cubemap_textures[n].is_inside = r->is_inside;
	loaded_cubemap_textures[n].linear_colorspace = r->linear_colorspace;
	for (i = 0; i < NCUBEMAP_TEXTURES; i++) {
		if (loaded_cubemap_textures[n].filename[i])
			free(loaded_cubemap_textures[n].filename[i]);
		loaded_cubemap_textures[n].filename[i] = strdup(r->filename[i]);
	}
	if (r->loaded_texture_index == -1)
		nloaded_cubemap_textures++;

	GLuint tid = r->texture_id;
	mark_texture_load_complete(tid);

	pthread_mutex_unlock(&finished_loading_mutex);
	graph_dev_free_image_load_request(r);
	PROFILE_ZONE_END();
	return 0;
}

void graph_dev_expire_all_textures(void)
{
	int i;
	PROFILE_ZONE_START("graph_dev_expire_all_textures");

	pthread_mutex_lock(&finished_loading_mutex);
	for (i = 0; i < nloaded_textures; i++)
		loaded_textures[i].expired = 1;
	for (i = 0; i < nloaded_cubemap_textures; i++)
		loaded_cubemap_textures[i].expired = 1;
	pthread_mutex_unlock(&finished_loading_mutex);

	PROFILE_ZONE_END();
}

void graph_dev_expire_texture(char *filename)
{
	int i;
	PROFILE_ZONE_START("graph_dev_expire_texture");

	pthread_mutex_lock(&finished_loading_mutex);
	for (i = 0; i < nloaded_textures; i++)
		if (strcmp(loaded_textures[i].filename, filename) == 0) {
			loaded_textures[i].expired = 1;
			break;
		}
	pthread_mutex_unlock(&finished_loading_mutex);
	PROFILE_ZONE_END();
}

void graph_dev_expire_cubemap_texture(int is_inside,
					const char *texture_filename_pos_x,
					const char *texture_filename_neg_x,
					const char *texture_filename_pos_y,
					const char *texture_filename_neg_y,
					const char *texture_filename_pos_z,
					const char *texture_filename_neg_z)
{
	int i, j;
	PROFILE_ZONE_START("graph_dev_expire_cubemap_texture");

	const char *tex_filenames[] = {
		texture_filename_pos_x, texture_filename_neg_x,
		texture_filename_pos_y, texture_filename_neg_y,
		texture_filename_pos_z, texture_filename_neg_z };

	for (i = 0; i < nloaded_cubemap_textures; i++) {
		if (loaded_cubemap_textures[i].is_inside == is_inside) {
			int match = 1;
			for (j = 0; j < NCUBEMAP_TEXTURES; j++) {
				if (strcmp(tex_filenames[j], loaded_cubemap_textures[i].filename[j]) != 0) {
					match = 0;
					break;
				}
			}
			if (match) {
				loaded_cubemap_textures[i].expired = 1;
				PROFILE_ZONE_END();
				return;
			}
		}
	}
	PROFILE_ZONE_END();
}

unsigned int graph_dev_load_cubemap_texture(
	int is_inside,
	int linear_colorspace,
	const char *texture_filename_pos_x,
	const char *texture_filename_neg_x,
	const char *texture_filename_pos_y,
	const char *texture_filename_neg_y,
	const char *texture_filename_pos_z,
	const char *texture_filename_neg_z)
{
	int loaded_texture_index = -1;
	const char *tex_filenames[] = {
		texture_filename_pos_x, texture_filename_neg_x,
		texture_filename_pos_y, texture_filename_neg_y,
		texture_filename_pos_z, texture_filename_neg_z };

	/* Check if we already loaded this texture */
	pthread_mutex_lock(&finished_loading_mutex);
	for (int i = 0; i < nloaded_cubemap_textures; i++) {
		if (loaded_cubemap_textures[i].is_inside == is_inside) {
			int match = 1;
			for (int j = 0; j < NCUBEMAP_TEXTURES; j++) {
				if (strcmp(tex_filenames[j], loaded_cubemap_textures[i].filename[j]) != 0) {
					match = 0;
					break;
				}
			}
			if (match) {
				loaded_cubemap_textures[i].expired = 0;
				pthread_mutex_unlock(&finished_loading_mutex);
				return loaded_cubemap_textures[i].texture_id;
			}
		}
	}

	/* See if we can re-use an expired texture (not the texture name itself though) */
	GLuint cube_texture_id = (GLuint) -1;
	for (int i = 0; i < nloaded_cubemap_textures; i++) {
		if (loaded_cubemap_textures[i].expired) {
			cube_texture_id = loaded_cubemap_textures[i].texture_id;
			loaded_texture_index = i;
			glDeleteTextures(1, &cube_texture_id);
			loaded_cubemap_textures[i].is_inside = is_inside;
			loaded_cubemap_textures[i].expired = 0;
			for (int j = 0; j < NCUBEMAP_TEXTURES; j++) {
				fprintf(stderr, "Replacing %s with %s\n",
					loaded_cubemap_textures[i].filename[j], tex_filenames[j]);
				if (loaded_cubemap_textures[i].filename[j])
					free(loaded_cubemap_textures[i].filename[j]);
				loaded_cubemap_textures[i].filename[j] = strdup(tex_filenames[j]);
			}
			loaded_cubemap_textures[i].linear_colorspace = linear_colorspace;
			break;
		}
	}

	if (nloaded_cubemap_textures >= MAX_LOADED_CUBEMAP_TEXTURES) {
		printf("Unable to load cubemap texture '%s': max of %d textures are already loaded\n",
			texture_filename_pos_x, nloaded_cubemap_textures);
		pthread_mutex_unlock(&finished_loading_mutex);
		return 0;
	}

	if (cube_texture_id != (GLuint) -1)
		mark_texture_load_unused(cube_texture_id);
	cube_texture_id = -1;
	graph_dev_gen_texture_no_lock(1, &cube_texture_id);

	mark_texture_load_pending(cube_texture_id);
	pthread_mutex_unlock(&finished_loading_mutex);

	struct graph_dev_image_load_request *r = calloc(1, sizeof(*r));
	r->texture_id = cube_texture_id;
	r->loaded_texture_index = loaded_texture_index;
	r->request_type = GRAPH_DEV_CUBEMAP_LOAD;
	r->is_inside = is_inside;
	r->linear_colorspace = linear_colorspace;
	for (int i = 0; i < 6; i++)
		r->filename[i] = strdup(tex_filenames[i]);
	r->flipVertical = 1;
	r->flipHorizontal = 0;
	r->pre_multiply_alpha = 1;
	r->use_mipmaps = 1;

	enqueue_image_load_request(r);
	return (unsigned int) cube_texture_id;
}

static time_t get_file_modify_time(const char *filename)
{
	struct stat s;
	if (stat(filename, &s) != 0)
		return 0;
	return s.st_mtime;
}

int graph_dev_reload_cubemap_textures(void)
{
	int failed = 0;

	/* Build a list of requests to reload all the cubemap textures */
	pthread_mutex_lock(&finished_loading_mutex);
	int n = nloaded_cubemap_textures;
	struct graph_dev_image_load_request **r = calloc(n, sizeof(*r));
	if (!r) {
		pthread_mutex_unlock(&finished_loading_mutex);
		return -1;
	}
	for (int i = 0; i < n; i++) {
		r[i] = calloc(1, sizeof(*r[i]));
		if (!r[i])
			continue;
		r[i]->texture_id = loaded_cubemap_textures[i].texture_id;
		r[i]->request_type = GRAPH_DEV_CUBEMAP_LOAD;
		r[i]->is_inside = loaded_cubemap_textures[i].is_inside;
		r[i]->linear_colorspace = loaded_cubemap_textures[i].linear_colorspace;
		for (int j = 0; j < 6; j++) {
			r[i]->filename[j] = strdup(loaded_cubemap_textures[i].filename[j]);
		}
		r[i]->flipVertical = 1;
		r[i]->flipHorizontal = 0;
		r[i]->pre_multiply_alpha = 1;
		r[i]->use_mipmaps = 1;

	}
	pthread_mutex_unlock(&finished_loading_mutex);

	/* Submit list of requests to work queue */
	for (int i = 0; i < n; i++)
		enqueue_image_load_request(r[i]);
	/* the indivdual pointers within r[] will get freed after the work is processed
	 * but we need to free r itself now.
	 */
	free(r);
	return failed;
}

/* Load image data to GPU, image_data is not freed */
static int texture_to_gpu_id(GLuint texture_number, char *image_data,
		int w, int h, int hasAlpha, int use_mipmaps, int linear_colorspace)
{
	GLint colorspace;

	if (!image_data) {
		fprintf(stderr, "texture_to_gpu_id: NULL image data\n");
		return -1;
	}

	if (linear_colorspace)
		colorspace = hasAlpha ? GL_RGBA8 : GL_RGB8;
	else
		colorspace = hasAlpha ? GL_SRGB8_ALPHA8 : GL_SRGB8;

	glBindTexture(GL_TEXTURE_2D, texture_number);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	if (use_mipmaps)
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	else
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

	glTexImage2D(GL_TEXTURE_2D, 0, colorspace, w, h, 0,
			(hasAlpha ? GL_RGBA : GL_RGB), GL_UNSIGNED_BYTE, image_data);
	if (use_mipmaps)
		glGenerateMipmap(GL_TEXTURE_2D);
	note_texture_bound_outside_cache(texture_number);
	return 0;
}

void graph_dev_free_image_load_request(struct graph_dev_image_load_request *r)
{
	if (!r)
		return;
	for (int i = 0; i < 6; i++) {
		if (r->filename[i])
			free(r->filename[i]);
		if (r->image_data[i])
			free(r->image_data[i]);
	}
	free(r);
}

unsigned int graph_dev_texture_to_gpu(struct graph_dev_image_load_request *r)
{
	if (r->request_type == GRAPH_DEV_CUBEMAP_LOAD)
		return cubemap_texture_to_gpu(r);

	if (texture_to_gpu_id(r->texture_id, r->image_data[0], r->w[0], r->h[0], r->hasAlpha[0],
				r->use_mipmaps, r->linear_colorspace)) {
		glDeleteTextures(1, (GLuint *) &r->texture_id);
		fprintf(stderr, "Failed to load texture to gpu from '%s'\n", r->filename[0]);
		return 0;
	}

	pthread_mutex_lock(&finished_loading_mutex);
	int n = (r->loaded_texture_index != -1) ?  r->loaded_texture_index : nloaded_textures;
	loaded_textures[n].texture_id = r->texture_id;
	if (loaded_textures[n].filename)
		free(loaded_textures[n].filename);
	loaded_textures[n].filename = strdup(r->filename[0]);
	loaded_textures[n].mtime = get_file_modify_time(r->filename[0]);
	loaded_textures[n].last_mtime_change = 0;
	loaded_textures[n].expired = 0;
	loaded_textures[n].use_mipmaps = r->use_mipmaps;
	loaded_textures[n].linear_colorspace = r->linear_colorspace;

	if (r->loaded_texture_index == -1)
		nloaded_textures++;
	int tid = r->texture_id;
	mark_texture_load_complete(tid);
	pthread_mutex_unlock(&finished_loading_mutex);
	graph_dev_free_image_load_request(r);
	return (unsigned int) tid;
}

const char *graph_dev_get_texture_filename(unsigned int texture_id)
{
	int i;
	pthread_mutex_lock(&finished_loading_mutex);
	for (i = 0; i < nloaded_textures; i++) {
		if (texture_id == loaded_textures[i].texture_id) {
			char *fname = loaded_textures[i].filename;
			pthread_mutex_unlock(&finished_loading_mutex);
			/* FIXME: Racy to allow this to be used, why do I need this?
			 * digging into it, it appears only to be used (eventually) by
			 * material_nebula_write_to_file(), which isn't used anywhere?
			 * Probably used at one point to create the data in
			 * share/snis/material/nebula*.mat
			 */
			return fname;
		}
	}
	pthread_mutex_unlock(&finished_loading_mutex);
	return "";
}

int graph_dev_reload_textures(void)
{
	/* Build a list of requests to re-load all the textures */
	pthread_mutex_lock(&finished_loading_mutex);
	int n = nloaded_textures;
	struct graph_dev_image_load_request **r = calloc(n, sizeof(*r));
	for (int i = 0; i < n; i++) {
		r[i] = calloc(1, sizeof(*r[i]));
		r[i]->texture_id = loaded_textures[i].texture_id;
		r[i]->loaded_texture_index = i;
		r[i]->request_type = GRAPH_DEV_IMAGE_LOAD;
		r[i]->filename[0] = strdup(loaded_textures[i].filename);
		r[i]->flipVertical = 1;
		r[i]->flipHorizontal = 0;
		r[i]->pre_multiply_alpha = 1;
		r[i]->linear_colorspace = loaded_textures[i].linear_colorspace;
		r[i]->use_mipmaps = loaded_textures[i].use_mipmaps;
	}
	pthread_mutex_unlock(&finished_loading_mutex);

	/* Submit the list of requests to re-load all the textures to work queue */
	for (int i = 0; i < n; i++)
		enqueue_image_load_request(r[i]);

	/* the indivdual pointers within r[] will get freed after the work is processed
	 * but we need to free r itself now.
	 */
	free(r);
	return 0;
}

int graph_dev_reload_changed_textures(void)
{
	int n = 0;

	/* Build a list of requests to reload changed textures */
	pthread_mutex_lock(&finished_loading_mutex);
	struct graph_dev_image_load_request **r = calloc(nloaded_textures, sizeof(*r));
	for (int i = 0; i < nloaded_textures; i++) {
		time_t mtime = get_file_modify_time(loaded_textures[i].filename);
		if (loaded_textures[i].mtime != mtime) {
			loaded_textures[i].mtime = mtime;
			loaded_textures[i].last_mtime_change = time_now_double();
		} else if (loaded_textures[i].last_mtime_change > 0 &&
			time_now_double() - loaded_textures[i].last_mtime_change >= TEX_RELOAD_DELAY) {
			printf("reloading texture '%s'\n", loaded_textures[i].filename);
			r[n] = calloc(1, sizeof(*r[n]));
			r[n]->texture_id = loaded_textures[i].texture_id;
			r[n]->loaded_texture_index = i;
			r[n]->request_type = GRAPH_DEV_IMAGE_LOAD;
			r[n]->filename[0] = strdup(loaded_textures[i].filename);
			r[n]->flipVertical = 1;
			r[n]->flipHorizontal = 0;
			r[n]->pre_multiply_alpha = 1;
			r[n]->linear_colorspace = loaded_textures[i].linear_colorspace;
			r[n]->use_mipmaps = loaded_textures[i].use_mipmaps;
			n++;
			loaded_textures[i].last_mtime_change = 0;
		}
	}
	pthread_mutex_unlock(&finished_loading_mutex);

	/* Submit list of requests to image loader work queue */
	for (int i = 0; i < n; i++)
		enqueue_image_load_request(r[i]);

	/* the indivdual pointers within r[] will get freed after the work is processed
	 * but we need to free r itself now.
	 */
	free(r);

	return 0;
}

int graph_dev_reload_changed_cubemap_textures(void)
{
	int n = 0;
	pthread_mutex_lock(&finished_loading_mutex);
	struct graph_dev_image_load_request **r = calloc(nloaded_cubemap_textures, sizeof(*r));
	for (int i = 0; i < nloaded_cubemap_textures; i++) {
		time_t mtime = get_file_modify_time(loaded_cubemap_textures[i].filename[5]);
		if (loaded_cubemap_textures[i].mtime != mtime) {
			loaded_cubemap_textures[i].mtime = mtime;
			loaded_cubemap_textures[i].last_mtime_change = time_now_double();
		} else if (loaded_cubemap_textures[i].last_mtime_change > 0 &&
			time_now_double() - loaded_cubemap_textures[i].last_mtime_change >=
					CUBEMAP_TEX_RELOAD_DELAY) {
			printf("reloading cubemap texture '%s'\n",
				loaded_cubemap_textures[i].filename[0]);
			loaded_cubemap_textures[i].last_mtime_change = 0;
			r[n] = calloc(1, sizeof(*r[n]));
			r[n]->texture_id = loaded_cubemap_textures[i].texture_id;
			r[n]->loaded_texture_index = i;
			r[n]->request_type = GRAPH_DEV_CUBEMAP_LOAD;
			r[n]->is_inside = loaded_cubemap_textures[i].is_inside;
			r[n]->linear_colorspace = loaded_cubemap_textures[i].linear_colorspace;
			for (int j = 0; j < 6; j++)
				r[n]->filename[j] = strdup(loaded_cubemap_textures[i].filename[j]);
			r[n]->flipVertical = 1;
			r[n]->flipHorizontal = 0;
			r[n]->pre_multiply_alpha = 1;
			r[n]->use_mipmaps = 1;
			n++;
		}
	}
	pthread_mutex_unlock(&finished_loading_mutex);

	for (int i = 0; i < n; i++)
		enqueue_image_load_request(r[i]);
	free(r);
	return 0;
}

/* returns 0 on success, -1 otherwise */
int graph_dev_load_skybox_texture(
	const char *texture_filename_pos_x,
	const char *texture_filename_neg_x,
	const char *texture_filename_pos_y,
	const char *texture_filename_neg_y,
	const char *texture_filename_pos_z,
	const char *texture_filename_neg_z)
{
	skybox_shader.cube_texture_id = graph_dev_load_cubemap_texture(1, 0, texture_filename_pos_x,
		texture_filename_neg_x, texture_filename_pos_y, texture_filename_neg_y, texture_filename_pos_z,
		texture_filename_neg_z);

	if (skybox_shader.cube_texture_id != 0)
		return 0;
	return -1;
}


#if MOVING_STARFIELD
/* The star field, drawn once before the scene with the depth test off.  The fade is zero at
 * both faces of the shell and full across the middle third, so stars recycling in and out at
 * the boundaries are never seen to do it; see the STAR_FIELD_OUTER note in entity.c for why
 * the radius is a floor rather than a ceiling. */
void graph_dev_draw_star_field(struct entity_context *cx, const struct mat44 *mat_mvp)
{
	float camera_pos[3], fade_params[4];
	struct sng_color color = sng_get_color(GRAY75);
	float r = cx->star_field_radius;

	if (!cx->star_field_mesh || cx->nstar_field <= 0 || r <= 0.0)
		return;

	camera_pos[0] = cx->camera.x;
	camera_pos[1] = cx->camera.y;
	camera_pos[2] = cx->camera.z;
	fade_params[0] = 1.00 * r;
	fade_params[1] = 1.50 * r;
	fade_params[2] = 2.00 * r;
	fade_params[3] = 3.00 * r;

	graph_dev_raster_point_cloud_mesh(&point_cloud_shader, mat_mvp, cx->star_field_mesh,
				&color, 1.0, 2.0, 1, camera_pos, fade_params, 1);
}
#endif

void graph_dev_draw_skybox(const struct mat44 *mat_vp)
{
	if (!graph_dev_texture_ready(skybox_shader.cube_texture_id))
		return;

	draw_vertex_buffer_2d();

	enable_3d_viewport();

	glDepthMask(GL_FALSE);
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_CULL_FACE);

	activate_shader(&skybox_shader);

	BIND_TEXTURE(GL_TEXTURE0, GL_TEXTURE_CUBE_MAP, skybox_shader.cube_texture_id);

	glUniformMatrix4fv(skybox_shader.mvp_id, 1, GL_FALSE, &mat_vp->m[0][0]);
	if (skybox_shader.filmic_tonemapping_id >= 0)
		glUniform1f(skybox_shader.filmic_tonemapping_id, (float) filmic_tonemapping);
	if (skybox_shader.tonemapping_gain_id >= 0)
		glUniform1f(skybox_shader.tonemapping_gain_id, tonemapping_gain);
	if (skybox_shader.lens_dir_id >= 0)
		glUniform3fv(skybox_shader.lens_dir_id, MAX_GRAVITATIONAL_LENSES, gravitational_lens_dir);
	if (skybox_shader.lens_params_id >= 0)
		glUniform3fv(skybox_shader.lens_params_id, MAX_GRAVITATIONAL_LENSES,
				gravitational_lens_params);

	glEnableVertexAttribArray(skybox_shader.vertex_id);
	glBindBuffer(GL_ARRAY_BUFFER, cubemap_cube.vertex_buffer);
	glVertexAttribPointer(
		skybox_shader.vertex_id, /* The attribute we want to configure */
		3,                           /* size */
		GL_FLOAT,                    /* type */
		GL_FALSE,                    /* normalized? */
		sizeof(struct vertex_buffer_data), /* stride */
		(void *)offsetof(struct vertex_buffer_data, position.v.x) /* array buffer offset */
	);

	glDrawArrays(GL_TRIANGLES, 0, cubemap_cube.nvertices);

	glDisableVertexAttribArray(skybox_shader.vertex_id);
	glDepthMask(GL_TRUE);
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_CULL_FACE);
}

static void debug_menu_draw_item(char *item, int itemnumber, int grayed, int checked)
{
	int x = 15;
	int y = 35 + itemnumber * 20;

	if (grayed)
		sng_set_foreground(GRAY75);
	else
		sng_set_foreground(WHITE);

	graph_dev_draw_rectangle(0, x, y, 15, 15);
	if (checked)
		graph_dev_draw_rectangle(1, x + 2, y + 2, 11, 11);
	sng_abs_xy_draw_string(item, NANO_FONT, (x + 20) / sgc.x_scale, (y + 10) / sgc.y_scale);
}

void graph_dev_display_debug_menu_show(void)
{
	sng_set_foreground(BLACK);
	graph_dev_draw_rectangle(1, 10, 30, 370 * sgc.x_scale, 305);
	sng_set_foreground(WHITE);
	graph_dev_draw_rectangle(0, 10, 30, 370 * sgc.x_scale, 305);

#if DEBUG_NORMALS
	debug_menu_draw_item("VERTEX NORM/TAN/BITAN (RGB)", 0, 0, draw_normal_lines);
#else
	debug_menu_draw_item("VERTEX NORM/TAN/BITAN (RGB)", 0, 1, draw_normal_lines);
#endif
	sng_set_foreground(WHITE);
	debug_menu_draw_item("BILLBOARD WIREFRAME", 1, 0, draw_billboard_wireframe);
	debug_menu_draw_item("POLYGON AS LINE", 2, 0, draw_polygon_as_lines);
	debug_menu_draw_item("NO MSAA", 3, 0, draw_msaa_samples == 0);

	int max_samples = msaa_max_samples();
	debug_menu_draw_item("2x MSAA", 4, max_samples < 2 || !draw_render_to_texture,
				draw_msaa_samples == 2 && draw_render_to_texture);
	debug_menu_draw_item("4x MSAA", 5, max_samples < 4 || !draw_render_to_texture,
				draw_msaa_samples == 4 && draw_render_to_texture);
	debug_menu_draw_item("RENDER TO TEXTURE", 6, 0, draw_render_to_texture);
	debug_menu_draw_item("SMAA", 7, !draw_render_to_texture,
									draw_smaa);
	debug_menu_draw_item("SMAA DEBUG EDGE", 8, !draw_smaa, draw_smaa_edge);
	debug_menu_draw_item("SMAA DEBUG BLEND", 9, !draw_smaa, draw_smaa_blend);
	debug_menu_draw_item("PLANETARY ATMOSPHERES", 10, 0, draw_atmospheres);
	debug_menu_draw_item("PLANET SPECULARITY", 11, 0, graph_dev_planet_specularity);
	debug_menu_draw_item("FILMIC TONEMAPPING", 12, 0, filmic_tonemapping);
	debug_menu_draw_item("CASCADED SHADOW MAPPING", 13, 0, graph_dev_shadow_map_enabled);
	debug_menu_draw_item("PLANETS RECV CSM SHADOWS", 14, 0, graph_dev_planets_receive_csm_shadows);
}

static int selected_debug_item_checkbox(int n, int x, int y, int *toggle)
{
	if (x > 15 && x < 35 && y >= 35 + n * 20 && y <= 50 + n * 20) {
		if (toggle)
			*toggle = !*toggle;
		return 1;
	}
	return 0;
}

int graph_dev_graph_dev_debug_menu_click(int x, int y)
{
#if DEBUG_NORMALS
	if (selected_debug_item_checkbox(0, x, y, &draw_normal_lines))
		return 1;
#endif
	if (selected_debug_item_checkbox(1, x, y, &draw_billboard_wireframe))
		return 1;
	if (selected_debug_item_checkbox(2, x, y, &draw_polygon_as_lines))
		return 1;
	if (selected_debug_item_checkbox(3, x, y, NULL)) {
		draw_msaa_samples = 0;
		return 1;
	}
	if (selected_debug_item_checkbox(4, x, y, NULL)) {
		if (msaa_max_samples() >= 2 && draw_render_to_texture)
			draw_msaa_samples = 2;
		return 1;
	}
	if (selected_debug_item_checkbox(5, x, y, NULL)) {
		if (msaa_max_samples() >= 4 && draw_render_to_texture)
			draw_msaa_samples = 4;
		return 1;
	}
	if (selected_debug_item_checkbox(6, x, y, &draw_render_to_texture)) {
		if (!draw_render_to_texture) {
			/* If render to texture is disabled, disable everything that needs it */
			draw_msaa_samples = 0;
			draw_smaa = 0;
			draw_smaa_edge = 0;
			draw_smaa_blend = 0;
		}
		return 1;
	}
	if (selected_debug_item_checkbox(7, x, y, &draw_smaa))
		if (draw_render_to_texture)
			return 1;
	if (selected_debug_item_checkbox(8, x, y, &draw_smaa_edge)) {
		if (draw_render_to_texture) {
			draw_smaa_blend = 0;
			return 1;
		}
	}
	if (selected_debug_item_checkbox(9, x, y, &draw_smaa_blend)) {
		if (draw_render_to_texture) {
			draw_smaa_edge = 0;
			return 1;
		}
	}
	if (selected_debug_item_checkbox(10, x, y, &draw_atmospheres))
		return 1;
	if (selected_debug_item_checkbox(11, x, y, &graph_dev_planet_specularity))
		return 1;
	if (selected_debug_item_checkbox(12, x, y, &filmic_tonemapping))
		return 1;
	if (selected_debug_item_checkbox(13, x, y, &graph_dev_shadow_map_enabled))
		return 1;
	if (selected_debug_item_checkbox(14, x, y, &graph_dev_planets_receive_csm_shadows))
		return 1;
	return 0;
}

void graph_dev_grab_framebuffer(unsigned char **buffer, int *width, int *height)
{
	*buffer = malloc(4 * sgc.screen_x * sgc.screen_y);
	*width = sgc.screen_x;
	*height = sgc.screen_y;
	glReadPixels(0, 0, sgc.screen_x, sgc.screen_y,
			GL_RGBA, GL_UNSIGNED_BYTE, *buffer);
}

void graph_dev_set_tonemapping_gain(float tmg)
{
	if (tmg >= MIN_TONEMAPPING_GAIN && tmg <= MAX_TONEMAPPING_GAIN)
		tonemapping_gain = tmg;
}

void graph_dev_set_error_texture(const char *error_texture_png)
{
	error_texture_file = strdup(error_texture_png);
}

void graph_dev_set_no_texture_mode()
{
	no_texture_mode = 1;
	if (!error_texture_file) {
		fprintf(stderr, "BUG at %s:%s:%d: error_texture_file is not set, but no_texture_mode set\n",
			__FILE__, __func__, __LINE__);
		fflush(stderr);
	}
}

int graph_dev_texture_ready(int i)
{
	if (i < 0 || i >= MAX_LOADED_TEXTURES)
		return 0;
	if (i == 0)
		return 1;
	pthread_mutex_lock(&finished_loading_mutex);
	int x = texture_finished_loading(i);
	pthread_mutex_unlock(&finished_loading_mutex);
	return x;
}

int graph_dev_textures_ready(int *tids)
{
	pthread_mutex_lock(&finished_loading_mutex);
	for (int i = 0; tids[i] != -1; i++) {
		if (tids[i] == 0)
			continue;
		if (!texture_finished_loading(tids[i])) {
			pthread_mutex_unlock(&finished_loading_mutex);
			return 0;
		}
	}
	pthread_mutex_unlock(&finished_loading_mutex);
	return 1;
}

/* Enqueue request to load a texture.  The texture will be loaded in another thread,
 * and the image data will appear in the loaded_images_wq work queue later on where
 * the main rendering thread can upload it to the GPU
 */
static unsigned int graph_dev_load_texture_helper(const char *filename, int linear_colorspace, int use_mipmaps)
{
	GLuint texture_id;
	int i;

	/* See if we already loaded this texture */
	pthread_mutex_lock(&finished_loading_mutex);
	for (i = 0; i < nloaded_textures; i++) {
		if (strcmp(filename, loaded_textures[i].filename) == 0) {
			loaded_textures[i].expired = 0;
			int tid = (int) loaded_textures[i].texture_id;
			pthread_mutex_unlock(&finished_loading_mutex);
			return tid;
		}
	}

	/* See if we can re-use an expired texture id (not the actual texture_name though) */
	int index = -1;
	for (i = 0; i < nloaded_textures; i++) {
		if (loaded_textures[i].expired) {
			glBindTexture(GL_TEXTURE_2D, 0);
			glDeleteTextures(1, &loaded_textures[i].texture_id);
			fprintf(stderr, "Replacing %s with %s\n", loaded_textures[i].filename, filename);
			if (loaded_textures[i].filename)
				free(loaded_textures[i].filename);
			loaded_textures[i].filename = strdup(filename);
			loaded_textures[i].mtime = get_file_modify_time(filename);
			loaded_textures[i].last_mtime_change = 0;
			loaded_textures[i].expired = 0;
			loaded_textures[i].use_mipmaps = use_mipmaps;
			loaded_textures[i].linear_colorspace = linear_colorspace;
			texture_id = loaded_textures[i].texture_id;
			mark_texture_load_unused(texture_id);
			index = i;
			break;
		}
	}

	graph_dev_gen_texture_no_lock(1, &texture_id);
	mark_texture_load_pending(texture_id);
	pthread_mutex_unlock(&finished_loading_mutex);

	/* Queue up the image load request */
	struct graph_dev_image_load_request *r = calloc(1, sizeof(*r));
	r->texture_id = (int) texture_id;
	r->loaded_texture_index = index;
	r->request_type = GRAPH_DEV_IMAGE_LOAD;
	r->filename[0] = strdup(filename);
	r->flipVertical = 1;
	r->flipHorizontal = 0;
	r->pre_multiply_alpha = 1;
	r->linear_colorspace = linear_colorspace;
	r->use_mipmaps = use_mipmaps;

	enqueue_image_load_request(r);
	return texture_id;
}

unsigned int graph_dev_load_texture(const char *filename, int linear_colorspace)
{
	return graph_dev_load_texture_helper(filename, linear_colorspace, 1);
}

unsigned int graph_dev_load_texture_no_mipmaps(const char *filename, int linear_colorspace)
{
	return graph_dev_load_texture_helper(filename, linear_colorspace, 0);
}

/* Call this early on to wipe out garbage otherwise left in the window */
void graph_dev_clear_window(void)
{
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT);
}

void graph_dev_prepare_for_window(uint32_t *window_flags)
{
	SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 5);
	SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 5);
	SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 5);
	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
	SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 0);
	SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 0);
	SDL_GL_SetAttribute(SDL_GL_FRAMEBUFFER_SRGB_CAPABLE, 1);

	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
	/* allow context upgrading (macOS, etc) */
	/* SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG); */

	*window_flags = *window_flags | SDL_WINDOW_OPENGL;
}

void graph_dev_create_context(SDL_Window *window)
{
	SDL_GLContext gl_context = SDL_GL_CreateContext(window);
	if (NULL == gl_context) {
		fprintf(stderr, "Couldn't create OpenGL Context: %s\n", SDL_GetError());
		exit(1);
	}
	(void) gl_context;
}

void graph_dev_shadow_map(int new_status)
{
	graph_dev_shadow_map_enabled = !!new_status;
}


int graph_dev_shadow_map_status(void)
{
	return graph_dev_shadow_map_enabled;
}

