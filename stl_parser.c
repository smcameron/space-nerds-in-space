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
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <malloc.h>
#include <string.h>
#include <errno.h>
#include <math.h>

#include "vertex.h"
#include "triangle.h"
#include "mesh.h"

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
	read(fd, tempstr, buf.st_size);
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

static float dist3d(struct vertex *v1, struct vertex *v2)
{
	return sqrt(
		(v1->x - v2->x) * (v1->x - v2->x) +
		(v1->y - v2->y) * (v1->y - v2->y) +
		(v1->z - v2->z) * (v1->z - v2->z));
}

#define VERTEX_MERGING_THRESHOLD (0.00001)
static struct vertex *add_vertex(struct mesh *m, struct vertex *v)
{
	/* Search vertices to see if we already have this one. */
	int i;

	for (i = 0; i < m->nvertices; i++) {
		if (dist3d(v, &m->v[i]) < VERTEX_MERGING_THRESHOLD) {
			return &m->v[i];
		}
	}
	m->v[m->nvertices] = *v;
	m->nvertices++;
	return &m->v[m->nvertices - 1];
}

static int add_facet(FILE *f, struct mesh *m, int *linecount)
{
	int rc, i;
	struct vertex v[3];
	struct triangle t;
	struct vertex *tv;

	for (i = 0; i < 3; i++)
		t.v[i] = &v[i];

	rc = read_facet(f, &t, linecount);
	if (rc)
		return rc;

	for (i = 0; i < 3; i++) {
		tv = add_vertex(m, t.v[i]);
		t.v[i] = tv;
	}
	m->t[m->ntriangles] = t;
	m->ntriangles++;
	return 0;
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
	my_mesh->nvertices = 0;
	my_mesh->ntriangles = 0;
	my_mesh->t = malloc(sizeof(*my_mesh->t) * facetcount);
	my_mesh->v = malloc(sizeof(*my_mesh->v) * facetcount * 3); /* worst case */

	for (i = 0; i < facetcount; i++) {
		rc = add_facet(f, my_mesh, &linecount);
		if (rc < 0)
			goto error;
	}
	fclose(f);
	my_mesh->radius = mesh_compute_radius(my_mesh);
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
#endif
