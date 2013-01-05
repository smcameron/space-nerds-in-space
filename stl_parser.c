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
	if (rc < 0)
		return rc;
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
		rc = fscanf(f, " vertex %f %f %f\n", &t->v[i].x, &t->v[i].y, &t->v[i].z);
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
	if (facetcount <= 0)
		return NULL;
	my_mesh = malloc(sizeof(*my_mesh));
	my_mesh->t = malloc(sizeof(*my_mesh->t) * facetcount);

	for (i = 0; i < facetcount; i++) {
		rc = read_facet(f, &my_mesh->t[i], &linecount);
		if (rc < 0)
			goto error;
		my_mesh->ntriangles++;
	}
	return my_mesh;

error:
	free(my_mesh->t);
	free(my_mesh);
	return NULL;
}

#ifdef TEST_STL_PARSER 
int main(int argc, char *argv[])
{
	struct mesh *s;

	s = read_stl_file(argv[1]);
	if (!s) {
		if (errno)
			fprintf(stderr, "%s: %s\n", argv[1], strerror(errno));
		else
			fprintf(stderr, "Failed to read stl file\n");
	} else
		fprintf(stderr, "Success!\n");
	return 0;
}
#endif
