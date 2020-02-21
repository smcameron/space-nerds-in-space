#ifndef SNIS_EVENT_CALLBACK_H__
#define SNIS_EVENT_CALLBACK_H__
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

/***************************************************

  We need to have some kind of list of callbacks that we can add to,
  then later scan from another thread.  This is because you can only call
  lua functions from the thread which created the lua state.  If other
  threads would like to call lua, they need to schedule their callbacks
  so the main thread can run them.

  How this is meant to be used:
  There are two opaque types:

	struct event_callback_entry;
	sturct callback_schedule_entry;

  event_callback_entry associates a type of "event", which has a name (a string)
  with a small list of callbacks (current max is 3 callbacks per event.)
  The event_callback_entry forms a linked list of such entries.  So if you get an
  event, you can figure out what callbacks need to be done by looking through a
  list of event_callback_entries.

  The callback_schedule_entry contains the name of a callback, how many params
  and the values of those params.  It is a linked list of callbacks that should
  be made.

  So, you can do this to add a callback entry to the schedule "my_schedule" to call
  "my_callback" with params 1.0, 2.0, 3.0:

  struct callback_schedule_entry *my_schedule = NULL;
  schedule_one_callback(&my_schedule, "my_callback", 1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0);

***************************************************/

struct event_callback_entry;
struct callback_schedule_entry;

#define MAX_LUA_CALLBACK_PARAMS (8)

/* Adds a new callback onto the schedule e */
void schedule_one_callback(struct callback_schedule_entry **e,
		const char *callback, double p1, double p2, double p3, double p4, double p5, double p6,
		double p7, double p8);

/* looks up the specified event in the list of callbacks for events, e, and
 * adds all the callbacks for that event (if found) into the schedule
 * with the given 3 params.
 */
void schedule_callback(struct event_callback_entry *e, struct callback_schedule_entry **s, 
                const char *callback, double param);
/* ugh, this is horrible, I'm a horrible person. */
/* looks up the specified event in the list of callbacks for events, e, and
 * adds all the callbacks for that event (if found) into the schedule
 * with the given 2 params (plus 0.0 as implicit 3rd param).
 */
void schedule_callback2(struct event_callback_entry *e, struct callback_schedule_entry **s, 
                const char *callback, double param1, double param2);
/* looks up the specified event in the list of callbacks for events, e, and
 * adds all the callbacks for that event (if found) into the schedule
 * with the given 1 param (plus 0.0, 0.0 as implicit 2nd and 3rd params).
 */
void schedule_callback3(struct event_callback_entry *e, struct callback_schedule_entry **s,
		const char *callback, double param1, double param2, double param3);

void schedule_callback4(struct event_callback_entry *e, struct callback_schedule_entry **s,
		const char *callback, double param1, double param2, double param3, double p4);

void schedule_callback5(struct event_callback_entry *e, struct callback_schedule_entry **s,
		const char *callback, double param1, double param2, double param3, double p4, double p5);

void schedule_callback6(struct event_callback_entry *e, struct callback_schedule_entry **s,
		const char *callback, double p1, double p2, double p3,
		double p4, double p5, double p6);

void schedule_callback7(struct event_callback_entry *e, struct callback_schedule_entry **s,
		const char *callback, double p1, double p2, double p3,
		double p4, double p5, double p6, double p7);

void schedule_callback8(struct event_callback_entry *e, struct callback_schedule_entry **s,
		const char *callback, double p1, double p2, double p3,
		double p4, double p5, double p6, double p7, double p8);

/* Frees everthing in a callback schedule */
void free_callback_schedule(struct callback_schedule_entry **e);

/* Returns the (newly allocated) name of the callback from the callback
 * schedule entry.  Caller is responsible for freeing it.
 */
char *callback_name(struct callback_schedule_entry *e);

/* Find out how many params a given callback schedule entry expects */
int callback_schedule_entry_nparams(struct callback_schedule_entry *e);

/* Return the value of the specified callback schedule entry param */
double callback_schedule_entry_param(struct callback_schedule_entry *e, int param);

/* looks up up the given event in *map, and creates a new callback for that
 * event and adds the new callback to that event.  IF the event isn't found,
 * it's added to map, with the new callback.
 */
void register_event_callback(const char *event, const char *callback,
				struct event_callback_entry **map);

/* looks up up the given event in *map, and deletes the specified callback for that
 * event.  IF the event isn't found, or does not have the specified callback, no
 * action is taken.
 */
void unregister_event_callback(const char *event, const char *callback,
				struct event_callback_entry **map);

/* Looks up the event in map, and returns the list of callbacks associated
 * with that event in the map.
 */
int callback_list(struct event_callback_entry *map, char *event, char **list[]);

/* Frees all the callbacks for all the events in map */
void free_event_callbacks(struct event_callback_entry **map);

/* literally returns e->next, useful for iterating through a callback schedule */
struct callback_schedule_entry *next_scheduled_callback(struct callback_schedule_entry *e);

/* Prints the list of registered callback/event tuples */
void print_registered_callbacks(struct event_callback_entry *map);

#endif
