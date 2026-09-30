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

/* mesh_fracture(), on a sphere, for the things that must hold whatever the ship: the pieces
 * account for the whole hull, the core is first and gets its share, every piece put back at its
 * offset lies where the hull was, and the same seed breaks it the same way -- which is what lets
 * every bridge screen agree on a wreck. */
#include <stdio.h>
#include <math.h>
#include <string.h>

#include "vertex.h"
#include "triangle.h"
#include "quat.h"
#include "snis_graph.h"
#include "material.h"
#include "mesh.h"
#include "mesh_fracture.h"

#define MAXP 32

static int failures;

#define CHECK(cond, ...) do { \
	if (!(cond)) { \
		failures++; \
		fprintf(stderr, "FAIL %s:%d: ", __FILE__, __LINE__); \
		fprintf(stderr, __VA_ARGS__); \
		fprintf(stderr, "\n"); \
	} \
} while (0)

static float mesh_area(const struct mesh *m)
{
	float a = 0.0f;
	int i;

	for (i = 0; i < m->ntriangles; i++) {
		const struct triangle *t = &m->t[i];
		float u[3] = { t->v[1]->x - t->v[0]->x, t->v[1]->y - t->v[0]->y,
				t->v[1]->z - t->v[0]->z };
		float v[3] = { t->v[2]->x - t->v[0]->x, t->v[2]->y - t->v[0]->y,
				t->v[2]->z - t->v[0]->z };
		float c[3] = { u[1] * v[2] - u[2] * v[1], u[2] * v[0] - u[0] * v[2],
				u[0] * v[1] - u[1] * v[0] };

		a += 0.5f * sqrtf(c[0] * c[0] + c[1] * c[1] + c[2] * c[2]);
	}
	return a;
}

int main(void)
{
	struct mesh *sphere = mesh_unit_icosphere(3);
	struct mesh_fracture_piece a[MAXP], b[MAXP];
	struct mesh_fracture_params p = {
		.npieces = 8, .core_fraction = 0.6f, .jaggedness = 0.08f, .grain = 0.1f,
		.seed = 1234,
	};
	float hull, sum = 0.0f;
	int na, nb, i, j;

	if (!sphere) {
		fprintf(stderr, "could not build a sphere\n");
		return 1;
	}
	hull = mesh_area(sphere);

	na = mesh_fracture(sphere, &p, a);
	CHECK(na >= 2 && na <= p.npieces, "%d pieces from %d asked", na, p.npieces);
	CHECK(na > 0 && a[0].is_core, "piece 0 is not the core");
	for (i = 1; i < na; i++)
		CHECK(!a[i].is_core, "piece %d claims to be the core too", i);

	for (i = 0; i < na; i++) {
		float area = mesh_area(a[i].m);

		CHECK(fabsf(area - a[i].area) < 1e-3f * hull, "piece %d reports %f, has %f",
			i, a[i].area, area);
		sum += area;
		/* Put back at its offset, every vertex is on the unit sphere it came from --
		 * subdividing puts new vertices on chords, a little inside, never outside. */
		for (j = 0; j < a[i].m->nvertices; j++) {
			float x = a[i].m->v[j].x + a[i].offset[0];
			float y = a[i].m->v[j].y + a[i].offset[1];
			float z = a[i].m->v[j].z + a[i].offset[2];
			float r = sqrtf(x * x + y * y + z * z);

			CHECK(r > 0.9f && r < 1.0001f, "piece %d vertex %d at radius %f", i, j, r);
			if (!(r > 0.9f && r < 1.0001f))
				break;
		}
	}
	/* Each vertex's w is its distance to the nearest tear, in hull radii: never negative,
	 * zero somewhere along every piece's torn edge, and more than that further in. */
	for (i = 0; i < na; i++) {
		float lo = 1e30f, hi = -1e30f;

		for (j = 0; j < a[i].m->nvertices; j++) {
			lo = fminf(lo, a[i].m->v[j].w);
			hi = fmaxf(hi, a[i].m->v[j].w);
		}
		CHECK(lo >= 0.0f, "piece %d has a vertex %f from a tear", i, lo);
		CHECK(lo < 0.02f, "piece %d comes no nearer a tear than %f", i, lo);
		CHECK(hi > lo + 0.02f, "piece %d is all edge, %f to %f", i, lo, hi);
	}
	CHECK(fabsf(sum - hull) < 1e-3f * hull, "pieces cover %f of a hull of %f", sum, hull);
	CHECK(na > 0 && fabsf(a[0].area / hull - p.core_fraction) < 0.03f,
		"core has %.3f of the hull, asked %.3f", na ? a[0].area / hull : 0.0f,
		p.core_fraction);

	nb = mesh_fracture(sphere, &p, b);
	CHECK(na == nb, "same seed, %d pieces then %d", na, nb);
	for (i = 0; i < na && i < nb; i++) {
		CHECK(a[i].m->ntriangles == b[i].m->ntriangles,
			"same seed, piece %d has %d triangles then %d", i, a[i].m->ntriangles,
			b[i].m->ntriangles);
		CHECK(memcmp(a[i].offset, b[i].offset, sizeof(a[i].offset)) == 0,
			"same seed, piece %d moved", i);
	}

	p.seed = 4321;
	nb = mesh_fracture(sphere, &p, b);
	CHECK(nb >= 2, "second seed gave %d pieces", nb);
	CHECK(nb != na || memcmp(a[1].offset, b[1].offset, sizeof(a[1].offset)) != 0,
		"a different seed broke it identically");

	for (i = 0; i < na; i++)
		mesh_free(a[i].m);
	for (i = 0; i < nb; i++)
		mesh_free(b[i].m);
	mesh_free(sphere);
	if (failures) {
		fprintf(stderr, "test_mesh_fracture: %d failures\n", failures);
		return 1;
	}
	printf("test_mesh_fracture: passed\n");
	return 0;
}
