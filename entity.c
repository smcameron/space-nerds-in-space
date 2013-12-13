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

struct entity {
	struct mesh *m;
	float x, y, z; /* world coords */
	float sx, sy; /* screen coords */
	float scale;
	float dist3dsqrd;
	int color;
	int shadecolor;
	int render_style;
	void *user_data;
	union quat orientation;
	unsigned char onscreen;
	entity_fragment_shader_fn fragment_shader;
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
	struct mat44d camera_vp_matrix;
	float frustum[6][4];
};

struct screen_triangle {
	float x1;
	float y1;
	float x2;
	float y2;
	float x3;
	float y3;
	int clipped;
	struct triangle *src;
};

struct tri_depth_entry {
	int tri_index;
	float depth;
};

struct entity_context {
	int maxobjs;
	struct snis_object_pool *entity_pool;
	struct entity *entity_list; /* array, [maxobjs] */
	int nentity_depth;
	int *entity_depth; /* array [maxobjs] */
	struct camera_info camera;
	int nscreen_triangle_list;
	struct screen_triangle screen_triangle_list[MAX_TRIANGLES_PER_ENTITY];
	int ntri_depth;
	struct tri_depth_entry tri_depth[MAX_TRIANGLES_PER_ENTITY];
	struct vertex *fake_star;
	int nfakestars; /* = 0; */
	float fakestars_radius;
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
static int clip_triangle(struct clip_triangle* triangles, struct mat41* vs_out0, struct mat41* vs_out1, struct mat41* vs_out2);
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

static int is_backface(float x1, float y1, float x2, float y2, float x3, float y3)
{
	int twicearea;

	twicearea =	(x1 * y2 - x2 * y1) +
			(x2 * y3 - x3 * y2) +
			(x3 * y1 - x1 * y3);
	return twicearea > 0;
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
	sng_current_draw_line(w->window, gc, x1, y1, x1 + 1, y1);
}

void software_render_point(GtkWidget *w, GdkGC *gc,
		struct entity_context *cx, struct vertex *v)
{
	float wx = v->wx / v->ww;
	float wy = v->wy / v->ww;

	float x1, y1;

	x1 = wx_screen(cx, wx);
	y1 = wy_screen(cx, wy);
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

void software_render_entity_point_cloud(GtkWidget *w, GdkGC *gc, struct entity_context *cx,
				struct entity *e)
{
	int i;

	sng_set_foreground(e->color);
	if (e->render_style & RENDER_SPARKLE) {
		for (i = 0; i < e->m->nvertices; i++)
			if (!e->m->v[i].clip && snis_randn(100) < 35)
				software_render_point(w, gc, cx, &e->m->v[i]);
	} else {
		for (i = 0; i < e->m->nvertices; i++)
			if (!e->m->v[i].clip)
				software_render_point(w, gc, cx, &e->m->v[i]);
	}
}

void software_render_entity_lines(GtkWidget *w, GdkGC *gc, struct entity_context *cx,
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
			struct vertex v1;

			while (vcurr <= vend) {

				if (vcurr != vstart) {
					struct vertex v2 = *vcurr;

					int clipped = 0;
					if (v1.clip || v2.clip) {
						clipped = clip_line((struct mat41*)&v1.wx, (struct mat41*)&v2.wx);
					}

					if (!clipped) {
						x1 = v1.wx / v1.ww;
						y1 = v1.wy / v1.ww;
						float z1 = v1.wz / v1.ww;
						x2 = v2.wx / v2.ww;
						y2 = v2.wy / v2.ww;
						float z2 = v2.wz / v2.ww;

						int frag_color = e->color;
						if (e->fragment_shader) {
							e->fragment_shader((x1+x2)/2.0, (y1+y2)/2.0, (z1+z2)/2.0, frag_color, &frag_color);
						}
						sng_set_foreground(frag_color);

						float wx1 = wx_screen(cx, x1);
						float wy1 = wy_screen(cx, y1);
						float wx2 = wx_screen(cx, x2);
						float wy2 = wy_screen(cx, y2);
						if (e->m->l[i].flag & MESH_LINE_DOTTED)
							sng_draw_dotted_line(w->window, gc, wx1, wy1, wx2, wy2);
						else
							sng_current_draw_line(w->window, gc, wx1, wy1, wx2, wy2);
					}
				}

				v1 = *vcurr;
				++vcurr;
			}
		} else {
			int clipped = 0;
			if (vstart->clip || vend->clip) {
				clipped = clip_line((struct mat41*)&vstart->wx, (struct mat41*)&vend->wx);
			}

			if (!clipped) {
				x1 = wx_screen(cx, vstart->wx / vstart->ww);
				y1 = wy_screen(cx, vstart->wy / vstart->ww);
				x2 = wx_screen(cx, vend->wx / vend->ww);
				y2 = wy_screen(cx, vend->wy / vend->ww);
				if (e->m->l[i].flag & MESH_LINE_DOTTED)
					sng_draw_dotted_line(w->window, gc, x1, y1, x2, y2);
				else
					sng_current_draw_line(w->window, gc, x1, y1, x2, y2);
			}
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

static void sort_triangle_distances(struct entity_context *cx)
{
	/* Sort the triangles */
	qsort(cx->tri_depth, cx->ntri_depth, sizeof(cx->tri_depth[0]), tri_depth_compare);
}

static void draw_outline_tri(GtkWidget *w, GdkGC *gc, struct entity_context *cx,
				struct entity *e, struct screen_triangle *st)
{
	if (!st->clipped) {
		if (e->render_style & RENDER_BRIGHT_LINE) {
			if (!(st->src->flag & TRIANGLE_0_1_COPLANAR))
				sng_current_draw_bright_line(w->window, gc, st->x1, st->y1, st->x2, st->y2, e->color);
			if (!(st->src->flag & TRIANGLE_1_2_COPLANAR))
				sng_current_draw_bright_line(w->window, gc, st->x2, st->y2, st->x3, st->y3, e->color);
			if (!(st->src->flag & TRIANGLE_0_2_COPLANAR))
				sng_current_draw_bright_line(w->window, gc, st->x3, st->y3, st->x1, st->y1, e->color);
		} else {
			sng_set_foreground(e->color);
			sng_draw_tri_outline(w->window, gc,
				!(st->src->flag & TRIANGLE_0_1_COPLANAR), st->x1, st->y1,
				!(st->src->flag & TRIANGLE_1_2_COPLANAR), st->x2, st->y2,
				!(st->src->flag & TRIANGLE_0_2_COPLANAR), st->x3, st->y3);
		}
	} else {
		/* all the triangle edges to test for clipping */
		int e1_clipped = 0;
		struct vertex *e1_v1 = st->src->v[0];
		struct vertex *e1_v2 = st->src->v[1];
		int e2_clipped = 0;
		struct vertex *e2_v1 = st->src->v[1];
		struct vertex *e2_v2 = st->src->v[2];
		int e3_clipped = 0;
		struct vertex *e3_v1 = st->src->v[0];
		struct vertex *e3_v2 = st->src->v[2];

		/* test to see if any edges need to be clipped */
		struct vertex ce1_v1, ce1_v2, ce2_v1, ce2_v2, ce3_v1, ce3_v2;
		if (e1_v1->clip || e1_v2->clip) {
			ce1_v1 = *e1_v1;
			ce1_v2 = *e1_v2;
			e1_clipped = clip_line((struct mat41 *) &ce1_v1.wx, (struct mat41 *) &ce1_v2.wx);
			e1_v1 = &ce1_v1;
			e1_v2 = &ce1_v2;
		}
		if (e2_v1->clip || e2_v2->clip) {
			ce2_v1 = *e2_v1;
			ce2_v2 = *e2_v2;
			e2_clipped = clip_line((struct mat41 *) &ce2_v1.wx, (struct mat41 *) &ce2_v2.wx);
			e2_v1 = &ce2_v1;
			e2_v2 = &ce2_v2;
		}
		if (e3_v1->clip || e3_v2->clip) {
			ce3_v1 = *e3_v1;
			ce3_v2 = *e3_v2;
			e3_clipped = clip_line((struct mat41 *) &ce3_v1.wx, (struct mat41 *) &ce3_v2.wx);
			e3_v1 = &ce3_v1;
			e3_v2 = &ce3_v2;
		}

		float e1_x1 = wx_screen(cx, e1_v1->wx / e1_v1->ww);
		float e1_y1 = wy_screen(cx, e1_v1->wy / e1_v1->ww);
		float e1_x2 = wx_screen(cx, e1_v2->wx / e1_v2->ww);
		float e1_y2 = wy_screen(cx, e1_v2->wy / e1_v2->ww);

		float e2_x1 = wx_screen(cx, e2_v1->wx / e2_v1->ww);
		float e2_y1 = wy_screen(cx, e2_v1->wy / e2_v1->ww);
		float e2_x2 = wx_screen(cx, e2_v2->wx / e2_v2->ww);
		float e2_y2 = wy_screen(cx, e2_v2->wy / e2_v2->ww);

		float e3_x1 = wx_screen(cx, e3_v1->wx / e3_v1->ww);
		float e3_y1 = wy_screen(cx, e3_v1->wy / e3_v1->ww);
		float e3_x2 = wx_screen(cx, e3_v2->wx / e3_v2->ww);
		float e3_y2 = wy_screen(cx, e3_v2->wy / e3_v2->ww);

		if (e->render_style & RENDER_BRIGHT_LINE) {
			if (!e1_clipped && !(st->src->flag & TRIANGLE_0_1_COPLANAR))
				sng_current_draw_bright_line(w->window, gc, e1_x1, e1_y1, e1_x2, e1_y2, e->color);
			if (!e2_clipped && !(st->src->flag & TRIANGLE_1_2_COPLANAR))
				sng_current_draw_bright_line(w->window, gc, e2_x1, e2_y1, e2_x2, e2_y2, e->color);
			if (!e3_clipped && !(st->src->flag & TRIANGLE_0_2_COPLANAR))
				sng_current_draw_bright_line(w->window, gc, e3_x1, e3_y1, e3_x2, e3_y2, e->color);
		} else {
			sng_set_foreground(e->color);
			if (!e1_clipped && !(st->src->flag & TRIANGLE_0_1_COPLANAR))
				sng_current_draw_line(w->window, gc, e1_x1, e1_y1, e1_x2, e1_y2);
			if (!e2_clipped && !(st->src->flag & TRIANGLE_1_2_COPLANAR))
				sng_current_draw_line(w->window, gc, e2_x1, e2_y1, e2_x2, e2_y2);
			if (!e3_clipped && !(st->src->flag & TRIANGLE_0_2_COPLANAR))
				sng_current_draw_line(w->window, gc, e3_x1, e3_y1, e3_x2, e3_y2);
		}
	}
}

void software_render_entity_triangles(GtkWidget *w, GdkGC *gc, struct entity_context *cx, struct entity *e)
{
	int i, j;

	struct camera_info *c = &cx->camera;

	int filled_triangle = ((c->renderer & FLATSHADING_RENDERER) || (c->renderer & BLACK_TRIS))
				&& !(e->render_style & RENDER_NO_FILL);
	int outline_triangle = (c->renderer & WIREFRAME_RENDERER) || (e->render_style & RENDER_WIREFRAME);

	cx->ntri_depth = 0;
	cx->nscreen_triangle_list = 0;

	for (i = 0; i < e->m->ntriangles; i++) {
		struct triangle *t = &e->m->t[i];

		/* whole triangle is outside the same clip plane */
		if (t->v[0]->clip & t->v[1]->clip & t->v[2]->clip)
			continue;

		int nclip_gen;
		struct clip_triangle clip_gen[64];

		int is_clipped = 0;
		struct mat41* mv0 = (struct mat41 *) &t->v[0]->wx;
		struct mat41* mv1 = (struct mat41 *) &t->v[1]->wx;
		struct mat41* mv2 = (struct mat41 *) &t->v[2]->wx;

		if (t->v[0]->clip || t->v[1]->clip || t->v[2]->clip) {
			/* part of triangle is clipped */
			is_clipped = 1;
			nclip_gen = clip_triangle(&clip_gen[0], mv0, mv1, mv2);
		} else {
			/* not clipped so just draw it */
			nclip_gen = 1;
			clip_gen[0].v[0] = *mv0;
			clip_gen[0].v[1] = *mv1;
			clip_gen[0].v[2] = *mv2;
		}

		for (j=0; j<nclip_gen; j++) {
			/* do the w divide now that the triangles are clipped */
			float x1 = wx_screen(cx, clip_gen[j].v[0].m[0] / clip_gen[j].v[0].m[3]);
			float y1 = wy_screen(cx, clip_gen[j].v[0].m[1] / clip_gen[j].v[0].m[3]);

			float x2 = wx_screen(cx, clip_gen[j].v[1].m[0] / clip_gen[j].v[1].m[3]);
			float y2 = wy_screen(cx, clip_gen[j].v[1].m[1] / clip_gen[j].v[1].m[3]);

			float x3 = wx_screen(cx, clip_gen[j].v[2].m[0] / clip_gen[j].v[2].m[3]);
			float y3 = wy_screen(cx, clip_gen[j].v[2].m[1] / clip_gen[j].v[2].m[3]);

			if (is_backface(x1, y1, x2, y2, x3, y3))
				continue;

			if (cx->nscreen_triangle_list > MAX_TRIANGLES_PER_ENTITY) {
				printf("Too many triangles, clipping rendering: max %d at %s:%d\n",
					MAX_TRIANGLES_PER_ENTITY, __FILE__, __LINE__);
				continue;
			}

			/* record this triangle to be rendered */
			struct screen_triangle *st = &cx->screen_triangle_list[cx->nscreen_triangle_list];
			st->x1 = x1;
			st->y1 = y1;
			st->x2 = x2;
			st->y2 = y2;
			st->x3 = x3;
			st->y3 = y3;
			st->clipped = is_clipped;
			st->src = t;

			/* setup triangle depth sorting */
			float wz1 = clip_gen[j].v[0].m[2] / clip_gen[j].v[0].m[3];
			float wz2 = clip_gen[j].v[1].m[2] / clip_gen[j].v[1].m[3];
			float wz3 = clip_gen[j].v[2].m[2] / clip_gen[j].v[2].m[3];

			cx->tri_depth[cx->ntri_depth].tri_index = cx->nscreen_triangle_list;
			cx->tri_depth[cx->ntri_depth].depth = (wz1 + wz2 + wz3) / 3.0;

			cx->nscreen_triangle_list++;
			cx->ntri_depth++;

			/* if the triangles are not filled then we only need one valid one from the clip */
			if (!filled_triangle)
				break;
		}
	}

	/* filled triangles need tobe sorted back to front */
	if (filled_triangle)
		sort_triangle_distances(cx);

	for (i = 0; i < cx->ntri_depth; i++) {
		struct screen_triangle *st = &cx->screen_triangle_list[cx->tri_depth[i].tri_index];

		if (filled_triangle) {
			int fill_color;
			if (cx->camera.renderer & BLACK_TRIS) {
				fill_color = BLACK;
			} else {
				float cos_theta;
				struct mat41 normal;
				normal = *(struct mat41 *) &st->src->n.wx;
				normalize_vector(&normal, &normal);
				cos_theta = mat41_dot_mat41(&cx->light, &normal);
				cos_theta = (cos_theta + 1.0) / 2.0;
				fill_color = (int) fmod((cos_theta * 240.0), 240.0) +
						GRAY + (NSHADESOFGRAY * e->shadecolor) + 10;
			}

			sng_set_foreground(fill_color);
			sng_draw_tri(w->window, gc, 1, st->x1, st->y1, st->x2, st->y2, st->x3, st->y3);
		}

		if (outline_triangle) {
			draw_outline_tri(w, gc, cx, e, st);
		}
	}
}

static void calculate_model_matricies(float x, float y, float z, union quat* orientation, float scale,
					struct mat44d *mat_r, struct mat44d *mat_model)
{
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

	quat_to_rh_rot_matrix_fd(orientation, &mat_r->m[0][0]);

	struct mat44d mat_s = {{
		{ scale, 0, 0, 0 },
		{ 0, scale, 0, 0 },
		{ 0, 0, scale, 0 },
		{ 0, 0, 0, 1 }}};

	struct mat44d mat_t_r;
	mat44_product_ddd(&mat_t, mat_r, &mat_t_r);

	mat44_product_ddd(&mat_t_r, &mat_s, mat_model);
}

int transform_vertices(struct entity_context *cx, struct mat44d* mat_model, struct vertex *v, int len)
{
	/* calculate the transform... */

	struct mat44 mat_mvp;

	if (mat_model) {
		mat44_product_ddf(&cx->camera.camera_vp_matrix, mat_model, &mat_mvp);
	} else {
		mat44_convert_df(&cx->camera.camera_vp_matrix, &mat_mvp);
	}

	int total_clip_flag = 0x3f;

	int i;
	for (i = 0; i < len; i++) {
		/* Set homogeneous coord to 1 */
		v[i].w = 1.0;

		/* Do the transformation... */
		struct mat41 *m1 = (struct mat41 *) &v[i].x;
		struct mat41 *m2 = (struct mat41 *) &v[i].wx;
		mat44_x_mat41(&mat_mvp, m1, m2);

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
	if (transform_vertices(cx, 0, &v[0], 2)) {
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
	if (transform_vertices(cx, 0, &v, 1)) {
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

static int transform_entity(struct entity_context *cx, struct entity *e)
{
	struct mat44d mat_rotation;
	struct mat44d mat_model;
	calculate_model_matricies(e->x, e->y, e->z, &e->orientation, e->scale, &mat_rotation, &mat_model);

	/* calculate screen coords of entity as a whole */
	transform_point(cx, e->x, e->y, e->z, &e->sx, &e->sy);

	/* Do the vertex transformation */
	transform_vertices(cx, &mat_model, &e->m->v[0], e->m->nvertices);

	/* rotate the normals */
	int i;
	for (i = 0; i < e->m->ntriangles; i++) {
		struct mat41 *m1 = (struct mat41 *) &e->m->t[i].n.x;
		struct mat41 *m2 = (struct mat41 *) &e->m->t[i].n.wx;
		mat44_x_mat41_dff(&mat_rotation, m1, m2);
	}
	return 0;
}

void render_entity(GtkWidget *w, GdkGC *gc, struct entity_context *cx, struct entity *e)
{
	transform_entity(cx, e);

	if (e->sx >=0 && e->sy >= 0)
		e->onscreen = 1;

	switch (e->m->geometry_mode) {
		case MESH_GEOMETRY_TRIANGLES:
			software_render_entity_triangles(w, gc, cx, e);
			break;
		case MESH_GEOMETRY_LINES:
			software_render_entity_lines(w, gc, cx, e);
			break;
		case MESH_GEOMETRY_POINTS:
			software_render_entity_point_cloud(w, gc, cx, e);
			break;
	}
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

   for( p = 0; p < 6; p++ )
      if( c->frustum[p][0] * x + c->frustum[p][1] * y + c->frustum[p][2] * z + c->frustum[p][3] <= -radius )
         return 0;
   return 1;
}

/* from http://www.crownandcutlass.com/features/technicaldetails/frustum.html
This page and its contents are Copyright 2000 by Mark Morley
Unless otherwise noted, you may use any and all code examples provided herein in any way you want. */

void extract_frustum_planes(struct camera_info *c)
{
	double *clip = &c->camera_vp_matrix.m[0][0];
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

	normalize_vector(&cx->light, &cx->light);

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
	cx->camera.look_direction = look_direction;
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
	struct mat44d view_matrix;
	mat44_product_ddd(&cameralook_transform, &cameratrans_transform, &view_matrix);

	/* make the view-perspective matrix, VP = P * V */
	mat44_product_ddd(&perspective_transform, &view_matrix, &cx->camera.camera_vp_matrix);

	/* pull out the frustum planes for entity culling */
	extract_frustum_planes(&cx->camera);
}

static void reposition_fake_star(struct entity_context *cx, struct vertex *fs, float radius);

void render_entities(GtkWidget *w, GdkGC *gc, struct entity_context *cx)
{
	int i, j, n;

	calculate_camera_transform(cx);

	struct camera_info *c = &cx->camera;

#ifdef WITH_ILDA_SUPPORT
	ilda_file_open(cx);
	ilda_file_newframe(cx);
#endif

	/* draw fake stars */
	sng_set_foreground(WHITE);
	transform_vertices(cx, 0, &cx->fake_star[0], cx->nfakestars);

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

	/* render the sorted entities */
	for (j = 0; j < cx->nentity_depth; j++) {
		struct entity *e = &cx->entity_list[cx->entity_depth[j]];

		render_entity(w, gc, cx, e);
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
	set_lighting(cx, 0.0, 1.0, 1.0);
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

struct mat44d get_camera_transform(struct entity_context *cx)
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

static int clip_triangle_to_plane(struct clip_triangle* clipped_triangles,
	clip_line_fn clipping_func,
	struct mat41* vs_out0, struct mat41* vs_out1, struct mat41* vs_out2,
	int vtx_0_outside, int vtx_1_outside, int vtx_2_outside,
	float component0, float component1, float component2,
	float pos0w, float pos1w, float pos2w)
{
	if (vtx_0_outside && vtx_1_outside && vtx_2_outside)
		return 0; /* whole triangle gets clipped */

	if (!vtx_0_outside && vtx_1_outside && vtx_2_outside) {
		/* 2 vtx outside, split into 1 smaller triangles */
		float line0_t = clipping_func(component1, pos1w, component0, pos0w);
		float line2_t = clipping_func(component2, pos2w, component0, pos0w);
		clipped_triangles[0].v[0] = *vs_out0;
		clipped_line_from_plane(&clipped_triangles[0].v[1], line0_t, vs_out1, vs_out0);
		clipped_line_from_plane(&clipped_triangles[0].v[2], line2_t, vs_out2, vs_out0);
		return 1;
	}

	if (vtx_0_outside && !vtx_1_outside && vtx_2_outside) {
		float line0_t = clipping_func( component0, pos0w, component1, pos1w);
		float line1_t = clipping_func( component2, pos2w, component1, pos1w);
		clipped_triangles[0].v[0] = *vs_out1;
		clipped_line_from_plane(&clipped_triangles[0].v[1], line1_t, vs_out2, vs_out1);
		clipped_line_from_plane(&clipped_triangles[0].v[2], line0_t, vs_out0, vs_out1);
		return 1;
	}

	if (vtx_0_outside && vtx_1_outside && !vtx_2_outside) {
		float line1_t = clipping_func( component1, pos1w, component2, pos2w);
		float line2_t = clipping_func( component0, pos0w, component2, pos2w);
		clipped_triangles[0].v[0] = *vs_out2;
		clipped_line_from_plane(&clipped_triangles[0].v[1], line2_t, vs_out0, vs_out2);
		clipped_line_from_plane(&clipped_triangles[0].v[2], line1_t, vs_out1, vs_out2);
		return 1;
	}

	if (!vtx_0_outside && !vtx_1_outside && vtx_2_outside) {
		/* 1 vtx outside, split into 2 triangles */
		float line1_t = clipping_func( component2, pos2w, component1, pos1w);
		float line2_t = clipping_func( component2, pos2w, component0, pos0w);

		struct mat41 clipped_vertex0;
		clipped_line_from_plane(&clipped_vertex0, line1_t, vs_out2, vs_out1);
		struct mat41 clipped_vertex1;
		clipped_line_from_plane(&clipped_vertex1, line2_t, vs_out2, vs_out0);

		clipped_triangles[0].v[0] = *vs_out0;
		clipped_triangles[0].v[1] = *vs_out1;
		clipped_triangles[0].v[2] = clipped_vertex0;
		clipped_triangles[1].v[0] = *vs_out0;
		clipped_triangles[1].v[1] = clipped_vertex0;
		clipped_triangles[1].v[2] = clipped_vertex1;
		return 2;
	}

	if (!vtx_0_outside && vtx_1_outside && !vtx_2_outside) {
		float line0_t = clipping_func( component1, pos1w, component0, pos0w);
		float line1_t = clipping_func( component1, pos1w, component2, pos2w);

		struct mat41 clipped_vertex0;
		clipped_line_from_plane(&clipped_vertex0, line0_t, vs_out1, vs_out0);
		struct mat41 clipped_vertex1;
		clipped_line_from_plane(&clipped_vertex1, line1_t, vs_out1, vs_out2);

		clipped_triangles[0].v[0] = *vs_out2;
		clipped_triangles[0].v[1] = *vs_out0;
		clipped_triangles[0].v[2] = clipped_vertex0;
		clipped_triangles[1].v[0] = *vs_out2;
		clipped_triangles[1].v[1] = clipped_vertex0;
		clipped_triangles[1].v[2] = clipped_vertex1;
		return 2;
	}

	if (vtx_0_outside && !vtx_1_outside && !vtx_2_outside) {
		float line0_t = clipping_func( component0, pos0w, component1, pos1w);
		float line2_t = clipping_func( component0, pos0w, component2, pos2w);

		struct mat41 clipped_vertex0;
		clipped_line_from_plane(&clipped_vertex0, line2_t, vs_out0, vs_out2);
		struct mat41 clipped_vertex1;
		clipped_line_from_plane(&clipped_vertex1, line0_t, vs_out0, vs_out1);

		clipped_triangles[0].v[0] = *vs_out1;
		clipped_triangles[0].v[1] = *vs_out2;
		clipped_triangles[0].v[2] = clipped_vertex0;
		clipped_triangles[1].v[0] = *vs_out1;
		clipped_triangles[1].v[1] = clipped_vertex0;
		clipped_triangles[1].v[2] = clipped_vertex1;
		return 2;
	}

	/* nothing gets clipped */
	clipped_triangles[0].v[0] = *vs_out0;
	clipped_triangles[0].v[1] = *vs_out1;
	clipped_triangles[0].v[2] = *vs_out2;
	return 1;
}

static int clip_triangle(struct clip_triangle* triangles, struct mat41* vs_out0,
				struct mat41* vs_out1, struct mat41* vs_out2)
{
	clip_line_fn clipping_func[] = {clip_line_pos, clip_line_neg, clip_line_pos,
					clip_line_neg, clip_line_pos, clip_line_neg};
	get_comp_fn get_component_func[] = {get_x, get_x, get_y, get_y, get_z, get_z};
	is_outside_fn is_outside_func[] = {is_outside_pos, is_outside_neg, is_outside_pos,
						is_outside_neg, is_outside_pos, is_outside_neg};

	int ntriangles = 0;

	ntriangles = 1;
	triangles[0].v[0] = *vs_out0;
	triangles[0].v[1] = *vs_out1;
	triangles[0].v[2] = *vs_out2;

	int plane;
	for (plane = 0; plane < 6; plane++) {
		int nremaining_triangles = 0;
		struct clip_triangle remaining_triangles[64];

		int i;
		for(i = 0; i < ntriangles; i++) {
			struct mat41* vtx0 = &triangles[i].v[0];
			struct mat41* vtx1 = &triangles[i].v[1];
			struct mat41* vtx2 = &triangles[i].v[2];

			float component0 = get_component_func[plane](vtx0);
			float component1 = get_component_func[plane](vtx1);
			float component2 = get_component_func[plane](vtx2);

			int is_outside0 = is_outside_func[plane](component0, vtx0->m[3]);
			int is_outside1 = is_outside_func[plane](component1, vtx1->m[3]);
			int is_outside2 = is_outside_func[plane](component2, vtx2->m[3]);

			nremaining_triangles += clip_triangle_to_plane(
				&remaining_triangles[nremaining_triangles],
				clipping_func[plane], vtx0, vtx1, vtx2,
				is_outside0 , is_outside1, is_outside2,
				component0, component1, component2,
				vtx0->m[3], vtx1->m[3], vtx2->m[3]);
		}
		ntriangles = nremaining_triangles;
		memcpy(triangles, &remaining_triangles[0], sizeof(struct clip_triangle) * ntriangles);
	}
	return ntriangles;
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

