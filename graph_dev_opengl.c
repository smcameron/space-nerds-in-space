#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <gtk/gtk.h>
#include <GL/glew.h>
#include <math.h>

#include "shader.h"
#include "vertex.h"
#include "triangle.h"
#include "mathutils.h"
#include "matrix.h"
#include "mesh.h"
#include "quat.h"
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
	GLuint triangle_normal_buffer;

	GLuint filled_wireframe_buffer;

	int nwireframe_lines;
	GLuint wireframe_vertex_buffer;
	GLuint wireframe_normal_buffer;
	GLuint texture_coord_buffer;

	int npoints;
	/* uses vertex_buffer for data */

	int nlines;
	/* uses vertex_buffer for data */
};

struct filled_wireframe_buffer_data {
	union vec3 position;
	union vec3 normal;
	union vec3 tvertex0;
	union vec3 tvertex1;
	union vec3 tvertex2;
	union vec3 edge_mask;
};

void mesh_graph_dev_cleanup(struct mesh *m)
{
	if (m->graph_ptr) {
		struct mesh_gl_info *ptr = m->graph_ptr;

		glDeleteBuffers(1, &ptr->vertex_buffer);
		glDeleteBuffers(1, &ptr->triangle_normal_buffer);
		glDeleteBuffers(1, &ptr->filled_wireframe_buffer);
		glDeleteBuffers(1, &ptr->wireframe_vertex_buffer);
		glDeleteBuffers(1, &ptr->wireframe_normal_buffer);

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
		size_t size = sizeof(GLfloat) * m->ntriangles * 9;
		size_t txsize = sizeof(GLfloat) * m->ntriangles * 3 * 4;
		GLfloat *g_vertex_buffer_data = malloc(size);
		GLfloat *g_normal_buffer_data = malloc(size);
		GLfloat *g_texture_coord_data = 0;

		if (m->tex)
			g_texture_coord_data = malloc(txsize);

		int wf_data_size = sizeof(struct filled_wireframe_buffer_data) * m->ntriangles * 3;
		struct filled_wireframe_buffer_data *g_filled_wf_buffer_data = malloc(wf_data_size);

		ptr->ntriangles = m->ntriangles;
		ptr->npoints = m->ntriangles * 3; /* can be rendered as a point cloud too */

		for (i = 0; i < m->ntriangles; i++) {
			int j = 0;
			for (j = 0; j < 3; j++) {
				int v_index = i * 9 + j * 3;
				g_vertex_buffer_data[v_index + 0] = m->t[i].v[j]->x;
				g_vertex_buffer_data[v_index + 1] = m->t[i].v[j]->y;
				g_vertex_buffer_data[v_index + 2] = m->t[i].v[j]->z;

				g_normal_buffer_data[v_index + 0] = m->t[i].vnormal[j].x;
				g_normal_buffer_data[v_index + 1] = m->t[i].vnormal[j].y;
				g_normal_buffer_data[v_index + 2] = m->t[i].vnormal[j].z;

				if (m->tex) {
					int d = i * 12 + j * 4;
					int s = i * 3 + j;

					g_texture_coord_data[d + 0] = m->tex[s].u;
					g_texture_coord_data[d + 1] = m->tex[s].v;
					g_texture_coord_data[d + 2] = 0.0f;
					g_texture_coord_data[d + 3] = 0.0f;
				}

				int wf_index = i * 3 + j;
				g_filled_wf_buffer_data[wf_index].position.v.x = m->t[i].v[j]->x;
				g_filled_wf_buffer_data[wf_index].position.v.y = m->t[i].v[j]->y;
				g_filled_wf_buffer_data[wf_index].position.v.z = m->t[i].v[j]->z;

				g_filled_wf_buffer_data[wf_index].normal.v.x = m->t[i].vnormal[j].x;
				g_filled_wf_buffer_data[wf_index].normal.v.y = m->t[i].vnormal[j].y;
				g_filled_wf_buffer_data[wf_index].normal.v.z = m->t[i].vnormal[j].z;

				g_filled_wf_buffer_data[wf_index].tvertex0.v.x = m->t[i].v[0]->x;
				g_filled_wf_buffer_data[wf_index].tvertex0.v.y = m->t[i].v[0]->y;
				g_filled_wf_buffer_data[wf_index].tvertex0.v.z = m->t[i].v[0]->z;

				g_filled_wf_buffer_data[wf_index].tvertex1.v.x = m->t[i].v[1]->x;
				g_filled_wf_buffer_data[wf_index].tvertex1.v.y = m->t[i].v[1]->y;
				g_filled_wf_buffer_data[wf_index].tvertex1.v.z = m->t[i].v[1]->z;

				g_filled_wf_buffer_data[wf_index].tvertex2.v.x = m->t[i].v[2]->x;
				g_filled_wf_buffer_data[wf_index].tvertex2.v.y = m->t[i].v[2]->y;
				g_filled_wf_buffer_data[wf_index].tvertex2.v.z = m->t[i].v[2]->z;

				g_filled_wf_buffer_data[wf_index].edge_mask.v.x = 0;
				g_filled_wf_buffer_data[wf_index].edge_mask.v.y = 0;
				g_filled_wf_buffer_data[wf_index].edge_mask.v.z = 0;

				/* bias the edge distance to make the coplanar edges not draw */
				if ((j == 1 || j == 2) && (m->t[i].flag & TRIANGLE_1_2_COPLANAR))
					g_filled_wf_buffer_data[wf_index].edge_mask.v.x = 1000;
				if ((j == 0 || j == 2) && (m->t[i].flag & TRIANGLE_0_2_COPLANAR))
					g_filled_wf_buffer_data[wf_index].edge_mask.v.y = 1000;
				if ((j == 0 || j == 1) && (m->t[i].flag & TRIANGLE_0_1_COPLANAR))
					g_filled_wf_buffer_data[wf_index].edge_mask.v.z = 1000;
			}
		}

		glGenBuffers(1, &ptr->vertex_buffer);
		glBindBuffer(GL_ARRAY_BUFFER, ptr->vertex_buffer);
		glBufferData(GL_ARRAY_BUFFER, size, g_vertex_buffer_data, GL_STATIC_DRAW);

		glGenBuffers(1, &ptr->triangle_normal_buffer);
		glBindBuffer(GL_ARRAY_BUFFER, ptr->triangle_normal_buffer);
		glBufferData(GL_ARRAY_BUFFER, size, g_normal_buffer_data, GL_STATIC_DRAW);

		glGenBuffers(1, &ptr->filled_wireframe_buffer);
		glBindBuffer(GL_ARRAY_BUFFER, ptr->filled_wireframe_buffer);
		glBufferData(GL_ARRAY_BUFFER, wf_data_size, g_filled_wf_buffer_data, GL_STATIC_DRAW);

		if (m->tex) {
			glGenBuffers(1, &ptr->texture_coord_buffer);
			glBindBuffer(GL_ARRAY_BUFFER, ptr->texture_coord_buffer);
			glBufferData(GL_ARRAY_BUFFER, txsize, g_texture_coord_data, GL_STATIC_DRAW);
			free(g_texture_coord_data);
		}

		free(g_vertex_buffer_data);
		free(g_normal_buffer_data);

		/* setup the line buffers used for wireframe */
		size = sizeof(GLfloat)*m->ntriangles * 3 * 3 * 2;
		g_vertex_buffer_data = malloc(size);
		g_normal_buffer_data = malloc(size);

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
					int index = 6 * ptr->nwireframe_lines;

					/* add the line from vertex j0 to j1 */
					g_vertex_buffer_data[index + 0] = m->t[i].v[j0]->x;
					g_vertex_buffer_data[index + 1] = m->t[i].v[j0]->y;
					g_vertex_buffer_data[index + 2] = m->t[i].v[j0]->z;

					g_vertex_buffer_data[index + 3] = m->t[i].v[j1]->x;
					g_vertex_buffer_data[index + 4] = m->t[i].v[j1]->y;
					g_vertex_buffer_data[index + 5] = m->t[i].v[j1]->z;

					/* the line normal is the same as the triangle */
					g_normal_buffer_data[index + 0] = g_normal_buffer_data[index + 3] = m->t[i].n.x;
					g_normal_buffer_data[index + 1] = g_normal_buffer_data[index + 4] = m->t[i].n.y;
					g_normal_buffer_data[index + 2] = g_normal_buffer_data[index + 5] = m->t[i].n.z;

					ptr->nwireframe_lines++;
				}
			}
		}

		glGenBuffers(1, &ptr->wireframe_vertex_buffer);
		glBindBuffer(GL_ARRAY_BUFFER, ptr->wireframe_vertex_buffer);
		glBufferData(GL_ARRAY_BUFFER, sizeof(GLfloat) * 6 * ptr->nwireframe_lines,
			g_vertex_buffer_data, GL_STATIC_DRAW);

		glGenBuffers(1, &ptr->wireframe_normal_buffer);
		glBindBuffer(GL_ARRAY_BUFFER, ptr->wireframe_normal_buffer);
		glBufferData(GL_ARRAY_BUFFER, sizeof(GLfloat) * 6 * ptr->nwireframe_lines,
			g_normal_buffer_data, GL_STATIC_DRAW);

		free(g_vertex_buffer_data);
		free(g_normal_buffer_data);
	}

	if (m->geometry_mode == MESH_GEOMETRY_LINES) {
		/* setup the line buffers */
		int i;
		size_t size = sizeof(GLfloat) * m->nvertices * 3 * 2;
		GLfloat *g_vertex_buffer_data = malloc(size);

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
						int index = 6 * ptr->nlines;
						g_vertex_buffer_data[index + 0] = v1->x;
						g_vertex_buffer_data[index + 1] = v1->y;
						g_vertex_buffer_data[index + 2] = v1->z;

						g_vertex_buffer_data[index + 3] = v2->x;
						g_vertex_buffer_data[index + 4] = v2->y;
						g_vertex_buffer_data[index + 5] = v2->z;

						ptr->nlines++;
					}
					v1 = v2;
					++vcurr;
				}
			} else {
				/* do dotted e->m->l[i].flag & MESH_LINE_DOTTED) */

				int index = 6 * ptr->nlines;
				g_vertex_buffer_data[index+0] = vstart->x;
				g_vertex_buffer_data[index+1] = vstart->y;
				g_vertex_buffer_data[index+2] = vstart->z;

				g_vertex_buffer_data[index+3] = vend->x;
				g_vertex_buffer_data[index+4] = vend->y;
				g_vertex_buffer_data[index+5] = vend->z;

				ptr->nlines++;
			}
		}

		glGenBuffers(1, &ptr->vertex_buffer);
		glBindBuffer(GL_ARRAY_BUFFER, ptr->vertex_buffer);
		glBufferData(GL_ARRAY_BUFFER, sizeof(GLfloat) * 6 * ptr->nlines, g_vertex_buffer_data, GL_STATIC_DRAW);

		ptr->npoints = ptr->nlines * 2; /* can be rendered as a point cloud too */

		free(g_vertex_buffer_data);
	}

	if (m->geometry_mode == MESH_GEOMETRY_POINTS) {
		/* setup the point buffers */
		int i;
		size_t size = sizeof(GLfloat) * m->nvertices * 3;
		GLfloat *g_vertex_buffer_data = malloc(size);

		ptr->npoints = m->nvertices;

		for (i = 0; i < m->nvertices; i++) {
			int index = i * 3;
			g_vertex_buffer_data[index + 0] = m->v[i].x;
			g_vertex_buffer_data[index + 1] = m->v[i].y;
			g_vertex_buffer_data[index + 2] = m->v[i].z;
		}

		glGenBuffers(1, &ptr->vertex_buffer);
		glBindBuffer(GL_ARRAY_BUFFER, ptr->vertex_buffer);
		glBufferData(GL_ARRAY_BUFFER, size, g_vertex_buffer_data, GL_STATIC_DRAW);

		free(g_vertex_buffer_data);
	}

	m->graph_ptr = ptr;
}

struct graph_dev_gl_solid_shader {
	GLuint programID;
	GLuint mvpMatrixID;
	GLuint mvMatrixID;
	GLuint normalMatrixID;
	GLuint vertexPositionID;
	GLuint vertexNormalID;
	GLuint lightPosID;
	GLuint colorID;
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
	GLuint programID;
	GLuint mvpMatrixID;
	GLuint mvMatrixID;
	GLuint normalMatrixID;
	GLuint vertexPositionID;
	GLuint vertexNormalID;
	GLuint colorID;
};

struct graph_dev_gl_single_color_shader {
	GLuint programID;
	GLuint mvpMatrixID;
	GLuint vertexPositionID;
	GLuint colorID;
};

struct graph_dev_gl_point_cloud_shader {
	GLuint programID;
	GLuint mvpMatrixID;
	GLuint vertexPositionID;
	GLuint pointSizeID;
	GLuint colorID;
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
	GLuint programID;
	GLuint mvpMatrixID;
	GLuint mvMatrixID;
	GLuint normalMatrixID;
	GLuint vertexPositionID;
	GLuint vertexNormalID;
	GLuint textureCoordID;
	GLuint lightPosID;
	GLuint colorID;
	GLuint texture_id; /* param to vertex shader */
};

struct graph_dev_gl_textured_lit_shader {
	GLuint programID;
	GLuint mvpMatrixID;
	GLuint mvMatrixID;
	GLuint normalMatrixID;
	GLuint vertexPositionID;
	GLuint vertexNormalID;
	GLuint textureCoordID;
	GLuint lightPosID;
	GLuint colorID;
	GLuint texture_id; /* param to vertex shader */
};

/* only use on high performance graphics card */
/*#define PER_PIXEL_LIGHT 1*/

/* store all the shader parameters */
static struct graph_dev_gl_solid_shader solid_shader;
static struct graph_dev_gl_trans_wireframe_shader trans_wireframe_shader;
static struct graph_dev_gl_filled_wireframe_shader filled_wireframe_shader;
static struct graph_dev_gl_single_color_shader single_color_shader;
static struct graph_dev_gl_point_cloud_shader point_cloud_shader;
static struct graph_dev_gl_skybox_shader skybox_shader;
static struct graph_dev_gl_color_by_w_shader color_by_w_shader;
static struct graph_dev_gl_textured_shader textured_shader;
static struct graph_dev_gl_textured_lit_shader textured_lit_shader;

static struct graph_dev_gl_context {
	int screen_x, screen_y;
	GdkColor *hue; /* current color */
	int alpha_blend;
	float alpha;

	int active_vp; /* 0=none, 1=2d, 2=3d */
	int vp_x_3d, vp_y_3d, vp_width_3d, vp_height_3d;
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

		glMatrixMode(GL_PROJECTION);
		glLoadIdentity();
		glOrtho(0.0f, sgc.screen_x, 0.0f, sgc.screen_y, -1.0, 1.0);
		glMatrixMode(GL_MODELVIEW);
		glLoadIdentity();

		sgc.active_vp = 1;
	}
}

static void enable_3d_viewport()
{
	if (sgc.active_vp != 2) {
		glMatrixMode(GL_PROJECTION);
		glLoadIdentity();
		glMatrixMode(GL_MODELVIEW);
		glPopMatrix();

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

static void graph_dev_raster_texture(const struct mat44 *mat_mvp, const struct mat44 *mat_mv,
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

	glUseProgram(textured_shader.programID);

	glUniformMatrix4fv(textured_shader.mvMatrixID, 1, GL_FALSE, &mat_mv->m[0][0]);
	glUniformMatrix4fv(textured_shader.mvpMatrixID, 1, GL_FALSE, &mat_mvp->m[0][0]);
	glUniformMatrix3fv(textured_shader.normalMatrixID, 1, GL_FALSE, &mat_normal->m[0][0]);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, texture_number);
	glUniform1i(skybox_shader.texture_id, 0);
	glUniform3f(textured_shader.colorID, triangle_color->red,
		triangle_color->green, triangle_color->blue);
	glUniform3f(textured_shader.lightPosID, eye_light_pos->v.x, eye_light_pos->v.y, eye_light_pos->v.z);

	glEnableVertexAttribArray(textured_shader.vertexPositionID);
	glBindBuffer(GL_ARRAY_BUFFER, ptr->vertex_buffer);
	glVertexAttribPointer(
		textured_shader.vertexPositionID, /* The attribute we want to configure */
		3,                           /* size */
		GL_FLOAT,                    /* type */
		GL_FALSE,                    /* normalized? */
		0,                           /* stride */
		(void *)0                    /* array buffer offset */
	);

	glEnableVertexAttribArray(textured_shader.vertexNormalID);
	glBindBuffer(GL_ARRAY_BUFFER, ptr->triangle_normal_buffer);
	glVertexAttribPointer(
		textured_shader.vertexNormalID,  /* The attribute we want to configure */
		3,                            /* size */
		GL_FLOAT,                     /* type */
		GL_FALSE,                     /* normalized? */
		0,                            /* stride */
		(void *)0                     /* array buffer offset */
	);

	glEnableVertexAttribArray(textured_shader.textureCoordID);
	glBindBuffer(GL_ARRAY_BUFFER, ptr->texture_coord_buffer);
	glVertexAttribPointer(
		textured_shader.textureCoordID,/* The attribute we want to configure */
		4,                            /* size */
		GL_FLOAT,                     /* type */
		GL_TRUE,                     /* normalized? */
		0,                            /* stride */
		(void *)0                     /* array buffer offset */
	);

	glDrawArrays(GL_TRIANGLES, 0, m->ntriangles * 3);

	glDisableVertexAttribArray(textured_shader.vertexPositionID);
	glDisableVertexAttribArray(textured_shader.vertexNormalID);
	glDisableVertexAttribArray(textured_shader.textureCoordID);
	glUseProgram(0);

	glDisable(GL_DEPTH_TEST);
	glDisable(GL_CULL_FACE);
	glDisable(GL_TEXTURE_2D);
	glDisable(GL_BLEND);
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

	glUseProgram(textured_lit_shader.programID);

	glUniformMatrix4fv(textured_lit_shader.mvMatrixID, 1, GL_FALSE, &mat_mv->m[0][0]);
	glUniformMatrix4fv(textured_lit_shader.mvpMatrixID, 1, GL_FALSE, &mat_mvp->m[0][0]);
	glUniformMatrix3fv(textured_lit_shader.normalMatrixID, 1, GL_FALSE, &mat_normal->m[0][0]);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, texture_number);
	glUniform3f(textured_lit_shader.colorID, triangle_color->red,
		triangle_color->green, triangle_color->blue);
	glUniform3f(textured_lit_shader.lightPosID, eye_light_pos->v.x, eye_light_pos->v.y, eye_light_pos->v.z);

	glEnableVertexAttribArray(textured_lit_shader.vertexPositionID);
	glBindBuffer(GL_ARRAY_BUFFER, ptr->vertex_buffer);
	glVertexAttribPointer(
		textured_lit_shader.vertexPositionID, /* The attribute we want to configure */
		3,                           /* size */
		GL_FLOAT,                    /* type */
		GL_FALSE,                    /* normalized? */
		0,                           /* stride */
		(void *)0                    /* array buffer offset */
	);

	glEnableVertexAttribArray(textured_lit_shader.vertexNormalID);
	glBindBuffer(GL_ARRAY_BUFFER, ptr->triangle_normal_buffer);
	glVertexAttribPointer(
		textured_lit_shader.vertexNormalID,  /* The attribute we want to configure */
		3,                            /* size */
		GL_FLOAT,                     /* type */
		GL_FALSE,                     /* normalized? */
		0,                            /* stride */
		(void *)0                     /* array buffer offset */
	);

	glEnableVertexAttribArray(textured_lit_shader.textureCoordID);
	glBindBuffer(GL_ARRAY_BUFFER, ptr->texture_coord_buffer);
	glVertexAttribPointer(
		textured_lit_shader.textureCoordID,/* The attribute we want to configure */
		4,                            /* size */
		GL_FLOAT,                     /* type */
		GL_TRUE,                     /* normalized? */
		0,                            /* stride */
		(void *)0                     /* array buffer offset */
	);

	glDrawArrays(GL_TRIANGLES, 0, m->ntriangles * 3);

	glDisableVertexAttribArray(textured_lit_shader.vertexPositionID);
	glDisableVertexAttribArray(textured_lit_shader.vertexNormalID);
	glDisableVertexAttribArray(textured_lit_shader.textureCoordID);
	glUseProgram(0);

	glDisable(GL_DEPTH_TEST);
	glDisable(GL_CULL_FACE);
	glDisable(GL_TEXTURE_2D);
	glDisable(GL_BLEND);
}

static void graph_dev_raster_solid_mesh(const struct mat44 *mat_mvp, const struct mat44 *mat_mv,
	const struct mat33 *mat_normal, struct mesh *m, struct sng_color *triangle_color, union vec3 *eye_light_pos)
{
	enable_3d_viewport();

	if (!m->graph_ptr)
		return;

	struct mesh_gl_info *ptr = m->graph_ptr;

	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS);
	glEnable(GL_CULL_FACE);

	glUseProgram(solid_shader.programID);

	glUniformMatrix4fv(solid_shader.mvMatrixID, 1, GL_FALSE, &mat_mv->m[0][0]);
	glUniformMatrix4fv(solid_shader.mvpMatrixID, 1, GL_FALSE, &mat_mvp->m[0][0]);
	glUniformMatrix3fv(solid_shader.normalMatrixID, 1, GL_FALSE, &mat_normal->m[0][0]);

	glUniform3f(solid_shader.colorID, triangle_color->red,
		triangle_color->green, triangle_color->blue);
	glUniform3f(solid_shader.lightPosID, eye_light_pos->v.x, eye_light_pos->v.y, eye_light_pos->v.z);

	glEnableVertexAttribArray(solid_shader.vertexPositionID);
	glBindBuffer(GL_ARRAY_BUFFER, ptr->vertex_buffer);
	glVertexAttribPointer(
		solid_shader.vertexPositionID, /* The attribute we want to configure */
		3,                           /* size */
		GL_FLOAT,                    /* type */
		GL_FALSE,                    /* normalized? */
		0,                           /* stride */
		(void *)0                    /* array buffer offset */
	);

	glEnableVertexAttribArray(solid_shader.vertexNormalID);
	glBindBuffer(GL_ARRAY_BUFFER, ptr->triangle_normal_buffer);
	glVertexAttribPointer(
		solid_shader.vertexNormalID,  /* The attribute we want to configure */
		3,                            /* size */
		GL_FLOAT,                     /* type */
		GL_FALSE,                     /* normalized? */
		0,                            /* stride */
		(void *)0                     /* array buffer offset */
	);

	glDrawArrays(GL_TRIANGLES, 0, m->ntriangles * 3);

	glDisableVertexAttribArray(solid_shader.vertexPositionID);
	glDisableVertexAttribArray(solid_shader.vertexNormalID);
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

	glUseProgram(filled_wireframe_shader.program_id);

	glUniform2f(filled_wireframe_shader.viewport_id, sgc.vp_width_3d, sgc.vp_height_3d);
	glUniformMatrix4fv(filled_wireframe_shader.mvp_matrix_id, 1, GL_FALSE, &mat_mvp->m[0][0]);

	glUniform3f(filled_wireframe_shader.line_color_id, line_color->red,
		line_color->green, line_color->blue);
	glUniform3f(filled_wireframe_shader.triangle_color_id, triangle_color->red,
		triangle_color->green, triangle_color->blue);

	glEnableVertexAttribArray(filled_wireframe_shader.position_id);
	glBindBuffer(GL_ARRAY_BUFFER, ptr->filled_wireframe_buffer);
	glVertexAttribPointer(
		filled_wireframe_shader.position_id,
		3,
		GL_FLOAT,
		GL_FALSE,
		sizeof(struct filled_wireframe_buffer_data),
		(void *)offsetof(struct filled_wireframe_buffer_data, position.v.x)
	);

	glEnableVertexAttribArray(filled_wireframe_shader.tvertex0_id);
	glBindBuffer(GL_ARRAY_BUFFER, ptr->filled_wireframe_buffer);
	glVertexAttribPointer(
		filled_wireframe_shader.tvertex0_id,
		3,
		GL_FLOAT,
		GL_FALSE,
		sizeof(struct filled_wireframe_buffer_data),
		(void *)offsetof(struct filled_wireframe_buffer_data, tvertex0.v.x)
	);

	glEnableVertexAttribArray(filled_wireframe_shader.tvertex1_id);
	glBindBuffer(GL_ARRAY_BUFFER, ptr->filled_wireframe_buffer);
	glVertexAttribPointer(
		filled_wireframe_shader.tvertex1_id,
		3,
		GL_FLOAT,
		GL_FALSE,
		sizeof(struct filled_wireframe_buffer_data),
		(void *)offsetof(struct filled_wireframe_buffer_data, tvertex1.v.x)
	);

	glEnableVertexAttribArray(filled_wireframe_shader.tvertex2_id);
	glBindBuffer(GL_ARRAY_BUFFER, ptr->filled_wireframe_buffer);
	glVertexAttribPointer(
		filled_wireframe_shader.tvertex2_id,
		3,
		GL_FLOAT,
		GL_FALSE,
		sizeof(struct filled_wireframe_buffer_data),
		(void *)offsetof(struct filled_wireframe_buffer_data, tvertex2.v.x)
	);

	glEnableVertexAttribArray(filled_wireframe_shader.edge_mask_id);
	glBindBuffer(GL_ARRAY_BUFFER, ptr->filled_wireframe_buffer);
	glVertexAttribPointer(
		filled_wireframe_shader.edge_mask_id,
		3,
		GL_FLOAT,
		GL_FALSE,
		sizeof(struct filled_wireframe_buffer_data),
		(void *)offsetof(struct filled_wireframe_buffer_data, edge_mask.v.x)
	);

	glDrawArrays(GL_TRIANGLES, 0, ptr->ntriangles*3);

	glDisableVertexAttribArray(filled_wireframe_shader.position_id);
	glUseProgram(0);

	glDisable(GL_DEPTH_TEST);
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

	glUseProgram(trans_wireframe_shader.programID);

	glUniformMatrix4fv(trans_wireframe_shader.mvMatrixID, 1, GL_FALSE, &mat_mv->m[0][0]);
	glUniformMatrix4fv(trans_wireframe_shader.mvpMatrixID, 1, GL_FALSE, &mat_mvp->m[0][0]);
	glUniformMatrix3fv(trans_wireframe_shader.normalMatrixID, 1, GL_FALSE, &mat_normal->m[0][0]);
	glUniform3f(trans_wireframe_shader.colorID, line_color->red,
		line_color->green, line_color->blue);

	glEnableVertexAttribArray(trans_wireframe_shader.vertexPositionID);
	glBindBuffer(GL_ARRAY_BUFFER, ptr->wireframe_vertex_buffer);
	glVertexAttribPointer(
		trans_wireframe_shader.vertexPositionID, /* The attribute we want to configure */
		3,                           /* size */
		GL_FLOAT,                    /* type */
		GL_FALSE,                    /* normalized? */
		0,                           /* stride */
		(void *)0                    /* array buffer offset */
	);

	glEnableVertexAttribArray(trans_wireframe_shader.vertexNormalID);
	glBindBuffer(GL_ARRAY_BUFFER, ptr->wireframe_normal_buffer);
	glVertexAttribPointer(
		trans_wireframe_shader.vertexNormalID,    /* The attribute we want to configure */
		3,                            /* size */
		GL_FLOAT,                     /* type */
		GL_FALSE,                     /* normalized? */
		0,                            /* stride */
		(void *)0                     /* array buffer offset */
	);

	glDrawArrays(GL_LINES, 0, ptr->nwireframe_lines*2);

	glDisableVertexAttribArray(trans_wireframe_shader.vertexPositionID);
	glDisableVertexAttribArray(trans_wireframe_shader.vertexNormalID);
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
		glUseProgram(single_color_shader.programID);

		glUniformMatrix4fv(single_color_shader.mvpMatrixID, 1, GL_FALSE, &mat_mvp->m[0][0]);
		glUniform3f(single_color_shader.colorID, line_color->red,
			line_color->green, line_color->blue);

		vertex_position_id = single_color_shader.vertexPositionID;
	}

	glEnableVertexAttribArray(vertex_position_id);
	glBindBuffer(GL_ARRAY_BUFFER, ptr->vertex_buffer);
	glVertexAttribPointer(
		vertex_position_id, /* The attribute we want to configure */
		3,                           /* size */
		GL_FLOAT,                    /* type */
		GL_FALSE,                    /* normalized? */
		0,                           /* stride */
		(void *)0                    /* array buffer offset */
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

	glUseProgram(point_cloud_shader.programID);

	glUniformMatrix4fv(point_cloud_shader.mvpMatrixID, 1, GL_FALSE, &mat_mvp->m[0][0]);
	glUniform1f(point_cloud_shader.pointSizeID, pointSize);
	glUniform3f(point_cloud_shader.colorID, point_color->red,
		point_color->green, point_color->blue);

	glEnableVertexAttribArray(point_cloud_shader.vertexPositionID);
	glBindBuffer(GL_ARRAY_BUFFER, ptr->vertex_buffer);
	glVertexAttribPointer(
		point_cloud_shader.vertexPositionID, /* The attribute we want to configure */
		3,                           /* size */
		GL_FLOAT,                    /* type */
		GL_FALSE,                    /* normalized? */
		0,                           /* stride */
		(void *)0                    /* array buffer offset */
	);

	glDrawArrays(GL_POINTS, 0, ptr->npoints);

	glDisableVertexAttribArray(point_cloud_shader.vertexPositionID);
	glUseProgram(0);

	glDisable(GL_DEPTH_TEST);
	glDisable(GL_VERTEX_PROGRAM_POINT_SIZE);
}

void graph_dev_draw_entity(struct entity_context *cx, struct entity *e, union vec3 *eye_light_pos,
	const struct mat44 *mat_mvp, const struct mat44 *mat_mv, const struct mat33 *mat_normal)
{
	struct sng_color line_color = sng_get_color(e->color);

	switch (e->m->geometry_mode) {
	case MESH_GEOMETRY_TRIANGLES:
	{
		struct camera_info *c = &cx->camera;

		int filled_triangle = ((c->renderer & FLATSHADING_RENDERER) || (c->renderer & BLACK_TRIS))
					&& !(e->render_style & RENDER_NO_FILL);
		int outline_triangle = (c->renderer & WIREFRAME_RENDERER)
					|| (e->render_style & RENDER_WIREFRAME);

		int has_texture = 0;
		GLuint texture_id = 0;
		if (e->material_type == MATERIAL_TEXTURE_MAPPED ||
				e->material_type == MATERIAL_BILLBOARD) {
			has_texture = 1;
			if (e->material_type == MATERIAL_TEXTURE_MAPPED) {
				struct material_texture_mapped *mt = e->material_ptr;
				texture_id = mt->texture_id;
			} else {
				struct material_billboard *mt = e->material_ptr;
				texture_id = mt->texture_id;
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
				graph_dev_raster_filled_wireframe_mesh(mat_mvp, e->m, &line_color, &triangle_color);
			} else {
				if (has_texture)
					if (e->material_type == MATERIAL_TEXTURE_MAPPED)
						graph_dev_raster_texture_lit(mat_mvp, mat_mv, mat_normal,
							e->m, &triangle_color, eye_light_pos, texture_id);
					else
						graph_dev_raster_texture(mat_mvp, mat_mv, mat_normal,
							e->m, &triangle_color, eye_light_pos, texture_id);
				else
					graph_dev_raster_solid_mesh(mat_mvp, mat_mv, mat_normal,
						e->m, &triangle_color, eye_light_pos);
			}
		} else if (outline_triangle) {
			if (has_texture)
				if (e->material_type == MATERIAL_TEXTURE_MAPPED)
					graph_dev_raster_texture_lit(mat_mvp, mat_mv, mat_normal,
						e->m, &line_color, eye_light_pos, texture_id);
				else
					graph_dev_raster_texture(mat_mvp, mat_mv, mat_normal,
						e->m, &line_color, eye_light_pos, texture_id);
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

void graph_dev_draw_line(float x1, float y1, float x2, float y2)
{
	enable_2d_viewport();

	glBegin(GL_LINES);
	glColor3us(sgc.hue->red, sgc.hue->green, sgc.hue->blue);
	glVertex2f(x1, sgc.screen_y - y1);
	glVertex2f(x2, sgc.screen_y - y2);
	glEnd();
}

void graph_dev_draw_rectangle(gboolean filled, float x, float y, float width, float height)
{
	enable_2d_viewport();

	int x2, y2;

	x2 = x + width;
	y2 = y + height;
	if (filled)
		glBegin(GL_POLYGON);
	else
		glBegin(GL_LINE_STRIP);
	glColor3us(sgc.hue->red, sgc.hue->green, sgc.hue->blue);
	glVertex2f(x, sgc.screen_y - y);
	glVertex2f(x2, sgc.screen_y - y);
	glVertex2f(x2, sgc.screen_y - y2);
	glVertex2f(x, sgc.screen_y - y2);
	if (!filled)
		glVertex2f(x, sgc.screen_y - y);
	glEnd();
}

void graph_dev_draw_point(float x, float y)
{
	enable_2d_viewport();

	glBegin(GL_POINTS);
	glColor3us(sgc.hue->red, sgc.hue->green, sgc.hue->blue);
	glVertex2f(x, sgc.screen_y - y);
	glEnd();
}

void graph_dev_draw_arc(gboolean filled, float x, float y, float width, float height, float angle1, float angle2)
{
	enable_2d_viewport();

	float max_angle_delta = 2.0 * M_PI / 180.0; /*some ratio to height and width? */
	float rx = width/2.0;
	float ry = height/2.0;
	float cx = x + rx;
	float cy = y + ry;

	int i;

	int segments = (int)((angle2 - angle1) / max_angle_delta) + 1;
	float delta = (angle2 - angle1) / segments;

	if (sgc.alpha_blend) {
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glColor4us(sgc.hue->red, sgc.hue->green, sgc.hue->blue, sgc.alpha*65535.0);
	} else {
		glColor3us(sgc.hue->red, sgc.hue->green, sgc.hue->blue);
	}

	if (filled)
		glBegin(GL_TRIANGLES);
	else
		glBegin(GL_LINE_STRIP);

	float x1 = 0, y1 = 0;
	for (i = 0; i <= segments; i++) {
		float a = angle1 + delta * (float)i;
		float x2 = cx + cos(a) * rx;
		float y2 = cy + sin(a) * ry;

		if (!filled || i > 0) {
			glVertex2f(x2, sgc.screen_y - y2);
			if (filled) {
				glVertex2f(x1, sgc.screen_y - y1);
				glVertex2f(cx, sgc.screen_y - cy);
			}
		}
		x1 = x2;
		y1 = y2;
	}

	if (sgc.alpha_blend)
		glDisable(GL_BLEND);

	glEnd();
}

static void setup_solid_shader(struct graph_dev_gl_solid_shader *shader)
{
	/* Create and compile our GLSL program from the shaders */
#ifdef PER_PIXEL_LIGHT
	shader->programID = load_shaders("share/snis/shader/solid_per_pixel_light.vert",
		"share/snis/shader/solid_per_pixel_light.frag");
#else
	shader->programID = load_shaders("share/snis/shader/solid_per_vertex_light.vert",
		"share/snis/shader/solid_per_vertex_light.frag");
#endif

	/* Get a handle for our "MVP" uniform */
	shader->mvpMatrixID = glGetUniformLocation(shader->programID, "u_MVPMatrix");
	shader->mvMatrixID = glGetUniformLocation(shader->programID, "u_MVMatrix");
	shader->normalMatrixID = glGetUniformLocation(shader->programID, "u_NormalMatrix");
	shader->lightPosID = glGetUniformLocation(shader->programID, "u_LightPos");

	/* Get a handle for our buffers */
	shader->vertexPositionID = glGetAttribLocation(shader->programID, "a_Position");
	shader->vertexNormalID = glGetAttribLocation(shader->programID, "a_Normal");
	shader->colorID = glGetUniformLocation(shader->programID, "u_Color");
}

static void setup_textured_shader(struct graph_dev_gl_textured_shader *shader)
{
	/* Create and compile our GLSL program from the shaders */
#ifdef PER_PIXEL_LIGHT
	shader->programID = load_shaders("share/snis/shader/textured-per-pixel-light.vert",
		"share/snis/shader/textured-per-pixel-light.frag");
#else
	shader->programID = load_shaders("share/snis/shader/textured-per-vertex-light.vert",
		"share/snis/shader/textured-per-vertex-light.frag");
#endif

	/* Get a handle for our "MVP" uniform */
	shader->mvpMatrixID = glGetUniformLocation(shader->programID, "u_MVPMatrix");
	shader->mvMatrixID = glGetUniformLocation(shader->programID, "u_MVMatrix");
	shader->normalMatrixID = glGetUniformLocation(shader->programID, "u_NormalMatrix");
	shader->lightPosID = glGetUniformLocation(shader->programID, "u_LightPos");
	shader->texture_id = glGetUniformLocation(shader->programID, "myTexture");

	/* Get a handle for our buffers */
	shader->vertexPositionID = glGetAttribLocation(shader->programID, "a_Position");
	shader->vertexNormalID = glGetAttribLocation(shader->programID, "a_Normal");
	shader->textureCoordID = glGetAttribLocation(shader->programID, "a_tex_coord");
	shader->colorID = glGetUniformLocation(shader->programID, "u_Color");
}

static void setup_textured_lit_shader(struct graph_dev_gl_textured_lit_shader *shader)
{
	/* Create and compile our GLSL program from the shaders */
	shader->programID = load_shaders("share/snis/shader/textured-and-lit-per-vertex.vert",
		"share/snis/shader/textured-and-lit-per-vertex.frag");

	/* Get a handle for our "MVP" uniform */
	shader->mvpMatrixID = glGetUniformLocation(shader->programID, "u_MVPMatrix");
	shader->mvMatrixID = glGetUniformLocation(shader->programID, "u_MVMatrix");
	shader->normalMatrixID = glGetUniformLocation(shader->programID, "u_NormalMatrix");
	shader->lightPosID = glGetUniformLocation(shader->programID, "u_LightPos");
	shader->texture_id = glGetUniformLocation(shader->programID, "myTexture");

	/* Get a handle for our buffers */
	shader->vertexPositionID = glGetAttribLocation(shader->programID, "a_Position");
	shader->vertexNormalID = glGetAttribLocation(shader->programID, "a_Normal");
	shader->textureCoordID = glGetAttribLocation(shader->programID, "a_tex_coord");
	shader->colorID = glGetUniformLocation(shader->programID, "u_Color");
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
	shader->programID = load_shaders("share/snis/shader/wireframe_transparent.vert",
		"share/snis/shader/wireframe_transparent.frag");

	/* Get a handle for our "MVP" uniform */
	shader->mvpMatrixID = glGetUniformLocation(shader->programID, "u_MVPMatrix");
	shader->mvMatrixID = glGetUniformLocation(shader->programID, "u_MVMatrix");
	shader->normalMatrixID = glGetUniformLocation(shader->programID, "u_NormalMatrix");

	/* Get a handle for our buffers */
	shader->vertexPositionID = glGetAttribLocation(shader->programID, "a_Position");
	shader->vertexNormalID = glGetAttribLocation(shader->programID, "a_Normal");
	shader->colorID = glGetUniformLocation(shader->programID, "u_Color");
}

static void setup_single_color_shader(struct graph_dev_gl_single_color_shader *shader)
{
	/* Create and compile our GLSL program from the shaders */
	shader->programID = load_shaders("share/snis/shader/single_color.vert",
		"share/snis/shader/single_color.frag");

	/* Get a handle for our "MVP" uniform */
	shader->mvpMatrixID = glGetUniformLocation(shader->programID, "u_MVPMatrix");

	/* Get a handle for our buffers */
	shader->vertexPositionID = glGetAttribLocation(shader->programID, "a_Position");
	shader->colorID = glGetUniformLocation(shader->programID, "u_Color");
}

static void setup_point_cloud_shader(struct graph_dev_gl_point_cloud_shader *shader)
{
	/* Create and compile our GLSL program from the shaders */
	shader->programID = load_shaders("share/snis/shader/point_cloud.vert",
		"share/snis/shader/point_cloud.frag");

	/* Get a handle for our "MVP" uniform */
	shader->mvpMatrixID = glGetUniformLocation(shader->programID, "u_MVPMatrix");

	/* Get a handle for our buffers */
	shader->vertexPositionID = glGetAttribLocation(shader->programID, "a_Position");
	shader->pointSizeID = glGetUniformLocation(shader->programID, "u_PointSize");
	shader->colorID = glGetUniformLocation(shader->programID, "u_Color");
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

	setup_solid_shader(&solid_shader);
	setup_trans_wireframe_shader(&trans_wireframe_shader);
	setup_filled_wireframe_shader(&filled_wireframe_shader);
	setup_single_color_shader(&single_color_shader);
	setup_point_cloud_shader(&point_cloud_shader);
	setup_color_by_w_shader(&color_by_w_shader);
	setup_skybox_shader(&skybox_shader);
	setup_textured_shader(&textured_shader);
	setup_textured_lit_shader(&textured_lit_shader);

	return 0;
}

void graph_dev_load_skybox_texture(
	const char *texture_filename_pos_x,
	const char *texture_filename_neg_x,
	const char *texture_filename_pos_y,
	const char *texture_filename_neg_y,
	const char *texture_filename_pos_z,
	const char *texture_filename_neg_z)
{
	glEnable(GL_TEXTURE_CUBE_MAP);

	if (skybox_shader.texture_loaded) {
		glDeleteTextures(1, &skybox_shader.cube_texture_id);
		skybox_shader.texture_loaded = 0;
	}

	int ntextures = 6;

	static const GLint tex_pos[] = {
		GL_TEXTURE_CUBE_MAP_POSITIVE_X, GL_TEXTURE_CUBE_MAP_NEGATIVE_X,
		GL_TEXTURE_CUBE_MAP_POSITIVE_Y, GL_TEXTURE_CUBE_MAP_NEGATIVE_Y,
		GL_TEXTURE_CUBE_MAP_POSITIVE_Z, GL_TEXTURE_CUBE_MAP_NEGATIVE_Z };
	const char *tex_filename[] = {
		texture_filename_pos_x, texture_filename_neg_x,
		texture_filename_pos_y, texture_filename_neg_y,
		texture_filename_pos_z, texture_filename_neg_z };

	glGenTextures(1, &skybox_shader.cube_texture_id);
	glBindTexture(GL_TEXTURE_CUBE_MAP, skybox_shader.cube_texture_id);
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
		char *image_data = sng_load_png_texture(tex_filename[i], 0, 1, &tw, &th, &hasAlpha, whynotz, whynotlen);
		if (image_data) {
			glTexImage2D(tex_pos[i], 0, GL_RGBA, tw, th, 0, (hasAlpha ? GL_RGBA : GL_RGB),
					GL_UNSIGNED_BYTE, image_data);
			free(image_data);
		} else {
			printf("Unable to load skybox texture %d '%s': %s\n", i, tex_filename[i], whynotz);
		}
	}

	skybox_shader.texture_loaded = 1;

	glDisable(GL_TEXTURE_CUBE_MAP);
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

void graph_dev_draw_skybox(struct entity_context *cx, const struct mat44 *mat_vp)
{
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

