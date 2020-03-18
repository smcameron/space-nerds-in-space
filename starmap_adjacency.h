#ifndef STARMAP_ADJACENCY_H__
#define STARMAP_ADJACENCY_H__

#include "ssgl/ssgl.h"
#include "vec4.h"
#include "quat.h"
#define SNIS_SERVER_DATA 1
#include "snis.h"

#define MAXSTARMAPENTRIES 1000 /* max number of solar systems */
#define MAX_STARMAP_ADJACENCIES 5 /* max warp lanes from one star to other stars */
struct starmap_entry {
	char name[SSGL_LOCATIONSIZE];
	double x, y, z;
	int time_before_expiration; /* Used by client */
};

void starmap_set_one_adjacency(int starmap_adjacency[MAXSTARMAPENTRIES][MAX_STARMAP_ADJACENCIES],
					int star_a, int star_b);
void starmap_set_adjacency(int starmap_adjacency[MAXSTARMAPENTRIES][MAX_STARMAP_ADJACENCIES],
				int star_a, int star_b);
void starmap_clear_all_adjacencies(int starmap_adjacency[MAXSTARMAPENTRIES][MAX_STARMAP_ADJACENCIES],
					int nstarmap_entries);
void starmap_compute_adjacencies(int starmap_adjacency[MAXSTARMAPENTRIES][MAX_STARMAP_ADJACENCIES],
					struct starmap_entry starmap[], int nstarmap_entries);
int starmap_stars_are_adjacent(int starmap_adjacency[MAXSTARMAPENTRIES][MAX_STARMAP_ADJACENCIES],
		int star_a, int star_b);

#endif

