#ifndef MY_POINT_H
#define MY_POINT_H
/* 
    (C) Copyright 2007,2008,2010 Stephen M. Cameron.

    This file is part of Space Nerds In Space.

    Space Nerds In Space is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    Space Nerds In Space is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Space Nerds In Space; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

 */

/* special values to do with drawing shapes. */

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(x) (sizeof((x)) / sizeof((x)[0]))
#endif

#define LINE_BREAK (-9999)
#define COLOR_CHANGE (-9998) /* note, color change can ONLY follow LINE_BREAK */

struct my_point_t {
	short x,y;
};

/* Just a grouping of arrays of points with the number of points in the array */
struct my_vect_obj {
	int npoints;
	struct my_point_t *p;
};

#define setup_vect(v, a) { v.p = a; v.npoints = ARRAY_SIZE(a); } 

#endif
