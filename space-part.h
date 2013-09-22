#ifndef SPACE_PART_H__
#define SPACE_PART_H__
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

struct space_partition;

struct space_partition_entry {
	struct space_partition_entry *next, *prev;
	int cell;
};

typedef void (*space_partition_function)(void *entity, void *context);

struct space_partition *space_partition_init(int xdim, int ydim,
	double minx, double maxx, double miny, double maxy,
	int offset);

void space_partition_update(struct space_partition *p,
		void *entity, double x, double y);

void space_partition_free(struct space_partition *partition);

void nearby_space_partitions(struct space_partition *p, void *entity, double x, double y, int cell[4]);

struct space_partition_entry *space_partition_neighbors(struct space_partition *p, int cell);

void space_partition_process(struct space_partition *p, void *entity, double x, double y,
				void *context, space_partition_function fn);

void remove_space_partition_entry(struct space_partition *p, struct space_partition_entry *e);

#endif
