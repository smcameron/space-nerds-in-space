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
#include "snis.h"
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
#include "mesh.h"
#include "turret_aimer.h"
#include "pthread_util.h"

#include "snis_entity_key_value_specification.h"

#define CLIENT_UPDATE_PERIOD_NSECS 500000000
#define MAXCLIENTS 100

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
	int sock;
	uint32_t ipaddr;
	uint16_t port;
	pthread_t read_thread;
	pthread_t write_thread;
	pthread_cond_t write_cond;
	struct packed_buffer_queue mverse_queue;
	pthread_mutex_t queue_mutex;
	pthread_mutex_t event_mutex;
	pthread_mutex_t exit_mutex;
	struct ssgl_game_server *mverse;
	int mverse_count;
	int have_packets_to_xmit;
	int writer_time_to_exit;
	int reader_time_to_exit;
#define LOCATIONSIZE (sizeof((struct ssgl_game_server *) 0)->location)
#define GAMEINSTANCESIZE (sizeof((struct ssgl_game_server *) 0)->game_instance)
	char location[LOCATIONSIZE];
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
static void npc_menu_item_board_passengers(struct npc_menu_item *item,
				char *npcname, struct npc_bot_state *botstate);
static void npc_menu_item_disembark_passengers(struct npc_menu_item *item,
				char *npcname, struct npc_bot_state *botstate);
static void npc_menu_item_eject_passengers(struct npc_menu_item *item,
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

static struct npc_menu_item repairs_and_maintenance_menu[] = {
	/* by convention, first element is menu title */
	{ "REPAIRS AND MAINTENANCE MENU", 0, 0, 0 },
	{ "BUY SHIELD SYSTEM PARTS", 0, 0, npc_menu_item_buy_parts },
	{ "BUY IMPULSE DRIVE PARTS", 0, 0, npc_menu_item_buy_parts },
	{ "BUY WARP DRIVE PARTS", 0, 0, npc_menu_item_buy_parts },
	{ "BUY MANEUVERING PARTS", 0, 0, npc_menu_item_buy_parts },
	{ "BUY PHASER BANKS PARTS", 0, 0, npc_menu_item_buy_parts },
	{ "BUY SENSORS PARTS", 0, 0, npc_menu_item_buy_parts },
	{ "BUY COMMUNICATIONS PARTS", 0, 0, npc_menu_item_buy_parts },
	{ "BUY TRACTOR BEAM PARTS", 0, 0, npc_menu_item_buy_parts },
	{ "BUY LIFE SUPPORT SYSTEM PARTS", 0, 0, npc_menu_item_buy_parts },
	{ 0, 0, 0, 0 }, /* mark end of menu items */
};

static struct npc_menu_item arrange_transport_contracts_menu[] = {
	/* by convention, first element is menu title */
	{ "TRANSPORT CONTRACT MENU", 0, 0, 0 },
	{ "BUY CARGO", 0, 0, npc_menu_item_buy_cargo },
	{ "SELL CARGO", 0, 0, npc_menu_item_sell_cargo },
	{ "BOARD PASSENGERS", 0, 0, npc_menu_item_board_passengers },
	{ "DISEMBARK PASSENGERS", 0, 0, npc_menu_item_disembark_passengers },
	{ "EJECT PASSENGERS", 0, 0, npc_menu_item_eject_passengers },
	{ 0, 0, 0, 0 }, /* mark end of menu items */
};

static struct npc_menu_item starbase_main_menu[] = {
	{ "STARBASE MAIN MENU", 0, 0, 0 },  /* by convention, first element is menu title */
	{ "LOCAL TRAVEL ADVISORY", 0, 0, npc_menu_item_travel_advisory },
	{ "REQUEST PERMISSION TO DOCK", 0, 0, npc_menu_item_request_dock },
	{ "BUY WARP-GATE TICKETS", 0, 0, npc_menu_item_warp_gate_tickets },
	{ "REQUEST TOWING SERVICE", 0, 0, npc_menu_item_towing_service },
	{ "BUY FUEL", 0, 0, npc_menu_item_not_implemented },
	{ "REPAIRS AND MAINTENANCE", 0, repairs_and_maintenance_menu, 0 },
	{ "ARRANGE TRANSPORT CONTRACTS", 0, arrange_transport_contracts_menu, 0 },
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
	int socket;
	pthread_t read_thread;
	pthread_t write_thread;
	struct packed_buffer_queue client_write_queue;
	pthread_mutex_t client_write_queue_mutex;
	uint32_t shipid;
	uint32_t ship_index;
	uint32_t role;
	uint32_t timestamp;
	int bridge;
	int debug_ai;
	struct snis_entity_client_info *go_clients; /* ptr to array of size MAXGAMEOBJS */
	struct snis_damcon_entity_client_info *damcon_data_clients; /* ptr to array of size MAXDAMCONENTITIES */
	uint8_t refcount; /* how many threads currently using this client structure. */
	int request_universe_timestamp;
	char *build_info[2];
	uint32_t latency_in_usec;
	int waypoints_dirty;
#define COMPUTE_AVERAGE_TO_CLIENT_BUFFER_SIZE 0
#if COMPUTE_AVERAGE_TO_CLIENT_BUFFER_SIZE
	uint64_t write_sum;
	uint64_t write_count;
#endif
} client[MAXCLIENTS];
static int nclients = 0;
#define client_index(client_ptr) ((client_ptr) - &client[0])
static pthread_mutex_t client_mutex = PTHREAD_MUTEX_INITIALIZER;

static struct bridge_data {
	unsigned char shipname[20];
	unsigned char password[20];
	uint32_t shipid;
	struct damcon_data damcon;
	struct snis_damcon_entity *robot;
	int incoming_fire_detected;
	int last_incoming_fire_sound_time;
	double warpx, warpy, warpz;
	union vec3 warpv;
	int warptimeleft;
	int comms_channel;
	struct npc_bot_state npcbot;
	int last_docking_permission_denied_time;
	uint32_t science_selection;
	int current_displaymode;
	struct ssgl_game_server warp_gate_ticket;
	unsigned char pwdhash[20];
	int verified; /* whether this bridge has verified with multiverse server */
#define BRIDGE_UNVERIFIED 0
#define BRIDGE_VERIFIED 1
#define BRIDGE_FAILED_VERIFICATION 2
#define BRIDGE_REFUSED 3
	int requested_verification; /* Whether we've requested verification from multiverse server yet */
	int requested_creation; /* whether user has requested creating new ship */
	int nclients;
	struct player_waypoint waypoint[MAXWAYPOINTS];
	int selected_waypoint;
	int nwaypoints;
} bridgelist[MAXCLIENTS];
static int nbridges = 0;
static pthread_mutex_t universe_mutex = PTHREAD_MUTEX_INITIALIZER;

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
static struct starmap_entry {
	char name[LOCATIONSIZE];
	double x, y, z;
} starmap[MAXSTARMAPENTRIES] = { 0 };
static int nstarmap_entries = 0;
static int starmap_dirty = 0;

static int safe_mode = 0; /* prevents enemies from attacking if set */

#ifndef PREFIX
#define PREFIX .
#warn "PREFIX defaulted to ."
#endif

#define STRPREFIX(s) str(s)
#define str(s) #s

static char *default_asset_dir = STRPREFIX(PREFIX) "/share/snis";
static char *asset_dir;

static int nebulalist[NNEBULA] = { 0 };

static int ncommodities;
static int ncontraband;
static struct commodity *commodity;

static struct mesh *unit_cube_mesh;

static struct snis_object_pool *pool;
static struct snis_entity go[MAXGAMEOBJS];
#define go_index(snis_entity_ptr) ((snis_entity_ptr) - &go[0])
static struct space_partition *space_partition = NULL;

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
	printf("xxxx1\n"); fflush(stdout);
	free(qe);
	fflush(stdout); fflush(stderr);
	printf("xxxx2\n"); fflush(stdout);
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

static int do_lua_pcall(char *function_name, lua_State *l, int nargs, int nresults, int errfunc)
{
	int rc;

	rc = lua_pcall(l, nargs, nresults, errfunc);
	if (rc) {
		fprintf(stderr, "snis_server: lua callback '%s' had error %d: '%s'.\n",
			function_name, rc, lua_tostring(l, -1));
		stacktrace("do_lua_pcall");
		fflush(stderr);
	}
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
	struct lua_proximity_check *i, *last, *next;
	struct lua_proximity_check *temp_list = NULL;
	struct snis_entity *o1, *o2;
	double dx, dy, dz, dist2;
	int a, b;

	last = NULL;
	pthread_mutex_lock(&universe_mutex);
	for (i = lua_proximity_check; i != NULL;) {
		next = i->next;

		a = lookup_by_id(i->oid1);
		b = lookup_by_id(i->oid2);
		if (a < 0 || b < 0) {
			/* remove this check */
			if (last)
				last->next = next;
			else
				lua_proximity_check = next;
			continue;
		}

		o1 = &go[a];
		o2 = &go[b];
		dx = o1->x - o2->x;
		dy = o1->y - o2->y;
		dz = o1->z - o2->z;
		dist2 = dx * dx + dy * dy + dz * dz;


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

	for (i = *sched; i != NULL; i = next) {
		next = next_scheduled_callback(i);
		callback = callback_name(i);
		lua_getglobal(lua_state, callback);
		lua_pushnumber(lua_state, callback_schedule_entry_param(i, 0));
		lua_pushnumber(lua_state, callback_schedule_entry_param(i, 1));
		lua_pushnumber(lua_state, callback_schedule_entry_param(i, 2));
		do_lua_pcall(callback, lua_state, 3, 0, 0);
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
	printf("put '%s' in buffer\n", buffer);
}

static char *logprefix(void)
{
	static char logprefixstrbuffer[100];
	static char *logprefixstr = NULL;
	if (logprefixstr != NULL)
		return logprefixstr;
	sprintf(logprefixstrbuffer, "%s(%s):", "snis_server", solarsystem_name);
	logprefixstr = logprefixstrbuffer;
	return logprefixstr;
}

static void log_client_info(int level, int connection, char *info)
{
	char client_ip[50];

	if (level < snis_log_level)
		return;

	memset(client_ip, 0, sizeof(client_ip));
	get_peer_name(connection, client_ip);
	snis_log(level, "%s: %s: %s", logprefix(), client_ip, info);
}

static void delete_from_clients_and_server_helper(struct snis_entity *o, int take_client_lock);
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
			delete_from_clients_and_server_helper(&go[i], 0);
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
	for (i = 0; i < 20; i++)
		fprintf(stderr, "%02x", pwdhash[i]);
	fprintf(stderr, "\n");
}

static int lookup_bridge_by_pwdhash(unsigned char *pwdhash)
{
	/* assumes universe_mutex held */
	int i;

	for (i = 0; i < nbridges; i++)
		if (memcmp(bridgelist[i].pwdhash, pwdhash, 20) == 0)
			return i;
	return -1;
}

static void generic_move(__attribute__((unused)) struct snis_entity *o)
{
	return;
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

static void delete_from_clients_and_server(struct snis_entity *o);
static void cargo_container_move(struct snis_entity *o)
{
	set_object_location(o, o->x + o->vx, o->y + o->vy, o->z + o->vz);
	o->timestamp = universe_timestamp;
	if (o->tsd.cargo_container.persistent)
		return;
	o->alive--;
	if (o->alive == 0)
		delete_from_clients_and_server(o);
}

static void derelict_collision_detection(void *derelict, void *object)
{
	struct snis_entity *o = object;
	struct snis_entity *d = derelict;
	if (o->type == OBJTYPE_SHIP1 || o->type == OBJTYPE_SHIP2)
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
		dist2 = dist3dsqrd(t->x - o->x, t->y - o->y, t->z - o->z);
		if (dist2 < 30.0 * 30.0) {
			a = snis_randn(360) * M_PI / 180.0;
			r = 60.0;
			set_object_location(t, o->tsd.wormhole.dest_x + cos(a) * r, 
						o->tsd.wormhole.dest_y,
						o->tsd.wormhole.dest_z + sin(a) * r);
			t->timestamp = universe_timestamp;
			if (t->type == OBJTYPE_SHIP1)
				send_wormhole_limbo_packet(t->id, 5 * 30);
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

static int add_ship(int faction, int auto_respawn);
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
				add_ship(lowest_faction, 1);
			else {
				/* respawn cops as cops */
				hp = o->tsd.ship.home_planet;
				i = add_ship(o->sdata.faction, 1);
				if (i < 0)
					break;
				o = &go[i];
				o->tsd.ship.home_planet = hp;
				o->tsd.ship.shiptype = SHIP_CLASS_ENFORCER;
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
	snis_object_pool_free_object(pool, go_index(o));
	o->id = -1;
	o->alive = 0;
}

static void snis_queue_delete_object_helper(struct snis_entity *o)
{
	int i;
	uint32_t oid = o->id;

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

static void send_packet_to_all_bridges_on_channel(uint32_t channel,
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

		if (bridgelist[c->bridge].comms_channel != channel)
			continue;

		pbc = packed_buffer_copy(pb);
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

static int add_explosion(double x, double y, double z, uint16_t velocity,
				uint16_t nsparks, uint16_t time, uint8_t victim_type);

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

static void distribute_damage_to_damcon_system_parts(struct snis_entity *o,
			struct damcon_data *d, int damage, int damcon_system)
{
	int i, n, count;
	struct snis_damcon_entity *p;
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

	/* distribute the per-part-damage among the parts */
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
	return;
}

static int roll_damage(struct snis_entity *o, struct damcon_data *d, 
			double weapons_factor, double shield_strength, uint8_t system,
			int damcon_system)
{
	int damage = (uint8_t) (weapons_factor *
				(double) (20 + snis_randn(40)) * (1.2 - shield_strength));
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

static int lookup_bridge_by_shipid(uint32_t shipid);
static void calculate_torpedolike_damage(struct snis_entity *o, double weapons_factor)
{
	double ss;
	const double twp = TORPEDO_WEAPONS_FACTOR * (o->type == OBJTYPE_SHIP1 ? 0.333 : 1.0);
	struct damcon_data *d = NULL;

	if (o->type == OBJTYPE_SHIP1) {
		int bridge = lookup_bridge_by_shipid(o->id);

		if (bridge < 0) {
			fprintf(stderr, "bug at %s:%d, bridge lookup failed.\n", __FILE__, __LINE__);
			return;
		}
		d = &bridgelist[bridge].damcon;
	} else if (o->type == OBJTYPE_TURRET) {
		calculate_turret_damage(o);
		return;
	} else if (o->type == OBJTYPE_BLOCK) {
		calculate_block_damage(o);
		return;
	}

	ss = shield_strength(snis_randn(255), o->sdata.shield_strength,
				o->sdata.shield_width,
				o->sdata.shield_depth,
				o->sdata.shield_wavelength);

	o->tsd.ship.damage.shield_damage = roll_damage(o, d, twp, ss,
			o->tsd.ship.damage.shield_damage, DAMCON_TYPE_SHIELDSYSTEM);
	o->tsd.ship.damage.impulse_damage = roll_damage(o, d, twp, ss,
			o->tsd.ship.damage.impulse_damage, DAMCON_TYPE_IMPULSE);
	o->tsd.ship.damage.warp_damage = roll_damage(o, d, twp, ss,
			o->tsd.ship.damage.warp_damage, DAMCON_TYPE_WARPDRIVE);
	o->tsd.ship.damage.maneuvering_damage = roll_damage(o, d, twp, ss,
			o->tsd.ship.damage.maneuvering_damage, DAMCON_TYPE_MANEUVERING);
	o->tsd.ship.damage.phaser_banks_damage = roll_damage(o, d, twp, ss,
			o->tsd.ship.damage.phaser_banks_damage, DAMCON_TYPE_PHASERBANK);
	o->tsd.ship.damage.sensors_damage = roll_damage(o, d, twp, ss,
			o->tsd.ship.damage.sensors_damage, DAMCON_TYPE_SENSORARRAY);
	o->tsd.ship.damage.comms_damage = roll_damage(o, d, twp, ss,
			o->tsd.ship.damage.comms_damage, DAMCON_TYPE_COMMUNICATIONS);
	o->tsd.ship.damage.tractor_damage = roll_damage(o, d, twp, ss,
			o->tsd.ship.damage.tractor_damage, DAMCON_TYPE_TRACTORSYSTEM);
	o->tsd.ship.damage.lifesupport_damage = roll_damage(o, d, twp, ss,
			o->tsd.ship.damage.lifesupport_damage, DAMCON_TYPE_LIFESUPPORTSYSTEM);

	if (o->tsd.ship.damage.shield_damage == 255) { 
		o->timestamp = universe_timestamp;
		o->respawn_time = universe_timestamp + RESPAWN_TIME_SECS * 10;
		o->alive = 0;
	}
}

static void calculate_torpedo_damage(struct snis_entity *o)
{
	calculate_torpedolike_damage(o, TORPEDO_WEAPONS_FACTOR);
}

static void calculate_atmosphere_damage(struct snis_entity *o)
{
	calculate_torpedolike_damage(o, ATMOSPHERE_DAMAGE_FACTOR);
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
		o->respawn_time = universe_timestamp + RESPAWN_TIME_SECS * 10;
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

static void send_ship_damage_packet(struct snis_entity *o);
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
	assert(i >= 0);
	p = &go[i];
	home_planet.v.x = p->x;
	home_planet.v.y = p->y;
	home_planet.v.z = p->z;

	orbit_radius = (0.01 * (float) snis_randn(100) + 2.0) * p->tsd.planet.radius;

	npoints = MAX_PATROL_POINTS;
	patrol = &cop->tsd.ship.ai[0].u.cop;
	patrol->npoints = npoints;
	patrol->dest = 0;

	random_quat(&q);

	for (i = 0; i < npoints; i++) {
		float angle = M_PI / 180.0 * i * (360.0 / MAX_PATROL_POINTS);
		union vec3 v = { { sin(angle) * orbit_radius, cos(angle) * orbit_radius, 0.0 } };
		quat_rot_vec(&patrol->p[i], &v, &q);
		vec3_add_self(&patrol->p[i], &home_planet);
	}
	cop->tsd.ship.dox = patrol->p[0].v.x;
	cop->tsd.ship.doy = patrol->p[0].v.y;
	cop->tsd.ship.doz = patrol->p[0].v.z;
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

	if (attacker->tsd.ship.ai[0].ai_mode != AI_MODE_COP) {
		if (go[i].tsd.ship.ai[0].ai_mode == AI_MODE_COP)
			return; /* Don't attack the cops */

		if (attacker->tsd.ship.in_secure_area || go[i].tsd.ship.in_secure_area) {
			/* TODO: something better */
			return;
		}
	}

	fleet_number = find_fleet_number(attacker);

	n = attacker->tsd.ship.nai_entries;

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
					uint32_t asteroid_id)
{
	int i = lookup_by_id(asteroid_id);
	if (i < 0)
		return;
	int n = miner->tsd.ship.nai_entries;
	if (n >= MAX_AI_STACK_ENTRIES)
		return;
	miner->tsd.ship.nai_entries++;
	miner->tsd.ship.ai[n].ai_mode = AI_MODE_MINING_BOT;
	miner->tsd.ship.ai[n].u.mining_bot.asteroid = asteroid_id;
	miner->tsd.ship.ai[n].u.mining_bot.parent_ship = parent_ship_id;
	miner->tsd.ship.ai[n].u.mining_bot.mode = MINING_MODE_APPROACH_ASTEROID;
	random_quat(&miner->tsd.ship.ai[n].u.mining_bot.orbital_orientation);
	miner->tsd.ship.dox = go[i].x;
	miner->tsd.ship.doy = go[i].y;
	miner->tsd.ship.doz = go[i].z;
}

static int add_derelict(const char *name, double x, double y, double z,
			double vx, double vy, double vz, int shiptype,
			int the_faction, int persistent);

static int add_cargo_container(double x, double y, double z, double vx, double vy, double vz,
				int item, float qty, int persistent);
static int make_derelict(struct snis_entity *o)
{
	int i, rc;
	rc = add_derelict(o->sdata.name, o->x, o->y, o->z,
				o->vx + snis_random_float() * 2.0,
				o->vy + snis_random_float() * 2.0,
				o->vz + snis_random_float() * 2.0,
				o->tsd.ship.shiptype, o->sdata.faction, 0);
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
					o->vz + snis_random_float(), item, qty, 0);
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

	/* only victimize players, other ships and starbases */
	if (v->type != OBJTYPE_STARBASE && v->type != OBJTYPE_SHIP1 &&
		v->type != OBJTYPE_SHIP2)
		return;

	if (!v->alive) /* skip the dead guys */
		return;

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
	dist = dist3d(o->x - v->x, o->y - v->y, o->z - v->z);

	if (dist > XKNOWN_DIM / 10.0) /* too far away */
		return;

	/* nearby friendlies reduce threat level */
	if (o->sdata.faction == v->sdata.faction)
		o->tsd.ship.threat_level -= 10000.0 / (dist + 1.0);

	if (hostility < FACTION_HOSTILITY_THRESHOLD)
		return;

	fightiness = (10000.0 * hostility) / (dist + 1.0);
	o->tsd.ship.threat_level += fightiness;

	if (v->type == OBJTYPE_SHIP1)
		fightiness *= 3.0f; /* prioritize hitting player... */

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

static union vec3 pick_random_patrol_destination(struct snis_entity *ship)
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
		if (count > 1000)
			break;
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
	}
	return v;
}

static void setup_patrol_route(struct snis_entity *o)
{
	int n, npoints, i;
	struct ai_patrol_data *patrol;

	n = o->tsd.ship.nai_entries;
	o->tsd.ship.nai_entries = n + 1;
	o->tsd.ship.ai[n].ai_mode = AI_MODE_PATROL;

	npoints = (snis_randn(100) % 4) + 2;
	assert(npoints > 1 && npoints <= MAX_PATROL_POINTS);
	patrol = &o->tsd.ship.ai[n].u.patrol;
	patrol->npoints = npoints;
	patrol->dest = 0;

	/* FIXME: ensure no duplicate points and order in some sane way */
	for (i = 0; i < npoints; i++)
		patrol->p[i] = pick_random_patrol_destination(o);
	o->tsd.ship.dox = patrol->p[0].v.x;
	o->tsd.ship.doy = patrol->p[0].v.y;
	o->tsd.ship.doz = patrol->p[0].v.z;
}

static void ship_figure_out_what_to_do(struct snis_entity *o)
{
	int fleet_shape;

	if (o->tsd.ship.nai_entries > 0)
		return;
	switch (snis_randn(100) % 3) {
	case 0:
	case 1:	
		setup_patrol_route(o);
		break;
	case 2:
		if (o->sdata.faction == 0) {
			setup_patrol_route(o);
			break;
		}
		if ((snis_randn(100) % 2) == 0)
			fleet_shape = FLEET_TRIANGLE;
		else
			fleet_shape = FLEET_SQUARE;
		setup_patrol_route(o);
		if (fleet_count() < max_fleets()) {
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
			}
			if (!joined) {
				setup_patrol_route(o);
				break;
			}
		}
	default:
		break;
	}
}

static void pop_ai_stack(struct snis_entity *o)
{
	int n;

	n = o->tsd.ship.nai_entries;
	if (n <= 0) {
		ship_figure_out_what_to_do(o);
		return;
	}
	o->tsd.ship.nai_entries--;
	n = o->tsd.ship.nai_entries - 1;
	if (o->tsd.ship.ai[n].ai_mode == AI_MODE_ATTACK)
		calculate_attack_vector(o, MIN_COMBAT_ATTACK_DIST,
						MAX_COMBAT_ATTACK_DIST);
	if (o->tsd.ship.ai[n].ai_mode == AI_MODE_TOW_SHIP)
		o->tsd.ship.ai[n].u.tow_ship.ship_connected = 0;
}

static void pop_ai_attack_mode(struct snis_entity *o)
{
	int n;

	n = o->tsd.ship.nai_entries;
	if (n <= 0) {
		ship_figure_out_what_to_do(o);
		return;
	}
	n--;
	if (o->tsd.ship.ai[n].ai_mode == AI_MODE_ATTACK)
		o->tsd.ship.nai_entries--;
	n--;
	if (o->tsd.ship.ai[n].ai_mode == AI_MODE_ATTACK)
		calculate_attack_vector(o, MIN_COMBAT_ATTACK_DIST,
						MAX_COMBAT_ATTACK_DIST);
	return;
}

static void add_starbase_attacker(struct snis_entity *starbase, int attacker_id)
{
	int n;

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

	dist2 = dist3dsqrd(cop->x - perp->x, cop->y - perp->y, cop->z - perp->z);
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

static void notify_the_cops(struct snis_entity *weapon)
{
	uint32_t perp_id;
	int perp_index;
	struct snis_entity *perp;

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
	if (o->alive >= TORPEDO_LIFETIME - 3)
		return;
	if (t->type != OBJTYPE_SHIP1 && t->type != OBJTYPE_SHIP2 &&
			t->type != OBJTYPE_STARBASE &&
			t->type != OBJTYPE_ASTEROID &&
			t->type != OBJTYPE_CARGO_CONTAINER &&
			t->type != OBJTYPE_PLANET &&
			t->type != OBJTYPE_BLOCK)
		return;
	if (t->id == o->tsd.torpedo.ship_id)
		return; /* can't torpedo yourself. */
	dist2 = dist3dsqrd(t->x - o->x, t->y - o->y, t->z - o->z);

	if (t->type == OBJTYPE_BLOCK) {
		union vec3 torpedo_pos, closest_point;

		if (dist2 > t->tsd.block.radius * t->tsd.block.radius)
			return;
		torpedo_pos.v.x = o->x;
		torpedo_pos.v.y = o->y;
		torpedo_pos.v.z = o->z;

		oriented_bounding_box_closest_point(&torpedo_pos, &t->tsd.block.obb, &closest_point);

		dist2 = dist3dsqrd(o->x - closest_point.v.x, o->y - closest_point.v.y, o->z - closest_point.v.z);
		if (dist2 > TORPEDO_VELOCITY * TORPEDO_VELOCITY)
			return;
		o->alive = 0; /* hit!!!! */
		schedule_callback2(event_callback, &callback_schedule,
					"object-hit-event", t->id, (double) o->tsd.torpedo.ship_id);
		impact_time = universe_timestamp;
		impact_fractional_time = 0.0; /* TODO: something better? */
		(void) add_explosion(closest_point.v.x, closest_point.v.y, closest_point.v.z, 50, 5, 5, t->type);
		snis_queue_add_sound(DISTANT_TORPEDO_HIT_SOUND, ROLE_SOUNDSERVER, t->id);
		block_add_to_naughty_list(t, o->tsd.torpedo.ship_id);
		calculate_torpedo_damage(o);
		return;
	}

	if (t->type == OBJTYPE_PLANET && dist2 < t->tsd.planet.radius * t->tsd.planet.radius) {
		o->alive = 0; /* smashed into planet */
		schedule_callback2(event_callback, &callback_schedule,
				"object-hit-event", (double) t->id,
				(double) o->tsd.torpedo.ship_id);
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

	notify_the_cops(o);

	if (t->type == OBJTYPE_STARBASE) {
		t->tsd.starbase.under_attack = 1;
		add_starbase_attacker(t, o->tsd.torpedo.ship_id);
		calculate_laser_starbase_damage(t, snis_randn(255));
		send_detonate_packet(t, ix, iy, iz, impact_time, impact_fractional_time);
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
		t->type != OBJTYPE_BLOCK)
		return;

	if (t->type == OBJTYPE_BLOCK) {
		double dist2;
		union vec3 laser_pos, closest_point;

		dist2 = dist3dsqrd(o->x - t->x, o->y - t->y, o->z - t->z);
		if (dist2 > t->tsd.block.radius * t->tsd.block.radius)
			return;
		laser_pos.v.x = o->x;
		laser_pos.v.y = o->y;
		laser_pos.v.z = o->z;

		oriented_bounding_box_closest_point(&laser_pos, &t->tsd.block.obb, &closest_point);

		dist2 = dist3dsqrd(o->x - closest_point.v.x, o->y - closest_point.v.y, o->z - closest_point.v.z);
		if (dist2 > 0.5 * LASER_VELOCITY)
			return;
		o->alive = 0; /* hit!!!! */
		schedule_callback2(event_callback, &callback_schedule,
					"object-hit-event", t->id, (double) o->tsd.laser.ship_id);
		impact_time = universe_timestamp;
		impact_fractional_time = 0.0; /* TODO: something better? */
		(void) add_explosion(closest_point.v.x, closest_point.v.y, closest_point.v.z, 50, 5, 5, t->type);
		snis_queue_add_sound(DISTANT_PHASER_HIT_SOUND, ROLE_SOUNDSERVER, t->id);
		block_add_to_naughty_list(t, o->tsd.laser.ship_id);
		calculate_laser_damage(t, o->tsd.laser.wavelength,
			(float) o->tsd.laser.power * LASER_PROJECTILE_BOOST);
		/* How does the laser get deleted? */
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
	notify_the_cops(o);
	schedule_callback2(event_callback, &callback_schedule,
				"object-hit-event", t->id, o->tsd.laser.ship_id);

	if (t->type == OBJTYPE_STARBASE) {
		t->tsd.starbase.under_attack = 1;
		/* FIXME: looks like lasers do not harm starbases... seems wrong */
		send_detonate_packet(t, ix, iy, iz, impact_time, impact_fractional_time);
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

static void send_comms_packet(char *sender, uint32_t channel, const char *str);
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

	sprintf(name, "%s: ", alien->sdata.name);
	for (i = 0; buffer[i]; i++) {
		buffer[i] = toupper(buffer[i]);
		if (buffer[i] == ' ')
			last_space = bytes_so_far;
		bytes_so_far++;
		if (last_space > 28) {
			strncpy(tmpbuf, start, bytes_so_far);
			tmpbuf[bytes_so_far] = '\0';
			send_comms_packet(name, 0, tmpbuf);
			strcpy(name, "-  ");
			start = &buffer[i];
			bytes_so_far = 0;
			last_space = 0;
		}
	}
	if (bytes_so_far > 0) {
		strcpy(tmpbuf, start);
		send_comms_packet(name, 0, tmpbuf);
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

static void spacemonster_move(struct snis_entity *o)
{
	/* FIXME: make this better */
	o->vx = cos(o->heading) * 10.0;
	o->vz = -sin(o->heading) * 10.0;
	o->tsd.spacemonster.zz =
		sin(M_PI *((universe_timestamp * 10 + o->id) % 360) / 180.0) * 35.0;
	set_object_location(o, o->x + o->vx, o->y + o->vy, o->z + o->vz);
	if (snis_randn(1000) < 50) {
		o->heading += (snis_randn(40) - 20) * M_PI / 180.0;
		normalize_angle(&o->heading);
	}
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

	if (fabs(o->vx) < 0.001 && fabs(o->vy) < 0.001 && fabs(o->vz) < 0.001)
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

		if (victim_id == -1) /* no nearby victims */
			return;

		i = lookup_by_id(victim_id);
		if (i >= 0) {
			double dist2;

			v = &go[i];
			dist2 = dist3dsqrd(o->x - v->x, o->y - v->y, o->z - v->z);
			if (dist2 < PATROL_ATTACK_DIST * PATROL_ATTACK_DIST)
				push_attack_mode(o, victim_id, 0);
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

	dist2 = dist3dsqrd(ship->x - cop->x, ship->y - cop->y, ship->z - cop->z);
	if (dist2 < SECURITY_RADIUS * SECURITY_RADIUS)
		ship->tsd.ship.in_secure_area = 1;
}

static int too_many_cops_around(struct snis_entity *o)
{
	space_partition_process(space_partition, o, o->x, o->z, o, count_nearby_cops);
	return o->tsd.ship.in_secure_area;
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

	n = o->tsd.ship.nai_entries - 1;
	assert(n >= 0);

	/* Don't evaluate this every tick */
	if ((universe_timestamp & 0x07) == (o->id & 0x07) &&
			calculate_threat_level(o) > THREAT_LEVEL_FLEE_THRESHOLD) {
		/* just change current mode to flee (pop + push) */
		o->tsd.ship.ai[n].ai_mode = AI_MODE_FLEE;
		o->tsd.ship.ai[n].u.flee.warp_countdown = 10 * (10 + snis_randn(10));
		return;
	}

	if (o->tsd.ship.ai[n].u.attack.victim_id == (uint32_t) -2) {
		pop_ai_attack_mode(o);
		return;
	}

	if (o->tsd.ship.ai[n].u.attack.victim_id == (uint32_t) -1) {
		int victim_id = find_nearest_victim(o);

		if (victim_id == -1) { /* no nearby victims */
			pop_ai_attack_mode(o);
			return;
		}
		o->tsd.ship.ai[n].u.attack.victim_id = victim_id;
		calculate_attack_vector(o, MIN_COMBAT_ATTACK_DIST,
						MAX_COMBAT_ATTACK_DIST);
	}
	maxv = ship_type[o->tsd.ship.shiptype].max_speed;
	v = lookup_entity_by_id(o->tsd.ship.ai[n].u.attack.victim_id);
	firing_range = 0;
	if (!v)
		return;
	if (!v->alive) {
		pop_ai_attack_mode(o);
		return;
	}

	destx = v->x + o->tsd.ship.dox;
	desty = v->y + o->tsd.ship.doy;
	destz = v->z + o->tsd.ship.doz;
	dx = destx - o->x;
	dy = desty - o->y;
	dz = destz - o->z;
	vdist = dist3d(o->x - v->x, o->y - v->y, o->z - v->z);
	if (vdist > ATTACK_MODE_GIVE_UP_DISTANCE &&
		v->type != OBJTYPE_PLANET && v->type != OBJTYPE_STARBASE) {
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
			/* continue zipping past target */
			vel.v.x = (float) o->vx;
			vel.v.y = (float) o->vy;
			vel.v.z = (float) o->vz;
			vec3_normalize(&veln, &vel);
			vec3_mul_self(&veln, 800.0f + snis_randn(600));
			o->tsd.ship.dox = veln.v.x;
			o->tsd.ship.doy = veln.v.y;
			o->tsd.ship.doz = veln.v.z;
		} else {
			calculate_attack_vector(o, MIN_COMBAT_ATTACK_DIST,
							MAX_COMBAT_ATTACK_DIST);
		}
	}

	if (!firing_range)
		return;

	notacop = o->tsd.ship.ai[0].ai_mode != AI_MODE_COP;
	imacop = !notacop;

	if (notacop && too_many_cops_around(o)) {
		pop_ai_stack(o); /* forget about attacking... do something else */
		return;
	}

	/* neutrals do not attack planets or starbases, and only select ships
	 * when attacked.
	 */
	if ((o->sdata.faction != 0 || imacop) ||
		(v->type != OBJTYPE_STARBASE && v->type != OBJTYPE_PLANET)) {

		if (snis_randn(1000) < 150 + imacop * 150 && vdist <= TORPEDO_RANGE + extra_range &&
			o->tsd.ship.next_torpedo_time <= universe_timestamp &&
			o->tsd.ship.torpedoes > 0 &&
			ship_type[o->tsd.ship.shiptype].has_torpedoes) {
			double dist, flight_time, tx, ty, tz, vx, vy, vz;
			/* int inside_nebula = in_nebula(o->x, o->y) || in_nebula(v->x, v->y); */

			if (v->type == OBJTYPE_PLANET || !planet_between_objs(o, v)) {
				dist = hypot3d(v->x - o->x, v->y - o->y, v->z - o->z);
				flight_time = dist / TORPEDO_VELOCITY;
				tx = v->x + (v->vx * flight_time);
				tz = v->z + (v->vz * flight_time);
				ty = v->y + (v->vy * flight_time);

				calculate_torpedo_velocities(o->x, o->y, o->z,
					tx, ty, tz, TORPEDO_VELOCITY, &vx, &vy, &vz);

				add_torpedo(o->x, o->y, o->z, vx, vy, vz, o->id);
				o->tsd.ship.torpedoes--;
				/* FIXME: how do the torpedoes refill? */
				o->tsd.ship.next_torpedo_time = universe_timestamp +
					ENEMY_TORPEDO_FIRE_INTERVAL;
				check_for_incoming_fire(v);
			}
		} else {
			if (snis_randn(1000) < 300 + imacop * 200 &&
				o->tsd.ship.next_laser_time <= universe_timestamp &&
				ship_type[o->tsd.ship.shiptype].has_lasers) {
				if (v->type == OBJTYPE_PLANET || !planet_between_objs(o, v)) {
					o->tsd.ship.next_laser_time = universe_timestamp +
						ENEMY_LASER_FIRE_INTERVAL;
					add_laserbeam(o->id, v->id, LASERBEAM_DURATION);
					check_for_incoming_fire(v);
				}
			}
		}
		if (v->type == OBJTYPE_SHIP1 && snis_randn(10000) < 50)
			taunt_player(o, v);
	} else {
		/* FIXME: give neutrals soemthing to do so they don't just sit there */;
	}
	if (notacop)
		check_for_nearby_targets(o);
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

	if (o->type != OBJTYPE_STARBASE && o->type != OBJTYPE_SHIP2)
		return;

	dist = dist3d(o->x - di->me->x, o->y - di->me->y, o->z - di->me->z);

	if (dist > XKNOWN_DIM / 10.0) /* too far, no effect */
		return;

	if (o->sdata.faction == di->me->sdata.faction) {
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

static void ai_flee_mode_brain(struct snis_entity *o)
{
	struct danger_info info;
	union vec3 thataway;
	int n;

	n = o->tsd.ship.nai_entries - 1;
	if (n < 0) {
		printf("n < 0 at %s:%d\n", __FILE__, __LINE__);
		return;
	}

	if ((universe_timestamp & 0x07) == (o->id & 0x07) &&
		calculate_threat_level(o) < THREAT_LEVEL_FLEE_THRESHOLD * 0.5) {
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
	vec3_normalize_self(&thataway);
	vec3_mul_self(&thataway, 4000.0);

	o->tsd.ship.dox = o->x + thataway.v.x;
	o->tsd.ship.doy = o->y + thataway.v.y;
	o->tsd.ship.doz = o->z + thataway.v.z;
	o->tsd.ship.desired_velocity = ship_type[o->tsd.ship.shiptype].max_speed;

	o->tsd.ship.ai[n].u.flee.warp_countdown--;
	if (o->tsd.ship.ai[n].u.flee.warp_countdown <= 0) {
		o->tsd.ship.ai[n].u.flee.warp_countdown = 10 * (20 + snis_randn(10));
		add_warp_effect(o->id, o->x, o->y, o->z,
			o->tsd.ship.dox, o->tsd.ship.doy, o->tsd.ship.doz);
		set_object_location(o, o->tsd.ship.dox, o->tsd.ship.doy, o->tsd.ship.doz);
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
		o->tsd.ship.ai[n].ai_mode = AI_MODE_FLEET_LEADER;
		o->tsd.ship.ai[0].u.fleet.fleet_position = 0;
		setup_patrol_route(o);
		return;
	}
	i = lookup_by_id(leader_id);
	if (i < 0)
		return;
	leader = &go[i];
	offset = fleet_position(f->fleet, position, &leader->orientation);
	o->tsd.ship.dox = offset.v.x + leader->x;
	o->tsd.ship.doy = offset.v.y + leader->y;
	o->tsd.ship.doz = offset.v.z + leader->z;
	o->tsd.ship.velocity = leader->tsd.ship.velocity * 1.05;

	dist2 = dist3dsqrd(o->x - o->tsd.ship.dox, o->y - o->tsd.ship.doy,
			o->z - o->tsd.ship.doz);

	if (dist2 > 75.0 * 75.0 && dist2 <= FLEET_WARP_DISTANCE * FLEET_WARP_DISTANCE) {
		/* catch up if too far away */
		o->tsd.ship.velocity = leader->tsd.ship.velocity * 1.5;
	} else if (dist2 > FLEET_WARP_DISTANCE * FLEET_WARP_DISTANCE && snis_randn(100) < 8) {
		/* If distance is too far, just warp */
		add_warp_effect(o->id, o->x, o->y, o->z,
			o->tsd.ship.dox, o->tsd.ship.doy, o->tsd.ship.doz);
		set_object_location(o, o->tsd.ship.dox, o->tsd.ship.doy, o->tsd.ship.doz);
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

static void ai_add_ship_movement_variety(struct snis_entity *o,
			float destx, float desty, float destz, float fractional_distance)
{
	union vec3 v, vn;
	union quat q;

	random_quat(&q);
	v.v.x = destx - o->x;
	v.v.y = desty - o->y;
	v.v.z = destz - o->z;
	vec3_mul(&v, &vn, fractional_distance);
	quat_rot_vec_self(&v, &q);
	v.v.x += vn.v.x;
	v.v.y += vn.v.y;
	v.v.z += vn.v.z;

	o->tsd.ship.dox = v.v.x + o->x;
	o->tsd.ship.doy = v.v.y + o->y;
	o->tsd.ship.doz = v.v.z + o->z;
}

static void ai_ship_warp_to(struct snis_entity *o, float destx, float desty, float destz)
{
	union vec3 v;

	v.v.x = destx - o->x;
	v.v.y = desty - o->y;
	v.v.z = destz - o->z;
	vec3_mul_self(&v, 0.90 + 0.05 * (float) snis_randn(100) / 100.0);
	if (!inside_planet(v.v.x, v.v.y, v.v.z)) {
		add_warp_effect(o->id, o->x, o->y, o->z,
			o->x + v.v.x, o->y + v.v.y, o->z + v.v.z);
		set_object_location(o, o->x + v.v.x, o->y + v.v.y, o->z + v.v.z);
		/* reset destination after warping to prevent backtracking */
		o->tsd.ship.dox = destx;
		o->tsd.ship.doy = desty;
		o->tsd.ship.doz = destz;
	}
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

	/* Check if there's a planet in the way and try to avoid it. */
	planet = planet_between_points(&startv, &endv);
	if (planet) {
		double planet_distance;
		double dx, dy, dz;
		dx = planet->x - o->x;
		dy = planet->y - o->y;
		dz = planet->z - o->z;

		/* Don't bother trying to avoid the planet unless it is close */
		planet_distance = sqrt(dx * dx + dy * dy + dz + dz);
		if (planet_distance < 2.0 * planet->tsd.planet.radius) {
			union vec3 planetv, planet_to_dest, planet_to_ship;
			union quat ship_to_dest;
			double angle;
			planetv.v.x = planet->x;
			planetv.v.y = planet->y;
			planetv.v.z = planet->z;

			/*
			 * Find vector from planet to ship, and from planet to
			 * destination and quaterion to rotate between these.
			 * vectors.
			 */
			vec3_sub(&planet_to_dest, &endv, &planetv);
			vec3_sub(&planet_to_ship, &startv, &planetv);
			vec3_normalize_self(&planet_to_dest);
			vec3_normalize_self(&planet_to_ship);
			quat_from_u2v(&ship_to_dest, &planet_to_ship, &planet_to_dest, NULL);

			/* Find a path along the great circle around planet from ship
			 * to destination at 1.5 * radius of planet.  First dest point
			 * on this path is (say) 5% around the curve from where the ship
			 * is now.
			 */
			vec3_mul_self(&planet_to_ship, planet->tsd.planet.radius * 1.5);

			/* Find the 5% of the angle of the quaternion, and change
			 * the quaternion to the new angle. */
			angle = 0.05f * 2.0f * acosf(ship_to_dest.v.w);
			ship_to_dest.v.w = cos(0.5f * angle);

			/* Find our new intermediate destination, 5% along our great circle. */
			quat_rot_vec_self(&planet_to_ship, &ship_to_dest);
			destx = planet->x + planet_to_ship.v.x;
			desty = planet->y + planet_to_ship.v.y;
			destz = planet->z + planet_to_ship.v.z;
		}
	}

	double dist2 = dist3dsqrd(o->x - destx, o->y - desty, o->z - destz);
	if (dist2 > 2000.0 * 2000.0) {
		double ld = dist3dsqrd(o->x - o->tsd.ship.dox,
				o->y - o->tsd.ship.doy, o->z - o->tsd.ship.doz);
		/* give ships some variety in movement */
		if (((universe_timestamp + o->id) & 0x3ff) == 0 || ld < 50.0 * 50.0)
			ai_add_ship_movement_variety(o, destx, desty, destz, 0.05);

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
			o->tsd.ship.dox = destx;
			o->tsd.ship.doy = desty;
			o->tsd.ship.doz = destz;
		}
		/* sometimes just warp if it's too far... */
		if (snis_randn(warproll) < ship_type[o->tsd.ship.shiptype].warpchance)
			ai_ship_warp_to(o, destx, desty, destz);
	} else {
		o->tsd.ship.dox = destx;
		o->tsd.ship.doy = desty;
		o->tsd.ship.doz = destz;
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
	quantity = ((float) *ore / 255.0) * 10;
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
		/* parent ship gone... */
		/* TODO figure out what to do here. */
		ai->mode = MINING_MODE_RETURN_TO_PARENT;
		fprintf(stderr, "mining bot's parent is gone unexpectedly\n");
		return;
	}
	parent = &go[i];
	quat_rot_vec_self(&offset, &parent->orientation);

	o->tsd.ship.dox = parent->x + offset.v.x;
	o->tsd.ship.doy = parent->y + offset.v.y;
	o->tsd.ship.doz = parent->z + offset.v.z;

	double dist2 = ai_ship_travel_towards(o, parent->x, parent->y, parent->z);
	if (dist2 < 300.0 * 300.0 && ai->mode == MINING_MODE_RETURN_TO_PARENT) {
		b = lookup_bridge_by_shipid(parent->id);
		if (b >= 0)
			send_comms_packet(o->sdata.name, bridgelist[b].npcbot.channel,
				" -- STANDING BY FOR TRANSPORT OF MATERIALS --");
		ai->mode = MINING_MODE_STANDBY_TO_TRANSPORT_ORE;
		snis_queue_add_sound(MINING_BOT_STANDING_BY,
				ROLE_COMMS | ROLE_SOUNDSERVER | ROLE_SCIENCE, parent->id);

	}
	if (dist2 < 300.0 * 300.0 && ai->mode == MINING_MODE_STOW_BOT) {
		b = lookup_bridge_by_shipid(ai->parent_ship);
		if (b >= 0) {
			channel = bridgelist[b].npcbot.channel;
			send_comms_packet(o->sdata.name, channel, "--- TRANSPORTING MATERIALS ---");
			send_comms_packet(o->sdata.name, channel, "--- MATERIALS TRANSPORTED ---");
			send_comms_packet(o->sdata.name, bridgelist[b].npcbot.channel,
				" MINING BOT STOWING AND SHUTTING DOWN");
			bridgelist[b].npcbot.current_menu = NULL;
			bridgelist[b].npcbot.special_bot = NULL;
		}
		mining_bot_unload_ores(o, parent, ai);
		delete_from_clients_and_server(o);
		snis_queue_add_sound(MINING_BOT_STOWED,
			ROLE_SOUNDSERVER | ROLE_COMMS | ROLE_SCIENCE, parent->id);
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
	struct snis_entity *asteroid;
	float threshold;
	float my_speed = dist3d(o->vx, o->vy, o->vz);
	int b;

	i = lookup_by_id(ai->asteroid);
	if (i < 0) {
		/* asteroid got blown up maybe */
		b = lookup_bridge_by_shipid(ai->parent_ship);
		if (b >= 0) {
			int channel = bridgelist[b].npcbot.channel;
			send_comms_packet(o->sdata.name, channel,
				"TARGET ASTEROID LOST -- RETURNING TO SHIP");
		}
		ai->mode = MINING_MODE_RETURN_TO_PARENT;
		return;
	}
	asteroid = &go[i];
	float distance = dist3d(o->x - asteroid->x, o->y - asteroid->y, o->z - asteroid->z);
	threshold = 2.0 * estimate_asteroid_radius(asteroid->id);
	if (my_speed < 0.1) {
		o->tsd.ship.dox = asteroid->x;
		o->tsd.ship.doy = asteroid->y;
		o->tsd.ship.doz = asteroid->z;
	} else {
		float time_to_travel = distance / my_speed;
		o->tsd.ship.dox = asteroid->x + asteroid->vx * time_to_travel;
		o->tsd.ship.doy = asteroid->y + asteroid->vy * time_to_travel;
		o->tsd.ship.doz = asteroid->z + asteroid->vz * time_to_travel;
	}
	double dist2 = ai_ship_travel_towards(o, asteroid->x, asteroid->y, asteroid->z);
	if (dist2 < threshold * threshold) {
		b = lookup_bridge_by_shipid(ai->parent_ship);
		if (b >= 0) {
			int channel = bridgelist[b].npcbot.channel;
			send_comms_packet(o->sdata.name, channel,
				"COMMENCING ORBITAL INJECTION BURN");
		}
		ai->mode = MINING_MODE_LAND_ON_ASTEROID;
		ai->countdown = 100;
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
			send_comms_packet(o->sdata.name, channel,
				"TARGET ASTEROID LOST -- RETURNING TO SHIP");
		}
		ai->mode = MINING_MODE_RETURN_TO_PARENT;
		return;
	}
	asteroid = &go[i];
	radius = estimate_asteroid_radius(asteroid->id) *
			(1.0 - 0.2 * ai->countdown / 200.0);
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
			if (asteroid->type == OBJTYPE_ASTEROID)
				send_comms_packet(o->sdata.name, channel,
					"COMMENCING MINING OPERATION");
			else if (asteroid->type == OBJTYPE_DERELICT)
				send_comms_packet(o->sdata.name, channel,
					"COMMENCING SALVAGE OPERATION");
			else
				send_comms_packet(o->sdata.name, channel,
					"COMMENCING OPERATION");
		}
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
			send_comms_packet(o->sdata.name, channel,
				"TARGET LOST -- RETURNING TO SHIP");
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
	if (b >= 0) {
		int channel = bridgelist[b].npcbot.channel;
		switch (asteroid->type) {
		case OBJTYPE_ASTEROID:
			send_comms_packet(o->sdata.name, channel,
				"MINING OPERATION COMPLETE -- RETURNING TO SHIP");
			break;
		case OBJTYPE_DERELICT:
			send_comms_packet(o->sdata.name, channel,
				"SALVAGE OPERATION COMPLETE -- RETURNING TO SHIP");
			break;
		default:
			send_comms_packet(o->sdata.name, channel,
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
		pop_ai_stack(o);
		return;
	}
	disabled_ship = &go[i];

	/* Find the starbase we are supposed to tow it to */
	i = lookup_by_id(o->tsd.ship.ai[n].u.tow_ship.starbase_dispatcher);
	if (i < 0) {
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
			dist2 = ai_ship_travel_towards(o,
				starbase_dispatcher->x, starbase_dispatcher->y, starbase_dispatcher->z);
			return;
		}
	}
}

static void ai_mining_mode_brain(struct snis_entity *o)
{
	int n = o->tsd.ship.nai_entries - 1;
	struct ai_mining_bot_data *mining_bot = &o->tsd.ship.ai[n].u.mining_bot;

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
	default:
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

	if (dist2 < 300.0 * 300.0) {
		patrol->dest = (patrol->dest + 1) % patrol->npoints;
		/* hang out here awhile... */
		n = o->tsd.ship.nai_entries;
		if (n < MAX_AI_STACK_ENTRIES) {
			o->tsd.ship.ai[n].ai_mode = AI_MODE_HANGOUT;
			o->tsd.ship.ai[n].u.hangout.time_to_go = 100 + snis_randn(100);
			o->tsd.ship.desired_velocity = 0;
			o->tsd.ship.nai_entries++;
		} else {
			d = patrol->dest;
			o->tsd.ship.dox = patrol->p[d].v.x;
			o->tsd.ship.doy = patrol->p[d].v.y;
			o->tsd.ship.doz = patrol->p[d].v.z;
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

	double dist2 = ai_ship_travel_towards(o,
				patrol->p[d].v.x, patrol->p[d].v.y, patrol->p[d].v.z);

	if (dist2 < 300.0 * 300.0) {
		patrol->dest = (patrol->dest + 1) % patrol->npoints;
		/* hang out here awhile... */
		n = o->tsd.ship.nai_entries;
		if (n < MAX_AI_STACK_ENTRIES) {
			o->tsd.ship.ai[n].ai_mode = AI_MODE_HANGOUT;
			o->tsd.ship.ai[n].u.hangout.time_to_go = 100 + snis_randn(100);
			o->tsd.ship.desired_velocity = 0;
			o->tsd.ship.nai_entries++;
		} else {
			d = patrol->dest;
			o->tsd.ship.dox = patrol->p[d].v.x;
			o->tsd.ship.doy = patrol->p[d].v.y;
			o->tsd.ship.doz = patrol->p[d].v.z;
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
			fleet_leave(o->id);
			o->tsd.ship.nai_entries = 0;
			memset(o->tsd.ship.ai, 0, sizeof(o->tsd.ship.ai));
			ship_figure_out_what_to_do(o);
			return;
		}
	}
}

static void ai_brain(struct snis_entity *o)
{
	int n;

	n = o->tsd.ship.nai_entries - 1;
	if (n < 0) {
		ship_figure_out_what_to_do(o);
		n = o->tsd.ship.nai_entries - 1;
	}
		
	/* main AI brain code is here... */
	switch (o->tsd.ship.ai[n].ai_mode) {
	case AI_MODE_ATTACK:
		ai_attack_mode_brain(o);
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
		o->tsd.ship.ai[n].u.hangout.time_to_go--;
		if (o->tsd.ship.ai[n].u.hangout.time_to_go <= 0) {
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

	dist2 = dist3dsqrd(ship->x - planet->x, ship->y - planet->y, ship->z - planet->z);
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

	if (obstacle->type == OBJTYPE_PLANET) {
		if (d < obstacle->tsd.planet.radius * obstacle->tsd.planet.radius) {
			o->alive = 0;
			return;
		}
		d -= (obstacle->tsd.planet.radius * 1.3 * obstacle->tsd.planet.radius * 1.3);
		if (d <= 0.0)
			d = 1.0;
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

	/* Compute a steering force */
	o->tsd.ship.steering_adjustment.v.x = o->x - obstacle->x;
	o->tsd.ship.steering_adjustment.v.y = o->y - obstacle->y;
	o->tsd.ship.steering_adjustment.v.z = o->z - obstacle->z;
	vec3_normalize_self(&o->tsd.ship.steering_adjustment);
	steering_magnitude = 800.0 / d;
	if (steering_magnitude > 10.0)
		steering_magnitude = 10.0;
	vec3_mul_self(&o->tsd.ship.steering_adjustment, steering_magnitude);
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

	/* Adjust velocity towards desired velocity */
	o->tsd.ship.velocity = o->tsd.ship.velocity +
			(o->tsd.ship.desired_velocity - o->tsd.ship.velocity) * 0.1;
	if (fabs(o->tsd.ship.velocity - o->tsd.ship.desired_velocity) < 0.1)
		o->tsd.ship.velocity = o->tsd.ship.desired_velocity;

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
			dist = fabsf(d->robot->x - part->x) + fabsf(d->robot->y - part->y);
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
					-MAX_ROBOT_VELOCITY / ((fabs(desired_delta_angle) * 180.0 / M_PI) + 1.0) / 3.0;
		else if (fabs(desired_delta_angle) < 40.0 * M_PI / 180.0 && dist > 200)
			o->tsd.robot.desired_velocity = 0.5 *
					-MAX_ROBOT_VELOCITY / ((fabs(desired_delta_angle) * 180.0 / M_PI) + 1.0) / 3.0;
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
		part = &d->o[i];
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
		if (fabs(o->tsd.robot.desired_velocity) > MAX_ROBOT_VELOCITY / 5.0)
			o->tsd.robot.desired_velocity =  
				(MAX_ROBOT_VELOCITY / 5.5) * o->tsd.robot.desired_velocity /
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

		/* Slower you're going, quicker you can turn */
		max_heading_change = (MAX_ROBOT_VELOCITY / fabs(o->velocity)) * 6.0 * M_PI / 180.0; 
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
			if (fabs(diff) > MAX_ROBOT_BRAKING)
				diff = MAX_ROBOT_BRAKING * diff / fabs(diff);
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
		if (fabs(o->tsd.robot.desired_velocity) > MAX_ROBOT_VELOCITY / 5.0)
			o->tsd.robot.desired_velocity =  
				(MAX_ROBOT_VELOCITY / 5.5) * o->tsd.robot.desired_velocity /
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
	o->tsd.ship.power_data.warp.i = device_power_byte_form(device);

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
		o->tsd.ship.damage.warp_damage > 240) {
		o->alive = 0;
		o->tsd.ship.damage.shield_damage = 255;
		o->timestamp = universe_timestamp;
		o->respawn_time = universe_timestamp + RESPAWN_TIME_SECS * 10;
		snis_queue_add_sound(EXPLOSION_SOUND,
				ROLE_SOUNDSERVER, o->id);
		schedule_callback(event_callback, &callback_schedule,
			"player-death-callback", o->id);
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
	double max_player_velocity =
		(MAX_PLAYER_VELOCITY * ship->tsd.ship.power_data.impulse.i) / 255;

	if (!ship->tsd.ship.reverse) {
		if (ship->tsd.ship.velocity < max_player_velocity)
			ship->tsd.ship.velocity += PLAYER_VELOCITY_INCREMENT;
	} else {
		if (ship->tsd.ship.velocity > -max_player_velocity)
			ship->tsd.ship.velocity -= PLAYER_VELOCITY_INCREMENT;
	}
	if (ship->tsd.ship.velocity > max_player_velocity ||
		ship->tsd.ship.velocity < -max_player_velocity)
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
						uint32_t docker, struct bridge_data *b,
						char *npcname, int channel)
{
	/* check if permission is already granted.... */
	int model = starbase->id % nstarbase_models;
	char msg[100];
	int i;

	for (i = 0; i < docking_port_info[model]->nports; i++) {
		if (starbase->tsd.starbase.expected_docker[i] == docker &&
			starbase->tsd.starbase.expected_docker_timer[i] > 0) {
			starbase->tsd.starbase.expected_docker_timer[i] = STARBASE_DOCK_TIME;
			/* transmit re-granting of docking permission */
			snprintf(msg, sizeof(msg), "%s, PERMISSION TO DOCK RE-GRANTED.", b->shipname);
			send_comms_packet(npcname, channel, msg);
			snis_queue_add_sound(PERMISSION_TO_DOCK_GRANTED, ROLE_NAVIGATION, b->shipid);
			return 1;
		}
	}
	/* See if there are any empty slots */
	for (i = 0; i < docking_port_info[model]->nports; i++) {
		if (starbase->tsd.starbase.expected_docker_timer[i] == 0) {
			starbase->tsd.starbase.expected_docker[i] = docker;
			starbase->tsd.starbase.expected_docker_timer[i] = STARBASE_DOCK_TIME;
			/* transmit granting of docking permission */
			snprintf(msg, sizeof(msg), "%s, PERMISSION TO DOCK GRANTED.", b->shipname);
			send_comms_packet(npcname, channel, msg);
			snis_queue_add_sound(PERMISSION_TO_DOCK_GRANTED, ROLE_NAVIGATION, b->shipid);
			return 1;
		}
	}
	if (rate_limit_docking_permission_denied(b)) {
		snprintf(msg, sizeof(msg), "%s, PERMISSION TO DOCK DENIED.", b->shipname);
		send_comms_packet(npcname, channel, msg);
		snis_queue_add_sound(PERMISSION_TO_DOCK_DENIED, ROLE_NAVIGATION, b->shipid);
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

static void init_player(struct snis_entity *o, int clear_cargo_bay, float *charges);
static void do_docking_action(struct snis_entity *ship, struct snis_entity *starbase,
			struct bridge_data *b, char *npcname)
{
	char msg[100];
	float charges;
	int channel = b->npcbot.channel;

	snprintf(msg, sizeof(msg), "%s, WELCOME TO OUR STARBASE, ENJOY YOUR STAY.", b->shipname);
	send_comms_packet(npcname, channel, msg);
	/* TODO make the repair/refuel process a bit less easy */
	snprintf(msg, sizeof(msg), "%s, YOUR SHIP HAS BEEN REPAIRED AND REFUELED.\n",
		b->shipname);
	init_player(ship, 0, &charges);
	send_ship_damage_packet(ship);
	ship->timestamp = universe_timestamp;
	snis_queue_add_sound(DOCKING_SOUND, ROLE_ALL, ship->id);
	snis_queue_add_sound(WELCOME_TO_STARBASE, ROLE_NAVIGATION, ship->id);
	send_comms_packet(npcname, channel, msg);
	snprintf(msg, sizeof(msg), "%s, YOUR ACCOUNT HAS BEEN BILLED $%5.2f\n",
		b->shipname, charges);
	send_comms_packet(npcname, channel, msg);
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
	char msg[100];
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
	npcname = sb->tsd.starbase.name;
	if (!starbase_expecting_docker(sb, player->id)) {
		if (rate_limit_docking_permission_denied(bridge)) {
			snprintf(msg, sizeof(msg), "%s, YOU ARE NOT CLEARED TO DOCK\n",
				bridge->shipname);
			send_comms_packet(npcname, channel, msg);
			snis_queue_add_sound(PERMISSION_TO_DOCK_DENIED, ROLE_NAVIGATION, bridge->shipid);
		}
		return;
	}
	if (docking_port->tsd.docking_port.docked_guy == -1) {
		docking_port->tsd.docking_port.docked_guy = player->id;
		do_docking_action(player, sb, bridge, npcname);
	} else {
		if (rate_limit_docking_permission_denied(bridge)) {
			snprintf(msg, sizeof(msg), "%s, YOU ARE NOT CLEARED FOR DOCKING\n",
				bridge->shipname);
			send_comms_packet(npcname, channel, msg);
			snis_queue_add_sound(PERMISSION_TO_DOCK_DENIED, ROLE_NAVIGATION, bridge->shipid);
		}
	}
}

static void do_collision_impulse(struct snis_entity *player, struct snis_entity *object)
{
	union vec3 p1, p2, v1, v2, vo1, vo2;
	float m1, m2, r1, r2;
	const float energy_transmission_factor = 0.7;

	m1 = 10;
	m2 = 20;
	r1 = 10;
	r2 = 10;

	p1.v.x = player->x;
	p1.v.y = player->y;
	p1.v.z = player->z;

	v1.v.x = player->vx;
	v1.v.y = player->vy;
	v1.v.z = player->vz;

	p2.v.x = object->x;
	p2.v.y = object->y;
	p2.v.z = object->z;

	v2.v.x = object->vx;
	v2.v.y = object->vy;
	v2.v.z = object->vz;

	elastic_collision(m1, &p1, &v1, r1, m2, &p2, &v2, r2,
				energy_transmission_factor, &vo1, &vo2);

	player->vx = vo1.v.x;
	player->vy = vo1.v.y;
	player->vz = vo1.v.z;

	object->vx = vo2.v.x;
	object->vy = vo2.v.y;
	object->vz = vo2.v.z;

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

	obb->e[0] = block->tsd.block.sx * 0.5;
	obb->e[1] = block->tsd.block.sy * 0.5;
	obb->e[2] = block->tsd.block.sz * 0.5;
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
	case OBJTYPE_LASER:
	case OBJTYPE_EXPLOSION:
	case OBJTYPE_NEBULA:
	case OBJTYPE_WORMHOLE:
	case OBJTYPE_SPACEMONSTER:
	case OBJTYPE_STARBASE:
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

	dist2 = dist3dsqrd(o->x - t->x, o->y - t->y, o->z - t->z);
	if (t->type == OBJTYPE_CARGO_CONTAINER && dist2 < 150.0 * 150.0) {
			scoop_up_cargo(o, t);
			return;
	}
	if (t->type == OBJTYPE_DOCKING_PORT && dist2 < 50.0 * 50.0 &&
		o->tsd.ship.docking_magnets) {
		player_attempt_dock_with_starbase(t, o);
	}
	if (t->type == OBJTYPE_WARPGATE && dist2 < 50.0 * 50.0) {
		if (player_attempt_warpgate_jump(t, o))
			return;
	}
	if (t->type == OBJTYPE_PLANET) {
		const float surface_dist2 = t->tsd.planet.radius * t->tsd.planet.radius;
		const float warn_dist2 = (t->tsd.planet.radius + PLAYER_PLANET_DIST_WARN) *
					(t->tsd.planet.radius + PLAYER_PLANET_DIST_WARN);
		const float too_close2 = (t->tsd.planet.radius + PLAYER_PLANET_DIST_TOO_CLOSE) *
					(t->tsd.planet.radius + PLAYER_PLANET_DIST_TOO_CLOSE);

		/* check security level ... */
		if (t->tsd.planet.security > o->tsd.ship.in_secure_area &&
			dist2 < SECURITY_RADIUS * SECURITY_RADIUS)
			o->tsd.ship.in_secure_area = t->tsd.planet.security;

		if (dist2 < surface_dist2)  {
			/* crashed into planet */
			o->alive = 0;
			o->timestamp = universe_timestamp;
			o->respawn_time = universe_timestamp + RESPAWN_TIME_SECS * 10;
			send_ship_damage_packet(o);
			snis_queue_add_sound(EXPLOSION_SOUND,
					ROLE_SOUNDSERVER, o->id);
			schedule_callback(event_callback, &callback_schedule,
					"player-death-callback", o->id);
		} else if (dist2 < too_close2 && (universe_timestamp & 0x7) == 0) {
			(void) add_explosion(o->x + o->vx * 2, o->y + o->vy * 2, o->z + o->vz * 2,
				20, 10, 50, OBJTYPE_SPARK);
			calculate_atmosphere_damage(o);
			send_ship_damage_packet(o);
			send_packet_to_all_clients_on_a_bridge(o->id,
				snis_opcode_pkt("b", OPCODE_ATMOSPHERIC_FRICTION),
					ROLE_SOUNDSERVER | ROLE_NAVIGATION);
		} else if (dist2 < warn_dist2 && (universe_timestamp & 0x7) == 0) {
			(void) add_explosion(o->x + o->vx * 2, o->y + o->vy * 2, o->z + o->vz * 2,
				5, 5, 50, OBJTYPE_SPARK);
			calculate_atmosphere_damage(o);
			send_ship_damage_packet(o);
			send_packet_to_all_clients_on_a_bridge(o->id,
				snis_opcode_pkt("b", OPCODE_ATMOSPHERIC_FRICTION),
					ROLE_SOUNDSERVER | ROLE_NAVIGATION);
		}
		return;
	}
	if (t->type == OBJTYPE_BLOCK) {
		union vec3 my_ship, closest_point, displacement;

		if (dist2 > t->tsd.block.radius * t->tsd.block.radius)
			return;

		my_ship.v.x = o->x;
		my_ship.v.y = o->y;
		my_ship.v.z = o->z;

		oriented_bounding_box_closest_point(&my_ship, &t->tsd.block.obb, &closest_point);

		dist2 = dist3dsqrd(o->x - closest_point.v.x, o->y - closest_point.v.y, o->z - closest_point.v.z);
		if (dist2 > 8.0 * 8.0)
			return;
		printf("BLOCK COLLISION DETECTION:HIT, dist2 = %f\n", sqrt(dist2));
		snis_queue_add_sound(HULL_CREAK_0 + (snis_randn(1000) % NHULL_CREAK_SOUNDS),
							ROLE_SOUNDSERVER, o->id);
		o->vx = 0;
		o->vy = 0;
		o->vz = 0;

		displacement.v.x = o->x - closest_point.v.x;
		displacement.v.y = o->y - closest_point.v.y;
		displacement.v.z = o->z - closest_point.v.z;
		vec3_normalize_self(&displacement);
		vec3_mul_self(&displacement, 30.0);
		o->x += displacement.v.x;
		o->y += displacement.v.y;
		o->z += displacement.v.z;
		(void) add_explosion(closest_point.v.x, closest_point.v.y, closest_point.v.z, 85, 5, 20, OBJTYPE_SPARK);
		return;
	}
	proximity_dist2 = PROXIMITY_DIST2;
	crash_dist2 = CRASH_DIST2;
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
			vec3_mul_self(&p, planet->tsd.planet.radius * STANDARD_ORBIT_RADIUS_FACTOR);
			d.v.x = o->vx;
			d.v.y = o->vy;
			d.v.z = o->vz;
			vec3_add_self(&d, &p);
			vec3_normalize_self(&d);
			vec3_mul_self(&d, planet->tsd.planet.radius * STANDARD_ORBIT_RADIUS_FACTOR);
			vec3_sub_self(&d, &p);
			vec3_normalize_self(&d);
			vec3_mul_self(&d, planet->tsd.planet.radius * 2.5 / M_PI); /* 2.5 degrees */
			vec3_add_self(&d, &p);
			d.v.x += planet->x;
			d.v.y += planet->y;
			d.v.z += planet->z;

			/* Compute new orientation to point us at d */
			/* Calculate new desired orientation of ship pointing towards destination */
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

static void update_ship_position_and_velocity(struct snis_entity *o)
{
	union vec3 desired_velocity;
	union vec3 dest, destn;
	struct snis_entity *v;
	int n, mode;

	if (o->tsd.ship.nai_entries == 0)
		ship_figure_out_what_to_do(o);
	n = o->tsd.ship.nai_entries - 1;
	mode = o->tsd.ship.ai[n].ai_mode;

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
	}

	if (vec3_len2(&dest) > 0.0001) {
		vec3_normalize(&destn, &dest);
	} else {
		destn.v.x = 0.0f;
		destn.v.y = 0.0f;
		destn.v.z = 0.0f;
	}

	/* Construct vector of desired velocity */
	desired_velocity.v.x = destn.v.x * o->tsd.ship.velocity; 
	desired_velocity.v.y = destn.v.y * o->tsd.ship.velocity;
	desired_velocity.v.z = destn.v.z * o->tsd.ship.velocity;

	/* apply braking and steering adjustments */
	vec3_mul_self(&desired_velocity, o->tsd.ship.braking_factor);
	vec3_add_self(&desired_velocity, &o->tsd.ship.steering_adjustment);

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

	/* Move ship */
	set_object_location(o, o->x + o->vx, o->y + o->vy, o->z + o->vz);
	/* FIXME: some kind of collision detection needed here... */

	if (mode == AI_MODE_HANGOUT) {
		o->vx *= 0.8;
		o->vy *= 0.8;
		o->vz *= 0.8;
		o->tsd.ship.desired_velocity = 0;
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
		sunburn = 0.0;
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
		o->respawn_time = universe_timestamp + RESPAWN_TIME_SECS * 10;
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
		o->vx = 0;
		o->vy = 0;
		o->vz = 0;
	}
}
		
static void player_move(struct snis_entity *o)
{
	int desired_rpm, desired_temp, diff;
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
	o->tsd.ship.overheating_damage_done = do_overheating_damage(o);
	o->tsd.ship.overheating_damage_done |= do_sunburn_damage(o);
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
	if (o->tsd.ship.oxygen < UINT32_MAX - new_oxygen)
		o->tsd.ship.oxygen += new_oxygen;
	else
		o->tsd.ship.oxygen = UINT32_MAX;
	update_player_orientation(o);
	update_player_sciball_orientation(o);
	update_player_weap_orientation(o);
	update_player_position_and_velocity(o);
	
	o->tsd.ship.sci_heading += o->tsd.ship.sci_yaw_velocity;
	o->tsd.ship.shields = universe_timestamp % 100;

	normalize_angle(&o->tsd.ship.sci_heading);
	o->timestamp = universe_timestamp;

	orientation_damping = PLAYER_ORIENTATION_DAMPING +
			(0.98 - PLAYER_ORIENTATION_DAMPING) * o->tsd.ship.nav_damping_suppression;
	damp_yaw_velocity(&o->tsd.ship.yaw_velocity, orientation_damping);
	damp_yaw_velocity(&o->tsd.ship.pitch_velocity, orientation_damping);
	damp_yaw_velocity(&o->tsd.ship.roll_velocity, orientation_damping);
	damp_yaw_velocity(&o->tsd.ship.gun_yaw_velocity, GUN_YAW_DAMPING);
	damp_yaw_velocity(&o->tsd.ship.sci_yaw_velocity, SCI_YAW_DAMPING);
	damp_yaw_velocity(&o->tsd.ship.sciball_yawvel, YAW_DAMPING);
	damp_yaw_velocity(&o->tsd.ship.sciball_pitchvel, PITCH_DAMPING);
	damp_yaw_velocity(&o->tsd.ship.sciball_rollvel, ROLL_DAMPING);
	damp_yaw_velocity(&o->tsd.ship.weap_yawvel, YAW_DAMPING);
	damp_yaw_velocity(&o->tsd.ship.weap_pitchvel, PITCH_DAMPING);

	/* Damp velocity */
	velocity_damping = PLAYER_VELOCITY_DAMPING +
		(1.0 - PLAYER_VELOCITY_DAMPING) * o->tsd.ship.nav_damping_suppression;

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
		o->respawn_time = universe_timestamp + RESPAWN_TIME_SECS * 10;
		send_ship_damage_packet(o);
	}
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
	damp_yaw_velocity(&o->tsd.ship.gun_yaw_velocity, GUN_YAW_DAMPING);
	damp_yaw_velocity(&o->tsd.ship.sci_yaw_velocity, SCI_YAW_DAMPING);

	/* Damp velocity */
	if (fabs(o->tsd.ship.velocity) < MIN_PLAYER_VELOCITY)
		o->tsd.ship.velocity = 0.0;
	else
		o->tsd.ship.velocity *= PLAYER_VELOCITY_DAMPING;
}

static void coords_to_location_string(double x, double z, char *buffer, int buflen)
{
	int sectorx, sectorz;
	static char *military_alphabet[] = {
		"ALPHA", "BRAVO", "CHARLIE", "DELTA", "ECHO",
		"FOXTROT", "GOLF", "HOTEL", "INDIA", "JULIETT",
	};
	static char *military_numerals[] = {
		"ZERO ZERO", "ZERO ONE", "ZERO TWO", "ZERO THREE",
		"ZERO FOUR", "ZERO FIVE",
		"ZERO SIX", "ZERO SEVEN", "ZERO EIGHT", "ZERO NINER", "ONE ZERO", };

	sectorx = floor((x / (double) XKNOWN_DIM) * 10.0);
	sectorz = floor((z / (double) ZKNOWN_DIM) * 10.0);

	if (sectorx >= 0 && sectorx <= 9 && sectorz >= 0 && sectorz <= 10)
		snprintf(buffer, buflen, "SECTOR %s %s",
			military_alphabet[sectorx], military_numerals[sectorz]);
	else
		snprintf(buffer, buflen, "(%8.2lf, %8.2lf)", x, z);
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
	const float motion_damping_factor = 0.1;
	const float slerp_rate = 0.05;

	if (o->tsd.docking_port.docked_guy == (uint32_t) -1)
		return;
	i = lookup_by_id(o->tsd.docking_port.docked_guy);
	if (i < 0)
		return;
	quat_rot_vec_self(&offset, &o->orientation);
	docker = &go[i];
	if (!docker->tsd.ship.docking_magnets) {
		o->tsd.docking_port.docked_guy = (uint32_t) -1;
		revoke_docking_permission(o, docker->id);
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
	block_calculate_obb(o, &o->tsd.block.obb);
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
	block_calculate_obb(o, &o->tsd.block.obb);
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
			uint32_t n = lookup_by_id(go[root].tsd.block.naughty_list[i]);
			if (n < 0)
				continue;
			double dist2 = dist3dsqrd(o->x - go[n].x, o->y - go[n].y, o->z - go[n].z);
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

static void starbase_move(struct snis_entity *o)
{
	char buf[100], location[50];
	int64_t then, now;
	int i, j, model;
	int fired_at_player = 0;
	static struct mtwist_state *mt = NULL;

	if (!mt)
		mt = mtwist_init(mtwist_seed);

	spin_starbase(o);
	starbase_update_docking_ports(o);
	then = o->tsd.starbase.last_time_called_for_help;
	now = universe_timestamp;
	if (o->tsd.starbase.under_attack &&
		(then < now - 2000 || 
		o->tsd.starbase.last_time_called_for_help == 0)) {
		o->tsd.starbase.last_time_called_for_help = universe_timestamp;
		// printf("starbase name = '%s'\n", o->tsd.starbase.name);
		sprintf(buf, "STARBASE %s:", o->sdata.name);
		send_comms_packet("", 0, buf);
		send_comms_packet("-  ", 0, starbase_comm_under_attack());
		coords_to_location_string(o->x, o->z, location, sizeof(location) - 1);
		sprintf(buf, "LOCATION %s", location);
		send_comms_packet("-  ", 0, buf);
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
		float dist2 = dist3dsqrd(o->x - a->x, o->y - a->y, o->z - a->z);
		if (dist2 > TORPEDO_RANGE * TORPEDO_RANGE)
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
			float flight_time = dist / TORPEDO_VELOCITY;

			tx = a->x + (a->vx * flight_time);
			tz = a->z + (a->vz * flight_time);
			ty = a->y + (a->vy * flight_time);
			calculate_torpedo_velocities(o->x, o->y, o->z,
				tx, ty, tz, TORPEDO_VELOCITY, &vx, &vy, &vz);
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
			char msg[300];
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
						snprintf(msg, sizeof(msg), ":  %s,  %s", a->sdata.name, lastc);
					else
						snprintf(msg, sizeof(msg), ":  %s", lastc);
					c++;
					send_comms_packet(o->tsd.starbase.name, 0, msg);
				} else {
					snprintf(msg, sizeof(msg), ":  %s", lastc);
					send_comms_packet(o->tsd.starbase.name, 0, msg);
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
			char msg[100];
			/* TODO: make this safe */
			int bn = lookup_bridge_by_shipid(id);
			if (bn < 0)
				continue;
			struct bridge_data *b = &bridgelist[bn];
			o->tsd.starbase.expected_docker[j] = -1;
			snprintf(msg, sizeof(msg), "%s, PERMISSION TO DOCK EXPIRED.", b->shipname);
			send_comms_packet(o->tsd.starbase.name, b->npcbot.channel, msg);
			snis_queue_add_sound(PERMISSION_TO_DOCK_EXPIRED, ROLE_NAVIGATION, b->shipid);
		}
	}
}

static void warpgate_move(struct snis_entity *o)
{
	o->timestamp = universe_timestamp;
}

static void explosion_move(struct snis_entity *o)
{
	if (o->alive > 0)
		o->alive--;

	if (o->alive <= 0)
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
}	
	
static void init_player(struct snis_entity *o, int clear_cargo_bay, float *charges)
{
	int i;
	float money = 0.0;
#define TORPEDO_UNIT_COST 50.0f
#define SHIELD_UNIT_COST 5.0f;
#define FUEL_UNIT_COST (1500.0f / (float) UINT32_MAX)
	o->move = player_move;
	money += (INITIAL_TORPEDO_COUNT - o->tsd.ship.torpedoes) * TORPEDO_UNIT_COST;
	o->tsd.ship.torpedoes = INITIAL_TORPEDO_COUNT;
	money += (100.0 - o->tsd.ship.shields) * SHIELD_UNIT_COST;
	o->tsd.ship.shields = 100.0;
	o->tsd.ship.power = 100.0;
	o->tsd.ship.yaw_velocity = 0.0;
	o->tsd.ship.pitch_velocity = 0.0;
	o->tsd.ship.roll_velocity = 0.0;
	o->tsd.ship.gun_yaw_velocity = 0.0;
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
	o->tsd.ship.phaser_bank_charge = 0;
	o->tsd.ship.scizoom = 0;
	o->tsd.ship.weapzoom = 0;
	o->tsd.ship.navzoom = 0;
	o->tsd.ship.mainzoom = 0;
	o->tsd.ship.warpdrive = 0;
	o->tsd.ship.requested_warpdrive = 0;
	o->tsd.ship.requested_shield = 0;
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
	o->tsd.ship.overheating_damage_done = 0;
	o->tsd.ship.ncargo_bays = 8;
	o->tsd.ship.passenger_berths = 2;
	o->tsd.ship.mining_bots = 1;
	o->tsd.ship.emf_detector = 0;
	o->tsd.ship.nav_mode = NAV_MODE_NORMAL;
	o->tsd.ship.orbiting_object_id = 0xffffffff;
	o->tsd.ship.nav_damping_suppression = 0.0;
	quat_init_axis(&o->tsd.ship.computer_desired_orientation, 0, 1, 0, 0);
	o->tsd.ship.computer_steering_time_left = 0;
	if (clear_cargo_bay) {
		/* The clear_cargo_bay param is a stopgap until real docking code
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
		o->tsd.ship.wallet = INITIAL_WALLET_MONEY;
	}
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

		o->tsd.ship.velocity = MAX_PLAYER_VELOCITY;
		boost.v.x = MAX_PLAYER_VELOCITY * 4.0;
		boost.v.y = MAX_PLAYER_VELOCITY * 4.0;
		boost.v.z = MAX_PLAYER_VELOCITY * 4.0;
		quat_rot_vec_self(&boost, &o->orientation);
		o->x += boost.v.x;
		o->y += boost.v.y;
		o->z += boost.v.z;
	}
	o->alive = 1;
}

static int add_player(double x, double z, double vx, double vz, double heading, uint8_t warpgate_number)
{
	int i;

	i = add_generic_object(x, 0.0, z, vx, 0.0, vz, heading, OBJTYPE_SHIP1);
	if (i < 0)
		return i;
	respawn_player(&go[i], warpgate_number);
	return i;
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

static int commodity_sample(void);
static int add_ship(int faction, int auto_respawn)
{
	int i, cb;
	double x, y, z, heading;
	int st;
	static struct mtwist_state *mt = NULL;

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
	go[i].tsd.ship.shields = 100.0;
	go[i].tsd.ship.power = 100.0;
	go[i].tsd.ship.yaw_velocity = 0.0;
	go[i].tsd.ship.pitch_velocity = 0.0;
	go[i].tsd.ship.roll_velocity = 0.0;
	go[i].tsd.ship.desired_velocity = 0;
	go[i].tsd.ship.velocity = 0;
	st = snis_randn(nshiptypes);
	go[i].tsd.ship.shiptype = st;
	go[i].sdata.shield_strength = ship_type[st].max_shield_strength;
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
	go[i].tsd.ship.ncargo_bays = ship_type[st].ncargo_bays;
	for (cb = 0; cb < go[i].tsd.ship.ncargo_bays; cb++) {
		int item = commodity_sample();
		float qty = (float) snis_randn(100);
		go[i].tsd.ship.cargo[cb].contents.item = item;
		go[i].tsd.ship.cargo[cb].contents.qty = qty;
		if (snis_randn(10000) < 2000)
			break;
	}
	go[i].tsd.ship.home_planet = choose_ship_home_planet();
	go[i].tsd.ship.auto_respawn = (uint8_t) auto_respawn;
	quat_init_axis(&go[i].tsd.ship.computer_desired_orientation, 0, 1, 0, 0);
	go[i].tsd.ship.computer_steering_time_left = 0;
	go[i].tsd.ship.orbiting_object_id = 0xffffffff;
	memset(go[i].tsd.ship.cargo, 0, sizeof(go[i].tsd.ship.cargo));
	if (faction >= 0 && faction < nfactions())
		go[i].sdata.faction = faction;
	if (st == SHIP_CLASS_MANTIS) /* Ensure all Mantis tow ships are neutral faction */
		faction = 0;
	ship_name(mt, go[i].sdata.name, sizeof(go[i].sdata.name));
	uppercase(go[i].sdata.name);
	return i;
}

static int add_mining_bot(struct snis_entity *parent_ship, uint32_t asteroid_id)
{
	int rc;
	struct snis_entity *o;

	rc = add_ship(parent_ship->sdata.faction, 0);
	/* TODO may want to make a special model just for the mining bot */
	if (rc < 0)
		return rc;
	o = &go[rc];
	parent_ship->tsd.ship.mining_bots--; /* maybe we want miningbots to live in cargo hold? */
	o->tsd.ship.shiptype = SHIP_CLASS_ASTEROIDMINER;
	push_mining_bot_mode(o, parent_ship->id, asteroid_id);
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

	i = add_ship(-1, auto_respawn);
	if (i < 0)
		return i;
	set_object_location(&go[i], x, y, z);
	go[i].tsd.ship.shiptype = shiptype % nshiptypes;
	go[i].sdata.faction = the_faction % nfactions();
	strncpy(go[i].sdata.name, name, sizeof(go[i].sdata.name) - 1);
	return i;
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
	i = add_ship(-1, 1);
	lua_pushnumber(lua_state, i >= 0 ? (double) go[i].id : -1.0);
	pthread_mutex_unlock(&universe_mutex);
	return 1;
}

static int add_spacemonster(double x, double y, double z)
{
	int i;
	double heading;

	heading = degrees_to_radians(0.0 + snis_randn(360)); 
	i = add_generic_object(x, y, z, 0.0, 0.0, 0.0, heading, OBJTYPE_SPACEMONSTER);
	if (i < 0)
		return i;
	go[i].tsd.spacemonster.zz = 0.0;
	go[i].move = spacemonster_move;
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
		qty = (float) snis_randn(100);
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
		mkt[i].qty = snis_randn(100); /* TODO: something better */
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
				uint8_t block_material_index)
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
	go[i].tsd.block.sx = sx;
	go[i].tsd.block.sy = sy;
	go[i].tsd.block.sz = sz;
	go[i].tsd.block.health = 255; /* immortal */
	go[i].tsd.block.rotational_velocity = random_spin[go[i].id % NRANDOM_SPINS];
	go[i].tsd.block.relative_orientation = relative_orientation;
	go[i].tsd.block.radius = mesh_compute_nonuniform_scaled_radius(unit_cube_mesh, sx, sy, sz);
	go[i].tsd.block.block_material_index = block_material_index;
	memset(&go[i].tsd.block.naughty_list, 0xff, sizeof(go[i].tsd.block.naughty_list));
	go[i].move = block_move;
	block_calculate_obb(&go[i], &go[i].tsd.block.obb);
	return i;
}

static int l_add_block(lua_State *l)
{
	double rid, x, y, z, sx, sy, sz, rotx, roty, rotz, angle, material_index;
	double dx, dy, dz;
	uint32_t parent_id;
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
	dx = 0.0;
	dy = 0.0;
	dz = 0.0;

	if ((int) material_index != 0 && (int) material_index != 1) {
		lua_pushnumber(lua_state, -1.0);
		return 1;
	}
	quat_init_axis(&rotation, rotx, roty, rotz, angle);

	pthread_mutex_lock(&universe_mutex);
	parent_id = (uint32_t) rid;
	i = lookup_by_id(parent_id);
	if (i >= 0) {
		dx = x;
		dy = y;
		dz = z;
		x = 0.0;
		y = 0.0;
		z = 0.0;
	}
	i = add_block_object(parent_id, x, y, z, 0.0, 0.0, 0.0, dx, dy, dz, sx, sy, sz, rotation,
				(int) material_index);
	lua_pushnumber(lua_state, i < 0 ? -1.0 : (double) go[i].id);
	pthread_mutex_unlock(&universe_mutex);
	return 1;
}

static int add_subblock(int parent_id, double scalefactor, double sx, double sy, double sz, /* nonuniform scaling */
			double dx, double dy, double dz, /* displacement from parent */
			uint8_t block_material_index)
{
	const double s = scalefactor;
	int i;

	i = add_block_object(parent_id, 0, 0, 0, 0, 0, 0,
				dx * s, dy * s, dz * s,
				sx * s, sy * s, sz * s, identity_quat,
				block_material_index);
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
	xo = xoff[face] * block->tsd.block.sx * 0.5 + turret_offset * xoff[face];
	yo = yoff[face] * block->tsd.block.sy * 0.5 + turret_offset * yoff[face];
	zo = zoff[face] * block->tsd.block.sz * 0.5 + turret_offset * zoff[face];

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
		yrowstep = block->tsd.block.sy / (1.0 + rows);
		zcolstep = block->tsd.block.sz / (1.0 + cols);
		printf("yrowstep = %lf\n", yrowstep);
		printf("zcolstep = %lf\n", zcolstep);
		printf("block->tsd.block.sy = %lf\n", block->tsd.block.sy);
		printf("block->tsd.block.sz = %lf\n", block->tsd.block.sz);
	} else if (fabs(yo) > 0.01) {
		printf("x z plane\n");
		/* x,z  plane */
		xrowstep = block->tsd.block.sx / (1.0 + rows);
		zcolstep = block->tsd.block.sz / (1.0 + cols);
	} else {
		/* x,y  plane */
		printf("x y plane\n");
		xrowstep = block->tsd.block.sx / (1.0 + rows);
		ycolstep = block->tsd.block.sy / (1.0 + cols);
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
						x + platformxo, y + platformyo, z + platformzo, 1);
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
	vec3_add_self(&pos, &dp[model].port[portnumber].pos);
	quat_mul(&orientation, &parent->orientation, &dp[model].port[portnumber].orientation);

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
	fabricate_prices(&go[i]);
	init_starbase_market(&go[i]);
	/* FIXME, why name stored twice? probably just use sdata.name is best
	 * but might be because we should know starbase name even if science
	 * doesn't scan it.
	 */
	sprintf(go[i].tsd.starbase.name, "SB-%02d", n);
	sprintf(go[i].sdata.name, "SB-%02d", n);

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

static int add_explosion(double x, double y, double z, uint16_t velocity,
				uint16_t nsparks, uint16_t time, uint8_t victim_type)
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
	return i;
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

	/* only deal laser damage every other tick */
	if (universe_timestamp & 0x01)
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
	
	if (ttype == OBJTYPE_STARBASE) {
		target->tsd.starbase.under_attack = 1;
		add_starbase_attacker(target, o->tsd.laserbeam.origin);
		calculate_laser_starbase_damage(target, o->tsd.laserbeam.wavelength);
		notify_the_cops(o);
	}

	if (ttype == OBJTYPE_SHIP1 || ttype == OBJTYPE_SHIP2) {
		calculate_laser_damage(target, o->tsd.laserbeam.wavelength,
					(float) o->tsd.laserbeam.power);
		send_ship_damage_packet(target);
		attack_your_attacker(target, lookup_entity_by_id(o->tsd.laserbeam.origin));
		notify_the_cops(o);
	}

	if (ttype == OBJTYPE_TURRET) {
		calculate_laser_damage(target, o->tsd.laserbeam.wavelength,
					(float) o->tsd.laserbeam.power);
	}

	if (ttype == OBJTYPE_ASTEROID)
		target->alive = 0;

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

	if (o->alive <= 0) {
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

	dist = hypot3d(target->x - origin->x, target->y - origin->y, target->z - origin->z);

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
	go[i].alive = TORPEDO_LIFETIME;
	go[i].tsd.torpedo.ship_id = ship_id;
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
		for (j = 0; j < NASTEROIDS / NASTEROID_CLUSTERS; j++) {
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
			int shiptype, int the_faction, int persistent)
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
	return i;
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
	i = add_derelict(name, x, y, z, vx, vy, vz, shiptype, the_faction, 1);
	lua_pushnumber(lua_state, i < 0 ? -1.0 : (double) go[i].id);
	pthread_mutex_unlock(&universe_mutex);
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
	else if (strcmp(solarsystem_assets->planet_type[planet_type], "earth-like") == 0)
		atm_type = earthlike_atmosphere_type;
	else if (strcmp(solarsystem_assets->planet_type[planet_type], "gas-giant") == 0)
		atm_type = gas_giant_atmosphere_type;
	else
		atm_type = gas_giant_atmosphere_type;
	return random_planetary_atmosphere_by_type(NULL, atm_type, atm_instance);
}

static int add_planet(double x, double y, double z, float radius, uint8_t security)
{
	int i, sst;

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
	go[i].move = generic_move;
	go[i].tsd.planet.government = snis_randn(1000) % ARRAYSIZE(government_name);
	go[i].tsd.planet.economy = snis_randn(1000) % ARRAYSIZE(economy_name);
	go[i].tsd.planet.tech_level = snis_randn(1000) % ARRAYSIZE(tech_level_name);
	go[i].tsd.planet.description_seed = snis_rand();
	go[i].tsd.planet.radius = radius;
	go[i].tsd.planet.ring = snis_randn(100) < 50;
	sst = (uint8_t) (go[i].id % solarsystem_assets->nplanet_textures);
	go[i].tsd.planet.solarsystem_planet_type = sst;
	go[i].tsd.planet.has_atmosphere = has_atmosphere(go[i].tsd.planet.solarsystem_planet_type);
	go[i].tsd.planet.atmosphere_type = select_atmospheric_profile(&go[i]);
	go[i].tsd.planet.ring_selector = snis_randn(256);
	go[i].tsd.planet.security = security;
	go[i].tsd.planet.contraband = choose_contraband();
	go[i].tsd.planet.atmosphere_r = solarsystem_assets->atmosphere_color[sst].r;
	go[i].tsd.planet.atmosphere_g = solarsystem_assets->atmosphere_color[sst].g;
	go[i].tsd.planet.atmosphere_b = solarsystem_assets->atmosphere_color[sst].b;
	go[i].tsd.planet.atmosphere_scale = 1.03;

	return i;
}

static int l_add_planet(lua_State *l)
{
	double x, y, z, r, s;
	const char *name;
	int i;

	name = lua_tostring(lua_state, 1);
	x = lua_tonumber(lua_state, 2);
	y = lua_tonumber(lua_state, 3);
	z = lua_tonumber(lua_state, 4);
	r = lua_tonumber(lua_state, 5);
	s = lua_tonumber(lua_state, 6);

	if (r < MIN_PLANET_RADIUS)
		r = MIN_PLANET_RADIUS;
	if (r > MAX_PLANET_RADIUS)
		r = MAX_PLANET_RADIUS;

	pthread_mutex_lock(&universe_mutex);
	i = add_planet(x, y, z, r, (uint8_t) s);
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
	double mindist = -1.0;
	double dist;

	for (i = 0; i <= snis_object_pool_highest_object(pool); i++) {
		struct snis_entity *o = &go[i];
		if (o->type != OBJTYPE_PLANET)
			continue;
		dist = dist3d(o->x - x, o->y - y, o->z - z);
		if (mindist < 0 || dist < mindist)
			mindist = dist;
	}

	dist = dist3d(x - SUNX, y - SUNY, z - SUNZ);
	if (dist < SUN_DIST_LIMIT) /* Too close to sun? */
		return 1;

	if (mindist < 0)
		return 0;
	if (mindist > limit)
		return 0;
	return 1;
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
		add_planet(x, y, z, radius, i < 4 ? HIGH_SECURITY : LOW_SECURITY);
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
	sprintf(go[i].tsd.starbase.name, "WG-%02d", n);
	sprintf(go[i].sdata.name, "WG-%02d", n);
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

	for (i = 0; i < NESHIPS; i++)
		add_ship(i % nfactions(), 1);
}

static void add_enforcers_to_planet(struct snis_entity *p)
{
	int nenforcers = p->tsd.planet.security * 2;
	int x;

	for (int i = 0; i < nenforcers; i++) {
		x = add_ship(p->sdata.faction, 1);
		if (x < 0)
			continue;
		go[x].tsd.ship.shiptype = SHIP_CLASS_ENFORCER;
		go[x].tsd.ship.home_planet = p->id;
		snprintf(go[x].sdata.name, sizeof(go[i].sdata.name), "POLICE-%02d",
			snis_randn(100));
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

static void make_universe(void)
{
	initialize_random_orientations_and_spins(COMMON_MTWIST_SEED);
	planetary_atmosphere_model_init_models(ATMOSPHERE_TYPE_GEN_SEED, NATMOSPHERE_TYPES);
	pthread_mutex_lock(&universe_mutex);
	snis_object_pool_setup(&pool, MAXGAMEOBJS);

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
	pthread_mutex_unlock(&universe_mutex);
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
static const int socket_waypoint_xoff[] = { -70, 0, 70, 0 };
static const int socket_waypoint_yoff[] = { 0, 90, 0, -90 };

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
	double max_player_velocity = MAX_PLAYER_VELOCITY;

	if (thrust > 0) {
		if (o->tsd.ship.velocity < max_player_velocity)
			o->tsd.ship.velocity += PLAYER_VELOCITY_INCREMENT;
	} else {
		if (o->tsd.ship.velocity > -max_player_velocity)
			o->tsd.ship.velocity -= PLAYER_VELOCITY_INCREMENT;
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
		if (robot->tsd.robot.desired_velocity < MAX_ROBOT_VELOCITY)
			robot->tsd.robot.desired_velocity += ROBOT_VELOCITY_INCREMENT;
	} else {
		if (robot->tsd.robot.desired_velocity > -MAX_ROBOT_VELOCITY)
			robot->tsd.robot.desired_velocity -= ROBOT_VELOCITY_INCREMENT;
	}
}

typedef void (*do_yaw_function)(struct game_client *c, int yaw);

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
			YAW_INCREMENT, YAW_INCREMENT_FINE);
}

static void do_pitch(struct game_client *c, int pitch)
{
	struct snis_entity *ship = &go[c->ship_index];
	double max_pitch_velocity =
		(MAX_PITCH_VELOCITY * ship->tsd.ship.power_data.maneuvering.i) / 255;

	ship->tsd.ship.computer_steering_time_left = 0; /* cancel any computer steering in progress */
	do_generic_axis_rot(&ship->tsd.ship.pitch_velocity, pitch, max_pitch_velocity,
			PITCH_INCREMENT, PITCH_INCREMENT_FINE);
}

static void do_roll(struct game_client *c, int roll)
{
	struct snis_entity *ship = &go[c->ship_index];
	double max_roll_velocity =
		(MAX_ROLL_VELOCITY * ship->tsd.ship.power_data.maneuvering.i) / 255;

	ship->tsd.ship.computer_steering_time_left = 0; /* cancel any computer steering in progress */
	do_generic_axis_rot(&ship->tsd.ship.roll_velocity, roll, max_roll_velocity,
			ROLL_INCREMENT, ROLL_INCREMENT_FINE);
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

static void do_gun_yaw(struct game_client *c, int yaw)
{
	/* FIXME combine this with do_yaw somehow */
	struct snis_entity *ship = &go[c->ship_index];

	do_generic_axis_rot(&ship->tsd.ship.gun_yaw_velocity, yaw,
				MAX_GUN_YAW_VELOCITY, GUN_YAW_INCREMENT,
				GUN_YAW_INCREMENT_FINE);
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
			MAX_SCI_BW_YAW_VELOCITY, SCI_BW_YAW_INCREMENT,
			SCI_BW_YAW_INCREMENT_FINE);
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
		add_warp_effect(o->id, o->x, o->y, o->z, o->x + dx, o->y, o->z + dz);
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
	pthread_mutex_unlock(&universe_mutex);
	send_ship_sdata_packet(c, &p);
	pthread_mutex_lock(&universe_mutex);
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

	range2 = ship->tsd.ship.scibeam_range * ship->tsd.ship.scibeam_range;
	range3 = 4.0 * ship->tsd.ship.scibeam_range * 4.0 * ship->tsd.ship.scibeam_range;
	/* distance to target... */
	dist2 = (o->x - ship->x) * (o->x - ship->x) +
		(o->z - ship->z) * (o->z - ship->z);
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
	if (save_sdata_bandwidth()) {
#if GATHER_OPCODE_STATS
		write_opcode_stats[OPCODE_SHIP_SDATA].count_not_sent++;
#endif
		return;
	}
	if (!should_send_sdata(c, ship, o)) {
#if GATHER_OPCODE_STATS
		write_opcode_stats[OPCODE_SHIP_SDATA].count_not_sent++;
#endif
		return;
	}
	pack_and_send_ship_sdata_packet(c, o);
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
	if (c->ship_index < 0)
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
			ROLE_SCIENCE);
	/* remember sci selection for retargeting mining bot */
	if (selection_type == OPCODE_SCI_SELECT_TARGET_TYPE_OBJECT) {
		bridgelist[c->bridge].selected_waypoint = -1;
		bridgelist[c->bridge].science_selection = id;
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
		"  * /about - information about the game",
		"",
		0,
	};
	for (i = 0; hlptxt[i]; i++)
		send_comms_packet("", bridgelist[c->bridge].comms_channel, hlptxt[i]);
}

static void meta_comms_about(char *name, struct game_client *c, char *txt)
{
	int i;
	char version[100];
	const char *abouttxt[] = {
		"Space Nerds In Space is free software",
		"Source code can be found here:",
		"https://github.com/smcameron/space-nerds-in-space",
		"--------------------------",
		0,
	};
	send_comms_packet("", bridgelist[c->bridge].comms_channel, "--------------------------");
	snprintf(version, sizeof(version), "SPACE NERDS IN SPACE CLIENT:");
	send_comms_packet("", bridgelist[c->bridge].comms_channel, version);
	if (c->build_info[0])
		send_comms_packet("", bridgelist[c->bridge].comms_channel, c->build_info[0]);
	else
		send_comms_packet("", bridgelist[c->bridge].comms_channel, "UNKNOWN SNIS CLIENT");
	if (c->build_info[1])
		send_comms_packet("", bridgelist[c->bridge].comms_channel, c->build_info[1]);
	snprintf(version, sizeof(version), "SPACE NERDS IN SPACE SERVER:");
	send_comms_packet("", bridgelist[c->bridge].comms_channel, version);
	send_comms_packet("", bridgelist[c->bridge].comms_channel, BUILD_INFO_STRING1);
	send_comms_packet("", bridgelist[c->bridge].comms_channel, BUILD_INFO_STRING2);
	send_comms_packet("", bridgelist[c->bridge].comms_channel, "--------------------------");
	for (i = 0; abouttxt[i]; i++)
		send_comms_packet("", bridgelist[c->bridge].comms_channel, abouttxt[i]);
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
	char msg[100];
	int passenger_count = 0;

	i = lookup_by_id(bridgelist[c->bridge].shipid);
	if (i < 0) {
		printf("Non fatal error in %s at %s:%d\n",
			__func__, __FILE__, __LINE__);
		return;
	}
	ship = &go[i];

	/* FIXME: sending this on current channel -- should make it private to ship somehow */
	send_comms_packet(name, ch, " PASSENGER LIST:");
	for (i = 0; i < npassengers; i++) {
		if (passenger[i].location == ship->id) {
			int x = lookup_by_id(passenger[i].destination);
			snprintf(msg, sizeof(msg), "%2d. FARE %4d DEST: %10s NAME: %30s\n",
					passenger_count + 1, passenger[i].fare,
					x < 0 ? "UNKNOWN" : go[x].tsd.starbase.name,
					passenger[i].name);
			send_comms_packet(name, ch, msg);
			passenger_count++;
		}
	}
	snprintf(msg, sizeof(msg), "  %d PASSENGERS ABOARD\n", passenger_count);
	send_comms_packet(name, ch, msg);
	if (strncasecmp(txt, "/passengers", 11) == 0)
		return;
	send_comms_packet(name, ch, " INVENTORY OF HOLD:");
	send_comms_packet(name, ch, " --------------------------------------");
	if (ship->tsd.ship.ncargo_bays == 0) {
		send_comms_packet(name, ch, "   SHIP HAS ZERO CARGO BAYS.");
		send_comms_packet(name, ch, " --------------------------------------");
		return;
	}
	for (i = 0; i < ship->tsd.ship.ncargo_bays; i++) {
		char *itemname, *unit;
		struct cargo_container_contents *ccc = &ship->tsd.ship.cargo[i].contents;
		float qty;
		char origin[20], dest[20];

		if (ccc->item == -1) {
			snprintf(msg, sizeof(msg), "    CARGO BAY %d: ** EMPTY **", i);
			send_comms_packet(name, ch, msg);
		} else {
			char due_date[20];
			itemname = commodity[ccc->item].name;
			unit = commodity[ccc->item].unit;
			qty = ccc->qty;
			get_location_name(ship->tsd.ship.cargo[i].origin, origin, sizeof(origin));
			get_location_name(ship->tsd.ship.cargo[i].dest, dest, sizeof(dest));
			format_due_date(due_date, sizeof(due_date), (double)
				ship->tsd.ship.cargo[i].due_date);
			snprintf(msg, sizeof(msg),
				"    CARGO BAY %d: %4.2f %s %s - PAID $%.2f ORIG %10s DEST %10s DUE %s",
					i, qty, unit, itemname, ship->tsd.ship.cargo[i].paid,
					origin, dest, due_date);
			send_comms_packet(name, ch, msg);
		}
	}
	send_comms_packet(name, ch, " --------------------------------------");
	if (ship->tsd.ship.mining_bots > 0) {
		snprintf(msg, sizeof(msg), " %d MINING BOT%s DOCKED AND STOWED.",
			ship->tsd.ship.mining_bots,
			ship->tsd.ship.mining_bots == 1 ? "" : "S");
		send_comms_packet(name, ch, msg);
	}
	snprintf(msg, sizeof(msg), " CASH ON HAND:  $%.2f", ship->tsd.ship.wallet);
	send_comms_packet(name, ch, msg);
	send_comms_packet(name, ch, " --------------------------------------");
}

static void meta_comms_channel(char *name, struct game_client *c, char *txt)
{
	int rc;
	uint32_t newchannel;
	char msg[100];

	rc = sscanf(txt, "%*[/chanel] %u\n", &newchannel);
	if (rc != 1) {
		snprintf(msg, sizeof(msg), "INVALID CHANNEL - CURRENT CHANNEL %u",
				bridgelist[c->bridge].comms_channel);
		send_comms_packet(name, bridgelist[c->bridge].comms_channel, msg);
		return;
	}
	snprintf(msg, sizeof(msg), "TRANSMISSION TERMINATED ON CHANNEL %u",
			bridgelist[c->bridge].comms_channel);
	send_comms_packet(name, bridgelist[c->bridge].comms_channel, msg);
	bridgelist[c->bridge].comms_channel = newchannel;

	if (bridgelist[c->bridge].npcbot.channel != newchannel) {
		/* disconnect npc bot */
		bridgelist[c->bridge].npcbot.channel = (uint32_t) -1;
		bridgelist[c->bridge].npcbot.object_id = (uint32_t) -1;
		bridgelist[c->bridge].npcbot.current_menu = NULL;
		bridgelist[c->bridge].npcbot.special_bot = NULL;
	}

	/* Note: client snoops this channel change message. */
	snprintf(msg, sizeof(msg), "%s%u", COMMS_CHANNEL_CHANGE_MSG, newchannel);
	send_comms_packet(name, newchannel, msg);
}

static void meta_comms_eject(char *name, struct game_client *c, char *txt)
{
	int i, cargobay, rc, item;
	float qty;
	char msg[100];
	struct snis_entity *ship;
	const union vec3 up = { { 0.0f, 1.0f, 0.0f } };
	union vec3 container_loc, container_vel;

	i = lookup_by_id(c->shipid);
	if (i < 0)
		return;
	ship = &go[i];

	rc = sscanf(txt, "/eject %d\n", &cargobay);
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

	(void) add_cargo_container(container_loc.v.x, container_loc.v.y, container_loc.v.z,
				container_vel.v.x, container_vel.v.y, container_vel.v.z,
				item, qty, 1);
	snprintf(msg, sizeof(msg), "CARGO BAY %d EJECTED", cargobay);
	send_comms_packet(name, bridgelist[c->bridge].comms_channel, msg);
	return;

invalid_cargo_bay:
	snprintf(msg, sizeof(msg), "INVALID CARGO BAY");
	send_comms_packet(name, bridgelist[c->bridge].comms_channel, msg);
	return;

empty_cargo_bay:
	snprintf(msg, sizeof(msg), "EMPTY CARGO BAY");
	send_comms_packet(name, bridgelist[c->bridge].comms_channel, msg);
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
	send_comms_packet(npcname, botstate->channel,
				"  SORRY, THAT IS NOT IMPLEMENTED");
}

static void npc_menu_item_sign_off(struct npc_menu_item *item,
					char *npcname, struct npc_bot_state *botstate)
{
	send_comms_packet(npcname, botstate->channel,
				"  IT HAS BEEN A PLEASURE SERVING YOU");
	send_comms_packet(npcname, botstate->channel,
				"  ZX81 SERVICE ROBOT TERMINATING TRANSMISSION");
	botstate->channel = (uint32_t) -1;
	botstate->object_id = (uint32_t) -1;
	botstate->current_menu = NULL;
}

static void npc_send_parts_menu(char *npcname, struct npc_bot_state *botstate)
{
	int i;
	char msg[100];
	struct snis_entity *sb;

	i = lookup_by_id(botstate->object_id);
	if (i < 0)
		return;
	sb = &go[i];

	send_comms_packet(npcname, botstate->channel, "");
	snprintf(msg, sizeof(msg), "  -- BUY %s PARTS --", damcon_system_name(botstate->parts_menu));
	send_comms_packet(npcname, botstate->channel, msg);
	for (i = 0; i < DAMCON_PARTS_PER_SYSTEM; i++) {
		float price = sb->tsd.starbase.part_price[botstate->parts_menu *
								DAMCON_PARTS_PER_SYSTEM + i];
		snprintf(msg, sizeof(msg), "  %c:   $%.2f   %s\n", i + 'A', price,
				damcon_part_name(botstate->parts_menu, i));
		send_comms_packet(npcname, botstate->channel, msg);
	}
	send_comms_packet(npcname, botstate->channel, "  0:   PREVIOUS MENU");
	send_comms_packet(npcname, botstate->channel, "");
}

static void parts_buying_npc_bot(struct snis_entity *o, int bridge,
		char *name, char *msg)
{
	int i, rc, selection;
	char sel;
	float range2;
	char *n = o->tsd.starbase.name;
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
	if (rc != 1)
		selection = -1;
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
		send_comms_packet(n, channel,
			" TRANSACTION NOT POSSIBLE - TRANSPORTER RANGE EXCEEDED");
		return;
	}
	price = o->tsd.starbase.part_price[botstate->parts_menu * DAMCON_PARTS_PER_SYSTEM + selection];
	if (price > ship->tsd.ship.wallet) {
		send_comms_packet(n, channel, " INSUFFICIENT FUNDS");
		return;
	}
	ship->tsd.ship.wallet -= price;
	send_comms_packet(n, channel, " THANK YOU FOR YOUR PURCHASE.");
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

	botstate->parts_menu = (item - &repairs_and_maintenance_menu[0] - 1) %
				(DAMCON_SYSTEM_COUNT - 1);
	botstate->special_bot = parts_buying_npc_bot;
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
	if (i < 0)
		parent = NULL;
	else
		parent = &go[i];
	if (ai->mode != MINING_MODE_STANDBY_TO_TRANSPORT_ORE) {
		send_comms_packet(npcname, channel,
			"UNABLE TO OBTAIN COHERENT TRANSPORTER BEAM LOCK");
	} else {
		send_comms_packet(npcname, channel, "--- TRANSPORTING MATERIALS ---");
		mining_bot_unload_ores(miner, parent, ai);
		send_comms_packet(npcname, channel, "--- MATERIALS TRANSPORTED ---");
		send_comms_packet(npcname, channel,
			"COMMENCING RENDEZVOUS WITH TARGET");
		ai->mode = MINING_MODE_APPROACH_ASTEROID;
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
	send_comms_packet(npcname, channel, " RETURNING TO SHIP");
}

static void npc_menu_item_mining_bot_retarget(struct npc_menu_item *item,
				char *npcname, struct npc_bot_state *botstate)
{
	int i;
	struct bridge_data *b;
	uint32_t channel = botstate->channel;
	struct ai_mining_bot_data *ai;
	char msg[60];
	struct snis_entity *miner, *asteroid;
	float dist;

	/* find our bridge... */
	b = container_of(botstate, struct bridge_data, npcbot);

	i = lookup_by_id(botstate->object_id);
	if (i < 0)
		return;
	miner = &go[i];
	ai = &miner->tsd.ship.ai[0].u.mining_bot;

	i = lookup_by_id(b->science_selection);
	if (i < 0) {
		send_comms_packet(npcname, channel, " NO DESTINATION TARGETED");
		return;
	}
	asteroid = &go[i];
	if (asteroid->type != OBJTYPE_ASTEROID && asteroid->type != OBJTYPE_DERELICT) {
		send_comms_packet(npcname, channel, " SELECTED DESTINATION INAPPROPRIATE");
		return;
	}
	ai->asteroid = b->science_selection;
	dist = dist3d(asteroid->x - miner->x, asteroid->y - miner->y, asteroid->z - miner->z);
	sprintf(msg, " RETARGETED TO %s, DISTANCE: %f",
			asteroid ? asteroid->sdata.name : "UNKNOWN", dist);
	send_comms_packet(npcname, channel, msg);
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
	char msg[100];

	i = lookup_by_id(botstate->object_id);
	if (i < 0)
		return;
	miner = &go[i];
	ai = &miner->tsd.ship.ai[0].u.mining_bot;
	i = lookup_by_id(ai->asteroid);
	if (i >= 0) {
		asteroid = &go[i];
		dist = dist3d(asteroid->x - miner->x, asteroid->y - miner->y, asteroid->z - miner->z);
	} else {
		asteroid = NULL;
		dist = -1.0;
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
	send_comms_packet(npcname, channel, "--- BEGIN STATUS REPORT ---");
	switch (ai->mode) {
	case MINING_MODE_APPROACH_ASTEROID:
		sprintf(msg, "RENDEZVOUS WITH %s, DISTANCE: %f\n",
			asteroid ? asteroid->sdata.name : "UNKNOWN", dist);
		send_comms_packet(npcname, channel, msg);
		break;
	case MINING_MODE_LAND_ON_ASTEROID:
		sprintf(msg, "DESCENT ONTO %s, ALTITUDE: %f\n",
			asteroid ? asteroid->sdata.name : "UNKNOWN", dist * 0.3);
		send_comms_packet(npcname, channel, msg);
		break;
	case MINING_MODE_MINE:
		if (asteroid->type == OBJTYPE_ASTEROID)
			sprintf(msg, "MINING ON %s\n",
				asteroid ? asteroid->sdata.name : "UNKNOWN");
		else
			sprintf(msg, "SALVAGING %s\n",
				asteroid ? asteroid->sdata.name : "UNKNOWN");
		send_comms_packet(npcname, channel, msg);
		break;
	case MINING_MODE_RETURN_TO_PARENT:
	case MINING_MODE_STOW_BOT:
		sprintf(msg, "RETURNING FROM %s\n",
			asteroid ? asteroid->sdata.name : "UNKNOWN");
		send_comms_packet(npcname, channel, msg);
		sprintf(msg, "DISTANCE TO %s: %f\n",
			asteroid ? asteroid->sdata.name : "UNKNOWN", dist);
		send_comms_packet(npcname, channel, msg);
		break;
	case MINING_MODE_STANDBY_TO_TRANSPORT_ORE:
		sprintf(msg, "STANDING BY TO TRANSPORT MATERIALS\n");
		send_comms_packet(npcname, channel, msg);
		sprintf(msg, "DISTANCE TO %s: %f\n",
			asteroid ? asteroid->sdata.name : "UNKNOWN", dist);
		send_comms_packet(npcname, channel, msg);
		break;
	default:
		break;
	}
	sprintf(msg, "DISTANCE TO %s: %f\n",
		parent ? parent->sdata.name : "MOTHER SHIP", dist_to_parent);
	send_comms_packet(npcname, channel, msg);
	switch (asteroid->type) {
	case OBJTYPE_ASTEROID:
		sprintf(msg, "ORE COLLECTED: %f TONS\n", 2.0 * total / (255.0 * 4.0));
		send_comms_packet(npcname, channel, msg);
		send_comms_packet(npcname, channel, "ORE COMPOSITION:");
		sprintf(msg, "GOLD: %2f%%\n", gold / total);
		send_comms_packet(npcname, channel, msg);
		sprintf(msg, "PLATINUM: %2f%%\n", platinum / total);
		send_comms_packet(npcname, channel, msg);
		sprintf(msg, "GERMANIUM: %2f%%\n", germanium / total);
		send_comms_packet(npcname, channel, msg);
		sprintf(msg, "URANIUM: %2f%%\n", uranium / total);
		send_comms_packet(npcname, channel, msg);
		break;
	case OBJTYPE_DERELICT:
		sprintf(msg, "FUEL AND OXYGEN COLLECTED:\n");
		send_comms_packet(npcname, channel, msg);
		sprintf(msg, "FUEL: %2f%%\n", fuel / 255.0);
		send_comms_packet(npcname, channel, msg);
		sprintf(msg, "OXYGEN: %2f%%\n", oxygen / 255.0);
		send_comms_packet(npcname, channel, msg);
		break;
	default:
		break;
	}
	send_comms_packet(npcname, channel, "--- END STATUS REPORT ---");
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
	char msg[100];
	char *npcname = sb->tsd.starbase.name;
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

	if (strcmp(command, "") == 0 || strcmp(command, "?") == 0) {
		snprintf(msg, sizeof(msg), " TRAVELERS SEEKING PASSAGE\n");
		send_comms_packet(npcname, ch, msg);

		count = 0;
		for (i = 0; i < npassengers; i++) {
			if (passenger[i].location == sb->id) {
				int d = lookup_by_id(passenger[i].destination);
				char *dest = d < 0 ? "unknown" : go[d].tsd.starbase.name;
				count++;
				snprintf(msg, sizeof(msg), "  %2d: DEST: %12s  FARE: $%5d  NAME: %s\n",
					count, dest, passenger[i].fare, passenger[i].name);
				send_comms_packet(npcname, ch, msg);
			}
		}
		if (count == 0)
			send_comms_packet(npcname, ch, " NO PASSENGERS AVAILABLE");
		send_comms_packet(npcname, ch, "\n");
		send_comms_packet(npcname, ch, " 0. PREVIOUS MENU\n");
		return;
	}

	/* Check if player is currently docked here */
	player_is_docked = ship_is_docked(bridgelist[bridge].shipid, sb);
	rc = sscanf(command, "%d", &selection);
	if (rc != 1) {
		send_comms_packet(npcname, ch, " HUH? TRY AGAIN\n");
		return;
	}
	if (selection == 0) {
		botstate->special_bot = NULL;
		send_to_npcbot(bridge, name, ""); /* poke generic bot so he says something */
		return;
	}
	if (selection < 1) {
		send_comms_packet(npcname, ch, " BAD SELECTION, TRY AGAIN\n");
		return;
	}

	if (!player_is_docked) {
		send_comms_packet(npcname, ch, " YOU MUST BE DOCKED TO BOARD PASSENGERS\n");
		return;
	}
	if (already_aboard >= ship->tsd.ship.passenger_berths) {
		send_comms_packet(npcname, ch, " ALL OF YOUR PASSENGER BERTHS ARE BOOKED\n");
		return;
	}
	count = 0;
	if (already_aboard < ship->tsd.ship.passenger_berths) {
		for (i = 0; i < npassengers; i++) {
			if (passenger[i].location == sb->id) {
				count++;
				if (count == selection) {
					sprintf(msg, " BOARDING PASSENGER %s\n", passenger[i].name);
					send_comms_packet(npcname, ch, msg);
					passenger[i].location = bridgelist[bridge].shipid;
					break;
				}
			}
		}
	}
	if (count != selection) {
		send_comms_packet(npcname, ch, " UNKNOWN SELECTION, TRY AGAIN\n");
		return;
	}
}

static void npc_menu_item_eject_passengers(struct npc_menu_item *item,
					char *npcname, struct npc_bot_state *botstate)
{
	struct snis_entity *sb, *ship;
	int ch = botstate->channel;
	struct bridge_data *b = container_of(botstate, struct bridge_data, npcbot);
	char msg[100];
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
		send_comms_packet(npcname, ch, " YOU MUST BE DOCKED TO DISEMBARK PASSENGERS\n");
		return;
	}
	for (i = 0; i < npassengers; i++) {
		if (passenger[i].location == b->shipid && passenger[i].destination == sb->id) {
			snprintf(msg, sizeof(msg), "  PASSENGER %s DISEMBARKED\n",
					passenger[i].name);
			send_comms_packet(npcname, ch, msg);
			ship->tsd.ship.wallet += passenger[i].fare;
			/* passenger disembarks, ceases to be a passenger, replace with new one */
			update_passenger(i, nstarbases);
			continue;
		}
		if (passenger[i].location == b->shipid) {
			snprintf(msg, sizeof(msg), "  PASSENGER %s EJECTED\n",
					passenger[i].name);
			send_comms_packet(npcname, ch, msg);
			/* Player is fined for ejecting passengers */
			ship->tsd.ship.wallet -= passenger[i].fare;
			/* passenger ejected, ceases to be a passenger, replace with new one */
			update_passenger(i, nstarbases);
		}
	}
}

static void npc_menu_item_disembark_passengers(struct npc_menu_item *item,
					char *npcname, struct npc_bot_state *botstate)
{
	struct snis_entity *sb, *ship;
	int ch = botstate->channel;
	struct bridge_data *b = container_of(botstate, struct bridge_data, npcbot);
	char msg[100];
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
		send_comms_packet(npcname, ch, " YOU MUST BE DOCKED TO DISEMBARK PASSENGERS\n");
		return;
	}
	for (i = 0; i < npassengers; i++) {
		if (passenger[i].location == b->shipid && passenger[i].destination == sb->id) {
			snprintf(msg, sizeof(msg), "  PASSENGER %s DISEMBARKED\n",
					passenger[i].name);
			send_comms_packet(npcname, ch, msg);
			ship->tsd.ship.wallet += passenger[i].fare;
			/* passenger disembarks, ceases to be a passenger, replace with new one */
			update_passenger(i, nstarbases);
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

static void npc_menu_item_travel_advisory(struct npc_menu_item *item,
					char *npcname, struct npc_bot_state *botstate)
{
	uint32_t plid, ch = botstate->channel;
	struct snis_entity *sb, *pl = NULL;
	char *name, msg[100];
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
			name = sb->tsd.starbase.name;
		}
	} else {
		name = sb->tsd.starbase.name;
	}

	if (pl) {
		/* TODO: fill in all this crap */
		snprintf(msg, sizeof(msg), " TRAVEL ADVISORY FOR %s", name);
		send_comms_packet(npcname, ch, msg);
		send_comms_packet(npcname, ch, "-----------------------------------------------------");
		snprintf(msg, sizeof(msg), " WELCOME TO STARBASE %s, IN ORBIT", sb->tsd.starbase.name);
		send_comms_packet(npcname, ch, msg);
		snprintf(msg, sizeof(msg), " AROUND THE BEAUTIFUL PLANET %s.", name);
		send_comms_packet(npcname, ch, msg);
		send_comms_packet(npcname, ch, "");
		send_comms_packet(npcname, ch, " PLANETARY SURFACE TEMP: -15 - 103");
		send_comms_packet(npcname, ch, " PLANETARY SURFACE WIND SPEED: 0 - 203");
	} else {
		snprintf(msg, sizeof(msg), " TRAVEL ADVISORY FOR STARBASE %s", name);
		send_comms_packet(npcname, ch, msg);
		send_comms_packet(npcname, ch, "-----------------------------------------------------");
		snprintf(msg, sizeof(msg), " WELCOME TO STARBASE %s, IN DEEP SPACE",
				sb->tsd.starbase.name);
		send_comms_packet(npcname, ch, msg);
		send_comms_packet(npcname, ch, "");
	}
	send_comms_packet(npcname, ch, " SPACE WEATHER ADVISORY: ALL CLEAR");
	send_comms_packet(npcname, ch, "");
	if (contraband != (uint16_t) -1) {
		send_comms_packet(npcname, ch, " TRAVELERS TAKE NOTICE OF PROHIBITED ITEMS:");
		snprintf(msg, sizeof(msg), "    %s", commodity[contraband].name);
		send_comms_packet(npcname, ch, msg);
	}
	send_comms_packet(npcname, ch, "");
	send_comms_packet(npcname, ch, " ENJOY YOUR VISIT!");
	send_comms_packet(npcname, ch, "-----------------------------------------------------");
}

static void npc_menu_item_request_dock(struct npc_menu_item *item,
				char *npcname, struct npc_bot_state *botstate)
{
	struct snis_entity *o;
	struct snis_entity *sb;
	char msg[100];
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
	dist = dist3d(sb->x - o->x, sb->y - o->y, sb->z - o->z);
	if (dist > STARBASE_DOCKING_PERM_DIST) {
		snprintf(msg, sizeof(msg), "%s, YOU ARE TOO FAR AWAY (%lf).\n", b->shipname, dist);
		send_comms_packet(npcname, ch, msg);
		return;
	}
	if (o->sdata.shield_strength > 15) {
		snprintf(msg, sizeof(msg), "%s, YOU MUST LOWER SHIELDS FIRST.\n", b->shipname);
		send_comms_packet(npcname, ch, msg);
		return;
	}
	starbase_grant_docker_permission(sb, o->id, b, npcname, ch);
}

static void warp_gate_ticket_buying_npc_bot(struct snis_entity *o, int bridge,
		char *name, char *msg)
{
	struct ssgl_game_server *gameserver = NULL;
	int selection, rc, i, nservers;
	char buf[100];
	int ch = bridgelist[bridge].npcbot.channel;
	double ssx, ssy, ssz; /* our solarsystem's position */
	double dx, dy, dz;
	int sslist[100];
	int nsslist = 0;

	if (server_tracker_get_server_list(server_tracker, &gameserver, &nservers) != 0
		|| nservers <= 0) {
		send_comms_packet(name, ch, "NO WARP-GATE TICKETS AVAILABLE\n");
		return;
	}
	send_comms_packet(name, ch, "WARP-GATE TICKETS:\n");
	send_comms_packet(name, ch, "------------------\n");

	/* Find our solarsystem */
	int len = strlen(solarsystem_name);
	if (len > LOCATIONSIZE)
		len = LOCATIONSIZE;
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
				sprintf(buf, "%3d: %s\n", nsslist, gameserver[i].location);
				send_comms_packet(name, ch, buf);
			}
		}
	}
	send_comms_packet(name, ch, "------------------\n");
	send_comms_packet(name, ch, "  0: PREVIOUS MENU\n");
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
		sprintf(buf, "WARP-GATE TICKET TO %s BOOKED\n", gameserver[selection].location);
		send_comms_packet(name, ch, buf);
	} else {
		sprintf(buf, "WARP-GATE TICKET TO %s EXCHANGED\n",
			bridgelist[bridge].warp_gate_ticket.location);
		send_comms_packet(name, ch, buf);
		sprintf(buf, "FOR WARP-GATE TICKET TO %s\n", gameserver[selection].location);
		send_comms_packet(name, ch, buf);
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
	char msg[100];
	struct snis_entity *o;
	struct bridge_data *b = container_of(botstate, struct bridge_data, npcbot);
	int i, bridge = b - bridgelist;
	int closest_tow_ship;
	double closest_tow_ship_distance;
	uint32_t channel = bridgelist[bridge].npcbot.channel;

	pthread_mutex_lock(&universe_mutex);
	i = lookup_by_id(b->shipid);
	if (i < 0)
		goto out;
	o = &go[i];

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
					snprintf(msg, sizeof(msg),
						"%s, THE MANTIS TOW SHIP %s IS ALREADY EN ROUTE",
						b->shipname, go[i].sdata.name);
					send_comms_packet(npcname, channel, msg);
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
		snprintf(msg, sizeof(msg), "%s, SORRY, ALL MANTIS TOW SHPS ARE CURRENTLY BUSY.",
				b->shipname);
		send_comms_packet(npcname, channel, msg);
		goto out;
	}

	/* Send the tow ship to the player */
	push_tow_mode(&go[closest_tow_ship], o->id, botstate->object_id);
	snprintf(msg, sizeof(msg), "%s, THE MANTIS TOW SHIP %s HAS BEEN",
			b->shipname, go[closest_tow_ship].sdata.name);
	send_comms_packet(npcname, channel, msg);
	snprintf(msg, sizeof(msg), "DISPATCHED TO YOUR LOCATION");
	send_comms_packet(npcname, channel, msg);
	snprintf(msg, sizeof(msg), "%s, UPON DELIVERY YOUR ACCOUNT\n", b->shipname);
	send_comms_packet(npcname, channel, msg);
	snprintf(msg, sizeof(msg), "WILL BE BILLED $%5.2f\n", TOW_SHIP_CHARGE);
	send_comms_packet(npcname, channel, msg);
	send_comms_packet(npcname, channel, " WARNING THE TOW SHIP NAVIGATION ALGORITHM IS BUGGY");
	send_comms_packet(npcname, channel, " AND IT MAY OCCASIONALLY TRY TO DRIVE YOU INTO A PLANET");
	send_comms_packet(npcname, channel, " USE DOCKING MAGNETS TO DISCONNECT IF NECESSARY");

out:
	pthread_mutex_unlock(&universe_mutex);
	return;
}

static void send_npc_menu(char *npcname,  int bridge)
{
	int i;
	uint32_t channel = bridgelist[bridge].npcbot.channel;
	struct npc_menu_item *menu = &bridgelist[bridge].npcbot.current_menu[0];
	char msg[80];

	if (!menu || !menu->name)
		return;

	send_comms_packet(npcname, channel, "-----------------------------------------------------");
	sprintf(msg, "     %s", menu->name);
	send_comms_packet(npcname, channel, msg);
	send_comms_packet(npcname, channel, "");
	menu++;
	i = 1;
	while (menu->name) {
		snprintf(msg, sizeof(msg), "     %d: %s\n", i, menu->name);
		send_comms_packet(npcname, channel, msg);
		menu++;
		i++;
	};
	menu = &bridgelist[bridge].npcbot.current_menu[0];
	if (menu->parent_menu) {
		snprintf(msg, sizeof(msg), "     %d: BACK TO %s\n", 0, menu->parent_menu->name);
		send_comms_packet(npcname, channel, msg);
	}
	send_comms_packet(npcname, channel, "");
	send_comms_packet(npcname, channel, "    MAKE YOUR SELECTION WHEN READY.");
	send_comms_packet(npcname, channel, "-----------------------------------------------------");
}

static void starbase_cargo_buyingselling_npc_bot(struct snis_entity *o, int bridge,
		char *name, char *msg, int buy)
{
	int i;
	char m[100];
	char *n = o->tsd.starbase.name;
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
				send_comms_packet(n, channel,
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
				send_comms_packet(n, channel, " ALL YOUR CARGO BAYS ARE FULL");
				return;
			}

			/* decode the buy order */
			rc = sscanf(msg + 4, "%d %c", &q, &x);
			if (rc != 2) {
				send_comms_packet(n, channel, " INVALID BUY ORDER");
				return;
			}

			/* check buy order is in range */
			x = toupper(x);
			if (x < 'A' || x >= 'A' + COMMODITIES_PER_BASE) {
				send_comms_packet(n, channel, " INVALID BUY ORDER");
				return;
			}

			/* check qty */
			if (q <= 0 || q > mkt[x - 'A'].qty) {
				send_comms_packet(n, channel, " INVALID BUY ORDER");
				return;
			}

			ask = mkt[x - 'A'].ask;
			/* check fundage */
			if (q * ask > ship->tsd.ship.wallet) {
				send_comms_packet(n, channel, " INSUFFICIENT FUNDS");
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
			snprintf(m, sizeof(m), " EXECUTING BUY ORDER %d %c", q, x);
			send_comms_packet(n, channel, m);
			snis_queue_add_sound(TRANSPORTER_SOUND,
					ROLE_SOUNDSERVER, ship->id);
			send_comms_packet(n, channel,
				"TRANSPORTER BEAM ACTIVATED - GOODS TRANSFERRED");
			return;
		}
		if (strncasecmp("sell ", msg, 5) == 0)  {
			send_comms_packet(n, channel, " INVALID SELL ORDER (USE THE 'SELL' MENU)");
			return;
		}
	} else {
		if (strncasecmp("buy ", msg, 4) == 0)  {
			send_comms_packet(n, channel, " INVALID BUY ORDER (USE THE 'BUY' MENU)");
			return;
		}
		if (strncasecmp("sell ", msg, 5) == 0)  {
			float qf;
			char x;

			/* check transporter range */
			if (range2 > TRANSPORTER_RANGE * TRANSPORTER_RANGE) {
				send_comms_packet(n, channel,
					" TRANSACTION NOT POSSIBLE - TRANSPORTER RANGE EXCEEDED");
				return;
			}

			rc = sscanf(msg + 5, "%f %c", &qf, &x);
			if (rc != 2) {
				rc = sscanf(msg + 5, "%c", &x);
				if (rc == 1 && strlen(msg) == 6) {
					if (toupper(x) - 'A' < 0 || toupper(x) - 'A' >= ship->tsd.ship.ncargo_bays) {
						send_comms_packet(n, channel, " INVALID SELL ORDER, BAD CARGO BAY");
						return;
					}
					qf = ship->tsd.ship.cargo[(int) toupper(x) - 'A'].contents.qty;
				} else {
					send_comms_packet(n, channel, " INVALID SELL ORDER, PARSE ERROR");
					return;
				}
			}
			x = toupper(x) - 'A';
			if (x < 0 || x >= ship->tsd.ship.ncargo_bays) {
				send_comms_packet(n, channel, " INVALID SELL ORDER, BAD CARGO BAY");
				return;
			}
			if (qf > ship->tsd.ship.cargo[(int) x].contents.qty) {
				send_comms_packet(n, channel, " INVALID SELL ORDER, INSUFFICIENT QUANTITY");
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
			snprintf(m, sizeof(m), " EXECUTING SELL ORDER %.2f %c", qf, x);
			send_comms_packet(n, channel, m);
			snis_queue_add_sound(TRANSPORTER_SOUND,
					ROLE_SOUNDSERVER, ship->id);
			send_comms_packet(n, channel,
				"TRANSPORTER BEAM ACTIVATED - GOODS TRANSFERRED");
			snprintf(m, sizeof(m), " SOLD FOR A %s OF $%.2f",
				profitloss < 0 ? "LOSS" : "PROFIT", profitloss);
			send_comms_packet(n, channel, m);
			return;
		}
	}

	if (selection == -1) {
		send_comms_packet(n, channel, "----------------------------");
		if (buy) {
			send_comms_packet(n, channel,
				"   QTY  UNIT ITEM   BID/UNIT     ASK/UNIT    ITEM");
			for (i = 0; i < COMMODITIES_PER_BASE; i++) {
				float bid, ask, qty;
				char *itemname = commodity[mkt[i].item].name;
				char *unit = commodity[mkt[i].item].unit;
				bid = mkt[i].bid;
				ask = mkt[i].ask;
				qty = mkt[i].qty;
				snprintf(m, sizeof(m), " %c: %4.2f %s %s -- $%4.2f  $%4.2f\n",
					i + 'A', qty, unit, itemname, bid, ask);
				send_comms_packet(n, channel, m);
			}
		} else {
			send_comms_packet(n, channel,
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
				snprintf(m, sizeof(m), " %c: %4.2f %s %s -- $%4.2f (PAID $%.2f)",
					i + 'A', qty, unit, itemname, bid,
					ship->tsd.ship.cargo[i].paid);
				send_comms_packet(n, channel, m);
				count++;
			}
			if (count == 0)
				send_comms_packet(n, channel, "   ** CARGO HOLD IS EMPTY **");
		}
		send_comms_packet(n, channel, " 0: PREVIOUS MENU");
		send_comms_packet(n, channel, "----------------------------");
		if (buy)
			send_comms_packet(n, channel, "  (TYPE 'BUY 3 A' TO BUY 3 of item A)");
		else
			send_comms_packet(n, channel,
				"  (TYPE 'SELL 3 A' TO SELL 3 of item A or 'SELL A' to sell all A)");
		send_comms_packet(n, channel, "----------------------------");
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
	char *n = o->tsd.starbase.name;
	uint32_t channel = bridgelist[bridge].comms_channel;
	char m[100];
	int selection, menu_count, i, rc;
	struct npc_menu_item *menu = bridgelist[bridge].npcbot.current_menu;

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
		send_comms_packet(n, channel, "");
		snprintf(m, sizeof(m),
			"  WELCOME %s, I AM A MODEL ZX81 SERVICE", name);
		send_comms_packet(n, channel, m);
		send_comms_packet(n, channel,
			"  ROBOT PROGRAMMED TO BE AT YOUR SERVICE");
		send_comms_packet(n, channel,
			"  THE FOLLOWING SERVICES ARE AVAILABLE:");
		send_comms_packet(n, channel, "");
		send_npc_menu(n, bridge);
	} else {
		if (menu[selection].submenu) {
			menu[selection].submenu->parent_menu = menu;
			bridgelist[bridge].npcbot.current_menu = menu[selection].submenu;
			send_comms_packet(n, channel, "");
			send_npc_menu(n, bridge);
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

	/* check for starbases being hailed */
	for (i = 0; i <= snis_object_pool_highest_object(pool); i++) {
		struct snis_entity *o = &go[i];
		char *sbname = NULL;

		if (o->type == OBJTYPE_STARBASE)
			sbname = o->tsd.starbase.name;
		else if (o->type == OBJTYPE_SHIP2 && o->tsd.ship.ai[0].ai_mode == AI_MODE_MINING_BOT)
			sbname = o->sdata.name;
		if (!sbname)
			continue;

		for (j = 0; j < nnames; j++) {
			printf("comparing '%s' to '%s'\n",
				sbname, namelist[j]);
			if (strcasecmp(sbname, namelist[j]) != 0)
				continue;
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
		send_comms_packet(name, bridgelist[c->bridge].comms_channel, msg);
		bridgelist[c->bridge].comms_channel = channel[0];
		bridgelist[c->bridge].npcbot.object_id = id;
		bridgelist[c->bridge].npcbot.channel = channel[0];
		bridgelist[c->bridge].npcbot.current_menu = starbase_main_menu;
		bridgelist[c->bridge].npcbot.special_bot = NULL;
		/* Note: client snoops this channel change message */
		snprintf(msg, sizeof(msg), "%s%u", COMMS_CHANNEL_CHANGE_MSG, channel[0]);
		send_comms_packet(name, channel[0], msg);
		send_to_npcbot(c->bridge, name, msg);
	} else {
		for (i = 0; i < nchannels; i++) {
			snprintf(msg, sizeof(msg), "*** HAILING ON CHANNEL %u ***",
					bridgelist[c->bridge].comms_channel);
			send_comms_packet(name, channel[i], msg);
		}
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
	uint8_t len;
	uint32_t id;
	char name[30];

	rc = read_and_unpack_buffer(c, buffer, "bw", &len, &id);
	if (rc)
		return rc;
	rc = snis_readsocket(c->socket, txt, len);
	if (rc)
		return rc;
	txt[len] = '\0';
	if (use_real_name) {
		sprintf(name, "%s: ", bridgelist[c->bridge].shipname);
	} else {
		i = lookup_by_id(id);
		if (i < 0)
			return 0;
		sprintf(name, "%s: ", go[i].sdata.name);
	}
	if (txt[0] == '/') {
		process_meta_comms_packet(name, c, txt);
		return 0;
	}
	send_comms_packet(name, bridgelist[c->bridge].comms_channel, txt);
	if (bridgelist[c->bridge].npcbot.channel == bridgelist[c->bridge].comms_channel)
		send_to_npcbot(c->bridge, name, txt);
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
			(double) c->shipid, (double) selection, 0.0);
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
	int rc;
	uint8_t subcode, row;
	double value[3];
	unsigned char buffer[20];
	int b;

	rc = read_and_unpack_buffer(c, buffer, "b", &subcode);
	if (rc)
		return rc;
	switch (subcode) {
	case OPCODE_SET_WAYPOINT_CLEAR:
		rc = read_and_unpack_buffer(c, buffer, "b", &row);
		if (rc)
			return rc;
		b = lookup_bridge_by_shipid(c->shipid);
		if (b < 0)
			return 0;
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
		return 0;
	case OPCODE_SET_WAYPOINT_ADD_ROW:
		rc = read_and_unpack_buffer(c, buffer, "SSS",
					&value[0], (int32_t) UNIVERSE_DIM,
					&value[1], (int32_t) UNIVERSE_DIM,
					&value[2], (int32_t) UNIVERSE_DIM);
		if (rc)
			return rc;
		b = lookup_bridge_by_shipid(c->shipid);
		if (b < 0)
			return 0;
		if (bridgelist[b].nwaypoints < MAXWAYPOINTS) {
			bridgelist[b].waypoint[bridgelist[b].nwaypoints].x = value[0];
			bridgelist[b].waypoint[bridgelist[b].nwaypoints].y = value[1];
			bridgelist[b].waypoint[bridgelist[b].nwaypoints].z = value[2];
			bridgelist[b].nwaypoints++;
			set_waypoints_dirty_all_clients_on_bridge(c->shipid, 1);
		}
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

	if (!lua_enscript_enabled)
		return 0;

	/* TODO: Send this client side instead of storing server side. */
#define LUASCRIPTDIR "share/snis/luascripts"
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

static int process_exec_lua_script(struct game_client *c)
{
	unsigned char buffer[sizeof(struct lua_script_packet)];
	char txt[256];
	int rc;
	uint8_t len;
	char scriptname[PATH_MAX];

	rc = read_and_unpack_buffer(c, buffer, "b", &len);
	if (rc)
		return rc;
	rc = snis_readsocket(c->socket, txt, len);
	if (rc)
		return rc;
	txt[len] = '\0';

#define LUASCRIPTDIR "share/snis/luascripts"
	snprintf(scriptname, sizeof(scriptname) - 1, "%s/%s", LUASCRIPTDIR, txt);
	pthread_mutex_lock(&universe_mutex);
	enqueue_lua_command(scriptname); /* queue up for execution by main thread. */
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
	o->tsd.ship.nai_entries++;

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
	switch (transmitter->type) {
	case OBJTYPE_STARBASE:
		send_comms_packet(transmitter->tsd.starbase.name, 0, transmission);
		break;
	default:
		send_comms_packet(transmitter->sdata.name, 0, transmission);
		break;
	}
error:
	pthread_mutex_unlock(&universe_mutex);
	return 0;
}

static int l_comms_channel_transmit(lua_State *l)
{
	const char *name = luaL_checkstring(l, 1);
	const double channel = luaL_checknumber(l, 2);
	const char *transmission = luaL_checkstring(l, 3);
	uint32_t ch;

	ch = (uint32_t) channel;
	send_comms_packet((char *) name, ch, transmission);
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
		lua_pushstring(l, o->tsd.starbase.name);
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
	uint32_t oid = (uint32_t) id;
	uint8_t bvalue;
	int i, b, damage_delta;
	struct snis_entity *o;
	int system_number;

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
	distribute_damage_to_damcon_system_parts(o, &bridgelist[b].damcon,
			damage_delta, system_number);
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
	const int bufsize = sizeof(struct request_mainscreen_view_change) -
					sizeof(uint8_t);
	unsigned char buffer[bufsize];
	double view_angle;
	uint8_t view_mode;

	rc = read_and_unpack_buffer(c, buffer, "Rb", &view_angle, &view_mode);
	if (rc)
		return rc;
	/* Rebuild packet and send to all clients with main screen role */
	send_packet_to_all_clients_on_a_bridge(c->shipid, 
			snis_opcode_pkt("bRb", OPCODE_MAINSCREEN_VIEW_MODE,
					view_angle, view_mode),
			ROLE_MAIN);
	return 0;
}

static void set_red_alert_mode(struct game_client *c, unsigned char new_alert_mode)
{
	send_packet_to_all_clients_on_a_bridge(c->shipid,
			snis_opcode_pkt("bb", OPCODE_REQUEST_REDALERT, new_alert_mode), ROLE_ALL);
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

		if (o->type != OBJTYPE_SHIP1)
			delete_from_clients_and_server(o);
	}
	pthread_mutex_unlock(&universe_mutex);
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
	if (safe_mode)
		snis_queue_add_global_text_to_speech("Safe mode enabled.");
	else
		snis_queue_add_global_text_to_speech("Safe mode disabled.");
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
	snprintf(scriptname, sizeof(scriptname) - 1, "%s/%s", LUASCRIPTDIR, upper_scriptname);
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

	oid = (uint32_t) lua_oid;
	attribute = lua_tostring(lua_state, 2);
	pthread_mutex_lock(&universe_mutex);
	i = lookup_by_id(oid);
	if (i < 0)
		goto error;
	o = &go[i];
	if (o->type != OBJTYPE_SHIP1 && o->type != OBJTYPE_SHIP2)
		goto error;
	base_address[0] = o;
	kvs = lookup_key_entry(snis_entity_kvs, attribute);
	if (!kvs)
		goto error;
	switch (kvs->type) {
	case KVS_STRING:
		str = (char *) o + kvs->address_offset;
		lua_pushstring(l, str);
		break;
	case KVS_UINT64:
		if (key_value_get_value(kvs, attribute, base_address, &ui64, sizeof(ui64)) != sizeof(ui64))
			goto error;
		lua_pushnumber(l, (double) ui64);
		break;
	case KVS_UINT32:
		if (key_value_get_value(kvs, attribute, base_address, &ui32, sizeof(ui32)) != sizeof(ui32))
			goto error;
		lua_pushnumber(l, (double) ui32);
		break;
	case KVS_UINT16:
		if (key_value_get_value(kvs, attribute, base_address, &ui16, sizeof(ui16)) != sizeof(ui16))
			goto error;
		lua_pushnumber(l, (double) ui16);
		break;
	case KVS_UINT8:
		if (key_value_get_value(kvs, attribute, base_address, &ui8, sizeof(ui16)) != sizeof(ui8))
			goto error;
		lua_pushnumber(l, (double) ui8);
		break;
	case KVS_INT64:
		if (key_value_get_value(kvs, attribute, base_address, &i64, sizeof(i64)) != sizeof(i64))
			goto error;
		lua_pushnumber(l, (double) i64);
		break;
	case KVS_INT32:
		if (key_value_get_value(kvs, attribute, base_address, &i32, sizeof(i32)) != sizeof(i32))
			goto error;
		lua_pushnumber(l, (double) i32);
		break;
	case KVS_INT16:
		if (key_value_get_value(kvs, attribute, base_address, &i16, sizeof(i16)) != sizeof(i16))
			goto error;
		lua_pushnumber(l, (double) i16);
		break;
	case KVS_INT8:
		if (key_value_get_value(kvs, attribute, base_address, &i8, sizeof(i8)) != sizeof(i8))
			goto error;
		lua_pushnumber(l, (double) i8);
		break;
	case KVS_DOUBLE:
		if (key_value_get_value(kvs, attribute, base_address, &dbl, sizeof(dbl)) != sizeof(dbl))
			goto error;
		lua_pushnumber(l, dbl);
		break;
	case KVS_FLOAT:
		if (key_value_get_value(kvs, attribute, base_address, &flt, sizeof(flt)) != sizeof(flt))
			goto error;
		lua_pushnumber(l, (double) flt);
		break;
	default:
		goto error;
	}
	pthread_mutex_unlock(&universe_mutex);
	return 1;
error:
	pthread_mutex_unlock(&universe_mutex);
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
	if (i >= snis_object_pool_highest_object(pool))
		goto out;
	o = &go[i];
	if (o->type != OBJTYPE_SHIP1 && o->type != OBJTYPE_CARGO_CONTAINER)
		goto out;
	if (index < -1 || index >= ncommodities)
		goto out;
	if (o->type == OBJTYPE_SHIP1) {
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
	const double base_price = luaL_checknumber(l, 3);
	const double volatility = luaL_checknumber(l, 4);
	const double legality = luaL_checknumber(l, 5);
	const double econ_sensitivity = luaL_checknumber(l, 6);
	const double govt_sensitivity = luaL_checknumber(l, 7);
	const double tech_sensitivity = luaL_checknumber(l, 8);
	const double odds = luaL_checknumber(l, 9);
	int iodds = odds;

	int n = add_commodity(&commodity, &ncommodities,
		name, units, base_price, volatility, legality,
		econ_sensitivity, govt_sensitivity, tech_sensitivity, iodds);
	lua_pushnumber(l, (double) n);
	return 1;
}

static int l_reset_player_ship(lua_State *l)
{
	const double lua_oid = luaL_checknumber(l, 1);
	struct snis_entity *o;
	uint32_t oid = (uint32_t) lua_oid;
	int i, b;

	pthread_mutex_lock(&universe_mutex);
	i = lookup_by_id(oid);
	if (i < 0)
		goto out;
	o = &go[i];
	if (o->type != OBJTYPE_SHIP1)
		goto out;
	init_player(o, 1, NULL);
	b = lookup_bridge_by_shipid(o->id);
	if (b >= 0)
		clear_bridge_waypoints(b);
	o->timestamp = universe_timestamp;
	pthread_mutex_unlock(&universe_mutex);
	lua_pushnumber(l, 0.0);
	return 1;
out:
	pthread_mutex_unlock(&universe_mutex);
	lua_pushnil(l);
	return 1;
}

static int process_create_item(struct game_client *c)
{
	unsigned char buffer[14];
	unsigned char item_type;
	double x, y, z, r;
	int rc, i = -1;

	rc = read_and_unpack_buffer(c, buffer, "bSSS", &item_type,
			&x, (int32_t) UNIVERSE_DIM,
			&y, (int32_t) UNIVERSE_DIM,
			&z, (int32_t) UNIVERSE_DIM);
	if (rc)
		return rc;

	pthread_mutex_lock(&universe_mutex);
	switch (item_type) {
	case OBJTYPE_SHIP2:
		i = add_ship(-1, 1);
		break;
	case OBJTYPE_STARBASE:
		i = add_starbase(x, y, z, 0, 0, 0, snis_randn(100), -1);
		break;
	case OBJTYPE_PLANET:
		r = (float) snis_randn(MAX_PLANET_RADIUS - MIN_PLANET_RADIUS) +
					MIN_PLANET_RADIUS;
		i = add_planet(x, y, z, r, 0);
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

static int nl_find_nearest_object_of_type(uint32_t id, int objtype);

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
	if (d > planet->tsd.planet.radius * STANDARD_ORBIT_RADIUS_FACTOR * 3.00) {
		pthread_mutex_unlock(&universe_mutex);
		snis_queue_add_text_to_speech("We are too far from a planet to enter standard orbit",
					ROLE_TEXT_TO_SPEECH, c->shipid);
		printf("radius = %lf, d = %lf, limit = %lf\n", planet->tsd.planet.radius, d,
			planet->tsd.planet.radius * STANDARD_ORBIT_RADIUS_FACTOR);
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

	b = lookup_bridge_by_shipid(o->id);
	if (b < 0) {
		return 0;
	}
	if (enough_oomph) {
		wfactor = ((double) o->tsd.ship.warpdrive / 255.0) * (XKNOWN_DIM / 2.0);
		quat_rot_vec(&warpvec, &rightvec, &o->orientation);
		bridgelist[b].warpx = o->x + wfactor * warpvec.v.x;
		bridgelist[b].warpy = o->y + wfactor * warpvec.v.y;
		bridgelist[b].warpz = o->z + wfactor * warpvec.v.z;
		o->tsd.ship.warp_time = 85; /* 8.5 seconds */
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

	if (ship->tsd.ship.torpedoes < 0)
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

static int process_request_torpedo(struct game_client *c)
{
	struct snis_entity *ship = &go[c->ship_index];
	union vec3 forwardvec = { { TORPEDO_VELOCITY, 0.0f, 0.0f } };
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
	vec3_mul_self(&tv, TORPEDO_VELOCITY);
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

	pthread_mutex_lock(&universe_mutex);
	if (oid == (uint32_t) 0xffffffff) /* nothing selected */
		goto miningbotfail;
	i = lookup_by_id(oid);
	if (i < 0)
		goto miningbotfail;
	if (ship->tsd.ship.mining_bots <= 0) /* no bots left */
		goto miningbotfail;
	if (go[i].type != OBJTYPE_ASTEROID && go[i].type != OBJTYPE_DERELICT)
		goto miningbotfail;

	i = add_mining_bot(ship, oid);
	if (i < 0)
		goto miningbotfail;
	pthread_mutex_unlock(&universe_mutex);
	queue_add_text_to_speech(c, "Mining robot deployed");
	return;

miningbotfail:
	/* TODO: make special miningbot failure sound */
	snis_queue_add_sound(LASER_FAILURE, ROLE_SOUNDSERVER, ship->id);
	pthread_mutex_unlock(&universe_mutex);
	queue_add_text_to_speech(c, "No target selected for mining robot");
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
	unsigned char buffer[10];
	double vx, vz;
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

	vx = LASER_VELOCITY * cos(o->heading);
	vz = LASER_VELOCITY * -sin(o->heading);
	add_laser(o->x, 0.0, o->z, vx, 0.0, vz, NULL, o->id);
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
	if (rc != 0)
		goto protocol_error;

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
		case OPCODE_REQUEST_GUNYAW:
			rc = process_request_yaw(c, do_gun_yaw);
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
#if 0
		case OPCODE_REQUEST_THRUST:
			rc = process_request_thrust(c);
			if (rc)
				goto protocol_error;
			break;
#endif
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
			rc = process_cycle_camera_point_of_view(c, opcode, ROLE_NAVIGATION, 4);
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
		default:
			goto protocol_error;
	}
	last_successful_opcode = opcode;
	return;

protocol_error:
	log_client_info(SNIS_ERROR, c->socket, "bad opcode protocol violation\n");
	snis_log(SNIS_ERROR, "Protocol error in process_instructions_from_client, opcode = %hu\n", opcode);
	snis_log(SNIS_ERROR, "Last successful opcode was %d (0x%hx)\n", last_successful_opcode,
			last_successful_opcode);
	snis_print_last_buffer(c->socket);
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
	while (1) {
		process_instructions_from_client(c);
		if (c->socket < 0)
			break;
	}
	log_client_info(SNIS_INFO, c->socket, "client reader thread exiting\n");
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
static void send_update_torpedo_packet(struct game_client *c,
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
		if (o->tsd.ship.overheating_damage_done)
			send_silent_ship_damage_packet(o); /* sends to all clients. */
		break;
	case OBJTYPE_SHIP2:
		send_econ_update_ship_packet(c, o);
		break;
	case OBJTYPE_ASTEROID:
		send_update_asteroid_packet(c, o);
		break;
	case OBJTYPE_CARGO_CONTAINER:
		send_update_cargo_container_packet(c, o);
		break;
	case OBJTYPE_DERELICT:
		send_update_derelict_packet(c, o);
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
	case OBJTYPE_DEBRIS:
		break;
	case OBJTYPE_SPARK:
		break;
	case OBJTYPE_TORPEDO:
		send_update_torpedo_packet(c, o);
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
	default:
		break;
	}
}

static void queue_up_client_object_sdata_update(struct game_client *c, struct snis_entity *o)
{
	switch (o->type) {
	case OBJTYPE_SHIP1:
		send_update_sdata_packets(c, o);
		/* TODO: remove the next two lines when send_update_sdata_packets does it already */
		if (o == &go[c->ship_index])
			pack_and_send_ship_sdata_packet(c, o);
		break;
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
	const double threshold = (XKNOWN_DIM / 2) * (XKNOWN_DIM / 2);

	dx = (ship->x - o->x);
	dz = (ship->z - o->z);
	dist = (dx * dx) + (dz * dz);
	return (dist > threshold);
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
		pb = packed_buffer_allocate(1 + 4 + 4 + 4 + LOCATIONSIZE);
		packed_buffer_append(pb, "bSSS", OPCODE_UPDATE_SOLARSYSTEM_LOCATION,
					starmap[i].x, (int32_t) 1000.0,
					starmap[i].y, (int32_t) 1000.0,
					starmap[i].z, (int32_t) 1000.0);
		packed_buffer_append_raw(pb, starmap[i].name, LOCATIONSIZE);
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

#define GO_TOO_FAR_UPDATE_PER_NTICKS 7

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
				(universe_timestamp + i) % GO_TOO_FAR_UPDATE_PER_NTICKS != 0) {

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
		fprintf(stderr, "%s: disconnecting client for failed bridge verification\n",
			logprefix());
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
			default:
				ai[i] = '?';
				break;
			}
		}
	}
	n = o->tsd.ship.nai_entries - 1;
	if (n < 0 || o->tsd.ship.ai[n].ai_mode != AI_MODE_ATTACK)
		victim_id = -1;
	else
		victim_id = o->tsd.ship.ai[n].u.attack.victim_id;

	pb_queue_to_client(c, packed_buffer_new("bwwhSSSQwbb", opcode,
			o->id, o->timestamp, o->alive, o->x, (int32_t) UNIVERSE_DIM,
			o->y, (int32_t) UNIVERSE_DIM, o->z, (int32_t) UNIVERSE_DIM,
			&o->orientation, victim_id, o->tsd.ship.shiptype, o->sdata.faction));

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
	send_packet_to_all_clients_on_a_bridge(c->shipid, pb, ROLE_ALL);
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

static void send_comms_packet(char *sender, uint32_t channel, const char *str)
{
	struct packed_buffer *pb;
	char tmpbuf[100];

	snprintf(tmpbuf, 99, "%s%s", sender, str);
	pb = packed_buffer_allocate(sizeof(struct comms_transmission_packet) + 100);
	packed_buffer_append(pb, "bb", OPCODE_COMMS_TRANSMISSION, (uint8_t) strlen(tmpbuf) + 1);
	packed_buffer_append_raw(pb, tmpbuf, strlen(tmpbuf) + 1);
	if (channel == 0)
		send_packet_to_all_clients(pb, ROLE_ALL);
	else
		send_packet_to_all_bridges_on_channel(channel, pb, ROLE_ALL);
	/* Send comms to any lua scripts that are listening */
	queue_comms_to_lua_channels(sender, channel, str);
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

	tloading = (uint8_t) (o->tsd.ship.torpedoes_loading & 0x0f);
	tloaded = (uint8_t) (o->tsd.ship.torpedoes_loaded & 0x0f);
	tloading = tloading | (tloaded << 4);

	pb = packed_buffer_allocate(sizeof(struct update_ship_packet));
	packed_buffer_append(pb, "bwwhSSS", opcode, o->id, o->timestamp, o->alive,
			o->x, (int32_t) UNIVERSE_DIM, o->y, (int32_t) UNIVERSE_DIM,
			o->z, (int32_t) UNIVERSE_DIM);
	packed_buffer_append(pb, "RRRwwRRRbbbwwbbbbbbbbbbbbbwQQQbbbb",
			o->tsd.ship.yaw_velocity,
			o->tsd.ship.pitch_velocity,
			o->tsd.ship.roll_velocity,
			o->tsd.ship.torpedoes, o->tsd.ship.power,
			o->tsd.ship.gun_yaw_velocity,
			o->tsd.ship.sci_heading,
			o->tsd.ship.sci_beam_width,
			tloading, throttle, rpm, fuel, oxygen, o->tsd.ship.temp,
			o->tsd.ship.scizoom, o->tsd.ship.weapzoom, o->tsd.ship.navzoom,
			o->tsd.ship.mainzoom,
			o->tsd.ship.warpdrive, o->tsd.ship.requested_warpdrive,
			o->tsd.ship.requested_shield, o->tsd.ship.phaser_charge,
			o->tsd.ship.phaser_wavelength, o->tsd.ship.shiptype,
			o->tsd.ship.reverse, o->tsd.ship.trident,
			o->tsd.ship.ai[0].u.attack.victim_id,
			&o->orientation.vec[0],
			&o->tsd.ship.sciball_orientation.vec[0],
			&o->tsd.ship.weap_orientation.vec[0],
			o->tsd.ship.in_secure_area,
			o->tsd.ship.docking_magnets,
			o->tsd.ship.emf_detector,
			o->tsd.ship.nav_mode);
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
	pb_queue_to_client(c, snis_opcode_pkt("bwwSSSbbbb", OPCODE_UPDATE_ASTEROID, o->id, o->timestamp,
					o->x, (int32_t) UNIVERSE_DIM,
					o->y, (int32_t) UNIVERSE_DIM,
					o->z, (int32_t) UNIVERSE_DIM,
					o->tsd.asteroid.carbon,
					o->tsd.asteroid.nickeliron,
					o->tsd.asteroid.silicates,
					o->tsd.asteroid.preciousmetals));
}

static void send_update_cargo_container_packet(struct game_client *c,
	struct snis_entity *o)
{
	pb_queue_to_client(c, snis_opcode_pkt("bwwSSS", OPCODE_UPDATE_CARGO_CONTAINER, o->id, o->timestamp,
					o->x, (int32_t) UNIVERSE_DIM,
					o->y, (int32_t) UNIVERSE_DIM,
					o->z, (int32_t) UNIVERSE_DIM));
}

static void send_update_derelict_packet(struct game_client *c,
	struct snis_entity *o)
{
	pb_queue_to_client(c, snis_opcode_pkt("bwwSSSbbb", OPCODE_UPDATE_DERELICT, o->id, o->timestamp,
					o->x, (int32_t) UNIVERSE_DIM,
					o->y, (int32_t) UNIVERSE_DIM,
					o->z, (int32_t) UNIVERSE_DIM,
					o->tsd.derelict.shiptype,
					o->tsd.derelict.fuel,
					o->tsd.derelict.oxygen));
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

	pb_queue_to_client(c, snis_opcode_pkt("bwwSSSSwbbbbhbbbSbhbb", OPCODE_UPDATE_PLANET, o->id, o->timestamp,
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
					o->tsd.planet.ring_selector));
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
	pb_queue_to_client(c, snis_opcode_pkt("bwwSSSQ", OPCODE_UPDATE_STARBASE,
					o->id, o->timestamp,
					o->x, (int32_t) UNIVERSE_DIM,
					o->y, (int32_t) UNIVERSE_DIM,
					o->z, (int32_t) UNIVERSE_DIM,
					&o->orientation));
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
	pb_queue_to_client(c, snis_opcode_pkt("bwwSSShhhb", OPCODE_UPDATE_EXPLOSION, o->id, o->timestamp,
				o->x, (int32_t) UNIVERSE_DIM, o->y, (int32_t) UNIVERSE_DIM,
				o->z, (int32_t) UNIVERSE_DIM,
				o->tsd.explosion.nsparks, o->tsd.explosion.velocity,
				o->tsd.explosion.time, o->tsd.explosion.victim_type));
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
	pb_queue_to_client(c, snis_opcode_pkt("bwwSSSSSSQbb", OPCODE_UPDATE_BLOCK,
					o->id, o->timestamp,
					o->x, (int32_t) UNIVERSE_DIM,
					o->y, (int32_t) UNIVERSE_DIM,
					o->z, (int32_t) UNIVERSE_DIM,
					o->tsd.block.sx, (int32_t) UNIVERSE_DIM,
					o->tsd.block.sy, (int32_t) UNIVERSE_DIM,
					o->tsd.block.sz, (int32_t) UNIVERSE_DIM,
					&o->orientation,
					o->tsd.block.block_material_index,
					o->tsd.block.health));
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
	pb_queue_to_client(c, snis_opcode_pkt("bwwSSS", OPCODE_UPDATE_SPACEMONSTER, o->id, o->timestamp,
					o->x, (int32_t) UNIVERSE_DIM,
					o->z, (int32_t) UNIVERSE_DIM,
					o->tsd.spacemonster.zz, (int32_t) UNIVERSE_DIM));
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
	fprintf(stderr, "%s: new client: sn='%s', pw='%s', create = %hhu\n",
			logprefix(), app.shipname, app.password, app.new_ship);

	c->bridge = lookup_bridge(app.shipname, app.password);
	fprintf(stderr, "%s: c->bridge = %d\n", logprefix(), c->bridge);
	c->role = app.role;
	if (c->bridge == -1) { /* didn't find our bridge, make a new one. */
		double x, z;
		fprintf(stderr, "%s: didn't find bridge, make new one\n", logprefix());

		for (int i = 0; i < 100; i++) {
			x = XKNOWN_DIM * (double) rand() / (double) RAND_MAX;
			z = ZKNOWN_DIM * (double) rand() / (double) RAND_MAX;
			if (dist3d(x - SUNX, 0, z - SUNZ) > SUN_DIST_LIMIT)
				break;
		}
		c->ship_index = add_player(x, z, 0.0, 0.0, M_PI / 2.0, app.warpgate_number);
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

	snis_sha1_hash(bridgelist[c->bridge].shipname, bridgelist[c->bridge].password,
			bridgelist[c->bridge].pwdhash);
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
		sprintf(portstr, "%d", port);
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
		send_silent_ship_damage_packet(o);
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
			"%s: did not find bridge for id:%d, index=%lu, alive=%d: %s:%d\n",
				logprefix(), o->id, go_index(o), o->alive, __FILE__, __LINE__);
		return;
	}

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

		print_hash("requesting verification of hash ", bridgelist[bridge].pwdhash);
		pb = packed_buffer_allocate(21);
		if (bridgelist[bridge].requested_creation)
			opcode = SNISMV_OPCODE_VERIFY_CREATE;
		else
			opcode = SNISMV_OPCODE_VERIFY_EXISTS;
		packed_buffer_append(pb, "br", opcode, bridgelist[bridge].pwdhash, (uint16_t) 20);
		bridgelist[bridge].requested_verification = 1;
		queue_to_multiverse(multiverse_server, pb);
		return;
	} else {
		fprintf(stderr, "%s: not requesting verification\n", logprefix());
	}

	/* Skip updating multiverse server if the bridge isn't verified yet. */
	if (bridgelist[bridge].verified != BRIDGE_VERIFIED) {
		fprintf(stderr, "%s: bridge is not verified, not updating multiverse\n",
			logprefix());
		return;
	}

	/* Update the ship */
	pb = build_bridge_update_packet(o, bridgelist[bridge].pwdhash);
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

	memset(&gs, 0, sizeof(gs));
	snis_log(SNIS_INFO, "port = %hu\n", port);
	gs.port = htons(port);
	snis_log(SNIS_INFO, "gs.port = %hu\n", gs.port);
		
	strncpy(gs.server_nickname, servernick, 14);
	strncpy(gs.game_instance, gameinstance, 19);
	strncpy(gs.location, location, 19);
	strcpy(gs.game_type, "SNIS");

	if (ssgl_get_primary_host_ip_addr(&gs.ipaddr) != 0)
		snis_log(SNIS_WARN, "Failed to get local ip address.\n");

	snis_log(SNIS_INFO, "Registering game server\n");
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
	if (rc)
		fprintf(stderr, "Error executing lua script: %s\n",
			lua_tostring(lua_state, -1));
	return rc;
}

static void process_lua_commands(void)
{
	char lua_command[PATH_MAX];
	int rc;

	pthread_mutex_lock(&universe_mutex);
	for (;;) {
		dequeue_lua_command(lua_command, sizeof(lua_command));
		if (lua_command[0] == '\0') /* empty string */
			break;

		pthread_mutex_unlock(&universe_mutex);
		rc = luaL_dofile(lua_state, lua_command);
		if (rc)
			fprintf(stderr, "Error executing lua script: %s\n",
				lua_tostring(lua_state, -1));
		pthread_mutex_lock(&universe_mutex);
	}
	pthread_mutex_unlock(&universe_mutex);
}

static void lua_teardown(void)
{
	lua_close(lua_state);
	lua_state = NULL;
}

static void override_asset_dir(void)
{
	char *d;

	asset_dir = default_asset_dir;
	d = getenv("SNIS_ASSET_DIR");
	if (!d)
		return;
	asset_dir = d;
}

static int read_ship_types()
{
	char path[PATH_MAX];

	sprintf(path, "%s/%s", asset_dir, "ship_types.txt");

	ship_type = snis_read_ship_types(path, &nshiptypes);
	if (!ship_type) {
		fprintf(stderr, "Unable to read ship types from %s", path);
		if (errno)
			fprintf(stderr, "%s: %s\n", path, strerror(errno));
		return -1;
	}
	return 0;
}

static int read_factions(void)
{
	char path[PATH_MAX];

	sprintf(path, "%s/%s", asset_dir, "factions.txt");

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

	sprintf(path, "%s/solarsystems/%s/assets.txt", asset_dir, solarsystem_name);

	s = solarsystem_asset_spec_read(path);
	if (!s) {
		fprintf(stderr, "Unable to read solarsystem asset spec from '%s'", path);
		if (errno)
			fprintf(stderr, "%s: %s\n", path, strerror(errno));
	}
	return s;
}

static void set_random_seed(void)
{
	char *seed = getenv("SNISRAND");
	int i, rc;

	if (!seed)
		return;

	rc = sscanf(seed, "%d", &i);
	if (rc != 1)
		return;

	snis_srand((unsigned int) i);
	srand(i);
	mtwist_seed = (uint32_t) i;
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

#if 0
static struct docking_port_attachment_point **read_docking_port_info(
		struct starbase_file_metadata starbase_metadata[], int n)
{
	int i;
	struct docking_port_attachment_point **d = malloc(sizeof(*d) * n);
	memset(d, 0, sizeof(*d) * n);
	for (i = 0; i < n; i++) {
		if (!starbase_metadata[i].docking_port_file)
			continue;
		d[i] = read_docking_port_attachments(starbase_metadata[i].docking_port_file,
				STARBASE_SCALE_FACTOR);
	}
	return d;
}
#endif

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
	for (i = 0; i < snis_object_pool_highest_object(pool); i++) {
		switch (go[i].type) {
		case OBJTYPE_STARBASE:
			if (strcmp(go[i].tsd.starbase.name, w) == 0) {
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
	snis_nl_add_synonym("boost", "raise");
	snis_nl_add_synonym("increase", "raise");
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
}

static const struct noun_description_entry {
	char *noun;
	char *description;
} noun_description[] = {
	{ "drive", "The warp drive is a powerful sub-nuclear device that enables faster than light space travel." },
	{ "warp drive", "The warp drive is a powerful sub-nuclear device that enables faster than light space travel." },
	{ "impulse drive", "The impulse drive is a sub light speed conventional thruster." },
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
		return o->tsd.starbase.name;
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
		pthread_mutex_unlock(&universe_mutex);
		mt = mtwist_init(go[i].tsd.planet.description_seed);
		ss_planet_type = go[i].tsd.planet.solarsystem_planet_type;
		planet_type_str = solarsystem_assets->planet_type[ss_planet_type];
		planet_description(mt, description, 250, 254, planet_type_from_string(planet_type_str));
		mtwist_free(mt);
		queue_add_text_to_speech(c, description);
		return;
	case OBJTYPE_ASTEROID:
		pthread_mutex_unlock(&universe_mutex);
		sprintf(description, "%s is a small and rather ordinary asteroid", go[i].sdata.name);
		queue_add_text_to_speech(c, description);
		return;
	case OBJTYPE_STARBASE:
		if (go[i].tsd.starbase.associated_planet_id < 0) {
			sprintf(description, "%s is a star base in deep space", go[i].tsd.starbase.name);
		} else {
			planet = lookup_by_id(go[i].tsd.starbase.associated_planet_id);
			if (planet < 0)
				sprintf(description, "%s is a star base in deep space", go[i].tsd.starbase.name);
			else
				sprintf(description, "%s is a star base in orbit around the planet %s",
					go[i].tsd.starbase.name, go[planet].sdata.name);
		}
		pthread_mutex_unlock(&universe_mutex);
		queue_add_text_to_speech(c, description);
		return;
	case OBJTYPE_SHIP2:
		planet = lookup_by_id(go[i].tsd.ship.home_planet);
		pthread_mutex_unlock(&universe_mutex);
		if (planet >= 0)
			sprintf(extradescription, " originating from the planet %s",
				go[planet].sdata.name);
		else
			strcpy(extradescription, " of unknown origin");
		sprintf(description, "%s is a %s class ship %s", go[i].sdata.name,
				ship_type[go[i].tsd.ship.shiptype].class, extradescription);
		queue_add_text_to_speech(c, description);
		return;
	case OBJTYPE_SHIP1:
		pthread_mutex_unlock(&universe_mutex);
		sprintf(description, "%s is a human piloted wombat class space ship", go[i].sdata.name);
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
	else if (strcmp(argv[object], "selection") == 0 ||
		strcmp(argv[object], "target") == 0) {
		if (bridgelist[c->bridge].science_selection < 0)
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
		if (bridgelist[c->bridge].science_selection < 0)
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
		sprintf(destination_name, "%s", nl_get_object_name(&go[i]));
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
		sprintf(destination_name, "%s", argv[second_noun]);
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
		sprintf(directions,
			"Course to %s%s calculated.  Destination lies at bearing %3.0lf, mark %3.0lf at a distance of %.0f clicks",
			modifier, destination_name, heading, mark, distance);
	if (calculate_distance)
		sprintf(directions, "The distance to %s%s is %.0lf clicks", modifier, destination_name, distance);
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

	first_noun = -1;
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

	sprintf(fuel_report, "Fuel tanks are at %2.0f percent.", fuel_level);
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
		sprintf(answer, "Sorry, I can't seem to find the %s, it must be a memory error", word);
		queue_add_text_to_speech(c, answer);
		return;
	}
	bytevalue = (uint8_t *) &go[i];
	bytevalue += offset;
	new_value = limit(c, new_value);
	*bytevalue = new_value;
	pthread_mutex_unlock(&universe_mutex);
	sprintf(answer, "setting the %s to %3.0f percent", word, fraction * 100.0);
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

static void nl_set_zoom(struct game_client *c, char *word, float value)
{
	char *zoom_name;

	switch (bridgelist[c->bridge].current_displaymode) {
	case DISPLAYMODE_MAINSCREEN:
		zoom_name = "main screen zoom";
		nl_set_mainzoom(c, zoom_name, value);
		break;
	case DISPLAYMODE_NAVIGATION:
		zoom_name = "navigation zoom";
		nl_set_navzoom(c, zoom_name, value);
		break;
	case DISPLAYMODE_WEAPONS:
		zoom_name = "weapons zoom";
		nl_set_weapzoom(c, zoom_name, value);
		break;
	case DISPLAYMODE_SCIENCE:
		zoom_name = "science zoom";
		nl_set_scizoom(c, zoom_name, value);
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
	{ "shield power", nl_set_shield_power, },
	{ "tractor beam power", nl_set_tractor_power, },
	{ "life support power", nl_set_lifesupport_power, },

	{ "maneuvering coolant", nl_set_maneuvering_coolant, },
	{ "impulse drive coolant", nl_set_impulse_coolant, },
	{ "warp drive coolant", nl_set_warp_coolant, },
	{ "sensor coolant", nl_set_sensor_coolant, },
	{ "communications coolant", nl_set_comms_coolant, },
	{ "phaser coolant", nl_set_phaser_coolant, },
	{ "shield coolant", nl_set_shield_coolant, },
	{ "tractor beam coolant", nl_set_tractor_coolant, },
	{ "life support coolant", nl_set_lifesupport_coolant, },
	{ "zoom", nl_set_zoom, },
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
		sprintf(answer, "Sorry, I do not know how to set the %s", argv[noun]);
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
	sprintf(answer, "Sorry, I do not understand how you want me to set the %s", argv[noun]);
	queue_add_text_to_speech(c, answer);
	return;
}

/* Assumes universe lock is held, will unlock */
static void nl_set_ship_course_to_dest_helper(struct game_client *c,
	struct snis_entity *ship,
	struct snis_entity *dest,
	char *name)
{
	union vec3 direction, right;
	union quat new_orientation;
	char *modifier;
	char reply[100];

	/* Calculate new desired orientation of ship pointing towards destination */
	right.v.x = 1.0;
	right.v.y = 0.0;
	right.v.z = 0.0;

	direction.v.x = dest->x - ship->x;
	direction.v.y = dest->y - ship->y;
	direction.v.z = dest->z - ship->z;
	vec3_normalize_self(&direction);

	quat_from_u2v(&new_orientation, &right, &direction, NULL);

	ship->tsd.ship.computer_desired_orientation = new_orientation;
	ship->tsd.ship.computer_steering_time_left = COMPUTER_STEERING_TIME;
	ship->tsd.ship.orbiting_object_id = 0xffffffff;

	if (dest->type == OBJTYPE_PLANET)
		modifier = "the planet ";
	else if (dest->type == OBJTYPE_ASTEROID)
		modifier = "the asteroid ";
	else if (dest->type == OBJTYPE_SHIP1 || dest->type == OBJTYPE_SHIP2)
		modifier = "the ship ";
	else
		modifier = "";

	pthread_mutex_unlock(&universe_mutex);
	sprintf(reply, "Setting course for %s%s.", modifier, name);
	queue_add_text_to_speech(c, reply);
	return;
}

/* E.g.: navigate to the star base one, navigate to warp gate seven */
static void nl_navigate_pnq(void *context, int argc, char *argv[], int pos[],
	union snis_nl_extra_data extra_data[])
{
	struct game_client *c = context;
	int i, prep, noun;
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
		sprintf(buffer, "SB-%02.0f", value);
		i = natural_language_object_lookup(NULL, buffer); /* slightly racy */
	} else if (strcasecmp(argv[noun], "gate") == 0) {
		sprintf(buffer, "WG-%02.0f", value);
		i = natural_language_object_lookup(NULL, buffer); /* slightly racy */
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
	int i = -1;
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
		sprintf(buffer, "SB-%02.0f", value);
		i = natural_language_object_lookup(NULL, buffer); /* slightly racy */
	} else if (strcasecmp(argv[settowhat], "gate") == 0) {
		sprintf(buffer, "WG-%02.0f", value);
		i = natural_language_object_lookup(NULL, buffer); /* slightly racy */
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
		sprintf(response, "Sorry, I cannot seem to disengage the %s.", argv[device]);
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

/* increase power to impulse, increase coolant to blah, raise shields to maximum */
static void nl_raise_or_lower_npa(void *context, int argc, char *argv[], int pos[],
		union snis_nl_extra_data extra_data[], int raise)
{
	struct game_client *c = context;
	nl_set_function setit = NULL;
	char answer[100];
	int i, noun, prep, adj;

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
		if (raise && strcasecmp(argv[adj], "maximum") == 0) {
			setit(c, argv[noun], 1.0);
			return;
		}
		if (!raise && strcasecmp(argv[adj], "minimum") == 0) {
			setit(c, argv[noun], 0.0);
			return;
		}
		sprintf(answer, "I don't know how to %s %s %s %s\n", raise ? "increase" : "decrease",
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
			sprintf(answer, "I don't know how to increase power to that.\n");
			queue_add_text_to_speech(c, answer);
			return;
		}
		setit(c, argv[adj], raise ? 1.0 : 0.0);
		return;
	} else if (strcasecmp(argv[noun], "coolant") == 0) {
		for (i = 0; i < ARRAYSIZE(nl_settable_coolant_thing); i++) {
			if (strcasecmp(nl_settable_coolant_thing[i].name, argv[adj]) == 0) {
				setit = nl_settable_coolant_thing[i].setfn;
				break;
			}
		}
		if (!setit) {
			sprintf(answer, "I don't know how to decrease power to that.\n");
			queue_add_text_to_speech(c, answer);
			return;
		}
		setit(c, argv[adj], raise ? 1.0 : 0.0);
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
		sprintf(answer, "Sorry, I do not know how to raise the %s", argv[noun]);
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
		sprintf(response, "Sorry, I cannot seem to engage the %s.", argv[device]);
		queue_add_text_to_speech(c, response);
		return;
	}

	if (strcasecmp("warp drive", argv[device]) == 0) {
		pthread_mutex_unlock(&universe_mutex);
		enough_oomph = do_engage_warp_drive(&go[i]);
		send_initiate_warp_packet(c, enough_oomph);
		sprintf(response, "%s engaged", argv[device]);
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
			sprintf(reply, "Sorry, I cannot seem to engage the %s.", argv[device]);
			queue_add_text_to_speech(c, reply);
			return;
		}
		pthread_mutex_unlock(&universe_mutex);
		turn_on_tractor_beam(c, &go[i], extra_data[target].external_noun.handle, 0);
		return;
	} else {
		sprintf(reply, "I do not understand how engaging the %s relates to the %s\n",
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
			sprintf(reply, "Sorry, I do not know what red alert %s means.", argv[prep]);
			queue_add_text_to_speech(c, reply);
			return;
		}
	}
	set_red_alert_mode(c, new_alert_mode);

no_understand:
	sprintf(reply, "Did you want red alert on or off?  I seem to have missed the preposition.");
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

	sprintf(reply, "Main screen displaying %s", argv[verb]);
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

	sprintf(reply, "Main screen displaying %s", argv[verb]);
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

static void nl_zoom_pq(void *context, int argc, char *argv[], int pos[], union snis_nl_extra_data extra_data[])
{
	struct game_client *c = context;
	int prep, number;
	float direction = 1.0;
	float amount;
	/* char *zoom_name; */

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

#if 1
	nl_set_zoom(c, "", amount);
#else
	switch (bridgelist[c->bridge].current_displaymode) {
	case DISPLAYMODE_MAINSCREEN:
		zoom_name = "main screen zoom";
		nl_set_mainzoom(c, zoom_name, amount);
		break;
	case DISPLAYMODE_NAVIGATION:
		zoom_name = "navigation zoom";
		nl_set_navzoom(c, zoom_name, amount);
		break;
	case DISPLAYMODE_WEAPONS:
		zoom_name = "weapons zoom";
		nl_set_weapzoom(c, zoom_name, amount);
		break;
	case DISPLAYMODE_SCIENCE:
		zoom_name = "science zoom";
		nl_set_scizoom(c, zoom_name, amount);
		break;
	default:
		goto no_understand;
	}
#endif
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
	sprintf(reply, "Targeting sensors on %s", namecopy);
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
		sprintf(buffer, "SB-%02.0f", amount);
		i = natural_language_object_lookup(NULL, buffer); /* slightly racy */
	} else if (strcmp(argv[noun], "gate") == 0) {
		sprintf(buffer, "WG-%02.0f", amount);
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
	sprintf(reply, "Targeting sensors on %s", namecopy);
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
	sprintf(dr[0].system, "Maneuvering");
	dr[1].percent = (int) (100.0 * (1.0 - (float) o->tsd.ship.damage.warp_damage / 255.0));
	sprintf(dr[1].system, "Impulse drive");
	dr[2].percent = (int) (100.0 * (1.0 - (float) o->tsd.ship.damage.impulse_damage / 255.0));
	sprintf(dr[2].system, "Warp drive");
	dr[3].percent = (int) (100.0 * (1.0 - (float) o->tsd.ship.damage.phaser_banks_damage / 255.0));
	sprintf(dr[3].system, "Phasers");
	dr[4].percent = (int) (100.0 * (1.0 - (float) o->tsd.ship.damage.comms_damage / 255.0));
	sprintf(dr[4].system, "Communications");
	dr[5].percent = (int) (100.0 * (1.0 - (float) o->tsd.ship.damage.sensors_damage / 255.0));
	sprintf(dr[5].system, "Sensors");
	dr[6].percent = (int) (100.0 * (1.0 - (float) o->tsd.ship.damage.shield_damage / 255.0));
	sprintf(dr[6].system, "Shields");
	dr[7].percent = (int) (100.0 * (1.0 - (float) o->tsd.ship.damage.tractor_damage / 255.0));
	sprintf(dr[7].system, "Tractor beam");
	pthread_mutex_unlock(&universe_mutex);

	qsort(dr, 8, sizeof(dr[0]), compare_damage_report_entries);

	sprintf(damage_report, "Damage Report. ");
	for (i = 0; i < 8; i++) {
		if (dr[i].percent > 80) {
			ok++;
			continue;
		}
		sprintf(next_bit, "%s %d%%. ", dr[i].system, dr[i].percent);
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
	sprintf(damage_report, "Fuel tanks are at %2.0f percent.",
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
	snis_nl_add_dictionary_verb("set",		"set",		"npq", nl_set_npq);
	snis_nl_add_dictionary_verb("set",		"set",		"npa", sorry_dave);
	snis_nl_add_dictionary_verb("set",		"set",		"npn", nl_set_npn);
	snis_nl_add_dictionary_verb("set",		"set",		"npan", nl_set_npn);
	snis_nl_add_dictionary_verb("set",		"set",		"npnq", nl_set_npnq);
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
	snis_nl_add_dictionary_verb("turn",		"turn",		"pn", sorry_dave);
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
	snis_nl_add_dictionary_verb("zoom",		"zoom",		"q", nl_zoom_q);
	snis_nl_add_dictionary_verb("zoom",		"zoom",		"p", sorry_dave);
	snis_nl_add_dictionary_verb("zoom",		"zoom",		"pq", nl_zoom_pq);
	snis_nl_add_dictionary_verb("shut",		"shut",		"an", sorry_dave);
	snis_nl_add_dictionary_verb("shut",		"shut",		"na", sorry_dave);
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
};

static char *default_lobbyhost = "localhost";
static char *default_lobby_servernick = "-";

static char lobby_gameinstance[GAMEINSTANCESIZE];
static char *lobbyhost = NULL;
static char *lobby_location = NULL;
static char *lobby_servernick = NULL;

static void process_options(int argc, char *argv[])
{
	int c;

	lobby_servernick = default_lobby_servernick;
	lobbyhost = default_lobbyhost;

	while (1) {
		int option_index;
		c = getopt_long(argc, argv, "ehi:L:l:m:n:s:v", long_options, &option_index);
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
	unsigned char pwdhash[20];
	int i, rc;
	unsigned char buffer[250];
	struct packed_buffer pb;
	struct snis_entity *o;
	double x, y, z;

#define bytes_to_read (sizeof(struct update_ship_packet) - 9 + 25 + 5 + \
			sizeof(struct power_model_data) + \
			sizeof(struct power_model_data) - 1)

	fprintf(stderr, "%s: process_update bridge 1\n", logprefix());
	memset(buffer, 0, sizeof(buffer));
	memset(pwdhash, 0, sizeof(pwdhash));
	rc = snis_readsocket(msi->sock, buffer, 20);
	if (rc != 0)
		return rc;
	memcpy(pwdhash, buffer, 20);
	print_hash("snis_server: update bridge, read 20 bytes: ", pwdhash);
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
	unsigned char buffer[22];
	unsigned char *pass;
	unsigned char *pwdhash;

	rc = snis_readsocket(msi->sock, buffer, 21);
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
		fprintf(stderr, "%s: reading from multiverse sock = %d...\n",
			logprefix(), msi->sock);
		rc = snis_readsocket(msi->sock, &opcode, sizeof(opcode));
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
	snis_print_last_buffer(msi->sock);
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

	assert(msi);

	fprintf(stderr, "%s: connecting to multiverse %s %hhu.%hhu.%hhu.%hhu/%hu\n",
		logprefix(), multiverse_server->location, x[0], x[1], x[2], x[3], port);
	char portstr[50];
	char hoststr[50];
	int flag = 1;

	sprintf(portstr, "%d", port);
	sprintf(hoststr, "%d.%d.%d.%d", x[0], x[1], x[2], x[3]);

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
	fprintf(stderr, "%s: writing SNIS_MULTIVERSE_VERSION (len = %d)\n",
			logprefix(), len);
	rc = snis_writesocket(sock, SNIS_MULTIVERSE_VERSION, len);
	fprintf(stderr, "%s: writesocket returned %d, sock = %d\n", logprefix(), rc, sock);
	if (rc < 0) {
		fprintf(stderr, "%s: snis_writesocket failed: %d (%d:%s)\n",
				logprefix(), rc, errno, strerror(errno));
		goto error;
	}
	memset(response, 0, sizeof(response));
	fprintf(stderr, "%s: reading SNIS_MULTIVERSE_VERSION (len = %d, sock = %d)\n",
			logprefix(), len, sock);
	rc = snis_readsocket(sock, response, len);
	fprintf(stderr, "%s: read socket returned %d (len = %d, sock = %d)\n",
			logprefix(), rc, len, sock);
	if (rc != 0) {
		fprintf(stderr, "%s: snis_readsocket failed: %d (%d:%s)\n",
				logprefix(), rc, errno, strerror(errno));
		fprintf(stderr, "response = '%s'\n", response);
		sock = -1;
	}
	response[len] = '\0';
	fprintf(stderr, "%s: got SNIS_MULTIVERSE_VERSION:'%s'\n",
			logprefix(), response);
	if (strcmp(response, SNIS_MULTIVERSE_VERSION) != 0) {
		fprintf(stderr, "%s: expected '%s' got '%s' from snis_multiverse\n",
			logprefix(), SNIS_MULTIVERSE_VERSION, response);
		goto error;
	}

	fprintf(stderr, "%s: connected to snis_multiverse (%hhu.%hhu.%hhu.%hhu/%hu on socket %d)\n",
		logprefix(), x[0], x[1], x[2], x[3], port, sock);

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
	fprintf(stderr, "%s: starting multiverse reader thread\n", logprefix());
	rc = create_thread(&msi->read_thread, multiverse_reader, msi, "sniss-mvrdr", 1);
	if (rc) {
		fprintf(stderr, "%s: Failed to create multiverse reader thread: %d '%s', '%s'\n",
			logprefix(), rc, strerror(rc), strerror(errno));
	}
	fprintf(stderr, "%s: started multiverse reader thread\n", logprefix());
	fprintf(stderr, "%s: starting multiverse writer thread\n", logprefix());
	rc = create_thread(&msi->write_thread, multiverse_writer, msi, "sniss-mvwrtr", 1);
	if (rc) {
		fprintf(stderr, "%s: Failed to create multiverse writer thread: %d '%s', '%s'\n",
			logprefix(), rc, strerror(rc), strerror(errno));
	}
	fprintf(stderr, "%s: started multiverse writer thread\n", logprefix());
	freeaddrinfo(mvserverinfo);
	return;

error:
	if (sock >= 0) {
		shutdown(sock, SHUT_RDWR);
		close(sock);
		sock = -1;
	}
	freeaddrinfo(mvserverinfo);
	return;
}

static void disconnect_from_multiverse(struct multiverse_server_info *msi)
{
	unsigned char *x;

	assert(multiverse_server);
	x = (unsigned char *) &multiverse_server->ipaddr;
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
			if (strncasecmp(starmap[j].name, gameserver[i].location, LOCATIONSIZE) == 0) {
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
			strncpy(starmap[nstarmap_entries].name, gameserver[i].location, LOCATIONSIZE);
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
			if (strncasecmp(starmap[i].name, gameserver[j].location, LOCATIONSIZE) == 0) {
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

	fprintf(stderr, "%s: servers_changed_cb zzz\n", logprefix());
	if (!multiverse_server) {
		fprintf(stderr, "%s: multiverse_server not set zzz\n", logprefix());
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
		if (strncmp(gameserver[i].location, multiverse_server->location, LOCATIONSIZE) != 0)
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
		fprintf(stderr, "%s: multiverse servers same as before zzz\n", logprefix());
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
}

int main(int argc, char *argv[])
{
	int port, rc, i;
	struct timespec thirtieth_second;

	take_your_locale_and_shove_it();

	process_options(argc, argv);

	override_asset_dir();
	set_random_seed();
	init_natural_language_system();
	init_meshes();

	char commodity_path[PATH_MAX];
	sprintf(commodity_path, "%s/%s", asset_dir, "commodities.txt");
	commodity = read_commodities(commodity_path, &ncommodities);

	/* count possible contraband items */
	ncontraband = 0;
	for (i = 0; i < ncommodities; i++)
		if (commodity[i].legality < 1.0)
			ncontraband++;

	if (read_ship_types()) {
		fprintf(stderr, "%s: unable to read ship types\n", argv[0]);
		return -1;
	}

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
		currentTime = time_now_double();

		if (currentTime - nextTime > maxTimeBehind) {
			printf("too far behind sec=%f, skipping\n", currentTime - nextTime);
			nextTime = currentTime;
			discontinuity = 1;
		}
		if (currentTime >= nextTime) {
			move_objects(nextTime, discontinuity);
			process_lua_commands();

			discontinuity = 0;
			nextTime += delta;
		} else {
			double timeToSleep = nextTime-currentTime;
			if (timeToSleep > 0)
				sleep_double(timeToSleep);
		}
		simulate_slow_server(0);
	}
	space_partition_free(space_partition);
	lua_teardown();

	snis_close_logfile();

	if (rc) /* satisfy compiler */
		return 0; 
	return 0;
}
