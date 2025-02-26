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
#ifndef __SSGL_H__
#define __SSGL_H__

#include <pthread.h>
#include <stdint.h>

#ifdef __APPLE__
#define pthread_setname_np(a, b)
#endif

#define GAMELOBBY_SERVICE_NUMBER 2914
#define GAMELOBBY_SERVICE_NAME "gamelobby"
#define GAMELOBBY_SERVICE_NUMBER_AS_STRING "2914"

#pragma pack(1)
#define SSGL_LOCATIONSIZE (20)
struct ssgl_game_server {
	/* ipaddr and port are both stored in network byte order here, and they are the addresses
	 * that the client has told us.  They might differ from the real_ipaddr if they connected
	 * from localhost (real_ipaddr will be 127.0.0.1) or if they are behind NAT.
	 */
	uint32_t ipaddr;
	uint32_t real_ipaddr;
	uint16_t port;
	char game_type[15];		/* What kind of game is this? */
	char game_instance[20];		/* which instance on the server of the game */
	char server_nickname[15];	/* server nickname where game is hosted */
	char protocol_version[9];
	char location[SSGL_LOCATIONSIZE];
	uint32_t nconnections;
};

struct ssgl_client_filter {
	char game_type[15];
};

struct ssgl_lobby_descriptor {
	uint32_t ipaddr;
	uint16_t port;
	char hostname[32];
};
#pragma pack()

#define SSGL_GAME_SERVER_TIMEOUT_SECS (20)

#ifdef __SSGL_DEFINE_GLOBALS__
#define GLOBAL
#else
#define GLOBAL extern
#endif

GLOBAL int ssgl_gameclient_connect_to_lobby(char *hostname); /* uses $SSGL_PORT or gamelobby port */
/* If 0 < port <= 65535, port will be used, else $SSGL_PORT, else default gamelobby port */
GLOBAL int ssgl_gameclient_connect_to_lobby_port(char *hostname, int port);
GLOBAL int ssgl_register_gameserver(char *lobbyhost, struct ssgl_game_server *gameserver,
	pthread_t *lobby_thread, int *nconnections);
GLOBAL int ssgl_recv_game_servers(int sock,
	struct ssgl_game_server **server_list, int *server_count,
	struct ssgl_client_filter *filter);

GLOBAL void ssgl_sleep(int seconds); /* just a thread safe sleep implemented by nanosleep w/ retries */
GLOBAL void ssgl_msleep(int milliseconds); /* just a thread safe sleep implemented by nanosleep w/ retries */
GLOBAL int ssgl_get_primary_host_ip_addr(uint32_t *ipaddr);
GLOBAL int ssgl_get_lobby_list(struct ssgl_lobby_descriptor[], int *maxlobbies);

#define SSGL_INFO 1
#define SSGL_WARN 2
#define SSGL_ERROR 3

extern int ssgl_open_logfile(char *logfilename);
extern void ssgl_log(int level, const char* format, ...);
extern void ssgl_close_logfile(void);
extern void ssgl_set_log_level(int level);
extern int ssgl_register_for_bcast_packets(void (*notify_fn)(struct ssgl_lobby_descriptor *, int));
extern unsigned short ssgl_get_gamelobby_port(char *proto);

#undef GLOBAL
#endif

