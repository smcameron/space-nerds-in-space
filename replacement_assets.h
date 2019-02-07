#ifndef REPLACED_ASSETS_H__
#define REPLACED_ASSETS_H__
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

struct replacement_asset_entry {
	char *old_filename;
	char *new_filename;
};

struct replacement_asset {
	struct replacement_asset_entry *e;
	char *asset_dir;
};

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
int replacement_asset_read(char *replacement_list_filename, char *asset_dir,
				struct replacement_asset *replacement_asset);

/* Looks up original in replacement_assets and if found, returns the corresponding replacement
 * asset if found, otherwise it returns the original.
 */
char *replacement_asset_lookup(char *original, struct replacement_asset *replacement_assets);

void replacement_asset_free(struct replacement_asset *replacement_assets);

#endif
