/*
	Copyright (C) 2016 Stephen M. Cameron
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

/* This program samples 4 points on an image, averages them and prints out the RGB value.
 * The intended use is to sample the textures for gas giants to produce a reasonable
 * color for the atmosphere effect.
 */

#include <stdio.h>
#include <stdlib.h>

#include "../png_utils.h"

static void sample_image_colors(char *imagedata, int width, int height,
				unsigned char *r, unsigned char *g, unsigned char *b)
{
	int i;
	unsigned char *p;
	float red, green, blue;
	float x, y;
	const float samplex[] = { 0.25, 0.75, 0.25, 0.75 };
	const float sampley[] = { 0.25, 0.25, 0.75, 0.75 };

	red = 0.0;
	green = 0.0;
	blue = 0.0;
	for (i = 0; i < 4; i++) {
		x = width * samplex[i];
		y = height * sampley[i];
		p = (unsigned char *) &imagedata[(int) (width * 3.0 * y + x * 3.0)];
		red += 0.25 * (float) p[0];
		green += 0.25 * (float) p[1];
		blue += 0.25 * (float) p[2];
	}
	*r = (unsigned char) red;
	*g = (unsigned char) green;
	*b = (unsigned char) blue;
}

static void usage(void)
{
	fprintf(stderr, "Usage: sample_image_colors filename\n");
	exit(1);
}

int main(int argc, char *argv[])
{
	char *image_filename;
	char *image_data;
	char whynot[1024];
	int hasalpha = 0;
	unsigned char r, g, b;
	int i, height, width;

	if (argc < 2)
		usage();

	for (i = 1; i < argc; i++) {
		image_filename = argv[i];
		image_data = png_utils_read_png_image(image_filename, 0, 0, 0,
							&width, &height, &hasalpha, whynot, sizeof(whynot));
		if (!image_data) {
			fprintf(stderr, "Failed to read png file '%s': %s\n", image_filename, whynot);
			exit(1);
		}
		sample_image_colors(image_data, width, height, &r, &g, &b);
		printf("Sampled colors from %s, rgb = %hhu, %hhu, %hhu\n", image_filename, r, g, b);
	}
	exit(0);
}
