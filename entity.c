/*
	Copyright (C) 2010 Stephen M. Cameron
	Author: Stephen M. Cameron

	This file is part of Spacenerds In Space.

	Spacenerds in Space is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.

	Spacenerds in Space is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with Spacenerds in Space; if not, write to the Free Software
	Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

/* Need _GNU_SOURCE for qsort_r, must be defined before any include directives */
#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <gtk/gtk.h>
#include <stdlib.h>

#include "mathutils.h"
#include "matrix.h"
#include "vertex.h"
#include "triangle.h"
#include "mesh.h"
#include "stl_parser.h"
#include "quat.h"
#include "entity.h"
#include "mathutils.h"

#include "my_point.h"
#include "snis_font.h"
#include "snis_typeface.h"
#include "snis_alloc.h"

#include "snis_graph.h"
#include "entity_private.h"
#include "graph_dev.h"

static int clip_line(struct mat41* vtx0, struct mat41* vtx1);

struct entity *add_entity(struct entity_context *cx,
	struct mesh *m, float x, float y, float z, int color)
{
	int n;

	n = snis_object_pool_alloc_obj(cx->entity_pool);
	if (n < 0)
		return NULL;

	cx->entity_list[n].m = m;
	cx->entity_list[n].x = x;
	cx->entity_list[n].y = y;
	cx->entity_list[n].z = z;
	cx->entity_list[n].scale = 1.0;
	cx->entity_list[n].color = color;
	cx->entity_list[n].render_style = RENDER_NORMAL;
	cx->entity_list[n].user_data = NULL;
	cx->entity_list[n].shadecolor = 0;
	quat_init_axis(&cx->entity_list[n].orientation, 0, 1, 0, 0);
	cx->entity_list[n].fragment_shader = 0;
	return &cx->entity_list[n];
}

void remove_entity(struct entity_context *cx, struct entity *e)
{
	int index;

	if (!e)
		return;
	index = e - &cx->entity_list[0];
	snis_object_pool_free_object(cx->entity_pool, index);
}

void remove_all_entity(struct entity_context *cx)
{
	snis_object_pool_free_all_objects(cx->entity_pool);
}

void update_entity_fragment_shader(struct entity *e, entity_fragment_shader_fn shader)
{
	e->fragment_shader = shader;
}

void update_entity_pos(struct entity *e, float x, float y, float z)
{
	e->x = x;
	e->y = y;
	e->z = z;
}

void update_entity_orientation(struct entity *e, const union quat *orientation)
{
	e->orientation = *orientation;
}

float entity_get_scale(struct entity *e)
{
	return e->scale;
}

void update_entity_scale(struct entity *e, float scale)
{
	e->scale = scale;
}

void update_entity_color(struct entity *e, int color)
{
	e->color = color;
}

void update_entity_shadecolor(struct entity *e, int color)
{
	e->shadecolor = color;
}

static inline float wx_screen(struct entity_context *cx, float wx)
{
	struct camera_info *c = &cx->camera;
	return (wx * c->xvpixels / 2) + c->xvpixels / 2 + cx->window_offset_x;
}

static inline float wy_screen(struct entity_context *cx, float wy)
{
	struct camera_info *c = &cx->camera;
	return (-wy * c->yvpixels / 2) + c->yvpixels / 2 + cx->window_offset_y;
}


static void wireframe_render_fake_star(GtkWidget *w, GdkGC *gc,
	struct entity_context *cx, struct vertex *fs)
{
	float wx = fs->wx / fs->ww;
	float wy = fs->wy / fs->ww;

	float x1, y1;

	x1 = wx_screen(cx, wx);
	y1 = wy_screen(cx, wy);
	sng_set_foreground(GRAY + snis_randn(NSHADESOFGRAY));
	sng_draw_point(w->window, gc, x1, y1);
}

#ifdef WITH_ILDA_SUPPORT

static unsigned short to_ildax(struct entity_context *cx, int x)
{
	double tx;
	struct camera_info *c = &cx->camera;

	tx = ((double) x - (double) (c->xvpixels / 2.0)) / (double) c->xvpixels + cx->window_offset_x;
	tx = tx * (32767 * 2);
	return (unsigned short) tx;
}

static unsigned short to_ilday(struct entity_context *cx, int y)
{
	double ty;
	struct camera_info *c = &cx->camera;

	/* note, use of xvpixels rather than yvpixels here is correct. */
	ty = ((double) y - (double) (c->xvpixels / 2.0)) / (double) c->xvpixels + cx->window_offset_y;
	ty = ty * (32767 * 2);
	return (unsigned short) ty;
}

static void ilda_render_triangle(struct entity_context *cx,
			int x1, int y1, int x2, int y2, int x3, int y3)
{
	unsigned short sx1, sy1, sx2, sy2, sx3, sy3;

	sx1 = to_ildax(cx, x1);
	sy1 = to_ilday(cx, y1);
	sx2 = to_ildax(cx, x2);
	sy2 = to_ilday(cx, y2);
	sx3 = to_ildax(cx, x3);
	sy3 = to_ilday(cx, y3);

	if (!cx->f)
		return;
	fprintf(cx->f, "  %hd %hd %d\n", sx1, sy1, -1);
	fprintf(cx->f, "  %hd %hd %d\n", sx2, sy2, 56);
	fprintf(cx->f, "  %hd %hd %d\n", sx3, sy3, 56);
	fprintf(cx->f, "  %hd %hd %d\n", sx1, sy1, 56);
}
#endif

static void calculate_model_matrices(struct entity_context *cx, float x, float y, float z, union quat *orientation,
	float scale, struct mat44 *mat_mvp, struct mat44 *mat_mv, struct mat33 *mat_normal)
{
	struct mat44d *mat_v = &cx->camera.camera_v_matrix;
	struct mat44d *mat_vp = &cx->camera.camera_vp_matrix;

	/* Model = (T*R)*S
	   T - translation matrix
	   R - rotation matrix
	   S - scale matrix
	   Combine for post multiply of vector */

	struct mat44d mat_t = {{
		{ 1, 0, 0, 0 },
		{ 0, 1, 0, 0 },
		{ 0, 0, 1, 0 },
		{ x, y, z, 1 }}};

	struct mat44d mat_r;
	quat_to_rh_rot_matrix_fd(orientation, &mat_r.m[0][0]);

	struct mat44d mat_s = {{
		{ scale, 0, 0, 0 },
		{ 0, scale, 0, 0 },
		{ 0, 0, scale, 0 },
		{ 0, 0, 0, 1 }}};

	struct mat44d mat_t_r;
	mat44_product_ddd(&mat_t, &mat_r, &mat_t_r);

	struct mat44d mat_m;
	mat44_product_ddd(&mat_t_r, &mat_s, &mat_m);

	/* calculate the final model-view-proj and model-view matrices */
	mat44_product_ddf(mat_vp, &mat_m, mat_mvp);
	mat44_product_ddf(mat_v, &mat_m, mat_mv);

	/* normal transform is the inverse transpose of the upper left 3x3 of the model-view matrix */
	struct mat33 mat_tmp33;
	mat33_inverse_transpose_ff(mat44_to_mat33_ff(mat_mv, &mat_tmp33), mat_normal);
}

int transform_vertices(const struct mat44 *matrix, struct vertex *v, int len)
{
	int total_clip_flag = 0x3f;

	int i;
	for (i = 0; i < len; i++) {
		/* Set homogeneous coord to 1 */
		v[i].w = 1.0;

		/* Do the transformation... */
		struct mat41 *m1 = (struct mat41 *) &v[i].x;
		struct mat41 *m2 = (struct mat41 *) &v[i].wx;
		mat44_x_mat41(matrix, m1, m2);

		/* check all 6 clip planes for this vertex */
		v[i].clip = 0;
		v[i].clip |= (v[i].wx < -v[i].ww) ? (1<<0) : 0;
		v[i].clip |= (v[i].wx > v[i].ww) ? (1<<1) : 0;
		v[i].clip |= (v[i].wy < -v[i].ww) ? (1<<2) : 0;
		v[i].clip |= (v[i].wy > v[i].ww) ? (1<<3) : 0;
		v[i].clip |= (v[i].wz < -v[i].ww) ? (1<<4) : 0;
		v[i].clip |= (v[i].wz > v[i].ww) ? (1<<5) : 0;

		/* total is the intersection of all the vertices */
		total_clip_flag &= v[i].clip;
	}

	/* if all vertices were clipped by the same plane then this set is all out */
	return total_clip_flag > 0;
}

int transform_line(struct entity_context *cx, float x1, float y1, float z1, float x2, float y2, float z2,
			float *sx1, float *sy1, float *sx2, float *sy2)
{
	struct vertex v[2] = {
		{x1, y1, z1},
		{x2, y2, z2}};

	/* there is no model transform on a point */
	struct mat44 mat_vp;
	mat44_convert_df(&cx->camera.camera_vp_matrix, &mat_vp);

	if (transform_vertices(&mat_vp, &v[0], 2)) {
		/* both ends outside frustum */
		*sx1 = -1;
		*sy1 = -1;
		*sx2 = -1;
		*sy2 = -1;
		return 1;
	}

	if (v[0].clip || v[1].clip) {
		if (clip_line((struct mat41 *) &v[0].wx, (struct mat41 *) &v[1].wx)) {
			/* both ends clipped */
			return 1;
		}
	}

	/* perspective divide */
	v[0].wx /= v[0].ww;
	v[0].wy /= v[0].ww;
	v[1].wx /= v[1].ww;
	v[1].wy /= v[1].ww;

	/* convert to screen coordinates */
	*sx1 = wx_screen(cx, v[0].wx);
	*sy1 = wy_screen(cx, v[0].wy);
	*sx2 = wx_screen(cx, v[1].wx);
	*sy2 = wy_screen(cx, v[1].wy);
	return 0;

}

int transform_point(struct entity_context *cx, float x, float y, float z, float *sx, float *sy)
{
	struct vertex v = {x, y, z};

	/* there is no model transform on a point */
	struct mat44 mat_vp;
	mat44_convert_df(&cx->camera.camera_vp_matrix, &mat_vp);

	if (transform_vertices(&mat_vp, &v, 1)) {
		*sx = -1;
		*sy = -1;
		return 1;
	}

	v.wx /= v.ww;
	v.wy /= v.ww;

	*sx = wx_screen(cx, v.wx);
	*sy = wy_screen(cx, v.wy);
	return 0;
}

void render_entity(GtkWidget *w, GdkGC *gc, struct entity_context *cx, struct entity *e, union vec3 *camera_light_pos)
{
	/* calculate screen coords of entity as a whole */
	transform_point(cx, e->x, e->y, e->z, &e->sx, &e->sy);

	if (e->sx >=0 && e->sy >= 0)
		e->onscreen = 1;

	struct mat44 mat_mvp, mat_mv;
	struct mat33 mat_normal;
	calculate_model_matrices(cx, e->x, e->y, e->z, &e->orientation, e->scale,
					&mat_mvp, &mat_mv, &mat_normal);

	graph_dev_draw_entity(cx, e, camera_light_pos, &mat_mvp, &mat_mv, &mat_normal);
}

#if defined(__APPLE__)  || defined(__FreeBSD__)
static int object_depth_compare(void *vcx, const void *a, const void *b)
#else
static int object_depth_compare(const void *a, const void *b, void *vcx)
#endif
{
	struct entity_context *cx = vcx;
	struct entity *A = &cx->entity_list[*(const int *) a];
	struct entity *B = &cx->entity_list[*(const int *) b];

	if (A->dist3dsqrd < B->dist3dsqrd)
		return 1;
	if (A->dist3dsqrd < B->dist3dsqrd)
		return -1;
	return 0;
}

static void sort_entity_distances(struct entity_context *cx)
{
#if defined(__APPLE__)  || defined(__FreeBSD__)
	qsort_r(cx->entity_depth, cx->nentity_depth, sizeof(cx->entity_depth[0]), cx, object_depth_compare);
#else
	qsort_r(cx->entity_depth, cx->nentity_depth, sizeof(cx->entity_depth[0]), object_depth_compare, cx);
#endif
}

static inline float sqr(float a)
{
	return a * a;
}

#ifdef WITH_ILDA_SUPPORT
static void ilda_file_open(struct entity_context *cx)
{
	if (cx->f)
		return;

	cx->f = fopen("/tmp/ilda.txt", "w+");
	cx->framenumber = 0;
	return;
}

static void ilda_file_newframe(struct entity_context *cx)
{
	if (!cx->f)
		return;
	cx->framenumber++;
	fprintf(cx->f, "\n#      %d ------------------------------------------\n",
			cx->framenumber);
	fprintf(cx->f, "frame xy palette short\n");
	if ((cx->framenumber % 60) == 0)
		fflush(cx->f);
}
#endif

/* from http://www.crownandcutlass.com/features/technicaldetails/frustum.html
This page and its contents are Copyright 2000 by Mark Morley
Unless otherwise noted, you may use any and all code examples provided herein in any way you want. */

int sphere_in_frustum(struct camera_info *c, float x, float y, float z, float radius )
{
	int p;

	for (p = 0; p < 6; p++)
		if (c->frustum[p][0] * x + c->frustum[p][1] * y + c->frustum[p][2] * z + c->frustum[p][3] <= -radius)
			return 0;
	return 1;
}

/* from http://www.crownandcutlass.com/features/technicaldetails/frustum.html
This page and its contents are Copyright 2000 by Mark Morley
Unless otherwise noted, you may use any and all code examples provided herein in any way you want. */

void extract_frustum_planes(struct camera_info *c)
{
	const double *clip = &c->camera_vp_matrix.m[0][0];
	float t;

	/* Extract the numbers for the RIGHT plane */
	c->frustum[0][0] = clip[ 3] - clip[ 0];
	c->frustum[0][1] = clip[ 7] - clip[ 4];
	c->frustum[0][2] = clip[11] - clip[ 8];
	c->frustum[0][3] = clip[15] - clip[12];

	/* Normalize the result */
	t = dist3d( c->frustum[0][0], c->frustum[0][1], c->frustum[0][2] );
	c->frustum[0][0] /= t;
	c->frustum[0][1] /= t;
	c->frustum[0][2] /= t;
	c->frustum[0][3] /= t;

	/* Extract the numbers for the LEFT plane */
	c->frustum[1][0] = clip[ 3] + clip[ 0];
	c->frustum[1][1] = clip[ 7] + clip[ 4];
	c->frustum[1][2] = clip[11] + clip[ 8];
	c->frustum[1][3] = clip[15] + clip[12];

	/* Normalize the result */
	t = dist3d( c->frustum[1][0], c->frustum[1][1], c->frustum[1][2] );
	c->frustum[1][0] /= t;
	c->frustum[1][1] /= t;
	c->frustum[1][2] /= t;
	c->frustum[1][3] /= t;

	/* Extract the BOTTOM plane */
	c->frustum[2][0] = clip[ 3] + clip[ 1];
	c->frustum[2][1] = clip[ 7] + clip[ 5];
	c->frustum[2][2] = clip[11] + clip[ 9];
	c->frustum[2][3] = clip[15] + clip[13];

	/* Normalize the result */
	t = dist3d( c->frustum[2][0], c->frustum[2][1], c->frustum[2][2] );
	c->frustum[2][0] /= t;
	c->frustum[2][1] /= t;
	c->frustum[2][2] /= t;
	c->frustum[2][3] /= t;

	/* Extract the TOP plane */
	c->frustum[3][0] = clip[ 3] - clip[ 1];
	c->frustum[3][1] = clip[ 7] - clip[ 5];
	c->frustum[3][2] = clip[11] - clip[ 9];
	c->frustum[3][3] = clip[15] - clip[13];

	/* Normalize the result */
	t = dist3d( c->frustum[3][0], c->frustum[3][1], c->frustum[3][2] );
	c->frustum[3][0] /= t;
	c->frustum[3][1] /= t;
	c->frustum[3][2] /= t;
	c->frustum[3][3] /= t;

	/* Extract the FAR plane */
	c->frustum[4][0] = clip[ 3] - clip[ 2];
	c->frustum[4][1] = clip[ 7] - clip[ 6];
	c->frustum[4][2] = clip[11] - clip[10];
	c->frustum[4][3] = clip[15] - clip[14];

	/* Normalize the result */
	t = dist3d( c->frustum[4][0], c->frustum[4][1], c->frustum[4][2] );
	c->frustum[4][0] /= t;
	c->frustum[4][1] /= t;
	c->frustum[4][2] /= t;
	c->frustum[4][3] /= t;

	/* Extract the NEAR plane */
	c->frustum[5][0] = clip[ 3] + clip[ 2];
	c->frustum[5][1] = clip[ 7] + clip[ 6];
	c->frustum[5][2] = clip[11] + clip[10];
	c->frustum[5][3] = clip[15] + clip[14];

	/* Normalize the result */
	t = dist3d( c->frustum[5][0], c->frustum[5][1], c->frustum[5][2] );
	c->frustum[5][0] /= t;
	c->frustum[5][1] /= t;
	c->frustum[5][2] /= t;
	c->frustum[5][3] /= t;
}

void calculate_camera_transform(struct entity_context *cx)
{
	struct mat41 *v; /* camera relative y axis (up/down) */
	struct mat41 *n; /* camera relative z axis (into view plane) */
	struct mat41 *u; /* camera relative x axis (left/right) */

	struct mat41 up;
	up.m[0] = cx->camera.ux;
	up.m[1] = cx->camera.uy;
	up.m[2] = cx->camera.uz;
	up.m[3] = 1;

	/* Translate to camera position... */
	struct mat44d cameratrans_transform = {{{1, 0, 0, 0},
						{0, 1, 0, 0},
						{0, 0, 1, 0},
						{-cx->camera.x, -cx->camera.y, -cx->camera.z, 1}}};

	/* Calculate camera rotation based on look direction ...
	   http://ksimek.github.io/2012/08/22/extrinsic/ 'The "Look-At" Camera'
	   p = look at point, C = camera, u = up
	   L = p - C
	   s = L x u
	   u' = s x L
	   camera rotation = | s1  s2  s3  |
			     | u'1 u'2 u'3 |
			     | -L1 -L2 -L3 |
	  snis n = L
	  snis u = s
	  snis v = u' */
	struct mat41 look_direction;
	look_direction.m[0] = (cx->camera.lx - cx->camera.x);
	look_direction.m[1] = (cx->camera.ly - cx->camera.y);
	look_direction.m[2] = (cx->camera.lz - cx->camera.z);
	look_direction.m[3] = 1.0;
	normalize_vector(&look_direction, &look_direction);
	n = &look_direction;

	/* Calculate x direction relative to camera, "camera_x" */
	struct mat41 camera_x;
	mat41_cross_mat41(&look_direction, &up, &camera_x);
	normalize_vector(&camera_x, &camera_x);
	u = &camera_x;

	/* Calculate camera relative x axis */
	struct mat41 x_cross_look;
	mat41_cross_mat41(&camera_x, &look_direction, &x_cross_look);
	/* v should not need normalizing as n and u are already
	 * unit, and are perpendicular */
	normalize_vector(&x_cross_look, &x_cross_look);
	v = &x_cross_look;

	/* Make a rotation matrix...
	   | ux uy uz 0 |
	   | vx vy vz 0 |
	   | -nx -ny -nz 0 |
	   |  0  0  0 1 |
	 */
	struct mat44d cameralook_transform;
	cameralook_transform.m[0][0] = u->m[0];
	cameralook_transform.m[0][1] = v->m[0];
	cameralook_transform.m[0][2] = -n->m[0];
	cameralook_transform.m[0][3] = 0.0;
	cameralook_transform.m[1][0] = u->m[1];
	cameralook_transform.m[1][1] = v->m[1];
	cameralook_transform.m[1][2] = -n->m[1];
	cameralook_transform.m[1][3] = 0.0;
	cameralook_transform.m[2][0] = u->m[2];
	cameralook_transform.m[2][1] = v->m[2];
	cameralook_transform.m[2][2] = -n->m[2];
	cameralook_transform.m[2][3] = 0.0;
	cameralook_transform.m[3][0] = 0.0;
	cameralook_transform.m[3][1] = 0.0;
	cameralook_transform.m[3][2] = 0.0;
	cameralook_transform.m[3][3] = 1.0;

	/* Make perspective transform based on OpenGL frustum
	   http://www.scratchapixel.com/lessons/3d-advanced-lessons/perspective-and-orthographic-projection-matrix/opengl-perspective-projection-matrix/
	*/
	struct mat44d perspective_transform;
	perspective_transform.m[0][0] = (2 * cx->camera.near) / (cx->camera.right - cx->camera.left);
	perspective_transform.m[0][1] = 0.0;
	perspective_transform.m[0][2] = 0.0;
	perspective_transform.m[0][3] = 0.0;
	perspective_transform.m[1][0] = 0.0;
	perspective_transform.m[1][1] = (2.0 * cx->camera.near) / (cx->camera.top - cx->camera.bottom);
	perspective_transform.m[1][2] = 0.0;
	perspective_transform.m[1][3] = 0.0;
	perspective_transform.m[2][0] = (cx->camera.right + cx->camera.left) / (cx->camera.right - cx->camera.left);
	perspective_transform.m[2][1] = (cx->camera.top + cx->camera.bottom) / (cx->camera.top - cx->camera.bottom);
	perspective_transform.m[2][2] = -(cx->camera.far + cx->camera.near) / (cx->camera.far - cx->camera.near);
	perspective_transform.m[2][3] = -1.0;
	perspective_transform.m[3][0] = 0.0;
	perspective_transform.m[3][1] = 0.0;
	perspective_transform.m[3][2] = (-2 * cx->camera.far * cx->camera.near) /
						(cx->camera.far - cx->camera.near);
	perspective_transform.m[3][3] = 0.0;

	/* make the view matrix, V = L * T */
	mat44_product_ddd(&cameralook_transform, &cameratrans_transform, &cx->camera.camera_v_matrix);

	/* make the view-perspective matrix, VP = P * V */
	mat44_product_ddd(&perspective_transform, &cx->camera.camera_v_matrix, &cx->camera.camera_vp_matrix);

	/* pull out the frustum planes for entity culling */
	extract_frustum_planes(&cx->camera);
}

static void reposition_fake_star(struct entity_context *cx, struct vertex *fs, float radius);

void render_entities(GtkWidget *w, GdkGC *gc, struct entity_context *cx)
{
	int i, j, n;
	struct camera_info *c = &cx->camera;

	sng_set_3d_viewport(cx->window_offset_x, cx->window_offset_y, c->xvpixels, c->yvpixels);

	calculate_camera_transform(cx);

#ifdef WITH_ILDA_SUPPORT
	ilda_file_open(cx);
	ilda_file_newframe(cx);
#endif

	/* draw fake stars */
	sng_set_foreground(WHITE);
	struct mat44 mat_vp;
	mat44_convert_df(&cx->camera.camera_vp_matrix, &mat_vp);
	transform_vertices(&mat_vp, &cx->fake_star[0], cx->nfakestars);

	for (i = 0; i < cx->nfakestars; i++) {
		struct vertex *fs = &cx->fake_star[i];

		float dist2 = dist3dsqrd(c->x - fs->x, c->y - fs->y, c->z - fs->z);

		if (!fs->clip)
			wireframe_render_fake_star(w, gc, cx, fs);

		if (dist2 > cx->fakestars_radius * cx->fakestars_radius)
			reposition_fake_star(cx, fs, cx->fakestars_radius);
	}

	/* find all entities in view frustum and sort them by distance */
	n = snis_object_pool_highest_object(cx->entity_pool);

	cx->nentity_depth = 0;

	for (j = 0; j <= n; j++) {
		if (!snis_object_pool_is_allocated(cx->entity_pool, j))
			continue;

		struct entity *e = &cx->entity_list[j];

		if (e->m == NULL)
			continue;

		e->onscreen = 0;

		if (!sphere_in_frustum(c, e->x, e->y, e->z, e->m->radius * fabs(e->scale)))
			continue;

		e->dist3dsqrd = dist3dsqrd(c->x - e->x, c->y - e->y, c->z - e->z);
		cx->entity_depth[cx->nentity_depth] = j;
		cx->nentity_depth++;
	}

	sort_entity_distances(cx);

	/* find the position of our light in camera space */
	struct mat41 camera_light_pos;
	mat44_x_mat41_dff(&cx->camera.camera_v_matrix, &cx->light, &camera_light_pos);

	/* render the sorted entities */
	for (j = 0; j < cx->nentity_depth; j++) {
		struct entity *e = &cx->entity_list[cx->entity_depth[j]];

		render_entity(w, gc, cx, e, (union vec3 *)&camera_light_pos.m[0]);
	}
}

void camera_set_pos(struct entity_context *cx, float x, float y, float z)
{
	cx->camera.x = x;
	cx->camera.y = y;
	cx->camera.z = z;
}

void camera_get_pos(struct entity_context *cx, float *x, float *y, float *z)
{
	*x = cx->camera.x;
	*y = cx->camera.y;
	*z = cx->camera.z;
}

void camera_look_at(struct entity_context *cx, float x, float y, float z)
{
	cx->camera.lx = x;
	cx->camera.ly = y;
	cx->camera.lz = z;
}

void camera_get_look_at(struct entity_context *cx, float *x, float *y, float *z)
{
	*x = cx->camera.lx;
	*y = cx->camera.ly;
	*z = cx->camera.lz;
}

void camera_assign_up_direction(struct entity_context *cx, float x, float y, float z)
{
	cx->camera.ux = x;
	cx->camera.uy = y;
	cx->camera.uz = z;
}

void camera_get_up_direction(struct entity_context *cx, float *x, float *y, float *z)
{
	*x = cx->camera.ux;
	*y = cx->camera.uy;
	*z = cx->camera.uz;
}

void camera_set_parameters(struct entity_context *cx, float near, float far,
				int xvpixels, int yvpixels, float angle_of_view)
{
	cx->camera.near = near;
	cx->camera.far = far;
	cx->camera.xvpixels = xvpixels;
	cx->camera.yvpixels = yvpixels;
	cx->camera.angle_of_view = angle_of_view;

	/* based on gluPerspective to find the right, left, top, and bottom */
	float scale = tan(cx->camera.angle_of_view * 0.5) * cx->camera.near;
	cx->camera.right = ((float) xvpixels / (float) yvpixels * scale);
	cx->camera.left = -cx->camera.right;
	cx->camera.top = scale;
	cx->camera.bottom = -cx->camera.top;
}

void camera_get_parameters(struct entity_context *cx, float *near, float *far,
				int *xvpixels, int *yvpixels, float *angle_of_view)
{
	*near = cx->camera.near;
	*far = cx->camera.far;
	*xvpixels = cx->camera.xvpixels;
	*yvpixels = cx->camera.yvpixels;
	*angle_of_view = cx->camera.angle_of_view;
}

struct entity_context *entity_context_new(int maxobjs)
{
	struct entity_context *cx;

	cx = malloc(sizeof(*cx));

	memset(cx, 0, sizeof(*cx));
	cx->entity_list = malloc(sizeof(cx->entity_list[0]) * maxobjs);
	memset(cx->entity_list, 0, sizeof(cx->entity_list[0]) * maxobjs);
	cx->entity_depth = malloc(sizeof(cx->entity_depth[0]) * maxobjs);
	memset(cx->entity_depth, 0, sizeof(cx->entity_depth[0]) * maxobjs);
	snis_object_pool_setup(&cx->entity_pool, maxobjs);
	cx->maxobjs = maxobjs;
	set_renderer(cx, FLATSHADING_RENDERER);
	set_lighting(cx, 0, 0, 0);
	camera_assign_up_direction(cx, 0.0, 1.0, 0.0);
	set_window_offset(cx, 0.0, 0.0);
#ifdef WITH_ILDA_SUPPORT
	cx->f = NULL;
#endif
	return cx;
}

void entity_context_free(struct entity_context *cx)
{
	free(cx->entity_list);
	free(cx->entity_depth);
	free(cx);
}

/* Re-position a star randomly on the surface of sphere of given radius */
static void reposition_fake_star(struct entity_context *cx, struct vertex *fs, float radius)
{
	/* I tried "on" sphere here, but I like the look of "in" better. */
	float dist3dsqrd;
	random_point_in_sphere(radius, &fs->x, &fs->y, &fs->z, &dist3dsqrd);
	fs->x += cx->camera.x;
	fs->y += cx->camera.y;
	fs->z += cx->camera.z;
}

/* fill a sphere of specified radius with randomly placed stars */
void entity_init_fake_stars(struct entity_context *cx, int nstars, float radius)
{
	int i;

	cx->fake_star = malloc(sizeof(*cx->fake_star) * nstars);
	memset(cx->fake_star, 0, sizeof(*cx->fake_star) * nstars);

	for (i = 0; i < nstars; i++) {
		reposition_fake_star(cx, &cx->fake_star[i], radius);
	}
	cx->nfakestars = nstars;
	cx->fakestars_radius = radius;
}

void entity_free_fake_stars(struct entity_context *cx)
{
	cx->nfakestars = 0;
	free(cx->fake_star);
}

void set_renderer(struct entity_context *cx, int renderer)
{
	cx->camera.renderer = renderer;
}

int get_renderer(struct entity_context *cx)
{
	return cx->camera.renderer;
}

void set_render_style(struct entity *e, int render_style)
{
	if (e)
		e->render_style = render_style;
}

void set_lighting(struct entity_context *cx, double x, double y, double z)
{
	cx->light.m[0] = x;
	cx->light.m[1] = y;
	cx->light.m[2] = z;
	cx->light.m[3] = 1;
}

struct mesh *entity_get_mesh(struct entity *e)
{
	return e->m;
}

void entity_set_mesh(struct entity *e, struct mesh *m)
{
	e->m = m;
}

int get_entity_count(struct entity_context *cx)
{
	return snis_object_pool_highest_object(cx->entity_pool);
}

struct entity *get_entity(struct entity_context *cx, int n)
{
	return &cx->entity_list[n];
}

void entity_get_screen_coords(struct entity *e, float *x, float *y)
{
	*x = e->sx;
	*y = e->sy;
}

void entity_set_user_data(struct entity *e, void *user_data)
{
	e->user_data = user_data;
}

void *entity_get_user_data(struct entity *e)
{
	return e->user_data;
}

void camera_set_orientation(struct entity_context *cx, union quat *q)
{
	union vec3 look, up;

	look.v.x = 1;
	look.v.y = 0;
	look.v.z = 0;
	up.v.x = 0;
	up.v.y = 1;
	up.v.z = 0;

	quat_rot_vec_self(&look, q);
	quat_rot_vec_self(&up, q);

	camera_look_at(cx, cx->camera.x + look.v.x * 256,
			cx->camera.y + look.v.y * 256,
			cx->camera.z + look.v.z * 256);
	camera_assign_up_direction(cx, up.v.x, up.v.y, up.v.z);
}

union quat *entity_get_orientation(struct entity *e)
{
	return &e->orientation;
}

struct mat44d get_camera_v_transform(struct entity_context *cx)
{
	return cx->camera.camera_v_matrix;
}

struct mat44d get_camera_vp_transform(struct entity_context *cx)
{
	return cx->camera.camera_vp_matrix;
}

void set_window_offset(struct entity_context *cx, float x, float y)
{
	cx->window_offset_x = x;
	cx->window_offset_y = y;
}

int entity_onscreen(struct entity *e)
{
	return (int) e->onscreen;
}

/* clipping triangles to frustum taken from
   http://simonstechblog.blogspot.com/2012/04/software-rasterizer-part-2.html
   converted from javascript to c */
static const float ZERO_TOLERANCE = 0.000001f;

typedef float (*clip_line_fn)(float vtx0x, float vtx0w, float vtx1x, float vtx1w );
typedef float (*get_comp_fn)(const struct mat41 *m);
typedef int (*is_outside_fn)(float x, float w);

/* return a pareameter t for line segment within range [0, 1] is intersect */
static float clip_line_pos(float vtx0x, float vtx0w, float vtx1x, float vtx1w)
{
	float denominator = vtx0w - vtx1w - vtx0x + vtx1x;
	if (fabs(denominator) < ZERO_TOLERANCE)
		return -1;	/* line parallel to the plane... */

	float t = (vtx0w - vtx0x) / denominator;
	if (t >= 0.0 && t <= 1.0)
		return t;	/* intersect */
	else
		return -1;	/* line segment not intersect */
}

static float clip_line_neg(float vtx0x, float vtx0w, float vtx1x, float vtx1w)
{
	float denominator = vtx0w - vtx1w + vtx0x - vtx1x;
	if (fabs(denominator) < ZERO_TOLERANCE)
		return -1;	/* line parallel to the plane... */

	float t = (vtx0w + vtx0x) / denominator;
	if (t >= 0.0 && t <= 1.0)
		return t;	/* intersect */
	else
		return -1;	/* line segment not intersect */
}

static int is_outside_pos(float x, float w)
{
	return x > w;
}

static int is_outside_neg(float x, float w)
{
	return x < -w;
}

static float get_x(const struct mat41 *m) {
	return m->m[0];
}

static float get_y(const struct mat41 *m)
{
	return m->m[1];
}

static float get_z(const struct mat41* m) {
	return m->m[2];
}

static void mat41_lerp(struct mat41 *out, struct mat41 *v1, struct mat41 *v2, float t)
{
	float invT = 1.0-t;
	out->m[0] = t * v2->m[0] + invT * v1->m[0];
	out->m[1] = t * v2->m[1] + invT * v1->m[1];
	out->m[2] = t * v2->m[2] + invT * v1->m[2];
	out->m[3] = t * v2->m[3] + invT * v1->m[3];
}

/* the clip line to plane with interpolated varying */
static void clipped_line_from_plane(struct mat41 *out, float t, struct mat41 *vs_out0,
					struct mat41 *vs_out1)
{
	if (t<0)
		printf("Assert: two vertex does not intersect plane\n");

	mat41_lerp(out, vs_out0, vs_out1, t);
}

static int clip_line(struct mat41* vtx0, struct mat41* vtx1)
{
	clip_line_fn clipping_func[] = {clip_line_pos, clip_line_neg, clip_line_pos,
					clip_line_neg, clip_line_pos, clip_line_neg};
	get_comp_fn get_component_func[] = {get_x, get_x, get_y, get_y, get_z, get_z};
	is_outside_fn is_outside_func[] = {is_outside_pos, is_outside_neg, is_outside_pos,
						is_outside_neg, is_outside_pos, is_outside_neg};

	int plane;
	for (plane = 0; plane < 6; plane++) {
		float component0 = get_component_func[plane](vtx0);
		float component1 = get_component_func[plane](vtx1);

		float pos0w = vtx0->m[3];
		float pos1w = vtx1->m[3];

		int is_outside0 = is_outside_func[plane](component0, pos0w);
		int is_outside1 = is_outside_func[plane](component1, pos1w);

		if (is_outside0 && is_outside1)
			return 1;

		if (is_outside0 && !is_outside1) {
			float line0_t = clipping_func[plane](component0, pos0w, component1, pos1w);
			clipped_line_from_plane(vtx0, line0_t, vtx0, vtx1);
		}
		else if (!is_outside0 && is_outside1) {
			float line1_t = clipping_func[plane](component1, pos1w, component0, pos0w);
			clipped_line_from_plane(vtx1, line1_t, vtx1, vtx0);
		}
	}
	return 0;
}

