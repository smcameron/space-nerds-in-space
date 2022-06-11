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
#ifndef ORIENTED_BOUNDING_BOX_H__
#define ORIENTED_BOUNDING_BOX_H__

struct oriented_bounding_box {
	double centerx, centery, centerz;
	double e[3]; /* Scale factor to apply to u[] to get to obb faces */
	union vec3 u[3]; /* x, y, and z unit vectors indicating orientation */
};
/* Returns the closest point q on the surface of an oriented bounding box to the given point.
 * This function is based on ClosestPtPointOBB() from "Real Time Collision Detection" by
 * Christer Ericcson, on page 133.  It is ok for normal_vector to be NULL.
 */
void oriented_bounding_box_closest_point(union vec3 *box_origin, struct oriented_bounding_box *obb,
					union vec3 *closest_point);

#endif
