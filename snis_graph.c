#include <stdlib.h>
#include <gtk/gtk.h>

#include "snis_graph.h"

static struct snis_graph_context {
	float xscale, yscale;
} sgc;

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

