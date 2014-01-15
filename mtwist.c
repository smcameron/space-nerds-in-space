/*
	Copyright (C) 2014 Stephen M. Cameron
	Author: Stephen M. Cameron

	This file is part of Spacenerds In Space.

	Spacenerds in Space is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.

	Spacenerds in Space is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with Spacenerds in Space; if not, write to the Free Software
	Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

/* See http://en.wikipedia.org/wiki/Mersenne_twister */

#include <stdlib.h>
#include <string.h>

#include "mtwist.h"

struct mtwist_state {
	uint32_t mt[624];
	uint32_t index;
};

struct mtwist_state *mtwist_init(uint32_t seed)
{
	int i;

	struct mtwist_state *mtstate;

	mtstate = malloc(sizeof(*mtstate));
	if (!mtstate)
		return mtstate;
	memset(mtstate, 0, sizeof(*mtstate));

	mtstate->index = 0;
	mtstate->mt[0] = seed;
	for (i = 1; i < 624; i++) {
		const uint64_t a = 1812433253ULL;
		uint64_t b = (uint64_t) mtstate->mt[i - 1] >> 30;
		uint64_t c = (a * (mtstate->mt[i - 1] ^ b) + i);
		mtstate->mt[i] = c & 0x0ffffffffULL;
	}
	return mtstate;
}

static void generate_numbers(struct mtwist_state *mtstate)
{
	int i;

	for (i = 0; i < 624; i++) {
		uint32_t y = (mtstate->mt[i] & 0x80000000)
				+ (mtstate->mt[(i + 1) % 624] & 0x7fffffff);
		mtstate->mt[i] = mtstate->mt[(i + 397) % 624] ^ (y >> 1);
		if (y % 2)
			mtstate->mt[i] = mtstate->mt[i] ^ 2567483615;
	}
}

uint32_t mtwist_next(struct mtwist_state *mtstate)
{
	if (mtstate->index == 0)
		generate_numbers(mtstate);

	uint32_t y = mtstate->mt[mtstate->index];
	y = y ^ (y >> 11);
	y = y ^ ((y << 7) & 2636928640);
	y = y ^ ((y << 15) & 4022730752);
	y = y ^ (y >> 18);

	mtstate->index = (mtstate->index + 1) % 624;
	return y;
}

float mtwist_float(struct mtwist_state *mtstate)
{
	return (float) mtwist_next(mtstate) / (float) 0xfffffffeUL;
}

int mtwist_int(struct mtwist_state *mtstate, int n)
{
	return (int) (mtwist_next(mtstate) % n);
}

void mtwist_free(struct mtwist_state *mt)
{
	if (mt)
		free(mt);
}

