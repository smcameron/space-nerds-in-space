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
#include <string.h>
#include <errno.h>

#include "snis-device-io.h"


int main(__attribute__((unused)) int argc, __attribute__((unused)) char *argv[])
{
	struct snis_device_io_connection *con = NULL;
	char *c;
	char buf[100];
	unsigned short opcode, value;
	int rc;

	rc = snis_device_io_setup(&con);
	if (rc < 0) {
		fprintf(stderr, "Failed to setup SNIS device io.\n");
		return -1;
	}
	for (;;) {
		printf("Enter device i/o opcode and value, 'q' to exit: ");
		fflush(stdout);
		memset(buf, 0, sizeof(buf));
		c = fgets(buf, sizeof(buf) - 1, stdin);
		if (!c)
			goto done;
		if (strncmp(c, "q", 1) == 0)
			goto done;
		rc = sscanf(buf, "%hu %hu", &opcode, &value);
		if (rc != 2) {
			fprintf(stderr, "Syntax error, try again\n");
			continue;
		}
		rc = snis_device_io_send(con, opcode, value);
		if (rc) {
			fprintf(stderr, "Error sending device io: %s\n", strerror(errno));
			/* just keep going, maybe the client crashed, user can restart it */
		}
	}
done:
	if (con)
		snis_device_io_shutdown(con);
	return 0;
}

