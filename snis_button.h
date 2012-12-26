#ifndef __SNIS_BUTTON_H__
#define __SNIS_BUTTON_H__

#ifdef DEFINE_BUTTON_GLOBALS
#define GLOBAL
#else
#define GLOBAL extern
#endif

typedef void (*button_function)(void *cookie);

struct button;

GLOBAL struct button *snis_button_init(int x, int y, int width, int height, char *label,
			int color, int font, button_function bf, void *cookie,
			int active_displaymode, volatile int *displaymode);

GLOBAL void snis_button_draw(GtkWidget *w, GdkGC *gc, struct button *b);

GLOBAL void snis_add_button(struct button *b);
GLOBAL void snis_draw_buttons(GtkWidget *w, GdkGC *gc);
GLOBAL void snis_buttons_button_press(int x, int y);
GLOBAL void snis_button_set_color(struct button *b, int color);

#undef GLOBAL
#endif
