#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "quat.h"
#include "fleet.h"

#define FLEET_SPACING 100.0f
#define MAXSHIPSPERFLEET 100
#define MAXFLEETS 50

struct fleet {
	int nships;
	int fleet_shape;
	int32_t id[MAXSHIPSPERFLEET];
};

static struct fleet f[MAXFLEETS];
static int nfleets;

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
	int i, r, c, cdir, rdir, squaresize;
	static int row[MAXSHIPSPERFLEET];
	static int column[MAXSHIPSPERFLEET];
	static int initialized = 0;

	/*  0  1  8  9                 */
	/*  3  2  7  10                */
	/*  4  5  6  11                */
	/*  15 14 13 12                */
        /*  etc.  */

	if (position == 0)
		return zero;

	r = 0;
	c = -1;
	squaresize = 1;
	cdir = 1;
	rdir = 0;

	if (!initialized) {
		for (i = 0; i < MAXSHIPSPERFLEET; i++) {
			c += cdir;
			r += rdir;
			row[i] = r;
			column[i] = c;
			if (cdir == 1 && c == squaresize) {
				cdir = 0;
				if (r == 0)
					rdir = 1;
				else
					rdir = -1;
			} else if (cdir == -1 && c == 0) {
				cdir = 0;
				rdir = 1;
				squaresize++;
			} else if (rdir == -1 && r == 0) {
				cdir = 1;
				rdir = 0;
				squaresize++;
			} else if (rdir == 1 && r == squaresize) {
				rdir = 0;
				if (c == 0)
					cdir = 1;
				else if (c == squaresize)
					cdir = -1;
			}
		}
		initialized = 1;
	}
	v.v.x = (float) row[position] * FLEET_SPACING;
	v.v.y = 0.0f;
	v.v.z = (float) column[position] * FLEET_SPACING;
	return v;
}

union vec3 fleet_position(int fleet_number, int position, union quat *fleet_orientation)
{
	union vec3 v;
	struct fleet *fleet = &f[fleet_number];

	switch (fleet->fleet_shape) {
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

int fleet_new(int fleet_shape, int32_t leader)
{
	int i;

	for (i = 0; i < nfleets; i++) {
		if (f[i].nships == 0) {
			f[i].nships = 1;
			f[i].id[0] = leader;
			f[i].fleet_shape = fleet_shape;
			return i;
		}
	}
	if (nfleets >= MAXFLEETS)
		return -1;
	i = nfleets;
	nfleets++;
	f[i].nships = 1;
	f[i].id[0] = leader;
	f[i].fleet_shape = fleet_shape;
	return i;
}

int fleet_count(void)
{
	return nfleets;
}

void fleet_leave(int32_t id)
{
	int i, j;

	for (i = 0; i < nfleets; i++) {
		for (j = 0; j < f[i].nships;) {
			if (f[i].id[j] == id) {
					memmove(&f[i].id[j], &f[i].id[j + 1],
						sizeof(f[i].id[j]) * (f[i].nships - 1 - j));
					f[i].nships--;
			} else {
				j++;
			}
		}
	}
}

int fleet_join(int fleet_number, int32_t id)
{
	int i, j;

	if (f[fleet_number].nships >= MAXSHIPSPERFLEET)
		return -1;

	for (i = 0; i < nfleets; i++) {
		for (j = 0; j < f[i].nships;) {
			if (f[i].id[j] == id) {
				if (i == fleet_number) {
					return j;
				} else {
					memmove(&f[i].id[j], &f[i].id[j + 1],
						sizeof(f[i].id[j]) * (f[i].nships - 1 - j));
					f[i].nships--;
				}
			} else {
				j++;
			}
		}
	}
	f[fleet_number].id[f[fleet_number].nships] = id;
	f[fleet_number].nships++;
	return f[fleet_number].nships - 1;
}

int fleet_position_number(int fleet_number, int32_t id)
{
	int i;

	for (i = 0; i < f[fleet_number].nships; i++)
		if (f[fleet_number].id[i] == id)
			return i;
	return -1;
}

int32_t fleet_get_leader_id(int fleet_number)
{
	return f[fleet_number].id[0];
}

