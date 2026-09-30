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
#ifndef PARTICLE_BATCH_H__
#define PARTICLE_BATCH_H__

#include "quat.h"

#ifdef DEFINE_PARTICLE_BATCH_GLOBALS
#define GLOBAL
#else
#define GLOBAL extern
#endif

struct mesh;

/* Many small soft things -- smoke, flame, sparks -- drawn as ONE mesh.
 *
 * A burning wreck gives off hundreds of them a second.  One entity each is hundreds of draws,
 * hundreds of material copies and a strain on the entity pool; and the blended ones are
 * drawn in whatever order they were added, which is wrong for smoke in front of smoke.  So each
 * frame the caller fills a batch with where each one is and how it looks, and the batch turns
 * them into camera facing quads, back to front, in a single mesh the caller draws with a
 * MATERIAL_PARTICLES material at the origin, unrotated, at scale 1.
 *
 * THE VERTEX CARRIES THE LOOK, since every particle in the batch differs.  The mesh's vertices
 * are the quad's corners in world space; each corner's texture coordinate is where it sits on the
 * quad, in radii from its middle -- -1..1 across, and further along a streak; its normal is not a
 * normal at all but (opacity, emission, kelvin); and its w is the kind plus the seed as a
 * fraction.  particles.shader unpacks them.  In the game, where
 * transform_vertices() writes w and the normals are meant to be normals, these want attributes
 * of their own.
 *
 * PREMULTIPLIED ALPHA, so the one blend -- ONE, ONE_MINUS_SRC_ALPHA -- does both: smoke hides what
 * is behind it, and a flame or a spark, which puts out light with little alpha, adds to it.  That
 * is what lets all three kinds be sorted together and drawn in one go.
 */
#define PARTICLE_SMOKE 0	/* lit by the star, soft and ragged, occluding */
#define PARTICLE_FLAME 1	/* emissive, mostly additive */
#define PARTICLE_SPARK 2	/* emissive, additive, a streak along its motion */

struct particle {
	union vec3 pos;		/* its middle, world space */
	/* How far it moves, world space, while the shutter is open: a spark is drawn as a streak
	 * this long, turned to lie along it on screen.  Zero for a round one. */
	union vec3 streak;
	float radius;		/* world units */
	int kind;
	float opacity;		/* 0..1 at its densest */
	float emission;		/* linear HDR light it gives off */
	float temperature;	/* kelvin, for the colour of that light */
	float seed;		/* 0..1, picks its ragged shape */
	float depth;		/* along the view, filled in by particle_batch_build() */
};

struct particle_batch {
	struct particle *p;
	int n, max;
	struct mesh *m;
};

GLOBAL struct particle_batch *particle_batch_new(int max);
GLOBAL void particle_batch_free(struct particle_batch *b);
GLOBAL void particle_batch_clear(struct particle_batch *b);
/* The next free particle, zeroed, or NULL when the batch is full. */
GLOBAL struct particle *particle_batch_add(struct particle_batch *b);
/* Turn this frame's particles into the batch's mesh, facing a camera at eye looking along
 * forward with up as given, farthest first. */
GLOBAL void particle_batch_build(struct particle_batch *b, const union vec3 *eye,
				const union vec3 *forward, const union vec3 *up);

#undef GLOBAL
#endif
