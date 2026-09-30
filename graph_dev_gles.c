#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <assert.h>
#include <sys/stat.h>
#include <limits.h>
#include <pthread.h>

#include <glad/gles2.h>
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
#include "graph_dev/texture_cache.h"
#include "graph_dev/mesh_cache.h"
#include "graph_dev/context.h"
#include "graph_dev/shader_setup.h"

/* Vertex array objects are an OES extension here, not core, so every use is conditional.  The
 * three graph_dev_*_vao() hooks below are the only places that should test this. */
#define GLES_HAS_VAO		(GLAD_GL_OES_vertex_array_object)
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




#define DEBUG_NORMALS 0










static int draw_normal_lines = 0;
static int draw_billboard_wireframe = 0;
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
int graph_dev_shadow_map_enabled = 0; /* Shadow maps are not supported on the GLES backend. */

static GLenum fbo_format = GL_RGBA4;


















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
static struct graph_dev_gl_atmosphere_shader atmosphere_shader;
static struct graph_dev_gl_atmosphere_shader atmosphere_with_annulus_shadow_shader;
static struct graph_dev_gl_trans_wireframe_shader trans_wireframe_shader;
static struct graph_dev_gl_trans_wireframe_shader trans_wireframe_with_clip_sphere_shader;
static struct graph_dev_gl_filled_wireframe_shader filled_wireframe_shader;
static struct graph_dev_gl_single_color_shader single_color_shader;
static struct graph_dev_gl_line_single_color_shader line_single_color_shader;
static struct graph_dev_gl_vertex_color_shader vertex_color_shader;
static struct graph_dev_gl_point_cloud_shader point_cloud_shader;
static struct graph_dev_gl_sun_shader sun_shader;
static struct graph_dev_gl_black_hole_shader black_hole_shader;
static struct graph_dev_gl_exhaust_plume_shader exhaust_plume_shader;
static struct graph_dev_gl_skybox_shader skybox_shader;

/* Gravitational lenses bending the skybox, packed the way the shader's uniform arrays want
 * them.  Active lenses are packed first and the remaining slots keep a zero Einstein radius,
 * which the shader treats as contributing nothing -- see graph_dev_set_gravitational_lenses()
 * below and share/snis/shader-es/skybox.frag. */
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

/* City shader: GLES never supports CSM; ring shadow support is deferred */
static struct graph_dev_gl_textured_shader city_shader_no_csm_no_ring;
static struct graph_dev_gl_textured_shader textured_lit_shader;
static struct graph_dev_gl_textured_shader textured_lit_emit_shader;
static struct graph_dev_gl_textured_shader textured_lit_emit_normal_shader;
static struct graph_dev_gl_textured_shader textured_lit_normal_shader;
static struct graph_dev_gl_textured_shader textured_cubemap_lit_shader;
static struct graph_dev_gl_textured_shader textured_cubemap_lit_normal_map_shader;
static struct graph_dev_gl_textured_shader textured_cubemap_shield_shader;
static struct graph_dev_gl_textured_shader textured_cubemap_lit_with_annulus_shadow_shader;
static struct graph_dev_gl_textured_shader textured_cubemap_normal_mapped_lit_with_annulus_shadow_shader;
static struct graph_dev_gl_textured_shader textured_cubemap_normal_mapped_lit_with_annulus_shadow_specular_shader;
static struct graph_dev_gl_textured_shader textured_cubemap_normal_mapped_lit_specular_shader;
static struct graph_dev_gl_textured_particle_shader textured_particle_shader;
static struct graph_dev_gl_textured_shader alpha_by_normal_shader;
static struct graph_dev_gl_textured_shader textured_alpha_by_normal_shader;
static struct graph_dev_gl_fs_effect_shader fs_copy_shader;
static struct graph_dev_smaa_effect smaa_effect;

static struct fbo_target msaa = { 0 };
static struct fbo_target post_target0 = { 0 };
static struct fbo_target post_target1 = { 0 };
static struct fbo_target render_target_2d = { 0 };


static struct graph_dev_primitive cubemap_cube;
static struct graph_dev_primitive textured_unit_quad;




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

	case GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS:
		printf("FBO Incomplete Dimensions.\n");
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
			if (GLAD_GL_EXT_texture_storage) {
				glTexStorage2DEXT(GL_TEXTURE_2D, 1, fbo_format, sgc.screen_x, sgc.screen_y);
			} else {
				glTexImage2D(GL_TEXTURE_2D, 0, fbo_format,
					sgc.screen_x, sgc.screen_y, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
			}
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
				target->color0_texture, 0);
		}

		if (target->depth_buffer > 0) {
			glBindRenderbuffer(GL_RENDERBUFFER, target->depth_buffer);
			glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT16, sgc.screen_x,
				sgc.screen_y);
			glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER,
				target->depth_buffer);
		}

		GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
		if (status != GL_FRAMEBUFFER_COMPLETE) {
			print_framebuffer_error();
		}

		target->width = sgc.screen_x;
		target->height = sgc.screen_y;
	}

	PROFILE_ZONE_END();
}

/* if the GL driver supports it, discard the contents of the FBO to free up memory.
 *
 * This reliably messes up the currently bound FBO (to prevent incidental bugs from
 * not anticipating the potential change), so you *must* rebind it after unless
 * you're going to render into the freshly invalidated FBO immediately.
 */
static void maybe_discard_fbo_contents(struct fbo_target *fbo)
{
	static const GLenum attachments[2] = {
		GL_COLOR_ATTACHMENT0,
		GL_DEPTH_ATTACHMENT,
	};
	glBindFramebuffer(GL_FRAMEBUFFER, fbo->fbo);
	if (GLAD_GL_EXT_discard_framebuffer) {
		glDiscardFramebufferEXT(GL_FRAMEBUFFER, (fbo->depth_buffer != 0) ? 2 : 1, attachments);
	}
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
			glBindFramebuffer(GL_FRAMEBUFFER, 0);
			sgc.fbo_current = 0;
		}

		sgc.active_vp = 1;
	}
	PROFILE_ZONE_END();
}

void enable_3d_viewport(void)
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
			glBindFramebuffer(GL_FRAMEBUFFER, 0);
			sgc.fbo_current = 0;
		}

		sgc.active_vp = 2;
	}
	PROFILE_ZONE_END();
}


void draw_vertex_buffer_2d(void)
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

		if (!GLES_HAS_VAO) {
			glDisableVertexAttribArray(vertex_color_shader.vertex_position_id);
			glDisableVertexAttribArray(vertex_color_shader.vertex_color_id);
		}

		/* orphan this buffer so we don't get blocked on these draw commands */
		glBufferData(GL_ARRAY_BUFFER, VERTEX_BUFFER_2D_SIZE, 0, GL_STREAM_DRAW);
	}
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

	if (!GLES_HAS_VAO) {
		glDisableVertexAttribArray(single_color_shader.vertex_position_id);
	}

	glDisable(GL_DEPTH_TEST);
}
#endif

struct raster_texture_params {
	struct graph_dev_gl_textured_shader *shader;
	const struct mat44 *mat_mvp;			/* model view projection matrix */
	const struct mat44 *mat_mv;			/* model view matrix */
	const struct mat33 *mat_normal;			/* Used to get normal vectors into eye space */
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

	if (!GLES_HAS_VAO) {
		glDisableVertexAttribArray(shader->vertex_position_id);
		if (shader->vertex_normal_id >= 0)
			glDisableVertexAttribArray(shader->vertex_normal_id);
		if (shader->vertex_tangent_id >= 0)
			glDisableVertexAttribArray(shader->vertex_tangent_id);
		if (shader->vertex_bitangent_id >= 0)
			glDisableVertexAttribArray(shader->vertex_bitangent_id);
		if (shader->texture_coord_id >= 0)
			glDisableVertexAttribArray(shader->texture_coord_id);
	}

	glDisable(GL_DEPTH_TEST);
	if (p->do_cullface)
		glDisable(GL_CULL_FACE);
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
	const struct mat33 *mat_normal, struct mesh *m, struct sng_color *triangle_color, union vec3 *eye_light_pos,
	float in_shade, float ambient, const float light_color[3], const float ambient_color[3])
{
	PROFILE_ZONE_START("graph_dev_raster_single_color_lit");
	enable_3d_viewport();

	if (!m->graph_ptr) {
		PROFILE_ZONE_END();
		return;
	}

	struct mesh_gl_info *ptr = m->graph_ptr;

	glEnable(GL_DEPTH_TEST);
	glEnable(GL_CULL_FACE);
	activate_shader(&single_color_lit_shader);

	glUniformMatrix4fv(single_color_lit_shader.mv_matrix_id, 1, GL_FALSE, &mat_mv->m[0][0]);
	glUniformMatrix4fv(single_color_lit_shader.mvp_matrix_id, 1, GL_FALSE, &mat_mvp->m[0][0]);
	glUniformMatrix3fv(single_color_lit_shader.normal_matrix_id, 1, GL_FALSE, &mat_normal->m[0][0]);

	glUniform3f(single_color_lit_shader.color_id, triangle_color->red,
		triangle_color->green, triangle_color->blue);
	glUniform3f(single_color_lit_shader.light_pos_id, eye_light_pos->v.x, eye_light_pos->v.y, eye_light_pos->v.z);
	glUniform1f(single_color_lit_shader.in_shade_id, in_shade);
	glUniform1f(single_color_lit_shader.ambient_id, ambient);
	if (single_color_lit_shader.light_color_id >= 0)
		glUniform3f(single_color_lit_shader.light_color_id,
			light_color[0], light_color[1], light_color[2]);
	if (single_color_lit_shader.ambient_color_id >= 0)
		glUniform3f(single_color_lit_shader.ambient_color_id,
			ambient_color[0], ambient_color[1], ambient_color[2]);
	glUniform1f(single_color_lit_shader.filmic_tonemapping_id, (float) filmic_tonemapping);
	glUniform1f(single_color_lit_shader.tonemapping_gain_id, tonemapping_gain);

	glEnableVertexAttribArray(single_color_lit_shader.vertex_position_id);
	glBindBuffer(GL_ARRAY_BUFFER, ptr->vertex_buffer);
	glVertexAttribPointer(
		single_color_lit_shader.vertex_position_id, /* The attribute we want to configure */
		3,                           /* size */
		GL_FLOAT,                    /* type */
		GL_FALSE,                    /* normalized? */
		sizeof(struct vertex_buffer_data), /* stride */
		(void *)offsetof(struct vertex_buffer_data, position.v.x) /* array buffer offset */
	);

	glEnableVertexAttribArray(single_color_lit_shader.vertex_normal_id);
	glBindBuffer(GL_ARRAY_BUFFER, ptr->triangle_vertex_buffer);
	glVertexAttribPointer(
		single_color_lit_shader.vertex_normal_id,  /* The attribute we want to configure */
		3,                            /* size */
		GL_FLOAT,                     /* type */
		GL_FALSE,                     /* normalized? */
		sizeof(struct vertex_triangle_buffer_data), /* stride */
		(void *)offsetof(struct vertex_triangle_buffer_data, normal.v.x) /* array buffer offset */
	);

	glDrawArrays(GL_TRIANGLES, 0, m->ntriangles * 3);

	if (!GLES_HAS_VAO) {
		glDisableVertexAttribArray(single_color_lit_shader.vertex_position_id);
		glDisableVertexAttribArray(single_color_lit_shader.vertex_normal_id);
	}
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_CULL_FACE);

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

	if (!GLES_HAS_VAO) {
		glDisableVertexAttribArray(shader->vertex_position_id);
		glDisableVertexAttribArray(shader->vertex_normal_id);
	}

	glDisable(GL_DEPTH_TEST);
	glDisable(GL_CULL_FACE);
	glDepthMask(GL_TRUE);

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

	if (!GLES_HAS_VAO) {
		glDisableVertexAttribArray(filled_wireframe_shader.position_id);
		glDisableVertexAttribArray(filled_wireframe_shader.tvertex0_id);
		glDisableVertexAttribArray(filled_wireframe_shader.tvertex1_id);
		glDisableVertexAttribArray(filled_wireframe_shader.tvertex2_id);
		glDisableVertexAttribArray(filled_wireframe_shader.edge_mask_id);
	}

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
			(void *)offsetof(struct vertex_wireframe_line_buffer_data,
				position.v.x) /* array buffer offset */
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
			(void *)offsetof(struct vertex_wireframe_line_buffer_data,
				position.v.x) /* array buffer offset */
		);
	}

	glDrawArrays(GL_LINES, 0, ptr->nwireframe_lines * 2);

	if (!GLES_HAS_VAO) {
		if (do_cullface) {
			glDisableVertexAttribArray(shader->vertex_position_id);
			glDisableVertexAttribArray(shader->vertex_normal_id);
		} else {
			glDisableVertexAttribArray(single_color_shader.vertex_position_id);
		}
	}

	glDisable(GL_DEPTH_TEST);

#if DEBUG_NORMALS
	if (draw_normal_lines) {
		graph_dev_draw_normal_lines(mat_mvp, m, ptr);
	}
#endif

	PROFILE_ZONE_END();
}

void graph_dev_raster_line_mesh(struct entity *e, const struct mat44 *mat_mvp, struct mesh *m,
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
			(void *)offsetof(struct vertex_line_buffer_data, line_vertex1.v.x));

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
	if (!GLES_HAS_VAO) {
		glDisableVertexAttribArray(vertex_position_id);
		if (e->material_ptr && e->material_ptr->type != MATERIAL_COLOR_BY_W) {
			glDisableVertexAttribArray(line_single_color_shader.multi_one_id);
			glDisableVertexAttribArray(line_single_color_shader.line_vertex0_id);
			glDisableVertexAttribArray(line_single_color_shader.line_vertex1_id);
		}
	}
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
	/* glEnable(GL_VERTEX_PROGRAM_POINT_SIZE); */

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

	if (!GLES_HAS_VAO) {
		glDisableVertexAttribArray(shader->vertex_position_id);
	}

	glDisable(GL_DEPTH_TEST);
	/* glDisable(GL_VERTEX_PROGRAM_POINT_SIZE); */
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

	if (!GLES_HAS_VAO) {
		glDisableVertexAttribArray(textured_particle_shader.multi_one_id);
		glDisableVertexAttribArray(textured_particle_shader.start_position_id);
		glDisableVertexAttribArray(textured_particle_shader.start_tint_color_id);
		glDisableVertexAttribArray(textured_particle_shader.start_apm_id);
		glDisableVertexAttribArray(textured_particle_shader.end_position_id);
		glDisableVertexAttribArray(textured_particle_shader.end_tint_color_id);
		glDisableVertexAttribArray(textured_particle_shader.end_apm_id);
	}

	glDisable(GL_DEPTH_TEST);
	glDepthMask(GL_TRUE);
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
	BLEND_FUNC(GL_ONE, GL_ONE_MINUS_SRC_ALPHA); /* premultiplied; see sun.frag */

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
 * the black hole shader.  The disc radius (in UV) comes from the material and the caller sets it
 * per frame, so the disc stays world-scale while the billboard is sized a little larger to leave
 * the rim glow somewhere to go. */
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

static void graph_dev_raster_exhaust_plume(const struct mat44 *mat_mvp, const struct mat44 *mat_mv,
					struct mesh *m, struct material *material)
{
	struct mesh_gl_info *ptr = m->graph_ptr;
	struct material_exhaust_plume *plume = &material->exhaust_plume;

	if (!ptr)
		return;

	glEnable(GL_DEPTH_TEST);
	glDepthMask(GL_FALSE); /* blended: test against depth but do not write it */
	glEnable(GL_BLEND);
	BLEND_FUNC(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
	glDisable(GL_CULL_FACE);

	activate_shader(&exhaust_plume_shader);

	glUniformMatrix4fv(exhaust_plume_shader.mvp_matrix_id, 1, GL_FALSE, &mat_mvp->m[0][0]);
	if (exhaust_plume_shader.mv_matrix_id >= 0)
		glUniformMatrix4fv(exhaust_plume_shader.mv_matrix_id, 1, GL_FALSE, &mat_mv->m[0][0]);
	glUniform3f(exhaust_plume_shader.tint_color_id, plume->tint.red, plume->tint.green, plume->tint.blue);
	glUniform1f(exhaust_plume_shader.core_brightness_id, plume->core_brightness);
	glUniform1f(exhaust_plume_shader.plume_length_id, plume->plume_length);
	glUniform1f(exhaust_plume_shader.noise_seed_id, plume->noise_seed);
	glUniform1f(exhaust_plume_shader.diamond_spacing_id, plume->shock_diamond_spacing);
	glUniform1f(exhaust_plume_shader.diamond_intensity_id, plume->shock_diamond_intensity);
	glUniform1f(exhaust_plume_shader.filmic_tonemapping_id, (float) filmic_tonemapping);
	glUniform1f(exhaust_plume_shader.tonemapping_gain_id, tonemapping_gain);

	glEnableVertexAttribArray(exhaust_plume_shader.vertex_position_id);
	glBindBuffer(GL_ARRAY_BUFFER, ptr->vertex_buffer);
	glVertexAttribPointer(exhaust_plume_shader.vertex_position_id, 3, GL_FLOAT, GL_FALSE,
		sizeof(struct vertex_buffer_data),
		(void *) offsetof(struct vertex_buffer_data, position.v.x));

	if (exhaust_plume_shader.vertex_normal_id >= 0) {
		glEnableVertexAttribArray(exhaust_plume_shader.vertex_normal_id);
		glBindBuffer(GL_ARRAY_BUFFER, ptr->triangle_vertex_buffer);
		glVertexAttribPointer(exhaust_plume_shader.vertex_normal_id, 3, GL_FLOAT, GL_FALSE,
			sizeof(struct vertex_triangle_buffer_data),
			(void *) offsetof(struct vertex_triangle_buffer_data, normal.v.x));
	}

	if (exhaust_plume_shader.texture_coord_id >= 0) {
		glEnableVertexAttribArray(exhaust_plume_shader.texture_coord_id);
		glBindBuffer(GL_ARRAY_BUFFER, ptr->triangle_vertex_buffer);
		glVertexAttribPointer(exhaust_plume_shader.texture_coord_id, 2, GL_FLOAT, GL_TRUE,
			sizeof(struct vertex_triangle_buffer_data),
			(void *) offsetof(struct vertex_triangle_buffer_data, texture_coord.v.x));
	}

	glDrawArrays(GL_TRIANGLES, 0, m->ntriangles * 3);

	glDisableVertexAttribArray(exhaust_plume_shader.vertex_position_id);
	if (exhaust_plume_shader.vertex_normal_id >= 0)
		glDisableVertexAttribArray(exhaust_plume_shader.vertex_normal_id);
	if (exhaust_plume_shader.texture_coord_id >= 0)
		glDisableVertexAttribArray(exhaust_plume_shader.texture_coord_id);

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


/* A SHIP'S DEATH on GLES: the fireball, the shrapnel, the wreck and the particle batch, as
 * graph_dev_opengl.c draws them, tuned down for the smaller GPUs GLES runs on.
 *
 *  - The fireball is marched at a quarter of the width and height by default, a sixteenth of
 *    the pixels, and at most 16 steps (EXPLOSION_MAX_STEPS in shader-es/explosion.shader).
 *
 *  - GLES2 cannot read the depth buffer back, so the fireball's march does not stop at a
 *    solid thing inside it: see graph_dev_capture_scene_depth() below.
 *
 * Everything else -- the pieces, the shards, the smoke, flame and sparks -- is drawn as on the
 * desktop, from the same shaders but for the GLSL version. */
static struct graph_dev_gl_shrapnel_shader shrapnel_shader;
static struct graph_dev_gl_wreck_shader wreck_shader;
static struct graph_dev_gl_particles_shader particles_shader;
static struct graph_dev_gl_explosion_shader explosion_shader;
static struct graph_dev_gl_volume_composite_shader volume_composite_shader;
static GLint volume_composite_corner_id = -1;
static GLuint volume_composite_corners;

static GLuint blackbody_lut;
static int volume_downsample = 4;
static GLuint volume_fbo, volume_color;
static int volume_w, volume_h;

void graph_dev_set_volume_downsample(int n)
{
	volume_downsample = n < 1 ? 1 : n;
}

int graph_dev_draws_ship_death(void)
{
	return 1;
}

/* See graph_dev.h: GLES2 cannot read the depth buffer back, so the fireball does without. */
void graph_dev_capture_scene_depth(float near, float far)
{
	(void) near;
	(void) far;
}

/* The blackbody ramp, as graph_dev_opengl.c's bake_blackbody_lut() makes it -- the two ends MUST
 * agree with BLACKBODY_MIN_K and BLACKBODY_MAX_K in the shaders -- but GL_RGB, since GLES2 has
 * no sized internal formats. */
#define BLACKBODY_LUT_TEXELS 256
#define BLACKBODY_LUT_MIN_K 1900.0f
#define BLACKBODY_LUT_MAX_K 40000.0f

static void bake_blackbody_lut(void)
{
	unsigned char texel[BLACKBODY_LUT_TEXELS * 3];
	int i;

	if (blackbody_lut)
		return;
	for (i = 0; i < BLACKBODY_LUT_TEXELS; i++) {
		float kelvin = BLACKBODY_LUT_MIN_K + (BLACKBODY_LUT_MAX_K - BLACKBODY_LUT_MIN_K) *
				((float) i / (float) (BLACKBODY_LUT_TEXELS - 1));
		float r, g, b;

		star_light_blackbody_color(kelvin, &r, &g, &b);
		texel[i * 3 + 0] = (unsigned char) (r * 255.0f + 0.5f);
		texel[i * 3 + 1] = (unsigned char) (g * 255.0f + 0.5f);
		texel[i * 3 + 2] = (unsigned char) (b * 255.0f + 0.5f);
	}
	glGenTextures(1, &blackbody_lut);
	glBindTexture(GL_TEXTURE_2D, blackbody_lut);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, BLACKBODY_LUT_TEXELS, 1, 0, GL_RGB,
			GL_UNSIGNED_BYTE, texel);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glBindTexture(GL_TEXTURE_2D, 0);
	/* The binding cache no longer knows what unit 0 holds. */
	sgc.texture_unit_bind[0] = 0;
}

static void setup_ship_death_shaders(void)
{
	/* GLSL 100 has no gl_VertexID: the composite's one triangle comes from a buffer. */
	static const GLfloat corner[] = { -1.0f, -1.0f, 3.0f, -1.0f, -1.0f, 3.0f };

	setup_shrapnel_shader(&shrapnel_shader);
	setup_wreck_shader(&wreck_shader);
	setup_particles_shader(&particles_shader);
	setup_explosion_shader(&explosion_shader);
	setup_volume_composite_shader(&volume_composite_shader);
	volume_composite_corner_id = glGetAttribLocation(volume_composite_shader.program_id,
							"a_Corner");
	if (!volume_composite_corners) {
		glGenBuffers(1, &volume_composite_corners);
		glBindBuffer(GL_ARRAY_BUFFER, volume_composite_corners);
		glBufferData(GL_ARRAY_BUFFER, sizeof(corner), corner, GL_STATIC_DRAW);
		glBindBuffer(GL_ARRAY_BUFFER, 0);
	}
	bake_blackbody_lut();
}

/* The star's light as the ship's death's shaders take it: in world space, its
 * tinted colour, and the ambient. */
struct ship_death_light {
	float pos[3];
	float tint[3];
	float ambient;
};

/* Point a shader's attribute at a column of a mesh's buffers: position from the vertex buffer,
 * everything else from the per-triangle-vertex one.  -1, for an attribute the shader does not
 * use, is skipped. */
static void ship_death_attribute(GLint id, struct mesh_gl_info *ptr, int size, size_t offset,
				int from_triangle_buffer)
{
	if (id < 0)
		return;
	glEnableVertexAttribArray(id);
	if (from_triangle_buffer) {
		glBindBuffer(GL_ARRAY_BUFFER, ptr->triangle_vertex_buffer);
		glVertexAttribPointer(id, size, GL_FLOAT, GL_FALSE,
			sizeof(struct vertex_triangle_buffer_data), (void *) offset);
	} else {
		glBindBuffer(GL_ARRAY_BUFFER, ptr->vertex_buffer);
		glVertexAttribPointer(id, size, GL_FLOAT, GL_FALSE,
			sizeof(struct vertex_buffer_data), (void *) offset);
	}
}

static void ship_death_attribute_off(GLint id)
{
	if (id >= 0)
		glDisableVertexAttribArray(id);
}

static int ship_death_shader_ok(GLuint program_id)
{
	return program_id != 0 && (GLint) program_id != -1;
}

/* One shard of shrapnel, or an ember off a burning chunk.  Opaque and depth written. */
static void graph_dev_raster_shrapnel(const struct mat44 *mat_mvp, const struct mat44 *model,
				struct mesh *m, struct material *material,
				const struct ship_death_light *light)
{
	struct graph_dev_gl_shrapnel_shader *sh = &shrapnel_shader;
	struct mesh_gl_info *ptr = m->graph_ptr;
	struct material_shrapnel *mt = &material->shrapnel;

	if (!ptr || !ship_death_shader_ok(sh->program_id))
		return;
	enable_3d_viewport();
	glEnable(GL_DEPTH_TEST);
	glDepthMask(GL_TRUE);
	glDisable(GL_BLEND);
	glEnable(GL_CULL_FACE);
	activate_shader(sh);
	glUniformMatrix4fv(sh->mvp_matrix_id, 1, GL_FALSE, &mat_mvp->m[0][0]);
	glUniformMatrix4fv(sh->model_matrix_id, 1, GL_FALSE, &model->m[0][0]);
	glUniform3f(sh->light_pos_id, light->pos[0], light->pos[1], light->pos[2]);
	glUniform3f(sh->star_tint_id, light->tint[0], light->tint[1], light->tint[2]);
	glUniform1f(sh->ambient_id, light->ambient);
	glUniform1f(sh->temperature_id, mt->temperature);
	glUniform1f(sh->brightness_id, mt->brightness);
	glUniform1f(sh->albedo_id, mt->albedo);
	glUniform1f(sh->filmic_tonemapping_id, (float) filmic_tonemapping);
	glUniform1f(sh->tonemapping_gain_id, tonemapping_gain);
	BIND_TEXTURE(GL_TEXTURE0, GL_TEXTURE_2D, blackbody_lut);
	glUniform1i(sh->blackbody_id, 0);

	ship_death_attribute(sh->vertex_position_id, ptr, 3,
			offsetof(struct vertex_buffer_data, position.v.x), 0);
	ship_death_attribute(sh->vertex_normal_id, ptr, 3,
			offsetof(struct vertex_triangle_buffer_data, normal.v.x), 1);
	glDrawArrays(GL_TRIANGLES, 0, m->ntriangles * 3);
	ship_death_attribute_off(sh->vertex_position_id);
	ship_death_attribute_off(sh->vertex_normal_id);
}

/* One piece of a broken ship, drawn two-sided.  Opaque and depth written. */
static void graph_dev_raster_wreck(const struct mat44 *mat_mvp, const struct mat44 *model,
				struct mesh *m, struct material *material,
				const struct ship_death_light *light)
{
	struct graph_dev_gl_wreck_shader *sh = &wreck_shader;
	struct mesh_gl_info *ptr = m->graph_ptr;
	struct material_wreck *mt = &material->wreck;
	int have_texture = mt->texture_id > 0 && graph_dev_texture_ready(mt->texture_id);

	if (!ptr || !ship_death_shader_ok(sh->program_id))
		return;
	enable_3d_viewport();
	glEnable(GL_DEPTH_TEST);
	glDepthMask(GL_TRUE);
	glDisable(GL_BLEND);
	glDisable(GL_CULL_FACE);	/* two-sided: the inside shows through the tear */
	activate_shader(sh);
	glUniformMatrix4fv(sh->mvp_matrix_id, 1, GL_FALSE, &mat_mvp->m[0][0]);
	glUniformMatrix4fv(sh->model_matrix_id, 1, GL_FALSE, &model->m[0][0]);
	glUniform3f(sh->light_pos_id, light->pos[0], light->pos[1], light->pos[2]);
	glUniform3f(sh->star_tint_id, light->tint[0], light->tint[1], light->tint[2]);
	glUniform1f(sh->ambient_id, light->ambient);
	glUniform3f(sh->aux_light_pos_id, mt->aux_light_pos[0], mt->aux_light_pos[1],
			mt->aux_light_pos[2]);
	glUniform3f(sh->aux_light_color_id, mt->aux_light_color[0], mt->aux_light_color[1],
			mt->aux_light_color[2]);
	glUniform1f(sh->aux_light_wrap_id, mt->aux_light_wrap);
	glUniform1f(sh->interior_id, mt->interior);
	glUniform1f(sh->scorch_id, mt->scorch);
	glUniform1f(sh->edge_width_id, mt->edge_width);
	glUniform1f(sh->edge_temp_id, mt->edge_temp);
	glUniform1f(sh->edge_brightness_id, mt->edge_brightness);
	glUniform1f(sh->hull_radius_id, mt->hull_radius);
	glUniform1f(sh->dissolve_id, mt->dissolve);
	glUniform1f(sh->burn_glow_id, mt->burn_glow);
	glUniform1f(sh->preheat_id, mt->preheat);
	glUniform1f(sh->time_id, mt->time);
	glUniform1f(sh->filmic_tonemapping_id, (float) filmic_tonemapping);
	glUniform1f(sh->tonemapping_gain_id, tonemapping_gain);
	if (have_texture)
		BIND_TEXTURE(GL_TEXTURE0, GL_TEXTURE_2D, (GLuint) mt->texture_id);
	glUniform1i(sh->albedo_id, 0);
	glUniform1i(sh->have_texture_id, have_texture);
	BIND_TEXTURE(GL_TEXTURE1, GL_TEXTURE_2D, blackbody_lut);
	glUniform1i(sh->blackbody_id, 1);

	ship_death_attribute(sh->vertex_position_id, ptr, 3,
			offsetof(struct vertex_buffer_data, position.v.x), 0);
	ship_death_attribute(sh->vertex_normal_id, ptr, 3,
			offsetof(struct vertex_triangle_buffer_data, normal.v.x), 1);
	ship_death_attribute(sh->texture_coord_id, ptr, 2,
			offsetof(struct vertex_triangle_buffer_data, texture_coord.v.x), 1);
	ship_death_attribute(sh->edge_id, ptr, 1, offsetof(struct vertex_triangle_buffer_data, w), 1);
	glDrawArrays(GL_TRIANGLES, 0, m->ntriangles * 3);
	ship_death_attribute_off(sh->vertex_position_id);
	ship_death_attribute_off(sh->vertex_normal_id);
	ship_death_attribute_off(sh->texture_coord_id);
	ship_death_attribute_off(sh->edge_id);
	glEnable(GL_CULL_FACE);
}

/* A batch of smoke, flame and sparks, already in world space and facing the camera.
 * Premultiplied, depth tested but not written, unculled. */
static void graph_dev_raster_particles(const struct mat44 *mat_mvp, const struct mat44d *view,
				struct mesh *m, struct material *material,
				const struct ship_death_light *light)
{
	struct graph_dev_gl_particles_shader *sh = &particles_shader;
	struct mesh_gl_info *ptr = m->graph_ptr;

	if (!ptr || m->ntriangles == 0 || !ship_death_shader_ok(sh->program_id))
		return;
	enable_3d_viewport();
	glEnable(GL_DEPTH_TEST);
	glDepthMask(GL_FALSE);
	glEnable(GL_BLEND);
	BLEND_FUNC(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
	glDisable(GL_CULL_FACE);
	activate_shader(sh);
	glUniformMatrix4fv(sh->mvp_matrix_id, 1, GL_FALSE, &mat_mvp->m[0][0]);
	glUniform3f(sh->cam_right_id, view->m[0][0], view->m[1][0], view->m[2][0]);
	glUniform3f(sh->cam_up_id, view->m[0][1], view->m[1][1], view->m[2][1]);
	glUniform3f(sh->cam_back_id, view->m[0][2], view->m[1][2], view->m[2][2]);
	glUniform3f(sh->light_pos_id, light->pos[0], light->pos[1], light->pos[2]);
	glUniform3f(sh->star_tint_id, light->tint[0], light->tint[1], light->tint[2]);
	glUniform1f(sh->ambient_id, light->ambient);
	glUniform1f(sh->albedo_id, material->particles.albedo);
	glUniform1f(sh->time_id, material->particles.time);
	glUniform1f(sh->filmic_tonemapping_id, (float) filmic_tonemapping);
	glUniform1f(sh->tonemapping_gain_id, tonemapping_gain);
	BIND_TEXTURE(GL_TEXTURE0, GL_TEXTURE_2D, blackbody_lut);
	glUniform1i(sh->blackbody_id, 0);

	ship_death_attribute(sh->vertex_position_id, ptr, 3,
			offsetof(struct vertex_buffer_data, position.v.x), 0);
	ship_death_attribute(sh->vertex_normal_id, ptr, 3,
			offsetof(struct vertex_triangle_buffer_data, normal.v.x), 1);
	ship_death_attribute(sh->texture_coord_id, ptr, 2,
			offsetof(struct vertex_triangle_buffer_data, texture_coord.v.x), 1);
	ship_death_attribute(sh->edge_id, ptr, 1, offsetof(struct vertex_triangle_buffer_data, w), 1);
	glDrawArrays(GL_TRIANGLES, 0, m->ntriangles * 3);
	ship_death_attribute_off(sh->vertex_position_id);
	ship_death_attribute_off(sh->vertex_normal_id);
	ship_death_attribute_off(sh->texture_coord_id);
	ship_death_attribute_off(sh->edge_id);
	glDepthMask(GL_TRUE);
	glDisable(GL_BLEND);
	glEnable(GL_CULL_FACE);
}

/* Make volume_fbo a colour target of w by h, remaking it only when the size changes.  Returns 0
 * if it could not be made, and the caller then draws at full size. */
static int volume_target(int w, int h)
{
	if (volume_fbo && volume_w == w && volume_h == h)
		return 1;
	if (!volume_fbo) {
		glGenFramebuffers(1, &volume_fbo);
		glGenTextures(1, &volume_color);
	}
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, volume_color);
	sgc.texture_unit_active = 0;
	sgc.texture_unit_bind[0] = volume_color;
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glBindFramebuffer(GL_FRAMEBUFFER, volume_fbo);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, volume_color, 0);
	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
		fprintf(stderr, "graph_dev: volume target incomplete; drawing the fireball full size\n");
		glBindFramebuffer(GL_FRAMEBUFFER, sgc.fbo_current);
		glDeleteFramebuffers(1, &volume_fbo);
		volume_fbo = 0;
		return 0;
	}
	glBindFramebuffer(GL_FRAMEBUFFER, sgc.fbo_current);
	volume_w = w;
	volume_h = h;
	return 1;
}

/* The fireball, raymarched inside a camera facing billboard, premultiplied, and at a
 * volume_downsample above 1 drawn into the reduced target and laid back over the window. */
static void graph_dev_raster_explosion(struct entity_context *cx, const struct mat44 *mat_mvp,
				const struct mat44 *model, struct mesh *m, struct material *material,
				const struct ship_death_light *light)
{
	struct graph_dev_gl_explosion_shader *sh = &explosion_shader;
	struct graph_dev_gl_volume_composite_shader *comp = &volume_composite_shader;
	struct mesh_gl_info *ptr = m->graph_ptr;
	struct material_explosion *mt = &material->explosion;
	int n = volume_downsample, reduced = 0;
	GLuint frame_fbo;

	if (!ptr || !ship_death_shader_ok(sh->program_id))
		return;
	enable_3d_viewport();
	frame_fbo = sgc.fbo_current;
	if (n > 1 && ship_death_shader_ok(comp->program_id) && volume_composite_corner_id >= 0)
		reduced = volume_target((sgc.screen_x + n - 1) / n, (sgc.screen_y + n - 1) / n);
	if (reduced) {
		glBindFramebuffer(GL_FRAMEBUFFER, volume_fbo);
		glViewport(sgc.vp_x_3d / n, sgc.vp_y_3d / n, sgc.vp_width_3d / n, sgc.vp_height_3d / n);
		glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
		glClear(GL_COLOR_BUFFER_BIT);
	}
	glDisable(GL_DEPTH_TEST);
	glDepthMask(GL_FALSE);
	glEnable(GL_BLEND);
	BLEND_FUNC(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
	glDisable(GL_CULL_FACE);
	activate_shader(sh);
	glUniformMatrix4fv(sh->mvp_matrix_id, 1, GL_FALSE, &mat_mvp->m[0][0]);
	glUniformMatrix4fv(sh->model_matrix_id, 1, GL_FALSE, &model->m[0][0]);
	glUniform3f(sh->eye_pos_id, cx->camera.x, cx->camera.y, cx->camera.z);
	glUniform3f(sh->light_pos_id, light->pos[0], light->pos[1], light->pos[2]);
	glUniform3f(sh->star_tint_id, light->tint[0], light->tint[1], light->tint[2]);
	glUniform1f(sh->ambient_id, light->ambient);
	glUniform1f(sh->age_id, mt->age);
	glUniform1f(sh->seed_id, mt->seed);
	glUniform1f(sh->peak_temp_id, mt->peak_temp);
	glUniform1f(sh->cooling_id, mt->cooling);
	glUniform1f(sh->brightness_id, mt->brightness);
	glUniform1f(sh->radiance_id, mt->radiance);
	glUniform1f(sh->density_id, mt->density);
	glUniform1f(sh->edge_id, mt->edge);
	glUniform1f(sh->lumpiness_id, mt->lumpiness);
	glUniform1f(sh->frequency_id, mt->frequency);
	glUniform1f(sh->roll_id, mt->roll);
	glUniform1f(sh->smoke_start_id, mt->smoke_start);
	glUniform1f(sh->smoke_albedo_id, mt->smoke_albedo);
	glUniform1f(sh->dilution_id, mt->dilution);
	glUniform1f(sh->shred_id, mt->shred);
	glUniform1i(sh->steps_id, mt->steps);
	glUniform1f(sh->filmic_tonemapping_id, (float) filmic_tonemapping);
	glUniform1f(sh->tonemapping_gain_id, tonemapping_gain);
	BIND_TEXTURE(GL_TEXTURE0, GL_TEXTURE_2D, blackbody_lut);
	glUniform1i(sh->blackbody_id, 0);

	ship_death_attribute(sh->vertex_position_id, ptr, 3,
			offsetof(struct vertex_buffer_data, position.v.x), 0);
	glDrawArrays(GL_TRIANGLES, 0, m->ntriangles * 3);
	ship_death_attribute_off(sh->vertex_position_id);

	if (reduced) {
		glBindFramebuffer(GL_FRAMEBUFFER, frame_fbo);
		glViewport(0, 0, sgc.screen_x, sgc.screen_y);
		activate_shader(comp);
		BIND_TEXTURE(GL_TEXTURE0, GL_TEXTURE_2D, volume_color);
		glUniform1i(comp->volume_id, 0);
		glEnableVertexAttribArray(volume_composite_corner_id);
		glBindBuffer(GL_ARRAY_BUFFER, volume_composite_corners);
		glVertexAttribPointer(volume_composite_corner_id, 2, GL_FLOAT, GL_FALSE, 0, NULL);
		glDrawArrays(GL_TRIANGLES, 0, 3);
		glDisableVertexAttribArray(volume_composite_corner_id);
		/* The viewport was set behind the cache's back: make the next one set it again. */
		sgc.active_vp = 0;
		sgc.vp_width = -1;
		enable_3d_viewport();
	}
	glEnable(GL_DEPTH_TEST);
	glDepthMask(GL_TRUE);
	glDisable(GL_BLEND);
	glEnable(GL_CULL_FACE);
}

/* A ship's death, which takes the star's light its own way: see struct ship_death_light. */
static void graph_dev_raster_ship_death(struct entity_context *cx,
				const struct entity_transform *transform, struct entity *e)
{
	struct ship_death_light light;
	float ambient_color[3];
	struct mat44 model;

	light.pos[0] = cx->light.m[0];
	light.pos[1] = cx->light.m[1];
	light.pos[2] = cx->light.m[2];
	graph_dev_compute_star_light(cx, light.tint, ambient_color);
	light.ambient = cx->ambient;
	mat44_convert_df(&transform->m, &model);
	switch (e->material_ptr->type) {
	case MATERIAL_SHRAPNEL:
		graph_dev_raster_shrapnel(&transform->mvp, &model, e->m, e->material_ptr, &light);
		break;
	case MATERIAL_WRECK:
		graph_dev_raster_wreck(&transform->mvp, &model, e->m, e->material_ptr, &light);
		break;
	case MATERIAL_PARTICLES:
		graph_dev_raster_particles(&transform->mvp, transform->v, e->m, e->material_ptr,
					&light);
		break;
	case MATERIAL_EXPLOSION:
		graph_dev_raster_explosion(cx, &transform->mvp, &model, e->m, e->material_ptr,
					&light);
		break;
	default:
		break;
	}
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
	int is_sun = 0;
	int is_black_hole = 0;
	int is_exhaust_plume = 0;
	int is_ship_death = 0;
	float shade_scale = 1.0; /* the shadow's pull on ambient; not rtp.ambient_scale */

	int filled_triangle = ((c->renderer & FLATSHADING_RENDERER) || (c->renderer & BLACK_TRIS))
				&& !(e->render_style & RENDER_NO_FILL);
	int outline_triangle = (c->renderer & WIREFRAME_RENDERER)
				|| (e->render_style & RENDER_WIREFRAME);

	int atmosphere = 0;
	struct sng_color texture_tint = { 1.0, 1.0, 1.0 };

	rtp.mat_mvp = &transform->mvp;
	rtp.mat_mv = &transform->mv;
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
		case MATERIAL_SUN:
			/* Handled by graph_dev_raster_sun() below; rtp.shader stays NULL. */
			is_sun = 1;
			break;
		case MATERIAL_BLACK_HOLE:
			/* Handled by graph_dev_raster_black_hole() below; rtp.shader stays NULL. */
			is_black_hole = 1;
			break;
		case MATERIAL_EXHAUST_PLUME:
			/* Handled by graph_dev_raster_exhaust_plume() below; rtp.shader stays NULL. */
			is_exhaust_plume = 1;
			break;
		case MATERIAL_SHRAPNEL:
		case MATERIAL_WRECK:
		case MATERIAL_PARTICLES:
		case MATERIAL_EXPLOSION:
			/* A ship's death: handled by graph_dev_raster_ship_death() below. */
			is_ship_death = 1;
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

				camera_pos_from_mv_matrix(rtp.mat_mv, &shadow_annulus.eye_pos);

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
			rtp.shader = &textured_cubemap_lit_shader;

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
		case MATERIAL_CITY: {
			struct material_city *mt = &e->material_ptr->city;

			rtp.ambient_scale = PLANET_AMBIENT_SCALE;
			rtp.texture_number = mt->texture_id;
			rtp.emit_texture_number = mt->emit_texture_id;
			rtp.do_blend = 1;
			rtp.alpha = entity_get_alpha(e);
			rtp.textures_not_ready = !graph_dev_textures_ready(
				(int []) {rtp.texture_number, rtp.emit_texture_number, -1});
			rtp.shader = &city_shader_no_csm_no_ring;
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
					rtp.shader = &textured_cubemap_lit_with_annulus_shadow_shader;
				} else if (graph_dev_planet_specularity)  {
					rtp.shader =
					&textured_cubemap_normal_mapped_lit_with_annulus_shadow_specular_shader;
				} else {
					rtp.shader =
						&textured_cubemap_normal_mapped_lit_with_annulus_shadow_shader;
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

				camera_pos_from_mv_matrix(rtp.mat_mv, &shadow_annulus.eye_pos);

				/* ring is the 2x to 3x of the planet scale, world space distance
				   is the same in eye space as the view matrix does not scale */
				shadow_annulus.r1 = vec3_cwise_max(&e->scale) *
								rtp.ring_inner_radius;
				shadow_annulus.r2 = vec3_cwise_max(&e->scale) *
								rtp.ring_outer_radius;
			} else {
				if (rtp.normalmap_id > 0) {
					if (graph_dev_planet_specularity) {
						rtp.shader =
							&textured_cubemap_normal_mapped_lit_specular_shader;
					} else {
						rtp.shader = &textured_cubemap_lit_normal_map_shader;
					}
				} else {
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

			camera_pos_from_mv_matrix(rtp.mat_mv, &shadow_sphere.eye_pos);

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
				} else if (is_exhaust_plume) {
					graph_dev_raster_exhaust_plume(rtp.mat_mvp, rtp.mat_mv, e->m, e->material_ptr);
				} else if (is_ship_death) {
					graph_dev_raster_ship_death(cx, transform, e);
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
						rtp.mat_normal, e->m, &triangle_color, eye_light_pos,
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

	if (!GLES_HAS_VAO) {
		glDisableVertexAttribArray(shader->vertex_position_id);
		glDisableVertexAttribArray(shader->texture_coord_id);
	}

	PROFILE_ZONE_END();
}


void graph_dev_start_frame(void)
{
	PROFILE_ZONE_START("graph_dev_start_frame");

	/* RESYNCHRONISE THE CACHED BLEND STATE, because the cache is only true while nothing else
	 * touches the context.
	 *
	 * BLEND_FUNC (graph_dev/context.h) skips glBlendFunc when sgc already records the pair
	 * being asked for, which is worth doing and is safe exactly as long as graph_dev is the
	 * only thing drawing.  It is not: anything sharing the GL context and calling glBlendFunc
	 * directly -- a lab's HUD, an overlay, a debug tool -- leaves the cache claiming a state
	 * the driver no longer has, and every later BLEND_FUNC for that pair is then skipped as
	 * redundant when it is the one call that would have fixed things.
	 *
	 * The symptom is worth recording, because it looks nothing like a blend bug.  With the
	 * func left at (SRC_ALPHA, ONE_MINUS_SRC_ALPHA), the star's two GLARE passes emit alpha
	 * zero -- they are premultiplied, the colour is emission and the alpha is only the disc's
	 * coverage -- so they multiply to nothing and vanish, leaving the disc pass alone.  The
	 * star renders as a hard white circle with no glow whatever, which reads as the sun shader
	 * not running at all.  It cost a long hunt through asset paths, shader loading and depth
	 * state before the actual cause, and it only appeared when the HUD was drawn, so every
	 * --no-hud screenshot taken to investigate it looked perfectly correct.
	 *
	 * One redundant glBlendFunc per frame is not a cost worth reasoning about.  Do this at the
	 * top of the frame and the cache is honest for the rest of it however the context is
	 * shared. */
	glBlendFunc(GL_ONE, GL_ZERO);
	sgc.src_blend_func = GL_ONE;
	sgc.dest_blend_func = GL_ZERO;

	graph_dev_send_completed_textures_to_gpu();

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

		/* glEnable(GL_MULTISAMPLE); */

		glBindFramebuffer(GL_FRAMEBUFFER, msaa.fbo);
		sgc.fbo_3d = msaa.fbo;
#if 0
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
#endif

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

#if 0
	if (msaa.fbo != 0 && sgc.fbo_3d == msaa.fbo) {
		glBindFramebuffer(GL_READ_FRAMEBUFFER, msaa.fbo);
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
		glBlitFramebuffer(0, 0, msaa.width, msaa.height, 0, 0,
			sgc.screen_x, sgc.screen_y, GL_COLOR_BUFFER_BIT, GL_NEAREST);

		/* glDisable(GL_MULTISAMPLE); */

	} else
#endif
	 if (post_target0.fbo != 0 && sgc.fbo_3d == post_target0.fbo) {
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


			/* clean up the mess and select which buffer to show - debug options don't invalidate
			 * the intermediate FBOs efficiently */
			maybe_discard_fbo_contents(&post_target0);

			if (draw_smaa_edge)
				result_texture = smaa_effect.edge_target.color0_texture;
			else {
				maybe_discard_fbo_contents(&smaa_effect.edge_target);
				if (draw_smaa_blend) {
					result_texture = smaa_effect.blend_target.color0_texture;
				} else {
					maybe_discard_fbo_contents(&smaa_effect.blend_target);
					result_texture = post_target1.color0_texture;
				}
			}

		} else {
			result_texture = post_target0.color0_texture;
		}

		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		graph_dev_raster_full_screen_effect(&fs_copy_shader, result_texture, 0, 0, 0, 1);

		maybe_discard_fbo_contents(&post_target1);
		/* discarding post_target0 a second time won't hurt here unless the
		 * driver is *really* bad */
		maybe_discard_fbo_contents(&post_target0);
	}

	if (render_target_2d.fbo != 0 && sgc.fbo_2d == render_target_2d.fbo) {
		/* alpha blend copy 2d fbo onto back buffer */
		glBindFramebuffer(GL_FRAMEBUFFER, 0);

		glEnable(GL_BLEND);
		BLEND_FUNC(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		graph_dev_raster_full_screen_effect(&fs_copy_shader, render_target_2d.color0_texture, 0, 0, 0, 1);
		glDisable(GL_BLEND);

		maybe_discard_fbo_contents(&render_target_2d);
	}

	PROFILE_ZONE_END();
}

/* Cascaded shadow mapping is not implemented on the GLES backend; these are
 * no-op stubs so the shared entity.c links, and shadows are simply disabled. */
void graph_dev_set_shadow_cascades(const struct mat44d *world_to_lightclip, int n)
{
	(void) world_to_lightclip;
	(void) n;
}

void graph_dev_shadow_map_begin(void)
{
}

void graph_dev_shadow_map_set_cascade(int cascade)
{
	(void) cascade;
}

void graph_dev_draw_shadow_caster(const struct mat44d *model, struct mesh *m)
{
	(void) model;
	(void) m;
}

void graph_dev_shadow_map_end(void)
{
}

void graph_dev_set_shadow_debug(int mode)
{
	(void) mode;
}

void graph_dev_set_shadow_bias(float factor, float units)
{
	(void) factor;
	(void) units;
}

void graph_dev_get_shadow_bias(float *factor, float *units)
{
	if (factor)
		*factor = 0.0f;
	if (units)
		*units = 0.0f;
}

void graph_dev_set_shadow_pcf_radius(int radius)
{
	(void) radius;
}

int graph_dev_get_shadow_pcf_radius(void)
{
	return 0;
}

void graph_dev_set_shadow_normal_offset(float texels)
{
	(void) texels;
}

float graph_dev_get_shadow_normal_offset(void)
{
	return 0.0;
}

void graph_dev_set_shadow_cascade_splits(const float *split_far, int n)
{
	(void) split_far;
	(void) n;
}


void graph_dev_set_shadow_blend(float fraction)
{
	(void) fraction;
}

float graph_dev_get_shadow_blend(void)
{
	return 0.0f;
}

static void setup_single_color_lit_shader(struct graph_dev_gl_single_color_lit_shader *shader)
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
				UNIVERSAL_SHADER_HEADER FILMIC_TONEMAPPING);

	/* create the VAO for this shader */
	graph_dev_gen_vao(&shader->vao_id);

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
}

static void setup_textured_shader(const char *basename, const char *defines,
	struct graph_dev_gl_textured_shader *shader)
{
	maybe_unload_shader(&shader->meta, &shader->program_id);
	memset(shader, 0xff, sizeof(*shader)); /* set all attributes to -1 */
	shader->meta.program_id = &shader->program_id; /* Work around what that memset just did. */

	char vert_header[512];
	snprintf(vert_header, sizeof(vert_header), "%s\n#define INCLUDE_VS 1\n", defines);
	char frag_header[512];
	snprintf(frag_header, sizeof(frag_header), "%s\n#define INCLUDE_FS 1\n", defines);

	char shader_filename[PATH_MAX];
	snprintf(shader_filename, sizeof(shader_filename), "%s.shader", basename);

	const char *filenames[] = { shader_filename };

	shader->program_id = load_concat_shaders(shader_directory,
				vert_header, 1, filenames, frag_header, 1, filenames);

	/* create the VAO for this shader */
	graph_dev_gen_vao(&shader->vao_id);

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
}

static void setup_textured_cubemap_shader(const char *basename, int use_normal_map,
				int use_specular, int use_annulus_shadow,
				struct graph_dev_gl_textured_shader *shader)
{
	maybe_unload_shader(&shader->meta, &shader->program_id);
	memset(shader, 0xff, sizeof(*shader)); /* set all attributes to -1 */
	shader->meta.program_id = &shader->program_id; /* Work around what that memset just did. */

	char vert_header[1024];
	char frag_header[1024];

	snprintf(vert_header, 1024, "%s\n%s\n%s\n%s\n%s\n",
		UNIVERSAL_SHADER_HEADER, "#define INCLUDE_VS 1\n",
			use_normal_map ? "#define USE_NORMAL_MAP 1\n" : "\n",
			use_specular ? "#define USE_SPECULAR 1\n" : "\n",
			use_annulus_shadow ? "#define USE_ANNULUS_SHADOW 1\n" : "\n");
	snprintf(frag_header, 1024, "%s\n%s\n%s\n%s\n%s\n",
		UNIVERSAL_SHADER_HEADER FILMIC_TONEMAPPING, "#define INCLUDE_FS 1\n",
			use_normal_map ? "#define USE_NORMAL_MAP 1\n" : "\n",
			use_specular ? "#define USE_SPECULAR 1\n" : "\n",
			use_annulus_shadow ? "#define USE_ANNULUS_SHADOW 1\n" : "\n");

	char shader_filename[PATH_MAX];
	snprintf(shader_filename, sizeof(shader_filename), "%s.shader", basename);

	const char *filenames[] = { shader_filename };

	shader->program_id = load_concat_shaders(shader_directory,
				vert_header, 1, filenames, frag_header, 1, filenames);
	graph_dev_gen_vao(&shader->vao_id);
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

static void setup_sun_shader(struct graph_dev_gl_sun_shader *shader)
{
	maybe_unload_shader(&shader->meta, &shader->program_id);
	shader->program_id = load_shaders(shader_directory,
				"sun.vert", "sun.frag", UNIVERSAL_SHADER_HEADER FILMIC_TONEMAPPING);
	graph_dev_gen_vao(&shader->vao_id);

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

static void setup_smaa_effect_shader(const char *basename, struct graph_dev_gl_fs_effect_shader *shader)
{
	char shader_filename[255];
	snprintf(shader_filename, sizeof(shader_filename), "%s.shader", basename);

	const char *vert_header;
	const char *frag_header;
	vert_header =
		UNIVERSAL_SHADER_HEADER
		"#define INCLUDE_VS 1\n";
	frag_header =
		UNIVERSAL_SHADER_HEADER
		"#define INCLUDE_FS 1\n";

	const char *filenames[] = { "smaa-high.shader", "SMAA.hlsl", shader_filename };

	maybe_unload_shader(&shader->meta, &shader->program_id);
	shader->program_id = load_concat_shaders(shader_directory,
				vert_header, 3, filenames, frag_header, 3, filenames);
	/* create the VAO for this shader */
	graph_dev_gen_vao(&shader->vao_id);

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
#include "smaa/gles2/AreaTex.h"

	graph_dev_gen_texture(1, &effect->area_tex);
	glBindTexture(GL_TEXTURE_2D, effect->area_tex);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE_ALPHA, (GLsizei)AREATEX_WIDTH, (GLsizei)AREATEX_HEIGHT, 0,
		GL_LUMINANCE_ALPHA, GL_UNSIGNED_BYTE, areaTexBytes);

	/* include file defines sizes and searchTexBytes of the search texture */
#include "smaa/gles2/SearchTex.h"

	graph_dev_gen_texture(1, &effect->search_tex);
	glBindTexture(GL_TEXTURE_2D, effect->search_tex);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, (GLsizei)SEARCHTEX_WIDTH, (GLsizei)SEARCHTEX_HEIGHT, 0,
		GL_LUMINANCE, GL_UNSIGNED_BYTE, searchTexBytes);

	glGenFramebuffers(1, &effect->edge_target.fbo);
	glBindFramebuffer(GL_FRAMEBUFFER, effect->edge_target.fbo);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
		effect->edge_target.color0_texture, 0);

	glGenFramebuffers(1, &effect->blend_target.fbo);
	glBindFramebuffer(GL_FRAMEBUFFER, effect->blend_target.fbo);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
		effect->blend_target.color0_texture, 0);
}

void graph_dev_select_draw_buffer(int which)
{
	/* GLES2 draws to the one buffer it has and has no glDrawBuffers to say so with. */
	(void) which;
}

void graph_dev_gen_vao(GLuint *vao)
{
	if (GLES_HAS_VAO)
		glGenVertexArraysOES(1, vao);
}

void graph_dev_bind_vao(GLuint vao)
{
	if (GLES_HAS_VAO)
		glBindVertexArrayOES(vao);
}

void graph_dev_unbind_vao(void)
{
	if (GLES_HAS_VAO)
		glBindVertexArrayOES(0);
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
		graph_dev_gen_texture(1, &render_target_2d.color0_texture);
		glBindTexture(GL_TEXTURE_2D, render_target_2d.color0_texture);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

		glGenFramebuffers(1, &render_target_2d.fbo);
		glBindFramebuffer(GL_FRAMEBUFFER, render_target_2d.fbo);
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
	glBufferData(GL_ARRAY_BUFFER, 0, 0, GL_STATIC_DRAW);

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

	setup_single_color_lit_shader(&single_color_lit_shader);
	setup_atmosphere_shader(&atmosphere_shader, 0);
	setup_atmosphere_shader(&atmosphere_with_annulus_shadow_shader, 1);
	setup_trans_wireframe_shader("wireframe_transparent", &trans_wireframe_shader);
	setup_trans_wireframe_shader("wireframe-transparent-sphere-clip", &trans_wireframe_with_clip_sphere_shader);
	setup_filled_wireframe_shader(&filled_wireframe_shader);
	setup_single_color_shader(&single_color_shader);
	setup_vertex_color_shader(&vertex_color_shader);
	setup_line_single_color_shader(&line_single_color_shader);
	setup_point_cloud_shader("point_cloud", &point_cloud_shader);
	setup_color_by_w_shader(&color_by_w_shader);
	setup_skybox_shader(&skybox_shader);
	setup_sun_shader(&sun_shader);
	setup_black_hole_shader(&black_hole_shader);
	setup_exhaust_plume_shader(&exhaust_plume_shader);
	setup_ship_death_shaders();
	setup_textured_shader("textured", UNIVERSAL_SHADER_HEADER FILMIC_TONEMAPPING, &textured_shader);
	setup_textured_shader("textured-with-sphere-shadow-per-pixel", UNIVERSAL_SHADER_HEADER FILMIC_TONEMAPPING,
				&textured_with_sphere_shadow_shader);
	setup_textured_shader("textured-and-lit-per-pixel",
				UNIVERSAL_SHADER_HEADER FILMIC_TONEMAPPING, &textured_lit_shader);
	setup_textured_shader("textured-and-lit-per-pixel",
				UNIVERSAL_SHADER_HEADER FILMIC_TONEMAPPING "#define USE_EMIT_MAP",
				&textured_lit_emit_shader);
	setup_textured_shader("textured-and-lit-per-pixel", UNIVERSAL_SHADER_HEADER FILMIC_TONEMAPPING
				"#define USE_EMIT_MAP\n"
				"#define USE_NORMAL_MAP 1\n",
				&textured_lit_emit_normal_shader);
	setup_textured_shader("textured-and-lit-per-pixel", UNIVERSAL_SHADER_HEADER FILMIC_TONEMAPPING
				"#define USE_NORMAL_MAP 1\n",
				&textured_lit_normal_shader);
	setup_textured_cubemap_shader("textured-cubemap-and-lit-with-annulus-shadow-per-pixel", 0, 0, 0,
					&textured_cubemap_lit_shader);
	setup_textured_cubemap_shader("textured-cubemap-and-lit-with-annulus-shadow-per-pixel", 1, 0, 0,
					&textured_cubemap_lit_normal_map_shader);
	setup_textured_cubemap_shader("textured-cubemap-shield-per-pixel", 0, 0, 0,
					&textured_cubemap_shield_shader);
	setup_textured_cubemap_shader("textured-cubemap-and-lit-with-annulus-shadow-per-pixel", 0, 0, 1,
					&textured_cubemap_lit_with_annulus_shadow_shader);
	setup_textured_cubemap_shader("textured-cubemap-and-lit-with-annulus-shadow-per-pixel", 1, 0, 1,
					&textured_cubemap_normal_mapped_lit_with_annulus_shadow_shader);
	setup_textured_cubemap_shader("textured-cubemap-and-lit-with-annulus-shadow-per-pixel", 1, 1, 1,
					&textured_cubemap_normal_mapped_lit_with_annulus_shadow_specular_shader);
	setup_textured_cubemap_shader("textured-cubemap-and-lit-with-annulus-shadow-per-pixel", 1, 1, 0,
					&textured_cubemap_normal_mapped_lit_specular_shader);
	setup_textured_particle_shader(&textured_particle_shader);
	setup_fs_effect_shader("fs-effect-copy", &fs_copy_shader);
	setup_textured_shader("alpha_by_normal", UNIVERSAL_SHADER_HEADER, &alpha_by_normal_shader);
	setup_textured_shader("alpha_by_normal", UNIVERSAL_SHADER_HEADER "#define TEXTURED_ALPHA_BY_NORMAL",
				&textured_alpha_by_normal_shader);
	setup_textured_shader("planetary-lightning", UNIVERSAL_SHADER_HEADER, &planetary_lightning_shader);
	setup_textured_shader("city", UNIVERSAL_SHADER_HEADER FILMIC_TONEMAPPING,
				&city_shader_no_csm_no_ring);
	setup_textured_shader("warp-gate-effect", UNIVERSAL_SHADER_HEADER FILMIC_TONEMAPPING, &warp_gate_effect_shader);

	if (fbo_render_to_texture_supported())
		setup_smaa_effect(&smaa_effect);

	PROFILE_ZONE_END();
}







int graph_dev_setup(const char *asset_dir)
{
	if (!gladLoadGLES2((GLADloadfunc)SDL_GL_GetProcAddress)) {
		fprintf(stderr, "Got error trying to bind GL ES\n");
		return -1;
	}
	printf("Initialized GLAD\n");

	const char *version = (const char *)glGetString(GL_VERSION);
	const char *vendor = (const char *)glGetString(GL_VENDOR);
	const char *renderer = (const char *)glGetString(GL_RENDERER);
	const char *glslversion = (const char *)glGetString(GL_SHADING_LANGUAGE_VERSION);
	fprintf(stderr, "OpenGL ES: Version:  %s\n", version);
	fprintf(stderr, "          Vendor:   %s\n", vendor);
	fprintf(stderr, "          Renderer: %s\n", renderer);
	fprintf(stderr, "          Shader Language Version: %s\n", glslversion);

	if (GLAD_GL_EXT_sRGB) {
		fprintf(stderr, "WARNING: No hardware support for SRGB colorspace - will force linear.\n");
	}

	if (GLAD_GL_EXT_discard_framebuffer) {
		fprintf(stderr, "Has hardware support for discarding framebuffers.\n");
	}

	int want8bit = 0;
	int bitWidth = 0;
	static const SDL_GLattr attrs[] = { SDL_GL_RED_SIZE, SDL_GL_GREEN_SIZE, SDL_GL_BLUE_SIZE };
	for (unsigned idx = 0; idx < ARRAYSIZE(attrs); idx++) {
		SDL_GL_GetAttribute(attrs[idx], &bitWidth);
		if (bitWidth > 4) {
			want8bit = 1;
			break;
		}
	}

	if (GLAD_GL_OES_rgb8_rgba8) {
		if (want8bit) {
			fbo_format = GL_RGBA8_OES;
		}
	} else {
		fbo_format = GL_RGBA4;
		if (want8bit) {
			fprintf(stderr, "WARNING: our real buffers are > RGBA4, but we don't have GL_OES_rgb8_rgba8 - FBOs will be RGBA4\n");
		}
	}

	if (asset_dir)
		snprintf(shader_directory, sizeof(shader_directory), "%s/shader-es", asset_dir);
	else
		strlcpy(shader_directory, GRAPH_DEV_DEFAULT_SHADER_DIRECTORY, PATH_MAX);

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
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
			post_target1.color0_texture, 0);
	}

	graph_dev_reload_all_shaders();

	/* after all the shaders are loaded */
	setup_cubemap_cube(&cubemap_cube);
	setup_textured_unit_quad(&textured_unit_quad);

	setup_2d();
	setup_3d();

	graph_dev_set_up_image_loader_work_queues();

	return 0;
}

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
/* returns zero on success, -1 otherwise */
unsigned int graph_dev_cubemap_texture_to_gpu(struct graph_dev_image_load_request *r)
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

	int i;
	for (i = 0; i < NCUBEMAP_TEXTURES; i++) {
		/* do horizontal invert if we are projecting on the inside */
		char *image_data = r->image_data[i];

		if (r->linear_colorspace || !GLAD_GL_EXT_sRGB)
			colorspace = r->hasAlpha[i] ? GL_RGBA : GL_RGB;
		else
			colorspace = r->hasAlpha[i] ? GL_SRGB_ALPHA_EXT : GL_SRGB_EXT;
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



/* Load image data to GPU, image_data is not freed */
int graph_dev_texture_to_gpu_id(GLuint texture_number, char *image_data,
		int w, int h, int hasAlpha, int use_mipmaps, int linear_colorspace)
{
	GLint colorspace;

	if (!image_data) {
		fprintf(stderr, "texture_to_gpu_id: NULL image data\n");
		return -1;
	}

	if (linear_colorspace || !GLAD_GL_EXT_sRGB)
		colorspace = hasAlpha ? GL_RGBA : GL_RGB;
	else
		colorspace = hasAlpha ? GL_SRGB_ALPHA_EXT : GL_SRGB_EXT;

	glBindTexture(GL_TEXTURE_2D, texture_number);
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

	if (!GLES_HAS_VAO) {
		glDisableVertexAttribArray(skybox_shader.vertex_id);
	}
	glDepthMask(GL_TRUE);
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_CULL_FACE);
}

void graph_dev_display_debug_menu_show(void)
{
	sng_set_foreground(BLACK);
	graph_dev_draw_rectangle(1, 10, 30, 370 * sgc.x_scale, 265);
	sng_set_foreground(WHITE);
	graph_dev_draw_rectangle(0, 10, 30, 370 * sgc.x_scale, 265);

#if DEBUG_NORMALS
	debug_menu_draw_item("VERTEX NORM/TAN/BITAN (RGB)", 0, 0, draw_normal_lines);
#else
	debug_menu_draw_item("VERTEX NORM/TAN/BITAN (RGB)", 0, 1, draw_normal_lines);
#endif
	sng_set_foreground(WHITE);
	debug_menu_draw_item("BILLBOARD WIREFRAME", 1, 0, draw_billboard_wireframe);
	/* debug_menu_draw_item("POLYGON AS LINE", 2, 0, draw_polygon_as_lines); */
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
}

int graph_dev_graph_dev_debug_menu_click(int x, int y)
{
#if DEBUG_NORMALS
	if (selected_debug_item_checkbox(0, x, y, &draw_normal_lines))
		return 1;
#endif
	if (selected_debug_item_checkbox(1, x, y, &draw_billboard_wireframe))
		return 1;
	/* if (selected_debug_item_checkbox(2, x, y, &draw_polygon_as_lines))
		return 1; */
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
	return 0;
}

void graph_dev_set_tonemapping_gain(float tmg)
{
	if (tmg >= MIN_TONEMAPPING_GAIN && tmg <= MAX_TONEMAPPING_GAIN)
		tonemapping_gain = tmg;
}








/* Call this early on to wipe out garbage otherwise left in the window */
void graph_dev_prepare_for_window(uint32_t *window_flags)
{
	SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 4);
	SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 4);
	SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 4);
	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 16);

	/* for GLES, we claim ES 2.0 */
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);

	*window_flags = *window_flags | SDL_WINDOW_OPENGL;
}

void graph_dev_create_context(SDL_Window *window)
{
	SDL_GLContext gl_context = SDL_GL_CreateContext(window);
	if (NULL == gl_context) {
		fprintf(stderr, "Couldn't create OpenGL ES Context: %s\n", SDL_GetError());
		exit(1);
	}
	(void) gl_context;
}

void graph_dev_shadow_map(int new_status)
{
	(void) new_status;
}

int graph_dev_shadow_map_status(void)
{
	return SHADOW_MAP_DISABLED;
}

