#ifndef SNIS_TYPEFACE_H__
#define SNIS_TYPEFACE_H__
/* indexes into the gamefont array */
#define BIG_FONT 0
#define SMALL_FONT 1
#define TINY_FONT 2
#define NANO_FONT 3
#define PICO_FONT 4

/* sizes of the fonts... in arbitrary units */
#define BIG_FONT_SCALE 12.0 
#define SMALL_FONT_SCALE 4.5 
#define TINY_FONT_SCALE 2.7 
#define NANO_FONT_SCALE 1.7 
#define PICO_FONT_SCALE 1.4

/* spacing of letters between the fonts, pixels */
#define BIG_LETTER_SPACING (10)
#define SMALL_LETTER_SPACING (5)
#define TINY_LETTER_SPACING (5)
#define NANO_LETTER_SPACING (4)
#define PICO_LETTER_SPACING (2)

#define NTYPEFACES (3) /* ascii, alien, and ascii_smallcaps */

/* for getting at the font scales and letter spacings, given  only font numbers */
#ifdef SNIS_TYPEFACE_DECLARE_GLOBALS
float font_scale[] = { BIG_FONT_SCALE, SMALL_FONT_SCALE, TINY_FONT_SCALE, NANO_FONT_SCALE, PICO_FONT_SCALE };
int letter_spacing[] = { BIG_LETTER_SPACING, SMALL_LETTER_SPACING, TINY_LETTER_SPACING, NANO_LETTER_SPACING, PICO_LETTER_SPACING };
int font_lineheight[sizeof(font_scale) / sizeof(font_scale[0])];
/* There are 4 home-made "fonts" in the game, all the same "typeface", but 
 * different sizes */
struct my_vect_obj **gamefont[5 * NTYPEFACES];
#else
extern int font_scale[];
extern int letter_spacing[];
extern int font_lineheight[];
extern struct my_vect_obj **gamefont[];
#endif

void snis_typefaces_init(void);
void snis_typefaces_init_with_scaling(float xscale, float yscale);

#endif
