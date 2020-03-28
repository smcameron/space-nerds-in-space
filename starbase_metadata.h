#ifndef STARBASE_METADATA_H_
#define STARBASE_METADATA_H_
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

#include "docking_port.h"

struct starbase_file_metadata {
	char *model_file;
	char *docking_port_file;
};

int read_starbase_model_metadata(char *asset_dir, char *filename, int *nstarbase_models,
			struct starbase_file_metadata **starbase_metadata);

struct docking_port_attachment_point **read_docking_port_info(
		struct starbase_file_metadata starbase_metadata[], int n,
		float starbase_scale_factor);

/* This returns the model name like "mustang" is the model name
 * of the car Ford makes. It does not return the name of the file
 * containing the mesh data.
 */
char *starbase_model_name(int id);

#endif

