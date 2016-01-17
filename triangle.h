#ifndef TRIANGLE_H__
#define TRIANGLE_H__
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

#define TRIANGLE_0_SHARED (1<<1)
#define TRIANGLE_1_SHARED (1<<2)
#define TRIANGLE_2_SHARED (1<<3)
#define TRIANGLE_0_1_COPLANAR (1<<4)
#define TRIANGLE_0_2_COPLANAR (1<<5)
#define TRIANGLE_1_2_COPLANAR (1<<6)

struct triangle {
	struct vertex *v[3]; /* three vertices */
	/* Umm, why are we using struct vertex for normals, etc. instead of union vec3? */
	struct vertex n; /* triangle normal */
	struct vertex vnormal[3], vtangent[3], vbitangent[3]; /* vertex normal, tangent and bitangent */
	int flag;
};
#endif

