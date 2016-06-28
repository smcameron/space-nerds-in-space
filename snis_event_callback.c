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

void schedule_callback2(struct event_callback_entry *e, struct callback_schedule_entry **s,
		const char *event, double param1, double param2)
{
	schedule_callback3(e, s, event, param1, param2, 0.0);
}

void schedule_callback(struct event_callback_entry *e, struct callback_schedule_entry **s,
		const char *event, double param1)
{
	schedule_callback3(e, s, event, param1, 0.0, 0.0);
}

struct callback_schedule_entry *next_scheduled_callback(struct callback_schedule_entry *e)
{
	return e->next;
}

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

static void add_callback(struct event_callback_entry *e, const char *callback)
{
	if (e->ncallbacks >= MAXCALLBACKS)
		return;
	e->callback[e->ncallbacks++] = strdup(callback);
}

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

int callback_schedule_entry_nparams(struct callback_schedule_entry *e)
{
	return e->nparams;
}

double callback_schedule_entry_param(struct callback_schedule_entry *e, int param)
{
	return e->param[param];
}

char *callback_name(struct callback_schedule_entry *e)
{
	return strdup(e->callback);
}

