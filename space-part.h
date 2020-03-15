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


/* This is a generic space partitioning system for collision detection.
 * If you have a bunch of things with x,y coordinates, you can set up a
 * partitioning so that if you need to see which other things are near
 * a given thing (for e.g. collision detection), you can do so without
 * doing N*N comparisons.
 *
 * The usage pattern is as follows:
 *
 * 1.  Put a struct space_partition_entry into your "thing" data structure.
 *
 * struct my_thing {
 *	int blah;
 *	int x, y;
 *	char whatever[100];
 *      struct space_partition_entry spe;
 *      struct whateverman yeah;
 * };
 *
 * 2.  Set up the space partition. Suppose your x,y coordinates range from
 *     -100000 to +100000 in x and y.  Suppose you want to divide this up into
 *     a 40x40 grid.
 *
 * struct space_partition *sp = space_partition(40, 40, -100000, 100000, -100000, 100000,
 *						offsetof(struct my_thing, spe));
 *
 * You should choose your minx, maxx, miny, maxy values so that things are unlikely to
 * go outside these bounds.  Things outside those bounds end up in a special "common"
 * cell, and if there are a lot of things that end up in the common cell, then things
 * degenerate to an O(n^2) algorithm.
 *
 * 3. Whenever you move a my_thing, update the space partition...
 *
 * void set_thing_location(struct my_thing *t, int x, int y)
 * {
 *    t->x = x;
 *    t->y = y;
 *    space_partition_update(sp, t, x, y);
 * }
 *
 * 4. Whenever you want to do collision detection, call space_partition_process() with
 *    your custom collision detection function.  It has two parameters. The first will
 *    be near neighbor things in the space partition, the 2nd is whatever you want, but
 *    typically is used for the thing you're testing collisions with.
 *
 * void my_collision_function(void *t1, void *context)
 * {
 *    struct my_thing *thing1 = t1;
 *    struct my_thing *thing2 = context;
 *    fine_collision_detection(thing1, thing2); // you write fine_collision_detection()
 * }
 *
 * space_partition_process(sp, some_thing, some_thing->x, some_thing->y, some_thing, my_collision_function);
 *
 * Note that the 5th parameter can be whatever you want. It will be passed along as the "context"
 * parameter.  Here we use it to pass along the thing we're testing for collisions with.
 *
 * That's basically it.
 */

struct space_partition;

struct space_partition_entry {
	struct space_partition_entry *next, *prev;
	int cell;
};

typedef void (*space_partition_function)(void *entity, void *context);

/* Create a new space partition scheme.
 * xdim, ydim are the dimensions of the 2D grid you want to create.
 * minx, maxx, miny, maxy define the range of x,y values you expect to see.
 * offset is the offset of the struct space_partition_entry within the structure
 * you're tracking.  e.g.: "offsetof(struct my_struct, spe)"
 */
struct space_partition *space_partition_init(int xdim, int ydim,
	double minx, double maxx, double miny, double maxy,
	int offset);

/* Whenever one of the entities you're tracking moves, you should call space_partition_update
 * to update it with the correct cell in the space partition.
 */
void space_partition_update(struct space_partition *p,
		void *entity, double x, double y);

/* Frees everything allocated for the space partition by space_partition_init(). */
void space_partition_free(struct space_partition *partition);

/* Returns in cell[] 4 neighboring cells to the cell in which entity resides. This is not
 * really meant to be called by outside programs, it is a helper for space_partition_process
 */
void nearby_space_partitions(struct space_partition *p, void *entity, double x, double y, int cell[4]);


/* Given a cell number obtained from nearby_space_partitions, returns a linked list of
 * struct space_partition_entry within that cell.
 */
struct space_partition_entry *space_partition_neighbors(struct space_partition *p, int cell);

/* Calls fn() for every nearby neighbor entity to the specified entity, passing the neighbor
 * entity and context to fn(). Typically context is used to pass the entity, but another
 * common case is to pass a pointer to a struct containing the entity and also other
 * information that fn() might need.
 */
void space_partition_process(struct space_partition *p, void *entity, double x, double y,
				void *context, space_partition_function fn);

/* Removes an entity from the space partition (e.g. if an entity is removed from the
 * simulation/game, you no longer need to track it in the space partition.) */
void remove_space_partition_entry(struct space_partition *p, struct space_partition_entry *e);

#endif
