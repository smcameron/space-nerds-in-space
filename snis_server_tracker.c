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

#include "snis_server_tracker.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/types.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <arpa/inet.h> /* for ntohs */
#include <pthread.h>

#include "ssgl/ssgl.h"

struct server_tracker {
	char *lobbyhost;
	pthread_t thread;
	pthread_attr_t thread_attr;
	int sock;
	pthread_mutex_t mutex;
	int time_to_quit;
	struct ssgl_game_server *game_server;
	int game_server_count;
};

int server_tracker_stop(struct server_tracker *st)
{
	pthread_mutex_lock(&st->mutex);
	st->time_to_quit = 1;
	pthread_mutex_unlock(&st->mutex);
	return 0;
}

static void print_game_servers(struct ssgl_game_server *game_server, int game_server_count)
{
#if 0
	int i;

	printf("IP addr/port       %15s %20s %15s %20s\n",
		"Game Type", "Instance/Map", "Server Nickname", "Location");
	printf("---------------------------------------------------------------------\n");
	for (i = 0; i < game_server_count; i++) {
		unsigned char *x = (unsigned char *) &game_server[i].ipaddr;
		printf("%d.%d.%d.%d/%d %15s %20s %15s %20s\n",
			x[0], x[1], x[2], x[3],
			ntohs(game_server[i].port),
			game_server[i].game_type,
			game_server[i].game_instance,
			game_server[i].server_nickname,
			game_server[i].location);
	}
	printf("\n");
#endif
}

static void *server_tracker_thread(void *tracker_info)
{
	struct server_tracker *st = tracker_info;
	struct ssgl_game_server *game_server = NULL;
	struct ssgl_client_filter filter;
	int game_server_count;
	int rc, time_to_quit = 0;

	pthread_mutex_init(&st->mutex, NULL);
	st->sock = ssgl_gameclient_connect_to_lobby(st->lobbyhost);
	if (st->sock < 0) {
		fprintf(stderr, "%s: %s: failed in %s: %s\n",
			"snis_server", "connect to lobby", __func__, strerror(errno));
		fprintf(stderr, "snis_server: server tracker thread exiting\n");
		return NULL;
	}

	strcpy(filter.game_type, "SNIS");
	do {
		rc = ssgl_recv_game_servers(st->sock, &game_server, &game_server_count, &filter);
		if (rc) {
			fprintf(stderr, "ssgl_recv_game_servers failed: %s\n", strerror(errno));
			break;
		}
		print_game_servers(game_server, game_server_count);
		if (game_server_count > 0)
			free(game_server);
		ssgl_sleep(20);  /* just a thread safe sleep. */

		(void) pthread_mutex_lock(&st->mutex);

		/* update internal list of game servers */
		if (st->game_server)
			free(st->game_server);
		st->game_server = malloc(sizeof(*st->game_server) * game_server_count);
		memcpy(st->game_server, game_server, sizeof(*st->game_server) * game_server_count);
		st->game_server_count = game_server_count;

		if (st->time_to_quit)
			time_to_quit = 1;

		pthread_mutex_unlock(&st->mutex);
	} while (!time_to_quit);
	fprintf(stderr, "snis_server: server tracker thread exiting\n");
	free(st->lobbyhost);
	free(st);
	return NULL;
}

struct server_tracker *server_tracker_start(char *lobbyhost)
{
	struct server_tracker *st;
	int rc;

	st = malloc(sizeof(*st));
	memset(st, 0, sizeof(*st));
	st->lobbyhost = strdup(lobbyhost);
	st->time_to_quit = 0;

	pthread_attr_init(&st->thread_attr);
	pthread_attr_setdetachstate(&st->thread_attr, PTHREAD_CREATE_DETACHED);
	rc = pthread_create(&st->thread, &st->thread_attr, server_tracker_thread, st);
	if (rc) {
		fprintf(stderr, "snis_server: failed to create server tracker theread: %d, %s, %s.\n",
			rc, strerror(rc), strerror(errno));
		free(st->lobbyhost);
		free(st);
		return NULL;
	}
	return st;
}

int server_tracker_get_server_list(struct server_tracker *st,
					struct ssgl_game_server **server_list, int *nservers)
{
	pthread_mutex_lock(&st->mutex);
	*server_list = malloc(sizeof(struct ssgl_game_server) * st->game_server_count);
	memcpy(*server_list, st->game_server,
			sizeof(struct ssgl_game_server) * st->game_server_count);
	*nservers = st->game_server_count;
	pthread_mutex_unlock(&st->mutex);
	return 0;
}

