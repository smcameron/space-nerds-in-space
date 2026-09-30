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
#include "vertex.h"
#include "triangle.h"
#include "mesh.h"

#define DEFINE_PARTICLE_BATCH_GLOBALS
#include "particle_batch.h"

struct particle_batch *particle_batch_new(int max)
{
	struct particle_batch *b = calloc(1, sizeof(*b));
	struct mesh *m;

	if (!b)
		return NULL;
	b->max = max;
	b->p = calloc((size_t) max, sizeof(*b->p));
	m = calloc(1, sizeof(*m));
	b->m = m;
	if (!b->p || !m)
		goto bail;
	m->geometry_mode = MESH_GEOMETRY_TRIANGLES;
	m->v = calloc((size_t) max * 4, sizeof(*m->v));
	m->t = calloc((size_t) max * 2, sizeof(*m->t));
	m->tex = calloc((size_t) max * 6, sizeof(*m->tex));
	if (!m->v || !m->t || !m->tex)
		goto bail;
	/* Everything is in world space, so there is no bound worth computing: big enough that
	 * nothing culls the batch by its radius. */
	m->radius = 1.0e9f;
	return b;
bail:
	particle_batch_free(b);
	return NULL;
}

void particle_batch_free(struct particle_batch *b)
{
	if (!b)
		return;
	if (b->m)
		mesh_free(b->m);
	free(b->p);
	free(b);
}

void particle_batch_clear(struct particle_batch *b)
{
	b->n = 0;
}

struct particle *particle_batch_add(struct particle_batch *b)
{
	struct particle *p;

	if (b->n >= b->max)
		return NULL;
	p = &b->p[b->n++];
	memset(p, 0, sizeof(*p));
	return p;
}

static int farthest_first(const void *a, const void *b)
{
	float da = ((const struct particle *) a)->depth;
	float db = ((const struct particle *) b)->depth;

	return (da < db) - (da > db);
}

/* One corner: world position, where it sits on the quad, and the particle's look. */
static void corner(struct vertex *v, const struct particle *p, const union vec3 *along, float ha,
			const union vec3 *across, float hc, float su, float sv)
{
	v->x = p->pos.v.x + along->v.x * ha * su + across->v.x * hc * sv;
	v->y = p->pos.v.y + along->v.y * ha * su + across->v.y * hc * sv;
	v->z = p->pos.v.z + along->v.z * ha * su + across->v.z * hc * sv;
	/* The kind in the whole part, the seed in the fraction; the seed kept clear of 1 so it
	 * never carries into the kind. */
	v->w = (float) p->kind + 0.999f * fminf(fmaxf(p->seed, 0.0f), 1.0f);
}

void particle_batch_build(struct particle_batch *b, const union vec3 *eye,
				const union vec3 *forward, const union vec3 *up)
{
	struct mesh *m = b->m;
	union vec3 right, rel;
	int i;

	vec3_cross(&right, forward, up);
	vec3_normalize_self(&right);
	for (i = 0; i < b->n; i++) {
		vec3_sub(&rel, &b->p[i].pos, eye);
		b->p[i].depth = vec3_dot(&rel, forward);
	}
	/* Back to front, for the smoke; the additive kinds do not care. */
	qsort(b->p, (size_t) b->n, sizeof(*b->p), farthest_first);

	for (i = 0; i < b->n; i++) {
		const struct particle *p = &b->p[i];
		struct vertex *v = &m->v[i * 4];
		struct triangle *t = &m->t[i * 2];
		union vec3 along = right, across = *up, flat;
		float ha = p->radius, hc = p->radius, len, look[3];
		int k;

		/* A streak lies along its motion as the camera sees it: take out the part of the
		 * motion toward or away from the eye, and stretch the quad along the rest. */
		vec3_mul(&flat, forward, vec3_dot(&p->streak, forward));
		vec3_sub(&flat, &p->streak, &flat);
		len = vec3_magnitude(&flat);
		if (len > 0.01f * p->radius) {
			vec3_mul(&along, &flat, 1.0f / len);
			vec3_cross(&across, forward, &along);
			vec3_normalize_self(&across);
			ha += 0.5f * len;
		}
		corner(&v[0], p, &along, ha, &across, hc, -1.0f, -1.0f);
		corner(&v[1], p, &along, ha, &across, hc, 1.0f, -1.0f);
		corner(&v[2], p, &along, ha, &across, hc, 1.0f, 1.0f);
		corner(&v[3], p, &along, ha, &across, hc, -1.0f, 1.0f);

		t[0].v[0] = &v[0];
		t[0].v[1] = &v[1];
		t[0].v[2] = &v[2];
		t[1].v[0] = &v[0];
		t[1].v[1] = &v[2];
		t[1].v[2] = &v[3];
		/* In radii from its middle, so a streak's shader can round its ends: -1..1
		 * across, and as far as the stretch goes along. */
		mesh_set_triangle_texture_coords(m, i * 2, -ha / hc, -1.0f, ha / hc, -1.0f,
						ha / hc, 1.0f);
		mesh_set_triangle_texture_coords(m, i * 2 + 1, -ha / hc, -1.0f, ha / hc, 1.0f,
						-ha / hc, 1.0f);
		look[0] = p->opacity;
		look[1] = p->emission;
		look[2] = p->temperature;
		for (k = 0; k < 3; k++) {
			t[0].vnormal[k].x = look[0];
			t[0].vnormal[k].y = look[1];
			t[0].vnormal[k].z = look[2];
			t[1].vnormal[k] = t[0].vnormal[k];
		}
	}
	m->nvertices = b->n * 4;
	m->ntriangles = b->n * 2;
}
