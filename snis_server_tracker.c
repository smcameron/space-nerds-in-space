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

#define _GNU_SOURCE
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
#include "pthread_util.h"

struct server_tracker {
	char *lobbyhost;
	pthread_t thread;
	int sock;
	pthread_mutex_t mutex;
	int time_to_quit;
	struct ssgl_game_server *game_server;
	int game_server_count;
	struct ssgl_game_server *mverse_server;
	int mverse_server_count;
	void *cookie;
	server_tracker_change_notifier notifier;
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

static int compare_game_server_list(struct ssgl_game_server *a, int acount,
				struct ssgl_game_server *b, int bcount)
{
	if (!a && !b)
		return 0;
	if ((!a && b) || (!b && a))
		return 1;
	if (acount != bcount)
		return 1;
	return memcmp(a, b, sizeof(*a) * acount);
}

static void copy_game_server_list(struct ssgl_game_server **output, int *outputcount,
				struct ssgl_game_server *input, int inputcount)
{
	/* fprintf(stderr, "copy_game_server_list 1\n"); */
	if (*output) {
		free(*output);
		*output = NULL;
		/* fprintf(stderr, "copy_game_server_list 2\n"); */
	}
	if (inputcount > 0) {
		*output = malloc(sizeof(**output) * inputcount);
		memcpy(*output, input, sizeof(**output) * inputcount);
		/* fprintf(stderr, "copy_game_server_list 3\n"); */
	}
	/* fprintf(stderr, "copy_game_server_list 4, count = %d\n", inputcount); */
	*outputcount = inputcount;
}

static void *server_tracker_thread(void *tracker_info)
{
	struct server_tracker *st = tracker_info;
	struct ssgl_game_server *game_server, *mverse_server;
	struct ssgl_client_filter server_filter;
	struct ssgl_client_filter multiverse_filter;
	int game_server_count, mverse_server_count;
	int rc, time_to_quit = 0;
	int changed;

	game_server = NULL;
	mverse_server = NULL;

	pthread_mutex_init(&st->mutex, NULL);
	st->sock = ssgl_gameclient_connect_to_lobby(st->lobbyhost);
	if (st->sock < 0) {
		fprintf(stderr, "%s: %s: failed in %s: %s\n",
			"snis_server", "connect to lobby", __func__, strerror(errno));
		fprintf(stderr, "snis_server: server tracker thread exiting\n");
		return NULL;
	}

	memset(&server_filter, 0, sizeof(server_filter));
	memset(&multiverse_filter, 0, sizeof(multiverse_filter));
	strcpy(server_filter.game_type, "SNIS");
	strcpy(multiverse_filter.game_type, "SNIS-MVERSE");
	do {
		rc = ssgl_recv_game_servers(st->sock, &game_server, &game_server_count, &server_filter);
		if (rc) {
			fprintf(stderr, "ssgl_recv_game_servers failed: %s\n", strerror(errno));
			break;
		}
		rc = ssgl_recv_game_servers(st->sock, &mverse_server,
						&mverse_server_count, &multiverse_filter);
		if (rc) {
			fprintf(stderr, "ssgl_recv_game_servers failed: %s\n", strerror(errno));
			break;
		}
		print_game_servers(game_server, game_server_count);
		print_game_servers(mverse_server, mverse_server_count);

		(void) pthread_mutex_lock(&st->mutex);

		/* update internal list of game servers */

		changed =
			compare_game_server_list(st->game_server, st->game_server_count,
							game_server, game_server_count) +
			compare_game_server_list(st->mverse_server, st->mverse_server_count,
							mverse_server, mverse_server_count);
		if (changed) {
			copy_game_server_list(&st->game_server, &st->game_server_count,
						game_server, game_server_count);
			copy_game_server_list(&st->mverse_server, &st->mverse_server_count,
						mverse_server, mverse_server_count);
		}
		if (game_server) {
			free(game_server);
			game_server = NULL;
		}
		if (mverse_server) {
			free(mverse_server);
			mverse_server = NULL;
		}
		if (st->time_to_quit)
			time_to_quit = 1;

		pthread_mutex_unlock(&st->mutex);
		if (changed && st->notifier) {
			/* fprintf(stderr, "server tracker calling callback zzz\n"); */
			st->notifier(st->cookie);
		} else if (changed) {
			/* fprintf(stderr, "server tracker noticed change, but not calling callback zzz\n"); */
		}

		if (mverse_server_count == 0 || game_server_count == 0)
			ssgl_sleep(1);  /* thread safe sleep. */
		else
			ssgl_sleep(5);  /* thread safe sleep. */

	} while (!time_to_quit);
	fprintf(stderr, "snis_server: server tracker thread exiting\n");
	free(st->lobbyhost);
	free(st);
	return NULL;
}

struct server_tracker *server_tracker_start(char *lobbyhost,
		server_tracker_change_notifier notifier, void *cookie)
{
	struct server_tracker *st;
	int rc;

	st = malloc(sizeof(*st));
	memset(st, 0, sizeof(*st));
	st->lobbyhost = strdup(lobbyhost);
	st->time_to_quit = 0;
	st->cookie = cookie;
	st->notifier = notifier;

	rc = create_thread(&st->thread, server_tracker_thread, st, "sniss-srvtrack", 1);
	if (rc) {
		fprintf(stderr, "snis_server: failed to create server tracker thread: %d, %s, %s.\n",
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
	copy_game_server_list(server_list, nservers, st->game_server, st->game_server_count);
	pthread_mutex_unlock(&st->mutex);
	return 0;
}

int server_tracker_get_multiverse_list(struct server_tracker *st,
					struct ssgl_game_server **server_list, int *nservers)
{
	pthread_mutex_lock(&st->mutex);
	copy_game_server_list(server_list, nservers, st->mverse_server, st->mverse_server_count);
	pthread_mutex_unlock(&st->mutex);
	return 0;
}

