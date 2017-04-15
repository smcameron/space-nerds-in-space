#ifndef SNIS_SERVER_TRACKER_H__
#define SNIS_SERVER_TRACKER_H__
/*
	Copyright (C) 2015 Stephen M. Cameron
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

#include "ssgl/ssgl.h"

struct server_tracker;

typedef void (*server_tracker_change_notifier)(void *cookie);

/* server_tracker_start() starts the server tracker thread.
 *
 * The server tracker thread connects to the lobby server, and maintains
 * two lists, one for the current snis_servers and one for the current
 * snis_multiverse servers.  It updates these lists (by polling the lobby
 * server) every 20 seconds (unless either list is empty, in which case it
 * updates these lists every 1 second. Whenever either list changes (what
 * it receives from the lobby server is different than what it previously
 * had recorded) it will call the callback function.
 *
 * The server tracker thread runs until server_tracker_stop() is called.
 */
struct server_tracker *server_tracker_start(char *lobbyhost,
		server_tracker_change_notifier notifier, void *cookie);

/* server_tracker_get_server_list allocates and makes a copy of its current list
 * of snis_server processes and returns it in server_list, and nservers
 */
int server_tracker_get_server_list(struct server_tracker *st,
					struct ssgl_game_server **server_list, int *nservers);

/* server_tracker_get_multiverse_list allocates and makes a copy of its current list
 * of snis_multiverse processes and returns it in server_list, and nservers
 */
int server_tracker_get_multiverse_list(struct server_tracker *st,
					struct ssgl_game_server **server_list, int *nservers);

/* server_tracker_stop stops the server tracker thread */
int server_tracker_stop(struct server_tracker *st);

#endif
