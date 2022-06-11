/*
	Copyright (C) 2010 Stephen M. Cameron
	Author: Stephen M. Cameron

	This file is part of Spacenerds In Space.

	Spacenerds in Space is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.

	Spacenerds in Space is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with Spacenerds in Space; if not, write to the Free Software
	Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "space-part.h"

struct space_partition {
	int xdim, ydim;
	double minx, maxx, miny, maxy;
	double cell_width, cell_height;
	struct space_partition_entry **content;
	struct space_partition_entry *common;
	int offset;
};

static inline int get_cell(struct space_partition *p, int x, int y)
{
	if (x < 0 || x >= p->xdim || y < 0 || y >= p->ydim)
		return -1;
	return x * p->ydim + y;
}

struct space_partition *space_partition_init(int xdim, int ydim,
	double minx, double maxx, double miny, double maxy,
	int offset)
{
	struct space_partition *p;

	p = malloc(sizeof(*p));
	if (!p)
		return p;
	p->content = malloc(sizeof(*p->content) * xdim * ydim);
	if (!p->content) {
		free(p);
		return NULL;
	}
	memset(p->content, 0, sizeof(void *) * xdim * ydim);
	p->minx = minx;
	p->maxx = maxx;
	p->miny = miny;
	p->maxy = maxy;
	p->xdim = xdim;
	p->ydim = ydim;
	p->cell_width = (maxx - minx) / (double) xdim;
	p->cell_height = (maxy - miny) / (double) ydim;
	p->offset = offset; 
	p->common = NULL;
	return p;
}

void remove_space_partition_entry(struct space_partition *sp, struct space_partition_entry *e)
{
	struct space_partition_entry *prev, *next;
	struct space_partition_entry **head;

	if (e->cell < 0)
		head = &sp->common; 
	else
		head = &sp->content[e->cell]; 

	prev = e->prev;
	next = e->next;

	if (*head == e)
		*head = next;

	if (prev)
		prev->next = next;
	if (next)
		next->prev = prev;
	e->next = NULL;
	e->prev = NULL;
}

static void add_sp_entry(struct space_partition *p, struct space_partition_entry *e, int cell)
{
	struct space_partition_entry **c;

	if (cell < 0)
		c = &p->common;
	else
		c = &p->content[cell];

	e->cell = cell;
	if (!(*c)) {
		(*c) = e;
		e->next = NULL;
		e->prev = NULL;
		return;
	}
	(*c)->prev = e;
	e->next = *c;
	e->prev = NULL;
	(*c) = e;
}

void space_partition_update(struct space_partition *p,
		void *entity, double x, double y)
{
	int cellx, celly;
	struct space_partition_entry *e;
	int cell;

	e = (struct space_partition_entry *) ((unsigned char *) entity + p->offset);

	cellx = ((x - p->minx) / (p->maxx - p->minx)) * p->xdim;
	celly = ((y - p->miny) / (p->maxy - p->miny)) * p->ydim;
	cell = get_cell(p, cellx, celly);
	/* printf("%4.1f,%4.1f -> cellx, celly = %d, %d, cell = %d\n", x, y, cellx, celly, cell); */

	if (e->cell == cell)
		return;

	remove_space_partition_entry(p, e);
	add_sp_entry(p, e, cell);
}

void space_partition_free(struct space_partition *p)
{
	free(p->content);
	free(p);
}

void nearby_space_partitions(struct space_partition *p, void *entity,
				double x, double y, int cell[4])
{
	int xo, yo, i, cellx, celly;
	struct space_partition_entry *e = (struct space_partition_entry *)
					((unsigned char *) entity + p->offset);
	double cx, cy;

	cellx = ((x - p->minx) / (p->maxx - p->minx)) * p->xdim;
	celly = ((y - p->miny) / (p->maxy - p->miny)) * p->ydim;
	cx = (double) cellx * p->cell_width + 0.5 * p->cell_width + p->minx;
	cy = (double) celly * p->cell_height + 0.5 * p->cell_height + p->miny;

	/* printf("CXCY=%4.2f,%4.2f\n", cx, cy); */

	xo = x < cx ? -p->ydim : p->ydim;	
	yo = y < cy ? -1 : 1;	

	cell[0] = e->cell;
	cell[1] = e->cell + xo; 
	cell[2] = e->cell + yo; 
	cell[3] = e->cell + xo + yo;

	for (i = 0; i < 4; i++) 
		if (cell[i] < 0 || cell[i] >= p->xdim * p->ydim)
			cell[i] = -1;
}

struct space_partition_entry *space_partition_neighbors(struct space_partition *p, int cell)
{
	if (cell < 0 || cell >= p->xdim * p->ydim)
		return p->common;
	return (struct space_partition_entry *) p->content[cell];
}

static void process_cell(struct space_partition *p, __attribute__((unused)) void *entity,
				__attribute__((unused)) double x,
				__attribute__((unused))  double y,
                                void *context, space_partition_function fn, int cell)
{
	struct space_partition_entry *e, *i, *next;
	void *guy;

	e = space_partition_neighbors(p, cell);

	for (i = e; i != NULL; i = next) {
		next = i->next;
		guy = ((unsigned char *) i) - p->offset;
		fn(context, guy);
	}
}

void space_partition_process(struct space_partition *p, void *entity, double x, double y,
                                void *context, space_partition_function fn)
{
	int cell[4];
	int i;
	int common_processed = 0;

	nearby_space_partitions(p, entity, x, y, cell);

	for (i = 0; i < 4; i++) {
		if (cell[i] < 0) {
			if (common_processed)
				continue;
			common_processed = 1;
		}
		process_cell(p, entity, x, y, context, fn, cell[i]);
	}
}

#ifdef TEST_SPACE_PARTITION
#include <stddef.h>

struct thingy {
	double x, y;
	struct space_partition_entry e;
};

struct spcontext {
	struct thingy *t;
};

static void callback(__attribute__((unused)) void *context, __attribute__((unused)) void *whatever)
{
	printf("callback called\n");
}

int main(__attribute__((unused)) int argc, __attribute__((unused)) char *argv[])
{

	struct space_partition *sp;
	struct thingy t = { 0 };
	struct thingy t2 = { 0 };
	struct spcontext spc;
	int i;
	int cell[4];
	

	t.x = 0.0;
	t.y = 0.0;

	t2.x = 20.0;
	t2.y = 20.0;

	sp = space_partition_init(20, 20, -100, 100, -100, 100, offsetof(struct thingy, e));
	space_partition_update(sp, &t, t.x, t.y);
	space_partition_update(sp, &t2, t2.x, t2.y);

	for (i = 0; i < 45; i++) {
		t.x += 1.1;
		t.y += 0.3;
		space_partition_update(sp, &t, t.x, t.y);
		nearby_space_partitions(sp, &t, t.x, t.y, cell);
		printf("neighbor cells = %d, %d, %d, %d\n", cell[0], cell[1], cell[2], cell[3]);
	}
	t.x = 7;
	t.y = 7;
	space_partition_update(sp, &t, t.x, t.y);
	nearby_space_partitions(sp, &t, t.x, t.y, cell);
	printf("neighbor cells = %d, %d, %d, %d\n", cell[0], cell[1], cell[2], cell[3]);

	spc.t = &t;
	space_partition_process(sp, &t,  t.x, t.y, &spc, callback);

	return 0;
}
#endif

