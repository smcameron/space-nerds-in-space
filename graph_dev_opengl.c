#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <gtk/gtk.h>
#include <GL/glew.h>

#include "shader.h"
#include "vertex.h"
#include "triangle.h"
#include "mathutils.h"
#include "matrix.h"
#include "math.h"
#include "mesh.h"
#include "quat.h"
#include "snis_graph.h"
#include "graph_dev.h"
#include "entity.h"
#include "entity_private.h"

struct mesh_gl_info {
	/* common buffer to hold vertex positions */
	GLuint vertex_buffer;

	int ntriangles;
	/* uses vertex_buffer for data */
	GLuint triangle_normal_buffer;

	int nwireframe_lines;
	GLuint wireframe_vertex_buffer;
	GLuint wireframe_normal_buffer;

	int npoints;
	/* uses vertex_buffer for data */

	int nlines;
	/* uses vertex_buffer for data */
};

void mesh_graph_dev_cleanup(struct mesh *m)
{
	if (m->graph_ptr) {
		struct mesh_gl_info *ptr = m->graph_ptr;

		glDeleteBuffers(1, &ptr->vertex_buffer);
		glDeleteBuffers(1, &ptr->triangle_normal_buffer);
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
		GLfloat *g_vertex_buffer_data = malloc(size);
		GLfloat *g_normal_buffer_data = malloc(size);

		ptr->ntriangles = m->ntriangles;
		ptr->npoints = m->ntriangles * 3; /* can be rendered as a point cloud too */

		for (i = 0; i < m->ntriangles; i++) {
			int j = 0;
			for (j = 0; j < 3; j++) {
				int index = i * 9 + j * 3;
				g_vertex_buffer_data[index + 0] = m->t[i].v[j]->x;
				g_vertex_buffer_data[index + 1] = m->t[i].v[j]->y;
				g_vertex_buffer_data[index + 2] = m->t[i].v[j]->z;

				g_normal_buffer_data[index + 0] = m->t[i].vnormal[j].x;
				g_normal_buffer_data[index + 1] = m->t[i].vnormal[j].y;
				g_normal_buffer_data[index + 2] = m->t[i].vnormal[j].z;
			}
		}

		glGenBuffers(1, &ptr->vertex_buffer);
		glBindBuffer(GL_ARRAY_BUFFER, ptr->vertex_buffer);
		glBufferData(GL_ARRAY_BUFFER, size, g_vertex_buffer_data, GL_STATIC_DRAW);

		glGenBuffers(1, &ptr->triangle_normal_buffer);
		glBindBuffer(GL_ARRAY_BUFFER, ptr->triangle_normal_buffer);
		glBufferData(GL_ARRAY_BUFFER, size, g_normal_buffer_data, GL_STATIC_DRAW);

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

/* only use on high performance graphics card */
/*#define PER_PIXEL_LIGHT 1*/

/* store all the shader parameters */
static struct graph_dev_gl_solid_shader solid_shader;
static struct graph_dev_gl_trans_wireframe_shader trans_wireframe_shader;
static struct graph_dev_gl_single_color_shader single_color_shader;
static struct graph_dev_gl_point_cloud_shader point_cloud_shader;

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

static void graph_dev_raster_solid_mesh(const float *mat_mvp, const float *mat_mv, const float *mat_normal,
	struct mesh *m, float r, float g, float b, union vec3 *eye_light_pos)
{
	enable_3d_viewport();

	if (!m->graph_ptr)
		return;

	struct mesh_gl_info *ptr = m->graph_ptr;

	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS);
	glEnable(GL_CULL_FACE);

	glUseProgram(solid_shader.programID);

	glUniformMatrix4fv(solid_shader.mvMatrixID, 1, GL_FALSE, mat_mv);
	glUniformMatrix4fv(solid_shader.mvpMatrixID, 1, GL_FALSE, mat_mvp);
	glUniformMatrix3fv(solid_shader.normalMatrixID, 1, GL_FALSE, mat_normal);
	glUniform3f(solid_shader.colorID, r, g, b);
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

static void graph_dev_raster_trans_wireframe_mesh(const float *mat_mvp, const float *mat_mv, const float *mat_normal,
	struct mesh *m, float r, float g, float b)
{
	enable_3d_viewport();

	if (!m->graph_ptr)
		return;

	struct mesh_gl_info *ptr = m->graph_ptr;

	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS);

	glUseProgram(trans_wireframe_shader.programID);

	glUniformMatrix4fv(trans_wireframe_shader.mvMatrixID, 1, GL_FALSE, mat_mv);
	glUniformMatrix4fv(trans_wireframe_shader.mvpMatrixID, 1, GL_FALSE, mat_mvp);
	glUniformMatrix3fv(trans_wireframe_shader.normalMatrixID, 1, GL_FALSE, mat_normal);
	glUniform3f(trans_wireframe_shader.colorID, r, g, b);

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

static void graph_dev_raster_line_mesh(const float *mat_mvp, struct mesh *m, float r, float g, float b)
{
	enable_3d_viewport();

	if (!m->graph_ptr)
		return;

	struct mesh_gl_info *ptr = m->graph_ptr;

	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS);

	glUseProgram(single_color_shader.programID);

	glUniformMatrix4fv(single_color_shader.mvpMatrixID, 1, GL_FALSE, mat_mvp);
	glUniform3f(single_color_shader.colorID, r, g, b);

	glEnableVertexAttribArray(single_color_shader.vertexPositionID);
	glBindBuffer(GL_ARRAY_BUFFER, ptr->vertex_buffer);
	glVertexAttribPointer(
		single_color_shader.vertexPositionID, /* The attribute we want to configure */
		3,                           /* size */
		GL_FLOAT,                    /* type */
		GL_FALSE,                    /* normalized? */
		0,                           /* stride */
		(void *)0                    /* array buffer offset */
	);

	glDrawArrays(GL_LINES, 0, ptr->nlines*2);

	glDisableVertexAttribArray(single_color_shader.vertexPositionID);
	glUseProgram(0);

	glDisable(GL_DEPTH_TEST);
}

void graph_dev_raster_point_cloud_mesh(const float *mat_mvp,
	struct mesh *m, float r, float g, float b, float pointSize)
{
	enable_3d_viewport();

	if (!m->graph_ptr)
		return;

	struct mesh_gl_info *ptr = m->graph_ptr;

	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS);
	glEnable(GL_VERTEX_PROGRAM_POINT_SIZE);

	glUseProgram(point_cloud_shader.programID);

	glUniformMatrix4fv(point_cloud_shader.mvpMatrixID, 1, GL_FALSE, mat_mvp);
	glUniform1f(point_cloud_shader.pointSizeID, pointSize);
	glUniform3f(point_cloud_shader.colorID, r, g, b);

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
	sng_set_foreground(e->color);

	switch (e->m->geometry_mode) {
	case MESH_GEOMETRY_TRIANGLES:
	{
		struct camera_info *c = &cx->camera;

		int filled_triangle = ((c->renderer & FLATSHADING_RENDERER) || (c->renderer & BLACK_TRIS))
					&& !(e->render_style & RENDER_NO_FILL);
		int outline_triangle = (c->renderer & WIREFRAME_RENDERER)
					|| (e->render_style & RENDER_WIREFRAME);

		if (filled_triangle) {
			sng_set_foreground(240 + GRAY + (NSHADESOFGRAY * e->shadecolor) + 10);

			graph_dev_raster_solid_mesh(&mat_mvp->m[0][0], &mat_mv->m[0][0], &mat_normal->m[0][0],
				e->m, sgc.hue->red/65536.0, sgc.hue->green/65536.0, sgc.hue->blue/65536.0,
				eye_light_pos);
		} else if (outline_triangle) {
			graph_dev_raster_trans_wireframe_mesh(&mat_mvp->m[0][0], &mat_mv->m[0][0],
				&mat_normal->m[0][0], e->m, sgc.hue->red/65536.0, sgc.hue->green/65536.0,
				sgc.hue->blue/65536.0);
		}
		break;
	}
	case MESH_GEOMETRY_LINES:
		graph_dev_raster_line_mesh(&mat_mvp->m[0][0], e->m, sgc.hue->red/65536.0,
			sgc.hue->green/65536.0, sgc.hue->blue/65536.0);
		break;

	case MESH_GEOMETRY_POINTS:
		graph_dev_raster_point_cloud_mesh(&mat_mvp->m[0][0], e->m, sgc.hue->red/65536.0,
			sgc.hue->green/65536.0, sgc.hue->blue/65536.0, 1.0);
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

	float x1, y1;
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
	setup_single_color_shader(&single_color_shader);
	setup_point_cloud_shader(&point_cloud_shader);

	return 0;
}

