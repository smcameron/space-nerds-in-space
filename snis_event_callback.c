/*
	Copyright (C) 2013 Stephen M. Cameron
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

#include <stdlib.h>
#include <string.h>

#include "snis_event_callback.h"

/* for now, this seems like plenty, probably 3x too much */
#define MAXCALLBACKS (3)
struct event_callback_entry {
	char *event;
	int ncallbacks;
	char *callback[MAXCALLBACKS];
	struct event_callback_entry *next;
};

struct callback_schedule_entry {
	char *callback;
	int nparams;
	double param[3];
	struct callback_schedule_entry *next;
};

/* Adds a new callback onto the schedule e */
void schedule_one_callback(struct callback_schedule_entry **e,
		const char *callback, double param1, double param2, double param3)
{
	struct callback_schedule_entry *newone;
	newone = calloc(sizeof(**e), 1);
	newone->callback = strdup(callback);
	newone->nparams = 3;
	newone->param[0] = param1;
	newone->param[1] = param2;
	newone->param[2] = param3;

	if (!*e) {
		*e = newone;
	} else {
		newone->next = *e;
		*e = newone;
	}
}

/* looks up the specified event in the list of callbacks for events, e, and
 * adds all the callbacks for that event (if found) into the schedule
 * with the given 3 params.
 */
void schedule_callback3(struct event_callback_entry *e, struct callback_schedule_entry **s,
		const char *event, double param1, double param2, double param3)
{
	struct event_callback_entry *i, *next;
	int j;

	for (i = e; i != NULL; i = next) {
		next = i->next;
		if (strcmp(i->event, event))
			continue;
		for (j = 0; j < i->ncallbacks; j++) {
			schedule_one_callback(s, i->callback[j], param1, param2, param3);
			break;
		}
		return;
	}
}

/* looks up the specified event in the list of callbacks for events, e, and
 * adds all the callbacks for that event (if found) into the schedule
 * with the given 2 params (plus 0.0 as implicit 3rd param).
 */
void schedule_callback2(struct event_callback_entry *e, struct callback_schedule_entry **s,
		const char *event, double param1, double param2)
{
	schedule_callback3(e, s, event, param1, param2, 0.0);
}

/* looks up the specified event in the list of callbacks for events, e, and
 * adds all the callbacks for that event (if found) into the schedule
 * with the given 2 params (plus 0.0, 0.0 as implicit 2nd and 3rd param).
 */
void schedule_callback(struct event_callback_entry *e, struct callback_schedule_entry **s,
		const char *event, double param1)
{
	schedule_callback3(e, s, event, param1, 0.0, 0.0);
}

/* literally returns e->next, useful for iterating through a callback schedule */
struct callback_schedule_entry *next_scheduled_callback(struct callback_schedule_entry *e)
{
	return e->next;
}

/* Allocates and initializes a new event callback entry */
static struct event_callback_entry *init_new_event_callback(const char *event, const char *callback)
{
	struct event_callback_entry *e;

	e = calloc(sizeof(*e), 1);
	e->next = NULL;
	e->event = strdup(event);
	e->callback[0] = strdup(callback);
	e->ncallbacks = 1;
	return e;
}

/* Adds a callback to an list of callbacks associated with an event (max 3) */
static void add_callback(struct event_callback_entry *e, const char *callback)
{
	if (e->ncallbacks >= MAXCALLBACKS)
		return;
	e->callback[e->ncallbacks++] = strdup(callback);
}

/* looks up up the given event in *map, and creates a new callback for that
 * event and adds the new callback to that event.  IF the event isn't found,
 * it's added to map, with the new callback.
 */
void register_event_callback(const char *event, const char *callback,
				struct event_callback_entry **map)
{
	struct event_callback_entry *i, *last;

	if (!*map) {
		*map = init_new_event_callback(event, callback);
		return;
	}

	for (i = *map; i != NULL; i = i->next) {
		if (strcmp(event, i->event) == 0) {
			add_callback(i, callback);
			return;
		}
		last = i;
	} 
	last->next = init_new_event_callback(event, callback);
}

/* Looks up the event in map, and returns the list of callbacks associated
 * with that event in the map.
 */
int callback_list(struct event_callback_entry *map, char *event, char **list[])
{
	struct event_callback_entry *i;

	for (i = map; i != NULL; i = i->next) {
		if (strcmp(i->event, event) == 0) {
			*list = i->callback;
			return i->ncallbacks;
		}
	}
	return 0;
}

/* Frees all the callbacks for all the events in map */
void free_event_callbacks(struct event_callback_entry **map)
{
	struct event_callback_entry *i, *next;
	int j;

	for (i = *map; i != NULL; i = next) {
		next = i->next;
		free(i->event);
		for (j = 0; j < i->ncallbacks; j++) 
			free(i->callback[j]);
		free(i);
	}
	*map = NULL;
}

/* Frees everthing in a callback schedule */
void free_callback_schedule(struct callback_schedule_entry **e)
{
	struct callback_schedule_entry *i, *next;

	for (i = *e; i != NULL; i = next) {
		next = i->next;
		free(i->callback);
		free(i);
	}
	*e = NULL;
}

/* Find out how many params a given callback schedule entry expects */
int callback_schedule_entry_nparams(struct callback_schedule_entry *e)
{
	return e->nparams;
}

/* Return the value of the specified callback schedule entry param */
double callback_schedule_entry_param(struct callback_schedule_entry *e, int param)
{
	return e->param[param];
}

/* Returns the (newly allocated) name of the callback from the callback
 * schedule entry.  Caller is responsible for freeing it.
 */
char *callback_name(struct callback_schedule_entry *e)
{
	return strdup(e->callback);
}

