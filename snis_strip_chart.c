#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "snis_font.h"
#include "snis_typeface.h"
#include "snis_graph.h"
#include "snis_strip_chart.h"
#include "string-utils.h"

struct strip_chart {
	int x, y, width, height, color, warn_color, font, history_size, needle;
	char label[100];
	char warning_msg[100];
	uint8_t *history;
	uint8_t warning_on, warning_level;
};

struct scaling_strip_chart {
	int x, y, width, height, color, warn_color, font, history_size, needle;
	char label[100];
	char warning_msg[100];
	float warning_on;
	float *history;
	float warning_level;
	float bottom, top;
};

struct strip_chart *snis_strip_chart_init(int x, int y, int width, int height, char *label, char *warning_msg,
			int color, int warn_color, uint8_t warning_level, int font, int history_size)
{
	struct strip_chart *sc = NULL;

	if (history_size > 1000)
		return NULL;

	sc = calloc(sizeof(*sc), 1);
	sc->history = calloc(sizeof(*sc->history), history_size);
	sc->x = x;
	sc->y = y;
	sc->width = width;
	sc->height = height;
	sc->color = color;
	sc->warn_color = warn_color;
	sc->font = font;
	sc->history_size = history_size;
	strlcpy(sc->label, label, sizeof(sc->label));
	strlcpy(sc->warning_msg, warning_msg, sizeof(sc->warning_msg));
	sc->needle = 0;
	sc->warning_on = 0;
	sc->warning_level = warning_level;
	return sc;
}

void snis_strip_chart_draw(struct strip_chart *sc)
{
	int i, index;
	float x1, y1, x2, y2, w, h, ox, oy;

	sng_set_foreground(sc->color);
	w = sc->width;
	h = sc->height;
	ox = sc->x;
	oy = sc->y;
	sng_current_draw_rectangle(0, ox, oy, w, h);

	x2 = ox;
	y2 = oy;

	for (i = 0; i < sc->history_size; i++) {
		index = (sc->needle + i) % sc->history_size;
		x1 = x2;
		y1 = y2;
		x2 = (w * i) / sc->history_size + ox;
		y2 = oy + h * (255.0 - sc->history[index]) / 255.0;
		sng_current_draw_line(x1, y1, x2, y2);
	}
	sng_abs_xy_draw_string(sc->label, sc->font, ox, oy + h + snis_font_lineheight(sc->font));
	if (sc->warning_on) {
		sng_set_foreground(sc->warn_color);
		sng_abs_xy_draw_string(sc->warning_msg, sc->font, ox + 10, oy + 0.5 * h);
	}
}

void snis_strip_chart_update(struct strip_chart *sc, uint8_t value)
{
	sc->history[sc->needle] = value;
	if (value > sc->warning_level)
		sc->warning_on = 1;
	else
		sc->warning_on = 0;
	sc->needle = (sc->needle + 1) % sc->history_size;
	sc->history[sc->needle] = 0;
}

int snis_strip_chart_inside(struct strip_chart *sc, int x, int y)
{
	x = sng_pixelx_to_screenx(x);
	y = sng_pixelx_to_screenx(y);

	if (x < sc->x)
		return 0;
	if (x > sc->x + sc->width)
		return 0;
	if (y < sc->y)
		return 0;
	if (y > sc->y + sc->height)
		return 0;
	return 1;

}

void snis_strip_chart_update_position(struct strip_chart *sc, int x, int y)
{
	sc->x = x;
	sc->y = y;
}

int snis_strip_chart_get_x(struct strip_chart *sc)
{
	return sc->x;
}

int snis_strip_chart_get_y(struct strip_chart *sc)
{
	return sc->y;
}

struct scaling_strip_chart *snis_scaling_strip_chart_init(int x, int y,
			int width, int height, char *label, char *warning_msg,
			int color, int warn_color, float warning_level, int font, int history_size)
{
	struct scaling_strip_chart *sc = NULL;

	if (history_size > 1000)
		return NULL;

	sc = calloc(sizeof(*sc), 1);
	sc->history = calloc(sizeof(*sc->history), history_size);
	sc->bottom = 0.0;
	sc->top = 1.0;
	sc->x = x;
	sc->y = y;
	sc->width = width;
	sc->height = height;
	sc->color = color;
	sc->warn_color = warn_color;
	sc->font = font;
	sc->history_size = history_size;
	strlcpy(sc->label, label, sizeof(sc->label));
	strlcpy(sc->warning_msg, warning_msg, sizeof(sc->warning_msg));
	sc->needle = 0;
	sc->warning_on = 0;
	sc->warning_level = warning_level;
	return sc;
}

static const char *format_string(float value)
{
	if (fabsf(value) == 0.0)
		return "%.0f";
	if (fabsf(value) < 0.1)
		return "%f";
	if (fabsf(value) < 100.0)
		return "%1.1f";
	return "%.0f";
}

void snis_scaling_strip_chart_draw(struct scaling_strip_chart *sc)
{
	int i, index;
	float x1, y1, x2, y2, w, h, ox, oy;
	char toplabel[20], bottomlabel[20];

	sng_set_foreground(sc->color);
	w = sc->width;
	h = sc->height;
	ox = sc->x;
	oy = sc->y;
	sng_current_draw_rectangle(0, ox, oy, w, h);
	char valuelabel[100];

	x2 = ox;
	y2 = oy;

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
	sprintf(toplabel, format_string(sc->top), sc->top);
	sprintf(bottomlabel, format_string(sc->bottom), sc->bottom);
#pragma GCC diagnostic pop

	for (i = 0; i < sc->history_size; i++) {
		index = (sc->needle + i) % sc->history_size;
		if (sc->history[index] > sc->top)
			sc->top = sc->history[index];
		if (sc->history[index] < sc->bottom)
			sc->bottom = sc->history[index];
		x1 = x2;
		y1 = y2;
		x2 = (w * i) / sc->history_size + ox;
		y2 = oy + h * (sc->top - sc->history[index] + sc->bottom) / (sc->top - sc->bottom);
		sng_current_draw_line(x1, y1, x2, y2);
		if (i == sc->history_size - 1) {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
			sprintf(valuelabel, format_string(sc->history[index]), sc->history[index]);
#pragma GCC diagnostic pop
			sng_set_foreground(sc->warn_color);
			sng_abs_xy_draw_string(valuelabel, sc->font, ox + w + 5,
						y2 + 0.5 * snis_font_lineheight(sc->font));
		}
	}
	sng_set_foreground(sc->color);
	sng_abs_xy_draw_string(sc->label, sc->font, ox, oy + h + snis_font_lineheight(sc->font));
	sng_abs_xy_draw_string(toplabel, sc->font, ox + w + 5, oy);
	sng_abs_xy_draw_string(bottomlabel, sc->font, ox + w + 5, oy + h);
	if (sc->warning_on) {
		sng_set_foreground(sc->warn_color);
		sng_abs_xy_draw_string(sc->warning_msg, sc->font, ox + 10, oy + 0.5 * h);
	}
}

void snis_scaling_strip_chart_update(struct scaling_strip_chart *sc, float value)
{
	sc->history[sc->needle] = value;
	if (value > sc->warning_level)
		sc->warning_on = 1;
	else
		sc->warning_on = 0;
	sc->needle = (sc->needle + 1) % sc->history_size;
	sc->history[sc->needle] = 0;
}

int snis_scaling_strip_chart_inside(struct scaling_strip_chart *sc, int x, int y)
{
	x = sng_pixelx_to_screenx(x);
	y = sng_pixelx_to_screenx(y);

	if (x < sc->x)
		return 0;
	if (x > sc->x + sc->width)
		return 0;
	if (y < sc->y)
		return 0;
	if (y > sc->y + sc->height)
		return 0;
	return 1;

}

void snis_scaling_strip_chart_update_position(struct scaling_strip_chart *sc, int x, int y)
{
	sc->x = x;
	sc->y = y;
}

int snis_scaling_strip_chart_get_x(struct scaling_strip_chart *sc)
{
	return sc->x;
}

int snis_scaling_strip_chart_get_y(struct scaling_strip_chart *sc)
{
	return sc->y;
}

char *snis_strip_chart_get_label(struct strip_chart *sc)
{
	return sc->label;
}

char *snis_scaling_strip_chart_get_label(struct scaling_strip_chart *sc)
{
	return sc->label;
}
