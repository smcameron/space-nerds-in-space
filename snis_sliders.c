#include <stdlib.h>
#include <string.h>
#include <gtk/gtk.h>

#include "snis_font.h"
#include "snis_typeface.h"
#include "snis_graph.h"
#include "wwviaudio.h"

#define SLIDERS_DEFINE_GLOBALS
#include "snis_sliders.h"
#undef SLIDERS_DEFINE_GLOBALS

struct slider {
	int x, y, length, height;
	int color;
	double value, input;
	char label[20], label1[5], label2[5];
	double r1, r2;
	slider_monitor_function sample;
	slider_clicked_function clicked;
	int vertical;
	int colors_reversed;
};

static int slider_sound = -1;

struct slider *snis_slider_init(int x, int y, int length, int height, int color,
		char *label, char *l1, char *l2, double r1, double r2,
		slider_monitor_function gmf, slider_clicked_function clicked)
{
	struct slider *s;

	s = malloc(sizeof(*s));
	s->x = x;
	s->y = y;
	s->length = length;
	s->height = height;
	s->color = color;
	strncpy(s->label, label, sizeof(s->label) - 1);
	s->label[sizeof(s->label) - 1] = '\0';
	strncpy(s->label1, l1, sizeof(s->label1) - 1);
	s->label1[sizeof(s->label1) - 1] = '\0';
	strncpy(s->label2, l2, sizeof(s->label2) - 1);
	s->label2[sizeof(s->label2) - 1] = '\0';
	s->r1 = r1;
	s->r2 = r2;
	s->sample = gmf;
	s->clicked = clicked;
	s->value = (s->sample() - s->r1) / (s->r2 - s->r1);
	s->input = 0.0;
	s->vertical = 0;
	s->colors_reversed = 0;
	return s;
}

void snis_slider_set_vertical(struct slider *s, int v)
{
	s->vertical = v;
}

static int choose_barcolor(struct slider *s, double v)
{
	if (s->clicked)
		return DARKGREEN;
	if (!s->colors_reversed) {
		if (v < 25.0)
			return RED;
		if (v < 50.0)
			return AMBER;
		return DARKGREEN;
	}
	if (v < 75.0)
		return DARKGREEN;
	if (v < 90.0)
		return AMBER;
	return RED;
}

static void snis_slider_draw_vertical(GtkWidget *w, GdkGC *gc, struct slider *s)
{
	double v;
	int height, ty1;
	int bar_color;
	int ptr_height = s->height / 2;
	int ptr_width = s->height / 3;

	v = s->sample();
	s->value = (v - s->r1) / (s->r2 - s->r1);
	v = s->sample();
	ty1 = (int) (s->y + s->length - s->input * s->length);
	bar_color = choose_barcolor(s, v);
	sng_set_foreground(s->color);
	sng_current_draw_rectangle(w->window, gc, 0, s->x, s->y, s->height, s->length);
	height = s->value * s->length - 1;
	if (height < 0)
		height = 0;
	if (!s->clicked)
		sng_set_foreground(bar_color);
	sng_current_draw_rectangle(w->window, gc, 1, s->x + 1, s->y + s->length - height,
					s->height - 2, height);
	if (!s->clicked)
		sng_set_foreground(s->color);

	if (s->clicked) {
		sng_current_draw_line(w->window, gc,
			s->x, ty1,
			s->x - ptr_height, ty1 - ptr_width); 
		sng_current_draw_line(w->window, gc,
			s->x - ptr_height, ty1 - ptr_width,
			s->x - ptr_height, ty1 + ptr_width);
		sng_current_draw_line(w->window, gc,
			s->x - ptr_height, ty1 + ptr_width,
			s->x, ty1);

		sng_current_draw_line(w->window, gc,
			s->x + s->height, ty1,
			s->x + ptr_height + s->height, ty1 - ptr_width); 
		sng_current_draw_line(w->window, gc,
			s->x + ptr_height + s->height, ty1 - ptr_width,
			s->x + ptr_height + s->height, ty1 + ptr_width);
		sng_current_draw_line(w->window, gc,
			s->x + ptr_height + s->height, ty1 + ptr_width,
			s->x + s->height, ty1);
	}
	/* sng_abs_xy_draw_string(w, gc, s->label, TINY_FONT, s->x + s->length + 5, s->y + 2 * s->height / 3);  */
} 

void snis_slider_draw(GtkWidget *w, GdkGC *gc, struct slider *s)
{
	double v;
	int width, tx1;
	int bar_color = DARKGREEN;
	int ptr_height = s->height / 2;
	int ptr_width = s->height / 3;

	if (s->vertical) {
		snis_slider_draw_vertical(w, gc, s);
		return;
	}

	v = s->sample();
	s->value = (v - s->r1) / (s->r2 - s->r1);
	v = s->sample();
	bar_color = choose_barcolor(s, v);
	sng_set_foreground(s->color);
	sng_current_draw_rectangle(w->window, gc, 0, s->x, s->y, s->length, s->height);
	width = s->value * s->length - 1;
	if (width < 0)
		width = 0;
	if (!s->clicked)
		sng_set_foreground(bar_color);
	sng_current_draw_rectangle(w->window, gc, 1, s->x + 1, s->y + 1, width, s->height - 2);
	if (!s->clicked)
		sng_set_foreground(s->color);

	tx1 = (int) (s->input * s->length) + s->x;

	if (s->clicked) {
		sng_current_draw_line(w->window, gc, tx1, s->y, tx1 - ptr_width, s->y - ptr_height); 
		sng_current_draw_line(w->window, gc, tx1, s->y, tx1 + ptr_width, s->y - ptr_height); 
		sng_current_draw_line(w->window, gc, tx1 - ptr_width, s->y - ptr_height,
						tx1 + ptr_width, s->y - ptr_height); 
		sng_current_draw_line(w->window, gc, tx1, s->y + s->height,
				tx1 - ptr_width, s->y + s->height + ptr_height); 
		sng_current_draw_line(w->window, gc, tx1, s->y + s->height,
				tx1 + ptr_width, s->y + s->height + ptr_height); 
		sng_current_draw_line(w->window, gc, tx1 - ptr_width, s->y + s->height + ptr_height,
				tx1 + ptr_width, s->y + s->height + ptr_height); 
	}
	sng_abs_xy_draw_string(w, gc, s->label, TINY_FONT, s->x + s->length + 5, s->y + 2 * s->height / 3); 
}

double snis_slider_get_value(struct slider *s)
{	
	return s->value;
}

double snis_slider_get_input(struct slider *s)
{	
	return s->input;
}

static int snis_slider_button_press_vertical(struct slider *s, int x, int y)
{
	if (x < s->x || x > s->x + s->height || 
		y < s->y - 5 || y > s->y + s->length + 5)
			return 0;
	if (y >= s->y + s->length)
		s->input = 0.0;
	else if (y <= s->y)
		s->input = 1.0;
	else
		s->input = ((double) (s->y + s->length) - (double) y) / (double) s->length;
	if (s->clicked) {
		s->clicked(s);
		if (slider_sound != -1) 
				wwviaudio_add_sound(slider_sound);
	}
	return 1;
}

int snis_slider_button_press(struct slider *s, int x, int y)
{
	if (s->vertical)
		return snis_slider_button_press_vertical(s, x, y);
	if (x < s->x - 5 || x > s->x + s->length + 5 || 
		y < s->y || y > s->y + s->height)
			return 0;
	if (x >= s->x + s->length)
		s->input = 1.0;
	else if (x <= s->x)
		s->input = 0.0;
	else
		s->input = ((double) x - (double) s->x) / (double) s->length;
	if (s->clicked) {
		s->clicked(s);
		if (slider_sound != -1) 
				wwviaudio_add_sound(slider_sound);
	}
	return 1;
}

void snis_slider_set_sound(int sound)
{
	slider_sound = sound;
}

void snis_slider_set_input(struct slider *s, double input)
{
	s->input = input;
}

void snis_slider_set_color_scheme(struct slider *s, int reversed)
{
	s->colors_reversed = reversed;
}


