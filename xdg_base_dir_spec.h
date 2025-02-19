#ifndef XDG_BASE_DIR_SPEC_H_
#define XDG_BASE_DIR_SPEC_H_
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
#include "stdio.h"

struct xdg_base_context;

struct xdg_base_context *xdg_base_context_new(char *appname, char *default_path);
void xdg_base_context_free(struct xdg_base_context *cx);
char *xdg_base_data_filename(struct xdg_base_context *cx, char *basename, char *filename, size_t filenamelen);
char *xdg_base_slurp_file(struct xdg_base_context *cx, char *filename);
FILE *xdg_base_fopen_for_write(struct xdg_base_context *cx, char *filename);
FILE *xdg_base_fopen_for_read(struct xdg_base_context *cx, char *filename);
int xdg_base_open_for_overwrite(struct xdg_base_context *cx, char *filename);

#endif
