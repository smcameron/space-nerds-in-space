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
#if ((__STDC_VERSION__ >= 199901L) || (_MSC_VER))
	#include <stdint.h>
#endif
#include <string.h>
#include <errno.h>
#include <math.h>
#include <stdlib.h>
#include <getopt.h>

#include "png_utils.h"
#include "open-simplex-noise.h"

#define WIDTH 2048
#define HEIGHT 2048
#define FEATURE_SIZE 16

static int width = WIDTH;
static int height = HEIGHT;
static char *outputfile = "nebulanoise.png";
static int featuresize = FEATURE_SIZE;
static int stripalpha = 1;
static int octaves = 4;

static uint32_t get_pixel(uint32_t *image, const int width, const int height, int x, int y)
{
	return image[y * width + x];
}

static void set_pixel(uint32_t *image, const int width, const int height, int x, int y, uint32_t value)
{
	image[y * width + x] = value;
}

static float clampf(float a, float min_val, float max_val)
{
	return (a < min_val ? min_val : (a > max_val ? max_val : a));
}

static void normalize_image(uint32_t *image, const int width, const int height, const float factor)
{
	int x, y;
	int min, max, v;
	float newv;
	uint32_t rgb;

	max = 0;
	min = INT_MAX;

	for (y = 0; y < height; y++) {
		for (x = 0; x < width; x++) {
			v = get_pixel(image, width, height, x, y) & 0x0ff;
			if (v < min)
				min = v;
			if (v > max)
				max = v;
		}
	}
	for (y = 0; y < height; y++) {
		for (x = 0; x < width; x++) {
			newv = (get_pixel(image, width, height, x, y) & 0x0ff);
			newv = newv - min;
			newv = factor * newv * 255.0 / (max - min);
			rgb = (uint8_t) newv;
			rgb = rgb | rgb << 8 | rgb << 16 | 0xff << 24;
			set_pixel(image, width, height, x, y, (0x0ff << 24) | (rgb));
		}
	}
}

static void combine_images(uint32_t *img1, uint32_t *img2, float factor1, float factor2, int width, int height)
{
	int x, y, i;
	uint32_t v1, v2;
	uint8_t v[4];

	factor1 = clampf(factor1, 0.0, 1.0);
	factor2 = clampf(factor2, 0.0, 1.0);

	for (y = 0; y < height; y++) {
		for (x = 0; x < width; x++) {
			v1 = get_pixel(img1, width, height, x, y);
			v2 = get_pixel(img2, width, height, x, y);
			for (i = 0; i < 4; i++) {
				v[i] = (uint8_t) (factor1 * ((v1 >> (i * 8)) & 0xff) +
							factor2 * ((v2 >> (i * 8)) & 0xff));
			}
			v1 = v[0] | v[1] << 8 | v[2] << 16 | v[3] << 24;
			set_pixel(img1, width, height, x, y, v1);
		}
	}
}


static void generate_nebula(struct osn_context *ctx, uint32_t *image,
		const int width, const int height, const int feature_size, const int octaves)
{
	int x, y;
	double value;
	double v0, v1, v2, v3, v4; /* values from different octaves. */
	uint32_t rgb;

	for (y = 0; y < height; y++) {
		for (x = 0; x < width; x++) {
			v1 = 0;
			v2 = 0;
			v3 = 0;
			/* Use 4 octaves: frequency N, N/2 and N/4 with relative amplitudes 4:2:1. */
			v0 = open_simplex_noise4(ctx, (double) x / feature_size / 8,
						(double) y / feature_size / 8, 0.0, 0.0);
			if (octaves > 1)
				v1 = open_simplex_noise4(ctx, (double) x / feature_size / 4,
							(double) y / feature_size / 4, 0.0, 0.0);
			if (octaves > 2)
				v2 = open_simplex_noise4(ctx, (double) x / feature_size / 2,
							(double) y / feature_size / 2, 0.0, 0.0);
			if (octaves > 3)
				v3 = open_simplex_noise4(ctx, (double) x / feature_size / 1,
							(double) y / feature_size / 1, 0.0, 0.0);

			v4 = open_simplex_noise4(ctx, (double) y / feature_size / 2,
						(double) x / feature_size / 2, 0.0, 0.0)
				 + 0.5 * open_simplex_noise4(ctx, (double) y / feature_size / 4,
							(double) x / feature_size / 4, 0.0, 0.0)
				 + 0.25 * open_simplex_noise4(ctx, (double) y / feature_size / 8,
							(double) x / feature_size / 8, 0.0, 0.0)
				 + 0.125 * open_simplex_noise4(ctx, (double) y / feature_size / 16,
							(double) x / feature_size / 16, 0.0, 0.0);

			value = v0 * 8 / 15.0 + v1 * 4 / 15.0 + v2 * 2 / 15.0 + v3 * 1 / 15.0;

			value = fabs(value);
			/* map -1 to 1 range into 0 to 1 range */
			value = (value * 0.5) + 0.5;

			value = 1.0 - value;


			if (value < 0.5) {
				value = value * 2.0 * M_PI / 2.0;
				value = 1.0 - cos(value);
				value = value * value;
				value = value / 2.0;
			}

			value = pow(value, 2.0);

			/* map 0 to 1 range into -1 to 1 range */
			value = value * 2.0 - 1.0;
			value = value * 0.75 - 0.25 * v4;

			rgb = (uint32_t) ((value + 1) * 127.5);
			rgb = rgb | rgb << 8 | rgb << 16 | 0xff << 24;
			set_pixel(image, width, height, x, y, (0x0ff << 24) | (rgb));
		}
	}
	normalize_image(image, width, height, 1.0);
}

static struct option long_options[] = {
	{ "width", required_argument, NULL, 'w' },
	{ "height", required_argument, NULL, 'h' },
	{ "output", required_argument, NULL, 'o' },
	{ "octaves", required_argument, NULL, 'O' },
	{ "help", required_argument, NULL, 'H' },
	{ "featuresize", required_argument, NULL, 'f' },
	{ "--alpha", no_argument, NULL, 'a' },
	{ 0, 0, 0, 0 },
};

static void usage(void)
{
	fprintf(stderr, "usage: nebula_noise [--octaves o] [--featuresize f] \\\n"
			"\t[--width w] [--height h] [--output outputfile]\n");
	fprintf(stderr, "example:\n\tnebula_noise --f 32 --width 1024 --height 1024 --output myoutput.png\n\n");
	exit(1);
}

static void process_cmdline_options(int argc, char *argv[])
{
	int c, rc, i;

	while (1) {
		int option_index;
		c = getopt_long(argc, argv, "af:h:o:O:w:H", long_options, &option_index);
		if (c == -1)
			break;
		switch (c) {
		case 'a':
			stripalpha = 0;
			break;
		case 'f':
			rc = sscanf(optarg, "%d", &i);
			if (rc != 1)
				usage();
			featuresize = i;
			break;
		case 'h':
			rc = sscanf(optarg, "%d", &i);
			if (rc != 1)
				usage();
			height = i;
			break;
		case 'O':
			rc = sscanf(optarg, "%d", &i);
			if (rc != 1)
				usage();
			octaves = i;
			if (octaves < 1)
				octaves = 1;
			if (octaves > 4)
				octaves = 4;
			break;
		case 'w':
			rc = sscanf(optarg, "%d", &i);
			if (rc != 1)
				usage();
			width = i;
			break;
		case 'o':
			outputfile = optarg;
			break;
		default:
			usage();
		}
	}
}

static uint8_t *strip_alpha(uint32_t *image, int width, int height)
{
	uint8_t *newimage;
	uint32_t v;

	int x, y, n;

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

int main(int argc, char *argv[])
{
	struct osn_context *ctx;
	uint32_t *neb1, *neb2;

	process_cmdline_options(argc, argv);

	neb1 = malloc(sizeof(uint32_t) * width * height);
	neb2 = malloc(sizeof(uint32_t) * width * height);

	open_simplex_noise(77374, &ctx);
	generate_nebula(ctx, neb1, width, height, featuresize, octaves);
	generate_nebula(ctx, neb2, width, height, featuresize * 2, octaves);
	combine_images(neb1, neb2, 0.45, 0.55, width, height);
	normalize_image(neb1, width, height, 1.0);
	if (stripalpha) {
		uint8_t *rgb = strip_alpha(neb1, width, height);
		png_utils_write_png_image(outputfile, rgb, width, height, 0, 0);
	} else {
		png_utils_write_png_image(outputfile, (unsigned char *) neb1, width, height, 1, 0);
	}
	open_simplex_noise_free(ctx);
	free(neb1);
	free(neb2);
	return 0;
}

