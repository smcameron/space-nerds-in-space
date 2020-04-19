#include "snis_font.h"
#define SNIS_TYPEFACE_DECLARE_GLOBALS
#include "snis_typeface.h"
#undef SNIS_TYPEFACE_DECLARE_GLOBALS

void snis_typefaces_init_with_scaling(float xscale, float yscale)
{
	/* big, small, tiny, nano, and pico scaling factors */
	float bx, by, sx, sy, tx, ty, nx, ny, px, py;

	bx = xscale * (float) font_scale[BIG_FONT];
	by = yscale * (float) font_scale[BIG_FONT];
	sx = xscale * (float) font_scale[SMALL_FONT];
	sy = yscale * (float) font_scale[SMALL_FONT];
	tx = xscale * (float) font_scale[TINY_FONT];
	ty = yscale * (float) font_scale[TINY_FONT];
	nx = xscale * (float) font_scale[NANO_FONT];
	ny = yscale * (float) font_scale[NANO_FONT];
	px = xscale * (float) font_scale[PICO_FONT];
	py = yscale * (float) font_scale[PICO_FONT];

	/* Make the line segment lists from the stroke_t structures. */
	snis_make_font(&gamefont[BIG_FONT], ascii_font, bx, by);
	snis_make_font(&gamefont[SMALL_FONT], ascii_font, sx, sy);
	snis_make_font(&gamefont[TINY_FONT], ascii_font, tx, ty);
	snis_make_font(&gamefont[NANO_FONT], ascii_font, nx, ny);
	snis_make_font(&gamefont[PICO_FONT], ascii_font, px, py);

	snis_make_font(&gamefont[5 + BIG_FONT], alien_font, bx, by);
	snis_make_font(&gamefont[5 + SMALL_FONT], alien_font, sx, sy);
	snis_make_font(&gamefont[5 + TINY_FONT], alien_font, tx, ty);
	snis_make_font(&gamefont[5 + NANO_FONT], alien_font, nx, ny);
	snis_make_font(&gamefont[5 + PICO_FONT], alien_font, px, py);

	snis_make_font(&gamefont[10 + BIG_FONT], ascii_smallcaps_font, bx, by);
	snis_make_font(&gamefont[10 + SMALL_FONT], ascii_smallcaps_font, sx, sy);
	snis_make_font(&gamefont[10 + TINY_FONT], ascii_smallcaps_font, tx, ty);
	snis_make_font(&gamefont[10 + NANO_FONT], ascii_smallcaps_font, nx, ny);
	snis_make_font(&gamefont[10 + PICO_FONT], ascii_smallcaps_font, px, py);

	font_lineheight[BIG_FONT] = snis_font_lineheight(by);
	font_lineheight[SMALL_FONT] = snis_font_lineheight(sy);
	font_lineheight[TINY_FONT] = snis_font_lineheight(ty);
	font_lineheight[NANO_FONT] = snis_font_lineheight(ny);
	font_lineheight[PICO_FONT] = snis_font_lineheight(py);
}

void snis_typefaces_init(void)
{
	snis_typefaces_init_with_scaling(1.0, 1.0);
}

