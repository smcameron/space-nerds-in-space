#ifndef MTWIST_H__
#define MTWIST_H__
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

#include <stdint.h>

struct mtwist_state;

struct mtwist_state *mtwist_init(uint32_t seed);
uint32_t mtwist_next(struct mtwist_state *mtstate);
float mtwist_float(struct mtwist_state *mtstate);
int mtwist_int(struct mtwist_state *mtstate, int n);
void mtwist_free(struct mtwist_state *mt);

#endif

