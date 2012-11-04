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
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netdb.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <pthread.h>

#include "ssgl.h"
#include "ssgl_sanitize.h"
#include "ssgl_socket_io.h"
#include "ssgl_protocol_id.h"

/* This is our directory of game servers, this is the data that we serve... */
#define MAX_GAME_SERVERS 5000
static int ngame_servers;
static struct ssgl_game_server game_server[MAX_GAME_SERVERS];
static time_t expiration[MAX_GAME_SERVERS]; 

/* lock to protect ngame_servers, ssgl_game_server[] and expiration[] */ 
static pthread_mutex_t ssgl_mutex = PTHREAD_MUTEX_INITIALIZER;

static inline void ssgl_lock()
{
	(void) pthread_mutex_lock(&ssgl_mutex);
}

static inline void ssgl_unlock()
{
	(void) pthread_mutex_unlock(&ssgl_mutex);
}

static int get_protocol_version(int connection, struct ssgl_protocol_id *proto_id)
{
	int protocol_version;
	int rc;

	/* get protocol version */	
	rc = ssgl_readsocket(connection, proto_id, sizeof(*proto_id));
	if (rc != 0)
		return rc;
	if (memcmp(proto_id->signature, SSGL_SIGNATURE_STRING, sizeof(proto_id->signature)) != 0)
		goto bad_protocol;
	protocol_version = ntohl(proto_id->protocol_version);
	if (protocol_version != SSGL_PROTOCOL_VERSION)
		goto bad_protocol;
	return 0;

bad_protocol:
	shutdown(connection, SHUT_RDWR);
	close(connection);
	return -1;
}

static void fill_trailing_zeroes(char *buf, int bufsize)
{
	int len;

	buf[bufsize - 1] = '\0';
	len = strlen(buf);
	if (bufsize - len > 0)
		memset(&buf[len], 0, bufsize - len);
}

static int sanitize_game_server_entry(struct ssgl_game_server *gs)
{
	/* If any bytes of the IP address are 255, reject.
	 * Do not want to get tricked into broadcasting.
	 */
	unsigned char *x = (unsigned char *) &gs->ipaddr;

	if (x[0] == 0xff || x[1] == 0xff ||
		x[2] == 0xff || x[3] == 0xff)
		return -1;

	/* Make sure all the strings are null terminated and that the 
	 * characters following the terminating NULL are also zeroed.
	 */

	fill_trailing_zeroes(gs->game_type, sizeof(gs->game_type));
	fill_trailing_zeroes(gs->game_instance, sizeof(gs->game_instance));
	fill_trailing_zeroes(gs->server_nickname, sizeof(gs->server_nickname));
	fill_trailing_zeroes(gs->location, sizeof(gs->location));
	
	return 0;	
}

static void update_expiration(int entry)
{
	int rc;
	struct timeval tv;

	rc = gettimeofday(&tv, NULL);
	if (rc) /* gettimeofday will not fail except for a bug in my program */
		abort();
	expiration[entry] = tv.tv_sec + SSGL_GAME_SERVER_TIMEOUT_SECS;
}

static void service_game_server(int connection)
{
	int rc;
	int i;
	struct ssgl_game_server gs;
#if 0
	struct sockaddr peer;
	struct sockaddr_in *ip4addr;
	unsigned int addrlen;
	unsigned char *x;
#endif

	/* Get game server information */
	rc = ssgl_readsocket(connection, &gs, sizeof(gs));
	if (rc < 0)
		return;

	/* printf("1 gs.game_type = '%s', gs.port = %hu\n",gs.game_type, gs.port); */
	if (sanitize_game_server_entry(&gs))
		return;
	/* printf("2 gs.game_type = '%s'\n",gs.game_type); */

#if 0

	/* This doesn't work... will return 127.0.0.1 if gameserver is on
	 * the same host as lobby server.
	 */

	/* Get the game server's ip addr (don't trust what we were told.) */
	memset(&gs.ipaddr, 0, sizeof(gs.ipaddr));
	addrlen = sizeof(peer);
	rc = getpeername(connection, &peer, &addrlen); 
	if (rc != 0) {
		printf("getpeername failed: %s\n", strerror(errno));
	}
	printf("addrlen = %d\n", addrlen);
	x = (unsigned char *) &peer;
	for (i = 0; i < addrlen; i++)
		printf("%02x ", x[i]);
	printf("\n");
	ip4addr = (struct sockaddr_in *) &peer;
	memcpy(&gs.ipaddr, &ip4addr->sin_addr, sizeof(gs.ipaddr));
#endif

	/* printf("3 gs.game_type = '%s'\n",gs.game_type); */
	/* Update directory with new info. */

	ssgl_lock();

	/* replace with faster algorithm if need be. */
	for (i = 0; i < ngame_servers; i++) {
		/* already present? */
		if (memcmp(&game_server[i], &gs, sizeof(gs)) == 0) {
			update_expiration(i);
			goto out;
		}
	}
	/* printf("4 gs.game_type = '%s'\n",gs.game_type); */

	if (ngame_servers >= MAX_GAME_SERVERS)
		goto out; /* no room at the inn. */

	/* printf("5 gs.game_type = '%s'\n",gs.game_type); */
	/* add the new game server info into the directory... */
	game_server[ngame_servers] = gs;
	update_expiration(ngame_servers);
	/* printf("6 gs.game_type = '%s'\n",gs.game_type); */
	ngame_servers++;
out:
	ssgl_unlock();
	return;
}

static void *expire_game_servers(__attribute__((unused)) void *arg)
{
	int i, j;
	struct timeval tv;

	printf("ssgl_server: game server expiration thread started.\n");
	while (1) { /* TODO, replace this with some condition... */
		(void) gettimeofday(&tv, NULL);
		ssgl_lock();
		i = 0;
		while (i < ngame_servers) {
			if (expiration[i] < tv.tv_sec) {
				/* game server has expired... remove it from the list. */
				for (j = i + 1; j < ngame_servers; j++) {
					game_server[j - 1] = game_server[j];
					expiration[j - 1] = expiration[j];
				}
				ngame_servers--;
			} else
				i++;
		}
		ssgl_unlock();
		ssgl_sleep(60); /* run once a minute. */
	}
	return 0; /* implicit pthread_exit() here. */
}

static void service_game_client(int connection)
{
	/* Send the game server directory to the client. */
	struct ssgl_game_server *directory;
	struct ssgl_client_filter filter;
	int nentries, i, rc, be_nentries;

	printf("ssgl_server: serving client...\n");


	while (1) {
		/* printf("ssgl_server: reading filter data.\n"); */
		rc = ssgl_readsocket(connection, &filter, sizeof(filter));
		if (rc)
			goto badclient;

		fill_trailing_zeroes(filter.game_type, sizeof(filter.game_type));
		/* printf("ssgl_server: client requested filter '%s'\n", filter.game_type); */

		nentries = 0;
		ssgl_lock();
		if (ngame_servers == 0)
			goto send_the_data;

		if (strncmp(filter.game_type, "*", 1) == 0) {
			/* they want the firehose... */
			directory = malloc(sizeof(*directory) * ngame_servers);
			memcpy(directory, game_server, sizeof(*directory) * ngame_servers);
			nentries = ngame_servers;
		} else {
			/* they want only entries of matching game type... */
			directory = malloc(sizeof(*directory) * ngame_servers);
			memset(directory, 0, sizeof(*directory) * ngame_servers);
			for (i = 0; i < ngame_servers; i++) {
				/* printf("%d '%s' vs. '%s'\n", i, game_server[i].game_type, filter.game_type); */
				if (strcmp(game_server[i].game_type, filter.game_type) == 0) {
					directory[nentries] = game_server[i];
					nentries++;
				}
			}
		}
send_the_data:
		ssgl_unlock();

		/* printf("ssgl_server: sending client count: %d/%d\n", nentries, ngame_servers); */
		be_nentries = htonl(nentries);
		rc = ssgl_writesocket(connection, &be_nentries, sizeof(be_nentries));
		if (rc)
			goto badclient;
		/* printf("ssgl_server: sending client %d entries\n", nentries); */

		if (nentries > 0) { 
			/* TODO insert zlib compression here, if need be. */
			rc = ssgl_writesocket(connection, directory, sizeof(*directory) * nentries);
			if (rc)
				goto badclient;
		}

		/* printf("ssgl_server: sent entries.\n"); */
		/* sleep at least 5 secs before updating client again.
		 * client can increase this by sleeping more before
		 * writing a new filter back to complete the readsocket
		 * at the top of this while loop.
		 */
		ssgl_sleep(5);
	}

badclient:
	printf("ssgl_server: bailing on client.\n");
	free(directory);
	shutdown(connection, SHUT_RDWR);
	close(connection);
	return;
}

static void * service_thread(void * arg)
{
	int rc;
	struct ssgl_protocol_id proto_id;
	int *connection = arg;

	/* Get the SSGL protocol version number from connection
	 * and make sure it's a version we understand.
	 */

	rc = get_protocol_version(*connection, &proto_id);
	if (rc < 0)
		goto out;	

	/* Determine if this is a game client or a game server connecting */
	if (proto_id.client_type == SSGL_GAME_SERVER)
		service_game_server(*connection);
	else
		service_game_client(*connection);
out:
	shutdown(*connection, SHUT_RDWR);
	close(*connection);
	free(connection); /* allocated prior to thread creation in service(), below. */
	return 0; /* implicit pthread_exit(); here */
}

static void service(int connection)
{
	pthread_attr_t attr;
	pthread_t thread;
	int *conn;

	printf("ssgl_server: servicing connection %d\n", connection);
	/* get connection moved off the stack so that when the thread needs it,
	 * it's actually still around. 
	 */
	conn = malloc(sizeof(*conn)); /* will be freed in service_thread(). */
	*conn = connection; /* linux overcommits, no sense in checking malloc return. */

	pthread_attr_init(&attr);
	(void) pthread_create(&thread, &attr, service_thread, (void *) conn);
}

static void start_game_server_expiration_thread(void)
{
	pthread_attr_t attr;
	pthread_t thread;
	int rc;

	pthread_attr_init(&attr);
	rc = pthread_create(&thread, &attr, expire_game_servers, NULL); 
	if (rc < 0) {
		fprintf(stderr, "Unable to create game server expiration thread: %s.\n",
			strerror(errno));
	}
}

int main(int argc, char *argv[])
{
	int rendezvous, connection, rc;
	struct servent *gamelobby_service;
	struct protoent *gamelobby_proto;
	struct sockaddr_in listen_address;
	struct sockaddr_in remote_addr;
	socklen_t remote_addr_len;

	memset(game_server, 0, sizeof(game_server));
	memset(expiration, 0, sizeof(expiration));
	ngame_servers = 0;

	printf("ssgl_server starting\n");
	start_game_server_expiration_thread();

	/* Get the "gamelobby" service protocol/port */
	gamelobby_service = getservbyname(GAMELOBBY_SERVICE_NAME, "tcp");
	if (!gamelobby_service) {
		fprintf(stderr, "getservbyname failed, %s\n", strerror(errno));
		fprintf(stderr, "Check that /etc/services contains the following lines:\n");
		fprintf(stderr, "gamelobby	2419/tcp\n");
		fprintf(stderr, "gamelobby	2419/udp\n");
		exit(1);
	}

	/* Get the protocol number... */
	if (gamelobby_service) {
		gamelobby_proto = getprotobyname(gamelobby_service->s_proto);
		if (!gamelobby_proto) {
			fprintf(stderr, "getprotobyname(%s) failed: %s\n", 
				gamelobby_service->s_proto, strerror(errno));
		}
	} else {
		gamelobby_proto = NULL;
	}

	/* Make ourselves a socket */
	rendezvous = socket(AF_INET, SOCK_STREAM, gamelobby_proto ? gamelobby_proto->p_proto : 0);

	/* Bind the socket to "any address" on our port */
	listen_address.sin_family = AF_INET;
	listen_address.sin_addr.s_addr = INADDR_ANY;
	listen_address.sin_port = gamelobby_service ? gamelobby_service->s_port : GAMELOBBY_SERVICE_NUMBER;
	rc = bind(rendezvous, (struct sockaddr *) &listen_address, sizeof(listen_address));
	if (rc < 0) {
		fprintf(stderr, "bind() failed: %s\n", strerror(errno));
		exit(1);
	}

	printf("ssgl_server: listening for connections...\n");
	/* Listen for incoming connections... */
	rc = listen(rendezvous, SOMAXCONN);
	if (rc < 0) {
		fprintf(stderr, "listen() failed: %s\n", strerror(errno));
		exit(1);
	}

	while (1) {
		remote_addr_len = sizeof(remote_addr);
		/* printf("Accepting connection...\n"); */
		connection = accept(rendezvous, (struct sockaddr *) &remote_addr, &remote_addr_len);
		/* printf("accept returned %d\n", connection); */
		if (connection < 0) {
			/* handle failed connection... */
			fprintf(stderr, "accept() failed: %s\n", strerror(errno));
			ssgl_sleep(1);
			continue;
		}
		if (remote_addr_len != sizeof(remote_addr)) {
			fprintf(stderr, "strange socket address length %d\n", remote_addr_len);
			shutdown(connection, SHUT_RDWR);
			close(connection);
			continue;
		}
		service(connection);
	}
	return 0;
}

