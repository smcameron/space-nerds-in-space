/*
	Copyright (C) 2018 Stephen M. Cameron
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

#include "starmap_adjacency.h"
#include "quat.h"

void starmap_set_one_adjacency(int starmap_adjacency[MAXSTARMAPENTRIES][MAX_STARMAP_ADJACENCIES],
				int star_a, int star_b)
{
	int i;

	for (i = 0; i < MAX_STARMAP_ADJACENCIES; i++) {
		if (starmap_adjacency[star_a][i] == star_b) /* already set */
			return;
		if (starmap_adjacency[star_a][i] == -1) {
			starmap_adjacency[star_a][i] = star_b;
			return;
		}
	}
	fprintf(stderr, "snis_client: Exceeded MAX_STARMAP_ADJACENCIES (%d)\n",
			MAX_STARMAP_ADJACENCIES);
}

void starmap_set_adjacency(int starmap_adjacency[MAXSTARMAPENTRIES][MAX_STARMAP_ADJACENCIES],
				int star_a, int star_b)
{
	starmap_set_one_adjacency(starmap_adjacency, star_a, star_b);
	starmap_set_one_adjacency(starmap_adjacency, star_b, star_a);
}

void starmap_clear_all_adjacencies(int starmap_adjacency[MAXSTARMAPENTRIES][MAX_STARMAP_ADJACENCIES],
					int nstarmap_entries)
{
	int i, j;

	for (i = 0; i < nstarmap_entries; i++)
		for (j = 0; j < MAX_STARMAP_ADJACENCIES; j++)
			starmap_adjacency[i][j] = -1;
}

void starmap_compute_adjacencies(int starmap_adjacency[MAXSTARMAPENTRIES][MAX_STARMAP_ADJACENCIES],
			struct starmap_entry starmap[], int nstarmap_entries)
{
	int i, j;
	union vec3 s1, s2, d;

	starmap_clear_all_adjacencies(starmap_adjacency, nstarmap_entries);
	for (i = 0; i < nstarmap_entries; i++) {
		for (j = i + 1; j < nstarmap_entries; j++) {
			double dist;
			s1.v.x = starmap[i].x;
			s1.v.y = starmap[i].y;
			s1.v.z = starmap[i].z;
			s2.v.x = starmap[j].x;
			s2.v.y = starmap[j].y;
			s2.v.z = starmap[j].z;
			vec3_sub(&d, &s1, &s2);
			dist = vec3_magnitude(&d);
			if (dist <= SNIS_WARP_GATE_THRESHOLD)
				starmap_set_adjacency(starmap_adjacency, i, j);
		}
	}
}

int starmap_stars_are_adjacent(int starmap_adjacency[MAXSTARMAPENTRIES][MAX_STARMAP_ADJACENCIES],
		int star_a, int star_b)
{
	int i;

	for (i = 0; i < MAX_STARMAP_ADJACENCIES; i++) {
		if (starmap_adjacency[star_a][i] == -1)
			return 0;
		if (starmap_adjacency[star_a][i] == star_b)
			return 1;
	}
	return 0;
}

