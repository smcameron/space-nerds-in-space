/*
	Copyright (C) 2010 Stephen M. Cameron
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

/* TCP implementation of snis_net_transport.h */

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>

#define DEFINE_SNIS_NET_TRANSPORT_GLOBALS
#include "snis_net_transport.h"

int snis_net_connect(const char *host, const char *port, char *errmsg, int errmsglen)
{
	int rc, fd;
	struct addrinfo *info, *i;
	struct addrinfo hints;
	int host_is_numeric, a, b, c, d;
	int port_is_numeric, p;
	int flag = 1;
	uint8_t iptos_lowdelay = IPTOS_LOWDELAY;

	/* Check if the host is numeric... */
	rc = sscanf(host, "%d.%d.%d.%d", &a, &b, &c, &d);
	host_is_numeric = (rc == 4);
	/* Check if the port is numeric */
	rc = sscanf(port, "%d", &p);
	port_is_numeric = (rc == 1);

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE |
			(AI_NUMERICSERV * port_is_numeric) | (AI_NUMERICHOST * host_is_numeric);
	rc = getaddrinfo(host, port, &hints, &info);
	if (rc) {
		fprintf(stderr, "snis_client: Failed looking up %s:%s: %s\n",
			host, port, gai_strerror(rc));
		snprintf(errmsg, errmsglen, "snis_client: Failed looking up %s:%s: %s\n",
				host, port, gai_strerror(rc));
		return SNIS_NET_LOOKUP_FAILED;
	}

	for (i = info; i != NULL; i = i->ai_next)
		if (i->ai_family == AF_INET)
			break;
	if (i == NULL) {
		freeaddrinfo(info);
		return SNIS_NET_LOOKUP_FAILED;
	}

	fd = socket(AF_INET, SOCK_STREAM, i->ai_protocol);
	if (fd < 0) {
		freeaddrinfo(info);
		return SNIS_NET_CONNECT_FAILED;
	}

	rc = connect(fd, i->ai_addr, i->ai_addrlen);
	freeaddrinfo(info);
	if (rc < 0) {
		fprintf(stderr, "Connect failed: %s\n", strerror(errno));
		close(fd);
		return SNIS_NET_CONNECT_FAILED;
	}

	rc = setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, (char *) &flag, sizeof(int));
	if (rc) {
		fprintf(stderr, "setsockopt(TCP_NODELAY) failed.\n");
		snprintf(errmsg, errmsglen, "setsockopt(TCP_NODELAY) failed.");
	}
	rc = setsockopt(fd, IPPROTO_IP, IP_TOS, &iptos_lowdelay, sizeof(uint8_t));
	if (rc) {
		fprintf(stderr, "setsockopt(IPTOS_LOWDELAY) failed.\n");
		snprintf(errmsg, errmsglen, "setsockopt(IPTOS_LOWDELAY) failed.");
	}
	return fd;
}

int snis_net_recv(int fd, void *buffer, int len)
{
	return recv(fd, buffer, len, 0);
}

int snis_net_send(int fd, void *buffer, int len)
{
	return send(fd, buffer, len, 0);
}

void snis_net_close(int fd)
{
	shutdown(fd, SHUT_RDWR);
	close(fd);
}
