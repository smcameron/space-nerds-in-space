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
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "graph_dev.h"
#include "vertex.h"
#include "triangle.h"
#include "mtwist.h"
#include "mathutils.h"
#include "matrix.h"
#include "quat.h"
#include "mesh.h"
#include "snis_typeface.h"
#include "snis_graph.h"
#include "material.h"
#include "entity.h"
#include "entity_private.h"

typedef void (*entity_fragment_shader_fn)(float x, float y, float z, int cin, int *cout);

#define MAX_TRIANGLES_PER_ENTITY 10000

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

static struct graph_dev_gdk_context {
	GdkDrawable *drawable;
	GdkGC *gc;

	int nscreen_triangle_list;
	struct screen_triangle screen_triangle_list[MAX_TRIANGLES_PER_ENTITY];
	int ntri_depth;
	struct tri_depth_entry tri_depth[MAX_TRIANGLES_PER_ENTITY];

	int screen_width, screen_height;
	float x_scale, y_scale; /* scale from extent to screen */

	float x_offset_3d, y_offset_3d;
	float width_3d, height_3d;
	float half_width_3d, half_height_3d;
} sgc;

__attribute__((unused)) int graph_dev_planet_specularity = 0;
__attribute__((unused)) int graph_dev_atmosphere_ring_shadows = 0;

/* nothing in here yet
struct mesh_gtk_info {
};
*/

void mesh_graph_dev_init(struct mesh *m)
{
/* nothing in here yet
	struct mesh_gtk_info *ptr = malloc(sizeof(struct mesh_gtk_info));

	m->graph_ptr = ptr;
*/
}

void mesh_graph_dev_cleanup(struct mesh *m)
{
/* nothing in here yet
	if (m->graph_ptr) {
		struct mesh_gtk_info *ptr = m->graph_ptr;

		free(ptr);
	}
*/
}

int graph_dev_setup(__attribute__((unused)) const char *shader_dir)
{
	return 0;
}

void graph_dev_start_frame()
{

}

void graph_dev_end_frame()
{

}

void graph_dev_clear_depth_bit()
{
	/* noop as we have no z-depth buffer */
}

void graph_dev_set_context(void *gdk_drawable, void *gdk_gc)
{
	sgc.drawable = gdk_drawable;
	sgc.gc = gdk_gc;
}

void graph_dev_setup_colors(void *gtk_widget_w, void *gtk_color_huex, int nhuex)
{
	GtkWidget *w = gtk_widget_w;
	GdkColor *huex = gtk_color_huex;

	int i;
	for (i = 0; i < nhuex; i++)
		gdk_colormap_alloc_color(gtk_widget_get_colormap(w),
				&huex[i], FALSE, FALSE);
	gtk_widget_modify_bg(w, GTK_STATE_NORMAL, &huex[BLACK]);
}

void graph_dev_set_color(void *gtk_color, float a)
{
	gdk_gc_set_foreground(sgc.gc, gtk_color);
}

void graph_dev_set_screen_size(int width, int height)
{
	sgc.screen_width = width;
	sgc.screen_height = height;
}

void graph_dev_set_extent_scale(float x_scale, float y_scale)
{
	sgc.x_scale = x_scale;
	sgc.y_scale = y_scale;
}

/* where to draw 3d in screen pixels */
void graph_dev_set_3d_viewport(int x_offset, int y_offset, int width, int height)
{
	sgc.x_offset_3d = x_offset;
	sgc.y_offset_3d = y_offset;
	sgc.width_3d = width;
	sgc.half_width_3d = width/2.0;
	sgc.height_3d = height;
	sgc.half_height_3d = height/2.0;
}

void graph_dev_draw_line(float x1, float y1, float x2, float y2)
{
	gdk_draw_line(sgc.drawable, sgc.gc, x1, y1, x2, y2);
}

void graph_dev_draw_rectangle(int filled, float x, float y, float width, float height)
{
	gdk_draw_rectangle(sgc.drawable, sgc.gc, (gboolean) filled, x, y, width, height);
}

void graph_dev_draw_point(float x, float y)
{
	gdk_draw_point(sgc.drawable, sgc.gc, x, y);
}

void graph_dev_draw_arc(int filled, float x, float y, float width, float height, float angle1, float angle2)
{
	gdk_draw_arc(sgc.drawable, sgc.gc, (gboolean) filled, x, y, width, height,
			angle1*64.0*180.0/M_PI, (angle2-angle1)*64.0*180.0/M_PI);
}

static void graph_dev_draw_triangle_outline(int draw12, float x1, float y1, int draw23,
					float x2, float y2, int draw31, float x3, float y3)
{
	/* faster than gdk_draw_segments or non-filled gdk_draw_polygon */
	if (draw12)
		gdk_draw_line(sgc.drawable, sgc.gc, x1, y1, x2, y2);
	if (draw23)
		gdk_draw_line(sgc.drawable, sgc.gc, x2, y2, x3, y3);
	if (draw31)
		gdk_draw_line(sgc.drawable, sgc.gc, x3, y3, x1, y1);
}

static void graph_dev_draw_triangle(int filled, float x1, float y1, float x2, float y2, float x3, float y3)
{
	GdkPoint tri[3] = { {x1, y1}, {x2, y2}, {x3, y3} };

	gdk_draw_polygon(sgc.drawable, sgc.gc, filled, tri, 3);
}


struct clip_triangle {
	struct mat41 v[3]; /* three vertices */
};
static int clip_triangle(struct clip_triangle *triangles, struct mat41 *vs_out0, struct mat41 *vs_out1,
				struct mat41 *vs_out2);
static int clip_line(struct mat41 *vtx0, struct mat41 *vtx1);

/* convert rendering coords into screen pixels */
static inline float wx_screen(float wx)
{
	return wx * sgc.half_width_3d + sgc.half_width_3d + sgc.x_offset_3d;
}

/* convert rendering coords into screen pixels */
static inline float wy_screen(float wy)
{
	return -wy * sgc.half_height_3d + sgc.half_height_3d + sgc.y_offset_3d;
}

/* convert screen pixels to extent coords */
static inline float sx_extent(float sx)
{
	return sx / sgc.x_scale;
}

/* convert screen pixels to extent coords */
static inline float sy_extent(float sy)
{
	return sy / sgc.y_scale;
}

static int is_backface(float x1, float y1, float x2, float y2, float x3, float y3)
{
	int twicearea;

	twicearea =	(x1 * y2 - x2 * y1) +
			(x2 * y3 - x3 * y2) +
			(x3 * y1 - x1 * y3);
	return twicearea > 0;
}

static void software_render_point(struct vertex *v)
{
	float wx = v->wx / v->ww;
	float wy = v->wy / v->ww;

	float x1, y1;

	x1 = wx_screen(wx);
	y1 = wy_screen(wy);
	graph_dev_draw_point(x1, y1);
}

void software_render_entity_point_cloud(struct entity_context *cx, struct entity *e)
{
	int i;

	sng_set_foreground(e->color);
	if (e->render_style & RENDER_SPARKLE) {
		for (i = 0; i < e->m->nvertices; i++)
			if (!e->m->v[i].clip && snis_randn(100) < 35)
				software_render_point(&e->m->v[i]);
	} else {
		for (i = 0; i < e->m->nvertices; i++)
			if (!e->m->v[i].clip)
				software_render_point(&e->m->v[i]);
	}
}

static void color_by_z_fragment_shader(float x, float y, float z, int cin, int *cout)
{
	/* shade color so it is darker father away from center and ligher closer */
	if (z > 0)
		*cout = cin - (int)(z*0.8 * NGRADIENT_SHADES);
	else
		*cout = cin - (int)(z * NGRADIENT_SHADES);
}

void software_render_entity_lines(struct entity_context *cx, struct entity *e)
{
	int i;
	float x1, y1;
	float x2, y2;

	entity_fragment_shader_fn fragment_shader = 0;
	if (e->material_ptr && e->material_ptr->type == MATERIAL_COLOR_BY_W) {
		/* this is the original method as I am not going to update it to the same logic as the opengl shader */
		fragment_shader = color_by_z_fragment_shader;
	}

	sng_set_foreground(e->color);
	for (i = 0; i < e->m->nlines; i++) {
		struct vertex *vstart = e->m->l[i].start;
		struct vertex *vend = e->m->l[i].end;

		if (e->m->l[i].flag & MESH_LINE_STRIP) {
			struct vertex *vcurr = vstart;
			struct vertex v1;

			while (vcurr <= vend) {

				if (vcurr != vstart) {
					struct vertex v2 = *vcurr;

					int clipped = 0;
					if (v1.clip || v2.clip)
						clipped = clip_line((struct mat41 *)&v1.wx, (struct mat41 *)&v2.wx);

					if (!clipped) {
						x1 = v1.wx / v1.ww;
						y1 = v1.wy / v1.ww;
						float z1 = v1.wz / v1.ww;
						x2 = v2.wx / v2.ww;
						y2 = v2.wy / v2.ww;
						float z2 = v2.wz / v2.ww;

						int frag_color = e->color;
						if (fragment_shader) {
							fragment_shader((x1 + x2) / 2.0, (y1 + y2) / 2.0,
								(z1 + z2) / 2.0, frag_color, &frag_color);
						}
						sng_set_foreground(frag_color);

						float wx1 = wx_screen(x1);
						float wy1 = wy_screen(y1);
						float wx2 = wx_screen(x2);
						float wy2 = wy_screen(y2);
						if (e->m->l[i].flag & MESH_LINE_DOTTED)
							sng_draw_dotted_line(sx_extent(wx1),
								sy_extent(wy1), sx_extent(wx2), sy_extent(wy2));
						else
							graph_dev_draw_line(wx1, wy1, wx2, wy2);
					}
				}

				v1 = *vcurr;
				++vcurr;
			}
		} else {
			int clipped = 0;
			if (vstart->clip || vend->clip)
				clipped = clip_line((struct mat41 *)&vstart->wx, (struct mat41 *)&vend->wx);

			if (!clipped) {
				x1 = wx_screen(vstart->wx / vstart->ww);
				y1 = wy_screen(vstart->wy / vstart->ww);
				x2 = wx_screen(vend->wx / vend->ww);
				y2 = wy_screen(vend->wy / vend->ww);
				if (e->m->l[i].flag & MESH_LINE_DOTTED)
					sng_draw_dotted_line(sx_extent(x1), sy_extent(y1),
								sx_extent(x2), sy_extent(y2));
				else
					graph_dev_draw_line(x1, y1, x2, y2);
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

static void sort_triangle_distances()
{
	/* Sort the triangles */
	qsort(sgc.tri_depth, sgc.ntri_depth, sizeof(sgc.tri_depth[0]), tri_depth_compare);
}

static void draw_triangle_outline(struct entity_context *cx, struct entity *e, struct screen_triangle *st)
{
	if (!st->clipped) {
		if (e->render_style & RENDER_BRIGHT_LINE) {
			if (!(st->src->flag & TRIANGLE_0_1_COPLANAR))
				sng_current_draw_bright_line(sx_extent(st->x1),
					sy_extent(st->y1), sx_extent(st->x2), sy_extent(st->y2), e->color);
			if (!(st->src->flag & TRIANGLE_1_2_COPLANAR))
				sng_current_draw_bright_line(sx_extent(st->x2),
					sy_extent(st->y2), sx_extent(st->x3), sy_extent(st->y3), e->color);
			if (!(st->src->flag & TRIANGLE_0_2_COPLANAR))
				sng_current_draw_bright_line(sx_extent(st->x3),
					sy_extent(st->y3), sx_extent(st->x1), sy_extent(st->y1), e->color);
		} else {
			sng_set_foreground(e->color);
			graph_dev_draw_triangle_outline(
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

		float e1_x1 = wx_screen(e1_v1->wx / e1_v1->ww);
		float e1_y1 = wy_screen(e1_v1->wy / e1_v1->ww);
		float e1_x2 = wx_screen(e1_v2->wx / e1_v2->ww);
		float e1_y2 = wy_screen(e1_v2->wy / e1_v2->ww);

		float e2_x1 = wx_screen(e2_v1->wx / e2_v1->ww);
		float e2_y1 = wy_screen(e2_v1->wy / e2_v1->ww);
		float e2_x2 = wx_screen(e2_v2->wx / e2_v2->ww);
		float e2_y2 = wy_screen(e2_v2->wy / e2_v2->ww);

		float e3_x1 = wx_screen(e3_v1->wx / e3_v1->ww);
		float e3_y1 = wy_screen(e3_v1->wy / e3_v1->ww);
		float e3_x2 = wx_screen(e3_v2->wx / e3_v2->ww);
		float e3_y2 = wy_screen(e3_v2->wy / e3_v2->ww);

		if (e->render_style & RENDER_BRIGHT_LINE) {
			if (!e1_clipped && !(st->src->flag & TRIANGLE_0_1_COPLANAR))
				sng_current_draw_bright_line(sx_extent(e1_x1),
					sy_extent(e1_y1), sx_extent(e1_x2), sy_extent(e1_y2), e->color);
			if (!e2_clipped && !(st->src->flag & TRIANGLE_1_2_COPLANAR))
				sng_current_draw_bright_line(sx_extent(e2_x1),
					sy_extent(e2_y1), sx_extent(e2_x2), sy_extent(e2_y2), e->color);
			if (!e3_clipped && !(st->src->flag & TRIANGLE_0_2_COPLANAR))
				sng_current_draw_bright_line(sx_extent(e3_x1),
					sy_extent(e3_y1), sx_extent(e3_x2), sy_extent(e3_y2), e->color);
		} else {
			sng_set_foreground(e->color);
			if (!e1_clipped && !(st->src->flag & TRIANGLE_0_1_COPLANAR))
				graph_dev_draw_line(e1_x1, e1_y1, e1_x2, e1_y2);
			if (!e2_clipped && !(st->src->flag & TRIANGLE_1_2_COPLANAR))
				graph_dev_draw_line(e2_x1, e2_y1, e2_x2, e2_y2);
			if (!e3_clipped && !(st->src->flag & TRIANGLE_0_2_COPLANAR))
				graph_dev_draw_line(e3_x1, e3_y1, e3_x2, e3_y2);
		}
	}
}

void software_render_entity_triangles(struct entity_context *cx, struct entity *e, union vec3 *light_vector)
{
	int i, j;

	struct camera_info *c = &cx->camera;

	int filled_triangle = ((c->renderer & FLATSHADING_RENDERER) || (c->renderer & BLACK_TRIS))
				&& !(e->render_style & RENDER_NO_FILL);
	int outline_triangle = (c->renderer & WIREFRAME_RENDERER) || (e->render_style & RENDER_WIREFRAME);

	sgc.ntri_depth = 0;
	sgc.nscreen_triangle_list = 0;

	for (i = 0; i < e->m->ntriangles; i++) {
		struct triangle *t = &e->m->t[i];

		/* whole triangle is outside the same clip plane */
		if (t->v[0]->clip & t->v[1]->clip & t->v[2]->clip)
			continue;

		int nclip_gen;
		struct clip_triangle clip_gen[64];

		int is_clipped = 0;
		struct mat41 *mv0 = (struct mat41 *) &t->v[0]->wx;
		struct mat41 *mv1 = (struct mat41 *) &t->v[1]->wx;
		struct mat41 *mv2 = (struct mat41 *) &t->v[2]->wx;

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

		for (j = 0; j < nclip_gen; j++) {
			/* do the w divide now that the triangles are clipped */
			float x1 = wx_screen(clip_gen[j].v[0].m[0] / clip_gen[j].v[0].m[3]);
			float y1 = wy_screen(clip_gen[j].v[0].m[1] / clip_gen[j].v[0].m[3]);

			float x2 = wx_screen(clip_gen[j].v[1].m[0] / clip_gen[j].v[1].m[3]);
			float y2 = wy_screen(clip_gen[j].v[1].m[1] / clip_gen[j].v[1].m[3]);

			float x3 = wx_screen(clip_gen[j].v[2].m[0] / clip_gen[j].v[2].m[3]);
			float y3 = wy_screen(clip_gen[j].v[2].m[1] / clip_gen[j].v[2].m[3]);

			if (is_backface(x1, y1, x2, y2, x3, y3))
				continue;

			if (sgc.nscreen_triangle_list > MAX_TRIANGLES_PER_ENTITY) {
				printf("Too many triangles, clipping rendering: max %d at %s:%d\n",
					MAX_TRIANGLES_PER_ENTITY, __FILE__, __LINE__);
				continue;
			}

			/* record this triangle to be rendered */
			struct screen_triangle *st = &sgc.screen_triangle_list[sgc.nscreen_triangle_list];
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

			sgc.tri_depth[sgc.ntri_depth].tri_index = sgc.nscreen_triangle_list;
			sgc.tri_depth[sgc.ntri_depth].depth = (wz1 + wz2 + wz3) / 3.0;

			sgc.nscreen_triangle_list++;
			sgc.ntri_depth++;

			/* if the triangles are not filled then we only need one valid one from the clip */
			if (!filled_triangle)
				break;
		}
	}

	/* filled triangles need tobe sorted back to front */
	if (filled_triangle)
		sort_triangle_distances();

	for (i = 0; i < sgc.ntri_depth; i++) {
		struct screen_triangle *st = &sgc.screen_triangle_list[sgc.tri_depth[i].tri_index];

		if (filled_triangle) {
			int fill_color;
			if (cx->camera.renderer & BLACK_TRIS) {
				fill_color = BLACK;
			} else {
				float cos_theta;

				union vec3 normal = *(union vec3 *) &st->src->n.wx;
				vec3_normalize_self(&normal);

				cos_theta = vec3_dot(&normal, light_vector);
				cos_theta = (cos_theta + 1.0) / 2.0;
				fill_color = (int) (cos_theta * 240.0f) +
						GRAY + (NSHADESOFGRAY * e->shadecolor) + 10;
			}

			sng_set_foreground(fill_color);
			graph_dev_draw_triangle(1, st->x1, st->y1, st->x2, st->y2, st->x3, st->y3);
		}

		if (outline_triangle)
			draw_triangle_outline(cx, e, st);
	}
}

static void transform_entity(const struct mat44 *mat_mvp, const struct mat44 *mat_mv,
				const struct mat33 *mat_normal, struct entity *e)
{
	/* Do the vertex transformation */
	transform_vertices(mat_mvp, &e->m->v[0], e->m->nvertices);

	/* rotate the normals */
	int i;
	for (i = 0; i < e->m->ntriangles; i++) {
		struct mat31 *m1 = (struct mat31 *) &e->m->t[i].n.x;
		struct mat31 *m2 = (struct mat31 *) &e->m->t[i].n.wx;
		mat33_x_mat31(mat_normal, m1, m2);
	}
}

extern int graph_dev_entity_render_order(struct entity_context *cx, struct entity *e)
{
	/* all software rendered entites are rendered far to near */
	return GRAPH_DEV_RENDER_FAR_TO_NEAR;
}

void graph_dev_draw_entity(struct entity_context *cx, struct entity *e, union vec3 *camera_light_pos,
			const struct entity_transform *transform)
{
	const struct mat44 *mat_mvp = &transform->mvp;
	const struct mat44 *mat_mv = &transform->mv;
	const struct mat33 *mat_normal = &transform->normal;

	transform_entity(mat_mvp, mat_mv, mat_normal, e);

	/* nebula rendering not supported on gdk */
	if (e->material_ptr && e->material_ptr->type == MATERIAL_NEBULA)
		return;

	switch (e->m->geometry_mode) {
	case MESH_GEOMETRY_TRIANGLES:
	{
		/* Map entity into camera space */
		struct mat41 model_ent_pos = { {0, 0, 0, 1} }; /* center of our model */
		struct mat41 tmp_pos;
		mat44_x_mat41(mat_mv, &model_ent_pos, &tmp_pos);
		union vec3 *camera_ent_pos = (union vec3 *) &tmp_pos.m[0];

		/* find the vector from the light to our entity in camera space */
		union vec3 light_vector;
		vec3_sub(&light_vector, camera_light_pos, camera_ent_pos);
		vec3_normalize_self(&light_vector);

		software_render_entity_triangles(cx, e, &light_vector);
		break;
	}
	case MESH_GEOMETRY_LINES:
		software_render_entity_lines(cx, e);
		break;
	case MESH_GEOMETRY_POINTS:
		software_render_entity_point_cloud(cx, e);
		break;
	}
}

void graph_dev_draw_3d_line(struct entity_context *cx, const struct mat44 *mat_vp,
	float x1, float y1, float z1, float x2, float y2, float z2)
{
	float sx1, sy1, sx2, sy2;
	if (!transform_line(cx, x1, y1, z1, x2, y2, z2, &sx1, &sy1, &sx2, &sy2)) {
		sng_current_draw_line(sx1, sy1, sx2, sy2);
	}
}

/* clipping triangles to frustum taken from
   http://simonstechblog.blogspot.com/2012/04/software-rasterizer-part-2.html
   converted from javascript to c */
static const float ZERO_TOLERANCE = 0.000001f;

typedef float (*clip_line_fn)(float vtx0x, float vtx0w, float vtx1x, float vtx1w);
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

static float get_x(const struct mat41 *m)
{
	return m->m[0];
}

static float get_y(const struct mat41 *m)
{
	return m->m[1];
}

static float get_z(const struct mat41 *m)
{
	return m->m[2];
}

static void mat41_lerp(struct mat41 *out, struct mat41 *v1, struct mat41 *v2, float t)
{
	float inv_t = 1.0 - t;
	out->m[0] = t * v2->m[0] + inv_t * v1->m[0];
	out->m[1] = t * v2->m[1] + inv_t * v1->m[1];
	out->m[2] = t * v2->m[2] + inv_t * v1->m[2];
	out->m[3] = t * v2->m[3] + inv_t * v1->m[3];
}

/* the clip line to plane with interpolated varying */
static void clipped_line_from_plane(struct mat41 *out, float t, struct mat41 *vs_out0,
					struct mat41 *vs_out1)
{
	if (t < 0)
		printf("Assert: two vertex does not intersect plane\n");

	mat41_lerp(out, vs_out0, vs_out1, t);
}

static int clip_triangle_to_plane(struct clip_triangle *clipped_triangles,
	clip_line_fn clipping_func,
	struct mat41 *vs_out0, struct mat41 *vs_out1, struct mat41 *vs_out2,
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
		float line0_t = clipping_func(component0, pos0w, component1, pos1w);
		float line1_t = clipping_func(component2, pos2w, component1, pos1w);
		clipped_triangles[0].v[0] = *vs_out1;
		clipped_line_from_plane(&clipped_triangles[0].v[1], line1_t, vs_out2, vs_out1);
		clipped_line_from_plane(&clipped_triangles[0].v[2], line0_t, vs_out0, vs_out1);
		return 1;
	}

	if (vtx_0_outside && vtx_1_outside && !vtx_2_outside) {
		float line1_t = clipping_func(component1, pos1w, component2, pos2w);
		float line2_t = clipping_func(component0, pos0w, component2, pos2w);
		clipped_triangles[0].v[0] = *vs_out2;
		clipped_line_from_plane(&clipped_triangles[0].v[1], line2_t, vs_out0, vs_out2);
		clipped_line_from_plane(&clipped_triangles[0].v[2], line1_t, vs_out1, vs_out2);
		return 1;
	}

	if (!vtx_0_outside && !vtx_1_outside && vtx_2_outside) {
		/* 1 vtx outside, split into 2 triangles */
		float line1_t = clipping_func(component2, pos2w, component1, pos1w);
		float line2_t = clipping_func(component2, pos2w, component0, pos0w);

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
		float line0_t = clipping_func(component1, pos1w, component0, pos0w);
		float line1_t = clipping_func(component1, pos1w, component2, pos2w);

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
		float line0_t = clipping_func(component0, pos0w, component1, pos1w);
		float line2_t = clipping_func(component0, pos0w, component2, pos2w);

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

static int clip_triangle(struct clip_triangle *triangles, struct mat41 *vs_out0,
				struct mat41 *vs_out1, struct mat41 *vs_out2)
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
		for (i = 0; i < ntriangles; i++) {
			struct mat41 *vtx0 = &triangles[i].v[0];
			struct mat41 *vtx1 = &triangles[i].v[1];
			struct mat41 *vtx2 = &triangles[i].v[2];

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

static int clip_line(struct mat41 *vtx0, struct mat41 *vtx1)
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
		} else if (!is_outside0 && is_outside1) {
			float line1_t = clipping_func[plane](component1, pos1w, component0, pos0w);
			clipped_line_from_plane(vtx1, line1_t, vtx1, vtx0);
		}
	}
	return 0;
}

void graph_dev_draw_skybox(const struct mat44 *mat_vp)
{
	/* no skybox on gdk rendering */
}

int graph_dev_load_skybox_texture(
	const char *texture_filename_pos_x,
	const char *texture_filename_neg_x,
	const char *texture_filename_pos_y,
	const char *texture_filename_neg_y,
	const char *texture_filename_pos_z,
	const char *texture_filename_neg_z)
{
	return 0; /* no skybox on gdk rendering */
}

unsigned int graph_dev_load_cubemap_texture(
	int is_inside,
	int linear_color_space,
	const char *texture_filename_pos_x,
	const char *texture_filename_neg_x,
	const char *texture_filename_pos_y,
	const char *texture_filename_neg_y,
	const char *texture_filename_pos_z,
	const char *texture_filename_neg_z)
{
	/* no cubemap texture on gdk rendering */
	return (unsigned int) -1;
}

unsigned int graph_dev_load_texture(const char *filename, int linear_colorspace)
{
	/* no laserbolt texture with gdk rendering */
	return (unsigned int) -1;
}

unsigned int graph_dev_load_texture_no_mipmaps(const char *filename, int linear_colorspace)
{
	return (unsigned int) -1;
}

const char *graph_dev_get_texture_filename(unsigned int texture_id)
{
	return "";
}

void graph_dev_display_debug_menu_show()
{
	/* no debug at this time */
}

int graph_dev_graph_dev_debug_menu_click(int x, int y)
{
	/* return false as we didn't consume the click */
	return 0;
}

int graph_dev_reload_changed_textures()
{
	return 0; /* noop */
}

int graph_dev_reload_changed_cubemap_textures()
{
	return 0; /* noop */
}

void graph_dev_grab_framebuffer(__attribute__((unused)) unsigned char **buffer,
				__attribute__((unused)) int *width,
				__attribute__((unused)) int *height)
{
	/* noop */
}

void graph_dev_expire_all_textures(void)
{
	/* noop */
}

void graph_dev_expire_texture(char *filename)
{
	/* noop */
}

void graph_dev_expire_cubemap_texture(int is_inside,
						const char *texture_filename_pos_x,
						const char *texture_filename_neg_x,
						const char *texture_filename_pos_y,
						const char *texture_filename_neg_y,
						const char *texture_filename_pos_z,
						const char *texture_filename_neg_z)
{
	/* noop */
}

void graph_dev_reload_all_shaders(void)
{
	/* noop */
}

void graph_dev_set_tonemapping_gain(float tmg) { }

