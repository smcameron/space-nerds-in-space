#include <stdlib.h>
#include <string.h>

#include "vertex.h"
#include "triangle.h"
#include "mathutils.h"
#include "matrix.h"
#include "math.h"

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

void mesh_derelict(struct mesh *m, float distortion)
{
	int i;

	for (i = 0; i < m->nvertices; i++) {
		float dx, dy, dz;

		dx = (float) (snis_randn(1000) / 1000.0 - 0.5) * distortion;
		dy = (float) (snis_randn(1000) / 1000.0 - 0.5) * (distortion / 10.0) - 0.5;
		dz = (float) (snis_randn(1000) / 1000.0 - 0.5) * (distortion / 10.0) - 0.5;

		if (m->v[i].x < 0) {
			m->v[i].x = dx;
			m->v[i].y += dy;
			m->v[i].z += dz;
		}
	}
	m->radius = mesh_compute_radius(m);
	for (i = 0; i < m->nvertices; i++)
		m->v[i].x -= m->radius / 2.0;
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
	copy->geometry_mode = original->geometry_mode;
	copy->ntriangles = original->ntriangles;
	copy->nvertices = original->nvertices;
	copy->nlines = original->nlines;
	copy->t = malloc(sizeof(*copy->t) * copy->ntriangles);
	memset(copy->t, 0, sizeof(*copy->t) * copy->ntriangles); 
	copy->v = malloc(sizeof(*copy->v) * copy->nvertices);
	memset(copy->v, 0, sizeof(*copy->v) * copy->nvertices);
	copy->l = malloc(sizeof(*copy->l) * copy->nlines);
	memset(copy->l, 0, sizeof(*copy->l) * copy->nlines);

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

	for (i = 0; i < original->nlines; i++) {
		int v0, v1;

		v0 = lookup_vertex(original, original->l[i].start);
		v1 = lookup_vertex(original, original->l[i].end);

		copy->l[i].start = &copy->v[v0];
		copy->l[i].end = &copy->v[v1];
	}
	copy->radius = original->radius;
	return copy;
}

void mesh_add_point(struct mesh *m, float x, float y, float z)
{
	m->v[m->nvertices].x = x;
	m->v[m->nvertices].y = y;
	m->v[m->nvertices].z = z;
	m->nvertices++;
}

void mesh_add_line_last_2(struct mesh *m, int flag)
{
	m->l[m->nlines].start = &m->v[m->nvertices - 2];
	m->l[m->nlines].end = &m->v[m->nvertices - 1];
	m->l[m->nlines].flag = flag;
	m->nlines++;
}

struct mesh *init_circle_mesh(double x, double z, double r, int npoints, double angle)
{
	int i;
	float increment;
	struct mesh *my_mesh = malloc(sizeof(*my_mesh));

	my_mesh->geometry_mode = MESH_GEOMETRY_LINES;
	my_mesh->nvertices = 0;
	my_mesh->ntriangles = 0;
	my_mesh->nlines = 1;
	my_mesh->t = 0;
	my_mesh->v = malloc(sizeof(*my_mesh->v) * (npoints + 1));
	my_mesh->l = malloc(sizeof(*my_mesh->l) * 1);
	my_mesh->radius = r;

	increment = angle / (float) npoints;
	for (i = 0; i <= npoints; i++) {
		float a = i * increment;
		my_mesh->v[my_mesh->nvertices].x = x + cos(a) * r;
		my_mesh->v[my_mesh->nvertices].y = 0;
		my_mesh->v[my_mesh->nvertices].z = z + sin(a) * r;
		my_mesh->nvertices++;
	}

	/* close the mesh */
	my_mesh->v[my_mesh->nvertices - 1].x = my_mesh->v[0].x;
	my_mesh->v[my_mesh->nvertices - 1].y = my_mesh->v[0].y;
	my_mesh->v[my_mesh->nvertices - 1].z = my_mesh->v[0].z;

	my_mesh->l[0].start = &my_mesh->v[0];
	my_mesh->l[0].end = &my_mesh->v[my_mesh->nvertices - 1];
	my_mesh->l[0].flag = MESH_LINE_STRIP;
	return my_mesh;
}

struct mesh *init_radar_circle_xz_plane_mesh(double x, double z, double r, int ticks, double tick_radius)
{
	int i;
	struct mesh *my_mesh = malloc(sizeof(*my_mesh));

	my_mesh->geometry_mode = MESH_GEOMETRY_LINES;
	my_mesh->nvertices = 0;
	my_mesh->ntriangles = 0;
	my_mesh->nlines = 0;
	my_mesh->t = 0;
	my_mesh->v = malloc(sizeof(*my_mesh->v) * (360 / 2 + 1 + ticks*2));
	my_mesh->l = malloc(sizeof(*my_mesh->l) * (1 + ticks));
	my_mesh->radius = dist3d(x, 0, z) + r;

	for (i = 0; i <= 360; i += 2) {
		my_mesh->v[my_mesh->nvertices].x = x + cos(i * M_PI / 180.0) * r;
		my_mesh->v[my_mesh->nvertices].y = 0;
		my_mesh->v[my_mesh->nvertices].z = z + sin(i * M_PI / 180.0) * r;
		my_mesh->nvertices++;
	}
	my_mesh->l[0].start = &my_mesh->v[0];
	my_mesh->l[0].end = &my_mesh->v[my_mesh->nvertices - 1];
	my_mesh->l[0].flag = MESH_LINE_STRIP;
	my_mesh->nlines++;

	for (i = 0; i < ticks; ++i) {
		double c = cos(i * 2.0 * M_PI / (double)ticks);
		double s = sin(i * 2.0 * M_PI / (double)ticks);
		mesh_add_point(my_mesh, x + c * (r - tick_radius), 0, z + s * (r - tick_radius));
		mesh_add_point(my_mesh, x + c * r, 0, z + s * r);
		mesh_add_line_last_2(my_mesh, MESH_LINE_DOTTED);
	}
	return my_mesh;
}

struct mesh *init_line_mesh(double x1, double y1, double z1, double x2, double y2, double z2)
{
	struct mesh *my_mesh = malloc(sizeof(*my_mesh));

	my_mesh->geometry_mode = MESH_GEOMETRY_LINES;
	my_mesh->nvertices = 2;
	my_mesh->ntriangles = 0;
	my_mesh->nlines = 1;
	my_mesh->t = 0;
	my_mesh->v = malloc(sizeof(*my_mesh->v) * 2);
	my_mesh->l = malloc(sizeof(*my_mesh->l) * 1);
	my_mesh->radius = fmax(dist3d(x1, y1, z1), dist3d(x2, y2, z2));

	my_mesh->v[0].x = x1;
	my_mesh->v[0].y = y1;
	my_mesh->v[0].z = -z1;
	my_mesh->v[1].x = x2;
	my_mesh->v[1].y = y2;
	my_mesh->v[1].z = -z2;

	my_mesh->l[0].start = &my_mesh->v[0];
	my_mesh->l[0].end = &my_mesh->v[1];
	my_mesh->l[0].flag = 0;

	return my_mesh;
}

