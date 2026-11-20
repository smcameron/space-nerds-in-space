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
#include "build_bug_on.h"

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
	char difficulty[11], decimation_str[32];
	char *s;
	int total_len, rc, lineno, np, pc, decimation_factor;
	const int max_starbase_models = 100;

	printf("Reading starbase model specifications...");
	*nstarbase_models = -1;
	fflush(stdout);
	snprintf(path, sizeof(path), "%s/%s", asset_dir, filename);
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
			if (np < 1) {
				fprintf(stderr, "Need at least one starbase model\n");
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
		BUILD_ASSERT(PATH_MAX == 4096);
		decimation_factor = DEFAULT_STARBASE_DECIMATION_FACTOR;
		rc = sscanf(s, "%4095s %4095s %10s %31s", model_file, docking_port_file,
			difficulty, decimation_str);
		if (rc == 4) {
			char extra;

			if (sscanf(decimation_str, "%d%c", &decimation_factor, &extra) != 1 ||
			    decimation_factor < 1) {
				fprintf(stderr, "%s:%d: Bad decimation factor: '%s', assuming %d\n",
						path, lineno, decimation_str,
						DEFAULT_STARBASE_DECIMATION_FACTOR);
				decimation_factor = DEFAULT_STARBASE_DECIMATION_FACTOR;
			}
		} else if (rc != 3) {
			/* Maybe it's an old file without the difficulty parameter */
			rc = sscanf(s, "%4095s %4095s", model_file, docking_port_file);
			if (rc != 2) {
				fprintf(stderr, "%s:%d bad starbase model specification\n",
						path, lineno);
				goto bailout;
			}
			strlcpy(difficulty, "HARD", sizeof(difficulty));
		}
		if (pc >= np) {
			fprintf(stderr, "More starbase models than declared, max = %d, declared = %d\n",
				max_starbase_models, np);
			goto bailout;
		}
		(*starbase_metadata)[pc].model_file = strdup(model_file);
		(*starbase_metadata)[pc].decimation_factor = decimation_factor;
		if (strncasecmp(difficulty, "EASY", 4) == 0)
			(*starbase_metadata)[pc].difficulty = DOCKING_PORT_EASY;
		else if (strncasecmp(difficulty, "HARD", 4) == 0)
			(*starbase_metadata)[pc].difficulty = DOCKING_PORT_HARD;
		else {
			fprintf(stderr, "%s:%d: Bad docking difficulty: '%s', assuming 'HARD'\n",
					path, lineno, difficulty);
			(*starbase_metadata)[pc].difficulty = DOCKING_PORT_HARD;
		}

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
	if (pc != np) {
		fprintf(stderr, "Number of starbase models does not match number declared: %d vs %d\n", pc, np);
		goto bailout;
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

#ifdef TEST_STARBASE_METADATA
#include <libgen.h>
#include <unistd.h>

struct docking_port_attachment_point *read_docking_port_attachments(
			__attribute__((unused)) char *filename,
			__attribute__((unused)) float scaling_factor)
{
	return NULL;
}

static void free_starbase_metadata(struct starbase_file_metadata *m, int n)
{
	int i;

	if (!m)
		return;
	for (i = 0; i < n; i++) {
		free(m[i].model_file);
		free(m[i].docking_port_file);
	}
	free(m);
}

int main(void)
{
	struct starbase_file_metadata *m = NULL;
	int n = 0;
	int i;

	if (read_starbase_model_metadata("share/snis", "starbase_models.txt", &n, &m) != 0) {
		fprintf(stderr, "Failed to read share/snis/starbase_models.txt\n");
		return 1;
	}
	if (n != 6) {
		fprintf(stderr, "Expected 6 starbase models, got %d\n", n);
		free_starbase_metadata(m, n);
		return 1;
	}
	for (i = 0; i < n; i++) {
		if (m[i].decimation_factor < 1) {
			fprintf(stderr, "Model %d: invalid decimation_factor=%d\n",
				i, m[i].decimation_factor);
			free_starbase_metadata(m, n);
			return 1;
		}
	}
	free_starbase_metadata(m, n);
	m = NULL;

	/* Verify custom, default, 2-column, and invalid decimation_factor handling */
	{
		char tmpname[] = "/tmp/test_starbase_models_XXXXXX";
		int fd = mkstemp(tmpname);
		FILE *tf;
		char *dir;
		char *base;
		char tmpcopy[PATH_MAX];

		if (fd < 0) {
			perror("mkstemp");
			return 1;
		}
		tf = fdopen(fd, "w");
		if (!tf) {
			perror("fdopen");
			close(fd);
			unlink(tmpname);
			return 1;
		}
		fprintf(tf,
			"starbase model count: 5\n"
			"sb1.stl - EASY 4\n"
			"sb2.stl - HARD 16\n"
			"sb3.stl - EASY\n"
			"sb4.stl -\n"
			"sb5.stl - HARD badval\n");
		fclose(tf);

		strlcpy(tmpcopy, tmpname, sizeof(tmpcopy));
		dir = dirname(tmpcopy);
		base = basename(tmpname);
		if (read_starbase_model_metadata(dir, base, &n, &m) != 0 || n != 5) {
			fprintf(stderr, "Failed to read temp starbase models (n=%d)\n", n);
			unlink(tmpname);
			return 1;
		}
		unlink(tmpname);

		if (m[0].difficulty != DOCKING_PORT_EASY || m[0].decimation_factor != 4) {
			fprintf(stderr, "sb1 mismatch: diff=%d dec=%d\n",
				m[0].difficulty, m[0].decimation_factor);
			free_starbase_metadata(m, n);
			return 1;
		}
		if (m[1].difficulty != DOCKING_PORT_HARD || m[1].decimation_factor != 16) {
			fprintf(stderr, "sb2 mismatch: diff=%d dec=%d\n",
				m[1].difficulty, m[1].decimation_factor);
			free_starbase_metadata(m, n);
			return 1;
		}
		if (m[2].difficulty != DOCKING_PORT_EASY ||
		    m[2].decimation_factor != DEFAULT_STARBASE_DECIMATION_FACTOR) {
			fprintf(stderr, "sb3 mismatch: diff=%d dec=%d\n",
				m[2].difficulty, m[2].decimation_factor);
			free_starbase_metadata(m, n);
			return 1;
		}
		if (m[3].difficulty != DOCKING_PORT_HARD ||
		    m[3].decimation_factor != DEFAULT_STARBASE_DECIMATION_FACTOR) {
			fprintf(stderr, "sb4 mismatch: diff=%d dec=%d\n",
				m[3].difficulty, m[3].decimation_factor);
			free_starbase_metadata(m, n);
			return 1;
		}
		if (m[4].difficulty != DOCKING_PORT_HARD ||
		    m[4].decimation_factor != DEFAULT_STARBASE_DECIMATION_FACTOR) {
			fprintf(stderr, "sb5 mismatch: diff=%d dec=%d\n",
				m[4].difficulty, m[4].decimation_factor);
			free_starbase_metadata(m, n);
			return 1;
		}
		free_starbase_metadata(m, n);
	}
	return 0;
}
#endif
