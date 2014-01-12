/*
	Copyright (C) 2010 Jeremy Van Grinsven
	Author: Jeremy Van Grinsven

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
#include <gtk/gtk.h>

#include "quat.h"
#include "snis_graph.h"
#include "material.h"
#include "graph_dev.h"

static unsigned int load_texture(const char *asset_dir, char *filename)
{
	char fname[PATH_MAX + 1];

	sprintf(fname, "%s/textures/%s", asset_dir, filename);
	return graph_dev_load_texture(fname);
}

int material_nebula_read_from_file(const char *asset_dir, const char *filename, struct material_nebula *mt)
{
	FILE *f;
	int rc;
	int i;
	char full_filename[PATH_MAX + 1];

	sprintf(full_filename, "%s/materials/%s", asset_dir, filename);

	f = fopen(full_filename, "r");
	if (!f) {
		fprintf(stderr, "material_nebula: Error opening file '%s'\n", full_filename);
		return -1;
	}

	for (i = 0; i < MATERIAL_NEBULA_NPLANES; i++) {
		char texture_filename[PATH_MAX+1];
		rc = fscanf(f, "texture %s\n", texture_filename);
		if (rc != 1) {
			fprintf(stderr, "material_nebula: Error reading 'texture' from file '%s'\n", full_filename);
			return -1;
		}
		mt->texture_id[i] = load_texture(asset_dir, texture_filename);

		rc = fscanf(f, "orientation %f %f %f %f\n", &mt->orientation[i].q.q0, &mt->orientation[i].q.q1,
			&mt->orientation[i].q.q2, &mt->orientation[i].q.q3);
		if (rc != 4) {
			fprintf(stderr, "material_nebula: Error reading 'orientation' from file '%s'\n", full_filename);
			return -1;
		}
	}

	rc = fscanf(f, "alpha %f\n", &mt->alpha);
	if (rc != 1) {
		fprintf(stderr, "material_nebula: Error reading 'alpha' from file '%s'\n", full_filename);
		return -1;
	}

	rc = fscanf(f, "tint %f %f %f\n", &mt->tint.red, &mt->tint.green, &mt->tint.blue);
	if (rc != 3) {
		fprintf(stderr, "material_nebula: Error reading 'tint' from file '%s'\n", full_filename);
		return -1;
	}

	fclose(f);

	return 0;
}

