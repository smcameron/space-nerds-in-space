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

#include "turret_aimer.h"

#include <math.h>

#include "mathutils.h"

static const struct turret_params default_turret_params = {
	.elevation_lower_limit = 0.0,
	.elevation_upper_limit = 90.0 * M_PI / 180.0,
	.azimuth_lower_limit = -2.0 * M_PI,
	.azimuth_upper_limit = 2.0 * M_PI,
	.elevation_rate_limit = 9.0 * M_PI / 180.0,
	.azimuth_rate_limit = 9.0 * M_PI / 180.0,
};

union quat *turret_aim(double target_x, double target_y, double target_z,
			double turret_x, double turret_y, double turret_z,
			union quat *turret_rest_orientation,
			union quat *current_turret_orientation,
			const struct turret_params *turret,
			union quat *new_turret_orientation,
			union quat *new_turret_base_orientation,
			int *aim_is_good)
{
	union vec3 yaw_axis = { { 0.0, 1.0, 0.0 } };
	union quat yaw, pitch, inverse_rest;
	union vec3 pitch_axis;
	union vec3 to_target, to_current_aim;
	float current_azimuth, azimuth;
	float current_elevation, elevation;
	float delta_elevation, delta_azimuth;
	float xzdist;

	if (!turret)
		turret = &default_turret_params;

	/* Figure out the desired azimuth and elevation */

	to_target.v.x = target_x - turret_x;
	to_target.v.y = target_y - turret_y;
	to_target.v.z = target_z - turret_z;
	quat_inverse(&inverse_rest, turret_rest_orientation);
	quat_rot_vec_self(&to_target, &inverse_rest);
	azimuth = atan2f(-to_target.v.z, to_target.v.x);
	xzdist = sqrtf(to_target.v.z * to_target.v.z + to_target.v.x * to_target.v.x);
	elevation = atan2f(to_target.v.y, xzdist);

	/* At this point we have the desired azimuth and elevation
	 * Now figure out the current elevation and azimuth
	 */

	to_current_aim.v.x = 1.0;
	to_current_aim.v.y = 0.0;
	to_current_aim.v.z = 0.0;
	quat_rot_vec_self(&to_current_aim, current_turret_orientation);
	quat_rot_vec_self(&to_current_aim, &inverse_rest);
	current_azimuth = atan2f(-to_current_aim.v.z, to_current_aim.v.x);
	xzdist = sqrtf(to_current_aim.v.z * to_current_aim.v.z + to_current_aim.v.x * to_current_aim.v.x);
	current_elevation = atan2f(to_current_aim.v.y, xzdist);

	/* Now we have the current elevation and azimuth
	 * Rate limit change to each to make the turret swing smoothly
	 */

	delta_elevation = short_angular_distance(current_elevation, elevation);
	delta_azimuth = short_angular_distance(current_azimuth, azimuth);
	if (delta_azimuth > turret->azimuth_rate_limit)
		delta_azimuth = turret->azimuth_rate_limit;
	else if (delta_azimuth < -turret->azimuth_rate_limit)
		delta_azimuth = -turret->azimuth_rate_limit;
	if (delta_elevation > turret->elevation_rate_limit)
		delta_elevation = turret->elevation_rate_limit;
	else if (delta_elevation < -turret->elevation_rate_limit)
		delta_elevation = -turret->elevation_rate_limit;
	azimuth = current_azimuth + delta_azimuth;
	elevation = current_elevation + delta_elevation;

	/* Enforce upper and lower limits on azimuth and elevation */

	if (elevation > turret->elevation_upper_limit)
		elevation = turret->elevation_upper_limit;
	else if (elevation < turret->elevation_lower_limit)
		elevation = turret->elevation_lower_limit;
	if (azimuth > turret->azimuth_upper_limit)
		azimuth = turret->azimuth_upper_limit;
	else if (azimuth < turret->azimuth_lower_limit)
		azimuth = turret->azimuth_lower_limit;

	/* Now set the azimuth and elevation */

	quat_rot_vec_self(&yaw_axis, turret_rest_orientation);
	quat_init_axis(&yaw, yaw_axis.v.x, yaw_axis.v.y, yaw_axis.v.z, azimuth);
	quat_mul(new_turret_base_orientation, &yaw, turret_rest_orientation);

	pitch_axis.v.x = 0.0;
	pitch_axis.v.y = 0.0;
	pitch_axis.v.z = 1.0;

	quat_rot_vec_self(&pitch_axis, new_turret_base_orientation);
	quat_init_axis(&pitch, pitch_axis.v.x, pitch_axis.v.y, pitch_axis.v.z, elevation);
	quat_mul(new_turret_orientation, &pitch, new_turret_base_orientation);

	if (aim_is_good)
		*aim_is_good = (fabsf(delta_elevation) < 0.5 * M_PI / 180.0 &&
				fabsf(delta_azimuth) < 0.5 * M_PI / 180.0);

	return new_turret_orientation;
}

int turret_can_aim_at(double target_x, double target_y, double target_z,
		double turret_x, double turret_y, double turret_z,
		union quat *turret_rest_orientation,
		const struct turret_params *turret)
{
	union quat inverse_rest;
	union vec3 to_target;
	float azimuth;
	float elevation;
	float xzdist;

	if (!turret)
		turret = &default_turret_params;

	/* Figure out the desired azimuth and elevation */

	to_target.v.x = target_x - turret_x;
	to_target.v.y = target_y - turret_y;
	to_target.v.z = target_z - turret_z;
	quat_inverse(&inverse_rest, turret_rest_orientation);
	quat_rot_vec_self(&to_target, &inverse_rest);
	azimuth = atan2f(-to_target.v.z, to_target.v.x);
	xzdist = sqrtf(to_target.v.z * to_target.v.z + to_target.v.x * to_target.v.x);
	elevation = atan2f(to_target.v.y, xzdist);

	if (elevation > turret->elevation_upper_limit)
		return 0;
	if (elevation < turret->elevation_lower_limit)
		return 0;
	if (azimuth > turret->azimuth_upper_limit)
		return 0;
	if (azimuth < turret->azimuth_lower_limit)
		return 0;
	return 1;
}
