#include "snis_font.h"
#define SNIS_TYPEFACE_DECLARE_GLOBALS
#include "snis_typeface.h"
#undef SNIS_TYPEFACE_DECLARE_GLOBALS

void snis_typefaces_init(void)
{
	/* Make the line segment lists from the stroke_t structures. */
	snis_make_font(&gamefont[BIG_FONT], font_scale[BIG_FONT], font_scale[BIG_FONT]);
	snis_make_font(&gamefont[SMALL_FONT], font_scale[SMALL_FONT], font_scale[SMALL_FONT]);
	snis_make_font(&gamefont[TINY_FONT], font_scale[TINY_FONT], font_scale[TINY_FONT]);
	snis_make_font(&gamefont[NANO_FONT], font_scale[NANO_FONT], font_scale[NANO_FONT]);

	font_lineheight[BIG_FONT] = snis_font_lineheight(font_scale[BIG_FONT]);
	font_lineheight[SMALL_FONT] = snis_font_lineheight(font_scale[SMALL_FONT]);
	font_lineheight[TINY_FONT] = snis_font_lineheight(font_scale[TINY_FONT]);
	font_lineheight[NANO_FONT] = snis_font_lineheight(font_scale[NANO_FONT]);
}

