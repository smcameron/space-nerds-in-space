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
#define _GNU_SOURCE
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
#include <netinet/in.h>
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <signal.h>

#include "ssgl.h"
#include "ssgl_sanitize.h"
#include "ssgl_socket_io.h"
#include "ssgl_protocol_id.h"
#include "ssgl_log.h"
#include "ssgl_get_gamelobby_port.h"

/* This is our directory of game servers, this is the data that we serve... */
#define MAX_GAME_SERVERS 5000
static int ngame_servers;
static struct ssgl_game_server game_server[MAX_GAME_SERVERS];
static time_t expiration[MAX_GAME_SERVERS]; 

/* lock to protect ngame_servers, ssgl_game_server[] and expiration[] */ 
static pthread_mutex_t ssgl_mutex = PTHREAD_MUTEX_INITIALIZER;

static void ssgl_exit(char *reason, int code)
{
	ssgl_log(SSGL_WARN, "ssgl_server exiting, reason: %s, exit code = %d\n",
			reason, code);
	exit(code);
}

static void get_peer_name(int connection, char *buffer)
{
	struct sockaddr_in peer;
	socklen_t addrlen;
	int rc;

	memset(&peer, 0, sizeof(peer));
	/* Get the game server's ip addr (don't trust what we were told.) */
	rc = getpeername(connection, (struct sockaddr * __restrict__) &peer, &addrlen); 
	if (rc != 0) {
		/* this happens quite a lot, so SSGL_INFO... */
		ssgl_log(SSGL_INFO, "getpeername failed: %s\n", strerror(errno));
		sprintf(buffer, "(UNKNOWN)");
		return;
	}
	sprintf(buffer, "%s:%hu", inet_ntoa(peer.sin_addr), ntohs(peer.sin_port));
}

static void log_disconnect(int level, int connection, char *reason)
{
	char client_ip[50];

	get_peer_name(connection, client_ip);
	ssgl_log(level, "ssgl_server: Disconnecting from %s, reason: %s\n",
			client_ip, reason);
}

static inline void ssgl_lock(void)
{
	(void) pthread_mutex_lock(&ssgl_mutex);
}

static inline void ssgl_unlock(void)
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
	log_disconnect(SSGL_WARN, connection, "bad protocol identifier");
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
	fill_trailing_zeroes(gs->protocol_version, sizeof(gs->protocol_version));
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

struct service_thread_info {
	int connection;
	pthread_t thread;
	struct sockaddr_in remote_addr;
	socklen_t remote_addr_len;
};

static void service_game_server(int connection, struct service_thread_info *threadinfo)
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
	if (rc < 0) {
		ssgl_log(SSGL_WARN, "Failed initial socket read from game server\n");
		return;
	}

	/* printf("1 gs.game_type = '%s', gs.port = %hu\n",gs.game_type, gs.port); */
	if (sanitize_game_server_entry(&gs)) { 
		ssgl_log(SSGL_WARN, "Failed to sanitize game server information\n");
		return;
	}
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
		struct ssgl_game_server *ogs = &game_server[i];
		/* already present? */
		if (ogs->port == gs.port &&
			ogs->ipaddr == gs.ipaddr &&
			memcmp(ogs->game_type, gs.game_type, sizeof(gs.game_type)) == 0 &&
			memcmp(ogs->game_instance, gs.game_instance, sizeof(gs.game_instance)) == 0 &&
			memcmp(ogs->server_nickname, gs.server_nickname, sizeof(gs.server_nickname)) == 0 &&
			memcmp(ogs->protocol_version, gs.protocol_version, sizeof(gs.protocol_version)) == 0 &&
			memcmp(ogs->location, gs.location, sizeof(gs.location)) == 0) {
			ogs->nconnections = gs.nconnections;
			update_expiration(i);
			goto out;
		}
	}
	/* printf("4 gs.game_type = '%s'\n",gs.game_type); */

	if (ngame_servers >= MAX_GAME_SERVERS) {
		ssgl_log(SSGL_WARN, "Too many game servers connected, rejecting game server.\n");
		goto out; /* no room at the inn. */
	}

	/* printf("5 gs.game_type = '%s'\n",gs.game_type); */
	/* add the new game server info into the directory... */
	memcpy(&gs.real_ipaddr, &threadinfo->remote_addr.sin_addr, sizeof(gs.real_ipaddr));
	game_server[ngame_servers] = gs;
	update_expiration(ngame_servers);
	/* printf("6 gs.game_type = '%s'\n",gs.game_type); */
	ngame_servers++;
out:
	ssgl_unlock();
	return;
}

static pthread_t expiry_thread;

static void *expire_game_servers(__attribute__((unused)) void *arg)
{
	int i, j;
	struct timeval tv;
#ifndef __APPLE__
	pthread_setname_np(expiry_thread, "ssgl-expiry");
#endif
	ssgl_log(SSGL_INFO, "ssgl_server: game server expiration thread started.\n");
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
	struct ssgl_game_server *directory = NULL;
	struct ssgl_client_filter filter;
	int nentries, i, rc, be_nentries;
	char client_ip[100];

	get_peer_name(connection, client_ip);
	ssgl_log(SSGL_INFO, "ssgl_server: new game client %s\n", client_ip);

	while (1) {
		/* printf("ssgl_server: reading filter data.\n"); */
		rc = ssgl_readsocket(connection, &filter, sizeof(filter));
		if (rc)
			goto badclient;

		fill_trailing_zeroes(filter.game_type, sizeof(filter.game_type));
		ssgl_log(SSGL_INFO, "ssgl_server: client %s requested filter '%s'\n",
				client_ip, filter.game_type);

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
		if (directory)
			free(directory);
		directory = NULL;
		ssgl_sleep(5);
	}

badclient:
	log_disconnect(SSGL_INFO, connection, "client disconnected");
	if (directory)
		free(directory);
	shutdown(connection, SHUT_RDWR);
	close(connection);
	return;
}

static void *service_thread(void *arg)
{
	int rc;
	struct ssgl_protocol_id proto_id;
	struct service_thread_info *threadinfo = arg;
	int connection = threadinfo->connection;

	char threadname[16];
	snprintf(threadname, sizeof(threadname), "ssglcon-%d", connection);
#ifndef __APPLE__
	pthread_setname_np(threadinfo->thread, threadname);
#endif
	/* Get the SSGL protocol version number from connection
	 * and make sure it's a version we understand.
	 */

	rc = get_protocol_version(connection, &proto_id);
	if (rc < 0)
		goto out;	

	/* Determine if this is a game client or a game server connecting */
	if (proto_id.client_type == SSGL_GAME_SERVER)
		service_game_server(connection, threadinfo);
	else
		service_game_client(connection);
out:
	log_disconnect(SSGL_INFO, connection, "service thread terminated.");
	shutdown(connection, SHUT_RDWR);
	close(connection);
	free(threadinfo); /* allocated prior to thread creation in service(), below. */
	return 0; /* implicit pthread_exit(); here */
}

static void service(int connection, struct sockaddr_in remote_addr, socklen_t remote_addr_len)
{
	pthread_attr_t attr;
	int rc;
	char client_ip[50];
	struct service_thread_info *threadinfo;

	/* printf("ssgl_server: servicing connection %d\n", connection); */
	/* get connection moved off the stack so that when the thread needs it,
	 * it's actually still around. 
	 */
	threadinfo = malloc(sizeof(*threadinfo)); /* will be freed in service_thread(). */
	threadinfo->connection = connection; /* linux overcommits, no sense in checking malloc return. */
	threadinfo->remote_addr = remote_addr;
	threadinfo->remote_addr_len = remote_addr_len;

	get_peer_name(connection, client_ip);
	ssgl_log(SSGL_INFO, "ssgl_server: New connection from %s\n", client_ip);

	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
	rc = pthread_create(&threadinfo->thread, &attr, service_thread, threadinfo);
	pthread_attr_destroy(&attr);
	if (rc) {
		ssgl_log(SSGL_ERROR, "ssgl_server: pthread_create failed for %s, rc = %d, errno = %d\n",
			client_ip, rc, errno); 
		shutdown(SHUT_RDWR, threadinfo->connection);
		close(threadinfo->connection);
		free(threadinfo);
	}
}

static void start_game_server_expiration_thread(void)
{
	pthread_attr_t attr;
	int rc;


	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
	rc = pthread_create(&expiry_thread, &attr, expire_game_servers, NULL);
	pthread_attr_destroy(&attr);
	if (rc < 0) {
		ssgl_log(SSGL_ERROR, "Unable to create game server expiration thread: %s.\n",
			strerror(errno));
	}
}

static pthread_t broadcast_thread;

struct broadcast_lobby_thread_info {
	unsigned short udp_port;
	unsigned short tcp_port;
};

static void *broadcast_lobby_info(__attribute__((unused)) void *arg)
{
	struct broadcast_lobby_thread_info *ports = arg;
	int on, rc, bcast;
	uint32_t ipaddr, netmask, broadcast;
	struct ssgl_lobby_descriptor payload;
	struct ifaddrs *ifaddr, *a;
	struct sockaddr_in bcast_addr;
	static char hostname[32] = { 0 };
	struct in_addr ip;

	if (hostname[0] == 0) {
		rc = gethostname(hostname, sizeof(hostname) - 1);
		if (rc)
			memset(hostname, 0, sizeof(hostname));
		hostname[31] = 0;
	}

	/* Get the IP address of this machine's primary interface (the IP address of
	 * interface that would be used to send a packet to 8.8.8.8 if we were to
	 * try that. */
	rc = ssgl_get_primary_host_ip_addr(&ipaddr);
	if (rc < 0) {
		ssgl_log(SSGL_WARN, "broadcast_lobby_info: Failed to get lobby host primary IP address: %s.\n",
			strerror(errno));
		return NULL;
	}
	ip.s_addr = ipaddr;
	ssgl_log(SSGL_WARN, "broadcasting IP addr %s\n", inet_ntoa(ip));

	/* Make ourselves a UDP datagram socket */
	bcast = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (bcast < 0) {
		ssgl_log(SSGL_ERROR, "broadcast_lobby_info: socket() failed: %s\n", strerror(errno));
		return NULL;
	}

	/* Set socket up for broadcasting */
	on = 1;
	rc = setsockopt(bcast, SOL_SOCKET, SO_REUSEADDR | SO_BROADCAST, (const char *) &on, sizeof(on));
	if (rc < 0) {
		ssgl_log(SSGL_ERROR, "setsockopt() failed: %s\n", strerror(errno));
		close(bcast);
		return NULL;
	}

	/* Get the netmask for our primary network interface */
	rc = getifaddrs(&ifaddr);
	if (rc < 0) {
		ssgl_log(SSGL_ERROR, "getifaddrs() failed: %s\n", strerror(errno));
		close(bcast);
		return NULL;
	}

	netmask = 0xa5a5a5a5; /* guess something excessively improbable */
	for (a = ifaddr; a; a = a->ifa_next) {
		uint32_t s;
		if (a->ifa_addr == NULL)
			continue;
		if (a->ifa_addr->sa_family != AF_INET)
			continue;
		s = ((struct sockaddr_in *) a->ifa_addr)->sin_addr.s_addr;
		bcast_addr = *(struct sockaddr_in *) a->ifa_addr;
		ssgl_log(SSGL_INFO, "comparing ipaddr %08x to %08x\n", s, ipaddr);
		if (s == ipaddr) {
			ssgl_log(SSGL_INFO, "Matched IP address %08x\n", ipaddr);
			netmask = ((struct sockaddr_in *) a->ifa_netmask)->sin_addr.s_addr;
			break;
		}
	}
	if (ifaddr)
		freeifaddrs(ifaddr);
	if (netmask == 0xa5a5a5a5) {
		ssgl_log(SSGL_ERROR, "failed to get netmask.\n");
		close(bcast);
		return NULL;
	}
	broadcast = ~netmask | ipaddr;
	memcpy(&bcast_addr.sin_addr.s_addr, &broadcast, sizeof(broadcast));
	bcast_addr.sin_port = ports->udp_port;

	ssgl_log(SSGL_INFO, "netmask is %08x\n", netmask);
	ssgl_log(SSGL_INFO, "broadcast is %08x\n", broadcast);
	payload.ipaddr = ipaddr;
	payload.port = ports->tcp_port;
	memcpy(payload.hostname, hostname, sizeof(payload.hostname));

	do {
		rc = sendto(bcast, &payload, sizeof(payload), 0, &bcast_addr, sizeof(struct sockaddr_in));
		if (rc < 0)
			ssgl_log(SSGL_ERROR, "sendto failed: %s\n", strerror(errno));
		ssgl_sleep(2);
	} while (1);
	close(bcast);
	return NULL;
}

static void start_broadcast_lobby_info_thread(struct broadcast_lobby_thread_info *ports)
{
	pthread_attr_t attr;
	int rc;

	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
	rc = pthread_create(&broadcast_thread, &attr, broadcast_lobby_info, ports);
	pthread_attr_destroy(&attr);
	if (rc < 0) {
		ssgl_log(SSGL_ERROR, "Unable to create broadcast_lobby_info thread: %s.\n",
			strerror(errno));
	}
}

static int find_ip_address(void)
{
	uint32_t ipaddr = 0;
	struct in_addr ip;

	if (ssgl_get_primary_host_ip_addr(&ipaddr) == 0) {
		ip.s_addr = ipaddr;
		fprintf(stdout, "%s\n", inet_ntoa(ip));
		return 0;
	} else {
		fprintf(stderr, "Unable to find IP address\n");
		return -1;
	}
}

int main(int argc, char *argv[])
{
	int rendezvous, connection, rc;
	struct sockaddr_in listen_address;
	struct sockaddr_in remote_addr;
	socklen_t remote_addr_len;
	int on;
	const char daemon_failed[] = "daemon(3) failed, exiting.\n";
	struct broadcast_lobby_thread_info ports;

	if (argc > 1 && strcmp(argv[1], "--find-ip-address") == 0)
		exit(find_ip_address());

	if (daemon(1, 0) != 0) {
		if (write(2, daemon_failed, sizeof(daemon_failed)) != sizeof(daemon_failed))
			_exit(-2);
		else
			_exit(-1);
	}

	signal(SIGPIPE, SIG_IGN);

	if (ssgl_open_logfile("SSGL_LOGFILE"))
		return 0;

	memset(game_server, 0, sizeof(game_server));
	memset(expiration, 0, sizeof(expiration));
	ngame_servers = 0;

	ssgl_log(SSGL_INFO, "ssgl_server starting\n");

	ports.tcp_port = ssgl_get_gamelobby_port("tcp");
	ports.udp_port = ssgl_get_gamelobby_port("udp");

	start_game_server_expiration_thread();
	start_broadcast_lobby_info_thread(&ports);

	/* Make ourselves a socket */
	rendezvous = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (rendezvous < 0) {
		ssgl_log(SSGL_ERROR, "socket() failed: %s\n", strerror(errno));
		return -1;
	}

	on = 1;
	rc = setsockopt(rendezvous, SOL_SOCKET, SO_REUSEADDR, (const char *) &on, sizeof(on));
	if (rc < 0) {
		ssgl_log(SSGL_ERROR, "setsockopt() failed: %s\n", strerror(errno));
		ssgl_exit("setsockopt failed.", 1);
	}

	/* Bind the socket to "any address" on our port */
	listen_address.sin_family = AF_INET;
	listen_address.sin_addr.s_addr = INADDR_ANY;
	listen_address.sin_port = ports.tcp_port;
	rc = bind(rendezvous, (struct sockaddr *) &listen_address, sizeof(listen_address));
	if (rc < 0) {
		ssgl_log(SSGL_ERROR, "bind() failed: %s\n", strerror(errno));
		ssgl_exit("ssgl_server already running?", 1);
	}

	ssgl_log(SSGL_INFO, "ssgl_server: listening for connections...\n");
	/* Listen for incoming connections... */
	rc = listen(rendezvous, SOMAXCONN);
	if (rc < 0) {
		ssgl_log(SSGL_ERROR, "listen() failed: %s\n", strerror(errno));
		ssgl_exit("listen failed.", 1);
	}

	while (1) {
		remote_addr_len = sizeof(remote_addr);
		/* printf("Accepting connection...\n"); */
		connection = accept(rendezvous, (struct sockaddr *) &remote_addr, &remote_addr_len);
		/* printf("accept returned %d\n", connection); */
		if (connection < 0) {
			/* handle failed connection... */
			ssgl_log(SSGL_WARN, "accept() failed: %s\n", strerror(errno));
			ssgl_sleep(1);
			continue;
		}
		if (remote_addr_len != sizeof(remote_addr)) {
			ssgl_log(SSGL_WARN, "strange socket address length %d\n", remote_addr_len);
			shutdown(connection, SHUT_RDWR);
			close(connection);
			continue;
		}
		service(connection, remote_addr, remote_addr_len);
	}
	return 0;
}

