#include <stdlib.h>
#include <string.h>

#include "snis_font.h"
#include "snis_typeface.h"
#include "snis_graph.h"
#define DEFINE_LABEL_GLOBALS
#include "snis_label.h"
#undef DEFINE_LABEL_GLOBALS
#include "string-utils.h"

struct label {
	int x, y, w, h;
	char label[20];
	int color;
	int font;
};

struct label *snis_label_init(int x, int y,
			char *label, int color, int font)
{
	struct label *l;

	l = malloc(sizeof(*l));
	l->x = x;
	l->y = y;
	strlcpy(l->label, label, sizeof(l->label));
	l->label[sizeof(l->label) - 1] = '\0';
	l->color = color;
	l->font = font;
	l->w = 0;
	l->h = 0;
	return l;
}

void snis_label_draw(struct label *l)
{
	sng_set_foreground(l->color);
	sng_abs_xy_draw_string(l->label, l->font, l->x, l->y);
	if (l->w == 0) {
		float x1, y1, x2, y2;
		sng_string_bounding_box(l->label, l->font, &x1, &y1, &x2, &y2);
		l->w = x2 - x1;
		l->h = y2 - y1;
	}
}

void snis_label_set_color(struct label *l, int color)
{
	l->color = color;
}

void snis_label_update_position(struct label *l, int x, int y)
{
	l->x = x;
	l->y = y;
}

int snis_label_get_x(struct label *l)
{
	return l->x;
}

int snis_label_get_y(struct label *l)
{
	return l->y;
}

int snis_label_inside(struct label *l, int x, int y)
{
	x = sng_pixelx_to_screenx(x);
	y = sng_pixelx_to_screenx(y);

	if (x < l->x)
		return 0;
	if (x > l->x + l->w)
		return 0;
	if (y < l->y)
		return 0;
	if (y > l->y + l->h)
		return 0;
	return 1;
}

char *snis_label_get_label(struct label *l)
{
	return l->label;
}
