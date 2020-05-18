/*
	Copyright (C) 2017 Stephen M. Cameron
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
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <math.h>
#include <getopt.h>

#include "../string-utils.h"

#define MAX_SS 1000

static struct solarsystem_data {
	char name[256];
	double x, y, z;
	int nconnections;
} ss[MAX_SS];
static int nsolarsystems = 0;
static int num_to_add = 10;
static int max_connections = 5; /* Should match MAX_STARMAP_ADJACENCIES in ../snis.h */
static double connection_threshold = 15.0;

static void usage(void)
{
	fprintf(stderr, "usage: generate_solarsystem_data [ -s stars-to-add ] [ -c max-connections ]\\\n");
	fprintf(stderr, "                                 [ -t connection-threshold ] asset-files\n");
	fprintf(stderr, "Options:\n");
	fprintf(stderr, "-s, --starcount:   How many stars to add.\n");
	fprintf(stderr, "-t, --threshold:   How close stars must be before they are considered connected.\n");
	fprintf(stderr, "-c, --connections: Max number of connections a star may have. This should be less\n");
	fprintf(stderr, "                   than or equal to MAX_STARMAP_ADJACENCIES in snis.h -- it should be 5\n");
	fprintf(stderr, "Example: Generate coords for 10 new solarsystems\n");
	fprintf(stderr, "         with max 3 connections and connection-threshold of 1.0\n");
	fprintf(stderr, "         using existing solarsystem coords as a starting point:\n\n");
	fprintf(stderr, "   generate_solarsystem_data -s 10 -c 3 -t 1 share/snis/solarsystems/*/assets.txt\n");
	exit(1);
}

static void read_solarsystem_data(char *filename)
{
	char *file_contents, *location;
	int rc, nbytes;
	double x, y, z;
	char *lineend;

	if (nsolarsystems >= MAX_SS) {
		fprintf(stderr, "generate_solarsystem_positions: Too many solarsystems\n");
		return;
	}
	file_contents = slurp_file(filename, &nbytes);
	if (!file_contents) {
		fprintf(stderr, "generate_solarsystem_positions: Failed to read file %s\n", filename);
		return;
	}
	location = strstr(file_contents, "\nstar location:");
	if (!location) {
		fprintf(stderr, "generate_solarsystem_positions: Failed to find location info in file %s\n", filename);
		return;
	}
	if (strlen(location) >= 2)
		location++;
	lineend = strstr(location, "\n");
	if (lineend)
		*lineend = '\0';
	rc = sscanf(location, "star location: %lf %lf %lf", &x, &y, &z);
	if (rc != 3) {
		fprintf(stderr, "generate_solarsystem_positions: Failed to extract location from file %s\n", filename);
		return;
	}
	ss[nsolarsystems].x = x;
	ss[nsolarsystems].y = y;
	ss[nsolarsystems].z = z;
	strlcpy(ss[nsolarsystems].name, filename, sizeof(ss[nsolarsystems].name));
	nsolarsystems++;
}

static double plus_or_minus_one(void)
{
	return 2.0 * (-0.5 + (double) rand() / (double) RAND_MAX);
}

static void generate_new_position(double *x, double *y, double *z)
{
	*x = 99.0 * plus_or_minus_one();
	*y = 99.0 * plus_or_minus_one();
	*z = 99.0 * plus_or_minus_one();
}

double dist(const double x1, const double y1, const double z1, const double x2, const double y2, const double z2)
{
	double dx, dy, dz;

	dx = x1 - x2;
	dy = y1 - y2;
	dz = z1 - z2;

	return sqrt(dx * dx + dy * dy + dz * dz);
}

static int count_connections(double x, double y, double z, int self, double threshold)
{
	int i, count = 0;
	double d;

	for (i = 0; i < nsolarsystems; i++) {
		if (i == self) /* do not count self connections */
			continue;
		d = dist(x, y, z, ss[i].x, ss[i].y, ss[i].z);
		if (d < threshold)
			count++;
	}
	return count;
}

static struct option long_options[] = {
	{ "help", no_argument, NULL, 'h' },
	{ "starcount", required_argument, NULL, 's' },
	{ "connections", required_argument, NULL, 'c' },
	{ "threshold", required_argument, NULL, 't' },
	{ NULL, 0, NULL, '\0' },
};

static int get_parameters(int argc, char *argv[])
{
	int rc, c;

	while (1) {
		int option_index;
		c = getopt_long(argc, argv, "c:hs:t:", long_options, &option_index);
		if (c == -1)
			break;
		switch (c) {
		case 'h':
			usage();
			break;
		case 'c':
			rc = sscanf(optarg, "%d", &max_connections);
			if (rc != 1)
				usage();
			break;
		case 's':
			rc = sscanf(optarg, "%d", &num_to_add);
			if (rc != 1)
				usage();
			break;
		case 't':
			rc = sscanf(optarg, "%lf", &connection_threshold);
			if (rc != 1)
				usage();
			break;
		default:
			usage();
		}
	}
	return optind;
}

static void generate_new_solarsystem_positions(void)
{
	int i, j, connections;
	double x, y, z;
	int new_neighbor[1000];
	int new_neighbor_count;

	for (i = 0; i < num_to_add; i++) {
try_a_new_point:
		do {
			if (nsolarsystems == 0) {
				x = 0.0;
				y = 0.0;
				z = 0.0;
			} else {
				generate_new_position(&x, &y, &z);
			}
			connections = count_connections(x, y, z, -1, connection_threshold);
		} while ((nsolarsystems != 0 && connections <= 0) || connections > max_connections);

		/* If we add this, will it push other systems over the limit? */
		new_neighbor_count = 0;
		for (j = 0; j < nsolarsystems; j++) {
			double d = dist(ss[j].x, ss[j].y, ss[j].z, x, y, z);
			if (d < connection_threshold) {
				if (ss[j].nconnections + 1 > max_connections) {
					/* Over the limit... try again */
					goto try_a_new_point;
				}
				new_neighbor[new_neighbor_count] = j;
				new_neighbor_count++;
			}
		}

		/* Add the new point */
		sprintf(ss[nsolarsystems].name, "solarsystem %d", i);
		ss[nsolarsystems].x = x;
		ss[nsolarsystems].y = y;
		ss[nsolarsystems].z = z;
		ss[nsolarsystems].nconnections = connections;
		nsolarsystems++;

		/* Adjust the new neighbor counts */
		for (j = 0; j < new_neighbor_count; j++)
			ss[new_neighbor[j]].nconnections++;
	}
}

static void read_all_solarsystem_data(int argc, char *argv[], int option_index)
{
	int i;

	for (i = option_index; i < argc; i++)
		read_solarsystem_data(argv[i]);
}

static void print_solarsystem_data(void)
{
	int i;

	for (i = 0; i < nsolarsystems; i++) {
		int c = count_connections(ss[i].x, ss[i].y, ss[i].z, i, connection_threshold);
		printf("%5d %3.1lf %3.1lf %3.1lf : %s\n", c, ss[i].x, ss[i].y, ss[i].z, ss[i].name);
	}
}

static void generate_openscad_starmap(void)
{
	FILE *f;
	int i, j;
	const double scale = 1.0;
	unsigned char *done;
	const char *filename = "starmap.scad";

	printf("Exporting data to OpenSCAD model %s\n", filename);
	f = fopen(filename, "w+");
	if (!f) {
		fprintf(stderr, "generation_solarsystem_positions: Failed to open %s: %s\n",
			filename, strerror(errno));
		return;
	}
	fprintf(f, "include <util/starmap_util.scad>\n\n");

	for (i = 0; i < nsolarsystems; i++) {
		fprintf(f, "star(%lf, %lf, %lf, %lf);\n", ss[i].x * scale, ss[i].y * scale, ss[i].z * scale, 1.0);
	}
	fprintf(f, "\n");

	done = malloc(nsolarsystems * nsolarsystems);
	memset(done, 0, nsolarsystems * nsolarsystems);

	for (i = 0; i < nsolarsystems; i++) {
		for (j = 0; j < nsolarsystems; j++) {
			if (i == j)
				continue;
			if (done[i * nsolarsystems + j])
				continue;
			double d = dist(ss[i].x, ss[i].y, ss[i].z, ss[j].x, ss[j].y, ss[j].z);
			if (d < connection_threshold) {
				fprintf(f, "rod([%lf, %lf, %lf], [%lf, %lf, %lf], 0.05);\n",
					ss[i].x * scale, ss[i].y * scale, ss[i].z * scale,
					ss[j].x * scale, ss[j].y * scale, ss[j].z * scale);
			}
			done[i * nsolarsystems + j] = 1;
			done[j * nsolarsystems + i] = 1;
		}
	}
	fprintf(f, "\n");
	fclose(f);
	free(done);
}

int main(int argc, char *argv[])
{
	int option_index;
	srand(314159);
	option_index = get_parameters(argc, argv);
	read_all_solarsystem_data(argc, argv, option_index);
	generate_new_solarsystem_positions();
	print_solarsystem_data();
	generate_openscad_starmap();
	return 0;
}
