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
#include <limits.h>
#include <stdlib.h>

#include "../png_utils.h"

static void usage(void)
{
	fprintf(stderr, "usage: mask_clouds cloud-prefix\n");
	exit(1);
}

static void level_to_alpha_pixel(unsigned char *pixel)
{
	unsigned char r, g, b;
	float level;

	r = pixel[0];
	g = pixel[1];
	b = pixel[2];

	level = ((float) r + (float) g + (float) b) / (3.0 * 255.0);

	pixel[0] = 255;
	pixel[1] = 255;
	pixel[2] = 255;
	pixel[3] = (unsigned char) (level * 255.0);
}

static void level_to_alpha(char *image, int width, int height)
{
	int i, bytes_per_pixel = 4;
	for (i = 0; i < width * height * bytes_per_pixel; i += bytes_per_pixel) {
		unsigned char *pixel = (unsigned char *) &image[i];
		level_to_alpha_pixel(pixel);
	}
}

int main(int argc, char *argv[])
{
	char *input_image;
	char whynot[256];
	int i, width, height, has_alpha;

	if (argc < 2)
		usage();

	for (i = 0; i < 6; i++) {
		char inputfilename[PATH_MAX], outputfilename[PATH_MAX];
		sprintf(inputfilename, "%s%d.png", argv[1], i);
		sprintf(outputfilename, "%s-masked%d.png", argv[1], i);
		printf("Processing %s -> %s\n", inputfilename, outputfilename);
		input_image = png_utils_read_png_image(inputfilename, 0, 0, 0,
						&width, &height, &has_alpha, whynot, sizeof(whynot));
		if (!input_image) {
			fprintf(stderr, "png_utils_read_png_image: %s: %s -- skipping.\n", inputfilename, whynot);
			continue;
		}
		if (!has_alpha) {
			fprintf(stderr, "%s does not have an alpha channel, skipping\n", inputfilename);
			continue;
		}
		level_to_alpha(input_image, width, height);
		if (png_utils_write_png_image(outputfilename, (unsigned char *) input_image, width, height, 1, 0) != 0)
			fprintf(stderr, "Failed to save image %s\n", outputfilename);
		free(input_image);
	}
	return 0;
}

