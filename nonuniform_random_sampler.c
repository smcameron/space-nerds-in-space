#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "nonuniform_random_sampler.h"
#include "mtwist.h"

struct intpair {
	int index;
	int relative_odds;
};

struct nonuniform_sample_distribution {
	struct mtwist_state *mt;
	int capacity;
	int nitems;
	struct intpair *entry;
	int sum_of_odds;
};

/* Allocate a new nonuniform sample distribution with given capacity for discrete items */
struct nonuniform_sample_distribution *nonuniform_sample_distribution_init(int capacity, uint32_t seed)
{
	struct nonuniform_sample_distribution *d;

	d = malloc(sizeof(*d));
	d->mt = mtwist_init(seed);
	d->entry = malloc(sizeof(*d->entry) * capacity);
	d->nitems = 0;
	d->capacity = capacity;
	memset(d->entry, 0, sizeof(*d->entry) * capacity);
	d->sum_of_odds = 0;
	return d;
}

/* Free a previously allocated nonunform sample distribution */
void nonuniform_sample_distribution_free(struct nonuniform_sample_distribution *d)
{
	mtwist_free(d->mt);
	free(d->entry);
	d->entry = NULL;
	free(d);
}

/* Add an item with given relative odds into the sample distribution */
void nonuniform_sample_add_item(struct nonuniform_sample_distribution *d, int index, int relative_odds)
{
	if (d->nitems >= d->capacity)
		return;
	if (d->capacity <= index || index < 0)
		return;
	d->entry[d->nitems].index = index;
	d->entry[d->nitems].relative_odds = relative_odds;
	d->sum_of_odds += relative_odds;
	d->nitems++;
}

/* Choose an item randomly according to the given non uniform sample distribution */
int nonuniform_sample(struct nonuniform_sample_distribution *d)
{
	int i, s, n;

	n = mtwist_next(d->mt) % d->sum_of_odds;
	s = 0;
	for (i = 0; i < d->nitems; i++) {
		s += d->entry[i].relative_odds;
		if (s >= n)
			return d->entry[i].index;
	}
	fprintf(stderr, "BUG at %s:%d we should not get to this line\n", __FILE__, __LINE__);
	abort();
}

#ifdef TEST_NONUNIFORM_SAMPLER

#include <sys/time.h>

int main(int argc, char *argv[])
{
	struct nonuniform_sample_distribution *d;
	struct timeval now;
	int i, x;
	uint32_t seed;
	int a = 70000;
	int b = 20000;
	int c = 10000;
	int count[3] = { 0 };

	gettimeofday(&now, NULL);

	d = nonuniform_sample_distribution_init(3, (uint32_t) now.tv_usec);
	nonuniform_sample_add_item(d, 0, a);
	nonuniform_sample_add_item(d, 1, b);
	nonuniform_sample_add_item(d, 2, c);

	for (i = 0; i < 100000; i++) {
		x = nonuniform_sample(d);
		count[x]++;
	}
	nonuniform_sample_distribution_free(d);
	printf("count[0] = %d (expected ~ %d)\n", count[0], a);
	printf("count[0] = %d (expected ~ %d)\n", count[1], b);
	printf("count[0] = %d (expected ~ %d)\n", count[2], c);
	return 0;
}
#endif
