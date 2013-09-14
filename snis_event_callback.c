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

