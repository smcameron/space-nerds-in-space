#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <pthread.h>

#include "snis_pull_down_menu.h"
#include "snis_graph.h"
#include "snis_typeface.h"


struct pull_down_menu_item {
	char *name;
	void (*func)(void *);
	void *cookie;
	int (*checkbox_function)(void *);
	void *checkbox_cookie;
	char *tooltip;
	int tooltip_timer;
};

#define PDM_TOOLTIP_DELAY (30) /* 1 second */

struct pull_down_menu_column {
	int width;
	int nrows;
	struct pull_down_menu_item item[MAX_PULL_DOWN_ROWS_PER_COLUMN];
};

struct pull_down_menu {
	int x, y, screen_width;
	int ncols;
	int font;
	int color;
	int current_col, current_row;
	int current_physical_x, current_physical_y; /* mouse pos */
	struct pull_down_menu_column *col[MAX_PULL_DOWN_COLUMNS];
	pthread_mutex_t mutex;
	float alpha;
	int gravity;
	pull_down_menu_tooltip_drawing_function tooltip_draw;
};

static int pull_down_menu_inside_col(struct pull_down_menu *m, int x, int col,
			int physical_x, int physical_y, int is_open)
{
	int limit;
	int sx, sy;

	sx = sng_pixelx_to_screenx(physical_x);
	sy = sng_pixely_to_screeny(physical_y);

	if (is_open)
		limit = m->col[col]->nrows;
	else
		limit = 1;

	if (sx < x)
		return 0;
	if (sx > x + m->col[col]->width)
		return 0;

	if (sy >= 0 && sy < limit * (font_lineheight[m->font] + 6))
		return 1;
	return 0;
}

static int pull_down_menu_inside_internal(struct pull_down_menu *m, int physical_x, int physical_y, int lock)
{
	int i, x, start, end, inc;

	if (lock)
		pthread_mutex_lock(&m->mutex);
	if (m->gravity) {
		x = m->screen_width;
		start = m->ncols - 1;
		end = -1;
		inc = -1;
	} else {
		x = 0;
		start = 0;
		end = m->ncols;
		inc = 1;
	}
	for (i = start; i != end; i += inc) {
		if (m->gravity)
			x -= m->col[i]->width;
		if (pull_down_menu_inside_col(m, x, i, physical_x, physical_y, i == m->current_col)) {
			if (lock)
				pthread_mutex_unlock(&m->mutex);
			return 1;
		}
		if (!m->gravity)
			x += m->col[i]->width;
	}
	if (lock)
		pthread_mutex_unlock(&m->mutex);
	return 0;
}

int pull_down_menu_inside(struct pull_down_menu *m, int physical_x, int physical_y)
{
	return pull_down_menu_inside_internal(m, physical_x, physical_y, 1);
}

void pull_down_menu_update_mouse_pos(struct pull_down_menu *m, int physical_x, int physical_y)
{
	int i, x, sy, start, end, inc, new_col = -1, new_row = -1;

	m->current_physical_x = physical_x;
	m->current_physical_y = physical_y;
	if (m->gravity) {
		x = m->screen_width;
		start = m->ncols - 1;
		end = -1;
		inc = -1;
	} else {
		x = 0;
		start = 0;
		end = m->ncols;
		inc = 1;
	}
	for (i = start; i != end; i += inc) {
		if (m->gravity)
			x -= m->col[i]->width;
		if (pull_down_menu_inside_col(m, x, i, physical_x, physical_y, i == m->current_col)) {
			new_col = i;
			break;
		}
		if (!m->gravity)
			x += m->col[i]->width;
	}
	if (new_col >= 0 && new_col < m->ncols) {
		sy = sng_pixely_to_screeny(physical_y);
		if (sy > 0 && sy < m->col[new_col]->nrows * (font_lineheight[m->font] + 6)) {
			new_row = sy / (font_lineheight[m->font] + 6);
		}
	}
	/* Adjust tooltip timer for the previous current row */
	if (new_col == m->current_col && new_row == m->current_row) {
		if (m->current_row >= 0 && m->current_col >= 0) {
			/* Current item has not changed, decrement tooltip timer */
			if (m->col[m->current_col]->item[m->current_row].tooltip_timer > 0)
				m->col[m->current_col]->item[m->current_row].tooltip_timer--;
		}
	}
	m->current_col = new_col;
	m->current_row = new_row;
}

struct pull_down_menu *create_pull_down_menu(int font, int screen_width)
{
	struct pull_down_menu *m;

	m = malloc(sizeof(*m));
	if (!m)
		return m;
	m->x = 10;
	m->screen_width = screen_width;
	m->y = 0;
	m->font = font;
	m->ncols = 0;
	m->current_col = -1;
	m->current_row = -1;
	memset(m->col, 0, sizeof(m->col));
	pthread_mutex_init(&m->mutex, NULL);
	m->alpha = -1; /* no alpha */
	m->gravity = 0;
	m->tooltip_draw = NULL;
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
		if (fabsf(x2 - x1) + 10 + 20 * (c->item[i].checkbox_function != NULL) > c->width)
			c->width = fabsf(x2 - x1) + 10 + 20 * (c->item[i].checkbox_function != NULL);
	}
}

static void update_menu_widths(struct pull_down_menu *m)
{
	int i;

	for (i = 0; i < m->ncols; i++)
		update_menu_column_width(m->col[i], m->font);
}

static void draw_menu_col(struct pull_down_menu *m, int col, float x, float y, int current_row, int font, int is_open)
{
	int i, limit, cb, cbw;
	struct pull_down_menu_column *c = m->col[col];

	if (is_open)
		limit = c->nrows;
	else
		limit = 1;

	for (i = 0; i < limit; i++) {
		struct pull_down_menu_item *r = &c->item[i];
		if (r->checkbox_function) {
			cb = r->checkbox_function(r->checkbox_cookie);
			cbw = 20;
		} else {
			cb = 0;
			cbw = 0;
		}
		sng_set_foreground_alpha(BLACK, m->alpha);
		sng_current_draw_rectangle(1, x, y, c->width, font_lineheight[font] + 6);
		sng_set_foreground(m->color);
		sng_current_draw_line(x, y, x, y + font_lineheight[font] + 6);
		sng_current_draw_line(x + c->width, y, x + c->width, y + font_lineheight[font] + 6);
		if (i == 0 || i == limit - 1)
			sng_current_draw_line(x, y + font_lineheight[font] + 6,
				x + c->width, y + font_lineheight[font] + 6);
		if (cbw) {
			float x1, y1, x2, y2;
			x1 = x + 5;
			x2 = x1 + 16;
			y1 = y + 8;
			y2 = y1 + 16;
			sng_current_draw_rectangle(0, x1, y1, 16, 16);
			if (cb) {
				sng_current_draw_line(x1, y1, x2, y2);
				sng_current_draw_line(x1, y2, x2, y1);
			}
		}
		y = y + font_lineheight[font];
		sng_abs_xy_draw_string(r->name, font, x + 4 + cbw, y - 2);
		if (i == current_row && is_open)
			sng_abs_xy_draw_string(r->name, font, x + 5 + cbw, y - 1);
		y = y + 6;
		/* Reset tooltip delays for non-current items */
		if ((i != current_row || !is_open) && r->tooltip)
			r->tooltip_timer = PDM_TOOLTIP_DELAY;
	}
}

static void pull_down_menu_draw_current_tooltip(struct pull_down_menu *m)
{
	struct pull_down_menu_item *r;

	if (!m->tooltip_draw)
		return;
	if (m->current_col < 0)
		return;
	if (m->current_row < 0)
		return;
	r = &m->col[m->current_col]->item[m->current_row];
	if (r->tooltip_timer > 0)
		return; /* Don't draw the tooltip until they hover for a bit. */
	if (r->tooltip)
		m->tooltip_draw(m->current_physical_x, m->current_physical_y, r->tooltip);
}

void pull_down_menu_draw(struct pull_down_menu *m)
{
	int i, x, start, end, inc;

	pthread_mutex_lock(&m->mutex);
	update_menu_widths(m);
	if (!pull_down_menu_inside_internal(m, m->current_physical_x, m->current_physical_y, 0)) {
		pthread_mutex_unlock(&m->mutex);
		return;
	}
	x = m->x;
	if (m->gravity) {
		start = m->ncols - 1;
		end = -1;
		inc = -1;
	} else {
		start = 0;
		end = m->ncols;
		inc = 1;
	}

	for (i = start; i != end; i += inc) {
		if (m->gravity)
			x -= m->col[i]->width;
		draw_menu_col(m, i, x, m->y, m->current_row, m->font, m->current_col == i);
		if (!m->gravity)
			x += m->col[i]->width;
	}
	pull_down_menu_draw_current_tooltip(m);
	pthread_mutex_unlock(&m->mutex);
}

static int pull_down_menu_add_column_internal(struct pull_down_menu *m, char *column, int lock)
{
	struct pull_down_menu_column *c;
	if (!m)
		return -1;
	if (lock)
		pthread_mutex_lock(&m->mutex);
	if (m->ncols >= MAX_PULL_DOWN_COLUMNS) {
		if (lock)
			pthread_mutex_unlock(&m->mutex);
		return -1;
	}
	c = malloc(sizeof(*c));
	if (!c) {
		if (lock)
			pthread_mutex_unlock(&m->mutex);
		return -1;
	}
	c->width = 0;
	memset(c->item, 0, sizeof(c->item));
	m->col[m->ncols] = c;
	m->col[m->ncols]->item[0].name = strdup(column);
	m->col[m->ncols]->item[0].func = NULL;
	m->col[m->ncols]->item[0].checkbox_function = NULL;
	m->col[m->ncols]->item[0].checkbox_cookie = NULL;
	c->nrows = 1;
	c->width = 0; /* So it will get recalculated */
	m->ncols++;
	if (lock)
		pthread_mutex_unlock(&m->mutex);
	return 0;
}

int pull_down_menu_add_column(struct pull_down_menu *m, char *column)
{
	return pull_down_menu_add_column_internal(m, column, 1);
}

static int pull_down_menu_add_row_internal(struct pull_down_menu *m,
			char *column, char *row, void (*func)(void *), void *cookie, int lock)
{
	int i;
	if (!m)
		return -1;
	if (lock)
		pthread_mutex_lock(&m->mutex);
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
		if (c->nrows >= MAX_PULL_DOWN_ROWS_PER_COLUMN) {
			if (lock)
				pthread_mutex_unlock(&m->mutex);
			return -1;
		}
		r = &c->item[c->nrows];
		r->name = strdup(row);
		r->func = func;
		r->checkbox_function = NULL;
		r->checkbox_cookie = NULL;
		r->cookie = cookie;
		r->tooltip = NULL;
		c->nrows++;
		c->width = 0; /* So it will get recalculated */
		if (lock)
			pthread_mutex_unlock(&m->mutex);
		return 0;
	}
	if (lock)
		pthread_mutex_unlock(&m->mutex);
	return -1;
}

int pull_down_menu_add_row(struct pull_down_menu *m, char *column, char *row, void (*func)(void *), void *cookie)
{
	return pull_down_menu_add_row_internal(m, column, row, func, cookie, 1);
}

int pull_down_menu_button_press(struct pull_down_menu *m, int x, int y)
{
	struct pull_down_menu_item *row;

	if (!m)
		return 0;
	pthread_mutex_lock(&m->mutex);
	if (m->current_col >= 0 && m->current_col < m->ncols &&
		m->current_row >= 0 && m->current_row < m->col[m->current_col]->nrows) {
			row = &m->col[m->current_col]->item[m->current_row];
			if (row->func)
				row->func(row->cookie);
			m->current_row = -1; /* deselect it */
			m->current_col = -1;
	}
	pthread_mutex_unlock(&m->mutex);
	return 0;
}

void pull_down_menu_set_color(struct pull_down_menu *m, int color)
{
	pthread_mutex_lock(&m->mutex);
	m->color = color;
	pthread_mutex_unlock(&m->mutex);
}

void pull_down_menu_set_background_alpha(struct pull_down_menu *m, float alpha)
{
	if (alpha < -1.0)
		alpha = -1.0;
	if (alpha > 1.0)
		alpha = 1.0;
	m->alpha = alpha;
}

static struct pull_down_menu_item *find_menu_item(struct pull_down_menu *m, char *column, char *row)
{
	int i;
	struct pull_down_menu_column *c;

	for (i = 0; i < m->ncols; i++) {
		c = m->col[i];
		if (!c) {
			fprintf(stderr, "BUG detected at %s:%d, m->col[%d] is NULL but m->ncols is %d\n",
				__FILE__, __LINE__, i, m->ncols);
			continue;
		}
		if (strcmp(c->item[0].name, column) == 0)
			goto found_column;
	}
	return NULL;

found_column:
	for (i = 1; i < c->nrows; i++) {
		if (strcmp(c->item[i].name, row) == 0)
			return &c->item[i];
	}
	return NULL;
}

void pull_down_menu_set_checkbox_function(struct pull_down_menu *m, char *column, char *row,
						int (*checkbox_function)(void *cookie), void *cookie)
{
	struct pull_down_menu_item *r;

	pthread_mutex_lock(&m->mutex);
	r = find_menu_item(m, column, row);
	if (!r) {
		pthread_mutex_unlock(&m->mutex);
		return;
	}
	r->checkbox_function = checkbox_function;
	r->checkbox_cookie = cookie;
	update_menu_widths(m);
	pthread_mutex_unlock(&m->mutex);
}

static void pull_down_menu_clear_internal(struct pull_down_menu *m, int lock)
{
	int i, j;

	if (!m)
		return;
	if (lock)
		pthread_mutex_lock(&m->mutex);
	for (i = 0; i < m->ncols; i++) {
		if (m->col[i]) {
			for (j = 0; j < m->col[i]->nrows; j++) {
				if (m->col[i]->item[j].name) {
					free(m->col[i]->item[j].name);
					m->col[i]->item[j].name = NULL;
				}
				if (m->col[i]->item[j].tooltip) {
					free(m->col[i]->item[j].tooltip);
					m->col[i]->item[j].tooltip = NULL;
				}
			}
			free(m->col[i]);
			m->col[i] = NULL;
		}
	}
	m->ncols = 0;
	if (lock)
		pthread_mutex_unlock(&m->mutex);
}

void pull_down_menu_clear(struct pull_down_menu *m)
{
	pull_down_menu_clear_internal(m, 1);
}

void pull_down_menu_copy(struct pull_down_menu *dest, struct pull_down_menu *src)
{
	int i, j;

	pthread_mutex_lock(&dest->mutex);
	pthread_mutex_lock(&src->mutex);

	pull_down_menu_clear_internal(dest, 0);
	dest->x = src->x;
	dest->y = src->y;
	dest->font = src->font;
	dest->color = src->color;
	dest->ncols = 0;
	dest->current_col = -1;
	dest->current_row = -1;
	dest->current_physical_x = src->current_physical_x;
	dest->current_physical_y = src->current_physical_y;

	for (i = 0; i < src->ncols; i++) {
		if (src->col[i]) {
			pull_down_menu_add_column_internal(dest, src->col[i]->item[0].name, 0);
			for (j = 1; j < src->col[i]->nrows; j++) {
				struct pull_down_menu_item *r = &src->col[i]->item[j];
				pull_down_menu_add_row_internal(dest, src->col[i]->item[0].name,
								r->name, r->func, r->cookie, 0);
				dest->col[i]->item[j].checkbox_function = r->checkbox_function;
				dest->col[i]->item[j].checkbox_cookie = r->checkbox_cookie;
				if (r->tooltip)
					dest->col[i]->item[j].tooltip = strdup(r->tooltip);
			}
		} else {
			dest->col[i] = NULL;
		}
	}

	pthread_mutex_unlock(&src->mutex);
	pthread_mutex_unlock(&dest->mutex);
}

void pull_down_menu_set_gravity(struct pull_down_menu *pdm, int right)
{
	pdm->gravity = !!right;
	if (pdm->gravity)
		pdm->x = pdm->screen_width;
	else
		pdm->x = 10;
}

void pull_down_menu_set_tooltip_drawing_function(struct pull_down_menu *pdm, pull_down_menu_tooltip_drawing_function f)
{
	pdm->tooltip_draw = f;
}

int pull_down_menu_add_tooltip(struct pull_down_menu *pdm, char *column, char *row, char *tooltip)
{
	struct pull_down_menu_item *item;

	pthread_mutex_lock(&pdm->mutex);
	item = find_menu_item(pdm, column, row);
	if (item) {
		item->tooltip = strdup(tooltip);
		pthread_mutex_unlock(&pdm->mutex);
		return 0;
	}
	pthread_mutex_unlock(&pdm->mutex);
	return 1;
}
