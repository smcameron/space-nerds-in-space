#ifndef SNIS_BIN_DIR_H__
#define SNIS_BIN_DIR_H__
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

/* Returns the binary location for SNIS executables.  Returns either the environment
 * variable ${SNISBINDIR}/bin, or if SNISBINDIR isn't set, returns ${DESTIR}${PREFIX}/bin where
 * DESTDIR and PREFIX are defined by the Makefile (See also: snis_asset_dir.h).
 */
char *get_snis_bin_dir(void);

#endif
