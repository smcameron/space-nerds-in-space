/*
	Copyright (C) 2026 Stephen M. Cameron
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
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#include "snis_wled.h"
#include "snis_packet.h"

static int wled_socket = -1;
static struct sockaddr_in wled_dest_addr;
static int wled_initialized = 0;

static uint8_t wled_base_mode = WLED_CMD_OFF;
static uint8_t wled_current_mode = WLED_CMD_OFF;
static uint8_t wled_base_r = 0;
static uint8_t wled_base_g = 0;
static uint8_t wled_base_b = 0;
static uint8_t wled_flash_r = 0;
static uint8_t wled_flash_g = 0;
static uint8_t wled_flash_b = 0;
static int wled_flash_ticks_remaining = 0;
static int wled_refresh_counter = 0;
static int wled_off_burst_count = 0;

#define WLED_PROTOCOL_DRGB 2
#define WLED_TIMEOUT_SECONDS 2
#define WLED_REFRESH_INTERVAL 15

static void send_wled_drgb_packet(uint8_t r, uint8_t g, uint8_t b)
{
	uint8_t packet[2 + WLED_MAX_LEDS * 3];
	int i;
	ssize_t sent;

	if (wled_socket < 0 || !wled_initialized)
		return;

	packet[0] = WLED_PROTOCOL_DRGB;
	packet[1] = WLED_TIMEOUT_SECONDS;

	for (i = 0; i < WLED_MAX_LEDS; i++) {
		packet[2 + i * 3] = r;
		packet[2 + i * 3 + 1] = g;
		packet[2 + i * 3 + 2] = b;
	}

	sent = sendto(wled_socket, packet, sizeof(packet), 0,
		(struct sockaddr *) &wled_dest_addr, sizeof(wled_dest_addr));
	if (sent < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
		fprintf(stderr, "snis_wled: sendto failed: %s\n", strerror(errno));
	}
}

int snis_wled_init(const char *host, int port)
{
	int flags, opt = 1;
	struct hostent *he;

	if (wled_socket >= 0)
		close(wled_socket);
	wled_socket = -1;
	wled_initialized = 0;

	if (!host || host[0] == '\0')
		host = "127.0.0.1";
	if (port <= 0)
		port = WLED_DEFAULT_PORT;

	wled_socket = socket(AF_INET, SOCK_DGRAM, 0);
	if (wled_socket < 0) {
		fprintf(stderr, "snis_wled: socket creation failed: %s\n", strerror(errno));
		return -1;
	}

	if (setsockopt(wled_socket, SOL_SOCKET, SO_BROADCAST, &opt, sizeof(opt)) < 0)
		fprintf(stderr, "snis_wled: warning: setsockopt(SO_BROADCAST) failed: %s\n", strerror(errno));

	flags = fcntl(wled_socket, F_GETFL, 0);
	if (flags >= 0)
		fcntl(wled_socket, F_SETFL, flags | O_NONBLOCK);

	memset(&wled_dest_addr, 0, sizeof(wled_dest_addr));
	wled_dest_addr.sin_family = AF_INET;
	wled_dest_addr.sin_port = htons(port);

	if (inet_pton(AF_INET, host, &wled_dest_addr.sin_addr) <= 0) {
		he = gethostbyname(host);
		if (!he) {
			fprintf(stderr, "snis_wled: unable to resolve host %s\n", host);
			close(wled_socket);
			wled_socket = -1;
			return -1;
		}
		memcpy(&wled_dest_addr.sin_addr, he->h_addr_list[0], he->h_length);
	}

	wled_initialized = 1;
	wled_current_mode = WLED_CMD_OFF;
	wled_base_mode = WLED_CMD_OFF;
	wled_off_burst_count = 0;
	wled_refresh_counter = 0;

	/* Send an initial off packet to clear state */
	send_wled_drgb_packet(0, 0, 0);
	return 0;
}

void snis_wled_shutdown(void)
{
	if (wled_socket >= 0) {
		send_wled_drgb_packet(0, 0, 0);
		close(wled_socket);
		wled_socket = -1;
	}
	wled_initialized = 0;
}

void snis_wled_set_color(uint8_t r, uint8_t g, uint8_t b)
{
	wled_base_mode = WLED_CMD_SOLID;
	wled_base_r = r;
	wled_base_g = g;
	wled_base_b = b;

	if (wled_current_mode != WLED_CMD_FLASH) {
		wled_current_mode = WLED_CMD_SOLID;
		send_wled_drgb_packet(r, g, b);
		wled_refresh_counter = 0;
	}
}

void snis_wled_flash(uint8_t r, uint8_t g, uint8_t b, uint16_t duration_ms)
{
	wled_flash_r = r;
	wled_flash_g = g;
	wled_flash_b = b;

	/* Advance game runs at ~30 ticks per second */
	wled_flash_ticks_remaining = ((int) duration_ms * 30) / 1000;
	if (wled_flash_ticks_remaining < 1)
		wled_flash_ticks_remaining = 1;

	wled_current_mode = WLED_CMD_FLASH;
	send_wled_drgb_packet(r, g, b);
}

void snis_wled_off(void)
{
	wled_base_mode = WLED_CMD_OFF;
	wled_current_mode = WLED_CMD_OFF;
	wled_flash_ticks_remaining = 0;
	wled_off_burst_count = 0;
	send_wled_drgb_packet(0, 0, 0);
}

void snis_wled_handle_command(uint8_t command, uint8_t r, uint8_t g, uint8_t b, uint16_t duration_ms)
{
	switch (command) {
	case WLED_CMD_OFF:
		snis_wled_off();
		break;
	case WLED_CMD_SOLID:
		snis_wled_set_color(r, g, b);
		break;
	case WLED_CMD_FLASH:
		snis_wled_flash(r, g, b, duration_ms);
		break;
	default:
		break;
	}
}

void snis_wled_tick(void)
{
	if (!wled_initialized || wled_socket < 0)
		return;

	if (wled_current_mode == WLED_CMD_FLASH) {
		wled_flash_ticks_remaining--;
		if (wled_flash_ticks_remaining <= 0) {
			wled_current_mode = wled_base_mode;
			if (wled_base_mode == WLED_CMD_SOLID) {
				send_wled_drgb_packet(wled_base_r, wled_base_g, wled_base_b);
			} else {
				send_wled_drgb_packet(0, 0, 0);
				wled_off_burst_count = 0;
			}
		}
		return;
	}

	if (wled_current_mode == WLED_CMD_SOLID) {
		wled_refresh_counter++;
		if (wled_refresh_counter >= WLED_REFRESH_INTERVAL) {
			wled_refresh_counter = 0;
			send_wled_drgb_packet(wled_base_r, wled_base_g, wled_base_b);
		}
		return;
	}

	if (wled_current_mode == WLED_CMD_OFF) {
		if (wled_off_burst_count < 3) {
			wled_off_burst_count++;
			send_wled_drgb_packet(0, 0, 0);
		}
	}
}
