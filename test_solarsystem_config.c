#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <unistd.h>

#include "solarsystem_config.h"

void usage()
{
	fprintf(stderr, "usage: test_solarsystem_config [solarsystem-asset-file ...]\n");
	fprintf(stderr, "  with no arguments, runs the self tests\n");
}

/* ---- self tests ------------------------------------------------------------------------- */

static int failures;

static void check(int condition, const char *what)
{
	if (condition) {
		printf("  %s\n", what);
		return;
	}
	printf("  FAILED: %s\n", what);
	failures++;
}

/* A preamble that produces NO errors or warnings of its own, so that a test can assert on the
 * count and mean it.  Getting this wrong is not a small thing: with too few planet textures the
 * read bails out before it reaches the star keys at all, and a test of them would pass without
 * one ever having been parsed.  Six gas giants with their atmosphere brightnesses and a star
 * location is the smallest arrangement sanity_check() is entirely happy with -- rocky planets
 * want normal maps, and a missing location is a warning. */
#define FIXTURE_TEXTURE \
	"planet texture: ../../textures/planet-texture0- no-normal-map gas-giant\n" \
	"atmosphere_brightness: 0.5\n"
#define FIXTURE_PREAMBLE \
	"planet texture count: 6\n" \
	FIXTURE_TEXTURE FIXTURE_TEXTURE FIXTURE_TEXTURE \
	FIXTURE_TEXTURE FIXTURE_TEXTURE FIXTURE_TEXTURE \
	"star location: 0 0 0\n"

static struct solarsystem_asset_spec *read_fixture(const char *body)
{
	char filename[] = "/tmp/test_solarsystem_config_XXXXXX";
	struct solarsystem_asset_spec *s;
	int fd = mkstemp(filename);
	FILE *f;

	if (fd < 0) {
		printf("  FAILED: could not create a temporary file\n");
		failures++;
		return NULL;
	}
	f = fdopen(fd, "w");
	if (!f) {
		printf("  FAILED: could not write the temporary file\n");
		failures++;
		close(fd);
		unlink(filename);
		return NULL;
	}
	fputs(FIXTURE_PREAMBLE, f);
	fputs(body, f);
	fclose(f);
	s = solarsystem_asset_spec_read(filename);
	unlink(filename);
	return s;
}

/* sscanf("%f") accepts "nan" and "inf", and every comparison against a NaN is false -- so a
 * range check written as "value < min || value > max" waves one straight through into the
 * struct.  This is a regression test for that, and it applies to every float key, not just the
 * quasar's. */
static void test_nonfinite_floats(void)
{
	struct solarsystem_asset_spec *s;

	printf("nan and inf are not numbers a config may state:\n");
	s = read_fixture("star temperature: nan\n");
	if (s) {
		check(s->spec_errors == 1, "a NaN temperature is one error");
		check(s->star_temperature == s->star_temperature, "and the default survives it");
		solarsystem_asset_spec_free(s);
	}
	s = read_fixture("star brightness: inf\n");
	if (s) {
		check(s->spec_errors == 1, "an infinite brightness is one error");
		solarsystem_asset_spec_free(s);
	}
}

static int run_self_tests(void)
{
	printf("test_solarsystem_config:\n");
	test_nonfinite_floats();
	if (failures) {
		printf("test_solarsystem_config: %d FAILED\n", failures);
		return 1;
	}
	printf("test_solarsystem_config: passed\n");
	return 0;
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
	fprintf(stderr, "s->star_light_tint = %f\n", s->star_light_tint);
	fprintf(stderr, "s->star_dark_tint = %f\n", s->star_dark_tint);
	fprintf(stderr, "s->star_shadow_darkening = %f\n", s->star_shadow_darkening);
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

	/* No arguments means the self tests, so that this can be run by something automatic.
	 * Given files, it stays what it has always been: a printer whose output a human reads. */
	if (argc < 2)
		return run_self_tests();

	for (i = 1; i < argc; i++)
		test_solarsystem_config(argv[i]);
	return 0;
}

