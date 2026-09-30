/*
	Copyright (C) 2026 Stephen M. Cameron
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
#ifndef MESH_FRACTURE_H__
#define MESH_FRACTURE_H__

#include <stdint.h>

#ifdef DEFINE_MESH_FRACTURE_GLOBALS
#define GLOBAL
#else
#define GLOBAL extern
#endif

struct mesh;

/* Breaking a ship's hull into the pieces it leaves when it is destroyed.
 *
 * ONE BIG PIECE AND SOME SMALL ONES.  The big one is the derelict -- a real game object that
 * a mission may ask the crew to tow somewhere -- and the rest are debris.  So the split is a
 * power diagram (a Voronoi diagram whose cells can be weighted): seeds scattered over the hull,
 * each triangle going to the seed it is nearest, and one seed near the middle given enough
 * weight that its cell takes core_fraction of the hull's area.
 *
 * TORN, NOT SLICED.  A plain Voronoi cut is a set of flat planes, and a hull cut by flat planes
 * looks machined.  So the hull is first subdivided until no triangle edge is longer than grain,
 * and the point at which each small triangle is judged is pushed about by noise before it is,
 * which makes every boundary a ragged tear following the triangles.
 *
 * OPEN SHELLS.  A piece is only the hull that was there: no cap is made over the break.  Ship
 * meshes are not all watertight, and capping an open mesh is not a well posed problem; drawn
 * two-sided, a shell's inside reads as the ship's interior through the torn edge, which is what
 * a cap would have been standing in for.
 *
 * DETERMINISTIC in the seed, so every bridge screen that breaks the same ship with the same seed
 * gets the same pieces.  In the game that seed is the dead ship's object id.
 */
struct mesh_fracture_params {
	int npieces;		/* the core and the debris together; at least 2 */
	float core_fraction;	/* share of the hull's area that stays with the core, 0..1 */
	float jaggedness;	/* how far the tears wander, as a fraction of the hull's radius */
	float grain;		/* longest triangle edge after subdividing, fraction of radius */
	uint32_t seed;
	/* Leave the pieces off the GPU: for breaking a ship on a thread other than the one that
	 * draws, which then sends each piece with mesh_graph_dev_init().  0, the default, sends
	 * them as they are made. */
	int defer_upload;
};

struct mesh_fracture_piece {
	/* The piece, with its vertices relative to its own centroid, so that it turns about
	 * itself.  Shares nothing with the source mesh; free it with mesh_free().  Carries the
	 * source's texture coordinates but no material -- mesh_free() would free a shared one --
	 * so the caller draws it with a material of its own.
	 *
	 * EACH VERTEX'S w IS ITS DISTANCE TO THE NEAREST TEAR, in hull radii: 0 on the torn edge,
	 * growing inward.  It is what scorches the hull along the break and makes the edge glow.
	 * The renderers ignore a vertex's w, so it is free to carry this; but transform_vertices()
	 * and mesh_set_average_vertex_normals() both overwrite it, so read it before either runs
	 * -- and when this goes into the game, it should become a vertex attribute of its own. */
	struct mesh *m;
	/* Where that centroid was in the source mesh's coordinates: add it back to put the
	 * piece where it was on the intact ship. */
	float offset[3];
	float area;		/* its share of the hull, in the source mesh's units squared */
	int is_core;		/* the derelict; always piece 0 */
};

/* Break src into at most params->npieces pieces, filling piece[] with them core first, and
 * return how many.  A cell the tearing left with no triangles is dropped, so it may be fewer
 * than asked for.  Returns 0 on failure.  src is not changed. */
GLOBAL int mesh_fracture(const struct mesh *src, const struct mesh_fracture_params *params,
				struct mesh_fracture_piece *piece);

#undef GLOBAL
#endif
