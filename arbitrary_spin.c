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

#include "arbitrary_spin.h"

void compute_arbitrary_spin(float frame_rate_hz, double timestamp,
				union quat *orientation,
				union quat *rotational_velocity)
{
	/* reduce to axis and angle */
	float x, y, z, a;
	quat_to_axis(rotational_velocity, &x, &y, &z, &a);

	/* current rotation is universe timestamp * rotation per timestamp
	   rotational_velocity is frame_rate_hz and universe is 1/10 sec */
	a = a * frame_rate_hz / (10.0 * frame_rate_hz / 30) * timestamp;

	quat_init_axis(orientation, x, y, z, a);
}

union quat random_orientation[NRANDOM_ORIENTATIONS];
union quat random_spin[NRANDOM_SPINS];

void initialize_random_orientations_and_spins(int mtwist_seed)
{
	int i;
	struct mtwist_state *mt;

	mt = mtwist_init(mtwist_seed);
	for (i = 0; i < NRANDOM_ORIENTATIONS; i++) {
		float angle = mtwist_float(mt) * 2.0 * M_PI;
		consistent_random_axis_quat(mt, &random_orientation[i], angle);
	}
	for (i = 0; i < NRANDOM_SPINS; i++) {
		float angular_speed = ((float) mtwist_int(mt, 100) / 10.0 - 5.0) * M_PI / 180.0;
		consistent_random_axis_quat(mt, &random_spin[i], angular_speed);
	}
	mtwist_free(mt);
}

