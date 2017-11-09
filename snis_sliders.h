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


GLOBAL struct slider *snis_slider_init(float x, float y, float length, float height, int color,
		char *label, char *l1, char *l2, double r1, double r2,
		slider_monitor_function gmf, slider_clicked_function clicked);

GLOBAL void snis_slider_set_vertical(struct slider *s, int v);
GLOBAL void snis_slider_draw(struct slider *s);
GLOBAL double snis_slider_get_value(struct slider *s);
GLOBAL double snis_slider_get_input(struct slider *s);
GLOBAL void snis_draw_sliders();
GLOBAL int snis_slider_button_press(struct slider *s, int x, int y);
GLOBAL void snis_slider_set_sound(int sound);
GLOBAL void snis_slider_set_input(struct slider *s, double input);
GLOBAL void snis_slider_poke_input(struct slider *s, double input, int with_sound);
GLOBAL void snis_slider_nudge(struct slider *s, double fraction, int with_sound);
GLOBAL void snis_slider_set_color_scheme(struct slider *s, int reversed);
GLOBAL void snis_slider_set_label_font(struct slider *s, int font);
GLOBAL void snis_slider_set_fuzz(struct slider *s, int fuzz);
GLOBAL int snis_slider_mouse_inside(struct slider *s, int x, int y);
GLOBAL void snis_slider_get_location(struct slider *s, float *x, float *y, float *length, float *height);

#undef GLOBAL
#endif
