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
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "quat.h"
#include "mtwist.h"
#include "vertex.h"
#include "triangle.h"
#include "mesh.h"
#include "snis_graph.h"
#include "material.h"
#include "mesh_fracture.h"
#include "particle_batch.h"
#include "star_light.h"

#define DEFINE_SHIP_DEATH_WRECK_GLOBALS
#include "ship_death_wreck.h"

#define MAX_PIECES SHIP_DEATH_WRECK_MAX_PIECES
#define COM_SAMPLES SHIP_DEATH_COM_SAMPLES

/* STATELESS.  Puff k of a piece's smoulder or burn is born at a fixed time from a fixed rate, and
 * everything else about it -- where on the piece, which way it drifts, its shape -- comes from a
 * hash of the piece, the channel and k.  Where it is now is where the piece was then, carried on
 * by part of the piece's velocity and a slow drift of its own.  So a frame is worked out from the
 * clock alone, and scrubbing, pausing and repeating all still hold.  The embers, sparks, flames
 * and pops likewise. */
#define PARTICLE_MAX 4096

#define EMBER_MESHES 4
#define EMBER_MESH_SEED 0x3e1b
static struct mesh *ember_mesh[EMBER_MESHES];

#define SMOKE_SMOULDER 0
#define SMOKE_BURN 1
#define SMOKE_POP 2
#define SMOKE_FLAME 3
#define POP_PUFFS 6		/* in each pop's burst of smoke */
#define POP_FLAMES 8		/* in each pop's gout of flame */

int ship_death_wreck_setup(void)
{
	struct ship_death_wreck_tuning *tu = &ship_death_wreck_tuning;
	struct mtwist_state *mt;
	int j;

	tu->pieces = 8.0f;
	tu->core = 0.6f;
	tu->jaggedness = 0.08f;
	tu->grain = 0.035f;
	tu->mode = SHIP_DEATH_WRECK_MOTION;
	tu->explode = 0.6f;
	tu->speed = 0.08f;
	tu->size_law = 1.5f;
	tu->scatter = 0.3f;
	tu->spin = 3.0f;
	tu->core_drift = 0.02f;
	tu->life_min = 20.0f;
	tu->life = 30.0f;
	tu->life_jitter = 0.3f;
	tu->burn_start = 1.0f;
	tu->burn_ease = 1.3f;
	tu->min_pixels = 1.0f;
	tu->interior = 0.06f;
	tu->scorch = 0.12f;
	tu->edge_width = 0.012f;
	tu->edge_temp = 2800.0f;
	tu->edge_cooling = 6.0f;
	tu->edge_brightness = 3.0f;
	tu->preheat = 0.04f;
	tu->smoke_burn_rate = 20.0f;
	tu->smoke_smoulder_rate = 15.0f;
	tu->smoke_smoulder_temp = 800.0f;
	tu->smoke_life = 5.0f;
	tu->smoke_size = 0.03f;
	tu->smoke_spread = 0.04f;
	tu->smoke_thinning = 0.6f;
	tu->smoke_opacity = 0.8f;
	tu->smoke_drift = 0.04f;
	tu->smoke_carry = 0.6f;
	tu->smoke_albedo = 0.12f;
	tu->ember_rate = 4.0f;
	tu->ember_life = 1.6f;
	tu->ember_size = 0.06f;
	tu->ember_speed = 0.12f;
	tu->spark_rate = 45.0f;
	tu->spark_life = 1.2f;
	tu->spark_size = 0.005f;
	tu->spark_speed = 0.2f;
	tu->spark_brightness = 6.0f;
	tu->spark_shutter = 0.04f;
	tu->pop_interval = 3.0f;
	tu->pop_chance = 0.7f;
	tu->pop_jet = 0.6f;
	tu->pop_embers = 3.0f;
	tu->pop_sparks = 8.0f;
	tu->flame_rate = 150.0f;
	tu->flame_life = 0.7f;
	tu->flame_size = 0.022f;
	tu->flame_length = 0.05f;
	tu->flame_brightness = 2.5f;
	tu->flame_light = 1.5f;

	mt = mtwist_init(EMBER_MESH_SEED);
	if (!mt)
		return -1;
	for (j = 0; j < EMBER_MESHES; j++) {
		ember_mesh[j] = mesh_fabricate_shard(mt);
		if (!ember_mesh[j]) {
			mtwist_free(mt);
			ship_death_wreck_teardown();
			return -1;
		}
	}
	mtwist_free(mt);
	return 0;
}

void ship_death_wreck_teardown(void)
{
	int j;

	for (j = 0; j < EMBER_MESHES; j++) {
		if (ember_mesh[j])
			mesh_free(ember_mesh[j]);
		ember_mesh[j] = NULL;
	}
}

/* ---------------------------------------------------------------------------------------------
 * THE FRACTURE: the pieces, and what is measured from their shapes.
 */

void ship_death_fracture_free(struct ship_death_fracture *fr)
{
	int i;

	for (i = 0; i < fr->npieces; i++) {
		if (fr->piece[i].m)
			mesh_free(fr->piece[i].m);
		free(fr->order[i]);
		fr->order[i] = NULL;
		free(fr->flow[i]);
		fr->flow[i] = NULL;
	}
	fr->npieces = 0;
}

/* The centre of what is left of piece i at each step of its burn: the area-weighted centroid of
 * the triangles still further from the torn edge than the front.  The hull is a shell of even
 * thickness, so area stands in for mass.  Once nothing is left the last centre is kept. */
static void measure_centres(struct ship_death_fracture *fr, int i)
{
	struct mesh *m = fr->piece[i].m;
	float reach = fr->reach[i] * 1.3f + 0.01f;
	int step, j, k;

	for (step = 0; step <= COM_SAMPLES; step++) {
		float front = reach * (float) step / COM_SAMPLES;
		float sum[3] = { 0.0f, 0.0f, 0.0f }, total = 0.0f;

		for (j = 0; j < m->ntriangles; j++) {
			struct triangle *tr = &m->t[j];
			float u[3], v[3], c[3], area, w;

			w = (tr->v[0]->w + tr->v[1]->w + tr->v[2]->w) / 3.0f;
			if (w < front)
				continue;
			u[0] = tr->v[1]->x - tr->v[0]->x;
			u[1] = tr->v[1]->y - tr->v[0]->y;
			u[2] = tr->v[1]->z - tr->v[0]->z;
			v[0] = tr->v[2]->x - tr->v[0]->x;
			v[1] = tr->v[2]->y - tr->v[0]->y;
			v[2] = tr->v[2]->z - tr->v[0]->z;
			c[0] = u[1] * v[2] - u[2] * v[1];
			c[1] = u[2] * v[0] - u[0] * v[2];
			c[2] = u[0] * v[1] - u[1] * v[0];
			area = 0.5f * sqrtf(c[0] * c[0] + c[1] * c[1] + c[2] * c[2]);
			sum[0] += area * (tr->v[0]->x + tr->v[1]->x + tr->v[2]->x) / 3.0f;
			sum[1] += area * (tr->v[0]->y + tr->v[1]->y + tr->v[2]->y) / 3.0f;
			sum[2] += area * (tr->v[0]->z + tr->v[1]->z + tr->v[2]->z) / 3.0f;
			total += area;
		}
		for (k = 0; k < 3; k++) {
			if (total > 0.0f)
				fr->com[i][step][k] = sum[k] / total;
			else
				fr->com[i][step][k] = step ? fr->com[i][step - 1][k] : 0.0f;
		}
	}
}

/* Which way a flame licks from each vertex of piece i: see the flow in ship_death_fracture.  In
 * each triangle the distance to the torn edge climbs along its gradient, so the way out across the
 * tear is against it; that and the face's normal, summed over the triangles round each vertex,
 * weighted by their area, and normalised. */
static void measure_flow(struct ship_death_fracture *fr, int i)
{
	struct mesh *m = fr->piece[i].m;
	float *flow;
	int j, k;

	flow = calloc((size_t) m->nvertices * 6, sizeof(*flow));
	fr->flow[i] = flow;
	if (!flow)
		return;
	for (j = 0; j < m->ntriangles; j++) {
		struct triangle *tr = &m->t[j];
		union vec3 p0, e1, e2, n, a, b, grad;
		float twice_area;

		vec3_init(&p0, tr->v[0]->x, tr->v[0]->y, tr->v[0]->z);
		vec3_init(&e1, tr->v[1]->x, tr->v[1]->y, tr->v[1]->z);
		vec3_init(&e2, tr->v[2]->x, tr->v[2]->y, tr->v[2]->z);
		vec3_sub_self(&e1, &p0);
		vec3_sub_self(&e2, &p0);
		vec3_cross(&n, &e1, &e2);
		twice_area = vec3_magnitude(&n);
		if (twice_area <= 0.0f)
			continue;
		vec3_mul_self(&n, 1.0f / twice_area);
		/* The gradient of w over the triangle, times twice its area -- which is the weight
		 * wanted anyway:  (w1 - w0) (e2 x n) + (w2 - w0) (n x e1). */
		vec3_cross(&a, &e2, &n);
		vec3_mul_self(&a, tr->v[1]->w - tr->v[0]->w);
		vec3_cross(&b, &n, &e1);
		vec3_mul_self(&b, tr->v[2]->w - tr->v[0]->w);
		vec3_add(&grad, &a, &b);
		for (k = 0; k < 3; k++) {
			float *f = &flow[(tr->v[k] - m->v) * 6];

			f[0] -= grad.v.x;
			f[1] -= grad.v.y;
			f[2] -= grad.v.z;
			f[3] += n.v.x * twice_area;
			f[4] += n.v.y * twice_area;
			f[5] += n.v.z * twice_area;
		}
	}
	for (j = 0; j < m->nvertices * 2; j++) {
		float *f = &flow[j * 3];
		float len = sqrtf(f[0] * f[0] + f[1] * f[1] + f[2] * f[2]);

		for (k = 0; k < 3; k++)
			f[k] = len > 0.0f ? f[k] / len : 0.0f;
	}
}

/* The vertex list being sorted, for by_edge_distance().  qsort offers no context pointer. */
static const struct mesh *sorting;

static int by_edge_distance(const void *a, const void *b)
{
	float wa = sorting->v[*(const int *) a].w;
	float wb = sorting->v[*(const int *) b].w;

	return (wa > wb) - (wa < wb);
}

int ship_death_fracture_build(struct ship_death_fracture *fr, const struct mesh *ship,
				uint32_t seed)
{
	struct ship_death_wreck_tuning *tu = &ship_death_wreck_tuning;
	struct mesh_fracture_params params;
	float total = 0.0f, largest = 0.0f;
	int i;

	memset(fr, 0, sizeof(*fr));
	params.npieces = (int) (tu->pieces + 0.5f);
	params.core_fraction = tu->core;
	params.jaggedness = tu->jaggedness;
	params.grain = tu->grain;
	params.seed = seed;
	fr->npieces = mesh_fracture(ship, &params, fr->piece);

	material_init_wreck(&fr->material);
	if (ship->material && ship->material->type == MATERIAL_TEXTURE_MAPPED)
		fr->material.wreck.texture_id = ship->material->texture_mapped.texture_id;
	fr->material.wreck.hull_radius = ship->radius;

	for (i = 0; i < fr->npieces; i++) {
		total += fr->piece[i].area;
		fr->total_triangles += fr->piece[i].m->ntriangles;
	}
	fr->core_share = fr->npieces && total > 0.0f ? fr->piece[0].area / total : 0.0f;
	for (i = 0; i < fr->npieces; i++) {
		struct mesh *m = fr->piece[i].m;
		int j;

		fr->reach[i] = 0.0f;
		for (j = 0; j < m->nvertices; j++)
			fr->reach[i] = fmaxf(fr->reach[i], m->v[j].w);
		fr->order[i] = malloc(sizeof(*fr->order[i]) * m->nvertices);
		if (!fr->order[i])
			continue;
		for (j = 0; j < m->nvertices; j++)
			fr->order[i][j] = j;
		sorting = m;
		qsort(fr->order[i], m->nvertices, sizeof(*fr->order[i]), by_edge_distance);
	}
	for (i = 0; i < fr->npieces; i++) {
		measure_centres(fr, i);
		measure_flow(fr, i);
	}
	for (i = 0; i < fr->npieces; i++)
		if (!fr->piece[i].is_core)
			largest = fmaxf(largest, sqrtf(fr->piece[i].area));
	for (i = 0; i < fr->npieces; i++)
		fr->size[i] = largest > 0.0f ? sqrtf(fr->piece[i].area) / largest : 1.0f;
	return fr->npieces > 0 ? 0 : -1;
}

/* The centre of what is left of piece i when it has burned the fraction f, in its mesh's
 * coordinates, from the steps measure_centres() took. */
static void centre_at(const struct ship_death_fracture *fr, int i, float f, union vec3 *c)
{
	float x = fminf(fmaxf(f, 0.0f), 1.0f) * COM_SAMPLES;
	int a = (int) x;
	int b = a < COM_SAMPLES ? a + 1 : a;
	float u = x - (float) a;

	vec3_init(c, fr->com[i][a][0] + u * (fr->com[i][b][0] - fr->com[i][a][0]),
			fr->com[i][a][1] + u * (fr->com[i][b][1] - fr->com[i][a][1]),
			fr->com[i][a][2] + u * (fr->com[i][b][2] - fr->com[i][a][2]));
}

/* ---------------------------------------------------------------------------------------------
 * THE WRECK: one death of a fractured ship.
 */

static float frand(struct mtwist_state *mt, float lo, float hi)
{
	return lo + (hi - lo) * mtwist_float(mt);
}

static void random_unit(struct mtwist_state *mt, union vec3 *v)
{
	float z = frand(mt, -1.0f, 1.0f);
	float phi = frand(mt, 0.0f, 2.0f * M_PI);
	float rxy = sqrtf(1.0f - z * z);

	vec3_init(v, rxy * cosf(phi), rxy * sinf(phi), z);
}

/* Each piece's flight, drawn from the wreck's seed so the same break flies the same way. */
void ship_death_wreck_plan(struct ship_death_wreck *w, uint32_t seed)
{
	struct ship_death_wreck_tuning *tu = &ship_death_wreck_tuning;
	const struct ship_death_fracture *fr = w->fracture;
	struct mtwist_state *mt = mtwist_init(seed * 2246822519u + 0x5bd1);
	float reference = 0.25f * w->fireball->ship_radius;
	int i;

	w->seed = seed;
	if (!mt)
		return;
	for (i = 0; i < fr->npieces; i++) {
		struct ship_death_piece_motion *pm = &w->motion[i];
		float size = sqrtf(fr->piece[i].area);
		float relative = size > 0.0f ? reference / size : 1.0f;
		union vec3 out, scatter;

		random_unit(mt, &scatter);
		random_unit(mt, &pm->axis);
		if (fr->piece[i].is_core) {
			/* Pushed from every side at once: only a small drift, any way at all. */
			pm->dir = scatter;
			pm->speed = tu->core_drift;
			pm->life = 0.0f;
		} else {
			vec3_init(&out, fr->piece[i].offset[0], fr->piece[i].offset[1],
					fr->piece[i].offset[2]);
			if (vec3_magnitude(&out) > 0.0f)
				vec3_normalize_self(&out);
			vec3_mul_self(&scatter, tu->scatter);
			vec3_add(&pm->dir, &out, &scatter);
			if (vec3_magnitude(&pm->dir) < 0.001f)
				pm->dir = scatter;
			vec3_normalize_self(&pm->dir);
			pm->speed = fminf(tu->speed * powf(relative, tu->size_law), 1.0f) *
					frand(mt, 0.8f, 1.2f);
			pm->life = 0.0f;	/* by rank, below */
		}
		pm->spin = tu->spin * relative * relative * frand(mt, 0.5f, 1.5f);
	}

	/* The chunks' lives, spread evenly between the smallest and the largest by their rank in
	 * size, so they end one at a time however alike they are, each jittered by a fraction of
	 * the spacing so the rhythm is not too even. */
	{
		int rank[MAX_PIECES], nchunks = 0, j, r;
		float spacing;

		for (i = 0; i < fr->npieces; i++)
			if (!fr->piece[i].is_core)
				rank[nchunks++] = i;
		/* Smallest first; an insertion sort, there are never more than a few dozen. */
		for (j = 1; j < nchunks; j++) {
			int x = rank[j];

			for (r = j - 1; r >= 0 && fr->piece[rank[r]].area > fr->piece[x].area; r--)
				rank[r + 1] = rank[r];
			rank[r + 1] = x;
		}
		spacing = nchunks > 1 ? (tu->life - tu->life_min) / (float) (nchunks - 1) : 0.0f;
		for (j = 0; j < nchunks; j++) {
			float life = nchunks > 1 ? tu->life_min + spacing * (float) j : tu->life;

			life += spacing * tu->life_jitter * frand(mt, -0.5f, 0.5f);
			w->motion[rank[j]].life = fmaxf(life, tu->burn_start + 1.0f);
		}
	}
	mtwist_free(mt);
}

int ship_death_wreck_init(struct ship_death_wreck *w, const struct ship_death_fireball *fireball,
				const struct ship_death_fracture *fracture, uint32_t seed)
{
	w->fireball = fireball;
	w->fracture = fracture;
	w->orientation = identity_quat;
	w->core_pose = NULL;
	w->core_cookie = NULL;
	w->core_drawn = 0;
	w->particles_generation = 0;
	w->nembers = 0;
	w->nflying = 0;
	material_init_particles(&w->particles_material);
	w->particles = particle_batch_new(PARTICLE_MAX);
	if (!w->particles)
		return -1;
	ship_death_wreck_plan(w, seed);
	return 0;
}

struct material *ship_death_wreck_core_material(struct ship_death_wreck *w)
{
	int i;

	if (!w->core_drawn)
		return NULL;
	for (i = 0; i < w->fracture->npieces; i++)
		if (w->fracture->piece[i].is_core)
			return &w->piece_material[i];
	return NULL;
}

void ship_death_fracture_cold_material(const struct ship_death_fracture *fr, struct material *m)
{
	struct ship_death_wreck_tuning *tu = &ship_death_wreck_tuning;

	*m = fr->material;
	m->wreck.interior = tu->interior;
	m->wreck.scorch = tu->scorch;
	m->wreck.edge_width = tu->edge_width;
	m->wreck.edge_brightness = tu->edge_brightness;
	m->wreck.edge_temp = 0.0f;
	m->wreck.preheat = tu->preheat;
}

void ship_death_wreck_fini(struct ship_death_wreck *w)
{
	particle_batch_free(w->particles);
	w->particles = NULL;
}

/* How far piece i has burned at time t, 0 to 1: nothing before the burn starts, all of it at the
 * end of its life, fastest at first.  See the note at life_min in the tuning. */
static float burn_progress(const struct ship_death_wreck *w, int i, float t)
{
	struct ship_death_wreck_tuning *tu = &ship_death_wreck_tuning;
	float span = w->motion[i].life - tu->burn_start;
	float s;

	if (w->fracture->piece[i].is_core || span <= 0.0f)
		return 0.0f;
	s = fminf(fmaxf((t - tu->burn_start) / span, 0.0f), 1.0f);
	return 1.0f - powf(1.0f - s, tu->burn_ease);
}

/* The time at which piece i has burned the fraction f: burn_progress() turned round. */
static float burn_time(const struct ship_death_wreck *w, int i, float f)
{
	struct ship_death_wreck_tuning *tu = &ship_death_wreck_tuning;
	float span = w->motion[i].life - tu->burn_start;

	return tu->burn_start + span * (1.0f - powf(1.0f - f, 1.0f / tu->burn_ease));
}

/* How hard piece i is burning at time t, as a fraction of how hard it started: its front's pace,
 * times how much of it is left to burn -- 1 at the start, dwindling to 0 as the last of it goes.
 * Dims the burning front's glow, as smoke_share() tapers its smoke. */
static float burn_rate(const struct ship_death_wreck *w, int i, float t)
{
	struct ship_death_wreck_tuning *tu = &ship_death_wreck_tuning;
	float span = w->motion[i].life - tu->burn_start;
	float s;

	if (w->fracture->piece[i].is_core || span <= 0.0f || t < tu->burn_start)
		return 0.0f;
	s = fminf((t - tu->burn_start) / span, 1.0f);
	return powf(1.0f - s, tu->burn_ease - 1.0f) * (1.0f - burn_progress(w, i, t));
}

#define COM_STEPS 48

/* Where piece i's mesh origin is at time t, and which way up, in motion mode: so that a vertex v
 * of it is at pos + orientation v.  Returns 0 once a chunk's life is over; the core never ends.
 *
 * TURNING ABOUT WHAT IS LEFT.  An intact piece coasts with its centre of mass on the path the gas
 * gave it, turning about that centre.  Burning material leaves without a push, so it jolts nothing
 * -- but what is left has its own centre, moved toward whatever survives, and a free body turns
 * about its own centre and carries that centre on in a straight line.  So as the centre moves by
 * dc (in the piece's frame, turned into the world's by the orientation at that moment), the path
 * gains dc, and from then on the drift that point had from the turning: spin x dc, for the rest of
 * the time.  The sum of those over the burn so far, taken in COM_STEPS steps, is what is added to
 * the gas's path -- and the piece turns about the centre it has now.  Worked out from the clock
 * alone, like everything else here. */
static int piece_pose(const struct ship_death_wreck *w, int i, float t, union vec3 *pos,
			union quat *orientation)
{
	struct ship_death_wreck_tuning *tu = &ship_death_wreck_tuning;
	const struct ship_death_fracture *fr = w->fracture;
	const struct ship_death_piece_motion *pm = &w->motion[i];
	float travel = ship_death_fireball_radius_at_time(w->fireball, t) -
			ship_death_fireball_radius_at_time(w->fireball, 0.0f);
	union vec3 out, now_com, turned;
	union quat turned_q;

	if (fr->piece[i].is_core && w->core_pose) {
		/* The derelict, where the game says it is: its origin is the ship's. */
		if (!w->core_pose(w->core_cookie, t, &out, orientation))
			return 0;
		vec3_init(&turned, fr->piece[i].offset[0], fr->piece[i].offset[1],
				fr->piece[i].offset[2]);
		quat_rot_vec(pos, &turned, orientation);
		vec3_add_self(pos, &out);
		return 1;
	}
	if (!fr->piece[i].is_core && t >= pm->life)
		return 0;
	/* Worked out in the ship's own frame, about the fireball's centre, and turned into the
	 * world's at the end. */
	/* The centre of mass's path, as if nothing had burned: where the whole piece's centroid --
	 * the mesh's origin -- would be. */
	vec3_init(pos, fr->piece[i].offset[0], fr->piece[i].offset[1], fr->piece[i].offset[2]);
	vec3_mul(&out, &pm->dir, pm->speed * travel);
	vec3_add_self(pos, &out);
	quat_init_axis_v(orientation, &pm->axis, pm->spin * t);

	if (!fr->piece[i].is_core && t > tu->burn_start) {
		float s0 = tu->burn_start, s1 = fminf(t, pm->life);
		union vec3 before, after, dc, spun;
		union quat then_q;
		int k;

		centre_at(fr, i, burn_progress(w, i, s0), &before);
		for (k = 1; k <= COM_STEPS; k++) {
			float sa = s0 + (s1 - s0) * (float) (k - 1) / COM_STEPS;
			float sb = s0 + (s1 - s0) * (float) k / COM_STEPS;
			float mid = 0.5f * (sa + sb);

			centre_at(fr, i, burn_progress(w, i, sb), &after);
			vec3_sub(&dc, &after, &before);
			before = after;
			quat_init_axis_v(&then_q, &pm->axis, pm->spin * mid);
			quat_rot_vec_self(&dc, &then_q);
			vec3_add_self(pos, &dc);
			/* The drift that point had from the turning: spin (axis x dc), for the time
			 * since. */
			vec3_cross(&spun, &pm->axis, &dc);
			vec3_mul_self(&spun, pm->spin * (t - mid));
			vec3_add_self(pos, &spun);
		}
		/* pos is now the centre of what is left; put the mesh's origin where that makes the
		 * piece's own centre land on it. */
		centre_at(fr, i, burn_progress(w, i, t), &now_com);
		quat_rot_vec(&turned, &now_com, orientation);
		vec3_sub_self(pos, &turned);
	}
	quat_rot_vec_self(pos, &w->orientation);
	vec3_add_self(pos, &w->fireball->pos);
	quat_mul(&turned_q, &w->orientation, orientation);
	*orientation = turned_q;
	return 1;
}

/* A hash of three small integers to 0..1, for the stateless puffs: the same puff gets the same
 * numbers every frame. */
static float puff_random(const struct ship_death_wreck *w, int piece_index, int channel, int k,
			int which)
{
	uint32_t h = (uint32_t) piece_index * 0x9e3779b1u ^ (uint32_t) channel * 0x85ebca6bu ^
			(uint32_t) k * 0xc2b2ae35u ^ (uint32_t) which * 0x27d4eb2fu ^
			w->seed * 0x165667b1u;

	h ^= h >> 15;
	h *= 0x2c1b3c6du;
	h ^= h >> 12;
	h *= 0x297a2d39u;
	h ^= h >> 15;
	return (float) (h & 0xffffff) / (float) 0x1000000;
}

/* Which vertex of piece i a thing comes off: one at the front -- front hull radii in from the torn
 * edge, within a band of a few percent of the piece's vertices, chosen by the key (vchannel, vk)
 * so that things sharing a key share a spot. */
static int front_vertex(const struct ship_death_wreck *w, int i, int vchannel, int vk, float front)
{
	const struct ship_death_fracture *fr = w->fracture;
	struct mesh *m = fr->piece[i].m;
	int nv = m->nvertices, lo = 0, hi = nv, mid, band, vi;

	/* JUST INSIDE THE BURNING EDGE, where there is still metal.  The shader roughens the front
	 * by up to three tenths of its depth either way, so a vertex barely past the nominal front
	 * may already be gone: search from a little ahead of it.  And never wrap round the list --
	 * its start is the original cut, long since burned away, and smoke from there comes out of
	 * nothing.  Once the front is past nearly everything, take from the last few survivors. */
	front = front > 0.0f ? front * 1.3f + 0.002f : 0.0f;
	while (lo < hi) {
		mid = (lo + hi) / 2;
		if (m->v[fr->order[i][mid]].w < front)
			lo = mid + 1;
		else
			hi = mid;
	}
	band = nv / 20 + 1;
	if (lo > nv - band)
		lo = nv - band > 0 ? nv - band : 0;
	vi = lo + (int) (puff_random(w, i, vchannel, vk, 0) * band);
	return fr->order[i][vi < nv ? vi : nv - 1];
}

/* Where on piece i a thing comes off, at the moment born: front_vertex()'s vertex.  Gives its
 * place then, its velocity then, and the way out from the piece's middle through it.  Returns 0 if
 * the piece was already gone by then. */
static int source_point(const struct ship_death_wreck *w, int i, int vchannel, int vk, float born,
			float front, union vec3 *at, union vec3 *piece_vel, union vec3 *outward)
{
	const struct ship_death_fracture *fr = w->fracture;
	struct mesh *m = fr->piece[i].m;
	union vec3 then_pos, later_pos, local;
	union quat then_q, later_q;
	int vi;

	if (!fr->order[i] || m->nvertices == 0 || !piece_pose(w, i, born, &then_pos, &then_q))
		return 0;
	vi = front_vertex(w, i, vchannel, vk, front);

	vec3_init(&local, m->v[vi].x, m->v[vi].y, m->v[vi].z);
	quat_rot_vec(at, &local, &then_q);
	vec3_add_self(at, &then_pos);

	/* The VERTEX's own velocity then, from where it is a moment later: with the piece turning
	 * about a centre that moves as it burns, the mesh origin's velocity is not the spot's. */
	if (piece_pose(w, i, born + 0.05f, &later_pos, &later_q)) {
		union vec3 later_at;

		quat_rot_vec(&later_at, &local, &later_q);
		vec3_add_self(&later_at, &later_pos);
		vec3_sub(piece_vel, &later_at, at);
		vec3_mul_self(piece_vel, 20.0f);
	} else {
		vec3_init(piece_vel, 0.0f, 0.0f, 0.0f);
	}

	/* Out from the middle of what is left of the piece, through the vertex. */
	{
		union vec3 com, com_world;

		centre_at(fr, i, burn_progress(w, i, born), &com);
		quat_rot_vec(&com_world, &com, &then_q);
		vec3_add_self(&com_world, &then_pos);
		vec3_sub(outward, at, &com_world);
	}
	if (vec3_magnitude(outward) > 0.0f)
		vec3_normalize_self(outward);
	else
		vec3_init(outward, 0.0f, 1.0f, 0.0f);
	return 1;
}

static float smoothstepf(float lo, float hi, float x)
{
	float u = fminf(fmaxf((x - lo) / (hi - lo), 0.0f), 1.0f);

	return u * u * (3.0f - 2.0f * u);
}

/* A direction mostly out from the piece, scattered by spread, from the thing's hash. */
static void scattered(const struct ship_death_wreck *w, int i, int channel, int k,
			const union vec3 *outward, float spread, union vec3 *dir)
{
	vec3_init(dir, puff_random(w, i, channel, k, 1) - 0.5f,
			puff_random(w, i, channel, k, 2) - 0.5f,
			puff_random(w, i, channel, k, 3) - 0.5f);
	vec3_mul_self(dir, spread);
	vec3_add_self(dir, outward);
	if (vec3_magnitude(dir) > 0.0f)
		vec3_normalize_self(dir);
}

/* Puff k of piece i's smoke on one channel, born at time born off the vertices front hull radii
 * in from the torn edge, glowing at warm_temp when new; added to the batch if it is still alive
 * at time t.  vk picks the spot on the front (puffs sharing it come from one place).  burst is 1
 * for a venting pop's smoke and 0 for the steady burn's: see emit_pops().  Returns 0 once the
 * batch is full. */
static int emit_puff(struct ship_death_wreck *w, int i, int channel, int k, int vk, float born,
			float front, float warm_temp, float burst, float t)
{
	struct ship_death_wreck_tuning *tu = &ship_death_wreck_tuning;
	float hull = w->fracture->material.wreck.hull_radius;
	float age = t - born, size, r0, r, fade;
	union vec3 at, vel, drift, outward;
	struct particle *p;

	if (age < 0.0f || age >= tu->smoke_life)
		return 1;
	if (!source_point(w, i, channel, vk, born, front, &at, &vel, &outward))
		return 1;
	vec3_mul_self(&vel, tu->smoke_carry);
	/* Its own drift: mostly out from the piece's middle, where the gas is coming from. */
	scattered(w, i, channel, k, &outward, 0.8f, &drift);
	vec3_mul_self(&drift, (tu->smoke_drift + 0.25f * tu->pop_jet * burst) * hull);
	vec3_add_self(&vel, &drift);
	vec3_mul_self(&vel, age);
	vec3_add_self(&at, &vel);

	size = 0.7f + 0.6f * puff_random(w, i, channel, k, 5);
	r0 = tu->smoke_size * hull * size;
	r = r0 + tu->smoke_spread * (1.0f + 3.0f * burst) * hull * size * age;
	/* Thinning as it spreads, a moment to come out of the flame, and the last of it gone by the
	 * end of its life. */
	fade = powf(r0 / r, tu->smoke_thinning + (2.0f - tu->smoke_thinning) * burst) *
			fminf(age / 0.1f, 1.0f) *
			(1.0f - smoothstepf(0.6f, 1.0f, age / tu->smoke_life));

	p = particle_batch_add(w->particles);
	if (!p)
		return 0;
	p->kind = PARTICLE_SMOKE;
	p->pos = at;
	p->opacity = tu->smoke_opacity * fade * (channel == SMOKE_SMOULDER ? 0.6f : 1.0f);
	p->temperature = warm_temp;
	/* DARK SMOKE.  It glows with the heat it came off only for its first instant, at the
	 * source; after that it is soot, and the embers and the burning front do the glowing.
	 * Smoke tinted orange all through reads as coloured fog, not as anything burning. */
	p->emission = 0.25f * fmaxf(1.0f - age / 0.15f, 0.0f);
	p->seed = puff_random(w, i, channel, k, 4);
	p->radius = r;
	return 1;
}

/* Ember k of piece i: a flake of hot hull breaking loose from the burning front at time born,
 * flying off with some of the piece's velocity and a push of its own, tumbling, cooling from
 * yellow-orange to dark, and shrinking away at the end.  speed is its push, hull radii a second.
 * Returns 0 once the pool or the frame is full. */
static int emit_ember(struct ship_death_wreck *w, struct ship_death_frame *f, int i, int channel,
			int k, int vk, float born, float front, float speed, float t)
{
	struct ship_death_wreck_tuning *tu = &ship_death_wreck_tuning;
	float hull = w->fracture->material.wreck.hull_radius;
	float age = t - born, remaining, scale;
	union vec3 at, vel, push, outward, axis;
	struct material *em;
	struct ship_death_drawable *d;

	if (w->nembers >= SHIP_DEATH_EMBER_MAX)
		return 0;
	if (age < 0.0f || age >= tu->ember_life)
		return 1;
	if (!source_point(w, i, channel + 16, vk, born, front, &at, &vel, &outward))
		return 1;
	vec3_mul_self(&vel, 0.8f);
	scattered(w, i, channel + 16, k, &outward, 1.2f, &push);
	vec3_mul_self(&push, speed * hull * (0.5f + puff_random(w, i, channel + 16, k, 5)));
	vec3_add_self(&vel, &push);
	vec3_mul_self(&vel, age);
	vec3_add_self(&at, &vel);

	scale = tu->ember_size * hull * (0.4f + 0.8f * puff_random(w, i, channel + 16, k, 6));
	remaining = tu->ember_life - age;
	if (remaining < 0.3f * tu->ember_life)
		scale *= remaining / (0.3f * tu->ember_life);

	em = &w->ember_material[w->nembers];
	material_init_shrapnel(em);
	em->shrapnel.temperature = 1700.0f *
			(0.85f + 0.3f * puff_random(w, i, channel + 16, k, 7)) *
			expf(-age / (0.5f * tu->ember_life));
	em->shrapnel.brightness = 4.0f;
	d = ship_death_frame_add(f, ember_mesh[k % EMBER_MESHES], em, &at, scale);
	if (!d)
		return 0;
	vec3_init(&axis, puff_random(w, i, channel + 16, k, 8) - 0.5f,
			puff_random(w, i, channel + 16, k, 9) - 0.5f, 0.3f);
	quat_init_axis_v(&d->orientation, &axis, (8.0f + 12.0f *
				puff_random(w, i, channel + 16, k, 10)) * age);
	d->no_cast_shadow = 1;
	w->nembers++;
	return 1;
}

/* Spark k of piece i: a speck of burning metal thrown off the front at time born with most of the
 * piece's velocity and a push of its own, speed hull radii a second, cooling as it goes.  Into
 * the batch as a streak along its motion.  Returns 0 once the batch is full. */
static int emit_spark(struct ship_death_wreck *w, int i, int channel, int k, int vk, float born,
			float front, float speed, float t)
{
	struct ship_death_wreck_tuning *tu = &ship_death_wreck_tuning;
	float hull = w->fracture->material.wreck.hull_radius;
	float age = t - born, heat, fade;
	union vec3 at, vel, push, outward;
	struct particle *p;

	if (age < 0.0f || age >= tu->spark_life)
		return 1;
	if (!source_point(w, i, channel + 48, vk, born, front, &at, &vel, &outward))
		return 1;
	vec3_mul_self(&vel, 0.8f);
	scattered(w, i, channel + 48, k, &outward, 1.2f, &push);
	vec3_mul_self(&push, speed * hull * (0.5f + puff_random(w, i, channel + 48, k, 5)));
	vec3_add_self(&vel, &push);

	p = particle_batch_add(w->particles);
	if (!p)
		return 0;
	p->kind = PARTICLE_SPARK;
	vec3_mul(&p->streak, &vel, tu->spark_shutter);
	vec3_mul_self(&vel, age);
	vec3_add(&p->pos, &at, &vel);
	heat = expf(-age / (0.4f * tu->spark_life));
	fade = 1.0f - smoothstepf(0.7f, 1.0f, age / tu->spark_life);
	p->radius = tu->spark_size * hull * (0.6f + 0.8f * puff_random(w, i, channel + 48, k, 6));
	p->opacity = 0.0f;
	p->emission = tu->spark_brightness * heat * fade;
	p->temperature = 2400.0f * (0.85f + 0.3f * puff_random(w, i, channel + 48, k, 7)) *
			(0.45f + 0.55f * heat);
	p->seed = puff_random(w, i, channel + 48, k, 8);
	return 1;
}

/* Tongue k of piece i's flame on one channel, born at time born off the front where it was then;
 * drawn at time t, when the piece is at pos and turned by q, if it is still alive.  big scales it
 * and lift carries it off the front, hull radii a second: both for a pop, and 1 and 0 for the
 * steady burn.  Returns 0 once the batch is full.
 *
 * ROOTED.  A flame is not a thing flying off; it is gas burning as it leaves, and what moves is the
 * gas, not the flame.  So a tongue stays where it was lit, on the front, swelling and dying away in
 * place, and the licking outward is the shader's: its noise runs from root to tip.  Tongues from
 * one stretch of edge lean the same way -- the way that vertex leans -- so they read together as a
 * sheet of fire rather than as sparks fanning out. */
static int emit_flame(struct ship_death_wreck *w, int i, int channel, int k, int vk, float born,
			float front, float big, float lift, float t, const union vec3 *pos,
			const union quat *q)
{
	struct ship_death_wreck_tuning *tu = &ship_death_wreck_tuning;
	const struct ship_death_fracture *fr = w->fracture;
	float hull = fr->material.wreck.hull_radius;
	float age = t - born, life = tu->flame_life * big, a, envelope, radius, len, side;
	const float *f;
	union vec3 at, dir, lean, scatter;
	struct particle *p;
	int vi;

	if (age < 0.0f || age >= life || !fr->order[i] || !fr->flow[i] ||
			fr->piece[i].m->nvertices == 0)
		return 1;
	a = age / life;
	vi = front_vertex(w, i, channel, vk, front);
	f = &fr->flow[i][vi * 6];
	/* Out across the tear, leaning off whichever face this vertex leans off, barely astray. */
	side = puff_random(w, i, SMOKE_FLAME + 64, vi, 1) < 0.5f ? -0.35f : 0.35f;
	vec3_init(&dir, f[0] + side * f[3], f[1] + side * f[4], f[2] + side * f[5]);
	vec3_init(&scatter, puff_random(w, i, channel, k, 2) - 0.5f,
			puff_random(w, i, channel, k, 3) - 0.5f,
			puff_random(w, i, channel, k, 4) - 0.5f);
	vec3_mul_self(&scatter, 0.25f);
	vec3_add_self(&dir, &scatter);
	if (vec3_magnitude(&dir) <= 0.0f)
		return 1;
	vec3_normalize_self(&dir);
	quat_rot_vec_self(&dir, q);

	vec3_init(&at, fr->piece[i].m->v[vi].x, fr->piece[i].m->v[vi].y, fr->piece[i].m->v[vi].z);
	quat_rot_vec_self(&at, q);
	vec3_add_self(&at, pos);

	/* Swells and dies away smoothly, so no tongue is seen to start or stop. */
	envelope = sinf((float) M_PI * a);
	big *= fmaxf(sqrtf(fr->size[i]), 0.35f);
	radius = tu->flame_size * big * hull * (0.7f + 0.6f * puff_random(w, i, channel, k, 5)) *
			(0.5f + 0.5f * envelope);
	len = tu->flame_length * big * hull * (0.6f + 0.8f * puff_random(w, i, channel, k, 6)) *
			(0.3f + 0.7f * envelope);
	/* The base on the front, the tongue reaching out past it. */
	vec3_mul(&lean, &dir, lift * hull * age + 0.5f * len);
	vec3_add_self(&at, &lean);

	p = particle_batch_add(w->particles);
	if (!p)
		return 0;
	p->kind = PARTICLE_FLAME;
	p->pos = at;
	vec3_mul(&p->streak, &dir, len);
	p->radius = radius;
	p->opacity = 0.12f;
	p->emission = tu->flame_brightness * envelope;
	p->temperature = 2000.0f;
	p->seed = puff_random(w, i, channel, k, 7);
	w->piece_nflames[i]++;
	vec3_add_self(&w->piece_fire_at[i], &at);
	return 1;
}

/* When a piece's edges cool below smoke_smoulder_temp, from the same fall the edges' heat takes in
 * ship_death_wreck_draw(). */
static float smoulder_end(void)
{
	struct ship_death_wreck_tuning *tu = &ship_death_wreck_tuning;

	if (tu->edge_temp <= tu->smoke_smoulder_temp || tu->edge_temp <= 0.0f)
		return 0.0f;
	return tu->edge_cooling * logf(tu->edge_temp / tu->smoke_smoulder_temp);
}

/* The derelict's smoulder: a steady trickle off its glowing tears, from the flash until they cool.
 * A bigger piece has more edge, and more smoke.  The fire hides it at first anyway. */
static void emit_smoulder(struct ship_death_wreck *w, int i, float rate, float t)
{
	struct ship_death_wreck_tuning *tu = &ship_death_wreck_tuning;
	float end = fminf(smoulder_end(), t);
	float warm = fmaxf(tu->edge_temp * expf(-fmaxf(t, 0.0f) / tu->edge_cooling), 1000.0f);
	int k, first, last;

	if (rate <= 0.0f || end <= 0.0f)
		return;
	first = (int) ceilf(fmaxf(t - tu->smoke_life, 0.0f) * rate);
	last = (int) floorf(end * rate);
	for (k = first; k <= last; k++)
		if (!emit_puff(w, i, SMOKE_SMOULDER, k, k, (float) k / rate, 0.0f, warm, 0.0f, t))
			return;
}

/* The share of a chunk's smoke that has come off by the time it has burned the fraction f.  How much
 * smoke a burning edge gives goes with how much edge is left burning, which shrinks with the piece:
 * so the smoke tapers to nothing as the last of it goes, even where the front itself keeps up its
 * pace, rather than stopping at a stroke.  The embers follow the same share. */
static float smoke_share(float f)
{
	return 1.0f - (1.0f - f) * (1.0f - f);
}

/* smoke_share() turned round: the burn's progress at a given share. */
static float share_progress(float share)
{
	return 1.0f - sqrtf(fmaxf(1.0f - share, 0.0f));
}

/* A chunk burning away, over its whole life: its smoke and its embers, off the front.  The k-th of
 * each is born when the burn has given off its k-th share of them -- the rate given a second at the
 * start, tapering to nothing as the last of the piece goes -- off the front where it was then. */
static void emit_burn(struct ship_death_wreck *w, struct ship_death_frame *fd, int i, float t)
{
	struct ship_death_wreck_tuning *tu = &ship_death_wreck_tuning;
	const struct ship_death_fracture *fr = w->fracture;
	float span = w->motion[i].life - tu->burn_start;
	float reach = fr->reach[i] * 1.3f + 0.01f;
	float total, share_then, share_now;
	union vec3 pos;
	union quat q;
	int k, first, last;

	if (fr->piece[i].is_core || span <= 0.0f || t < tu->burn_start)
		return;
	share_now = smoke_share(burn_progress(w, i, t));

	/* So that the first come at the rate given: d(share)/dt starts at 2 ease / span. */
	total = tu->smoke_burn_rate * span / (2.0f * tu->burn_ease);
	share_then = smoke_share(burn_progress(w, i, t - tu->smoke_life));
	first = (int) ceilf(share_then * total);
	last = (int) floorf(share_now * total);
	for (k = first; k <= last; k++) {
		float f = share_progress((float) k / total);

		if (!emit_puff(w, i, SMOKE_BURN, k, k, burn_time(w, i, f), f * reach, 1400.0f, 0.0f,
				t))
			break;
	}

	/* The flames, where the piece is now: they stay on the front as it turns. */
	if (piece_pose(w, i, t, &pos, &q)) {
		total = tu->flame_rate * fr->size[i] * span / (2.0f * tu->burn_ease);
		share_then = smoke_share(burn_progress(w, i, t - tu->flame_life));
		first = (int) ceilf(share_then * total);
		last = (int) floorf(share_now * total);
		for (k = first; k <= last; k++) {
			float f = share_progress((float) k / total);

			if (!emit_flame(w, i, SMOKE_FLAME, k, k, burn_time(w, i, f), f * reach, 1.0f,
					0.0f, t, &pos, &q))
				break;
		}
	}

	total = tu->ember_rate * span / (2.0f * tu->burn_ease);
	share_then = smoke_share(burn_progress(w, i, t - tu->ember_life));
	first = (int) ceilf(share_then * total);
	last = (int) floorf(share_now * total);
	for (k = first; k <= last; k++) {
		float f = share_progress((float) k / total);

		if (!emit_ember(w, fd, i, SMOKE_BURN, k, k, burn_time(w, i, f), f * reach,
				tu->ember_speed, t))
			break;
	}

	total = tu->spark_rate * fr->size[i] * span / (2.0f * tu->burn_ease);
	share_then = smoke_share(burn_progress(w, i, t - tu->spark_life));
	first = (int) ceilf(share_then * total);
	last = (int) floorf(share_now * total);
	for (k = first; k <= last; k++) {
		float f = share_progress((float) k / total);

		if (!emit_spark(w, i, SMOKE_BURN, k, k, burn_time(w, i, f), f * reach,
				tu->spark_speed, t))
			break;
	}
}

/* When piece i's j-th candidate pop goes off, or a negative time if it does not.
 *
 * The pockets that vent are found early: a pop's chance goes with how hard the piece is burning
 * at the time, so they come thick at the start of the burn and die away with it, leaving the
 * last of a piece to burn down quietly. */
static float pop_time(const struct ship_death_wreck *w, int i, int j)
{
	struct ship_death_wreck_tuning *tu = &ship_death_wreck_tuning;
	float when = tu->burn_start + tu->pop_interval *
			((float) j + 0.5f + 0.8f * (puff_random(w, i, SMOKE_POP, j, 20) - 0.5f));

	if (puff_random(w, i, SMOKE_POP, j, 21) >= tu->pop_chance * burn_rate(w, i, when))
		return -1.0f;
	return when;
}

/* The flare of piece i's pops at time t, to add to its burning front's glow: each spikes and
 * dies away in about a tenth of a second. */
static float pop_flare(const struct ship_death_wreck *w, int i, float t)
{
	struct ship_death_wreck_tuning *tu = &ship_death_wreck_tuning;
	float flare = 0.0f;
	int j, last;

	if (w->fracture->piece[i].is_core || tu->pop_interval <= 0.0f)
		return 0.0f;
	last = (int) ((t - tu->burn_start) / tu->pop_interval) + 1;
	for (j = 0; j <= last; j++) {
		float when = pop_time(w, i, j);

		if (when < 0.0f || when > t || when >= w->motion[i].life)
			continue;
		flare += 1.5f * expf(-(t - when) / 0.1f);
	}
	return flare;
}

/* Piece i's pops still to be seen at time t: each a gout of flame, a burst of smoke and a burst
 * of faster embers, all from one spot on the front where it was when it went off.
 *
 * THE SMOKE BALLOONS WHERE IT IS.  Gas venting into a vacuum does not hold together as a cloud and
 * sail off; it spreads out from the vent as fast as it leaves it.  So a pop's puffs barely drift,
 * spread several times as fast as the steady burn's and thin as a lone cloud does: a ball of smoke
 * swelling at the spot and gone in a second or so.  Sent out on the jet instead, as they once
 * were, they flew off together as a little formation of tidy clouds, which nothing real does. */
static void emit_pops(struct ship_death_wreck *w, struct ship_death_frame *fd, int i, float t)
{
	struct ship_death_wreck_tuning *tu = &ship_death_wreck_tuning;
	float reach = w->fracture->reach[i] * 1.3f + 0.01f;
	float oldest = t - fmaxf(fmaxf(tu->smoke_life, tu->ember_life), tu->spark_life);
	union vec3 pos;
	union quat q;
	int j, m, first, last, have_pose;

	if (w->fracture->piece[i].is_core || tu->pop_interval <= 0.0f)
		return;
	have_pose = piece_pose(w, i, t, &pos, &q);
	first = (int) floorf((oldest - tu->burn_start) / tu->pop_interval) - 1;
	last = (int) ((t - tu->burn_start) / tu->pop_interval) + 1;
	for (j = first < 0 ? 0 : first; j <= last; j++) {
		float when = pop_time(w, i, j);
		float front;

		if (when < 0.0f || when > t || when >= w->motion[i].life)
			continue;
		front = burn_progress(w, i, when) * reach;
		/* A gout of flame from the one spot, bigger than the steady burn's, and a pop is a
		 * jet: this once, the flame is carried out with it. */
		for (m = 0; have_pose && m < POP_FLAMES; m++)
			if (!emit_flame(w, i, SMOKE_POP + 32, j * POP_FLAMES + m, j,
					when + 0.02f * m, front, 1.5f, tu->pop_jet * 0.5f, t,
					&pos, &q))
				return;
		for (m = 0; m < POP_PUFFS; m++)
			if (!emit_puff(w, i, SMOKE_POP, j * POP_PUFFS + m, j, when + 0.02f * m,
					front, 1500.0f, 1.0f, t))
				return;
		for (m = 0; m < (int) tu->pop_embers; m++)
			if (!emit_ember(w, fd, i, SMOKE_POP, j * 64 + m, j, when, front,
					tu->ember_speed * 3.0f, t))
				return;
		for (m = 0; m < (int) tu->pop_sparks; m++)
			if (!emit_spark(w, i, SMOKE_POP, j * 256 + m, j, when, front,
					tu->spark_speed * 2.0f, t))
				return;
	}
}

/* Each burning piece lit by its own fire, from the middle of this frame's flames on it, as bright
 * as the burn and flickering with it, and wrapped a little since the fire runs all along the edge.
 * The fireball's light, while it lasts, is far the stronger, and wins. */
static void light_by_fire(struct ship_death_wreck *w, struct ship_death_frame *fd, float t)
{
	struct ship_death_wreck_tuning *tu = &ship_death_wreck_tuning;
	int i;

	if (tu->flame_light <= 0.0f)
		return;
	for (i = 0; i < w->fracture->npieces; i++) {
		struct ship_death_drawable *d;
		float r, g, b, glow, flicker, old;

		if (w->piece_drawable[i] < 0 || w->piece_nflames[i] == 0)
			continue;
		d = &fd->d[w->piece_drawable[i]];
		flicker = 0.8f + 0.12f * sinf(23.0f * t + 1.7f * (float) i) +
				0.08f * sinf(37.0f * t + 4.1f * (float) i);
		glow = tu->flame_light * (burn_rate(w, i, t) + pop_flare(w, i, t)) * flicker;
		old = d->aux_light_color[0] + d->aux_light_color[1] + d->aux_light_color[2];
		star_light_blackbody_color(1700.0f, &r, &g, &b);
		if (glow * (r + g + b) <= old)
			continue;
		vec3_mul(&d->aux_light_pos, &w->piece_fire_at[i], 1.0f / (float) w->piece_nflames[i]);
		d->aux_light_color[0] = r * glow;
		d->aux_light_color[1] = g * glow;
		d->aux_light_color[2] = b * glow;
		d->aux_light_wrap = 0.5f;
	}
}

static void draw_smoke(struct ship_death_wreck *w, float t, const struct ship_death_view *view,
			struct ship_death_frame *fd)
{
	struct ship_death_wreck_tuning *tu = &ship_death_wreck_tuning;
	const struct ship_death_fracture *fr = w->fracture;
	float largest = 0.0f;
	union vec3 origin;
	struct ship_death_drawable *d;
	int i;

	w->nembers = 0;
	for (i = 0; i < fr->npieces; i++) {
		w->piece_nflames[i] = 0;
		vec3_init(&w->piece_fire_at[i], 0.0f, 0.0f, 0.0f);
	}
	if (!w->particles)
		return;
	particle_batch_clear(w->particles);
	if (tu->mode != SHIP_DEATH_WRECK_MOTION)
		return;
	for (i = 0; i < fr->npieces; i++)
		largest = fmaxf(largest, sqrtf(fr->piece[i].area));
	for (i = 0; i < fr->npieces; i++) {
		if (fr->piece[i].is_core) {
			emit_smoulder(w, i, tu->smoke_smoulder_rate *
					(largest > 0.0f ? sqrtf(fr->piece[i].area) / largest : 1.0f), t);
		} else {
			emit_burn(w, fd, i, t);
			emit_pops(w, fd, i, t);
		}
	}
	light_by_fire(w, fd, t);
	if (w->particles->n == 0)
		return;
	/* Whatever is inside the fireball's smoke, behind its gas. */
	for (i = 0; i < w->particles->n; i++) {
		float through = ship_death_fireball_transmittance(w->fireball, &view->eye,
								&w->particles->p[i].pos);

		w->particles->p[i].opacity *= through;
		w->particles->p[i].emission *= through;
	}
	particle_batch_build(w->particles, &view->eye, &view->forward, &view->up);
	w->particles_material.particles.albedo = tu->smoke_albedo;
	/* Wrapped as the wreck's own is; see ship_death_wreck_draw(). */
	w->particles_material.particles.time = fmodf(fmaxf(t, 0.0f), 1000.0f);
	vec3_init(&origin, 0.0f, 0.0f, 0.0f);
	d = ship_death_frame_add(fd, w->particles->m, &w->particles_material, &origin, 1.0f);
	if (!d)
		return;
	/* Never zero, which would mean a mesh that never changes. */
	w->particles_generation = w->particles_generation + 1 ? w->particles_generation + 1 : 1;
	d->mesh_generation = w->particles_generation;
	d->no_shade = 1;
	d->no_cast_shadow = 1;
}

void ship_death_wreck_draw(struct ship_death_wreck *w, float t, const struct ship_death_view *view,
				struct ship_death_frame *fd)
{
	struct ship_death_wreck_tuning *tu = &ship_death_wreck_tuning;
	const struct ship_death_fracture *fr = w->fracture;
	float ship_radius = w->fireball->ship_radius;
	int i;

	w->material = fr->material;
	w->material.wreck.interior = tu->interior;
	w->material.wreck.scorch = tu->scorch;
	w->material.wreck.edge_width = tu->edge_width;
	w->material.wreck.edge_brightness = tu->edge_brightness;
	w->material.wreck.edge_temp = tu->edge_temp * expf(-fmaxf(t, 0.0f) / tu->edge_cooling);
	w->nflying = 0;
	w->core_drawn = 0;
	for (i = 0; i < fr->npieces; i++)
		w->piece_drawable[i] = -1;
	for (i = 0; i < fr->npieces; i++) {
		struct ship_death_drawable *d;
		union vec3 pos, out, to_camera;
		union quat orientation = identity_quat;
		float dissolve = 0.0f, distance;

		if (tu->mode == SHIP_DEATH_WRECK_EXPLODED) {
			/* Straight out from the ship's middle, the core left where it was. */
			vec3_init(&pos, fr->piece[i].offset[0], fr->piece[i].offset[1],
					fr->piece[i].offset[2]);
			if (!fr->piece[i].is_core && vec3_magnitude(&pos) > 0.0f) {
				vec3_normalize(&out, &pos);
				vec3_mul_self(&out, tu->explode * ship_radius);
				vec3_add_self(&pos, &out);
			}
			quat_rot_vec_self(&pos, &w->orientation);
			vec3_add_self(&pos, &w->fireball->pos);
			orientation = w->orientation;
		} else {
			/* Carried by the gas: its own fraction of the edge's travel, so it is still
			 * at the flash, is swept up in the blast and then coasts. */
			if (!piece_pose(w, i, t, &pos, &orientation))
				continue;
			/* A little past the furthest point, so the last speck goes too. */
			dissolve = burn_progress(w, i, t) * (fr->reach[i] * 1.3f + 0.01f);
		}
		if (!fr->piece[i].is_core) {
			vec3_sub(&to_camera, &view->eye, &pos);
			distance = vec3_magnitude(&to_camera);
			if (distance > 0.0f && 2.0f * fr->piece[i].m->radius * view->pixels_per_unit /
					distance < tu->min_pixels)
				continue;
		}
		w->piece_material[i] = w->material;
		w->piece_material[i].wreck.dissolve = dissolve;
		w->piece_material[i].wreck.burn_glow = burn_rate(w, i, t) + pop_flare(w, i, t);
		w->piece_material[i].wreck.preheat = tu->preheat;
		/* Wrapped, so the flicker's noise coordinate keeps its precision however long it
		 * runs; the jump once in a thousand seconds is a flicker like any other. */
		w->piece_material[i].wreck.time = fmodf(fmaxf(t, 0.0f), 1000.0f) + (float) i * 17.0f;
		if (fr->piece[i].is_core && w->core_pose) {
			/* The game draws the derelict; it takes this material, lit by the fire. */
			struct material_wreck *cm = &w->piece_material[i].wreck;
			union vec3 light_pos;

			vec3_init(&light_pos, 0.0f, 0.0f, 0.0f);
			memset(cm->aux_light_color, 0, sizeof(cm->aux_light_color));
			cm->aux_light_wrap = 0.0f;
			ship_death_fireball_light(w->fireball, &pos, &light_pos, cm->aux_light_color,
						&cm->aux_light_wrap);
			cm->aux_light_pos[0] = light_pos.v.x;
			cm->aux_light_pos[1] = light_pos.v.y;
			cm->aux_light_pos[2] = light_pos.v.z;
			w->core_drawn = 1;
			continue;
		}
		d = ship_death_frame_add(fd, fr->piece[i].m, &w->piece_material[i], &pos, 1.0f);
		if (!d)
			break;
		d->orientation = orientation;
		ship_death_fireball_light(w->fireball, &d->pos, &d->aux_light_pos, d->aux_light_color,
					&d->aux_light_wrap);
		w->piece_drawable[i] = (int) (d - fd->d);
		w->nflying++;
	}
	draw_smoke(w, t, view, fd);
}

float ship_death_wreck_end(const struct ship_death_wreck *w)
{
	struct ship_death_wreck_tuning *tu = &ship_death_wreck_tuning;
	float end = smoulder_end();
	int i;

	for (i = 0; i < w->fracture->npieces; i++)
		if (!w->fracture->piece[i].is_core)
			end = fmaxf(end, w->motion[i].life);
	return end + fmaxf(tu->smoke_life, fmaxf(tu->ember_life, fmaxf(tu->spark_life,
				tu->flame_life)));
}
