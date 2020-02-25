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
#include <stdlib.h>
#include <string.h>

#include "snis_bridge_update_packet.h"

#define PWDHASHLEN 34

struct packed_buffer *build_bridge_update_packet(struct snis_entity *o,
			struct persistent_bridge_data *bd, unsigned char *pwdhash)
{
	struct packed_buffer *pb;
	uint32_t fuel, oxygen;
	uint8_t tloading, tloaded, throttle, rpm;
	uint32_t iwallet = (int32_t) (o->tsd.ship.wallet * 100.0);

	pb = packed_buffer_allocate(64 + sizeof(struct update_ship_packet) +
					sizeof(struct power_model_data) +
					sizeof(struct power_model_data) +
					sizeof(struct persistent_bridge_data));
	if (!pb)
		return pb;
	packed_buffer_append(pb, "br", SNISMV_OPCODE_UPDATE_BRIDGE, pwdhash, (uint16_t) PWDHASHLEN);

	throttle = o->tsd.ship.throttle;
	rpm = o->tsd.ship.rpm;
	fuel = o->tsd.ship.fuel;
	oxygen = o->tsd.ship.fuel;

	tloading = (uint8_t) (o->tsd.ship.torpedoes_loading & 0x0f);
	tloaded = (uint8_t) (o->tsd.ship.torpedoes_loaded & 0x0f);
	tloading = tloading | (tloaded << 4);

	packed_buffer_append(pb, "hSSS", o->alive,
			o->x, (int32_t) UNIVERSE_DIM,
			o->y, (int32_t) UNIVERSE_DIM,
			o->z, (int32_t) UNIVERSE_DIM);
	packed_buffer_append(pb, "RRRwwRRbbbwwbbbbbbbbbbbbbwQQQbbwbbbb",
			o->tsd.ship.yaw_velocity,
			o->tsd.ship.pitch_velocity,
			o->tsd.ship.roll_velocity,
			o->tsd.ship.torpedoes, o->tsd.ship.power,
			o->tsd.ship.sci_heading,
			o->tsd.ship.sci_beam_width,
			tloading, throttle, rpm, fuel, oxygen, o->tsd.ship.temp,
			o->tsd.ship.scizoom, o->tsd.ship.weapzoom, o->tsd.ship.navzoom,
			o->tsd.ship.mainzoom,
			o->tsd.ship.warpdrive,
			o->tsd.ship.missile_count,
			o->tsd.ship.phaser_charge, o->tsd.ship.phaser_wavelength,
			o->tsd.ship.shiptype,
			o->tsd.ship.reverse, o->tsd.ship.trident,
			o->tsd.ship.comms_crypto_mode,
			o->tsd.ship.ai[0].u.attack.victim_id,
			&o->orientation.vec[0],
			&o->tsd.ship.sciball_orientation.vec[0],
			&o->tsd.ship.weap_orientation.vec[0],
			o->tsd.ship.in_secure_area,
			o->tsd.ship.docking_magnets,
			(uint32_t) iwallet,
			o->tsd.ship.warp_core_status,
			o->tsd.ship.exterior_lights,
			o->tsd.ship.alarms_silenced,
			o->tsd.ship.missile_lock_detected);
	packed_buffer_append(pb, "bbbbbr",
		o->sdata.shield_strength, o->sdata.shield_wavelength, o->sdata.shield_width, o->sdata.shield_depth,
		o->sdata.faction, o->sdata.name, (unsigned short) sizeof(o->sdata.name));
	packed_buffer_append(pb, "r", &o->tsd.ship.power_data, (uint16_t) sizeof(o->tsd.ship.power_data));
	packed_buffer_append(pb, "r", &o->tsd.ship.coolant_data, (uint16_t) sizeof(o->tsd.ship.power_data));
	packed_buffer_append(pb, "r", bd, (uint16_t) sizeof(*bd));
	return pb;
}

void unpack_bridge_update_packet(struct snis_entity *o, struct persistent_bridge_data *bd, struct packed_buffer *pb)
{
	uint16_t alive;
	uint32_t torpedoes, power;
	uint32_t fuel, oxygen, victim_id;
	double dx, dy, dz, dyawvel, dpitchvel, drollvel;
	double dsheading, dbeamwidth;
	uint8_t tloading, tloaded, throttle, rpm, temp, scizoom, weapzoom, navzoom,
		mainzoom, warpdrive,
		missile_count, phaser_charge, phaser_wavelength, shiptype,
		reverse, trident, in_secure_area, docking_magnets, shield_strength,
		shield_wavelength, shield_width, shield_depth, faction;
	union quat orientation, sciball_orientation, weap_orientation;
	union euler ypr;
	unsigned char name[sizeof(o->sdata.name)];
	int32_t iwallet;
	uint8_t warp_core_status, exterior_lights, alarms_silenced, missile_lock_detected;
	uint8_t comms_crypto_mode;
	struct power_model_data power_data, coolant_data;

	packed_buffer_extract(pb, "hSSS", &alive,
				&dx, (int32_t) UNIVERSE_DIM, &dy, (int32_t) UNIVERSE_DIM,
				&dz, (int32_t) UNIVERSE_DIM);
	packed_buffer_extract(pb, "RRRwwRR",
				&dyawvel,
				&dpitchvel,
				&drollvel,
				&torpedoes, &power,
				&dsheading,
				&dbeamwidth);
	packed_buffer_extract(pb, "bbbwwbbbbbbbbbbbbbwQQQbbwbbbb",
			&tloading, &throttle, &rpm, &fuel, &oxygen, &temp,
			&scizoom, &weapzoom, &navzoom, &mainzoom,
			&warpdrive,
			&missile_count, &phaser_charge, &phaser_wavelength, &shiptype,
			&reverse, &trident, &comms_crypto_mode, &victim_id, &orientation.vec[0],
			&sciball_orientation.vec[0], &weap_orientation.vec[0], &in_secure_area,
			&docking_magnets, (uint32_t *) &iwallet, &warp_core_status, &exterior_lights,
			&alarms_silenced, &missile_lock_detected);
	packed_buffer_extract(pb, "bbbbbr", &shield_strength, &shield_wavelength, &shield_width, &shield_depth,
			&faction, name, (uint16_t) sizeof(name));
	packed_buffer_extract(pb, "r", &power_data, (uint16_t) sizeof(struct power_model_data));
	packed_buffer_extract(pb, "r", &coolant_data, (int) sizeof(struct power_model_data));
	packed_buffer_extract(pb, "r", bd, (int) sizeof(*bd));
	tloaded = (tloading >> 4) & 0x0f;
	tloading = tloading & 0x0f;
	quat_to_euler(&ypr, &orientation);

	if (!o->tsd.ship.damcon) {
		o->tsd.ship.damcon = malloc(sizeof(*o->tsd.ship.damcon));
		memset(o->tsd.ship.damcon, 0, sizeof(*o->tsd.ship.damcon));
	}
#if 0
	if (!o->tsd.ship.power_model) {
		o->tsd.ship.power_model = malloc(sizeof(*o->tsd.ship.power_model));
		memset(o->tsd.ship.power_model, 0, sizeof(*o->tsd.ship.power_model));
	}
	if (!o->tsd.ship.coolant_model) {
		o->tsd.ship.coolant_model = malloc(sizeof(*o->tsd.ship.coolant_model));
		memset(o->tsd.ship.coolant_model, 0, sizeof(*o->tsd.ship.coolant_model));
	}
#endif
	o->x = dx;
	o->y = dy;
	o->z = dz;
	o->vx = 0;
	o->vy = 0;
	o->vz = 0;
	o->orientation = orientation;
	o->alive = alive;
	o->tsd.ship.yaw_velocity = dyawvel;
	o->tsd.ship.pitch_velocity = dpitchvel;
	o->tsd.ship.roll_velocity = drollvel;
	o->tsd.ship.torpedoes = torpedoes;
	o->tsd.ship.power = power;
	o->tsd.ship.sci_heading = dsheading;
	o->tsd.ship.sci_beam_width = dbeamwidth;
	o->tsd.ship.torpedoes_loaded = tloaded;
	o->tsd.ship.torpedoes_loading = tloading;
	o->tsd.ship.throttle = throttle;
	o->tsd.ship.rpm = rpm;
	o->tsd.ship.fuel = fuel;
	o->tsd.ship.oxygen = oxygen;
	o->tsd.ship.temp = temp;
	o->tsd.ship.scizoom = scizoom;
	o->tsd.ship.weapzoom = weapzoom;
	o->tsd.ship.navzoom = navzoom;
	o->tsd.ship.mainzoom = mainzoom;
	o->tsd.ship.warpdrive = warpdrive;
	o->tsd.ship.phaser_charge = phaser_charge;
	o->tsd.ship.phaser_wavelength = phaser_wavelength;
	o->tsd.ship.damcon = NULL;
	o->tsd.ship.shiptype = shiptype;
	o->tsd.ship.in_secure_area = in_secure_area;
	o->tsd.ship.docking_magnets = docking_magnets;
	o->tsd.ship.wallet = (float) iwallet / 100.0f;
	o->tsd.ship.warp_core_status = warp_core_status;
	o->sdata.shield_strength = shield_strength;
	o->sdata.shield_wavelength = shield_wavelength;
	o->sdata.shield_width = shield_width;
	o->sdata.shield_depth = shield_depth;
	o->sdata.faction = faction;
	memcpy(o->sdata.name, name, sizeof(o->sdata.name));
	o->tsd.ship.power_data = power_data;
	o->tsd.ship.coolant_data = coolant_data;

	/* shift old updates to make room for this one */
	int j;
	for (j = SNIS_ENTITY_NUPDATE_HISTORY - 1; j >= 1; j--) {
		o->tsd.ship.sciball_o[j] = o->tsd.ship.sciball_o[j-1];
		o->tsd.ship.weap_o[j] = o->tsd.ship.weap_o[j-1];
	}
	o->tsd.ship.sciball_o[0] = sciball_orientation;
	o->tsd.ship.sciball_orientation = sciball_orientation;
	o->tsd.ship.weap_o[0] = weap_orientation;
	o->tsd.ship.weap_orientation = weap_orientation;
	o->tsd.ship.reverse = reverse;
	o->tsd.ship.trident = trident;
	o->tsd.ship.exterior_lights = exterior_lights;
	o->tsd.ship.alarms_silenced = alarms_silenced;
	o->tsd.ship.missile_lock_detected = missile_lock_detected;
	o->tsd.ship.comms_crypto_mode = comms_crypto_mode;
}


