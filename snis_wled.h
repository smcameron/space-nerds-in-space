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

#ifndef __SNIS_WLED_H__
#define __SNIS_WLED_H__

#include <stdint.h>

#define WLED_DEFAULT_PORT 21324
#define WLED_MAX_LEDS 490

int snis_wled_init(const char *host, int port);
void snis_wled_shutdown(void);
void snis_wled_handle_command(uint8_t command, uint8_t r, uint8_t g, uint8_t b, uint16_t duration_ms);
void snis_wled_set_color(uint8_t r, uint8_t g, uint8_t b);
void snis_wled_flash(uint8_t r, uint8_t g, uint8_t b, uint16_t duration_ms);
void snis_wled_off(void);
void snis_wled_tick(void);

#endif
