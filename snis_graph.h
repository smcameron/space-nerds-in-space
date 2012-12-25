#ifndef SNIS_GRAPH_H__
#define SNIS_GRAPH_H__

typedef void line_drawing_function(GdkDrawable *drawable,
	GdkGC *gc, gint x1, gint y1, gint x2, gint y2);

extern void sng_set_scale(float xscale, float yscale);
extern void sng_scaled_line(GdkDrawable *drawable, GdkGC *gc, gint x1, gint y1, gint x2, gint y2);
extern void sng_scaled_rectangle(GdkDrawable *drawable,
	GdkGC *gc, gboolean filled, gint x, gint y, gint width, gint height);

#endif
