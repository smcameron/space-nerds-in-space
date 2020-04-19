#ifndef WWVI_FONT_H
#define WWVI_FONT_H
/* 
    (C) Copyright 2010, Stephen M. Cameron.

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

#include "my_point.h"

#ifdef SNIS_FONT_DEFINE_GLOBALS
#define GLOBAL
#else 
#define GLOBAL extern
#endif

struct font_def;
extern struct font_def *ascii_font;
extern struct font_def *alien_font;
extern struct font_def *ascii_smallcaps_font;

GLOBAL int snis_make_font(struct my_vect_obj ***font, struct font_def *f, float xscale, float yscale);
GLOBAL int snis_font_lineheight(float yscale);

#undef GLOBAL
#endif
