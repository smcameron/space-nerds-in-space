#ifndef SNIS_SLIDERS_H__
#define SNIS_SLIDERS_H__

#ifdef DEFINE_SLIDERS_GLOBALS
#define GLOBAL
#else
#define GLOBAL extern
#endif

struct slider;

typedef void (*slider_clicked_function)(struct slider *s);
typedef double (*slider_monitor_function)(void);


GLOBAL struct slider *snis_slider_init(int x, int y, int length, int height, int color,
		char *label, char *l1, char *l2, double r1, double r2,
		slider_monitor_function gmf, slider_clicked_function clicked);

GLOBAL void snis_slider_set_vertical(struct slider *s, int v);
GLOBAL void snis_slider_draw(GtkWidget *w, GdkGC *gc, struct slider *s);
GLOBAL double snis_slider_get_value(struct slider *s);
GLOBAL double snis_slider_get_input(struct slider *s);
GLOBAL void snis_draw_sliders(GtkWidget *w, GdkGC *gc);
GLOBAL int snis_slider_button_press(struct slider *s, int x, int y);
GLOBAL void snis_slider_set_sound(int sound);
GLOBAL void snis_slider_set_input(struct slider *s, double input);

#undef GLOBAL
#endif
