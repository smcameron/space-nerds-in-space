#include "vertex.h"
#include "mesh.h"
#include "mathutils.h"
#include "matrix.h"

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

