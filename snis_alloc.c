/*
	Copyright (C) 2010 Stephen M. Cameron
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

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#define DEFINE_SNIS_ALLOC_GLOBALS
#include "snis_alloc.h"
#undef DEFINE_SNIS_ALLOC_GLOBALS

#include "stacktrace.h"

/* borrowed heavily from Word War vi (wordwarvi.c, http://wordwarvi.sf.net ) */

struct snis_object_pool {
	uint32_t *free_obj_bitmap;
	int nbitblocks;
	int highest_object_number;
	int maxobjs;
};

#define BITISSET(pool, id) \
	((pool)->free_obj_bitmap[(id) >> 5] & ((uint32_t) 1 << ((id) % 32)))

void snis_object_pool_setup(struct snis_object_pool **pool, int maxobjs)
{
	struct snis_object_pool *p;

	*pool = malloc(sizeof(**pool));
	p = *pool;
	p->maxobjs = maxobjs;
	p->nbitblocks = ((maxobjs >> 5) + 1);  /* 5, 2^5 = 32, 32 bits per int. */
	p->highest_object_number = -1;
	p->free_obj_bitmap = malloc(sizeof(*p->free_obj_bitmap) * p->nbitblocks);
	memset(p->free_obj_bitmap, 0, sizeof(*p->free_obj_bitmap) * p->nbitblocks);
}

int snis_object_pool_alloc_obj(struct snis_object_pool *pool)
{
	int i, j, answer;
	unsigned int block;

	/* this might be optimized by find_first_zero_bit, or whatever */
	/* it's called that's in the linux kernel.  But, it's pretty */
	/* fast as is, and this is portable without writing asm code. */
	/* Er, portable, except for assuming an int is 32 bits. */

	for (i = 0; i < pool->nbitblocks; i++) {
		if (pool->free_obj_bitmap[i] == 0xffffffff) /* is this block full?  continue. */
			continue;

		/* I tried doing a preliminary binary search using bitmasks to figure */
		/* which byte in block contains a free slot so that the for loop only */
		/* compared 8 bits, rather than up to 32.  This resulted in a performance */
		/* drop, (according to the gprof) perhaps contrary to intuition.  My guess */
		/* is branch misprediction hurt performance in that case.  Just a guess though. */

		/* undoubtedly the best way to find the first empty bit in an array of ints */
		/* is some custom ASM code.  But, this is portable, and seems fast enough. */
		/* profile says we spend about 3.8% of time in here. */
	
		/* Not full. There is an empty slot in this block, find it. */
		block = pool->free_obj_bitmap[i];			
		for (j = 0; j < 32; j++) {
			if (block & 0x01) {	/* is bit j set? */
				block = block >> 1;
				continue;	/* try the next bit. */
			}

			/* Found free bit j */
			answer = (i * 32 + j);	/* return the corresponding array index, if in bounds. */
			/* But it might be in the tail end of the last block, make sure it's in bounds. */
			if (answer >= pool->maxobjs)
				goto allocation_failure;
			if (answer > pool->highest_object_number)
				pool->highest_object_number = answer;
			/* Found free bit, bit j.  Set it, marking it non free.  */
			pool->free_obj_bitmap[i] |= ((uint32_t) 1 << j);
			return answer;
		}
	}

allocation_failure:
#if 0
	printf("snis_object_pool_alloc_obj allocation failed, pool = %p\n", (void *) pool);
	stacktrace("snis_object_pool_alloc_obj allocation failed.\n");
#endif
	return -1;
}

void snis_object_pool_free_object(struct snis_object_pool *pool, int i)
{
	int j;

	pool->free_obj_bitmap[i >> 5] &= ~((uint32_t) 1 << (i % 32)); /* clear the proper bit. */
	if (i != pool->highest_object_number)
		return;

	for (i = pool->nbitblocks - 1; i >= 0; i--) {
		if (pool->free_obj_bitmap[i] == 0)
			continue;
		for (j = 31 ; j >= 0; j--) {
			if (pool->free_obj_bitmap[i] & ((uint32_t) 1 << j)) {
				pool->highest_object_number = (i << 5) + j;
				return;
			}
		}
	}
	pool->highest_object_number = -1;
}

void snis_object_pool_free_all_objects(struct snis_object_pool* pool)
{
	pool->highest_object_number = -1;
	memset(pool->free_obj_bitmap, 0, sizeof(*pool->free_obj_bitmap) * pool->nbitblocks);
}

int snis_object_pool_highest_object(struct snis_object_pool *pool)
{
	return pool->highest_object_number;
}

void snis_object_pool_free(struct snis_object_pool *pool)
{
	free(pool->free_obj_bitmap);
}

int snis_object_pool_is_allocated(struct snis_object_pool *pool, int id)
{
        return BITISSET(pool, id);
}

