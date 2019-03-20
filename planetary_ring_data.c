#include <math.h>

#include "planetary_ring_data.h"
#include "mtwist.h"

void init_planetary_ring_data(struct planetary_ring_data ring_data[],
				int nplanetary_rings, uint32_t mtwist_seed)
{

	int i;
	struct mtwist_state *mt;
	float r1, r2;

	mt = mtwist_init(mtwist_seed);

	for (i = 0; i < nplanetary_rings; i++) {
		ring_data[i].alpha = 1.0;
		ring_data[i].texture_v = (float) i / 256.0f;

		r1 = fabsf(mtwist_float(mt) * (MAX_RING_RADIUS - MIN_RING_RADIUS - 0.5)) + MIN_RING_RADIUS + 0.5;
		r2 = fabsf(mtwist_float(mt) * (MAX_RING_RADIUS - MIN_RING_RADIUS - 0.5)) + MIN_RING_RADIUS + 0.5;

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
		int n = ((int) (mtwist_float(mt) * 256 * 100)) % 256;
		float x = ring_data[n].texture_v;
		ring_data[n].texture_v = ring_data[i].texture_v;
		ring_data[i].texture_v = x;
	}
	mtwist_free(mt);
}

