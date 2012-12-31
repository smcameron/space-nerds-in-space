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
	int *checkbox_value;
	button_function bf;
	void *cookie;
	unsigned char button_press_feedback_counter;
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
	b->checkbox_value = NULL;
	b->button_press_feedback_counter = 0;
	return b;
}

void snis_button_draw(GtkWidget *w, GdkGC *gc, struct button *b)
{
	sng_set_foreground(b->color);
	sng_current_draw_rectangle(w->window, gc, 0, b->x, b->y, b->width, b->height);
	if (b->button_press_feedback_counter)
		sng_current_draw_rectangle(w->window, gc, 0, b->x + 1, b->y + 1,
					b->width - 2, b->height - 2);
	if (!b->checkbox_value) {
		sng_abs_xy_draw_string(w, gc, b->label, b->font, b->x + 10, b->y + b->height / 1.7); 
		if (b->button_press_feedback_counter)
			sng_abs_xy_draw_string(w, gc, b->label, b->font, b->x + 11, b->y + b->height / 1.7 + 1); 
	} else {
		sng_current_draw_rectangle(w->window, gc, 0, b->x + 5, b->y + 2, 15, 15);
		if (*b->checkbox_value) {
			sng_current_draw_line(w->window, gc,
				b->x + 5, b->y + 2, b->x + 5 + 15, b->y + 2 + 15);
			sng_current_draw_line(w->window, gc,
				b->x + 5, b->y + 2 + 15, b->x + 5 + 15, b->y + 2);
		}
		sng_abs_xy_draw_string(w, gc, b->label, b->font, b->x + 30, b->y + b->height / 1.7); 
		if (b->button_press_feedback_counter)
			sng_abs_xy_draw_string(w, gc, b->label, b->font,
					b->x + 31, b->y + 1 + b->height / 1.7); 
	}
	if (b->button_press_feedback_counter)
		b->button_press_feedback_counter--;
}

int snis_button_button_press(struct button *b, int x, int y)
{
	if (x < b->x || x > b->x + b->width || 
		y < b->y || y > b->y + b->height)
		return 0;
	if (b->bf)
		b->bf(b->cookie);
	if (b->checkbox_value)
		*b->checkbox_value = !*b->checkbox_value;
	b->button_press_feedback_counter = 5;
	return 1;
}

void snis_button_set_color(struct button *b, int color)
{
	b->color = color;
}

void snis_button_checkbox(struct button *b, int *value)
{
	b->checkbox_value = value;
}
