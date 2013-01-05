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
#include <math.h>
#include "mathutils.h"

#include "vertex.h"
#include "triangle.h"
#include "mesh.h"
#include "stl_parser.h"
#include "entity.h"

#define MAX_ENTITIES 1000

struct entity {
	struct mesh *m;
	float x, y, z; /* world coords */
};

static int nentities = 0;
static struct entity entity_list[MAX_ENTITIES];

int add_entity(struct mesh *m, float x, float y, float z)
{
	if (nentities < MAX_ENTITIES) {
		entity_list[nentities].m = m;
		entity_list[nentities].x = x;
		entity_list[nentities].y = y;
		entity_list[nentities].z = z;
		nentities++;
		return nentities - 1;
	}
	return -1;
}

