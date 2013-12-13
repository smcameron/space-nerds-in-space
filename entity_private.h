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
	float x, y, z; /* world coords */
	float cx, cy, cz; /* camera coords */
	unsigned char onscreen; /* if screen coords are valid */
	float sx, sy; /* screen coords */
	float scale;
	float dist3dsqrd;
	int color;
	int shadecolor;
	int render_style;
	void *user_data;
	union quat orientation;
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
	struct mat44d camera_v_matrix;
	struct mat44d camera_vp_matrix;
	float frustum[6][4];
};

struct entity_context {
	int maxobjs;
	struct snis_object_pool *entity_pool;
	struct entity *entity_list; /* array, [maxobjs] */
	int nentity_depth;
	int *entity_depth; /* array [maxobjs] */
	struct camera_info camera;
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

#endif

