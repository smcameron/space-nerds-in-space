#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "snis_pull_down_menu.h"
#include "snis_graph.h"
#include "snis_typeface.h"

struct pull_down_menu_item {
	char *name;
	void (*func)(void *);
};

struct pull_down_menu_column {
	int width;
	int nrows;
	struct pull_down_menu_item item[MAX_PULL_DOWN_ROWS_PER_COLUMN];
};

struct pull_down_menu {
	int x, y;
	int ncols;
	int font;
	int current_col, current_row;
	int current_physical_x, current_physical_y; /* mouse pos */
	struct pull_down_menu_column *col[MAX_PULL_DOWN_COLUMNS];
};

static int pull_down_menu_inside_col(struct pull_down_menu *m, int x, int col,
			int physical_x, int physical_y, int is_open)
{
	int limit;

	if (is_open)
		limit = m->col[col]->nrows;
	else
		limit = 1;

	if (physical_x < x)
		return 0;
	if (physical_x > x + m->col[col]->width)
		return 0;

	if (physical_y > 0 && physical_y < limit * (font_lineheight[m->font] + 6))
		return 1;
	return 0;
}

int pull_down_menu_inside(struct pull_down_menu *m, int physical_x, int physical_y)
{
	int i, x;

	x = 0;
	for (i = 0; i < m->ncols; i++) {
		if (pull_down_menu_inside_col(m, x, i, physical_x, physical_y, i == m->current_col))
			return 1;
		x += m->col[i]->width;
	}
	return 0;
}

void pull_down_menu_update_mouse_pos(struct pull_down_menu *m, int physical_x, int physical_y)
{
	int i, x, new_col = -1;

	m->current_physical_x = physical_x;
	m->current_physical_y = physical_y;
	x = 0;
	for (i = 0; i < m->ncols; i++) {
		if (pull_down_menu_inside_col(m, x, i, physical_x, physical_y, i == m->current_col)) {
			new_col = i;
			break;
		}
		x += m->col[i]->width;
	}
	if (new_col >= 0 && new_col < m->ncols) {
		if (physical_y > 0 && physical_y < m->col[new_col]->nrows * (font_lineheight[m->font] + 6))
			m->current_row =
				(int) (physical_y / (font_lineheight[m->font] + 6));
	}
	m->current_col = new_col;
}

struct pull_down_menu *create_pull_down_menu(int font)
{
	struct pull_down_menu *m;

	m = malloc(sizeof(*m));
	if (!m)
		return m;
	m->x = 10;
	m->y = 0;
	m->font = font;
	m->ncols = 0;
	m->current_col = -1;
	m->current_row = -1;
	memset(m->col, 0, sizeof(m->col));
	return m;
}

static void update_menu_column_width(struct pull_down_menu_column *c, int font)
{
	int i;
	float x1, y1, x2, y2;

	if (c->width != 0)
		return;
	for (i = 0; i < c->nrows; i++) {
		sng_string_bounding_box(c->item[i].name, font, &x1, &y1, &x2, &y2);
		if (fabsf(x2 - x1) + 10 > c->width)
			c->width = fabsf(x2 - x1) + 10;
	}
}

static void update_menu_widths(struct pull_down_menu *m)
{
	int i;

	for (i = 0; i < m->ncols; i++)
		update_menu_column_width(m->col[i], m->font);
}

static void draw_menu_col(struct pull_down_menu_column *c, float x, float y, int current_row, int font, int is_open)
{
	int i, limit;

	if (is_open)
		limit = c->nrows;
	else
		limit = 1;

	for (i = 0; i < limit; i++) {
		struct pull_down_menu_item *r = &c->item[i];
		sng_current_draw_line(x, y, x, y + font_lineheight[font] + 6);
		sng_current_draw_line(x + c->width, y, x + c->width, y + font_lineheight[font] + 6);
		if (i == 0 || i == limit - 1)
			sng_current_draw_line(x, y + font_lineheight[font] + 6,
				x + c->width, y + font_lineheight[font] + 6);
		y = y + font_lineheight[font];
		sng_abs_xy_draw_string(r->name, font, x + 4, y - 2);
		if (i == current_row && is_open)
			sng_abs_xy_draw_string(r->name, font, x + 5, y - 1);
		y = y + 6;
	}
}

void pull_down_menu_draw(struct pull_down_menu *m)
{
	int i, x;

	update_menu_widths(m);
	if (!pull_down_menu_inside(m, m->current_physical_x, m->current_physical_y))
		return;
	x = m->x;
	for (i = 0; i < m->ncols; i++) {
		draw_menu_col(m->col[i], x, m->y, m->current_row, m->font, m->current_col == i);
		x += m->col[i]->width;
	}
}

int pull_down_menu_add_column(struct pull_down_menu *m, char *column)
{
	struct pull_down_menu_column *c;
	if (!m)
		return -1;
	if (m->ncols >= MAX_PULL_DOWN_COLUMNS)
		return -1;
	c = malloc(sizeof(*c));
	if (!c)
		return -1;
	c->width = 0;
	memset(c->item, 0, sizeof(c->item));
	m->col[m->ncols] = c;
	m->col[m->ncols]->item[0].name = strdup(column);
	m->col[m->ncols]->item[0].func = NULL;
	c->nrows = 1;
	c->width = 0; /* So it will get recalculated */
	m->ncols++;
	return 0;
}

int pull_down_menu_add_row(struct pull_down_menu *m, char *column, char *row, void (*func)(void *))
{
	int i;
	if (!m)
		return -1;
	for (i = 0; i < m->ncols; i++) {
		struct pull_down_menu_column *c;
		struct pull_down_menu_item *r;
		c = m->col[i];
		if (!c) {
			fprintf(stderr, "BUG detected at %s:%d, m->col[%d] is NULL but m->ncols is %d\n",
				__FILE__, __LINE__, i, m->ncols);
			continue;
		}
		if (strcmp(c->item[0].name, column) != 0)
			continue;
		if (c->nrows >= MAX_PULL_DOWN_ROWS_PER_COLUMN)
			return -1;
		r = &c->item[c->nrows];
		r->name = strdup(row);
		r->func = func;
		c->nrows++;
		c->width = 0; /* So it will get recalculated */
		return 0;
	}
	return -1;
}

int pull_down_menu_button_press(struct pull_down_menu *m, int x, int y)
{
	if (m->current_col >= 0 && m->current_col < m->ncols &&
		m->current_row >= 0 && m->current_row < m->col[m->current_col]->nrows) {
			printf("selected %s/%s\n", m->col[m->current_col]->item[0].name,
					m->col[m->current_col]->item[m->current_row].name);
			if (m->col[m->current_col]->item[m->current_row].func)
				/* TODO: pass a cookie */
				m->col[m->current_col]->item[m->current_row].func(NULL);
			m->current_row = -1; /* deselect it */
			m->current_col = -1;
	}
	return 0;
}
