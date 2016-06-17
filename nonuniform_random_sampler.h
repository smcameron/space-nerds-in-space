#ifndef __NONUNIFORM_RANDOM_SAMPLER
#define __NONUNIFORM_RANDOM_SAMPLER

#include <stdint.h>

struct nonuniform_sample_distribution;

/* Allocate a new nonuniform sample distribution with given capacity for discrete items */
struct nonuniform_sample_distribution *nonuniform_sample_distribution_init(int capacity, uint32_t seed);

/* Free a previously allocated nonunform sample distribution */
void nonuniform_sample_distribution_free(struct nonuniform_sample_distribution *d);

/* Add an item with given relative odds into the sample distribution */
void nonuniform_sample_add_item(struct nonuniform_sample_distribution *d, int index, int relative_odds);

/* Choose an item randomly according to the given non uniform sample distribution */
int nonuniform_sample(struct nonuniform_sample_distribution *);

#endif
