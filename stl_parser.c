/*
	Copyright (C) 2010 Stephen M. Cameron
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
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <inttypes.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <math.h>
#include <limits.h>

#include "vertex.h"
#include "triangle.h"
#include "matrix.h"
#include "mtwist.h"
#include "quat.h"
#include "mesh.h"
#include "string-utils.h"
#include "snis_graph.h"
#include "graph_dev.h"
#include "material.h"
#include "stacktrace.h"

#define DEFINE_STL_FILE_GLOBALS
#include "stl_parser.h"

/* consider two faces to be smoothed if <37 degrees */
#define DEFAULT_SHARP_CORNER_ANGLE (37.0 * M_PI / 180.0)

static void stl_error(char *msg, int linecount)
{
	fprintf(stderr, "%d: %s\n", linecount, msg);
}

static int count_facets(char *filename)
{
	int fd, rc;
	struct stat buf;
	char *haystack, *needle;
	int count = 0;
	char *tempstr;

	fd = open(filename, O_RDONLY);
	if (fd < 0)
		return fd;
	rc = fstat(fd, &buf);
	if (rc < 0) {
		close(fd);
		return rc;
	}
	tempstr = malloc(buf.st_size + 1);
	rc = read(fd, tempstr, buf.st_size);
	if (rc < 0) {
		fprintf(stderr, "stl_parser: Error reading %"PRIu64" bytes of %s: %s\n",
			(uint64_t) buf.st_size, filename, strerror(errno));
		free(tempstr);
		close(fd);
		return rc;
	}
	if (rc != buf.st_size) {
		fprintf(stderr, "stl_parser: Tried reading %"PRIu64" bytes of %s, got only %d\n",
			(uint64_t) buf.st_size, filename, rc);
		free(tempstr);
		close(fd);
		return rc;
	}	
	tempstr[buf.st_size] = '\0';
	close(fd);

	haystack = tempstr;
	do {
		needle = strstr(haystack, "facet normal");
		if (!needle) {
			free(tempstr);
			return count;
		}
		count++;
		haystack = needle + 1;
	} while (1);
}

static void calculate_triangle_normal(struct triangle *t)
{
	union vec3 v1, v2, v3;

	v1.v.x = t->v[1]->x - t->v[0]->x;
	v1.v.y = t->v[1]->y - t->v[0]->y;
	v1.v.z = t->v[1]->z - t->v[0]->z;
	v2.v.x = t->v[2]->x - t->v[1]->x;
	v2.v.y = t->v[2]->y - t->v[1]->y;
	v2.v.z = t->v[2]->z - t->v[1]->z;
	vec3_cross(&v3, &v1, &v2);
	if (vec3_magnitude(&v3) < 1e-20)
		vec3_init(&v3, 1, 0, 0); /* Make sure normalizing won't produce NaNs */
	vec3_normalize_self(&v3);
	t->n.x = v3.v.x;
	t->n.y = v3.v.y;
	t->n.z = v3.v.z;
}

static int read_facet(FILE *f, struct triangle *t, int *linecount)
{
	int i, rc;

	rc = fscanf(f, " facet normal %f %f %f\n", &t->n.x, &t->n.y, &t->n.z);
	t->n.w = 1.0;
	t->n.ww = 1.0;
	(*linecount)++;
	if (rc != 3) {
		fprintf(stderr, "failed reading normal at line %d\n", *linecount);
		return -1;
	}
	rc = fscanf(f, " outer loop\n");
	(*linecount)++;
	if (rc != 0) {
		fprintf(stderr, "failed 'outer loop' at line %d\n", *linecount);
		return -1;
	}
	for (i = 0; i < 3; i++) {
		rc = fscanf(f, " vertex %f %f %f\n", &t->v[i]->x, &t->v[i]->y, &t->v[i]->z);
		(*linecount)++;
		if (rc != 3) {
			fprintf(stderr, "failed reading vertex at line %d\n", *linecount);
			return -1;
		}
	}
	/* If they didn't bother to figure the normal, figure it now. */
	if (t->n.x == 0.0f && t->n.y == 0.0f && t->n.z == 0.0f)
		calculate_triangle_normal(t);
	rc = fscanf(f, " endloop\n");
	(*linecount)++;
	if (rc != 0) {
		fprintf(stderr, "failed 'endloop' at line %d\n", *linecount);
		return -1;
	}
	rc = fscanf(f, " endfacet\n");
	(*linecount)++;
	if (rc != 0) {
		fprintf(stderr, "failed 'endfacet' at line %d\n", *linecount);
		return -1;
	}
	return 0;
}

static float dist3d_v(struct vertex *v1, struct vertex *v2)
{
	return sqrt(
		(v1->x - v2->x) * (v1->x - v2->x) +
		(v1->y - v2->y) * (v1->y - v2->y) +
		(v1->z - v2->z) * (v1->z - v2->z));
}

#define VERTEX_MERGING_THRESHOLD (1e-18)
static struct vertex *add_vertex(struct mesh *m, struct vertex *v, int* is_shared)
{
	/* Search vertices to see if we already have this one. */
	int i;

	for (i = 0; i < m->nvertices; i++) {
		if (dist3d_v(v, &m->v[i]) < VERTEX_MERGING_THRESHOLD) {
			*is_shared = 1;
			return &m->v[i];
		}
	}
	m->v[m->nvertices] = *v;
	m->nvertices++;
	*is_shared = 0;
	return &m->v[m->nvertices - 1];
}

struct vertex_owner {
	struct triangle *t;
	struct vertex_owner *next;
};

static struct vertex_owner* alloc_vertex_owners(int vertices) {
	struct vertex_owner *owners = malloc(sizeof(struct vertex_owner)*vertices);
	memset(owners, 0, sizeof(struct vertex_owner)*vertices);
	return owners;
}

static int shared_owners(struct vertex_owner *owner)
{
	if (!owner)
		return 0;
	if (!owner->t)
		return 0;
	if (!owner->next)
		return 0;
	return 1;
}

static void free_vertex_owners(struct vertex_owner* owners, int vertices) {

	int i;
	for (i=0; i<vertices; ++i) {
		struct vertex_owner* owner = owners[i].next;
		while (owner) {
			struct vertex_owner *current = owner;
			owner = current->next;
			free(current);
		}
	}
	free(owners);
}

static void add_vertex_owner(struct triangle *t, struct vertex_owner *owner)
{
	if (!owner->t) {
		owner->t = t;
		return;
	}
	while (owner->next) {
		owner = owner->next;
	}
	owner->next = malloc(sizeof(struct vertex_owner));
	owner = owner->next;
	owner->t = t;
	owner->next = 0;
}

static int add_facet(FILE *f, struct mesh *m, int *linecount, struct vertex_owner* owners)
{
	int rc, i;
	struct vertex v[3];
	struct vertex *tv;

	struct triangle *t = &m->t[m->ntriangles];
	m->ntriangles++;

	t->flag = 0;

	for (i = 0; i < 3; i++)
		t->v[i] = &v[i];

	rc = read_facet(f, t, linecount);
	if (rc)
		return rc;

	for (i = 0; i < 3; i++) {
		int is_shared = 0;
		tv = add_vertex(m, t->v[i], &is_shared);
		t->v[i] = tv;
		if (is_shared) {
			/* set the shared flag for i'th vertex */
			t->flag |= TRIANGLE_0_SHARED<<(i);
		}
		add_vertex_owner(t, &owners[tv - &m->v[0]]);
	}

	return 0;
}

#define NORMAL_COPLANER_THRESHOLD (0.0001)
void process_coplanar_triangles(struct mesh *m, struct vertex_owner* owners)
{
	/* combinations of edges, the other vertex, and flags required */
	static const int edge_i[6][5] = {
		{0, 1, 2, TRIANGLE_0_SHARED|TRIANGLE_1_SHARED, TRIANGLE_0_1_COPLANAR},
		{0, 2, 1, TRIANGLE_0_SHARED|TRIANGLE_2_SHARED, TRIANGLE_0_2_COPLANAR},
		{1, 2, 0, TRIANGLE_1_SHARED|TRIANGLE_2_SHARED, TRIANGLE_1_2_COPLANAR},
		{1, 0, 2, TRIANGLE_0_SHARED|TRIANGLE_1_SHARED, TRIANGLE_0_1_COPLANAR},
		{2, 0, 1, TRIANGLE_0_SHARED|TRIANGLE_2_SHARED, TRIANGLE_0_2_COPLANAR},
		{2, 1, 0, TRIANGLE_1_SHARED|TRIANGLE_2_SHARED, TRIANGLE_1_2_COPLANAR}
	};

	int i;
	for (i=0; i<m->ntriangles; ++i) {
		int t_flag = m->t[i].flag;

		/* triangle needs to have atleast two shared vertices to have a coplanar edge
		   and not already marked as coplanar */
		if ( !(((t_flag & edge_i[0][3]) && !(t_flag & edge_i[0][4])) ||
		       ((t_flag & edge_i[1][3]) && !(t_flag & edge_i[1][4])) ||
		       ((t_flag & edge_i[2][3]) && !(t_flag & edge_i[2][4]))))
		{
			continue;
		}

		/* this will possibly check the same triangle multiple times but that is better
		   than just brute forcing them all */
		int vertex_index;
		for (vertex_index=0; vertex_index<3; ++vertex_index) {
			struct vertex_owner *start_owner = &owners[m->t[i].v[vertex_index] - &m->v[0]];
			struct vertex_owner *owner;
			for (owner = start_owner; owner != NULL; owner=owner->next) {
				struct triangle *other_t = owner->t;

				/* skip ourself */
				if (!other_t || other_t == &m->t[i]) continue;

				/* check if me and other_t triangles is parallel, do this first
				   as it is faster than all the logic to find the common edges and fails
				   in most common cases */
				float dot_p = mat41_dot_mat41((struct mat41 *) &m->t[i].n.x,(struct mat41 *) &other_t->n.x);
				if (fabs(dot_p-1) > NORMAL_COPLANER_THRESHOLD) {
					/* not parallel enough */
					continue;
				}

				/* check for edge overlap against other_t with all 36 ways */
				int u,v;
				for (u=0; u<6; ++u) {
					if (!((t_flag & edge_i[u][3]) && !(t_flag & edge_i[u][4]))) {
						/* my edge vertecies are not both shared or coplanar already */
						continue;
					}
					for (v=0; v<6; ++v) {
						if (m->t[i].v[edge_i[u][0]] == other_t->v[edge_i[v][0]] &&
						    m->t[i].v[edge_i[u][1]] == other_t->v[edge_i[v][1]])
						{
							/* edge u and v are common so add the coplanar flag associated
							   with this set of vertices */
							m->t[i].flag |= edge_i[u][4];
							other_t->flag |= edge_i[v][4];
							break;
						}
					}
				}
			}
		}
	}
}

void process_vertex_normals(struct mesh *m, float sharp_edge_angle, struct vertex_owner *owners)
{
	float cos_sharp_edge_angle = cos(sharp_edge_angle);

	int tri_index, vert_index;
	for (tri_index = 0; tri_index < m->ntriangles; ++tri_index) {
		struct triangle *this_t = &m->t[tri_index];
		union vec3 this_tnormal = { { this_t->n.x, this_t->n.y, this_t->n.z } };

		for (vert_index = 0; vert_index < 3; vert_index++) {
			union vec3 vnormal = { { 0, 0, 0} };

			struct vertex *this_v = this_t->v[vert_index];

			struct vertex_owner *start_owner = &owners[this_v - &m->v[0]];
			if (start_owner) {
				struct vertex_owner *owner;
				for (owner = start_owner; owner != NULL; owner = owner->next) {
					struct triangle *t = owner->t;

					if (!t)
						continue;

					union vec3 tnormal = { { t->n.x, t->n.y, t->n.z } };

					if (t != this_t) {
						/* don't include t in vertex normal if angle is greater than sharp angle */
						if (vec3_dot(&this_tnormal, &tnormal) < cos_sharp_edge_angle)
							continue;
					}

					union vec3 v0 = { { t->v[0]->x, t->v[0]->y, t->v[0]->z } };
					union vec3 v1 = { { t->v[1]->x, t->v[1]->y, t->v[1]->z } };
					union vec3 v2 = { { t->v[2]->x, t->v[2]->y, t->v[2]->z } };

					union vec3 tv1, tv2;

#if 1
					/* length of cross product = triangle area * 2 */
					union vec3 cross_p;
					vec3_cross(&cross_p, vec3_sub(&tv1, &v1, &v0), vec3_sub(&tv2, &v2, &v0));
					float area = 0.5 * vec3_magnitude(&cross_p);
					vec3_mul_self(&tnormal, area);
					if (area < 1e-20)
						continue;
#endif
					/* figure out the angle between this vertex and the other two on the triangle */
					float dot = 0;
					if (this_v == t->v[0]) {
						vec3_sub(&tv1, &v0, &v1);
						vec3_sub(&tv2, &v0, &v2);
					} else if (this_v == t->v[1]) {
						vec3_sub(&tv1, &v1, &v0);
						vec3_sub(&tv2, &v1, &v2);
					} else if (this_v == t->v[2]) {
						vec3_sub(&tv1, &v2, &v0);
						vec3_sub(&tv2, &v2, &v1);
					}
					if (vec3_magnitude(&tv1) < 1e-20)
						vec3_init(&tv1, 1, 0, 0); /* Avoid NaN when normalizing */
					if (vec3_magnitude(&tv2) < 1e-20)
						vec3_init(&tv2, 1, 0, 0); /* Avoid NaN when normalizing */
					vec3_normalize_self(&tv1);
					vec3_normalize_self(&tv2);
					dot = vec3_dot(&tv1, &tv2);
					if (dot < 0.0)
						dot = 0.0;
					if (dot > 1.0)
						dot = 1.0;
					float angle = acos(dot);
					if (angle < 1e-20)
						continue;

					/* Multiplying by an angle here is pretty weird.  I asked Jeremy about it,
					 * and he provided this link:
					 *
					 * https://gamedev.stackexchange.com/questions/8408/best-way-to-compute-vertex-normals-from-a-triangles-list/8410#8410
					 *
					 *   "If you want to compute vertex normals, you should weight triangle
					 *    contribution by angle. But you can use naive solution and just sum
					 *    all normals from all faces around vertex."
					 *
					 * Basically, the idea is that to figure a vertex normal, you
					 * combine the normals of the surrounding triangles. But each
					 * triangle should not contribute the same amount.  Instead
					 * their contributions should be weighted by the angle the
					 * triangle forms at that vertex, so that triangles with a
					 * large angle contribute a lot, while triangles with a small
					 * angle should contribute less.  So we see that there is no
					 * real "correct" answer to how to compute vertex normals,
					 * as the entire concept of vertex normals is a compromise,
					 * and what is "best" is subjective and also depends on the
					 * model, and what is best in one case will not necessarily
					 * be best in another.
					 */
					vec3_mul_self(&tnormal, angle);
					vec3_add_self(&vnormal, &tnormal);
				}
			}
			/* If we got through the above loop without any contributions from
			 * neighboring triangles, just use the triangle normal for the
			 * vertex normal.
			 */
			if (vnormal.v.x == 0 && vnormal.v.y == 0 && vnormal.v.z == 0) {
				vnormal.v.x = m->t[tri_index].n.x;
				vnormal.v.y = m->t[tri_index].n.y;
				vnormal.v.z = m->t[tri_index].n.z;
			}
			vec3_normalize_self(&vnormal);

			m->t[tri_index].vnormal[vert_index].x = vnormal.v.x;
			m->t[tri_index].vnormal[vert_index].y = vnormal.v.y;
			m->t[tri_index].vnormal[vert_index].z = vnormal.v.z;
		}
	}
}

void free_mesh(struct mesh * m)
{
	if (!m)
		return;
	if (m->t)
		free(m->t);
	if (m->v)
		free(m->v);
	free(m);
}

/* check if any triangle in the mesh contains fewer than 3 vertices */
static void check_triangle_vertices(struct mesh *m)
{
	int i;

	for (i = 0; i < m->ntriangles; i++) {
		struct vertex *v0 = m->t[i].v[0];
		struct vertex *v1 = m->t[i].v[1];
		struct vertex *v2 = m->t[i].v[2];

		if (v0 == v1)
			printf("triangle %d, vertices 0 and 1 are welded together %p.\n", i, (void *)v0);
		if (v1 == v2)
			printf("triangle %d, vertices 1 and 2 are welded together %p.\n", i, (void *)v1);
		if (v0 == v2)
			printf("triangle %d, vertices 0 and 2 are welded together %p.\n", i, (void *)v0);
	}
}

static char *default_asset_replacement_function(char *s)
{
	return s;
}

static asset_replacement_function maybe_replace_asset = default_asset_replacement_function;

void stl_parser_set_asset_replacement_function(asset_replacement_function fn)
{
	maybe_replace_asset = fn;
}

struct mesh *read_stl_file(char *filename)
{
	FILE *f;
	char *s;
	char buffer[500];
	int facetcount, rc;
	int linecount = 0;
	struct mesh *my_mesh;
	int i;
	char *file;

	file = maybe_replace_asset(filename);
	f = fopen(file, "r");
	if (!f)
		return NULL;

	s = fgets(buffer, 500, f);
	linecount++;
	if (!s) {
		stl_error("unexpected EOF", linecount);
		fclose(f);
		return NULL;
	}
	facetcount = count_facets(file);
	if (facetcount <= 0) {
		fclose(f);
		return NULL;
	}
	my_mesh = malloc(sizeof(*my_mesh));
	if (!my_mesh) {
		fclose(f);
		return my_mesh;
	}
	memset(my_mesh, 0, sizeof(*my_mesh));
	my_mesh->geometry_mode = MESH_GEOMETRY_TRIANGLES;
	my_mesh->nvertices = 0;
	my_mesh->ntriangles = 0;
	my_mesh->nlines = 0;
	my_mesh->t = malloc(sizeof(*my_mesh->t) * facetcount);
	my_mesh->v = malloc(sizeof(*my_mesh->v) * facetcount * 3); /* worst case */
	my_mesh->l = NULL;
	my_mesh->tex = NULL;
	my_mesh->graph_ptr = 0;

	struct vertex_owner* owners = alloc_vertex_owners(facetcount * 3);

	for (i = 0; i < facetcount; i++) {
		rc = add_facet(f, my_mesh, &linecount, owners);
		if (rc < 0)
			goto error;
	}
	fclose(f);
	my_mesh->radius = mesh_compute_radius(my_mesh);
	process_coplanar_triangles(my_mesh, owners);
	check_triangle_vertices(my_mesh);
	process_vertex_normals(my_mesh, DEFAULT_SHARP_CORNER_ANGLE, owners);
	mesh_set_reasonable_tangents_and_bitangents(my_mesh);

	free_vertex_owners(owners, facetcount * 3);

	mesh_graph_dev_init(my_mesh);
	return my_mesh;

error:
	fclose(f);
	free_mesh(my_mesh);
	return NULL;
}

/* Before realloc, we need to convert internal ptrs to offsets so we can fix them up
 * afterwards . We used to do this by saving the old value of the pointer we
 * realloc'ed, then after the realloc, adjust the internal pointers by subtracting the
 * old value and adding the new value, which worked (on x86, x86_64).  However, I think
 * that using the old value in pointer arithmetic this way is technically undefined
 * behavior (and clang scan-build considered it "use after free").  So as not to
 * accidentally give the compiler a license to go crazy, we'll do it this way which
 * avoids any (seemingly benign) use of a free'd pointer even though it's essentially
 * twice as much work. Note, m->t[n].v[x] and m->t[n].offset[x] are anonymously union'ed */
static void convert_triangle_vertex_ptrs_to_offsets(struct mesh *m)
{
	int i;

	for (i = 0; i < m->ntriangles; i++) {
		m->t[i].offset[0] = m->t[i].v[0] - m->v;
		m->t[i].offset[1] = m->t[i].v[1] - m->v;
		m->t[i].offset[2] = m->t[i].v[2] - m->v;
	}
}

/* After realloc, we need to convert offsets back to internal ptrs */
static void convert_triangle_vertex_offsets_to_ptrs(struct mesh *m)
{
	int i;

	for (i = 0; i < m->ntriangles; i++) {
		m->t[i].v[0] = m->v + m->t[i].offset[0];
		m->t[i].v[1] = m->v + m->t[i].offset[1];
		m->t[i].v[2] = m->v + m->t[i].offset[2];
	}
}

static int allocate_more_vertices(struct mesh *m, int *verts_alloced, int additional_verts)
{
	/* Get some more memory for vertices if needed */
	struct vertex *newmem;

	convert_triangle_vertex_ptrs_to_offsets(m); /* Convert internal ptrs to offsets before realloc */
	*verts_alloced = *verts_alloced + additional_verts;
	newmem = realloc(m->v, *verts_alloced * sizeof(*m->v));
	if (!newmem) {
		convert_triangle_vertex_offsets_to_ptrs(m); /* Convert offsets back to ptrs */
		return -1;
	}
	m->v = newmem;
	convert_triangle_vertex_offsets_to_ptrs(m); /* Convert offsets back to ptrs */
	return 0;
}

static int obj_add_vertex(struct mesh *m, char *line, int *verts_alloced)
{
	int rc;
	float x, y, z, w;

	rc = sscanf(line, "v %f %f %f %f", &x, &y, &z, &w);
	switch (rc) {
	case 3: /* w is optional weight for rational curves/surfaces (unsupported by SNIS),
		   if missing, implicitly 1.0f */
		w = 1.0f;
		break;
	case 4:
		break;
	default:
		return -1;
	}

	/* Get some more memory for vertices if needed */
	if (m->nvertices + 1 > *verts_alloced)
		if (allocate_more_vertices(m, verts_alloced, 100))
			return -1;

	m->v[m->nvertices].x = x;
	m->v[m->nvertices].y = y;
	m->v[m->nvertices].z = z;
	m->v[m->nvertices].w = w;
	m->nvertices++;
	return 0;
}

static int obj_add_texture_vertex(struct vertex **vt, char *line,
				int *nverts_alloced, int *nverts_used)
{
	int rc;
	float u, v, w;

	w = 0.0f;
	rc = sscanf(line, "vt %f %f %f", &u, &v, &w);
	switch (rc) {
	case 2: /* w is optional depth of texture, (unsupported by SNIS) if missing, implicitly 0.0f */
		w = 0.0f;
		break;
	case 3:
		break;
	default:
		return -1;
	}

	if (*nverts_used >= *nverts_alloced) {
		struct vertex *newmem;

		*nverts_alloced += 100;
		newmem = realloc(*vt, *nverts_alloced * sizeof(**vt));
		if (!newmem)
			return -1;
		*vt = newmem;
	}
	(*vt)[*nverts_used].x = u;
	(*vt)[*nverts_used].y = v;
	(*vt)[*nverts_used].z = 0.0f;
	(*vt)[*nverts_used].w = w;
	(*nverts_used)++;

	return 0;
}

static int obj_add_vertex_normal(struct vertex **vt, char *line,
					int *nverts_alloced, int *nverts_used)
{
	int rc;
	float x, y, z;
	union vec3 v;

	rc = sscanf(line, "vn %f %f %f", &x, &y, &z);
	if (rc != 3)
		return -1;

	if (*nverts_used >= *nverts_alloced) {
		struct vertex *newmem;

		*nverts_alloced += 100;
		newmem = realloc(*vt, *nverts_alloced * sizeof(**vt));
		if (!newmem)
			return -1;
		*vt = newmem;
	}

	v.v.x = x;
	v.v.y = y;
	v.v.z = z;
	vec3_normalize_self(&v);

	(*vt)[*nverts_used].x = v.v.x;
	(*vt)[*nverts_used].y = v.v.y;
	(*vt)[*nverts_used].z = v.v.z;
	(*vt)[*nverts_used].w = 1.0f;
	(*nverts_used)++;

	return 0;
}

static int fixup_vertex_indices(int v[], int n)
{
	int i;

	for (i = 0; i < 3; i++) {
		if (v[i] > 0) { /* convert positive numbers to zero-based array indices */
			v[i]--;
		} else {
			if (v[i] < 0) {/* convert negative numbers to array indices */
				v[i] = n - v[i];
			} else {
				fprintf(stderr, "Face contains illegal vertex 0.\n");
				return -1;
			}
		}
	}
	return 0;
}

static int bounds_check_vertex_indices(int v[], int count, int nvertices)
{
	for (int i = 0; i < count; i++)
		if (v[i] < 0 || v[i] >= nvertices)
			return -1;
	return 0;
}

static int obj_add_face(struct mesh *m, char *line, int *tris_alloced,
			struct vertex *vt, int nvt, struct vertex *vn, int nvn)
{
	int rc, v[3], tv[3], nv[3], tmp;
	int vvalid, tvalid, nvalid;

	vvalid = 0;
	tvalid = 0;
	nvalid = 0;
	(void) vvalid; /* To suppress scan-build complaining about dead stores. */

	rc = sscanf(line, "f %d/%d/%d %d/%d/%d %d/%d/%d",
			&v[0], &tv[0], &nv[0],
			&v[1], &tv[1], &nv[1],
			&v[2], &tv[2], &nv[2]);
	if (rc == 9) {
		vvalid = 1;
		tvalid = 1;
		nvalid = 1;
		goto process_it;
	}
	rc = sscanf(line, "f %d//%d %d//%d %d//%d",
			&v[0], &nv[0], &v[1], &nv[1], &v[2], &nv[2]);
	if (rc == 6) {
		vvalid = 1;
		nvalid = 1;
		goto process_it;
	}
	rc = sscanf(line, "f %d/%d %d/%d %d/%d",
			&v[0], &tv[0], &v[1], &tv[1], &v[2], &tv[2]);
	if (rc == 6) {
		vvalid = 1;
		tvalid = 1;
		goto process_it;
	}
	rc = sscanf(line, "f %d %d %d %d", &v[0], &v[1], &v[2], &tmp);
	if (rc == 4) {
		fprintf(stderr, "Faces with more than 3 vertices not supported.\n");
		return -1;
	}
	rc = sscanf(line, "f %d %d %d", &v[0], &v[1], &v[2]);
	if (rc == 3) {
		vvalid = 1;
		goto process_it;
	}
	return -1;

process_it:

	if (vvalid) {
		if (fixup_vertex_indices(v, m->nvertices))
			return -1;
		if (bounds_check_vertex_indices(v, 3, m->nvertices))
			return -1;
	}
	if (tvalid) {
		if (fixup_vertex_indices(tv, nvt))
			return -1;
		if (bounds_check_vertex_indices(tv, 3, nvt))
			return -1;
	}
	if (nvalid) {
		if (fixup_vertex_indices(nv, nvn))
			return -1;
		if (bounds_check_vertex_indices(nv, 3, nvn))
			return -1;
	}


	if (m->ntriangles + 1 > *tris_alloced) {
		struct triangle *newmem;
		struct texture_coord *newtex;

		*tris_alloced += 100;
		newmem = realloc(m->t, *tris_alloced * sizeof(*m->t));
		if (!newmem)
			return -1;
		m->t = newmem;
		if (tvalid) {
			newtex = realloc(m->tex, *tris_alloced * 3 * sizeof(*m->tex));
			if (!newtex)
				return -1;
			m->tex = newtex;
		}
	}
	m->t[m->ntriangles].v[0] = &m->v[v[0]];
	m->t[m->ntriangles].v[1] = &m->v[v[1]];
	m->t[m->ntriangles].v[2] = &m->v[v[2]];
	m->t[m->ntriangles].flag = 0;
	calculate_triangle_normal(&m->t[m->ntriangles]);
	if (nvalid) {
		m->t[m->ntriangles].vnormal[0].x = vn[nv[0]].x;
		m->t[m->ntriangles].vnormal[0].y = vn[nv[0]].y;
		m->t[m->ntriangles].vnormal[0].z = vn[nv[0]].z;
		m->t[m->ntriangles].vnormal[1].x = vn[nv[1]].x;
		m->t[m->ntriangles].vnormal[1].y = vn[nv[1]].y;
		m->t[m->ntriangles].vnormal[1].z = vn[nv[1]].z;
		m->t[m->ntriangles].vnormal[2].x = vn[nv[2]].x;
		m->t[m->ntriangles].vnormal[2].y = vn[nv[2]].y;
		m->t[m->ntriangles].vnormal[2].z = vn[nv[2]].z;
	} else {
		/* just use the triangle face normal for all the vertex normals for now */
		m->t[m->ntriangles].vnormal[0].x = m->t[m->ntriangles].n.x;
		m->t[m->ntriangles].vnormal[0].y = m->t[m->ntriangles].n.y;
		m->t[m->ntriangles].vnormal[0].z = m->t[m->ntriangles].n.z;
		m->t[m->ntriangles].vnormal[1].x = m->t[m->ntriangles].n.x;
		m->t[m->ntriangles].vnormal[1].y = m->t[m->ntriangles].n.y;
		m->t[m->ntriangles].vnormal[1].z = m->t[m->ntriangles].n.z;
		m->t[m->ntriangles].vnormal[2].x = m->t[m->ntriangles].n.x;
		m->t[m->ntriangles].vnormal[2].y = m->t[m->ntriangles].n.y;
		m->t[m->ntriangles].vnormal[2].z = m->t[m->ntriangles].n.z;
	}
	if (tvalid) {
		if (!m->tex) {
			printf("Unexpectedly found texture data mid stream, no thanks.\n");
			return -1;
		}
		m->tex[m->ntriangles * 3 + 0].u = vt[tv[0]].x;
		m->tex[m->ntriangles * 3 + 0].v = vt[tv[0]].y;
		m->tex[m->ntriangles * 3 + 1].u = vt[tv[1]].x;
		m->tex[m->ntriangles * 3 + 1].v = vt[tv[1]].y;
		m->tex[m->ntriangles * 3 + 2].u = vt[tv[2]].x;
		m->tex[m->ntriangles * 3 + 2].v = vt[tv[2]].y;
	}
	m->ntriangles++;
	return 0;
}

static void compact_mesh_allocations(struct mesh *m)
{
	struct vertex *newptr;
	struct triangle *newtri;
	struct texture_coord *newtex;

	if (m->nvertices <= 0) {
		stacktrace("compact_mesh_allocations() -- mesh with nvertices <= 0");
		abort();
	}
	convert_triangle_vertex_ptrs_to_offsets(m); /* Convert internal ptrs to offsets before realloc */
	newptr = realloc(m->v, m->nvertices * sizeof(*m->v));
	if (!newptr) {
		convert_triangle_vertex_offsets_to_ptrs(m); /* Convert offsets back to ptrs */
		return;
	}
	m->v = newptr;
	convert_triangle_vertex_offsets_to_ptrs(m); /* Convert offsets back to ptrs */
	if (m->ntriangles <= 0) {
		stacktrace("compact_mesh_allocations() -- mesh with ntriangles <= 0");
		abort();
	}
	newtri = realloc(m->t, m->ntriangles * sizeof(*m->t));
	if (newtri)
		m->t = newtri;
	newtex = realloc(m->tex, 3 * m->ntriangles * sizeof(*m->tex));
	if (newtex)
		m->tex = newtex;
}

static void parse_mtllib(char *parentfilename, char *mtllib_line, char *tfile,
				int tfilelen, char *efile, int efilelen, char *nfile, int nfilelen)
{
	char fname[PATH_MAX];
	char fname2[PATH_MAX];
	char *fn3;
	char texturefile[PATH_MAX];
	char *dname, *s;
	char ln[500];
	FILE *f;
	int rc;

	if (tfilelen <= 0)
		return;
	strcpy(tfile, "");

	dname = dir_name(parentfilename);
	rc = sscanf(mtllib_line, "mtllib %s", fname);
	if (rc != 1) {
		free(dname);
		fprintf(stderr, "Failed to parse '%s:%s'\n", parentfilename, mtllib_line);
		return;
	}
	snprintf(fname2, sizeof(fname2), "%s/%s", dname, fname);
	fn3 = maybe_replace_asset(fname2);
	f = fopen(fn3, "r");
	if (!f) {
		free(dname);
		fprintf(stderr, "Failed to open '%s': %s\n", fn3, strerror(errno));
		return;
	}

	tfile[0] = '\0';
	efile[0] = '\0';
	nfile[0] = '\0';

	while (!feof(f)) {
		s = fgets(ln, sizeof(ln), f);
		if (!s)
			goto out;
		remove_trailing_whitespace(ln);
		clean_spaces(ln);

		/* FIXME: this is the stupidest thing that might work.
		 * Ignore everything but map_Kd lines and take the first
		 * one we find to be the only one.
		 */
		if (!tfile[0] && strncmp(ln, "map_Kd ", 7) == 0) {
			rc = sscanf(ln, "map_Kd %s", texturefile);
			if (rc != 1)
				continue;
			snprintf(tfile, tfilelen, "%s%s", dname, texturefile);
		}
		if (!efile[0] && strncmp(ln, "map_Ke ", 7) == 0) {
			rc = sscanf(ln, "map_Ke %s", texturefile);
			if (rc != 1)
				continue;
			snprintf(efile, efilelen, "%s%s", dname, texturefile);
		}
		/* This is a non-standard extension to obj... wavefront obj doesn't support
		 * normal maps, only bump maps, which the latter are fairly useless.
		 */
		if (!nfile[0] && strncmp(ln, "norm ", 5) == 0) {
			rc = sscanf(ln, "norm %s", texturefile);
			if (rc != 1)
				continue;
			snprintf(nfile, nfilelen, "%s%s", dname, texturefile);
		}
	}
out:
	free(dname);
	fclose(f);
	return;
}

static struct vertex_owner *mesh_compute_vertex_owners(struct mesh *m)
{
	struct vertex_owner *owners = alloc_vertex_owners(m->ntriangles * 3);

	if (!owners)
		return owners;

	for (int i = 0; i < m->ntriangles; i++) {
		struct triangle *t = &m->t[i];

		for (int j = 0; j < 3; j++)
			add_vertex_owner(t, &owners[t->v[j] - &m->v[0]]);
	}
	return owners;
}

static void mesh_compute_shared_vertex_flags(struct mesh *m, struct vertex_owner *owners)
{
	for (int i = 0; i < m->ntriangles; i++) {
		struct triangle *t = &m->t[i];

		for (int j = 0; j < 3; j++)
			if (shared_owners(&owners[t->v[j] - &m->v[0]]))
				t->flag |= TRIANGLE_0_SHARED << j;
	}
}

/* If we have a file-blah-blah.obj, check for file-blah-blah-normalmap.png and if
 * we find it, assume it is a normal map we can use.
 */
static void search_for_normalmap_file(char *filename, char *normalmapfile, int nmlen)
{
	int rc, n;
	struct stat statbuf;
	char candidate[PATH_MAX];

	n = strlen(filename);
	if (n < 5)
		return;

	if (strcmp(&filename[n - 4], ".obj") == 0) {
		snprintf(candidate, sizeof(candidate) - 1, "%s", filename);
		candidate[n - 4] = '\0';
		strncat(candidate, "-normalmap.png", sizeof(candidate) - strlen(candidate) - 1);
		candidate[sizeof(candidate) - 1] = '\0';
		rc = stat(candidate, &statbuf);
		if (rc == 0)
			snprintf(normalmapfile, nmlen, "%s", candidate);
	}
}

struct mesh *read_obj_file(char *file_name)
{
	FILE *f;
	char *s;
	char buffer[500];
	char line[1000];
	char tfile[PATH_MAX];
	char efile[PATH_MAX];
	char nfile[PATH_MAX];
	int continuation;
	int lineno = 0;
	int verts_alloced = 0;
	int tris_alloced = 0;
	int texture_verts_alloced = 0;
	int texture_verts_used = 0;
	int normal_verts_alloced = 0;
	int normal_verts_used = 0;
	struct mesh *m;
	struct vertex *vt = NULL;
	struct vertex *vn = NULL;
	struct triangle *ft = NULL;
	struct vertex_owner *owners;
	char *filename = maybe_replace_asset(file_name);

	f = fopen(filename, "r");
	if (!f)
		return NULL;

	m = malloc(sizeof(*m));
	m->geometry_mode = MESH_GEOMETRY_TRIANGLES;
	m->nvertices = 0;
	m->ntriangles = 0;
	m->nlines = 0;
	m->t = NULL;
	m->v = NULL;
	m->l = NULL;
	m->tex = NULL;
	m->graph_ptr = 0;

	continuation = 0;
	while (!feof(f)) {
		char *d = line;
		/* read a line... */
		do {
			if (continuation)
				d += strlen(d) - 1;
			s = fgets(buffer, sizeof(buffer), f);
			if (!s)
				break;
			lineno++;
			if (s[0] == '#') /* skip comments */
				continue;
			remove_trailing_whitespace(s);
			if (strlen(s) > 0)
				continuation = (s[strlen(s) - 1] == '\\');
			else
				continuation = 0;
			clean_spaces(s);
			if (strlen(line) + strlen(s) + 1 > sizeof(line)) {
				printf("%s: line too long: %d\n", filename, lineno);
				s = NULL;
				break;
			}
			strcpy(d, s);
		} while (continuation || s[0] == '#' || strcmp(line, "") == 0);

		if (!s)
			break;

		/* printf("%d: line = '%s'\n", lineno, line); */

		if (strncmp(line, "v ", 2) == 0) { /* vertex */
			if (obj_add_vertex(m, line, &verts_alloced))
				goto flame_out;
		} else if (strncmp(line, "vt ", 3) == 0) { /* texture vertex (u/v mapping) */
			if (obj_add_texture_vertex(&vt, line,
					&texture_verts_alloced, &texture_verts_used))
				goto flame_out;
		} else if (strncmp(line, "vn ", 3) == 0) { /* vertex normal */
			if (obj_add_vertex_normal(&vn, line,
					&normal_verts_alloced, &normal_verts_used))
				goto flame_out;
		} else if (strncmp(line, "f ", 2) == 0) { /* face */
			if (obj_add_face(m, line, &tris_alloced,
					vt, texture_verts_used, vn, normal_verts_used))
				goto flame_out;
		} else if (strncmp(line, "mtllib ", 2) == 0) {
			printf("parsing material library: %s\n", line);
			parse_mtllib(filename, line, tfile, sizeof(tfile), efile, sizeof(efile), nfile, sizeof(nfile));
			if (strcmp(tfile, "") != 0) {
				struct material *mtl;

				mtl = malloc(sizeof(*mtl));
				material_init_texture_mapped(mtl);
				mtl->texture_mapped.texture_id = graph_dev_load_texture(maybe_replace_asset(tfile), 0);
				if (strcmp(efile, "") != 0)
					mtl->texture_mapped.emit_texture_id =
						graph_dev_load_texture(maybe_replace_asset(efile), 0);
				if (strcmp(nfile, "") == 0)
					search_for_normalmap_file(filename, nfile, sizeof(nfile));
				if (strcmp(nfile, "") != 0) {
					mtl->texture_mapped.normalmap_id =
						graph_dev_load_texture(maybe_replace_asset(nfile), 1);
				}
				m->material = mtl;
			}
		} else if (strncmp(line, "usemtl ", 2) == 0) {
			printf("ignoring usemtl: %s\n", line);
		} else if (strncmp(line, "g ", 2) == 0) { /* group */
			printf("ignoring group %s\n", line);
		} else {
			printf("ignoring unknown data '%d':%s\n", lineno, line);
		}
	}

	if (vt)
		free(vt);
	if (vn)
		free(vn);
	if (ft)
		free(ft);

	if (m->nvertices <= 0) {
		printf("Mesh contains no vertices\n");
		goto flame_out;
	}
	if (m->ntriangles <= 0) {
		printf("Mesh contains no triangles\n");
		goto flame_out;
	}
	if (m->nvertices > m->ntriangles * 3) {
		printf("Mesh contains too many vertices for the number of triangles\n");
		goto flame_out;
	}
	compact_mesh_allocations(m);
	m->radius = mesh_compute_radius(m);
	check_triangle_vertices(m);
	owners = mesh_compute_vertex_owners(m);
	if (owners) {
		mesh_compute_shared_vertex_flags(m, owners);
		process_coplanar_triangles(m, owners);
		if (normal_verts_used == 0) {
			printf("no normals in obj, using default smoothing\n");
			process_vertex_normals(m, DEFAULT_SHARP_CORNER_ANGLE, owners);
		}
		free_vertex_owners(owners, m->ntriangles * 3);
	}
	/* TODO: if obj file actually contains tangents/bitangents, we should use those. */
	mesh_set_reasonable_tangents_and_bitangents(m);
	mesh_graph_dev_init(m);
	fclose(f);
	return m;

flame_out:
	fprintf(stderr, "Error parsing %s:%d: %s\n",
		filename, lineno, line);
	free_mesh(m);
	fclose(f);
	return NULL;
}

struct mesh *read_oolite_dat_file(char *file_name)
{
	FILE *f;
	char *s;
	char buffer[500];
	char line[1000];
	int continuation;
	int lineno = 0;
	int verts_alloced = 0;
	int rc, ival;
	struct mesh *m;
	struct vertex_owner *owners;
	int mode = -1;
	int nfaces = 0;
	int ntex = 0;
	char *filename = maybe_replace_asset(file_name);

	f = fopen(filename, "r");
	if (!f)
		return NULL;

	m = malloc(sizeof(*m));
	memset(m, 0, sizeof(*m));
	m->geometry_mode = MESH_GEOMETRY_TRIANGLES;
	m->nvertices = 0;
	m->ntriangles = 0;
	m->nlines = 0;
	m->t = NULL;
	m->v = NULL;
	m->l = NULL;
	m->tex = NULL;
	m->graph_ptr = 0;

	continuation = 0;
	while (!feof(f)) {
		char *d = line;
		/* read a line... */
		do {
			if (continuation)
				d += strlen(d) - 1;
			s = fgets(buffer, sizeof(buffer), f);
			if (!s)
				break;
			lineno++;
			if (s[0] == '/' && s[1] == '/') /* skip comments */
				continue;
			remove_trailing_whitespace(s);
			continuation = (s[strlen(s) - 1] == '\\');
			clean_spaces(s);
			strcpy(d, s);
		} while (continuation || s[0] == '#' || strcmp(line, "") == 0);

		if (!s)
			break;

		if (strncmp(line, "NVERTS ", 6) == 0) { /* number of vertices */
			if (verts_alloced != 0)  {
				fprintf(stderr, "Duplicate NVERTS line at %s:%d\n", filename, lineno);
				goto error;
			}
			rc = sscanf(line, "NVERTS %d", &ival);
			if (rc != 1) {
				fprintf(stderr, "Bad NVERTS line at %s:%d:%s\n", filename, lineno, line);
				goto error;
			}
			if (ival < verts_alloced) {
				fprintf(stderr, "Bad NVERTS line %s:%d:%s: (%d < %d)\n",
					filename, lineno, line, ival, verts_alloced);
				goto error;
			}
			if (allocate_more_vertices(m, &verts_alloced, ival - verts_alloced)) {
				fprintf(stderr, "%s:%d:%s: Failed to allocated %d vertices\n",
						filename, lineno, line, ival - verts_alloced);
			}
			continue;
		}

		if (strncmp(line, "NFACES ", 7) == 0) { /* number of faces */
			struct triangle *newmem = NULL;
			struct texture_coord *newtex;
			if (nfaces != 0)  {
				fprintf(stderr, "Duplicate NFACES line at %s:%d\n", filename, lineno);
				goto error;
			}
			rc = sscanf(line, "NFACES %d", &ival);
			if (rc != 1) {
				fprintf(stderr, "Bad NFACES line at %s:%d:%s\n", filename, lineno, line);
				goto error;
			}
			nfaces = ival;
			newmem = realloc(m->t, nfaces * sizeof(*m->t));
			if (!newmem) {
				fprintf(stderr, "realloc failed at %s:%d\n", __FILE__, __LINE__);
				goto error;
			}
			memset(newmem, 0, nfaces * sizeof(*m->t));
			m->t = newmem;
			newtex = realloc(m->tex, nfaces * 3 * sizeof(*m->tex));
			if (!newtex) {
				fprintf(stderr, "realloc failed at %s:%d\n", __FILE__, __LINE__);
				goto error;
			}
			m->tex = newtex;
			continue;
		}

		if (strncmp(line, "VERTEX", 6) == 0) { /* enter face mode */
			printf("ENTER VERTEX MODE\n");
			mode = 1; /* vertex mode */
			continue;
		}
		if (strncmp(line, "FACES", 5) == 0) { /* enter face mode */
			printf("ENTER FACES MODE\n");
			mode = 2; /* face mode */
			continue;
		}
		if (strncmp(line, "TEXTURES", 8) == 0) { /* enter face mode */
			printf("ENTER TEXTURES MODE\n");
			mode = 3; /* texture mode */
			continue;
		}
		if (strncmp(line, "NORMALS", 7) == 0) { /* enter normals mode */
			printf("ENTER NORMALS MODE\n");
			mode = 4;
			continue;
		}
		if (strncmp(line, "NAMES", 5) == 0) { /* enter names mode */
			printf("ENTER NAMES MODE\n");
			mode = 5;
			continue;
		}
		if (strncmp(line, "END", 3) == 0) { /* enter names mode */
			printf("ENTER END MODE\n");
			mode = 6;
			continue;
		}

		if (mode == 1) { /* vertex mode */
			float x, y, z;
			rc = sscanf(line, "%f%*[, ]%f%*[, ]%f", &x, &y, &z);
			if (rc != 3) {
				fprintf(stderr, "%s:%d:%s: bad vertex line\n", filename, lineno, line);
				goto error;
			}
			if (m->nvertices >= verts_alloced) {
				fprintf(stderr, "%s:%d:%s Too many vertices (should be %d)\n",
					filename, lineno, line, verts_alloced);
				goto error;
			}
			m->v[m->nvertices].x = x;
			m->v[m->nvertices].y = y;
			m->v[m->nvertices].z = z;
			m->nvertices++;
		} else if (mode == 2) { /* face mode */
			float nx, ny, nz;
			int v1, v2, v3;
			rc = sscanf(line,
				"%*d%*[, ]%*d%*[, ]%*d%*[, ]%f%*[, ]%f%*[, ]%f%*[, ]%*d%*[, ]%d%*[, ]%d%*[, ]%d",
				&nx, &ny, &nz, &v1, &v2, &v3);
			if (rc != 6) {
				fprintf(stderr, "%s:%d:%s: bad face\n", filename, lineno, line);
				goto error;
			}
			if (v1 < 0 || v1 >= m->nvertices ||
				v2 < 0 || v2 >= m->nvertices ||
				v3 < 0 || v3 >= m->nvertices) {
				fprintf(stderr, "%s:%d:%s: vertex out of range (min = %d, max = %d\n",
					filename, lineno, line, 0, m->nvertices - 1);
				goto error;
			}
			if (m->ntriangles >= nfaces) {
				fprintf(stderr, "%s:%d:%s: too many faces\n", filename, lineno, line);
				goto error;
			}
			/* Wtf? They put the vertices in backwards?
			 * Something's a bit weird here. I noticed most of their models have
			 * the vertices backwards, but some don't.
			 */
			m->t[m->ntriangles].v[2] = &m->v[v1];
			m->t[m->ntriangles].v[1] = &m->v[v2];
			m->t[m->ntriangles].v[0] = &m->v[v3];
#if 0
			/* I don't really know what's going on with their normals. And they don't
			 * seem to have per-vertex normals either.
			 */
			m->t[m->ntriangles].n.x = nx;
			m->t[m->ntriangles].n.y = ny;
			m->t[m->ntriangles].n.z = nz;
#endif
			m->t[m->ntriangles].flag = 0;
			calculate_triangle_normal(&m->t[m->ntriangles]); /* Just calc the triangle normal ourself. */
			m->ntriangles++;
		} else if (mode == 3) { /* texture mode */
			char filename[PATH_MAX];
			float u[3], v[3];
			rc = sscanf(line, "%s%*[ ,]%*f%*[ ,]%*f%*[ ,]%f%*[ ,]%f%*[ ,]%f%*[ ,]%f%*[ ,]%f%*[ ,]%f",
				filename, &u[0], &v[0], &u[1], &v[1], &u[2], &v[2]);
			if (rc != 7) {
				fprintf(stderr, "%s:%d:%s: bad texture line\n", filename, lineno, line);
				goto error;
			}
			if (!m->tex) {
				fprintf(stderr, "%s:%d:%s: texture mode, but no texture allocated (no NFACES line?)\n",
					filename, lineno, line);
				goto error;
			}
			/* Backwards?  Ok. */
			m->tex[ntex * 3 + 2].u = u[0];
			m->tex[ntex * 3 + 2].v = v[0];
			m->tex[ntex * 3 + 1].u = u[1];
			m->tex[ntex * 3 + 1].v = v[1];
			m->tex[ntex * 3 + 0].u = u[2];
			m->tex[ntex * 3 + 0].v = v[2];
			ntex++;
		}
	}

	compact_mesh_allocations(m);
	m->radius = mesh_compute_radius(m);
	check_triangle_vertices(m);
	owners = mesh_compute_vertex_owners(m);
	if (owners) {
		mesh_compute_shared_vertex_flags(m, owners);
		process_coplanar_triangles(m, owners);
		printf("No normals in oolite, using default smoothing\n");
		process_vertex_normals(m, DEFAULT_SHARP_CORNER_ANGLE, owners);
		free_vertex_owners(owners, m->ntriangles * 3);
	}
	mesh_set_mikktspace_tangents_and_bitangents(m);
	mesh_graph_dev_init(m);
	fclose(f);

	return m;
error:
	free_mesh(m);
	m = NULL;
	fclose(f);
	return m;
}

struct mesh *read_mesh(char *filename)
{
	int n;

	n = strlen(filename);
	if (n < 5)
		return NULL;
	if (strcmp(&filename[n - 4], ".stl") == 0)
		return read_stl_file(filename);
	if (strcmp(&filename[n - 4], ".obj") == 0)
		return read_obj_file(filename);
	if (strcmp(&filename[n - 4], ".dat") == 0)
		return read_oolite_dat_file(filename);
	return NULL;
}

#ifdef TEST_STL_PARSER 

void print_mesh(struct mesh *m)
{
	int i;

	printf("Triangles ntriangles=%d\n", m->ntriangles);

	for (i = 0; i < m->ntriangles; ++i) {
		printf("%d: flag=%d\n", i, m->t[i].flag );
		printf("  %f, %f, %f\n", m->t[i].v[0]->x, m->t[i].v[0]->y, m->t[i].v[0]->z );
		printf("  %f, %f, %f\n", m->t[i].v[1]->x, m->t[i].v[1]->y, m->t[i].v[1]->z );
		printf("  %f, %f, %f\n", m->t[i].v[2]->x, m->t[i].v[2]->y, m->t[i].v[2]->z );
		if (m->t[i].flag & TRIANGLE_0_SHARED)
			printf("  TRIANGLE_0_SHARED\n");
		if (m->t[i].flag & TRIANGLE_1_SHARED)
			printf("  TRIANGLE_1_SHARED\n");
		if (m->t[i].flag & TRIANGLE_2_SHARED)
			printf("  TRIANGLE_2_SHARED\n");
		if (m->t[i].flag & TRIANGLE_0_1_COPLANAR)
			printf("  TRIANGLE_0_1_COPLANAR\n");
		if (m->t[i].flag & TRIANGLE_0_2_COPLANAR)
			printf("  TRIANGLE_0_2_COPLANAR\n");
		if (m->t[i].flag & TRIANGLE_1_2_COPLANAR)
			printf("  TRIANGLE_1_2_COPLANAR\n");
	}

	printf("Vertices nvertices=%d\n", m->nvertices);

	for (i = 0; i < m->nvertices; i++)
		printf("%d: %f, %f, %f\n", i, m->v[i].x, m->v[i].y, m->v[i].z);
}

int main(int argc, char *argv[])
{
	struct mesh *s;

	if (argc < 2) {
		fprintf(stderr, "usage: stl_parser mesh-file\n");
		return 1;
	}

	s = read_mesh(argv[1]);
	if (!s) {
		if (errno)
			fprintf(stderr, "%s: %s\n", argv[1], strerror(errno));
		else
			fprintf(stderr, "Failed to read mesh file '%s'\n", argv[1]);
		return 1;
	} else {
		fprintf(stderr, "Success!, nvertices = %d, ntriangle = %d\n",
				s->nvertices, s->ntriangles);
		print_mesh(s);
		free_mesh(s);
		s = NULL;
	}
	return 0;
}

void mesh_graph_dev_init(__attribute__((unused)) struct mesh *m)
{

}

void mesh_graph_dev_cleanup(__attribute__((unused)) struct mesh *m)
{

}

unsigned int graph_dev_load_texture(__attribute__((unused)) const char *filename,
					__attribute__((unused)) int linear_colorspace)
{
	return 1;
}

void material_init_texture_mapped(__attribute__((unused)) struct material *m)
{
}

#endif
