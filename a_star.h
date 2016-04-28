#ifndef A_STAR_H__
#define A_STAR_H__
/*
	Copyright (C) 2016 Stephen M. Cameron
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

struct a_star_path {
	int node_count;
	__extension__ void *path[0];
};

typedef float (*a_star_node_cost_fn)(void *context, void *first, void *second);
typedef void *(*a_star_neighbor_iterator_fn)(void *context, void *node, int neighbor);

/**********************
 *
 * a_star() runs the 'A*' algorithm to find a path from the start to the goal.
 *
 * context, start, and goal are opaque pointers which the algorithm doesn't look
 * at (except to compare pointer values for equality).
 *
 * maxnodes is the maximum number of nodes the algorithm will encounter (ie.
 *	the number of possible locations in a maze you are traversing.)
 *
 * distance is a function which you provide which returns the distance between
 *	two nodes
 *
 * cost_estimate is similar to distance but provides a heuristic cost estimate
 *	between two nodes
 *
 * nth_neighbor returns the nth neighbor of a given node, or NULL if all
 *	n is greater than the number of neighbors - 1.
 *
 * The return value is an allocated struct a_star_path.  the node_count is the
 * number of nodes in the path, and the path array is a pointer to an array of
 * node_count nodes.
 *
 * See a_star_test.c for example of how to use this.
 *
 *********************/

struct a_star_path *a_star(void *context,
		void *start,
		void *goal,
		int maxnodes,
		a_star_node_cost_fn distance,
		a_star_node_cost_fn cost_estimate,
		a_star_neighbor_iterator_fn nth_neighbor);
#endif
