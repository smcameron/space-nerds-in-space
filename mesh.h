#ifndef MESH_H__
#define MESH_H__
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

#ifdef DEFINE_MESH_GLOBALS
#define GLOBAL
#else
#define GLOBAL extern
#endif

struct mesh {
	int ntriangles;
	int nvertices;
	struct triangle *t;
	struct vertex *v;
	float radius;
};

GLOBAL float mesh_compute_radius(struct mesh *m);
GLOBAL void mesh_distort(struct mesh *m, float distortion);
GLOBAL struct mesh *mesh_duplicate(struct mesh *original);
GLOBAL void mesh_scale(struct mesh *m, float scale);

#undef GLOBAL
#endif
