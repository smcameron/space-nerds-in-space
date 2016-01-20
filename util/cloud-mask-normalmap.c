#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <getopt.h>
#include <limits.h>

#include "../png_utils.h"

static char *cloudmaskfile = NULL;
static char *normalmapfile = NULL;
static char *outputfile = NULL;

static void usage(void)
{
	fprintf(stderr, "usage: cloud-mask-normalmap -c cloudmaskfile -n normalmapfile -o outputfile\n");
	exit(1);
}

static struct option long_options[] = {
	{ "cloudmaskfile", no_argument, NULL, 'c' },
	{ "normalmapfile", required_argument, NULL, 'n' },
	{ 0 },
};

static int process_options(int argc, char *argv[])
{
	int c;

	while (1) {
		int option_index;

		c = getopt_long(argc, argv, "c:n:o:", long_options, &option_index);
		if (c < 0)
			break;
		switch (c) {
		case 'c':
			cloudmaskfile = optarg;
			break;
		case 'n':
			normalmapfile = optarg;
			break;
		case 'o':
			outputfile = optarg;
			break;
		default:
			usage();
		}
	}
	return 0;
}

#if 0
int png_utils_write_png_image(const char *filename, unsigned char *pixels,
	int w, int h, int has_alpha, int invert);

char *png_utils_read_png_image(const char *filename, int flipVertical, int flipHorizontal,
	int pre_multiply_alpha,
	int *w, int *h, int *hasAlpha, char *whynot, int whynotlen);
#endif

static float bias(float original_value, float towards_value, float fraction)
{
	return original_value + (towards_value - original_value) * fraction;
}

static void mask_normalmap(unsigned char *normal_map, unsigned char *cloud_mask, int w, int h)
{
	int i, j;
	unsigned char *n, *m;
	float alpha;
	float x, y, z;

	for (i = 0; i < w; i++) {
		for (j = 0; j < h; j++) {
			n = normal_map + 4 * w * j + 4 * i;
			m = cloud_mask + 4 * w * j + 4 * i;
			alpha = ((float) m[0] + 127.5) / 255.0f;
			x = (float) n[0] - 127.5f;
			y = (float) n[1] - 127.5f;
			z = (float) n[2] - 127.5f;

			x /= 127.5;
			y /= 127.5;
			z /= 127.5;

			/* this is a questionable adjustment, but seems ok. */
			x = bias(x, 0.0, alpha);
			y = bias(y, 0.0, alpha);
			z = bias(z, 1.0, alpha);

			x = x * 127.5 + 127.5;
			y = y * 127.5 + 127.5;
			z = z * 127.5 + 127.5;

			n[0] = (unsigned char) x;
			n[1] = (unsigned char) y;
			n[2] = (unsigned char) z;
		}
	}
}

int main(int argc, char *argv[])
{
	int nmw, nmh, nm_has_alpha;
	int cmw, cmh, cm_has_alpha;
	char whynot[1024];
	unsigned char *normal_map, *cloud_mask;

	process_options(argc, argv);
	if (normalmapfile == NULL || cloudmaskfile == NULL || outputfile == NULL)
		usage();

	normal_map = png_utils_read_png_image(normalmapfile,
		0, 0, 0, &nmw, &nmh, &nm_has_alpha, whynot, sizeof(whynot) - 1);
	if (!normal_map) {
		fprintf(stderr, "Failed to read png file %s: %s\n", normalmapfile, whynot);
		return -1;
	}
	cloud_mask = png_utils_read_png_image(cloudmaskfile,
		0, 0, 0, &cmw, &cmh, &cm_has_alpha, whynot, sizeof(whynot) - 1);
	if (!cloud_mask) {
		fprintf(stderr, "Failed to read png file %s: %s\n", normalmapfile, whynot);
		return -1;
	}

	if (nmh <= 0 || nmw <= 0) {
		fprintf(stderr, "normal_map width x height is nonsense (%dx%d)! Cannot proceed.\n",
			nmh, nmw);
		exit(1);
	}
	if (cmh != nmh || cmw != nmw) {
		fprintf(stderr, "cloud mask dimensions (%dx%d) != normal map dimensions (%dx%d)!\n",
			cmw, cmh, nmw, nmh);
		exit(1);
	}

	if (!cm_has_alpha) {
		fprintf(stderr, "cloud mask does not have an alpha channel.\n");
		exit(1);
	}
	if (!nm_has_alpha) {
		fprintf(stderr, "normal map does not have an alpha channel.\n");
		exit(1);
	}

	mask_normalmap(normal_map, cloud_mask, nmw, nmh);

	png_utils_write_png_image(outputfile, normal_map, nmw, nmh, nm_has_alpha, 0);

	return 0;
}

