#ifndef ARBITRARY_SPIN_H__
#define ARBITRARY_SPIN_H__

#include <math.h>
#include "quat.h"

#define NRANDOM_ORIENTATIONS 20
#define NRANDOM_SPINS 20
extern union quat random_orientation[NRANDOM_ORIENTATIONS];
extern union quat random_spin[NRANDOM_SPINS];
extern void initialize_random_orientations_and_spins(int mtwist_seed);
extern void compute_arbitrary_spin(double timestamp, /* in secs, with usec precision */
					union quat *orientation,
					union quat *rotational_velocity);

#endif

