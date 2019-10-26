/*
	Copyright (C) 2018 Stephen M. Cameron
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
#define _GNU_SOURCE /* for pthread_setname_np */
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>


#include "snis_dmx.h"
#include "local_termios2.h"

#define MAX_DMX_DEVICE_CHAINS 10 /* Number of serial ports connected to DMX device chains */
#define DMX_BAUD_RATE 250000	 /* This is what the standard says, so you can't really change this. */
#define DMX_PACKET_SIZE 512
#define MARK_AFTER_BREAK (0xff)
#define NULL_START_BYTE (0x00)
#define SERIAL_BREAK_LENGTH_MS 80 /* Send BREAK for this many ms before sending next packet */
#define SLEEP_MS_BETWEEN_PACKETS 100 /* Wait this many ms after break sent before sending packet */

struct dmx_light {
	int16_t byte; /* Which byte in the DMX packet does this light start in? */
	uint8_t size; /* How many bytes? 1, or 3, typically */
};

static struct per_thread_data {
	pthread_t thread;
	int fd; /* file descriptor to serial device for this DMX light chain */
	struct dmx_light light[512];
	int nlights;
	uint8_t dmx_packet[DMX_PACKET_SIZE];
	int time_to_quit;
	int thread_in_use;
} thread_data[MAX_DMX_DEVICE_CHAINS] = { { 0 } };
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

static void thread_safe_msleep(int milliseconds)
{
	int rc;
	struct timespec req, rem;

	req.tv_sec = milliseconds / 1000;
	req.tv_nsec = 1000000L * (milliseconds - (req.tv_sec * 1000));
	rem.tv_sec = 0;
	rem.tv_nsec = 0;

	do {
		rc = nanosleep(&req, &rem);
		if (rc == 0)
			break;
		if (rc < 0 && errno == -EINTR) {
			req = rem;
			rem.tv_sec = 0;
			rem.tv_nsec = 0;
			continue;
		}
	} while (1);
}

static void init_per_thread_data(struct per_thread_data *t, int fd)
{
	t->fd = fd;
	memset(t->light, 0, sizeof(t->light));
	t->nlights = 0;
	memset(t->dmx_packet, 0, sizeof(t->dmx_packet));
	t->time_to_quit = 0;
}

/* Closes the device and ends the associated thread */
int snis_dmx_close_device(int handle)
{
	struct per_thread_data *t;

	if (handle < 0 || handle >= MAX_DMX_DEVICE_CHAINS)
		return -1;
	t = &thread_data[handle];
	pthread_mutex_lock(&mutex);
	t->time_to_quit = 1;
	init_per_thread_data(t, -1);
	t->thread_in_use = 0;
	/* No need to join thread we just told to quit since it was created in detached state. */
	pthread_mutex_unlock(&mutex);
	return 0;
}

static int send_dmx_data(int fd, uint8_t *data, int bytes_to_send)
{
	int rc, bytes_left = bytes_to_send;
	do {
		rc = write(fd, data + bytes_to_send - bytes_left, bytes_left);
		if (rc >= 0) {
			bytes_left -= rc;
		} else if (rc == -1 && errno == EINTR) {
			continue;
		} else {
			/* Some error on the file descriptor */
			return -1;
		}
	} while (bytes_left > 0);
	return 0;
}

static void *dmx_writer_thread(void *arg)
{
	struct per_thread_data *t = arg;
	uint8_t dmx_packet[sizeof(t->dmx_packet)];
	/* This entec start code was cribbed from EmptyEpsilon code */
	uint8_t entec_start_code[] = { 0x7E, 0x06, 512 & 0xff, 512 >> 8, 0x00 };
	uint8_t entec_end_code[] = { 0xE7 };

	do {
		/* Lock, copy and unlock, so that we don't block in write() while holding the lock */
		pthread_mutex_lock(&mutex);
		memcpy(dmx_packet, t->dmx_packet, sizeof(dmx_packet));
		if (t->time_to_quit) {
			pthread_mutex_unlock(&mutex);
			close(t->fd);
			return NULL;
		}
		pthread_mutex_unlock(&mutex);

		/* I suspect this part is almost certainly wrong in some way.
		 * Send break, then a single 1 bit, then the data.
		 * From the man page: "tcsendbreak() transmits a continuous stream of
		 * zero-valued bits for a specific duration, if the terminal is using
		 * asynchronous serial data transmission."
		 *
		 * So if we use tcsendbreak() to send the break, then follow it by
		 * 0xff for the MARK AFTER BREAK, then our data, then maybe that... should
		 * do it? Maybe?
		 */
		// tcsendbreak(t->fd, SERIAL_BREAK_LENGTH_MS); /* some small ms of break, this is not super portable */

		/* Write packet to device */
		send_dmx_data(t->fd, entec_start_code, sizeof(entec_start_code));
		send_dmx_data(t->fd, dmx_packet, sizeof(dmx_packet));
		send_dmx_data(t->fd, entec_end_code, sizeof(entec_end_code));
		thread_safe_msleep(SLEEP_MS_BETWEEN_PACKETS);
	} while (1);
}

static int setup_serial_port(int fd, char *device)
{
	struct termios2 options;
	int rc;

	/* Set baud rate to 250000. We have to do a bit of black magic for this
	 * somewhat non-standard baud rate using the little-known TCSETS2 ioctl.
	 * Per wikipedia: "The data format is fixed at one start bit, eight data bits
	 * (least significant first[7]), two stop bits and no parity.
	 *
	 * The "least significant bit first" part is no different than regular
	 * old RS232 serial, and the driver will handle all that.
	 */
	rc = ioctl(fd, TCGETS2, &options);
	if (rc) {
		fprintf(stderr, "snis_dmx: ioctl TCGETS2 failed: %s\n", strerror(errno));
		/* Just continue.  Lixada USB to dmx dongle gives "inappropriate ioctl for device." */
	}

	/* Set baud rate to 250000 (DMX_BAUD_RATE) */
	options.c_cflag &= ~CBAUD;
	options.c_cflag |= CBAUDEX; /* We're setting it to a non-standard baud rate */
	options.c_ispeed = DMX_BAUD_RATE;
	options.c_ospeed = DMX_BAUD_RATE;

	options.c_cflag &= ~PARENB; /* No parity */
	options.c_cflag |= CSTOPB; /* 2 stop bits */
	options.c_cflag &= ~CSIZE;
	options.c_cflag |= CS8; /* 8 data bits */

	options.c_cflag &= ~CRTSCTS; /* Disable hardware flow control */
	options.c_iflag &= ~(IXON | IXOFF | IXANY); /* Disable software flow control */

	/* Raw input, not line buffered -- not that we will do any input. */
	options.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
	/* Raw output -- don't mess with our data */
	options.c_oflag &= ~OPOST;

	rc = ioctl(fd, TCSETS2, &options);
	if (rc) {
		fprintf(stderr, "ioctl TCSETS2 failed: %s\n", strerror(errno));
		/* Just continue.  Lixada USB to dmx dongle gives "inappropriate ioctl for device." */
	}
	/* Check to see if our baud rate took */
	memset(&options, 0, sizeof(options));
	rc = ioctl(fd, TCGETS2, &options);
	if (rc) {
		fprintf(stderr, "ioctl TCGETS2 failed: %s\n", strerror(errno));
		/* Just continue.  Lixada USB to dmx dongle gives "inappropriate ioctl for device." */
	}
#if 0
	if (options.c_ispeed != DMX_BAUD_RATE) {
		fprintf(stderr,
			"snis_dmx: Failed to set input to %d baud on %s, currently set to %d baud.\n",
			DMX_BAUD_RATE, device, options.c_ispeed);
	}
	if (options.c_ospeed != DMX_BAUD_RATE) {
		fprintf(stderr,
			"snis_dmx: Failed to set output to %d baud on %s, currently set to %d baud.\n",
			DMX_BAUD_RATE, device, options.c_ospeed);
		rc = -1;
	}
#endif
	rc = 0;
	return rc;
}

/* Starts the main thread for transmitting DMX data to the device */
int snis_dmx_start_main_thread(char *device)
{
	pthread_attr_t attr;
	struct per_thread_data *t;
#ifdef _GNU_SOURCE
	char thread_name[16];
#endif
	int rc, fd, i;

	fprintf(stderr, "%s:%d: NOTICE!!! This snis_dmx library is COMPLETELY UNTESTED as\n",
			__FILE__,  __LINE__);
	fprintf(stderr, "%s:%d: I currently do not have any hardware I can test with.\n",
			__FILE__,  __LINE__);
	fprintf(stderr, "%s:%d: Do not be surprised if it utterly fails to do what you want.\n",
			__FILE__,  __LINE__);

	/* Find an unused slot for a new thread */
	pthread_mutex_lock(&mutex);
	for (i = 0; i < MAX_DMX_DEVICE_CHAINS; i++) {
		if (!thread_data[i].thread_in_use)
			break;
	}
	if (i == MAX_DMX_DEVICE_CHAINS) { /* No unused slots left */
		pthread_mutex_unlock(&mutex);
		return -1;
	}
	t = &thread_data[i]; /* i is our unused slot */
	t->thread_in_use = 1;
	pthread_mutex_unlock(&mutex);

	fd = open(device, O_RDWR | O_NOCTTY | O_NDELAY);
	if (fd < 0)
		return -1;

	init_per_thread_data(t, fd);
	rc = setup_serial_port(fd, device);
	if (rc)
		return rc;

	rc = pthread_attr_init(&attr);
	if (rc)
		return -1;
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
	rc = pthread_create(&t->thread, &attr, dmx_writer_thread, t);
	pthread_attr_destroy(&attr);
	if (rc)
		return -1;
#ifdef _GNU_SOURCE
	/* Set the thread name so it shows up in ps output reasonably */
	if (strlen(device) > 16 - 5) /* 16 is the limit for thread name length, including terminating null. */
		device = device + strlen(device) - 5; /* Use the more interesting part of the name */
	snprintf(thread_name, sizeof(thread_name), "dmx-%s", device);
	pthread_setname_np(t->thread, thread_name);
#endif

	return i;
}

/* Describes where in the DMX packet a given light is. This allows for having
 * lighting setups with lights that have different numbers of controls. For example,
 * If you had one light, with a 3 bytes to control RGB brightness, and a 2nd light with
 * 1 byte for brightness, and a 3rd light, with RGB, you would do:
 *
 * snis_dmx_add_light(fd, 3); -- add the first light, with 3 bytes for brightness
 * snis_dmx_add_light(fd, 1); -- add the 2nd light, with 1 byte for brightness
 * snis_dmx_add_light(fd, 3); -- add the 3rd light, with 3 byte for brightness
 *
 */
int snis_dmx_add_light(int handle, int size)
{
	struct per_thread_data *t;
	int i, n, offset = 0;

	if (handle < 0 || handle >= MAX_DMX_DEVICE_CHAINS)
		return -1;

	t = &thread_data[handle];
	pthread_mutex_lock(&mutex);
	if (!t->thread_in_use || t->nlights >= 512)
		goto error_condition;
	n = t->nlights;
	for (i = 0; i < t->nlights; i++)
		offset += t->light[i].size;
	if (offset + size > 512)
		goto error_condition;
	t->light[n].byte = offset;
	t->light[n].size = size;
	t->nlights++;
	pthread_mutex_unlock(&mutex);
	return n;

error_condition:
	pthread_mutex_unlock(&mutex);
	return -1;
}

/* Functions for setting light levels */
int snis_dmx_set_rgb(int handle, int number, uint8_t r, uint8_t g, uint8_t b)
{
	struct per_thread_data *t;

	int offset;
	printf("Setting RGB light level for f:%dn%d to %hhu, %hhu, %hhu\n",
		handle, number, r, g, b);
	if (number < 0 || handle < 0 || handle >= MAX_DMX_DEVICE_CHAINS)
		return -1;
	t = &thread_data[handle];
	pthread_mutex_lock(&mutex);
	if (!t->thread_in_use || t->fd < 0 || number >= t->nlights || t->light[number].size != 4)
		goto error;
	offset = t->light[number].byte;
	if (offset < 0 || offset >= sizeof(t->dmx_packet))
		goto error;
	t->dmx_packet[offset] = r;
	t->dmx_packet[offset + 1] = g;
	t->dmx_packet[offset + 2] = b;
	t->dmx_packet[offset + 3] = 255;
	pthread_mutex_unlock(&mutex);
	return 0;
error:
	pthread_mutex_unlock(&mutex);
	return -1;
}

int snis_dmx_set_u8_level(int handle, int number, uint8_t level)
{
	int offset;
	struct per_thread_data *t;

	printf("Setting u8 light level for f:%dn%d to %hhu\n",
		handle, number, level);
	if (number < 0 || handle < 0 || handle >= MAX_DMX_DEVICE_CHAINS)
		return -1;
	t = &thread_data[handle];
	pthread_mutex_lock(&mutex);
	if (!t->thread_in_use || t->fd < 0 || number >= t->nlights || t->light[number].size != 1)
		goto error;
	offset = t->light[number].byte;
	if (offset < 0 || offset >= sizeof(t->dmx_packet))
		goto error;
	t->dmx_packet[offset] = level;
	pthread_mutex_unlock(&mutex);
	return 0;
error:
	pthread_mutex_unlock(&mutex);
	return -1;
}

/* Set a 2 byte big endian 16-bit light level, level is CPU native format. */
int snis_dmx_set_be16_level(int handle, int number, uint16_t level)
{
	int offset;
	struct per_thread_data *t;
	const uint16_t cpu_value = 0x0102;
	const int cpu_is_little_endian = (((char *) &cpu_value)[0] == 0x02);
	unsigned char *levelbyte = (unsigned char *) &level;

	printf("Setting u16 light level for f:%dn%d to %hhu\n",
		handle, number, level);
	if (number < 0 || handle < 0 || handle >= MAX_DMX_DEVICE_CHAINS)
		return -1;
	t = &thread_data[handle];
	pthread_mutex_lock(&mutex);
	if (!t->thread_in_use || t->fd < 0 || number >= t->nlights || t->light[number].size != 2)
		goto error;
	offset = t->light[number].byte;
	if (offset < 0 || offset >= sizeof(t->dmx_packet))
		goto error;
	/* Convert native CPU endian u16 to big endian u16 */
	t->dmx_packet[offset] = levelbyte[cpu_is_little_endian];
	t->dmx_packet[offset + 1] = levelbyte[!cpu_is_little_endian];
	pthread_mutex_unlock(&mutex);
	return 0;
error:
	pthread_mutex_unlock(&mutex);
	return -1;
}
