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

#define MESH_LINE_STRIP (1<<1)
#define MESH_LINE_DOTTED (1<<2)

struct mesh_line
{
	struct vertex *start;
	struct vertex *end;
	int flag;
};

struct mesh {
	int ntriangles;
	int nvertices;
	int nlines;
	struct triangle *t;
	struct vertex *v;
	struct mesh_line *l;
	float radius;
};

GLOBAL float mesh_compute_radius(struct mesh *m);
GLOBAL void mesh_distort(struct mesh *m, float distortion);
GLOBAL void mesh_derelict(struct mesh *m, float distortion);
GLOBAL struct mesh *mesh_duplicate(struct mesh *original);
GLOBAL void mesh_scale(struct mesh *m, float scale);
GLOBAL void mesh_add_point(struct mesh *m, float x, float y, float z);
GLOBAL void mesh_add_line_last_2(struct mesh *m, int flag);
GLOBAL struct mesh *init_circle_mesh(double x, double y, double r);
GLOBAL struct mesh *init_radar_circle_xz_plane_mesh(double x, double z, double r, int ticks, double tick_radius);
GLOBAL struct mesh *init_line_mesh(double x1, double y1, double z1, double x2, double y2, double z2);

#undef GLOBAL
#endif
