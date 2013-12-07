#ifndef SNIS_GAUGE_H__
#define SNIS_GAUGE_H__

#ifdef DEFINE_SNIS_GAUGE_GLOBALS
#define GLOBAL
#else
#define GLOBAL extern
#endif

typedef double (*gauge_monitor_function)(void);

struct gauge;

GLOBAL void gauge_add_needle(struct gauge *g, gauge_monitor_function sample, int color);

GLOBAL struct gauge *gauge_init(int x, int y, int r, double r1, double r2,
			double start_angle, double angular_range,
			int needle_color, int dial_color, int ndivs, char *title,
			gauge_monitor_function gmf);
GLOBAL void draw_gauge_needle(GdkDrawable *drawable, GdkGC *gc,
		gint x, gint y, gint r, double a);
GLOBAL void gauge_fill_background(struct gauge *g, int bg);
GLOBAL void gauge_draw(GtkWidget *w, GdkGC *gc, struct gauge *g);
GLOBAL void gauge_free(struct gauge *g);

#endif
