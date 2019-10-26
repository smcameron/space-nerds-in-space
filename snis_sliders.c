#include <stdlib.h>
#include <string.h>
#include <gtk/gtk.h>

#include "snis_font.h"
#include "snis_typeface.h"
#include "snis_graph.h"
#include "wwviaudio.h"
#include "mtwist.h"
#include "mathutils.h"
#include "ui_colors.h"

#define SLIDERS_DEFINE_GLOBALS
#include "snis_sliders.h"
#undef SLIDERS_DEFINE_GLOBALS

static int *slider_mouse_x = NULL;
static int *slider_mouse_y = NULL;

struct slider {
	float x, y, length, height;
	int color;
	double value, input;
	char label[20], label1[5], label2[5];
	double r1, r2;
	slider_monitor_function sample;
	slider_clicked_function clicked;
	int vertical;
	int font;
	int colors_reversed;
	unsigned char timer;
	unsigned char fuzz;
};

static int slider_sound = -1;

struct slider *snis_slider_init(float x, float y, float length, float height, int color,
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
	s->timer = 0;
	s->font = TINY_FONT;
	s->fuzz = 0;
	return s;
}

void snis_slider_set_vertical(struct slider *s, int v)
{
	s->vertical = v;
}

static int choose_barcolor(struct slider *s, double v)
{
	if (s->clicked)
		return UI_COLOR(slider_good);
	if (!s->colors_reversed) {
		if (v <= 15.0)
			return ((s->timer & 0x04) == 0) ? UI_COLOR(slider_black) : UI_COLOR(slider_warning);
		if (v <= 40.0)
			return UI_COLOR(slider_warning);
		if (v <= 75.0)
			return UI_COLOR(slider_caution);
		return UI_COLOR(slider_good);
	}
	if (v < 75.0)
		return UI_COLOR(slider_good);
	if (v < 90.0)
		return UI_COLOR(slider_caution);
	return ((s->timer & 0x04) == 0) ? UI_COLOR(slider_black) : UI_COLOR(slider_warning);
}

int snis_slider_alarm_triggered(struct slider *s)
{
	double v = s->sample();
	if (!s->colors_reversed)
		return (v <= 15.0);
	return (v >= 90.0);
}

static void snis_slider_draw_vertical_arrow(struct slider *s, int y)
{
	int ptr_height = s->height / 2;
	int ptr_width = s->height / 3;

	sng_current_draw_line(s->x, y, s->x - ptr_height, y - ptr_width);
	sng_current_draw_line(s->x - ptr_height, y - ptr_width, s->x - ptr_height, y + ptr_width);
	sng_current_draw_line(s->x - ptr_height, y + ptr_width, s->x, y);

	sng_current_draw_line(s->x + s->height, y, s->x + ptr_height + s->height, y - ptr_width);
	sng_current_draw_line(s->x + ptr_height + s->height, y - ptr_width,
		s->x + ptr_height + s->height, y + ptr_width);
	sng_current_draw_line(s->x + ptr_height + s->height, y + ptr_width,
		s->x + s->height, y);
}

static void snis_slider_draw_vertical(struct slider *s)
{
	double v;
	int height, ty1;
	int bar_color;
	float f;

	if (s->fuzz) {
		f = (float) ((snis_randn(1000) - 500.0f) * s->fuzz) / 100000.0f;
		f = f * s->length;
	} else {
		f = 0.0f;
	}

	v = s->sample();
	s->value = (v - s->r1) / (s->r2 - s->r1);
	if (s->value == 0)
		f = 0.0f; /* no fuzz if no power */
	v = s->sample();
	ty1 = (int) (s->y + s->length - s->input * s->length);
	bar_color = choose_barcolor(s, v);
	sng_set_foreground(s->color);
	sng_current_draw_rectangle(0, s->x, s->y, s->height, s->length);
	height = s->value * s->length - 1;
	height += f;
	if (height < 0)
		height = 0;
	if (height > s->length - 1)
		height = s->length - 1;
	if (!s->clicked)
		sng_set_foreground(bar_color);
	sng_current_draw_rectangle(1, s->x + 1, s->y + s->length - height,
					s->height - 2, height);
	if (!s->clicked)
		sng_set_foreground(s->color);

	if (s->clicked) {
		snis_slider_draw_vertical_arrow(s, ty1);
		if (slider_mouse_y && (s->timer & 0x04) == 0 &&
			snis_slider_mouse_inside(s, *slider_mouse_x, *slider_mouse_y)) {
			ty1 = sng_pixely_to_screeny(*slider_mouse_y);
			snis_slider_draw_vertical_arrow(s, ty1);
		}
	}
	/* sng_abs_xy_draw_string(s->label, s->font, s->x + s->length + 5, s->y + 2 * s->height / 3);  */
} 

static void snis_slider_draw_arrow(struct slider *s, int x)
{
	float ptr_height = s->height / 2.0;
	float ptr_width = s->height / 3.0;

	sng_current_draw_line(x, s->y, x - ptr_width, s->y - ptr_height);
	sng_current_draw_line(x, s->y, x + ptr_width, s->y - ptr_height);
	sng_current_draw_line(x - ptr_width, s->y - ptr_height,
					x + ptr_width, s->y - ptr_height);
	sng_current_draw_line(x, s->y + s->height,
			x - ptr_width, s->y + s->height + ptr_height);
	sng_current_draw_line(x, s->y + s->height,
			x + ptr_width, s->y + s->height + ptr_height);
	sng_current_draw_line(x - ptr_width, s->y + s->height + ptr_height,
			x + ptr_width, s->y + s->height + ptr_height);
}

void snis_slider_draw(struct slider *s)
{
	double v;
	float width, tx1;
	int bar_color;

	s->timer++;
	if (s->vertical) {
		snis_slider_draw_vertical(s);
		return;
	}
	float f;

	if (s->fuzz) {
		f = (float) ((snis_randn(1000) - 500.0f) * s->fuzz) / 100000.0f;
		f = f * s->length;
	} else {
		f = 0.0f;
	}

	v = s->sample();
	s->value = (v - s->r1) / (s->r2 - s->r1);
	if (s->value == 0)
		f = 0.0f; /* no fuzz if no power */
	v = s->sample();
	bar_color = choose_barcolor(s, v);
	sng_set_foreground(s->color);
	sng_current_draw_rectangle(0, s->x, s->y, s->length, s->height);
	width = s->value * (s->length - 2.0);
	width = width + f;
	if (width < 0.0)
		width = 0;
	if (width > s->length - 2.0)
		width = s->length - 2.0;
	if (!s->clicked)
		sng_set_foreground(bar_color);
	sng_current_draw_rectangle(1, s->x + 1.0, s->y + 1.0, width, s->height - 2.0);
	if (!s->clicked)
		sng_set_foreground(s->color);

	tx1 = (s->input * s->length) + s->x;

	if (s->clicked) {
		snis_slider_draw_arrow(s, tx1);
		if (slider_mouse_x && (s->timer & 0x04) == 0 &&
			snis_slider_mouse_inside(s, *slider_mouse_x, *slider_mouse_y)) {
			tx1 = sng_pixelx_to_screenx(*slider_mouse_x);
			snis_slider_draw_arrow(s, tx1);
		}
	}
	sng_abs_xy_draw_string(s->label, s->font,
				s->x + s->length + 5.0, s->y + 2.0 * s->height / 3.0);
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
	x = sng_pixelx_to_screenx(x);
	y = sng_pixely_to_screeny(y);
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
	x = sng_pixelx_to_screenx(x);
	y = sng_pixely_to_screeny(y);
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

void snis_slider_poke_input(struct slider *s, double input, int with_sound)
{
	s->input = input;
	if (s->clicked) {
		s->clicked(s);
		if (slider_sound != -1 && with_sound)
				wwviaudio_add_sound(slider_sound);
	}
}

void snis_slider_nudge(struct slider *s, double fraction, int with_sound)
{
	s->input = s->input + fraction;
	if (s->input < 0.0)
		s->input = 0.0;
	if (s->input > 1.0)
		s->input = 1.0;
	if (s->clicked) {
		s->clicked(s);
		if (slider_sound != -1 && with_sound)
				wwviaudio_add_sound(slider_sound);
	}
}

void snis_slider_set_color_scheme(struct slider *s, int reversed)
{
	s->colors_reversed = reversed;
}

void snis_slider_set_label_font(struct slider *s, int font)
{
	s->font = font;
}

void snis_slider_set_fuzz(struct slider *s, int fuzz)
{
	s->fuzz = fuzz & 0x0ff;
}

static int snis_slider_mouse_inside_vertical(struct slider *s, int x, int y)
{
	x = sng_pixelx_to_screenx(x);
	y = sng_pixely_to_screeny(y);
	return !(x < s->x || x > s->x + s->height ||
		y < s->y - 5 || y > s->y + s->length + 5);
}

int snis_slider_mouse_inside(struct slider *s, int x, int y)
{
	if (s->vertical)
		return snis_slider_mouse_inside_vertical(s, x, y);
	x = sng_pixelx_to_screenx(x);
	y = sng_pixely_to_screeny(y);
	return !(x < s->x - 5 || x > s->x + s->length + 5 ||
		y < s->y || y > s->y + s->height);
}

void snis_slider_get_location(struct slider *s, float *x, float *y, float *length, float *height)
{
	*x = s->x;
	*y = s->y;
	*length = s->length;
	*height = s->height;
}

void snis_slider_mouse_position_query(int *x, int *y) /* Allows sliders to query mouse position */
{
	slider_mouse_x = x;
	slider_mouse_y = y;
}

