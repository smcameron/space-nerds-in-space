#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <assert.h>
#include <stdio.h>

#include "vertex.h"
#include "triangle.h"
#include "mtwist.h"
#include "mathutils.h"
#include "matrix.h"
#include "quat.h"
#include "snis_graph.h"
#include "material.h"
#include "quat.h"
#include "mikktspace/mikktspace.h"

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

float mesh_compute_nonuniform_scaled_radius(struct mesh *m, double sx, double sy, double sz)
{
	int i;
	float r, max_radius = 0.0;

	for (i = 0; i < m->nvertices; i++) {
		r = dist3d(m->v[i].x * sx, m->v[i].y * sy, m->v[i].z * sz);
		if (r > max_radius)
			max_radius = r;
	}
	return max_radius;
}

void mesh_distort_helper(struct mesh *m, float distortion)
{
	int i;

	for (i = 0; i < m->nvertices; i++) {
		float n = 2.0 * (0.001 * snis_randn(1000) - 0.5);
		union vec3 v;
		v.v.x = m->v[i].x;
		v.v.y = m->v[i].y;
		v.v.z = m->v[i].z;
		vec3_mul_self(&v, 1.0 + n * distortion);
		m->v[i].x = v.v.x;
		m->v[i].y = v.v.y;
		m->v[i].z = v.v.z;
	}
	m->radius = mesh_compute_radius(m);
	mesh_set_flat_shading_vertex_normals(m);
}

/* This is not very good, but better than nothing. */
void mesh_random_uv_map(struct mesh *m)
{
	int i;

	if (m->tex)
		free(m->tex);
	m->tex = malloc(sizeof(*m->tex) * m->ntriangles * 3);
	if (!m)
		return;
	for (i = 0; i < m->ntriangles; i++) {
		float u1, v1, u2, v2, u3, v3;

		u1 = (float) snis_randn(25) / 100.0f;
		v1 = (float) snis_randn(25) / 100.0f;
		u2 = (float) (50.0f + snis_randn(25)) / 100.0f;
		v2 = (float) snis_randn(25) / 100.0f;
		u3 = (float) (50.0f + snis_randn(25)) / 100.0f;
		v3 = (float) (50.0f + snis_randn(25)) / 100.0f;
		mesh_set_triangle_texture_coords(m, i, u1, v1, u2, v2, u3, v3);
	}
}

void mesh_set_reasonable_tangent_and_bitangent(struct vertex *vnormal,
		struct vertex *vtangent, struct vertex *vbitangent)
{
	union vec3 normal, tangent, bitangent;

	normal.v.x = vnormal->x;
	normal.v.y = vnormal->y;
	normal.v.z = vnormal->z;
	vec3_normalize_self(&normal);

	/* Find a perpendicular vector by switching two coords & negating 1, avoiding degenerate case */
	if (fabsf(normal.v.x - normal.v.y) < 0.00001) {
		tangent.v.x = -normal.v.z;
		tangent.v.y = 0;
		tangent.v.z = normal.v.x;
	} else {
		tangent.v.x = -normal.v.y;
		tangent.v.y = 0;
		tangent.v.z = normal.v.x;
	}

	vec3_normalize_self(&tangent);
	vec3_cross(&bitangent, &normal, &tangent);
	vec3_normalize_self(&bitangent);
	vtangent->x = tangent.v.x;
	vtangent->y = tangent.v.y;
	vtangent->z = tangent.v.z;
	vtangent->w = 1.0;
	vbitangent->x = bitangent.v.x;
	vbitangent->y = bitangent.v.y;
	vbitangent->z = bitangent.v.z;
	vbitangent->w = 1.0;
}

/* If normals are already set, then this will set some reasonable tangents and bitangents */
void mesh_set_reasonable_tangents_and_bitangents(struct mesh *m)
{
	int i, j;

	for (i = 0; i < m->ntriangles; i++) {
		for (j = 0; j < 3; j++) {
			mesh_set_reasonable_tangent_and_bitangent(&m->t[i].vnormal[j],
					&m->t[i].vtangent[j], &m->t[i].vbitangent[j]);
		}
	}
}

void mesh_distort(struct mesh *m, float distortion)
{
	mesh_distort_helper(m, distortion);
	mesh_graph_dev_init(m);
}

void mesh_distort_and_random_uv_map(struct mesh *m, float distortion)
{
	mesh_distort_helper(m, distortion);
	mesh_random_uv_map(m);
	mesh_graph_dev_init(m);
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
	mesh_set_flat_shading_vertex_normals(m);
	mesh_graph_dev_init(m);
}

void mesh_scale(struct mesh *m, float scale)
{
	int i;

	for (i = 0; i < m->nvertices; i++) {
		m->v[i].x *= scale;
		m->v[i].y *= scale;
		m->v[i].z *= scale;
	}
	m->radius = mesh_compute_radius(m);
	mesh_graph_dev_init(m);
}

static int lookup_vertex(struct mesh *m, struct vertex *v)
{
	int i;

	for (i = 0; i < m->nvertices; i++)
		if (&m->v[i] == v)
			return i;
	return -1;
}

static struct mesh *allocate_mesh_for_copy(int ntriangles, int nvertices, int nlines,
						int with_texture)
{
	struct mesh *copy;

	assert(nvertices > 0);
	assert(nlines > 0 || ntriangles > 0);

	copy = malloc(sizeof(*copy));
	if (!copy)
		goto bail;
	memset(copy, 0, sizeof(*copy));
	copy->t = NULL;
	copy->v = NULL;
	copy->l = NULL;
	copy->tex = NULL;
	copy->material = NULL;
	if (ntriangles) {
		copy->t = malloc(sizeof(*copy->t) * ntriangles);
		if (!copy->t)
			goto bail;
		memset(copy->t, 0, sizeof(*copy->t) * ntriangles);
	}
	if (nvertices) {
		copy->v = malloc(sizeof(*copy->v) * nvertices);
		if (!copy->v)
			goto bail;
		memset(copy->v, 0, sizeof(*copy->v) * nvertices);
	}
	if (nlines) {
		copy->l = malloc(sizeof(*copy->l) * nlines);
		if (!copy->l)
			goto bail;
		memset(copy->l, 0, sizeof(*copy->l) * nlines);
	}
	if (with_texture && ntriangles) {
		copy->tex = malloc(sizeof(*copy->tex) * ntriangles * 3);
		if (!copy->tex)
			goto bail;
		memset(copy->tex, 0, sizeof(*copy->tex) * ntriangles * 3);
	}
	copy->graph_ptr = 0;
	return copy;
bail:
	mesh_free(copy);
	return NULL;
}

static void copy_mesh_contents(struct mesh *copy, struct mesh *original)
{
	int i;

	copy->geometry_mode = original->geometry_mode;
	copy->ntriangles = original->ntriangles;
	copy->nvertices = original->nvertices;
	copy->nlines = original->nlines;
	copy->graph_ptr = 0;

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
		copy->t[i].vnormal[0] = original->t[i].vnormal[0];
		copy->t[i].vnormal[1] = original->t[i].vnormal[1];
		copy->t[i].vnormal[2] = original->t[i].vnormal[2];
	}

	for (i = 0; i < original->nlines; i++) {
		int v0, v1;

		v0 = lookup_vertex(original, original->l[i].start);
		v1 = lookup_vertex(original, original->l[i].end);

		copy->l[i].start = &copy->v[v0];
		copy->l[i].end = &copy->v[v1];
		copy->l[i].flag = original->l[i].flag;
		copy->l[i].additivity = original->l[i].additivity;
		copy->l[i].opacity = original->l[i].opacity;
		copy->l[i].tint_color = original->l[i].tint_color;
		copy->l[i].time_offset = original->l[i].time_offset;
	}
	if (original->tex)
		memcpy(copy->tex, original->tex, sizeof(*copy->tex) * original->ntriangles * 3);
	copy->radius = original->radius;
	if (original->material) {
		copy->material = malloc(sizeof(*copy->material));
		*copy->material = *original->material;
	}
	mesh_graph_dev_init(copy);
}

struct mesh *mesh_duplicate(struct mesh *original)
{
	struct mesh *copy;

	copy = allocate_mesh_for_copy(original->ntriangles, original->nvertices,
					original->nlines, original->tex != NULL);
	if (!copy)
		return copy;
	copy_mesh_contents(copy, original);
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

	if (!my_mesh)
		return my_mesh;
	memset(my_mesh, 0, sizeof(*my_mesh));

	my_mesh->geometry_mode = MESH_GEOMETRY_LINES;
	my_mesh->nvertices = 0;
	my_mesh->ntriangles = 0;
	my_mesh->nlines = 1;
	my_mesh->t = 0;
	my_mesh->v = malloc(sizeof(*my_mesh->v) * (npoints + 1));
	my_mesh->l = malloc(sizeof(*my_mesh->l) * 1);
	my_mesh->tex = 0;
	my_mesh->radius = r;
	my_mesh->graph_ptr = 0;

	increment = angle / (float) npoints;
	assert(npoints > 0);
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

	mesh_graph_dev_init(my_mesh);
	return my_mesh;
}

struct mesh *init_radar_circle_xz_plane_mesh(double x, double z, double r, int ticks, double tick_radius)
{
	int i;
	struct mesh *my_mesh = malloc(sizeof(*my_mesh));

	if (!my_mesh)
		return my_mesh;
	memset(my_mesh, 0, sizeof(*my_mesh));

	my_mesh->geometry_mode = MESH_GEOMETRY_LINES;
	my_mesh->nvertices = 0;
	my_mesh->ntriangles = 0;
	my_mesh->nlines = 0;
	my_mesh->t = 0;
	my_mesh->v = malloc(sizeof(*my_mesh->v) * (360 / 2 + 1 + ticks*2));
	my_mesh->l = malloc(sizeof(*my_mesh->l) * (1 + ticks));
	my_mesh->radius = dist3d(x, 0, z) + r;
	my_mesh->tex = 0;
	my_mesh->graph_ptr = 0;

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

	mesh_graph_dev_init(my_mesh);
	return my_mesh;
}

struct mesh *init_line_mesh(double x1, double y1, double z1, double x2, double y2, double z2)
{
	struct mesh *my_mesh = malloc(sizeof(*my_mesh));

	if (!my_mesh)
		return my_mesh;
	memset(my_mesh, 0, sizeof(*my_mesh));

	my_mesh->geometry_mode = MESH_GEOMETRY_LINES;
	my_mesh->nvertices = 2;
	my_mesh->ntriangles = 0;
	my_mesh->nlines = 1;
	my_mesh->t = 0;
	my_mesh->v = malloc(sizeof(*my_mesh->v) * 2);
	my_mesh->l = malloc(sizeof(*my_mesh->l) * 1);
	my_mesh->tex = 0;
	my_mesh->radius = fmax(dist3d(x1, y1, z1), dist3d(x2, y2, z2));
	my_mesh->graph_ptr = 0;

	my_mesh->v[0].x = x1;
	my_mesh->v[0].y = y1;
	my_mesh->v[0].z = -z1;
	my_mesh->v[1].x = x2;
	my_mesh->v[1].y = y2;
	my_mesh->v[1].z = -z2;

	my_mesh->l[0].start = &my_mesh->v[0];
	my_mesh->l[0].end = &my_mesh->v[1];
	my_mesh->l[0].flag = 0;

	mesh_graph_dev_init(my_mesh);
	return my_mesh;
}

struct mesh *mesh_fabricate_axes(void)
{
	struct mesh *my_mesh = malloc(sizeof(*my_mesh));

	if (!my_mesh)
		return my_mesh;
	memset(my_mesh, 0, sizeof(*my_mesh));

	my_mesh->geometry_mode = MESH_GEOMETRY_LINES;
	my_mesh->nvertices = 6;
	my_mesh->ntriangles = 0;
	my_mesh->nlines = 3;
	my_mesh->t = 0;
	my_mesh->v = malloc(sizeof(*my_mesh->v) * my_mesh->nvertices);
	my_mesh->l = malloc(sizeof(*my_mesh->l) * my_mesh->nlines);
	my_mesh->tex = 0;
	my_mesh->radius = 2.0;
	my_mesh->graph_ptr = 0;

	my_mesh->v[0].x = -1.0f;
	my_mesh->v[0].y = 0.0f;
	my_mesh->v[0].z = 0.0f;
	my_mesh->v[1].x = 1.0f;
	my_mesh->v[1].y = 0.0f;
	my_mesh->v[1].z = 0.0f;

	my_mesh->v[2].x = 0.0f;
	my_mesh->v[2].y = -1.0f;
	my_mesh->v[2].z = 0.0f;
	my_mesh->v[3].x = 0.0f;
	my_mesh->v[3].y = 1.0f;
	my_mesh->v[3].z = 0.0f;

	my_mesh->v[4].x = 0.0f;
	my_mesh->v[4].y = 0.0f;
	my_mesh->v[4].z = -1.0f;
	my_mesh->v[5].x = 0.0f;
	my_mesh->v[5].y = 0.0f;
	my_mesh->v[5].z = 1.0f;

	my_mesh->l[0].start = &my_mesh->v[0];
	my_mesh->l[0].end = &my_mesh->v[1];
	my_mesh->l[0].flag = 0;

	my_mesh->l[1].start = &my_mesh->v[2];
	my_mesh->l[1].end = &my_mesh->v[3];
	my_mesh->l[1].flag = 0;

	my_mesh->l[2].start = &my_mesh->v[4];
	my_mesh->l[2].end = &my_mesh->v[5];
	my_mesh->l[2].flag = 0;
	my_mesh->radius = 1.0;

	mesh_graph_dev_init(my_mesh);
	return my_mesh;
}

static union vec3 compute_triangle_normal(struct triangle *t)
{
	union vec3 v1, v2, cross;

	v1.v.x = t->v[1]->x - t->v[0]->x;
	v1.v.y = t->v[1]->y - t->v[0]->y;
	v1.v.z = t->v[1]->z - t->v[0]->z;

	v2.v.x = t->v[2]->x - t->v[1]->x;
	v2.v.y = t->v[2]->y - t->v[1]->y;
	v2.v.z = t->v[2]->z - t->v[1]->z;

	vec3_cross(&cross, &v1, &v2);

	/* make sure we always have a valid normal, not NaNs */
	if (vec3_magnitude(&cross) < 1e-20)
		vec3_init(&cross, 0, 1, 0);
	else
		vec3_normalize_self(&cross);

	return cross;
}

void mesh_set_flat_shading_vertex_normals(struct mesh *m)
{
	int i, j;

	for (i = 0; i < m->ntriangles; i++) {
		union vec3 normal;
		normal = compute_triangle_normal(&m->t[i]);
		m->t[i].n.x = normal.v.x;
		m->t[i].n.y = normal.v.y;
		m->t[i].n.z = normal.v.z;
		for (j = 0; j < 3; j++) {
			m->t[i].vnormal[j] = m->t[i].n;
			mesh_set_reasonable_tangent_and_bitangent(&m->t[i].vnormal[j],
					&m->t[i].vtangent[j], &m->t[i].vbitangent[j]);
		}
	}
}

void mesh_set_spherical_vertex_normals(struct mesh *m)
{
	int i, j;

	for (i = 0; i < m->ntriangles; i++) {
		union vec3 normal;
		normal = compute_triangle_normal(&m->t[i]);
		m->t[i].n.x = normal.v.x;
		m->t[i].n.y = normal.v.y;
		m->t[i].n.z = normal.v.z;
		for (j = 0; j < 3; j++) {
			union vec3 normal = { { m->t[i].v[j]->x, m->t[i].v[j]->y, m->t[i].v[j]->z } };
			vec3_normalize_self(&normal);
			m->t[i].vnormal[j].x = normal.v.x;
			m->t[i].vnormal[j].y = normal.v.y;
			m->t[i].vnormal[j].z = normal.v.z;
		}
	}
}

/* An attempt at an analytic solution.
 * (see: http://www.iquilezles.org/www/articles/patchedsphere/patchedsphere.htm )
 * This does not seem to work (undoubtedly I've done something wrong.)
 */
void mesh_set_spherical_cubemap_tangent_and_bitangent(struct mesh *m)
{
	int i, j;

	/* This algorithm will have problems for triangles which have vertices which
	 * are on different faces of the cubemap...
	 */
	for (i = 0; i < m->ntriangles; i++) {
		for (j = 0; j < 3; j++) {
			union vec3 normal = { { m->t[i].v[j]->x, m->t[i].v[j]->y, m->t[i].v[j]->z } };
			const float nx = normal.v.x;
			const float ny = normal.v.y;
			const float nz = normal.v.z;
			float x = 0.0f;
			float y = 0.0f;
			union vec3 tangent, bitangent;

			/* Figure out which face of cubemap we're on, and which coords
			 * play roles of x and y in calculation of tangent and bitangent
			 */
			if (fabsf(nx) > fabsf(ny) && fabsf(nx) > fabsf(nz)) {
				if (nx > 0)
					x = 1.0f - nz;
				else
					x = nz;
				y = ny;
			} else if (fabsf(ny) > fabsf(nx) && fabsf(ny) > fabsf(nz)) {
				if (ny > 0)
					y = 1.0f - nz;
				else
					y = nz;
				x = nx;
			} else /* it must be true that (fabsf(nz) > fabsf(nx) && fabsf(nz) > fabsf(ny)) */ {
				if (nz > 0)
					x = nx;
				else
					x = 1.0f - nx;
				y = ny;
			}
			cubemapped_sphere_tangent_and_bitangent(x, y, &tangent, &bitangent);
			m->t[i].vtangent[j].x = tangent.v.x;
			m->t[i].vtangent[j].y = tangent.v.y;
			m->t[i].vtangent[j].z = tangent.v.z;
			m->t[i].vbitangent[j].x = bitangent.v.x;
			m->t[i].vbitangent[j].y = bitangent.v.y;
			m->t[i].vbitangent[j].z = bitangent.v.z;
		}
	}
}

/* An attempt at an empirical solution, sampling rather than computing analytically.
 * This only works with a spherified cube.
 */
void mesh_sample_spherical_cubemap_tangent_and_bitangent(struct mesh *m)
{
	int i, j;
	union vec3 normal, tsample, bsample, tangent, bitangent;
	float epsilon = 0.001;
	int triangles_per_face;
	int face;
	const float epsilon_factor[6][6] = {
			{  1.0,  0.0f,  0.0f, 0.0f, -1.0f,  0.0f, }, /* face 0 */
			{  0.0f, 0.0f, -1.0f, 0.0f, -1.0f,  0.0f, }, /* face 1 */
			{ -1.0f, 0.0f,  0.0f, 0.0f, -1.0f,  0.0f, }, /* face 2 */
			{  0.0f, 0.0f,  1.0f, 0.0f, -1.0f,  0.0f, }, /* face 3 */
			{  1.0f, 0.0f,  0.0f, 0.0f,  0.0f,  1.0f, }, /* face 4 */
			{  1.0f, 0.0f,  0.0f, 0.0f,  0.0f, -1.0f, }, /* face 5 */
	};

	/* This algorithm will have problems for triangles which have vertices which
	 * are on different faces of the cubemap.  Luckily there are no such vertices
	 * in the case of a spherified cube with duplicated edge vertices.
	 */

	triangles_per_face = m->ntriangles / 6;
	assert(triangles_per_face * 6 == m->ntriangles);
	for (i = 0; i < m->ntriangles; i++) {
		face = i / triangles_per_face;
		for (j = 0; j < 3; j++) {
			normal.v.x = m->t[i].v[j]->x;
			normal.v.y = m->t[i].v[j]->y;
			normal.v.z = m->t[i].v[j]->z;
			vec3_normalize_self(&normal);

			/* Based on which face of cubemap we're on, figure which coords
			 * play roles of x and y in calculation of tangent and bitangent
			 */
			tsample.v.x = normal.v.x + epsilon_factor[face][0] * epsilon;
			tsample.v.y = normal.v.y + epsilon_factor[face][1] * epsilon;
			tsample.v.z = normal.v.z + epsilon_factor[face][2] * epsilon;
			bsample.v.x = normal.v.x + epsilon_factor[face][3] * epsilon;
			bsample.v.y = normal.v.y + epsilon_factor[face][4] * epsilon;
			bsample.v.z = normal.v.z + epsilon_factor[face][5] * epsilon;
			vec3_normalize_self(&tsample);
			vec3_normalize_self(&bsample);
			vec3_sub(&tangent, &tsample, &normal);
			vec3_normalize_self(&tangent);
			vec3_sub(&bitangent, &bsample, &normal);
			vec3_normalize_self(&bitangent);
			m->t[i].vtangent[j].x = tangent.v.x;
			m->t[i].vtangent[j].y = tangent.v.y;
			m->t[i].vtangent[j].z = tangent.v.z;
			m->t[i].vbitangent[j].x = bitangent.v.x;
			m->t[i].vbitangent[j].y = bitangent.v.y;
			m->t[i].vbitangent[j].z = bitangent.v.z;
		}
	}
}

static int find_vertex(struct vertex *haystack[], struct vertex *needle, int nitems)
{
	int i;

	for (i = 0; i < nitems; i++)
		if (haystack[i] == needle)
			return i;
	return -1;
}

void mesh_set_average_vertex_normals(struct mesh *m)
{
	int i, j, k;
	struct vertex *vn, **v;

	v = malloc(sizeof(*v) * m->nvertices);
	vn = malloc(sizeof(*vn) * m->nvertices);

	/* Use w as a counter of how many triangles use a vertex, init to 0 */
	for (i = 0; i < m->nvertices; i++)
		m->v[i].w = 0.0;
	k = 0;
	for (i = 0; i < m->ntriangles; i++) {
		union vec3 normal;
		normal = compute_triangle_normal(&m->t[i]);
		m->t[i].n.x = normal.v.x;
		m->t[i].n.y = normal.v.y;
		m->t[i].n.z = normal.v.z;
		for (j = 0; j < 3; j++) {
			/* First time we've encountered this vertex? */
			if (m->t[i].v[j]->w < 0.5) {
				v[k] = m->t[i].v[j];
				vn[k].x = normal.v.x;
				vn[k].y = normal.v.y;
				vn[k].z = normal.v.z;
				vn[k].w = 1.0;
				m->t[i].v[j]->w = 1.0;
				k++;
			} else {
				int fv = find_vertex(v, m->t[i].v[j], m->nvertices);
				assert(fv >= 0);
				vn[fv].x += normal.v.x;	/* clang scan-build complains about garbage values */
				vn[fv].y += normal.v.y;	/* in vn[fv]. I believe this is a false positive because */
				vn[fv].z += normal.v.z;	/* the values will have been initialized by the "then" part */
				vn[fv].w += 1.0;	/* of this "if" statement before we encounter them */
				m->t[i].v[j]->w += 1.0;	/* again in the "else" part. -- smcameron, 2019-07-09. */
			}
		}
	}
	for (i = 0; i < m->ntriangles; i++) {
		for (j = 0; j < 3; j++) {
			k = find_vertex(v, m->t[i].v[j], m->nvertices);
			assert(k >= 0);
			/* Average the normals... */
			m->t[i].vnormal[j].x = vn[k].x / m->t[i].v[j]->w;
			m->t[i].vnormal[j].y = vn[k].y / m->t[i].v[j]->w;
			m->t[i].vnormal[j].z = vn[k].z / m->t[i].v[j]->w;
			mesh_set_reasonable_tangent_and_bitangent(&m->t[i].vnormal[j],
					&m->t[i].vtangent[j], &m->t[i].vbitangent[j]);
		}
	}

	/* set w's back to 1.0 */
	for (i = 0; i < m->nvertices; i++)
		m->v[i].w = 1.0;
	free(vn);
	free(v);
}

void mesh_set_triangle_texture_coords(struct mesh *m, int triangle,
	float u1, float v1, float u2, float v2, float u3, float v3)
{
	m->tex[triangle * 3 + 0].u = u1;
	m->tex[triangle * 3 + 0].v = v1;
	m->tex[triangle * 3 + 1].u = u2;
	m->tex[triangle * 3 + 1].v = v2;
	m->tex[triangle * 3 + 2].u = u3;
	m->tex[triangle * 3 + 2].v = v3;
}

void mesh_free(struct mesh *m)
{
	if (!m)
		return;

	if (m->t)
		free(m->t);
	if (m->v)
		free(m->v);
	if (m->l)
		free(m->l);
	if (m->tex)
		free(m->tex);
	if (m->material)
		free(m->material);
	mesh_graph_dev_cleanup(m);
	free(m);
}

/* mesh_fabricate_crossbeam fabricates a mesh like so, out of 8 triangles:
 *          0
 *         |\
 *         | \
 *         |  \
 *  4______|   \__5
 *   \     \    \ \
 *    \     \   |3 \
 *     \     \  |   \
 *      \ 1   \ |    \
 *       \_____\|_____\6
 *      7   \   |
 *           \  |
 *            \ |
 *             \|
 *              2
 * centered on origin, length axis parallel to x axis.
 * length is the distance betwee 0 and 3, above, and
 * radius is the distance between the center of the cross
 * beam and 2,6,7,3 and 0,4,5,1.
 * 
 * 8 triangles are needed because we need to prevent backface
 * culling, so we wind one set of tris one way, and the other,
 * the other.
 */
struct mesh *mesh_fabricate_crossbeam(float length, float radius)
{
	struct mesh *m;

	m = malloc(sizeof(*m));
	if (!m)
		return m;
	memset(m, 0, sizeof(*m));
	m->nvertices = 8;
	m->ntriangles = 8;

	m->t = malloc(sizeof(*m->t) * m->ntriangles);
	if (!m->t)
		goto bail;
	memset(m->t, 0, sizeof(*m->t) * m->ntriangles);
	m->v = malloc(sizeof(*m->v) * m->nvertices);
	if (!m->v)
		goto bail;
	memset(m->v, 0, sizeof(*m->v) * m->nvertices);
	m->tex = malloc(sizeof(*m->tex) * m->ntriangles * 3);
	if (!m->tex)
		goto bail;
	memset(m->tex, 0, sizeof(*m->tex) * m->ntriangles * 3);
	m->l = NULL;

	m->geometry_mode = MESH_GEOMETRY_TRIANGLES;
	m->v[0].x = -length / 2.0f;
	m->v[0].y = radius;
	m->v[0].z = 0.0f;
	m->v[1].x = -length / 2.0f;
	m->v[1].y = -radius;
	m->v[1].z = 0.0f;
	m->v[2].x = length / 2.0f;
	m->v[2].y = -radius;
	m->v[2].z = 0.0f;
	m->v[3].x = length / 2.0f;
	m->v[3].y = radius;
	m->v[3].z = 0.0f;
	m->v[4].x = -length / 2.0f;
	m->v[4].y = 0.0f;
	m->v[4].z = radius;
	m->v[5].x = -length / 2.0f;
	m->v[5].y = 0.0f;
	m->v[5].z = -radius;
	m->v[6].x = length / 2.0f;
	m->v[6].y = 0.0f;
	m->v[6].z = -radius;
	m->v[7].x = length / 2.0f;
	m->v[7].y = 0.0f;
	m->v[7].z = radius;

	m->t[0].v[0] = &m->v[0];
	m->t[0].v[1] = &m->v[1];
	m->t[0].v[2] = &m->v[2];
	mesh_set_triangle_texture_coords(m, 0, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f);

	m->t[1].v[0] = &m->v[2];
	m->t[1].v[1] = &m->v[3];
	m->t[1].v[2] = &m->v[0];
	mesh_set_triangle_texture_coords(m, 1, 1.0f, 1.0f, 1.0f, 0.0f, 0.0f, 0.0f);
	
	m->t[2].v[0] = &m->v[4];
	m->t[2].v[1] = &m->v[5];
	m->t[2].v[2] = &m->v[6];
	mesh_set_triangle_texture_coords(m, 2, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f);
	
	m->t[3].v[0] = &m->v[6];
	m->t[3].v[1] = &m->v[7];
	m->t[3].v[2] = &m->v[4];
	mesh_set_triangle_texture_coords(m, 3, 1.0f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f);

	m->t[4].v[0] = &m->v[2];
	m->t[4].v[1] = &m->v[1];
	m->t[4].v[2] = &m->v[0];
	mesh_set_triangle_texture_coords(m, 4, 1.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f);

	m->t[5].v[0] = &m->v[0];
	m->t[5].v[1] = &m->v[3];
	m->t[5].v[2] = &m->v[2];
	mesh_set_triangle_texture_coords(m, 5, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f);
	
	m->t[6].v[0] = &m->v[6];
	m->t[6].v[1] = &m->v[5];
	m->t[6].v[2] = &m->v[4];
	mesh_set_triangle_texture_coords(m, 6, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f);
	
	m->t[7].v[0] = &m->v[4];
	m->t[7].v[1] = &m->v[7];
	m->t[7].v[2] = &m->v[6];
	mesh_set_triangle_texture_coords(m, 7, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.0f);

	mesh_compute_radius(m);
	mesh_set_flat_shading_vertex_normals(m);
	mesh_graph_dev_init(m);

	return m;

bail:
	mesh_free(m);
	return NULL;
}

/* mesh_fabricate_billboard() makes a billboard:
 *   quad is centered on 0,0 and texture coords are 0,0 in lower left per opengl convention
 *               .-- x
 *    -x <--     V     ---> +x
 * +y       0         1
 * ^   (0,1) +-------+ (1,1)
 * |         |\      |
 * |         | \     |
 *           |  \    |
 * y ------> |   \   |
 *           |    \  |
 * |         |     \ |
 * |   (0,0) |      \| (1,0)
 * v       3 +-------+ 2
 * -y
 *          normal = +z
 */

struct mesh *mesh_fabricate_billboard(float width, float height)
{
	struct mesh *m;

	m = malloc(sizeof(*m));
	if (!m)
		return m;
	memset(m, 0, sizeof(*m));
	m->nvertices = 4;
	m->ntriangles = 2;

	m->t = malloc(sizeof(*m->t) * m->ntriangles);
	if (!m->t)
		goto bail;
	memset(m->t, 0, sizeof(*m->t) * m->ntriangles);
	m->v = malloc(sizeof(*m->v) * m->nvertices);
	if (!m->v)
		goto bail;
	memset(m->v, 0, sizeof(*m->v) * m->nvertices);
	m->tex = malloc(sizeof(*m->tex) * m->ntriangles * 3);
	if (!m->tex)
		goto bail;
	memset(m->tex, 0, sizeof(*m->tex) * m->ntriangles * 3);
	m->l = NULL;

	m->geometry_mode = MESH_GEOMETRY_TRIANGLES;
	m->v[0].x = -width / 2.0f;
	m->v[0].y = height / 2.0f;
	m->v[0].z = 0;
	m->v[1].x = width / 2.0f;
	m->v[1].y = height / 2.0f;
	m->v[1].z = 0;
	m->v[2].x = width / 2.0f;
	m->v[2].y = -height / 2.0f;
	m->v[2].z = 0;
	m->v[3].x = -width / 2.0f;
	m->v[3].y = -height / 2.0f;
	m->v[3].z = 0;

	m->t[0].v[0] = &m->v[0];
	m->t[0].v[1] = &m->v[2];
	m->t[0].v[2] = &m->v[1];
	m->t[0].flag = TRIANGLE_0_1_COPLANAR;
	mesh_set_triangle_texture_coords(m, 0, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f, 1.0f);

	m->t[1].v[0] = &m->v[0];
	m->t[1].v[1] = &m->v[3];
	m->t[1].v[2] = &m->v[2];
	m->t[1].flag = TRIANGLE_0_2_COPLANAR;
	mesh_set_triangle_texture_coords(m, 1, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f);

	m->radius = mesh_compute_radius(m);
	mesh_set_flat_shading_vertex_normals(m);
	mesh_graph_dev_init(m);

	return m;

bail:
	mesh_free(m);
	return NULL;
}

static void normalize_sphere(struct mesh *m)
{
	int i;

	for (i = 0; i < m->nvertices; i++) {
		union vec3 v = { { m->v[i].x, m->v[i].y, m->v[i].z } };
		vec3_normalize_self(&v);
		m->v[i].x = v.v.x;
		m->v[i].y = v.v.y;
		m->v[i].y = v.v.y;
	}
	m->radius = 1.0;
}

/* TODO: make stl parser use this code. */
#define VERTEX_MERGING_THRESHOLD (1e-18)
static struct vertex *lookup_maybe_add_vertex(struct mesh *m, struct vertex *v, int *is_shared)
{
	/* Search vertices to see if we already have this one. */
	int i;

	for (i = 0; i < m->nvertices; i++) {
		double d = hypot3d(v->x - m->v[i].x, v->y - m->v[i].y, v->z - m->v[i].z);
		if (d < VERTEX_MERGING_THRESHOLD) {
			*is_shared = 1;
			return &m->v[i];
		}
	}
	m->v[m->nvertices] = *v;
	m->nvertices++;
	*is_shared = 0;
	return &m->v[m->nvertices - 1];
}

/* See: http://blog.andreaskahler.com/2009/06/creating-icosphere-mesh-in-code.html */
struct mesh *mesh_unit_icosahedron(void)
{
	const double tau = (1.0 + sqrt(5.0)) / 2.0;
	const double scale = 1.0 / sqrt(1.0 + tau * tau);
	struct mesh *m;

	m = malloc(sizeof(*m));
	if (!m)
		return m;
	memset(m, 0, sizeof(*m));
	m->nvertices = 12;
	m->ntriangles = 20;

	m->t = malloc(sizeof(*m->t) * m->ntriangles);
	if (!m->t)
		goto bail;
	memset(m->t, 0, sizeof(*m->t) * m->ntriangles);
	m->v = malloc(sizeof(*m->v) * m->nvertices);
	if (!m->v)
		goto bail;
	memset(m->v, 0, sizeof(*m->v) * m->nvertices);
	m->tex = 0;
	/* m->tex = malloc(sizeof(*m->tex) * m->ntriangles * 3);
	if (!m->tex)
		goto bail;
	memset(m->tex, 0, sizeof(*m->tex) * m->ntriangles * 3); */
	m->l = NULL;

	m->geometry_mode = MESH_GEOMETRY_TRIANGLES;

	m->v[0].x = scale * -1.0;
	m->v[0].y = scale * tau;
	m->v[0].z = scale * 0.0;

	m->v[1].x = scale * 1.0;
	m->v[1].y = scale * tau;
	m->v[1].z = scale * 0.0;

	m->v[2].x = scale * -1.0;
	m->v[2].y = scale * -tau;
	m->v[2].z = scale * 0.0;

	m->v[3].x = scale * 1.0;
	m->v[3].y = scale * -tau;
	m->v[3].z = scale * 0.0;

	m->v[4].x = scale * 0.0;
	m->v[4].y = scale * -1.0;
	m->v[4].z = scale * tau;

	m->v[5].x = scale * 0.0;
	m->v[5].y = scale * 1.0;
	m->v[5].z = scale * tau;

	m->v[6].x = scale * 0.0;
	m->v[6].y = scale * -1.0;
	m->v[6].z = scale * -tau;

	m->v[7].x = scale * 0.0;
	m->v[7].y = scale * 1.0;
	m->v[7].z = scale * -tau;

	m->v[8].x = scale * tau;
	m->v[8].y = scale * 0.0;
	m->v[8].z = scale * -1.0;

	m->v[9].x = scale * tau;
	m->v[9].y = scale * 0.0;
	m->v[9].z = scale * 1.0;

	m->v[10].x = scale * -tau;
	m->v[10].y = scale * 0.0;
	m->v[10].z = scale * -1.0;

	m->v[11].x = scale * -tau;
	m->v[11].y = scale * 0.0;
	m->v[11].z = scale * 1.0;

	m->t[0].v[0] = &m->v[0];
	m->t[0].v[1] = &m->v[11];
	m->t[0].v[2] = &m->v[5];

	m->t[1].v[0] = &m->v[0];
	m->t[1].v[1] = &m->v[5];
	m->t[1].v[2] = &m->v[1];

	m->t[2].v[0] = &m->v[0];
	m->t[2].v[1] = &m->v[1];
	m->t[2].v[2] = &m->v[7];

	m->t[3].v[0] = &m->v[0];
	m->t[3].v[1] = &m->v[7];
	m->t[3].v[2] = &m->v[10];

	m->t[4].v[0] = &m->v[0];
	m->t[4].v[1] = &m->v[10];
	m->t[4].v[2] = &m->v[11];

	m->t[5].v[0] = &m->v[1];
	m->t[5].v[1] = &m->v[5];
	m->t[5].v[2] = &m->v[9];

	m->t[6].v[0] = &m->v[5];
	m->t[6].v[1] = &m->v[11];
	m->t[6].v[2] = &m->v[4];

	m->t[7].v[0] = &m->v[11];
	m->t[7].v[1] = &m->v[10];
	m->t[7].v[2] = &m->v[2];

	m->t[8].v[0] = &m->v[10];
	m->t[8].v[1] = &m->v[7];
	m->t[8].v[2] = &m->v[6];

	m->t[9].v[0] = &m->v[7];
	m->t[9].v[1] = &m->v[1];
	m->t[9].v[2] = &m->v[8];

	m->t[10].v[0] = &m->v[3];
	m->t[10].v[1] = &m->v[9];
	m->t[10].v[2] = &m->v[4];

	m->t[11].v[0] = &m->v[3];
	m->t[11].v[1] = &m->v[4];
	m->t[11].v[2] = &m->v[2];

	m->t[12].v[0] = &m->v[3];
	m->t[12].v[1] = &m->v[2];
	m->t[12].v[2] = &m->v[6];

	m->t[13].v[0] = &m->v[3];
	m->t[13].v[1] = &m->v[6];
	m->t[13].v[2] = &m->v[8];

	m->t[14].v[0] = &m->v[3];
	m->t[14].v[1] = &m->v[8];
	m->t[14].v[2] = &m->v[9];

	m->t[15].v[0] = &m->v[4];
	m->t[15].v[1] = &m->v[9];
	m->t[15].v[2] = &m->v[5];

	m->t[16].v[0] = &m->v[2];
	m->t[16].v[1] = &m->v[4];
	m->t[16].v[2] = &m->v[11];

	m->t[17].v[0] = &m->v[6];
	m->t[17].v[1] = &m->v[2];
	m->t[17].v[2] = &m->v[10];

	m->t[18].v[0] = &m->v[8];
	m->t[18].v[1] = &m->v[6];
	m->t[18].v[2] = &m->v[7];

	m->t[19].v[0] = &m->v[9];
	m->t[19].v[1] = &m->v[8];
	m->t[19].v[2] = &m->v[1];

	m->radius = mesh_compute_radius(m);
	mesh_set_flat_shading_vertex_normals(m);
	mesh_graph_dev_init(m);

	return m;

bail:
	mesh_free(m);
	return NULL;
}

/*
 * given a triangle a,b,c, create 4 triangles:
 *
 *                  a
 *                 / \
 *                /   \
 *              d-------e
 *              /\     /\
 *             /  \   /  \
 *            /    \ /    \
 *           /      \      \
 *          b-------f-------c
 *
 * a,d,e, e,d,f, d,b,f, and e,f,c
 *
 */
static void subdivide_triangle(struct mesh *m, int tri)
{
	struct vertex d, e, f, *pa, *pb, *pc, *pd, *pe, *pf;
	union vec3 vd, ve, vf;
	int shared, t;

	pa = m->t[tri].v[0];
	pb = m->t[tri].v[1];
	pc = m->t[tri].v[2];

	vd.v.x = (pa->x + pb->x) / 2.0;
	vd.v.y = (pa->y + pb->y) / 2.0;
	vd.v.z = (pa->z + pb->z) / 2.0;
	ve.v.x = (pa->x + pc->x) / 2.0;
	ve.v.y = (pa->y + pc->y) / 2.0;
	ve.v.z = (pa->z + pc->z) / 2.0;
	vf.v.x = (pb->x + pc->x) / 2.0;
	vf.v.y = (pb->y + pc->y) / 2.0;
	vf.v.z = (pb->z + pc->z) / 2.0;

	vec3_normalize_self(&vd);
	vec3_normalize_self(&ve);
	vec3_normalize_self(&vf);

	d.x = vd.v.x;
	d.y = vd.v.y;
	d.z = vd.v.z;
	e.x = ve.v.x;
	e.y = ve.v.y;
	e.z = ve.v.z;
	f.x = vf.v.x;
	f.y = vf.v.y;
	f.z = vf.v.z;

	pd = lookup_maybe_add_vertex(m, &d, &shared);
	pe = lookup_maybe_add_vertex(m, &e, &shared);
	pf = lookup_maybe_add_vertex(m, &f, &shared);

	/* replace initial triangle with central triangle, e,d,f */
	m->t[tri].v[0] = pe;
	m->t[tri].v[1] = pd;
	m->t[tri].v[2] = pf;

	/* add 3 surrounding triangles  a,d,e, d,b,f, and e,f,c */
	t = m->ntriangles;
	m->t[t].v[0] = pa;
	m->t[t].v[1] = pd;
	m->t[t].v[2] = pe;
	t++;
	m->t[t].v[0] = pd;
	m->t[t].v[1] = pb;
	m->t[t].v[2] = pf;
	t++;
	m->t[t].v[0] = pe;
	m->t[t].v[1] = pf;
	m->t[t].v[2] = pc;
	t++;
	m->ntriangles = t;
}

static struct mesh *mesh_subdivide_icosphere(struct mesh *m, int subdivisions)
{
	struct mesh *m2;
	int i, ntris = m->ntriangles;

	if (subdivisions == 0)
		return m;

	/* Allocate space for the new, bigger mesh */
	m2 = allocate_mesh_for_copy(m->ntriangles * 4, m->nvertices + m->ntriangles * 3, 0, 0);
	if (!m2)
		return NULL;
	copy_mesh_contents(m2, m);
	mesh_free(m);

	for (i = 0; i < ntris; i++)
		subdivide_triangle(m2, i);
	normalize_sphere(m2);
	mesh_set_spherical_vertex_normals(m2);
	return mesh_subdivide_icosphere(m2, subdivisions - 1);
}

struct mesh *mesh_unit_icosphere(int subdivisions)
{
	struct mesh *m = mesh_unit_icosahedron();
	struct mesh *m2, *m3;

	if (!m)
		return NULL;

	/* note mesh_subdivide_icosphere will free m */
	m2 = mesh_subdivide_icosphere(m, subdivisions);

	/* m2 will be over-allocated, duplicating will clean up the overallocation */
	m3 = mesh_duplicate(m2);
	mesh_free(m2);
	mesh_set_spherical_vertex_normals(m3);
	mesh_set_spherical_cubemap_tangent_and_bitangent(m3);
	mesh_graph_dev_init(m3);
	return m3;
}

/* Compute the distance between two vertices of a cube after spherification */
static float spherified_cube_vertex_distance(struct mesh *m, int v1, int v2)
{
	union vec3 vert1, vert2, diff;

	vert1.v.x = m->v[v1].x;
	vert1.v.y = m->v[v1].y;
	vert1.v.z = m->v[v1].z;
	vert2.v.x = m->v[v2].x;
	vert2.v.y = m->v[v2].y;
	vert2.v.z = m->v[v2].z;
	vec3_normalize_self(&vert1);
	vec3_normalize_self(&vert2);
	vec3_sub(&diff, &vert1, &vert2);
	return vec3_magnitude(&diff);
}

static void make_unit_cube_triangles(struct mesh *m, int face, int subdivisions)
{
	int i, j, v1, v2, v3, v4, vindex, tindex;
	float v1v4dist, v2v3dist;

	vindex = face * (subdivisions + 1) * (subdivisions + 1);
	tindex = face * (subdivisions * subdivisions) * 2;
	for (i = 0; i < subdivisions; i++) {
		for (j = 0; j < subdivisions; j++) {

			/*
			 *	v1--------v2
			 *	|          |
			 *	|          |
			 *	v3--------v4
			 */

			v1 = vindex + i + j * (subdivisions + 1);
			v2 = v1 + 1;
			v3 = v1 + subdivisions + 1;
			v4 = v3 + 1;

			/* Cut the rectangle (v1,v2,v4,v3) into two triangles by the shortest diagonal */
			v1v4dist = spherified_cube_vertex_distance(m, v1, v4);
			v2v3dist = spherified_cube_vertex_distance(m, v2, v3);
			if (v2v3dist < v1v4dist) {
				/* Make triangles (v1,v2,v3) and (v3,v2,v4). */
				m->t[tindex].v[0] = &m->v[v1];
				m->t[tindex].v[1] = &m->v[v2];
				m->t[tindex].v[2] = &m->v[v3];
				tindex++;

				m->t[tindex].v[0] = &m->v[v3];
				m->t[tindex].v[1] = &m->v[v2];
				m->t[tindex].v[2] = &m->v[v4];
				tindex++;
			} else {
				/* Make triangles (v1,v2,v4) and (v1,v4,v3). */
				m->t[tindex].v[0] = &m->v[v1];
				m->t[tindex].v[1] = &m->v[v2];
				m->t[tindex].v[2] = &m->v[v4];
				tindex++;

				m->t[tindex].v[0] = &m->v[v1];
				m->t[tindex].v[1] = &m->v[v4];
				m->t[tindex].v[2] = &m->v[v3];
				tindex++;
			}
		}
	}
}

struct mesh *mesh_unit_cube(int subdivisions)
{
	struct mesh *m;
	int i, j, face, vindex;
	float xangle, yangle, zangle;
	const double da = (90.0 * M_PI / 180.0) / (double) subdivisions;
	const double start_angle = 45.0 * M_PI / 180.0;

	m = malloc(sizeof(*m));
	if (!m)
		return m;
	memset(m, 0, sizeof(*m));
	m->nvertices = 6 * (subdivisions + 1) * (subdivisions + 1);
	m->ntriangles = 6 * (subdivisions * subdivisions) * 2;

	m->t = malloc(sizeof(*m->t) * m->ntriangles);
	if (!m->t)
		goto bail;
	memset(m->t, 0, sizeof(*m->t) * m->ntriangles);
	m->v = malloc(sizeof(*m->v) * m->nvertices);
	if (!m->v)
		goto bail;
	memset(m->v, 0, sizeof(*m->v) * m->nvertices);
	m->tex = 0;
	/* m->tex = malloc(sizeof(*m->tex) * m->ntriangles * 3);
	if (!m->tex)
		goto bail;
	memset(m->tex, 0, sizeof(*m->tex) * m->ntriangles * 3); */
	m->l = NULL;

	m->geometry_mode = MESH_GEOMETRY_TRIANGLES;

	face = 0; /* normal is positive z */
	vindex = face * (subdivisions + 1) * (subdivisions + 1);
	for (i = 0; i < subdivisions + 1; i++) {
		for (j = 0; j < subdivisions + 1; j++) {
			xangle = start_angle - da * i;
			yangle = -start_angle + da * j;
			m->v[vindex].x = tan(xangle);
			m->v[vindex].y = tan(yangle);
			m->v[vindex].z = 1.0f;
			vindex++;
		}
	}

	face = 1; /* normal is positive x */
	vindex = face * (subdivisions + 1) * (subdivisions + 1);
	for (i = 0; i < subdivisions + 1; i++) {
		for (j = 0; j < subdivisions + 1; j++) {
			yangle = start_angle - da * j;
			zangle = start_angle - da * i;
			m->v[vindex].x = 1.0;
			m->v[vindex].y = tan(yangle);
			m->v[vindex].z = tan(zangle);
			vindex++;
		}
	}

	face = 2; /* normal is negative z */
	vindex = face * (subdivisions + 1) * (subdivisions + 1);
	for (i = 0; i < subdivisions + 1; i++) {
		for (j = 0; j < subdivisions + 1; j++) {
			xangle = -start_angle + da * i;
			yangle = -start_angle + da * j;
			m->v[vindex].x = tan(xangle);
			m->v[vindex].y = tan(yangle);
			m->v[vindex].z = -1.0;
			vindex++;
		}
	}

	face = 3; /* normal is negative x */
	vindex = face * (subdivisions + 1) * (subdivisions + 1);
	for (i = 0; i < subdivisions + 1; i++) {
		for (j = 0; j < subdivisions + 1; j++) {
			yangle = -start_angle + da * j;
			zangle = start_angle - da * i;
			m->v[vindex].x = -1.0;
			m->v[vindex].y = tan(yangle);
			m->v[vindex].z = tan(zangle);
			vindex++;
		}
	}

	face = 4; /* normal is positive y */
	vindex = face * (subdivisions + 1) * (subdivisions + 1);
	for (i = 0; i < subdivisions + 1; i++) {
		for (j = 0; j < subdivisions + 1; j++) {
			xangle = start_angle - da * i;
			zangle = start_angle - da * j;
			m->v[vindex].x = tan(xangle);
			m->v[vindex].y = 1.0f;
			m->v[vindex].z = tan(zangle);
			vindex++;
		}
	}

	face = 5; /* normal is negative y */
	vindex = face * (subdivisions + 1) * (subdivisions + 1);
	for (i = 0; i < subdivisions + 1; i++) {
		for (j = 0; j < subdivisions + 1; j++) {
			xangle = start_angle - da * i;
			zangle = -start_angle + da * j;
			m->v[vindex].x = tan(xangle);
			m->v[vindex].y = -1.0f;
			m->v[vindex].z = tan(zangle);
			vindex++;
		}
	}

	for (i = 0; i < 6; i++)
		make_unit_cube_triangles(m, i, subdivisions);

	mesh_set_flat_shading_vertex_normals(m);
	m->nlines = 0;
	m->radius = mesh_compute_radius(m);
	mesh_graph_dev_init(m);
	return m;
bail:
	mesh_free(m);
	return NULL;
}

struct mesh *mesh_unit_spherified_cube(int subdivisions)
{
	int i;
	union vec3 v;

	struct mesh *m = mesh_unit_cube(subdivisions);
	if (!m)
		return m;

	/* Normalize all the vertices to turn the cube into a sphere */
	for (i = 0; i < m->nvertices; i++) {
		v.v.x = m->v[i].x;
		v.v.y = m->v[i].y;
		v.v.z = m->v[i].z;

		vec3_normalize_self(&v);

		m->v[i].x = v.v.x;
		m->v[i].y = v.v.y;
		m->v[i].z = v.v.z;
	}

	mesh_set_spherical_vertex_normals(m);
	mesh_sample_spherical_cubemap_tangent_and_bitangent(m);
	m->nlines = 0;
	m->radius = mesh_compute_radius(m);
	mesh_graph_dev_init(m);
	return m;
}

/* this has a known issue mapping vertices of tris that span the "international date line" */
void mesh_sphere_uv_map(struct mesh *m)
{
	float u0, v0, u1, v1, u2, v2;
	int i;

	if (m->tex)
		free(m->tex);

	m->tex = malloc(sizeof(*m->tex) * m->ntriangles * 3);
	if (!m->tex)
		return;

	for (i = 0; i < m->ntriangles; i++) {
		struct vertex *vtx0, *vtx1, *vtx2;

		vtx0 = m->t[i].v[0];
		vtx1 = m->t[i].v[1];
		vtx2 = m->t[i].v[2];

		u0 = acosf(vtx0->z) / M_PI;
		v0 = (atan2f(vtx0->y, vtx0->x) + M_PI) / (2.0 * M_PI);
		u1 = acosf(vtx1->z) / M_PI;
		v1 = (atan2f(vtx1->y, vtx1->x) + M_PI) / (2.0 * M_PI);
		u2 = acosf(vtx2->z) / M_PI;
		v2 = (atan2f(vtx2->y, vtx2->x) + M_PI) / (2.0 * M_PI);

		mesh_set_triangle_texture_coords(m, i, u0, v0, u1, v1, u2, v2);
	}
	mesh_graph_dev_init(m);
}

/* UV map points to cylinder aligned with X axis */
/* this has a known issue mapping vertices of tris that span the "international date line" */
void mesh_cylindrical_yz_uv_map(struct mesh *m)
{
	float u0, v0, u1, v1, u2, v2;
	int i;
	float minx, miny, minz, maxx, maxy, maxz;

	minx = 0;
	miny = 0;
	minz = 0;
	maxx = 0;
	maxy = 0;
	maxz = 0;

	if (m->tex)
		free(m->tex);

	m->tex = malloc(sizeof(*m->tex) * m->ntriangles * 3);
	if (!m->tex)
		return;

	for (i = 0; i < m->nvertices; i++) {
		if (i == 0) {
			minx = m->v[i].x;
			maxx = m->v[i].x;
			miny = m->v[i].y;
			maxy = m->v[i].y;
			minz = m->v[i].z;
			maxz = m->v[i].z;
		} else {
			if (m->v[i].x < minx)
				minx = m->v[i].x;
			if (m->v[i].x > maxx)
				maxx = m->v[i].x;
			if (m->v[i].y < miny)
				miny = m->v[i].y;
			if (m->v[i].y > maxy)
				maxy = m->v[i].y;
			if (m->v[i].z < minz)
				minz = m->v[i].z;
			if (m->v[i].z > maxz)
				maxz = m->v[i].z;
		}
	}

	for (i = 0; i < m->ntriangles; i++) {
		struct vertex *vtx0, *vtx1, *vtx2;
		float t0x, t0y, t0z, t1x, t1y, t1z, t2x, t2y, t2z;

		vtx0 = m->t[i].v[0];
		vtx1 = m->t[i].v[1];
		vtx2 = m->t[i].v[2];

		t0x = (vtx0->x - minx) / (maxx - minx);
		t0y = (vtx0->y - miny) / (maxy - miny);
		t0z = (vtx0->z - minz) / (maxz - minz);
		t1x = (vtx1->x - minx) / (maxx - minx);
		t1y = (vtx1->y - miny) / (maxy - miny);
		t1z = (vtx1->z - minz) / (maxz - minz);
		t2x = (vtx2->x - minx) / (maxx - minx);
		t2y = (vtx2->y - miny) / (maxy - miny);
		t2z = (vtx2->z - minz) / (maxz - minz);

		u0 = t0x;
		v0 = (atan2f(2.0 * t0y - 1.0, 2.0 * t0z - 1.0) + M_PI) / (2.0 * M_PI);
		u1 = t1x;
		v1 = (atan2f(2.0 * t1y - 1.0, 2.0 * t1z - 1.0) + M_PI) / (2.0 * M_PI);
		u2 = t2x;
		v2 = (atan2f(2.0 * t2y - 1.0, 2.0 * t2z - 1.0) + M_PI) / (2.0 * M_PI);

		mesh_set_triangle_texture_coords(m, i, u0, v0, u1, v1, u2, v2);
	}
	mesh_graph_dev_init(m);
}

/* UV map points to cylinder aligned with Z axis */
/* this has a known issue mapping vertices of tris that span the "international date line" */
void mesh_cylindrical_xy_uv_map(struct mesh *m)
{
	float u0, v0, u1, v1, u2, v2;
	int i;
	float minx, miny, minz, maxx, maxy, maxz;

	minx = 0;
	miny = 0;
	minz = 0;
	maxx = 0;
	maxy = 0;
	maxz = 0;

	if (m->tex)
		free(m->tex);

	m->tex = malloc(sizeof(*m->tex) * m->ntriangles * 3);
	if (!m->tex)
		return;

	for (i = 0; i < m->nvertices; i++) {
		if (i == 0) {
			minx = m->v[i].x;
			maxx = m->v[i].x;
			miny = m->v[i].y;
			maxy = m->v[i].y;
			minz = m->v[i].z;
			maxz = m->v[i].z;
		} else {
			if (m->v[i].x < minx)
				minx = m->v[i].x;
			if (m->v[i].x > maxx)
				maxx = m->v[i].x;
			if (m->v[i].y < miny)
				miny = m->v[i].y;
			if (m->v[i].y > maxy)
				maxy = m->v[i].y;
			if (m->v[i].z < minz)
				minz = m->v[i].z;
			if (m->v[i].z > maxz)
				maxz = m->v[i].z;
		}
	}

	for (i = 0; i < m->ntriangles; i++) {
		struct vertex *vtx0, *vtx1, *vtx2;
		float t0x, t0y, t0z, t1x, t1y, t1z, t2x, t2y, t2z;

		vtx0 = m->t[i].v[0];
		vtx1 = m->t[i].v[1];
		vtx2 = m->t[i].v[2];

		t0x = (vtx0->x - minx) / (maxx - minx);
		t0y = (vtx0->y - miny) / (maxy - miny);
		t0z = (vtx0->z - minz) / (maxz - minz);
		t1x = (vtx1->x - minx) / (maxx - minx);
		t1y = (vtx1->y - miny) / (maxy - miny);
		t1z = (vtx1->z - minz) / (maxz - minz);
		t2x = (vtx2->x - minx) / (maxx - minx);
		t2y = (vtx2->y - miny) / (maxy - miny);
		t2z = (vtx2->z - minz) / (maxz - minz);

		u0 = t0z;
		v0 = (atan2f(2.0 * t0y - 1.0, 2.0 * t0x - 1.0) + M_PI) / (2.0 * M_PI);
		u1 = t1z;
		v1 = (atan2f(2.0 * t1y - 1.0, 2.0 * t1x - 1.0) + M_PI) / (2.0 * M_PI);
		u2 = t2z;
		v2 = (atan2f(2.0 * t2y - 1.0, 2.0 * t2x - 1.0) + M_PI) / (2.0 * M_PI);

		mesh_set_triangle_texture_coords(m, i, u0, v0, u1, v1, u2, v2);
	}
	mesh_graph_dev_init(m);
}

/* UV map points to cylinder aligned with Y axis */
/* this has a known issue mapping vertices of tris that span the "international date line" */
void mesh_cylindrical_xz_uv_map(struct mesh *m)
{
	float u0, v0, u1, v1, u2, v2;
	int i;
	float minx, miny, minz, maxx, maxy, maxz;

	minx = 0;
	miny = 0;
	minz = 0;
	maxx = 0;
	maxy = 0;
	maxz = 0;

	if (m->tex)
		free(m->tex);

	m->tex = malloc(sizeof(*m->tex) * m->ntriangles * 3);
	if (!m->tex)
		return;

	for (i = 0; i < m->nvertices; i++) {
		if (i == 0) {
			minx = m->v[i].x;
			maxx = m->v[i].x;
			miny = m->v[i].y;
			maxy = m->v[i].y;
			minz = m->v[i].z;
			maxz = m->v[i].z;
		} else {
			if (m->v[i].x < minx)
				minx = m->v[i].x;
			if (m->v[i].x > maxx)
				maxx = m->v[i].x;
			if (m->v[i].y < miny)
				miny = m->v[i].y;
			if (m->v[i].y > maxy)
				maxy = m->v[i].y;
			if (m->v[i].z < minz)
				minz = m->v[i].z;
			if (m->v[i].z > maxz)
				maxz = m->v[i].z;
		}
	}

	for (i = 0; i < m->ntriangles; i++) {
		struct vertex *vtx0, *vtx1, *vtx2;
		float t0x, t0y, t0z, t1x, t1y, t1z, t2x, t2y, t2z;

		vtx0 = m->t[i].v[0];
		vtx1 = m->t[i].v[1];
		vtx2 = m->t[i].v[2];

		t0x = (vtx0->x - minx) / (maxx - minx);
		t0y = (vtx0->y - miny) / (maxy - miny);
		t0z = (vtx0->z - minz) / (maxz - minz);
		t1x = (vtx1->x - minx) / (maxx - minx);
		t1y = (vtx1->y - miny) / (maxy - miny);
		t1z = (vtx1->z - minz) / (maxz - minz);
		t2x = (vtx2->x - minx) / (maxx - minx);
		t2y = (vtx2->y - miny) / (maxy - miny);
		t2z = (vtx2->z - minz) / (maxz - minz);

		u0 = t0y;
		v0 = (atan2f(2.0 * t0z - 1.0, 2.0 * t0x - 1.0) + M_PI) / (2.0 * M_PI);
		u1 = t1y;
		v1 = (atan2f(2.0 * t1z - 1.0, 2.0 * t1x - 1.0) + M_PI) / (2.0 * M_PI);
		u2 = t2y;
		v2 = (atan2f(2.0 * t2z - 1.0, 2.0 * t2x - 1.0) + M_PI) / (2.0 * M_PI);

		mesh_set_triangle_texture_coords(m, i, u0, v0, u1, v1, u2, v2);
	}
	mesh_graph_dev_init(m);
}

static float mesh_calculate_xy_radius(struct mesh *m)
{
	float maxy = 0.0f, maxx = 0.0f;
	int i;

	for (i = 0; i < m->nvertices; i++) {
		if (fabs(m->v[i].y) > maxy)
			maxy = fabs(m->v[i].y);
		if (fabs(m->v[i].x) > maxx)
			maxx = fabs(m->v[i].x);
	}
	if (maxy > maxx)
		return maxy;
	return maxx;
}

static int outside_edge_of_ring(struct vertex *v, float r)
{
	float R = v->x * v->x + v->y * v->y;
	return R >= (r * r) * 0.95;
}

static void mesh_uv_map_planetary_ring(struct mesh *m)
{
	float u0, u1, u2, r;
	int i;

	r = mesh_calculate_xy_radius(m);
	if (m->tex)
		free(m->tex);

	m->tex = malloc(sizeof(*m->tex) * m->ntriangles * 3);
	if (!m->tex)
		return;

	for (i = 0; i < m->ntriangles; i++) {
		struct vertex *vtx0, *vtx1, *vtx2;

		vtx0 = m->t[i].v[0];
		vtx1 = m->t[i].v[1];
		vtx2 = m->t[i].v[2];

		/* v will be a per-instance constant passed as uniform to the shader */
		u0 = (float) outside_edge_of_ring(vtx0, r);
		u1 = (float) outside_edge_of_ring(vtx1, r);
		u2 = (float) outside_edge_of_ring(vtx2, r);

		mesh_set_triangle_texture_coords(m, i, u0, 0.0f, u1, 0.0f, u2, 0.0f);
	}
	mesh_graph_dev_init(m);
}

void mesh_map_xy_to_uv(struct mesh *m)
{
	float u0, v0, u1, v1, u2, v2, r;
	int i;

	r = mesh_calculate_xy_radius(m);
	if (m->tex)
		free(m->tex);

	m->tex = malloc(sizeof(*m->tex) * m->ntriangles * 3);
	if (!m->tex)
		return;

	for (i = 0; i < m->ntriangles; i++) {
		struct vertex *vtx0, *vtx1, *vtx2;

		vtx0 = m->t[i].v[0];
		vtx1 = m->t[i].v[1];
		vtx2 = m->t[i].v[2];

		u0 = vtx0->x / (r * 2.0) + 0.5;
		v0 = vtx0->y / (r * 2.0) + 0.5;
		u1 = vtx1->x / (r * 2.0) + 0.5;
		v1 = vtx1->y / (r * 2.0) + 0.5;
		u2 = vtx2->x / (r * 2.0) + 0.5;
		v2 = vtx2->y / (r * 2.0) + 0.5;

		mesh_set_triangle_texture_coords(m, i, u0, v0, u1, v1, u2, v2);
	}
	mesh_graph_dev_init(m);
}

void mesh_unit_cube_uv_map(struct mesh *m)
{
	float u0, v0, u1, v1, u2, v2;
	int i;

	if (m->tex)
		free(m->tex);

	m->tex = malloc(sizeof(*m->tex) * m->ntriangles * 3);
	if (!m->tex)
		return;

	for (i = 0; i < m->ntriangles; i++) {
		struct vertex *vtx0, *vtx1, *vtx2;

		vtx0 = m->t[i].v[0];
		vtx1 = m->t[i].v[1];
		vtx2 = m->t[i].v[2];

		if (fabs(vtx0->x - vtx2->x) < 0.01 && fabs(vtx0->x - vtx1->x) < 0.01) {
			/* x is constant, y and z control the mapping */
			u0 = vtx0->z + 0.5;
			v0 = vtx0->y + 0.5;
			u1 = vtx1->z + 0.5;
			v1 = vtx1->y + 0.5;
			u2 = vtx2->z + 0.5;
			v2 = vtx2->y + 0.5;
		} else if (fabs(vtx0->y - vtx2->y) < 0.01 && fabs(vtx0->y - vtx1->y) < 0.01) {
			/* y is constant, x and z control the mapping */
			u0 = vtx0->z + 0.5;
			v0 = vtx0->x + 0.5;
			u1 = vtx1->z + 0.5;
			v1 = vtx1->x + 0.5;
			u2 = vtx2->z + 0.5;
			v2 = vtx2->x + 0.5;
		} else {
			u0 = vtx0->x + 0.5;
			v0 = vtx0->y + 0.5;
			u1 = vtx1->x + 0.5;
			v1 = vtx1->y + 0.5;
			u2 = vtx2->x + 0.5;
			v2 = vtx2->y + 0.5;
		}
		mesh_set_triangle_texture_coords(m, i, u0, v0, u1, v1, u2, v2);
	}
	mesh_graph_dev_init(m);
}

struct mesh *mesh_fabricate_planetary_ring(float ir, float or, int nvertices)
{
	struct mesh *m;
	int i;

	m = malloc(sizeof(*m));
	if (!m)
		return m;
	memset(m, 0, sizeof(*m));
	m->nvertices = nvertices;
	m->ntriangles = m->nvertices;

	m->t = malloc(sizeof(*m->t) * m->ntriangles);
	if (!m->t)
		goto bail;
	memset(m->t, 0, sizeof(*m->t) * m->ntriangles);
	m->v = malloc(sizeof(*m->v) * m->nvertices);
	if (!m->v)
		goto bail;
	memset(m->v, 0, sizeof(*m->v) * m->nvertices);
	m->l = NULL;

	m->geometry_mode = MESH_GEOMETRY_TRIANGLES;


	/* set up vertices */
	for (i = 0; i < m->nvertices; i += 2) {
		const float angle = ((2 * M_PI)  * i) / m->nvertices;
		m->v[i].x = cos(angle) * ir;
		m->v[i].y = sin(angle) * ir;
		m->v[i].z = 0.0;
		m->v[i + 1].x = cos(angle) * or;
		m->v[i + 1].y = sin(angle) * or;
		m->v[i + 1].z = 0.0;
	}

	/* set up triangles */
	for (i = 0; i < m->nvertices; i += 2) {
		struct vertex *v1, *v2, *v3, *v4;

		v1 = &m->v[i % m->nvertices];
		v2 = &m->v[(i + 1) % m->nvertices];
		v3 = &m->v[(i + 2) % m->nvertices];
		v4 = &m->v[(i + 3) % m->nvertices];
		m->t[i].v[0] = v3;
		m->t[i].v[1] = v2;
		m->t[i].v[2] = v1;
		m->t[i + 1].v[0] = v2;
		m->t[i + 1].v[1] = v3;
		m->t[i + 1].v[2] = v4;
		m->t[i].flag = TRIANGLE_0_1_COPLANAR;
		m->t[i + 1].flag = TRIANGLE_0_1_COPLANAR;
	}
	m->radius = mesh_compute_radius(m);
	mesh_set_flat_shading_vertex_normals(m);
	mesh_uv_map_planetary_ring(m);
	return m;
bail:
	mesh_free(m);
	return NULL;
}

static void __attribute__((unused)) uniform_random_point_in_spehere(float r, float *x, float *y)
{
	float a = M_PI * snis_random_float();
	r = r * sqrt(fabs(snis_random_float()));
	*x = r * cos(a);
	*y = r * sin(a);
}

struct particle {
	float lifetime;
	float decay;
	float xpos, ypos, zpos;
	float xspeed, yspeed, zspeed;
	int active;
	float offset;
};

static void create_particle(float h, float r1, struct particle *particles, int i)
{
	float angle, r;
	particles[i].offset = fabs(snis_random_float());
	particles[i].lifetime = 0.5 * fabs(snis_random_float()) + 0.5;
	particles[i].decay = 0.1;

	angle = snis_random_float() * M_PI * 2;
	r = snis_random_float();
	particles[i].xpos = 0.0;
	particles[i].ypos = 0.0 + cos(angle) * r * r1;
	particles[i].zpos = 0.0 + sin(angle) * r * r1;

	/* uniform_random_point_in_spehere(r1 * 0.1, &particles[i].yspeed, &particles[i].zspeed); */
	particles[i].xspeed = -fabs(snis_random_float() * 0.5 + 0.5) * h * 0.1;
	particles[i].yspeed = 0;
	particles[i].zspeed = 0;

	particles[i].active = 1;
}

static void evolve_particle(float h, struct particle *particles, int i)
{
	particles[i].lifetime -= particles[i].decay;
	particles[i].xpos += particles[i].xspeed;
	particles[i].ypos += particles[i].yspeed;
	particles[i].zpos += particles[i].zspeed;
	/* particles[i].yspeed -= fabs(snis_random_float()) * h * 0.2; */
	/* particles[i].zspeed -= fabs(snis_random_float()) * h * 0.2; */
}


struct mesh *init_thrust_mesh(int streaks, double h, double r1, double r2)
{
#define MAX_THRUST_MESH_STREAKS 70
	struct mesh *my_mesh = malloc(sizeof(*my_mesh));

	if (!my_mesh)
		return my_mesh;
	memset(my_mesh, 0, sizeof(*my_mesh));

	my_mesh->geometry_mode = MESH_GEOMETRY_PARTICLE_ANIMATION;

	if (streaks > MAX_THRUST_MESH_STREAKS)
		streaks = MAX_THRUST_MESH_STREAKS;
	my_mesh->nlines = streaks * 50;
	my_mesh->nvertices = my_mesh->nlines * 2;
	my_mesh->ntriangles = 0;
	my_mesh->t = 0;
	my_mesh->v = malloc(sizeof(*my_mesh->v) * my_mesh->nvertices);
	my_mesh->l = malloc(sizeof(*my_mesh->l) * my_mesh->nlines);
	my_mesh->tex = 0;
	my_mesh->radius = h;
	my_mesh->graph_ptr = 0;

	int maxparticle = streaks;
	struct particle particles[MAX_THRUST_MESH_STREAKS];
	int i;
	for (i = 0; i < maxparticle; i++)
		create_particle(h, r1, particles, i);

	int line_index = 0;

	while (1) {
		int one_is_active = 0;
		for (i = 0; i < maxparticle; i++) {
			if (!particles[i].active)
				continue;

			float x1 = particles[i].xpos;
			float y1 = particles[i].ypos;
			float z1 = particles[i].zpos;

			evolve_particle(h, particles, i);

			if (particles[i].lifetime < 0) {
				particles[i].active = 0;
				continue;
			}

			one_is_active = 1;
			float x2 = particles[i].xpos;
			float y2 = particles[i].ypos;
			float z2 = particles[i].zpos;

			int v_index = line_index * 2;
			my_mesh->v[v_index + 0].x = x1;
			my_mesh->v[v_index + 0].y = y1;
			my_mesh->v[v_index + 0].z = z1;
			my_mesh->v[v_index + 1].x = x2;
			my_mesh->v[v_index + 1].y = y2;
			my_mesh->v[v_index + 1].z = z2;

			my_mesh->l[line_index].start = &my_mesh->v[v_index + 0];
			my_mesh->l[line_index].end = &my_mesh->v[v_index + 1];
			my_mesh->l[line_index].flag = 0;
			my_mesh->l[line_index].additivity = 1.0;
			my_mesh->l[line_index].opacity = particles[i].lifetime;
			my_mesh->l[line_index].tint_color.red = 1.0;
			my_mesh->l[line_index].tint_color.green = 1.0;
			my_mesh->l[line_index].tint_color.blue = 1.0;
			my_mesh->l[line_index].time_offset = particles[i].offset;

			line_index++;
			if (line_index >= my_mesh->nlines) {
				one_is_active = 0;
				break;
			}
		}
		if (!one_is_active)
			break;
	}

	my_mesh->nlines = line_index;
	my_mesh->nvertices = line_index * 2;

	struct mesh *optimized_mesh = mesh_duplicate(my_mesh);
	mesh_free(my_mesh);

	return optimized_mesh;
}

struct mesh *init_burst_rod_mesh(int streaks, double h, double r1, double r2)
{
	struct mesh *my_mesh = malloc(sizeof(*my_mesh));

	if (!my_mesh)
		return my_mesh;
	memset(my_mesh, 0, sizeof(*my_mesh));

	my_mesh->geometry_mode = MESH_GEOMETRY_PARTICLE_ANIMATION;

	my_mesh->nlines = streaks;
	my_mesh->nvertices = my_mesh->nlines * 2;
	my_mesh->ntriangles = 0;
	my_mesh->t = 0;
	my_mesh->v = malloc(sizeof(*my_mesh->v) * my_mesh->nvertices);
	my_mesh->l = malloc(sizeof(*my_mesh->l) * my_mesh->nlines);
	my_mesh->tex = 0;
	my_mesh->radius = fmax(sqrt(h * h / 4.0 + r1 * r1), sqrt(h * h / 4.0 + r2 * r2));
	my_mesh->graph_ptr = 0;

	int line_index;
	for (line_index = 0; line_index < streaks; line_index++) {

		float p = fabs(snis_random_float());
		float pr = p * (r2 - r1) + r1;
		float ph = p * h - h / 2.0;
		float pa = fabs(snis_random_float()) * 2.0 * M_PI;

		int v_index = line_index * 2;
		my_mesh->v[v_index + 0].x = ph;
		my_mesh->v[v_index + 0].y = 0;
		my_mesh->v[v_index + 0].z = 0;
		my_mesh->v[v_index + 1].x = ph;
		my_mesh->v[v_index + 1].y = sin(pa) * pr;
		my_mesh->v[v_index + 1].z = cos(pa) * pr;

		my_mesh->l[line_index].start = &my_mesh->v[v_index + 0];
		my_mesh->l[line_index].end = &my_mesh->v[v_index + 1];
		my_mesh->l[line_index].flag = 0;
		my_mesh->l[line_index].additivity = 1.0;
		my_mesh->l[line_index].opacity = 1.0;
		my_mesh->l[line_index].tint_color.red = 1.0;
		my_mesh->l[line_index].tint_color.green = 1.0;
		my_mesh->l[line_index].tint_color.blue = 1.0;
		my_mesh->l[line_index].time_offset = fabs(snis_random_float());
	}

	mesh_graph_dev_init(my_mesh);
	return my_mesh;
}

void mesh_update_material(struct mesh *m, struct material *material)
{
	if (m->material)
		free(m->material);
	m->material = material;
}

static void vertex_rotate(struct vertex *v, union quat *q)
{
	union vec3 vo,  vi = { { v->x, v->y, v->z } };

	quat_rot_vec(&vo, &vi, q);
	v->x = vo.v.x;
	v->y = vo.v.y;
	v->z = vo.v.z;
}

static void triangle_rotate_normals(struct triangle *t, union quat *q)
{
	union vec3 vo;
	int i;

	union vec3 vi = { { t->n.x, t->n.y, t->n.z } };
	quat_rot_vec(&vo, &vi, q);
	t->n.x = vo.v.x;
	t->n.y = vo.v.y;
	t->n.z = vo.v.z;

	for (i = 0; i < 3; i++) {
		union vec3 vi = { { t->vnormal[i].x, t->vnormal[i].y, t->vnormal[i].z } };
		quat_rot_vec(&vo, &vi, q);
		t->vnormal[i].x = vo.v.x;
		t->vnormal[i].y = vo.v.y;
		t->vnormal[i].z = vo.v.z;
	}
}


void mesh_rotate(struct mesh *m, union quat *q)
{
	int i;

	for (i = 0; i < m->nvertices; i++)
		vertex_rotate(&m->v[i], q);
	for (i = 0; i < m->ntriangles; i++)
		triangle_rotate_normals(&m->t[i], q);
	mesh_graph_dev_init(m);
}

/* fabricate a tube of length h, radius r, with nfaces, parallel to x axis */
struct mesh *mesh_tube(float h, float r, float nfaces)
{
	struct mesh *m;
	int ntris = nfaces * 2;
	int nvertices = nfaces * 2;
	int i, j;
	float angle, da;

	float l = h / 2.0;

	m = allocate_mesh_for_copy(ntris, nvertices, 0, 1);
	if (!m)
		return m;

	m->geometry_mode = MESH_GEOMETRY_TRIANGLES;
	da = 2 * M_PI / (float) nfaces;
	angle = 0.0;
	for (i = 0; i < nvertices; i += 2) {
		m->v[i].x = -l;
		m->v[i].y = r * cos(angle);
		m->v[i].z = r * -sin(angle);
		m->v[i + 1].x = l;
		m->v[i + 1].y = m->v[i].y;
		m->v[i + 1].z = m->v[i].z;
		angle += da;
	}
	m->nvertices = nvertices;

	for (i = 0; i < ntris; i += 2) {
		m->t[i].v[2] = &m->v[i % nvertices];
		m->t[i].v[1] = &m->v[(i + 1) % nvertices];
		m->t[i].v[0] = &m->v[(i + 2) % nvertices];

		m->t[i + 1].v[2] = &m->v[(i + 2) % nvertices];
		m->t[i + 1].v[1] = &m->v[(i + 1) % nvertices];
		m->t[i + 1].v[0] = &m->v[(i + 3) % nvertices];
	}
	m->ntriangles = ntris;

	for (i = 0; i < ntris; i++) {
		union vec3 normal;
		normal = compute_triangle_normal(&m->t[i]);
		m->t[i].n.x = normal.v.x;
		m->t[i].n.y = normal.v.y;
		m->t[i].n.z = normal.v.z;
		for (j = 0; j < 3; j++) {
			union vec3 normal = { { 0, -m->t[i].v[j]->y, -m->t[i].v[j]->z } };
			vec3_normalize_self(&normal);
			m->t[i].vnormal[j].x = normal.v.x;
			m->t[i].vnormal[j].y = normal.v.y;
			m->t[i].vnormal[j].z = normal.v.z;
		}
	}
	mesh_set_flat_shading_vertex_normals(m);
	for (i = 0; i < ntris; i += 2) {
		mesh_set_triangle_texture_coords(m, i,
			0.0, (float) ((int) ((i + 2) / 2)) * 1.0 / (float) nfaces,
			1.0, (float) ((int) (i / 2)) / (float) nfaces,
			0.0, (float) ((int) (i / 2)) / (float) nfaces);
		mesh_set_triangle_texture_coords(m, i + 1,
			1.0, (float) ((int) ((i + 2) / 2)) / (float) nfaces,
			1.0, (float) ((int) (i / 2)) / (float) nfaces,
			0.0, (float) ((int) ((i + 2) / 2)) / (float) nfaces);
	}
	m->nlines = 0;
	m->radius = mesh_compute_radius(m);
	mesh_graph_dev_init(m);
	return m;
}

/* Given point x, y, z and assuming mesh m is at origin, return the index of the
 * closest vertex scaled by (sx, sy, sz) to x, y, z.  Distance is returned in *dist,
 * unless dist is NULL.
 */
int mesh_nearest_vertex(struct mesh *m, float x, float y, float z,
				float sx, float sy, float sz, float *dist)
{
	int i, answer;
	float d, dist2 = -1;
	struct vertex *v;

	answer = -1;
	for (i = 0; i < m->nvertices; i++) {
		v = &m->v[i];
		d = (v->x * sx - x) * (v->x * sx - x) +
			(v->y * sy - y) * (v->y * sy - y) +
			(v->z * sz - z) * (v->z * sz - z);
		if (dist2 < 0 || d < dist2) {
			dist2 = d;
			answer = i;
		}
	}
	if (answer >= 0 && dist)
			*dist = sqrtf(dist2);
	return answer;
}

/* Return axis aligned bounding box for a mesh in min[x,y,z], max[x,y,z] */
void mesh_aabb(struct mesh *m, float *minx, float *miny, float *minz, float *maxx, float *maxy, float *maxz)
{
	int i;

	*minx = 0;
	*miny = 0;
	*minz = 0;
	*maxx = 0;
	*maxy = 0;
	*maxz = 0;

	if (m->nvertices < 1)
		return;
	*minx = m->v[0].x;
	*miny = m->v[0].y;
	*minz = m->v[0].z;
	*maxx = m->v[0].x;
	*maxy = m->v[0].y;
	*maxz = m->v[0].z;

	for (i = 0; i < m->nvertices; i++) {
		if (m->v[i].x < *minx)
			*minx = m->v[i].x;
		if (m->v[i].x > *maxx)
			*maxx = m->v[i].x;
		if (m->v[i].y < *miny)
			*miny = m->v[i].y;
		if (m->v[i].y > *maxy)
			*maxy = m->v[i].y;
		if (m->v[i].z < *minz)
			*minz = m->v[i].z;
		if (m->v[i].z > *maxz)
			*maxz = m->v[i].z;
	}
	return;
}

/* Functions to allow mikktspace library to interface with our mesh representation */
static int mikktspace_get_num_faces(const SMikkTSpaceContext *pContext)
{
	struct mesh *m = pContext->m_pUserData;

	return m->ntriangles;
}

static int mikktspace_get_num_vertices_of_face(const SMikkTSpaceContext *pContext, const int iFace)
{
	return 3;
}

static void mikktspace_get_position(const SMikkTSpaceContext *pContext, float fvPosOut[],
					const int iFace, const int iVert)
{
	struct mesh *m = pContext->m_pUserData;
	struct triangle *t = &m->t[iFace];
	struct vertex *v = t->v[iVert];

	fvPosOut[0] = v->x;
	fvPosOut[1] = v->y;
	fvPosOut[2] = v->z;
}

static void mikktspace_get_normal(const SMikkTSpaceContext *pContext, float fvNormOut[],
					const int iFace, const int iVert)
{
	struct mesh *m = pContext->m_pUserData;
	struct triangle *t = &m->t[iFace];
	struct vertex *v = &t->vnormal[iVert];

	fvNormOut[0] = v->x;
	fvNormOut[1] = v->y;
	fvNormOut[2] = v->z;
}

static void mikktspace_get_tex_coord(const SMikkTSpaceContext *pContext, float fvTexcOut[],
					const int iFace, const int iVert)
{
	struct mesh *m = pContext->m_pUserData;

	fvTexcOut[0] = m->tex[iFace * 3 + iVert].u;
	fvTexcOut[1] = m->tex[iFace * 3 + iVert].v;
}

static void mikktspace_set_t_space_basic(const SMikkTSpaceContext *pContext, const float fvTangent[],
				const float fSign, const int iFace, const int iVert)
{
	struct mesh *m = pContext->m_pUserData;
	struct triangle *t = &m->t[iFace];
	union vec3 normal, tangent, bitangent;

	/* Note this comment from mikktspace.h:
	 *
	 * -------
	 * This function is used to return the tangent and fSign to the application.
	 * fvTangent is a unit length vector.
	 * For normal maps it is sufficient to use the following simplified version of the
	 * bitangent which is generated at pixel/vertex level.
	 *
	 * bitangent = fSign * cross(vN, tangent);
	 *
	 * Note that the results are returned unindexed. It is possible to generate a new index list
	 * But averaging/overwriting tangent spaces by using an already existing index list WILL
	 * produce INCRORRECT results.  DO NOT! use an already existing index list.
	 * -------
	 *
	 * I am not quite sure what those last 3 lines mean. I think they *might* mean that
	 * if you were to try to store "the" normal/tangent/bitangent (NTB) with the vertex,
	 * and vertices might be shared among triangles (this latter being true in our case),
	 * you would run into problems. However, we store 3 NTBs per triangle, and we don't
	 * store them in the vertices, so two triangles will never share the same storage for
	 * NTBs for any vertices, so I *think* * that means we're fine here. I think what is
	 * meant by "index list", is to have an array(s) of NTBs that is separate from the
	 * triangles, with the triangles containing indices into this NTB array(s), with
	 * different triangles potentially sharing NTB entries for shared vertices. We don't
	 * do that.
	 */

	normal.v.x = t->vnormal[iVert].x;
	normal.v.y = t->vnormal[iVert].y;
	normal.v.z = t->vnormal[iVert].z;

	tangent.v.x = fvTangent[0];
	tangent.v.y = fvTangent[1];
	tangent.v.z = fvTangent[2];

	vec3_cross(&bitangent, &normal, &tangent);
	vec3_mul_self(&bitangent, fSign);

	t->vtangent[iVert].x = fvTangent[0];
	t->vtangent[iVert].y = fvTangent[1];
	t->vtangent[iVert].z = fvTangent[2];
	t->vtangent[iVert].w = 1.0; /* Put fSign in w, so we can calc bitangent in pixel shader? */

	t->vbitangent[iVert].x = bitangent.v.x;
	t->vbitangent[iVert].y = bitangent.v.y;
	t->vbitangent[iVert].z = bitangent.v.z;
	t->vbitangent[iVert].w = 1.0;
}

#if 0
/* TODO: fill this in if needed */
static void mikktspace_set_t_space(const SMikkTSpaceContext *pContext, const float fvTangent[],
				const float fvBiTangent[], const float fMagS, const float fMagT,
				const tbool bIsOrientationPreserving, const int iFace,
				const int iVert)
{
}
#endif

static SMikkTSpaceInterface mikktspace_interface = {
	.m_getNumFaces			= mikktspace_get_num_faces,
	.m_getNumVerticesOfFace		= mikktspace_get_num_vertices_of_face,
	.m_getPosition			= mikktspace_get_position,
	.m_getNormal			= mikktspace_get_normal,
	.m_getTexCoord			= mikktspace_get_tex_coord,
	.m_setTSpaceBasic		= mikktspace_set_t_space_basic,
#if 0
	/* TODO: fill this in if needed */
	.m_setTSpace			= mikktspace_set_t_space,
#else
	.m_setTSpace			= NULL,
#endif
};

void mesh_set_mikktspace_tangents_and_bitangents(struct mesh *m)
{

	struct SMikkTSpaceContext mikktspace_context;

	mikktspace_context.m_pInterface = &mikktspace_interface;
	mikktspace_context.m_pUserData = m;
	genTangSpaceDefault(&mikktspace_context);
}
