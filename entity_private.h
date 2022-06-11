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
#ifndef ENTITY_PRIVATE_H__
#define ENTITY_PRIVATE_H__

struct entity {
	struct mesh *m;
	struct mesh *low_poly;
	struct mesh *high_poly;
	int visible;
	float x, y, z; /* world coords */
	float cx, cy, cz; /* camera coords */
	float sx, sy; /* screen coords */
	union vec3 scale;
	float dist3dsqrd;
	float emit_intensity; /* Range 0.0 - 1.0. Multiplied with material emit intensity. */
	int color;
	int shadecolor;
	float alpha;
	int render_style;
	void *user_data;
	union quat orientation;

	struct material *material_ptr;

	union vec3 e_pos;
	union vec3 e_scale;
	union quat e_orientation;
	struct entity *parent;
	int entity_child_index;
	float in_shade;
	int e_visible;
	unsigned char onscreen; /* if screen coords are valid */
};

struct entity_child {
	int child_entity_index;
	int next_entity_child_index;
};

struct frustum {
	float near, far;
	float top, bottom, left, right;
	struct mat44d v_matrix;
	struct mat44d vp_matrix;
	struct mat44d p_matrix;
	struct mat44 vp_matrix_no_translate;
	float planes[6][4];
};

struct camera_info {
	float x, y, z;		/* position of camera */
	float lx, ly, lz;	/* where camera is looking */
	float ux, uy, uz;	/* up vector */
	float near, far;
	float angle_of_view;
	int xvpixels, yvpixels;
	struct frustum frustum;
	int renderer;
};

struct entity_context {
	struct snis_object_pool *entity_pool;
	struct entity *entity_list; /* array, [maxobjs] */
	int maxobjs;
	int maxchildren;
	struct snis_object_pool *entity_child_pool;
	struct entity_child *entity_child_list; /* array, [maxchildren] */
	int nfar_to_near_entity_depth;
	int nnear_to_far_entity_depth;
	int *far_to_near_entity_depth; /* array [maxobjs] */
	int *near_to_far_entity_depth; /* array [maxobjs] */
	struct camera_info camera;
	struct entity *fake_stars;
	struct mesh *fake_stars_mesh;
	int nfakestars; /* = 0; */
	float fakestars_radius;
	struct mat41 light;
	float window_offset_x, window_offset_y;
	float ambient;

	/* Screen size of model at which we switch between lo/hi poly models, if available */
	/* Possibly this threshold should be per mesh, rather than per entity context. */
	float hi_lo_poly_pixel_threshold;

#ifdef WITH_ILDA_SUPPORT
	int framenumber;
	FILE *f;
#endif
};

struct entity_transform {
	struct mat44d m_no_scale;
	struct mat44d m;
	struct mat44 mvp;
	struct mat44 mv;
	struct mat44d *vp;
	struct mat44d *v;
	struct mat33 normal;
};

#endif

