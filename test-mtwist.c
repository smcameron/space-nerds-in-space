#include <stdio.h>
#include <stdint.h>

#include "mtwist.h"


int main(int argc, char *argv[])
{
	int i;
	struct mtwist_state *mt;

	mt = mtwist_init(99973);

	for (i = 0; i < 1000; i++) {
		uint32_t x;

		x = mtwist_next(mt);
		printf("%u, %d\n", x, x % 100);
	}
	return 0;
}

