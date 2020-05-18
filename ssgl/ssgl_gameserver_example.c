/*
 
Copyright (c) 2010 Stephen M. Cameron 

Permission is hereby granted, free of charge, to any person
obtaining a copy of this software and associated documentation
files (the "Software"), to deal in the Software without
restriction, including without limitation the rights to use,
copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the
Software is furnished to do so, subject to the following
conditions:

The above copyright notice and this permission notice shall be
included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
OTHER DEALINGS IN THE SOFTWARE.

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

#include "ssgl.h"
#include "ssgl_string.h"

static void usage()
{
	fprintf(stderr, "usage: ssgl_gameserver_example lobbyserver servernick gametype gameinst location\n");
	exit(1);
}

int nconnections = 0;

int main(int argc, char *argv[])
{
	struct ssgl_game_server gameserver;
	pthread_t lobby_thread;

	if (argc < 6)
		usage();

	/* Set up the game server structure that we will send to the lobby server. */
	memset(&gameserver, 0, sizeof(gameserver));
	gameserver.ipaddr = 0; /* lobby server will figure this out. */
	gameserver.port = htons(1234); /* whatever your game server's initial port is... */
#define COPYINARG(field, arg) strlcpy(gameserver.field, argv[arg], sizeof(gameserver.field))
	COPYINARG(server_nickname, 2);
	COPYINARG(game_type, 3);
	COPYINARG(game_instance, 4);
	COPYINARG(location, 5);
	strcpy(gameserver.protocol_version, "SNIS001");

	/* create a thread to contact and update the lobby server... */
	(void) ssgl_register_gameserver(argv[1], &gameserver, &lobby_thread, &nconnections);
	
	do {
		/* do whatever it is that your game server does here... */
		ssgl_sleep(5);
	} while (1);
	return 0;
}
