#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "snis_font.h"
#include "snis_typeface.h"
#include "snis_graph.h"
#define DEFINE_BUTTON_GLOBALS
#include "snis_button.h"
#undef DEFINE_BUTTON_GLOBALS
#include "wwviaudio.h"
#include "string-utils.h"

static int default_button_sound = -1;

struct button {
	int x, y, width, height;
	char label[80];
	int enabled;
	int color, hover_color;
	int disabled_color;
	int font;
	int long_press_timer;
	int button_sound;
	int (*checkbox_function)(void *);
	void *checkbox_cookie;
	void *long_press_cookie;
	button_function button_release; /* Called when the user presses then releases the button. */
	button_function long_press_button_release; /* Called when the user presses holds then release button */
	void *cookie;
	unsigned char button_press_feedback_counter;
	int visible_border;
	int resize_when_label_changes;
	int hover_state;
};

char *snis_button_get_label(struct button *b)
{
	return b->label;
}

static void snis_button_compute_dimensions(struct button *b)
{
	float x1, y1, x2, y2, emwidth, emheight;

	sng_string_bounding_box("M", b->font, &x1, &y1, &x2, &y2);
	emwidth = fabs(x2 - x1);
	emheight = fabs(y2 - y1);
	sng_string_bounding_box(b->label, b->font, &x1, &y1, &x2, &y2);
	if (b->height < 0)
		b->height = emheight * 1.8;
	if (b->width < 0 || b->resize_when_label_changes)
		b->width = 1.0 * fabs(x2 - x1) + emwidth * 1.8;
}

void snis_button_set_label(struct button *b, char *label)
{
	if (strcmp(label, b->label) != 0) {
		strlcpy(b->label, label, sizeof(b->label));
		if (b->resize_when_label_changes)
			snis_button_compute_dimensions(b);
	}
}

struct button *snis_button_init(int x, int y, int width, int height,
			char *label, int color, int font, button_function button_release,
			void *cookie)
{
	struct button *b;

	b = malloc(sizeof(*b));
	b->x = x;
	b->y = y;
	b->width = width;
	b->height = height;
	b->font = font; /* this must happen before setting the label */
	b->resize_when_label_changes = 0; /* must happen before setting label */
	snis_button_set_label(b, label);
	b->label[sizeof(b->label) - 1] = '\0';
	b->color = color;
	b->disabled_color = color;
	b->hover_color = color;
	b->button_release = button_release;
	b->cookie = cookie;
	b->checkbox_function = NULL;
	b->checkbox_cookie = NULL;
	b->long_press_button_release = NULL;
	b->button_press_feedback_counter = 0;
	b->button_sound = default_button_sound;
	b->enabled = 1;
	b->long_press_timer = 0;
	b->visible_border = 1;
	if (b->width < 0 || b->height < 0)
		snis_button_compute_dimensions(b);
	b->hover_state = 0;
	return b;
}

void snis_button_resize_when_label_changes(struct button *b, int resize)
{
	b->resize_when_label_changes = resize;
}

static void snis_button_draw_outline(float x1, float y1, float width, float height)
{
	float dx = width * 0.03;
	float dy = height * 0.15;
	float d;
	if (dx < dy)
		d = dx;
	else
		d = dy;

	sng_current_draw_line(x1 + d, y1, x1 + width - d, y1);
	sng_current_draw_line(x1 + d, y1 + height, x1 + width - d, y1 + height);
	sng_current_draw_line(x1, y1 + d, x1, y1 + height - d);
	sng_current_draw_line(x1 + width, y1 + d, x1 + width, y1 + height - d);

	sng_current_draw_line(x1 + d, y1, x1, y1 + d);
	sng_current_draw_line(x1 + width - d, y1, x1 + width, y1 + d);
	sng_current_draw_line(x1, y1 + height - d, x1 + d, y1 + height);
	sng_current_draw_line(x1 + width, y1 + height - d, x1 + width - d, y1 + height);
}

int snis_button_inside(struct button *b, int x, int y)
{
	x = sng_pixelx_to_screenx(x);
	y = sng_pixely_to_screeny(y);
	return x >= b->x && x <= b->x + b->width &&
		y >= b->y && y <= b->y + b->height;
}

void snis_button_draw(struct button *b)
{
	int offset;

	if (b->height < 0 || b->width < 0)
		snis_button_compute_dimensions(b);

	if (b->enabled && b->hover_state)
		sng_set_foreground(b->hover_color);
	else if (b->enabled)
		sng_set_foreground(b->color);
	else
		sng_set_foreground(b->disabled_color);

	if (b->button_press_feedback_counter)
		offset = 1;
	else
		offset = 0;
	if (b->visible_border) {
		snis_button_draw_outline(b->x + offset, b->y + offset,
						b->width + offset, b->height + offset);
		if (b->button_press_feedback_counter)
			snis_button_draw_outline(b->x + 1 + offset, b->y + 1 + offset,
						b->width - 2 + offset, b->height - 2 + offset);
	}
	if (!b->checkbox_function) {
		sng_abs_xy_draw_string(b->label, b->font, b->x + 10, b->y + b->height / 1.7);
		if (b->button_press_feedback_counter)
			sng_abs_xy_draw_string(b->label, b->font, b->x + 11, b->y + b->height / 1.7 + 1);
	} else {
		float x1, y1, x2, y2;

		x1 = b->x + 5;
		x2 = b->x + 5 + 16;
		y1 = b->y + b->height / 2 - 8;
		y2 = b->y + b->height / 2 + 8;

		sng_current_draw_rectangle(0, x1, y1, 16, 16);
		if (b->checkbox_function(b->checkbox_cookie)) {
			sng_current_draw_line(x1, y1, x2, y2);
			sng_current_draw_line(x1, y2, x2, y1);
		}
		sng_abs_xy_draw_string(b->label, b->font, b->x + 30, b->y + b->height / 1.7);
		if (b->button_press_feedback_counter)
			sng_abs_xy_draw_string(b->label, b->font,
					b->x + 31, b->y + 1 + b->height / 1.7); 
	}
	if (b->button_press_feedback_counter)
		b->button_press_feedback_counter--;
	if (b->long_press_timer > 0)
		b->long_press_timer--;
}

int snis_button_trigger_button(struct button *b)
{
	if (b->button_sound != -1)
		wwviaudio_add_sound(b->button_sound);
	if (b->button_release)
		b->button_release(b->cookie);
	b->long_press_timer = 0;
	b->button_press_feedback_counter = 5;
	return 1;
}

int snis_button_trigger_long_press(struct button *b)
{
	if (b->button_sound != -1)
		wwviaudio_add_sound(b->button_sound);
	if (b->long_press_button_release)
		b->long_press_button_release(b->long_press_cookie);
	b->long_press_timer = 0;
	b->button_press_feedback_counter = 5;
	return 1;
}

int snis_button_button_release(struct button *b, int x, int y)
{
	if (!snis_button_inside(b, x, y))
		return 0;
	if (!b->enabled)
		return 0;
	if (b->long_press_timer == 0 && b->long_press_button_release)
		return snis_button_trigger_long_press(b);
	else
		return snis_button_trigger_button(b);
}

int snis_button_button_press(struct button *b, int x, int y)
{
	if (!snis_button_inside(b, x, y))
		return 0;
	if (!b->enabled)
		return 0;
	b->long_press_timer = 60; /* assuming button is drawn at 30Hz, 2 seconds */
	return 1;
}

void snis_button_set_color(struct button *b, int color)
{
	b->color = color;
}

void snis_button_set_hover_color(struct button *b, int color)
{
	b->hover_color = color;
}

void snis_button_set_disabled_color(struct button *b, int color)
{
	b->disabled_color = color;
}

int snis_button_get_color(struct button *b)
{
	return b->color;
}

int snis_button_get_disabled_color(struct button *b)
{
	return b->disabled_color;
}

void snis_button_set_position(struct button *b, int x, int y)
{
	b->x = x;
	b->y = y;
}

int snis_button_get_x(struct button *b) { return b->x; }
int snis_button_get_y(struct button *b) { return b->y; }
int snis_button_get_width(struct button *b) { return b->width; }
int snis_button_get_height(struct button *b) { return b->height; }

void snis_button_set_sound(struct button *b, int sound)
{
	b->button_sound = sound;
}

void snis_button_set_default_sound(int sound)
{
	default_button_sound = sound;
}

void snis_button_disable(struct button *b)
{
	b->enabled = 0;
}

void snis_button_enable(struct button *b)
{
	b->enabled = 1;
}

void snis_button_set_cookie(struct button *b, void *cookie)
{
	b->cookie = cookie;
}

void snis_button_set_checkbox_function(struct button *b, int (*checkbox_function)(void *), void *cookie)
{
	b->checkbox_function = checkbox_function;
	b->checkbox_cookie = cookie;
}

void snis_button_set_long_press_function(struct button *b, button_function long_press_function, void *cookie)
{
	b->long_press_button_release = long_press_function;
	b->long_press_cookie = cookie;
}

void snis_button_set_visible_border(struct button *b, int visible_border)
{
	b->visible_border = !!visible_border;
}

/* This is meant for use as a checkbox function for checkboxes that modify/reflect a simple int */
int snis_button_generic_checkbox_function(void *x)
{
	return *((int *) x);
}

void snis_button_generic_checkbox_toggler(void *x)
{
	int *y = x;
	*y = !*y;
}

void snis_button_set_mouse_hover(struct button *b, int state)
{
	b->hover_state = state;
}

