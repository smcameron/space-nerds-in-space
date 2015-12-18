#ifndef SNIS_SERVER_TRACKER_H__
#define SNIS_SERVER_TRACKER_H__

#include "ssgl/ssgl.h"

struct server_tracker;

typedef void (*server_tracker_change_notifier)(void *cookie);

struct server_tracker *server_tracker_start(char *lobbyhost,
		server_tracker_change_notifier notifier, void *cookie);
int server_tracker_get_server_list(struct server_tracker *st,
					struct ssgl_game_server **server_list, int *nservers);
int server_tracker_get_multiverse_list(struct server_tracker *st,
					struct ssgl_game_server **server_list, int *nservers);
int server_tracker_stop(struct server_tracker *st);

#endif
