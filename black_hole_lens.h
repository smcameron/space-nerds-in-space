#ifndef __BLACK_HOLE_LENS_H__
#define __BLACK_HOLE_LENS_H__

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

#include <stdint.h>

#include "quat.h"
#include "graph_dev.h"

#ifndef DEFINE_BLACK_HOLE_LENS_GLOBALS
#define GLOBAL extern
#else
#define GLOBAL
#endif

/* Choosing which black holes bend the sky.
 *
 * The skybox shader has MAX_GRAVITATIONAL_LENSES slots and a scene can hold more black holes
 * than that, so the ones worth spending a slot on have to be picked every frame, and then put
 * into the slots in the right order.  Both the game client and shadow_lab need to do this
 * identically -- the lab exists to predict what the game will look like, which it cannot do if
 * the two disagree -- so the geometry lives here rather than in either of them.
 *
 * A hole that misses out still draws its disc and both of its rings; it simply does not bend
 * the sky.  That is the degradation to expect when more holes are in view than there are slots.
 */

/* A sane bound for a caller gathering holes into an array on the stack before calling
 * black_hole_lens_select(), which itself takes any number.  Far more than the handful a solar
 * system holds; a caller that hits it simply stops looking, and the holes it did not reach draw
 * normally but are never lensed. */
#define MAX_BLACK_HOLE_LENS_CANDIDATES 64

struct black_hole_lens_hole {
	union vec3 pos;		/* world space centre */
	float radius;		/* event horizon radius, world units */
	uint32_t id;		/* stable per-hole identity; the swirl is derived from it, so
				 * every client spirals a given hole the same way without the
				 * server having to send which way it turns */
};

struct black_hole_lens_view {
	union vec3 cam_pos;
	union quat cam_orientation;
	float fov;		/* vertical field of view, radians */
	int screen_width;
	int screen_height;
};

/* Fill in up to MAX_GRAVITATIONAL_LENSES lenses for the given holes and return how many.
 *
 * Holes are ranked by how much of the window their distortion falls on, so a slot is not spent
 * on one that is off the edge of the view while the hole dead ahead goes without; ties break on
 * angular Einstein radius, for two holes close enough that both flood the window.  The lenses
 * come back sorted nearest first, which is the order graph_dev_set_gravitational_lenses()
 * requires: the shader bends the ray by one lens at a time in slot order.
 *
 * lens[] must hold MAX_GRAVITATIONAL_LENSES entries.  chosen[], if not NULL, must hold that
 * many too, and receives the index into hole[] that each lens came from.  coverage[], if not
 * NULL, must hold nholes entries, and receives every hole's score whether it won a slot or not
 * -- shadow_lab reports these on its HUD.  Holes behind the camera, holes the camera is inside,
 * and holes whose distortion misses the window entirely score zero and are never chosen.
 *
 * lens_strength is the Einstein radius as a multiple of the horizon radius; swirl is the base
 * frame-dragging strength, varied per hole by black_hole_lens_swirl_of() below.
 */
GLOBAL int black_hole_lens_select(const struct black_hole_lens_hole *hole, int nholes,
			const struct black_hole_lens_view *view,
			float lens_strength, float swirl,
			struct graph_dev_gravitational_lens *lens, int *chosen, float *coverage);

/* Signed frame-dragging strength for one hole, so that a cluster of them does not all spiral
 * the same way.  Exposed because it is part of how a hole looks, not just how it is chosen. */
GLOBAL float black_hole_lens_swirl_of(uint32_t id, float swirl);

#undef GLOBAL
#endif
