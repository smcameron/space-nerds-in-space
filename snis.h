#ifndef __SNIS_H__
#define __SNIS_H__
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

#define SNIS_PROTOCOL_VERSION "SNIS001"

#define XUNIVERSE_DIMENSION 10000.0
#define YUNIVERSE_DIMENSION (XUNIVERSE_DIMENSION * (600.0/800.0))  /* assumes 800x600 screen aspect ratio */
#define MAXGAMEOBJS 5000
#define MAXSPARKS 1000

#define NBASES 5
#define NPLANETS 10
#define NESHIPS 50

#define MAXPLAYERS 10

#define OBJTYPE_SHIP1 9 /* players */
#define OBJTYPE_SHIP2 1 /* computer controlled ships */
#define OBJTYPE_PLANET 2 
#define OBJTYPE_STARBASE 3
#define OBJTYPE_DEBRIS 4
#define OBJTYPE_SPARK 5
#define OBJTYPE_TORPEDO 6
#define OBJTYPE_LASER 7
#define OBJTYPE_EXPLOSION 8

struct ship_data {
	uint32_t torpedoes;
#define TORPEDO_LIFETIME 20
#define TORPEDO_VELOCITY (45.0)
#define TORPEDO_DETONATE_DIST2 (150 * 150)
	uint32_t energy;
	uint32_t shields;
	char shipname[100];
	double velocity;
#define MIN_PLAYER_VELOCITY (0.1)
#define MAX_PLAYER_VELOCITY (30.0)
#define PLAYER_VELOCITY_DAMPING (0.8)
#define PLAYER_VELOCITY_INCREMENT (4.0)
	double yaw_velocity;
	double desired_heading;
	double desired_velocity;
#define MAX_YAW_VELOCITY (5 * PI / 180.0)
#define YAW_INCREMENT (1 * PI / 180.0)
#define YAW_DAMPING 0.85
#define MAX_GUN_YAW_VELOCITY (15 * PI / 180.0)
#define GUN_YAW_INCREMENT (3.5 * PI / 180.0)
#define GUN_YAW_DAMPING 0.45
	double gun_heading;
	double gun_yaw_velocity;
#define MAX_SCI_YAW_VELOCITY (15 * PI / 180.0)
#define SCI_YAW_INCREMENT (3.5 * PI / 180.0)
#define SCI_YAW_DAMPING 0.45
	double sci_heading;
#define MAX_SCI_BW_YAW_VELOCITY (85 * PI / 180.0)
#define SCI_BW_YAW_INCREMENT (1 * PI / 180.0)
#define SCI_BW_YAW_DAMPING 0.45
#define MIN_SCI_BEAM_WIDTH (5 * PI / 180.0)
	double sci_beam_width;
	double sci_yaw_velocity;
	uint8_t torpedoes_loaded;
	uint8_t torpedoes_loading;
	uint16_t torpedo_load_time;
	uint8_t phaser_bank_charge;
	uint32_t fuel;
	uint8_t throttle;
};

struct starbase_data {
	uint32_t sheilds;
};

struct laser_data {
	uint32_t power;
	uint32_t color;
};

struct torpedo_data {
	uint32_t power;
	uint32_t ship_id;
};

struct explosion_data {
	uint16_t nsparks;
	uint16_t velocity;
	uint16_t time;
};

union type_specific_data {
	struct ship_data ship;
	struct laser_data laser;
	struct torpedo_data torpedo;
	struct starbase_data starbase;
	struct explosion_data explosion;
};

struct snis_entity;
typedef void (*move_function)(struct snis_entity *o);

struct snis_entity_science_data {
	char name[20];
	uint16_t science_data_known;
	uint8_t science_data_requested;
	uint8_t subclass;
};
	
struct snis_entity {
	uint32_t index;
	uint32_t id;
	double x, y;
	double vx, vy;
	double heading;
	uint32_t alive;
	uint32_t type;
	uint32_t timestamp;
	union type_specific_data tsd;
	move_function move;
	struct snis_entity_science_data sdata;
};

#endif
