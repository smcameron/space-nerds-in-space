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
#ifndef TURRET_AIMER_H__
#define TURRET_AIMER_H__
#include "quat.h"

struct turret_params {

	float elevation_lower_limit;	/* how low and high can the turret be raised in radians */
	float elevation_upper_limit;
	float azimuth_lower_limit;	/* how far left and right can turret swivel in radians */
	float azimuth_upper_limit;
	float elevation_rate_limit;	/* how much can turret move in one step in radians */
	float azimuth_rate_limit;
};

/* Given target location, turret location,
 * current turret orientation, and limits on turret movement and movement rates,
 * compute a new turret orientation aimed more towards the target within those
 * limits.
 *
 * If the turret_params are NULL, then default limits will be used, which are:
 * No limit on azimuth angle (+2 * M_PI to -2 * M_PI)
 * elevation limited to 0 to M_PI
 * elevation and azimuth rate limited to 9 degrees change
 *
 * It is assumed that in the turret model, at rest, the turret is pointed down the X
 * axis, Y axis is up (the azimuth axis) and the Z axis is to the turret's right
 * -- the Z axis is the elevation axis. If your model is different (pretty
 * likely) you'll have to do additional transformations.
 */
union quat *turret_aim(double target_x, double target_y, double target_z,	/* in: world coord position of target */
			double turret_x, double turret_y, double turret_z,	/* in: world coord position of turret */
			union quat *turret_rest_orientation,			/* in: orientation of turret at rest */
			union quat *current_turret_orientation,			/* in: Current orientation of turret */
			const struct turret_params *turret,			/* in: turret params, can be NULL */
			union quat *new_turret_orientation,			/* out: new orientation of turret */
			union quat *new_turret_base_orientation,		/* out: new orientation of the */
										/*      base of the turret */
			int *aim_is_good);					/* out: true if aim is good */

/* Given target location, turret location, orientation, and limits on elevation
 * and azimuth, return true if the turret is capable of aiming at the target
 * or false if it is not capable of aiming at the target.
 */
int turret_can_aim_at(double target_x, double target_y, double target_z,	/* in: world coord pos of target */
		double turret_x, double turret_y, double turret_z,		/* in: world coord pos of turret */
		union quat *turret_rest_orientation,			/* in: orientation of turret at rest */
		const struct turret_params *turret);			/* in: turret params, can be NULL */

#endif
