/*
	Copyright (C) 2026 Stephen M. Cameron
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
#include <string.h>

#include "quat.h"

#define DEFINE_SHIP_DEATH_GLOBALS
#include "ship_death.h"

void ship_death_frame_clear(struct ship_death_frame *f)
{
	f->n = 0;
}

struct ship_death_drawable *ship_death_frame_add(struct ship_death_frame *f, struct mesh *m,
					struct material *material, const union vec3 *pos, float scale)
{
	struct ship_death_drawable *d;

	if (f->n >= SHIP_DEATH_MAX_DRAWABLES)
		return NULL;
	d = &f->d[f->n++];
	memset(d, 0, sizeof(*d));
	d->m = m;
	d->material = material;
	d->pos = *pos;
	d->orientation = identity_quat;
	d->scale = scale;
	return d;
}
