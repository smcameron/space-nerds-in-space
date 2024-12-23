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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "crater.h"
#include "mtwist.h"
#include "png_utils.h"
#include "mathutils.h"

unsigned char base_level = 127;

static float zerotoone(void)
{
	return 0.5 * (1.0 + snis_random_float());
}

static int distsqrd(int x1, int y1, int x2, int y2)
{
	return (x2 - x1) * (x2 - x1) + (y2 - y1) * (y2 - y1);
}

static void add_cone(unsigned char *image, int imagew, int imageh, int x, int y, int r, int height)
{
	float d, h, n;
	int i, j, ix, iy, index, d2;

	for (i = 0; i < r * 2; i++) {
		for (j = 0; j < r * 2; j++) {
			d2 = distsqrd(i - r, j - r, 0, 0);
			if (d2 > r * r)
				continue;
			d = sqrtf((float) d2);
			h = height * (r - d) / r;
			ix = (x - r) + i;
			iy = (y - r) + j;
			if (ix < 0 || ix >= imagew || iy < 0 || iy >= imageh)
				continue;
			index = 3 * (iy * imagew + ix);
			n = (float) image[index] + h;
			image[index] = (unsigned char) n;
			image[index + 1] = image[index];
			image[index + 2] = image[index];
		}
	}
}

static float crater_depth_profile(float x)
{
	/* values chosen empirically */
	return sigmoid(x / 50.0, 10.0, 2.0);
}

static void crater_dimple(unsigned char *image, int imagew, int imageh, int x, int y, int r, unsigned char base)
{
	int i, j, ix, iy, index, d2;
	float d;
	float base_height;
	float level;
	float crater_depth = 0.02 * r;

	if (crater_depth < 0.2)
		crater_depth = 0.2;

	ix = x + r;
	iy = y + r;
	if (ix < 0 || ix >= imagew || iy < 0 || iy >= imageh) {
		base_height = base;
	} else {
		index = 3 * (iy * imagew + ix);
		base_height = (float) image[index];
	}

	if (crater_depth > 1.0)
		crater_depth = 1.0;
	for (i = 0; i < r * 2; i++) {
		for (j = 0; j < r * 2; j++) {
			d2 = distsqrd(i - r, j - r, 0, 0);
			if (d2 <= r * r) {
				ix = (x - r) + i;
				iy = (y - r) + j;
				if (ix < 0 || ix >= imagew || iy < 0 || iy >= imageh)
					continue;
				d = sqrtf((float) d2);
				index = 3 * (iy * imagew + ix);
				level = (1.0 - crater_depth) * base_height +
					crater_depth * crater_depth_profile(1.0 - (d / r));
				base = (unsigned char) level;
				image[index] = (unsigned char) base;
				image[index + 1] = image[index];
				image[index + 2] = image[index];
			}
		}
	}
}

static void add_ray(unsigned char *image, int imagew, int imageh, int x, int y, int r, int h)
{
	int i;
	int npoints = snis_randn(100);
	float angle = snis_random_float() * 2.0 * M_PI;
	float radius = r;
	float con_rad = radius * 0.1;
	float tx, ty;
	float height = h;

	for (i = 0; i < npoints; i++) {
		tx = x + cos(angle) * radius;
		ty = y - sin(angle) * radius;
		add_cone(image, imagew, imageh, tx, ty, (int) con_rad, height);
		con_rad = con_rad * 0.98;
		height = height * (0.9 + 0.08 * zerotoone());
		radius = radius + zerotoone() * r * 0.3;
	}
}

static void add_rays(unsigned char *image, int imagew, int imageh, int x, int y, int r, int h, int n)
{
	int i;

	for (i = 0; i < n; i++)
		add_ray(image, imagew, imageh, x, y, r, h);
}

void create_crater_heightmap(unsigned char *image, int imagew, int imageh, int x, int y, int r, int h)
{
	int i, npoints = (int) (1.7 * (M_PI * 2.0 * r));
	float angle;

	crater_dimple(image, imagew, imageh, x, y, r * 0.95, base_level);
	for (i = 0; i < npoints; i++) {
		int tx, ty;

		angle = (2.0 * M_PI * i) / (float) npoints + snis_random_float() * 10.0 * M_PI / 180.0;
		tx = x + cos(angle) * r + snis_random_float() * snis_random_float() * r * 3.9;
		ty = y - sin(angle) * r + snis_random_float() * snis_random_float() * r * 3.9;

		add_cone(image, imagew, imageh, tx, ty, (0.2 + 0.50 * zerotoone()) * r, h);
	}
	add_rays(image, imagew, imageh, x, y, r, h * 2.75, 50 + snis_randn(350));
}

