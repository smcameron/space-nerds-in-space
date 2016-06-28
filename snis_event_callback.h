#ifndef SNIS_EVENT_CALLBACK_H__
#define SNIS_EVENT_CALLBACK_H__

struct event_callback_entry;
struct callback_schedule_entry;

void schedule_one_callback(struct callback_schedule_entry **e,
		const char *callback, double param1, double param2, double param3);
void schedule_callback(struct event_callback_entry *e, struct callback_schedule_entry **s, 
                const char *callback, double param);
/* ugh, this is horrible, I'm a horrible person. */
void schedule_callback2(struct event_callback_entry *e, struct callback_schedule_entry **s, 
                const char *callback, double param1, double param2);
void schedule_callback3(struct event_callback_entry *e, struct callback_schedule_entry **s, 
                const char *callback, double param1, double param2, double param3);

void free_callback_schedule(struct callback_schedule_entry **e);
char *callback_name(struct callback_schedule_entry *e);
int callback_schedule_entry_nparams(struct callback_schedule_entry *e);
double callback_schedule_entry_param(struct callback_schedule_entry *e, int param);
void register_event_callback(const char *event, const char *callback,
				struct event_callback_entry **map);
int callback_list(struct event_callback_entry *map, char *event, char **list[]);
void free_event_callbacks(struct event_callback_entry **map);
struct callback_schedule_entry *next_scheduled_callback(struct callback_schedule_entry *e);

#endif
