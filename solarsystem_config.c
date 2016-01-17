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

static void free_string_ptr(char **x)
{
	if (*x) {
		free(*x);
		*x = NULL;
	}
}

static char *get_field(char *line)
{
	char *i;

	for (i = line; *i != '\0' && *i != ':';)
		i++;
	if (*i == ':') {
		i++;
		return skip_leading_whitespace(i);
	}
	return i;
}

struct solarsystem_asset_spec *solarsystem_asset_spec_read(char *filename)
{
	FILE *f;
	struct solarsystem_asset_spec *a;
	char *field, *l, line[1000];
	int rc, ln = 0;
	int planet_textures_read = 0;

	f = fopen(filename, "r");
	if (!f) {
		fprintf(stderr, "fopen: %s: %s\n", filename, strerror(errno));
		return NULL;
	}
	a = malloc(sizeof(*a));
	memset(a, 0, sizeof(*a));

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
			if (value > 5) {
				fprintf(stderr, "%s:line %d: planet texture count %d exceeds max 5, capping\n",
						filename, ln, value);
				value = 5;
			}
			if (a->planet_texture != NULL || a->nplanet_textures != 0) {
				fprintf(stderr, "%s:line %d: multiple planet texture counts encountered, ignoring\n",
						filename, ln);
				goto bad_line;
			}
			a->nplanet_textures = value;
			a->planet_texture = malloc(sizeof(a->planet_texture[0]) * value);
			memset(a->planet_texture, 0, sizeof(a->planet_texture[0]) * value);
			a->planet_normalmap = malloc(sizeof(a->planet_normalmap[0]) * value);
			memset(a->planet_normalmap, 0, sizeof(a->planet_normalmap[0]) * value);
			a->planet_type = malloc(sizeof(a->planet_type[0]) * value);
			memset(a->planet_type, 0, sizeof(a->planet_type[0]) * value);
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
			field = get_field(line);
			rc = sscanf(field, "%s %s %s", word1, word2, word3);
			if (rc == 3) {
				a->planet_texture[planet_textures_read] = strdup(word1);
				a->planet_normalmap[planet_textures_read] = strdup(word2);
				a->planet_type[planet_textures_read] = strdup(word3);
				planet_textures_read++;
				continue;
			}
			rc = sscanf(field, "%s %s", word1, word2);
			if (rc == 2) { /* old style, no normal map */
				a->planet_texture[planet_textures_read] = strdup(word1);
				a->planet_normalmap[planet_textures_read] = strdup("no-normal-map");
				a->planet_type[planet_textures_read] = strdup(word3);
				planet_textures_read++;
				fprintf(stderr,
					"%s:line %d: expected planet texture prefix, planet normal map prefix, and planet type\n",
					filename, ln);
				fprintf(stderr,
					"%s:line %d: Assuming old style without normal map (use no-normal-map to suppress this message).\n",
					filename, ln);
				continue;
			}
			fprintf(stderr,
				"%s:line %d: expected planet texture prefix, [ planet normal map prefix ], and planet type\n",
				filename, ln);
			goto bad_line;
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
		}
bad_line:
		fprintf(stderr, "solar system asset file %s:ignoring line %d:%s\n", filename, ln, line);
	}
	fclose(f);
	return a;
}

void solarsystem_asset_spec_free(struct solarsystem_asset_spec *s)
{
	int i;

	if (!s)
		return;
	free_string_ptr(&s->directory);
	free_string_ptr(&s->sun_texture);
	free_string_ptr(&s->skybox_prefix);
	for (i = 0; i < s->nplanet_textures; i++) {
		free_string_ptr(&s->planet_texture[i]);
		free_string_ptr(&s->planet_type[i]);
	}
	free(s);
}

