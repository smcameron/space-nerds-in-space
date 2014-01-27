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
#define YKNOWN_DIM (XKNOWN_DIM * 0.2) /* i guess... */
#define ZKNOWN_DIM (XKNOWN_DIM * (600.0/800.0))  /* assumes 800x600 screen aspect ratio */

#define UNIVERSE_DIM ((XKNOWN_DIM) * 4.0)
#define UNIVERSE_LIMIT (UNIVERSE_DIM / 2.0) /* plus or minus, x, or y, this is as far as you can go. */
#define PROXIMITY_DIST2 (400.0 * 400.0)
#define CRASH_DIST2 (70.0 * 70.0)

#define MAXGAMEOBJS 5000
#define MAXSPARKS 1000

#define NSTARBASE_MODELS 6
#define STARBASE_DOCKING_DIST 600
#define NASTEROID_MODELS 4
#define NASTEROID_SCALES 3
#define NASTEROIDS 200
#define NASTEROID_CLUSTERS 10
#define ASTEROID_CLUSTER_RADIUS 20000
#define ASTEROID_SPEED 10 
#define NESHIPS 250
#define NEBULA_CLUSTERS 4
#define NEBULAS_PER_CLUSTER 10
#define NNEBULA (NEBULA_CLUSTERS * NEBULAS_PER_CLUSTER)
#define NSPACEMONSTERS 3
#define NWORMHOLE_PAIRS 10
#define NEBULA_RADIUS 5000
#define MIN_NEBULA_RADIUS 200
#define NDERELICTS 20

#define NPLANET_MATERIALS 4
#define NPLANETS 10
#define NBASES (NPLANETS + 3)
#define COMMODITIES_PER_BASE 10

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
#define OBJTYPE_LASERBEAM 14
#define OBJTYPE_DERELICT 15
#define OBJTYPE_TRACTORBEAM 16
#define OBJTYPE_CARGO_CONTAINER 17

/* Careful, CURRENT / VOLTAGE ratio is twitchy, keep it in the sweet spot
 * MAX_CURRENT between 5 and 10, MAX_VOLTAGE at 1000000.0.
 */	
#define MAX_CURRENT 6.5
#define MAX_COOLANT 10 
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
	struct power_model_device tractor;
	uint8_t voltage;
};

/*
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
#define SHIP_CLASS_DRAGONHAWK 10 
#define SHIP_CLASS_SKORPIO 11 
#define SHIP_CLASS_DISRUPTOR 12 
#define SHIP_CLASS_RESEARCH_VESSEL 13 
#define SHIP_CLASS_CONQUERER 14 
#define SHIP_CLASS_SCRAMBLER 15 
#define SHIP_CLASS_SWORDFISH 16
#define SHIP_CLASS_WOMBAT 17

struct ship_type_entry {
	char *class;
	double max_speed;
	int crew_max;
} *ship_type;
[] = {
	{ "CRUISER", 10.0, 15 },
	{ "DESTROYER", 15.0, 10 },
	{ "FREIGHTER", 8.0, 10 },
	{ "TANKER", 7.5, 10 },
	{ "TRANSPORT", 9.0, 200 },
	{ "BATTLESTAR", 11.0, 75 },
	{ "STARSHIP", 8.0, 10 },
	{ "MINER", 11.5, 3 },
	{ "SCIENCE", 7.5, 7 },
	{ "SPACEFARER", 9.0, 5 },
	{ "DRAGONHAWK", 12.0, 5 },
	{ "SKORPIO", 14.0, 9 },
	{ "DISRUPTOR", 10.0, 21 },
	{ "RESEARCH", 7.0, 7 },
	{ "CONQUEROR", 10.0, 17 },
	{ "SCRAMBLER", 14.0, 30 },
	{ "SWORDFISH", 14.0, 30 },
	{ "WOMBAT", 14.0, 30 },
};
*/

#define NSHIPTYPES ARRAYSIZE(ship_type)

struct ship_damage_data {
	uint8_t shield_damage;
	uint8_t impulse_damage;
	uint8_t warp_damage;
	uint8_t maneuvering_damage;
	uint8_t phaser_banks_damage;
	uint8_t sensors_damage;
	uint8_t comms_damage;
	uint8_t tractor_damage;
};

struct command_data {
	uint8_t command;
	double x, z;
	uint8_t nids1, nids2;
	uint32_t id[256];
};

struct damcon_data;

/* tentative ai modes... */
#define AI_MODE_IDLE 0
#define AI_MODE_ATTACK 1
#define AI_MODE_TRAVEL 2
#define AI_MODE_FLEE 3
#define AI_MODE_PATROL 4
#define AI_MODE_FLEET_MEMBER 5
#define AI_MODE_FLEET_LEADER 6

/* distance more than which fleet ships will warp back to position rather than simply flying */
#define FLEET_WARP_DISTANCE 5000.0

struct ai_attack_data {
	int32_t victim_id;
};

struct ai_patrol_data {
#define MAX_PATROL_POINTS 5
	uint8_t npoints;
	uint8_t dest;
	union vec3 p[MAX_PATROL_POINTS];
};

struct ai_fleet_data {
	struct ai_patrol_data patrol;
	int fleet;
	int fleet_position;
};

union ai_data {
	struct ai_attack_data attack;
	struct ai_patrol_data patrol;
	struct ai_fleet_data fleet;
};

struct ai_stack_entry {
	uint8_t ai_mode;
	union ai_data u;
};

struct cargo_container_contents {
	int item;
	float qty;
};

struct ship_data {
	uint32_t torpedoes;
#define TORPEDO_LIFETIME 40
#define TORPEDO_LOAD_SECONDS 3
#define TORPEDO_VELOCITY (90.0)
#define TORPEDO_RANGE (TORPEDO_LIFETIME * TORPEDO_VELOCITY)
#define TORPEDO_WEAPONS_FACTOR (3.0)
#define TORPEDO_DETONATE_DIST2 (150 * 150)
#define INITIAL_TORPEDO_COUNT 10
#define LASER_LIFETIME 15
#define LASER_VELOCITY (200.0)
#define LASER_RANGE (LASER_VELOCITY * LASER_LIFETIME)
#define LASER_DETONATE_DIST2 (100 * 100)
#define PATROL_ATTACK_DIST (LASER_RANGE)
#define LASERBEAM_DURATION 5 
#define MINIMUM_ATTACK_SPEED 3.0
#define TRANSPORTER_RANGE 1500.0f

/* Max damage dealt per "hit"
 * laser damage is boosted by increased phaser power and diminished
 * by deflector shields.
 * 
 * There are 2 types of lasers -- "laserbeams", and "lasers"
 * "laserbeams" last for a few seconds, and deal damage 5 times per second.
 * and cannot miss.
 *
 * "lasers" are projectile weapons and deal a single dose of damage
 * 
 * lasers deal boosted damage relative to laserbeams by a factor
 * of LASER_PROJECTILE_BOOST because they are single dose weapons,
 * and because they are harder to use (require aiming).
 */
#define LASER_DAMAGE_MAX (4)
#define LASER_PROJECTILE_BOOST 20.0

	uint32_t power;
	uint32_t shields;
	char shipname[100];
	double velocity;
#define MIN_PLAYER_VELOCITY (0.1)
#define MAX_PLAYER_VELOCITY (30.0)
#define PLAYER_VELOCITY_DAMPING (0.97)
#define PLAYER_VELOCITY_INCREMENT (1.0)
	double yaw_velocity, pitch_velocity, roll_velocity;
	double desired_heading;
	double desired_velocity;
#define MAX_YAW_VELOCITY (5 * PI / 180.0)
#define YAW_INCREMENT (1 * PI / 180.0)
#define YAW_INCREMENT_FINE (0.2 * PI / 180.0)
#define YAW_DAMPING 0.85
#define MAX_PITCH_VELOCITY (5 * PI / 180.0)
#define PITCH_INCREMENT (1 * PI / 180.0)
#define PITCH_INCREMENT_FINE (0.2 * PI / 180.0)
#define PITCH_DAMPING 0.85
#define MAX_ROLL_VELOCITY (5 * PI / 180.0)
#define ROLL_INCREMENT (1 * PI / 180.0)
#define ROLL_INCREMENT_FINE (0.2 * PI / 180.0)
#define ROLL_DAMPING 0.85
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
	union quat sciball_orientation;
	union quat sciball_o1, sciball_o2;
	double sciball_yawvel, sciball_pitchvel, sciball_rollvel;
	union quat weap_orientation, weap_o1, weap_o2;
	double weap_yawvel, weap_pitchvel; /* no roll for weapons */
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
	uint8_t mainzoom;
	uint8_t warpdrive;
	uint8_t requested_warpdrive;
	uint8_t requested_shield;
	uint8_t phaser_wavelength;
	uint8_t phaser_charge;
#define MAX_AI_STACK_ENTRIES 5
	struct ai_stack_entry ai[MAX_AI_STACK_ENTRIES];
	int nai_entries;
	double dox, doy, doz; /* destination offsets */
	struct ship_damage_data damage;
	struct command_data cmd_data;
	struct damcon_data *damcon;
	uint8_t view_mode;
	double view_angle;
	struct power_model_data power_data;
	struct power_model *power_model;
	struct power_model_data coolant_data;
	struct power_model *coolant_model;
	struct ship_damage_data temperature_data;
	int32_t warp_time; /* time remaining until warp engages */
	double scibeam_a1, scibeam_a2, scibeam_range; /* used server side to cache sci beam calcs */
	uint8_t reverse;
	int32_t next_torpedo_time;
#define ENEMY_TORPEDO_FIRE_INTERVAL (4 * 30) /* 4 seconds */ 
	int32_t next_laser_time;
#define ENEMY_LASER_FIRE_INTERVAL (2 * 30) /* 2 seconds */ 
	uint8_t lifeform_count;
#define MAX_TRACTOR_DIST 5000.0 /* TODO: tweak this */
#define TRACTOR_BEAM_IDEAL_DIST 200.0 /* TODO: tweak this */
#define MAX_TRACTOR_VELOCITY 10.0
	uint32_t tractor_beam; 
	uint8_t overheating_damage_done;
	union vec3 steering_adjustment;
	float braking_factor;
#define MAX_CARGO_BAYS_PER_SHIP 8
	struct cargo_container_contents cargo[MAX_CARGO_BAYS_PER_SHIP];
	float cargo_price_paid[MAX_CARGO_BAYS_PER_SHIP];
	int ncargo_bays;
#define INITIAL_WALLET_MONEY (2500.0f)
	float wallet;
};

#define MIN_COMBAT_ATTACK_DIST 200
#define MAX_COMBAT_ATTACK_DIST LASER_RANGE

struct marketplace_data {
	int item;
	float qty;
	float bid;
	float ask;
	float refill_rate;
};

struct starbase_data {
	uint8_t under_attack;
	uint32_t last_time_called_for_help;
	uint8_t lifeform_count;
	char name[16];
	struct marketplace_data *mkt;
	int associated_planet_id;
	float *bid_price;
	float *part_price;
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
	union quat rotational_velocity;
};

struct asteroid_data {
	double r; /* distance from center of universe */
	double angle_offset;
	union quat rotational_velocity;
};

struct cargo_container_data {
	union quat rotational_velocity;
	struct cargo_container_contents contents;
};

struct derelict_data {
	uint8_t shiptype; /* same as snis_entity_science_data subclass */
	union quat rotational_velocity;
};

struct wormhole_data {
	double dest_x, dest_y, dest_z;
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

#define MAX_LASERBEAM_SEGMENTS 32
struct laserbeam_data {
	double *x;
	double *y;
	double *z;
	uint32_t origin;
	uint32_t target;
	uint8_t power;
	uint8_t wavelength;
	struct entity **entity;
};

/* Cribbed from Oolite */
static char *economy_name[] = {
	"Rich Industrial",
	"Average Industrial",
	"Poor Industrial",
	"Mainly Industrial",
	"Mainly Agricultural",
	"Rich Agricultural",
	"Average Agricultural",
	"Poor Agricultural",
};

static char *government_name[] = {
	"Anarchy",
	"Feudal",
	"Multi-Governmental",
	"Dictatorship",
	"Communist",
	"Confederacy",
	"Democracy",
	"Corporate State",
};

/* need to think of something better here... */
__attribute__((unused)) static char *tech_level_name[] = {
	"stone age",
	"iron age",
	"steel age",
	"silicon age",
	"positronic age",
	"space age",
	"interstellar age",
};

struct planet_data {
	uint32_t description_seed;
	uint8_t government;
	uint8_t tech_level;
	uint8_t economy;
#define MIN_PLANET_RADIUS 800.0f
#define MAX_PLANET_RADIUS 2000.0f
#define PLAYER_PLANET_DIST_TOO_CLOSE (200)
#define PLAYER_PLANET_DIST_WARN (400)
	float radius;
	uint8_t ring;
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
	struct laserbeam_data laserbeam;
	struct derelict_data derelict;
	struct cargo_container_data cargo_container;
	struct planet_data planet;
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
	int nupdates;
	double updatetime1;
	double updatetime2;
	uint32_t index;
	uint32_t id;
	union vec3 r1, r2;
	double x, y, z;
	double vx, vy, vz;
	double heading;
	uint32_t alive;
	uint32_t type;
	uint32_t timestamp;
	uint32_t respawn_time;
	union type_specific_data tsd;
	move_function move;
	struct snis_entity_science_data sdata;
	double sci_coordx; /* selected coords by science station for warp calculations */
	double sci_coordz;
	struct entity *entity;
	struct space_partition_entry partition;
	union quat o1, o2;
	union quat orientation;
	char ai[6];
};

/* These are for the robot and various parts on the engineering deck on the damcon screen */

#define MAXDAMCONENTITIES 200  /* per ship */
struct snis_damcon_entity;

typedef void (*damcon_move_function)(struct snis_damcon_entity *o, struct damcon_data *d);
typedef void (*damcon_draw_function)(void *drawable, struct snis_damcon_entity *o);

#define DAMCON_TYPE_SHIELDSYSTEM 0
#define DAMCON_TYPE_IMPULSE 1
#define DAMCON_TYPE_WARPDRIVE 2
#define DAMCON_TYPE_MANEUVERING 3
#define DAMCON_TYPE_PHASERBANK 4
#define DAMCON_TYPE_SENSORARRAY 5
#define DAMCON_TYPE_COMMUNICATIONS 6
#define DAMCON_TYPE_TRACTORSYSTEM 7
#define DAMCON_TYPE_REPAIR_STATION 8
#define DAMCON_TYPE_SOCKET 9
#define DAMCON_TYPE_PART 10 
#define DAMCON_TYPE_ROBOT 11 

struct damcon_robot_type_specific_data {
	uint32_t cargo_id; /* what the robot is carrying */
#define ROBOT_CARGO_EMPTY 0xffffffff
	double yaw_velocity;
	double desired_velocity, desired_heading;
	uint8_t autonomous_mode;
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
#define DAMCONYDIM 2000.0

/* Time after being killed to wait for respawn */
#define RESPAWN_TIME_SECS 20

#endif
