#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <gtk/gtk.h>

#define SNIS_GAUGE_GLOBALS
#include "mtwist.h"
#include "mathutils.h"
#include "snis_font.h"
#include "snis_typeface.h"
#include "snis_graph.h"
#include "snis_gauge.h"

struct gauge {
	int x, y, r;
	gauge_monitor_function sample;
	gauge_monitor_function sample2;
	double r1,r2;
	double start_angle, angular_range;
	int needle_color, dial_color, needle_color2;
	int dial_font, label_font;
	int ndivs;
	char title[16]; 
	int bg_color;
	float bg_alpha;
	float multiplier;
};

void gauge_add_needle(struct gauge *g, gauge_monitor_function sample, int color)
{
	g->needle_color2 = color;
	g->sample2 = sample;
}

struct gauge *gauge_init(int x, int y, int r, double r1, double r2,
			double start_angle, double angular_range,
			int needle_color, int dial_color, int ndivs, char *title,
			gauge_monitor_function gmf)
{
	struct gauge *g;

	g = malloc(sizeof(*g));
	g->x = x;
	g->y = y;
	g->r = r;
	g->r1 = r1;
	g->r2 = r2;
	g->start_angle = start_angle;
	g->angular_range = angular_range;
	g->needle_color = needle_color;
	g->dial_color = dial_color;
	g->dial_font = NANO_FONT;
	g->label_font = TINY_FONT;
	if (g->r < 60) {
		g->dial_font = PICO_FONT;
		g->label_font = PICO_FONT;
	}
	g->ndivs = ndivs;
	g->sample = gmf;
	strncpy(g->title, title, sizeof(g->title) - 1);
	g->title[sizeof(g->title) - 1] = '\0';
	g->sample2 = NULL;
	g->bg_color = -1;
	g->multiplier = 1.0;

	return g;
}

static void draw_gauge_needle(
		float x, float y, float r, double a)
{
	int x1, y1, x2, y2, x3, y3, x4, y4;

	x1 = r *  sin(a) * 0.9 + x;
	y1 = r * -cos(a) * 0.9 + y;
	x2 = r * -sin(a) * 0.2 + x;
	y2 = r *  cos(a) * 0.2 + y;
	x3 = r *  sin(a + M_PI / 2.0) * 0.05 + x;
	y3 = r * -cos(a + M_PI / 2.0) * 0.05 + y;
	x4 = r *  sin(a - M_PI / 2.0) * 0.05 + x;
	y4 = r * -cos(a - M_PI / 2.0) * 0.05 + y;

	sng_current_draw_line(x1, y1, x3, y3);
	sng_current_draw_line(x3, y3, x2, y2);
	sng_current_draw_line(x2, y2, x4, y4);
	sng_current_draw_line(x4, y4, x1, y1);
}

void gauge_fill_background(struct gauge *g, int bg, float alpha)
{
	g->bg_color = bg;
	g->bg_alpha = alpha;
}

void gauge_draw(struct gauge *g)
{
	int i;
	double a, ai;
	int x1, y1, x2, y2, x3, y3;
	double value;
	double inc, v;
	char buffer[10], buf2[10];

	if (g->bg_color >= 0) {
		sng_set_foreground_alpha(g->bg_color, g->bg_alpha);
		sng_draw_circle(1, g->x, g->y, g->r);
	}
	sng_set_foreground(g->dial_color);
	sng_draw_circle(0, g->x, g->y, g->r);

	ai = g->angular_range / g->ndivs;
	normalize_angle(&ai);

	v = g->r1;
	inc = (double) (g->r2 - g->r1) / (double) g->ndivs;
	for (i = 0; i <= g->ndivs; i++) {
		a = (ai * (double) i) + g->start_angle;
		normalize_angle(&a);
		x1 = g->r * sin(a);
		x2 = 0.9 * x1;
		y1 = g->r * -cos(a);
		y2 = 0.9 * y1;
		x3 = 0.7 * x1;
		y3 = 0.7 * y1;

		x1 = (x1 + g->x);
		x2 = (x2 + g->x);
		x3 = (x3 + g->x);
		y1 = (y1 + g->y);
		y2 = (y2 + g->y);
		y3 = (y3 + g->y);
		sng_current_draw_line(x1, y1, x2, y2);
		sprintf(buf2, "%1.0lf", v / g->multiplier);
		v += inc;
		sng_center_xy_draw_string(buf2, g->dial_font, x3, y3);
	}
	sng_center_xy_draw_string(g->title, g->label_font,
			g->x, (g->y + (g->r * 0.5)));
	value = g->sample();
	sprintf(buffer, "%4.2lf", value);
	sng_center_xy_draw_string(buffer, g->label_font,
			g->x, (g->y + (g->r * 0.5)) + 15);

	a = ((value - g->r1) / (g->r2 - g->r1))	* g->angular_range + g->start_angle;
	sng_set_foreground(g->needle_color);
	draw_gauge_needle(g->x, g->y, g->r, a);

	if (g->sample2) {
		a = ((g->sample2() - g->r1) / (g->r2 - g->r1)) * g->angular_range + g->start_angle;
		sng_set_foreground(g->needle_color2);
		draw_gauge_needle(g->x, g->y, g->r * 0.8, a);
	}
}

void gauge_free(struct gauge *g)
{
	memset(g, 0, sizeof(*g));
	free(g);
}

void gauge_get_location(struct gauge *g, float *x, float *y, float *r)
{
	*x = g->x;
	*y = g->y;
	*r = g->r;
}

void gauge_set_fonts(struct gauge *g, int dial_font, int label_font)
{
	g->dial_font = dial_font;
	g->label_font = label_font;
}

void gauge_set_multiplier(struct gauge *g, float multiplier)
{
	g->multiplier = multiplier;
}

/* Returns true if (physical_x, physical_y) is inside the gauge.  Used for tooltips. */
int gauge_inside(struct gauge *g, int physical_x, int physical_y)
{
	int x, y, dx, dy;
	x = sng_pixelx_to_screenx(physical_x);
	y = sng_pixely_to_screeny(physical_y);
	dx = x - g->x;
	dy = y - g->y;
	return dx * dx + dy * dy < g->r * g->r;
}

