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

/*****************************************************************************

This program is used to persist player ships across instances of snis_server.
Each instance of snis_server connects to snis_multiverse (which it locates by
way of ssgl_server) and periodically sends player ship information which is
persisted in a simple database by snis_multiverse.

*****************************************************************************/

#include "snis_multiverse.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <errno.h>
#include <pthread.h>
#include <assert.h>

#include "snis_marshal.h"
#include "quat.h"
#include "ssgl/ssgl.h"
#include "snis_log.h"
#include "snis_socket_io.h"
#include "snis.h"
#include "mathutils.h"

static pthread_mutex_t listener_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t listener_started;
static pthread_mutex_t service_mutex = PTHREAD_MUTEX_INITIALIZER;
int listener_port = -1;
static int default_snis_multiverse_port = -1; /* -1 means choose a random port */
pthread_t lobbythread;
char *lobbyserver = NULL;
static int snis_log_level = 2;
int nconnections = 0;
int nbridges = 0;

#define MAX_BRIDGES 1024
#define MAX_CONNECTIONS 100
#define MAX_STARSYSTEMS 100

static struct starsystem_info { /* one of these per connected snis_server */
	int socket;
	int refcount;
	pthread_t read_thread;
	pthread_t write_thread;
	pthread_attr_t read_attr, write_attr;
	struct packed_buffer_queue write_queue;
	pthread_mutex_t write_queue_mutex;
	char starsystem_name[20]; /* corresponds to "location" parameter of snis_server */
} starsystem[MAX_STARSYSTEMS] = { { 0 } };
static int nstarsystems = 0;

struct bridge_info {
	unsigned char key[32];
	int occupied;
	struct timeval created_on;
	struct timeval last_seen;
};

static void remove_starsystem(struct starsystem_info *ss)
{
}

/* assumes service_mutex held. */
static void put_starsystem(struct starsystem_info *ss)
{
	assert(ss->refcount > 0);
	ss->refcount--;
	if (ss->refcount == 0)
		remove_starsystem(ss);
}

/* assumes service_mutex held. */
static void get_starsystem(struct starsystem_info *ss)
{
	ss->refcount++;
}

static void lock_get_starsystem(struct starsystem_info *ss)
{
	pthread_mutex_lock(&service_mutex);
	get_starsystem(ss);
	pthread_mutex_unlock(&service_mutex);
}

static void lock_put_starsystem(struct starsystem_info *ss)
{
	pthread_mutex_lock(&service_mutex);
	put_starsystem(ss);
	pthread_mutex_unlock(&service_mutex);
}

static int __attribute((unused)) save_bridge_info(char *filename, struct bridge_info *b, int nitems)
{
	return 0;
}

static int __attribute((unused)) restore_bridge_info(char *filename, struct bridge_info **b, int *nitems)
{
	return 0;
}

static void usage()
{
	fprintf(stderr, "usage: snis_multiverse lobbyserver servernick location\n");
	exit(1);
}

static void get_peer_name(int connection, char *buffer)
{
	struct sockaddr_in *peer;
	struct sockaddr p;
	socklen_t addrlen = sizeof(p);
	int rc;

	peer = (struct sockaddr_in *) &p;

	/* This seems to get the wrong info... not sure what's going wrong */
	/* Get the game server's ip addr (don't trust what we were told.) */
	rc = getpeername(connection, (struct sockaddr * __restrict__) &p, &addrlen);
	if (rc != 0) {
		/* this happens quite a lot, so SSGL_INFO... */
		snis_log(SNIS_INFO, "getpeername failed: %s\n", strerror(errno));
		sprintf(buffer, "(UNKNOWN)");
		return;
	}
	sprintf(buffer, "%s:%hu", inet_ntoa(peer->sin_addr), ntohs(peer->sin_port));
	printf("put '%s' in buffer\n", buffer);
}

static void log_client_info(int level, int connection, char *info)
{
	char client_ip[50];

	if (level < snis_log_level)
		return;

	get_peer_name(connection, client_ip);
	snis_log(level, "snis_multiverse: connection from %s: %s",
			client_ip, info);
}

static int verify_client_protocol(int connection)
{
	int rc;
	char protocol_version[sizeof(SNIS_MULTIVERSE_VERSION) + 1];
	const int len = sizeof(SNIS_MULTIVERSE_VERSION) - 1;

	rc = snis_writesocket(connection, SNIS_MULTIVERSE_VERSION, len);
	if (rc < 0)
		return -1;
	memset(protocol_version, 0, sizeof(protocol_version));
	rc = snis_readsocket(connection, protocol_version, len);
	if (rc < 0)
		return rc;
	protocol_version[len] = '\0';
	snis_log(SNIS_INFO, "snis_multiverse: protocol read...'%s'\n", protocol_version);
	if (strcmp(protocol_version, SNIS_MULTIVERSE_VERSION) != 0)
		return -1;
	snis_log(SNIS_INFO, "snis_multiverse: protocol verified.\n");
	return 0;
}

static void process_instructions_from_snis_server(struct starsystem_info *ss)
{
}

static void *starsystem_read_thread(void /* struct starsystem_info */ *starsystem)
{
	struct starsystem_info *ss = (struct starsystem_info *) starsystem;

	lock_get_starsystem(ss);
	while (1) {
		process_instructions_from_snis_server(ss);
		if (ss->socket < 0)
			break;
	}
	lock_put_starsystem(ss);
	return NULL;
}

static void write_queued_updates_to_snis_server(struct starsystem_info *ss)
{
	int rc;
	uint8_t noop = SNISMV_OPCODE_NOOP;

	struct packed_buffer *buffer;

	buffer = packed_buffer_queue_combine(&ss->write_queue, &ss->write_queue_mutex);
	if (buffer) {
		rc = snis_writesocket(ss->socket, buffer->buffer, buffer->buffer_size);
		packed_buffer_free(buffer);
		if (rc != 0) {
			snis_log(SNIS_ERROR, "snis_multiverse: writesocket failed, rc= %d, errno = %d(%s)\n",
				rc, errno, strerror(errno));
			goto badclient;
		}
	} else {
		/* Nothing to write, send a no-op just so we know if client is still there */
		rc = snis_writesocket(ss->socket, &noop, sizeof(noop));
		if (rc != 0) {
			snis_log(SNIS_ERROR, "snis_multiverse: (noop) writesocket failed, rc= %d, errno = %d(%s)\n",
				rc, errno, strerror(errno));
			goto badclient;
		}
	}
	return;

badclient:
	shutdown(ss->socket, SHUT_RDWR);
	close(ss->socket);
	ss->socket = -1;
}

static void *starsystem_write_thread(void /* struct starsystem_info */ *starsystem)
{
	struct starsystem_info *ss = (struct starsystem_info *) starsystem;

	lock_get_starsystem(ss);
	while (1) {
		if (ss->socket < 0)
			break;
		write_queued_updates_to_snis_server(ss);
		sleep(5);
	}
	lock_put_starsystem(ss);
	return NULL;
}

static void add_new_starsystem(struct starsystem_info *ss)
{
}

/* Creates a thread for each incoming connection... */
static void service_connection(int connection)
{
	int i, rc, flag = 1;
	int thread_count, iterations;

	log_client_info(SNIS_INFO, connection, "snis_multiverse: servicing connection\n");
	/* get connection moved off the stack so that when the thread needs it,
	 * it's actually still around.
	 */

	i = setsockopt(connection, IPPROTO_TCP, TCP_NODELAY, (char *) &flag, sizeof(int));
	if (i < 0)
		snis_log(SNIS_ERROR, "snis_multiverse: setsockopt failed: %s.\n", strerror(errno));

	if (verify_client_protocol(connection)) {
		log_client_info(SNIS_ERROR, connection, "snis_multiverse: disconnected, protocol violation\n");
		close(connection);
		return;
	}

	pthread_mutex_lock(&service_mutex);

	/* Set i to a free slot in starsystem[], or to nstarsystems if none free */
	for (i = 0; i < nstarsystems; i++)
		if (starsystem[i].refcount == 0)
			break;
	if (i == nstarsystems)
		nstarsystems++;

	starsystem[i].socket = connection;

	log_client_info(SNIS_INFO, connection, "snis_multiverse: snis_server connected\n");

	pthread_mutex_init(&starsystem[i].write_queue_mutex, NULL);
	packed_buffer_queue_init(&starsystem[i].write_queue);

	add_new_starsystem(&starsystem[i]);

	pthread_attr_init(&starsystem[i].read_attr);
	pthread_attr_setdetachstate(&starsystem[i].read_attr, PTHREAD_CREATE_DETACHED);
	pthread_attr_init(&starsystem[i].write_attr);
	pthread_attr_setdetachstate(&starsystem[i].write_attr, PTHREAD_CREATE_DETACHED);

	/* initialize refcount to 1 to keep starsystem[i] from getting reaped. */
	starsystem[i].refcount = 1;

	/* create threads... */
	rc = pthread_create(&starsystem[i].read_thread,
		&starsystem[i].read_attr, starsystem_read_thread, (void *) &starsystem[i]);
	if (rc) {
		snis_log(SNIS_ERROR, "per client read thread, pthread_create failed: %d %s %s\n",
			rc, strerror(rc), strerror(errno));
	}
	rc = pthread_create(&starsystem[i].write_thread,
		&starsystem[i].write_attr, starsystem_write_thread, (void *) &starsystem[i]);
	if (rc) {
		snis_log(SNIS_ERROR, "per client write thread, pthread_create failed: %d %s %s\n",
			rc, strerror(rc), strerror(errno));
	}
	pthread_mutex_unlock(&service_mutex);

	/* Wait for at least one of the threads to prevent premature reaping */
	iterations = 0;
	thread_count = 0;
	do {
		pthread_mutex_lock(&service_mutex);
		thread_count = starsystem[i].refcount;
		pthread_mutex_unlock(&service_mutex);
		if (thread_count < 2)
			usleep(1000);
		iterations++;
		if (iterations > 1000 && (iterations % 1000) == 0)
			printf("Way too many iterations at %s:%d\n", __FILE__, __LINE__);
	} while (thread_count < 2);

	/* release this thread's reference */
	lock_put_starsystem(&starsystem[i]);

	snis_log(SNIS_INFO, "snis_multiverse: bottom of 'service connection'\n");
}

/* This thread listens for incoming client connections, and
 * on establishing a connection, starts a thread for that
 * connection.
 */
static void *listener_thread(__attribute__((unused)) void *unused)
{
	int rendezvous, connection, rc;
	struct sockaddr_in remote_addr;
	socklen_t remote_addr_len;
	uint16_t port;
	struct addrinfo hints;
	struct addrinfo *result, *rp;
	int s;
	char portstr[20];
	char *snis_multiverse_port_var;

	snis_log(SNIS_INFO, "snis_multiverse: snis_multiverse starting\n");
	snis_multiverse_port_var = getenv("SNISMULTIVERSEPORT");
	default_snis_multiverse_port = -1;
	if (snis_multiverse_port_var != NULL) {
		int rc, value;
		rc = sscanf(snis_multiverse_port_var, "%d", &value);
		if (rc == 1) {
			default_snis_multiverse_port = value & 0x0ffff;
			printf("snis_multiverse: Using SNISMULTIVERSEPORT value %d\n",
				default_snis_multiverse_port);
		}
	}

	/* Bind "rendezvous" socket to a random port to listen for connections.
	 * unless SNISSERVERPORT is defined, in which case, try to use that.
	 */
	while (1) {

		/*
		 * choose a random port in the "Dynamic and/or Private" range
		 * see http://www.iana.org/assignments/port-numbers
		 */
		if (default_snis_multiverse_port == -1)
			port = snis_randn(65335 - 49152) + 49151;
		else
			port = default_snis_multiverse_port;
		snis_log(SNIS_INFO, "snis_multiverse: Trying port %d\n", port);
		sprintf(portstr, "%d", port);
		memset(&hints, 0, sizeof(struct addrinfo));
		hints.ai_family = AF_UNSPEC;    /* Allow IPv4 or IPv6 */
		hints.ai_socktype = SOCK_STREAM;
		hints.ai_flags = AI_PASSIVE | AI_NUMERICSERV;    /* For wildcard IP address */
		hints.ai_protocol = 0;          /* Any protocol */
		hints.ai_canonname = NULL;
		hints.ai_addr = NULL;
		hints.ai_next = NULL;

		s = getaddrinfo(NULL, portstr, &hints, &result);
		if (s != 0) {
			snis_log(SNIS_ERROR, "snis_multiverse: snis_multiverse: getaddrinfo: %s\n", gai_strerror(s));
			exit(EXIT_FAILURE);
		}

		/* getaddrinfo() returns a list of address structures.
		 * Try each address until we successfully bind(2).
		 * If socket(2) (or bind(2)) fails, we (close the socket
		 * and) try the next address.
		 */

		for (rp = result; rp != NULL; rp = rp->ai_next) {
			rendezvous = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
			if (rendezvous == -1)
				continue;

			if (bind(rendezvous, rp->ai_addr, rp->ai_addrlen) == 0)
				break;                  /* Success */
			close(rendezvous);
		}
		if (rp != NULL)
			break;
	}

	/* At this point, "rendezvous" is bound to a random port */
	snis_log(SNIS_INFO, "snis_multiverse listening for connections on port %d\n", port);
	listener_port = port;

	/* Listen for incoming connections... */
	rc = listen(rendezvous, SOMAXCONN);
	if (rc < 0) {
		snis_log(SNIS_ERROR, "snis_multiverse: listen() failed, exiting: %s\n", strerror(errno));
		exit(1);
	}

	/* Notify other threads that the listener thread is ready... */
	(void) pthread_mutex_lock(&listener_mutex);
	pthread_cond_signal(&listener_started);
	(void) pthread_mutex_unlock(&listener_mutex);

	while (1) {
		remote_addr_len = sizeof(remote_addr);
		snis_log(SNIS_INFO, "snis_multiverse: Accepting connection...\n");
		connection = accept(rendezvous, (struct sockaddr *) &remote_addr, &remote_addr_len);
		snis_log(SNIS_INFO, "snis_multiverse: accept returned %d\n", connection);
		if (connection < 0) {
			/* handle failed connection... */
			snis_log(SNIS_WARN, "snis_multiverse: accept() failed: %s\n", strerror(errno));
			ssgl_sleep(1);
			continue;
		}
		if (remote_addr_len != sizeof(remote_addr)) {
			snis_log(SNIS_ERROR, "snis_multiverse: strange socket address length %d\n", remote_addr_len);
			/* shutdown(connection, SHUT_RDWR);
			close(connection);
			continue; */
		}
		service_connection(connection);
	}
}

/* Starts listener thread to listen for incoming client connections.
 * Returns port on which listener thread is listening.
 */
static int start_listener_thread(void)
{
	pthread_attr_t attr;
	pthread_t thread;
	int rc;

	/* Setup to wait for the listener thread to become ready... */
	pthread_cond_init(&listener_started, NULL);
	(void) pthread_mutex_lock(&listener_mutex);

	/* Create the listener thread... */
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
	rc = pthread_create(&thread, &attr, listener_thread, NULL);
	if (rc) {
		snis_log(SNIS_ERROR, "snis_multiverse: Failed to create listener thread, pthread_create: %d %s %s\n",
				rc, strerror(rc), strerror(errno));
	}

	/* Wait for the listener thread to become ready... */
	pthread_cond_wait(&listener_started, &listener_mutex);
	(void) pthread_mutex_unlock(&listener_mutex);
	snis_log(SNIS_INFO, "snis_multiverse: Listener started.\n");
	return listener_port;
}

static void restore_data(void)
{
}

static void checkpoint_data(void)
{
}

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
	int rc;
	pthread_t lobby_thread;

	open_log_file();
	restore_data();

	if (argc < 4)
		usage();

	printf("SNIS multiverse server\n");

	snis_protocol_debugging(1);
	ignore_sigpipe();
	rc = start_listener_thread();
	if (rc < 0) {
		fprintf(stderr, "snis_multiverse: start_listener_thread() failed.\n");
		return -1;
	}
	/* Set up the game server structure that we will send to the lobby server. */
	memset(&gameserver, 0, sizeof(gameserver));
	if (ssgl_get_primary_host_ip_addr(&gameserver.ipaddr) != 0)
		snis_log(SNIS_WARN, "snis_multiverse: Failed to get local ip address.\n");
	gameserver.port = htons(listener_port);
#define COPYINARG(field, arg) strncpy(gameserver.field, argv[arg], sizeof(gameserver.field) - 1)
	COPYINARG(server_nickname, 2);
	strcpy(gameserver.game_type, "SNIS-MVERSE");
	strcpy(gameserver.game_instance, "-");
	COPYINARG(location, 3);

	/* create a thread to contact and update the lobby server... */
	(void) ssgl_register_gameserver(argv[1], &gameserver, &lobby_thread, &nconnections);

	do {
		/* do whatever it is that your game server does here... */
		ssgl_sleep(30);
		checkpoint_data();
	} while (1);
	return 0;
}

