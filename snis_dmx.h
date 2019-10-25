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

#include <stdint.h>

/* Closes the device and stops the associated thread */
int snis_dmx_close_device(int handle);

/* Starts the main thread for transmitting DMX data to the device */
int snis_dmx_start_main_thread(char *device);

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
int snis_dmx_add_light(int handle, int size);

/* Functions for setting light levels */
int snis_dmx_set_rgb(int handle, int number, uint8_t r, uint8_t g, uint8_t b);
int snis_dmx_set_u8_level(int handle, int number, uint8_t level);
int snis_dmx_set_be16_level(int handle, int number, uint16_t level);

