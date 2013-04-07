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

#include "snis_marshal.h"

#define OPCODE_UPDATE_SHIP		100
#define OPCODE_UPDATE_STARBASE	101
#define OPCODE_UPDATE_LASER	102
#define OPCODE_UPDATE_TORPEDO	103
#define OPCODE_UPDATE_PLAYER	104
#define OPCODE_ACK_PLAYER	105	
#define OPCODE_ID_CLIENT_SHIP	106
#define OPCODE_UPDATE_PLANET	107
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
#define OPCODE_REQUEST_SHIP_SDATA 119
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

#define OPCODE_POS_SHIP		200
#define OPCODE_POS_STARBASE	201
#define OPCODE_POS_LASER	202
#define OPCODE_NOOP		0xffff

#define NAMESIZE 20

#pragma pack(1)
struct update_ship_packet {
	uint16_t opcode;
	uint32_t id;
	uint32_t alive;
	uint32_t x, y, vx, vy, heading;
        uint32_t ntorpedoes;
        uint32_t power;
	uint32_t gun_heading;
	uint32_t sci_heading;
	uint32_t sci_beam_width;
	uint8_t torpedoes_loaded; /* lower nybble == no. loading, upper == no. loaded. */
	uint8_t throttle;
	uint8_t rpm;
	uint32_t fuel;
	uint8_t temp;
/* next fields must match struct power_dist from snis.h */
        uint8_t maneuvering_slider;
        uint8_t warp_slider;
        uint8_t impulse_slider;
        uint8_t sensors_slider;
        uint8_t comm_slider;
        uint8_t phaserbanks_slider;
	uint8_t shields_slider;
/* end of struct power_dist */
	uint8_t scizoom;
	uint8_t warpdrive;
	uint8_t requested_warpdrive;
	uint8_t requested_shield;
	uint8_t phaser_charge;
	uint8_t phaser_wavelength;
};

struct ship_sdata_packet {
	uint16_t opcode;
	uint32_t id;
	uint8_t subclass;
	uint8_t shield_strength;
	uint8_t shield_wavelength;
	uint8_t shield_width;
	uint8_t shield_depth;
	char name[NAMESIZE];
};

struct request_ship_sdata_packet {
	uint16_t opcode;
	uint32_t id;
};

struct update_econ_ship_packet {
	uint16_t opcode;
	uint32_t id;
	uint32_t alive;
	uint32_t x, y, v, heading;
	uint32_t victim;
};

struct client_ship_id_packet {
	uint16_t opcode;
	uint32_t shipid;
};

struct update_planet_packet {
	uint16_t opcode;
	uint32_t id;
	uint32_t x, y;
};

struct update_starbase_packet {
	uint16_t opcode;
	uint32_t id;
	uint32_t x, y;
};

struct update_explosion_packet {
	uint16_t opcode;
	uint32_t id;
	uint32_t x, y;
	uint16_t nsparks;
	uint16_t velocity;
	uint16_t time;
};

struct add_laser_packet {
	uint16_t opcode;
	uint32_t id;
	uint8_t power;
	uint8_t wavelength;
	struct packed_double x, y;
	struct packed_double vx, vy;
};

struct delete_object_packet {
	uint16_t opcode;
	uint32_t id;
};

struct update_torpedo_packet {
	uint16_t opcode;
	uint32_t id;
	uint32_t ship_oid; /* ship torpedo came from */
	uint32_t x, y;
	uint32_t vx, vy;
}; 

struct update_laser_packet {
	uint16_t opcode;
	uint32_t id;
	uint32_t ship_oid; /* ship laser came from */
	uint32_t x, y;
	uint32_t vx, vy;
}; 

struct request_torpedo_packet {
	uint16_t opcode;
};

struct request_throttle_packet {
	uint16_t opcode;
	uint32_t id;
	uint8_t throttle;
};

struct request_yaw_packet {
	uint16_t opcode;
	uint8_t direction;
#define YAW_LEFT 1
#define YAW_RIGHT 2
};

struct request_thrust_packet {
	uint16_t opcode;
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
#define DISPLAYMODE_DEBUG 6 
#define DISPLAYMODE_DEMON 7 
#define DISPLAYMODE_FONTTEST 8
#define DISPLAYMODE_INTROSCREEN 9
#define DISPLAYMODE_LOBBYSCREEN 10 
#define DISPLAYMODE_CONNECTING 11 
#define DISPLAYMODE_CONNECTED 12 
#define DISPLAYMODE_FINDSERVER 13 
#define DISPLAYMODE_NETWORK_SETUP 14

#define ROLE_MAIN		(1 << DISPLAYMODE_MAINSCREEN)
#define ROLE_NAVIGATION		(1 << DISPLAYMODE_NAVIGATION)
#define ROLE_WEAPONS		(1 << DISPLAYMODE_WEAPONS)
#define ROLE_ENGINEERING	(1 << DISPLAYMODE_ENGINEERING)
#define ROLE_SCIENCE		(1 << DISPLAYMODE_SCIENCE)
#define ROLE_COMMS		(1 << DISPLAYMODE_COMMS)
#define ROLE_DEBUG		(1 << DISPLAYMODE_DEBUG)
#define ROLE_DEMON		(1 << DISPLAYMODE_DEMON)
#define ROLE_SOUNDSERVER	(1 << DISPLAYMODE_FONTTEST)
#define ROLE_ALL		(0xffffffff)

struct add_player_packet {
	uint16_t opcode;
	uint32_t role;
	unsigned char shipname[20];
	unsigned char password[20];
};

struct ack_player_packet {
	uint16_t opcode;
	uint8_t allowed;
};

struct pos_ship_packet {
	uint16_t opcode;
	uint32_t id;
	struct packed_double x, y;
	struct packed_double vx, vy;
	struct packed_double heading;
};

struct pos_starbase_packet {
	uint16_t opcode;
	uint32_t id;
	struct packed_double x, y;
	struct packed_double vx, vy;
	struct packed_double heading;
};

struct pos_laser_packet {
	uint16_t opcode;
	uint32_t id;
	struct packed_double x, y;
};

struct pos_torpedo_packet {
	uint16_t opcode;
	uint32_t id;
	struct packed_double x, y;
}; 

struct play_sound_packet {
	uint16_t opcode;
	uint16_t sound_number;
};

struct role_onscreen_packet {
	uint16_t opcode;
	uint8_t role;
};

struct snis_sci_select_target_packet {
	uint16_t opcode;
	uint32_t target_id;
};

struct snis_sci_select_coords_packet {
	uint16_t opcode;
	uint32_t x;
	uint32_t y;
};

struct ship_damage_packet {
	uint16_t opcode;
	uint32_t id;
/* this must match struct ship_damage_data exactly, and be endian neutral */
	uint8_t shields;
	uint8_t impulse;
	uint8_t warp;
	uint8_t torpedo_tubes;
	uint8_t phaser_banks;
	uint8_t sensors;
	uint8_t comms;
};

struct respawn_time_packet {
	uint16_t opcode;
	uint8_t seconds;
};

struct netstats_packet {
	uint16_t opcode;
	uint64_t bytes_sent;
	uint64_t bytes_recd;
	uint32_t elapsed_seconds;
};

struct comms_transmission_packet {
	uint16_t opcode;
	uint8_t length;
};

struct warp_limbo_packet {
	uint16_t opcode;
	uint16_t limbo_time;
};

#define MAX_DEMON_CMD_IDS 100
struct demon_cmd_packet {
	uint16_t opcode;
	uint8_t demon_cmd;
#define DEMON_CMD_ATTACK 1
	uint32_t x, y;	
	uint8_t nattackerids;
	uint8_t nvictimids;
	uint32_t id[1];
};

#pragma pack()
#endif	
