#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "vertex.h"
#include "triangle.h"
#include "mtwist.h"
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

void mesh_distort_helper(struct mesh *m, float distortion)
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

static struct mesh *allocate_mesh_for_copy(int ntriangles, int nvertices, int nlines,
						int with_texture)
{
	struct mesh *copy;

	copy = malloc(sizeof(*copy));
	if (!copy)
		goto bail;
	copy->t = NULL;
	copy->v = NULL;
	copy->l = NULL;
	copy->tex = NULL;
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
	if (with_texture) {
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
		copy->l[i].alpha = original->l[i].alpha;
		copy->l[i].time_offset = original->l[i].time_offset;
	}
	if (original->tex)
		memcpy(copy->tex, original->tex, sizeof(*copy->tex) * original->ntriangles * 3);
	copy->radius = original->radius;

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

	/* make sure we always have a valid normal */
	if (isnan(cross.v.x) || isnan(cross.v.y) || isnan(cross.v.z))
		vec3_init(&cross, 0, 1, 0);

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
 *               .-- x = cx
 *    -x <--     V     ---> +x
 * +y       0         1
 * ^   (0,1) +-------+ (1,1)
 * |         |\      |
 * |         | \     |
 *           |  \    |
 * y = cy -> |   \   |
 *           |    \  |
 * |         |     \ |
 * |   (0,0) |      \| (1,0)
 * v       3 +-------+ 2
 * -y
 *          normal = +z
 */

struct mesh *mesh_fabricate_billboard(float cx, float cy, float width, float height)
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
	m->v[0].x = -width / 2.0f + cx;
	m->v[0].y = height / 2.0f + cy;
	m->v[0].z = 0;
	m->v[1].x = width / 2.0f + cx;
	m->v[1].y = height / 2.0f + cy;
	m->v[1].z = 0;
	m->v[2].x = width / 2.0f + cx;
	m->v[2].y = -height / 2.0f + cy;
	m->v[2].z = 0;
	m->v[3].x = -width / 2.0f + cx;
	m->v[3].y = -height / 2.0f + cy;
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
struct mesh *mesh_unit_icosohedron(void)
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
	struct mesh *m = mesh_unit_icosohedron();
	struct mesh *m2, *m3;

	if (!m)
		return NULL;

	/* note mesh_subdivide_icosphere will free m */
	m2 = mesh_subdivide_icosphere(m, subdivisions);

	/* m2 will be over-allocated, duplicating will clean up the overallocation */
	m3 = mesh_duplicate(m2);
	mesh_free(m2);
	mesh_set_spherical_vertex_normals(m3);
	return m3;
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

struct mesh *mesh_fabricate_planetary_ring(float ir, float or)
{
	struct mesh *m;
	int i;

	m = malloc(sizeof(*m));
	if (!m)
		return m;
	memset(m, 0, sizeof(*m));
	m->nvertices = 100;
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
		/* FIXME: set coplanar flags */
	}
	m->radius = mesh_compute_radius(m);
	mesh_set_flat_shading_vertex_normals(m);
	mesh_map_xy_to_uv(m);
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
	particles[i].offset = fabs(snis_random_float());
	particles[i].lifetime = fabs(snis_random_float());
	particles[i].decay = 0.1;

	particles[i].xpos = 0.0;
	particles[i].ypos = 0.0;
	particles[i].zpos = 0.0;

	/* uniform_random_point_in_spehere(r1 * 0.1, &particles[i].yspeed, &particles[i].zspeed); */
	particles[i].xspeed = -fabs(snis_random_float()) * h * 0.1;
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
	struct mesh *my_mesh = malloc(sizeof(*my_mesh));

	my_mesh->geometry_mode = MESH_GEOMETRY_PARTICLE_ANIMATION;

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
	struct particle particles[maxparticle];
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
			my_mesh->l[line_index].alpha = particles[i].lifetime;
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
