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

#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <string.h>
#include <sys/time.h>
#include <stdint.h>
#include <pthread.h>
#include <sys/types.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netdb.h>
#include <errno.h>
#include <time.h>
#include <ctype.h>
#include <getopt.h>
#include <math.h>
#include <netinet/tcp.h>
#include <stddef.h>
#include <stdarg.h>
#include <limits.h>
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
#include <assert.h>
#include <locale.h>
#include <dirent.h>
#include <signal.h>
#include <fenv.h>

#include "arraysize.h"
#include "container-of.h"
#include "string-utils.h"
#include "mtwist.h"
#include "build_bug_on.h"
#include "ssgl/ssgl.h"
#include "snis_version.h"
#include "snis_ship_type.h"
#include "snis_faction.h"
#include "space-part.h"
#include "quat.h"
#include "oriented_bounding_box.h"
#include "arbitrary_spin.h"
#define SNIS_SERVER_DATA 1
#include "snis.h"
#undef SNIS_SERVER_DATA
#include "snis-culture.h"
#include "snis_log.h"
#include "mathutils.h"
#include "matrix.h"
#include "snis_alloc.h"
#include "snis_marshal.h"
#include "snis_opcode_def.h"
#include "snis_socket_io.h"
#include "snis_packet.h"
#include "sounds.h"
#include "names.h"
#include "shield_strength.h"
#include "starbase-comms.h"
#include "infinite-taunt.h"
#include "snis_damcon_systems.h"
#include "stacktrace.h"
#include "power-model.h"
#include "snis_event_callback.h"
#include "fleet.h"
#include "commodities.h"
#include "docking_port.h"
#include "build_info.h"
#include "starbase_metadata.h"
#include "elastic_collision.h"
#include "snis_nl.h"
#include "snis_server_tracker.h"
#include "snis_multiverse.h"
#include "snis_hash.h"
#include "snis_bridge_update_packet.h"
#include "solarsystem_config.h"
#include "key_value_parser.h"
#include "a_star.h"
#include "nonuniform_random_sampler.h"
#include "planetary_atmosphere.h"
#include "vertex.h"
#include "mesh.h"
#include "turret_aimer.h"
#include "pthread_util.h"
#include "rts_unit_data.h"
#include "snis_tweak.h"
#include "snis_debug.h"
#include "snis_entity_key_value_specification.h"
#include "snis_cardinal_colors.h"
#include "starmap_adjacency.h"
#include "rootcheck.h"
#include "ship_registration.h"
#include "corporations.h"
#include "replacement_assets.h"
#include "snis_asset_dir.h"
#include "shape_collision.h"
#include "planetary_ring_data.h"
#include "scipher.h"

#define CLIENT_UPDATE_PERIOD_NSECS 500000000
#define MAXCLIENTS 100

/*
 * The following globals are adjustable at runtime via the demon console.
 * See server_builtin_set(), and server_builtin_vars();
 */
static int initial_missile_count = INITIAL_MISSILE_COUNT;
static float missile_max_velocity = DEFAULT_MISSILE_VELOCITY;

	/* TODO: adjusting the following player velocity related variables to
	 * allow high player velocities can cause spurious warp tunnel entities
	 * to be created on the clients. That should be fixed somehow.
	 */
static float max_damcon_robot_velocity = MAX_ROBOT_VELOCITY;
static float max_damcon_robot_braking = MAX_ROBOT_BRAKING;
static float max_player_velocity = MAX_PLAYER_VELOCITY;
static float max_reverse_player_velocity = MAX_REVERSE_PLAYER_VELOCITY;
static float player_velocity_damping = PLAYER_VELOCITY_DAMPING;
static float player_velocity_increment = PLAYER_VELOCITY_INCREMENT;
static float torpedo_damage_factor = TORPEDO_WEAPONS_FACTOR;
static float atmosphere_damage_factor = ATMOSPHERE_DAMAGE_FACTOR;
static float missile_damage_factor = MISSILE_EXPLOSION_WEAPONS_FACTOR;
static float spacemonster_damage_factor = SPACEMONSTER_WEAPONS_FACTOR;
static float warp_core_damage_factor = WARP_CORE_EXPLOSION_WEAPONS_FACTOR;
static int limit_warp_near_planets = 1;
static float warp_limit_dist_factor = 1.5;
static float threat_level_flee_threshold = THREAT_LEVEL_FLEE_THRESHOLD;
static float max_spacemonster_accel = MAX_SPACEMONSTER_ACCEL;
static float max_spacemonster_velocity = MAX_SPACEMONSTER_VELOCITY;
static int torpedo_lifetime = TORPEDO_LIFETIME;
static int missile_lifetime = MISSILE_LIFETIME;
static float torpedo_velocity = TORPEDO_VELOCITY;
static float missile_target_dist = MISSILE_TARGET_DIST;
static float laserbeam_missile_chance = LASERBEAM_MISSILE_CHANCE;
static float max_missile_deltav = MAX_MISSILE_DELTAV;
static float enemy_missile_fire_chance = ENEMY_MISSILE_FIRE_CHANCE;
static float enemy_laser_fire_chance = ENEMY_LASER_FIRE_CHANCE;
static float enemy_torpedo_fire_chance = ENEMY_LASER_FIRE_CHANCE;
static int enemy_laser_fire_interval = ENEMY_LASER_FIRE_INTERVAL;
static int enemy_torpedo_fire_interval = ENEMY_TORPEDO_FIRE_INTERVAL;
static int enemy_missile_fire_interval = ENEMY_MISSILE_FIRE_INTERVAL;
static float missile_proximity_distance = MISSILE_PROXIMITY_DISTANCE;
static float chaff_proximity_distance = CHAFF_PROXIMITY_DISTANCE;
static int chaff_cooldown_time = CHAFF_COOLDOWN_TIME;
static int missile_countermeasure_delay = MISSILE_COUNTERMEASURE_DELAY;
static float missile_explosion_damage_distance = MISSILE_EXPLOSION_DAMAGE_DISTANCE;
static float spacemonster_flee_dist = SPACEMONSTER_FLEE_DIST;
static float spacemonster_aggro_radius = SPACEMONSTER_AGGRO_RADIUS;
static float spacemonster_collision_radius = SPACEMONSTER_COLLISION_RADIUS;
static float cargo_container_max_velocity = CARGO_CONTAINER_MAX_VELOCITY;
static float bounty_chance = BOUNTY_CHANCE;
static float chaff_speed = CHAFF_SPEED;
static int chaff_count = CHAFF_COUNT;
static float chaff_confuse_chance = CHAFF_CONFUSE_CHANCE;
static int multiverse_debug = 0;
static int suppress_starbase_complaints = 0;
static int player_invincibility = 0;
static int game_paused = 0;
#define DEFAULT_LUA_INSTRUCTION_COUNT_LIMIT 100000
static int lua_instruction_count_limit = DEFAULT_LUA_INSTRUCTION_COUNT_LIMIT;
static float bandwidth_throttle_distance = (XKNOWN_DIM / 3); /* How far before object */
						/* updates may be throttled. tweakable */
static int distant_update_period = 20; /* Distant objects are updated only after */
				       /* this many ticks. tweakable */
static int docking_by_faction = 0;
static int npc_ship_count = 250; /* tweakable.  Used by universe regeneration */
static int asteroid_count = 200; /* tweakable.  Used by universe regeneration */

/* Tweakable values for player ship steering speeds */
static float steering_adjust_factor = 1.0;  /* steering adjust factors applies to roll, pitch and yaw */
static float steering_fine_adjust_factor = 0.5;
static float yaw_adjust_factor = 1.0;
static float roll_adjust_factor = 1.0;
static float pitch_adjust_factor = 1.0;
static float yaw_fine_adjust_factor = 1.0;
static float roll_fine_adjust_factor = 1.0;
static float pitch_fine_adjust_factor = 1.0;

static float standard_orbit = 1.1; /* From starfleet technical manual, 1.376, but 1.1 looks better */

static float sci_bw_yaw_increment = SCI_BW_YAW_INCREMENT;
static float sci_bw_yaw_increment_fine = SCI_BW_YAW_INCREMENT_FINE;
static int player_respawn_time = RESPAWN_TIME_SECS; /* Number of seconds player spends dead before being respawned */
static int starbases_orbit = 0; /* 1 means starbases can orbit planets, 0 means starbases are stationary and just hover */
				/* Currently starbases_orbit == 1 kind of breaks the patrol code of NPC ships */
				/* because they do not notice that the starbases move. */

/*
 * End of runtime adjustable globals
 */

static int trap_nans = 0;

static uint32_t ai_trace_id = (uint32_t) -1;

static uint32_t mtwist_seed = COMMON_MTWIST_SEED;

static int lua_enscript_enabled = 0;
static char *initial_lua_script = "initialize.lua";

static struct network_stats netstats;
static int faction_population[5];
static int lowest_faction = 0;

static int nstarbase_models;
static struct starbase_file_metadata *starbase_metadata;
static struct docking_port_attachment_point **docking_port_info;
static struct passenger_data passenger[MAX_PASSENGERS];
static int npassengers;

static struct multiverse_server_info {
	int sock;			/* Socket to the snis_multiverse process */
	uint32_t ipaddr;		/* IP address of snis_multiverse obtained from ssgl_server */
	uint16_t port;			/* listening port of snis_multiverse obtained from ssgl_server */
	pthread_t read_thread;		/* Thread that reads from snis_multiverse */
	pthread_t write_thread;		/* Thread to write to snis_multiverse */
	pthread_cond_t write_cond;	/* Used by thread writing to snis_multiverse */
	struct packed_buffer_queue mverse_queue; /* queue of packets to be sent to snis_multiverse */
	pthread_mutex_t queue_mutex;	/* mutex to protext mverse_queue */
	pthread_mutex_t event_mutex;	/* mutex to protect write_cond */
	pthread_mutex_t exit_mutex;	/* Used to protect writer_time_to_exit and reader_time_to_exit */
	int have_packets_to_xmit;	/* How write_thread knows there are things in the queue to xmit */
	int writer_time_to_exit;	/* How write_thread knows when to exit */
	int reader_time_to_exit;	/* How read_thread knows when to exit */
#define GAMEINSTANCESIZE (sizeof((struct ssgl_game_server *) 0)->game_instance)
	char location[SSGL_LOCATIONSIZE]; /* The "location" string that multiverse registers with ssgl_server */
} *multiverse_server = NULL;

static char *solarsystem_name = DEFAULT_SOLAR_SYSTEM;
static struct solarsystem_asset_spec *solarsystem_assets = NULL;

/* This is used by l_show_menu and process_textscreen_op for user
 * definable menus from lua.
 */
static char *current_user_menu_callback = NULL;

#define GATHER_OPCODE_STATS 0

#if GATHER_OPCODE_STATS

static struct opcode_stat {
	uint8_t opcode;
	uint64_t count;
	uint64_t bytes;
	uint64_t count_not_sent;
} write_opcode_stats[256];

#endif

struct npc_bot_state;
struct npc_menu_item;
typedef void (*npc_menu_func)(struct npc_menu_item *item,
				char *npcname, struct npc_bot_state *botstate);
static void npc_menu_item_not_implemented(struct npc_menu_item *item,
				char *npcname, struct npc_bot_state *botstate);
static void npc_menu_item_travel_advisory(struct npc_menu_item *item,
				char *npcname, struct npc_bot_state *botstate);
static void npc_menu_item_request_dock(struct npc_menu_item *item,
				char *npcname, struct npc_bot_state *botstate);
static void npc_menu_item_warp_gate_tickets(struct npc_menu_item *item,
				char *npcname, struct npc_bot_state *botstate);
static void npc_menu_item_towing_service(struct npc_menu_item *item,
				char *npcname, struct npc_bot_state *botstate);
static void npc_menu_item_buy_cargo(struct npc_menu_item *item,
				char *npcname, struct npc_bot_state *botstate);
static void npc_menu_item_sell_cargo(struct npc_menu_item *item,
				char *npcname, struct npc_bot_state *botstate);
static void npc_menu_item_query_ship_registration(struct npc_menu_item *item,
				char *npcname, struct npc_bot_state *botstate);
static void npc_menu_item_board_passengers(struct npc_menu_item *item,
				char *npcname, struct npc_bot_state *botstate);
static void npc_menu_item_disembark_passengers(struct npc_menu_item *item,
				char *npcname, struct npc_bot_state *botstate);
static void npc_menu_item_eject_passengers(struct npc_menu_item *item,
				char *npcname, struct npc_bot_state *botstate);
static void npc_menu_item_collect_bounties(struct npc_menu_item *item,
				char *npcname, struct npc_bot_state *botstate);
static void npc_menu_item_sign_off(struct npc_menu_item *item,
				char *npcname, struct npc_bot_state *botstate);
static void npc_menu_item_buy_parts(struct npc_menu_item *item,
				char *npcname, struct npc_bot_state *botstate);
static void npc_menu_item_mining_bot_status_report(struct npc_menu_item *item,
				char *npcname, struct npc_bot_state *botstate);
static void npc_menu_item_mining_bot_transport_ores(struct npc_menu_item *item,
				char *npcname, struct npc_bot_state *botstate);
static void npc_menu_item_mining_bot_stow(struct npc_menu_item *item,
				char *npcname, struct npc_bot_state *botstate);
static void npc_menu_item_mining_bot_retarget(struct npc_menu_item *item,
				char *npcname, struct npc_bot_state *botstate);
static void npc_menu_item_mining_bot_cam(struct npc_menu_item *item,
				char *npcname, struct npc_bot_state *botstate);
static void send_to_npcbot(int bridge, char *name, char *msg);

typedef void (*npc_special_bot_fn)(struct snis_entity *o, int bridge, char *name, char *msg);

struct npc_menu_item {
	char *name;
	struct npc_menu_item *parent_menu;
	struct npc_menu_item *submenu;
	npc_menu_func f;
};

/* kinda not happy with the gnarlyness of how this whole menu system works... */
struct npc_bot_state {
	uint32_t object_id;
	uint32_t channel;
	int parts_menu;
	struct npc_menu_item *current_menu;
	npc_special_bot_fn special_bot; /* for special case interactions, non-standard menus, etc. */
};

static struct npc_menu_item repairs_and_fuel_menu[] = {
	/* by convention, first element is menu title */
	{ "REPAIRS AND FUEL MENU", 0, 0, 0 },
	{ "BUY SHIELD SYSTEM PARTS", 0, 0, npc_menu_item_buy_parts },
	{ "BUY IMPULSE DRIVE PARTS", 0, 0, npc_menu_item_buy_parts },
	{ "BUY WARP DRIVE PARTS", 0, 0, npc_menu_item_buy_parts },
	{ "BUY MANEUVERING PARTS", 0, 0, npc_menu_item_buy_parts },
	{ "BUY PHASER BANKS PARTS", 0, 0, npc_menu_item_buy_parts },
	{ "BUY SENSORS PARTS", 0, 0, npc_menu_item_buy_parts },
	{ "BUY COMMUNICATIONS PARTS", 0, 0, npc_menu_item_buy_parts },
	{ "BUY TRACTOR BEAM PARTS", 0, 0, npc_menu_item_buy_parts },
	{ "BUY LIFE SUPPORT SYSTEM PARTS", 0, 0, npc_menu_item_buy_parts },
	{ "BUY FUEL", 0, 0, npc_menu_item_not_implemented },
	{ 0, 0, 0, 0 }, /* mark end of menu items */
};

static struct npc_menu_item cargo_and_passengers_menu[] = {
	/* by convention, first element is menu title */
	{ "CARGO PASSENGERS AND BOUNTIES", 0, 0, 0 },
	{ "BUY CARGO", 0, 0, npc_menu_item_buy_cargo },
	{ "SELL CARGO", 0, 0, npc_menu_item_sell_cargo },
	{ "BOARD PASSENGERS", 0, 0, npc_menu_item_board_passengers },
	{ "DISEMBARK PASSENGERS", 0, 0, npc_menu_item_disembark_passengers },
	{ "EJECT PASSENGERS (FINES APPLY)", 0, 0, npc_menu_item_eject_passengers },
	{ "COLLECT BOUNTIES", 0, 0, npc_menu_item_collect_bounties },
	{ 0, 0, 0, 0 }, /* mark end of menu items */
};

static struct npc_menu_item starbase_main_menu[] = {
	{ "STARBASE MAIN MENU", 0, 0, 0 },  /* by convention, first element is menu title */
	{ "REQUEST PERMISSION TO DOCK", 0, 0, npc_menu_item_request_dock },
	{ "BUY WARP-GATE TICKETS", 0, 0, npc_menu_item_warp_gate_tickets },
	{ "REQUEST TOWING SERVICE", 0, 0, npc_menu_item_towing_service },
	{ "REPAIRS AND FUEL", 0, repairs_and_fuel_menu, 0 },
	{ "CARGO PASSENGERS AND BOUNTIES", 0, cargo_and_passengers_menu, 0 },
	{ "QUERY SHIP REGISTRATIONS", 0, 0, npc_menu_item_query_ship_registration },
	{ "LOCAL TRAVEL ADVISORY", 0, 0, npc_menu_item_travel_advisory },
	{ "SIGN OFF", 0, 0, npc_menu_item_sign_off },
	{ 0, 0, 0, 0 }, /* mark end of menu items */
};

static struct npc_menu_item mining_bot_main_menu[] = {
	{ "MINING BOT MAIN MENU", 0, 0, 0 },  /* by convention, first element is menu title */
	{ "STATUS REPORT", 0, 0, npc_menu_item_mining_bot_status_report },
	{ "RETURN TO SHIP", 0, 0, npc_menu_item_mining_bot_stow },
	{ "TRANSPORT MATERIALS TO CARGO BAYS", 0, 0, npc_menu_item_mining_bot_transport_ores },
	{ "STOW MINING BOT", 0, 0, npc_menu_item_mining_bot_stow },
	{ "RETARGET MINING BOT", 0, 0, npc_menu_item_mining_bot_retarget },
	{ "TOGGLE MINING BOT CAMERA FEED", 0, 0, npc_menu_item_mining_bot_cam },
	{ "SIGN OFF", 0, 0, npc_menu_item_sign_off },
	{ 0, 0, 0, 0 }, /* mark end of menu items */
};


struct snis_entity_client_info {
	uint32_t last_timestamp_sent;
};
struct snis_damcon_entity_client_info {
	unsigned int last_version_sent;
};

static struct game_client {
	int socket;		/* socket to connect snis_client process */
	pthread_t read_thread;	/* thread for reading from snis_client */
	pthread_t write_thread; /* thread for writing to snis_client */
	struct packed_buffer_queue client_write_queue; /* queue of packets to be writtent to snis_client */
	pthread_mutex_t client_write_queue_mutex;	/* mutex to protect client_write_queue */
	uint32_t shipid;	/* ID of player ship for this snis_client */
	uint32_t ship_index;	/* Index into go[] for player ship for this client */
	uint32_t role;		/* bitmap of roles this snis_client has set */
	uint32_t timestamp;	/* The universe_timestamp when this client was last updated, */
				/* see queue_up_client_updates() */
	int bridge;		/* index into bridgelist[] for the bridge this client is on. */
	int debug_ai;		/* Whether or not to transmit NPC ai debug info to this client */
	struct snis_entity_client_info *go_clients; /* ptr to array of size MAXGAMEOBJS. Contains a timestamp */
				/* for each object indicating when that obj was last updated to this client */
	struct snis_damcon_entity_client_info *damcon_data_clients; /* ptr to array of size MAXDAMCONENTITIES */
				/* Contains timestamps for last update time of damcon objs for this client */
	uint8_t refcount; /* count of threads using this client structure. When 0, can delete. see put_client(). */
	int request_universe_timestamp; /* Used for sending timestamp samples to client to gauge latency */
	char *build_info[2]; /* version/endianness etc. of client, see gather_build_info script, string1, and string2 */ 
	uint32_t latency_in_usec; /* network latency, visible via demon screen */
	int waypoints_dirty;	/* trigger update of waypoints to client. If one client updates */
				/* waypoints, all clients need to see that update */
	uint8_t current_station; /* what station (displaymode) client is currently on. Mostly the server doesn't */
				/* need to know this, but uses it for demon serverside builtin commands "clients" */
				/* "lockroles", and "rotateroles" */
#define COMPUTE_AVERAGE_TO_CLIENT_BUFFER_SIZE 0
#if COMPUTE_AVERAGE_TO_CLIENT_BUFFER_SIZE
	uint64_t write_sum;	/* Statistics about data written to client */
	uint64_t write_count;
#endif
} client[MAXCLIENTS];
static int nclients = 0; /* number of entries in client[] that are active */
#define client_index(client_ptr) ((long) ((client_ptr) - &client[0]))
static pthread_mutex_t client_mutex = PTHREAD_MUTEX_INITIALIZER; /* protects client[] array */

static struct bridge_data {
	unsigned char shipname[20];	/* Name of this ship */
	unsigned char password[20];	/* Password for this ship (plaintext) */
	uint32_t shipid;		/* ID of this ship */
	struct damcon_data damcon;	/* All the damage control objects for this ship */
	int last_incoming_fire_sound_time; /* prevents incoming fire warning from sounding too frequently */
	double warpx, warpy, warpz;	/* Holds coordinates related to warp drive. See maybe_do_player_warp() and */
					/* do_engage_warp_drive() */
	union vec3 warpv;		/* warp vector. 1/50th of the distance of the warp in direction of warp */
	int warptimeleft;		/* How many ticks left for warp travel? See maybe_do_player_warp() */
	int comms_channel;		/* Current channel number comms will transmit/recieve on */
	struct npc_bot_state npcbot;	/* Context for NPC comms to maintain conversation */
	int last_docking_permission_denied_time; /* rate limits "docking permission denied" messages */
	uint32_t last_docked_time;		/* Last time docked, limits spurious docking magnet hint */
	uint32_t science_selection;	/* ID of currently selected item on science station */
	int current_displaymode;	/* what the main screen(s) of the bridge are currently showing, */
					/* See process_role_onscreen() */
	struct ssgl_game_server warp_gate_ticket; /* ssgl server entry of the snis_server to which player has */
					/* bought a warp gate ticket. */
	unsigned char pwdhash[PWDHASHLEN]; /* hash of ship name+password+salt, used to identify ships */
					/* to/from multiverse server */
	int verified; /* whether this bridge has verified with multiverse server */
#define BRIDGE_UNVERIFIED 0
#define BRIDGE_VERIFIED 1
#define BRIDGE_FAILED_VERIFICATION 2
#define BRIDGE_REFUSED 3
	int requested_verification; /* Whether we've requested verification from multiverse server yet */
	int requested_creation; /* whether user has requested creating new ship */
	int nclients;			/* Number of connected clients that are part of this bridge */
	struct player_waypoint waypoint[MAXWAYPOINTS]; /* list of science waypoints created for this bridge */
	int selected_waypoint;		/* Currently selected waypoint (or -1 if none) for this bridge */
	int nwaypoints;			/* Number of waypoints created for this bridge */
	int warp_core_critical_timer;	/* Time (ticks) until warp core explodes */
	int warp_core_critical;		/* Is the warp core going to explode? */
	char last_text_to_speech[256];  /* last uttered speech, used to answer "/computer what", See nl_repeat_n() */
	uint32_t text_to_speech_volume_timestamp; /* last time we adjusted computer volume */
	float text_to_speech_volume;	/* Current computer volume */
	uint8_t active_custom_buttons;	/* Number of active custom buttons, see l_enable_custom_button() */
	char custom_button_text[6][16]; /* Text of custom buttons. See queue_up_client_custom_buttons(), */
					/* and l_set_custom_button_label() */
	/* ship id chips are used for collecting bounties */
#define MAX_SHIP_ID_CHIPS 5
	uint32_t ship_id_chip[MAX_SHIP_ID_CHIPS]; /* The ship ID chips players on this bridge have collected */
	int nship_id_chips;		/* Count of ship ID chips, number of ship_id_chip[] entries that are valid */
	char enciphered_message[256];
	char cipher_key[26], guessed_key[26];
	int chaff_cooldown;
} bridgelist[MAXCLIENTS];
static int nbridges = 0;		/* Number of elements present in bridgelist[] */

static pthread_mutex_t universe_mutex = PTHREAD_MUTEX_INITIALIZER; /* main mutex to protect go[] array */

static pthread_mutex_t listener_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t listener_started;
static int listener_port = -1;
static int default_snis_server_port = -1; /* -1 means choose a random port */
static pthread_t lobbythread;
static struct server_tracker *server_tracker;
static int snis_log_level = 2;
static struct ship_type_entry *ship_type;
static int nshiptypes;

/*
 * Starmap entries -- these are locations of star systems
 * as defined in the solarsystem asset files and transmitted
 * via the game_instance data in the lobby -- 1 star system
 * corresponds to 1 server process.  See queue_starmap().
 */
static struct starmap_entry starmap[MAXSTARMAPENTRIES];
static int nstarmap_entries = 0;
static int starmap_dirty = 0;

static struct planetary_ring_data planetary_ring_data[NPLANETARY_RING_MATERIALS];

static int safe_mode = 0; /* prevents enemies from attacking if set */

static char *asset_dir;

static struct replacement_asset replacement_assets;

static int nebulalist[NNEBULA] = { 0 };

static int ncommodities;
static int nstandard_commodities;
static int ncontraband;
static struct commodity *commodity;

static struct mesh *unit_cube_mesh;
static struct mesh *low_poly_sphere_mesh;

static struct snis_object_pool *pool;
static struct snis_entity go[MAXGAMEOBJS];
#define go_index(snis_entity_ptr) ((long) ((snis_entity_ptr) - &go[0]))
static struct space_partition *space_partition = NULL;

static struct ship_registry ship_registry = { 0 };

/* Do planets, black holes, nebula and antenna mis-aiming block comms?  Default is false, disabled */
static int enable_comms_attenuation = 0;

#define MAX_LUA_CHANNELS 10
static struct lua_comms_channel {
	uint32_t channel;
	char *name;
	char *callback;
} lua_channel[MAX_LUA_CHANNELS];
static int nlua_channels = 0;

/* Queue of pending lua comms transmissions */
struct lua_comms_transmission {
	uint32_t channel;
	char *sender;
	char *transmission;
	struct lua_comms_transmission *next;
};

static struct lua_comms_transmission *lua_comms_transmission_queue = NULL;

static uint8_t rts_mode = 0;
static int rts_finish_timer = -1;

static union vec3 rts_main_planet_pos[] = { /* Positions of main RTS planets */
	{ { 120000, -XKNOWN_DIM / 2 + 120000, 120000 } },
	{ { XKNOWN_DIM - 120000, XKNOWN_DIM / 2 - 120000, XKNOWN_DIM - 120000 } },
};

static struct rts_main_planet {
	int index;   /* Index into go[] for corrsponding planet object */
	uint16_t health;
} rts_planet[ARRAYSIZE(rts_main_planet_pos)];

static struct rts_ai_state {
	int active;
	float wallet;
	uint8_t faction;
	uint32_t main_base_id;
} rts_ai;

static struct rts_ai_build_queue_entry {
	int priority; /* lower number means more important */
	int age;
	int unittype;
	uint32_t builder_id;
#define MAX_RTS_BUILD_QUEUE_SIZE 200
} rts_ai_build_queue[MAX_RTS_BUILD_QUEUE_SIZE];
static int rts_ai_build_queue_size;

static void rts_ai_add_to_build_queue(int priority, int unittype, uint32_t builder_id)
{
	int i;

	if (rts_ai_build_queue_size >= MAX_RTS_BUILD_QUEUE_SIZE)
		return;

	for (i = 0; i < rts_ai_build_queue_size; i++) {
		if (rts_ai_build_queue[i].unittype == unittype &&
			rts_ai_build_queue[i].builder_id == builder_id) {
			// rts_ai_build_queue[i].priority = priority; /* Already in queue, just adjust priority */
			return;
		}
	}
	rts_ai_build_queue[rts_ai_build_queue_size].priority = priority;
	rts_ai_build_queue[rts_ai_build_queue_size].unittype = unittype;
	rts_ai_build_queue[rts_ai_build_queue_size].builder_id = builder_id;
	rts_ai_build_queue[rts_ai_build_queue_size].age = 0;
	rts_ai_build_queue_size++;
}

static int rts_ai_build_queue_compare(const void *a, const void *b,
					__attribute__((unused)) void *cookie)
{
	const struct rts_ai_build_queue_entry *entry1 = a;
	const struct rts_ai_build_queue_entry *entry2 = b;
	if (entry1->priority == entry2->priority) /* Same priority, prioritize cheapest one */
		return rts_unit_type(entry1->unittype)->cost_to_build -
				rts_unit_type(entry2->unittype)->cost_to_build;
	return entry1->priority - entry2->priority;
}

static void rts_ai_sort_build_queue(void)
{
	qsort_r(rts_ai_build_queue, rts_ai_build_queue_size,
			sizeof(struct rts_ai_build_queue_entry),
			rts_ai_build_queue_compare, NULL);
}

static void rts_ai_build_queue_remove_entry(int n)
{
	if (n < 0 || n >= rts_ai_build_queue_size)
		return;
	if (n == rts_ai_build_queue_size - 1) {
		rts_ai_build_queue_size--;
		return;
	}
	memmove(&rts_ai_build_queue[n], &rts_ai_build_queue[n + 1],
			sizeof(struct rts_ai_build_queue_entry) * (rts_ai_build_queue_size - n - 1));
	rts_ai_build_queue_size--;
}

static int lookup_by_id(uint32_t id);

static uint32_t get_new_object_id(void)
{
	static uint32_t current_id = 0;
	static uint32_t answer;
	static pthread_mutex_t object_id_lock = PTHREAD_MUTEX_INITIALIZER;

	pthread_mutex_lock(&object_id_lock);
	answer = current_id++;
	pthread_mutex_unlock(&object_id_lock);
	return answer;
}

static inline void client_lock()
{
        (void) pthread_mutex_lock(&client_mutex);
}

static inline void client_unlock()
{
        (void) pthread_mutex_unlock(&client_mutex);
}

/* remove a client from the client array.  Assumes universe and client
 * locks held.
 */
static void remove_client(int client_index)
{
	struct game_client *c;

	assert(client_index >= 0);
	assert(client_index < nclients);

	c = &client[client_index];
	if (c->go_clients) {
		free(c->go_clients);
		c->go_clients = NULL;
	}
	if (c->damcon_data_clients) {
		free(c->damcon_data_clients);
		c->damcon_data_clients = NULL;
	}
	if (client_index == nclients - 1) { /* last in list client, decrement nclients. */
		while (nclients > 0 && client[nclients - 1].refcount == 0)
			nclients--;
		return;
	}
	/* Otherwise, we leave a hole marked by refcount == 0 for later reuse.
	 * we can't (easily) exchange the last item into the hole because the client
	 * reader and writer threads for the last item have a pointer to it which
	 * we currently have no way to update.
	 */
}

/* assumes universe and client locks are held. */
static void delete_bridge(int b);
static void put_client(struct game_client *c)
{
	int bridge_to_delete = -1;
	assert(c->refcount > 0);
	c->refcount--;
	fprintf(stderr, "snis_server: client refcount = %d\n", c->refcount);
	if (c->refcount == 0) {
		if (c->bridge >= 0 && c->bridge < nbridges) {
			bridgelist[c->bridge].nclients--;
			fprintf(stderr, "snis_server: client count of bridge %d = %d\n", c->bridge, bridgelist[c->bridge].nclients);
			if (bridgelist[c->bridge].nclients <= 0) {
				bridge_to_delete = c->bridge;
			}
		}
		fprintf(stderr, "snis_server: calling remove_client %ld\n", client_index(c));
		remove_client(client_index(c));
		fprintf(stderr, "snis_server: remove_client %ld returned\n", client_index(c));
	}
	if (bridge_to_delete >= 0)
		delete_bridge(bridge_to_delete);
}

/* assumes universe and client locks are held. */
static void get_client(struct game_client *c)
{
	c->refcount++;
}

static lua_State *lua_state = NULL;
/* should obtain universe_mutex before touching this queue. */
static struct lua_command_queue_entry {
	struct lua_command_queue_entry *next;
	char lua_command[PATH_MAX];
} *lua_command_queue_head = NULL,  *lua_command_queue_tail = NULL;

static void enqueue_lua_command(const char *cmd)
{
	/* should obtain universe_mutex before touching this queue. */
	struct lua_command_queue_entry *q;

	q = malloc(sizeof(*q));
	q->next = NULL;
	strncpy(q->lua_command, cmd, sizeof(q->lua_command) - 1);
	q->lua_command[sizeof(q->lua_command) - 1] = '\0';

	if (!lua_command_queue_head) {
		lua_command_queue_head = q;
		lua_command_queue_tail = q;
		return;
	}
	lua_command_queue_tail->next = q;
	return;
}

static void dequeue_lua_command(char *cmdbuf, int bufsize)
{
	/* should obtain universe_mutex before touching this queue. */
	struct lua_command_queue_entry *qe;

	qe = lua_command_queue_head;
	if (!qe) {
		strncpy(cmdbuf, "", bufsize - 1);
		return;
	}

	lua_command_queue_head = qe->next;
	qe->next = NULL;
	strncpy(cmdbuf, qe->lua_command, bufsize - 1);
	free(qe);
}

static struct timer_event {
	char *callback;
	uint32_t firetime;
	double cookie_val;
	struct timer_event *next;
} *lua_timer = NULL;

static struct event_callback_entry *event_callback = NULL;
static struct callback_schedule_entry *callback_schedule = NULL;

static struct timer_event *init_lua_timer(const char *callback,
			const double firetime, const double cookie_val,
			struct timer_event *next)
{
	struct timer_event *newone = calloc(sizeof(*newone), 1);

	newone->callback = strdup(callback);
	newone->firetime = firetime;
	newone->cookie_val = cookie_val;
	newone->next = next;
	return newone;
}

static uint32_t universe_timestamp = 1;
static double universe_timestamp_absolute = 0;

/* insert new timer into list, keeping list in sorted order. */
static double register_lua_timer_callback(const char *callback,
		const double timer_ticks, const double cookie_val)
{
	struct timer_event *i, *last, *newone;
	uint32_t firetime = universe_timestamp + (uint32_t) timer_ticks;

	last = NULL;
	for (i = lua_timer; i != NULL; i = i->next) {
		if (i->firetime > firetime)
			break;
		last = i;
	}
	newone = init_lua_timer(callback, firetime, cookie_val, i);
	if (!last)
		lua_timer = newone;
	else
		last->next = newone;
	return 0.0;
}

static void free_timer_event(struct timer_event **e)
{
	free((*e)->callback);
	free(*e);
	*e = NULL;
}

static void free_timer_events(struct timer_event **e)
{
	struct timer_event *i, *next;

	for (i = *e; i != NULL; i = next) {
		next = i->next;
		free_timer_event(&i);
	}
	*e = NULL;
}

static void runaway_lua_script_detected(lua_State *lstate, __attribute__((unused)) lua_Debug *arg)
{
	luaL_error(lstate, "RUNAWAY LUA SCRIPT DETECTED");
}

static void send_demon_console_msg(const char *fmt, ...);
static void send_demon_console_color_msg(uint8_t color, const char *fmt, ...);

static int do_lua_pcall(char *function_name, lua_State *l, int nargs, int nresults, int errfunc)
{
	int rc;

	if (lua_instruction_count_limit > 0)
		lua_sethook(lua_state, runaway_lua_script_detected, LUA_MASKCOUNT, lua_instruction_count_limit);

	rc = lua_pcall(l, nargs, nresults, errfunc);
	if (rc) {
		char errmsg[1000];
		snprintf(errmsg, sizeof(errmsg) - 1, "snis_server: lua callback '%s' had error %d: '%s'.\n",
			function_name, rc, lua_tostring(l, -1));
		send_demon_console_color_msg(YELLOW, "%s", errmsg);
		fprintf(stderr, "%s\n", errmsg);
		stacktrace("do_lua_pcall");
		fflush(stderr);
	}
	if (lua_instruction_count_limit > 0)
		lua_sethook(lua_state, runaway_lua_script_detected, LUA_MASKCOUNT, 0);

	return rc;
}

static void fire_lua_timers(void)
{
	struct timer_event *i, *last, *next;
	struct timer_event *temp_list = NULL;

	last = NULL;
	pthread_mutex_lock(&universe_mutex);
	for (i = lua_timer; i != NULL && i->firetime <= universe_timestamp;) {
		next = i->next;

		/* remove the timer */
		if (last)
			last->next = next;
		else
			lua_timer = next;

		/* add the timer to temp list */
		i->next = temp_list;
		temp_list = i;
		i = next;
	}
	pthread_mutex_unlock(&universe_mutex);

	for (i = temp_list; i != NULL; i = next) {
		/* Call the lua timer function */
		next = i->next;
		lua_getglobal(lua_state, i->callback);
		lua_pushnumber(lua_state, i->cookie_val);
		do_lua_pcall(i->callback, lua_state, 1, 0, 0);
		free_timer_event(&i);
	}

}

static struct lua_proximity_check {
	char *callback;
	uint32_t oid1, oid2;
	double distance2;
	struct lua_proximity_check *next;
} *lua_proximity_check = NULL;

static struct lua_proximity_check *init_lua_proximity_check(const char *callback,
			const uint32_t oid1, const uint32_t oid2, const double distance,
			struct lua_proximity_check *next)
{
	struct lua_proximity_check *newone = calloc(sizeof(*newone), 1);

	newone->callback = strdup(callback);
	newone->oid1 = oid1;
	newone->oid2 = oid2;
	newone->distance2 = distance * distance;
	newone->next = next;
	return newone;
}

/* insert new lua_proximity check into list */
static double register_lua_proximity_callback(const char *callback,
		const uint32_t oid1, const uint32_t oid2, const double distance)
{
	struct lua_proximity_check *i, *last, *newone;

	last = NULL;
	for (i = lua_proximity_check; i != NULL; i = i->next) {
		/* Replace values if callback already exists */
		if (oid1 == i->oid1 && oid2 == i->oid2) {
			if (i->callback)
				free(i->callback);
			i->callback = strdup(callback);
			i->distance2 = distance * distance;
			return 0.0;
		}
		last = i;
	}
	newone = init_lua_proximity_check(callback, oid1, oid2, distance, i);
	if (!last)
		lua_proximity_check = newone;
	else
		last->next = newone;
	return 0.0;
}

static void free_lua_proximity_check(struct lua_proximity_check **e)
{
	free((*e)->callback);
	free(*e);
	*e = NULL;
}

static void free_lua_proximity_checks(struct lua_proximity_check **e)
{
	struct lua_proximity_check *i, *next;

	for (i = *e; i != NULL; i = next) {
		next = i->next;
		free_lua_proximity_check(&i);
	}
	*e = NULL;
}

static void fire_lua_proximity_checks(void)
{
	struct lua_proximity_check *i, *last, *next, *this;
	struct lua_proximity_check *temp_list = NULL;
	double dist2;
	int a, b;

	last = NULL;
	pthread_mutex_lock(&universe_mutex);
	for (i = lua_proximity_check; i != NULL;) {
		next = i->next;

		a = lookup_by_id(i->oid1);
		b = lookup_by_id(i->oid2);
		if (a < 0 || b < 0) {
			/* remove this check */
			if (last) {
				this = last->next;
				last->next = next;
			} else {
				this = lua_proximity_check;
				lua_proximity_check = next;
			}
			free_lua_proximity_check(&this);
			continue;
		}

		dist2 = object_dist2(&go[a], &go[b]);
		if (dist2 <= i->distance2) { /* add the proximity check to temp list */
			/* Remove the proximity check */
			if (last)
				last->next = next;
			else
				lua_proximity_check = next;
			/* Add the check to a temporary list */
			i->next = temp_list;
			temp_list = i;
		}
		i = next;
	}
	pthread_mutex_unlock(&universe_mutex);

	/* Call all the lua proximity check callback functions in the temporary list  */
	for (i = temp_list; i != NULL; i = next) {
		next = i->next;
		lua_getglobal(lua_state, i->callback);
		lua_pushnumber(lua_state, (double) i->oid1);
		lua_pushnumber(lua_state, (double) i->oid2);
		lua_pushnumber(lua_state, (double) sqrt(dist2));
		do_lua_pcall(i->callback, lua_state, 3, 0, 0);
		free_lua_proximity_check(&i);
	}
}

/* This sends one comms transmission to any lua callback listening on the channel
 * This is for lua scripts to recieve transmissions from players.  This should only
 * be called from the main thread that started lua.
 */
static void send_one_lua_comms_transmission(struct lua_comms_transmission *transmission)
{
	int i;

	if (!transmission || !transmission->sender || !transmission->transmission) {
		fprintf(stderr, "send_one_lua_comms_transmission: something is not right.\n");
		return;
	}
	for (i = 0; i < nlua_channels; i++) {
		if (transmission->channel != lua_channel[i].channel)
			continue;
		lua_getglobal(lua_state, lua_channel[i].callback);
		lua_pushstring(lua_state, transmission->sender);
		lua_pushnumber(lua_state, (double) lua_channel[i].channel);
		lua_pushstring(lua_state, transmission->transmission);
		do_lua_pcall(lua_channel[i].callback, lua_state, 3, 0, 0);
	}
}

/* This sends comms transmission to any lua callback listening on the channel
 * This is for lua scripts to recieve transmissions from players
 */
static void send_queued_lua_comms_transmissions(struct lua_comms_transmission **transmission_queue)
{
	struct lua_comms_transmission *t, *next = NULL;

	for (t = *transmission_queue; t != NULL; t = next) {
		send_one_lua_comms_transmission(t);
		next = t->next;
		if (t->sender)
			free(t->sender);
		if (t->transmission)
			free(t->transmission);
		t->sender = NULL;
		t->transmission = NULL;
		free(t);
		t = NULL;
	}
	*transmission_queue = NULL;
}

static void fire_lua_callbacks(struct callback_schedule_entry **sched)
{
	struct callback_schedule_entry *i, *next;
	char *callback;
	int j;

	for (i = *sched; i != NULL; i = next) {
		next = next_scheduled_callback(i);
		callback = callback_name(i);
		lua_getglobal(lua_state, callback);
		for (j = 0; j < MAX_LUA_CALLBACK_PARAMS; j++)
			lua_pushnumber(lua_state, callback_schedule_entry_param(i, j));
		do_lua_pcall(callback, lua_state, MAX_LUA_CALLBACK_PARAMS, 0, 0);
		free(callback);
	}
	free_callback_schedule(sched);
}

static void set_object_location(struct snis_entity *o, double x, double y, double z);
static void normalize_coords(struct snis_entity *o)
{
	/* This algorihm is like that big ball in "The Prisoner". */

	if (o->x < -UNIVERSE_LIMIT)
		goto fixit;
	if (o->x > UNIVERSE_LIMIT)
		goto fixit;
	if (o->z < -UNIVERSE_LIMIT)
		goto fixit;
	if (o->z > UNIVERSE_LIMIT)
		goto fixit;
	if (o->y < -UNIVERSE_LIMIT)
		goto fixit;
	if (o->y > UNIVERSE_LIMIT)
		goto fixit;
	return;
fixit:
	set_object_location(o, snis_randn(XKNOWN_DIM), snis_randn(YKNOWN_DIM), snis_randn(ZKNOWN_DIM));
}

static void set_object_location(struct snis_entity *o, double x, double y, double z)
{
	/* NaNs in NPC ship navigation have been enough of a problem that we should
	 * just always monitor for them.
	 */
	if (isnan(o->x) || isnan(o->y) || isnan(o->z)) {
		send_demon_console_color_msg(ORANGERED,
			"NaN DETECTED AT %s:%d:SET_OBJECT_LOCATION() x,y,z = %lf,%lf,%lf!",
			__FILE__, __LINE__, x, y, z);
		fprintf(stderr,
			"NaN DETECTED AT %s:%d:SET_OBJECT_LOCATION() x,y,z = %lf,%lf,%lf!\n",
			__FILE__, __LINE__, x, y, z);
	}
	o->x = x;
	o->y = y;
	o->z = z;
	normalize_coords(o);
	space_partition_update(space_partition, o, x, z);
}

static void set_object_velocity(struct snis_entity *o, double vx, double vy, double vz)
{
	o->vx = vx;
	o->vy = vy;
	o->vz = vz;
}

static void get_peer_name(int connection, char *buffer)
{
	struct sockaddr_in *peer;
	struct sockaddr p = { 0 };
	socklen_t addrlen = sizeof(p);
	int rc;

	peer = (struct sockaddr_in *) &p;
	memset(peer, 0, addrlen); /* Redundant, but shuts up clang scan-build. */

	/* This seems to get the wrong info... not sure what's going wrong */
	/* Get the game server's ip addr (don't trust what we were told.) */
	rc = getpeername(connection, (struct sockaddr * __restrict__) &p, &addrlen); 
	if (rc != 0) {
		/* this happens quite a lot, so SSGL_INFO... */
		snis_log(SNIS_INFO, "getpeername failed: %s\n", strerror(errno));
		sprintf(buffer, "(UNKNOWN)");
		return;
	}
	sprintf(buffer, "%s:%hu", inet_ntoa(peer->sin_addr), ntohs(peer->sin_port));
}

static char *logprefixstr = NULL;

static char *logprefix(void)
{
	static char logprefixstrbuffer[100];
	if (logprefixstr != NULL)
		return logprefixstr;
	snprintf(logprefixstrbuffer, sizeof(logprefixstrbuffer), "%s(%s):", "snis_server", solarsystem_name);
	logprefixstr = logprefixstrbuffer;
	return logprefixstr;
}

static void log_client_info(__attribute__((unused)) int level, int connection, char *info)
{
	char client_ip[50];

	if (level < snis_log_level)
		return;

	memset(client_ip, 0, sizeof(client_ip));
	get_peer_name(connection, client_ip);
	fprintf(stderr, "%s: %s: %s", logprefix(), client_ip, info);
}

static void delete_from_clients_and_server_helper(struct snis_entity *o, int take_client_lock);
static void delete_from_clients_and_server(struct snis_entity *o);
static void delete_object(struct snis_entity *o);
static void remove_from_attack_lists(uint32_t victim_id);

static void delete_bridge(int b)
{
	/* Assumes universe mutex is held.
	 * Assumes client lock held.
	 */

	int i;
	int clients_still_active = 0;

	fprintf(stderr, "snis_server: delete_bridge %d, nbridges = %d\n", b, nbridges);
	if (nbridges <= 0)
		return;
	for (i = 0; i < nclients; i++) {
		if (!client[i].refcount)
			continue;
		if (client[i].bridge == b)
			clients_still_active = 1;
	}
	fprintf(stderr, "snis_server: delete_bridge, clients_still_active = %d\n", clients_still_active);
	if (clients_still_active) {
		fprintf(stderr, "%s: attempted to delete bridge clients still uses.\n",
			logprefix());
		return;
	}
	fprintf(stderr, "snis_server: deleting player ship\n");
	for (i = 0; i <= snis_object_pool_highest_object(pool); i++) {
		if (go[i].type == OBJTYPE_SHIP1 && go[i].id == bridgelist[b].shipid) {
			delete_from_clients_and_server_helper(&go[i], 0); /* 0 because client lock is already held. */
			break;
		}
	}
	fprintf(stderr, "snis_server: deleting bridge %d\n", b);
	/* delete the bridge */
	for (i = b + 1; i < nbridges; i++)
		bridgelist[i - 1] = bridgelist[i];
	nbridges--;
	fprintf(stderr, "%s: deleted bridge %d\n", logprefix(), b);
}

static void print_hash(char *s, unsigned char *pwdhash)
{
	int i;

	fprintf(stderr, "%s: %s", logprefix(), s);
	for (i = 0; i < PWDHASHLEN; i++)
		fprintf(stderr, "%02x", pwdhash[i]);
	fprintf(stderr, "\n");
}

static int lookup_bridge_by_pwdhash(unsigned char *pwdhash)
{
	/* assumes universe_mutex held */
	int i;

	for (i = 0; i < nbridges; i++)
		if (memcmp(bridgelist[i].pwdhash, pwdhash, PWDHASHLEN) == 0)
			return i;
	return -1;
}

static void generic_move(__attribute__((unused)) struct snis_entity *o)
{
	return;
}

static int add_ship(int faction, int shiptype, int auto_respawn);
static void add_new_rts_unit(struct snis_entity *builder)
{
	float dx, dy, dz;
	struct snis_entity *unit;
	uint8_t unit_type;
	int unit_number;
	int i, shiptype;

	/* Figure out where to add the new unit */
	if (builder->type == OBJTYPE_PLANET) {
		random_point_on_sphere(builder->tsd.planet.radius * 1.3 + 400.0f +
					snis_randn(400), &dx, &dy, &dz);
		unit_type = builder->tsd.planet.build_unit_type;
	} else if (builder->type == OBJTYPE_STARBASE) {
		random_point_on_sphere(400.0f + snis_randn(400), &dx, &dy, &dz);
		unit_type = builder->tsd.starbase.build_unit_type;
	} else {
		fprintf(stderr, "Unexpected builder type %u\n", builder->type);
		return;
	}
	shiptype = rts_unit_type_to_ship_type(unit_type);
	if (shiptype < 0) {
		fprintf(stderr, "Did not figure out what model of ship to use.\n");
		return;
	}
	dx += builder->x;
	dy += builder->y;
	dz += builder->z;
	i = add_ship(builder->sdata.faction, shiptype, 0);
	if (i < 0) {
		fprintf(stderr, "add_ship failed.\n");
		return;
	}
	unit = &go[i];
	unit->x = dx;
	unit->y = dy;
	unit->z = dz;
	unit->vx = 0;
	unit->vy = 0;
	unit->vz = 0;
	unit->tsd.ship.nai_entries = 1;
	unit->tsd.ship.ai[0].ai_mode = AI_MODE_RTS_STANDBY;
	unit->tsd.ship.ai[0].u.occupy_base.base_id = (uint32_t) -1;

	/* Give the unit a short name based on the type, ie. a scout becomes, eg. "S5". */
	unit_number = rts_allocate_unit_number(unit_type, builder->sdata.faction);
	snprintf(unit->sdata.name, sizeof(unit->sdata.name), "%s%d",
			rts_unit_type(unit_type)->short_name_prefix, unit_number);
	unit->tsd.ship.fuel = rts_unit_type(unit_type)->fuel_capacity;
	fprintf(stderr, "Added ship successfully\n");
}

static int add_blackhole_explosion(uint32_t related_id, double x, double y, double z, uint16_t velocity,
				uint16_t nsparks, uint16_t time, uint8_t victim_type);
static void snis_queue_add_sound(uint16_t sound_number, uint32_t roles, uint32_t shipid);

static void black_hole_collision_detection(void *o1, void *o2)
{
	struct snis_entity *black_hole = o1;
	struct snis_entity *object = o2;
	double dist;

	if (o1 == o2)
		return;
	if (!object->alive)
		return;
	if (object->type == OBJTYPE_EXPLOSION)
		return;
	if (object->type == OBJTYPE_NEBULA)
		return;
	dist = object_dist(black_hole, object);
	if (dist > BLACK_HOLE_INFLUENCE_LIMIT * black_hole->tsd.black_hole.radius)
		return;
	if (dist < BLACK_HOLE_EVENT_HORIZON * black_hole->tsd.black_hole.radius) {
		if (object->type != OBJTYPE_SHIP1) {
			(void) add_blackhole_explosion(black_hole->id,
						black_hole->x, black_hole->y, black_hole->z,
						500, 100, 100, object->type);
			schedule_callback3(event_callback, &callback_schedule,
					"black-hole-consumed-object-event", (double) black_hole->id,
					(double) object->id, (double) object->type);
			delete_from_clients_and_server(object);
			return;
		} else if (!player_invincibility) {
			object->alive = 0;
			object->respawn_time = universe_timestamp + player_respawn_time * 10;
			object->timestamp = universe_timestamp;
			snis_queue_add_sound(EXPLOSION_SOUND,
				ROLE_SOUNDSERVER, object->id);
			schedule_callback3(event_callback, &callback_schedule,
					"black-hole-consumed-object-event", (double) black_hole->id,
					(double) object->id, (double) object->type);
			schedule_callback(event_callback, &callback_schedule,
					"player-death-callback", object->id);
		}
		return;
	} else {
		/* in the zone of influence, but not dead yet */
		union vec3 towards_black_hole;

		towards_black_hole.v.x = black_hole->x - object->x;
		towards_black_hole.v.y = black_hole->y - object->y;
		towards_black_hole.v.z = black_hole->z - object->z;
		vec3_normalize_self(&towards_black_hole);
		vec3_mul_self(&towards_black_hole, BLACK_HOLE_VFACTOR);
		vec3_mul_self(&towards_black_hole, 1.0 / (dist + 1.0));
		object->vx += towards_black_hole.v.x;
		object->vy += towards_black_hole.v.y;
		object->vz += towards_black_hole.v.z;
		set_object_location(object, object->x + object->vx, object->y + object->vy, object->z + object->vz);
		object->timestamp = universe_timestamp;
	}
}

static void black_hole_move(struct snis_entity *o)
{
	set_object_location(o, o->x + o->vx, o->y + o->vy, o->z + o->vy);
	o->vx *= 0.95;
	o->vy *= 0.95;
	o->vz *= 0.95;
	space_partition_process(space_partition, o, o->x, o->z, o,
				black_hole_collision_detection);
	return;
}

static void planet_move(struct snis_entity *o)
{
	if (!rts_mode)
		return;
	if (o->tsd.planet.time_left_to_build == 0)
		return;
	o->tsd.planet.time_left_to_build--;
	o->timestamp = universe_timestamp;
	if (o->tsd.planet.time_left_to_build == 0 && o->tsd.planet.build_unit_type != 255) /* Unit is completed? */
		add_new_rts_unit(o);
}

static void asteroid_move(struct snis_entity *o)
{
	set_object_location(o, o->x + o->vx * o->tsd.asteroid.v,
				o->y + o->vy * o->tsd.asteroid.v,
				o->z + o->vz * o->tsd.asteroid.v);
	o->timestamp = universe_timestamp;

	if (((o->timestamp + o->id) & 0x07) == 0) { /* throttle this calculation */
		union vec3 from_center, dir, dirn;
		union vec3 up = { { 0.0f, 1.0f, 0.0f } };

		from_center.v.x = o->x - XKNOWN_DIM / 2.0f;
		from_center.v.y = o->y - YKNOWN_DIM / 2.0f;
		from_center.v.z = o->z - ZKNOWN_DIM / 2.0f;

		vec3_cross(&dir, &from_center, &up);
		vec3_normalize(&dirn, &dir);
		vec3_mul(&dir, &dirn, ASTEROID_SPEED);
		o->vx = dir.v.x;
		o->vy = dir.v.y;
		o->vz = dir.v.z;
	}
	compute_arbitrary_spin(30, universe_timestamp, &o->orientation,
				&o->tsd.asteroid.rotational_velocity);
}

static void calculate_torpedolike_damage(struct snis_entity *target, double weapons_factor);
static void calculate_warp_core_explosion_damage(struct snis_entity *target, double damage_factor)
{
	calculate_torpedolike_damage(target, warp_core_damage_factor * damage_factor);
}

static void calculate_missile_explosion_damage(struct snis_entity *target, double damage_factor)
{
	if (target->type == OBJTYPE_SHIP1) /* Nerf the missiles against player ships */
		damage_factor = damage_factor * MISSILE_NERF_FACTOR;
	calculate_torpedolike_damage(target, missile_damage_factor * damage_factor);
}

static void send_ship_damage_packet(struct snis_entity *o);
static void check_warp_core_explosion_damage(struct snis_entity *warp_core,
						struct snis_entity *object)
{
	double dist;
	double damage_factor;

	switch (object->type) {
	case OBJTYPE_SHIP1:
	case OBJTYPE_SHIP2:
		dist = object_dist(warp_core, object);
		if (dist > WARP_CORE_EXPLOSION_DAMAGE_DISTANCE)
			return;
		damage_factor = WARP_CORE_EXPLOSION_DAMAGE_DISTANCE / (dist + 1.0);
		calculate_warp_core_explosion_damage(object, damage_factor);
		send_ship_damage_packet(object);
		break;
	default:
		break;
	}
}

static void do_detailed_collision_impulse(struct snis_entity *o1, struct snis_entity *o2, /* objects */
						float m1, float m2, /* masses */
						float r1, float r2, /* radii */
						float energy_transmission_factor);

static void warp_core_collision_detection(void *o1, void *o2)
{
	struct snis_entity *warp_core = o1;
	struct snis_entity *object = o2;
	union vec3 core_pos, obj_pos, closest_point, displacement, normal_vector;
	double dist2;

	if (warp_core->tsd.warp_core.countdown_timer == 0)
		check_warp_core_explosion_damage(warp_core, object);
	if (object->type != OBJTYPE_BLOCK)
		return;
	dist2 = object_dist2(warp_core, object);
	if (dist2 > object->tsd.block.shape.overall_radius * object->tsd.block.shape.overall_radius)
		return;

	core_pos.v.x = warp_core->x;
	core_pos.v.y = warp_core->y;
	core_pos.v.z = warp_core->z;
	obj_pos.v.x = object->x;
	obj_pos.v.y = object->y;
	obj_pos.v.z = object->z;

	shape_closest_point(&core_pos, &obj_pos, &object->orientation,
				&object->tsd.block.shape, &closest_point, &normal_vector);

	dist2 = dist3dsqrd(warp_core->x - closest_point.v.x,
				warp_core->y - closest_point.v.y,
				warp_core->z - closest_point.v.z);
	if (dist2 > 8.0 * 8.0)
		return;
	warp_core->vx = 0;
	warp_core->vy = 0;
	warp_core->vz = 0;

	displacement.v.x = warp_core->x - closest_point.v.x;
	displacement.v.y = warp_core->y - closest_point.v.y;
	displacement.v.z = warp_core->z - closest_point.v.z;
	vec3_normalize_self(&displacement);
	vec3_mul_self(&displacement, 30.0);
	warp_core->x += displacement.v.x;
	warp_core->y += displacement.v.y;
	warp_core->z += displacement.v.z;
	do_detailed_collision_impulse(warp_core, object, 1.0, 1e6, 10, 100, 0.5);
	return;
}

static int add_explosion(double x, double y, double z, uint16_t velocity,
				uint16_t nsparks, uint16_t time, uint8_t victim_type);

static void delete_from_clients_and_server(struct snis_entity *o);
static void warp_core_move(struct snis_entity *o)
{
	set_object_location(o, o->x + o->vx, o->y + o->vy, o->z + o->vz);
	o->timestamp = universe_timestamp;
	o->tsd.warp_core.countdown_timer--;
	space_partition_process(space_partition, o, o->x, o->z, o,
				warp_core_collision_detection);
	compute_arbitrary_spin(30, universe_timestamp, &o->orientation,
				&o->tsd.warp_core.rotational_velocity);
	if (o->tsd.warp_core.countdown_timer == 0) {
		(void) add_explosion(o->x, o->y, o->z, 150, 350, 250, o->type);
		snis_queue_add_sound(EXPLOSION_SOUND, ROLE_SOUNDSERVER, o->tsd.warp_core.ship_id);
		o->alive = 0;
		delete_from_clients_and_server(o);
	}
}

static void cargo_container_move(struct snis_entity *o)
{
	float v;

	set_object_location(o, o->x + o->vx, o->y + o->vy, o->z + o->vz);

	/* Make sure the player can catch the cargo containers */
	v = dist3d(o->vx, o->vy, o->vz);
	if (v > cargo_container_max_velocity) {
		o->vx *= 0.95;
		o->vy *= 0.95;
		o->vz *= 0.95;
	}

	o->timestamp = universe_timestamp;
	if (o->tsd.cargo_container.persistent)
		return;
	if (o->alive)
		o->alive--;
	if (o->alive == 0)
		delete_from_clients_and_server(o);
}

static void derelict_collision_detection(void *derelict, void *object)
{
	struct snis_entity *o = object;
	struct snis_entity *d = derelict;
	if (o->type == OBJTYPE_SHIP1) /* If some player ship is nearby, do not cull. */
		d->alive = 1;
}

static void derelict_move(struct snis_entity *o)
{
	set_object_location(o, o->x + o->vx, o->y + o->vy, o->z + o->vz);
	o->timestamp = universe_timestamp;

	/* Unrealistic, but slow derelicts so the mining bot can catch them */
	float v = sqrtf(o->vx * o->vx + o->vy * o->vy + o->vz * o->vz);
	if (v > ship_type[SHIP_CLASS_ASTEROIDMINER].max_speed * 0.8) {
		o->vx *= 0.99;
		o->vy *= 0.99;
		o->vz *= 0.99;
	}

	if (o->tsd.derelict.persistent)
		return;

	if (o->alive > 1)
		o->alive--;

	/* Cull distant derelicts so they don't overpopulate the server */
	if (o->alive == 1 &&
		(universe_timestamp & 0x03f) == (o->id & 0x03f)) { /* throttle computation */
		float dist = dist3d(o->x - XKNOWN_DIM / 2.0,
					o->y - YKNOWN_DIM / 2.0, o->z - ZKNOWN_DIM / 2.0);
		if (dist > (1.15 * XKNOWN_DIM / 2.0)) {
			o->alive = 0;
			delete_from_clients_and_server(o);
			return;
		}
		o->alive = 0; /* assume it is time to die and nobody's nearby */
		/* check if anybody's nearby ... */
		space_partition_process(space_partition, o, o->x, o->z, o,
					derelict_collision_detection);
	}
	if (!o->alive) {
		delete_from_clients_and_server(o);
		return;
	}
}

static void send_wormhole_limbo_packet(int shipid, uint16_t value);
static void wormhole_collision_detection(void *wormhole, void *object)
{
	struct snis_entity *t, *o;
	double dist2, a, r;
	double x1, y1, z1, warp_dist, equiv_warp_factor;

	o = wormhole;
	t = object;

	if (o == t)
		return;

	switch (t->type) {
	case OBJTYPE_ASTEROID:
		/* because of functional movement of asteroids. *sigh* */
	case OBJTYPE_WORMHOLE:
		return;
	default:
		dist2 = object_dist2(t, o);
		if (dist2 < 30.0 * 30.0) {
			a = snis_randn(360) * M_PI / 180.0;
			r = 60.0;
			x1 = o->x;
			y1 = o->y;
			z1 = o->z;
			set_object_location(t, o->tsd.wormhole.dest_x + cos(a) * r, 
						o->tsd.wormhole.dest_y,
						o->tsd.wormhole.dest_z + sin(a) * r);
			t->timestamp = universe_timestamp;
			if (t->type == OBJTYPE_SHIP1) {
				warp_dist = hypot3d(o->x - x1, o->y - y1, o->z - z1);
				equiv_warp_factor = 10.0 * warp_dist / (XKNOWN_DIM / 2.0);
				schedule_callback8(event_callback, &callback_schedule,
					"player-wormhole-travel-event", (double) o->id,
					x1, y1, z1, o->x, o->y, o->z, equiv_warp_factor);
				send_wormhole_limbo_packet(t->id, 5 * 30);
			}
		}
	}
}

static void wormhole_move(struct snis_entity *o)
{
	space_partition_process(space_partition, o, o->x, o->z, o,
				wormhole_collision_detection);
}

#if GATHER_OPCODE_STATS
static void gather_opcode_stats(struct packed_buffer *pb)
{
	int length;
	uint8_t opcode;

	/* assumption, first byte is opcode. */
	memcpy(&opcode, pb->buffer, sizeof(opcode));
	length = packed_buffer_length(pb);
	write_opcode_stats[opcode].opcode = opcode;
	write_opcode_stats[opcode].count++;
	write_opcode_stats[opcode].bytes += (uint64_t) length;
}

static void gather_opcode_not_sent_stats(struct snis_entity *o)
{
	int opcode[4] = { -1, -1, -1, -1 };

	switch (o->type) {
	case OBJTYPE_SHIP1:
		opcode[0] = OPCODE_UPDATE_SHIP;
		opcode[1] = OPCODE_UPDATE_POWER_DATA;
		opcode[2] = OPCODE_UPDATE_COOLANT_DATA;
		break;
	case OBJTYPE_SHIP2:
		opcode[0] = OPCODE_ECON_UPDATE_SHIP;
		break;
	case OBJTYPE_ASTEROID:
		opcode[0] = OPCODE_UPDATE_ASTEROID;
		break;
	case OBJTYPE_CARGO_CONTAINER:
		opcode[0] = OPCODE_UPDATE_CARGO_CONTAINER;
		break;
	case OBJTYPE_DERELICT:
		opcode[0] = OPCODE_UPDATE_DERELICT;
		break;
	case OBJTYPE_BLACK_HOLE:
		opcode[0] = OPCODE_UPDATE_BLACK_HOLE;
		break;
	case OBJTYPE_PLANET:
		opcode[0] = OPCODE_UPDATE_PLANET;
		break;
	case OBJTYPE_WORMHOLE:
		opcode[0] = OPCODE_UPDATE_WORMHOLE;
		break;
	case OBJTYPE_WARPGATE:
		opcode[0] = OPCODE_UPDATE_WARPGATE;
		break;
	case OBJTYPE_STARBASE:
		opcode[0] = OPCODE_UPDATE_STARBASE;
		break;
	case OBJTYPE_NEBULA:
		opcode[0] = OPCODE_UPDATE_NEBULA;
		break;
	case OBJTYPE_EXPLOSION:
		opcode[0] = OPCODE_UPDATE_EXPLOSION;
		break;
	case OBJTYPE_DEBRIS:
		break;
	case OBJTYPE_SPARK:
		break;
	case OBJTYPE_TORPEDO:
		opcode[0] = OPCODE_UPDATE_TORPEDO;
		break;
	case OBJTYPE_LASER:
		opcode[0] = OPCODE_UPDATE_LASER;
		break;
	case OBJTYPE_SPACEMONSTER:
		opcode[0] = OPCODE_UPDATE_SPACEMONSTER;
		break;
	case OBJTYPE_LASERBEAM:
		opcode[0] = OPCODE_UPDATE_LASERBEAM;
		break;
	case OBJTYPE_TRACTORBEAM:
		opcode[0] = OPCODE_UPDATE_TRACTORBEAM;
		break;
	default:
		break;
	}
	for (int i = 0; i < ARRAYSIZE(opcode); i++)
		if (opcode[i] > 0)
			write_opcode_stats[opcode[i]].count_not_sent++;
}
#else
#define gather_opcode_stats(x)
#define gather_opcode_not_sent_stats(x)
#endif

static inline void pb_queue_to_client(struct game_client *c, struct packed_buffer *pb)
{

	if (!pb) {
		stacktrace("snis_server: NULL packed_buffer in pb_queue_to_client()");
		return;
	}
	gather_opcode_stats(pb);
	packed_buffer_queue_add(&c->client_write_queue, pb, &c->client_write_queue_mutex);
}

static inline void pb_prepend_queue_to_client(struct game_client *c, struct packed_buffer *pb)
{

	if (!pb) {
		stacktrace("snis_server: NULL packed_buffer in pb_prepend_queue_to_client()");
		return;
	}
	gather_opcode_stats(pb);
	packed_buffer_queue_prepend(&c->client_write_queue, pb, &c->client_write_queue_mutex);
}

static void queue_delete_oid(struct game_client *c, uint32_t oid)
{
	pb_queue_to_client(c, snis_opcode_pkt("bw", OPCODE_DELETE_OBJECT, oid));
}

static void push_cop_mode(struct snis_entity *cop);
static void respawn_object(struct snis_entity *o)
{
	int i;
	uint32_t hp;

	switch (o->type) {
		case OBJTYPE_SHIP2:
			if (!o->tsd.ship.auto_respawn)
				break;
			if (o->tsd.ship.ai[0].ai_mode != AI_MODE_COP)
				add_ship(lowest_faction, snis_randn(nshiptypes), 1);
			else {
				/* respawn cops as cops */
				hp = o->tsd.ship.home_planet;
				i = add_ship(o->sdata.faction, SHIP_CLASS_ENFORCER, 1);
				if (i < 0)
					break;
				o = &go[i];
				o->tsd.ship.home_planet = hp;
				push_cop_mode(o);
				break;
			}
			break;
		case OBJTYPE_ASTEROID:
			/* TODO: respawn asteroids */
			break;
		default:
			break;
	}
	return;
}

static void delete_object(struct snis_entity *o)
{
	remove_space_partition_entry(space_partition, &o->partition);
	if (o->sdata.science_text) {
		free(o->sdata.science_text);
		o->sdata.science_text = NULL;
	}

	/* Omit OBJTYPE_SHIP2 here because we want the registration preserved for the derelict */
	if (o->type == OBJTYPE_SHIP1 || o->type == OBJTYPE_DERELICT)
		ship_registry_delete_ship_entries(&ship_registry, o->id);
	/* If a starbase gets killed, any bounties registered for it should get wiped out */
	if (o->type == OBJTYPE_STARBASE)
		ship_registry_delete_bounty_entries_by_site(&ship_registry, o->id);

	if (o->type == OBJTYPE_DERELICT && o->tsd.derelict.ships_log) {
		free(o->tsd.derelict.ships_log);
		o->tsd.derelict.ships_log = NULL;
	}

	if (o->type == OBJTYPE_PLANET && o->tsd.planet.custom_description) {
		free(o->tsd.planet.custom_description);
		o->tsd.planet.custom_description = NULL;
	}

	snis_object_pool_free_object(pool, go_index(o));
	o->id = -1;
	o->alive = 0;
}

static void snis_queue_delete_object_helper(struct snis_entity *o)
{
	int i;
	uint32_t oid = o->id;

	/* This should never happen. If it does happen, don't propagate it to the clients. */
	if (oid == (uint32_t) -1) {
		fprintf(stderr, "%s BUG detected at %s:%s:%d, o->id == -1, o->type = %d\n",
				logprefix(), __FILE__, __func__,  __LINE__, o->type);
		return;
	}

	for (i = 0; i < nclients; i++)
		if (client[i].refcount)
			queue_delete_oid(&client[i], oid);
}

static void snis_queue_delete_object(struct snis_entity *o)
{
	/* Iterate over all clients and inform them that
	 * an object is gone.
	 * Careful with the locking, we have 3 layers of locks
	 * here (couldn't figure out a simpler way.
	 * 
	 * We should already hold the universe_mutex, we 
	 * need the client lock to iterate over the clients,
	 * and when we build the packet, we'll need the
	 * client_write_queue_mutexes.  Hope there's no deadlocks.
	 */

	client_lock();
	snis_queue_delete_object_helper(o);
	client_unlock();
}

static void remove_ship_victim(struct snis_entity *ship, uint32_t victim_id)
{
	int i;

	for (i = 0; i < ship->tsd.ship.nai_entries; i++) {
		if (ship->tsd.ship.ai[i].ai_mode == AI_MODE_ATTACK) {
			if (ship->tsd.ship.ai[i].u.attack.victim_id == victim_id)
				ship->tsd.ship.ai[i].u.attack.victim_id = -2;
		}
	}
}

static void remove_starbase_victim(struct snis_entity *starbase, uint32_t victim_id)
{
	int i, j;

	for (i = 0; i < starbase->tsd.starbase.nattackers;) {
		if (starbase->tsd.starbase.attacker[i] == victim_id) {
			for (j = i; j < starbase->tsd.starbase.nattackers - 1; j++) {
				starbase->tsd.starbase.attacker[j] = starbase->tsd.starbase.attacker[j + 1];
			}
			starbase->tsd.starbase.nattackers--;
			continue;
		}
		i++;
	}
}

static void remove_from_attack_lists(uint32_t victim_id)
{
	int i;
	const int n = snis_object_pool_highest_object(pool);

	for (i = 0; i <= n; i++) {
		switch (go[i].type) {
		case OBJTYPE_SHIP2:
			remove_ship_victim(&go[i], victim_id);
			break;
		case OBJTYPE_STARBASE:
			remove_starbase_victim(&go[i], victim_id);
			break;
		default:
			continue;
		}
	}
}

static void delete_starbase_docking_ports(struct snis_entity *o)
{
	int i;

	assert(o->type == OBJTYPE_STARBASE);
	for (i = 0; i <= snis_object_pool_highest_object(pool); i++) {
		if (go[i].type == OBJTYPE_DOCKING_PORT &&
			go[i].tsd.docking_port.parent == o->id && go[i].alive) {
				delete_from_clients_and_server(&go[i]);
		}
	}
}

static void unhook_remote_cameras(uint32_t id)
{
	int i;

	/* id is an object about to be deleted. Make sure anyone ship with a remote
	 * camera feed from this object gets unhooked from it.
	 */
	for (i = 0; i <= snis_object_pool_highest_object(pool); i++) {
		if (go[i].alive && go[i].type == OBJTYPE_SHIP1 && go[i].tsd.ship.viewpoint_object == id)
			go[i].tsd.ship.viewpoint_object = go[i].id;
	}
}

static void ai_trace(uint32_t id, char *format, ...);
static void delete_from_clients_and_server_helper(struct snis_entity *o, int take_client_lock)
{
	if (take_client_lock)
		snis_queue_delete_object(o);
	else
		snis_queue_delete_object_helper(o);
	switch (o->type) {
	case OBJTYPE_DEBRIS:
	case OBJTYPE_SPARK:
	case OBJTYPE_TORPEDO:
	case OBJTYPE_LASER:
	case OBJTYPE_EXPLOSION:
	case OBJTYPE_LASERBEAM:
	case OBJTYPE_TRACTORBEAM:
	case OBJTYPE_DOCKING_PORT:
		break;
	case OBJTYPE_STARBASE:
		delete_starbase_docking_ports(o);
		/* deliberate fallthrough */
	default:
		schedule_callback(event_callback, &callback_schedule,
				"object-death-event", o->id);
		break;
	}

	if (o->type == OBJTYPE_SHIP2)
		fleet_leave(o->id); /* leave any fleets ship might be a member of */
	remove_from_attack_lists(o->id);
	unhook_remote_cameras(o->id);
	if (o->id == ai_trace_id) {
		ai_trace(o->id, "TRACED OBJECT %u DELETED", o->id);
		ai_trace_id = (uint32_t) -1;
	}
	delete_object(o);
}

static void delete_from_clients_and_server(struct snis_entity *o)
{
	delete_from_clients_and_server_helper(o, 1);
}

#define ANY_SHIP_ID (0xffffffff)
static void send_packet_to_all_clients_on_a_bridge(uint32_t shipid, struct packed_buffer *pb, uint32_t roles)
{
	int i;

	client_lock();
	for (i = 0; i < nclients; i++) {
		struct packed_buffer *pbc;
		struct game_client *c = &client[i];

		if (!c->refcount)
			continue;

		if (c->shipid != shipid && shipid != ANY_SHIP_ID)
			continue;

		if (!(c->role & roles))
			continue;

		pbc = packed_buffer_copy(pb);
		pb_queue_to_client(c, pbc);
	}
	packed_buffer_free(pb);
	client_unlock();
}

static float comms_transmission_strength(struct snis_entity *transmitter, struct snis_entity *receiver)
{
	int i;
	float blocking_factor = 0.0;
	float max_blocking_factor = 0.0;
	float signal_strength = 1.0;
	float radius;
	union vec3 source, target, occluder, ray_direction;
	const float to_receiver = object_dist(transmitter, receiver);

	if (transmitter == NULL || !enable_comms_attenuation)
		return 1.0;

	source.v.x = transmitter->x;
	source.v.y = transmitter->y;
	source.v.z = transmitter->z;
	target.v.x = receiver->x;
	target.v.y = receiver->y;
	target.v.z = receiver->z;
	vec3_sub(&ray_direction, &target, &source);
	vec3_normalize_self(&ray_direction);

	/* Check for planets, nebula or black holes occluding comms transmission */
	for (i = -1; i <= snis_object_pool_highest_object(pool); i++) {
		if (i == -1) {
			occluder.v.x = SUNX;
			occluder.v.y = SUNY;
			occluder.v.z = SUNZ;
			radius = 5000;
		} else {
			if (!go[i].alive)
				continue;
			switch (go[i].type) {
			case OBJTYPE_PLANET:
				occluder.v.x = go[i].x;
				occluder.v.y = go[i].y;
				occluder.v.z = go[i].z;
				radius = 1.05 * go[i].tsd.planet.radius;
				blocking_factor = 1.0;
				break;
			case OBJTYPE_NEBULA:
				occluder.v.x = go[i].x;
				occluder.v.y = go[i].y;
				occluder.v.z = go[i].z;
				radius = 1.05 * go[i].tsd.nebula.r;
				blocking_factor = 0.75;
				break;
			case OBJTYPE_BLACK_HOLE:
				occluder.v.x = go[i].x;
				occluder.v.y = go[i].y;
				occluder.v.z = go[i].z;
				radius = 1.3 * go[i].tsd.black_hole.radius;
				blocking_factor = 1.0;
				break;
			default:
				continue;
			}
		}
		if (ray_intersects_sphere(&source, &ray_direction, &occluder, radius)) {
			float to_occluder = object_dist(transmitter, &go[i]);
			if (to_receiver > to_occluder) { /* occluder blocks transmission */
				if (blocking_factor > max_blocking_factor)
					max_blocking_factor = blocking_factor;
			}
		} else {
			if (to_receiver > COMMS_LONG_DISTANCE_THRESHOLD) {
				blocking_factor = blocking_factor +
					(0.5 * blocking_factor * to_receiver /
						(2.0 * COMMS_LONG_DISTANCE_THRESHOLD));
				if (blocking_factor > max_blocking_factor)
					max_blocking_factor = blocking_factor;
			}
		}
	}
	if (transmitter->type == OBJTYPE_SHIP1) {
		/* Account for antenna aim */
		union vec3 antenna_dir;
		antenna_dir.v.x = 1.0;
		antenna_dir.v.y = 0.0;
		antenna_dir.v.z = 0.0;
		quat_rot_vec_self(&antenna_dir, &transmitter->tsd.ship.current_hg_ant_orientation);
		signal_strength = vec3_dot(&antenna_dir, &transmitter->tsd.ship.desired_hg_ant_aim) -
					max_blocking_factor;
		/* Account for comms power */
		signal_strength *= 2.0 * (float) (transmitter->tsd.ship.power_data.comms.i) / 255.0;
	} else {
		signal_strength = 1.0 - max_blocking_factor;
	}
	if (signal_strength < 0.0)
		signal_strength = 0.0;
	return signal_strength;
}

static void send_comms_packet_to_all_clients(struct snis_entity *transmitter,
					struct packed_buffer *pb, uint32_t roles)
{
	int i;

	client_lock();
	for (i = 0; i < nclients; i++) {
		struct packed_buffer *pbc;
		struct game_client *c = &client[i];

		if (!c->refcount)
			continue;

		if (!(c->role & roles))
			continue;

		/* transmitter may be null (e.g. from LUA script) */
		if (transmitter && c->shipid != transmitter->id) {
			float strength = comms_transmission_strength(transmitter, &go[c->ship_index]);
			if (strength < COMMS_TRANSMISSION_STRENGTH_THRESHOLD) /* TODO: something better here */
				continue;
		}

		pbc = packed_buffer_copy(pb);
		pb_queue_to_client(c, pbc);
	}
	packed_buffer_free(pb);
	client_unlock();
}

static void set_waypoints_dirty_all_clients_on_bridge(uint32_t shipid, int value)
{
	int i;

	/* client_lock(); TODO - do we need this lock? (it deadlocks though) */
	for (i = 0; i < nclients; i++) {
		if (!client[i].refcount)
			continue;
		if (client[i].shipid != shipid)
			continue;
		client[i].waypoints_dirty = value;
	}
	/* client_unlock(); TODO - do we need this lock? (it deadlocks though) */
}

static struct packed_buffer *distort_comms_message(struct packed_buffer *pb, float strength)
{
	struct packed_buffer *pbc;
	int distortion = (int) (((1.0 - strength) * 0.5) * 1000);
	int i, length;
	unsigned char *buffer;

	pbc = packed_buffer_copy(pb);
	if (strength >= COMMS_TRANSMISSION_STRENGTH_THRESHOLD) /* Signal strength is fine, no distortion */
		return pbc;
	/* Signal strength is weak, some distortion */

	/* This is a little ugly, as it requires knowing the format of the comms transmission packet.
	 * Luckily, the format is very simple, byte 1 is opcode, byte 2 is length, and the
	 * remainder is the message. It also requires knowing innards of struct packed_buffer,
	 * which is even uglier. Might be cleaner to distort the message before packing, but the message
	 * is unfortunately packed early and in very many places.
	 */
	buffer = (unsigned char *) pbc->buffer;
	length = packed_buffer_length(pbc);
	/* Clobber some of the message */
	/* 3 to skip opcode, enciphered and length (see struct comms_transmission_packet) */
	for (i = 3; i < length; i++)
		if (snis_randn(1000) < distortion)
			buffer[i] = '*'; /* clobber it */
	return pbc;
}

static void send_comms_packet_to_all_bridges_on_channel(struct snis_entity *transmitter,
				uint32_t channel, struct packed_buffer *pb, uint32_t roles)
{
	int i;
	float strength;

	client_lock();
	for (i = 0; i < nclients; i++) {
		struct packed_buffer *pbc;
		struct game_client *c = &client[i];

		if (!c->refcount)
			continue;

		if (!(c->role & roles))
			continue;

		if (bridgelist[c->bridge].comms_channel != channel)
			continue;

		/* transmitter may be null (e.g. LUA script) */
		if (transmitter && c->shipid != transmitter->id) {
			strength = comms_transmission_strength(transmitter, &go[c->ship_index]);
			if (strength <= 0.05)  /* Too weak, drop the message */
				continue;
		} else {
			strength = 1.0;
		}

		pbc = distort_comms_message(pb, strength);
		pb_queue_to_client(c, pbc);
	}
	packed_buffer_free(pb);
	client_unlock();
}

/* send packet to a particular client, plus clients with the specified roles,
 * on a bridge.  This is used for the sub-modes of various screens, so that
 * that multiple science stations (for example) on the same ship can be on
 * different sub screens (e.g. sciball vs. 2d) so long as those stations do
 * not support MAIN_SCREEN_ROLE (for example).  This enables "spectator" screens
 * that do not have their modes myteriously changed by remote parties while
 * still enabling the captain to order any screen "onscreen!" to be mirrored
 * by the MAIN SCREEN.
 */
static void send_packet_to_requestor_plus_role_on_a_bridge(struct game_client *requestor,
				struct packed_buffer *pb, uint32_t roles)
{
	int i;

	client_lock();
	for (i = 0; i < nclients; i++) {
		struct packed_buffer *pbc;
		struct game_client *c = &client[i];

		if (!c->refcount)
			continue;

		if (c->shipid != requestor->shipid)
			continue;

		if (!(c->role & roles) && c != requestor)
			continue;

		pbc = packed_buffer_copy(pb);
		pb_queue_to_client(c, pbc);
	}
	packed_buffer_free(pb);
	client_unlock();
}

static void send_packet_to_all_clients(struct packed_buffer *pb, uint32_t roles)
{
	send_packet_to_all_clients_on_a_bridge(ANY_SHIP_ID, pb, roles);
}

static void ai_trace(uint32_t id, char *format, ...)
{
	char buf[DEMON_CONSOLE_MSG_MAX];
	struct packed_buffer *pb;
	va_list arg_ptr;
	static char last_buf[DEMON_CONSOLE_MSG_MAX] = { 0 };

	if (id == (uint32_t) -1 || id != ai_trace_id)
		return;

	memset(buf, 0, sizeof(buf));
	va_start(arg_ptr, format);
	vsnprintf(buf, sizeof(buf) - 1, format, arg_ptr);
	va_end(arg_ptr);
	buf[sizeof(buf) - 1] = '\0';
	if (strcmp(buf, last_buf) == 0) /* suppress duplicate messages */
		return;
	/* fprintf(stderr, "%s\n", buf); */
	strcpy(last_buf, buf);
	pb = packed_buffer_allocate(3 + sizeof(buf));
	packed_buffer_append(pb, "bbb", OPCODE_CONSOLE_OP, OPCODE_CONSOLE_SUBCMD_ADD_TEXT, 255);
	packed_buffer_append_raw(pb, buf, sizeof(buf));
	send_packet_to_all_clients(pb, ROLE_DEMON);
}

static void queue_add_sound(struct game_client *c, uint16_t sound_number)
{
	pb_queue_to_client(c, snis_opcode_pkt("bh", OPCODE_PLAY_SOUND, sound_number));
}

static void queue_add_text_to_speech(struct game_client *c, const char *text)
{
	struct packed_buffer *pb;
	char tmpbuf[256];
	uint8_t length;

	length = strlen(text) & 0x0ff;
	if (length >= 255)
		length = 254;
	snprintf(tmpbuf, 255, "%s", text);
	tmpbuf[255] = '\0';

	/* Save this text in case we are asked to repeat it later, unless this text is already a repeat */
	if (strncmp(tmpbuf, "I said, ", 8) != 0 && strcmp(tmpbuf, "I didn't say anything") != 0)
		strncpy(bridgelist[c->bridge].last_text_to_speech, tmpbuf,
			sizeof(bridgelist[c->bridge].last_text_to_speech) - 1);

	pb = packed_buffer_allocate(512);
	packed_buffer_append(pb, "bbb", OPCODE_NATURAL_LANGUAGE_REQUEST,
				OPCODE_NL_SUBCOMMAND_TEXT_TO_SPEECH, (uint8_t) strlen(tmpbuf) + 1);
	packed_buffer_append_raw(pb, tmpbuf, length + 1);
	pb_queue_to_client(c, pb);
}

static void snis_queue_add_sound(uint16_t sound_number, uint32_t roles, uint32_t shipid)
{
	int i;
	struct game_client *c;

	client_lock();
	for (i = 0; i < nclients; i++) {
		c = &client[i];
		if (c->refcount && (c->role & roles) && c->shipid == shipid)
			queue_add_sound(c, sound_number);
	}
	client_unlock();
}

static void snis_queue_add_text_to_speech(const char *text, uint32_t roles, uint32_t shipid)
{
	int i;
	struct game_client *c;

	client_lock();
	for (i = 0; i < nclients; i++) {
		c = &client[i];
		if (c->refcount && (c->role & roles) && c->shipid == shipid)
			queue_add_text_to_speech(c, text);
	}
	client_unlock();
}

static void __attribute__((unused)) snis_queue_add_global_sound(uint16_t sound_number)
{
	int i;

	for (i = 0; i < nbridges; i++)
		snis_queue_add_sound(sound_number, ROLE_ALL, bridgelist[i].shipid);
}

static void snis_queue_add_global_text_to_speech(const char *text)
{
	int i;

	for (i = 0; i < nbridges; i++)
		snis_queue_add_text_to_speech(text, ROLE_ALL, bridgelist[i].shipid);
}

static void instantly_repair_damcon_part(struct damcon_data *d, int system, int part)
{
	int i, n = snis_object_pool_highest_object(d->pool);
	struct snis_damcon_entity *p;

	for (i = 0; i <= n; i++) {
		p = &d->o[i];
		if (p->type != DAMCON_TYPE_PART)
			continue;
		if (p->tsd.part.system != system)
			continue;
		if (p->tsd.part.part != part)
			continue;
		p->tsd.part.damage = 0;
		p->version++;
		break;
	}
}

static void distribute_per_part_damage_among_parts(struct snis_entity *o,
		struct damcon_data *d, int per_part_damage[], int damcon_system)
{
	int i, n, count;
	struct snis_damcon_entity *p;

	count = 0;
	n = snis_object_pool_highest_object(d->pool);
	for (i = 0; i <= n; i++) {
		int new_damage;
		p = &d->o[i];
		if (p->type != DAMCON_TYPE_PART)
			continue;
		if (p->tsd.part.system != damcon_system)
			continue;
		new_damage = p->tsd.part.damage + per_part_damage[count];
		if (new_damage > 255)
			new_damage = 255;
		p->tsd.part.damage = new_damage;
		count++;
		p->version++;
		if (count == DAMCON_PARTS_PER_SYSTEM)
			break;
	}
}

static void distribute_damage_to_damcon_system_parts(struct snis_entity *o,
			struct damcon_data *d, int damage, int damcon_system)
{
	int i;
	int total_damage;
	int per_part_damage[DAMCON_PARTS_PER_SYSTEM];

	if (!d) /* OBJTYPE_SHIP2 don't have all the systems OBJTYPE_SHIP1 have */
		return;

	total_damage = damage * DAMCON_PARTS_PER_SYSTEM;

	/* distribute total_damage into per_part_damage[] 
	 * (unequally, just because it is easy)
	 */
	for (i = 0; i < DAMCON_PARTS_PER_SYSTEM - 1; i++) {
		per_part_damage[i] = snis_randn(total_damage);
		total_damage -= per_part_damage[i];
	}
	per_part_damage[DAMCON_PARTS_PER_SYSTEM - 1] = total_damage;

	/* Now scramble per_part_damage[] so the same system doesn't
	 * get hammered over and over by inequal distribution above
	 */
	for (i = 0; i < DAMCON_PARTS_PER_SYSTEM; i++) {
		int n, tmp;

		do {
			n = snis_randn(100) % DAMCON_PARTS_PER_SYSTEM;
		} while (n == i);
		tmp = per_part_damage[i];
		per_part_damage[i] = per_part_damage[n];
		per_part_damage[n] = tmp;
	}

	distribute_per_part_damage_among_parts(o, d, per_part_damage, damcon_system);
	return;
}

static void distribute_damage_to_damcon_system_parts_fractionally(struct snis_entity *o,
			struct damcon_data *d, int damage, int damcon_system, double f[])
{
	int i;
	int total_damage;
	int per_part_damage[DAMCON_PARTS_PER_SYSTEM];

	if (!d) /* OBJTYPE_SHIP2 don't have all the systems OBJTYPE_SHIP1 have */
		return;

	total_damage = damage * DAMCON_PARTS_PER_SYSTEM;

	/* distribute total_damage into per_part_damage[] */
	for (i = 0; i < DAMCON_PARTS_PER_SYSTEM; i++)
		per_part_damage[i] = (int) (total_damage * f[i]);

	distribute_per_part_damage_among_parts(o, d, per_part_damage, damcon_system);
	return;
}

static int lookup_bridge_by_shipid(uint32_t shipid);
static void player_update_shield_wavelength_width_depth(struct snis_entity *player_ship)
{
	/* Dig through the damcon system and find the individual shield system parts
	 * and translate their damage values in to shield wavelength, width and depth.
	 */
	int i, bridge;
	struct damcon_data *d;
	struct snis_damcon_entity *t;
	int n = 0;

	bridge = lookup_bridge_by_shipid(player_ship->id);
	if (bridge < 0)
		return;
	d = &bridgelist[bridge].damcon;
	for (i = 0; i <= snis_object_pool_highest_object(d->pool); i++) {
		t = &d->o[i];
		switch (t->type) {
		case DAMCON_TYPE_PART:
			if (d->o[i].tsd.part.system != DAMCON_TYPE_SHIELDSYSTEM)
				break;
			switch (n % 3) {
			case 0:
				player_ship->sdata.shield_wavelength = ((t->tsd.part.damage * 11) % 255);
				break;
			case 1:
				player_ship->sdata.shield_width = t->tsd.part.damage;
				break;
			case 2:
				player_ship->sdata.shield_depth = t->tsd.part.damage;
				break;
			default:
				break;
			}
			n++;
			break;
		default:
			break;
		}
	}
}

static int roll_damage(struct snis_entity *o, struct damcon_data *d, 
			double weapons_factor, double shield_strength, uint8_t system,
			int damcon_system)
{
	int damage = (weapons_factor * (double) (20 + snis_randn(40)) * (1.2 - shield_strength));
	if (damage + system > 255)
		damage = 255 - system;

	distribute_damage_to_damcon_system_parts(o, d, damage, damcon_system);
	
	return damage + system;
}

static void calculate_turret_damage(struct snis_entity *o)
{
	if (o->type != OBJTYPE_TURRET)
		return;
	int damage = 100 + snis_randn(100);
	int health = o->tsd.turret.health;
	health -= damage;
	if (health < 0)
		health = 0;
	o->tsd.turret.health = health;
	if (o->tsd.turret.health == 0)
		o->alive = 0;
}

static void calculate_spacemonster_damage(struct snis_entity *o)
{
	if (o->type != OBJTYPE_SPACEMONSTER)
		return;
	int damage = 50 + snis_randn(50);
	int defense = 25 * (snis_randn(o->tsd.spacemonster.toughness) / 255.0);
	if (defense > damage)
		defense = damage;
	damage = damage - defense;
	int health = o->tsd.spacemonster.health;
	health -= damage;
	if (health < 0)
		health = 0;
	o->tsd.spacemonster.health = health;
	if (o->tsd.spacemonster.health == 0)
		o->alive = 0;
	else {
		int anger, fear;
		if (o->tsd.spacemonster.health > 100) {
			anger = o->tsd.spacemonster.anger + damage;
			if (anger > 255)
				anger = 255;
			o->tsd.spacemonster.anger = anger;
		} else {
			anger = o->tsd.spacemonster.anger - damage / 2;
			if (anger < 0)
				anger = 0;
			o->tsd.spacemonster.anger = anger;
			fear = o->tsd.spacemonster.fear + damage;
			if (fear > 255)
				fear = 255;
			o->tsd.spacemonster.fear = fear;
		}
	}
}


static void calculate_block_damage(struct snis_entity *o)
{
	if (o->type != OBJTYPE_BLOCK)
		return;
	if (o->tsd.block.health == 255) /* immortal */
		return;
	int damage = 100 + snis_randn(100);
	int health = o->tsd.block.health;
	health -= damage;
	if (health < 0)
		health = 0;
	printf("block health is %d\n", health);
	o->tsd.block.health = health;
	/* Doesn't die, just breaks away... */
}

static void calculate_torpedolike_damage(struct snis_entity *target, double weapons_factor)
{
	double ss;
	const double twp = weapons_factor * (target->type == OBJTYPE_SHIP1 ? 0.333 : 1.0);
	struct damcon_data *d = NULL;

	if (target->type == OBJTYPE_SHIP1) {
		if (player_invincibility)
			return;
		int bridge = lookup_bridge_by_shipid(target->id);

		if (bridge < 0) {
			fprintf(stderr, "bug at %s:%d, bridge lookup failed.\n", __FILE__, __LINE__);
			return;
		}
		d = &bridgelist[bridge].damcon;
	} else if (target->type == OBJTYPE_TURRET) {
		calculate_turret_damage(target);
		return;
	} else if (target->type == OBJTYPE_BLOCK) {
		calculate_block_damage(target);
		return;
	} else if (target->type == OBJTYPE_SPACEMONSTER) {
		calculate_spacemonster_damage(target);
		return;
	}

	ss = shield_strength(snis_randn(255), target->sdata.shield_strength,
				target->sdata.shield_width,
				target->sdata.shield_depth,
				target->sdata.shield_wavelength);

	target->tsd.ship.damage.shield_damage = roll_damage(target, d, twp, ss,
			target->tsd.ship.damage.shield_damage, DAMCON_TYPE_SHIELDSYSTEM);
	target->tsd.ship.damage.impulse_damage = roll_damage(target, d, twp, ss,
			target->tsd.ship.damage.impulse_damage, DAMCON_TYPE_IMPULSE);
	target->tsd.ship.damage.warp_damage = roll_damage(target, d, twp, ss,
			target->tsd.ship.damage.warp_damage, DAMCON_TYPE_WARPDRIVE);
	target->tsd.ship.damage.maneuvering_damage = roll_damage(target, d, twp, ss,
			target->tsd.ship.damage.maneuvering_damage, DAMCON_TYPE_MANEUVERING);
	target->tsd.ship.damage.phaser_banks_damage = roll_damage(target, d, twp, ss,
			target->tsd.ship.damage.phaser_banks_damage, DAMCON_TYPE_PHASERBANK);
	target->tsd.ship.damage.sensors_damage = roll_damage(target, d, twp, ss,
			target->tsd.ship.damage.sensors_damage, DAMCON_TYPE_SENSORARRAY);
	target->tsd.ship.damage.comms_damage = roll_damage(target, d, twp, ss,
			target->tsd.ship.damage.comms_damage, DAMCON_TYPE_COMMUNICATIONS);
	target->tsd.ship.damage.tractor_damage = roll_damage(target, d, twp, ss,
			target->tsd.ship.damage.tractor_damage, DAMCON_TYPE_TRACTORSYSTEM);
	target->tsd.ship.damage.lifesupport_damage = roll_damage(target, d, twp, ss,
			target->tsd.ship.damage.lifesupport_damage, DAMCON_TYPE_LIFESUPPORTSYSTEM);

	if (target->tsd.ship.damage.shield_damage == 255) {
		target->timestamp = universe_timestamp;
		target->respawn_time = universe_timestamp + player_respawn_time * 10;
		target->alive = 0;
	}
}

static void calculate_torpedo_damage(struct snis_entity *target)
{
	calculate_torpedolike_damage(target, torpedo_damage_factor);
}

static void calculate_atmosphere_damage(struct snis_entity *target)
{
	calculate_torpedolike_damage(target, atmosphere_damage_factor);
}

static void calculate_laser_damage(struct snis_entity *o, uint8_t wavelength, float power)
{
	int b, i;
	unsigned char *x = (unsigned char *) &o->tsd.ship.damage;
	double ss;
	float power_factor = (float) power / 255.0;
	float damage;
	float old_damage;
	struct damcon_data *d = NULL;

	if (o->type == OBJTYPE_SHIP1) {
		b = lookup_bridge_by_shipid(o->id);
		if (b < 0)
			fprintf(stderr, "b < 0 at %s:%d\n", __FILE__, __LINE__);
		else
			d = &bridgelist[b].damcon;
	} else if (o->type == OBJTYPE_TURRET) {
		calculate_turret_damage(o);
		return;
	} else if (o->type == OBJTYPE_BLOCK) {
		calculate_block_damage(o);
		return;
	} else if (o->type == OBJTYPE_SPACEMONSTER) {
		calculate_spacemonster_damage(o); /* FIXME: this should take power into account. */
		return;
	}

	ss = shield_strength(wavelength, o->sdata.shield_strength,
				o->sdata.shield_width,
				o->sdata.shield_depth,
				o->sdata.shield_wavelength);

	for (i = 0; i < sizeof(o->tsd.ship.damage); i++) {
		damage = ((float) (snis_randn(LASER_DAMAGE_MAX) + 1) * (1 - ss) * power_factor);

		/* if damage will get rounded down to nothing bump it up to 1.0 */
		if (damage < 0.5)
			damage = 1.0;

		old_damage = (float) x[i];
		damage = (int) ((float) x[i] + damage);
		if (damage > 255.0)
			damage = 255.0;
		x[i] = (uint8_t) damage;
		damage = x[i] - old_damage;
		if (o->type == OBJTYPE_SHIP1)
			distribute_damage_to_damcon_system_parts(o, d, (int) damage, i);
	}
	if (o->tsd.ship.damage.shield_damage == 255) {
		o->timestamp = universe_timestamp;
		o->respawn_time = universe_timestamp + player_respawn_time * 10;
		o->alive = 0;
	}
}

static void calculate_laser_starbase_damage(struct snis_entity *o, uint8_t wavelength)
{
	double damage, ss, new_strength;

	ss = shield_strength(wavelength, o->sdata.shield_strength,
				o->sdata.shield_width,
				o->sdata.shield_depth,
				o->sdata.shield_wavelength);

	damage = (0.95 - ss) * (double) (snis_randn(5) + 2.0) + 1.0;
	
	new_strength = (double) o->sdata.shield_strength - damage;
	if (new_strength < 0)
		new_strength = 0;
	o->sdata.shield_strength = (int) new_strength;
	if (o->sdata.shield_strength == 0)
		o->alive = 0;
}

static int calculate_rts_main_base_laser_damage(struct snis_entity *o, struct snis_entity *target)
{
	/* FIXME: do something better here to take strength of unit into account */
	return RTS_MAX_LASER_MAIN_BASE_DAMAGE;
}

static int calculate_rts_main_base_torpedo_damage(struct snis_entity *o, struct snis_entity *target)
{
	/* FIXME: do something better here taking the strength of the unit into account */
	return RTS_MAX_TORPEDO_MAIN_BASE_DAMAGE;
}

static void inflict_rts_main_base_damage(struct snis_entity *o, struct snis_entity *target,
						int (*damage_fn)(struct snis_entity *o, struct snis_entity *target))
{
	int i;
	if (!rts_mode)
		return;
	if (target->type != OBJTYPE_PLANET)
		return;
	for (i = 0; i < ARRAYSIZE(rts_planet); i++) {
		if (go[rts_planet[i].index].id == target->id) {
			int health = rts_planet[i].health;
			health = health - damage_fn(o, target);
			if (health < 0) {
				health = 0;
			}
			if (health == 0)
				rts_finish_timer = 100; /* 10 seconds */
			rts_planet[i].health = health;
			break;
		}
	}
}

static void disable_rts_mode(void);
static void check_rts_finish_condition(void)
{
	if (!rts_mode)
		return;
	if (rts_finish_timer >= 0)
		rts_finish_timer--;
	if (rts_finish_timer == 0) {
		disable_rts_mode();
		rts_finish_timer = -1;
	}
}

static void inflict_rts_main_base_torpedo_damage(struct snis_entity *o, struct snis_entity *target)
{
	inflict_rts_main_base_damage(o, target, calculate_rts_main_base_torpedo_damage);
}

static void inflict_rts_main_base_laser_damage(struct snis_entity *o, struct snis_entity *target)
{
	inflict_rts_main_base_damage(o, target, calculate_rts_main_base_laser_damage);
}

static void send_detonate_packet(struct snis_entity *o, double x, double y, double z,
				uint32_t time, double fractional_time);
static void send_silent_ship_damage_packet(struct snis_entity *o);

static struct snis_entity *lookup_entity_by_id(uint32_t id)
{
	int index;

	if (id == (uint32_t) -1)
		return NULL;

	index = lookup_by_id(id);
	if (index >= 0)
		return &go[index];
	else
		return NULL;
}

static int enemy_faction(int faction1, int faction2)
{
	return (faction_hostility(faction1, faction2) > FACTION_HOSTILITY_THRESHOLD);
}

static int friendly_fire(struct snis_entity *attacker, struct snis_entity *victim)
{
	if (attacker->type == OBJTYPE_SHIP1) /* players cannot commit friendly fire */
		return 0;

	return !enemy_faction(attacker->sdata.faction, victim->sdata.faction);
}

static int find_fleet_number(struct snis_entity *o)
{
	int i;

	for (i = o->tsd.ship.nai_entries - 1; i >= 0; i--) {
		if (o->tsd.ship.ai[i].ai_mode == AI_MODE_FLEET_MEMBER ||
			o->tsd.ship.ai[i].ai_mode == AI_MODE_FLEET_LEADER)
			return o->tsd.ship.ai[i].u.fleet.fleet;
	}
	return -1;
}

static uint32_t buddies_pick_who_to_attack(struct snis_entity *attacker)
{
	int f;

	if (attacker->type == OBJTYPE_SHIP1) /* player controlled ship */
		return attacker->id;

	f = find_fleet_number(attacker);

	/* if attacker isn't a fleet member, attack attacker */
	if (f < 0)
		return attacker->id;

	/* if attacker is a fleet member, attack random fleet member */
	return fleet_member_get_id(f, snis_randn(fleet_members(f)));
}

static void push_attack_mode(struct snis_entity *attacker, uint32_t victim_id,
		int recursion_level);

static void buddy_join_the_fight(uint32_t id, struct snis_entity *attacker, int recursion_level)
{
	struct snis_entity *o;
	int i;

	if (id == (uint32_t) -1) /* shouldn't happen */
		return;

	if (snis_randn(100) > 33)  /* only get involved 1/3 times */
		return;

	i = lookup_by_id(id);
	if (i < 0)
		return;
	o = &go[i];
	if (!o->alive)
		return;
	if (o->type != OBJTYPE_SHIP2) /* shouldn't happen */
		return;
	push_attack_mode(o, buddies_pick_who_to_attack(attacker), recursion_level + 1);
}

static void buddies_join_the_fight(int fleet_number, struct snis_entity *victim, int recursion_level)
{
	int i;

	if (fleet_number < 0)
		return;

	for (i = 0; i < fleet_members(fleet_number); i++)
		buddy_join_the_fight(fleet_member_get_id(fleet_number, i), victim, recursion_level);
}

static void calculate_attack_vector(struct snis_entity *o, int mindist, int maxdist)
{
	/* Assumptions: 
	 * o is attacker, 
	 * o->tsd.ship.ai[o->tsd.ship.nai_entries - 1].u.attack.victim_id is victim
 	 * o->tsd.ship.dox,doy,doz are set to offsets from victim location to aim at.
	 * mindist and maxdist are the min and max dist for the offset.
	 */
	int victim_id = o->tsd.ship.ai[o->tsd.ship.nai_entries - 1].u.attack.victim_id;
	int i = lookup_by_id(victim_id);

	/* If it's a planet, adjust mindist, maxdist */
	if (i >= 0 && go[i].type == OBJTYPE_PLANET) {
		mindist += go[i].tsd.planet.radius;
		maxdist += go[i].tsd.planet.radius;
	}

	/* FIXME: do something smarter/better */
	random_dpoint_on_sphere(snis_randn(maxdist - mindist) + mindist,
					&o->tsd.ship.dox,
					&o->tsd.ship.doy,
					&o->tsd.ship.doz);
}

/* Sets short term destination of a ship, tsd.ship.dox, doy, doz = x,y,z */
static void set_ship_destination(struct snis_entity *o, double x, double y, double z)
{
	o->tsd.ship.dox = x;
	o->tsd.ship.doy = y;
	o->tsd.ship.doz = z;
}

static uint32_t choose_ship_home_planet(void)
{
	int i, hp;

	hp = snis_randn(NPLANETS);
	for (i = 0; i <= snis_object_pool_highest_object(pool); i++) {
		if (go[i].type == OBJTYPE_PLANET) {
			if (hp <= 0)
				return go[i].id;
			hp--;
		}
	}
	return (uint32_t) -1;
}

static void set_patrol_waypoint_destination(struct snis_entity *o, int patrol_point)
{
	int n = o->tsd.ship.nai_entries - 1;
	struct ai_cop_data *cpatrol;
	struct ai_patrol_data *patrol;

	assert(patrol_point >= 0);
	assert(n >= 0);
	assert(o->type == OBJTYPE_SHIP2);

	switch (o->tsd.ship.ai[n].ai_mode) {
	case AI_MODE_COP:
		cpatrol = &o->tsd.ship.ai[n].u.cop;
		if (patrol_point < cpatrol->npoints) {
			cpatrol->dest = patrol_point;
			set_ship_destination(o,
				cpatrol->p[patrol_point].v.x,
				cpatrol->p[patrol_point].v.y,
				cpatrol->p[patrol_point].v.z);
			o->tsd.ship.desired_velocity = ship_type[o->tsd.ship.shiptype].max_speed;
		}
		break;
	case AI_MODE_PATROL:
	case AI_MODE_FLEET_LEADER:
		patrol = &o->tsd.ship.ai[n].u.patrol;
		if (patrol_point < patrol->npoints) {
			patrol->dest = patrol_point;
			set_ship_destination(o,
				patrol->p[patrol_point].v.x,
				patrol->p[patrol_point].v.y,
				patrol->p[patrol_point].v.z);
			o->tsd.ship.desired_velocity = ship_type[o->tsd.ship.shiptype].max_speed;
		}
		break;
	default:
		break;
	}
}

static void push_cop_mode(struct snis_entity *cop)
{
	int i, npoints;
	float orbit_radius;
	struct snis_entity *p;
	union quat q;
	struct ai_cop_data *patrol;
	union vec3 home_planet;

	cop->tsd.ship.nai_entries = 1;
	cop->tsd.ship.ai[0].ai_mode = AI_MODE_COP;

	i = lookup_by_id(cop->tsd.ship.home_planet);
	if (i < 0) { /* The planet might get deleted on the demon screen, for instance. */
		cop->tsd.ship.home_planet = choose_ship_home_planet(); /* Pick a new home planet */
		i = lookup_by_id(cop->tsd.ship.home_planet);
	}
	if (i >= 0) {
		p = &go[i];
		home_planet.v.x = p->x;
		home_planet.v.y = p->y;
		home_planet.v.z = p->z;
		orbit_radius = (0.01 * (float) snis_randn(100) + 2.0) * p->tsd.planet.radius;
	} else {
		/* Apparently there aren't any planets.  Alright... let's just patrol around our current position.
		 * TODO: I should probably figure out something better here.  A cop with a destroyed home planet
		 * should probably stop being a cop and do something else.
		 */
		home_planet.v.x = cop->x;
		home_planet.v.y = cop->y;
		home_planet.v.z = cop->z;
		orbit_radius = (float) snis_randn(MAX_GAS_GIANT_RADIUS - MIN_GAS_GIANT_RADIUS) + MIN_GAS_GIANT_RADIUS;
	}

	npoints = MAX_PATROL_POINTS;
	patrol = &cop->tsd.ship.ai[0].u.cop;
	patrol->npoints = npoints;
	patrol->dest = 0;
	patrol->oneshot = 0;

	random_quat(&q);

	for (i = 0; i < npoints; i++) {
		float angle = M_PI / 180.0 * i * (360.0 / MAX_PATROL_POINTS);
		union vec3 v = { { sin(angle) * orbit_radius, cos(angle) * orbit_radius, 0.0 } };
		quat_rot_vec(&patrol->p[i], &v, &q);
		vec3_add_self(&patrol->p[i], &home_planet);
	}
	set_patrol_waypoint_destination(cop, 0);
}

static void push_catatonic_mode(struct snis_entity *ship)
{
	if (ship->type != OBJTYPE_SHIP2)
		return;
	ship->tsd.ship.nai_entries = 1;
	ship->tsd.ship.ai[0].ai_mode = AI_MODE_CATATONIC;
}

static void push_attack_mode(struct snis_entity *attacker, uint32_t victim_id, int recursion_level)
{
	int n, i;
	double d1, d2;
	struct snis_entity *v;
	int fleet_number;

	if (safe_mode)
		return;

	/* Do not attack if no weapons */
	if (!ship_type[attacker->tsd.ship.shiptype].has_lasers &&
		!ship_type[attacker->tsd.ship.shiptype].has_torpedoes)
		return;

	if (recursion_level > 2) /* guard against infinite recursion */
		return;

	i = lookup_by_id(victim_id);
	if (i < 0)
		return;

	if (attacker->type != OBJTYPE_SHIP2) {
		fprintf(stderr, "%s:%d: Unexpectedly called push_attack_mode on objtype %d\n",
			__FILE__, __LINE__, (int) attacker->type);
		return;
	}

	if (go[i].type == OBJTYPE_SHIP2 || go[i].type == OBJTYPE_SHIP1) {
		if (attacker->tsd.ship.ai[0].ai_mode != AI_MODE_COP) {
			if (go[i].type == OBJTYPE_SHIP2 &&
				go[i].tsd.ship.ai[0].ai_mode == AI_MODE_COP)
				return; /* Don't attack the cops */
		}
		/* If we're in a secure area ... */
		if (attacker->tsd.ship.in_secure_area || go[i].tsd.ship.in_secure_area) {
			/* And we're an NPC and not a cop ... */
			if (attacker->type != OBJTYPE_SHIP2 || attacker->tsd.ship.ai[0].ai_mode != AI_MODE_COP)
				/* TODO: something better */
				return; /* Do not attack in secure area (unless cop). */
		}
	}

	fleet_number = find_fleet_number(attacker);

	n = attacker->tsd.ship.nai_entries;

	if (n > 0 && attacker->tsd.ship.ai[n - 1].ai_mode == AI_MODE_CATATONIC)
		return;

	/* too busy running away... */
	if (n > 0 && attacker->tsd.ship.ai[n - 1].ai_mode == AI_MODE_FLEE) {
		i = lookup_by_id(victim_id);
		if (i < 0)
			return;
		buddies_join_the_fight(fleet_number, &go[i], recursion_level);
		return;
	}

	if (n > 0 && attacker->tsd.ship.ai[n - 1].ai_mode == AI_MODE_ATTACK) {

		if (attacker->tsd.ship.ai[n - 1].u.attack.victim_id == victim_id)
			return; /* already attacking this guy */
		v = &go[i];
		d1 = dist3dsqrd(v->x - attacker->x, v->y - attacker->y, v->z - attacker->z);
		i = lookup_by_id(attacker->tsd.ship.ai[n - 1].u.attack.victim_id);
		if (i < 0) {
			attacker->tsd.ship.ai[n - 1].u.attack.victim_id = victim_id;
			calculate_attack_vector(attacker, MIN_COMBAT_ATTACK_DIST,
							MAX_COMBAT_ATTACK_DIST);
			i = lookup_by_id(victim_id);
			if (i >= 0)
				buddies_join_the_fight(fleet_number, &go[i], recursion_level);
			return;
		}
		v = &go[i];
		d2 = dist3dsqrd(v->x - attacker->x, v->y - attacker->y, v->z - attacker->z);
		if (d1 < d2) {
			/* new attack victim closer than old one. */
			attacker->tsd.ship.ai[n - 1].u.attack.victim_id = victim_id;
			calculate_attack_vector(attacker, MIN_COMBAT_ATTACK_DIST,
							MAX_COMBAT_ATTACK_DIST);
			buddies_join_the_fight(fleet_number, v, recursion_level);
			return;
		}
	}
	if (n < MAX_AI_STACK_ENTRIES) {
		attacker->tsd.ship.ai[n].ai_mode = AI_MODE_ATTACK;
		attacker->tsd.ship.ai[n].u.attack.victim_id = victim_id;
		attacker->tsd.ship.nai_entries++;
		calculate_attack_vector(attacker, MIN_COMBAT_ATTACK_DIST,
						MAX_COMBAT_ATTACK_DIST);
		i = lookup_by_id(victim_id);
		if (i >= 0)
			buddies_join_the_fight(fleet_number, &go[i], recursion_level);
	}
}

static void push_avoid_missile(struct snis_entity *ship, struct snis_entity *missile)
{
	int n;
	n = ship->tsd.ship.nai_entries;
	if (n >= MAX_AI_STACK_ENTRIES)
		return;
	if (n > 0 && ship->tsd.ship.ai[n - 1].ai_mode == AI_MODE_AVOID_MISSILE)
		return;
	ship->tsd.ship.ai[n].ai_mode = AI_MODE_AVOID_MISSILE;
	ship->tsd.ship.ai[n].u.avoid_missile.id = missile->id;
	ship->tsd.ship.nai_entries++;
	return;
}

static void attack_your_attacker(struct snis_entity *attackee, struct snis_entity *attacker)
{
	if (!attacker)
		return;

	if (attackee->type != OBJTYPE_SHIP2)
		return;

	if (friendly_fire(attacker, attackee))
		return;

	if (snis_randn(100) >= 75)
		return;
	push_attack_mode(attackee, attacker->id, 0);
}

static void push_mining_bot_mode(struct snis_entity *miner, uint32_t parent_ship_id,
					uint32_t asteroid_id, int bridge, int selected_waypoint)
{
	int i, n;

	n = miner->tsd.ship.nai_entries;
	if (asteroid_id == (uint32_t) -1 && selected_waypoint == -1)
		return;
	if (asteroid_id != (uint32_t) -1) {
		i = lookup_by_id(asteroid_id);
		if (i < 0)
			return;
		int n = miner->tsd.ship.nai_entries;
		if (n >= MAX_AI_STACK_ENTRIES)
			return;
		miner->tsd.ship.nai_entries++;
		miner->tsd.ship.ai[n].u.mining_bot.object_or_waypoint = go[i].type;
		set_ship_destination(miner, go[i].x, go[i].y, go[i].z);
	} else if (selected_waypoint >= 0 && selected_waypoint < bridgelist[bridge].nwaypoints) {
		if (n >= MAX_AI_STACK_ENTRIES)
			return;
		miner->tsd.ship.nai_entries++;
		struct player_waypoint *wp =
			&bridgelist[bridge].waypoint[selected_waypoint];
		miner->tsd.ship.ai[n].u.mining_bot.object_or_waypoint = 128; /* waypoint */
		miner->tsd.ship.ai[n].u.mining_bot.wpx = wp->x;
		miner->tsd.ship.ai[n].u.mining_bot.wpy = wp->y;
		miner->tsd.ship.ai[n].u.mining_bot.wpz = wp->z;
		set_ship_destination(miner, wp->x, wp->y, wp->z);
	}
	miner->tsd.ship.ai[n].ai_mode = AI_MODE_MINING_BOT;
	miner->tsd.ship.ai[n].u.mining_bot.asteroid = asteroid_id;
	miner->tsd.ship.ai[n].u.mining_bot.parent_ship = parent_ship_id;
	miner->tsd.ship.ai[n].u.mining_bot.mode = MINING_MODE_APPROACH_ASTEROID;
	miner->tsd.ship.ai[n].u.mining_bot.orphan_time = 0;
	miner->tsd.ship.ai[n].u.mining_bot.towing = 0;
	miner->tsd.ship.ai[n].u.mining_bot.towed_object = (uint32_t) -1;
	random_quat(&miner->tsd.ship.ai[n].u.mining_bot.orbital_orientation);
}

static int add_derelict(const char *name, double x, double y, double z,
			double vx, double vy, double vz, int shiptype,
			int the_faction, int persistent, uint32_t orig_ship_id);

static int add_cargo_container(double x, double y, double z, double vx, double vy, double vz,
				int item, float qty, int persistent);
static int make_derelict(struct snis_entity *o)
{
	int i, rc;
	rc = add_derelict(o->sdata.name, o->x, o->y, o->z,
				o->vx + snis_random_float() * 2.0,
				o->vy + snis_random_float() * 2.0,
				o->vz + snis_random_float() * 2.0,
				o->tsd.ship.shiptype, o->sdata.faction, 0, o->id);
	if (o->type == OBJTYPE_SHIP1 || o->type == OBJTYPE_SHIP2) {
		for (i = 0; i < o->tsd.ship.ncargo_bays; i++) {
			int item;
			float qty;

			item = o->tsd.ship.cargo[i].contents.item;
			qty = o->tsd.ship.cargo[i].contents.qty;
			if (qty > 0.0 && item >= 0)
				(void) add_cargo_container(o->x, o->y, o->z,
					o->vx + snis_random_float() * 2.0,
					o->vy + snis_random_float() * 2.0,
					o->vz + snis_random_float(), item, qty,
					item >= nstandard_commodities);
		}
	} else {
		(void) add_cargo_container(o->x, o->y, o->z,
				o->vx + snis_random_float() * 2.0,
				o->vy + snis_random_float() * 2.0,
				o->vz + snis_random_float(), -1, 0.0, 0);
	}
	return rc;
}

struct potential_victim_info {
	struct snis_entity *o;
	double fightiness;
	int victim_id;
};

static void ship_security_avoidance(void *context, void *entity);
static void process_potential_victim(void *context, void *entity)
{
	struct potential_victim_info *info = context;
	struct snis_entity *v = entity; 
	struct snis_entity *o = info->o; 
	double dist;
	float hostility, fightiness;

	if (o == v) /* don't victimize self */
		return;

	/* only victimize players, other ships, starbases, and space monsters */
	if (v->type != OBJTYPE_STARBASE && v->type != OBJTYPE_SHIP1 &&
		v->type != OBJTYPE_SHIP2 && v->type != OBJTYPE_SPACEMONSTER)
		return;

	if (!v->alive) /* skip the dead guys */
		return;

	if (o->sdata.faction == v->sdata.faction)
		return; /* Don't attack own faction */

	if (v->type == OBJTYPE_SHIP2) {
		if (v->tsd.ship.ai[0].ai_mode == AI_MODE_COP)
			return; /* skip cops */
		if (o->tsd.ship.ai[0].ai_mode != AI_MODE_COP) { /* I'm not a cop... */
			if (v->tsd.ship.in_secure_area) /* already know secure? */
				return;
			space_partition_process(space_partition, v, v->x, v->z, v,
					ship_security_avoidance);
			if (v->tsd.ship.in_secure_area) /* check again... */
				return;
		}
	}

	hostility = faction_hostility(o->sdata.faction, v->sdata.faction);
	dist = object_dist(o, v);

	if (dist > XKNOWN_DIM / 10.0) /* too far away */
		return;

	if (v->type == OBJTYPE_SPACEMONSTER) {
		hostility = 1.0;
		fightiness = (100000.0 * hostility) / (dist + 1.0);
		o->tsd.ship.threat_level += fightiness;
		return;
	} else {

		/* nearby friendlies reduce threat level */
		if (o->sdata.faction == v->sdata.faction)
			o->tsd.ship.threat_level -= 10000.0 / (dist + 1.0);

		if (hostility < FACTION_HOSTILITY_THRESHOLD)
			return;

		fightiness = (10000.0 * hostility) / (dist + 1.0);
		o->tsd.ship.threat_level += fightiness;

		if (v->type == OBJTYPE_SHIP1)
			fightiness *= 3.0f; /* prioritize hitting player... */
	}

	if (info->victim_id == -1 || fightiness > info->fightiness) {
		info->victim_id = v->id;
		info->fightiness = fightiness;
	}
}

static int find_nearest_victim(struct snis_entity *o)
{
	struct potential_victim_info info;

	/* assume universe mutex is held */
	info.victim_id = -1;
	info.fightiness = -1.0f;
	info.o = o;
	o->tsd.ship.threat_level = 0.0;

	space_partition_process(space_partition, o, o->x, o->z, &info,
				process_potential_victim);
	return info.victim_id;
}

static struct snis_entity *pick_random_patrol_destination(struct snis_entity *ship, union vec3 *dest)
{
	union vec3 v;
	struct snis_entity *o;
	int i, count;

	/* FIXME: do something better here. */
	count = 0;
	while (1) {
		i = snis_randn(snis_object_pool_highest_object(pool));
		o = &go[i];
		count++;
		if (count > 1000) {
			o = NULL;
			break;
		}
		if (!o->alive)
			continue;
		if (o->type != OBJTYPE_PLANET && o->type != OBJTYPE_STARBASE)
			continue;
		break;
	}
	if (count <= 1000) {
		double dx, dy, dz;

		if (o->type == OBJTYPE_PLANET)
			random_dpoint_on_sphere(o->tsd.planet.radius * 1.3, &dx, &dy, &dz);
		else
			random_dpoint_on_sphere(100.0, &dx, &dy, &dz);
		v.v.x = o->x + dx;
		v.v.y = o->y + dy;
		v.v.z = o->z + dz;
	} else {
		/* FIXME: do something better */
		v.v.x = ship->x + (float) snis_randn(XKNOWN_DIM / 3);
		v.v.y = ship->y + (float) snis_randn(YKNOWN_DIM / 3);
		v.v.z = ship->z + (float) snis_randn(ZKNOWN_DIM / 3);
		o = NULL;
	}
	*dest = v;
	return o;
}

static void setup_patrol_route(struct snis_entity *o)
{
	int n, npoints, i;
	struct ai_patrol_data *patrol;
	struct snis_entity *p[MAX_PATROL_POINTS];
	union vec3 v[MAX_PATROL_POINTS];

	n = o->tsd.ship.nai_entries;
	o->tsd.ship.nai_entries = n + 1;
	o->tsd.ship.ai[n].ai_mode = AI_MODE_PATROL;

	npoints = (snis_randn(100) % 3) + 3;
	assert(npoints > 1 && npoints <= MAX_PATROL_POINTS);
	patrol = &o->tsd.ship.ai[n].u.patrol;
	patrol->npoints = npoints;
	patrol->max_hangout_time = 255;
	patrol->dest = 0;
	patrol->oneshot = 0;

	/* FIXME: ensure no duplicate points and order in some sane way */
	ai_trace(o->id, "SET UP PATROL ROUTE");
	for (i = 0; i < npoints; i++) {
		memset(&v[i], 0, sizeof(v[i]));
		int duplicate;
		do {
			int j;
			p[i] = pick_random_patrol_destination(o, &v[i]);
			if (i == 0) /* First pick cannot be a duplicate */
				break;
			/* check to see if we've already picked this destination */
			duplicate = 0;
			for (j = 0; j < i; j++) {
				if (p[j] == p[i]) {
					duplicate = 1;
					break;
				}
			}
		} while (duplicate);
	}
	for (i = 0; i < npoints; i++) {
		patrol->p[i] = v[i];
		ai_trace(o->id, "- %.2f, %.2f, %.2f", patrol->p[i].v.x, patrol->p[i].v.y, patrol->p[i].v.z);
	}
	set_patrol_waypoint_destination(o, 0);
}

static void ship_figure_out_what_to_do(struct snis_entity *o)
{
	int fleet_shape;

	ai_trace(o->id, "FIGURING WHAT TO DO");
	if (o->tsd.ship.nai_entries > 0)
		return;
	switch (snis_randn(100) % 3) {
	case 0:
	case 1:	
		ai_trace(o->id, "NORMAL PATROL");
		setup_patrol_route(o);
		break;
	case 2:
		if (o->sdata.faction == 0) {
			ai_trace(o->id, "FACTION 0 NORMAL PATROL");
			setup_patrol_route(o);
			break;
		}
		if ((snis_randn(100) % 2) == 0)
			fleet_shape = FLEET_TRIANGLE;
		else
			fleet_shape = FLEET_SQUARE;
		setup_patrol_route(o);
		if (fleet_count() < max_fleets()) {
			ai_trace(o->id, "FLEET LEADER");
			o->tsd.ship.ai[0].ai_mode = AI_MODE_FLEET_LEADER;
			o->tsd.ship.ai[0].u.fleet.fleet = fleet_new(fleet_shape, o->id);
			o->tsd.ship.ai[0].u.fleet.fleet_position = 0;
		} else {
			struct snis_entity *leader;
			int32_t leader_id;
			int i, j, joined;

			joined = 0;
			for (i = 0; i < max_fleets(); i++) {
				int fleet_pos;
				leader_id = fleet_get_leader_id(i);
				j = lookup_by_id(leader_id);
				if (j < 0)
					continue;
				leader = &go[j];
				/* only join fleet of own faction */
				if (leader->sdata.faction != o->sdata.faction)
					continue;
				fleet_pos = fleet_join(i, o->id);
				if (fleet_pos < 0)
					continue;
				o->tsd.ship.ai[0].ai_mode = AI_MODE_FLEET_MEMBER;
				o->tsd.ship.ai[0].u.fleet.fleet = i;
				o->tsd.ship.ai[0].u.fleet.fleet_position = fleet_pos;
				joined = 1;
				ai_trace(o->id, "FLEET MEMBER");
			}
			if (!joined) {
				ai_trace(o->id, "FAILED TO FIND FLEET, NORMAL PATROL");
				setup_patrol_route(o);
				break;
			}
		}
	default:
		ai_trace(o->id, "UNEXPECTED DEFAULT, NO ACTION");
		break;
	}
}

static void pop_ai_stack(struct snis_entity *o)
{
	int n;
	struct ai_patrol_data *patrol;
	struct ai_cop_data *cpatrol;

	n = o->tsd.ship.nai_entries;
	if (n <= 0) {
		ship_figure_out_what_to_do(o);
		return;
	}
	o->tsd.ship.nai_entries--;
	n = o->tsd.ship.nai_entries - 1;
	switch (o->tsd.ship.ai[n].ai_mode) {
	case AI_MODE_ATTACK:
		calculate_attack_vector(o, MIN_COMBAT_ATTACK_DIST,
						MAX_COMBAT_ATTACK_DIST);
		break;
	case AI_MODE_TOW_SHIP:
		o->tsd.ship.ai[n].u.tow_ship.ship_connected = 0;
		break;
	case AI_MODE_PATROL:
	case AI_MODE_FLEET_LEADER:
		patrol = &o->tsd.ship.ai[n].u.patrol;
		set_patrol_waypoint_destination(o, patrol->dest);
		break;
	case AI_MODE_COP:
		cpatrol = &o->tsd.ship.ai[n].u.cop;
		set_patrol_waypoint_destination(o, cpatrol->dest);
		break;
	default:
		break;
	}
}

static void pop_ai_attack_mode(struct snis_entity *o)
{
	int n;

	if (o->type != OBJTYPE_SHIP2) /* Could be a starbase, calling in here from torpedo collision detection */
		return;

	n = o->tsd.ship.nai_entries;
	if (n <= 0) {
		ship_figure_out_what_to_do(o);
		return;
	}
	n--;
	if (n < 0)
		return;
	if (o->tsd.ship.ai[n].ai_mode == AI_MODE_ATTACK)
		o->tsd.ship.nai_entries--;
	n--;
	if (n < 0)
		return;
	if (o->tsd.ship.ai[n].ai_mode == AI_MODE_ATTACK)
		calculate_attack_vector(o, MIN_COMBAT_ATTACK_DIST,
						MAX_COMBAT_ATTACK_DIST);
	return;
}

static void add_starbase_attacker(struct snis_entity *starbase, int attacker_id)
{
	int i, n;

	/* First check if this attacker is already known */
	for (i = 0; i < starbase->tsd.starbase.nattackers; i++)
		if (starbase->tsd.starbase.attacker[i] == attacker_id)
			return; /* Attacker is already known */

	/* Attacker is not already known */
	n = starbase->tsd.starbase.nattackers;
	if (n >= ARRAYSIZE(starbase->tsd.starbase.attacker))
		n %= ARRAYSIZE(starbase->tsd.starbase.attacker);
	else
		starbase->tsd.starbase.nattackers++;
	starbase->tsd.starbase.attacker[n] = attacker_id;
}

static void notify_a_cop(void *context, void *entity)
{
	struct snis_entity *perp = context;
	struct snis_entity *cop = entity;
	float dist2;
	int n;

	if (cop->type != OBJTYPE_SHIP2)
		return;

	if (perp->type == OBJTYPE_STARBASE)
		return;
	
	if (perp->type == OBJTYPE_SHIP2 && perp->tsd.ship.ai[0].ai_mode == AI_MODE_COP)
		return;

	if (cop->tsd.ship.ai[0].ai_mode != AI_MODE_COP)
		return;

	dist2 = object_dist2(cop, perp);
	if (dist2 > SECURITY_RADIUS * SECURITY_RADIUS)
		return;

	/* Farther away from cop, greater chance cop doesn't see... */
	dist2 = sqrt(dist2) / (float) SECURITY_RADIUS;
	if (snis_randn(1000) < dist2 * 250)
		return;

	n = cop->tsd.ship.nai_entries;
	assert(n > 0);
	if (n == 1) {
		push_attack_mode(cop, perp->id, 0);
		return;
	}
	n--;
	if (cop->tsd.ship.ai[n].ai_mode == AI_MODE_HANGOUT) {
		pop_ai_stack(cop);
		push_attack_mode(cop, perp->id, 0);
	}
}

static void notify_the_cops(struct snis_entity *weapon, struct snis_entity *target)
{
	uint32_t perp_id;
	int perp_index;
	struct snis_entity *perp;

	if (target->type == OBJTYPE_SHIP2 && target->tsd.ship.ai[0].ai_mode == AI_MODE_COP) {
		/* If the target is a cop, it's already notified. */
		return;
	}

	switch (weapon->type) {
	case OBJTYPE_TORPEDO:
		perp_id = weapon->tsd.torpedo.ship_id;
		break;
	case OBJTYPE_LASER:
		perp_id = weapon->tsd.laser.ship_id;
		break;
	case OBJTYPE_LASERBEAM:
		perp_id = weapon->tsd.laserbeam.origin;
		break;
	case OBJTYPE_MISSILE:
		perp_id = weapon->tsd.missile.origin;
		break;
	default:
		return;
	}
	perp_index = lookup_by_id(perp_id);
	if (perp_index < 0)
		return;
	perp = &go[perp_index];
	space_partition_process(space_partition, perp, perp->x, perp->z, perp, notify_a_cop);
}

static int projectile_collides(double x1, double y1, double z1,
				double vx1, double vy1, double vz1, double r1,
				double x2, double y2, double z2,
				double vx2, double vy2, double vz2, double r2,
				float *time)
{
	union vec3 s1, s2, v1, v2;

	s2.v.x = (float) (x2 - x1);
	s2.v.y = (float) (y2 - y1);
	s2.v.z = (float) (z2 - z1);
	v2.v.x = (float) vx2;
	v2.v.y = (float) vy2;
	v2.v.z = (float) vz2;

	s1.v.x = 0.0f;
	s1.v.y = 0.0f;
	s1.v.z = 0.0f;
	v1.v.x = (float) vx1;
	v1.v.y = (float) vy1;
	v1.v.z = (float) vz1;

	return moving_spheres_intersection(&s1, (float) r1, &v1, &s2, (float) r2, &v2, 1.0f, time);
}

static void do_collision_impulse(struct snis_entity *player, struct snis_entity *object);
static void block_add_to_naughty_list(struct snis_entity *o, uint32_t id);
static void spacemonster_set_antagonist(struct snis_entity *o, uint32_t id);

/* Given a block and a point on the surface of that block, find the position of that
 * point 1/10th second ago. This is for calculating how that point is moving for
 * collision response
 */
static void find_block_point_last_pos(struct snis_entity *block, union vec3 *point,
		union vec3 *previous_point)
{
	int i;
	struct snis_entity *root;
	union vec3 root_to_point;
	union quat inverse_rotation;

	if (block->tsd.block.root_id == (uint32_t) -1) {
		root = block;
	} else {
		i = lookup_by_id(block->tsd.block.root_id);
		if (i == -1)
			root = block; /* shouldn't happen */
		else
			root = &go[i];
	}
	root_to_point.v.x = point->v.x - root->x;
	root_to_point.v.y = point->v.y - root->y;
	root_to_point.v.z = point->v.z - root->z;
	quat_inverse(&inverse_rotation, &root->tsd.block.rotational_velocity);
	quat_rot_vec_self(&root_to_point, &inverse_rotation);
	previous_point->v.x = root_to_point.v.x + root->x - root->vx;
	previous_point->v.y = root_to_point.v.y + root->y - root->vy;
	previous_point->v.z = root_to_point.v.z + root->z - root->vz;
}

static void missile_explode(struct snis_entity *o)
{
	delete_from_clients_and_server(o);
	(void) add_explosion(o->x, o->y, o->z, 50, 50, 50, o->type);
}

static void missile_collision_detection(void *context, void *entity)
{
	struct snis_entity *missile = context;
	struct snis_entity *target = entity;
	double dist2, damage_factor;

	if (missile == target)
		return;
	if (missile->tsd.missile.target_id != target->id && target->type != OBJTYPE_CHAFF)
		return;

	dist2 = object_dist2(missile, target);
	if (dist2 < chaff_proximity_distance * chaff_proximity_distance && target->type == OBJTYPE_CHAFF) {
		if (snis_randn(1000) < 1000.0 * chaff_confuse_chance) {
			/* Missile is confused by chaff. Let the ship that was evading this missile
			 * stop doing that. */
			int i = lookup_by_id(missile->tsd.missile.target_id);
			if (i >= 0) {
				struct snis_entity *o = &go[i];
				if (o->alive && o->type == OBJTYPE_SHIP2) {
					int n = o->tsd.ship.nai_entries;
					if (n > 0 && o->tsd.ship.ai[n - 1].ai_mode == AI_MODE_AVOID_MISSILE)
						pop_ai_stack(o);
				}
			}
			/* Missile is now chasing the chaff */
			missile->tsd.missile.target_id = target->id;
		}
	}
	if (dist2 < missile_proximity_distance * missile_proximity_distance) {
		switch (target->type) {
		case OBJTYPE_CHAFF:
			if (snis_randn(1000) < 1000.0 * chaff_confuse_chance)
				missile->tsd.missile.target_id = target->id;
			break;
		case OBJTYPE_SHIP1:
		case OBJTYPE_SHIP2:
			notify_the_cops(missile, target);
			damage_factor = missile_explosion_damage_distance / (sqrt(dist2) + 3.0);
			calculate_missile_explosion_damage(target, damage_factor);
			send_ship_damage_packet(target);
			if (target->type == OBJTYPE_SHIP2)
				attack_your_attacker(target, lookup_entity_by_id(missile->tsd.missile.origin));
			if (!target->alive) {
				(void) add_explosion(target->x, target->y, target->z, 50, 150, 50, target->type);
				/* TODO -- these should be different sounds */
				/* make sound for players that got hit */
				/* make sound for players that did the hitting */
				snis_queue_add_sound(EXPLOSION_SOUND, ROLE_SOUNDSERVER, missile->tsd.missile.target_id);
				if (target->type != OBJTYPE_SHIP1) {
					if (target->type == OBJTYPE_SHIP2)
						make_derelict(target);
					respawn_object(target);
					delete_from_clients_and_server(target);
				} else {
					snis_queue_add_sound(EXPLOSION_SOUND,
						ROLE_SOUNDSERVER, target->id);
					schedule_callback(event_callback, &callback_schedule,
							"player-death-callback", target->id);
				}
				missile_explode(missile);
			} else {
				(void) add_explosion(target->x, target->y, target->z, 50, 5, 5, target->type);
				snis_queue_add_sound(DISTANT_TORPEDO_HIT_SOUND, ROLE_SOUNDSERVER,
							missile->tsd.missile.origin);
				snis_queue_add_sound(TORPEDO_HIT_SOUND, ROLE_SOUNDSERVER,
							missile->tsd.missile.target_id);
				missile_explode(missile);
			}
			break;
		default:
			break;
		}
	}
}

static void torpedo_collision_detection(void *context, void *entity)
{
	struct snis_entity *t = entity;  /* target */
	struct snis_entity *o = context;
	float delta_t;
	double tolerance = 350.0;
	double dist2, ix, iy, iz;
	uint32_t impact_time;
	double impact_fractional_time;

	if (!t->alive)
		return;
	if (t == o)
		return;

	/* What's the -3 about? */
	if (o->alive >= torpedo_lifetime - 3)
		return;
	if (t->type != OBJTYPE_SHIP1 && t->type != OBJTYPE_SHIP2 &&
			t->type != OBJTYPE_STARBASE &&
			t->type != OBJTYPE_ASTEROID &&
			t->type != OBJTYPE_CARGO_CONTAINER &&
			t->type != OBJTYPE_PLANET &&
			t->type != OBJTYPE_BLOCK &&
			t->type != OBJTYPE_SPACEMONSTER)
		return;
	if (t->id == o->tsd.torpedo.ship_id)
		return; /* can't torpedo yourself. */
	dist2 = object_dist2(t, o);

	if (t->type == OBJTYPE_BLOCK) {
		union vec3 torpedo_pos, closest_point, normal, block_pos;

		if (dist2 > t->tsd.block.shape.overall_radius * t->tsd.block.shape.overall_radius)
			return;
		torpedo_pos.v.x = o->x;
		torpedo_pos.v.y = o->y;
		torpedo_pos.v.z = o->z;

		block_pos.v.x = t->x;
		block_pos.v.y = t->y;
		block_pos.v.z = t->z;

		dist2 = shape_closest_point(&torpedo_pos, &block_pos, &t->orientation,
						&t->tsd.block.shape, &closest_point, &normal);

		if (t->tsd.block.shape.type == SHAPE_CAPSULE) {
			if (sqrtf(dist2) - t->tsd.block.shape.capsule.radius > torpedo_velocity)
				return;
		} else {
			if (dist2 > torpedo_velocity * torpedo_velocity)
				return;
		}
		o->alive = 0; /* hit!!!! */
		schedule_callback2(event_callback, &callback_schedule,
					"object-hit-event", t->id, (double) o->tsd.torpedo.ship_id);
		(void) add_explosion(closest_point.v.x, closest_point.v.y, closest_point.v.z, 50, 5, 5, t->type);
		snis_queue_add_sound(DISTANT_TORPEDO_HIT_SOUND, ROLE_SOUNDSERVER, t->id);
		block_add_to_naughty_list(t, o->tsd.torpedo.ship_id);
		calculate_torpedo_damage(t);
		return;
	}

	if (t->type == OBJTYPE_PLANET && dist2 < t->tsd.planet.radius * t->tsd.planet.radius) {
		o->alive = 0; /* smashed into planet */
		schedule_callback2(event_callback, &callback_schedule,
				"object-hit-event", (double) t->id,
				(double) o->tsd.torpedo.ship_id);
		if (rts_mode)
			inflict_rts_main_base_torpedo_damage(o, t);
	} else if (dist2 > TORPEDO_DETONATE_DIST2)
		return; /* not close enough */

	/* make sure torpedoes aren't *too* easy to hit */
	if (t->type == OBJTYPE_TORPEDO)
		tolerance = 30.0;

	if (!projectile_collides(o->x, o->y, o->z, o->vx, o->vy, o->vz, 0.5 * tolerance,
				t->x, t->y, t->z, t->vx, t->vy, t->vz, 0.5 * tolerance, &delta_t))
		return;

	o->alive = 0; /* hit!!!! */
	schedule_callback2(event_callback, &callback_schedule,
				"object-hit-event", t->id, (double) o->tsd.torpedo.ship_id);

	/* calculate impact point */
	ix = o->x + o->vx * delta_t;
	iy = o->y + o->vy * delta_t;
	iz = o->z + o->vz * delta_t;
	/* not sure this time stuff is quite correct, may need universe_absolute_time
	 * as part of the calculation of impact_fractional_time
	 */
	impact_time = universe_timestamp;
	impact_fractional_time = (double) delta_t;

	notify_the_cops(o, entity);

	if (t->type == OBJTYPE_STARBASE) {
		t->tsd.starbase.under_attack = 1;
		add_starbase_attacker(t, o->tsd.torpedo.ship_id);
		calculate_laser_starbase_damage(t, snis_randn(255));
		send_detonate_packet(t, ix, iy, iz, impact_time, impact_fractional_time);
		if (!t->alive)
			delete_from_clients_and_server(t);
		return;
	}

	if (t->type == OBJTYPE_SHIP1 || t->type == OBJTYPE_SHIP2) {
		if (t->type != OBJTYPE_SHIP1 ||
			t->tsd.ship.damage.maneuvering_damage > 150)
			do_collision_impulse(t, o);
		calculate_torpedo_damage(t);
		send_ship_damage_packet(t);
		send_detonate_packet(t, ix, iy, iz, impact_time, impact_fractional_time);
		attack_your_attacker(t, lookup_entity_by_id(o->tsd.torpedo.ship_id));
	} else if (t->type == OBJTYPE_ASTEROID || t->type == OBJTYPE_CARGO_CONTAINER) {
		if (t->alive)
			t->alive--;
	} else if (t->type == OBJTYPE_TURRET) {
		calculate_torpedo_damage(t);
	} else if (t->type == OBJTYPE_SPACEMONSTER) {
		spacemonster_set_antagonist(t, o->tsd.torpedo.ship_id);
		calculate_torpedo_damage(t);
	}

	if (!t->alive) {
		(void) add_explosion(t->x, t->y, t->z, 50, 150, 50, t->type);
		/* TODO -- these should be different sounds */
		/* make sound for players that got hit */
		/* make sound for players that did the hitting */
		snis_queue_add_sound(EXPLOSION_SOUND, ROLE_SOUNDSERVER, o->tsd.torpedo.ship_id);
		if (t->type == OBJTYPE_SHIP1 || t->type == OBJTYPE_SHIP2) {
			int i = lookup_by_id(o->tsd.torpedo.ship_id);
			if (i > 0)
				pop_ai_attack_mode(&go[i]);
		}
		if (t->type != OBJTYPE_SHIP1) {
			if (t->type == OBJTYPE_SHIP2)
				make_derelict(t);
			respawn_object(t);
			delete_from_clients_and_server(t);
			
		} else {
			snis_queue_add_sound(EXPLOSION_SOUND,
				ROLE_SOUNDSERVER, t->id);
			schedule_callback(event_callback, &callback_schedule,
					"player-death-callback", t->id);
		}
	} else {
		(void) add_explosion(t->x, t->y, t->z, 50, 5, 5, t->type);
		snis_queue_add_sound(DISTANT_TORPEDO_HIT_SOUND, ROLE_SOUNDSERVER, t->id);
		snis_queue_add_sound(TORPEDO_HIT_SOUND, ROLE_SOUNDSERVER, o->tsd.torpedo.ship_id);
	}
}

static void torpedo_move(struct snis_entity *o)
{
	if (o->alive)
		o->alive--;
	if (o->alive == 0) {
		delete_from_clients_and_server(o);
		return;
	}
	o->timestamp = universe_timestamp;
	set_object_location(o, o->x + o->vx, o->y + o->vy, o->z + o->vz);
	space_partition_process(space_partition, o, o->x, o->z, o,
			torpedo_collision_detection);
	if (!o->alive)
		delete_from_clients_and_server(o);
}

static void update_ship_orientation(struct snis_entity *o);
static void missile_move(struct snis_entity *o)
{
	int i;
	struct snis_entity *target;
	union vec3 desired_v, to_target, target_v;
	float flight_time;

	if (o->alive)
		o->alive--;
	if (o->alive == 0) {
		missile_explode(o);
		return;
	}
	o->timestamp = universe_timestamp;

	i = lookup_by_id(o->tsd.missile.target_id);
	if (i >= 0) { /* our target is still around */
		target = &go[i];
		if (target->type == OBJTYPE_SHIP2) {
			target->tsd.ship.missile_lock_detected = 10;
			/* NPCs don't notice missiles instantly. */
			if (o->alive < missile_lifetime - missile_countermeasure_delay)
				push_avoid_missile(target, o);
		} else if (target->type == OBJTYPE_SHIP1) {
			if (target->tsd.ship.missile_lock_detected == 0)
				snis_queue_add_text_to_speech("Missile lock detected.",
					ROLE_TEXT_TO_SPEECH, target->id);
			target->tsd.ship.missile_lock_detected = 10;
		}

		/* Calculate desired velocity */
		to_target.v.x = target->x - o->x;
		to_target.v.y = target->y - o->y;
		to_target.v.z = target->z - o->z;

		target_v.v.x = target->vx;
		target_v.v.y = target->vy;
		target_v.v.z = target->vz;

		flight_time = vec3_magnitude(&to_target) / missile_max_velocity;
		vec3_mul_self(&target_v, flight_time);
		vec3_add_self(&to_target, &target_v);
		vec3_normalize_self(&to_target);
		vec3_mul(&desired_v, &to_target, missile_max_velocity);
	} else { /* our target is gone, just keep going straight */
		desired_v.v.x = o->vx;
		desired_v.v.y = o->vy;
		desired_v.v.z = o->vz;
		vec3_normalize_self(&desired_v);
		vec3_mul_self(&desired_v, missile_max_velocity);
	}

	/* Update velocity towards desired velocity */
	o->vx += clampf(0.1 * (desired_v.v.x - o->vx), -max_missile_deltav, max_missile_deltav);
	o->vy += clampf(0.1 * (desired_v.v.y - o->vy), -max_missile_deltav, max_missile_deltav);
	o->vz += clampf(0.1 * (desired_v.v.z - o->vz), -max_missile_deltav, max_missile_deltav);

	/* Move missile */
	set_object_location(o, o->x + o->vx, o->y + o->vy, o->z + o->vz);
	update_ship_orientation(o); /* Update missile orienation */

	/* See if we hit something */
	space_partition_process(space_partition, o, o->x, o->z, o,
			missile_collision_detection);
}

static double __attribute__((unused)) point_to_line_dist(double lx1, double ly1,
				double lx2, double ly2, double px, double py)
{
	double normal_length = hypot(lx1 - lx2, ly1 - ly2);
	return fabs((px - lx1) * (ly2 - ly1) - (py - ly1) * (lx2 - lx1)) / normal_length;
}

static double point_to_3d_line_dist(double lx1, double ly1, double lz1,
				double lx2, double ly2, double lz2,
				double px, double py, double pz)
{
	union vec3 p0, p1, p2, t1, t2, t3;
	float numerator, denominator;

	p0.v.x = px;
	p0.v.y = py;
	p0.v.z = pz;

	p1.v.x = lx1;
	p1.v.y = ly1;
	p1.v.z = lz1;

	p2.v.x = lx2;
	p2.v.y = ly2;
	p2.v.z = lz2;

	/* see http://mathworld.wolfram.com/Point-LineDistance3-Dimensional.html */
	vec3_sub(&t1, &p0, &p1);
	vec3_sub(&t2, &p0, &p2);
	vec3_cross(&t3, &t1, &t2);
	numerator = vec3_magnitude(&t3);
	vec3_sub(&t3, &p2, &p1);
	denominator = vec3_magnitude(&t3);
	if (denominator < 0.0001) {
		stacktrace("Bad denominator");
		return 0;
	}
	return numerator / denominator;
}

static int laser_point_collides(double lx1, double ly1, double lz1,
				double lx2, double ly2, double lz2,
				double px, double py, double pz, double tolerance)
{
	if (px < lx1 && px < lx2)
		return 0;
	if (px > lx1 && px > lx2)
		return 0;
	if (py < ly1 && py < ly2)
		return 0;
	if (py > ly1 && py > ly2)
		return 0;
	if (pz > lz1 && pz > lz2)
		return 0;
	if (pz < lz1 && pz < lz2)
		return 0;
	return (point_to_3d_line_dist(lx1, ly1, lz1, lx2, ly2, lz2, px, py, pz) < tolerance);
}

static void laser_collision_detection(void *context, void *entity)
{
	struct snis_entity *o = context;
	struct snis_entity *t = entity;  /* target */
	double tolerance = 350.0;
	double ix, iy, iz, impact_fractional_time;
	uint32_t impact_time;
	float delta_t;

	if (!t->alive)
		return;
	if (t == o)
		return;
	if (t->type != OBJTYPE_SHIP1 && t->type != OBJTYPE_SHIP2 &&
		t->type != OBJTYPE_STARBASE && t->type != OBJTYPE_ASTEROID &&
		t->type != OBJTYPE_TORPEDO && t->type != OBJTYPE_CARGO_CONTAINER &&
		t->type != OBJTYPE_BLOCK && t->type != OBJTYPE_SPACEMONSTER)
		return;

	if (t->type == OBJTYPE_BLOCK) {
		double dist2;
		union vec3 laser_pos, t_pos, closest_point, normal_vec;

		dist2 = object_dist2(o, t);
		if (dist2 > t->tsd.block.shape.overall_radius * t->tsd.block.shape.overall_radius)
			return;
		laser_pos.v.x = o->x;
		laser_pos.v.y = o->y;
		laser_pos.v.z = o->z;
		t_pos.v.x = t->x;
		t_pos.v.y = t->y;
		t_pos.v.z = t->z;

		shape_closest_point(&laser_pos, &t_pos, &t->orientation, &t->tsd.block.shape,
					&closest_point, &normal_vec);

		dist2 = dist3dsqrd(o->x - closest_point.v.x, o->y - closest_point.v.y, o->z - closest_point.v.z);
		if (dist2 > 0.5 * LASER_VELOCITY)
			return;
		o->alive = 0; /* hit!!!!, laser_move() will delete the laser. */
		schedule_callback2(event_callback, &callback_schedule,
					"object-hit-event", t->id, (double) o->tsd.laser.ship_id);
		(void) add_explosion(closest_point.v.x, closest_point.v.y, closest_point.v.z, 50, 5, 5, t->type);
		snis_queue_add_sound(DISTANT_PHASER_HIT_SOUND, ROLE_SOUNDSERVER, t->id);
		block_add_to_naughty_list(t, o->tsd.laser.ship_id);
		calculate_laser_damage(t, o->tsd.laser.wavelength,
			(float) o->tsd.laser.power * LASER_PROJECTILE_BOOST);
		return;
	}

	if (t->id == o->tsd.laser.ship_id)
		return; /* can't laser yourself. */

	/* make sure torpedoes aren't *too* easy to hit */
	if (t->type == OBJTYPE_TORPEDO)
		tolerance = 30.0;

	if (!laser_point_collides(o->x + o->vx, o->y + o->vy, o->z + o->vz,
				o->x - o->vx, o->y - o->vy, o->z - o->vz,
				t->x, t->y, t->z, tolerance))
		return; /* not close enough */

	if (!projectile_collides(o->x, o->y, o->z, o->vx, o->vy, o->vz, 0.5 * tolerance,
				t->x, t->y, t->z, t->vx, t->vy, t->vz, 0.5 * tolerance, &delta_t))
		return;

	/* Calculate impact point */
	ix = o->x + o->vx * delta_t;
	iy = o->y + o->vy * delta_t;
	iz = o->z + o->vz * delta_t;
	/* not sure this time stuff is quite correct, may need universe_absolute_time
	 * as part of the calculation of impact_fractional_time
	 */
	impact_time = universe_timestamp;
	impact_fractional_time = (double) delta_t;
		
	/* hit!!!! */
	o->alive = 0;
	notify_the_cops(o, entity);
	schedule_callback2(event_callback, &callback_schedule,
				"object-hit-event", t->id, o->tsd.laser.ship_id);

	if (t->type == OBJTYPE_STARBASE) {
		t->tsd.starbase.under_attack = 1;
		/* FIXME: looks like lasers do not harm starbases... seems wrong */
		send_detonate_packet(t, ix, iy, iz, impact_time, impact_fractional_time);
		if (!t->alive)
			delete_from_clients_and_server(t);
		return;
	}

	if (t->type == OBJTYPE_SHIP1 || t->type == OBJTYPE_SHIP2) {
		calculate_laser_damage(t, o->tsd.laser.wavelength,
			(float) o->tsd.laser.power * LASER_PROJECTILE_BOOST);
		send_ship_damage_packet(t);
		attack_your_attacker(t, lookup_entity_by_id(o->tsd.laser.ship_id));
		send_detonate_packet(t, ix, iy, iz, impact_time, impact_fractional_time);
	}

	if (t->type == OBJTYPE_TURRET) {
		calculate_laser_damage(t, o->tsd.laser.wavelength,
			(float) o->tsd.laser.power * LASER_PROJECTILE_BOOST);
	}

	if (t->type == OBJTYPE_ASTEROID || t->type == OBJTYPE_TORPEDO ||
		t->type == OBJTYPE_CARGO_CONTAINER)
		if (t->alive)
			t->alive--;

	if (t->type == OBJTYPE_SPACEMONSTER) {
		calculate_laser_damage(t, o->tsd.laser.wavelength,
			(float) o->tsd.laser.power * LASER_PROJECTILE_BOOST);
		spacemonster_set_antagonist(t, o->id);
	}

	if (!t->alive) {
		(void) add_explosion(t->x, t->y, t->z, 50, 150, 50, t->type);
		/* TODO -- these should be different sounds */
		/* make sound for players that got hit */
		/* make sound for players that did the hitting */
		snis_queue_add_sound(EXPLOSION_SOUND,
				ROLE_SOUNDSERVER, o->tsd.laser.ship_id);
		if (t->type != OBJTYPE_SHIP1) {
			if (t->type == OBJTYPE_SHIP2)
				make_derelict(t);
			respawn_object(t);
			delete_from_clients_and_server(t);
		} else {
			snis_queue_add_sound(EXPLOSION_SOUND, ROLE_SOUNDSERVER, t->id);
			schedule_callback(event_callback, &callback_schedule,
					"player-death-callback", t->id);
		}
	} else {
		(void) add_explosion(t->x, t->y, t->z, 50, 5, 5, t->type);
		snis_queue_add_sound(DISTANT_PHASER_HIT_SOUND, ROLE_SOUNDSERVER, t->id);
		snis_queue_add_sound(PHASER_HIT_SOUND, ROLE_SOUNDSERVER,
					o->tsd.torpedo.ship_id);
	}
}

static void laser_move(struct snis_entity *o)
{
	if (o->alive)
		o->alive--;
	if (o->alive == 0) {
		delete_from_clients_and_server(o);
		return;
	}
	o->timestamp = universe_timestamp;
	set_object_location(o, o->x + o->vx, o->y + o->vy, o->z + o->vz);
	space_partition_process(space_partition, o, o->x, o->z, o,
			laser_collision_detection);
	if (!o->alive)
		delete_from_clients_and_server(o);
}

static void send_comms_packet(struct snis_entity *transmitter, char *sender, uint32_t channel, const char *format, ...);
static void taunt_player(struct snis_entity *alien, struct snis_entity *player)
{
	char buffer[1000];
	char name[100];
	char tmpbuf[50];
	int last_space = 0;
	int i, bytes_so_far = 0;
	char *start = buffer;
	static struct mtwist_state *mt = NULL;

	if (!mt)
		mt = mtwist_init(mtwist_seed);

	if (alien->tsd.ship.ai[0].ai_mode == AI_MODE_COP)
		cop_attack_warning(mt, buffer, sizeof(buffer) - 1, sizeof(buffer) - 1);
	else
		infinite_taunt(mt, buffer, sizeof(buffer) - 1);

	snprintf(name, sizeof(name), "%s: ", alien->sdata.name);
	for (i = 0; buffer[i]; i++) {
		buffer[i] = toupper(buffer[i]);
		if (buffer[i] == ' ')
			last_space = bytes_so_far;
		bytes_so_far++;
		if (last_space > 28) {
			strncpy(tmpbuf, start, bytes_so_far);
			tmpbuf[bytes_so_far] = '\0';
			send_comms_packet(alien, name, 0, tmpbuf);
			strcpy(name, "-  ");
			start = &buffer[i];
			bytes_so_far = 0;
			last_space = 0;
		}
	}
	if (bytes_so_far > 0) {
		strcpy(tmpbuf, start);
		send_comms_packet(alien, name, 0, tmpbuf);
	}
}

static void calculate_torpedo_velocities(double ox, double oy, double oz, 
			double tx, double ty, double tz, double speed,
			double *vx, double *vy, double *vz)
{
	struct mat41 m;

	m.m[0] = tx - ox;
	m.m[1] = ty - oy;
	m.m[2] = tz - oz;
	m.m[3] = 1.0;

	normalize_vector(&m, &m);

	*vx = m.m[0] * speed;
	*vy = m.m[1] * speed;
	*vz = m.m[2] * speed;
}

static int add_torpedo(double x, double y, double z,
		double vx, double vy, double vz, uint32_t ship_id);
static int add_laser(double x, double y, double z,
		double vx, double vy, double vz, union quat *orientation,
		uint32_t ship_id);
static uint8_t update_phaser_banks(int current, int rate, int max);

static void ship_choose_new_attack_victim(struct snis_entity *o)
{
	struct command_data *cmd_data;
	int i, nids1, nids2;
	double d;
	int eid;

	cmd_data = &o->tsd.ship.cmd_data;
	if (cmd_data->command != DEMON_CMD_ATTACK)
		return;

	nids1 = cmd_data->nids1;
	nids2 = cmd_data->nids2;

	/* delete the dead ones. */
	for (i = 0; i < cmd_data->nids2;) {
		int index;
		struct snis_entity *e;

		index = lookup_by_id(cmd_data->id[nids1 + i]);
		if (index < 0)
			goto delete_it;

		e = &go[index];
		if (e->alive) {
			i++;
			continue;
		}
delete_it:
		if (i == nids2 - 1) {
			cmd_data->nids2--;
			break;
		}
		cmd_data->id[nids1 + i] = cmd_data->id[nids1 + nids2 - 1];
		cmd_data->nids2--;
	}

	/* find the nearest one */
	eid = -1;
	d = -1.0;
	for (i = 0; i < cmd_data->nids2; i++) {
		double dist2;
		struct snis_entity *e;
		int index;

		index = lookup_by_id(cmd_data->id[nids1 + i]);
		if (index < 0)
			continue;

		e = &go[index];
		if (!e->alive)
			continue;

		dist2 = dist3dsqrd(o->x - e->x, o->y - e->y, o->z - e->z); 
		if (d < 0 || d > dist2) {
			d = dist2;
			eid = go[index].id;
		}
	}
	push_attack_mode(o, eid, 0);
}

static void maybe_find_spacemonster_a_home(struct snis_entity *o)
{
	int i, nebula_count, home_nebula;

	if (snis_randn(1000) > 50)
		return;

	if (o->tsd.spacemonster.home != (uint32_t) -1) {
		i = lookup_by_id(o->tsd.spacemonster.home);
		if (i >= 0 && go[i].alive && go[i].type == OBJTYPE_NEBULA)
			return; /* already have a home */
		else
			o->tsd.spacemonster.home = (uint32_t) -1; /* home is gone */
	}

	/* Find our spacemonster a home */
	nebula_count = 0;
	for (i = 0; i <= snis_object_pool_highest_object(pool); i++)
		if (go[i].alive && go[i].type == OBJTYPE_NEBULA)
			nebula_count++;
	if (nebula_count != 0) {
		home_nebula = snis_randn(nebula_count);
		for (i = 0; i <= snis_object_pool_highest_object(pool); i++) {
			if (go[i].alive && go[i].type == OBJTYPE_NEBULA) {
				if (home_nebula == 0) {
					o->tsd.spacemonster.home = go[i].id;
					break;
				} else {
					home_nebula--;
				}
			}
		}
	} else {
		o->tsd.spacemonster.home = (uint32_t) -1;
	}
}

static void spacemonster_set_antagonist(struct snis_entity *o, uint32_t id)
{
	int i;

	for (i = 0; i < o->tsd.spacemonster.nantagonists; i++) {
		if (o->tsd.spacemonster.antagonist[i] == id) {
			o->tsd.spacemonster.current_antagonist = i;
			return; /* already set */
		}
	}
	if (o->tsd.spacemonster.nantagonists < 5) {
		o->tsd.spacemonster.antagonist[o->tsd.spacemonster.nantagonists] = id;
		o->tsd.spacemonster.current_antagonist = o->tsd.spacemonster.nantagonists;
		o->tsd.spacemonster.nantagonists++;
		return;
	}
	i = snis_randn(1000) % 5;
	o->tsd.spacemonster.antagonist[i] = id;
	o->tsd.spacemonster.current_antagonist = i;
}

static int nl_find_nearest_object_of_type(uint32_t id, int objtype);

static void spacemonster_collision_process(void *context, void *entity)
{
	struct snis_entity *o = context;
	struct snis_entity *thing = entity;
	float dist;

	if (thing == o)
		return;

	switch (thing->type) {
	case OBJTYPE_SPACEMONSTER:
		if (o->tsd.spacemonster.spacemonster_dist < 0.0) {
			o->tsd.spacemonster.nearest_spacemonster = thing->id;
			o->tsd.spacemonster.spacemonster_dist = object_dist(o, thing);
		} else if (o->id != o->tsd.spacemonster.nearest_spacemonster) {
			dist = object_dist(o, thing);
			if (dist < o->tsd.spacemonster.spacemonster_dist) {
				o->tsd.spacemonster.spacemonster_dist = dist;
				o->tsd.spacemonster.nearest_spacemonster = thing->id;
			}
		}
		break;
	case OBJTYPE_ASTEROID:
		if (o->tsd.spacemonster.asteroid_dist < 0.0) {
			o->tsd.spacemonster.nearest_asteroid = thing->id;
			o->tsd.spacemonster.asteroid_dist = object_dist(o, thing);
		} else if (o->id != o->tsd.spacemonster.nearest_asteroid) {
			dist = object_dist(o, thing);
			if (dist < o->tsd.spacemonster.asteroid_dist) {
				o->tsd.spacemonster.asteroid_dist = dist;
				o->tsd.spacemonster.nearest_asteroid = thing->id;
			}
		}
		break;
	case OBJTYPE_SHIP1:
	case OBJTYPE_SHIP2:
		if (o->tsd.spacemonster.ship_dist < 0.0) {
			o->tsd.spacemonster.nearest_ship = thing->id;
			o->tsd.spacemonster.ship_dist = object_dist(o, thing);
		} else if (o->id != o->tsd.spacemonster.nearest_ship) {
			dist = object_dist(o, thing);
			if (dist < o->tsd.spacemonster.ship_dist) {
				o->tsd.spacemonster.ship_dist = dist;
				o->tsd.spacemonster.nearest_ship = thing->id;
			}
		}
		/* Space monster gets angry if healthy and ship is too close
		 * Space monster gets fearful if not healthy and ship is too close
		 * Ship interactions happen in ship collision processing functions.
		 */
		if (o->tsd.spacemonster.ship_dist >= 0.0) {
			if (o->tsd.spacemonster.ship_dist < spacemonster_aggro_radius &&
				snis_randn(1000) < 200) {
				if (o->tsd.spacemonster.health > 100) {
					if (o->tsd.spacemonster.anger < 255)
						o->tsd.spacemonster.anger += snis_randn(10);
				} else {
					if (o->tsd.spacemonster.fear < 255)
						o->tsd.spacemonster.fear += snis_randn(10);
				}
			}
		}
		break;
	case OBJTYPE_NEBULA:
		break;
	}
}

static void spacemonster_flee(struct snis_entity *o)
{
	int i, n, count;
	union vec3 total;
	double mag;

	total.v.x = 0;
	total.v.y = 0;
	total.v.z = 0;

	if (o->tsd.spacemonster.decision_age == 0) { /* Have just decided to flee? */
		count = 0;
		for (i = 0; i < o->tsd.spacemonster.nantagonists; i++) {
			union vec3 v, total;
			n = lookup_by_id(o->tsd.spacemonster.antagonist[i]);
			if (n < 0)
				continue;
			count++;
			v.v.x = o->x - go[n].x;
			v.v.y = o->y - go[n].y;
			v.v.z = o->z - go[n].z;
			mag = vec3_magnitude(&v);
			vec3_normalize_self(&v);
			vec3_mul_self(&v, 1.0 / mag);
			vec3_add_self(&total, &v);
		}
		if (count > 0 && snis_randn(1000) < 750) {
			vec3_normalize_self(&total);
			vec3_mul_self(&total, spacemonster_flee_dist);
			o->tsd.spacemonster.dest = total;
		} else {
			random_point_on_sphere(spacemonster_flee_dist, &total.v.x, &total.v.y, &total.v.z);
		}
	}
	o->tsd.spacemonster.decision_age++;
}

static void spacemonster_play(struct snis_entity *o)
{
	int i;
	union vec3 v;
	double dist;
	float vel, esttime;

	if (o->tsd.spacemonster.nearest_spacemonster == -1)
		return;
	i = lookup_by_id(o->tsd.spacemonster.nearest_spacemonster);
	if (i < 0)
		return;
	o->tsd.spacemonster.dest.v.x = go[i].x;
	o->tsd.spacemonster.dest.v.y = go[i].y;
	o->tsd.spacemonster.dest.v.z = go[i].z;
	v.v.x = go[i].x - o->x;
	v.v.y = go[i].y - o->y;
	v.v.z = go[i].z - o->z;
	dist = vec3_magnitude(&v);
	vel = sqrt(go[i].vx * go[i].vx + go[i].vy * go[i].vy + go[i].vz * go[i].vz);
	if (vel < 0.01)
		vel = 0.01;
	esttime = 0.75 * dist / vel;
	v.v.x += go[i].vx * esttime;
	v.v.y += go[i].vy * esttime;
	v.v.z += go[i].vz * esttime;
	vec3_normalize_self(&v);
	if (dist > 3000) {
		vec3_mul_self(&v, max_spacemonster_velocity);
		o->tsd.spacemonster.dvx = v.v.x;
		o->tsd.spacemonster.dvy = v.v.y;
		o->tsd.spacemonster.dvz = v.v.z;
	} else if (snis_randn(1000) < 50) {
			random_point_on_sphere(1.0, &v.v.x, &v.v.y, &v.v.y);
			vec3_mul_self(&v, max_spacemonster_velocity);
			o->tsd.spacemonster.dvx = v.v.x;
			o->tsd.spacemonster.dvy = v.v.y;
			o->tsd.spacemonster.dvz = v.v.z;
	} else if (dist < 500) {
		union vec3 steer;
		float mul;

		steer.v.x = o->x - go[i].x;
		steer.v.y = o->y - go[i].y;
		steer.v.z = o->z - go[i].z;
		vec3_normalize_self(&steer);
		mul = max_spacemonster_velocity * 0.5;
		vec3_mul_self(&steer, mul);
		o->tsd.spacemonster.dvx += steer.v.x;
		o->tsd.spacemonster.dvy += steer.v.y;
		o->tsd.spacemonster.dvz += steer.v.z;
	}
}

static void spacemonster_fight(struct snis_entity *o)
{
	int i;
	union vec3 ship_vel, to_player, to_target;
	float esttime;

	if (o->tsd.spacemonster.nearest_ship == (uint32_t) -1 ||
		o->tsd.spacemonster.ship_dist < 0.0) {
		spacemonster_play(o);
		return;
	}
	i = lookup_by_id(o->tsd.spacemonster.nearest_ship);
	if (i < 0) {
		spacemonster_play(o);
		return;
	}
	if (o->tsd.spacemonster.ship_dist > spacemonster_aggro_radius) {
		spacemonster_play(o);
		o->tsd.spacemonster.anger = imax(o->tsd.spacemonster.anger - snis_randn(10), 0);
		o->tsd.spacemonster.fear = imax(o->tsd.spacemonster.fear - snis_randn(10), 0);
		return;
	}
	ship_vel.v.x = go[i].vx;
	ship_vel.v.y = go[i].vy;
	ship_vel.v.z = go[i].vz;
	to_player.v.x = go[i].x - o->x;
	to_player.v.y = go[i].y - o->y;
	to_player.v.z = go[i].z - o->z;
	esttime = 0.75 * vec3_magnitude(&to_player) / max_spacemonster_velocity;
	vec3_mul_self(&ship_vel, esttime);
	vec3_add(&to_target, &to_player, &ship_vel);
	vec3_normalize_self(&to_target);
	vec3_mul_self(&to_target, max_spacemonster_velocity);
	o->tsd.spacemonster.dvx = to_target.v.x;
	o->tsd.spacemonster.dvy = to_target.v.y;
	o->tsd.spacemonster.dvz = to_target.v.z;

	/* If too close to other spacemonster, steer away from it */
	if (o->tsd.spacemonster.nearest_spacemonster != -1) {
		i = lookup_by_id(o->tsd.spacemonster.nearest_spacemonster);
		if (i >= 0) {
			float dist = object_dist(o, &go[i]);
			if (dist < 500) { /* too close */
				union vec3 steer;

				steer.v.x = o->x - go[i].x;
				steer.v.y = o->y - go[i].y;
				steer.v.z = o->z - go[i].z;
				vec3_normalize_self(&steer);
				vec3_mul_self(&steer, 0.5 * max_spacemonster_velocity);
				o->tsd.spacemonster.dvx += steer.v.x;
				o->tsd.spacemonster.dvy += steer.v.y;
				o->tsd.spacemonster.dvz += steer.v.z;
			}
		}
	}
}

static void spacemonster_eat(struct snis_entity *o)
{
	int i;
	union vec3 v;
	double dist, mindist = -1.0;
	int found = -1;
	float vscale;
	int candidate;

	if (o->tsd.spacemonster.nearest_asteroid == -1 || snis_randn(1000) < 20) {
		/* Find the nearest asteroid somewhere We look sometimes randomly too
		 * because asteroids move, and the nearest one could change
		 */
		for (i = 0; i <= snis_object_pool_highest_object(pool); i++) {
			if (!go[i].alive || go[i].type != OBJTYPE_ASTEROID)
				continue;
			dist = object_dist(o, &go[i]);
			if (mindist < 0.0 || dist < mindist) {
				found = i;
				mindist = dist;
			}
		}
		if (found < 0)
			return;
		o->tsd.spacemonster.nearest_asteroid = go[found].id;
	}
	candidate = o->tsd.spacemonster.nearest_asteroid;
	i = lookup_by_id(candidate);
	if (i < 0)
		return;
	o->tsd.spacemonster.dest.v.x = go[i].x;
	o->tsd.spacemonster.dest.v.y = go[i].y;
	o->tsd.spacemonster.dest.v.z = go[i].z;
	v.v.x = go[i].x - o->x;
	v.v.y = go[i].y - o->y;
	v.v.z = go[i].z - o->z;
	dist = vec3_magnitude(&v);
	if (dist > 1000)
		vscale = max_spacemonster_velocity;
	else {
		if (dist <= 1000) {
			if (snis_randn(1000) < 100) {
				if (o->tsd.spacemonster.hunger > 105)
					o->tsd.spacemonster.hunger--;
				else
					o->tsd.spacemonster.hunger = 0;
			}
		}
		if (dist <= 500)
			vscale = 0.1;
		else
			vscale = max_spacemonster_velocity * (dist - 500.0) / 500.0;
	}
	vec3_normalize_self(&v);
	vec3_mul_self(&v, vscale);
	o->tsd.spacemonster.dvx = v.v.x;
	o->tsd.spacemonster.dvy = v.v.y;
	o->tsd.spacemonster.dvz = v.v.z;

	/* If too close to other spacemonster, steer away from it */
	if (o->tsd.spacemonster.nearest_spacemonster != -1) {
		i = lookup_by_id(o->tsd.spacemonster.nearest_spacemonster);
		if (i >= 0) {
			dist = object_dist(o, &go[i]);
			if (dist < 500) { /* too close */
				union vec3 steer;
				float mul;

				steer.v.x = o->x - go[i].x;
				steer.v.y = o->y - go[i].y;
				steer.v.z = o->z - go[i].z;
				vec3_normalize_self(&steer);
				mul = vscale * 0.5 + 0.5;
				vec3_mul_self(&steer, mul);
				o->tsd.spacemonster.dvx += steer.v.x;
				o->tsd.spacemonster.dvy += steer.v.y;
				o->tsd.spacemonster.dvz += steer.v.z;
			}
		}
	}
}

static void spacemonster_move(struct snis_entity *o)
{
	/* FIXME: make this better */
	float v, dx, dy, dz;
	set_object_location(o, o->x + o->vx, o->y + o->vy, o->z + o->vz);
	int anger, fear, hunger;

	maybe_find_spacemonster_a_home(o);

	anger = o->tsd.spacemonster.anger;
	fear = o->tsd.spacemonster.fear;
	hunger = o->tsd.spacemonster.hunger;

	if (snis_randn(1000) < 20)
		o->tsd.spacemonster.hunger = imin(hunger + 1, 255);

	if (o->tsd.spacemonster.hunger > 150 && snis_randn(1000) < 25
		&& o->tsd.spacemonster.health > 0)
			o->tsd.spacemonster.health--;

	/* Clear out distance knowledge so it won't be stale */
	o->tsd.spacemonster.asteroid_dist = -1.0;
	o->tsd.spacemonster.nearest_asteroid = (uint32_t) -1;
	o->tsd.spacemonster.spacemonster_dist = -1.0;
	o->tsd.spacemonster.nearest_spacemonster = (uint32_t) -1;
	o->tsd.spacemonster.ship_dist = -1.0;
	o->tsd.spacemonster.nearest_ship = (uint32_t) -1;
	/* Re-acquire distance knowledge */
	space_partition_process(space_partition, o, o->x, o->z, o,
		spacemonster_collision_process);

	/* See if we need to do something other than what we're already doing */
	if (fear >= anger && fear >= hunger && fear > 20) {
		if (o->tsd.spacemonster.mode != SPACEMONSTER_MODE_FLEE)
			o->tsd.spacemonster.decision_age = 0;
		o->tsd.spacemonster.mode = SPACEMONSTER_MODE_FLEE;
	} else if (anger >= fear && anger >= hunger && anger > 20) {
		if (o->tsd.spacemonster.mode != SPACEMONSTER_MODE_FIGHT)
			o->tsd.spacemonster.decision_age = 0;
		o->tsd.spacemonster.mode = SPACEMONSTER_MODE_FIGHT;
	} else if (hunger >= fear && hunger >= anger && hunger > 100) {
		if (o->tsd.spacemonster.mode != SPACEMONSTER_MODE_EAT)
			o->tsd.spacemonster.decision_age = 0;
		o->tsd.spacemonster.mode = SPACEMONSTER_MODE_EAT;
	} else {
		if (o->tsd.spacemonster.mode != SPACEMONSTER_MODE_PLAY)
			o->tsd.spacemonster.decision_age = 0;
		o->tsd.spacemonster.mode = SPACEMONSTER_MODE_PLAY;
	}

	switch (o->tsd.spacemonster.mode) {
	case SPACEMONSTER_MODE_FLEE:
		spacemonster_flee(o);
		break;
	case SPACEMONSTER_MODE_FIGHT:
		spacemonster_fight(o);
		break;
	case SPACEMONSTER_MODE_EAT:
		spacemonster_eat(o);
		break;
	case SPACEMONSTER_MODE_PLAY:
		spacemonster_play(o);
		break;
	default:
		if (snis_randn(1000) < 50) {
			random_point_on_sphere(1.0, &dx, &dy, &dz);
			v = snis_randn(1000) / 1000.0;
			v = v * max_spacemonster_velocity;
			o->tsd.spacemonster.dvx = v * dx;
			o->tsd.spacemonster.dvy = v * dy;
			o->tsd.spacemonster.dvz = v * dz;
		}
	}
	dx = o->tsd.spacemonster.dvx - o->vx;
	if (dx > max_spacemonster_accel)
		dx = max_spacemonster_accel;
	if (dx < -max_spacemonster_accel)
		dx = -max_spacemonster_accel;
	dy = o->tsd.spacemonster.dvy - o->vy;
	if (dy > max_spacemonster_accel)
		dy = max_spacemonster_accel;
	if (dy < -max_spacemonster_accel)
		dy = -max_spacemonster_accel;
	dz = o->tsd.spacemonster.dvz - o->vz;
	if (dz > max_spacemonster_accel)
		dz = max_spacemonster_accel;
	if (dz < -max_spacemonster_accel)
		dz = -max_spacemonster_accel;
	o->vx += dx;
	o->vy += dy;
	o->vz += dz;
	update_ship_orientation(o);

	/* Move tentacles */
	if (o->tsd.spacemonster.movement_countdown > 0) {
		o->tsd.spacemonster.movement_countdown--;
	} else {
		o->tsd.spacemonster.movement_countdown = snis_randn(20) + 5; /* 0.5 secs to 2.5 secs */
		o->tsd.spacemonster.seed = snis_randn(1000);
	}
	o->tsd.spacemonster.emit_intensity += snis_randn(10);
	o->timestamp = universe_timestamp;
}

static int __attribute__((unused)) in_nebula(double x, double y, double z)
{
	struct snis_entity *n;
	double r, dist;
	int i, j;

	for (i = 0; i < NNEBULA; i++) {
		j = nebulalist[i];
		if (j < 0)
			continue;
		n = &go[j];
		r = n->tsd.nebula.r * n->tsd.nebula.r;
		dist = dist3dsqrd(x - n->x, y - n->y, z - n->z);
		if (dist < r)
			return 1;
	}
	return 0;
}

static void check_for_incoming_fire(struct snis_entity *o)
{
	int i;

	if (o->type != OBJTYPE_SHIP1)
		return;

	for (i = 0; i < nbridges; i++) {
		if (bridgelist[i].shipid == o->id) {
			if (universe_timestamp - bridgelist[i].last_incoming_fire_sound_time >
				30 * 20) {
				snis_queue_add_sound(INCOMING_FIRE_DETECTED,
					ROLE_SOUNDSERVER, o->id);
				bridgelist[i].last_incoming_fire_sound_time = universe_timestamp;
			}
			break;
		}
	}
}

static void update_ship_orientation(struct snis_entity *o)
{
	union vec3 right = { { 1.0f, 0.0f, 0.0f } };
	union vec3 up = { { 0.0f, 1.0f, 0.0f } };
	union vec3 current = { { o->vx, o->vy, o->vz } };

	/* The value 0.05 is a bit finicky here.  Smaller values (e.g. 0.01) causes
	 * ships to "wiggle" when moving very slowly near their destinations.
	 * Higher values (0.1) causes ships to be unable to hit their destinations,
	 * and instead wander around them. 0.05 was determined empirically to be
	 * reduce wiggling and let ships hit their destinations reasonably well.
	 * Kind of gross though.
	 */
	if (fabs(o->vx) < 0.05 && fabs(o->vy) < 0.05 && fabs(o->vz) < 0.05)
		return;

	quat_from_u2v(&o->orientation, &right, &current, &up);
}

/* Returns 0 if not being towed, the id of the tow ship otherwise */
static uint32_t is_being_towed(struct snis_entity *o)
{
	int i, j, n;

	for (i = 0; i <= snis_object_pool_highest_object(pool); i++) {
		if (go[i].type != OBJTYPE_SHIP2)
			continue;
		if (!go[i].alive)
			continue;
		n = go[i].tsd.ship.nai_entries - 1;
		for (j = n; n >= 0; n--) {
			if (go[i].tsd.ship.ai[j].ai_mode != AI_MODE_TOW_SHIP)
				continue;
			if (go[i].tsd.ship.ai[j].u.tow_ship.ship_connected &&
				go[i].tsd.ship.ai[n].u.tow_ship.disabled_ship == o->id)
					return go[i].id;
		}
	}
	return 0;
}

/* Returns 0 if ship is not towing anything, the object ID of the thing it's towing otherwise */
static uint32_t ship_is_towing(struct snis_entity *o)
{
	int i, n;
	if (o->type != OBJTYPE_SHIP2)
		return 0;
	n = o->tsd.ship.nai_entries - 1;
	for (i = n; n >= 0; n--) {
		if (o->tsd.ship.ai[i].ai_mode != AI_MODE_TOW_SHIP)
			continue;
		if (o->tsd.ship.ai[i].u.tow_ship.ship_connected)
			return o->tsd.ship.ai[n].u.tow_ship.disabled_ship;
	}
	return 0;
}

static void update_towed_ship(struct snis_entity *o)
{
	int i;
	struct snis_entity *disabled_ship;
	uint32_t disabled_ship_id;
	union vec3 towing_offset = { { 0.0, -20.0, 0.0 } };

	if (o->tsd.ship.shiptype != SHIP_CLASS_MANTIS)
		return;
	disabled_ship_id = ship_is_towing(o);
	if (!disabled_ship_id)
		return;
	i = lookup_by_id(disabled_ship_id);
	if (i < 0)
		return;
	disabled_ship = &go[i];
	quat_rot_vec_self(&towing_offset, &o->orientation);
	disabled_ship->orientation = o->orientation;
	disabled_ship->x = o->x + towing_offset.v.x;
	disabled_ship->y = o->y + towing_offset.v.y;
	disabled_ship->z = o->z + towing_offset.v.z;
	disabled_ship->timestamp = universe_timestamp;
}

static void update_ship_position_and_velocity(struct snis_entity *o);
static int add_laserbeam(uint32_t origin, uint32_t target, int alive);

static void check_for_nearby_targets(struct snis_entity *o)
{
	uint32_t victim_id;

	/* check for nearby targets... */
	if (((universe_timestamp + o->id) & 0x0f) == 0) {
		victim_id = find_nearest_victim(o);
		int i;
		struct snis_entity *v;

		if (victim_id == -1) {/* no nearby victims */
			ai_trace(o->id, "CHECKING FOR NEARBY TARGETS, FOUND NONE");
			return;
		}

		i = lookup_by_id(victim_id);
		if (i >= 0) {
			double dist2;

			v = &go[i];
			dist2 = object_dist2(o, v);
			if (dist2 < PATROL_ATTACK_DIST * PATROL_ATTACK_DIST) {
				ai_trace(o->id, "CHECKING FOR NEARBY TARGETS, FOUND %u", victim_id);
				push_attack_mode(o, victim_id, 0);
			}
		}
	}
}

/* check if a planet is between two points */
static struct snis_entity *planet_between_points(union vec3 *ray_origin, union vec3 *target)
{
	int i;
	union vec3 ray_direction, sphere_origin;
	float target_dist;
	float planet_dist;

	vec3_sub(&ray_direction, target, ray_origin);
	target_dist = vec3_magnitude(&ray_direction);
	vec3_normalize_self(&ray_direction);

	for (i = 0; i <= snis_object_pool_highest_object(pool); i++) {
		if (!go[i].alive)
			continue;
		if (go[i].type != OBJTYPE_PLANET)
			continue;
		sphere_origin.v.x = go[i].x;
		sphere_origin.v.y = go[i].y;
		sphere_origin.v.z = go[i].z;
		if (!ray_intersects_sphere(ray_origin, &ray_direction,
						&sphere_origin,
						go[i].tsd.planet.radius * 1.05))
			continue;
		planet_dist = dist3d(sphere_origin.v.x - ray_origin->v.x,
					sphere_origin.v.y - ray_origin->v.y,
					sphere_origin.v.z - ray_origin->v.z);
		if (planet_dist < target_dist) /* planet blocks... */
			return &go[i];
	}
	return NULL; /* no planets blocking */
}

/* check if a planet is between two objects */
static struct snis_entity *planet_between_objs(struct snis_entity *origin,
				struct snis_entity *target)
{
	union vec3 from, to;

	from.v.x = origin->x;
	from.v.y = origin->y;
	from.v.z = origin->z;

	to.v.x = target->x;
	to.v.y = target->y;
	to.v.z = target->z;

	return planet_between_points(&from, &to);
}

static float calculate_threat_level(struct snis_entity *o)
{
	float toughness, threat_level;

	(void) find_nearest_victim(o); /* side effect, calc threat level. */
	toughness = (255.0 - o->tsd.ship.damage.shield_damage) / 255.0;
	threat_level = o->tsd.ship.threat_level / (toughness + 0.001);
	return threat_level;
}

static void count_nearby_cops(void *context, void *entity)
{
	struct snis_entity *ship = context;
	struct snis_entity *cop = entity;
	double dist2;

	if (cop->type != OBJTYPE_SHIP2)
		return;
	if (cop->tsd.ship.ai[0].ai_mode != AI_MODE_COP)
		return;

	dist2 = object_dist2(ship, cop);
	if (dist2 < SECURITY_RADIUS * SECURITY_RADIUS)
		ship->tsd.ship.in_secure_area = 1;
}

static int too_many_cops_around(struct snis_entity *o)
{
	space_partition_process(space_partition, o, o->x, o->z, o, count_nearby_cops);
	return o->tsd.ship.in_secure_area;
}

struct warp_inhibition {
	int inhibit;
	union vec3 ship_pos;
};

static void calculate_planet_warp_inhibition(void *context, void *entity)
{
	struct warp_inhibition *wi = context;
	struct snis_entity *gravity_source = entity;
	float dist2, limit, radius;

	if (wi->inhibit) /* We already know it's inhibited? */
		return;
	switch (gravity_source->type) {
	case OBJTYPE_PLANET:
		radius = gravity_source->tsd.planet.radius;
		break;
	case OBJTYPE_BLACK_HOLE:
		radius = gravity_source->tsd.black_hole.radius;
		break;
	default:
		return;
	}
	limit = warp_limit_dist_factor * radius;
	limit = limit * limit;
	dist2 = dist3dsqrd(gravity_source->x - wi->ship_pos.v.x,
			gravity_source->y - wi->ship_pos.v.y,
			gravity_source->z - wi->ship_pos.v.z);
	if (dist2 < limit)
		wi->inhibit = 1;
}

static int warp_inhibited_by_planet(struct snis_entity *ship)
{
	struct warp_inhibition wi;
	if (!limit_warp_near_planets)
		return 0;
	wi.inhibit = 0;
	wi.ship_pos.v.x = ship->x;
	wi.ship_pos.v.y = ship->y;
	wi.ship_pos.v.z = ship->z;
	space_partition_process(space_partition, ship, ship->x, ship->z, &wi, calculate_planet_warp_inhibition);
	return wi.inhibit;
}

static void fire_missile(struct snis_entity *shooter, uint32_t target_id);

/* o is ai controled entity doing the weapons firing
 * v is the victim object being fired upon
 * imacop is true if o is a cop
 * vdist is distance to victim (minus anything like planet radius)
 * extra_range is e.g. planet radius.
 */
static void ai_maybe_fire_weapon(struct snis_entity *o, struct snis_entity *v, int imacop,
					double vdist, float extra_range)
{
	if (snis_randn(1000) < (enemy_torpedo_fire_chance * 10) + imacop * 150 &&
		vdist <= (torpedo_lifetime * torpedo_velocity) + extra_range &&
		o->tsd.ship.next_torpedo_time <= universe_timestamp &&
		o->tsd.ship.torpedoes > 0 &&
		ship_type[o->tsd.ship.shiptype].has_torpedoes) {
		double dist, flight_time, tx, ty, tz, vx, vy, vz;
		/* int inside_nebula = in_nebula(o->x, o->y) || in_nebula(v->x, v->y); */

		if (v->type == OBJTYPE_PLANET || !planet_between_objs(o, v)) {
			dist = object_dist(v, o);
			flight_time = dist / torpedo_velocity;
			tx = v->x + (v->vx * flight_time);
			tz = v->z + (v->vz * flight_time);
			ty = v->y + (v->vy * flight_time);

			calculate_torpedo_velocities(o->x, o->y, o->z,
				tx, ty, tz, torpedo_velocity, &vx, &vy, &vz);

			ai_trace(o->id, "FIRING TORPEDO AT %u", v->id);
			add_torpedo(o->x, o->y, o->z, vx, vy, vz, o->id);
			o->tsd.ship.torpedoes--;
			/* FIXME: how do the torpedoes refill? */
			o->tsd.ship.next_torpedo_time = universe_timestamp +
				enemy_torpedo_fire_interval;
			check_for_incoming_fire(v);
		}
	} else {
		if (snis_randn(1000) < enemy_laser_fire_chance * 10 + imacop * 200 &&
			o->tsd.ship.next_laser_time <= universe_timestamp &&
			ship_type[o->tsd.ship.shiptype].has_lasers) {
			if (v->type == OBJTYPE_PLANET || !planet_between_objs(o, v)) {
				o->tsd.ship.next_laser_time = universe_timestamp +
					enemy_laser_fire_interval;
				ai_trace(o->id, "FIRING LASERBEAM AT %u", v->id);
				add_laserbeam(o->id, v->id, LASERBEAM_DURATION);
				check_for_incoming_fire(v);
			}
		} else {
			/* TODO: This probability may need tuning. */
			if (snis_randn(1000) < (enemy_missile_fire_chance * 10) + imacop * 200 &&
				o->tsd.ship.next_missile_time < universe_timestamp &&
				ship_type[o->tsd.ship.shiptype].has_missiles) {
				if (v->type == OBJTYPE_SHIP1 || v->type == OBJTYPE_SHIP2) {
					o->tsd.ship.next_missile_time = universe_timestamp +
						enemy_missile_fire_interval;
					ai_trace(o->id, "FIRING MISSILE AT %u", v->id);
					fire_missile(o, v->id);
					check_for_incoming_fire(v);
				}
			}
		}
	}
}

static void ai_attack_mode_brain(struct snis_entity *o)
{
	struct snis_entity *v;
	double destx, desty, destz, dx, dy, dz, vdist;
	int n, firing_range;
	double maxv;
	int notacop = 1;
	int imacop = 0;
	double extra_range;
	uint32_t assailant;

	n = o->tsd.ship.nai_entries - 1;
	assert(n >= 0);

	/* Don't evaluate this every tick */
	if ((universe_timestamp & 0x07) == (o->id & 0x07) &&
			calculate_threat_level(o) > threat_level_flee_threshold) {
		/* just change current mode to flee (pop + push) */
		assailant = o->tsd.ship.ai[n].u.attack.victim_id;
		o->tsd.ship.ai[n].ai_mode = AI_MODE_FLEE;
		o->tsd.ship.ai[n].u.flee.warp_countdown = 10 * (10 + snis_randn(10));
		o->tsd.ship.ai[n].u.flee.assailant = assailant;
		ai_trace(o->id, "ATTACK -> FLEE");
		return;
	}

	if (o->tsd.ship.ai[n].u.attack.victim_id == (uint32_t) -2) { /* victim was removed */
		ai_trace(o->id, "POP ATTACK, VICTIM WAS REMOVED");
		pop_ai_attack_mode(o);
		return;
	}

	if (o->tsd.ship.ai[n].u.attack.victim_id == (uint32_t) -1) {
		int victim_id = find_nearest_victim(o);

		if (victim_id == -1) { /* no nearby victims */
			ai_trace(o->id, "NO NEARBY VICTIM, POPPING ATTACK");
			pop_ai_attack_mode(o);
			return;
		}
		o->tsd.ship.ai[n].u.attack.victim_id = victim_id;
		calculate_attack_vector(o, MIN_COMBAT_ATTACK_DIST,
						MAX_COMBAT_ATTACK_DIST);
		ai_trace(o->id, "CALC ATTACK VECTOR TO %u", victim_id);
	}
	maxv = ship_type[o->tsd.ship.shiptype].max_speed;
	v = lookup_entity_by_id(o->tsd.ship.ai[n].u.attack.victim_id);
	if (!v) {
		ai_trace(o->id, "FAILED TO LOOKUP VICTIM %u", o->tsd.ship.ai[n].u.attack.victim_id);
		return;
	}
	if (!v->alive) {
		ai_trace(o->id, "VICTIM DEAD, POPPING ATTACK");
		pop_ai_attack_mode(o);
		return;
	}

	destx = v->x + o->tsd.ship.dox;
	desty = v->y + o->tsd.ship.doy;
	destz = v->z + o->tsd.ship.doz;
	dx = destx - o->x;
	dy = desty - o->y;
	dz = destz - o->z;
	vdist = object_dist(o, v);
	if (vdist > ATTACK_MODE_GIVE_UP_DISTANCE &&
		v->type != OBJTYPE_PLANET && v->type != OBJTYPE_STARBASE && !rts_mode) {
		ai_trace(o->id, "VICTIM TOO FAR, POPPING ATTACK");
		pop_ai_attack_mode(o);
		return;
	}

	/* Trigger the victim ship's emf detector */
	if (v->type == OBJTYPE_SHIP1 || v->type == OBJTYPE_SHIP2) {
		uint8_t emf_value = (uint8_t) (snis_randn(120) +
			130.0 * ATTACK_MODE_GIVE_UP_DISTANCE / vdist);
		if (v->tsd.ship.emf_detector < emf_value)
			v->tsd.ship.emf_detector = emf_value;
	}

	extra_range = 0.0;
	if (v->type == OBJTYPE_PLANET)
		extra_range = v->tsd.planet.radius;
	firing_range = (vdist <= LASER_RANGE + extra_range);
	o->tsd.ship.desired_velocity = maxv;

	/* Close enough to destination? */
	if (fabs(dx) < 400 && fabs(dy) < 400 && fabs(dz) < 400) {
		union vec3 vel, veln;

		/* pretty close to enemy? */
		if (vdist < 1000) {
			ai_trace(o->id, "ATTACK VICTIM %u CLOSE, ZIPPING PAST", v->id);
			/* continue zipping past target */
			vel.v.x = (float) o->vx;
			vel.v.y = (float) o->vy;
			vel.v.z = (float) o->vz;
			vec3_normalize(&veln, &vel);
			vec3_mul_self(&veln, 800.0f + snis_randn(600));
			set_ship_destination(o, veln.v.x, veln.v.y, veln.v.z);
		} else {
			ai_trace(o->id, "ATTACK VICTIM %u FAR, CALC ATK VECTOR", v->id);
			calculate_attack_vector(o, MIN_COMBAT_ATTACK_DIST,
							MAX_COMBAT_ATTACK_DIST);
		}
	}

	if (!firing_range) {
		ai_trace(o->id, "VICTIM %u OUT OF FIRING RANGE",
				o->tsd.ship.ai[n].u.attack.victim_id);
		return;
	}

	notacop = o->tsd.ship.ai[0].ai_mode != AI_MODE_COP;
	imacop = !notacop;

	if (notacop && too_many_cops_around(o)) {
		ai_trace(o->id, "TOO MANY COPS, POPPING ATTACK");
		pop_ai_stack(o); /* forget about attacking... do something else */
		return;
	}

	/* neutrals do not attack planets or starbases, and only select ships
	 * when attacked.
	 */
	if ((o->sdata.faction != 0 || imacop) ||
		(v->type != OBJTYPE_STARBASE && v->type != OBJTYPE_PLANET)) {
		ai_maybe_fire_weapon(o, v, imacop, vdist, extra_range);
		if (v->type == OBJTYPE_SHIP1 && snis_randn(10000) < 50)
			taunt_player(o, v);
	} else {
		/* FIXME: give neutrals soemthing to do so they don't just sit there */;
		ai_trace(o->id, "ATTACK - HIT DO NOTHING CASE");
	}
	if (notacop && o->sdata.faction != 0)
		check_for_nearby_targets(o);
}

static float ai_ship_travel_towards(struct snis_entity *o,
		float destx, float desty, float destz);
static int add_chaff(double x, double y, double z);
static void ai_avoid_missile_brain(struct snis_entity *o)
{
	int i, n;
	struct snis_entity *missile;
	n = o->tsd.ship.nai_entries - 1;
	union quat turn90, localturn90;
	union vec3 dir = { { 1.0, 0.0, 0.0 } };

	i = lookup_by_id(o->tsd.ship.ai[n].u.avoid_missile.id);
	if (i < 0) {
		pop_ai_stack(o);
		return;
	}
	missile = &go[i];

	/* Get the direction of the missile's travel */
	quat_rot_vec_self(&dir, &missile->orientation);
	/* Convert 90 degree rotation to missile's local coordinate system */
	quat_init_axis(&turn90, 0.0, 1.0, 0.0, 0.5 * M_PI);
	quat_conjugate(&localturn90, &turn90, &missile->orientation);
	/* Turn vector 90 degrees to get desired direction of travel to evade missile */
	quat_rot_vec_self(&dir, &localturn90);
	vec3_mul_self(&dir, 1000.0);
	dir.v.x += o->x;
	dir.v.y += o->y;
	dir.v.z += o->z;
	(void) ai_ship_travel_towards(o, dir.v.x, dir.v.y, dir.v.z);
	if (snis_randn(1000) < 250)
		(void) add_chaff(o->x, o->y, o->z);
	if (snis_randn(1000) < 50)
		add_laserbeam(o->id, missile->id, LASERBEAM_DURATION);
}

struct danger_info {
	struct snis_entity *me;
	union vec3 danger, friendly;
};

static void compute_danger_vectors(void *context, void *entity)
{
	struct snis_entity *o = entity;
	struct danger_info *di = context;
	float dist;

	if (o == di->me)
		return;

	if (o->type != OBJTYPE_STARBASE &&
		o->type != OBJTYPE_SHIP2 &&
		o->type != OBJTYPE_SPACEMONSTER)
		return;

	dist = dist3d(o->x - di->me->x, o->y - di->me->y, o->z - di->me->z);

	if (dist > XKNOWN_DIM / 10.0) /* too far, no effect */
		return;

	if (o->type != OBJTYPE_SPACEMONSTER && o->sdata.faction == di->me->sdata.faction) {
		union vec3 f, fn;
		float factor;

		f.v.x = o->x - di->me->x;
		f.v.y = o->y - di->me->y;
		f.v.z = o->z - di->me->z;
		if (fabs(f.v.x) < 0.01)
			f.v.x = 0.01;
		if (fabs(f.v.y) < 0.01)
			f.v.y = 0.01;
		if (fabs(f.v.z) < 0.01)
			f.v.z = 0.01;
		vec3_normalize(&fn, &f);
		factor = ((XKNOWN_DIM / 10.0) - dist) / (XKNOWN_DIM / 10.0);
		vec3_mul(&f, &fn, factor);
		vec3_add(&fn, &di->friendly, &f);
		di->friendly = fn;
	} else {
		float factor, hostility, fightiness;
		union vec3 d, dn;

		if (o->type == OBJTYPE_SPACEMONSTER)
			hostility = 1.0;
		else
			hostility = faction_hostility(o->sdata.faction, di->me->sdata.faction);
		fightiness = ((XKNOWN_DIM / 10.0) * hostility) / (dist + 1.0);
		d.v.x = di->me->x - o->x;
		d.v.y = di->me->y - o->y;
		d.v.z = di->me->z - o->z;
		if (fabs(d.v.x) < 0.01)
			d.v.x = 0.01;
		if (fabs(d.v.y) < 0.01)
			d.v.y = 0.01;
		if (fabs(d.v.z) < 0.01)
			d.v.z = 0.01;
		vec3_normalize(&dn, &d);
		factor = ((XKNOWN_DIM / 10.0) - dist) / (XKNOWN_DIM / 10.0) * fightiness;
		vec3_mul(&d, &dn, factor);
		vec3_add(&dn, &di->danger, &d);
		di->danger = dn;
	}
}

static void add_warp_effect(uint32_t oid, double ox, double oy, double oz, double dx, double dy, double dz)
{
	struct packed_buffer *pb;

	pb = snis_opcode_pkt("bwSSSSSS", OPCODE_ADD_WARP_EFFECT,
			oid,
			ox, (uint32_t) UNIVERSE_DIM,
			oy, (uint32_t) UNIVERSE_DIM,
			oz, (uint32_t) UNIVERSE_DIM,
			dx, (uint32_t) UNIVERSE_DIM,
			dy, (uint32_t) UNIVERSE_DIM,
			dz, (uint32_t) UNIVERSE_DIM);
	if (pb)
		send_packet_to_all_clients(pb, ROLE_ALL);
}

static void warp_ship(struct snis_entity *o, double x, double y, double z)
{
	float ox, oy, oz;

	ox = o->x;
	oy = o->y;
	oz = o->z;
	set_object_location(o, x, y, z);
	add_warp_effect(o->id, ox, oy, oz, o->x, o->y, o->z);
}

static void ai_flee_mode_brain(struct snis_entity *o)
{
	struct danger_info info;
	union vec3 thataway;
	double vdist;
	int i, n;

	n = o->tsd.ship.nai_entries - 1;
	if (n < 0) {
		ai_trace(o->id, "n < 0 at %s:%d\n", __FILE__, __LINE__);
		printf("n < 0 at %s:%d\n", __FILE__, __LINE__);
		return;
	}

	if ((universe_timestamp & 0x07) == (o->id & 0x07) &&
		calculate_threat_level(o) < threat_level_flee_threshold * 0.5) {
		ai_trace(o->id, "POPPING FLEE DUE TO REDUCED THREAT LEVEL");
		pop_ai_stack(o);
		return;
	}

	info.me = o;
	info.danger.v.x = 0.0;
	info.danger.v.y = 0.0;
	info.danger.v.z = 0.0;
	info.friendly.v.x = 0.0;
	info.friendly.v.y = 0.0;
	info.friendly.v.z = 0.0;
	space_partition_process(space_partition, o, o->x, o->z, &info,
				compute_danger_vectors);
	vec3_mul_self(&info.danger, 2.0);
	vec3_add(&thataway, &info.danger, &info.friendly);
	if (vec3_magnitude(&thataway) < 0.01) { /* it can happen that that info.thataway == { 0, 0, 0 } */
		ai_trace(o->id, "FLEE DANGER VECTOR INDETERMINATE");
		random_point_on_sphere(1.0, &thataway.v.x, &thataway.v.y, &thataway.v.z);
	} else {
		vec3_normalize_self(&thataway);
		ai_trace(o->id, "FLEE DANGER VECTOR %.2f, %.2f, %.2f", thataway.v.x, thataway.v.y, thataway.v.z);
	}
	vec3_mul_self(&thataway, 4000.0);
	set_ship_destination(o, o->x + thataway.v.x, o->y + thataway.v.y, o->z + thataway.v.z);
	o->tsd.ship.desired_velocity = ship_type[o->tsd.ship.shiptype].max_speed;

	o->tsd.ship.ai[n].u.flee.warp_countdown--;
	if (o->tsd.ship.ai[n].u.flee.warp_countdown <= 0) {
		o->tsd.ship.ai[n].u.flee.warp_countdown = 10 * (20 + snis_randn(10));
		warp_ship(o, o->tsd.ship.dox, o->tsd.ship.doy, o->tsd.ship.doz);
	}

	/* Maybe shoot at assailant if he is close enough */
	i = lookup_by_id(o->tsd.ship.ai[n].u.flee.assailant);
	if (i >= 0) {
		vdist = object_dist(&go[i], o);
		if (vdist < LASER_RANGE)
			ai_maybe_fire_weapon(o, &go[i], o->tsd.ship.ai[0].ai_mode == AI_MODE_COP,
						vdist, 0.0);
	}
}

static void ai_fleet_member_mode_brain(struct snis_entity *o)
{
	int i, n = o->tsd.ship.nai_entries - 1;
	struct ai_fleet_data *f = &o->tsd.ship.ai[n].u.fleet;
	struct snis_entity *leader;
	int position;
	union vec3 offset;
	int32_t leader_id;
	double dist2;

	position = fleet_position_number(f->fleet, o->id);
	leader_id = fleet_get_leader_id(f->fleet);
	if (leader_id == o->id) { /* I'm the leader now */
		ai_trace(o->id, "BECAME FLEET LEADER");
		o->tsd.ship.ai[n].ai_mode = AI_MODE_FLEET_LEADER;
		o->tsd.ship.ai[0].u.fleet.fleet_position = 0;
		setup_patrol_route(o);
		return;
	}
	i = lookup_by_id(leader_id);
	if (i < 0) {
		ai_trace(o->id, "FAILED TO FIND FLEET LEADER");
		return;
	}
	leader = &go[i];
	offset = fleet_position(f->fleet, position, &leader->orientation);
	set_ship_destination(o, offset.v.x + leader->x, offset.v.y + leader->y, offset.v.z + leader->z);
	o->tsd.ship.velocity = leader->tsd.ship.velocity * 1.05;

	dist2 = dist3dsqrd(o->x - o->tsd.ship.dox, o->y - o->tsd.ship.doy,
			o->z - o->tsd.ship.doz);

	if (dist2 > 75.0 * 75.0 && dist2 <= FLEET_WARP_DISTANCE * FLEET_WARP_DISTANCE) {
		/* catch up if too far away */
		ai_trace(o->id, "CATCHING UP TO FLEET POSITION");
		o->tsd.ship.velocity = leader->tsd.ship.velocity * 1.5;
	} else if (dist2 > FLEET_WARP_DISTANCE * FLEET_WARP_DISTANCE && snis_randn(100) < 8) {
		/* If distance is too far, just warp */
		ai_trace(o->id, "WARPING TO FLEET POSITION");
		warp_ship(o, o->tsd.ship.dox, o->tsd.ship.doy, o->tsd.ship.doz);
		o->vx = leader->vx;
		o->vy = leader->vy;
		o->vz = leader->vz;
		o->orientation = leader->orientation;
	}
}

static int inside_planet(float x, float y, float z)
{
	int i;
	float d2;
	struct snis_entity *o;

	for (i = 0; i <= snis_object_pool_highest_object(pool); i++) {
		o = &go[i];
		if (o->type == OBJTYPE_PLANET && o->alive) {
			d2 =	(x - o->x) * (x - o->x) +
				(y - o->y) * (y - o->y) +
				(z - o->z) * (z - o->z);
			if (d2 < (o->tsd.planet.radius * o->tsd.planet.radius) * 1.05)
				return 1;
		}
	}
	return 0;
}

static void ai_ship_warp_to(struct snis_entity *o, float destx, float desty, float destz)
{
	union vec3 v;

	v.v.x = destx - o->x;
	v.v.y = desty - o->y;
	v.v.z = destz - o->z;
	vec3_mul_self(&v, 0.90 + 0.05 * (float) snis_randn(100) / 100.0);
	if (!inside_planet(v.v.x, v.v.y, v.v.z)) {
		ai_trace(o->id, "WARPING TO %.2f, %.2f, %.2f",
				o->x + v.v.x, o->y + v.v.y, o->z + v.v.z);
		warp_ship(o, o->x + v.v.x, o->y + v.v.y, o->z + v.v.z);
		/* reset destination after warping to prevent backtracking */
		set_ship_destination(o, destx, desty, destz);
		o->timestamp = universe_timestamp;
	}
}

static int has_arrived_at_destination(struct snis_entity *o, float dist2)
{
	float max_speed, arrival_dist2;

	if (o->type != OBJTYPE_SHIP2)
		return 0;
	max_speed = ship_type[o->tsd.ship.shiptype].max_speed;
	arrival_dist2 = (max_speed * 10.0) * (max_speed * 10.0);
	if (arrival_dist2 < 100 * 100)
		arrival_dist2 = 100 * 100;
	return dist2 < arrival_dist2;
}

/* Push a oneshot patrol route to get from startv to endv while avoiding the planet */
static void push_planet_avoidance_route(struct snis_entity *o,
		union vec3 *startv, union vec3 *endv, struct snis_entity *planet)
{
	int i, n;
	union vec3 p, p2s, p2e, v, v2;
	float r1, r2, dr, r, x, y, z;
	struct ai_patrol_data *patrol;
	union quat rotation;
	float angle;
	const float min_altitude = 1000;
	const float max_altitude = 3000;

	n = o->tsd.ship.nai_entries;
	if (n >= MAX_AI_STACK_ENTRIES)
		return; /* FIXME: what can we do here? */

	p.v.x = planet->x;
	p.v.y = planet->y;
	p.v.z = planet->z;

	/* planet to ship vector */
	p2s.v.x = o->x - p.v.x;
	p2s.v.y = o->y - p.v.y;
	p2s.v.z = o->z - p.v.z;
	r1 = vec3_magnitude(&p2s);
	if (r1 > planet->tsd.planet.radius + max_altitude)
		r1 = planet->tsd.planet.radius + max_altitude;
	if (r1 < planet->tsd.planet.radius + min_altitude)
		r1 = planet->tsd.planet.radius + min_altitude;
	vec3_normalize_self(&p2s);

	/* planet to end of route vector */
	p2e.v.x = endv->v.x - p.v.x;
	p2e.v.y = endv->v.y - p.v.y;
	p2e.v.z = endv->v.z - p.v.z;
	r2 = vec3_magnitude(&p2e);
	if (r2 > planet->tsd.planet.radius + max_altitude)
		r1 = planet->tsd.planet.radius + max_altitude;
	if (r2 < planet->tsd.planet.radius + min_altitude)
		r1 = planet->tsd.planet.radius + min_altitude;
	vec3_normalize_self(&p2e);

	/* Figure a quaternion to rotate an arc around the planet from start to end */
	quat_from_u2v(&rotation, &p2s, &p2e, NULL);
	quat_to_axis(&rotation, &x, &y, &z, &angle);

	/* Divide the rotation into MAX_PATROL_POINTS smaller rotations */
	angle = angle / (float) MAX_PATROL_POINTS;
	quat_init_axis(&rotation, x, y, z, angle);

	/* Figure out how much to change the radius each step */
	dr = (r2 - r1) / (float) MAX_PATROL_POINTS;

	/* Set up the new oneshot patrol route */
	patrol = &o->tsd.ship.ai[n].u.patrol;
	patrol->oneshot = 1;
	patrol->npoints = MAX_PATROL_POINTS;
	patrol->max_hangout_time = 1; /* do not pause between waypoints */
	v = p2s; /* Start at the ship */
	r = r1;
	for (i = 0; i < MAX_PATROL_POINTS; i++) {
		vec3_normalize_self(&v);
		quat_rot_vec_self(&v, &rotation);	/* rotate v around the planet a bit */
		r = r + dr;		/* Change the radius a bit */
		vec3_mul(&v2, &v, r);	/* v2 = v scaled by radius */
		vec3_add_self(&v2, &p);	/* Add in the planet position */
		patrol->p[i] = v2;	/* Add waypoint to the patrol route */
	}
	o->tsd.ship.ai[n].ai_mode = AI_MODE_PATROL;
	o->tsd.ship.nai_entries++;	/* push new oneshot patrol route onto AI stack */
	set_patrol_waypoint_destination(o, 0);
}

static float ai_ship_travel_towards(struct snis_entity *o,
		float destx, float desty, float destz)
{
	double maxv = ship_type[o->tsd.ship.shiptype].max_speed;
	o->tsd.ship.desired_velocity = maxv;
	int warproll = 10000;

	union vec3 startv, endv;
	struct snis_entity *planet;

	startv.v.x = o->x;
	startv.v.y = o->y;
	startv.v.z = o->z;
	endv.v.x = destx;
	endv.v.y = desty;
	endv.v.z = destz;

	double dist2 = dist3dsqrd(o->x - destx, o->y - desty, o->z - destz);

	/* Check if there's a planet in the way and try to avoid it. */
	planet = planet_between_points(&startv, &endv);
	if (planet) {
		push_planet_avoidance_route(o, &startv, &endv, planet);
		return dist2;
	}

	if (dist2 > 2000.0 * 2000.0) {
		/* TODO: give ships some variety in movement? */

		/* Check that current destination isn't too far from desired destination,
		 * If we have just set a new destination, we want it to take effect right
		 * away, not just within the (possibly) 400 secs of the above variety
		 * injecting code.
		 */
		double dest_discrepancy = dist3dsqrd(
					o->tsd.ship.dox - destx,
					o->tsd.ship.doy - desty,
					o->tsd.ship.doz - destz);
		if (dest_discrepancy > 0.05 * dist2 * 0.05 * dist2) {
			set_ship_destination(o, destx, desty, destz);
		}
		/* sometimes just warp if it's too far... */
		if (snis_randn(warproll) < ship_type[o->tsd.ship.shiptype].warpchance
			&& !warp_inhibited_by_planet(o)) {
			if (rts_mode) {
				/* RTS mode has constraints on warping, because warping without
				 * connstraints means that spatial proximity doesn't matter wrt
				 * e.g., the order and speed in which bases may be captured, and
				 * spatial proximity *needs* to matter for the sake of the game.
				 * So we have to nerf warping of troop ships a bit, for example.
				 */
				union vec3 warpvec;
				double warpmag;
				warpvec.v.x = destx - o->x;
				warpvec.v.y = desty - o->y;
				warpvec.v.z = destz - o->z;
				warpmag = vec3_magnitude(&warpvec);
				if (warpmag < (XKNOWN_DIM / 15.0)) {
					/* If already pretty close, do not warp. This prevents warping
					 * right up to a starbase and entering, giving defenders a chance.
					 */
					set_ship_destination(o, destx, desty, destz);
					return dist2;
				}
				/* Limit max warp distance to a reasonably short distance. */
				if (warpmag > (XKNOWN_DIM / 20.0)) {
					vec3_normalize_self(&warpvec);
					vec3_mul_self(&warpvec, (XKNOWN_DIM / 20.0));
					destx = o->x + warpvec.v.x;
					desty = o->y + warpvec.v.y;
					destz = o->z + warpvec.v.z;
				}
			}
			ai_ship_warp_to(o, destx, desty, destz);
		}
	} else {
		set_ship_destination(o, destx, desty, destz);
	}
	return dist2;
}

static void mining_bot_unload_one_ore(struct snis_entity *bot,
					struct snis_entity *parent,
					struct ai_mining_bot_data *ai,
					uint8_t *ore, int commodity_index)
{
	int i, cargo_bay = -1;
	float quantity;

	/* Find cargo bay already containing some of this ore... */
	for (i = 0; i < parent->tsd.ship.ncargo_bays; i++)
		if (parent->tsd.ship.cargo[i].contents.item == commodity_index) {
			cargo_bay = i;
			break;
		}
	if (cargo_bay < 0) { /* or, find an empty cargo bay */
		for (i = 0; i < parent->tsd.ship.ncargo_bays; i++)
			if (parent->tsd.ship.cargo[i].contents.item == -1) {
				cargo_bay = i;
				break;
			}
	}
	if (cargo_bay < 0) { /* No room in cargo bays, just dump it */
		*ore = 0;
		return;
	}
	quantity = ((float) *ore / 255.0) * 10; /* TODO: Will this make degenerate qty close to zero? */
	if (parent->tsd.ship.cargo[cargo_bay].contents.item == -1) {
		parent->tsd.ship.cargo[cargo_bay].contents.item = commodity_index;
		parent->tsd.ship.cargo[cargo_bay].contents.qty = quantity; 
	} else {
		parent->tsd.ship.cargo[cargo_bay].contents.qty += quantity; 
	}
	parent->tsd.ship.cargo[cargo_bay].paid = 0.0;
	parent->tsd.ship.cargo[cargo_bay].origin = ai->asteroid;
	parent->tsd.ship.cargo[cargo_bay].dest = -1;
	parent->tsd.ship.cargo[cargo_bay].due_date = -1;
}

static void mining_bot_unload_ores(struct snis_entity *bot, 
					struct snis_entity *parent,
					struct ai_mining_bot_data *ai)
{
	if (!bot || !parent)
		return;
	uint32_t fuel, oxygen;
	uint64_t tmpval;

#define GOLD 0
#define PLATINUM 1
#define GERMANIUM 2
#define URANIUM 3

	mining_bot_unload_one_ore(bot, parent, ai, &ai->gold, GOLD);
	mining_bot_unload_one_ore(bot, parent, ai, &ai->platinum, PLATINUM);
	mining_bot_unload_one_ore(bot, parent, ai, &ai->germanium, GERMANIUM);
	mining_bot_unload_one_ore(bot, parent, ai, &ai->uranium, URANIUM);

	/* unload the fuel */
	fuel = (uint32_t) ((ai->fuel / 255.0) * 0.33 * UINT32_MAX);
	ai->fuel = 0;
	tmpval = parent->tsd.ship.fuel;
	tmpval += fuel;
	if (tmpval > UINT32_MAX)
		tmpval = UINT32_MAX;
	parent->tsd.ship.fuel = tmpval;

	/* unload the oxygen */
	oxygen = (uint32_t) ((ai->oxygen / 255.0) * 0.33 * UINT32_MAX);
	ai->oxygen = 0;
	tmpval = parent->tsd.ship.oxygen;
	tmpval += oxygen;
	if (tmpval > UINT32_MAX)
		tmpval = UINT32_MAX;
	parent->tsd.ship.oxygen = tmpval;

	snis_queue_add_sound(TRANSPORTER_SOUND, ROLE_SOUNDSERVER, parent->id);
}

static void ai_mining_mode_return_to_parent(struct snis_entity *o, struct ai_mining_bot_data *ai)
{
	int i, b, channel;
	struct snis_entity *parent;
	union vec3 offset = { { -50, -50, 0 } };

	i = lookup_by_id(ai->parent_ship);
	if (i < 0) {
		ai->orphan_time++;
		return;
	}
	ai->orphan_time = 0;
	parent = &go[i];
	quat_rot_vec_self(&offset, &parent->orientation);
	set_ship_destination(o, parent->x + offset.v.x, parent->y + offset.v.y, parent->z + offset.v.z);

	double dist2 = ai_ship_travel_towards(o, parent->x, parent->y, parent->z);
	if (has_arrived_at_destination(o, dist2) && ai->mode == MINING_MODE_RETURN_TO_PARENT) {
		b = lookup_bridge_by_shipid(parent->id);
		if (b < 0) {
			ai->orphan_time++;
		} else {
			ai->orphan_time = 0;
			ai_trace(o->id, "STANDING BY FOR TRANSPORT OF MATERIALS");
			send_comms_packet(o, o->sdata.name, bridgelist[b].npcbot.channel,
				" -- STANDING BY FOR TRANSPORT OF MATERIALS --");
		}
		ai->mode = MINING_MODE_STANDBY_TO_TRANSPORT_ORE;
		snis_queue_add_sound(MINING_BOT_STANDING_BY,
				ROLE_COMMS | ROLE_SOUNDSERVER | ROLE_SCIENCE, parent->id);

	}
	if (dist2 < 300.0 * 300.0 && ai->mode == MINING_MODE_STOW_BOT) {
		b = lookup_bridge_by_shipid(ai->parent_ship);
		if (b >= 0) {
			ai->orphan_time = 0;
			channel = bridgelist[b].npcbot.channel;
			ai_trace(o->id, "TRANSPORTED MATERIALS");
			send_comms_packet(o, o->sdata.name, channel, "--- TRANSPORTING MATERIALS ---");
			send_comms_packet(o, o->sdata.name, channel, "--- MATERIALS TRANSPORTED ---");
			send_comms_packet(o, o->sdata.name, bridgelist[b].npcbot.channel,
				" MINING BOT STOWING AND SHUTTING DOWN");
			bridgelist[b].npcbot.current_menu = NULL;
			bridgelist[b].npcbot.special_bot = NULL;
			/* Hack, we overload the tractor beam to hold ship id of salvaged derelict */
			if (o->tsd.ship.tractor_beam != 0xffffffff) {
				int s = bridgelist[b].nship_id_chips;
				bridgelist[b].nship_id_chips = s + 1;
				if (bridgelist[b].nship_id_chips > MAX_SHIP_ID_CHIPS)
					bridgelist[b].nship_id_chips = MAX_SHIP_ID_CHIPS;
				if (s >= MAX_SHIP_ID_CHIPS) {
					s = MAX_SHIP_ID_CHIPS - 1;
				}
				send_comms_packet(o, o->sdata.name, bridgelist[b].npcbot.channel,
						"TRANSFERRED SHIP ID CHIP TO INVENTORY");
				bridgelist[b].ship_id_chip[s] = o->tsd.ship.tractor_beam;
				o->tsd.ship.tractor_beam = 0xffffffff;
			}
		} else {
			ai->orphan_time++;
		}
		ai_trace(o->id, "MINING BOT STOWED");
		mining_bot_unload_ores(o, parent, ai);
		delete_from_clients_and_server(o);
		snis_queue_add_sound(MINING_BOT_STOWED,
			ROLE_SOUNDSERVER | ROLE_COMMS | ROLE_SCIENCE, parent->id);
		schedule_callback(event_callback, &callback_schedule, "mining-bot-stowed-event", (double) parent->id);
		parent->tsd.ship.mining_bots = 1;
	}
}

static float estimate_asteroid_radius(uint32_t id)
{
	int k, s;
	k = id % (NASTEROID_MODELS * NASTEROID_SCALES);
	s = k % NASTEROID_SCALES;
	return s ? (float) s * 10.0 * 25.0 : 3.0 * 25.0;
}

static void ai_mining_mode_approach_asteroid(struct snis_entity *o, struct ai_mining_bot_data *ai)
{
	int i;
	struct snis_entity *asteroid = NULL;
	float threshold;
	double x, y, z, vx, vy, vz;
	float my_speed = dist3d(o->vx, o->vy, o->vz);
	float time_to_travel = 0.0;
	int b;

	if (ai->object_or_waypoint < 128) {
		/* destination is asteroid or derelict  */
		i = lookup_by_id(ai->asteroid);
		if (i < 0) {
			/* asteroid/cargo container got blown up maybe */
			b = lookup_bridge_by_shipid(ai->parent_ship);
			if (b >= 0) {
				int channel = bridgelist[b].npcbot.channel;
				ai->orphan_time = 0;
				send_comms_packet(o, o->sdata.name, channel,
					"TARGET LOST -- RETURNING TO SHIP.");
				ai_trace(o->id, "MINING BOT TARGET LOST");
			} else {
				ai_trace(o->id, "MINING BOT ORPHANED");
				ai->orphan_time++;
			}
			ai->mode = MINING_MODE_RETURN_TO_PARENT;
			return;
		}
		asteroid = &go[i];
		x = asteroid->x;
		y = asteroid->y;
		z = asteroid->z;
		vx = asteroid->vx;
		vy = asteroid->vy;
		vz = asteroid->vz;
		switch (asteroid->type) {
		case OBJTYPE_ASTEROID:
			threshold = 2.0 * estimate_asteroid_radius(asteroid->id);
			break;
		default:
			threshold = 200.0;
			break;
		}
	} else { /* destination is waypoint */
		x = ai->wpx;
		y = ai->wpy;
		z = ai->wpz;
		vx = 0.0;
		vy = 0.0;
		vz = 0.0;
		threshold = ship_type[o->tsd.ship.shiptype].max_speed * 10.0;
		/* note: at this point, asteroid == NULL. */
	}
	float distance = dist3d(o->x - x, o->y - y, o->z - z);
	if (my_speed < 0.1) {
		set_ship_destination(o, x, y, z);
	} else {
		time_to_travel = distance / my_speed;
		set_ship_destination(o, x + vx * time_to_travel, y + vy * time_to_travel, z + vz * time_to_travel);
	}
	(void) ai_ship_travel_towards(o, x + vx * time_to_travel,
						y + vy * time_to_travel,
						z + vz * time_to_travel);
	if (!asteroid)
		return;

	double dist2 = object_dist2(o, asteroid);
	if (dist2 < threshold * threshold) {
		b = lookup_bridge_by_shipid(ai->parent_ship);
		if (b >= 0) {
			int channel = bridgelist[b].npcbot.channel;
			ai->orphan_time = 0;
			if (ai->object_or_waypoint < 128) /* object */
				switch (asteroid->type) {
				case OBJTYPE_ASTEROID:
					send_comms_packet(o, o->sdata.name, channel,
						"COMMENCING ORBITAL INJECTION BURN");
					ai_trace(o->id, "ORBITAL INJECTION BURN");
					break;
				case OBJTYPE_CARGO_CONTAINER:
					if (!ai->towing) {
						ai_trace(o->id, "PICKED UP CARGO CONTAINER");
						send_comms_packet(o, o->sdata.name, channel,
							"PICKED UP CARGO CONTAINER.");
						ai->towing = 1;
						ai->towed_object = asteroid->id;
					} else {
						ai_trace(o->id, "ARRIVING AT CARGO CONTAINER (TOWING ALREADY)");
						send_comms_packet(o, o->sdata.name, channel,
							"ARRIVING AT CARGO CONTAINER "
							"(ALREADY TOWING ANOTHER CONTAINER).");
					}
					break;
				default:
					ai_trace(o->id, "APPROACHING OBJECT");
					send_comms_packet(o, o->sdata.name, channel,
						"APPROACHING OBJECT.");
					break;
				}
			else {
				ai_trace(o->id, "ARRIVING AT WAYPOINT");
				send_comms_packet(o, o->sdata.name, channel,
					"ARRIVING AT WAYPOINT");
			}
		} else {
			ai_trace(o->id, "MINING BOT ORPHANED");
			ai->orphan_time++;
		}
		if (ai->object_or_waypoint < 128 && !ai->towing) { /* object */
			ai_trace(o->id, "MINING BOT APPROACH -> LAND ON ASTEROID");
			ai->mode = MINING_MODE_LAND_ON_ASTEROID;
		} else if (ai->towing) {
			ai->mode = MINING_MODE_RETURN_TO_PARENT;
			ai_trace(o->id, "MINING BOT APPROACH -> RETURN TO PARENT");
		} else {
			ai->mode = MINING_MODE_IDLE;
			ai_trace(o->id, "MINING BOT APPROACH -> IDLE");
		}
		ai->countdown = 100;
		schedule_callback2(event_callback, &callback_schedule,
				"mining-bot-arrived-event", (double) bridgelist[b].shipid,
				(double) o->id);
	}
}

static void ai_mining_mode_land_on_asteroid(struct snis_entity *o, struct ai_mining_bot_data *ai)
{
	struct snis_entity *asteroid;
	float radius;
	union vec3 offset = { { 1.0f, 0.0f, 0.0f } };
	union quat new_orientation;
	const float slerp_rate = 0.05;
	float dx, dy, dz;
	int n;

	int i = lookup_by_id(ai->asteroid);
	if (i < 0) {
		/* asteroid got blown up maybe */
		int b = lookup_bridge_by_shipid(ai->parent_ship);
		if (b >= 0) {
			int channel = bridgelist[b].npcbot.channel;
			ai->orphan_time = 0;
			send_comms_packet(o, o->sdata.name, channel,
				"TARGET ASTEROID LOST -- RETURNING TO SHIP");
			ai_trace(o->id, "MINING BOT TARGET LOST");
		} else {
			ai->orphan_time++;
		}
		ai->mode = MINING_MODE_RETURN_TO_PARENT;
		ai_trace(o->id, "MINING BOT -> RETURN TO PARENT");
		return;
	}
	asteroid = &go[i];
	if (go[i].type == OBJTYPE_ASTEROID)
		radius = estimate_asteroid_radius(asteroid->id) *
				(1.0 - 0.2 * ai->countdown / 200.0);
	else
		radius = 180.0;
	vec3_mul_self(&offset, radius);
	n = o->tsd.ship.nai_entries - 1;
	quat_rot_vec_self(&offset, &o->tsd.ship.ai[n].u.mining_bot.orbital_orientation);
	quat_rot_vec_self(&offset, &asteroid->orientation);

	/* TODO: fix this up -- slerp orientation, slowly maneuver to landing spot, etc. */
	dx = asteroid->x + offset.v.x - o->x;
	dy = asteroid->y + offset.v.y - o->y;
	dz = asteroid->z + offset.v.z - o->z;

	o->x += 0.1 * dx;
	o->y += 0.1 * dy;
	o->z += 0.1 * dz;

	quat_slerp(&new_orientation, &o->orientation, &asteroid->orientation, slerp_rate);
	o->orientation = new_orientation;
	o->timestamp = universe_timestamp;
	ai->countdown--;
	if (ai->countdown == 0) {
		int b = lookup_bridge_by_shipid(ai->parent_ship);
		if (b >= 0) {
			int channel = bridgelist[b].npcbot.channel;
			ai->orphan_time = 0;
			if (asteroid->type == OBJTYPE_ASTEROID) {
				ai_trace(o->id, "COMMENCING MINING OPERATION");
				send_comms_packet(o, o->sdata.name, channel,
					"COMMENCING MINING OPERATION");
			} else if (asteroid->type == OBJTYPE_DERELICT) {
				ai_trace(o->id, "COMMENCING SALVAGE OPERATION");
				send_comms_packet(o, o->sdata.name, channel,
					"COMMENCING SALVAGE OPERATION");
			} else {
				ai_trace(o->id, "COMMENCING OPERATION");
				send_comms_packet(o, o->sdata.name, channel,
					"COMMENCING OPERATION");
			}
		} else {
			ai->orphan_time++;
		}
		ai_trace(o->id, "MINING BOT -> MINE MODE");
		ai->mode = MINING_MODE_MINE;
		ai->countdown = 400;
	}
}

static void update_mineral_amount(uint8_t *mineral, int amount)
{
	if (*mineral + amount > 255)
		*mineral = 255;
	else
		*mineral += amount;
}

static int add_mining_laserbeam(uint32_t origin, uint32_t target, int alive);

static void ai_mining_mode_mine_asteroid(struct snis_entity *o, struct ai_mining_bot_data *ai)
{
	struct snis_entity *asteroid;
	float radius;
	union vec3 sparks_offset, offset = { { 1.0f, 0.0f, 0.0f } };
	union quat new_orientation;
	const float slerp_rate = 0.05;
	float dx, dy, dz;
	int n, chance;

	int i = lookup_by_id(ai->asteroid);
	if (i < 0) {
		/* asteroid got blown up maybe */
		int b = lookup_bridge_by_shipid(ai->parent_ship);
		if (b >= 0) {
			int channel = bridgelist[b].npcbot.channel;
			ai->orphan_time = 0;
			send_comms_packet(o, o->sdata.name, channel,
				"TARGET LOST -- RETURNING TO SHIP");
			ai_trace(o->id, "MINING TARGET LOST, RETURN TO SHIP");
		} else {
			ai->orphan_time++;
		}
		ai->mode = MINING_MODE_RETURN_TO_PARENT;
		return;
	}
	asteroid = &go[i];
	radius = estimate_asteroid_radius(asteroid->id) * 0.8;
	vec3_mul_self(&offset, radius);
	n = o->tsd.ship.nai_entries - 1;
	quat_rot_vec_self(&offset, &o->tsd.ship.ai[n].u.mining_bot.orbital_orientation);
	quat_rot_vec_self(&offset, &asteroid->orientation);

	/* TODO fix this up to keep ship landed on asteroid */
	dx = asteroid->x + offset.v.x - o->x;
	dy = asteroid->y + offset.v.y - o->y;
	dz = asteroid->z + offset.v.z - o->z;

	o->x += 0.1 * dx;
	o->y += 0.1 * dy;
	o->z += 0.1 * dz;

	quat_slerp(&new_orientation, &o->orientation, &asteroid->orientation, slerp_rate);
	o->orientation = new_orientation;
	o->timestamp = universe_timestamp;

	/* TODO something better here that depends on composition of asteroid */
	ai->countdown--;
	switch (asteroid->type) {
	case OBJTYPE_ASTEROID:
		chance = snis_randn(1000);
		if (chance < 20) {
			int total = (int) ((float) snis_randn(100) *
				(float) asteroid->tsd.asteroid.preciousmetals / 255.0);
			int n;

			n = snis_randn(total); total -= n;
			update_mineral_amount(&ai->germanium, n);
			n = snis_randn(total); total -= n;
			update_mineral_amount(&ai->gold, n);
			n = snis_randn(total); total -= n;
			update_mineral_amount(&ai->platinum, n);
			n = snis_randn(total); total -= n;
			update_mineral_amount(&ai->uranium, n);
			n = snis_randn(total); total -= n;
			(void) total; /* To suppress scan-build complaining about dead stores */
		}
		if (chance < 80) {
			add_mining_laserbeam(o->id, asteroid->id, MINING_LASER_DURATION);
			vec3_mul(&sparks_offset, &offset, 0.7);
			(void) add_explosion(asteroid->x + sparks_offset.v.x,
						asteroid->y + sparks_offset.v.y,
						asteroid->z + sparks_offset.v.z,
						 20, 20, 15, OBJTYPE_ASTEROID);
		}
		break;
	case OBJTYPE_DERELICT:
		chance = snis_randn(1000);
		if (chance < 20) {
			n = snis_randn(10);
			if (n > asteroid->tsd.derelict.fuel)
				n = asteroid->tsd.derelict.fuel;
			asteroid->tsd.derelict.fuel -= n;
			if (ai->fuel + n < 255)
				ai->fuel += n;
			else
				ai->fuel = 255;
		} else if (chance < 40) {
			n = snis_randn(10);
			if (n > asteroid->tsd.derelict.oxygen)
				n = asteroid->tsd.derelict.oxygen;
			asteroid->tsd.derelict.oxygen -= n;
			if (ai->oxygen + n < 255)
				ai->oxygen += n;
			else
				ai->oxygen = 255;
		}
		break;
	default:
		break;
	}

	if (ai->countdown != 0)
		return;
	int b = lookup_bridge_by_shipid(ai->parent_ship);
	if (b < 0) {
		ai->orphan_time++;
	} else {
		ai->orphan_time = 0;
		int channel = bridgelist[b].npcbot.channel;
		switch (asteroid->type) {
		case OBJTYPE_ASTEROID:
			ai_trace(o->id, "MINING COMPLETE, RETURNING TO SHIP");
			send_comms_packet(o, o->sdata.name, channel,
				"MINING OPERATION COMPLETE -- RETURNING TO SHIP");
			schedule_callback2(event_callback, &callback_schedule,
						"asteroid-mined-event", (double) asteroid->id,
						(double) bridgelist[b].shipid);
			break;
		case OBJTYPE_DERELICT:
			ai_trace(o->id, "SALVAGE COMPLETE, RETURNING TO SHIP");
			send_comms_packet(o, o->sdata.name, channel,
				"SALVAGE OPERATION COMPLETE -- RETURNING TO SHIP");
			schedule_callback2(event_callback, &callback_schedule,
						"derelict-salvaged-event", (double) asteroid->id,
						(double) bridgelist[b].shipid);
			if (asteroid->tsd.derelict.ship_id_chip_present) {
				send_comms_packet(o, o->sdata.name, channel,
					"*** SHIPS IDENTIFICATION CHIP RECOVERED ***");
				asteroid->tsd.derelict.ship_id_chip_present = 0;
				/* Hack alert... we're overloading otherwise unused miner tractor_beam
				 * to contain ID of derelict original ship here. */
				o->tsd.ship.tractor_beam = asteroid->tsd.derelict.orig_ship_id;
			} else {
				send_comms_packet(o, o->sdata.name, channel,
					"*** SHIPS IDENTIFICATION CHIP NOT FOUND ***");
			}
			break;
		default:
			ai_trace(o->id, "OPERATION COMPLETE, RETURNING TO SHIP");
			send_comms_packet(o, o->sdata.name, channel,
				"OPERATION COMPLETE -- RETURNING TO SHIP");
			break;
		}
	}
	ai->mode = MINING_MODE_RETURN_TO_PARENT;
}

static void disconnect_from_tow_ship(struct snis_entity *towing_ship,
		__attribute__((unused)) struct snis_entity *towed_ship)
{
	pop_ai_stack(towing_ship);
	snis_queue_add_text_to_speech("Disconnected from mantis tow ship.",
					ROLE_TEXT_TO_SPEECH, towed_ship->id);
	towed_ship->tsd.ship.wallet -= TOW_SHIP_CHARGE; /* Charge player */
}

static void ai_tow_ship_mode_brain(struct snis_entity *o)
{
	int i, n;
	struct snis_entity *disabled_ship, *starbase_dispatcher;

	n = o->tsd.ship.nai_entries - 1;
	if (n < 0)
		return;

	/* Find the ship we're supposed to be towing */
	i = lookup_by_id(o->tsd.ship.ai[n].u.tow_ship.disabled_ship);
	if (i < 0) {
		ai_trace(o->id, "CANNOT FIND SHIP TO TOW %u, POPPING TOW SHIP",
				o->tsd.ship.ai[n].u.tow_ship.disabled_ship);
		pop_ai_stack(o);
		return;
	}
	disabled_ship = &go[i];

	/* Find the starbase we are supposed to tow it to */
	i = lookup_by_id(o->tsd.ship.ai[n].u.tow_ship.starbase_dispatcher);
	if (i < 0) {
		ai_trace(o->id, "CANNOT FIND STARBASE TO TOW TO %u, POPPING TOW SHIP",
				o->tsd.ship.ai[n].u.tow_ship.starbase_dispatcher);
		pop_ai_stack(o);
		return;
	}
	starbase_dispatcher = &go[i];

	if (o->tsd.ship.ai[n].u.tow_ship.ship_connected) {
		double dist2 = ai_ship_travel_towards(o,
			starbase_dispatcher->x, starbase_dispatcher->y, starbase_dispatcher->z);
		if (dist2 < (TOWING_DROP_DIST * TOWING_DROP_DIST)) {
			/* Give player a little fuel if they're super low. */
			if (disabled_ship->tsd.ship.fuel < FUEL_CONSUMPTION_UNIT * 30 * 60)
				disabled_ship->tsd.ship.fuel = FUEL_CONSUMPTION_UNIT * 30 * 60;
			/* We're done */
			ai_trace(o->id, "DROPPING OFF DISABLED SHIP %u", disabled_ship->id);
			disconnect_from_tow_ship(o, disabled_ship);
			return;
		}
	} else {
		double dist2 = ai_ship_travel_towards(o,
			disabled_ship->x, disabled_ship->y, disabled_ship->z);
		if (dist2 < (TOWING_PICKUP_DIST * TOWING_PICKUP_DIST)) {
			o->tsd.ship.ai[n].u.tow_ship.ship_connected = 1;
			snis_queue_add_sound(HULL_CREAK_0 + (snis_randn(1000) % NHULL_CREAK_SOUNDS),
							ROLE_SOUNDSERVER, disabled_ship->id);
			snis_queue_add_text_to_speech("Mantis tow ship has attached.",
							ROLE_TEXT_TO_SPEECH, disabled_ship->id);
			(void) ai_ship_travel_towards(o,
				starbase_dispatcher->x, starbase_dispatcher->y, starbase_dispatcher->z);
			ai_trace(o->id, "ATTACHED TO DISABLED SHIP %u", disabled_ship->id);
			return;
		}
	}
}

static void mining_bot_move_towed_cargo(struct snis_entity *o)
{
	int i, n = o->tsd.ship.nai_entries - 1;
	union vec3 cargo_position = { { 40, 0, 0 } };
	struct snis_entity *cargo;

	struct ai_mining_bot_data *ai = &o->tsd.ship.ai[n].u.mining_bot;
	if (!ai->towing)
		return;
	if (ai->towed_object == (uint32_t) -1)
		return;
	i = lookup_by_id(ai->towed_object);
	if (i < 0) {
		ai->towing = 0;
		ai->towed_object = (uint32_t) -1;
		return;
	}
	quat_rot_vec_self(&cargo_position, &o->orientation);
	cargo = &go[i];
	set_object_location(cargo, o->x + cargo_position.v.x, o->y + cargo_position.v.y, o->z + cargo_position.v.z);
	cargo->vx = o->vx;
	cargo->vy = o->vy;
	cargo->vz = o->vz;
	switch (cargo->type) {
	case OBJTYPE_CARGO_CONTAINER:
		quat_init_axis(&cargo->tsd.cargo_container.rotational_velocity, 1, 0, 0, 0);
		break;
	default:
		break;
	}
}

static void ai_mining_mode_brain(struct snis_entity *o)
{
	int n = o->tsd.ship.nai_entries - 1;
	struct ai_mining_bot_data *mining_bot = &o->tsd.ship.ai[n].u.mining_bot;

	if (mining_bot->orphan_time > MINING_BOT_MAX_ORPHAN_TIME) {
		ai_trace(o->id, "MINING BOT ORPHANED TOO LONG");
		printf("Mining bot orphaned for too long.\n");
		delete_from_clients_and_server(o);
		return;
	}
	mining_bot_move_towed_cargo(o);
	switch (mining_bot->mode) {
	case MINING_MODE_APPROACH_ASTEROID:
		ai_mining_mode_approach_asteroid(o, mining_bot);
		break;
	case MINING_MODE_LAND_ON_ASTEROID:
		ai_mining_mode_land_on_asteroid(o, mining_bot);
		break;
	case MINING_MODE_MINE:
		ai_mining_mode_mine_asteroid(o, mining_bot);
		break;
	case MINING_MODE_RETURN_TO_PARENT:
	case MINING_MODE_STOW_BOT:
	case MINING_MODE_STANDBY_TO_TRANSPORT_ORE:
		ai_mining_mode_return_to_parent(o, mining_bot);
		break;
	case MINING_MODE_IDLE:
		ai_trace(o->id, "MINING BOT IDLE");
		o->vx *= 0.95; /* Come to a stop */
		o->vy *= 0.95;
		o->vz *= 0.95;
		break;
	default:
		ai_trace(o->id, "MINING BOT MODE UNKNOWN %hhu", mining_bot->mode);
		fprintf(stderr, "unexpected default value of mining ai mode; %hhu\n",
			mining_bot->mode);
		break;
	}
}

static void ai_patrol_mode_brain(struct snis_entity *o)
{
	int n = o->tsd.ship.nai_entries - 1;
	struct ai_patrol_data *patrol = &o->tsd.ship.ai[n].u.patrol;
	int d;

	d = patrol->dest;
	double dist2 = ai_ship_travel_towards(o,
			patrol->p[d].v.x, patrol->p[d].v.y, patrol->p[d].v.z);
	ai_trace(o->id, "PATROL DIST TO DEST = %.2f, WP = %d, ONESHOT = %d",
				sqrt(dist2), patrol->dest, patrol->oneshot);

	if (has_arrived_at_destination(o, dist2)) {
		/* Advance to the next patrol destination */
		ai_trace(o->id, "PATROL ARRIVED AT DEST");
		if (patrol->dest + 1 == patrol->npoints && patrol->oneshot) {
			pop_ai_stack(o);
			ai_trace(o->id, "POPPED ONESHOT PATROL ROUTE");
			return;
		}
		set_patrol_waypoint_destination(o, (patrol->dest + 1) % patrol->npoints);
		/* But first hang out here awhile... */
		n = o->tsd.ship.nai_entries;
		if (n < MAX_AI_STACK_ENTRIES) {
			ai_trace(o->id, "PATROL -> HANGOUT");
			o->tsd.ship.ai[n].ai_mode = AI_MODE_HANGOUT;
			o->tsd.ship.ai[n].u.hangout.time_to_go = 100 + snis_randn(100);
			if (o->tsd.ship.ai[n].u.hangout.time_to_go > patrol->max_hangout_time)
				o->tsd.ship.ai[n].u.hangout.time_to_go = patrol->max_hangout_time;
			o->tsd.ship.desired_velocity = 0;
			o->tsd.ship.nai_entries++;
		}
	}
	check_for_nearby_targets(o);
}

static void ai_cop_mode_brain(struct snis_entity *o)
{
	int n = o->tsd.ship.nai_entries - 1;
	struct ai_cop_data *patrol = &o->tsd.ship.ai[n].u.cop;
	int d;

	d = patrol->dest;
	ai_trace(o->id, "TO PATROL POINT %d\n", d);

	double dist2 = ai_ship_travel_towards(o,
				patrol->p[d].v.x, patrol->p[d].v.y, patrol->p[d].v.z);
	ai_trace(o->id, "COP MODE DIST TO DEST = %.2f WP = %d ONESHOT = %d",
				sqrt(dist2), patrol->dest, patrol->oneshot);

	if (has_arrived_at_destination(o, dist2)) {
		/* Advance to the next patrol destination */
		ai_trace(o->id, "COP MODE ARRIVED AT DEST");
		if (patrol->dest + 1 == patrol->npoints && patrol->oneshot) {
			pop_ai_stack(o);
			ai_trace(o->id, "COP POPPED ONESHOT PATROL ROUTE");
			return;
		}
		set_patrol_waypoint_destination(o, (patrol->dest + 1) % patrol->npoints);
		/* But first hang out here awhile... */
		n = o->tsd.ship.nai_entries;
		if (n < MAX_AI_STACK_ENTRIES) {
			ai_trace(o->id, "COP -> HANGOUT");
			o->tsd.ship.ai[n].ai_mode = AI_MODE_HANGOUT;
			o->tsd.ship.ai[n].u.hangout.time_to_go = 100 + snis_randn(100);
			if (o->tsd.ship.ai[n].u.hangout.time_to_go > patrol->max_hangout_time)
				o->tsd.ship.ai[n].u.hangout.time_to_go = patrol->max_hangout_time;
			o->tsd.ship.desired_velocity = 0;
			o->tsd.ship.nai_entries++;
		}
	}
}

static void maybe_leave_fleet(struct snis_entity *o)
{
	int i;

	if (snis_randn(FLEET_LEAVE_CHANCE) != 1)
		return;

	for (i = 0; i < o->tsd.ship.nai_entries; i++) {
		if (o->tsd.ship.ai[i].ai_mode == AI_MODE_FLEET_MEMBER) {
			ai_trace(o->id, "LEAVING FLEET");
			fleet_leave(o->id);
			o->tsd.ship.nai_entries = 0;
			memset(o->tsd.ship.ai, 0, sizeof(o->tsd.ship.ai));
			ship_figure_out_what_to_do(o);
			return;
		}
	}
}

static void announce_starbase_status_change(char *basename, int winner, int loser, int zone)
{
	int i;
	char buffer[100];

	if (zone <= 0) /* should not happen */
		return;
	for (i = 0; i <= snis_object_pool_highest_object(pool); i++) {
		struct snis_entity *o = &go[i];
		if (o->alive && o->type == OBJTYPE_SHIP1) {
			if (o->sdata.faction == loser) {
				if (zone == 4) /* Starbase changed hands */
					snprintf(buffer, sizeof(buffer), "%s has been captured by the enemy.",
						basename);
				else
					snprintf(buffer, sizeof(buffer), "%s zone %d has fallen to the enemy.",
						basename, zone);
				snis_queue_add_text_to_speech(buffer, ROLE_TEXT_TO_SPEECH, o->id);
			} else if (o->sdata.faction == winner) {
				if (zone == 4) /* Starbase changed hands */
					snprintf(buffer, sizeof(buffer), "We have captured %s.",
						basename);
				else
					snprintf(buffer, sizeof(buffer), "%s zone %d has been secured.",
						basename, zone);
				snis_queue_add_text_to_speech(buffer, ROLE_TEXT_TO_SPEECH, o->id);
			}
		}
	}
}

static void rts_occupy_starbase_slot(struct snis_entity *starbase, struct snis_entity *occupier)
{
	int i;
	int loser = 255, zone = -1;
	/* See if there's an unoccupied slot in the first 3 */
	for (i = 0; i < 3; i++) {
		if (starbase->tsd.starbase.occupant[i] == 255) {
			loser = starbase->tsd.starbase.occupant[i];
			starbase->tsd.starbase.occupant[i] = occupier->sdata.faction;
			zone = i + 1;
			goto out;
		}
	}
	/* See if there's a slot in the first 3 that's not us */
	for (i = 0; i < 3; i++) {
		if (starbase->tsd.starbase.occupant[i] != occupier->sdata.faction) {
			loser = starbase->tsd.starbase.occupant[i];
			starbase->tsd.starbase.occupant[i] = occupier->sdata.faction;
			zone = i + 1;
			goto out;
		}
	}
	/* See if the last slot is not us */
	if (starbase->tsd.starbase.occupant[3] != occupier->sdata.faction) {
		loser = starbase->tsd.starbase.occupant[3];
		starbase->tsd.starbase.occupant[3] = occupier->sdata.faction;
		starbase->sdata.faction = occupier->sdata.faction; /* take over the base */
		zone = 4;
		goto out;
	}
out:
	delete_from_clients_and_server(occupier);
	starbase->timestamp = universe_timestamp;
	announce_starbase_status_change(starbase->sdata.name, occupier->sdata.faction, loser, zone);
	return;

}

static void ai_atk_near_enemy_mode_brain(struct snis_entity *o)
{
	int i, n;
	int closest = -1;
	double dist, min_dist = 0;
	struct snis_entity *e = NULL;

	/* Find the closest enemy ship to engage */
	n = o->tsd.ship.nai_entries - 1;
	if (o->tsd.ship.ai[n].u.atk_near_enemy.enemy_id == (uint32_t) -1 ||
		((o->id + universe_timestamp) % 17) == 0) {
		for (i = 0; i <= snis_object_pool_highest_object(pool); i++) {
			e = &go[i];
			if (!e->alive)
				continue;
			if (e->type != OBJTYPE_SHIP2)
				continue;
			if (e->sdata.faction == o->sdata.faction)
				continue;
			dist = dist3d(e->x - o->x, e->y - o->y, e->z - o->z);
			if (closest == -1 || dist < min_dist) {
				closest = i;
				min_dist = dist;
			}
		}
		if (closest == -1) { /* There are no enemies */
			return;
		} else {
			o->tsd.ship.ai[n].u.atk_near_enemy.enemy_id = go[closest].id;
		}
	}
	i = lookup_by_id(o->tsd.ship.ai[n].u.atk_near_enemy.enemy_id);
	if (i < 0)
		return;
	e = &go[i];
	if (!e->alive || e->type != OBJTYPE_SHIP2 || e->sdata.faction == o->sdata.faction) {
		o->tsd.ship.ai[n].u.atk_near_enemy.enemy_id = (uint32_t) -1;
		return;
	}
	push_attack_mode(o, e->id, 0);
}

static void ai_occupy_enemy_base_mode_brain(struct snis_entity *o)
{
	int i, n;
	int closest = -1;
	double dist, min_dist = 0;

	/* Find the closest starbase that we do not already control */
	n = o->tsd.ship.nai_entries - 1;
	if (o->tsd.ship.ai[n].u.occupy_base.base_id == (uint32_t) -1 ||
		((o->id + universe_timestamp)  % 10) == 0) {
		for (i = 0; i <= snis_object_pool_highest_object(pool); i++) {
			struct snis_entity *sb = &go[i];
			if (!sb->alive)
				continue;
			if (sb->type != OBJTYPE_STARBASE)
				continue;
			if (sb->tsd.starbase.occupant[3] == o->sdata.faction)
				continue;
			dist = object_dist(sb, o);
			if (closest == -1 || dist < min_dist) {
				closest = i;
				min_dist = dist;
			}
		}
		if (closest == -1) { /* There are no starbases we do not already control */
			n = o->tsd.ship.nai_entries - 1;
			o->tsd.ship.ai[n].ai_mode = AI_MODE_RTS_STANDBY; /* switch to standby mode */
		} else {
			o->tsd.ship.ai[n].u.occupy_base.base_id = go[closest].id;
		}
	}
	if (o->tsd.ship.ai[n].u.occupy_base.base_id != (uint32_t) -1) {
		i = lookup_by_id(o->tsd.ship.ai[n].u.occupy_base.base_id);
		if (i >= 0) {
			struct snis_entity *sb = &go[i];
			if (sb->type == OBJTYPE_STARBASE && sb->tsd.starbase.occupant[3] != o->sdata.faction) {
				dist = ai_ship_travel_towards(o, sb->x, sb->y, sb->z);
				if (has_arrived_at_destination(o, dist)) {
					rts_occupy_starbase_slot(sb, o);
				}
			}
		}
	}
}

static void ai_atk_main_base_mode_brain(struct snis_entity *o)
{
	int i, j, n, firing_range;
	struct snis_entity *main_base;
	union vec3 to_main_base;
	float dist;
	double dist2;

	/* Find the main base if we haven't already */
	n = o->tsd.ship.nai_entries - 1; /* We expect n to be 0 here. */
	if (o->tsd.ship.ai[n].u.atk_main_base.base_id == (uint32_t) -1) {
		for (i = 0; i <= snis_object_pool_highest_object(pool); i++) {
			main_base = &go[i];
			if (main_base->alive && main_base->type == OBJTYPE_PLANET &&
				main_base->sdata.faction != o->sdata.faction) {
				for (j = 0; j < ARRAYSIZE(rts_planet); j++) {
					if (i == rts_planet[j].index) {
						o->tsd.ship.ai[n].u.atk_main_base.base_id = main_base->id;
						break;
					}
				}
				if (o->tsd.ship.ai[n].u.atk_main_base.base_id != (uint32_t) -1)
					break;
			}
		}
		/* If we didn't find the base (should not happen), go to standby mode. */
		if (o->tsd.ship.ai[n].u.atk_main_base.base_id == (uint32_t) -1) {
			o->tsd.ship.ai[n].ai_mode = AI_MODE_RTS_STANDBY;
			return;
		}
	}
	i = lookup_by_id(o->tsd.ship.ai[n].u.atk_main_base.base_id);
	if (i < 0) {
		o->tsd.ship.ai[n].ai_mode = AI_MODE_RTS_STANDBY;
		return;
	}
	main_base = &go[i];
	/* Figure out how to move */
	to_main_base.v.x = main_base->x - o->x;
	to_main_base.v.y = main_base->y - o->y;
	to_main_base.v.z = main_base->z - o->z;
	dist = vec3_magnitude(&to_main_base);
	firing_range = (dist <= LASER_RANGE + main_base->tsd.planet.radius);
	dist = dist - main_base->tsd.planet.radius - LASER_RANGE * 0.9;
	vec3_normalize_self(&to_main_base);
	vec3_mul_self(&to_main_base, dist);
	o->tsd.ship.desired_velocity = ship_type[o->tsd.ship.shiptype].max_speed;
	dist2 = ai_ship_travel_towards(o, o->x + to_main_base.v.x, o->y + to_main_base.v.y, o->z + to_main_base.v.z);
	if (firing_range)
		ai_maybe_fire_weapon(o, main_base, 0, sqrt(dist2), main_base->tsd.planet.radius);
}

static void ai_move_to_waypoint_mode_brain(struct snis_entity *o)
{
	int b, w;
	union vec3 to_waypoint;
	float dist;

	b = lookup_bridge_by_shipid(o->tsd.ship.ai[0].u.goto_waypoint.bridge_ship_id);
	if (b < 0)
		goto bail;
	w = o->tsd.ship.ai[0].u.goto_waypoint.waypoint;
	if (w < 0 || w >= bridgelist[b].nwaypoints)
		goto bail;
	to_waypoint.v.x = bridgelist[b].waypoint[w].x - o->x;
	to_waypoint.v.y = bridgelist[b].waypoint[w].y - o->y;
	to_waypoint.v.z = bridgelist[b].waypoint[w].z - o->z;
	dist = vec3_magnitude(&to_waypoint);
	if (dist < WAYPOINT_CLOSE_ENOUGH)
		goto bail;
	o->tsd.ship.desired_velocity = ship_type[o->tsd.ship.shiptype].max_speed;
	(void) ai_ship_travel_towards(o, o->x + to_waypoint.v.x, o->y + to_waypoint.v.y, o->z + to_waypoint.v.z);
	return;
bail:
	o->tsd.ship.nai_entries = 1;
	o->tsd.ship.ai[0].ai_mode = AI_MODE_RTS_STANDBY;
	return;
}

static void ai_rts_guard_base(struct snis_entity *o)
{
	int i, k, n, j;
	int found = -1;
	double dist2, closest = -1;

	n = o->tsd.ship.nai_entries - 1;
	if (n < 0 || o->tsd.ship.ai[n].ai_mode != AI_MODE_RTS_GUARD_BASE) { /* shouldn't happen */
		o->tsd.ship.nai_entries = 1;
		o->tsd.ship.ai[0].ai_mode = AI_MODE_RTS_STANDBY;
		return;
	}
	if (o->tsd.ship.ai[n].u.guard_base.base_id == -1) {
		for (i = 0; i <= snis_object_pool_highest_object(pool); i++) {
			struct snis_entity *b = &go[i];
			if (!b->alive)
				continue;
			if (b->type == OBJTYPE_STARBASE) {
				if (b->sdata.faction != o->sdata.faction)
					continue;
				dist2 = object_dist2(o, b);
				if (closest < 0 || dist2 < closest) {
					closest = dist2;
					found = i;
				}
			} else if (b->type == OBJTYPE_PLANET) {
				int found_planet = 0;

				if (b->sdata.faction != o->sdata.faction)
					continue;
				for (j = 0; j < ARRAYSIZE(rts_planet); j++) {
					if (go[rts_planet[j].index].id == b->id) {
						found_planet = 1;
						break;
					}
				}
				if (!found_planet)
					continue;
				dist2 = object_dist2(o, b);
				if (closest < 0 || dist2 < closest) {
					closest = dist2;
					found = i;
				}
			}
		}
		if (found < 0) { /* This shouldn't happen. */
			fprintf(stderr, "snis_server: Didn't find home planet at %s:%d\n", __FILE__, __LINE__);
			return;
		} else {
			o->tsd.ship.ai[n].u.guard_base.base_id = go[found].id;
		}
	}
	/* Find the closest enemy headed for this base, and if he is close enough, attack him */
	/* FIXME: Can we use the space partitioning here? */
	closest = -1;
	found = -1;
	for (i = 0; i <= snis_object_pool_highest_object(pool); i++) {
		struct snis_entity *s = &go[i];
		if (!s->alive)
			continue;
		if (s->type != OBJTYPE_SHIP2) /* What about human controlled ships here? */
			continue;
		if (s->sdata.faction == o->sdata.faction) /* Don't guard against friendlies */
			continue;
		k = s->tsd.ship.nai_entries - 1;
		if (k < 0)
			continue; /* Shouldn't happen, barring locking bugs */
		if (s->tsd.ship.ai[0].ai_mode == AI_MODE_RTS_OCCUPY_NEAR_BASE &&
			s->tsd.ship.ai[0].u.occupy_base.base_id == o->tsd.ship.ai[n].u.guard_base.base_id) {
			/* We found a ship that is attempting to occupy the base that we are guarding */
			dist2 = object_dist2(o, s);
			if (closest < 0 || dist2 < closest) {
				closest = dist2;
				found = i;
			}
		} else if (s->tsd.ship.ai[0].ai_mode == AI_MODE_RTS_ATK_MAIN_BASE &&
			s->tsd.ship.ai[0].u.atk_main_base.base_id == o->tsd.ship.ai[n].u.guard_base.base_id) {
			/* We found a ship that is attacking the base that we are guarding */
			dist2 = object_dist2(o, s);
			if (closest < 0 || dist2 < closest) {
				closest = dist2;
				found = i;
			}
		} else { /* TODO: do we care about other enemy brain modes here? */
			continue;
		}
	}
	if (found < 0) /* Didn't find any enemies attacking the base we are guarding base */
		return; /* TODO: put some code to make us patrol around the base or something here. */
	if (closest > (XKNOWN_DIM / 20.0) * (XKNOWN_DIM / 20.0))
		return; /* Too far away to worry about yet. */
		/* FIXME: the above check is probably not suitable if guarding the home planet */

	/* We found an enemy that is attacking the base we are guarding, and he is close enough
	 * that we want to do something about it. So, go attack him.
	 */
	push_attack_mode(o, go[found].id, 0);
}

static void ai_rts_resupply(struct snis_entity *o)
{
	int i, n, sn;
	struct snis_entity *u;
	double dist;

	n = o->tsd.ship.nai_entries - 1;
	if (n < 0 || o->tsd.ship.ai[n].ai_mode != AI_MODE_RTS_RESUPPLY) {
		o->tsd.ship.nai_entries = 1;
		o->tsd.ship.ai[0].ai_mode = AI_MODE_RTS_STANDBY;
		return;
	}
	/* Every 1.7 seconds or so */
	if (((o->id + universe_timestamp) % 17) == 0) {
		/* Find the closest guy needing resupply. This simple minded algorithm
		 * can result in some bad behaviors. TODO: something better.
		 */
		double closest_dist = -1.0;
		int closest = -1;
		for (i = 0; i <= snis_object_pool_highest_object(pool); i++) {
			struct snis_entity *s = &go[i];
			if (!s->alive)
				continue;
			if (s->type != OBJTYPE_SHIP2)
				continue;
			if (o->sdata.faction != s->sdata.faction)
				continue;
			if (o->id == s->id) /* Cannot self-refuel */
				continue;
			sn = s->tsd.ship.nai_entries - 1;
			if (sn < 0)
				continue;
			if (s->tsd.ship.ai[sn].ai_mode == AI_MODE_RTS_OUT_OF_FUEL) {
				double dist = object_dist2(o, s);
				if (closest == -1 || dist < closest_dist) {
					closest_dist = dist;
					closest = i;
				}
			}
		}
		if (closest >= 0)
			o->tsd.ship.ai[n].u.resupply.unit_to_resupply = go[closest].id;
	}
	if (o->tsd.ship.ai[n].u.resupply.unit_to_resupply == (uint32_t) -1)
		return;
	i = lookup_by_id(o->tsd.ship.ai[n].u.resupply.unit_to_resupply);
	if (i < 0) {
		o->tsd.ship.ai[n].u.resupply.unit_to_resupply = (uint32_t) -1;
		return;
	}
	u = &go[i];
	dist = ai_ship_travel_towards(o, u->x, u->y, u->z);
	if (has_arrived_at_destination(o, dist)) { /* Refuel the ship */
		int unit_type = ship_type[o->tsd.ship.shiptype].rts_unit_type;
		u->tsd.ship.fuel = rts_unit_type(unit_type)->fuel_capacity;
		u->tsd.ship.nai_entries--; /* restore the previous ai mode */
		o->tsd.ship.ai[n].u.resupply.unit_to_resupply = (uint32_t) -1;
	}
}

static void ai_brain(struct snis_entity *o)
{
	int n;

	ai_trace(o->id, "TOP OF AI BRAIN FOR %u", o->id);

	n = o->tsd.ship.nai_entries - 1;
	if (n < 0) {
		ship_figure_out_what_to_do(o);
		n = o->tsd.ship.nai_entries - 1;
	}

	if (rts_mode) {
		/* Check fuel */
		if (o->tsd.ship.fuel == 0 && o->tsd.ship.ai[n].ai_mode != AI_MODE_RTS_OUT_OF_FUEL) {
			if (n < MAX_AI_STACK_ENTRIES) {
				n++;
				o->tsd.ship.nai_entries++;
			}
			o->tsd.ship.ai[n].ai_mode = AI_MODE_RTS_OUT_OF_FUEL;
			ai_trace(o->id, "OUT OF FUEL (RTS)");
		}
	}

	/* main AI brain code is here... */
	switch (o->tsd.ship.ai[n].ai_mode) {
	case AI_MODE_ATTACK:
		ai_attack_mode_brain(o);
		break;
	case AI_MODE_AVOID_MISSILE:
		ai_avoid_missile_brain(o);
		break;
	case AI_MODE_FLEE:
		ai_flee_mode_brain(o);
		break;
	case AI_MODE_PATROL:
		ai_patrol_mode_brain(o);
		break;
	case AI_MODE_COP:
		ai_cop_mode_brain(o);
		break;
	case AI_MODE_FLEET_LEADER:
		/* Because fleet leader uses patrol brain, these assumptions need to hold */
		BUILD_ASSERT(offsetof(union ai_data, patrol) ==
				offsetof(union ai_data, fleet.patrol));
		BUILD_ASSERT(sizeof(((union ai_data *) 0)->patrol) ==
				sizeof(((union ai_data *) 0)->fleet.patrol));
		ai_patrol_mode_brain(o);
		break;
	case AI_MODE_FLEET_MEMBER:
		ai_fleet_member_mode_brain(o);
		break;
	case AI_MODE_HANGOUT:
		ai_trace(o->id, "HANGOUT %d", 10 * o->tsd.ship.ai[n].u.hangout.time_to_go / 10);
		o->tsd.ship.ai[n].u.hangout.time_to_go--;
		if (o->tsd.ship.ai[n].u.hangout.time_to_go <= 0) {
			ai_trace(o->id, "POP HANGOUT");
			pop_ai_stack(o);
			return;
		}
		o->vx *= 0.8;
		o->vy *= 0.8;
		o->vz *= 0.8;
		o->tsd.ship.desired_velocity = 0.0;
		maybe_leave_fleet(o);
		break;
	case AI_MODE_MINING_BOT:
		ai_mining_mode_brain(o);
		break;
	case AI_MODE_TOW_SHIP:
		ai_tow_ship_mode_brain(o);
		break;
	case AI_MODE_RTS_OUT_OF_FUEL:
		ai_trace(o->id, "RTS MODE OUT OF FUEL");
		o->vx = 0;
		o->vy = 0;
		o->vz = 0;
		break;
	case AI_MODE_RTS_STANDBY: /* Just sit there not doing anything. */
		ai_trace(o->id, "RTS MODE STANDBY");
		o->vx = 0;
		o->vy = 0;
		o->vz = 0;
		break;
	case AI_MODE_RTS_ATK_NEAR_ENEMY:
		ai_atk_near_enemy_mode_brain(o);
		break;
	case AI_MODE_RTS_OCCUPY_NEAR_BASE:
		ai_occupy_enemy_base_mode_brain(o);
		break;
	case AI_MODE_RTS_ATK_MAIN_BASE:
		ai_atk_main_base_mode_brain(o);
		break;
	case AI_MODE_RTS_MOVE_TO_WAYPOINT:
		ai_move_to_waypoint_mode_brain(o);
		break;
	case AI_MODE_RTS_GUARD_BASE:
		ai_rts_guard_base(o);
		break;
	case AI_MODE_RTS_RESUPPLY:
		ai_rts_resupply(o);
	case AI_MODE_CATATONIC:
		break;
	default:
		break;
	}
}

static void ship_security_avoidance(void *context, void *entity)
{
	struct snis_entity *ship = context;
	struct snis_entity *planet = entity;
	float dist2;

	if (planet->type != OBJTYPE_PLANET)
		return;

	if (planet->tsd.planet.security == LOW_SECURITY)
		return;

	dist2 = object_dist2(ship, planet);
	if (dist2 < SECURITY_RADIUS * SECURITY_RADIUS)
		return;

	ship->tsd.ship.in_secure_area = 1;
}

struct collision_avoidance_context {
	struct snis_entity *o;
	double closest_dist2;
	double worrythreshold;
};

/* adjust o->tsd.ship.desired_velocity vector to avoid collisions */
static void ship_collision_avoidance(void *context, void *entity)
{
	struct collision_avoidance_context *ca = context;
	struct snis_entity *o = ca->o;
	struct snis_entity *obstacle = entity;
	double d;
	float steering_magnitude;

	if (o == obstacle) /* don't avoid oneself */
		return;

	if (obstacle->type == OBJTYPE_SPARK) /* no point trying to avoid */
		return;

	/* Tow ship should not try to avoid thing it's towing */
	if ((obstacle->type == OBJTYPE_SHIP2 || obstacle->type == OBJTYPE_SHIP1) &&
		ship_is_towing(o) == obstacle->id)
		return;

	/* hmm, server has no idea about meshes... */
	d = dist3dsqrd(o->x - obstacle->x, o->y - obstacle->y, o->z - obstacle->z);

	if (obstacle->type == OBJTYPE_PLANET)
		return; /* Do not use steering forces to avoid planets.  That is done elsewhere */

	if (obstacle->type == OBJTYPE_SPACEMONSTER) {
		if (d < spacemonster_collision_radius * spacemonster_collision_radius) {
			calculate_torpedolike_damage(o, spacemonster_damage_factor);
			do_collision_impulse(o, obstacle);
			attack_your_attacker(o, obstacle);
		}
	}
	/* Pretend torpedoes are closer than they are since they're scary */
	if (obstacle->type == OBJTYPE_TORPEDO)
		d = d / 6.0;

	/* We only care about the closest one. */
	if (d > ca->closest_dist2 && ca->closest_dist2 > 0)
		return;
	ca->closest_dist2 = d;
	if (d > ca->worrythreshold) { /* not close enough to worry about */
		o->tsd.ship.steering_adjustment.v.x = 0.0f;
		o->tsd.ship.steering_adjustment.v.y = 0.0f;
		o->tsd.ship.steering_adjustment.v.z = 0.0f;
		o->tsd.ship.braking_factor = 1.0;
		return;
	}

	d = sqrt(d);
	if (d < 1.0)
		d = 1.0;
	/* Compute a braking force */
	if (d < 2.0)
		o->tsd.ship.braking_factor = 1.0 - 1.0 / 2;
	else
		o->tsd.ship.braking_factor = 1.0 - 1.0 / d; 
	ai_trace(o->id, "COMPUTING BRAKING FORCE %f\n", o->tsd.ship.braking_factor);

	/* Compute a steering force */
	o->tsd.ship.steering_adjustment.v.x = o->x - obstacle->x;
	o->tsd.ship.steering_adjustment.v.y = o->y - obstacle->y;
	o->tsd.ship.steering_adjustment.v.z = o->z - obstacle->z;
	if (vec3_magnitude(&o->tsd.ship.steering_adjustment) <= 0.0001)
		return; /* Otherwise, the normalization will get you NaNs. */
	vec3_normalize_self(&o->tsd.ship.steering_adjustment);
	steering_magnitude = 800.0 / d;
	if (steering_magnitude > 10.0)
		steering_magnitude = 10.0;
	vec3_mul_self(&o->tsd.ship.steering_adjustment, steering_magnitude);
	ai_trace(o->id, "STEERING FORCE %f, %f, %f\n",
			o->tsd.ship.steering_adjustment.v.x,
			o->tsd.ship.steering_adjustment.v.y,
			o->tsd.ship.steering_adjustment.v.z);
}

static void ship_move(struct snis_entity *o)
{
	int n;
	struct collision_avoidance_context ca;

	netstats.nships++;
	if (o->tsd.ship.nai_entries == 0)
		ship_figure_out_what_to_do(o);

	n = o->tsd.ship.nai_entries - 1;

	switch (o->tsd.ship.cmd_data.command) {
	case DEMON_CMD_ATTACK:
		o->tsd.ship.ai[n].ai_mode = AI_MODE_ATTACK;
		if (o->tsd.ship.ai[n].u.attack.victim_id == (uint32_t) -1 || snis_randn(1000) < 50)
			ship_choose_new_attack_victim(o);
		break;
	default:
		break;
	}

	/* Repair the ship over time */
	const int st = o->tsd.ship.shiptype;
	if (o->sdata.shield_strength < ship_type[st].max_shield_strength && snis_randn(1000) < 7)
		o->sdata.shield_strength++;

	/* Change shield wavelength every 10 seconds */
	if ((universe_timestamp % 100) == 0)
		o->sdata.shield_wavelength = snis_randn(256);

	/* Check if we are in a secure area */
	o->tsd.ship.in_secure_area = 0;
	space_partition_process(space_partition, o, o->x, o->z, o, ship_security_avoidance);
	ai_brain(o);

	/* try to avoid collisions by computing steering and braking adjustments */
	ca.closest_dist2 = -1.0;
	ca.o = o;
	if (o->tsd.ship.ai[o->tsd.ship.nai_entries - 1].ai_mode == AI_MODE_FLEET_LEADER ||
		o->tsd.ship.ai[o->tsd.ship.nai_entries - 1].ai_mode == AI_MODE_FLEET_MEMBER)
		ca.worrythreshold = 150.0 * 150.0;
	else
		ca.worrythreshold = 400.0 * 400.0;
	space_partition_process(space_partition, o, o->x, o->z, &ca, ship_collision_avoidance);
	if (!o->alive) {
		(void) add_explosion(o->x, o->y, o->z, 50, 150, 50, o->type);
		respawn_object(o);
		delete_from_clients_and_server(o);
		return;
	}

	/* Consume fuel */
	if (rts_mode) {
		if (o->tsd.ship.ai[0].ai_mode != AI_MODE_RTS_RESUPPLY) { /* Prevent resupply ships from running out */
			int unit_type = ship_type[o->tsd.ship.shiptype].rts_unit_type;
			if (rts_unit_type(unit_type)->fuel_consumption_unit > o->tsd.ship.fuel)
				o->tsd.ship.fuel = 0;
			else
				o->tsd.ship.fuel -= rts_unit_type(unit_type)->fuel_consumption_unit;
		}
	}
	update_ship_position_and_velocity(o);
	update_ship_orientation(o);
	update_towed_ship(o);
	o->timestamp = universe_timestamp;

	o->tsd.ship.phaser_charge = update_phaser_banks(o->tsd.ship.phaser_charge, 200, 100);
	if (o->sdata.shield_strength > (255 - o->tsd.ship.damage.shield_damage))
		o->sdata.shield_strength = 255 - o->tsd.ship.damage.shield_damage;
}

static void damp_yaw_velocity(double *yv, double damp_factor)
{
	if (fabs(*yv) < (0.02 * PI / 180.0))
		*yv = 0.0;
	else
		*yv *= damp_factor;
}

static double rpmx[] = {
	 -1.0, 0.0,
		255.0 / 5.0,
		2.0 * 255.0 / 5.0,
		3.0 * 255.0 / 5.0,
		4.0 * 255.0 / 5.0,
		256.0 };

static double powery[] =  {
	0.0, 0.0, 
		0.33 * (double) UINT32_MAX, 
		0.6 * (double) UINT32_MAX, 
		0.8 * (double) UINT32_MAX,  
		0.95 * (double) UINT32_MAX,  
		0.8 * (double) UINT32_MAX,  } ;

static double tempy[] = {
	0.0, 0.0,
		0.3 * (double) UINT8_MAX,
		0.5 * (double) UINT8_MAX,
		0.55 * (double) UINT8_MAX,
		0.65 * (double) UINT8_MAX,
		1.0 * (double) UINT8_MAX,
	};

static double powertempy[] = {
	0.0, 0.0, 
		0.4,
		0.5,
		0.7,
		0.8,
		1.0,
};

static uint8_t update_phaser_banks(int current, int power, int max)
{
	double delta;
	if (current == max)
		return (uint8_t) current;

	delta = ((max - current) / 10.0) * (float) power / 80.0f;
	if (delta < 0 && delta > -1.0)
		delta = -1.0;
	if (delta > 0 && delta < 1.0)
		delta = 1.0;
	return (uint8_t) (current + (int) delta);
}

static int robot_collision_detect(struct snis_damcon_entity *o,
				double x, double y, struct damcon_data *d)
{
	int i;
	struct snis_damcon_entity *t;

	for (i = 0; i <= snis_object_pool_highest_object(d->pool); i++) {
		if (i == damcon_index(d, o)) /* skip self */
			continue;
		t = &d->o[i];
		switch (d->o[i].type) {
			case DAMCON_TYPE_ROBOT:
				break;
			case DAMCON_TYPE_WARPDRIVE:
			case DAMCON_TYPE_SENSORARRAY:
			case DAMCON_TYPE_COMMUNICATIONS:
			case DAMCON_TYPE_PHASERBANK:
			case DAMCON_TYPE_IMPULSE:
			case DAMCON_TYPE_MANEUVERING:
			case DAMCON_TYPE_SHIELDSYSTEM:
			case DAMCON_TYPE_TRACTORSYSTEM:
			case DAMCON_TYPE_LIFESUPPORTSYSTEM:
			case DAMCON_TYPE_REPAIR_STATION:
				/* ugh, this is ugly. Ideally, polygon intersection, but... bleah. */
				if (x + 25 > t->x && x - 25 < t->x + 270 &&
				    y + 25 > t->y - 90 && y - 25 < t->y + 90)
					return 1;
				break;
			default:
				break;
		}
	}
	return 0;
}

static double robot_clawx(struct snis_damcon_entity *o)
{
	return o->x + cos(o->tsd.robot.desired_heading - M_PI / 4.0)  * 70.0;
}

static double robot_clawy(struct snis_damcon_entity *o)
{
	return o->y + sin(o->tsd.robot.desired_heading - M_PI / 4.0)  * 70.0;
}

static int lookup_by_damcon_id(struct damcon_data *d, int id);

static void damcon_repair_socket_move(struct snis_damcon_entity *o,
					struct damcon_data *d)
{
	int i, id, new_damage;
	struct snis_damcon_entity *part = NULL;

	id = o->tsd.socket.contents_id;
	if (id == DAMCON_SOCKET_EMPTY)
		return;
	i = lookup_by_damcon_id(d, id);
	if (i < 0)
		return;
	part = &d->o[i];

	new_damage = part->tsd.part.damage - 8;
	if (new_damage < 0)
		new_damage = 0;
	if (part->tsd.part.damage != new_damage) {
		part->tsd.part.damage = new_damage;
		part->version++;
	}
}

static void *damcon_robot_nth_neighbor(__attribute__((unused)) void *context, void *node, int n)
{
	struct snis_damcon_entity *n1 = node;

	if (n < 0 || n > 9)
		return NULL;
	if (n1->type != DAMCON_TYPE_WAYPOINT)
		return NULL;
	return n1->tsd.waypoint.neighbor[n];
}

static float damcon_robot_movement_cost(void *context, void *node1, void *node2)
{
	struct snis_damcon_entity *n1, *n2;
	float dx, dy;
	assert(node1 != NULL);
	assert(node2 != NULL);
	n1 = node1;
	n2 = node2;
	assert(n1->type == DAMCON_TYPE_WAYPOINT);
	assert(n2->type == DAMCON_TYPE_WAYPOINT);

	dx = n1->x - n2->x;
	dy = n1->y - n2->y;
	return fabsf(dx) + fabsf(dy); /* Manhattan distance */
}

static struct snis_damcon_entity *find_nearest_waypoint(struct damcon_data *d, float x, float y)
{
	int i, nearest = -1;
	float dx, dy, dist, nearest_dist;

	for (i = 0; i <= snis_object_pool_highest_object(d->pool); i++) {
		if (d->o[i].type != DAMCON_TYPE_WAYPOINT)
			continue;
		dx = x - d->o[i].x;
		dy = y - d->o[i].y;
		dist = dx * dx + dy * dy;
		if (nearest < 0 || dist < nearest_dist) {
			nearest = i;
			nearest_dist = dist;
		}
	}
	if (nearest < 0)
		return NULL;
	return &d->o[nearest];
}

static struct snis_damcon_entity *find_nearest_waypoint_to_obj(struct damcon_data *d, struct snis_damcon_entity *target)
{
	return find_nearest_waypoint(d, target->x, target->y);
}

static struct snis_damcon_entity *damcon_find_closest_damaged_part_by_damage(struct damcon_data *d,
									uint8_t damage_limit)
{
	struct snis_damcon_entity *part, *damaged_part = NULL;
	uint8_t max_damage = 0;
	float dist, min_dist;
	int i;

	/* Find the *closest* (by manhattan distance) part that is more than damage_limit damaged */
	damaged_part = NULL;
	max_damage = 0;
	min_dist = 0;
	for (i = 0; i <= snis_object_pool_highest_object(d->pool); i++) {
		if (d->o[i].type != DAMCON_TYPE_PART)
			continue;
		part = &d->o[i];
		if (part->tsd.part.damage > max_damage && part->tsd.part.damage > damage_limit) {
			dist = fabs(d->robot->x - part->x) + fabs(d->robot->y - part->y);
			if (dist < min_dist || damaged_part == NULL) {
				damaged_part = part;
				max_damage = part->tsd.part.damage;
				min_dist = dist;
			}
		}
	}
	return damaged_part;
}

static struct snis_damcon_entity *damcon_find_next_thing_to_repair(struct damcon_data *d)
{
	struct snis_damcon_entity *part, *damaged_part = NULL;
	struct snis_entity *o;
	uint8_t max_damage = 0;
	int i;

	/* First get anything sitting around in the repair socket */
	for (i = 0; i <= snis_object_pool_highest_object(d->pool); i++) {
		if (d->o[i].type == DAMCON_TYPE_SOCKET &&
			d->o[i].tsd.socket.system == DAMCON_TYPE_REPAIR_STATION &&
			d->o[i].tsd.socket.contents_id != DAMCON_SOCKET_EMPTY) {
			i = lookup_by_damcon_id(d, d->o[i].tsd.socket.contents_id);
			if (i < 0)
				continue;
			return &d->o[i];
		}
	}

	/* Next if oxygen is < 50% and any oxygen part damaged more then 33%, repair that */
	i = lookup_by_id(bridgelist[d->bridge].shipid);
	if (i >= 0) {
		max_damage = 0;
		o = &go[i];
		if (o->tsd.ship.oxygen < (UINT32_MAX / 2)) {
			for (i = 0; i <= snis_object_pool_highest_object(d->pool); i++) {
				part = &d->o[i];
				if (part->type != DAMCON_TYPE_PART ||
					part->tsd.part.system != DAMCON_TYPE_LIFESUPPORTSYSTEM)
					continue;
				if (part->tsd.part.damage > max_damage) {
					damaged_part = part;
					max_damage = part->tsd.part.damage;
				}
			}
			if (max_damage >= 85) { /* 1/3rd of 256, or 33% damage */
				return damaged_part;
			}
		}
	}

	/* Next repair any shields below 60% */
	max_damage = 0;
	for (i = 0; i <= snis_object_pool_highest_object(d->pool); i++) {
		part = &d->o[i];
		if (part->type != DAMCON_TYPE_PART || part->tsd.part.system != DAMCON_TYPE_SHIELDSYSTEM)
			continue;
		if (part->tsd.part.damage > max_damage) {
			damaged_part = part;
			max_damage = part->tsd.part.damage;
		}
	}
	if (max_damage > 153) /* 60% of 256 */
		return damaged_part;

	/* Next find the *closest* (by manhattan distance) part that is more than 75%, 50%, 25%, or 0% damaged */
	damaged_part = damcon_find_closest_damaged_part_by_damage(d, 192);
	if (damaged_part)
		return damaged_part;
	damaged_part = damcon_find_closest_damaged_part_by_damage(d, 127);
	if (damaged_part)
		return damaged_part;
	damaged_part = damcon_find_closest_damaged_part_by_damage(d, 65);
	if (damaged_part)
		return damaged_part;
	damaged_part = damcon_find_closest_damaged_part_by_damage(d, 0);
	return damaged_part;
}

static struct snis_damcon_entity *find_repair_socket(struct damcon_data *d)
{
	int i;

	for (i = 0; i <= snis_object_pool_highest_object(d->pool); i++) {
		if (d->o[i].type != DAMCON_TYPE_SOCKET)
			continue;
		if (d->o[i].tsd.part.system != DAMCON_TYPE_REPAIR_STATION)
			continue;
		return &d->o[i];
	}
	return NULL;
}

static struct snis_damcon_entity *find_socket_for_part(struct damcon_data *d, struct snis_damcon_entity *part)
{
	int i;

	for (i = 0; i <= snis_object_pool_highest_object(d->pool); i++) {
		if (d->o[i].type != DAMCON_TYPE_SOCKET)
			continue;
		if (d->o[i].tsd.socket.part == part->tsd.part.part &&
			d->o[i].tsd.socket.system == part->tsd.part.system)
				return &d->o[i];
	}
	return NULL;
}

static struct snis_damcon_entity *find_nth_waypoint(struct damcon_data *d, int n);
static struct snis_damcon_entity *find_robot_goal(struct damcon_data *d)
{
	struct snis_damcon_entity *part, *socket, *waypoint;
	float dx, dy, dist2;
	int i, next_state;

	/* Is the robot carrying something? */
	if (d->robot->tsd.robot.cargo_id != ROBOT_CARGO_EMPTY) {
		i = lookup_by_damcon_id(d, d->robot->tsd.robot.cargo_id);
		if (i < 0)
			return NULL;
		part = &d->o[i];
		if (part->tsd.part.damage >= DAMCON_EASY_REPAIR_THRESHOLD) {
			/* have to take it to the repair station */
			socket = find_repair_socket(d);
			waypoint = find_nearest_waypoint_to_obj(d, socket);
			d->robot->tsd.robot.repair_socket_id = socket->id;
			next_state = DAMCON_ROBOT_REPAIR;
		} else {
			socket = find_socket_for_part(d, part);
			if (!socket)
				return NULL;
			waypoint = find_nearest_waypoint_to_obj(d, socket);
			next_state = DAMCON_ROBOT_REPLACE;
		}
		dx = waypoint->x - d->robot->x;
		dy = waypoint->y - d->robot->y;
		dist2 = (dx * dx) + (dy * dy);
		if (dist2 < 20 * 20)
			d->robot->tsd.robot.robot_state = next_state;
		return waypoint;
	}

	d->robot->tsd.robot.damaged_part_id = (uint32_t) -1;
	part = damcon_find_next_thing_to_repair(d);
	if (part)
		waypoint = find_nearest_waypoint_to_obj(d, part);
	else
		waypoint = find_nth_waypoint(d, 17); /* default destination */
	if (waypoint && !part)
		return waypoint;
	if (waypoint && part) {
		if (part && part->type == DAMCON_TYPE_PART)
			d->robot->tsd.robot.damaged_part_id = part->id;
		dx = d->robot->x - waypoint->x;
		dy = d->robot->y - waypoint->y;
		dist2 = (dx * dx) + (dy * dy);
		if (dist2 < 50 * 50 && part->type == DAMCON_TYPE_PART)
			d->robot->tsd.robot.robot_state = DAMCON_ROBOT_PICKUP;
		return waypoint;
	} else {
		return find_nearest_waypoint(d, d->robot->tsd.robot.long_term_goal_x,
						d->robot->tsd.robot.long_term_goal_y);
	}
}

typedef void (*robot_claw_action_fn)(struct damcon_data *d);
typedef int (*robot_claw_condition_fn)(struct damcon_data *d);

static void print_waypoint_table(struct damcon_data *d, int tablesize);
static void do_robot_drop(struct damcon_data *d);
static void do_robot_pickup(struct damcon_data *d);

static int damcon_part_sufficiently_repaired(struct damcon_data *d)
{
	int i;
	struct snis_damcon_entity *part;

	i = lookup_by_damcon_id(d, d->robot->tsd.robot.damaged_part_id);
	if (i < 0) {
		d->robot->tsd.robot.robot_state = DAMCON_ROBOT_DECIDE_LTG;
		return 0;
	}
	part = &d->o[i];
	return part->tsd.part.damage < DAMCON_EASY_REPAIR_THRESHOLD;
}

/* damcon_robot_claw_object():
 *   Make the robot aim its claw at something and either pick it up or put it down,
 *   then transition to a new state
 */
static void damcon_robot_claw_object(struct damcon_data *d, struct snis_damcon_entity *object,
					robot_claw_action_fn robot_claw_action,
					robot_claw_condition_fn robot_claw_condition,
					int next_robot_state)
{
	double dx, dy, goal_heading, desired_delta_angle;

	dx = robot_clawx(d->robot) - object->x;
	dy = robot_clawy(d->robot) - object->y;
	if (dx * dx + dy * dy < ROBOT_MAX_GRIP_DIST2) {
		if (robot_claw_condition) {
			if (!robot_claw_condition(d))
				return; /* condition not met */
		}
		robot_claw_action(d);
		d->robot->tsd.robot.robot_state = next_robot_state;
		return;
	} else {
		dx = d->robot->x - object->x;
		dy = d->robot->y - object->y;
		goal_heading = atan2(-dx, dy) - M_PI / 4;
		normalize_angle(&goal_heading);
		desired_delta_angle = goal_heading - d->robot->heading;
		normalize_angle(&desired_delta_angle);
		while (desired_delta_angle > M_PI)
			desired_delta_angle = desired_delta_angle - 2.0 * M_PI;
		while (desired_delta_angle < -M_PI)
			desired_delta_angle = desired_delta_angle + 2.0 * M_PI;
		desired_delta_angle = 0.25 * desired_delta_angle;
		if (desired_delta_angle > 5.0 * M_PI / 180.0)
			desired_delta_angle = 5 * M_PI / 180.0;
		if (desired_delta_angle < -5.0 * M_PI / 180.0)
			desired_delta_angle = -5.0 * M_PI / 180.0;
		d->robot->tsd.robot.desired_heading += desired_delta_angle;
		d->robot->tsd.robot.desired_velocity = 0;
		normalize_angle(&d->robot->tsd.robot.desired_heading);
	}
}

static void damcon_robot_think(struct snis_damcon_entity *o, struct damcon_data *d)
{
	double goal_heading, desired_delta_angle;
	double dist, dx, dy;
	struct a_star_path *path;
	struct snis_damcon_entity *part, *socket, *goal;
	struct snis_damcon_entity *start;
	int i;

	if (o->tsd.robot.autonomous_mode == DAMCON_ROBOT_MANUAL_MODE)
		return;

	switch (o->tsd.robot.robot_state) {
	case  DAMCON_ROBOT_DECIDE_LTG:
		start = find_nearest_waypoint_to_obj(d, d->robot);
		goal = find_robot_goal(d);
		d->robot->tsd.robot.long_term_goal_x = goal->x;
		d->robot->tsd.robot.long_term_goal_y = goal->y;
		path = a_star(d, start, goal, snis_object_pool_highest_object(d->pool),
				damcon_robot_movement_cost,
				damcon_robot_movement_cost,
				damcon_robot_nth_neighbor);
		if (d->robot->tsd.robot.robot_state == DAMCON_ROBOT_REPAIR)
			break;
		if (d->robot->tsd.robot.robot_state == DAMCON_ROBOT_REPLACE)
			break;
		if (d->robot->tsd.robot.robot_state == DAMCON_ROBOT_PICKUP)
			break;
		if (!path) {
			fprintf(stderr, "%s: path is null at %s:%dn", logprefix(), __FILE__, __LINE__);
			break;
		}
		if (path->node_count <= 0) {
			fprintf(stderr, "%s: path->node_count is %d\n", logprefix(), path->node_count);
			break;
		}
		if (path->node_count > 1)
			goal = path->path[1];
		else if (path->node_count > 0)
			goal = path->path[0];
		d->robot->tsd.robot.short_term_goal_x = goal->x;
		d->robot->tsd.robot.short_term_goal_y = goal->y;
		d->robot->tsd.robot.autonomous_mode = DAMCON_ROBOT_FULLY_AUTONOMOUS;
		d->robot->tsd.robot.robot_state = DAMCON_ROBOT_CRUISE;
		free(path);
		break;
	case DAMCON_ROBOT_CRUISE:
		dx = o->tsd.robot.short_term_goal_x - o->x;
		dy = o->tsd.robot.short_term_goal_y - o->y;
		dist = dx * dx + dy * dy;
		if (dist < 400) { /* we have arrived */
			if (o->tsd.robot.autonomous_mode == DAMCON_ROBOT_SEMI_AUTONOMOUS)
				o->tsd.robot.robot_state = DAMCON_ROBOT_DECIDE_LTG;
			if (o->tsd.robot.autonomous_mode == DAMCON_ROBOT_FULLY_AUTONOMOUS)
				o->tsd.robot.robot_state = DAMCON_ROBOT_DECIDE_LTG;
			return;
		}
		goal_heading = atan2(dy, dx) + M_PI / 2;
		normalize_angle(&goal_heading);
		desired_delta_angle = goal_heading - o->tsd.robot.desired_heading;
		if (desired_delta_angle > M_PI)
			desired_delta_angle = desired_delta_angle - 2.0 * M_PI;
		else if (desired_delta_angle < -M_PI)
			desired_delta_angle = desired_delta_angle + 2.0 * M_PI;

		if (fabs(desired_delta_angle) < 15.0 * M_PI / 180.0 && dist > 600)
			o->tsd.robot.desired_velocity =
					-max_damcon_robot_velocity /
						((fabs(desired_delta_angle) * 180.0 / M_PI) + 1.0) / 3.0;
		else if (fabs(desired_delta_angle) < 40.0 * M_PI / 180.0 && dist > 200)
			o->tsd.robot.desired_velocity = 0.5 *
					-max_damcon_robot_velocity /
						((fabs(desired_delta_angle) * 180.0 / M_PI) + 1.0) / 3.0;
		else
			o->tsd.robot.desired_velocity = 0;

		desired_delta_angle = 0.25 * desired_delta_angle;
		o->tsd.robot.desired_heading += desired_delta_angle;
		normalize_angle(&o->tsd.robot.desired_heading);
		break;
	case DAMCON_ROBOT_PICKUP:
		i = lookup_by_damcon_id(d, d->robot->tsd.robot.damaged_part_id);
		if (i < 0) {
			d->robot->tsd.robot.robot_state = DAMCON_ROBOT_DECIDE_LTG;
			return;
		}
		damcon_robot_claw_object(d, &d->o[i], do_robot_pickup, NULL, DAMCON_ROBOT_DECIDE_LTG);
		break;
	case DAMCON_ROBOT_REPAIR:
		if (d->robot->tsd.robot.cargo_id == DAMCON_SOCKET_EMPTY) {
			d->robot->tsd.robot.robot_state = DAMCON_ROBOT_DECIDE_LTG;
			return;
		}
		i = lookup_by_damcon_id(d, d->robot->tsd.robot.cargo_id);
		if (i < 0) {
			d->robot->tsd.robot.robot_state = DAMCON_ROBOT_DECIDE_LTG;
			return;
		}
		part = &d->o[i];
		if (part->tsd.part.damage < DAMCON_EASY_REPAIR_THRESHOLD) {
			d->robot->tsd.robot.robot_state = DAMCON_ROBOT_DECIDE_LTG;
			return;
		}
		socket = NULL;
		if (d->robot->tsd.robot.repair_socket_id != ROBOT_CARGO_EMPTY) {
			i = lookup_by_damcon_id(d, d->robot->tsd.robot.repair_socket_id);
			if (i >= 0)
				socket = &d->o[i];
		}
		if (!socket) {
			socket = find_repair_socket(d);
			if (!socket) {
				d->robot->tsd.robot.robot_state = DAMCON_ROBOT_DECIDE_LTG;
				return;
			}
		}
		damcon_robot_claw_object(d, socket, do_robot_drop, NULL, DAMCON_ROBOT_REPAIR_WAIT);
		break;
	case DAMCON_ROBOT_REPAIR_WAIT:
		if (d->robot->tsd.robot.repair_socket_id == DAMCON_SOCKET_EMPTY) {
			d->robot->tsd.robot.robot_state = DAMCON_ROBOT_DECIDE_LTG;
			return;
		}
		i = lookup_by_damcon_id(d, d->robot->tsd.robot.repair_socket_id);
		if (i < 0) {
			d->robot->tsd.robot.robot_state = DAMCON_ROBOT_DECIDE_LTG;
			return;
		}
		socket = &d->o[i];
		i = lookup_by_damcon_id(d, d->robot->tsd.robot.damaged_part_id);
		if (i < 0) {
			d->robot->tsd.robot.robot_state = DAMCON_ROBOT_DECIDE_LTG;
			return;
		}
		damcon_robot_claw_object(d, socket, do_robot_pickup, damcon_part_sufficiently_repaired,
					DAMCON_ROBOT_DECIDE_LTG);
		break;
	case DAMCON_ROBOT_REPLACE:
		i = lookup_by_damcon_id(d, d->robot->tsd.robot.cargo_id);
		if (i < 0) {
			d->robot->tsd.robot.robot_state = DAMCON_ROBOT_DECIDE_LTG;
			return;
		}
		part = &d->o[i];
		if (part->tsd.part.damage > 0)
			return; /* just wait for it to repair */
		socket = find_socket_for_part(d, part);
		if (!socket) {
			d->robot->tsd.robot.robot_state = DAMCON_ROBOT_DECIDE_LTG;
			return;
		}
		damcon_robot_claw_object(d, socket, do_robot_drop, NULL, DAMCON_ROBOT_DECIDE_LTG);
		break;
	default:
		break;
	}
}

static void damcon_robot_move(struct snis_damcon_entity *o, struct damcon_data *d)
{
	double vx, vy, lastx, lasty, lasth, dv;
	int bounds_hit = 0;
	struct snis_damcon_entity *cargo = NULL;
	double clawx, clawy;

	damcon_robot_think(o, d);

	lastx = o->x;
	lasty = o->y;
	lasth = o->heading;

	vy = o->velocity * cos(o->heading);
	vx = o->velocity * -sin(o->heading);

	if (robot_collision_detect(o, o->x + vx, o->y + vy, d)) {
		vx = 0;
		vy = 0;
		o->velocity = 0;
		if (fabs(o->tsd.robot.desired_velocity) > max_damcon_robot_velocity / 5.0)
			o->tsd.robot.desired_velocity =  
				(max_damcon_robot_velocity / 5.5) * o->tsd.robot.desired_velocity /
					fabs(o->tsd.robot.desired_velocity);
	}

	o->tsd.robot.desired_heading += o->tsd.robot.yaw_velocity;
	normalize_angle(&o->tsd.robot.desired_heading);
	damp_yaw_velocity(&o->tsd.robot.yaw_velocity, YAW_DAMPING * 0.9);

	normalize_angle(&o->heading);
	if (o->heading != o->tsd.robot.desired_heading) {
		double diff = o->tsd.robot.desired_heading - o->heading;
		double max_heading_change;

		while (fabs(diff) > M_PI) {
			/* trying to go the wrong way around the circle */
			if (diff < 0)
				diff += 2.0 * M_PI;
			else
				diff -= 2.0 * M_PI;
		}

		/* Slower you're going, quicker you can turn.  The "+ 0.01" is to avoid divide by zero. */
		max_heading_change = (max_damcon_robot_velocity / (fabs(o->velocity) + 0.01)) * 6.0 * M_PI / 180.0;
		if (fabs(diff) > max_heading_change)
			diff = max_heading_change * diff / fabs(diff);

		o->heading += diff;
		o->velocity *= (1.0 - (0.4 * fabs(sin(diff))));
		o->tsd.robot.desired_velocity *= (1.0 - (0.4 * fabs(sin(diff))));
		normalize_angle(&o->heading);
	}

	dv = o->tsd.robot.desired_velocity;
	if (fabs(o->velocity - dv) > 0.01) {
		double diff = dv - o->velocity;

		if (fabs(o->velocity) - fabs(dv) > 0) {
			/* braking */
			if (fabs(diff) > max_damcon_robot_braking)
				diff = max_damcon_robot_braking * diff / fabs(diff);
		} else {
			/* accelerating. */ 
			if (fabs(diff) > MAX_ROBOT_ACCELERATION)
				diff = MAX_ROBOT_ACCELERATION * diff / fabs(diff);
		}
		o->velocity += diff;
	} else {
		o->velocity = o->tsd.robot.desired_velocity;
	}

	/* Bounds checking */
	if (o->x < -DAMCONXDIM / 2.0 + DAMCON_WALL_DIST) {
		o->x = -DAMCONXDIM / 2.0 + DAMCON_WALL_DIST;
		bounds_hit = 1;
	}
	if (o->x > DAMCONXDIM / 2.0 - DAMCON_WALL_DIST) {
		o->x = DAMCONXDIM / 2.0 - DAMCON_WALL_DIST;
		bounds_hit = 1;
	}
	if (o->y < -DAMCONYDIM / 2.0 + DAMCON_WALL_DIST) {
		o->y = -DAMCONYDIM / 2.0 + DAMCON_WALL_DIST;
		bounds_hit = 1;
	}
	if (o->y > DAMCONYDIM / 2.0 - DAMCON_WALL_DIST) {
		o->y = DAMCONYDIM / 2.0 - DAMCON_WALL_DIST;
		bounds_hit = 1;
	}
	if (bounds_hit) {
		o->velocity *= 0.4;
		if (fabs(o->tsd.robot.desired_velocity) > max_damcon_robot_velocity / 5.0)
			o->tsd.robot.desired_velocity =  
				(max_damcon_robot_velocity / 5.5) * o->tsd.robot.desired_velocity /
					fabs(o->tsd.robot.desired_velocity);
	}

#if 0
	/* Damp velocity */
	if (fabs(o->velocity) < MIN_ROBOT_VELOCITY)
		o->velocity = 0.0;
	else
		o->velocity *= ROBOT_VELOCITY_DAMPING;
#endif
	o->x += vx;
	o->y += vy;
	clawx = robot_clawx(o);
	clawy = robot_clawy(o);

	if (o->tsd.robot.cargo_id != ROBOT_CARGO_EMPTY) {
		int i;
		
		i = lookup_by_damcon_id(d, o->tsd.robot.cargo_id);
		if (i >= 0) {
			int new_damage;

			cargo = &d->o[i];
			cargo->x = clawx;
			cargo->y = clawy;
			cargo->heading = o->tsd.robot.desired_heading + M_PI;
			normalize_angle(&cargo->heading);

			/* If part is lightly damaged, the robot can repair in place */
			if (cargo->tsd.part.damage < DAMCON_EASY_REPAIR_THRESHOLD) {
				new_damage = cargo->tsd.part.damage - 8;
				if (new_damage < 0)
					new_damage = 0;
				if (cargo->tsd.part.damage != new_damage) {
					cargo->tsd.part.damage = new_damage;
					cargo->version++;
				}
			}
		}
	}


	if (fabs(lastx - o->x) > 0.00001 ||
		fabs(lasty - o->y) > 0.00001 ||
		fabs(lasth - o->heading) > 0.00001) {
		o->version++;
		if (cargo)
			cargo->version++;
	}

	/* TODO: collision detection */
}

static unsigned char device_power_byte_form(struct power_device *d)
{
	return (unsigned char) (255.0 * device_current(d) / device_max_current(d));
}

static void do_power_model_computations(struct snis_entity *o)
{
	struct power_model *m = o->tsd.ship.power_model;
	struct power_device *device;

	power_model_compute(m);

#define WARP_POWER_DEVICE 0
#define SENSORS_POWER_DEVICE 1
#define PHASERS_POWER_DEVICE 2
#define MANEUVERING_POWER_DEVICE 3
#define SHIELDS_POWER_DEVICE 4
#define COMMS_POWER_DEVICE 5
#define IMPULSE_POWER_DEVICE 6
#define TRACTOR_POWER_DEVICE 7
#define LIFESUPPORT_POWER_DEVICE 8

	device = power_model_get_device(m, WARP_POWER_DEVICE);
	power_device_set_damage(device, (float) o->tsd.ship.damage.warp_damage / 255.0f);
	if (o->tsd.ship.warp_core_status != WARP_CORE_STATUS_EJECTED)
		o->tsd.ship.power_data.warp.i = device_power_byte_form(device);
	else /* Force current to zero if warp core has been ejected. */
		o->tsd.ship.power_data.warp.i = 0;

	device = power_model_get_device(m, SENSORS_POWER_DEVICE);
	power_device_set_damage(device, (float) o->tsd.ship.damage.sensors_damage / 255.0f);
	o->tsd.ship.power_data.sensors.i = device_power_byte_form(device);

	device = power_model_get_device(m, PHASERS_POWER_DEVICE);
	power_device_set_damage(device, (float) o->tsd.ship.damage.phaser_banks_damage / 255.0f);
	o->tsd.ship.power_data.phasers.i = device_power_byte_form(device);

	device = power_model_get_device(m, MANEUVERING_POWER_DEVICE);
	power_device_set_damage(device, (float) o->tsd.ship.damage.maneuvering_damage / 255.0f);
	o->tsd.ship.power_data.maneuvering.i = device_power_byte_form(device);

	device = power_model_get_device(m, SHIELDS_POWER_DEVICE);
	power_device_set_damage(device, (float) o->tsd.ship.damage.shield_damage / 255.0f);
	o->tsd.ship.power_data.shields.i = device_power_byte_form(device);

	device = power_model_get_device(m, COMMS_POWER_DEVICE);
	power_device_set_damage(device, (float) o->tsd.ship.damage.comms_damage / 255.0f);
	o->tsd.ship.power_data.comms.i = device_power_byte_form(device);

	device = power_model_get_device(m, IMPULSE_POWER_DEVICE);
	power_device_set_damage(device, (float) o->tsd.ship.damage.impulse_damage / 255.0f);
	o->tsd.ship.power_data.impulse.i = device_power_byte_form(device);

	device = power_model_get_device(m, TRACTOR_POWER_DEVICE);
	power_device_set_damage(device, (float) o->tsd.ship.damage.tractor_damage / 255.0f);
	o->tsd.ship.power_data.tractor.i = device_power_byte_form(device);

	device = power_model_get_device(m, LIFESUPPORT_POWER_DEVICE);
	power_device_set_damage(device, (float) o->tsd.ship.damage.lifesupport_damage / 255.0f);
	o->tsd.ship.power_data.lifesupport.i = device_power_byte_form(device);

	o->tsd.ship.power_data.voltage = (unsigned char)
		(255.0 * power_model_actual_voltage(m) / power_model_nominal_voltage(m));
}

static void do_coolant_model_computations(struct snis_entity *o)
{
	struct power_model *m = o->tsd.ship.coolant_model;
	struct power_device *device;

	power_model_compute(m);

	device = power_model_get_device(m, WARP_POWER_DEVICE);
	o->tsd.ship.coolant_data.warp.i = device_power_byte_form(device);

	device = power_model_get_device(m, SENSORS_POWER_DEVICE);
	o->tsd.ship.coolant_data.sensors.i = device_power_byte_form(device);

	device = power_model_get_device(m, PHASERS_POWER_DEVICE);
	o->tsd.ship.coolant_data.phasers.i = device_power_byte_form(device);

	device = power_model_get_device(m, MANEUVERING_POWER_DEVICE);
	o->tsd.ship.coolant_data.maneuvering.i = device_power_byte_form(device);

	device = power_model_get_device(m, SHIELDS_POWER_DEVICE);
	o->tsd.ship.coolant_data.shields.i = device_power_byte_form(device);

	device = power_model_get_device(m, COMMS_POWER_DEVICE);
	o->tsd.ship.coolant_data.comms.i = device_power_byte_form(device);

	device = power_model_get_device(m, IMPULSE_POWER_DEVICE);
	o->tsd.ship.coolant_data.impulse.i = device_power_byte_form(device);

	device = power_model_get_device(m, TRACTOR_POWER_DEVICE);
	o->tsd.ship.coolant_data.tractor.i = device_power_byte_form(device);

	device = power_model_get_device(m, LIFESUPPORT_POWER_DEVICE);
	o->tsd.ship.coolant_data.lifesupport.i = device_power_byte_form(device);

	o->tsd.ship.coolant_data.voltage = (unsigned char)
		(255.0 * power_model_actual_voltage(m) / power_model_nominal_voltage(m));
}

static uint8_t steady_state_temperature(uint8_t current, uint8_t coolant)
{
	float t;
	t = ((float) current * 1.05f / (coolant * 0.95f + 1.0f));
	t = (200.0f * t) + 25.0f;
	if (t > 255.0)
		t = 255.0;
	if (t < 0)
		t = 0;
	return (uint8_t) t;
}

static void calc_temperature_change(uint8_t current, uint8_t coolant, uint8_t *temperature)
{
	float diff, steady, answer;

	steady = (float) steady_state_temperature(current, coolant);
	diff = steady - (float) *temperature;
	diff = diff / 255.0f;

	if (diff > 0) /* heating up */
		diff = diff * 255.0f / ((float) coolant + 20.0);
	else /* cooling down */
		diff = diff * ((float) coolant) / 255.0f;

	if (diff < 1.0f && diff > 0.0f && (universe_timestamp & 0x08) == 0)
		diff = 1.0f;
	if (diff > -1.0f && diff < 0.0f && (universe_timestamp & 0x08) == 0)
		diff = -1.0f;

	answer = (float) *temperature + diff;
	if (answer > 255.0f)
		answer = 255.0f;
	if (answer < 0.0f)
		answer = 0.0f;
	*temperature = answer;
}

static void do_temperature_computations(struct snis_entity *o)
{
	int b = lookup_bridge_by_shipid(o->id);

	calc_temperature_change(o->tsd.ship.power_data.warp.i,
			o->tsd.ship.coolant_data.warp.i,
			&o->tsd.ship.temperature_data.warp_damage);
	calc_temperature_change(o->tsd.ship.power_data.sensors.i,
			o->tsd.ship.coolant_data.sensors.i,
			&o->tsd.ship.temperature_data.sensors_damage);
	calc_temperature_change(o->tsd.ship.power_data.phasers.i,
			o->tsd.ship.coolant_data.phasers.i,
			&o->tsd.ship.temperature_data.phaser_banks_damage);
	calc_temperature_change(o->tsd.ship.power_data.maneuvering.i,
			o->tsd.ship.coolant_data.maneuvering.i,
			&o->tsd.ship.temperature_data.maneuvering_damage);
	calc_temperature_change(o->tsd.ship.power_data.shields.i,
			o->tsd.ship.coolant_data.shields.i,
			&o->tsd.ship.temperature_data.shield_damage);
	calc_temperature_change(o->tsd.ship.power_data.comms.i,
			o->tsd.ship.coolant_data.comms.i,
			&o->tsd.ship.temperature_data.comms_damage);
	calc_temperature_change(o->tsd.ship.power_data.impulse.i,
			o->tsd.ship.coolant_data.impulse.i,
			&o->tsd.ship.temperature_data.impulse_damage);
	calc_temperature_change(o->tsd.ship.power_data.tractor.i,
			o->tsd.ship.coolant_data.tractor.i,
			&o->tsd.ship.temperature_data.tractor_damage);
	calc_temperature_change(o->tsd.ship.power_data.lifesupport.i,
			o->tsd.ship.coolant_data.lifesupport.i,
			&o->tsd.ship.temperature_data.lifesupport_damage);


	/* overheated and overdamaged warp system == self destruct */
	if (o->tsd.ship.temperature_data.warp_damage > 240 &&
		o->tsd.ship.damage.warp_damage > 240 &&
		o->tsd.ship.warp_core_status != WARP_CORE_STATUS_EJECTED) {
		if (!bridgelist[b].warp_core_critical) {
			bridgelist[b].warp_core_critical = 1;
			bridgelist[b].warp_core_critical_timer = 300; /* 30 seconds */
		}
	}
	if (bridgelist[b].warp_core_critical) {
		if (bridgelist[b].warp_core_critical_timer > 0 &&
			(bridgelist[b].warp_core_critical_timer % 100) == 0) {
			char msg[100];
			snprintf(msg, sizeof(msg), "Warning, %d seconds until warp core breach",
					bridgelist[b].warp_core_critical_timer / 10);
			snis_queue_add_text_to_speech(msg, ROLE_TEXT_TO_SPEECH, o->id);
		}
		bridgelist[b].warp_core_critical_timer--;
		if (bridgelist[b].warp_core_critical_timer == 0) {
			bridgelist[b].warp_core_critical = 0;
			if (!player_invincibility) {
				o->alive = 0;
				o->tsd.ship.damage.shield_damage = 255;
				o->timestamp = universe_timestamp;
				o->respawn_time = universe_timestamp + player_respawn_time * 10;
				schedule_callback(event_callback, &callback_schedule,
					"player-death-callback", o->id);
			}
			snis_queue_add_sound(EXPLOSION_SOUND, ROLE_SOUNDSERVER, o->id);
		}
	}
}

static int calc_overheat_damage(struct snis_entity *o, struct damcon_data *d,
				uint8_t *system, uint8_t temperature)
{
	float damage, overheat_amount, old_value, new_value;
	int system_number = system - (unsigned char *) &o->tsd.ship.damage;

	BUILD_ASSERT(sizeof(o->tsd.ship.damage) == DAMCON_SYSTEM_COUNT - 1);
	if (system_number < 0 || system_number >= DAMCON_SYSTEM_COUNT - 1)
		fprintf(stderr, "system_number out of range: %d at %s:%d\n",
			system_number, __FILE__, __LINE__);
	if (temperature < (uint8_t) (0.9f * 255.0f))
		return 0; /* temp is ok, no damage */

	if (snis_randn(100) > 10) 
		return 0; /* lucky, no damage */

	overheat_amount = (temperature - (0.9f * 255.0f)) / (0.1f * 255.0f);

	damage = (float) snis_randn(5) / 100.0f;
	damage *= overheat_amount; 
	old_value = (float) *system;
	new_value = old_value + 255.0f * damage;
	if (new_value > 255.0f)
		new_value = 255.0f;
	*system = (uint8_t) new_value;
	damage = (new_value - old_value);
	distribute_damage_to_damcon_system_parts(o, d, (int) damage, system_number);
	return 1;
}

static int do_overheating_damage(struct snis_entity *o)
{
	int damage_was_done = 0;
	struct damcon_data *d;
	int b;

	if (player_invincibility)
		return 0;

	b = lookup_bridge_by_shipid(o->id);
	d = &bridgelist[b].damcon;
	

	damage_was_done += calc_overheat_damage(o, d, &o->tsd.ship.damage.warp_damage,
			o->tsd.ship.temperature_data.warp_damage);
	damage_was_done += calc_overheat_damage(o, d, &o->tsd.ship.damage.sensors_damage,
			o->tsd.ship.temperature_data.sensors_damage);
	damage_was_done += calc_overheat_damage(o, d, &o->tsd.ship.damage.phaser_banks_damage,
			o->tsd.ship.temperature_data.phaser_banks_damage);
	damage_was_done += calc_overheat_damage(o, d, &o->tsd.ship.damage.maneuvering_damage,
			o->tsd.ship.temperature_data.maneuvering_damage);
	damage_was_done += calc_overheat_damage(o, d, &o->tsd.ship.damage.shield_damage,
			o->tsd.ship.temperature_data.shield_damage);
	damage_was_done += calc_overheat_damage(o, d, &o->tsd.ship.damage.comms_damage,
			o->tsd.ship.temperature_data.comms_damage);
	damage_was_done += calc_overheat_damage(o, d, &o->tsd.ship.damage.impulse_damage,
			o->tsd.ship.temperature_data.impulse_damage);
	damage_was_done += calc_overheat_damage(o, d, &o->tsd.ship.damage.tractor_damage,
			o->tsd.ship.temperature_data.tractor_damage);
	return damage_was_done;
}

static int lookup_bridge_by_shipid(uint32_t shipid)
{
	/* assumes universe lock is held */
	int i;

	for (i = 0; i < nbridges; i++)
		if (bridgelist[i].shipid == shipid)
			return i;
	return -1;
}

static int lookup_bridge_by_ship_name(char *name)
{
	/* assumes universe lock is held */
	int i;

	for (i = 0; i < nbridges; i++) {
		if (strcasecmp((char *) bridgelist[i].shipname, name) == 0)
			return i;
	}
	return -1;
}

static void calculate_ship_scibeam_info(struct snis_entity *ship)
{
	double range, tx, tz, angle, A1, A2;

	/* calculate the range of the science scope based on power. */
	range = (MAX_SCIENCE_SCREEN_RADIUS - MIN_SCIENCE_SCREEN_RADIUS) *
		(ship->tsd.ship.scizoom / 255.0) + MIN_SCIENCE_SCREEN_RADIUS;
	ship->tsd.ship.scibeam_range = range;

	tx = cos(ship->tsd.ship.sci_heading) * range;
	tz = -sin(ship->tsd.ship.sci_heading) * range;

	angle = atan2(tz, tx);
	A1 = angle - ship->tsd.ship.sci_beam_width / 2.0;
	A2 = angle + ship->tsd.ship.sci_beam_width / 2.0;
        if (A1 < -M_PI)
                A1 += 2.0 * M_PI;
        if (A2 > M_PI)
                A2 -= 2.0 * M_PI;
	ship->tsd.ship.scibeam_a1 = A1;
	ship->tsd.ship.scibeam_a2 = A2;
	return;
}

static void do_thrust(struct snis_entity *ship)
{
	double current_max_player_velocity = ship->tsd.ship.reverse ?
		(max_reverse_player_velocity * ship->tsd.ship.power_data.impulse.i) / 255 :
		(max_player_velocity * ship->tsd.ship.power_data.impulse.i) / 255;

	if (!ship->tsd.ship.reverse) {
		if (ship->tsd.ship.velocity < current_max_player_velocity)
			ship->tsd.ship.velocity += player_velocity_increment;
	} else {
		if (ship->tsd.ship.velocity > -current_max_player_velocity)
			ship->tsd.ship.velocity -= player_velocity_increment;
	}
	if (ship->tsd.ship.velocity > current_max_player_velocity ||
		ship->tsd.ship.velocity < -current_max_player_velocity)
		ship->tsd.ship.velocity = ship->tsd.ship.velocity * 0.99;
}

static void scoop_up_cargo(struct snis_entity *player, struct snis_entity *cargo)
{
	int i;

	for (i = 0; i < player->tsd.ship.ncargo_bays; i++)
		if (player->tsd.ship.cargo[i].contents.item == -1) {
			/* put it in first empty cargo bay */
			player->tsd.ship.cargo[i].contents = cargo->tsd.cargo_container.contents;
			player->tsd.ship.cargo[i].paid = 0.0f;
			player->tsd.ship.cargo[i].origin = -1;
			player->tsd.ship.cargo[i].dest = -1;
			player->tsd.ship.cargo[i].due_date = -1;
			cargo->alive = 0;
			delete_from_clients_and_server(cargo);
			snis_queue_add_sound(TRANSPORTER_SOUND,
					ROLE_SOUNDSERVER, player->id);
			schedule_callback3(event_callback, &callback_schedule,
					"cargo-container-acquired-event",
					(double) player->id,
					(double) player->tsd.ship.cargo[i].contents.item,
					(double) player->tsd.ship.cargo[i].contents.qty);
			break;
		}
}

static void revoke_docking_permission(struct snis_entity *docking_port, uint32_t player_id)
{
	struct snis_entity *starbase;
	int i, model;

	i = lookup_by_id(docking_port->tsd.docking_port.parent);
	if (i < 0)
		return;
	starbase = &go[i];
	model = starbase->id % nstarbase_models;
	for (i = 0; i < docking_port_info[model]->nports; i++) {
		if (starbase->tsd.starbase.expected_docker[i] == player_id) {
			starbase->tsd.starbase.expected_docker[i] = -1;
			starbase->tsd.starbase.expected_docker_timer[i] = 0;
		}
	}
}

static int rate_limit_docking_permission_denied(struct bridge_data *b)
{
	if (universe_timestamp - b->last_docking_permission_denied_time < 150)
		return 0;
	b->last_docking_permission_denied_time = universe_timestamp;
	return 1;
}

static int starbase_grant_docker_permission(struct snis_entity *starbase,
						struct snis_entity *docker, struct bridge_data *b,
						char *npcname, int channel)
{
	/* check if permission is already granted.... */
	int model = starbase->id % nstarbase_models;
	int i;

	for (i = 0; i < docking_port_info[model]->nports; i++) {
		if (starbase->tsd.starbase.expected_docker[i] == docker->id &&
			starbase->tsd.starbase.expected_docker_timer[i] > 0) {
			starbase->tsd.starbase.expected_docker_timer[i] = STARBASE_DOCK_TIME;
			/* transmit re-granting of docking permission */
			send_comms_packet(starbase, npcname, channel,
				"%s, PERMISSION TO DOCK RE-GRANTED.", b->shipname);
			snis_queue_add_sound(PERMISSION_TO_DOCK_GRANTED, ROLE_NAVIGATION, b->shipid);
			if (docker->tsd.ship.exterior_lights != 255)
				send_comms_packet(starbase, npcname, channel,
					"%s, ALSO, PLEASE TURN ON YOUR EXTERIOR LIGHTS.", b->shipname);
			return 1;
		}
	}
	/* See if there are any empty slots */
	for (i = 0; i < docking_port_info[model]->nports; i++) {
		if (starbase->tsd.starbase.expected_docker_timer[i] == 0) {
			starbase->tsd.starbase.expected_docker[i] = docker->id;
			starbase->tsd.starbase.expected_docker_timer[i] = STARBASE_DOCK_TIME;
			/* transmit granting of docking permission */
			send_comms_packet(starbase, npcname, channel, "%s, PERMISSION TO DOCK GRANTED.", b->shipname);
			snis_queue_add_sound(PERMISSION_TO_DOCK_GRANTED, ROLE_NAVIGATION, b->shipid);
			if (docker->tsd.ship.exterior_lights != 255)
				send_comms_packet(starbase, npcname, channel,
					"%s, ALSO, PLEASE TURN ON YOUR EXTERIOR LIGHTS.", b->shipname);
			return 1;
		}
	}
	if (rate_limit_docking_permission_denied(b)) {
		send_comms_packet(starbase, npcname, channel, "%s, PERMISSION TO DOCK DENIED.", b->shipname);
		snis_queue_add_sound(PERMISSION_TO_DOCK_DENIED, ROLE_NAVIGATION, b->shipid);
	}
	return 0;
}

static int ship_is_docked(uint32_t ship_id, struct snis_entity *starbase)
{
	int i;
	int model = starbase->id % nstarbase_models;

	for (i = 0; i < docking_port_info[model]->nports; i++) {
		struct snis_entity *docking_port;
		int dpi = lookup_by_id(starbase->tsd.starbase.docking_port[i]);
		if (dpi < 0)
			continue;
		docking_port = &go[dpi];
		if (docking_port->tsd.docking_port.docked_guy == ship_id) {
			return 1;
			break;
		}
	}
	return 0;
}

static int starbase_expecting_docker(struct snis_entity *starbase, uint32_t docker)
{
	int model = starbase->id % nstarbase_models;
	int i;

	for (i = 0; i < docking_port_info[model]->nports; i++) {
		if (starbase->tsd.starbase.expected_docker[i] == docker &&
			starbase->tsd.starbase.expected_docker_timer[i] > 0)
			return 1;
	}
	return 0;
}

static void init_player(struct snis_entity *o, int reset_ship, float *charges);
static void do_docking_action(struct snis_entity *ship, struct snis_entity *starbase,
			struct bridge_data *b, char *npcname)
{
	float charges;
	int channel = b->npcbot.channel;

	if (enemy_faction(ship->sdata.faction, starbase->sdata.faction)) {
		send_comms_packet(starbase, npcname, channel,
			"DEPART IMMEDIATELY %s! YOU ARE A SWORN ENEMY OF THE %s.",
			b->shipname, faction_name(starbase->sdata.faction));
	} else {
		if (starbase->sdata.faction == 0) /* Neutral */
			send_comms_packet(starbase, npcname, channel,
				"%s, PERMISSION GRANTED TO ENTER THIS STARBASE. WELCOME.",
				b->shipname);
		else
			send_comms_packet(starbase, npcname, channel,
				"%s, WELCOME TO OUR STARBASE, ENJOY YOUR STAY.", b->shipname);
	}
	send_comms_packet(starbase, npcname, channel,
		"CONTINUOUS AIR SUPPLY ESTABLISH THROUGH DOCKING PORT AIRLOCK.");
	/* TODO make the repair/refuel process a bit less easy */
	send_comms_packet(starbase, npcname, channel,
		"%s, YOUR SHIP HAS BEEN REPAIRED AND REFUELED.\n", b->shipname);
	init_player(ship, 0, &charges);
	ship->timestamp = universe_timestamp;
	snis_queue_add_sound(DOCKING_SOUND, ROLE_ALL, ship->id);
	snis_queue_add_sound(WELCOME_TO_STARBASE, ROLE_NAVIGATION, ship->id);
	send_comms_packet(starbase, npcname, channel,
		"%s, YOUR ACCOUNT HAS BEEN BILLED $%5.2f\n", b->shipname, charges);
	schedule_callback2(event_callback, &callback_schedule,
			"player-docked-event", (double) ship->id, starbase->id);
}

static int player_attempt_warpgate_jump(struct snis_entity *warpgate, struct snis_entity *player)
{
	struct packed_buffer *pb;
	int b = lookup_bridge_by_shipid(player->id);

	if (b < 0 || b >= nbridges) {
		fprintf(stderr, "BUG: %s:%d player ship has no bridge\n", __FILE__, __LINE__);
		return 0;
	}
	if (bridgelist[b].warp_gate_ticket.ipaddr == 0) /* No ticket? No warp. */
		return 0;
	pb = packed_buffer_allocate(3 + 20);
	packed_buffer_append(pb, "bb", OPCODE_SWITCH_SERVER,
		(uint8_t) warpgate->tsd.warpgate.warpgate_number);
	packed_buffer_append_raw(pb, bridgelist[b].warp_gate_ticket.location, 20);
	send_packet_to_all_clients_on_a_bridge(player->id, pb, ROLE_ALL);
	return 1;
}

static void player_attempt_dock_with_starbase(struct snis_entity *docking_port,
						struct snis_entity *player)
{
	char *npcname;
	int channel;
	struct bridge_data *bridge;

	if (docking_port->tsd.docking_port.docked_guy == player->id) /* already docked? */
		return;

	int b = lookup_bridge_by_shipid(player->id);
	if (b < 0 || b >= nbridges) {
		/* player has no bridge??? */
		fprintf(stderr, "BUG: %s:%d player ship has no bridge\n", __FILE__, __LINE__);
		return;
	}
	bridge = &bridgelist[b];
	channel = bridge->npcbot.channel;
	int sb_index = lookup_by_id(docking_port->tsd.docking_port.parent);
	if (sb_index < 0)
		return; /* docking port not connected to anything... shouldn't happen. */
	struct snis_entity *sb = &go[sb_index];

	/* Dock... */
	npcname = sb->sdata.name;
	if (!starbase_expecting_docker(sb, player->id)) {
		if (rate_limit_docking_permission_denied(bridge)) {
			send_comms_packet(sb, npcname, channel, "%s, YOU ARE NOT CLEARED TO DOCK\n",
				bridge->shipname);
			snis_queue_add_sound(PERMISSION_TO_DOCK_DENIED, ROLE_NAVIGATION, bridge->shipid);
		}
		return;
	}
	if (docking_port->tsd.docking_port.docked_guy == -1) {
		docking_port->tsd.docking_port.docked_guy = player->id;
		do_docking_action(player, sb, bridge, npcname);
	} else {
		if (rate_limit_docking_permission_denied(bridge)) {
			send_comms_packet(sb, npcname, channel, "%s, YOU ARE NOT CLEARED FOR DOCKING\n",
				bridge->shipname);
			snis_queue_add_sound(PERMISSION_TO_DOCK_DENIED, ROLE_NAVIGATION, bridge->shipid);
		}
	}
}

static void do_detailed_collision_impulse(struct snis_entity *o1, struct snis_entity *o2, /* objects */
						float m1, float m2, /* masses */
						float r1, float r2, /* radii */
						float energy_transmission_factor)
{
	union vec3 p1, p2, v1, v2, vo1, vo2;

	p1.v.x = o1->x;
	p1.v.y = o1->y;
	p1.v.z = o1->z;

	v1.v.x = o1->vx;
	v1.v.y = o1->vy;
	v1.v.z = o1->vz;

	if (o2->type != OBJTYPE_WARPGATE) {
		p2.v.x = o2->x;
		p2.v.y = o2->y;
		p2.v.z = o2->z;
	} else { /* Warp gate is approximately a torus in y,z plane, major radius ~92, minor radius ~25 */
		union vec3 o1_in_torus_space;
		union vec3 closest_point_on_yz_circle;
		p2.v.x = o2->x;
		p2.v.y = o2->y;
		p2.v.z = o2->z;
		vec3_sub(&o1_in_torus_space, &p1, &p2);
		closest_point_on_yz_circle.v.x = 0.0;
		closest_point_on_yz_circle.v.y = o1_in_torus_space.v.y;
		closest_point_on_yz_circle.v.z = o1_in_torus_space.v.z;
		vec3_normalize_self(&closest_point_on_yz_circle);
		vec3_mul_self(&closest_point_on_yz_circle, WARPGATE_MAJOR_RADIUS);
		vec3_add_self(&p2, &closest_point_on_yz_circle);
	}

	v2.v.x = o2->vx;
	v2.v.y = o2->vy;
	v2.v.z = o2->vz;

	elastic_collision(m1, &p1, &v1, r1, m2, &p2, &v2, r2,
				energy_transmission_factor, &vo1, &vo2);

	o1->vx = vo1.v.x;
	o1->vy = vo1.v.y;
	o1->vz = vo1.v.z;

	o2->vx = vo2.v.x;
	o2->vy = vo2.v.y;
	o2->vz = vo2.v.z;
}

static void do_collision_impulse(struct snis_entity *player, struct snis_entity *object)
{
	do_detailed_collision_impulse(player, object, 10, 20, 10, 10, 0.7);

	/* Give ship some random small rotational velocity */
	const float maxrv = 15.0;
	player->tsd.ship.yaw_velocity = 0.01 * (maxrv * snis_randn(100) - maxrv * 0.5) * M_PI / 180.0;
	player->tsd.ship.pitch_velocity = 0.01 * (maxrv * snis_randn(100) - maxrv * 0.5) * M_PI / 180.0;
	player->tsd.ship.roll_velocity = 0.01 * (maxrv * snis_randn(100) - maxrv * 0.5) * M_PI / 180.0;

	player->tsd.ship.nav_damping_suppression = 1.0;
}

/* Calculate oriented bounding box for a block */
static void block_calculate_obb(struct snis_entity *block, struct oriented_bounding_box *obb)
{
	union vec3 xaxis, yaxis, zaxis;

	if (block->type != OBJTYPE_BLOCK)
		return;

	xaxis.v.x = 1.0;
	xaxis.v.y = 0.0;
	xaxis.v.z = 0.0;

	yaxis.v.x = 0.0;
	yaxis.v.y = 1.0;
	yaxis.v.z = 0.0;

	zaxis.v.x = 0.0;
	zaxis.v.y = 0.0;
	zaxis.v.z = 1.0;

	quat_rot_vec_self(&xaxis, &block->orientation);
	quat_rot_vec_self(&yaxis, &block->orientation);
	quat_rot_vec_self(&zaxis, &block->orientation);

	obb->centerx = block->x;
	obb->centery = block->y;
	obb->centerz = block->z;

	obb->u[0] = xaxis;
	obb->u[1] = yaxis;
	obb->u[2] = zaxis;

	obb->e[0] = block->tsd.block.shape.cuboid.sx * 0.5;
	obb->e[1] = block->tsd.block.shape.cuboid.sy * 0.5;
	obb->e[2] = block->tsd.block.shape.cuboid.sz * 0.5;
}

static void player_collision_detection(void *player, void *object)
{
	struct snis_entity *o, *t;
	double dist2;
	double proximity_dist2, crash_dist2;

	o = player;
	t = object;
	
	switch (t->type) {
	case OBJTYPE_DEBRIS:
	case OBJTYPE_SPARK:
	case OBJTYPE_TORPEDO:
	case OBJTYPE_MISSILE:
	case OBJTYPE_LASER:
	case OBJTYPE_EXPLOSION:
	case OBJTYPE_NEBULA:
	case OBJTYPE_WORMHOLE:
	case OBJTYPE_STARBASE:
	case OBJTYPE_WARP_CORE:
	case OBJTYPE_CHAFF:
		return;
	default:
		break;
	}
	if (t == o) /* skip self */
		return;

	/* Prevent our own mining bot from bumping us. */
	if (t->type == OBJTYPE_SHIP2 && t->tsd.ship.shiptype == SHIP_CLASS_ASTEROIDMINER) {
		int n = t->tsd.ship.nai_entries - 1;
		if (n < MAX_AI_STACK_ENTRIES && n >= 0) {
			if (t->tsd.ship.ai[n].ai_mode == AI_MODE_MINING_BOT &&
				t->tsd.ship.ai[n].u.mining_bot.parent_ship == o->id)
				return;
		}
	}

	/* Prevent tow ship from bumping us */
	if (t->type == OBJTYPE_SHIP2 && t->tsd.ship.shiptype == SHIP_CLASS_MANTIS) {
		return;
	}

	dist2 = object_dist2(o, t);
	if (t->type == OBJTYPE_CARGO_CONTAINER) {
		if (dist2 < 150.0 * 150.0) {
			scoop_up_cargo(o, t);
			return;
		}
		/* Prevent nearby cargo containers from disappearing */
		if (dist2 < SCIENCE_SHORT_RANGE * SCIENCE_SHORT_RANGE)
			t->alive = CARGO_CONTAINER_LIFETIME;
	}
	if (t->type == OBJTYPE_DOCKING_PORT && dist2 < 50.0 * 50.0) {
		if (o->tsd.ship.docking_magnets) {
			player_attempt_dock_with_starbase(t, o);
		} else {
			int bn = lookup_bridge_by_shipid(o->id);
			/* Give docking magnets hint, but not if they just undocked */
			if (bn >= 0 && universe_timestamp > bridgelist[bn].last_docked_time + 600)
				snis_queue_add_text_to_speech(
					"If you are attempting to dock perhaps you should turn on the docking magnets.",
					ROLE_TEXT_TO_SPEECH, o->id);
		}
	}
	if (t->type == OBJTYPE_WARPGATE && dist2 < 110.0 * 110.0) {
		/* Warp gate is approximately a torus with major radius 92, minor radius 25.0. */
		const float minor_radius = WARPGATE_MINOR_RADIUS;
		const float major_radius = WARPGATE_MAJOR_RADIUS;
		const float trigger_dist2 = (major_radius - minor_radius) * (major_radius - minor_radius);
		union vec3 point;
		float dist;
		point.v.x = o->x - t->x;
		point.v.y = o->y - t->y;
		point.v.z = o->z - t->z;
		dist = point_to_torus_dist(&point, major_radius, minor_radius);
		if (dist > 20.0) { /* No collision with warp gate */
			if (dist2 < trigger_dist2) {
				/* Going through the middle of the warp gate */
				player_attempt_warpgate_jump(t, o);
			}
			return;
		} /* else, dist to torus is <= 20.0, collision with warp gate */
	}
	if (t->type == OBJTYPE_PLANET) {
		const float surface_dist2 = t->tsd.planet.radius * t->tsd.planet.radius;
		const float warn_dist2 = (t->tsd.planet.radius + PLAYER_PLANET_DIST_WARN) *
					(t->tsd.planet.radius + PLAYER_PLANET_DIST_WARN);
		const float too_close2 = (t->tsd.planet.radius + PLAYER_PLANET_DIST_TOO_CLOSE) *
					(t->tsd.planet.radius + PLAYER_PLANET_DIST_TOO_CLOSE);
		const float low_alt = (t->tsd.planet.radius + PLAYER_PLANET_LOW_ALT_WARN) *
					(t->tsd.planet.radius + PLAYER_PLANET_LOW_ALT_WARN);

		/* check security level ... */
		if (t->tsd.planet.security > o->tsd.ship.in_secure_area &&
			dist2 < SECURITY_RADIUS * SECURITY_RADIUS)
			o->tsd.ship.in_secure_area = t->tsd.planet.security;

		if (dist2 < surface_dist2 && !player_invincibility)  {
			/* crashed into planet */
			o->alive = 0;
			o->timestamp = universe_timestamp;
			o->respawn_time = universe_timestamp + player_respawn_time * 10;
			send_ship_damage_packet(o);
			snis_queue_add_sound(EXPLOSION_SOUND,
					ROLE_SOUNDSERVER, o->id);
			schedule_callback(event_callback, &callback_schedule,
					"player-death-callback", o->id);
		} else if (dist2 < too_close2 && (universe_timestamp & 0x7) == 0) {
			if (t->tsd.planet.has_atmosphere) {
				(void) add_explosion(o->x + o->vx * 2, o->y + o->vy * 2, o->z + o->vz * 2,
					20, 10, 50, OBJTYPE_SPARK);
				calculate_atmosphere_damage(o);
				send_ship_damage_packet(o);
				send_packet_to_all_clients_on_a_bridge(o->id,
					snis_opcode_pkt("b", OPCODE_ATMOSPHERIC_FRICTION),
						ROLE_SOUNDSERVER | ROLE_NAVIGATION);
			} else if (dist2 < low_alt && (universe_timestamp & 0x1f) == 0) {
				snis_queue_add_text_to_speech("Warning, low altitude.",
					ROLE_TEXT_TO_SPEECH, o->id);
			}
		} else if (dist2 < warn_dist2 && (universe_timestamp & 0x7) == 0 && t->tsd.planet.has_atmosphere) {
			(void) add_explosion(o->x + o->vx * 2, o->y + o->vy * 2, o->z + o->vz * 2,
				5, 5, 50, OBJTYPE_SPARK);
			calculate_atmosphere_damage(o);
			send_ship_damage_packet(o);
			send_packet_to_all_clients_on_a_bridge(o->id,
				snis_opcode_pkt("b", OPCODE_ATMOSPHERIC_FRICTION),
					ROLE_SOUNDSERVER | ROLE_NAVIGATION);
		}

		/* Check for collision with planetary ring */
		if (t->tsd.planet.ring && (universe_timestamp & 0x7) == 0) {
			union vec3 player_center, ring_center;
			union quat orientation;
			float radius, inner, outer;
			const int index = t->tsd.planet.solarsystem_planet_type;

			ring_center.v.x = t->x;
			ring_center.v.y = t->y;
			ring_center.v.z = t->z;

			player_center.v.x = o->x;
			player_center.v.y = o->y;
			player_center.v.z = o->z;

			orientation = random_orientation[t->id % NRANDOM_ORIENTATIONS];
			radius = t->tsd.planet.radius;
			inner = planetary_ring_data[index].inner_radius;
			outer = planetary_ring_data[index].outer_radius;
			if (collides_with_planetary_ring(&player_center, &ring_center, &orientation,
								radius, inner, outer)) {
				(void) add_explosion(o->x + o->vx * 2, o->y + o->vy * 2, o->z + o->vz * 2,
					5, 5, 50, OBJTYPE_SPARK);
				calculate_atmosphere_damage(o);
				send_ship_damage_packet(o);
				snis_queue_add_text_to_speech("high velocity micro-meteorites detected",
						ROLE_TEXT_TO_SPEECH, o->id);
			}
		}
		return;
	}
	if (t->type == OBJTYPE_BLOCK) {
		union vec3 my_ship, t_pos, closest_point, displacement, normal, hit_vel, prev_point;
		float bounce_speed;

		if (dist2 > t->tsd.block.shape.overall_radius * t->tsd.block.shape.overall_radius)
			return;

		my_ship.v.x = o->x;
		my_ship.v.y = o->y;
		my_ship.v.z = o->z;
		t_pos.v.x = t->x;
		t_pos.v.y = t->y;
		t_pos.v.z = t->z;

		dist2 = shape_closest_point(&my_ship, &t_pos, &t->orientation, &t->tsd.block.shape,
						&closest_point, &normal);
		if (t->tsd.block.shape.type == SHAPE_CAPSULE) {
			if (sqrtf(dist2) - t->tsd.block.shape.capsule.radius > 4.0)
				return;
		} else {
			if (dist2 > 8.0 * 8.0)
				return;
		}
		printf("BLOCK COLLISION DETECTION:HIT, dist2 = %f\n", sqrt(dist2));
		snis_queue_add_sound(HULL_CREAK_0 + (snis_randn(1000) % NHULL_CREAK_SOUNDS),
							ROLE_SOUNDSERVER, o->id);
		o->vx = 0;
		o->vy = 0;
		o->vz = 0;

		switch (t->tsd.block.shape.type) {
		case SHAPE_CUBOID:
			displacement.v.x = o->x - closest_point.v.x;
			displacement.v.y = o->y - closest_point.v.y;
			displacement.v.z = o->z - closest_point.v.z;
			vec3_normalize_self(&displacement);
			break;
		case SHAPE_SPHERE:
		case SHAPE_CAPSULE:
			displacement = normal;
			break;
		default:
			displacement.v.x = o->x - t->x;
			displacement.v.y = o->y - t->y;
			displacement.v.z = o->z - t->z;
			vec3_normalize_self(&displacement);
			break;
		}
		/* Find where this closest point was a tick ago */
		find_block_point_last_pos(t, &closest_point, &prev_point);
		/* Find how fast and what dir this point is moving. */
		vec3_sub(&hit_vel, &closest_point, &prev_point);
		vec3_mul_self(&hit_vel, 1.4); /* If the baseball bat is moving, it imparts energy */
		/* Add in our velocity */
		hit_vel.v.x -= o->vx;
		hit_vel.v.y -= o->vy;
		hit_vel.v.z -= o->vz;
		bounce_speed = fabsf(vec3_dot(&hit_vel, &displacement));
		if (bounce_speed < 10.0)
			bounce_speed = 10.0;
		vec3_mul_self(&displacement, bounce_speed);
		o->x += displacement.v.x;
		o->y += displacement.v.y;
		o->z += displacement.v.z;
		o->vx += displacement.v.x * 1.05; /* conservation of energy? Nah. */
		o->vy += displacement.v.y * 1.05;
		o->vz += displacement.v.z * 1.05;
		(void) add_explosion(closest_point.v.x, closest_point.v.y, closest_point.v.z,
					85, 5, 20, OBJTYPE_SPARK);
		return;
	}
	proximity_dist2 = PROXIMITY_DIST2;
	crash_dist2 = CRASH_DIST2;
	if (t->type == OBJTYPE_SPACEMONSTER) {
		if (dist2 < spacemonster_collision_radius * spacemonster_collision_radius) {
			do_collision_impulse(o, t);
			snis_queue_add_sound(SPACEMONSTER_SLAP, ROLE_SOUNDSERVER, o->id);
			/* TODO: some sort of damage to ship */
		}
	}
	if (t->type == OBJTYPE_STARBASE) {
		proximity_dist2 *= STARBASE_SCALE_FACTOR;
		crash_dist2 *= STARBASE_SCALE_FACTOR;
	}
	if (t->type != OBJTYPE_DOCKING_PORT && dist2 < proximity_dist2 && (universe_timestamp & 0x7) == 0) {
		do_collision_impulse(o, t);
		send_packet_to_all_clients_on_a_bridge(o->id, 
			snis_opcode_pkt("b", OPCODE_PROXIMITY_ALERT),
					ROLE_SOUNDSERVER | ROLE_NAVIGATION);
		if (dist2 < crash_dist2) {
			send_packet_to_all_clients_on_a_bridge(o->id, 
				snis_opcode_pkt("b", OPCODE_COLLISION_NOTIFICATION),
					ROLE_SOUNDSERVER | ROLE_NAVIGATION);
		}
	}
}

static void update_player_orientation(struct snis_entity *o)
{
	int i;
	struct snis_entity *planet;
	double v;

	/* This is where "standard orbit" is implemented */
	if (o->tsd.ship.orbiting_object_id != 0xffffffff) {

		/* If we're moving too slowly, numerical problems result */
		v = sqrt(o->vx * o->vx + o->vy * o->vy + o->vz * o->vz);
		if (v < 0.01) {
			o->tsd.ship.orbiting_object_id = 0xffffffff;
			goto skip_standard_orbit;
		}

		i = lookup_by_id(o->tsd.ship.orbiting_object_id);
		if (i < 0) {
			o->tsd.ship.orbiting_object_id = 0xffffffff;
		} else {
			/* Find where we would like to be pointed for "standard orbit"
			 * Take vector P, from planet to us, and make it the standard length.
			 * Take velocity vector V, project to be parallel to surface of sphere,
			 * then scale to some constant fraction of planet radius (2.5 degrees, say).
			 * Add V to P to produce D.  This is where we want to aim.
			 */
			union vec3 p, d, direction, right, up, desired_up;
			union quat new_orientation, up_adjustment, adjusted_orientation;

			planet = &go[i];
			p.v.x = o->x - planet->x;
			p.v.y = o->y - planet->y;
			p.v.z = o->z - planet->z;
			vec3_normalize_self(&p);
			vec3_mul_self(&p, planet->tsd.planet.radius * standard_orbit);
			d.v.x = o->vx;
			d.v.y = o->vy;
			d.v.z = o->vz;
			vec3_add_self(&d, &p);
			vec3_normalize_self(&d);
			vec3_mul_self(&d, planet->tsd.planet.radius * standard_orbit);
			vec3_sub_self(&d, &p);
			vec3_normalize_self(&d);
			vec3_mul_self(&d, planet->tsd.planet.radius * 2.5 / M_PI); /* 2.5 degrees */
			vec3_add_self(&d, &p);
			d.v.x += planet->x;
			d.v.y += planet->y;
			d.v.z += planet->z;

			/* Compute new orientation to point us at d */
			/* Calculate new desired orientation of ship pointing towards destination */
			if (o->tsd.ship.reverse)
				right.v.x = -1.0; /* backwards */
			else
				right.v.x = 1.0;
			right.v.y = 0.0;
			right.v.z = 0.0;

			direction.v.x = d.v.x - o->x;
			direction.v.y = d.v.y - o->y;
			direction.v.z = d.v.z - o->z;
			vec3_normalize_self(&direction);

			quat_from_u2v(&new_orientation, &right, &direction, NULL);

			/* new_orientation is at least pointing in the right direction, but
			 * "up" in that orientation is not necessarily "correct" yet.
			 * We want "up" to be perpendicular to direction, and parallel to
			 * a plane tangent to the sphere at the closest point.
			 */
			vec3_normalize_self(&p);
			vec3_cross(&desired_up, &p, &direction); /* p is vector from planet to player */
			vec3_normalize_self(&desired_up);

			/* Find up in the "new_orientation" */
			up.v.x = 0.0;
			up.v.y = 1.0;
			up.v.z = 0.0;
			quat_rot_vec_self(&up, &new_orientation);

			/* Find the rotation from up in the new orientation to the desired up */
			quat_from_u2v(&up_adjustment, &up, &desired_up, NULL);
			quat_normalize_self(&up_adjustment);

			/* Adjust new orientation to have the desired up direction */
			quat_mul(&adjusted_orientation, &up_adjustment, &new_orientation);
			quat_normalize_self(&adjusted_orientation);

			o->tsd.ship.computer_desired_orientation = adjusted_orientation;
			o->tsd.ship.computer_steering_time_left = 50; /* let the computer steer */
		}
	}
skip_standard_orbit:

	if (o->tsd.ship.computer_steering_time_left > 0) {
		float time = (COMPUTER_STEERING_TIME - o->tsd.ship.computer_steering_time_left) / COMPUTER_STEERING_TIME;
		union quat new_orientation;
		float slerp_power_factor = 0.1 * (float) (o->tsd.ship.power_data.maneuvering.i) / 255.0;

		quat_slerp(&new_orientation, &o->orientation, &o->tsd.ship.computer_desired_orientation,
				time * slerp_power_factor);
		o->orientation = new_orientation;
		o->tsd.ship.computer_steering_time_left--;
	}

	/* Apply current rotational velocities... (player's input comes into play via this path) */
	quat_apply_relative_yaw_pitch_roll(&o->orientation,
			o->tsd.ship.yaw_velocity,
			o->tsd.ship.pitch_velocity,
			o->tsd.ship.roll_velocity);
}

static void update_player_sciball_orientation(struct snis_entity *o)
{
	quat_apply_relative_yaw_pitch_roll(&o->tsd.ship.sciball_orientation,
			o->tsd.ship.sciball_yawvel,
			o->tsd.ship.sciball_pitchvel,
			o->tsd.ship.sciball_rollvel);
}

static void update_high_gain_antenna_orientation(struct snis_entity *o)
{
	/* Normally we would want to recalculate this from the reference orientation
	 * of the ship from scratch and not accumulate rotations like this
	 * lest they eventually get out of sync with the ship, but it is OK because
	 * aim_high_gain_antenna() will refigger it properly immediately after this.
	 */
	quat_apply_relative_yaw_pitch_roll(&o->tsd.ship.current_hg_ant_orientation,
			-o->tsd.ship.yaw_velocity,
			-o->tsd.ship.pitch_velocity,
			-o->tsd.ship.roll_velocity);
}

static void set_player_sciball_orientation(struct snis_entity *o, float x, float y)
{
	quat_apply_relative_yaw_pitch_roll(&o->tsd.ship.sciball_orientation,
			(double) x,
			(double) y,
			(double) 0);
}

static void update_player_weap_orientation(struct snis_entity *o)
{
	quat_apply_relative_yaw_pitch(&o->tsd.ship.weap_orientation,
			o->tsd.ship.weap_yawvel,
			o->tsd.ship.weap_pitchvel);

	/* turret is on top of the ship */
	union vec3 aim = {{0,1,0}};
	union quat swing, twist;
	quat_decompose_swing_twist(&o->tsd.ship.weap_orientation, &aim, &swing, &twist);

	union vec3 swing_axis;
	float swing_angle;
	quat_to_axis_v(&swing, &swing_axis, &swing_angle);
#if 0
	union vec3 twist_axis;
	float twist_angle;
	quat_to_axis_v(&twist, &twist_axis, &twist_angle);

	printf("----\n");
	printf("twist w=%f x=%f y=%f z=%f\n", twist.v.w, twist.v.x, twist.v.y, twist.v.z);
	printf("twist axis a=%f x=%f, y=%f, z=%f\n", radians_to_degrees(twist_angle),twist_axis.v.x, twist_axis.v.y, twist_axis.v.z);
	printf("swing w=%f x=%f y=%f z=%f\n", swing.v.w, swing.v.x, swing.v.y, swing.v.z);
	printf("swing axis a=%f x=%f, y=%f, z=%f\n", radians_to_degrees(swing_angle),swing_axis.v.x, swing_axis.v.y, swing_axis.v.z);
#endif
	/* need to use dot product to determine if the swing angle is toward +y or -y */
	union vec3 dir = {{1,0,0}};
	quat_rot_vec_self(&dir, &o->tsd.ship.weap_orientation);
	float dot = vec3_dot(&aim, &dir);

	if (dot<0) {
		/* we are <0 from x axis, so that is >90 from y */

		/* remove swing which means align with xz plane */
		o->tsd.ship.weap_orientation = twist;
		quat_normalize_self(&o->tsd.ship.weap_orientation);
	}
	else if (swing_angle > M_PI/2.0) {
		/* we are >90 from x axis so that is past vertical on y */

		/* force the swing to 90 and recombine with the twist */
		quat_init_axis_v(&swing, &swing_axis, M_PI/2.0);
		quat_mul(&o->tsd.ship.weap_orientation, &swing, &twist);
		quat_normalize_self(&o->tsd.ship.weap_orientation);
	}
}

static float new_velocity(float desired_velocity, float current_velocity)
{
	float delta;

	delta = desired_velocity - current_velocity;
	if (fabs(delta) < 0.05)
		return desired_velocity;
	return current_velocity + (delta / 20.0);
}

/* If an NPC ship wants to do a 180, or some other drastic direction change,
 * we don't want to just gradually shift the velocity towards the new velocity,
 * because this will result in strange things like the ship slowly coming to a halt,
 * very rapidly doing a 180, then slowly accelerating. Instead, we want it to
 * execute a nice turn. So we limit the desired change of angle to no more than
 * 45 degrees by just capping such desires.
 */
static void temper_velocity_change(struct snis_entity *o, union vec3 *desired_velocity)
{
	union vec3 current_direction, dv, cross, newv1, newv2;
	union quat rotation1, rotation2;
	float dot, d1, d2;

	if (vec3_magnitude(desired_velocity) < 0.01)
		return;

	/* Use orientation rather than velocity, as velocity might be close to zero. */
	current_direction.v.x = 1.0;
	current_direction.v.y = 0.0;
	current_direction.v.z = 0.0;
	quat_rot_vec_self(&current_direction, &o->orientation);

	vec3_normalize_self(&current_direction);
	vec3_normalize(&dv, desired_velocity);
	dot = vec3_dot(&current_direction, &dv);
	if (dot >= cos(45.0 * M_PI / 180.0)) /* Angle of turn is less than +/- 45 degrees */
		return;
	/* Angle of turn is more than +/- 45 degrees */
	/* Find a vector perpendicular to current and desired velocities */
	vec3_cross(&cross, &current_direction, &dv);
	if (vec3_magnitude(&cross) < 0.001) /* Avoid NaNs when normalizing */
		return;
	vec3_normalize_self(&cross);
	/* Try plus and minus 45 degrees */
	quat_init_axis_v(&rotation1, &cross, 45.0 * M_PI / 180.0);
	quat_init_axis_v(&rotation2, &cross, -45.0 * M_PI / 180.0);
	quat_rot_vec(&newv1, &current_direction, &rotation1);
	quat_rot_vec(&newv2, &current_direction, &rotation2);
	/* Find the cos(angles) between our +/- 45 degree vectors and desired velocity */
	d1 = vec3_dot(&newv1, desired_velocity);
	d2 = vec3_dot(&newv2, desired_velocity);
	/* Figure out which one is the smallest angle */
	if (d1 > d2) { /* d1 is the larger cos == smaller angle */
		vec3_normalize_self(&newv1);
		vec3_mul_self(&newv1, vec3_magnitude(desired_velocity));
		*desired_velocity = newv1;
	} else { /* d2 is the larger cos == smaller angle */
		vec3_normalize_self(&newv1);
		vec3_mul_self(&newv1, vec3_magnitude(desired_velocity));
		*desired_velocity = newv2;
	}
}

static void update_ship_position_and_velocity(struct snis_entity *o)
{
	union vec3 desired_velocity;
	union vec3 dest, destn;
	struct snis_entity *v;
	int n, mode;

	/* Adjust velocity towards desired velocity */
	o->tsd.ship.velocity = o->tsd.ship.velocity +
			(o->tsd.ship.desired_velocity - o->tsd.ship.velocity) * 0.1;
	if (fabs(o->tsd.ship.velocity - o->tsd.ship.desired_velocity) < 0.1)
		o->tsd.ship.velocity = o->tsd.ship.desired_velocity;
	ai_trace(o->id, "V = %f, DV = %f", o->tsd.ship.velocity, o->tsd.ship.desired_velocity);

	if (o->tsd.ship.nai_entries == 0)
		ship_figure_out_what_to_do(o);
	n = o->tsd.ship.nai_entries - 1;
	mode = o->tsd.ship.ai[n].ai_mode;

	if (mode == AI_MODE_CATATONIC) {
		o->vx = 0;
		o->vy = 0;
		o->vz = 0;
		o->tsd.ship.desired_velocity = 0;
		return;
	}

	/* FIXME: need a better way to do this. */
	if (mode == AI_MODE_ATTACK) {
		v = lookup_entity_by_id(o->tsd.ship.ai[n].u.attack.victim_id);
		if (v) {
			dest.v.x = v->x + o->tsd.ship.dox - o->x;
			dest.v.y = v->y + o->tsd.ship.doy - o->y;
			dest.v.z = v->z + o->tsd.ship.doz - o->z;
		} else {
			/* victim must have died. */
			pop_ai_attack_mode(o);
			return;
		}
	} else {
		dest.v.x = o->tsd.ship.dox - o->x;
		dest.v.y = o->tsd.ship.doy - o->y;
		dest.v.z = o->tsd.ship.doz - o->z;
		if (vec3_magnitude(&dest) < 0.1) {
			dest.v.x = 0.05;
			dest.v.y = 0.0;
			dest.v.z = 0.0;
			quat_rot_vec_self(&dest, &o->orientation);
		}
	}

	o->tsd.ship.dist = vec3_magnitude(&dest);
	ai_trace(o->id, "DIST TO DEST = %f", o->tsd.ship.dist);

#if 1
	vec3_normalize(&destn, &dest);
#else
	if (o->tsd.ship.dist > 100.0) { /* If this is too tight, ships wiggle around too much */
		vec3_normalize(&destn, &dest);
	} else {
		destn.v.x = 0.0f;
		destn.v.y = 0.0f;
		destn.v.z = 0.0f;
		o->tsd.ship.desired_velocity = 0;
		ai_trace(o->id, "DV = 0 (ARRIVAL)");
	}
#endif

	/* Construct vector of desired velocity */
	desired_velocity.v.x = destn.v.x * o->tsd.ship.desired_velocity;
	desired_velocity.v.y = destn.v.y * o->tsd.ship.desired_velocity;
	desired_velocity.v.z = destn.v.z * o->tsd.ship.desired_velocity;
	ai_trace(o->id, "DV XYZ = %f, %f, %f", desired_velocity.v.x, desired_velocity.v.y, desired_velocity.v.z);

	/* apply braking and steering adjustments */
	vec3_mul_self(&desired_velocity, o->tsd.ship.braking_factor);
	ai_trace(o->id, "AFTER BRAKE DV = %f, %f, %f",
			desired_velocity.v.x, desired_velocity.v.y, desired_velocity.v.z);
	vec3_add_self(&desired_velocity, &o->tsd.ship.steering_adjustment);
	ai_trace(o->id, "AFTER STEER DV = %f, %f, %f",
			desired_velocity.v.x, desired_velocity.v.y, desired_velocity.v.z);

	/* Make the ship execute nice turns instead of stopping and quickly reversing */
	temper_velocity_change(o, &desired_velocity);
	ai_trace(o->id, "AFTER TEMPER DV = %f, %f, %f",
			desired_velocity.v.x, desired_velocity.v.y, desired_velocity.v.z);

	union vec3 curvel = { { o->vx, o->vy, o->vz } };
	float dot = vec3_dot(&curvel, &desired_velocity);
	if (o->tsd.ship.velocity < MINIMUM_TURN_SPEED &&
		dot < cos(M_PI * MAX_SLOW_TURN_ANGLE / 180.0 && mode != AI_MODE_HANGOUT)) {
		/* Currently moving too slow and trying to turn too much, just go straight */
		union vec3 rightvec;
		rightvec.v.x = MINIMUM_TURN_SPEED * 1.05;
		rightvec.v.y = 0.0;
		rightvec.v.z = 0.0;
		quat_rot_vec(&desired_velocity, &rightvec, &o->orientation);
	}
	o->vx = new_velocity(desired_velocity.v.x, o->vx);
	o->vy = new_velocity(desired_velocity.v.y, o->vy);
	o->vz = new_velocity(desired_velocity.v.z, o->vz);

	ai_trace(o->id, "NEW VEL = %f, %f, %f", o->vx, o->vy, o->vz);

	/* Move ship */
	set_object_location(o, o->x + o->vx, o->y + o->vy, o->z + o->vz);
	/* FIXME: some kind of collision detection needed here... */

	if (mode == AI_MODE_HANGOUT ||
		(rts_mode && o->tsd.ship.fuel == 0)) {
		o->vx *= 0.8;
		o->vy *= 0.8;
		o->vz *= 0.8;
		o->tsd.ship.desired_velocity = 0;
		ai_trace(o->id, "HANGOUT BRAKING");
	}
}

static void update_player_position_and_velocity(struct snis_entity *o)
{
	union vec3 desired_velocity;
	uint8_t previous_security;

	/* Apply player's thrust input from power model */
	do_thrust(o);

	/* Construct vector of desired velocity */
	desired_velocity.v.x = o->tsd.ship.velocity;
	desired_velocity.v.y = 0;
	desired_velocity.v.z = 0;
	quat_rot_vec_self(&desired_velocity, &o->orientation);

	/* Make actual velocity move towards desired velocity */
	o->vx = new_velocity(desired_velocity.v.x, o->vx);
	o->vy = new_velocity(desired_velocity.v.y, o->vy);
	o->vz = new_velocity(desired_velocity.v.z, o->vz);

	/* Move ship */
	set_object_location(o, o->x + o->vx, o->y + o->vy, o->z + o->vz);

	previous_security = o->tsd.ship.in_secure_area;
	o->tsd.ship.in_secure_area = 0;  /* player_collision_detection fills this in. */
	space_partition_process(space_partition, o, o->x, o->z, o,
				player_collision_detection);
	if (o->tsd.ship.in_secure_area == 0 && previous_security > 0) {
		/* snis_queue_add_sound(LEAVING_SECURE_AREA, ROLE_SOUNDSERVER, o->id); */
		snis_queue_add_text_to_speech("Leaving high security area.", ROLE_TEXT_TO_SPEECH, o->id);
	}
	if (o->tsd.ship.in_secure_area > 0 && previous_security == 0) {
		/* snis_queue_add_sound(ENTERING_SECURE_AREA, ROLE_SOUNDSERVER, o->id); */
		snis_queue_add_text_to_speech("Entering high security area.", ROLE_TEXT_TO_SPEECH, o->id);
	}
}

static int calc_sunburn_damage(struct snis_entity *o, struct damcon_data *d,
				uint8_t *system, uint8_t sunburn)
{
	float damage, old_value, new_value;
	int system_number = system - (unsigned char *) &o->tsd.ship.damage;

	if (player_invincibility)
		return 0;

	BUILD_ASSERT(sizeof(o->tsd.ship.damage) == DAMCON_SYSTEM_COUNT - 1);
	if (system_number < 0 || system_number >= DAMCON_SYSTEM_COUNT - 1)
		fprintf(stderr, "system_number out of range: %d at %s:%d\n",
			system_number, __FILE__, __LINE__);
	if (snis_randn(1000) > 10) 
		return 0; /* lucky, no damage */

	damage = (float) snis_randn(3) / 100.0f;
	damage *= sunburn; 
	old_value = (float) *system;
	new_value = old_value + 255.0f * damage;
	if (new_value > 255.0f)
		new_value = 255.0f;
	*system = (uint8_t) new_value;
	damage = (new_value - old_value);
	distribute_damage_to_damcon_system_parts(o, d, (int) damage, system_number);
	return 1;
}

static int do_sunburn_damage(struct snis_entity *o)
{
	float sundist, sunburn;
	int damage_was_done = 0;
	struct damcon_data *d;
	int b;

	sundist = dist3d(o->x - SUNX, o->y - SUNY, o->z - SUNZ);
	if (sundist > SUN_DIST_LIMIT) {
		return 0;
	} else {
		sunburn = 1.0 - sundist / SUN_DIST_LIMIT;
		if ((universe_timestamp % (50) == 0))
			snis_queue_add_sound(DANGEROUS_RADIATION, ROLE_SOUNDSERVER, o->id);
	}

	b = lookup_bridge_by_shipid(o->id);
	d = &bridgelist[b].damcon;
	
	damage_was_done += calc_sunburn_damage(o, d, &o->tsd.ship.damage.sensors_damage,
			(uint8_t) (sunburn * 255.0));
	damage_was_done += calc_sunburn_damage(o, d, &o->tsd.ship.damage.shield_damage,
			(uint8_t) (sunburn * 255.0));
	damage_was_done += calc_sunburn_damage(o, d, &o->tsd.ship.damage.comms_damage,
			(uint8_t) (sunburn * 255.0));
	damage_was_done += calc_sunburn_damage(o, d, &o->tsd.ship.damage.warp_damage,
			(uint8_t) (sunburn * 255.0));

	/* kill the player if they are too close to the sun for too long */
	if (damage_was_done && o->tsd.ship.damage.shield_damage == 255) {
		o->alive = 0;
		o->tsd.ship.damage.shield_damage = 255;
		o->timestamp = universe_timestamp;
		o->respawn_time = universe_timestamp + player_respawn_time * 10;
		snis_queue_add_sound(EXPLOSION_SOUND,
				ROLE_SOUNDSERVER, o->id);
		schedule_callback(event_callback, &callback_schedule,
			"player-death-callback", o->id);
	}
	return damage_was_done;
}

static void maybe_do_player_warp(struct snis_entity *o)
{
	int b = lookup_bridge_by_shipid(o->id);
	if (b < 0)
		return;

	if (o->tsd.ship.warp_time >= 0)
		o->tsd.ship.warp_time--;

	if (o->tsd.ship.warp_time == 0) { /* Is it time to engage warp? */
		/* 5 seconds of warp limbo */
		send_packet_to_all_clients_on_a_bridge(o->id,
			snis_opcode_pkt("bh", OPCODE_WARP_LIMBO,
				(uint16_t) (5 * 30)), ROLE_ALL);
		bridgelist[b].warpv.v.x = (bridgelist[b].warpx - o->x) / 50.0;
		bridgelist[b].warpv.v.y = (bridgelist[b].warpy - o->y) / 50.0;
		bridgelist[b].warpv.v.z = (bridgelist[b].warpz - o->z) / 50.0;
		bridgelist[b].warptimeleft = 50;
		add_warp_effect(o->id, o->x, o->y, o->z,
				bridgelist[b].warpx,
				bridgelist[b].warpy,
				bridgelist[b].warpz);
		/* At this point, warp[xyz] holds coordinates we are warping FROM */
		bridgelist[b].warpx = o->x;
		bridgelist[b].warpy = o->y;
		bridgelist[b].warpz = o->z;
	}

	if (bridgelist[b].warptimeleft <= 0)
		return;

	float angle = (2.0 * M_PI / 50.0) * (float) bridgelist[b].warptimeleft;

	o->vx = bridgelist[b].warpv.v.x * (1.0 - cos(angle));
	o->vy = bridgelist[b].warpv.v.y * (1.0 - cos(angle));
	o->vz = bridgelist[b].warpv.v.z * (1.0 - cos(angle));
	o->timestamp = universe_timestamp;
	bridgelist[b].warptimeleft--;
	if (bridgelist[b].warptimeleft == 0) {
		double dist_travelled = hypot3d(bridgelist[b].warpx - o->x,
						bridgelist[b].warpy - o->y,
						bridgelist[b].warpz - o->z);
		double warp_factor = 10.0 * dist_travelled / (XKNOWN_DIM / 2.0);
		o->vx = 0;
		o->vy = 0;
		o->vz = 0;
		schedule_callback8(event_callback, &callback_schedule,
				"player-warp-travel-event", (double) o->id,
					bridgelist[b].warpx,
					bridgelist[b].warpy,
					bridgelist[b].warpz,
					o->x, o->y, o->z, warp_factor);
	}
}

static void maybe_transmit_attached_science_text(int bridge, struct snis_entity *ship,
	struct snis_entity *sci_selection)
{
	static int timer = 0;
	struct packed_buffer *pb;
	uint16_t len;

	if (!sci_selection->sdata.science_text) /* Nothing to send */
		return;
	timer++;
	if ((timer & 0x07) != 7) /* Throttle this transmission */
		return;
	len = strnlen(sci_selection->sdata.science_text, 256);
	pb = packed_buffer_allocate(len + 7);
	packed_buffer_append(pb, "bwhr", OPCODE_UPDATE_SCI_TEXT,
				sci_selection->id, len, sci_selection->sdata.science_text, len);
	send_packet_to_all_clients_on_a_bridge(ship->id, pb, ROLE_SCIENCE);
}

static void maybe_transmit_custom_planet_description(int bridge, struct snis_entity *ship,
				struct snis_entity *planet)
{
	struct packed_buffer *pb;
	uint16_t len;
	static int timer = 0;

	if (planet->type != OBJTYPE_PLANET)
		return;
	if (!planet->tsd.planet.custom_description)
		return;
	timer++;
	if ((timer & 0x7) != 7) /* Throttle this transmission */
		return;

	len = strnlen(planet->tsd.planet.custom_description, 256);
	pb = packed_buffer_allocate(len + 7);
	packed_buffer_append(pb, "bwhr", OPCODE_UPDATE_PLANET_DESCRIPTION,
				planet->id, len, planet->tsd.planet.custom_description, len);
	send_packet_to_all_clients_on_a_bridge(ship->id, pb, ROLE_SCIENCE);
}

/* If the selected object is too far away and is not a planet, starbase, or waypoint,
 * deselect it. Also maybe transmit attached science text.
 */
static void check_science_selection(struct snis_entity *o)
{
	int bn = lookup_bridge_by_shipid(o->id);
	uint32_t id;
	int i;
	float dist2, range2;

	id = bridgelist[bn].science_selection;
	if (id == (uint32_t) -1)
		return; /* Nothing selected */
	if (bridgelist[bn].selected_waypoint != -1)
		return; /* Don't deselect waypoints */
	i = lookup_by_id(id);
	if (i < 0)
		goto deselect;
	if (!go[i].alive)
		goto deselect;
	dist2 = object_dist2(o, &go[i]);
	range2 = o->tsd.ship.scibeam_range * o->tsd.ship.scibeam_range;
	maybe_transmit_attached_science_text(bn, o, &go[i]);
	if (go[i].type == OBJTYPE_PLANET)
		maybe_transmit_custom_planet_description(bn, o, &go[i]);
	if (dist2 <= range2)
		return;
	if ((go[i].type == OBJTYPE_PLANET || go[i].type == OBJTYPE_STARBASE) &&
		dist2 <= range2 * 4.0 * 4.0)
		return;
deselect:
	bridgelist[bn].science_selection = (uint32_t) -1;
	send_packet_to_all_clients_on_a_bridge(o->id, snis_opcode_pkt("bbw", OPCODE_SCI_SELECT_TARGET,
		OPCODE_SCI_SELECT_TARGET_TYPE_OBJECT, (uint32_t) -1), ROLE_SCIENCE | ROLE_NAVIGATION | ROLE_WEAPONS);
}

static void aim_high_gain_antenna(struct snis_entity *o)
{
	union vec3 straight_ahead = { { 1.0, 0.0, 0.0 } };
	union vec3 aim_point = o->tsd.ship.desired_hg_ant_aim;
	union quat new_orientation;
	float d;

	if (!enable_comms_attenuation) {
		/* This is a hack to let the client know that antenna is disabled so
		 * it knows whether to draw the arrows on the nav screen.
		 */
		o->tsd.ship.desired_hg_ant_aim.v.x = 0.0;
		o->tsd.ship.desired_hg_ant_aim.v.y = 0.0;
		o->tsd.ship.desired_hg_ant_aim.v.z = 0.0;
		return;
	}
	/* Figure what "straight ahead" means in the ship's orientation */
	quat_rot_vec_self(&straight_ahead, &o->orientation);

	if (vec3_magnitude(&o->tsd.ship.desired_hg_ant_aim) < 0.1) {
		/* If we get here it means that previously enable_comms_attenuation was zero (disabled)
		 * but now it has been enabled.
		 */
		o->tsd.ship.desired_hg_ant_aim = straight_ahead;
	}

	vec3_normalize_self(&aim_point);
	/* Figure if the aim point is more or less behind the ship */
	d = vec3_dot(&aim_point, &straight_ahead);
	if (d < 0.0) { /* aim point is behind ship */
		/* Subtract out the backwards portion of the aim point */
		vec3_mul_self(&straight_ahead, -d);
		vec3_add_self(&aim_point, &straight_ahead); /* */
		vec3_normalize_self(&aim_point);
	}
	/* Figure what the new orientation should be */
	straight_ahead.v.x = 1.0;
	straight_ahead.v.y = 0.0;
	straight_ahead.v.z = 0.0;
	quat_from_u2v(&new_orientation, &straight_ahead, &aim_point, NULL);
	quat_normalize_self(&new_orientation);
	if (d < 0.0) { /* hard limit, antenna physically cannot point backwards */
		o->tsd.ship.current_hg_ant_orientation = new_orientation;
	} else {
		/* TODO: would be nice to rate limit the antenna movement to something
		 * less than the ship's rotation rate. However, doing so is a bit tricky,
		 * akin to the turret movement code. Here we just snap the antenna to the
		 * new orientation.
		 */
		o->tsd.ship.current_hg_ant_orientation = new_orientation;
	}
}

static void update_chaff_cooldown_timer(struct snis_entity *o)
{
	/* Decrement chaff cooldown timer */
	int bn = lookup_bridge_by_shipid(o->id);
	if (bn >= 0 && bridgelist[bn].chaff_cooldown > 0)
		bridgelist[bn].chaff_cooldown--;
}

static void player_move(struct snis_entity *o)
{
	int i, desired_rpm, desired_temp, diff;
	int phaser_chargerate, current_phaserbank;
	float orientation_damping, velocity_damping;

	if (o->tsd.ship.damage.shield_damage > 100) {
		if (o->tsd.ship.damage.shield_damage > 200) {
			if (universe_timestamp % (10 * 5) == 0)
				snis_queue_add_sound(HULL_BREACH_IMMINENT, ROLE_SOUNDSERVER, o->id);
		}
		if (snis_randn(8000) < 100 + o->tsd.ship.damage.shield_damage)
			snis_queue_add_sound(HULL_CREAK_0 + (snis_randn(1000) % NHULL_CREAK_SOUNDS),
							ROLE_SOUNDSERVER, o->id);
	}
	if (o->tsd.ship.fuel < FUEL_CONSUMPTION_UNIT * 30 * 60 &&
		(universe_timestamp % (20 * 5)) == 15) {
		snis_queue_add_sound(FUEL_LEVELS_CRITICAL, ROLE_SOUNDSERVER, o->id);

		/* auto-refuel in safe mode */
		if (safe_mode) {
			o->tsd.ship.fuel = UINT32_MAX;
			o->tsd.ship.oxygen = UINT32_MAX;
		}
	}
	do_power_model_computations(o);
	do_coolant_model_computations(o);
	do_temperature_computations(o);
	o->tsd.ship.damage_data_dirty = do_overheating_damage(o);
	o->tsd.ship.damage_data_dirty |= do_sunburn_damage(o);
	if (o->tsd.ship.fuel > FUEL_CONSUMPTION_UNIT) {
		power_model_enable(o->tsd.ship.power_model);
		o->tsd.ship.fuel -= (int) (FUEL_CONSUMPTION_UNIT *
			power_model_total_current(o->tsd.ship.power_model) / MAX_CURRENT);
	} else {
		power_model_disable(o->tsd.ship.power_model);
	}
	if (o->tsd.ship.oxygen > OXYGEN_CONSUMPTION_UNIT)
		o->tsd.ship.oxygen -= OXYGEN_CONSUMPTION_UNIT; /* Consume oxygen */
	else
		o->tsd.ship.oxygen = 0;
	/* Generate oxygen */
	uint32_t new_oxygen = (uint32_t) (OXYGEN_PRODUCTION_UNIT *
				o->tsd.ship.power_data.lifesupport.i / 255.0);
	/* Acquire oxygen from friendly starbase when docked */
	for (i = 0; i <= snis_object_pool_highest_object(pool); i++) {
		if (go[i].type == OBJTYPE_STARBASE && ship_is_docked(o->id, &go[i])
				&& !enemy_faction(o->sdata.faction, go[i].sdata.faction)) {
			new_oxygen += OXYGEN_CONSUMPTION_UNIT * 1.5;
		}
	}
	if (o->tsd.ship.oxygen < UINT32_MAX - new_oxygen)
		o->tsd.ship.oxygen += new_oxygen;
	else
		o->tsd.ship.oxygen = UINT32_MAX;
	update_player_orientation(o);
	update_player_sciball_orientation(o);
	update_player_weap_orientation(o);
	update_high_gain_antenna_orientation(o);	/* apply ship rotation to antenna */
	aim_high_gain_antenna(o);			/* try to aim antenna at desired comms target */
	update_player_position_and_velocity(o);
	
	o->tsd.ship.sci_heading += o->tsd.ship.sci_yaw_velocity;

	normalize_angle(&o->tsd.ship.sci_heading);
	o->timestamp = universe_timestamp;

	orientation_damping = PLAYER_ORIENTATION_DAMPING +
			(1.0 - o->tsd.ship.power_data.maneuvering.i / 255.0) *
				(0.98 - PLAYER_ORIENTATION_DAMPING) +
			(0.98 - PLAYER_ORIENTATION_DAMPING) * o->tsd.ship.nav_damping_suppression;
	if (orientation_damping > 0.998)
		orientation_damping = 0.998;
	if (orientation_damping < 0.85)
		orientation_damping = 0.85;
	damp_yaw_velocity(&o->tsd.ship.yaw_velocity, orientation_damping);
	damp_yaw_velocity(&o->tsd.ship.pitch_velocity, orientation_damping);
	damp_yaw_velocity(&o->tsd.ship.roll_velocity, orientation_damping);
	damp_yaw_velocity(&o->tsd.ship.sci_yaw_velocity, SCI_YAW_DAMPING);
	damp_yaw_velocity(&o->tsd.ship.sciball_yawvel, YAW_DAMPING);
	damp_yaw_velocity(&o->tsd.ship.sciball_pitchvel, PITCH_DAMPING);
	damp_yaw_velocity(&o->tsd.ship.sciball_rollvel, ROLL_DAMPING);
	damp_yaw_velocity(&o->tsd.ship.weap_yawvel, YAW_DAMPING);
	damp_yaw_velocity(&o->tsd.ship.weap_pitchvel, PITCH_DAMPING);

	/* Damp velocity */
	velocity_damping = player_velocity_damping +
		(1.0 - player_velocity_damping) * o->tsd.ship.nav_damping_suppression;

	if (fabs(o->tsd.ship.velocity) < MIN_PLAYER_VELOCITY &&
			o->tsd.ship.nav_damping_suppression < 0.05)
		o->tsd.ship.velocity = 0.0;
	else
		o->tsd.ship.velocity *= velocity_damping;

	/* decay the damping suppression over time */
	o->tsd.ship.nav_damping_suppression *= DAMPING_SUPPRESSION_DECAY;

	/* check to see if any torpedoes have finished loading */
	if (o->tsd.ship.torpedo_load_time > 0) {
		o->tsd.ship.torpedo_load_time--;
		if (o->tsd.ship.torpedo_load_time == 0) {
			o->tsd.ship.torpedoes_loaded++;
			o->tsd.ship.torpedoes_loading = 0;
		}
	}
	/* calculate fuel/power/rpm, etc. */
	desired_rpm = o->tsd.ship.throttle;
	if (o->tsd.ship.rpm < desired_rpm) {
		diff = desired_rpm - o->tsd.ship.rpm;
		if (diff > 10) {
			diff = 10;
		} else {
			diff = diff / 2;
			if (diff <= 0)
				diff = 1;
		}
		o->tsd.ship.rpm += diff;
	} else {
		if (o->tsd.ship.rpm > desired_rpm) {
			diff = o->tsd.ship.rpm - desired_rpm;
			if (diff > 10) {
				diff = 10;
			} else {
				diff = diff / 2;
				if (diff <= 0)
					diff = 1;
			}
			o->tsd.ship.rpm -= diff;
		}
	}
	o->tsd.ship.power = table_interp((double) o->tsd.ship.rpm,
			rpmx, powery, ARRAYSIZE(rpmx));
	desired_temp = (uint8_t) table_interp((double) o->tsd.ship.rpm,
			rpmx, tempy, ARRAYSIZE(rpmx));
	if (snis_randn(100) < 50) { /* adjust temp slowly, stochastically */
		diff = 0;
		if (o->tsd.ship.temp < desired_temp) {
			diff = 1;
		} else {
			if (o->tsd.ship.temp > desired_temp) {
				diff = -1;
			}
		}
		o->tsd.ship.temp += diff;
	}
	o->tsd.ship.power *= table_interp((double) o->tsd.ship.temp,
			rpmx, powertempy, ARRAYSIZE(powertempy));

	/* Update shield strength */
	if (o->sdata.shield_strength < o->tsd.ship.power_data.shields.i)
		o->sdata.shield_strength++;
	if (o->sdata.shield_strength > o->tsd.ship.power_data.shields.i)
		o->sdata.shield_strength--;
	if (o->sdata.shield_strength > (255 - o->tsd.ship.damage.shield_damage))
		o->sdata.shield_strength = 255 - o->tsd.ship.damage.shield_damage;
	player_update_shield_wavelength_width_depth(o);

	/* Update warp drive */
	if (o->tsd.ship.warpdrive < o->tsd.ship.power_data.warp.i)
		o->tsd.ship.warpdrive++;
	if (o->tsd.ship.warpdrive > o->tsd.ship.power_data.warp.i)
		o->tsd.ship.warpdrive--;

	/* Update phaser charge */
	/* TODO: change this so phaser power affects charge rate, damage affects max. */
	current_phaserbank = o->tsd.ship.phaser_charge;
	phaser_chargerate = o->tsd.ship.power_data.phasers.i;
	o->tsd.ship.phaser_charge = update_phaser_banks(current_phaserbank, phaser_chargerate, 255);
	maybe_do_player_warp(o);
	calculate_ship_scibeam_info(o);

	/* Check if crew has asphixiated */
	if (o->tsd.ship.oxygen == 0) {
		o->alive = 0;
		o->timestamp = universe_timestamp;
		o->respawn_time = universe_timestamp + player_respawn_time * 10;
		send_ship_damage_packet(o);
	}

	/* Update player wallet */
	if (rts_mode) { /* count how many starbases the player controls */
		int starbase_count = 0;
		for (i = 0; i <= snis_object_pool_highest_object(pool); i++) {
			if (go[i].type != OBJTYPE_STARBASE)
				continue;
			if (!go[i].alive)
				continue;
			if (go[i].tsd.starbase.occupant[3] == o->sdata.faction)
				starbase_count++;
		}
		o->tsd.ship.wallet += starbase_count * RTS_WALLET_REFRESH_PER_BASE_PER_TICK +
					RTS_WALLET_REFRESH_MINIMUM;
	}
	check_science_selection(o);

	update_chaff_cooldown_timer(o);

	/* Missiles will set this to 10, here we decrement, and if no missiles are around, it will hit zero soon. */
	if (o->tsd.ship.missile_lock_detected > 0)
		o->tsd.ship.missile_lock_detected--;
}

static void demon_ship_move(struct snis_entity *o)
{
	union vec3 right = { { 1.0, 0.0, 0.0 } };

	quat_rot_vec_self(&right, &o->orientation);
	o->vx = o->tsd.ship.velocity * right.v.x;
	o->vy = o->tsd.ship.velocity * right.v.y;
	o->vz = o->tsd.ship.velocity * right.v.z;
	set_object_location(o, o->x + o->vx, o->y + o->vy, o->z + o->vz);
	quat_apply_relative_yaw_pitch_roll(&o->orientation,
			o->tsd.ship.yaw_velocity,
			o->tsd.ship.pitch_velocity,
			o->tsd.ship.roll_velocity);

	o->timestamp = universe_timestamp;

	damp_yaw_velocity(&o->tsd.ship.yaw_velocity, YAW_DAMPING);
	damp_yaw_velocity(&o->tsd.ship.pitch_velocity, YAW_DAMPING);
	damp_yaw_velocity(&o->tsd.ship.roll_velocity, YAW_DAMPING);
	damp_yaw_velocity(&o->tsd.ship.sci_yaw_velocity, SCI_YAW_DAMPING);

	/* Damp velocity */
	if (fabs(o->tsd.ship.velocity) < MIN_PLAYER_VELOCITY)
		o->tsd.ship.velocity = 0.0;
	else
		o->tsd.ship.velocity *= player_velocity_damping;
}

static void nebula_move(struct snis_entity *o)
{
	return;
}

static void spin_starbase(struct snis_entity *o)
{
	/* ticks per sec * 10ths degree per circle / 10ths_deg_per_sec = ticks per circle */
	int ticks_per_circle = 10 * 3600 / o->tsd.starbase.spin_rate_10ths_deg_per_sec;
	float a = M_PI * 2.0 * (universe_timestamp % ticks_per_circle) / (float) ticks_per_circle;
	quat_init_axis(&o->orientation, 0.0, 0.0, 1.0, a);
	o->timestamp = universe_timestamp;
}

static void docking_port_move(struct snis_entity *o)
{
	int i;
	struct snis_entity *docker;
	union vec3 offset = { { -25, 0, 0 } };
	union quat new_orientation;
	double ox, oy, oz, dx, dy, dz;
	const float motion_damping_factor = 0.5;
	const float slerp_rate = 0.1;

	if (o->tsd.docking_port.docked_guy == (uint32_t) -1)
		return;
	i = lookup_by_id(o->tsd.docking_port.docked_guy);
	if (i < 0)
		return;
	quat_rot_vec_self(&offset, &o->orientation);
	docker = &go[i];
	if (!docker->tsd.ship.docking_magnets) { /* undocking happens here */
		o->tsd.docking_port.docked_guy = (uint32_t) -1;
		revoke_docking_permission(o, docker->id);
		if (docker->type == OBJTYPE_SHIP1) {
			int bn = lookup_bridge_by_shipid(docker->id);
			if (bn >= 0)
				bridgelist[bn].last_docked_time = universe_timestamp;
		}
	}
	quat_slerp(&new_orientation, &docker->orientation, &o->orientation, slerp_rate);
	docker->orientation = new_orientation;
	ox = docker->x;
	oy = docker->y;
	oz = docker->z;

	dx = o->x + offset.v.x - docker->x;
	dy = o->y + offset.v.y - docker->y;
	dz = o->z + offset.v.z - docker->z;

	/* damp motion a bit instead of just snapping. */
	dx *= motion_damping_factor;
	dy *= motion_damping_factor;
	dz *= motion_damping_factor;

	set_object_location(docker, docker->x + dx, docker->y + dy, docker->z + dz);

	/* set velocity in accord with the movement we just did */
	if (docker->tsd.ship.docking_magnets) {
		docker->vx = docker->x - ox;
		docker->vy = docker->y - oy;
		docker->vz = docker->z - oz;
	} else {
		/* if undocking, don't damp motion */
		docker->vx = (docker->x - ox) / motion_damping_factor;
		docker->vy = (docker->y - oy) / motion_damping_factor;
		docker->vz = (docker->z - oz) / motion_damping_factor;
	}

	docker->timestamp = universe_timestamp;
}

static void block_add_to_naughty_list(struct snis_entity *o, uint32_t id)
{
	const int itemsize = sizeof(o->tsd.block.naughty_list[0]);
	uint32_t *dest = &o->tsd.block.naughty_list[1];
	uint32_t *src = &o->tsd.block.naughty_list[0];
	uint32_t rootid;
	int i;

	if (id == (uint32_t) -1)
		return;
	if (o->type != OBJTYPE_BLOCK)
		return;
	i = lookup_by_id(id);
	if (i < 0)
		return;
	if (go[i].type != OBJTYPE_SHIP1 && go[i].type != OBJTYPE_SHIP2)
		return;
	rootid = o->tsd.block.root_id;
	if (rootid != -1) {
		i = lookup_by_id(rootid);
		if (i < 0)
			return;
		dest = &go[i].tsd.block.naughty_list[1];
		src = &go[i].tsd.block.naughty_list[0];
	}
	memmove(dest, src, itemsize * (ARRAYSIZE(o->tsd.block.naughty_list) - 1));
	*src = id;
}

static void block_move(struct snis_entity *o)
{
	struct snis_entity *parent;
	union vec3 pos;
	int i;

	if (o->tsd.block.parent_id == (uint32_t) -1)
		goto default_move;
	i = lookup_by_id(o->tsd.block.parent_id);
	if (i < 0)
		goto default_move;
	parent = &go[i];
	pos.v.x = o->tsd.block.dx;
	pos.v.y = o->tsd.block.dy;
	pos.v.z = o->tsd.block.dz;
	quat_rot_vec_self(&pos, &parent->orientation);
	quat_mul(&o->orientation, &parent->orientation, &o->tsd.block.relative_orientation);
	quat_normalize_self(&o->orientation);
	o->vx = pos.v.x + parent->x - o->x;
	o->vy = pos.v.y + parent->y - o->y;
	o->vz = pos.v.z + parent->z - o->z;
	set_object_location(o, pos.v.x + parent->x, pos.v.y + parent->y, pos.v.z + parent->z);
	block_calculate_obb(o, &o->tsd.block.shape.cuboid.obb);
	o->timestamp = universe_timestamp;
	if (o->tsd.block.health <= 100 && o->tsd.block.parent_id != (uint32_t) -1) {
		o->tsd.block.parent_id = (uint32_t) -1;
		(void) add_explosion(o->x, o->y, o->z, 50, 150, 50, o->type);
		o->tsd.block.health = snis_randn(10) + 10;
	}
	return;

default_move:
	quat_mul(&o->orientation, &o->orientation, &o->tsd.block.rotational_velocity);
	quat_normalize_self(&o->orientation);
	set_object_location(o, o->x + o->vx, o->y + o->vy, o->z + o->vz);
	block_calculate_obb(o, &o->tsd.block.shape.cuboid.obb);
	if (o->tsd.block.health == 0) {
		(void) add_explosion(o->x, o->y, o->z, 50, 150, 50, o->type);
		o->alive = 0;
		delete_from_clients_and_server(o);
	}
	if (o->tsd.block.health <= 100 && o->tsd.block.health > 0)
		o->tsd.block.health--;
	o->timestamp = universe_timestamp;
	return;
}

static void turret_fire(struct snis_entity *o)
{
	union vec3 pos_right = { { 40.0, 0.0, 20.0 } };
	union vec3 pos_left = { { 40.0, 0.0, -20.0 } };
	union vec3 vel = { { 1.0, 0.0, 0.0 } };

	quat_rot_vec_self(&vel, &o->orientation);
	vec3_mul_self(&vel, LASER_VELOCITY);
	quat_rot_vec_self(&pos_right, &o->orientation);
	quat_rot_vec_self(&pos_left, &o->orientation);

	add_laser(o->x, o->y, o->z,
		vel.v.x, vel.v.y, vel.v.z, &o->orientation, o->id);
	add_laser(o->x + pos_left.v.x, o->y + pos_left.v.y, o->z + pos_left.v.z,
		vel.v.x, vel.v.y, vel.v.z, &o->orientation, o->id);
	o->tsd.turret.fire_countdown = o->tsd.turret.fire_countdown_reset_value;
}

static void turret_move(struct snis_entity *o)
{
	struct snis_entity *parent, *target;
	union vec3 pos;
	int i, t;
	int root, aim_is_good;
	double min_dist = 1e20;
	int closest = -1;
	struct turret_params tparams;

	tparams.elevation_lower_limit = -30.0 * M_PI / 180.0;
	tparams.elevation_upper_limit = 90.0 * M_PI / 180.0;
	tparams.azimuth_lower_limit = -3.0 * M_PI; /* no limit */
	tparams.azimuth_upper_limit = 3.0 * M_PI; /* no limit */
	tparams.elevation_rate_limit = 6.0 * M_PI / 180.0; /* 60 degrees/sec at 10Hz */
	tparams.azimuth_rate_limit = 6.0 * M_PI / 180.0; /* 60 degrees/sec at 10Hz */

	if (o->tsd.turret.health == 0) {
		/* TODO add turret derelict */
		o->alive = 0;
	}
	if (o->tsd.turret.parent_id == (uint32_t) -1)
		goto default_move;
	i = lookup_by_id(o->tsd.turret.parent_id);
	if (i < 0)
		goto default_move;
	parent = &go[i];

	pos.v.x = o->tsd.turret.dx;
	pos.v.y = o->tsd.turret.dy;
	pos.v.z = o->tsd.turret.dz;

	/* Check to see what our target should be, if anything */
	root = lookup_by_id(o->tsd.turret.root_id);
	if (root < 0 || (parent->type == OBJTYPE_BLOCK && parent->tsd.block.health == 0)) {
		o->tsd.turret.current_target_id = (uint32_t) -1;
	} else {
		for (i = 0; i < ARRAYSIZE(go[root].tsd.block.naughty_list); i++)  {
			if (go[root].tsd.block.naughty_list[i] == (uint32_t) -1)
				break;
			int n = lookup_by_id(go[root].tsd.block.naughty_list[i]);
			if (n < 0)
				continue;
			double dist2 = object_dist2(o, &go[n]);
			if (dist2 > LASER_RANGE * LASER_RANGE) /* too far? */
				continue;
			if (dist2 < min_dist) {
				min_dist = dist2;
				closest = n;
			}
		}
		if (closest < 0) {
			o->tsd.turret.current_target_id = (uint32_t) -1;
		} else {
			o->tsd.turret.current_target_id = go[closest].id;
		}
	}

	if (o->tsd.turret.current_target_id == (uint32_t) -1) { /* no target, return to rest orientation */
		union quat rest_orientation, new_turret_orientation, new_turret_base_orientation;
		union vec3 aim_point;

		aim_point.v.x = 1.0;
		aim_point.v.y = 0.0;
		aim_point.v.z = 0.0;
		quat_mul(&rest_orientation, &parent->orientation, &o->tsd.turret.relative_orientation);
		quat_rot_vec_self(&aim_point, &rest_orientation);
		aim_point.v.x = aim_point.v.x + o->x;
		aim_point.v.y = aim_point.v.y + o->y;
		aim_point.v.z = aim_point.v.z + o->z;
		turret_aim(aim_point.v.x, aim_point.v.y, aim_point.v.z, o->x, o->y, o->z,
				&rest_orientation, &o->orientation, &tparams,
				&new_turret_orientation, &new_turret_base_orientation, &aim_is_good);
		o->orientation = new_turret_orientation;
		o->tsd.turret.base_orientation = new_turret_base_orientation;
	} else { /* aim at target */
		union vec3 to_enemy;

		t = lookup_by_id(o->tsd.turret.current_target_id);
		if (t < 0) {
			o->tsd.turret.current_target_id = (uint32_t) -1;
		} else {
			double dist, lasertime;
			union quat rest_orientation, new_turret_orientation, new_turret_base_orientation;
			union vec3 aim_point;

			target = &go[t];
			to_enemy.v.x = target->x - o->x;
			to_enemy.v.y = target->y - o->y;
			to_enemy.v.z = target->z - o->z;
			dist = vec3_magnitude(&to_enemy);
			lasertime = dist / (double) LASER_VELOCITY;
			aim_point.v.x = target->x + target->vx * lasertime;
			aim_point.v.y = target->y + target->vy * lasertime;
			aim_point.v.z = target->z + target->vz * lasertime;
			quat_mul(&rest_orientation, &parent->orientation, &o->tsd.turret.relative_orientation);
			turret_aim(aim_point.v.x, aim_point.v.y, aim_point.v.z, o->x, o->y, o->z,
					&rest_orientation, &o->orientation, &tparams,
					&new_turret_orientation, &new_turret_base_orientation, &aim_is_good);
			o->orientation = new_turret_orientation;
			o->tsd.turret.base_orientation = new_turret_base_orientation;
			if (aim_is_good && o->tsd.turret.fire_countdown == 0)
				turret_fire(o);
		}
	}
	quat_rot_vec_self(&pos, &parent->orientation);
	o->vx = pos.v.x + parent->x - o->x;
	o->vy = pos.v.y + parent->y - o->y;
	o->vz = pos.v.z + parent->z - o->z;
	set_object_location(o, pos.v.x + parent->x, pos.v.y + parent->y, pos.v.z + parent->z);
	o->timestamp = universe_timestamp;
	if (o->tsd.turret.fire_countdown > 0)
		o->tsd.turret.fire_countdown--;
	return;

default_move:
	quat_mul(&o->orientation, &o->orientation, &o->tsd.turret.rotational_velocity);
	quat_normalize_self(&o->orientation);
	set_object_location(o, o->x + o->vx, o->y + o->vy, o->z + o->vz);
	o->timestamp = universe_timestamp;
	if (o->tsd.turret.health > 10)
		o->tsd.turret.health = snis_randn(7) + 3;
	o->tsd.turret.health--;
	if (o->tsd.turret.health == 0) {
		(void) add_explosion(o->x, o->y, o->z, 50, 150, 50, o->type);
		o->alive = 0;
		delete_from_clients_and_server(o);
	}
	return;
}

static void starbase_update_docking_ports(struct snis_entity *o)
{
	int i, d, model;
	struct snis_entity *port;
	union vec3 pos;

	model = o->id % nstarbase_models;

	if (!docking_port_info[model])
		return;

	for (i = 0; i < docking_port_info[model]->nports; i++) {
		d = lookup_by_id(o->tsd.starbase.docking_port[i]);
		if (d < 0)
			continue;
		port = &go[d];
		pos = docking_port_info[model]->port[i].pos;
		quat_rot_vec_self(&pos, &o->orientation);
		quat_mul(&port->orientation, &o->orientation,
			 &docking_port_info[model]->port[i].orientation);
		set_object_location(port, pos.v.x + o->x, pos.v.y + o->y, pos.v.z + o->z);
		port->timestamp = universe_timestamp;
	}
}

static uint32_t docking_port_resident(uint32_t docking_port_id)
{
	int i;

	i = lookup_by_id(docking_port_id);
	if (i < 0)
		return (uint32_t) -1;
	return go[i].tsd.docking_port.docked_guy;
}

static void orbit_starbase(struct snis_entity *o)
{
	int i;
	struct snis_entity *p;
	union vec3 pos, *orbit;
	union quat q;

	if (!starbases_orbit)
		return;
	if (o->tsd.starbase.associated_planet_id == -1)
		return;
	i = lookup_by_id(o->tsd.starbase.associated_planet_id);
	if (i < 0) {
		o->tsd.starbase.associated_planet_id = -1;
		return;
	}
	p = &go[i];
	if (p->type != OBJTYPE_PLANET) {
		/* This can happen if a planet gets destroyed and another object re-uses the ID. */
		o->tsd.starbase.associated_planet_id = -1;
		return;
	}
	/* Compute the new position in the orbit */
	pos.v.x = 1.0;
	pos.v.y = 0.0;
	pos.v.z = 0.0;
	orbit = &o->tsd.starbase.orbital_axis;
	quat_init_axis(&q, orbit->v.x, orbit->v.y, orbit->v.z, o->tsd.starbase.orbital_angle);
	quat_rot_vec_self(&pos, &q);
	vec3_mul_self(&pos, o->tsd.starbase.altitude);
	o->x = p->x + pos.v.x;
	o->y = p->y + pos.v.y;
	o->z = p->z + pos.v.z;
	o->tsd.starbase.orbital_angle += o->tsd.starbase.orbital_velocity;
	/* Wrap the orbital angle around to 0 at 2PI */
	if (o->tsd.starbase.orbital_angle >= 2 * M_PI)
		o->tsd.starbase.orbital_angle -= 2 * M_PI;
}

static void starbase_move(struct snis_entity *o)
{
	char location[50];
	int64_t then, now;
	int i, j, model;
	int fired_at_player = 0;
	static struct mtwist_state *mt = NULL;

	if (!mt)
		mt = mtwist_init(mtwist_seed);

	spin_starbase(o);
	orbit_starbase(o);
	starbase_update_docking_ports(o);
	then = o->tsd.starbase.last_time_called_for_help;
	now = universe_timestamp;
	if (!suppress_starbase_complaints && o->tsd.starbase.under_attack &&
		(then < now - 2000 || 
		o->tsd.starbase.last_time_called_for_help == 0)) {
		o->tsd.starbase.last_time_called_for_help = universe_timestamp;
		send_comms_packet(o, "", 0, "STARBASE %s:", o->sdata.name);
		send_comms_packet(o, "-  ", 0, starbase_comm_under_attack());
		snprintf(location, sizeof(location), "COORDINATES %.0lf, %.0lf, %.0lf", o->x, o->y, o->z);
		send_comms_packet(o, "-  ", 0, "LOCATION %s", location);
	}

	for (i = 0; i < o->tsd.starbase.nattackers; i++) {
		int j;
		struct snis_entity *a;

		j = lookup_by_id(o->tsd.starbase.attacker[i]);
		if (j < 0)
			continue;
		a = &go[j];
		if (!a->alive)
			continue;
		if (a->type != OBJTYPE_SHIP1 && a->type != OBJTYPE_SHIP2)
			continue;
		float dist2 = object_dist2(o, a);
		if (dist2 > (torpedo_velocity * torpedo_lifetime) * (torpedo_velocity * torpedo_lifetime))
			continue;
		if (snis_randn(1000) < STARBASE_FIRE_CHANCE)
			continue;
		if (planet_between_objs(o, a))
			continue;
		if (snis_randn(100) < 30 &&
			o->tsd.starbase.next_torpedo_time <= universe_timestamp) {
			/* fire torpedo */
			float dist = sqrt(dist2);
			double tx, ty, tz, vx, vy, vz;
			float flight_time = dist / torpedo_velocity;

			tx = a->x + (a->vx * flight_time);
			tz = a->z + (a->vz * flight_time);
			ty = a->y + (a->vy * flight_time);
			calculate_torpedo_velocities(o->x, o->y, o->z,
				tx, ty, tz, torpedo_velocity, &vx, &vy, &vz);
			add_torpedo(o->x, o->y, o->z, vx, vy, vz, o->id);
			o->tsd.starbase.next_torpedo_time = universe_timestamp +
					STARBASE_TORPEDO_FIRE_INTERVAL;
			fired_at_player = a->type == OBJTYPE_SHIP1;
		}
		if (snis_randn(100) < 30 &&
			o->tsd.starbase.next_laser_time <= universe_timestamp) {
			/* fire laser */
			add_laserbeam(o->id, a->id, LASERBEAM_DURATION);
			o->tsd.starbase.next_laser_time = universe_timestamp +
					STARBASE_LASER_FIRE_INTERVAL;
			fired_at_player |= (a->type == OBJTYPE_SHIP1);
		}
		if (fired_at_player && snis_randn(100) < 20) {
			char x[200];
			char *c, *lastc;

			starbase_attack_warning(mt, x, sizeof(x), 50);
			uppercase(x);

			c = x;
			while (c) {
				lastc = c;
				c = strchr(c, '\n');
				if (c) {
					*c = '\0';
					if (lastc == x)
						send_comms_packet(o, o->sdata.name, 0,
							":  %s,  %s", a->sdata.name, lastc);
					else
						send_comms_packet(o, o->sdata.name, 0, ":  %s", lastc);
					c++;
				} else {
					send_comms_packet(o, o->sdata.name, 0, ":  %s", lastc);
				}
			}
		}
	}

	/* expire the expected dockers */
	model = o->id % nstarbase_models;
	if (docking_port_info[model]) {
		for (j = 0; j < docking_port_info[model]->nports; j++) {
			int id = o->tsd.starbase.expected_docker[j];
			if (id == -1)
				continue;
			if (o->tsd.starbase.expected_docker_timer[j] > 0)
				o->tsd.starbase.expected_docker_timer[j]--;
			if (o->tsd.starbase.expected_docker_timer[j] > 0)
				continue;
			/* TODO: make this safe */
			int bn = lookup_bridge_by_shipid(id);
			if (bn < 0)
				continue;
			struct bridge_data *b = &bridgelist[bn];
			if (docking_port_resident(o->tsd.starbase.docking_port[j]) == b->shipid)
				continue; /* Do not expire docking permission on a docked ship */
			o->tsd.starbase.expected_docker[j] = -1;
			send_comms_packet(o, o->sdata.name, b->npcbot.channel,
				"%s, PERMISSION TO DOCK EXPIRED.", b->shipname);
			snis_queue_add_sound(PERMISSION_TO_DOCK_EXPIRED, ROLE_NAVIGATION, b->shipid);
		}
	}

	if (rts_mode) { /* Build any RTS units ordered */
		if (o->tsd.starbase.time_left_to_build > 0) { /* Building something? */
			o->tsd.starbase.time_left_to_build--;
			if (o->tsd.starbase.time_left_to_build == 0 &&
				o->tsd.starbase.build_unit_type != 255) { /* Unit is completed? */
				printf("Starbase has completed building a unit\n");
				add_new_rts_unit(o);
			}
		}
	}
}

static void warpgate_move(struct snis_entity *o)
{
	o->timestamp = universe_timestamp;
}

static void explosion_move(struct snis_entity *o)
{
	if (o->alive)
		o->alive--;
	if (o->alive == 0)
		delete_from_clients_and_server(o);
}

static void chaff_move(struct snis_entity *o)
{
	set_object_location(o, o->x + o->vx, o->y + o->vy, o->z + o->vz);
	o->timestamp = universe_timestamp;
	if (o->alive)
		o->alive--;
	/* TODO collide with missiles */
	if (o->alive == 0)
		delete_from_clients_and_server(o);
}

static void queue_up_to_clients_that_care(struct snis_entity *o);

static int add_generic_object(double x, double y, double z,
				double vx, double vy, double vz, double heading, int type)
{
	int i, j;
	char *n;
	union vec3 v;
	static struct mtwist_state *mt = NULL;

	if (!mt)
		mt = mtwist_init(mtwist_seed);

	i = snis_object_pool_alloc_obj(pool); 	 
	if (i < 0)
		return -1;
	memset(&go[i], 0, sizeof(go[i]));
	go[i].id = get_new_object_id();
	go[i].alive = 1;
	set_object_location(&go[i], x, y, z);
	go[i].vx = vx;
	go[i].vy = vy;
	go[i].vz = vz;
	go[i].heading = heading;
	go[i].type = type;
	go[i].timestamp = universe_timestamp;
	go[i].move = generic_move;

	/* clear out the client update state */
	for (j = 0; j < nclients; j++)
		if (client[j].refcount)
			/* gcc produces identical code for this memset as for a straight assignment */
			memset(&client[j].go_clients[i], 0, sizeof(client[j].go_clients[i]));

	switch (type) {
	case OBJTYPE_SHIP1:
	case OBJTYPE_SHIP2:
	case OBJTYPE_ASTEROID:
	case OBJTYPE_STARBASE:
	case OBJTYPE_DERELICT:
	case OBJTYPE_PLANET:
	case OBJTYPE_BLACK_HOLE:
		n = random_name(mt);
		strncpy(go[i].sdata.name, n, sizeof(go[i].sdata.name) - 1);
		free(n);
		break;
	default:
		memset(go[i].sdata.name, 0, sizeof(go[i].sdata.name));
		break;
	}

	go[i].sdata.shield_strength = snis_randn(256);
	go[i].sdata.shield_wavelength = snis_randn(256);
	go[i].sdata.shield_width = snis_randn(128);
	go[i].sdata.shield_depth = snis_randn(255);
	v.v.x = x;
	v.v.y = y;
	v.v.z = z;
	go[i].sdata.faction = (uint8_t) nearest_faction(v);
	quat_init_axis(&go[i].orientation, 0, 1, 0, heading);
	return i;
}

#define DECLARE_POWER_MODEL_SAMPLER(name, model, which) \
static float sample_##model##_##name##_##which(void *cookie) \
{ \
	struct snis_entity *o = cookie; \
	float v = 255.0 - (float) o->tsd.ship.model.name.which; \
\
	if (v > 250.0) \
		v = 10000.0; \
	v =  v * 10000.0; \
	return v; \
	/* return (float) 255.0 / (float) (o->tsd.ship.power_data.name.which + 1.0); */ \
	/* return (float) (256.0 - (float) o->tsd.ship.power_data.name.which) / 256.0; */  \
}

DECLARE_POWER_MODEL_SAMPLER(warp, power_data, r1) /* declares sample_power_data_warp_r1 */
DECLARE_POWER_MODEL_SAMPLER(warp, power_data, r2) /* declares sample_power_data_warp_r2 */
DECLARE_POWER_MODEL_SAMPLER(warp, power_data, r3) /* declares sample_power_data_warp_r3 */
DECLARE_POWER_MODEL_SAMPLER(sensors, power_data, r1) /* declares sample_power_data_sensors_r1 */
DECLARE_POWER_MODEL_SAMPLER(sensors, power_data, r2) /* declares sample_power_data_sensors_r2 */
DECLARE_POWER_MODEL_SAMPLER(sensors, power_data, r3) /* declares sample_power_data_sensors_r3 */
DECLARE_POWER_MODEL_SAMPLER(phasers, power_data, r1) /* declares sample_power_data_phasers_r1 */
DECLARE_POWER_MODEL_SAMPLER(phasers, power_data, r2) /* declares sample_power_data_phasers_r2 */
DECLARE_POWER_MODEL_SAMPLER(phasers, power_data, r3) /* declares sample_power_data_phasers_r3 */
DECLARE_POWER_MODEL_SAMPLER(maneuvering, power_data, r1) /* declares sample_power_data_maneuvering_r1 */
DECLARE_POWER_MODEL_SAMPLER(maneuvering, power_data, r2) /* declares sample_power_data_maneuvering_r2 */
DECLARE_POWER_MODEL_SAMPLER(maneuvering, power_data, r3) /* declares sample_power_data_maneuvering_r3 */
DECLARE_POWER_MODEL_SAMPLER(shields, power_data, r1) /* declares sample_power_data_shields_r1 */
DECLARE_POWER_MODEL_SAMPLER(shields, power_data, r2) /* declares sample_power_data_shields_r2 */
DECLARE_POWER_MODEL_SAMPLER(shields, power_data, r3) /* declares sample_power_data_shields_r3 */
DECLARE_POWER_MODEL_SAMPLER(comms, power_data, r1) /* declares sample_power_data_comms_r1 */
DECLARE_POWER_MODEL_SAMPLER(comms, power_data, r2) /* declares sample_power_data_comms_r2 */
DECLARE_POWER_MODEL_SAMPLER(comms, power_data, r3) /* declares sample_power_data_comms_r3 */
DECLARE_POWER_MODEL_SAMPLER(impulse, power_data, r1) /* declares sample_power_data_impulse_r1 */
DECLARE_POWER_MODEL_SAMPLER(impulse, power_data, r2) /* declares sample_power_data_impulse_r2 */
DECLARE_POWER_MODEL_SAMPLER(impulse, power_data, r3) /* declares sample_power_data_impulse_r3 */
DECLARE_POWER_MODEL_SAMPLER(tractor, power_data, r1) /* declares sample_power_data_tractor_r1 */
DECLARE_POWER_MODEL_SAMPLER(tractor, power_data, r2) /* declares sample_power_data_tractor_r2 */
DECLARE_POWER_MODEL_SAMPLER(tractor, power_data, r3) /* declares sample_power_data_tractor_r3 */
DECLARE_POWER_MODEL_SAMPLER(lifesupport, power_data, r1) /* declares sample_power_data_lifesupport_r1 */
DECLARE_POWER_MODEL_SAMPLER(lifesupport, power_data, r2) /* declares sample_power_data_lifesupport_r2 */
DECLARE_POWER_MODEL_SAMPLER(lifesupport, power_data, r3) /* declares sample_power_data_lifesupport_r3 */

DECLARE_POWER_MODEL_SAMPLER(warp, coolant_data, r1) /* declares sample_coolant_data_warp_r1 */
DECLARE_POWER_MODEL_SAMPLER(warp, coolant_data, r2) /* declares sample_coolant_data_warp_r2 */
DECLARE_POWER_MODEL_SAMPLER(warp, coolant_data, r3) /* declares sample_coolant_data_warp_r3 */
DECLARE_POWER_MODEL_SAMPLER(sensors, coolant_data, r1) /* declares sample_coolant_data_sensors_r1 */
DECLARE_POWER_MODEL_SAMPLER(sensors, coolant_data, r2) /* declares sample_coolant_data_sensors_r2 */
DECLARE_POWER_MODEL_SAMPLER(sensors, coolant_data, r3) /* declares sample_coolant_data_sensors_r3 */
DECLARE_POWER_MODEL_SAMPLER(phasers, coolant_data, r1) /* declares sample_coolant_data_phasers_r1 */
DECLARE_POWER_MODEL_SAMPLER(phasers, coolant_data, r2) /* declares sample_coolant_data_phasers_r2 */
DECLARE_POWER_MODEL_SAMPLER(phasers, coolant_data, r3) /* declares sample_coolant_data_phasers_r3 */
DECLARE_POWER_MODEL_SAMPLER(maneuvering, coolant_data, r1) /* declares sample_coolant_data_maneuvering_r1 */
DECLARE_POWER_MODEL_SAMPLER(maneuvering, coolant_data, r2) /* declares sample_coolant_data_maneuvering_r2 */
DECLARE_POWER_MODEL_SAMPLER(maneuvering, coolant_data, r3) /* declares sample_coolant_data_maneuvering_r3 */
DECLARE_POWER_MODEL_SAMPLER(shields, coolant_data, r1) /* declares sample_coolant_data_shields_r1 */
DECLARE_POWER_MODEL_SAMPLER(shields, coolant_data, r2) /* declares sample_coolant_data_shields_r2 */
DECLARE_POWER_MODEL_SAMPLER(shields, coolant_data, r3) /* declares sample_coolant_data_shields_r3 */
DECLARE_POWER_MODEL_SAMPLER(comms, coolant_data, r1) /* declares sample_coolant_data_comms_r1 */
DECLARE_POWER_MODEL_SAMPLER(comms, coolant_data, r2) /* declares sample_coolant_data_comms_r2 */
DECLARE_POWER_MODEL_SAMPLER(comms, coolant_data, r3) /* declares sample_coolant_data_comms_r3 */
DECLARE_POWER_MODEL_SAMPLER(impulse, coolant_data, r1) /* declares sample_coolant_data_impulse_r1 */
DECLARE_POWER_MODEL_SAMPLER(impulse, coolant_data, r2) /* declares sample_coolant_data_impulse_r2 */
DECLARE_POWER_MODEL_SAMPLER(impulse, coolant_data, r3) /* declares sample_coolant_data_impulse_r3 */
DECLARE_POWER_MODEL_SAMPLER(tractor, coolant_data, r1) /* declares sample_coolant_data_tractor_r1 */
DECLARE_POWER_MODEL_SAMPLER(tractor, coolant_data, r2) /* declares sample_coolant_data_tractor_r2 */
DECLARE_POWER_MODEL_SAMPLER(tractor, coolant_data, r3) /* declares sample_coolant_data_tractor_r3 */
DECLARE_POWER_MODEL_SAMPLER(lifesupport, coolant_data, r1) /* declares sample_coolant_data_lifesupport_r1 */
DECLARE_POWER_MODEL_SAMPLER(lifesupport, coolant_data, r2) /* declares sample_coolant_data_lifesupport_r2 */
DECLARE_POWER_MODEL_SAMPLER(lifesupport, coolant_data, r3) /* declares sample_coolant_data_lifesupport_r3 */


#define POWERFUNC(system, number) \
	sample_power_data_##system##_r##number

#define POWERFUNCS(system) \
	POWERFUNC(system, 1), POWERFUNC(system, 2), POWERFUNC(system, 3)

#define COOLANTFUNC(system, number) \
	sample_coolant_data_##system##_r##number

#define COOLANTFUNCS(system) \
	COOLANTFUNC(system, 1), COOLANTFUNC(system, 2), COOLANTFUNC(system, 3)

static void init_power_model(struct snis_entity *o)
{
	struct power_model *pm;
	struct power_model_data *pd;
	struct power_device *d;

/*
	if (o->tsd.ship.power_model)
		free_power_model(o->tsd.ship.power_model);
*/
	memset(&o->tsd.ship.power_data, 0, sizeof(o->tsd.ship.power_data));

	pm = new_power_model(MAX_CURRENT, MAX_VOLTAGE, INTERNAL_RESIST);
	pd = &o->tsd.ship.power_data;
	o->tsd.ship.power_model = pm; 

	/* Warp */
	pd->warp.r1 = 0;
	pd->warp.r2 = 0;
	pd->warp.r3 = 200;
	d = new_power_device(o, POWERFUNCS(warp));
	power_model_add_device(pm, d);

	/* Sensors */
	pd->sensors.r1 = 255;
	pd->sensors.r2 = 0;
	pd->sensors.r3 = 200;
	d = new_power_device(o, POWERFUNCS(sensors));
	power_model_add_device(pm, d);

	/* Phasers */
	pd->phasers.r1 = 255;
	pd->phasers.r2 = 0;
	pd->phasers.r3 = 200;
	d = new_power_device(o, POWERFUNCS(phasers));
	power_model_add_device(pm, d);

	/* Maneuvering */
	pd->maneuvering.r1 = 255;
	pd->maneuvering.r2 = 0;
	pd->maneuvering.r3 = 200;
	d = new_power_device(o, POWERFUNCS(maneuvering));
	power_model_add_device(pm, d);

	/* Shields */
	pd->shields.r1 = 255;
	pd->shields.r2 = 0;
	pd->shields.r3 = 200;
	d = new_power_device(o, POWERFUNCS(shields));
	power_model_add_device(pm, d);

	/* Comms */
	pd->comms.r1 = 255;
	pd->comms.r2 = 0;
	pd->comms.r3 = 200;
	d = new_power_device(o, POWERFUNCS(comms));
	power_model_add_device(pm, d);

	/* Impulse */
	pd->impulse.r1 = 0; //255;
	pd->impulse.r2 = 0;
	pd->impulse.r3 = 200;
	d = new_power_device(o, POWERFUNCS(impulse));
	power_model_add_device(pm, d);

	/* Tractor Beam */
	pd->tractor.r1 = 255;
	pd->tractor.r2 = 0;
	pd->tractor.r3 = 200;
	d = new_power_device(o, POWERFUNCS(tractor));
	power_model_add_device(pm, d);

	/* Lifesupport system */
	pd->lifesupport.r1 = 255;
	pd->lifesupport.r2 = 0;
	pd->lifesupport.r3 = 200;
	d = new_power_device(o, POWERFUNCS(lifesupport));
	power_model_add_device(pm, d);
}

static void init_coolant_model(struct snis_entity *o)
{
	struct power_model *pm;
	struct power_model_data *pd;
	struct power_device *d;

/*
	if (o->tsd.ship.power_model)
		free_power_model(o->tsd.ship.power_model);
*/
	memset(&o->tsd.ship.coolant_data, 0, sizeof(o->tsd.ship.coolant_data));

	pm = new_power_model(MAX_COOLANT, MAX_VOLTAGE, INTERNAL_RESIST);
	pd = &o->tsd.ship.coolant_data;
	o->tsd.ship.coolant_model = pm; 

	/* Warp */
	pd->warp.r1 = 255;
	pd->warp.r2 = 0;
	pd->warp.r3 = 200;
	d = new_power_device(o, COOLANTFUNCS(warp));
	power_model_add_device(pm, d);

	/* Sensors */
	pd->sensors.r1 = 255;
	pd->sensors.r2 = 0;
	pd->sensors.r3 = 200;
	d = new_power_device(o, COOLANTFUNCS(sensors));
	power_model_add_device(pm, d);

	/* Phasers */
	pd->phasers.r1 = 255;
	pd->phasers.r2 = 0;
	pd->phasers.r3 = 200;
	d = new_power_device(o, COOLANTFUNCS(phasers));
	power_model_add_device(pm, d);

	/* Maneuvering */
	pd->maneuvering.r1 = 255;
	pd->maneuvering.r2 = 0;
	pd->maneuvering.r3 = 200;
	d = new_power_device(o, COOLANTFUNCS(maneuvering));
	power_model_add_device(pm, d);

	/* Shields */
	pd->shields.r1 = 255;
	pd->shields.r2 = 0;
	pd->shields.r3 = 200;
	d = new_power_device(o, COOLANTFUNCS(shields));
	power_model_add_device(pm, d);

	/* Comms */
	pd->comms.r1 = 255;
	pd->comms.r2 = 0;
	pd->comms.r3 = 200;
	d = new_power_device(o, COOLANTFUNCS(comms));
	power_model_add_device(pm, d);

	/* Impulse */
	pd->impulse.r1 = 255;
	pd->impulse.r2 = 0;
	pd->impulse.r3 = 200;
	d = new_power_device(o, COOLANTFUNCS(impulse));
	power_model_add_device(pm, d);

	/* Tractor Beam */
	pd->tractor.r1 = 255;
	pd->tractor.r2 = 0;
	pd->tractor.r3 = 200;
	d = new_power_device(o, COOLANTFUNCS(tractor));
	power_model_add_device(pm, d);

	/* Life Support System */
	pd->lifesupport.r1 = 255;
	pd->lifesupport.r2 = 0;
	pd->lifesupport.r3 = 200;
	d = new_power_device(o, COOLANTFUNCS(lifesupport));
	power_model_add_device(pm, d);
}

static void repair_damcon_systems(struct snis_entity *o)
{
	int i, n, b;
	struct damcon_data *d;
	struct snis_damcon_entity *p;

	if (o->type != OBJTYPE_SHIP1)
		return;

	b = lookup_bridge_by_shipid(o->id);
	if (b < 0)
		return;
	d = &bridgelist[b].damcon;
	n = snis_object_pool_highest_object(d->pool);
	for (i = 0; i <= n; i++) {
		p = &d->o[i];
		if (p->type != DAMCON_TYPE_PART)
			continue;
		p->tsd.part.damage = 0;
		p->version++;
	}
	o->tsd.ship.damage_data_dirty = 1;
}	
	
static void update_passenger(int i, int nstarbases);
static int count_starbases(void);


/* init_player()
 * o is the player ship
 *
 * reset_ship is 1 if we are resetting the ship (e.g. at
 *   the beginning of a game) and 0 if we do not want to reset the ship (e.g. we
 *   are just docking at a starbase.  Resetting the ship means the following
 *   additional actions are taken (as compared to just docking with a starbase):
 *
 *   1. Clearing any ship_id_chips the player has collected.
 *   2. Clearing any custom buttons lua scripts might have created.
 *   3. Clearing the ship's cargo bay.
 *   4. Resetting the ship's wallet to a default value.
 *   5. Clearing any passengers off the ship.
 *   6. Clearing any enciphered message and cipher keys
 *
 * *charges is an "out" parameter indicating the fees charged for repairs and
 *  restocking
 */
static void init_player(struct snis_entity *o, int reset_ship, float *charges)
{
	int i;
	int b;
	float money = 0.0;

#define TORPEDO_UNIT_COST 50.0f
#define FUEL_UNIT_COST (1500.0f / (float) UINT32_MAX)
#define WARP_CORE_COST 1500
	b = lookup_bridge_by_shipid(o->id);
	o->move = player_move;
	money += (INITIAL_TORPEDO_COUNT - o->tsd.ship.torpedoes) * TORPEDO_UNIT_COST;
	o->tsd.ship.torpedoes = INITIAL_TORPEDO_COUNT;
	o->tsd.ship.power = 100.0;
	o->tsd.ship.yaw_velocity = 0.0;
	o->tsd.ship.pitch_velocity = 0.0;
	o->tsd.ship.roll_velocity = 0.0;
	o->tsd.ship.velocity = 0.0;
	o->tsd.ship.desired_velocity = 0.0;
	o->tsd.ship.sci_beam_width = MAX_SCI_BW_YAW_VELOCITY;
	money += (float) (UINT32_MAX - o->tsd.ship.fuel) * FUEL_UNIT_COST;
	o->tsd.ship.fuel = UINT32_MAX;
	o->tsd.ship.oxygen = UINT32_MAX;
	o->tsd.ship.rpm = 0;
	o->tsd.ship.temp = 0;
	o->tsd.ship.power = 0;
	o->tsd.ship.scizoom = 128;
	o->tsd.ship.throttle = 200;
	o->tsd.ship.torpedo_load_time = 0;
	o->tsd.ship.torpedoes_loaded = 1;
	o->tsd.ship.torpedoes_loading = 0;
	o->tsd.ship.missile_count = initial_missile_count;
	o->tsd.ship.phaser_bank_charge = 0;
	o->tsd.ship.scizoom = 0;
	o->tsd.ship.weapzoom = 0;
	o->tsd.ship.navzoom = 0;
	o->tsd.ship.mainzoom = 0;
	o->tsd.ship.warpdrive = 0;
	o->tsd.ship.phaser_wavelength = 0;
	o->tsd.ship.nai_entries = 0;
	memset(o->tsd.ship.ai, 0, sizeof(o->tsd.ship.ai));
	o->tsd.ship.power_data.shields.r1 = 0;
	o->tsd.ship.power_data.phasers.r1 = 0;
	o->tsd.ship.power_data.sensors.r1 = 0;
	o->tsd.ship.power_data.comms.r1 = 0;
	o->tsd.ship.power_data.warp.r1 = 0;
	o->tsd.ship.power_data.maneuvering.r1 = 0;
	o->tsd.ship.power_data.impulse.r1 = 0;
	o->tsd.ship.power_data.tractor.r1 = 0;
	o->tsd.ship.power_data.lifesupport.r1 = 230; /* don't turn off the air */
	o->tsd.ship.coolant_data.lifesupport.r1 = 255;
	o->tsd.ship.warp_time = -1;
	o->tsd.ship.scibeam_range = 0;
	o->tsd.ship.scibeam_a1 = 0;
	o->tsd.ship.scibeam_a2 = 0;
	o->tsd.ship.sci_heading = M_PI / 2.0;
	o->tsd.ship.reverse = 0;
	o->tsd.ship.shiptype = SHIP_CLASS_WOMBAT; 
	o->tsd.ship.damage_data_dirty = 1;
	o->tsd.ship.ncargo_bays = 8;
	o->tsd.ship.passenger_berths = PASSENGER_BERTHS;
	o->tsd.ship.mining_bots = 1;
	o->tsd.ship.emf_detector = 0;
	o->tsd.ship.nav_mode = NAV_MODE_NORMAL;
	o->tsd.ship.orbiting_object_id = 0xffffffff;
	o->tsd.ship.nav_damping_suppression = 0.0;
	o->tsd.ship.rts_active_button = 255; /* none active */
	o->tsd.ship.exterior_lights = 255; /* On */
	if (o->tsd.ship.warp_core_status != WARP_CORE_STATUS_GOOD)
		money += WARP_CORE_COST;
	o->tsd.ship.warp_core_status = WARP_CORE_STATUS_GOOD;
	if (b >= 0) { /* On first joining, ship won't have a bridge yet. */
		bridgelist[b].warp_core_critical = 0;
		strcpy(bridgelist[b].last_text_to_speech, "");
		bridgelist[b].text_to_speech_volume = 0.33;
		bridgelist[b].text_to_speech_volume_timestamp = universe_timestamp;
		if (reset_ship) {
			/* Clear ship id chips */
			memset(bridgelist[b].ship_id_chip, 0, sizeof(bridgelist[b].ship_id_chip));
			bridgelist[b].nship_id_chips = 0;
			/* Clear any custom buttons a Lua script might have created */
			bridgelist[b].active_custom_buttons = 0;
			memset(bridgelist[b].custom_button_text, 0, sizeof(bridgelist[b].custom_button_text));
			memset(bridgelist[b].cipher_key, '_', sizeof(bridgelist[b].cipher_key));
			memset(bridgelist[b].guessed_key, '_', sizeof(bridgelist[b].guessed_key));
			memset(bridgelist[b].enciphered_message, 0, sizeof(bridgelist[b].enciphered_message));
			bridgelist[b].chaff_cooldown = 0;
		}
	}
	quat_init_axis(&o->tsd.ship.computer_desired_orientation, 0, 1, 0, 0);
	o->tsd.ship.computer_steering_time_left = 0;
	if (reset_ship) {
		int nstarbases = count_starbases();
		/* The reset_ship param is a stopgap until real docking code
		 * is done.
		 */
		for (i = 0; i < o->tsd.ship.ncargo_bays; i++) {
			o->tsd.ship.cargo[i].contents.item = -1;
			o->tsd.ship.cargo[i].contents.qty = 0.0f;
			o->tsd.ship.cargo[i].paid = 0.0;
			o->tsd.ship.cargo[i].origin = -1;
			o->tsd.ship.cargo[i].dest = -1;
			o->tsd.ship.cargo[i].due_date = -1;
		}
		if (rts_mode)
			o->tsd.ship.wallet = INITIAL_RTS_WALLET_MONEY;
		else
			o->tsd.ship.wallet = INITIAL_WALLET_MONEY;

		/* Clear any passengers off the ship */
		for (i = 0; i < MAX_PASSENGERS; i++)
			if (passenger[i].location == o->id)
				update_passenger(i, nstarbases);
		o->tsd.ship.lifeform_count = 5; /* because there are 5 stations: nav, weap, eng+damcon, sci, comms */
	}
	o->tsd.ship.viewpoint_object = o->id;
	quat_init_axis(&o->tsd.ship.sciball_orientation, 1, 0, 0, 0);
	quat_init_axis(&o->tsd.ship.weap_orientation, 1, 0, 0, 0);
	memset(&o->tsd.ship.damage, 0, sizeof(o->tsd.ship.damage));
	memset(&o->tsd.ship.temperature_data, 0, sizeof(o->tsd.ship.temperature_data));
	init_power_model(o);
	init_coolant_model(o);
	repair_damcon_systems(o);
	if (charges) {
		o->tsd.ship.wallet -= money;
		*charges = money;
	}
	o->tsd.ship.current_hg_ant_orientation = identity_quat;
	o->tsd.ship.desired_hg_ant_aim.v.x = 1.0;
	o->tsd.ship.desired_hg_ant_aim.v.y = 0.0;
	o->tsd.ship.desired_hg_ant_aim.v.z = 0.0;
}

static void clear_bridge_waypoints(int bridge)
{
	bridgelist[bridge].nwaypoints = 0;
	memset(bridgelist[bridge].waypoint, 0, sizeof(bridgelist[bridge].waypoint));
	set_waypoints_dirty_all_clients_on_bridge(bridgelist[bridge].shipid, 1);
}

static void respawn_player(struct snis_entity *o, uint8_t warpgate_number)
{
	int b, i, found;
	double x, y, z, a1, a2, rf;
	char mining_bot_name[20];
	static struct mtwist_state *mt = NULL;
	if (!mt)
		mt = mtwist_init(mtwist_seed);

	/* Find a friendly location to respawn... */
	if (warpgate_number != (uint8_t) -1) {
		for (i = 0; i <= snis_object_pool_highest_object(pool); i++) {
			struct snis_entity *f = &go[i];
			if (!f->alive || f->type != OBJTYPE_WARPGATE)
				continue;
			if (f->tsd.warpgate.warpgate_number != warpgate_number)
				continue;
			/* put player near at gate */
			set_object_location(o, f->x, f->y, f->z);
			goto finished;
		}
	}

	found = 0;
	for (i = 0; i <= snis_object_pool_highest_object(pool); i++) {
		struct snis_entity *f = &go[i];

		if (!f->alive || f->type != OBJTYPE_PLANET)
			continue;

		if (f->tsd.planet.security != HIGH_SECURITY)
			continue;

		/* put player near friendly planet at dist of 2 radii plus a bit. */
		a1 = snis_randn(360) * M_PI / 180;
		a2 = snis_randn(360) * M_PI / 180;
		rf = 2.0 + (double) snis_randn(1000) / 2000.0;
		x = f->x + cos(a1) * f->tsd.planet.radius * rf;
		y = f->y + sin(a1) * f->tsd.planet.radius * rf;
		z = f->z + sin(a2) * f->tsd.planet.radius * rf;
		set_object_location(o, x, y, z);
		o->tsd.ship.in_secure_area = 1;
		printf("found!\n");
		found = 1;
		break;
	}

	if (!found) { /* It's a lonely universe.  Roll the dice. */
		double x, y, z;
		for (int i = 0; i < 100; i++) {
			x = XKNOWN_DIM * (double) rand() / (double) RAND_MAX;
			y = 0;
			z = ZKNOWN_DIM * (double) rand() / (double) RAND_MAX;
			if (dist3d(x - SUNX, y - SUNY, z - SUNZ) > SUN_DIST_LIMIT)
				break;
		}
		set_object_location(o, x, y, z);
		o->tsd.ship.in_secure_area = 0;
	}

finished:
	b = lookup_bridge_by_shipid(o->id);
	if (b >= 0) {
		bridgelist[b].warptimeleft = 0; /* Stop any warp that might be in progress */
		if (warpgate_number != (uint8_t) -1) /* just entered new solar system? */
			clear_bridge_waypoints(b);
	}
	o->vx = 0;
	o->vy = 0;
	o->vz = 0;
	o->heading = 3 * M_PI / 2;
	memset(mining_bot_name, 0, sizeof(mining_bot_name));
	robot_name(mt, mining_bot_name, sizeof(mining_bot_name));
	snprintf(o->tsd.ship.mining_bot_name, sizeof(o->tsd.ship.mining_bot_name),
			"MNR-%s", mining_bot_name);
	quat_init_axis(&o->orientation, 0, 1, 0, o->heading);
	init_player(o, 1, NULL);
	if (warpgate_number != (uint8_t) -1) { /* Give player a little boost out of the warp gate */
		union vec3 boost;

		o->tsd.ship.velocity = max_player_velocity;
		boost.v.x = max_player_velocity * 4.0;
		boost.v.y = max_player_velocity * 4.0;
		boost.v.z = max_player_velocity * 4.0;
		quat_rot_vec_self(&boost, &o->orientation);
		o->x += boost.v.x;
		o->y += boost.v.y;
		o->z += boost.v.z;
	}
	o->alive = 1;
}

static int add_player(double x, double z, double vx, double vz, double heading,
			uint8_t warpgate_number, uint8_t requested_faction)
{
	int i;

	i = add_generic_object(x, 0.0, z, vx, 0.0, vz, heading, OBJTYPE_SHIP1);
	if (i < 0)
		return i;
	respawn_player(&go[i], warpgate_number);
	if (requested_faction < nfactions())
		go[i].sdata.faction = requested_faction;
	return i;
}

static uint32_t nth_starbase(int n);
static int commodity_sample(void);
static int add_ship(int faction, int shiptype, int auto_respawn)
{
	int i, cb;
	double x, y, z, heading;
	static struct mtwist_state *mt = NULL;
	char registration[100];

	if (!mt)
		mt = mtwist_init(mtwist_seed);

	for (i = 0; i < 100; i++) {
		x = ((double) snis_randn(1000)) * XKNOWN_DIM / 1000.0;
		y = (double) snis_randn(700) - 350;
		z = ((double) snis_randn(1000)) * ZKNOWN_DIM / 1000.0;
		if (dist3d(x - SUNX, y - SUNY, z - SUNZ) > SUN_DIST_LIMIT)
			break;
	}
	heading = degrees_to_radians(0.0 + snis_randn(360)); 
	i = add_generic_object(x, y, z, 0.0, 0.0, 0.0, heading, OBJTYPE_SHIP2);
	if (i < 0)
		return i;
	go[i].move = ship_move;
	go[i].tsd.ship.torpedoes = INITIAL_TORPEDO_COUNT;
	go[i].tsd.ship.power = 100.0;
	go[i].tsd.ship.yaw_velocity = 0.0;
	go[i].tsd.ship.pitch_velocity = 0.0;
	go[i].tsd.ship.roll_velocity = 0.0;
	go[i].tsd.ship.desired_velocity = 0;
	go[i].tsd.ship.velocity = 0;
	go[i].tsd.ship.shiptype = shiptype;
	go[i].sdata.subclass = shiptype;
	go[i].sdata.shield_strength = ship_type[shiptype].max_shield_strength;
	go[i].tsd.ship.nai_entries = 0;
	memset(go[i].tsd.ship.ai, 0, sizeof(go[i].tsd.ship.ai));
	go[i].tsd.ship.lifeform_count = ship_type[go[i].tsd.ship.shiptype].crew_max;
	go[i].tsd.ship.tractor_beam = 0xffffffff; /* turn off tractor beam */
	memset(&go[i].tsd.ship.damage, 0, sizeof(go[i].tsd.ship.damage));
	memset(&go[i].tsd.ship.power_data, 0, sizeof(go[i].tsd.ship.power_data));
	go[i].tsd.ship.braking_factor = 1.0f;
	go[i].tsd.ship.steering_adjustment.v.x = 0.0;
	go[i].tsd.ship.steering_adjustment.v.y = 0.0;
	go[i].tsd.ship.steering_adjustment.v.z = 0.0;
	go[i].tsd.ship.ncargo_bays = ship_type[shiptype].ncargo_bays;
	memset(go[i].tsd.ship.cargo, 0, sizeof(go[i].tsd.ship.cargo));
	for (cb = 0; cb < go[i].tsd.ship.ncargo_bays; cb++) {
		int item = commodity_sample();
		float qty = (float) snis_randn(99) + 1;
		go[i].tsd.ship.cargo[cb].contents.item = item;
		go[i].tsd.ship.cargo[cb].contents.qty = qty;
		if (snis_randn(10000) < 2000)
			break;
	}
	set_ship_destination(&go[i], 0.0, 0.0, 0.0);
	go[i].tsd.ship.home_planet = choose_ship_home_planet();
	go[i].tsd.ship.auto_respawn = (uint8_t) auto_respawn;
	quat_init_axis(&go[i].tsd.ship.computer_desired_orientation, 0, 1, 0, 0);
	go[i].tsd.ship.computer_steering_time_left = 0;
	go[i].tsd.ship.orbiting_object_id = 0xffffffff;
	if (faction >= 0 && faction < nfactions())
		go[i].sdata.faction = faction;
	if (shiptype == SHIP_CLASS_MANTIS) /* Ensure all Mantis tow ships are neutral faction */
		go[i].sdata.faction = 0;
	ship_name(mt, go[i].sdata.name, sizeof(go[i].sdata.name));
	uppercase(go[i].sdata.name);
	go[i].tsd.ship.fuel = 0; /* TODO: maybe sandbox mode should consume fuel too? */
	go[i].tsd.ship.current_hg_ant_orientation = identity_quat;
	go[i].tsd.ship.desired_hg_ant_aim.v.x = 1;
	go[i].tsd.ship.desired_hg_ant_aim.v.y = 0;
	go[i].tsd.ship.desired_hg_ant_aim.v.z = 0;

	if (go[i].tsd.ship.shiptype == SHIP_CLASS_ENFORCER) {
		snprintf(registration, sizeof(registration) - 1, "EXEMPT - GOVERNMENT SPACECRAFT");
		ship_registry_add_owner(&ship_registry, go[i].id, 0);
	} else {
		snprintf(registration, sizeof(registration) - 1, "PRIVATE SPACECRAFT");
		ship_registry_add_owner(&ship_registry, go[i].id, snis_randn(ncorporations()));
		if (snis_randn(1000) <= 1000.0 * bounty_chance) {
			char crime[100];
			generate_crime(mt, crime, sizeof(crime) - 1);
			uppercase(crime);
			ship_registry_add_bounty(&ship_registry, go[i].id, crime,
				1000.0 + snis_randn(10) * 100.0,
				nth_starbase(snis_randn(NBASES)));
		}
	}
	ship_registry_add_entry(&ship_registry, go[i].id, SHIP_REG_TYPE_REGISTRATION, registration);
	return i;
}

static int add_mining_bot(struct snis_entity *parent_ship, uint32_t asteroid_id,
				int bridge, int selected_waypoint)
{
	int rc;
	struct snis_entity *o;

	rc = add_ship(parent_ship->sdata.faction, SHIP_CLASS_ASTEROIDMINER, 0);
	/* TODO may want to make a special model just for the mining bot */
	if (rc < 0)
		return rc;
	o = &go[rc];
	parent_ship->tsd.ship.mining_bots--; /* maybe we want miningbots to live in cargo hold? */
	push_mining_bot_mode(o, parent_ship->id, asteroid_id, bridge, selected_waypoint);
	snprintf(o->sdata.name, sizeof(o->sdata.name), "%s-MINER-%02d",
		parent_ship->sdata.name, parent_ship->tsd.ship.mining_bots);
	memset(o->sdata.name, 0, sizeof(o->sdata.name));
	strncpy(o->sdata.name, parent_ship->tsd.ship.mining_bot_name, sizeof(o->sdata.name));

	/* TODO make this better: */
	go[rc].x = parent_ship->x + 30;
	go[rc].y = parent_ship->y + 30;
	go[rc].z = parent_ship->z + 30;

	return rc;
}

static int add_specific_ship(const char *name, double x, double y, double z,
			uint8_t shiptype, uint8_t the_faction, int auto_respawn)
{
	int i;

	i = add_ship(the_faction, shiptype % nshiptypes, auto_respawn);
	if (i < 0)
		return i;
	set_object_location(&go[i], x, y, z);
	go[i].sdata.faction = the_faction % nfactions();
	strncpy(go[i].sdata.name, name, sizeof(go[i].sdata.name) - 1);
	return i;
}

static int l_attach_science_text(lua_State *l)
{
	const double id = luaL_checknumber(lua_state, 1);
	const char *text = lua_tostring(lua_state, 2);
	pthread_mutex_lock(&universe_mutex);
	int i = lookup_by_id((uint32_t) id);
	if (i < 0)
		goto error;
	if (go[i].sdata.science_text)
		free(go[i].sdata.science_text);
	go[i].sdata.science_text = strndup(text, 256);
	pthread_mutex_unlock(&universe_mutex);
	lua_pushnumber(lua_state, 0.0);
	return 1;
error:
	pthread_mutex_unlock(&universe_mutex);
	lua_pushnil(lua_state);
	return 1;
}

static int l_add_ship(lua_State *l)
{
	const char *name;
	double x, y, z, ar;
	double shiptype, the_faction;
	int i, auto_respawn;

	name = lua_tostring(lua_state, 1);
	x = lua_tonumber(lua_state, 2);
	y = lua_tonumber(lua_state, 3);
	z = lua_tonumber(lua_state, 4);
	shiptype = lua_tonumber(lua_state, 5);
	the_faction = lua_tonumber(lua_state, 6);
	ar = lua_tonumber(lua_state, 7);
	auto_respawn = (ar > 0.999);

	if (shiptype < 0 || shiptype > nshiptypes - 1) {
		lua_pushnumber(lua_state, -1.0);
		return 1;
	}

	pthread_mutex_lock(&universe_mutex);
	i = add_specific_ship(name, x, y, z,
		(uint8_t) shiptype % nshiptypes,
		(uint8_t) the_faction % nfactions(), auto_respawn);
	lua_pushnumber(lua_state, i < 0 ? -1.0 : (double) go[i].id);
	pthread_mutex_unlock(&universe_mutex);
	return 1;
}

static int add_asteroid(double x, double y, double z, double vx, double vz, double heading, double v);
static int l_add_asteroid(lua_State *l)
{
	double x, y, z;
	int i;

	x = lua_tonumber(lua_state, 1);
	y = lua_tonumber(lua_state, 2);
	z = lua_tonumber(lua_state, 3);

	pthread_mutex_lock(&universe_mutex);
	i = add_asteroid(x, y, z, 0.0, 0.0, 0.0, 1.0);
	lua_pushnumber(lua_state, i < 0 ? -1.0 : (double) go[i].id);
	pthread_mutex_unlock(&universe_mutex);
	return 1;
}

static int l_set_asteroid_speed(lua_State *l)
{
	const double id = luaL_checknumber(lua_state, 1);
	const double v = luaL_checknumber(lua_state, 2);
	pthread_mutex_lock(&universe_mutex);
	int i = lookup_by_id((uint32_t) id);
	if (i < 0)
		goto error;
	if (go[i].type != OBJTYPE_ASTEROID)
		goto error;
	if (v < 0.0 || v > 1.0)
		goto error;
	go[i].tsd.asteroid.v = (float) v;
	pthread_mutex_unlock(&universe_mutex);
	lua_pushnumber(lua_state, 0.0);
	return 1;
error:
	pthread_mutex_unlock(&universe_mutex);
	lua_pushnil(lua_state);
	return 1;
}

static int l_add_cargo_container(lua_State *l)
{
	double x, y, z, vx, vy, vz;
	int i;

	x = lua_tonumber(lua_state, 1);
	y = lua_tonumber(lua_state, 2);
	z = lua_tonumber(lua_state, 3);
	vx = lua_tonumber(lua_state, 4);
	vy = lua_tonumber(lua_state, 5);
	vz = lua_tonumber(lua_state, 6);

	pthread_mutex_lock(&universe_mutex);
	i = add_cargo_container(x, y, z, vx, vy, vz, -1, 0, 1);
	if (i < 0) {
		lua_pushnil(lua_state);
		pthread_mutex_unlock(&universe_mutex);
		return 1;
	}
	lua_pushnumber(lua_state, i < 0 ? -1.0 : (double) go[i].id);
	pthread_mutex_unlock(&universe_mutex);
	return 1;
}

static int l_get_player_ship_ids(lua_State *l)
{
	int i, index;

	index = 1;
	pthread_mutex_lock(&universe_mutex);
	lua_newtable(l);
	for (i = 0; i <= snis_object_pool_highest_object(pool); i++) {
		if (go[i].type == OBJTYPE_SHIP1) {
			lua_pushnumber(l, (double) index);
			lua_pushnumber(l, (double) go[i].id);
			lua_settable(l, -3);
			index++;
		}
	}
	pthread_mutex_unlock(&universe_mutex);
	return 1;
}

static int l_get_object_location(lua_State *l)
{
	int i;
	double id;

	pthread_mutex_lock(&universe_mutex);
	id = lua_tonumber(lua_state, 1);
	i = lookup_by_id((uint32_t) id);
	if (i < 0) {
		pthread_mutex_unlock(&universe_mutex);
		lua_pushnil(lua_state);
		lua_pushnil(lua_state);
		lua_pushnil(lua_state);
		return 3;
	}
	lua_pushnumber(lua_state, go[i].x);
	lua_pushnumber(lua_state, go[i].y);
	lua_pushnumber(lua_state, go[i].z);
	pthread_mutex_unlock(&universe_mutex);
	return 3;
}

static int l_move_object(lua_State *l)
{
	int i;
	double id, x, y, z;
	struct snis_entity *o;

	id = lua_tonumber(lua_state, 1);
	x = lua_tonumber(lua_state, 2);
	y = lua_tonumber(lua_state, 3);
	z = lua_tonumber(lua_state, 4);

	pthread_mutex_lock(&universe_mutex);
	i = lookup_by_id((uint32_t) id);
	if (i < 0) {
		pthread_mutex_unlock(&universe_mutex);
		return 0;
	}
	o = &go[i];
	set_object_location(o, x, y, z);
	pthread_mutex_unlock(&universe_mutex);
	return 0;
}

static int l_set_object_velocity(lua_State *l)
{
	int i;
	double id, vx, vy, vz;
	struct snis_entity *o;

	id = lua_tonumber(lua_state, 1);
	vx = lua_tonumber(lua_state, 2);
	vy = lua_tonumber(lua_state, 3);
	vz = lua_tonumber(lua_state, 4);

	pthread_mutex_lock(&universe_mutex);
	i = lookup_by_id((uint32_t) id);
	if (i < 0) {
		pthread_mutex_unlock(&universe_mutex);
		return 0;
	}
	o = &go[i];
	set_object_velocity(o, vx, vy, vz);
	pthread_mutex_unlock(&universe_mutex);
	return 0;
}

static int l_set_object_orientation(lua_State *l)
{
	int i;
	double id, rotx, roty, rotz, angle;

	id = lua_tonumber(lua_state, 1);
	rotx = lua_tonumber(lua_state, 2);
	roty = lua_tonumber(lua_state, 3);
	rotz = lua_tonumber(lua_state, 4);
	angle = lua_tonumber(lua_state, 5);

	pthread_mutex_lock(&universe_mutex);
	i = lookup_by_id((uint32_t) id);
	if (i < 0) {
		pthread_mutex_unlock(&universe_mutex);
		return 0;
	}
	quat_init_axis(&go[i].orientation, rotx, roty, rotz, angle);
	go[i].timestamp = universe_timestamp;
	pthread_mutex_unlock(&universe_mutex);
	return 0;
}

static int l_align_object_towards(lua_State *l)
{
	int i;
	double id, x, y, z, max_angle;
	float axisx, axisy, axisz, unlimited_angle, angle;
	union vec3 start, goal;
	union quat rotation, new_orientation;

	id = lua_tonumber(lua_state, 1);
	x = lua_tonumber(lua_state, 2);
	y = lua_tonumber(lua_state, 3);
	z = lua_tonumber(lua_state, 4);
	max_angle = fabs(lua_tonumber(lua_state, 5));

	pthread_mutex_lock(&universe_mutex);
	i = lookup_by_id((uint32_t) id);
	if (i < 0) {
		pthread_mutex_unlock(&universe_mutex);
		lua_pushnumber(lua_state, -720.0 * M_PI / 180.0);
		return 1;
	}
	start.v.x = 1.0;
	start.v.y = 0.0;
	start.v.z = 0.0;
	quat_rot_vec_self(&start, &go[i].orientation);

	goal.v.x = x - go[i].x;
	goal.v.y = y - go[i].y;
	goal.v.z = z - go[i].z;
	vec3_normalize_self(&goal);

	quat_from_u2v(&rotation, &start, &goal, NULL);

	quat_to_axis(&rotation, &axisx, &axisy, &axisz, &angle);
	unlimited_angle = angle;

	if (angle < max_angle)
		angle = -max_angle;
	else if (angle > max_angle)
		angle = max_angle;

	quat_init_axis(&rotation, axisx, axisy, axisz, angle);
	quat_mul(&new_orientation, &go[i].orientation, &rotation);
	quat_normalize_self(&new_orientation);
	go[i].orientation = new_orientation;
	go[i].timestamp = universe_timestamp;
	pthread_mutex_unlock(&universe_mutex);
	lua_pushnumber(lua_state, unlimited_angle);
	return 1;
}

static int l_set_object_rotational_velocity(lua_State *l)
{
	int i;
	double id, rotx, roty, rotz, angle;
	struct snis_entity *o;

	id = lua_tonumber(lua_state, 1);
	rotx = lua_tonumber(lua_state, 2);
	roty = lua_tonumber(lua_state, 3);
	rotz = lua_tonumber(lua_state, 4);
	angle = lua_tonumber(lua_state, 5);

	pthread_mutex_lock(&universe_mutex);
	i = lookup_by_id((uint32_t) id);
	if (i < 0) {
		pthread_mutex_unlock(&universe_mutex);
		return 0;
	}
	o = &go[i];
	/* note that we can't set this for a lot of things because it's done client side
	 * or doesn't have a rotational velocity at all.
	 */
	switch (o->type) {
	case OBJTYPE_BLOCK:
		/* This is the main use case for this function. */
		quat_init_axis(&go[i].tsd.block.rotational_velocity, rotx, roty, rotz, angle);
		o->timestamp = universe_timestamp;
		break;
	case OBJTYPE_ASTEROID:
	case OBJTYPE_CARGO_CONTAINER:
	case OBJTYPE_PLANET:
		/* These have rotational velocities, but they're done client side. */
		break;
	case OBJTYPE_TURRET:
		/* Only works if turret if free floating, not attached to a block. */
		quat_init_axis(&go[i].tsd.turret.rotational_velocity, rotx, roty, rotz, angle);
		o->timestamp = universe_timestamp;
		break;
	default:
		break;
	}
	pthread_mutex_unlock(&universe_mutex);
	return 0;
}

static int l_set_object_relative_position(lua_State *l)
{
	int i;
	double id, rx, ry, rz;
	struct snis_entity *o;

	id = lua_tonumber(lua_state, 1);
	rx = lua_tonumber(lua_state, 2);
	ry = lua_tonumber(lua_state, 3);
	rz = lua_tonumber(lua_state, 4);

	pthread_mutex_lock(&universe_mutex);
	i = lookup_by_id((uint32_t) id);
	if (i < 0) {
		pthread_mutex_unlock(&universe_mutex);
		return 0;
	}
	o = &go[i];
	/* note that this only works for OBJTYPE_BLOCK for now. */
	switch (o->type) {
	case OBJTYPE_BLOCK:
		/* This is the main use case for this function. */
		o->tsd.block.dx = rx;
		o->tsd.block.dy = ry;
		o->tsd.block.dz = rz;
		o->timestamp = universe_timestamp;
		break;
	default:
		break;
	}
	pthread_mutex_unlock(&universe_mutex);
	return 0;
}


static int l_delete_object(lua_State *l)
{
	int i;
	double id;

	id = lua_tonumber(lua_state, 1);

	pthread_mutex_lock(&universe_mutex);
	i = lookup_by_id((uint32_t) id);
	if (i < 0) {
		pthread_mutex_unlock(&universe_mutex);
		return 0;
	}
	delete_from_clients_and_server(&go[i]);
	pthread_mutex_unlock(&universe_mutex);
	return 0;
}

static int l_add_random_ship(lua_State *l)
{
	int i;

	pthread_mutex_lock(&universe_mutex);
	i = add_ship(-1, snis_randn(nshiptypes), 1);
	lua_pushnumber(lua_state, i >= 0 ? (double) go[i].id : -1.0);
	pthread_mutex_unlock(&universe_mutex);
	return 1;
}

static int add_spacemonster(double x, double y, double z)
{
	int i;
	float dx, dy, dz, v;

	i = add_generic_object(x, y, z, 0.0, 0.0, 0.0, 0.0, OBJTYPE_SPACEMONSTER);
	if (i < 0)
		return i;
	go[i].tsd.spacemonster.seed = snis_randn(1000);
	go[i].tsd.spacemonster.head_size = snis_randn(255);
	go[i].tsd.spacemonster.tentacle_size = snis_randn(255);
	go[i].move = spacemonster_move;
	strncpy(go[i].sdata.name, "M. MYSTERIUM", sizeof(go[i].sdata.name) - 1);
	random_point_on_sphere(1.0, &dx, &dy, &dz);
	v = snis_randn(1000) / 1000.0;
	v = v * max_spacemonster_velocity;
	go[i].vx = v * dx;
	go[i].vy = v * dy;
	go[i].vz = v * dz;
	go[i].tsd.spacemonster.fear = 0;
	go[i].tsd.spacemonster.anger = 0;
	go[i].tsd.spacemonster.health = 255;
	go[i].tsd.spacemonster.hunger = 0;
	go[i].tsd.spacemonster.nearest_spacemonster = -1;
	go[i].tsd.spacemonster.toughness = snis_randn(50); /* out of 255 */
	update_ship_orientation(&go[i]);

	go[i].tsd.spacemonster.home = (uint32_t) -1;
	return i;
}

static int l_add_spacemonster(lua_State *l)
{
	double x, y, z;
	const char *name;
	int i;

	name = lua_tostring(lua_state, 1);
	x = lua_tonumber(lua_state, 2);
	y = lua_tonumber(lua_state, 3);
	z = lua_tonumber(lua_state, 4);

	pthread_mutex_lock(&universe_mutex);
	i = add_spacemonster(x, y, z);
	if (i < 0) {
		pthread_mutex_unlock(&universe_mutex);
		lua_pushnumber(lua_state, -1.0);
		return 1;
	}
	strncpy(go[i].sdata.name, name, sizeof(go[i].sdata.name) - 1);
	lua_pushnumber(lua_state, (double) go[i].id);
	pthread_mutex_unlock(&universe_mutex);
	return 1;
}

static int add_cargo_container(double x, double y, double z, double vx, double vy, double vz,
			int item, float qty, int persistent)
{
	int i;

	i = add_generic_object(x, y, z, vx, vy, vz, 0, OBJTYPE_CARGO_CONTAINER);
	if (i < 0)
		return i;
	go[i].sdata.shield_strength = 0;
	go[i].sdata.shield_wavelength = 0;
	go[i].sdata.shield_width = 0;
	go[i].sdata.shield_depth = 0;
	go[i].move = cargo_container_move;
	go[i].alive = CARGO_CONTAINER_LIFETIME;
	if (item < 0) {
		/* TODO: something better for container contents */
		item = commodity_sample();
		qty = (float) snis_randn(99) + 1;
	}
	go[i].tsd.cargo_container.contents.item = item;
	go[i].tsd.cargo_container.contents.qty = qty;
	go[i].tsd.cargo_container.persistent = persistent & 0xff;
	return i;
}

static void normalize_percentage(uint8_t *v, int total)
{
	float fraction = (float) *v / (float) total;
	*v = (uint8_t) (fraction * 255.0);
}

static int set_asteroid_minerals(uint32_t id, float carbon, float silicates, float nickeliron, float preciousmetals)
{
	float total;
	int i;
	struct snis_entity *o;

	i = lookup_by_id(id);
	if (i < 0)
		return -1;
	o = &go[i];
	if (o->type != OBJTYPE_ASTEROID)
		return -1;

	carbon = fabsf(carbon);
	silicates = fabsf(silicates);
	nickeliron = fabsf(nickeliron);
	preciousmetals = fabsf(preciousmetals);
	total = carbon + silicates + nickeliron + preciousmetals;

	if (total < 0.0001) {
		total += 1.0;
		carbon += 1.0;
	}

	o->tsd.asteroid.carbon = (uint8_t) (255.0 * carbon);
	o->tsd.asteroid.silicates = (uint8_t) (255.0 * silicates);
	o->tsd.asteroid.nickeliron = (uint8_t) (255.0 * nickeliron);
	o->tsd.asteroid.preciousmetals = (uint8_t) (255.0 * preciousmetals);

	normalize_percentage(&go[i].tsd.asteroid.carbon, total);
	normalize_percentage(&go[i].tsd.asteroid.silicates, total);
	normalize_percentage(&go[i].tsd.asteroid.nickeliron, total);
	normalize_percentage(&go[i].tsd.asteroid.preciousmetals, total);
	return 0;
}

static int add_asteroid(double x, double y, double z, double vx, double vz, double heading, double velocity)
{
	int i, n, v;
	int total;

	i = add_generic_object(x, y, z, vx, 0.0, vz, heading, OBJTYPE_ASTEROID);
	if (i < 0)
		return i;
	go[i].sdata.shield_strength = 0;
	go[i].sdata.shield_wavelength = 0;
	go[i].sdata.shield_width = 0;
	go[i].sdata.shield_depth = 0;
	go[i].move = asteroid_move;
	go[i].vx = snis_random_float() * ASTEROID_SPEED * 2.0 - ASTEROID_SPEED;
	go[i].vz = snis_random_float() * ASTEROID_SPEED * 2.0 - ASTEROID_SPEED;
	go[i].tsd.asteroid.v = velocity;
	go[i].alive = snis_randn(10) + 5;
	go[i].orientation = random_spin[go[i].id % NRANDOM_ORIENTATIONS];
	go[i].tsd.asteroid.rotational_velocity = random_spin[go[i].id % NRANDOM_SPINS];

	/* FIXME: make these distributions better */
	total = 0;
	n = 256; v = snis_randn(200) + 10; total += v;
	go[i].tsd.asteroid.carbon = v;
	n = n - v; v = snis_randn(n); total += v;
	go[i].tsd.asteroid.silicates = v;
	n = n - v; v = snis_randn(n); total += v;
	go[i].tsd.asteroid.nickeliron = v;
	n = n - v; v = snis_randn(n); total += v;
	go[i].tsd.asteroid.preciousmetals = v;

	normalize_percentage(&go[i].tsd.asteroid.carbon, total);
	normalize_percentage(&go[i].tsd.asteroid.silicates, total);
	normalize_percentage(&go[i].tsd.asteroid.nickeliron, total);
	normalize_percentage(&go[i].tsd.asteroid.preciousmetals, total);

	return i;
}

static int add_warp_core(double x, double y, double z,
				double vx, double vy, double vz,
				uint16_t countdown_timer, uint32_t ship_id)
{
	int i;

	i = add_generic_object(x, y, z, vx, vy, vz, 0.0, OBJTYPE_WARP_CORE);
	if (i < 0)
		return i;
	go[i].sdata.shield_strength = 0;
	go[i].sdata.shield_wavelength = 0;
	go[i].sdata.shield_width = 0;
	go[i].sdata.shield_depth = 0;
	go[i].move = warp_core_move;
	go[i].vx = vx;
	go[i].vy = vy;
	go[i].vz = vz;
	go[i].tsd.warp_core.countdown_timer = countdown_timer;
	go[i].tsd.warp_core.ship_id = ship_id;
	go[i].alive = 1;
	go[i].orientation = random_spin[go[i].id % NRANDOM_ORIENTATIONS];
	go[i].tsd.warp_core.rotational_velocity = random_spin[go[i].id % NRANDOM_SPINS];
	return i;
}

static int mkt_item_already_present(struct marketplace_data *mkt, int nitems, int item)
{
	int i;

	for (i = 0; i < nitems; i++)
		if (mkt[i].item == item)
			return 1;
	return 0;
}

static int commodity_sample(void)
{
	static struct nonuniform_sample_distribution *d = NULL;
	int i;

	if (d)
		return nonuniform_sample(d);

	d = nonuniform_sample_distribution_init(ncommodities, 77277);
	for (i = 0; i < ncommodities; i++)
		nonuniform_sample_add_item(d, i, commodity[i].odds);
	return nonuniform_sample(d);
}

static float calculate_commodity_price(struct snis_entity *planet, int item)
{
	float economy, tech_level, government;
#if 0
	int i;
	static int test_done = 0;
	float price;

	if (!test_done) {

		for (i = 0; i < ARRAYSIZE(economy_name); i++) {
			economy = 1.0f - (float) i / (float) ARRAYSIZE(economy_name);
			tech_level = 0.5f;
			government = 0.5f;
			price = commodity_calculate_price(&commodity[item],
					economy, tech_level, government);
			printf("economy %d: %.2f %s\n", i, price, commodity[item].name);
		}
		for (i = 0; i < ARRAYSIZE(tech_level_name); i++) {
			economy = 0.5f;
			tech_level = 1.0f - (float) i / (float) ARRAYSIZE(tech_level_name);
			government = 0.5f;
			price = commodity_calculate_price(&commodity[item],
					economy, tech_level, government);
			printf("tech level %d: %.2f %s\n", i, price, commodity[item].name);
		}
		for (i = 0; i < ARRAYSIZE(government_name); i++) {
			economy = 0.5f;
			tech_level = 0.5f;
			government = 1.0f - (float) i / (float) ARRAYSIZE(government_name);
			price = commodity_calculate_price(&commodity[item],
					economy, tech_level, government);
			printf("government %d: %.2f %s\n", i, price, commodity[item].name);
		}
		test_done = 1;
	}
#endif

	/* economy, tech_level, government will be between 0.0 and 1.0 indicating the
	 * "wealthiness", "techiness", and "government stability" of the planet,
	 * respectively.
	 */
	if (planet) {
		economy = 1.0f - (float) planet->tsd.planet.economy /
					(float) ARRAYSIZE(economy_name);
		tech_level = 1.0f - (float) planet->tsd.planet.tech_level /
					(float) ARRAYSIZE(tech_level_name);
		government = 1.0f - (float) planet->tsd.planet.government /
					(float) ARRAYSIZE(government_name);
	} else {
		/* Deep space starbases will be in top 10% of everything, let's say */
		economy = 1.0 - snis_randn(100) / 1000;
		tech_level = 1.0 - snis_randn(100) / 1000;
		government = 1.0 - snis_randn(100) / 1000;
	}
	return commodity_calculate_price(&commodity[item], economy, tech_level, government);
}

static void init_starbase_market(struct snis_entity *o)
{
	int i;
	struct marketplace_data *mkt = NULL;

	o->tsd.starbase.mkt = NULL;
	if (ncommodities == 0)
		return;
	mkt = malloc(sizeof(*mkt) * COMMODITIES_PER_BASE);
	o->tsd.starbase.mkt = mkt;
	if (!mkt)
		return;
	for (i = 0; i < COMMODITIES_PER_BASE; i++) {
		int item;
		do {
			item = commodity_sample();
		} while (mkt_item_already_present(mkt, i, item)); 
		mkt[i].item = item;
		mkt[i].qty = snis_randn(99) + 1; /* TODO: something better */
		mkt[i].refill_rate = (float) snis_randn(1000) / 1000.0; /* TODO: something better */
		mkt[i].bid = o->tsd.starbase.bid_price[item];
		mkt[i].ask = (float) 1.1 * mkt[i].bid;
	}
}

static void fabricate_prices(struct snis_entity *starbase)
{
	int i, j;
	float variation;
	struct snis_entity *planet;

	/* lookup associated planet, if any... */
	if (starbase->tsd.starbase.associated_planet_id < 0) {
		planet = NULL;
	} else {
		i = lookup_by_id(starbase->tsd.starbase.associated_planet_id);
		if (i < 0)
			planet = NULL;
		else
			planet = &go[i];
	}

	/* calculate commodity prices at this starbase based on assoc. planet */
	for (i = 0; i < ncommodities; i++)
		starbase->tsd.starbase.bid_price[i] =
			calculate_commodity_price(planet, i);
	starbase->tsd.starbase.part_price = malloc(sizeof(*starbase->tsd.starbase.part_price) *
						((DAMCON_SYSTEM_COUNT - 1) * DAMCON_PARTS_PER_SYSTEM));
	for (i = 0; i < DAMCON_SYSTEM_COUNT - 1; i++) {
		for (j = 0; j < DAMCON_PARTS_PER_SYSTEM; j++) {
			variation = 1.0f + (float) (snis_randn(200) - 100) / 1000.0f;
			starbase->tsd.starbase.part_price[i * DAMCON_PARTS_PER_SYSTEM + j] =
				damcon_base_price(i, j) * variation;
		}
	}
}

static uint32_t find_root_id(int parent_id)
{
	/* TODO: Detect cycles. */
	int i;
	uint32_t id;
	if (parent_id == (uint32_t) -1) /* am I root? */
		return -1;
	id = parent_id;
	do {
		i = lookup_by_id(id);
		if (i < 0) /* shouldn't happen */
			return i;
		if (go[i].type != OBJTYPE_BLOCK)
			return id;
		if (go[i].tsd.block.parent_id == (uint32_t) -1)
			return id;
		id = go[i].tsd.block.parent_id;
	} while (1);
}

static int add_turret(int parent_id, double x, double y, double z,
			double dx, double dy, double dz,
			union quat relative_orientation, union vec3 up,
			int turret_fire_interval)
{
	int i;
	i = add_generic_object(x, y, z, 0.0, 0.0, 0.0, 0.0, OBJTYPE_TURRET);
	if (i < 0)
		return i;
	go[i].tsd.turret.parent_id = parent_id;
	go[i].tsd.turret.root_id = find_root_id(parent_id);
	go[i].tsd.turret.dx = dx;
	go[i].tsd.turret.dy = dy;
	go[i].tsd.turret.dz = dz;
	go[i].tsd.turret.relative_orientation = relative_orientation;
	go[i].tsd.turret.health = 255;
	go[i].tsd.turret.rotational_velocity = random_spin[go[i].id % NRANDOM_SPINS];
	go[i].tsd.turret.up_direction = up;
	go[i].tsd.turret.fire_countdown = 0;
	if (turret_fire_interval < 0)
		go[i].tsd.turret.fire_countdown_reset_value = TURRET_FIRE_INTERVAL;
	else
		go[i].tsd.turret.fire_countdown_reset_value = turret_fire_interval;
	go[i].move = turret_move;
	return i;
}

static int l_add_turret(lua_State *l)
{
	double rid, x, y, z, firing_interval;
	uint32_t parent_id;
	union vec3 up = { { 0.0, 0.0, 1.0 } };
	int i;

	rid = lua_tonumber(lua_state, 1);
	x = lua_tonumber(lua_state, 2);
	y = lua_tonumber(lua_state, 3);
	z = lua_tonumber(lua_state, 4);
	firing_interval = lua_tonumber(lua_state, 5);

	pthread_mutex_lock(&universe_mutex);
	parent_id = (uint32_t) rid;
	i = lookup_by_id(parent_id);
	if (i < 0) {
		pthread_mutex_unlock(&universe_mutex);
		lua_pushnumber(lua_state, -1.0);
		return 1;
	}
	i = add_turret(parent_id, 0.0, 0.0, 0.0, x, y, z, identity_quat, up,
			(int) firing_interval);
	lua_pushnumber(lua_state, i < 0 ? -1.0 : (double) go[i].id);
	pthread_mutex_unlock(&universe_mutex);
	return 1;
}

static int add_block_object(int parent_id, double x, double y, double z,
				double vx, double vy, double vz,
				double dx, double dy, double dz, /* displacement from parent */
				double sx, double sy, double sz, /* nonuniform scaling */
				union quat relative_orientation,
				uint8_t block_material_index, uint8_t form)
{
	int i;
	i = add_generic_object(x, y, z, vx, vy, vz, 0.0, OBJTYPE_BLOCK);
	if (i < 0)
		return i;
	go[i].tsd.block.parent_id = parent_id;
	go[i].tsd.block.root_id = find_root_id(parent_id);
	go[i].tsd.block.dx = dx;
	go[i].tsd.block.dy = dy;
	go[i].tsd.block.dz = dz;
	go[i].tsd.block.health = 255; /* immortal */
	go[i].tsd.block.rotational_velocity = random_spin[go[i].id % NRANDOM_SPINS];
	go[i].tsd.block.relative_orientation = relative_orientation;
	if (form == SHAPE_SPHERE) {
		/* Force a sphere, forbid ellipsoids */
		if (sx >= sy && sx >= sz)
			shape_init_sphere(&go[i].tsd.block.shape, 0.5 * sx);
		else if (sy >= sx && sy >= sz)
			shape_init_sphere(&go[i].tsd.block.shape, 0.5 * sy);
		else
			shape_init_sphere(&go[i].tsd.block.shape, 0.5 * sz);
	} else if (form == SHAPE_CUBOID) {
		shape_init_cuboid(&go[i].tsd.block.shape, sx, sy, sz);
	} else if (form == SHAPE_CAPSULE) {
		if (sy >= sz)
			shape_init_capsule(&go[i].tsd.block.shape, sx, sy);
		else
			shape_init_capsule(&go[i].tsd.block.shape, sx, sz);
	}
	go[i].tsd.block.block_material_index = block_material_index;
	memset(&go[i].tsd.block.naughty_list, 0xff, sizeof(go[i].tsd.block.naughty_list));
	go[i].move = block_move;
	block_calculate_obb(&go[i], &go[i].tsd.block.shape.cuboid.obb);
	return i;
}

static int l_add_block(lua_State *l)
{
	double rid, x, y, z, sx, sy, sz, rotx, roty, rotz, angle, material_index, form;
	double dx, dy, dz;
	int parent_id;
	union quat rotation;
	int i;

	rid = lua_tonumber(lua_state, 1);
	x = lua_tonumber(lua_state, 2);
	y = lua_tonumber(lua_state, 3);
	z = lua_tonumber(lua_state, 4);
	sx = lua_tonumber(lua_state, 5);
	sy = lua_tonumber(lua_state, 6);
	sz = lua_tonumber(lua_state, 7);
	rotx = lua_tonumber(lua_state, 8);
	roty = lua_tonumber(lua_state, 9);
	rotz = lua_tonumber(lua_state, 10);
	angle = lua_tonumber(lua_state, 11);
	material_index = lua_tonumber(lua_state, 12);
	form = lua_tonumber(lua_state, 13);
	dx = 0.0;
	dy = 0.0;
	dz = 0.0;

	if ((int) material_index != 0 && (int) material_index != 1) {
		lua_pushnumber(lua_state, -1.0);
		return 1;
	}
	switch ((int) form) {
	case SHAPE_CUBOID:
	case SHAPE_SPHERE:
	case SHAPE_CAPSULE:
		break;
	default:
		lua_pushnumber(lua_state, -1.0);
		return 1;
	}
	quat_init_axis(&rotation, rotx, roty, rotz, angle);

	parent_id = (int) rid;
	pthread_mutex_lock(&universe_mutex);
	if (parent_id >= 0) {
		i = lookup_by_id(parent_id);
		if (i >= 0) {
			dx = x;
			dy = y;
			dz = z;
			x = 0.0;
			y = 0.0;
			z = 0.0;
		}
	}
	i = add_block_object(parent_id, x, y, z, 0.0, 0.0, 0.0, dx, dy, dz, sx, sy, sz, rotation,
				(int) material_index, (int) form);
	lua_pushnumber(lua_state, i < 0 ? -1.0 : (double) go[i].id);
	pthread_mutex_unlock(&universe_mutex);
	return 1;
}

static int add_subblock(int parent_id, double scalefactor, double sx, double sy, double sz, /* nonuniform scaling */
			double dx, double dy, double dz, /* displacement from parent */
			uint8_t block_material_index, uint8_t form)
{
	const double s = scalefactor;
	int i;

	i = add_block_object(parent_id, 0, 0, 0, 0, 0, 0,
				dx * s, dy * s, dz * s,
				sx * s, sy * s, sz * s, identity_quat,
				block_material_index, form);
	return i;
}

static void add_turrets_to_block_face(int parent_id, int face, int rows, int cols)
{
	int i, j, index;
	const double xoff[] = { 1, -1, 0, 0, 0, 0 };
	const double yoff[] = { 0,  0, -1, 1, 0, 0 };
	const double zoff[] = { 0,  0, 0, 0, 1, -1 };
	double x, y, z, xo, yo, zo, xrowstep, xcolstep, yrowstep, ycolstep, zrowstep, zcolstep;
	struct snis_entity *block;
	const double turret_offset = 350.0;
	double platformsx, platformsy, platformsz;
	double platformxo, platformyo, platformzo;
	union vec3 absolute_up = { { 0.0, 0.0, 1.0 } };
	union quat rest_orientation;
	union vec3 up;

	face = abs(face) % 6;

	up.v.x = xoff[face];
	up.v.y = yoff[face];
	up.v.z = zoff[face];


	index = lookup_by_id(parent_id);
	if (index < 0)
		return;
	block = &go[index];
	if (block->type != OBJTYPE_BLOCK)
		return;
	xo = xoff[face] * block->tsd.block.shape.cuboid.sx * 0.5 + turret_offset * xoff[face];
	yo = yoff[face] * block->tsd.block.shape.cuboid.sy * 0.5 + turret_offset * yoff[face];
	zo = zoff[face] * block->tsd.block.shape.cuboid.sz * 0.5 + turret_offset * zoff[face];

	if (xoff[face] > 0.0)
		quat_init_axis(&rest_orientation, 0, 0, 1, -0.5 * M_PI);
	else if (xoff[face] < 0.0)
		quat_init_axis(&rest_orientation, 0, 0, 1, 0.5 * M_PI);
	else if (yoff[face] > 0.0)
		quat_init_axis(&rest_orientation, 0, 0, 1, 0.0 * M_PI);
	else if (yoff[face] < 0.0)
		quat_init_axis(&rest_orientation, 0, 0, 1, 1.0 * M_PI);
	else if (zoff[face] > 0.0)
		quat_init_axis(&rest_orientation, 1, 0, 0, 0.5 * M_PI);
	else if (zoff[face] < 0.0)
		quat_init_axis(&rest_orientation, 1, 0, 0, -0.5 * M_PI);
	quat_rot_vec(&up, &absolute_up, &rest_orientation);

	platformsx = turret_offset * 0.20 + turret_offset * 0.80 * fabs(xoff[face]);
	platformsy = turret_offset * 0.20 + turret_offset * 0.80 * fabs(yoff[face]);
	platformsz = turret_offset * 0.20 + turret_offset * 0.80 * fabs(zoff[face]);

	platformxo = -turret_offset * 0.55 * xoff[face];
	platformyo = -turret_offset * 0.55 * yoff[face];
	platformzo = -turret_offset * 0.55 * zoff[face];

	xrowstep = 0.0;
	yrowstep = 0.0;
	zrowstep = 0.0;
	xcolstep = 0.0;
	ycolstep = 0.0;
	zcolstep = 0.0;

	if (fabs(xo) > 0.01) {
		/* y,z  plane */
		printf("y z plane\n");
		yrowstep = block->tsd.block.shape.cuboid.sy / (1.0 + rows);
		zcolstep = block->tsd.block.shape.cuboid.sz / (1.0 + cols);
		printf("yrowstep = %lf\n", yrowstep);
		printf("zcolstep = %lf\n", zcolstep);
		printf("block->tsd.block.sy = %lf\n", block->tsd.block.shape.cuboid.sy);
		printf("block->tsd.block.sz = %lf\n", block->tsd.block.shape.cuboid.sz);
	} else if (fabs(yo) > 0.01) {
		printf("x z plane\n");
		/* x,z  plane */
		xrowstep = block->tsd.block.shape.cuboid.sx / (1.0 + rows);
		zcolstep = block->tsd.block.shape.cuboid.sz / (1.0 + cols);
	} else {
		/* x,y  plane */
		printf("x y plane\n");
		xrowstep = block->tsd.block.shape.cuboid.sx / (1.0 + rows);
		ycolstep = block->tsd.block.shape.cuboid.sy / (1.0 + cols);
	}

	printf("xcs, ycs, zcs, xrs, yrs, zrs = %lf, %lf, %lf, %lf, %lf, %lf\n",
		xcolstep, ycolstep, zcolstep, xrowstep, yrowstep, zrowstep);

	for (i = 0; i < rows; i++) {
		double row = i;
		double rowfactor = 0.5 + row - 0.5 * rows;
		for (j = 0; j < cols; j++) {
			int block;
			double col = j;
			double colfactor = 0.5 + col - 0.5 * cols;
			x = rowfactor * xrowstep + colfactor * xcolstep + xo;
			y = rowfactor * yrowstep + colfactor * ycolstep + yo;
			z = rowfactor * zrowstep + colfactor * zcolstep + zo;
			block = add_subblock(parent_id, 1.0, platformsx, platformsy, platformsz,
						x + platformxo, y + platformyo, z + platformzo, 1,
						SHAPE_CUBOID);
			if (block >= 0) {
				printf("ADDING TURRET parent = %d, %lf, %lf, %lf\n", go[block].id, x, y, z); 
				add_turret(go[block].id, 0, 0, 0,
						platformsx * 0.65 * xoff[face],
						platformsy * 0.65 * yoff[face],
						platformsz * 0.65 * zoff[face], rest_orientation, up, -1);
				go[block].tsd.block.health = 121; /* mortal */
			}
		}
	}
}

static int l_add_turrets_to_block_face(lua_State *l)
{
	double rid, dface, drows, dcols;
	uint32_t parent_id;
	int i, face, rows, cols;

	rid = lua_tonumber(lua_state, 1);
	dface = lua_tonumber(lua_state, 2);
	drows = lua_tonumber(lua_state, 3);
	dcols = lua_tonumber(lua_state, 4);

	pthread_mutex_lock(&universe_mutex);
	parent_id = (uint32_t) rid;
	i = lookup_by_id(parent_id);
	if (i < 0) {
		pthread_mutex_unlock(&universe_mutex);
		lua_pushnumber(lua_state, -1.0);
		return 1;
	}
	face = (int) dface;
	rows = (int) drows;
	cols = (int) dcols;
	add_turrets_to_block_face(parent_id, face, rows, cols);
	pthread_mutex_unlock(&universe_mutex);
	lua_pushnumber(lua_state, 0.0);
	return 1;
}

static int add_docking_port(int parent_id, int portnumber)
{
	int i, p, model;
	struct snis_entity *parent;
	struct docking_port_attachment_point *dp;
	union vec3 pos;
	union quat orientation;

	/* Find the thing we're adding a docking port to */
	p = lookup_by_id(parent_id);
	if (p < 0) {
		fprintf(stderr, "Failed to find docking port's parent id\n");
		return -1;
	}
	parent = &go[p];

	/* Check if this is the sort of thing which can have a docking port */
	switch (parent->type) {
	case OBJTYPE_STARBASE:
		break;
	default:
		fprintf(stderr, "docking port parent->type is wrong\n");
		return -1;
	}

	/* Get the relevent docking port info for this thing */
	model = parent_id % nstarbase_models;
	dp = docking_port_info[model];
	if (!dp) {
		fprintf(stderr, "no docking port info for this model: %d\n", model);
		return -1;
	}

	if (portnumber >= dp->nports) { /* validate the docking port number */
		fprintf(stderr, "bad docking port number %d >= %d\n",
			portnumber, dp->nports);
		return -1;
	}

	pos.v.x = parent->x;
	pos.v.y = parent->y;
	pos.v.z = parent->z;
	vec3_add_self(&pos, &dp->port[portnumber].pos);
	quat_mul(&orientation, &parent->orientation, &dp->port[portnumber].orientation);

	i = add_generic_object(pos.v.x, pos.v.y, pos.v.z, parent->vx, parent->vy, parent->vz,
			parent->heading, OBJTYPE_DOCKING_PORT);
	if (i < 0) {
		fprintf(stderr, "Failed to add docking port\n");
		return i;
	}
	go[i].move = docking_port_move;
	go[i].type = OBJTYPE_DOCKING_PORT;
	go[i].tsd.docking_port.parent = parent_id;
	go[i].tsd.docking_port.model = model;
	go[i].tsd.docking_port.portnumber = portnumber;
	go[i].tsd.docking_port.docked_guy = (uint32_t) -1;
	go[i].timestamp = universe_timestamp;
	return i;
}

static void setup_randomized_starbase_orbit(struct snis_entity *o)
{
	int i;
	struct snis_entity *p;
	float x, y, z;

	/* Initialized everything regardless of whether there's an associated planet */
	random_point_on_sphere(1.0, &x, &y, &z);
	o->tsd.starbase.orbital_axis.v.x = x;
	o->tsd.starbase.orbital_axis.v.y = y;
	o->tsd.starbase.orbital_axis.v.z = z;
	o->tsd.starbase.altitude = 0;
	o->tsd.starbase.orbital_angle = snis_randn(360) * M_PI / 180.0;
	o->tsd.starbase.orbital_velocity = 0.05 * (M_PI / 180.0);
	if (o->tsd.starbase.associated_planet_id == -1)
		return;
	i = lookup_by_id(o->tsd.starbase.associated_planet_id);
	if (i < 0) {
		o->tsd.starbase.associated_planet_id = -1;
		return;
	}
	p = &go[i];

	/* Modify the angular velocity so that the speed is more or less constant
	 * regardless of altitude
	 */
	o->tsd.starbase.altitude = p->tsd.planet.radius * 1.3 + 800.0f + snis_randn(1400);
	o->tsd.starbase.orbital_velocity = o->tsd.starbase.orbital_velocity * 10000.0 / o->tsd.starbase.altitude;
}

static int add_starbase(double x, double y, double z,
			double vx, double vz, double heading, int n, uint32_t assoc_planet_id)
{
	int i, j, model;

	i = add_generic_object(x, y, z, vx, 0.0, vz, heading, OBJTYPE_STARBASE);
	if (i < 0)
		return i;
	if (n < 0)
		n = -n;
	n %= 99;
	go[i].move = starbase_move;
	go[i].type = OBJTYPE_STARBASE;
	go[i].tsd.starbase.last_time_called_for_help = 0;
	go[i].tsd.starbase.under_attack = 0;
	go[i].tsd.starbase.lifeform_count = snis_randn(100) + 100;
	go[i].tsd.starbase.associated_planet_id = assoc_planet_id;
	go[i].sdata.shield_strength = 255;
	go[i].tsd.starbase.spin_rate_10ths_deg_per_sec = snis_randn(130) + 20;
	go[i].tsd.starbase.bid_price = malloc(sizeof(*go[i].tsd.starbase.bid_price) * ncommodities);
	go[i].tsd.starbase.occupant[0] = 255;
	go[i].tsd.starbase.occupant[1] = 255;
	go[i].tsd.starbase.occupant[2] = 255;
	go[i].tsd.starbase.occupant[3] = 255;
	go[i].tsd.starbase.time_left_to_build = 0;
	go[i].tsd.starbase.build_unit_type = 255;
	go[i].tsd.starbase.starbase_number = n;
	go[i].tsd.starbase.factions_allowed = ALL_FACTIONS_ALLOWED;
	fabricate_prices(&go[i]);
	init_starbase_market(&go[i]);
	snprintf(go[i].sdata.name, sizeof(go[i].sdata.name), "SB-%02d", n);

	model = go[i].id % nstarbase_models;
	if (docking_port_info[model]) {
		for (j = 0; j < docking_port_info[model]->nports; j++) {
			int dpi = add_docking_port(go[i].id, j);
			if (dpi >= 0) {
				go[i].tsd.starbase.docking_port[j] = go[dpi].id;
				if (go[dpi].type != OBJTYPE_DOCKING_PORT) {
					fprintf(stderr,
						"docking port is wrong type model=%d, port = %d\n",
						model, j);
				}
			}
			go[i].tsd.starbase.expected_docker[j] = -1;
			go[i].tsd.starbase.expected_docker_timer[j] = 0;
		}
	}
	setup_randomized_starbase_orbit(&go[i]);
	return i;
}

static int l_add_starbase(lua_State *l)
{
	double x, y, z, n;
	int i;

	x = lua_tonumber(lua_state, 1);
	y = lua_tonumber(lua_state, 2);
	z = lua_tonumber(lua_state, 3);
	n = lua_tonumber(lua_state, 4);

	pthread_mutex_lock(&universe_mutex);
	i  = add_starbase(x, y, z, 0, 0, 0, n, -1);
	lua_pushnumber(lua_state, i < 0 ? -1.0 : (double) go[i].id);
	pthread_mutex_unlock(&universe_mutex);
	return 1;
}

static int add_nebula(double x, double y, double z,
		double vx, double vz, double heading, double r)
{
	int i;
	float av;
	union quat angvel;

	i = add_generic_object(x, y, z, vx, 0.0, vz, heading, OBJTYPE_NEBULA);
	if (i < 0)
		return i;
	go[i].move = nebula_move;
	go[i].type = OBJTYPE_NEBULA;
	go[i].tsd.nebula.r = r;
	av = (((float) snis_randn(10000)) / 5000.0f - 1.0f) * 0.1;
	random_axis_quat(&angvel, av * M_PI / 180);
	quat_to_axis(&angvel,
			&go[i].tsd.nebula.avx, &go[i].tsd.nebula.avy, &go[i].tsd.nebula.avz,
			&go[i].tsd.nebula.ava);
	random_axis_quat(&go[i].tsd.nebula.unrotated_orientation, snis_randn(360) * M_PI / 180.0);
	go[i].tsd.nebula.phase_angle = 4.0 * (float) snis_randn(100) / 50.0f - 1.0;
	go[i].tsd.nebula.phase_speed = (snis_randn(1000) / 1000.0 - 0.5) * 0.1;
	return i;
}

static int add_typed_explosion(uint32_t related_id,
				double x, double y, double z, uint16_t velocity,
				uint16_t nsparks, uint16_t time, uint8_t victim_type,
				uint8_t explosion_type)
{
	int i;

	i = add_generic_object(x, y, z, 0, 0, 0, 0, OBJTYPE_EXPLOSION);
	if (i < 0)
		return i;
	go[i].move = explosion_move;
	go[i].alive = 30; /* long enough to get propagaed out to all clients */
	go[i].tsd.explosion.velocity = velocity;
	go[i].tsd.explosion.nsparks = nsparks;
	go[i].tsd.explosion.time = time;
	go[i].tsd.explosion.victim_type = victim_type;
	go[i].tsd.explosion.explosion_type = explosion_type;
	go[i].tsd.explosion.related_id = related_id;
	return i;
}

static int add_explosion(double x, double y, double z, uint16_t velocity,
				uint16_t nsparks, uint16_t time, uint8_t victim_type)
{
	return add_typed_explosion((uint32_t) -1, x, y, z, velocity, nsparks, time, victim_type,
					EXPLOSION_TYPE_REGULAR);
}

static int add_blackhole_explosion(uint32_t black_hole, double x, double y, double z, uint16_t velocity,
				uint16_t nsparks, uint16_t time, uint8_t victim_type)
{
	return add_typed_explosion(black_hole, x, y, z, velocity, nsparks, time, victim_type,
					EXPLOSION_TYPE_BLACKHOLE);
}

static int add_chaff(double x, double y, double z)
{
	int i;
	float vx, vy, vz;

	random_point_on_sphere(1.0 * chaff_speed, &vx, &vy, &vz);
	i = add_generic_object(x, y, z, 0, 0, 0, 0, OBJTYPE_CHAFF);
	if (i < 0)
		return i;
	go[i].vx = vx;
	go[i].vy = vy;
	go[i].vz = vz;
	go[i].move = chaff_move;
	go[i].alive = 30;
	return i;
}

static int l_add_explosion(lua_State *l)
{
	int rc;
	double x, y, z, v, nsparks, time, victim_type, explosion_type;

	x = lua_tonumber(lua_state, 1);
	y = lua_tonumber(lua_state, 2);
	z = lua_tonumber(lua_state, 3);
	v = lua_tonumber(lua_state, 4);
	nsparks = lua_tonumber(lua_state, 5);
	time = lua_tonumber(lua_state, 6);
	victim_type = lua_tonumber(lua_state, 7);
	explosion_type = lua_tonumber(lua_state, 8);

	if (nsparks <= 0 || time <= 0 || v <= 0) {
		lua_pushnumber(lua_state, -1.0);
		return 1;
	}
	if (nsparks >= 200.0)
		nsparks = 200.0;
	rc = add_typed_explosion((uint32_t) -1, x, y, z, (uint16_t) v,
				(uint16_t) nsparks, (uint16_t) time, (uint8_t) victim_type,
				(uint8_t) explosion_type);
	if (rc < 0) {
		lua_pushnumber(lua_state, -1.0);
		return 1;
	}
	lua_pushnumber(lua_state, 0.0);
	return 1;
}

/* must hold universe mutex */
static int lookup_by_id(uint32_t id)
{
	int i;
	const int cachebits = 13;
	const int cachesize = 1 << cachebits;
	const int cachemask = cachesize - 1;
	static int *cache = NULL;

	if (cache == NULL) {
		cache = malloc(sizeof(int) * cachesize);
		memset(cache, -1, sizeof(int) * cachesize);
	}

	i = cache[id & cachemask];
	if (i != -1 && go[i].id == id)
		return i;

	for (i = 0; i <= snis_object_pool_highest_object(pool); i++)
		if (go[i].id == id) {
			cache[id & cachemask] = i;
			return i;
		}
	return -1;
}

static int lookup_by_damcon_id(struct damcon_data *d, int id) 
{
	int i;

	for (i = 0; i <= snis_object_pool_highest_object(d->pool); i++) {
		if (d->o[i].id == id)
			return 	i;
	}
	return -1;
}

static int add_laser(double x, double y, double z,
		double vx, double vy, double vz, union quat *orientation,
		uint32_t ship_id)
{
	int i, s;

	i = add_generic_object(x, y, z, vx, vy, vz, 0.0, OBJTYPE_LASER);
	if (i < 0)
		return i;
	go[i].move = laser_move;
	go[i].alive = LASER_LIFETIME;
	go[i].tsd.laser.ship_id = ship_id;
	s = lookup_by_id(ship_id);
	if (go[s].type == OBJTYPE_SHIP1 || go[s].type == OBJTYPE_SHIP2) {
		go[i].tsd.laser.power = go[s].tsd.ship.phaser_charge;
		go[i].tsd.laser.wavelength = go[s].tsd.ship.phaser_wavelength;
	} else if (go[s].type == OBJTYPE_TURRET) {
		go[i].tsd.laser.power = TURRET_LASER_POWER;
		go[i].tsd.laser.wavelength = TURRET_LASER_WAVELENGTH;
	}
	if (orientation) {
		go[i].orientation = *orientation;
	} else {
		union vec3 from = { { 1.0, 0.0, 0.0 } };
		union vec3 to = { { vx, vy, vz } };
		quat_from_u2v(&go[i].orientation, &from, &to, NULL);
	}
	queue_up_to_clients_that_care(&go[i]);
	return i;
}

static void laserbeam_move(struct snis_entity *o)
{
	int tid, oid, ttype;
	struct snis_entity *target, *origin;

	if (o->alive)
		o->alive--;
	if (o->alive == 0) {
		delete_from_clients_and_server(o);
		return;
	}
	if (o->tsd.laserbeam.mining_laser) /* don't deal damage from mining laser */
		return;

	/* only deal laser damage every 4th tick */
	if ((universe_timestamp & 0x011) != 0x011)
		return;

	tid = lookup_by_id(o->tsd.laserbeam.target);
	oid = lookup_by_id(o->tsd.laserbeam.origin);
	if (tid < 0 || oid < 0) {
		delete_from_clients_and_server(o);
		return;
	}

	schedule_callback2(event_callback, &callback_schedule,
				"object-hit-event", o->tsd.laserbeam.target,
				o->tsd.laserbeam.origin);
	/* if target or shooter is dead, stop firing */
	if (!go[tid].alive || !go[oid].alive) {
		if (!go[tid].alive)
			pop_ai_attack_mode(&go[oid]);
		delete_from_clients_and_server(o);
		return;
	}
		 
	target = &go[tid];
	origin = &go[oid];
	ttype = target->type;

	switch (ttype) {
	case OBJTYPE_STARBASE:
		target->tsd.starbase.under_attack = 1;
		add_starbase_attacker(target, o->tsd.laserbeam.origin);
		calculate_laser_starbase_damage(target, o->tsd.laserbeam.wavelength);
		notify_the_cops(o, target);
		break;
	case OBJTYPE_SHIP1: /* Fall thru */
	case OBJTYPE_SHIP2:
		calculate_laser_damage(target, o->tsd.laserbeam.wavelength,
					(float) o->tsd.laserbeam.power);
		send_ship_damage_packet(target);
		attack_your_attacker(target, lookup_entity_by_id(o->tsd.laserbeam.origin));
		notify_the_cops(o, target);
		break;
	case OBJTYPE_TURRET:
		calculate_laser_damage(target, o->tsd.laserbeam.wavelength,
					(float) o->tsd.laserbeam.power);
		break;
	case OBJTYPE_PLANET:
		if (rts_mode)
			inflict_rts_main_base_laser_damage(o, target);
		break;
	case OBJTYPE_ASTEROID:
		target->alive = 0;
		break;
	case OBJTYPE_MISSILE:
		if (snis_randn(1000) < laserbeam_missile_chance)
			target->alive = 0;
		break;
	case OBJTYPE_SPACEMONSTER:
		calculate_laser_damage(target, o->tsd.laserbeam.wavelength,
					(float) o->tsd.laserbeam.power);
		break;
	default:
		break;
	}

	if (!target->alive) {
		(void) add_explosion(target->x, target->y, target->z, 50, 50, 50, ttype);
		/* TODO -- these should be different sounds */
		/* make sound for players that got hit */
		/* make sound for players that did the hitting */

		if (origin->type == OBJTYPE_SHIP1)
			snis_queue_add_sound(EXPLOSION_SOUND,
					ROLE_SOUNDSERVER, origin->id);
		if (ttype != OBJTYPE_SHIP1) {
			if (ttype == OBJTYPE_SHIP2)
				make_derelict(target);
			respawn_object(target);
			delete_from_clients_and_server(target);
		} else {
			snis_queue_add_sound(EXPLOSION_SOUND,
						ROLE_SOUNDSERVER, target->id);
			schedule_callback(event_callback, &callback_schedule,
					"player-death-callback", target->id);
		}
	} else {
		(void) add_explosion(target->x, target->y, target->z, 50, 5, 5, ttype);
	}
	return;
}

static void tractorbeam_move(struct snis_entity *o)
{
	int tid, oid;
	struct snis_entity *target, *origin;
	struct mat41 to_object, nto_object, desired_object_loc, tractor_vec, tractor_velocity;
	double dist;

	if (o->alive == 0) {
		delete_from_clients_and_server(o);
		return;
	}

	if (universe_timestamp & 0x0f)
		return;

	tid = lookup_by_id(o->tsd.laserbeam.target);
	oid = lookup_by_id(o->tsd.laserbeam.origin);
	if (tid < 0 || oid < 0) {
		delete_from_clients_and_server(o);
		return;
	}
	target = &go[tid];
	origin = &go[oid];
	dist = object_dist(target, origin);
	if (dist > MAX_TRACTOR_DIST) {
		/* Tractor beam distance too much, beam failure... */
		o->alive = 0;
		delete_from_clients_and_server(o);
		return;
	}

	/* Make unit vector pointing to object */
	to_object.m[0] = target->x - origin->x;
	to_object.m[1] = target->y - origin->y;
	to_object.m[2] = target->z - origin->z;
	to_object.m[3] = 1.0;

	/* Find desired location of object */
	nto_object = to_object;
	nto_object.m[2] = 0.0;
	normalize_vector(&nto_object, &nto_object);
	mat41_scale(&nto_object, TRACTOR_BEAM_IDEAL_DIST, &desired_object_loc);

	/* Find vector from current object position towards desired position */ 
	tractor_vec.m[0] = desired_object_loc.m[0] - to_object.m[0];
	tractor_vec.m[1] = desired_object_loc.m[1] - to_object.m[1];
	tractor_vec.m[2] = desired_object_loc.m[2] - to_object.m[2];
	tractor_vec.m[3] = 1.0;

	/* Find how much velocity to impart to tractored object */
	dist = hypot3d(tractor_vec.m[0], tractor_vec.m[1], tractor_vec.m[2]);
	if (dist > MAX_TRACTOR_VELOCITY)
		dist = MAX_TRACTOR_VELOCITY;
	normalize_vector(&tractor_vec, &tractor_vec);
	mat41_scale(&tractor_vec, dist, &tractor_velocity);

	/* move tractored object */
	target->vx += tractor_velocity.m[0];
	target->vy += tractor_velocity.m[1];
	target->vz += tractor_velocity.m[2];

	return;
}

static int add_laserbeam(uint32_t origin, uint32_t target, int alive)
{
	int i, s, ti, oi;
	struct snis_entity *o, *t;

	i = add_generic_object(0, 0, 0, 0, 0, 0, 0, OBJTYPE_LASERBEAM);
	if (i < 0)
		return i;

	go[i].move = laserbeam_move;
	go[i].alive = alive;
	go[i].tsd.laserbeam.origin = origin;
	go[i].tsd.laserbeam.target = target;
	s = lookup_by_id(origin);
	go[i].tsd.laserbeam.power = go[s].tsd.ship.phaser_charge;
	go[s].tsd.ship.phaser_charge = 0;
	go[i].tsd.laserbeam.wavelength = go[s].tsd.ship.phaser_wavelength;
	go[i].tsd.laserbeam.mining_laser = 0;
	ti = lookup_by_id(target);
	if (ti < 0)
		return i;
	oi = lookup_by_id(origin);
	if (oi < 0)
		return i;
	o = &go[oi];
	t = &go[ti];
	if (t->type != OBJTYPE_SHIP1 && t->type != OBJTYPE_SHIP2 && t->type != OBJTYPE_STARBASE)
		return i;

	union vec3 impact_point;

	impact_point.v.x = (float) (t->x - o->x);
	impact_point.v.y = (float) (t->y - o->y);
	impact_point.v.z = (float) (t->z - o->z);
	vec3_normalize_self(&impact_point);
	vec3_mul_self(&impact_point, 100.0); /* server doesn't know radius */

	send_detonate_packet(t, t->x - impact_point.v.x,
			t->y - impact_point.v.y, t->z - impact_point.v.z, 0, 0.0);
	return i;
}

static int add_mining_laserbeam(uint32_t origin, uint32_t target, int alive)
{
	int i;
	i = add_laserbeam(origin, target, alive);
	if (i >= 0)
		go[i].tsd.laserbeam.mining_laser = 1;
	return i;
}

static int add_tractorbeam(struct snis_entity *origin, uint32_t target, int alive)
{
	int i;

	i = add_generic_object(0, 0, 0, 0, 0, 0, 0, OBJTYPE_TRACTORBEAM);
	if (i < 0)
		return i;

	go[i].move = tractorbeam_move;
	go[i].alive = alive;
	/* TODO: for now, re-use laserbeam data...consider if we need tbeam data */
	go[i].tsd.laserbeam.origin = origin->id;
	go[i].tsd.laserbeam.target = target;
	go[i].tsd.laserbeam.power = origin->tsd.ship.phaser_charge;
	origin->tsd.ship.phaser_charge = 0; /* TODO: fix this */
	go[i].tsd.laserbeam.wavelength = origin->tsd.ship.phaser_wavelength;
	origin->tsd.ship.tractor_beam = go[i].id;
	return i;
}

static int add_torpedo(double x, double y, double z, double vx, double vy, double vz, uint32_t ship_id)
{
	int i;
	i = add_generic_object(x, y, z, vx, vy, vz, 0.0, OBJTYPE_TORPEDO);
	if (i < 0)
		return i;
	go[i].move = torpedo_move;
	go[i].alive = torpedo_lifetime;
	go[i].tsd.torpedo.ship_id = ship_id;
	return i;
}

static int add_missile(double x, double y, double z, double vx, double vy, double vz,
			uint32_t target_id, uint32_t origin_id)
{
	int i;
	i = add_generic_object(x, y, z, vx, vy, vz, 0.0, OBJTYPE_MISSILE);
	if (i < 0)
		return i;
	go[i].move = missile_move;
	go[i].alive = missile_lifetime;
	go[i].tsd.missile.target_id = target_id;
	go[i].tsd.missile.origin = origin_id;
	return i;
}

static void add_starbases(void)
{
	int i, j, p, found;
	double x, y, z;
	uint32_t assoc_planet_id;

	for (i = 0; i < NBASES; i++) {
		if (i < NPLANETS) {
			p = 0;
			found = 0;
			for (j = 0; j <= snis_object_pool_highest_object(pool); j++) {
				if (go[j].type == OBJTYPE_PLANET)
					p++;
				if (p == i + 1) {
					float dx, dy, dz;
					random_point_on_sphere(go[j].tsd.planet.radius * 1.3 + 400.0f +
							snis_randn(400), &dx, &dy, &dz);
					x = go[j].x + dx;
					y = go[j].y + dy;
					z = go[j].z + dz;
					found = 1;
					assoc_planet_id = go[j].id;
					break;
				}
			}
			if (!found)  {
				/* If we get here, it's a bug... */
				printf("Nonfatal bug at %s:%d\n", __FILE__, __LINE__);
				x = ((double) snis_randn(1000)) * XKNOWN_DIM / 1000.0;
				y = ((double) snis_randn(1000) - 500.0) * YKNOWN_DIM / 1000.0;
				z = ((double) snis_randn(1000)) * ZKNOWN_DIM / 1000.0;
				assoc_planet_id = (uint32_t) -1;
			}
		} else {
			x = ((double) snis_randn(1000)) * XKNOWN_DIM / 1000.0;
			y = ((double) snis_randn(1000) - 500.0) * YKNOWN_DIM / 1000.0;
			z = ((double) snis_randn(1000)) * ZKNOWN_DIM / 1000.0;
				assoc_planet_id = (uint32_t) -1;
		}
		add_starbase(x, y, z, 0.0, 0.0, 0.0, i, assoc_planet_id);
	}
}

static int nebula_too_close(double ix[], double iy[], double iz[], int n)
{
	int i;
	const double limit = (XKNOWN_DIM / 4.0) * (XKNOWN_DIM / 4.0);

	for (i = 0; i < n; i++) {
		const double d = dist3dsqrd(ix[i] - ix[n], iy[i] - iy[n], iz[i] - iz[n]);
		if (d < limit)
			return 1; 
	}
	return 0;
}

static void add_nebulae(void)
{
	int i, j, k;
	double ix[NEBULA_CLUSTERS], iy[NEBULA_CLUSTERS], iz[NEBULA_CLUSTERS], x, y, z, r, factor;
	union vec3 d;
	int count;

	for (i = 0; i < NEBULA_CLUSTERS; i++) {
		random_point_on_sphere(1.0, &d.v.x, &d.v.y, &d.v.z);
		count = 0;
		do {
			ix[i] = x = ((double) snis_randn(1000)) * XKNOWN_DIM / 1000.0;
			iy[i] = y = ((double) snis_randn(1000) - 500.0) * YKNOWN_DIM / 1000.0;
			iz[i] = z = ((double) snis_randn(1000)) * ZKNOWN_DIM / 1000.0;
			count++;
		} while (nebula_too_close(ix, iy, iz, i) && count < 100);
		for (j = 0; j < NEBULAS_PER_CLUSTER; j++) {
			r = (double) snis_randn(NEBULA_RADIUS) +
					(double) MIN_NEBULA_RADIUS;
			k = add_nebula(x, y, z, 0.0, 0.0, 0.0, r);
			factor = 1.2;

			/* every once in awhile, maybe put a gap in between nebula blobs */
			if (snis_randn(100) < 10)
				factor += snis_randn(100) / 30.0;

			/* every once in awhile, go back to the nebula center */
			if (snis_randn(100) < 20) {
				x = ix[i];
				y = iy[i];
				z = iz[i];
				random_point_on_sphere(1.0, &d.v.x, &d.v.y, &d.v.z);
			}
			x += d.v.x * r * factor;
			y += d.v.y * r * factor;
			z += d.v.z * r * factor;

			/* perturb direction a bit... */
			d.v.x += snis_randn(1000) / 3000.0 - (1.0 / 6.0);
			d.v.y += snis_randn(1000) / 3000.0 - (1.0 / 6.0);
			d.v.z += snis_randn(1000) / 3000.0 - (1.0 / 6.0);
			vec3_normalize_self(&d);

			nebulalist[i * NEBULAS_PER_CLUSTER + j] = k;
		}
	}
}

static int l_add_nebula(lua_State *l)
{
	double x, y, z, r;
	const char *name;
	int i;

	name = lua_tostring(lua_state, 1);
	x = lua_tonumber(lua_state, 2);
	y = lua_tonumber(lua_state, 3);
	z = lua_tonumber(lua_state, 4);
	r = lua_tonumber(lua_state, 5);

	pthread_mutex_lock(&universe_mutex);
	i = add_nebula(x, y, z, 0.0, 0.0, 0.0, r);
	if (i < 0) {
		pthread_mutex_unlock(&universe_mutex);
		lua_pushnumber(lua_state, -1.0);
		return 1;
	}
	strncpy(go[i].sdata.name, name, sizeof(go[i].sdata.name) - 1);
	lua_pushnumber(lua_state, (double) go[i].id);
	pthread_mutex_unlock(&universe_mutex);
	return 1;
}


static void add_asteroids(void)
{
	int i, j;
	double x, y, z, cx, cy, cz, a, a2, r;

	for (i = 0; i < NASTEROID_CLUSTERS; i++) {
		cx = ((double) snis_randn(1000)) * XKNOWN_DIM / 1000.0;
		cy = ((double) snis_randn(1000) - 500.0) * YKNOWN_DIM / 1000.0;
		cz = ((double) snis_randn(1000)) * ZKNOWN_DIM / 1000.0;
		for (j = 0; j < asteroid_count / NASTEROID_CLUSTERS; j++) {
			a = (double) snis_randn(360) * M_PI / 180.0;
			a2 = (double) snis_randn(360) * M_PI / 180.0;
			r = snis_randn(ASTEROID_CLUSTER_RADIUS);
			x = cx + r * sin(a);
			z = cz + r * cos(a);
			y = cy + r * cos(a2);
			add_asteroid(x, y, z, 0.0, 0.0, 0.0, 1.0);
		}
	}
}

static int add_derelict(const char *name, double x, double y, double z,
			double vx, double vy, double vz,
			int shiptype, int the_faction, int persistent, uint32_t orig_ship_id)
{
	int i;

	i = add_generic_object(x, y, z, 0, 0, 0, 0, OBJTYPE_DERELICT);
	if (i < 0)
		return i;
	if (name)
		strncpy(go[i].sdata.name, name, sizeof(go[i].sdata.name) - 1);
	go[i].sdata.shield_strength = 0;
	go[i].sdata.shield_wavelength = 0;
	go[i].sdata.shield_width = 0;
	go[i].sdata.shield_depth = 0;
	go[i].move = derelict_move;
	if (shiptype != -1 && shiptype <= 255)
		go[i].tsd.derelict.shiptype = (uint8_t) shiptype;
	else
		go[i].tsd.derelict.shiptype = snis_randn(nshiptypes);
	if (the_faction >= 0)
		go[i].sdata.faction = (uint8_t) (the_faction % nfactions());
	else {
		union vec3 v;
		v.v.x = x;
		v.v.y = y;
		v.v.z = z;
		go[i].sdata.faction = (uint8_t) nearest_faction(v);
	}
	go[i].vx = vx;
	go[i].vy = vy;
	go[i].vz = vz;
	go[i].alive = 60 * 10; /* 1 minute */
	go[i].tsd.derelict.persistent = persistent & 0xff;
	go[i].tsd.derelict.fuel = snis_randn(255); /* TODO some better non-uniform distribution */
	go[i].tsd.derelict.oxygen = snis_randn(255); /* TODO some better non-uniform distribution */
	go[i].tsd.derelict.ships_log = NULL;
	go[i].tsd.derelict.orig_ship_id = orig_ship_id;
	go[i].tsd.derelict.ship_id_chip_present = 1;
	return i;
}

static int derelict_set_ships_log(uint32_t id, const char *ships_log)
{
	int i;

	pthread_mutex_lock(&universe_mutex);
	i = lookup_by_id(id);
	if (i < 0) {
		pthread_mutex_unlock(&universe_mutex);
		return -1;
	}
	if (go[i].type != OBJTYPE_DERELICT) {
		pthread_mutex_unlock(&universe_mutex);
		return -1;
	}
	if (go[i].tsd.derelict.ships_log) {
		free(go[i].tsd.derelict.ships_log);
		go[i].tsd.derelict.ships_log = NULL;
	}
	go[i].tsd.derelict.ships_log = strndup(ships_log, 100);
	pthread_mutex_unlock(&universe_mutex);
	return 0;
}

static int l_add_derelict(lua_State *l)
{
	double x, y, z, vx, vy, vz, shiptype, the_faction;
	const char *name;
	int i;

	name = lua_tostring(lua_state, 1);
	x = lua_tonumber(lua_state, 2);
	y = lua_tonumber(lua_state, 3);
	z = lua_tonumber(lua_state, 4);
	shiptype = lua_tonumber(lua_state, 5);
	the_faction = lua_tonumber(lua_state, 6);

	pthread_mutex_lock(&universe_mutex);
	vx = snis_random_float() * 10.0;
	vy = snis_random_float() * 10.0;
	vz = snis_random_float() * 10.0;
	/* assume lua-added derelicts are part of some scenario, so should be persistent */
	i = add_derelict(name, x, y, z, vx, vy, vz, shiptype, the_faction, 1, (uint32_t) -1);
	lua_pushnumber(lua_state, i < 0 ? -1.0 : (double) go[i].id);
	pthread_mutex_unlock(&universe_mutex);
	return 1;
}

static int l_derelict_set_ships_log(lua_State *l)
{
	double did;
	const char *ships_log;
	int rc, id;

	did = lua_tonumber(lua_state, 1);
	ships_log = lua_tostring(lua_state, 2);
	id = (int) did;
	if (id < 0) {
		lua_pushnumber(lua_state, -1.0);
		return 1;
	}
	rc = derelict_set_ships_log(id, ships_log);
	if (rc)
		lua_pushnumber(lua_state, -1.0);
	else
		lua_pushnumber(lua_state, 0.0);
	return 1;
}


static int choose_contraband(void)
{
	int i, n, c;

	n = snis_randn(ncontraband);
	c = 0;

	/* find the nth possible contraband item */
	for (i = 0; i < ncommodities; i++) {
		if (commodity[i].legality < 1.0) {
			if (n == c)
				return i;
			c++;
		}
	}
	return -1;
}

static int has_atmosphere(int i)
{
	return (strcmp(solarsystem_assets->planet_type[i], "rocky") != 0);
}

static int select_atmospheric_profile(struct snis_entity *planet)
{
	enum planetary_atmosphere_type atm_type;
	int planet_type = planet->tsd.planet.solarsystem_planet_type;
	int atm_instance = snis_randn(NATMOSPHERE_TYPES);

	if (strcmp(solarsystem_assets->planet_type[planet_type], "rocky") == 0)
		atm_type = marslike_atmosphere_type;
	else if (strcmp(solarsystem_assets->planet_type[planet_type], "earthlike") == 0)
		atm_type = earthlike_atmosphere_type;
	else if (strcmp(solarsystem_assets->planet_type[planet_type], "gas-giant") == 0)
		atm_type = gas_giant_atmosphere_type;
	else
		atm_type = gas_giant_atmosphere_type;
	return random_planetary_atmosphere_by_type(NULL, atm_type, atm_instance);
}

static int add_black_hole(double x, double y, double z, float radius)
{
	int i;

	i = add_generic_object(x, y, z, 0, 0, 0, 0, OBJTYPE_BLACK_HOLE);
	if (i < 0)
		return i;
	go[i].sdata.shield_strength = 0;
	go[i].sdata.shield_wavelength = 0;
	go[i].sdata.shield_width = 0;
	go[i].sdata.shield_depth = 0;
	go[i].move = black_hole_move;
	go[i].tsd.black_hole.radius = radius;

	return i;
}

/* type here is 0 == earthlike, 1 == gas-giant, 2 == rocky */
static int choose_planet_texture_of_type(int planet_type)
{
	int i;
	int ntextures = solarsystem_assets->nplanet_textures;
	int n = snis_randn(100) % ntextures;
	const char *ptypes[] = { "earthlike", "gas-giant", "rocky" };

	planet_type %= 3;

	for (i = 0; i < ntextures; i++) {
		if (strcmp(solarsystem_assets->planet_type[(i + n) % ntextures], ptypes[planet_type]) == 0)
			break;
	}
	return (i + n) % solarsystem_assets->nplanet_textures;
}

/* type here is -1 == random, 0 == earthlike, 1 == gas-giant, 2 == rocky */
static int add_planet(double x, double y, double z, float radius, uint8_t security, int type)
{
	int i, sst, minr, maxr;

	i = add_generic_object(x, y, z, 0, 0, 0, 0, OBJTYPE_PLANET);
	if (i < 0)
		return i;
	if (fabsl(y) < 0.01) {
		if (snis_randn(100) < 50)
			go[i].y = (double) snis_randn(3000) - 1500;
		else
			go[i].y = (double) snis_randn(70) - 35;
	}
	go[i].sdata.shield_strength = 0;
	go[i].sdata.shield_wavelength = 0;
	go[i].sdata.shield_width = 0;
	go[i].sdata.shield_depth = 0;
	go[i].move = planet_move;
	go[i].tsd.planet.government = snis_randn(1000) % ARRAYSIZE(government_name);
	go[i].tsd.planet.economy = snis_randn(1000) % ARRAYSIZE(economy_name);
	go[i].tsd.planet.tech_level = snis_randn(1000) % ARRAYSIZE(tech_level_name);
	go[i].tsd.planet.description_seed = snis_rand();
	go[i].tsd.planet.ring = snis_randn(100) < 20;
	if (type < 0) /* choose type randomly */
		sst = (uint8_t) (go[i].id % solarsystem_assets->nplanet_textures);
	else	/* choose a random instance of the given type */
		sst = (uint8_t) choose_planet_texture_of_type(type);
	go[i].tsd.planet.solarsystem_planet_type = sst;

	/* Enforce planet sizes based on planet type */
	if (strcmp(solarsystem_assets->planet_type[sst], "rocky") == 0) {
		minr = MIN_ROCKY_RADIUS;
		maxr = MAX_ROCKY_RADIUS;
	} else if (strcmp(solarsystem_assets->planet_type[sst], "earthlike") == 0) {
		minr = MIN_EARTHLIKE_RADIUS;
		maxr = MAX_EARTHLIKE_RADIUS;
	} else if (strcmp(solarsystem_assets->planet_type[sst], "gas-giant") == 0) {
		minr = MIN_GAS_GIANT_RADIUS;
		maxr = MAX_GAS_GIANT_RADIUS;
	} else {
		fprintf(stderr, "snis_server:%s:%d: Unexpected planet type '%s'\n",
			__FILE__, __LINE__, solarsystem_assets->planet_type[sst]);
		minr = MIN_PLANET_RADIUS;
		maxr = MAX_PLANET_RADIUS;
	}
	if (radius < minr || radius > maxr)
		radius = (float) snis_randn(maxr - minr) + minr;
	go[i].tsd.planet.radius = radius;
	go[i].tsd.planet.has_atmosphere = has_atmosphere(go[i].tsd.planet.solarsystem_planet_type);
	go[i].tsd.planet.atmosphere_type = select_atmospheric_profile(&go[i]);
	go[i].tsd.planet.ring_selector = snis_randn(256);
	go[i].tsd.planet.security = security;
	go[i].tsd.planet.contraband = choose_contraband();
	go[i].tsd.planet.atmosphere_r = solarsystem_assets->atmosphere_color[sst].r;
	go[i].tsd.planet.atmosphere_g = solarsystem_assets->atmosphere_color[sst].g;
	go[i].tsd.planet.atmosphere_b = solarsystem_assets->atmosphere_color[sst].b;
	go[i].tsd.planet.atmosphere_scale = 1.02;

	return i;
}

static int l_add_planet(lua_State *l)
{
	double x, y, z, r, s, t;
	const char *name;
	int i;

	name = lua_tostring(lua_state, 1);
	x = lua_tonumber(lua_state, 2);
	y = lua_tonumber(lua_state, 3);
	z = lua_tonumber(lua_state, 4);
	r = lua_tonumber(lua_state, 5);
	s = lua_tonumber(lua_state, 6);

	/* 7th optional param is planet type, 0 = earthlike, 1 = gas-giant, 2 = rocky, default = random */
	if (lua_gettop(l) >= 7)
		t = (int) lua_tonumber(lua_state, 7) % 3;
	else
		t = -1;

	if (r < MIN_PLANET_RADIUS)
		r = MIN_PLANET_RADIUS;
	if (r > MAX_PLANET_RADIUS)
		r = MAX_PLANET_RADIUS;

	pthread_mutex_lock(&universe_mutex);
	i = add_planet(x, y, z, r, (uint8_t) s, t);
	if (i < 0) {
		pthread_mutex_unlock(&universe_mutex);
		lua_pushnumber(lua_state, -1.0);
		return 1;
	}
	strncpy(go[i].sdata.name, name, sizeof(go[i].sdata.name) - 1);
	lua_pushnumber(lua_state, (double) go[i].id);
	pthread_mutex_unlock(&universe_mutex);
	return 1;
}

static int l_add_black_hole(lua_State *l)
{
	double x, y, z, r;
	const char *name;
	int i;

	name = lua_tostring(lua_state, 1);
	x = lua_tonumber(lua_state, 2);
	y = lua_tonumber(lua_state, 3);
	z = lua_tonumber(lua_state, 4);
	r = lua_tonumber(lua_state, 5);

	if (r < MIN_BLACK_HOLE_RADIUS)
		r = MIN_BLACK_HOLE_RADIUS;
	if (r > MAX_BLACK_HOLE_RADIUS)
		r = MAX_BLACK_HOLE_RADIUS;

	pthread_mutex_lock(&universe_mutex);
	i = add_black_hole(x, y, z, r);
	if (i < 0) {
		pthread_mutex_unlock(&universe_mutex);
		lua_pushnumber(lua_state, -1.0);
		return 1;
	}
	strncpy(go[i].sdata.name, name, sizeof(go[i].sdata.name) - 1);
	lua_pushnumber(lua_state, (double) go[i].id);
	pthread_mutex_unlock(&universe_mutex);
	return 1;
}

static int too_close_to_other_planet_or_sun(double x, double y, double z, double limit)
{
	int i;
	double dist;

	dist = dist3d(x - SUNX, y - SUNY, z - SUNZ);
	if (dist < SUN_DIST_LIMIT) /* Too close to sun? */
		return 1;

	for (i = 0; i <= snis_object_pool_highest_object(pool); i++) {
		struct snis_entity *o = &go[i];
		if (o->type != OBJTYPE_PLANET)
			continue;
		dist = dist3d(o->x - x, o->y - y, o->z - z);
		if (dist < limit)
			return 1;
	}

	return 0;
}

static int l_too_close_to_planet_or_sun(lua_State *l)
{
	double x, y, z, limit;
	int too_close;

	x = lua_tonumber(lua_state, 1);
	y = lua_tonumber(lua_state, 2);
	z = lua_tonumber(lua_state, 3);
	limit = lua_tonumber(lua_state, 4);

	pthread_mutex_lock(&universe_mutex);
	too_close = too_close_to_other_planet_or_sun(x, y, z, limit);
	pthread_mutex_unlock(&universe_mutex);

	lua_pushnumber(lua_state, too_close);
	return 1;
}

static void send_oneshot_sound_request(struct snis_entity *ship, char *filename)
{
	struct packed_buffer *pb;
	uint16_t len;

	len = strnlen(filename, 256);
	pb = packed_buffer_allocate(len + 3);
	packed_buffer_append(pb, "bhr", OPCODE_REQUEST_ONESHOT_SOUND, len, filename, len);
	send_packet_to_all_clients_on_a_bridge(ship->id, pb, ROLE_SOUNDSERVER);
}

static int l_play_sound(lua_State *l)
{
	const double lua_oid1 = luaL_checknumber(l, 1);
	const char *filename = luaL_checkstring(l, 2);
	int i;
	char *fn = strdup(filename); /* damn const. */

	pthread_mutex_lock(&universe_mutex);
	i = lookup_by_id(lua_oid1);
	if (i < 0) {
		pthread_mutex_unlock(&universe_mutex);
		send_demon_console_msg("PLAY_SOUND: BAD OBJECT ID: %f", lua_oid1);
		lua_pushnumber(lua_state, -1);
		goto out;
	}
	if (go[i].type != OBJTYPE_SHIP1) {
		pthread_mutex_unlock(&universe_mutex);
		send_demon_console_msg("PLAY_SOUND: NOT PLAYER SHIP: %f", lua_oid1);
		lua_pushnumber(lua_state, -1);
		goto out;
	}
	send_oneshot_sound_request(&go[i], fn);
	pthread_mutex_unlock(&universe_mutex);
	lua_pushnumber(lua_state, 0);
out:
	if (fn)
		free(fn);
	return 1;
}

static void set_red_alert_mode(struct game_client *c, unsigned char new_alert_mode);
static int l_set_red_alert_status(lua_State *l)
{
	const double lua_oid = luaL_checknumber(l, 1);
	const double status = luaL_checknumber(l, 2);
	uint32_t shipid;
	unsigned char redalert = 0;
	struct game_client *c;
	int i;

	if (status > 0.5)
		redalert = 1;

	pthread_mutex_lock(&universe_mutex);
	i = lookup_by_id(lua_oid);
	if (i < 0) {
		pthread_mutex_unlock(&universe_mutex);
		lua_pushnumber(l, -1.0);
		return 1;
	}
	if (go[i].type != OBJTYPE_SHIP1) {
		pthread_mutex_unlock(&universe_mutex);
		lua_pushnumber(l, -1.0);
		return 1;
	}
	shipid = go[i].id;
	pthread_mutex_unlock(&universe_mutex);

	client_lock();
	c = NULL;
	for (i = 0; i < nclients; i++) {
		c = &client[i];
		if (c->refcount && c->shipid == shipid)
			break;
	}
	client_unlock();
	if (!c) {
		pthread_mutex_unlock(&universe_mutex);
		lua_pushnumber(l, -1.0);
		return 1;
	}
	set_red_alert_mode(c, redalert);
	lua_pushnumber(l, 0);
	return 1;
}

static int l_destroy_ship(lua_State *l)
{
	const double lua_oid = luaL_checknumber(l, 1);
	struct snis_entity *t;
	int i;

	pthread_mutex_lock(&universe_mutex);
	i = lookup_by_id(lua_oid);
	if (i < 0) {
		pthread_mutex_unlock(&universe_mutex);
		lua_pushnumber(l, -1.0);
		send_demon_console_msg("DESTROY_SHIP: OBJECT NOT FOUND");
		return 1;
	}
	t = &go[i];
	if (t->type != OBJTYPE_SHIP2) {
		pthread_mutex_unlock(&universe_mutex);
		lua_pushnumber(l, -1.0);
		send_demon_console_msg("DESTROY_SHIP: OBJECT NOT AN NPC SHIP");
		return 1;
	}
	(void) add_explosion(t->x, t->y, t->z, 50, 150, 50, t->type);
	make_derelict(t);
	respawn_object(t);
	delete_from_clients_and_server(t);
	pthread_mutex_unlock(&universe_mutex);
	lua_pushnumber(l, 0.0);
	return 1;
}

static int l_add_torpedo(lua_State *l)
{
	const double x = luaL_checknumber(l, 1);
	const double y = luaL_checknumber(l, 2);
	const double z = luaL_checknumber(l, 3);
	const double vx = luaL_checknumber(l, 4);
	const double vy = luaL_checknumber(l, 5);
	const double vz = luaL_checknumber(l, 6);
	const double id = luaL_checknumber(l, 7);
	uint32_t iid;
	int i;

	iid = (uint32_t) id;
	pthread_mutex_lock(&universe_mutex);
	i = lookup_by_id(iid);
	if (i < 0) {
		pthread_mutex_unlock(&universe_mutex);
		lua_pushnumber(l, -1.0);
		send_demon_console_msg("ADD_TORPEDO: OBJECT NOT FOUND");
		return 1;
	}
	i = add_torpedo(x, y, z, vx, vy, vz, iid);
	pthread_mutex_unlock(&universe_mutex);
	lua_pushnumber(l, (double) i);
	return 1;
}

static int l_set_starbase_factions_allowed(lua_State *l)
{
	const double sbid = luaL_checknumber(l, 1);
	const double factionmask = luaL_checknumber(l, 2);
	uint8_t factions_allowed = (uint8_t) factionmask;
	uint32_t starbase_id = (uint32_t) sbid;
	struct snis_entity *sb;
	int i;

	pthread_mutex_lock(&universe_mutex);
	i = lookup_by_id(starbase_id);
	if (i < 0) {
		pthread_mutex_unlock(&universe_mutex);
		lua_pushnumber(l, -1.0);
		send_demon_console_msg("SET_STARBASE_FACTIONS_ALLOWED: STARBASE NOT FOUND");
		return 1;
	}
	sb = &go[i];
	if (sb->type != OBJTYPE_STARBASE) {
		pthread_mutex_unlock(&universe_mutex);
		lua_pushnumber(l, -1.0);
		send_demon_console_msg("SET_STARBASE_FACTIONS_ALLOWED: OBJECT IS WRONG TYPE");
		return 1;
	}
	sb->tsd.starbase.factions_allowed = factions_allowed;
	pthread_mutex_unlock(&universe_mutex);
	lua_pushnumber(l, 0.0);
	return 1;
}

static void add_black_holes(void)
{
	int i;
	double x, y, z, radius;

	for (i = 0; i < NBLACK_HOLES; i++) {
		x = ((double) snis_randn(1000)) * XKNOWN_DIM / 1000.0;
		y = ((double) snis_randn(2000) - 1000.0) * YKNOWN_DIM / 1000.0;
		z = ((double) snis_randn(1000)) * ZKNOWN_DIM / 1000.0;
		radius = (float) snis_randn(MAX_BLACK_HOLE_RADIUS - MIN_BLACK_HOLE_RADIUS) +
						MIN_BLACK_HOLE_RADIUS;
		add_black_hole(x, y, z, radius);
	}
}

static void add_planets(void)
{
	int i;
	double x, y, z, radius, limit;
	int count;

	limit = MIN_PLANET_SEPARATION;
	for (i = 0; i < NPLANETS; i++) {
		count = 0;
		do {
			x = ((double) snis_randn(1000)) * XKNOWN_DIM / 1000.0;
			y = ((double) snis_randn(2000) - 1000.0) * YKNOWN_DIM / 1000.0;
			z = ((double) snis_randn(1000)) * ZKNOWN_DIM / 1000.0;
			count++;
			if (count > 100)
				limit *= 0.95;
		} while (too_close_to_other_planet_or_sun(x, y, z, limit) && count < 250);
		if (count >= 250)
			printf("Minimum planet separation distance not attainable\n");
		radius = (float) snis_randn(MAX_PLANET_RADIUS - MIN_PLANET_RADIUS) +
						MIN_PLANET_RADIUS;
		add_planet(x, y, z, radius, i < 4 ? HIGH_SECURITY : LOW_SECURITY, -1);
	}
}

static int add_wormhole(double x1, double y1, double z1, double x2, double y2, double z2)
{
	int i;

	i = add_generic_object(x1, y1, z1, 0.0, 0.0, 0.0, 0.0, OBJTYPE_WORMHOLE);
	if (i < 0)
		return i;
	go[i].move = wormhole_move;
	go[i].tsd.wormhole.dest_x = x2;
	go[i].tsd.wormhole.dest_y = y2;
	go[i].tsd.wormhole.dest_z = z2;
	return i;
}

static void add_wormhole_pair(int *id1, int *id2,
	double x1, double y1, double z1, double x2, double y2, double z2)
{
	*id1 = add_wormhole(x1, y1, z1, x2, y2, z2);
	*id2 = add_wormhole(x2, y2, z2, x1, y1, z1);
	return;
}

static int l_add_wormhole_pair(lua_State *l)
{
	double x1, y1, z1, x2, y2, z2; 
	int id1, id2;

	x1 = lua_tonumber(lua_state, 1);
	y1 = lua_tonumber(lua_state, 2);
	z1 = lua_tonumber(lua_state, 3);
	x2 = lua_tonumber(lua_state, 4);
	y2 = lua_tonumber(lua_state, 5);
	z2 = lua_tonumber(lua_state, 6);

	pthread_mutex_lock(&universe_mutex);
	add_wormhole_pair(&id1, &id2, x1, y1, z1, x2, y2, z2);
	lua_pushnumber(lua_state, (double) go[id1].id);
	lua_pushnumber(lua_state, (double) go[id2].id);
	pthread_mutex_unlock(&universe_mutex);
	return 2;
}

static void add_wormholes(void)
{
	int i, id1, id2;
	double x1, y1, z1, x2, y2, z2;

	for (i = 0; i < NWORMHOLE_PAIRS; i++) {
		do {
			x1 = ((double) snis_randn(1000)) * XKNOWN_DIM / 1000.0;
			y1 = ((double) snis_randn(1000) - 500) * YKNOWN_DIM / 1000.0;
			z1 = ((double) snis_randn(1000)) * ZKNOWN_DIM / 1000.0;
			x2 = ((double) snis_randn(1000)) * XKNOWN_DIM / 1000.0;
			y2 = ((double) snis_randn(1000) - 500) * YKNOWN_DIM / 1000.0;
			z2 = ((double) snis_randn(1000)) * ZKNOWN_DIM / 1000.0;
		} while (dist3dsqrd(x1 - x2, y1 - y2, z1 - z2) <
				(XKNOWN_DIM / 2.0) * (XKNOWN_DIM / 2.0));
		add_wormhole_pair(&id1, &id2, x1, y1, z1, x2, y2, z2);
	}
}

static int add_warpgate(double x, double y, double z,
			double vx, double vz, double heading, int n, uint32_t assoc_planet_id)
{
	int i;

	i = add_generic_object(x, y, z, vx, 0.0, vz, heading, OBJTYPE_WARPGATE);
	if (i < 0)
		return i;
	if (n < 0)
		n = -n;
	n %= 99;
	go[i].move = warpgate_move;
	go[i].type = OBJTYPE_WARPGATE;
	go[i].sdata.shield_strength = 255;
	go[i].tsd.warpgate.warpgate_number = n;
	snprintf(go[i].sdata.name, sizeof(go[i].sdata.name), "WG-%02d", n);
	/* Note: if we ever change the orientation from un-rotated, it will break
	 * collision detection because of point_to_torus_dist(), and probably
	 * some other places related to toroidal collision detection.
	 */
	return i;
}

static void add_warpgates(void)
{
	int i, j;
	double x, y, z;
	uint32_t assoc_planet_id;

	for (i = 0; i < NWARPGATES; i++) {
		int p = 0;
		int found = 0;
		for (j = 0; j <= snis_object_pool_highest_object(pool); j++) {
			if (go[j].type == OBJTYPE_PLANET)
				p++;
			if (p == i + 1) {
				float dx, dy, dz;
				random_point_on_sphere(go[j].tsd.planet.radius * 1.3 + 400.0f +
						snis_randn(400), &dx, &dy, &dz);
				x = go[j].x + dx;
				y = go[j].y + dy;
				z = go[j].z + dz;
				found = 1;
				assoc_planet_id = go[j].id;
				break;
			}
		}
		if (!found)  {
			/* If we get here, it's a bug... */
			printf("Nonfatal bug at %s:%d\n", __FILE__, __LINE__);
			x = ((double) snis_randn(1000)) * XKNOWN_DIM / 1000.0;
			y = ((double) snis_randn(1000) - 500.0) * YKNOWN_DIM / 1000.0;
			z = ((double) snis_randn(1000)) * ZKNOWN_DIM / 1000.0;
			assoc_planet_id = (uint32_t) -1;
		}
		add_warpgate(x, y, z, 0.0, 0.0, 0.0, i, assoc_planet_id);
	}
}

static void add_eships(void)
{
	int i;

	for (i = 0; i < npc_ship_count; i++)
		add_ship(i % nfactions(), snis_randn(nshiptypes), 1);
}

static void add_enforcers_to_planet(struct snis_entity *p)
{
	int nenforcers = p->tsd.planet.security * 2;
	int x;

	for (int i = 0; i < nenforcers; i++) {
		x = add_ship(p->sdata.faction, SHIP_CLASS_ENFORCER, 1);
		if (x < 0)
			continue;
		go[x].tsd.ship.home_planet = p->id;
		snprintf(go[x].sdata.name, sizeof(go[i].sdata.name), "POLICE-%02d",
			snis_randn(100));
		ship_registry_delete_ship_entries(&ship_registry, go[x].id);
		ship_registry_add_entry(&ship_registry, go[x].id, SHIP_REG_TYPE_REGISTRATION,
					"LAW ENFORCEMENT SPACECRAFT");
		/* TODO: zero is law enforcement... something better. */
		ship_registry_add_owner(&ship_registry, go[x].id, 0);
		push_cop_mode(&go[x]);
	}
}

static void add_enforcers()
{
	int i;

	for (i = 0; i <= snis_object_pool_highest_object(pool); i++)
		if (go[i].type == OBJTYPE_PLANET && go[i].tsd.planet.security > 0)
			add_enforcers_to_planet(&go[i]);
}

static void add_spacemonsters(void)
{
	int i;
	double x, y, z;

	for (i = 0; i < NSPACEMONSTERS; i++) {
		x = ((double) snis_randn(1000)) * XKNOWN_DIM / 1000.0;
		y = ((double) snis_randn(1000) - 500.0) * YKNOWN_DIM / 1000.0;
		z = ((double) snis_randn(1000)) * ZKNOWN_DIM / 1000.0;
		add_spacemonster(x, y, z);
	}
}

static uint32_t nth_starbase(int n)
{
	int nstarbases = 0;
	int i;

	do {
		for (i = 0; i <= snis_object_pool_highest_object(pool); i++) {
			if (go[i].type == OBJTYPE_STARBASE) {
				nstarbases++;
				if (nstarbases - 1 == n)
					return go[i].id;
			}
		}
		if (nstarbases == 0)
			return (uint32_t) -1;

		/* If n > number of starbase, we will make n = n modulo nstarbases and try again. */
		n = n % nstarbases;
		nstarbases = 0;
	} while (1);
}

static int compute_fare(uint32_t src, uint32_t dest)
{
	union vec3 travel;
	int i;
	struct snis_entity *s, *d;
	const int default_fare = 350;
	const int min_fare = 150;
	int fare_noise = snis_randn(40);

	i = lookup_by_id(src);
	if (i < 0)
		return default_fare + fare_noise;
	s = &go[i];
	i = lookup_by_id(dest);
	if (i < 0)
		return default_fare + fare_noise;
	d = &go[i];
	if (d == s)
		return min_fare + fare_noise;
	travel.v.x = d->x - s->x;
	travel.v.y = d->y - s->y;
	travel.v.z = d->z - s->z;
	return (int) (fare_noise + min_fare + (vec3_magnitude(&travel) * 2170.0 / XKNOWN_DIM));
}

static void update_passenger(int i, int nstarbases)
{
	static struct mtwist_state *mt = NULL;
	if (!mt)
		mt = mtwist_init(mtwist_seed);
	character_name(mt, passenger[i].name,  sizeof(passenger[i].name) - 1);
	passenger[i].location = nth_starbase(snis_randn(nstarbases));
	do {
		passenger[i].destination = nth_starbase(snis_randn(nstarbases));
	} while (passenger[i].destination == passenger[i].location && nstarbases > 1);
	passenger[i].fare = compute_fare(passenger[i].location, passenger[i].destination);
}

static int count_starbases(void)
{
	int i, nstarbases = 0;
	for (i = 0; i <= snis_object_pool_highest_object(pool); i++) {
		if (go[i].type == OBJTYPE_STARBASE)
			nstarbases++;
	}
	return nstarbases;
}

static void add_passengers(void)
{
	int i;
	int nstarbases;

	nstarbases = count_starbases();
	for (i = 0; i < MAX_PASSENGERS; i++)
		update_passenger(i, nstarbases);
	npassengers = MAX_PASSENGERS;
}

/* Sets random seeds to value in environment variable SNISRAND
 * if set, or to value of new_seed if new_seed is not -1.
 * If both SNISRAND is not set and new_seed is -1, then there
 * is no effect.
 */
static void set_random_seed(int new_seed)
{
	char *seed = getenv("SNISRAND");
	int i, rc;

	if (!seed && !new_seed)
		return;

	if (seed) {
		rc = sscanf(seed, "%d", &i);
		if (rc != 1)
			return;
	} else if (new_seed != -1) {
		i = new_seed;
	} else { /* new_seed is -1 */
		return;
	}

	snis_srand((unsigned int) i);
	srand(i);
	mtwist_seed = (uint32_t) i;
}

static void populate_universe(void)
{
	if (solarsystem_assets->random_seed != -1)
		set_random_seed(solarsystem_assets->random_seed);
	initialize_random_orientations_and_spins(COMMON_MTWIST_SEED);
	if (ship_registry.entry)
		free_ship_registry(&ship_registry);
	ship_registry_init(&ship_registry);
	add_nebulae(); /* do nebula first */
	add_asteroids();
	add_planets();
	add_starbases();
	add_warpgates();
	add_wormholes();
	add_eships();
	add_enforcers();
	add_spacemonsters();
	add_passengers();
	add_black_holes();
}

static void make_universe(void)
{
	pthread_mutex_lock(&universe_mutex);
	snis_object_pool_setup(&pool, MAXGAMEOBJS);
	planetary_atmosphere_model_init_models(ATMOSPHERE_TYPE_GEN_SEED, NATMOSPHERE_TYPES);
	populate_universe();
	pthread_mutex_unlock(&universe_mutex);
}

static void regenerate_universe(void)
{
	disable_rts_mode();
	pthread_mutex_lock(&universe_mutex);
	populate_universe();
	pthread_mutex_unlock(&universe_mutex);
	send_demon_console_msg("UNIVERSE REGENERATED");
}

static int add_generic_damcon_object(struct damcon_data *d, int x, int y,
				uint32_t type, damcon_move_function move_fn)
{
	int i, j;
	struct snis_damcon_entity *o;

	i = snis_object_pool_alloc_obj(d->pool); 	 
	if (i < 0)
		return -1;
	o = &d->o[i];
	memset(o, 0, sizeof(*o));
	o->x = x;
	o->y = y;
	o->id = get_new_object_id();
	o->velocity = 0;
	o->heading = 0;
	o->version = 1;
	o->type = type; 
	o->move = move_fn;

	/* clear out the client update state */
	for (j = 0; j < nclients; j++)
		if (client[j].refcount && d->bridge == client[j].bridge)
			memset(&client[j].damcon_data_clients[i], 0, sizeof(client[j].damcon_data_clients[i]));
	return i;
}

static void add_damcon_robot(struct damcon_data *d)
{
	int i;

	i = add_generic_damcon_object(d, 0, 0, DAMCON_TYPE_ROBOT, damcon_robot_move);
	if (i < 0)
		return;
	d->robot = &d->o[i];
	d->robot->tsd.robot.cargo_id = ROBOT_CARGO_EMPTY;
	d->robot->tsd.robot.short_term_goal_x = 50.0;
	d->robot->tsd.robot.short_term_goal_y = 50.0;
	d->robot->tsd.robot.long_term_goal_x = -1.0;
	d->robot->tsd.robot.long_term_goal_y = -1.0;
	d->robot->tsd.robot.robot_state = DAMCON_ROBOT_DECIDE_LTG;
	d->robot->tsd.robot.repair_socket_id = ROBOT_CARGO_EMPTY;
}

static int inside_damcon_system(struct damcon_data *d, int x, int y)
{
	int i;
	const int w = 270; /* These must match placeholder-system-points.h */
	const int h = 180 / 2;

	for (i = 0; i <= snis_object_pool_highest_object(d->pool); i++) {
		switch (d->o[i].type) {
		case DAMCON_TYPE_SHIELDSYSTEM:
		case DAMCON_TYPE_IMPULSE:
		case DAMCON_TYPE_WARPDRIVE:
		case DAMCON_TYPE_MANEUVERING:
		case DAMCON_TYPE_PHASERBANK:
		case DAMCON_TYPE_SENSORARRAY:
		case DAMCON_TYPE_COMMUNICATIONS:
		case DAMCON_TYPE_TRACTORSYSTEM:
		case DAMCON_TYPE_REPAIR_STATION:
			break;
		default:
			continue;
		}
		if (x >= d->o[i].x && x <= d->o[i].x + w &&
			y >= d->o[i].y - h && y <= d->o[i].y + h)
			return 1;
	}
	return 0;
}

static int add_damcon_waypoint(struct damcon_data *d, int x, int y)
{
	if (inside_damcon_system(d, x, y)) /* reject waypoints robot can't reach */
		return -1;
	if (x < -DAMCONXDIM / 2 || x > DAMCONXDIM / 2 ||
		y < -DAMCONYDIM / 2  || y > DAMCONYDIM / 2) /* reject waypoints outside damcon area */
		return -1;
	return add_generic_damcon_object(d, x, y, DAMCON_TYPE_WAYPOINT, NULL);
}

/* offsets for sockets... */
static const int dcxo[] = { 20, 160, 205, 160, 20 };
static const int dcyo[] = { -65, -65, 0, 65, 65 };

static void add_damcon_sockets(struct damcon_data *d, int x, int y,
				uint8_t system, int left_side)
{
	int i, p, px, py;
	struct snis_damcon_entity *socket;
	damcon_move_function fn = NULL;
	int n;

	if (system != DAMCON_TYPE_REPAIR_STATION) {
		n = DAMCON_PARTS_PER_SYSTEM;
		fn = NULL;
	} else {
		n = 2;
		fn = damcon_repair_socket_move;
	}

	for (i = 0; i < n; i++) {
		if (left_side) {
			px = x + dcxo[i] + 30;	
			py = y + dcyo[i];	
		} else {
			px = x - dcxo[i] + 210 + 30;	
			py = y + dcyo[i];	
		}
		p = add_generic_damcon_object(d, px, py, DAMCON_TYPE_SOCKET, fn);
		d->o[p].version++;
		d->o[p].tsd.socket.system = system;
		d->o[p].tsd.socket.part = i;
		d->o[p].tsd.socket.contents_id = DAMCON_SOCKET_EMPTY;

		if (system != DAMCON_TYPE_REPAIR_STATION) {
			socket = &d->o[p];
			p = add_generic_damcon_object(d, px, py, DAMCON_TYPE_PART, NULL);
			d->o[p].version++;
			d->o[p].tsd.part.system = system;
			d->o[p].tsd.part.part = i;
			d->o[p].tsd.part.damage = 0;
			socket->tsd.socket.contents_id = d->o[p].id;
		}
	}
}

static void add_damcon_labels(struct damcon_data *d)
{
}

static void add_damcon_systems(struct damcon_data *d)
{
	int i;
	int x, y, dy;

	x = -DAMCONXDIM / 2;
	dy = DAMCONYDIM / 8;
	y = dy - DAMCONYDIM / 2;
	i = add_generic_damcon_object(d, x, y, DAMCON_TYPE_WARPDRIVE, NULL);
	d->o[i].version++;
	add_damcon_sockets(d, x, y, DAMCON_TYPE_WARPDRIVE, 1);
	y += dy;

	i = add_generic_damcon_object(d, x, y, DAMCON_TYPE_SENSORARRAY, NULL);
	add_damcon_sockets(d, x, y, DAMCON_TYPE_SENSORARRAY, 1);
	y += dy;
	d->o[i].version++;
	i = add_generic_damcon_object(d, x, y, DAMCON_TYPE_COMMUNICATIONS, NULL);
	add_damcon_sockets(d, x, y, DAMCON_TYPE_COMMUNICATIONS, 1);
	x = 2 * DAMCONXDIM / 3 - DAMCONXDIM / 2;
	y = dy - DAMCONYDIM / 2;
	d->o[i].version++;
	i = add_generic_damcon_object(d, x, y, DAMCON_TYPE_MANEUVERING, NULL);
	add_damcon_sockets(d, x, y, DAMCON_TYPE_MANEUVERING, 0);
	y += dy;
	d->o[i].version++;
	i = add_generic_damcon_object(d, x, y, DAMCON_TYPE_PHASERBANK, NULL);
	add_damcon_sockets(d, x, y, DAMCON_TYPE_PHASERBANK, 0);
	y += dy;
	d->o[i].version++;
	i = add_generic_damcon_object(d, x, y, DAMCON_TYPE_IMPULSE, NULL);
	add_damcon_sockets(d, x, y, DAMCON_TYPE_IMPULSE, 0);
	y += dy;
	x = -DAMCONXDIM / 2;
	d->o[i].version++;
	i = add_generic_damcon_object(d, x, y, DAMCON_TYPE_SHIELDSYSTEM, NULL);
	add_damcon_sockets(d, x, y, DAMCON_TYPE_SHIELDSYSTEM, 1);
	x = 2 * DAMCONXDIM / 3 - DAMCONXDIM / 2;
	d->o[i].version++;
	i = add_generic_damcon_object(d, x, y, DAMCON_TYPE_TRACTORSYSTEM, NULL);
	add_damcon_sockets(d, x, y, DAMCON_TYPE_TRACTORSYSTEM, 0);
	y += dy;
	x = -DAMCONXDIM / 2;
	d->o[i].version++;
	i = add_generic_damcon_object(d, x, y, DAMCON_TYPE_REPAIR_STATION, NULL);
	add_damcon_sockets(d, x, y, DAMCON_TYPE_REPAIR_STATION, 0);
	x = 2 * DAMCONXDIM / 3 - DAMCONXDIM / 2;
	d->o[i].version++;
	i = add_generic_damcon_object(d, x, y, DAMCON_TYPE_LIFESUPPORTSYSTEM, NULL);
	add_damcon_sockets(d, x, y, DAMCON_TYPE_LIFESUPPORTSYSTEM, 0);
	d->o[i].version++;
}

static void add_damcon_parts(struct damcon_data *d)
{
}

static struct snis_damcon_entity *find_nth_waypoint(struct damcon_data *d, int n)
{
	int i;

	for (i = 0; i <= snis_object_pool_highest_object(d->pool); i++) {
		if (d->o[i].type != DAMCON_TYPE_WAYPOINT)
			continue;
		if (d->o[i].tsd.waypoint.n == n)
			return &d->o[i];
	}
	return NULL;
}

/******************************************************************************

The waypoint_data below describes the following network of waypoints:

    ------------------------------------------------------------------------
    |                                                                      |
    |           0 ---------------------- 1 ------------------ 2            |
    |         /    \                  /  |  \              /     \         |
    |       24     25             /      |      \        26        27      |
    |                          40 ------ 3 ------ 41                       |
    |                              \     |      /                          |
    |                                  \ |   /                             |
    |           4 ---------------------- 5 ------------------- 6           |
    |         /   \                   /  |   \               /   \         |
    |       28      29             /     |      \          30      31      |
    |                          42 ------ 7 ------ 43                       |
    |                              \     |       /                         |
    |                                 \  |    /                            |
    |           8 ---------------------- 9 ------------------- 10          |
    |         /   \                  /   |  \                /   \         |
    |       32     33              /     |    \            34     35       |
    |                          44 ------ 11 ----- 45                       |
    |                             \      |     /                           |
    |                                \   |  /                              |
    |           12---------------------- 13------------------- 14          |
    |         /   \                  /   |  \                /   \         |
    |       36     37             /      |     \           38     39       |
    |                          46 ------ 15 ----- 47                       |
    |                             \      |     /                           |
    |                                \   |  /                              |
    |           16---------------------- 17------------------- 18          |
    |          /   \                  /  | \                  /   \        |
    |        48     49                   |   \              50     51      |
    |                              /     |     \                           |
    |                            /       |       \                         |
    |                          19-------20------- 21                       |
    |                            \       |        /                        |
    |                              \     |     /                           |
    |                                 \  |  /                              |
    |                                   22-------------------- 23          |
    |                                                                      |
    ------------------------------------------------------------------------
*******************************************************************************/

static struct waypoint_data_entry {
	int n, x, y;
	int neighbor[10];
} damcon_waypoint_data[] = {
#define WAYPOINTBASE (-850 - 170 * 2)
#define WAYPOINTY(n) (WAYPOINTBASE + n * 170)
	{  0, -256, WAYPOINTY(0), { 1, 24, 25, -1 }, },
	{  1,    0, WAYPOINTY(0), { 0, 2, 3, 40, 41, -1  }, },
	{  2,  256, WAYPOINTY(0), { 1, 26, 27, -1 }, },
	{  3,    0, WAYPOINTY(1), { 1, 5, 40, 41, -1 }, },
	{  4, -256, WAYPOINTY(2), { 5, 28, 29, -1 }, },
	{  5,    0, WAYPOINTY(2), { 3, 4, 6, 7, 40, 41, 42, 43, -1  }, },
	{  6,  256, WAYPOINTY(2), { 5, 30, 31, -1 }, },
	{  7,    0, WAYPOINTY(3), { 5, 9, 42, 43, -1 }, },
	{  8, -256, WAYPOINTY(4), { 9, 32, 33, -1 }, },
	{  9,    0, WAYPOINTY(4), { 7, 8, 10, 11, 42, 43, 44, 45, -1 }, },
	{ 10,  256, WAYPOINTY(4), { 9, 34, 35, -1 }, },
	{ 11,    0, WAYPOINTY(5), { 9, 13, 44, 45, -1 }, },
	{ 12, -256, WAYPOINTY(6), { 13, 36, 37, -1 }, },
	{ 13,    0, WAYPOINTY(6), { 11, 12, 14, 15, 44, 45, 46, 47, -1 }, },
	{ 14,  256, WAYPOINTY(6), { 13, 38, 39, -1 }, },
	{ 15,    0, WAYPOINTY(7), { 13, 17, 46, 47, -1 }, },
	{ 16, -256, WAYPOINTY(8), { 17, 48, 49, -1 }, },
	{ 17,    0, WAYPOINTY(8), { 15, 16, 18, 19, 20, 21, 46, 47, -1 }, },
	{ 18,  256, WAYPOINTY(8), { 17, 50, 51, -1 }, },
	{ 19, -100, WAYPOINTY(9), { 17, 20, 22, -1 }, },
	{ 20,    0, WAYPOINTY(9), { 17, 19, 21, 22, -1 }, },
	{ 21,  100, WAYPOINTY(9), { 17, 20, 22, -1 }, },
	{ 22,    0, WAYPOINTY(10), { 19, 20, 21, 23, -1 }, },
	{ 23,  256, WAYPOINTY(10), { 22, -1 }, },
	{ 24, -340, WAYPOINTY(0) + 50, { 0, -1 }, },
	{ 25, -220, WAYPOINTY(0) + 50, { 0, -1 }, },
	{ 26,  340, WAYPOINTY(0) + 50, { 2, -1 }, },
	{ 27,  220, WAYPOINTY(0) + 50, { 2, -1 }, },
	{ 28, -340, WAYPOINTY(2) + 50, { 4, -1 }, },
	{ 29, -220, WAYPOINTY(2) + 50, { 4, -1 }, },
	{ 30,  340, WAYPOINTY(2) + 50, { 6, -1 }, },
	{ 31,  220, WAYPOINTY(2) + 50, { 6, -1 }, },
	{ 32, -340, WAYPOINTY(4) + 50, { 8, -1 }, },
	{ 33, -220, WAYPOINTY(4) + 50, { 8, -1 }, },
	{ 34,  340, WAYPOINTY(4) + 50, { 10, -1 }, },
	{ 35,  220, WAYPOINTY(4) + 50, { 10, -1 }, },
	{ 36, -340, WAYPOINTY(6) + 50, { 12, -1 }, },
	{ 37, -220, WAYPOINTY(6) + 50, { 12, -1 }, },
	{ 38,  340, WAYPOINTY(6) + 50, { 14, -1 }, },
	{ 39,  220, WAYPOINTY(6) + 50, { 14, -1 }, },
	{ 40, -100, WAYPOINTY(1), { 1, 3, 5, -1 }, },
	{ 41,  100, WAYPOINTY(1), { 1, 3, 5, -1 }, },
	{ 42, -100, WAYPOINTY(3), { 5, 7, 9, -1 }, },
	{ 43,  100, WAYPOINTY(3), { 5, 7, 9, -1 }, },
	{ 44, -100, WAYPOINTY(5), { 11, 9, 13, -1 }, },
	{ 45,  100, WAYPOINTY(5), { 11, 9, 13, -1 }, },
	{ 46, -100, WAYPOINTY(7), { 15, 13, 17, -1 }, },
	{ 47,  100, WAYPOINTY(7), { 15, 13, 17, -1 }, },
	{ 48, -340, WAYPOINTY(8) + 50, { 16, -1 }, },
	{ 49, -220, WAYPOINTY(8) + 50, { 16, -1 }, },
	{ 50,  340, WAYPOINTY(8) + 50, { 18, -1 }, },
	{ 51,  220, WAYPOINTY(8) + 50, { 18, -1 }, },
};

static void __attribute__((unused)) print_waypoint_table(struct damcon_data *d, int tablesize)
{
	int i, j;
	struct snis_damcon_entity *w;

	printf("--- Begin Travel table ------------------\n");
	for (i = 0; i < tablesize; i++) {
		struct waypoint_data_entry *e = &damcon_waypoint_data[i];
		w = find_nth_waypoint(d, e->n);
		for (j = 0; e->neighbor[j] > 0; j++) {
			printf("    %d[%d]) --> %d\n", w->tsd.waypoint.n, j,
				w->tsd.waypoint.neighbor[j] ? w->tsd.waypoint.neighbor[j]->tsd.waypoint.n : -1);
		}
	}
	printf("--- End Travel table ------------------\n");

}

/* return 0 if there is a connection from "from" to "to", 1 otherwise */
static int  damcon_waypoint_continuity_failure(int from, int to)
{
	int i, j;

	for (i = 0; i < ARRAYSIZE(damcon_waypoint_data); i++) {
		struct waypoint_data_entry *e = &damcon_waypoint_data[i];
		if (e->n != from)
			continue;
		for (j = 0; e->neighbor[j] != -1; j++)
			if (e->neighbor[j] == to)
				return 0;
	}
	return 1;
}

static void damcon_waypoint_sanity_check(void)
{
	int i, j;

	for (i = 0; i < ARRAYSIZE(damcon_waypoint_data); i++) {
		struct waypoint_data_entry *e = &damcon_waypoint_data[i];
		for (j = 0; e->neighbor[j] != -1; j++) {
			if (damcon_waypoint_continuity_failure(e->neighbor[j], e->n)) {
				printf("damcon waypoint continuity failure: %d -> %d, but not %d -> %d\n",
					e->n, e->neighbor[j], e->neighbor[j], e->n);
			}
		}
	}
}

static void add_damcon_waypoints(struct damcon_data *d)
{
	int i, j, rc;
	struct snis_damcon_entity *w, *n;

	damcon_waypoint_sanity_check();
	for (i = 0; i < ARRAYSIZE(damcon_waypoint_data); i++) {
		struct waypoint_data_entry *e = &damcon_waypoint_data[i];
		rc = add_damcon_waypoint(d, e->x, e->y);
		if (rc < 0) {
			fprintf(stderr, "%s: Failed to add waypoint, i = %d at %s:%d\n",
				logprefix(), i, __FILE__, __LINE__);
			break;
		}
		d->o[rc].tsd.waypoint.n = e->n;
	}
	for (i = 0; i < ARRAYSIZE(damcon_waypoint_data); i++) {
		struct waypoint_data_entry *e = &damcon_waypoint_data[i];
		w = find_nth_waypoint(d, e->n);
		for (j = 0; e->neighbor[j] >= 0; j++) {
			n = find_nth_waypoint(d, e->neighbor[j]);
			if (!n) {
				fprintf(stderr, "%s: n is unexpectedly null at %s:%d.\n",
					logprefix(), __FILE__, __LINE__);
			}
			w->tsd.waypoint.neighbor[j] = n;
		}
	}
}

static void populate_damcon_arena(struct damcon_data *d)
{
	snis_object_pool_setup(&d->pool, MAXDAMCONENTITIES);
	add_damcon_robot(d);
	add_damcon_systems(d);
	add_damcon_labels(d);
	add_damcon_parts(d);
	add_damcon_waypoints(d);
}

typedef void (*thrust_function)(struct game_client *c, int thrust);

static void do_demon_thrust(struct snis_entity *o, int thrust)
{
	if (thrust > 0) {
		if (o->tsd.ship.velocity < max_player_velocity)
			o->tsd.ship.velocity += player_velocity_increment;
	} else {
		if (o->tsd.ship.velocity > -max_player_velocity)
			o->tsd.ship.velocity -= player_velocity_increment;
	}
	if (o->tsd.ship.velocity > max_player_velocity)
		o->tsd.ship.velocity = max_player_velocity;
	else if (o->tsd.ship.velocity < -max_player_velocity)
		o->tsd.ship.velocity = -max_player_velocity;
}

static void do_robot_thrust(struct game_client *c, int thrust)
{
	struct snis_damcon_entity *robot = bridgelist[c->bridge].damcon.robot;

	if (thrust > 0) {
		if (robot->tsd.robot.desired_velocity < max_damcon_robot_velocity)
			robot->tsd.robot.desired_velocity += ROBOT_VELOCITY_INCREMENT;
	} else {
		if (robot->tsd.robot.desired_velocity > -max_damcon_robot_velocity)
			robot->tsd.robot.desired_velocity -= ROBOT_VELOCITY_INCREMENT;
	}
}

typedef void (*do_yaw_function)(struct game_client *c, int yaw);
typedef void (*do_mouse_rot_function)(struct game_client *c, float x, float y);

static void do_generic_axis_rot(double *axisvel, int amount, double max_amount,
				double amount_inc, double amount_inc_fine)
{
	double delta_amount = abs(amount) > 1 ? amount_inc_fine : amount_inc;
	if (amount > 0) {
		if (*axisvel < max_amount)
			*axisvel += delta_amount;
	} else {
		if (*axisvel > -max_amount)
			*axisvel -= delta_amount;
	}
}

static void do_yaw(struct game_client *c, int yaw)
{
	struct snis_entity *ship = &go[c->ship_index];
	double max_yaw_velocity =
		(MAX_YAW_VELOCITY * ship->tsd.ship.power_data.maneuvering.i) / 255;

	ship->tsd.ship.computer_steering_time_left = 0; /* cancel any computer steering in progress */
	do_generic_axis_rot(&ship->tsd.ship.yaw_velocity, yaw, max_yaw_velocity,
			YAW_INCREMENT * yaw_adjust_factor * steering_adjust_factor,
			YAW_INCREMENT_FINE * yaw_fine_adjust_factor * steering_fine_adjust_factor);
}

static void do_pitch(struct game_client *c, int pitch)
{
	struct snis_entity *ship = &go[c->ship_index];
	double max_pitch_velocity =
		(MAX_PITCH_VELOCITY * ship->tsd.ship.power_data.maneuvering.i) / 255;

	ship->tsd.ship.computer_steering_time_left = 0; /* cancel any computer steering in progress */
	do_generic_axis_rot(&ship->tsd.ship.pitch_velocity, pitch, max_pitch_velocity,
			PITCH_INCREMENT * pitch_adjust_factor * steering_adjust_factor,
			PITCH_INCREMENT_FINE * pitch_fine_adjust_factor * steering_fine_adjust_factor);
}

static void do_roll(struct game_client *c, int roll)
{
	struct snis_entity *ship = &go[c->ship_index];
	double max_roll_velocity =
		(MAX_ROLL_VELOCITY * ship->tsd.ship.power_data.maneuvering.i) / 255;

	ship->tsd.ship.computer_steering_time_left = 0; /* cancel any computer steering in progress */
	do_generic_axis_rot(&ship->tsd.ship.roll_velocity, roll, max_roll_velocity,
			ROLL_INCREMENT * roll_adjust_factor * steering_adjust_factor,
			ROLL_INCREMENT_FINE * roll_fine_adjust_factor * steering_fine_adjust_factor);
}

static void do_sciball_yaw(struct game_client *c, int yaw)
{
	struct snis_entity *ship = &go[c->ship_index];
	const double max_yaw_velocity = MAX_YAW_VELOCITY;

	do_generic_axis_rot(&ship->tsd.ship.sciball_yawvel, yaw, max_yaw_velocity,
			YAW_INCREMENT, YAW_INCREMENT_FINE);
}

static void do_sciball_pitch(struct game_client *c, int pitch)
{
	struct snis_entity *ship = &go[c->ship_index];
	double max_pitch_velocity = MAX_PITCH_VELOCITY;

	do_generic_axis_rot(&ship->tsd.ship.sciball_pitchvel, pitch, max_pitch_velocity,
			PITCH_INCREMENT, PITCH_INCREMENT_FINE);
}

static void do_sciball_roll(struct game_client *c, int roll)
{
	struct snis_entity *ship = &go[c->ship_index];
	double max_roll_velocity = MAX_ROLL_VELOCITY;

	do_generic_axis_rot(&ship->tsd.ship.sciball_rollvel, roll, max_roll_velocity,
			ROLL_INCREMENT, ROLL_INCREMENT_FINE);
}

/* This is currently broken. It seems to hang the server.*/
static void do_sciball_mouse_rot(struct game_client *c, float x, float y)
{
	struct snis_entity *ship = &go[c->ship_index];

	set_player_sciball_orientation(ship, x, y);
}

static void do_manual_gunyaw(struct game_client *c, int yaw)
{
	struct snis_entity *ship = &go[c->ship_index];
	double max_yaw_velocity = MAX_YAW_VELOCITY;

	do_generic_axis_rot(&ship->tsd.ship.weap_yawvel, yaw, max_yaw_velocity,
			YAW_INCREMENT, YAW_INCREMENT_FINE);
}

static void do_manual_gunpitch(struct game_client *c, int pitch)
{
	struct snis_entity *ship = &go[c->ship_index];
	double max_pitch_velocity = MAX_PITCH_VELOCITY;

	do_generic_axis_rot(&ship->tsd.ship.weap_pitchvel, pitch, max_pitch_velocity,
			PITCH_INCREMENT, PITCH_INCREMENT_FINE);
}

static void do_demon_yaw(struct snis_entity *o, int yaw)
{
	double max_yaw_velocity = MAX_YAW_VELOCITY;

	do_generic_axis_rot(&o->tsd.ship.yaw_velocity, yaw, max_yaw_velocity,
			YAW_INCREMENT, YAW_INCREMENT_FINE);
}

static void do_demon_pitch(struct snis_entity *o, int pitch)
{
	double max_pitch_velocity = MAX_PITCH_VELOCITY;

	do_generic_axis_rot(&o->tsd.ship.pitch_velocity, pitch, max_pitch_velocity,
			PITCH_INCREMENT, PITCH_INCREMENT_FINE);
}

static void do_demon_roll(struct snis_entity *o, int roll)
{
	double max_roll_velocity = MAX_ROLL_VELOCITY;

	do_generic_axis_rot(&o->tsd.ship.roll_velocity, roll, max_roll_velocity,
			ROLL_INCREMENT, ROLL_INCREMENT_FINE);
}

static void do_sci_yaw(struct game_client *c, int yaw)
{
	struct snis_entity *ship = &go[c->ship_index];

	do_generic_axis_rot(&ship->tsd.ship.sci_yaw_velocity, yaw,
				MAX_SCI_YAW_VELOCITY, SCI_YAW_INCREMENT,
				SCI_YAW_INCREMENT_FINE);
}

static void do_robot_yaw(struct game_client *c, int yaw)
{
	struct damcon_data *d = &bridgelist[c->bridge].damcon;
	struct snis_damcon_entity *r = d->robot;

	do_generic_axis_rot(&r->tsd.robot.yaw_velocity, yaw,
			2.0 * MAX_SCI_YAW_VELOCITY, SCI_YAW_INCREMENT,
				SCI_YAW_INCREMENT_FINE);
}

static void do_sci_bw_yaw(struct game_client *c, int yaw)
{
	struct snis_entity *ship = &go[c->ship_index];

	do_generic_axis_rot(&ship->tsd.ship.sci_beam_width, yaw,
			MAX_SCI_BW_YAW_VELOCITY, sci_bw_yaw_increment,
			sci_bw_yaw_increment_fine);
	ship->tsd.ship.sci_beam_width = fabs(ship->tsd.ship.sci_beam_width);
	if (ship->tsd.ship.sci_beam_width < MIN_SCI_BEAM_WIDTH)
		ship->tsd.ship.sci_beam_width = MIN_SCI_BEAM_WIDTH;
}

static int read_and_unpack_buffer(struct game_client *c, unsigned char *buffer, char *format, ...)
{
        va_list ap;
        struct packed_buffer pb;
        int rc, size = calculate_buffer_size(format);

	rc = snis_readsocket(c->socket, buffer, size);
	if (rc != 0)
		return rc;
        packed_buffer_init(&pb, buffer, size);
        va_start(ap, format);
        rc = packed_buffer_extract_va(&pb, format, ap);
        va_end(ap);
        return rc;
}

static int process_generic_request_thrust(struct game_client *c, thrust_function thrust_fn)
{
	unsigned char buffer[10];
	uint8_t thrust;
	int rc;

	rc = read_and_unpack_buffer(c, buffer, "b", &thrust);
	if (rc)
		return rc;
	switch (thrust) {
	case THRUST_FORWARDS:
		thrust_fn(c, 1);
		break;
	case THRUST_BACKWARDS:
		thrust_fn(c, -1);
		break;
	default:
		break;
	}
	return 0;
}

#if 0
static int process_request_thrust(struct game_client *c)
{
	return process_generic_request_thrust(c, do_thrust);
}
#endif

static void send_demon_console_color_msg(uint8_t color, const char *fmt, ...)
{
	char buf[DEMON_CONSOLE_MSG_MAX];
	struct packed_buffer *pb;
	va_list arg_ptr;

	memset(buf, 0, sizeof(buf));
	va_start(arg_ptr, fmt);
	vsnprintf(buf, sizeof(buf) - 1, fmt, arg_ptr);
	va_end(arg_ptr);
	buf[sizeof(buf) - 1] = '\0';
	pb = packed_buffer_allocate(3 + sizeof(buf));
	packed_buffer_append(pb, "bbb", OPCODE_CONSOLE_OP, OPCODE_CONSOLE_SUBCMD_ADD_TEXT, color);
	packed_buffer_append_raw(pb, buf, sizeof(buf));
	send_packet_to_all_clients(pb, ROLE_DEMON);
}

static void send_demon_console_msg(const char *fmt, ...)
{
	char buf[DEMON_CONSOLE_MSG_MAX];
	struct packed_buffer *pb;
	va_list arg_ptr;

	memset(buf, 0, sizeof(buf));
	va_start(arg_ptr, fmt);
	vsnprintf(buf, sizeof(buf) - 1, fmt, arg_ptr);
	va_end(arg_ptr);
	buf[sizeof(buf) - 1] = '\0';
	pb = packed_buffer_allocate(3 + sizeof(buf));
	packed_buffer_append(pb, "bbb", OPCODE_CONSOLE_OP, OPCODE_CONSOLE_SUBCMD_ADD_TEXT, 255);
	packed_buffer_append_raw(pb, buf, sizeof(buf));
	send_packet_to_all_clients(pb, ROLE_DEMON);
}

static int process_demon_thrust(struct game_client *c)
{
	struct snis_entity *o;
	unsigned char buffer[10];
	uint8_t thrust;
	uint32_t oid;
	int i, rc;

	rc = read_and_unpack_buffer(c, buffer, "wb", &oid, &thrust);
	if (rc)
		return rc;
	if (!(c->role & ROLE_DEMON))
		return 0;
	pthread_mutex_lock(&universe_mutex);
	i = lookup_by_id(oid);
	if (i < 0)
		goto out;
	o = &go[i];
	if (o->type != OBJTYPE_SHIP2)
		goto out;

	switch (thrust) {
	case THRUST_FORWARDS:
		do_demon_thrust(o, 1);
		break;
	case THRUST_BACKWARDS:
		do_demon_thrust(o, -1);
		break;
	default:
		break;
	}
out:
	pthread_mutex_unlock(&universe_mutex);
	return 0;
}

static int process_demon_move_object(struct game_client *c)
{
	struct snis_entity *o;
	unsigned char buffer[sizeof(struct demon_move_object_packet)];
	uint32_t oid;
	double dx, dy, dz;
	int i, rc;
	union quat orientation;

	rc = read_and_unpack_buffer(c, buffer, "wSSSQ", &oid,
			&dx, (int32_t) UNIVERSE_DIM,
			&dy, (int32_t) UNIVERSE_DIM,
			&dz, (int32_t) UNIVERSE_DIM,
			&orientation);
	if (rc)
		return rc;
	if (!(c->role & ROLE_DEMON))
		return 0;
	pthread_mutex_lock(&universe_mutex);
	i = lookup_by_id(oid);
	if (i < 0 || !go[i].alive)
		goto out;
	o = &go[i];
	if (o->type == OBJTYPE_SHIP2 || o->type == OBJTYPE_SHIP1)
		warp_ship(o, o->x + dx, o->y + dy, o->z + dz);
	else
		set_object_location(o, o->x + dx, o->y + dy, o->z + dz);
	o->orientation = orientation;
	o->timestamp = universe_timestamp;
out:
	pthread_mutex_unlock(&universe_mutex);
	return 0;
}

static int process_request_robot_thrust(struct game_client *c)
{
	return process_generic_request_thrust(c, do_robot_thrust);
}

static int process_request_robot_cmd(struct game_client *c)
{
	double x, y;
	struct damcon_data *d;
	int rc;
	uint8_t subcmd;
	unsigned char buffer[30];

	rc = read_and_unpack_buffer(c, buffer, "b", &subcmd);
	if (rc < 0)
		return rc;
	switch (subcmd) {
	case OPCODE_ROBOT_SUBCMD_STG:
	case OPCODE_ROBOT_SUBCMD_LTG:
		rc = read_and_unpack_buffer(c, buffer, "SS",
					&x, (int32_t) DAMCONXDIM, &y, (int32_t) DAMCONYDIM);
		if (rc < 0)
			return rc;
		pthread_mutex_lock(&universe_mutex);
		d = &bridgelist[c->bridge].damcon;
		if (subcmd == OPCODE_ROBOT_SUBCMD_STG) {
			d->robot->tsd.robot.short_term_goal_x = x;
			d->robot->tsd.robot.short_term_goal_y = y;
		} else {
			d->robot->tsd.robot.long_term_goal_x = x;
			d->robot->tsd.robot.long_term_goal_y = y;
		}
		if (d->robot->tsd.robot.autonomous_mode == DAMCON_ROBOT_MANUAL_MODE)
			d->robot->tsd.robot.autonomous_mode = DAMCON_ROBOT_SEMI_AUTONOMOUS;
		pthread_mutex_unlock(&universe_mutex);
		break;
	default:
		return -1;
	}
	return 0;
}

static void send_ship_sdata_packet(struct game_client *c, struct ship_sdata_packet *sip);
static void pack_and_send_ship_sdata_packet(struct game_client *c, struct snis_entity *o)
{
	struct ship_sdata_packet p; 

	memset(p.name, 0, sizeof(p.name));
	strcpy(p.name, o->sdata.name);
	p.id = o->id;
	if (o->type == OBJTYPE_SHIP2)
		p.subclass = o->tsd.ship.shiptype;
	else
		p.subclass = 0;
	p.shield_strength = o->sdata.shield_strength;
	p.shield_wavelength = o->sdata.shield_wavelength;
	p.shield_width = o->sdata.shield_width;
	p.shield_depth = o->sdata.shield_depth;
	p.faction = o->sdata.faction;
	p.lifeform_count = 0;
	if (o->type == OBJTYPE_SHIP2 || o->type == OBJTYPE_SHIP1)
		p.lifeform_count = o->tsd.ship.lifeform_count;
	else if (o->type == OBJTYPE_STARBASE)
		p.lifeform_count = o->tsd.starbase.lifeform_count;
	else
		p.lifeform_count = 0;
	send_ship_sdata_packet(c, &p);
}

static void send_update_ship_cargo_info(struct game_client *c, struct snis_entity *o)
{
	struct packed_buffer *pb;
	int i, count;
	uint32_t item;
	double qty;

	/* TODO: maybe we do not always send all this info or throttle it some how */

	if (o->type != OBJTYPE_SHIP1 && o->type != OBJTYPE_SHIP2)
		return;

	/* If ship has no cargo, skip it */
	count = 0;
	for (i = 0; i < ship_type[o->tsd.ship.shiptype].ncargo_bays; i++) {
		struct cargo_container_contents *cbc = &o->tsd.ship.cargo[i].contents;
		if (cbc->item == -1 || cbc->qty == 0.0) /* empty */
			continue;
		count++;
	}
	if (count == 0)
		return;

	/* Ship has cargo, build up the buffer, opcode, id, non-empty bay count, contents...
	 * 6 for "bwb", 9 for "bwS" per non-empty bay.
	 */
	pb = packed_buffer_allocate(6 + 9 * MAX_CARGO_BAYS_PER_SHIP);
	packed_buffer_append(pb, "bwb", OPCODE_UPDATE_SHIP_CARGO_INFO, o->id, (uint8_t) count);

	/* Build up the contents */
	for (i = 0; i < ship_type[o->tsd.ship.shiptype].ncargo_bays; i++) {
		struct cargo_container_contents *cbc = &o->tsd.ship.cargo[i].contents;
		if (cbc->item == -1 || cbc->qty == 0.0) /* empty */
			continue;
		item = cbc->item;
		qty = cbc->qty;
		packed_buffer_append(pb, "bwS", (uint8_t) i, item, qty, (int32_t) 1000000);
	}
	pb_queue_to_client(c, pb);
}

static int save_sdata_bandwidth(void)
{
	/* TODO: something clever here. */
	if (snis_randn(100) > 25)
		return 1;
	return 0;
}

static int should_send_sdata(struct game_client *c, struct snis_entity *ship,
				struct snis_entity *o)
{
	double dist, dist2, angle, range, range2, range3, A1, A2;
	int in_beam, bw, pwr, divisor, dr;

	/*
	 * Figure out if we should send sdata. Contributing factors:
	 *
	 * 1. science beam direction and angle.
	 * 2. science beam power
	 * 3. proximity
	 * 4. science beam zoom level
	 * 5. nebula effects
	 */

	if (ship == o) /* always send our own sdata to ourself */
		return 1;

	/* Always send sdata for our own faction ships for RTS mode */
	if (rts_mode && ship->sdata.faction == go[c->ship_index].sdata.faction)
		return 1;

	range2 = ship->tsd.ship.scibeam_range * ship->tsd.ship.scibeam_range;
	range3 = 4.0 * ship->tsd.ship.scibeam_range * 4.0 * ship->tsd.ship.scibeam_range;
	/* distance to target... */
	dist2 = object_dist2(o, ship);
	if ((dist2 > range2 && o->type != OBJTYPE_PLANET && o->type != OBJTYPE_STARBASE) ||
			(dist2 > range3))  /* too far, no sdata for you. */
		return 0;

	/* super close? Send it. */
	if (dist2 < 1.2 * (MIN_SCIENCE_SCREEN_RADIUS * MIN_SCIENCE_SCREEN_RADIUS))
		return 1;

	range = sqrt(range2);
	dist = sqrt(dist2);
	bw =  ship->tsd.ship.sci_beam_width * 180.0 / M_PI;
	pwr = ship->tsd.ship.power_data.sensors.i;

	/* Compute radius of ship blip */
	divisor = hypot((float) bw + 1, 256.0 - pwr);
	dr = (int) (dist / (3.0 * XKNOWN_DIM / divisor));
	dr = dr * MAX_SCIENCE_SCREEN_RADIUS / range;
#if 0
	if (nebula_factor) {
		dr = dr * 10; 
		dr += 200;
	}
#endif
	if (dr >= 5 && o->type != OBJTYPE_STARBASE && o->type != OBJTYPE_PLANET)
		return 0;

	/* Is the target in the beam? */
	angle = atan2(o->z - ship->z, o->x - ship->x);
	in_beam = 1;
	A1 = ship->tsd.ship.scibeam_a1;
	A2 = ship->tsd.ship.scibeam_a2;

	if (!(A2 < 0 && A1 > 0 && fabs(A1) > M_PI / 2.0)) {
		if (angle < A1)
			in_beam = 0;
		if (angle > A2)
			in_beam = 0;
	} else {
		if (angle < 0 && angle > A2)
			in_beam = 0;
		if (angle > 0 && angle < A1)
			in_beam = 0;
	}
	return in_beam;
}

static void send_update_sdata_packets(struct game_client *c, struct snis_entity *o)
{
	struct snis_entity *ship;
	int i;

	/* Assume universe mutex held. */
	if (!o->alive)
		return;
	i = lookup_by_id(c->shipid);
	if (i < 0)
		return;
	ship = &go[i];
	if (save_sdata_bandwidth() && o != &go[c->ship_index]) {
#if GATHER_OPCODE_STATS
		write_opcode_stats[OPCODE_SHIP_SDATA].count_not_sent++;
		write_opcode_stats[OPCODE_UPDATE_SHIP_CARGO_INFO].count_not_sent++;
#endif
		return;
	}
	if (!should_send_sdata(c, ship, o)) {
#if GATHER_OPCODE_STATS
		write_opcode_stats[OPCODE_SHIP_SDATA].count_not_sent++;
		write_opcode_stats[OPCODE_UPDATE_SHIP_CARGO_INFO].count_not_sent++;
#endif
		return;
	}
	pack_and_send_ship_sdata_packet(c, o);
	send_update_ship_cargo_info(c, o);
}

static int process_role_onscreen(struct game_client *c)
{
	unsigned char buffer[10];
	uint8_t new_displaymode;
	int rc;

	rc = read_and_unpack_buffer(c, buffer, "b", &new_displaymode);
	if (rc)
		return rc;
	if (new_displaymode >= DISPLAYMODE_FONTTEST)
		new_displaymode = DISPLAYMODE_MAINSCREEN;
	send_packet_to_all_clients_on_a_bridge(c->shipid, 
			snis_opcode_pkt("bb", OPCODE_ROLE_ONSCREEN, new_displaymode),
			ROLE_MAIN);
	bridgelist[c->bridge].current_displaymode = new_displaymode;
	return 0;
}

static int process_sci_details(struct game_client *c)
{
	unsigned char buffer[10];
	uint8_t new_details;
	int rc;

	rc = read_and_unpack_buffer(c, buffer, "b", &new_details);
	if (rc)
		return rc;
	/* just turn it around and fan it out to all the right places */
	if (new_details > 4)
		new_details = 0;
	send_packet_to_requestor_plus_role_on_a_bridge(c, 
			snis_opcode_pkt("bb", OPCODE_SCI_DETAILS,
			new_details), ROLE_MAIN);
	return 0;
}

static int process_sci_align_to_ship(struct game_client *c)
{
	if (c->ship_index == (uint32_t) -1)
		return 0;
	/* Snap sciball orientation to ship orientation */
	struct snis_entity *o = &go[c->ship_index];
	pthread_mutex_lock(&universe_mutex);
	o->tsd.ship.sciball_orientation = o->orientation;
	o->timestamp = universe_timestamp;
	pthread_mutex_unlock(&universe_mutex);
	return 0;
}

/* This is for mouse control of weapons */
static int process_request_weapons_yaw_pitch(struct game_client *c)
{
	unsigned char buffer[10];
	double yaw, pitch;
	union quat y, p;
	int i, rc;
	struct snis_entity *o;

	rc = read_and_unpack_buffer(c, buffer, "RR", &yaw, &pitch);
	if (rc)
		return rc;

	i = lookup_by_id(c->shipid);
	if (i < 0)
		return 0;
	o = &go[i];
	yaw = fmod(yaw, 2.0 * M_PI); 
	pitch = fmod(pitch, M_PI / 2.0); 

	quat_init_axis(&y, 0, 1, 0, yaw);
	quat_init_axis(&p, 0, 0, 1, pitch);
	quat_mul(&o->tsd.ship.weap_orientation, &y, &p);
	quat_normalize_self(&o->tsd.ship.weap_orientation);
	
	return 0;
}

static void science_select_target(struct game_client *c, uint8_t selection_type, uint32_t id)
{
	/* just turn it around and fan it out to all the right places */
	send_packet_to_all_clients_on_a_bridge(c->shipid,
			snis_opcode_pkt("bbw", OPCODE_SCI_SELECT_TARGET, selection_type, id),
			ROLE_SCIENCE | ROLE_NAVIGATION | ROLE_WEAPONS);
	/* remember sci selection for retargeting mining bot */
	if (selection_type == OPCODE_SCI_SELECT_TARGET_TYPE_OBJECT) {
		bridgelist[c->bridge].selected_waypoint = -1;
		bridgelist[c->bridge].science_selection = id;
		schedule_callback2(event_callback, &callback_schedule,
			"object-scanned-event", (double) c->shipid, (double) id);
	}
	if (selection_type == OPCODE_SCI_SELECT_TARGET_TYPE_WAYPOINT) {
		if (id < bridgelist[c->bridge].nwaypoints || id == (uint32_t) -1) {
			bridgelist[c->bridge].selected_waypoint = id;
			bridgelist[c->bridge].science_selection = (uint32_t) -1;
		}
	}
}

static int process_sci_select_target(struct game_client *c)
{
	unsigned char buffer[10];
	uint8_t selection_type;
	uint32_t id;
	int rc;

	rc = read_and_unpack_buffer(c, buffer, "bw", &selection_type, &id);
	if (rc)
		return rc;
	science_select_target(c, selection_type, id);
	return 0;
}

static int process_weap_select_target(struct game_client *c)
{
	unsigned char buffer[10];
	uint32_t id;
	int rc, i, index;
	struct snis_entity *ship;

	rc = read_and_unpack_buffer(c, buffer, "w", &id);
	if (rc)
		return rc;
	index = lookup_by_id(id);
	if (index < 0)
		return 0;
	i = lookup_by_id(c->shipid);
	if (i < 0)
		return 0;
	ship = &go[i];
	ship->tsd.ship.nai_entries = 1;
	ship->tsd.ship.ai[0].u.attack.victim_id = go[index].id;
	return 0;
}

typedef void (*meta_comms_func)(char *name, struct game_client *c, char *txt);

static void meta_comms_help(char *name, struct game_client *c, char *txt)
{
	int i;
	const char *hlptxt[] = {
		"COMMUNICATIONS",
		"  CONTROLS",
		"  * ZOOM CONTROLS MAIN SCREEN ZOOM",
		"  * RED ALERT SOUNDS RED ALERT ALARM",
		"  * TOP ROW OF BUTTONS CONTROLS MAIN SCREEN",
		"  COMMANDS",
		"  * COMMANDS ARE PRECEDED BY FORWARD SLASH ->  /",
		"  * /help",
		"  * /computer <english request for computer>",
		"  * /channel channel-number - change current channel",
		"  * /eject cargo-bay-number - eject cargo",
		"  * /hail ship-name - hail ship or starbase on current channel",
		"  * /inventory - report inventory of ship's cargo hold",
		"  * /passengers - report list of passengers",
		"  * /antenna <bearing> <mark> - aim the high gain antenna",
		"  * /about - information about the game",
		"",
		0,
	};
	for (i = 0; hlptxt[i]; i++)
		send_comms_packet(NULL, "", bridgelist[c->bridge].comms_channel, hlptxt[i]);
}

static void meta_comms_about(char *name, struct game_client *c, char *txt)
{
	int i;
	const char *abouttxt[] = {
		"Space Nerds In Space is free software",
		"Source code can be found here:",
		"https://github.com/smcameron/space-nerds-in-space",
		"--------------------------",
		0,
	};
	send_comms_packet(NULL, "", bridgelist[c->bridge].comms_channel, "--------------------------");
	send_comms_packet(NULL, "", bridgelist[c->bridge].comms_channel, "SPACE NERDS IN SPACE CLIENT:");
	if (c->build_info[0])
		send_comms_packet(NULL, "", bridgelist[c->bridge].comms_channel, c->build_info[0]);
	else
		send_comms_packet(NULL, "", bridgelist[c->bridge].comms_channel, "UNKNOWN SNIS CLIENT");
	if (c->build_info[1])
		send_comms_packet(NULL, "", bridgelist[c->bridge].comms_channel, c->build_info[1]);
	send_comms_packet(NULL, "", bridgelist[c->bridge].comms_channel, "SPACE NERDS IN SPACE SERVER:");
	send_comms_packet(NULL, "", bridgelist[c->bridge].comms_channel, BUILD_INFO_STRING1);
	send_comms_packet(NULL, "", bridgelist[c->bridge].comms_channel, BUILD_INFO_STRING2);
	send_comms_packet(NULL, "", bridgelist[c->bridge].comms_channel, "--------------------------");
	for (i = 0; abouttxt[i]; i++)
		send_comms_packet(NULL, "", bridgelist[c->bridge].comms_channel, abouttxt[i]);
}

static void get_location_name(int id, char *buffer, int bufsize)
{
	int i;

	if (id == -1) {
		snprintf(buffer, bufsize, "N/A");
		return;
	}
	i = lookup_by_id(id);
	if (i < 0) {
		snprintf(buffer, bufsize, "N/A");
		return;
	}
	snprintf(buffer, bufsize, "%s", go[i].sdata.name);
}

static void format_date(char *buf, int bufsize, double date)
{
	buf[bufsize - 1] = '\0';
	snprintf(buf, bufsize, "%-8.1f", FICTIONAL_DATE(date));
}

static void format_due_date(char *buf, int bufsize, double date)
{
	buf[bufsize - 1] = '\0';
	if (date < 0) {
		snprintf(buf, bufsize, "N/A");
		return;
	}
	format_date(buf, bufsize, date);
}

static void meta_comms_inventory(char *name, struct game_client *c, char *txt)
{
	int i;
	int ch = bridgelist[c->bridge].comms_channel;
	struct snis_entity *ship;
	int passenger_count = 0;

	i = lookup_by_id(bridgelist[c->bridge].shipid);
	if (i < 0) {
		printf("Non fatal error in %s at %s:%d\n",
			__func__, __FILE__, __LINE__);
		return;
	}
	ship = &go[i];

	/* FIXME: sending this on current channel -- should make it private to ship somehow */
	send_comms_packet(NULL, name, ch, " PASSENGER LIST:");
	for (i = 0; i < npassengers; i++) {
		if (passenger[i].location == ship->id) {
			int x = lookup_by_id(passenger[i].destination);
			send_comms_packet(NULL, name, ch, "%2d. FARE %4d DEST: %10s NAME: %30s\n",
					passenger_count + 1, passenger[i].fare,
					x < 0 ? "UNKNOWN" : go[x].sdata.name,
					passenger[i].name);
			passenger_count++;
		}
	}
	send_comms_packet(NULL, name, ch, "  %d PASSENGERS ABOARD\n", passenger_count);
	if (strncasecmp(txt, "/passengers", 11) == 0)
		return;
	send_comms_packet(NULL, name, ch, " INVENTORY OF HOLD:");
	send_comms_packet(NULL, name, ch, " --------------------------------------");
	if (ship->tsd.ship.ncargo_bays == 0) {
		send_comms_packet(NULL, name, ch, "   SHIP HAS ZERO CARGO BAYS.");
		send_comms_packet(NULL, name, ch, " --------------------------------------");
		return;
	}
	for (i = 0; i < ship->tsd.ship.ncargo_bays; i++) {
		char *itemname, *unit;
		struct cargo_container_contents *ccc = &ship->tsd.ship.cargo[i].contents;
		float qty;
		char origin[20], dest[20];

		if (ccc->item == -1 || ccc->qty < 0.001) {
			send_comms_packet(NULL, name, ch, "    CARGO BAY %d: ** EMPTY **", i);
			/* If we somehow got a zero quantity but non-empty item, or non-zero or negative
			 * quantity of "nothing", then empty the item. Suppress any "no tea" situation.
			 * This should not happen, so log a message if it does.
			 */
			if (ccc->item != -1 || ccc->qty >= 0.001) {
				fprintf(stderr,
					"No tea detected and suppressed in cargo bay %d, ship id %d (q=%f,i=%d).\n",
					i, ship->id, ccc->qty, ccc->item);
				ccc->item = -1;
				ccc->qty = 0.0;
			}
		} else {
			char due_date[20];
			itemname = commodity[ccc->item].name;
			unit = commodity[ccc->item].unit;
			qty = ccc->qty;
			get_location_name(ship->tsd.ship.cargo[i].origin, origin, sizeof(origin));
			get_location_name(ship->tsd.ship.cargo[i].dest, dest, sizeof(dest));
			format_due_date(due_date, sizeof(due_date), (double)
				ship->tsd.ship.cargo[i].due_date);
			send_comms_packet(NULL, name, ch,
				"    CARGO BAY %d: %4.2f %s %s - PAID $%.2f ORIG %10s DEST %10s DUE %s",
					i, qty, unit, itemname, ship->tsd.ship.cargo[i].paid,
					origin, dest, due_date);
		}
	}
	send_comms_packet(NULL, name, ch, " --------------------------------------");
	if (ship->tsd.ship.mining_bots > 0)
		send_comms_packet(NULL, name, ch, " %d MINING BOT%s DOCKED AND STOWED.",
			ship->tsd.ship.mining_bots,
			ship->tsd.ship.mining_bots == 1 ? "" : "S");
	send_comms_packet(NULL, name, ch, " CASH ON HAND:  $%.2f", ship->tsd.ship.wallet);
	if (bridgelist[c->bridge].warp_gate_ticket.ipaddr != 0) /* ticket currently held? */
		send_comms_packet(NULL, name, ch, " HOLDING WARP-GATE TICKET TO %s\n",
			bridgelist[c->bridge].warp_gate_ticket.location);
	if (bridgelist[c->bridge].nship_id_chips > 0) {
		for (i = 0; i < bridgelist[c->bridge].nship_id_chips; i++)
			send_comms_packet(NULL, name, ch, " SHIP ID CHIP REGISTERED AS %u",
						bridgelist[c->bridge].ship_id_chip[i]);
	}
	send_comms_packet(NULL, name, ch, " --------------------------------------");
}

static void meta_comms_channel(char *name, struct game_client *c, char *txt)
{
	int rc;
	uint32_t newchannel;

	rc = sscanf(txt, "%*[/cChHaAnNeElL] %u\n", &newchannel);
	if (rc != 1) {
		send_comms_packet(NULL, name, bridgelist[c->bridge].comms_channel,
			"INVALID CHANNEL - CURRENT CHANNEL %u", bridgelist[c->bridge].comms_channel);
		return;
	}
	send_comms_packet(NULL, name, bridgelist[c->bridge].comms_channel, "TRANSMISSION TERMINATED ON CHANNEL %u",
			bridgelist[c->bridge].comms_channel);
	bridgelist[c->bridge].comms_channel = newchannel;

	if (bridgelist[c->bridge].npcbot.channel != newchannel) {
		/* disconnect npc bot */
		bridgelist[c->bridge].npcbot.channel = (uint32_t) -1;
		bridgelist[c->bridge].npcbot.object_id = (uint32_t) -1;
		bridgelist[c->bridge].npcbot.current_menu = NULL;
		bridgelist[c->bridge].npcbot.special_bot = NULL;
	}

	/* Note: client snoops this channel change message. */
	send_comms_packet(NULL, name, newchannel, "%s%u", COMMS_CHANNEL_CHANGE_MSG, newchannel);
}

static void meta_comms_eject(char *name, struct game_client *c, char *txt)
{
	int i, cargobay, rc, cc, item;
	float qty;
	struct snis_entity *ship;
	const union vec3 up = { { 0.0f, 1.0f, 0.0f } };
	union vec3 container_loc, container_vel;

	i = lookup_by_id(c->shipid);
	if (i < 0)
		return;
	ship = &go[i];

	rc = sscanf(txt, "/%*[eEjJcCtT] %d\n", &cargobay);
	if (rc != 1)
		goto invalid_cargo_bay;
	if (cargobay < 0 || cargobay >= ship->tsd.ship.ncargo_bays)
		goto invalid_cargo_bay;
	if (ship->tsd.ship.cargo[cargobay].contents.item == -1 ||
			ship->tsd.ship.cargo[cargobay].contents.qty <= 0.0)
		goto empty_cargo_bay;
	item = ship->tsd.ship.cargo[cargobay].contents.item;
	qty = ship->tsd.ship.cargo[cargobay].contents.qty;
	ship->tsd.ship.cargo[cargobay].contents.item = -1;
	ship->tsd.ship.cargo[cargobay].contents.qty = 0.0f;
	ship->tsd.ship.cargo[cargobay].paid = 0.0f;
	ship->tsd.ship.cargo[cargobay].origin = -1;
	ship->tsd.ship.cargo[cargobay].dest = -1;
	ship->tsd.ship.cargo[cargobay].due_date = -1;

	quat_rot_vec(&container_loc, &up, &ship->orientation);
	vec3_mul_self(&container_loc, 160.0);
	quat_rot_vec(&container_vel, &up, &ship->orientation);
	vec3_mul_self(&container_vel, 5.0);

	container_vel.v.x += ship->vx;
	container_vel.v.y += ship->vy;
	container_vel.v.z += ship->vz;
	container_loc.v.x += ship->x;
	container_loc.v.y += ship->y;
	container_loc.v.z += ship->z;

	rc = add_cargo_container(container_loc.v.x, container_loc.v.y, container_loc.v.z,
				container_vel.v.x, container_vel.v.y, container_vel.v.z,
				item, qty, 1);
	if (rc >= 0)
		cc = go[rc].id;
	else
		cc = -1;
	send_comms_packet(NULL, name, bridgelist[c->bridge].comms_channel, "CARGO BAY %d EJECTED", cargobay);
	schedule_callback7(event_callback, &callback_schedule,
					"player-ejected-cargo-event", (double) ship->id, (double) cc,
					ship->x, ship->y, ship->z, (double) item, (double) qty);
	return;

invalid_cargo_bay:
	send_comms_packet(NULL, name, bridgelist[c->bridge].comms_channel, "INVALID CARGO BAY");
	return;

empty_cargo_bay:
	send_comms_packet(NULL, name, bridgelist[c->bridge].comms_channel, "EMPTY CARGO BAY");
	return;
}

static uint32_t find_free_channel(void)
{
	/* FIXME: this needs to actually find a free channel, not just fail to be unlucky */
	return snis_randn(100000); /* simplest possible thing that might work. */
}

static void npc_menu_item_not_implemented(struct npc_menu_item *item,
					char *npcname, struct npc_bot_state *botstate)
{
	send_comms_packet(NULL, npcname, botstate->channel,
				"  SORRY, THAT IS NOT IMPLEMENTED");
}

static void npc_menu_item_sign_off(struct npc_menu_item *item,
					char *npcname, struct npc_bot_state *botstate)
{
	int i;
	struct snis_entity *transmitter = NULL;

	i = lookup_by_id(botstate->object_id);
	if (i >= 0)
		transmitter = &go[i];
	send_comms_packet(transmitter, npcname, botstate->channel,
				"  IT HAS BEEN A PLEASURE SERVING YOU");
	send_comms_packet(transmitter, npcname, botstate->channel,
				"  ZX81 SERVICE ROBOT TERMINATING TRANSMISSION");
	botstate->channel = (uint32_t) -1;
	botstate->object_id = (uint32_t) -1;
	botstate->current_menu = NULL;
}

static void npc_send_parts_menu(char *npcname, struct npc_bot_state *botstate)
{
	int i;
	struct snis_entity *sb;

	i = lookup_by_id(botstate->object_id);
	if (i < 0)
		return;
	sb = &go[i];

	send_comms_packet(sb, npcname, botstate->channel, "");
	send_comms_packet(sb, npcname, botstate->channel, "  -- BUY %s PARTS --",
				damcon_system_name(botstate->parts_menu));
	for (i = 0; i < DAMCON_PARTS_PER_SYSTEM; i++) {
		float price = sb->tsd.starbase.part_price[botstate->parts_menu *
								DAMCON_PARTS_PER_SYSTEM + i];
		send_comms_packet(sb, npcname, botstate->channel, "  %c:   $%.2f   %s\n", i + 'A', price,
				damcon_part_name(botstate->parts_menu, i));
	}
	send_comms_packet(sb, npcname, botstate->channel, "  0:   PREVIOUS MENU");
	send_comms_packet(sb, npcname, botstate->channel, "");
}

static void parts_buying_npc_bot(struct snis_entity *o, int bridge,
		char *name, char *msg)
{
	int i, rc, selection;
	char sel;
	float range2;
	char *n = o->sdata.name;
	struct snis_entity *ship;
	struct npc_bot_state *botstate = &bridgelist[bridge].npcbot;
	uint32_t channel = botstate->channel;
	float price;

	i = lookup_by_id(bridgelist[bridge].shipid);
	if (i < 0) {
		printf("Non fatal error at %s:%s:%d\n",
			__FILE__, __func__, __LINE__);
		return;
	}
	ship = &go[i];

	range2 = dist3dsqrd(ship->x - o->x, ship->y - o->y, ship->z - o->z);

	rc = sscanf(msg, "%c", (char *) &sel);
	if (rc != 1) {
		npc_send_parts_menu(name, botstate);
		return;
	}
	selection = sel;
	if (selection == '0') {
		botstate->special_bot = NULL;
		send_to_npcbot(bridge, name, ""); /* poke generic bot so he says something */
		return;
	}
	selection = toupper(selection);
	if (selection >= 'A' && selection <= 'A' + DAMCON_PARTS_PER_SYSTEM - 1)
		selection = selection - 'A';
	else
		selection = -1;

	if (selection == -1) {
		npc_send_parts_menu(name, botstate);
		return;
	}

	/* check transporter range */
	if (range2 > TRANSPORTER_RANGE * TRANSPORTER_RANGE) {
		send_comms_packet(o, n, channel,
			" TRANSACTION NOT POSSIBLE - TRANSPORTER RANGE EXCEEDED");
		return;
	}
	price = o->tsd.starbase.part_price[botstate->parts_menu * DAMCON_PARTS_PER_SYSTEM + selection];
	if (price > ship->tsd.ship.wallet) {
		send_comms_packet(o, n, channel, " INSUFFICIENT FUNDS");
		return;
	}
	ship->tsd.ship.wallet -= price;
	send_comms_packet(o, n, channel, " THANK YOU FOR YOUR PURCHASE.");
	instantly_repair_damcon_part(&bridgelist[bridge].damcon, botstate->parts_menu, selection); /* "buy" the part. */
}

static void npc_menu_item_buy_parts(struct npc_menu_item *item,
				char *npcname, struct npc_bot_state *botstate)
{
	struct bridge_data *b;
	int i, bridge;

	i = lookup_by_id(botstate->object_id);
	if (i < 0)
		return;

	/* find our bridge... */
	b = container_of(botstate, struct bridge_data, npcbot);
	bridge = b - bridgelist;

	botstate->parts_menu = (item - &repairs_and_fuel_menu[0] - 1) %
				(DAMCON_SYSTEM_COUNT - 1);
	botstate->special_bot = parts_buying_npc_bot;
	botstate->special_bot(&go[i], bridge, (char *) b->shipname, "");
}

static int lookup_by_registration(uint32_t id)
{
	int i;

	for (i = 0; i <= snis_object_pool_highest_object(pool); i++)
		if (go[i].id == id || (go[i].type == OBJTYPE_DERELICT && go[i].tsd.derelict.orig_ship_id == id))
			return i;
	return -1;
}
static void starbase_registration_query_npc_bot(struct snis_entity *o, int bridge,
		char *name, char *msg)
{
	int i, c;
	char *n = o->sdata.name;
	uint32_t channel = bridgelist[bridge].npcbot.channel;
	int rc, selection;

	i = lookup_by_id(bridgelist[bridge].shipid);
	if (i < 0) {
		printf("Non fatal error at %s:%s:%d\n",
			__FILE__, __func__, __LINE__);
		return;
	}

	if (strcasecmp(msg, "Q") == 0) {
		bridgelist[bridge].npcbot.special_bot = NULL; /* deactivate cargo buying bot */
		send_to_npcbot(bridge, name, ""); /* poke generic bot so he says something */
	}

	rc = sscanf(msg, "%d", &selection);
	if (rc != 1) {
		selection = -1;
		send_comms_packet(o, n, channel,
			" ENTER REGISTRATION ID (Q to quit) ");
		return;
	}

	i = lookup_by_id(selection);
	if (i < 0) {
		i = lookup_by_registration(selection);
		if (i < 0) {
			send_comms_packet(o, n, channel, "NO SUCH REGISTRATION FOUND");
			return;
		}
	}
	if (go[i].type != OBJTYPE_SHIP1 &&
		go[i].type != OBJTYPE_SHIP2 &&
		go[i].type != OBJTYPE_DERELICT) {
		send_comms_packet(o, n, channel, "NO SUCH REGISTRATION FOUND");
		return;
	}
	send_comms_packet(o, n, channel, "REGISTRATION ID - %d", selection);
	send_comms_packet(o, n, channel, "NAME - %s", go[i].sdata.name);
	send_comms_packet(o, n, channel, "MAKE - %s",
			corporation_get_name(ship_type[go[i].tsd.ship.shiptype].manufacturer));
	send_comms_packet(o, n, channel, "MODEL - %s", ship_type[go[i].tsd.ship.shiptype].class);
	for (i = 0; i >= 0;) {
		i = ship_registry_get_next_entry(&ship_registry, selection, i);
		if (i < 0)
			break;
		switch (ship_registry.entry[i].type) {
		case SHIP_REG_TYPE_OWNER:
			send_comms_packet(o, n, channel, "OWNER - %s",
					corporation_get_name(ship_registry.entry[i].owner));
			break;
		case SHIP_REG_TYPE_REGISTRATION:
			send_comms_packet(o, n, channel, "REGISTRATION - %s", ship_registry.entry[i].entry);
			break;
		case SHIP_REG_TYPE_BOUNTY:
			c = lookup_by_id(ship_registry.entry[i].bounty_collection_site);
			if (c >= 0 && go[c].type == OBJTYPE_STARBASE && go[c].alive) {
				send_comms_packet(o, n, channel, "WANTED FOR - %s", ship_registry.entry[i].entry);
				int p = lookup_by_id(go[c].tsd.starbase.associated_planet_id);
				if (p >= 0 && go[c].tsd.starbase.associated_planet_id != (uint32_t) -1)
					send_comms_packet(o, n, channel, "BOUNTY - $%.0f COLLECTIBLE AT %s ORBITING %s",
						ship_registry.entry[i].bounty,
						c < 0 ? "UNKNOWN" : go[c].sdata.name, go[p].sdata.name);
				else
					send_comms_packet(o, n, channel, "BOUNTY - $%.0f COLLECTIBLE AT %s",
						ship_registry.entry[i].bounty,
						c < 0 ? "UNKNOWN" : go[c].sdata.name);
			}
			break;
		case SHIP_REG_TYPE_CAPTAIN:
			send_comms_packet(o, n, channel, "CAPTAIN - %s", ship_registry.entry[i].entry);
			break;
		case SHIP_REG_TYPE_COMMENT:
			send_comms_packet(o, n, channel, " - %s", ship_registry.entry[i].entry);
			break;
		}
		i++;
	}
}

static void npc_menu_item_query_ship_registration(struct npc_menu_item *item,
					char *npcname, struct npc_bot_state *botstate)
{
	struct bridge_data *b;
	int i, bridge;

	i = lookup_by_id(botstate->object_id);
	if (i < 0) {
		printf("nonfatal bug in %s at %s:%d\n", __func__, __FILE__, __LINE__);
		return;
	}

	/* find our bridge... */
	b = container_of(botstate, struct bridge_data, npcbot);
	bridge = b - bridgelist;

	/* poke the special bot to make it say something. */
	botstate->special_bot = starbase_registration_query_npc_bot;
	botstate->special_bot(&go[i], bridge, (char *) b->shipname, "");
}

static void starbase_cargo_buying_npc_bot(struct snis_entity *o, int bridge,
						char *name, char *msg);
static void starbase_cargo_selling_npc_bot(struct snis_entity *o, int bridge,
						char *name, char *msg);
static void npc_menu_item_buysell_cargo(struct npc_menu_item *item,
					char *npcname, struct npc_bot_state *botstate, int buy)
{
	struct bridge_data *b;
	int i, bridge;

	i = lookup_by_id(botstate->object_id);
	if (i < 0) {
		printf("nonfatal bug in %s at %s:%d\n", __func__, __FILE__, __LINE__);
		return;
	}

	/* find our bridge... */
	b = container_of(botstate, struct bridge_data, npcbot);
	bridge = b - bridgelist;

	/* poke the special bot to make it say something. */
	if (buy)
		botstate->special_bot = starbase_cargo_buying_npc_bot;
	else
		botstate->special_bot = starbase_cargo_selling_npc_bot;
	botstate->special_bot(&go[i], bridge, (char *) b->shipname, "");
}

static void npc_menu_item_mining_bot_transport_ores(struct npc_menu_item *item,
				char *npcname, struct npc_bot_state *botstate)
{
	int i;
	uint32_t channel = botstate->channel;
	struct snis_entity *miner, *parent;
	struct ai_mining_bot_data *ai;

	i = lookup_by_id(botstate->object_id);
	if (i < 0)
		return;
	miner = &go[i];
	ai = &miner->tsd.ship.ai[0].u.mining_bot;
	i = lookup_by_id(ai->parent_ship);
	if (i < 0) {
		/* Can't find the parent, can't transport the ores to the parent.
		 * Not sure if this can actually happen.  Maybe if all players disconnect
		 * while mining bot is mining, then reconnect? Still not sure if we could
		 * get in here though. Log it.
		 */
		fprintf(stderr, "%s: Could not find mining bot parent at %s:%d\n",
			logprefix(), __FILE__, __LINE__);
		return;
	}
	parent = &go[i];
	if (ai->mode != MINING_MODE_STANDBY_TO_TRANSPORT_ORE) {
		send_comms_packet(miner, npcname, channel,
				"UNABLE TO OBTAIN COHERENT TRANSPORTER BEAM LOCK");
	} else {
		send_comms_packet(miner, npcname, channel, "--- TRANSPORTING MATERIALS ---");
		mining_bot_unload_ores(miner, parent, ai);
		send_comms_packet(miner, npcname, channel, "--- MATERIALS TRANSPORTED ---");
		send_comms_packet(miner, npcname, channel,
			"COMMENCING RENDEZVOUS WITH TARGET");
		ai->mode = MINING_MODE_APPROACH_ASTEROID;
		schedule_callback2(event_callback, &callback_schedule,
				"mining-bot-cargo-transfer-event",
				(double) parent->id, (double) miner->id);
	}
}

static void npc_menu_item_mining_bot_stow(struct npc_menu_item *item,
				char *npcname, struct npc_bot_state *botstate)
{
	uint32_t channel = botstate->channel;
	struct ai_mining_bot_data *ai;
	struct snis_entity *miner;
	int i;

	i = lookup_by_id(botstate->object_id);
	if (i < 0)
		return;
	miner = &go[i];
	ai = &miner->tsd.ship.ai[0].u.mining_bot;
	if (strcmp(item->name, "STOW MINING BOT") == 0) {
		ai->mode = MINING_MODE_STOW_BOT;
	} else if (strcmp(item->name, "RETURN TO SHIP") == 0) {
		if (ai->mode != MINING_MODE_STOW_BOT) {
			ai->mode = MINING_MODE_RETURN_TO_PARENT;
		}
	}
	send_comms_packet(miner, npcname, channel, " RETURNING TO SHIP");
}

static void npc_menu_item_mining_bot_retarget(struct npc_menu_item *item,
				char *npcname, struct npc_bot_state *botstate)
{
	int i;
	struct bridge_data *b;
	uint32_t channel = botstate->channel;
	struct ai_mining_bot_data *ai;
	struct snis_entity *miner, *asteroid;
	float dist;

	/* find our bridge... */
	b = container_of(botstate, struct bridge_data, npcbot);

	i = lookup_by_id(botstate->object_id);
	if (i < 0)
		return;
	miner = &go[i];
	ai = &miner->tsd.ship.ai[0].u.mining_bot;

	if (b->science_selection == (uint32_t) -1 && b->selected_waypoint == -1) {
		send_comms_packet(miner, npcname, channel, " NO DESTINATION TARGETED");
		return;
	}
	if (b->science_selection != (uint32_t) -1) {
		i = lookup_by_id(b->science_selection);
		if (i < 0) {
			send_comms_packet(miner, npcname, channel, " NO DESTINATION TARGETED");
			return;
		}
		asteroid = &go[i];
		if (asteroid->type != OBJTYPE_ASTEROID && asteroid->type != OBJTYPE_DERELICT) {
			send_comms_packet(miner, npcname, channel, " SELECTED DESTINATION INAPPROPRIATE");
			return;
		}
		ai->asteroid = b->science_selection;
		dist = object_dist(asteroid, miner);
		send_comms_packet(miner, npcname, channel, " RETARGETED TO %s, DISTANCE: %f",
				asteroid ? asteroid->sdata.name : "UNKNOWN", dist);
		schedule_callback6(event_callback, &callback_schedule,
				"mining-bot-retargeted-event", (double) b->shipid, (double) miner->id,
					asteroid->id, asteroid->x, asteroid->y, asteroid->z);
	} else {
		if (b->selected_waypoint < 0 || b->selected_waypoint >= b->nwaypoints) {
			send_comms_packet(miner, npcname, channel,
				"INAPPROPRIATE WAYPOINT %d SELECTED", b->selected_waypoint);
		} else {
			struct player_waypoint *wp = &b->waypoint[b->selected_waypoint];
			ai->asteroid = (uint32_t) -1;
			ai->wpx = wp->x;
			ai->wpy = wp->y;
			ai->wpz = wp->z;
			dist = dist3d(ai->wpx - miner->x, ai->wpy - miner->y, ai->wpz - miner->z);
			send_comms_packet(miner, npcname, channel,
				" RETARGETED TO SELECTED_WAYPOINT, DISTANCE: %f", dist);
			schedule_callback6(event_callback, &callback_schedule,
					"mining-bot-retargeted-event", (double) b->shipid, (double) miner->id,
						-1.0, wp->x, wp->y, wp->z);
		}
	}
	ai->mode = MINING_MODE_APPROACH_ASTEROID;
}

static void npc_menu_item_mining_bot_cam(struct npc_menu_item *item,
				char *npcname, struct npc_bot_state *botstate)
{
	int i;
	uint32_t channel = botstate->channel;
	struct ai_mining_bot_data *ai;
	struct snis_entity *miner, *parent;

	i = lookup_by_id(botstate->object_id);
	if (i < 0) {
		send_comms_packet(NULL, npcname, channel, "MINING BOT CAMERA FEED MALFUNCTION.");
		return;
	}
	miner = &go[i];
	ai = &miner->tsd.ship.ai[0].u.mining_bot;
	i = lookup_by_id(ai->parent_ship);
	if (i < 0) {
		send_comms_packet(miner, npcname, channel, "MINING BOT CAMERA FEED MALFUNCTION.");
		return;
	}
	parent = &go[i];
	if (parent->tsd.ship.viewpoint_object != parent->id) {
		parent->tsd.ship.viewpoint_object = parent->id;
		send_comms_packet(miner, npcname, channel, "MINING BOT CAMERA FEED TERMINATED.");
	} else {
		parent->tsd.ship.viewpoint_object = miner->id;
		send_comms_packet(miner, npcname, channel, "MINING BOT CAMERA FEED INITIATED.");
	}
}

static void npc_menu_item_mining_bot_status_report(struct npc_menu_item *item,
				char *npcname, struct npc_bot_state *botstate)
{
	int i;
	uint32_t channel = botstate->channel;
	float gold, platinum, germanium, uranium, total, fuel, oxygen;
	struct snis_entity *miner, *asteroid, *parent;
	struct ai_mining_bot_data *ai;
	float dist, dist_to_parent;

	i = lookup_by_id(botstate->object_id);
	if (i < 0)
		return;
	miner = &go[i];
	ai = &miner->tsd.ship.ai[0].u.mining_bot;
	if (ai->object_or_waypoint < 128) {
		i = lookup_by_id(ai->asteroid);
		if (i >= 0) {
			asteroid = &go[i];
			dist = object_dist(asteroid, miner);
		} else {
			asteroid = NULL;
			dist = -1.0;
		}
	} else {
		asteroid = NULL;
		dist = dist3d(ai->wpx - miner->x, ai->wpy - miner->y, ai->wpz - miner->z);
	}

	i = lookup_by_id(ai->parent_ship);
	if (i < 0) {
		parent = NULL;
		dist_to_parent = -1;
	} else {
		parent = &go[i];
		dist_to_parent =
			dist3d(parent->x - miner->x, parent->y - miner->y, parent->z - miner->z);
	}

	gold = ai->gold;
	platinum = ai->platinum;
	germanium = ai->germanium;
	uranium = ai->uranium;
	total = gold + platinum + germanium + uranium;
	fuel = ai->fuel;
	oxygen = ai->oxygen;
	send_comms_packet(miner, npcname, channel, "--- BEGIN STATUS REPORT ---");
	switch (ai->mode) {
	case MINING_MODE_APPROACH_ASTEROID:
		if (ai->object_or_waypoint < 128) /* object */
			send_comms_packet(miner, npcname, channel, "RENDEZVOUS WITH %s, DISTANCE: %f\n",
				asteroid ? asteroid->sdata.name : "UNKNOWN", dist);
		else
			send_comms_packet(miner, npcname, channel,
				"RENDEZVOUS WITH WAYPOINT, DISTANCE: %f\n", dist);
		break;
	case MINING_MODE_LAND_ON_ASTEROID:
		send_comms_packet(miner, npcname, channel, "DESCENT ONTO %s, ALTITUDE: %f\n",
			asteroid ? asteroid->sdata.name : "UNKNOWN", dist * 0.3);
		break;
	case MINING_MODE_MINE:
		if (!asteroid) {
			send_comms_packet(miner, npcname, channel, "LOST CONTACT WITH TARGET");
			break;
		}
		if (asteroid->type == OBJTYPE_ASTEROID)
			send_comms_packet(miner, npcname, channel, "MINING ON %s\n",
				asteroid ? asteroid->sdata.name : "UNKNOWN");
		else
			send_comms_packet(miner, npcname, channel, "SALVAGING %s\n",
				asteroid ? asteroid->sdata.name : "UNKNOWN");
		break;
	case MINING_MODE_RETURN_TO_PARENT:
	case MINING_MODE_STOW_BOT:
		send_comms_packet(miner, npcname, channel, "RETURNING FROM %s\n",
			asteroid ? asteroid->sdata.name : "UNKNOWN");
		send_comms_packet(miner, npcname, channel, "DISTANCE TO %s: %f\n",
			asteroid ? asteroid->sdata.name : "UNKNOWN", dist);
		break;
	case MINING_MODE_STANDBY_TO_TRANSPORT_ORE:
		send_comms_packet(miner, npcname, channel, "STANDING BY TO TRANSPORT MATERIALS\n");
		send_comms_packet(miner, npcname, channel, "DISTANCE TO %s: %f\n",
			asteroid ? asteroid->sdata.name : "UNKNOWN", dist);
		break;
	case MINING_MODE_IDLE:
		send_comms_packet(miner, npcname, channel, "STANDING BY FOR ORDERS\n");
		send_comms_packet(miner, npcname, channel, "DISTANCE TO DESTINATION: %f\n", dist);
		break;
	default:
		break;
	}
	send_comms_packet(miner, npcname, channel, "DISTANCE TO %s: %f\n",
		parent ? parent->sdata.name : "MOTHER SHIP", dist_to_parent);
	if (ai->object_or_waypoint < 128) {
		switch (ai->object_or_waypoint) {
		case OBJTYPE_ASTEROID:
			send_comms_packet(miner, npcname, channel,
				"ORE COLLECTED: %f TONS\n", 2.0 * total / (255.0 * 4.0));
			send_comms_packet(miner, npcname, channel, "ORE COMPOSITION:");
			send_comms_packet(miner, npcname, channel, "GOLD: %2f%%\n", gold / total);
			send_comms_packet(miner, npcname, channel, "PLATINUM: %2f%%\n", platinum / total);
			send_comms_packet(miner, npcname, channel, "GERMANIUM: %2f%%\n", germanium / total);
			send_comms_packet(miner, npcname, channel, "URANIUM: %2f%%\n", uranium / total);
			break;
		case OBJTYPE_DERELICT:
			send_comms_packet(miner, npcname, channel, "FUEL AND OXYGEN COLLECTED:\n");
			send_comms_packet(miner, npcname, channel, "- FUEL: %2f%%\n", fuel / 255.0);
			send_comms_packet(miner, npcname, channel, "- OXYGEN: %2f%%\n", oxygen / 255.0);
			/* FIXME: This ai->mode test is a little imprecise about whether we've recovered
			 * the logs. But, maybe it's good enough.
			 */
			if (ai->mode == MINING_MODE_RETURN_TO_PARENT ||
				ai->mode == MINING_MODE_STOW_BOT ||
				ai->mode == MINING_MODE_STANDBY_TO_TRANSPORT_ORE) {
				/* This is a bug, because if we recovered the ship's log, we should not require
				 * the ship to exist. The bug is, we left the ships log on the ship
				 * instead of making a copy of it. */
				if (asteroid && asteroid->tsd.derelict.ships_log) {
					int i, n;
					char m[50];

					send_comms_packet(miner, npcname, channel,
							"*** RECOVERED PARTIAL SHIPS LOG FROM %s ***\n",
							asteroid->sdata.name);
					n = strlen(asteroid->tsd.derelict.ships_log);
					i = 0;
					do {
						int len;
						memset(m, 0, sizeof(m));
						strncpy(m, asteroid->tsd.derelict.ships_log + i, sizeof(m) - 1);
						send_comms_packet(miner, npcname, channel, "... %s\n", m);
						len = strlen(m);
						n = n - len;
						i = i + len;
						printf("m = '%s', n = %d, i = %d\n", m, n, i);
					} while (n > 0);
					send_comms_packet(miner, npcname, channel,
						"*** END OF PARTIAL SHIP'S LOG FROM %s ***\n", asteroid->sdata.name);
					schedule_callback2(event_callback, &callback_schedule,
								"ships-logs-recovered-event", (double) asteroid->id,
								parent ? (double) parent->id : -1.0);
				} else {
					if (asteroid)
						send_comms_packet(miner, npcname, channel,
							"UNABLE TO RECOVER SHIPS LOG FROM %s\n", asteroid->sdata.name);
					else
						send_comms_packet(miner, npcname, channel,
							"UNABLE TO RECOVER SHIPS LOG\n");
				}
				/* Hack, we overload miner's tractor_beam to hold ship id of salvaged derelict */
				if (miner->tsd.ship.tractor_beam != 0xffffffff)
					send_comms_packet(miner, npcname, channel,
						"*** SHIPS IDENTIFICATION CHIP RECOVERED ***");
			}
			break;
		default:
			break;
		}
	}
	send_comms_packet(miner, npcname, channel, "--- END STATUS REPORT ---");
}

static void npc_menu_item_buy_cargo(struct npc_menu_item *item,
					char *npcname, struct npc_bot_state *botstate)
{
	npc_menu_item_buysell_cargo(item, npcname, botstate, 1);
}

static void npc_menu_item_sell_cargo(struct npc_menu_item *item,
					char *npcname, struct npc_bot_state *botstate)
{
	npc_menu_item_buysell_cargo(item, npcname, botstate, 0);
}

static void starbase_passenger_boarding_npc_bot(struct snis_entity *sb, int bridge,
						char *name, char *command)
{
	struct npc_bot_state *botstate = &bridgelist[bridge].npcbot;
	uint32_t ch = botstate->channel;
	char *npcname = sb->sdata.name;
	int player_is_docked, count, i, rc, selection;
	int already_aboard = 0;
	struct snis_entity *ship;

	i = lookup_by_id(bridgelist[bridge].shipid);
	if (i < 0) {
		fprintf(stderr, "BUG: can't find ship id at %s:%d\n", __FILE__, __LINE__);
		return;
	}
	ship = &go[i];

	/* Count passengers already aboard */
	for (i = 0; i < npassengers; i++)
		if (passenger[i].location == ship->id)
			already_aboard++;
	ship->tsd.ship.lifeform_count = 5 + already_aboard;

	if (strcmp(command, "") == 0 || strcmp(command, "?") == 0) {
		send_comms_packet(sb, npcname, ch, " TRAVELERS SEEKING PASSAGE\n");

		count = 0;
		for (i = 0; i < npassengers; i++) {
			if (passenger[i].location == sb->id) {
				int d = lookup_by_id(passenger[i].destination);
				char *dest = d < 0 ? "unknown" : go[d].sdata.name;
				count++;
				send_comms_packet(sb, npcname, ch, "  %2d: DEST: %12s  FARE: $%5d  NAME: %s\n",
					count, dest, passenger[i].fare, passenger[i].name);
			}
		}
		if (count == 0)
			send_comms_packet(sb, npcname, ch, " NO PASSENGERS AVAILABLE");
		send_comms_packet(sb, npcname, ch, "\n");
		send_comms_packet(sb, npcname, ch, " 0. PREVIOUS MENU\n");
		return;
	}

	/* Check if player is currently docked here */
	player_is_docked = ship_is_docked(bridgelist[bridge].shipid, sb);
	rc = sscanf(command, "%d", &selection);
	if (rc != 1) {
		send_comms_packet(sb, npcname, ch, " HUH? TRY AGAIN\n");
		return;
	}
	if (selection == 0) {
		botstate->special_bot = NULL;
		send_to_npcbot(bridge, name, ""); /* poke generic bot so he says something */
		return;
	}
	if (selection < 1) {
		send_comms_packet(sb, npcname, ch, " BAD SELECTION, TRY AGAIN\n");
		return;
	}

	if (!player_is_docked) {
		send_comms_packet(sb, npcname, ch, " YOU MUST BE DOCKED TO BOARD PASSENGERS\n");
		return;
	}
	if (already_aboard >= ship->tsd.ship.passenger_berths) {
		send_comms_packet(sb, npcname, ch, " ALL OF YOUR PASSENGER BERTHS ARE BOOKED\n");
		return;
	}
	count = 0;
	if (already_aboard < ship->tsd.ship.passenger_berths) {
		for (i = 0; i < npassengers; i++) {
			if (passenger[i].location == sb->id) {
				count++;
				if (count == selection) {
					send_comms_packet(sb, npcname, ch,
						" BOARDING PASSENGER %s\n", passenger[i].name);
					passenger[i].location = bridgelist[bridge].shipid;
					ship->tsd.ship.lifeform_count++;
					break;
				}
			}
		}
	}
	if (count != selection) {
		send_comms_packet(sb, npcname, ch, " UNKNOWN SELECTION, TRY AGAIN\n");
		return;
	}
}

static void npc_menu_item_eject_passengers(struct npc_menu_item *item,
					char *npcname, struct npc_bot_state *botstate)
{
	struct snis_entity *sb, *ship;
	int ch = botstate->channel;
	struct bridge_data *b = container_of(botstate, struct bridge_data, npcbot);
	int nstarbases = count_starbases();
	int i = lookup_by_id(botstate->object_id);
	if (i < 0) {
		printf("nonfatal bug in %s at %s:%d\n", __func__, __FILE__, __LINE__);
		return;
	}
	sb = &go[i];
	i = lookup_by_id(b->shipid);
	if (i < 0) {
		printf("nonfatal bug in %s at %s:%d\n", __func__, __FILE__, __LINE__);
		return;
	}
	ship = &go[i];

	if (!ship_is_docked(b->shipid, sb)) {
		send_comms_packet(sb, npcname, ch, " YOU MUST BE DOCKED TO DISEMBARK PASSENGERS\n");
		return;
	}
	for (i = 0; i < npassengers; i++) {
		if (passenger[i].location == b->shipid && passenger[i].destination == sb->id) {
			send_comms_packet(sb, npcname, ch, "  PASSENGER %s DISEMBARKED\n",
					passenger[i].name);
			send_comms_packet(sb, npcname, ch, "  YOU COLLECT $%d FARE FOR SAFE PASSAGE OF %s\n",
						passenger[i].fare, passenger[i].name);
			ship->tsd.ship.wallet += passenger[i].fare;
			/* passenger disembarks, ceases to be a passenger, replace with new one */
			update_passenger(i, nstarbases);
			schedule_callback2(event_callback, &callback_schedule,
						"passenger-disembarked", (double) i, (double) sb->id);
			continue;
		}
		if (passenger[i].location == b->shipid) {
			send_comms_packet(sb, npcname, ch, "  PASSENGER %s EJECTED\n",
					passenger[i].name);
			/* Player is fined for ejecting passengers */
			send_comms_packet(sb, npcname, ch, "  YOU ARE FINED $%d FOR EJECTING %s\n",
						passenger[i].fare, passenger[i].name);
			ship->tsd.ship.wallet -= passenger[i].fare;
			ship->tsd.ship.lifeform_count--;
			/* passenger ejected, ceases to be a passenger, replace with new one */
			update_passenger(i, nstarbases);
			schedule_callback2(event_callback, &callback_schedule,
						"passenger-ejected", (double) i, (double) sb->id);
		}
	}
}

static void npc_menu_item_disembark_passengers(struct npc_menu_item *item,
					char *npcname, struct npc_bot_state *botstate)
{
	struct snis_entity *sb, *ship;
	int ch = botstate->channel;
	struct bridge_data *b = container_of(botstate, struct bridge_data, npcbot);
	int nstarbases = count_starbases();
	int i = lookup_by_id(botstate->object_id);
	if (i < 0) {
		printf("nonfatal bug in %s at %s:%d\n", __func__, __FILE__, __LINE__);
		return;
	}
	sb = &go[i];
	i = lookup_by_id(b->shipid);
	if (i < 0) {
		printf("nonfatal bug in %s at %s:%d\n", __func__, __FILE__, __LINE__);
		return;
	}
	ship = &go[i];

	if (!ship_is_docked(b->shipid, sb)) {
		send_comms_packet(sb, npcname, ch, " YOU MUST BE DOCKED TO DISEMBARK PASSENGERS\n");
		return;
	}
	for (i = 0; i < npassengers; i++) {
		if (passenger[i].location == b->shipid && passenger[i].destination == sb->id) {
			send_comms_packet(sb, npcname, ch, "  PASSENGER %s DISEMBARKED\n",
					passenger[i].name);
			send_comms_packet(sb, npcname, ch, "  YOU COLLECT $%d FARE FOR SAFE PASSAGE OF %s\n",
						passenger[i].fare, passenger[i].name);
			ship->tsd.ship.wallet += passenger[i].fare;
			ship->tsd.ship.lifeform_count--;
			/* passenger disembarks, ceases to be a passenger, replace with new one */
			update_passenger(i, nstarbases);
			schedule_callback2(event_callback, &callback_schedule,
						"passenger-disembarked", (double) i, (double) sb->id);
		}
	}
}

static void npc_menu_item_board_passengers(struct npc_menu_item *item,
					char *npcname, struct npc_bot_state *botstate)
{
	struct snis_entity *sb;
	struct bridge_data *b = container_of(botstate, struct bridge_data, npcbot);
	int bridge = b - bridgelist;
	int i = lookup_by_id(botstate->object_id);
	if (i < 0) {
		printf("nonfatal bug in %s at %s:%d\n", __func__, __FILE__, __LINE__);
		return;
	}
	sb = &go[i];
	botstate->special_bot = starbase_passenger_boarding_npc_bot;
	botstate->special_bot(sb, bridge, (char *) b->shipname, "");
}

static void npc_menu_item_collect_bounties(struct npc_menu_item *item,
					char *npcname, struct npc_bot_state *botstate)
{
	struct snis_entity *sb, *ship;
	uint32_t ch = botstate->channel;
	struct bridge_data *b = container_of(botstate, struct bridge_data, npcbot);
	int count = 0;
	int i, j, s, d;
	uint32_t ids[MAX_SHIP_ID_CHIPS];
	float total = 0.0;

	pthread_mutex_lock(&universe_mutex);
	i = lookup_by_id(botstate->object_id);
	if (i < 0) {
		pthread_mutex_unlock(&universe_mutex);
		printf("nonfatal bug in %s at %s:%d\n", __func__, __FILE__, __LINE__);
		return;
	}
	sb = &go[i];

	i = lookup_by_id(b->shipid);
	if (i < 0) {
		pthread_mutex_unlock(&universe_mutex);
		printf("nonfatal bug in %s at %s:%d\n", __func__, __FILE__, __LINE__);
		return;
	}
	ship = &go[i];

	if (!ship_is_docked(ship->id, sb)) {
		send_comms_packet(sb, npcname, ch,
				" YOU MUST BE DOCKED TO REDEEM SHIP ID CHIPS FOR BOUNTIES");
		pthread_mutex_unlock(&universe_mutex);
		return;
	}

	if (b->nship_id_chips <= 0) {
		send_comms_packet(sb, npcname, ch, " SORRY, YOU HAVE NO SHIP ID CHIPS");
		pthread_mutex_unlock(&universe_mutex);
		return;
	}

	for (i = 0; i < b->nship_id_chips; i++) {
		uint32_t id = b->ship_id_chip[i];
		for (j = 0; j >= 0; j++) {
			j = ship_registry_get_next_entry(&ship_registry, id, j);
			if (j < 0)
				break;
			if (ship_registry.entry[j].id != id)
				continue;
			if (ship_registry.entry[j].type != SHIP_REG_TYPE_BOUNTY)
				continue;
			if (ship_registry.entry[j].bounty_collection_site != sb->id)
				continue;
			ids[count] = id;
			count++;
			b->ship_id_chip[i] = 0; /* player doesn't get to keep the id chip */
			send_comms_packet(sb, npcname, ch, " BOUNTY of $%.2f AWARDED FOR SHIP ID CHIP %u",
						ship_registry.entry[j].bounty, id);
			send_comms_packet(sb, npcname, ch, " FOR %s",
						ship_registry.entry[j].entry);
			schedule_callback4(event_callback, &callback_schedule,
					"player-collected-bounty", (double) ship->id, (double) id,
					ship_registry.entry[j].bounty, (double) sb->id);
			total += ship_registry.entry[j].bounty;
			break;
		}
	}
	if (count == 0) {
		send_comms_packet(sb, npcname, ch, " SORRY, YOU HAVE NO REDEEMABLE SHIP ID CHIPS");
		pthread_mutex_unlock(&universe_mutex);
		return;
	}
	for (i = 0; i < count; i++)
		ship_registry_delete_ship_entries(&ship_registry, ids[i]);
	send_comms_packet(sb, npcname, ch, " TOTAL BOUNTY AWARDED - $%.2f", total);

	/* Clear out ID chips for ships that have no bounties anywhere */
	for (i = 0; i < b->nship_id_chips; i++) {
		uint32_t id = b->ship_id_chip[i];
		if (id == 0)
			continue;
		for (j = 0; j >= 0; j++) {
			j = ship_registry_get_next_entry(&ship_registry, id, j);
			if (ship_registry.entry[j].id != id)
				continue;
			if (ship_registry.entry[j].type != SHIP_REG_TYPE_BOUNTY)
				continue;
			break;
		}
		if (j < 0) {
			send_comms_packet(sb, npcname, ch, " SHIP ID CHIP %u IS CLEAN WITH NO KNOWN BOUNTIES", id);
			b->ship_id_chip[i] = 0;
		}
	}
	ship->tsd.ship.wallet += total;

	/* Clean up b->ship_id_chip[i], gettting rid of zeroed entries */
	count = 0;
	s = 0;
	d = 0;
	while (s < b->nship_id_chips) {
		if (b->ship_id_chip[s] == 0) {
			s++;
			continue;
		}
		if (s != d)
			b->ship_id_chip[d] = b->ship_id_chip[s];
		count++;
		s++;
		d++;
	}
	if (count != b->nship_id_chips)
		send_comms_packet(sb, npcname, ch, " COLLECTED %d SHIP ID CHIPS", b->nship_id_chips - count);
	b->nship_id_chips = count;
	send_comms_packet(sb, npcname, ch, " THANK YOU FOR YOUR SERVICE", total);
	pthread_mutex_unlock(&universe_mutex);
}

static void npc_menu_item_travel_advisory(struct npc_menu_item *item,
					char *npcname, struct npc_bot_state *botstate)
{
	uint32_t plid, ch = botstate->channel;
	struct snis_entity *sb, *pl = NULL;
	char *name;
	int i;
	uint16_t contraband = -1;

	if (ch == (uint32_t) -1)
		return;

	i = lookup_by_id(botstate->object_id);
	if (i < 0) {
		printf("nonfatal bug in %s at %s:%d\n", __func__, __FILE__, __LINE__);
		return;
	}
	sb = &go[i];
	plid = sb->tsd.starbase.associated_planet_id;
	if (plid != (uint32_t) -1) {
		i = lookup_by_id(plid);
		if (i >= 0) {
			pl = &go[i];
			name = pl->sdata.name;
			contraband = pl->tsd.planet.contraband;
		} else {
			name = sb->sdata.name;
		}
	} else {
		name = sb->sdata.name;
	}

	if (pl) {
		/* TODO: fill in all this crap */
		send_comms_packet(sb, npcname, ch, " TRAVEL ADVISORY FOR %s", name);
		send_comms_packet(sb, npcname, ch, "-----------------------------------------------------");
		send_comms_packet(sb, npcname, ch, " WELCOME TO STARBASE %s, IN ORBIT", sb->sdata.name);
		send_comms_packet(sb, npcname, ch, " AROUND THE BEAUTIFUL PLANET %s.", name);
		send_comms_packet(sb, npcname, ch, "");
		send_comms_packet(sb, npcname, ch, " PLANETARY SURFACE TEMP: -15 - 103");
		send_comms_packet(sb, npcname, ch, " PLANETARY SURFACE WIND SPEED: 0 - 203");
	} else {
		send_comms_packet(sb, npcname, ch, " TRAVEL ADVISORY FOR STARBASE %s", name);
		send_comms_packet(sb, npcname, ch, "-----------------------------------------------------");
		send_comms_packet(sb, npcname, ch, " WELCOME TO STARBASE %s, IN DEEP SPACE", sb->sdata.name);
		send_comms_packet(sb, npcname, ch, "");
	}
	send_comms_packet(sb, npcname, ch, " SPACE WEATHER ADVISORY: ALL CLEAR");
	send_comms_packet(sb, npcname, ch, "");
	if (contraband != (uint16_t) -1) {
		send_comms_packet(sb, npcname, ch, " TRAVELERS TAKE NOTICE OF PROHIBITED ITEMS:");
		send_comms_packet(sb, npcname, ch, "    %s", commodity[contraband].name);
	}
	if (docking_by_faction && !rts_mode) {
		for (i = 0; i < nfactions(); i++) {
			if ((sb->tsd.starbase.factions_allowed & (1 << i)) == 0)
				send_comms_packet(sb, npcname, ch, " NOTICE - THE %s ARE NOT WELCOME HERE",
					faction_name(i));
		}
	}
	send_comms_packet(sb, npcname, ch, "");
	send_comms_packet(sb, npcname, ch, " ENJOY YOUR VISIT!");
	send_comms_packet(sb, npcname, ch, "-----------------------------------------------------");
}

static void npc_menu_item_request_dock(struct npc_menu_item *item,
				char *npcname, struct npc_bot_state *botstate)
{
	struct snis_entity *o;
	struct snis_entity *sb;
	double dist;
	struct bridge_data *b;
	int ch = botstate->channel;
	int i;

	b = container_of(botstate, struct bridge_data, npcbot);
	printf("npc_menu_item_request_dock called for %s\n", b->shipname);

	i = lookup_by_id(b->shipid);
	if (i < 0)
		return;
	o = &go[i];

	i = lookup_by_id(botstate->object_id);
	if (i < 0)
		return;
	sb = &go[i];
	if (docking_by_faction && !rts_mode) {
		if ((sb->tsd.starbase.factions_allowed & (1 << o->sdata.faction)) == 0) {
			send_comms_packet(sb, npcname, ch, "%s, SORRY, WE DO NOT DEAL WITH THE %s.\n", b->shipname,
						faction_name(o->sdata.faction));
			return;
		}
	}
	if (rts_mode) {
		/* Do not grant permission to dock to enemy faction in RTS mode */
		if (sb->sdata.faction != 255 && sb->sdata.faction != o->sdata.faction) {
			send_comms_packet(sb, npcname, ch, "%s, PERMISSION DENIED, ENEMY SCUM!\n", b->shipname);
			return;
		}
	}
	dist = object_dist(sb, o);
	if (dist > STARBASE_DOCKING_PERM_DIST) {
		send_comms_packet(sb, npcname, ch, "%s, YOU ARE TOO FAR AWAY (%lf).\n", b->shipname, dist);
		return;
	}
	if (o->sdata.shield_strength > 15) {
		send_comms_packet(sb, npcname, ch, "%s, YOU MUST LOWER SHIELDS FIRST.\n", b->shipname);
		return;
	}
	if (docking_by_faction && !rts_mode) {
		if ((sb->tsd.starbase.factions_allowed & (1 << o->sdata.faction)) == 0) {
			send_comms_packet(sb, npcname, ch, "%s, SORRY, WE DO NOT DEAL WITH THE %s.\n", b->shipname,
						faction_name(o->sdata.faction));
			return;
		}
	}
	starbase_grant_docker_permission(sb, o, b, npcname, ch);
}

static void warp_gate_ticket_buying_npc_bot(struct snis_entity *o, int bridge,
		char *name, char *msg)
{
	struct npc_bot_state *botstate = &bridgelist[bridge].npcbot;
	struct ssgl_game_server *gameserver = NULL;
	int selection, rc, i, nservers;
	int ch = bridgelist[bridge].npcbot.channel;
	double ssx, ssy, ssz; /* our solarsystem's position */
	double dx, dy, dz;
	int sslist[100];
	int nsslist = 0;
	struct snis_entity *sb;

	i = lookup_by_id(botstate->object_id);
	if (i < 0)
		return;
	sb = &go[i];

	if (server_tracker_get_server_list(server_tracker, &gameserver, &nservers) != 0
		|| nservers <= 1) {
		/*
		 * nservers <= 1 rather than <= 0 because if there's only 1 server,
		 * then we must already be in it, and there's no place else for us to go.
		 */
		send_comms_packet(sb, name, ch, "NO WARP-GATE TICKETS AVAILABLE\n");
		return;
	}
	send_comms_packet(sb, name, ch, "WARP-GATE TICKETS:\n");
	send_comms_packet(sb, name, ch, "------------------\n");

	/* Find our solarsystem */
	int len = strlen(solarsystem_name);
	if (len > SSGL_LOCATIONSIZE)
		len = SSGL_LOCATIONSIZE;
	ssx = 0.0;
	ssy = 0.0;
	ssz = 0.0;
	for (i = 0; i < nservers; i++) {
		if (strncasecmp(gameserver[i].location, solarsystem_name, len) == 0) {
			rc = sscanf(gameserver[i].game_instance, "%lf %lf %lf", &ssx, &ssy, &ssz);
			if (rc != 3) {
				ssx = 0.0;
				ssy = 0.0;
				ssz = 0.0;
				fprintf(stderr,
					"%s: Could not extract own solarsystem position from game_instance '%s'\n",
					logprefix(), gameserver[i].game_instance);
			}
			break;
		}
	}
	for (i = 0; i < nservers; i++) {
		/* Do not list the solarsystem we're already in */
		if (strncasecmp(gameserver[i].location, solarsystem_name, len) != 0) {
			rc = sscanf(gameserver[i].game_instance, "%lf %lf %lf", &dx, &dy, &dz);
			if (rc != 3) {
				dx = 0.0;
				dy = 0.0;
				dz = 0.0;
				fprintf(stderr,
					"%s: Could not extract solarsystem position from game_instance '%s'\n",
					logprefix(), gameserver[i].game_instance);
			}
			dx = dx - ssx;
			dy = dy - ssy;
			dz = dz - ssz;
			double dist = sqrt(dx * dx + dy * dy + dz * dz);
			if (dist < SNIS_WARP_GATE_THRESHOLD) {
				sslist[nsslist] = i;
				nsslist++;
				send_comms_packet(sb, name, ch, "%3d: %s\n", nsslist, gameserver[i].location);
			}
		}
	}
	if (nsslist == 0)
		send_comms_packet(sb, name, ch, "NO WARP-GATE TICKETS AVAILABLE\n");
	send_comms_packet(sb, name, ch, "------------------\n");
	send_comms_packet(sb, name, ch, "  0: PREVIOUS MENU\n");
	rc = sscanf(msg, "%d", &selection);
	if (rc != 1)
		selection = -1;
	if (selection == 0) {
		bridgelist[bridge].npcbot.special_bot = NULL; /* deactivate warpgate ticket bot */
		free(gameserver);
		send_to_npcbot(bridge, name, ""); /* poke generic bot so he says something */
		return;
	}
	if (selection < 1 || selection > nsslist) {
		free(gameserver);
		return;
	}
	/* Figure out what we selected */
	selection = sslist[selection - 1];
	if (selection < 0 || selection >= nservers) {
		free(gameserver);
		return;
	}
	if (bridgelist[bridge].warp_gate_ticket.ipaddr == 0) { /* no current ticket held */
		send_comms_packet(sb, name, ch, "WARP-GATE TICKET TO %s BOOKED\n",
					gameserver[selection].location);
	} else {
		send_comms_packet(sb, name, ch, "WARP-GATE TICKET TO %s EXCHANGED\n",
			bridgelist[bridge].warp_gate_ticket.location);
		send_comms_packet(sb, name, ch, "FOR WARP-GATE TICKET TO %s\n",
			gameserver[selection].location);
	}
	bridgelist[bridge].warp_gate_ticket = gameserver[selection];
	free(gameserver);
}

static void npc_menu_item_warp_gate_tickets(struct npc_menu_item *item,
				char *npcname, struct npc_bot_state *botstate)
{
	struct snis_entity *o;
	struct bridge_data *b = container_of(botstate, struct bridge_data, npcbot);
	int i, bridge = b - bridgelist;

	fprintf(stderr, "%s: npc_menu_item_warp_gate_tickets called for %s\n",
			logprefix(), b->shipname);

	i = lookup_by_id(b->shipid);
	if (i < 0)
		return;
	o = &go[i];
	botstate->special_bot = warp_gate_ticket_buying_npc_bot;
	botstate->special_bot(o, bridge, npcname, "");
}

static void push_tow_mode(struct snis_entity *tow_ship, uint32_t disabled_ship,
					uint32_t starbase_dispatcher)
{
	if (!tow_ship)
		return;
	if (tow_ship->type != OBJTYPE_SHIP2)
		return;
	if (tow_ship->tsd.ship.shiptype != SHIP_CLASS_MANTIS)
		return;

	/* Drop everything and go tow the disabled ship */
	tow_ship->tsd.ship.nai_entries = 1;
	tow_ship->tsd.ship.ai[0].ai_mode = AI_MODE_TOW_SHIP;
	tow_ship->tsd.ship.ai[0].u.tow_ship.disabled_ship = disabled_ship;
	tow_ship->tsd.ship.ai[0].u.tow_ship.starbase_dispatcher = starbase_dispatcher;
	tow_ship->tsd.ship.ai[0].u.tow_ship.ship_connected = 0;
}

static void npc_menu_item_towing_service(struct npc_menu_item *item,
				char *npcname, struct npc_bot_state *botstate)
{
	struct snis_entity *o;
	struct bridge_data *b = container_of(botstate, struct bridge_data, npcbot);
	int i, bridge = b - bridgelist;
	int closest_tow_ship;
	double closest_tow_ship_distance;
	uint32_t channel = bridgelist[bridge].npcbot.channel;
	struct snis_entity *sb;

	pthread_mutex_lock(&universe_mutex);
	i = lookup_by_id(b->shipid);
	if (i < 0)
		goto out;
	o = &go[i];

	i = lookup_by_id(botstate->object_id);
	if (i < 0)
		return;
	sb = &go[i];

	closest_tow_ship = -1;
	for (i = 0; i <= snis_object_pool_highest_object(pool); i++) {
		if (!go[i].alive)
			continue;
		if (go[i].type == OBJTYPE_SHIP2 && go[i].tsd.ship.shiptype == SHIP_CLASS_MANTIS) {
			/* Check to see if this tow ship is already en route someplace */
			int n = go[i].tsd.ship.nai_entries - 1;
			if (go[i].tsd.ship.ai[n].ai_mode == AI_MODE_TOW_SHIP) {
				/* Check to see if there's a tow ship already en route to this player */
				if (go[i].tsd.ship.ai[n].u.tow_ship.disabled_ship == b->shipid) {
					send_comms_packet(sb, npcname, channel,
						"%s, THE MANTIS TOW SHIP %s IS ALREADY EN ROUTE",
						b->shipname, go[i].sdata.name);
					goto out;
				}
			} else {
				/* Tow ship is not busy, see how far away */
				double dx, dy, dz, dist;
				dx = o->x - go[i].x;
				dy = o->y - go[i].y;
				dz = o->z - go[i].z;
				dist = sqrt(dx * dx + dy * dy + dz * dz);
				if (closest_tow_ship == -1 || dist < closest_tow_ship_distance) {
					closest_tow_ship = i;
					closest_tow_ship_distance = dist;
				}
			}
		}
	}

	if (closest_tow_ship == -1) { /* No tow ships, or all tow ships busy */
		send_comms_packet(sb, npcname, channel, "%s, SORRY, ALL MANTIS TOW SHPS ARE CURRENTLY BUSY.",
				b->shipname);
		goto out;
	}

	/* Send the tow ship to the player */
	push_tow_mode(&go[closest_tow_ship], o->id, botstate->object_id);
	send_comms_packet(sb, npcname, channel, "%s, THE MANTIS TOW SHIP %s HAS BEEN",
			b->shipname, go[closest_tow_ship].sdata.name);
	send_comms_packet(sb, npcname, channel, "DISPATCHED TO YOUR LOCATION");
	send_comms_packet(sb, npcname, channel, "%s, UPON DELIVERY YOUR ACCOUNT\n", b->shipname);
	send_comms_packet(sb, npcname, channel, "WILL BE BILLED $%5.2f\n", TOW_SHIP_CHARGE);
	send_comms_packet(sb, npcname, channel, " WARNING THE TOW SHIP NAVIGATION ALGORITHM IS BUGGY");
	send_comms_packet(sb, npcname, channel, " AND IT MAY OCCASIONALLY TRY TO DRIVE YOU INTO A PLANET");
	send_comms_packet(sb, npcname, channel, " USE DOCKING MAGNETS TO DISCONNECT IF NECESSARY");

out:
	pthread_mutex_unlock(&universe_mutex);
	return;
}

static void send_npc_menu(struct snis_entity *transmitter, char *npcname,  int bridge)
{
	int i;
	uint32_t channel = bridgelist[bridge].npcbot.channel;
	struct npc_menu_item *menu = &bridgelist[bridge].npcbot.current_menu[0];

	if (!menu || !menu->name)
		return;

	send_comms_packet(transmitter, npcname, channel, "-----------------------------------------------------");
	send_comms_packet(transmitter, npcname, channel, "     %s", menu->name);
	send_comms_packet(transmitter, npcname, channel, "");
	menu++;
	i = 1;
	while (menu->name) {
		send_comms_packet(transmitter, npcname, channel, "     %d: %s\n", i, menu->name);
		menu++;
		i++;
	};
	menu = &bridgelist[bridge].npcbot.current_menu[0];
	if (menu->parent_menu)
		send_comms_packet(transmitter, npcname, channel, "     %d: BACK TO %s\n", 0, menu->parent_menu->name);
	send_comms_packet(transmitter, npcname, channel, "");
	send_comms_packet(transmitter, npcname, channel, "    MAKE YOUR SELECTION WHEN READY.");
	send_comms_packet(transmitter, npcname, channel, "-----------------------------------------------------");
}

static void starbase_cargo_buyingselling_npc_bot(struct snis_entity *o, int bridge,
		char *name, char *msg, int buy)
{
	int i;
	char *n = o->sdata.name;
	struct snis_entity *ship;
	struct marketplace_data *mkt = o->tsd.starbase.mkt;
	uint32_t channel = bridgelist[bridge].npcbot.channel;
	int rc, selection;
	float range2, profitloss;

	i = lookup_by_id(bridgelist[bridge].shipid);
	if (i < 0) {
		printf("Non fatal error at %s:%s:%d\n",
			__FILE__, __func__, __LINE__);
		return;
	}
	ship = &go[i];

	range2 = dist3dsqrd(ship->x - o->x, ship->y - o->y, ship->z - o->z);

	if (!mkt)
		return;
	rc = sscanf(msg, "%d", &selection);
	if (rc != 1)
		selection = -1;
	if (selection < 0 || selection > 10)
		selection = -1;

	if (buy) {
		if (strncasecmp("buy ", msg, 4) == 0)  {
			int empty_bay = -1;
			float ask;
			int q;
			char x;

			/* check transporter range */
			if (range2 > TRANSPORTER_RANGE * TRANSPORTER_RANGE) {
				send_comms_packet(o, n, channel,
					" TRANSACTION NOT POSSIBLE - TRANSPORTER RANGE EXCEEDED");
				return;
			}
			/* check that there is an empty cargo bay.
			 * FIXME: if you have a cargo bay with 5 Zarkon swords
			 * and you try to buy 1 more Zarkon sword, you should just
			 * be able to add another Zarkon sword into that bay, but,
			 * you can't.
			 */
			for (i = 0; i < ship->tsd.ship.ncargo_bays; i++) {
				if (ship->tsd.ship.cargo[i].contents.item == -1) {
					empty_bay = i;
					break;
				}
			}
			if (empty_bay == -1) {
				send_comms_packet(o, n, channel, " ALL YOUR CARGO BAYS ARE FULL");
				return;
			}

			/* decode the buy order */
			rc = sscanf(msg + 4, "%d %c", &q, &x);
			if (rc != 2) {
				send_comms_packet(o, n, channel, " INVALID BUY ORDER");
				return;
			}

			/* check buy order is in range */
			x = toupper(x);
			if (x < 'A' || x >= 'A' + COMMODITIES_PER_BASE) {
				send_comms_packet(o, n, channel, " INVALID BUY ORDER");
				return;
			}

			/* check qty */
			if (q <= 0 || q > mkt[x - 'A'].qty) {
				send_comms_packet(o, n, channel, " INVALID BUY ORDER");
				return;
			}

			ask = mkt[x - 'A'].ask;
			/* check fundage */
			if (q * ask > ship->tsd.ship.wallet) {
				send_comms_packet(o, n, channel, " INSUFFICIENT FUNDS");
				return;
			}
			ship->tsd.ship.wallet -= q * ask;
			ship->tsd.ship.cargo[empty_bay].paid = ask;
			ship->tsd.ship.cargo[empty_bay].contents.item = mkt[x - 'A'].item;
			ship->tsd.ship.cargo[empty_bay].contents.qty = q;
			ship->tsd.ship.cargo[empty_bay].origin = o->tsd.starbase.associated_planet_id;
			ship->tsd.ship.cargo[empty_bay].dest = -1;
			ship->tsd.ship.cargo[empty_bay].due_date = -1;
			mkt[x - 'A'].qty -= q;
			send_comms_packet(o, n, channel, " EXECUTING BUY ORDER %d %c", q, x);
			snis_queue_add_sound(TRANSPORTER_SOUND,
					ROLE_SOUNDSERVER, ship->id);
			send_comms_packet(o, n, channel,
				"TRANSPORTER BEAM ACTIVATED - GOODS TRANSFERRED");
			return;
		}
		if (strncasecmp("sell ", msg, 5) == 0)  {
			send_comms_packet(o, n, channel, " INVALID SELL ORDER (USE THE 'SELL' MENU)");
			return;
		}
	} else {
		if (strncasecmp("buy ", msg, 4) == 0)  {
			send_comms_packet(o, n, channel, " INVALID BUY ORDER (USE THE 'BUY' MENU)");
			return;
		}
		if (strncasecmp("sell ", msg, 5) == 0)  {
			float qf;
			char x;

			/* check transporter range */
			if (range2 > TRANSPORTER_RANGE * TRANSPORTER_RANGE) {
				send_comms_packet(o, n, channel,
					" TRANSACTION NOT POSSIBLE - TRANSPORTER RANGE EXCEEDED");
				return;
			}

			rc = sscanf(msg + 5, "%f %c", &qf, &x);
			if (rc != 2) {
				rc = sscanf(msg + 5, "%c", &x);
				if (rc == 1 && strlen(msg) == 6) {
					if (toupper(x) - 'A' < 0 || toupper(x) - 'A' >= ship->tsd.ship.ncargo_bays) {
						send_comms_packet(o, n, channel, " INVALID SELL ORDER, BAD CARGO BAY");
						return;
					}
					qf = ship->tsd.ship.cargo[(int) toupper(x) - 'A'].contents.qty;
				} else {
					send_comms_packet(o, n, channel, " INVALID SELL ORDER, PARSE ERROR");
					return;
				}
			}
			if (qf < 0.0) {
				send_comms_packet(o, n, channel, " OH A WISE GUY, EH?");
				return;
			}
			x = toupper(x) - 'A';
			if (x < 0 || x >= ship->tsd.ship.ncargo_bays) {
				send_comms_packet(o, n, channel, " INVALID SELL ORDER, BAD CARGO BAY");
				return;
			}
			if (qf > ship->tsd.ship.cargo[(int) x].contents.qty) {
				send_comms_packet(o, n, channel, " INVALID SELL ORDER, INSUFFICIENT QUANTITY");
				return;
			}
			ship->tsd.ship.cargo[(int) x].contents.qty -= qf;
			ship->tsd.ship.wallet +=
				qf * o->tsd.starbase.bid_price[ship->tsd.ship.cargo[(int) x].contents.item];
			profitloss = qf * o->tsd.starbase.bid_price[ship->tsd.ship.cargo[(int) x].contents.item]
					- qf * ship->tsd.ship.cargo[(int) x].paid;
			if (ship->tsd.ship.cargo[(int) x].contents.qty < 0.001) {
				ship->tsd.ship.cargo[(int) x].contents.item = -1;
				ship->tsd.ship.cargo[(int) x].contents.qty = 0.0f;
				ship->tsd.ship.cargo[(int) x].paid = 0.0f;
				ship->tsd.ship.cargo[(int) x].origin = -1;
				ship->tsd.ship.cargo[(int) x].dest = -1;
				ship->tsd.ship.cargo[(int) x].due_date = -1;
			}
			send_comms_packet(o, n, channel, " EXECUTING SELL ORDER %.2f %c", qf, x);
			snis_queue_add_sound(TRANSPORTER_SOUND,
					ROLE_SOUNDSERVER, ship->id);
			send_comms_packet(o, n, channel,
				"TRANSPORTER BEAM ACTIVATED - GOODS TRANSFERRED");
			send_comms_packet(o, n, channel, " SOLD FOR A %s OF $%.2f",
				profitloss < 0 ? "LOSS" : "PROFIT", profitloss);
			return;
		}
	}

	if (selection == -1) {
		send_comms_packet(o, n, channel, "----------------------------");
		if (buy) {
			send_comms_packet(o, n, channel,
				"   QTY  UNIT ITEM   BID/UNIT     ASK/UNIT    ITEM");
			for (i = 0; i < COMMODITIES_PER_BASE; i++) {
				float bid, ask, qty;
				char *itemname = commodity[mkt[i].item].name;
				char *unit = commodity[mkt[i].item].unit;
				bid = mkt[i].bid;
				ask = mkt[i].ask;
				qty = mkt[i].qty;
				send_comms_packet(o, n, channel, " %c: %4.2f %s %s -- $%4.2f  $%4.2f\n",
					i + 'A', qty, unit, itemname, bid, ask);
			}
		} else {
			send_comms_packet(o, n, channel,
				"   QTY  UNIT ITEM   BID/UNIT     ITEM");
			int count = 0;
			for (i = 0; i < ship->tsd.ship.ncargo_bays; i++) {
				float bid, qty;
				char *itemname, *unit;
				int item = ship->tsd.ship.cargo[i].contents.item;

				if (item < 0)
					continue;
				itemname = commodity[item].name;
				unit = commodity[item].unit;
				qty = ship->tsd.ship.cargo[i].contents.qty;
				bid = o->tsd.starbase.bid_price[item];
				send_comms_packet(o, n, channel, " %c: %4.2f %s %s -- $%4.2f (PAID $%.2f)",
					i + 'A', qty, unit, itemname, bid,
					ship->tsd.ship.cargo[i].paid);
				count++;
			}
			if (count == 0)
				send_comms_packet(o, n, channel, "   ** CARGO HOLD IS EMPTY **");
		}
		send_comms_packet(o, n, channel, " 0: PREVIOUS MENU");
		send_comms_packet(o, n, channel, "----------------------------");
		if (buy)
			send_comms_packet(o, n, channel, "  (TYPE 'BUY 3 A' TO BUY 3 of item A)");
		else
			send_comms_packet(o, n, channel,
				"  (TYPE 'SELL 3 A' TO SELL 3 of item A or 'SELL A' to sell all A)");
		send_comms_packet(o, n, channel, "----------------------------");
	}
	if (selection == 0) {
		bridgelist[bridge].npcbot.special_bot = NULL; /* deactivate cargo buying bot */
		send_to_npcbot(bridge, name, ""); /* poke generic bot so he says something */
	}
}

static void starbase_cargo_buying_npc_bot(struct snis_entity *o, int bridge,
						char *name, char *msg)
{
	starbase_cargo_buyingselling_npc_bot(o, bridge, name, msg, 1);
}

static void starbase_cargo_selling_npc_bot(struct snis_entity *o, int bridge,
						char *name, char *msg)
{
	starbase_cargo_buyingselling_npc_bot(o, bridge, name, msg, 0);
}

static void generic_npc_bot(struct snis_entity *o, int bridge, char *name, char *msg)
{
	char *n;
	uint32_t channel = bridgelist[bridge].comms_channel;
	int selection, menu_count, i, rc;
	struct npc_menu_item *menu = bridgelist[bridge].npcbot.current_menu;

	n = o->sdata.name;
	/* count current menu items */
	menu_count = -1;
	if (menu) {
		menu_count = 0;
		for (i = 0; menu[i].name; i++)
			menu_count++;
	}

	/* See if they have selected some menu item. */
	rc = sscanf(msg, "%d", &selection);
	if (rc != 1)
		selection = -1;
	if (rc == 1 && (!menu || selection < 0 || selection > menu_count))
		selection = -1;

	if (selection == 0 && menu->parent_menu) {
		bridgelist[bridge].npcbot.current_menu = menu->parent_menu;
		selection = -1;
	}

	if (selection == -1) {
		/* TODO: make this welcome message customizable per npc bot */
		send_comms_packet(o, n, channel, "");
		send_comms_packet(o, n, channel,
			"  WELCOME %s, I AM A MODEL ZX81 SERVICE", name);
		send_comms_packet(o, n, channel,
			"  ROBOT PROGRAMMED TO BE AT YOUR SERVICE");
		send_comms_packet(o, n, channel,
			"  THE FOLLOWING SERVICES ARE AVAILABLE:");
		send_comms_packet(o, n, channel, "");
		send_npc_menu(o, n, bridge);
	} else {
		if (menu[selection].submenu) {
			menu[selection].submenu->parent_menu = menu;
			bridgelist[bridge].npcbot.current_menu = menu[selection].submenu;
			send_comms_packet(o, n, channel, "");
			send_npc_menu(o, n, bridge);
		} else {
			if (menu[selection].f)
				menu[selection].f(&menu[selection], n, &bridgelist[bridge].npcbot);
			else
				printf("Non fatal bug at %s:%d\n", __FILE__, __LINE__);
		}
	}
}

static struct npc_menu_item *topmost_menu(struct npc_menu_item *m)
{
	if (!m)
		return m;
	if (!m->parent_menu)
		return m;
	return topmost_menu(m->parent_menu);
}

static void send_to_npcbot(int bridge, char *name, char *msg)
{
	int i;
	struct snis_entity *o;
	uint32_t id;

	printf("npcbot for bridge %d received msg: '%s' from %s\n",
		bridge, msg, name);
	/* TODO: something better than this crap. */

	id = bridgelist[bridge].npcbot.object_id;
	if (id == (uint32_t) -1)
		return;

	if (bridgelist[bridge].npcbot.channel == (uint32_t) -1)
		return;

	i = lookup_by_id(id);
	if (i < 0)
		return;

	o = &go[i];

	/* TODO: check if transmission is successful */

	/* if a special bot is active, use that instead. */
	if (bridgelist[bridge].npcbot.special_bot) {
		bridgelist[bridge].npcbot.special_bot(o, bridge, name, msg);
		return;
	}

	switch (o->type) {
	case OBJTYPE_STARBASE:
		if (topmost_menu(bridgelist[bridge].npcbot.current_menu) != starbase_main_menu)
			bridgelist[bridge].npcbot.current_menu = starbase_main_menu;
		generic_npc_bot(o, bridge, name, msg);
		break;
	case OBJTYPE_SHIP2:
		if (o->tsd.ship.ai[0].ai_mode != AI_MODE_MINING_BOT)
			break;
		if (topmost_menu(bridgelist[bridge].npcbot.current_menu) != mining_bot_main_menu)
			bridgelist[bridge].npcbot.current_menu = mining_bot_main_menu;
		generic_npc_bot(o, bridge, name, msg);
		break;
	default:
		break;
	}
	return;
}

static void perform_natural_language_request(struct game_client *c, char *txt);
static void meta_comms_computer(char *name, struct game_client *c, char *txt)
{
	char *duptxt;
	char *x;

	printf("meta_comms_computer, txt = '%s'\n", txt);
	duptxt = strdup(txt);

	x = index(duptxt, ' ');
	if (x) {
		x++;
		if (*x != '\0')
			perform_natural_language_request(c, x);
	}
	free(duptxt);
}

static void meta_comms_antenna(char *name, struct game_client *c, char *txt)
{
	char *duptxt;
	float bearing, mark;
	int rc;

	duptxt = strndup(txt, 256);
	if (!duptxt)
		return;
	printf("Antenna text = '%s'\n", duptxt);
	rc = sscanf(duptxt, "%*s %f %f", &bearing, &mark);
	free(duptxt);
	printf("antenna rc = %d\n", rc);
	if (rc == 2) {
		union vec3 dir;
		printf("bearing %f, mark %f\n", bearing, mark);
		bearing *= M_PI / 180.0;
		mark *= M_PI / 180.0;
		bearing = math_angle_to_game_angle(bearing);
		heading_mark_to_vec3(1.0, bearing, mark, &dir);
		go[c->ship_index].tsd.ship.desired_hg_ant_aim = dir;
	}
}

static void meta_comms_hail(char *name, struct game_client *c, char *txt)
{
#define MAX_SHIPS_HAILABLE 10
	int i, j, k, nnames;
	char *namelist[MAX_SHIPS_HAILABLE], msg[100];
	char *duptxt;
	int nchannels;
	uint32_t channel[MAX_SHIPS_HAILABLE];
	char *x, *saveptr;
	int switch_channel = 0;
	uint32_t id = (uint32_t) -1;

	duptxt = strdup(txt);

	x = strtok_r(duptxt, " ,", &saveptr);
	i = 0;
	while (x && i < ARRAYSIZE(namelist)) {
		x = strtok_r(NULL, " ,", &saveptr);
		if (x)
			namelist[i++] = x;
	}
	nnames = i;

	nchannels = 0;
	pthread_mutex_lock(&universe_mutex);
	for (i = 0; i < nnames; i++) {
		for (j = 0; j < nbridges; j++) {
			char *shipname = (char *) bridgelist[j].shipname;
			if (strcasecmp(shipname, (const char *) namelist[i]) == 0) {
				int found = 0;

				for (k = 0; k < nchannels; k++)
					if (bridgelist[j].comms_channel == channel[k]) {
						found = 1;
						break;
					}
				if (!found) {
					channel[nchannels] = bridgelist[j].comms_channel;
					nchannels++;
					if (nchannels >= MAX_SHIPS_HAILABLE)
						goto channels_maxxed;
				}
			}
		}
	}

	/* check for lua channels being hailed */
	for (i = 0; i < nnames; i++) {
		for (j = 0; j < nlua_channels; j++) {
			if (!lua_channel[j].name)
				continue;
			if (strcasecmp(lua_channel[j].name, namelist[i]) != 0)
				continue;
			/* What to do here? */
		}
	}

	/* check for starbases and NPC ships (mining bots) being hailed */
	for (i = 0; i <= snis_object_pool_highest_object(pool); i++) {
		struct snis_entity *o = &go[i];
		char *sbname = NULL;

		if (o->type != OBJTYPE_STARBASE && o->type != OBJTYPE_SHIP2)
			continue;
		if (o->type == OBJTYPE_SHIP2 && o->tsd.ship.ai[0].ai_mode != AI_MODE_MINING_BOT)
			continue;
		sbname = o->sdata.name;
		for (j = 0; j < nnames; j++) {
			if (strcasecmp(namelist[j], "MINING-BOT") == 0) {
				if (strcasecmp(go[c->ship_index].tsd.ship.mining_bot_name, sbname) != 0)
					continue;
			} else {
				printf("comparing '%s' to '%s'\n",
					sbname, namelist[j]);
				if (strcasecmp(sbname, namelist[j]) != 0)
					continue;
			}
			nchannels = 1;
			channel[0] = find_free_channel();
			switch_channel = 1;
			id = o->id;
			goto channels_maxxed;
		}
	}

channels_maxxed:
	pthread_mutex_unlock(&universe_mutex);

	/* If switching to channel to communicate with a starbase... */
	if (switch_channel && bridgelist[c->bridge].comms_channel != channel[0]) {
		snprintf(msg, sizeof(msg), "TRANSMISSION TERMINATED ON CHANNEL %u",
				bridgelist[c->bridge].comms_channel);
		send_comms_packet(&go[c->ship_index], name, bridgelist[c->bridge].comms_channel, msg);
		bridgelist[c->bridge].comms_channel = channel[0];
		bridgelist[c->bridge].npcbot.object_id = id;
		bridgelist[c->bridge].npcbot.channel = channel[0];
		bridgelist[c->bridge].npcbot.current_menu = starbase_main_menu;
		bridgelist[c->bridge].npcbot.special_bot = NULL;
		/* Note: client snoops this channel change message */
		/* TODO: add check here to see if hail is successful first */
		send_comms_packet(&go[c->ship_index], name, channel[0], "%s%u",
					COMMS_CHANNEL_CHANGE_MSG, channel[0]);
		send_to_npcbot(c->bridge, name, msg);
	} else {
		for (i = 0; i < nchannels; i++)
			send_comms_packet(&go[c->ship_index], name, channel[i],
					"*** HAILING ON CHANNEL %u ***",
					bridgelist[c->bridge].comms_channel);
			/* TODO: check to see if hail successful first? */
	}
	free(duptxt);
}

static void meta_comms_error(char *name, struct game_client *c, char *txt)
{
	printf("meta comms error %u,%s\n", bridgelist[c->bridge].comms_channel, txt);
}

static const struct meta_comms_data {
	char *command;
	meta_comms_func f;
} meta_comms[] = {
	{ "/channel", meta_comms_channel },
	{ "/hail", meta_comms_hail },
	{ "/inventory", meta_comms_inventory },
	{ "/manifest", meta_comms_inventory },
	{ "/passengers", meta_comms_inventory },
	{ "/eject", meta_comms_eject },
	{ "/about", meta_comms_about },
	{ "/help", meta_comms_help },
	{ "/computer", meta_comms_computer },
	{ "/antenna", meta_comms_antenna },
};

static void process_meta_comms_packet(char *name, struct game_client *c, char *txt)
{
	int i, j;
	struct meta_comms_data const * mcd = NULL;
	int current_match_length = 0;
	int len2 = strlen(txt);
	int limit;
	int ambiguous = 1;

	/* allow user to get away with typing enough of the command
	 * name to resolve ambiguity
	 */
	for (i = 0; i < ARRAYSIZE(meta_comms); i++) {
		int len = strlen(meta_comms[i].command);
		if (len < len2)
			limit = len;
		else
			limit = len2;

		for (j = 0; j < limit; j++) {
			if (toupper(txt[j]) == toupper(meta_comms[i].command[j])) {
				if (j == len - 1 || j == len2 - 1) { /* exact match */
					mcd = &meta_comms[i];
					ambiguous = 0;
					current_match_length = j;
					goto match;
				}
				continue;
			}
			if (j > current_match_length) {
				current_match_length = j;
				ambiguous = 0;
				mcd = &meta_comms[i];
			} else {
				if (j == current_match_length) {
					ambiguous = 1;
				}
			}
			break;
		}
	}

match:

	if (current_match_length > 1 && !ambiguous)
		mcd->f(name, c, txt);
	else
		meta_comms_error(name, c, txt);
}

static int process_comms_transmission(struct game_client *c, int use_real_name)
{
	unsigned char buffer[sizeof(struct comms_transmission_packet)];
	char txt[256];
	int rc, i;
	uint8_t len, enciphered;
	uint32_t id;
	char name[30];

	/* REMINDER: If the format of OPCODE_COMMS_TRANSMISSION ever changes, you need to
	 * fix distort_comms_message in snis_server.c too.
	 */

	rc = read_and_unpack_buffer(c, buffer, "bbw", &enciphered, &len, &id);
	if (rc)
		return rc;
	rc = snis_readsocket(c->socket, txt, len);
	if (rc)
		return rc;
	txt[len] = '\0';
	switch (enciphered) {
	case OPCODE_COMMS_PLAINTEXT:
		if (use_real_name) {
			snprintf(name, sizeof(name), "%s: ", bridgelist[c->bridge].shipname);
		} else {
			i = lookup_by_id(id);
			if (i < 0)
				return 0;
			snprintf(name, sizeof(name), "%s: ", go[i].sdata.name);
		}
		if (txt[0] == '/') {
			process_meta_comms_packet(name, c, txt);
			return 0;
		}
		send_comms_packet(&go[c->ship_index], name, bridgelist[c->bridge].comms_channel, "%s", txt);
		if (bridgelist[c->bridge].npcbot.channel == bridgelist[c->bridge].comms_channel)
			send_to_npcbot(c->bridge, name, txt);
		break;
	case OPCODE_COMMS_KEY_GUESS:
		if (len != 26)
			return -1;
		memcpy(bridgelist[c->bridge].guessed_key, txt, 26);
		break;
	default:
		return -1;
	}
	return 0;
}

static int process_natural_language_request(struct game_client *c)
{
	unsigned char buffer[sizeof(struct comms_transmission_packet)];
	char txt[256];
	uint8_t subcommand, len;
	int rc, b;
	char *ship_name, *ch;

	rc = read_and_unpack_buffer(c, buffer, "bb", &subcommand, &len);
	if (rc)
		return rc;
	rc = snis_readsocket(c->socket, txt, len);
	if (rc)
		return rc;
	txt[len] = '\0';
	switch (subcommand) {
	case OPCODE_NL_SUBCOMMAND_TEXT_REQUEST:
		perform_natural_language_request(c, txt);
		break;
	case OPCODE_NL_SUBCOMMAND_TEXT_TO_SPEECH:
		/* Assume the string is of the form "shipname: text to send" */
		ch = index(txt, ':');
		if (!ch)
			return 0;
		if (strlen(txt) - (ch - txt) <= 1) /* nothing after the colon */
			return 0;
		/* look up the bridge with the given ship name */
		ship_name = txt;
		*ch = '\0';
		pthread_mutex_lock(&universe_mutex);
		b = lookup_bridge_by_ship_name(ship_name);
		pthread_mutex_unlock(&universe_mutex);
		if (b < 0)
			return 0;
		snis_queue_add_text_to_speech(ch + 1, ROLE_TEXT_TO_SPEECH, bridgelist[b].shipid);
		break;
	default:
		return -1;
	}
	return 0;
}

static int process_textscreen_op(struct game_client *c)
{
	int rc;
	uint8_t subcommand, selection;
	unsigned char buffer[10];

	rc = read_and_unpack_buffer(c, buffer, "bb", &subcommand, &selection);
	if (rc)
		return rc;
	if (!current_user_menu_callback)
		return 0;
	switch (subcommand) {
	case OPCODE_TEXTSCREEN_MENU_CHOICE:
		schedule_one_callback(&callback_schedule, current_user_menu_callback,
			(double) c->shipid, (double) selection, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0);
		break;
	default:
		return -1;
	}
	return 0;
}

static int process_check_opcode_format(struct game_client *c)
{
	int rc;
	uint8_t subcode, opcode;
	uint16_t len;
	unsigned char buffer[256];
	const char *format;

	rc = read_and_unpack_buffer(c, buffer, "bb", &subcode, &opcode);
	if (rc)
		return rc;
	/* fprintf(stderr, "snis_server: Checking opcode for client.  check = %hhu, opcode = %hhu\n",
		subcode, opcode); */
	switch (subcode) {
	case OPCODE_CHECK_OPCODE_UNKNOWN:
		fprintf(stderr, "\n\n\n\n"
			"snis_server:  ==> Client reports unknown opcode: %hhu <==\n"
			"\n\n\n", opcode);
		break;
	case OPCODE_CHECK_OPCODE_MATCH:
		fprintf(stderr, "snis_server: Unexpected CHECK_OPCODE_MATCH from client.\n");
		return -1;
	case OPCODE_CHECK_OPCODE_QUERY:
		fprintf(stderr, "snis_server: Unexpected CHECK_OPCODE_QUERY from client.\n");
		return -1;
	case OPCODE_CHECK_OPCODE_VERIFY:
		rc = read_and_unpack_buffer(c, buffer, "h", &len);
		if (rc)
			return rc;
		/* fprintf(stderr, "snis_server: verifying client opcode, len = %hu\n", len); */
		if (len > 255) {
			fprintf(stderr, "\n\n\n\n"
				"snis_server: Client sent too long (%hu) format string for opcode %hhu verification."
				"\n\n\n\n", len, opcode);
			return -1;
		}
		memset(buffer, 0, sizeof(buffer));
		rc = snis_readsocket(c->socket, buffer, len);
		if (rc)
			return rc;
		buffer[len] = '\0';
		format = snis_opcode_format(opcode);
		if (!format) {
			fprintf(stderr, "\n\n\n\n"
				"snis_server: Client requested verification of unknown opcode %hhu with format '%s'\n"
				"\n\n\n", opcode, buffer);
			pb_queue_to_client(c, packed_buffer_new("bbb",
					OPCODE_CHECK_OPCODE_FORMAT,
					OPCODE_CHECK_OPCODE_UNKNOWN, opcode));
			return 0;
		}
		if (strcmp(format, (char *) buffer) == 0) {
			/* fprintf(stderr, "snis_server: Client opcode %hhu verified.\n", opcode); */
			pb_queue_to_client(c, packed_buffer_new("bbb",
					OPCODE_CHECK_OPCODE_FORMAT,
					OPCODE_CHECK_OPCODE_MATCH, opcode));
			return 0;
		}
		fprintf(stderr, "\n\n\n\n"
			"snis_server:  ==> Client opcode %hhu format mismatch, expected '%s', got '%s' <==\n"
			"\n\n\n", opcode, format, buffer);
		pb_queue_to_client(c, packed_buffer_new("bbb",
				OPCODE_CHECK_OPCODE_FORMAT,
				OPCODE_CHECK_OPCODE_MISMATCH, opcode));
		return 0;
	case OPCODE_CHECK_OPCODE_MISMATCH:
		fprintf(stderr, "snis_server: Unexpected CHECK_OPCODE_MISMATCH from client.\n");
		return -1;
	default:
		return -1;
	}
	return 0;
}

static int process_set_waypoint(struct game_client *c)
{
	int i, rc;
	uint8_t subcode, row;
	double value[3];
	unsigned char buffer[20];
	int b;
	struct snis_entity *o;

	rc = read_and_unpack_buffer(c, buffer, "b", &subcode);
	if (rc)
		return rc;
	switch (subcode) {
	case OPCODE_SET_WAYPOINT_CLEAR:
		rc = read_and_unpack_buffer(c, buffer, "b", &row);
		if (rc)
			return rc;
		pthread_mutex_lock(&universe_mutex);
		b = lookup_bridge_by_shipid(c->shipid);
		rc = lookup_by_id(c->shipid);
		if (rc < 0) {
			pthread_mutex_unlock(&universe_mutex);
			return 0;
		}
		o = &go[rc]; /* our ship */
		if (b < 0) {
			pthread_mutex_unlock(&universe_mutex);
			return 0;
		}
		/* If any RTS ships were headed to this waypoint, it is gone now. */
		if (rts_mode) {
			for (i = 0; i <= snis_object_pool_highest_object(pool); i++) {
				struct snis_entity *s = &go[i];
				if (s->alive && s->type == OBJTYPE_SHIP2 &&
					s->sdata.faction == o->sdata.faction &&
					o->tsd.ship.ai[0].ai_mode == AI_MODE_RTS_MOVE_TO_WAYPOINT &&
					o->tsd.ship.ai[0].u.goto_waypoint.bridge_ship_id == c->shipid) {
					if (o->tsd.ship.ai[0].u.goto_waypoint.waypoint == row)
						o->tsd.ship.ai[0].ai_mode = AI_MODE_RTS_STANDBY;
					else if (o->tsd.ship.ai[0].u.goto_waypoint.waypoint > row)
						o->tsd.ship.ai[0].u.goto_waypoint.waypoint--;
				}
			}
		}

		if (row >= 0 && row < bridgelist[b].nwaypoints) {
			for (int i = row; i < bridgelist[b].nwaypoints - 1; i++)
				bridgelist[b].waypoint[i] = bridgelist[b].waypoint[i + 1];
			bridgelist[b].nwaypoints--;
			if (bridgelist[b].selected_waypoint >= 0) {
				if (row == bridgelist[b].selected_waypoint)
					bridgelist[b].selected_waypoint = -1;
				else if (row < bridgelist[b].selected_waypoint)
					bridgelist[b].selected_waypoint--;
			} else {
				bridgelist[b].selected_waypoint = -1;
			}
			set_waypoints_dirty_all_clients_on_bridge(c->shipid, 1);
		}
		pthread_mutex_unlock(&universe_mutex);
		return 0;
	case OPCODE_SET_WAYPOINT_ADD_ROW:
		rc = read_and_unpack_buffer(c, buffer, "SSS",
					&value[0], (int32_t) UNIVERSE_DIM,
					&value[1], (int32_t) UNIVERSE_DIM,
					&value[2], (int32_t) UNIVERSE_DIM);
		if (rc)
			return rc;
		pthread_mutex_lock(&universe_mutex);
		b = lookup_bridge_by_shipid(c->shipid);
		if (b < 0) {
			pthread_mutex_unlock(&universe_mutex);
			return 0;
		}
		if (bridgelist[b].nwaypoints < MAXWAYPOINTS) {
			bridgelist[b].waypoint[bridgelist[b].nwaypoints].x = value[0];
			bridgelist[b].waypoint[bridgelist[b].nwaypoints].y = value[1];
			bridgelist[b].waypoint[bridgelist[b].nwaypoints].z = value[2];
			bridgelist[b].nwaypoints++;
			set_waypoints_dirty_all_clients_on_bridge(c->shipid, 1);
		}
		pthread_mutex_unlock(&universe_mutex);
		return 0;
	default:
		fprintf(stderr, "%s: bad subcode for OPCODE_SET_WAYPOINT: %hhu\n",
			logprefix(), subcode);
		return -1;
	}
}

static void enscript_prologue(FILE *f)
{
	fprintf(f, "\n");
}

static void enscript_player(FILE *f, struct snis_entity *o, int player)
{
	if (player == 1)
		fprintf(f, "player_ids = get_player_ship_ids();\n\n");
	fprintf(f, "-- set player %d location\n", player);
	fprintf(f, "if (player_ids[%d]) then\n", player);
	fprintf(f, "	move_object(player_ids[%d], %lf, %lf, %lf);\n",
			player, o->x, o->y, o->z);
	fprintf(f, "end\n\n");
}

static void enscript_ship(FILE *f, struct snis_entity *o, int ship)
{
	if (ship == 1)
		fprintf(f, "ship = {};\n");
	fprintf(f, "ship[%d] = add_ship(\"%s\", %lf, %lf, %lf, %d, %d, %hhd);\n",
			 ship, o->sdata.name, o->x, o->y, o->z,
			o->tsd.ship.shiptype, o->sdata.faction,
			o->tsd.ship.auto_respawn);
}

static void enscript_asteroid(FILE *f, struct snis_entity *o, int asteroid)
{
	if (asteroid == 1)
		fprintf(f, "asteroid = {};\n");
	fprintf(f, "asteroid[%d] = add_asteroid(%lf, %lf, %lf);\n",
				asteroid, o->x, o->y, o->z);
	fprintf(f, "set_asteroid_speed(asteroid[%d], %f);\n",
				asteroid, o->tsd.asteroid.v);
}

static void enscript_starbase(FILE *f, struct snis_entity *o, int starbase)
{
	if (starbase == 1)
		fprintf(f, "starbase = {};\n");
	fprintf(f, "starbase[%d] = add_starbase(%lf, %lf, %lf, %d);\n",
			starbase, o->x, o->y, o->z, starbase - 1);
}

static void enscript_nebula(FILE *f, struct snis_entity *o, int nebula)
{
	if (nebula == 1)
		fprintf(f, "nebula = {};\n");
	fprintf(f, "nebula[%d] = add_nebula(\"%s\", %lf, %lf, %lf, %lf);\n",
		nebula, o->sdata.name, o->x, o->y, o->z, o->tsd.nebula.r);
}

static void enscript_wormhole(FILE *f, struct snis_entity *o, int wormhole)
{
	if (wormhole == 1)
		fprintf(f, "wormhole = {};\n");
	fprintf(f, "-- FIXME enscript wormhole %d\n", wormhole);
}

static void enscript_spacemonster(FILE *f, struct snis_entity *o, int spacemonster)
{
	if (spacemonster == 1)
		fprintf(f, "spacemonster = {};\n");
	fprintf(f, "spacemonster[%d] = add_spacemonster(\"%s\", %lf, %lf, %lf);\n",
		spacemonster, o->sdata.name, o->x, o->y, o->z);
}

static void enscript_planet(FILE *f, struct snis_entity *o, int planet)
{
	if (planet == 1)
		fprintf(f, "planet = {};\n");
	fprintf(f, "planet[%d] = add_planet(\"%s\", %lf, %lf, %lf, %lf, %hhd);\n",
			planet, o->sdata.name, o->x, o->y, o->z,
			o->tsd.planet.radius, o->tsd.planet.security);
}

static void enscript_black_hole(FILE *f, struct snis_entity *o, int black_hole)
{
	if (black_hole == 1)
		fprintf(f, "black_hole = {};\n");
	fprintf(f, "black_hole[%d] = add_black_hole(\"%s\", %lf, %lf, %lf, %lf);\n",
			black_hole, o->sdata.name, o->x, o->y, o->z,
			o->tsd.black_hole.radius);
}

static void enscript_derelict(FILE *f, struct snis_entity *o, int derelict)
{
	if (derelict == 1)
		fprintf(f, "derelict = {};\n");
	fprintf(f, "derelict[%d] = add_derelict(\"%s\", %lf, %lf, %lf, %d, %d);\n",
			derelict, o->sdata.name, o->x, o->y, o->z,
			o->tsd.derelict.shiptype, o->sdata.faction);
}

static void enscript_cargo_container(FILE *f, struct snis_entity *o, int cargo_container)
{
	if (cargo_container == 1)
		fprintf(f, "cargo_container = {};\n");
	fprintf(f, "cargo_container[%d] = add_cargo_container(%lf, %lf, %lf, %lf, %lf, %lf);\n",
			cargo_container, o->x, o->y, o->z, o->vx, o->vy, o->vz);
}

static void partially_enscript_game_state(FILE *f)
{
	int i;
	int player = 0;
	int ship = 0;
	int asteroid = 0;
	int starbase = 0;
	int nebula = 0;
	int wormhole = 0;
	int spacemonster = 0;
	int planet = 0;
	int black_hole = 0;
	int derelict = 0;
	int cargo_container = 0;

	enscript_prologue(f);
	pthread_mutex_lock(&universe_mutex);
	for (i = 0; i <= snis_object_pool_highest_object(pool); i++) {
		struct snis_entity *o = &go[i];
		if (!o->alive)
			continue;
		switch (go[i].type) {
		case OBJTYPE_SHIP1:
			enscript_player(f, o, ++player);
			break;
		case OBJTYPE_SHIP2:
			enscript_ship(f, o, ++ship);
			break;
		case OBJTYPE_ASTEROID:
			enscript_asteroid(f, o, ++asteroid);
			break;
		case OBJTYPE_STARBASE:
			enscript_starbase(f, o, ++starbase);
			break;
		case OBJTYPE_NEBULA:
			enscript_nebula(f, o, ++nebula);
			break;
		case OBJTYPE_WORMHOLE:
			enscript_wormhole(f, o, ++wormhole);
			break;
		case OBJTYPE_SPACEMONSTER:
			enscript_spacemonster(f, o, ++spacemonster);
			break;
		case OBJTYPE_PLANET:
			enscript_planet(f, o, ++planet);
			break;
		case OBJTYPE_BLACK_HOLE:
			enscript_black_hole(f, o, ++black_hole);
			break;
		case OBJTYPE_DERELICT:
			enscript_derelict(f, o, ++derelict);
			break;
		case OBJTYPE_CARGO_CONTAINER:
			enscript_cargo_container(f, o, ++cargo_container);
			break;
		case OBJTYPE_TRACTORBEAM:
		case OBJTYPE_LASERBEAM:
		case OBJTYPE_WARP_EFFECT:
		case OBJTYPE_SHIELD_EFFECT:
		case OBJTYPE_DEBRIS:
		case OBJTYPE_SPARK:
		case OBJTYPE_TORPEDO:
		case OBJTYPE_LASER:
		case OBJTYPE_EXPLOSION:
		default:
			break;
		}
	}
	pthread_mutex_unlock(&universe_mutex);
}

static int process_enscript_command(struct game_client *c)
{
	unsigned char buffer[sizeof(struct lua_script_packet)];
	char txt[256];
	int fd, rc;
	uint8_t len;
	char scriptname[PATH_MAX];
	FILE *f;

	rc = read_and_unpack_buffer(c, buffer, "b", &len);
	if (rc)
		return rc;
	rc = snis_readsocket(c->socket, txt, len);
	if (rc)
		return rc;
	txt[len] = '\0';

	if (!lua_enscript_enabled) {
		send_demon_console_msg("ENSCRIPT IS DISABLED ON THIS SERVER");
		return 0;
	}

	/* TODO: Send this client side instead of storing server side. */
#define LUASCRIPTDIR STRPREFIX(PREFIX) "/share/snis/luascripts"
	snprintf(scriptname, sizeof(scriptname) - 1, "%s/%s", LUASCRIPTDIR, txt);

	/* Open the file only if it doesn't already exist.
	 * Using a combo of open + fdopen here rather than just fopen
	 * to guarantee file doesn't already exist in a race free way.
	 */
	fd = open(scriptname, O_CREAT | O_WRONLY, 0644);
	if (fd < 0) /* FIXME: we fail silently here. */
		return 0;

	/* let's do buffered i/o for this */
	f = fdopen(fd, "w");
	if (!f) /* FIXME: we fail silently here. */
		return 0;

	partially_enscript_game_state(f);
	fclose(f); /* close() not needed, fclose() is enough. */
	return 0;
}

static void server_builtin_clients(__attribute__((unused)) char *cmd)
{
	int i, rc;
	char buf[80];
	char *station;
	struct sockaddr_in name;
	socklen_t namelen;
	uint16_t port;
	uint32_t ip;

	send_demon_console_msg("%10s %5s %5s %20s %8s %20s",
				"CURRENT", "CLNT", "BRDG", "SHIP NAME", "ROLES", "IP ADDR");
	send_demon_console_msg("--------------------------------------------------------------");
	for (i = 0; i < nclients; i++) {
		struct game_client *c = &client[i];
		switch (c->current_station) {
		case DISPLAYMODE_MAINSCREEN:
			station = "MAIN VIEW";
			break;
		case DISPLAYMODE_NAVIGATION:
			station = "NAVIGATION";
			break;
		case DISPLAYMODE_WEAPONS:
			station = "WEAPONS";
			break;
		case DISPLAYMODE_ENGINEERING:
			station = "ENGINEERING";
			break;
		case DISPLAYMODE_SCIENCE:
			station = "SCIENCE";
			break;
		case DISPLAYMODE_COMMS:
			station = "COMMS";
			break;
		case DISPLAYMODE_DEMON:
			station = "DEMON";
			break;
		case DISPLAYMODE_DAMCON:
			station = "DAMCON";
			break;
		case DISPLAYMODE_FONTTEST:
			station = "FONTTEST";
			break;
		case DISPLAYMODE_INTROSCREEN:
			station = "INTROSCREEN";
			break;
		case DISPLAYMODE_LOBBYSCREEN:
			station = "LOBBYSCREEN";
			break;
		case DISPLAYMODE_CONNECTING:
			station = "CONNECTING";
			break;
		case DISPLAYMODE_CONNECTED:
			station = "CONNECTED";
			break;
		case DISPLAYMODE_NETWORK_SETUP:
			station = "NETWORK SETUP";
			break;
		default:
			station = "UNKNOWN";
			break;
		}
		ip = 0;
		port = 0;
		if (c->socket >= 0) {
			namelen = sizeof(name);
			rc = getsockname(c->socket, &name, &namelen);
			if (rc == 0) {
				memcpy(&ip, &name.sin_addr, sizeof(ip));
				ip = ntohl(ip);
				port = ntohs(name.sin_port);
			}
		}
		snprintf(buf, sizeof(buf), "%s %5d %5d %20s %08x %hhu.%hhu.%hhu.%hhu:%hu", station, i, c->bridge,
				bridgelist[c->bridge].shipname, c->role,
				(ip >> 24) & 0xff, (ip >> 16) & 0xff, (ip >> 8) & 0xff, ip & 0xff, port);
		send_demon_console_msg(buf);
	}
}

static void server_builtin_disconnect(char *cmd)
{
	int rc, i;

	rc = sscanf(cmd, "%*s %d", &i);
	if (rc != 1) {
		send_demon_console_color_msg(YELLOW, "INVALID DISCONNECT COMMAND");
		send_demon_console_color_msg(YELLOW, "     \"%s\"", cmd);
		return;
	}
	if (i >= 0 && i < nclients) {
		close(client[i].socket);
		client[i].socket = -1;
		send_demon_console_msg("DISCONNECTED CLIENT %d", i);
	} else {
		send_demon_console_color_msg(YELLOW, "INVALID CLIENT %d", i);
	}
}

static struct tweakable_var_descriptor server_tweak[] = {
	{ "INITIAL_MISSILE_COUNT",
		"NUMBER OF MISSILES PLAYERS MAY HAVE",
		&initial_missile_count, 'i', 0, 100, INITIAL_MISSILE_COUNT,
		0.0, 255.0, INITIAL_MISSILE_COUNT },
	{ "MISSILE_MAX_VELOCITY",
		"MAXIMUM VELOCITY OF MISSILES", /* used as denominator so min cannot be zero */
		&missile_max_velocity, 'f', 5.0, 1000.0, DEFAULT_MISSILE_VELOCITY, 0, 0, 0 },
	{ "MAX_PLAYER_VELOCITY",
		"MAXIMUM VELOCITY PLAYER MAY TRAVEL USING IMPULSE POWER",
		&max_player_velocity, 'f', 20.0, 20000.0, MAX_PLAYER_VELOCITY, 0, 0, 0},
	{ "MAX_REVERSE_PLAYER_VELOCITY",
		"MAXIMUM REVERSE VELOCITY PLAYER MAY TRAVEL USING IMPULSE POWER",
		&max_reverse_player_velocity, 'f', 0.0, 20000.0, MAX_REVERSE_PLAYER_VELOCITY, 0, 0, 0},
	{ "PLAYER_VELOCITY_INCREMENT",
		"MAXIMUM ACCELERATION PLAYER SHIP HAS USING IMPULSE POWER",
		&player_velocity_increment, 'f',
		0.0, 100.0, PLAYER_VELOCITY_INCREMENT, 0, 0, 0 },
	{ "PLAYER_VELOCITY_DAMPING",
		"DECELERATION FACTOR OF PLAYER SHIP",
		&player_velocity_damping, 'f',
		0.1, 0.9999, PLAYER_VELOCITY_DAMPING, 0, 0, 0 },
	{ "TORPEDO_DAMAGE_FACTOR",
		"TORPEDO DAMAGE MULTIPLIER",
		&torpedo_damage_factor, 'f',
		0.0, 100.0, TORPEDO_WEAPONS_FACTOR, 0, 0, 0 },
	{ "MISSILE_DAMAGE_FACTOR",
		"MISSILE DAMAGE MULTIPLIER",
		&missile_damage_factor, 'f',
		0.0, 100.0, MISSILE_EXPLOSION_WEAPONS_FACTOR, 0, 0, 0 },
	{ "SPACEMONSTER_DAMAGE_FACTOR",
		"SPACE MONSTER DAMAGE MULTIPLIER",
		&spacemonster_damage_factor, 'f',
		0.0, 100.0, SPACEMONSTER_WEAPONS_FACTOR, 0, 0, 0 },
	{ "ATMOSPHERE_DAMAGE_FACTOR",
		"ATMOSPHERE DAMAGE MULTIPLIER",
		&atmosphere_damage_factor, 'f',
		0.0, 100.0, ATMOSPHERE_DAMAGE_FACTOR, 0, 0, 0 },
	{ "WARP_CORE_DAMAGE_FACTOR",
		"WARP CORE EXPLOSION DAMAGE MULTIPLIER",
		&warp_core_damage_factor, 'f',
		0.0, 100.0, WARP_CORE_EXPLOSION_WEAPONS_FACTOR, 0, 0, 0 },
	{ "THREAT_LEVEL_FLEE_THRESHOLD",
		"DANGER LEVEL AT WHICH NPC SHIPS DECIDE TO FLEE",
		&threat_level_flee_threshold, 'f',
		0.0, 1000.0, THREAT_LEVEL_FLEE_THRESHOLD, 0, 0, 0 },
	{ "MAX_DAMCON_ROBOT_VELOCITY",
		"MAX VELOCITY OF DAMAGE CONTROL ROBOT",
		&max_damcon_robot_velocity, 'f',
		0.0, 100.0, MAX_ROBOT_VELOCITY, 0, 0, 0 },
	{ "MAX_SPACEMONSTER_ACCEL",
		"MAX ACCELERATION OF SPACEMONSTERS",
		&max_spacemonster_accel, 'f',
		0.0, 100.0, MAX_SPACEMONSTER_ACCEL, 0, 0, 0 },
	{ "MAX_SPACEMONSTER_VELOCITY",
		"MAX VELOCITY OF SPACEMONSTERS",
		&max_spacemonster_velocity, 'f',
		0.0, 100.0, MAX_SPACEMONSTER_VELOCITY, 0, 0, 0 },
	{ "MAX_DAMCON_ROBOT_BRAKING",
		"MAX DECELERATION RATE OF DAMAGE CONTROL ROBOT",
		&max_damcon_robot_braking, 'f',
		0.0, 100.0, MAX_ROBOT_BRAKING, 0, 0, 0 },
	{ "TORPEDO_LIFETIME",
		"TORPEDO LIFETIME IN 10ths OF A SECOND",
		&torpedo_lifetime, 'i',
		0.0, 0.0, 0.0, 0, TORPEDO_LIFETIME * 10, TORPEDO_LIFETIME },
	{ "MISSILE_LIFETIME",
		"MISSILE LIFETIME IN 10ths OF A SECOND",
		&missile_lifetime, 'i',
		0.0, 0.0, 0.0, 0, MISSILE_LIFETIME * 10, MISSILE_LIFETIME },
	{ "TORPEDO_VELOCITY",
		"TORPEDO VELOCITY",
		&torpedo_velocity, 'f',
		0.0, TORPEDO_VELOCITY * 10.0, TORPEDO_VELOCITY, 0, 0, 0 },
	{ "MISSILE_TARGET_DIST",
		"HOW CLOSE TARGET MUST BE FOR MISSILE TO BE FIREABLE",
		&missile_target_dist, 'f',
		0.0, MISSILE_TARGET_DIST * 10.0, MISSILE_TARGET_DIST, 0, 0, 0 },
	{ "LASERBEAM_MISSILE_CHANCE",
		"CHANCE OUT OF 1000 THAT LASERBEAM DESTROYS MISSILE",
		&laserbeam_missile_chance, 'i', 0.0, 0.0, 0.0, 0, 1000, LASERBEAM_MISSILE_CHANCE },
	{ "MAX_MISSILE_DELTAV",
		"MAX ACCELERATION OF MISSILES",
		&max_missile_deltav, 'f',
		0.0, MAX_MISSILE_DELTAV * 10.0, MAX_MISSILE_DELTAV, 0, 0, 0 },
	{ "MISSILE_FIRE_CHANCE",
		"CHANCE OF NPC SHIP FIRING MISSILES (0-100)",
		&enemy_missile_fire_chance, 'f',
		0.0, 100.0, ENEMY_MISSILE_FIRE_CHANCE, 0, 0, 0 },
	{ "TORPEDO_FIRE_CHANCE",
		"CHANCE OF NPC SHIP FIRING TORPEDOES (0-100)",
		&enemy_torpedo_fire_chance, 'f',
		0.0, 100.0, ENEMY_TORPEDO_FIRE_CHANCE, 0, 0, 0 },
	{ "LASER_FIRE_CHANCE",
		"CHANCE OF NPC SHIP FIRING LASER (0-100)",
		&enemy_laser_fire_chance, 'f',
		0.0, 100.0, ENEMY_LASER_FIRE_CHANCE, 0, 0, 0 },
	{ "MISSILE_FIRE_INTERVAL",
		"10THS OF SECS. MINIMUM PERIOD BETWEEN ENEMY MISSILE LAUNCHES",
		&enemy_missile_fire_interval, 'i',
		0.0, 0.0, 0.0, 0, 10000, ENEMY_MISSILE_FIRE_INTERVAL },
	{ "LASER_FIRE_INTERVAL",
		"10THS OF SECS. MINIMUM PERIOD BETWEEN ENEMY LASER FIRE",
		&enemy_laser_fire_interval, 'i',
		0.0, 0.0, 0.0, 0, 10000, ENEMY_LASER_FIRE_INTERVAL },
	{ "TORPEDO_FIRE_INTERVAL",
		"10THS OF SECS. MINIMUM PERIOD BETWEEN ENEMY TORPEDO FIRE",
		&enemy_laser_fire_interval, 'i',
		0.0, 0.0, 0.0, 0, 10000, ENEMY_TORPEDO_FIRE_INTERVAL },
	{ "MISSILE_PROXIMITY_DISTANCE",
		"MINIMUM DISTANCE BETWEEN MISSILE AND TARGET TO DETONATE",
		&missile_proximity_distance, 'f',
		0.0, 2000.0, MISSILE_PROXIMITY_DISTANCE, 0, 0, 0 },
	{ "CHAFF_PROXIMITY_DISTANCE",
		"MINIMUM DISTANCE BETWEEN MISSILE AND CHAFF FOR MISSILE TO GET CONFUSED",
		&chaff_proximity_distance, 'f',
		0.0, 2000.0, CHAFF_PROXIMITY_DISTANCE, 0, 0, 0 },
	{ "CHAFF_COOLDOWN_TIME",
		"MINIMUM 10ths OF SECONDS BETWEEN CHAFF DEPLOYMENTS",
		&chaff_cooldown_time, 'i', 0.0, 0.0, 0.0, 0, 1000, CHAFF_COOLDOWN_TIME },
	{ "MISSILE_EXPLOSION_DAMAGE_DISTANCE",
		"DAMAGE FACTOR PER UNIT DISTANCE",
		&missile_explosion_damage_distance, 'f',
		0.0, 2000.0, MISSILE_EXPLOSION_DAMAGE_DISTANCE, 0, 0, 0 },
	{ "MISSILE_COUNTERMEASURE_DELAY",
		"HOW MANY 10ths secs missiles can move before NPCS deploy countermeasures",
		&missile_countermeasure_delay, 'i', 0.0, 0.0, 0.0, 0, 1000, MISSILE_COUNTERMEASURE_DELAY },
	{ "SPACEMONSTER_FLEE_DIST",
		"SPACE MONSTER FLEE DISTANCE",
		&spacemonster_flee_dist, 'f',
		500.0, 5000.0, SPACEMONSTER_FLEE_DIST, 0, 0, 0 },
	{ "SPACEMONSTER_AGGRO_RADIUS",
		"SPACE MONSTER AGGRO RADIUS",
		&spacemonster_aggro_radius, 'f',
		500.0, 15000.0, SPACEMONSTER_AGGRO_RADIUS, 0, 0, 0 },
	{ "SPACEMONSTER_COLLISION_RADIUS",
		"SPACE MONSTER COLLISION RADIUS",
		&spacemonster_collision_radius, 'f',
		10.0, 500.0, SPACEMONSTER_COLLISION_RADIUS, 0, 0, 0 },
	{ "CARGO_CONTAINER_MAX_VELOCITY",
		"CARGO CONTAINER MAX VELOCITY",
		&cargo_container_max_velocity, 'f',
		0.0, MAX_PLAYER_VELOCITY * 10.0, CARGO_CONTAINER_MAX_VELOCITY, 0, 0, 0 },
	{ "BOUNTY_CHANCE",
		"CHANCE THAT A NEWLY ADDED SHIP WILL HAVE A BOUNTY ON IT",
		&bounty_chance, 'f',
		0.0, 1.0, BOUNTY_CHANCE, 0, 0, 0 },
	{ "CHAFF_SPEED",
		"SPEED OF CHAFF PARTICLES",
		&chaff_speed, 'f',
		10.0, 500.0, CHAFF_SPEED, 0, 0, 0 },
	{ "CHAFF_COUNT",
		"NUMBER OF CHAFF PARTICLES TO DEPLOY",
		&chaff_count, 'i',
		0.0, 0.0, 0.0, 1, 25, CHAFF_COUNT },
	{ "CHAFF_CONFUSE_CHANCE",
		"LIKELYHOOD CHAFF WILL CONFUSE MISSILES",
		&chaff_confuse_chance, 'f',
		0.0, 1.0, CHAFF_CONFUSE_CHANCE, 0, 0, 0 },
	{ "MULTIVERSE_DEBUG",
		"MULTIVERSE_DEBUG FLAG 1 = ON, 0 = OFF",
		&multiverse_debug, 'i',
		0.0, 0.0, 0.0, 0, 1, 0 },
	{ "SUPPRESS_STARBASE_COMPLAINTS", "0 or 1, ENABLES OR SUPPRESSES STARBASE COMPLAINING",
		&suppress_starbase_complaints, 'i', 0.0, 0.0, 0.0, 0, 1, 0 },
	{ "INVINCIBILITY", "0 or 1, ENABLES OR SUPPRESSES PLAYER SHIP INVINCIBILITY",
		&player_invincibility, 'i', 0.0, 0.0, 0.0, 0, 1, 0 },
	{ "GAME_PAUSED", "0 or 1, 1 TO PAUSE, 0 to UNPAUSE (EXPERIMENTAL)",
		&game_paused, 'i', 0.0, 0.0, 0.0, 0, 1, 0 },
	{ "LUA_INSTRUCTION_LIMIT", "USED TO DETECT RUNAWAY LUA SCRIPTS",
		&lua_instruction_count_limit, 'i', 0.0, 0.0, 0.0, 0, 1000000, DEFAULT_LUA_INSTRUCTION_COUNT_LIMIT },
	{ "BANDWIDTH_THROTTLE_DISTANCE", "DISTANCE AT WHICH UPDATES MAY BE DROPPED TO CONTROL BANDWIDTH",
		&bandwidth_throttle_distance, 'f',
			(float) XKNOWN_DIM / 4.0f, (float) XKNOWN_DIM, (float) XKNOWN_DIM / 3.0f, 0, 0, 0 },
	{ "DISTANT_UPDATE_PERIOD", "HOW MANY TICKS BEFORE UPDATING DISTANT OBJECTS",
		&distant_update_period, 'i', 0.0, 0.0, 0.0, 1, 40, 20},
	{ "DOCKING_BY_FACTION", "1 - STARBASES DISCRIMINATE BY FACTION, 0 - THEY DO NOT",
		&docking_by_faction, 'i', 0.0, 0.0, 0.0, 0, 1, 0},
	{ "NPC_SHIP_COUNT", "NUMBER OF NPC SHIPS TO GENERATE",
		&npc_ship_count, 'i', 0.0, 0.0, 0.0, 0, 300, 250},
	{ "ASTEROID_COUNT", "NUMBER OF ASTEROIDS TO GENERATE",
		&asteroid_count, 'i', 0.0, 0.0, 0.0, 0, 300, 200},
	{ "ROLL_ADJUST_FACTOR", "ADJUST SPEED OF ROLL INPUT",
		&roll_adjust_factor, 'f', 0.0, 3.0, 1.0, 0, 0, 0},
	{ "YAW_ADJUST_FACTOR", "ADJUST SPEED OF YAW INPUT",
		&yaw_adjust_factor, 'f', 0.0, 3.0, 1.0, 0, 0, 0},
	{ "PITCH_ADJUST_FACTOR", "ADJUST SPEED OF PITCH INPUT",
		&pitch_adjust_factor, 'f', 0.0, 3.0, 1.0, 0, 0, 0},
	{ "ROLL_FINE_ADJUST_FACTOR", "ADJUST SPEED OF FINE ROLL INPUT",
		&roll_fine_adjust_factor, 'f', 0.0, 3.0, 1.0, 0, 0, 0},
	{ "YAW_FINE_ADJUST_FACTOR", "ADJUST SPEED OF FINE YAW INPUT",
		&yaw_fine_adjust_factor, 'f', 0.0, 3.0, 1.0, 0, 0, 0},
	{ "PITCH_FINE_ADJUST_FACTOR", "ADJUST SPEED OF FINE PITCH INPUT",
		&pitch_fine_adjust_factor, 'f', 0.0, 3.0, 1.0, 0, 0, 0},
	{ "STEERING_ADJUST_FACTOR", "ADJUST SPEED OF STEERING INPUT",
		&steering_adjust_factor, 'f', 0.0, 3.0, 1.0, 0, 0, 0},
	{ "STEERING_FINE_ADJUST_FACTOR", "ADJUST SPEED OF FINE STEERING INPUT",
		&steering_fine_adjust_factor, 'f', 0.0, 3.0, 0.5, 0, 0, 0},
	{ "SCI_BW_YAW_INCREMENT", "SCIENCE BEAM YAW INCREMENT IN RADIANS",
		&sci_bw_yaw_increment, 'f', 0.5 * M_PI / 180.0, 30 * M_PI / 180.0,
		SCI_BW_YAW_INCREMENT, 0, 0, 0 },
	{ "SCI_BW_YAW_INCREMENT_FINE", "FINE SCIENCE BEAM YAW INCREMENT IN RADIANS",
		&sci_bw_yaw_increment, 'f', 0.001 * M_PI / 180.0, 10 * M_PI / 180.0,
		SCI_BW_YAW_INCREMENT_FINE, 0, 0, 0 },
	{ "PLAYER_RESPAWN_TIME", "SECONDS PLAYER SPENDS DEAD BEFORE RESPAWNING",
		&player_respawn_time, 'i', 0.0, 0.0, 0.0, 0, 255, RESPAWN_TIME_SECS },
	{ "STANDARD_ORBIT", "RADIUS MULTIPLIER FOR STANDARD ORBIT ALTITUDE",
		&standard_orbit, 'f', 1.1, 2.95, 1.1, 0, 0, 0},
	{ "STARBASES_ORBIT", "0 or 1, starbases are stationary - 0, or may orbit planets - 1",
		&starbases_orbit, 'i', 0.0, 0.0, 0.0, 0, 1, 0},
	{ "AI_TRACE_ID", "AI TRACE ID",
		&ai_trace_id, 'i', 0.0, 0.0, 0.0, INT_MIN, INT_MAX, -1},
	{ "LIMIT_WARP_NEAR_PLANETS", "LIMITS WARPING NEAR PLANETS OR BLACK HOLES",
		&limit_warp_near_planets, 'i', 0.0, 0.0, 0.0, 0, 1, 1},
	{ "WARP_LIMIT_DIST_FACTOR", "MULTIPLE OF RADIUS YOU MUST BE FROM GRAVITY SOURCE TO WARP",
		&warp_limit_dist_factor, 'f', 1.0, 1000000.0, 1.5, 0, 0, 0},
	{ NULL, NULL, NULL, '\0', 0.0, 0.0, 0.0, 0, 0, 0 },
};

static void server_builtin_set(char *cmd)
{
	char msg[255];

	(void) tweak_variable(server_tweak, ARRAYSIZE(server_tweak), cmd, msg, sizeof(msg));
	send_demon_console_msg(msg);
}

static void server_builtin_vars(__attribute__((unused)) char *cmd)
{
	tweakable_vars_list(server_tweak, ARRAYSIZE(server_tweak), send_demon_console_msg);
}

static void server_builtin_describe(char *cmd)
{
	(void) tweakable_var_describe(server_tweak, ARRAYSIZE(server_tweak), cmd, send_demon_console_msg, 0);
}

static uint32_t verify_role(uint32_t role)
{
	/* Enforce that we always have at least one "on screen" role active.
	 * We can't set a client to have no onscreen role.
	 */
	const uint32_t onscreen_roles = ROLE_MAIN | ROLE_NAVIGATION | ROLE_WEAPONS |
				ROLE_ENGINEERING | ROLE_SCIENCE | ROLE_COMMS |
				ROLE_DEMON | ROLE_DAMCON;

	if ((role & onscreen_roles) == 0)
		role = role | ROLE_MAIN;

	/* Enforce that if a client has ROLE_MAIN, it has all onscreen roles */
	if (role & ROLE_MAIN)
		role |= onscreen_roles;
	
	role = role | ROLE_DEMON; /* always have demon, otherwise you can get into trouble. */
	return role;
}

static void invalid_role_syntax_msg(void)
{
	send_demon_console_color_msg(YELLOW, "INVALID ROLE SYNTAX");
	send_demon_console_color_msg(YELLOW, "USAGE:");
	send_demon_console_color_msg(YELLOW, "- TO LIST ROLES: ROLE <CLIENT>");
	send_demon_console_color_msg(YELLOW, "- TO ADD ROLES: ROLE <CLIENT> +ROLE");
	send_demon_console_color_msg(YELLOW, "- TO REMOVE ROLES: ROLE <CLIENT> -ROLE");
	send_demon_console_color_msg(YELLOW, "- TO SET SINGLE ROLE: ROLE <CLIENT> ROLE");
	send_demon_console_color_msg(
		YELLOW, " - ROLES ARE: MAIN, NAV, WEAP, ENG, DAM, COM, SCI, DEM, TTS, SOU, ALL");
}

static void server_builtin_setrole(char *cmd)
{
	int rc;
	int c, mode;
	uint32_t role, newrole;
	char rolestr[255];

	rc = sscanf(cmd, "ROLE %d +%s", &c, rolestr);
	if (rc == 2) {
		/* Add a new role to a client */
		mode = 0;
	} else {
		rc = sscanf(cmd, "ROLE %d -%s", &c, rolestr);
		if (rc == 2) {
			/* Subtract a role from a client */
			mode = 1;
		} else {
			rc = sscanf(cmd, "ROLE %d %s", &c, rolestr);
			if (rc == 2) {
				/* set a client to have a single role */
				mode = 2;
			} else {
				rc = sscanf(cmd, "ROLE %d", &c);
				if (rc == 1) {
					/* List a client's roles */
					mode = 3;
				} else {
					invalid_role_syntax_msg();
					return;
				}

			}
		}
	}

	newrole = 0;
	/* Verify the role name */
	if (mode != 3) { /* list roles */
		if (strncmp(rolestr, "MAI", 3) == 0)
			newrole = ROLE_MAIN;
		else if (strncmp(rolestr, "NAV", 3) == 0)
			newrole = ROLE_NAVIGATION;
		else if (strncmp(rolestr, "WEA", 3) == 0)
			newrole = ROLE_WEAPONS;
		else if (strncmp(rolestr, "ENG", 3) == 0)
			newrole = ROLE_ENGINEERING;
		else if (strncmp(rolestr, "DAM", 3) == 0)
			newrole = ROLE_DAMCON;
		else if (strncmp(rolestr, "SCI", 3) == 0)
			newrole = ROLE_SCIENCE;
		else if (strncmp(rolestr, "COM", 3) == 0)
			newrole = ROLE_COMMS;
		else if (strncmp(rolestr, "DEM", 3) == 0)
			newrole = ROLE_DEMON;
		else if (strncmp(rolestr, "TTS", 3) == 0)
			newrole = ROLE_TEXT_TO_SPEECH;
		else if (strncmp(rolestr, "SOU", 3) == 0)
			newrole = ROLE_SOUNDSERVER;
		else if (strncmp(rolestr, "ALL", 3) == 0)
			newrole = ROLE_MAIN | ROLE_NAVIGATION | ROLE_WEAPONS | ROLE_ENGINEERING | ROLE_DAMCON |
					ROLE_SCIENCE | ROLE_COMMS | ROLE_DEMON | ROLE_TEXT_TO_SPEECH |
					ROLE_SOUNDSERVER;
		else {
			send_demon_console_color_msg(YELLOW, "INVALID ROLE NAME");
			return;
		}
	}

	/* Verify the client number */
	if (c < 0) {
		send_demon_console_color_msg(YELLOW, "ROLE: INVALID CLIENT NUMBER");
		return;
	}
	client_lock();
	if (c >= nclients) {
		client_unlock();
		send_demon_console_color_msg(YELLOW, "ROLE: INVALID CLIENT NUMBER");
		return;
	}


	switch (mode) {
	case 0: /* Add a new role to a client */
		role = verify_role(client[c].role | newrole);
		client[c].role = role;
		pb_queue_to_client(&client[c], snis_opcode_subcode_pkt("bbw",
			OPCODE_CLIENT_CONFIG, OPCODE_CLIENT_SET_PERMITTED_ROLES, role));
		client_unlock();
		send_demon_console_msg("SET CLIENT %d ROLE TO %08x", c, role);
		break;
	case 1: /* Subtract a role from a client */
		role = verify_role(client[c].role & ~newrole);
		client[c].role = role;
		pb_queue_to_client(&client[c], snis_opcode_subcode_pkt("bbw",
			OPCODE_CLIENT_CONFIG, OPCODE_CLIENT_SET_PERMITTED_ROLES, role));
		client_unlock();
		send_demon_console_msg("SET CLIENT %d ROLE TO %08x", c, role);
		break;
	case 2: /* Set client to have a single role */
		role = verify_role(newrole);
		client[c].role = role;
		pb_queue_to_client(&client[c], snis_opcode_subcode_pkt("bbw",
			OPCODE_CLIENT_CONFIG, OPCODE_CLIENT_SET_PERMITTED_ROLES, role));
		client_unlock();
		send_demon_console_msg("SET CLIENT %d ROLE TO %08x", c, role);
		break;
	case 3: /* List client roles */
		role = client[c].role;
		client_unlock();
		send_demon_console_msg("CLIENT %d HAS THE FOLLOWING ROLES:", c);
		if (role & ROLE_MAIN)
			send_demon_console_msg("- MAIN VIEW");
		if (role & ROLE_NAVIGATION)
			send_demon_console_msg("- NAVIGATION");
		if (role & ROLE_WEAPONS)
			send_demon_console_msg("- WEAPONS");
		if (role & ROLE_ENGINEERING)
			send_demon_console_msg("- ENGINEERING");
		if (role & ROLE_DAMCON)
			send_demon_console_msg("- DAMAGE CONTROL");
		if (role & ROLE_COMMS)
			send_demon_console_msg("- COMMUNICATIONS");
		if (role & ROLE_SCIENCE)
			send_demon_console_msg("- SCIENCE");
		if (role & ROLE_DEMON)
			send_demon_console_msg("- DEMON");
		if (role & ROLE_TEXT_TO_SPEECH)
			send_demon_console_msg("- TEXT TO SPEECH");
		if (role & ROLE_SOUNDSERVER)
			send_demon_console_msg("- SOUND SERVER");
		break;
	default:
		client_unlock();
		send_demon_console_color_msg(YELLOW, "BUG: UNKNOWN ROLE MODE %d", mode);
		break;
	}
}

static void server_builtin_lockroles(char *cmd)
{
	int i;
	uint32_t newrole, lock, unlock;
	const uint32_t player_roles = ROLE_NAVIGATION | ROLE_MAIN | ROLE_WEAPONS |
					ROLE_ENGINEERING | ROLE_DAMCON | ROLE_COMMS | ROLE_SCIENCE;

	unlock = (strcasecmp(cmd, "UNLOCKROLES") == 0);
	lock = !unlock;

	pthread_mutex_lock(&universe_mutex);
	client_lock();
	for (i = 0; i < nclients; i++) {
		switch (client[i].current_station) {
		case DISPLAYMODE_MAINSCREEN:
			newrole = ROLE_MAIN;
			break;
		case DISPLAYMODE_NAVIGATION:
			newrole = ROLE_NAVIGATION;
			break;
		case DISPLAYMODE_WEAPONS:
			newrole = ROLE_WEAPONS;
			break;
		case DISPLAYMODE_ENGINEERING:
			newrole = ROLE_ENGINEERING | ROLE_DAMCON;
			break;
		case DISPLAYMODE_SCIENCE:
			newrole = ROLE_SCIENCE;
			break;
		case DISPLAYMODE_COMMS:
			newrole = ROLE_COMMS;
			break;
		case DISPLAYMODE_DAMCON:
			newrole = ROLE_ENGINEERING | ROLE_DAMCON;
			break;
		default:
			continue; /* No change */
		}
		newrole = verify_role(lock * (newrole | (client[i].role & ~player_roles)) +
					unlock * (player_roles | client[i].role));
		client[i].role = newrole;
		pb_queue_to_client(&client[i], snis_opcode_subcode_pkt("bbw",
				OPCODE_CLIENT_CONFIG, OPCODE_CLIENT_SET_PERMITTED_ROLES, newrole));
	}
	client_unlock();
	pthread_mutex_unlock(&universe_mutex);
	send_demon_console_msg("%s CLIENT ROLES", lock ? "LOCKED" : "UNLOCKED");
	server_builtin_clients(NULL);
}

static void server_builtin_rotateroles(char *cmd)
{
	int i;
	uint32_t newrole, lock, unlock;
	const uint32_t player_roles = ROLE_NAVIGATION | ROLE_MAIN | ROLE_WEAPONS |
					ROLE_ENGINEERING | ROLE_DAMCON | ROLE_COMMS | ROLE_SCIENCE;

	unlock = (strcasecmp(cmd, "UNLOCKROLES") == 0);
	lock = !unlock;

	pthread_mutex_lock(&universe_mutex);
	client_lock();
	for (i = 0; i < nclients; i++) {
		switch (client[i].current_station) {
		case DISPLAYMODE_NAVIGATION:
			newrole = ROLE_WEAPONS;
			break;
		case DISPLAYMODE_WEAPONS:
			newrole = ROLE_ENGINEERING | ROLE_DAMCON;
			break;
		case DISPLAYMODE_ENGINEERING:
			newrole = ROLE_SCIENCE;
			break;
		case DISPLAYMODE_SCIENCE:
			newrole = ROLE_COMMS;
			break;
		case DISPLAYMODE_COMMS:
			newrole = ROLE_NAVIGATION;
			break;
		case DISPLAYMODE_DAMCON:
			newrole = ROLE_SCIENCE;
			break;
		default:
			continue; /* No change */
		}
		newrole = verify_role(lock * (newrole | (client[i].role & ~player_roles)) +
					unlock * (player_roles | client[i].role));
		client[i].role = newrole;
		pb_queue_to_client(&client[i], snis_opcode_subcode_pkt("bbw",
				OPCODE_CLIENT_CONFIG, OPCODE_CLIENT_SET_PERMITTED_ROLES, newrole));
	}
	client_unlock();
	pthread_mutex_unlock(&universe_mutex);
	send_demon_console_msg("ROTATED CLIENT ROLES");
	server_builtin_clients(NULL);
}

static void server_builtin_help(char *cmd);

static void server_builtin_find(char *cmd)
{
	char name[100];
	int i, rc;

	rc = sscanf(cmd, "%*s %s", name);
	if (rc != 1) {
		send_demon_console_color_msg(YELLOW, "INVALID FIND COMMAND");
		return;
	}
	pthread_mutex_lock(&universe_mutex);
	for (i = 0; i <= snis_object_pool_highest_object(pool); i++)
		if (strncmp(go[i].sdata.name, name, strlen(name)) == 0)
			send_demon_console_msg("- %d %s TYPE = %d", go[i].id, go[i].sdata.name, go[i].type);
	for (i = 0; i < nbridges; i++) {
		if (strncasecmp((char *) bridgelist[i].shipname, name, strlen(name)) == 0)
			send_demon_console_msg("- %d %s - PLAYER SHIP BRIDGE %d",
				bridgelist[i].shipid, bridgelist[i].shipname, i);
	}
	pthread_mutex_unlock(&universe_mutex);
}

static void server_builtin_dump(char *cmd)
{
	pthread_mutex_lock(&universe_mutex);
	snis_debug_dump(cmd, go, nstarbase_models, docking_port_info, lookup_by_id,
			send_demon_console_msg, ship_type, nshiptypes, &ship_registry);
	pthread_mutex_unlock(&universe_mutex);
}

static void server_builtin_ai_trace(char *cmd)
{
	uint32_t id;
	int rc;

	rc = sscanf(cmd, "%*s %u", &id);
	if (rc == 1) {
		if (ai_trace_id != (uint32_t) -1)
			send_demon_console_msg("DISABLED AI TRACING FOR %u", ai_trace_id);
		ai_trace_id = id;
		if (ai_trace_id != (uint32_t) -1)
			send_demon_console_msg("ENABLED AI TRACING FOR %u", ai_trace_id);
		pthread_mutex_lock(&universe_mutex);
		rc = lookup_by_id(ai_trace_id);
		if (rc < 0) {
			pthread_mutex_unlock(&universe_mutex);
			send_demon_console_color_msg(YELLOW, "WARNING TRACING %u, OBJECT NOT FOUND", ai_trace_id);
		} else {
			if (go[rc].type != OBJTYPE_SHIP2) {
				pthread_mutex_unlock(&universe_mutex);
				send_demon_console_color_msg(YELLOW, "WARNING TRACING %u, NOT NPC SHIP", ai_trace_id);
			} else {
				pthread_mutex_unlock(&universe_mutex);
			}
		}
	} else {
		if (ai_trace_id != (uint32_t) -1) {
			send_demon_console_msg("DISABLED AI TRACING FOR %u", ai_trace_id);
			ai_trace_id = (uint32_t) -1;
		} else {
			send_demon_console_msg("USAGE: AITRACE SHIP-ID");
		}
	}
}

static void server_builtin_ai_pop(char *cmd)
{
	uint32_t id;
	int rc;
	int i;

	rc = sscanf(cmd, "%*s %u", &id);
	if (rc != 1) {
		send_demon_console_msg("AIPOP - BAD ID SPECIFIED");
		return;
	}
	pthread_mutex_lock(&universe_mutex);
	i = lookup_by_id(id);
	if (i < 0) {
		pthread_mutex_unlock(&universe_mutex);
		send_demon_console_msg("AIPOP - %d NOT FOUND", id);
		return;
	}
	if (go[i].type != OBJTYPE_SHIP2) {
		pthread_mutex_unlock(&universe_mutex);
		send_demon_console_msg("AIPOP - %d IS NOT AN NPC SHIP", id);
		return;
	}
	pop_ai_stack(&go[i]);
	send_demon_console_msg("AIPOP - POPPED AI STACK OF %d", id);
	pthread_mutex_unlock(&universe_mutex);
}

static struct server_builtin_cmd {
	char *cmd;
	char *description;
	void (*fn)(char *cmd);
} server_builtin[] = {
	{ "CLIENTS", "LIST CURRENTLY CONNECTED CLIENTS", server_builtin_clients, },
	{ "DISCONNECT", "DISCONNECT A SPECIFIED CLIENT", server_builtin_disconnect, },
	{ "SET", "SET A SERVER BUILTIN VARIABLE", server_builtin_set, },
	{ "VARS", "LIST SERVER BUILTIN VARIABLES", server_builtin_vars, },
	{ "DESCRIBE", "DESCRIBE A SERVER BUILTIN VARIABLE", server_builtin_describe, },
	{ "ROLE", "ADD, REMOVE, or LIST CLIENT ROLES", server_builtin_setrole, },
	{ "LOCKROLES", "LOCK DOWN CURRENT CLIENT ROLES", server_builtin_lockroles, },
	{ "UNLOCKROLES", "UNLOCK CLIENT ROLES", server_builtin_lockroles, },
	{ "ROTATEROLES", "UNLOCK CLIENT ROLES", server_builtin_rotateroles, },
	{ "DUMP", "DUMP STATE OF SELECTED OBJECTS", server_builtin_dump, },
	{ "DU", "DUMP STATE OF SELECTED OBJECTS", server_builtin_dump, },
	{ "FIND", "FIND AN OBJECT BY NAME", server_builtin_find, },
	{ "F", "FIND AN OBJECT BY NAME", server_builtin_find, },
	{ "AITRACE", "DEBUG TRACE NPC SHIP AI", server_builtin_ai_trace, },
	{ "AIPOP", "POP TOP ITEM OFF AI STACK", server_builtin_ai_pop, },
	{ "HELP", "LIST SERVER BUILTIN COMMANDS", server_builtin_help, },
};

static int filter_directories(const struct dirent *d)
{
	if (strcmp(d->d_name, ".") == 0)
		return 0;
	if (strcmp(d->d_name, "..") == 0)
		return 0;
	return d->d_type == DT_DIR;
}

static int filter_lua_scripts_and_dirs(const struct dirent *d)
{
	int l = strlen(d->d_name);

	/* Check for xxx.LUA */
	if (l > 4 && strcmp(&d->d_name[l - 4], ".LUA") == 0)
		return 1;

	/* Skip . and .. */
	if (strcmp(d->d_name, ".") == 0)
		return 0;
	if (strcmp(d->d_name, "..") == 0)
		return 0;

	if (d->d_type == DT_DIR)
		return 1;
	return 0;
}

/* Reads the first line of the file in luadir/subdir/filename and returns it
 * if it begins with -- (-- is a lua comment), or "" if it doesn't.
 */
static void get_lua_script_help_string(char *luadir, char *subdir, char *filename, char *helpstring, int buflen)
{
	FILE *f = NULL;
	char path[PATH_MAX], buf[256];
	char *s;

	if (buflen <= 0)
		goto out;
	snprintf(path, sizeof(path), "%s/%s/%s", luadir, subdir, filename);
	f = fopen(path, "r");
	if (!f) {
		snprintf(helpstring, buflen, "%s", strerror(errno));
		return;
	}
	s = fgets(buf, sizeof(buf), f);
	if (!s) {
		snprintf(helpstring, buflen, "%s", strerror(errno));
		goto out;
	}
	uppercase(buf);
	if (strncmp(buf, "-- ", 3) == 0) {
		snprintf(helpstring, buflen, "%s", buf);
		goto out;
	}
	strcpy(helpstring, "");
out:
	if (f)
		fclose(f);
}

static void list_lua_scripts(void)
{
	struct dirent **namelist;
	int i, j, n;
	char helpstring[256];

	n = scandir(LUASCRIPTDIR, &namelist, filter_lua_scripts_and_dirs, alphasort);
	if (n < 0) {
		send_demon_console_msg("%s: %s\n", LUASCRIPTDIR, "", strerror(errno));
		return;
	}
	send_demon_console_color_msg(WHITE, "LUA SCRIPTS:");

	for (i = 0; i < n; i++) {
		if (namelist[i]->d_type == DT_REG) { /* regular file? */
			strcpy(helpstring, "");
			get_lua_script_help_string(LUASCRIPTDIR, "", namelist[i]->d_name,
							helpstring, sizeof(helpstring));
			send_demon_console_msg("- %s %s", namelist[i]->d_name, helpstring);
		} else if (namelist[i]->d_type == DT_DIR) { /* directory? */
			struct dirent **subnamelist;
			char path[PATH_MAX];
			int sn;

			snprintf(path, sizeof(path) - 1, "%s/%s", LUASCRIPTDIR, namelist[i]->d_name);
			sn = scandir(path, &subnamelist, filter_lua_scripts_and_dirs, alphasort);
			if (sn < 0) {
				send_demon_console_msg("%s: %s\n", path, strerror(errno));
				free(namelist[i]);
				continue;
			}
			for (j = 0; j < sn; j++) {
				if (subnamelist[j]->d_type != DT_REG) {
					free(subnamelist[j]);
					continue;
				}
				strcpy(helpstring, "");
				get_lua_script_help_string(LUASCRIPTDIR, namelist[i]->d_name, subnamelist[j]->d_name,
						helpstring, sizeof(helpstring));
				send_demon_console_msg("- %s/%s %s", namelist[i]->d_name,
								subnamelist[j]->d_name, helpstring);
				free(subnamelist[j]);
			}
			free(subnamelist);
		}
		free(namelist[i]);
	}
	free(namelist);
}

static void server_builtin_help(char *cmd)
{
	int i;

	send_demon_console_color_msg(WHITE, "SERVER BUILTIN COMMANDS:");
	for (i = 0; i < ARRAYSIZE(server_builtin) - 1; i++)
		send_demon_console_msg("- %s -- %s", server_builtin[i].cmd, server_builtin[i].description);
	list_lua_scripts();
}

static int process_exec_lua_script(struct game_client *c)
{
	unsigned char buffer[sizeof(struct lua_script_packet)];
	char txt[300];
	int i, rc;
	uint8_t len;
	char firstword[300];

	rc = read_and_unpack_buffer(c, buffer, "b", &len);
	if (rc)
		return rc;
	rc = snis_readsocket(c->socket, txt, len);
	if (rc)
		return rc;
	txt[len] = '\0';
	memset(firstword, 0, sizeof(firstword));
	for (i = 0; txt[i] != '\0'; i++) {
		if (txt[i] == ' ')
			break;
		firstword[i] = txt[i];
	}

	/* See if it's a server builtin command */
	for (i = 0; i < ARRAYSIZE(server_builtin); i++) {
		if (strcmp(firstword, server_builtin[i].cmd) == 0) {
			server_builtin[i].fn(txt);
			return 0;
		}
	}

	pthread_mutex_lock(&universe_mutex);
	enqueue_lua_command(txt); /* queue up for execution by main thread. */
	pthread_mutex_unlock(&universe_mutex);
	return 0;
}

static void update_command_data(uint32_t id, struct command_data *cmd_data)
{
	struct snis_entity *o;
	int i, n;

	i = lookup_by_id(id);
	if (i < 0)
		goto out;

	o = &go[i];
	if (o->tsd.ship.nai_entries == 0)
		ship_figure_out_what_to_do(o);
	n = o->tsd.ship.nai_entries - 1;

	if (o->type != OBJTYPE_SHIP2)
		goto out;

	switch (cmd_data->command) {
		case DEMON_CMD_ATTACK:
			o->tsd.ship.cmd_data = *cmd_data;
			o->tsd.ship.ai[n].ai_mode = AI_MODE_ATTACK;
			o->tsd.ship.ai[n].u.attack.victim_id = (uint32_t) -1;
			ship_choose_new_attack_victim(o);
			break;
		default:
			goto out;	
	}

out:
	pthread_mutex_unlock(&universe_mutex);
	return;
}

static int l_ai_push_attack(lua_State *l)
{
	int i;
	double oid;
	struct snis_entity *o, *v;

	pthread_mutex_lock(&universe_mutex);
	oid = lua_tonumber(lua_state, 1);
	i = lookup_by_id(oid);
	if (i < 0)
		goto error;
	o = &go[i];
	oid = lua_tonumber(lua_state, 2);
	i = lookup_by_id(oid);
	if (i < 0)
		goto error;
	v = &go[i];

	push_attack_mode(o, v->id, 0);

	pthread_mutex_unlock(&universe_mutex);
	lua_pushnumber(l, 0.0);
	return 1;
error:
	pthread_mutex_unlock(&universe_mutex);
	lua_pushnil(l);
	return 1;
}

static int l_ai_push_catatonic(lua_State *l)
{
	int i;
	double oid;

	pthread_mutex_lock(&universe_mutex);
	oid = lua_tonumber(lua_state, 1);
	i = lookup_by_id(oid);
	if (i < 0)
		goto error;
	if (go[i].type != OBJTYPE_SHIP2)
		goto error;
	push_catatonic_mode(&go[i]);

	pthread_mutex_unlock(&universe_mutex);
	lua_pushnumber(l, 0.0);
	return 1;
error:
	pthread_mutex_unlock(&universe_mutex);
	lua_pushnil(l);
	return 1;
}

static int l_ai_push_patrol(lua_State *l)
{
	int i, n, p, np;
	double oid, x, y, z, npd;
	struct snis_entity *o;

	pthread_mutex_lock(&universe_mutex);
	oid = lua_tonumber(lua_state, 1);
	i = lookup_by_id(oid);
	if (i < 0)
		goto error;
	o = &go[i];

	npd = lua_tonumber(lua_state, 2);
	if (npd < 2.0 || npd > 5.0)
		goto error;
	np = (int) npd;

	if (o->tsd.ship.nai_entries >= MAX_AI_STACK_ENTRIES)
		goto error;
	
	n = o->tsd.ship.nai_entries;
	o->tsd.ship.ai[n].ai_mode = AI_MODE_PATROL;

	i = 3;
	for (p = 0; p < np; p++) {
		if (lua_isnoneornil(l, i))
			break;
		x = lua_tonumber(lua_state, i);
		i++;
		if (lua_isnoneornil(l, i))
			goto error;
		y = lua_tonumber(lua_state, i);
		i++;
		if (lua_isnoneornil(l, i))
			goto error;
		z = lua_tonumber(lua_state, i);
		i++;

		o->tsd.ship.ai[n].u.patrol.p[p].v.x = x; 
		o->tsd.ship.ai[n].u.patrol.p[p].v.y = y; 
		o->tsd.ship.ai[n].u.patrol.p[p].v.z = z; 

		if (p >= ARRAYSIZE(o->tsd.ship.ai[n].u.patrol.p))
			break;
	}
	o->tsd.ship.ai[n].u.patrol.npoints = p;
	o->tsd.ship.ai[n].u.patrol.oneshot = 0;
	o->tsd.ship.nai_entries++;
	set_patrol_waypoint_destination(o, 0);

	pthread_mutex_unlock(&universe_mutex);
	lua_pushnumber(l, 0.0);
	return 1;

error:
	pthread_mutex_unlock(&universe_mutex);
	lua_pushnil(l);
	return 1;
}

static double register_lua_callback(const char *event, const char *callback)
{

	register_event_callback(event, callback, &event_callback);
	printf("Registered callback %s for event %s\n", callback, event);
	return 0.0;
}

static int l_register_callback(lua_State *l)
{
	const char *event = luaL_checkstring(l, 1);
	const char *callback = luaL_checkstring(l, 2);
	double rc;

	pthread_mutex_lock(&universe_mutex);
	rc = register_lua_callback(event, callback);
	pthread_mutex_unlock(&universe_mutex);
	lua_pushnumber(l, rc);
	return 1;
}

static int l_register_proximity_callback(lua_State *l)
{
	const char *callback = luaL_checkstring(l, 1);
	const double lua_oid1 = luaL_checknumber(l, 2);
	const double lua_oid2 = luaL_checknumber(l, 3);
	const double distance = luaL_checknumber(l, 4);
	double rc;
	uint32_t oid1, oid2;

	if (lua_oid1 < lua_oid2) {
		oid1 = (uint32_t) lua_oid1;
		oid2 = (uint32_t) lua_oid2;
	} else {
		oid1 = (uint32_t) lua_oid2;
		oid2 = (uint32_t) lua_oid1;
	}
	pthread_mutex_lock(&universe_mutex);
	rc = register_lua_proximity_callback(callback, oid1, oid2, distance);
	pthread_mutex_unlock(&universe_mutex);
	lua_pushnumber(l, rc);
	return 1;
}

static int l_register_timer_callback(lua_State *l)
{
	const char *callback = luaL_checkstring(l, 1);
	const double timer_ticks = luaL_checknumber(l, 2);
	const double cookie_value = luaL_checknumber(l, 3);
	int rc;

	pthread_mutex_lock(&universe_mutex);
	rc = register_lua_timer_callback(callback, timer_ticks, cookie_value);
	pthread_mutex_unlock(&universe_mutex);
	lua_pushnumber(l, rc);
	return 1;
}

static int l_comms_transmission(lua_State *l)
{
	int i;
	const double transmitter_id = luaL_checknumber(l, 1);
	const char *transmission = luaL_checkstring(l, 2);
	struct snis_entity *transmitter;

	pthread_mutex_lock(&universe_mutex);
	i = lookup_by_id(transmitter_id);
	if (i < 0)
		goto error;
	transmitter = &go[i];
	send_comms_packet(transmitter, transmitter->sdata.name, 0, "%s", transmission);
error:
	pthread_mutex_unlock(&universe_mutex);
	return 0;
}

static void send_comms_enciphered_packet(struct snis_entity *transmitter, char *sender,
						uint32_t channel, const char *format, ...);
static int l_comms_enciphered_transmission(lua_State *l)
{
	int i;
	const double transmitter_id = luaL_checknumber(l, 1);
	const char *transmission = luaL_checkstring(l, 2);
	const char *cipher_key = luaL_checkstring(l, 3);
	char *ck, *tr;
	struct snis_entity *transmitter;
	struct scipher_key *key;
	char *ciphertext;

	pthread_mutex_lock(&universe_mutex);
	i = lookup_by_id(transmitter_id);
	if (i < 0)
		goto error;
	transmitter = &go[i];
	if (strlen(cipher_key) != 26)
		goto error;
	ck = strdup(cipher_key);
	key = scipher_make_key(ck);
	ciphertext = strdup(transmission);
	tr = strdup(transmission);
	scipher_encipher(tr, ciphertext, strlen(transmission) + 1, key);
	free(tr);
	send_comms_enciphered_packet(transmitter, transmitter->sdata.name, 0, "%s", ciphertext);
	/* We also need to update bridgelist[x].enciphered_message and key for all x */
	for (i = 0; i < nbridges; i++) {
		snprintf(bridgelist[i].enciphered_message, sizeof(bridgelist[i].enciphered_message), "%s",
				ciphertext);
		memcpy(bridgelist[i].cipher_key, ck, 26);
	}
	free(ck);
	scipher_key_free(key);
error:
	pthread_mutex_unlock(&universe_mutex);
	return 0;
}

static int l_generate_cipher_key(lua_State *l)
{
	struct scipher_key *k;
	char keystring[27];

	k = scipher_make_key(NULL);
	scipher_key_to_string(k, keystring);
	keystring[26] = '\0';
	lua_pushstring(l, keystring);
	return 1;
}

static int l_get_faction_name(lua_State *l)
{
	int f = luaL_checknumber(l, 1);

	if (f < 0 || f >= nfactions()) {
		lua_pushnil(l);
		return 1;
	}
	lua_pushstring(l, faction_name(f));
	return 1;
}

static int l_comms_channel_transmit(lua_State *l)
{
	const char *name = luaL_checkstring(l, 1);
	const double channel = luaL_checknumber(l, 2);
	const char *transmission = luaL_checkstring(l, 3);
	uint32_t ch;

	ch = (uint32_t) channel;
	/* TODO: should probably have transmitter object here. */
	send_comms_packet(NULL, (char *) name, ch, "%s", transmission);
	return 0;
}

static int l_comms_channel_unlisten(lua_State *l)
{
	int i, n;
	const char *name = luaL_checkstring(l, 1);
	const double channel = luaL_checknumber(l, 2);

	n = -1;
	pthread_mutex_lock(&universe_mutex);
	for (i = 0; i < nlua_channels; i++) {
		if (strcasecmp(lua_channel[i].name, name) == 0 &&
			lua_channel[i].channel == (uint32_t) channel) {
			n = i;
			break;
		}
	}
	if (n >= 0) {
		free(lua_channel[n].name);
		free(lua_channel[n].callback);
		for (i = n; i < nlua_channels - 1; i++)
			lua_channel[i] = lua_channel[i + 1];
		nlua_channels--;
	}
	pthread_mutex_unlock(&universe_mutex);
	return 0;
}

/* Listen on a comms channel and forward traffic to lua callback */
static int l_comms_channel_listen(lua_State *l)
{
	const char *name = luaL_checkstring(l, 1);
	const double channel = luaL_checknumber(l, 2);
	const char *callback = luaL_checkstring(l, 3);

	pthread_mutex_lock(&universe_mutex);
	if (nlua_channels >= MAX_LUA_CHANNELS) {
		pthread_mutex_unlock(&universe_mutex);
		return 0;
	}
	lua_channel[nlua_channels].name = strdup(name);
	lua_channel[nlua_channels].channel = (uint32_t) channel;
	lua_channel[nlua_channels].callback = strdup(callback);
	nlua_channels++;
	pthread_mutex_unlock(&universe_mutex);
	return 0;
}

static int l_text_to_speech(lua_State *l)
{
	int i;
	const double receiver_id = luaL_checknumber(l, 1);
	const char *transmission = luaL_checkstring(l, 2);
	struct snis_entity *receiver;

	pthread_mutex_lock(&universe_mutex);
	i = lookup_by_id(receiver_id);
	if (i < 0)
		goto error;
	receiver = &go[i];
	switch (receiver->type) {
	case OBJTYPE_SHIP1:
		pthread_mutex_unlock(&universe_mutex);
		snis_queue_add_text_to_speech(transmission, ROLE_TEXT_TO_SPEECH, receiver->id);
		return 0;
	default:
		goto error;
		break;
	}
error:
	pthread_mutex_unlock(&universe_mutex);
	return 0;
}

static int l_get_object_name(lua_State *l)
{
	const double id = luaL_checknumber(l, 1);
	struct snis_entity *o;
	int i;

	pthread_mutex_lock(&universe_mutex);
	i = lookup_by_id(id);
	if (i < 0) {
		lua_pushstring(l, "unknown");
		goto error;
	}
	o = &go[i];
	switch (o->type) {
	case OBJTYPE_STARBASE:
		lua_pushstring(l, o->sdata.name);
		break;
	default:
		lua_pushstring(l, o->sdata.name);
		break;
	}
error:
	pthread_mutex_unlock(&universe_mutex);
	return 1;
}

static int l_set_faction(lua_State *l)
{
	const double id = luaL_checknumber(l, 1);
	const double faction = luaL_checknumber(l, 2);

	int i, f;

	f = ((int) faction) % nfactions();
	i = lookup_by_id((int) id);
	if (i < 0)
		goto error;
	go[i].sdata.faction = f;
	if (go[i].type == OBJTYPE_SHIP2) /* clear ship ai stack */
		go[i].tsd.ship.nai_entries = 0;
	go[i].timestamp = universe_timestamp;
	lua_pushnumber(l, 0.0);
	return 1;
error:
	lua_pushnil(l);
	return 1;
}

static int l_set_player_damage(lua_State *l)
{
	const double id = luaL_checknumber(l, 1);
	const char *system = luaL_checkstring(l, 2);
	const double value = luaL_checknumber(l, 3);
	double f[DAMCON_PARTS_PER_SYSTEM];
	double ftotal;
	uint32_t oid = (uint32_t) id;
	uint8_t bvalue;
	int i, b, damage_delta;
	struct snis_entity *o;
	int system_number;

	ftotal = 0.0;
	for (i = 0; i < DAMCON_PARTS_PER_SYSTEM; i++) {
		f[i] = luaL_optnumber(l, i + 4, 0.0);
		ftotal += f[i];
	}

	i = lookup_by_id(oid);
	if (i < 0)
		goto error;
	o = &go[i];
	if (o->type != OBJTYPE_SHIP1)
		goto error;
	if (value < 0 || value > 255)
		goto error;
	b = lookup_bridge_by_shipid(o->id);
	bvalue = (uint8_t) value;
	if (strncmp(system, "shield", 6) == 0) {
		damage_delta =
			(int) bvalue - (int) o->tsd.ship.damage.shield_damage;
		o->tsd.ship.damage.shield_damage = bvalue;
		system_number = DAMCON_TYPE_SHIELDSYSTEM;
		goto distribute_damage;
	}
	if (strncmp(system, "impulse", 7) == 0) {
		damage_delta =
			(int) bvalue - (int) o->tsd.ship.damage.impulse_damage;
		o->tsd.ship.damage.impulse_damage = bvalue;
		system_number = DAMCON_TYPE_IMPULSE;
		goto distribute_damage;
	}
	if (strncmp(system, "warp", 4) == 0) {
		damage_delta =
			(int) bvalue - (int) o->tsd.ship.damage.warp_damage;
		o->tsd.ship.damage.warp_damage = bvalue;
		system_number = DAMCON_TYPE_WARPDRIVE;
		goto distribute_damage;
	}
	if (strncmp(system, "maneuvering", 7) == 0) {
		damage_delta =
			(int) bvalue - (int) o->tsd.ship.damage.maneuvering_damage;
		o->tsd.ship.damage.maneuvering_damage = bvalue;
		system_number = DAMCON_TYPE_MANEUVERING;
		goto distribute_damage;
	}
	if (strncmp(system, "phaser", 6) == 0) {
		damage_delta =
			(int) bvalue - (int) o->tsd.ship.damage.phaser_banks_damage;
		o->tsd.ship.damage.phaser_banks_damage = bvalue;
		system_number = DAMCON_TYPE_PHASERBANK;
		goto distribute_damage;
	}
	if (strncmp(system, "sensor", 6) == 0) {
		damage_delta =
			(int) bvalue - (int) o->tsd.ship.damage.sensors_damage;
		o->tsd.ship.damage.sensors_damage = bvalue;
		system_number = DAMCON_TYPE_SENSORARRAY;
		goto distribute_damage;
	}
	if (strncmp(system, "comms", 5) == 0) {
		damage_delta =
			(int) bvalue - (int) o->tsd.ship.damage.comms_damage;
		o->tsd.ship.damage.comms_damage = bvalue;
		system_number = DAMCON_TYPE_COMMUNICATIONS;
		goto distribute_damage;
	}
	if (strncmp(system, "tractor", 7) == 0) {
		damage_delta =
			(int) bvalue - (int) o->tsd.ship.damage.tractor_damage;
		o->tsd.ship.damage.tractor_damage = bvalue;
		system_number = DAMCON_TYPE_TRACTORSYSTEM;
		goto distribute_damage;
	}
	if (strncmp(system, "lifesupport", 11) == 0) {
		damage_delta =
			(int) bvalue - (int) o->tsd.ship.damage.lifesupport_damage;
		o->tsd.ship.damage.lifesupport_damage = bvalue;
		system_number = DAMCON_TYPE_LIFESUPPORTSYSTEM;
		goto distribute_damage;
	}
error:
	lua_pushnil(l);
	return 1;

distribute_damage:

	assert(b >= 0 && b < nbridges);
	if (fabs(ftotal) < 0.0001)
		distribute_damage_to_damcon_system_parts(o, &bridgelist[b].damcon,
				damage_delta, system_number);
	else
		distribute_damage_to_damcon_system_parts_fractionally(o, &bridgelist[b].damcon,
				damage_delta, system_number, f);
	lua_pushnumber(l, 0.0);
	return 1;
}

static int l_load_skybox(lua_State *l)
{
	const double id = luaL_checknumber(l, 1);
	const char *fileprefix = luaL_checkstring(l, 2);
	struct snis_entity *o;
	struct packed_buffer *pb;
	int i;

	i = lookup_by_id(id);
	if (i < 0)
		goto error;
	o = &go[i];
	if (o->type != OBJTYPE_SHIP1)
		goto error;
	if (strlen(fileprefix) > 100)
		goto error;
	pb = packed_buffer_allocate(3 + 100);
	packed_buffer_append(pb, "bb", OPCODE_LOAD_SKYBOX, (uint8_t) strlen(fileprefix) + 1);
	packed_buffer_append_raw(pb, fileprefix, strlen(fileprefix) + 1);
	send_packet_to_all_clients_on_a_bridge(o->id, pb, ROLE_MAIN);
	lua_pushnumber(l, 0.0);
	return 1;
error:
	lua_pushnil(l);
	return 1;
}

static int l_user_coords(lua_State *l)
{
	const double x = luaL_checknumber(l, 1);
	const double y = luaL_checknumber(l, 2);
	const double z = luaL_checknumber(l, 3);

	lua_pushnumber(l, x);
	lua_pushnumber(l, z);
	lua_pushnumber(l, y);
	return 3;
}

static int l_get_player_damage(lua_State *l)
{
	const double id = luaL_checknumber(l, 1);
	const char *system = luaL_checkstring(l, 2);
	uint32_t oid = (uint32_t) id;
	uint8_t bvalue;
	int i;
	struct snis_entity *o;

	i = lookup_by_id(oid);
	if (i < 0)
		goto error;
	o = &go[i];
	if (o->type != OBJTYPE_SHIP1)
		goto error;
	if (strncmp(system, "shields", 6) == 0) {
		bvalue = o->tsd.ship.damage.shield_damage;
		goto done;
	}
	if (strncmp(system, "impulse", 7) == 0) {
		bvalue = o->tsd.ship.damage.impulse_damage;
		goto done;
	}
	if (strncmp(system, "warp", 4) == 0) {
		bvalue = o->tsd.ship.damage.warp_damage;
		goto done;
	}
	if (strncmp(system, "maneuvering", 7) == 0) {
		bvalue = o->tsd.ship.damage.maneuvering_damage;
		goto done;
	}
	if (strncmp(system, "phaser", 6) == 0) {
		bvalue = o->tsd.ship.damage.phaser_banks_damage;
		goto done;
	}
	if (strncmp(system, "sensors", 6) == 0) {
		bvalue = o->tsd.ship.damage.sensors_damage;
		goto done;
	}
	if (strncmp(system, "comms", 5) == 0) {
		bvalue = o->tsd.ship.damage.comms_damage;
		goto done;
	}
	if (strncmp(system, "tractor", 7) == 0) {
		bvalue = o->tsd.ship.damage.tractor_damage;
		goto done;
	}
	if (strncmp(system, "lifesupport", 7) == 0) {
		bvalue = o->tsd.ship.damage.lifesupport_damage;
		goto done;
	}
error:
	lua_pushnil(l);
	return 1;
done:
	lua_pushnumber(l, (double) bvalue);
	return 1;
}

static void do_robot_drop(struct damcon_data *d)
{
	int i, c, found_socket = -1;
	struct snis_damcon_entity *cargo;
	double dist2, mindist = -1;

	c = lookup_by_damcon_id(d, d->robot->tsd.robot.cargo_id);
	if (c >= 0) {
		cargo = &d->o[c];
		d->robot->tsd.robot.cargo_id = ROBOT_CARGO_EMPTY;
		cargo->version++;
		d->robot->version++;

		/* find nearest socket... */
		found_socket = -1;
		for (i = 0; i <= snis_object_pool_highest_object(d->pool); i++) {
			if (d->o[i].type != DAMCON_TYPE_SOCKET)
				continue;
			if (d->o[i].tsd.socket.contents_id != DAMCON_SOCKET_EMPTY)
				continue;
			dist2 = (cargo->x - d->o[i].x) * (cargo->x - d->o[i].x) + 
				(cargo->y - d->o[i].y) * (cargo->y - d->o[i].y);

			if (mindist < 0 || mindist > dist2) {
				mindist = dist2;
				if (dist2 < (ROBOT_MAX_GRIP_DIST2))
					found_socket = i;
			}
		}

		if (found_socket >= 0) {
			cargo->x = d->o[found_socket].x;
			cargo->y = d->o[found_socket].y;
			cargo->heading = 0;
			d->o[found_socket].tsd.socket.contents_id = cargo->id;
			d->o[found_socket].version++;
			snis_queue_add_sound(ROBOT_INSERT_COMPONENT,
				ROLE_SOUNDSERVER | ROLE_DAMCON, bridgelist[d->bridge].shipid);
		}
	}
}

static void do_robot_pickup(struct damcon_data *d)
{
	int i;
	struct snis_damcon_entity *item, *socket;
	double mindist = -1.0;
	int found = -1;
	double clawx, clawy;

	clawx = robot_clawx(d->robot);
	clawy = robot_clawy(d->robot);
	for (i = 0; i <= snis_object_pool_highest_object(d->pool); i++) {
		int dist2;

		item = &d->o[i];
		if (item->type != DAMCON_TYPE_PART)
			continue;
		dist2 = (item->x - clawx) *  (item->x - clawx) +
			(item->y - clawy) *  (item->y - clawy);

		if (mindist < 0 || mindist > dist2) {
			mindist = dist2;
			if (dist2 < ROBOT_MAX_GRIP_DIST2)
				found = i;
		}
	}
	if (found < 0)
		return;

	item = &d->o[found];
	d->robot->tsd.robot.cargo_id = item->id;
	d->robot->version++;
	item->x = clawx;
	item->y = clawy;
	item->version++;

	/* See if any socket thinks it has this item, if so, remove it. */
	for (i = 0; i <= snis_object_pool_highest_object(d->pool); i++) {
		socket = &d->o[i];
		if (socket->type != DAMCON_TYPE_SOCKET)
			continue;
		if (socket->tsd.socket.contents_id == item->id) {
			socket->tsd.socket.contents_id = DAMCON_SOCKET_EMPTY;
			socket->version++;
			snis_queue_add_sound(ROBOT_REMOVE_COMPONENT,
				ROLE_SOUNDSERVER | ROLE_DAMCON, bridgelist[d->bridge].shipid);
			break;
		}
	}
}

static int process_request_robot_gripper(struct game_client *c)
{
	/* no data to read, opcode only */
	struct damcon_data *d = &bridgelist[c->bridge].damcon;

	pthread_mutex_lock(&universe_mutex);
	if (d->robot->tsd.robot.cargo_id == ROBOT_CARGO_EMPTY)
		do_robot_pickup(d);
	else
		do_robot_drop(d);
	pthread_mutex_unlock(&universe_mutex);
	return 0;
}

static int process_mainscreen_view_mode(struct game_client *c)
{
	int rc;
	unsigned char buffer[sizeof(struct request_mainscreen_view_change) -
					sizeof(uint8_t)];
	double view_angle;
	uint8_t view_mode;

	rc = read_and_unpack_buffer(c, buffer, "Rb", &view_angle, &view_mode);
	if (rc)
		return rc;
	/* Rebuild packet and send to all clients with main screen, nav or weapons roles */
	send_packet_to_all_clients_on_a_bridge(c->shipid, 
			snis_opcode_pkt("bRb", OPCODE_MAINSCREEN_VIEW_MODE,
					view_angle, view_mode),
			ROLE_MAIN | ROLE_WEAPONS | ROLE_NAVIGATION);
	return 0;
}

static void set_red_alert_mode(struct game_client *c, unsigned char new_alert_mode)
{
	send_packet_to_all_clients_on_a_bridge(c->shipid,
			snis_opcode_pkt("bb", OPCODE_REQUEST_REDALERT, new_alert_mode), ROLE_ALL);
	schedule_callback2(event_callback, &callback_schedule, "player-red-alert-status-event",
				(double) c->shipid, (double) !!new_alert_mode);
}

static int process_request_redalert(struct game_client *c)
{
	int rc;
	unsigned char buffer[10];
	unsigned char new_alert_mode;

	rc = read_and_unpack_buffer(c, buffer, "b", &new_alert_mode);
	if (rc)
		return rc;
	set_red_alert_mode(c, new_alert_mode);
	return 0;
}

static int process_comms_mainscreen(struct game_client *c)
{
	int rc;
	unsigned char buffer[10];
	unsigned char new_comms_mainscreen;

	rc = read_and_unpack_buffer(c, buffer, "b", &new_comms_mainscreen);
	if (rc)
		return rc;
	send_packet_to_all_clients_on_a_bridge(c->shipid,
			snis_opcode_pkt("bb", OPCODE_COMMS_MAINSCREEN,
						new_comms_mainscreen), ROLE_ALL);
	return 0;
}

static int process_demon_command(struct game_client *c)
{
	unsigned char buffer[sizeof(struct demon_cmd_packet) + 255 * sizeof(uint32_t)];
	struct packed_buffer pb;
	int i, rc;
	uint32_t ix, iz;
	int offset;
	struct command_data cmd_data;
	uint32_t id1[255];
	uint32_t id2[255];
	uint8_t nids1, nids2;

	offset = offsetof(struct demon_cmd_packet, id) - sizeof(uint8_t);

	rc = snis_readsocket(c->socket, buffer,
		sizeof(struct demon_cmd_packet) - sizeof(uint32_t) - sizeof(uint8_t));
	if (rc)
		return rc;
	packed_buffer_init(&pb, buffer, sizeof(buffer));
	packed_buffer_extract(&pb, "bwwbb", &cmd_data.command, &ix, &iz, &nids1, &nids2);
	rc = snis_readsocket(c->socket, buffer + offset, (nids1 + nids2) * sizeof(uint32_t));
	if (rc)
		return rc;

	for (i = 0; i < nids1; i++)
		packed_buffer_extract(&pb, "w", &id1[i]);
	for (i = 0; i < nids2; i++)
		packed_buffer_extract(&pb, "w", &id2[i]);

	printf("Demon command received, opcode = %02x\n", cmd_data.command);
	printf("  x = %08x, z = %08x\n", ix, iz);
	printf("Group 1 = \n");
	for (i = 0; i < nids1; i++) {
		printf("%d ", id1[i]);
		cmd_data.id[i] = id1[i];
	}
	printf("\n");
	printf("Group 2 = \n");
	for (i = 0; i < nids2; i++) {
		printf("%d ", id2[i]);
		cmd_data.id[nids1 + i] = id2[i];
	}
	printf("\n\n");

	cmd_data.x = ix;
	cmd_data.z = iz;
	cmd_data.nids1 = nids1;
	cmd_data.nids2 = nids2;

	for (i = 0; i < nids1; i++)
		update_command_data(id1[i], &cmd_data);
	
	return 0;
}

static void process_demon_clear_all(void)
{
	int i;

	pthread_mutex_lock(&universe_mutex);
	free_event_callbacks(&event_callback);
	free_timer_events(&lua_timer);
	free_lua_proximity_checks(&lua_proximity_check);
	free_callback_schedule(&callback_schedule);
	for (i = 0; i <= snis_object_pool_highest_object(pool); i++) {
		struct snis_entity *o = &go[i];

		if (o->type != OBJTYPE_SHIP1 && snis_object_pool_is_allocated(pool, i) && o->id != (uint32_t) -1)
			delete_from_clients_and_server(o);
	}
	rts_mode = 0;
	send_demon_console_msg("CLEAR ALL COMPLETE");
	pthread_mutex_unlock(&universe_mutex);
}

static void reset_player_ship(struct snis_entity *o)
{
	int b;

	init_player(o, 1, NULL);
	b = lookup_bridge_by_shipid(o->id);
	if (b >= 0)
		clear_bridge_waypoints(b);
	o->timestamp = universe_timestamp;
}

static void initialize_rts_ai()
{
	rts_ai.active = 0;
	rts_ai.wallet = INITIAL_RTS_WALLET_MONEY;
	rts_ai.faction = 255;
	rts_ai_build_queue_size = 0;
}

static void setup_rtsmode_battlefield(void)
{
	int i, j, rc;
	float x, y, z;
	int human_teams = 0;
	uint8_t computer_faction = 255;

	process_demon_clear_all(); /* Clear the universe */
	pthread_mutex_lock(&universe_mutex);

	rts_mode = 1;

	/* Reset all the human controlled ships to good state */
	for (i = 0; i <= snis_object_pool_highest_object(pool); i++) {
		struct snis_entity *o = &go[i];

		if (o->type == OBJTYPE_SHIP1)
			reset_player_ship(o);
	}
	for (i = 0; i < ARRAYSIZE(rts_planet); i++) { /* Add the main planets */
		union vec3 *p = &rts_main_planet_pos[0];
		struct snis_entity *planet;
		rts_planet[i].index = add_planet(p[i].v.x, p[i].v.y, p[i].v.z, MAX_PLANET_RADIUS * 0.85, 0, -1);
		planet = &go[rts_planet[i].index];
		fprintf(stderr, "Added main planet %d, index = %d\n", i, rts_planet[i].index);
		fprintf(stderr, "Position = %fm %fm %f\n", planet->x, planet->y, planet->z);
		rts_planet[i].health = MAX_RTS_MAIN_PLANET_HEALTH;
		planet->sdata.faction = (i % 2) + 1;
	}
	/* Place each human controlled ship near their home planets */
	for (i = 0; i <= snis_object_pool_highest_object(pool); i++) {
		struct snis_entity *o = &go[i];
		int side = i % ARRAYSIZE(rts_planet);
		union vec3 *p = &rts_main_planet_pos[side];
		if (o->type != OBJTYPE_SHIP1)
			continue;
		random_point_on_sphere(MAX_PLANET_RADIUS * 1.2, &x, &y, &z);
		set_object_location(o, x + p->v.x, y + p->v.y, z + p->v.z);
		o->sdata.faction = side + 1;
		human_teams++;
		computer_faction = ((side + 1) % ARRAYSIZE(rts_planet)) + 1;
	}
	/* Create a number of bases in a sphere around the main star */
	for (i = 0; i < NUM_RTS_BASES; i++) {
		random_point_on_sphere(XKNOWN_DIM * 0.35, &x, &y, &z);
		x += XKNOWN_DIM / 2;
		/* y doesn't need adjusting */
		z += ZKNOWN_DIM / 2;
		rc = add_starbase(x, y, z, 0, 0, 0, i, (uint32_t) -1);
		if (rc < 0)
			continue;
		if (i % 2) {
			/* Put a small planet near the starbase */
			random_point_on_sphere(MAX_PLANET_RADIUS * 1.1, &x, &y, &z);
			add_planet(go[rc].x + x, go[rc].y + y, go[rc].z + z, MAX_PLANET_RADIUS * 0.6, 0, -1);
			go[rc].sdata.faction = 0; /* neutral */
		} else {
			/* Put some asteroids around the starbase */
			for (j = 0; j < 20; j++) {
				random_point_on_sphere(MAX_PLANET_RADIUS * 0.75, &x, &y, &z);
				add_asteroid(x + go[rc].x, y + go[rc].y, z + go[rc].z, 0, 0, 0, 0);
				go[rc].sdata.faction = 0;
			}
		}
	}
	add_nebulae();

	/* Turn on the RTS ai if there is only one human team */
	if (human_teams == 1) {
		rts_ai.faction = computer_faction;
		rts_ai.active = 1;
		for (i = 0; i < ARRAYSIZE(rts_planet); i++) {
			if (go[rts_planet[i].index].sdata.faction == computer_faction)
				rts_ai.main_base_id = go[rts_planet[i].index].id;
		}
	}
	pthread_mutex_unlock(&universe_mutex);
}

static void enable_rts_mode(void)
{
	initialize_rts_ai();
	send_packet_to_all_clients(snis_opcode_pkt("bb",
			OPCODE_DEMON_RTSMODE, OPCODE_RTSMODE_SUBCMD_ENABLE), ROLE_ALL);
	setup_rtsmode_battlefield();
	pthread_mutex_lock(&universe_mutex);
	rts_mode = 1;
	pthread_mutex_unlock(&universe_mutex);
	snis_queue_add_global_text_to_speech("Real time strategy mode is now enabled.");
}

static void disable_rts_mode(void)
{
	int orig_rts_mode = rts_mode;
	initialize_rts_ai();
	rts_ai.active = 0;
	send_packet_to_all_clients(snis_opcode_pkt("bb",
			OPCODE_DEMON_RTSMODE, OPCODE_RTSMODE_SUBCMD_DISABLE), ROLE_ALL);
	process_demon_clear_all(); /* Clear the universe */
	pthread_mutex_lock(&universe_mutex);
	rts_mode = 0;
	rts_finish_timer = -1;
	if (orig_rts_mode)
		snis_queue_add_global_text_to_speech("Real time strategy mode has been disabled.");
	pthread_mutex_unlock(&universe_mutex);
}

static int process_demon_rtsmode(struct game_client *c)
{
	int rc;
	uint8_t subcode;
	unsigned char buffer[20];

	rc = read_and_unpack_buffer(c, buffer, "b", &subcode);
	if (rc)
		return rc;
	switch (subcode) {
	case OPCODE_RTSMODE_SUBCMD_ENABLE:
		enable_rts_mode();
		break;
	case OPCODE_RTSMODE_SUBCMD_DISABLE:
		disable_rts_mode();
		break;
	default:
		return -1;
	}
	return 0;
}

static int process_rts_func_comms_button(struct game_client *c)
{
	int rc;
	unsigned char buffer[20];
	uint8_t rts_button_number;

	rc = read_and_unpack_buffer(c, buffer, "b", &rts_button_number);
	if (rc)
		return rc;
	pthread_mutex_lock(&universe_mutex);
	int ship = lookup_by_id(bridgelist[c->bridge].shipid);
	if (go[ship].tsd.ship.rts_active_button != rts_button_number)
		go[ship].tsd.ship.rts_active_button = rts_button_number;
	else
		go[ship].tsd.ship.rts_active_button = 255; /* deactivate on double selection */
	pthread_mutex_unlock(&universe_mutex);
	return 0;
}

static int process_rts_func_build_unit(struct game_client *c)
{
	int rc, index;
	unsigned char buffer[20];
	char speech[100];
	uint32_t builder_id;
	uint8_t unit_type;
	struct snis_entity *ship, *builder;

	rc = read_and_unpack_buffer(c, buffer, "wb", &builder_id, &unit_type);
	if (rc)
		return rc;
	if (!rts_mode)
		return 0;
	if (unit_type >= NUM_RTS_UNIT_TYPES)
		return 0;
	pthread_mutex_lock(&universe_mutex);
	index = lookup_by_id(bridgelist[c->bridge].shipid);
	if (index == (uint32_t) -1)
		goto out;
	ship = &go[index];
	index = lookup_by_id(builder_id);
	if (index == (uint32_t) -1)
		goto out;
	builder = &go[index];

	if (rts_unit_type(unit_type)->cost_to_build > ship->tsd.ship.wallet) {
		pthread_mutex_unlock(&universe_mutex);
		snis_queue_add_text_to_speech("Insufficient funds.", ROLE_TEXT_TO_SPEECH, c->shipid);
		return 0;
	}
	if (ship->sdata.faction != builder->sdata.faction) {
		snprintf(speech, sizeof(speech), "You do not control %s", builder->sdata.name);
		pthread_mutex_unlock(&universe_mutex);
		snis_queue_add_text_to_speech(speech, ROLE_TEXT_TO_SPEECH, c->shipid);
		return 0;
	}
	if (builder->type == OBJTYPE_STARBASE) {
		if (builder->tsd.starbase.time_left_to_build != 0) {
			pthread_mutex_unlock(&universe_mutex);
			snprintf(speech, sizeof(speech), "%s already building %s.", builder->sdata.name,
				rts_unit_type(builder->tsd.planet.build_unit_type)->name);
			snis_queue_add_text_to_speech(speech, ROLE_TEXT_TO_SPEECH, c->shipid);
			return 0;
		} else {
			ship->tsd.ship.wallet -= rts_unit_type(unit_type)->cost_to_build;
			builder->tsd.starbase.time_left_to_build = rts_unit_type(unit_type)->time_to_build;
			builder->tsd.starbase.build_unit_type = unit_type;
			pthread_mutex_unlock(&universe_mutex);
			return 0;
		}
	} else if (builder->type == OBJTYPE_PLANET) {
		if (builder->tsd.planet.time_left_to_build != 0) {
			pthread_mutex_unlock(&universe_mutex);
			snprintf(speech, sizeof(speech), "Home planet already building %s.",
				rts_unit_type(builder->tsd.planet.build_unit_type)->name);
			snis_queue_add_text_to_speech(speech, ROLE_TEXT_TO_SPEECH, c->shipid);
			return 0;
		} else {
			ship->tsd.ship.wallet -= rts_unit_type(unit_type)->cost_to_build;
			builder->tsd.planet.time_left_to_build = rts_unit_type(unit_type)->time_to_build;
			builder->tsd.planet.build_unit_type = unit_type;
			pthread_mutex_unlock(&universe_mutex);
			return 0;
		}
	}
out:
	pthread_mutex_unlock(&universe_mutex);
	return 0;
}

static int process_rts_func_command_unit(struct game_client *c)
{
	unsigned char buffer[20];
	uint32_t ship_id, direct_object;
	uint8_t command;
	struct snis_entity *ship;
	int b, rc, index;
	float cost;

	rc = read_and_unpack_buffer(c, buffer, "wbw", &ship_id, &command, &direct_object);
	if (rc)
		return rc;
	pthread_mutex_lock(&universe_mutex);
	index = lookup_by_id(ship_id);
	if (index < 0)
		goto out;
	ship = &go[index];
	if (!ship->alive)
		goto out;
	if (ship->type != OBJTYPE_SHIP2)
		goto out;
	if (ship->sdata.faction != go[c->ship_index].sdata.faction)
		goto out;

	if (!orders_valid_for_unit_type((int) command - AI_MODE_RTS_FIRST_COMMAND,
					ship_type[ship->tsd.ship.shiptype].rts_unit_type))
		goto out;

	switch ((int) command) {
	case AI_MODE_RTS_STANDBY:
		goto common;
	case AI_MODE_RTS_GUARD_BASE:
		ship->tsd.ship.ai[0].u.guard_base.base_id = (uint32_t) -1;
		goto common;
	case AI_MODE_RTS_ESCORT:
		goto common;
	case AI_MODE_RTS_ATK_NEAR_ENEMY:
		ship->tsd.ship.ai[0].u.atk_near_enemy.enemy_id = (uint32_t) -1;
		goto common;
	case AI_MODE_RTS_RESUPPLY:
		ship->tsd.ship.ai[0].u.resupply.unit_to_resupply = (uint32_t) -1;
		goto common;
	case AI_MODE_RTS_MOVE_TO_WAYPOINT:
		b = c->bridge;
		if (b < 0 && b >= nclients)
			return 0;
		if (bridgelist[b].selected_waypoint < 0 ||
			bridgelist[b].selected_waypoint >= bridgelist[b].nwaypoints)
			return 0;
		ship->tsd.ship.ai[0].u.goto_waypoint.waypoint = bridgelist[b].selected_waypoint;
		ship->tsd.ship.ai[0].u.goto_waypoint.bridge_ship_id = bridgelist[b].shipid;
		goto common;
	case AI_MODE_RTS_OCCUPY_NEAR_BASE:
		ship->tsd.ship.ai[0].u.occupy_base.base_id = (uint32_t) -1;
		goto common;
	case AI_MODE_RTS_ATK_MAIN_BASE:
		ship->tsd.ship.ai[0].u.atk_main_base.base_id = (uint32_t) -1;
		goto common;
common:
		if (ship->tsd.ship.ai[0].ai_mode == command) /* Already set to this order? */
			goto out;
		cost = rts_order_type(command - AI_MODE_RTS_FIRST_COMMAND)->cost_to_order;
		if (cost > go[c->ship_index].tsd.ship.wallet) {
			pthread_mutex_unlock(&universe_mutex);
			snis_queue_add_text_to_speech("Insufficient funds.", ROLE_TEXT_TO_SPEECH, c->shipid);
			return 0;
		}
		go[c->ship_index].tsd.ship.wallet -= cost;
		ship->tsd.ship.ai[0].ai_mode = command;
		ship->tsd.ship.nai_entries = 1;
		ship->timestamp = universe_timestamp;
		break;
	default:
		break;
	}
out:
	pthread_mutex_unlock(&universe_mutex);
	return 0;
}

static int process_rts_func(struct game_client *c)
{
	int rc;
	unsigned char subcode;
	unsigned char buffer[10];

	rc = read_and_unpack_buffer(c, buffer, "b", &subcode);
	if (rc)
		return rc;
	switch (subcode) {
	case OPCODE_RTS_FUNC_COMMS_BUTTON:
		rc = process_rts_func_comms_button(c);
		if (rc)
			return rc;
		break;
	case OPCODE_RTS_FUNC_BUILD_UNIT:
		rc = process_rts_func_build_unit(c);
		if (rc)
			return rc;
		break;
	case OPCODE_RTS_FUNC_COMMAND_UNIT:
		rc = process_rts_func_command_unit(c);
		if (rc)
			return rc;
		break;
	default:
		return -1;
	}
	return 0;
}

static int process_custom_button(struct game_client *c)
{
	int rc;
	uint8_t subcode;
	unsigned char buffer[16];

	rc = read_and_unpack_buffer(c, buffer, "b", &subcode);
	if (rc)
		return rc;
	if (subcode > 5) /* must be 0 - 5... a custom button press */
		return rc;
	schedule_callback2(event_callback, &callback_schedule,
				"custom-button-press-event", c->shipid, subcode);
	return 0;
}

static int process_client_config(struct game_client *c)
{
	int rc;
	unsigned char subcode, current_station;
	unsigned char buffer[10];

	rc = read_and_unpack_buffer(c, buffer, "b", &subcode);
	if (rc)
		return rc;
	switch (subcode) {
	case OPCODE_CLIENT_NOTIFY_CURRENT_STATION:
		rc = read_and_unpack_buffer(c, buffer, "b", &current_station);
		if (rc)
			return rc;
		c->current_station = current_station;
		break;
	default:
		fprintf(stderr, "snis_server: OPCODE_CLIENT_CONFIG subcode is unknown value%hhu\n", subcode);
		return -1;
	}
	return 0;
}

static int process_comms_crypto(struct game_client *c)
{
	int i, rc;
	unsigned char new_mode;
	unsigned char buffer[10];
	struct snis_entity *o;

	rc = read_and_unpack_buffer(c, buffer, "b", &new_mode);
	if (rc)
		return rc;
	new_mode = !!new_mode;
	if (c->ship_index < 0) /* shouldn't happen */
		return 0;
	i = lookup_by_id(c->shipid);
	if (i < 0)
		return 0;
	o = &go[i];
	o->tsd.ship.comms_crypto_mode = new_mode;
	return 0;
}

static int process_toggle_demon_ai_debug_mode(struct game_client *c)
{
	c->debug_ai = !c->debug_ai;
	return 0;
}

static int process_request_universe_timestamp(struct game_client *c)
{
	c->request_universe_timestamp = UPDATE_UNIVERSE_TIMESTAMP_COUNT;
	return 0;
}

static int process_latency_check(struct game_client *c)
{
	struct timespec new_ts, orig_ts;
	unsigned char buffer[20];
	uint64_t value[2];
	int rc;
	double latency_in_usec;

	clock_gettime(CLOCK_MONOTONIC, &new_ts);
	rc = read_and_unpack_buffer(c, buffer, "qq", &value[0], &value[1]);
	if (rc)
		return rc;
	BUILD_ASSERT(sizeof(value) >= sizeof(orig_ts));
	memcpy(&orig_ts, value, sizeof(orig_ts));

	latency_in_usec = (1000000.0 * new_ts.tv_sec + 0.001 * new_ts.tv_nsec) -
				(1000000.0 * orig_ts.tv_sec + 0.001 * orig_ts.tv_nsec);
	c->latency_in_usec = (uint32_t) latency_in_usec;
	return 0;
}

static int process_build_info(struct game_client *c)
{
	unsigned char buffer[256], data[256];
	uint32_t buflen = 255;
	uint8_t x;
	int rc;

	memset(buffer, 0, sizeof(buffer));
	memset(data, 0, sizeof(data));
	rc = read_and_unpack_buffer(c, buffer, "bw", &x, &buflen);
	if (rc != 0)
		return rc;
	if (x != 0 && x != 1)
		return -1;
	if (buflen > 256)
		return -1;
	rc = snis_readsocket(c->socket, data, buflen);
	if (rc != 0)
		return rc;
	data[255] = '\0';
	if (c->build_info[x])
		free(c->build_info[x]);
	c->build_info[x] = strdup((char *) data);
	return 0;
}

static int process_toggle_demon_safe_mode(void)
{
	safe_mode = !safe_mode;
	if (safe_mode) {
		snis_queue_add_global_text_to_speech("Safe mode enabled.");
		send_demon_console_msg("SAFE MODE ENABLED.");
	} else {
		snis_queue_add_global_text_to_speech("Safe mode disabled.");
		send_demon_console_msg("SAFE MODE DISABLED.");
	}
	return 0;
}

static int l_clear_all(__attribute__((unused)) lua_State *l)
{
	process_demon_clear_all();
	return 0;
}

static void send_timed_text(int id, uint16_t timevalue, uint16_t length, char *text)
{
	struct packed_buffer *pb;

	assert(strlen(text) + 1 == length);
	pb = packed_buffer_allocate(2 + 2 + 2 + length);
	packed_buffer_append(pb, "bbhh", OPCODE_TEXTSCREEN_OP, OPCODE_TEXTSCREEN_TIMEDTEXT,
				length, timevalue);
	packed_buffer_append_raw(pb, text, length);
	if (id == -1)
		send_packet_to_all_clients(pb, ROLE_ALL);
	else
		send_packet_to_all_clients_on_a_bridge(id, pb, ROLE_ALL);
}

static void send_menu(int id, uint16_t length, char *text)
{
	struct packed_buffer *pb;

	assert(strlen(text) + 1 == length);
	pb = packed_buffer_allocate(2 + 2 + length);
	packed_buffer_append(pb, "bbh", OPCODE_TEXTSCREEN_OP, OPCODE_TEXTSCREEN_MENU, length);
	packed_buffer_append_raw(pb, text, length);
	if (id == -1)
		send_packet_to_all_clients(pb, ROLE_ALL);
	else
		send_packet_to_all_clients_on_a_bridge(id, pb, ROLE_ALL);
}

static int l_show_menu(lua_State *l)
{
	const double id = luaL_checknumber(l, 1);
	const char *luatext = luaL_checkstring(l, 2);
	const char *luacallback = luaL_checkstring(l, 3);
	if (current_user_menu_callback) {
		/* FIXME, this races with process_textscreen_op() */
		free(current_user_menu_callback);
	}
	current_user_menu_callback = strdup(luacallback);
	char *text = NULL;
	uint32_t oid = (uint32_t) id;
	int i;
	struct snis_entity *o;
	int length;

	if (!luatext)
		goto error;
	text = strdup(luatext);

	length = strlen(text) + 1;
	if (length > 1024) { /* truncate excessively long strings */
		text[1024] = '\0';
		length = 1024;
	}

	if (oid != (uint32_t) -1) { /* User has specified a ship id. */
		pthread_mutex_lock(&universe_mutex);
		i = lookup_by_id(oid);
		if (i < 0) {
			pthread_mutex_unlock(&universe_mutex);
			goto error;
		}
		o = &go[i];
		if (o->type != OBJTYPE_SHIP1) {
			pthread_mutex_unlock(&universe_mutex);
			goto error;
		}
		pthread_mutex_unlock(&universe_mutex);
		send_menu(o->id, length, text);
	} else {
		/* User has specified -1 as ship id == all ships. */
		send_menu(-1, length, text);
	}
	if (text)
		free(text);
	lua_pushnumber(l, 0.0);
	return 1;
error:
	if (text)
		free(text);
	lua_pushnil(l);
	return 1;
}

static int l_show_timed_text(lua_State *l)
{
	const double id = luaL_checknumber(l, 1);
	const double seconds = luaL_checknumber(l, 2);
	const char *luatext = luaL_checkstring(l, 3);
	char *text = NULL;
	uint32_t oid = (uint32_t) id;
	int i;
	struct snis_entity *o;
	int length;
	uint16_t timevalue = (uint16_t) seconds;

	if (!luatext)
		goto error;
	text = strdup(luatext);
	if (seconds > 120.0)
		goto error;

	length = strlen(text) + 1;
	if (length > 1024) { /* truncate excessively long strings */
		text[1024] = '\0';
		length = 1024;
	}

	if (oid != (uint32_t) -1) { /* User has specified a ship id. */
		pthread_mutex_lock(&universe_mutex);
		i = lookup_by_id(oid);
		if (i < 0) {
			pthread_mutex_unlock(&universe_mutex);
			goto error;
		}
		o = &go[i];
		if (o->type != OBJTYPE_SHIP1) {
			pthread_mutex_unlock(&universe_mutex);
			goto error;
		}
		pthread_mutex_unlock(&universe_mutex);
		send_timed_text(o->id, timevalue, length, text);
	} else {
		/* User has specified -1 as ship id == all ships. */
		send_timed_text(-1, timevalue, length, text);
	}
	if (text)
		free(text);
	lua_pushnumber(l, 0.0);
	return 1;
error:
	if (text)
		free(text);
	lua_pushnil(l);
	return 1;
}

static int l_enqueue_lua_script(lua_State *l)
{
	const char *lua_scriptname = luaL_checkstring(l, 1);
	char scriptname[PATH_MAX];
	char *upper_scriptname = NULL;
	if (!lua_scriptname)
		goto error;

	upper_scriptname = strdup(lua_scriptname);
	if (!upper_scriptname)
		goto error;

	uppercase(upper_scriptname);
	snprintf(scriptname, sizeof(scriptname) - 1, "%s", upper_scriptname);
	pthread_mutex_lock(&universe_mutex);
	enqueue_lua_command(scriptname); /* queue up for execution by main thread. */
	pthread_mutex_unlock(&universe_mutex);
	lua_pushnumber(l, 0.0);
	if (upper_scriptname)
		free(upper_scriptname);
	return 1;
error:
	if (upper_scriptname)
		free(upper_scriptname);
	lua_pushnil(l);
	return 1;
}

static int l_set_asteroid_minerals(lua_State *l)
{
	const double lua_oid = luaL_checknumber(l, 1);
	const double lua_carbon = luaL_checknumber(l, 2);
	const double lua_silicates = luaL_checknumber(l, 3);
	const double lua_nickeliron = luaL_checknumber(l, 4);
	const double lua_preciousmetals = luaL_checknumber(l, 5);
	uint32_t oid = (uint32_t) lua_oid;
	int rc;

	pthread_mutex_lock(&universe_mutex);
	rc = set_asteroid_minerals(oid, (float) lua_carbon, (float) lua_silicates,
					(float) lua_nickeliron, (float) lua_preciousmetals);
	pthread_mutex_unlock(&universe_mutex);
	if (rc) {
		lua_pushnil(lua_state);
		return 1;
	}
	lua_pushnumber(lua_state, 0.0);
	return 1;
}

static int l_get_ship_attribute(lua_State *l)
{
	struct key_value_specification *kvs;
	const double lua_oid = luaL_checknumber(l, 1);
	const char *attribute;
	struct snis_entity *o;
	uint64_t ui64;
	uint32_t ui32;
	uint16_t ui16;
	uint8_t ui8;
	int64_t i64;
	int32_t i32;
	int16_t i16;
	int8_t i8;
	double dbl;
	float flt;
	char *str;
	uint32_t oid;
	int i;
	void *base_address[1];
	char errmsg[100] = { 0 };

	oid = (uint32_t) lua_oid;
	attribute = lua_tostring(lua_state, 2);
	pthread_mutex_lock(&universe_mutex);
	i = lookup_by_id(oid);
	if (i < 0) {
		snprintf(errmsg, sizeof(errmsg), "FAILED TO FIND ID %d\n", oid);
		goto error;
	}
	o = &go[i];
	if (o->type != OBJTYPE_SHIP1 && o->type != OBJTYPE_SHIP2) {
		snprintf(errmsg, sizeof(errmsg), "ID %d IS NOT A SHIP\n", oid);
		goto error;
	}
	base_address[0] = o;
	kvs = lookup_key_entry(snis_entity_kvs, attribute);
	if (!kvs) {
		snprintf(errmsg, sizeof(errmsg), "FAILED TO FIND ATTRIBUTE %s\n", attribute);
		goto error;
	}
	switch (kvs->type) {
	case KVS_STRING:
		str = (char *) o + kvs->address_offset;
		lua_pushstring(l, str);
		send_demon_console_msg(errmsg);
		break;
	case KVS_UINT64:
		if (key_value_get_value(kvs, attribute, base_address, &ui64, sizeof(ui64)) != sizeof(ui64)) {
			snprintf(errmsg, sizeof(errmsg), "FAILED TO GET UINT64 VALUE FOR %s\n", attribute);
			goto error;
		}
		lua_pushnumber(l, (double) ui64);
		break;
	case KVS_UINT32:
		if (key_value_get_value(kvs, attribute, base_address, &ui32, sizeof(ui32)) != sizeof(ui32)) {
			snprintf(errmsg, sizeof(errmsg), "FAILED TO GET UINT32 VALUE FOR %s\n", attribute);
			goto error;
		}
		lua_pushnumber(l, (double) ui32);
		break;
	case KVS_UINT16:
		if (key_value_get_value(kvs, attribute, base_address, &ui16, sizeof(ui16)) != sizeof(ui16)) {
			snprintf(errmsg, sizeof(errmsg), "FAILED TO GET UINT16 VALUE FOR %s\n", attribute);
			goto error;
		}
		lua_pushnumber(l, (double) ui16);
		break;
	case KVS_UINT8:
		if (key_value_get_value(kvs, attribute, base_address, &ui8, sizeof(ui16)) != sizeof(ui8)) {
			snprintf(errmsg, sizeof(errmsg), "FAILED TO GET UINT8 VALUE FOR %s\n", attribute);
			goto error;
		}
		lua_pushnumber(l, (double) ui8);
		break;
	case KVS_INT64:
		if (key_value_get_value(kvs, attribute, base_address, &i64, sizeof(i64)) != sizeof(i64)) {
			snprintf(errmsg, sizeof(errmsg), "FAILED TO GET INT64 VALUE FOR %s\n", attribute);
			goto error;
		}
		lua_pushnumber(l, (double) i64);
		break;
	case KVS_INT32:
		if (key_value_get_value(kvs, attribute, base_address, &i32, sizeof(i32)) != sizeof(i32)) {
			snprintf(errmsg, sizeof(errmsg), "FAILED TO GET INT32 VALUE FOR %s\n", attribute);
			goto error;
		}
		lua_pushnumber(l, (double) i32);
		break;
	case KVS_INT16:
		if (key_value_get_value(kvs, attribute, base_address, &i16, sizeof(i16)) != sizeof(i16)) {
			snprintf(errmsg, sizeof(errmsg), "FAILED TO GET INT16 VALUE FOR %s\n", attribute);
			goto error;
		}
		lua_pushnumber(l, (double) i16);
		break;
	case KVS_INT8:
		if (key_value_get_value(kvs, attribute, base_address, &i8, sizeof(i8)) != sizeof(i8)) {
			snprintf(errmsg, sizeof(errmsg), "FAILED TO GET INT8 VALUE FOR %s\n", attribute);
			goto error;
		}
		lua_pushnumber(l, (double) i8);
		break;
	case KVS_DOUBLE:
		if (key_value_get_value(kvs, attribute, base_address, &dbl, sizeof(dbl)) != sizeof(dbl)) {
			snprintf(errmsg, sizeof(errmsg), "FAILED TO GET DOUBLE VALUE FOR %s\n", attribute);
			goto error;
		}
		lua_pushnumber(l, dbl);
		break;
	case KVS_FLOAT:
		if (key_value_get_value(kvs, attribute, base_address, &flt, sizeof(flt)) != sizeof(flt)) {
			snprintf(errmsg, sizeof(errmsg), "FAILED TO GET FLOAT VALUE FOR %s\n", attribute);
			goto error;
		}
		lua_pushnumber(l, (double) flt);
		break;
	default:
		snprintf(errmsg, sizeof(errmsg), "UNKNOWN DATA TYPE %d\n", kvs->type);
		goto error;
	}
	pthread_mutex_unlock(&universe_mutex);
	return 1;
error:
	pthread_mutex_unlock(&universe_mutex);
	send_demon_console_msg(errmsg);
	lua_pushnil(lua_state);
	return 1;
}

static int l_get_commodity_name(lua_State *l)
{
	const double lua_commodity_index = luaL_checknumber(l, 1);
	int index = (int) lua_commodity_index;

	if (index < 0 || index >= ncommodities) {
		lua_pushnil(lua_state);
		return 1;
	}
	lua_pushstring(l, commodity[index].name);
	return 1;
}

static int l_get_commodity_units(lua_State *l)
{
	const double lua_commodity_index = luaL_checknumber(l, 1);
	int index = (int) lua_commodity_index;

	if (index < 0 || index >= ncommodities) {
		lua_pushnil(lua_state);
		return 1;
	}
	lua_pushstring(l, commodity[index].unit);
	return 1;
}

static int l_lookup_commodity(lua_State *l)
{
	const char *lua_commodity_name = luaL_checkstring(l, 1);
	int i;

	for (i = 0; i < ncommodities; i++) {
		if (strcasecmp(lua_commodity_name, commodity[i].name) == 0) {
			lua_pushnumber(l, (float) i);
			return 1;
		}
	}
	lua_pushnil(l);
	return 1;
}

static int l_set_commodity_contents(lua_State *l)
{
	const double lua_oid = luaL_checknumber(l, 1);
	const double lua_commodity_index = luaL_checknumber(l, 2);
	const double lua_quantity = luaL_checknumber(l, 3);
	struct snis_entity *o;
	uint32_t oid = (uint32_t) lua_oid;
	int index = (int) lua_commodity_index;
	int i;

	pthread_mutex_lock(&universe_mutex);
	i = lookup_by_id(oid);
	if (i < 0)
		goto out;
	if (i > snis_object_pool_highest_object(pool))
		goto out;
	o = &go[i];
	if (o->type != OBJTYPE_SHIP1 && o->type != OBJTYPE_CARGO_CONTAINER && o->type != OBJTYPE_SHIP2)
		goto out;
	if (index < -1 || index >= ncommodities)
		goto out;
	if (lua_quantity < 0.01)
		goto out;
	if (o->type == OBJTYPE_SHIP1 || o->type == OBJTYPE_SHIP2) {
		const double lua_cargo_bay = luaL_checknumber(l, 4);
		int cargo_bay = (int) lua_cargo_bay;
		if (cargo_bay < 0 || cargo_bay >= o->tsd.ship.ncargo_bays)
			goto out;
		o->tsd.ship.cargo[cargo_bay].contents.item = index;
		o->tsd.ship.cargo[cargo_bay].contents.qty = lua_quantity;
		o->tsd.ship.cargo[cargo_bay].paid = 0.0;
		o->tsd.ship.cargo[cargo_bay].origin = -1;
		o->tsd.ship.cargo[cargo_bay].dest = -1;
	} else if (o->type == OBJTYPE_CARGO_CONTAINER) {
		o->tsd.cargo_container.contents.item = index;
		o->tsd.cargo_container.contents.qty = lua_quantity;
	}
	pthread_mutex_unlock(&universe_mutex);
	lua_pushnumber(l, 0.0);
	return 1;
out:
	pthread_mutex_unlock(&universe_mutex);
	lua_pushnil(l);
	return 1;
}

static int l_add_commodity(lua_State *l)
{
	const char *name = luaL_checkstring(l, 1);
	const char *units = luaL_checkstring(l, 2);
	const char *scans_as = luaL_checkstring(l, 3);
	const double base_price = luaL_checknumber(l, 4);
	const double volatility = luaL_checknumber(l, 5);
	const double legality = luaL_checknumber(l, 6);
	const double econ_sensitivity = luaL_checknumber(l, 7);
	const double govt_sensitivity = luaL_checknumber(l, 8);
	const double tech_sensitivity = luaL_checknumber(l, 9);
	const double odds = luaL_checknumber(l, 10);
	int iodds = odds;

	int n = add_commodity(&commodity, &ncommodities,
		name, units, scans_as, base_price, volatility, legality,
		econ_sensitivity, govt_sensitivity, tech_sensitivity, iodds);
	lua_pushnumber(l, (double) n);
	return 1;
}

static int l_reset_player_ship(lua_State *l)
{
	const double lua_oid = luaL_checknumber(l, 1);
	struct snis_entity *o;
	uint32_t oid = (uint32_t) lua_oid;
	int i;

	pthread_mutex_lock(&universe_mutex);
	i = lookup_by_id(oid);
	if (i < 0)
		goto out;
	o = &go[i];
	if (o->type != OBJTYPE_SHIP1)
		goto out;
	reset_player_ship(o);
	pthread_mutex_unlock(&universe_mutex);
	lua_pushnumber(l, 0.0);
	return 1;
out:
	pthread_mutex_unlock(&universe_mutex);
	lua_pushnil(l);
	return 1;
}

static int l_dock_player_to_starbase(lua_State *l)
{
	int i, b;
	struct snis_entity *player, *starbase, *docking_port;
	const double player_id = luaL_checknumber(l, 1);
	const double starbase_id = luaL_checknumber(l, 2);

	pthread_mutex_lock(&universe_mutex);
	i = lookup_by_id(player_id);
	if (i < 0)
		goto failure;
	if (go[i].type != OBJTYPE_SHIP1)
		goto failure;
	player = &go[i];
	i = lookup_by_id(starbase_id);
	if (i < 0)
		goto failure;
	if (go[i].type != OBJTYPE_STARBASE)
		goto failure;
	starbase = &go[i];
	/* Find a docking port */
	b = lookup_bridge_by_shipid(player->id);
	if (b < 0) {
		fprintf(stderr, "%s:%d: player bridge unexpectedly not found.\n",
			__FILE__, __LINE__);
		goto failure;
	}
	for (i = 0; i <= snis_object_pool_highest_object(pool); i++) {
		if (go[i].type != OBJTYPE_DOCKING_PORT)
			continue;
		docking_port = &go[i];
		if (!docking_port->alive || docking_port->tsd.docking_port.parent != starbase->id ||
			docking_port->tsd.docking_port.docked_guy != (uint32_t) -1)
			continue;
		docking_port->tsd.docking_port.docked_guy = player->id; /* Dock player */
		player->tsd.ship.docking_magnets = 1; /* Turn on docking magnets */
		break;
	}
	/* docking_port_move() will move the player ship to the right place. */
	do_docking_action(player, starbase, &bridgelist[b], starbase->sdata.name);

	pthread_mutex_unlock(&universe_mutex);
	lua_pushnumber(l, 0.0);
	return 1;
failure:
	pthread_mutex_unlock(&universe_mutex);
	lua_pushnumber(l, -1.0);
	return 1;
}

static int l_object_distance(lua_State *l)
{
	int i;
	struct snis_entity *o1, *o2;
	const double oid1 = luaL_checknumber(l, 1);
	const double oid2 = luaL_checknumber(l, 2);
	double dist;

	pthread_mutex_lock(&universe_mutex);
	i = lookup_by_id(oid1);
	if (i < 0)
		goto failure;
	o1 = &go[i];
	i = lookup_by_id(oid2);
	if (i < 0)
		goto failure;
	o2 = &go[i];
	dist = object_dist(o1, o2);
	pthread_mutex_unlock(&universe_mutex);
	lua_pushnumber(lua_state, dist);
	return 1;
failure:
	pthread_mutex_unlock(&universe_mutex);
	lua_pushnil(lua_state);
	return 1;
}

static int l_enable_antenna_aiming(lua_State *l)
{
	enable_comms_attenuation = 1;
	return 0;
}

static int l_disable_antenna_aiming(lua_State *l)
{
	enable_comms_attenuation = 0;
	return 0;
}

static int l_set_custom_button_label(lua_State *l)
{
	int i, b, s;
	const double oid = luaL_checknumber(l, 1);
	const double screen = luaL_checknumber(l, 2);
	const char *text = lua_tostring(lua_state, 3);

	s = (int) screen;
	pthread_mutex_lock(&universe_mutex);
	i = lookup_by_id(oid);
	if (i < 0)
		goto error;
	b = lookup_bridge_by_shipid(go[i].id);
	if (b < 0)
		goto error;
	if (s < 0 || s > 5)
		goto error;
	memset(bridgelist[b].custom_button_text[s], 0, sizeof(bridgelist[b].custom_button_text[s]));
	strncpy(bridgelist[b].custom_button_text[s], text, sizeof(bridgelist[b].custom_button_text[s]) - 1);
	pthread_mutex_unlock(&universe_mutex);
	lua_pushnumber(lua_state, 0);
	return 1;
error:
	pthread_mutex_unlock(&universe_mutex);
	lua_pushnumber(lua_state, -1);
	return 1;
}

static int enable_or_disable_custom_button(double oid, double screen, int enable)
{
	int i, b, s;

	s = (int) screen;

	pthread_mutex_lock(&universe_mutex);
	i = lookup_by_id(oid);
	if (i < 0)
		goto error;
	b = lookup_bridge_by_shipid(go[i].id);
	if (b < 0)
		goto error;
	if (s < 0 || s > 5)
		goto error;
	if (enable)
		bridgelist[b].active_custom_buttons |= (1 << s);
	else
		bridgelist[b].active_custom_buttons &= ~(1 << s);
	pthread_mutex_unlock(&universe_mutex);
	lua_pushnumber(lua_state, 0);
	return 1;
error:
	pthread_mutex_unlock(&universe_mutex);
	lua_pushnumber(lua_state, -1);
	return 1;
}

static int l_enable_custom_button(lua_State *l)
{
	const double oid = luaL_checknumber(l, 1);
	const double screen = luaL_checknumber(l, 2);
	return enable_or_disable_custom_button(oid, screen, 1);
}

static int l_disable_custom_button(lua_State *l)
{
	const double oid = luaL_checknumber(l, 1);
	const double screen = luaL_checknumber(l, 2);
	return enable_or_disable_custom_button(oid, screen, 0);
}

static int l_fire_missile(lua_State *l)
{
	const double sid = luaL_checknumber(l, 1);
	const double tid = luaL_checknumber(l, 2);
	int s, t;

	pthread_mutex_lock(&universe_mutex);
	s = lookup_by_id((uint32_t) sid);
	t = lookup_by_id((uint32_t) tid);
	if (s >= 0 && t >= 0)
		fire_missile(&go[s], go[t].id);
	pthread_mutex_unlock(&universe_mutex);
	return 0;
}

static void regenerate_universe(void);
static int l_regenerate_universe(lua_State *l)
{
	regenerate_universe();
	return 0;
}

static int l_set_variable(lua_State *l)
{
	const char *name = luaL_checkstring(l, 1);
	const char *value = luaL_checkstring(l, 2);
	char cmd[255];

	snprintf(cmd, sizeof(cmd), "SET %s = %s", name, value);
	server_builtin_set(cmd);

	return 0;
}

static int l_demon_print(lua_State *l)
{
	const char *str = luaL_checkstring(l, 1);
	char buf[DEMON_CONSOLE_MSG_MAX];
	snprintf(buf, sizeof(buf) - 1, "%s", str);
	send_demon_console_msg(buf);
	return 0;
}

static int l_add_bounty(lua_State *l)
{
	const float fid = luaL_checknumber(l, 1);
	const char *crime_desc = luaL_checkstring(l, 2);
	const float amount = luaL_checknumber(l, 3);
	const float fsbid = luaL_checknumber(l, 4);
	int i;

	uint32_t id = (uint32_t) fid;
	uint32_t sbid = (uint32_t) fsbid;
	char *crime = strdup(crime_desc);
	uppercase(crime);

	pthread_mutex_lock(&universe_mutex);
	i = lookup_by_id(sbid);
	if (i < 0) {
		pthread_mutex_unlock(&universe_mutex);
		lua_pushnumber(l, -1);
		return 1;
	}
	if (go[i].type != OBJTYPE_STARBASE) {
		pthread_mutex_unlock(&universe_mutex);
		lua_pushnumber(l, -1);
		return 1;
	}
	i = lookup_by_id(id);
	if (i < 0) {
		pthread_mutex_unlock(&universe_mutex);
		lua_pushnumber(l, -1);
		return 1;
	}
	if (go[i].type != OBJTYPE_SHIP2) {
		pthread_mutex_unlock(&universe_mutex);
		lua_pushnumber(l, -1);
		return 1;
	}
	ship_registry_add_bounty(&ship_registry, id, crime, amount, sbid);
	pthread_mutex_unlock(&universe_mutex);
	lua_pushnumber(l, 0);
	return 1;
}

static int l_random_point_on_sphere(lua_State *l)
{
	const float radius = luaL_checknumber(l, 1);
	float x, y, z;

	random_point_on_sphere(radius, &x, &y, &z);
	lua_pushnumber(l, x);
	lua_pushnumber(l, y);
	lua_pushnumber(l, z);
	return 3;
}

static int l_generate_ship_name(lua_State *l)
{
	char shipname[256];
	static struct mtwist_state *mt = NULL;

	if (!mt)
		mt = mtwist_init(mtwist_seed);

	ship_name(mt, shipname, sizeof(shipname) - 1);
	uppercase(shipname);
	lua_pushfstring(l, "%s", shipname);
	return 1;
}

static int l_generate_character_name(lua_State *l)
{
	char charname[256];
	static struct mtwist_state *mt = NULL;

	if (!mt)
		mt = mtwist_init(mtwist_seed);

	character_name(mt, charname, sizeof(charname) - 1);
	uppercase(charname);
	lua_pushfstring(l, "%s", charname);
	return 1;
}

static int l_generate_name(lua_State *l)
{
	char *name;
	static struct mtwist_state *mt = NULL;

	if (!mt)
		mt = mtwist_init(mtwist_seed);

	name = random_name(mt);
	uppercase(name);
	lua_pushfstring(l, "%s", name);
	free(name);
	return 1;
}

static int l_ai_trace(lua_State *l)
{
	const double id = luaL_checknumber(lua_state, 1);
	uint32_t id32 = (uint32_t) id;
	int i;

	if (id < 0) {
		ai_trace_id = (uint32_t) -1;
		send_demon_console_msg("AI TRACING OFF");
		return 0;
	}

	pthread_mutex_lock(&universe_mutex);
	i = lookup_by_id(id32);
	if (i < 0)
		ai_trace_id = (uint32_t) -1;
	else
		ai_trace_id = id32;
	pthread_mutex_unlock(&universe_mutex);
	if (ai_trace_id != (uint32_t) -1)
		send_demon_console_msg("AI TRACING OBJECT %u", ai_trace_id);
	else
		send_demon_console_msg("AI TRACING OFF");
	return 0;
}

static int l_get_passenger_location(lua_State *l)
{
	const double pidx = luaL_checknumber(lua_state, 1);
	int p = (int) pidx;
	double location;

	if (p < 0 || p > MAX_PASSENGERS) {
		send_demon_console_msg("GET_PASSENGER_LOCATION: PASSENGER OUT OF RANGE (0 - %d): %d",
			MAX_PASSENGERS, p);
		return 0;
	}
	pthread_mutex_lock(&universe_mutex);
	location = passenger[p].location;
	pthread_mutex_unlock(&universe_mutex);
	lua_pushnumber(lua_state, location);
	return 1;
}

static int l_set_planet_description(lua_State *l)
{
	const double planet_id = luaL_checknumber(lua_state, 1);
	const char *description = luaL_checkstring(l, 2);
	struct snis_entity *o;
	int i;

	pthread_mutex_lock(&universe_mutex);
	i = lookup_by_id(planet_id);
	if (i < 0) {
		pthread_mutex_unlock(&universe_mutex);
		send_demon_console_msg("SET_PLANET_DESCRIPTION: BAD OBJECT ID: %u\n", (unsigned int) planet_id);
		return 0;
	}
	o = &go[i];
	if (o->type != OBJTYPE_PLANET) {
		pthread_mutex_unlock(&universe_mutex);
		send_demon_console_msg("SET_PLANET_DESCRIPTION: WRONG OBJECT TYPE FOR ID: %u\n",
						(unsigned int) planet_id);
		return 0;
	}
	if (o->tsd.planet.custom_description)
		free(o->tsd.planet.custom_description);
	o->tsd.planet.custom_description = strndup(description, 255);
	o->tsd.planet.custom_description[255] = '\0';
	o->timestamp = universe_timestamp;
	pthread_mutex_unlock(&universe_mutex);
	return 0;
}

static int set_planet_byte_property_helper(lua_State *l, char *property,
						int offset, int lobound, int highbound)
{
	const double planet_id = luaL_checknumber(lua_state, 1);
	const double value = luaL_checknumber(l, 2);
	struct snis_entity *p;
	uint8_t *target, v;
	int i;

	pthread_mutex_lock(&universe_mutex);
	i = lookup_by_id(planet_id);
	if (i < 0) {
		pthread_mutex_unlock(&universe_mutex);
		send_demon_console_msg("SET_PLANET_%s: BAD OBJECT ID: %u\n",
				property, (unsigned int) planet_id);
		return 0;
	}
	p = &go[i];
	if (p->type != OBJTYPE_PLANET) {
		pthread_mutex_unlock(&universe_mutex);
		send_demon_console_msg("SET_PLANET_%s: OBJECT %u IS NOT A PLANET\n",
				property, (unsigned int) planet_id);
		return 0;
	}
	target = (uint8_t *) p;
	target += offset;
	v = (uint8_t) value;
	if (v < lobound)
		v = lobound;
	if (v > highbound)
		v = highbound;
	*target = v;
	p->timestamp = universe_timestamp;
	pthread_mutex_unlock(&universe_mutex);
	return 0;
}

static int l_set_planet_government(lua_State *l)
{
	return set_planet_byte_property_helper(l, "GOVERNMENT",
		offsetof(struct snis_entity, tsd.planet.government), 0, ARRAYSIZE(government_name) - 1);
}

static int l_set_planet_tech_level(lua_State *l)
{
	return set_planet_byte_property_helper(l, "TECH_LEVEL",
		offsetof(struct snis_entity, tsd.planet.tech_level), 0, ARRAYSIZE(tech_level_name) - 1);
}

static int l_set_planet_economy(lua_State *l)
{
	return set_planet_byte_property_helper(l, "ECONOMY",
		offsetof(struct snis_entity, tsd.planet.economy), 0, ARRAYSIZE(economy_name) - 1);
}

static int l_set_planet_security(lua_State *l)
{
	return set_planet_byte_property_helper(l, "SECURITY",
		offsetof(struct snis_entity, tsd.planet.security), LOW_SECURITY, HIGH_SECURITY);
}

static int l_update_player_wallet(lua_State *l)
{
	const double pid = luaL_checknumber(lua_state, 1);
	const double delta_money = luaL_checknumber(lua_state, 2);
	int i;

	pthread_mutex_lock(&universe_mutex);
	i = lookup_by_id(pid);
	if (i < 0) {
		pthread_mutex_unlock(&universe_mutex);
		send_demon_console_msg("UPDATE_PLAYER_WALLET: BAD PLAYER SHIP ID: %u", (uint32_t) pid);
		return 0;
	}
	if (go[i].type != OBJTYPE_SHIP1) {
		pthread_mutex_unlock(&universe_mutex);
		send_demon_console_msg("UPDATE_PLAYER_WALLET: WRONG OBJECT TYPE: %u", (uint32_t) pid);
		return 0;
	}
	go[i].tsd.ship.wallet += delta_money;
	pthread_mutex_unlock(&universe_mutex);
	return 0;
}

static int l_set_passenger_location(lua_State *l)
{
	const double pidx = luaL_checknumber(lua_state, 1);
	const double ploc = luaL_checknumber(lua_state, 2);
	int i, p = (int) pidx;
	uint32_t location = (uint32_t) ploc;

	if (p < 0 || p > MAX_PASSENGERS) {
		send_demon_console_msg("SET_PASSENGER_LOCATION: PASSENGER OUT OF RANGE (0 - %d): %d",
			MAX_PASSENGERS, p);
		return 0;
	}
	pthread_mutex_lock(&universe_mutex);
	i = lookup_by_id(location);
	if (i < 0) {
		pthread_mutex_unlock(&universe_mutex);
		send_demon_console_msg("SET_PASSENGER_LOCATION: BAD LOCATION: %d", location);
		return 0;
	}
	if (go[i].type != OBJTYPE_STARBASE && go[i].type != OBJTYPE_SHIP1) {
		pthread_mutex_unlock(&universe_mutex);
		send_demon_console_msg("SET_PASSENGER_LOCATION: INAPPROPRIATE LOCATION: %d", location);
		return 0;
	}
	passenger[p].location = location;
	pthread_mutex_unlock(&universe_mutex);
	return 0;
}

static int l_create_passenger(lua_State *l)
{
	const double pidx = luaL_checknumber(lua_state, 1);
	const char *name = luaL_checkstring(l, 2);
	const double loc = luaL_checknumber(lua_state, 3);
	const double dest = luaL_checknumber(lua_state, 4);
	const double fare = luaL_checknumber(lua_state, 5);
	uint32_t id32;
	int location, destination, p = (int) pidx;

	if (p < 0 || p > MAX_PASSENGERS) {
		send_demon_console_msg("CREATE_PASSENGER: PASSENGER OUT OF RANGE (0 - %d): %d",
			MAX_PASSENGERS, p);
		return 0;
	}
	pthread_mutex_lock(&universe_mutex);
	id32 = (uint32_t) loc;
	location = lookup_by_id(id32);
	if (location < 0) {
		pthread_mutex_unlock(&universe_mutex);
		send_demon_console_msg("CREATE_PASSENGER: BAD LOCATION ID %u", id32);
		return 0;
	}
	if (go[location].type != OBJTYPE_SHIP1 && go[location].type != OBJTYPE_STARBASE) {
		pthread_mutex_unlock(&universe_mutex);
		send_demon_console_msg("CREATE_PASSENGER: INAPPROPRIATE LOCATION ID %u", id32);
		return 0;
	}
	id32 = (uint32_t) dest;
	destination = lookup_by_id(id32);
	if (destination < 0) {
		pthread_mutex_unlock(&universe_mutex);
		send_demon_console_msg("CREATE_PASSENGER: BAD DESTINATION ID %u", id32);
		return 0;
	}
	if (go[location].type != OBJTYPE_STARBASE) {
		pthread_mutex_unlock(&universe_mutex);
		send_demon_console_msg("CREATE_PASSENGER: INAPPROPRIATE DESTINATION ID %u", id32);
		return 0;
	}
	snprintf(passenger[p].name, sizeof(passenger[p].name), "%s", name);
	passenger[p].location = go[location].id;
	passenger[p].destination = go[destination].id;
	passenger[p].fare = fare;
	pthread_mutex_unlock(&universe_mutex);
	return 0;
}

static int process_create_item(struct game_client *c)
{
	unsigned char buffer[14];
	unsigned char item_type, data1, data2;
	char *n;
	double x, y, z, r;
	int rc, i = -1;
	static struct mtwist_state *mt = NULL;

	if (!mt)
		mt = mtwist_init(mtwist_seed);

	rc = read_and_unpack_buffer(c, buffer, "bSSSbb", &item_type,
			&x, (int32_t) UNIVERSE_DIM,
			&y, (int32_t) UNIVERSE_DIM,
			&z, (int32_t) UNIVERSE_DIM, &data1, &data2);
	if (rc)
		return rc;

	pthread_mutex_lock(&universe_mutex);
	switch (item_type) {
	case OBJTYPE_SHIP2:
		if (data1 < 0 || data1 >= nshiptypes)
			data1 = snis_randn(nshiptypes);
		n = random_name(mt);
		if (data2 >= nfactions())
			data2 = snis_randn(nfactions());
		i = add_specific_ship(n, x, y, z, data1, data2, 1);
		free(n);
		break;
	case OBJTYPE_STARBASE:
		i = add_starbase(x, y, z, 0, 0, 0, snis_randn(100), -1);
		break;
	case OBJTYPE_PLANET:
		r = (float) snis_randn(MAX_PLANET_RADIUS - MIN_PLANET_RADIUS) +
					MIN_PLANET_RADIUS;
		i = add_planet(x, y, z, r, 0, -1);
		break;
	case OBJTYPE_BLACK_HOLE:
		r = (float) snis_randn(MAX_BLACK_HOLE_RADIUS - MIN_BLACK_HOLE_RADIUS) +
					MIN_BLACK_HOLE_RADIUS;
		i = add_black_hole(x, y, z, r);
		break;
	case OBJTYPE_ASTEROID:
		i = add_asteroid(x, y, z, 0.0, 0.0, 0.0, 1.0);
		break;
	case OBJTYPE_NEBULA:
		r = (double) snis_randn(NEBULA_RADIUS) +
				(double) MIN_NEBULA_RADIUS;
		i = add_nebula(x, y, z, 0, 0, 0, r);
		break;
	case OBJTYPE_SPACEMONSTER:
		i = add_spacemonster(x, y, z);
		break;
	default:
		break;
	}
	if (i >= 0)
		set_object_location(&go[i], x, y, z);
	pthread_mutex_unlock(&universe_mutex);
	return 0;
}

static int process_delete_item(struct game_client *c)
{
	unsigned char buffer[10];
	struct snis_entity *o;
	uint32_t oid;
	int rc, i;

	rc = read_and_unpack_buffer(c, buffer, "w", &oid);
	if (rc)
		return rc;
	if (!(c->role & ROLE_DEMON))
		return 0;
	pthread_mutex_lock(&universe_mutex);
	i = lookup_by_id(oid);
	if (i < 0)
		goto out;
	o = &go[i];
	if (o->type == OBJTYPE_SHIP1)
		goto out;
	delete_from_clients_and_server(o);
out:
	pthread_mutex_unlock(&universe_mutex);

	return 0;
}

static int process_robot_auto_manual(struct game_client *c)
{
	unsigned char buffer[10];
	unsigned char new_mode;
	struct damcon_data *d;
	int rc;

	rc = read_and_unpack_buffer(c, buffer, "b", &new_mode);
	if (rc)
		return rc;
	new_mode = !!new_mode;
	d = &bridgelist[c->bridge].damcon;
	d->robot->tsd.robot.autonomous_mode = new_mode;
	d->robot->tsd.robot.robot_state = DAMCON_ROBOT_DECIDE_LTG;
	d->robot->version++;
	return 0;
}

static int process_cycle_camera_point_of_view(struct game_client *c, uint8_t opcode, uint8_t roles, int camera_modes)
{
	unsigned char buffer[10];
	unsigned char new_mode;
	int rc;

	rc = read_and_unpack_buffer(c, buffer, "b", &new_mode);
	if (rc)
		return rc;
	new_mode = new_mode % camera_modes;
	send_packet_to_all_clients_on_a_bridge(c->shipid,
			snis_opcode_pkt("bb", opcode, new_mode), roles);
	return 0;
}

typedef uint8_t (*bytevalue_limit_function)(struct game_client *c, uint8_t value);

static uint8_t no_limit(__attribute__((unused)) struct game_client *c, uint8_t value)
{
	return value;
}

static int process_request_bytevalue_pwr(struct game_client *c, int offset,
		bytevalue_limit_function limit)
{
	unsigned char buffer[10];
	int i, rc;
	uint32_t id;
	uint8_t *bytevalue;
	uint8_t v;

	rc = read_and_unpack_buffer(c, buffer, "wb", &id, &v);
	if (rc)
		return rc;
	pthread_mutex_lock(&universe_mutex);
	i = lookup_by_id(id);
	if (i < 0) {
		pthread_mutex_unlock(&universe_mutex);
		return -1;
	}
	if (i != c->ship_index)
		snis_log(SNIS_ERROR, "i != ship index at %s:%d\n", __FILE__, __LINE__);
	bytevalue = (uint8_t *) &go[i];
	bytevalue += offset;
	v = limit(c, v);
	*bytevalue = v;
	pthread_mutex_unlock(&universe_mutex);
	return 0;
}

static int process_request_weapzoom(struct game_client *c)
{
	return process_request_bytevalue_pwr(c, offsetof(struct snis_entity, tsd.ship.weapzoom), no_limit);
}

static int process_request_reverse(struct game_client *c)
{
	unsigned char buffer[10];
	int i, rc;
	uint32_t id;
	uint8_t v;

	rc = read_and_unpack_buffer(c, buffer, "wb", &id, &v);
	if (rc)
		return rc;
	pthread_mutex_lock(&universe_mutex);
	i = lookup_by_id(id);
	if (i < 0) {
		pthread_mutex_unlock(&universe_mutex);
		return -1;
	}
	if (i != c->ship_index)
		snis_log(SNIS_ERROR, "i != ship index at %s:%d\n", __FILE__, __LINE__);
	go[i].tsd.ship.reverse = !!v;
	pthread_mutex_unlock(&universe_mutex);
	return 0;
}

static int process_nav_trident_mode(struct game_client *c)
{
	unsigned char buffer[10];
	int i, rc;
	uint32_t id;
	uint8_t v;

	rc = read_and_unpack_buffer(c, buffer, "wb", &id, &v);
	if (rc)
		return rc;
	pthread_mutex_lock(&universe_mutex);
	i = lookup_by_id(id);
	if (i < 0) {
		pthread_mutex_unlock(&universe_mutex);
		return -1;
	}
	if (i != c->ship_index)
		snis_log(SNIS_ERROR, "i != ship index at %s:%d\n", __FILE__, __LINE__);
	go[i].tsd.ship.trident = !!v;
	pthread_mutex_unlock(&universe_mutex);
	return 0;
}

static int process_adjust_control_bytevalue(struct game_client *c, uint32_t id,
				int offset, uint8_t value, bytevalue_limit_function limit)
{
	int i;
	uint8_t *bytevalue;

	pthread_mutex_lock(&universe_mutex);
	i = lookup_by_id(id);
	if (i < 0) {
		pthread_mutex_unlock(&universe_mutex);
		return -1;
	}
	if (i != c->ship_index)
		snis_log(SNIS_ERROR, "i != ship index at %s:%d\n", __FILE__, __LINE__);
	bytevalue = (uint8_t *) &go[i];
	bytevalue += offset;
	value = limit(c, value);
	*bytevalue = value;
	pthread_mutex_unlock(&universe_mutex);
	return 0;
}

/* Find in-range potential target that best matches weapons direction */
static uint32_t find_potential_missile_target(struct snis_entity *shooter)
{
	int i;
	float dist, dot, bestdot;
	int best;
	union vec3 to_target;
	union vec3 weapons_vec;
	struct snis_entity *target;
	union quat orientation;

	weapons_vec.v.x = 1.0;
	weapons_vec.v.y = 0.0;
	weapons_vec.v.z = 0.0;

	/* Calculate which way weapons is pointed, and velocity of torpedo. */
	quat_mul(&orientation, &shooter->orientation, &shooter->tsd.ship.weap_orientation);
	quat_rot_vec_self(&weapons_vec, &orientation);
	vec3_normalize_self(&weapons_vec);

	best = -1;
	bestdot = -1.0;
	for (i = 0; i <= snis_object_pool_highest_object(pool); i++) {
		target = &go[i];
		switch (target->type) {
		case OBJTYPE_SHIP1:
		case OBJTYPE_SHIP2:
		case OBJTYPE_STARBASE:
		case OBJTYPE_DERELICT:
		case OBJTYPE_CARGO_CONTAINER:
			break;
		default:
			continue;
		}
		if (target->id == shooter->id) /* Don't target self */
			continue;
		dist = object_dist2(shooter, target);
		if (dist > missile_target_dist * missile_target_dist)
			continue;
		to_target.v.x = target->x - shooter->x;
		to_target.v.y = target->y - shooter->y;
		to_target.v.z = target->z - shooter->z;
		vec3_normalize_self(&to_target);
		dot = vec3_dot(&to_target, &weapons_vec);
		if (dot < 0.98) /* Not aimed well enough. */
			continue;
		if (dot > bestdot) {
			best = i;
			bestdot = dot;
		}
	}
	if (best < 0)
		return (uint32_t) -1;
	return go[best].id;
}

static void fire_missile(struct snis_entity *shooter, uint32_t target_id)
{
	if (shooter->type == OBJTYPE_SHIP1) {
		snis_queue_add_sound(MISSILE_LAUNCH, ROLE_SOUNDSERVER, shooter->id);
		snis_queue_add_text_to_speech("Missile away.",
				ROLE_TEXT_TO_SPEECH, shooter->id);
	}
	add_missile(shooter->x, shooter->y, shooter->z, shooter->vx, shooter->vy, shooter->vz,
			target_id, shooter->id);
}

static int process_adjust_control_fire_missile(struct game_client *c, uint32_t id)
{
	int i;
	struct snis_entity *o;
	uint32_t target_id;

	pthread_mutex_lock(&universe_mutex);
	i = lookup_by_id(id);
	if (i < 0) {
		pthread_mutex_unlock(&universe_mutex);
		return -1;
	}
	if (i != c->ship_index)
		snis_log(SNIS_ERROR, "i != ship index at %s:%d\n", __FILE__, __LINE__);
	o = &go[i];
	if (o->tsd.ship.missile_count <= 0)
		goto missile_fail;
	target_id = find_potential_missile_target(o);
	if (target_id == (uint32_t) -1)
		goto missile_fail;
	fire_missile(o, target_id);
	o->tsd.ship.missile_count--;
	pthread_mutex_unlock(&universe_mutex);
	return 0;

missile_fail:
	pthread_mutex_unlock(&universe_mutex);
	snis_queue_add_sound(LASER_FAILURE, ROLE_SOUNDSERVER, o->id);
	return 0;
}

static int process_adjust_control_deploy_chaff(struct game_client *c, uint32_t id)
{
	int i;
	struct snis_entity *o;

	pthread_mutex_lock(&universe_mutex);
	i = lookup_by_id(id);
	if (i < 0) {
		pthread_mutex_unlock(&universe_mutex);
		return -1;
	}
	if (c->bridge < 0 || bridgelist[c->bridge].chaff_cooldown > 0) {
		snis_queue_add_sound(LASER_FAILURE, ROLE_SOUNDSERVER, c->shipid);
		pthread_mutex_unlock(&universe_mutex);
		return 0;
	}
	if (i != c->ship_index)
		snis_log(SNIS_ERROR, "i != ship index at %s:%d\n", __FILE__, __LINE__);
	o = &go[i];
	for (i = 0; i < chaff_count; i++)
		(void) add_chaff(o->x, o->y, o->z);
	if (c->bridge >= 0)
		bridgelist[c->bridge].chaff_cooldown = chaff_cooldown_time;
	pthread_mutex_unlock(&universe_mutex);
	return 0;
}

static int process_adjust_control_input(struct game_client *c)
{
	int rc;
	uint32_t id;
	unsigned char subcode, v;
	unsigned char buffer[10];

	rc = read_and_unpack_buffer(c, buffer, "bwb", &subcode, &id, &v);
	if (rc)
		return rc;
	switch (subcode) {
	case OPCODE_ADJUST_CONTROL_LASER_WAVELENGTH:
		return process_adjust_control_bytevalue(c, id,
				offsetof(struct snis_entity, tsd.ship.phaser_wavelength), v, no_limit);
	case OPCODE_ADJUST_CONTROL_NAVZOOM:
		return process_adjust_control_bytevalue(c, id,
				offsetof(struct snis_entity, tsd.ship.navzoom), v, no_limit);
	case OPCODE_ADJUST_CONTROL_MAINZOOM:
		return process_adjust_control_bytevalue(c, id,
				offsetof(struct snis_entity, tsd.ship.mainzoom), v, no_limit);
	case OPCODE_ADJUST_CONTROL_THROTTLE:
		return process_adjust_control_bytevalue(c, id,
			offsetof(struct snis_entity, tsd.ship.power_data.impulse.r1), v, no_limit);
	case OPCODE_ADJUST_CONTROL_SCIZOOM:
		return process_adjust_control_bytevalue(c, id,
			offsetof(struct snis_entity, tsd.ship.scizoom), v, no_limit);
	case OPCODE_ADJUST_CONTROL_WARPDRIVE:
		return process_adjust_control_bytevalue(c, id,
			offsetof(struct snis_entity, tsd.ship.power_data.warp.r1), v, no_limit);
	case OPCODE_ADJUST_CONTROL_SHIELD:
		return process_adjust_control_bytevalue(c, id,
			offsetof(struct snis_entity, tsd.ship.power_data.shields.r1), v, no_limit);
	case OPCODE_ADJUST_CONTROL_MANEUVERING_PWR:
		return process_adjust_control_bytevalue(c, id,
			offsetof(struct snis_entity, tsd.ship.power_data.maneuvering.r2), v, no_limit);
	case OPCODE_ADJUST_CONTROL_TRACTOR_PWR:
		return process_adjust_control_bytevalue(c, id,
			offsetof(struct snis_entity, tsd.ship.power_data.tractor.r2), v, no_limit);
	case OPCODE_ADJUST_CONTROL_LIFESUPPORT_PWR:
		return process_adjust_control_bytevalue(c, id,
			offsetof(struct snis_entity, tsd.ship.power_data.lifesupport.r2), v, no_limit);
	case OPCODE_ADJUST_CONTROL_SHIELDS_PWR:
		return process_adjust_control_bytevalue(c, id,
			offsetof(struct snis_entity, tsd.ship.power_data.shields.r2), v, no_limit);
	case OPCODE_ADJUST_CONTROL_IMPULSE_PWR:
		return process_adjust_control_bytevalue(c, id,
			offsetof(struct snis_entity, tsd.ship.power_data.impulse.r2), v, no_limit);
	case OPCODE_ADJUST_CONTROL_WARP_PWR:
		return process_adjust_control_bytevalue(c, id,
			offsetof(struct snis_entity, tsd.ship.power_data.warp.r2), v, no_limit);
	case OPCODE_ADJUST_CONTROL_SENSORS_PWR:
		return process_adjust_control_bytevalue(c, id,
			offsetof(struct snis_entity, tsd.ship.power_data.sensors.r2), v, no_limit);
	case OPCODE_ADJUST_CONTROL_PHASERBANKS_PWR:
		return process_adjust_control_bytevalue(c, id,
			offsetof(struct snis_entity, tsd.ship.power_data.phasers.r2), v, no_limit);
	case OPCODE_ADJUST_CONTROL_COMMS_PWR:
		return process_adjust_control_bytevalue(c, id,
			offsetof(struct snis_entity, tsd.ship.power_data.comms.r2), v, no_limit);
	case OPCODE_ADJUST_CONTROL_MANEUVERING_COOLANT:
		return process_adjust_control_bytevalue(c, id,
			offsetof(struct snis_entity, tsd.ship.coolant_data.maneuvering.r2), v, no_limit);
	case OPCODE_ADJUST_CONTROL_TRACTOR_COOLANT:
		return process_adjust_control_bytevalue(c, id,
			offsetof(struct snis_entity, tsd.ship.coolant_data.tractor.r2), v, no_limit);
	case OPCODE_ADJUST_CONTROL_LIFESUPPORT_COOLANT:
		return process_adjust_control_bytevalue(c, id,
			offsetof(struct snis_entity, tsd.ship.coolant_data.lifesupport.r2), v, no_limit);
	case OPCODE_ADJUST_CONTROL_SHIELDS_COOLANT:
		return process_adjust_control_bytevalue(c, id,
			offsetof(struct snis_entity, tsd.ship.coolant_data.shields.r2), v, no_limit);
	case OPCODE_ADJUST_CONTROL_IMPULSE_COOLANT:
		return process_adjust_control_bytevalue(c, id,
			offsetof(struct snis_entity, tsd.ship.coolant_data.impulse.r2), v, no_limit);
	case OPCODE_ADJUST_CONTROL_WARP_COOLANT:
		return process_adjust_control_bytevalue(c, id,
			offsetof(struct snis_entity, tsd.ship.coolant_data.warp.r2), v, no_limit);
	case OPCODE_ADJUST_CONTROL_SENSORS_COOLANT:
		return process_adjust_control_bytevalue(c, id,
			offsetof(struct snis_entity, tsd.ship.coolant_data.sensors.r2), v, no_limit);
	case OPCODE_ADJUST_CONTROL_PHASERBANKS_COOLANT:
		return process_adjust_control_bytevalue(c, id,
			offsetof(struct snis_entity, tsd.ship.coolant_data.phasers.r2), v, no_limit);
	case OPCODE_ADJUST_CONTROL_COMMS_COOLANT:
		return process_adjust_control_bytevalue(c, id,
			offsetof(struct snis_entity, tsd.ship.coolant_data.comms.r2), v, no_limit);
	case OPCODE_ADJUST_CONTROL_EXTERIOR_LIGHTS:
		return process_adjust_control_bytevalue(c, id,
			offsetof(struct snis_entity, tsd.ship.exterior_lights), v, no_limit);
	case OPCODE_ADJUST_CONTROL_SILENCE_ALARMS:
		return process_adjust_control_bytevalue(c, id,
			offsetof(struct snis_entity, tsd.ship.alarms_silenced), v, no_limit);
	case OPCODE_ADJUST_CONTROL_FIRE_MISSILE:
		return process_adjust_control_fire_missile(c, id);
	case OPCODE_ADJUST_CONTROL_DEPLOY_CHAFF:
		return process_adjust_control_deploy_chaff(c, id);
	default:
		return -1;
	}
	return 0;
}

static void send_initiate_warp_packet(struct game_client *c, int enough_oomph)
{
	send_packet_to_all_clients_on_a_bridge(c->shipid,
			snis_opcode_pkt("bb", OPCODE_INITIATE_WARP,
					(unsigned char) enough_oomph),
			ROLE_ALL);
}

static void send_wormhole_limbo_packet(int shipid, uint16_t value)
{
	send_packet_to_all_clients_on_a_bridge(shipid,
			snis_opcode_pkt("bh", OPCODE_WORMHOLE_LIMBO, value),
			ROLE_ALL);
}

static int process_docking_magnets(struct game_client *c)
{
	unsigned char buffer[10];
	uint32_t id;
	uint8_t __attribute__((unused)) v;
	int i;
	int sound;
	uint32_t tow_ship_id;

	int rc = read_and_unpack_buffer(c, buffer, "wb", &id, &v);
	if (rc)
		return rc;
	pthread_mutex_lock(&universe_mutex);
	i = lookup_by_id(id);
	if (i < 0) {
		pthread_mutex_unlock(&universe_mutex);
		return -1;
	}
	if (i != c->ship_index)
		snis_log(SNIS_ERROR, "i != ship index at %s:%t\n", __FILE__, __LINE__);
	go[i].tsd.ship.docking_magnets = !go[i].tsd.ship.docking_magnets;
	go[i].timestamp = universe_timestamp;
	sound = go[i].tsd.ship.docking_magnets ? DOCKING_SYSTEM_ENGAGED : DOCKING_SYSTEM_DISENGAGED;
	tow_ship_id = is_being_towed(&go[i]);
	if (!go[i].tsd.ship.docking_magnets && tow_ship_id) {
		int tow_ship_index;
		tow_ship_index = lookup_by_id(tow_ship_id);
		if (tow_ship_index >= 0)
			disconnect_from_tow_ship(&go[tow_ship_index], &go[i]);
	}
	pthread_mutex_unlock(&universe_mutex);
	/* snis_queue_add_sound(sound, ROLE_NAVIGATION, c->shipid); */
	snis_queue_add_text_to_speech(sound == DOCKING_SYSTEM_ENGAGED ?
				"Docking system engaged." : "Docking system disengaged",
				ROLE_TEXT_TO_SPEECH, c->shipid);
	return 0;
}

/* Assumes universe lock is held, and releases it. */
static void toggle_standard_orbit(struct game_client *c, struct snis_entity *ship)
{
	int planet_index;
	struct snis_entity *planet;
	double dx, dy, dz, d;

	if (ship->tsd.ship.orbiting_object_id != 0xffffffff) {
		ship->tsd.ship.orbiting_object_id = 0xffffffff;
		ship->tsd.ship.computer_steering_time_left = 0; /* turn off computer steering */
		pthread_mutex_unlock(&universe_mutex);
		snis_queue_add_text_to_speech("Leaving standard orbit",
					ROLE_TEXT_TO_SPEECH, c->shipid);
		return;
	}
	/* Find nearest planet to ship */
	planet_index = nl_find_nearest_object_of_type(c->shipid, OBJTYPE_PLANET);
	if (planet_index < 0) { /* no planets around */
		pthread_mutex_unlock(&universe_mutex);
		snis_queue_add_text_to_speech("There are no nearby planets to orbit",
					ROLE_TEXT_TO_SPEECH, c->shipid);
		return;
	}
	planet = &go[planet_index];
	dx = planet->x - ship->x;
	dy = planet->y - ship->y;
	dz = planet->z - ship->z;
	d = sqrt(dx * dx + dy * dy + dz * dz);
	if (d > planet->tsd.planet.radius * standard_orbit * 3.00) {
		pthread_mutex_unlock(&universe_mutex);
		snis_queue_add_text_to_speech("We are too far from a planet to enter standard orbit",
					ROLE_TEXT_TO_SPEECH, c->shipid);
		printf("radius = %lf, d = %lf, limit = %lf\n", planet->tsd.planet.radius, d,
			planet->tsd.planet.radius * standard_orbit);
		return;
	}
	ship->tsd.ship.orbiting_object_id = planet->id;
	pthread_mutex_unlock(&universe_mutex);
	snis_queue_add_text_to_speech("Entering standard orbit",
				ROLE_TEXT_TO_SPEECH, c->shipid);
	return;
}

static int process_standard_orbit(struct game_client *c)
{
	unsigned char buffer[10];
	uint8_t v;
	uint32_t id;
	int i;
	struct snis_entity *ship;

	int rc = read_and_unpack_buffer(c, buffer, "wb", &id, &v);
	if (rc)
		return rc;
	pthread_mutex_lock(&universe_mutex);
	i = lookup_by_id(id);
	if (i < 0) {
		pthread_mutex_unlock(&universe_mutex);
		return -1;
	}
	if (i != c->ship_index)
		snis_log(SNIS_ERROR, "i != ship index at %s:%t\n", __FILE__, __LINE__);
	ship = &go[i];
	toggle_standard_orbit(c, ship);
	return 0;
}

static int process_request_starmap(struct game_client *c)
{
	unsigned char buffer[10];
	uint8_t v;
	uint32_t id;
	int i;
	struct snis_entity *ship;

	int rc = read_and_unpack_buffer(c, buffer, "wb", &id, &v);
	if (rc)
		return rc;
	pthread_mutex_lock(&universe_mutex);
	i = lookup_by_id(id);
	if (i < 0) {
		pthread_mutex_unlock(&universe_mutex);
		return -1;
	}
	if (i != c->ship_index)
		snis_log(SNIS_ERROR, "i != ship index at %s:%t\n", __FILE__, __LINE__);
	ship = &go[i];
	ship->tsd.ship.nav_mode = !ship->tsd.ship.nav_mode;
	pthread_mutex_unlock(&universe_mutex);
	return 0;
}

static int do_engage_warp_drive(struct snis_entity *o)
{
	int enough_oomph = ((double) o->tsd.ship.warpdrive / 255.0) > 0.075;
	int b;
	const union vec3 rightvec = { { 1.0f, 0.0f, 0.0f, } };
	union vec3 warpvec;
	double wfactor;

	if (o->tsd.ship.warp_core_status == WARP_CORE_STATUS_EJECTED)
		enough_oomph = 0;
	b = lookup_bridge_by_shipid(o->id);
	if (b < 0) {
		return 0;
	}
	if (enough_oomph) {
		wfactor = ((double) o->tsd.ship.warpdrive / 255.0) * (XKNOWN_DIM / 2.0);
		quat_rot_vec(&warpvec, &rightvec, &o->orientation);
		/* At this point, warp[xyz] holds coordinates we are warping TO */
		bridgelist[b].warpx = o->x + wfactor * warpvec.v.x;
		bridgelist[b].warpy = o->y + wfactor * warpvec.v.y;
		bridgelist[b].warpz = o->z + wfactor * warpvec.v.z;
		o->tsd.ship.warp_time = PLAYER_WARP_SPINUP_TIME;
	}
	return enough_oomph;
}

static int process_engage_warp(struct game_client *c)
{
	unsigned char buffer[10];
	int i, rc;
	uint32_t id;
	uint8_t __attribute__((unused)) v;
	struct snis_entity *o;
	int enough_oomph;

	rc = read_and_unpack_buffer(c, buffer, "wb", &id, &v);
	if (rc)
		return rc;
	pthread_mutex_lock(&universe_mutex);
	i = lookup_by_id(id);
	if (i < 0) {
		pthread_mutex_unlock(&universe_mutex);
		return -1;
	}
	if (i != c->ship_index)
		snis_log(SNIS_ERROR, "i != ship index at %s:%t\n", __FILE__, __LINE__);
	o = &go[i];
	if (o->tsd.ship.warp_time >= 0) {/* already engaged */
		pthread_mutex_unlock(&universe_mutex);
		return 0;
	}
	enough_oomph = do_engage_warp_drive(o);
	pthread_mutex_unlock(&universe_mutex);
	send_initiate_warp_packet(c, enough_oomph);
	return 0;
}

static int process_request_yaw(struct game_client *c, do_yaw_function yaw_func)
{
	unsigned char buffer[10];
	uint8_t yaw;
	int rc;

	rc = read_and_unpack_buffer(c, buffer, "b", &yaw);
	if (rc)
		return rc;
	switch (yaw) {
	case YAW_LEFT:
		yaw_func(c, 1);
		break;
	case YAW_RIGHT:
		yaw_func(c, -1);
		break;
	case YAW_LEFT_FINE:
		yaw_func(c, 2);
		break;
	case YAW_RIGHT_FINE:
		yaw_func(c, -2);
		break;
	default:
		break;
	}
	return 0;
}

static int process_request_rot(struct game_client *c, do_mouse_rot_function rot_func)
{
	unsigned char buffer[10];
	float x, y;
	int rc;

	rc = read_and_unpack_buffer(c, buffer, "qq", &x, &y);
	if (rc)
		return rc;
	/*debugging noises*/
	struct snis_entity *ship = &go[c->ship_index];
	snis_queue_add_sound(DOCKING_SOUND, ROLE_ALL, ship->id);

	rot_func(c, x, y);
	return 0;
}

static int process_demon_rot(struct game_client *c)
{
	unsigned char buffer[10];
	struct snis_entity *o;
	uint32_t oid;
	uint8_t subcmd, amount;
	int i, rc;

	rc = read_and_unpack_buffer(c, buffer, "bwb", &subcmd, &oid, &amount);
	if (rc)
		return rc;
	if (!(c->role & ROLE_DEMON))
		return 0;
	pthread_mutex_lock(&universe_mutex);
	i = lookup_by_id(oid);
	if (i < 0)
		goto out;
	o = &go[i];

	switch (subcmd) {
	case OPCODE_DEMON_ROT_YAW: {
		switch (amount) {
		case YAW_LEFT:
			do_demon_yaw(o, 1);
			break;
		case YAW_RIGHT:
			do_demon_yaw(o, -1);
			break;
		case YAW_LEFT_FINE:
			do_demon_yaw(o, 2);
			break;
		case YAW_RIGHT_FINE:
			do_demon_yaw(o, -2);
			break;
		default:
			break;
		}
		break;
	}
	case OPCODE_DEMON_ROT_PITCH: {
		switch (amount) {
		case PITCH_FORWARD:
			do_demon_pitch(o, 1);
			break;
		case PITCH_BACK:
			do_demon_pitch(o, -1);
			break;
		case PITCH_FORWARD_FINE:
			do_demon_pitch(o, 2);
			break;
		case PITCH_BACK_FINE:
			do_demon_pitch(o, -2);
			break;
		default:
			break;
		}
		break;
	}
	case OPCODE_DEMON_ROT_ROLL: {
		switch (amount) {
		case ROLL_LEFT:
			do_demon_roll(o, 1);
			break;
		case ROLL_RIGHT:
			do_demon_roll(o, -1);
			break;
		case ROLL_LEFT_FINE:
			do_demon_roll(o, 2);
			break;
		case ROLL_RIGHT_FINE:
			do_demon_roll(o, -2);
			break;
		default:
			break;
		}
		break;
	}
	default:
		break;
	}
out:
	pthread_mutex_unlock(&universe_mutex);
	return 0;
}

static int process_load_torpedo(struct game_client *c)
{
	struct snis_entity *ship = &go[c->ship_index];

	if (ship->tsd.ship.torpedoes == 0)
		return -1;
	if (ship->tsd.ship.torpedoes_loading != 0)
		return -1;
	if (ship->tsd.ship.torpedoes_loaded + ship->tsd.ship.torpedoes_loading >= 2)
		return -1;
	ship->tsd.ship.torpedoes--;
	ship->tsd.ship.torpedoes_loading++;
	ship->tsd.ship.torpedo_load_time = TORPEDO_LOAD_SECONDS * 10;
	snis_queue_add_sound(TORPEDO_LOAD_SOUND, ROLE_SOUNDSERVER, ship->id);
	return 0;
}

static int process_eject_warp_core(struct game_client *c)
{
	struct snis_entity *ship = &go[c->ship_index];
	union vec3 relvel = { { 1.0, 0.0, 0.0, }, };
	int b = c->bridge;

	if (ship->tsd.ship.warp_core_status == WARP_CORE_STATUS_EJECTED) {
		snis_queue_add_text_to_speech("The warp core has already been ejected.",
						ROLE_TEXT_TO_SPEECH, ship->id);
		return 0; /* Warp core is already ejected */
	}
	snis_queue_add_text_to_speech("Ejecting the warp core.", ROLE_TEXT_TO_SPEECH, ship->id);
	quat_rot_vec_self(&relvel, &ship->orientation);
	add_warp_core(ship->x, ship->y, ship->z,
			ship->vx + relvel.v.x, ship->vy + relvel.v.y, ship->vz + relvel.v.z,
			bridgelist[b].warp_core_critical_timer, ship->id);
	bridgelist[b].warp_core_critical = 0; /* Save player from warp core breach */
	ship->tsd.ship.warp_core_status = WARP_CORE_STATUS_EJECTED;
	return 0;
}

static int process_request_torpedo(struct game_client *c)
{
	struct snis_entity *ship = &go[c->ship_index];
	union vec3 forwardvec = { { torpedo_velocity, 0.0f, 0.0f } };
	union vec3 velocity;
	union quat orientation;

	pthread_mutex_lock(&universe_mutex);
	if (ship->tsd.ship.torpedoes_loaded <= 0)
		goto torpedo_fail;

	/* Calculate which way weapons is pointed, and velocity of torpedo. */
	quat_mul(&orientation, &ship->orientation, &ship->tsd.ship.weap_orientation);
	quat_rot_vec(&velocity, &forwardvec, &orientation);

	/* Add ship velocity into torpedo velocity */
	velocity.v.x += ship->vx;
	velocity.v.y += ship->vy;
	velocity.v.z += ship->vz;

	add_torpedo(ship->x, ship->y, ship->z,
			velocity.v.x, velocity.v.y, velocity.v.z, ship->id);
	ship->tsd.ship.torpedoes_loaded--;
	snis_queue_add_sound(LASER_FIRE_SOUND, ROLE_SOUNDSERVER, ship->id);
	pthread_mutex_unlock(&universe_mutex);
	return 0;

torpedo_fail:
	snis_queue_add_sound(LASER_FAILURE, ROLE_SOUNDSERVER, ship->id);
	pthread_mutex_unlock(&universe_mutex);
	return 0;
}

static int process_demon_fire_torpedo(struct game_client *c)
{
	struct snis_entity *o;
	unsigned char buffer[10];
	uint32_t oid;
	int i, rc;
	union vec3 tv = { { 1.0, 0.0, 0.0 } };

	rc = read_and_unpack_buffer(c, buffer, "w", &oid);
	if (rc)
		return rc;
	if (!(c->role & ROLE_DEMON))
		return 0;
	pthread_mutex_lock(&universe_mutex);
	i = lookup_by_id(oid);
	if (i < 0)
		goto out;
	o = &go[i];
	if (o->type != OBJTYPE_SHIP2)
		goto out;

	quat_rot_vec_self(&tv, &o->orientation);
	vec3_mul_self(&tv, torpedo_velocity);
	tv.v.x += o->vx;
	tv.v.y += o->vy;
	tv.v.z += o->vz;
	add_torpedo(o->x, o->y, o->z, tv.v.x, tv.v.y, tv.v.z, o->id);
out:
	pthread_mutex_unlock(&universe_mutex);

	return 0;
}

static int process_demon_change_possession(struct game_client *c, int possessed)
{
	struct snis_entity *o;
	unsigned char buffer[10];
	uint32_t oid;
	int i, rc;

	rc = read_and_unpack_buffer(c, buffer, "w", &oid);
	if (rc)
		return rc;
	if (!(c->role & ROLE_DEMON))
		return 0;
	pthread_mutex_lock(&universe_mutex);
	i = lookup_by_id(oid);
	if (i < 0)
		goto out;
	o = &go[i];
	if (o->type != OBJTYPE_SHIP2 && o->type != OBJTYPE_STARBASE)
		goto out;
	if (possessed)
		o->move = demon_ship_move;
	else
		o->move = ship_move;
out:
	pthread_mutex_unlock(&universe_mutex);
	return 0;
}

static int process_demon_possess(struct game_client *c)
{
	return process_demon_change_possession(c, 1);
}

static int process_demon_dispossess(struct game_client *c)
{
	return process_demon_change_possession(c, 0);
}

static int process_request_laser(struct game_client *c)
{
	struct snis_entity *ship = &go[c->ship_index];
	int tid;

	pthread_mutex_lock(&universe_mutex);
	if (ship->tsd.ship.ai[0].u.attack.victim_id < 0)
		goto laserfail;

	tid = ship->tsd.ship.ai[0].u.attack.victim_id;
	add_laserbeam(ship->id, tid, LASERBEAM_DURATION);
	snis_queue_add_sound(LASER_FIRE_SOUND, ROLE_SOUNDSERVER, ship->id);
	pthread_mutex_unlock(&universe_mutex);
	return 0;

laserfail:
	snis_queue_add_sound(LASER_FAILURE, ROLE_SOUNDSERVER, ship->id);
	pthread_mutex_unlock(&universe_mutex);
	return 0;
}

static int process_request_manual_laser(struct game_client *c)
{
	struct snis_entity *ship = &go[c->ship_index];
	union vec3 forwardvec = { { LASER_VELOCITY, 0.0f, 0.0f } };
	union vec3 turret_pos = { { -4.0f * SHIP_MESH_SCALE, 5.45 * SHIP_MESH_SCALE, 0.0f } };
	union vec3 barrel_r_offset = { { 4.0f * SHIP_MESH_SCALE, 0.0f, 1.4f * SHIP_MESH_SCALE } };
	union vec3 barrel_l_offset = { { 4.0f * SHIP_MESH_SCALE, 0.0f, -1.4f * SHIP_MESH_SCALE } };
	union vec3 velocity;
	union quat orientation;

	pthread_mutex_lock(&universe_mutex);
	if (ship->tsd.ship.phaser_charge <= 0) {
		pthread_mutex_unlock(&universe_mutex);
		return 0;
	}

	/* Calculate which way weapons is pointed, and velocity of laser. */
	quat_rot_vec_self(&turret_pos, &ship->orientation);

	quat_mul(&orientation, &ship->orientation, &ship->tsd.ship.weap_orientation);
	quat_rot_vec(&velocity, &forwardvec, &orientation);
	quat_rot_vec_self(&barrel_r_offset, &orientation);
	quat_rot_vec_self(&barrel_l_offset, &orientation);

	/* Add ship velocity into laser velocity */
	velocity.v.x += ship->vx;
	velocity.v.y += ship->vy;
	velocity.v.z += ship->vz;

	union vec3 right_bolt = { { ship->x, ship->y, ship->z } };
	vec3_add_self(&right_bolt, &turret_pos);
	vec3_add_self(&right_bolt, &barrel_r_offset);

	union vec3 left_bolt = { { ship->x, ship->y, ship->z } };
	vec3_add_self(&left_bolt, &turret_pos);
	vec3_add_self(&left_bolt, &barrel_l_offset);

	add_laser(right_bolt.v.x, right_bolt.v.y, right_bolt.v.z,
			velocity.v.x, velocity.v.y, velocity.v.z,
			&orientation, ship->id);
	add_laser(left_bolt.v.x, left_bolt.v.y, left_bolt.v.z,
			velocity.v.x, velocity.v.y, velocity.v.z,
			&orientation, ship->id);
	ship->tsd.ship.phaser_charge = 0;
	snis_queue_add_sound(LASER_FIRE_SOUND, ROLE_SOUNDSERVER, ship->id);
	pthread_mutex_unlock(&universe_mutex);
	return 0;

#if 0
	/* resurrect this code for laser overheating later? */
laserfail:
	snis_queue_add_sound(LASER_FAILURE, ROLE_SOUNDSERVER, ship->id);
	pthread_mutex_unlock(&universe_mutex);
	return 0;
#endif
}

static int turn_off_tractorbeam(struct snis_entity *ship)
{
	int i;
	struct snis_entity *tb;

	/* universe lock must be held already. */
	if (ship->tsd.ship.tractor_beam == (uint32_t) 0xffffffff)
		return 0;
	i = lookup_by_id(ship->tsd.ship.tractor_beam);
	if (i < 0) {
		/* thing we were tractoring died. */
		ship->tsd.ship.tractor_beam = -1;
		return 0;
	}
	tb = &go[i];
	delete_from_clients_and_server(tb);
	ship->tsd.ship.tractor_beam = -1;
	return 1;
}

static int turn_on_tractor_beam(struct game_client *c, struct snis_entity *ship, uint32_t oid, int toggle)
{
	int i;

	pthread_mutex_lock(&universe_mutex);
	if (oid == (uint32_t) 0xffffffff && c)
		oid = bridgelist[c->bridge].science_selection;
	if (oid == (uint32_t) 0xffffffff) { /* nothing selected, turn off tractor beam */
		if (toggle) {
			turn_off_tractorbeam(ship);
			pthread_mutex_unlock(&universe_mutex);
			return 0;
		} else {
			pthread_mutex_unlock(&universe_mutex);
			if (c)
				queue_add_text_to_speech(c, "No target for tractor beam, beam not engaged.");
			return 0;
		}
	}
	i = lookup_by_id(oid);
	if (i < 0)
		goto tractorfail;

	/* If something is already tractored, turn off beam... */
	if (ship->tsd.ship.tractor_beam != -1) {
		i = lookup_by_id(ship->tsd.ship.tractor_beam);
		if (i >= 0 && oid == go[i].tsd.laserbeam.target) {
			/* if same thing selected, turn off beam and we're done */
			if (toggle)
				turn_off_tractorbeam(ship);
			pthread_mutex_unlock(&universe_mutex);
			if (!toggle && c)
				queue_add_text_to_speech(c, "The tractor beam is already engaged");
			return 0;
		}
		/* otherwise turn beam off then back on to newly selected item */
		turn_off_tractorbeam(ship);
	}

	/* TODO: check tractor beam energy here */
	if (0) 
		goto tractorfail;

	/* TODO check tractor distance here */
	if (0)
		goto tractorfail;

	add_tractorbeam(ship, oid, 30);
	/* TODO: tractor beam sound here. */
	pthread_mutex_unlock(&universe_mutex);
	if (!toggle && c)
		queue_add_text_to_speech(c, "Tractor beam engaged");
	return 0;

tractorfail:
	/* TODO: make special tractor failure sound */
	snis_queue_add_sound(LASER_FAILURE, ROLE_SOUNDSERVER, ship->id);
	pthread_mutex_unlock(&universe_mutex);
	return 0;
}

static int process_request_tractor_beam(struct game_client *c)
{
	unsigned char buffer[10];
	struct snis_entity *ship = &go[c->ship_index];
	uint32_t oid;
	int rc;

	rc = read_and_unpack_buffer(c, buffer, "w", &oid);
	if (rc)
		return rc;
	return turn_on_tractor_beam(NULL, ship, oid, 1);
}

static void launch_mining_bot(struct game_client *c, struct snis_entity *ship, uint32_t oid)
{
	int i;
	char *message = "What?";

	pthread_mutex_lock(&universe_mutex);
	if (oid == (uint32_t) 0xffffffff && bridgelist[c->bridge].selected_waypoint == -1) { /* nothing selected */
		message = "No target selected for miniing robot.";
		goto miningbotfail;
	}
	if (ship->tsd.ship.mining_bots <= 0) { /* no bots left */
		message = "No mining bots remain to be launched.";
		goto miningbotfail;
	}
	if (bridgelist[c->bridge].selected_waypoint == -1) {
		i = lookup_by_id(oid);
		if (i < 0) {
			message = "Malfunction detected. The mining robot's navigation system has failed to start.";
			goto miningbotfail;
		}
		if (go[i].type != OBJTYPE_ASTEROID && go[i].type != OBJTYPE_DERELICT &&
			go[i].type != OBJTYPE_CARGO_CONTAINER) {
			message = "No appropriate target selected for miniing robot.";
			goto miningbotfail;
		}
	} else {
		if (bridgelist[c->bridge].selected_waypoint < 0 ||
			bridgelist[c->bridge].selected_waypoint >= bridgelist[c->bridge].nwaypoints) {
			message = "Invalid waypoint selected for mining robot.";
			goto miningbotfail;
		}
	}

	i = add_mining_bot(ship, oid, c->bridge, bridgelist[c->bridge].selected_waypoint);
	if (i < 0) {
		message = "Malfunction detected. The mining robot's thrusters have failed to ignite.";
		goto miningbotfail;
	}
	schedule_callback3(event_callback, &callback_schedule,
		"mining-bot-deployed-event", (double) ship->id, (double) go[i].id,
			oid == (uint32_t) -1 ? -1.0 : (double) oid);
	pthread_mutex_unlock(&universe_mutex);
	switch (go[i].type) {
	case OBJTYPE_ASTEROID:
		queue_add_text_to_speech(c, "Mining robot deployed to mine selected asteroid.");
		break;
	case OBJTYPE_DERELICT:
		queue_add_text_to_speech(c, "Mining robot deployed to salvage selected derelict wreckage.");
		break;
	case OBJTYPE_CARGO_CONTAINER:
		queue_add_text_to_speech(c, "Mining robot deployed to salvage selected cargo container.");
		break;
	default:
		/* Shouldn't get here. */
		queue_add_text_to_speech(c, "Mining robot deployed.");
		break;
	}
	return;

miningbotfail:
	/* TODO: make special miningbot failure sound */
	snis_queue_add_sound(LASER_FAILURE, ROLE_SOUNDSERVER, ship->id);
	pthread_mutex_unlock(&universe_mutex);
	queue_add_text_to_speech(c, message);
	return;
}

static int process_request_mining_bot(struct game_client *c)
{
	unsigned char buffer[10];
	struct snis_entity *ship = &go[c->ship_index];
	uint32_t oid;
	int rc;

	rc = read_and_unpack_buffer(c, buffer, "w", &oid);
	if (rc)
		return rc;
	launch_mining_bot(c, ship, oid);
	return 0;
}

static int process_demon_fire_phaser(struct game_client *c)
{
	struct snis_entity *o;
	union vec3 vel;
	unsigned char buffer[10];
	uint32_t oid;
	int i, rc;

	rc = read_and_unpack_buffer(c, buffer, "w", &oid);
	if (rc)
		return rc;
	if (!(c->role & ROLE_DEMON))
		return 0;
	pthread_mutex_lock(&universe_mutex);
	i = lookup_by_id(oid);
	if (i < 0)
		goto out;
	o = &go[i];
	if (o->type != OBJTYPE_SHIP2)
		goto out;
	vel.v.x = LASER_VELOCITY;
	vel.v.y = 0;
	vel.v.z = 0;
	quat_rot_vec_self(&vel, &o->orientation);
	add_laser(o->x, o->y, o->z, vel.v.x, vel.v.y, vel.v.z, NULL, o->id);
	o->tsd.ship.phaser_charge = 0;
out:
	pthread_mutex_unlock(&universe_mutex);

	return 0;
}

static void process_instructions_from_client(struct game_client *c)
{
	int rc;
	uint8_t opcode;
	static uint8_t last_successful_opcode = OPCODE_NOOP;

	opcode = OPCODE_NOOP;
	rc = snis_readsocket(c->socket, &opcode, sizeof(opcode));
	if (rc != 0) {
		if (errno == 0) { /* Client has performed orderly shutdown */
			fprintf(stderr, "%s: client on socket %d performed orderly shutdown\n",
				logprefix(), c->socket);
			goto orderly_client_shutdown;
		}
		fprintf(stderr, "%s: snis_readsocket() returned %d, errno = %s\n",
			logprefix(), rc, strerror(errno));
		goto protocol_error;
	}

	switch (opcode) {
		case OPCODE_REQUEST_TORPEDO:
			process_request_torpedo(c);
			break;
		case OPCODE_REQUEST_TRACTORBEAM:
			process_request_tractor_beam(c);
			break;
		case OPCODE_REQUEST_MINING_BOT:
			process_request_mining_bot(c);
			break;
		case OPCODE_DEMON_FIRE_TORPEDO:
			process_demon_fire_torpedo(c);
			break;
		case OPCODE_DEMON_POSSESS:
			process_demon_possess(c);
			break;
		case OPCODE_DEMON_DISPOSSESS:
			process_demon_dispossess(c);
			break;
		case OPCODE_REQUEST_LASER:
			process_request_laser(c);
			break;
		case OPCODE_REQUEST_MANUAL_LASER:
			process_request_manual_laser(c);
			break;
		case OPCODE_DEMON_FIRE_PHASER:
			process_demon_fire_phaser(c);
			break;
		case OPCODE_LOAD_TORPEDO:
			process_load_torpedo(c);
			break;
		case OPCODE_EJECT_WARP_CORE:
			process_eject_warp_core(c);
			break;
		case OPCODE_REQUEST_YAW:
			rc = process_request_yaw(c, do_yaw);
			if (rc)
				goto protocol_error;
			break;
		case OPCODE_REQUEST_PITCH:
			rc = process_request_yaw(c, do_pitch); /* process_request_yaw is correct */
			if (rc)
				goto protocol_error;
			break;
		case OPCODE_REQUEST_ROLL:
			rc = process_request_yaw(c, do_roll); /* process_request_yaw is correct */
			if (rc)
				goto protocol_error;
			break;
		case OPCODE_REQUEST_SCIBALL_YAW:
			rc = process_request_yaw(c, do_sciball_yaw);
			if (rc)
				goto protocol_error;
			break;
		case OPCODE_REQUEST_SCIBALL_PITCH:
			rc = process_request_yaw(c, do_sciball_pitch); /* process_request_yaw is correct */
			if (rc)
				goto protocol_error;
			break;
		case OPCODE_REQUEST_SCIBALL_ROLL:
			rc = process_request_yaw(c, do_sciball_roll); /* process_request_yaw is correct */
			if (rc)
				goto protocol_error;
			break;
		case OPCODE_REQUEST_SCIBALL_ROT:
			rc = process_request_rot(c, do_sciball_mouse_rot);
			if (rc)
				goto protocol_error;
			break;
		case OPCODE_DEMON_ROT:
			rc = process_demon_rot(c);
			if (rc)
				goto protocol_error;
			break;
		case OPCODE_ENGAGE_WARP:
			rc = process_engage_warp(c);
			if (rc)
				goto protocol_error;
			break;
		case OPCODE_DOCKING_MAGNETS:
			rc = process_docking_magnets(c);
			if (rc)
				goto protocol_error;
			break;
		case OPCODE_REQUEST_STANDARD_ORBIT:
			rc = process_standard_orbit(c);
			if (rc)
				goto protocol_error;
			break;
		case OPCODE_REQUEST_STARMAP:
			rc = process_request_starmap(c);
			if (rc)
				goto protocol_error;
			break;
		case OPCODE_ADJUST_CONTROL_INPUT:
			rc = process_adjust_control_input(c);
			if (rc)
				goto protocol_error;
			break;
		case OPCODE_REQUEST_MANUAL_GUNYAW:
			rc = process_request_yaw(c, do_manual_gunyaw);
			if (rc)
				goto protocol_error;
			break;
		case OPCODE_REQUEST_MANUAL_GUNPITCH:
			rc = process_request_yaw(c, do_manual_gunpitch);
			if (rc)
				goto protocol_error;
			break;
		case OPCODE_REQUEST_SCIYAW:
			rc = process_request_yaw(c, do_sci_yaw);
			if (rc)
				goto protocol_error;
			break;
		case OPCODE_REQUEST_ROBOT_YAW:
			rc = process_request_yaw(c, do_robot_yaw);
			if (rc)
				goto protocol_error;
			break;
		case OPCODE_REQUEST_ROBOT_CMD:
			rc = process_request_robot_cmd(c);
			if (rc)
				goto protocol_error;
			break;
		case OPCODE_REQUEST_WEAPZOOM:
			rc = process_request_weapzoom(c);
			if (rc)
				goto protocol_error;
			break;
		case OPCODE_REQUEST_SCIBEAMWIDTH:
			rc = process_request_yaw(c, do_sci_bw_yaw);
			if (rc)
				goto protocol_error;
			break;
		case OPCODE_REQUEST_REVERSE:
			rc = process_request_reverse(c);
			if (rc)
				goto protocol_error;
			break;
		case OPCODE_NAV_TRIDENT_MODE:
			rc = process_nav_trident_mode(c);
			if (rc)
				goto protocol_error;
			break;
		case OPCODE_DEMON_THRUST:
			rc = process_demon_thrust(c);
			if (rc)
				goto protocol_error;
			break;
		case OPCODE_REQUEST_ROBOT_THRUST:
			rc = process_request_robot_thrust(c);
			if (rc)
				goto protocol_error;
			break;
		case OPCODE_DEMON_MOVE_OBJECT:
			rc = process_demon_move_object(c);
			if (rc)
				goto protocol_error;
			break;
		case OPCODE_NOOP:
			break;
		case OPCODE_ROLE_ONSCREEN:
			rc = process_role_onscreen(c);
			if (rc)
				goto protocol_error;
			break;
		case OPCODE_SCI_SELECT_TARGET:
			rc = process_sci_select_target(c);
			if (rc)
				goto protocol_error;
			break;
		case OPCODE_SCI_DETAILS:
			rc = process_sci_details(c);
			if (rc)
				goto protocol_error;
			break;
		case OPCODE_SCI_ALIGN_TO_SHIP:
			rc = process_sci_align_to_ship(c);
			if (rc)
				goto protocol_error;
			break;
		case OPCODE_REQUEST_WEAPONS_YAW_PITCH:
			rc = process_request_weapons_yaw_pitch(c);
			if (rc)
				goto protocol_error;
			break;
		case OPCODE_WEAP_SELECT_TARGET:
			rc = process_weap_select_target(c);
			if (rc)
				goto protocol_error;
			break;
		case OPCODE_COMMS_TRANSMISSION:
			rc = process_comms_transmission(c, 1);
			if (rc)
				goto protocol_error;
			break;
		case OPCODE_DEMON_COMMS_XMIT:
			rc = process_comms_transmission(c, 0);
			if (rc)
				goto protocol_error;
			break;
		case OPCODE_DEMON_COMMAND:
			rc = process_demon_command(c);
			if (rc)
				goto protocol_error;
			break;
		case OPCODE_REQUEST_ROBOT_GRIPPER:
			rc = process_request_robot_gripper(c);
			if (rc)
				goto protocol_error;
			break;
		case OPCODE_MAINSCREEN_VIEW_MODE:
			rc = process_mainscreen_view_mode(c);
			if (rc)
				goto protocol_error;
			break;
		case OPCODE_REQUEST_REDALERT:
			rc = process_request_redalert(c);
			if (rc)
				goto protocol_error;
			break;
		case OPCODE_COMMS_MAINSCREEN:
			rc = process_comms_mainscreen(c);
			if (rc)
				goto protocol_error;
			break;
		case OPCODE_CREATE_ITEM:
			rc = process_create_item(c);
			if (rc)
				goto protocol_error;
			break;
		case OPCODE_DELETE_OBJECT:
			rc = process_delete_item(c);
			if (rc)
				goto protocol_error;
			break;
		case OPCODE_DEMON_CLEAR_ALL:
			process_demon_clear_all();
			break;
		case OPCODE_TOGGLE_DEMON_AI_DEBUG_MODE:
			process_toggle_demon_ai_debug_mode(c);
			break;
		case OPCODE_TOGGLE_DEMON_SAFE_MODE:
			process_toggle_demon_safe_mode();
			break;
		case OPCODE_EXEC_LUA_SCRIPT:
			rc = process_exec_lua_script(c);
			if (rc)
				goto protocol_error;
			break;
		case OPCODE_ENSCRIPT:
			rc = process_enscript_command(c);
			if (rc)
				goto protocol_error;
			break;
		case OPCODE_ROBOT_AUTO_MANUAL:
			rc = process_robot_auto_manual(c);
			if (rc)
				goto protocol_error;
			break;
		case OPCODE_CYCLE_MAINSCREEN_POINT_OF_VIEW:
			rc = process_cycle_camera_point_of_view(c, opcode, ROLE_MAIN, 3);
			if (rc)
				goto protocol_error;
			break;
		case OPCODE_CYCLE_NAV_POINT_OF_VIEW:
			rc = process_cycle_camera_point_of_view(c, opcode, ROLE_NAVIGATION, 5);
			if (rc)
				goto protocol_error;
			break;
		case OPCODE_REQUEST_UNIVERSE_TIMESTAMP:
			rc = process_request_universe_timestamp(c);
			if (rc)
				goto protocol_error;
			break;
		case OPCODE_LATENCY_CHECK:
			rc = process_latency_check(c);
			if (rc)
				goto protocol_error;
			break;
		case OPCODE_UPDATE_BUILD_INFO:
			rc = process_build_info(c);
			if (rc)
				goto protocol_error;
			break;
		case OPCODE_NATURAL_LANGUAGE_REQUEST:
			rc = process_natural_language_request(c);
			if (rc)
				goto protocol_error;
			break;
		case OPCODE_TEXTSCREEN_OP:
			rc = process_textscreen_op(c);
			if (rc)
				goto protocol_error;
			break;
		case OPCODE_CHECK_OPCODE_FORMAT:
			rc = process_check_opcode_format(c);
			if (rc)
				goto protocol_error;
			break;
		case OPCODE_SET_WAYPOINT:
			rc = process_set_waypoint(c);
			if (rc)
				goto protocol_error;
			break;
		case OPCODE_DEMON_RTSMODE:
			rc = process_demon_rtsmode(c);
			if (rc)
				goto protocol_error;
			break;
		case OPCODE_RTS_FUNC:
			rc = process_rts_func(c);
			if (rc)
				goto protocol_error;
			break;
		case OPCODE_CUSTOM_BUTTON:
			rc = process_custom_button(c);
			if (rc)
				goto protocol_error;
			break;
		case OPCODE_CLIENT_CONFIG:
			rc = process_client_config(c);
			if (rc)
				goto protocol_error;
			break;
		case OPCODE_COMMS_CRYPTO:
			rc = process_comms_crypto(c);
			if (rc)
				goto protocol_error;
			break;
		default:
			goto protocol_error;
	}
	last_successful_opcode = opcode;
	return;

protocol_error:
	fprintf(stderr, "%s: bad opcode, protocol violation on socket %d\n", logprefix(), c->socket);
	fprintf(stderr, "%s: Protocol error in process_instructions_from_client, opcode = %hu\n",
		logprefix(), opcode);
	fprintf(stderr, "%s: Last successful opcode was %d (0x%hx)\n", logprefix(), last_successful_opcode,
			last_successful_opcode);
	snis_print_last_buffer("snis_server: ", c->socket);
orderly_client_shutdown:
	shutdown(c->socket, SHUT_RDWR);
	close(c->socket);
	c->socket = -1;
	return;
}

static void *per_client_read_thread(void /* struct game_client */ *client)
{
	struct game_client *c = (struct game_client *) client;

	/* Wait for client[] array to get fully updated before proceeding. */
	client_lock();
	get_client(c);
	client_unlock();
	log_client_info(SNIS_WARN, c->socket, "client reader thread processing opcodes\n");
	while (1) {
		process_instructions_from_client(c);
		if (c->socket < 0)
			break;
	}
	log_client_info(SNIS_WARN, c->socket, "client reader thread exiting\n");
	pthread_mutex_lock(&universe_mutex);
	client_lock();
	put_client(c);
	client_unlock();
	pthread_mutex_unlock(&universe_mutex);
	return NULL;
}

static void queue_update_universe_timestamp(struct game_client *c, uint8_t code)
{
	/* send the timestamp and time_delta.  time_delta should be 0.0 - 0.1 seconds, marshal as 5 to be safe */
	pb_prepend_queue_to_client(c, snis_opcode_pkt("bbwS", OPCODE_UPDATE_UNIVERSE_TIMESTAMP, code,
		universe_timestamp, time_now_double() - universe_timestamp_absolute, 5));
}

static void write_queued_updates_to_client(struct game_client *c, uint8_t over_clock, uint8_t *no_write_count)
{
	/* write queued updates to client */
	int rc;
	uint8_t noop = 0xff;

	struct packed_buffer *buffer;

	/* timestamp request must be prepended to queue right before it is sent or the timestamp
	   contained will be stale */
	if (c->request_universe_timestamp > 0) {
		uint8_t code = UPDATE_UNIVERSE_TIMESTAMP_SAMPLE;

		/* update code if this is a special sample */
		switch (c->request_universe_timestamp) {
		case UPDATE_UNIVERSE_TIMESTAMP_COUNT:
			code = UPDATE_UNIVERSE_TIMESTAMP_START_SAMPLE;
			break;
		case 1:
			code = UPDATE_UNIVERSE_TIMESTAMP_END_SAMPLE;
			break;
		}

		queue_update_universe_timestamp(c, code);
		c->request_universe_timestamp--;
	}

	/*  packed_buffer_queue_print(&c->client_write_queue); */
	buffer = packed_buffer_queue_combine(&c->client_write_queue, &c->client_write_queue_mutex);
	if (buffer) {
#if COMPUTE_AVERAGE_TO_CLIENT_BUFFER_SIZE
		/* Last I checked, average buffer size was in the 14.5kbyte range. */
		c->write_sum += buffer->buffer_size;
		c->write_count++;
		if ((universe_timestamp % 50) == 0)
			printf("avg = %llu\n", c->write_sum / c->write_count);
#endif
		rc = snis_writesocket(c->socket, buffer->buffer, buffer->buffer_size);
		packed_buffer_free(buffer);
		if (rc != 0) {
			snis_log(SNIS_ERROR, "writesocket failed, rc= %d, errno = %d(%s)\n", 
				rc, errno, strerror(errno));
			goto badclient;
		}
		*no_write_count = 0;
	} else if (*no_write_count > over_clock) {
		/* no-op, just so we know if client is still there */
		rc = snis_writesocket(c->socket, &noop, sizeof(noop));
		if (rc != 0) {
			snis_log(SNIS_ERROR, "(noop) writesocket failed, rc= %d, errno = %d(%s)\n", 
				rc, errno, strerror(errno));
			goto badclient;
		}
		*no_write_count = 0;
	} else
		(*no_write_count)++;
	return;

badclient:
	log_client_info(SNIS_WARN, c->socket, "disconnected, failed writing socket\n");
	shutdown(c->socket, SHUT_RDWR);
	close(c->socket);
	c->socket = -1;
}

static void send_update_ship_packet(struct game_client *c,
	struct snis_entity *o, uint8_t opcode);
static void send_econ_update_ship_packet(struct game_client *c,
	struct snis_entity *o);
static void send_update_asteroid_packet(struct game_client *c,
	struct snis_entity *o);
static void send_update_asteroid_minerals_packet(struct game_client *c,
	struct snis_entity *o);
static void send_update_docking_port_packet(struct game_client *c,
	struct snis_entity *o);
static void send_update_block_packet(struct game_client *c,
	struct snis_entity *o);
static void send_update_turret_packet(struct game_client *c,
	struct snis_entity *o);
static void send_update_cargo_container_packet(struct game_client *c,
	struct snis_entity *o);
static void send_update_derelict_packet(struct game_client *c,
	struct snis_entity *o);
static void send_update_black_hole_packet(struct game_client *c,
	struct snis_entity *o);
static void send_update_planet_packet(struct game_client *c,
	struct snis_entity *o);
static void send_update_wormhole_packet(struct game_client *c,
	struct snis_entity *o);
static void send_update_starbase_packet(struct game_client *c,
	struct snis_entity *o);
static void send_update_warpgate_packet(struct game_client *c,
	struct snis_entity *o);
static void send_update_explosion_packet(struct game_client *c,
	struct snis_entity *o);
static void send_update_chaff_packet(struct game_client *c,
	struct snis_entity *o);
static void send_update_torpedo_packet(struct game_client *c,
	struct snis_entity *o);
static void send_update_missile_packet(struct game_client *c,
	struct snis_entity *o);
static void send_update_laser_packet(struct game_client *c,
	struct snis_entity *o);
static void send_update_laserbeam_packet(struct game_client *c,
	struct snis_entity *o);
static void send_update_tractorbeam_packet(struct game_client *c,
	struct snis_entity *o);
static void send_update_spacemonster_packet(struct game_client *c,
	struct snis_entity *o);
static void send_update_nebula_packet(struct game_client *c,
	struct snis_entity *o);
static void send_update_damcon_obj_packet(struct game_client *c,
	struct snis_damcon_entity *o);
static void send_update_damcon_socket_packet(struct game_client *c,
	struct snis_damcon_entity *o);
static void send_update_damcon_part_packet(struct game_client *c,
	struct snis_damcon_entity *o);
static void send_update_power_model_data(struct game_client *c,
		struct snis_entity *o);
static void send_update_coolant_model_data(struct game_client *c,
		struct snis_entity *o);
static void send_update_warp_core(struct game_client *c,
		struct snis_entity *o);

static void send_respawn_time(struct game_client *c, struct snis_entity *o);

static void queue_up_client_object_update(struct game_client *c, struct snis_entity *o)
{
	switch(o->type) {
	case OBJTYPE_SHIP1:
		send_update_ship_packet(c, o, OPCODE_UPDATE_SHIP);
		if (!o->alive)
			send_respawn_time(c, o);
		send_update_power_model_data(c, o);
		send_update_coolant_model_data(c, o);
		if (o->tsd.ship.damage_data_dirty) {
			send_silent_ship_damage_packet(o); /* sends to all clients. */
			o->tsd.ship.damage_data_dirty = 0;
		}
		break;
	case OBJTYPE_SHIP2:
		send_econ_update_ship_packet(c, o);
		break;
	case OBJTYPE_ASTEROID:
		send_update_asteroid_packet(c, o);
		/* The asteroid minerals rarely change, so send them only once every 64 ticks, or every 6.4 seconds */
		if (((o->id + universe_timestamp) & 0x3f) == 0)
			send_update_asteroid_minerals_packet(c, o);
		break;
	case OBJTYPE_CARGO_CONTAINER:
		send_update_cargo_container_packet(c, o);
		break;
	case OBJTYPE_DERELICT:
		send_update_derelict_packet(c, o);
		break;
	case OBJTYPE_BLACK_HOLE:
		send_update_black_hole_packet(c, o);
		break;
	case OBJTYPE_PLANET:
		send_update_planet_packet(c, o);
		break;
	case OBJTYPE_WORMHOLE:
		send_update_wormhole_packet(c, o);
		break;
	case OBJTYPE_STARBASE:
		send_update_starbase_packet(c, o);
		break;
	case OBJTYPE_WARPGATE:
		send_update_warpgate_packet(c, o);
		break;
	case OBJTYPE_NEBULA:
		send_update_nebula_packet(c, o);
		break;
	case OBJTYPE_EXPLOSION:
		send_update_explosion_packet(c, o);
		break;
	case OBJTYPE_CHAFF:
		send_update_chaff_packet(c, o);
		break;
	case OBJTYPE_DEBRIS:
		break;
	case OBJTYPE_SPARK:
		break;
	case OBJTYPE_TORPEDO:
		send_update_torpedo_packet(c, o);
		break;
	case OBJTYPE_MISSILE:
		send_update_missile_packet(c, o);
		break;
	case OBJTYPE_LASER:
		send_update_laser_packet(c, o);
		break;
	case OBJTYPE_SPACEMONSTER:
		send_update_spacemonster_packet(c, o);
		break;
	case OBJTYPE_LASERBEAM:
		send_update_laserbeam_packet(c, o);
		break;
	case OBJTYPE_TRACTORBEAM:
		send_update_tractorbeam_packet(c, o);
		break;
	case OBJTYPE_DOCKING_PORT:
		send_update_docking_port_packet(c, o);
		break;
	case OBJTYPE_BLOCK:
		send_update_block_packet(c, o);
		break;
	case OBJTYPE_TURRET:
		send_update_turret_packet(c, o);
		break;
	case OBJTYPE_WARP_CORE:
		send_update_warp_core(c, o);
		break;
	default:
		break;
	}
}

static void queue_up_client_object_sdata_update(struct game_client *c, struct snis_entity *o)
{
	switch (o->type) {
	case OBJTYPE_SHIP1:
	case OBJTYPE_SHIP2:
	case OBJTYPE_ASTEROID:
	case OBJTYPE_CARGO_CONTAINER:
	case OBJTYPE_DERELICT:
	case OBJTYPE_PLANET:
	case OBJTYPE_WORMHOLE:
	case OBJTYPE_STARBASE:
	case OBJTYPE_TORPEDO:
	case OBJTYPE_LASER:
	case OBJTYPE_SPACEMONSTER:
	case OBJTYPE_WARP_CORE:
	case OBJTYPE_BLACK_HOLE:
		send_update_sdata_packets(c, o);
		break;
	default:
		break;
	}
}
static int too_far_away_to_care(struct game_client *c, struct snis_entity *o)
{
	struct snis_entity *ship = &go[c->ship_index];
	double dx, dz, dist;

	dx = (ship->x - o->x);
	dz = (ship->z - o->z);
	dist = (dx * dx) + (dz * dz);
	return (dist > (bandwidth_throttle_distance * bandwidth_throttle_distance));
}

static void queue_latency_check(struct game_client *c)
{
	struct timespec ts;
	uint64_t value[2];

	value[0] = 0;
	value[1] = 0;
	BUILD_ASSERT(sizeof(value) >= sizeof(ts));
	clock_gettime(CLOCK_MONOTONIC, &ts);
	memcpy(value, &ts, sizeof(ts));
	pb_queue_to_client(c, snis_opcode_pkt("bqq",
		OPCODE_LATENCY_CHECK, value[0], value[1]));
}

static void queue_netstats(struct game_client *c)
{
	struct timeval now;
	uint32_t elapsed_seconds;

	if ((universe_timestamp & 0x0f) != 0x0f)
		return;
	gettimeofday(&now, NULL);
	elapsed_seconds = now.tv_sec - netstats.start.tv_sec;
	pb_queue_to_client(c, snis_opcode_pkt("bqqwwwwwwwww", OPCODE_UPDATE_NETSTATS,
					netstats.bytes_sent, netstats.bytes_recd,
					netstats.nobjects, netstats.nships,
					elapsed_seconds,
					c->latency_in_usec,
					faction_population[0],
					faction_population[1],
					faction_population[2],
					faction_population[3],
					faction_population[4]));
}

static void queue_starmap(struct game_client *c)
{
	int i;
	struct packed_buffer *pb;

	/* Send starmap if it is dirty, or once every 8 seconds
	 * This is because deletions happen on client side via
	 * expiration, refreshes stop the expiration.
	 */
	if (!starmap_dirty && ((universe_timestamp + 20) % 80) != 0)
		return;
	for (i = 0; i < nstarmap_entries; i++) {
		pb = packed_buffer_allocate(1 + 4 + 4 + 4 + SSGL_LOCATIONSIZE);
		packed_buffer_append(pb, "bSSS", OPCODE_UPDATE_SOLARSYSTEM_LOCATION,
					starmap[i].x, (int32_t) 1000.0,
					starmap[i].y, (int32_t) 1000.0,
					starmap[i].z, (int32_t) 1000.0);
		packed_buffer_append_raw(pb, starmap[i].name, SSGL_LOCATIONSIZE);
		pb_queue_to_client(c, pb);
	}
}

static void queue_up_client_damcon_object_update(struct game_client *c,
			struct damcon_data *d, struct snis_damcon_entity *o)
{
	if (o->version != c->damcon_data_clients[damcon_index(d, o)].last_version_sent) {
		switch(o->type) {
		case DAMCON_TYPE_PART:
			send_update_damcon_part_packet(c, o);
			break;
		case DAMCON_TYPE_SOCKET:
			send_update_damcon_socket_packet(c, o);
			break;
		case DAMCON_TYPE_WAYPOINT:
			break; /* Comment this "break" out to debug damcon robot waypoints */
		default:
			send_update_damcon_obj_packet(c, o);
			break;
		}
		c->damcon_data_clients[damcon_index(d, o)].last_version_sent = o->version;
	}
}

static void queue_up_client_damcon_update(struct game_client *c)
{
	int i;
	struct damcon_data *d = &bridgelist[c->bridge].damcon;
	
	for (i = 0; i <= snis_object_pool_highest_object(d->pool); i++)
		queue_up_client_damcon_object_update(c, d, &d->o[i]);
}

static void queue_up_client_waypoint_update(struct game_client *c)
{
	int i;

	if ((universe_timestamp % 100) == c->bridge) /* update every 10 secs regardless */
		c->waypoints_dirty = 1;
	if (!c->waypoints_dirty)
		return;

	struct bridge_data *b = &bridgelist[c->bridge];
	pb_queue_to_client(c, snis_opcode_subcode_pkt("bbb",
			OPCODE_SET_WAYPOINT, OPCODE_SET_WAYPOINT_COUNT,
			(uint8_t) b->nwaypoints));
	for (i = 0; i < b->nwaypoints; i++) {
		uint8_t row = i;
		pb_queue_to_client(c, snis_opcode_subcode_pkt("bbbSSS",
			OPCODE_SET_WAYPOINT, OPCODE_SET_WAYPOINT_ROW, row,
				b->waypoint[i].x, (int32_t) UNIVERSE_DIM,
				b->waypoint[i].y, (int32_t) UNIVERSE_DIM,
				b->waypoint[i].z, (int32_t) UNIVERSE_DIM));
	}
	pb_queue_to_client(c, snis_opcode_subcode_pkt("bbw", OPCODE_SET_WAYPOINT,
				OPCODE_SET_WAYPOINT_UPDATE_SELECTION,
				(uint32_t) b->selected_waypoint));
	c->waypoints_dirty = 0;
}

static void queue_up_client_rts_update(struct game_client *c)
{
	int i;

	if (!rts_mode)
		return;

	/* Queue up main base health updates */
	for (i = 0; i < ARRAYSIZE(rts_planet); i++) {
		uint32_t health = rts_planet[i].health;
		uint32_t id = go[rts_planet[i].index].id;
		pb_queue_to_client(c, snis_opcode_subcode_pkt("bbbww", OPCODE_RTS_FUNC,
					OPCODE_RTS_FUNC_MAIN_BASE_UPDATE,
					go[rts_planet[i].index].sdata.faction, id, health));
	}
}

static void queue_up_client_volume_update(struct game_client *c)
{
	struct bridge_data *b;
	uint8_t new_volume;

	if (c->bridge < 0 && c->bridge >= nbridges)
		return;
	b  = &bridgelist[c->bridge];
	if (universe_timestamp - b->text_to_speech_volume_timestamp > 3)
		return;
	new_volume = (uint8_t) (b->text_to_speech_volume * 255.0);
	pb_queue_to_client(c, snis_opcode_pkt("bb", OPCODE_ADJUST_TTS_VOLUME, new_volume));
}

static void queue_up_client_custom_buttons(struct game_client *c)
{
	struct bridge_data *b;
	int i;

	if (c->bridge < 0 && c->bridge >= nbridges)
		return;
	b  = &bridgelist[c->bridge];
	if ((universe_timestamp % 10) != 0) /* Update at 1 Hz */
		return;
	pb_queue_to_client(c, snis_opcode_subcode_pkt("bbb", OPCODE_CUSTOM_BUTTON,
			OPCODE_CUSTOM_BUTTON_SUBCMD_ACTIVE_LIST, b->active_custom_buttons));
	for (i = 0; i < 6; i++)
		if (b->active_custom_buttons & (1 << i))
			pb_queue_to_client(c, snis_opcode_subcode_pkt("bbbbbbbbbbbbbbbbbb",
				OPCODE_CUSTOM_BUTTON, OPCODE_CUSTOM_BUTTON_SUBCMD_UPDATE_TEXT, (uint8_t) i,
				b->custom_button_text[i][0],
				b->custom_button_text[i][1],
				b->custom_button_text[i][2],
				b->custom_button_text[i][3],
				b->custom_button_text[i][4],
				b->custom_button_text[i][5],
				b->custom_button_text[i][6],
				b->custom_button_text[i][7],
				b->custom_button_text[i][8],
				b->custom_button_text[i][9],
				b->custom_button_text[i][10],
				b->custom_button_text[i][11],
				b->custom_button_text[i][12],
				b->custom_button_text[i][13],
				b->custom_button_text[i][14]));
}

static void queue_up_client_cipher_update(struct game_client *c)
{
	struct bridge_data *b;
	struct packed_buffer *pb;

	if (c->bridge < 0 && c->bridge >= nbridges)
		return;
	b  = &bridgelist[c->bridge];
	if ((universe_timestamp % 10) != 0) /* Update at 1 Hz */
		return;

	/* Send the current enciphered message for this bridge */
	pb = packed_buffer_allocate(sizeof(struct comms_transmission_packet) + 255);
	packed_buffer_append(pb, "bbb", OPCODE_COMMS_TRANSMISSION, OPCODE_COMMS_UPDATE_ENCIPHERED,
				(uint8_t) strlen(b->enciphered_message) + 1);
	packed_buffer_append_raw(pb, b->enciphered_message, strlen(b->enciphered_message) + 1);
	pb_queue_to_client(c, pb);

	/* Send the current key guess for this bridge */
	pb = packed_buffer_allocate(sizeof(struct comms_transmission_packet) + 26);
	packed_buffer_append(pb, "bbb", OPCODE_COMMS_TRANSMISSION, OPCODE_COMMS_KEY_GUESS, (uint8_t) 26);
	packed_buffer_append_raw(pb, b->guessed_key, 26);
	pb_queue_to_client(c, pb);
}

static void queue_up_client_updates(struct game_client *c)
{
	int i;
	int count;

	count = 0;
	pthread_mutex_lock(&universe_mutex);
	if (universe_timestamp != c->timestamp) {
		queue_netstats(c);
		queue_starmap(c);
		for (i = 0; i <= snis_object_pool_highest_object(pool); i++) {
			/* printf("obj %d: a=%d, ts=%u, uts%u, type=%hhu\n",
				i, go[i].alive, go[i].timestamp, universe_timestamp, go[i].type); */
			if (!go[i].alive && go[i].type != OBJTYPE_SHIP1)
				continue;

			if (too_far_away_to_care(c, &go[i]) &&
				(universe_timestamp + i) % distant_update_period != 0) {

				gather_opcode_not_sent_stats(&go[i]);
				continue;
			}

			if (go[i].timestamp != c->go_clients[i].last_timestamp_sent) {
				queue_up_client_object_update(c, &go[i]);
				c->go_clients[i].last_timestamp_sent = go[i].timestamp;
				count++;
			}
			queue_up_client_object_sdata_update(c, &go[i]);
		}
		queue_up_client_damcon_update(c);
		queue_up_client_waypoint_update(c);
		queue_up_client_rts_update(c);
		queue_up_client_volume_update(c);
		queue_up_client_custom_buttons(c);
		queue_up_client_cipher_update(c);
		queue_latency_check(c);
		/* printf("queued up %d updates for client\n", count); */

		c->timestamp = universe_timestamp;
	}
	pthread_mutex_unlock(&universe_mutex);
}

static void queue_up_to_clients_that_care(struct snis_entity *o)
{
	int i;
	for (i = 0; i < nclients; i++) {
		struct game_client *c = &client[i];

		if (!c->refcount)
			continue;

		if (too_far_away_to_care(c, o))
			continue;

		if (o->timestamp != c->go_clients[go_index(o)].last_timestamp_sent) {
			queue_up_client_object_update(c, o);
			c->go_clients[go_index(o)].last_timestamp_sent = o->timestamp;
		}
	}
}

static void queue_up_client_id(struct game_client *c)
{
	/* tell the client what his ship id is. */
	pb_queue_to_client(c, snis_opcode_pkt("bw", OPCODE_ID_CLIENT_SHIP, c->shipid));
}

static void queue_set_solarsystem(struct game_client *c)
{
	char solarsystem[100];
	struct packed_buffer *pb;

	pb = packed_buffer_allocate(101);
	if (!pb)
		return;
	memset(solarsystem, 0, sizeof(solarsystem));
	strncpy(solarsystem, solarsystem_name, 99);
	packed_buffer_append(pb, "br", OPCODE_SET_SOLARSYSTEM, solarsystem, (uint16_t) 100);
	pb_queue_to_client(c, pb);
}

#define SIMULATE_SLOW_SERVER 0
#if SIMULATE_SLOW_SERVER
static void simulate_slow_server(__attribute__((unused)) int x)
{
	/* sleep an extra amount between 2/10th and 4/10ths seconds */
	sleep_double(0.02 + 0.2 * ((double) snis_randn(100) / 100.0));
}
#else
#define simulate_slow_server(x)
#endif
	
static void *per_client_write_thread(__attribute__((unused)) void /* struct game_client */ *client)
{
	struct game_client *c = (struct game_client *) client;
	int bridge_status;

	/* Wait for client[] array to get fully updated before proceeding. */
	client_lock();
	get_client(c);
	client_unlock();
	double disconnect_timer = -1.0;

	const uint8_t over_clock = 4;
	const double maxTimeBehind = 0.5;
	double delta = 1.0/10.0/(double)over_clock;

	uint8_t no_write_count = 0;
	double currentTime = time_now_double();
	double nextTime = currentTime + delta;
	log_client_info(SNIS_WARN, c->socket, "client writer thread sending opcodes\n");
	while (1) {
		if (c->socket < 0)
			break;

		bridge_status = bridgelist[c->bridge].verified;
		if (bridge_status == BRIDGE_FAILED_VERIFICATION ||
			bridge_status == BRIDGE_REFUSED) {
			unsigned char player_error = ADD_PLAYER_ERROR_FAILED_VERIFICATION;
			if (bridge_status == BRIDGE_REFUSED)
				player_error = ADD_PLAYER_ERROR_TOO_MANY_BRIDGES;
			pb_queue_to_client(c, snis_opcode_pkt("bb", OPCODE_ADD_PLAYER_ERROR, player_error));
			disconnect_timer = 1.0;
		}

		currentTime = time_now_double();

		if (currentTime - nextTime > maxTimeBehind)
			nextTime = currentTime;

		if (currentTime >= nextTime) {
			queue_up_client_updates(c);
			write_queued_updates_to_client(c, over_clock, &no_write_count);

			nextTime += delta;
		} else {
			double timeToSleep = nextTime-currentTime;
			if (timeToSleep > 0)
				sleep_double(timeToSleep);
		}
		simulate_slow_server(0);
		if (disconnect_timer > 0.0)
			break;
	}
	if (disconnect_timer > 0.0) {
		log_client_info(SNIS_WARN, c->socket, "disconnecting client for failed bridge verification\n");
		sleep_double(disconnect_timer);
		shutdown(c->socket, SHUT_RDWR);
		close(c->socket);
		c->socket = -1;
	}
	log_client_info(SNIS_INFO, c->socket, "client writer thread exiting.\n");
	pthread_mutex_lock(&universe_mutex);
	client_lock();
	put_client(c);
	client_unlock();
	pthread_mutex_unlock(&universe_mutex);
	return NULL;
}

static int verify_client_protocol(int connection)
{
	int rc;
	char protocol_version[10];
	rc = snis_readsocket(connection, protocol_version, strlen(SNIS_PROTOCOL_VERSION));
	if (rc < 0)
		return rc;
	protocol_version[7] = '\0';
	snis_log(SNIS_INFO, "protocol read...'%s'\n", protocol_version);
	if (strcmp(protocol_version, SNIS_PROTOCOL_VERSION) != 0)
		return -1;
	snis_log(SNIS_INFO, "protocol verified.\n");
	return 0;
}

static int lookup_bridge(unsigned char *shipname, unsigned char *password)
{
	int i;

	for (i = 0; i < nbridges; i++) {
		if (strcmp((const char *) shipname, (const char *) bridgelist[i].shipname) == 0 &&
			strcmp((const char *) password, (const char *) bridgelist[i].password) == 0) {
			pthread_mutex_unlock(&universe_mutex);
			return i;
		}
	}
	return -1;
}

static int insane(unsigned char *word, int len)
{
	int i;

	word[len-1] = '\0';
	len = strlen((const char *) word);
	for (i = 0; i < len; i++) {
		if (index(" 	-@#%^&*()=+_/?>.<,~[]{}", word[i]))
			continue;
		if (!isalnum(word[i]))
			return 1;
	}
	return 0;
}

static void send_econ_update_ship_packet(struct game_client *c,
	struct snis_entity *o)
{
	int n;
	int32_t victim_id;
	uint8_t ai[MAX_AI_STACK_ENTRIES] = { 0 };
	int i;
	uint8_t opcode = OPCODE_ECON_UPDATE_SHIP;
	uint8_t rts_order;

	n = o->tsd.ship.nai_entries - 1;
	rts_order = 255;
	victim_id = -1;
	if (n >= 0) {
		if (rts_mode && go[c->ship_index].sdata.faction == o->sdata.faction) {
			int index = n;
			while (index > 0 && o->tsd.ship.ai[index].ai_mode < AI_MODE_RTS_FIRST_COMMAND)
				index--;
			rts_order = o->tsd.ship.ai[index].ai_mode;
		}
		if (o->tsd.ship.ai[n].ai_mode == AI_MODE_ATTACK) {
			victim_id = o->tsd.ship.ai[n].u.attack.victim_id;
		}
	}

	if (c->debug_ai) {
		opcode = OPCODE_ECON_UPDATE_SHIP_DEBUG_AI; 
		memset(ai, 0, MAX_AI_STACK_ENTRIES);
		for (i = 0; i < o->tsd.ship.nai_entries && i < MAX_AI_STACK_ENTRIES; i++) {
			switch (o->tsd.ship.ai[i].ai_mode) {
			case AI_MODE_IDLE:
				ai[i] = 'I';
				break;
			case AI_MODE_ATTACK:
				ai[i] = 'A';
				break;
			case AI_MODE_FLEE:
				ai[i] = 'F';
				break;
			case AI_MODE_FLEET_LEADER:
				ai[i] = 'L';
				break;
			case AI_MODE_FLEET_MEMBER:
				ai[i] = 'M';
				break;
			case AI_MODE_PATROL:
				ai[i] = 'P';
				break;
			case AI_MODE_HANGOUT:
				ai[i] = 'H';
				break;
			case AI_MODE_COP:
				ai[i] = 'C';
				break;
			case AI_MODE_RTS_STANDBY:
				ai[i] = 'S';
				break;
			case AI_MODE_RTS_OCCUPY_NEAR_BASE:
				ai[i] = 'O';
				break;
			case AI_MODE_RTS_OUT_OF_FUEL:
				ai[i] = 'E';
				break;
			case AI_MODE_CATATONIC:
				ai[i] = 'K';
				break;
			default:
				ai[i] = '?';
				break;
			}
		}
	}
	pb_queue_to_client(c, packed_buffer_new("bwwhSSSQwbbb", opcode,
			o->id, o->timestamp, o->alive, o->x, (int32_t) UNIVERSE_DIM,
			o->y, (int32_t) UNIVERSE_DIM, o->z, (int32_t) UNIVERSE_DIM,
			&o->orientation, victim_id, o->tsd.ship.shiptype, o->sdata.faction, rts_order));

	if (!c->debug_ai)
		return;

	uint8_t npoints = 0;
	union vec3 *v = NULL;
	struct packed_buffer *pb;

	/* Find top-most patrol/leader mode on stack, if any */
	n = o->tsd.ship.nai_entries - 1;
	while (n >= 0 && o->tsd.ship.ai[n].ai_mode != AI_MODE_FLEET_LEADER &&
		o->tsd.ship.ai[n].ai_mode != AI_MODE_PATROL &&
		o->tsd.ship.ai[n].ai_mode != AI_MODE_COP)
		n--;

	if (n >= 0 && (o->tsd.ship.ai[n].ai_mode == AI_MODE_FLEET_LEADER ||
		o->tsd.ship.ai[n].ai_mode == AI_MODE_PATROL)) {
		npoints = o->tsd.ship.ai[n].u.patrol.npoints;
		v = o->tsd.ship.ai[n].u.patrol.p;
	}

	if (n >= 0 && (o->tsd.ship.ai[n].ai_mode == AI_MODE_COP)) {
		npoints = o->tsd.ship.ai[n].u.cop.npoints;
		v = o->tsd.ship.ai[n].u.cop.p;
	}

	pb = packed_buffer_allocate(6 + sizeof(uint32_t) + sizeof(uint32_t) * 3 * npoints);
	if (!pb)
		return;

	BUILD_ASSERT(MAX_AI_STACK_ENTRIES == 5);
	packed_buffer_append(pb, "bbbbbSb",
			ai[0], ai[1], ai[2], ai[3], ai[4],
			(double) o->tsd.ship.threat_level, (int32_t) UNIVERSE_DIM, npoints);

	for (i = 0; i < npoints; i++) {
		packed_buffer_append(pb, "SSS",
			v[i].v.x, (int32_t) UNIVERSE_DIM,
			v[i].v.y, (int32_t) UNIVERSE_DIM,
			v[i].v.z, (int32_t) UNIVERSE_DIM);
	}
	pb_queue_to_client(c, pb);
}

static void send_ship_sdata_packet(struct game_client *c, struct ship_sdata_packet *sip)
{
	struct packed_buffer *pb;

	pb = packed_buffer_allocate(sizeof(struct ship_sdata_packet));
	packed_buffer_append(pb, "bwbbbbbbbr", OPCODE_SHIP_SDATA, sip->id, sip->subclass,
		sip->shield_strength, sip->shield_wavelength, sip->shield_width, sip->shield_depth,
		sip->faction, sip->lifeform_count,
		sip->name, (unsigned short) sizeof(sip->name));
	pb_queue_to_client(c, pb);
}

static void send_generic_ship_damage_packet(struct snis_entity *o, uint8_t opcode)
{
	struct packed_buffer *pb;

	pb = packed_buffer_allocate(sizeof(struct ship_damage_packet));
	packed_buffer_append(pb, "bwr", opcode, o->id,
		(char *) &o->tsd.ship.damage, sizeof(o->tsd.ship.damage));
	send_packet_to_all_clients(pb, ROLE_ALL);
}

static void send_detonate_packet(struct snis_entity *o, double x, double y, double z,
				uint32_t time, double fractional_time)
{
	struct packed_buffer *pb;

	pb = snis_opcode_pkt("bwSSSwU", OPCODE_DETONATE,
			o->id,
			x, (int32_t) UNIVERSE_DIM,
			y, (int32_t) UNIVERSE_DIM,
			z, (int32_t) UNIVERSE_DIM,
			time, fractional_time, (uint32_t) 5);
	if (pb)
		send_packet_to_all_clients(pb, ROLE_ALL);
}

static void send_ship_damage_packet(struct snis_entity *o)
{
	send_generic_ship_damage_packet(o, OPCODE_UPDATE_DAMAGE);
}

static void send_silent_ship_damage_packet(struct snis_entity *o)
{
	send_generic_ship_damage_packet(o, OPCODE_SILENT_UPDATE_DAMAGE);
}

/* Append a transmission onto the trasmission queue */
static void schedule_lua_comms_transmission(struct lua_comms_transmission **transmission_queue,
						char *sender, uint32_t channel, const char *str)
{
	/* Assumes universe lock is held */
	struct lua_comms_transmission *t, *i;

	t = malloc(sizeof(*t));
	t->channel = channel;
	t->sender = strdup(sender);
	t->transmission = strdup(str);
	t->next = NULL;

	if (!*transmission_queue) {
		*transmission_queue = t;
		return;
	}
	/* Find the last queue entry */
	for (i = *transmission_queue; i->next != NULL; i = i->next)
		; /* empty loop body */
	i->next = t;
}

/* If any lua scripts are listening on the channel, queue up comms to them. */
static void queue_comms_to_lua_channels(char *sender, uint32_t channel, const char *str)
{
	int i;

	/* TODO somehow incorporate comms_transmission_strength() in here */
	/* assumes universe lock is held */
	for (i = 0; i < nlua_channels; i++) {
		if (channel != lua_channel[i].channel)
			continue;
		/* We have to schedule a callback to send this because
		 * you can't just call lua from any old thread.
		 */
		schedule_lua_comms_transmission(&lua_comms_transmission_queue, sender, channel, str);
		break;
	}
}

/* Note: be careful when using this that no part of format comes from the user. in that case, use: "%s", userstring */
static void send_comms_packet(struct snis_entity *transmitter, char *sender, uint32_t channel, const char *format, ...)
{
	struct packed_buffer *pb;
	char tmpbuf[100], tmpbuf2[100];
	va_list arg_ptr;

	/* REMINDER: If the format of OPCODE_COMMS_TRANSMISSION ever changes, you need to
	 * fix distort_comms_message in snis_server.c too.
	 */

	va_start(arg_ptr, format);
	vsnprintf(tmpbuf2, sizeof(tmpbuf2) - 1, format, arg_ptr);
	va_end(arg_ptr);
	snprintf(tmpbuf, sizeof(tmpbuf) - 1, "%s | %s", sender, tmpbuf2);
	pb = packed_buffer_allocate(sizeof(struct comms_transmission_packet) + 100);
	packed_buffer_append(pb, "bbb", OPCODE_COMMS_TRANSMISSION, OPCODE_COMMS_PLAINTEXT,
				(uint8_t) strlen(tmpbuf) + 1);
	packed_buffer_append_raw(pb, tmpbuf, strlen(tmpbuf) + 1);
	if (channel == 0)
		send_comms_packet_to_all_clients(transmitter, pb, ROLE_ALL);
	else
		send_comms_packet_to_all_bridges_on_channel(transmitter, channel, pb, ROLE_ALL);
	/* Send comms to any lua scripts that are listening */
	queue_comms_to_lua_channels(sender, channel, tmpbuf2);
}

/* Note: be careful when using this that no part of format comes from the user. in that case, use: "%s", userstring */
static void send_comms_enciphered_packet(struct snis_entity *transmitter,
						char *sender, uint32_t channel, const char *format, ...)
{
	struct packed_buffer *pb;
	char tmpbuf[100], tmpbuf2[100];
	va_list arg_ptr;

	/* REMINDER: If the format of OPCODE_COMMS_TRANSMISSION ever changes, you need to
	 * fix distort_comms_message in snis_server.c too.
	 */

	va_start(arg_ptr, format);
	vsnprintf(tmpbuf2, sizeof(tmpbuf2) - 1, format, arg_ptr);
	va_end(arg_ptr);
	snprintf(tmpbuf, sizeof(tmpbuf) - 1, "%s | %s", sender, tmpbuf2);
	pb = packed_buffer_allocate(sizeof(struct comms_transmission_packet) + 100);
	packed_buffer_append(pb, "bbb", OPCODE_COMMS_TRANSMISSION, OPCODE_COMMS_ENCIPHERED,
				(uint8_t) strlen(tmpbuf) + 1);
	packed_buffer_append_raw(pb, tmpbuf, strlen(tmpbuf) + 1);
	if (channel == 0)
		send_comms_packet_to_all_clients(transmitter, pb, ROLE_ALL);
	else
		send_comms_packet_to_all_bridges_on_channel(transmitter, channel, pb, ROLE_ALL);
	/* Send comms to any lua scripts that are listening */
	queue_comms_to_lua_channels(sender, channel, tmpbuf2);
}

static void send_respawn_time(struct game_client *c,
	struct snis_entity *o)
{
	uint8_t seconds = (o->respawn_time - universe_timestamp) / 10;

	pb_queue_to_client(c, snis_opcode_pkt("bb", OPCODE_UPDATE_RESPAWN_TIME, seconds));
}

static void send_update_power_model_data(struct game_client *c,
		struct snis_entity *o)
{
	struct packed_buffer *pb;

	pb = packed_buffer_allocate(sizeof(uint8_t) +
			sizeof(o->tsd.ship.power_data) + sizeof(uint32_t));
	packed_buffer_append(pb, "bwr", OPCODE_UPDATE_POWER_DATA, o->id,
		(char *) &o->tsd.ship.power_data, (unsigned short) sizeof(o->tsd.ship.power_data)); 
	pb_queue_to_client(c, pb);
}
	
static void send_update_coolant_model_data(struct game_client *c,
		struct snis_entity *o)
{
	struct packed_buffer *pb;

	pb = packed_buffer_allocate(sizeof(uint8_t) +
			sizeof(o->tsd.ship.coolant_data) + sizeof(uint32_t) +
			sizeof(o->tsd.ship.temperature_data));
	packed_buffer_append(pb, "bwr", OPCODE_UPDATE_COOLANT_DATA, o->id,
		(char *) &o->tsd.ship.coolant_data, (unsigned short) sizeof(o->tsd.ship.coolant_data));
	packed_buffer_append(pb, "r",
		(char *) &o->tsd.ship.temperature_data,
			(unsigned short) sizeof(o->tsd.ship.temperature_data)); 
	pb_queue_to_client(c, pb);
}

static void send_update_warp_core(struct game_client *c,
		struct snis_entity *o)
{
	pb_queue_to_client(c, snis_opcode_pkt("bwwSSS", OPCODE_UPDATE_WARP_CORE,
					o->id, o->timestamp,
					o->x, (int32_t) UNIVERSE_DIM,
					o->y, (int32_t) UNIVERSE_DIM,
					o->z, (int32_t) UNIVERSE_DIM));
}

static void send_update_ship_packet(struct game_client *c,
	struct snis_entity *o, uint8_t opcode)
{
	struct packed_buffer *pb;
	uint32_t fuel, oxygen;
	uint8_t tloading, tloaded, throttle, rpm;

	throttle = o->tsd.ship.throttle;
	rpm = o->tsd.ship.rpm;
	fuel = o->tsd.ship.fuel;
	oxygen = o->tsd.ship.oxygen;
	uint32_t wallet;

	tloading = (uint8_t) (o->tsd.ship.torpedoes_loading & 0x0f);
	tloaded = (uint8_t) (o->tsd.ship.torpedoes_loaded & 0x0f);
	tloading = tloading | (tloaded << 4);
	wallet = (uint32_t) o->tsd.ship.wallet;

	pb = packed_buffer_allocate(sizeof(struct update_ship_packet));
	packed_buffer_append(pb, "bwwhSSS", opcode, o->id, o->timestamp, o->alive,
			o->x, (int32_t) UNIVERSE_DIM, o->y, (int32_t) UNIVERSE_DIM,
			o->z, (int32_t) UNIVERSE_DIM);
	packed_buffer_append(pb, "RRRwwRRbbbwwbbbbbbbbbbbbwQQQQSSSbB8bbww",
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
			o->tsd.ship.ai[0].u.attack.victim_id,
			&o->orientation.vec[0],
			&o->tsd.ship.sciball_orientation.vec[0],
			&o->tsd.ship.weap_orientation.vec[0],
			&o->tsd.ship.current_hg_ant_orientation.vec[0],
			o->tsd.ship.desired_hg_ant_aim.v.x * 100000, (int32_t) 1000000,
			o->tsd.ship.desired_hg_ant_aim.v.y * 100000, (int32_t) 1000000,
			o->tsd.ship.desired_hg_ant_aim.v.z * 100000, (int32_t) 1000000,
			o->tsd.ship.emf_detector,
			o->tsd.ship.in_secure_area,
			o->tsd.ship.docking_magnets,
			o->tsd.ship.nav_mode,
			o->tsd.ship.warp_core_status,
			rts_mode,
			o->tsd.ship.exterior_lights,
			o->tsd.ship.alarms_silenced,
			o->tsd.ship.missile_lock_detected,
			o->tsd.ship.comms_crypto_mode,
			o->tsd.ship.rts_active_button,
			wallet, o->tsd.ship.viewpoint_object);
	pb_queue_to_client(c, pb);

	/* now that we've sent the accumulated value, clear the emf_detector to the noise floor */
	o->tsd.ship.emf_detector = snis_randn(50);
}

static void send_update_damcon_obj_packet(struct game_client *c,
		struct snis_damcon_entity *o)
{
	pb_queue_to_client(c, snis_opcode_pkt("bwwwSSSRb",
					OPCODE_DAMCON_OBJ_UPDATE,   
					o->id, o->ship_id, o->type,
					o->x, (int32_t) DAMCONXDIM,
					o->y, (int32_t) DAMCONYDIM,
					o->velocity,  (int32_t) DAMCONXDIM,
		/* send desired_heading as heading to client to enable drifting */
					o->tsd.robot.desired_heading,
					o->tsd.robot.autonomous_mode));
}

static void send_update_damcon_socket_packet(struct game_client *c,
		struct snis_damcon_entity *o)
{
	pb_queue_to_client(c, snis_opcode_pkt("bwwwSSwbb",
					OPCODE_DAMCON_SOCKET_UPDATE,   
					o->id, o->ship_id, o->type,
					o->x, (int32_t) DAMCONXDIM,
					o->y, (int32_t) DAMCONYDIM,
					o->tsd.socket.contents_id,
					o->tsd.socket.system,
					o->tsd.socket.part));
}

static void send_update_damcon_part_packet(struct game_client *c,
		struct snis_damcon_entity *o)
{
	pb_queue_to_client(c, snis_opcode_pkt("bwwwSSRbbb",
					OPCODE_DAMCON_PART_UPDATE,   
					o->id, o->ship_id, o->type,
					o->x, (int32_t) DAMCONXDIM,
					o->y, (int32_t) DAMCONYDIM,
					o->heading,
					o->tsd.part.system,
					o->tsd.part.part,
					o->tsd.part.damage));
}

static void send_update_asteroid_packet(struct game_client *c,
	struct snis_entity *o)
{
	pb_queue_to_client(c, snis_opcode_pkt("bwwSSS", OPCODE_UPDATE_ASTEROID, o->id, o->timestamp,
					o->x, (int32_t) UNIVERSE_DIM,
					o->y, (int32_t) UNIVERSE_DIM,
					o->z, (int32_t) UNIVERSE_DIM));
}

static void send_update_asteroid_minerals_packet(struct game_client *c,
	struct snis_entity *o)
{
	pb_queue_to_client(c, snis_opcode_pkt("bwbbbb", OPCODE_UPDATE_ASTEROID_MINERALS, o->id,
					o->tsd.asteroid.carbon,
					o->tsd.asteroid.nickeliron,
					o->tsd.asteroid.silicates,
					o->tsd.asteroid.preciousmetals));
}

static void send_update_cargo_container_packet(struct game_client *c,
	struct snis_entity *o)
{
	pb_queue_to_client(c, snis_opcode_pkt("bwwSSSwS", OPCODE_UPDATE_CARGO_CONTAINER, o->id, o->timestamp,
					o->x, (int32_t) UNIVERSE_DIM,
					o->y, (int32_t) UNIVERSE_DIM,
					o->z, (int32_t) UNIVERSE_DIM,
					(uint32_t) o->tsd.cargo_container.contents.item,
					o->tsd.cargo_container.contents.qty, (int32_t) 1000000));
}

static void send_update_derelict_packet(struct game_client *c,
	struct snis_entity *o)
{
	pb_queue_to_client(c, snis_opcode_pkt("bwwSSSbbbw", OPCODE_UPDATE_DERELICT, o->id, o->timestamp,
					o->x, (int32_t) UNIVERSE_DIM,
					o->y, (int32_t) UNIVERSE_DIM,
					o->z, (int32_t) UNIVERSE_DIM,
					o->tsd.derelict.shiptype,
					o->tsd.derelict.fuel,
					o->tsd.derelict.oxygen,
					o->tsd.derelict.orig_ship_id));
}


static void send_update_black_hole_packet(struct game_client *c,
	struct snis_entity *o)
{
	pb_queue_to_client(c, snis_opcode_pkt("bwwSSSS", OPCODE_UPDATE_BLACK_HOLE,
					o->id, o->timestamp,
					o->x, (int32_t) UNIVERSE_DIM,
					o->y, (int32_t) UNIVERSE_DIM,
					o->z, (int32_t) UNIVERSE_DIM,
					o->tsd.black_hole.radius, (int32_t) UNIVERSE_DIM));
}

static void send_update_planet_packet(struct game_client *c,
	struct snis_entity *o)
{
	double ring;

	/* encode ring presence as negative radius */
	if (o->tsd.planet.ring)
		ring = -1.0;
	else
		ring = 1.0;

	pb_queue_to_client(c, snis_opcode_pkt("bwwSSSSwbbbbhbbbSbhbbwb", OPCODE_UPDATE_PLANET, o->id, o->timestamp,
					o->x, (int32_t) UNIVERSE_DIM,
					o->y, (int32_t) UNIVERSE_DIM,
					o->z, (int32_t) UNIVERSE_DIM,
					ring * (double) o->tsd.planet.radius, (int32_t) UNIVERSE_DIM,
					o->tsd.planet.description_seed,
					o->tsd.planet.government,
					o->tsd.planet.tech_level,
					o->tsd.planet.economy,
					o->tsd.planet.security,
					o->tsd.planet.contraband,
					o->tsd.planet.atmosphere_r,
					o->tsd.planet.atmosphere_g,
					o->tsd.planet.atmosphere_b,
					o->tsd.planet.atmosphere_scale, (int32_t) UNIVERSE_DIM,
					o->tsd.planet.has_atmosphere,
					o->tsd.planet.atmosphere_type,
					o->tsd.planet.solarsystem_planet_type,
					o->tsd.planet.ring_selector,
					o->tsd.planet.time_left_to_build,
					o->tsd.planet.build_unit_type));
}

static void send_update_wormhole_packet(struct game_client *c,
	struct snis_entity *o)
{
	pb_queue_to_client(c, snis_opcode_pkt("bwwSSS", OPCODE_UPDATE_WORMHOLE,
					o->id, o->timestamp,
					o->x, (int32_t) UNIVERSE_DIM,
					o->y, (int32_t) UNIVERSE_DIM,
					o->z, (int32_t) UNIVERSE_DIM));
}

static void send_update_starbase_packet(struct game_client *c,
	struct snis_entity *o)
{
	pb_queue_to_client(c, snis_opcode_pkt("bwwSSSQbbbbwbb", OPCODE_UPDATE_STARBASE,
					o->id, o->timestamp,
					o->x, (int32_t) UNIVERSE_DIM,
					o->y, (int32_t) UNIVERSE_DIM,
					o->z, (int32_t) UNIVERSE_DIM,
					&o->orientation,
					o->tsd.starbase.occupant[0],
					o->tsd.starbase.occupant[1],
					o->tsd.starbase.occupant[2],
					o->tsd.starbase.occupant[3],
					o->tsd.starbase.time_left_to_build,
					o->tsd.starbase.build_unit_type,
					o->tsd.starbase.starbase_number));
}

static void send_update_warpgate_packet(struct game_client *c,
	struct snis_entity *o)
{
	pb_queue_to_client(c, snis_opcode_pkt("bwwSSSQ", OPCODE_UPDATE_WARPGATE,
					o->id, o->timestamp,
					o->x, (int32_t) UNIVERSE_DIM,
					o->y, (int32_t) UNIVERSE_DIM,
					o->z, (int32_t) UNIVERSE_DIM,
					&o->orientation));
}

static void send_update_nebula_packet(struct game_client *c,
	struct snis_entity *o)
{
	union quat q;

	quat_init_axis(&q, o->tsd.nebula.avx, o->tsd.nebula.avy, o->tsd.nebula.avz,
			o->tsd.nebula.ava);
	pb_queue_to_client(c, snis_opcode_pkt("bwwSSSSQQSS", OPCODE_UPDATE_NEBULA, o->id, o->timestamp,
					o->x, (int32_t) UNIVERSE_DIM,
					o->y, (int32_t) UNIVERSE_DIM,
					o->z, (int32_t) UNIVERSE_DIM,
					o->tsd.nebula.r, (int32_t) UNIVERSE_DIM,
					&q,
					&o->tsd.nebula.unrotated_orientation,
					o->tsd.nebula.phase_angle, (int32_t) 360,
					o->tsd.nebula.phase_speed, (int32_t) 100));
}

static void send_update_explosion_packet(struct game_client *c,
	struct snis_entity *o)
{
	pb_queue_to_client(c, snis_opcode_pkt("bwwwSSShhhbb", OPCODE_UPDATE_EXPLOSION, o->id, o->timestamp,
				o->tsd.explosion.related_id,
				o->x, (int32_t) UNIVERSE_DIM, o->y, (int32_t) UNIVERSE_DIM,
				o->z, (int32_t) UNIVERSE_DIM,
				o->tsd.explosion.nsparks, o->tsd.explosion.velocity,
				o->tsd.explosion.time, o->tsd.explosion.victim_type,
				o->tsd.explosion.explosion_type));
}

static void send_update_chaff_packet(struct game_client *c,
	struct snis_entity *o)
{
	pb_queue_to_client(c, snis_opcode_pkt("bwwSSS", OPCODE_UPDATE_CHAFF, o->id, o->timestamp,
				o->x, (int32_t) UNIVERSE_DIM, o->y, (int32_t) UNIVERSE_DIM,
				o->z, (int32_t) UNIVERSE_DIM));
}

static void send_update_torpedo_packet(struct game_client *c,
	struct snis_entity *o)
{
	pb_queue_to_client(c, snis_opcode_pkt("bwwwSSS", OPCODE_UPDATE_TORPEDO, o->id, o->timestamp,
					o->tsd.torpedo.ship_id,
					o->x, (int32_t) UNIVERSE_DIM,
					o->y, (int32_t) UNIVERSE_DIM,
					o->z, (int32_t) UNIVERSE_DIM));
}

static void send_update_missile_packet(struct game_client *c, struct snis_entity *o)
{
	pb_queue_to_client(c, snis_opcode_pkt("bwwSSSQ", OPCODE_UPDATE_MISSILE, o->id, o->timestamp,
					o->x, (int32_t) UNIVERSE_DIM,
					o->y, (int32_t) UNIVERSE_DIM,
					o->z, (int32_t) UNIVERSE_DIM,
					&o->orientation));
}

static void send_update_laser_packet(struct game_client *c,
	struct snis_entity *o)
{
	pb_queue_to_client(c, snis_opcode_pkt("bwwwbSSSQ", OPCODE_UPDATE_LASER,
					o->id, o->timestamp, o->tsd.laser.ship_id, o->tsd.laser.power,
					o->x, (int32_t) UNIVERSE_DIM,
					o->y, (int32_t) UNIVERSE_DIM,
					o->z, (int32_t) UNIVERSE_DIM,
					&o->orientation));
}

static void send_update_laserbeam_packet(struct game_client *c,
	struct snis_entity *o)
{
	pb_queue_to_client(c, snis_opcode_pkt("bwwww", OPCODE_UPDATE_LASERBEAM,
					o->id, o->timestamp, o->tsd.laserbeam.origin,
					o->tsd.laserbeam.target));
}

static void send_update_tractorbeam_packet(struct game_client *c,
	struct snis_entity *o)
{
	pb_queue_to_client(c, snis_opcode_pkt("bwwww", OPCODE_UPDATE_TRACTORBEAM,
					o->id, o->timestamp, o->tsd.laserbeam.origin,
					o->tsd.laserbeam.target));
}

static void send_update_docking_port_packet(struct game_client *c,
	struct snis_entity *o)
{
	double scale;
	int model = o->tsd.docking_port.model;
	int port = o->tsd.docking_port.portnumber;

	scale = docking_port_info[model]->port[port].scale;
	pb_queue_to_client(c, snis_opcode_pkt("bwwSSSSQb", OPCODE_UPDATE_DOCKING_PORT,
					o->id, o->timestamp,
					scale, (int32_t) 1000,
					o->x, (int32_t) UNIVERSE_DIM,
					o->y, (int32_t) UNIVERSE_DIM,
					o->z, (int32_t) UNIVERSE_DIM,
					&o->orientation,
					o->tsd.docking_port.model));
}

static void send_update_block_packet(struct game_client *c,
	struct snis_entity *o)
{
	switch (o->tsd.block.shape.type) {
	case SHAPE_CUBOID:
		pb_queue_to_client(c, snis_opcode_pkt("bwwSSSSSSQbbb", OPCODE_UPDATE_BLOCK,
					o->id, o->timestamp,
					o->x, (int32_t) UNIVERSE_DIM,
					o->y, (int32_t) UNIVERSE_DIM,
					o->z, (int32_t) UNIVERSE_DIM,
					o->tsd.block.shape.cuboid.sx, (int32_t) UNIVERSE_DIM,
					o->tsd.block.shape.cuboid.sy, (int32_t) UNIVERSE_DIM,
					o->tsd.block.shape.cuboid.sz, (int32_t) UNIVERSE_DIM,
					&o->orientation,
					o->tsd.block.block_material_index,
					o->tsd.block.health,
					o->tsd.block.shape.type));
		break;
	case SHAPE_SPHERE:
		pb_queue_to_client(c, snis_opcode_pkt("bwwSSSSSSQbbb", OPCODE_UPDATE_BLOCK,
					o->id, o->timestamp,
					o->x, (int32_t) UNIVERSE_DIM,
					o->y, (int32_t) UNIVERSE_DIM,
					o->z, (int32_t) UNIVERSE_DIM,
					o->tsd.block.shape.sphere.radius, (int32_t) UNIVERSE_DIM,
					o->tsd.block.shape.sphere.radius, (int32_t) UNIVERSE_DIM,
					o->tsd.block.shape.sphere.radius, (int32_t) UNIVERSE_DIM,
					&o->orientation,
					o->tsd.block.block_material_index,
					o->tsd.block.health,
					o->tsd.block.shape.type));
		break;
	case SHAPE_CAPSULE:
		pb_queue_to_client(c, snis_opcode_pkt("bwwSSSSSSQbbb", OPCODE_UPDATE_BLOCK,
					o->id, o->timestamp,
					o->x, (int32_t) UNIVERSE_DIM,
					o->y, (int32_t) UNIVERSE_DIM,
					o->z, (int32_t) UNIVERSE_DIM,
					o->tsd.block.shape.capsule.length, (int32_t) UNIVERSE_DIM,
					o->tsd.block.shape.capsule.radius, (int32_t) UNIVERSE_DIM,
					o->tsd.block.shape.capsule.radius, (int32_t) UNIVERSE_DIM,
					&o->orientation,
					o->tsd.block.block_material_index,
					o->tsd.block.health,
					o->tsd.block.shape.type));
		break;
	}
}

static void send_update_turret_packet(struct game_client *c,
	struct snis_entity *o)
{
	pb_queue_to_client(c, snis_opcode_pkt("bwwSSSQQb", OPCODE_UPDATE_TURRET,
					o->id, o->timestamp,
					o->x, (int32_t) UNIVERSE_DIM,
					o->y, (int32_t) UNIVERSE_DIM,
					o->z, (int32_t) UNIVERSE_DIM,
					&o->orientation,
					&o->tsd.turret.base_orientation,
					o->tsd.turret.health));
}

static void send_update_spacemonster_packet(struct game_client *c,
	struct snis_entity *o)
{
	pb_queue_to_client(c, snis_opcode_pkt("bwwSSSwQbbb", OPCODE_UPDATE_SPACEMONSTER, o->id, o->timestamp,
					o->x, (int32_t) UNIVERSE_DIM,
					o->y, (int32_t) UNIVERSE_DIM,
					o->z, (int32_t) UNIVERSE_DIM,
					o->tsd.spacemonster.seed,
					&o->orientation,
					o->tsd.spacemonster.emit_intensity,
					o->tsd.spacemonster.head_size,
					o->tsd.spacemonster.tentacle_size));
}

static int add_new_player(struct game_client *c)
{
	int rc;
	struct add_player_packet app;
	uint8_t no_write_count = 0;

	fprintf(stderr, "%s: reading update player packet\n", logprefix());
	rc = snis_readsocket(c->socket, &app, sizeof(app));
	fprintf(stderr, "%s: read update player packet, rc = %d\n", logprefix(), rc);
	if (rc)
		return rc;
	app.role = ntohl(app.role);
	if (app.opcode != OPCODE_UPDATE_PLAYER) {
		fprintf(stderr, "%s: bad opcode %d\n", logprefix(), app.opcode);
		snis_log(SNIS_ERROR, "bad opcode %d\n", app.opcode);
		goto protocol_error;
	}
	app.shipname[19] = '\0';
	app.password[19] = '\0';

	if (insane(app.shipname, 20) || insane(app.password, 20)) {
		fprintf(stderr, "%s: name or password\n", logprefix());
		snis_log(SNIS_ERROR, "Bad ship name or password\n");
		goto protocol_error;
	}
	fprintf(stderr, "%s: new client: sn='%s', create = %hhu\n",
			logprefix(), app.shipname, app.new_ship);

	c->bridge = lookup_bridge(app.shipname, app.password);
	fprintf(stderr, "%s: c->bridge = %d\n", logprefix(), c->bridge);
	c->role = verify_role(app.role);
	if (c->bridge == -1) { /* didn't find our bridge, make a new one. */
		double x, z;
		fprintf(stderr, "%s: didn't find bridge, make new one\n", logprefix());

		for (int i = 0; i < 100; i++) {
			x = XKNOWN_DIM * (double) rand() / (double) RAND_MAX;
			z = ZKNOWN_DIM * (double) rand() / (double) RAND_MAX;
			if (dist3d(x - SUNX, 0, z - SUNZ) > SUN_DIST_LIMIT)
				break;
		}
		c->ship_index = add_player(x, z, 0.0, 0.0, M_PI / 2.0, app.warpgate_number, app.requested_faction);
		c->shipid = go[c->ship_index].id;
		strcpy(go[c->ship_index].sdata.name, (const char * restrict) app.shipname);
		memset(&bridgelist[nbridges], 0, sizeof(bridgelist[nbridges]));
		strcpy((char *) bridgelist[nbridges].shipname, (const char *) app.shipname);
		strcpy((char *) bridgelist[nbridges].password, (const char *) app.password);
		bridgelist[nbridges].shipid = c->shipid;
		bridgelist[nbridges].comms_channel = c->shipid;
		bridgelist[nbridges].npcbot.channel = (uint32_t) -1;
		bridgelist[nbridges].npcbot.object_id = (uint32_t) -1;
		bridgelist[nbridges].damcon.bridge = nbridges;
		bridgelist[nbridges].current_displaymode = DISPLAYMODE_MAINSCREEN;
		bridgelist[nbridges].verified = BRIDGE_UNVERIFIED;
		bridgelist[nbridges].requested_verification = 0;
		bridgelist[nbridges].requested_creation = !!app.new_ship;
		bridgelist[nbridges].nclients = 1;
		bridgelist[nbridges].selected_waypoint = -1;
		bridgelist[nbridges].nwaypoints = 0;
		bridgelist[nbridges].warp_core_critical_timer = 0;
		bridgelist[nbridges].warp_core_critical = 0;
		bridgelist[nbridges].active_custom_buttons = 0;
		memset(bridgelist[nbridges].custom_button_text, 0, sizeof(bridgelist[nbridges].custom_button_text));
		memset(bridgelist[nbridges].cipher_key, '_', sizeof(bridgelist[nbridges].cipher_key));
		memset(bridgelist[nbridges].guessed_key, '_', sizeof(bridgelist[nbridges].guessed_key));
		memset(bridgelist[nbridges].enciphered_message, 0, sizeof(bridgelist[nbridges].enciphered_message));
		strcpy(bridgelist[nbridges].last_text_to_speech, "");
		bridgelist[nbridges].text_to_speech_volume = 0.33;
		bridgelist[nbridges].text_to_speech_volume_timestamp = universe_timestamp;
		clear_bridge_waypoints(nbridges);
		c->bridge = nbridges;
		populate_damcon_arena(&bridgelist[c->bridge].damcon);
	
		nbridges++;
		schedule_callback(event_callback, &callback_schedule,
				"player-respawn-event", (double) c->shipid);
	} else if (c->bridge != -1 && !app.new_ship) { /* join existing ship */
		fprintf(stderr, "%s: join existing ship\n", logprefix());
		c->shipid = bridgelist[c->bridge].shipid;
		c->ship_index = lookup_by_id(c->shipid);
		bridgelist[c->bridge].nclients++;
	} else if (c->bridge != -1 && app.new_ship) { /* ship already exists, can't create */
		fprintf(stderr, "%s: ship already exists, can't create\n", logprefix());
		pb_queue_to_client(c, snis_opcode_pkt("bb", OPCODE_ADD_PLAYER_ERROR,
				ADD_PLAYER_ERROR_SHIP_ALREADY_EXISTS));
		write_queued_updates_to_client(c, 4, &no_write_count);
		return -1;
	}

	/* FIXME: need to redesign this mess not to use a constant salt */
	snis_crypt(bridgelist[c->bridge].shipname, bridgelist[c->bridge].password,
			bridgelist[c->bridge].pwdhash, PWDHASHLEN, "$1$abcdefgh", 11);
	c->debug_ai = 0;
	c->waypoints_dirty = 1;
	c->request_universe_timestamp = 0;
	fprintf(stderr, "%s: queue client id %d\n", logprefix(), c->shipid);
	queue_up_client_id(c);

	c->go_clients = malloc(sizeof(*c->go_clients) * MAXGAMEOBJS);
	memset(c->go_clients, 0, sizeof(*c->go_clients) * MAXGAMEOBJS);
	c->damcon_data_clients = malloc(sizeof(*c->damcon_data_clients) * MAXDAMCONENTITIES);
	memset(c->damcon_data_clients, 0, sizeof(*c->damcon_data_clients) * MAXDAMCONENTITIES);

	return 0;

protocol_error:
	log_client_info(SNIS_ERROR, c->socket, "disconnected, protocol error\n");
	close(c->socket);
	return -1;
}

static void snis_server_cross_check_opcodes(struct game_client *c)
{
	uint8_t i;

	for (i = snis_first_opcode(); i != 255; i = snis_next_opcode(i)) {
		/* fprintf(stderr, "snis_server: cross checking opcode %hhu, '%s'\n",
			i, snis_opcode_format(i)); */
		pb_queue_to_client(c, packed_buffer_new("bbb",
				OPCODE_CHECK_OPCODE_FORMAT,
				OPCODE_CHECK_OPCODE_QUERY, i));
	}
	/* fprintf(stderr, "snis_server: submitted last cross check\n"); */
}

/* Creates a thread for each incoming connection... */
static void service_connection(int connection)
{
	int i, j, rc, flag = 1;
	int bridgenum, client_count;
	int thread_count, iterations;

	log_client_info(SNIS_INFO, connection, "servicing snis_client connection\n");
        /* get connection moved off the stack so that when the thread needs it,
	 * it's actually still around. 
	 */

	i = setsockopt(connection, IPPROTO_TCP, TCP_NODELAY, (char *) &flag, sizeof(int));
	if (i < 0)
		snis_log(SNIS_ERROR, "setsockopt failed: %s.\n", strerror(errno));

	if (verify_client_protocol(connection)) {
		log_client_info(SNIS_ERROR, connection, "disconnected, protocol violation\n");
		fprintf(stderr, "%s: connection terminated protocol violation\n", logprefix());
		close(connection);
		return;
	}
	fprintf(stderr, "%s: connection 3\n", logprefix());

	pthread_mutex_lock(&universe_mutex);
	client_lock();
	if (nclients >= MAXCLIENTS) {
		client_unlock();
		pthread_mutex_unlock(&universe_mutex);
		snis_log(SNIS_ERROR, "Too many clients.\n");
		/* TODO: isn't there more to do here?  Close the socket, etc? */
		return;
	}

	/* Set i to a free slot in client[], or to nclients if none free */
	for (i = 0; i < nclients; i++)
		if (client[i].refcount == 0)
			break;
	if (i == nclients)
		nclients++;

	client[i].socket = connection;
	client[i].timestamp = 0;  /* newborn client, needs everything */
	client[i].current_station = 255; /* unknown */

	log_client_info(SNIS_INFO, connection, "add new player\n");

	pthread_mutex_init(&client[i].client_write_queue_mutex, NULL);
	packed_buffer_queue_init(&client[i].client_write_queue);

	fprintf(stderr, "%s: calling add new player\n", logprefix());
	rc = add_new_player(&client[i]);
	if (rc) {
		nclients--;
		client_unlock();
		pthread_mutex_unlock(&universe_mutex);
		return;
	}


	/* initialize refcount to 1 to keep client[i] from getting reaped. */
	client[i].refcount = 1;

	/* create threads... */
	rc = create_thread(&client[i].read_thread,
		per_client_read_thread, (void *) &client[i], "sniss-reader", 1);
	if (rc) {
		snis_log(SNIS_ERROR, "per client read thread, create_thread failed: %d %s %s\n",
			rc, strerror(rc), strerror(errno));
	}
	rc = create_thread(&client[i].write_thread,
		per_client_write_thread, (void *) &client[i], "sniss-writer", 1);
	if (rc) {
		snis_log(SNIS_ERROR, "per client write thread, create_thread failed: %d %s %s\n",
			rc, strerror(rc), strerror(errno));
	}
	client_count = 0;
	bridgenum = client[i].bridge;
	for (j = 0; j < nclients; j++) {
		if (client[j].refcount && client[j].bridge == bridgenum)
			client_count++;
	} 

	client_unlock();
	pthread_mutex_unlock(&universe_mutex);

	if (client_count == 1) {
		/* snis_queue_add_global_sound(STARSHIP_JOINED); */
		snis_queue_add_global_text_to_speech("star ship has joined");
	} else {
		/* snis_queue_add_sound(CREWMEMBER_JOINED, ROLE_ALL,
					bridgelist[bridgenum].shipid); */
		snis_queue_add_text_to_speech("crew member has joined.", ROLE_TEXT_TO_SPEECH,
					bridgelist[bridgenum].shipid);
	}

	/* Wait for at least one of the threads to prevent premature reaping */
	iterations = 0;
	do {
		pthread_mutex_lock(&universe_mutex);
		client_lock();
		thread_count = client[i].refcount;
		client_unlock();
		pthread_mutex_unlock(&universe_mutex);
		if (thread_count < 2)
			usleep(1000);
		iterations++;
		if (iterations > 1000 && (iterations % 1000) == 0)
			printf("Way too many iterations at %s:%d\n", __FILE__, __LINE__);
	} while (thread_count < 2);

	/* release this thread's reference */
	pthread_mutex_lock(&universe_mutex);
	client_lock();
	put_client(&client[i]);
	client_unlock();
	pthread_mutex_unlock(&universe_mutex);

	snis_server_cross_check_opcodes(&client[i]);
	queue_set_solarsystem(&client[i]);
	snis_log(SNIS_INFO, "bottom of 'service connection'\n");
}

static pthread_t listener_thread;

/* This thread listens for incoming client connections, and
 * on establishing a connection, starts a thread for that 
 * connection.
 */
static void *listener_thread_fn(__attribute__((unused)) void *unused)
{
	int rendezvous, connection, rc;
        struct sockaddr_in remote_addr;
        socklen_t remote_addr_len;
	uint16_t port;
	struct addrinfo hints;
	struct addrinfo *result, *rp;
	int s;
	char portstr[20];
	char *snis_server_port_var;

        snis_log(SNIS_INFO, "snis_server starting\n");
	snis_server_port_var = getenv("SNISSERVERPORT");
	default_snis_server_port = -1;
	if (snis_server_port_var != NULL) {
		int rc, value;
		rc = sscanf(snis_server_port_var, "%d", &value);
		if (rc == 1) {
			default_snis_server_port = value & 0x0ffff;
			printf("%s: Using SNISSERVERPORT value %d\n",
				logprefix(), default_snis_server_port);
		}
	}

	/* Bind "rendezvous" socket to a random port to listen for connections.
	 * unless SNISSERVERPORT is defined, in which case, try to use that.
	 */
	while (1) {

		/* 
		 * choose a random port in the "Dynamic and/or Private" range
		 * see http://www.iana.org/assignments/port-numbers
		 */
		if (default_snis_server_port == -1)
			port = snis_randn(65335 - 49152) + 49151;
		else
			port = default_snis_server_port;
		snis_log(SNIS_INFO, "Trying port %d\n", port);
		snprintf(portstr, sizeof(portstr), "%d", port);
		memset(&hints, 0, sizeof(struct addrinfo));
		hints.ai_family = AF_UNSPEC;    /* Allow IPv4 or IPv6 */
		hints.ai_socktype = SOCK_STREAM;
		hints.ai_flags = AI_PASSIVE | AI_NUMERICSERV;    /* For wildcard IP address */
		hints.ai_protocol = 0;          /* Any protocol */
		hints.ai_canonname = NULL;
		hints.ai_addr = NULL;
		hints.ai_next = NULL;

		s = getaddrinfo(NULL, portstr, &hints, &result);
		if (s != 0) {
			snis_log(SNIS_ERROR, "getaddrinfo: %s\n", gai_strerror(s));
			exit(EXIT_FAILURE);
		}

		/* getaddrinfo() returns a list of address structures.
		 * Try each address until we successfully bind(2).
		 * If socket(2) (or bind(2)) fails, we (close the socket
		 * and) try the next address. 
		 */

		for (rp = result; rp != NULL; rp = rp->ai_next) {
			rendezvous = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
			if (rendezvous == -1)
				continue;

			if (bind(rendezvous, rp->ai_addr, rp->ai_addrlen) == 0)
				break;                  /* Success */
			close(rendezvous);
		}
		if (rp != NULL)
			break;
	}

	/* At this point, "rendezvous" is bound to a random port */
	snis_log(SNIS_INFO, "snis_server listening for connections on port %d\n", port);
	listener_port = port;

	/* Listen for incoming connections... */
	rc = listen(rendezvous, SOMAXCONN);
	if (rc < 0) {
		snis_log(SNIS_ERROR, "listen() failed, exiting: %s\n", strerror(errno));
		exit(1);
	}

	/* Notify other threads that the listener thread is ready... */
	(void) pthread_mutex_lock(&listener_mutex);
	pthread_cond_signal(&listener_started);
	(void) pthread_mutex_unlock(&listener_mutex);

	while (1) {
		remote_addr_len = sizeof(remote_addr);
		snis_log(SNIS_INFO, "Accepting connection...\n");
		connection = accept(rendezvous, (struct sockaddr *) &remote_addr, &remote_addr_len);
		snis_log(SNIS_INFO, "accept returned %d\n", connection);
		fprintf(stderr, "%s: accept returned %d\n", logprefix(), connection);
		if (connection < 0) {
			/* handle failed connection... */
			snis_log(SNIS_WARN, "accept() failed: %s\n", strerror(errno));
			ssgl_sleep(1);
			continue;
		}
		if (remote_addr_len != sizeof(remote_addr)) {
			snis_log(SNIS_ERROR, "strange socket address length %d\n", remote_addr_len);
			/* shutdown(connection, SHUT_RDWR);
			close(connection);
			continue; */
		}
		service_connection(connection);
	}
}

/* Starts listener thread to listen for incoming client connections.
 * Returns port on which listener thread is listening. 
 */
static int start_listener_thread(void)
{
	int rc;

	/* Setup to wait for the listener thread to become ready... */
	pthread_cond_init (&listener_started, NULL);
        (void) pthread_mutex_lock(&listener_mutex);

	/* Create the listener thread... */
	rc = create_thread(&listener_thread, listener_thread_fn, NULL, "sniss-listener", 1);
	if (rc) {
		snis_log(SNIS_ERROR, "Failed to create listener thread, create_thread: %d %s %s\n",
				rc, strerror(rc), strerror(errno));
	}

	/* Wait for the listener thread to become ready... */
	pthread_cond_wait(&listener_started, &listener_mutex);
	(void) pthread_mutex_unlock(&listener_mutex);
	snis_log(SNIS_INFO, "Listener started.\n");
	
	return listener_port;
}

static void count_socket_as_empty(float *damage, int system)
{
	/* Empty socket counts as full damage for that part */
	damage[system] += 255.0f / (float) DAMCON_PARTS_PER_SYSTEM;
}

static void move_damcon_entities_on_bridge(int bridge_number)
{
	int i, j;
	struct damcon_data *d = &bridgelist[bridge_number].damcon;
	struct snis_damcon_entity *socket, *part;
	float damage[DAMCON_SYSTEM_COUNT - 1];
	int nobjs;

	for (i = 0; i < DAMCON_SYSTEM_COUNT - 1; i++)
		damage[i] = 0.0f;

	if (!d->pool)
		return;

	nobjs = snis_object_pool_highest_object(d->pool);
	for (i = 0; i <= nobjs; i++) {
		int id;

		if (d->o[i].move)
			d->o[i].move(&d->o[i], d);

		/* As long as we're iterating over all the damcon objects
		 * We may as well compute cumulative damage
		 */
		if (d->o[i].type != DAMCON_TYPE_SOCKET)
			continue;
		socket = &d->o[i];
		if (socket->tsd.socket.system >= DAMCON_SYSTEM_COUNT - 1)
			continue;
		if (socket->tsd.socket.system < 0)
			continue;
		id = socket->tsd.socket.contents_id;
		if (id == DAMCON_SOCKET_EMPTY) {
			count_socket_as_empty(damage, socket->tsd.socket.system);
			continue;
		}

		j = lookup_by_damcon_id(d, id);
		if (j < 0) {
			printf("j unexpectedly zero at %s:%d\n", __FILE__, __LINE__);
			continue;
		}
		part = &d->o[j];

		/* part is in wrong system? */
		if (part->tsd.part.system != socket->tsd.socket.system) {
			count_socket_as_empty(damage, socket->tsd.socket.system);
			continue;
		}
		/* part is in right system, but wrong socket? */
		if (part->tsd.part.part != socket->tsd.socket.part) {
			count_socket_as_empty(damage, socket->tsd.socket.system);
			continue;
		}
		damage[part->tsd.part.system] +=
			(float) part->tsd.part.damage / (float) DAMCON_PARTS_PER_SYSTEM;
	}

	/* Update ship with damage info */
	int ship = lookup_by_id(bridgelist[bridge_number].shipid);
	if (ship < 0) {
		printf("ship unexpectedly negative at %s:%d\n", __FILE__, __LINE__);
		return;
	}
	struct snis_entity *o = &go[ship];

	BUILD_ASSERT(sizeof(o->tsd.ship.damage) == DAMCON_SYSTEM_COUNT - 1);
	int changed = 0;
	for (i = 0; i < DAMCON_SYSTEM_COUNT - 1; i++) {
		unsigned char *x = (unsigned char *) &o->tsd.ship.damage;
		if (x[i] != (unsigned char) damage[i]) {
			x[i] = (unsigned char) damage[i];
			changed = 1;
		}
	}
	if (changed)
		o->tsd.ship.damage_data_dirty = 1;
}

static void move_damcon_entities(void)
{
	int i;

	for (i = 0; i < nbridges; i++)
		move_damcon_entities_on_bridge(i);
}

#if GATHER_OPCODE_STATS
static int compare_opcode_stats(const void *a, const void *b)
{
	const struct opcode_stat *A = a;
	const struct opcode_stat *B = b;

	return B->bytes - A->bytes;
}

static void dump_opcode_stats(struct opcode_stat *data)
{
	int i;

	struct opcode_stat s[256];

	if ((universe_timestamp % 50) != 0)
		return;

	memcpy(s, data, sizeof(s));
	qsort(s, 256, sizeof(s[0]), compare_opcode_stats);

	printf("\n");
	printf("%3s  %10s %15s %9s %10s %15s\n",
		"Op", "Count", "total bytes", "bytes/op", "not sent", "saved bytes");
	for (i = 0; i < 20; i++)
		if (s[i].count != 0)
			printf("%3hu: %10llu %15llu %9llu %10llu %15llu\n",
				s[i].opcode, s[i].count, s[i].bytes, s[i].bytes / s[i].count,
				s[i].count_not_sent,
				s[i].count_not_sent * s[i].bytes / s[i].count);
		else
			printf("%3hu: %20llu %20llu %20s\n",
				s[i].opcode, s[i].count, s[i].bytes, "n/a");
	printf("\n");
}
#else
#define dump_opcode_stats(x)
#endif

static void wakeup_multiverse_writer(struct multiverse_server_info *msi);
static void queue_to_multiverse(struct multiverse_server_info *msi, struct packed_buffer *pb)
{
	assert(pb);
	packed_buffer_queue_add(&msi->mverse_queue, pb, &msi->queue_mutex);
	wakeup_multiverse_writer(msi);
}

static void update_multiverse(struct snis_entity *o)
{
	struct packed_buffer *pb;
	int bridge;

	if (!multiverse_server)
		return;

	bridge = lookup_bridge_by_shipid(o->id);
	if (bridge < 0) {
		fprintf(stderr,
			"%s: did not find bridge for id:%d, index=%ld, alive=%d: %s:%d\n",
				logprefix(), o->id, go_index(o), o->alive, __FILE__, __LINE__);
		return;
	}

	if (multiverse_debug)
		fprintf(stderr, "%s: update_multiverse: verified = '%s'\n", logprefix(),
			bridgelist[bridge].verified == BRIDGE_UNVERIFIED ? "unverified" :
			bridgelist[bridge].verified == BRIDGE_VERIFIED ? "verified" :
			bridgelist[bridge].verified == BRIDGE_FAILED_VERIFICATION ? "failed verification" :
			bridgelist[bridge].verified == BRIDGE_REFUSED ? "refused" : "unknown");

	/* Verify that this ship does not already exist if creation was requested, or
	 * that it does already exist if creation was not requested.  Only do this once.
	 */
	if (bridgelist[bridge].verified == BRIDGE_UNVERIFIED &&
		!bridgelist[bridge].requested_verification && multiverse_server->sock != -1) {
		unsigned char opcode;

		if (multiverse_debug)
			print_hash("requesting verification of hash ", bridgelist[bridge].pwdhash);
		pb = packed_buffer_allocate(PWDHASHLEN + 1);
		if (bridgelist[bridge].requested_creation)
			opcode = SNISMV_OPCODE_VERIFY_CREATE;
		else
			opcode = SNISMV_OPCODE_VERIFY_EXISTS;
		packed_buffer_append(pb, "br", opcode, bridgelist[bridge].pwdhash, (uint16_t) PWDHASHLEN);
		bridgelist[bridge].requested_verification = 1;
		queue_to_multiverse(multiverse_server, pb);
		return;
	} else {
		char reason[100];

		if (multiverse_debug) {
			snprintf(reason, sizeof(reason), "Requested already: %s, socket status: %s",
				bridgelist[bridge].requested_verification ? "yes" : "no",
				multiverse_server->sock >= 0 ? "valid" : "invalid");
			fprintf(stderr, "%s: not requesting verification: %s\n", logprefix(), reason);
		}
	}

	/* Skip updating multiverse server if the bridge isn't verified yet. */
	if (bridgelist[bridge].verified != BRIDGE_VERIFIED) {
		if (multiverse_debug)
			fprintf(stderr, "%s: bridge is not verified, not updating multiverse\n",
				logprefix());
		return;
	}

	/* Update the ship */
	pb = build_bridge_update_packet(o, bridgelist[bridge].pwdhash);
	if (packed_buffer_length(pb) != UPDATE_BRIDGE_PACKET_SIZE) {
		fprintf(stderr, "snis_multiverse: bridge packet size is wrong (actual: %d, nominal: %d)\n",
			packed_buffer_length(pb), UPDATE_BRIDGE_PACKET_SIZE);
		abort();
	}
	queue_to_multiverse(multiverse_server, pb);
}

static void move_objects(double absolute_time, int discontinuity)
{
	int i;
	struct lua_comms_transmission *lua_comms_transmission_queue_copy = NULL;

	pthread_mutex_lock(&universe_mutex);
	memset(faction_population, 0, sizeof(faction_population));
	netstats.nobjects = 0;
	netstats.nships = 0;
	universe_timestamp++;
	universe_timestamp_absolute = absolute_time;
	static uint32_t multiverse_update_time = 0;
	int b;

	if (discontinuity) {
		for (i = 0; i < nclients; i++)
			client[i].request_universe_timestamp = UPDATE_UNIVERSE_TIMESTAMP_COUNT;
	}

	dump_opcode_stats(write_opcode_stats);
	for (i = 0; i <= snis_object_pool_highest_object(pool); i++) {
		if (go[i].alive) {
			go[i].move(&go[i]);
			if (go[i].type == OBJTYPE_SHIP2 &&
				go[i].sdata.faction < ARRAYSIZE(faction_population)) {
				faction_population[go[i].sdata.faction]++;
			}
			if (go[i].type == OBJTYPE_SHIP1)  {
				b = lookup_bridge_by_shipid(go[i].id);
				if (b >= 0 && (universe_timestamp >= multiverse_update_time ||
					(!bridgelist[b].verified && !bridgelist[b].requested_verification)))
					update_multiverse(&go[i]);
			}
			netstats.nobjects++;
		} else {
			if (go[i].type == OBJTYPE_SHIP1)  {
				int b = lookup_bridge_by_shipid(go[i].id);
				if (b != -1) {
					if (universe_timestamp >= go[i].respawn_time) {
						respawn_player(&go[i], (uint8_t) -1);
						schedule_callback(event_callback, &callback_schedule,
							"player-respawn-event", (double) go[i].id);
						send_ship_damage_packet(&go[i]);
					} else {
						go[i].timestamp = universe_timestamp; /* respawn is counting down */
					}
				}
			} else {
				if (snis_object_pool_is_allocated(pool, i)) { /* dead, but not de-allocated */
					fprintf(stderr,
						"%s:%s:%s:%d BUG: dead object still allocated: id = %u, type = %d\n",
						logprefix(), __FILE__, __func__, __LINE__, go[i].id, go[i].type);
					delete_from_clients_and_server(&go[i]);
				}
			}
		}
	}
	/* Update multiverse every 10 seconds */
	if (universe_timestamp >= multiverse_update_time)
		multiverse_update_time = universe_timestamp + 100;

	for (i = 0; i < nfactions(); i++)
		if (i == 0 || faction_population[lowest_faction] > faction_population[i])
			lowest_faction = i;
	move_damcon_entities();

	/* copy and clear the lua comms queue head pointer while holding the lock */
	lua_comms_transmission_queue_copy = lua_comms_transmission_queue;
	lua_comms_transmission_queue = NULL;

	pthread_mutex_unlock(&universe_mutex);

	fire_lua_timers();
	fire_lua_callbacks(&callback_schedule);
	fire_lua_proximity_checks();
	send_queued_lua_comms_transmissions(&lua_comms_transmission_queue_copy);
}

static void register_with_game_lobby(char *lobbyhost, int port,
	char *servernick, char *gameinstance, char *location)
{
	struct ssgl_game_server gs;
	struct in_addr ip;
	char *v;

	memset(&gs, 0, sizeof(gs));
	snis_log(SNIS_INFO, "port = %hu\n", port);
	gs.port = htons(port);
	snis_log(SNIS_INFO, "gs.port = %hu\n", gs.port);
		
	strncpy(gs.server_nickname, servernick, 14);
	strncpy(gs.protocol_version, SNIS_PROTOCOL_VERSION, sizeof(gs.protocol_version));
	strncpy(gs.game_instance, gameinstance, 19);
	strncpy(gs.location, location, 19);
	strcpy(gs.game_type, "SNIS");

	if (ssgl_get_primary_host_ip_addr(&gs.ipaddr) != 0) {
		snis_log(SNIS_WARN, "Failed to get local IP address.\n");
		fprintf(stderr, "%s: Failed to get local IP address.\n", logprefix());
		v = getenv("SSGL_PRIMARY_IP_PROBE_ADDR");
		if (!v)
			fprintf(stderr,
				"%s: You may need to set SSGL_PRIMARY_IP_PROBE_ADDR environment variable\n",
				logprefix());
		else
			fprintf(stderr,
				"%s: You may have set SSGL_PRIMARY_IP_PROBE_ADDR incorrectly (currently '%s')\n",
				logprefix(), v);
		exit(1); /* No point in continuing, since we have no IP address to register with the lobby. */
	}
	ip.s_addr = gs.ipaddr;
	fprintf(stderr, "%s: Registering IP/port as %s:%d\n", logprefix(), inet_ntoa(ip), port);
	if (ssgl_register_gameserver(lobbyhost, &gs, &lobbythread, &nclients))
		snis_log(SNIS_WARN, "Game server registration failed.\n");
	else	
		snis_log(SNIS_INFO, "Game server registered.\n");
	return;	
}

static void usage(void)
{
	fprintf(stderr, "snis_server: usage:\n");
	fprintf(stderr, "snis_server -l lobbyhost -L location \\\n"
			"          [ -m multiverse-location ] [ -n servernick ]\n");
	fprintf(stderr, "For example: snis_server -l lobbyserver -g 'steves game' -n zuul -L Houston\n");
	exit(0);
}

static void open_log_file(void)
{
	char *loglevelstring;
	int ll, rc;

	rc = snis_open_logfile("SNIS_SERVER_LOGFILE");
	loglevelstring = getenv("SNIS_LOG_LEVEL");
	if (rc == 0 && loglevelstring && sscanf(loglevelstring, "%d", &ll) == 1) {
		snis_log_level = ll;
		snis_set_log_level(snis_log_level);
	}
}

static void add_lua_callable_fn(int (*fn)(lua_State *l), char *lua_fn_name)
{
	lua_pushcfunction(lua_state, fn);
	lua_setglobal(lua_state, lua_fn_name);
}

static void setup_lua(void)
{
	int dofile = -1;

	lua_state = luaL_newstate();
	luaL_openlibs(lua_state);
	dofile = luaL_dostring(lua_state, "print(\"Lua setup done.\");");
	if (dofile) {
		fprintf(stderr, "Lua setup failed. Lua scripts will be unavailable.\n");
		lua_close(lua_state);
		lua_state = NULL;
		return;
	}
	add_lua_callable_fn(l_clear_all, "clear_all");
	add_lua_callable_fn(l_add_random_ship, "add_random_ship");
	add_lua_callable_fn(l_add_ship, "add_ship");
	add_lua_callable_fn(l_add_asteroid, "add_asteroid");
	add_lua_callable_fn(l_set_asteroid_speed, "set_asteroid_speed");
	add_lua_callable_fn(l_add_starbase, "add_starbase");
	add_lua_callable_fn(l_add_planet, "add_planet");
	add_lua_callable_fn(l_add_nebula, "add_nebula");
	add_lua_callable_fn(l_add_spacemonster, "add_spacemonster");
	add_lua_callable_fn(l_add_derelict, "add_derelict");
	add_lua_callable_fn(l_derelict_set_ships_log, "derelict_set_ships_log");
	add_lua_callable_fn(l_add_wormhole_pair, "add_wormhole_pair");
	add_lua_callable_fn(l_get_player_ship_ids, "get_player_ship_ids");
	add_lua_callable_fn(l_get_object_location, "get_object_location");
	add_lua_callable_fn(l_move_object, "move_object");
	add_lua_callable_fn(l_set_object_velocity, "set_object_velocity");
	add_lua_callable_fn(l_set_object_orientation, "set_object_orientation");
	add_lua_callable_fn(l_align_object_towards, "align_object_towards");
	add_lua_callable_fn(l_set_object_rotational_velocity, "set_object_rotational_velocity");
	add_lua_callable_fn(l_set_object_relative_position, "set_object_relative_position");
	add_lua_callable_fn(l_delete_object, "delete_object");
	add_lua_callable_fn(l_register_callback, "register_callback");
	add_lua_callable_fn(l_register_timer_callback, "register_timer_callback");
	add_lua_callable_fn(l_comms_transmission, "comms_transmission");
	add_lua_callable_fn(l_comms_enciphered_transmission, "comms_enciphered_transmission");
	add_lua_callable_fn(l_get_object_name, "get_object_name");
	add_lua_callable_fn(l_get_player_damage, "get_player_damage");
	add_lua_callable_fn(l_set_player_damage, "set_player_damage");
	add_lua_callable_fn(l_load_skybox, "load_skybox");
	add_lua_callable_fn(l_user_coords, "user_coords");
	add_lua_callable_fn(l_ai_push_patrol, "ai_push_patrol");
	add_lua_callable_fn(l_ai_push_attack, "ai_push_attack");
	add_lua_callable_fn(l_add_cargo_container, "add_cargo_container");
	add_lua_callable_fn(l_set_faction, "set_faction");
	add_lua_callable_fn(l_text_to_speech, "text_to_speech");
	add_lua_callable_fn(l_show_timed_text, "show_timed_text");
	add_lua_callable_fn(l_enqueue_lua_script, "enqueue_lua_script");
	add_lua_callable_fn(l_register_proximity_callback, "register_proximity_callback");
	add_lua_callable_fn(l_set_asteroid_minerals, "set_asteroid_minerals");
	add_lua_callable_fn(l_get_ship_attribute, "get_ship_attribute");
	add_lua_callable_fn(l_get_commodity_name, "get_commodity_name");
	add_lua_callable_fn(l_get_commodity_units, "get_commodity_units");
	add_lua_callable_fn(l_lookup_commodity, "lookup_commodity");
	add_lua_callable_fn(l_set_commodity_contents, "set_commodity_contents");
	add_lua_callable_fn(l_add_commodity, "add_commodity");
	add_lua_callable_fn(l_reset_player_ship, "reset_player_ship");
	add_lua_callable_fn(l_show_menu, "show_menu");
	add_lua_callable_fn(l_add_turret, "add_turret");
	add_lua_callable_fn(l_add_block, "add_block");
	add_lua_callable_fn(l_add_turrets_to_block_face, "add_turrets_to_block_face");
	add_lua_callable_fn(l_comms_channel_listen, "comms_channel_listen");
	add_lua_callable_fn(l_comms_channel_unlisten, "comms_channel_unlisten");
	add_lua_callable_fn(l_comms_channel_transmit, "comms_channel_transmit");
	add_lua_callable_fn(l_add_black_hole, "add_black_hole");
	add_lua_callable_fn(l_attach_science_text, "attach_science_text");
	add_lua_callable_fn(l_add_explosion, "add_explosion");
	add_lua_callable_fn(l_dock_player_to_starbase, "dock_player_to_starbase");
	add_lua_callable_fn(l_object_distance, "object_distance");
	add_lua_callable_fn(l_enable_antenna_aiming, "enable_antenna_aiming");
	add_lua_callable_fn(l_disable_antenna_aiming, "disable_antenna_aiming");
	add_lua_callable_fn(l_set_custom_button_label, "set_custom_button_label");
	add_lua_callable_fn(l_enable_custom_button, "enable_custom_button");
	add_lua_callable_fn(l_disable_custom_button, "disable_custom_button");
	add_lua_callable_fn(l_fire_missile, "fire_missile");
	add_lua_callable_fn(l_regenerate_universe, "regenerate_universe");
	add_lua_callable_fn(l_set_variable, "set_variable");
	add_lua_callable_fn(l_demon_print, "demon_print");
	add_lua_callable_fn(l_add_bounty, "add_bounty");
	add_lua_callable_fn(l_random_point_on_sphere, "random_point_on_sphere");
	add_lua_callable_fn(l_generate_ship_name, "generate_ship_name");
	add_lua_callable_fn(l_generate_character_name, "generate_character_name");
	add_lua_callable_fn(l_generate_name, "generate_name");
	add_lua_callable_fn(l_ai_trace, "ai_trace");
	add_lua_callable_fn(l_create_passenger, "create_passenger");
	add_lua_callable_fn(l_set_passenger_location, "set_passenger_location");
	add_lua_callable_fn(l_get_passenger_location, "get_passenger_location");
	add_lua_callable_fn(l_set_planet_description, "set_planet_description");
	add_lua_callable_fn(l_set_planet_government, "set_planet_government");
	add_lua_callable_fn(l_set_planet_tech_level, "set_planet_tech_level");
	add_lua_callable_fn(l_set_planet_economy, "set_planet_economy");
	add_lua_callable_fn(l_set_planet_security, "set_planet_security");
	add_lua_callable_fn(l_update_player_wallet, "update_player_wallet");
	add_lua_callable_fn(l_ai_push_catatonic, "ai_push_catatonic");
	add_lua_callable_fn(l_too_close_to_planet_or_sun, "too_close_to_planet_or_sun");
	add_lua_callable_fn(l_play_sound, "play_sound");
	add_lua_callable_fn(l_set_red_alert_status, "set_red_alert_status");
	add_lua_callable_fn(l_destroy_ship, "destroy_ship");
	add_lua_callable_fn(l_add_torpedo, "add_torpedo");
	add_lua_callable_fn(l_set_starbase_factions_allowed, "set_starbase_factions_allowed");
	add_lua_callable_fn(l_generate_cipher_key, "generate_cipher_key");
	add_lua_callable_fn(l_get_faction_name, "get_faction_name");
}

static int run_initial_lua_scripts(void)
{
	int rc;
	struct stat statbuf;
	char scriptname[PATH_MAX];

	snprintf(scriptname, sizeof(scriptname) - 1,
			"%s/%s", LUASCRIPTDIR, initial_lua_script);
	rc = stat(scriptname, &statbuf);
	if (rc != 0)
		return rc;
	rc = luaL_dofile(lua_state, scriptname);
	if (rc) {
		char errmsg[1000];
		snprintf(errmsg, sizeof(errmsg) - 1, "ERROR IN SCRIPT %s", initial_lua_script);
		send_demon_console_color_msg(YELLOW, "%s", errmsg);
		fprintf(stderr, "%s\n", errmsg);

		snprintf(errmsg, sizeof(errmsg) - 1, "LUA: %s",
			lua_tostring(lua_state, -1));
		send_demon_console_color_msg(YELLOW, "%s", errmsg);
		fprintf(stderr, "%s\n", errmsg);
	}
	return rc;
}

static void print_lua_error_message(char *error_context, char *lua_command)
{
	char error_msg[1000];

	if (lua_command)
		snprintf(error_msg, sizeof(error_msg) - 1, "%s %s", error_context, lua_command);
	else
		snprintf(error_msg, sizeof(error_msg) - 1, "%s", error_context);
	fprintf(stderr, "%s", error_msg);
	send_demon_console_color_msg(YELLOW, "%s", error_msg);

	if (lua_command) {
		snprintf(error_msg, sizeof(error_msg) - 1, "LUA: %s",
			lua_tostring(lua_state, -1));
		fprintf(stderr, "%s", error_msg);
		send_demon_console_color_msg(YELLOW, "%s", error_msg);
	}
}

/* Parse a lua command into tokens.  Input is lua_command, output is arg[],
 * full of allocated strings. Return is the number of args.
 */
static int tokenize_lua_command_args(char *lua_command, char *arg[], int maxargs)
{
	char *saveptr;
	char *cmd, *c, *token;
	int i = 0;

	cmd = strdup(lua_command);
	c = cmd;
	do {
		token = strtok_r(c, " ", &saveptr);
		c = NULL;
		if (!token)
			break;
		arg[i] = strdup(token);
		i++;
	} while (i < maxargs);
	free(cmd);
	return i;
}

static void free_lua_command_tokens(char *arg[], int nargs, int maxargs)
{
	int i;

	for (i = 0; i < nargs; i++) {
		free(arg[i]);
		arg[i] = NULL;
	}
	memset(arg, 0, sizeof(arg[0]) * maxargs);
}

static char *find_lua_script(char *base_script_name)
{
	char path[PATH_MAX];
	struct stat statbuf;
	struct dirent **namelist;
	int n, i, rc;

	/* First, try the obvious. */
	snprintf(path, sizeof(path) - 1, "%s/%s.LUA", LUASCRIPTDIR, base_script_name);
	rc = stat(path, &statbuf);
	if (rc == 0)
		return strdup(path);

	/* Maybe it's in a subdirectory */
	n = scandir(LUASCRIPTDIR, &namelist, filter_directories, alphasort);
	if (n < 0) {
		send_demon_console_color_msg(YELLOW, "FAILED TO SCAN %s: %s", LUASCRIPTDIR, strerror(errno));
		return NULL;
	}
	for (i = 0; i < n; i++) {
		snprintf(path, sizeof(path) - 1, "%s/%s/%s.LUA", LUASCRIPTDIR,
				namelist[i]->d_name, base_script_name);
		rc = stat(path, &statbuf);
		free(namelist[i]);
		if (rc == 0)
			return strdup(path);
	}
	free(namelist);
	return NULL;
}

static void process_lua_commands(void)
{
	char lua_command[PATH_MAX], *scriptname = NULL;
	char *arg[16];
	int rc, nargs;

	pthread_mutex_lock(&universe_mutex);
	for (;;) {
		dequeue_lua_command(lua_command, sizeof(lua_command));
		if (lua_command[0] == '\0') /* empty string */
			break;
		pthread_mutex_unlock(&universe_mutex);

		nargs = tokenize_lua_command_args(lua_command, arg, ARRAYSIZE(arg));
		if (nargs <= 0) {
			send_demon_console_color_msg(YELLOW, "NARGS IS UNEXPECTEDLY %d", nargs);
			pthread_mutex_lock(&universe_mutex);
			continue;
		}

		scriptname = find_lua_script(arg[0]);
		if (!scriptname) {
			print_lua_error_message("SCRIPT NOT FOUND", scriptname);
			free_lua_command_tokens(arg, nargs, ARRAYSIZE(arg));
			pthread_mutex_lock(&universe_mutex);
			continue;
		}
		rc = luaL_loadfile(lua_state, scriptname);
		if (rc) {
			print_lua_error_message("ERROR LOADING", scriptname);
			free_lua_command_tokens(arg, nargs, ARRAYSIZE(arg));
			free(scriptname);
			pthread_mutex_lock(&universe_mutex);
			continue;
		}
		free(scriptname);

		for (int i = 0; i < nargs; i++) /* Push the args. */
			lua_pushstring(lua_state, arg[i]);

		if (lua_instruction_count_limit > 0)
			lua_sethook(lua_state, runaway_lua_script_detected, LUA_MASKCOUNT, lua_instruction_count_limit);

		rc = lua_pcall(lua_state, nargs, 0, 0); /* Call the script */
		if (rc)
			print_lua_error_message("ERROR IN SCRIPT", arg[0]);

		if (lua_instruction_count_limit > 0)
			lua_sethook(lua_state, runaway_lua_script_detected, LUA_MASKCOUNT, 0);

		free_lua_command_tokens(arg, nargs, ARRAYSIZE(arg));
		pthread_mutex_lock(&universe_mutex);
	}
	pthread_mutex_unlock(&universe_mutex);
}

static void lua_teardown(void)
{
	lua_close(lua_state);
	lua_state = NULL;
}

/* Figure out what we want to build at the given starbase and add it to the build queue */
static void rts_ai_figure_what_to_build(struct snis_entity *base)
{
	/* TODO: something much better here
	 * For now, the answer is always "build a troop ship!", except sometimes
	 * build a resupply ship.
	 */
	if (snis_randn(5000) < 1000)
		rts_ai_add_to_build_queue(100, RTS_UNIT_RESUPPLY_SHIP, base->id);
	else
		rts_ai_add_to_build_queue(100, RTS_UNIT_TROOP_SHIP, base->id);
}

/* starbase_index is array of indices into go[], nstarbases is how many
 * Should be only starbases that we own.
 */
static void rts_ai_process_build_queue(int starbase_index[], int nstarbases)
{
	int i, j, do_increment, index;
	rts_ai_sort_build_queue();

	for (i = 0; i < rts_ai_build_queue_size; /* no auto increment */) {
		struct rts_ai_build_queue_entry *e = &rts_ai_build_queue[i];
		do_increment = 1;
		if (e->builder_id == rts_ai.main_base_id) {
			index = lookup_by_id(rts_ai.main_base_id);
			if (index >= 0) {
				struct snis_entity *mb = &go[index];
				float cost = rts_unit_type(e->unittype)->cost_to_build;
				if (mb->tsd.planet.time_left_to_build != 0)
					break; /* The main base is busy */
				if (cost > rts_ai.wallet)
					break; /* Costs too much */
				/* Lets build it */
				mb->tsd.planet.time_left_to_build = rts_unit_type(e->unittype)->time_to_build;
				mb->tsd.planet.build_unit_type = e->unittype;
				rts_ai.wallet -= cost;
				rts_ai_build_queue_remove_entry(i); /* Delete this work item */
				do_increment = 0;
			}
		} else {
			for (j = 0; j < nstarbases; j++) {
				if (go[starbase_index[j]].id == e->builder_id) {
					struct snis_entity *sb = &go[starbase_index[j]];
					float cost = rts_unit_type(e->unittype)->cost_to_build;
					if (sb->tsd.starbase.time_left_to_build != 0)
						break; /* This starbase is busy */
					if (cost > rts_ai.wallet)
						break; /* Costs too much */
					/* Lets build it */
					sb->tsd.starbase.time_left_to_build = rts_unit_type(e->unittype)->time_to_build;
					sb->tsd.starbase.build_unit_type = e->unittype;
					rts_ai.wallet -= cost;
					rts_ai_build_queue_remove_entry(i); /* Delete this work item */
					do_increment = 0; /* Do not advance, since we deleted one */
				}
			}
		}
		if (do_increment) /* Advance to next work item if this one was not deleted */
			i++;
	}
	for (i = 0; i < rts_ai_build_queue_size; i++) {
		struct rts_ai_build_queue_entry *e = &rts_ai_build_queue[i];
		printf("Incrementing age, age = %d\n", e->age);
		e->age++;
		if ((e->age % 50) == 0 && e->priority > 0) /* boost priority based on age in queue */
			e->priority--;
	}
}

static void rts_ai_assign_orders_to_units(int starbase_count, int unit_count)
{
	uint8_t orders, per_unit_orders;
	int i;

	/* TODO: something much better here */

	if (starbase_count < 7 || unit_count < 40)
		orders = RTS_ORDERS_OCCUPY_NEAR_BASE;
	else
		orders = RTS_ORDERS_ATK_MAIN_BASE;

	for (i = 0; i <= snis_object_pool_highest_object(pool); i++) {
		struct snis_entity *o = &go[i];
		if (o->alive && o->type == OBJTYPE_SHIP2 && o->sdata.faction == rts_ai.faction) {
			int unit_type = ship_type[o->tsd.ship.shiptype].rts_unit_type;
			if (unit_type == RTS_UNIT_RESUPPLY_SHIP)
				per_unit_orders = RTS_ORDERS_RESUPPLY;
			else
				per_unit_orders = orders;
			if (o->tsd.ship.ai[0].ai_mode != per_unit_orders + AI_MODE_RTS_FIRST_COMMAND &&
				rts_ai.wallet > rts_order_type(per_unit_orders)->cost_to_order) {
				o->tsd.ship.ai[0].ai_mode = per_unit_orders + AI_MODE_RTS_FIRST_COMMAND;
				rts_ai.wallet -= rts_order_type(per_unit_orders)->cost_to_order;
				switch (per_unit_orders + AI_MODE_RTS_FIRST_COMMAND) {
				case AI_MODE_RTS_ATK_MAIN_BASE:
					o->tsd.ship.ai[0].u.atk_main_base.base_id = (uint32_t) -1;
					break;
				case AI_MODE_RTS_OCCUPY_NEAR_BASE:
					o->tsd.ship.ai[0].u.occupy_base.base_id = (uint32_t) -1;
					break;
				default:
					break;
				}
			}
		}
	}
}

static void update_multiverse_bridge_count(double current_time)
{
	static double last_time = 0;
	uint32_t bridge_count;

	if (current_time - last_time < 5.0)
		return;

	if (!multiverse_server)
		return;

	bridge_count = nbridges;
	queue_to_multiverse(multiverse_server, packed_buffer_new("bw",
				SNISMV_OPCODE_UPDATE_BRIDGE_COUNT, bridge_count));
	last_time = current_time;
}

static void rts_ai_run(void)
{
	int i;
	int starbase_count = 0;
	int unit_count = 0;
	int starbase_index[NUM_RTS_BASES];

	if (!rts_ai.active || !rts_mode)
		return;

	/* Find all the starbases we own */
	for (i = 0; i <= snis_object_pool_highest_object(pool); i++) {
		struct snis_entity *o = &go[i];
		if (o->alive && o->type == OBJTYPE_STARBASE && o->sdata.faction == rts_ai.faction) {
			starbase_index[starbase_count] = i;
			starbase_count++;
		}
		if (o->alive && o->type == OBJTYPE_SHIP2 && o->sdata.faction == rts_ai.faction)
			unit_count++;
	};

	/* Each tick of the clock figure out what we would like to build at a different starbase */
	if (starbase_count != 0)
		rts_ai_figure_what_to_build(&go[starbase_index[universe_timestamp % starbase_count]]);
	if (starbase_count == 0 || (universe_timestamp % starbase_count) == 0) {
		/* Also figure out what we want to build at our main base sometimes */
		i = lookup_by_id(rts_ai.main_base_id);
		if (i >= 0)
			rts_ai_figure_what_to_build(&go[i]);
	}
	rts_ai_process_build_queue(starbase_index, starbase_count); /* Build stuff */
	rts_ai_assign_orders_to_units(starbase_count, unit_count);
	/* Adjust wallet money */
	rts_ai.wallet += starbase_count * RTS_WALLET_REFRESH_PER_BASE_PER_TICK +
				RTS_WALLET_REFRESH_MINIMUM;
}

static int read_ship_types(void)
{
	char path[PATH_MAX];

	snprintf(path, sizeof(path), "%s/%s", asset_dir, "ship_types.txt");

	ship_type = snis_read_ship_types(path, &nshiptypes);
	if (!ship_type) {
		fprintf(stderr, "Unable to read ship types from %s\n", path);
		if (errno)
			fprintf(stderr, "%s: %s\n", path, strerror(errno));
		return -1;
	}
	return 0;
}

static int read_factions(void)
{
	char path[PATH_MAX];

	snprintf(path, sizeof(path), "%s/%s", asset_dir, "factions.txt");

	if (snis_read_factions(path)) {
		fprintf(stderr, "Unable to read factions from %s", path);
		if (errno)
			fprintf(stderr, "%s: %s\n", path, strerror(errno));
		return -1;
	}
	return 0;
}

static struct solarsystem_asset_spec *read_solarsystem_assets(char *solarsystem_name)
{
	char path[PATH_MAX];
	struct solarsystem_asset_spec *s;

	snprintf(path, sizeof(path), "%s/solarsystems/%s/assets.txt", asset_dir, solarsystem_name);

	s = solarsystem_asset_spec_read(replacement_asset_lookup(path, &replacement_assets));
	if (!s) {
		fprintf(stderr, "Unable to read solarsystem asset spec from '%s'", path);
		if (errno)
			fprintf(stderr, "%s: %s\n", path, strerror(errno));
	} else if (s->spec_warnings || s->spec_errors) {
		fprintf(stderr, "Solarsystem asset spec %s: %d errors, %d warnings.\n",
			path, s->spec_errors, s->spec_warnings);
	}
	return s;
}

static void take_your_locale_and_shove_it(void)
{
	/* need this so that fscanf can read floats properly */
#define LOCALE_THAT_WORKS "C"
	if (setenv("LANG", LOCALE_THAT_WORKS, 1) < 0)
		fprintf(stderr, "Failed to setenv LANG to '%s'\n",
				LOCALE_THAT_WORKS);
	setlocale(LC_ALL, "C");
}

/*****************************************************************************************
 * Here begins the natural language parsing code.
 *****************************************************************************************/

/* callback used by natural language parser to look up game objects */
static uint32_t natural_language_object_lookup(void *context, char *word)
{
	uint32_t answer = 0xffffffff; /* not found */
	int i, b;

	char *w = strdup(word);
	uppercase(w);
	pthread_mutex_lock(&universe_mutex);
	for (i = 0; i <= snis_object_pool_highest_object(pool); i++) {
		switch (go[i].type) {
		case OBJTYPE_STARBASE:
			if (strcmp(go[i].sdata.name, w) == 0) {
				answer = go[i].id;
				goto done;
			}
			break;
		case OBJTYPE_PLANET:
		case OBJTYPE_ASTEROID:
		case OBJTYPE_SHIP2:
		case OBJTYPE_WARPGATE:
			if (strcmp(go[i].sdata.name, w) == 0) {
				answer = go[i].id;
				goto done;
			}
			break;
		case OBJTYPE_SHIP1:
			b = lookup_bridge_by_shipid(go[i].id);
			if (b < 0)
				break;
			if (strcasecmp((char *) bridgelist[b].shipname, w) == 0) {
				answer = go[i].id;
				goto done;
			}
			break;
		default:
			break;
		}
	}
done:
	pthread_mutex_unlock(&universe_mutex);
	free(w);
	return answer;
}

static void perform_natural_language_request(struct game_client *c, char *txt)
{
	lowercase(txt);
	printf("%s\n", txt);
	snis_nl_parse_natural_language_request(c, txt);
}

static void init_synonyms(void)
{
	snis_nl_add_synonym("exit", "leave");
	snis_nl_add_synonym("velocity", "speed");
	snis_nl_add_synonym("max", "maximum");
	snis_nl_add_synonym("min", "minimum");
	snis_nl_add_synonym("cut", "lower");
	snis_nl_add_synonym("reduce", "lower");
	snis_nl_add_synonym("decrease", "lower");
	snis_nl_add_synonym("less", "lower");
	snis_nl_add_synonym("boost", "raise");
	snis_nl_add_synonym("increase", "raise");
	snis_nl_add_synonym("more", "raise");
	snis_nl_add_synonym("calculate", "compute");
	snis_nl_add_synonym("figure", "compute");
	snis_nl_add_synonym("activate", "engage");
	snis_nl_add_synonym("actuate", "engage");
	snis_nl_add_synonym("start", "engage");
	snis_nl_add_synonym("energize", "engage");
	snis_nl_add_synonym("deactivate", "disengage");
	snis_nl_add_synonym("deenergize", "disengage");
	snis_nl_add_synonym("disable", "disengage");
	snis_nl_add_synonym("shutdown", "disengage");
	snis_nl_add_synonym("deploy", "launch");
	snis_nl_add_synonym("path", "course");
	snis_nl_add_synonym("route", "course");
	snis_nl_add_synonym("towards", "toward");
	snis_nl_add_synonym("tell me about", "describe");
	snis_nl_add_synonym("energy", "power");
	snis_nl_add_synonym("manuevering", "maneuvering");
	snis_nl_add_synonym("cooling", "coolant");
	snis_nl_add_synonym("throttle", "impulse drive");
	snis_nl_add_synonym("engines", "impulse drive");
	snis_nl_add_synonym("warp power", "warp drive power");
	snis_nl_add_synonym("warp coolant", "warp drive coolant");
	snis_nl_add_synonym("tractor power", "tractor beam power");
	snis_nl_add_synonym("tractor coolant", "tractor beam coolant");
	snis_nl_add_synonym("impulse power", "impulse drive power");
	snis_nl_add_synonym("impulse coolant", "impulse drive coolant");
	snis_nl_add_synonym("docking magnets", "docking system");
	snis_nl_add_synonym("comms", "communications");
	snis_nl_add_synonym("counter clockwise", "counterclockwise");
	snis_nl_add_synonym("counter-clockwise", "counterclockwise");
	snis_nl_add_synonym("anti clockwise", "counterclockwise");
	snis_nl_add_synonym("anti-clockwise", "counterclockwise");
	snis_nl_add_synonym("anticlockwise", "counterclockwise");
	snis_nl_add_synonym("star base", "starbase");
	snis_nl_add_synonym("warp gate", "gate");
	snis_nl_add_synonym("far away is it", "far");
	snis_nl_add_synonym("far away is", "far");
	snis_nl_add_synonym("far away", "far");
	snis_nl_add_synonym("far is it", "far");
	snis_nl_add_synonym("far is", "far");
	snis_nl_add_synonym("black hole", "blackhole");
	snis_nl_add_synonym("space monster", "spacemonster");
	snis_nl_add_synonym("monster", "spacemonster");
	snis_nl_add_synonym("creature", "spacemonster");
	snis_nl_add_synonym("m. mysterium", "spacemonster");
	snis_nl_add_synonym("monstrum mysterium", "spacemonster");
	snis_nl_add_synonym("could you repeat", "repeat");
	snis_nl_add_synonym("say again", "repeat");
	snis_nl_add_synonym("say what", "repeat");
	snis_nl_add_synonym("come again", "repeat");
	snis_nl_add_synonym("pardon me", "repeat");
	snis_nl_add_synonym("what did you say", "repeat");
}

static const struct noun_description_entry {
	char *noun;
	char *description;
} noun_description[] = {
	{ "drive", "The warp drive is a powerful sub-nuclear device that enables faster than light space travel." },
	{ "warp drive", "The warp drive is a powerful sub-nuclear device that enables faster than light space travel." },
	{ "impulse drive", "The impulse drive is a sub light speed conventional thruster." },
	{ "blackhole", "A black hole is a singularity in space time with a gravitational field so intense "
		"that no matter or radiation is able to escape." },
	{ "spacemonster", "Very little is known about the species Monstrum Mysterium, "
		"but they have proven to be quite dangerous." },
	/* TODO: flesh this out more */
};

static int nl_find_next_word(int argc, int pos[], int part_of_speech, int start_with)
{
	int i;

	for (i = start_with; i < argc; i++) {
		if (pos[i] == part_of_speech)
			return i;
	}
	return -1;
}

static void nl_describe_noun(struct game_client *c, char *word)
{
	int i;
	printf("%s: describing '%s'\n", logprefix(), word);

	for (i = 0; i < ARRAYSIZE(noun_description); i++) {
		if (strcasecmp(word, noun_description[i].noun) == 0) {
			queue_add_text_to_speech(c, noun_description[i].description);
			return;
		}
	}
	queue_add_text_to_speech(c, "I do not know anything about that.");
}

static char *nl_get_object_name(struct snis_entity *o)
{
	/* Assumes universe lock is held */
	switch (o->type) {
	case OBJTYPE_STARBASE:
		return o->sdata.name;
	case OBJTYPE_NEBULA:
		return "nebula";
	default:
		return o->sdata.name;
	}
}

static void nl_describe_game_object(struct game_client *c, uint32_t id)
{
	int i;
	char description[254];
	char extradescription[40];
	static struct mtwist_state *mt = NULL;
	int planet;
	char *planet_type_str;
	int ss_planet_type;

	pthread_mutex_lock(&universe_mutex);
	i = lookup_by_id(id);
	if (i < 0) {
		pthread_mutex_unlock(&universe_mutex);
		queue_add_text_to_speech(c, "I do not know anything about that.");
		return;
	}
	switch (go[i].type) {
	case OBJTYPE_PLANET:
		if (go[i].tsd.planet.custom_description) {
			strncpy(description, go[i].tsd.planet.custom_description, 253);
			description[253] = '\0';
		} else {
			mt = mtwist_init(go[i].tsd.planet.description_seed);
			ss_planet_type = go[i].tsd.planet.solarsystem_planet_type;
			planet_type_str = solarsystem_assets->planet_type[ss_planet_type];
			planet_description(mt, description, 250, 254, planet_type_from_string(planet_type_str));
			mtwist_free(mt);
		}
		pthread_mutex_unlock(&universe_mutex);
		queue_add_text_to_speech(c, description);
		return;
	case OBJTYPE_ASTEROID:
		pthread_mutex_unlock(&universe_mutex);
		snprintf(description, sizeof(description), "%s is a small and rather ordinary asteroid",
				go[i].sdata.name);
		queue_add_text_to_speech(c, description);
		return;
	case OBJTYPE_STARBASE:
		if (go[i].tsd.starbase.associated_planet_id < 0) {
			snprintf(description, sizeof(description), "%s is a star base in deep space",
					go[i].sdata.name);
		} else {
			planet = lookup_by_id(go[i].tsd.starbase.associated_planet_id);
			if (planet < 0)
				snprintf(description, sizeof(description), "%s is a star base in deep space",
						go[i].sdata.name);
			else
				snprintf(description, sizeof(description),
					"%s is a star base in orbit around the planet %s",
					go[i].sdata.name, go[planet].sdata.name);
		}
		pthread_mutex_unlock(&universe_mutex);
		queue_add_text_to_speech(c, description);
		return;
	case OBJTYPE_SHIP2:
		planet = lookup_by_id(go[i].tsd.ship.home_planet);
		pthread_mutex_unlock(&universe_mutex);
		if (planet >= 0)
			snprintf(extradescription, sizeof(extradescription), " originating from the planet %s",
				go[planet].sdata.name);
		else
			strcpy(extradescription, " of unknown origin");
		snprintf(description, sizeof(description), "%s is a %s class ship %s", go[i].sdata.name,
				ship_type[go[i].tsd.ship.shiptype].class, extradescription);
		queue_add_text_to_speech(c, description);
		return;
	case OBJTYPE_SHIP1:
		pthread_mutex_unlock(&universe_mutex);
		snprintf(description, sizeof(description),
				"%s is a human piloted wombat class space ship", go[i].sdata.name);
		queue_add_text_to_speech(c, description);
		return;
	default:
		pthread_mutex_unlock(&universe_mutex);
		queue_add_text_to_speech(c, "I do not know anything about that.");
		return;
	}
	pthread_mutex_unlock(&universe_mutex);
}

static void nl_describe_n(void *context, int argc, char *argv[], int pos[],
		union snis_nl_extra_data extra_data[])
{
	struct game_client *c = context;
	int i;

	for (i = 0; i < argc; i++) {
		switch (pos[i]) {
		case POS_NOUN:
			nl_describe_noun(c, argv[i]);
			return;
		case POS_EXTERNAL_NOUN:
			nl_describe_game_object(c, extra_data[i].external_noun.handle);
			return;
		default:
			break;
		}
	}
	queue_add_text_to_speech(c, "I do not know anything about that.");
}

/* Assumes universe lock is held */
static int nl_find_nearest_object_of_type(uint32_t id, int objtype)
{
	int i;
	double x, y, z;
	double nearest_yet = -1.0;
	double d;
	struct snis_entity *o;
	int nearest = -1;

	i = lookup_by_id(id);
	if (i < 0)
		return -1;

	o = &go[i];

	for (i = 0; i <= snis_object_pool_highest_object(pool); i++) {
		if (go[i].type != objtype)
			continue;
		if (!go[i].alive)
			continue;
		x = go[i].x - o->x;
		y = go[i].y - o->y;
		z = go[i].z - o->z;
		d = x * x + y * y + z * z;
		if (d < nearest_yet || nearest_yet < 0) {
			nearest = i;
			nearest_yet = d;
		}
	}
	return nearest;
}

/* Assumes universe lock is held -- find "nearest" or "selected" or "other adjective" object */
static int nl_find_nearest_object(struct game_client *c, int argc, char *argv[], int pos[],
					union snis_nl_extra_data extra_data[], int starting_word)
{
	int i, object, adj, objtype;

	object = nl_find_next_word(argc, pos, POS_NOUN, starting_word);
	if (object < 0)
		goto no_understand;
	adj = nl_find_next_word(argc, pos, POS_ADJECTIVE, starting_word);
	if (adj > 0) {
		if (strcmp(argv[adj], "nearest") != 0 &&
			strcmp(argv[adj], "selected") != 0)
			goto no_understand;
	} /* else assume they meant nearest */
	if (strcmp(argv[object], "planet") == 0)
		objtype = OBJTYPE_PLANET;
	else if (strcmp(argv[object], "starbase") == 0)
		objtype = OBJTYPE_STARBASE;
	else if (strcmp(argv[object], "nebula") == 0)
		objtype = OBJTYPE_NEBULA;
	else if (strcmp(argv[object], "ship") == 0)
		objtype = OBJTYPE_SHIP2;
	else if (strcmp(argv[object], "asteroid") == 0)
		objtype = OBJTYPE_ASTEROID;
	else if (strcmp(argv[object], "gate") == 0)
		objtype = OBJTYPE_WARPGATE;
	else if (strcmp(argv[object], "blackhole") == 0)
		objtype = OBJTYPE_BLACK_HOLE;
	else if (strcmp(argv[object], "selection") == 0 ||
		strcmp(argv[object], "target") == 0) {
		if (bridgelist[c->bridge].science_selection == (uint32_t) -1)
			return -1;
		i = lookup_by_id(bridgelist[c->bridge].science_selection);
		if (i >= 0)
			return i;
		return -1;
	} else
		objtype = -1;
	if (objtype < 0)
		goto no_understand;
	if (adj > 0 && strcmp(argv[adj], "selected") == 0) {
		if (bridgelist[c->bridge].science_selection == (uint32_t) -1)
			return -1;
		i = lookup_by_id(bridgelist[c->bridge].science_selection);
	} else {
		i = nl_find_nearest_object_of_type(c->shipid, objtype);
	}
	if (i < 0)
		return -1;
	return i;
no_understand:
	return -1;
}

static void nl_describe_an(void *context, int argc, char *argv[], int pos[],
		union snis_nl_extra_data extra_data[])
{
	struct game_client *c = context;
	int i, adj, noun;
	uint32_t id;

	noun = nl_find_next_word(argc, pos, POS_EXTERNAL_NOUN, 0);
	if (noun >= 0) {
		nl_describe_game_object(c, extra_data[noun].external_noun.handle);
		return;
	}
	adj = nl_find_next_word(argc, pos, POS_ADJECTIVE, 0);
	if (adj < 0)
		goto no_understand;
	if (strcmp(argv[adj], "nearest") == 0) {
		pthread_mutex_lock(&universe_mutex);
		i = nl_find_nearest_object(c, argc, argv, pos, extra_data, adj);
		if (i < 0) {
			pthread_mutex_unlock(&universe_mutex);
			goto no_understand;
		}
		id = go[i].id;
		pthread_mutex_unlock(&universe_mutex);
		nl_describe_game_object(c, id);
		return;
	}
no_understand:
	queue_add_text_to_speech(c, "I do not know anything about that.");
}

static void calculate_course_and_distance(struct game_client *c, int argc, char *argv[], int pos[],
				union snis_nl_extra_data extra_data[], int calculate_distance,
				int calculate_course, int first_noun)
{
	int second_noun;
	struct snis_entity *us, *dest;
	union vec3 direction;
	char directions[200];
	double heading, mark;
	char destination_name[100];
	char *modifier = "";
	double distance;
	int i;

	/* Find the second noun, it should be a place... */
	second_noun = nl_find_next_word(argc, pos, POS_EXTERNAL_NOUN, first_noun + 1);
	pthread_mutex_lock(&universe_mutex);
	if (second_noun < 0) {
		i = nl_find_nearest_object(c, argc, argv, pos, extra_data, first_noun + 1);
		if (i < 0) {
			pthread_mutex_unlock(&universe_mutex);
			goto no_understand;
		}
		snprintf(destination_name, sizeof(destination_name), "%s", nl_get_object_name(&go[i]));
	} else {
		i = lookup_by_id(extra_data[second_noun].external_noun.handle);
		if (i < 0) {
			pthread_mutex_unlock(&universe_mutex);
			if (calculate_course)
				queue_add_text_to_speech(c,
					"Sorry, I cannot compute a course to an unknown location.");
			else if (calculate_distance)
				queue_add_text_to_speech(c,
					"Sorry, I cannot compute the distance to an unknown location.");
			return;
		}
		snprintf(destination_name, sizeof(destination_name), "%s", argv[second_noun]);
	}
	dest = &go[i];
	if (dest->type == OBJTYPE_PLANET)
		modifier = "the planet ";
	else if (dest->type == OBJTYPE_ASTEROID)
		modifier = "the asteroid ";

	i = lookup_by_id(c->shipid);
	if (i < 0) {
		pthread_mutex_unlock(&universe_mutex);
		queue_add_text_to_speech(c, "Sorry, I do not seem to know our current location.");
		return;
	}
	us = &go[i];

	/* Compute course... */
	direction.v.x = dest->x - us->x;
	direction.v.y = dest->y - us->y;
	direction.v.z = dest->z - us->z;
	pthread_mutex_unlock(&universe_mutex);

	distance = vec3_magnitude(&direction);
	vec3_to_heading_mark(&direction, NULL, &heading, &mark);
	heading = heading * 180.0 / M_PI;
	heading = 360 - heading + 90; /* why?  why do I have to do this? */
	mark = mark * 180.0 / M_PI;
	if (calculate_course)
		snprintf(directions, sizeof(directions),
			"Course to %s%s calculated.  Destination lies at bearing %3.0lf, mark %3.0lf at a distance of %.0f clicks",
			modifier, destination_name, heading, mark, distance);
	if (calculate_distance)
		snprintf(directions, sizeof(directions), "The distance to %s%s is %.0lf clicks",
				modifier, destination_name, distance);
	queue_add_text_to_speech(c, directions);
	return;

no_understand:
	queue_add_text_to_speech(c, "Sorry, I do not know how to compute that.");
}

static void nl_compute_npn(void *context, int argc, char *argv[], int pos[],
				union snis_nl_extra_data extra_data[])
{
	int first_noun;
	struct game_client *c = context;
	int calculate_course = 0;
	int calculate_distance = 0;

	/* Find the first noun... it should be "course", or "distance". */

	first_noun = nl_find_next_word(argc, pos, POS_NOUN, 0);
	if (first_noun < 0) /* didn't find first noun... */
		goto no_understand;

	if (strcasecmp(argv[first_noun], "course") == 0)
		calculate_course = 1;
	else if (strcasecmp(argv[first_noun], "distance") == 0)
		calculate_distance = 1;

	if (!calculate_course && !calculate_distance)
		goto no_understand;
	if (calculate_course && calculate_distance)
		goto no_understand;

	/* TODO:  check the preposition here. "away", "from", "around", change the meaning.
	 * for now, assume "to", "toward", etc.
	 */

	calculate_course_and_distance(c, argc, argv, pos, extra_data,
					calculate_distance, calculate_course, first_noun + 1);
	return;

no_understand:
	queue_add_text_to_speech(c, "Sorry, I do not know how to compute that.");
}

static void nl_what_is_npn(void *context, int argc, char *argv[], int pos[],
				union snis_nl_extra_data extra_data[])
{
	struct game_client *c = context;
	int noun;

	noun = nl_find_next_word(argc, pos, POS_NOUN, 0);
	if (noun < 0) { /* didn't find first noun... */
		noun = nl_find_next_word(argc, pos, POS_EXTERNAL_NOUN, 0);
		if (noun < 0) /* didn't find first noun... */
			goto no_understand;
	}
	if (strcasecmp(argv[noun], "distance") == 0) { /* what is the distance <preposition> <noun> */
		nl_compute_npn(c, argc, argv, pos, extra_data);
		return;
	}

no_understand:
	queue_add_text_to_speech(c, "Sorry, I do not know what that is.");
}

static void nl_what_is_anpan(void *context, int argc, char *argv[], int pos[],
				union snis_nl_extra_data extra_data[])
{
	struct game_client *c = context;
	int adj1, adj2, adj3, prep, noun1, noun2;

	adj1 = nl_find_next_word(argc, pos, POS_ADJECTIVE, 0);
	if (adj1 < 0)
		goto skip_joke;
	noun1 = nl_find_next_word(argc, pos, POS_NOUN, adj1 + 1);
	if (noun1 < 0)
		goto skip_joke;
	prep = nl_find_next_word(argc, pos, POS_PREPOSITION, noun1 + 1);
	if (prep < 0)
		goto skip_joke;
	adj2 = nl_find_next_word(argc, pos, POS_ADJECTIVE, prep + 1);
	if (adj2 < 0)
		goto skip_joke;
	adj3 = nl_find_next_word(argc, pos, POS_ADJECTIVE, adj2 + 1);
	noun2 = nl_find_next_word(argc, pos, POS_NOUN, adj2 + 1);
	if (noun2 < 0)
		goto skip_joke;

	if (strcasecmp(argv[adj1], "airspeed") == 0 &&
	    strcasecmp(argv[noun1], "speed" /* velocity */) == 0 &&
	    strcasecmp(argv[prep], "of" /* velocity */) == 0 &&
	    strcasecmp(argv[adj2], "unladen") == 0 &&
	    strcasecmp(argv[noun2], "swallow") == 0) {
		if (adj3 >= 0) {
			if (strcasecmp(argv[adj3], "african") == 0) {
				queue_add_text_to_speech(c, "uh? I don't know that.");
			} else if (strcasecmp(argv[adj3], "european") == 0) {
				queue_add_text_to_speech(c, "About 11 meters per second.");
				queue_add_text_to_speech(c,
					"You have to know these things when your a computer on a starship you know.");
					/* "your" is pronounced better than "you're" by pico2wave. */
			} else {
				queue_add_text_to_speech(c, "What do you mean? African or European?");
			}
		} else {
			queue_add_text_to_speech(c, "What do you mean? African or European?");
		}
	}
	return;

skip_joke:
	queue_add_text_to_speech(c, "Sorry, I didn't understand that.");
}

/* "how far to starbase 0?" */
static void nl_how_apn(void *context, int argc, char *argv[], int pos[],
				union snis_nl_extra_data extra_data[])
{
	struct game_client *c = context;
	int adjective;

	adjective = nl_find_next_word(argc, pos, POS_ADJECTIVE, 0);
	if (adjective < 0)
		goto no_understand;
	if (strcasecmp(argv[adjective], "far") != 0)
		goto no_understand;
	calculate_course_and_distance(c, argc, argv, pos, extra_data, 1, 0, adjective + 1);
	return;

no_understand:
	queue_add_text_to_speech(c, "Sorry, I do not know.");
}

static void nl_fuel_report(struct game_client *c)
{
	int i;
	struct snis_entity *ship;
	float fuel_level;
	char fuel_report[100];

	pthread_mutex_lock(&universe_mutex);
	i = lookup_by_id(c->shipid);
	if (i < 0) {
		pthread_mutex_unlock(&universe_mutex);
		return;
	}
	ship = &go[i];
	fuel_level = 100.0 * (float) ship->tsd.ship.fuel / (float) UINT32_MAX;
	pthread_mutex_unlock(&universe_mutex);

	snprintf(fuel_report, sizeof(fuel_report), "Fuel tanks are at %2.0f percent.", fuel_level);
	queue_add_text_to_speech(c, fuel_report);
}

/* "How much fuel..." */
static void nl_how_an(void *context, int argc, char *argv[], int pos[],
				union snis_nl_extra_data extra_data[])
{
	struct game_client *c = context;
	int adjective, noun;

	adjective = nl_find_next_word(argc, pos, POS_ADJECTIVE, 0);
	if (adjective < 0)
		goto no_understand;
	if (strcasecmp(argv[adjective], "far") == 0) {
		nl_how_apn(context, argc, argv, pos, extra_data);
		return;
	}
	if (strcasecmp(argv[adjective], "much") != 0)
		goto no_understand;
	noun = nl_find_next_word(argc, pos, POS_NOUN, adjective + 1);
	if (noun < 0)
		goto no_understand;
	if (strcasecmp(argv[noun], "fuel") != 0)
		goto no_understand;

	nl_fuel_report(c);

	return;

no_understand:
	queue_add_text_to_speech(c, "Sorry, I do not know.");
}

/* How much fuel do we have */
static void nl_how_anxPx(void *context, int argc, char *argv[], int pos[],
				union snis_nl_extra_data extra_data[])
{
	nl_how_an(context, argc, argv, pos, extra_data);
}

/* do we have enough fuel */
static void nl_do_Pxan(void *context, int argc, char *argv[], int pos[],
				union snis_nl_extra_data extra_data[])
{
	struct game_client *c = context;
	int pronoun, auxverb, adjective, noun;

	pronoun = nl_find_next_word(argc, pos, POS_PRONOUN, 0);
	if (pronoun < 0)
		goto no_understand;
	if (strcasecmp(argv[pronoun], "we") != 0)
		goto no_understand;
	auxverb = nl_find_next_word(argc, pos, POS_AUXVERB, pronoun + 1);
	if (auxverb < 0)
		goto no_understand;
	if (strcasecmp(argv[auxverb], "have") != 0)
		goto no_understand;
	adjective = nl_find_next_word(argc, pos, POS_ADJECTIVE, pronoun + 1);
	if (adjective < 0)
		goto no_understand;
	if (strcasecmp(argv[adjective], "enough") != 0)
		goto no_understand;
	noun = nl_find_next_word(argc, pos, POS_NOUN, pronoun + 1);
	if (noun < 0)
		goto no_understand;
	if (strcasecmp(argv[noun], "fuel") == 0) {
		nl_fuel_report(c);
		return;
	}

no_understand:
	queue_add_text_to_speech(c, "Sorry, I do not know.");
}

static void nl_african_or_european(void *context, int argc, char *argv[], int pos[],
				union snis_nl_extra_data extra_data[])
{
	struct game_client *c = context;
	int verb;

	verb = nl_find_next_word(argc, pos, POS_VERB, 0);
	if (strcasecmp(argv[verb], "african") == 0) {
		queue_add_text_to_speech(c, "uh? I don't know that.");
		return;
	}
	if (strcasecmp(argv[verb], "european") == 0) {
		queue_add_text_to_speech(c, "About 11 meters per second.");
		queue_add_text_to_speech(c,
			"You have to know these things when your a computer on a starship you know.");
			/* "your" is pronounced better than "you're" by pico2wave. */
		return;
	}
	queue_add_text_to_speech(c, "Um, what?");
}

static void nl_what_is_n(void *context, int argc, char *argv[], int pos[],
				union snis_nl_extra_data extra_data[])
{
	nl_describe_n(context, argc, argv, pos, extra_data);
}

static void nl_rotate_ship(struct game_client *c, union quat *rotation)
{
	int i;
	struct snis_entity *o;
	union quat local_rotation, new_orientation;

	pthread_mutex_lock(&universe_mutex);
	i = lookup_by_id(c->shipid);
	if (i < 0) {
		pthread_mutex_unlock(&universe_mutex);
		queue_add_text_to_speech(c, "Sorry, I can't seem to steer, Computer needs maintenance.");
		return;
	}
	o = &go[i];

	/* Convert rotation to local coordinate system */
	quat_conjugate(&local_rotation, rotation, &o->orientation);
	/* Apply to local orientation */
	quat_mul(&new_orientation, &local_rotation, &o->orientation);
	quat_normalize_self(&new_orientation);

	/* Now let the computer steer for awhile */
	o->tsd.ship.computer_desired_orientation = new_orientation;
	o->tsd.ship.computer_steering_time_left = COMPUTER_STEERING_TIME;
	o->tsd.ship.orbiting_object_id = 0xffffffff;
	// o->orientation = new_orientation;
	pthread_mutex_unlock(&universe_mutex);

}

static int nl_calculate_ship_rotation(struct game_client *c,
					int argc, char *argv[], int pos[],
					union snis_nl_extra_data extra_data[],
					int direction, int amount, char *reply,
					union quat *rotation)
{
	float degrees = extra_data[amount].number.value;

	strcpy(reply, "");

	/* If the ship facing down the positive x axis, what rotation would we apply? */
	if (strcasecmp(argv[direction], "starboard") == 0 ||
		strcasecmp(argv[direction], "right") == 0) {
		quat_init_axis(rotation, 0, 1, 0, -degrees * M_PI / 180.0);
		sprintf(reply, "rotating %3.0f degrees to starboard", degrees);
	} else if (strcasecmp(argv[direction], "port") == 0 ||
			strcasecmp(argv[direction], "left") == 0) {
		quat_init_axis(rotation, 0, 1, 0, degrees * M_PI / 180.0);
		sprintf(reply, "rotating %3.0f degrees to port", degrees);
	} else if (strcasecmp(argv[direction], "clockwise") == 0) {
		quat_init_axis(rotation, 1, 0, 0, degrees * M_PI / 180.0);
		sprintf(reply, "rolling %3.0f degrees clockwise", degrees);
	} else if (strcasecmp(argv[direction], "counterclockwise") == 0) {
		quat_init_axis(rotation, 1, 0, 0, -degrees * M_PI / 180.0);
		sprintf(reply, "rolling %3.0f degrees counter clockwise", degrees);
	} else if (strcasecmp(argv[direction], "up") == 0) {
		quat_init_axis(rotation, 0, 0, 1, degrees * M_PI / 180.0);
		sprintf(reply, "pitching %3.0f degrees up", degrees);
	} else if (strcasecmp(argv[direction], "down") == 0) {
		quat_init_axis(rotation, 0, 0, 1, -degrees * M_PI / 180.0);
		sprintf(reply, "pitching %3.0f degrees down", degrees);
	} else {
		queue_add_text_to_speech(c, "Sorry, I do not understand which direction you want to turn.");
		return -1;
	}
	return 0;
}

static void nl_turn_aq(void *context, int argc, char *argv[], int pos[],
				union snis_nl_extra_data extra_data[])
{
	struct game_client *c = context;
	int adj, number;
	union quat rotation;
	char reply[100];

	adj = nl_find_next_word(argc, pos, POS_ADJECTIVE, 0);
	if (adj < 0)
		goto no_understand;
	number = nl_find_next_word(argc, pos, POS_NUMBER, adj);
	if (number < 0)
		goto no_understand;

	if (nl_calculate_ship_rotation(c, argc, argv, pos, extra_data,
					adj, number, reply, &rotation))
		return;

	nl_rotate_ship(c, &rotation);
	queue_add_text_to_speech(c, reply);
	return;

no_understand:
	queue_add_text_to_speech(c, "Sorry, I do not understand which direction you want to turn.");
}

static void nl_turn_qa(void *context, int argc, char *argv[], int pos[],
				union snis_nl_extra_data extra_data[])
{
	struct game_client *c = context;
	int adj, number;
	union quat rotation;
	char reply[100];

	number = nl_find_next_word(argc, pos, POS_NUMBER, 0);
	if (number < 0)
		goto no_understand;
	adj = nl_find_next_word(argc, pos, POS_ADJECTIVE, number);
	if (adj < 0)
		goto no_understand;

	if (nl_calculate_ship_rotation(c, argc, argv, pos, extra_data,
					adj, number, reply, &rotation))
		return;

	nl_rotate_ship(c, &rotation);
	queue_add_text_to_speech(c, reply);
	return;

no_understand:
	queue_add_text_to_speech(c, "Sorry, I do not understand which direction you want to turn.");
}

/* lights on/off/out */
static void nl_lights_p(void *context, int argc, char *argv[], int pos[],
			union snis_nl_extra_data extra_data[])
{
	struct game_client *c = context;
	int i, prep;
	uint8_t current_lights;
	prep = nl_find_next_word(argc, pos, POS_PREPOSITION, 0);
	if (prep < 0)
		goto no_understand;

	pthread_mutex_lock(&universe_mutex);
	i = lookup_by_id(c->shipid);
	if (i < 0) {
		pthread_mutex_unlock(&universe_mutex);
		return;
	}
	current_lights = go[i].tsd.ship.exterior_lights;
	pthread_mutex_unlock(&universe_mutex);
	if (strcasecmp(argv[prep], "on") == 0 || strcasecmp(argv[prep], "up") == 0) {
		if (current_lights) {
			queue_add_text_to_speech(c, "The exterior lights are already on.");
		} else {
			queue_add_text_to_speech(c, "Turning exterior lights on.");
			process_adjust_control_bytevalue(c, c->shipid,
				offsetof(struct snis_entity, tsd.ship.exterior_lights), 1, no_limit);
		}
	} else if (strcasecmp(argv[prep], "off") == 0 || strcasecmp(argv[prep], "out") == 0 ||
			strcasecmp(argv[prep], "down") == 0) {
		if (!current_lights) {
			queue_add_text_to_speech(c, "The exterior lights are already off.");
		} else {
			queue_add_text_to_speech(c, "Turning exterior lights off.");
			process_adjust_control_bytevalue(c, c->shipid,
				offsetof(struct snis_entity, tsd.ship.exterior_lights), 0, no_limit);
		}
	} else {
		goto no_understand;
	}
	return;

no_understand:
	queue_add_text_to_speech(c, "Sorry, I do not understand which direction you want to turn.");
}

static void nl_repeat_n(void *context, int argc, char *argv[], int pos[],
			union snis_nl_extra_data extra_data[])
{
	char buf[256];
	struct game_client *c = context;

	if (strcmp(bridgelist[c->bridge].last_text_to_speech, "") == 0)
		snprintf(buf, sizeof(buf), "I didn't say anything.");
	else
		snprintf(buf, sizeof(buf), "I said, %s", bridgelist[c->bridge].last_text_to_speech);
	queue_add_text_to_speech(c, buf);
}

static void nl_set_volume(struct game_client *c, char *word, float value)
{
	char text[100];

	if (value > 1.0)
		value = 1.0;
	if (value < 0.0)
		value = 0.0;
	if (c->bridge < 0 && c->bridge >= nbridges) {
		queue_add_text_to_speech(c, "I can't seem to do that right now.");
		return;
	}
	if (fabsf(value - bridgelist[c->bridge].text_to_speech_volume) <= 0.01) {
		snprintf(text, sizeof(text), "Volume already set to %d percent.",
			(int) (100.0 * value));
		queue_add_text_to_speech(c, text);
		return;
	}
	bridgelist[c->bridge].text_to_speech_volume_timestamp = universe_timestamp;
	bridgelist[c->bridge].text_to_speech_volume = value;
	snprintf(text, sizeof(text), "Setting volume to %d percent.",
			(int) (100.0 * value));
	queue_add_text_to_speech(c, text);
	return;
}

/* Eg: "turn/shut on/off/out lights", "turn up/down volume" */
static void nl_turn_pn_or_np(void *context, int argc, char *argv[], int pos[],
			union snis_nl_extra_data extra_data[], int np)
{
	struct game_client *c = context;
	int i, prep, noun, value;
	uint8_t current_lights, current_docking;
	uint32_t current_tractor;
	char buffer[100];

	if (!np) { /* pn */
		prep = nl_find_next_word(argc, pos, POS_PREPOSITION, 0);
		if (prep < 0)
			goto no_understand;
		noun = nl_find_next_word(argc, pos, POS_NOUN, prep + 1);
		if (noun < 0)
			goto no_understand;
	} else { /* np */
		noun = nl_find_next_word(argc, pos, POS_NOUN, 0);
		if (noun < 0)
			goto no_understand;
		prep = nl_find_next_word(argc, pos, POS_PREPOSITION, noun + 1);
		if (prep < 0)
			goto no_understand;
	}
	if (strcasecmp(argv[prep], "on") == 0 || strcasecmp(argv[prep], "up") == 0)
		value = 1;
	else if (strcasecmp(argv[prep], "off") == 0 || strcasecmp(argv[prep], "out") == 0 ||
			strcasecmp(argv[prep], "down") == 0)
		value = 0;
	else
		goto no_understand;
	pthread_mutex_lock(&universe_mutex);
	i = lookup_by_id(c->shipid);
	if (i < 0) {
		pthread_mutex_unlock(&universe_mutex);
		return;
	}
	current_lights = go[i].tsd.ship.exterior_lights;
	current_docking = go[i].tsd.ship.docking_magnets = 0;
	current_tractor = go[i].tsd.ship.tractor_beam;
	pthread_mutex_unlock(&universe_mutex);

	if (strcasecmp(argv[noun], "lights") == 0) {
		if (current_lights == value * 255) {
			snprintf(buffer, sizeof(buffer), "The exterior lights are already %s.",
				value ? "on" : "off");
			queue_add_text_to_speech(c, buffer);
			return;
		}
		snprintf(buffer, sizeof(buffer), "Turning exterior lights %s.", value ? "on" : "off");
		queue_add_text_to_speech(c, buffer);
		process_adjust_control_bytevalue(c, c->shipid,
			offsetof(struct snis_entity, tsd.ship.exterior_lights), value * 255, no_limit);
		return;
	} else if (strcasecmp(argv[noun], "docking system") == 0) {
		if (value == current_docking) {
			snprintf(buffer, sizeof(buffer), "The docking system is already %s.",
				value ? "engaged" : "disengaged");
			queue_add_text_to_speech(c, buffer);
			return;
		}
		snprintf(buffer, sizeof(buffer), "%s docking system.",
			current_docking ? "Disengaging" : "Engaging");
		queue_add_text_to_speech(c, buffer);
		process_adjust_control_bytevalue(c, c->shipid,
			offsetof(struct snis_entity, tsd.ship.docking_magnets), value, no_limit);
	} else if (strcasecmp(argv[noun], "tractor beam") == 0) {
		if (value == 1) {
			if (current_tractor != 0xffffffff) {
				queue_add_text_to_speech(c, "The tractor beam is already engaged.");
				return;
			}
			turn_on_tractor_beam(c, &go[i], 0xffffffff, 0); /* TODO: fix raciness. */
			return;
		} else {
			if (current_tractor == 0xffffffff) {
				queue_add_text_to_speech(c, "The tractor beam is already disengaged.");
				return;
			}
			pthread_mutex_lock(&universe_mutex);
			i = lookup_by_id(c->shipid);
			if (i < 0) {
				pthread_mutex_unlock(&universe_mutex);
				return;
			}
			turn_off_tractorbeam(&go[i]);
			pthread_mutex_unlock(&universe_mutex);
			queue_add_text_to_speech(c, "Tractor beam disengaged.");
		}
	} else if (strcasecmp(argv[noun], "red alert") == 0) {
		set_red_alert_mode(c, value);
	} else if (strcasecmp(argv[noun], "volume") == 0) {
		if (value == 0)
			nl_set_volume(c, "volume", bridgelist[c->bridge].text_to_speech_volume / 2.0);
		else
			nl_set_volume(c, "volume", bridgelist[c->bridge].text_to_speech_volume * 2.0);
	}
	return;

no_understand:
	queue_add_text_to_speech(c, "Sorry, I do not understand which direction you want to turn.");
}

/* Eg: "turn/shut on/off/out lights" */
static void nl_turn_pn(void *context, int argc, char *argv[], int pos[],
			union snis_nl_extra_data extra_data[])
{
	nl_turn_pn_or_np(context, argc, argv, pos, extra_data, 0);
}

/* Eg: "turn/shut lights on/off/out" */
static void nl_turn_np(void *context, int argc, char *argv[], int pos[],
			union snis_nl_extra_data extra_data[])
{
	nl_turn_pn_or_np(context, argc, argv, pos, extra_data, 1);
}

/* Eg: "turn right 90 degrees" */
static void nl_turn_aqa(void *context, int argc, char *argv[], int pos[],
				union snis_nl_extra_data extra_data[])
{
	struct game_client *c = context;
	int direction, amount, unit;
	union quat rotation;
	char reply[100];

	direction = nl_find_next_word(argc, pos, POS_ADJECTIVE, 0);
	if (direction < 0)
		goto no_understand;
	amount = nl_find_next_word(argc, pos, POS_NUMBER, direction + 1);
	if (amount < 0)
		goto no_understand;
	unit = nl_find_next_word(argc, pos, POS_ADJECTIVE, amount + 1);
	if (unit < 0)
		goto no_understand;

	if (strcasecmp(argv[unit], "degrees") != 0) {
		queue_add_text_to_speech(c, "In degrees please?");
		return;
	}

	if (nl_calculate_ship_rotation(c, argc, argv, pos, extra_data,
					direction, amount, reply, &rotation))
		return;
	nl_rotate_ship(c, &rotation);
	queue_add_text_to_speech(c, reply);
	return;

no_understand:
	queue_add_text_to_speech(c, "Sorry, I do not understand which direction you want to turn.");
}

/* Eg: "turn 90 degrees right " */
static void nl_turn_qaa(void *context, int argc, char *argv[], int pos[],
				union snis_nl_extra_data extra_data[])
{
	struct game_client *c = context;
	int direction, amount, unit;
	union quat rotation;
	char reply[100];

	amount = nl_find_next_word(argc, pos, POS_NUMBER, 0);
	if (amount < 0)
		goto no_understand;
	unit = nl_find_next_word(argc, pos, POS_ADJECTIVE, amount + 1);
	if (unit < 0)
		goto no_understand;
	direction = nl_find_next_word(argc, pos, POS_ADJECTIVE, unit + 1);
	if (direction < 0)
		goto no_understand;

	if (strcasecmp(argv[unit], "degrees") != 0) {
		queue_add_text_to_speech(c, "In degrees please?");
		return;
	}

	if (nl_calculate_ship_rotation(c, argc, argv, pos, extra_data,
					direction, amount, reply, &rotation))
		return;
	nl_rotate_ship(c, &rotation);
	queue_add_text_to_speech(c, reply);
	return;

no_understand:
	queue_add_text_to_speech(c, "Sorry, I do not understand which direction you want to turn.");
}

static void nl_set_controllable_byte_value(struct game_client *c, char *word, float fraction, int offset,
						 bytevalue_limit_function limit)
{
	char answer[100];
	uint8_t *bytevalue;
	uint8_t new_value;

	if (fraction < 0)
		fraction = 0.0;
	if (fraction > 1.0)
		fraction = 1.0;
	new_value = (uint8_t) (255.0 * fraction);

	int i;
	pthread_mutex_lock(&universe_mutex);
	i = lookup_by_id(c->shipid);
	if (i < 0) {
		pthread_mutex_unlock(&universe_mutex);
		snprintf(answer, sizeof(answer), "Sorry, I can't seem to find the %s, it must be a memory error", word);
		queue_add_text_to_speech(c, answer);
		return;
	}
	bytevalue = (uint8_t *) &go[i];
	bytevalue += offset;
	new_value = limit(c, new_value);
	*bytevalue = new_value;
	pthread_mutex_unlock(&universe_mutex);
	snprintf(answer, sizeof(answer), "setting the %s to %3.0f percent", word, fraction * 100.0);
	queue_add_text_to_speech(c, answer);
}

static void nl_set_shields(struct game_client *c, char *word, float fraction)
{
	int offset = offsetof(struct snis_entity, tsd.ship.power_data.shields.r1);
	nl_set_controllable_byte_value(c, word, fraction, offset, no_limit);
}

static void nl_set_impulse_drive(struct game_client *c, char *word, float fraction)
{
	int offset = offsetof(struct snis_entity, tsd.ship.power_data.impulse.r1);
	nl_set_controllable_byte_value(c, word, fraction, offset, no_limit);
}

static void nl_set_maneuvering_power(struct game_client *c, char *word, float fraction)
{
	int offset = offsetof(struct snis_entity, tsd.ship.power_data.maneuvering.r2);
	nl_set_controllable_byte_value(c, word, fraction, offset, no_limit);
}

static void nl_set_impulse_power(struct game_client *c, char *word, float fraction)
{
	int offset = offsetof(struct snis_entity, tsd.ship.power_data.impulse.r2);
	nl_set_controllable_byte_value(c, word, fraction, offset, no_limit);
}

static void nl_set_warp_power(struct game_client *c, char *word, float fraction)
{
	int offset = offsetof(struct snis_entity, tsd.ship.power_data.warp.r2);
	nl_set_controllable_byte_value(c, word, fraction, offset, no_limit);
}

static void nl_set_sensor_power(struct game_client *c, char *word, float fraction)
{
	int offset = offsetof(struct snis_entity, tsd.ship.power_data.sensors.r2);
	nl_set_controllable_byte_value(c, word, fraction, offset, no_limit);
}

static void nl_set_comms_power(struct game_client *c, char *word, float fraction)
{
	int offset = offsetof(struct snis_entity, tsd.ship.power_data.comms.r2);
	nl_set_controllable_byte_value(c, word, fraction, offset, no_limit);
}

static void nl_set_phaser_power(struct game_client *c, char *word, float fraction)
{
	int offset = offsetof(struct snis_entity, tsd.ship.power_data.phasers.r2);
	nl_set_controllable_byte_value(c, word, fraction, offset, no_limit);
}

static void nl_set_shield_power(struct game_client *c, char *word, float fraction)
{
	int offset = offsetof(struct snis_entity, tsd.ship.power_data.shields.r2);
	nl_set_controllable_byte_value(c, word, fraction, offset, no_limit);
}

static void nl_set_tractor_power(struct game_client *c, char *word, float fraction)
{
	int offset = offsetof(struct snis_entity, tsd.ship.power_data.tractor.r2);
	nl_set_controllable_byte_value(c, word, fraction, offset, no_limit);
}

static void nl_set_lifesupport_power(struct game_client *c, char *word, float fraction)
{
	int offset = offsetof(struct snis_entity, tsd.ship.power_data.lifesupport.r2);
	nl_set_controllable_byte_value(c, word, fraction, offset, no_limit);
}

static void nl_set_warpdrive(struct game_client *c, char *word, float fraction)
{
	int offset = offsetof(struct snis_entity, tsd.ship.power_data.warp.r1);
	nl_set_controllable_byte_value(c, word, fraction, offset, no_limit);
}

static void nl_set_maneuvering_coolant(struct game_client *c, char *word, float fraction)
{
	int offset = offsetof(struct snis_entity, tsd.ship.coolant_data.maneuvering.r2);
	nl_set_controllable_byte_value(c, word, fraction, offset, no_limit);
}

static void nl_set_impulse_coolant(struct game_client *c, char *word, float fraction)
{
	int offset = offsetof(struct snis_entity, tsd.ship.coolant_data.impulse.r2);
	nl_set_controllable_byte_value(c, word, fraction, offset, no_limit);
}

static void nl_set_warp_coolant(struct game_client *c, char *word, float fraction)
{
	int offset = offsetof(struct snis_entity, tsd.ship.coolant_data.warp.r2);
	nl_set_controllable_byte_value(c, word, fraction, offset, no_limit);
}

static void nl_set_sensor_coolant(struct game_client *c, char *word, float fraction)
{
	int offset = offsetof(struct snis_entity, tsd.ship.coolant_data.sensors.r2);
	nl_set_controllable_byte_value(c, word, fraction, offset, no_limit);
}

static void nl_set_comms_coolant(struct game_client *c, char *word, float fraction)
{
	int offset = offsetof(struct snis_entity, tsd.ship.coolant_data.comms.r2);
	nl_set_controllable_byte_value(c, word, fraction, offset, no_limit);
}

static void nl_set_phaser_coolant(struct game_client *c, char *word, float fraction)
{
	int offset = offsetof(struct snis_entity, tsd.ship.coolant_data.phasers.r2);
	nl_set_controllable_byte_value(c, word, fraction, offset, no_limit);
}

static void nl_set_shield_coolant(struct game_client *c, char *word, float fraction)
{
	int offset = offsetof(struct snis_entity, tsd.ship.coolant_data.shields.r2);
	nl_set_controllable_byte_value(c, word, fraction, offset, no_limit);
}

static void nl_set_tractor_coolant(struct game_client *c, char *word, float fraction)
{
	int offset = offsetof(struct snis_entity, tsd.ship.coolant_data.tractor.r2);
	nl_set_controllable_byte_value(c, word, fraction, offset, no_limit);
}

static void nl_set_lifesupport_coolant(struct game_client *c, char *word, float fraction)
{
	int offset = offsetof(struct snis_entity, tsd.ship.coolant_data.lifesupport.r2);
	nl_set_controllable_byte_value(c, word, fraction, offset, no_limit);
}

static void nl_set_scizoom(struct game_client *c, char *word, float fraction)
{
	int offset = offsetof(struct snis_entity, tsd.ship.scizoom);
	nl_set_controllable_byte_value(c, word, fraction, offset, no_limit);
}

static void nl_set_navzoom(struct game_client *c, char *word, float fraction)
{
	int offset = offsetof(struct snis_entity, tsd.ship.navzoom);
	nl_set_controllable_byte_value(c, word, fraction, offset, no_limit);
}

static void nl_set_weapzoom(struct game_client *c, char *word, float fraction)
{
	int offset = offsetof(struct snis_entity, tsd.ship.weapzoom);
	nl_set_controllable_byte_value(c, word, fraction, offset, no_limit);
}

static void nl_set_mainzoom(struct game_client *c, char *word, float fraction)
{
	int offset = offsetof(struct snis_entity, tsd.ship.mainzoom);
	nl_set_controllable_byte_value(c, word, fraction, offset, no_limit);
}

static float nl_get_zoom(struct game_client *c)
{
	struct snis_entity *o;
	int i;

	i = lookup_by_id(c->shipid);
	if (i < 0) {
		queue_add_text_to_speech(c, "Sorry Dave, I can't do that right now.");
		return -1.0;
	}
	o = &go[i];
	switch (bridgelist[c->bridge].current_displaymode) {
	case DISPLAYMODE_MAINSCREEN:
		return (float) o->tsd.ship.mainzoom / 255.0;
	case DISPLAYMODE_NAVIGATION:
		return (float) o->tsd.ship.navzoom / 255.0;
	case DISPLAYMODE_WEAPONS:
		return (float) o->tsd.ship.weapzoom / 255.0;
	case DISPLAYMODE_SCIENCE:
		return (float) o->tsd.ship.scizoom / 255.0;
	default:
		break;
	}
	queue_add_text_to_speech(c, "I do not understand your zoom request.");
	return -1.0;
}

static void nl_set_zoom(struct game_client *c, char *word, float value)
{
	switch (bridgelist[c->bridge].current_displaymode) {
	case DISPLAYMODE_MAINSCREEN:
		nl_set_mainzoom(c, "main screen zoom", value);
		break;
	case DISPLAYMODE_NAVIGATION:
		nl_set_navzoom(c, "navigation zoom", value);
		break;
	case DISPLAYMODE_WEAPONS:
		nl_set_weapzoom(c, "weapons zoom", value);
		break;
	case DISPLAYMODE_SCIENCE:
		nl_set_scizoom(c, "science zoom", value);
		break;
	default:
		goto no_understand;
	}
	return;

no_understand:
	queue_add_text_to_speech(c, "I do not understand your zoom request.");
}

typedef void (*nl_set_function)(struct game_client *c, char *word, float value);
static struct settable_thing_entry {
	char *name;
	nl_set_function setfn;
} nl_settable_power_thing[] = {
	{ "impulse", nl_set_impulse_power, },
	{ "impulse drive", nl_set_impulse_power, },
	{ "warp", nl_set_warp_power, },
	{ "warp drive", nl_set_warp_power, },
	{ "maneuvering", nl_set_maneuvering_power, },
	{ "sensors", nl_set_sensor_power, },
	{ "sensor", nl_set_sensor_power, },
	{ "communications", nl_set_comms_power, },
	{ "phasers", nl_set_phaser_power, },
	{ "shields", nl_set_shield_power, },
	{ "tractor beam", nl_set_tractor_power, },
	{ "tractor", nl_set_tractor_power, },
	{ "life support", nl_set_lifesupport_power, },
};

static struct settable_thing_entry nl_settable_coolant_thing[] = {
	{ "impulse", nl_set_impulse_coolant, },
	{ "impulse drive", nl_set_impulse_coolant, },
	{ "warp", nl_set_warp_coolant, },
	{ "warp drive", nl_set_warp_coolant, },
	{ "maneuvering", nl_set_maneuvering_coolant, },
	{ "sensors", nl_set_sensor_coolant, },
	{ "sensor", nl_set_sensor_coolant, },
	{ "communications", nl_set_comms_coolant, },
	{ "phasers", nl_set_phaser_coolant, },
	{ "shields", nl_set_shield_coolant, },
	{ "tractor beam", nl_set_tractor_coolant, },
	{ "tractor", nl_set_tractor_coolant, },
	{ "life support", nl_set_lifesupport_coolant, },
};

static struct settable_thing_entry nl_settable_thing[] = {
	{ "shields", nl_set_shields, },
	{ "impulse drive", nl_set_impulse_drive, },
	{ "warp drive", nl_set_warpdrive, },
	{ "maneuvering power", nl_set_maneuvering_power, },
	{ "impulse drive power", nl_set_impulse_power, },
	{ "warp drive power", nl_set_warp_power, },
	{ "sensor power", nl_set_sensor_power, },
	{ "communications power", nl_set_comms_power, },
	{ "phaser power", nl_set_phaser_power, },
	{ "weapons power", nl_set_phaser_power, },
	{ "weapon power", nl_set_phaser_power, },
	{ "shield power", nl_set_shield_power, },
	{ "tractor beam power", nl_set_tractor_power, },
	{ "life support power", nl_set_lifesupport_power, },

	{ "maneuvering coolant", nl_set_maneuvering_coolant, },
	{ "impulse drive coolant", nl_set_impulse_coolant, },
	{ "warp drive coolant", nl_set_warp_coolant, },
	{ "sensor coolant", nl_set_sensor_coolant, },
	{ "communications coolant", nl_set_comms_coolant, },
	{ "phaser coolant", nl_set_phaser_coolant, },
	{ "weapons coolant", nl_set_phaser_coolant, },
	{ "weapon coolant", nl_set_phaser_coolant, },
	{ "shield coolant", nl_set_shield_coolant, },
	{ "tractor beam coolant", nl_set_tractor_coolant, },
	{ "life support coolant", nl_set_lifesupport_coolant, },
	{ "zoom", nl_set_zoom, },
	{ "volume", nl_set_volume, },
};

static void nl_set_npq(void *context, int argc, char *argv[], int pos[],
		union snis_nl_extra_data extra_data[])
{
	struct game_client *c = context;
	int i, noun, prep, value;
	float fraction;
	nl_set_function setit = NULL;
	char answer[100];

	noun = nl_find_next_word(argc, pos, POS_NOUN, 0);
	if (noun < 0)
		goto no_understand;

	for (i = 0; i < ARRAYSIZE(nl_settable_thing); i++) {
		if (strcasecmp(nl_settable_thing[i].name, argv[noun]) == 0) {
			setit = nl_settable_thing[i].setfn;
			break;
		}
	}
	if (!setit) {
		snprintf(answer, sizeof(answer), "Sorry, I do not know how to set the %s", argv[noun]);
		queue_add_text_to_speech(c, answer);
		return;
	}
	prep = nl_find_next_word(argc, pos, POS_PREPOSITION, noun + 1);
	if (prep < 0)
		goto no_understand2;
	value = nl_find_next_word(argc, pos, POS_NUMBER, prep + 1);
	if (value < 0)
		goto no_understand2;
	fraction = extra_data[value].number.value;
	if (fraction > 1.0 && fraction <= 100.0)
		fraction = fraction / 100.0;
	setit(c, argv[noun], fraction);
	return;

no_understand:
	queue_add_text_to_speech(c, "Sorry, I do not know how to set that.");
	return;
no_understand2:
	snprintf(answer, sizeof(answer), "Sorry, I do not understand how you want me to set the %s", argv[noun]);
	queue_add_text_to_speech(c, answer);
	return;
}

/* Assumes universe lock held, will unlock */
static void nl_set_ship_course_to_direction_helper(struct game_client *c,
	struct snis_entity *ship,
	union vec3 direction,
	char *destination_name)
{
	union vec3 right;
	union quat new_orientation;
	char reply[100];

	/* Calculate new desired orientation of ship pointing towards destination */
	right.v.x = 1.0;
	right.v.y = 0.0;
	right.v.z = 0.0;

	vec3_normalize_self(&direction);
	quat_from_u2v(&new_orientation, &right, &direction, NULL);

	ship->tsd.ship.computer_desired_orientation = new_orientation;
	ship->tsd.ship.computer_steering_time_left = COMPUTER_STEERING_TIME;
	ship->tsd.ship.orbiting_object_id = 0xffffffff;
	pthread_mutex_unlock(&universe_mutex);

	snprintf(reply, sizeof(reply), "Setting course for %s.", destination_name);
	queue_add_text_to_speech(c, reply);
	return;
}

/* Assumes universe lock held, will unlock */
static void nl_set_ship_course_to_waypoint_helper(struct game_client *c,
	struct snis_entity *ship,
	int waypoint_number,
	char *destination_name)
{
	union vec3 direction;
	char waypoint_name[20];

	struct player_waypoint *wp = &bridgelist[c->bridge].waypoint[waypoint_number];
	direction.v.x = wp->x - ship->x;
	direction.v.y = wp->y - ship->y;
	direction.v.z = wp->z - ship->z;

	snprintf(waypoint_name, sizeof(waypoint_name), "waypoint %d", waypoint_number);
	nl_set_ship_course_to_direction_helper(c, ship, direction, waypoint_name); /* will unlock */;
	return;
}

/* Assumes universe lock is held, will unlock */
static void nl_set_ship_course_to_dest_helper(struct game_client *c,
	struct snis_entity *ship,
	struct snis_entity *dest,
	char *name)
{
	union vec3 direction;
	char *modifier;
	char dest_name[100];

	direction.v.x = dest->x - ship->x;
	direction.v.y = dest->y - ship->y;
	direction.v.z = dest->z - ship->z;

	if (dest->type == OBJTYPE_PLANET)
		modifier = "the planet ";
	else if (dest->type == OBJTYPE_ASTEROID)
		modifier = "the asteroid ";
	else if (dest->type == OBJTYPE_SHIP1 || dest->type == OBJTYPE_SHIP2)
		modifier = "the ship ";
	else
		modifier = "";

	snprintf(dest_name, sizeof(dest_name), "%s%s", modifier, name);
	nl_set_ship_course_to_direction_helper(c, ship, direction, dest_name);
	return;
}

/* E.g.: navigate to the star base one, navigate to warp gate seven */
static void nl_navigate_pnq(void *context, int argc, char *argv[], int pos[],
	union snis_nl_extra_data extra_data[])
{
	struct game_client *c = context;
	int wp, i, prep, noun;
	char *name, *namecopy;
	char buffer[20];
	struct snis_entity *dest;
	int number;
	float value;

	prep = nl_find_next_word(argc, pos, POS_PREPOSITION, 0);
	if (prep < 0)
		goto no_understand;

	noun = nl_find_next_word(argc, pos, POS_NOUN, prep);
	if (noun < 0)
		goto no_understand;
	number = nl_find_next_word(argc, pos, POS_NUMBER, noun);
	if (number < 0)
		goto no_understand;
	value = extra_data[number].number.value;
	i = -1;
	if (strcasecmp(argv[noun], "starbase") == 0) {
		snprintf(buffer, sizeof(buffer), "SB-%02.0f", value);
		i = natural_language_object_lookup(NULL, buffer); /* slightly racy */
	} else if (strcasecmp(argv[noun], "gate") == 0) {
		snprintf(buffer, sizeof(buffer), "WG-%02.0f", value);
		i = natural_language_object_lookup(NULL, buffer); /* slightly racy */
	} else if (strcasecmp(argv[noun], "waypoint") == 0) {
		snprintf(buffer, sizeof(buffer), "WP-%02.0f", value);
		wp = (int) value;
		if (wp < 0 || wp >= bridgelist[c->bridge].nwaypoints) {
			queue_add_text_to_speech(c, "No such waypoint recorded.");
			return;
		}
		pthread_mutex_lock(&universe_mutex);
		i = lookup_by_id(c->shipid);
		if (i < 0) {
			pthread_mutex_unlock(&universe_mutex);
			queue_add_text_to_speech(c, "Sorry, I am not quite sure where we are.");
			return;
		}
		nl_set_ship_course_to_waypoint_helper(c, &go[i], wp, buffer); /* will unlock */
		return;
	}
	if (i < 0) {
		goto no_understand;
	}
	pthread_mutex_lock(&universe_mutex);
	if (i < 0) {
		pthread_mutex_unlock(&universe_mutex);
		goto no_understand;
	}
	dest = &go[i];
	name = nl_get_object_name(dest);
	if (!name) {
		pthread_mutex_unlock(&universe_mutex);
		queue_add_text_to_speech(c, "Sorry, I am not sure where that is.");
		return;
	}
	namecopy = strdup(name);
	i = lookup_by_id(c->shipid);
	if (i < 0) {
		pthread_mutex_unlock(&universe_mutex);
		queue_add_text_to_speech(c, "Sorry, I am not quite sure where we are.");
		free(namecopy);
		return;
	}

	nl_set_ship_course_to_dest_helper(c, &go[i], dest, namecopy); /* unlocks */
	free(namecopy);
	return;

no_understand:
	queue_add_text_to_speech(c, "I did not understand your request.");
}

/* E.g.: navigate to the nearest starbase */
static void nl_navigate_pn(void *context, int argc, char *argv[], int pos[],
	union snis_nl_extra_data extra_data[])
{
	struct game_client *c = context;
	int i, prep, noun;
	char *name, *namecopy;
	struct snis_entity *dest;

	prep = nl_find_next_word(argc, pos, POS_PREPOSITION, 0);
	if (prep < 0)
		goto no_understand;

	noun = nl_find_next_word(argc, pos, POS_EXTERNAL_NOUN, prep);
	pthread_mutex_lock(&universe_mutex);
	if (noun < 0)
		i = nl_find_nearest_object(c, argc, argv, pos, extra_data, 0);
	else
		i = lookup_by_id(extra_data[noun].external_noun.handle);
	if (i < 0) {
		pthread_mutex_unlock(&universe_mutex);
		goto no_understand;
	}
	dest = &go[i];
	name = nl_get_object_name(dest);
	if (!name) {
		pthread_mutex_unlock(&universe_mutex);
		queue_add_text_to_speech(c, "Sorry, I am not sure where that is.");
		return;
	}
	namecopy = strdup(name);
	i = lookup_by_id(c->shipid);
	if (i < 0) {
		pthread_mutex_unlock(&universe_mutex);
		queue_add_text_to_speech(c, "Sorry, I am not quite sure where we are.");
		free(namecopy);
		return;
	}

	nl_set_ship_course_to_dest_helper(c, &go[i], dest, namecopy); /* unlocks */
	free(namecopy);
	return;

no_understand:
	queue_add_text_to_speech(c, "I did not understand your request.");
}

/* Eg: "set a course for starbase one..." */
static void nl_set_npnq(void *context, int argc, char *argv[], int pos[],
		union snis_nl_extra_data extra_data[])
{
	struct game_client *c = context;
	int setthing, prep, settowhat;
	char *name, *namecopy;
	int wp, i = -1;
	struct snis_entity *dest;
	int number;
	float value;
	char buffer[100];

	setthing = nl_find_next_word(argc, pos, POS_NOUN, 0);
	if (setthing < 0)
		goto no_understand;
	prep = nl_find_next_word(argc, pos, POS_PREPOSITION, 0);
	if (prep < 0)
		goto no_understand;
	settowhat = nl_find_next_word(argc, pos, POS_NOUN, prep);
	if (settowhat < 0)
		goto no_understand;
	number = nl_find_next_word(argc, pos, POS_NUMBER, 0);
	if (number < 0)
		goto no_understand;
	value = extra_data[number].number.value;
	i = -1;
	if (strcasecmp(argv[settowhat], "starbase") == 0) {
		snprintf(buffer, sizeof(buffer), "SB-%02.0f", value);
		i = natural_language_object_lookup(NULL, buffer); /* slightly racy */
	} else if (strcasecmp(argv[settowhat], "gate") == 0) {
		snprintf(buffer, sizeof(buffer), "WG-%02.0f", value);
		i = natural_language_object_lookup(NULL, buffer); /* slightly racy */
	} else if (strcasecmp(argv[settowhat], "waypoint") == 0) {
		snprintf(buffer, sizeof(buffer), "WP-%02.0f", value);
		wp = (int) value;
		if (wp < 0 || wp >= bridgelist[c->bridge].nwaypoints) {
			queue_add_text_to_speech(c, "No such waypoint recorded.");
			return;
		}
		pthread_mutex_lock(&universe_mutex);
		i = lookup_by_id(c->shipid);
		if (i < 0) {
			pthread_mutex_unlock(&universe_mutex);
			queue_add_text_to_speech(c, "Sorry, I am not quite sure where we are.");
			return;
		}
		nl_set_ship_course_to_waypoint_helper(c, &go[i], wp, buffer); /* will unlock */
		return;
	}
	if (i < 0)
		goto no_understand;
	pthread_mutex_lock(&universe_mutex);

	if (strcasecmp(argv[prep], "for") != 0 &&
		strcasecmp(argv[prep], "to") != 0 &&
		strcasecmp(argv[prep], "toward") != 0) {
		pthread_mutex_unlock(&universe_mutex);
		goto no_understand;
	}

	if (strcasecmp(argv[setthing], "course") != 0) {
		pthread_mutex_unlock(&universe_mutex);
		goto no_understand;
	}

	if (i < 0)
		i = lookup_by_id(extra_data[settowhat].external_noun.handle);
	if (i < 0) {
		pthread_mutex_unlock(&universe_mutex);
		queue_add_text_to_speech(c, "Sorry, I am not sure where that is.");
		return;
	}
	dest = &go[i];
	name = nl_get_object_name(dest);
	if (!name) {
		pthread_mutex_unlock(&universe_mutex);
		queue_add_text_to_speech(c, "Sorry, I am not sure where that is.");
		return;
	}
	namecopy = strdup(name);
	i = lookup_by_id(c->shipid);
	if (i < 0) {
		pthread_mutex_unlock(&universe_mutex);
		queue_add_text_to_speech(c, "Sorry, I am not quite sure where we are.");
		free(namecopy);
		return;
	}

	nl_set_ship_course_to_dest_helper(c, &go[i], dest, namecopy); /* unlocks */
	free(namecopy);
	return;

no_understand:
	queue_add_text_to_speech(c, "Sorry, I am not sure what you're asking me to do.");
	return;
}

/* Eg: "set a course for blah..." */
static void nl_set_npn(void *context, int argc, char *argv[], int pos[],
		union snis_nl_extra_data extra_data[])
{
	struct game_client *c = context;
	int setthing, prep, settowhat;
	char *name, *namecopy;
	int i = -1;
	struct snis_entity *dest;

	setthing = nl_find_next_word(argc, pos, POS_NOUN, 0);
	if (setthing < 0)
		goto no_understand;
	prep = nl_find_next_word(argc, pos, POS_PREPOSITION, 0);
	if (prep < 0)
		goto no_understand;
	settowhat = nl_find_next_word(argc, pos, POS_EXTERNAL_NOUN, prep);
	pthread_mutex_lock(&universe_mutex);
	if (settowhat < 0) {
		i = nl_find_nearest_object(c, argc, argv, pos, extra_data, prep);
		if (i < 0) {
			pthread_mutex_unlock(&universe_mutex);
			goto no_understand;
		}
	}

	if (strcasecmp(argv[prep], "for") != 0 &&
		strcasecmp(argv[prep], "to") != 0 &&
		strcasecmp(argv[prep], "toward") != 0) {
		pthread_mutex_unlock(&universe_mutex);
		goto no_understand;
	}

	if (strcasecmp(argv[setthing], "course") != 0) {
		pthread_mutex_unlock(&universe_mutex);
		goto no_understand;
	}

	if (i < 0)
		i = lookup_by_id(extra_data[settowhat].external_noun.handle);
	if (i < 0) {
		pthread_mutex_unlock(&universe_mutex);
		queue_add_text_to_speech(c, "Sorry, I am not sure where that is.");
		return;
	}
	dest = &go[i];
	name = nl_get_object_name(dest);
	if (!name) {
		pthread_mutex_unlock(&universe_mutex);
		queue_add_text_to_speech(c, "Sorry, I am not sure where that is.");
		return;
	}
	namecopy = strdup(name);
	i = lookup_by_id(c->shipid);
	if (i < 0) {
		pthread_mutex_unlock(&universe_mutex);
		queue_add_text_to_speech(c, "Sorry, I am not quite sure where we are.");
		free(namecopy);
		return;
	}

	nl_set_ship_course_to_dest_helper(c, &go[i], dest, namecopy); /* unlocks */
	free(namecopy);
	return;

no_understand:
	queue_add_text_to_speech(c, "Sorry, I am not sure what you're asking me to do.");
	return;
}

static void nl_disengage_n(void *context, int argc, char *argv[], int pos[],
		union snis_nl_extra_data extra_data[])
{
	struct game_client *c = context;
	int i, device;
	char response[100];

	device = nl_find_next_word(argc, pos, POS_NOUN, 0);
	if (device < 0)
		goto no_understand;

	pthread_mutex_lock(&universe_mutex);
	i = lookup_by_id(c->shipid);
	if (i < 0) {
		pthread_mutex_unlock(&universe_mutex);
		snprintf(response, sizeof(response), "Sorry, I cannot seem to disengage the %s.", argv[device]);
		queue_add_text_to_speech(c, response);
		return;
	}

	if (strcasecmp("docking system", argv[device]) == 0) {
		if (!go[i].tsd.ship.docking_magnets) {
			pthread_mutex_unlock(&universe_mutex);
			queue_add_text_to_speech(c, "The docking system is already disengaged");
			return;
		} else {
			go[i].tsd.ship.docking_magnets = 0;
			pthread_mutex_unlock(&universe_mutex);
			queue_add_text_to_speech(c, "Docking system disengaged");
			return;
		}
	} else {
		if (strcasecmp("tractor beam", argv[device]) == 0) {
			int did_something = turn_off_tractorbeam(&go[i]);
			pthread_mutex_unlock(&universe_mutex);
			if (did_something)
				queue_add_text_to_speech(c, "Tractor beam disengaged");
			else
				queue_add_text_to_speech(c, "The tractor beam is already disengaged");
			return;
		} else {
			if (strcasecmp("red alert", argv[device]) == 0) {
				pthread_mutex_unlock(&universe_mutex);
				set_red_alert_mode(c, 0);
				return;
			} else {
				pthread_mutex_unlock(&universe_mutex);
				goto no_understand;
			}
		}
	}

no_understand:
	queue_add_text_to_speech(c, "Sorry, I do not know how to disengage that.");
	return;
}

/* increase power to impulse, increase coolant to blah, raise shields to maximum.
 * "raise" value of 0 means decrease, 1 means increase, 2 means set, used only in
 * figuring maximum/minimum and in reply construction.
 */
static void nl_raise_or_lower_npa(void *context, int argc, char *argv[], int pos[],
		union snis_nl_extra_data extra_data[], int raise)
{
	struct game_client *c = context;
	nl_set_function setit = NULL;
	char answer[100];
	int i, noun, prep, adj;
	char *active_verb;

	switch (raise) {
	case 0: active_verb = "increase";
		break;
	case 1: active_verb = "decrease";
		break;
	default:
	case 2: active_verb = "set";
		break;
	}

	noun = nl_find_next_word(argc, pos, POS_NOUN, 0);
	if (noun < 0)
		goto no_understand;
	prep = nl_find_next_word(argc, pos, POS_PREPOSITION, noun + 1);
	if (prep < 0)
		goto no_understand;
	adj = nl_find_next_word(argc, pos, POS_ADJECTIVE, prep + 1);
	if (adj < 0)
		goto no_understand;

	for (i = 0; i < ARRAYSIZE(nl_settable_thing); i++) {
		if (strcasecmp(nl_settable_thing[i].name, argv[noun]) == 0) {
			setit = nl_settable_thing[i].setfn;
			break;
		}
	}
	if (setit) {
		if ((raise == 1  || raise == 2) && strcasecmp(argv[adj], "maximum") == 0) {
			setit(c, argv[noun], 1.0);
			return;
		}
		if ((raise == 0 || raise == 2) && strcasecmp(argv[adj], "minimum") == 0) {
			setit(c, argv[noun], 0.0);
			return;
		}
		snprintf(answer, sizeof(answer), "I don't know how to %s %s %s %s\n", active_verb,
				argv[noun], argv[prep], argv[adj]);
		queue_add_text_to_speech(c, answer);
		return;
	} else if (strcasecmp(argv[noun], "power") == 0) {
		for (i = 0; i < ARRAYSIZE(nl_settable_power_thing); i++) {
			if (strcasecmp(nl_settable_power_thing[i].name, argv[adj]) == 0) {
				setit = nl_settable_power_thing[i].setfn;
				break;
			}
		}
		if (!setit) {
			snprintf(answer, sizeof(answer), "I don't know how to increase power to that.\n");
			queue_add_text_to_speech(c, answer);
			return;
		}
		setit(c, argv[adj], (raise == 0) ? 0.0 : 1.0);
		return;
	} else if (strcasecmp(argv[noun], "coolant") == 0) {
		for (i = 0; i < ARRAYSIZE(nl_settable_coolant_thing); i++) {
			if (strcasecmp(nl_settable_coolant_thing[i].name, argv[adj]) == 0) {
				setit = nl_settable_coolant_thing[i].setfn;
				break;
			}
		}
		if (!setit) {
			snprintf(answer, sizeof(answer), "I don't know how to %s power to that.\n", active_verb);
			queue_add_text_to_speech(c, answer);
			return;
		}
		setit(c, argv[adj], (raise == 0) ? 0.0 : 1.0);
		return;
	}
no_understand:
	queue_add_text_to_speech(c, "I don't know how to adjust that.");
}

static void nl_raise_npa(void *context, int argc, char *argv[], int pos[],
		union snis_nl_extra_data extra_data[])
{
	nl_raise_or_lower_npa(context, argc, argv, pos, extra_data, 1);
}

static void nl_lower_npa(void *context, int argc, char *argv[], int pos[],
		union snis_nl_extra_data extra_data[])
{
	nl_raise_or_lower_npa(context, argc, argv, pos, extra_data, 0);
}

static void nl_set_npa(void *context, int argc, char *argv[], int pos[],
		union snis_nl_extra_data extra_data[])
{
	nl_raise_or_lower_npa(context, argc, argv, pos, extra_data, 2);
}

/* "raise shields" */
static void nl_shields_p(void *context, int argc, char *argv[], int pos[],
		union snis_nl_extra_data extra_data[])
{
	struct game_client *c = context;
	int prep;

	prep = nl_find_next_word(argc, pos, POS_PREPOSITION, 0);
	if (prep < 0)
		goto no_understand;
	if (strcasecmp(argv[prep], "up") == 0)
		nl_set_shields(c, "shields", 1.0);
	else if (strcasecmp(argv[prep], "down") == 0)
		nl_set_shields(c, "shields", 0.0);
	else
		goto no_understand;
	return;

no_understand:
	queue_add_text_to_speech(c, "Sorry, I do not know what you want me to do with the shields.");
}

/* "raise shields" */
static void nl_raise_or_lower_n(void *context, int argc, char *argv[], int pos[],
		union snis_nl_extra_data extra_data[], int raise)
{
	struct game_client *c = context;
	nl_set_function setit = NULL;
	char answer[100];
	int i, noun;

	noun = nl_find_next_word(argc, pos, POS_NOUN, 0);
	if (noun < 0)
		goto no_understand;

	for (i = 0; i < ARRAYSIZE(nl_settable_thing); i++) {
		if (strcasecmp(nl_settable_thing[i].name, argv[noun]) == 0) {
			setit = nl_settable_thing[i].setfn;
			break;
		}
	}
	if (!setit) {
		snprintf(answer, sizeof(answer), "Sorry, I do not know how to raise the %s", argv[noun]);
		queue_add_text_to_speech(c, answer);
		return;
	}
	setit(c, argv[noun], raise ? 1.0 : 0.0);
	return;

no_understand:
	if (raise)
		queue_add_text_to_speech(c, "Sorry, I do not know how to increase that.");
	else
		queue_add_text_to_speech(c, "Sorry, I do not know how to decrease that.");
}

static void nl_raise_n(void *context, int argc, char *argv[], int pos[],
		union snis_nl_extra_data extra_data[])
{
	nl_raise_or_lower_n(context, argc, argv, pos, extra_data, 1);
}

static void nl_lower_n(void *context, int argc, char *argv[], int pos[],
		union snis_nl_extra_data extra_data[])
{
	nl_raise_or_lower_n(context, argc, argv, pos, extra_data, 0);
}

static void nl_engage_n(void *context, int argc, char *argv[], int pos[],
		union snis_nl_extra_data extra_data[])
{
	struct game_client *c = context;
	int i;
	int device, enough_oomph;
	char response[100];

	device = nl_find_next_word(argc, pos, POS_NOUN, 0);
	if (device < 0)
		goto no_understand;

	pthread_mutex_lock(&universe_mutex);
	i = lookup_by_id(c->shipid);
	if (i < 0) {
		pthread_mutex_unlock(&universe_mutex);
		snprintf(response, sizeof(response), "Sorry, I cannot seem to engage the %s.", argv[device]);
		queue_add_text_to_speech(c, response);
		return;
	}

	if (strcasecmp("warp drive", argv[device]) == 0) {
		pthread_mutex_unlock(&universe_mutex);
		enough_oomph = do_engage_warp_drive(&go[i]);
		send_initiate_warp_packet(c, enough_oomph);
		snprintf(response, sizeof(response), "%s engaged", argv[device]);
		queue_add_text_to_speech(c, response);
		return;
	} else if (strcasecmp("docking system", argv[device]) == 0) {
		if (go[i].tsd.ship.docking_magnets) {
			pthread_mutex_unlock(&universe_mutex);
			queue_add_text_to_speech(c, "The docking system is already engaged");
			return;
		} else {
			go[i].tsd.ship.docking_magnets = 1;
			pthread_mutex_unlock(&universe_mutex);
			queue_add_text_to_speech(c, "Docking system engaged");
			return;
		}
	} else if (strcasecmp("tractor beam", argv[device]) == 0) {
		pthread_mutex_unlock(&universe_mutex);
		turn_on_tractor_beam(c, &go[i], 0xffffffff, 0);
		return;
	} else if (strcasecmp("red alert", argv[device]) == 0) {
		pthread_mutex_unlock(&universe_mutex);
		set_red_alert_mode(c, 1);
		return;
	} else {
		pthread_mutex_unlock(&universe_mutex);
		goto no_understand;
	}

no_understand:
	queue_add_text_to_speech(c, "Sorry, I do not know how to engage that.");
	return;
}

static void nl_engage_npn(void *context, int argc, char *argv[], int pos[],
		union snis_nl_extra_data extra_data[])
{
	struct game_client *c = context;
	int i, device, prep, target;
	char reply[100];

	device = nl_find_next_word(argc, pos, POS_NOUN, 0);
	if (device < 0)
		goto no_understand;
	prep = nl_find_next_word(argc, pos, POS_PREPOSITION, 0);
	if (prep < 0)
		goto no_understand;
	target = nl_find_next_word(argc, pos, POS_EXTERNAL_NOUN, 0);
	if (target < 0)
		goto no_understand;

	if (strcasecmp("tractor beam", argv[device]) == 0) {
		pthread_mutex_lock(&universe_mutex);
		i = lookup_by_id(c->shipid);
		if (i < 0) {
			pthread_mutex_unlock(&universe_mutex);
			snprintf(reply, sizeof(reply), "Sorry, I cannot seem to engage the %s.", argv[device]);
			queue_add_text_to_speech(c, reply);
			return;
		}
		pthread_mutex_unlock(&universe_mutex);
		turn_on_tractor_beam(c, &go[i], extra_data[target].external_noun.handle, 0);
		return;
	} else {
		snprintf(reply, sizeof(reply), "I do not understand how engaging the %s relates to the %s\n",
				argv[device], argv[target]);
		queue_add_text_to_speech(c, reply);
		return;
	}
	return;

no_understand:
	queue_add_text_to_speech(c, "Sorry, I am unfamiliar with the way you are using the word engage");
}

static void nl_leave_or_enter_n(void *context, int argc, char *argv[], int pos[],
			__attribute__((unused)) union snis_nl_extra_data extra_data[], int entering)
{
	struct game_client *c = context;
	int i, noun;

	noun = nl_find_next_word(argc, pos, POS_NOUN, 0);
	if (noun < 0)
		goto no_understand;
	pthread_mutex_lock(&universe_mutex);
	i = lookup_by_id(c->shipid);
	if (i < 0) {
		pthread_mutex_unlock(&universe_mutex);
		queue_add_text_to_speech(c, "Sorry, I seem to have lost my mind.");
		return;
	}
	if (strcasecmp(argv[noun], "orbit") == 0) {
		if (entering && go[i].tsd.ship.orbiting_object_id != 0xffffffff) {
			pthread_mutex_unlock(&universe_mutex);
			queue_add_text_to_speech(c, "Already in standard orbit");
			return;
		}
		if (!entering && go[i].tsd.ship.orbiting_object_id == 0xffffffff) {
			pthread_mutex_unlock(&universe_mutex);
			queue_add_text_to_speech(c, "We are not in standard orbit");
			return;
		}
		toggle_standard_orbit(c, &go[i]); /* releases universe mutex */
		return;
	}
	pthread_mutex_unlock(&universe_mutex);

no_understand:
	queue_add_text_to_speech(c, "Sorry, I am unfamiliar with the way you are using the word leave");
}

static void nl_leave_n(void *context, int argc, char *argv[], int pos[], union snis_nl_extra_data extra_data[])
{
	nl_leave_or_enter_n(context, argc, argv, pos, extra_data, 0);
}

static void nl_leave_an(void *context, int argc, char *argv[], int pos[], union snis_nl_extra_data extra_data[])
{
	nl_leave_or_enter_n(context, argc, argv, pos, extra_data, 0);
}

static void nl_enter_n(void *context, int argc, char *argv[], int pos[], union snis_nl_extra_data extra_data[])
{
	nl_leave_or_enter_n(context, argc, argv, pos, extra_data, 1);
}

static void nl_enter_an(void *context, int argc, char *argv[], int pos[], union snis_nl_extra_data extra_data[])
{
	nl_leave_or_enter_n(context, argc, argv, pos, extra_data, 1);
}

static void nl_red_alert(void *context,
			__attribute__((unused)) int argc,
			__attribute__((unused)) char *argv[], int pos[],
			__attribute__((unused)) union snis_nl_extra_data extra_data[])
{
	struct game_client *c = context;
	unsigned char new_alert_mode = 1;
	set_red_alert_mode(c, new_alert_mode);
}

static void nl_red_alert_p(void *context, int argc, char *argv[], int pos[],
			__attribute__((unused)) union snis_nl_extra_data extra_data[])
{
	struct game_client *c = context;
	unsigned char new_alert_mode = 0;
	char reply[100];
	int prep;

	prep = nl_find_next_word(argc, pos, POS_PREPOSITION, 0);
	if (prep < 0)
		goto no_understand; /* Should not get here, there really should be a preposition. */
	if (strcasecmp(argv[prep], "on") == 0) {
		new_alert_mode = 1;
	} else {
		if (strcasecmp(argv[prep], "off") == 0) {
			new_alert_mode = 0;
		} else {
			snprintf(reply, sizeof(reply), "Sorry, I do not know what red alert %s means.", argv[prep]);
			queue_add_text_to_speech(c, reply);
			return;
		}
	}
	set_red_alert_mode(c, new_alert_mode);

no_understand:
	snprintf(reply, sizeof(reply), "Did you want red alert on or off?  I seem to have missed the preposition.");
	return;
}

static int string_to_displaymode(char *string)
{
	if (strcasecmp(string, "navigation") == 0)
		return DISPLAYMODE_NAVIGATION;
	else if (strcasecmp(string, "main view") == 0)
		return DISPLAYMODE_MAINSCREEN;
	else if (strcasecmp(string, "weapons") == 0)
		return DISPLAYMODE_WEAPONS;
	else if (strcasecmp(string, "engineering") == 0)
		return DISPLAYMODE_ENGINEERING;
	else if (strcasecmp(string, "science") == 0)
		return DISPLAYMODE_SCIENCE;
	else if (strcasecmp(string, "communications") == 0)
		return DISPLAYMODE_COMMS;
	return -1;
}

static void nl_onscreen_verb_n(void *context, int argc, char *argv[], int pos[],
			__attribute__((unused)) union snis_nl_extra_data extra_data[])
{
	struct game_client *c = context;
	int verb, noun;
	int new_displaymode = 255;
	char reply[100];

	verb = nl_find_next_word(argc, pos, POS_VERB, 0);
	if (verb < 0)
		goto no_understand;
	noun = nl_find_next_word(argc, pos, POS_NOUN, 0);
	if (noun < 0)
		goto no_understand;
	new_displaymode = string_to_displaymode(argv[verb]);
	if (new_displaymode < 0)
		goto no_understand;

	if (strcasecmp(argv[noun], "screen") != 0)
		goto no_understand;

	snprintf(reply, sizeof(reply), "Main screen displaying %s", argv[verb]);
	queue_add_text_to_speech(c, reply);
	send_packet_to_all_clients_on_a_bridge(c->shipid,
			snis_opcode_pkt("bb", OPCODE_ROLE_ONSCREEN, new_displaymode),
			ROLE_MAIN);
	bridgelist[c->bridge].current_displaymode = new_displaymode;
	return;

no_understand:
	queue_add_text_to_speech(c, "I do not understand your request.");
	return;
}

static void nl_onscreen_verb_pn(void *context, int argc, char *argv[], int pos[],
			__attribute__((unused)) union snis_nl_extra_data extra_data[])
{
	struct game_client *c = context;
	int verb, prep, noun;
	int new_displaymode = 255;
	char reply[100];

	verb = nl_find_next_word(argc, pos, POS_VERB, 0);
	if (verb < 0)
		goto no_understand;
	prep = nl_find_next_word(argc, pos, POS_PREPOSITION, 0);
	if (prep < 0)
		goto no_understand;
	noun = nl_find_next_word(argc, pos, POS_NOUN, 0);
	if (noun < 0)
		goto no_understand;
	new_displaymode = string_to_displaymode(argv[verb]);
	if (new_displaymode < 0)
		goto no_understand;

	if (strcasecmp(argv[prep], "on") != 0)
		goto no_understand;

	if (strcasecmp(argv[noun], "screen") != 0)
		goto no_understand;

	snprintf(reply, sizeof(reply), "Main screen displaying %s", argv[verb]);
	queue_add_text_to_speech(c, reply);
	send_packet_to_all_clients_on_a_bridge(c->shipid,
			snis_opcode_pkt("bb", OPCODE_ROLE_ONSCREEN, new_displaymode),
			ROLE_MAIN);
	bridgelist[c->bridge].current_displaymode = new_displaymode;
	return;

no_understand:
	queue_add_text_to_speech(c, "I do not understand your request.");
	return;
}

/* "zoom 10%" */
static void nl_zoom_q(void *context, int argc, char *argv[], int pos[], union snis_nl_extra_data extra_data[])
{
	struct game_client *c = context;
	int number;
	float amount;

	number = nl_find_next_word(argc, pos, POS_NUMBER, 0);
	if (number < 0)
		goto no_understand;
	amount = extra_data[number].number.value;
	if (amount > 1.0)
		amount = amount / 100.0;
	if (amount < 0.0 || amount > 1.0)
		goto no_understand;
	nl_set_zoom(c, "", amount);
	return;
no_understand:
	queue_add_text_to_speech(c, "I do not understand your zoom request.");
}

/* "zoom in/out x%" */
static void nl_zoom_pq(void *context, int argc, char *argv[], int pos[], union snis_nl_extra_data extra_data[])
{
	struct game_client *c = context;
	int prep, number;
	float direction = 1.0;
	float amount;

	prep = nl_find_next_word(argc, pos, POS_PREPOSITION, 0);
	if (prep < 0)
		goto no_understand;
	if (strcasecmp(argv[prep], "out") == 0)
		direction = -1.0;
	else
		direction = 1.0;
	number = nl_find_next_word(argc, pos, POS_NUMBER, prep);
	if (number < 0)
		goto no_understand;
	amount = extra_data[number].number.value;
	if (amount > 1.0)
		amount = amount / 100.0;
	if (amount < 0.0 || amount > 1.0)
		goto no_understand;

	if (direction < 0)
		amount = 1.0 - amount;

	nl_set_zoom(c, "", amount);
	return;

no_understand:
	queue_add_text_to_speech(c, "I do not understand your zoom request.");
}

/* Just "zoom" or "magnify", by itself */
static void nl_zoom(void *context, int argc, char *argv[], int pos[],
			union snis_nl_extra_data extra_data[])
{
	struct game_client *c = context;
	float amount;

	amount = nl_get_zoom(c);
	if (amount < 0.0) /* This means we couldn't find our ship object. */
		return;
	if (amount >= 0.999) {
		queue_add_text_to_speech(c, "Already at maximum zoom.");
		return;
	}
	amount = amount + 0.25;
	if (amount > 1.0)
		amount = 1.0;
	nl_set_zoom(c, "", amount);
	return;
}

/* "zoom in/out" */
static void nl_zoom_p(void *context, int argc, char *argv[], int pos[],
			union snis_nl_extra_data extra_data[])
{
	struct game_client *c = context;
	int prep;
	float direction = 1.0;
	float amount;

	amount = nl_get_zoom(c);
	if (amount < 0.0) /* This means we couldn't find our ship object. */
		return;
	prep = nl_find_next_word(argc, pos, POS_PREPOSITION, 0);
	if (prep < 0)
		goto no_understand;
	if (strcasecmp(argv[prep], "out") == 0)
		direction = -1.0;
	else
		direction = 1.0;

	if (direction < 0) {
		if (amount <= 0.001) {
			queue_add_text_to_speech(c, "Already at minimum zoom.");
			return;
		}
		amount = amount - 0.25;
		if (amount < 0.0)
			amount = 0.0;
	} else {
		if (amount >= 0.999) {
			queue_add_text_to_speech(c, "Already at maximum zoom.");
			return;
		}
		amount = amount + 0.25;
		if (amount > 1.0)
			amount = 1.0;
	}
	nl_set_zoom(c, "", amount);
	return;

no_understand:
	queue_add_text_to_speech(c, "I do not understand your zoom request.");
}

static void nl_reverse_n(void *context, int argc, char *argv[], int pos[],
			union snis_nl_extra_data extra_data[])
{
	struct game_client *c = context;
	int i, noun;
	struct snis_entity *o;

	noun = nl_find_next_word(argc, pos, POS_NOUN, 0);
	if (noun < 0)
		goto no_understand;

	if (strcasecmp(argv[noun], "polarity") == 0) {
		queue_add_text_to_speech(c, "Very funny, Dave");
		return;
	}
	if (strcasecmp(argv[noun], "thrusters") == 0 ||
		strcasecmp(argv[noun], "impulse drive") == 0) {
		goto full_reverse;
	}
	queue_add_text_to_speech(c, "I do not how to reverse that.");
	return;

full_reverse:
	pthread_mutex_lock(&universe_mutex);
	i = lookup_by_id(c->shipid);
	if (i < 0) {
		pthread_mutex_unlock(&universe_mutex);
		queue_add_text_to_speech(c, "I lost my train of thought.");
		return;
	}
	o = &go[i];
	o->tsd.ship.reverse = 1;
	pthread_mutex_unlock(&universe_mutex);
	nl_set_impulse_drive(c, "impulse drive", 1.0);
	return;

no_understand:
	queue_add_text_to_speech(c, "I do not understand your zoom request.");
}

static void nl_shortlong_range_scan(void *context, int argc, char *argv[], int pos[],
			union snis_nl_extra_data extra_data[])
{
	struct game_client *c = context;
	uint8_t mode;

	int verb;

	verb = nl_find_next_word(argc, pos, POS_VERB, 0);
	if (verb < 0)
		goto no_understand;

	if (strcasecmp("long range scan", argv[verb]) == 0)
		mode = SCI_DETAILS_MODE_THREED;
	else if (strcasecmp("short range scan", argv[verb]) == 0)
		mode = SCI_DETAILS_MODE_SCIPLANE;
	else if (strcasecmp("details", argv[verb]) == 0)
		mode = SCI_DETAILS_MODE_DETAILS;
	else
		goto no_understand;
	send_packet_to_all_clients_on_a_bridge(c->shipid,
			snis_opcode_pkt("bb", OPCODE_SCI_DETAILS, mode), ROLE_ALL | ROLE_SCIENCE);
	return;

no_understand:
	queue_add_text_to_speech(c, "I do not understand your request.");
}

static void nl_help(void *context, int argc, char *argv[], int pos[],
			union snis_nl_extra_data extra_data[])
{
	struct game_client *c = context;

	queue_add_text_to_speech(c, "Sure.  Just ask me what you would like me to do in plain English.");
}

static void nl_target_n(void *context, int argc, char *argv[], int pos[], union snis_nl_extra_data extra_data[])
{
	struct game_client *c = context;
	int i, noun;
	char *name, *namecopy;
	char reply[100];
	uint32_t id;

	noun = nl_find_next_word(argc, pos, POS_EXTERNAL_NOUN, 0);
	pthread_mutex_lock(&universe_mutex);
	if (noun < 0) {
		i = nl_find_nearest_object(c, argc, argv, pos, extra_data, 0);
		if (i < 0) {
			pthread_mutex_unlock(&universe_mutex);
			goto target_lost;
		}
		id = go[i].id;
	} else {
		id = extra_data[noun].external_noun.handle;
		i = lookup_by_id(id);
	}
	if (i < 0) {
		pthread_mutex_unlock(&universe_mutex);
		goto target_lost;
	}
	name = nl_get_object_name(&go[i]);
	if (name)
		namecopy = strdup(name);
	else
		namecopy = NULL;
	pthread_mutex_unlock(&universe_mutex);
	if (!namecopy)
		goto target_lost;
	snprintf(reply, sizeof(reply), "Targeting sensors on %s", namecopy);
	free(namecopy);
	queue_add_text_to_speech(c, reply);
	science_select_target(c, OPCODE_SCI_SELECT_TARGET_TYPE_OBJECT, id);
	return;

target_lost:
	queue_add_text_to_speech(c, "Unable to locate the specified target for scanning.");
	return;
}

/* E.g.: target warp gate 1, target star base 2 */
static void nl_target_nq(void *context, int argc, char *argv[], int pos[], union snis_nl_extra_data extra_data[])
{
	struct game_client *c = context;
	int i, noun;
	char *name, *namecopy;
	char reply[100];
	uint32_t id;
	int number;
	char buffer[20];
	float amount;

	noun = nl_find_next_word(argc, pos, POS_NOUN, 0);
	if (noun < 0)
		goto target_lost;
	number = nl_find_next_word(argc, pos, POS_NUMBER, noun);
	if (number < 0)
		goto target_lost;
	amount = extra_data[number].number.value;
	if (strcmp(argv[noun], "starbase") == 0) {
		snprintf(buffer, sizeof(buffer), "SB-%02.0f", amount);
		i = natural_language_object_lookup(NULL, buffer); /* slightly racy */
	} else if (strcmp(argv[noun], "gate") == 0) {
		snprintf(buffer, sizeof(buffer), "WG-%02.0f", amount);
		i = natural_language_object_lookup(NULL, buffer); /* slightly racy */
	} else {
		goto target_lost;
	}

	pthread_mutex_lock(&universe_mutex);
	if (i < 0) {
		pthread_mutex_unlock(&universe_mutex);
		goto target_lost;
	}
	id = go[i].id;
	name = nl_get_object_name(&go[i]);
	if (name)
		namecopy = strdup(name);
	else
		namecopy = NULL;
	pthread_mutex_unlock(&universe_mutex);
	if (!namecopy)
		goto target_lost;
	snprintf(reply, sizeof(reply), "Targeting sensors on %s", namecopy);
	free(namecopy);
	queue_add_text_to_speech(c, reply);
	science_select_target(c, OPCODE_SCI_SELECT_TARGET_TYPE_OBJECT, id);
	return;

target_lost:
	queue_add_text_to_speech(c, "Unable to locate the specified target for scanning.");
	return;
}

struct damage_report_entry {
	char system[100];
	int percent;
};

static int compare_damage_report_entries(const void *a, const void *b)
{
	const struct damage_report_entry * const dra = a;
	const struct damage_report_entry * const drb = b;

	return dra->percent > drb->percent;
}

static void nl_damage_report(void *context, int argc, char *argv[], int pos[],
		__attribute__((unused)) union snis_nl_extra_data extra_data[])
{
	struct game_client *c = context;
	struct damage_report_entry dr[8];
	struct snis_entity *o;
	int i;
	char damage_report[250];
	char next_bit[100];
	int ok = 0;

	pthread_mutex_lock(&universe_mutex);
	i = lookup_by_id(c->shipid);
	if (i < 0) {
		pthread_mutex_unlock(&universe_mutex);
		queue_add_text_to_speech(c,
			"I seem to be unable to collect the necessary data to provide a damage report.");
		return;
	}
	o = &go[i];

	dr[0].percent = (int) (100.0 * (1.0 - (float) o->tsd.ship.damage.maneuvering_damage / 255.0));
	snprintf(dr[0].system, sizeof(dr[0].system), "Maneuvering");
	dr[1].percent = (int) (100.0 * (1.0 - (float) o->tsd.ship.damage.warp_damage / 255.0));
	snprintf(dr[1].system, sizeof(dr[1].system), "Impulse drive");
	dr[2].percent = (int) (100.0 * (1.0 - (float) o->tsd.ship.damage.impulse_damage / 255.0));
	snprintf(dr[2].system, sizeof(dr[2].system), "Warp drive");
	dr[3].percent = (int) (100.0 * (1.0 - (float) o->tsd.ship.damage.phaser_banks_damage / 255.0));
	snprintf(dr[3].system, sizeof(dr[3].system), "Phasers");
	dr[4].percent = (int) (100.0 * (1.0 - (float) o->tsd.ship.damage.comms_damage / 255.0));
	snprintf(dr[4].system, sizeof(dr[4].system), "Communications");
	dr[5].percent = (int) (100.0 * (1.0 - (float) o->tsd.ship.damage.sensors_damage / 255.0));
	snprintf(dr[5].system, sizeof(dr[5].system), "Sensors");
	dr[6].percent = (int) (100.0 * (1.0 - (float) o->tsd.ship.damage.shield_damage / 255.0));
	snprintf(dr[6].system, sizeof(dr[6].system), "Shields");
	dr[7].percent = (int) (100.0 * (1.0 - (float) o->tsd.ship.damage.tractor_damage / 255.0));
	snprintf(dr[7].system, sizeof(dr[7].system), "Tractor beam");
	pthread_mutex_unlock(&universe_mutex);

	qsort(dr, 8, sizeof(dr[0]), compare_damage_report_entries);

	snprintf(damage_report, sizeof(damage_report), "Damage Report. ");
	for (i = 0; i < 8; i++) {
		if (dr[i].percent > 80) {
			ok++;
			continue;
		}
		snprintf(next_bit, sizeof(next_bit), "%s %d%%. ", dr[i].system, dr[i].percent);
		strcat(damage_report, next_bit);
	}
	if (ok == 8) {
		strcat(damage_report, "All systems within normal operating range.");
	} else {
		if (ok > 0) {
			strcat(damage_report, "Remaining systems are within normal operating range.");
		}
	}
	if (dr[6].percent < 20) {
		strcat(damage_report, " Suggest repairing sheilds immediately.");
	}
	queue_add_text_to_speech(c, damage_report);
	snprintf(damage_report, sizeof(damage_report), "Fuel tanks are at %2.0f percent.",
		100.0 * (float) o->tsd.ship.fuel / (float) UINT32_MAX);
	queue_add_text_to_speech(c, damage_report);
}

static void nl_launch_n(void *context, int argc, char *argv[], int pos[],
		__attribute__((unused)) union snis_nl_extra_data extra_data[])
{
	struct game_client *c = context;
	struct snis_entity *ship;
	uint32_t oid;
	int i, noun;

	noun = nl_find_next_word(argc, pos, POS_NOUN, 0);
	if (noun < 0)
		goto no_understand;
	if (strcasecmp(argv[noun], "robot") == 0) {
		pthread_mutex_lock(&universe_mutex);
		oid = bridgelist[c->bridge].science_selection;
		if (oid == 0xffffffff) {
			pthread_mutex_unlock(&universe_mutex);
			goto no_target;
		}
		i = lookup_by_id(c->shipid);
		pthread_mutex_unlock(&universe_mutex);
		if (i < 0) {
			queue_add_text_to_speech(c, "I'm sorry, I seem to have lost my train of thought");
			return;
		}
		ship = &go[i];
		launch_mining_bot(c, ship, oid);
		return;
	}

no_understand:
	queue_add_text_to_speech(c, "I'm sorry, launch what?");
	return;

no_target:
	queue_add_text_to_speech(c, "Sorry, no mining target selected.");
	return;
}

static void nl_set_reverse(struct game_client *c, int reverse, float value)
{
	int i;
	struct snis_entity *o;

	pthread_mutex_lock(&universe_mutex);
	i = lookup_by_id(c->shipid);
	if (i < 0) {
		pthread_mutex_unlock(&universe_mutex);
		queue_add_text_to_speech(c, "I lost my train of thought.");
		return;
	}
	o = &go[i];
	o->tsd.ship.reverse = reverse;
	pthread_mutex_unlock(&universe_mutex);
	nl_set_impulse_drive(c, "impulse drive", value);
}

/* full stop, full throttle, full reverse... */
static void nl_full_n(void *context, int argc, char *argv[], int pos[],
		__attribute__((unused)) union snis_nl_extra_data extra_data[])
{
	struct game_client *c = context;
	int noun, reverse = 0;
	float value = 1.0;

	noun = nl_find_next_word(argc, pos, POS_NOUN, 0);
	if (noun < 0)
		goto no_understand;
	if (strcasecmp(argv[noun], "impulse drive") == 0)
		goto full_throttle;
	if (strcasecmp(argv[noun], "power") == 0)
		goto full_throttle;
	if (strcasecmp(argv[noun], "speed") == 0)
		goto full_throttle;
	if (strcasecmp(argv[noun], "reverse") == 0) {
		reverse = 1;
		goto full_throttle;
	}
	if (strcasecmp(argv[noun], "stop") == 0) {
		value = 0.0;
		goto full_throttle;
	}

full_throttle: /* or full reverse, or full stop */
	nl_set_reverse(c, reverse, value);
	return;

no_understand:
	queue_add_text_to_speech(c, "I'm sorry, full what?");
}

/* full speed ahead */
static void nl_full_na(void *context, int argc, char *argv[], int pos[],
		__attribute__((unused)) union snis_nl_extra_data extra_data[])
{
	struct game_client *c = context;
	int adj, noun, reverse;

	noun = nl_find_next_word(argc, pos, POS_NOUN, 0);
	if (noun < 0)
		goto no_understand;
	adj = nl_find_next_word(argc, pos, POS_ADJECTIVE, 0);
	if (adj < 0)
		goto no_understand;
	if (strcasecmp(argv[noun], "speed") == 0 ||
		strcasecmp(argv[noun], "throttle") == 0 ||
		strcasecmp(argv[noun], "impulse-drive") == 0) {
		if (strcasecmp(argv[adj], "ahead") == 0 ||
			strcasecmp(argv[adj], "impulse") == 0) {
			reverse = 0;
		} else if (strcasecmp(argv[adj], "reverse") == 0 ||
				strcasecmp(argv[adj], "backwards") == 0 ||
				strcasecmp(argv[adj], "back") == 0) {
			reverse = 1;
		} else  {
			goto no_understand;
		}
		nl_set_reverse(c, reverse, 1.0);
		return;
	}

no_understand:
	queue_add_text_to_speech(c, "I am sorry, I did not understand that.");
}

static void sorry_dave(void *context, int argc, char *argv[], int pos[],
		__attribute__((unused)) union snis_nl_extra_data extra_data[])
{
	struct game_client *c = context;
	queue_add_text_to_speech(c, "I am sorry Dave, I am afraid I can't do that.");
}

static void natural_language_parse_failure(void *context)
{
	struct game_client *c = context;
	queue_add_text_to_speech(c, "I am sorry, I did not understand that.");
}

static inline void nl_decode_spaces(char *text)
{
	char *x;

	for (x = text; *x; x++)
		if (*x == '_')
			*x = ' ';
}

static inline void nl_encode_spaces(char *text, int len)
{
	int i;
	char *x;

	x = text;
	for (i = 0; i < len && *x; i++) {
		if (*x == ' ')
			*x = '_';
		x++;
	}
}

static void natural_language_multiword_preprocessor(char *text, int coding_direction)
{
	char *hit;
	int i;

	if (coding_direction == SNIS_NL_DECODE) {
		nl_decode_spaces(text);
		return;
	}
	if (coding_direction != SNIS_NL_ENCODE) {
		fprintf(stderr, "%s: Invalid coding direction\n", logprefix());
		return;
	}
	pthread_mutex_lock(&universe_mutex);
	for (i = 0; i <= snis_object_pool_highest_object(pool); i++) {
		char *name = nl_get_object_name(&go[i]);
		if (name[0] == '\0')
			continue;
		hit = strcasestr(text, name);
		if (hit)
			nl_encode_spaces(hit, strlen(name));
	}
	pthread_mutex_unlock(&universe_mutex);
}

static const struct nl_test_case_entry {
	char *text;
	int expected;
} nl_test_case[] = {
	{ "exit standard orbit", 0, },
	{ "leave standard orbit", 0, },
	{ "enter standard orbit", 0, },
	{ "standard orbit", 0, },
	{ "ramming speed", 0, },
	{ "ramming velocity", 0, },
	{ "shields up", 0, },
	{ "shields down", 0, },
	{ "set a course for the nearest planet", 0, },
	{ "activate warp drive", 0, },
	{ "engage warp drive", 0, },
	{ "actuate warp drive", 0, },
	{ "energize warp drive", 0, },
	{ "disengage docking system", 0, },
	{ "deactivate docking system", 0, },
	{ "energize docking system", 0, },
	{ "engage docking system", 0, },
	{ "actuate docking system", 0, },
	{ "increase power to impulse", 0, },
	{ "maximum power to impulse", 0, },
	{ "max coolant to tractor beam", 0, },
	{ "increase power to warp", 0, },
	{ "increase coolant to maneuvering", 0, },
	{ "increase coolant to sensors", 0, },
	{ "full stop", 0, },
	{ "full reverse", 0, },
	{ "full power", 0, },
	{ "full impulse", 0, },
	{ "full impulse drive", 0, },
	{ "full power to impulse drive", 0, },
	{ "full power to shields", 0, },
	{ "full power to warp drive", 0, },
	{ "full power to maneuvering", 0, },
	{ "full power to impulse", 0, },
	{ "full power to tractor beam", 0, },
	{ "full power to sensors", 0, },
	{ "full power to phasers", 0, },
	{ "full speed", 0, },
	{ "full speed ahead", 0, },
	{ "full speed reverse", 0, },
	{ "maximum speed ahead", 0, },
	{ "maximum speed reverse", 0, },
	{ "cut power to impulse drive", 0, },
	{ "cut power to impulse drive", 0, },
	{ "cut power to shields", 0, },
	{ "cut power to warp drive", 0, },
	{ "cut power to maneuvering", 0, },
	{ "cut power to impulse", 0, },
	{ "cut power to tractor beam", 0, },
	{ "cut power to sensors", 0, },
	{ "cut power to phasers", 0, },
	{ "set a course for the nearest planet", 0, },
	{ "plot a course for the nearest planet", 0, },
	{ "lay in a course for the nearest planet", 0, },
	{ "set a course for the nearest asteroid", 0, },
	{ "plot a course for the nearest asteroid", 0, },
	{ "lay in a course for the nearest asteroid", 0, },
	{ "set a course for the nearest star base", 0, },
	{ "plot a course for the nearest star base", 0, },
	{ "lay in a course for the nearest star base", 0, },
	{ "set a course for the nearest nebula", 0, },
	{ "plot a course for the nearest nebula", 0, },
	{ "lay in a course for the nearest nebula", 0, },
	{ "set a course for the nearest warp gate", 0, },
	{ "plot a course for the nearest warp gate", 0, },
	{ "lay in a course for the nearest warp gate", 0, },
	{ "set a course for the nearest worm hole", 0, },
	{ "plot a course for the nearest worm hole", 0, },
	{ "lay in a course for the nearest worm hole", 0, },
	{ "navigation on screen", 0, },
	{ "main view on screen", 0, },
	{ "weapons on screen", 0, },
	{ "communications on screen", 0, },
	{ "comms on screen", 0, },
	{ "engineering on screen", 0, },
	{ "science on screen", 0, },
	{ "navigation screen", 0, },
	{ "main view screen", 0, },
	{ "weapon screen", 0, },
	{ "communications screen", 0, },
	{ "comms screen", 0, },
	{ "engineering screen", 0, },
	{ "science screen", 0, },
	{ "full throttle", 0, },
	{ "set warp drive power to one quarter", 0, },
	{ "set warp drive coolant to 0.75", 0, },
	{ "set warp drive to 50", 0, },
	{ "set impulse drive power to 50 percent", 0, },
	{ "set impulse drive coolant to 50%", 0, },
	{ "set impulse drive to fifty five", 0, },
	{ "set throttle to forty two percent", 0, },
	{ "set maneuvering power to one quarter", 0, },
	{ "set maneuvering coolant to one hundred", 0, },
	{ "set sensor power to five", 0, },
	{ "set sensor coolant to maximum", 0, },
	{ "set tractor beam power to max", 0, },
	{ "set tractor beam coolant to 100", 0, },
	{ "set communications power to 100", 0, },
	{ "set communications coolant to 100", 0, },
	{ "set comms power to 100", 0, },
	{ "set comms coolant to 100", 0, },
	{ "set phaser power to 100", 0, },
	{ "set phaser coolant to 100", 0, },
	{ "set shields power to 100", 0, },
	{ "set shields coolant to 100", 0, },
	{ "set shields to 100", 0, },
	{ "raise shields", 0, },
	{ "lower shields", 0, },
	{ "calculate the distance to the nearest planet", 0, },
	{ "compute the distance to the nearest planet", 0, },
	{ "figure the distance to the nearest ship", 0, },
	{ "turn left 30 degrees", 0, },
	{ "turn right 30 degrees", 0, },
	{ "roll 30 degrees left", 0, },
	{ "roll 30 degrees right", 0, },
	{ "turn clockwise 20 degrees", 0, },
	{ "turn anti clockwise twenty degrees", 0, },
	{ "turn ten degrees counter clockwise", 0, },
	{ "turn up five degrees", 0, },
	{ "turn 200 degrees down", 0, },
	{ "pitch up twenty degrees", 0, },
	{ "pitch forty degrees down", 0, },
	{ "pitch forward 90 degrees", 0, },
	{ "pitch back ninety two degrees", 0, },
	{ "pitch backwards ninety two degrees", 0, },
	{ "turn towards the nearest asteroid", 0, },
	{ "damage report", 0, },
	{ "status report", 0, },
	{ "zoom one hundred", 0, },
	{ "zoom maximum", 0, },
	{ "maximum zoom", 0, },
	{ "minimum zoom", 0, },
	{ "zoom fifty", 0, },
	{ "zoom thirty", 0, },
	{ "zoom twenty", 0, },
	{ "zoom ninety", 0, },
	{ "zoom zero", 0, },
	{ "red alert", 0, },
	{ "cancel red alert", 0, },
	{ "stop red alert", 0, },
	{ "scan the nearest planet", 0, },
	{ "scan the nearest asteroid", 0, },
	{ "scan the nearest nebula", 0, },
	{ "scan the nearest ship", 0, },
	{ "scan the nearest derelict", 0, },
	{ "select the nearest planet", 0, },
	{ "select the nearest asteroid", 0, },
	{ "select the nearest nebula", 0, },
	{ "select the nearest ship", 0, },
	{ "select the nearest derelict", 0, },
	{ "scan the closest planet", 0, },
	{ "scan the closest asteroid", 0, },
	{ "scan the closest nebula", 0, },
	{ "scan the closest ship", 0, },
	{ "scan the closest derelict", 0, },
	{ "select the closest planet", 0, },
	{ "select the closest asteroid", 0, },
	{ "select the closest nebula", 0, },
	{ "select the closest ship", 0, },
	{ "select the closest derelict", 0, },
	{ "scan the nearby planet", 0, },
	{ "scan the nearby asteroid", 0, },
	{ "scan the nearby nebula", 0, },
	{ "scan the nearby ship", 0, },
	{ "scan the nearby derelict", 0, },
	{ "select the nearby planet", 0, },
	{ "select the nearby asteroid", 0, },
	{ "select the nearby nebula", 0, },
	{ "select the nearby ship", 0, },
	{ "select the nearby derelict", 0, },
	{ "scan the planet", 0, },
	{ "scan the nearest asteroid", 0, },
	{ "scan the nearest nebula", 0, },
	{ "scan the nearest ship", 0, },
	{ "scan the nearest derelict", 0, },
	{ "select the nearest planet", 0, },
	{ "select the nearest asteroid", 0, },
	{ "select the closest nebula", 0, },
	{ "select the nearby ship", 0, },
	{ "select the closest derelict", 0, },
	{ "launch the mining bot", 0, },
	{ "deploy the mining bot", 0, },
	{ "launch the mining robot", 0, },
	{ "long range scan", 0, },
	{ "long range scanner", 0, },
	{ "long range", 0, },
	{ "short range scan", 0, },
	{ "short range scanner", 0, },
	{ "short range", 0, },
	{ "deploy the mining robot", 0, },
	{ "eject the warp core", 0, },
	{ "minimum warp drive", 0, },
	{ "maximum warp drive", 0, },
	{ "maximum warp", 0, },
	{ "max coolant to warp drive", 0, },
	{ "minimum warp drive power", 0, },
	{ "increase warp drive to max", 0, },
	{ "decrease warp drive to minimum", 0, },
	{ "head for the starbase", 0, },
	{ "navigate towards the starbase", 0, },
	{ "navigate towards the starbase", 0, },
	{ "set a course for star base two", 0, },
	{ "set a course for warp gate three", 0, },
	{ "set a course for the warp gate", 0, },
};

static void nl_run_snis_test_cases(__attribute__((unused)) void *context,
		__attribute__((unused)) int argc,
		__attribute__((unused)) char *argv[], int pos[],
		__attribute__((unused)) union snis_nl_extra_data extra_data[])
{
	int i, rc;
	int passed = 0;
	int failed = 0;

	for (i = 0; i < ARRAYSIZE(nl_test_case); i++) {
		rc = snis_nl_test_parse_natural_language_request(NULL, nl_test_case[i].text);
		if (rc != nl_test_case[i].expected) {
			fprintf(stderr, "FAILED NATURAL LANGUAGE TEST: '%s' expected %s got %s\n",
				nl_test_case[i].text,
				nl_test_case[i].expected ? "NOT PARSEABLE" : "PARSEABLE",
				rc ? "NOT PARSEABLE" : "PARSEABLE");
			failed++;
		} else {
			passed++;
		}
	}
	fprintf(stderr, "NATURAL LANGUAGE TESTS FINISHED: PASSED = %d, FAILED = %d, TOTAL = %d\n",
			passed, failed, passed + failed);
	snis_nl_print_verbs_by_fn("Unimplemented verb: ", sorry_dave);
}

static void init_dictionary(void)
{
	snis_nl_add_dictionary_verb("run-snis-nl-test-cases", "run-snis-nl-test-cases", "", nl_run_snis_test_cases);
	snis_nl_add_dictionary_verb("describe",		"describe",	"n", nl_describe_n);
	snis_nl_add_dictionary_verb("describe",		"describe",	"an", nl_describe_an);
	snis_nl_add_dictionary_verb("navigate",		"navigate",	"pn", nl_navigate_pn);
	snis_nl_add_dictionary_verb("head",		"navigate",	"pn", nl_navigate_pn);
	snis_nl_add_dictionary_verb("navigate",		"navigate",	"pnq", nl_navigate_pnq);
	snis_nl_add_dictionary_verb("head",		"navigate",	"pnq", nl_navigate_pnq);
	snis_nl_add_dictionary_verb("set",		"set",		"npq", nl_set_npq); /* set warp drive to 50 percent */
	snis_nl_add_dictionary_verb("set",		"set",		"npa", nl_set_npa);
	snis_nl_add_dictionary_verb("set",		"set",		"npn", nl_set_npn);
	snis_nl_add_dictionary_verb("set",		"set",		"npan", nl_set_npn); /* set a course for the nearest planet */
	snis_nl_add_dictionary_verb("set",		"set",		"npnq", nl_set_npnq); /* set a course for starbase one */
	snis_nl_add_dictionary_verb("plot",		"plot",		"npn", nl_set_npn);
	snis_nl_add_dictionary_verb("plot",		"plot",		"npan", nl_set_npn);
	snis_nl_add_dictionary_verb("plot",		"plot",		"npnq", nl_set_npnq);
	snis_nl_add_dictionary_verb("lay in",		"lay in",	"npn", nl_set_npn);
	snis_nl_add_dictionary_verb("lay in",		"lay in",	"npan", nl_set_npn);
	snis_nl_add_dictionary_verb("lay in",		"lay in",	"npnq", nl_set_npnq);
	snis_nl_add_dictionary_verb("lower",		"lower",	"npq", nl_set_npq);
	snis_nl_add_dictionary_verb("lower",		"lower",	"npn", nl_set_npn);
	snis_nl_add_dictionary_verb("lower",		"lower",	"npa", nl_lower_npa); /* lower power to impulse */
	snis_nl_add_dictionary_verb("lower",		"lower",	"n", nl_lower_n); /* decrease warp drive */
	snis_nl_add_dictionary_verb("minimum",		"lower",	"n", nl_lower_n); /* minimum warp drive */
	snis_nl_add_dictionary_verb("raise",		"raise",	"npq", nl_set_npq); /* raise warp drive to 100 */
	snis_nl_add_dictionary_verb("raise",		"raise",	"npn", sorry_dave);
	snis_nl_add_dictionary_verb("raise",		"raise",	"npa", nl_raise_npa); /* increase power to impulse, raise warp to max */
	snis_nl_add_dictionary_verb("maximum",		"raise",	"npa", nl_raise_npa); /* max power to impulse, max coolant to warp */
	snis_nl_add_dictionary_verb("raise",		"raise",	"n", nl_raise_n);	/* increase warp drive */
	snis_nl_add_dictionary_verb("maximum",		"raise",	"n", nl_raise_n);	/* maximum warp drive */
	snis_nl_add_dictionary_verb("shields",		"shields",	"p", nl_shields_p);	/* shields up/down */
	snis_nl_add_dictionary_verb("maximum",		"raise",	"n", nl_raise_n);	/* max warp */
	snis_nl_add_dictionary_verb("maximum",		"raise",	"npa", nl_raise_npa); /* max power to impulse */
	snis_nl_add_dictionary_verb("engage",		"engage",	"n", nl_engage_n);
	snis_nl_add_dictionary_verb("engage",		"engage",	"npn", nl_engage_npn);
	snis_nl_add_dictionary_verb("leave",		"leave",	"n", nl_leave_n); /* leave orbit */
	snis_nl_add_dictionary_verb("leave",		"leave",	"an", nl_leave_an); /* leave standard orbit */
	snis_nl_add_dictionary_verb("standard",		"enter",	"n", nl_enter_n); /* enter orbit */
	snis_nl_add_dictionary_verb("enter",		"enter",	"n", nl_enter_n); /* enter orbit */
	snis_nl_add_dictionary_verb("enter",		"enter",	"an", nl_enter_an); /* enter standard orbit */
	snis_nl_add_dictionary_verb("disengage",	"disengage",	"n", nl_disengage_n);
	snis_nl_add_dictionary_verb("stop",		"disengage",	"n", nl_disengage_n);
	snis_nl_add_dictionary_verb("turn",		"turn",		"pn", nl_turn_pn); /* turn on lights */
	snis_nl_add_dictionary_verb("turn",		"turn",		"np", nl_turn_np); /* turn lights on */
	snis_nl_add_dictionary_verb("shut",		"shut",		"pn", nl_turn_pn); /* shut off lights */
	snis_nl_add_dictionary_verb("shut",		"shut",		"np", nl_turn_np); /* shut lights off */
	snis_nl_add_dictionary_verb("turn",		"turn",		"aqa", nl_turn_aqa);
	snis_nl_add_dictionary_verb("rotate",		"rotate",	"aqa", nl_turn_aqa);
	snis_nl_add_dictionary_verb("turn",		"turn",		"qaa", nl_turn_qaa);
	snis_nl_add_dictionary_verb("rotate",		"rotate",	"qaa", nl_turn_qaa);
	snis_nl_add_dictionary_verb("turn",		"turn",		"qa", nl_turn_qa);
	snis_nl_add_dictionary_verb("rotate",		"rotate",	"qa", nl_turn_qa);
	snis_nl_add_dictionary_verb("turn",		"turn",		"aq", nl_turn_aq);
	snis_nl_add_dictionary_verb("rotate",		"rotate",	"aq", nl_turn_aq);
	snis_nl_add_dictionary_verb("compute",		"compute",	"npn", nl_compute_npn);
	snis_nl_add_dictionary_verb("damage report",	"damage report", "", nl_damage_report);
	snis_nl_add_dictionary_verb("report damage",	"damage report", "", nl_damage_report);
	snis_nl_add_dictionary_verb("status report",	"damage report", "", nl_damage_report);
	snis_nl_add_dictionary_verb("report status",	"damage report", "", nl_damage_report);
	snis_nl_add_dictionary_verb("yaw",		"yaw",		"aqa", nl_turn_aqa);
	snis_nl_add_dictionary_verb("pitch",		"pitch",	"aqa", nl_turn_aqa);
	snis_nl_add_dictionary_verb("roll",		"roll",		"aqa", nl_turn_aqa);
	snis_nl_add_dictionary_verb("yaw",		"yaw",		"qaa", nl_turn_qaa);
	snis_nl_add_dictionary_verb("pitch",		"pitch",	"qaa", nl_turn_qaa);
	snis_nl_add_dictionary_verb("roll",		"roll",		"qaa", nl_turn_qaa);
	snis_nl_add_dictionary_verb("zoom",		"zoom",		"", nl_zoom);
	snis_nl_add_dictionary_verb("zoom",		"zoom",		"q", nl_zoom_q);
	snis_nl_add_dictionary_verb("zoom",		"zoom",		"p", nl_zoom_p);
	snis_nl_add_dictionary_verb("zoom",		"zoom",		"pq", nl_zoom_pq);
	snis_nl_add_dictionary_verb("magnify",		"zoom",		"", nl_zoom);
	snis_nl_add_dictionary_verb("magnify",		"zoom",		"q", nl_zoom_q);
	snis_nl_add_dictionary_verb("magnify",		"zoom",		"p", nl_zoom_p);
	snis_nl_add_dictionary_verb("magnify",		"zoom",		"pq", nl_zoom_pq);
	snis_nl_add_dictionary_verb("launch",		"launch",	"n", nl_launch_n);
	snis_nl_add_dictionary_verb("eject",		"eject",	"n", sorry_dave);
	snis_nl_add_dictionary_verb("full",		"full",		"a", nl_full_n),    /* full impulse */
	snis_nl_add_dictionary_verb("full",		"full",		"n", nl_full_n),    /* full impulse drive */
	snis_nl_add_dictionary_verb("ramming",		"full",		"n", nl_full_n),    /* ramming speed */
	snis_nl_add_dictionary_verb("full",		"full",		"npn", sorry_dave), /* full power to impulse drive */
	snis_nl_add_dictionary_verb("full",		"full",		"npa", nl_raise_npa), /* full power to impulse */
	snis_nl_add_dictionary_verb("full",		"full",		"na", nl_full_na), /* full speed ahead */
	snis_nl_add_dictionary_verb("maximum",		"full",		"na", nl_full_na), /* maximum speed ahead */
	snis_nl_add_dictionary_verb("red alert",	"red alert",	"", nl_red_alert);
	snis_nl_add_dictionary_verb("red alert",	"red alert",	"p", nl_red_alert_p);
	snis_nl_add_dictionary_verb("main view",	"main view",	"pn", nl_onscreen_verb_pn);
	snis_nl_add_dictionary_verb("navigation",	"navigation",	"pn", nl_onscreen_verb_pn);
	snis_nl_add_dictionary_verb("weapons",		"weapons",	"pn", nl_onscreen_verb_pn);
	snis_nl_add_dictionary_verb("weapon",		"weapons",	"pn", nl_onscreen_verb_pn);
	snis_nl_add_dictionary_verb("engineering",	"engineering",	"pn", nl_onscreen_verb_pn);
	snis_nl_add_dictionary_verb("science",		"science",	"pn", nl_onscreen_verb_pn);
	snis_nl_add_dictionary_verb("communications",	"communications", "pn", nl_onscreen_verb_pn);
	snis_nl_add_dictionary_verb("main view",	"main view",	"n", nl_onscreen_verb_n);
	snis_nl_add_dictionary_verb("navigation",	"navigation",	"n", nl_onscreen_verb_n);
	snis_nl_add_dictionary_verb("weapons",		"weapons",	"n", nl_onscreen_verb_n);
	snis_nl_add_dictionary_verb("weapon",		"weapons",	"n", nl_onscreen_verb_n);
	snis_nl_add_dictionary_verb("engineering",	"engineering",	"n", nl_onscreen_verb_n);
	snis_nl_add_dictionary_verb("science",		"science",	"n", nl_onscreen_verb_n);
	snis_nl_add_dictionary_verb("communications",	"communications", "n", nl_onscreen_verb_n);
	snis_nl_add_dictionary_verb("target",		"target",	"n", nl_target_n);
	snis_nl_add_dictionary_verb("target",		"target",	"nq", nl_target_nq);
	snis_nl_add_dictionary_verb("scan",		"target",	"n", nl_target_n);
	snis_nl_add_dictionary_verb("scan",		"target",	"nq", nl_target_nq);
	snis_nl_add_dictionary_verb("select",		"target",	"n", nl_target_n);
	snis_nl_add_dictionary_verb("select",		"target",	"nq", nl_target_nq);
	snis_nl_add_dictionary_verb("reverse",		"reverse",	"n", nl_reverse_n);
	snis_nl_add_dictionary_verb("long range scanner",	"long range scan",	"", nl_shortlong_range_scan);
	snis_nl_add_dictionary_verb("long range scan",	"long range scan",	"", nl_shortlong_range_scan);
	snis_nl_add_dictionary_verb("long range",	"long range scan",	"", nl_shortlong_range_scan);
	snis_nl_add_dictionary_verb("short range scanner",	"short range scan",	"", nl_shortlong_range_scan);
	snis_nl_add_dictionary_verb("short range scan",	"short range scan",	"", nl_shortlong_range_scan);
	snis_nl_add_dictionary_verb("short range",	"short range scan",	"", nl_shortlong_range_scan);
	snis_nl_add_dictionary_verb("details",		"details",		"", nl_shortlong_range_scan);
	snis_nl_add_dictionary_verb("detail",		"details",		"", nl_shortlong_range_scan);
	snis_nl_add_dictionary_verb("help",		"help",			"", nl_help);
	snis_nl_add_dictionary_verb("what is",		"what is",		"n", nl_what_is_n);
	snis_nl_add_dictionary_verb("what is",		"what is",		"npn", nl_what_is_npn);
	snis_nl_add_dictionary_verb("what is",		"what is",		"npn", nl_what_is_anpan);
	snis_nl_add_dictionary_verb("how",		"how",			"apn", nl_how_apn);
	snis_nl_add_dictionary_verb("how",		"how",			"an", nl_how_an);
		/* how much fuel do we have */
	snis_nl_add_dictionary_verb("how",		"how",			"anxPx", nl_how_anxPx);
	snis_nl_add_dictionary_verb("african",		"african",		"", nl_african_or_european);
	snis_nl_add_dictionary_verb("european",		"european",		"", nl_african_or_european);
		/* do we have enough fuel */
	snis_nl_add_dictionary_verb("do",		"do",			"Pxan", nl_do_Pxan);
		/* lights on/off/out */
	snis_nl_add_dictionary_verb("lights",		"lights",		"p",	nl_lights_p);
	snis_nl_add_dictionary_verb("repeat",		"repeat",		"n",	nl_repeat_n);
	snis_nl_add_dictionary_verb("repeat",		"repeat",		"",	nl_repeat_n);
	snis_nl_add_dictionary_verb("what",		"repeat",		"",	nl_repeat_n);
	snis_nl_add_dictionary_verb("huh",		"repeat",		"",	nl_repeat_n);

	snis_nl_add_dictionary_word("drive",		"drive",	POS_NOUN);
	snis_nl_add_dictionary_word("system",		"system",	POS_NOUN);
	snis_nl_add_dictionary_word("starbase",		"starbase",	POS_NOUN);
	snis_nl_add_dictionary_word("station",		"starbase",	POS_NOUN);
	snis_nl_add_dictionary_word("base",		"starbase",	POS_NOUN);
	snis_nl_add_dictionary_word("planet",		"planet",	POS_NOUN);
	snis_nl_add_dictionary_word("ship",		"ship",		POS_NOUN);
	snis_nl_add_dictionary_word("bot",		"robot",	POS_NOUN);
	snis_nl_add_dictionary_word("shields",		"shields",	POS_NOUN);
	snis_nl_add_dictionary_word("factor",		"factor",	POS_NOUN);
	snis_nl_add_dictionary_word("level",		"level",	POS_NOUN);
	snis_nl_add_dictionary_word("worm hole",	"worm hole",	POS_NOUN);

	snis_nl_add_dictionary_word("maneuvering power", "maneuvering power",	POS_NOUN);
	snis_nl_add_dictionary_word("warp drive power", "warp drive power",	POS_NOUN);
	snis_nl_add_dictionary_word("impulse drive power", "impulse drive power",	POS_NOUN);
	snis_nl_add_dictionary_word("sensor power", "sensor power",	POS_NOUN);
	snis_nl_add_dictionary_word("sensors power", "sensor power",	POS_NOUN);
	snis_nl_add_dictionary_word("communications power", "communications power",	POS_NOUN);
	snis_nl_add_dictionary_word("phaser power", "phaser power",	POS_NOUN);
	snis_nl_add_dictionary_word("weapons power", "phaser power",	POS_NOUN);
	snis_nl_add_dictionary_word("weapon power", "phaser power",	POS_NOUN);
	snis_nl_add_dictionary_word("shield power", "shield power",	POS_NOUN);
	snis_nl_add_dictionary_word("shields power", "shield power",	POS_NOUN);
	snis_nl_add_dictionary_word("tractor beam power", "tractor beam power",	POS_NOUN);
	snis_nl_add_dictionary_word("life support power", "life support power",	POS_NOUN);

	snis_nl_add_dictionary_word("maneuvering coolant", "maneuvering coolant",	POS_NOUN);
	snis_nl_add_dictionary_word("warp drive coolant", "warp drive coolant",	POS_NOUN);
	snis_nl_add_dictionary_word("impulse drive coolant", "impulse drive coolant",	POS_NOUN);
	snis_nl_add_dictionary_word("sensor coolant", "sensor coolant",	POS_NOUN);
	snis_nl_add_dictionary_word("sensors coolant", "sensor coolant",	POS_NOUN);
	snis_nl_add_dictionary_word("communications coolant", "communications coolant",	POS_NOUN);
	snis_nl_add_dictionary_word("phaser coolant", "phaser coolant",	POS_NOUN);
	snis_nl_add_dictionary_word("weapons coolant", "phaser coolant",	POS_NOUN);
	snis_nl_add_dictionary_word("weapon coolant", "phaser coolant",	POS_NOUN);
	snis_nl_add_dictionary_word("shield coolant", "shield coolant",	POS_NOUN);
	snis_nl_add_dictionary_word("shields coolant", "shield coolant",	POS_NOUN);
	snis_nl_add_dictionary_word("tractor beam coolant", "tractor beam coolant",	POS_NOUN);
	snis_nl_add_dictionary_word("life support coolant", "life support coolant",	POS_NOUN);
	snis_nl_add_dictionary_word("zoom",		"zoom",		POS_NOUN);

	snis_nl_add_dictionary_word("tractor beam", "tractor beam",	POS_NOUN);
	snis_nl_add_dictionary_word("life support", "life support",	POS_NOUN);
	snis_nl_add_dictionary_word("docking system", "docking system",	POS_NOUN);
	snis_nl_add_dictionary_word("coolant",		"coolant",	POS_NOUN);
	snis_nl_add_dictionary_word("power",		"power",	POS_NOUN);
	snis_nl_add_dictionary_word("stop",		"stop",		POS_NOUN); /* as in:full stop */
	snis_nl_add_dictionary_word("speed",		"speed",	POS_NOUN);
	snis_nl_add_dictionary_word("reverse",		"reverse",	POS_NOUN);
	snis_nl_add_dictionary_word("impulse drive",	"impulse drive",	POS_NOUN);
	snis_nl_add_dictionary_word("warp drive",	"warp drive",	POS_NOUN);
	snis_nl_add_dictionary_word("asteroid",		"asteroid",	POS_NOUN);
	snis_nl_add_dictionary_word("nebula",		"nebula",	POS_NOUN);
	snis_nl_add_dictionary_word("star",		"star",		POS_NOUN);
	snis_nl_add_dictionary_word("range",		"range",	POS_NOUN);
	snis_nl_add_dictionary_word("distance",		"range",	POS_NOUN);
	snis_nl_add_dictionary_word("weapons",		"weapons",	POS_NOUN);
	snis_nl_add_dictionary_word("screen",		"screen",	POS_NOUN);
	snis_nl_add_dictionary_word("robot",		"robot",	POS_NOUN);
	snis_nl_add_dictionary_word("torpedo",		"torpedo",	POS_NOUN);
	snis_nl_add_dictionary_word("phasers",		"phasers",	POS_NOUN);
	snis_nl_add_dictionary_word("weapons",		"phasers",	POS_NOUN);
	snis_nl_add_dictionary_word("weapon",		"phasers",	POS_NOUN);
	snis_nl_add_dictionary_word("maneuvering",	"maneuvering",	POS_NOUN);
	snis_nl_add_dictionary_word("thruster",		"thrusters",	POS_NOUN);
	snis_nl_add_dictionary_word("thrusters",	"thrusters",	POS_NOUN);
	snis_nl_add_dictionary_word("polarity",		"polarity",	POS_NOUN);
	snis_nl_add_dictionary_word("sensors",		"sensors",	POS_NOUN);
	snis_nl_add_dictionary_word("sensor",		"sensors",	POS_NOUN);
	snis_nl_add_dictionary_word("science",		"science",	POS_NOUN);
	snis_nl_add_dictionary_word("communications",	"communications", POS_NOUN);
	snis_nl_add_dictionary_word("enemy",		"enemy",	POS_NOUN);
	snis_nl_add_dictionary_word("derelict",		"derelict",	POS_NOUN);
	snis_nl_add_dictionary_word("computer",		"computer",	POS_NOUN);
	snis_nl_add_dictionary_word("fuel",		"fuel",		POS_NOUN);
	snis_nl_add_dictionary_word("radiation",	"radiation",	POS_NOUN);
	snis_nl_add_dictionary_word("wavelength",	"wavelength",	POS_NOUN);
	snis_nl_add_dictionary_word("charge",		"charge",	POS_NOUN);
	snis_nl_add_dictionary_word("magnets",		"magnets",	POS_NOUN);
	snis_nl_add_dictionary_word("gate",		"gate",		POS_NOUN);
	snis_nl_add_dictionary_word("percent",		"percent",	POS_NOUN);
	snis_nl_add_dictionary_word("sequence",		"sequence",	POS_NOUN);
	snis_nl_add_dictionary_word("core",		"core",		POS_NOUN);
	snis_nl_add_dictionary_word("code",		"code",		POS_NOUN);
	snis_nl_add_dictionary_word("hull",		"hull",		POS_NOUN);
	snis_nl_add_dictionary_word("scanner",		"scanner",	POS_NOUN);
	snis_nl_add_dictionary_word("scanners",		"scanners",	POS_NOUN);
	snis_nl_add_dictionary_word("detail",		"details",	POS_NOUN);
	snis_nl_add_dictionary_word("report",		"report",	POS_NOUN);
	snis_nl_add_dictionary_word("damage",		"damage",	POS_NOUN);
	snis_nl_add_dictionary_word("course",		"course",	POS_NOUN);
	snis_nl_add_dictionary_word("distance",		"distance",	POS_NOUN);
	snis_nl_add_dictionary_word("red alert",	"red alert",	POS_NOUN);
	snis_nl_add_dictionary_word("orbit",		"orbit",	POS_NOUN);
	snis_nl_add_dictionary_word("target",		"selection",	POS_NOUN);
	snis_nl_add_dictionary_word("selection",	"selection",	POS_NOUN);
	snis_nl_add_dictionary_word("swallow",		"swallow",	POS_NOUN);
	snis_nl_add_dictionary_word("waypoint",		"waypoint",	POS_NOUN);
	snis_nl_add_dictionary_word("blackhole",	"blackhole",	POS_NOUN);
	snis_nl_add_dictionary_word("spacemonster",	"spacemonster",	POS_NOUN);
	snis_nl_add_dictionary_word("lights",		"lights",	POS_NOUN);
	snis_nl_add_dictionary_word("volume",		"volume",	POS_NOUN);

	snis_nl_add_dictionary_word("a",		"a",		POS_ARTICLE);
	snis_nl_add_dictionary_word("an",		"an",		POS_ARTICLE);
	snis_nl_add_dictionary_word("the",		"the",		POS_ARTICLE);

	snis_nl_add_dictionary_word("above",		"above",	POS_PREPOSITION);
	snis_nl_add_dictionary_word("aboard",		"aboard",	POS_PREPOSITION);
	snis_nl_add_dictionary_word("across",		"across",	POS_PREPOSITION);
	snis_nl_add_dictionary_word("after",		"after",	POS_PREPOSITION);
	snis_nl_add_dictionary_word("along",		"along",	POS_PREPOSITION);
	snis_nl_add_dictionary_word("alongside",	"alongside",	POS_PREPOSITION);
	snis_nl_add_dictionary_word("at",		"at",		POS_PREPOSITION);
	snis_nl_add_dictionary_word("atop",		"atop",		POS_PREPOSITION);
	snis_nl_add_dictionary_word("around",		"around",	POS_PREPOSITION);
	snis_nl_add_dictionary_word("before",		"before",	POS_PREPOSITION);
	snis_nl_add_dictionary_word("behind",		"behind",	POS_PREPOSITION);
	snis_nl_add_dictionary_word("beneath",		"beneath",	POS_PREPOSITION);
	snis_nl_add_dictionary_word("below",		"below",	POS_PREPOSITION);
	snis_nl_add_dictionary_word("beside",		"beside",	POS_PREPOSITION);
	snis_nl_add_dictionary_word("besides",		"besides",	POS_PREPOSITION);
	snis_nl_add_dictionary_word("between",		"between",	POS_PREPOSITION);
	snis_nl_add_dictionary_word("by",		"by",		POS_PREPOSITION);
	snis_nl_add_dictionary_word("down",		"down",		POS_PREPOSITION);
	snis_nl_add_dictionary_word("during",		"during",	POS_PREPOSITION);
	snis_nl_add_dictionary_word("except",		"except",	POS_PREPOSITION);
	snis_nl_add_dictionary_word("for",		"for",		POS_PREPOSITION);
	snis_nl_add_dictionary_word("from",		"from",		POS_PREPOSITION);
	snis_nl_add_dictionary_word("in",		"in",		POS_PREPOSITION);
	snis_nl_add_dictionary_word("inside",		"inside",	POS_PREPOSITION);
	snis_nl_add_dictionary_word("of",		"of",		POS_PREPOSITION);
	snis_nl_add_dictionary_word("off",		"off",		POS_PREPOSITION);
	snis_nl_add_dictionary_word("on",		"on",		POS_PREPOSITION);
	snis_nl_add_dictionary_word("onto",		"onto",		POS_PREPOSITION);
	snis_nl_add_dictionary_word("out",		"out",		POS_PREPOSITION);
	snis_nl_add_dictionary_word("outside",		"outside",	POS_PREPOSITION);
	snis_nl_add_dictionary_word("through",		"through",	POS_PREPOSITION);
	snis_nl_add_dictionary_word("throughout",	"throughout",	POS_PREPOSITION);
	snis_nl_add_dictionary_word("to",		"to",		POS_PREPOSITION);
	snis_nl_add_dictionary_word("toward",		"toward",	POS_PREPOSITION);
	snis_nl_add_dictionary_word("under",		"under",	POS_PREPOSITION);
	snis_nl_add_dictionary_word("up",		"up",		POS_PREPOSITION);
	snis_nl_add_dictionary_word("until",		"until",	POS_PREPOSITION);
	snis_nl_add_dictionary_word("with",		"with",		POS_PREPOSITION);
	snis_nl_add_dictionary_word("within",		"within",	POS_PREPOSITION);
	snis_nl_add_dictionary_word("without",		"without",	POS_PREPOSITION);
	/* bit of a hack for speech recognition to handle "two" vs. "to" */
	snis_nl_add_dictionary_word("2",		"to",		POS_PREPOSITION);

	snis_nl_add_dictionary_word("or",		"or",		POS_SEPARATOR);
	snis_nl_add_dictionary_word("and",		"and",		POS_SEPARATOR);
	snis_nl_add_dictionary_word("then",		"then",		POS_SEPARATOR);
	snis_nl_add_dictionary_word(",",		",",		POS_SEPARATOR);
	snis_nl_add_dictionary_word(".",		".",		POS_SEPARATOR);
	snis_nl_add_dictionary_word(";",		";",		POS_SEPARATOR);
	snis_nl_add_dictionary_word("!",		"!",		POS_SEPARATOR);
	snis_nl_add_dictionary_word("?",		"?",		POS_SEPARATOR);

	snis_nl_add_dictionary_word("damage",		"damage",	POS_ADJECTIVE);
	snis_nl_add_dictionary_word("status",		"status",	POS_ADJECTIVE);
	snis_nl_add_dictionary_word("warp",		"warp",		POS_ADJECTIVE);
	snis_nl_add_dictionary_word("impulse",		"impulse",	POS_ADJECTIVE);
	snis_nl_add_dictionary_word("docking",		"docking",	POS_ADJECTIVE);
	snis_nl_add_dictionary_word("star",		"star",		POS_ADJECTIVE);
	snis_nl_add_dictionary_word("space",		"space",	POS_ADJECTIVE);
	snis_nl_add_dictionary_word("mining",		"mining",	POS_ADJECTIVE);
	snis_nl_add_dictionary_word("energy",		"energy",	POS_ADJECTIVE);
	snis_nl_add_dictionary_word("main",		"main",		POS_ADJECTIVE);
	snis_nl_add_dictionary_word("navigation",	"navigation",	POS_ADJECTIVE);
	snis_nl_add_dictionary_word("comms",		"comms",	POS_ADJECTIVE);
	snis_nl_add_dictionary_word("engineering",	"engineering",	POS_ADJECTIVE);
	snis_nl_add_dictionary_word("science",		"science",	POS_ADJECTIVE);
	snis_nl_add_dictionary_word("enemy",		"enemy",	POS_ADJECTIVE);
	snis_nl_add_dictionary_word("derelict",		"derelict",	POS_ADJECTIVE);
	snis_nl_add_dictionary_word("solar",		"solar",	POS_ADJECTIVE);
	snis_nl_add_dictionary_word("nearest",		"nearest",	POS_ADJECTIVE);
	snis_nl_add_dictionary_word("selected",		"selected",	POS_ADJECTIVE);
	snis_nl_add_dictionary_word("closest",		"nearest",	POS_ADJECTIVE);
	snis_nl_add_dictionary_word("nearby",		"nearest",	POS_ADJECTIVE);
	snis_nl_add_dictionary_word("close",		"nearest",	POS_ADJECTIVE);
	snis_nl_add_dictionary_word("up",		"up",		POS_ADJECTIVE);
	snis_nl_add_dictionary_word("down",		"down",		POS_ADJECTIVE);
	snis_nl_add_dictionary_word("port",		"port",		POS_ADJECTIVE);
	snis_nl_add_dictionary_word("left",		"left",		POS_ADJECTIVE);
	snis_nl_add_dictionary_word("starboard",	"starboard",	POS_ADJECTIVE);
	snis_nl_add_dictionary_word("right",		"right",	POS_ADJECTIVE);
	snis_nl_add_dictionary_word("clockwise",	"clockwise",	POS_ADJECTIVE);
	snis_nl_add_dictionary_word("counterclockwise",	"counterclockwise",	POS_ADJECTIVE);
	snis_nl_add_dictionary_word("degrees",		"degrees",	POS_ADJECTIVE);
	snis_nl_add_dictionary_word("self",		"self",		POS_ADJECTIVE);
	snis_nl_add_dictionary_word("destruct",		"destruct",	POS_ADJECTIVE);
	snis_nl_add_dictionary_word("self-destruct",	"self-destruct",	POS_ADJECTIVE);
	snis_nl_add_dictionary_word("short",		"short",	POS_ADJECTIVE);
	snis_nl_add_dictionary_word("long",		"long",		POS_ADJECTIVE);
	snis_nl_add_dictionary_word("range",		"range",	POS_ADJECTIVE);
	snis_nl_add_dictionary_word("full",		"maximum",	POS_ADJECTIVE);
	snis_nl_add_dictionary_word("maximum",		"maximum",	POS_ADJECTIVE);
	snis_nl_add_dictionary_word("minimum",		"minimum",	POS_ADJECTIVE);
	snis_nl_add_dictionary_word("planet",		"planet",	POS_ADJECTIVE);
	snis_nl_add_dictionary_word("ahead",		"ahead",	POS_ADJECTIVE);
	snis_nl_add_dictionary_word("reverse",		"reverse",	POS_ADJECTIVE);
	snis_nl_add_dictionary_word("forward",		"forward",	POS_ADJECTIVE);
	snis_nl_add_dictionary_word("backwards",	"backwards",	POS_ADJECTIVE);
	snis_nl_add_dictionary_word("backward",		"backwards",	POS_ADJECTIVE);
	snis_nl_add_dictionary_word("back",		"backwards",	POS_ADJECTIVE);
	snis_nl_add_dictionary_word("standard",		"standard",	POS_ADJECTIVE);
	snis_nl_add_dictionary_word("far",		"far",		POS_ADJECTIVE);
	snis_nl_add_dictionary_word("airspeed",		"airspeed",	POS_ADJECTIVE);
	snis_nl_add_dictionary_word("unladen",		"unladen",	POS_ADJECTIVE);
	snis_nl_add_dictionary_word("african",		"african",	POS_ADJECTIVE);
	snis_nl_add_dictionary_word("european",		"european",	POS_ADJECTIVE);
	snis_nl_add_dictionary_word("much",		"much",		POS_ADJECTIVE);
	snis_nl_add_dictionary_word("enough",		"enough",	POS_ADJECTIVE);
	snis_nl_add_dictionary_word("sufficient",	"enough",	POS_ADJECTIVE);
	snis_nl_add_dictionary_word("exterior",		"exterior",	POS_ADJECTIVE);
	snis_nl_add_dictionary_word("external",		"external",	POS_ADJECTIVE);

	snis_nl_add_dictionary_word("percent",		"percent",	POS_ADVERB);
	snis_nl_add_dictionary_word("quickly",		"quickly",	POS_ADVERB);
	snis_nl_add_dictionary_word("rapidly",		"quickly",	POS_ADVERB);
	snis_nl_add_dictionary_word("swiftly",		"quickly",	POS_ADVERB);
	snis_nl_add_dictionary_word("slowly",		"slowly",	POS_ADVERB);

	snis_nl_add_dictionary_word("it",		"it",		POS_PRONOUN);
	snis_nl_add_dictionary_word("me",		"me",		POS_PRONOUN);
	snis_nl_add_dictionary_word("we",		"we",		POS_PRONOUN);
	snis_nl_add_dictionary_word("them",		"them",		POS_PRONOUN);
	snis_nl_add_dictionary_word("all",		"all",		POS_PRONOUN);
	snis_nl_add_dictionary_word("everything",	"everything",	POS_PRONOUN);
	snis_nl_add_dictionary_word("that",		"that",		POS_PRONOUN);

	snis_nl_add_dictionary_word("do",		"do",		POS_AUXVERB);
	snis_nl_add_dictionary_word("be",		"be",		POS_AUXVERB);
	snis_nl_add_dictionary_word("have",		"have",		POS_AUXVERB);
	snis_nl_add_dictionary_word("will",		"will",		POS_AUXVERB);
	snis_nl_add_dictionary_word("shall",		"shall",	POS_AUXVERB);
	snis_nl_add_dictionary_word("would",		"would",	POS_AUXVERB);
	snis_nl_add_dictionary_word("could",		"could",	POS_AUXVERB);
	snis_nl_add_dictionary_word("should",		"should",	POS_AUXVERB);
	snis_nl_add_dictionary_word("can",		"can",		POS_AUXVERB);
	snis_nl_add_dictionary_word("may",		"may",		POS_AUXVERB);
	snis_nl_add_dictionary_word("must",		"must",		POS_AUXVERB);
	snis_nl_add_dictionary_word("ought",		"ought",	POS_AUXVERB);

}

static void init_natural_language_system(void)
{
	init_synonyms();
	init_dictionary();
	snis_nl_add_error_function(natural_language_parse_failure);
	snis_nl_add_external_lookup(natural_language_object_lookup);
	snis_nl_add_multiword_preprocessor(natural_language_multiword_preprocessor);
}

/*****************************************************************************************
 * Here ends the natural language parsing code.
 *****************************************************************************************/

static struct option long_options[] = {
	{ "enable-enscript", no_argument, NULL, 'e' },
	{ "initscript", required_argument, NULL, 'i' },
	{ "help", no_argument, NULL, 'h' },
	{ "lobbyhost", required_argument, NULL, 'l' },
	{ "servernick", required_argument, NULL, 'n' },
	{ "location", required_argument, NULL, 'L' },
	{ "multiverse", required_argument, NULL, 'm' },
	{ "solarsystem", required_argument, NULL, 's' },
	{ "version", no_argument, NULL, 'v' },
	{ "trap-nans", no_argument, NULL, 't' },
	{ 0, 0, 0, 0 },
};

static char *default_lobbyhost = "localhost";
static char *default_lobby_servernick = "-";

static char lobby_gameinstance[GAMEINSTANCESIZE];
static char *lobbyhost = NULL;
static char *lobby_location = NULL;
static char *lobby_servernick = NULL;
static char snis_server_lockfile[PATH_MAX] = { 0 };

/* Clean up the lock file on SIGTERM */
static void sigterm_handler(int sig, siginfo_t *siginfo, void *context)
{
	static const char buffer[] = "snis_server: Received SIGTERM, exiting.\n";
	int rc;

	if (snis_server_lockfile[0] != '\0')
		(void) rmdir(snis_server_lockfile);
	/* Need to check return value of write() here even though we can't do
	 * anything with that info at this point, otherwise the compiler complains
	 * if we just ignore write() return value.
	 */
	rc = write(2, buffer, sizeof(buffer));  /* We're assuming 2 is still hooked to stderr */
	if (rc < 0)
		exit(2);
	exit(1);
}

static void catch_sigterm(void)
{
	struct sigaction action;

	memset(&action, 0, sizeof(action));
	action.sa_sigaction = &sigterm_handler;
	action.sa_flags = SA_SIGINFO;
	if (sigaction(SIGTERM, &action, NULL) < 0)
		fprintf(stderr, "%s: Failed to register SIGTERM handler.\n", logprefix());
}

static void cleanup_lockfile(void)
{
	if (snis_server_lockfile[0] != '\0')
		(void) rmdir(snis_server_lockfile);
}

static void create_lock_or_die(char *location)
{
	int rc;

	if (!location)
		return;

	snprintf(snis_server_lockfile, sizeof(snis_server_lockfile) - 1, "/tmp/snis_server_lock.%s", location);
	catch_sigterm();
	rc = mkdir(snis_server_lockfile, 0755);
	if (rc != 0) {
		fprintf(stderr, "%s: Failed to create lockdir %s, exiting.\n", logprefix(), snis_server_lockfile);
		exit(1);
	}
	atexit(cleanup_lockfile);
	fprintf(stderr, "%s: Created lockfile %s, proceeding.\n", logprefix(), snis_server_lockfile);
}

static void process_options(int argc, char *argv[])
{
	int c;

	lobby_servernick = default_lobby_servernick;
	lobbyhost = default_lobbyhost;

	while (1) {
		int option_index;
		c = getopt_long(argc, argv, "ehi:L:l:m:n:s:tv", long_options, &option_index);
		if (c == -1)
			break;
		switch (c) {
		case 'e':
			lua_enscript_enabled = 1;
			fprintf(stderr, "WARNING: lua enscript enabled!\n");
			fprintf(stderr, "THIS PERMITS USERS TO CREATE FILES ON THE SERVER\n");
			break;
		case 'i':
			initial_lua_script = optarg;
			break;
		case 'h':
			usage();
			break;
		case 'l':
			lobbyhost = optarg;
			break;
		case 'L':
			lobby_location = optarg;
			create_lock_or_die(lobby_location);
			break;
		case 'm':
			if (!multiverse_server) {
				multiverse_server = malloc(sizeof(*multiverse_server));
				memset(multiverse_server, 0, sizeof(*multiverse_server));
			}
			strncpy(multiverse_server->location, optarg,
					sizeof(multiverse_server->location) - 1);
			multiverse_server->sock = -1;
			pthread_mutex_init(&multiverse_server->queue_mutex, NULL);
			break;
		case 's':
			solarsystem_name = optarg;
			logprefixstr = NULL; /* Force update by logprefix() */
			break;
		case 'n':
			lobby_servernick = optarg;
			break;
		case 'v':
			printf("SPACE NERDS IN SPACE SERVER ");
			printf("%s\n", BUILD_INFO_STRING1);
			printf("%s\n", BUILD_INFO_STRING2);
			exit(0);
			break;
		case 't':
			trap_nans = 1;
			break;
		}
	}
	if (lobby_location == NULL)
		usage();
}

static void wait_for_multiverse_bound_packets(struct multiverse_server_info *msi)
{
	int rc;

	pthread_mutex_lock(&msi->event_mutex);
	while (!msi->have_packets_to_xmit) {
		rc = pthread_cond_wait(&msi->write_cond, &msi->event_mutex);
		if (rc != 0)
			printf("%s: pthread_cond_wait failed %s:%d.\n",
				logprefix(), __FILE__, __LINE__);
		if (msi->have_packets_to_xmit)
			break;
	}
	pthread_mutex_unlock(&msi->event_mutex);
}

static void wakeup_multiverse_writer(struct multiverse_server_info *msi)
{
	int rc;

	pthread_mutex_lock(&msi->event_mutex);
	msi->have_packets_to_xmit = 1;
	rc = pthread_cond_broadcast(&msi->write_cond);
	if (rc)
		printf("%s: huh... pthread_cond_broadcast failed.\n", logprefix());
	pthread_mutex_unlock(&msi->event_mutex);
}

static void wakeup_multiverse_reader(struct multiverse_server_info *msi)
{
	/* TODO: fill this in */
}

static void write_queued_packets_to_mvserver(struct multiverse_server_info *msi)
{
	struct packed_buffer *buffer;
	int rc;

	pthread_mutex_lock(&msi->event_mutex);
	buffer = packed_buffer_queue_combine(&msi->mverse_queue, &msi->queue_mutex);
	if (buffer) {
		rc = snis_writesocket(msi->sock, buffer->buffer, buffer->buffer_size);
		packed_buffer_free(buffer);
		if (rc) {
			fprintf(stderr, "snis_server; failed to write to multiverse server\n");
			goto badserver;
		}
	} else {
		if (multiverse_debug)
			fprintf(stderr, "%s: multiverse_writer awakened, but nothing to write.\n",
				logprefix());
	}
	if (msi->have_packets_to_xmit)
		msi->have_packets_to_xmit = 0;
	pthread_mutex_unlock(&msi->event_mutex);
	return;

badserver:
	fprintf(stderr, "%s: multiverse server disappeared\n", logprefix());
	pthread_mutex_unlock(&msi->event_mutex);
	shutdown(msi->sock, SHUT_RDWR);
	close(msi->sock);
	msi->sock = -1;
}

static void *multiverse_writer(void *arg)
{
	struct multiverse_server_info *msi = arg;

	assert(msi);
	while (1) {
		wait_for_multiverse_bound_packets(msi);
		write_queued_packets_to_mvserver(msi);
		pthread_mutex_lock(&msi->exit_mutex);
		if (msi->writer_time_to_exit) {
			msi->writer_time_to_exit = 0;
			pthread_mutex_unlock(&msi->exit_mutex);
			break;
		}
		pthread_mutex_unlock(&msi->exit_mutex);
	}
	fprintf(stderr, "%s: multiverse_writer thread exiting\n", logprefix());
	return NULL;
}


static int process_update_bridge(struct multiverse_server_info *msi)
{
	unsigned char pwdhash[PWDHASHLEN];
	int i, rc;
	unsigned char buffer[250];
	struct packed_buffer pb;
	struct snis_entity *o;
	double x, y, z;

#define bytes_to_read (UPDATE_BRIDGE_PACKET_SIZE - PWDHASHLEN - 1)

	fprintf(stderr, "%s: process_update bridge 1, expecting %d bytes\n", logprefix(), bytes_to_read);
	memset(buffer, 0, sizeof(buffer));
	memset(pwdhash, 0, sizeof(pwdhash));
	rc = snis_readsocket(msi->sock, buffer, PWDHASHLEN);
	if (rc != 0)
		return rc;
	memcpy(pwdhash, buffer, PWDHASHLEN);
	print_hash("snis_server: update bridge: ", pwdhash);
	BUILD_ASSERT(sizeof(buffer) > bytes_to_read);
	memset(buffer, 0, sizeof(buffer));
	rc = snis_readsocket(msi->sock, buffer, bytes_to_read);
	if (rc != 0)
		return rc;
	fprintf(stderr, "%s: update bridge 3\n", logprefix());
	pthread_mutex_lock(&universe_mutex);
	i = lookup_bridge_by_pwdhash(pwdhash);
	if (i < 0) {
		fprintf(stderr, "%s: Unknown bridge hash\n", logprefix());
		pthread_mutex_unlock(&universe_mutex);
		return i;
	}
	fprintf(stderr, "%s: update bridge 4\n", logprefix());
	rc = lookup_by_id(bridgelist[i].shipid);
	if (rc < 0) {
		fprintf(stderr, "%s: Failed to lookup ship by id\n", logprefix());
		pthread_mutex_unlock(&universe_mutex);
		return rc;
	}
	o = &go[rc];

	/* Save position */
	x = o->x;
	y = o->y;
	z = o->z;
	fprintf(stderr, "%s: Saving position to %lf,%lf,%lf\n", logprefix(), x, y, z);

	if (!o->tsd.ship.damcon) {
		o->tsd.ship.damcon = malloc(sizeof(*o->tsd.ship.damcon));
		memset(o->tsd.ship.damcon, 0, sizeof(*o->tsd.ship.damcon));
	}
	packed_buffer_init(&pb, buffer, bytes_to_read);
	unpack_bridge_update_packet(o, &pb);

	/* Restore position... */
	fprintf(stderr, "%s: update would set position to %lf,%lf,%lf\n",
		logprefix(), o->x, o->y, o->z);
	fprintf(stderr, "%s: restoring position to %lf,%lf,%lf\n",
		logprefix(), x, y, z);
	o->x = x;
	o->y = y;
	o->z = z;

	pthread_mutex_unlock(&universe_mutex);
	fprintf(stderr, "%s: update bridge 10\n", logprefix());
	return 0;
}

static int process_multiverse_verification(struct multiverse_server_info *msi)
{
	int rc, b;
	unsigned char buffer[PWDHASHLEN + 2];
	unsigned char *pass;
	unsigned char *pwdhash;

	rc = snis_readsocket(msi->sock, buffer, PWDHASHLEN + 1);
	if (rc)
		return rc;
	pass = &buffer[0];
	pwdhash = &buffer[1];
	pthread_mutex_lock(&universe_mutex);
	print_hash("Looking up hash:", pwdhash);
	b = lookup_bridge_by_pwdhash(pwdhash);
	if (b >= 0) {
		switch (*pass) {
		case SNISMV_VERIFICATION_RESPONSE_PASS:
			bridgelist[b].verified = BRIDGE_VERIFIED;
			break;
		case SNISMV_VERIFICATION_RESPONSE_TOO_MANY_BRIDGES:
			bridgelist[b].verified = BRIDGE_REFUSED;
			break;
		case SNISMV_VERIFICATION_RESPONSE_FAIL: /* deliberate fall through */
		default:
			bridgelist[b].verified = BRIDGE_FAILED_VERIFICATION;
			break;
		}
	}
	pthread_mutex_unlock(&universe_mutex);
	if (b < 0) {
		fprintf(stderr, "%s: received verification for unknown pwdhash.  Weird.\n", logprefix());
		print_hash("unknown hash: ", pwdhash);
		return 0;
	}
	return 0;
}

static void *multiverse_reader(void *arg)
{
	struct multiverse_server_info *msi = arg;
	uint8_t previous_opcode, last_opcode, opcode;
	int rc;

	assert(msi);

	last_opcode = 0x00;
	opcode = 0x00;
	while (1) {
		previous_opcode = last_opcode;
		last_opcode = opcode;
		opcode = 0x00;
		if (multiverse_debug)
			fprintf(stderr, "%s: reading from multiverse sock = %d...\n",
				logprefix(), msi->sock);
		rc = snis_readsocket(msi->sock, &opcode, sizeof(opcode));
		if (multiverse_debug)
			fprintf(stderr, "%s: read from multiverse, rc = %d, sock = %d\n",
				logprefix(), rc, msi->sock);
		if (rc != 0) {
			fprintf(stderr, "snis_server multiverse_reader(): snis_readsocket returns %d, errno  %s\n",
				rc, strerror(errno));
			goto protocol_error;
		}
		switch (opcode)	{
		case SNISMV_OPCODE_NOOP:
			break;
		case SNISMV_OPCODE_LOOKUP_BRIDGE:
			fprintf(stderr, "%s: unimplemented multiverse opcode %hhu\n",
				logprefix(), opcode);
			break;
		case SNISMV_OPCODE_UPDATE_BRIDGE:
			rc = process_update_bridge(msi);
			if (rc)
				goto protocol_error;
			break;
		case SNISMV_OPCODE_VERIFICATION_RESPONSE:
			rc = process_multiverse_verification(msi);
			if (rc)
				goto protocol_error;
			break;
		case SNISMV_OPCODE_SHUTDOWN_SNIS_SERVER:
			if (multiverse_debug) {
				fprintf(stderr, "%s: multiverse requested snis_server shutdown. Shutting down now.\n",
					logprefix());
				fflush(stderr);
			}
			exit(0); /* Is this all we need to do? I think so... */
			break;
		default:
			fprintf(stderr, "%s: unimplemented multiverse opcode %hhu\n",
				logprefix(), opcode);
			goto protocol_error;
		}
	}
	return NULL;

protocol_error:
	fprintf(stderr, "%s: protocol error in data from multiverse_server\n", logprefix());
	fprintf(stderr, "%s: opcodes: current = %hhu, last = %hhu, previous = %hhu\n",
			logprefix(), opcode, last_opcode, previous_opcode);
	snis_print_last_buffer("snis_server: ", msi->sock);
	shutdown(msi->sock, SHUT_RDWR);
	close(msi->sock);
	msi->sock = -1;
	return NULL;
}

static void connect_to_multiverse(struct multiverse_server_info *msi, uint32_t ipaddr, uint16_t port)
{
	int rc;
	int sock = -1;
	struct addrinfo *mvserverinfo, *i;
	struct addrinfo hints;
	unsigned char *x = (unsigned char *) &ipaddr;
	char response[100];
	char starsystem_name[SSGL_LOCATIONSIZE];

	assert(msi);

	if (multiverse_debug)
		fprintf(stderr, "%s: connecting to multiverse %s %hhu.%hhu.%hhu.%hhu/%hu\n",
			logprefix(), multiverse_server->location, x[0], x[1], x[2], x[3], port);
	char portstr[50];
	char hoststr[50];
	int flag = 1;

	snprintf(portstr, sizeof(portstr), "%d", port);
	snprintf(hoststr, sizeof(hoststr), "%d.%d.%d.%d", x[0], x[1], x[2], x[3]);

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE | AI_NUMERICSERV | AI_NUMERICHOST;
	rc = getaddrinfo(hoststr, portstr, &hints, &mvserverinfo);
	if (rc) {
		fprintf(stderr, "%s: Failed looking up %s:%s: %s\n",
			logprefix(), hoststr, portstr, gai_strerror(rc));
		goto error;
	}

	for (i = mvserverinfo; i != NULL; i = i->ai_next)
		if (i->ai_family == AF_INET)
			break;
	if (i == NULL)
		goto error;

	sock = socket(AF_INET, SOCK_STREAM, i->ai_protocol);
	if (sock < 0)
		goto error;

	rc = connect(sock, i->ai_addr, i->ai_addrlen);
	if (rc < 0)
		goto error;

	rc = setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, (char *) &flag, sizeof(int));
	if (rc)
		fprintf(stderr, "setsockopt(TCP_NODELAY) failed.\n");
	const int len = sizeof(SNIS_MULTIVERSE_VERSION) - 1;
	if (multiverse_debug)
		fprintf(stderr, "%s: writing SNIS_MULTIVERSE_VERSION (len = %d)\n",
			logprefix(), len);
	rc = snis_writesocket(sock, SNIS_MULTIVERSE_VERSION, len);
	if (multiverse_debug)
		fprintf(stderr, "%s: writesocket returned %d, sock = %d\n", logprefix(), rc, sock);
	if (rc < 0) {
		fprintf(stderr, "%s: snis_writesocket failed: %d (%d:%s)\n",
				logprefix(), rc, errno, strerror(errno));
		goto error;
	}
	memset(response, 0, sizeof(response));
	if (multiverse_debug)
		fprintf(stderr, "%s: reading SNIS_MULTIVERSE_VERSION (len = %d, sock = %d)\n",
			logprefix(), len, sock);
	rc = snis_readsocket(sock, response, len);
	if (multiverse_debug)
		fprintf(stderr, "%s: read socket returned %d (len = %d, sock = %d)\n",
			logprefix(), rc, len, sock);
	if (rc != 0) {
		fprintf(stderr, "%s: snis_readsocket failed: %d (%d:%s)\n",
				logprefix(), rc, errno, strerror(errno));
		fprintf(stderr, "response = '%s'\n", response);
		sock = -1;
	}
	response[len] = '\0';
	if (multiverse_debug)
		fprintf(stderr, "%s: got SNIS_MULTIVERSE_VERSION:'%s'\n",
			logprefix(), response);
	if (strcmp(response, SNIS_MULTIVERSE_VERSION) != 0) {
		fprintf(stderr, "%s: expected '%s' got '%s' from snis_multiverse\n",
			logprefix(), SNIS_MULTIVERSE_VERSION, response);
		goto error;
	}

	memset(starsystem_name, 0, sizeof(starsystem_name));
	snprintf(starsystem_name, sizeof(starsystem_name) - 1, "%s", solarsystem_name);
	if (multiverse_debug)
		fprintf(stderr, "%s: writing starsystem name %s to multiverse\n", logprefix(), starsystem_name);
	rc = snis_writesocket(sock, starsystem_name, SSGL_LOCATIONSIZE);
	if (rc != 0) {
		fprintf(stderr, "%s: failed to write starsystem name to snis_multiverse\n",
			logprefix());
	}
	if (multiverse_debug) {
		fprintf(stderr, "%s: write starsystem name %s to multiverse\n", logprefix(), starsystem_name);

		fprintf(stderr, "%s: connected to snis_multiverse (%hhu.%hhu.%hhu.%hhu/%hu on socket %d)\n",
			logprefix(), x[0], x[1], x[2], x[3], port, sock);
	}

	msi->sock = sock;
	msi->ipaddr = ipaddr;
	msi->port = port;
	msi->writer_time_to_exit = 0;
	msi->reader_time_to_exit = 0;
	msi->have_packets_to_xmit = 0;
	packed_buffer_queue_init(&msi->mverse_queue);
	pthread_mutex_init(&msi->queue_mutex, NULL);
	pthread_mutex_init(&msi->event_mutex, NULL);
	pthread_mutex_init(&msi->exit_mutex, NULL);
	pthread_cond_init(&msi->write_cond, NULL);
	if (multiverse_debug)
		fprintf(stderr, "%s: starting multiverse reader thread\n", logprefix());
	rc = create_thread(&msi->read_thread, multiverse_reader, msi, "sniss-mvrdr", 1);
	if (rc) {
		fprintf(stderr, "%s: Failed to create multiverse reader thread: %d '%s', '%s'\n",
			logprefix(), rc, strerror(rc), strerror(errno));
	}
	if (multiverse_debug) {
		fprintf(stderr, "%s: started multiverse reader thread\n", logprefix());
		fprintf(stderr, "%s: starting multiverse writer thread\n", logprefix());
	}
	rc = create_thread(&msi->write_thread, multiverse_writer, msi, "sniss-mvwrtr", 1);
	if (rc) {
		fprintf(stderr, "%s: Failed to create multiverse writer thread: %d '%s', '%s'\n",
			logprefix(), rc, strerror(rc), strerror(errno));
	}
	if (multiverse_debug)
		fprintf(stderr, "%s: started multiverse writer thread\n", logprefix());
	freeaddrinfo(mvserverinfo);
	return;

error:
	if (sock >= 0) {
		shutdown(sock, SHUT_RDWR);
		close(sock);
	}
	freeaddrinfo(mvserverinfo);
	return;
}

static void disconnect_from_multiverse(struct multiverse_server_info *msi)
{
	unsigned char *x;

	assert(multiverse_server);
	x = (unsigned char *) &multiverse_server->ipaddr;
	if (multiverse_debug)
		fprintf(stderr, "%s: disconnecting from multiverse %s %u.%u.%u.%u/%hu\n",
			logprefix(), multiverse_server->location,
			x[0], x[1], x[2], x[3], multiverse_server->port);

	/* Tell multiverse reader and writer threads to exit */
	pthread_mutex_lock(&msi->exit_mutex);
	msi->reader_time_to_exit = 1;
	msi->writer_time_to_exit = 1;
	pthread_mutex_unlock(&msi->exit_mutex);
	wakeup_multiverse_writer(msi);
	wakeup_multiverse_reader(msi);
}

static void update_starmap(struct ssgl_game_server *gameserver, int ngameservers)
{
	int i, j, rc, found;
	double x, y, z;

	pthread_mutex_lock(&universe_mutex);
	for (i = 0; i < ngameservers; i++) {
		rc = sscanf(gameserver[i].game_instance, "%lf %lf %lf", &x, &y, &z);
		if (rc != 3)
			continue;
		/* Lookup this entry in the current star map */
		found = 0;
		for (j = 0; j < nstarmap_entries; j++) {
			if (strncasecmp(starmap[j].name, gameserver[i].location, SSGL_LOCATIONSIZE) == 0) {
				if (x != starmap[j].x || y != starmap[j].y || z != starmap[j].z) {
					/* found it, and (strangely) it moved */
					starmap[j].x = x;
					starmap[j].y = y;
					starmap[j].z = z;
					starmap_dirty = 1;
				}
				found = 1;
				break;
			}
		}
		/* Didn't find it, it is one we do not know about, add it. */
		if (!found && nstarmap_entries < ARRAYSIZE(starmap)) {
			strncpy(starmap[nstarmap_entries].name, gameserver[i].location, SSGL_LOCATIONSIZE);
			starmap[j].x = x;
			starmap[j].y = y;
			starmap[j].z = z;
			nstarmap_entries++;
			starmap_dirty = 1;
		}
	}

	/* Now check if there are ones we knew about that have disappeared */
	for (i = 0; i < nstarmap_entries; /* no increment here */) {
		found = 0;
		for (j = 0; j < ngameservers; j++) {
			if (strncasecmp(starmap[i].name, gameserver[j].location, SSGL_LOCATIONSIZE) == 0) {
				found = 1;
				break;
			}
		}
		if (!found) {
			/* starmap contains an entry with no corresponding server, so delete it. */
			for (j = i; j < nstarmap_entries - 1; j++)
				starmap[j] = starmap[j + 1];
			nstarmap_entries--;
			starmap_dirty = 1;
		} else {
			i++;
		}
	}
	pthread_mutex_unlock(&universe_mutex);
}

/* This is used to check if the multiverse server has changed */
static void servers_changed_cb(void *cookie)
{
	struct ssgl_game_server *gameserver = NULL;
	int i, nservers, same_as_before = 0;
	uint32_t ipaddr = -1;
	uint16_t port = -1;
	int found_multiverse_server = 0;

	fprintf(stderr, "%s: servers_changed_cb, solarsystem_name = '%s'\n", logprefix(), solarsystem_name);
	if (!multiverse_server) {
		fprintf(stderr, "%s: multiverse_server not set\n", logprefix());
		return;
	}

	if (server_tracker_get_server_list(server_tracker, &gameserver, &nservers) != 0) {
		fprintf(stderr, "%s: Failed to get server list at %s:%d\n",
			logprefix(), __FILE__, __LINE__);
		return;
	}
	update_starmap(gameserver, nservers);
	free(gameserver);
	gameserver = NULL;

	if (server_tracker_get_multiverse_list(server_tracker, &gameserver, &nservers) != 0) {
		fprintf(stderr, "%s: Failed to get server list at %s:%d\n",
			logprefix(), __FILE__, __LINE__);
		return;
	}

	fprintf(stderr, "%s: servers_changed_cb taking queue lock\n", logprefix());
	pthread_mutex_lock(&multiverse_server->queue_mutex);
	fprintf(stderr, "%s: servers_changed_cb got queue lock (nservers = %d)\n",
			logprefix(), nservers);
	for (i = 0; i < nservers; i++) {
		fprintf(stderr, "%s: servers_changed_cb i = %d\n", logprefix(), i);
		if (strncmp(gameserver[i].location, multiverse_server->location, SSGL_LOCATIONSIZE) != 0)
			continue;
		fprintf(stderr, "%s: servers_changed_cb i = %d, location = %s\n",
					logprefix(), i, multiverse_server->location);
		if (strncmp(gameserver[i].game_type, "SNIS-MVERSE",
				sizeof(gameserver[i].game_type)) != 0)
			continue;
		fprintf(stderr, "%s: servers_changed_cb i = %d, game type = SNIS-MVERSE\n",
			logprefix(), i);
		if (gameserver[i].ipaddr == multiverse_server->ipaddr &&
		    ntohs(gameserver[i].port) == multiverse_server->port) {
			same_as_before = 1;
			break;
		}
		fprintf(stderr, "%s: servers_changed_cb i = %d, new ip/port\n",
			logprefix(), i);
		ipaddr = gameserver[i].ipaddr;
		port = ntohs(gameserver[i].port);
		fprintf(stderr, "%s: connect_to_multiverse, ipaddr=%08x,port=%d\n",
			logprefix(), ipaddr, port);
		found_multiverse_server = 1;
		break;
	}

	if (!found_multiverse_server || nservers <= 0) {
		pthread_mutex_unlock(&multiverse_server->queue_mutex);
		free(gameserver);
		return;
	}

	if (same_as_before) {
		fprintf(stderr, "%s: multiverse servers same as before\n", logprefix());
		pthread_mutex_unlock(&multiverse_server->queue_mutex);
		free(gameserver);
		return;
	}

	if (multiverse_server->sock != -1) {
		fprintf(stderr, "%s: servers_changed_cb disconnecting from multiverse server\n",
			logprefix());
		disconnect_from_multiverse(multiverse_server);
	}
	fprintf(stderr, "%s: servers_changed_cb connecting to multiverse server\n",
			logprefix());
	connect_to_multiverse(multiverse_server, ipaddr, port);
	fprintf(stderr, "%s: servers_changed_cb connected to multiverse server\n",
		logprefix());
	fprintf(stderr, "%s: servers_changed_cb releasing queue lock\n",
		logprefix());
	pthread_mutex_unlock(&multiverse_server->queue_mutex);
	fprintf(stderr, "%s: servers_changed_cb released queue lock\n", logprefix());
	if (gameserver)
		free(gameserver);
}

static void init_meshes(void)
{
	unit_cube_mesh = mesh_unit_cube(1);
	/* The "unit" cube is 1 unit in *radius* -- we need one with 0.5 unit radius. */
	mesh_scale(unit_cube_mesh, 0.5);
	low_poly_sphere_mesh = mesh_unit_icosphere(4);
	mesh_scale(low_poly_sphere_mesh, 0.5);
}

static void read_replacement_assets(struct replacement_asset *r, char *asset_dir)
{
	int rc;
	char p[PATH_MAX];

	sprintf(p, "%s/replacement_assets.txt", asset_dir);
	errno = 0;
	rc = replacement_asset_read(p, asset_dir, r);
	if (rc < 0 && errno != EEXIST)
		fprintf(stderr, "%s: Warning:  %s\n", p, strerror(errno));
}

int main(int argc, char *argv[])
{
	int port, i;
	struct timespec thirtieth_second;

	refuse_to_run_as_root("snis_server");
	take_your_locale_and_shove_it();

	process_options(argc, argv);
	if (trap_nans)
		feenableexcept(FE_DIVBYZERO | FE_INVALID | FE_OVERFLOW);

	asset_dir = override_asset_dir();
	read_replacement_assets(&replacement_assets, asset_dir);
	set_random_seed(-1);
	init_natural_language_system();
	init_meshes();
	init_planetary_ring_data(planetary_ring_data, NPLANETARY_RING_MATERIALS, PLANETARY_RING_MTWIST_SEED);

	char commodity_path[PATH_MAX];
	snprintf(commodity_path, sizeof(commodity_path), "%s/%s", asset_dir, "commodities.txt");
	commodity = read_commodities(commodity_path, &ncommodities);
	nstandard_commodities = ncommodities;

	/* count possible contraband items */
	ncontraband = 0;
	for (i = 0; i < ncommodities; i++)
		if (commodity[i].legality < 1.0)
			ncontraband++;

	if (read_ship_types()) {
		fprintf(stderr, "%s: unable to read ship types\n", argv[0]);
		return -1;
	}
	setup_rts_unit_type_to_ship_type_map(ship_type, nshiptypes);

	if (read_factions())
		return -1;

	if (read_starbase_model_metadata(asset_dir, "starbase_models.txt",
		&nstarbase_models, &starbase_metadata)) {
		fprintf(stderr, "Failed reading starbase model metadata\n");
		return -1;
	}
	docking_port_info = read_docking_port_info(starbase_metadata, nstarbase_models,
					STARBASE_SCALE_FACTOR);

	solarsystem_assets = read_solarsystem_assets(solarsystem_name);
	if (!solarsystem_assets) {
		fprintf(stderr, "Failed reading solarsystem assets for '%s'\n", solarsystem_name);
		return -1;
	}

	open_log_file();

	setup_lua();
	snis_protocol_debugging(1);
	snis_debug_dump_set_label("SERVER");
	snis_opcode_def_init();

	memset(&thirtieth_second, 0, sizeof(thirtieth_second));
	thirtieth_second.tv_nsec = 33333333; /* 1/30th second */

	space_partition = space_partition_init(40, 40,
			-UNIVERSE_LIMIT, UNIVERSE_LIMIT,
			-UNIVERSE_LIMIT, UNIVERSE_LIMIT,
			offsetof(struct snis_entity, partition));

	make_universe();
	run_initial_lua_scripts();
	port = start_listener_thread();

	ignore_sigpipe();	
	snis_collect_netstats(&netstats);
	if (getenv("SNISSERVERNOLOBBY") == NULL) {
		/* Pack the solarsystem position into the gameinstance string */
		snprintf(lobby_gameinstance, sizeof(lobby_gameinstance), "%3.1lf %3.1lf %3.1lf",
			solarsystem_assets->x, solarsystem_assets->y, solarsystem_assets->z);
		register_with_game_lobby(lobbyhost, port,
			lobby_servernick, lobby_gameinstance, lobby_location);
		server_tracker = server_tracker_start(lobbyhost, servers_changed_cb, NULL);
	} else {
		printf("%s: Skipping lobby registration\n", logprefix());
		server_tracker = NULL;
		if (multiverse_server != NULL) {
			fprintf(stderr,
				"SNISSERVERNOLOBBY and --multiverse option are mutually exclusive\n");
			fprintf(stderr,
				"ignoring --multiverse option\n");
			free(multiverse_server);
			multiverse_server = NULL;
		}
	}

	const double maxTimeBehind = 0.5;
	double delta = 1.0/10.0;

	int discontinuity = 0;
	double currentTime = time_now_double();
	double nextTime = currentTime + delta;
	universe_timestamp_absolute = currentTime;
	while (1) {
		if ((volatile int) game_paused) {
			double this_time = time_now_double();
			sleep_double(0.1);
			update_multiverse_bridge_count(this_time);
			continue;
		}
		currentTime = time_now_double();

		if (currentTime - nextTime > maxTimeBehind) {
			printf("too far behind sec=%f, skipping\n", currentTime - nextTime);
			nextTime = currentTime;
			discontinuity = 1;
		}
		if (currentTime >= nextTime) {
			move_objects(nextTime, discontinuity);
			process_lua_commands();
			rts_ai_run();
			update_multiverse_bridge_count(currentTime);

			discontinuity = 0;
			nextTime += delta;
		} else {
			double timeToSleep = nextTime-currentTime;
			if (timeToSleep > 0)
				sleep_double(timeToSleep);
		}
		simulate_slow_server(0);
		check_rts_finish_condition();
	}
	space_partition_free(space_partition);
	lua_teardown();

	snis_close_logfile();

	return 0;
}
