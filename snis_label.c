#include <stdlib.h>
#include <string.h>

#include "snis_font.h"
#include "snis_typeface.h"
#include "snis_graph.h"
#define DEFINE_LABEL_GLOBALS
#include "snis_label.h"
#undef DEFINE_LABEL_GLOBALS

struct label {
	int x, y;
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
	strncpy(l->label, label, sizeof(l->label) - 1);
	l->label[sizeof(l->label) - 1] = '\0';
	l->color = color;
	l->font = font;
	return l;
}

void snis_label_draw(struct label *l)
{
	sng_set_foreground(l->color);
	sng_abs_xy_draw_string(l->label, l->font, l->x, l->y);
}

void snis_label_set_color(struct label *l, int color)
{
	l->color = color;
}

