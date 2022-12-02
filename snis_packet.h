#ifndef __SNIS_PACKET_H__
#define __SNIS_PACKET_H__
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
#include <stdint.h>

#include "snis_marshal.h"

#define OPCODE_UPDATE_SHIP		100
#define OPCODE_UPDATE_STARBASE	101
#define OPCODE_UPDATE_LASER	102
#define OPCODE_UPDATE_TORPEDO	103
#define OPCODE_UPDATE_PLAYER	104
#define OPCODE_ACK_PLAYER	105	
#define OPCODE_ID_CLIENT_SHIP	106
#define OPCODE_UPDATE_ASTEROID	107
#define OPCODE_REQUEST_YAW	108
#define OPCODE_UPDATE_BLACK_HOLE 109
#define OPCODE_UPDATE_SCI_TEXT	110
#define OPCODE_REQUEST_TORPEDO	111
#define OPCODE_DELETE_OBJECT    112
#define OPCODE_UPDATE_EXPLOSION 113
#define OPCODE_PLAY_SOUND	114
#define OPCODE_REQUEST_SCIYAW	115
#define OPCODE_REQUEST_SCIBEAMWIDTH	116
#define OPCODE_SHIP_SDATA_WITHOUT_NAME	117
#define OPCODE_ECON_UPDATE_SHIP	118
#define OPCODE_SHIP_SDATA	120
#define OPCODE_LOAD_TORPEDO	121
#define OPCODE_REQUEST_PHASER	122
#define OPCODE_EJECT_WARP_CORE	123
#define OPCODE_UPDATE_WARP_CORE 124
#define OPCODE_DEMON_RTSMODE	125
#define		OPCODE_RTSMODE_SUBCMD_DISABLE	0
#define		OPCODE_RTSMODE_SUBCMD_ENABLE	1
#define OPCODE_RTS_FUNC		126
#define		OPCODE_RTS_FUNC_COMMS_BUTTON 1
#define		OPCODE_RTS_FUNC_BUILD_UNIT 2
#define		OPCODE_RTS_FUNC_COMMAND_UNIT 3
#define		OPCODE_RTS_FUNC_MAIN_BASE_UPDATE 4
#define OPCODE_UPDATE_SHIP_CARGO_INFO 127
#define OPCODE_ADJUST_TTS_VOLUME 128
#define OPCODE_CUSTOM_BUTTON 129
#define   OPCODE_CUSTOM_BUTTON_SUBCMD_NAV 0
#define   OPCODE_CUSTOM_BUTTON_SUBCMD_WEAPONS 1
#define   OPCODE_CUSTOM_BUTTON_SUBCMD_ENG 2
#define   OPCODE_CUSTOM_BUTTON_SUBCMD_DAMCON 3
#define   OPCODE_CUSTOM_BUTTON_SUBCMD_SCIENCE 4
#define   OPCODE_CUSTOM_BUTTON_SUBCMD_COMMS 5
#define   OPCODE_CUSTOM_BUTTON_SUBCMD_ACTIVE_LIST 20
#define   OPCODE_CUSTOM_BUTTON_SUBCMD_UPDATE_TEXT 21
#define OPCODE_CONSOLE_OP 130
#define   OPCODE_CONSOLE_SUBCMD_ADD_TEXT 0
#define OPCODE_CLIENT_CONFIG 131
#define   OPCODE_CLIENT_NOTIFY_CURRENT_STATION 0
#define   OPCODE_CLIENT_SET_PERMITTED_ROLES 1
#define OPCODE_UPDATE_FLARE 132
#define OPCODE_ENGAGE_WARP 133
#define OPCODE_ROLE_ONSCREEN 134
#define OPCODE_SCI_SELECT_TARGET 135
#define   OPCODE_SCI_SELECT_TARGET_TYPE_OBJECT 1
#define   OPCODE_SCI_SELECT_TARGET_TYPE_WAYPOINT 2
#define OPCODE_UPDATE_DAMAGE 136
#define OPCODE_REQUEST_LASER 137
#define OPCODE_UPDATE_MISSILE 138

#define OPCODE_ADJUST_CONTROL_INPUT 139
#define   OPCODE_ADJUST_CONTROL_LASER_WAVELENGTH 0
#define   OPCODE_ADJUST_CONTROL_NAVZOOM 1
#define   OPCODE_ADJUST_CONTROL_MAINZOOM 2
#define   OPCODE_ADJUST_CONTROL_THROTTLE 3
#define   OPCODE_ADJUST_CONTROL_SCIZOOM 4
#define   OPCODE_ADJUST_CONTROL_WARPDRIVE 5
#define   OPCODE_ADJUST_CONTROL_SHIELD 6
#define   OPCODE_ADJUST_CONTROL_MANEUVERING_PWR 7
#define   OPCODE_ADJUST_CONTROL_TRACTOR_PWR 8
#define   OPCODE_ADJUST_CONTROL_LIFESUPPORT_PWR 9
#define   OPCODE_ADJUST_CONTROL_SHIELDS_PWR 10
#define   OPCODE_ADJUST_CONTROL_IMPULSE_PWR 11
#define   OPCODE_ADJUST_CONTROL_WARP_PWR 12
#define   OPCODE_ADJUST_CONTROL_SENSORS_PWR 13
#define   OPCODE_ADJUST_CONTROL_PHASERBANKS_PWR 14
#define   OPCODE_ADJUST_CONTROL_COMMS_PWR 15
#define   OPCODE_ADJUST_CONTROL_MANEUVERING_COOLANT 16
#define   OPCODE_ADJUST_CONTROL_TRACTOR_COOLANT 17
#define   OPCODE_ADJUST_CONTROL_LIFESUPPORT_COOLANT 18
#define   OPCODE_ADJUST_CONTROL_SHIELDS_COOLANT 19
#define   OPCODE_ADJUST_CONTROL_IMPULSE_COOLANT 20
#define   OPCODE_ADJUST_CONTROL_WARP_COOLANT 21
#define   OPCODE_ADJUST_CONTROL_SENSORS_COOLANT 22
#define   OPCODE_ADJUST_CONTROL_PHASERBANKS_COOLANT 23
#define   OPCODE_ADJUST_CONTROL_COMMS_COOLANT 24
#define	  OPCODE_ADJUST_CONTROL_EXTERIOR_LIGHTS 25
#define   OPCODE_ADJUST_CONTROL_SILENCE_ALARMS 26
#define   OPCODE_ADJUST_CONTROL_FIRE_MISSILE 27
#define   OPCODE_ADJUST_CONTROL_DEPLOY_FLARE 28

#define OPCODE_UPDATE_PLANET_DESCRIPTION 140
#define OPCODE_UPDATE_RESPAWN_TIME 141
#define OPCODE_UPDATE_NETSTATS 142
#define OPCODE_COMMS_TRANSMISSION 143
#define   OPCODE_COMMS_PLAINTEXT 0
#define   OPCODE_COMMS_ENCIPHERED 1
#define   OPCODE_COMMS_KEY_GUESS 2
#define   OPCODE_COMMS_UPDATE_ENCIPHERED 3
#define OPCODE_WARP_LIMBO	144	
#define OPCODE_DEMON_COMMAND 145
#define OPCODE_UPDATE_NEBULA		146
#define OPCODE_DAMCON_OBJ_UPDATE	147
#define OPCODE_REQUEST_ROBOT_YAW	148
#define OPCODE_REQUEST_ROBOT_THRUST	149
#define OPCODE_DAMCON_SOCKET_UPDATE	150
#define OPCODE_DAMCON_PART_UPDATE	151
#define OPCODE_REQUEST_ROBOT_GRIPPER	152
#define OPCODE_MAINSCREEN_VIEW_MODE	153
#define OPCODE_UPDATE_WORMHOLE		154
#define OPCODE_WORMHOLE_LIMBO		155
#define OPCODE_UPDATE_SPACEMONSTER	156
#define OPCODE_REQUEST_WEAPZOOM		157
#define OPCODE_REQUEST_ONESHOT_SOUND	158
#define OPCODE_REQUEST_REDALERT		159
#define OPCODE_UPDATE_POWER_DATA	160
#define OPCODE_UPDATE_PLANET		161
#define OPCODE_CREATE_ITEM		162
#define OPCODE_DEMON_COMMS_XMIT		163
#define OPCODE_DEMON_FIRE_TORPEDO	164
#define OPCODE_DEMON_FIRE_PHASER	165
#define OPCODE_DEMON_POSSESS		166
#define OPCODE_DEMON_DISPOSSESS		167
#define OPCODE_DEMON_ROT		168
#define   OPCODE_DEMON_ROT_YAW		0
#define   OPCODE_DEMON_ROT_PITCH	1
#define   OPCODE_DEMON_ROT_ROLL		2
#define OPCODE_DEMON_THRUST		169
#define OPCODE_DEMON_MOVE_OBJECT	170
#define OPCODE_INITIATE_WARP		171
#define OPCODE_REQUEST_REVERSE		172
#define OPCODE_UPDATE_LASERBEAM		173
#define OPCODE_WEAP_SELECT_TARGET 	174
#define OPCODE_SCI_DETAILS		175
#define OPCODE_PROXIMITY_ALERT		176
#define OPCODE_COLLISION_NOTIFICATION	177
#define OPCODE_UPDATE_DERELICT		178
#define OPCODE_DEMON_CLEAR_ALL		179
#define OPCODE_EXEC_LUA_SCRIPT		180
#define OPCODE_REQUEST_TRACTORBEAM	181
#define OPCODE_UPDATE_TRACTORBEAM	182
#define OPCODE_COMMS_CRYPTO		183
#define OPCODE_COMMS_MAINSCREEN	184
#define COMMS_CHANNEL_CHANGE_MSG "TX/RX INITIATED ON CHANNEL "
#define OPCODE_LOAD_SKYBOX 185
#define OPCODE_ROBOT_AUTO_MANUAL 	186
#define OPCODE_ADD_WARP_EFFECT		187
#define OPCODE_OPUS_AUDIO_DATA		188
#define OPCODE_REQUEST_PITCH		189
#define OPCODE_REQUEST_ROLL		190
#define OPCODE_REQUEST_SCIBALL_YAW	191
#define OPCODE_REQUEST_SCIBALL_PITCH	192
#define OPCODE_REQUEST_SCIBALL_ROLL	193
#define OPCODE_REQUEST_SCIBALL_ROT	194
#define OPCODE_REQUEST_MANUAL_GUNYAW	195
#define OPCODE_REQUEST_MANUAL_GUNPITCH	196
#define OPCODE_REQUEST_MANUAL_LASER	197
#define OPCODE_REQUEST_WEAPONS_YAW_PITCH 198
#define OPCODE_UPDATE_COOLANT_DATA	199
#define OPCODE_UPDATE_ASTEROID_MINERALS 200
#define OPCODE_TALKING_STICK		201
#define		OPCODE_TALKING_STICK_REQUEST	1
#define		OPCODE_TALKING_STICK_GRANT	2
#define		OPCODE_TALKING_STICK_RELEASE	3
/* UNUSED OPCODE 202 */
/* UNUSED OPCODE 203 */
/* UNUSED OPCODE 204 */
/* UNUSED OPCODE 205 */
/* UNUSED OPCODE 206 */
/* UNUSED OPCODE 207 */
#define OPCODE_SILENT_UPDATE_DAMAGE		208
#define OPCODE_ECON_UPDATE_SHIP_DEBUG_AI 209
#define OPCODE_TOGGLE_DEMON_AI_DEBUG_MODE 210
#define OPCODE_TOGGLE_DEMON_SAFE_MODE 211
#define OPCODE_UPDATE_CARGO_CONTAINER	212
#define OPCODE_CYCLE_MAINSCREEN_POINT_OF_VIEW	213
#define OPCODE_REQUEST_UNIVERSE_TIMESTAMP	214
#define OPCODE_UPDATE_UNIVERSE_TIMESTAMP	215
#define OPCODE_UPDATE_BUILD_INFO		216
#define OPCODE_DETONATE				217
#define OPCODE_ENSCRIPT				218
#define OPCODE_SCI_ALIGN_TO_SHIP		219
#define OPCODE_NAV_TRIDENT_MODE			220
#define OPCODE_ATMOSPHERIC_FRICTION		221
#define OPCODE_UPDATE_CARGO_CONTAINER_POSITION	222
#define OPCODE_DOCKING_MAGNETS			223
#define OPCODE_CYCLE_NAV_POINT_OF_VIEW		224
#define OPCODE_REQUEST_MINING_BOT		225
#define OPCODE_REQUEST_STANDARD_ORBIT		226
#define OPCODE_SWITCH_SERVER			227
#define OPCODE_UPDATE_WARPGATE			228
#define OPCODE_ADD_PLAYER_ERROR			229
#define OPCODE_NATURAL_LANGUAGE_REQUEST		230
#define   OPCODE_NL_SUBCOMMAND_TEXT_REQUEST	1
#define   OPCODE_NL_SUBCOMMAND_TEXT_TO_SPEECH	2
#define OPCODE_SET_SOLARSYSTEM			231
#define OPCODE_REQUEST_ROBOT_CMD		232
/* TODO: delete these unused ROBOT_SUBCDM_?TG opcodes */
#define   OPCODE_ROBOT_SUBCMD_STG		1	/* set short term goal */
#define   OPCODE_ROBOT_SUBCMD_LTG		2	/* set long term goal */
#define OPCODE_TEXTSCREEN_OP			233
#define   OPCODE_TEXTSCREEN_CLEAR		0
#define   OPCODE_TEXTSCREEN_TIMEDTEXT		1
#define   OPCODE_TEXTSCREEN_MENU		2
#define   OPCODE_TEXTSCREEN_MENU_CHOICE		3
#define OPCODE_UPDATE_BLOCK			234
#define OPCODE_UPDATE_TURRET			235
#define OPCODE_LATENCY_CHECK			236

/* Opcode to make sure client and server are running compatible protocols.
 * Client can send FORMAT+VERIFY with format of opcode
 * Server will respond with UNKNOWN, MATCH, or MISMATCH
 * Server can send FORMAT+QUERY to client (no format included)
 * Client will respond with VERIFY + format, or UNKNOWN
 */
#define OPCODE_CHECK_OPCODE_FORMAT		237
#define   OPCODE_CHECK_OPCODE_UNKNOWN		1
#define   OPCODE_CHECK_OPCODE_MATCH		2
#define   OPCODE_CHECK_OPCODE_QUERY		3
#define   OPCODE_CHECK_OPCODE_VERIFY		4
#define   OPCODE_CHECK_OPCODE_MISMATCH		5
#define OPCODE_REQUEST_STARMAP			238
#define OPCODE_UPDATE_SOLARSYSTEM_LOCATION	239

#define OPCODE_SET_WAYPOINT			240
#define OPCODE_SET_WAYPOINT_CLEAR               1
#define OPCODE_SET_WAYPOINT_COUNT		2
#define OPCODE_SET_WAYPOINT_ROW			3
#define OPCODE_SET_WAYPOINT_ADD_ROW		4
#define OPCODE_SET_WAYPOINT_UPDATE_SELECTION	5

#define OPCODE_SAVE_ENGINEERING_PRESET		241
#define OPCODE_APPLY_ENGINEERING_PRESET		242

/* UNUSED OPCODE 243 */

#define OPCODE_NOOP		0xff

#define   ADD_PLAYER_ERROR_SHIP_ALREADY_EXISTS	0x01
#define   ADD_PLAYER_ERROR_SHIP_DOES_NOT_EXIST	0x02
#define   ADD_PLAYER_ERROR_TOO_MANY_BRIDGES	0x03
#define   ADD_PLAYER_ERROR_FAILED_VERIFICATION	0x04

#define UPDATE_UNIVERSE_TIMESTAMP_COUNT 5
#define UPDATE_UNIVERSE_TIMESTAMP_START_SAMPLE 1
#define UPDATE_UNIVERSE_TIMESTAMP_SAMPLE 0
#define UPDATE_UNIVERSE_TIMESTAMP_END_SAMPLE 2

#define OPCODE_NO_SUBCODE 0

#define NAMESIZE 20

#pragma pack(1)
struct update_ship_packet {
	uint8_t opcode;
	uint32_t id;
	uint32_t timestamp;
	uint16_t alive;
	uint32_t x, y, z, yawvel, pitchvel, rollvel;
        uint32_t ntorpedoes;
	uint32_t sci_heading;
	uint32_t sci_beam_width;
	uint8_t torpedoes_loaded; /* lower nybble == no. loading, upper == no. loaded. */
	uint8_t throttle;
	uint8_t rpm;
	uint32_t fuel;
	uint32_t oxygen;
	uint8_t temp;
	uint8_t scizoom;
	uint8_t weapzoom;
	uint8_t navzoom;
	uint8_t mainzoom;
	uint8_t warpdrive;
	uint8_t missile_count;
	uint8_t phaser_charge;
	uint8_t phaser_wavelength;
	uint8_t shiptype; /* same as ship_sdata_packet subclass */
	uint8_t reverse;
	uint8_t trident;
	uint32_t victim_id;
	int16_t orientation[4]; /* int16_t encoded orientation quaternion */
	int16_t sciball_orientation[4]; /* int16_t encoded orientation quaternion */
	int16_t weap_orientation[4];
	int16_t current_hg_ant_orientation[4]; /* high gain antenna orientation */
	uint32_t desired_hg_ant_aim[3]; /* high gain antenna desired vec3 direction vector */
	uint8_t emf_detector;
#if 0
	/* flag_bits contain the following fields packed 1 bit each */
#error These fields are not meant to be compiled
	uint8_t in_secure_area;
	uint8_t docking_magnets;
	uint8_t nav_mode;
	uint8_t warp_core_status;
	uint8_t rts_mode;
	uint8_t exterior_lights;
	uint8_t alarms_silenced;
#endif
	uint8_t flag_bits;
	uint8_t missile_lock_detected;
	uint8_t rts_active_button;
	uint32_t wallet;
	uint32_t viewpoint_object;
};

struct ship_sdata_packet {
	uint8_t opcode;
	uint32_t id;
	uint8_t subclass; /* freighter, destroyer, etc. */
	uint8_t shield_strength;
	uint8_t shield_wavelength;
	uint8_t shield_width;
	uint8_t shield_depth;
	uint8_t faction;
	uint8_t lifeform_count;
	uint8_t flags;
	char name[NAMESIZE];
};

struct ship_sdata_without_name_packet {
	uint8_t opcode;
	uint32_t id;
	uint8_t subclass; /* freighter, destroyer, etc. */
	uint8_t shield_strength;
	uint8_t shield_wavelength;
	uint8_t shield_width;
	uint8_t shield_depth;
	uint8_t faction;
	uint8_t lifeform_count;
	uint8_t flags;
};

struct request_ship_sdata_packet {
	uint8_t opcode;
	uint32_t id;
};

struct update_econ_ship_packet {
	uint8_t opcode;
	uint32_t id;
	uint32_t timestamp;
	uint16_t alive;
	uint32_t x, y, z;
	union quat orientation;
	uint8_t shiptype; /* same as ship_sdata_packet subclass */
	uint8_t rts_order; /* current order of this ship */
};

struct client_ship_id_packet {
	uint8_t opcode;
	uint32_t shipid;
};

struct update_black_hole_packet {
	uint8_t opcode;
	uint32_t id;
	uint32_t timestampe;
	uint32_t x, y, z;
};

struct update_asteroid_packet {
	uint8_t opcode;
	uint32_t id;
	uint32_t timestamp;
	uint32_t x, y, z;
};

struct update_asteroid_minerals_packet {
	uint8_t opcode;
	uint32_t id;
	uint8_t carbon, nickel, silicates, precious_metals;
};

struct update_warp_core_packet {
	uint8_t opcode;
	uint32_t id;
	uint32_t timestamp;
	uint32_t x, y, z;
};

struct update_cargo_container_packet {
	uint8_t opcode;
	uint32_t id;
	uint32_t timestamp;
	uint32_t x, y, z;
	uint32_t item;
	uint32_t qty;
};

struct update_cargo_container_position {
	uint8_t opcode;
	uint32_t id;
	uint32_t timestamp;
	uint32_t x, y, z;
};

struct update_wormhole_packet {
	uint8_t opcode;
	uint32_t id;
	uint32_t timestamp;
	uint32_t x, y;
};

struct update_starbase_packet {
	uint8_t opcode;
	uint32_t id;
	uint32_t timestamp;
	uint32_t x, y;
	uint8_t occupant[4];
	uint32_t time_left_to_build;
	uint8_t build_unit_type;
	uint8_t starbase_number;
};

struct update_nebula_packet {
	uint8_t opcode;
	uint32_t id;
	uint32_t timestamp;
	uint32_t x, y, z, r;
};

struct update_explosion_packet {
	uint8_t opcode;
	uint32_t id;
	uint32_t timestamp;
	uint32_t related_id;
	uint32_t x, y, z;
	uint16_t nsparks;
	uint16_t velocity;
	uint16_t time;
	uint8_t victim_type;
	uint8_t explosion_type;
};

struct update_flare_packet {
	uint8_t opcode;
	uint32_t id;
	uint32_t timestamp;
	uint32_t x, y, z;
};

struct delete_object_packet {
	uint8_t opcode;
	uint32_t id;
};

struct update_torpedo_packet {
	uint8_t opcode;
	uint32_t id;
	uint32_t timestamp;
	uint32_t ship_oid; /* ship torpedo came from */
	uint32_t x, y, z;
}; 

struct update_missile_packet {
	uint8_t opcode;
	uint32_t id;
	uint32_t timestamp;
	uint32_t x, y, z;
	uint16_t orientation[4];
};

struct update_laser_packet {
	uint8_t opcode;
	uint32_t id;
	uint32_t timestamp;
	uint32_t ship_oid; /* ship laser came from */
	uint32_t x, y, z;
	uint16_t orientation[4]; /* encoded orientation quaternion */
}; 

struct update_spacemonster_packet {
	uint8_t opcode;
	uint32_t id;
	uint32_t timestamp;
	uint32_t x, y, z;
	uint16_t orientation[4]; /* encoded orientation quaternion */
};

struct request_torpedo_packet {
	uint8_t opcode;
};

struct request_throttle_packet {
	uint8_t opcode;
	uint32_t id;
	uint8_t throttle;
};

struct request_yaw_packet {
	uint8_t opcode;
	uint8_t direction;
#define YAW_LEFT 0
#define YAW_RIGHT 1
#define YAW_LEFT_FINE 2
#define YAW_RIGHT_FINE 3
#define PITCH_FORWARD 0
#define PITCH_BACK 1
#define PITCH_FORWARD_FINE 2
#define PITCH_BACK_FINE 3
#define ROLL_LEFT 0
#define ROLL_RIGHT 1
#define ROLL_LEFT_FINE 2
#define ROLL_RIGHT_FINE 3
};

struct request_thrust_packet {
	uint8_t opcode;
	uint8_t direction;
#define THRUST_FORWARDS 1
#define THRUST_BACKWARDS 2
};

#define DISPLAYMODE_MAINSCREEN 0
#define DISPLAYMODE_NAVIGATION 1
#define DISPLAYMODE_WEAPONS 2
#define DISPLAYMODE_ENGINEERING 3
#define DISPLAYMODE_SCIENCE 4
#define DISPLAYMODE_COMMS 5
#define DISPLAYMODE_DEMON 6
#define DISPLAYMODE_DAMCON 7
#define DISPLAYMODE_FONTTEST 8
#define DISPLAYMODE_INTROSCREEN 9
#define DISPLAYMODE_LOBBYSCREEN 10
#define DISPLAYMODE_CONNECTING 11
#define DISPLAYMODE_CONNECTED 12
#define DISPLAYMODE_NETWORK_SETUP 13

#define ROLE_MAIN		(1 << DISPLAYMODE_MAINSCREEN)
#define ROLE_NAVIGATION		(1 << DISPLAYMODE_NAVIGATION)
#define ROLE_WEAPONS		(1 << DISPLAYMODE_WEAPONS)
#define ROLE_ENGINEERING	(1 << DISPLAYMODE_ENGINEERING)
#define ROLE_SCIENCE		(1 << DISPLAYMODE_SCIENCE)
#define ROLE_COMMS		(1 << DISPLAYMODE_COMMS)
#define ROLE_DEMON		(1 << DISPLAYMODE_DEMON)
#define ROLE_DAMCON		(1 << DISPLAYMODE_DAMCON)
#define ROLE_SOUNDSERVER	(1 << DISPLAYMODE_FONTTEST)
#define ROLE_TEXT_TO_SPEECH	(1 << 10)
#define ROLE_ALL		((uint32_t) 0x0ffffffffff)

#define SCI_DETAILS_MODE_THREED 1
#define SCI_DETAILS_MODE_DETAILS 2
#define SCI_DETAILS_MODE_SCIPLANE 3
#define SCI_DETAILS_MODE_WAYPOINTS 4

struct add_player_packet {
	uint8_t opcode;
	uint8_t new_ship;
	uint8_t warpgate_number;
	uint8_t requested_faction;
	uint32_t role;
#define SHIPNAME_LEN 20
	unsigned char shipname[SHIPNAME_LEN];
#define PASSWORD_LEN 20
	unsigned char password[PASSWORD_LEN];
};

struct ack_player_packet {
	uint8_t opcode;
	uint8_t allowed;
};

struct play_sound_packet {
	uint8_t opcode;
	uint16_t sound_number;
};

struct role_onscreen_packet {
	uint8_t opcode;
	uint8_t role;
};

struct snis_sci_select_target_packet {
	uint8_t opcode;
	uint32_t target_id;
};

struct snis_sci_select_coords_packet {
	uint8_t opcode;
	uint32_t x;
	uint32_t y;
};

struct ship_damage_packet {
	uint8_t opcode;
	uint32_t id;
/* this must match struct ship_damage_data exactly, and be endian neutral */
	uint8_t shields;
	uint8_t impulse;
	uint8_t warp;
	uint8_t torpedo_tubes;
	uint8_t phaser_banks;
	uint8_t sensors;
	uint8_t comms;
	uint8_t tractor;
	uint8_t lifesupport;
};

struct respawn_time_packet {
	uint8_t opcode;
	uint8_t seconds;
};

struct netstats_packet {
	uint8_t opcode;
	uint64_t bytes_sent;
	uint64_t bytes_recd;
	uint32_t nobjects;
	uint32_t nships;
	uint32_t elapsed_seconds;
	uint32_t latency_in_usec;
	uint32_t faction_population[5];
	uint32_t outgoing_queue_length;
	uint32_t incoming_queue_length;
};

struct comms_transmission_packet {
	uint8_t opcode;
	uint8_t enciphered;
	uint8_t length;
	uint32_t id;
};

struct lua_script_packet {
	uint8_t opcode;
	uint8_t length;
};

struct lua_enscript_packet {
	uint8_t opcode;
	uint8_t length;
};

struct warp_limbo_packet {
	uint8_t opcode;
	uint16_t limbo_time;
};

#define MAX_DEMON_CMD_IDS 100
struct demon_cmd_packet {
	uint8_t opcode;
	uint8_t demon_cmd;
#define DEMON_CMD_ATTACK 1
	uint32_t x, y;	
	uint8_t nattackerids;
	uint8_t nvictimids;
	uint32_t id[1];
};

struct damcon_obj_update_packet {
	uint8_t opcode;
	uint32_t id;
	uint32_t ship_id;
	uint32_t type;
	uint32_t x, y, velocity, heading;
	uint8_t autonomous_mode;
	uint32_t stgx, stgy, ltgx, ltgy;
};

struct damcon_socket_update_packet {
	uint8_t opcode;
	uint32_t id;
	uint32_t ship_id;
	uint32_t type;
	uint32_t x, y, contents;
	uint8_t system;
	uint8_t part; 
};

struct damcon_part_update_packet {
	uint8_t opcode;
	uint32_t id;
	uint32_t ship_id;
	uint32_t type;
	uint32_t x, y;
	uint32_t heading;
	uint8_t system;
	uint8_t part; 
	uint8_t damage;
};

struct request_robot_gripper_packet {
	uint8_t opcode;
};

struct demon_move_object_packet {
	uint8_t opcode;
	uint32_t id;
	uint32_t x, y, z;
	union quat q;
};

struct request_mainscreen_view_change {
	uint8_t opcode;
	uint32_t heading_delta;
	uint8_t view_mode;
#define MAINSCREEN_VIEW_MODE_NORMAL 0
#define MAINSCREEN_VIEW_MODE_WEAPONS 1
};

struct opcode_format_descriptor {
	int32_t size;
	uint8_t subcode;
	char format[256];
};

#define DEMON_CONSOLE_MSG_MAX 80

#pragma pack()
#endif	
