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
#include <errno.h>

#include "string-utils.h"
#include "solarsystem_config.h"
#include "build_bug_on.h"

static void free_string_ptr(char **x)
{
	if (!x)
		return;
	if (*x) {
		free(*x);
		*x = NULL;
	}
}

/* Parse one "key: value" line as a float and range check it.  Returns 0 on success, or -1 with a
 * message already printed, so the caller can just goto bad_line.  The star and sun keys all have
 * this shape, and spelling each one out was three times the code and three times the places to
 * get the error message wrong. */
static int parse_float_field(char *filename, int ln, char *line, const char *what,
				float min, float max, float *result)
{
	float value;

	if (sscanf(get_field(line), "%f", &value) != 1) {
		fprintf(stderr, "%s:line %d: bad %s specification.\n", filename, ln, what);
		return -1;
	}
	if (value < min || value > max) {
		fprintf(stderr, "%s:line %d: %s must be between %g and %g.\n",
			filename, ln, what, min, max);
		return -1;
	}
	*result = value;
	return 0;
}

static void sanity_check(char *filename, struct solarsystem_asset_spec *ss)
{
	int i;

	for (i = 0; i < ss->nplanet_textures; i++) {
		if (strcmp(ss->planet_type[i], "rocky") != 0 &&
			strcmp(ss->planet_type[i], "earthlike") != 0 &&
			strcmp(ss->planet_type[i], "gas-giant") != 0 &&
			strcmp(ss->planet_type[i], "ice-giant") != 0) {
			fprintf(stderr, "%s: unknown planet type '%s'\n", filename, ss->planet_type[i]);
		}
		if (strcmp(ss->planet_type[i], "rocky") != 0 &&
			strcmp(ss->planet_type[i], "earthlike") != 0)
			continue;
		if (strcmp(ss->planet_normalmap[i], "no-normal-map") == 0) {
			fprintf(stderr, "%s: planet texture %s is type %s but has no normalmap\n",
				filename, ss->planet_texture[i], ss->planet_type[i]);
			ss->spec_warnings++;
		}
	}
}

float solarsystem_star_diameter(const struct solarsystem_asset_spec *a, float texture_width)
{
	if (a->star_diameter_specified)
		return a->star_diameter;
	if (texture_width <= 0.0 || a->star_diameter_pixels <= 0.0)
		return a->star_diameter;
	return a->star_diameter_pixels * SOLARSYSTEM_SUN_BILLBOARD_SIZE / texture_width;
}

struct solarsystem_asset_spec *solarsystem_asset_spec_read(char *filename)
{
	FILE *f;
	struct solarsystem_asset_spec *a;
	char *field, *l, line[1000];
	int rc, ln = 0;
	int planet_textures_read = 0;
	int atmosphere_brightnesses_read = 0;
	int atmosphere_brightnesses_expected = 0;
	int planet_textures_expected = 0;
	int got_position = 0;
	int i;

	f = fopen(filename, "r");
	if (!f) {
		fprintf(stderr, "fopen: %s: %s\n", filename, strerror(errno));
		return NULL;
	}
	a = malloc(sizeof(*a));
	memset(a, 0, sizeof(*a));
	a->random_seed = -1; /* no seed */
	/* Defaults chosen so a 512 pixel wide sun texture yields the traditional
	 * 30000 unit sun billboard: 5625 * 512 / 96 = 30000.  star_diameter is only a fallback
	 * for callers that cannot measure the texture; solarsystem_star_diameter() infers it. */
	a->star_diameter_pixels = 96.0;
	a->star_diameter = 5625.0;
	a->sun_color.r = 255;
	a->sun_color.g = 255;
	a->sun_color.b = 179;
	/* Star rendering defaults.  These are a least-squares fit of the sun shader to the shipped
	 * sun.png.  The STYLE defaults to the original textured billboard, so a system that says
	 * nothing renders exactly as it always has and nothing changes for anyone who has not
	 * asked for it; "sun style: shader" opts a system in, and SUN_STYLE on the demon console
	 * flips it live.  The rest are what a system that opts in gets if it says nothing else.
	 * The derivation, and what could not be matched, are in doc/star-rendering-and-lighting-notes.txt. */
	a->sun_style = SOLARSYSTEM_SUN_STYLE_TEXTURE;
	a->star_temperature = 5980.0;	/* fitted to the shipped sun.png */
	a->star_tint[0] = 1.0;		/* no deviation from the blackbody colour */
	a->star_tint[1] = 1.0;
	a->star_tint[2] = 1.0;
	a->sun_edge_softness = 0.01;
	/* Star-coloured lighting, as tuned in shadow_lab.  All three at 0 = the old untinted look. */

	while (!feof(f)) {
		l = fgets(line, 1000, f);
		if (!l)
			break;
		trim_whitespace(line);
		ln++;

		if (line[0] == '#') /* skip comments */
			continue;
		clean_spaces(line);
		if (strcmp(line, "") == 0) /* skip blank lines */
			continue;
		if (has_prefix("planet texture count:", line)) {
			int value;

			field = get_field(line);
			rc = sscanf(field, "%d", &value);
			if (rc != 1) {
				fprintf(stderr, "%s:%d: failed to parse '%s' as integer, rc = %d\n",
					filename, ln, field, rc);
				goto bad_line;
			}
			/* FIXME: what should this limit really be? */
			if (value > PLANET_TYPE_COUNT_SHALL_BE) {
				fprintf(stderr, "%s:line %d: planet texture count %d exceeds max %d, capping\n",
						filename, ln, value, PLANET_TYPE_COUNT_SHALL_BE);
				value = PLANET_TYPE_COUNT_SHALL_BE;
				a->spec_warnings++;
			}
			planet_textures_expected = value;
			atmosphere_brightnesses_expected = value;
			if (a->planet_texture != NULL || a->nplanet_textures != 0) {
				fprintf(stderr, "%s:line %d: multiple planet texture counts encountered, ignoring\n",
						filename, ln);
				goto bad_line;
			}
			a->nplanet_textures = PLANET_TYPE_COUNT_SHALL_BE;
			a->planet_texture = malloc(sizeof(a->planet_texture[0]) * PLANET_TYPE_COUNT_SHALL_BE);
			memset(a->planet_texture, 0, sizeof(a->planet_texture[0]) * PLANET_TYPE_COUNT_SHALL_BE);
			a->planet_normalmap = malloc(sizeof(a->planet_normalmap[0]) * PLANET_TYPE_COUNT_SHALL_BE);
			memset(a->planet_normalmap, 0, sizeof(a->planet_normalmap[0]) * PLANET_TYPE_COUNT_SHALL_BE);
			a->planet_type = malloc(sizeof(a->planet_type[0]) * PLANET_TYPE_COUNT_SHALL_BE);
			memset(a->planet_type, 0, sizeof(a->planet_type[0]) * PLANET_TYPE_COUNT_SHALL_BE);
			a->atmosphere_color = malloc(sizeof(a->atmosphere_color[0]) * PLANET_TYPE_COUNT_SHALL_BE);
			a->water_color = malloc(sizeof(a->atmosphere_color[0]) * PLANET_TYPE_COUNT_SHALL_BE);
			a->atmosphere_brightness = malloc(sizeof(a->atmosphere_brightness[0]) *
								PLANET_TYPE_COUNT_SHALL_BE);
			for (i = 0; i < PLANET_TYPE_COUNT_SHALL_BE; i++)
				a->atmosphere_brightness[i] = 0.5;  /* Default */
			continue;
		} else if (has_prefix("planet texture:", line)) {
			if (a->nplanet_textures == 0) {
				fprintf(stderr, "%s:line %d: encountered planet texture before planet texture count\n",
						filename, ln);
				goto bad_line;
			}
			if (planet_textures_read >= a->nplanet_textures) {
				fprintf(stderr, "%s:line %d: too many planet textures.\n", filename, ln);
				goto bad_line;
			}
			char word1[1000], word2[1000], word3[1000];
			unsigned char r, g, b;
			a->water_color[planet_textures_read].r = 25; /* default values for r,g,b water color */
			a->water_color[planet_textures_read].g = 76; /* May be overridden later */
			a->water_color[planet_textures_read].b = 255;
			field = get_field(line);
			BUILD_ASSERT(sizeof(word1) == 1000);
			BUILD_ASSERT(sizeof(word2) == 1000);
			BUILD_ASSERT(sizeof(word3) == 1000);
			rc = sscanf(field, "%999s %999s %999s %hhu %hhu %hhu", word1, word2, word3, &r, &g, &b);
			if (rc == 6) {
				a->planet_texture[planet_textures_read] = strdup(word1);
				a->planet_normalmap[planet_textures_read] = strdup(word2);
				a->planet_type[planet_textures_read] = strdup(word3);
				a->atmosphere_color[planet_textures_read].r = r;
				a->atmosphere_color[planet_textures_read].g = g;
				a->atmosphere_color[planet_textures_read].b = b;
				planet_textures_read++;
				continue;
			}
			rc = sscanf(field, "%999s %999s %hhu %hhu %hhu", word1, word2, &r, &g, &b);
			if (rc == 5) {
				a->planet_texture[planet_textures_read] = strdup(word1);
				a->planet_type[planet_textures_read] = strdup(word2);
				a->planet_normalmap[planet_textures_read] = strdup("no-normal-map");
				a->atmosphere_color[planet_textures_read].r = r;
				a->atmosphere_color[planet_textures_read].g = g;
				a->atmosphere_color[planet_textures_read].b = b;
				planet_textures_read++;
				continue;
			}
			rc = sscanf(field, "%999s %999s %999s", word1, word2, word3);
			if (rc == 3) {
				a->planet_texture[planet_textures_read] = strdup(word1);
				a->planet_normalmap[planet_textures_read] = strdup(word2);
				a->planet_type[planet_textures_read] = strdup(word3);
				a->atmosphere_color[planet_textures_read].r = 81;
				a->atmosphere_color[planet_textures_read].g = 81;
				a->atmosphere_color[planet_textures_read].b = 255;
				planet_textures_read++;
				continue;
			}
			rc = sscanf(field, "%999s %999s", word1, word2);
			if (rc == 2) { /* old style, no normal map */
				a->planet_texture[planet_textures_read] = strdup(word1);
				a->planet_normalmap[planet_textures_read] = strdup("no-normal-map");
				a->planet_type[planet_textures_read] = strdup(word2);
				a->atmosphere_color[planet_textures_read].r = 81;
				a->atmosphere_color[planet_textures_read].g = 81;
				a->atmosphere_color[planet_textures_read].b = 255;
				planet_textures_read++;
				fprintf(stderr,
					"%s:line %d: expected planet texture prefix, planet normal map prefix, and planet type\n",
					filename, ln);
				fprintf(stderr,
					"%s:line %d: Assuming old style without normal map (use no-normal-map to suppress this message).\n",
					filename, ln);
				a->spec_warnings++;
				continue;
			}
			fprintf(stderr,
				"%s:line %d: expected planet texture prefix, [ planet normal map prefix ], and planet type\n",
				filename, ln);
			goto bad_line;
		} else if (has_prefix("atmosphere brightness:", line) ||
				has_prefix("atmosphere_brightness:", line)) { /* assets.txt may vary */
			float value;
			if (a->nplanet_textures == 0) {
				fprintf(stderr,
					"%s:line %d: found atmosphere brightness before planet texture count.\n",
						filename, ln);
				goto bad_line;
			}
			if (atmosphere_brightnesses_read >= a->nplanet_textures) {
				fprintf(stderr, "%s:line %d: Too many atmosphere brightnesses\n", filename, ln);
				goto bad_line;
			}
			field = get_field(line);
			rc = sscanf(field, "%f", &value);
			if (rc != 1) {
				fprintf(stderr, "%s:line %d: bad atmosphere brightness specification.\n", filename, ln);
				goto bad_line;
			}
			if (value < 0.0 || value > 1.0) {
				fprintf(stderr, "%s:line %d: atmosphere brightness must be between 0.0 and 1.0.\n",
					filename, ln);
				goto bad_line;
			}
			a->atmosphere_brightness[atmosphere_brightnesses_read] = value;
			atmosphere_brightnesses_read++;
			continue;
		} else if (has_prefix("water color:", line)) {
			unsigned char r, g, b;
			rc = sscanf(line, "water color: %hhu, %hhu, %hhu", &r, &g, &b);
			if (rc != 3) {
				fprintf(stderr, "%s:line %d: bad water color specification.\n", filename, ln);
				goto bad_line;
			}
			if (planet_textures_read <= 0) {
				fprintf(stderr, "%s:line %d: water color before 1st planet texture, skipping.\n",
					filename, ln);
				goto bad_line;
			}
			a->water_color[planet_textures_read - 1].r = r;
			a->water_color[planet_textures_read - 1].g = g;
			a->water_color[planet_textures_read - 1].b = b;
			continue;
		} else if (has_prefix("sun color:", line)) {
			unsigned char r, g, b;
			rc = sscanf(line, "sun color: %hhu, %hhu, %hhu", &r, &g, &b);
			if (rc != 3) {
				fprintf(stderr, "%s:line %d: bad sun color specification.\n", filename, ln);
				goto bad_line;
			}
			a->sun_color.r = r;
			a->sun_color.g = g;
			a->sun_color.b = b;
			continue;
		} else if (has_prefix("sun texture:", line)) {
			if (a->sun_texture != NULL) {
				fprintf(stderr, "%s:line %d: too many sun textures.\n", filename, ln);
				goto bad_line;
			}
			a->sun_texture = strdup(get_field(line));
			continue;
		} else if (has_prefix("skybox texture:", line)) {
			if (a->skybox_prefix != NULL) {
				fprintf(stderr, "%s:line %d: too many skybox prefixes.\n", filename, ln);
				goto bad_line;
			}
			a->skybox_prefix = strdup(get_field(line));
			continue;
		} else if (has_prefix("star diameter pixels:", line)) {
			float value;

			field = get_field(line);
			rc = sscanf(field, "%f", &value);
			if (rc != 1 || value <= 0.0) {
				fprintf(stderr, "%s:line %d: bad star diameter pixels specification.\n",
					filename, ln);
				goto bad_line;
			}
			a->star_diameter_pixels = value;
			a->star_keys_specified++;
			continue;
		} else if (has_prefix("star diameter:", line)) {
			float value;

			field = get_field(line);
			rc = sscanf(field, "%f", &value);
			if (rc != 1 || value <= 0.0) {
				fprintf(stderr, "%s:line %d: bad star diameter specification.\n",
					filename, ln);
				goto bad_line;
			}
			a->star_diameter = value;
			a->star_diameter_specified = 1;
			continue;
		} else if (has_prefix("sun style:", line)) {
			field = get_field(line);
			if (strcmp(field, "shader") == 0) {
				a->sun_style = SOLARSYSTEM_SUN_STYLE_SHADER;
			} else if (strcmp(field, "texture") == 0) {
				a->sun_style = SOLARSYSTEM_SUN_STYLE_TEXTURE;
			} else {
				fprintf(stderr, "%s:line %d: sun style must be 'shader' or 'texture'.\n",
					filename, ln);
				goto bad_line;
			}
			a->star_keys_specified++;
			continue;
		} else if (has_prefix("star brightness:", line)) {
			if (parse_float_field(filename, ln, line, "star brightness",
						0.0, 1e9, &a->star_brightness))
				goto bad_line;
			a->star_brightness_specified = 1;
			a->star_keys_specified++;
			continue;
		} else if (has_prefix("star temperature:", line)) {
			/* The blackbody locus only describes stars over roughly this range: below it
			 * the colour goes red then simply dark, and above it stops changing. */
			if (parse_float_field(filename, ln, line, "star temperature",
						1900.0, 40000.0, &a->star_temperature))
				goto bad_line;
			a->star_keys_specified++;
			continue;
		} else if (has_prefix("star tint:", line)) {
			float r, g, b;

			field = get_field(line);
			if (sscanf(field, "%f %f %f", &r, &g, &b) != 3) {
				fprintf(stderr, "%s:line %d: bad star tint specification.\n",
					filename, ln);
				goto bad_line;
			}
			if (r < 0.0 || r > 4.0 || g < 0.0 || g > 4.0 || b < 0.0 || b > 4.0) {
				fprintf(stderr, "%s:line %d: star tint channels must be 0 to 4.\n",
					filename, ln);
				goto bad_line;
			}
			a->star_tint[0] = r;
			a->star_tint[1] = g;
			a->star_tint[2] = b;
			a->star_keys_specified++;
			continue;
		} else if (has_prefix("sun edge softness:", line)) {
			if (parse_float_field(filename, ln, line, "sun edge softness",
						0.0, 1.0, &a->sun_edge_softness))
				goto bad_line;
			a->star_keys_specified++;
			continue;
		} else if (has_prefix("star location:", line)) {
			/* On the client, this info will be overridden by info from the lobby,
			 * On the server, this info is authoritative.
			 */
			double x, y, z;
			field = get_field(line);
			rc = sscanf(field, "%lf %lf %lf", &x, &y, &z);
			if (rc == 3) {
				a->x = x;
				a->y = y;
				a->z = z;
				got_position = 1;
			}
			continue;
		} else if (has_prefix("random seed:", line)) {
			int random_seed;
			field = get_field(line);
			rc = sscanf(field, "%d", &random_seed);
			if (rc == 1)
				a->random_seed = random_seed;
			continue;
		}
bad_line:
		fprintf(stderr, "solar system asset file %s:ignoring line %d:%s\n", filename, ln, line);
		a->spec_errors++;
	}

	if (!got_position) {
		a->spec_warnings++;
		fprintf(stderr, "Solar system '%s' had no position information, using default.\n", filename);
		a->x = 0.0;
		a->y = 0.0;
		a->z = 0.0;
	}
	fclose(f);

	if (planet_textures_read <= 0) {
		fprintf(stderr, "%s: failed to read any planet types\n", filename);
		solarsystem_asset_spec_free(a);
		return NULL;
	}

	if (planet_textures_read < planet_textures_expected) {
		fprintf(stderr, "%s: expected %d planet types, but only found %d.\n",
			filename, planet_textures_expected, planet_textures_read);
		a->spec_errors++;
	}

	if (atmosphere_brightnesses_read < atmosphere_brightnesses_expected) {
		fprintf(stderr, "%s: expected %d atmosphere brightnesses, but only found %d.\n",
			filename, atmosphere_brightnesses_expected, atmosphere_brightnesses_read);
		a->spec_errors++;
	}

	if (planet_textures_read < PLANET_TYPE_COUNT_SHALL_BE) {
		int n;
		fprintf(stderr, "solar system asset file is short %d planet types, padding with duplicates\n",
			PLANET_TYPE_COUNT_SHALL_BE - planet_textures_read);
		a->spec_warnings++;

		n = PLANET_TYPE_COUNT_SHALL_BE - planet_textures_read;
		for (i = 0; i < n; i++)  {
			a->planet_texture[i + planet_textures_read] =
					strdup(a->planet_texture[i % planet_textures_read]);
			a->planet_normalmap[i + planet_textures_read] =
					strdup(a->planet_normalmap[i % planet_textures_read]);
			a->planet_type[i + planet_textures_read] =
					strdup(a->planet_type[i % planet_textures_read]);
		}
	}

	sanity_check(filename, a);

	return a;
}

void solarsystem_asset_spec_free(struct solarsystem_asset_spec *s)
{
	int i;

	if (!s)
		return;
	free_string_ptr(&s->sun_texture);
	free_string_ptr(&s->skybox_prefix);
	if (s->planet_texture) {
		for (i = 0; i < PLANET_TYPE_COUNT_SHALL_BE; i++)
			free_string_ptr(&s->planet_texture[i]);
		free(s->planet_texture);
	}
	if (s->planet_normalmap) {
		for (i = 0; i < PLANET_TYPE_COUNT_SHALL_BE; i++)
			free_string_ptr(&s->planet_normalmap[i]);
		free(s->planet_normalmap);
	}
	if (s->planet_type) {
		for (i = 0; i < PLANET_TYPE_COUNT_SHALL_BE; i++)
			free_string_ptr(&s->planet_type[i]);
		free(s->planet_type);
	}
	if (s->atmosphere_color)
		free(s->atmosphere_color);
	if (s->water_color)
		free(s->water_color);
	if (s->atmosphere_brightness)
		free(s->atmosphere_brightness);
	s->planet_texture = NULL;
	s->planet_normalmap = NULL;
	s->planet_type = NULL;
	s->atmosphere_color = NULL;
	s->water_color = NULL;
	s->atmosphere_brightness = NULL;
	free(s);
}

#ifdef SOLARSYSTEM_CONFIG_TEST

static void print_solarsystem_config(char *name, struct solarsystem_asset_spec *ss)
{
	int i;

	printf("-----------------------------\n");
	printf("Solarsystem %s:\n", name);
	printf("  Sun texture: %s\n", ss->sun_texture);
	printf("  skybox prefix: %s\n", ss->skybox_prefix);
	printf("  sun style: %s\n",
		ss->sun_style == SOLARSYSTEM_SUN_STYLE_SHADER ? "shader" : "texture");
	printf("  star temperature: %g K, tint: %g %g %g\n", ss->star_temperature,
		ss->star_tint[0], ss->star_tint[1], ss->star_tint[2]);
	printf("  star brightness: %s%g\n", ss->star_brightness_specified ? "" : "(from temperature) ",
		ss->star_brightness);
	printf("  star diameter: %g (%g px in the texture)\n",
		ss->star_diameter, ss->star_diameter_pixels);
	printf("  sun edge softness: %g\n", ss->sun_edge_softness);
	printf("  star keys specified: %d%s\n", ss->star_keys_specified,
		ss->star_keys_specified ? "" : " (every star value above is a default)");
	printf("  nplanet textures: %d\n", ss->nplanet_textures);

	for (i = 0; i < ss->nplanet_textures; i++) {
		printf("    planet_texture[%d]   : %s\n", i, ss->planet_texture[i]);
		printf("    planet_normalmap[%d] : %s\n", i, ss->planet_normalmap[i]);
		printf("    planet_type[%d] : %s\n", i, ss->planet_type[i]);
		printf("    planet  rgb[%d] : %hhu, %hhu, %hhu\n", i,
			ss->atmosphere_color[i].r, ss->atmosphere_color[i].g, ss->atmosphere_color[i].b);
	}
	printf("%d errors, %d warnings.\n", ss->spec_errors, ss->spec_warnings);
}

int main(int argc, char *argv[])
{
	struct solarsystem_asset_spec *ss;
	int i;

	for (i = 1; i < argc; i++) {
		printf("Reading solarsystem config file %s\n", argv[i]);
		ss = solarsystem_asset_spec_read(argv[i]);
		if (!ss) {
			fprintf(stderr, "Failed to read solarsystem config '%s'\n", argv[i]);
			continue;
		}
		if (ss->random_seed == -1) {
			fprintf(stderr, "Solarsystem %s has no random seed set.\n", argv[i]);
			ss->spec_warnings++;
		}
		print_solarsystem_config(argv[i], ss);
		solarsystem_asset_spec_free(ss);
		ss = NULL;
	}
	exit(0);
}

#endif
