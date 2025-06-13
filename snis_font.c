/*
	(C) Copyright 2007,2008, Stephen M. Cameron.

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

#include <stdlib.h>
#include <string.h>

#define SNIS_FONT_DEFINE_GLOBALS
#include "snis_font.h"
#undef  SNIS_FONT_DEFINE_GLOBALS

typedef unsigned char stroke_t;

#define LP (36)

/**************************

	Here's how this works, there's a 5x7 grid. on which the
	letters are drawn, as below.

		0   1   2   3   4

                5   6   7   8   9

               10  11  12  13  14

               15  16  17  18  19

               20  21  22  23  24

               25  26  27  28  29

               30  31  32  33  34

	To represent a glyph list a sequence of strokes between grid points.
	Use LP to lift the pen, 99 to mark the end.

	The letter 'A' can be compactly represented by: { 20, 5, 1, 2, 8, 23, LP, 10, 13, 99 },

	The grid numbers can be decoded into (x,y) coords like:

	x = ((n % 5) * xscale);
	y = (x/5) * yscale;      truncating division.

	use decode_glyph[] to get x and y values -- see below

	Inspired by "Fluid Concepts and Creative Analogies", p. 420-421,
	chapter 10, "Letter Spirit: Esthetic Perception and Creative Play
	in the Rich Microcosm of the Roman Alphabet", by Douglas Hofstadter
	and Gary McGraw

***************************/

struct font_def {
	stroke_t *glyph_open_curly;
	stroke_t *glyph_close_curly;
	stroke_t *glyph_Z;
	stroke_t *glyph_Y;
	stroke_t *glyph_X;
	stroke_t *glyph_W;
	stroke_t *glyph_V;
	stroke_t *glyph_U;
	stroke_t *glyph_T;
	stroke_t *glyph_S;
	stroke_t *glyph_R;
	stroke_t *glyph_Q;
	stroke_t *glyph_P;
	stroke_t *glyph_O;
	stroke_t *glyph_N;
	stroke_t *glyph_M;
	stroke_t *glyph_L;
	stroke_t *glyph_K;
	stroke_t *glyph_J;
	stroke_t *glyph_I;
	stroke_t *glyph_H;
	stroke_t *glyph_G;
	stroke_t *glyph_F;
	stroke_t *glyph_E;
	stroke_t *glyph_D;
	stroke_t *glyph_C;
	stroke_t *glyph_B;
	stroke_t *glyph_A;
	stroke_t *glyph_slash;
	stroke_t *glyph_backslash;
	stroke_t *glyph_pipe;
	stroke_t *glyph_que;
	stroke_t *glyph_bang;
	stroke_t *glyph_colon;
	stroke_t *glyph_leftparen;
	stroke_t *glyph_rightparen;
	stroke_t *glyph_space;
	stroke_t *glyph_plus;
	stroke_t *glyph_dash;
	stroke_t *glyph_0;
	stroke_t *glyph_9;
	stroke_t *glyph_8;
	stroke_t *glyph_7;
	stroke_t *glyph_6;
	stroke_t *glyph_5;
	stroke_t *glyph_4;
	stroke_t *glyph_3;
	stroke_t *glyph_2;
	stroke_t *glyph_1;
	stroke_t *glyph_comma;
	stroke_t *glyph_period;
	stroke_t *glyph_z;
	stroke_t *glyph_y;
	stroke_t *glyph_x;
	stroke_t *glyph_w;
	stroke_t *glyph_v;
	stroke_t *glyph_u;
	stroke_t *glyph_t;
	stroke_t *glyph_s;
	stroke_t *glyph_a;
	stroke_t *glyph_b;
	stroke_t *glyph_c;
	stroke_t *glyph_d;
	stroke_t *glyph_e;
	stroke_t *glyph_f;
	stroke_t *glyph_g;
	stroke_t *glyph_h;
	stroke_t *glyph_i;
	stroke_t *glyph_j;
	stroke_t *glyph_k;
	stroke_t *glyph_l;
	stroke_t *glyph_m;
	stroke_t *glyph_n;
	stroke_t *glyph_o;
	stroke_t *glyph_p;
	stroke_t *glyph_q;
	stroke_t *glyph_r;
	stroke_t *glyph_apostrophe;
	stroke_t *glyph_doublequote;
	stroke_t *glyph_asterisk;
	stroke_t *glyph_underscore;
	stroke_t *glyph_hash;
	stroke_t *glyph_dollar;
	stroke_t *glyph_percent;
	stroke_t *glyph_caret;
	stroke_t *glyph_ampersand;
	stroke_t *glyph_at;
	stroke_t *glyph_lessthan;
	stroke_t *glyph_greaterthan;
	stroke_t *glyph_rightbracket;
	stroke_t *glyph_leftbracket;
	stroke_t *glyph_semicolon;
	stroke_t *glyph_tilde;
	stroke_t *glyph_equals;
};



static struct font_def ascii_font_def = {
	.glyph_open_curly = (stroke_t []) { 2, 6, 11, 15, 21, 26, 32, 99 },
	.glyph_close_curly = (stroke_t []) { 2, 8, 13, 19, 23, 28, 32, 99 },
	.glyph_Z = (stroke_t []) { 0, 4, 20, 24, 99 },
	.glyph_Y = (stroke_t []) { 0, 12, 22, LP, 12, 4, 99 },
	.glyph_X = (stroke_t []) { 0, 23, LP, 20, 3, 99 },
	.glyph_W = (stroke_t []) { 0, 21, 7, 23, 4, 99 },
	.glyph_V = (stroke_t []) { 0, 22, 4, 99 },
	.glyph_U = (stroke_t []) { 0, 15, 21, 23, 19, 4, 99 },
	.glyph_T = (stroke_t []) { 0, 4, LP, 2, 22, 99 },
	.glyph_S = (stroke_t []) { 9, 3, 1, 5, 11, 13, 19, 23, 21, 15, 99 },
	.glyph_R = (stroke_t []) { 20, 0, 3, 9, 13, 10, LP, 13, 24, 99 },
	.glyph_Q = (stroke_t []) { 1, 3, 9, 19, 23, 21, 15, 5, 1, LP, 18, 24, 99 },
	.glyph_P = (stroke_t []) { 20, 0, 3, 9, 13, 10, 99 },
	.glyph_O = (stroke_t []) { 1, 3, 9, 19, 23, 21, 15, 5, 1, 99 },
	.glyph_N = (stroke_t []) { 20, 0, 23, 3, 99 },
	.glyph_M = (stroke_t []) { 20, 0, 17, 4, 24, 99 },
	.glyph_L = (stroke_t []) { 0, 20, 23, 99},
	.glyph_K = (stroke_t []) { 0, 20, LP, 15, 3, LP, 12, 24, 99},
	.glyph_J = (stroke_t []) { 3, 18, 22, 21, 15, 99},
	.glyph_I = (stroke_t []) { 0, 2, LP, 1, 21, LP, 20, 22, 99 },
	.glyph_H = (stroke_t []) { 0, 20, LP, 3, 23, LP, 10, 13, 99 },
	.glyph_G = (stroke_t []) { 9, 3, 1, 5, 15, 21, 23, 19, 14, 12, 99 },
	.glyph_F = (stroke_t []) { 20, 0, 3, LP, 10, 12, 99 },
	.glyph_E = (stroke_t []) { 23, 20, 0, 3, LP, 10, 12, 99 },
	.glyph_D = (stroke_t []) { 20, 0, 3, 9, 19, 23, 20, 99 },
	.glyph_C = (stroke_t []) { 19, 23, 21, 15, 5, 1, 3, 9, 99 },
	.glyph_B = (stroke_t []) { 20, 0, 3, 9, 13, 10, LP, 13, 19, 23, 20, 99 },
	.glyph_A = (stroke_t []) { 20, 2, 24, LP, 11, 13, 99 },
	.glyph_slash = (stroke_t []) { 20, 3, 99 },
	.glyph_backslash = (stroke_t []) { 0, 23, 99 },
	.glyph_pipe = (stroke_t []) { 2, 22, 99 },
	.glyph_que = (stroke_t []) { 5, 1, 3, 9, 14, 17, 22, LP, 27, 32, 99 },
	.glyph_bang = (stroke_t []) { 2, 22, LP, 27, 32, 99 },
	.glyph_colon = (stroke_t []) { 6, 7, LP, 16, 17, 99 },
	.glyph_leftparen = (stroke_t []) { 1, 5, 15, 21, 99 },
	.glyph_rightparen = (stroke_t []) { 0, 6, 16, 20, 99 },
	.glyph_space = (stroke_t []) { 99 },
	.glyph_plus = (stroke_t []) { 2, 22, LP, 10, 14, 99 },
	.glyph_dash = (stroke_t []) { 11, 13, 99 },
	.glyph_0 = (stroke_t []) { 1, 3, 9, 19, 23, 21, 15, 5, 1, LP, 8, 16, 99 },
	.glyph_9 = (stroke_t []) { 15, 21, 23, 19, 9, 3, 1, 5, 11, 13, 9, 99 },
	.glyph_8 = (stroke_t []) { 11, 5, 1, 3, 9, 13, 11, 15, 21, 23, 19, 13, 99 },
	.glyph_7 = (stroke_t []) { 0, 4, 12, 21, 99 },
	.glyph_6 = (stroke_t []) { 15, 11, 13, 19, 23, 21, 15, 5, 1, 3, 9, 99 },
	.glyph_5 = (stroke_t []) { 4, 0, 10, 13, 19, 23, 21, 15, 99 },
	.glyph_4 = (stroke_t []) { 23, 3, 15, 19, 99 },
	.glyph_3 = (stroke_t []) { 0, 4, 12, 13, 19, 23, 21, 15, 99 },
	.glyph_2 = (stroke_t []) { 5, 1, 3, 9, 20, 24, 99 },
	.glyph_1 = (stroke_t []) { 5, 1, 21, LP, 20, 22, 99 },
	.glyph_comma = (stroke_t []) { 21, 25, 99 },
	.glyph_period = (stroke_t []) { 20, 21, 99 },
	.glyph_z = (stroke_t []) { 10, 13, 20, 23, 99},
	.glyph_y = (stroke_t []) { 10, 21, LP, 12, 30, 99},
	.glyph_x = (stroke_t []) { 10, 23, LP, 20, 13, 99 },
	.glyph_w = (stroke_t []) { 10, 21, 12, 23, 14, 99 },
	.glyph_v = (stroke_t []) { 10, 21, 12, 99 },
	.glyph_u = (stroke_t []) { 10, 15, 21, 22, 18, 13, 23, 99 },
	.glyph_t = (stroke_t []) { 6, 21, 22, LP, 10, 12, 99 },
	.glyph_s = (stroke_t []) { 13, 10, 15, 18, 23, 20, 99 },
	.glyph_a = (stroke_t []) { 10, 13, 23, 20, 15, 18, 99 },
	.glyph_b = (stroke_t []) { 0, 20, 23, 13, 10, 99 },
	.glyph_c = (stroke_t []) { 13, 10, 20, 23, 99 },
	.glyph_d = (stroke_t []) { 3, 23, 20, 10, 13, 99 },
	.glyph_e = (stroke_t []) { 15, 18, 13, 10, 20, 23, 99 },
	.glyph_f = (stroke_t []) { 21, 1, 2, LP, 10, 12, 99 },
	.glyph_g = (stroke_t []) { 25, 28, 33, 30, 10, 13, 23, 20, 99 },
	.glyph_h = (stroke_t []) { 0, 20, LP, 10, 12, 18, 23, 99 },
	.glyph_i = (stroke_t []) { 10, 20, 99},
	.glyph_j = (stroke_t []) { 12, 27, 31, 30, 99 },
	.glyph_k = (stroke_t []) { 0, 20, LP, 7, 15, LP, 11, 23, 99 },
	.glyph_l = (stroke_t []) { 0, 20, 99 },
	.glyph_m = (stroke_t []) { 20, 10, 11, 17, 22, 12, 13, 19, 24, 99 },
	.glyph_n = (stroke_t []) { 20, 10, 11, 17, 22, 99 },
	.glyph_o = (stroke_t []) { 10, 13, 23, 20, 10, 99 },
	.glyph_p = (stroke_t []) { 30, 10, 13, 23, 20, 99 },
	.glyph_q = (stroke_t []) { 33, 13, 10, 20, 23, 99 },
	.glyph_r = (stroke_t []) { 10, 20, LP, 15, 11, 12, 18, 99 },
	.glyph_apostrophe = (stroke_t []) { 2, 7, 99 },
	.glyph_doublequote = (stroke_t []) { 1, 6, LP, 3, 8, 99 },
	.glyph_asterisk = (stroke_t []) { 7, 17, LP, 11, 13, LP, 6, 18, LP, 16, 8, 99 },
	.glyph_underscore = (stroke_t []) { 30, 34, 99 },
	.glyph_hash = (stroke_t []) { 6, 21, LP, 8, 23, LP, 10, 14, LP, 15, 19, 99 },
	.glyph_dollar = (stroke_t []) { 0, 3, 25, LP, 4, 26, 29, LP, 15, 19, LP, 10, 14, 99 },
	.glyph_percent = (stroke_t []) { 4, 25, LP, 1, 5, 11, 7, 1, LP, 18, 22, 28, 24, 18, 99 },
	.glyph_caret = (stroke_t []) { 6, 2, 8, 99 },
	.glyph_ampersand = (stroke_t []) { 24, 6, 1, 2, 7, 15, 21, 23, 14, 99 },
	.glyph_at = (stroke_t []) { 18, 8, 7, 11, 16, 18, 13, 19, 9, 3, 1, 5, 15, 21, 24, 99 },
	.glyph_lessthan = (stroke_t []) { 4, 10, 24, 99 },
	.glyph_greaterthan = (stroke_t []) { 0, 14, 20, 99 },
	.glyph_rightbracket = (stroke_t []) { 2, 4, 24, 22, 99 },
	.glyph_leftbracket = (stroke_t []) { 2, 0, 20, 22, 99 },
	.glyph_semicolon = (stroke_t []) { 6, 7, LP, 16, 17, 22, 99 },
	.glyph_tilde = (stroke_t []) { 5, 1, 7, 3, 4, 99 },
	.glyph_equals = (stroke_t []) { 5, 9, LP, 15, 19, 99 },
};

static struct font_def alien_font_def = {
	.glyph_open_curly = (stroke_t []) { 2, 6, 11, 15, 21, 26, 32, 99 },
	.glyph_close_curly = (stroke_t []) { 2, 8, 13, 19, 23, 28, 32, 99 },
	.glyph_Z = (stroke_t []) { 15, 20, 24, 4, LP, 9, 7, 11, 99 },
	.glyph_Y = (stroke_t []) { 0, 22, 23, 4, 3, 7, 99},
	.glyph_X = (stroke_t []) { 2, 20, 24, 2, 99 },
	.glyph_W = (stroke_t []) { 0, 4, 24, 20, 0, 99 },
	.glyph_V = (stroke_t []) { 0, 12, 4, LP, 12, 22, 99 },
	.glyph_U = (stroke_t []) { 0, 20, 24, 4, 3, 7, 99 },
	.glyph_T = (stroke_t []) { 15, 21, 23, 19, LP, 22, 2, 99 },
	.glyph_S = (stroke_t []) { 1, 24, 4, LP, 15, 22, 99 },
	.glyph_R = (stroke_t []) { 0, 4, 20, 99 },
	.glyph_Q = (stroke_t []) { 9, 4, 0, 20, 22, 99 },
	.glyph_P = (stroke_t []) { 1, 10, 21, 24, 4, LP, 2, 7, 99 },
	.glyph_O = (stroke_t []) { 1, 20, 24, 3, 1, 99 },
	.glyph_N = (stroke_t []) { 1, 10, 21, 2, 23, 99 },
	.glyph_M = (stroke_t []) { 24, 20, 3, 4, 9, 99 },
	.glyph_L = (stroke_t []) { 15, 23, 24, 4, 99 },
	.glyph_K = (stroke_t []) { 0, 4, 24, 20, 99 },
	.glyph_J = (stroke_t []) { 10, 12, 9, 22, 20, 99 },
	.glyph_I = (stroke_t []) { 5, 1, 2, 22, 99 },
	.glyph_H = (stroke_t []) { 0, 4, LP, 11, 13, LP, 20, 24, 99 },
	.glyph_G = (stroke_t []) { 0, 20, 23, 4, 1, 6, 99 },
	.glyph_F = (stroke_t []) { 24, 20, 10, 12, 22, LP, 7, 12, 9, 99 },
	.glyph_E = (stroke_t []) { 0, 21, 2, 4, LP, 3, 23, 99 },
	.glyph_D = (stroke_t []) { 0, 4, 20, LP, 10, 12, 99 },
	.glyph_C = (stroke_t []) { 0, 15, LP, 7, 17, LP, 9, 24, 99 },
	.glyph_B = (stroke_t []) { 5, 1, 3, 9, LP, 11, 13, LP, 15, 21, 23, 19, 99 },
	.glyph_A = (stroke_t []) { 0, 5, 7, 4, LP, 20, 15, 17, 24, 99 },
	.glyph_slash = (stroke_t []) { 20, 3, 99 },
	.glyph_backslash = (stroke_t []) { 0, 23, 99 },
	.glyph_pipe = (stroke_t []) { 2, 22, 99 },
	.glyph_que = (stroke_t []) { 5, 1, 3, 9, 14, 17, 22, LP, 27, 32, 99 },
	.glyph_bang = (stroke_t []) { 2, 22, LP, 27, 32, 99 },
	.glyph_colon = (stroke_t []) { 6, 7, LP, 16, 17, 99 },
	.glyph_leftparen = (stroke_t []) { 1, 5, 15, 21, 99 },
	.glyph_rightparen = (stroke_t []) { 0, 6, 16, 20, 99 },
	.glyph_space = (stroke_t []) { 99 },
	.glyph_plus = (stroke_t []) { 2, 22, LP, 10, 14, 99 },
	.glyph_dash = (stroke_t []) { 11, 13, 99 },
	.glyph_0 = (stroke_t []) { 0, 3, 23, 20, 0, LP, 11, 12, 99 },
	.glyph_1 = (stroke_t []) { 0, 1, 21, LP, 20, 22, 99 },
	.glyph_2 = (stroke_t []) { 0, 3, 13, 10, LP, 20, 23, 99 },
	.glyph_3 = (stroke_t []) { 0, 3, 23, 20, LP, 10, 12, 99 },
	.glyph_4 = (stroke_t []) { 0, 10, 13, LP, 3, 23, 99 },
	.glyph_5 = (stroke_t []) { 0, 3, LP, 10, 13, 23, 20, 99 },
	.glyph_6 = (stroke_t []) { 0, 3, LP, 10, 13, 23, 20, 10, 99 },
	.glyph_7 = (stroke_t []) { 0, 3, 23, 99 },
	.glyph_8 = (stroke_t []) { 0, 3, 23, 20, 0, LP, 10, 13, 99 },
	.glyph_9 = (stroke_t []) { 0, 3, 13, 10, 0, LP, 20, 23, 99 },
	.glyph_comma = (stroke_t []) { 21, 25, 99 },
	.glyph_period = (stroke_t []) { 20, 21, 99 },
	.glyph_z = (stroke_t []) { 15, 20, 24, 4, LP, 9, 7, 11, 99 },
	.glyph_y = (stroke_t []) { 0, 22, 23, 4, 3, 7, 99},
	.glyph_x = (stroke_t []) { 2, 20, 24, 2, 99 },
	.glyph_w = (stroke_t []) { 0, 4, 24, 20, 0, 99 },
	.glyph_v = (stroke_t []) { 0, 12, 4, LP, 12, 22, 99 },
	.glyph_u = (stroke_t []) { 0, 20, 24, 4, 3, 7, 99 },
	.glyph_t = (stroke_t []) { 15, 21, 23, 19, LP, 22, 2, 99 },
	.glyph_s = (stroke_t []) { 1, 24, 4, LP, 15, 22, 99 },
	.glyph_r = (stroke_t []) { 0, 4, 20, 99 },
	.glyph_q = (stroke_t []) { 9, 4, 0, 20, 22, 99 },
	.glyph_p = (stroke_t []) { 1, 10, 21, 24, 4, LP, 2, 7, 99 },
	.glyph_o = (stroke_t []) { 1, 20, 24, 3, 1, 99 },
	.glyph_n = (stroke_t []) { 1, 10, 21, 2, 23, 99 },
	.glyph_m = (stroke_t []) { 24, 20, 3, 4, 9, 99 },
	.glyph_l = (stroke_t []) { 15, 23, 24, 4, 99 },
	.glyph_k = (stroke_t []) { 0, 4, 24, 20, 99 },
	.glyph_j = (stroke_t []) { 10, 12, 9, 22, 20, 99 },
	.glyph_i = (stroke_t []) { 5, 1, 2, 22, 99 },
	.glyph_h = (stroke_t []) { 0, 4, LP, 11, 13, LP, 20, 24, 99 },
	.glyph_g = (stroke_t []) { 0, 20, 23, 4, 1, 6, 99 },
	.glyph_f = (stroke_t []) { 24, 20, 10, 12, 22, LP, 7, 12, 9, 99 },
	.glyph_e = (stroke_t []) { 0, 21, 2, 4, LP, 3, 23, 99 },
	.glyph_d = (stroke_t []) { 0, 4, 20, LP, 10, 12, 99 },
	.glyph_c = (stroke_t []) { 0, 15, LP, 7, 17, LP, 9, 24, 99 },
	.glyph_b = (stroke_t []) { 5, 1, 3, 9, LP, 11, 13, LP, 15, 21, 23, 19, 99 },
	.glyph_a = (stroke_t []) { 0, 5, 7, 4, LP, 20, 15, 17, 24, 99 },
	.glyph_apostrophe = (stroke_t []) { 2, 7, 99 },
	.glyph_doublequote = (stroke_t []) { 1, 6, LP, 3, 8, 99 },
	.glyph_asterisk = (stroke_t []) { 7, 17, LP, 11, 13, LP, 6, 18, LP, 16, 8, 99 },
	.glyph_underscore = (stroke_t []) { 30, 34, 99 },
	.glyph_hash = (stroke_t []) { 6, 21, LP, 8, 23, LP, 10, 14, LP, 15, 19, 99 },
	.glyph_dollar = (stroke_t []) { 0, 3, 25, LP, 4, 26, 29, LP, 15, 19, LP, 10, 14, 99 },
	.glyph_percent = (stroke_t []) { 4, 25, LP, 1, 5, 11, 7, 1, LP, 18, 22, 28, 24, 18, 99 },
	.glyph_caret = (stroke_t []) { 6, 2, 8, 99 },
	.glyph_ampersand = (stroke_t []) { 24, 6, 1, 2, 7, 15, 21, 23, 14, 99 },
	.glyph_at = (stroke_t []) { 18, 8, 7, 11, 16, 18, 13, 19, 9, 3, 1, 5, 15, 21, 24, 99 },
	.glyph_lessthan = (stroke_t []) { 4, 10, 24, 99 },
	.glyph_greaterthan = (stroke_t []) { 0, 14, 20, 99 },
	.glyph_rightbracket = (stroke_t []) { 2, 4, 24, 22, 99 },
	.glyph_leftbracket = (stroke_t []) { 2, 0, 20, 22, 99 },
	.glyph_semicolon = (stroke_t []) { 6, 7, LP, 16, 17, 22, 99 },
	.glyph_tilde = (stroke_t []) { 5, 1, 7, 3, 4, 99 },
	.glyph_equals = (stroke_t []) { 5, 9, LP, 15, 19, 99 },
};

static struct font_def ascii_smallcaps_font_def = {
	.glyph_open_curly = (stroke_t []) { 2, 1, 6, 10, 16, 21, 22, 99 },
	.glyph_close_curly = (stroke_t []) {0, 1, 6, 12, 16, 21, 20, 99 },
	.glyph_Z = (stroke_t []) { 0, 3, 20, 23, 99 },
	.glyph_Y = (stroke_t []) { 0, 6, 3, LP, 6, 21, 99 },
	.glyph_X = (stroke_t []) { 0, 23, LP, 3, 20, 99 },
	.glyph_W = (stroke_t []) { 0, 20, 11, 23, 3, 99 },
	.glyph_V = (stroke_t []) { 0, 21, 3, 99 },
	.glyph_U = (stroke_t []) { 0, 15, 21, 22, 18, 3, 99 },
	.glyph_T = (stroke_t []) { 0, 3, LP, 1, 21, 99 },
	.glyph_S = (stroke_t []) { 3, 1, 5, 18, 22, 20, 99 },
	.glyph_R = (stroke_t []) { 20, 0, 2, 8, 12, 23, LP, 10, 12, 99 },
	.glyph_Q = (stroke_t []) { 1, 2, 8, 18, 22, 21, 15, 5, 1, LP, 17, 23, 99 },
	.glyph_P = (stroke_t []) { 20, 0, 2, 8, 12, 10, 99 },
	.glyph_O = (stroke_t []) { 1, 2, 8, 18, 22, 21, 15, 5, 1, 99 },
	.glyph_N = (stroke_t []) { 20, 0, 23, 3, 99 },
	.glyph_M = (stroke_t []) { 20, 0, 11, 3, 23, 99 },
	.glyph_L = (stroke_t []) { 0, 20, 23, 99 },
	.glyph_K = (stroke_t []) { 0, 20, LP, 3, 10, 23, 99 },
	.glyph_J = (stroke_t []) { 0, 3, LP, 2, 17, 21, 15, 99 },
	.glyph_I = (stroke_t []) { 0, 3, LP, 1, 21, LP, 20, 23, 99 },
	.glyph_H = (stroke_t []) { 0, 20, LP, 3, 23, LP, 10, 13, 99 },
	.glyph_G = (stroke_t []) { 8, 2, 1, 5, 15, 21, 22, 18, 12, 99 },
	.glyph_F = (stroke_t []) { 3, 0, 20, LP, 10, 13, 99 },
	.glyph_E = (stroke_t []) { 3, 0, 20, 23, LP, 10, 13, 99 },
	.glyph_D = (stroke_t []) { 0, 2, 8, 18, 22, 20, 0, 99 },
	.glyph_C = (stroke_t []) { 8, 2, 1, 5, 15, 21, 22, 18, 99 },
	.glyph_B = (stroke_t []) { 0, 20, 22, 18, 12, 8, 2, 0, LP, 10, 12, 99 },
	.glyph_A = (stroke_t []) { 20, 5, 1, 2, 8, 23, LP, 10, 13, 99 },
	.glyph_slash = (stroke_t []) { 20, 3, 99 },
	.glyph_backslash = (stroke_t []) { 23, 0, 99 },
	.glyph_pipe = (stroke_t []) { 0, 20, 99 },
	.glyph_que = (stroke_t []) { 5, 1, 2, 8, 11, LP, 21, 22, 17, 16, 21, 99 },
	.glyph_bang = (stroke_t []) { 0, 10, LP, 15, 20, 99 },
	.glyph_colon = (stroke_t []) { 5, 6, 11, 10, 5, LP, 15, 16, 21, 20, 15 , 99 },
	.glyph_leftparen = (stroke_t []) { 1, 5, 15, 21, 99 },
	.glyph_rightparen = (stroke_t []) { 0, 6, 16, 20, 99 },
	.glyph_space = (stroke_t []) { 99 },
	.glyph_plus = (stroke_t []) { 10, 12, LP, 6, 16, 99 },
	.glyph_dash = (stroke_t []) { 10, 12, 99 },
	.glyph_0 = (stroke_t []) { 1, 2, 8, 18, 22, 21, 15, 5, 1, LP, 8, 15, 99 },
	.glyph_9 = (stroke_t []) { 13, 11, 5, 1, 2, 8, 18, 22, 21, 15, 99 },
	.glyph_8 = (stroke_t []) { 11, 5, 1, 2, 8, 12, 11, 15, 21, 22, 18, 12, 99 },
	.glyph_7 = (stroke_t []) { 0, 3, 21, 99 },
	.glyph_6 = (stroke_t []) { 8, 2, 1, 5, 15, 21, 22, 18, 12, 10, 99 },
	.glyph_5 = (stroke_t []) { 3, 0, 10, 6, 7, 13, 18, 22, 21, 15, 99 },
	.glyph_4 = (stroke_t []) { 13, 10, 2, 22, 99 },
	.glyph_3 = (stroke_t []) { 5, 1, 2, 8, 12, 18, 22, 21, 15, 99 },
	.glyph_2 = (stroke_t []) { 5, 1, 2, 8, 20, 23, 99 },
	.glyph_1 = (stroke_t []) { 5, 2, 22, LP, 20, 23, 99 },
	.glyph_comma = (stroke_t []) { 15, 16, 20, 99 },
	.glyph_period = (stroke_t []) { 15, 16, 21, 20, 15, 99 },
	.glyph_z = (stroke_t []) { 5, 8, 20, 23, 99 },
	.glyph_y = (stroke_t []) { 5, 11, 8, LP, 11, 21, 99 },
	.glyph_x = (stroke_t []) { 5, 23, LP, 8, 20, 99 },
	.glyph_w = (stroke_t []) { 5, 20, 11, 23, 8, 99 },
	.glyph_v = (stroke_t []) { 5, 21, 8, 99 },
	.glyph_u = (stroke_t []) { 5, 15, 21, 22, 18, 8, 99 },
	.glyph_t = (stroke_t []) { 5, 8, LP, 6, 21, 99 },
	.glyph_s = (stroke_t []) { 8, 6, 10, 18, 22, 20, 99 },
	.glyph_a = (stroke_t []) { 20, 10, 6, 7, 13, 23, LP, 15, 18, 99 },
	.glyph_b = (stroke_t []) { 5, 8, 12, 18, 22, 20, 5, LP, 10, 12, 99 },
	.glyph_c = (stroke_t []) { 13, 7, 6, 10, 15, 21, 22, 18, 99 },
	.glyph_d = (stroke_t []) { 20, 5, 7, 13, 18, 22, 20, 99 },
	.glyph_e = (stroke_t []) { 8, 5, 20, 23, LP, 10, 13, 99 },
	.glyph_f = (stroke_t []) { 8, 5, 20, LP, 10, 13, 99 },
	.glyph_g = (stroke_t []) { 13, 7, 6, 10, 15, 21, 22, 18, 17, 99 },
	.glyph_h = (stroke_t []) { 5, 20, LP, 8, 23, LP, 10, 13, 99 },
	.glyph_i = (stroke_t []) { 5, 8, LP, 20, 23, LP, 6, 21, 99 },
	.glyph_j = (stroke_t []) { 5, 8, LP, 7, 17, 21, 15, 99 },
	.glyph_k = (stroke_t []) { 5, 20, LP, 8, 10, 23, 99 },
	.glyph_l = (stroke_t []) { 5, 20, 23, 99 },
	.glyph_m = (stroke_t []) { 20, 5, 16, 8, 23, 99 },
	.glyph_n = (stroke_t []) { 20, 5, 23, 8, 99 },
	.glyph_o = (stroke_t []) { 6, 10, 15, 21, 22, 18, 13, 7, 6, 99 },
	.glyph_p = (stroke_t []) { 20, 5, 7, 13, 17, 15, 99 },
	.glyph_q = (stroke_t []) { 6, 10, 15, 21, 22, 18, 13, 7, 6, LP, 17, 23, 99 },
	.glyph_r = (stroke_t []) { 20, 5, 7, 13, 17, 23, LP, 15, 17, 99 },
	.glyph_apostrophe = (stroke_t []) {0, 5, 99 },
	.glyph_doublequote = (stroke_t []) {0, 5, LP, 1, 6, 99 },
	.glyph_asterisk = (stroke_t []) {5, 18, LP, 8, 15, LP, 7, 17, LP, 10, 13 , 99 },
	.glyph_underscore = (stroke_t []) { 30, 34, 99 },
	.glyph_hash = (stroke_t []) { 1, 21, LP, 2, 22, LP, 5, 8, LP, 15, 18, 99 },
	.glyph_dollar = (stroke_t []) { 8, 5, 10, 13, 18, 15, LP, 1, 21, LP, 2, 22, 99 },
	.glyph_percent = (stroke_t []) { 0, 1, 6, 5, 0, LP, 3, 20, LP, 17, 18, 23, 22, 17, 99 },
	.glyph_caret = (stroke_t []) { 5, 1, 7, 99 },
	.glyph_ampersand = (stroke_t []) { 23, 5, 1, 7, 15, 21, 13, 99 },
	.glyph_at = (stroke_t []) { 17, 12, 11, 16, 17, 13, 7, 6, 10, 15, 21, 22, 99 },
	.glyph_lessthan = (stroke_t []) { 7, 10, 17, 99 },
	.glyph_greaterthan = (stroke_t []) { 5, 12, 15, 99 },
	.glyph_rightbracket = (stroke_t []) { 0, 1, 21, 20, 99 },
	.glyph_leftbracket = (stroke_t []) { 1, 0, 20, 21, 99 },
	.glyph_semicolon = (stroke_t []) { 5, 6, 11, 10, 5, LP, 15, 16, 20, 99 },
	.glyph_tilde = (stroke_t []) { 10, 6, 12, 8, 99 },
	.glyph_equals = (stroke_t []) {10, 12, LP, 15, 17 , 99 },
};

struct font_def *ascii_font = &ascii_font_def;
struct font_def *alien_font = &alien_font_def;
struct font_def *ascii_smallcaps_font = &ascii_smallcaps_font_def;

/* x and y offsets for decoding stroke_t's, above */
static struct my_point_t decode_glyph[] = {
	{ 0, -3 },
	{ 1, -3 },
	{ 2, -3 },
	{ 3, -3 },
	{ 4, -3 },
	{ 0, -2 },
	{ 1, -2 },
	{ 2, -2 },
	{ 3, -2 },
	{ 4, -2 },
	{ 0, -1 },
	{ 1, -1 },
	{ 2, -1 },
	{ 3, -1 },
	{ 4, -1 },
	{ 0, -0 },
	{ 1, -0 },
	{ 2, -0 },
	{ 3, -0 },
	{ 4, -0 },
	{ 0, 1 },
	{ 1, 1 },
	{ 2, 1 },
	{ 3, 1 },
	{ 4, 1 },
	{ 0, 2 },
	{ 1, 2 },
	{ 2, 2 },
	{ 3, 2 },
	{ 4, 2 },
	{ 0, 3 },
	{ 1, 3 },
	{ 2, 3 },
	{ 3, 3 },
	{ 4, 3 },
};

/* This converts a stroke_t, which is a sort of slightly compressed coding
 * of how to draw a letter, lifted from Hofstadter's book, and converts it
 * into a set of line segments and breaks, like all the other objects in
 * the game, while also scaling it by some amount.  It is used in making
 * a particular font size
 */
static struct my_vect_obj *prerender_glyph(stroke_t g[], float xscale, float yscale)
{
	int i;
	float x, y;
	int npoints = 0;
	struct my_point_t scratch[100];
	struct my_vect_obj *v;

	int bbx1=0, bby1=0, bbx2=0, bby2=0;

	/* printf("Prerendering glyph..\n"); */

	for (i=0;g[i] != 99;i++) {
		if (g[i] == LP) {
			/* printf("LINE_BREAK\n"); */
			x = LINE_BREAK;
			y = LINE_BREAK;
		} else {
			x = decode_glyph[g[i]].x * xscale;
			y = decode_glyph[g[i]].y * yscale;
			/* printf("x=%d, y=%d\n", x,y); */

			if (i==0 || x < bbx1) bbx1=x;
			if (i==0 || x > bbx2) bbx2=x;
			if (i==0 || y < bby1) bby1=y;
			if (i==0 || y > bby2) bby2=y;
		}
		scratch[npoints].x = (int) x;
		scratch[npoints].y = (int) y;
		npoints++;
	}

	v = (struct my_vect_obj *) malloc(sizeof(struct my_vect_obj));
	v->npoints = npoints;
	if (npoints != 0) {
		v->p = (struct my_point_t *) malloc(sizeof(struct my_point_t) * npoints);
		memcpy(v->p, scratch, sizeof(struct my_point_t) * npoints);
	} else
		v->p = NULL;
	v->bbx1 = bbx1;
	v->bby1 = bby1;
	v->bbx2 = bbx2;
	v->bby2 = bby2;
	return v;
}

/* This makes a font, by prerendering all the known glyphs into
 * prescaled sets of line segments that the drawing routines know
 * how to draw.
 */
int snis_make_font(struct my_vect_obj ***font, struct font_def *f, float xscale, float yscale)
{
	struct my_vect_obj **v;

	v = malloc(sizeof(*v) * 256);
	if (!v) {
		if (v) free(v);
		return -1;
	}
	memset(v, 0, sizeof(*v) * 256);
	v['A'] = prerender_glyph(f->glyph_A, xscale, yscale);
	v['B'] = prerender_glyph(f->glyph_B, xscale, yscale);
	v['C'] = prerender_glyph(f->glyph_C, xscale, yscale);
	v['D'] = prerender_glyph(f->glyph_D, xscale, yscale);
	v['E'] = prerender_glyph(f->glyph_E, xscale, yscale);
	v['F'] = prerender_glyph(f->glyph_F, xscale, yscale);
	v['G'] = prerender_glyph(f->glyph_G, xscale, yscale);
	v['H'] = prerender_glyph(f->glyph_H, xscale, yscale);
	v['I'] = prerender_glyph(f->glyph_I, xscale, yscale);
	v['J'] = prerender_glyph(f->glyph_J, xscale, yscale);
	v['K'] = prerender_glyph(f->glyph_K, xscale, yscale);
	v['L'] = prerender_glyph(f->glyph_L, xscale, yscale);
	v['M'] = prerender_glyph(f->glyph_M, xscale, yscale);
	v['N'] = prerender_glyph(f->glyph_N, xscale, yscale);
	v['O'] = prerender_glyph(f->glyph_O, xscale, yscale);
	v['P'] = prerender_glyph(f->glyph_P, xscale, yscale);
	v['Q'] = prerender_glyph(f->glyph_Q, xscale, yscale);
	v['R'] = prerender_glyph(f->glyph_R, xscale, yscale);
	v['S'] = prerender_glyph(f->glyph_S, xscale, yscale);
	v['T'] = prerender_glyph(f->glyph_T, xscale, yscale);
	v['U'] = prerender_glyph(f->glyph_U, xscale, yscale);
	v['V'] = prerender_glyph(f->glyph_V, xscale, yscale);
	v['W'] = prerender_glyph(f->glyph_W, xscale, yscale);
	v['X'] = prerender_glyph(f->glyph_X, xscale, yscale);
	v['Y'] = prerender_glyph(f->glyph_Y, xscale, yscale);
	v['Z'] = prerender_glyph(f->glyph_Z, xscale, yscale);
	v['!'] = prerender_glyph(f->glyph_bang, xscale, yscale);
	v['/'] = prerender_glyph(f->glyph_slash, xscale, yscale);
	v['\\'] = prerender_glyph(f->glyph_backslash, xscale, yscale);
	v['|'] = prerender_glyph(f->glyph_pipe, xscale, yscale);
	v['?'] = prerender_glyph(f->glyph_que, xscale, yscale);
	v[':'] = prerender_glyph(f->glyph_colon, xscale, yscale);
	v['('] = prerender_glyph(f->glyph_leftparen, xscale, yscale);
	v[')'] = prerender_glyph(f->glyph_rightparen, xscale, yscale);
	v['a'] = prerender_glyph(f->glyph_a, xscale, yscale);
	v[' '] = prerender_glyph(f->glyph_space, xscale, yscale);
	v['b'] = prerender_glyph(f->glyph_b, xscale, yscale);
	v['c'] = prerender_glyph(f->glyph_c, xscale, yscale);
	v['d'] = prerender_glyph(f->glyph_d, xscale, yscale);
	v['e'] = prerender_glyph(f->glyph_e, xscale, yscale);
	v['f'] = prerender_glyph(f->glyph_f, xscale, yscale);
	v['g'] = prerender_glyph(f->glyph_g, xscale, yscale);
	v['h'] = prerender_glyph(f->glyph_h, xscale, yscale);
	v['i'] = prerender_glyph(f->glyph_i, xscale, yscale);
	v['j'] = prerender_glyph(f->glyph_j, xscale, yscale);
	v['k'] = prerender_glyph(f->glyph_k, xscale, yscale);
	v['l'] = prerender_glyph(f->glyph_l, xscale, yscale);
	v['m'] = prerender_glyph(f->glyph_m, xscale, yscale);
	v['n'] = prerender_glyph(f->glyph_n, xscale, yscale);
	v['o'] = prerender_glyph(f->glyph_o, xscale, yscale);
	v['p'] = prerender_glyph(f->glyph_p, xscale, yscale);
	v['q'] = prerender_glyph(f->glyph_q, xscale, yscale);
	v['r'] = prerender_glyph(f->glyph_r, xscale, yscale);
	v['s'] = prerender_glyph(f->glyph_s, xscale, yscale);
	v['t'] = prerender_glyph(f->glyph_t, xscale, yscale);
	v['u'] = prerender_glyph(f->glyph_u, xscale, yscale);
	v['v'] = prerender_glyph(f->glyph_v, xscale, yscale);
	v['w'] = prerender_glyph(f->glyph_w, xscale, yscale);
	v['x'] = prerender_glyph(f->glyph_x, xscale, yscale);
	v['y'] = prerender_glyph(f->glyph_y, xscale, yscale);
	v['z'] = prerender_glyph(f->glyph_z, xscale, yscale);
	v['0'] = prerender_glyph(f->glyph_0, xscale, yscale);
	v['1'] = prerender_glyph(f->glyph_1, xscale, yscale);
	v['2'] = prerender_glyph(f->glyph_2, xscale, yscale);
	v['3'] = prerender_glyph(f->glyph_3, xscale, yscale);
	v['4'] = prerender_glyph(f->glyph_4, xscale, yscale);
	v['5'] = prerender_glyph(f->glyph_5, xscale, yscale);
	v['6'] = prerender_glyph(f->glyph_6, xscale, yscale);
	v['7'] = prerender_glyph(f->glyph_7, xscale, yscale);
	v['8'] = prerender_glyph(f->glyph_8, xscale, yscale);
	v['9'] = prerender_glyph(f->glyph_9, xscale, yscale);
	v['-'] = prerender_glyph(f->glyph_dash, xscale, yscale);
	v['+'] = prerender_glyph(f->glyph_plus, xscale, yscale);
	v[','] = prerender_glyph(f->glyph_comma, xscale, yscale);
	v['.'] = prerender_glyph(f->glyph_period, xscale, yscale);
	v['\''] = prerender_glyph(f->glyph_apostrophe, xscale, yscale);
	v['\"'] = prerender_glyph(f->glyph_doublequote, xscale, yscale);
	v['*'] = prerender_glyph(f->glyph_asterisk, xscale, yscale);
	v['_'] = prerender_glyph(f->glyph_underscore, xscale, yscale);
	v['#'] = prerender_glyph(f->glyph_hash, xscale, yscale);
	v['$'] = prerender_glyph(f->glyph_dollar, xscale, yscale);
	v['%'] = prerender_glyph(f->glyph_percent, xscale, yscale);
	v['^'] = prerender_glyph(f->glyph_caret, xscale, yscale);
	v['&'] = prerender_glyph(f->glyph_ampersand, xscale, yscale);
	v['@'] = prerender_glyph(f->glyph_at, xscale, yscale);
	v['<'] = prerender_glyph(f->glyph_lessthan, xscale, yscale);
	v['>'] = prerender_glyph(f->glyph_greaterthan, xscale, yscale);
	v[']'] = prerender_glyph(f->glyph_rightbracket, xscale, yscale);
	v['['] = prerender_glyph(f->glyph_leftbracket, xscale, yscale);
	v[';'] = prerender_glyph(f->glyph_semicolon, xscale, yscale);
	v['~'] = prerender_glyph(f->glyph_tilde, xscale, yscale);
	v['{'] = prerender_glyph(f->glyph_open_curly, xscale, yscale);
	v['}'] = prerender_glyph(f->glyph_close_curly, xscale, yscale);
	v['='] = prerender_glyph(f->glyph_equals, xscale, yscale);

	*font = v;
	return 0;
}

int snis_font_lineheight(float yscale)
{
	return (int) ((decode_glyph[30].y - decode_glyph[0].y) * yscale + 2);
}

