#include <stdlib.h>
#include <string.h>
#include <gtk/gtk.h>

#include "snis_font.h"
#include "snis_typeface.h"
#include "snis_graph.h"
#define DEFINE_BUTTON_GLOBALS
#include "snis_button.h"
#undef DEFINE_BUTTON_GLOBALS

struct button {
	int x, y, width, height, active_displaymode;
	volatile int *displaymode;
	char label[20];
	int color;
	int font;
	button_function bf;
	void *cookie;
};

struct button *snis_button_init(int x, int y, int width, int height,
			char *label, int color, int font, button_function bf,
			void *cookie, int active_displaymode, volatile int *displaymode)
{
	struct button *b;

	b = malloc(sizeof(*b));
	b->x = x;
	b->y = y;
	b->width = width;
	b->height = height;
	strncpy(b->label, label, sizeof(b->label) - 1);
	b->active_displaymode = active_displaymode;
	b->color = color;
	b->font = font;
	b->bf = bf;
	b->cookie = cookie;
	return b;
}

void snis_button_draw(GtkWidget *w, GdkGC *gc, struct button *b)
{
	sng_set_foreground(b->color);
	sng_current_draw_rectangle(w->window, gc, 0, b->x, b->y, b->width, b->height);
	sng_abs_xy_draw_string(w, gc, b->label, b->font, b->x + 10, b->y + b->height / 1.7); 
}

#define MAXBUTTONS 50
static struct button *buttonlist[MAXBUTTONS];
static int nbuttons = 0;

void snis_add_button(struct button *b)
{
	if (nbuttons >= MAXBUTTONS)
		return;
	buttonlist[nbuttons] = b;
	nbuttons++;
}

void snis_draw_buttons(GtkWidget *w, GdkGC *gc)
{
	int i;

	for (i = 0; i < nbuttons; i++) {
		struct button *b = buttonlist[i];

		if (b->active_displaymode == *b->displaymode)
			button_draw(w, gc, buttonlist[i]);
	}
}

void snis_buttons_button_press(int x, int y)
{
	int i;
	struct button *b;

	for (i = 0; i < nbuttons; i++) {
		b = buttonlist[i];
		if (b->active_displaymode == *b->displaymode) {
			if (x < b->x || x > b->x + b->width || 
				y < b->y || y > b->y + b->height)
				continue;
			b->bf(b->cookie);
		}
	}
}

