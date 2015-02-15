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
	snis_make_font(&gamefont[BIG_FONT], bx, by);
	snis_make_font(&gamefont[SMALL_FONT], sx, sy);
	snis_make_font(&gamefont[TINY_FONT], tx, ty);
	snis_make_font(&gamefont[NANO_FONT], nx, ny);
	snis_make_font(&gamefont[PICO_FONT], px, py);

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

