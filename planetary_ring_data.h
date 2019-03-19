#ifndef PLANETARY_RING_MATERIAL_H__
#define PLANETARY_RING_MATERIAL_H__

#include <stdint.h>

#include "quat.h"

#define PLANETARY_RING_MTWIST_SEED 178851064
#define NPLANETARY_RING_MATERIALS 256
#define PLANETARY_RING_DAMAGE_DIST 50.0

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

/* Returns true if object_position is colliding with the given planetary ring
 * object_position is the position of the object you're testing.
 * planet_position is the position of the center of the ring.
 * planet_orientation is the orientation of the planet.
 * planet_radius is the radius of the planet
 * ring_inner_radius and ring_outer_radius are the inner and outer radii of the
 * planetary ring in terms of the planet_radius and are between 1.0 and 4.0
 */
int collides_with_planetary_ring(const union vec3 *object_position, const union vec3 *planet_position,
					const union quat *planet_orientation, float planet_radius,
					float ring_inner_radius, float ring_outer_radius);

#endif
