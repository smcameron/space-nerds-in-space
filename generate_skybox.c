/*
	Copyright (C) 2018 Stephen M. Cameron
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
#include <stdlib.h>
#include <errno.h>
#include <strings.h>
#include <math.h>

#include "png_utils.h"
#include "mtwist.h"
#include "mathutils.h"
#include "quat.h"

#define MAXDIM 2048
#define NSTARS 10000

int dim = 2048;
int nstars = NSTARS;
uint32_t seed = 23342;
static uint8_t *output_image[6];
static char *starcolors;
static int starcolorwidth, starcolorheight, starcoloralpha;
struct mtwist_state *mt;

union cast {
	double d;
	long l;
};

static inline int float2int(double d)
{
	volatile union cast c;
	c.d = d + 6755399441055744.0;
	return c.l;
}

static float alphablendcolor(float underchannel, float underalpha, float overchannel, float overalpha)
{
	return overchannel * overalpha + underchannel * underalpha * (1.0 - overalpha);
}

struct color {
	float r, g, b, a;
};

static struct color combine_color(struct color *oc, struct color *c)
{
	struct color nc;

	nc.a = (c->a + oc->a * (1.0f - c->a));
	nc.r = alphablendcolor(oc->r, oc->a, c->r, c->a) / nc.a;
	nc.b = alphablendcolor(oc->b, oc->a, c->b, c->a) / nc.a;
	nc.g = alphablendcolor(oc->g, oc->a, c->g, c->a) / nc.a;
	return nc;
}

/* face, i, j -- coords on a cube map */
struct fij {
	int f, i, j;
};

/* convert from cubemap coords to cartesian coords on surface of sphere */
static union vec3 fij_to_xyz(int f, int i, int j, const int dim)
{
	union vec3 answer;

	switch (f) {
	case 0:
		answer.v.x = (float) (i - dim / 2) / (float) dim;
		answer.v.y = -(float) (j - dim / 2) / (float) dim;
		answer.v.z = 0.5;
		break;
	case 1:
		answer.v.x = 0.5;
		answer.v.y = -(float) (j - dim / 2) / (float) dim;
		answer.v.z = -(float) (i - dim / 2) / (float) dim;
		break;
	case 2:
		answer.v.x = -(float) (i - dim / 2) / (float) dim;
		answer.v.y = -(float) (j - dim / 2) / (float) dim;
		answer.v.z = -0.5;
		break;
	case 3:
		answer.v.x = -0.5;
		answer.v.y = -(float) (j - dim / 2) / (float) dim;
		answer.v.z = (float) (i - dim / 2) / (float) dim;
		break;
	case 4:
		answer.v.x = (float) (i - dim / 2) / (float) dim;
		answer.v.y = 0.5;
		answer.v.z = (float) (j - dim / 2) / (float) dim;
		break;
	case 5:
		answer.v.x = (float) (i - dim / 2) / (float) dim;
		answer.v.y = -0.5;
		answer.v.z = -(float) (j - dim / 2) / (float) dim;
		break;
	}
	vec3_normalize_self(&answer);
	return answer;
}

/* convert from cartesian coords on surface of a sphere to cubemap coords */
static struct fij xyz_to_fij(const union vec3 *p, const int dim)
{
	struct fij answer;
	union vec3 t;
	int f, i, j;
	float d;
	const float fdim = (float) dim;

	vec3_normalize(&t, p);

	if (fabs(t.v.x) > fabs(t.v.y)) {
		if (fabs(t.v.x) > fabs(t.v.z)) {
			/* x is longest leg */
			d = fabs(t.v.x);
			if (t.v.x < 0) {
				f = 3;
				i = float2int((t.v.z / d) * fdim * 0.5 + 0.5 * (float) fdim);
			} else {
				f = 1;
				i = float2int((-t.v.z / d)  * fdim * 0.5 + 0.5 * fdim);
			}
		} else {
			/* z is longest leg */
			d = fabs(t.v.z);
			if (t.v.z < 0) {
				f = 2;
				i = float2int((-t.v.x / d) * fdim * 0.5 + 0.5 * fdim);
			} else {
				f = 0;
				i = float2int((t.v.x / d) * fdim * 0.5 + 0.5 * fdim);
#if 0
				/* FIXME: we get this sometimes, not sure why. */
				if (i < 0 || i > 1023)
					printf("i = %d!!!!!!!!!!!!!!!!\n", i);
#endif
			}
		}
		j = float2int((-t.v.y / d) * fdim * 0.5 + 0.5 * fdim);
	} else {
		/* x is not longest leg, y or z must be. */
		if (fabs(t.v.y) > fabs(t.v.z)) {
			/* y is longest leg */
			d = fabs(t.v.y);
			if (t.v.y < 0) {
				f = 5;
				j = float2int((-t.v.z / d) * fdim * 0.5 + 0.5 * fdim);
			} else {
				f = 4;
				j = float2int((t.v.z / d) * fdim * 0.5 + 0.5 * fdim);
			}
			i = float2int((t.v.x / d) * fdim * 0.5 + 0.5 * fdim);
		} else {
			/* z is longest leg */
			d = fabs(t.v.z);
			if (t.v.z < 0) {
				f = 2;
				i = float2int((-t.v.x / d) * fdim * 0.5 + 0.5 * fdim);
			} else {
				f = 0;
				i = float2int((t.v.x / d) * fdim * 0.5 + 0.5 * fdim);
			}
			j = float2int((-t.v.y / d) * fdim * 0.5 + 0.5 * fdim);
		}
	}

	answer.f = f;
	answer.i = i;
	answer.j = j;

	/* FIXME: some other problem makes these checks necessary for now. */
	if (answer.f < 0)
		answer.f = 0;
	else if (answer.f > 5)
		answer.f = 5;
	if (answer.i < 0)
		answer.i = 0;
	else if (answer.i >= dim)
		answer.i = dim - 1;
	if (answer.j < 0)
		answer.j = 0;
	else if (answer.j >= dim)
		answer.j = dim - 1;

	return answer;
}

static void allocate_output_images(int dim)
{
	int i;

	for (i = 0; i < 6; i++) {
		output_image[i] = malloc(4 * dim * dim);
		memset(output_image[i], 0, 4 * dim * dim);
		if (((intptr_t) output_image[i] & 0x03) != 0) {
			/* TODO: just align the damn thing ourself if need be.  It
			 * shouldn't happen, and in my experience, doesn't happen. But,
			 * if it were to happen, we want to know.
			 */
			fprintf(stderr, "generate_skybox: image pointer unexpectedly unaligned: %p\n",
				output_image[i]);
			exit(1);
		}
	}
}

static void read_starcolors(void)
{
	char msg[100] = { 0 };

	/* This image is a 64x64 pixel image with white at the top, and gradients
	 * to various colors as you move towards the bottom.
	 */
	starcolors = png_utils_read_png_image("starcolors.png", 0, 0, 0,
				&starcolorwidth, &starcolorheight, &starcoloralpha,
				msg, sizeof(msg) - 1);
	if (!starcolors) {
		fprintf(stderr, "generate_skybox: %s\n", msg);
		exit(1);
	}
}

static uint32_t get_pixel(uint32_t *image, const int width, const int height, int x, int y)
{
	return image[y * width + x];
}

static void set_pixel(uint32_t *image, const int width, const int height, int x, int y, uint32_t value)
{
	image[y * width + x] = value;
}

static uint8_t *strip_alpha(uint8_t *input_image, int width, int height)
{
	uint8_t *newimage;
	uint32_t v, *image;
	int x, y, n;

	image = (uint32_t *) input_image;

	n = 0;
	newimage = malloc(3 * width * height);
	for (y = 0; y < height; y++) {
		for (x = 0; x < width; x++) {
			v = get_pixel(image, width, height, x, y);
			newimage[n] = (v >> 16) & 0x0ff;
			newimage[n + 1] = (v >> 8) & 0x0ff;
			newimage[n + 2] = v & 0x0ff;
			n += 3;
		}
	}
	return newimage;
}

static void generate_star(struct mtwist_state *mt)
{
	union vec3 position;
	struct fij p;
	uint32_t color = 0xffffffff;
	uint32_t *image;

	/* TODO: something much better than this garbage. */
	consistent_random_point_on_sphere(mt, 1.0, &position.v.x, &position.v.y, &position.v.z);
	p = xyz_to_fij(&position, dim);
	image = (uint32_t *) output_image[p.f];
	set_pixel(image, dim, dim, p.i, p.j, color);
}

static void generate_stars(struct mtwist_state *mt)
{
	int i;

	for (i = 0; i < nstars; i++)
		generate_star(mt);
}

static void save_output_images()
{
	int i, rc;
	char filename[PATH_MAX];
	uint8_t *stripped;

	for (i = 0; i < 6; i++) {
		snprintf(filename, sizeof(filename) - 1, "%s-%d.png", "skybox", i);
		stripped = strip_alpha(output_image[i], dim, dim);
		rc = png_utils_write_png_image(filename, stripped, dim, dim, 0, 0);
		if (rc)
			fprintf(stderr, "generate_skybox: Failed to save image to %s: %s\n",
				filename, strerror(errno));
		free(stripped);
	}
}

int main(int argc, char *argv[])
{
	read_starcolors();
	allocate_output_images(dim);
	mt = mtwist_init(seed);
	generate_stars(mt);
	save_output_images();
	return 0;
}

