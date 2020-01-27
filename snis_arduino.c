/*
	Copyright (C) 2020 Stephen M. Cameron
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
#include <errno.h>

#include "snis-device-io.h"

#include "arraysize.h"

static unsigned short input_eng_opcode[] = {
	DEVIO_OPCODE_ENG_PWR_SHIELDS,
	DEVIO_OPCODE_ENG_PWR_PHASERS,
	DEVIO_OPCODE_ENG_PWR_COMMS,
	DEVIO_OPCODE_ENG_PWR_SENSORS,
	DEVIO_OPCODE_ENG_PWR_IMPULSE,
	DEVIO_OPCODE_ENG_PWR_WARP,
	DEVIO_OPCODE_ENG_PWR_MANEUVERING,
	DEVIO_OPCODE_ENG_PWR_TRACTOR,
	DEVIO_OPCODE_ENG_COOL_SHIELDS,
	DEVIO_OPCODE_ENG_COOL_PHASERS,
	DEVIO_OPCODE_ENG_COOL_COMMS,
	DEVIO_OPCODE_ENG_COOL_SENSORS,
	DEVIO_OPCODE_ENG_COOL_IMPULSE,
	DEVIO_OPCODE_ENG_COOL_WARP,
	DEVIO_OPCODE_ENG_COOL_MANEUVERING,
	DEVIO_OPCODE_ENG_COOL_TRACTOR,
	DEVIO_OPCODE_ENG_SHIELD_LEVEL,
	DEVIO_OPCODE_ENG_PRESET1_BUTTON,
	DEVIO_OPCODE_ENG_PRESET2_BUTTON,
	DEVIO_OPCODE_ENG_DAMAGE_CTRL,
	DEVIO_OPCODE_ENG_PWR_LIFESUPPORT,
	DEVIO_OPCODE_ENG_COOL_LIFESUPPORT,
	DEVIO_OPCODE_ENG_SILENCE_ALARMS,
	DEVIO_OPCODE_ENG_CHAFF,
};

static void usage(void)
{
	fprintf(stderr, "Usage: snis_arduino <device>\n");
	exit(1);
}

static void interpret_command(char *command, __attribute__ ((unused)) struct snis_device_io_connection *snis_client)
{
	int rc;
	int input_channel, input_value;
	unsigned short opcode, value;

	fprintf(stderr, "snis_arduino: interpreting command: %s", command);

	rc = sscanf(command, "#%d=%d\n", &input_channel, &input_value);
	if (rc != 2)
		return;
	fprintf(stderr, "Sending command %d,%d\n", input_channel, input_value);

	if (input_channel >= 0 && (unsigned int) input_channel < ARRAYSIZE(input_eng_opcode)) {
		opcode = input_eng_opcode[input_channel];
		value = (unsigned short) (64 * input_value);
		rc = snis_device_io_send(snis_client, opcode, value);
		if (rc) {
			fprintf(stderr, "Error sending opcode,value %d,%d to snis_client: %s\n",
				opcode, value, strerror(errno));
		}
	}
}

int main(int argc, char *argv[])
{
	int rc;
	char *device, *s;
	FILE *d;
	char buffer[1024];
	struct snis_device_io_connection *snis_client;

	fprintf(stderr, "snis_arduino begins\n");
	rc = snis_device_io_setup(&snis_client);
	if (rc != 0) {
		fprintf(stderr, "snis_arduino: snis_device_io_setup() failed\n");
		exit(1);
	}

	if (argc < 2)
		usage();
	device = argv[1];
	d = fopen(device, "rw");
	if (!d) {
		fprintf(stderr, "snis_arduino: Failed to open %s for read/write\n", device);
		return -1;
	}
	fprintf(stderr, "snis_arduino: opened %s\n", device);

	do {
		memset(buffer, 0, sizeof(buffer));
		errno = 0;
		s = fgets(buffer, sizeof(buffer), d);
		if (!s) {
			switch (errno) {
			case 0:
				/* I would sort of expect fgets() to block if there's no data from the
				 * Arduino, but it doesn't, it just returns eof. */
				usleep(100000); /* Wait a few milliseconds and try again */
				continue;
			case EINTR:
				continue;
			default:
				break;
			}
			fprintf(stderr, "snis_arduino: Error on device %s: %s\n", device, strerror(errno));
			goto bailout;
		}
		interpret_command(buffer, snis_client);
	} while (1);

bailout:
	fclose(d);
	return 0;
}
