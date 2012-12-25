#include <stdlib.h>
#include <gtk/gtk.h>

#include "snis_font.h"
#define SNIS_GRAPH_DECLARE_GLOBALS
#include "snis_graph.h"
#undef SNIS_GRAPH_DECLARE_GLOBALS

#include "bline.h"

/* cardinal color indexes into huex array */
#define WHITE 0
#define BLUE 1
#define BLACK 2
#define GREEN 3
#define YELLOW 4
#define RED 5
#define ORANGE 6
#define CYAN 7
#define MAGENTA 8
#define DARKGREEN 9
#define DARKRED 10
#define AMBER 11
#define LIMEGREEN 12

#define NCOLORS 13              /* number of "cardinal" colors */
#define NSPARKCOLORS 25         /* 25 shades from yellow to red for the sparks */
#define NRAINBOWSTEPS (16)
#define NRAINBOWCOLORS (NRAINBOWSTEPS*3)

extern GdkColor huex[];

extern struct my_vect_obj **gamefont[];
extern int font_scale[];

static struct snis_graph_context {
	float xscale, yscale;
} sgc;

static int sng_rand(int n)
{
	return rand() % n;
}

void sng_set_scale(float xscale, float yscale)
{
	sgc.xscale = xscale;
	sgc.yscale = yscale;
}

void sng_scaled_line(GdkDrawable *drawable,
        GdkGC *gc, gint x1, gint y1, gint x2, gint y2)
{
        gdk_draw_line(drawable, gc, x1 * sgc.xscale, y1 * sgc.yscale,
                x2 * sgc.xscale, y2 * sgc.yscale);
}

void sng_thick_scaled_line(GdkDrawable *drawable,
	GdkGC *gc, gint x1, gint y1, gint x2, gint y2)
{
	int sx1, sy1, sx2, sy2, dx, dy;

	if (abs(x1 - x2) > abs(y1 - y2)) {
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
	
	gdk_draw_line(drawable, gc, sx1, sy1, sx2, sy2);
	gdk_draw_line(drawable, gc, sx1 - dx, sy1 - dy, sx2 - dx, sy2 - dy);
	gdk_draw_line(drawable, gc, sx1 + dx, sy1 + dy, sx2 + dx, sy2 + dy);
}

void sng_scaled_rectangle(GdkDrawable *drawable,
	GdkGC *gc, gboolean filled, gint x, gint y, gint width, gint height)
{
	gdk_draw_rectangle(drawable, gc, filled, x * sgc.xscale, y * sgc.yscale,
		width * sgc.xscale, height * sgc.yscale);
}
void sng_scaled_bright_line(GdkDrawable *drawable,
	GdkGC *gc, gint x1, gint y1, gint x2, gint y2, int color)
{
	int sx1, sy1, sx2, sy2, dx, dy;

	if (abs(x1 - x2) > abs(y1 - y2)) {
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
	
	gdk_gc_set_foreground(gc, &huex[WHITE]);
	gdk_draw_line(drawable, gc, sx1, sy1, sx2, sy2);
	gdk_gc_set_foreground(gc, &huex[color]);
	gdk_draw_line(drawable, gc, sx1 - dx, sy1 - dy, sx2 - dx, sy2 - dy);
	gdk_draw_line(drawable, gc, sx1 + dx, sy1 + dy, sx2 + dx, sy2 + dy);
}

void sng_unscaled_bright_line(GdkDrawable *drawable,
	GdkGC *gc, gint x1, gint y1, gint x2, gint y2, int color)
{
	int dx,dy;

	if (abs(x1-x2) > abs(y1-y2)) {
		dx = 0;
		dy = 1;
	} else {
		dx = 1;
		dy = 0;
	}
	
	gdk_gc_set_foreground(gc, &huex[WHITE]);
	gdk_draw_line(drawable, gc, x1,y1,x2,y2);
	gdk_gc_set_foreground(gc, &huex[color]);
	gdk_draw_line(drawable, gc, x1-dx,y1-dy,x2-dx,y2-dy);
	gdk_draw_line(drawable, gc, x1+dx,y1+dy,x2+dx,y2+dy);
}

void sng_scaled_arc(GdkDrawable *drawable, GdkGC *gc,
	gboolean filled, gint x, gint y, gint width, gint height, gint angle1, gint angle2)
{
	gdk_draw_arc(drawable, gc, filled, x * sgc.xscale, y * sgc.yscale,
			width * sgc.xscale, height * sgc.yscale, angle1, angle2);
}

void sng_use_unscaled_drawing_functions(void)
{
	sng_current_draw_line = gdk_draw_line;
	sng_current_draw_rectangle = gdk_draw_rectangle;
	sng_current_bright_line = sng_unscaled_bright_line;
	sng_current_draw_arc = gdk_draw_arc;
}

void sng_use_scaled_drawing_functions(void)
{
	sng_current_draw_line = sng_scaled_line;
	sng_current_draw_rectangle = sng_scaled_rectangle;
	sng_current_bright_line = sng_scaled_bright_line;
	sng_current_draw_arc = sng_scaled_arc;
}

void sng_use_thick_lines(void)
{
	sng_current_draw_line = sng_thick_scaled_line;
}

void sng_dotted_line_plot_func(int x, int y, void *context)
{
	struct sng_dotted_plot_func_context *c = context;

	c->i = (c->i + 1) % 10;
	if (c->i != 0)
		return;
	gdk_draw_point(c->drawable, c->gc, x, y);
}

void sng_electric_line_plot_func(int x, int y, void *context)
{
	struct sng_dotted_plot_func_context *c = context;

	if (sng_rand(100) < 10)
		gdk_draw_point(c->drawable, c->gc, x, y);
}

void sng_draw_dotted_line(GdkDrawable *drawable,
	GdkGC *gc, gint x1, gint y1, gint x2, gint y2)
{
	struct sng_dotted_plot_func_context context;

	context.drawable = drawable;
	context.gc = gc;
	context.i = 0;

	bline(x1 * sgc.xscale, y1 * sgc.yscale, x2 * sgc.xscale, y2 * sgc.yscale,
			sng_dotted_line_plot_func, &context);
}

void sng_draw_electric_line(GdkDrawable *drawable,
	GdkGC *gc, gint x1, gint y1, gint x2, gint y2)
{
	struct sng_dotted_plot_func_context context;

	context.drawable = drawable;
	context.gc = gc;
	context.i = 0;

	bline(x1 * sgc.xscale, y1 * sgc.yscale, x2 * sgc.xscale, y2 * sgc.yscale,
			sng_electric_line_plot_func, &context);
}

/* Draws a letter in the given font at an absolute x,y coords on the screen. */
int sng_abs_xy_draw_letter(GtkWidget *w, GdkGC *gc, struct my_vect_obj **font, 
		unsigned char letter, int x, int y)
{
	int i, x1, y1, x2, y2;
	int minx, maxx, diff;

	if (letter == ' ' || letter == '\n' || letter == '\t' || font[letter] == NULL)
		return abs(font['Z']->p[0].x - font['Z']->p[1].x);

	for (i = 0; i < font[letter]->npoints-1; i++) {
		if (font[letter]->p[i+1].x == LINE_BREAK)
			i += 2;
		x1 = x + font[letter]->p[i].x;
		y1 = y + font[letter]->p[i].y;
		x2 = x + font[letter]->p[i + 1].x;
		y2 = y + font[letter]->p[i + 1].y;

		if (i == 0) {
			minx = x1;
			maxx = x1;
		}

		if (x1 < minx)
			minx = x1;
		if (x2 < minx)
			minx = x2;
		if (x1 > maxx)
			maxx = x1;
		if (x2 > maxx)
			maxx = x2;
		
		if (x1 > 0 && x2 > 0)
			sng_current_draw_line(w->window, gc, x1, y1, x2, y2); 
	}
	diff = abs(maxx - minx);
	/* if (diff == 0)
		return (abs(font['Z']->p[0].x - font['Z']->p[1].x) / 4); */
	return diff; 
}

/* Used for floating labels in the game. */
/* Draws a string at an absolute x,y position on the screen. */ 
void sng_abs_xy_draw_string(GtkWidget *w, GdkGC *gc, char *s, int font, int x, int y) 
{

	int i, dx;	
	int deltax = 0;

	for (i=0;s[i];i++) {
		dx = (font_scale[font]) + sng_abs_xy_draw_letter(w, gc, gamefont[font], s[i], x + deltax, y);  
		deltax += dx;
	}
}

