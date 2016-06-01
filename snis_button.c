#include <stdlib.h>
#include <string.h>
#include <gtk/gtk.h>
#include <math.h>

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

void snis_button_set_label(struct button *b, char *label)
{
	strncpy(b->label, label, sizeof(b->label) - 1);
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
	if (b->width < 0)
		b->width = fabs(x2 - x1) + emwidth * 1.8;
}

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
	snis_button_set_label(b, label);
	b->label[sizeof(b->label) - 1] = '\0';
	b->color = color;
	b->font = font;
	b->bf = bf;
	b->cookie = cookie;
	b->checkbox_value = NULL;
	b->button_press_feedback_counter = 0;
	if (b->width < 0 || b->height < 0)
		snis_button_compute_dimensions(b);
	return b;
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

void snis_button_draw(struct button *b)
{
	int offset;

	if (b->height < 0 || b->width < 0)
		snis_button_compute_dimensions(b);

	sng_set_foreground(b->color);
	if (b->button_press_feedback_counter)
		offset = 1;
	else
		offset = 0;
	snis_button_draw_outline(b->x + offset, b->y + offset,
					b->width + offset, b->height + offset);
	if (b->button_press_feedback_counter)
		snis_button_draw_outline(b->x + 1 + offset, b->y + 1 + offset,
					b->width - 2 + offset, b->height - 2 + offset);
	if (!b->checkbox_value) {
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
		if (*b->checkbox_value) {
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

void snis_button_set_position(struct button *b, int x, int y)
{
	b->x = x;
	b->y = y;
}

int snis_button_get_x(struct button *b) { return b->x; }
int snis_button_get_y(struct button *b) { return b->y; }
int snis_button_get_width(struct button *b) { return b->width; }
int snis_button_get_height(struct button *b) { return b->height; }

