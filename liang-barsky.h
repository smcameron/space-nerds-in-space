#ifndef LIANG_BARSKY_H__
#define LIANG_BARSKY_H__
/*
        Copyright (C) 2013 Hin Jang, Stephen M. Cameron

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

/* Liang-Barsky clipping algorithm.
 * Adapted from Hin Jang's C++ implementation found here:
 * http://hinjang.com/articles/04.html#eight
 */

#ifdef DEFINE_LIANG_BARSKY_GLOBALS
#define GLOBAL
#else
#define GLOBAL extern
#endif

GLOBAL int set_clip_window(int x1, int y1, int x2, int y2);

/* clip_line()
 * modifies parameters in place to clip the line,
 * returns 0 if line is totally outside clip window
 * returns 1 if line is not totally outside clip window
 */
GLOBAL int clip_line(int *x1, int *y1, int *x2,  int *y2);

/* clip_line_copy()
 * first four params are input line coords
 * last four params are clipped output line coords
 * returns 0 if line is totally outside clip window
 * returns 1 if line is not totally outside clip window
 */
GLOBAL int clip_line_copy(int x1, int y1, int x2, int y2,
			int *ox1, int *oy1, int *ox2, int *oy2);

#endif
