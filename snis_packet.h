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
#define OPCODE_REQUEST_THRUST	109
#define OPCODE_REQUEST_GUNYAW	110
#define OPCODE_REQUEST_TORPEDO	111
#define OPCODE_DELETE_OBJECT    112
#define OPCODE_UPDATE_EXPLOSION 113
#define OPCODE_PLAY_SOUND	114
#define OPCODE_REQUEST_SCIYAW	115
#define OPCODE_REQUEST_SCIBEAMWIDTH	116
#define OPCODE_UPDATE_SHIP2		117
#define OPCODE_ECON_UPDATE_SHIP	118
#define OPCODE_SHIP_SDATA	120
#define OPCODE_LOAD_TORPEDO	121
#define OPCODE_REQUEST_PHASER	122
#define OPCODE_REQUEST_THROTTLE	123
#define OPCODE_REQUEST_MANEUVERING_PWR	124
#define OPCODE_REQUEST_WARP_PWR	125
#define OPCODE_REQUEST_IMPULSE_PWR	126
#define OPCODE_REQUEST_SHIELDS_PWR	127
#define OPCODE_REQUEST_COMMS_PWR	128
#define OPCODE_REQUEST_SENSORS_PWR	129
#define OPCODE_REQUEST_PHASERBANKS_PWR	130
#define OPCODE_REQUEST_SCIZOOM	131
#define OPCODE_REQUEST_WARPDRIVE 132
#define OPCODE_ENGAGE_WARP 133
#define OPCODE_ROLE_ONSCREEN 134
#define OPCODE_SCI_SELECT_TARGET 135
#define OPCODE_UPDATE_DAMAGE 136
#define OPCODE_REQUEST_LASER 137
#define OPCODE_REQUEST_LASER_WAVELENGTH 138
#define OPCODE_SCI_SELECT_COORDS 139
#define OPCODE_REQUEST_SHIELD 140
#define OPCODE_UPDATE_RESPAWN_TIME 141
#define OPCODE_UPDATE_NETSTATS 142
#define OPCODE_COMMS_TRANSMISSION 143
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
#define OPCODE_REQUEST_NAVZOOM		158
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
#define OPCODE_REQUEST_TRACTOR_PWR	183
#define OPCODE_COMMS_MAINSCREEN	184
#define COMMS_CHANNEL_CHANGE_MSG "TX/RX INITIATED ON CHANNEL "
#define OPCODE_LOAD_SKYBOX 185
#define OPCODE_ROBOT_AUTO_MANUAL 	186
#define OPCODE_ADD_WARP_EFFECT		187
#define OPCODE_REQUEST_MAINZOOM		188
#define OPCODE_REQUEST_PITCH		189
#define OPCODE_REQUEST_ROLL		190
#define OPCODE_REQUEST_SCIBALL_YAW	191
#define OPCODE_REQUEST_SCIBALL_PITCH	192
#define OPCODE_REQUEST_SCIBALL_ROLL	193
#define OPCODE_REQUEST_MANUAL_GUNYAW	195
#define OPCODE_REQUEST_MANUAL_GUNPITCH	196
#define OPCODE_REQUEST_MANUAL_LASER	197
#define OPCODE_REQUEST_WEAPONS_YAW_PITCH 198
#define OPCODE_UPDATE_COOLANT_DATA	199
#define OPCODE_REQUEST_MANEUVERING_COOLANT	200
#define OPCODE_REQUEST_WARP_COOLANT	201	
#define OPCODE_REQUEST_IMPULSE_COOLANT	202
#define OPCODE_REQUEST_SHIELDS_COOLANT	203
#define OPCODE_REQUEST_COMMS_COOLANT	204
#define OPCODE_REQUEST_SENSORS_COOLANT	205
#define OPCODE_REQUEST_PHASERBANKS_COOLANT	206
#define OPCODE_REQUEST_TRACTOR_COOLANT	207
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
#define OPCODE_UPDATE_DOCKING_PORT		222
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
#define   OPCODE_ROBOT_SUBCMD_STG		1	/* set short term goal */
#define   OPCODE_ROBOT_SUBCMD_LTG		2	/* set long term goal */
#define OPCODE_TEXTSCREEN_OP			233
#define   OPCODE_TEXTSCREEN_CLEAR		0
#define   OPCODE_TEXTSCREEN_TIMEDTEXT		1
#define   OPCODE_TEXTSCREEN_MENU		2
#define   OPCODE_TEXTSCREEN_MENU_CHOICE		3

#define OPCODE_NOOP		0xff

#define   ADD_PLAYER_ERROR_SHIP_ALREADY_EXISTS	0x01
#define   ADD_PLAYER_ERROR_SHIP_DOES_NOT_EXIST	0x02
#define   ADD_PLAYER_ERROR_TOO_MANY_BRIDGES	0x03
#define   ADD_PLAYER_ERROR_FAILED_VERIFICATION	0x04

#define UPDATE_UNIVERSE_TIMESTAMP_COUNT 5
#define UPDATE_UNIVERSE_TIMESTAMP_START_SAMPLE 1
#define UPDATE_UNIVERSE_TIMESTAMP_SAMPLE 0
#define UPDATE_UNIVERSE_TIMESTAMP_END_SAMPLE 2

#define NAMESIZE 20

#pragma pack(1)
struct update_ship_packet {
	uint8_t opcode;
	uint32_t id;
	uint32_t timestamp;
	uint16_t alive;
	uint32_t x, y, z, yawvel, pitchvel, rollvel;
        uint32_t ntorpedoes;
        uint32_t power;
	uint32_t gunyawvel;
	uint32_t sci_heading;
	uint32_t sci_beam_width;
	uint8_t torpedoes_loaded; /* lower nybble == no. loading, upper == no. loaded. */
	uint8_t throttle;
	uint8_t rpm;
	uint32_t fuel;
	uint8_t temp;
	uint8_t scizoom;
	uint8_t weapzoom;
	uint8_t navzoom;
	uint8_t mainzoom;
	uint8_t warpdrive;
	uint8_t requested_warpdrive;
	uint8_t requested_shield;
	uint8_t phaser_charge;
	uint8_t phaser_wavelength;
	uint8_t shiptype; /* same as ship_sdata_packet subclass */
	uint8_t reverse;
	uint8_t trident;
	uint32_t victim_id;
	int32_t orientation[4]; /* int32_t encoded orientation quaternion */
	int32_t sciball_orientation[4]; /* int32_t encoded orientation quaternion */
	int32_t weap_orientation[4];
	uint8_t in_secure_area;
	uint8_t docking_magnets;
	uint8_t emf_detector;
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
	char name[NAMESIZE];
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
};

struct client_ship_id_packet {
	uint8_t opcode;
	uint32_t shipid;
};

struct update_asteroid_packet {
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
	uint32_t x, y, z;
	uint16_t nsparks;
	uint16_t velocity;
	uint16_t time;
	uint8_t victim_type;
};

struct add_laser_packet {
	uint8_t opcode;
	uint32_t id;
	uint32_t timestamp;
	uint8_t power;
	uint8_t wavelength;
	struct packed_double x, y;
	struct packed_double vx, vy;
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

struct update_laser_packet {
	uint8_t opcode;
	uint32_t id;
	uint32_t timestamp;
	uint32_t ship_oid; /* ship laser came from */
	uint32_t x, y, z;
	uint32_t orientation[4]; /* encoded orientation quaternion */	
}; 

struct update_spacemonster_packet {
	uint8_t opcode;
	uint32_t id;
	uint32_t timestamp;
	uint32_t x, y, z;
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
#define DISPLAYMODE_GLMAIN 8
#define DISPLAYMODE_FONTTEST 9 
#define DISPLAYMODE_INTROSCREEN 10 
#define DISPLAYMODE_LOBBYSCREEN 11 
#define DISPLAYMODE_CONNECTING 12 
#define DISPLAYMODE_CONNECTED 13 
#define DISPLAYMODE_FINDSERVER 14 
#define DISPLAYMODE_NETWORK_SETUP 15

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

struct add_player_packet {
	uint8_t opcode;
	uint8_t new_ship;
	uint8_t warpgate_number;
	uint32_t role;
	unsigned char shipname[20];
	unsigned char password[20];
};

struct ack_player_packet {
	uint8_t opcode;
	uint8_t allowed;
};

struct pos_ship_packet {
	uint8_t opcode;
	uint32_t id;
	struct packed_double x, y;
	struct packed_double vx, vy;
	struct packed_double heading;
};

struct pos_starbase_packet {
	uint8_t opcode;
	uint32_t id;
	struct packed_double x, y;
	struct packed_double vx, vy;
	struct packed_double heading;
};

struct pos_laser_packet {
	uint8_t opcode;
	uint32_t id;
	struct packed_double x, y;
};

struct pos_torpedo_packet {
	uint8_t opcode;
	uint32_t id;
	struct packed_double x, y;
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
};

struct respawn_time_packet {
	uint8_t opcode;
	uint8_t seconds;
};

struct netstats_packet {
	uint8_t opcode;
	uint64_t bytes_sent;
	uint64_t bytes_recd;
	uint32_t nships, nobjects;
	uint32_t elapsed_seconds;
	uint32_t faction_population[5];
};

struct comms_transmission_packet {
	uint8_t opcode;
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

#pragma pack()
#endif	
