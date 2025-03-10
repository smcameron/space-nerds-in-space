#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <errno.h>
#include <string.h>
#include <inttypes.h>
#include <errno.h>
#include <string.h>
#include <assert.h>
#ifndef WITHOUTOPENGL
#include <GL/glew.h>
#include <png.h>
#endif

#include "arraysize.h"
#include "snis_font.h"
#define SNIS_GRAPH_DECLARE_GLOBALS
#include "snis_graph.h"
#undef SNIS_GRAPH_DECLARE_GLOBALS

#include "liang-barsky.h"
#include "bline.h"
#include "build_bug_on.h"
#include "mtwist.h"
#include "mathutils.h"
#include "graph_dev.h"
#include "ui_colors.h"
#include "string-utils.h"
#include "png_utils.h"

#define TOTAL_COLORS (NCOLORS + NSPARKCOLORS + NRAINBOWCOLORS + NSHADESOFGRAY * (NSHADECOLORS + 1) + \
	(NGRADIENTS * NTOTAL_GRADIENT_SHADES) + MAX_USER_COLORS)
struct graph_dev_color huex[TOTAL_COLORS];

/* first index of user colors: Note this will break if user colors
 * aren't at the end of the huex array
 */
#define USER_COLOR ((sizeof(huex) / sizeof(huex[0])) - MAX_USER_COLORS)
int nuser_colors;

extern struct my_vect_obj **gamefont[];
extern int font_scale[];
extern int letter_spacing[];
extern int font_lineheight[];
static int sng_font_family = 1;

struct snis_graph_viewport {
	int x_offset, y_offset, width, height;
};

static struct snis_graph_context {
	float xscale, yscale;
	struct liang_barsky_clip_window c;
	int extent_width, extent_height; /* size of drawing area used in snis_draw_* functions */
	int screen_width, screen_height; /* size of screen in pixels */
	struct snis_graph_viewport vp_3d; /* in extent coords */
	int hue; /* current color, index into huex[] and glhue[] */

	int has_scale;
	int has_viewport;
} sgc;

/* size of the extent we draw to with the drawing commands */
void sng_set_extent_size(int width, int height)
{
	sgc.extent_width = width;
	sgc.extent_height = height;
}

/* pixel size of the target screen */
void sng_set_screen_size(int width, int height)
{
	sgc.screen_width = width;
	sgc.screen_height = height;
	graph_dev_set_screen_size(width, height);

	sgc.xscale = (float)sgc.screen_width / (float)sgc.extent_width;
	sgc.yscale = (float)sgc.screen_height / (float)sgc.extent_height;

	/* sgc.has_scale = (sgc.screen_width != sgc.extent_width || sgc.screen_height != sgc.extent_height); */

	
	/* hack in fixed aspect ratio for now */
	if (sgc.yscale < sgc.xscale)
		sgc.xscale = sgc.yscale;
	else
		sgc.yscale = sgc.xscale;

	sgc.has_scale = 1;
	graph_dev_set_extent_scale(sgc.xscale, sgc.yscale);
	/* update the viewport in graph_dev as they are in screen coords */
	graph_dev_set_3d_viewport(sgc.vp_3d.x_offset * sgc.xscale, sgc.vp_3d.y_offset * sgc.yscale,
					sgc.vp_3d.width * sgc.xscale, sgc.vp_3d.height * sgc.yscale);
}

void sng_set_3d_viewport(int x_offset, int y_offset, int width, int height)
{
	sgc.vp_3d.x_offset = x_offset;
	sgc.vp_3d.y_offset = y_offset;
	sgc.vp_3d.width = width;
	sgc.vp_3d.height = height;

	graph_dev_set_3d_viewport(x_offset * sgc.xscale, y_offset * sgc.yscale,
					width * sgc.xscale, height * sgc.yscale);
}

void sng_set_scale(float xscale, float yscale)
{
	sgc.xscale = xscale;
	sgc.yscale = yscale;
}

void sng_set_clip_window(int x1, int y1, int x2, int y2)
{
	sgc.c.x1 = x1;
	sgc.c.y1 = y1;
	sgc.c.x2 = x2;
	sgc.c.y2 = y2;
}

void sng_current_draw_line(float x1, float y1, float x2, float y2)
{
	if (!clip_line(&sgc.c, &x1, &y1, &x2, &y2))
		return;
	graph_dev_draw_line(x1 * sgc.xscale, y1 * sgc.yscale, x2 * sgc.xscale, y2 * sgc.yscale);
}

void sng_current_draw_thick_line(float x1, float y1, float x2, float y2)
{
	float sx1, sy1, sx2, sy2, dx, dy;

	if (!clip_line(&sgc.c, &x1, &y1, &x2, &y2))
		return;

	if (fabs(x1 - x2) > fabs(y1 - y2)) {
		dx = 0;
		dy = 1;
	} else {
		dx = 1;
		dy = 0;
	}
	sx1 = x1 * sgc.xscale;
	sx2 = x2 * sgc.xscale;
	sy1 = y1 * sgc.yscale;	
	sy2 = y2 * sgc.yscale;	
	
	graph_dev_draw_line(sx1, sy1, sx2, sy2);
	graph_dev_draw_line(sx1 - dx, sy1 - dy, sx2 - dx, sy2 - dy);
	graph_dev_draw_line(sx1 + dx, sy1 + dy, sx2 + dx, sy2 + dy);
}

static int clip_rectangle(float *x, float *y, float *width, float *height)
{
	float x2, y2;

	x2 = *x + *width;
	y2 = *y + *height;
	if (x2 < sgc.c.x1)
		return 0;
	if (*x > sgc.c.x2)
		return 0;
	if (y2 < sgc.c.y1)
		return 0;
	if (*y > sgc.c.y2)
		return 0;

	if (*x < sgc.c.x1)
		*x = sgc.c.x1;
	if (x2 > sgc.c.x2)
		x2 = sgc.c.x2;
	if (*y < sgc.c.y1)
		*y = sgc.c.y1;
	if (y2 > sgc.c.y2)
		y2 = sgc.c.y2;

	*width = x2 - *x;
	*height = y2 - *y;
	return 1;
}

void sng_current_draw_rectangle(int filled, float x, float y, float width, float height)
{
	if (!clip_rectangle(&x, &y, &width, &height))
		return;
	graph_dev_draw_rectangle(filled, x * sgc.xscale, y * sgc.yscale,
		width * sgc.xscale, height * sgc.yscale);
}

void sng_current_draw_bright_line(float x1, float y1, float x2, float y2, int color)
{
	float sx1, sy1, sx2, sy2, dx, dy;

	if (fabs(x1 - x2) > fabs(y1 - y2)) {
		dx = 0;
		dy = 1;
	} else {
		dx = 1;
		dy = 0;
	}

	if (!clip_line(&sgc.c, &x1, &y1, &x2, &y2))
		return;

	sx1 = x1 * sgc.xscale;
	sx2 = x2 * sgc.xscale;
	sy1 = y1 * sgc.yscale;	
	sy2 = y2 * sgc.yscale;	

	sng_set_foreground(WHITE);	
	graph_dev_draw_line(sx1, sy1, sx2, sy2);
	sng_set_foreground(color);
	graph_dev_draw_line(sx1 - dx, sy1 - dy, sx2 - dx, sy2 - dy);
	graph_dev_draw_line(sx1 + dx, sy1 + dy, sx2 + dx, sy2 + dy);
}

void sng_current_draw_arc(int filled, float x, float y, float width, float height, float angle1, float angle2)
{
	graph_dev_draw_arc(filled, x * sgc.xscale, y * sgc.yscale, width * sgc.xscale,
				height * sgc.yscale, angle1, angle2);
}

void sng_dotted_line_plot_func(int x, int y, void *context)
{
	struct sng_dotted_plot_func_context *c = context;

	c->i = (c->i + 1) % 10;
	if (c->i != 0)
		return;
	graph_dev_draw_point(x, y);
}

void sng_electric_line_plot_func(int x, int y, __attribute__((unused)) void *context)
{
	if (snis_randn(100) < 10)
		graph_dev_draw_point(x, y);
}

static void sng_bright_electric_line_plot_func(int x, int y, void *context)
{
	struct sng_dotted_plot_func_context *c = context;

	if (snis_randn(100) < 20) {
		sng_set_foreground(c->i);
		graph_dev_draw_point(x, y);
	}
}

void sng_draw_dotted_line(float x1, float y1, float x2, float y2)
{
	struct sng_dotted_plot_func_context context;

	context.i = 0;

	if (!clip_line(&sgc.c, &x1, &y1, &x2, &y2))
		return;

	bline(x1 * sgc.xscale, y1 * sgc.yscale, x2 * sgc.xscale, y2 * sgc.yscale,
			sng_dotted_line_plot_func, &context);
}

void sng_draw_electric_line(float x1, float y1, float x2, float y2)
{
	struct sng_dotted_plot_func_context context;

	context.i = 0;

	if (!clip_line(&sgc.c, &x1, &y1, &x2, &y2))
		return;

	bline(x1 * sgc.xscale, y1 * sgc.yscale, x2 * sgc.xscale, y2 * sgc.yscale,
			sng_electric_line_plot_func, &context);
}

static void sng_draw_bright_white_electric_line(float x1, float y1, float x2, float y2, int color)
{
	struct sng_dotted_plot_func_context context;

	context.i = color;

	if (!clip_line(&sgc.c, &x1, &y1, &x2, &y2))
		return;

	bline(x1 * sgc.xscale, y1 * sgc.yscale, x2 * sgc.xscale, y2 * sgc.yscale,
			sng_bright_electric_line_plot_func, &context);
}

void sng_draw_laser_line(float x1, float y1, float x2, float y2, int color)
{
	float dx, dy;

	if (fabs(x1 - x2) > fabs(y1 - y2)) {
		dx = 0;
		dy = 1;
	} else {
		dx = 1;
		dy = 0;
	}

	if (!clip_line(&sgc.c, &x1, &y1, &x2, &y2))
		return;

	sng_draw_bright_white_electric_line(x1, y1, x2, y2, color);
	sng_set_foreground(color);
	sng_draw_electric_line(x1 - dx, y1 - dy, x2 - dx, y2 - dy);
	sng_draw_electric_line(x1 + dx, y1 + dy, x2 + dx, y2 + dy);
}

void sng_draw_vect_obj(struct my_vect_obj *v, float x, float y)
{
	int i;
	float x1, y1, x2, y2;

	for (i = 0; i < v->npoints-1; i++) {
		if (v->p[i+1].x == LINE_BREAK)
			i += 2;
		x1 = x + v->p[i].x;
		y1 = y + v->p[i].y;
		x2 = x + v->p[i + 1].x;
		y2 = y + v->p[i + 1].y;
		sng_current_draw_line(x1, y1, x2, y2);
	}
}

/* Draws a letter in the given font at an absolute x,y coords on the screen. */
float sng_abs_xy_draw_letter(struct my_vect_obj **font, unsigned char letter, float x, float y)
{
	int i;
	float x1, y1, x2, y2;
	float minx, maxx, diff;
	int underlined = (letter & 0x80); /* high bit set indicates underlined char */

	letter = (letter & ~0x80); /* mask out high bit */
	assert(font['_'] != NULL); /* prevent scan-build from complaining about null ptr deref */
	if (letter == ' ' || letter == '\n' || letter == '\t' || font[letter] == NULL)
		return abs(font['_']->p[0].x - font['_']->p[1].x);

	minx = x + font[letter]->p[0].x;
	maxx = minx;

	if (underlined)
		(void) sng_abs_xy_draw_letter(font, '_', x, y);
	for (i = 0; i < font[letter]->npoints-1; i++) {
		if (font[letter]->p[i+1].x == LINE_BREAK)
			i += 2;
		x1 = x + font[letter]->p[i].x;
		y1 = y + font[letter]->p[i].y;
		x2 = x + font[letter]->p[i + 1].x;
		y2 = y + font[letter]->p[i + 1].y;

		if (x1 < minx)
			minx = x1;
		if (x2 < minx)
			minx = x2;
		if (x1 > maxx)
			maxx = x1;
		if (x2 > maxx)
			maxx = x2;
		
		if (x1 > 0 && x2 > 0)
			sng_current_draw_line(x1, y1, x2, y2);
	}
	diff = fabs(maxx - minx);
	/* if (diff == 0)
		return (abs(font['_']->p[0].x - font['_']->p[1].x) / 4); */
	return diff; 
}

static float crawl3d_transform_x(float x, float y)
{
	x = (1800.0 * (x - sgc.screen_width / 2.0) / (2000.0 - y)) + sgc.screen_width / 2.0;
	return x;
}

static float crawl3d_transform_y(float y)
{
	y = (1300.0 * (y - sgc.screen_height / 2.0) / (3000.0 - y)) + 7.0 * sgc.screen_height / 8.0;
	return y;
}

float sng_abs_xz_draw_letter(struct my_vect_obj **font, unsigned char letter, float x, float y)
{
	int i;
	float x1, y1, x2, y2;
	float minx, maxx, diff;

	if (letter == ' ' || letter == '\n' || letter == '\t' || font[letter] == NULL)
		return abs(font['_']->p[0].x - font['_']->p[1].x);

	minx = x + font[letter]->p[0].x;
	maxx = minx;
	for (i = 0; i < font[letter]->npoints-1; i++) {
		if (font[letter]->p[i+1].x == LINE_BREAK)
			i += 2;
		x1 = x + font[letter]->p[i].x;
		y1 = y + font[letter]->p[i].y;
		x2 = x + font[letter]->p[i + 1].x;
		y2 = y + font[letter]->p[i + 1].y;

		if (x1 < minx)
			minx = x1;
		if (x2 < minx)
			minx = x2;
		if (x1 > maxx)
			maxx = x1;
		if (x2 > maxx)
			maxx = x2;

		x1 = crawl3d_transform_x(x1, y1);
		y1 = crawl3d_transform_y(y1);
		x2 = crawl3d_transform_x(x2, y2);
		y2 = crawl3d_transform_y(y2);

		if (x1 > 0 && x2 > 0 && y1 > 0 && y2 > 0)
			sng_current_draw_line(x1, y1, x2, y2);
	}
	diff = fabs(maxx - minx);
	/* if (diff == 0)
		return (abs(font['_']->p[0].x - font['_']->p[1].x) / 4); */
	return diff;
}

/* Used for floating labels in the game. */
/* Draws a string at an absolute x,y position on the screen. */ 
void sng_abs_xy_draw_string(char *s, int font, float x, float y)
{

	int i, dx;	
	float deltax = 0;

	for (i = 0; s[i]; i++) {
		dx = (letter_spacing[font]) + sng_abs_xy_draw_letter(gamefont[font + 5 * sng_font_family],
			s[i], x + deltax, y);
		if (s[i] == '\n') {
			deltax = 0;
			dx = 0;
			y += font_lineheight[font];
		}
		deltax += dx;
	}
}

void sng_string_bounding_box(char *s, int font, float *bbx1, float *bby1, float *bbx2, float *bby2)
{
	struct my_vect_obj **fontobj = gamefont[font + 5 * sng_font_family];
	int i;
	float extra_vert_space = 0;
	float max_x_extent;

	max_x_extent = 0;
	*bbx2 = *bby1 = *bby2 = 0;

	for (i = 0; s[i]; i++) {
		unsigned char letter = s[i];
		if (letter == '\n') {
			extra_vert_space += font_lineheight[font];
			*bbx2 = 0;
			continue;
		}
		if (letter == ' ' || letter == '\n' || letter == '\t' || fontobj[letter] == NULL) {
			letter = '_';
		}

		/* figure out the letter size based on the bounding box */
		*bbx2 += fontobj[letter]->bbx2 - fontobj[letter]->bbx1;
		if (i != 0)
			*bbx2 += letter_spacing[font]; /* add between character space */
		if (*bbx2 > max_x_extent)
			max_x_extent = *bbx2;

		if (i == 0 || fontobj[letter]->bby1 < *bby1)
			*bby1 = fontobj[letter]->bby1;
		if (i == 0 || fontobj[letter]->bby2 > *bby2)
			*bby2 = fontobj[letter]->bby2;
	}
	*bbx1 = 0;
	if (*bbx2 < max_x_extent)
		*bbx2 = max_x_extent;
	*bby2 += extra_vert_space;
}

/* Used for floating labels in the game. */
/* Draws a string centered at x,y position on the screen. */
void sng_center_xy_draw_string(char *s, int font, float x, float y)
{
	float bbx1, bby1, bbx2, bby2;
	sng_string_bounding_box(s, font, &bbx1, &bby1, &bbx2, &bby2);

	float ox = x - (bbx2 + bbx1)/2.0;
	float oy = y - (bby2 + bby1)/2.0;

	int i, dx;
	float deltax = 0;

	for (i=0;s[i];i++) {
		dx = (letter_spacing[font]) + sng_abs_xy_draw_letter(gamefont[font + 5 * sng_font_family],
			s[i], ox + deltax, oy);
		deltax += dx;
	}
}

void sng_center_xz_draw_string(char *s, int font, float x, float y)
{
	float bbx1, bby1, bbx2, bby2;
	sng_string_bounding_box(s, font, &bbx1, &bby1, &bbx2, &bby2);

	float ox = x - (bbx2 + bbx1)/2.0;
	float oy = y - (bby2 + bby1)/2.0;

	int i, dx;
	float deltax = 0;

	for (i = 0; s[i]; i++) {
		dx = (letter_spacing[font]) + sng_abs_xz_draw_letter(gamefont[font + 5 * sng_font_family],
			s[i], ox + deltax, oy);
		deltax += dx;
	}
}

void sng_abs_xy_draw_string_with_cursor(char *s, int font, float x, float y, int cursor_pos, int cursor_on)
{

	int i;
	float dx;	
	float deltax = 0;

	if (!cursor_on) {
		sng_abs_xy_draw_string(s, font, x, y);
		return;
	}

	for (i = 0; s[i]; i++) {
		if (i == cursor_pos)
			sng_abs_xy_draw_letter(gamefont[font + 5 * sng_font_family], '_', x + deltax, y);
		dx = (letter_spacing[font]) + sng_abs_xy_draw_letter(gamefont[font + 5 * sng_font_family],
			s[i], x + deltax, y);
		deltax += dx;
	}
	if (i == cursor_pos)
		sng_abs_xy_draw_letter(gamefont[font + 5 * sng_font_family], '_', x + deltax, y);
}

void sng_draw_point(float x, float y)
{
	graph_dev_draw_point(x * sgc.xscale, y * sgc.yscale);
}

/* from http://stackoverflow.com/a/6930407
0 <= h < 360, 0 <= s <= 1, , 0 <= v <= 1 */
static void hsv2rgb(double h, double s, double v, struct graph_dev_color* rgb)
{
    double      hh, p, q, t, ff;
    long        i;

    if(s <= 0.0) {       // < is bogus, just shuts up warnings
        rgb->red = v * 65535;
        rgb->green = v * 65535;
        rgb->blue = v * 65535;
        return;
    }
    hh = h;
    if(hh >= 360.0) hh = 0.0;
    hh /= 60.0;
    i = (long)hh;
    ff = hh - i;
    p = v * (1.0 - s);
    q = v * (1.0 - (s * ff));
    t = v * (1.0 - (s * (1.0 - ff)));

    switch(i) {
    case 0:
        rgb->red = v * 65535;
        rgb->green = t * 65535;
        rgb->blue = p * 65535;
        break;
    case 1:
        rgb->red = q * 65535;
        rgb->green = v * 65535;
        rgb->blue = p * 65535;
        break;
    case 2:
        rgb->red = p * 65535;
        rgb->green = v * 65535;
        rgb->blue = t * 65535;
        break;

    case 3:
        rgb->red = p * 65535;
        rgb->green = q * 65535;
        rgb->blue = v * 65535;
        break;
    case 4:
        rgb->red = t * 65535;
        rgb->green = p * 65535;
        rgb->blue = v * 65535;
        break;
    case 5:
    default:
        rgb->red = v * 65535;
        rgb->green = p * 65535;
        rgb->blue = q * 65535;
        break;
    }
}

int BLUE = 0;
int GREEN = 0;
int CYAN = 0;

struct gradient_color {
	int *color_index;
	double h;
	double s;
	double v;
};
static struct gradient_color gradient_colors[] = {
	{&BLUE, 240, 1, 1},
	{&GREEN, 120, 1, 1},
	{&CYAN, 180, 1, 1}
};

int sng_add_user_color(uint8_t r, uint8_t g, uint8_t b)
{
	/* Check and see if color is already known */
	int i;
	uint16_t r2, g2, b2;
	r2 = r << 8;
	g2 = g << 8;
	b2 = b << 8;

	for (i = 0; i < TOTAL_COLORS; i++) {
		if (huex[i].red == r2 &&
			huex[i].green == g2 &&
			huex[i].blue == b2) {
			return i;
		}
	}

	/* color is not known, need to try to add it. */
	if (MAX_USER_COLORS - nuser_colors <= 0)
		return -1; /* no room to add it */

	/* add it */
	i = USER_COLOR + nuser_colors;
	huex[i].red = (uint16_t) r2;
	huex[i].green = (uint16_t) g2;
	huex[i].blue = (uint16_t) b2;
	nuser_colors++;
	return nuser_colors - 1 + USER_COLOR;
}

static struct user_color_entry {
	char name[20];
	int index;
} *user_color;
static int nuser_color_entries;

static int lookup_user_color(char *colorname)
{
	int i;

	for (i = 0; i < nuser_color_entries; i++)
		if (strcmp(user_color[i].name, colorname) == 0)
			return user_color[i].index;
	return -1;
}

static int parse_user_color_line(char *filename, char *line, int ln,
					struct user_color_entry *e)
{
	int rc;
	unsigned int r, g, b;
	char name[100], ui_component[100], colorname[100];

	if (line[0] == '#')
		return 1; /* comment */

	clean_spaces(line);
	remove_trailing_whitespace(line);

	if (strcmp(line, "") == 0)
		return 1; /* comment (blank line) */

	memset(name, 0, sizeof(name));
	rc = sscanf(line, "color %[^	 ] #%02x%02x%02x",
			name, &r, &g, &b);
	if (rc == 4) {
		name[19] = '\0';
		memcpy(e->name, name, 20);
		e->index = sng_add_user_color(r, g, b);
		if (e->index < 0) {
			fprintf(stderr, "%s:%d Failed to add user color '%s'\n",
				filename, ln, name);
			return -1;
		}
		return 0;
	} else {
		rc = sscanf(line, "%[^	 ] #%02x%02x%02x",
				ui_component, &r, &g, &b);
		if (rc == 4) {
			e->index = sng_add_user_color(r, g, b);
			if (e->index < 0) {
				fprintf(stderr, "%s:%d Failed to modify ui component color '%s'\n",
					filename, ln, line);
				return -1;
			}
			modify_ui_color(ui_component, e->index);
			return 0;
		} else {
			rc = sscanf(line, "%[^	 ] %[^	 ]",
				ui_component, colorname);
			if (rc == 2) {
				e->index = lookup_user_color(colorname);
				if (e->index < 0) {
					fprintf(stderr,
						"%s:%d Failed to modify ui component color '%s'\n",
						filename, ln, line);
					return -1;
				}
				modify_ui_color(ui_component, e->index);
				return 0;
			}
		}
	}
	fprintf(stderr, "%s:%d: Syntax error: '%s'\n", filename, ln, line);
	return -1;
}

static void sng_read_user_colors(char *filename)
{
	FILE *f;
	char line[1000], *l;
	int rc, ln = 0;
	struct user_color_entry entry;

	if (!filename)
		return;

	user_color = malloc(sizeof(*user_color) * MAX_USER_COLORS);
	memset(user_color, 0, sizeof(*user_color) * MAX_USER_COLORS);

	f = fopen(filename, "r");
	if (!f) {
		fprintf(stderr, "Cannot open %s: %s\n", filename, strerror(errno));
		goto bailout;
	}
	while (!feof(f)) {
		l = fgets(line, 1000, f);
		if (!l)
			break;
		if (strlen(line) == 0)
			continue;
		line[strlen(line) - 1] = '\0';
		ln++;
		rc = parse_user_color_line(filename, line, ln, &entry);
		switch (rc) {
		case 0:
			user_color[nuser_color_entries] = entry;
			nuser_color_entries++;
			continue;
		case 1: /* comment */
			continue;
		case -1: /* error of some kind */
			continue;
		}
	}

bailout:
	if (f)
		fclose(f);
	if (user_color)
		free(user_color);
	return;
}

void sng_setup_colors(char *user_color_file)
{
	int i;

	BUILD_ASSERT(ARRAYSIZE(gradient_colors) == NGRADIENTS);

	/* values extracted from gdk_color_parse() ages ago back when we used GTK. */
	huex[WHITE].red = 65535;
	huex[WHITE].green = 65535;
	huex[WHITE].blue = 65535;

	huex[BLACK].red = 0;
	huex[BLACK].green = 0;
	huex[BLACK].blue = 0;

	huex[LIMEGREEN].red = 12850;
	huex[LIMEGREEN].green = 52685;
	huex[LIMEGREEN].blue = 12850;

	huex[DARKGREEN].red = 0;
	huex[DARKGREEN].green = 25700;
	huex[DARKGREEN].blue = 0;

	huex[YELLOW].red = 65535;
	huex[YELLOW].green = 65535;
	huex[YELLOW].blue = 0;

	huex[RED].red = 65535;
	huex[RED].green = 0;
	huex[RED].blue = 0;

	huex[ORANGE].red = 65535;
	huex[ORANGE].green = 42405;
	huex[ORANGE].blue = 0;

	huex[MAGENTA].red = 65535;
	huex[MAGENTA].green = 0;
	huex[MAGENTA].blue = 65535;

	huex[DARKRED].red = 35723;
	huex[DARKRED].green = 0;
	huex[DARKRED].blue = 0;

	huex[AMBER].red = 65535;
	huex[AMBER].green = 42405;
	huex[AMBER].blue = 0;

	huex[DARKTURQUOISE].red = 0;
	huex[DARKTURQUOISE].green = 52942;
	huex[DARKTURQUOISE].blue = 53713;

	huex[ORANGERED].red = 65535;
	huex[ORANGERED].green = 17733;
	huex[ORANGERED].blue = 0;

	huex[LIGHT_AMBER].red = 65535;
	huex[LIGHT_AMBER].green = 230 * 256;
	huex[LIGHT_AMBER].blue = 131 * 256;

	for (i = 0; i < NSHADESOFGRAY; i++) {
		huex[GRAY + i].red = (i * 32767 * 2) / 256;
		huex[GRAY + i].green = (i * 32767 * 2) / 256;
		huex[GRAY + i].blue = (i * 32767 * 2) / 256;
	}

	for (i = 1; i <= NSHADECOLORS; i++) {
		int j, r, g, b;

		r = snis_randn(32767); 
		g = snis_randn(32767); 
		b = snis_randn(32767); 

		for (j = 0; j < NSHADESOFGRAY / 2; j++) { 
			int index;
			float f;

			f = (float) j / (float) (NSHADESOFGRAY / 2.0);

			index = GRAY + (i * NSHADESOFGRAY) + j;
			huex[index].red = (f * (float) r);
			huex[index].green = (f * (float) g); 
			huex[index].blue = (f * (float) b); 
		}

		for (j = NSHADESOFGRAY / 2; j < NSHADESOFGRAY; j++) {
			int index;
			float f;

			f = (float) (j - NSHADESOFGRAY / 2) / (float) NSHADESOFGRAY / 2.0;

			index = GRAY + (i * NSHADESOFGRAY) + j;
			huex[index].red = r + (f * ((32767.0 * 2.0) - (float) r));
			huex[index].green = g + (f * ((32767.0 * 2.0) - (float) g)); 
			huex[index].blue = b + (f * ((32767.0 * 2.0) - (float) b)); 
		}
	}

	int grad_index = GRADIENTS;

	for (i=0; i<NGRADIENTS; i++ ) {
		int j;
		double h = gradient_colors[i].h;
		double s = gradient_colors[i].s;
		double v = gradient_colors[i].v;

		/* add the shades from black to color */
		for (j=0; j<NGRADIENT_SHADES; j++) {
			double f = j/(double)NGRADIENT_SHADES;
			double fi = 1.0 - f;
			hsv2rgb(h, s + (1.0-s)*fi, v * f, &huex[grad_index]);
			grad_index++;
		}

		/* add the pure color */
		hsv2rgb(h, s, v, &huex[grad_index]);
		*gradient_colors[i].color_index = grad_index;
		grad_index++;

		/* add the shades from color to white */
		for (j=1; j<=NGRADIENT_SHADES; j++) {
			double f = (NGRADIENT_SHADES-j)/(double)NGRADIENT_SHADES;
			double fi = 1.0 - f;
			hsv2rgb(h, s * f, v + (1.0-v)*fi, &huex[grad_index]);
			grad_index++;
		}
	}

	sng_read_user_colors(user_color_file);

	fixup_ui_color(BLUE_FIXUP, BLUE);
	fixup_ui_color(GREEN_FIXUP, GREEN);
	fixup_ui_color(CYAN_FIXUP, CYAN);
}

void sng_set_foreground_alpha(int c, float a)
{
	sgc.hue = c;
	graph_dev_set_color(&huex[c], a);
}

void sng_set_foreground(int c)
{
	if (c < 0)
		return; /* TODO: find out why snis_limited_client calls this with negative color */
	sgc.hue = c;
	graph_dev_set_color(&huex[c], -1);
}

struct sng_color sng_get_foreground()
{
	struct sng_color color;
	color.red = huex[sgc.hue].red / 65535.0;
	color.green = huex[sgc.hue].green / 65535.0;
	color.blue = huex[sgc.hue].blue / 65535.0;
	return color;
}

struct sng_color sng_get_color(int c)
{
	struct sng_color color;
	color.red = huex[c].red / 65535.0;
	color.green = huex[c].green / 65535.0;
	color.blue = huex[c].blue / 65535.0;
	return color;
}

void sng_draw_circle(int filled, float x, float y, float r)
{
	sng_current_draw_arc(filled, x - r, y - r, r * 2, r * 2, 0, 2.0*M_PI);
}

/* convert from physical (e.g. mouse) coords to extent coords */
float sng_pixelx_to_screenx(float x)
{
	return x / sgc.xscale;
}

float sng_pixely_to_screeny(float y)
{
	return y / sgc.yscale;
}

void sng_set_font_family(int family)
{
	sng_font_family = (family % 3);
}

int sng_get_font_family(void)
{
	return sng_font_family;
}
