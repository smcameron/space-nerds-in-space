#include <math.h>

#include "quat.h"
#include "fleet.h"

#define FLEET_SPACING 100.0f

static union vec3 zero = { { 0, 0, 0 } };

static union vec3 fleet_line_position(int position)
{
	union vec3 v = { { -1.0f, 0.0f, 0.0f } };

	if (position == 0)
		return zero;

	vec3_mul_self(&v, (float) position * FLEET_SPACING);
	return v;
}

static union vec3 fleet_triangle_position(int position)
{
	int i, row, rowcount, rowpos;
	static int r[256], rp[256], rc[256], R, RP, RC;
	static int initialized = 0;
	int limit;
	float x, z;

	union vec3 v;
	/* FIXME: there is probably a closed form analytical solution to this */
	/* FIXME: make rows alternate directions so that filling in holes works nicely */

	if (position == 0)
		return zero;

	row = 0;
	rowpos = 0;
	rowcount = 1;
	rc[0] = 1;
	limit = position > 256 ? position : 256;

	if (!initialized || position > 256) {
		for (i = 0; i < limit; i++) {
			if (!initialized && i < 256) {
				r[i] = row;
				rp[i] = rowpos;
				rc[i] = rowcount;
			}
			if (rowpos >= rowcount - 1) {
				row++;
				rowcount++;
				rowpos = 0;
			} else {
				rowpos++;
			}
			R = row;
			RP = rowpos;
			RC = rowcount;
		}
		initialized = 1;
	}

	if (position >= 256) {
		row = R;
		rowpos = RP;
		rowcount = RC;
	} else {
		row = r[position];
		rowpos = rp[position];
		rowcount = rc[position];
	}

	x = FLEET_SPACING * -row;
	//zzz z = (((float) -rowcount / 2.0f) + (float) rowcount * (float) rowpos) * FLEET_SPACING;
	z = ((float) -((rowcount - 1) / 2.0f) + (float) rowpos) * FLEET_SPACING;

	v.v.x = x;
	v.v.y = 0.0f;
	v.v.z = z;
	return v;
}

static union vec3 fleet_square_position(int position)
{
	union vec3 v = { { 0.0f, 0.0f, 0.0f } };
	/* FIXME: do some kind of hilbert curve thing here? */
	return v;
}

union vec3 fleet_position(int fleet_shape, int position, union quat *fleet_orientation)
{
	union vec3 v;

	switch (fleet_shape) {
	case FLEET_LINE:
		v = fleet_line_position(position);
		break;
	case FLEET_TRIANGLE:
		v = fleet_triangle_position(position);
		break;
	case FLEET_SQUARE:
		v = fleet_square_position(position);
		break;
	default:
		v = fleet_triangle_position(position);
		break;
	}
	quat_rot_vec_self(&v, fleet_orientation);
	return v;
}

