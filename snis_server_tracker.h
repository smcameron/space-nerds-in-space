#ifndef SNIS_SERVER_TRACKER_H__
#define SNIS_SERVER_TRACKER_H__

#include "ssgl/ssgl.h"

struct server_tracker;

struct server_tracker *server_tracker_start(char *lobbyhost);
int server_tracker_get_server_list(struct server_tracker *st,
					struct ssgl_game_server **server_list, int *nservers);
int server_tracker_stop(struct server_tracker *st);

#endif
