#include <stdio.h>

#include "solarsystem_config.h"

void usage()
{
	fprintf(stderr, "usage: test_solarsystem_config solarsystem-asset-file\n");
}


int test_solarsystem_config(char *filename)
{
	int i;
	struct solarsystem_asset_spec *s;

	fprintf(stderr, "\n\nTesting %s\n\n", filename);
	s = solarsystem_asset_spec_read(filename);
	if (!s) {
		fprintf(stderr, "solarsystem_asset_spec_read() returned NULL\n");
		return 0;
	}

	fprintf(stderr, "s->sun_texture = '%s'\n", s->sun_texture);
	fprintf(stderr, "s->skybox_prefix = '%s'\n", s->skybox_prefix);
	fprintf(stderr, "s->star_diameter = %f\n", s->star_diameter);
	fprintf(stderr, "s->star_diameter_pixels = %f\n", s->star_diameter_pixels);
	fprintf(stderr, "s->sun_style = %d\n", s->sun_style);
	fprintf(stderr, "s->star_temperature = %f\n", s->star_temperature);
	fprintf(stderr, "s->star_brightness = %f (specified %d)\n",
		s->star_brightness, s->star_brightness_specified);
	fprintf(stderr, "s->star_tint = %f %f %f\n",
		s->star_tint[0], s->star_tint[1], s->star_tint[2]);
	fprintf(stderr, "s->sun_edge_softness = %f\n", s->sun_edge_softness);
	fprintf(stderr, "s->star_keys_specified = %d\n", s->star_keys_specified);
	fprintf(stderr, "s->nplanet_textures = %d\n", s->nplanet_textures);
	for (i = 0; i < s->nplanet_textures; i++) {
		fprintf(stderr, "s->planet_texture[%d] = '%s'\n", i, s->planet_texture[i]);
		fprintf(stderr, "s->planet_normalmap[%d] = '%s'\n", i, s->planet_normalmap[i]);
		fprintf(stderr, "s->planet_type[%d] = '%s'\n", i, s->planet_type[i]);
		fprintf(stderr, "s->atmosphere_brightness[%d] = %f\n", i, s->atmosphere_brightness[i]);
	}
	fprintf(stderr, "%d errors, %d warnings\n", s->spec_errors, s->spec_warnings);
	solarsystem_asset_spec_free(s);
	return 0;
}

int main(int argc, char *argv[])
{
	int i;

	if (argc < 2)
		usage();

	for (i = 1; i < argc; i++)
		test_solarsystem_config(argv[i]);
	return 0;
}

