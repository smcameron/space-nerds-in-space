/*
	Copyright (C) 2015 Stephen M. Cameron
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
#include <stdlib.h>

#define DEFINE_UI_COLORS_GLOBALS 1
#include "ui_colors.h"
#undef DEFINE_UI_COLORS_GLOBALS

#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))

/* Some color indices are not decided until runtime.  This is a hack
 * to fixup such indices at runtime.
 */
void fixup_ui_color(int old_color, int new_color)
{
	int i;
	int count = 0;

	fprintf(stderr, "Fixing up %d to %d\n", old_color, new_color);
	/* Kind of hacky, we just tromp through memory clobbering ints on the
	 * presumption that a struct full of ints will be virtually contiguous
	 */
	for (i = 0; i < ARRAY_SIZE(ui_color.u.entry); i++) {
		struct ui_color_entry *e = &ui_color.u.entry[i];
		if (e->index == old_color) {
			fprintf(stderr, "fixed up %s: %d -> %d\n", e->name, old_color, new_color);
			e->index = new_color;
			count++;
		}
	}
	fprintf(stderr, "fixed up %d instances of %d to %d\n", count, old_color, new_color);
}

