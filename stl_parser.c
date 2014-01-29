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

static void clean_spaces(char *line)
{
	char *s, *d;
	int skip_spaces = 1;

	s = line;
	d = line;

	while (*s) {
		if ((*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r') && skip_spaces) {
			s++;
			continue;
		}
		skip_spaces = 0;
		if (*s == '\t' || *s == '\n' || *s == '\r')
			*s = ' ';
		if (*s == ' ')
			skip_spaces = 1;
		*d = *s;
		s++;
		d++;
	}
	*d = '\0';
}

static void remove_trailing_whitespace(char *s)
{
	int len = strlen(s) - 1;

	do {
		switch (s[len]) {
		case '\t':
		case ' ':
		case '\r':
		case '\n':
			s[len] = '\0';
			len--;
		default:
			return;
		}
	} while (1);
}

static void fixup_triangle_vertex_ptrs(struct mesh *m, struct vertex *oldptr, struct vertex *newptr)
{
	int i;

	/* when you realloc() m->v, all the pointers in m->t need fixing up. */
	for (i = 0; i < m->ntriangles; i++) {
		m->t[i].v[0] = newptr + (m->t[i].v[0] - oldptr);
		m->t[i].v[1] = newptr + (m->t[i].v[1] - oldptr);
		m->t[i].v[2] = newptr + (m->t[i].v[2] - oldptr);
	}
}

static int obj_add_vertex(struct mesh *m, char *line, int *verts_alloced)
{
	int rc;
	float x, y, z, w;
	struct vertex *newmem;

	rc = sscanf(line, "v %f %f %f %f", &x, &y, &z, &w);
	if (rc != 4) {
		w = 1.0f;
		rc = sscanf(line, "v %f %f %f", &x, &y, &z);
		if (rc != 3)
			return -1;
	}

	/* Get some more memory for vertices if needed */
	if (m->nvertices + 1 > *verts_alloced) {
		struct vertex *oldptr = m->v;
		*verts_alloced = *verts_alloced + 100;
		newmem = realloc(m->v, *verts_alloced * sizeof(*m->v));
		if (!newmem)
			return -1;
		m->v = newmem;
		fixup_triangle_vertex_ptrs(m, oldptr, newmem);
	}

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
	if (rc == 3) {
		fprintf(stderr, "ignoring w component of texture.\n");
		w = 0.0f;
	} else {
		rc = sscanf(line, "vt %f %f", &u, &v);
		if (rc != 2)
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
	if (rc == 3)
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

static int obj_add_face(struct mesh *m, char *line, int *tris_alloced,
			struct triangle **ft, int *ft_alloced, int *ft_used,
			struct vertex *vt, int nvt, struct vertex *vn, int nvn)
{
	int rc, v[3], tv[3], nv[3], tmp;
	int vvalid, tvalid, nvalid;

	vvalid = 0;
	tvalid = 0;
	nvalid = 0;

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
	rc = sscanf(line, "f %d %d %d %d", &v[0], &v[1], &v[3], &tmp);
	if (rc == 4) {
		fprintf(stderr, "Faces with more than 3 vertices not supported.\n");
		return -1;
	}
	rc = sscanf(line, "f %d %d %d", &v[0], &v[1], &v[3]);
	if (rc == 3) {
		vvalid = 1;
		goto process_it;
	}
	return -1;

process_it:

	if (vvalid)
		if (fixup_vertex_indices(v, m->nvertices))
			return -1;
	if (tvalid)
		if (fixup_vertex_indices(tv, nvt))
			return -1;
	if (nvalid)
		if (fixup_vertex_indices(nv, nvn))
			return -1;

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
	}
	if (tvalid) {
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
	struct vertex *oldptr;

	oldptr = m->v;
	m->v = realloc(m->v, m->nvertices * sizeof(*m->v));
	fixup_triangle_vertex_ptrs(m, oldptr, m->v);
	m->t = realloc(m->t, m->ntriangles * sizeof(*m->t));
	m->tex = realloc(m->t, 3 * m->ntriangles * sizeof(*m->tex));
}

struct mesh *read_obj_file(char *filename)
{
	FILE *f;
	char *s;
	char buffer[500];
	char line[1000];
	int continuation;
	int lineno = 0;
	int verts_alloced = 0;
	int tris_alloced = 0;
	int texture_verts_alloced = 0;
	int texture_verts_used = 0;
	int texture_faces_alloced = 0;
	int texture_faces_used = 0;
	int normal_verts_alloced = 0;
	int normal_verts_used = 0;
	struct mesh *m;
	struct vertex *vt = NULL;
	struct vertex *vn = NULL;
	struct triangle *ft = NULL;

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
		/* read a line... */
		do {
			char *d;

			d = continuation ? d + strlen(d) - 1 : line;
			s = fgets(buffer, sizeof(buffer), f);
			if (!s)
				break;
			lineno++;
			if (s[0] == '#') /* skip comments */
				continue;
			remove_trailing_whitespace(s);
			continuation = (s[strlen(s) - 1] == '\\');
			clean_spaces(s);
			strcpy(d, s);
		} while (continuation || s[0] == '#' || strcmp(line, "") == 0);

		if (!s)
			break;

		/* printf("%d: line = '%s'\n", lineno, line); */

		if (strncmp(line, "v ", 2) == 0) { /* vertex */
			if (obj_add_vertex(m, line, &verts_alloced))
				goto flame_out;
		} else if (strncmp(line, "vt ", 3) == 0) { /* face */
			if (obj_add_texture_vertex(&vt, line,
					&texture_verts_alloced, &texture_verts_used))
				goto flame_out;
		} else if (strncmp(line, "vn ", 3) == 0) { /* vertex normal */
			if (obj_add_vertex_normal(&vn, line,
					&normal_verts_alloced, &normal_verts_used))
				goto flame_out;
		} else if (strncmp(line, "f ", 2) == 0) { /* face */
			if (obj_add_face(m, line, &tris_alloced,
					&ft, &texture_faces_alloced, &texture_faces_used,
					vt, texture_verts_used, vn, normal_verts_used))
				goto flame_out;
		} else if (strncmp(line, "mtllib ", 2) == 0) { /* group */
			printf("ignoring material library: %s\n", line);
		} else if (strncmp(line, "usemtl ", 2) == 0) { /* group */
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
	compact_mesh_allocations(m);

	/* FIXME: need to do coplanar stuff, and normals if they weren't in the file */

	return m;

flame_out:
	fprintf(stderr, "Error parsing %s:%d: %s\n",
		filename, lineno, line);
	free_mesh(m);
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
