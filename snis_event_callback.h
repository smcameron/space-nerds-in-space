#ifndef SNIS_EVENT_CALLBACK_H__
#define SNIS_EVENT_CALLBACK_H__

struct event_callback_entry;

void register_event_callback(const char *event, const char *callback,
				struct event_callback_entry **map);
int callback_list(struct event_callback_entry *map, char *event, char **list[]);
void free_event_callbacks(struct event_callback_entry **map);

#endif
