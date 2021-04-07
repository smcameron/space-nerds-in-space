#ifndef HANDLE_SPELLED_NUMBERS_H__
#define HANDLE_SPELLED_NUMBERS_H__

/*
	Copyright (C) 2016 Stephen M. Cameron
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

/* These functions search the given string for sections containing
 * spelled out numbers like 'one hundred fifty thousand three hundred twelve'
 * and replace such sections with numeric equivalents, either in-place, or
 * in a new string.  So:
 *
 *    char x[] = "turn left forty five degrees and then set the throttle to ninety";
 *    printf("%s\n", handled_spelled_numbers_in_place(x));
 *
 * would print out:
 *
 *    turn left 45 degrees and then set the throttle to 90
 *
 * You do not want to use this code for anything where it is important
 * to actually get the correct answer, as it pretty hacky and it *will*
 * get a few cases wrong for sure.
 *
 */

char *handle_spelled_numbers_in_place(char *input);
char *handle_spelled_numbers_alloc(char *input);

int numbers_to_words(float number, int max_decimal_places, char *buffer, int buflen); /* convert floats to words (to 3 decimal places) */

#endif
