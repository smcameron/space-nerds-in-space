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
#include <stdint.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include "ssgl.h"
#include "ssgl_socket_io.h"
#include "ssgl_protocol_id.h"

static struct ssgl_protocol_id proto_id[] = {
	{ -1, SSGL_SIGNATURE_STRING, SSGL_GAME_SERVER },
	{ -1, SSGL_SIGNATURE_STRING, SSGL_GAME_CLIENT },
};

static int send_protocol_id_to_lobby(int lobbysock, int client)
{
	int rc;

	proto_id[0].protocol_version = htonl(SSGL_PROTOCOL_VERSION);
	proto_id[1].protocol_version = htonl(SSGL_PROTOCOL_VERSION);
	rc = ssgl_writesocket(lobbysock, &proto_id[client ? 1 : 0], sizeof(proto_id[0]));
	return rc;
}

static int ssgl_connect_to_lobby(char *ssgl_hostname, int client, int specific_port)
{
	int rc;
	struct addrinfo *lobbyserverinfo, *i;
	struct addrinfo hints;
	int lobby_sock;
	unsigned short tcp_port;

	if (specific_port > 0 && specific_port <= 65535)
		tcp_port = htons((unsigned short) specific_port);
	else
		tcp_port = ssgl_get_gamelobby_port("tcp");
	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;
	hints.ai_protocol = IPPROTO_TCP;
	rc = getaddrinfo(ssgl_hostname, NULL, &hints, &lobbyserverinfo);
	if (rc) {
		hints.ai_flags |= AI_NUMERICSERV;
		rc = getaddrinfo(ssgl_hostname, GAMELOBBY_SERVICE_NUMBER_AS_STRING,
					&hints, &lobbyserverinfo);
		if (rc)
			return rc;
	}

	for (i = lobbyserverinfo; i != NULL; i = i->ai_next) {
		if (i->ai_family == AF_INET) {
			struct sockaddr_in *s = (struct sockaddr_in *) lobbyserverinfo->ai_addr;
			s->sin_port = tcp_port;
			break;
		}
	}
	if (i == NULL)
		return -1;

	lobby_sock = socket(AF_INET, SOCK_STREAM, i->ai_protocol); 
	if (lobby_sock < 0)
		return lobby_sock;

	rc = connect(lobby_sock, i->ai_addr, i->ai_addrlen);
	if (rc < 0)
		return rc;

	rc = send_protocol_id_to_lobby(lobby_sock, client);
	if (rc) {
		shutdown(lobby_sock, SHUT_RDWR);
		close(lobby_sock);
		return rc;
	}

	freeaddrinfo(lobbyserverinfo);
	return lobby_sock;	
}

int ssgl_gameserver_connect_to_lobby_port(char *ssgl_hostname, int port)
{
	return ssgl_connect_to_lobby(ssgl_hostname, 0, port);
}

int ssgl_gameclient_connect_to_lobby_port(char *ssgl_hostname, int port)
{
	return ssgl_connect_to_lobby(ssgl_hostname, 1, port);
}

int ssgl_gameserver_connect_to_lobby(char *ssgl_hostname)
{
	return ssgl_connect_to_lobby(ssgl_hostname, 0, -1);
}

int ssgl_gameclient_connect_to_lobby(char *ssgl_hostname)
{
	return ssgl_connect_to_lobby(ssgl_hostname, 1, -1);
}

