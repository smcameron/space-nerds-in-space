#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <string.h>

#include "ssgl.h"

unsigned short ssgl_get_gamelobby_port(char *proto)
{
	struct addrinfo hints;
	struct addrinfo *lobbyserverinfo;
	unsigned short port;
	int rc;

	port = -1;

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
			port = htons(2419);
		}
	}
	return port;
}

