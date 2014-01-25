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
#if !defined(__APPLE__) && !defined(__FreeBSD__)
/* Apple gets what it needs for malloc from stdlib.h */
#include <malloc.h>
#endif
#include <string.h>
#include <errno.h>
#include <math.h>

#include "vertex.h"
#include "triangle.h"
#include "mesh.h"
#include "matrix.h"
#include "mtwist.h"
#include "quat.h"

#define DEFINE_STL_FILE_GLOBALS
#include "stl_parser.h"

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
	rc = fscanf(f, " endloop\n");
	(*linecount)++;
	if (rc != 0) {
		fprintf(stderr, "failed 'endloop' at line %d\n", *linecount);
		return -1;
	}
	rc = fscanf(f, " endfacet\n");
	(*linecount)++;
	if (rc != 0) {
		fprintf(stderr, "failed 'endloop' at line %d\n", *linecount);
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
#endif
					/* figure out the angle between this vertex and the other two on the triangle */
					float dot = 0;
					if (this_v == t->v[0]) {
						dot = vec3_dot(vec3_normalize_self(vec3_sub(&tv1, &v0, &v1)),
								vec3_normalize_self(vec3_sub(&tv2, &v0, &v2)));
					} else if (this_v == t->v[1]) {
						dot = vec3_dot(vec3_normalize_self(vec3_sub(&tv1, &v1, &v0)),
								vec3_normalize_self(vec3_sub(&tv2, &v1, &v2)));
					} else if (this_v == t->v[2]) {
						dot = vec3_dot(vec3_normalize_self(vec3_sub(&tv1, &v2, &v0)),
								vec3_normalize_self(vec3_sub(&tv2, &v2, &v1)));
					}
					if (dot < 0.0)
						dot = 0.0;
					if (dot > 1.0)
						dot = 1.0;
					float angle = acos(dot);

					vec3_mul_self(&tnormal, angle);

					vec3_add_self(&vnormal, &tnormal);
				}
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
	if (m->v);
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

struct mesh *read_stl_file(char *file)
{
	FILE *f;
	char *s;
	char buffer[500];
	int facetcount, rc;
	int linecount = 0;
	struct mesh *my_mesh;
	int i;

	f = fopen(file, "r");
	if (!f)
		return NULL;

	s = fgets(buffer, 500, f);
	linecount++;
	if (!s) {
		stl_error("unexpected EOF", linecount);
		return NULL;
	}
	facetcount = count_facets(file);
	if (facetcount <= 0) {
		fclose(f);
		return NULL;
	}
	my_mesh = malloc(sizeof(*my_mesh));
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
	process_vertex_normals(my_mesh, 37.0 * M_PI / 180.0, owners);

	free_vertex_owners(owners, facetcount * 3);

	mesh_graph_dev_init(my_mesh);
	return my_mesh;

error:
	fclose(f);
	free_mesh(my_mesh);
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

	s = read_stl_file(argv[1]);
	if (!s) {
		if (errno)
			fprintf(stderr, "%s: %s\n", argv[1], strerror(errno));
		else
			fprintf(stderr, "Failed to read stl file\n");
	} else {
		fprintf(stderr, "Success!, nvertices = %d, ntriangle = %d\n",
				s->nvertices, s->ntriangles);
	}
	print_mesh(s);
	free_mesh(s);
	s = NULL;
	return 0;
}

void mesh_graph_dev_init(struct mesh *m)
{

}

void mesh_graph_dev_cleanup(struct mesh *m)
{

}

#endif
