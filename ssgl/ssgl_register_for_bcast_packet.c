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

static struct bcast_info {
	uint32_t *ipaddr;
	uint16_t *port;
} binfo; /* FIXME, there can be only one... */

struct bcast_payload {
	uint32_t ipaddr;
	uint16_t port;
};

static void *broadcast_recv_thread(__attribute__((unused)) void *arg)
{

	int rc, bcast;
	struct bcast_payload payload;
	struct servent *gamelobby_service;
	struct protoent *gamelobby_proto;
	struct sockaddr_in bcast_addr;
	struct sockaddr remote_addr;
	socklen_t remote_addr_len;

	/* Get the "gamelobby" UDP service protocol/port */
	gamelobby_service = getservbyname(GAMELOBBY_SERVICE_NAME, "udp");
	if (!gamelobby_service) {
		fprintf(stderr, "getservbyname failed, %s\n", strerror(errno));
		fprintf(stderr, "Check that /etc/services contains the following lines:\n");
		fprintf(stderr, "gamelobby	2419/tcp\n");
		fprintf(stderr, "gamelobby	2419/udp\n");
		fprintf(stderr, "Continuing anyway, will assume port is 2419.\n");
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

	/* Make ourselves a UDP datagram socket */
	bcast = socket(AF_INET, SOCK_DGRAM, gamelobby_proto ? gamelobby_proto->p_proto : 0);
	if (bcast < 0) {
		fprintf(stderr, "broadcast_lobby_info: socket() failed: %s\n", strerror(errno));
		return NULL;
	}

	bcast_addr.sin_family = AF_INET;
	bcast_addr.sin_addr.s_addr = INADDR_ANY;
	bcast_addr.sin_port =
		gamelobby_service ?  gamelobby_service->s_port : htons(GAMELOBBY_SERVICE_NUMBER);

	rc = bind(bcast, (struct sockaddr *) &bcast_addr, sizeof(bcast_addr));
	if (rc < 0) {
		fprintf(stderr, "bind() bcast_addr failed: %s\n", strerror(errno));
		return NULL;
	}

	do {
		memset(&payload, 0, sizeof(payload));
		rc = recvfrom(bcast, &payload, sizeof(payload), 0, &remote_addr, &remote_addr_len);
		if (rc < 0) {
			fprintf(stderr, "sendto failed: %s\n", strerror(errno));
		} else {
			fprintf(stderr, "Received broadcast lobby info: addr = %08x, port = %04x\n",
				ntohl(payload.ipaddr), ntohs(payload.port));
			*binfo.ipaddr = payload.ipaddr;
			*binfo.port = payload.port;
		}
	} while (1);
	close(bcast);
	return NULL;
}

int ssgl_register_for_bcast_packet(uint32_t *ipaddr, uint16_t *port)
{
	pthread_attr_t attr;
	int rc;

	binfo.ipaddr = ipaddr;
	binfo.port = port;
	*binfo.ipaddr = 0xffffffff;
	*binfo.port = 0xffff;

	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
	rc = pthread_create(&bcast_recv_thread, &attr, broadcast_recv_thread, NULL);
	pthread_attr_destroy(&attr);
	if (rc < 0) {
		fprintf(stderr, "Unable to create broadcast receiver thread: %s.\n",
			strerror(errno));
		return -1;
	}
	return 0;
}


