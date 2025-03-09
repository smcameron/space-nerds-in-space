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

/* Underlined characters */
#define space_ "\xa0"
#define exclamation_ "\xa1"
#define double_quote_ "\xa2"
#define pound_ "\xa3"
#define dollar_ "\xa4"
#define percent_ "\xa5"
#define ampersand_ "\xa6"
#define single_quote_ "\xa7"
#define open_paren_ "\xa8"
#define close_paren_ "\xa9"
#define asterisk_ "\xaa"
#define plus_ "\xab"
#define comma_ "\xac"
#define dash_ "\xad"
#define period_ "\xae"
#define slash_ "\xaf"
#define zero_ "\xb0"
#define one_ "\xb1"
#define two_ "\xb2"
#define three_ "\xb3"
#define four_ "\xb4"
#define five_ "\xb5"
#define six_ "\xb6"
#define seven_ "\xb7"
#define eight_ "\xb8"
#define nine_ "\xb9"
#define colon_ "\xba"
#define semicolon_ "\xbb"
#define lessthan_ "\xbc"
#define equals_ "\xbd"
#define greaterthan_ "\xbe"
#define question_ "\xbf"
#define at_ "\xc0"
#define A_ "\xc1"
#define B_ "\xc2"
#define C_ "\xc3"
#define D_ "\xc4"
#define E_ "\xc5"
#define F_ "\xc6"
#define G_ "\xc7"
#define H_ "\xc8"
#define I_ "\xc9"
#define J_ "\xca"
#define K_ "\xcb"
#define L_ "\xcc"
#define M_ "\xcd"
#define N_ "\xce"
#define O_ "\xcf"
#define P_ "\xd0"
#define Q_ "\xd1"
#define R_ "\xd2"
#define S_ "\xd3"
#define T_ "\xd4"
#define U_ "\xd5"
#define V_ "\xd6"
#define W_ "\xd7"
#define X_ "\xd8"
#define Y_ "\xd9"
#define Z_ "\xda"
#define open_bracket_ "\xdb"
#define backslash_ "\xdc"
#define close_bracket_ "\xdd"
#define circumflex_ "\xde"
#define underscore_ "\xdf"
#define backquote_ "\xe0"
#define a_ "\xe1"
#define b_ "\xe2"
#define c_ "\xe3"
#define d_ "\xe4"
#define e_ "\xe5"
#define f_ "\xe6"
#define g_ "\xe7"
#define h_ "\xe8"
#define i_ "\xe9"
#define j_ "\xea"
#define k_ "\xeb"
#define l_ "\xec"
#define m_ "\xed"
#define n_ "\xee"
#define o_ "\xef"
#define p_ "\xf0"
#define q_ "\xf1"
#define r_ "\xf2"
#define s_ "\xf3"
#define t_ "\xf4"
#define u_ "\xf5"
#define v_ "\xf6"
#define w_ "\xf7"
#define x_ "\xf8"
#define y_ "\xf9"
#define z_ "\xfa"
#define open_curly_ "\xfb"
#define pipe_ "\xfc"
#define close_curly_ "\xfd"
#define tilde_ "\xfe"

#undef GLOBAL
#endif
