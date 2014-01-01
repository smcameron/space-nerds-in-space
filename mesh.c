#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "vertex.h"
#include "triangle.h"
#include "mathutils.h"
#include "matrix.h"
#include "quat.h"

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
	mesh_set_flat_shading_vertex_normals(m);
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
		m->v[i].x += m->v[i].x * scale;
		m->v[i].y += m->v[i].y * scale;
		m->v[i].z += m->v[i].z * scale;
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
	if (original->tex) {
		copy->tex = malloc(sizeof(*copy->tex) * original->ntriangles * 3);
		memcpy(copy->tex, original->tex, sizeof(*copy->tex) * original->ntriangles * 3);
	} else {
		copy->tex = 0;
	}
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
	}
	copy->radius = original->radius;

	mesh_graph_dev_init(copy);
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
	my_mesh->tex = 0;
	my_mesh->radius = r;
	my_mesh->graph_ptr = 0;

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

	mesh_graph_dev_init(my_mesh);
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
		for (j = 0; j < 3; j++)
			m->t[i].vnormal[j] = m->t[i].n;
	}
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
	if (m) {
		if (m->t)
			free(m->t);
		if (m->v)
			free(m->v);
		if (m->tex)
			free(m->tex);
		free(m);
	}
	return NULL;
}

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
	m->v[0].x = 0;
	m->v[0].y = height / 2.0f;
	m->v[0].z = width / 2.0f;
	m->v[1].x = 0;
	m->v[1].y = height / 2.0f;
	m->v[1].z = -width / 2.0f;
	m->v[2].x = 0;
	m->v[2].y = -height / 2.0f;
	m->v[2].z = -width / 2.0f;
	m->v[3].x = 0;
	m->v[3].y = -height / 2.0f;
	m->v[3].z = width / 2.0f;

	m->t[0].v[0] = &m->v[0];
	m->t[0].v[1] = &m->v[1];
	m->t[0].v[2] = &m->v[2];
	mesh_set_triangle_texture_coords(m, 0, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f);

	m->t[1].v[0] = &m->v[1];
	m->t[1].v[1] = &m->v[3];
	m->t[1].v[2] = &m->v[2];
	mesh_set_triangle_texture_coords(m, 1, 1.0f, 1.0f, 1.0f, 0.0f, 0.0f, 0.0f);

	mesh_compute_radius(m);
	mesh_set_flat_shading_vertex_normals(m);
	mesh_graph_dev_init(m);

	return m;

bail:
	if (m) {
		if (m->t)
			free(m->t);
		if (m->v)
			free(m->v);
		if (m->tex)
			free(m->tex);
		free(m);
	}
	return NULL;
}

