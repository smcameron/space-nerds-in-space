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

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>

#include "snis_asset_dir.h"

static char snis_bin_dir[PATH_MAX] = { 0 };

char *get_snis_bin_dir(void)
{
	char *envbindir;

	if (snis_bin_dir[0] != '\0')
		return snis_bin_dir;

	envbindir = getenv("SNISBINDIR");
	if (envbindir)
		snprintf(snis_bin_dir, sizeof(snis_bin_dir) - 1, "%s/bin", envbindir);
	else
		snprintf(snis_bin_dir, sizeof(snis_bin_dir) - 1, "%s%s/bin", STRPREFIX(DESTDIR), STRPREFIX(PREFIX));

	return snis_bin_dir;
}

