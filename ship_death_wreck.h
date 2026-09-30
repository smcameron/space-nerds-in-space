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
#ifndef SHIP_DEATH_WRECK_H__
#define SHIP_DEATH_WRECK_H__

#include <stdint.h>

#include "quat.h"
/* material.h uses struct sng_color by value without including the header that defines it. */
#include "snis_graph.h"
#include "material.h"
#include "mesh_fracture.h"
#include "ship_death.h"
#include "ship_death_fireball.h"

#ifdef DEFINE_SHIP_DEATH_WRECK_GLOBALS
#define GLOBAL
#else
#define GLOBAL extern
#endif

struct mesh;
struct particle_batch;

/* The ship that died, broken into one big piece -- the derelict -- and a handful of chunks that
 * the explosion carries off and that burn away, smoking, flaming and throwing sparks, over half a
 * minute.  See ship_death.h.
 *
 * Two parts.  A FRACTURE is a ship's hull broken into pieces, and everything worked out from the
 * pieces' shapes; it depends only on the mesh and how it was broken, so every death of the same
 * ship broken the same way can share one.  A WRECK is one death: how its pieces fly, drawn from
 * its seed, and how each burns, frame by frame.
 */
struct ship_death_wreck_tuning {
	/* HOW IT BREAKS: see mesh_fracture.h. */
	float pieces;		/* the core and the chunks together */
	float core;		/* share of the hull that stays with the derelict */
	float jaggedness;	/* how far the tears wander, hull radii */
	float grain;		/* longest triangle edge after subdividing, hull radii */

	/* THE MOTION, the shrapnel's physics at a larger size.  The gas pushes a piece with its
	 * area and its mass resists, so a piece's speed is speed times (a shrapnel-sized piece /
	 * its size) to the power size_law, as a fraction of the gas edge's speed, and it spins as
	 * 1/size squared.  Size is the square root of the hull area it carries.
	 *
	 * THE CORE IS THE EXCEPTION.  It sits at the middle of the blast and is pushed from every
	 * side at once, so the pushes largely cancel: it gets only a small drift, in a random
	 * direction, rather than the size law.  That is also what the game wants of it -- the
	 * server slows a derelict so a mining bot can catch it, and a mission may send the crew to
	 * tow it. */
	int mode;		/* SHIP_DEATH_WRECK_MOTION, or an exploded view, to look at the tears */
	float explode;		/* exploded view: chunk separation, in ship radii */
	/* Slow enough that the chunks hang about the derelict as a debris field for the half minute
	 * they last, rather than racing out of view with the gas. */
	float speed;		/* a shrapnel-sized piece, as a fraction of the gas edge */
	float size_law;
	float scatter;		/* how far a chunk's heading strays from straight out */
	float spin;		/* radians per second for a quarter-ship-radius piece */
	float core_drift;	/* the core's speed, as a fraction of the gas edge */
	/* HOW DEBRIS GOES.  A chunk flying away is dropped once it is smaller than a pixel: nobody
	 * sees that.  Otherwise it burns away, from its torn edges inward, over its WHOLE life:
	 * from burn_start seconds after the flash, as the smoke clears, to the end of its life.
	 * The front goes fastest at first and slows as the piece burns down -- its progress is
	 * 1 - (1 - s)^burn_ease over the burn's share s of the time -- so the smoke off it, and
	 * the glow of the front, taper away to nothing rather than stopping at a stroke.
	 *
	 * The lives are spread evenly by size, from life_min for the smallest to life for the
	 * largest, each jittered by a fraction of the spacing: so the chunks finish one at a
	 * time, seconds apart, however alike their sizes are.  The core -- the derelict -- never
	 * goes. */
	float life_min;		/* seconds the smallest chunk lasts */
	float life;		/* seconds the largest chunk lasts */
	float life_jitter;	/* of the spacing between one chunk's end and the next */
	float burn_start;	/* seconds after the flash the chunks start to burn */
	float burn_ease;	/* how much faster the burn goes at first than on average */
	float min_pixels;

	/* ALONG THE TEAR: soot, and the torn metal glowing as it cools.  The glow starts at
	 * edge_temp at the flash and falls by e every edge_cooling seconds, slow enough that the
	 * edges still glow orange through the thinning smoke and dull red after it has cleared. */
	float interior;		/* albedo of the inside of the hull */
	float scorch;		/* width of the sooted band, hull radii */
	float edge_width;	/* width of the glowing edge, hull radii */
	float edge_temp;	/* kelvin at the flash */
	/* Seconds per e-folding.  Slow enough that the wreck visibly cools -- glowing and smoking
	 * some seven or eight seconds -- in among a debris field that lasts half a minute. */
	float edge_cooling;
	float edge_brightness;
	/* Width of the dull red zone ahead of a burning front, hull radii: metal heating before it
	 * goes. */
	float preheat;

	/* SMOKE FROM THE WRECKAGE.  Hot, torn structure vents and outgasses -- a ship carries air,
	 * fuel and plastics -- so the wreck smokes: a thin trickle from its glowing edges while
	 * they are hot, the derelict's included, and much more from a chunk burning away, off the
	 * burning front. */
	float smoke_burn_rate;		/* puffs a second off a chunk burning away */
	float smoke_smoulder_rate;	/* puffs a second off the largest piece's hot edges */
	/* Kelvin the edges smoulder above: where their glow gives out, so every piece -- the
	 * derelict too -- smokes for as long as its tears can be seen glowing, and stops as they
	 * go dark. */
	float smoke_smoulder_temp;
	/* HOW A PUFF SPREADS.  Not a burst: the burn is a slow outgassing with little energy in
	 * it, so a puff is born the size of the flame it comes out of and spreads at a steady
	 * pace, the way gas does in a vacuum -- its radius smoke_size plus smoke_spread a second.
	 * Its soot is fixed, so as it spreads it thins: its opacity goes as (birth radius /
	 * radius) to the smoke_thinning.  2 is a lone cloud seen through; well under that here,
	 * since each puff is a stretch of a continuous plume, overlapping its neighbours, and at 2
	 * the plume is gone a second out.  It fades by spreading, not on a timer; the end of its
	 * life only takes away what little is left.  More, smaller and longer lived than a burst's
	 * puffs, so they run together into a plume. */
	float smoke_life;		/* seconds a puff lasts */
	float smoke_size;		/* a new puff's radius, hull radii */
	float smoke_spread;		/* how fast its radius grows, hull radii a second */
	float smoke_thinning;
	float smoke_opacity;
	float smoke_drift;		/* hull radii a second, its own */
	float smoke_carry;		/* how much of its piece's velocity it keeps */
	float smoke_albedo;

	/* EMBERS.  Flakes of hot hull breaking loose from a burning front and flying off, tumbling
	 * and cooling out in a second or two.  The shrapnel's own hot-metal material on small
	 * shards of the wreck's own.  Tied to the same share of the burn as the smoke, so they
	 * come thickest at first and dwindle with it. */
	float ember_rate;		/* embers a second off a chunk, at the start of its burn */
	float ember_life;		/* seconds */
	float ember_size;		/* hull radii, tip to tip */
	float ember_speed;		/* hull radii a second, their own push */

	/* SPARKS.  The embers' small change, and far more of them: specks of burning metal too
	 * small to see the shape of, only a hot point -- which, moving, the eye takes as a streak.
	 * So a spark is a particle in the batch, a white-hot core in an orange glow drawn along
	 * the way it moves on screen, as long as it travels while a shutter of spark_shutter
	 * seconds is open.  They cool from yellow-white to red and go out in about a second.  The
	 * embers stay, fewer, as the occasional flake big enough to be seen tumbling. */
	float spark_rate;		/* sparks a second off the largest chunk, at first */
	float spark_life;		/* seconds */
	float spark_size;		/* radius of the glow, hull radii */
	float spark_speed;		/* hull radii a second, their own push */
	float spark_brightness;		/* linear HDR, when new */
	float spark_shutter;		/* seconds of motion a streak shows */

	/* POPS.  Now and then a pocket of trapped air or fuel in a burning piece vents: a flare on
	 * the front, a gout of flame, a burst of smoke and a burst of faster embers, all from one
	 * spot.  They break up the steady burn.  A chunk's candidate pops come every pop_interval
	 * seconds or so, jittered, and each goes off with chance pop_chance at the start of the
	 * burn, less as it burns down -- all from the wreck's seed, so the same break pops the
	 * same way. */
	float pop_interval;
	float pop_chance;
	float pop_jet;			/* the jet's extra speed, hull radii a second */
	float pop_embers;		/* in each pop's burst */
	float pop_sparks;		/* likewise */

	/* FLAME.  The thing that says "burning" rather than "hot": tongues of burning gas licking
	 * off the front, a few tenths of a second each, so many at once that they read as one
	 * ragged flickering edge of fire.  Without air there is no up for them to rise toward;
	 * they are gas venting out of the torn structure and burning as it goes, so they lick out
	 * across the tear -- away from the metal still left -- and off one face of the plate or
	 * the other.  Each stays rooted on the front as the piece turns.
	 *
	 * The flames light their own piece, through its second light: a flickering orange from
	 * the middle of its fire, as strong as the burn, taking over from the fireball's light
	 * once that has died. */
	float flame_rate;		/* tongues a second off a chunk, at the start of its burn */
	float flame_life;		/* seconds */
	float flame_size;		/* half its width at the base, hull radii */
	float flame_length;		/* hull radii, base to tip */
	float flame_brightness;		/* linear HDR */
	float flame_light;		/* its light on its own piece; 0 for none */
};

#define SHIP_DEATH_WRECK_MOTION 0
#define SHIP_DEATH_WRECK_EXPLODED 1

GLOBAL struct ship_death_wreck_tuning ship_death_wreck_tuning;

#define SHIP_DEATH_WRECK_MAX_PIECES 32
/* Steps of a piece's burn at which the centre of what is left is measured. */
#define SHIP_DEATH_COM_SAMPLES 24

struct ship_death_fracture {
	struct mesh_fracture_piece piece[SHIP_DEATH_WRECK_MAX_PIECES];
	int npieces;
	/* How far the furthest point of each piece is from its torn edge, hull radii: how far the
	 * dissolving front has to go to have eaten the whole piece. */
	float reach[SHIP_DEATH_WRECK_MAX_PIECES];
	/* Where the middle of what is left of each piece is, in its own mesh's coordinates, at
	 * SHIP_DEATH_COM_SAMPLES + 1 even steps of its burn: as it burns away, its centre of mass
	 * moves toward whatever survives, and it is about that that it turns. */
	float com[SHIP_DEATH_WRECK_MAX_PIECES][SHIP_DEATH_COM_SAMPLES + 1][3];
	/* Each piece's vertices, nearest the torn edge first: where smoke comes off.  Smouldering
	 * takes from the start of the list; burning from wherever the dissolving front has got
	 * to. */
	int *order[SHIP_DEATH_WRECK_MAX_PIECES];
	/* Which way a flame licks from each vertex of each piece: across the tear, away from the
	 * metal still left -- down the slope of the vertex's distance to the torn edge -- and the
	 * plate's normal, for leaning off one face or the other.  Six floats a vertex. */
	float *flow[SHIP_DEATH_WRECK_MAX_PIECES];
	/* Each chunk's size, as the square root of its area over the largest chunk's.  A smaller
	 * chunk has less edge to burn, so fewer tongues, and they are smaller too -- though not in
	 * proportion, or the last scraps would have no visible fire at all. */
	float size[SHIP_DEATH_WRECK_MAX_PIECES];
	float core_share;		/* what the core actually got */
	int total_triangles;
	/* The wreck's material as this ship wears it: its texture, and its size. */
	struct material material;
};

/* How each piece flies, drawn once per death from its seed. */
struct ship_death_piece_motion {
	union vec3 dir;		/* straight out from the ship's middle, scattered a little */
	float speed;		/* as a fraction of the gas edge's speed */
	union vec3 axis;	/* of its tumble */
	float spin;		/* radians per second */
	float life;		/* seconds; the core has none */
};

#define SHIP_DEATH_EMBER_MAX 96

struct ship_death_wreck {
	const struct ship_death_fireball *fireball;
	const struct ship_death_fracture *fracture;
	uint32_t seed;
	/* THE SHIP'S FRAME.  The fracture is in the ship's own coordinates, and the ship died
	 * facing this way, so the pieces and the way they fly are turned by it.  Identity unless
	 * the caller says otherwise. */
	union quat orientation;
	/* THE DERELICT.  In the game the core is a real object, the server's: it goes where the
	 * server says, not where the blast would have sent it, and the game draws it.  When
	 * core_pose is set, the core is not put in the frame -- its material, heat and the fire's
	 * light on it, is left for the caller: see ship_death_wreck_core_material() -- and
	 * everything that comes off it asks core_pose where the ship's origin was, and which way
	 * up, t seconds after the flash.  It returns 0 when it does not know, and then nothing
	 * comes off the core. */
	int (*core_pose)(void *cookie, float t, union vec3 *pos, union quat *orientation);
	void *core_cookie;
	int core_drawn;		/* this frame's: the core's material is good */
	struct ship_death_piece_motion motion[SHIP_DEATH_WRECK_MAX_PIECES];
	/* This frame's: the fracture's, with the edges' heat.  Then each piece's own copy, since
	 * each dissolves on its own schedule. */
	struct material material;
	struct material piece_material[SHIP_DEATH_WRECK_MAX_PIECES];
	/* The smoke, flame and sparks: one batch, drawn as one mesh; see particle_batch.h. */
	struct particle_batch *particles;
	struct material particles_material;
	unsigned int particles_generation;
	struct material ember_material[SHIP_DEATH_EMBER_MAX];
	int nembers;
	/* This frame's flames on each piece, for its light: how many, where they are on average,
	 * and which drawable in the frame the piece is, or -1. */
	int piece_nflames[SHIP_DEATH_WRECK_MAX_PIECES];
	union vec3 piece_fire_at[SHIP_DEATH_WRECK_MAX_PIECES];
	int piece_drawable[SHIP_DEATH_WRECK_MAX_PIECES];
	int nflying;		/* this frame's, for anyone who wants to know */
};

/* Once, before anything else here: the tuning's defaults and the ember shapes.  0 on success. */
GLOBAL int ship_death_wreck_setup(void);
GLOBAL void ship_death_wreck_teardown(void);

/* Break ship as the tuning says, the way seed picks.  0 on success; free it with
 * ship_death_fracture_free() either way. */
GLOBAL int ship_death_fracture_build(struct ship_death_fracture *fr, const struct mesh *ship,
				uint32_t seed);
/* The same, in two halves: the breaking, which touches no GPU and so may be done on any thread,
 * and sending the pieces to the GPU, on the thread that draws.  Several fractures may be built
 * at once on different threads; the tuning must not change meanwhile. */
GLOBAL int ship_death_fracture_build_deferred(struct ship_death_fracture *fr,
				const struct mesh *ship, uint32_t seed);
GLOBAL void ship_death_fracture_upload(struct ship_death_fracture *fr);
GLOBAL void ship_death_fracture_free(struct ship_death_fracture *fr);

/* One death of the fractured ship, blown apart by fireball and flying as seed says.  0 on
 * success; ship_death_wreck_fini() it when done. */
GLOBAL int ship_death_wreck_init(struct ship_death_wreck *w, const struct ship_death_fireball *fireball,
				const struct ship_death_fracture *fracture, uint32_t seed);
GLOBAL void ship_death_wreck_fini(struct ship_death_wreck *w);
/* How the pieces fly, drawn again from seed: after a change to the tuning or the fracture. */
GLOBAL void ship_death_wreck_plan(struct ship_death_wreck *w, uint32_t seed);

/* The pieces t seconds after the flash, and their smoke, flame, sparks and embers, into the
 * frame. */
GLOBAL void ship_death_wreck_draw(struct ship_death_wreck *w, float t,
				const struct ship_death_view *view, struct ship_death_frame *f);

/* Seconds after the flash by which the chunks, and everything off them and off the core, are
 * gone: all that is left is the derelict, if there is one, cold. */
GLOBAL float ship_death_wreck_end(const struct ship_death_wreck *w);

/* With core_pose set: the material the caller should draw the derelict with this frame, the
 * edges' heat and the fire's light on it -- or NULL, for the cold one, when core_pose did not
 * know where the core was. */
GLOBAL struct material *ship_death_wreck_core_material(struct ship_death_wreck *w);

/* The wreck's material as this fracture wears it once it has cooled: the soot and the inside of
 * the hull, no glow.  For a derelict long after its ship died. */
GLOBAL void ship_death_fracture_cold_material(const struct ship_death_fracture *fr,
				struct material *m);

#undef GLOBAL
#endif
