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

#define MESH_GEOMETRY_TRIANGLES 0
#define MESH_GEOMETRY_LINES 1
#define MESH_GEOMETRY_POINTS 2
#define MESH_GEOMETRY_PARTICLE_ANIMATION 3

#define MESH_LINE_STRIP (1<<1)
#define MESH_LINE_DOTTED (1<<2)

#include "snis_graph.h"
#include "open-simplex-noise.h"

struct mesh_line
{
	struct vertex *start;
	struct vertex *end;
	int flag;
	float additivity;
	float opacity;
	struct sng_color tint_color;
	float time_offset;
};

struct texture_coord {
	float u, v;
};

struct mesh {
	int geometry_mode;
	int ntriangles;
	int nvertices;
	int nlines;
	struct triangle *t;
	struct vertex *v;
	struct mesh_line *l;
	struct texture_coord *tex; /* if not null, contains 3 (u,v)'s per triangle */
	float radius;
	struct material *material; /* for now just one material */
	void *graph_ptr;
};

GLOBAL float mesh_compute_radius(struct mesh *m);
GLOBAL float mesh_compute_nonuniform_scaled_radius(struct mesh *m, double sx, double sy, double sz);
GLOBAL void mesh_distort(struct mesh *m, float distortion, struct osn_context *osn);
GLOBAL void mesh_derelict(struct mesh *m, float distortion);
GLOBAL struct mesh *mesh_duplicate(struct mesh *original);
GLOBAL void mesh_scale(struct mesh *m, float scale);
GLOBAL void mesh_add_point(struct mesh *m, float x, float y, float z);
GLOBAL void mesh_add_line_last_2(struct mesh *m, int flag);
GLOBAL struct mesh *init_circle_mesh(double x, double y, double r, int npoints, double angle);
GLOBAL struct mesh *init_radar_circle_xz_plane_mesh(double x, double z, double r, int ticks, double tick_radius);
GLOBAL struct mesh *init_line_mesh(double x1, double y1, double z1, double x2, double y2, double z2);
GLOBAL struct mesh *mesh_fabricate_axes(void);

GLOBAL void mesh_graph_dev_init(struct mesh *m);
GLOBAL void mesh_graph_dev_cleanup(struct mesh *m);
GLOBAL void mesh_set_flat_shading_vertex_normals(struct mesh *m);
GLOBAL void mesh_set_spherical_vertex_normals(struct mesh *m);
GLOBAL void mesh_set_spherical_cubemap_tangent_and_bitangent(struct mesh *m);
GLOBAL void mesh_set_average_vertex_normals(struct mesh *m);
GLOBAL void mesh_set_reasonable_tangent_and_bitangent(struct vertex *vnormal,
		struct vertex *vtangent, struct vertex *vbitangent);
GLOBAL void mesh_set_reasonable_tangents_and_bitangents(struct mesh *m);
GLOBAL void mesh_set_mikktspace_tangents_and_bitangents(struct mesh *m);
GLOBAL struct mesh *mesh_fabricate_crossbeam(float length, float radius);
GLOBAL void mesh_set_triangle_texture_coords(struct mesh *m, int triangle,
	float u1, float v1, float u2, float v2, float u3, float v3);
GLOBAL struct mesh *mesh_fabricate_billboard(float width, float height);
GLOBAL struct mesh *mesh_fabricate_billboard_with_uv_map(float width, float height,
			float u1, float v1, float u2, float v2);
GLOBAL struct mesh *mesh_unit_icosahedron(void);
GLOBAL struct mesh *mesh_unit_icosphere(int subdivisions);
GLOBAL struct mesh *mesh_unit_cube(int subdivisions);
GLOBAL struct mesh *mesh_unit_spherified_cube(int subdivisions);
GLOBAL void mesh_free(struct mesh *m);
GLOBAL void mesh_sphere_uv_map(struct mesh *m);
GLOBAL void mesh_cylindrical_yz_uv_map(struct mesh *m);
GLOBAL void mesh_cylindrical_xy_uv_map(struct mesh *m);
GLOBAL void mesh_cylindrical_xz_uv_map(struct mesh *m);
GLOBAL void mesh_unit_cube_uv_map(struct mesh *m);
GLOBAL void mesh_map_xy_to_uv(struct mesh *m);
GLOBAL void mesh_distort_and_random_uv_map(struct mesh *m, float distortion, struct osn_context *osn);
GLOBAL struct mesh *mesh_fabricate_planetary_ring(float ir, float or, int nvertices);
GLOBAL struct mesh *init_thrust_mesh(int streaks, double h, double r1, double r2);
GLOBAL struct mesh *init_burst_rod_mesh(int streaks, double h, double r1, double r2);
GLOBAL void mesh_update_material(struct mesh *m, struct material *material);
GLOBAL void mesh_rotate(struct mesh *m, union quat *q);
GLOBAL struct mesh *mesh_tube(float h, float r, float nfaces);
/* Given point x, y, z and assuming mesh m is at origin, return the index of the
 * closest vertex to x, y, z. Distance is returned in *dist unless dist is NULL.
 */
GLOBAL int mesh_nearest_vertex(struct mesh *m, float x, float y, float z, float sx, float sy, float sz, float *dist);

/* Return axis aligned bounding box for a mesh in min[x,y,z], max[x,y,z] */
GLOBAL void mesh_aabb(struct mesh *m, float *minx, float *miny, float *minz, float *maxx, float *maxy, float *maxz);

#undef GLOBAL
#endif
