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

/* dimensions of the "known" universe */
#define XKNOWN_DIM 300000.0
#define YKNOWN_DIM (XKNOWN_DIM * (600.0/800.0))  /* assumes 800x600 screen aspect ratio */

#define UNIVERSE_DIM ((XKNOWN_DIM) * 20.0)
#define UNIVERSE_LIMIT (UNIVERSE_DIM / 2.0) /* plus or minus, x, or y, this is as far as you can go. */

#define MAXGAMEOBJS 5000
#define MAXSPARKS 1000

#define NBASES 5
#define NSTARBASE_MODELS 4
#define STARBASE_DOCKING_DIST 100
#define NASTEROID_MODELS 4
#define NASTEROID_SCALES 3
#define NASTEROIDS 200
#define NASTEROID_CLUSTERS 10
#define ASTEROID_CLUSTER_RADIUS 20000
#define ASTEROID_SPEED 200 
#define NESHIPS 50
#define NNEBULA 20
#define NSPACEMONSTERS 3
#define NWORMHOLE_PAIRS 10
#define NEBULA_RADIUS 5000
#define MIN_NEBULA_RADIUS 200

#define NPLANET_MODELS 3
#define NPLANETS 4

#define MAXPLAYERS 10

#define OBJTYPE_SHIP1 9 /* players */
#define OBJTYPE_SHIP2 1 /* computer controlled ships */
#define OBJTYPE_ASTEROID 2
#define OBJTYPE_STARBASE 3
#define OBJTYPE_DEBRIS 4
#define OBJTYPE_SPARK 5
#define OBJTYPE_TORPEDO 6
#define OBJTYPE_LASER 7
#define OBJTYPE_EXPLOSION 8
#define OBJTYPE_NEBULA 10 
#define OBJTYPE_WORMHOLE 11
#define OBJTYPE_SPACEMONSTER 12
#define OBJTYPE_PLANET 13

char *faction[] = {
	"Neutral",
	"Wallunni",
	"Gordouni",
	"Zarkon",
	"Vekkazi",
};

/* Careful, CURRENT / VOLTAGE ratio is twitchy, keep it in the sweet spot
 * MAX_CURRENT between 5 and 10, MAX_VOLTAGE at 1000000.0.
 */	
#define MAX_CURRENT 6.5
#define MAX_VOLTAGE 1000000.0
#define INTERNAL_RESIST 0.000001

struct power_model_device {
	uint8_t r1, r2, r3, i;
};

struct power_model_data {
	struct power_model_device maneuvering;
	struct power_model_device warp;
	struct power_model_device impulse;
	struct power_model_device sensors;
	struct power_model_device comms;
	struct power_model_device phasers;
	struct power_model_device shields;
	uint8_t voltage;
};

#define SHIP_CLASS_CRUISER 0
#define SHIP_CLASS_DESTROYER 1
#define SHIP_CLASS_FREIGHTER 2
#define SHIP_CLASS_TANKER 3
#define SHIP_CLASS_TRANSPORT 4
#define SHIP_CLASS_BATTLESTAR 5
#define SHIP_CLASS_STARSHIP 6
#define SHIP_CLASS_ASTEROIDMINER 7
#define SHIP_CLASS_SCIENCE 8
#define SHIP_CLASS_SCOUT 9

static char *shipclass[] = {
	"Cruiser",
	"Destroyer",
	"Freighter",
	"Tanker",
	"Transport",
	"Battlestar",
	"Starship",
	"Miner",
	"Science",
	"Spacefarer",
};

static double max_speed[] = {
	10.0,
	15.0,
	8.0,
	7.5,
	9.0,
	11.0,
	8.0,
	11.5,
	7.5,
	9.0,
};

struct ship_damage_data {
	uint8_t shield_damage;
	uint8_t impulse_damage;
	uint8_t warp_damage;
	uint8_t torpedo_tubes_damage;
	uint8_t phaser_banks_damage;
	uint8_t sensors_damage;
	uint8_t comms_damage;
};

struct command_data {
	uint8_t command;
	double x, y;
	uint8_t nids1, nids2;
	uint32_t id[256];
};

struct damcon_data;

struct ship_data {
	uint32_t torpedoes;
#define TORPEDO_LIFETIME 40
#define TORPEDO_VELOCITY (90.0)
#define TORPEDO_RANGE (TORPEDO_LIFETIME * TORPEDO_VELOCITY)
#define TORPEDO_DETONATE_DIST2 (150 * 150)
#define LASER_LIFETIME 30
#define LASER_VELOCITY (200.0)
#define LASER_RANGE (LASER_VELOCITY * LASER_LIFETIME)
#define LASER_DETONATE_DIST2 (100 * 100)
	uint32_t power;
	uint32_t shields;
	char shipname[100];
	double velocity;
#define MIN_PLAYER_VELOCITY (0.1)
#define MAX_PLAYER_VELOCITY (30.0)
#define PLAYER_VELOCITY_DAMPING (0.97)
#define PLAYER_VELOCITY_INCREMENT (1.0)
	double yaw_velocity;
	double desired_heading;
	double desired_velocity;
#define MAX_YAW_VELOCITY (5 * PI / 180.0)
#define YAW_INCREMENT (1 * PI / 180.0)
#define YAW_INCREMENT_FINE (0.2 * PI / 180.0)
#define YAW_DAMPING 0.85
#define MAX_GUN_YAW_VELOCITY (15 * PI / 180.0)
#define GUN_YAW_INCREMENT (3.5 * PI / 180.0)
#define GUN_YAW_INCREMENT_FINE (0.5 * PI / 180.0)
#define GUN_YAW_DAMPING 0.1
	double gun_heading;
	double gun_yaw_velocity;
#define MAX_SCI_YAW_VELOCITY (15 * PI / 180.0)
#define SCI_YAW_INCREMENT (3.5 * PI / 180.0)
#define SCI_YAW_INCREMENT_FINE (0.5 * PI / 180.0)
#define SCI_YAW_DAMPING 0.45
	double sci_heading;
#define MAX_SCI_BW_YAW_VELOCITY (85 * PI / 180.0)
#define SCI_BW_YAW_INCREMENT (1 * PI / 180.0)
#define SCI_BW_YAW_INCREMENT_FINE (0.2 * PI / 180.0)
#define SCI_BW_YAW_DAMPING 0.45
#define MIN_SCI_BEAM_WIDTH (5 * PI / 180.0)
#define MAX_SCIENCE_SCREEN_RADIUS (XKNOWN_DIM / 3.0)
#define MIN_SCIENCE_SCREEN_RADIUS (XKNOWN_DIM / 45.0)
#define SCIENCE_SHORT_RANGE (0.08 * XKNOWN_DIM)
	double sci_beam_width;
	double sci_yaw_velocity;
	uint8_t torpedoes_loaded;
	uint8_t torpedoes_loading;
	uint16_t torpedo_load_time;
	uint8_t phaser_bank_charge;
#define FUEL_DURATION (5.0) /* minutes */
#define FUEL_UNITS (FUEL_DURATION * 60.0 * 30.0)
#define FUEL_CONSUMPTION_UNIT ((uint32_t) (UINT_MAX / FUEL_UNITS))
	uint32_t fuel;
	uint8_t rpm;
	uint8_t throttle;
	uint8_t temp;
	uint8_t shiptype; /* same as snis_entity_science_data subclass */
	uint8_t scizoom;
	uint8_t weapzoom;
	uint8_t navzoom;
	uint8_t warpdrive;
	uint8_t requested_warpdrive;
	uint8_t requested_shield;
	uint8_t phaser_wavelength;
	uint8_t phaser_charge;
	int32_t victim;
	double dox, doy; /* destination offsets */
	struct ship_damage_data damage;
	struct command_data cmd_data;
	struct damcon_data *damcon;
	uint8_t view_mode;
	double view_angle;
	struct power_model_data power_data;
	struct power_model *power_model;
	int32_t warp_time; /* time remaining until warp engages */
	double scibeam_a1, scibeam_a2, scibeam_range; /* used server side to cache sci beam calcs */
	uint8_t reverse;
};

struct starbase_data {
	uint32_t sheilds;
	uint8_t under_attack;
	uint32_t last_time_called_for_help;
	char name[16];
};

struct nebula_data {
	double r;
};

struct laser_data {
	uint8_t power;
	uint8_t wavelength;
	uint32_t ship_id;
};

struct torpedo_data {
	uint32_t power;
	uint32_t ship_id;
};

struct explosion_data {
	uint16_t nsparks;
	uint16_t velocity;
	uint16_t time;
	uint8_t victim_type;
};

struct spark_data {
	double z, vz, rx, ry, rz, avx, avy, avz;
};

struct asteroid_data {
	double r; /* distance from center of universe */
	double angle_offset;
};

struct wormhole_data {
	double dest_x, dest_y;
};

#define MAX_SPACEMONSTER_SEGMENTS 20
struct spacemonster_data {
	double zz;
	int front;
	double *x;
	double *y;
	double *z;
	struct entity **entity;
};

union type_specific_data {
	struct ship_data ship;
	struct laser_data laser;
	struct torpedo_data torpedo;
	struct starbase_data starbase;
	struct explosion_data explosion;
	struct nebula_data nebula;
	struct spark_data spark;
	struct asteroid_data asteroid;
	struct wormhole_data wormhole;
	struct spacemonster_data spacemonster;
};

struct snis_entity;
typedef void (*move_function)(struct snis_entity *o);

struct snis_entity_science_data {
	char name[20];
	uint16_t science_data_known;
	uint8_t science_data_requested;
	uint8_t subclass;
	uint8_t shield_strength;
	uint8_t shield_wavelength;
	uint8_t shield_width;
	uint8_t shield_depth;
	uint8_t faction;
};
	
struct snis_entity {
	uint32_t index;
	uint32_t id;
	double x, y, z;
	double vx, vy;
	double heading;
	uint32_t alive;
	uint32_t type;
	uint32_t timestamp;
	uint32_t respawn_time;
	union type_specific_data tsd;
	move_function move;
	struct snis_entity_science_data sdata;
	double sci_coordx; /* selected coords by science station for warp calculations */
	double sci_coordy;
	struct entity *entity;
};

/* These are for the robot and various parts on the engineering deck on the damcon screen */

#define MAXDAMCONENTITIES 200  /* per ship */
struct snis_damcon_entity;

typedef void (*damcon_move_function)(struct snis_damcon_entity *o, struct damcon_data *d);
typedef void (*damcon_draw_function)(void *drawable, struct snis_damcon_entity *o);

#define DAMCON_TYPE_ROBOT 0
#define DAMCON_TYPE_WARPDRIVE 1
#define DAMCON_TYPE_SENSORARRAY 2
#define DAMCON_TYPE_COMMUNICATIONS 3
#define DAMCON_TYPE_NAVIGATION 4
#define DAMCON_TYPE_PHASERBANK 5
#define DAMCON_TYPE_TORPEDOSYSTEM 6
#define DAMCON_TYPE_SHIELDSYSTEM 7
#define DAMCON_TYPE_SOCKET 8
#define DAMCON_TYPE_PART 9

struct damcon_robot_type_specific_data {
	uint32_t cargo_id; /* what the robot is carrying */
#define ROBOT_CARGO_EMPTY 0xffffffff
	double yaw_velocity;
	double desired_velocity, desired_heading;
};

struct damcon_label_specific_data {
	char value[25];
	int font;
};

struct damcon_part_specific_data {
	uint8_t system, part, damage;
};

struct damcon_system_specific_data {
	uint8_t system;
};

struct damcon_socket_specific_data {
	uint8_t system, part;
	uint32_t contents_id; /* id of what socket contains */
#define DAMCON_SOCKET_EMPTY 0xffffffff
};

union damcon_type_specific_data {
	struct damcon_robot_type_specific_data robot;
	struct damcon_label_specific_data label;
	struct damcon_system_specific_data system;
	struct damcon_part_specific_data part;
	struct damcon_socket_specific_data socket;
};

struct snis_damcon_entity {
	uint32_t index;
	uint32_t id;
	uint32_t ship_id; /* ID of ship this entity is on */
	double x, y, velocity, heading;
#define MIN_ROBOT_VELOCITY (0.1)
#define MAX_ROBOT_VELOCITY (25.0)
#define ROBOT_VELOCITY_INCREMENT (0.5)
#define ROBOT_VELOCITY_DAMPING (0.9)
#define MAX_ROBOT_BRAKING 5.0
#define MAX_ROBOT_ACCELERATION 3.5
#define DAMCON_WALL_DIST (50)
	uint32_t type;
	uint32_t timestamp;
	union damcon_type_specific_data tsd;
	damcon_move_function move;
	void *drawing_data;
};

#define ROBOT_MAX_GRIP_DIST2 (40.0 * 40.0)

struct damcon_data {
	struct snis_object_pool *pool;	
	struct snis_damcon_entity o[MAXDAMCONENTITIES];
	struct snis_damcon_entity *robot; /* pointers into o[] */
};

#define DAMCONXDIM 800.0
#define DAMCONYDIM 1800.0

#endif
