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

#include "mesh.h"
#include "material.h"
#include "docking_port.h"
#include "space-part.h"

#define DEFAULT_SOLAR_SYSTEM "default"
#define SNIS_PROTOCOL_VERSION "SNIS001"
#define COMMON_MTWIST_SEED 97872
/* dimensions of the "known" universe */
#define XKNOWN_DIM 600000.0
#define YKNOWN_DIM (XKNOWN_DIM * 0.2) /* i guess... */
#define ZKNOWN_DIM (XKNOWN_DIM)  /* square universe */

#define UNIVERSE_DIM ((XKNOWN_DIM) * 4.0)
#define UNIVERSE_LIMIT (UNIVERSE_DIM / 2.0) /* plus or minus, x, or y, this is as far as you can go. */

#define SUNX (XKNOWN_DIM / 2.0)
#define SUNY (0.0)
#define SUNZ (ZKNOWN_DIM / 2.0)
#define SUN_DIST_LIMIT (XKNOWN_DIM / 20.0)

#define PROXIMITY_DIST2 (100.0 * 100.0)
#define CRASH_DIST2 (40.0 * 40.0)

#define MAXGAMEOBJS 5000
#define MAXSPARKS 5000

#define STARBASE_DOCKING_PERM_DIST 5000
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
#define NWARPGATES 10

#define NPLANETS 6
#define MIN_PLANET_SEPARATION (UNIVERSE_DIM / 10.0)
#define NBASES (NPLANETS + 10)
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
#define OBJTYPE_WARP_EFFECT 18
#define OBJTYPE_SHIELD_EFFECT 19
#define OBJTYPE_DOCKING_PORT 20
#define OBJTYPE_WARPGATE 21

#define SHIELD_EFFECT_LIFETIME 30

#define SNIS_ENTITY_NUPDATE_HISTORY 4

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

/* This thing must contain only endian clean data -- single byte values only */
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
	__extension__ union {
		uint32_t id[256];
		char text[256];
	};
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
#define AI_MODE_HANGOUT 7
#define AI_MODE_COP 8
#define AI_MODE_MINING_BOT 9

/* distance more than which fleet ships will warp back to position rather than simply flying */
#define FLEET_WARP_DISTANCE 5000.0

/* Roughly every 5 minutes, 1 in 10 guys leaves a fleet */
#define FLEET_LEAVE_CHANCE (10 /* ticks */ * 60 /* secs/min */ * 5 /* mins */ * 10/* guys */)

struct ai_attack_data {
	int32_t victim_id;
};

struct ai_patrol_data {
#define MAX_PATROL_POINTS 5
	uint8_t npoints;
	uint8_t dest;
	union vec3 p[MAX_PATROL_POINTS];
};

struct ai_cop_data {
	uint8_t npoints;
	uint8_t dest;
	union vec3 p[MAX_PATROL_POINTS];
};

struct ai_fleet_data {
	struct ai_patrol_data patrol;
	int fleet;
	int fleet_position;
};

struct ai_flee_data {
	int warp_countdown;
};

struct ai_hangout_data {
	uint32_t time_to_go;
};

struct ai_mining_bot_data {
	union quat orbital_orientation;
	uint32_t parent_ship;
	uint32_t asteroid;
	uint16_t countdown;
	uint8_t mode;
#define MINING_MODE_APPROACH_ASTEROID 0
#define MINING_MODE_LAND_ON_ASTEROID 1
#define MINING_MODE_MINE 2
#define MINING_MODE_RETURN_TO_PARENT 3
#define MINING_MODE_STANDBY_TO_TRANSPORT_ORE 4
#define MINING_MODE_STOW_BOT 5
	uint8_t gold;
	uint8_t platinum;
	uint8_t germanium;
	uint8_t uranium;
};

union ai_data {
	struct ai_attack_data attack;
	struct ai_patrol_data patrol;
	struct ai_cop_data cop;
	struct ai_fleet_data fleet;
	struct ai_hangout_data hangout;
	struct ai_flee_data flee;
	struct ai_mining_bot_data mining_bot;
};

struct ai_stack_entry {
	uint8_t ai_mode;
	union ai_data u;
};

#define CARGO_CONTAINER_LIFETIME (4 * 10 * 60) /* 4 minutes */
struct cargo_container_contents {
	int item;
	float qty;
};


struct cargo_bay_info {
	struct cargo_container_contents contents;
	float paid;
	int origin, dest;
	int due_date;
};

#define ATMOSPHERE_DAMAGE_FACTOR (0.1)

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
#define MINING_LASER_DURATION 2 
#define MINIMUM_ATTACK_SPEED 3.0
#define MINIMUM_TURN_SPEED 5.0
#define MAX_SLOW_TURN_ANGLE 2 /* degrees */
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
#define STANDARD_ORBIT_RADIUS_FACTOR (1.1)
#define MIN_PLAYER_VELOCITY (0.1)
#define MAX_PLAYER_VELOCITY (30.0)
#define PLAYER_VELOCITY_DAMPING (0.97)
#define PLAYER_VELOCITY_INCREMENT (1.0)
	double yaw_velocity, pitch_velocity, roll_velocity;
	double desired_velocity;
#define PLAYER_ORIENTATION_DAMPING (0.85)
#define DAMPING_SUPPRESSION_DECAY (0.98)
#define COMPUTER_STEERING_TIME 120.0f
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
	union quat sciball_o[SNIS_ENTITY_NUPDATE_HISTORY];
	double sciball_yawvel, sciball_pitchvel, sciball_rollvel;
	union quat weap_orientation, weap_o[SNIS_ENTITY_NUPDATE_HISTORY];
	double weap_yawvel, weap_pitchvel; /* no roll for weapons */
	uint8_t torpedoes_loaded;
	uint8_t torpedoes_loading;
	uint16_t torpedo_load_time;
	uint8_t phaser_bank_charge;
#define FUEL_DURATION (10.0) /* minutes */
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
	uint8_t trident;
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
	/* struct cargo_container_contents cargo[MAX_CARGO_BAYS_PER_SHIP]; */
	struct cargo_bay_info cargo[MAX_CARGO_BAYS_PER_SHIP];
	int32_t ncargo_bays;
#define INITIAL_WALLET_MONEY (2500.0f)
	float wallet;
#define THREAT_LEVEL_FLEE_THRESHOLD 50.0 /* arrived at empirically */
	float threat_level;
#define MAX_THRUST_PORTS 5
	int nthrust_ports;
	struct entity *thrust_entity[MAX_THRUST_PORTS];
	uint8_t in_secure_area;
	uint8_t auto_respawn;
	uint32_t home_planet;
	int flames_timer;
	uint8_t docking_magnets;
	uint8_t passenger_berths;
	uint8_t mining_bots;
	uint32_t orbiting_object_id;
	char mining_bot_name[20];
	float nav_damping_suppression;
	union quat computer_desired_orientation;
	uint32_t computer_steering_time_left;
};

#define MIN_COMBAT_ATTACK_DIST 200
#define MAX_COMBAT_ATTACK_DIST LASER_RANGE
#define ATTACK_MODE_GIVE_UP_DISTANCE ((XKNOWN_DIM / 10.0) * 0.9)

struct marketplace_data {
	int item;
	float qty;
	float bid;
	float ask;
	float refill_rate;
};

#define STARBASE_FIRE_CHANCE 25 /* ... out of 1000, 10x per sec */
#define STARBASE_SCALE_FACTOR (2.0)
#define STARBASE_DOCK_TIME (1200) /* 2 minutes */
struct starbase_data {
	uint8_t under_attack;
	uint32_t last_time_called_for_help;
	uint8_t lifeform_count;
	uint8_t security;
	char name[16];
	struct marketplace_data *mkt;
	int associated_planet_id;
	float *bid_price;
	float *part_price;
	int nattackers;
	int attacker[5]; /* track up to 5 attackers at once */
	int32_t next_laser_time;
	int32_t next_torpedo_time;
#define STARBASE_LASER_FIRE_INTERVAL (3.2 * 10) /* 3.27 seconds */ 
#define STARBASE_TORPEDO_FIRE_INTERVAL (2.9 * 10) /* 2.9 seconds */ 
	int32_t docking_port[MAX_DOCKING_PORTS];
	int32_t expected_docker[MAX_DOCKING_PORTS];
	int32_t expected_docker_timer[MAX_DOCKING_PORTS];
	int32_t spin_rate_10ths_deg_per_sec;
};

struct nebula_data {
	double r;
	/* avx,avy,avz and ava are components of angular velocity quaternion.
	 * We storethem decomposed into x,y,z,a because we only ever change a
	 * and this saves us decomposing the quaternion every time we need to
	 * change a.
	 */
	float avx, avy, avz, ava;
	union quat unrotated_orientation;
	double phase_angle;
	double phase_speed;
};

struct laser_data {
	uint8_t power;
	uint8_t wavelength;
	uint32_t ship_id;
	union vec3 birth_r;
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
	float shrink_factor;
};

struct asteroid_data {
	double r; /* distance from center of universe */
	double angle_offset;
	union quat rotational_velocity;
	uint8_t carbon;
	uint8_t nickeliron;
	uint8_t silicates;
	uint8_t preciousmetals;
};

struct cargo_container_data {
	union quat rotational_velocity;
	struct cargo_container_contents contents;
};

struct warp_effect_data {
#define WARP_EFFECT_LIFETIME 15
#define WARP_EFFECT_MAX_SIZE 60
	float scale;
	int arriving; /* 1 for arriving, 0 for departing */
};

struct docking_port_data {
	uint32_t parent;
	uint32_t docked_guy;
	uint8_t portnumber;
	uint8_t model; /* which starbase model */
};

struct derelict_data {
	uint8_t shiptype; /* same as snis_entity_science_data subclass */
	union quat rotational_velocity;
	uint8_t persistent;
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
	struct material *material;
	uint8_t mining_laser;
};

struct planet_data {
	uint32_t description_seed;
	uint8_t government;
	uint8_t tech_level;
	uint8_t economy;
#define LOW_SECURITY 0
#define MEDIUM_SECURITY 1
#define HIGH_SECURITY 2
	uint8_t security;
#define MIN_PLANET_RADIUS 800.0f
#define MAX_PLANET_RADIUS 7000.0f
#define SECURITY_RADIUS (MAX_PLANET_RADIUS * 1.5)
#define PLAYER_PLANET_DIST_TOO_CLOSE (200)
#define PLAYER_PLANET_DIST_WARN (400)
	float radius;
	uint8_t planet_type;
#define PLANET_TYPE_ROCKY 0
#define PLANET_TYPE_EARTHLIKE 1
#define PLANET_TYPE_GASGIANT 2
	uint8_t has_atmosphere;
	uint8_t ring_selector;
	uint8_t solarsystem_planet_type;
	uint8_t ring;
	uint8_t atmosphere_r, atmosphere_g, atmosphere_b;
	double atmosphere_scale;
	uint16_t contraband;
	struct entity *atmosphere;
	struct material atm_material;
};

struct warpgate_data {
	uint32_t warpgate_number;
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
	struct warp_effect_data warp_effect;
	struct docking_port_data docking_port;
	struct warpgate_data warpgate;
};

struct snis_entity;
typedef void (*move_function)(struct snis_entity *o);

struct snis_entity_science_data {
	char name[20];
	uint16_t science_data_known;
	uint8_t subclass;
	uint8_t shield_strength;
	uint8_t shield_wavelength;
	uint8_t shield_width;
	uint8_t shield_depth;
	uint8_t faction;
};

struct snis_entity {
	int nupdates;
	double updatetime[SNIS_ENTITY_NUPDATE_HISTORY];
	uint32_t id;
	union vec3 r[SNIS_ENTITY_NUPDATE_HISTORY];
	double x, y, z;
	double vx, vy, vz;
	double heading;
	uint16_t alive;
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
	union quat o[SNIS_ENTITY_NUPDATE_HISTORY];
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
#define DAMCON_TYPE_WAYPOINT 12
/* Threshold beyond which repair requires using the repair station */
#define DAMCON_EASY_REPAIR_THRESHOLD 200

struct damcon_robot_type_specific_data {
	uint32_t cargo_id; /* what the robot is carrying */
#define ROBOT_CARGO_EMPTY 0xffffffff
	double yaw_velocity;
	double desired_velocity, desired_heading;
	uint8_t autonomous_mode;
#define DAMCON_ROBOT_MANUAL_MODE 0
#define DAMCON_ROBOT_FULLY_AUTONOMOUS 1
#define DAMCON_ROBOT_SEMI_AUTONOMOUS 2
	double short_term_goal_x, short_term_goal_y;
	double long_term_goal_x, long_term_goal_y;
	uint8_t robot_state;
#define DAMCON_ROBOT_DECIDE_LTG 3
#define DAMCON_ROBOT_CRUISE 4
#define DAMCON_ROBOT_PICKUP 5
#define DAMCON_ROBOT_REPAIR 6
#define DAMCON_ROBOT_REPLACE 7
#define DAMCON_ROBOT_REPAIR_WAIT 8
	uint32_t damaged_part_id;
	uint32_t repair_socket_id;
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

struct damcon_waypoint_specific_data {
	int n;
	struct snis_damcon_entity *neighbor[10];
};

union damcon_type_specific_data {
	struct damcon_robot_type_specific_data robot;
	struct damcon_label_specific_data label;
	struct damcon_system_specific_data system;
	struct damcon_part_specific_data part;
	struct damcon_socket_specific_data socket;
	struct damcon_waypoint_specific_data waypoint;
};

struct snis_damcon_entity {
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
	unsigned int version;
	union damcon_type_specific_data tsd;
	damcon_move_function move;
	void *drawing_data;
};

#define ROBOT_MAX_GRIP_DIST2 (40.0 * 40.0)

struct damcon_data {
	int bridge;
	struct snis_object_pool *pool;	
	struct snis_damcon_entity o[MAXDAMCONENTITIES];
	struct snis_damcon_entity *robot; /* pointers into o[] */
};
#define damcon_index(data, object) ((object) - &(data)->o[0])

#define DAMCONXDIM 800.0
#define DAMCONYDIM 2000.0

/* Time after being killed to wait for respawn */
#define RESPAWN_TIME_SECS 20

#define FICTIONAL_CLOCK_START (4273.0)
#define FICTIONAL_DATE(timestamp) (FICTIONAL_CLOCK_START + (timestamp) / 1000.0)

#define MAX_PASSENGERS (NBASES * 5)
struct passenger_data {
	char name[50];
	uint32_t location, destination;
	uint32_t fare;
};

#endif
