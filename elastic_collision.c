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
#include "elastic_collision.h"

void elastic_collision(float m1, union vec3 *p1, union vec3 *v1i, float r1,
		float m2, union vec3 *p2, union vec3 *v2i, float r2,
		float energy_transmission_factor, union vec3 *v1o, union vec3 *v2o)
{
	union vec3 to2, to2n, to1n, deltav1, deltav2;
	float dist;

	vec3_sub(&to2, p2, p1);
	dist = vec3_dist(p1, p2);

	vec3_normalize(&to2n, &to2);
	to1n.v.x = -to2n.v.x;
	to1n.v.y = -to2n.v.y;
	to1n.v.z = -to2n.v.z;

	/* Separate the particles, if necessary */
	if (dist < r1 + r2) {
		/* split the difference, plus a little */
		float amount = (1.01 * (r1 - r2) - dist) / 2.0;
		union vec3 d1, d2;
		vec3_mul(&d1, &to1n, amount);
		vec3_mul(&d2, &to2n, amount);
		vec3_add_self(p1, &d1);
		vec3_add_self(p2, &d2);
	}

	/* transfer momentum... */
	float p1momentum = vec3_dot(v1i, &to2n) * m1 / m2;
	float p2momentum = vec3_dot(v2i, &to1n) * m2 / m1;

	p1momentum *= sqrt(energy_transmission_factor);
	p2momentum *= sqrt(energy_transmission_factor);

	vec3_mul(&deltav1, &to1n, p2momentum);
	vec3_mul(&deltav2, &to2n, p1momentum);

	vec3_add(v1o, v1i, &deltav1);
	vec3_add(v2o, v2i, &deltav2);
}
