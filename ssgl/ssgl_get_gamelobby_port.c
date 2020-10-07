#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <string.h>

#include "ssgl.h"

unsigned short ssgl_get_gamelobby_port(char *proto)
{
	struct addrinfo hints;
	struct addrinfo *lobbyserverinfo;
	int rc;
	unsigned short port = -1;
	char *override_port;

	/* Allow overriding the port */
	override_port = getenv("SSGL_PORT");
	if (override_port) {
		rc = sscanf(override_port, "%hu", &port);
		if (rc == 1) {
			port = htons(port);
			return port;
		}
	}

	/* Find the gamelobby tcp port */
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = 0;
	if (strcmp(proto, "tcp") == 0)
		hints.ai_protocol = IPPROTO_TCP;
	else if (strcmp(proto, "udp") == 0)
		hints.ai_protocol = IPPROTO_UDP;
	rc = getaddrinfo(NULL, GAMELOBBY_SERVICE_NAME, &hints, &lobbyserverinfo);
	if (rc == 0) {
		struct sockaddr_in *s = (struct sockaddr_in *) lobbyserverinfo->ai_addr;
		port = s->sin_port;
		freeaddrinfo(lobbyserverinfo);
	} else {
		hints.ai_flags |= AI_NUMERICSERV;
		rc = getaddrinfo(NULL, GAMELOBBY_SERVICE_NUMBER_AS_STRING,
					&hints, &lobbyserverinfo);
		if (rc == 0) {
			struct sockaddr_in *s = (struct sockaddr_in *) lobbyserverinfo->ai_addr;
			port = s->sin_port;
			freeaddrinfo(lobbyserverinfo);
		} else {
			port = htons(2914);
		}
	}
	return port;
}

