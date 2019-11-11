#ifndef SNIS_GAUGE_H__
#define SNIS_GAUGE_H__

#ifdef DEFINE_SNIS_GAUGE_GLOBALS
#define GLOBAL
#else
#define GLOBAL extern
#endif

typedef double (*gauge_monitor_function)(void);

struct gauge;

/* gauge_init() returns a new gauge.
 * x, y: position of gauge
 * r: radius of gauge
 * r1, r2: range of gauge (low and high)
 * start_angle: Angle in radians at which to start the gauge.
 *		0 is straight up. -90.0 * M_PI / 180.0 is to the left.
 *		90 * M_PI / 180 is to the right.
 * angular_range: Angle in radians that the needle sweeps
 * needle_color: the color of the needle
 * ndivs: number of divisions (tick marks) to draw.
 * title: the name to display on the gauge (e.g. "RPM")
 * gauge_monitor_function: function returning a value between r1 and r2 that
 * the gauge should display.
 */
GLOBAL struct gauge *gauge_init(int x, int y, int r, double r1, double r2,
			double start_angle, double angular_range,
			int needle_color, int dial_color, int ndivs, char *title,
			gauge_monitor_function gmf);

/* Adds a 2nd needle to a gauge with a 2nd monitoring function and a 2nd color */
GLOBAL void gauge_add_needle(struct gauge *g, gauge_monitor_function sample, int color);

/* Sets the fonts used for the gauge label and numeric dial indicator values */
GLOBAL void gauge_set_fonts(struct gauge *g, int dial_font, int label_font);

/* Fills the gauge background with the color bg, with opaqueness alpha */
GLOBAL void gauge_fill_background(struct gauge *g, int bg, float alpha);

GLOBAL void gauge_draw(struct gauge *g); /* draws the gauge */
GLOBAL void gauge_free(struct gauge *g); /* Frees gauge resources */

/* Get location and radius of a gauge */
GLOBAL void gauge_get_location(struct gauge *g, float *x, float *y, float *r);

/* Set multiplier for gauge marks.  Example: a tachometer would have a multipler of
 * 1000 so that 1 means 1000, 2 means 2000, etc.
 */
GLOBAL void gauge_set_multiplier(struct gauge *g, float multiplier);

#endif
