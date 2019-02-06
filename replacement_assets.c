/*
	Copyright (C) 2019 Stephen M. Cameron
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
#include <string.h>
#include <stdlib.h>

#include "replacement_assets.h"

#if 0
struct replacement_asset {
	char *old_filename;
	char *new_filename;
};
#endif

/* read_replacement_assets() reads the specified file of asset replacements.
 * Each line in the file is a pair of filenames.  The first filename is the
 * original file, and the 2nd filename is the replacement.  Blank lines and
 * comment lines (beginning with '#') are permitted.
 *
 * Returns -1 on error, or the number of asset replacement records allocated.
 * The 2nd parameter, *replacement_asset will point to a newly allocated array
 * containing the replacement asset records, plus one extra sentinel asset
 * with old_filename and new_filename both NULL.
 */
int replacement_asset_read(char *replacement_list_filename, struct replacement_asset **replacement_asset)
{
	char line[1000], *l;
	char f1[sizeof(line)], f2[sizeof(line)];
	FILE *f;
	int ln = 0;
	int n = 0;
	int rc;
	struct replacement_asset *ra = NULL;

	f = fopen(replacement_list_filename, "r");
	if (!f)
		return -1;

	while (!feof(f)) {
		l = fgets(line, sizeof(line), f);
		if (!l)
			break;
		ln++;
		if (strlen(line) == 0)
			continue;
		line[strlen(line) - 1] = '\0';
		if (line[0] == '#')
			continue;
		memset(f1, 0, sizeof(f1));
		memset(f2, 0, sizeof(f2));
		rc = sscanf(line, "%s%*[	 ]%s", f1, f2);
		if (rc != 2) {
			fprintf(stderr, "%s: syntax error at line %d\n", replacement_list_filename, ln);
			continue;
		}
		ra = realloc(ra, sizeof(*ra) * (n + 1));
		ra[n].old_filename = strdup(f1);
		ra[n].new_filename = strdup(f2);
		n++;
	}
	ra = realloc(ra, sizeof(*ra) * (n + 1));
	ra[n].old_filename = NULL;
	ra[n].new_filename = NULL;
	n++;
	*replacement_asset = ra;
	return n;
}

/* Looks up original in replacement_assets and if found, returns the corresponding replacement
 * asset if found, otherwise it returns the original.
 */
char *replacement_asset_lookup(char *original, struct replacement_asset *replacement_assets)
{
	struct replacement_asset *r = replacement_assets;

	while (r) {
		if (!r->old_filename)
			break;
		if (strcmp(r->old_filename, original) == 0)
			return r->new_filename;
		r++;
	}
	return original;
}

void replacement_asset_free(struct replacement_asset *replacement_assets)
{
	struct replacement_asset *r = replacement_assets;

	while (r) {
		if (r->new_filename)
			free(r->new_filename);
		else
			break;
		if (r->old_filename)
			free(r->old_filename);
		r++;
	}
	free(replacement_assets);
}
