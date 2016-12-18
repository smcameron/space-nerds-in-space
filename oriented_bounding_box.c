/*
	Copyright (C) 2017 Stephen M. Cameron
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
#include <stdio.h> /* for NULL */

#include "quat.h"
#include "oriented_bounding_box.h"

/* Returns the closest_point q on the surface of an oriented bounding box to the given point.
 * This function is based on ClosestPtPointOBB() from "Real Time Collision Detection"
 * by Christer Ericcson, on page 133.
 */
void oriented_bounding_box_closest_point(union vec3 *box_origin, struct oriented_bounding_box *obb,
					union vec3 *closest_point)
{
	union vec3 d,  tmp_point, tmpvec;
	int i;

	d.v.x = box_origin->v.x - obb->centerx;
	d.v.y = box_origin->v.y - obb->centery;
	d.v.z = box_origin->v.z - obb->centerz;

	tmp_point.v.x = obb->centerx;
	tmp_point.v.y = obb->centery;
	tmp_point.v.z = obb->centerz;

	for (i = 0; i < 3; i++) {
		float dist = vec3_dot(&d, &obb->u[i]);
		if (dist > obb->e[i])
			dist = obb->e[i];
		if (dist < -obb->e[i])
			dist = -obb->e[i];
		vec3_mul(&tmpvec, &obb->u[i], dist);
		vec3_add_self(&tmp_point, &tmpvec);
	}
	*closest_point = tmp_point;
}
