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
#ifndef SHIP_DEATH_FIREBALL_H__
#define SHIP_DEATH_FIREBALL_H__

#include "quat.h"
/* material.h uses struct sng_color by value without including the header that defines it. */
#include "snis_graph.h"
#include "material.h"
#include "ship_death.h"

#ifdef DEFINE_SHIP_DEATH_FIREBALL_GLOBALS
#define GLOBAL
#else
#define GLOBAL extern
#endif

/* The fireball: a raymarched MATERIAL_EXPLOSION volume that cools into smoke and thins away to
 * nothing, with the flash's glare and the fire's light on what is near it.  See ship_death.h.
 *
 * The material knows nothing of seconds: it takes an age from 0 to 1.  This is what turns the
 * clock into one -- how long it lasts, how fast the ball grows -- and says where the ball's edge
 * is at any time, which is what carries the debris outward.
 */
struct ship_death_fireball_tuning {
	/* PERCUSSIVE.  A real blast is nearly instant: nearly all of its violence is in the first
	 * few tenths of a second, and in vacuum the gas then races away and thins out fast.  So
	 * the whole explosion is short -- the fire out by about 0.6s, the smoke gone within a few
	 * seconds -- and what lingers is the shrapnel glowing on, and in time the wreck. */
	float lifetime;		/* seconds from flash to the last of the smoke */
	/* NO FINAL SIZE.  In air a blast is braked by what it pushes aside and comes to rest; in
	 * vacuum nothing pushes back, so the gas coasts outward for ever and fades by thinning
	 * out.  So this is the radius as the fire goes out, not a limit, and the ball keeps
	 * growing at the same speed after it. */
	float size;		/* radius when the fire is out, in ship radii */
	float start;		/* radius at the flash, as a fraction of that */
	/* How long the blast takes to bring the gas up to its coasting speed, as a fraction of
	 * the life.  Really it is about the time sound takes to cross the ship -- hundredths of a
	 * second, lost inside the flash -- and this is about that: the ball bursts out rather
	 * than swelling. */
	float burst;
	float brightness;	/* the fire's glow, before the flash is added */
	/* THE FLASH: the fire over-driven for the first instant, so the frame blows out white and
	 * then the fire is seen through it.  flash_gain times the glow at the flash, falling away
	 * with a time constant of flash_time seconds. */
	float flash_gain;
	float flash_time;
	float light;		/* how strongly the fire lights what is near it; 0 for not at all */
	/* The flash's glare: the game's own star glare, borrowed.  Over-driving the fire alone
	 * only makes a small ball white; a flash that blows out the frame has to spread light far
	 * past the ball, and the star's point spread function is exactly that, already in the
	 * renderer.  Its billboard is flash_size ship radii across, and its disc a pinpoint. */
	float flash_size;	/* the glare billboard, in ship radii across */
	float flash_glare;	/* its brightness at the flash */
	/* The volume's own dials -- temperature, smoke, shape, march steps -- as a
	 * MATERIAL_EXPLOSION; its age, seed, dilution and brightness are set per frame. */
	struct material material;
};

GLOBAL struct ship_death_fireball_tuning ship_death_fireball_tuning;

/* One death's fireball. */
struct ship_death_fireball {
	union vec3 pos;		/* its centre */
	float ship_radius;
	float seed;
	float age;		/* 0 at the flash, 1 when the last smoke is gone */
	float radius_now;
	float flash_now;	/* this frame's multiplier on the glow */
	struct material material;
	struct material flash_material;
};

/* Once, before anything else here: the tuning's defaults and the shared meshes.  0 on success. */
GLOBAL int ship_death_fireball_setup(void);
GLOBAL void ship_death_fireball_teardown(void);

GLOBAL void ship_death_fireball_init(struct ship_death_fireball *fb, const union vec3 *pos,
				float ship_radius, float seed);
/* Bring it to an age, 0 to 1: every frame, before anything asks it anything. */
GLOBAL void ship_death_fireball_set_age(struct ship_death_fireball *fb, float age);

/* The gas's leading edge seconds after the flash, extrapolated past the end of the smoke: it is
 * what carries debris outward. */
GLOBAL float ship_death_fireball_radius_at_time(const struct ship_death_fireball *fb, float seconds);

/* The fireball and the flash's glare, into the frame. */
GLOBAL void ship_death_fireball_draw(struct ship_death_fireball *fb, struct ship_death_frame *f);

/* The fire as a second light on something at pos: where it is, its colour times its intensity,
 * and how far it wraps round.  Returns 0, setting nothing, once the fire is too cool to light
 * anything. */
GLOBAL int ship_death_fireball_light(const struct ship_death_fireball *fb, const union vec3 *pos,
				union vec3 *light_pos, float color[3], float *wrap);

/* How much of a soft thing at p shows through the fireball, seen from eye: 1 with no fire in the
 * way, toward 0 deep inside a dense one.  The ball is raymarched and writes no depth, so the
 * blended things drawn after it -- smoke, flame, sparks -- would otherwise all float on top of
 * it; solid things need not ask, since the march stops at them. */
GLOBAL float ship_death_fireball_transmittance(const struct ship_death_fireball *fb,
				const union vec3 *eye, const union vec3 *p);

#undef GLOBAL
#endif
