#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <gtk/gtk.h>
#include <GL/glew.h>
#include <math.h>
#include <assert.h>

#include "shader.h"
#include "vertex.h"
#include "triangle.h"
#include "mathutils.h"
#include "matrix.h"
#include "mesh.h"
#include "quat.h"
#include "vec4.h"
#include "snis_graph.h"
#include "graph_dev.h"
#include "entity.h"
#include "entity_private.h"
#include "material.h"

struct mesh_gl_info {
	/* common buffer to hold vertex positions */
	GLuint vertex_buffer;

	int ntriangles;
	/* uses vertex_buffer for data */
	GLuint triangle_vertex_buffer;

	int nwireframe_lines;
	GLuint wireframe_lines_vertex_buffer;

	int npoints;
	/* uses vertex_buffer for data */

	int nlines;
	/* uses vertex_buffer for data */
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
};

struct vertex_wireframe_line_buffer_data {
	union vec3 position;
	union vec3 normal;
};

struct vertex_color_buffer_data {
	GLfloat position[2];
	GLubyte color[4];
};

void mesh_graph_dev_cleanup(struct mesh *m)
{
	if (m->graph_ptr) {
		struct mesh_gl_info *ptr = m->graph_ptr;

		glDeleteBuffers(1, &ptr->vertex_buffer);
		glDeleteBuffers(1, &ptr->triangle_vertex_buffer);
		glDeleteBuffers(1, &ptr->wireframe_lines_vertex_buffer);

		free(ptr);
		m->graph_ptr = 0;
	}
}

void mesh_graph_dev_init(struct mesh *m)
{
	if (m->graph_ptr)
		mesh_graph_dev_cleanup(m);

	struct mesh_gl_info *ptr = malloc(sizeof(struct mesh_gl_info));
	memset(ptr, 0, sizeof(*ptr));

	if (m->geometry_mode == MESH_GEOMETRY_TRIANGLES) {
		/* setup the triangle mesh buffers */
		int i;
		size_t v_size = sizeof(struct vertex_buffer_data) * m->ntriangles * 3;
		size_t vt_size = sizeof(struct vertex_triangle_buffer_data) * m->ntriangles * 3;
		struct vertex_buffer_data *g_v_buffer_data = malloc(v_size);
		struct vertex_triangle_buffer_data *g_vt_buffer_data = malloc(vt_size);

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
			}
		}

		glGenBuffers(1, &ptr->vertex_buffer);
		glBindBuffer(GL_ARRAY_BUFFER, ptr->vertex_buffer);
		glBufferData(GL_ARRAY_BUFFER, v_size, g_v_buffer_data, GL_STATIC_DRAW);

		glGenBuffers(1, &ptr->triangle_vertex_buffer);
		glBindBuffer(GL_ARRAY_BUFFER, ptr->triangle_vertex_buffer);
		glBufferData(GL_ARRAY_BUFFER, vt_size, g_vt_buffer_data, GL_STATIC_DRAW);

		free(g_v_buffer_data);
		free(g_vt_buffer_data);

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

		glGenBuffers(1, &ptr->wireframe_lines_vertex_buffer);
		glBindBuffer(GL_ARRAY_BUFFER, ptr->wireframe_lines_vertex_buffer);
		glBufferData(GL_ARRAY_BUFFER, sizeof(struct vertex_wireframe_line_buffer_data) *
			ptr->nwireframe_lines * 2, g_wfl_buffer_data, GL_STATIC_DRAW);

		free(g_wfl_buffer_data);
	}

	if (m->geometry_mode == MESH_GEOMETRY_LINES) {
		/* setup the line buffers */
		int i;
		size_t v_size = sizeof(struct vertex_buffer_data) * m->nvertices * 2;
		struct vertex_buffer_data *g_v_buffer_data = malloc(v_size);

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

						ptr->nlines++;
					}
					v1 = v2;
					++vcurr;
				}
			} else {
				/* do dotted e->m->l[i].flag & MESH_LINE_DOTTED) */

				int index = ptr->nlines * 2;
				g_v_buffer_data[index].position.v.x = vstart->x;
				g_v_buffer_data[index].position.v.y = vstart->y;
				g_v_buffer_data[index].position.v.z = vstart->z;

				g_v_buffer_data[index + 1].position.v.x = vend->x;
				g_v_buffer_data[index + 1].position.v.y = vend->y;
				g_v_buffer_data[index + 1].position.v.z = vend->z;

				ptr->nlines++;
			}
		}

		glGenBuffers(1, &ptr->vertex_buffer);
		glBindBuffer(GL_ARRAY_BUFFER, ptr->vertex_buffer);
		glBufferData(GL_ARRAY_BUFFER, sizeof(struct vertex_buffer_data) * 2 * ptr->nlines, g_v_buffer_data, GL_STATIC_DRAW);

		ptr->npoints = ptr->nlines * 2; /* can be rendered as a point cloud too */

		free(g_v_buffer_data);
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

		glGenBuffers(1, &ptr->vertex_buffer);
		glBindBuffer(GL_ARRAY_BUFFER, ptr->vertex_buffer);
		glBufferData(GL_ARRAY_BUFFER, v_size, g_v_buffer_data, GL_STATIC_DRAW);

		free(g_v_buffer_data);
	}

	m->graph_ptr = ptr;
}

struct graph_dev_gl_vertex_color_shader {
	GLuint program_id;
	GLuint mvp_matrix_id;
	GLuint vertex_position_id;
	GLuint vertex_color_id;
};

struct graph_dev_gl_single_color_lit_shader {
	GLuint program_id;
	GLuint mvp_matrix_id;
	GLuint mv_matrix_id;
	GLuint normal_matrix_id;
	GLuint vertex_position_id;
	GLuint vertex_normal_id;
	GLuint light_pos_id;
	GLuint color_id;
};

struct graph_dev_gl_filled_wireframe_shader {
	GLuint program_id;
	GLuint viewport_id;
	GLuint mvp_matrix_id;
	GLuint position_id;
	GLuint tvertex0_id;
	GLuint tvertex1_id;
	GLuint tvertex2_id;
	GLuint edge_mask_id;
	GLuint line_color_id;
	GLuint triangle_color_id;
};

struct graph_dev_gl_trans_wireframe_shader {
	GLuint program_id;
	GLuint mvp_matrix_id;
	GLuint mv_matrix_id;
	GLuint normal_matrix_id;
	GLuint vertex_position_id;
	GLuint vertex_normal_id;
	GLuint color_id;
};

struct graph_dev_gl_single_color_shader {
	GLuint program_id;
	GLuint mvp_matrix_id;
	GLuint vertex_position_id;
	GLuint color_id;
};

struct graph_dev_gl_point_cloud_shader {
	GLuint program_id;
	GLuint mvp_matrix_id;
	GLuint vertex_position_id;
	GLuint point_size_id;
	GLuint color_id;
};

struct graph_dev_gl_skybox_shader {
	GLuint program_id;
	GLuint mvp_id;
	GLuint vertex_id;
	GLuint texture_id;

	int nvertices;
	GLuint vbo_cube_vertices;

	int texture_loaded;
	GLuint cube_texture_id;
};

struct graph_dev_gl_color_by_w_shader {
	GLuint program_id;
	GLuint mvp_id;
	GLuint position_id;
	GLuint near_color_id;
	GLuint near_w_id;
	GLuint center_color_id;
	GLuint center_w_id;
	GLuint far_color_id;
	GLuint far_w_id;
};

struct graph_dev_gl_textured_shader {
	GLuint program_id;
	GLuint mvp_matrix_id;
	GLuint vertex_position_id;
	GLuint tint_color_id;
	GLuint texture_coord_id;
	GLuint texture_id; /* param to vertex shader */
};

struct graph_dev_gl_textured_lit_shader {
	GLuint program_id;
	GLuint mvp_matrix_id;
	GLuint mv_matrix_id;
	GLuint normal_matrix_id;
	GLuint vertex_position_id;
	GLuint vertex_normal_id;
	GLuint texture_coord_id;
	GLuint light_pos_id;
	GLuint color_id;
	GLuint texture_id; /* param to vertex shader */
};

struct graph_dev_gl_textured_cubemap_lit_shader {
	GLuint program_id;
	GLuint mvp_matrix_id;
	GLuint mv_matrix_id;
	GLuint normal_matrix_id;
	GLuint vertex_position_id;
	GLuint vertex_normal_id;
	GLuint light_pos_id;
	GLuint texture_id; /* param to vertex shader */
};

/* store all the shader parameters */
static struct graph_dev_gl_single_color_lit_shader single_color_lit_shader;
static struct graph_dev_gl_trans_wireframe_shader trans_wireframe_shader;
static struct graph_dev_gl_filled_wireframe_shader filled_wireframe_shader;
static struct graph_dev_gl_single_color_shader single_color_shader;
static struct graph_dev_gl_vertex_color_shader vertex_color_shader;
static struct graph_dev_gl_point_cloud_shader point_cloud_shader;
static struct graph_dev_gl_skybox_shader skybox_shader;
static struct graph_dev_gl_color_by_w_shader color_by_w_shader;
static struct graph_dev_gl_textured_shader textured_shader;
static struct graph_dev_gl_textured_lit_shader textured_lit_shader;
static struct graph_dev_gl_textured_cubemap_lit_shader textured_cubemap_lit_shader;

#define BUFFERED_VERTICES_2D 2000
#define VERTEX_BUFFER_2D_SIZE (BUFFERED_VERTICES_2D*sizeof(struct vertex_color_buffer_data))

static struct graph_dev_gl_context {
	int screen_x, screen_y;
	GdkColor *hue; /* current color */
	int alpha_blend;
	float alpha;

	int active_vp; /* 0=none, 1=2d, 2=3d */
	int vp_x_3d, vp_y_3d, vp_width_3d, vp_height_3d;
	struct mat44 ortho_2d_mvp;

	int nvertex_2d;
	GLbyte vertex_type_2d[BUFFERED_VERTICES_2D];
	GLuint vertex_buffer_2d;
	int vertex_buffer_2d_offset;
} sgc;

void graph_dev_set_screen_size(int width, int height)
{
	sgc.active_vp = 0;
	sgc.screen_x = width;
	sgc.screen_y = height;
}

void graph_dev_set_extent_scale(float x_scale, float y_scale)
{

}

void graph_dev_set_3d_viewport(int x_offset, int y_offset, int width, int height)
{
	sgc.active_vp = 0;
	sgc.vp_x_3d = x_offset;
	sgc.vp_y_3d = sgc.screen_y - height - y_offset;
	sgc.vp_width_3d = width;
	sgc.vp_height_3d = height;
}

static void enable_2d_viewport()
{
	if (sgc.active_vp != 1) {
		glDisable(GL_DEPTH_TEST);

		/* 2d viewport is entire screen */
		glViewport(0, 0, sgc.screen_x, sgc.screen_y);

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

		sgc.active_vp = 1;
	}
}

static void enable_3d_viewport()
{
	if (sgc.active_vp != 2) {
		glViewport(sgc.vp_x_3d, sgc.vp_y_3d, sgc.vp_width_3d, sgc.vp_height_3d);

		sgc.active_vp = 2;
	}
}


void graph_dev_set_color(GdkColor *c, float a)
{
	sgc.hue = c;

	if (a >= 0) {
		sgc.alpha_blend = 1;
		sgc.alpha = a;
	} else {
		sgc.alpha_blend = 0;
	}
}

void graph_dev_set_context(GdkDrawable *drawable, GdkGC *gc)
{
	/* noop */
}

static void draw_vertex_buffer_2d()
{
	if (sgc.nvertex_2d > 0) {
		/* printf("start draw_vertex_buffer_2d %d\n", sgc.nvertex_2d); */
		enable_2d_viewport();

		glUseProgram(vertex_color_shader.program_id);

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
		sgc.vertex_buffer_2d_offset = 0;

		glDisableVertexAttribArray(vertex_color_shader.vertex_position_id);
		glDisableVertexAttribArray(vertex_color_shader.vertex_color_id);
		glUseProgram(0);

		/* orphan this buffer so we don't get blocked on these draw commands */
		glBindBuffer(GL_ARRAY_BUFFER, sgc.vertex_buffer_2d);
		glBufferData(GL_ARRAY_BUFFER, VERTEX_BUFFER_2D_SIZE, 0, GL_STREAM_DRAW);
	}
}

static void make_room_in_vertex_buffer_2d(int nvertices)
{
	if (sgc.nvertex_2d + nvertices > BUFFERED_VERTICES_2D) {
		/* buffer needs to be emptied to fit next batch */
		draw_vertex_buffer_2d();
	}
}

static void add_vertex_2d(float x, float y, GdkColor* color, GLubyte alpha, GLenum mode)
{
	struct vertex_color_buffer_data vertex;

	/* setup the vertex and color */
	vertex.position[0] = x;
	vertex.position[1] = sgc.screen_y - y;

	vertex.color[0] = color->red >> 8;
	vertex.color[1] = color->green >> 8;
	vertex.color[2] = color->blue >> 8;
	vertex.color[3] = alpha;

	/* transfer into opengl buffer */
	glBindBuffer(GL_ARRAY_BUFFER, sgc.vertex_buffer_2d);
	glBufferSubData(GL_ARRAY_BUFFER, sgc.vertex_buffer_2d_offset, sizeof(vertex), &vertex);

	sgc.vertex_type_2d[sgc.nvertex_2d] = mode;

	sgc.nvertex_2d += 1;
	sgc.vertex_buffer_2d_offset += sizeof(vertex);
}

static void graph_dev_raster_texture(const struct mat44 *mat_mvp, const struct mat44 *mat_mv,
	const struct mat33 *mat_normal, struct mesh *m, struct sng_color *triangle_color, float alpha,
	union vec3 *eye_light_pos, GLuint texture_number, int do_depth, int do_cullface)
{
	enable_3d_viewport();

	if (!m->graph_ptr)
		return;

	struct mesh_gl_info *ptr = m->graph_ptr;

	if (do_depth) {
		glEnable(GL_DEPTH_TEST);
		glDepthFunc(GL_LESS);
	}
	if (do_cullface)
		glEnable(GL_CULL_FACE);
	glEnable(GL_TEXTURE_2D);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	glUseProgram(textured_shader.program_id);

	glUniformMatrix4fv(textured_shader.mvp_matrix_id, 1, GL_FALSE, &mat_mvp->m[0][0]);
	glUniform4f(textured_shader.tint_color_id, triangle_color->red,
		triangle_color->green, triangle_color->blue, alpha);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, texture_number);
	glUniform1i(textured_shader.texture_id, 0);

	glEnableVertexAttribArray(textured_shader.vertex_position_id);
	glBindBuffer(GL_ARRAY_BUFFER, ptr->vertex_buffer);
	glVertexAttribPointer(
		textured_shader.vertex_position_id, /* The attribute we want to configure */
		3,                           /* size */
		GL_FLOAT,                    /* type */
		GL_FALSE,                    /* normalized? */
		sizeof(struct vertex_buffer_data), /* stride */
		(void *)offsetof(struct vertex_buffer_data, position.v.x) /* array buffer offset */
	);

	glEnableVertexAttribArray(textured_shader.texture_coord_id);
	glBindBuffer(GL_ARRAY_BUFFER, ptr->triangle_vertex_buffer);
	glVertexAttribPointer(
		textured_shader.texture_coord_id,/* The attribute we want to configure */
		2,                            /* size */
		GL_FLOAT,                     /* type */
		GL_TRUE,                     /* normalized? */
		sizeof(struct vertex_triangle_buffer_data), /* stride */
		(void *)offsetof(struct vertex_triangle_buffer_data, texture_coord.v.x) /* array buffer offset */
	);

	glDrawArrays(GL_TRIANGLES, 0, m->ntriangles * 3);

	glDisableVertexAttribArray(textured_shader.vertex_position_id);
	glDisableVertexAttribArray(textured_shader.texture_coord_id);
	glUseProgram(0);

	if (do_depth)
		glDisable(GL_DEPTH_TEST);
	if (do_cullface)
		glDisable(GL_CULL_FACE);
	glDisable(GL_TEXTURE_2D);
	glDisable(GL_BLEND);
}

static void graph_dev_raster_texture_cubemap_lit(const struct mat44 *mat_mvp, const struct mat44 *mat_mv,
	const struct mat33 *mat_normal, struct mesh *m, struct sng_color *triangle_color,
	union vec3 *eye_light_pos, GLuint texture_number)
{
	enable_3d_viewport();

	if (!m->graph_ptr)
		return;

	struct mesh_gl_info *ptr = m->graph_ptr;

	glEnable(GL_TEXTURE_CUBE_MAP);
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_CULL_FACE);

	glUseProgram(textured_cubemap_lit_shader.program_id);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_CUBE_MAP, texture_number);
	glUniform1i(textured_cubemap_lit_shader.texture_id, 0);

	glUniformMatrix4fv(textured_cubemap_lit_shader.mv_matrix_id, 1, GL_FALSE, &mat_mv->m[0][0]);
	glUniformMatrix4fv(textured_cubemap_lit_shader.mvp_matrix_id, 1, GL_FALSE, &mat_mvp->m[0][0]);
	glUniformMatrix3fv(textured_cubemap_lit_shader.normal_matrix_id, 1, GL_FALSE, &mat_normal->m[0][0]);

	glUniform3f(textured_cubemap_lit_shader.light_pos_id, eye_light_pos->v.x, eye_light_pos->v.y, eye_light_pos->v.z);

	glEnableVertexAttribArray(textured_cubemap_lit_shader.vertex_position_id);
	glBindBuffer(GL_ARRAY_BUFFER, ptr->vertex_buffer);
	glVertexAttribPointer(
		textured_cubemap_lit_shader.vertex_position_id, /* The attribute we want to configure */
		3,                           /* size */
		GL_FLOAT,                    /* type */
		GL_FALSE,                    /* normalized? */
		sizeof(struct vertex_buffer_data), /* stride */
		(void *)offsetof(struct vertex_buffer_data, position.v.x) /* array buffer offset */
	);

	glEnableVertexAttribArray(textured_cubemap_lit_shader.vertex_normal_id);
	glBindBuffer(GL_ARRAY_BUFFER, ptr->triangle_vertex_buffer);
	glVertexAttribPointer(
		textured_cubemap_lit_shader.vertex_normal_id,  /* The attribute we want to configure */
		3,                            /* size */
		GL_FLOAT,                     /* type */
		GL_FALSE,                     /* normalized? */
		sizeof(struct vertex_triangle_buffer_data), /* stride */
		(void *)offsetof(struct vertex_triangle_buffer_data, normal.v.x) /* array buffer offset */
	);

	glDrawArrays(GL_TRIANGLES, 0, m->ntriangles * 3);

	glDisableVertexAttribArray(textured_cubemap_lit_shader.vertex_position_id);
	glDisableVertexAttribArray(textured_cubemap_lit_shader.vertex_normal_id);
	glUseProgram(0);

	glDisable(GL_TEXTURE_CUBE_MAP);
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_CULL_FACE);
}

static void graph_dev_raster_texture_lit(const struct mat44 *mat_mvp, const struct mat44 *mat_mv,
	const struct mat33 *mat_normal, struct mesh *m, struct sng_color *triangle_color,
	union vec3 *eye_light_pos, GLuint texture_number)
{
	enable_3d_viewport();

	if (!m->graph_ptr)
		return;

	struct mesh_gl_info *ptr = m->graph_ptr;

	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS);
	glEnable(GL_CULL_FACE);
	glEnable(GL_TEXTURE_2D);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	glUseProgram(textured_lit_shader.program_id);

	glUniformMatrix4fv(textured_lit_shader.mv_matrix_id, 1, GL_FALSE, &mat_mv->m[0][0]);
	glUniformMatrix4fv(textured_lit_shader.mvp_matrix_id, 1, GL_FALSE, &mat_mvp->m[0][0]);
	glUniformMatrix3fv(textured_lit_shader.normal_matrix_id, 1, GL_FALSE, &mat_normal->m[0][0]);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, texture_number);
	glUniform1i(textured_lit_shader.texture_id, 0);
	glUniform3f(textured_lit_shader.light_pos_id, eye_light_pos->v.x, eye_light_pos->v.y, eye_light_pos->v.z);

	glEnableVertexAttribArray(textured_lit_shader.vertex_position_id);
	glBindBuffer(GL_ARRAY_BUFFER, ptr->vertex_buffer);
	glVertexAttribPointer(
		textured_lit_shader.vertex_position_id, /* The attribute we want to configure */
		3,                           /* size */
		GL_FLOAT,                    /* type */
		GL_FALSE,                    /* normalized? */
		sizeof(struct vertex_buffer_data), /* stride */
		(void *)offsetof(struct vertex_buffer_data, position.v.x) /* array buffer offset */
	);

	glEnableVertexAttribArray(textured_lit_shader.vertex_normal_id);
	glBindBuffer(GL_ARRAY_BUFFER, ptr->triangle_vertex_buffer);
	glVertexAttribPointer(
		textured_lit_shader.vertex_normal_id,  /* The attribute we want to configure */
		3,                            /* size */
		GL_FLOAT,                     /* type */
		GL_FALSE,                     /* normalized? */
		sizeof(struct vertex_triangle_buffer_data), /* stride */
		(void *)offsetof(struct vertex_triangle_buffer_data, normal.v.x) /* array buffer offset */
	);

	glEnableVertexAttribArray(textured_lit_shader.texture_coord_id);
	glBindBuffer(GL_ARRAY_BUFFER, ptr->triangle_vertex_buffer);
	glVertexAttribPointer(
		textured_lit_shader.texture_coord_id,/* The attribute we want to configure */
		2,                            /* size */
		GL_FLOAT,                     /* type */
		GL_TRUE,                     /* normalized? */
		sizeof(struct vertex_triangle_buffer_data), /* stride */
		(void *)offsetof(struct vertex_triangle_buffer_data, texture_coord.v.x) /* array buffer offset */
	);

	glDrawArrays(GL_TRIANGLES, 0, m->ntriangles * 3);

	glDisableVertexAttribArray(textured_lit_shader.vertex_position_id);
	glDisableVertexAttribArray(textured_lit_shader.vertex_normal_id);
	glDisableVertexAttribArray(textured_lit_shader.texture_coord_id);
	glUseProgram(0);

	glDisable(GL_DEPTH_TEST);
	glDisable(GL_CULL_FACE);
	glDisable(GL_TEXTURE_2D);
	glDisable(GL_BLEND);
}

static void graph_dev_raster_single_color_lit(const struct mat44 *mat_mvp, const struct mat44 *mat_mv,
	const struct mat33 *mat_normal, struct mesh *m, struct sng_color *triangle_color, union vec3 *eye_light_pos)
{
	enable_3d_viewport();

	if (!m->graph_ptr)
		return;

	struct mesh_gl_info *ptr = m->graph_ptr;

	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS);
	glEnable(GL_CULL_FACE);

	glUseProgram(single_color_lit_shader.program_id);

	glUniformMatrix4fv(single_color_lit_shader.mv_matrix_id, 1, GL_FALSE, &mat_mv->m[0][0]);
	glUniformMatrix4fv(single_color_lit_shader.mvp_matrix_id, 1, GL_FALSE, &mat_mvp->m[0][0]);
	glUniformMatrix3fv(single_color_lit_shader.normal_matrix_id, 1, GL_FALSE, &mat_normal->m[0][0]);

	glUniform3f(single_color_lit_shader.color_id, triangle_color->red,
		triangle_color->green, triangle_color->blue);
	glUniform3f(single_color_lit_shader.light_pos_id, eye_light_pos->v.x, eye_light_pos->v.y, eye_light_pos->v.z);

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

	glDisableVertexAttribArray(single_color_lit_shader.vertex_position_id);
	glDisableVertexAttribArray(single_color_lit_shader.vertex_normal_id);
	glUseProgram(0);

	glDisable(GL_DEPTH_TEST);
	glDisable(GL_CULL_FACE);
}

static void graph_dev_raster_filled_wireframe_mesh(const struct mat44 *mat_mvp, struct mesh *m,
	struct sng_color *line_color, struct sng_color *triangle_color)
{
	enable_3d_viewport();

	if (!m->graph_ptr)
		return;

	struct mesh_gl_info *ptr = m->graph_ptr;

	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS);
	glEnable(GL_CULL_FACE);

	glUseProgram(filled_wireframe_shader.program_id);

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

	glDisableVertexAttribArray(filled_wireframe_shader.position_id);
	glUseProgram(0);

	glDisable(GL_DEPTH_TEST);
	glDisable(GL_CULL_FACE);
}

static void graph_dev_raster_trans_wireframe_mesh(const struct mat44 *mat_mvp, const struct mat44 *mat_mv,
					const struct mat33 *mat_normal, struct mesh *m, struct sng_color *line_color)
{
	enable_3d_viewport();

	if (!m->graph_ptr)
		return;

	struct mesh_gl_info *ptr = m->graph_ptr;

	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS);

	glUseProgram(trans_wireframe_shader.program_id);

	glUniformMatrix4fv(trans_wireframe_shader.mv_matrix_id, 1, GL_FALSE, &mat_mv->m[0][0]);
	glUniformMatrix4fv(trans_wireframe_shader.mvp_matrix_id, 1, GL_FALSE, &mat_mvp->m[0][0]);
	glUniformMatrix3fv(trans_wireframe_shader.normal_matrix_id, 1, GL_FALSE, &mat_normal->m[0][0]);
	glUniform3f(trans_wireframe_shader.color_id, line_color->red,
		line_color->green, line_color->blue);

	glEnableVertexAttribArray(trans_wireframe_shader.vertex_position_id);
	glBindBuffer(GL_ARRAY_BUFFER, ptr->wireframe_lines_vertex_buffer);
	glVertexAttribPointer(
		trans_wireframe_shader.vertex_position_id, /* The attribute we want to configure */
		3,                           /* size */
		GL_FLOAT,                    /* type */
		GL_FALSE,                    /* normalized? */
		sizeof(struct vertex_wireframe_line_buffer_data), /* stride */
		(void *)offsetof(struct vertex_wireframe_line_buffer_data, position.v.x) /* array buffer offset */
	);

	glEnableVertexAttribArray(trans_wireframe_shader.vertex_normal_id);
	glBindBuffer(GL_ARRAY_BUFFER, ptr->wireframe_lines_vertex_buffer);
	glVertexAttribPointer(
		trans_wireframe_shader.vertex_normal_id,    /* The attribute we want to configure */
		3,                            /* size */
		GL_FLOAT,                     /* type */
		GL_FALSE,                     /* normalized? */
		sizeof(struct vertex_wireframe_line_buffer_data), /* stride */
		(void *)offsetof(struct vertex_wireframe_line_buffer_data, normal.v.x) /* array buffer offset */
	);

	glDrawArrays(GL_LINES, 0, ptr->nwireframe_lines*2);

	glDisableVertexAttribArray(trans_wireframe_shader.vertex_position_id);
	glDisableVertexAttribArray(trans_wireframe_shader.vertex_normal_id);
	glUseProgram(0);

	glDisable(GL_DEPTH_TEST);
}

static void graph_dev_raster_line_mesh(struct entity *e, const struct mat44 *mat_mvp, struct mesh *m,
					struct sng_color *line_color)
{
	enable_3d_viewport();

	if (!m->graph_ptr)
		return;

	struct mesh_gl_info *ptr = m->graph_ptr;

	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS);

	GLuint vertex_position_id;

	if (e->material_type == MATERIAL_COLOR_BY_W) {
		struct material_color_by_w *m = e->material_ptr;

		glUseProgram(color_by_w_shader.program_id);

		glUniformMatrix4fv(color_by_w_shader.mvp_id, 1, GL_FALSE, &mat_mvp->m[0][0]);

		struct sng_color near_color = sng_get_color(m->near_color);
		glUniform3f(color_by_w_shader.near_color_id, near_color.red,
			near_color.green, near_color.blue);
		glUniform1f(color_by_w_shader.near_w_id, m->near_w);

		struct sng_color center_color = sng_get_color(m->center_color);
		glUniform3f(color_by_w_shader.center_color_id, center_color.red,
			center_color.green, center_color.blue);
		glUniform1f(color_by_w_shader.center_w_id, m->center_w);

		struct sng_color far_color = sng_get_color(m->far_color);
		glUniform3f(color_by_w_shader.far_color_id, far_color.red,
			far_color.green, far_color.blue);
		glUniform1f(color_by_w_shader.far_w_id, m->far_w);

		vertex_position_id = color_by_w_shader.position_id;
	} else {
		glUseProgram(single_color_shader.program_id);

		glUniformMatrix4fv(single_color_shader.mvp_matrix_id, 1, GL_FALSE, &mat_mvp->m[0][0]);
		glUniform4f(single_color_shader.color_id, line_color->red,
			line_color->green, line_color->blue, 1);

		vertex_position_id = single_color_shader.vertex_position_id;
	}

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

	glDrawArrays(GL_LINES, 0, ptr->nlines*2);

	glDisableVertexAttribArray(vertex_position_id);
	glUseProgram(0);

	glDisable(GL_DEPTH_TEST);
}

void graph_dev_raster_point_cloud_mesh(const struct mat44 *mat_mvp, struct mesh *m,
					struct sng_color *point_color, float pointSize)
{
	enable_3d_viewport();

	if (!m->graph_ptr)
		return;

	struct mesh_gl_info *ptr = m->graph_ptr;

	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS);
	glEnable(GL_VERTEX_PROGRAM_POINT_SIZE);

	glUseProgram(point_cloud_shader.program_id);

	glUniformMatrix4fv(point_cloud_shader.mvp_matrix_id, 1, GL_FALSE, &mat_mvp->m[0][0]);
	glUniform1f(point_cloud_shader.point_size_id, pointSize);
	glUniform3f(point_cloud_shader.color_id, point_color->red,
		point_color->green, point_color->blue);

	glEnableVertexAttribArray(point_cloud_shader.vertex_position_id);
	glBindBuffer(GL_ARRAY_BUFFER, ptr->vertex_buffer);
	glVertexAttribPointer(
		point_cloud_shader.vertex_position_id, /* The attribute we want to configure */
		3,                           /* size */
		GL_FLOAT,                    /* type */
		GL_FALSE,                    /* normalized? */
		sizeof(struct vertex_buffer_data), /* stride */
		(void *)offsetof(struct vertex_buffer_data, position.v.x) /* array buffer offset */
	);

	glDrawArrays(GL_POINTS, 0, ptr->npoints);

	glDisableVertexAttribArray(point_cloud_shader.vertex_position_id);
	glUseProgram(0);

	glDisable(GL_DEPTH_TEST);
	glDisable(GL_VERTEX_PROGRAM_POINT_SIZE);
}

static void graph_dev_draw_nebula(const struct mat44 *mat_mvp, const struct mat44 *mat_mv, const struct mat33 *mat_normal,
	struct entity *e, union vec3 *eye_light_pos)
{
	struct material_nebula *mt = e->material_ptr;

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

		float alpha = fabs(vec3_dot(&camera_normal, &camera_ent_vector));
		struct sng_color no_tint = { 1, 1, 1 };

		graph_dev_raster_texture(&mat_mvp_local_r, &mat_mv_local_r, &mat_normal_local_r,
			e->m, &no_tint, alpha, eye_light_pos, mt->texture_id[i], 0, 0);
	}
}

void graph_dev_draw_entity(struct entity_context *cx, struct entity *e, union vec3 *eye_light_pos,
	const struct mat44 *mat_mvp, const struct mat44 *mat_mv, const struct mat33 *mat_normal)
{
	draw_vertex_buffer_2d();

	struct sng_color line_color = sng_get_color(e->color);

	if (e->material_type == MATERIAL_NEBULA) {
		graph_dev_draw_nebula(mat_mvp, mat_mv, mat_normal, e, eye_light_pos);
		return;
	}

	switch (e->m->geometry_mode) {
	case MESH_GEOMETRY_TRIANGLES:
	{
		struct camera_info *c = &cx->camera;

		int filled_triangle = ((c->renderer & FLATSHADING_RENDERER) || (c->renderer & BLACK_TRIS))
					&& !(e->render_style & RENDER_NO_FILL);
		int outline_triangle = (c->renderer & WIREFRAME_RENDERER)
					|| (e->render_style & RENDER_WIREFRAME);

		int has_texture = 0;
		int has_cubemap_texture = 0;
		GLuint texture_id = 0;
		switch (e->material_type) {
		case MATERIAL_TEXTURE_MAPPED: {
				struct material_texture_mapped *mt = e->material_ptr;
				texture_id = mt->texture_id;
				has_texture = 1;
			}
			break;
		case MATERIAL_TEXTURE_MAPPED_UNLIT: {
				struct material_texture_mapped_unlit *mt = e->material_ptr;
				texture_id = mt->texture_id;
				has_texture = 1;
			}
			break;
		case MATERIAL_BILLBOARD: {
				struct material_billboard *mt = e->material_ptr;
				texture_id = mt->texture_id;
				has_texture = 1;
			}
			break;
		case MATERIAL_TEXTURE_CUBEMAP: {
				struct material_texture_cubemap *mt = e->material_ptr;
				texture_id = mt->texture_id;
				has_cubemap_texture = 1;
			}
			break;
		}

		if (filled_triangle) {
			struct sng_color triangle_color;
			if (cx->camera.renderer & BLACK_TRIS)
				triangle_color = sng_get_color(BLACK);
			else
				triangle_color = sng_get_color(240 + GRAY + (NSHADESOFGRAY * e->shadecolor) + 10);

			/* outline and filled */
			if (outline_triangle) {
				graph_dev_raster_filled_wireframe_mesh(mat_mvp, e->m, &line_color, &triangle_color);
			} else {
				if (has_texture)
					if (e->material_type == MATERIAL_TEXTURE_MAPPED)
						graph_dev_raster_texture_lit(mat_mvp, mat_mv, mat_normal,
							e->m, &triangle_color, eye_light_pos, texture_id);
					else {
						struct sng_color no_tint = { 1, 1, 1 };
						graph_dev_raster_texture(mat_mvp, mat_mv, mat_normal,
							e->m, &no_tint, 1.0, eye_light_pos, texture_id, 1, 1);
					}
				else if (has_cubemap_texture)
					graph_dev_raster_texture_cubemap_lit(mat_mvp, mat_mv, mat_normal,
						e->m, &triangle_color, eye_light_pos, texture_id);

				else
					graph_dev_raster_single_color_lit(mat_mvp, mat_mv, mat_normal,
						e->m, &triangle_color, eye_light_pos);
			}
		} else if (outline_triangle) {
			if (has_texture)
				if (e->material_type == MATERIAL_TEXTURE_MAPPED)
					graph_dev_raster_texture_lit(mat_mvp, mat_mv, mat_normal,
						e->m, &line_color, eye_light_pos, texture_id);
				else {
					struct sng_color no_tint = { 1, 1, 1 };
					graph_dev_raster_texture(mat_mvp, mat_mv, mat_normal,
						e->m, &no_tint, 1.0, eye_light_pos, texture_id, 1, 1);
				}
			else
				graph_dev_raster_trans_wireframe_mesh(mat_mvp, mat_mv,
					mat_normal, e->m, &line_color);
		}
		break;
	}
	case MESH_GEOMETRY_LINES:
		graph_dev_raster_line_mesh(e, mat_mvp, e->m, &line_color);
		break;

	case MESH_GEOMETRY_POINTS:
		graph_dev_raster_point_cloud_mesh(mat_mvp, e->m, &line_color, 1.0);
		break;
	}
}

void graph_dev_start_frame()
{
	/* printf("start frame\n"); */
}

void graph_dev_end_frame()
{
	draw_vertex_buffer_2d();
	/* printf("end frame\n"); */
}

void graph_dev_draw_line(float x1, float y1, float x2, float y2)
{
	make_room_in_vertex_buffer_2d(2);

	add_vertex_2d(x1, y1, sgc.hue, 255, GL_LINES);
	add_vertex_2d(x2, y2, sgc.hue, 255, GL_LINES);
}

void graph_dev_draw_rectangle(gboolean filled, float x, float y, float width, float height)
{
	int x2, y2;

	x2 = x + width;
	y2 = y + height;

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
		add_vertex_2d(x, y, sgc.hue, 255, GL_TRIANGLES);
		add_vertex_2d(x2, y2, sgc.hue, 255, GL_TRIANGLES);
		add_vertex_2d(x2, y, sgc.hue, 255, GL_TRIANGLES);

		/* triangle 2 = 0, 3, 2 */
		add_vertex_2d(x, y, sgc.hue, 255, GL_TRIANGLES);
		add_vertex_2d(x2, y2, sgc.hue, 255, GL_TRIANGLES);
		add_vertex_2d(x, y2, sgc.hue, 255, GL_TRIANGLES);
	} else {
		/* not filled */
		make_room_in_vertex_buffer_2d(5);

		add_vertex_2d(x, y, sgc.hue, 255, GL_LINE_STRIP);
		add_vertex_2d(x2, y, sgc.hue, 255, GL_LINE_STRIP);
		add_vertex_2d(x2, y2, sgc.hue, 255, GL_LINE_STRIP);
		add_vertex_2d(x, y2, sgc.hue, 255, GL_LINE_STRIP);
		add_vertex_2d(x, y, sgc.hue, 255, -1 /* primitive end */);
	}
}

void graph_dev_draw_point(float x, float y)
{
	make_room_in_vertex_buffer_2d(1);

	add_vertex_2d(x, y, sgc.hue, 255, GL_POINTS);
}

void graph_dev_draw_arc(gboolean filled, float x, float y, float width, float height, float angle1, float angle2)
{
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
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
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
}

static void setup_single_color_lit_shader(struct graph_dev_gl_single_color_lit_shader *shader)
{
	/* Create and compile our GLSL program from the shaders */
	shader->program_id = load_shaders("share/snis/shader/single-color-lit-per-vertex.vert",
		"share/snis/shader/single-color-lit-per-vertex.frag");

	/* Get a handle for our "MVP" uniform */
	shader->mvp_matrix_id = glGetUniformLocation(shader->program_id, "u_MVPMatrix");
	shader->mv_matrix_id = glGetUniformLocation(shader->program_id, "u_MVMatrix");
	shader->normal_matrix_id = glGetUniformLocation(shader->program_id, "u_NormalMatrix");
	shader->light_pos_id = glGetUniformLocation(shader->program_id, "u_LightPos");

	/* Get a handle for our buffers */
	shader->vertex_position_id = glGetAttribLocation(shader->program_id, "a_Position");
	shader->vertex_normal_id = glGetAttribLocation(shader->program_id, "a_Normal");
	shader->color_id = glGetUniformLocation(shader->program_id, "u_Color");
}

static void setup_textured_shader(struct graph_dev_gl_textured_shader *shader)
{
	/* Create and compile our GLSL program from the shaders */
	shader->program_id = load_shaders("share/snis/shader/textured.vert",
		"share/snis/shader/textured.frag");

	/* Get a handle for our "MVP" uniform */
	shader->mvp_matrix_id = glGetUniformLocation(shader->program_id, "u_MVPMatrix");
	shader->tint_color_id = glGetUniformLocation(shader->program_id, "u_TintColor");
	shader->texture_id = glGetUniformLocation(shader->program_id, "myTexture");

	/* Get a handle for our buffers */
	shader->vertex_position_id = glGetAttribLocation(shader->program_id, "a_Position");
	shader->texture_coord_id = glGetAttribLocation(shader->program_id, "a_tex_coord");
}

static void setup_textured_lit_shader(struct graph_dev_gl_textured_lit_shader *shader)
{
	/* Create and compile our GLSL program from the shaders */
	shader->program_id = load_shaders("share/snis/shader/textured-and-lit-per-vertex.vert",
		"share/snis/shader/textured-and-lit-per-vertex.frag");

	/* Get a handle for our "MVP" uniform */
	shader->mvp_matrix_id = glGetUniformLocation(shader->program_id, "u_MVPMatrix");
	shader->mv_matrix_id = glGetUniformLocation(shader->program_id, "u_MVMatrix");
	shader->normal_matrix_id = glGetUniformLocation(shader->program_id, "u_NormalMatrix");
	shader->light_pos_id = glGetUniformLocation(shader->program_id, "u_LightPos");
	shader->texture_id = glGetUniformLocation(shader->program_id, "myTexture");

	/* Get a handle for our buffers */
	shader->vertex_position_id = glGetAttribLocation(shader->program_id, "a_Position");
	shader->vertex_normal_id = glGetAttribLocation(shader->program_id, "a_Normal");
	shader->texture_coord_id = glGetAttribLocation(shader->program_id, "a_tex_coord");
	shader->color_id = glGetUniformLocation(shader->program_id, "u_Color");
}

static void setup_textured_cubemap_lit_shader(struct graph_dev_gl_textured_cubemap_lit_shader *shader)
{
	/* Create and compile our GLSL program from the shaders */
	shader->program_id = load_shaders("share/snis/shader/textured-cubemap-and-lit-per-vertex.vert",
		"share/snis/shader/textured-cubemap-and-lit-per-vertex.frag");

	/* Get a handle for our "MVP" uniform */
	shader->mvp_matrix_id = glGetUniformLocation(shader->program_id, "u_MVPMatrix");
	shader->mv_matrix_id = glGetUniformLocation(shader->program_id, "u_MVMatrix");
	shader->normal_matrix_id = glGetUniformLocation(shader->program_id, "u_NormalMatrix");
	shader->light_pos_id = glGetUniformLocation(shader->program_id, "u_LightPos");
	shader->texture_id = glGetUniformLocation(shader->program_id, "myTexture");

	/* Get a handle for our buffers */
	shader->vertex_position_id = glGetAttribLocation(shader->program_id, "a_Position");
	shader->vertex_normal_id = glGetAttribLocation(shader->program_id, "a_Normal");
}

static void setup_filled_wireframe_shader(struct graph_dev_gl_filled_wireframe_shader *shader)
{
	/* Create and compile our GLSL program from the shaders */
	shader->program_id = load_shaders("share/snis/shader/wireframe_filled.vert",
		"share/snis/shader/wireframe_filled.frag");

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

static void setup_trans_wireframe_shader(struct graph_dev_gl_trans_wireframe_shader *shader)
{
	/* Create and compile our GLSL program from the shaders */
	shader->program_id = load_shaders("share/snis/shader/wireframe_transparent.vert",
		"share/snis/shader/wireframe_transparent.frag");

	/* Get a handle for our "MVP" uniform */
	shader->mvp_matrix_id = glGetUniformLocation(shader->program_id, "u_MVPMatrix");
	shader->mv_matrix_id = glGetUniformLocation(shader->program_id, "u_MVMatrix");
	shader->normal_matrix_id = glGetUniformLocation(shader->program_id, "u_NormalMatrix");

	/* Get a handle for our buffers */
	shader->vertex_position_id = glGetAttribLocation(shader->program_id, "a_Position");
	shader->vertex_normal_id = glGetAttribLocation(shader->program_id, "a_Normal");
	shader->color_id = glGetUniformLocation(shader->program_id, "u_Color");
}

static void setup_single_color_shader(struct graph_dev_gl_single_color_shader *shader)
{
	/* Create and compile our GLSL program from the shaders */
	shader->program_id = load_shaders("share/snis/shader/single_color.vert",
		"share/snis/shader/single_color.frag");

	/* Get a handle for our "MVP" uniform */
	shader->mvp_matrix_id = glGetUniformLocation(shader->program_id, "u_MVPMatrix");

	/* Get a handle for our buffers */
	shader->vertex_position_id = glGetAttribLocation(shader->program_id, "a_Position");
	shader->color_id = glGetUniformLocation(shader->program_id, "u_Color");
}

static void setup_vertex_color_shader(struct graph_dev_gl_vertex_color_shader *shader)
{
	shader->program_id = load_shaders("share/snis/shader/per_vertex_color.vert",
		"share/snis/shader/per_vertex_color.frag");

	shader->mvp_matrix_id = glGetUniformLocation(shader->program_id, "u_MVPMatrix");

	shader->vertex_position_id = glGetAttribLocation(shader->program_id, "a_Position");
	shader->vertex_color_id = glGetAttribLocation(shader->program_id, "a_Color");
}

static void setup_point_cloud_shader(struct graph_dev_gl_point_cloud_shader *shader)
{
	/* Create and compile our GLSL program from the shaders */
	shader->program_id = load_shaders("share/snis/shader/point_cloud.vert",
		"share/snis/shader/point_cloud.frag");

	/* Get a handle for our "MVP" uniform */
	shader->mvp_matrix_id = glGetUniformLocation(shader->program_id, "u_MVPMatrix");

	/* Get a handle for our buffers */
	shader->vertex_position_id = glGetAttribLocation(shader->program_id, "a_Position");
	shader->point_size_id = glGetUniformLocation(shader->program_id, "u_PointSize");
	shader->color_id = glGetUniformLocation(shader->program_id, "u_Color");
}

static void setup_color_by_w_shader(struct graph_dev_gl_color_by_w_shader *shader)
{
	/* Create and compile our GLSL program from the shaders */
	shader->program_id = load_shaders("share/snis/shader/color_by_w.vert",
		"share/snis/shader/color_by_w.frag");

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
	/* Create and compile our GLSL program from the shaders */
	shader->program_id = load_shaders("share/snis/shader/skybox.vert",
		"share/snis/shader/skybox.frag");

	/* Get a handle for our "MVP" uniform */
	shader->mvp_id = glGetUniformLocation(shader->program_id, "MVP");
	shader->texture_id = glGetUniformLocation(shader->program_id, "texture");

	/* Get a handle for our buffers */
	shader->vertex_id = glGetAttribLocation(shader->program_id, "vertex");

	/* cube vertices in triangle strip for vertex buffer object */
	static const GLfloat cube_vertices[] = {
		-10.0f,  10.0f, -10.0f,
		-10.0f, -10.0f, -10.0f,
		10.0f, -10.0f, -10.0f,
		10.0f, -10.0f, -10.0f,
		10.0f,  10.0f, -10.0f,
		-10.0f,  10.0f, -10.0f,

		-10.0f, -10.0f,  10.0f,
		-10.0f, -10.0f, -10.0f,
		-10.0f,  10.0f, -10.0f,
		-10.0f,  10.0f, -10.0f,
		-10.0f,  10.0f,  10.0f,
		-10.0f, -10.0f,  10.0f,

		10.0f, -10.0f, -10.0f,
		10.0f, -10.0f,  10.0f,
		10.0f,  10.0f,  10.0f,
		10.0f,  10.0f,  10.0f,
		10.0f,  10.0f, -10.0f,
		10.0f, -10.0f, -10.0f,

		-10.0f, -10.0f,  10.0f,
		-10.0f,  10.0f,  10.0f,
		10.0f,  10.0f,  10.0f,
		10.0f,  10.0f,  10.0f,
		10.0f, -10.0f,  10.0f,
		-10.0f, -10.0f,  10.0f,

		-10.0f,  10.0f, -10.0f,
		10.0f,  10.0f, -10.0f,
		10.0f,  10.0f,  10.0f,
		10.0f,  10.0f,  10.0f,
		-10.0f,  10.0f,  10.0f,
		-10.0f,  10.0f, -10.0f,

		-10.0f, -10.0f, -10.0f,
		-10.0f, -10.0f,  10.0f,
		10.0f, -10.0f, -10.0f,
		10.0f, -10.0f, -10.0f,
		-10.0f, -10.0f,  10.0f,
		10.0f, -10.0f,  10.0f };

	glGenBuffers(1, &shader->vbo_cube_vertices);
	glBindBuffer(GL_ARRAY_BUFFER, shader->vbo_cube_vertices);
	glBufferData(GL_ARRAY_BUFFER, sizeof(cube_vertices), cube_vertices, GL_STATIC_DRAW);
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	shader->nvertices = sizeof(cube_vertices)/sizeof(GLfloat);

	shader->texture_loaded = 0;
}

static void setup_2d()
{
	glGenBuffers(1, &sgc.vertex_buffer_2d);
	glBindBuffer(GL_ARRAY_BUFFER, sgc.vertex_buffer_2d);
	glBufferData(GL_ARRAY_BUFFER, VERTEX_BUFFER_2D_SIZE, 0, GL_STREAM_DRAW);

	sgc.nvertex_2d = 0;
	sgc.vertex_buffer_2d_offset = 0;
}

int graph_dev_setup()
{
	if (glewInit() != GLEW_OK) {
		fprintf(stderr, "Failed to initialize GLEW\n");
		return -1;
	}
	if (!GLEW_VERSION_2_1) {
		fprintf(stderr, "Need atleast OpenGL 2.1\n");
		return -1;
	}
	printf("Initialized GLEW\n");

	setup_single_color_lit_shader(&single_color_lit_shader);
	setup_trans_wireframe_shader(&trans_wireframe_shader);
	setup_filled_wireframe_shader(&filled_wireframe_shader);
	setup_single_color_shader(&single_color_shader);
	setup_vertex_color_shader(&vertex_color_shader);
	setup_point_cloud_shader(&point_cloud_shader);
	setup_color_by_w_shader(&color_by_w_shader);
	setup_skybox_shader(&skybox_shader);
	setup_textured_shader(&textured_shader);
	setup_textured_lit_shader(&textured_lit_shader);
	setup_textured_cubemap_lit_shader(&textured_cubemap_lit_shader);

	/* after all the shaders are loaded */
	setup_2d();

	return 0;
}

unsigned int graph_dev_load_cubemap_texture(
	int is_inside,
	const char *texture_filename_pos_x,
	const char *texture_filename_neg_x,
	const char *texture_filename_pos_y,
	const char *texture_filename_neg_y,
	const char *texture_filename_pos_z,
	const char *texture_filename_neg_z)
{
	glEnable(GL_TEXTURE_CUBE_MAP);

	int ntextures = 6;

	static const GLint tex_pos[] = {
		GL_TEXTURE_CUBE_MAP_POSITIVE_X, GL_TEXTURE_CUBE_MAP_NEGATIVE_X,
		GL_TEXTURE_CUBE_MAP_POSITIVE_Y, GL_TEXTURE_CUBE_MAP_NEGATIVE_Y,
		GL_TEXTURE_CUBE_MAP_POSITIVE_Z, GL_TEXTURE_CUBE_MAP_NEGATIVE_Z };
	const char *tex_filename[] = {
		texture_filename_pos_x, texture_filename_neg_x,
		texture_filename_pos_y, texture_filename_neg_y,
		texture_filename_pos_z, texture_filename_neg_z };

	GLuint cube_texture_id;
	glGenTextures(1, &cube_texture_id);
	glBindTexture(GL_TEXTURE_CUBE_MAP, cube_texture_id);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

	char whynotz[100];
	int whynotlen = 100;

	int i;
	for (i = 0; i < ntextures; i++) {
		int tw, th, hasAlpha = 0;
		/* do horizontal invert if we are projecting on the inside */
		char *image_data = sng_load_png_texture(tex_filename[i], 0, is_inside, &tw, &th, &hasAlpha,
			whynotz, whynotlen);
		if (image_data) {
			glTexImage2D(tex_pos[i], 0, GL_RGBA, tw, th, 0, (hasAlpha ? GL_RGBA : GL_RGB),
					GL_UNSIGNED_BYTE, image_data);
			free(image_data);
		} else {
			printf("Unable to load skybox texture %d '%s': %s\n", i, tex_filename[i], whynotz);
		}
	}

	glDisable(GL_TEXTURE_CUBE_MAP);

	return (unsigned int) cube_texture_id;
}

/* returning unsigned int instead of GLuint so as not to leak opengl types out
 * Kind of ugly, but should not be dangerous.
 */
unsigned int graph_dev_load_texture(const char *filename)
{
	char whynotz[100];
	int whynotlen = 100;
	int tw, th, hasAlpha = 1;
	GLuint texture_number;

	glEnable(GL_TEXTURE_2D);
	glGenTextures(1, &texture_number);
	glBindTexture(GL_TEXTURE_2D, texture_number);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

	char *image_data = sng_load_png_texture(filename, 1, 0, &tw, &th, &hasAlpha,
						whynotz, whynotlen);
	if (image_data) {
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, tw, th, 0, (hasAlpha ? GL_RGBA : GL_RGB),
				GL_UNSIGNED_BYTE, image_data);
		free(image_data);
	} else {
		printf("Unable to load laserbolt texture '%s': %s\n", filename, whynotz);
	}
	glDisable(GL_TEXTURE_2D);
	return (unsigned int) texture_number;
}

void graph_dev_load_skybox_texture(
	const char *texture_filename_pos_x,
	const char *texture_filename_neg_x,
	const char *texture_filename_pos_y,
	const char *texture_filename_neg_y,
	const char *texture_filename_pos_z,
	const char *texture_filename_neg_z)
{
	if (skybox_shader.texture_loaded) {
		glDeleteTextures(1, &skybox_shader.cube_texture_id);
		skybox_shader.texture_loaded = 0;
	}

	skybox_shader.cube_texture_id = graph_dev_load_cubemap_texture(1, texture_filename_pos_x,
		texture_filename_neg_x, texture_filename_pos_y, texture_filename_neg_y, texture_filename_pos_z,
		texture_filename_neg_z);

	skybox_shader.texture_loaded = 1;
}

void graph_dev_draw_skybox(struct entity_context *cx, const struct mat44 *mat_vp)
{
	draw_vertex_buffer_2d();

	enable_3d_viewport();

	glEnable(GL_TEXTURE_CUBE_MAP);
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_CULL_FACE);

	glUseProgram(skybox_shader.program_id);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_CUBE_MAP, skybox_shader.cube_texture_id);
	glUniform1i(skybox_shader.texture_id, 0);

	glUniformMatrix4fv(skybox_shader.mvp_id, 1, GL_FALSE, &mat_vp->m[0][0]);

	glBindBuffer(GL_ARRAY_BUFFER, skybox_shader.vbo_cube_vertices);
	glEnableVertexAttribArray(skybox_shader.vertex_id);
	glVertexAttribPointer(skybox_shader.vertex_id, 3, GL_FLOAT, GL_FALSE, 0, 0);

	glDrawArrays(GL_TRIANGLES, 0, skybox_shader.nvertices);

	glDisableVertexAttribArray(skybox_shader.vertex_id);
	glDisable(GL_TEXTURE_CUBE_MAP);
	glUseProgram(0);
}

