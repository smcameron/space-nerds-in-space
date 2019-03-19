#include <math.h>

#include "planetary_ring_data.h"
#include "mtwist.h"

void init_planetary_ring_data(struct planetary_ring_data ring_data[],
				int nplanetary_rings, uint32_t mtwist_seed)
{

	int i;
	struct mtwist_state *mt;

	mt = mtwist_init(mtwist_seed);

	for (i = 0; i < nplanetary_rings; i++) {
		ring_data[i].alpha = 1.0;
		ring_data[i].texture_v = (float) i / 256.0f;
		ring_data[i].inner_radius = MIN_RING_RADIUS +
					2.0f * fabs(mtwist_float(mt) * mtwist_float(mt));
		if (ring_data[i].inner_radius < MIN_RING_RADIUS)
			ring_data[i].inner_radius = MIN_RING_RADIUS;
		ring_data[i].outer_radius = ring_data[i].inner_radius +
				fabs(mtwist_float(mt)) * (MAX_RING_RADIUS + 0.5 - ring_data[i].inner_radius);
		if (ring_data[i].outer_radius > MAX_RING_RADIUS)
			ring_data[i].outer_radius = MAX_RING_RADIUS;
	}
	/* Because of the way that planet rings are chosen based on object id
	 * and because of the way planets are generated and object ids are handed
	 * out we want to scramble the order of ring_data[i].texture_v
	 * so that consecutively generated planets will not have rings that are
	 * too similar.
	 */
	for (i = 0; i < nplanetary_rings; i++) {
		int n = ((int) (mtwist_float(mt) * 256 * 100)) % 256;
		float x = ring_data[n].texture_v;
		ring_data[n].texture_v = ring_data[i].texture_v;
		ring_data[i].texture_v = x;
	}
	mtwist_free(mt);
}

