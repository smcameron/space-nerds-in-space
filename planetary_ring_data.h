#ifndef PLANETARY_RING_MATERIAL_H__
#define PLANETARY_RING_MATERIAL_H__

#include <stdint.h>

#include "quat.h"

#define PLANETARY_RING_MTWIST_SEED 178851064
#define NPLANETARY_RING_MATERIALS 256
#define PLANETARY_RING_TEXTURE "textures/planetary-ring0.png"
#define PLANETARY_RING_DAMAGE_DIST 50.0
#define PLANETARY_RING_CELL_SIZE 160.0f
#define PLANETARY_RING_HALF_THICKNESS 45.0f
#define PLANETARY_RING_BUBBLE_RADIUS 3200.0f
#define PLANETARY_RING_FADE_RADIUS 2400.0f
#define PLANETARY_RING_SMALL_BUBBLE_RADIUS 1400.0f
#define PLANETARY_RING_SMALL_FADE_RADIUS 950.0f
#define PLANETARY_RING_MAX_ROCK_RADIUS 48.0f

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

struct planetary_ring_rock {
	union vec3 pos;
	union quat base_orientation;
	union vec3 spin_axis;
	float spin_rate;
	float radius;
	float scale;
	int model;
	int material_index;
};

/* Function to consistently init planetary ring data */
void init_planetary_ring_data(struct planetary_ring_data ring_data[], int nplanetary_rings,
					uint32_t mtwist_seed, const char *ring_texture_path);

/* Sample normalized ring opacity [0.0, 1.0] at normalized radial u and row texture_v */
float planetary_ring_sample_density(float u, float texture_v);

/* Compute world-space orthonormal basis vectors for a planet's equatorial ring plane */
void planetary_ring_basis(const union quat *planet_orientation,
			union vec3 *u_axis, union vec3 *v_axis, union vec3 *normal);

/* Deterministically generate a ring rock in cell (ix, iy) slot sub_index.
 * Returns 1 if populated, 0 if empty.
 */
int planetary_ring_cell_rock(const union vec3 *planet_pos,
			const union quat *planet_orientation,
			float planet_radius,
			const struct planetary_ring_data *ring,
			uint32_t planet_seed, int ix, int iy, int sub_index,
			struct planetary_ring_rock *out);

/* Compute rock orientation including tumble at simulation time_sec (seconds) */
void planetary_ring_rock_spin_orientation(const struct planetary_ring_rock *rock,
					double time_sec, union quat *out);

/* Test a bounding sphere at obj_pos (radius obj_radius) against nearby primary (sub_index 0)
 * ring rocks. Returns 1 on collision (filling hit_rock if non-NULL), 0 otherwise.
 */
int planetary_ring_find_rock_collision(const union vec3 *obj_pos, float obj_radius,
				const union vec3 *planet_pos,
				const union quat *planet_orientation,
				float planet_radius,
				const struct planetary_ring_data *ring,
				uint32_t planet_seed,
				struct planetary_ring_rock *hit_rock);

/* Returns true if object_position is colliding with the given planetary ring
 * object_position is the position of the object you're testing.
 * planet_position is the position of the center of the ring.
 * planet_orientation is the orientation of the planet.
 * planet_radius is the radius of the planet
 * ring_inner_radius and ring_outer_radius are the inner and outer radii of the
 * planetary ring in terms of the planet_radius and are between 1.0 and 4.0
 * ring_texture_v is the v coordinate in the ring texture (0.0 to 1.0)
 */
int collides_with_planetary_ring(const union vec3 *object_position, const union vec3 *planet_position,
					const union quat *planet_orientation, float planet_radius,
					float ring_inner_radius, float ring_outer_radius,
					float ring_texture_v);

#endif
