#ifndef SNIS_GRAPH_H__
#define SNIS_GRAPH_H__

#ifdef SNIS_GRAPH_DECLARE_GLOBALS
#define GLOBAL
#define INIT(y) = y
#else
#define GLOBAL extern
#define INIT(y)
#endif

typedef void line_drawing_function(GdkDrawable *drawable,
	GdkGC *gc, gint x1, gint y1, gint x2, gint y2);

GLOBAL line_drawing_function *sng_current_draw_line INIT(gdk_draw_line);

extern void sng_set_scale(float xscale, float yscale);
extern void sng_scaled_line(GdkDrawable *drawable, GdkGC *gc, gint x1, gint y1, gint x2, gint y2);
extern void sng_scaled_rectangle(GdkDrawable *drawable,
	GdkGC *gc, gboolean filled, gint x, gint y, gint width, gint height);
extern void sng_thick_scaled_line(GdkDrawable *drawable,
	GdkGC *gc, gint x1, gint y1, gint x2, gint y2);
extern void sng_scaled_bright_line(GdkDrawable *drawable,
	GdkGC *gc, gint x1, gint y1, gint x2, gint y2, int color);
extern void sng_unscaled_bright_line(GdkDrawable *drawable,
	GdkGC *gc, gint x1, gint y1, gint x2, gint y2, int color);
extern void sng_scaled_arc(GdkDrawable *drawable, GdkGC *gc,
	gboolean filled, gint x, gint y, gint width, gint height, gint angle1, gint angle2);

extern void sng_scaled_rectangle(GdkDrawable *drawable,
	GdkGC *gc, gboolean filled, gint x, gint y, gint width, gint height);


#endif
