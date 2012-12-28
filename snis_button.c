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
	int x, y, width, height;
	char label[20];
	int color;
	int font;
	button_function bf;
	void *cookie;
};

struct button *snis_button_init(int x, int y, int width, int height,
			char *label, int color, int font, button_function bf,
			void *cookie)
{
	struct button *b;

	b = malloc(sizeof(*b));
	b->x = x;
	b->y = y;
	b->width = width;
	b->height = height;
	strncpy(b->label, label, sizeof(b->label) - 1);
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

void snis_button_button_press(struct button *b, int x, int y)
{
	if (x < b->x || x > b->x + b->width || 
		y < b->y || y > b->y + b->height)
		return;
	b->bf(b->cookie);
}

void snis_button_set_color(struct button *b, int color)
{
	b->color = color;
}
