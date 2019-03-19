#ifndef PLANETARY_RING_MATERIAL_H__
#define PLANETARY_RING_MATERIAL_H__

#include <stdint.h>

#define PLANETARY_RING_MTWIST_SEED 178851064
#define NPLANETARY_RING_MATERIALS 256

/* I'm duplicating most of struct material_textured_planet_ring from material.h here
 * because that includes struct sng_color, defined in snis_graph.h
 * which brings in loads of other stuff not needed by snis_server, but we want this
 * info (particularly inner_radius and outer_radius) available in snis_server for
 * ring collision detection.
 */
struct planetary_ring_data { /* almost the same as struct material_textured_planet_ring from material.h */
	/* Note texture_id is deliberately missing here. */
	float alpha;
	float texture_v;
#define MAX_RING_RADIUS 4.0f
#define MIN_RING_RADIUS 1.0f
	float inner_radius;
	float outer_radius;
	/* note struct sng_color is deliberately missing here. */
};

/* Function to consistently init planetary ring data */
void init_planetary_ring_data(struct planetary_ring_data ring_data[], int nplanetary_rings,
					uint32_t mtwist_seed);

#endif
