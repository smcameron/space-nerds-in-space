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

#define MAX_TRIANGLES_PER_ENTITY 10000

struct fake_star {
	struct vertex v;
	float dist3dsqrd;
};

struct entity {
	struct mesh *m;
	float x, y, z; /* world coords */
	float rx, ry, rz;
	float sx, sy; /* screen coords */
	float scale;
	float dist3dsqrd;
	int color;
	int shadecolor;
	int render_style;
	void *user_data;
	union quat orientation;
	unsigned char onscreen;
};

struct camera_info {
	float x, y, z;		/* position of camera */
	float lx, ly, lz;	/* where camera is looking */
	float ux, uy, uz;	/* up vector */
	float near, far, right, left, top, bottom;
	float angle_of_view;
	int xvpixels, yvpixels;
	int renderer;
	struct mat41 look_direction;
	struct mat44 camera_transform;
};

struct tri_depth_entry {
	int tri_index;
	float depth;
};

struct entity_context {
	int maxobjs;
	struct snis_object_pool *entity_pool;
	struct entity *entity_list; /* array, [maxobjs] */
	int *entity_depth; /* array [maxobjs] */
	// unsigned long ntris, nents, nlines;
	struct camera_info camera;
	struct tri_depth_entry tri_depth[MAX_TRIANGLES_PER_ENTITY];
	struct fake_star *fake_star;
	int nfakestars; /* = 0; */
	struct mat41 light;
	float window_offset_x, window_offset_y;
#ifdef WITH_ILDA_SUPPORT
	int framenumber;
	FILE *f;
#endif
};

struct clip_triangle {
	struct mat41 v[3]; /* three vertices */
};
static int clipTriangle(struct clip_triangle* triangles, struct mat41* vsOutput0, struct mat41* vsOutput1, struct mat41* vsOutput2);

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
	cx->entity_list[n].rx = 0;
	cx->entity_list[n].ry = 0;
	cx->entity_list[n].rz = 0;
	cx->entity_list[n].scale = 1.0;
	cx->entity_list[n].color = color;
	cx->entity_list[n].render_style = RENDER_NORMAL;
	cx->entity_list[n].user_data = NULL;
	cx->entity_list[n].shadecolor = 0;
	quat_init_axis(&cx->entity_list[n].orientation, 0, 1, 0, 0);
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

static int is_backface(float x1, float y1, float x2, float y2, float x3, float y3)
{
	int twicearea;

	twicearea =	(x1 * y2 - x2 * y1) +
			(x2 * y3 - x3 * y2) +
			(x3 * y1 - x1 * y3);
	return twicearea > 0;
}

static void wireframe_render_fake_star(GtkWidget *w, GdkGC *gc,
	struct entity_context *cx, struct fake_star *fs)
{
	float x1, y1;

	x1 = (fs->v.wx * cx->camera.xvpixels / 2) + cx->camera.xvpixels / 2 + cx->window_offset_x;
	y1 = (-fs->v.wy * cx->camera.yvpixels / 2) + cx->camera.yvpixels / 2 + cx->window_offset_y;
	sng_current_draw_line(w->window, gc, x1, y1, x1 + (snis_randn(10) < 7), y1); 
}

void wireframe_render_triangle(GtkWidget *w, GdkGC *gc,
		struct entity_context *cx, struct triangle *t)
{
	struct vertex *v1, *v2, *v3;
	float x1, y1, x2, y2, x3, y3;
	struct camera_info *c = &cx->camera;

	v1 = t->v[0];
	v2 = t->v[1];
	v3 = t->v[2];

	x1 = (v1->wx * c->xvpixels / 2) + c->xvpixels / 2 + cx->window_offset_x;
	x2 = (v2->wx * c->xvpixels / 2) + c->xvpixels / 2 + cx->window_offset_x;
	x3 = (v3->wx * c->xvpixels / 2) + c->xvpixels / 2 + cx->window_offset_x;
	y1 = (-v1->wy * c->yvpixels / 2) + c->yvpixels / 2 + cx->window_offset_y;
	y2 = (-v2->wy * c->yvpixels / 2) + c->yvpixels / 2 + cx->window_offset_y;
	y3 = (-v3->wy * c->yvpixels / 2) + c->yvpixels / 2 + cx->window_offset_y;

	if (is_backface(x1, y1, x2, y2, x3, y3))
		return;

	if ( !(t->flag & TRIANGLE_0_1_COPLANAR))
		sng_current_draw_line(w->window, gc, x1, y1, x2, y2);
	if ( !(t->flag & TRIANGLE_1_2_COPLANAR))
		sng_current_draw_line(w->window, gc, x2, y2, x3, y3);
	if ( !(t->flag & TRIANGLE_0_2_COPLANAR))
		sng_current_draw_line(w->window, gc, x3, y3, x1, y1);
}

void wireframe_render_point(GtkWidget *w, GdkGC *gc,
		struct entity_context *cx, struct vertex *v)
{
	float x1, y1;

	x1 = (v->wx * cx->camera.xvpixels / 2) + cx->camera.xvpixels / 2 + cx->window_offset_x;
	y1 = (-v->wy * cx->camera.yvpixels / 2) + cx->camera.yvpixels / 2 + cx->window_offset_y;
	sng_current_draw_line(w->window, gc, x1, y1, x1 + 1, y1); 
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

static void scan_convert_triangle(GtkWidget *w, GdkGC *gc, struct entity_context *cx,
				struct triangle *t, int color,
				int render_style)
{
	/* int xa, ya, xb, yb, xc, yc; */
	struct camera_info *c = &cx->camera;

	int ntriangles;
	struct clip_triangle triangles[12];

	struct mat41* mv0 = (struct mat41 *) &t->v[0]->wx;
	struct mat41* mv1 = (struct mat41 *) &t->v[1]->wx;
	struct mat41* mv2 = (struct mat41 *) &t->v[2]->wx;

	/* part of triangle is behind screen so try and clip it */
	if (mv0->m[2] < -mv0->m[3] || mv1->m[2] < -mv1->m[3] || mv2->m[2] < -mv2->m[3]) {
		ntriangles = clipTriangle(&triangles[0], mv0, mv1, mv2);
	} else {
		/* not clipped by near plane so just draw it and not worry about the back or sides */
		ntriangles = 1;
		triangles[0].v[0] = *mv0;
		triangles[0].v[1] = *mv1;
		triangles[0].v[2] = *mv2;
	}

	int i;
	for (i=0; i<ntriangles; i++) {
		/* do the w divide now that the triangles are culled and clipped */
		float v1wx = triangles[i].v[0].m[0] / triangles[i].v[0].m[3];
		float v1wy = triangles[i].v[0].m[1] / triangles[i].v[0].m[3];
		float v2wx = triangles[i].v[1].m[0] / triangles[i].v[1].m[3];
		float v2wy = triangles[i].v[1].m[1] / triangles[i].v[1].m[3];
		float v3wx = triangles[i].v[2].m[0] / triangles[i].v[2].m[3];
		float v3wy = triangles[i].v[2].m[1] / triangles[i].v[2].m[3];

		float x1, y1, x2, y2, x3, y3;
		x1 = (v1wx * c->xvpixels / 2) + c->xvpixels / 2 + cx->window_offset_x;
		x2 = (v2wx * c->xvpixels / 2) + c->xvpixels / 2 + cx->window_offset_x;
		x3 = (v3wx * c->xvpixels / 2) + c->xvpixels / 2 + cx->window_offset_x;
		y1 = (-v1wy * c->yvpixels / 2) + c->yvpixels / 2 + cx->window_offset_y;
		y2 = (-v2wy * c->yvpixels / 2) + c->yvpixels / 2 + cx->window_offset_y;
		y3 = (-v3wy * c->yvpixels / 2) + c->yvpixels / 2 + cx->window_offset_y;

		if (is_backface(x1, y1, x2, y2, x3, y3))
			return;

		if (!(render_style & RENDER_NO_FILL) && cx->camera.renderer != WIREFRAME_RENDERER) {
			sng_filled_tri(w->window, gc, x1, y1, x2, y2, x3, y3);
		}

		if (cx->camera.renderer & WIREFRAME_RENDERER || render_style & RENDER_WIREFRAME) {
			if (render_style & RENDER_BRIGHT_LINE) {
				if ( !(t->flag & TRIANGLE_0_1_COPLANAR))
					sng_current_bright_line(w->window, gc, x1, y1, x2, y2, color); 
				if ( !(t->flag & TRIANGLE_1_2_COPLANAR))
					sng_current_bright_line(w->window, gc, x2, y2, x3, y3, color); 
				if ( !(t->flag & TRIANGLE_0_2_COPLANAR))
					sng_current_bright_line(w->window, gc, x3, y3, x1, y1, color); 
			} else {
				sng_set_foreground(color);
				if ( !(t->flag & TRIANGLE_0_1_COPLANAR))
					sng_current_draw_line(w->window, gc, x1, y1, x2, y2); 
				if ( !(t->flag & TRIANGLE_1_2_COPLANAR))
					sng_current_draw_line(w->window, gc, x2, y2, x3, y3); 
				if ( !(t->flag & TRIANGLE_0_2_COPLANAR))
					sng_current_draw_line(w->window, gc, x3, y3, x1, y1); 
			}
	#ifdef WITH_ILDA_SUPPORT
			if (render_style & RENDER_ILDA)
				ilda_render_triangle(cx, x1, y1, x2, y2, x3, y3);
	#endif
		}
	}
}

void wireframe_render_entity(GtkWidget *w, GdkGC *gc, struct entity_context *cx,
			struct entity *e)
{
	int i;

	sng_set_foreground(e->color);
	for (i = 0; i < e->m->ntriangles; i++)
		wireframe_render_triangle(w, gc, cx, &e->m->t[i]);
}

void wireframe_render_point_cloud(GtkWidget *w, GdkGC *gc, struct entity_context *cx,
				struct entity *e)
{
	int i;

	sng_set_foreground(e->color);
	if (e->render_style & RENDER_SPARKLE) {
		for (i = 0; i < e->m->nvertices; i++)
			if (snis_randn(100) < 35)
				wireframe_render_point(w, gc, cx, &e->m->v[i]);
	} else {
		for (i = 0; i < e->m->nvertices; i++)
			wireframe_render_point(w, gc, cx, &e->m->v[i]);
	}
}

void wireframe_render_point_line(GtkWidget *w, GdkGC *gc, struct entity_context *cx,
				struct entity *e)
{
	int i;
	float x1, y1;
	float x2, y2;

	sng_set_foreground(e->color);
	for (i = 0; i < e->m->nlines; i++) {
		struct vertex* vstart = e->m->l[i].start;
		struct vertex* vend = e->m->l[i].end;

		if (e->m->l[i].flag & MESH_LINE_STRIP) {
			struct vertex* vcurr = vstart;

			while (vcurr <= vend) {
				x2 = (vcurr->wx * cx->camera.xvpixels / 2) +
							cx->camera.xvpixels / 2 + cx->window_offset_x;
				y2 = (-vcurr->wy * cx->camera.yvpixels / 2) +
							cx->camera.yvpixels / 2 + cx->window_offset_y;
				if (vcurr != vstart) {
					if (e->m->l[i].flag & MESH_LINE_DOTTED)
						sng_draw_dotted_line(w->window, gc, x1, y1, x2, y2);
					else
						sng_current_draw_line(w->window, gc, x1, y1, x2, y2);
				}
				x1 = x2;
				y1 = y2;
				++vcurr;
			}
		} else {
			x1 = (vstart->wx * cx->camera.xvpixels / 2) + cx->camera.xvpixels / 2
					+ cx->window_offset_x;
			y1 = (-vstart->wy * cx->camera.yvpixels / 2) + cx->camera.yvpixels / 2
					+ cx->window_offset_y;
			x2 = (vend->wx * cx->camera.xvpixels / 2) + cx->camera.xvpixels / 2
					+ cx->window_offset_x;
			y2 = (-vend->wy * cx->camera.yvpixels / 2) + cx->camera.yvpixels / 2
					+ cx->window_offset_y;
			if (e->m->l[i].flag & MESH_LINE_DOTTED)
				sng_draw_dotted_line(w->window, gc, x1, y1, x2, y2);
			else
				sng_current_draw_line(w->window, gc, x1, y1, x2, y2);
		}
	}
}

static int tri_depth_compare(const void *a, const void *b)
{
	const struct tri_depth_entry *A = a;
	const struct tri_depth_entry *B = b;

	if (A->depth > B->depth)
		return -1;
	if (A->depth < B->depth)
		return 1;
	return 0;
}

static void sort_triangle_distances(struct entity_context *cx, struct entity *e)
{
	int i;

	if (e->m->ntriangles > MAX_TRIANGLES_PER_ENTITY)
		printf("Too many triangles, %d vs %d at %s:%d\n",
			e->m->ntriangles, MAX_TRIANGLES_PER_ENTITY,
			__FILE__, __LINE__);

	/* Initialize array */
	for (i = 0; i < e->m->ntriangles; i++) {
		float dist;
		struct triangle *t = &e->m->t[i];
		dist = t->v[0]->wz + t->v[1]->wz + t->v[2]->wz;
		dist = dist / 3.0;
		cx->tri_depth[i].tri_index = i;
		cx->tri_depth[i].depth = dist;
	}

	if (e->render_style & RENDER_NO_FILL)
		return;

	/* Sort the triangles */
	qsort(cx->tri_depth, e->m->ntriangles, sizeof(cx->tri_depth[0]), tri_depth_compare);
}

void render_entity(GtkWidget *w, GdkGC *gc, struct entity_context *cx, struct entity *e)
{
	int i;
	float cos_theta;
	struct mat41 normal;

	sort_triangle_distances(cx, e);

	for (i = 0; i < e->m->ntriangles; i++) {
		int tri_index = cx->tri_depth[i].tri_index;
		normal = *(struct mat41 *) &e->m->t[tri_index].n.wx;
		normalize_vector(&normal, &normal);
		cos_theta = mat41_dot_mat41(&cx->light, &normal);
		cos_theta = (cos_theta + 1.0) / 2.0;
		if (cx->camera.renderer & BLACK_TRIS || e->render_style & RENDER_WIREFRAME)
			sng_set_foreground(BLACK);
		else
			sng_set_foreground((int) fmod((cos_theta * 240.0), 240.0) +
					GRAY + (NSHADESOFGRAY * e->shadecolor) + 10);
		scan_convert_triangle(w, gc, cx, &e->m->t[tri_index], e->color,
					e->render_style);
	}
}

static int transform_fake_star(struct entity_context *cx,
				struct fake_star *fs, struct mat44 *transform)
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
	if (fs->v.wx < -1.0 || fs->v.wx > 1.0 ||
		fs->v.wy < -1.0 || fs->v.wy > 1.0)
		return 1;
	return 0;
}

int transform_point(struct entity_context *cx,
			float x, float y, float z, float *sx, float *sy, int do_clip)
{
	struct vertex t;

	/* calculate the object transform... */
	struct mat44 object_transform;
	struct mat44 object_translation = {{{ 1, 0, 0, 0 },
					    { 0, 1, 0, 0 },
					    { 0, 0, 1, 0 },
					    { x, y, z, 1 }}};
	mat44_product(&cx->camera.camera_transform, &object_translation, &object_transform);
	
	/* calculate screen coords */
	t.x = 0;
	t.y = 0;
	t.z = 0;
	t.w = 1.0;
	mat44_x_mat41(&object_transform, (struct mat41 *) &t.x, (struct mat41 *) &t.wx);
	t.wx /= t.ww;
	t.wy /= t.ww;
	t.wz /= t.ww;
	*sx = cx->window_offset_x + (t.wx * cx->camera.xvpixels / 2) + cx->camera.xvpixels / 2;
	*sy = cx->window_offset_y + (-t.wy * cx->camera.yvpixels / 2) + cx->camera.yvpixels / 2;

	if (do_clip) {
		/* consider rendering stuff that is also 50% off the screen
		   to stop objects popping in on rotate */
		if (t.wx < -1.5 || t.wx > 1.5 || t.wy < -1.5 || t.wy > 1.5) /* off screen? */
			return 1; /* clip it. */
	}
	return 0;
}

static int transform_entity(struct entity_context *cx,
				struct entity *e,struct mat44 *transform, int do_clip)
{
	int i;
	struct mat41 *m1, *m2;
	struct vertex t;

	/* calculate the object transform... */
	struct mat44 object_transform, total_transform, tmp_transform;
	struct mat44 object_scale = {{{ e->scale, 0, 0, 0 },
					 { 0, e->scale, 0, 0 },
					 { 0, 0, e->scale, 0 },
					 { 0, 0, 0, 1 }}}; /* fixme, do this really. */
	struct mat44 object_translation = {{{ 1,    0,     0,    0 },
					    { 0,    1,     0,    0 },
					    { 0,    0,     1,    0 },
					    { e->x, e->y, e->z, 1 }}};
	struct mat44 orientation, object_rotation;

	quat_to_rh_rot_matrix(&e->orientation, &orientation.m[0][0]);
	mat44_product(&orientation, &object_scale, &object_rotation);  

	tmp_transform = *transform;
	mat44_product(&tmp_transform, &object_translation, &object_transform);
	mat44_product(&object_transform, &object_rotation, &total_transform);
	
	/* calculate screen coords of entity as a whole */
	t.x = 0;
	t.y = 0;
	t.z = 0;
	t.w = 1.0;
	mat44_x_mat41(&total_transform, (struct mat41 *) &t.x, (struct mat41 *) &t.wx);
	t.wx /= t.ww;
	t.wy /= t.ww;
	t.wz /= t.ww;
	e->sx = cx->window_offset_x + (t.wx * cx->camera.xvpixels / 2) + cx->camera.xvpixels / 2;
	e->sy = cx->window_offset_y + (-t.wy * cx->camera.yvpixels / 2) + cx->camera.yvpixels / 2;

	if (do_clip) {
		/* consider rendering stuff that is also 50% off the screen
		   to stop objects popping in on rotate */
		if (t.wx < -1.5 || t.wx > 1.5 || t.wy < -1.5 || t.wy > 1.5) /* off screen? */
			return 1; /* clip it. */
	}

	/* Set homogeneous coord to 1 initially for all vertices */
	for (i = 0; i < e->m->nvertices; i++)
		e->m->v[i].w = 1.0;

	/* Do the transformation... */
	for (i = 0; i < e->m->nvertices; i++) {
		m1 = (struct mat41 *) &e->m->v[i].x;
		m2 = (struct mat41 *) &e->m->v[i].wx;
		mat44_x_mat41(&total_transform, m1, m2);
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
	return 0;
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
	int i, n;
	struct camera_info *c = &cx->camera;

	n = snis_object_pool_highest_object(cx->entity_pool);

	for (i = 0; i <= n; i++) {
		cx->entity_list[i].dist3dsqrd = dist3dsqrd(
				c->x - cx->entity_list[i].x,
				c->y - cx->entity_list[i].y,
				c->z - cx->entity_list[i].z);
		cx->entity_depth[i] = i;
	}
#if defined(__APPLE__)  || defined(__FreeBSD__)
	qsort_r(cx->entity_depth, n, sizeof(cx->entity_depth[0]), cx, object_depth_compare);
#else
	qsort_r(cx->entity_depth, n, sizeof(cx->entity_depth[0]), object_depth_compare, cx);
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

void calculate_camera_transform(struct entity_context *cx)
{
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

	struct mat41 up;
	struct mat41 camera_x, x_cross_look;
	struct mat41 *v; /* camera relative y axis (up/down) */ 
	struct mat41 *n; /* camera relative z axis (into view plane) */
	struct mat41 *u; /* camera relative x axis (left/right) */

	up.m[0] = cx->camera.ux;
	up.m[1] = cx->camera.uy;
	up.m[2] = cx->camera.uz;
	up.m[3] = 1;

	normalize_vector(&cx->light, &cx->light);

	/* Translate to camera position... */
	mat44_translate(&identity, -cx->camera.x, -cx->camera.y, -cx->camera.z,
				&cameralocation_transform);

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
	look_direction.m[0] = (cx->camera.lx - cx->camera.x);
	look_direction.m[1] = (cx->camera.ly - cx->camera.y);
	look_direction.m[2] = (cx->camera.lz - cx->camera.z);
	look_direction.m[3] = 1.0;
	normalize_vector(&look_direction, &look_direction);
	cx->camera.look_direction = look_direction;
	n = &look_direction;

	/* Calculate x direction relative to camera, "camera_x" */
	mat41_cross_mat41(&look_direction, &up, &camera_x);
	normalize_vector(&camera_x, &camera_x);
	u = &camera_x;

	/* Calculate camera relative x axis */
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

	mat44_product(&perspective_transform, &cameralook_transform, &tmp_transform);
	mat44_product(&tmp_transform, &cameralocation_transform, &total_transform);
	cx->camera.camera_transform = total_transform;
}

static void reposition_fake_star(struct entity_context *cx, struct fake_star *fs, float radius);

static void do_w_divide(struct entity *e)
{
	int i;
	for (i = 0; i < e->m->nvertices; i++) {
		e->m->v[i].wx /= e->m->v[i].ww;
		e->m->v[i].wy /= e->m->v[i].ww;
		e->m->v[i].wz /= e->m->v[i].ww;
	}
}

void render_entities(GtkWidget *w, GdkGC *gc, struct entity_context *cx)
{
	int i, j;
	int clipped, do_clip;

	calculate_camera_transform(cx);

#ifdef WITH_ILDA_SUPPORT
	ilda_file_open(cx);
	ilda_file_newframe(cx);
#endif

	/* draw fake stars */
	sng_set_foreground(WHITE);
	for (i = 0; i < cx->nfakestars; i++) {
		float behind_camera;
		struct fake_star *fs = &cx->fake_star[i];
		struct mat41 point_to_test;

		point_to_test.m[0] = fs->v.x - cx->camera.x;
		point_to_test.m[1] = fs->v.y - cx->camera.y;
		point_to_test.m[2] = fs->v.z - cx->camera.z;
		point_to_test.m[3] = 1.0;

		fs->dist3dsqrd = dist3dsqrd(cx->camera.x - fs->v.x,
						cx->camera.y - fs->v.y,
						cx->camera.z - fs->v.z);

		behind_camera = mat41_dot_mat41(&cx->camera.look_direction, &point_to_test);
		if (behind_camera < 0) /* behind camera */
			goto check_for_reposition;
		if (!transform_fake_star(cx, fs, &cx->camera.camera_transform))
			wireframe_render_fake_star(w, gc, cx, fs);
check_for_reposition:
		if (fs->dist3dsqrd > cx->camera.far * 10.0f * cx->camera.far * 10.0f)
			reposition_fake_star(cx, fs, cx->camera.far * 10.0f);
	}

	sort_entity_distances(cx);
	   
	for (j = 0; j <= snis_object_pool_highest_object(cx->entity_pool); j++) {
		struct mat41 point_to_test;
		float behind_camera;

		i = cx->entity_depth[j];

		if (!snis_object_pool_is_allocated(cx->entity_pool, i))
			continue;
		if (cx->entity_list[i].m == NULL)
			continue;
		cx->entity_list[i].onscreen = 0;

		do_clip = !(cx->entity_list[i].render_style & RENDER_DISABLE_CLIP);
		if (do_clip) {
			point_to_test.m[0] = cx->entity_list[i].x - cx->camera.x;
			point_to_test.m[1] = cx->entity_list[i].y - cx->camera.y;
			point_to_test.m[2] = cx->entity_list[i].z - cx->camera.z;
			point_to_test.m[3] = 1.0;
			behind_camera = mat41_dot_mat41(&cx->camera.look_direction, &point_to_test);
			if (behind_camera < 0) /* behind camera */
				continue;
/* increasing STANDARD_RADIUS makes fewer objects visible, decreasing it makes more */
#define STANDARD_RADIUS (4.0)
			if (cx->entity_list[i].dist3dsqrd * STANDARD_RADIUS / cx->entity_list[i].m->radius * cx->entity_list[i].scale >
					sqr(fabs(cx->camera.far) * 20.0))
				continue;
		}

		clipped = transform_entity(cx, &cx->entity_list[i], &cx->camera.camera_transform, do_clip);
		if (clipped)
			continue;

		if (cx->entity_list[i].render_style & RENDER_POINT_CLOUD) {
			do_w_divide(&cx->entity_list[i]);
			wireframe_render_point_cloud(w, gc, cx, &cx->entity_list[i]);
		} else if (cx->entity_list[i].render_style & RENDER_POINT_LINE) {
			do_w_divide(&cx->entity_list[i]);
			wireframe_render_point_line(w, gc, cx, &cx->entity_list[i]);
		} else {
			render_entity(w, gc, cx, &cx->entity_list[i]);
		}
		cx->entity_list[i].onscreen = 1;
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
	cx->light.m[0] = 0;
	cx->light.m[1] = 1;
	cx->light.m[2] = 1;
	cx->light.m[3] = 1;
	camera_assign_up_direction(cx, 0.0, 1.0, 0.0);
	cx->camera.ux = 0;
	cx->camera.uy = 1;
	cx->camera.uz = 0;
	cx->window_offset_x = 0;
	cx->window_offset_y = 0;
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

/* fill a sphere of specified radius with randomly placed stars */
void entity_init_fake_stars(struct entity_context *cx, int nstars, float radius)
{
	int i;

	cx->fake_star = malloc(sizeof(*cx->fake_star) * nstars);
	memset(cx->fake_star, 0, sizeof(*cx->fake_star) * nstars);

	for (i = 0; i < nstars; i++) {
		struct fake_star *fs = &cx->fake_star[i];

		random_point_in_sphere(radius, &fs->v.x, &fs->v.y, &fs->v.z,
					&fs->dist3dsqrd);
		fs->v.x += cx->camera.x;
		fs->v.y += cx->camera.y;
		fs->v.z += cx->camera.z;
	}
	cx->nfakestars = nstars;
}

/* Re-position a star randomly on the surface of sphere of given radius */
static void reposition_fake_star(struct entity_context *cx, struct fake_star *fs, float radius)
{
	/* I tried "on" sphere here, but I like the look of "in" better. */
	random_point_in_sphere(radius, &fs->v.x, &fs->v.y, &fs->v.z, &fs->dist3dsqrd);
	fs->v.x += cx->camera.x;
	fs->v.y += cx->camera.y;
	fs->v.z += cx->camera.z;
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

struct mat44 get_camera_transform(struct entity_context *cx)
{
	return cx->camera.camera_transform;
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

/* clipping triangles to frustrum taken from
   http://simonstechblog.blogspot.com/2012/04/software-rasterizer-part-2.html
   converted from javascript to c */
static const float ZERO_TOLERANCE = 0.000001f;

typedef float (*func_clipLine)(float vtx0x, float vtx0w, float vtx1x, float vtx1w );
typedef float (*func_getComp)(const struct mat41* m);
typedef int (*func_isOutside)(float x, float w);

// return a pareameter t for line segment within range [0, 1] is intersect
static float clipLine_pos(float vtx0x, float vtx0w, float vtx1x, float vtx1w )
{
	float denominator= vtx0w-vtx1w-vtx0x+vtx1x;
	if (fabs(denominator) < ZERO_TOLERANCE)
		return -1;	// line parallel to the plane...

	float t = (vtx0w - vtx0x)/denominator;
	if (t>=0.0 && t<=1.0)
		return t;	// intersect
	else
		return -1;	// line segment not intersect
}

static float clipLine_neg(float vtx0x, float vtx0w, float vtx1x, float vtx1w)
{
	float denominator= vtx0w-vtx1w+vtx0x-vtx1x;
	if (fabs(denominator) < ZERO_TOLERANCE)
		return -1;	// line parallel to the plane...

	float t= (vtx0w + vtx0x)/denominator;
	if (t>=0.0 && t<=1.0)
		return t;	// intersect
	else
		return -1;	// line segment not intersect
}

static int isOutside_pos(float x, float w) { 
	return x > w;
}

static int isOutside_neg(float x, float w) {
	return x < -w;
}

static float getX(const struct mat41* m) {
	return m->m[0];
}
static float getY(const struct mat41* m) {
	return m->m[1];
}
static float getZ(const struct mat41* m) {
	return m->m[2];
}

static void mat41_lerp(struct mat41* out, struct mat41* v1, struct mat41* v2, float t)
{
	float invT = 1.0-t;
	out->m[0] = t * v2->m[0] + invT * v1->m[0];
	out->m[1] = t * v2->m[1] + invT * v1->m[1];
	out->m[2] = t * v2->m[2] + invT * v1->m[2];
	out->m[3] = t * v2->m[3] + invT * v1->m[3];
}

// the clip line to plane with interpolated varying
static void clippedLineFromPlane(struct mat41* out, float t, struct mat41* vsOutput0, struct mat41* vsOutput1)
{
	if (t<0)
		printf("Assert: two vertex does not intersect plane\n");

	mat41_lerp(out, vsOutput0, vsOutput1, t);
}

static int clipTriangleToPlane(
	struct clip_triangle* clipped_triangles,
	func_clipLine clippingFunc,
	struct mat41* vsOutput0, struct mat41* vsOutput1, struct mat41* vsOutput2,
	int isVtx0outside, int isVtx1outside, int isVtx2outside,
	float component0, float component1, float component2,
	float pos0w, float pos1w, float pos2w)
{
	if (isVtx0outside && isVtx1outside && isVtx2outside)
	{
		// whole triangle get clipped
		return 0;
	}
	else if (!isVtx0outside && isVtx1outside && isVtx2outside)
	{
		// 2 vtx outside, split into 1 smaller triangles
		float line0_t = clippingFunc(component1, pos1w, component0, pos0w);
		float line2_t = clippingFunc(component2, pos2w, component0, pos0w);
		clipped_triangles[0].v[0] = *vsOutput0;
		clippedLineFromPlane(&clipped_triangles[0].v[1], line0_t, vsOutput1, vsOutput0);
		clippedLineFromPlane(&clipped_triangles[0].v[2], line2_t, vsOutput2, vsOutput0);
		return 1;
	}
	else if (isVtx0outside && !isVtx1outside && isVtx2outside)
	{
		float line0_t = clippingFunc( component0, pos0w, component1, pos1w);
		float line1_t = clippingFunc( component2, pos2w, component1, pos1w);
		clipped_triangles[0].v[0] = *vsOutput1;
		clippedLineFromPlane(&clipped_triangles[0].v[1], line1_t, vsOutput2, vsOutput1);
		clippedLineFromPlane(&clipped_triangles[0].v[2], line0_t, vsOutput0, vsOutput1);
		return 1;
	}
	else if (isVtx0outside && isVtx1outside && !isVtx2outside)
	{
		float line1_t = clippingFunc( component1, pos1w, component2, pos2w);
		float line2_t = clippingFunc( component0, pos0w, component2, pos2w);
		clipped_triangles[0].v[0] = *vsOutput2;
		clippedLineFromPlane(&clipped_triangles[0].v[1], line2_t, vsOutput0, vsOutput2);
		clippedLineFromPlane(&clipped_triangles[0].v[2], line1_t, vsOutput1, vsOutput2);
		return 1;
	}
	else if (!isVtx0outside && !isVtx1outside && isVtx2outside)
	{
		// 1 vtx outside, split into 2 triangles
		float line1_t = clippingFunc( component2, pos2w, component1, pos1w);
		float line2_t = clippingFunc( component2, pos2w, component0, pos0w);

		struct mat41 clippedVertex0;
		clippedLineFromPlane(&clippedVertex0, line1_t, vsOutput2, vsOutput1);
		struct mat41 clippedVertex1;
		clippedLineFromPlane(&clippedVertex1, line2_t, vsOutput2, vsOutput0);

		clipped_triangles[0].v[0] = *vsOutput0;
		clipped_triangles[0].v[1] = *vsOutput1;
		clipped_triangles[0].v[2] = clippedVertex0;
		clipped_triangles[1].v[0] = *vsOutput0;
		clipped_triangles[1].v[1] = clippedVertex0;
		clipped_triangles[1].v[2] = clippedVertex1;
		return 2;
	}
	else if (!isVtx0outside && isVtx1outside && !isVtx2outside)
	{
		float line0_t = clippingFunc( component1, pos1w, component0, pos0w);
		float line1_t = clippingFunc( component1, pos1w, component2, pos2w);

		struct mat41 clippedVertex0;
		clippedLineFromPlane(&clippedVertex0, line0_t, vsOutput1, vsOutput0);
		struct mat41 clippedVertex1;
		clippedLineFromPlane(&clippedVertex1, line1_t, vsOutput1, vsOutput2);

		clipped_triangles[0].v[0] = *vsOutput2;
		clipped_triangles[0].v[1] = *vsOutput0;
		clipped_triangles[0].v[2] = clippedVertex0;
		clipped_triangles[1].v[0] = *vsOutput2;
		clipped_triangles[1].v[1] = clippedVertex0;
		clipped_triangles[1].v[2] = clippedVertex1;
		return 2;
	}
	else if (isVtx0outside && !isVtx1outside && !isVtx2outside)
	{
		float line0_t = clippingFunc( component0, pos0w, component1, pos1w);
		float line2_t = clippingFunc( component0, pos0w, component2, pos2w);

		struct mat41 clippedVertex0;
		clippedLineFromPlane(&clippedVertex0, line2_t, vsOutput0, vsOutput2);
		struct mat41 clippedVertex1;
		clippedLineFromPlane(&clippedVertex1, line0_t, vsOutput0, vsOutput1);

		clipped_triangles[0].v[0] = *vsOutput1;
		clipped_triangles[0].v[1] = *vsOutput2;
		clipped_triangles[0].v[2] = clippedVertex0;
		clipped_triangles[1].v[0] = *vsOutput1;
		clipped_triangles[1].v[1] = clippedVertex0;
		clipped_triangles[1].v[2] = clippedVertex1;
		return 2;
	}
	else
	{
		// nothing get clipped
		clipped_triangles[0].v[0] = *vsOutput0;
		clipped_triangles[0].v[1] = *vsOutput1;
		clipped_triangles[0].v[2] = *vsOutput2;
		return 1;
	}
}

static int clipTriangle(struct clip_triangle* triangles, struct mat41* vsOutput0, struct mat41* vsOutput1, struct mat41* vsOutput2)
{
	func_clipLine clippingFunc[] = {clipLine_pos, clipLine_neg, clipLine_pos, clipLine_neg, clipLine_pos, clipLine_neg};
	func_getComp getComponentFunc[] = {getX, getX, getY, getY, getZ, getZ};
	func_isOutside isOutsideFunc[] = {isOutside_pos, isOutside_neg, isOutside_pos, isOutside_neg, isOutside_pos, isOutside_neg};

	int ntriangles = 0;

	ntriangles = 1;
	triangles[0].v[0] = *vsOutput0;
	triangles[0].v[1] = *vsOutput1;
	triangles[0].v[2] = *vsOutput2;

	int plane;
	for(plane=0; plane<6; plane++)
	{
		int nremainingTriangles=0;
		struct clip_triangle remainingTriangles[12];

		int i;
		for(i=0; i<ntriangles; i++)
		{
			struct mat41* vtx0 = &triangles[i].v[0];
			struct mat41* vtx1 = &triangles[i].v[1];
			struct mat41* vtx2 = &triangles[i].v[2];

			float component0 = getComponentFunc[plane](vtx0);
			float component1 = getComponentFunc[plane](vtx1);
			float component2 = getComponentFunc[plane](vtx2);

			int isOutside0 = isOutsideFunc[plane](component0, vtx0->m[3]);
			int isOutside1 = isOutsideFunc[plane](component1, vtx1->m[3]);
			int isOutside2 = isOutsideFunc[plane](component2, vtx2->m[3]);

			nremainingTriangles += clipTriangleToPlane(
				&remainingTriangles[nremainingTriangles],
				clippingFunc[plane], vtx0, vtx1, vtx2, isOutside0 , isOutside1, isOutside2,
				component0, component1, component2, vtx0->m[3], vtx1->m[3], vtx2->m[3]);
		}
		ntriangles = nremainingTriangles;
		memcpy(triangles, &remainingTriangles[0], sizeof(struct clip_triangle) * ntriangles);
	}
	return ntriangles;
}

