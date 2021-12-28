#ifndef VERTEX_H__
#define VERTEX_H__
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
struct vertex {
	float x, y, z, w;
	float wx, wy, wz, ww; /* world coords */
	int clip;
};

#define VERTEX_INIT(x, y, z, w) { (x), (y), (z), (w), 0, 0, 0, 0, 0 }

#endif
