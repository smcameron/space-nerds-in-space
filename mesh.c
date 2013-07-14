#include <stdlib.h>

#include "vertex.h"
#include "triangle.h"
#include "mathutils.h"
#include "matrix.h"

#define DEFINE_MESH_GLOBALS 1
#include "mesh.h"
#undef DEFINE_MESH_GLOBALS

float mesh_compute_radius(struct mesh *m)
{
	int i;
	float r, max_radius = 0.0;

	for (i = 0; i < m->nvertices; i++) {
		r = dist3d(m->v[i].x, m->v[i].y, m->v[i].z);
		if (r > max_radius)
			max_radius = r;
	}
	return max_radius;
}

void mesh_distort(struct mesh *m, float distortion)
{
	int i;

	for (i = 0; i < m->nvertices; i++) {
		float dx, dy, dz;

		dx = (float) snis_randn(1000) / 1000.0 * distortion - 0.5;
		dy = (float) snis_randn(1000) / 1000.0 * distortion - 0.5;
		dz = (float) snis_randn(1000) / 1000.0 * distortion - 0.5;

		m->v[i].x += m->v[i].x * dx;
		m->v[i].y += m->v[i].y * dy;
		m->v[i].z += m->v[i].z * dz;
	}
	m->radius = mesh_compute_radius(m);
}

void mesh_scale(struct mesh *m, float scale)
{
	int i;

	for (i = 0; i < m->nvertices; i++) {
		m->v[i].x += m->v[i].x * scale;
		m->v[i].y += m->v[i].y * scale;
		m->v[i].z += m->v[i].z * scale;
	}
	m->radius = mesh_compute_radius(m);
}

static int lookup_vertex(struct mesh *m, struct vertex *v)
{
	int i;

	for (i = 0; i < m->nvertices; i++)
		if (&m->v[i] == v)
			return i;
	return -1;
}

struct mesh *mesh_duplicate(struct mesh *original)
{
	struct mesh *copy;
	int i;

	copy = malloc(sizeof(*copy));
	copy->ntriangles = original->ntriangles;
	copy->nvertices = original->nvertices;
	copy->t = malloc(sizeof(*copy->t) * copy->ntriangles);
	memset(copy->t, 0, sizeof(*copy->t) * copy->ntriangles); 
	copy->v = malloc(sizeof(*copy->v) * copy->nvertices);
	memset(copy->v, 0, sizeof(*copy->v) * copy->nvertices);

	for (i = 0; i < original->nvertices; i++)
		copy->v[i] = original->v[i];

	for (i = 0; i < original->ntriangles; i++) {
		int v0, v1, v2;

		v0 = lookup_vertex(original, original->t[i].v[0]);
		v1 = lookup_vertex(original, original->t[i].v[1]);
		v2 = lookup_vertex(original, original->t[i].v[2]);

		copy->t[i].v[0] = &copy->v[v0];
		copy->t[i].v[1] = &copy->v[v1];
		copy->t[i].v[2] = &copy->v[v2];
		copy->t[i].n = original->t[i].n; 
	}
	copy->radius = original->radius;
	return copy;
}

