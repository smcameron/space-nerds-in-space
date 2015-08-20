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
#include "bline.h"

/*
 * Bresenham's line drawing algorithm.
 */
void bline(int x1, int y1, int x2, int y2, plotting_function plot_func, void *context)
{
	int dx, dy, i, e;
	int incx, incy, inc1, inc2;

	dx = x2 - x1;
	if (dx < 0)
		dx = -dx;

	dy = y2 - y1;
	if (dy < 0)
		dy = -dy;

	incx = (x2 < x1) ? -1 : 1;
	incy = (y2 < y1) ? -1 : 1;

	if (dx > dy) {
		plot_func(x1, y1, context);
		e = 2 * dy - dx;
		inc1 = 2 * (dy - dx);
		inc2 = 2 * dy;
		for (i = 0; i < dx; i++) {
			if (e >= 0) {
				y1 += incy;
				e += inc1;
			} else {
				e += inc2;
			}
			x1 += incx;
			plot_func(x1, y1, context);
		}
	} else {
		plot_func(x1, y1, context);
		e = 2 * dx - dy;
		inc1 = 2 * (dx - dy);
		inc2 = 2 * dx;
		for (i = 0; i < dy; i++) {
			if (e >= 0) {
				x1 += incx;
				e += inc1;
			} else {
				e += inc2;
			}
			y1 += incy;
			plot_func(x1, y1, context);
		}
	}
}
