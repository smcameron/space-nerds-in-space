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
#include <stdlib.h>

#include "snis-device-io.h"
#include "pthread_util.h"
#include "ssgl/ssgl.h"

static pthread_t output_thread;
static pthread_mutex_t yoke_mutex = PTHREAD_MUTEX_INITIALIZER;

static unsigned short pitch_value = 500;
static unsigned short roll_value = 500;

static void *output_thread_fn(__attribute__((unused)) void *n)
{
	int rc;
	int delay_time = 250;
	unsigned short roll, pitch;
	struct snis_device_io_connection *con = NULL;

	rc = snis_device_io_setup(&con);
	if (rc < 0) {
		fprintf(stderr, "Failed to setup SNIS device io.\n");
		exit(1);
	}

	do {
		pthread_mutex_lock(&yoke_mutex);
		roll = roll_value;
		pitch = pitch_value;
		pthread_mutex_unlock(&yoke_mutex);
		delay_time = 250;
		if (pitch < 400)
			(void) snis_device_io_send(con, DEVIO_OPCODE_NAV_PITCH_DOWN, 0);
		if (pitch > 600)
			(void) snis_device_io_send(con, DEVIO_OPCODE_NAV_PITCH_UP, 0);
		if (roll < 400)
			(void) snis_device_io_send(con, DEVIO_OPCODE_NAV_ROLL_RIGHT, 0);
		if (roll > 600)
			(void) snis_device_io_send(con, DEVIO_OPCODE_NAV_ROLL_LEFT, 0);
		if (pitch < 300 || pitch > 700 || roll < 300 || roll > 700)
			delay_time = 100;
		ssgl_msleep(delay_time);
	} while (1);
	return NULL;
}

int main(__attribute__((unused)) int argc, __attribute__((unused)) char *argv[])
{
	struct snis_device_io_connection *con = NULL;
	char yoke_input[255];
	char axis;
	unsigned short value;
	int rc;

	FILE *yoke = fopen("/dev/ttyACM0", "r");
	if (!yoke) {
		fprintf(stderr, "Can't open /dev/ttyACM0: %s\n", strerror(errno));
		exit(1);
	}

	rc = create_thread(&output_thread,  output_thread_fn, NULL, "yoke-thr", 1);
	if (rc != 0) {
		fprintf(stderr, "Failed to create output thread.\n");
		exit(1);
	}

	for (;;) {
		if (fgets(yoke_input, sizeof(yoke_input), yoke) == NULL) {
			fprintf(stderr, "End of yoke input, bailing out.\n");
			goto done;
		}
		rc = sscanf(yoke_input, "%c:%hu\n", &axis, &value);
		if (rc != 2) {
			fprintf(stderr, "Bad yoke input '%s'\n", yoke_input);
			continue;
		}
		pthread_mutex_lock(&yoke_mutex);
		if (axis == 'P')
			pitch_value = value;
		if (axis == 'R')
			roll_value = value;
		pthread_mutex_unlock(&yoke_mutex);
	}
done:
	if (con)
		snis_device_io_shutdown(con);
	return 0;
}

