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

/**************************

	Here's how this works, there's a 3x7 grid. on which the
	letters are drawn.  You list the sequence of strokes.
	Use 21 to lift the pen, 99 to mark the end.

	Inspired by Hofstadters' Creative Concepts and Creative Analogies.

                               letter 'A'    can be compactly represented by:

		0   1   2      *   *   *    { 6, 8, 14, 12, 9, 11, 99 }

                3   4   5      *   *   *

                6   7   8      *---*---*
                                       |
                9  10  11      *---*---*
                               |       |
               12  13  14      *---*---*

               15  16  17      *   *   *

               18  19  20      *   *   *
The grid numbers can be decoded into (x,y) coords like:
	x = ((n % 3) * xscale);
	y = (x/3) * yscale;      truncating division. 

	(not sure the above actually works)

	instead, use decode_glyph[] to get x and y values -- see below
***************************/

static stroke_t glyph_Z[] = { 0, 2, 12, 14, 99 };
static stroke_t glyph_Y[] = { 0, 7, 2, 21, 7, 13, 99 };
static stroke_t glyph_X[] = { 0, 14, 21, 12, 2, 99 };
static stroke_t glyph_W[] = { 0, 12, 10, 14, 2, 21, 10, 1, 99 };
static stroke_t glyph_V[] = { 0, 13, 2, 99 };
static stroke_t glyph_U[] = { 0, 9, 13, 11, 2, 99 };
static stroke_t glyph_T[] = { 13, 1, 21, 0, 2, 99 };
static stroke_t glyph_S[] = { 9, 13, 11, 3, 1, 5, 99 };
static stroke_t glyph_R[] = { 12, 0, 1, 5, 7, 6, 21, 7, 14, 99 };
static stroke_t glyph_Q[] = { 13, 9, 3, 1, 5, 11, 13, 21, 10, 14, 99 };
static stroke_t glyph_P[] = { 12, 0, 1, 5, 7, 6, 99 };
static stroke_t glyph_O[] = { 13, 9, 3, 1, 5, 11, 13, 99 };
static stroke_t glyph_N[] = { 12, 0, 14, 2, 99 };
static stroke_t glyph_M[] = { 12, 0, 4, 2, 14, 99 };
static stroke_t glyph_L[] = { 0, 12, 14, 99};
static stroke_t glyph_K[] = { 0, 12, 21, 6, 7, 11, 14, 21, 7, 5, 2, 99};
static stroke_t glyph_J[] = { 9, 13, 11, 2, 99}; 
static stroke_t glyph_I[] = { 12, 14, 21, 13, 1, 21, 0, 2, 99 }; 
static stroke_t glyph_H[] = { 0, 12, 21, 2, 14, 21, 6, 8, 99 };
static stroke_t glyph_G[] = { 7, 8, 11, 13, 9, 3, 1, 5, 99 };
static stroke_t glyph_F[] = { 12, 0, 2, 21, 7, 6, 99 };
static stroke_t glyph_E[] = { 14, 12, 0, 2, 21, 6, 7, 99 };
static stroke_t glyph_D[] = { 12, 13, 11, 5, 1, 0, 12, 99 };
static stroke_t glyph_C[] = { 11, 13, 9, 3, 1, 5, 99 };
static stroke_t glyph_B[] = { 0, 12, 13, 11, 5, 1, 0, 21, 6, 8, 99 };
static stroke_t glyph_A[] = { 12, 3, 0, 5, 14, 21, 8, 6, 99 };
static stroke_t glyph_slash[] = { 12, 2, 99 };
static stroke_t glyph_backslash[] = { 0, 14, 99 };
static stroke_t glyph_pipe[] = { 1, 13, 99 };
static stroke_t glyph_que[] = { 13, 10, 21, 7, 5, 2, 0, 3, 99 };
static stroke_t glyph_bang[] = { 10, 13, 21, 1, 7, 99};
static stroke_t glyph_colon[] = { 6, 7, 21, 12, 13, 99 };
static stroke_t glyph_leftparen[] = { 2, 4, 10, 14,99 };
static stroke_t glyph_rightparen[] = { 0, 4, 10, 12,99 };
static stroke_t glyph_space[] = { 99 };
static stroke_t glyph_plus[] = { 6, 8, 21, 7, 13, 99 };
static stroke_t glyph_dash[] = { 6, 8, 99 };
static stroke_t glyph_0[] = { 12, 0, 2, 14, 12, 2, 99 };
static stroke_t glyph_9[] = { 8, 6, 0, 2, 14, 99 };
static stroke_t glyph_8[] = { 0, 2, 14, 12, 0, 21, 6, 8, 99 };
static stroke_t glyph_7[] = { 0, 2, 12, 99 };
static stroke_t glyph_6[] = { 2, 0, 12, 14, 8, 6, 99 };
static stroke_t glyph_5[] = { 2, 0, 6, 8, 14, 12, 99 };
static stroke_t glyph_4[] = { 0, 6, 8, 21, 2, 14, 99 };
static stroke_t glyph_3[] = { 0, 2, 14, 12, 21, 6, 8, 99 };
static stroke_t glyph_2[] = { 0, 2, 5, 9, 12, 14, 99 };
static stroke_t glyph_1[] = { 1, 13, 99 };
static stroke_t glyph_comma[] = { 13, 16, 99 };
static stroke_t glyph_period[] = { 12, 13, 99 };
static stroke_t glyph_z[] = { 6, 8, 12, 14, 99 };
static stroke_t glyph_y[] = { 6, 12, 14, 21, 8, 17, 19, 18,  99};
static stroke_t glyph_x[] = { 12, 8, 21, 6, 14, 99 };
static stroke_t glyph_w[] = { 6, 12, 14, 8, 21, 7, 13, 99 };
static stroke_t glyph_v[] = { 6, 13, 8, 99 };
static stroke_t glyph_u[] = { 6, 12, 14, 8, 99 };
static stroke_t glyph_t[] = { 14, 13, 4, 21, 6, 8, 99 };
static stroke_t glyph_s[] = { 8, 6, 9, 11, 14,12, 99 };

static stroke_t glyph_a[] = { 6, 8, 14, 12, 9, 11, 99 };
static stroke_t glyph_b[] = { 0, 12, 14, 8, 6, 99 };
static stroke_t glyph_c[] = { 8, 6, 12, 14, 99 };
static stroke_t glyph_d[] = { 8, 6, 12, 14, 2, 99 };
static stroke_t glyph_e[] = { 9, 11, 8, 6, 12, 14, 99 };
static stroke_t glyph_f[] = { 13, 1, 2, 21, 6, 8, 99 };
static stroke_t glyph_g[] = { 11, 12, 6, 8, 20, 18, 99 };
static stroke_t glyph_h[] = { 0, 12, 21, 6, 8, 14, 99 };
static stroke_t glyph_i[] = { 13, 7, 21, 4, 2, 99 };
static stroke_t glyph_j[] = { 18, 16, 7, 21, 4, 2, 99 };
static stroke_t glyph_k[] = { 0, 12, 21, 6, 7, 11, 14, 21, 7, 5, 99 };
static stroke_t glyph_l[] = { 1, 13, 99 };
static stroke_t glyph_m[] = { 12, 6, 8, 14, 21, 7, 13, 99 };
static stroke_t glyph_n[] = { 12, 6, 7, 13, 99 };
static stroke_t glyph_o[] = { 12, 6, 8, 14, 12, 99 };
static stroke_t glyph_p[] = { 18, 6, 8, 14, 12, 99 };
static stroke_t glyph_q[] = { 14, 12, 6, 8, 20, 99 };
static stroke_t glyph_r[] = { 12, 6, 21, 9,7,8,99 };
static stroke_t glyph_apostrophe[] = { 7, 5, 99 };
static stroke_t glyph_doublequote[] = { 0, 3, 21, 1, 4, 99 };
static stroke_t glyph_asterisk[] = { 9, 5, 21, 3, 11, 21, 6, 8, 21, 4, 10, 99 };
static stroke_t glyph_underscore[] = { 18, 20, 99 };

/* x and y offsets for decoding stroke_t's, above */
static struct my_point_t decode_glyph[] = {
	{ 0, -4 },
	{ 1, -4 },
	{ 2, -4 },
	{ 0, -3 },
	{ 1, -3 },
	{ 2, -3 },
	{ 0, -2 },
	{ 1, -2 },
	{ 2, -2 },
	{ 0, -1 },
	{ 1, -1 },
	{ 2, -1 },
	{ 0, -0 },
	{ 1, -0 },
	{ 2, -0 },
	{ 0, 1 },
	{ 1, 1 },
	{ 2, 1 },
	{ 0, 2 },
	{ 1, 2 },
	{ 2, 2 },
	{ 0, 3 },
	{ 1, 3 },
	{ 2, 3 },
};

/* This converts a stroke_t, which is a sort of slightly compressed coding 
 * of how to draw a letter, lifted from Hofstadter's book, and converts it
 * into a set of line segments and breaks, like all the other objects in
 * the game, while also scaling it by some amount.  It is used in making
 * a particular font size
 */
static struct my_vect_obj *prerender_glyph(stroke_t g[], int xscale, int yscale)
{
	int i, x, y;
	int npoints = 0;
	struct my_point_t scratch[100];
	struct my_vect_obj *v;

	/* printf("Prerendering glyph..\n"); */

	for (i=0;g[i] != 99;i++) {
		if (g[i] == 21) {
			/* printf("LINE_BREAK\n"); */
			x = LINE_BREAK;
			y = LINE_BREAK;
		} else {
			/* x = ((g[i] % 3) * xscale);*/
			/* y = ((g[i]/3)-4) * yscale ;     // truncating division.*/
			x = decode_glyph[g[i]].x * xscale;
			y = decode_glyph[g[i]].y * yscale;
			/* printf("x=%d, y=%d\n", x,y); */
		}
		scratch[npoints].x = x;
		scratch[npoints].y = y;
		npoints++;
	}

	v = (struct my_vect_obj *) malloc(sizeof(struct my_vect_obj));
	v->npoints = npoints;
	if (npoints != 0) {
		v->p = (struct my_point_t *) malloc(sizeof(struct my_point_t) * npoints);
		memcpy(v->p, scratch, sizeof(struct my_point_t) * npoints);
	} else
		v->p = NULL;
	return v;
}

/* This makes a font, by prerendering all the known glyphs into
 * prescaled sets of line segments that the drawing routines know
 * how to draw.
 */
int snis_make_font(struct my_vect_obj ***font, int xscale, int yscale) 
{
	struct my_vect_obj **v;

	v = malloc(sizeof(**v) * 256);
	if (!v) {
		if (v) free(v);
		return -1;
	}
	memset(v, 0, sizeof(**v) * 256);
	v['A'] = prerender_glyph(glyph_A, xscale, yscale);
	v['B'] = prerender_glyph(glyph_B, xscale, yscale);
	v['C'] = prerender_glyph(glyph_C, xscale, yscale);
	v['D'] = prerender_glyph(glyph_D, xscale, yscale);
	v['E'] = prerender_glyph(glyph_E, xscale, yscale);
	v['F'] = prerender_glyph(glyph_F, xscale, yscale);
	v['G'] = prerender_glyph(glyph_G, xscale, yscale);
	v['H'] = prerender_glyph(glyph_H, xscale, yscale);
	v['I'] = prerender_glyph(glyph_I, xscale, yscale);
	v['J'] = prerender_glyph(glyph_J, xscale, yscale);
	v['K'] = prerender_glyph(glyph_K, xscale, yscale);
	v['L'] = prerender_glyph(glyph_L, xscale, yscale);
	v['M'] = prerender_glyph(glyph_M, xscale, yscale);
	v['N'] = prerender_glyph(glyph_N, xscale, yscale);
	v['O'] = prerender_glyph(glyph_O, xscale, yscale);
	v['P'] = prerender_glyph(glyph_P, xscale, yscale);
	v['Q'] = prerender_glyph(glyph_Q, xscale, yscale);
	v['R'] = prerender_glyph(glyph_R, xscale, yscale);
	v['S'] = prerender_glyph(glyph_S, xscale, yscale);
	v['T'] = prerender_glyph(glyph_T, xscale, yscale);
	v['U'] = prerender_glyph(glyph_U, xscale, yscale);
	v['V'] = prerender_glyph(glyph_V, xscale, yscale);
	v['W'] = prerender_glyph(glyph_W, xscale, yscale);
	v['X'] = prerender_glyph(glyph_X, xscale, yscale);
	v['Y'] = prerender_glyph(glyph_Y, xscale, yscale);
	v['Z'] = prerender_glyph(glyph_Z, xscale, yscale);
	v['!'] = prerender_glyph(glyph_bang, xscale, yscale);
	v['/'] = prerender_glyph(glyph_slash, xscale, yscale);
	v['\\'] = prerender_glyph(glyph_backslash, xscale, yscale);
	v['|'] = prerender_glyph(glyph_pipe, xscale, yscale);
	v['?'] = prerender_glyph(glyph_que, xscale, yscale);
	v[':'] = prerender_glyph(glyph_colon, xscale, yscale);
	v['('] = prerender_glyph(glyph_leftparen, xscale, yscale);
	v[')'] = prerender_glyph(glyph_rightparen, xscale, yscale);
	v['a'] = prerender_glyph(glyph_a, xscale, yscale);
	v[' '] = prerender_glyph(glyph_space, xscale, yscale);
	v['b'] = prerender_glyph(glyph_b, xscale, yscale);
	v['c'] = prerender_glyph(glyph_c, xscale, yscale);
	v['d'] = prerender_glyph(glyph_d, xscale, yscale);
	v['e'] = prerender_glyph(glyph_e, xscale, yscale);
	v['f'] = prerender_glyph(glyph_f, xscale, yscale);
	v['g'] = prerender_glyph(glyph_g, xscale, yscale);
	v['h'] = prerender_glyph(glyph_h, xscale, yscale);
	v['i'] = prerender_glyph(glyph_i, xscale, yscale);
	v['j'] = prerender_glyph(glyph_j, xscale, yscale);
	v['k'] = prerender_glyph(glyph_k, xscale, yscale);
	v['l'] = prerender_glyph(glyph_l, xscale, yscale);
	v['m'] = prerender_glyph(glyph_m, xscale, yscale);
	v['n'] = prerender_glyph(glyph_n, xscale, yscale);
	v['o'] = prerender_glyph(glyph_o, xscale, yscale);
	v['p'] = prerender_glyph(glyph_p, xscale, yscale);
	v['q'] = prerender_glyph(glyph_q, xscale, yscale);
	v['r'] = prerender_glyph(glyph_r, xscale, yscale);
	v['s'] = prerender_glyph(glyph_s, xscale, yscale);
	v['t'] = prerender_glyph(glyph_t, xscale, yscale);
	v['u'] = prerender_glyph(glyph_u, xscale, yscale);
	v['v'] = prerender_glyph(glyph_v, xscale, yscale);
	v['w'] = prerender_glyph(glyph_w, xscale, yscale);
	v['x'] = prerender_glyph(glyph_x, xscale, yscale);
	v['y'] = prerender_glyph(glyph_y, xscale, yscale);
	v['z'] = prerender_glyph(glyph_z, xscale, yscale);
	v['0'] = prerender_glyph(glyph_0, xscale, yscale);
	v['1'] = prerender_glyph(glyph_1, xscale, yscale);
	v['2'] = prerender_glyph(glyph_2, xscale, yscale);
	v['3'] = prerender_glyph(glyph_3, xscale, yscale);
	v['4'] = prerender_glyph(glyph_4, xscale, yscale);
	v['5'] = prerender_glyph(glyph_5, xscale, yscale);
	v['6'] = prerender_glyph(glyph_6, xscale, yscale);
	v['7'] = prerender_glyph(glyph_7, xscale, yscale);
	v['8'] = prerender_glyph(glyph_8, xscale, yscale);
	v['9'] = prerender_glyph(glyph_9, xscale, yscale);
	v['-'] = prerender_glyph(glyph_dash, xscale, yscale);
	v['+'] = prerender_glyph(glyph_plus, xscale, yscale);
	v[','] = prerender_glyph(glyph_comma, xscale, yscale);
	v['.'] = prerender_glyph(glyph_period, xscale, yscale);
	v['\''] = prerender_glyph(glyph_apostrophe, xscale, yscale);
	v['\"'] = prerender_glyph(glyph_doublequote, xscale, yscale);
	v['*'] = prerender_glyph(glyph_asterisk, xscale, yscale);
	v['_'] = prerender_glyph(glyph_underscore, xscale, yscale);
	*font = v;
	return 0;
}


