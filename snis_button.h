#ifndef __SNIS_BUTTON_H__
#define __SNIS_BUTTON_H__

#ifdef DEFINE_BUTTON_GLOBALS
#define GLOBAL
#else
#define GLOBAL extern
#endif

typedef void (*button_function)(void *cookie);

struct button;

GLOBAL struct button *button_init(int x, int y, int width, int height, char *label,
			int color, int font, button_function bf, void *cookie,
			int active_displaymode, volatile int *displaymode);

GLOBAL void button_draw(GtkWidget *w, GdkGC *gc, struct button *b);

GLOBAL void add_button(struct button *b);
GLOBAL void draw_buttons(GtkWidget *w, GdkGC *gc);
GLOBAL void buttons_button_press(int x, int y);

#undef GLOBAL
#endif
