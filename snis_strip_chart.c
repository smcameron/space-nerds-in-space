#include <stdlib.h>
#include <string.h>
#include <gtk/gtk.h>
#include <math.h>

#include "snis_font.h"
#include "snis_typeface.h"
#include "snis_graph.h"

#include "snis_strip_chart.h"

struct strip_chart {
	int x, y, width, height, color, warn_color, font, history_size, needle;
	char label[100];
	char warning_msg[100];
	uint8_t *history;
	uint8_t warning_on, warning_level;
};

struct strip_chart *snis_strip_chart_init(int x, int y, int width, int height, char *label, char *warning_msg,
			int color, int warn_color, uint8_t warning_level, int font, int history_size)
{
	struct strip_chart *sc = NULL;

	if (history_size > 1000)
		return NULL;

	sc = calloc(sizeof(*sc), 1);
	sc->history = calloc(1, history_size);
	sc->x = x;
	sc->y = y;
	sc->width = width;
	sc->height = height;
	sc->color = color;
	sc->warn_color = warn_color;
	sc->font = font;
	sc->history_size = history_size;
	strncpy(sc->label, label, sizeof(sc->label) - 1);
	strncpy(sc->warning_msg, warning_msg, sizeof(sc->warning_msg) - 1);
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
		sng_abs_xy_draw_string(sc->label, sc->font, ox, oy + h + snis_font_lineheight(sc->font));
	}
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
