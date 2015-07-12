/*
	Copyright (C) 2010 Stephen M. Cameron
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
#include <errno.h>
#include <stdlib.h>
#include <limits.h>

#include "starbase_metadata.h"
#include "string-utils.h"

int read_starbase_model_metadata(char *asset_dir, char *filename, int *nstarbase_models,
		char ***starbase_model_filename)
{
	FILE *f;
	char path[PATH_MAX], fname[PATH_MAX], line[256];
	char *s;
	int rc, lineno, np, pc;
	const int max_starbase_models = 100;

	printf("Reading starbase model specifications...");
	fflush(stdout);
	sprintf(path, "%s/%s", asset_dir, filename);
	f = fopen(path, "r");
	if (!f) {
		fprintf(stderr, "open: %s: %s\n", path, strerror(errno));
		return -1;
	}

	np = -1;
	pc = 0;
	lineno = 0;
	while (!feof(f)) {
		s = fgets(line, 256, f);
		if (!s)
			break;
		s = trim_whitespace(s);
		lineno++;
		if (strcmp(s, "") == 0)
			continue;
		if (s[0] == '#') /* comment? */
			continue;
		rc = sscanf(s, "starbase model count: %d", &np);
		if (rc == 1) {
			if (*nstarbase_models != -1) {
				fprintf(stderr, "%s:%d: duplicate starbase model count\n",
					path, lineno);
				goto bailout;
			}
			if (np > max_starbase_models) {
				fprintf(stderr,
					"Too many starbase models (%d), capping to %d\n",
					np, max_starbase_models);
				np = max_starbase_models;
			}
			*nstarbase_models = np;
			*starbase_model_filename = malloc(sizeof((*starbase_model_filename)[0]) * np);
			memset(*starbase_model_filename, 0,
				sizeof((*starbase_model_filename)[0]) * np);
			continue;
		}
		rc = sscanf(s, "%s", fname);
		if (rc != 1) {
			fprintf(stderr, "%s:%d bad starbase model specification\n",
					path, lineno);
			goto bailout;
		}
		(*starbase_model_filename)[pc++] = strdup(fname);
		if (pc > max_starbase_models)
			break;
	}
	fclose(f);
	printf("done\n");
	fflush(stdout);
	return 0;
bailout:
	fclose(f);
	if (*starbase_model_filename) {
		int i;
		for (i = 0; i < pc; i++) {
			if ((*starbase_model_filename)[i]) {
				free((*starbase_model_filename)[i]);
				(*starbase_model_filename)[i] = NULL;
			}
		}
		free(*starbase_model_filename);
		*starbase_model_filename = NULL;
	}
	return -1;
}

