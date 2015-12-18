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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <errno.h>

#include "ssgl/ssgl.h"
#include "snis_log.h"

static int snis_log_level = 2;

static void usage()
{
	fprintf(stderr, "usage: ssgl_gameserver_example lobbyserver servernick gametype gameinst location\n");
	exit(1);
}

int nconnections = 0;

static void open_log_file(void)
{
	char *loglevelstring;
	int ll, rc;

	rc = snis_open_logfile("SNIS_SERVER_LOGFILE");
	loglevelstring = getenv("SNIS_LOG_LEVEL");
	if (rc == 0 && loglevelstring && sscanf(loglevelstring, "%d", &ll) == 1) {
		snis_log_level = ll;
		snis_set_log_level(snis_log_level);
	}
}

int main(int argc, char *argv[])
{
	struct ssgl_game_server gameserver;
	pthread_t lobby_thread;

	open_log_file();
	if (argc < 6)
		usage();

	printf("SNIS multiverse server\n");
	/* Set up the game server structure that we will send to the lobby server. */
	memset(&gameserver, 0, sizeof(gameserver));
	gameserver.ipaddr = 0; /* lobby server will figure this out. */
	gameserver.port = htons(1234); /* whatever your game server's initial port is... */
#define COPYINARG(field, arg) strncpy(gameserver.field, argv[arg], sizeof(gameserver.field) - 1)
	COPYINARG(server_nickname, 2);
	COPYINARG(game_type, 3);
	COPYINARG(game_instance, 4);
	COPYINARG(location, 5);

	/* create a thread to contact and update the lobby server... */
	(void) ssgl_register_gameserver(argv[1], &gameserver, &lobby_thread, &nconnections);

	do {
		/* do whatever it is that your game server does here... */
		ssgl_sleep(5);
	} while (1);
	return 0;
}

