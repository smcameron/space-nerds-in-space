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
#include "arbitrary_spin.h"
#include "snis.h"
#include "snis-culture.h"
#include "snis_log.h"
#include "mathutils.h"
#include "matrix.h"
#include "snis_alloc.h"
#include "snis_marshal.h"
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

#define CLIENT_UPDATE_PERIOD_NSECS 500000000
#define MAXCLIENTS 100

static uint32_t mtwist_seed = COMMON_MTWIST_SEED;

static int lua_enscript_enabled = 0;

struct network_stats netstats;
static int faction_population[5];
static int lowest_faction = 0;

static int nstarbase_models;
static struct starbase_file_metadata *starbase_metadata;
static struct docking_port_attachment_point **docking_port_info;
static struct passenger_data passenger[MAX_PASSENGERS];
static int npassengers;

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
	{ "REQUEST REMOTE FUEL DELIVERY", 0, 0, npc_menu_item_not_implemented },
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
	{ "TRANSPORT ORES TO CARGO BAYS", 0, 0, npc_menu_item_mining_bot_transport_ores },
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

struct game_client {
	int socket;
	pthread_t read_thread;
	pthread_t write_thread;
	pthread_attr_t read_attr, write_attr;

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
#define COMPUTE_AVERAGE_TO_CLIENT_BUFFER_SIZE 0
#if COMPUTE_AVERAGE_TO_CLIENT_BUFFER_SIZE
	uint64_t write_sum;
	uint64_t write_count;
#endif
} client[MAXCLIENTS];
int nclients = 0;
#define client_index(client_ptr) ((client_ptr) - &client[0])
static pthread_mutex_t client_mutex = PTHREAD_MUTEX_INITIALIZER;

struct bridge_data {
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
} bridgelist[MAXCLIENTS];
int nbridges = 0;
static pthread_mutex_t universe_mutex = PTHREAD_MUTEX_INITIALIZER;

static pthread_mutex_t listener_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t listener_started;
int listener_port = -1;
static int default_snis_server_port = -1; /* -1 means choose a random port */
pthread_t lobbythread;
char *lobbyserver = NULL;
static int snis_log_level = 2;
static struct ship_type_entry *ship_type;
int nshiptypes;

static int safe_mode = 0; /* prevents enemies from attacking if set */

#ifndef PREFIX
#define PREFIX .
#warn "PREFIX defaulted to ."
#endif

#define STRPREFIX(s) str(s)
#define str(s) #s

char *default_asset_dir = STRPREFIX(PREFIX) "/share/snis";
char *asset_dir;

static int nebulalist[NNEBULA] = { 0 };

static int ncommodities;
static int ncontraband;
static struct commodity *commodity;

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
static void put_client(struct game_client *c)
{
	assert(c->refcount > 0);
	c->refcount--;
	if (c->refcount == 0)
		remove_client(client_index(c));
}

/* assumes universe and client locks are held. */
static void get_client(struct game_client *c)
{
	c->refcount++;
}

static lua_State *lua_state = NULL;
/* should obtain universe_mutex before touching this queue. */
struct lua_command_queue_entry {
	struct lua_command_queue_entry *next;
	char lua_command[PATH_MAX];
} *lua_command_queue_head = NULL,  *lua_command_queue_tail = NULL;

static void enqueue_lua_command(char *cmd)
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

struct timer_event {
	char *callback;
	uint32_t firetime;
	double cookie_val;
	struct timer_event *next;
} *lua_timer = NULL;

struct event_callback_entry *event_callback = NULL;
struct callback_schedule_entry *callback_schedule = NULL; 

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
		lua_pcall(lua_state, 1, 0, 0);
		free_timer_event(&i);
	}

}


void lua_object_id_event(char *event, uint32_t object_id)
{
	int i, ncallbacks;
	char **callback;
	double tmp;

	ncallbacks = callback_list(event_callback, event, &callback);
	for (i = 0; i < ncallbacks; i++) {
		lua_getglobal(lua_state, callback[i]);
		tmp = (double) object_id;
		lua_pushnumber(lua_state, tmp);
		lua_pcall(lua_state, 1, 0, 0);
	}
}

void fire_lua_callbacks(struct callback_schedule_entry **sched)
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
		lua_pcall(lua_state, 3, 0, 0);
	}
	free_callback_schedule(sched);
}

void lua_player_respawn_event(uint32_t object_id)
{
	lua_object_id_event("player-respawn-event", object_id);
}

void lua_player_death_event(uint32_t object_id)
{
	lua_object_id_event("player-death-event", object_id);
}

int nframes = 0;
int timer = 0;
struct timeval start_time, end_time;

static struct snis_object_pool *pool;
static struct snis_entity go[MAXGAMEOBJS];
#define go_index(snis_entity_ptr) ((snis_entity_ptr) - &go[0])
static struct space_partition *space_partition = NULL;

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

static void get_peer_name(int connection, char *buffer)
{
	struct sockaddr_in *peer;
	struct sockaddr p;
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

static void log_client_info(int level, int connection, char *info)
{
	char client_ip[50];

	if (level < snis_log_level)
		return;

	get_peer_name(connection, client_ip);
	snis_log(level, "snis_server: %s: %s",
			client_ip, info);
}

static void generic_move(__attribute__((unused)) struct snis_entity *o)
{
	return;
}

static void asteroid_move(struct snis_entity *o)
{
	set_object_location(o, o->x + o->vx, o->y + o->vy, o->z + o->vz);
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
	pb_queue_to_client(c, packed_buffer_new("bw", OPCODE_DELETE_OBJECT, oid));
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
	int i;
	uint32_t oid = o->id;

	client_lock();
	for (i = 0; i < nclients; i++)
		if (client[i].refcount)
			queue_delete_oid(&client[i], oid);
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

static void delete_from_clients_and_server(struct snis_entity *o);
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

static void delete_from_clients_and_server(struct snis_entity *o)
{
	snis_queue_delete_object(o);
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
		break;
	default:
		schedule_callback(event_callback, &callback_schedule,
				"object-death-callback", o->id);
		break;
	}

	if (o->type == OBJTYPE_SHIP2)
		fleet_leave(o->id); /* leave any fleets ship might be a member of */
	remove_from_attack_lists(o->id);
	delete_object(o);
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
	pb_queue_to_client(c, packed_buffer_new("bh", OPCODE_PLAY_SOUND, sound_number));
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

static void snis_queue_add_global_sound(uint16_t sound_number)
{
	int i;

	for (i = 0; i < nbridges; i++)
		snis_queue_add_sound(sound_number, ROLE_ALL, bridgelist[i].shipid);
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
static int lookup_by_id(uint32_t id);

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
				int item, float qty);
static int make_derelict(struct snis_entity *o)
{
	int rc;
	rc = add_derelict(o->sdata.name, o->x, o->y, o->z,
				o->vx + snis_random_float() * 2.0,
				o->vy + snis_random_float() * 2.0,
				o->vz + snis_random_float() * 2.0,
				o->tsd.ship.shiptype, o->sdata.faction, 0);
	(void) add_cargo_container(o->x, o->y, o->z,
				o->vx + snis_random_float() * 2.0,
				o->vy + snis_random_float() * 2.0,
				o->vz + snis_random_float(), -1, 0.0);
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

union vec3 pick_random_patrol_destination(struct snis_entity *ship)
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
			t->type != OBJTYPE_PLANET)
		return;
	if (t->id == o->tsd.torpedo.ship_id)
		return; /* can't torpedo yourself. */
	dist2 = dist3dsqrd(t->x - o->x, t->y - o->y, t->z - o->z);

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
		calculate_torpedo_damage(t);
		send_ship_damage_packet(t);
		send_detonate_packet(t, ix, iy, iz, impact_time, impact_fractional_time);
		attack_your_attacker(t, lookup_entity_by_id(o->tsd.torpedo.ship_id));
	} else if (t->type == OBJTYPE_ASTEROID || t->type == OBJTYPE_CARGO_CONTAINER) {
		if (t->alive)
			t->alive--;
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
		t->type != OBJTYPE_TORPEDO && t->type != OBJTYPE_CARGO_CONTAINER)
		return;
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

/* check if a planet is in the way of a shot */
static int planet_in_the_way(struct snis_entity *origin,
				struct snis_entity *target)
{
	int i;
	union vec3 ray_origin, ray_direction, sphere_origin;
	const float radius = 800.0; /* FIXME: nuke this hardcoded crap */
	float target_dist;
	float planet_dist;

	ray_origin.v.x = origin->x;
	ray_origin.v.y = origin->y;
	ray_origin.v.z = origin->z;

	ray_direction.v.x = target->x - ray_origin.v.x;
	ray_direction.v.y = target->y - ray_origin.v.y;
	ray_direction.v.z = target->z - ray_origin.v.z;

	target_dist = vec3_magnitude(&ray_direction);
	vec3_normalize_self(&ray_direction);

	for (i = 0; i <= snis_object_pool_highest_object(pool); i++) {
		if (go[i].type != OBJTYPE_PLANET)
			continue;
		sphere_origin.v.x = go[i].x;
		sphere_origin.v.y = go[i].y;
		sphere_origin.v.z = go[i].z;
		if (!ray_intersects_sphere(&ray_origin, &ray_direction,
						&sphere_origin, radius))
			continue;
		planet_dist = dist3d(sphere_origin.v.x - ray_origin.v.x,
					sphere_origin.v.y - ray_origin.v.y,
					sphere_origin.v.z - ray_origin.v.z);
		if (planet_dist < target_dist) /* planet blocks... */
			return 1;
	}
	return 0; /* no planets blocking */
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
			o->tsd.ship.torpedoes > 0) {
			double dist, flight_time, tx, ty, tz, vx, vy, vz;
			/* int inside_nebula = in_nebula(o->x, o->y) || in_nebula(v->x, v->y); */

			if (v->type == OBJTYPE_PLANET || !planet_in_the_way(o, v)) {
				dist = hypot3d(v->x - o->x, v->y - o->y, v->z - o->z);
				flight_time = dist / TORPEDO_VELOCITY;
				tx = v->x + (v->vx * flight_time);
				tz = v->z + (v->vz * flight_time);
				ty = v->y + (v->vy * flight_time);

				calculate_torpedo_velocities(o->x, o->y, o->z,
					tx, ty, tz, TORPEDO_VELOCITY, &vx, &vy, &vz);

				add_torpedo(o->x, o->y, o->z, vx, vy, vz, o->id);
				o->tsd.ship.torpedoes--;
				o->tsd.ship.next_torpedo_time = universe_timestamp +
					ENEMY_TORPEDO_FIRE_INTERVAL;
				check_for_incoming_fire(v);
			}
		} else {
			if (snis_randn(1000) < 300 + imacop * 200 &&
				o->tsd.ship.next_laser_time <= universe_timestamp) {
				if (v->type == OBJTYPE_PLANET || !planet_in_the_way(o, v)) {
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

	pb = packed_buffer_new("bwSSSSSS", OPCODE_ADD_WARP_EFFECT,
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

	v.v.x = destx - o->x;
	v.v.y = desty - o->y;
	v.v.z = destz - o->z;
	vec3_normalize(&vn, &v);
	vec3_mul(&v, &vn, fractional_distance);
	v.v.x += (float) snis_randn((int) (fractional_distance / 15.0f));
	v.v.y += (float) snis_randn((int) (fractional_distance / 15.0f));
	v.v.z += (float) snis_randn((int) (fractional_distance / 15.0f));

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

	double dist2 = dist3dsqrd(o->x - destx, o->y - desty, o->z - destz);
	if (dist2 > 2000.0 * 2000.0) {
		double ld = dist3dsqrd(o->x - o->tsd.ship.dox,
				o->y - o->tsd.ship.doy, o->z - o->tsd.ship.doz);
		/* give ships some variety in movement */
		if (((universe_timestamp + o->id) & 0x3ff) == 0 || ld < 50.0 * 50.0)
			ai_add_ship_movement_variety(o, destx, desty, destz, 1500.0f);
		/* sometimes just warp if it's too far... */
		if (snis_randn(10000) < ship_type[o->tsd.ship.shiptype].warpchance)
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

#define GOLD 0
#define PLATINUM 1
#define GERMANIUM 2
#define URANIUM 3

	mining_bot_unload_one_ore(bot, parent, ai, &ai->gold, GOLD);
	mining_bot_unload_one_ore(bot, parent, ai, &ai->platinum, PLATINUM);
	mining_bot_unload_one_ore(bot, parent, ai, &ai->germanium, GERMANIUM);
	mining_bot_unload_one_ore(bot, parent, ai, &ai->uranium, URANIUM);
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
				" -- STANDING BY FOR TRANSPORT OF ORES --");
		ai->mode = MINING_MODE_STANDBY_TO_TRANSPORT_ORE;
		snis_queue_add_sound(MINING_BOT_STANDING_BY,
				ROLE_COMMS | ROLE_SOUNDSERVER | ROLE_SCIENCE, parent->id);

	}
	if (dist2 < 300.0 * 300.0 && ai->mode == MINING_MODE_STOW_BOT) {
		b = lookup_bridge_by_shipid(ai->parent_ship);
		if (b >= 0) {
			channel = bridgelist[b].npcbot.channel;
			send_comms_packet(o->sdata.name, channel, "--- TRANSPORTING ORES ---");
			send_comms_packet(o->sdata.name, channel, "--- ORES TRANSPORTED ---");
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
			send_comms_packet(o->sdata.name, channel,
				"COMMENCING MINING OPERATION");
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
				"TARGET ASTEROID LOST -- RETURNING TO SHIP");
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

	if (ai->countdown != 0)
		return;
	int b = lookup_bridge_by_shipid(ai->parent_ship);
	if (b >= 0) {
		int channel = bridgelist[b].npcbot.channel;
		send_comms_packet(o->sdata.name, channel,
			"MINING OPERATION COMPLETE -- RETURNING TO SHIP");
	}
	ai->mode = MINING_MODE_RETURN_TO_PARENT;
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

	/* hmm, server has no idea about meshes... */
	d = dist3dsqrd(o->x - obstacle->x, o->y - obstacle->y, o->z - obstacle->z);

	if (obstacle->type == OBJTYPE_PLANET) {
		if (d < obstacle->tsd.planet.radius * obstacle->tsd.planet.radius) {
			o->alive = 0;
			return;
		}
		d -= (obstacle->tsd.planet.radius * 1.2 * obstacle->tsd.planet.radius * 1.2);
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

static void damcon_robot_move(struct snis_damcon_entity *o, struct damcon_data *d)
{
	double vx, vy, lastx, lasty, lasth, dv;
	int bounds_hit = 0;
	struct snis_damcon_entity *cargo = NULL;
	double clawx, clawy;

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
			if (cargo->tsd.part.damage < 200) {
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

	BUILD_ASSERT(sizeof(o->tsd.ship.damage) == 8);
	if (system_number < 0 || system_number >= 8)
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
void do_docking_action(struct snis_entity *ship, struct snis_entity *starbase,
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
		return;
	default:
		break;
	}
	if (t == o) /* skip self */
		return;
	dist2 = dist3dsqrd(o->x - t->x, o->y - t->y, o->z - t->z);
	if (t->type == OBJTYPE_CARGO_CONTAINER && dist2 < 150.0 * 150.0) {
			scoop_up_cargo(o, t);
			return;
	}
	if (t->type == OBJTYPE_DOCKING_PORT && dist2 < 50.0 * 50.0 &&
		o->tsd.ship.docking_magnets) {
		player_attempt_dock_with_starbase(t, o);
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
				packed_buffer_new("b", OPCODE_ATMOSPHERIC_FRICTION),
					ROLE_SOUNDSERVER | ROLE_NAVIGATION);
		} else if (dist2 < warn_dist2 && (universe_timestamp & 0x7) == 0) {
			(void) add_explosion(o->x + o->vx * 2, o->y + o->vy * 2, o->z + o->vz * 2,
				5, 5, 50, OBJTYPE_SPARK);
			calculate_atmosphere_damage(o);
			send_ship_damage_packet(o);
			send_packet_to_all_clients_on_a_bridge(o->id,
				packed_buffer_new("b", OPCODE_ATMOSPHERIC_FRICTION),
					ROLE_SOUNDSERVER | ROLE_NAVIGATION);
		}
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
			packed_buffer_new("b", OPCODE_PROXIMITY_ALERT),
					ROLE_SOUNDSERVER | ROLE_NAVIGATION);
		if (dist2 < crash_dist2) {
			send_packet_to_all_clients_on_a_bridge(o->id, 
				packed_buffer_new("b", OPCODE_COLLISION_NOTIFICATION),
					ROLE_SOUNDSERVER | ROLE_NAVIGATION);
		}
	}
}

static void update_player_orientation(struct snis_entity *o)
{
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
	if (o->tsd.ship.in_secure_area == 0 && previous_security > 0)
		snis_queue_add_sound(LEAVING_SECURE_AREA, ROLE_SOUNDSERVER, o->id);
	if (o->tsd.ship.in_secure_area > 0 && previous_security == 0)
		snis_queue_add_sound(ENTERING_SECURE_AREA, ROLE_SOUNDSERVER, o->id);
}

static int calc_sunburn_damage(struct snis_entity *o, struct damcon_data *d,
				uint8_t *system, uint8_t sunburn)
{
	float damage, old_value, new_value;
	int system_number = system - (unsigned char *) &o->tsd.ship.damage;

	BUILD_ASSERT(sizeof(o->tsd.ship.damage) == 8);
	if (system_number < 0 || system_number >= 8)
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
		
static void player_move(struct snis_entity *o)
{
	int desired_rpm, desired_temp, diff;
	int phaser_chargerate, current_phaserbank;
	float orientation_damping, velocity_damping;

	if (o->tsd.ship.damage.shield_damage > 200 && (universe_timestamp % (10 * 5)) == 0)
		snis_queue_add_sound(HULL_BREACH_IMMINENT, ROLE_SOUNDSERVER, o->id);
	if (o->tsd.ship.fuel < FUEL_CONSUMPTION_UNIT * 30 * 60 &&
		(universe_timestamp % (20 * 5)) == 15) {
		snis_queue_add_sound(FUEL_LEVELS_CRITICAL, ROLE_SOUNDSERVER, o->id);

		/* auto-refuel in safe mode */
		if (safe_mode)
			o->tsd.ship.fuel = UINT32_MAX;
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

	/* Warp the ship if it's time to engage warp. */
	if (o->tsd.ship.warp_time >= 0) {
		o->tsd.ship.warp_time--;
		if (o->tsd.ship.warp_time == 0) {
			int b;

			b = lookup_bridge_by_shipid(o->id);
			if (b >= 0) {
				/* 5 seconds of warp limbo */
				send_packet_to_all_clients_on_a_bridge(o->id,
					packed_buffer_new("bh", OPCODE_WARP_LIMBO,
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
		}
	}
	int b = lookup_bridge_by_shipid(o->id);
	if (bridgelist[b].warptimeleft > 0) {
		float angle = (2.0 * M_PI / 50.0) * (float) bridgelist[b].warptimeleft;

		/* 2.0 below determined empirically.  I am lazy. :) */
		o->vx = bridgelist[b].warpv.v.x * 2.0 * (1.0 - cos(angle)) * 0.5;
		o->vy = bridgelist[b].warpv.v.y * 2.0 * (1.0 - cos(angle)) * 0.5;
		o->vz = bridgelist[b].warpv.v.z * 2.0 * (1.0 - cos(angle)) * 0.5;
		o->timestamp = universe_timestamp;
		bridgelist[b].warptimeleft--;
		if (bridgelist[b].warptimeleft == 0) {
			o->vx = 0;
			o->vy = 0;
			o->vz = 0;
		}
	}
	calculate_ship_scibeam_info(o);
}

static void demon_ship_move(struct snis_entity *o)
{
	o->vx = o->tsd.ship.velocity * cos(o->heading);
	o->vz = o->tsd.ship.velocity * -sin(o->heading);
	set_object_location(o, o->x + o->vx, o->y + o->vy, o->z + o->vz);
	o->heading += o->tsd.ship.yaw_velocity;
	normalize_angle(&o->heading);
	quat_init_axis(&o->orientation, 0, 1, 0, o->heading);
	o->timestamp = universe_timestamp;

	damp_yaw_velocity(&o->tsd.ship.yaw_velocity, YAW_DAMPING);
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
		if (planet_in_the_way(o, a))
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
	o->tsd.ship.nav_damping_suppression = 0.0;
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

static void respawn_player(struct snis_entity *o)
{
	int b, i, found;
	double x, y, z, a1, a2, rf;
	char mining_bot_name[20];
	static struct mtwist_state *mt = NULL;
	if (!mt)
		mt = mtwist_init(mtwist_seed);

	/* Find a friendly location to respawn... */
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
	/* Stop any warp that might be in progress */
	b = lookup_bridge_by_shipid(o->id);
	if (b >= 0)
		bridgelist[b].warptimeleft = 0;
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
	o->alive = 1;
}

static int add_player(double x, double z, double vx, double vz, double heading)
{
	int i;

	i = add_generic_object(x, 0.0, z, vx, 0.0, vz, heading, OBJTYPE_SHIP1);
	if (i < 0)
		return i;
	respawn_player(&go[i]);
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

static int add_ship(int faction, int auto_respawn)
{
	int i;
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
	memset(&go[i].tsd.ship.damage, 0, sizeof(go[i].tsd.ship.damage));
	memset(&go[i].tsd.ship.power_data, 0, sizeof(go[i].tsd.ship.power_data));
	go[i].tsd.ship.braking_factor = 1.0f;
	go[i].tsd.ship.steering_adjustment.v.x = 0.0;
	go[i].tsd.ship.steering_adjustment.v.y = 0.0;
	go[i].tsd.ship.steering_adjustment.v.z = 0.0;
	go[i].tsd.ship.ncargo_bays = 0;
	go[i].tsd.ship.home_planet = choose_ship_home_planet();
	go[i].tsd.ship.auto_respawn = (uint8_t) auto_respawn;
	memset(go[i].tsd.ship.cargo, 0, sizeof(go[i].tsd.ship.cargo));
	if (faction >= 0 && faction < nfactions())
		go[i].sdata.faction = faction;
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

static int add_asteroid(double x, double y, double z, double vx, double vz, double heading);
static int l_add_asteroid(lua_State *l)
{
	double x, y, z;
	int i;

	x = lua_tonumber(lua_state, 1);
	y = lua_tonumber(lua_state, 2);
	z = lua_tonumber(lua_state, 3);

	pthread_mutex_lock(&universe_mutex);
	i = add_asteroid(x, y, z, 0.0, 0.0, 0.0);
	lua_pushnumber(lua_state, i < 0 ? -1.0 : (double) go[i].id);
	pthread_mutex_unlock(&universe_mutex);
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
	i = add_cargo_container(x, y, z, vx, vy, vz, -1, 0);
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
			int item, float qty)
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
		item = snis_randn(ncommodities);
		qty = (float) snis_randn(100);
	}
	go[i].tsd.cargo_container.contents.item = item;
	go[i].tsd.cargo_container.contents.qty = qty;
	return i;
}

static void normalize_percentage(uint8_t *v, int total)
{
	float fraction = (float) *v / (float) total;
	*v = (uint8_t) (fraction * 255.0);
}

static int add_asteroid(double x, double y, double z, double vx, double vz, double heading)
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
			item = snis_randn(ncommodities);
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

	for (i = 0; i <= snis_object_pool_highest_object(pool); i++)
		if (go[i].id == id)
			return i;
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
	go[i].tsd.laser.power = go[s].tsd.ship.phaser_charge;
	go[i].tsd.laser.wavelength = go[s].tsd.ship.phaser_wavelength;
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
			add_asteroid(x, y, z, 0.0, 0.0, 0.0);
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

static int add_planet(double x, double y, double z, float radius, uint8_t security)
{
	int i;

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
	go[i].tsd.planet.security = security;
	go[i].tsd.planet.contraband = choose_contraband();

	/* If we wish each planet to have a different color and size atmosphere
	 * this is where that would happen.
	 */
	go[i].tsd.planet.atmosphere_r = (uint8_t) (255.0 * 0.6);
	go[i].tsd.planet.atmosphere_g = (uint8_t) (255.0 * 0.6);
	go[i].tsd.planet.atmosphere_b = (uint8_t) (255.0 * 1.0);
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
	} while (passenger[i].destination == passenger[i].location);
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
	pthread_mutex_lock(&universe_mutex);
	snis_object_pool_setup(&pool, MAXGAMEOBJS);

	add_nebulae(); /* do nebula first */
	add_asteroids();
	add_planets();
	add_starbases();
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
}

/* offsets for sockets... */
static int dcxo[] = { 20, 160, 205, 160, 20 };
static int dcyo[] = { -65, -65, 0, 65, 65 };

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
	dy = DAMCONYDIM / 6;
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
	d->o[i].version++;

	x = -DAMCONXDIM / 6;
	y += dy;
	i = add_generic_damcon_object(d, x, y, DAMCON_TYPE_REPAIR_STATION, NULL);
	d->o[i].version++;
	add_damcon_sockets(d, x, y, DAMCON_TYPE_REPAIR_STATION, 0);
}

static void add_damcon_parts(struct damcon_data *d)
{
}

static void populate_damcon_arena(struct damcon_data *d)
{
	snis_object_pool_setup(&d->pool, MAXDAMCONENTITIES);
	add_damcon_robot(d);
	add_damcon_systems(d);
	add_damcon_labels(d);
	add_damcon_parts(d);
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

static void do_generic_yaw(double *yawvel, int yaw, double max_yaw, double yaw_inc,
				double yaw_inc_fine)
{
	double delta_yaw = abs(yaw) > 1 ? yaw_inc_fine : yaw_inc;
	if (yaw > 0) {
		if (*yawvel < max_yaw)
			*yawvel += delta_yaw;
	} else {
		if (*yawvel > -max_yaw)
			*yawvel -= delta_yaw;
	}
}

static void do_yaw(struct game_client *c, int yaw)
{
	struct snis_entity *ship = &go[c->ship_index];
	double max_yaw_velocity =
		(MAX_YAW_VELOCITY * ship->tsd.ship.power_data.maneuvering.i) / 255;

	do_generic_yaw(&ship->tsd.ship.yaw_velocity, yaw, max_yaw_velocity,
			YAW_INCREMENT, YAW_INCREMENT_FINE);
}

static void do_pitch(struct game_client *c, int pitch)
{
	struct snis_entity *ship = &go[c->ship_index];
	double max_pitch_velocity =
		(MAX_PITCH_VELOCITY * ship->tsd.ship.power_data.maneuvering.i) / 255;

	do_generic_yaw(&ship->tsd.ship.pitch_velocity, pitch, max_pitch_velocity,
			PITCH_INCREMENT, PITCH_INCREMENT_FINE);
}

static void do_roll(struct game_client *c, int roll)
{
	struct snis_entity *ship = &go[c->ship_index];
	double max_roll_velocity =
		(MAX_ROLL_VELOCITY * ship->tsd.ship.power_data.maneuvering.i) / 255;

	do_generic_yaw(&ship->tsd.ship.roll_velocity, roll, max_roll_velocity,
			ROLL_INCREMENT, ROLL_INCREMENT_FINE);
}

static void do_sciball_yaw(struct game_client *c, int yaw)
{
	struct snis_entity *ship = &go[c->ship_index];
	const double max_yaw_velocity = MAX_YAW_VELOCITY;

	do_generic_yaw(&ship->tsd.ship.sciball_yawvel, yaw, max_yaw_velocity,
			YAW_INCREMENT, YAW_INCREMENT_FINE);
}

static void do_sciball_pitch(struct game_client *c, int pitch)
{
	struct snis_entity *ship = &go[c->ship_index];
	double max_pitch_velocity = MAX_PITCH_VELOCITY;

	do_generic_yaw(&ship->tsd.ship.sciball_pitchvel, pitch, max_pitch_velocity,
			PITCH_INCREMENT, PITCH_INCREMENT_FINE);
}

static void do_sciball_roll(struct game_client *c, int roll)
{
	struct snis_entity *ship = &go[c->ship_index];
	double max_roll_velocity = MAX_ROLL_VELOCITY;

	do_generic_yaw(&ship->tsd.ship.sciball_rollvel, roll, max_roll_velocity,
			ROLL_INCREMENT, ROLL_INCREMENT_FINE);
}

static void do_manual_gunyaw(struct game_client *c, int yaw)
{
	struct snis_entity *ship = &go[c->ship_index];
	double max_yaw_velocity = MAX_YAW_VELOCITY;

	do_generic_yaw(&ship->tsd.ship.weap_yawvel, yaw, max_yaw_velocity,
			YAW_INCREMENT, YAW_INCREMENT_FINE);
}

static void do_manual_gunpitch(struct game_client *c, int pitch)
{
	struct snis_entity *ship = &go[c->ship_index];
	double max_pitch_velocity = MAX_PITCH_VELOCITY;

	do_generic_yaw(&ship->tsd.ship.weap_pitchvel, pitch, max_pitch_velocity,
			PITCH_INCREMENT, PITCH_INCREMENT_FINE);
}

static void do_demon_yaw(struct snis_entity *o, int yaw)
{
	double max_yaw_velocity = MAX_YAW_VELOCITY;

	do_generic_yaw(&o->tsd.ship.yaw_velocity, yaw, max_yaw_velocity,
			YAW_INCREMENT, YAW_INCREMENT_FINE);
}

static void do_gun_yaw(struct game_client *c, int yaw)
{
	/* FIXME combine this with do_yaw somehow */
	struct snis_entity *ship = &go[c->ship_index];

	do_generic_yaw(&ship->tsd.ship.gun_yaw_velocity, yaw,
				MAX_GUN_YAW_VELOCITY, GUN_YAW_INCREMENT,
				GUN_YAW_INCREMENT_FINE);
}

static void do_sci_yaw(struct game_client *c, int yaw)
{
	struct snis_entity *ship = &go[c->ship_index];

	do_generic_yaw(&ship->tsd.ship.sci_yaw_velocity, yaw,
				MAX_SCI_YAW_VELOCITY, SCI_YAW_INCREMENT,
				SCI_YAW_INCREMENT_FINE);
}

static void do_robot_yaw(struct game_client *c, int yaw)
{
	struct damcon_data *d = &bridgelist[c->bridge].damcon;
	struct snis_damcon_entity *r = d->robot;

	do_generic_yaw(&r->tsd.robot.yaw_velocity, yaw,
			2.0 * MAX_SCI_YAW_VELOCITY, SCI_YAW_INCREMENT,
				SCI_YAW_INCREMENT_FINE);
}

static void do_sci_bw_yaw(struct game_client *c, int yaw)
{
	struct snis_entity *ship = &go[c->ship_index];

	do_generic_yaw(&ship->tsd.ship.sci_beam_width, yaw,
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
	double dx, dz;
	int i, rc;

	rc = read_and_unpack_buffer(c, buffer, "wSS", &oid,
			&dx, (int32_t) UNIVERSE_DIM,
			&dz, (int32_t) UNIVERSE_DIM);
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
	set_object_location(o, o->x + dx, o->y, o->z + dz);
	o->timestamp = universe_timestamp;
out:
	pthread_mutex_unlock(&universe_mutex);
	return 0;
}

static int process_request_robot_thrust(struct game_client *c)
{
	return process_generic_request_thrust(c, do_robot_thrust);
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
			packed_buffer_new("bb", OPCODE_ROLE_ONSCREEN, new_displaymode),
			ROLE_MAIN);
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
	if (new_details > 3)
		new_details = 0;
	send_packet_to_requestor_plus_role_on_a_bridge(c, 
			packed_buffer_new("bb", OPCODE_SCI_DETAILS,
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

static int process_sci_select_target(struct game_client *c)
{
	unsigned char buffer[10];
	uint32_t id;
	int rc;

	rc = read_and_unpack_buffer(c, buffer, "w", &id);
	if (rc)
		return rc;
	/* just turn it around and fan it out to all the right places */
	send_packet_to_all_clients_on_a_bridge(c->shipid, 
			packed_buffer_new("bw", OPCODE_SCI_SELECT_TARGET, id),
			ROLE_SCIENCE);
	/* remember sci selection for retargeting mining bot */
	bridgelist[c->bridge].science_selection = id;
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

static int process_sci_select_coords(struct game_client *c)
{
	unsigned char buffer[sizeof(struct snis_sci_select_coords_packet)];
	uint32_t x, z;
	int rc;

	rc = read_and_unpack_buffer(c, buffer, "ww", &x, &z);
	if (rc)
		return rc;
	/* just turn it around and fan it out to all the right places */
	send_packet_to_all_clients_on_a_bridge(c->shipid,
				packed_buffer_new("bww", OPCODE_SCI_SELECT_COORDS, x, z),
				ROLE_SCIENCE);
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
				"    CARGO BAY %d: %4.0f %s %s - PAID $%.2f ORIG %10s DEST %10s DUE %s",
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

	rc = sscanf(txt, "/channel %u\n", &newchannel);
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

	snprintf(msg, sizeof(msg), "TX/RX INITIATED ON CHANNEL %u", newchannel);
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
				item, qty);
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

void npc_menu_item_not_implemented(struct npc_menu_item *item,
					char *npcname, struct npc_bot_state *botstate)
{
	send_comms_packet(npcname, botstate->channel,
				"  SORRY, THAT IS NOT IMPLEMENTED");
}

void npc_menu_item_sign_off(struct npc_menu_item *item,
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
void npc_menu_item_buysell_cargo(struct npc_menu_item *item,
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
		send_comms_packet(npcname, channel, "--- TRANSPORTING ORES ---");
		mining_bot_unload_ores(miner, parent, ai);
		send_comms_packet(npcname, channel, "--- ORES TRANSPORTED ---");
		send_comms_packet(npcname, channel,
			"COMMENCING RENDEZVOUS WITH TARGET ASTEROID");
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
	if (asteroid->type != OBJTYPE_ASTEROID) {
		send_comms_packet(npcname, channel, " SELECTED DESTINATION UNMINABLE");
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
	float gold, platinum, germanium, uranium, total;
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

	total = 0.0;
	gold = 0.0;
	platinum = 0.0;
	germanium = 0.0;
	uranium = 0.0;

	gold = ai->gold;
	platinum = ai->platinum;
	germanium = ai->germanium;
	uranium = ai->uranium;
	total = gold + platinum + germanium + uranium;
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
		sprintf(msg, "MINING ON %s\n",
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
		sprintf(msg, "STANDING BY TO TRANSPORT ORES\n");
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
	send_comms_packet(npcname, channel, "FUEL: UNKNOWN");
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

void npc_menu_item_buy_cargo(struct npc_menu_item *item,
					char *npcname, struct npc_bot_state *botstate)
{
	npc_menu_item_buysell_cargo(item, npcname, botstate, 1);
}

void npc_menu_item_sell_cargo(struct npc_menu_item *item,
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

void npc_menu_item_eject_passengers(struct npc_menu_item *item,
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

void npc_menu_item_disembark_passengers(struct npc_menu_item *item,
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

void npc_menu_item_board_passengers(struct npc_menu_item *item,
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

void npc_menu_item_travel_advisory(struct npc_menu_item *item,
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
	if (contraband >= 0) {
		send_comms_packet(npcname, ch, " TRAVELERS TAKE NOTICE OF PROHIBITED ITEMS:");
		snprintf(msg, sizeof(msg), "    %s", commodity[contraband].name);
		send_comms_packet(npcname, ch, msg);
	}
	send_comms_packet(npcname, ch, "");
	send_comms_packet(npcname, ch, " ENJOY YOUR VISIT!");
	send_comms_packet(npcname, ch, "-----------------------------------------------------");
}

void npc_menu_item_request_dock(struct npc_menu_item *item,
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
			int q;
			char x;

			/* check transporter range */
			if (range2 > TRANSPORTER_RANGE * TRANSPORTER_RANGE) {
				send_comms_packet(n, channel,
					" TRANSACTION NOT POSSIBLE - TRANSPORTER RANGE EXCEEDED");
				return;
			}

			rc = sscanf(msg + 5, "%d %c", &q, &x);
			if (rc != 2) {
				send_comms_packet(n, channel, " INVALID SELL ORDER");
				return;
			}
			x = toupper(x) - 'A';
			if (x < 0 || x >= ship->tsd.ship.ncargo_bays) {
				send_comms_packet(n, channel, " INVALID SELL ORDER");
				return;
			}
			if (q > ship->tsd.ship.cargo[(int) x].contents.qty) {
				send_comms_packet(n, channel, " INVALID SELL ORDER");
				return;
			}
			ship->tsd.ship.cargo[(int) x].contents.qty -= q;
			ship->tsd.ship.wallet +=
				q * o->tsd.starbase.bid_price[ship->tsd.ship.cargo[(int) x].contents.item];
			profitloss = q * o->tsd.starbase.bid_price[ship->tsd.ship.cargo[(int) x].contents.item]
					- q * ship->tsd.ship.cargo[(int) x].paid;
			if (ship->tsd.ship.cargo[(int) x].contents.qty < 0.1) {
				ship->tsd.ship.cargo[(int) x].contents.item = -1;
				ship->tsd.ship.cargo[(int) x].contents.qty = 0.0f;
				ship->tsd.ship.cargo[(int) x].paid = 0.0f;
				ship->tsd.ship.cargo[(int) x].origin = -1;
				ship->tsd.ship.cargo[(int) x].dest = -1;
				ship->tsd.ship.cargo[(int) x].due_date = -1;
			}
			snprintf(m, sizeof(m), " EXECUTING SELL ORDER %d %c", q, x);
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
				snprintf(m, sizeof(m), " %c: %04.0f %s %s -- $%4.2f  $%4.2f\n",
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
				snprintf(m, sizeof(m), " %c: %04.0f %s %s -- $%4.2f (PAID $%.2f)",
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
		bridgelist[bridge].npcbot.current_menu = starbase_main_menu;
		generic_npc_bot(o, bridge, name, msg);
		break;
	case OBJTYPE_SHIP2:
		if (o->tsd.ship.ai[0].ai_mode != AI_MODE_MINING_BOT)
			break;
		bridgelist[bridge].npcbot.current_menu = mining_bot_main_menu;
		generic_npc_bot(o, bridge, name, msg);
		break;
	default:
		break;
	}
	return;
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
		snprintf(msg, sizeof(msg), "TX/RX INITIATED ON CHANNEL %u", channel[0]);
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
};

static void process_meta_comms_packet(char *name, struct game_client *c, char *txt)
{
	int i;

	for (i = 0; i < ARRAYSIZE(meta_comms); i++) {
		int len = strlen(meta_comms[i].command);
		if (strncasecmp(txt, meta_comms[i].command, len) == 0)  {
			meta_comms[i].f(name, c, txt);
			return;
		}
	}
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
			packed_buffer_new("bRb", OPCODE_MAINSCREEN_VIEW_MODE,
					view_angle, view_mode),
			ROLE_MAIN);
	return 0;
}

static int process_request_redalert(struct game_client *c)
{
	int rc;
	unsigned char buffer[10];
	unsigned char new_alert_mode;

	rc = read_and_unpack_buffer(c, buffer, "b", &new_alert_mode);
	if (rc)
		return rc;
	send_packet_to_all_clients_on_a_bridge(c->shipid,
			packed_buffer_new("bb", OPCODE_REQUEST_REDALERT, new_alert_mode), ROLE_ALL);
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
			packed_buffer_new("bb", OPCODE_COMMS_MAINSCREEN,
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
	return 0;
}

static int l_clear_all(__attribute__((unused)) lua_State *l)
{
	process_demon_clear_all();
	return 0;
}

static int process_create_item(struct game_client *c)
{
	unsigned char buffer[10];
	unsigned char item_type;
	double x, z, r;
	int rc, i = -1;

	rc = read_and_unpack_buffer(c, buffer, "bSS", &item_type,
			&x, (int32_t) UNIVERSE_DIM, &z, (int32_t) UNIVERSE_DIM);
	if (rc)
		return rc;

	pthread_mutex_lock(&universe_mutex);
	switch (item_type) {
	case OBJTYPE_SHIP2:
		i = add_ship(-1, 1);
		break;
	case OBJTYPE_STARBASE:
		i = add_starbase(x, 0, z, 0, 0, 0, snis_randn(100), -1);
		break;
	case OBJTYPE_PLANET:
		r = (float) snis_randn(MAX_PLANET_RADIUS - MIN_PLANET_RADIUS) +
					MIN_PLANET_RADIUS;
		i = add_planet(x, 0.0, z, r, 0);
		break;
	case OBJTYPE_ASTEROID:
		i = add_asteroid(x, 0.0, z, 0.0, 0.0, 0.0);
		break;
	case OBJTYPE_NEBULA:
		r = (double) snis_randn(NEBULA_RADIUS) +
				(double) MIN_NEBULA_RADIUS;
		i = add_nebula(x, 0, z, 0, 0, 0, r);
		break;
	case OBJTYPE_SPACEMONSTER:
		i = add_spacemonster(x, 0.0, z);
		break;
	default:
		break;
	}
	if (i >= 0)
		set_object_location(&go[i], x, go[i].y, z);
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
			packed_buffer_new("bb", opcode, new_mode), roles);
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

static int process_request_scizoom(struct game_client *c)
{
	return process_request_bytevalue_pwr(c, offsetof(struct snis_entity, tsd.ship.scizoom), no_limit); 
}

static int process_request_navzoom(struct game_client *c)
{
	return process_request_bytevalue_pwr(c, offsetof(struct snis_entity, tsd.ship.navzoom), no_limit);
}

static int process_request_mainzoom(struct game_client *c)
{
	return process_request_bytevalue_pwr(c, offsetof(struct snis_entity, tsd.ship.mainzoom), no_limit);
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

static int process_request_throttle(struct game_client *c)
{
	return process_request_bytevalue_pwr(c, offsetof(struct snis_entity,
			tsd.ship.power_data.impulse.r1), no_limit); 
}

static int process_request_warpdrive(struct game_client *c)
{
	return process_request_bytevalue_pwr(c, offsetof(struct snis_entity,
			tsd.ship.power_data.warp.r1), no_limit); 
}

static int process_request_shield(struct game_client *c)
{
	return process_request_bytevalue_pwr(c, offsetof(struct snis_entity,
			tsd.ship.power_data.shields.r1), no_limit); 
}

static int process_request_maneuvering_pwr(struct game_client *c)
{
	return process_request_bytevalue_pwr(c, offsetof(struct snis_entity, tsd.ship.power_data.maneuvering.r2), no_limit); 
}

static int process_request_maneuvering_coolant(struct game_client *c)
{
	return process_request_bytevalue_pwr(c, offsetof(struct snis_entity, tsd.ship.coolant_data.maneuvering.r2), no_limit); 
}

static int process_request_tractor_pwr(struct game_client *c)
{
	return process_request_bytevalue_pwr(c, offsetof(struct snis_entity, tsd.ship.power_data.tractor.r2), no_limit); 
}

static int process_request_tractor_coolant(struct game_client *c)
{
	return process_request_bytevalue_pwr(c, offsetof(struct snis_entity, tsd.ship.coolant_data.tractor.r2), no_limit); 
}

static int process_request_laser_wavelength(struct game_client *c)
{
	return process_request_bytevalue_pwr(c, offsetof(struct snis_entity, tsd.ship.phaser_wavelength), no_limit); 
}

static void send_initiate_warp_packet(struct game_client *c, int enough_oomph)
{
	send_packet_to_all_clients_on_a_bridge(c->shipid,
			packed_buffer_new("bb", OPCODE_INITIATE_WARP,
					(unsigned char) enough_oomph),
			ROLE_ALL);
}

static void send_wormhole_limbo_packet(int shipid, uint16_t value)
{
	send_packet_to_all_clients_on_a_bridge(shipid,
			packed_buffer_new("bh", OPCODE_WORMHOLE_LIMBO, value),
			ROLE_ALL);
}

static int process_docking_magnets(struct game_client *c)
{
	unsigned char buffer[10];
	uint32_t id;
	uint8_t __attribute__((unused)) v;
	int i;
	int sound;

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
	pthread_mutex_unlock(&universe_mutex);
	snis_queue_add_sound(sound, ROLE_NAVIGATION, c->shipid);
	return 0;
}

static int process_engage_warp(struct game_client *c)
{
	unsigned char buffer[10];
	int b, i, rc;
	uint32_t id;
	uint8_t __attribute__((unused)) v;
	double wfactor;
	struct snis_entity *o;
	const union vec3 rightvec = { { 1.0f, 0.0f, 0.0f, } };
	union vec3 warpvec;
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
	b = lookup_bridge_by_shipid(o->id);
	if (b < 0) {
		snis_log(SNIS_ERROR, "Can't find bridge for shipid %u\n",
				o->id, __FILE__, __LINE__);
		pthread_mutex_unlock(&universe_mutex);
		return 0;
	}
	enough_oomph = ((double) o->tsd.ship.warpdrive / 255.0) > 0.075;
	if (enough_oomph) {
		wfactor = ((double) o->tsd.ship.warpdrive / 255.0) * (XKNOWN_DIM / 2.0);
		quat_rot_vec(&warpvec, &rightvec, &o->orientation);
		bridgelist[b].warpx = o->x + wfactor * warpvec.v.x;
		bridgelist[b].warpy = o->y + wfactor * warpvec.v.y;
		bridgelist[b].warpz = o->z + wfactor * warpvec.v.z;
		o->tsd.ship.warp_time = 85; /* 8.5 seconds */
	}
	pthread_mutex_unlock(&universe_mutex);
	send_initiate_warp_packet(c, enough_oomph);
	return 0;
}

static int process_request_warp_pwr(struct game_client *c)
{
	return process_request_bytevalue_pwr(c, offsetof(struct snis_entity, tsd.ship.power_data.warp.r2), no_limit); 
}

static int process_request_warp_coolant(struct game_client *c)
{
	return process_request_bytevalue_pwr(c, offsetof(struct snis_entity, tsd.ship.coolant_data.warp.r2), no_limit); 
}

static int process_request_impulse_pwr(struct game_client *c)
{
	return process_request_bytevalue_pwr(c, offsetof(struct snis_entity, tsd.ship.power_data.impulse.r2), no_limit); 
}

static int process_request_impulse_coolant(struct game_client *c)
{
	return process_request_bytevalue_pwr(c, offsetof(struct snis_entity, tsd.ship.coolant_data.impulse.r2), no_limit); 
}

static int process_request_sensors_pwr(struct game_client *c)
{
	return process_request_bytevalue_pwr(c, offsetof(struct snis_entity, tsd.ship.power_data.sensors.r2), no_limit); 
}

static int process_request_sensors_coolant(struct game_client *c)
{
	return process_request_bytevalue_pwr(c, offsetof(struct snis_entity, tsd.ship.coolant_data.sensors.r2), no_limit); 
}

static int process_request_comms_pwr(struct game_client *c)
{
	return process_request_bytevalue_pwr(c, offsetof(struct snis_entity, tsd.ship.power_data.comms.r2), no_limit); 
}

static int process_request_comms_coolant(struct game_client *c)
{
	return process_request_bytevalue_pwr(c, offsetof(struct snis_entity, tsd.ship.coolant_data.comms.r2), no_limit); 
}

static int process_request_shields_pwr(struct game_client *c)
{
	return process_request_bytevalue_pwr(c, offsetof(struct snis_entity, tsd.ship.power_data.shields.r2), no_limit); 
}

static int process_request_shields_coolant(struct game_client *c)
{
	return process_request_bytevalue_pwr(c, offsetof(struct snis_entity, tsd.ship.coolant_data.shields.r2), no_limit); 
}

static int process_request_phaserbanks_pwr(struct game_client *c)
{
	return process_request_bytevalue_pwr(c, offsetof(struct snis_entity, tsd.ship.power_data.phasers.r2), no_limit); 
}

static int process_request_phaserbanks_coolant(struct game_client *c)
{
	return process_request_bytevalue_pwr(c, offsetof(struct snis_entity, tsd.ship.coolant_data.phasers.r2), no_limit); 
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

static int process_demon_yaw(struct game_client *c)
{
	unsigned char buffer[10];
	struct snis_entity *o;
	uint32_t oid;
	uint8_t yaw;
	int i, rc;

	rc = read_and_unpack_buffer(c, buffer, "wb", &oid, &yaw);
	if (rc)
		return rc;
	if (!(c->role & ROLE_DEMON))
		return 0;
	pthread_mutex_lock(&universe_mutex);
	i = lookup_by_id(oid);
	if (i < 0)
		goto out;
	o = &go[i];

	switch (yaw) {
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

	vx = TORPEDO_VELOCITY * cos(o->heading);
	vz = TORPEDO_VELOCITY * -sin(o->heading);
	add_torpedo(o->x, o->y, o->z, vx, 0.0, vz, o->id); /* vy is wrong here... */
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

static void turn_off_tractorbeam(struct snis_entity *ship)
{
	int i;
	struct snis_entity *tb;

	/* universe lock must be held already. */
	i = lookup_by_id(ship->tsd.ship.tractor_beam);
	if (i < 0) {
		/* thing we were tractoring died. */
		ship->tsd.ship.tractor_beam = -1;
		return;
	}
	tb = &go[i];
	delete_from_clients_and_server(tb);
	ship->tsd.ship.tractor_beam = -1;
}

static int process_request_tractor_beam(struct game_client *c)
{
	unsigned char buffer[10];
	struct snis_entity *ship = &go[c->ship_index];
	uint32_t oid;
	int rc, i;

	rc = read_and_unpack_buffer(c, buffer, "w", &oid);
	if (rc)
		return rc;
	pthread_mutex_lock(&universe_mutex);
	if (oid == (uint32_t) 0xffffffff) { /* nothing selected, turn off tractor beam */
		turn_off_tractorbeam(ship);
		pthread_mutex_unlock(&universe_mutex);
		return 0;
	}
	i = lookup_by_id(oid);
	if (i < 0)
		goto tractorfail;

	/* If something is already tractored, turn off beam... */
	if (ship->tsd.ship.tractor_beam != -1) {
		i = lookup_by_id(ship->tsd.ship.tractor_beam);
		if (i >= 0 && oid == go[i].tsd.laserbeam.target) {
			/* if same thing selected, turn off beam and we're done */
			turn_off_tractorbeam(ship);
			pthread_mutex_unlock(&universe_mutex);
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
	return 0;

tractorfail:
	/* TODO: make special tractor failure sound */
	snis_queue_add_sound(LASER_FAILURE, ROLE_SOUNDSERVER, ship->id);
	pthread_mutex_unlock(&universe_mutex);
	return 0;
}

static int process_request_mining_bot(struct game_client *c)
{
	unsigned char buffer[10];
	struct snis_entity *ship = &go[c->ship_index];
	uint32_t oid;
	int rc, i;

	rc = read_and_unpack_buffer(c, buffer, "w", &oid);
	if (rc)
		return rc;
	pthread_mutex_lock(&universe_mutex);
	if (oid == (uint32_t) 0xffffffff) /* nothing selected */
		goto miningbotfail;
	i = lookup_by_id(oid);
	if (i < 0)
		goto miningbotfail;
	if (ship->tsd.ship.mining_bots <= 0) /* no bots left */
		goto miningbotfail;
	if (go[oid].type != OBJTYPE_ASTEROID)
		goto miningbotfail;

	i = add_mining_bot(ship, oid);
	if (i < 0)
		goto miningbotfail;
	snis_queue_add_sound(MINING_BOT_DEPLOYED,
				ROLE_COMMS | ROLE_SOUNDSERVER | ROLE_SCIENCE, ship->id);
	pthread_mutex_unlock(&universe_mutex);
	return 0;

miningbotfail:
	/* TODO: make special miningbot failure sound */
	snis_queue_add_sound(LASER_FAILURE, ROLE_SOUNDSERVER, ship->id);
	pthread_mutex_unlock(&universe_mutex);
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
		case OPCODE_DEMON_YAW:
			rc = process_demon_yaw(c);
			if (rc)
				goto protocol_error;
			break;
		case OPCODE_REQUEST_THROTTLE:
			rc = process_request_throttle(c);
			if (rc)
				goto protocol_error;
			break;
		case OPCODE_REQUEST_WARPDRIVE:
			rc = process_request_warpdrive(c);
			if (rc)
				goto protocol_error;
			break;
		case OPCODE_REQUEST_SHIELD:
			rc = process_request_shield(c);
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
		case OPCODE_REQUEST_LASER_WAVELENGTH:
			rc = process_request_laser_wavelength(c);
			if (rc)
				goto protocol_error;
			break;
		case OPCODE_REQUEST_MANEUVERING_PWR:
			rc = process_request_maneuvering_pwr(c);
			if (rc)
				goto protocol_error;
			break;
		case OPCODE_REQUEST_TRACTOR_PWR:
			rc = process_request_tractor_pwr(c);
			if (rc)
				goto protocol_error;
			break;
		case OPCODE_REQUEST_WARP_PWR:
			rc = process_request_warp_pwr(c);
			if (rc)
				goto protocol_error;
			break;
		case OPCODE_REQUEST_IMPULSE_PWR:
			rc = process_request_impulse_pwr(c);
			if (rc)
				goto protocol_error;
			break;
		case OPCODE_REQUEST_SHIELDS_PWR:
			rc = process_request_shields_pwr(c);
			if (rc)
				goto protocol_error;
			break;
		case OPCODE_REQUEST_SENSORS_PWR:
			rc = process_request_sensors_pwr(c);
			if (rc)
				goto protocol_error;
			break;
		case OPCODE_REQUEST_COMMS_PWR:
			rc = process_request_comms_pwr(c);
			if (rc)
				goto protocol_error;
			break;
		case OPCODE_REQUEST_PHASERBANKS_PWR:
			rc = process_request_phaserbanks_pwr(c);
			if (rc)
				goto protocol_error;
			break;
		case OPCODE_REQUEST_MANEUVERING_COOLANT:
			rc = process_request_maneuvering_coolant(c);
			if (rc)
				goto protocol_error;
			break;
		case OPCODE_REQUEST_TRACTOR_COOLANT:
			rc = process_request_tractor_coolant(c);
			if (rc)
				goto protocol_error;
			break;
		case OPCODE_REQUEST_WARP_COOLANT:
			rc = process_request_warp_coolant(c);
			if (rc)
				goto protocol_error;
			break;
		case OPCODE_REQUEST_IMPULSE_COOLANT:
			rc = process_request_impulse_coolant(c);
			if (rc)
				goto protocol_error;
			break;
		case OPCODE_REQUEST_SHIELDS_COOLANT:
			rc = process_request_shields_coolant(c);
			if (rc)
				goto protocol_error;
			break;
		case OPCODE_REQUEST_SENSORS_COOLANT:
			rc = process_request_sensors_coolant(c);
			if (rc)
				goto protocol_error;
			break;
		case OPCODE_REQUEST_COMMS_COOLANT:
			rc = process_request_comms_coolant(c);
			if (rc)
				goto protocol_error;
			break;
		case OPCODE_REQUEST_PHASERBANKS_COOLANT:
			rc = process_request_phaserbanks_coolant(c);
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
		case OPCODE_REQUEST_SCIZOOM:
			rc = process_request_scizoom(c);
			if (rc)
				goto protocol_error;
			break;
		case OPCODE_REQUEST_NAVZOOM:
			rc = process_request_navzoom(c);
			if (rc)
				goto protocol_error;
			break;
		case OPCODE_REQUEST_MAINZOOM:
			rc = process_request_mainzoom(c);
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
		case OPCODE_SCI_SELECT_COORDS:
			rc = process_sci_select_coords(c);
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
		case OPCODE_UPDATE_BUILD_INFO:
			rc = process_build_info(c);
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
	pb_prepend_queue_to_client(c, packed_buffer_new("bbwS", OPCODE_UPDATE_UNIVERSE_TIMESTAMP, code,
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

static void queue_netstats(struct game_client *c)
{
	struct timeval now;
	uint32_t elapsed_seconds;

	if ((universe_timestamp & 0x0f) != 0x0f)
		return;
	gettimeofday(&now, NULL);
	elapsed_seconds = now.tv_sec - netstats.start.tv_sec;
	pb_queue_to_client(c, packed_buffer_new("bqqwwwwwwww", OPCODE_UPDATE_NETSTATS,
					netstats.bytes_sent, netstats.bytes_recd,
					netstats.nobjects, netstats.nships,
					elapsed_seconds,
					faction_population[0],
					faction_population[1],
					faction_population[2],
					faction_population[3],
					faction_population[4]));
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

#define GO_TOO_FAR_UPDATE_PER_NTICKS 7

static void queue_up_client_updates(struct game_client *c)
{
	int i;
	int count;

	count = 0;
	pthread_mutex_lock(&universe_mutex);
	if (universe_timestamp != c->timestamp) {
		queue_netstats(c);
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
			return;

		if (o->timestamp != c->go_clients[go_index(o)].last_timestamp_sent) {
			queue_up_client_object_update(c, o);
			c->go_clients[go_index(o)].last_timestamp_sent = o->timestamp;
		}
	}
}

static void queue_up_client_id(struct game_client *c)
{
	/* tell the client what his ship id is. */
	pb_queue_to_client(c, packed_buffer_new("bw", OPCODE_ID_CLIENT_SHIP, c->shipid));
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

	/* Wait for client[] array to get fully updated before proceeding. */
	client_lock();
	get_client(c);
	client_unlock();

	const uint8_t over_clock = 4;
	const double maxTimeBehind = 0.5;
	double delta = 1.0/10.0/(double)over_clock;

	uint8_t no_write_count = 0;
	double currentTime = time_now_double();
	double nextTime = currentTime + delta;
	while (1) {
		if (c->socket < 0)
			break;

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
	uint8_t ai[MAX_AI_STACK_ENTRIES];
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

	pb_queue_to_client(c, packed_buffer_new("bwwhSSSQwb", opcode,
			o->id, o->timestamp, o->alive, o->x, (int32_t) UNIVERSE_DIM,
			o->y, (int32_t) UNIVERSE_DIM, o->z, (int32_t) UNIVERSE_DIM,
			&o->orientation, victim_id, o->tsd.ship.shiptype));

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

	pb = packed_buffer_new("bwSSSwU", OPCODE_DETONATE,
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
}

static void send_respawn_time(struct game_client *c,
	struct snis_entity *o)
{
	uint8_t seconds = (o->respawn_time - universe_timestamp) / 10;

	pb_queue_to_client(c, packed_buffer_new("bb", OPCODE_UPDATE_RESPAWN_TIME, seconds));
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
	uint32_t fuel;
	uint8_t tloading, tloaded, throttle, rpm;

	throttle = o->tsd.ship.throttle;
	rpm = o->tsd.ship.rpm;
	fuel = o->tsd.ship.fuel;

	tloading = (uint8_t) (o->tsd.ship.torpedoes_loading & 0x0f);
	tloaded = (uint8_t) (o->tsd.ship.torpedoes_loaded & 0x0f);
	tloading = tloading | (tloaded << 4);

	pb = packed_buffer_allocate(sizeof(struct update_ship_packet));
	packed_buffer_append(pb, "bwwhSSS", opcode, o->id, o->timestamp, o->alive,
			o->x, (int32_t) UNIVERSE_DIM, o->y, (int32_t) UNIVERSE_DIM,
			o->z, (int32_t) UNIVERSE_DIM);
	packed_buffer_append(pb, "RRRwwRRRbbbwbbbbbbbbbbbbbwQQQbb",
			o->tsd.ship.yaw_velocity,
			o->tsd.ship.pitch_velocity,
			o->tsd.ship.roll_velocity,
			o->tsd.ship.torpedoes, o->tsd.ship.power,
			o->tsd.ship.gun_yaw_velocity,
			o->tsd.ship.sci_heading,
			o->tsd.ship.sci_beam_width,
			tloading, throttle, rpm, fuel, o->tsd.ship.temp,
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
			o->tsd.ship.docking_magnets);
	pb_queue_to_client(c, pb);
}

static void send_update_damcon_obj_packet(struct game_client *c,
		struct snis_damcon_entity *o)
{
	pb_queue_to_client(c, packed_buffer_new("bwwwSSSRb",
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
	pb_queue_to_client(c, packed_buffer_new("bwwwSSwbb",
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
	pb_queue_to_client(c, packed_buffer_new("bwwwSSRbbb",
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
	pb_queue_to_client(c, packed_buffer_new("bwwSSSSSSbbbb", OPCODE_UPDATE_ASTEROID, o->id, o->timestamp,
					o->x, (int32_t) UNIVERSE_DIM,
					o->y, (int32_t) UNIVERSE_DIM,
					o->z, (int32_t) UNIVERSE_DIM,
					o->vx, (int32_t) UNIVERSE_DIM,
					o->vy, (int32_t) UNIVERSE_DIM,
					o->vz, (int32_t) UNIVERSE_DIM,
					o->tsd.asteroid.carbon,
					o->tsd.asteroid.nickeliron,
					o->tsd.asteroid.silicates,
					o->tsd.asteroid.preciousmetals));
}

static void send_update_cargo_container_packet(struct game_client *c,
	struct snis_entity *o)
{
	pb_queue_to_client(c, packed_buffer_new("bwwSSS", OPCODE_UPDATE_CARGO_CONTAINER, o->id, o->timestamp,
					o->x, (int32_t) UNIVERSE_DIM,
					o->y, (int32_t) UNIVERSE_DIM,
					o->z, (int32_t) UNIVERSE_DIM));
}

static void send_update_derelict_packet(struct game_client *c,
	struct snis_entity *o)
{
	pb_queue_to_client(c, packed_buffer_new("bwwSSSb", OPCODE_UPDATE_DERELICT, o->id, o->timestamp,
					o->x, (int32_t) UNIVERSE_DIM,
					o->y, (int32_t) UNIVERSE_DIM,
					o->z, (int32_t) UNIVERSE_DIM,
					o->tsd.derelict.shiptype));
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

	pb_queue_to_client(c, packed_buffer_new("bwwSSSSwbbbbhbbbS", OPCODE_UPDATE_PLANET, o->id, o->timestamp,
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
					o->tsd.planet.atmosphere_scale, (int32_t) UNIVERSE_DIM));
}

static void send_update_wormhole_packet(struct game_client *c,
	struct snis_entity *o)
{
	pb_queue_to_client(c, packed_buffer_new("bwwSSS", OPCODE_UPDATE_WORMHOLE,
					o->id, o->timestamp,
					o->x, (int32_t) UNIVERSE_DIM,
					o->y, (int32_t) UNIVERSE_DIM,
					o->z, (int32_t) UNIVERSE_DIM));
}

static void send_update_starbase_packet(struct game_client *c,
	struct snis_entity *o)
{
	pb_queue_to_client(c, packed_buffer_new("bwwSSSQ", OPCODE_UPDATE_STARBASE,
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
	pb_queue_to_client(c, packed_buffer_new("bwwSSSSQQSS", OPCODE_UPDATE_NEBULA, o->id, o->timestamp,
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
	pb_queue_to_client(c, packed_buffer_new("bwwSSShhhb", OPCODE_UPDATE_EXPLOSION, o->id, o->timestamp,
				o->x, (int32_t) UNIVERSE_DIM, o->y, (int32_t) UNIVERSE_DIM,
				o->z, (int32_t) UNIVERSE_DIM,
				o->tsd.explosion.nsparks, o->tsd.explosion.velocity,
				o->tsd.explosion.time, o->tsd.explosion.victim_type));
}

static void send_update_torpedo_packet(struct game_client *c,
	struct snis_entity *o)
{
	pb_queue_to_client(c, packed_buffer_new("bwwwSSS", OPCODE_UPDATE_TORPEDO, o->id, o->timestamp,
					o->tsd.torpedo.ship_id,
					o->x, (int32_t) UNIVERSE_DIM,
					o->y, (int32_t) UNIVERSE_DIM,
					o->z, (int32_t) UNIVERSE_DIM));
}

static void send_update_laser_packet(struct game_client *c,
	struct snis_entity *o)
{
	pb_queue_to_client(c, packed_buffer_new("bwwwbSSSQ", OPCODE_UPDATE_LASER,
					o->id, o->timestamp, o->tsd.laser.ship_id, o->tsd.laser.power,
					o->x, (int32_t) UNIVERSE_DIM,
					o->y, (int32_t) UNIVERSE_DIM,
					o->z, (int32_t) UNIVERSE_DIM,
					&o->orientation));
}

static void send_update_laserbeam_packet(struct game_client *c,
	struct snis_entity *o)
{
	pb_queue_to_client(c, packed_buffer_new("bwwww", OPCODE_UPDATE_LASERBEAM,
					o->id, o->timestamp, o->tsd.laserbeam.origin,
					o->tsd.laserbeam.target));
}

static void send_update_tractorbeam_packet(struct game_client *c,
	struct snis_entity *o)
{
	pb_queue_to_client(c, packed_buffer_new("bwwww", OPCODE_UPDATE_TRACTORBEAM,
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
	pb_queue_to_client(c, packed_buffer_new("bwwSSSSQb", OPCODE_UPDATE_DOCKING_PORT,
					o->id, o->timestamp,
					scale, (int32_t) 1000,
					o->x, (int32_t) UNIVERSE_DIM,
					o->y, (int32_t) UNIVERSE_DIM,
					o->z, (int32_t) UNIVERSE_DIM,
					&o->orientation,
					o->tsd.docking_port.model));
}

static void send_update_spacemonster_packet(struct game_client *c,
	struct snis_entity *o)
{
	pb_queue_to_client(c, packed_buffer_new("bwwSSS", OPCODE_UPDATE_SPACEMONSTER, o->id, o->timestamp,
					o->x, (int32_t) UNIVERSE_DIM,
					o->z, (int32_t) UNIVERSE_DIM,
					o->tsd.spacemonster.zz, (int32_t) UNIVERSE_DIM));
}

static int add_new_player(struct game_client *c)
{
	int rc;
	struct add_player_packet app;

	rc = snis_readsocket(c->socket, &app, sizeof(app));
	if (rc)
		return rc;
	app.role = ntohl(app.role);
	if (app.opcode != OPCODE_UPDATE_PLAYER) {
		snis_log(SNIS_ERROR, "bad opcode %d\n", app.opcode);
		goto protocol_error;
	}
	app.shipname[19] = '\0';
	app.password[19] = '\0';

	if (insane(app.shipname, 20) || insane(app.password, 20)) {
		snis_log(SNIS_ERROR, "Bad ship name or password\n");
		goto protocol_error;
	}

	c->bridge = lookup_bridge(app.shipname, app.password);
	c->role = app.role;
	if (c->bridge == -1) { /* did not find our bridge, have to make a new one. */
		double x, z;

		for (int i = 0; i < 100; i++) {
			x = XKNOWN_DIM * (double) rand() / (double) RAND_MAX;
			z = ZKNOWN_DIM * (double) rand() / (double) RAND_MAX;
			if (dist3d(x - SUNX, 0, z - SUNZ) > SUN_DIST_LIMIT)
				break;
		}
		c->ship_index = add_player(x, z, 0.0, 0.0, M_PI / 2.0);
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
		c->bridge = nbridges;
		populate_damcon_arena(&bridgelist[c->bridge].damcon);
	
		nbridges++;
		schedule_callback(event_callback, &callback_schedule,
				"player-respawn-event", (double) c->shipid);
	} else {
		c->shipid = bridgelist[c->bridge].shipid;
		c->ship_index = lookup_by_id(c->shipid);
	}
	c->debug_ai = 0;
	c->request_universe_timestamp = 0;
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

/* Creates a thread for each incoming connection... */
static void service_connection(int connection)
{
	int i, j, rc, flag = 1;
	int bridgenum, client_count;
	int thread_count, iterations;

	log_client_info(SNIS_INFO, connection, "snis_server: servicing snis_client connection\n");
        /* get connection moved off the stack so that when the thread needs it,
	 * it's actually still around. 
	 */

	i = setsockopt(connection, IPPROTO_TCP, TCP_NODELAY, (char *) &flag, sizeof(int));
	if (i < 0)
		snis_log(SNIS_ERROR, "setsockopt failed: %s.\n", strerror(errno));

	if (verify_client_protocol(connection)) {
		log_client_info(SNIS_ERROR, connection, "disconnected, protocol violation\n");
		close(connection);
		return;
	}

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

	add_new_player(&client[i]);

	pthread_attr_init(&client[i].read_attr);
	pthread_attr_setdetachstate(&client[i].read_attr, PTHREAD_CREATE_DETACHED);
	pthread_attr_init(&client[i].write_attr);
	pthread_attr_setdetachstate(&client[i].write_attr, PTHREAD_CREATE_DETACHED);

	/* initialize refcount to 1 to keep client[i] from getting reaped. */
	client[i].refcount = 1;

	/* create threads... */
        rc = pthread_create(&client[i].read_thread,
		&client[i].read_attr, per_client_read_thread, (void *) &client[i]);
	if (rc) {
		snis_log(SNIS_ERROR, "per client read thread, pthread_create failed: %d %s %s\n",
			rc, strerror(rc), strerror(errno));
	}
        rc = pthread_create(&client[i].write_thread,
		&client[i].write_attr, per_client_write_thread, (void *) &client[i]);
	if (rc) {
		snis_log(SNIS_ERROR, "per client write thread, pthread_create failed: %d %s %s\n",
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

	if (client_count == 1)
		snis_queue_add_global_sound(STARSHIP_JOINED);
	else
		snis_queue_add_sound(CREWMEMBER_JOINED, ROLE_ALL,
					bridgelist[bridgenum].shipid);

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

	snis_log(SNIS_INFO, "bottom of 'service connection'\n");
}

/* This thread listens for incoming client connections, and
 * on establishing a connection, starts a thread for that 
 * connection.
 */
static void *listener_thread(__attribute__((unused)) void * unused)
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
			printf("snis_server: Using SNISSERVERPORT value %d\n",
				default_snis_server_port);
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
	pthread_attr_t attr;
	pthread_t thread;
	int rc;

	/* Setup to wait for the listener thread to become ready... */
	pthread_cond_init (&listener_started, NULL);
        (void) pthread_mutex_lock(&listener_mutex);

	/* Create the listener thread... */
        pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
        rc = pthread_create(&thread, &attr, listener_thread, NULL);
	if (rc) {
		snis_log(SNIS_ERROR, "Failed to create listener thread, pthread_create: %d %s %s\n",
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
	float damage[8];
	int nobjs;

	for (i = 0; i < 8; i++)
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
		if (socket->tsd.socket.system >= 8)
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

	BUILD_ASSERT(sizeof(o->tsd.ship.damage) == 8);
	int changed = 0;
	for (i = 0; i < 8; i++) {
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
int compare_opcode_stats(const void *a, const void *b)
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

static void move_objects(double absolute_time, int discontinuity)
{
	int i;

	pthread_mutex_lock(&universe_mutex);
	memset(faction_population, 0, sizeof(faction_population));
	netstats.nobjects = 0;
	netstats.nships = 0;
	universe_timestamp++;
	universe_timestamp_absolute = absolute_time;

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
			netstats.nobjects++;
		} else {
			if (go[i].type == OBJTYPE_SHIP1 &&
				universe_timestamp >= go[i].respawn_time) {
				respawn_player(&go[i]);
				schedule_callback(event_callback, &callback_schedule,
					"player-respawn-event", (double) go[i].id);
				send_ship_damage_packet(&go[i]);
			} else
				go[i].timestamp = universe_timestamp; /* respawn is counting down */
		}
	}
	for (i = 0; i < nfactions(); i++)
		if (i == 0 || faction_population[lowest_faction] > faction_population[i])
			lowest_faction = i;
	move_damcon_entities();
	pthread_mutex_unlock(&universe_mutex);
	fire_lua_timers();
	fire_lua_callbacks(&callback_schedule);
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

void usage(void)
{
	fprintf(stderr, "snis_server lobbyserver gameinstance servernick location\n");
	fprintf(stderr, "For example: snis_server lobbyserver 'steves game' zuul Houston\n");
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
	add_lua_callable_fn(l_add_starbase, "add_starbase");
	add_lua_callable_fn(l_add_planet, "add_planet");
	add_lua_callable_fn(l_add_nebula, "add_nebula");
	add_lua_callable_fn(l_add_spacemonster, "add_spacemonster");
	add_lua_callable_fn(l_add_derelict, "add_derelict");
	add_lua_callable_fn(l_add_wormhole_pair, "add_wormhole_pair");
	add_lua_callable_fn(l_get_player_ship_ids, "get_player_ship_ids");
	add_lua_callable_fn(l_get_object_location, "get_object_location");
	add_lua_callable_fn(l_move_object, "move_object");
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
}

static int run_initial_lua_scripts(void)
{
	int rc;
	struct stat statbuf;
	char scriptname[PATH_MAX];

	snprintf(scriptname, sizeof(scriptname) - 1,
			"%s/%s", LUASCRIPTDIR, "initialize.lua");
	rc = stat(scriptname, &statbuf);
	if (rc != 0)
		return rc;
	rc = luaL_dofile(lua_state, scriptname);
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
		if (rc) {
			/* TODO: something? */
			printf("lua script %s failed to execute.\n", lua_command);
		}
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

int main(int argc, char *argv[])
{
	int port, rc, i;
	struct timespec thirtieth_second;

	take_your_locale_and_shove_it();
	if (argc < 5) 
		usage();

	if (argc >= 6) {
		if (strcmp(argv[5], "--enable-enscript") == 0) {
			lua_enscript_enabled = 1;
			fprintf(stderr, "WARNING: lua enscript enabled!\n");
			fprintf(stderr, "THIS PERMITS USERS TO CREATE FILES ON THE SERVER\n");
		}
	}

	override_asset_dir();
	set_random_seed();

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

	open_log_file();

	setup_lua();
	snis_protocol_debugging(1);

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
	if (getenv("SNISSERVERNOLOBBY") == NULL)
		register_with_game_lobby(argv[1], port, argv[2], argv[1], argv[3]);
	else
		printf("snis_server: Skipping lobby registration\n");

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
