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
#include <stdint.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <errno.h>

#include "ssgl.h"
#include "ssgl_socket_io.h"
#include "ssgl_sanitize.h"
#include "ssgl_connect_to_lobby.h"

struct lobby_thread_arg {
	struct ssgl_game_server gameserver;
	char lobbyhost[1000];
};

static void *update_lobby_thread(void *arg)
{
	int lobbysock, rc;
	struct lobby_thread_arg *a = (struct lobby_thread_arg *) arg;
	struct ssgl_game_server gameserver;
	char lobbyhost[1024];

	/* now move this to stack, and free malloc'ed memory. */
	memset(lobbyhost, 0, sizeof(lobbyhost));
	strncpy(lobbyhost, a->lobbyhost, 1023);
	gameserver = a->gameserver;

	free(arg);

	while (1) {
		lobbysock = ssgl_gameserver_connect_to_lobby(lobbyhost);
		if (lobbysock < 0) {
			fprintf(stderr, "ssgl_connect_to_lobby failed: %s\n", strerror(errno));
			pthread_exit(NULL);
		}

		printf("writing, gs.game_type = '%s'\n", gameserver.game_type);
		rc =  ssgl_writesocket(lobbysock, &gameserver, sizeof(gameserver)); 
		if (rc) {
			fprintf(stderr, "ssgl_register_gameserver failed: %s\n", strerror(errno));
			break;
		}
		shutdown(lobbysock, SHUT_RDWR);
		close(lobbysock);
		ssgl_sleep(SSGL_GAME_SERVER_TIMEOUT_SECS - 10);
	}
	pthread_exit(NULL);
}

int ssgl_register_gameserver(char *lobbyhost, struct ssgl_game_server *gameserver, pthread_t *lobby_thread)
{
	int rc;
	struct lobby_thread_arg *arg;

	/* Can't put this on the stack as the stack will be gone when thread needs it */
	arg = malloc(sizeof(*arg));
	arg->gameserver = *gameserver;
	strncpy(arg->lobbyhost, lobbyhost, sizeof(arg->lobbyhost)-1);

	rc = pthread_create(lobby_thread, NULL, update_lobby_thread, arg);
	return rc;
}

