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
#include "entity.h"
#include "mathutils.h"

#include "my_point.h"
#include "snis_font.h"
#include "snis_typeface.h"
#include "snis_alloc.h"

#include "snis_graph.h"

#define MAX_ENTITIES 5000

static unsigned long ntris, nents, nlines;

struct fake_star {
	struct vertex v;
	float dist3dsqrd;
};

struct entity {
	struct mesh *m;
	float x, y, z; /* world coords */
	float rx, ry, rz;
	float dist3dsqrd;
	int color;
	int render_as_point_cloud;
};

struct camera_info {
	float x, y, z;		/* position of camera */
	float lx, ly, lz;	/* where camera is looking */
	float near, far, width, height;
	float angle_of_view;
	int xvpixels, yvpixels;
} camera;

static struct snis_object_pool *entity_pool;
static struct entity entity_list[MAX_ENTITIES];
static int entity_depth[MAX_ENTITIES];
static struct fake_star *fake_star;
static int nfakestars = 0;

static float rx, ry, rz;

struct entity *add_entity(struct mesh *m, float x, float y, float z, int color)
{
	int n;

	n = snis_object_pool_alloc_obj(entity_pool);
	if (n < 0)
		return NULL;

	entity_list[n].m = m;
	entity_list[n].x = x;
	entity_list[n].y = y;
	entity_list[n].z = z;
	entity_list[n].rx = 0;
	entity_list[n].ry = 0;
	entity_list[n].rz = 0;
	entity_list[n].color = color;
	entity_list[n].render_as_point_cloud = 0;
	return &entity_list[n];
}

struct entity *add_point_cloud(struct mesh *m, float x, float y, float z, int color)
{
	struct entity *e;

	e = add_entity(m, x, y, z, color);
	if (!e)
		return NULL;
	e->render_as_point_cloud = 1;
	return e;
}

void remove_entity(struct entity *e)
{
	int index;

	if (!e)
		return;
	index = e - &entity_list[0]; 
	snis_object_pool_free_object(entity_pool, index);
}

void update_entity_pos(struct entity *e, float x, float y, float z)
{
	e->x = x;
	e->y = y;
	e->z = z;
}

void update_entity_rotation(struct entity *e, float rx, float ry, float rz)
{
	e->rx = rx;
	e->ry = ry;
	e->rz = rz;
}

void update_entity_color(struct entity *e, int color)
{
	e->color = color;
}

static int is_backface(int x1, int y1, int x2, int y2, int x3, int y3)
{
	int twicearea;

	twicearea =	(x1 * y2 - x2 * y1) +
			(x2 * y3 - x3 * y2) +
			(x3 * y1 - x1 * y3);
	return twicearea >= 0;
}

static void wireframe_render_fake_star(GtkWidget *w, GdkGC *gc, struct fake_star *fs)
{
	int x1, y1;

	x1 = (int) (fs->v.wx * camera.xvpixels / 2) + camera.xvpixels / 2;
	y1 = (int) (fs->v.wy * camera.yvpixels / 2) + camera.yvpixels / 2;
	sng_current_draw_line(w->window, gc, x1, y1, x1 + (snis_randn(10) < 7), y1); 
}

void wireframe_render_triangle(GtkWidget *w, GdkGC *gc, struct triangle *t)
{
	struct vertex *v1, *v2, *v3;
	int x1, y1, x2, y2, x3, y3;

	ntris++;
	nlines += 3;
	v1 = t->v[0];
	v2 = t->v[1];
	v3 = t->v[2];

	x1 = (int) (v1->wx * camera.xvpixels / 2) + camera.xvpixels / 2;
	x2 = (int) (v2->wx * camera.xvpixels / 2) + camera.xvpixels / 2;
	x3 = (int) (v3->wx * camera.xvpixels / 2) + camera.xvpixels / 2;
	y1 = (int) (v1->wy * camera.yvpixels / 2) + camera.yvpixels / 2;
	y2 = (int) (v2->wy * camera.yvpixels / 2) + camera.yvpixels / 2;
	y3 = (int) (v3->wy * camera.yvpixels / 2) + camera.yvpixels / 2;

	if (is_backface(x1, y1, x2, y2, x3, y3))
		return;

	sng_current_draw_line(w->window, gc, x1, y1, x2, y2); 
	sng_current_draw_line(w->window, gc, x2, y2, x3, y3); 
	sng_current_draw_line(w->window, gc, x3, y3, x1, y1); 
}

void wireframe_render_point(GtkWidget *w, GdkGC *gc, struct vertex *v)
{
	int x1, y1;

	x1 = (int) (v->wx * camera.xvpixels / 2) + camera.xvpixels / 2;
	y1 = (int) (v->wy * camera.yvpixels / 2) + camera.yvpixels / 2;
	sng_current_draw_line(w->window, gc, x1, y1, x1 + 1, y1); 
}

static void scan_convert_sorted_triangle(GtkWidget *w, GdkGC *gc,
			int x1, int y1, int x2, int y2, int x3, int y3)
{
	float xa, xb, y;
	float dxdy1, dxdy2;
	int i;

	if (y1 > y2 || y2 > y3)
		printf("you dun fucked up\n");

	if (y2 == y1)
		dxdy1 = x2 - x1;
	else
		dxdy1 = (float) (x2 - x1) / (float) (y2 - y1);

	if (y3 == y1)
		dxdy2 = x3 - x1;
	else
		dxdy2 = (float) (x3 - x1) / (float) (y3 - y1);

	xa = x1;
	xb = x1;
	y = y1;

	for (i = y1; i < y2; i++) {
		sng_device_line(w->window, gc, (int) xa, (int) y, (int) xb, (int) y);
		xa += dxdy1;
		xb += dxdy2;
		y += 1;
	}

	if (y2 == y3)
		dxdy1 = x3 - x2;
	else
		dxdy1 = (float) (x3 - x2) / (float) (y3 - y2);

	xa = x2;
	y = y2;
	for (i = y2; i <= y3; i++) {
		sng_device_line(w->window, gc, (int) xa, (int) y, (int) xb, (int) y);
		xa += dxdy1;
		xb += dxdy2;
		y += 1;
	}
}

static void scan_convert_triangle(GtkWidget *w, GdkGC *gc, struct triangle *t)
{
	struct vertex *v1, *v2, *v3;
	int x1, y1, x2, y2, x3, y3;
	int xa, ya, xb, yb, xc, yc;

	ntris++;
	nlines += 3;
	v1 = t->v[0];
	v2 = t->v[1];
	v3 = t->v[2];

	x1 = (int) (v1->wx * camera.xvpixels / 2) + camera.xvpixels / 2;
	x2 = (int) (v2->wx * camera.xvpixels / 2) + camera.xvpixels / 2;
	x3 = (int) (v3->wx * camera.xvpixels / 2) + camera.xvpixels / 2;
	y1 = (int) (v1->wy * camera.yvpixels / 2) + camera.yvpixels / 2;
	y2 = (int) (v2->wy * camera.yvpixels / 2) + camera.yvpixels / 2;
	y3 = (int) (v3->wy * camera.yvpixels / 2) + camera.yvpixels / 2;

	if (is_backface(x1, y1, x2, y2, x3, y3))
		return;

	/* sort from lowest y to highest y */
	ya = sng_device_y(y1);
	yb = sng_device_y(y2);
	yc = sng_device_y(y3);
	if (ya <= yb) {
		if (ya <= yc) {
			/* order is 1, ... */
			xa = sng_device_x(x1);
			if (yb <= yc) {
				/* order is 1, 2, 3 */
				xb = sng_device_x(x2);
				xc = sng_device_x(x3);
			} else {
				/* order is 1, 3, 2 */
				xb = sng_device_x(x3);
				yb = sng_device_y(y3);
				xc = sng_device_x(x2);
				yc = sng_device_y(y2);
			}
		} else { /* we know c < a <= b, so order is 3, 1, 2... */
			xa = sng_device_x(x3);
			ya = sng_device_y(y3);
			xb = sng_device_x(x1);
			yb = sng_device_y(y1);
			xc = sng_device_x(x2);
			yc = sng_device_y(y2);
		}
	} else {
		/* a > b */
		if (yb > yc) { /* a > b >= c, so order is 3, 2, 1 */
			xa = sng_device_x(x3);
			ya = sng_device_y(y3);
			xb = sng_device_x(x2);
			yb = sng_device_y(y2);
			xc = sng_device_x(x1);
			yc = sng_device_y(y1);
		} else { /* c >= b && b < a */
			if (ya <= yc) {
				/* c >= b && b < a && a <= c : 2, 1, 3 */
				xa = sng_device_x(x2);
				ya = sng_device_y(y2);
				xb = sng_device_x(x1);
				yb = sng_device_y(y1);
				xc = sng_device_x(x3);
				yc = sng_device_y(y3);
			} else {
				/* a > b && c >=b && c < a: 2, 3, 1 */ 
				xa = sng_device_x(x2);
				ya = sng_device_y(y2);
				xb = sng_device_x(x3);
				yb = sng_device_y(y3);
				xc = sng_device_x(x1);
				yc = sng_device_y(y1);
			}
		}
	}
	/* now device coord vertices xa, ya, xb, yb, xc, yc are sorted by y value */
	scan_convert_sorted_triangle(w, gc, xa, ya, xb, yb, xc, yc);
}

void wireframe_render_entity(GtkWidget *w, GdkGC *gc, struct entity *e)
{
	int i;

	sng_set_foreground(e->color);
	for (i = 0; i < e->m->ntriangles; i++)
		wireframe_render_triangle(w, gc, &e->m->t[i]);
	nents++;
}

void wireframe_render_point_cloud(GtkWidget *w, GdkGC *gc, struct entity *e)
{
	int i;

	sng_set_foreground(e->color);
	for (i = 0; i < e->m->nvertices; i++)
		wireframe_render_point(w, gc, &e->m->v[i]);
}

void render_entity(GtkWidget *w, GdkGC *gc, struct entity *e)
{
	int i;
	float cos_theta;
	struct mat41 light = { {0, -1, -1, 1} };
	struct mat41 normal;

	for (i = 0; i < e->m->ntriangles; i++) {
		normal = *(struct mat41 *) &e->m->t[i].n.wx;
		normalize_vector(&normal, &normal);
		normalize_vector(&light, &light);
		cos_theta = mat41_dot_mat41(&light, &normal);
		cos_theta = (cos_theta + 1.0) / 2.0;
		sng_set_foreground((int) fmod((cos_theta * 256.0), 255.0) + GRAY);
		// sng_set_foreground(RED);
		scan_convert_triangle(w, gc, &e->m->t[i]);
	}
	nents++;
}

static void transform_fake_star(struct fake_star *fs, struct mat44 *transform)
{
	struct mat41 *m1, *m2;
	
	/* Set homogeneous coord to 1 initially for all vertices */
	fs->v.w = 1.0;

	/* Do the transformation... */
	m1 = (struct mat41 *) &fs->v.x;
	m2 = (struct mat41 *) &fs->v.wx;
	mat44_x_mat41(transform, m1, m2);
	/* normalize... */
	m2->m[0] /= m2->m[3];
	m2->m[1] /= m2->m[3];
	m2->m[2] /= m2->m[3];
}

static void transform_entity(struct entity *e, struct mat44 *transform)
{
	int i;
	struct mat41 *m1, *m2;

	/* calculate the object transform... */
	struct mat44 object_transform, total_transform, tmp_transform;
	struct mat44 object_rotation = {{{ 1, 0, 0, 0 },
					 { 0, 1, 0, 0 },
					 { 0, 0, 1, 0 },
					 { 0, 0, 0, 1 }}}; /* fixme, do this really. */
	struct mat44 object_translation = {{{ 1,    0,     0,    0 },
					    { 0,    1,     0,    0 },
					    { 0,    0,     1,    0 },
					    { e->x, e->y, e->z, 1 }}};
	/* for testing, do small rotation... */
	struct mat44 r1, r2;
#if 0
	mat44_rotate_x(&object_rotation, rx * M_PI / 180.0, &r1);  
	mat44_rotate_y(&r1, ry * M_PI / 180.0, &r2);  
	mat44_rotate_z(&r2, rz * M_PI / 180.0, &object_rotation);  
#else
	mat44_rotate_y(&object_rotation, e->ry, &r1);  
	mat44_rotate_x(&r1, e->rx, &r2);  
	mat44_rotate_z(&r2, e->rz, &object_rotation);  
#endif

	tmp_transform = *transform;
	mat44_product(&tmp_transform, &object_translation, &object_transform);
	mat44_product(&object_transform, &object_rotation, &total_transform);
#if 0
	mat44_product(&object_translation, &object_rotation, &object_transform);
	mat44_product(&object_transform, transform, &total_transform);
#endif
	
	/* Set homogeneous coord to 1 initially for all vertices */
	for (i = 0; i < e->m->nvertices; i++)
		e->m->v[i].w = 1.0;

	/* Do the transformation... */
	for (i = 0; i < e->m->nvertices; i++) {
		m1 = (struct mat41 *) &e->m->v[i].x;
		m2 = (struct mat41 *) &e->m->v[i].wx;
		mat44_x_mat41(&total_transform, m1, m2);
		/* normalize... */
		m2->m[0] /= m2->m[3];
		m2->m[1] /= m2->m[3];
		m2->m[2] /= m2->m[3];

	}
	
	for (i = 0; i < e->m->ntriangles; i++) {
		m1 = (struct mat41 *) &e->m->t[i].n.x;
		m2 = (struct mat41 *) &e->m->t[i].n.wx;
		mat44_x_mat41(&object_rotation, m1, m2);
		/* normalize... */
		m2->m[0] /= m2->m[3];
		m2->m[1] /= m2->m[3];
		m2->m[2] /= m2->m[3];
	}
}

static void insert_distance(int e, int *nsorted)
{
	int i, j, insertion_point = 0;

	for (i = 0; i < *nsorted; i++) {
		j = entity_depth[i];
		if (entity_list[j].dist3dsqrd <= entity_list[e].dist3dsqrd)
			break;
	}
	insertion_point = i;

	if (i < *nsorted) {
		memmove(&entity_depth[insertion_point + 1], &entity_depth[insertion_point],
				(*nsorted - insertion_point) * sizeof(entity_depth[0]));
	}
	entity_depth[insertion_point] = e;
	(*nsorted)++;
	return;
}

static void sort_entity_distances(void)
{
	int i;
	int nsorted;

	for (i = 0; i < snis_object_pool_highest_object(entity_pool); i++) {
		entity_list[i].dist3dsqrd = dist3dsqrd(
				camera.x - entity_list[i].x,
				camera.y - entity_list[i].y,
				camera.z - entity_list[i].z);
	}
	nsorted = 0;
	for (i = 0; i < snis_object_pool_highest_object(entity_pool); i++)
		insert_distance(i, &nsorted);
}

static inline float sqr(float a)
{
	return a * a;
}

static void reposition_fake_star(struct fake_star *fs, float radius);

void render_entities(GtkWidget *w, GdkGC *gc)
{
	int i, j;

	struct mat44 identity = {{{ 1, 0, 0, 0 }, /* identity matrix */
				    { 0, 1, 0, 0 },
				    { 0, 0, 1, 0 },
				    { 0, 0, 0, 1 }}};
	struct mat44 perspective_transform;
	struct mat44 cameralook_transform;
	struct mat44 tmp_transform;
	struct mat44 cameralocation_transform;
	struct mat44 total_transform;
	
	struct mat41 look_direction;

	struct mat41 up = { { 0, 1, 0, 0 } };
	struct mat41 camera_x, x_cross_look;
	struct mat41 *v; /* camera relative y axis (up/down) */ 
	struct mat41 *n; /* camera relative z axis (into view plane) */
	struct mat41 *u; /* camera relative x axis (left/right) */
	float camera_angle, ent_angle; /* this is for cheezy view culling */

	nents = 0;
	ntris = 0;
	nlines = 0;
	/* Translate to camera position... */
	mat44_translate(&identity, -camera.x, -camera.y, -camera.z,
				&cameralocation_transform);

	/* Calculate look direction, look direction, ... */
	look_direction.m[0] = (camera.lx - camera.x);
	look_direction.m[1] = (camera.ly - camera.y);
	look_direction.m[2] = (camera.lz - camera.z);
	look_direction.m[3] = 1.0;
	normalize_vector(&look_direction, &look_direction);
	n = &look_direction;

	camera_angle = atan2f(camera.lz - camera.z, camera.lx - camera.x);

	/* Calculate x direction relative to camera, "camera_x" */
	mat41_cross_mat41(&up, &look_direction, &camera_x);
	normalize_vector(&camera_x, &camera_x);
	u = &camera_x;

	/* Calculate camera relative x axis */
	v = &x_cross_look;
	mat41_cross_mat41(n, u, v);
	/* v should not need normalizing as n and u are already
	 * unit, and are perpendicular */
	normalize_vector(v, v);

	/* Make a rotation matrix...
	   | ux uy uz 0 |
	   | vx vy vz 0 |
	   | nx ny nz 0 |
	   |  0  0  0 1 |
	 */
	cameralook_transform.m[0][0] = u->m[0];
	cameralook_transform.m[0][1] = v->m[0];
	cameralook_transform.m[0][2] = n->m[0];
	cameralook_transform.m[0][3] = 0.0;
	cameralook_transform.m[1][0] = u->m[1];
	cameralook_transform.m[1][1] = v->m[1];
	cameralook_transform.m[1][2] = n->m[1];
	cameralook_transform.m[1][3] = 0.0;
	cameralook_transform.m[2][0] = u->m[2];
	cameralook_transform.m[2][1] = v->m[2];
	cameralook_transform.m[2][2] = n->m[2];
	cameralook_transform.m[2][3] = 0.0;
	cameralook_transform.m[3][0] = 0.0;
	cameralook_transform.m[3][1] = 0.0;
	cameralook_transform.m[3][2] = 0.0; 
	cameralook_transform.m[3][3] = 1.0;

	/* Make perspective transform... */
	perspective_transform.m[0][0] = (2 * camera.near) / camera.width;
	perspective_transform.m[0][1] = 0.0;
	perspective_transform.m[0][2] = 0.0;
	perspective_transform.m[0][3] = 0.0;
	perspective_transform.m[1][0] = 0.0;
	perspective_transform.m[1][1] = (2.0 * camera.near) / camera.height;
	perspective_transform.m[1][2] = 0.0;
	perspective_transform.m[1][3] = 0.0;
	perspective_transform.m[2][0] = 0.0;
	perspective_transform.m[2][1] = 0.0;
	perspective_transform.m[2][2] = -(camera.far + camera.near) /
						(camera.far - camera.near);
	perspective_transform.m[2][3] = -1.0;
	perspective_transform.m[3][0] = 0.0;
	perspective_transform.m[3][1] = 0.0;
	perspective_transform.m[3][2] = (-2 * camera.far * camera.near) /
						(camera.far - camera.near);
	perspective_transform.m[3][3] = 0.0;

	mat44_product(&perspective_transform, &cameralook_transform, &tmp_transform);
	mat44_product(&tmp_transform, &cameralocation_transform, &total_transform);

	/* draw fake stars */
	sng_set_foreground(WHITE);
	for (i = 0; i < nfakestars; i++) {
		float behind_camera;
		struct fake_star *fs = &fake_star[i];
		struct mat41 point_to_test;

		point_to_test.m[0] = fs->v.x - camera.x;
		point_to_test.m[1] = fs->v.y - camera.y;
		point_to_test.m[2] = fs->v.z - camera.z;
		point_to_test.m[3] = 1.0;

		fs->dist3dsqrd = dist3dsqrd(camera.x - fs->v.x,
						camera.y - fs->v.y,
						camera.z - fs->v.z);

		behind_camera = mat41_dot_mat41(&look_direction, &point_to_test);
		if (behind_camera < 0) /* behind camera */
			goto check_for_reposition;

		/* Really cheezy view culling */
		ent_angle = atan2f(point_to_test.m[2], point_to_test.m[0]);
		ent_angle = fabs(ent_angle - camera_angle);
		if (ent_angle > M_PI)
			ent_angle = (2 * M_PI - ent_angle);
		if (ent_angle > (camera.angle_of_view / 2.0) * M_PI / 180.0)
			goto check_for_reposition;

		transform_fake_star(fs, &total_transform);
		wireframe_render_fake_star(w, gc, fs);

check_for_reposition:
		if (fs->dist3dsqrd > camera.far * 10.0f * camera.far * 10.0f)
			reposition_fake_star(fs, camera.far * 10.0f);
	}

	sort_entity_distances();
	   
	for (j = 0; j < snis_object_pool_highest_object(entity_pool); j++) {
		struct mat41 point_to_test;
		float behind_camera;

		i = entity_depth[j];

		if (!snis_object_pool_is_allocated(entity_pool, i))
			continue;
		if (entity_list[i].m == NULL)
			continue;
		point_to_test.m[0] = entity_list[i].x - camera.x;
		point_to_test.m[1] = entity_list[i].y - camera.y;
		point_to_test.m[2] = entity_list[i].z - camera.z;
		point_to_test.m[3] = 1.0;
		behind_camera = mat41_dot_mat41(&look_direction, &point_to_test);
		if (behind_camera < 0) /* behind camera */
			continue;

/* increasing STANDARD_RADIUS makes fewer objects visible, decreasing it makes more */
#define STANDARD_RADIUS (4.0)
		if (entity_list[i].dist3dsqrd * STANDARD_RADIUS / entity_list[i].m->radius >
				sqr(fabs(camera.far) * 20.0))
			continue;

		/* Really cheezy view culling */
		ent_angle = atan2f(point_to_test.m[2], point_to_test.m[0]);
		ent_angle = fabs(ent_angle - camera_angle);
		if (ent_angle > M_PI)
			ent_angle = (2 * M_PI - ent_angle);
		if (ent_angle > (camera.angle_of_view / 2.0) * M_PI / 180.0)
			continue;

		transform_entity(&entity_list[i], &total_transform);
		if (entity_list[i].render_as_point_cloud)
			wireframe_render_point_cloud(w, gc, &entity_list[i]);
		else
			render_entity(w, gc, &entity_list[i]);
	}
	// printf("ntris = %lu, nlines = %lu, nents = %lu\n", ntris, nlines, nents);
	rx = fmod(rx + 0.3, 360.0);
	ry = fmod(ry + 0.15, 360.0);
	rz = fmod(rz + 0.6, 360.0);
}

void camera_set_pos(float x, float y, float z)
{
	camera.x = x;
	camera.y = y;
	camera.z = z;
}

void camera_look_at(float x, float y, float z)
{
	camera.lx = x;
	camera.ly = y;
	camera.lz = z;
}

void camera_set_parameters(float near, float far, float width, float height,
				int xvpixels, int yvpixels, float angle_of_view)
{
	camera.near = -near;
	camera.far = -far;
	camera.width = width;
	camera.height = height;	
	camera.xvpixels = xvpixels;
	camera.yvpixels = yvpixels;
	camera.angle_of_view = angle_of_view;
}

void entity_init(void)
{
	snis_object_pool_setup(&entity_pool, MAX_ENTITIES);
}

/* fill a sphere of specified radius with randomly placed stars */
void entity_init_fake_stars(int nstars, float radius)
{
	int i;

	fake_star = malloc(sizeof(*fake_star) * nstars);
	memset(fake_star, 0, sizeof(*fake_star) * nstars);

	for (i = 0; i < nstars; i++) {
		struct fake_star *fs = &fake_star[i];

		random_point_in_sphere(radius, &fs->v.x, &fs->v.y, &fs->v.z,
					&fs->dist3dsqrd);
		fs->v.x += camera.x;
		fs->v.y += camera.y;
		fs->v.z += camera.z;
	}
	nfakestars = nstars;
}

/* Re-position a star randomly on the surface of sphere of given radius */
static void reposition_fake_star(struct fake_star *fs, float radius)
{
	/* I tried "on" sphere here, but I like the look of "in" better. */
	random_point_in_sphere(radius, &fs->v.x, &fs->v.y, &fs->v.z, &fs->dist3dsqrd);
	fs->v.x += camera.x;
	fs->v.y += camera.y;
	fs->v.z += camera.z;
}

void entity_free_fake_stars(void)
{
	nfakestars = 0;
	free(fake_star);
}

