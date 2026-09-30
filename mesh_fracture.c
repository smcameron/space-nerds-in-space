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
#include <stdio.h>
#include <math.h>

#include "vertex.h"
#include "triangle.h"
#include "mtwist.h"
#include "quat.h"
#include "snis_graph.h"
#include "material.h"
#include "mesh.h"

#define DEFINE_MESH_FRACTURE_GLOBALS
#include "mesh_fracture.h"

/* One triangle of the hull, in the working form: positions, normals and texture coordinates
 * by value, so subdividing is only arithmetic and no vertex has to be shared. */
struct soup_tri {
	float p[3][3];
	float n[3][3];
	float uv[3][2];
	float centroid[3];	/* where it is judged from, after the tearing noise */
	float area;
	int cell;
	float edge[3];		/* each corner's distance to its cell's nearest tear */
};

struct soup {
	struct soup_tri *t;
	int n, cap;
};

static int soup_add(struct soup *s, const struct soup_tri *t)
{
	if (s->n == s->cap) {
		int cap = s->cap ? s->cap * 2 : 1024;
		struct soup_tri *nt = realloc(s->t, sizeof(*nt) * cap);

		if (!nt)
			return -1;
		s->t = nt;
		s->cap = cap;
	}
	s->t[s->n++] = *t;
	return 0;
}

static float dist2(const float *a, const float *b)
{
	float dx = a[0] - b[0], dy = a[1] - b[1], dz = a[2] - b[2];

	return dx * dx + dy * dy + dz * dz;
}

static uint32_t hash3(int32_t x, int32_t y, int32_t z, uint32_t seed);

/* Where along an edge to split it, 0.3 to 0.7 of the way from whichever end sorts first.  An
 * exact halving turns a big flat panel into an even grid of equal triangles, and a tear through
 * an even grid is an even sawtooth -- pinking shears, not a tear.  Hashed from the endpoints so
 * it is deterministic, and ordered by them so the triangles either side of a shared edge would
 * pick the same point. */
static float split_ratio(const float *a, const float *b, uint32_t seed, int *swap)
{
	int32_t qa[3], qb[3];
	uint32_t h;
	int k;

	for (k = 0; k < 3; k++) {
		qa[k] = (int32_t) lrintf(a[k] * 4096.0f);
		qb[k] = (int32_t) lrintf(b[k] * 4096.0f);
	}
	*swap = qa[0] > qb[0] || (qa[0] == qb[0] && (qa[1] > qb[1] ||
			(qa[1] == qb[1] && qa[2] > qb[2])));
	if (*swap)
		h = hash3(qb[0] ^ (qa[0] * 31), qb[1] ^ (qa[1] * 37), qb[2] ^ (qa[2] * 41), seed);
	else
		h = hash3(qa[0] ^ (qb[0] * 31), qa[1] ^ (qb[1] * 37), qa[2] ^ (qb[2] * 41), seed);
	return 0.3f + 0.4f * (float) (h & 0xffff) / 65535.0f;
}

/* Split the triangle across its longest edge until no edge is longer than max_edge.  Splitting
 * the longest edge, rather than splitting in four, keeps slivers from getting thinner. */
static int subdivide(struct soup *s, const struct soup_tri *t, float max_edge2, int depth,
			uint32_t seed)
{
	struct soup_tri a, b;
	float e[3], f;
	int i, k, j0, j1, j2, swap;

	e[0] = dist2(t->p[0], t->p[1]);
	e[1] = dist2(t->p[1], t->p[2]);
	e[2] = dist2(t->p[2], t->p[0]);
	i = 0;
	if (e[1] > e[i])
		i = 1;
	if (e[2] > e[i])
		i = 2;
	if (e[i] <= max_edge2 || depth > 24)
		return soup_add(s, t);

	/* The longest edge runs from j0 to j1; j2 is the vertex opposite it. */
	j0 = i;
	j1 = (i + 1) % 3;
	j2 = (i + 2) % 3;
	a = *t;
	b = *t;
	/* f is the fraction of the way from j0 to j1. */
	f = split_ratio(t->p[j0], t->p[j1], seed, &swap);
	if (swap)
		f = 1.0f - f;
	for (k = 0; k < 3; k++) {
		float mp = t->p[j0][k] + f * (t->p[j1][k] - t->p[j0][k]);
		float mn = t->n[j0][k] + f * (t->n[j1][k] - t->n[j0][k]);

		a.p[j1][k] = mp;
		a.n[j1][k] = mn;
		b.p[j0][k] = mp;
		b.n[j0][k] = mn;
	}
	for (k = 0; k < 2; k++) {
		float muv = t->uv[j0][k] + f * (t->uv[j1][k] - t->uv[j0][k]);

		a.uv[j1][k] = muv;
		b.uv[j0][k] = muv;
	}
	(void) j2;
	if (subdivide(s, &a, max_edge2, depth + 1, seed))
		return -1;
	return subdivide(s, &b, max_edge2, depth + 1, seed);
}

/* Integer hashing, so the noise is the same on every machine and compiler: the fracture must
 * come out identically on every bridge screen. */
static uint32_t hash3(int32_t x, int32_t y, int32_t z, uint32_t seed)
{
	uint32_t h = seed * 0x9e3779b1u;

	h ^= (uint32_t) x * 0x85ebca6bu;
	h = (h << 13) | (h >> 19);
	h ^= (uint32_t) y * 0xc2b2ae35u;
	h = (h << 17) | (h >> 15);
	h ^= (uint32_t) z * 0x27d4eb2fu;
	h ^= h >> 16;
	h *= 0x7feb352du;
	h ^= h >> 15;
	h *= 0x846ca68bu;
	h ^= h >> 16;
	return h;
}

static float smooth(float t)
{
	return t * t * (3.0f - 2.0f * t);
}

/* Value noise, -1..1. */
static float value_noise(float x, float y, float z, uint32_t seed)
{
	int32_t ix = (int32_t) floorf(x), iy = (int32_t) floorf(y), iz = (int32_t) floorf(z);
	float fx = smooth(x - ix), fy = smooth(y - iy), fz = smooth(z - iz);
	float c[8];
	int i;

	for (i = 0; i < 8; i++)
		c[i] = (float) (hash3(ix + (i & 1), iy + ((i >> 1) & 1), iz + ((i >> 2) & 1), seed) &
				0xffffff) / (float) 0x800000 - 1.0f;
	return (1 - fz) * ((1 - fy) * ((1 - fx) * c[0] + fx * c[1]) +
				fy * ((1 - fx) * c[2] + fx * c[3])) +
		fz * ((1 - fy) * ((1 - fx) * c[4] + fx * c[5]) +
			fy * ((1 - fx) * c[6] + fx * c[7]));
}

/* Push a point about by noise: where it is judged from, which is what turns a straight Voronoi
 * boundary into a tear.  Three scales, each doing something the others cannot:
 *
 *	broad	the tear's course across the hull, so it does not run straight
 *	medium	its kinks, a quarter of the hull across
 *	fine	a couple of triangles across, so that neighbouring teeth along the tear are not
 *		all the same size -- without it the edge is a staircase of equal steps
 *
 * The broad and medium ones go by the hull's radius and jaggedness; the fine one by the grain,
 * since it is the triangles it is breaking up, and it goes when jaggedness does so that zero
 * still means a straight cut. */
static void tear(const float *p, float radius, float jaggedness, float grain_len, uint32_t seed,
			float *out)
{
	float amplitude = jaggedness * radius;
	/* Kept under a triangle's own size: much more and it flips single triangles across a
	 * boundary, leaving pinholes in one piece and specks of it riding on another. */
	float fine = jaggedness > 0.0f ? 0.6f * grain_len : 0.0f;
	int k;

	for (k = 0; k < 3; k++) {
		uint32_t s = seed + (uint32_t) k * 101u;
		float broad = 1.5f / radius, medium = 4.0f / radius, tiny = 0.5f / grain_len;

		out[k] = p[k] +
			amplitude * (0.5f * value_noise(p[0] * broad, p[1] * broad, p[2] * broad, s) +
				0.5f * value_noise(p[0] * medium, p[1] * medium, p[2] * medium,
							s + 7u)) +
			fine * value_noise(p[0] * tiny, p[1] * tiny, p[2] * tiny, s + 13u);
	}
}

static void assign(struct soup *s, const float seed_pos[][3], int nseeds, float core_weight)
{
	int i, j;

	for (i = 0; i < s->n; i++) {
		float best = dist2(s->t[i].centroid, seed_pos[0]) - core_weight;
		int cell = 0;

		for (j = 1; j < nseeds; j++) {
			float d = dist2(s->t[i].centroid, seed_pos[j]);

			if (d < best) {
				best = d;
				cell = j;
			}
		}
		s->t[i].cell = cell;
	}
}

/* How far each corner of each triangle is from the nearest tear of its own cell.
 *
 * A power diagram's boundaries are planes: the power distances to two seeds, |p - s|^2 less the
 * seed's weight, differ by a linear function of p.  So a point's distance to the boundary between
 * its own cell c and another j is (D_j - D_c) / (2 |s_j - s_c|), and its distance to the nearest
 * tear is the least of those.  Judged from the same torn position the cells were, so it follows
 * the ragged edge, not the flat plane.  Negative -- a corner beyond the boundary of a triangle
 * assigned by its centroid -- counts as on it. */
static void measure_edges(struct soup *s, const float seed_pos[][3], int nseeds, float core_weight,
			float radius, const struct mesh_fracture_params *params)
{
	int i, j, k;

	for (i = 0; i < s->n; i++) {
		struct soup_tri *t = &s->t[i];
		int c = t->cell;

		for (k = 0; k < 3; k++) {
			float q[3], own, nearest = 1e30f;

			tear(t->p[k], radius, params->jaggedness, params->grain * radius, params->seed,
				q);
			own = dist2(q, seed_pos[c]) - (c == 0 ? core_weight : 0.0f);
			for (j = 0; j < nseeds; j++) {
				float other, gap, cosine, sine, len, nrm[3];
				int m;

				if (j == c)
					continue;
				gap = sqrtf(dist2(seed_pos[j], seed_pos[c]));
				if (gap <= 0.0f)
					continue;
				other = dist2(q, seed_pos[j]) - (j == 0 ? core_weight : 0.0f);
				/* That is the straight distance to the boundary's plane.  What the
				 * shading wants is the distance ACROSS THE HULL to where the plane cuts
				 * it, which is greater by 1/sin of the angle between the plane and the
				 * hull there: where a boundary runs nearly along a flat panel, the
				 * straight distance is small over the whole panel though the tear is
				 * nowhere near, and the panel would glow as one broad blob. */
				len = sqrtf(t->n[k][0] * t->n[k][0] + t->n[k][1] * t->n[k][1] +
						t->n[k][2] * t->n[k][2]);
				cosine = 0.0f;
				if (len > 0.0f) {
					for (m = 0; m < 3; m++)
						nrm[m] = (seed_pos[j][m] - seed_pos[c][m]) / gap;
					cosine = (nrm[0] * t->n[k][0] + nrm[1] * t->n[k][1] +
							nrm[2] * t->n[k][2]) / len;
				}
				sine = sqrtf(fmaxf(1.0f - cosine * cosine, 1e-4f));
				nearest = fminf(nearest, (other - own) / (2.0f * gap) / sine);
			}
			t->edge[k] = fmaxf(nearest, 0.0f);
		}
	}
}

static float cell_area(const struct soup *s, int cell)
{
	float a = 0.0f;
	int i;

	for (i = 0; i < s->n; i++)
		if (s->t[i].cell == cell)
			a += s->t[i].area;
	return a;
}

static void triangle_area_and_centroid(struct soup_tri *t)
{
	float u[3], v[3], c[3];
	int k;

	for (k = 0; k < 3; k++) {
		u[k] = t->p[1][k] - t->p[0][k];
		v[k] = t->p[2][k] - t->p[0][k];
		t->centroid[k] = (t->p[0][k] + t->p[1][k] + t->p[2][k]) / 3.0f;
	}
	c[0] = u[1] * v[2] - u[2] * v[1];
	c[1] = u[2] * v[0] - u[0] * v[2];
	c[2] = u[0] * v[1] - u[1] * v[0];
	t->area = 0.5f * sqrtf(c[0] * c[0] + c[1] * c[1] + c[2] * c[2]);
}

/* Build one piece's mesh from the soup triangles in cell, centred on their area-weighted
 * centroid. */
static struct mesh *build_piece(const struct mesh *src, const struct soup *s, int cell,
				float *offset, float *area, int defer_upload)
{
	struct mesh *m;
	float cx = 0, cy = 0, cz = 0, total = 0;
	int i, n = 0, k, j;

	for (i = 0; i < s->n; i++) {
		const struct soup_tri *t = &s->t[i];
		float c[3];

		if (t->cell != cell)
			continue;
		n++;
		for (k = 0; k < 3; k++)
			c[k] = (t->p[0][k] + t->p[1][k] + t->p[2][k]) / 3.0f;
		cx += c[0] * t->area;
		cy += c[1] * t->area;
		cz += c[2] * t->area;
		total += t->area;
	}
	if (n == 0 || total <= 0.0f)
		return NULL;
	offset[0] = cx / total;
	offset[1] = cy / total;
	offset[2] = cz / total;
	*area = total;

	m = calloc(1, sizeof(*m));
	if (!m)
		return NULL;
	m->geometry_mode = MESH_GEOMETRY_TRIANGLES;
	m->nvertices = n * 3;
	m->ntriangles = n;
	m->v = calloc(m->nvertices, sizeof(*m->v));
	m->t = calloc(m->ntriangles, sizeof(*m->t));
	if (src->tex)
		m->tex = calloc(m->ntriangles * 3, sizeof(*m->tex));
	if (!m->v || !m->t || (src->tex && !m->tex)) {
		free(m->v);
		free(m->t);
		free(m->tex);
		free(m);
		return NULL;
	}
	n = 0;
	for (i = 0; i < s->n; i++) {
		const struct soup_tri *t = &s->t[i];
		struct triangle *mt;

		if (t->cell != cell)
			continue;
		mt = &m->t[n];
		for (j = 0; j < 3; j++) {
			struct vertex *v = &m->v[n * 3 + j];
			float len;

			v->x = t->p[j][0] - offset[0];
			v->y = t->p[j][1] - offset[1];
			v->z = t->p[j][2] - offset[2];
			/* The distance to the nearest tear, in hull radii.  See mesh_fracture.h. */
			v->w = t->edge[j] / (src->radius > 0.0f ? src->radius : 1.0f);
			mt->v[j] = v;
			len = sqrtf(t->n[j][0] * t->n[j][0] + t->n[j][1] * t->n[j][1] +
					t->n[j][2] * t->n[j][2]);
			if (len <= 0.0f)
				len = 1.0f;
			mt->vnormal[j].x = t->n[j][0] / len;
			mt->vnormal[j].y = t->n[j][1] / len;
			mt->vnormal[j].z = t->n[j][2] / len;
			if (m->tex) {
				m->tex[n * 3 + j].u = t->uv[j][0];
				m->tex[n * 3 + j].v = t->uv[j][1];
			}
		}
		n++;
	}
	/* The face normal, which some shading reads; the vertex normals above are the source's
	 * own, interpolated, so the hull keeps its smoothing across the break. */
	for (i = 0; i < m->ntriangles; i++) {
		struct triangle *t = &m->t[i];
		float u[3], v[3], c[3], len;

		u[0] = t->v[1]->x - t->v[0]->x;
		u[1] = t->v[1]->y - t->v[0]->y;
		u[2] = t->v[1]->z - t->v[0]->z;
		v[0] = t->v[2]->x - t->v[0]->x;
		v[1] = t->v[2]->y - t->v[0]->y;
		v[2] = t->v[2]->z - t->v[0]->z;
		c[0] = u[1] * v[2] - u[2] * v[1];
		c[1] = u[2] * v[0] - u[0] * v[2];
		c[2] = u[0] * v[1] - u[1] * v[0];
		len = sqrtf(c[0] * c[0] + c[1] * c[1] + c[2] * c[2]);
		if (len <= 0.0f)
			len = 1.0f;
		t->n.x = c[0] / len;
		t->n.y = c[1] / len;
		t->n.z = c[2] / len;
	}
	mesh_set_reasonable_tangents_and_bitangents(m);
	/* NOT the source's material: mesh_free() frees a mesh's material, so sharing the pointer would
	 * free it once per piece and again with the ship.  A piece is drawn with a material of the
	 * caller's choosing anyway. */
	m->material = NULL;
	m->radius = mesh_compute_radius(m);
	snprintf(m->name, sizeof(m->name), "%.200s piece %d", src->name, cell);
	if (!defer_upload)
		mesh_graph_dev_init(m);
	return m;
}

int mesh_fracture(const struct mesh *src, const struct mesh_fracture_params *params,
			struct mesh_fracture_piece *piece)
{
	struct soup s = { 0 };
	struct mtwist_state *mt;
	float (*seed_pos)[3];
	float total_area = 0.0f, radius, lo, hi, core_weight = 0.0f;
	float hull_centroid[3] = { 0, 0, 0 };
	int npieces = params->npieces < 2 ? 2 : params->npieces;
	int i, j, iter, count = 0;

	if (!src || src->ntriangles <= 0)
		return 0;
	radius = src->radius > 0.0f ? src->radius : 1.0f;

	/* The source into the soup, subdivided to the grain. */
	for (i = 0; i < src->ntriangles; i++) {
		struct soup_tri t;

		memset(&t, 0, sizeof(t));
		for (j = 0; j < 3; j++) {
			t.p[j][0] = src->t[i].v[j]->x;
			t.p[j][1] = src->t[i].v[j]->y;
			t.p[j][2] = src->t[i].v[j]->z;
			t.n[j][0] = src->t[i].vnormal[j].x;
			t.n[j][1] = src->t[i].vnormal[j].y;
			t.n[j][2] = src->t[i].vnormal[j].z;
			if (src->tex) {
				t.uv[j][0] = src->tex[i * 3 + j].u;
				t.uv[j][1] = src->tex[i * 3 + j].v;
			}
		}
		if (subdivide(&s, &t, params->grain * radius * params->grain * radius, 0,
				params->seed)) {
			free(s.t);
			return 0;
		}
	}

	/* Where each small triangle is judged from: its centroid, torn about by the noise. */
	for (i = 0; i < s.n; i++) {
		float c[3];

		triangle_area_and_centroid(&s.t[i]);
		for (j = 0; j < 3; j++) {
			c[j] = s.t[i].centroid[j];
			hull_centroid[j] += c[j] * s.t[i].area;
		}
		total_area += s.t[i].area;
		tear(c, radius, params->jaggedness, params->grain * radius, params->seed,
			s.t[i].centroid);
	}
	if (total_area <= 0.0f) {
		free(s.t);
		return 0;
	}
	for (j = 0; j < 3; j++)
		hull_centroid[j] /= total_area;

	/* The seeds.  The core's is the middle of the hull; the debris seeds are dropped on the
	 * hull itself, area weighted, so the pieces are spread over the skin rather than bunched
	 * where the triangles happen to be small. */
	seed_pos = calloc(npieces, sizeof(*seed_pos));
	mt = mtwist_init(params->seed);
	if (!seed_pos || !mt) {
		free(seed_pos);
		if (mt)
			mtwist_free(mt);
		free(s.t);
		return 0;
	}
	for (j = 0; j < 3; j++)
		seed_pos[0][j] = hull_centroid[j];
	for (i = 1; i < npieces; i++) {
		float pick = mtwist_float(mt) * total_area;
		int k = 0;

		while (k < s.n - 1 && pick > s.t[k].area) {
			pick -= s.t[k].area;
			k++;
		}
		for (j = 0; j < 3; j++)
			seed_pos[i][j] = s.t[k].centroid[j];
	}
	mtwist_free(mt);

	/* The core's weight, by bisection, so that it takes core_fraction of the area.  Its cell
	 * only grows as the weight does, so this converges; forty halvings of a range some
	 * sixteen radii squared wide is far finer than any triangle. */
	lo = -4.0f * radius * radius;
	hi = 16.0f * radius * radius;
	for (iter = 0; iter < 40; iter++) {
		core_weight = 0.5f * (lo + hi);
		assign(&s, (const float (*)[3]) seed_pos, npieces, core_weight);
		if (cell_area(&s, 0) < params->core_fraction * total_area)
			lo = core_weight;
		else
			hi = core_weight;
	}
	assign(&s, (const float (*)[3]) seed_pos, npieces, core_weight);
	measure_edges(&s, (const float (*)[3]) seed_pos, npieces, core_weight, radius, params);
	free(seed_pos);

	for (i = 0; i < npieces; i++) {
		struct mesh *m = build_piece(src, &s, i, piece[count].offset, &piece[count].area,
						params->defer_upload);

		if (!m) {
			if (i == 0)
				break;	/* no core, no wreck */
			continue;
		}
		piece[count].m = m;
		piece[count].is_core = (i == 0);
		count++;
	}
	free(s.t);
	return count;
}
