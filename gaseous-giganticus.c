/*
	Copyright (C) 2014 Stephen M. Cameron
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
#include <stdio.h>
#include <math.h>
#include <time.h>

#include "mtwist.h"
#include "mathutils.h"
#include "quat.h"
#include "simplexnoise1234.h"

#define NPARTICLES 1000000

#define XDIM 1024
#define YDIM 1024

static const float noise_scale = 10.0;
static const float velocity_factor = 10.0;

static struct velocity_field {
	union vec3 v[6][XDIM][YDIM];
} vf;

struct color {
	float r, g, b;
};

struct fij {
	int f, i, j;
};

static struct particle {
	union vec3 pos;
	struct color c;
} particle[NPARTICLES];

static union vec3 fij_to_xyz(int f, int i, int j)
{
	union vec3 answer;

	answer.v.x = 0.0;
	answer.v.y = 0.0;
	answer.v.z = 0.0;
	return answer;
}

static void init_particles(struct particle p[], const int nparticles)
{
	float x, y, z;
	for (int i = 0; i < nparticles; i++) {
		random_point_on_sphere(1.0, &x, &y, &z);
		p[i].pos.v.x = x;
		p[i].pos.v.y = y;
		p[i].pos.v.z = z;
		p[i].c.r = 1.0;
		p[i].c.g = 1.0;
		p[i].c.b = 1.0;
	}
}

static struct fij xyz_to_fij(float x, float y, float z)
{
	struct fij answer;

	answer.f = 0;
	answer.i = 0;
	answer.j = 0;
	return answer;
}

static float noise(union vec3 v, float w)
{
	return snoise4(v.v.x, v.v.y, v.v.z, w);
}

static union vec3 curl(union vec3 v, float noise)
{
	union vec3 answer;

	answer.v.x = noise;
	answer.v.y = noise;
	answer.v.z = noise;
	return answer;
}

static void update_velocity_field(struct velocity_field *vf, float noise_scale, float w)
{
	int f, i, j;
	union vec3 v, c;
	float n;

	for (f = 0; f < 6; f++) {
		for (i = 0; i < XDIM; i++) {
			for (j = 0; j < XDIM; j++) {
				v = fij_to_xyz(f, i, j);
				vec3_mul_self(&v, noise_scale);
				n = noise(v, w * noise_scale);
				c = curl(v, n);
				vec3_mul(&vf->v[f][i][j], &c, velocity_factor);
			}
		}
	}
}

int main(int argc, char *argv[])
{
	init_particles(particle, NPARTICLES);
	update_velocity_field(&vf, noise_scale, 0.0);
	return 0;
}

