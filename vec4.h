/*
	Copyright (C) 2010 Jeremy Van Grinsven
	Author: Jeremy Van Grinsven

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

#ifndef __VEC4_H__
#define __VEC4_H__

union vec3;

union vec4 {
	struct {
		float x;
		float y;
		float z;
		float w;
	} v;
	float vec[4];
};
#define VEC4_INITIALIZER { { 0.0, 0.0, 0.0, 1.0 } }

/* initialize a vec4 from a vec3 plus w component */
extern union vec4 *vec4_init_vec3(union vec4 *vo, const union vec3 *vi, float w);

extern union vec3 *vec4_to_vec3(const union vec4 *vi, union vec3 *vo);

extern void vec4_print(const char *prefix, const union vec4 *v);

#endif

