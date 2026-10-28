/*
	This file (snis_web_ssgl.c) is entirely machine-generated. To the extent
	possible under law, the author(s) have dedicated all copyright and related and
	neighboring rights to this software to the public domain worldwide.  This
	software is distributed without any warranty.

	You should have received a copy of the CC0 Public Domain Dedication along with
	this software. If not, see <http://creativecommons.org>.
*/

/*
 * The lobby connection, for the browser build of snis_client.
 *
 * These replace ssgl/ssgl_connect_to_lobby.c and ssgl/ssgl_socket_io.c when
 * building with "make WEB=1", so that the client reaches the lobby the same
 * way it reaches snis_server: through snis_net_transport, which in a browser
 * is a WebSocket relayed by snis_web_gateway.  The lobby protocol is
 * unchanged, and so is the rest of ssgl.
 *
 * Only what snis_client uses is here: a browser never registers a game
 * server with the lobby.
 */

#include <stdio.h>
#include <errno.h>
#include <arpa/inet.h>

#include "ssgl/ssgl.h"
#include "ssgl/ssgl_socket_io.h"
#include "ssgl/ssgl_protocol_id.h"
#include "snis_net_transport.h"

int ssgl_readsocket(int fd, void *buffer, int buflen)
{
	char *c = buffer;
	int rc;

	while (buflen > 0) {
		rc = snis_net_recv(fd, c, buflen);
		if (rc == 0) /* other side closed conn */
			return -1;
		if (rc < 0) {
			if (errno == EINTR)
				continue;
			return rc;
		}
		buflen -= rc;
		c += rc;
	}
	return 0;
}

int ssgl_writesocket(int fd, void *buffer, int buflen)
{
	char *c = buffer;
	int rc;

	while (buflen > 0) {
		rc = snis_net_send(fd, c, buflen);
		if (rc < 0) {
			if (errno == EINTR)
				continue;
			return rc;
		}
		buflen -= rc;
		c += rc;
	}
	return 0;
}

int ssgl_gameclient_connect_to_lobby_port(char *ssgl_hostname, int port)
{
	struct ssgl_protocol_id proto_id = { -1, SSGL_SIGNATURE_STRING, SSGL_GAME_CLIENT };
	char portstr[20], errmsg[200];
	int sock;

	if (port <= 0 || port > 65535)
		port = ntohs(ssgl_get_gamelobby_port("tcp"));
	snprintf(portstr, sizeof(portstr), "%d", port);
	sock = snis_net_connect(ssgl_hostname, portstr, errmsg, sizeof(errmsg));
	if (sock < 0)
		return -1;
	proto_id.protocol_version = htonl(SSGL_PROTOCOL_VERSION);
	if (ssgl_writesocket(sock, &proto_id, sizeof(proto_id)) != 0) {
		snis_net_close(sock);
		return -1;
	}
	return sock;
}

int ssgl_gameclient_connect_to_lobby(char *ssgl_hostname)
{
	return ssgl_gameclient_connect_to_lobby_port(ssgl_hostname, -1);
}
