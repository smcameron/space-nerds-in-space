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
#ifndef SHIP_DEATH_SHRAPNEL_H__
#define SHIP_DEATH_SHRAPNEL_H__

#include <stdint.h>

#include "quat.h"
/* material.h uses struct sng_color by value without including the header that defines it. */
#include "snis_graph.h"
#include "material.h"
#include "ship_death.h"
#include "ship_death_fireball.h"

#ifdef DEFINE_SHIP_DEATH_SHRAPNEL_GLOBALS
#define GLOBAL
#else
#define GLOBAL extern
#endif

struct particle_batch;

/* Hot shards thrown out of the explosion, carried on its gas: generic pre-built shard shapes,
 * not pieces of the ship, glowing on the blackbody ramp and cooling as they fly.  See
 * ship_death.h. */
struct ship_death_shrapnel_tuning {
	float count;
	/* NO FASTER THAN THE GAS, AND THE BIG ONES MUCH SLOWER.  The gas is what throws them, so
	 * nothing it carries can outrun its leading edge.  And how nearly a shard is brought up to
	 * the gas's speed depends on its size: the push goes with its area, size squared, the
	 * resistance with its mass, size cubed.  So the smallest ride with the gas while the
	 * largest are left behind -- which is what leaves glowing wreckage hanging about the site
	 * as the smoke clears, rather than every shard racing out of view with the gas.
	 *
	 * A shard's speed is speed times (smallest size / its size) to the power size_law, less a
	 * random spread.  1 is the plain area-over-mass law; the default is steeper, since a big
	 * plate of hull also tears free later than a splinter. */
	float speed;		/* the smallest, as a fraction of the gas edge's speed */
	float size_law;
	float spread;		/* random scatter below that */
	float size;		/* the largest, in ship radii */
	float life;		/* seconds, the largest shard's life */
	/* Lifetime goes as size to this power, give or take a fifth at random, so they do not all
	 * go at once: splinters first, the big pieces last. */
	float life_law;
	/* NOT ALL AT ONCE.  The hull does not come apart in one instant: shards break free over
	 * this many seconds after the flash, most of them early, each launched hot from near the
	 * centre and so trailing those that went before -- which is what staggers them emerging
	 * from the smoke. */
	float stagger;
	float spin;		/* radians per second for the largest */
	float temp;		/* kelvin at the flash */
	/* Slow enough that the largest still glow red as the smoke clears, a few seconds in. */
	float cooling;		/* seconds per e-folding, for the largest */
	float brightness;
	float albedo;
	/* A shard drawn smaller than this, in pixels, is not drawn at all.  A distance limit, but
	 * one that scales with the shard: a big one carries further than a splinter before it is
	 * lost. */
	float min_pixels;

	/* TRAILS.  What makes debris flying out of a blast read as dangerous is that it trails
	 * fire and smoke: the first few shards -- they are in no order, so these are a fair
	 * sample -- burn as they fly, laying down a trail that glows at first with the shard's heat
	 * and cools to soot, spreading and thinning like the wreck's smoke.  Only while the shard
	 * is hot enough to be burning.
	 *
	 * OFF BY DEFAULT.  Drawn on top of the fireball they looked stuck on; drawn inside it, as
	 * they now are, most of their flight is hidden in its gas and what is left adds little.
	 * Kept for a look now and then.
	 *
	 * NOT A ROD.  A trail that glows along its length, stays one width and runs ruler straight
	 * reads as a beam of light, not as something burning: a first cut did exactly that.  So
	 * the fire is only a short hot tip just behind the shard, and the rest is smoke that
	 * wanders off the line as it ages -- each stretch drifting its own way, smoothly from one
	 * stretch to the next -- thickens and breaks up into patches before it thins away. */
	float trail_count;	/* shards that trail; off unless asked for */
	float trail_life;	/* seconds a segment lasts */
	float trail_width;	/* a new segment's radius, of its shard's size */
	float trail_spread;	/* its radius's growth, ship radii a second */
	float trail_opacity;
	float trail_glow;	/* linear HDR, a new segment off a shard at its hottest */
	float trail_burning;	/* kelvin a shard trails above */
	float trail_wander;	/* how far its smoke strays, ship radii a second */
};

GLOBAL struct ship_death_shrapnel_tuning ship_death_shrapnel_tuning;

#define SHIP_DEATH_SHRAPNEL_MAX 160

struct ship_death_shard {
	union vec3 dir;		/* straight out from the centre */
	float speed;		/* as a fraction of the gas edge's */
	union quat orientation;	/* at the flash */
	union vec3 axis;	/* of its tumble */
	float spin;		/* radians per second */
	float size;		/* world units, tip to tip */
	float launch;		/* seconds after the flash that it breaks free */
	float life;		/* seconds, from its launch */
	float temp;		/* kelvin at its launch */
	float cooling;		/* seconds per e-folding of heat */
	int mesh;
};

/* One death's shrapnel, thrown by its fireball. */
struct ship_death_shrapnel {
	const struct ship_death_fireball *fireball;
	uint32_t seed;
	struct ship_death_shard shard[SHIP_DEATH_SHRAPNEL_MAX];
	struct material shard_material[SHIP_DEATH_SHRAPNEL_MAX];
	int nshards;
	struct particle_batch *trail;
	struct material trail_material;
	unsigned int trail_generation;
	int nvisible, nculled;	/* this frame's, for anyone who wants to know */
};

/* Once, before anything else here: the tuning's defaults and the shard shapes.  0 on success. */
GLOBAL int ship_death_shrapnel_setup(void);
GLOBAL void ship_death_shrapnel_teardown(void);

/* The shards thrown by fireball, drawn from seed.  0 on success; ship_death_shrapnel_fini() it
 * when done. */
GLOBAL int ship_death_shrapnel_init(struct ship_death_shrapnel *sh,
				const struct ship_death_fireball *fireball, uint32_t seed);
GLOBAL void ship_death_shrapnel_fini(struct ship_death_shrapnel *sh);
/* Draw them again from seed: after a change to the tuning, or to throw another lot. */
GLOBAL void ship_death_shrapnel_generate(struct ship_death_shrapnel *sh, uint32_t seed);

/* The shards t seconds after the flash, and their trails, into the frame. */
GLOBAL void ship_death_shrapnel_draw(struct ship_death_shrapnel *sh, float t,
				const struct ship_death_view *view, struct ship_death_frame *f);

#undef GLOBAL
#endif
