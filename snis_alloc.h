#ifndef __SNIS_ALLOC_H__
#define __SNIS_ALLOC_H__
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

#ifdef DEFINE_SNIS_ALLOC_GLOBALS
#define GLOBAL
#else
#define GLOBAL extern
#endif

/* This sets up an object pool allocator. The idea is you have some pre-allocated
 * or statically allocated array that you want to treat as an object pool from
 * which you can "allocate". The snis_object_pool tracks which elements of the
 * array are in use and which are free by setting up a big bitmap with one
 * bit for each element, and providing functions to quickly find a free (zero)
 * bit in the bitmap and set it. Note, these functions must be protected by a
 * mutex when used in a multithreaded program.
 */
struct snis_object_pool;

/* Allocate a new bitmap for up to maxobjs to track allocations in an array */
GLOBAL void snis_object_pool_setup(struct snis_object_pool **pool, int maxobjs);

/* Find and set a free bit in the bitmap (allocate an object).
 * Returns the index into the array of the allocated item, or
 * -1 if the array is fully in use (no unused elements left.)
 */
GLOBAL int snis_object_pool_alloc_obj(struct snis_object_pool *pool);

/* Free an element in the pool (mark the bit as 0) */
GLOBAL void snis_object_pool_free_object(struct snis_object_pool *pool, int i);

/* Memset the entire bitmap to zero, freeing all elements */
GLOBAL void snis_object_pool_free_all_objects(struct snis_object_pool* pool);

/* Return the highest element number in use.  This is tracked as allocations
 * are done, so does not require scanning the bitmap. If there are no allocated
 * objects (the bitmap is entirely cleared) then -1 is returned.
 */
GLOBAL int snis_object_pool_highest_object(struct snis_object_pool *pool);

/* Free the bitmap.  That is, call this when you no longer need to allocate
 * from the array, e.g. when your program is about to exit.
 */
GLOBAL void snis_object_pool_free(struct snis_object_pool *pool);

/* Return 1 if the specified index is allocated (bit is set), 0 otherwise. */
GLOBAL int snis_object_pool_is_allocated(struct snis_object_pool *pool, int id);

#endif
