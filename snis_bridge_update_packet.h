#ifndef __SNIS_BRIDGE_UPDATE_PACKET_H
#define __SNIS_BRIDGE_UPDATE_PACKET_H
/*
	Copyright (C) 2016 Stephen M. Cameron
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

#include "quat.h"
#define SNIS_SERVER_DATA 1
#include "snis.h"
#include "snis_packet.h"
#include "snis_marshal.h"
#include "snis_multiverse.h"
#include "commodities.h"

#define UPDATE_BRIDGE_PACKET_SIZE 416

/* struct persistent_bridge_data contains per bridge data that snis_multiverse needs to
 * save/restore but which is not present in struct snis_entity.
 */

struct persistent_bridge_data {
	/* 6 sets of engineering presets for 9 systems each with power and coolant */
	uint8_t engineering_preset[ENG_PRESET_NUMBER][18];
};

struct packed_buffer *build_bridge_update_packet(struct snis_entity *o,
				struct persistent_bridge_data *bd, unsigned char *pwdhash);
struct packed_buffer *build_cargo_update_packet(struct snis_entity *o, unsigned char *pwdhash,
				struct commodity c[]);
struct packed_buffer *build_passenger_update_packet(unsigned char *pwdhash,
		struct flattened_passenger fp[], int passengers_aboard);

void unpack_bridge_update_packet(struct snis_entity *o, struct persistent_bridge_data *bd, struct packed_buffer *pb);

#endif
