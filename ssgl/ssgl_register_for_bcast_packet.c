#define _GNU_SOURCE
#include <stdio.h>
#include <unistd.h>
#include <stdint.h>
#include <errno.h>
#include <pthread.h>
#include <string.h>
#include <netdb.h>
#include <arpa/inet.h>

#include "ssgl.h"

static pthread_t bcast_recv_thread;
static pthread_t lobby_expiration_thread;

#define MAXLOBBIES 256
static struct ssgl_lobby_descriptor lobbylist[MAXLOBBIES];
static struct ssgl_lobby_descriptor oldlist[MAXLOBBIES];
static int nlobbies = 0;
static int oldnlobbies = 0;
static char stale_lobby[MAXLOBBIES] = { 0 };
static pthread_mutex_t lobbylist_mutex = PTHREAD_MUTEX_INITIALIZER;

static void (*notification_fn)(struct ssgl_lobby_descriptor *, int) = NULL;

#define LOBBY_STALENESS_THRESHOLD 10
static void *expire_lobbies(__attribute__((unused)) void *cookie)
{
	int i;

	do {
		pthread_mutex_lock(&lobbylist_mutex);
		/* Mark everything as older */
		for (i = 0; i < nlobbies; i++)
			stale_lobby[i]++;

		for (i = 0; i < nlobbies;) {

			if (stale_lobby[i] > LOBBY_STALENESS_THRESHOLD) {
				memmove(&stale_lobby[i], &stale_lobby[i + 1],
						sizeof(stale_lobby[0]) * (nlobbies - i - 1));
				nlobbies--;
				continue;
			}
			i++;
		}

		/* If anything has changed, notify... */
		if ((nlobbies != oldnlobbies ||
			memcmp(oldlist, lobbylist, sizeof(oldlist[0]) * nlobbies) != 0) &&
			notification_fn)
			notification_fn(lobbylist, nlobbies);

		/* Remember how things are right now to check against later */
		oldnlobbies = nlobbies;
		memcpy(oldlist, lobbylist, sizeof(oldlist[0]) * nlobbies);

		pthread_mutex_unlock(&lobbylist_mutex);
		ssgl_sleep(1);
	} while (1);
	return NULL;
}

static void save_payload(struct ssgl_lobby_descriptor *p)
{
	int i;

	pthread_mutex_lock(&lobbylist_mutex);
	/* Do we already know about this lobby? */
	for (i = 0; i < nlobbies; i++) {
		if (memcmp(p, &lobbylist[i], sizeof(*p)) == 0) {
			stale_lobby[i] = 0;
			pthread_mutex_unlock(&lobbylist_mutex);
			return; /* We already know. */
		}
	}

	/* We found a new lobby... */
	if (nlobbies >= MAXLOBBIES) {
		pthread_mutex_unlock(&lobbylist_mutex);
		return; /* But there's no room for it */
	}

	/* Save this lobby info */
	p->hostname[31] = '\0';
	lobbylist[nlobbies] = *p;
	stale_lobby[nlobbies] = 0;
	nlobbies++;

	pthread_mutex_unlock(&lobbylist_mutex);
}

static void *broadcast_recv_thread(__attribute__((unused)) void *arg)
{

	int rc, bcast, on;
	struct ssgl_lobby_descriptor payload;
	struct sockaddr_in bcast_addr;
	struct sockaddr remote_addr;
	socklen_t remote_addr_len;
	unsigned short udp_port;

	udp_port = ssgl_get_gamelobby_port("udp");

	/* Make ourselves a UDP datagram socket */
	bcast = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (bcast < 0) {
		fprintf(stderr, "broadcast_lobby_info: socket() failed: %s\n", strerror(errno));
		return NULL;
	}

	bcast_addr.sin_family = AF_INET;
	bcast_addr.sin_addr.s_addr = INADDR_ANY;
	bcast_addr.sin_port = udp_port;

	/* Allow multiple snis_clients on the same host to recieve broadcast packets from ssgl */
	on = 1;
	setsockopt(bcast, SOL_SOCKET, SO_REUSEADDR, (const char *) &on, sizeof(on));

	rc = bind(bcast, (struct sockaddr *) &bcast_addr, sizeof(bcast_addr));
	if (rc < 0) {
		fprintf(stderr, "bind() bcast_addr failed: %s\n", strerror(errno));
		return NULL;
	}

	do {
		memset(&payload, 0, sizeof(payload));
		rc = recvfrom(bcast, &payload, sizeof(payload), 0, &remote_addr, &remote_addr_len);
		if (rc < 0) {
			fprintf(stderr, "recvfrom failed: %s\n", strerror(errno));
		} else {
			/* fprintf(stderr, "Received broadcast lobby info: addr = %08x, port = %04x\n",
				ntohl(payload.ipaddr), ntohs(payload.port)); */
			save_payload(&payload);
		}
	} while (1);
	close(bcast);
	return NULL;
}

int ssgl_register_for_bcast_packets(void (*lobby_change_notify)(struct ssgl_lobby_descriptor *, int))
{
	pthread_attr_t attr;
	int rc;

	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
	rc = pthread_create(&bcast_recv_thread, &attr, broadcast_recv_thread, NULL);
	pthread_attr_destroy(&attr);
	if (rc < 0) {
		fprintf(stderr, "Unable to create broadcast receiver thread: %s.\n",
			strerror(errno));
		return -1;
	}
	pthread_setname_np(bcast_recv_thread, "ssgl_bcastrx");

	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
	rc = pthread_create(&lobby_expiration_thread, &attr, expire_lobbies, NULL);
	pthread_attr_destroy(&attr);
	if (rc < 0) {
		fprintf(stderr, "Unable to create lobby expiration thread: %s.\n",
			strerror(errno));
		return -1;
	}
	pthread_setname_np(lobby_expiration_thread, "ssgl_lbbyexp");
	notification_fn = lobby_change_notify;
	return 0;
}


