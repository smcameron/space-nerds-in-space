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

#define DIM 1024
#define FDIM ((float) (DIM - 1))
#define XDIM DIM
#define YDIM DIM

static const int niterations = 1000;
static const float noise_scale = 10.0;
static const float velocity_factor = 10.0;

/* velocity field for 6 faces of a cubemap */
static struct velocity_field {
	union vec3 v[6][XDIM][YDIM];
} vf;

struct color {
	float r, g, b, a;
};

/* face, i, j -- coords on a cube map */
struct fij {
	int f, i, j;
};

/* particles have color, and exist on the surface of a sphere. */
static struct particle {
	union vec3 pos;
	struct color c;
} particle[NPARTICLES];

static float alphablendcolor(float underchannel, float underalpha, float overchannel, float overalpha)
{
	return overchannel * overalpha + underchannel * underalpha * (1.0 - overalpha);
}

static struct color combine_color(struct color *oc, struct color *c)
{
	struct color nc;

	nc.a = (c->a + oc->a * (1.0f - c->a));
	nc.r = alphablendcolor(oc->r, oc->a, c->r, c->a) / nc.a;
	nc.b = alphablendcolor(oc->b, oc->a, c->b, c->a) / nc.a;
	nc.g = alphablendcolor(oc->g, oc->a, c->g, c->a) / nc.a;
	return nc;
}

/* convert from cubemap coords to cartesian coords on surface of sphere */
static union vec3 fij_to_xyz(int f, int i, int j)
{
	union vec3 answer;

	switch (f) {
	case 0:
		answer.v.x = (float) (i - XDIM / 2) / (float) XDIM;
		answer.v.y = -(float) (j - YDIM / 2) / (float) YDIM;
		answer.v.z = (float) DIM / 2.0f;
		break;
	case 1:
		answer.v.x = (float) DIM / 2.0f;
		answer.v.y = -(float) (j - YDIM / 2) / (float) YDIM;
		answer.v.z = -(float) (i - XDIM / 2) / (float) XDIM;
		break;
	case 2:
		answer.v.x = -(float) (i - XDIM / 2) / (float) XDIM;
		answer.v.y = -(float) (j - YDIM / 2) / (float) YDIM;
		answer.v.z = -(float) DIM / 2.0f;
		break;
	case 3:
		answer.v.x = -(float) DIM / 2.0f;
		answer.v.y = -(float) (j - YDIM / 2) / (float) YDIM;
		answer.v.z = (float) (i - XDIM / 2) / (float) XDIM;
		break;
	case 4:
		answer.v.x = (float) (i - XDIM / 2) / (float) XDIM;
		answer.v.y = (float) (float) YDIM / 2.0f;
		answer.v.z = (float) (j - YDIM / 2) / (float) YDIM;
		break;
	case 5:
		answer.v.x = (float) (i - XDIM / 2) / (float) XDIM;
		answer.v.y = -(float) (float) YDIM / 2.0f;
		answer.v.z = -(float) (j - YDIM / 2) / (float) YDIM;
		break;
	}
	vec3_normalize_self(&answer);
	return answer;
}

/* convert from cartesian coords on surface of a sphere to cubemap coords */
static struct fij xyz_to_fij(const union vec3 *p)
{
	struct fij answer;
	union vec3 t;
	int f, i, j;
	float d;

	vec3_normalize(&t, p);

	if (fabs(t.v.x) > fabs(t.v.y)) {
		if (fabs(t.v.x) > fabs(t.v.z)) {
			/* x is longest leg */
			d = fabs(t.v.x);
			if (t.v.x < 0) {
				f = 3;
				i = (int) ((t.v.z / d) * FDIM * 0.5 + 0.5 * (float) FDIM);
			} else {
				f = 1;
				i = (int) ((-t.v.z / d)  * FDIM * 0.5 + 0.5 * FDIM);
			}
		} else {
			/* z is longest leg */
			d = fabs(t.v.z);
			if (t.v.z < 0) {
				f = 2;
				i = (int) ((-t.v.x / d) * FDIM * 0.5 + 0.5 * FDIM);
			} else {
				f = 0;
				i = (int) ((t.v.x / d) * FDIM * 0.5 + 0.5 * FDIM);
			}
		}
		j = (int) ((-t.v.y / d) * FDIM * 0.5 + 0.5 * FDIM);
	} else {
		/* x is not longest leg, y or z must be. */
		if (fabs(t.v.y) > fabs(t.v.z)) {
			/* y is longest leg */
			d = fabs(t.v.y);
			if (t.v.y < 0) {
				f = 5;
				j = (int) ((-t.v.z / d) * FDIM * 0.5 + 0.5 * FDIM);
			} else {
				f = 4;
				j = (int) ((t.v.z / d) * FDIM * 0.5 + 0.5 * FDIM);
			}
			i = (int) ((t.v.x / d) * FDIM * 0.5 + 0.5 * FDIM);
		} else {
			/* z is longest leg */
			d = fabs(t.v.z);
			if (t.v.z < 0) {
				f = 2;
				i = (int) ((-t.v.x / d) * FDIM * 0.5 + 0.5 * FDIM);
			} else {
				f = 0;
				i = (int) ((t.v.x / d) * FDIM * 0.5 + 0.5 * FDIM);
			}
			j = (int) ((-t.v.y / d) * FDIM * 0.5 + 0.5 * FDIM);
		}
	}

	answer.f = f;
	answer.i = i;
	answer.j = j;
	return answer;
}

/* place particles randomly on the surface of a sphere */
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

/* compute the noise gradient at the given point on the surface of a sphere */
static union vec3 noise_gradient(union vec3 position, float w, float noise_scale)
{
	union vec3 g;
	float dx, dy, dz;

	dx = noise_scale * (1.0f / (float) DIM);
	dy = noise_scale * (1.0f / (float) DIM);
	dz = noise_scale * (1.0f / (float) DIM);

	g.v.x = snoise4(position.v.x + dx, position.v.y, position.v.z, w) -
		snoise4(position.v.x - dx, position.v.y, position.v.z, w);
	g.v.y = snoise4(position.v.x, position.v.y + dy, position.v.z, w) -
		snoise4(position.v.x, position.v.y - dy, position.v.z, w);
	g.v.z = snoise4(position.v.x, position.v.y, position.v.z + dz, w) -
		snoise4(position.v.x, position.v.y, position.v.z - dz, w);
	return g;
}

/* compute the curl of the given noise gradient at the given position */
static union vec3 curl(union vec3 pos, union vec3 noise_gradient)
{
	union quat rot;
	union vec3 unrotated_ng, rotated_ng;
	union vec3 straight_up = { { 0.0f, 1.0f, 0.0f } };

	/* calculate quaternion to rotate from point on sphere to straight up. */
	quat_from_u2v(&rot, &pos, &straight_up, &straight_up);

	/* Rotate noise gradient to top of sphere */
	quat_rot_vec(&rotated_ng, &noise_gradient, &rot);


	/* Now we can turn rotated_ng 90 degrees by swapping x and z (using y as tmp) */
	rotated_ng.v.y = rotated_ng.v.z;
	rotated_ng.v.z = rotated_ng.v.x;
	rotated_ng.v.x = rotated_ng.v.y;

	/* Now we can project rotated noise gradient into x-z plane by zeroing y component. */
	rotated_ng.v.y = 0.0f;

	/* Now unrotate projected, 90-degree rotated noise gradient (swap quaternion axis) */
	rot.v.x = -rot.v.x;
	rot.v.y = -rot.v.y;
	rot.v.z = -rot.v.z;
	quat_rot_vec(&unrotated_ng, &rotated_ng, &rot);

	/* and we're done */
	return unrotated_ng;
}

/* compute velocity field for all cells in cubemap.  It is scaled curl of gradient of noise field */
static void update_velocity_field(struct velocity_field *vf, float noise_scale, float w)
{
	int f, i, j;
	union vec3 v, c, ng;

	for (f = 0; f < 6; f++) {
		for (i = 0; i < XDIM; i++) {
			for (j = 0; j < YDIM; j++) {
				v = fij_to_xyz(f, i, j);
				vec3_mul_self(&v, noise_scale);
				ng = noise_gradient(v, w * noise_scale, noise_scale);
				c = curl(v, ng);
				vec3_mul(&vf->v[f][i][j], &c, velocity_factor);
			}
		}
	}
}

/* move a particle according to velocity field at its current location */
static void move_particle(struct particle *p, struct velocity_field *vf)
{
	struct fij fij;

	fij = xyz_to_fij(&p->pos);
	vec3_add_self(&p->pos, &vf->v[fij.f][fij.i][fij.j]);
	vec3_normalize_self(&p->pos);
	vec3_mul_self(&p->pos, (float) XDIM / 2.0f);
}

static void move_particles(struct particle p[], const int nparticles,
			struct velocity_field *vf)
{
	for (int i = 0; i < nparticles; i++)
		move_particle(&p[i], vf);
}

static void update_image(struct particle p[], const int nparticles)
{
}

int main(int argc, char *argv[])
{
	int i;

	printf("Initializing particles\n");
	init_particles(particle, NPARTICLES);
	printf("Initializing velocity field\n");
	update_velocity_field(&vf, noise_scale, 0.0);

	for (i = 0; i < niterations; i++) {
		printf("Iteration: %d\n", i);
		move_particles(particle, NPARTICLES, &vf);
		update_image(particle, NPARTICLES);
	}
	return 0;
}

