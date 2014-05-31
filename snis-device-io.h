#ifndef __SNIS_DEVICE_IO_H
#define __SNIS_DEVICE_IO_H
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

/* everything you can do on the engineering screen */
#define DEVIO_OPCODE_ENG_PWR_SHIELDS		0
#define DEVIO_OPCODE_ENG_PWR_PHASERS		1
#define DEVIO_OPCODE_ENG_PWR_COMMS		2
#define DEVIO_OPCODE_ENG_PWR_SENSORS		3
#define DEVIO_OPCODE_ENG_PWR_IMPULSE		4
#define DEVIO_OPCODE_ENG_PWR_WARP		5
#define DEVIO_OPCODE_ENG_PWR_MANEUVERING	6
#define DEVIO_OPCODE_ENG_PWR_TRACTOR		7
#define DEVIO_OPCODE_ENG_COOL_SHIELDS		8
#define DEVIO_OPCODE_ENG_COOL_PHASERS		9
#define DEVIO_OPCODE_ENG_COOL_COMMS		10
#define DEVIO_OPCODE_ENG_COOL_SENSORS		11
#define DEVIO_OPCODE_ENG_COOL_IMPULSE		12
#define DEVIO_OPCODE_ENG_COOL_WARP		13
#define DEVIO_OPCODE_ENG_COOL_MANEUVERING	14
#define DEVIO_OPCODE_ENG_COOL_TRACTOR		15
#define DEVIO_OPCODE_ENG_SHIELD_LEVEL		16
#define DEVIO_OPCODE_ENG_PRESET1_BUTTON		17
#define DEVIO_OPCODE_ENG_PRESET2_BUTTON		18
#define DEVIO_OPCODE_ENG_DAMAGE_CTRL		19

struct snis_device_io_connection;
int snis_device_io_setup(struct snis_device_io_connection **c);
int snis_device_io_send(struct snis_device_io_connection *c,
			unsigned short opcode, unsigned short value);
void snis_device_io_shutdown(struct snis_device_io_connection *c);

#endif
