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
#include <math.h>

#include "quat.h"
#include "mtwist.h"
#include "vertex.h"
#include "triangle.h"
#include "mesh.h"
#include "snis_graph.h"
#include "material.h"
#include "particle_batch.h"

#define DEFINE_SHIP_DEATH_SHRAPNEL_GLOBALS
#include "ship_death_shrapnel.h"

/* Shard shapes.  Enough that the eye does not pick out repeats among a few dozen tumbling
 * shards; a fixed seed, since the shapes are part of the look and not of any one explosion. */
#define SHARD_MESHES 8
#define SHARD_MESH_SEED 0x5a4d
static struct mesh *shard_mesh[SHARD_MESHES];

/* STATELESS, like everything else here: a trail is short segments laid down on a fixed grid of
 * times after the shard broke free, each a capsule from where the shard was at the start of its
 * slot to where it was at the end, so a trail is continuous however fast the shard went. */
#define TRAIL_STEP 0.02f		/* seconds of flight per segment */
#define TRAIL_KNOT 5			/* segments between the knots its wander is drawn at */
#define TRAIL_PARTICLES 1024

int ship_death_shrapnel_setup(void)
{
	struct ship_death_shrapnel_tuning *tu = &ship_death_shrapnel_tuning;
	struct mtwist_state *mt;
	int i;

	tu->count = 48.0f;
	tu->speed = 1.0f;
	tu->size_law = 1.5f;
	tu->spread = 0.3f;
	tu->size = 0.3f;
	tu->life = 10.0f;
	tu->life_law = 1.0f;
	tu->stagger = 0.4f;
	tu->spin = 8.0f;
	tu->temp = 4500.0f;
	tu->cooling = 4.0f;
	tu->brightness = 4.0f;
	tu->albedo = 0.25f;
	tu->min_pixels = 1.0f;
	tu->trail_count = 0.0f;
	tu->trail_life = 1.5f;
	tu->trail_width = 0.35f;
	tu->trail_spread = 0.06f;
	tu->trail_opacity = 0.5f;
	tu->trail_glow = 4.0f;
	tu->trail_burning = 1500.0f;
	tu->trail_wander = 0.08f;

	mt = mtwist_init(SHARD_MESH_SEED);
	if (!mt)
		return -1;
	for (i = 0; i < SHARD_MESHES; i++) {
		shard_mesh[i] = mesh_fabricate_shard(mt);
		if (!shard_mesh[i]) {
			mtwist_free(mt);
			ship_death_shrapnel_teardown();
			return -1;
		}
	}
	mtwist_free(mt);
	return 0;
}

void ship_death_shrapnel_teardown(void)
{
	int i;

	for (i = 0; i < SHARD_MESHES; i++) {
		if (shard_mesh[i])
			mesh_free(shard_mesh[i]);
		shard_mesh[i] = NULL;
	}
}

static float frand(struct mtwist_state *mt, float lo, float hi)
{
	return lo + (hi - lo) * mtwist_float(mt);
}

void ship_death_shrapnel_generate(struct ship_death_shrapnel *sh, uint32_t seed)
{
	struct ship_death_shrapnel_tuning *tu = &ship_death_shrapnel_tuning;
	struct mtwist_state *mt = mtwist_init(seed * 2654435761u + 0x9e37);
	float largest = tu->size * sh->fireball->ship_radius;
	int i;

	sh->seed = seed;
	if (!mt)
		return;
	sh->nshards = (int) (tu->count + 0.5f);
	if (sh->nshards > SHIP_DEATH_SHRAPNEL_MAX)
		sh->nshards = SHIP_DEATH_SHRAPNEL_MAX;
	for (i = 0; i < sh->nshards; i++) {
		struct ship_death_shard *s = &sh->shard[i];
		float z = frand(mt, -1.0f, 1.0f);
		float phi = frand(mt, 0.0f, 2.0f * M_PI);
		float rxy = sqrtf(1.0f - z * z);
		float u = mtwist_float(mt);
		float relative;

		vec3_init(&s->dir, rxy * cosf(phi), rxy * sinf(phi), z);
		/* Mostly small: a handful of large shards among many little ones. */
		s->size = largest * (0.25f + 0.75f * u * u);
		relative = largest / s->size;
		s->speed = tu->speed * powf(0.25f * relative, tu->size_law) *
				(1.0f - tu->spread * mtwist_float(mt));
		quat_init_axis(&s->orientation, frand(mt, -1, 1), frand(mt, -1, 1),
				frand(mt, -1, 1), frand(mt, 0.0f, 2.0f * M_PI));
		vec3_init(&s->axis, frand(mt, -1, 1), frand(mt, -1, 1), frand(mt, -1, 1));
		if (vec3_magnitude(&s->axis) < 0.01f)
			vec3_init(&s->axis, 0.0f, 0.0f, 1.0f);
		vec3_normalize_self(&s->axis);
		/* A shard's surface catches the blast and its bulk resists turning, so the spin a
		 * blast gives goes as 1/size^2.  Taken as 1/size here: the full law makes the
		 * smallest a blur. */
		s->spin = tu->spin * relative * frand(mt, 0.5f, 1.5f);
		s->life = tu->life * powf(1.0f / relative, tu->life_law) * frand(mt, 0.8f, 1.2f);
		/* Squared, so most break free early and a few straggle. */
		u = mtwist_float(mt);
		s->launch = tu->stagger * u * u;
		s->temp = tu->temp * frand(mt, 0.7f, 1.0f);
		/* Heat held goes as volume and heat lost as area: small shards go dark first. */
		s->cooling = tu->cooling / relative * frand(mt, 0.7f, 1.3f);
		s->mesh = mtwist_int(mt, SHARD_MESHES);
		material_init_shrapnel(&sh->shard_material[i]);
	}
	mtwist_free(mt);
}

int ship_death_shrapnel_init(struct ship_death_shrapnel *sh,
				const struct ship_death_fireball *fireball, uint32_t seed)
{
	sh->fireball = fireball;
	sh->nshards = 0;
	sh->trail_generation = 0;
	sh->nvisible = 0;
	sh->nculled = 0;
	material_init_particles(&sh->trail_material);
	sh->trail = particle_batch_new(TRAIL_PARTICLES);
	if (!sh->trail)
		return -1;
	ship_death_shrapnel_generate(sh, seed);
	return 0;
}

void ship_death_shrapnel_fini(struct ship_death_shrapnel *sh)
{
	particle_batch_free(sh->trail);
	sh->trail = NULL;
}

/* How far from the centre shard s is at time t: its own fraction of the gas edge's travel since
 * it broke free, from its own fraction of the flash's radius.  So a late one trails the early
 * ones. */
static float shard_out(const struct ship_death_shrapnel *sh, const struct ship_death_shard *s,
			float t)
{
	float edge0 = ship_death_fireball_radius_at_time(sh->fireball, 0.0f);

	return s->speed * (edge0 + ship_death_fireball_radius_at_time(sh->fireball, t) -
				ship_death_fireball_radius_at_time(sh->fireball, s->launch));
}

/* A hash of three small integers to 0..1, for the trails' stateless variety. */
static float trail_random(const struct ship_death_shrapnel *sh, int i, int k, int which)
{
	uint32_t h = (uint32_t) i * 0x9e3779b1u ^ (uint32_t) k * 0xc2b2ae35u ^
			(uint32_t) which * 0x27d4eb2fu ^ sh->seed * 0x165667b1u;

	h ^= h >> 15;
	h *= 0x2c1b3c6du;
	h ^= h >> 12;
	h *= 0x297a2d39u;
	h ^= h >> 15;
	return (float) (h & 0xffffff) / (float) 0x1000000;
}

/* A smooth random value along shard i's trail at segment k, 0..1, from knots every TRAIL_KNOT
 * segments: which on channel which. */
static float trail_smooth(const struct ship_death_shrapnel *sh, int i, int k, int which)
{
	int knot = k / TRAIL_KNOT;
	float u = (float) (k % TRAIL_KNOT) / TRAIL_KNOT;
	float a = trail_random(sh, i, knot, which), b = trail_random(sh, i, knot + 1, which);

	u = u * u * (3.0f - 2.0f * u);
	return a + (b - a) * u;
}

/* Shard i's trail at time t: segment k covers its flight from k to k + 1 steps after it broke
 * free, and is as old as the time since the end of that.  Returns 0 once the batch is full. */
static int shard_trail(struct ship_death_shrapnel *sh, int i, float t)
{
	struct ship_death_shrapnel_tuning *tu = &ship_death_shrapnel_tuning;
	struct ship_death_shard *s = &sh->shard[i];
	float ship = sh->fireball->ship_radius;
	float flown = t - s->launch;
	int k, first, last;

	if (flown <= 0.0f)
		return 1;
	first = (int) floorf(fmaxf(flown - tu->trail_life, 0.0f) / TRAIL_STEP);
	last = (int) floorf(flown / TRAIL_STEP);
	for (k = first; k <= last; k++) {
		float t0 = s->launch + TRAIL_STEP * (float) k;
		float t1 = fminf(t0 + TRAIL_STEP, t);
		float age = t - t1, kelvin, burning, r0, r;
		union vec3 a, b, stray;
		float patch;
		struct particle *p;

		if (age >= tu->trail_life || t1 <= t0)
			continue;
		/* How hot the shard was as it laid this segment: no trail once it has cooled. */
		kelvin = s->temp * expf(-(t0 - s->launch) / s->cooling);
		burning = fminf(fmaxf((kelvin - tu->trail_burning) / 500.0f, 0.0f), 1.0f);
		if (burning <= 0.0f)
			continue;
		vec3_mul(&a, &s->dir, shard_out(sh, s, t0));
		vec3_mul(&b, &s->dir, shard_out(sh, s, t1));
		r0 = tu->trail_width * s->size * (0.8f + 0.4f * trail_random(sh, i, k, 1));
		r = r0 + tu->trail_spread * ship * age * (0.6f + 0.8f * trail_smooth(sh, i, k, 6));
		/* Wandering off the line, smoothly along it, further the older it is. */
		vec3_init(&stray, trail_smooth(sh, i, k, 3) - 0.5f, trail_smooth(sh, i, k, 4) - 0.5f,
				trail_smooth(sh, i, k, 5) - 0.5f);
		vec3_mul_self(&stray, 2.0f * tu->trail_wander * ship * age);
		vec3_add_self(&a, &stray);
		vec3_add_self(&b, &stray);
		/* Breaking into patches as it ages. */
		patch = trail_smooth(sh, i, k, 7);
		patch = 1.0f + (patch * patch * 1.6f - 1.0f) * fminf(2.0f * age / tu->trail_life, 1.0f);

		p = particle_batch_add(sh->trail);
		if (!p)
			return 0;
		p->kind = PARTICLE_SMOKE;
		vec3_add(&p->pos, &a, &b);
		vec3_mul_self(&p->pos, 0.5f);
		vec3_add_self(&p->pos, &sh->fireball->pos);
		vec3_sub(&p->streak, &b, &a);
		p->radius = r;
		/* Thinning as it spreads, as the wreck's smoke does, and gone by the end. */
		p->opacity = tu->trail_opacity * burning * powf(r0 / r, 0.8f) * fmaxf(patch, 0.0f) *
				(1.0f - fminf(age / tu->trail_life, 1.0f)) *
				fminf(age / 0.03f + 0.2f, 1.0f);
		/* A short hot tip just behind the shard, then soot. */
		p->temperature = fminf(kelvin * 0.6f, 2200.0f);
		p->emission = tu->trail_glow * burning * expf(-age / 0.03f);
		p->seed = trail_random(sh, i, k, 2);
	}
	return 1;
}

/* The trails, into their batch and the frame. */
static void draw_trails(struct ship_death_shrapnel *sh, float t, const struct ship_death_view *view,
			struct ship_death_frame *f)
{
	struct ship_death_drawable *d;
	union vec3 origin;
	int i, n;

	if (!sh->trail)
		return;
	particle_batch_clear(sh->trail);
	n = (int) (ship_death_shrapnel_tuning.trail_count + 0.5f);
	for (i = 0; i < sh->nshards && i < n; i++)
		if (!shard_trail(sh, i, t))
			break;
	if (sh->trail->n == 0)
		return;
	/* Inside the fireball, behind its gas. */
	for (i = 0; i < sh->trail->n; i++) {
		float through = ship_death_fireball_transmittance(sh->fireball, &view->eye,
								&sh->trail->p[i].pos);

		sh->trail->p[i].opacity *= through;
		sh->trail->p[i].emission *= through;
	}
	particle_batch_build(sh->trail, &view->eye, &view->forward, &view->up);
	sh->trail_material.particles.albedo = 0.1f;
	vec3_init(&origin, 0.0f, 0.0f, 0.0f);
	d = ship_death_frame_add(f, sh->trail->m, &sh->trail_material, &origin, 1.0f);
	if (!d)
		return;
	/* Never zero, which would mean a mesh that never changes. */
	sh->trail_generation = sh->trail_generation + 1 ? sh->trail_generation + 1 : 1;
	d->mesh_generation = sh->trail_generation;
	d->no_shade = 1;
	d->no_cast_shadow = 1;
}

void ship_death_shrapnel_draw(struct ship_death_shrapnel *sh, float t,
				const struct ship_death_view *view, struct ship_death_frame *f)
{
	struct ship_death_shrapnel_tuning *tu = &ship_death_shrapnel_tuning;
	int i;

	sh->nvisible = 0;
	sh->nculled = 0;
	for (i = 0; i < sh->nshards; i++) {
		struct ship_death_shard *s = &sh->shard[i];
		struct ship_death_drawable *d;
		union vec3 pos, to_camera;
		union quat tumble;
		float scale = s->size;
		float age = t - s->launch;
		float remaining = s->life - age;
		float distance, out;

		if (age < 0.0f || remaining <= 0.0f)
			continue;
		if (remaining < 0.25f * s->life)
			scale *= remaining / (0.25f * s->life);
		/* Always inside the ball -- see shard_out() -- less half its length, so its tip does
		 * not poke out of the edge while its middle is still within. */
		out = shard_out(sh, s, t);
		vec3_mul(&pos, &s->dir, fmaxf(out - 0.5f * scale, 0.0f));
		vec3_add_self(&pos, &sh->fireball->pos);
		vec3_sub(&to_camera, &view->eye, &pos);
		distance = vec3_magnitude(&to_camera);
		if (distance > 0.0f && scale * view->pixels_per_unit / distance < tu->min_pixels) {
			sh->nculled++;
			continue;
		}
		d = ship_death_frame_add(f, shard_mesh[s->mesh], &sh->shard_material[i], &pos, scale);
		if (!d)
			break;
		quat_init_axis_v(&tumble, &s->axis, s->spin * age);
		quat_mul(&d->orientation, &tumble, &s->orientation);
		d->no_cast_shadow = 1;
		sh->shard_material[i].shrapnel.temperature = s->temp * expf(-age / s->cooling);
		sh->shard_material[i].shrapnel.brightness = tu->brightness;
		sh->shard_material[i].shrapnel.albedo = tu->albedo;
		sh->nvisible++;
	}
	draw_trails(sh, t, view, f);
}
