#ifndef ELASTIC_COLLISION_H__
#define ELASTIC_COLLISION_H__

/*
	Copyright (C) 2015 Stephen M. Cameron
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

#include "quat.h"

/*
 * elastic_collision():
 *
 * Inputs:
 * m1, p1, v1i, and r1 are mass, position, input velocity and radius of particle 1.
 * m2, p2, v2i, and r2 are mass, position, input velocity and radius of particle 2.
 *
 * Outputs:
 * p1, and p2 are output as adjusted positions of particles (separated by at least r1 + r2 after
 * collisions)
 * v1o and v2o are new velocities of particles 1 and 2, respectively.
 */
void elastic_collision(float m1, union vec3 *p1, union vec3 *v1i, float r1,
		float m2, union vec3 *p2, union vec3 *v2i, float r2,
		float energy_transmission_factor, union vec3 *v1o, union vec3 *v2o);

#endif
