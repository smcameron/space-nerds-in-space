#ifndef SNIS_TWEAK_H__
#define SNIS_TWEAK_H__
/*
	Copyright (C) 2018 Stephen M. Cameron
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

struct tweakable_var_descriptor {
	char *name;
	char *description;
	void *address;
	char type;
	float minf, maxf, defaultf;
	int mini, maxi, defaulti;
};

int find_tweakable_var_descriptor(struct tweakable_var_descriptor *desc, int count, char *name);

#define TWEAK_UNKNOWN_VARIABLE (-2)
#define TWEAK_OUT_OF_RANGE (-3)
#define TWEAK_BAD_SYNTAX (-4)
#define TWEAK_UNKNOWN_TYPE (-5)
int tweak_variable(struct tweakable_var_descriptor *tweak, int count, char *cmd,
			char *msg, int msgsize);
void tweakable_vars_list(struct tweakable_var_descriptor *tweak, char *regex_pattern,
				int count, void (*printfn)(const char *, ...));
int tweakable_var_describe(struct tweakable_var_descriptor *tweak, int count, char *cmd,
				void (*printfn)(const char *, ...), int suppress_unknown_var);
#endif
