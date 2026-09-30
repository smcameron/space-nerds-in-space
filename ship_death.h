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
#ifndef SHIP_DEATH_H__
#define SHIP_DEATH_H__

#include "quat.h"

#ifdef DEFINE_SHIP_DEATH_GLOBALS
#define GLOBAL
#else
#define GLOBAL extern
#endif

struct mesh;
struct material;

/* A SHIP'S DEATH: the fireball, the shrapnel it throws, and the ship broken into a derelict and
 * burning chunks.  ship_death_fireball.c, ship_death_shrapnel.c and ship_death_wreck.c.
 *
 * These modules decide what is where and how it looks, and draw nothing themselves: each frame
 * they fill a ship_death_frame with things to draw -- a mesh, a material, a pose -- and the
 * caller hands those to its renderer: in snis_client, as entities.
 *
 * Everything that can be tuned is in each module's tuning struct, one for the whole program,
 * with the looks the effect was made with as its defaults.  Everything about one death is in
 * its own instance, so several can be going at once.
 */

/* One thing to draw: the fields of an entity, near enough. */
struct ship_death_drawable {
	struct mesh *m;
	struct material *material;
	union vec3 pos;
	union quat orientation;
	float scale;
	/* Non-zero, and changed, when the mesh's CONTENTS changed since the last frame: the
	 * particle batch's, which is rebuilt every frame. */
	unsigned int mesh_generation;
	int no_shade;			/* self-lit or self-shaded; no analytic shade term */
	int no_cast_shadow;
	/* A second light, from the fire: see ship_death_fireball_light().  Zero colour for none. */
	union vec3 aux_light_pos;
	float aux_light_color[3];
	float aux_light_wrap;
};

#define SHIP_DEATH_MAX_DRAWABLES 256

struct ship_death_frame {
	struct ship_death_drawable d[SHIP_DEATH_MAX_DRAWABLES];
	int n;
};

GLOBAL void ship_death_frame_clear(struct ship_death_frame *f);
/* The next drawable, with identity orientation and nothing else set, or NULL when the frame is
 * full. */
GLOBAL struct ship_death_drawable *ship_death_frame_add(struct ship_death_frame *f, struct mesh *m,
					struct material *material, const union vec3 *pos, float scale);

#undef GLOBAL
#endif
