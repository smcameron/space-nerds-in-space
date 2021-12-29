#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "crater.h"
#include "png_utils.h"

int main(__attribute__((unused)) int argc, __attribute__((unused)) char *argv[])
{
	unsigned char *image;
	float crater_r;
	const char *filename = "test_crater_output.png";

	image = malloc(3 * 1024 * 1024);
	memset(image, 127, 3 * 1024 * 1024);
	crater_r = 75.0;
	create_crater_heightmap((unsigned char *) image, 1024, 1024, 512, 512, (int) crater_r, 3);

	if (png_utils_write_png_image(filename, image, 1024, 1024, 0, 0)) {
		fprintf(stderr, "Failed to write png image %s: %s\n", filename, strerror(errno));
		exit(1);
	}
	printf("Wrote png image to %s\n", filename);
	return 0;
}
