#ifndef SNIS_ENTITY_KEY_VALUE_SPECIFICATION_H__
#define SNIS_ENTITY_KEY_VALUE_SPECIFICATION_H__

#include <stddef.h>

/* Macros for constructing key value specifications for snis_entity, and snis_entity.tsd.ship */
#define DOUBLE_FIELD(x) { #x, KVS_DOUBLE, 0, offsetof(struct snis_entity, x), sizeof(double), }
#define FLOAT_FIELD(x) { #x, KVS_FLOAT, 0, offsetof(struct snis_entity, x), sizeof(float), }
#define UINT32_FIELD(x) { #x, KVS_UINT32, 0, offsetof(struct snis_entity, x), sizeof(uint32_t), }
#define UINT16_FIELD(x) { #x, KVS_UINT16, 0, offsetof(struct snis_entity, x), sizeof(uint16_t), }
#define UINT8_FIELD(x) { #x, KVS_INT8, 0, offsetof(struct snis_entity, x), sizeof(int8_t), }
#define INT32_FIELD(x) { #x, KVS_INT32, 0, offsetof(struct snis_entity, x), sizeof(int32_t), }
#define INT16_FIELD(x) { #x, KVS_INT16, 0, offsetof(struct snis_entity, x), sizeof(int16_t), }
#define INT8_FIELD(x) { #x, KVS_INT8, 0, offsetof(struct snis_entity, x), sizeof(int8_t), }
#define STRING_FIELD(x) { #x, KVS_STRING, 0, offsetof(struct snis_entity, x), \
	sizeof(((struct snis_entity *) 0)->x), }

#define FLOAT_TSDFIELD(x) { #x, KVS_FLOAT, 0, offsetof(struct snis_entity, tsd) + \
				offsetof(struct ship_data, x), sizeof(float), }
#define DOUBLE_TSDFIELD(x) { #x, KVS_DOUBLE, 0, offsetof(struct snis_entity, tsd) + \
				offsetof(struct ship_data, x), sizeof(double), }
#define UINT32_TSDFIELD(x) { #x, KVS_UINT32, 0, offsetof(struct snis_entity, tsd) + \
				offsetof(struct ship_data, x), sizeof(uint32_t), }
#define UINT16_TSDFIELD(x) { #x, KVS_UINT16, 0, offsetof(struct snis_entity, tsd) + \
				offsetof(struct ship_data, x), sizeof(uint16_t), }
#define UINT8_TSDFIELD(x) { #x, KVS_UINT8, 0, offsetof(struct snis_entity, tsd) + \
				offsetof(struct ship_data, x), sizeof(uint8_t), }
#define INT32_TSDFIELD(x) { #x, KVS_INT32, 0, offsetof(struct snis_entity, tsd) + \
				offsetof(struct ship_data, x), sizeof(int32_t), }
#define INT16_TSDFIELD(x) { #x, KVS_INT16, 0, offsetof(struct snis_entity, tsd) + \
				offsetof(struct ship_data, x), sizeof(int16_t), }
#define INT8_TSDFIELD(x) { #x, KVS_INT8, 0, offsetof(struct snis_entity, tsd) + \
				offsetof(struct ship_data, x), sizeof(int8_t), }
#define STRING_TSDFIELD(x) { #x, KVS_STRING, 0, offsetof(struct snis_entity, tsd) + \
				offsetof(struct ship_data, x), sizeof(((struct snis_entity *) 0)->tsd.ship.x) }
#ifdef INCLUDE_BRIDGE_INFO_FIELDS
#define INT32_BRIDGE_FIELD(x) { #x, KVS_INT32, 1, offsetof(struct bridge_info, x), sizeof(int32_t), }
#define UINT8_BRIDGE_FIELD(x) { #x, KVS_INT8, 1, offsetof(struct bridge_info, x), sizeof(uint8_t), }
#endif

struct key_value_specification snis_entity_kvs[] = {
	DOUBLE_FIELD(x),
	DOUBLE_FIELD(y),
	DOUBLE_FIELD(z),
	DOUBLE_FIELD(vx),
	DOUBLE_FIELD(vy),
	DOUBLE_FIELD(vz),
	DOUBLE_FIELD(heading),
	UINT16_FIELD(alive),
	UINT32_FIELD(type),
	UINT32_TSDFIELD(torpedoes),
	DOUBLE_TSDFIELD(velocity),
	DOUBLE_TSDFIELD(yaw_velocity),
	DOUBLE_TSDFIELD(pitch_velocity),
	DOUBLE_TSDFIELD(roll_velocity),
	DOUBLE_TSDFIELD(desired_velocity),
	DOUBLE_TSDFIELD(sci_heading),
	DOUBLE_TSDFIELD(sci_beam_width),
	DOUBLE_TSDFIELD(sci_yaw_velocity),
	FLOAT_TSDFIELD(sciball_orientation.vec[0]),
	FLOAT_TSDFIELD(sciball_orientation.vec[1]),
	FLOAT_TSDFIELD(sciball_orientation.vec[2]),
	FLOAT_TSDFIELD(sciball_orientation.vec[3]),
	DOUBLE_TSDFIELD(sciball_yawvel),
	DOUBLE_TSDFIELD(sciball_pitchvel),
	DOUBLE_TSDFIELD(sciball_rollvel),
	FLOAT_TSDFIELD(weap_orientation.vec[0]),
	FLOAT_TSDFIELD(weap_orientation.vec[1]),
	FLOAT_TSDFIELD(weap_orientation.vec[2]),
	FLOAT_TSDFIELD(weap_orientation.vec[3]),
	DOUBLE_TSDFIELD(weap_yawvel),
	DOUBLE_TSDFIELD(weap_pitchvel),
	UINT8_TSDFIELD(torpedoes_loaded),
	UINT8_TSDFIELD(torpedoes_loading),
	UINT16_TSDFIELD(torpedo_load_time),
	UINT8_TSDFIELD(phaser_bank_charge),
	UINT32_TSDFIELD(fuel),
	UINT32_TSDFIELD(oxygen),
	UINT8_TSDFIELD(rpm),
	UINT8_TSDFIELD(throttle),
	UINT8_TSDFIELD(temp),
	UINT8_TSDFIELD(shiptype),
	UINT8_TSDFIELD(scizoom),
	UINT8_TSDFIELD(weapzoom),
	UINT8_TSDFIELD(mainzoom),
	UINT8_TSDFIELD(phaser_wavelength),
	UINT8_TSDFIELD(phaser_charge),
	UINT8_TSDFIELD(damage.shield_damage),
	UINT8_TSDFIELD(damage.impulse_damage),
	UINT8_TSDFIELD(damage.warp_damage),
	UINT8_TSDFIELD(damage.maneuvering_damage),
	UINT8_TSDFIELD(damage.phaser_banks_damage),
	UINT8_TSDFIELD(damage.sensors_damage),
	UINT8_TSDFIELD(damage.comms_damage),
	UINT8_TSDFIELD(damage.tractor_damage),
	UINT8_TSDFIELD(damage.lifesupport_damage),
	/* TODO damcon data... */
	UINT8_TSDFIELD(view_mode),
	DOUBLE_TSDFIELD(view_angle),

	/* Beginning of power_data */
	UINT8_TSDFIELD(power_data.maneuvering.r1),
	UINT8_TSDFIELD(power_data.maneuvering.r2),
	UINT8_TSDFIELD(power_data.maneuvering.r3),
	UINT8_TSDFIELD(power_data.maneuvering.i),

	UINT8_TSDFIELD(power_data.warp.r1),
	UINT8_TSDFIELD(power_data.warp.r2),
	UINT8_TSDFIELD(power_data.warp.r3),
	UINT8_TSDFIELD(power_data.warp.i),

	UINT8_TSDFIELD(power_data.impulse.r1),
	UINT8_TSDFIELD(power_data.impulse.r2),
	UINT8_TSDFIELD(power_data.impulse.r3),
	UINT8_TSDFIELD(power_data.impulse.i),

	UINT8_TSDFIELD(power_data.sensors.r1),
	UINT8_TSDFIELD(power_data.sensors.r2),
	UINT8_TSDFIELD(power_data.sensors.r3),
	UINT8_TSDFIELD(power_data.sensors.i),

	UINT8_TSDFIELD(power_data.comms.r1),
	UINT8_TSDFIELD(power_data.comms.r2),
	UINT8_TSDFIELD(power_data.comms.r3),
	UINT8_TSDFIELD(power_data.comms.i),

	UINT8_TSDFIELD(power_data.phasers.r1),
	UINT8_TSDFIELD(power_data.phasers.r2),
	UINT8_TSDFIELD(power_data.phasers.r3),
	UINT8_TSDFIELD(power_data.phasers.i),

	UINT8_TSDFIELD(power_data.shields.r1),
	UINT8_TSDFIELD(power_data.shields.r2),
	UINT8_TSDFIELD(power_data.shields.r3),
	UINT8_TSDFIELD(power_data.shields.i),

	UINT8_TSDFIELD(power_data.tractor.r1),
	UINT8_TSDFIELD(power_data.tractor.r2),
	UINT8_TSDFIELD(power_data.tractor.r3),
	UINT8_TSDFIELD(power_data.tractor.i),

	UINT8_TSDFIELD(power_data.lifesupport.r1),
	UINT8_TSDFIELD(power_data.lifesupport.r2),
	UINT8_TSDFIELD(power_data.lifesupport.r3),
	UINT8_TSDFIELD(power_data.lifesupport.i),

	UINT8_TSDFIELD(power_data.voltage),
	/* End of power_data */

	/* Beginning of coolant_data */
	UINT8_TSDFIELD(coolant_data.maneuvering.r1),
	UINT8_TSDFIELD(coolant_data.maneuvering.r2),
	UINT8_TSDFIELD(coolant_data.maneuvering.r3),
	UINT8_TSDFIELD(coolant_data.maneuvering.i),

	UINT8_TSDFIELD(coolant_data.warp.r1),
	UINT8_TSDFIELD(coolant_data.warp.r2),
	UINT8_TSDFIELD(coolant_data.warp.r3),
	UINT8_TSDFIELD(coolant_data.warp.i),

	UINT8_TSDFIELD(coolant_data.impulse.r1),
	UINT8_TSDFIELD(coolant_data.impulse.r2),
	UINT8_TSDFIELD(coolant_data.impulse.r3),
	UINT8_TSDFIELD(coolant_data.impulse.i),

	UINT8_TSDFIELD(coolant_data.sensors.r1),
	UINT8_TSDFIELD(coolant_data.sensors.r2),
	UINT8_TSDFIELD(coolant_data.sensors.r3),
	UINT8_TSDFIELD(coolant_data.sensors.i),

	UINT8_TSDFIELD(coolant_data.comms.r1),
	UINT8_TSDFIELD(coolant_data.comms.r2),
	UINT8_TSDFIELD(coolant_data.comms.r3),
	UINT8_TSDFIELD(coolant_data.comms.i),

	UINT8_TSDFIELD(coolant_data.phasers.r1),
	UINT8_TSDFIELD(coolant_data.phasers.r2),
	UINT8_TSDFIELD(coolant_data.phasers.r3),
	UINT8_TSDFIELD(coolant_data.phasers.i),

	UINT8_TSDFIELD(coolant_data.shields.r1),
	UINT8_TSDFIELD(coolant_data.shields.r2),
	UINT8_TSDFIELD(coolant_data.shields.r3),
	UINT8_TSDFIELD(coolant_data.shields.i),

	UINT8_TSDFIELD(coolant_data.tractor.r1),
	UINT8_TSDFIELD(coolant_data.tractor.r2),
	UINT8_TSDFIELD(coolant_data.tractor.r3),
	UINT8_TSDFIELD(coolant_data.tractor.i),

	UINT8_TSDFIELD(coolant_data.lifesupport.r1),
	UINT8_TSDFIELD(coolant_data.lifesupport.r2),
	UINT8_TSDFIELD(coolant_data.lifesupport.r3),
	UINT8_TSDFIELD(coolant_data.lifesupport.i),

	UINT8_TSDFIELD(coolant_data.voltage),
	/* End of coolant_data */

	UINT8_TSDFIELD(temperature_data.shield_damage),
	UINT8_TSDFIELD(temperature_data.impulse_damage),
	UINT8_TSDFIELD(temperature_data.warp_damage),
	UINT8_TSDFIELD(temperature_data.maneuvering_damage),
	UINT8_TSDFIELD(temperature_data.phaser_banks_damage),
	UINT8_TSDFIELD(temperature_data.sensors_damage),
	UINT8_TSDFIELD(temperature_data.comms_damage),
	UINT8_TSDFIELD(temperature_data.tractor_damage),
	UINT8_TSDFIELD(temperature_data.lifesupport_damage),
	INT32_TSDFIELD(warp_time),
	DOUBLE_TSDFIELD(scibeam_a1),
	DOUBLE_TSDFIELD(scibeam_a2),
	DOUBLE_TSDFIELD(scibeam_range),
	UINT8_TSDFIELD(reverse),
	UINT8_TSDFIELD(trident),
	INT32_TSDFIELD(next_torpedo_time),
	INT32_TSDFIELD(next_laser_time),
	UINT8_TSDFIELD(lifeform_count),
	UINT32_TSDFIELD(tractor_beam),
	UINT8_TSDFIELD(damage_data_dirty),
	FLOAT_TSDFIELD(steering_adjustment.vec[0]),
	FLOAT_TSDFIELD(steering_adjustment.vec[1]),
	FLOAT_TSDFIELD(steering_adjustment.vec[2]),
	FLOAT_TSDFIELD(braking_factor),

	FLOAT_TSDFIELD(cargo[0].paid),
	INT32_TSDFIELD(cargo[0].contents.item),
	FLOAT_TSDFIELD(cargo[0].contents.qty),
	UINT32_TSDFIELD(cargo[0].origin),
	UINT32_TSDFIELD(cargo[0].dest),
	INT32_TSDFIELD(cargo[0].due_date),

	FLOAT_TSDFIELD(cargo[1].paid),
	INT32_TSDFIELD(cargo[1].contents.item),
	FLOAT_TSDFIELD(cargo[1].contents.qty),
	UINT32_TSDFIELD(cargo[1].origin),
	UINT32_TSDFIELD(cargo[1].dest),
	INT32_TSDFIELD(cargo[1].due_date),

	FLOAT_TSDFIELD(cargo[2].paid),
	INT32_TSDFIELD(cargo[2].contents.item),
	FLOAT_TSDFIELD(cargo[2].contents.qty),
	UINT32_TSDFIELD(cargo[2].origin),
	UINT32_TSDFIELD(cargo[2].dest),
	INT32_TSDFIELD(cargo[2].due_date),

	FLOAT_TSDFIELD(cargo[3].paid),
	INT32_TSDFIELD(cargo[3].contents.item),
	FLOAT_TSDFIELD(cargo[3].contents.qty),
	UINT32_TSDFIELD(cargo[3].origin),
	UINT32_TSDFIELD(cargo[3].dest),
	INT32_TSDFIELD(cargo[3].due_date),

	FLOAT_TSDFIELD(cargo[4].paid),
	INT32_TSDFIELD(cargo[4].contents.item),
	FLOAT_TSDFIELD(cargo[4].contents.qty),
	UINT32_TSDFIELD(cargo[4].origin),
	UINT32_TSDFIELD(cargo[4].dest),
	INT32_TSDFIELD(cargo[4].due_date),

	FLOAT_TSDFIELD(cargo[5].paid),
	INT32_TSDFIELD(cargo[5].contents.item),
	FLOAT_TSDFIELD(cargo[5].contents.qty),
	UINT32_TSDFIELD(cargo[5].origin),
	UINT32_TSDFIELD(cargo[5].dest),
	INT32_TSDFIELD(cargo[5].due_date),

	FLOAT_TSDFIELD(cargo[6].paid),
	INT32_TSDFIELD(cargo[6].contents.item),
	FLOAT_TSDFIELD(cargo[6].contents.qty),
	UINT32_TSDFIELD(cargo[6].origin),
	UINT32_TSDFIELD(cargo[6].dest),
	INT32_TSDFIELD(cargo[6].due_date),

	FLOAT_TSDFIELD(cargo[7].paid),
	INT32_TSDFIELD(cargo[7].contents.item),
	FLOAT_TSDFIELD(cargo[7].contents.qty),
	UINT32_TSDFIELD(cargo[7].origin),
	UINT32_TSDFIELD(cargo[7].dest),
	INT32_TSDFIELD(cargo[7].due_date),

	INT32_TSDFIELD(ncargo_bays),
	FLOAT_TSDFIELD(wallet),
	UINT8_TSDFIELD(warp_core_status),
	FLOAT_TSDFIELD(threat_level),
	UINT8_TSDFIELD(docking_magnets),
	UINT8_TSDFIELD(passenger_berths),
	UINT8_TSDFIELD(mining_bots),
	STRING_TSDFIELD(mining_bot_name),
	STRING_FIELD(sdata.name),
	UINT8_FIELD(sdata.subclass),
	UINT8_FIELD(sdata.shield_strength),
	UINT8_FIELD(sdata.shield_wavelength),
	UINT8_FIELD(sdata.shield_width),
	UINT8_FIELD(sdata.shield_depth),
	UINT8_FIELD(sdata.faction),
	FLOAT_FIELD(orientation.vec[0]),
	FLOAT_FIELD(orientation.vec[1]),
	FLOAT_FIELD(orientation.vec[2]),
	FLOAT_FIELD(orientation.vec[3]),
	UINT8_TSDFIELD(exterior_lights),
	UINT8_TSDFIELD(align_sciball_to_ship),
#ifdef INCLUDE_BRIDGE_INFO_FIELDS
	INT32_BRIDGE_FIELD(initialized),
	UINT8_BRIDGE_FIELD(persistent_bridge_data.engineering_preset[0][0]),
	UINT8_BRIDGE_FIELD(persistent_bridge_data.engineering_preset[0][1]),
	UINT8_BRIDGE_FIELD(persistent_bridge_data.engineering_preset[0][2]),
	UINT8_BRIDGE_FIELD(persistent_bridge_data.engineering_preset[0][3]),
	UINT8_BRIDGE_FIELD(persistent_bridge_data.engineering_preset[0][4]),
	UINT8_BRIDGE_FIELD(persistent_bridge_data.engineering_preset[0][5]),
	UINT8_BRIDGE_FIELD(persistent_bridge_data.engineering_preset[0][6]),
	UINT8_BRIDGE_FIELD(persistent_bridge_data.engineering_preset[0][7]),
	UINT8_BRIDGE_FIELD(persistent_bridge_data.engineering_preset[0][8]),
	UINT8_BRIDGE_FIELD(persistent_bridge_data.engineering_preset[0][9]),
	UINT8_BRIDGE_FIELD(persistent_bridge_data.engineering_preset[0][10]),
	UINT8_BRIDGE_FIELD(persistent_bridge_data.engineering_preset[0][11]),
	UINT8_BRIDGE_FIELD(persistent_bridge_data.engineering_preset[0][12]),
	UINT8_BRIDGE_FIELD(persistent_bridge_data.engineering_preset[0][13]),
	UINT8_BRIDGE_FIELD(persistent_bridge_data.engineering_preset[0][14]),
	UINT8_BRIDGE_FIELD(persistent_bridge_data.engineering_preset[0][15]),
	UINT8_BRIDGE_FIELD(persistent_bridge_data.engineering_preset[0][16]),
	UINT8_BRIDGE_FIELD(persistent_bridge_data.engineering_preset[0][17]),

	UINT8_BRIDGE_FIELD(persistent_bridge_data.engineering_preset[1][0]),
	UINT8_BRIDGE_FIELD(persistent_bridge_data.engineering_preset[1][1]),
	UINT8_BRIDGE_FIELD(persistent_bridge_data.engineering_preset[1][2]),
	UINT8_BRIDGE_FIELD(persistent_bridge_data.engineering_preset[1][3]),
	UINT8_BRIDGE_FIELD(persistent_bridge_data.engineering_preset[1][4]),
	UINT8_BRIDGE_FIELD(persistent_bridge_data.engineering_preset[1][5]),
	UINT8_BRIDGE_FIELD(persistent_bridge_data.engineering_preset[1][6]),
	UINT8_BRIDGE_FIELD(persistent_bridge_data.engineering_preset[1][7]),
	UINT8_BRIDGE_FIELD(persistent_bridge_data.engineering_preset[1][8]),
	UINT8_BRIDGE_FIELD(persistent_bridge_data.engineering_preset[1][9]),
	UINT8_BRIDGE_FIELD(persistent_bridge_data.engineering_preset[1][10]),
	UINT8_BRIDGE_FIELD(persistent_bridge_data.engineering_preset[1][11]),
	UINT8_BRIDGE_FIELD(persistent_bridge_data.engineering_preset[1][12]),
	UINT8_BRIDGE_FIELD(persistent_bridge_data.engineering_preset[1][13]),
	UINT8_BRIDGE_FIELD(persistent_bridge_data.engineering_preset[1][14]),
	UINT8_BRIDGE_FIELD(persistent_bridge_data.engineering_preset[1][15]),
	UINT8_BRIDGE_FIELD(persistent_bridge_data.engineering_preset[1][16]),
	UINT8_BRIDGE_FIELD(persistent_bridge_data.engineering_preset[1][17]),

	UINT8_BRIDGE_FIELD(persistent_bridge_data.engineering_preset[2][0]),
	UINT8_BRIDGE_FIELD(persistent_bridge_data.engineering_preset[2][1]),
	UINT8_BRIDGE_FIELD(persistent_bridge_data.engineering_preset[2][2]),
	UINT8_BRIDGE_FIELD(persistent_bridge_data.engineering_preset[2][3]),
	UINT8_BRIDGE_FIELD(persistent_bridge_data.engineering_preset[2][4]),
	UINT8_BRIDGE_FIELD(persistent_bridge_data.engineering_preset[2][5]),
	UINT8_BRIDGE_FIELD(persistent_bridge_data.engineering_preset[2][6]),
	UINT8_BRIDGE_FIELD(persistent_bridge_data.engineering_preset[2][7]),
	UINT8_BRIDGE_FIELD(persistent_bridge_data.engineering_preset[2][8]),
	UINT8_BRIDGE_FIELD(persistent_bridge_data.engineering_preset[2][9]),
	UINT8_BRIDGE_FIELD(persistent_bridge_data.engineering_preset[2][10]),
	UINT8_BRIDGE_FIELD(persistent_bridge_data.engineering_preset[2][11]),
	UINT8_BRIDGE_FIELD(persistent_bridge_data.engineering_preset[2][12]),
	UINT8_BRIDGE_FIELD(persistent_bridge_data.engineering_preset[2][13]),
	UINT8_BRIDGE_FIELD(persistent_bridge_data.engineering_preset[2][14]),
	UINT8_BRIDGE_FIELD(persistent_bridge_data.engineering_preset[2][15]),
	UINT8_BRIDGE_FIELD(persistent_bridge_data.engineering_preset[2][16]),
	UINT8_BRIDGE_FIELD(persistent_bridge_data.engineering_preset[2][17]),

	UINT8_BRIDGE_FIELD(persistent_bridge_data.engineering_preset[3][0]),
	UINT8_BRIDGE_FIELD(persistent_bridge_data.engineering_preset[3][1]),
	UINT8_BRIDGE_FIELD(persistent_bridge_data.engineering_preset[3][2]),
	UINT8_BRIDGE_FIELD(persistent_bridge_data.engineering_preset[3][3]),
	UINT8_BRIDGE_FIELD(persistent_bridge_data.engineering_preset[3][4]),
	UINT8_BRIDGE_FIELD(persistent_bridge_data.engineering_preset[3][5]),
	UINT8_BRIDGE_FIELD(persistent_bridge_data.engineering_preset[3][6]),
	UINT8_BRIDGE_FIELD(persistent_bridge_data.engineering_preset[3][7]),
	UINT8_BRIDGE_FIELD(persistent_bridge_data.engineering_preset[3][8]),
	UINT8_BRIDGE_FIELD(persistent_bridge_data.engineering_preset[3][9]),
	UINT8_BRIDGE_FIELD(persistent_bridge_data.engineering_preset[3][10]),
	UINT8_BRIDGE_FIELD(persistent_bridge_data.engineering_preset[3][11]),
	UINT8_BRIDGE_FIELD(persistent_bridge_data.engineering_preset[3][12]),
	UINT8_BRIDGE_FIELD(persistent_bridge_data.engineering_preset[3][13]),
	UINT8_BRIDGE_FIELD(persistent_bridge_data.engineering_preset[3][14]),
	UINT8_BRIDGE_FIELD(persistent_bridge_data.engineering_preset[3][15]),
	UINT8_BRIDGE_FIELD(persistent_bridge_data.engineering_preset[3][16]),
	UINT8_BRIDGE_FIELD(persistent_bridge_data.engineering_preset[3][17]),

	UINT8_BRIDGE_FIELD(persistent_bridge_data.engineering_preset[4][0]),
	UINT8_BRIDGE_FIELD(persistent_bridge_data.engineering_preset[4][1]),
	UINT8_BRIDGE_FIELD(persistent_bridge_data.engineering_preset[4][2]),
	UINT8_BRIDGE_FIELD(persistent_bridge_data.engineering_preset[4][3]),
	UINT8_BRIDGE_FIELD(persistent_bridge_data.engineering_preset[4][4]),
	UINT8_BRIDGE_FIELD(persistent_bridge_data.engineering_preset[4][5]),
	UINT8_BRIDGE_FIELD(persistent_bridge_data.engineering_preset[4][6]),
	UINT8_BRIDGE_FIELD(persistent_bridge_data.engineering_preset[4][7]),
	UINT8_BRIDGE_FIELD(persistent_bridge_data.engineering_preset[4][8]),
	UINT8_BRIDGE_FIELD(persistent_bridge_data.engineering_preset[4][9]),
	UINT8_BRIDGE_FIELD(persistent_bridge_data.engineering_preset[4][10]),
	UINT8_BRIDGE_FIELD(persistent_bridge_data.engineering_preset[4][11]),
	UINT8_BRIDGE_FIELD(persistent_bridge_data.engineering_preset[4][12]),
	UINT8_BRIDGE_FIELD(persistent_bridge_data.engineering_preset[4][13]),
	UINT8_BRIDGE_FIELD(persistent_bridge_data.engineering_preset[4][14]),
	UINT8_BRIDGE_FIELD(persistent_bridge_data.engineering_preset[4][15]),
	UINT8_BRIDGE_FIELD(persistent_bridge_data.engineering_preset[4][16]),
	UINT8_BRIDGE_FIELD(persistent_bridge_data.engineering_preset[4][17]),

	UINT8_BRIDGE_FIELD(persistent_bridge_data.engineering_preset[5][0]),
	UINT8_BRIDGE_FIELD(persistent_bridge_data.engineering_preset[5][1]),
	UINT8_BRIDGE_FIELD(persistent_bridge_data.engineering_preset[5][2]),
	UINT8_BRIDGE_FIELD(persistent_bridge_data.engineering_preset[5][3]),
	UINT8_BRIDGE_FIELD(persistent_bridge_data.engineering_preset[5][4]),
	UINT8_BRIDGE_FIELD(persistent_bridge_data.engineering_preset[5][5]),
	UINT8_BRIDGE_FIELD(persistent_bridge_data.engineering_preset[5][6]),
	UINT8_BRIDGE_FIELD(persistent_bridge_data.engineering_preset[5][7]),
	UINT8_BRIDGE_FIELD(persistent_bridge_data.engineering_preset[5][8]),
	UINT8_BRIDGE_FIELD(persistent_bridge_data.engineering_preset[5][9]),
	UINT8_BRIDGE_FIELD(persistent_bridge_data.engineering_preset[5][10]),
	UINT8_BRIDGE_FIELD(persistent_bridge_data.engineering_preset[5][11]),
	UINT8_BRIDGE_FIELD(persistent_bridge_data.engineering_preset[5][12]),
	UINT8_BRIDGE_FIELD(persistent_bridge_data.engineering_preset[5][13]),
	UINT8_BRIDGE_FIELD(persistent_bridge_data.engineering_preset[5][14]),
	UINT8_BRIDGE_FIELD(persistent_bridge_data.engineering_preset[5][15]),
	UINT8_BRIDGE_FIELD(persistent_bridge_data.engineering_preset[5][16]),
	UINT8_BRIDGE_FIELD(persistent_bridge_data.engineering_preset[5][17]),

	UINT8_BRIDGE_FIELD(persistent_bridge_data.engineering_preset[6][0]),
	UINT8_BRIDGE_FIELD(persistent_bridge_data.engineering_preset[6][1]),
	UINT8_BRIDGE_FIELD(persistent_bridge_data.engineering_preset[6][2]),
	UINT8_BRIDGE_FIELD(persistent_bridge_data.engineering_preset[6][3]),
	UINT8_BRIDGE_FIELD(persistent_bridge_data.engineering_preset[6][4]),
	UINT8_BRIDGE_FIELD(persistent_bridge_data.engineering_preset[6][5]),
	UINT8_BRIDGE_FIELD(persistent_bridge_data.engineering_preset[6][6]),
	UINT8_BRIDGE_FIELD(persistent_bridge_data.engineering_preset[6][7]),
	UINT8_BRIDGE_FIELD(persistent_bridge_data.engineering_preset[6][8]),
	UINT8_BRIDGE_FIELD(persistent_bridge_data.engineering_preset[6][9]),
	UINT8_BRIDGE_FIELD(persistent_bridge_data.engineering_preset[6][10]),
	UINT8_BRIDGE_FIELD(persistent_bridge_data.engineering_preset[6][11]),
	UINT8_BRIDGE_FIELD(persistent_bridge_data.engineering_preset[6][12]),
	UINT8_BRIDGE_FIELD(persistent_bridge_data.engineering_preset[6][13]),
	UINT8_BRIDGE_FIELD(persistent_bridge_data.engineering_preset[6][14]),
	UINT8_BRIDGE_FIELD(persistent_bridge_data.engineering_preset[6][15]),
	UINT8_BRIDGE_FIELD(persistent_bridge_data.engineering_preset[6][16]),
	UINT8_BRIDGE_FIELD(persistent_bridge_data.engineering_preset[6][17]),

	UINT8_BRIDGE_FIELD(persistent_bridge_data.engineering_preset[7][0]),
	UINT8_BRIDGE_FIELD(persistent_bridge_data.engineering_preset[7][1]),
	UINT8_BRIDGE_FIELD(persistent_bridge_data.engineering_preset[7][2]),
	UINT8_BRIDGE_FIELD(persistent_bridge_data.engineering_preset[7][3]),
	UINT8_BRIDGE_FIELD(persistent_bridge_data.engineering_preset[7][4]),
	UINT8_BRIDGE_FIELD(persistent_bridge_data.engineering_preset[7][5]),
	UINT8_BRIDGE_FIELD(persistent_bridge_data.engineering_preset[7][6]),
	UINT8_BRIDGE_FIELD(persistent_bridge_data.engineering_preset[7][7]),
	UINT8_BRIDGE_FIELD(persistent_bridge_data.engineering_preset[7][8]),
	UINT8_BRIDGE_FIELD(persistent_bridge_data.engineering_preset[7][9]),
	UINT8_BRIDGE_FIELD(persistent_bridge_data.engineering_preset[7][10]),
	UINT8_BRIDGE_FIELD(persistent_bridge_data.engineering_preset[7][11]),
	UINT8_BRIDGE_FIELD(persistent_bridge_data.engineering_preset[7][12]),
	UINT8_BRIDGE_FIELD(persistent_bridge_data.engineering_preset[7][13]),
	UINT8_BRIDGE_FIELD(persistent_bridge_data.engineering_preset[7][14]),
	UINT8_BRIDGE_FIELD(persistent_bridge_data.engineering_preset[7][15]),
	UINT8_BRIDGE_FIELD(persistent_bridge_data.engineering_preset[7][16]),
	UINT8_BRIDGE_FIELD(persistent_bridge_data.engineering_preset[7][17]),

	UINT8_BRIDGE_FIELD(persistent_bridge_data.engineering_preset[8][0]),
	UINT8_BRIDGE_FIELD(persistent_bridge_data.engineering_preset[8][1]),
	UINT8_BRIDGE_FIELD(persistent_bridge_data.engineering_preset[8][2]),
	UINT8_BRIDGE_FIELD(persistent_bridge_data.engineering_preset[8][3]),
	UINT8_BRIDGE_FIELD(persistent_bridge_data.engineering_preset[8][4]),
	UINT8_BRIDGE_FIELD(persistent_bridge_data.engineering_preset[8][5]),
	UINT8_BRIDGE_FIELD(persistent_bridge_data.engineering_preset[8][6]),
	UINT8_BRIDGE_FIELD(persistent_bridge_data.engineering_preset[8][7]),
	UINT8_BRIDGE_FIELD(persistent_bridge_data.engineering_preset[8][8]),
	UINT8_BRIDGE_FIELD(persistent_bridge_data.engineering_preset[8][9]),
	UINT8_BRIDGE_FIELD(persistent_bridge_data.engineering_preset[8][10]),
	UINT8_BRIDGE_FIELD(persistent_bridge_data.engineering_preset[8][11]),
	UINT8_BRIDGE_FIELD(persistent_bridge_data.engineering_preset[8][12]),
	UINT8_BRIDGE_FIELD(persistent_bridge_data.engineering_preset[8][13]),
	UINT8_BRIDGE_FIELD(persistent_bridge_data.engineering_preset[8][14]),
	UINT8_BRIDGE_FIELD(persistent_bridge_data.engineering_preset[8][15]),
	UINT8_BRIDGE_FIELD(persistent_bridge_data.engineering_preset[8][16]),
	UINT8_BRIDGE_FIELD(persistent_bridge_data.engineering_preset[8][17]),

	UINT8_BRIDGE_FIELD(persistent_bridge_data.engineering_preset[9][0]),
	UINT8_BRIDGE_FIELD(persistent_bridge_data.engineering_preset[9][1]),
	UINT8_BRIDGE_FIELD(persistent_bridge_data.engineering_preset[9][2]),
	UINT8_BRIDGE_FIELD(persistent_bridge_data.engineering_preset[9][3]),
	UINT8_BRIDGE_FIELD(persistent_bridge_data.engineering_preset[9][4]),
	UINT8_BRIDGE_FIELD(persistent_bridge_data.engineering_preset[9][5]),
	UINT8_BRIDGE_FIELD(persistent_bridge_data.engineering_preset[9][6]),
	UINT8_BRIDGE_FIELD(persistent_bridge_data.engineering_preset[9][7]),
	UINT8_BRIDGE_FIELD(persistent_bridge_data.engineering_preset[9][8]),
	UINT8_BRIDGE_FIELD(persistent_bridge_data.engineering_preset[9][9]),
	UINT8_BRIDGE_FIELD(persistent_bridge_data.engineering_preset[9][10]),
	UINT8_BRIDGE_FIELD(persistent_bridge_data.engineering_preset[9][11]),
	UINT8_BRIDGE_FIELD(persistent_bridge_data.engineering_preset[9][12]),
	UINT8_BRIDGE_FIELD(persistent_bridge_data.engineering_preset[9][13]),
	UINT8_BRIDGE_FIELD(persistent_bridge_data.engineering_preset[9][14]),
	UINT8_BRIDGE_FIELD(persistent_bridge_data.engineering_preset[9][15]),
	UINT8_BRIDGE_FIELD(persistent_bridge_data.engineering_preset[9][16]),
	UINT8_BRIDGE_FIELD(persistent_bridge_data.engineering_preset[9][17]),


/* Macros for constructing key value specifications for snis mv cargo, */
#define CARGO_DOUBLE_FIELD(x) { #x, KVS_DOUBLE, 1, offsetof(struct bridge_info, x), sizeof(double), }
#define CARGO_FLOAT_FIELD(x) { #x, KVS_FLOAT, 1, offsetof(struct bridge_info, x), sizeof(float), }
#define CARGO_UINT32_FIELD(x) { #x, KVS_UINT32, 1, offsetof(struct bridge_info, x), sizeof(uint32_t), }
#define CARGO_UINT16_FIELD(x) { #x, KVS_UINT16, 1, offsetof(struct bridge_info, x), sizeof(uint16_t), }
#define CARGO_UINT8_FIELD(x) { #x, KVS_INT8, 1, offsetof(struct bridge_info, x), sizeof(int8_t), }
#define CARGO_INT32_FIELD(x) { #x, KVS_INT32, 1, offsetof(struct bridge_info, x), sizeof(int32_t), }
#define CARGO_INT16_FIELD(x) { #x, KVS_INT16, 1, offsetof(struct bridge_info, x), sizeof(int16_t), }
#define CARGO_INT8_FIELD(x) { #x, KVS_INT8, 1, offsetof(struct bridge_info, x), sizeof(int8_t), }
#define CARGO_STRING_FIELD(x) { #x, KVS_STRING, 1, offsetof(struct bridge_info, x), \
	sizeof(((struct bridge_info *) 0)->x), }

	CARGO_INT32_FIELD(cargo.ncargo_bays),
	CARGO_STRING_FIELD(cargo.cargo[0].fc.name),
	CARGO_STRING_FIELD(cargo.cargo[0].fc.unit),
	CARGO_STRING_FIELD(cargo.cargo[0].fc.scans_as),
	CARGO_STRING_FIELD(cargo.cargo[0].fc.category),
	CARGO_STRING_FIELD(cargo.cargo[0].fc.base_price),
	CARGO_STRING_FIELD(cargo.cargo[0].fc.volatility),
	CARGO_STRING_FIELD(cargo.cargo[0].fc.legality),
	CARGO_STRING_FIELD(cargo.cargo[0].qty),
	CARGO_STRING_FIELD(cargo.cargo[0].paid),

	CARGO_STRING_FIELD(cargo.cargo[1].fc.name),
	CARGO_STRING_FIELD(cargo.cargo[1].fc.unit),
	CARGO_STRING_FIELD(cargo.cargo[1].fc.scans_as),
	CARGO_STRING_FIELD(cargo.cargo[1].fc.category),
	CARGO_STRING_FIELD(cargo.cargo[1].fc.base_price),
	CARGO_STRING_FIELD(cargo.cargo[1].fc.volatility),
	CARGO_STRING_FIELD(cargo.cargo[1].fc.legality),
	CARGO_STRING_FIELD(cargo.cargo[1].qty),
	CARGO_STRING_FIELD(cargo.cargo[1].paid),

	CARGO_STRING_FIELD(cargo.cargo[2].fc.name),
	CARGO_STRING_FIELD(cargo.cargo[2].fc.unit),
	CARGO_STRING_FIELD(cargo.cargo[2].fc.scans_as),
	CARGO_STRING_FIELD(cargo.cargo[2].fc.category),
	CARGO_STRING_FIELD(cargo.cargo[2].fc.base_price),
	CARGO_STRING_FIELD(cargo.cargo[2].fc.volatility),
	CARGO_STRING_FIELD(cargo.cargo[2].fc.legality),
	CARGO_STRING_FIELD(cargo.cargo[2].qty),
	CARGO_STRING_FIELD(cargo.cargo[2].paid),

	CARGO_STRING_FIELD(cargo.cargo[3].fc.name),
	CARGO_STRING_FIELD(cargo.cargo[3].fc.unit),
	CARGO_STRING_FIELD(cargo.cargo[3].fc.scans_as),
	CARGO_STRING_FIELD(cargo.cargo[3].fc.category),
	CARGO_STRING_FIELD(cargo.cargo[3].fc.base_price),
	CARGO_STRING_FIELD(cargo.cargo[3].fc.volatility),
	CARGO_STRING_FIELD(cargo.cargo[3].fc.legality),
	CARGO_STRING_FIELD(cargo.cargo[3].qty),
	CARGO_STRING_FIELD(cargo.cargo[3].paid),

	CARGO_STRING_FIELD(cargo.cargo[4].fc.name),
	CARGO_STRING_FIELD(cargo.cargo[4].fc.unit),
	CARGO_STRING_FIELD(cargo.cargo[4].fc.scans_as),
	CARGO_STRING_FIELD(cargo.cargo[4].fc.category),
	CARGO_STRING_FIELD(cargo.cargo[4].fc.base_price),
	CARGO_STRING_FIELD(cargo.cargo[4].fc.volatility),
	CARGO_STRING_FIELD(cargo.cargo[4].fc.legality),
	CARGO_STRING_FIELD(cargo.cargo[4].qty),
	CARGO_STRING_FIELD(cargo.cargo[4].paid),

	CARGO_STRING_FIELD(cargo.cargo[5].fc.name),
	CARGO_STRING_FIELD(cargo.cargo[5].fc.unit),
	CARGO_STRING_FIELD(cargo.cargo[5].fc.scans_as),
	CARGO_STRING_FIELD(cargo.cargo[5].fc.category),
	CARGO_STRING_FIELD(cargo.cargo[5].fc.base_price),
	CARGO_STRING_FIELD(cargo.cargo[5].fc.volatility),
	CARGO_STRING_FIELD(cargo.cargo[5].fc.legality),
	CARGO_STRING_FIELD(cargo.cargo[5].qty),
	CARGO_STRING_FIELD(cargo.cargo[5].paid),

	CARGO_STRING_FIELD(cargo.cargo[6].fc.name),
	CARGO_STRING_FIELD(cargo.cargo[6].fc.unit),
	CARGO_STRING_FIELD(cargo.cargo[6].fc.scans_as),
	CARGO_STRING_FIELD(cargo.cargo[6].fc.category),
	CARGO_STRING_FIELD(cargo.cargo[6].fc.base_price),
	CARGO_STRING_FIELD(cargo.cargo[6].fc.volatility),
	CARGO_STRING_FIELD(cargo.cargo[6].fc.legality),
	CARGO_STRING_FIELD(cargo.cargo[6].qty),
	CARGO_STRING_FIELD(cargo.cargo[6].paid),

	CARGO_STRING_FIELD(cargo.cargo[7].fc.name),
	CARGO_STRING_FIELD(cargo.cargo[7].fc.unit),
	CARGO_STRING_FIELD(cargo.cargo[7].fc.scans_as),
	CARGO_STRING_FIELD(cargo.cargo[7].fc.category),
	CARGO_STRING_FIELD(cargo.cargo[7].fc.base_price),
	CARGO_STRING_FIELD(cargo.cargo[7].fc.volatility),
	CARGO_STRING_FIELD(cargo.cargo[7].fc.legality),
	CARGO_STRING_FIELD(cargo.cargo[7].qty),
	CARGO_STRING_FIELD(cargo.cargo[7].paid),
#endif

	{ 0, 0, 0, 0, 0 },
};

#undef CARGO_DOUBLE_FIELD
#undef CARGO_FLOAT_FIELD
#undef CARGO_UINT32_FIELD
#undef CARGO_UINT16_FIELD
#undef CARGO_UINT8_FIELD
#undef CARGO_INT32_FIELD
#undef CARGO_INT16_FIELD
#undef CARGO_INT8_FIELD
#undef CARGO_STRING_FIELD

#undef DOUBLE_FIELD
#undef FLOAT_FIELD
#undef UINT32_FIELD
#undef UINT16_FIELD
#undef UINT8_FIELD
#undef INT32_FIELD
#undef INT16_FIELD
#undef INT8_FIELD
#undef STRING_FIELD

#endif

