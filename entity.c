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
#include <stdlib.h>
#include <limits.h>

#include "mtwist.h"
#include "mathutils.h"
#include "matrix.h"
#include "vertex.h"
#include "triangle.h"
#include "quat.h"
#include "vec4.h"
#include "mesh.h"
#include "stl_parser.h"
#include "snis_graph.h"
#include "material.h"
#include "entity.h"
#include "mathutils.h"
#include "stacktrace.h"

#include "my_point.h"
#include "snis_font.h"
#include "snis_typeface.h"
#include "snis_alloc.h"

#include "entity_private.h"
#include "graph_dev.h"

static int clip_line(struct mat41* vtx0, struct mat41* vtx1);

struct entity *add_entity(struct entity_context *cx,
	struct mesh *m, float x, float y, float z, int color)
{
	int n;
	static int throttle = 0;

#if ADD_ENTITY_CHAOS_MONKEY
	/* for testing that code can withstand add_entity failures */
	if (snis_randn(1000) < 50)
		return NULL;
#endif

	n = snis_object_pool_alloc_obj(cx->entity_pool);
	if (n < 0) {
		if (throttle < 10 || (throttle & (0x3f)) == 0) { /* Throttle these messages */
			printf("Out of entities at %s:%d\n", __FILE__, __LINE__);
			fflush(stdout);
		}
		throttle++;
		return NULL;
	} else {
		if (throttle > 0)
			throttle--;
	}

	cx->entity_list[n].visible = 1;
	cx->entity_list[n].e_visible = 1;
	cx->entity_list[n].m = m;
	cx->entity_list[n].high_poly = m;
	cx->entity_list[n].low_poly = m;
	cx->entity_list[n].x = x;
	cx->entity_list[n].y = y;
	cx->entity_list[n].z = z;
	vec3_init(&cx->entity_list[n].e_pos, x, y, z);
	vec3_init(&cx->entity_list[n].scale, 1, 1, 1);
	vec3_init(&cx->entity_list[n].e_scale, 1, 1, 1);
	cx->entity_list[n].color = color;
	cx->entity_list[n].render_style = RENDER_NORMAL;
	cx->entity_list[n].user_data = NULL;
	cx->entity_list[n].shadecolor = 0;
	cx->entity_list[n].orientation = identity_quat;
	cx->entity_list[n].e_orientation = identity_quat;
	cx->entity_list[n].material_ptr = 0;
	cx->entity_list[n].parent = 0;
	cx->entity_list[n].entity_child_index = -1;
	cx->entity_list[n].emit_intensity = 1.0;
	cx->entity_list[n].sx = -1;
	cx->entity_list[n].sy = -1;
	cx->entity_list[n].onscreen = 0;
	cx->entity_list[n].no_cast_shadow = 0;
	cx->entity_list[n].being_removed = 0;
#if CHECK_ENTITY_CHILD_LIST_FOR_CYCLES
	cx->entity_list[n].visited = 0;
#endif
	if (m && m->material)
		update_entity_material(&cx->entity_list[n], m->material);

	return &cx->entity_list[n];
}

static void debug_duplicate_entity_list_entries(struct entity_context *cx,
			struct entity *e, int duplicate_list_index)
{
#if CHECK_ENTITY_CHILD_LIST_FOR_CYCLES
	int child = e->entity_child_index;
	struct entity_child *this_ec = NULL;
	int loop_count = 0;
	fprintf(stderr, "Debugging entity with duplicate entry in child list:\n");
	fprintf(stderr, "Entity %lu: '%s'\n", e - cx->entity_list, mesh_name(e->m));

	/* Walk the list, printing it out, and printing out info about the entity.  */
	fprintf(stderr, "%-4s %-20s %-20s\n", "list pos", "entity[] index", "next child index");
	fprintf(stderr, "%-4s %-20s %-20s\n", "----", "----------------", "----------------");
	do {
		if (child < 0)
			break;
		this_ec = &cx->entity_child_list[child];
		fprintf(stderr, "%-4d %-20d %-20d %s\n",
				loop_count, this_ec->child_entity_index,
				this_ec->next_entity_child_index,
				child == duplicate_list_index ? "(Duplicate)" : "");
		if (this_ec->child_entity_index >= 0) {
			fprintf(stderr, "Entity %d: '%s'\n", this_ec->child_entity_index,
				mesh_name(cx->entity_list[this_ec->child_entity_index].m));
		}
		child = this_ec->next_entity_child_index;
		loop_count++;
	} while (loop_count < 20);
	if (loop_count >= 20)
		fprintf(stderr, "Bailed out on maximum loop count\n");
#endif
}

static void check_entity_child_list_for_cycles(int line, struct entity_context *cx, struct entity *e)
{
#if CHECK_ENTITY_CHILD_LIST_FOR_CYCLES
	int child = e->entity_child_index;
	struct entity_child *this_ec = NULL;

	if (child < 0)
		return;
	/* Walk the list setting visited flags, checking for visited nodes first
	 * If we encounter a visited flag, we found a cycle
	 */
	do {
		if (child < 0)
			break;
		this_ec = &cx->entity_child_list[child];
		if (this_ec->visited) {
			fprintf(stderr, "Cycle in entity child list detected, entity.c:%d\n", line);
			stacktrace("Cycle in entity child list detected\n");
			abort();
		}
		if (this_ec->child_entity_index >= 0) {
			if (cx->entity_list[this_ec->child_entity_index].visited) {
				fprintf(stderr,
					"Duplicate entity in entity list detected, entity.c:%d\n", line);
				stacktrace("Duplicate entity in entity child list detected\n");
				debug_duplicate_entity_list_entries(cx, e, this_ec->child_entity_index);
			}
			cx->entity_list[this_ec->child_entity_index].visited = 1;
		}
		this_ec->visited = 1;
		cx->entity_list[this_ec->child_entity_index].visited = 1;
		child = this_ec->next_entity_child_index;
	} while (1);

	child = e->entity_child_index;
	do { /* Walk the list and clear all the visited flags */
		if (child < 0)
			break;
		this_ec = &cx->entity_child_list[child];
		if (this_ec->child_entity_index >= 0)
			cx->entity_list[this_ec->child_entity_index].visited = 0;
		this_ec->visited = 0;
		child = this_ec->next_entity_child_index;
	} while (1);
#endif
}

/* We need to call this when removing an entity that is a child of
 * another entity which is not also being removed, and when updating
 * an entity's parent if the entity already had a parent.
 */
static void remove_child_from_parent(struct entity_context *cx,
				struct entity *child, struct entity *parent)
{
	/* index of child into cx->entity_list[] */
	int child_index = child - cx->entity_list;

	/* index into cx->child_entity_list[] */
	int entity_child_index = parent->entity_child_index;
	struct entity_child *last_ec = 0;

	while (entity_child_index >= 0) {
		struct entity_child *this_ec = &cx->entity_child_list[entity_child_index];
		if (this_ec->child_entity_index == child_index) {
			/* we found the child, fix the list */
			if (!last_ec) /* first link */
				child->parent->entity_child_index = this_ec->next_entity_child_index;
			else
				last_ec->next_entity_child_index = this_ec->next_entity_child_index;

			snis_object_pool_free_object(cx->entity_child_pool, entity_child_index);

			break; /* found the child, done */
		}
		entity_child_index = this_ec->next_entity_child_index;
		last_ec = this_ec;
	}
}

static void remove_entity_children(struct entity_context *cx, struct entity *e)
{
	check_entity_child_list_for_cycles(__LINE__, cx, e);
	int entity_child_index = e->entity_child_index;

	if (e->parent && !e->parent->being_removed)
		remove_child_from_parent(cx, e, e->parent);

	while (entity_child_index >= 0) {
		struct entity_child *this_ec = &cx->entity_child_list[entity_child_index];
		struct entity *this_child = &cx->entity_list[this_ec->child_entity_index];

		if (this_child->entity_child_index >= 0) {
			if (this_child == e) {
				fprintf(stderr, "this_child == e in remove_entity_children\n");
				abort();
			}
			remove_entity_children(cx, this_child);
		}
		snis_object_pool_free_object(cx->entity_pool, this_ec->child_entity_index);

		int next_entity_child_index = this_ec->next_entity_child_index;
		snis_object_pool_free_object(cx->entity_child_pool, entity_child_index);

		entity_child_index = next_entity_child_index;
	}
	e->entity_child_index = -1;
	check_entity_child_list_for_cycles(__LINE__, cx, e);
}

void remove_entity(struct entity_context *cx, struct entity *e)
{
	int index;

	if (!e)
		return;
	e->being_removed = 1; /* let entity children know they don't have to unlink from parent */
	if (e->entity_child_index >= 0)
		remove_entity_children(cx, e);
	e->being_removed = 0;
	index = e - &cx->entity_list[0];
	snis_object_pool_free_object(cx->entity_pool, index);
	check_entity_child_list_for_cycles(__LINE__, cx, e);
}

void remove_all_entity(struct entity_context *cx)
{
	snis_object_pool_free_all_objects(cx->entity_pool);
	snis_object_pool_free_all_objects(cx->entity_child_pool);
}

void update_entity_parent(struct entity_context *cx, struct entity *child, struct entity *parent)
{
	check_entity_child_list_for_cycles(__LINE__, cx, parent);
	if (child->parent == parent)
		return;

	int new_entity_child_index = -1;
	int child_index = child - &cx->entity_list[0];

	if (parent) {
		/* preallocate this so that in case we can't get one, at least we don't crash. */
		new_entity_child_index = snis_object_pool_alloc_obj(cx->entity_child_pool);
		if (new_entity_child_index < 0) {
			printf("entity_child_pool exhausted at %s:%d\n", __FILE__, __LINE__);
			return;
		}
#if CHECK_ENTITY_CHILD_LIST_FOR_CYCLES
		cx->entity_child_list[new_entity_child_index].visited = 0;
#endif
		cx->entity_child_list[new_entity_child_index].next_entity_child_index = -1;
		cx->entity_child_list[new_entity_child_index].child_entity_index = -1;
	}

	if (child->parent) /* Unlink the old parent, if any */
		remove_child_from_parent(cx, child, child->parent);

	child->parent = parent; /* link the new parent */
	if (parent) {
		/* add child into new parent child_entity_list */
		struct entity_child *new_ec = &cx->entity_child_list[new_entity_child_index];

		/* insert entity_child at the front of the list */
		new_ec->child_entity_index = child_index;
		new_ec->next_entity_child_index = parent->entity_child_index;
		parent->entity_child_index = new_entity_child_index;
	}
	check_entity_child_list_for_cycles(__LINE__, cx, parent);
}

void update_entity_pos(struct entity *e, float x, float y, float z)
{
	vec3_init(&e->e_pos, x, y, z);
}

void update_entity_orientation(struct entity *e, const union quat *orientation)
{
	e->e_orientation = *orientation;
}

float entity_get_scale(struct entity *e)
{
	return vec3_cwise_max(&e->e_scale);
}

void update_entity_scale(struct entity *e, float scale)
{
	vec3_init(&e->e_scale, scale, scale, scale);
}

void update_entity_shadow_casting(struct entity *e, int casts_shadow)
{
	e->no_cast_shadow = !casts_shadow;
}

void entity_get_non_uniform_scale(struct entity *e, float *x_scale, float *y_scale, float *z_scale)
{
	*x_scale = e->e_scale.v.x;
	*y_scale = e->e_scale.v.y;
	*z_scale = e->e_scale.v.z;
}

void update_entity_non_uniform_scale(struct entity *e, float x_scale, float y_scale, float z_scale)
{
	vec3_init(&e->e_scale, x_scale, y_scale, z_scale);
}

void update_entity_color(struct entity *e, int color)
{
	e->color = color;
}

void update_entity_shadecolor(struct entity *e, int color)
{
	e->shadecolor = color;
}

void update_entity_visibility(struct entity *e, int visible)
{
	e->e_visible = visible;
}

int entity_get_visibility(struct entity *e)
{
	return e->e_visible;
}

void update_entity_material(struct entity *e, struct material *material_ptr)
{
	e->material_ptr = material_ptr;
}

struct material *entity_get_material(struct entity *e)
{
	return e->material_ptr;
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


static void calculate_model_matrices(struct camera_info *c, struct frustum *f, struct entity *e,
	struct entity_transform *transform)
{
	transform->v = &f->v_matrix;
	transform->vp = &f->vp_matrix;

	/* Model = (T*R)*S
	   T - translation matrix
	   R - rotation matrix
	   S - scale matrix
	   Combine for post multiply of vector */

	struct mat44d mat_t = {{
		{ 1, 0, 0, 0 },
		{ 0, 1, 0, 0 },
		{ 0, 0, 1, 0 },
		{ e->x, e->y, e->z, 1 }}};

	struct mat44d mat_s = {{
		{ e->scale.v.x, 0, 0, 0 },
		{ 0, e->scale.v.y, 0, 0 },
		{ 0, 0, e->scale.v.z, 0 },
		{ 0, 0, 0, 1 }}};

	struct mat44d mat_r = {{
		{ 1, 0, 0, 0 },
		{ 0, 1, 0, 0 },
		{ 0, 0, 1, 0 },
		{ 0, 0, 0, 1 } } };

	if (e->material_ptr && e->material_ptr->billboard_type != MATERIAL_BILLBOARD_TYPE_NONE) {
		switch (e->material_ptr->billboard_type) {
			/* aligned so that +y axis = camera up and normal parallel with camera look direction */
			case MATERIAL_BILLBOARD_TYPE_SCREEN:
			{
				/* take the corner 3x3 from the view matrix, transpose, and pad back to 4x4 */
				struct mat33d tm1, tm2;
				mat44_to_mat33_dd(transform->v, &tm1);
				mat33_transpose_dd(&tm1, &tm2);
				mat33_to_mat44_dd(&tm2, &mat_r);
			}
			break;

			/* aligned so that +y axis = camera up and normal is pointing to camera position */
			case MATERIAL_BILLBOARD_TYPE_SPHERICAL:
			{
				union vec3 cam_up = { { c->ux, c->uy, c->uz } };
				union vec3 look = { { c->x - e->x, c->y - e->y, c->z - e->z } };
				vec3_normalize_self(&look);

				union vec3 right;
				vec3_cross(&right, &cam_up, &look);
				vec3_normalize_self(&right);

				union vec3 up;
				vec3_cross(&up, &look, &right);
				vec3_normalize_self(&up);

				if (e->material_ptr->rotate_randomly) {
					/* Rotate randomly about normal. */
					union quat rotation;
					float angle = (snis_randn(1000) % 360) * M_PI / 180.0;
					quat_init_axis(&rotation, look.v.x, look.v.y, look.v.z, angle);
					quat_rot_vec_self(&up, &rotation);
					quat_rot_vec_self(&right, &rotation);
				}

				/* a rotation matrix to align with up, right, and look unit vectors
				   r.x r.y r.z 0
				   u.x u.y u.z 0
				   l.x l.y l.z 0
				   0   0   0   1 */
				mat_r.m[0][0] = right.v.x;
				mat_r.m[0][1] = right.v.y;
				mat_r.m[0][2] = right.v.z;
				mat_r.m[0][3] = 0;

				mat_r.m[1][0] = up.v.x;
				mat_r.m[1][1] = up.v.y;
				mat_r.m[1][2] = up.v.z;
				mat_r.m[1][3] = 0;

				mat_r.m[2][0] = look.v.x;
				mat_r.m[2][1] = look.v.y;
				mat_r.m[2][2] = look.v.z;
				mat_r.m[2][3] = 0;

				mat_r.m[3][0] = 0;
				mat_r.m[3][1] = 0;
				mat_r.m[3][2] = 0;
				mat_r.m[3][3] = 1;
			}
			break;

			/* aligned so that +y axis = quaternion axis and normal is pointing in direction of camera position */
			case MATERIAL_BILLBOARD_TYPE_AXIS:
			{
				union vec3 look_to_cam = { { c->x - e->x, c->y - e->y, c->z - e->z } };
				vec3_normalize_self(&look_to_cam);

				union vec3 up = { { 1, 0, 0 } };
				quat_rot_vec_self(&up, &e->orientation);
				vec3_normalize_self(&up);

				union vec3 right;
				vec3_cross(&right, &up, &look_to_cam);
				vec3_normalize_self(&right);

				union vec3 look;
				vec3_cross(&look, &right, &up);
				vec3_normalize_self(&look);

				/* a rotation matrix to align with up, right, and look unit vectors
				   r.x r.y r.z 0
				   u.x u.y u.z 0
				   l.x l.y l.z 0
				   0   0   0   1 */
				struct mat44d mat_r_y;
				mat_r_y.m[0][0] = right.v.x;
				mat_r_y.m[0][1] = right.v.y;
				mat_r_y.m[0][2] = right.v.z;
				mat_r_y.m[0][3] = 0;

				mat_r_y.m[1][0] = up.v.x;
				mat_r_y.m[1][1] = up.v.y;
				mat_r_y.m[1][2] = up.v.z;
				mat_r_y.m[1][3] = 0;

				mat_r_y.m[2][0] = look.v.x;
				mat_r_y.m[2][1] = look.v.y;
				mat_r_y.m[2][2] = look.v.z;
				mat_r_y.m[2][3] = 0;

				mat_r_y.m[3][0] = 0;
				mat_r_y.m[3][1] = 0;
				mat_r_y.m[3][2] = 0;
				mat_r_y.m[3][3] = 1;

				/* rotate the model by 90 degrees so +x is up before applying
				   the billboarding matrix */
				struct mat44d mat_y_to_x = { {
					{ 0, 1, 0, 0 },
					{ -1, 0, 0, 0 },
					{ 0, 0, 1, 0 },
					{ 0, 0, 0, 1 } } };

				mat44_product_ddd(&mat_r_y, &mat_y_to_x, &mat_r);
			}
			break;
		}
	} else {
		/* normal quaternion based rotation */
		quat_to_rh_rot_matrix_fd(&e->orientation, &mat_r.m[0][0]);
	}

	mat44_product_ddd(&mat_t, &mat_r, &transform->m_no_scale);

	mat44_product_ddd(&transform->m_no_scale, &mat_s, &transform->m);

	/* calculate the final model-view-proj and model-view matrices */
	mat44_product_ddf(transform->vp, &transform->m, &transform->mvp);
	mat44_product_ddf(transform->v, &transform->m, &transform->mv);

	/* normal transform is the inverse transpose of the upper left 3x3 of the model-view matrix */
	struct mat33 mat_tmp33;
	mat33_inverse_transpose_ff(mat44_to_mat33_ff(&transform->mv, &mat_tmp33), &transform->normal);
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
		VERTEX_INIT(x1, y1, z1, 1),
		VERTEX_INIT(x2, y2, z2, 1) };

	/* there is no model transform on a point */
	struct mat44 mat_vp;
	mat44_convert_df(&cx->camera.frustum.vp_matrix, &mat_vp);

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

static int transform_point_in_frustum(struct entity_context *cx, struct frustum *f,
	float x, float y, float z, float *sx, float *sy)
{
	struct vertex v = VERTEX_INIT(x, y, z, 1);

	/* there is no model transform on a point */
	struct mat44 mat_vp;
	mat44_convert_df(&f->vp_matrix, &mat_vp);

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

int transform_point(struct entity_context *cx, float x, float y, float z, float *sx, float *sy)
{
	return transform_point_in_frustum(cx, &cx->camera.frustum, x, y, z, sx, sy);
}

static void render_entity(struct entity_context *cx, struct frustum *f, struct entity *e, union vec3 *camera_light_pos)
{
	/* calculate screen coords of entity as a whole */
	transform_point_in_frustum(cx, f, e->x, e->y, e->z, &e->sx, &e->sy);

	if (e->sx >=0 && e->sy >= 0)
		e->onscreen = 1;

	/* Do not render entities with scale of zero. Doing so will lead to infinities
	 * in the model normal matrix, besides being pointless.
	 */
	if (e->scale.v.x == 0 && e->scale.v.y == 0 && e->scale.v.z == 0) {
		e->onscreen = 0;
		return;
	}

	struct entity_transform transform;
	calculate_model_matrices(&cx->camera, f, e, &transform);

	graph_dev_draw_entity(cx, e, camera_light_pos, &transform);
}

void render_line(struct entity_context *cx, float x1, float y1, float z1, float x2, float y2, float z2)
{
	calculate_camera_transform(cx);

	struct mat44 mat_vp;
	mat44_convert_df(&cx->camera.frustum.vp_matrix, &mat_vp);

	graph_dev_draw_3d_line(cx, &mat_vp, x1, y1, z1, x2, y2, z2);
}

void render_skybox(struct entity_context *cx)
{
	graph_dev_draw_skybox(&cx->camera.frustum.vp_matrix_no_translate);
}

#if defined(__APPLE__)  || defined(__FreeBSD__)
static int object_depth_compare_greater(void *vcx, const void *a, const void *b)
#else
static int object_depth_compare_greater(const void *a, const void *b, void *vcx)
#endif
{
	struct entity_context *cx = vcx;
	struct entity *A = &cx->entity_list[*(const int *) a];
	struct entity *B = &cx->entity_list[*(const int *) b];

	if (A->dist3dsqrd < B->dist3dsqrd)
		return 1;
	if (A->dist3dsqrd > B->dist3dsqrd)
		return -1;
	return 0;
}

#if defined(__APPLE__)  || defined(__FreeBSD__)
static int object_depth_compare_less(void *vcx, const void *a, const void *b)
#else
static int object_depth_compare_less(const void *a, const void *b, void *vcx)
#endif
{
	struct entity_context *cx = vcx;
	struct entity *A = &cx->entity_list[*(const int *) a];
	struct entity *B = &cx->entity_list[*(const int *) b];

	if (A->dist3dsqrd > B->dist3dsqrd)
		return 1;
	if (A->dist3dsqrd < B->dist3dsqrd)
		return -1;
	return 0;
}

static void sort_entity_distances(struct entity_context *cx)
{
#if defined(__APPLE__)  || defined(__FreeBSD__)
	qsort_r(cx->far_to_near_entity_depth, cx->nfar_to_near_entity_depth, sizeof(cx->far_to_near_entity_depth[0]),
		cx, object_depth_compare_greater);
	qsort_r(cx->near_to_far_entity_depth, cx->nnear_to_far_entity_depth, sizeof(cx->near_to_far_entity_depth[0]),
		cx, object_depth_compare_less);
#else
	qsort_r(cx->far_to_near_entity_depth, cx->nfar_to_near_entity_depth, sizeof(cx->far_to_near_entity_depth[0]),
		object_depth_compare_greater, cx);
	qsort_r(cx->near_to_far_entity_depth, cx->nnear_to_far_entity_depth, sizeof(cx->near_to_far_entity_depth[0]),
		object_depth_compare_less, cx);
#endif
}

/* from http://www.crownandcutlass.com/features/technicaldetails/frustum.html
This page and its contents are Copyright 2000 by Mark Morley
Unless otherwise noted, you may use any and all code examples provided herein in any way you want. */

int sphere_in_frustum(struct frustum *f, float x, float y, float z, float radius)
{
	int p;

	for (p = 0; p < 6; p++)
		if (f->planes[p][0] * x + f->planes[p][1] * y + f->planes[p][2] * z + f->planes[p][3] <= -radius)
			return 0;
	return 1;
}

/* from http://www.crownandcutlass.com/features/technicaldetails/frustum.html
This page and its contents are Copyright 2000 by Mark Morley
Unless otherwise noted, you may use any and all code examples provided herein in any way you want. */

static void extract_frustum_planes(struct frustum *f)
{
	const double *clip = &f->vp_matrix.m[0][0];
	float t;

	/* Extract the numbers for the RIGHT plane */
	f->planes[0][0] = clip[3] - clip[0];
	f->planes[0][1] = clip[7] - clip[4];
	f->planes[0][2] = clip[11] - clip[8];
	f->planes[0][3] = clip[15] - clip[12];

	/* Normalize the result */
	t = dist3d(f->planes[0][0], f->planes[0][1], f->planes[0][2]);
	f->planes[0][0] /= t;
	f->planes[0][1] /= t;
	f->planes[0][2] /= t;
	f->planes[0][3] /= t;

	/* Extract the numbers for the LEFT plane */
	f->planes[1][0] = clip[3] + clip[0];
	f->planes[1][1] = clip[7] + clip[4];
	f->planes[1][2] = clip[11] + clip[8];
	f->planes[1][3] = clip[15] + clip[12];

	/* Normalize the result */
	t = dist3d(f->planes[1][0], f->planes[1][1], f->planes[1][2]);
	f->planes[1][0] /= t;
	f->planes[1][1] /= t;
	f->planes[1][2] /= t;
	f->planes[1][3] /= t;

	/* Extract the BOTTOM plane */
	f->planes[2][0] = clip[3] + clip[1];
	f->planes[2][1] = clip[7] + clip[5];
	f->planes[2][2] = clip[11] + clip[9];
	f->planes[2][3] = clip[15] + clip[13];

	/* Normalize the result */
	t = dist3d(f->planes[2][0], f->planes[2][1], f->planes[2][2]);
	f->planes[2][0] /= t;
	f->planes[2][1] /= t;
	f->planes[2][2] /= t;
	f->planes[2][3] /= t;

	/* Extract the TOP plane */
	f->planes[3][0] = clip[3] - clip[1];
	f->planes[3][1] = clip[7] - clip[5];
	f->planes[3][2] = clip[11] - clip[9];
	f->planes[3][3] = clip[15] - clip[13];

	/* Normalize the result */
	t = dist3d(f->planes[3][0], f->planes[3][1], f->planes[3][2]);
	f->planes[3][0] /= t;
	f->planes[3][1] /= t;
	f->planes[3][2] /= t;
	f->planes[3][3] /= t;

	/* Extract the FAR plane */
	f->planes[4][0] = clip[3] - clip[2];
	f->planes[4][1] = clip[7] - clip[6];
	f->planes[4][2] = clip[11] - clip[10];
	f->planes[4][3] = clip[15] - clip[14];

	/* Normalize the result */
	t = dist3d(f->planes[4][0], f->planes[4][1], f->planes[4][2]);
	f->planes[4][0] /= t;
	f->planes[4][1] /= t;
	f->planes[4][2] /= t;
	f->planes[4][3] /= t;

	/* Extract the NEAR plane */
	f->planes[5][0] = clip[3] + clip[2];
	f->planes[5][1] = clip[7] + clip[6];
	f->planes[5][2] = clip[11] + clip[10];
	f->planes[5][3] = clip[15] + clip[14];

	/* Normalize the result */
	t = dist3d(f->planes[5][0], f->planes[5][1], f->planes[5][2]);
	f->planes[5][0] /= t;
	f->planes[5][1] /= t;
	f->planes[5][2] /= t;
	f->planes[5][3] /= t;
}

static void calculate_camera_transform_near_far(struct camera_info *c, struct frustum *f, float near, float far)
{
	f->near = near;
	f->far = far;

	/* based on gluPerspective to find the right, left, top, and bottom */
	float scale = tan(c->angle_of_view * 0.5) * f->near;
	f->right = ((float) c->xvpixels / (float) c->yvpixels * scale);
	f->left = -f->right;
	f->top = scale;
	f->bottom = -f->top;

	struct mat41 *v; /* camera relative y axis (up/down) */
	struct mat41 *n; /* camera relative z axis (into view plane) */
	struct mat41 *u; /* camera relative x axis (left/right) */

	struct mat41 up;
	up.m[0] = c->ux;
	up.m[1] = c->uy;
	up.m[2] = c->uz;
	up.m[3] = 1;

	/* Translate to camera position... */
	struct mat44d cameratrans_transform = { { { 1, 0, 0, 0 },
						  { 0, 1, 0, 0 },
						  { 0, 0, 1, 0 },
						  { -c->x, -c->y, -c->z, 1} } };

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
	look_direction.m[0] = (c->lx - c->x);
	look_direction.m[1] = (c->ly - c->y);
	look_direction.m[2] = (c->lz - c->z);
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
	perspective_transform.m[0][0] = (2 * f->near) / (f->right - f->left);
	perspective_transform.m[0][1] = 0.0;
	perspective_transform.m[0][2] = 0.0;
	perspective_transform.m[0][3] = 0.0;
	perspective_transform.m[1][0] = 0.0;
	perspective_transform.m[1][1] = (2.0 * f->near) / (f->top - f->bottom);
	perspective_transform.m[1][2] = 0.0;
	perspective_transform.m[1][3] = 0.0;
	perspective_transform.m[2][0] = (f->right + f->left) / (f->right - f->left);
	perspective_transform.m[2][1] = (f->top + f->bottom) / (f->top - f->bottom);
	perspective_transform.m[2][2] = -(f->far + f->near) / (f->far - f->near);
	perspective_transform.m[2][3] = -1.0;
	perspective_transform.m[3][0] = 0.0;
	perspective_transform.m[3][1] = 0.0;
	perspective_transform.m[3][2] = (-2 * f->far * f->near) / (f->far - f->near);
	perspective_transform.m[3][3] = 0.0;

	/* save perspective */
	f->p_matrix = perspective_transform;

	/* make the view matrix, V = L * T */
	mat44_product_ddd(&cameralook_transform, &cameratrans_transform, &f->v_matrix);

	/* make the view-perspective matrix, VP = P * V */
	mat44_product_ddd(&perspective_transform, &f->v_matrix, &f->vp_matrix);

	/* save vp matrix without any camera translation */
	mat44_product_ddf(&perspective_transform, &cameralook_transform, &f->vp_matrix_no_translate);

	/* pull out the frustum planes for entity culling */
	extract_frustum_planes(f);
}

void calculate_camera_transform(struct entity_context *cx)
{
	/* calculate for the entire frustum */
	calculate_camera_transform_near_far(&cx->camera, &cx->camera.frustum, cx->camera.near, cx->camera.far);
}

#if SPACEDUST
static void reposition_space_dust(struct entity_context *cx, struct vertex *fs, float radius);
#endif

#if MOVING_STARFIELD
/* The radius passed to entity_init_star_field() is the CLOSEST a star may come, not the
 * furthest it may be.  That is the useful way round: parallax rate is speed over distance,
 * so a floor on the distance is a ceiling on the drift, and the radius becomes a promise
 * that no star will sweep faster than speed/radius no matter where it is or how long you
 * fly toward it.
 *
 * Read the other way -- radius as an outer bound, which is what the space dust does -- there
 * is no floor at all, and any star ahead keeps closing for as long as you fly at it.  Those
 * are the ones that streak, which is exactly what the dust is for and exactly what a star
 * field must not do.
 *
 * Stars therefore live in a shell between radius and STAR_FIELD_OUTER times it, and are
 * recycled when they leave through EITHER face.  The shell is deep because BOTH faces need
 * a fade wide enough to be invisible: a star leaving through the near face is just as
 * abrupt as one arriving at the far face. */
#define STAR_FIELD_OUTER 3.0

static void reposition_star_field_star(struct entity_context *cx, struct vertex *fs, float radius,
					int initial);
#endif

static void update_entity_child_state(struct entity *e)
{
	int visible = e->e_visible;
	union vec3 pos = e->e_pos;
	union vec3 scale = e->e_scale;
	union quat orientation = e->e_orientation;

	struct entity *parent = e->parent;
	while (parent) {
		visible = visible && parent->e_visible;
		quat_rot_vec_self(&pos, &parent->e_orientation);
		vec3_add_self(&pos, &parent->e_pos);
		vec3_cwise_product_self(&scale, &parent->e_scale);
		quat_mul_self_right(&parent->e_orientation, &orientation);

		parent = parent->parent;
	}

	e->visible = visible;
	e->x = pos.v.x;
	e->y = pos.v.y;
	e->z = pos.v.z;
	e->scale = scale;
	e->orientation = orientation;
}


/* Must not exceed MAX_SHADOW_CASCADES in graph_dev_opengl.c. */
#define MAX_ENTITY_SHADOW_CASCADES 6
/* Must match SHADOW_MAP_TEXTURE_SIZE in graph_dev_opengl.c; used to snap each cascade's ortho
 * window to texel increments so shadow texels do not swim as the camera moves. */
#define ENTITY_SHADOW_MAP_TEXTURE_SIZE 4096

/* The shadow cascades cover the view frustum only out to this distance, beyond which
 * objects neither cast nor receive shadows (keeps shadow texels usefully small).  Sized to
 * take in a starbase (~210 unit radius) and the largest asteroids (~640) at the ranges they
 * are actually seen from; with a logarithmic split this costs almost nothing near the camera. */
static float shadow_map_max_distance = 14000.0;
/* Number of shadow cascades and the split-scheme blend (0 = uniform spacing, 1 = fully
 * logarithmic).
 *
 * Lambda must be 1.0.  Shadow quality is best measured in shadow texels per screen pixel,
 * which works out to 2 * d * tan(fov / 2) / (height * texel): holding it constant as the
 * viewing distance d changes requires texel to be proportional to d, and only a purely
 * logarithmic split puts the cascade far distances in the geometric progression that gives
 * that.  Any uniform component breaks the proportionality, and it does so violently -- the
 * uniform term here is around 200x the logarithmic one, so even lambda 0.99 leaves cascade 0
 * three times larger than it should be, and the old 0.6 left it nearly a hundred times too
 * large.  The visible symptom was shadow quality collapsing as you approached an object,
 * worst of all in the gun turret view where the camera sits a few units from the hull. */
static int shadow_map_num_cascades = 6;
static float shadow_map_split_lambda = 1.0;

/* Fit an orthographic shadow frustum to the slice of the camera view frustum between
 * near_d and far_d, and build the world-space -> light clip-space matrix for that cascade.
 * The light source is whatever set_lighting() configured (cx->light) -- the sun at the
 * world origin in normal play -- and is treated as directional over the slice.  Returns
 * 1 on success, 0 if degenerate. */
static int compute_shadow_light_matrix(struct entity_context *cx, float near_d, float far_d,
	struct mat44d *world_to_lightclip)
{
	struct camera_info *cam = &cx->camera;
	union vec3 cpos, look, up0, n, u, v;
	int i;

	vec3_init(&cpos, cam->x, cam->y, cam->z);
	vec3_init(&look, cam->lx - cam->x, cam->ly - cam->y, cam->lz - cam->z);
	if (vec3_dot(&look, &look) < 1e-12)
		return 0;
	vec3_normalize_self(&look);
	n = look;
	vec3_init(&up0, cam->ux, cam->uy, cam->uz);
	vec3_cross(&u, &n, &up0);
	if (vec3_dot(&u, &u) < 1e-12)
		return 0;
	vec3_normalize_self(&u);
	vec3_cross(&v, &u, &n);
	vec3_normalize_self(&v);

	float aspect = (float) cam->xvpixels / (float) cam->yvpixels;
	if (far_d <= near_d)
		return 0;

	/* Eight corners of the (clamped) view frustum in world space. */
	union vec3 corners[8];
	int ci = 0;
	float dist[2] = { near_d, far_d };
	int di, sx, sy;
	for (di = 0; di < 2; di++) {
		float d = dist[di];
		float h = tanf(cam->angle_of_view * 0.5f) * d;
		float w = aspect * h;
		for (sy = -1; sy <= 1; sy += 2) {
			for (sx = -1; sx <= 1; sx += 2) {
				union vec3 corner, tmp;
				vec3_mul(&corner, &n, d);
				vec3_mul(&tmp, &u, w * sx);
				vec3_add_self(&corner, &tmp);
				vec3_mul(&tmp, &v, h * sy);
				vec3_add_self(&corner, &tmp);
				vec3_add_self(&corner, &cpos);
				corners[ci++] = corner;
			}
		}
	}

	/* Region center: the average of the frustum corners. */
	union vec3 center;
	vec3_init(&center, 0, 0, 0);
	for (i = 0; i < 8; i++)
		vec3_add_self(&center, &corners[i]);
	vec3_mul_self(&center, 1.0f / 8.0f);

	/* Light direction: from the configured light source (cx->light, the sun at the world
	 * origin in normal play) toward a single shared reference point -- the camera position --
	 * so every cascade uses the SAME direction.  The sun is a finite-distance point light, so
	 * taking the direction to each cascade's own center instead would give the cascades
	 * slightly different light angles and project the same shadow to different places, an
	 * offset that is visible off the light axis and vanishes when the cascade centers line up
	 * along the sun ray.  A directional-light approximation must hold the direction constant. */
	union vec3 light_pos, ldir, ref;
	vec3_init(&light_pos, cx->light.m[0], cx->light.m[1], cx->light.m[2]);
	vec3_init(&ref, cam->x, cam->y, cam->z);
	vec3_sub(&ldir, &ref, &light_pos);
	if (vec3_dot(&ldir, &ldir) < 1e-6)
		return 0;
	vec3_normalize_self(&ldir);

	/* Light view basis (mirrors the camera look-at construction in
	 * calculate_camera_transform_near_far()). */
	union vec3 lup, lu, lv;
	vec3_init(&lup, 0, 1, 0);
	if (fabsf(vec3_dot(&ldir, &lup)) > 0.99f)
		vec3_init(&lup, 1, 0, 0);
	vec3_cross(&lu, &ldir, &lup);
	vec3_normalize_self(&lu);
	vec3_cross(&lv, &lu, &ldir);
	vec3_normalize_self(&lv);

	/* Fit a bounding sphere to the frustum corners instead of an AABB: because the camera
	 * frustum is rigid, the sphere's radius is invariant to camera rotation, so the ortho
	 * window keeps a fixed size and its texels do not resize/swim as the view turns (which is
	 * what made the two cascades disagree off-axis). */
	double radius = 0.0;
	for (i = 0; i < 8; i++) {
		union vec3 d;
		double rd;

		vec3_sub(&d, &corners[i], &center);
		rd = vec3_magnitude(&d);
		if (rd > radius)
			radius = rd;
	}
	if (radius < 1e-6)
		return 0;

	/* Snap the window centre to whole-texel increments along the light's right/up axes, on a
	 * grid anchored at the world origin, so a given world point always lands on the same texel
	 * -- eliminating the sub-texel drift between cascades and the shimmer as the camera moves.
	 * Do the snap in the light basis, then reconstruct the snapped centre in world space so the
	 * light-view translation keeps the math camera-relative (small coordinates) for precision. */
	union vec3 neg_ldir;
	vec3_mul(&neg_ldir, &ldir, -1.0);
	double texel = 2.0 * radius / (double) ENTITY_SHADOW_MAP_TEXTURE_SIZE;
	double cx_l = vec3_dot(&center, &lu);
	double cy_l = vec3_dot(&center, &lv);
	double cz_l = vec3_dot(&center, &neg_ldir);
	cx_l = round(cx_l / texel) * texel;
	cy_l = round(cy_l / texel) * texel;

	union vec3 snapped_center, tmp;
	vec3_mul(&snapped_center, &lu, cx_l);
	vec3_mul(&tmp, &lv, cy_l);
	vec3_add_self(&snapped_center, &tmp);
	vec3_mul(&tmp, &neg_ldir, cz_l);
	vec3_add_self(&snapped_center, &tmp);

	/* World -> light view: rotation (rows lu, lv, -ldir) composed with translate(-snapped). */
	struct mat44d lrot = {{
		{ lu.v.x, lv.v.x, -ldir.v.x, 0 },
		{ lu.v.y, lv.v.y, -ldir.v.y, 0 },
		{ lu.v.z, lv.v.z, -ldir.v.z, 0 },
		{ 0, 0, 0, 1 } } };
	struct mat44d ltrans = {{
		{ 1, 0, 0, 0 },
		{ 0, 1, 0, 0 },
		{ 0, 0, 1, 0 },
		{ -snapped_center.v.x, -snapped_center.v.y, -snapped_center.v.z, 1 } } };
	struct mat44d lview;
	mat44_product_ddd(&lrot, &ltrans, &lview);

	/* z extent of the corners in this light view (x/y come from the fixed-size sphere). */
	double zmin = 1e30, zmax = -1e30;
	for (i = 0; i < 8; i++) {
		union vec4 cw;
		union vec3 cv;
		cw.v.x = corners[i].v.x;
		cw.v.y = corners[i].v.y;
		cw.v.z = corners[i].v.z;
		cw.v.w = 1.0;
		mat44_x_vec4_into_vec3_dff(&lview, &cw, &cv);
		if (cv.v.z < zmin)
			zmin = cv.v.z;
		if (cv.v.z > zmax)
			zmax = cv.v.z;
	}

	/* Forward is -ldir, so points in front of the light have negative view z.  Convert to
	 * positive ortho near/far distances.
	 *
	 * The near plane has to reach from this cascade's region back toward the light far enough
	 * to catch every caster that could shadow it, and casters are routinely much further away
	 * than the region is deep -- a starbase or a large asteroid hundreds of units off.  Padding
	 * by the region's own depth, as this used to, tied the maximum caster distance to the
	 * cascade's size, so making the near cascades smaller (to win back near-field resolution)
	 * silently stopped distant occluders casting into them.  Pad by the shadow coverage
	 * distance instead: that is the documented reach of the whole shadow system, and it does
	 * not move when the split scheme changes.
	 *
	 * The far plane needs no such padding.  A caster further from the light than every point of
	 * the region cannot occlude any of them, and every visible receiver lies inside the frustum
	 * slice and so within the corners' own z extent; a small margin for interpolation error is
	 * enough.
	 *
	 * The cost is shadow-map depth precision: the range is now about the coverage distance
	 * rather than a few cascade depths, which over a 24-bit depth buffer still resolves to
	 * ~3e-4 world units at the default 4500 -- far below the smallest cascade's texel.  The x/y
	 * window is the (snapped) sphere, one texel larger each side to absorb the snap offset. */
	double zrange = zmax - zmin;
	if (zrange < 1e-6)
		return 0;
	double ortho_near = -zmax - shadow_map_max_distance;
	double ortho_far = -zmin + 0.01 * zrange;
	double l = -radius - texel;
	double r = radius + texel;
	double b = -radius - texel;
	double t = radius + texel;
	if (ortho_far - ortho_near < 1e-6)
		return 0;

	struct mat44d ortho = {{ { 0 } } };
	ortho.m[0][0] = 2.0 / (r - l);
	ortho.m[1][1] = 2.0 / (t - b);
	ortho.m[2][2] = -2.0 / (ortho_far - ortho_near);
	ortho.m[3][0] = -(r + l) / (r - l);
	ortho.m[3][1] = -(t + b) / (t - b);
	ortho.m[3][2] = -(ortho_far + ortho_near) / (ortho_far - ortho_near);
	ortho.m[3][3] = 1.0;

	mat44_product_ddd(&ortho, &lview, world_to_lightclip);
	return 1;
}

/* Compute the cascade split distances (the far edge of each cascade) using the practical
 * split scheme: a blend of logarithmic and uniform spacing over [near, far]. */
static void compute_cascade_splits(float near_d, float far_d, int num_cascades, float *split_far)
{
	int i;

	for (i = 0; i < num_cascades; i++) {
		float p = (float) (i + 1) / (float) num_cascades;
		float log_split = near_d * powf(far_d / near_d, p);
		float uni_split = near_d + (far_d - near_d) * p;
		split_far[i] = shadow_map_split_lambda * log_split +
				(1.0f - shadow_map_split_lambda) * uni_split;
	}
}

/* Only opaque, lit surfaces belong in the shadow map.  The depth pass ignores materials, so
 * anything drawn into it casts a fully opaque shadow.  Emissive or blended geometry -- thrust
 * flares and plumes, sparks, nebulae, wormholes, the warp tunnel, the sun billboard -- is not
 * an occluder, and most of it is a camera-facing billboard sitting right against the hull it
 * is attached to, so casting from it drops a solid slab of shadow across nearby ships and
 * starbases.  Entities with no material use the default lit shader and do cast. */
static int entity_casts_shadow(struct entity *e)
{
	struct material *m = e->material_ptr;

	if (e->no_cast_shadow)
		return 0;
	if (!m)
		return 1;
	if (m->billboard_type != MATERIAL_BILLBOARD_TYPE_NONE)
		return 0;
	switch (m->type) {
	case MATERIAL_COLOR:
	case MATERIAL_TEXTURE_MAPPED:
	case MATERIAL_TEXTURE_CUBEMAP:
		return 1;
	default:
		return 0;
	}
}

/* Render all triangle-mesh shadow casters into each cascade of the shadow map. */
static void render_shadow_map(struct entity_context *cx)
{
	struct mat44d cascade_w2l[MAX_ENTITY_SHADOW_CASCADES];
	float split_far[MAX_ENTITY_SHADOW_CASCADES];
	int num_cascades, valid_cascades;
	int j, k, n;

	if (!graph_dev_shadow_map_enabled)
		return;

	num_cascades = shadow_map_num_cascades;
	if (num_cascades > MAX_ENTITY_SHADOW_CASCADES)
		num_cascades = MAX_ENTITY_SHADOW_CASCADES;
	if (num_cascades < 1)
		num_cascades = 1;

	float near_d = cx->camera.near;
	float far_d = cx->camera.far;
	if (far_d > shadow_map_max_distance)
		far_d = shadow_map_max_distance;
	if (far_d <= near_d)
		return;

	compute_cascade_splits(near_d, far_d, num_cascades, split_far);

	/* Build a light matrix for each cascade slice.  A degenerate slice aborts shadows. */
	float slice_near = near_d;
	for (k = 0; k < num_cascades; k++) {
		if (!compute_shadow_light_matrix(cx, slice_near, split_far[k], &cascade_w2l[k]))
			return;
		slice_near = split_far[k];
	}
	valid_cascades = num_cascades;

	graph_dev_set_shadow_cascades(cascade_w2l, valid_cascades);
	graph_dev_set_shadow_cascade_splits(split_far, valid_cascades);
	graph_dev_shadow_map_begin();

	for (k = 0; k < valid_cascades; k++) {
		graph_dev_shadow_map_set_cascade(k);
		n = snis_object_pool_highest_object(cx->entity_pool);
		for (j = 0; j <= n; j++) {
			if (!snis_object_pool_is_allocated(cx->entity_pool, j))
				continue;
			struct entity *e = &cx->entity_list[j];
			if (e->m == NULL || e->m->geometry_mode != MESH_GEOMETRY_TRIANGLES)
				continue;
			if (!entity_casts_shadow(e))
				continue;
			update_entity_child_state(e);
			if (!e->visible)
				continue;
			struct entity_transform transform;
			calculate_model_matrices(&cx->camera, &cx->camera.frustum, e, &transform);
			graph_dev_draw_shadow_caster(&transform.m, e->m);
		}
	}

	graph_dev_shadow_map_end();
}

void render_entities(struct entity_context *cx)
{
	int j, k, n;
	struct camera_info *c = &cx->camera;

	sng_set_3d_viewport(cx->window_offset_x, cx->window_offset_y, c->xvpixels, c->yvpixels);

	calculate_camera_transform(cx);

	/* Render the shadow map before drawing the scene so lit shaders can sample it. */
	render_shadow_map(cx);

#if SPACEDUST
	/* see if the space dust have wandered outside of our immediate area */
	if (cx->ndust_motes > 0) {
		float r2 = cx->space_dust_radius * cx->space_dust_radius;
		for (int i = 0; i < cx->ndust_motes; i++) {
			struct vertex *fs = &cx->space_dust_mesh->v[i];

			float dist2 = dist3dsqrd(c->x - fs->x, c->y - fs->y, c->z - fs->z);

			if (dist2 > r2)
				reposition_space_dust(cx, fs, cx->space_dust_radius);
		}
		mesh_graph_dev_init(cx->space_dust_mesh);
	}
#endif

#if MOVING_STARFIELD
	/* The star field recycles on both faces, unlike the space dust above, which only
	 * recycles stars that have fallen behind.  Recycling on the NEAR face is what enforces
	 * the distance floor: without it a star ahead closes for as long as you fly at it and
	 * ends up sweeping past however far out the field began. */
	if (cx->nstar_field > 0) {
		float inner2 = cx->star_field_radius * cx->star_field_radius;
		float outer = cx->star_field_radius * STAR_FIELD_OUTER;
		float outer2 = outer * outer;

		for (int i = 0; i < cx->nstar_field; i++) {
			struct vertex *fs = &cx->star_field_mesh->v[i];

			float dist2 = dist3dsqrd(c->x - fs->x, c->y - fs->y, c->z - fs->z);

			if (dist2 > outer2 || dist2 < inner2)
				reposition_star_field_star(cx, fs, cx->star_field_radius, 0);
		}
		mesh_graph_dev_init(cx->star_field_mesh);
		cx->star_field_last_camera.v.x = c->x;
		cx->star_field_last_camera.v.y = c->y;
		cx->star_field_last_camera.v.z = c->z;
		cx->star_field_have_last_camera = 1;
	}
#endif

	/* For better depth buffer precision do the draw in multiple (two) passes based on the
	 * dynamic range of near/far, and clear the depth buffer between passes. A good rule of
	 * thumb is to have far / near < 10000 on 24-bit depth buffer.  At the boundary between
	 * passes a slight seam can become visible. To avoid this visible seam we should try to
	 * avoid having the boundary between passes intersect any large objects.  To this end we
	 * have 3 reasonable candidate boundary distances and choose the boundary distance which
	 * intersects the fewest large objects.
	 */
	float boundary_candidate[3] = { /* distance candidates for the boundary between passes */
		cx->camera.near * 4000.0,
		cx->camera.near * 7000.0,
		cx->camera.near * 10000.0,
	};
	int boundary_intersects[3] = { 0, 0, 0 }; /* Count of objects intersecting each boundary distance plane */
	int last_candidate, candidate = 0;
	float render_pass_boundary = boundary_candidate[0];
	union vec3 camera_pos, camera_look, camera_to_entity;
	int n_passes;
	struct frustum rendering_pass[2];

	camera_pos.v.x = cx->camera.x;
	camera_pos.v.y = cx->camera.y;
	camera_pos.v.z = cx->camera.z;
	camera_look.v.x = cx->camera.lx - cx->camera.x;
	camera_look.v.y = cx->camera.ly - cx->camera.y;
	camera_look.v.z = cx->camera.lz - cx->camera.z;
	vec3_normalize_self(&camera_look);

	if (cx->camera.far / cx->camera.near < render_pass_boundary) {
		n_passes = 1;
		rendering_pass[0] = c->frustum;
	} else {
		n_passes = 2;
		calculate_camera_transform_near_far(&cx->camera, &rendering_pass[0],
							render_pass_boundary, cx->camera.far);
		calculate_camera_transform_near_far(&cx->camera, &rendering_pass[1],
			cx->camera.near, render_pass_boundary + render_pass_boundary / 1000.0);
			/* the additional 0.1% is to render a little farther to cover seam */
	}

#if MOVING_STARFIELD
	/* The star field, drawn once here rather than as an ordinary entity.
	 *
	 * Its mesh radius is INT_MAX, since the stars surround the camera and no bounding
	 * sphere describes them, so sphere_in_frustum() puts it in BOTH render passes: every
	 * star would be drawn twice, and the near pass draws after the depth buffer is cleared,
	 * so the second copy would land on top of everything the far pass had drawn -- the sun
	 * included.  Sorting cannot fix that; the field is not at a depth.
	 *
	 * So draw it once, first, with the depth test off entirely.  These are meant to read as
	 * distant stars whatever their geometry says, and the geometry only ever says anything
	 * because parallax needs a finite distance to work with.  Everything else draws over
	 * them afterwards, which is what being behind everything means.
	 *
	 * The space dust is deliberately NOT drawn this way: it is meant to be among the scene,
	 * occluded by what it passes behind, so it stays on the ordinary entity path. */
	if (cx->star_field && cx->nstar_field > 0 && cx->star_field_mesh) {
		struct entity_transform st;
		struct mat44d id = { { { 1, 0, 0, 0 }, { 0, 1, 0, 0 },
					{ 0, 0, 1, 0 }, { 0, 0, 0, 1 } } };

		st.m = id;
		st.vp = &c->frustum.vp_matrix;
		mat44_product_ddf(st.vp, &st.m, &st.mvp);
		graph_dev_draw_star_field(cx, &st.mvp);
	}
#endif

	int pass;
	for (pass = 0; pass < n_passes; pass++) {

		struct frustum *f = &rendering_pass[pass];

		/* find all entities in view frustum and sort them by distance */
		n = snis_object_pool_highest_object(cx->entity_pool);

		cx->nnear_to_far_entity_depth = 0;
		cx->nfar_to_near_entity_depth = 0;

		if (pass == 0) {
			boundary_intersects[0] = 0;
			boundary_intersects[1] = 0;
			boundary_intersects[2] = 0;
		}

		for (j = 0; j <= n; j++) {
			if (!snis_object_pool_is_allocated(cx->entity_pool, j))
				continue;

			struct entity *e = &cx->entity_list[j];

			if (e->m == NULL)
				continue;

#if MOVING_STARFIELD
			/* Already drawn above, before the passes and without depth. */
			if (e == cx->star_field)
				continue;
#endif

			/* clear on the first pass and accumulate the state */
			if (pass == 0) {
				update_entity_child_state(e);
				e->onscreen = 0;
			}

			if (!e->visible)
				continue;


			float max_scale = vec3_cwise_max(&e->scale);

			if (!sphere_in_frustum(f, e->x, e->y, e->z, e->m->radius * max_scale))
				continue;

			e->dist3dsqrd = dist3dsqrd(c->x - e->x, c->y - e->y, c->z - e->z);

			/* See: http://stackoverflow.com/questions/3717226/radius-of-projected-sphere */
			float approx_pixel_size = c->yvpixels * e->m->radius * max_scale /
				tan(cx->camera.angle_of_view * 0.5) / sqrt(e->dist3dsqrd);

			/* only cull stuff entirely that is too small on flat shader */
			if (c->renderer & FLATSHADING_RENDERER && e->dist3dsqrd != 0 && approx_pixel_size < 2.0)
				continue;

			/* Choose low or high poly mesh based on on-screen size */
			if (approx_pixel_size < cx->hi_lo_poly_pixel_threshold)
				e->m = e->low_poly;
			else
				e->m = e->high_poly;

			int render_order = graph_dev_entity_render_order(e);
			switch (render_order) {
				case GRAPH_DEV_RENDER_FAR_TO_NEAR:
					cx->far_to_near_entity_depth[cx->nfar_to_near_entity_depth] = j;
					cx->nfar_to_near_entity_depth++;
					break;
				case GRAPH_DEV_RENDER_NEAR_TO_FAR:
					cx->near_to_far_entity_depth[cx->nnear_to_far_entity_depth] = j;
					cx->nnear_to_far_entity_depth++;
					break;
			}

			/* For each boundary candidate, count large object intersections */
			if (pass == 0 && n_passes > 1) {
				if (e->m->radius * max_scale < 500)
					continue; /* ignore small objects in choosing render pass boundary */
				camera_to_entity.v.x = e->x - camera_pos.v.x;
				camera_to_entity.v.y = e->y - camera_pos.v.y;
				camera_to_entity.v.z = e->z - camera_pos.v.z;

				float zdist = vec3_dot(&camera_to_entity, &camera_look);
				if (zdist < 0)
					continue; /* behind camera, ignore (should already be frustum culled). */
				if (e->m->geometry_mode == MESH_GEOMETRY_POINTS) /* Ignore space dust, etc. */
					continue;
				if (e->m->radius * max_scale >= 299999.0)
					continue; /* Hack to ignore warp tunnels (r=300000) */

				/* Check each candidate boundary for intersection with this object */
				for (k = 0; k < 3; k++)
					if (fabsf(zdist - boundary_candidate[k]) < e->m->radius * max_scale)
						boundary_intersects[k]++;
			}
		}

		if (pass == 0 && n_passes > 1) {
			/* Find the boundary candidate with the fewest large object intersections */
			last_candidate = candidate;
			candidate = 0;
			if (boundary_intersects[candidate] != 0) {
				/* Test for less than or *equal* so we choose the *farthest* among equals so
				 * that any resulting artifacts are more likely to occupy less screen space
				 * and be less obnoxious. With a little luck, it will even move it to a place
				 * that is invisible, like the back side of a planet.
				 */
				if (boundary_intersects[1] <= boundary_intersects[candidate])
					candidate = 1;
				if (boundary_intersects[2] <= boundary_intersects[candidate])
					candidate = 2;
			}

			if (last_candidate != candidate) {
				/* Recalculate the camera transforms if the render pass boundary was changed
				   to avoid some intersecting object */
				render_pass_boundary = boundary_candidate[candidate];
				calculate_camera_transform_near_far(&cx->camera, &rendering_pass[0],
								render_pass_boundary, cx->camera.far);
				calculate_camera_transform_near_far(&cx->camera, &rendering_pass[1],
					cx->camera.near, render_pass_boundary + render_pass_boundary / 1000.0);
			}
		}

		if (cx->nfar_to_near_entity_depth > 0 || cx->nnear_to_far_entity_depth > 0) {
			/* need to reset the depth buffer between passes */
			if (n_passes > 1 && pass != 0)
				graph_dev_clear_depth_bit();

			sort_entity_distances(cx);

			/* find the position of our light in camera space */
			struct mat41 camera_light_pos;
			mat44_x_mat41_dff(&f->v_matrix, &cx->light, &camera_light_pos);

			/* render the sorted entities */

			/* near to far first, usually opaque geometry */
			for (j = 0; j < cx->nnear_to_far_entity_depth; j++) {
				struct entity *e = &cx->entity_list[cx->near_to_far_entity_depth[j]];
				if (e->render_style & RENDER_SHADOWS_ONLY)
					continue; /* don't render objects that exist only to cast shadows */
				if (e)
					render_entity(cx, f, e, (union vec3 *)&camera_light_pos.m[0]);
			}

			/* then far to near, usually blended geometry and software renderer */
			for (j = 0; j < cx->nfar_to_near_entity_depth; j++) {
				struct entity *e = &cx->entity_list[cx->far_to_near_entity_depth[j]];
				if (e->render_style & RENDER_SHADOWS_ONLY)
					continue; /* don't render objects that exist only to cast shadows */
				if (e)
					render_entity(cx, f, e, (union vec3 *)&camera_light_pos.m[0]);
			}
		}
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

struct entity_context *entity_context_new(int maxobjs, int maxchildren)
{
	struct entity_context *cx;

	cx = malloc(sizeof(*cx));

	memset(cx, 0, sizeof(*cx));
	cx->entity_list = malloc(sizeof(cx->entity_list[0]) * maxobjs);
	memset(cx->entity_list, 0, sizeof(cx->entity_list[0]) * maxobjs);
	cx->far_to_near_entity_depth = malloc(sizeof(cx->far_to_near_entity_depth[0]) * maxobjs);
	memset(cx->far_to_near_entity_depth, 0, sizeof(cx->far_to_near_entity_depth[0]) * maxobjs);
	cx->near_to_far_entity_depth = malloc(sizeof(cx->near_to_far_entity_depth[0]) * maxobjs);
	memset(cx->near_to_far_entity_depth, 0, sizeof(cx->near_to_far_entity_depth[0]) * maxobjs);
	snis_object_pool_setup(&cx->entity_pool, maxobjs);
	cx->maxobjs = maxobjs;
	cx->entity_child_list = malloc(sizeof(cx->entity_child_list[0]) * maxchildren);
	memset(cx->entity_child_list, 0, sizeof(cx->entity_child_list[0]) * maxchildren);
	snis_object_pool_setup(&cx->entity_child_pool, maxchildren);
	cx->maxchildren = maxchildren;
	set_renderer(cx, FLATSHADING_RENDERER);
	set_lighting(cx, 0, 0, 0);
	camera_assign_up_direction(cx, 0.0, 1.0, 0.0);
	set_window_offset(cx, 0.0, 0.0);
#if SPACEDUST
	cx->ndust_motes = 0;
#endif

#if MOVING_STARFIELD
	cx->star_field = 0;
	cx->star_field_mesh = 0;
	cx->nstar_field = 0;
	cx->star_field_radius = 0.0;
	cx->star_field_have_last_camera = 0;
#endif

	cx->hi_lo_poly_pixel_threshold = 100.0;
	cx->star_color[0] = 1.0;
	cx->star_color[1] = 1.0;
	cx->star_color[2] = 1.0;
	cx->star_light_tint = 0.0;
	cx->star_dark_tint = 0.0;
	cx->star_shadow_darkening = 0.0;
	return cx;
}

void entity_context_free(struct entity_context *cx)
{
	free(cx->entity_list);
	free(cx->far_to_near_entity_depth);
	free(cx->near_to_far_entity_depth);
	free(cx);
}

#if SPACEDUST
/* Re-position a star randomly on the surface of sphere of given radius */
static void reposition_space_dust(struct entity_context *cx, struct vertex *fs, float radius)
{
	/* I tried "on" sphere here, but I like the look of "in" better. */
	float dist3dsqrd;
	random_point_in_sphere(radius, &fs->x, &fs->y, &fs->z, &dist3dsqrd);
	fs->x += cx->camera.x;
	fs->y += cx->camera.y;
	fs->z += cx->camera.z;
}

/* fill a sphere of specified radius with randomly placed stars */
void entity_init_space_dust(struct entity_context *cx, int ndust_motes, float radius)
{
	int i;
	if (cx->ndust_motes > 0)
		entity_free_space_dust(cx);

	cx->ndust_motes = ndust_motes;
	if (ndust_motes == 0)
		return;
	cx->space_dust_radius = radius;

	cx->space_dust_mesh = calloc(1, sizeof(*cx->space_dust_mesh));
	cx->space_dust_mesh->geometry_mode = MESH_GEOMETRY_POINTS;
	cx->space_dust_mesh->nvertices = ndust_motes;
	cx->space_dust_mesh->v = calloc(1, sizeof(*cx->space_dust_mesh->v) * ndust_motes);
	/* no good way to calculate this as the origin is always 0,0 and the stars move
	   in space relative to the camera */
	cx->space_dust_mesh->radius = INT_MAX;

	for (i = 0; i < ndust_motes; i++) {
		reposition_space_dust(cx, &cx->space_dust_mesh->v[i], radius);
	}
	mesh_graph_dev_init(cx->space_dust_mesh);

	cx->space_dust_mesh->material = 0;

	cx->space_dust = add_entity(cx, cx->space_dust_mesh, 0, 0, 0, GRAY75);
}

void entity_free_space_dust(struct entity_context *cx)
{
	cx->ndust_motes = 0;
	if (cx->space_dust)
		remove_entity(cx, cx->space_dust);
	cx->space_dust = 0;
	mesh_free(cx->space_dust_mesh);
	cx->space_dust_mesh = 0;
}
#endif /* SPACEDUST */

#if MOVING_STARFIELD
/* Where a recycled star is put back: hard against the outer face, where the far fade is
 * still zero, so it creeps into view rather than appearing part-lit. */
#define STAR_FIELD_RECYCLE_OUTER 0.97

/* Put a star on the shell, biased toward the direction the camera is travelling.
 *
 * A star leaves the shell by falling behind, so putting it back in a uniformly random
 * direction lets it reappear beside or ahead of the camera at the near face, popping into
 * view.  Weighting the placement toward the leading hemisphere puts it back where parallax
 * would actually bring one in: far ahead, drifting backward.  The weighting is soft rather
 * than a hard restriction to that hemisphere, which would drain a visible hole astern.
 *
 * With no motion recorded yet there is no leading direction, so fall back to uniform. */
static void reposition_star_field_star(struct entity_context *cx, struct vertex *fs, float radius,
					int initial)
{
	float dist3dsqrd, r, len, u;
	union vec3 dir, travel;

	random_point_in_sphere(radius, &dir.v.x, &dir.v.y, &dir.v.z, &dist3dsqrd);
	len = sqrtf(dist3dsqrd);
	if (len < 1e-6) {
		dir.v.x = 1.0;
		dir.v.y = 0.0;
		dir.v.z = 0.0;
	} else {
		vec3_mul_self(&dir, 1.0 / len);
	}

	if (!initial && cx->star_field_have_last_camera) {
		travel.v.x = cx->camera.x - cx->star_field_last_camera.v.x;
		travel.v.y = cx->camera.y - cx->star_field_last_camera.v.y;
		travel.v.z = cx->camera.z - cx->star_field_last_camera.v.z;
		if (vec3_magnitude(&travel) > 1e-6) {
			vec3_normalize_self(&travel);
			vec3_mul_self(&travel, 0.6);
			vec3_add_self(&dir, &travel);
			vec3_normalize_self(&dir);
		}
	}

	/* Filled evenly over the shell's depth so the sky is populated the moment it is
	 * switched on; recycled hard against the outer face so it fades in. */
	u = fabsf(snis_random_float());
	if (initial)
		r = radius * (1.0 + (STAR_FIELD_OUTER - 1.0) * u);
	else
		r = radius * STAR_FIELD_OUTER * (STAR_FIELD_RECYCLE_OUTER +
						(1.0 - STAR_FIELD_RECYCLE_OUTER) * u);
	fs->x = cx->camera.x + dir.v.x * r;
	fs->y = cx->camera.y + dir.v.y * r;
	fs->z = cx->camera.z + dir.v.z * r;
}

/* Fill a shell around the camera with stars.  radius is the nearest they may come; see the
 * note on STAR_FIELD_OUTER. */
void entity_init_star_field(struct entity_context *cx, int nstars, float radius)
{
	int i;

	if (cx->nstar_field > 0)
		entity_free_star_field(cx);

	cx->nstar_field = nstars;
	if (nstars == 0)
		return;
	cx->star_field_radius = radius;

	cx->star_field_mesh = calloc(1, sizeof(*cx->star_field_mesh));
	cx->star_field_mesh->geometry_mode = MESH_GEOMETRY_POINTS;
	cx->star_field_mesh->nvertices = nstars;
	cx->star_field_mesh->v = calloc(1, sizeof(*cx->star_field_mesh->v) * nstars);
	/* No good way to calculate this: the stars surround the camera and move with it, so
	   no bounding sphere describes them.  Nothing sorts on it -- the field is drawn
	   before the passes -- but the entity code expects a radius. */
	cx->star_field_mesh->radius = INT_MAX;

	for (i = 0; i < nstars; i++)
		reposition_star_field_star(cx, &cx->star_field_mesh->v[i], radius, 1);
	mesh_graph_dev_init(cx->star_field_mesh);

	cx->star_field_mesh->material = 0;

	cx->star_field = add_entity(cx, cx->star_field_mesh, 0, 0, 0, GRAY75);
}

/* Put the star field entity back after a remove_all_entity(), reusing the existing mesh so
 * the stars keep the world positions they had -- rebuilding would re-roll the whole sky, and
 * a sky that changes every time the scene is rebuilt is not a sky.  A no-op if the field is
 * off, was never built, or is already present. */
void entity_readd_star_field(struct entity_context *cx)
{
	if (cx->nstar_field <= 0 || !cx->star_field_mesh || cx->star_field)
		return;
	cx->star_field = add_entity(cx, cx->star_field_mesh, 0, 0, 0, GRAY75);
}

void entity_free_star_field(struct entity_context *cx)
{
	cx->nstar_field = 0;
	cx->star_field_have_last_camera = 0;
	if (cx->star_field)
		remove_entity(cx, cx->star_field);
	cx->star_field = 0;
	mesh_free(cx->star_field_mesh);
	cx->star_field_mesh = 0;
}
#endif /* MOVING_STARFIELD */

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

void set_ambient_light(struct entity_context *cx, float ambient)
{
	cx->ambient = ambient;
}

/* Star-coloured lighting: tint the direct (sun-lit) term of lit objects toward the star's
 * colour (r, g, b) by light_tint, and their ambient/shaded term toward the star's complement
 * by dark_tint, and darkening it by shadow_darkening (see star_light.c).  All three
 * strengths at 0 restore the untinted look.  The renderer derives the actual light/ambient
 * colours from these plus cx->ambient at draw time, so callers may set the ambient level and
 * this tint in either order. */
void set_star_light_tint(struct entity_context *cx, float r, float g, float b,
			float light_tint, float dark_tint, float shadow_darkening)
{
	cx->star_color[0] = r;
	cx->star_color[1] = g;
	cx->star_color[2] = b;
	cx->star_light_tint = light_tint;
	cx->star_dark_tint = dark_tint;
	cx->star_shadow_darkening = shadow_darkening;
}

/* Cascaded-shadow-map coverage tunables (global, not per-context).  Exposed so tools like
 * shadow_lab (and eventually the client's tweakable-variables table) can adjust the
 * near-crisp / far-coarse tradeoff at runtime. */
void set_shadow_map_max_distance(float distance)
{
	if (distance > 0.0f)
		shadow_map_max_distance = distance;
}

float get_shadow_map_max_distance(void)
{
	return shadow_map_max_distance;
}

void set_shadow_map_num_cascades(int n)
{
	if (n < 1)
		n = 1;
	if (n > MAX_ENTITY_SHADOW_CASCADES)
		n = MAX_ENTITY_SHADOW_CASCADES;
	shadow_map_num_cascades = n;
}

int get_shadow_map_num_cascades(void)
{
	return shadow_map_num_cascades;
}

void set_shadow_map_split_lambda(float lambda)
{
	if (lambda < 0.0f)
		lambda = 0.0f;
	if (lambda > 1.0f)
		lambda = 1.0f;
	shadow_map_split_lambda = lambda;
}

float get_shadow_map_split_lambda(void)
{
	return shadow_map_split_lambda;
}

struct mesh *entity_get_mesh(struct entity *e)
{
	return e->m;
}

void entity_set_mesh(struct entity *e, struct mesh *m)
{
	e->m = m;
}

struct mesh *entity_get_low_poly_mesh(struct entity *e)
{
	return e->low_poly;
}

void entity_set_low_poly_mesh(struct entity *e, struct mesh *m)
{
	e->low_poly = m;
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
	return &e->e_orientation;
}

void entity_get_pos(struct entity *e, float *x, float *y, float *z)
{
	*x = e->x;
	*y = e->y;
	*z = e->z;
}

struct mat44d get_camera_v_transform(struct entity_context *cx)
{
	return cx->camera.frustum.v_matrix;
}

struct mat44d get_camera_vp_transform(struct entity_context *cx)
{
	return cx->camera.frustum.vp_matrix;
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

float entity_get_alpha(struct entity *e)
{
	return e->alpha;
}

void entity_update_alpha(struct entity *e, float alpha)
{
	e->alpha = alpha;
}

void entity_update_emit_intensity(struct entity *e, float intensity)
{
	e->emit_intensity = intensity;
}

float entity_get_emit_intensity(struct entity *e)
{
	return e->emit_intensity;
}

float entity_get_in_shade(struct entity *e)
{
	return e->in_shade;
}

void entity_set_in_shade(struct entity *e, float in_shade)
{
	e->in_shade = in_shade;
}

void entity_context_set_hi_lo_poly_pixel_threshold(struct entity_context *cx, float pixel_threshold)
{
	cx->hi_lo_poly_pixel_threshold = pixel_threshold;
}

