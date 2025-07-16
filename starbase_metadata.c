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

#include "docking_port.h"
#include "starbase_metadata.h"
#include "string-utils.h"
#include "arraysize.h"

static char *starbase_model_names[] = {
	"S-CLASS SPACE STATION",
	"SPACE CASTLE",
	"SPACE HABITAT",
	"SPACE BUNGALOW",
	"COSMOTEL",
};

int read_starbase_model_metadata(char *asset_dir, char *filename, int *nstarbase_models,
		struct starbase_file_metadata **starbase_metadata)
{
	FILE *f;
	char path[PATH_MAX], model_file[PATH_MAX], docking_port_file[PATH_MAX], line[256];
	char *s;
	int total_len, rc, lineno, np, pc;
	const int max_starbase_models = 100;

	printf("Reading starbase model specifications...");
	*nstarbase_models = -1;
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
			*starbase_metadata = malloc(sizeof(**starbase_metadata) * np);
			memset(*starbase_metadata, 0,
				sizeof(**starbase_metadata) * np);
			continue;
		}
		rc = sscanf(s, "%s %s", model_file, docking_port_file);
		if (rc != 2) {
			fprintf(stderr, "%s:%d bad starbase model specification\n",
					path, lineno);
			goto bailout;
		}
		(*starbase_metadata)[pc].model_file = strdup(model_file);

		if (strcmp(docking_port_file, "!") == 0) {
			(*starbase_metadata)[pc].docking_port_file = NULL;
				/* docking port file deliberately omitted */
			pc++;
			continue;
		}
		if (strcmp(docking_port_file, "-") == 0) {
			/* Default location, construct the path to *.docking_ports.h file
			 * From the model file.
			 */
			total_len = strlen(asset_dir) + strlen(model_file) +
				strlen("docking_ports.h") + strlen("/models/") + 2;
			if (total_len >= PATH_MAX) {
				fprintf(stderr, "path '%s' is too long.\n", model_file);
				(*starbase_metadata)[pc].docking_port_file = NULL;
				goto bailout;
			}
			snprintf(path, PATH_MAX, "%s/models/%s", asset_dir, model_file);
			int i = strlen(path);
			while (i >= 0 && path[i] != '.') {
				path[i] = '\0';
				i--;
			}
			if (i < 0) {
				fprintf(stderr, "Bad path model path %s\n", model_file);
				(*starbase_metadata)[pc].docking_port_file = NULL;
				goto bailout;
			}
			strcat(path, "docking_ports.h");
		} else {
			/* Specialized location of docking ports file */
			total_len = strlen(asset_dir) + strlen(model_file) +
				strlen(docking_port_file) + strlen("/models/") + 10;
			if (total_len >= PATH_MAX) {
				fprintf(stderr, "path '%s/models/%s' is too long.\n",
						asset_dir, docking_port_file);
				(*starbase_metadata)[pc].docking_port_file = NULL;
				goto bailout;
			}
			snprintf(path, PATH_MAX, "%s/models/%s", asset_dir, model_file);
			int i = strlen(path);
			while (i >= 0 && path[i] != '/') {
				path[i] = '\0';
				i--;
			}
			if (i < 0) {
				fprintf(stderr, "Bad path model path %s\n", model_file);
				(*starbase_metadata)[pc].docking_port_file = NULL;
				goto bailout;
			}
			strcat(path, docking_port_file);
		}
		(*starbase_metadata)[pc++].docking_port_file = strdup(path);
		if (pc > max_starbase_models)
			break;
	}
	fclose(f);
	printf("done\n");
	fflush(stdout);
	return 0;
bailout:
	fclose(f);
	if (*starbase_metadata) {
		for (int i = 0; i < pc; i++) {
			if ((*starbase_metadata)[i].model_file) {
				free((*starbase_metadata)[i].model_file);
				(*starbase_metadata)[i].model_file = NULL;
			}
			if ((*starbase_metadata)[i].docking_port_file) {
				free((*starbase_metadata)[i].docking_port_file);
				(*starbase_metadata)[i].docking_port_file = NULL;
			}
		}
		free(*starbase_metadata);
		*starbase_metadata = NULL;
	}
	return -1;
}

struct docking_port_attachment_point **read_docking_port_info(
		struct starbase_file_metadata starbase_metadata[], int n,
		float starbase_scale_factor)
{
	int i;
	struct docking_port_attachment_point **d = malloc(sizeof(*d) * n);
	memset(d, 0, sizeof(*d) * n);
	for (i = 0; i < n; i++) {
		if (!starbase_metadata[i].docking_port_file)
			continue;
		d[i] = read_docking_port_attachments(starbase_metadata[i].docking_port_file,
				starbase_scale_factor);
	}
	return d;
}

char *starbase_model_name(int id)
{
	if (id < 0)
		return NULL;
	return starbase_model_names[id % ARRAYSIZE(starbase_model_names)];
}
