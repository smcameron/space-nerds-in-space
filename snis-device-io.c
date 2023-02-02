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
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/un.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <errno.h>

#include "snis-device-io.h"
#include "string-utils.h"

struct snis_device_io_connection {
	struct sockaddr_un server_addr, client_addr;
	int client_socket;
};

void snis_device_io_shutdown(struct snis_device_io_connection *con)
{
	memset(con, 0, sizeof(*con));
	free(con);
}

int snis_device_io_setup(struct snis_device_io_connection **con)
{
	struct sockaddr_un server_addr, client_addr;
	int s;
	pid_t pid;
	int rc = 0;

	pid = getpid();
	if (pid < 0)
		return pid;

	*con = malloc(sizeof(**con));
	if (!*con)
		return -ENOMEM;
	memset(*con, 0, sizeof(**con));

	memset(&client_addr, 0, sizeof(client_addr));
	/* Note: client_addr.sun_path[0] is set to 0 by above memset.  Setting the
	 * name starting at addr.sun_path[1], below -- after the initial 0 byte --
	 * is a convention to set up a name in the linux-specific abstract socket
	 * name space.  See "The Linux Programming Interface", by Michael Kerrisk,
	 * Ch. 57, p. 1175-1176.  To get this to work on non-linux, we'll need
	 * to do something else.
	 */
	client_addr.sun_family = AF_UNIX;
	snprintf(&client_addr.sun_path[1], sizeof(client_addr.sun_path) - 2,
				"snis-phys-io-%lu", (unsigned long) pid);
	s = socket(AF_UNIX, SOCK_DGRAM, 0);
	if (s < 0) {
		rc = s;
		goto error;
	}
	rc = bind(s, (struct sockaddr *) &client_addr, sizeof(client_addr));
	if (rc < 0)
		goto error;

	memset(&server_addr, 0, sizeof(server_addr));
	server_addr.sun_family = AF_UNIX;
	strlcpy(&server_addr.sun_path[1], "snis-phys-io", sizeof(server_addr.sun_path) - 1);
	memcpy(&(*con)->client_addr, &client_addr, sizeof(client_addr));
	memcpy(&(*con)->server_addr, &server_addr, sizeof(server_addr));
	(*con)->client_socket = s;
	return 0;
error:
	free(*con);
	return rc;
}

int snis_device_io_send(struct snis_device_io_connection *con,
			unsigned short opcode, unsigned short value)
{
	size_t msglen;
	int rc;
	unsigned short netopcode, netvalue;
	unsigned char buf[2 * sizeof(unsigned short)];
	unsigned char *b = buf;
	
	netopcode = htons(opcode);
	netvalue = htons(value);
	msglen = 2 * sizeof(unsigned short);
	memcpy(b, &netopcode, sizeof(netopcode));
	b += sizeof(netopcode);
	memcpy(b, &netvalue, sizeof(netvalue));
	rc = sendto(con->client_socket, buf, msglen, 0, (struct sockaddr *) &con->server_addr,
			sizeof(con->server_addr));
	if (rc < 0 || (size_t) rc != msglen)
		return -1;
	return 0;
}

