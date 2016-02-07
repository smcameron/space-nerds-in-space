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

#include "snis_bridge_update_packet.h"

struct packed_buffer *build_bridge_update_packet(struct snis_entity *o, unsigned char *pwdhash)
{
	struct packed_buffer *pb;
	uint32_t fuel;
	uint8_t tloading, tloaded, throttle, rpm;
	uint32_t iwallet = (int32_t) (o->tsd.ship.wallet * 100.0);

	pb = packed_buffer_allocate(50 + sizeof(struct update_ship_packet) +
					sizeof(struct power_model_data) +
					sizeof(struct power_model_data));
	if (!pb)
		return pb;
	packed_buffer_append(pb, "br", SNISMV_OPCODE_UPDATE_BRIDGE, pwdhash, (uint16_t) 20);

	throttle = o->tsd.ship.throttle;
	rpm = o->tsd.ship.rpm;
	fuel = o->tsd.ship.fuel;

	tloading = (uint8_t) (o->tsd.ship.torpedoes_loading & 0x0f);
	tloaded = (uint8_t) (o->tsd.ship.torpedoes_loaded & 0x0f);
	tloading = tloading | (tloaded << 4);

	packed_buffer_append(pb, "hSSS", o->alive,
			o->x, (int32_t) UNIVERSE_DIM,
			o->y, (int32_t) UNIVERSE_DIM,
			o->z, (int32_t) UNIVERSE_DIM);
	packed_buffer_append(pb, "RRRwwRRRbbbwbbbbbbbbbbbbbwQQQbbw",
			o->tsd.ship.yaw_velocity,
			o->tsd.ship.pitch_velocity,
			o->tsd.ship.roll_velocity,
			o->tsd.ship.torpedoes, o->tsd.ship.power,
			o->tsd.ship.gun_yaw_velocity,
			o->tsd.ship.sci_heading,
			o->tsd.ship.sci_beam_width,
			tloading, throttle, rpm, fuel, o->tsd.ship.temp,
			o->tsd.ship.scizoom, o->tsd.ship.weapzoom, o->tsd.ship.navzoom,
			o->tsd.ship.mainzoom,
			o->tsd.ship.warpdrive, o->tsd.ship.requested_warpdrive,
			o->tsd.ship.requested_shield, o->tsd.ship.phaser_charge,
			o->tsd.ship.phaser_wavelength, o->tsd.ship.shiptype,
			o->tsd.ship.reverse, o->tsd.ship.trident,
			o->tsd.ship.ai[0].u.attack.victim_id,
			&o->orientation.vec[0],
			&o->tsd.ship.sciball_orientation.vec[0],
			&o->tsd.ship.weap_orientation.vec[0],
			o->tsd.ship.in_secure_area,
			o->tsd.ship.docking_magnets,
			(uint32_t) iwallet);
	packed_buffer_append(pb, "bbbbbr",
		o->sdata.shield_strength, o->sdata.shield_wavelength, o->sdata.shield_width, o->sdata.shield_depth,
		o->sdata.faction, o->sdata.name, (unsigned short) sizeof(o->sdata.name));
	packed_buffer_append(pb, "r", &o->tsd.ship.power_data, (uint16_t) sizeof(o->tsd.ship.power_data));
	packed_buffer_append(pb, "r", &o->tsd.ship.coolant_data, (uint16_t) sizeof(o->tsd.ship.power_data));
	return pb;
}

