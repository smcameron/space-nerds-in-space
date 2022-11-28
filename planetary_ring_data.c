#include <math.h>

#include "planetary_ring_data.h"
#include "mtwist.h"
#include "quat.h"
#include "mathutils.h"

void init_planetary_ring_data(struct planetary_ring_data ring_data[],
				int nplanetary_rings, uint32_t mtwist_seed)
{

	int i;
	struct mtwist_state *mt;
	float r1, r2;

	mt = mtwist_init(mtwist_seed);

	for (i = 0; i < nplanetary_rings; i++) {
		ring_data[i].alpha = 1.0;
		/* texture_v is the vertical offset into the texture in share/snis/planetary-ring0.png.
		 * We are choosing a horizontal row of pixels in this texture to serve as a cross section
		 * of the planetary ring.
		 */
		ring_data[i].texture_v = (float) i / (float) nplanetary_rings;

		r1 = fabsf(mtwist_float(mt) * (MAX_RING_RADIUS - MIN_RING_RADIUS - 0.5f)) + MIN_RING_RADIUS + 0.5f;
		r2 = fabsf(mtwist_float(mt) * (MAX_RING_RADIUS - MIN_RING_RADIUS - 0.5f)) + MIN_RING_RADIUS + 0.5f;

		if (r1 > r2) {
			ring_data[i].inner_radius = r2;
			ring_data[i].outer_radius = r1;
		} else {
			ring_data[i].inner_radius = r1;
			ring_data[i].outer_radius = r2;
		}
	}
	/* Because of the way that planet rings are chosen based on object id
	 * and because of the way planets are generated and object ids are handed
	 * out we want to scramble the order of ring_data[i].texture_v
	 * so that consecutively generated planets will not have rings that are
	 * too similar.
	 */
	for (i = 0; i < nplanetary_rings; i++) {
		int n = ((int) (mtwist_float(mt) * nplanetary_rings * 100)) % nplanetary_rings;
		float x = ring_data[n].texture_v;
		ring_data[n].texture_v = ring_data[i].texture_v;
		ring_data[i].texture_v = x;
	}
	mtwist_free(mt);
}

int collides_with_planetary_ring(const union vec3 *object_position, const union vec3 *planet_position,
				const union quat *planet_orientation, float planet_radius,
				float ring_inner_radius, float ring_outer_radius)
{
	float d, dist_to_plane, inner, outer, dist;
	union vec3 ring_normal;

	inner = fmap(ring_inner_radius, 1.0, 4.0, 0.0, 1.0);
	outer = fmap(ring_outer_radius, 1.0, 4.0, 0.0, 1.0);
	dist = vec3_dist(object_position, planet_position) / planet_radius;
	d = fmap(dist, 1.0, 4.0, 0.0, 1.0);

	if (d < inner)
		return 0;
	if (d > outer) 
		return 0;

	ring_normal.v.x = 0.0;
	ring_normal.v.y = 0.0;
	ring_normal.v.z = 1.0;

	quat_rot_vec_self(&ring_normal, planet_orientation);
	dist_to_plane = fabsf(plane_to_point_dist(*planet_position, ring_normal, *object_position));
	return (dist_to_plane < PLANETARY_RING_DAMAGE_DIST);
}

