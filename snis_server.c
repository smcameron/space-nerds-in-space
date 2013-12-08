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
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netdb.h>
#include <errno.h>
#include <time.h>
#include <ctype.h>
#include <math.h>
#if !defined(__APPLE__) && !defined(__FreeBSD__)
#include <linux/tcp.h>
#else
#include <netinet/tcp.h>
#endif
#include <stddef.h>
#include <stdarg.h>
#include <limits.h>
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
#include <assert.h>

#include "build_bug_on.h"
#include "ssgl/ssgl.h"
#include "snis_ship_type.h"
#include "snis_faction.h"
#include "space-part.h"
#include "quat.h"
#include "snis.h"
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

#define ARRAY_SIZE(x) (sizeof(x) / sizeof(x[0]))
#define CLIENT_UPDATE_PERIOD_NSECS 500000000
#define MAXCLIENTS 100

struct network_stats netstats;

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
} client[MAXCLIENTS];
int nclients = 0;
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
} bridgelist[MAXCLIENTS];
int nbridges = 0;
static pthread_mutex_t universe_mutex = PTHREAD_MUTEX_INITIALIZER;

static pthread_mutex_t listener_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t listener_started;
int listener_port = -1;
pthread_t lobbythread;
char *lobbyserver = NULL;
static int snis_log_level = 2;
static struct ship_type_entry *ship_type;
int nshiptypes;

static int safe_mode = 0; /* prevents enemies from attacking if set */

static char *default_asset_dir = "share/snis";
static char *asset_dir;

static int nebulalist[NNEBULA] = { 0 };

static inline void client_lock()
{
        (void) pthread_mutex_lock(&client_mutex);
}

static inline void client_unlock()
{
        (void) pthread_mutex_unlock(&client_mutex);
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
	free(qe);
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
	unsigned int addrlen;
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
}

static void derelict_move(struct snis_entity *o)
{
	set_object_location(o, o->x + o->vx, o->y + o->vy, o->z + o->vz);
	o->timestamp = universe_timestamp;
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
		dist2 = ((t->x - o->x) * (t->x - o->x)) +
			((t->z - o->z) * (t->z - o->z));
		if (dist2 < 30.0 * 30.0) {
			a = snis_randn(360) * M_PI / 180.0;
			r = 60.0;
			set_object_location(t, o->tsd.wormhole.dest_x + cos(a) * r, 
						t->y, o->tsd.wormhole.dest_z + sin(a) * r);
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

static inline void pb_queue_to_client(struct game_client *c, struct packed_buffer *pb)
{
	if (!pb) {
		stacktrace("snis_server: NULL packed_buffer in pb_queue_to_client()");
		return;
	}
	packed_buffer_queue_add(&c->client_write_queue, pb, &c->client_write_queue_mutex);
}

static void queue_delete_oid(struct game_client *c, uint32_t oid)
{
	pb_queue_to_client(c, packed_buffer_new("hw",OPCODE_DELETE_OBJECT, oid));
}

static int add_ship(void);
static void respawn_object(int otype)
{
	switch (otype) {
		case OBJTYPE_SHIP2:
			add_ship();
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
	snis_object_pool_free_object(pool, o->index);
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
		queue_delete_oid(&client[i], oid);
	client_unlock();
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
		break;
	default:
		schedule_callback(event_callback, &callback_schedule,
				"object-death-callback", o->id);
		break;
	}

	if (o->type == OBJTYPE_SHIP2)
		fleet_leave(o->id); /* leave any fleets ship might be a member of */
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
	pb_queue_to_client(c, packed_buffer_new("hh", OPCODE_PLAY_SOUND, sound_number));
}

static void snis_queue_add_sound(uint16_t sound_number, uint32_t roles, uint32_t shipid)
{
	int i;
	struct game_client *c;

	client_lock();
	for (i = 0; i < nclients; i++) {
		c = &client[i];
		if ((c->role & roles) && c->shipid == shipid)
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

static int roll_damage(double weapons_factor, double shield_strength, uint8_t system)
{
	int damage = system + (uint8_t) (weapons_factor *
				(double) (20 + snis_randn(40)) * (1.2 - shield_strength));
	if (damage > 255)
		damage = 255;
	return damage;
}

static void calculate_torpedo_damage(struct snis_entity *o)
{
	double ss;
	const double twp = TORPEDO_WEAPONS_FACTOR * (o->type == OBJTYPE_SHIP1 ? 0.333 : 1.0);

	ss = shield_strength(snis_randn(255), o->sdata.shield_strength,
				o->sdata.shield_width,
				o->sdata.shield_depth,
				o->sdata.shield_wavelength);

	o->tsd.ship.damage.shield_damage = roll_damage(twp, ss, o->tsd.ship.damage.shield_damage);
	o->tsd.ship.damage.impulse_damage = roll_damage(twp, ss, o->tsd.ship.damage.impulse_damage);
	o->tsd.ship.damage.warp_damage = roll_damage(twp, ss, o->tsd.ship.damage.warp_damage);
	o->tsd.ship.damage.maneuvering_damage = roll_damage(twp, ss, o->tsd.ship.damage.maneuvering_damage);
	o->tsd.ship.damage.phaser_banks_damage = roll_damage(twp, ss, o->tsd.ship.damage.phaser_banks_damage);
	o->tsd.ship.damage.sensors_damage = roll_damage(twp, ss, o->tsd.ship.damage.sensors_damage);
	o->tsd.ship.damage.comms_damage = roll_damage(twp, ss, o->tsd.ship.damage.comms_damage);
	o->tsd.ship.damage.tractor_damage = roll_damage(twp, ss, o->tsd.ship.damage.tractor_damage);

	if (o->tsd.ship.damage.shield_damage == 255) { 
		o->respawn_time = universe_timestamp + RESPAWN_TIME_SECS * 10;
		o->alive = 0;
	}
}

static void calculate_laser_damage(struct snis_entity *o, uint8_t wavelength, float power)
{
	int i;
	unsigned char *x = (unsigned char *) &o->tsd.ship.damage;
	double ss;
	float power_factor = (float) power / 255.0;
	float damage;

	ss = shield_strength(wavelength, o->sdata.shield_strength,
				o->sdata.shield_width,
				o->sdata.shield_depth,
				o->sdata.shield_wavelength);

	for (i = 0; i < sizeof(o->tsd.ship.damage); i++) {
		damage = ((float) (snis_randn(LASER_DAMAGE_MAX) + 1) * (1 - ss) * power_factor);

		/* if damage will get rounded down to nothing bump it up to 1.0 */
		if (damage < 0.5)
			damage = 1.0;

		damage = (int) ((float) x[i] + damage);
		if (damage > 255.0)
			damage = 255.0;
		x[i] = (uint8_t) damage;
	}
	if (o->tsd.ship.damage.shield_damage == 255) {
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
static void send_overheat_ship_damage_packet(struct snis_entity *o);
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

	/* FIXME: do something smarter/better */
	random_dpoint_on_sphere(snis_randn(maxdist - mindist) + mindist,
					&o->tsd.ship.dox,
					&o->tsd.ship.doy,
					&o->tsd.ship.doz);
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

	fleet_number = find_fleet_number(attacker);

	n = attacker->tsd.ship.nai_entries;
	if (n > 0 && attacker->tsd.ship.ai[n - 1].ai_mode == AI_MODE_ATTACK) {

		if (attacker->tsd.ship.ai[n - 1].u.attack.victim_id == victim_id)
			return; /* already attacking this guy */

		i = lookup_by_id(victim_id);
		if (i < 0)
			return;
		v = &go[i];
		d1 = dist3dsqrd(v->x - attacker->x, v->y - attacker->y, v->z - attacker->z);
		i = lookup_by_id(attacker->tsd.ship.ai[n - 1].u.attack.victim_id);
		if (i < 0) {
			attacker->tsd.ship.ai[n - 1].u.attack.victim_id = victim_id;
			calculate_attack_vector(attacker, LASER_RANGE, 400);
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
			calculate_attack_vector(attacker, LASER_RANGE, 400);
			buddies_join_the_fight(fleet_number, v, recursion_level);
			return;
		}
	}
	if (n < MAX_AI_STACK_ENTRIES) {
		attacker->tsd.ship.ai[n].ai_mode = AI_MODE_ATTACK;
		attacker->tsd.ship.ai[n].u.attack.victim_id = victim_id;
		attacker->tsd.ship.nai_entries++;
		calculate_attack_vector(attacker, LASER_RANGE, 400);
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

static int add_derelict(const char *name, double x, double y, double z,
			int shiptype, int the_faction);

static int make_derelict(struct snis_entity *o)
{
	return add_derelict(o->sdata.name, o->x, o->y, o->z, o->tsd.ship.shiptype, o->sdata.faction);
}

struct potential_victim_info {
	struct snis_entity *o;
	double fightiness;
	int victim_id;
};

static void process_potential_victim(void *context, void *entity)
{
	struct potential_victim_info *info = context;
	struct snis_entity *v = entity; 
	struct snis_entity *o = info->o; 
	double dist;
	float hostility, fightiness;

	if (o->index == v->index) /* don't victimize self */
		return;

	/* only victimize players, other ships and starbases */
	if (v->type != OBJTYPE_STARBASE && v->type != OBJTYPE_SHIP1 &&
		v->type != OBJTYPE_SHIP2)
		return;

	if (!v->alive) /* skip the dead guys */
		return;

	hostility = faction_hostility(o->sdata.faction, v->sdata.faction);
	dist = dist3d(o->x - v->x, o->y - v->y, o->z - v->z);

	if (dist > XKNOWN_DIM / 10.0) /* too far away */
		return;

	if (hostility < FACTION_HOSTILITY_THRESHOLD)
		return;

	fightiness = (10000.0 * hostility) / (dist + 1.0);

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

	space_partition_process(space_partition, o, o->x, o->y, &info,
				process_potential_victim);
	return info.victim_id;
}

union vec3 pick_random_patrol_destination(struct snis_entity *ship)
{
	union vec3 v;
	struct snis_entity *o;
	int i, count;
	double dx, dy, dz;

	random_dpoint_on_sphere(100.0, &dx, &dy, &dz);
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
	int n;
	int fleet_shape;

	if (o->tsd.ship.nai_entries > 0)
		return;
	switch (snis_randn(100) % 3) {
	case 0:
	case 1:	
		setup_patrol_route(o);
		break;
	case 2:
		if ((snis_randn(100) % 2) == 0)
			fleet_shape = FLEET_TRIANGLE;
		else
			fleet_shape = FLEET_SQUARE;
		setup_patrol_route(o);
		if (fleet_count() < 5) {
			o->tsd.ship.ai[0].ai_mode = AI_MODE_FLEET_LEADER;
			o->tsd.ship.ai[0].u.fleet.fleet = fleet_new(fleet_shape, o->id);
			o->tsd.ship.ai[0].u.fleet.fleet_position = 0;
			if (o->sdata.faction == 0)
				o->sdata.faction = 1;
		} else {
			struct snis_entity *leader;
			int32_t leader_id;
			int i;

			n = snis_randn(5);
			o->tsd.ship.ai[0].ai_mode = AI_MODE_FLEET_MEMBER;
			o->tsd.ship.ai[0].u.fleet.fleet = n;
			o->tsd.ship.ai[0].u.fleet.fleet_position = fleet_join(n, o->id);

			/* keep fleet memebers from fighting each other */
			/* FIXME: think of something better */
			leader_id = fleet_get_leader_id(n);
			i = lookup_by_id(leader_id);
			leader = &go[i];
			o->sdata.faction = leader->sdata.faction;
		}
	default:
		break;
	}
}

static void __attribute__((unused)) pop_ai_stack(struct snis_entity *o)
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
		calculate_attack_vector(o, LASER_RANGE, 400);
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
		calculate_attack_vector(o, LASER_RANGE, 400);
	return;
}

static void torpedo_collision_detection(void *context, void *entity)
{
	struct snis_entity *o = context; /* torpedo */
	struct snis_entity *t = entity;  /* target */
	double dist2;

	if (!t->alive)
		return;
	if (t->index == o->index)
		return;
	if (o->alive >= TORPEDO_LIFETIME - 3)
		return;
	if (t->type != OBJTYPE_SHIP1 && t->type != OBJTYPE_SHIP2 &&
			t->type != OBJTYPE_STARBASE &&
			t->type != OBJTYPE_ASTEROID)
		return;
	if (t->id == o->tsd.torpedo.ship_id)
		return; /* can't torpedo yourself. */
	dist2 = ((t->x - o->x) * (t->x - o->x) +
		(t->z - o->z) * (t->z - o->z));

	if (dist2 > TORPEDO_DETONATE_DIST2)
		return; /* not close enough */

	/* hit!!!! */
	o->alive = 0;

	if (t->type == OBJTYPE_STARBASE) {
		t->tsd.starbase.under_attack = 1;
		return;
	}

	if (t->type == OBJTYPE_SHIP1 || t->type == OBJTYPE_SHIP2) {
		calculate_torpedo_damage(t);
		send_ship_damage_packet(t);
		attack_your_attacker(t, lookup_entity_by_id(o->tsd.torpedo.ship_id));
	} else if (t->type == OBJTYPE_ASTEROID && fabs(t->y) < 100.0) {
		t->alive = 0;
	}

	if (!t->alive) {
		(void) add_explosion(t->x, t->y, t->z, 50, 50, 50, t->type);
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
			delete_from_clients_and_server(t);
			respawn_object(t->type);
			
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
	set_object_location(o, o->x + o->vx, o->y + o->vy, o->z + o->vz);
	o->timestamp = universe_timestamp;
	o->alive--;
	space_partition_process(space_partition, o, o->x, o->z, o,
			torpedo_collision_detection);
	if (o->alive <= 0)
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
				double px, double py, double pz)
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
	return (point_to_3d_line_dist(lx1, ly1, lz1, lx2, ly2, lz2, px, py, pz) < 350.0);
}

static void laser_collision_detection(void *context, void *entity)
{
	struct snis_entity *o = context; /* laser */
	struct snis_entity *t = entity;  /* target */

	if (!t->alive)
		return;
	if (t->index == o->index)
		return;
	if (t->type != OBJTYPE_SHIP1 && t->type != OBJTYPE_SHIP2 &&
		t->type != OBJTYPE_STARBASE && t->type != OBJTYPE_ASTEROID)
		return;
	if (t->id == o->tsd.laser.ship_id)
		return; /* can't laser yourself. */

	if (!laser_point_collides(o->x + o->vx, o->y + o->vy, o->z + o->vz,
				o->x - o->vx, o->y - o->vy, o->z - o->vz,
				t->x, t->y, t->z))
		return; /* not close enough */
		
	/* hit!!!! */
	o->alive = 0;

	if (t->type == OBJTYPE_STARBASE) {
		t->tsd.starbase.under_attack = 1;
		return;
	}

	if (t->type == OBJTYPE_SHIP1 || t->type == OBJTYPE_SHIP2) {
		calculate_laser_damage(t, o->tsd.laser.wavelength,
			(float) o->tsd.laser.power * LASER_PROJECTILE_BOOST);
		send_ship_damage_packet(t);
		attack_your_attacker(t, lookup_entity_by_id(o->tsd.laser.ship_id));
	}

	if (t->type == OBJTYPE_ASTEROID)
		t->alive = 0;

	if (!t->alive) {
		(void) add_explosion(t->x, t->y, t->z, 50, 50, 50, t->type);
		/* TODO -- these should be different sounds */
		/* make sound for players that got hit */
		/* make sound for players that did the hitting */
		snis_queue_add_sound(EXPLOSION_SOUND,
				ROLE_SOUNDSERVER, o->tsd.laser.ship_id);
		if (t->type != OBJTYPE_SHIP1) {
			if (t->type == OBJTYPE_SHIP2)
				make_derelict(t);
			delete_from_clients_and_server(t);
			respawn_object(t->type);
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
	set_object_location(o, o->x + o->vx, o->y + o->vy, o->z + o->vz);
	o->timestamp = universe_timestamp;
	o->alive--;
	space_partition_process(space_partition, o, o->x, o->z, o,
			laser_collision_detection);
	if (o->alive <= 0)
		delete_from_clients_and_server(o);
}

static void send_comms_packet(char *sender, const char *str);
static void taunt_player(struct snis_entity *alien, struct snis_entity *player)
{
	char buffer[1000];
	char name[100];
	char tmpbuf[50];
	int last_space = 0;
	int i, bytes_so_far = 0;
	char *start = buffer;
	
	infinite_taunt(buffer, sizeof(buffer) - 1);

	sprintf(name, "%s: ", alien->sdata.name);
	for (i = 0; buffer[i]; i++) {
		buffer[i] = toupper(buffer[i]);
		if (buffer[i] == ' ')
			last_space = bytes_so_far;
		bytes_so_far++;
		if (last_space > 28) {
			strncpy(tmpbuf, start, bytes_so_far);
			tmpbuf[bytes_so_far] = '\0';
			send_comms_packet(name, tmpbuf);
			strcpy(name, "-  ");
			start = &buffer[i];
			bytes_so_far = 0;
			last_space = 0;
		}
	}
	if (bytes_so_far > 0) {
		strcpy(tmpbuf, start);
		send_comms_packet(name, tmpbuf);
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
		double heading, uint32_t ship_id);
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

static int __attribute__((unused)) in_nebula(double x, double z)
{
	double dist2;
	int i, j;
	struct snis_entity *n;

	for (i = 0; i < NNEBULA; i++) {
		j = nebulalist[i];
		if (j < 0)
			continue;
		n = &go[j];
		/* FIXME make this 3d */
		dist2 = (x - n->x) * (x - n->x) + (z - n->z) * (z - n->z);
		if (dist2 < n->tsd.nebula.r * n->tsd.nebula.r)
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

static void ai_attack_mode_brain(struct snis_entity *o)
{
	struct snis_entity *v;
	double destx, desty, destz, dx, dy, dz, vdist;
	int n, firing_range;
	double maxv;

	n = o->tsd.ship.nai_entries - 1;
	assert(n >= 0);

	if (o->tsd.ship.ai[n].u.attack.victim_id == (uint32_t) -1) {
		int victim_id = find_nearest_victim(o);

		if (victim_id == -1) { /* no nearby victims */
			pop_ai_attack_mode(o);
			return;
		}
		o->tsd.ship.ai[n].u.attack.victim_id = victim_id;
		calculate_attack_vector(o, 400, 200);
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
	firing_range = (vdist <= LASER_RANGE);
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
			calculate_attack_vector(o, 400, 200); /* head back toward target */
		}
	}

	if (!firing_range)
		return;

	double range = dist3d(o->x - v->x, o->y - v->y, o->z - v->z);

	/* neutrals do not attack planets or starbases, and only select ships
	 * when attacked.
	 */
	if (o->sdata.faction != 0 ||
		(v->type != OBJTYPE_STARBASE && v->type == OBJTYPE_PLANET)) {

		if (snis_randn(1000) < 50 && range <= TORPEDO_RANGE &&
			o->tsd.ship.next_torpedo_time <= universe_timestamp &&
			o->tsd.ship.torpedoes > 0) {
			double dist, flight_time, tx, ty, tz, vx, vy, vz;
			/* int inside_nebula = in_nebula(o->x, o->y) || in_nebula(v->x, v->y); */

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
		} else {
			if (snis_randn(1000) < 150 &&
				o->tsd.ship.next_laser_time <= universe_timestamp) {
				o->tsd.ship.next_laser_time = universe_timestamp +
					ENEMY_LASER_FIRE_INTERVAL;
				add_laserbeam(o->id, v->id, LASERBEAM_DURATION);
				check_for_incoming_fire(v);
			}
		}
		if (v->type == OBJTYPE_SHIP1 && snis_randn(1000) < 25)
			taunt_player(o, v);
	} else {
		/* FIXME: give neutrals soemthing to do so they don't just sit there */;
	}
	check_for_nearby_targets(o);
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
	i = lookup_by_id(leader_id);
	if (i == o->id) { /* I'm the leader now */
		o->tsd.ship.ai[n].ai_mode = AI_MODE_FLEET_LEADER;
		return;
	}
	leader = &go[i];
	offset = fleet_position(f->fleet, position, &leader->orientation);
	o->tsd.ship.dox = offset.v.x + leader->x;
	o->tsd.ship.doy = offset.v.y + leader->y;
	o->tsd.ship.doz = offset.v.z + leader->z;
	o->tsd.ship.velocity = leader->tsd.ship.velocity * 1.05;

	dist2 = dist3dsqrd(o->x - o->tsd.ship.dox, o->y - o->tsd.ship.doy,
			o->z - o->tsd.ship.doz);

	if (dist2 > 75.0 * 75.0 && dist2 <= 1000.0 * 1000.0) { /* catch up if too far away */
		o->tsd.ship.velocity = leader->tsd.ship.velocity * 1.5;
	} else if (dist2 > 1000.0 * 1000.0) { /* If distance is too far, just warp */
		set_object_location(o, o->tsd.ship.dox, o->tsd.ship.doy, o->tsd.ship.doz);
		o->vx = leader->vx;
		o->vy = leader->vy;
		o->vz = leader->vz;
		o->orientation = leader->orientation;
	}
}

static void ai_patrol_mode_brain(struct snis_entity *o)
{
	int n = o->tsd.ship.nai_entries - 1;
	struct ai_patrol_data *patrol = &o->tsd.ship.ai[n].u.patrol;
	double maxv;
	int d;

	maxv = ship_type[o->tsd.ship.shiptype].max_speed;
	o->tsd.ship.desired_velocity = maxv;
	d = patrol->dest;

	double dist2 = dist3dsqrd(o->x - patrol->p[d].v.x,
				o->y - patrol->p[d].v.y,
				o->z - patrol->p[d].v.z);

	if (dist2 > 1000.0 * 1000.0) {
		double ld = dist3dsqrd(o->x - o->tsd.ship.dox,
				o->y - o->tsd.ship.doy, o->z - o->tsd.ship.doz);
		/* give ships some variety in movement */
		if (((universe_timestamp + o->id) & 0x3ff) == 0 || ld < 50.0 * 50.0) {
			union vec3 v, vn;

			v.v.x = patrol->p[d].v.x - o->x;
			v.v.y = patrol->p[d].v.y - o->y;
			v.v.z = patrol->p[d].v.z - o->z;
			vec3_normalize(&vn, &v);
			vec3_mul(&v, &vn, 300.0f);
			v.v.x += (float) snis_randn(100);
			v.v.y += (float) snis_randn(100);
			v.v.z += (float) snis_randn(100);

			o->tsd.ship.dox = v.v.x + o->x;
			o->tsd.ship.doy = v.v.y + o->y;
			o->tsd.ship.doz = v.v.z + o->z;
		}
	} else {
		o->tsd.ship.dox = patrol->p[d].v.x;
		o->tsd.ship.doy = patrol->p[d].v.y;
		o->tsd.ship.doz = patrol->p[d].v.z;
	}

	if (dist2 < 300.0 * 300.0) {
		patrol->dest = (patrol->dest + 1) % patrol->npoints;
		d = patrol->dest;
		o->tsd.ship.dox = patrol->p[d].v.x;
		o->tsd.ship.doy = patrol->p[d].v.y;
		o->tsd.ship.doz = patrol->p[d].v.z;
	}
	check_for_nearby_targets(o);
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
	case AI_MODE_PATROL:
		ai_patrol_mode_brain(o);
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
	default:
		break;
	}
}

struct collision_avoidance_context {
	struct snis_entity *o;
	double closest_dist2;
};

/* adjust o->tsd.ship.desired_velocity vector to avoid collisions */
static void ship_collision_avoidance(void *context, void *entity)
{
	struct collision_avoidance_context *ca = context;
	struct snis_entity *o = ca->o;
	struct snis_entity *obstacle = entity;
	double d;
	float steering_magnitude;
	const double worrythreshold = 100.0 * 100.0;

	/* hmm, server has no idea about meshes... */
	d = dist3dsqrd(o->x - obstacle->x, o->y - obstacle->y, o->z - obstacle->z);

	/* We only care about the closest one. */
	if (d > ca->closest_dist2 && ca->closest_dist2 > 0)
		return;

	ca->closest_dist2 = d;
	if (d < worrythreshold) { /* not close enough to worry about */
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
	steering_magnitude = 3000.0 / d;
	vec3_mul_self(&o->tsd.ship.steering_adjustment, steering_magnitude);
}

static void ship_move(struct snis_entity *o)
{
	int n;
	struct collision_avoidance_context ca;

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

	ai_brain(o);

	/* try to avoid collisions by computing steering and braking adjustments */
	ca.closest_dist2 = -1.0;
	ca.o = o;
	space_partition_process(space_partition, o, o->x, o->z, &ca, ship_collision_avoidance);

	/* Adjust velocity towards desired velocity */
	o->tsd.ship.velocity = o->tsd.ship.velocity +
			(o->tsd.ship.desired_velocity - o->tsd.ship.velocity) * 0.1;
	if (fabs(o->tsd.ship.velocity - o->tsd.ship.desired_velocity) < 1.0)
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
		if (i == o->index) /* skip self */
			continue;
		t = &d->o[i];
		switch (d->o[i].type) {
			case DAMCON_TYPE_ROBOT:
				break;
			case DAMCON_TYPE_WARPDRIVE:
			case DAMCON_TYPE_SENSORARRAY:
			case DAMCON_TYPE_COMMUNICATIONS:
			case DAMCON_TYPE_NAVIGATION:
			case DAMCON_TYPE_PHASERBANK:
			case DAMCON_TYPE_TORPEDOSYSTEM:
			case DAMCON_TYPE_SHIELDSYSTEM:
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
	damp_yaw_velocity(&o->tsd.robot.yaw_velocity, YAW_DAMPING);

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
		max_heading_change = (MAX_ROBOT_VELOCITY / fabs(o->velocity)) * 4.0 * M_PI / 180.0; 
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
			cargo = &d->o[i];
			cargo->x = clawx;
			cargo->y = clawy;
			cargo->heading = o->tsd.robot.desired_heading + M_PI;
			normalize_angle(&cargo->heading);
		}
	}


	if (fabs(lastx - o->x) > 0.05 ||
		fabs(lasty - o->y) > 0.05 ||
		fabs(lasth - o->heading) > 0.05) {
		o->timestamp = universe_timestamp;
		if (cargo)
			cargo->timestamp = universe_timestamp;
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
	o->tsd.ship.power_data.warp.i = device_power_byte_form(device);

	device = power_model_get_device(m, SENSORS_POWER_DEVICE);
	o->tsd.ship.power_data.sensors.i = device_power_byte_form(device);

	device = power_model_get_device(m, PHASERS_POWER_DEVICE);
	o->tsd.ship.power_data.phasers.i = device_power_byte_form(device);

	device = power_model_get_device(m, MANEUVERING_POWER_DEVICE);
	o->tsd.ship.power_data.maneuvering.i = device_power_byte_form(device);

	device = power_model_get_device(m, SHIELDS_POWER_DEVICE);
	o->tsd.ship.power_data.shields.i = device_power_byte_form(device);

	device = power_model_get_device(m, COMMS_POWER_DEVICE);
	o->tsd.ship.power_data.comms.i = device_power_byte_form(device);

	device = power_model_get_device(m, IMPULSE_POWER_DEVICE);
	o->tsd.ship.power_data.impulse.i = device_power_byte_form(device);

	device = power_model_get_device(m, TRACTOR_POWER_DEVICE);
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
		o->respawn_time = universe_timestamp + RESPAWN_TIME_SECS * 10;
		snis_queue_add_sound(EXPLOSION_SOUND,
				ROLE_SOUNDSERVER, o->id);
		schedule_callback(event_callback, &callback_schedule,
			"player-death-callback", o->id);
	}
}

static int calc_overheat_damage(uint8_t *system, uint8_t temperature)
{
	float damage, overheat_amount, new_value;

	if (temperature < (uint8_t) (0.9f * 255.0f))
		return 0; /* temp is ok, no damage */

	if (snis_randn(100) > 10) 
		return 0; /* lucky, no damage */

	overheat_amount = (temperature - (0.9f * 255.0f)) / (0.1f * 255.0f);

	damage = (float) snis_randn(5) / 100.0f;
	damage *= overheat_amount; 
	new_value = (float) *system + 255.0f * damage;
	if (new_value > 255.0f)
		new_value = 255.0f;
	*system = (uint8_t) new_value;
	return 1;
}

static int do_overheating_damage(struct snis_entity *o)
{
	int damage_was_done = 0;

	damage_was_done += calc_overheat_damage(&o->tsd.ship.damage.warp_damage,
			o->tsd.ship.temperature_data.warp_damage);
	damage_was_done += calc_overheat_damage(&o->tsd.ship.damage.sensors_damage,
			o->tsd.ship.temperature_data.sensors_damage);
	damage_was_done += calc_overheat_damage(&o->tsd.ship.damage.phaser_banks_damage,
			o->tsd.ship.temperature_data.phaser_banks_damage);
	damage_was_done += calc_overheat_damage(&o->tsd.ship.damage.maneuvering_damage,
			o->tsd.ship.temperature_data.maneuvering_damage);
	damage_was_done += calc_overheat_damage(&o->tsd.ship.damage.shield_damage,
			o->tsd.ship.temperature_data.shield_damage);
	damage_was_done += calc_overheat_damage(&o->tsd.ship.damage.comms_damage,
			o->tsd.ship.temperature_data.comms_damage);
	damage_was_done += calc_overheat_damage(&o->tsd.ship.damage.impulse_damage,
			o->tsd.ship.temperature_data.impulse_damage);
	damage_was_done += calc_overheat_damage(&o->tsd.ship.damage.tractor_damage,
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

struct auto_select_context {
	struct snis_entity *self;
	uint32_t new_victim_id;
	int found;
	double minrange2;
};

static void auto_select_enemy(void *context, void *entity)
{
	struct auto_select_context *c = context;
	struct snis_entity *t = entity;
	struct snis_entity *o = c->self;
	const double acceptable_angle = 8.0 * M_PI / 180.0;
	const double acceptable_range2 = (LASER_RANGE * LASER_RANGE);
	double angle, range2, a1;

	if (t->alive <= 0)
		return;

	if (t->id == o->id) /* avoid self selection */
		return;

	switch (t->type) {
	case OBJTYPE_SHIP1:
	case OBJTYPE_SHIP2:
	case OBJTYPE_ASTEROID:
	case OBJTYPE_DERELICT:
	case OBJTYPE_STARBASE: 
		range2 = hypot2(t->x - o->x, t->z - o->z);
		if (range2 > acceptable_range2)
			return;
		angle = atan2(-(t->z - o->z), t->x - o->x);
		if (angle < 0.0)
			angle += 2.0 * M_PI;
		else if (angle > 2 * M_PI)
			angle -= 2.0 * M_PI;
		a1 = o->tsd.ship.gun_heading;
		if (a1 < 0.0)
			a1 += 2.0 * M_PI;
		else if (a1 > 2.0 * M_PI)
			a1 -= 2.0 * M_PI;
		
		angle = (a1 > angle) * (a1 - angle) + (a1 <= angle) * (angle - a1);
		if (angle > acceptable_angle)
			return;
		if (!c->found || range2 < c->minrange2) {
			c->found = 1;
			c->minrange2 = range2;
			c->new_victim_id = t->id;
		}
		break;
	default:
		break;
	}
}

static void auto_select_enemies(struct snis_entity *o)
{
	struct auto_select_context asc;
	int n;

	if (o->tsd.ship.nai_entries == 0)
		ship_figure_out_what_to_do(o);
	n = o->tsd.ship.nai_entries - 1;

	if ((universe_timestamp & 0x3) != 0) /* throttle this computation */
		return;

	asc.self = o;
	asc.found = 0;
	asc.minrange2 = UNIVERSE_DIM * 10.0;
	asc.new_victim_id = -1;

	space_partition_process(space_partition, o, o->x, o->z, &asc, auto_select_enemy);
	o->tsd.ship.ai[n].u.attack.victim_id = asc.new_victim_id;
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

static void player_collision_detection(void *player, void *object)
{
	struct snis_entity *o, *t;
	double dist2;

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
	if (t->index == o->index) /* skip self */
		return;
	dist2 = dist3dsqrd(o->x - t->x, o->y - t->y, o->z - t->z);
	if (dist2 < PROXIMITY_DIST2 && (universe_timestamp & 0x7) == 0) {
		send_packet_to_all_clients_on_a_bridge(o->id, 
			packed_buffer_new("h", OPCODE_PROXIMITY_ALERT),
					ROLE_SOUNDSERVER | ROLE_NAVIGATION);
		if (dist2 < CRASH_DIST2) {
			send_packet_to_all_clients_on_a_bridge(o->id, 
				packed_buffer_new("h", OPCODE_COLLISION_NOTIFICATION),
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

static void enforce_minimum_attack_speed(double *vx, double *vy, double *vz)
{
	double v = dist3d(*vx, *vy, *vz);

	if (v < 0.000001) /* Avoid NaNs when close to stopped */
		return;

	if (v >= MINIMUM_ATTACK_SPEED)
		return;

	union vec3 veln, vel = { { (float) *vx, (float) *vy, (float) *vz } };
	vec3_normalize(&veln, &vel);
	vec3_mul(&vel, &veln, MINIMUM_ATTACK_SPEED);
	*vx = (double) vel.v.x;
	*vy = (double) vel.v.y;
	*vy = (double) vel.v.y;
}

static void update_ship_position_and_velocity(struct snis_entity *o)
{
	union vec3 desired_velocity;
	union vec3 dest, destn;
	struct snis_entity *v;
	int n;

	if (o->tsd.ship.nai_entries == 0)
		ship_figure_out_what_to_do(o);
	n = o->tsd.ship.nai_entries - 1;

	/* FIXME: need a better way to do this. */
	if (o->tsd.ship.ai[n].ai_mode == AI_MODE_ATTACK) {
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

	/* Make actual velocity move towards desired velocity */
	o->vx = new_velocity(desired_velocity.v.x, o->vx);
	o->vy = new_velocity(desired_velocity.v.y, o->vy);
	o->vz = new_velocity(desired_velocity.v.z, o->vz);

	enforce_minimum_attack_speed(&o->vx, &o->vy, &o->vz);

	/* Move ship */
	set_object_location(o, o->x + o->vx, o->y + o->vy, o->z + o->vz);
	space_partition_process(space_partition, o, o->x, o->z, o,
				player_collision_detection);
	/* FIXME: player_collision_detection probably wrong */
}

static void update_player_position_and_velocity(struct snis_entity *o)
{
	union vec3 desired_velocity;

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
	space_partition_process(space_partition, o, o->x, o->z, o,
				player_collision_detection);
}

static void player_move(struct snis_entity *o)
{
	int desired_rpm, desired_temp, diff;
	int phaser_chargerate, current_phaserbank;

	if (o->tsd.ship.damage.shield_damage > 200 && (universe_timestamp % (10 * 5)) == 0)
		snis_queue_add_sound(HULL_BREACH_IMMINENT, ROLE_SOUNDSERVER, o->id);
	if (o->tsd.ship.fuel < FUEL_CONSUMPTION_UNIT * 30 * 60 && (universe_timestamp % (20 * 5)) == 15)
		snis_queue_add_sound(FUEL_LEVELS_CRITICAL, ROLE_SOUNDSERVER, o->id);

	do_power_model_computations(o);
	do_coolant_model_computations(o);
	do_temperature_computations(o);
	o->tsd.ship.overheating_damage_done = do_overheating_damage(o);
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
	
	o->tsd.ship.gun_heading += o->tsd.ship.gun_yaw_velocity;
	o->tsd.ship.sci_heading += o->tsd.ship.sci_yaw_velocity;
	o->tsd.ship.shields = universe_timestamp % 100;

	normalize_angle(&o->tsd.ship.gun_heading);
	normalize_angle(&o->tsd.ship.sci_heading);
	o->timestamp = universe_timestamp;

	damp_yaw_velocity(&o->tsd.ship.yaw_velocity, YAW_DAMPING);
	damp_yaw_velocity(&o->tsd.ship.pitch_velocity, PITCH_DAMPING);
	damp_yaw_velocity(&o->tsd.ship.roll_velocity, ROLL_DAMPING);
	damp_yaw_velocity(&o->tsd.ship.gun_yaw_velocity, GUN_YAW_DAMPING);
	damp_yaw_velocity(&o->tsd.ship.sci_yaw_velocity, SCI_YAW_DAMPING);
	damp_yaw_velocity(&o->tsd.ship.sciball_yawvel, YAW_DAMPING);
	damp_yaw_velocity(&o->tsd.ship.sciball_pitchvel, PITCH_DAMPING);
	damp_yaw_velocity(&o->tsd.ship.sciball_rollvel, ROLL_DAMPING);
	damp_yaw_velocity(&o->tsd.ship.weap_yawvel, YAW_DAMPING);
	damp_yaw_velocity(&o->tsd.ship.weap_pitchvel, PITCH_DAMPING);

	/* Damp velocity */
	if (fabs(o->tsd.ship.velocity) < MIN_PLAYER_VELOCITY)
		o->tsd.ship.velocity = 0.0;
	else
		o->tsd.ship.velocity *= PLAYER_VELOCITY_DAMPING;

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
			rpmx, powery, ARRAY_SIZE(rpmx));
	desired_temp = (uint8_t) table_interp((double) o->tsd.ship.rpm,
			rpmx, tempy, ARRAY_SIZE(rpmx));
	desired_rpm = o->tsd.ship.throttle;
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
			rpmx, powertempy, ARRAY_SIZE(powertempy));

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
					packed_buffer_new("hh", OPCODE_WARP_LIMBO,
						(uint16_t) (5 * 30)), ROLE_ALL);
				set_object_location(o, bridgelist[b].warpx,
							bridgelist[b].warpy,
							bridgelist[b].warpz);
			}
		}
	}
	calculate_ship_scibeam_info(o);
	auto_select_enemies(o);
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
			military_alphabet[sectorz], military_numerals[sectorx]);
	else
		snprintf(buffer, buflen, "(%8.2lf, %8.2lf)", x, z);
}

static void nebula_move(struct snis_entity *o)
{
	return;
}

static void starbase_move(struct snis_entity *o)
{
	char buf[100], location[50];
	int64_t then, now;
	/* FIXME, fill this in. */

	then = o->tsd.starbase.last_time_called_for_help;
	now = universe_timestamp;
	if (o->tsd.starbase.under_attack &&
		(then < now - 2000 || 
		o->tsd.starbase.last_time_called_for_help == 0)) {
		o->tsd.starbase.last_time_called_for_help = universe_timestamp;
		// printf("starbase name = '%s'\n", o->tsd.starbase.name);
		sprintf(buf, "STARBASE %s:", o->sdata.name);
		send_comms_packet("", buf);
		send_comms_packet("-  ", starbase_comm_under_attack());
		coords_to_location_string(o->x, o->z, location, sizeof(location) - 1);
		sprintf(buf, "LOCATION %s", location);
		send_comms_packet("-  ", buf);
	}
}

static void explosion_move(struct snis_entity *o)
{
	if (o->alive > 0)
		o->alive--;

	if (o->alive <= 0)
		delete_from_clients_and_server(o);
}

static int add_generic_object(double x, double y, double z,
				double vx, double vy, double vz, double heading, int type)
{
	int i;
	char *n;
	union vec3 v;

	i = snis_object_pool_alloc_obj(pool); 	 
	if (i < 0)
		return -1;
	memset(&go[i], 0, sizeof(go[i]));
	go[i].id = get_new_object_id();
	go[i].index = i;
	go[i].alive = 1;
	set_object_location(&go[i], x, y, z);
	go[i].vx = vx;
	go[i].vy = vy;
	go[i].vz = vz;
	go[i].heading = heading;
	go[i].type = type;
	go[i].timestamp = universe_timestamp + 1;
	go[i].move = generic_move;

	switch (type) {
	case OBJTYPE_SHIP1:
	case OBJTYPE_SHIP2:
	case OBJTYPE_ASTEROID:
	case OBJTYPE_STARBASE:
	case OBJTYPE_DERELICT:
	case OBJTYPE_PLANET:
		n = random_name();
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


static void init_player(struct snis_entity *o)
{
	o->move = player_move;
	o->tsd.ship.torpedoes = INITIAL_TORPEDO_COUNT;
	o->tsd.ship.shields = 100.0;
	o->tsd.ship.power = 100.0;
	o->tsd.ship.yaw_velocity = 0.0;
	o->tsd.ship.pitch_velocity = 0.0;
	o->tsd.ship.roll_velocity = 0.0;
	o->tsd.ship.gun_yaw_velocity = 0.0;
	o->tsd.ship.gun_heading = M_PI / 2.0;
	o->tsd.ship.velocity = 0.0;
	o->tsd.ship.desired_velocity = 0.0;
	o->tsd.ship.desired_heading = 0.0;
	o->tsd.ship.sci_beam_width = MAX_SCI_BW_YAW_VELOCITY;
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
	quat_init_axis(&o->tsd.ship.sciball_orientation, 1, 0, 0, 0);
	quat_init_axis(&o->tsd.ship.weap_orientation, 1, 0, 0, 0);
	memset(&o->tsd.ship.damage, 0, sizeof(o->tsd.ship.damage));
	memset(&o->tsd.ship.temperature_data, 0, sizeof(o->tsd.ship.temperature_data));
	init_power_model(o);
	init_coolant_model(o);
}

static int add_player(double x, double z, double vx, double vz, double heading)
{
	int i;

	i = add_generic_object(x, 0.0, z, vx, 0.0, vz, heading, OBJTYPE_SHIP1);
	if (i < 0)
		return i;
	init_player(&go[i]);
	return i;
}

static void respawn_player(struct snis_entity *o)
{
	set_object_location(o, XKNOWN_DIM * (double) rand() / (double) RAND_MAX,
				o->y, ZKNOWN_DIM * (double) rand() / (double) RAND_MAX);
	o->vx = 0;
	o->vz = 0;
	o->heading = 3 * M_PI / 2;
	quat_init_axis(&o->orientation, 0, 1, 0, o->heading);
	init_player(o);
	o->alive = 1;
	send_ship_damage_packet(o);
}

static int add_ship(void)
{
	int i;
	double x, y, z, heading;

	x = ((double) snis_randn(1000)) * XKNOWN_DIM / 1000.0;
	y = (double) snis_randn(700) - 350;
	z = ((double) snis_randn(1000)) * ZKNOWN_DIM / 1000.0;
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
	go[i].tsd.ship.desired_heading = 0;
	go[i].tsd.ship.velocity = 0;
	go[i].tsd.ship.shiptype = snis_randn(nshiptypes);
	go[i].tsd.ship.nai_entries = 0;
	memset(go[i].tsd.ship.ai, 0, sizeof(go[i].tsd.ship.ai));
	go[i].tsd.ship.lifeform_count = ship_type[go[i].tsd.ship.shiptype].crew_max;
	memset(&go[i].tsd.ship.damage, 0, sizeof(go[i].tsd.ship.damage));
	memset(&go[i].tsd.ship.power_data, 0, sizeof(go[i].tsd.ship.power_data));
	go[i].tsd.ship.braking_factor = 1.0f;
	go[i].tsd.ship.steering_adjustment.v.x = 0.0;
	go[i].tsd.ship.steering_adjustment.v.y = 0.0;
	go[i].tsd.ship.steering_adjustment.v.z = 0.0;
	return i;
}

static int add_specific_ship(const char *name, double x, double y, double z,
			uint8_t shiptype, uint8_t the_faction)
{
	int i;

	i = add_ship();
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
	double x, y, z;
	double shiptype, the_faction;
	int i;

	name = lua_tostring(lua_state, 1);
	x = lua_tonumber(lua_state, 2);
	y = lua_tonumber(lua_state, 3);
	z = lua_tonumber(lua_state, 4);
	shiptype = lua_tonumber(lua_state, 5);
	the_faction = lua_tonumber(lua_state, 6);

	if (shiptype < 0 || shiptype > nshiptypes - 1) {
		lua_pushnumber(lua_state, -1.0);
		return 1;
	}

	pthread_mutex_lock(&universe_mutex);
	i = add_specific_ship(name, x, y, z,
		(uint8_t) shiptype % nshiptypes,
		(uint8_t) the_faction % nfactions());
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

static int l_add_random_ship(lua_State *l)
{
	int i;

	pthread_mutex_lock(&universe_mutex);
	i = add_ship();
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

static int add_asteroid(double x, double y, double z, double vx, double vz, double heading)
{
	int i;

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
	return i;
}

static int add_starbase(double x, double y, double z,
			double vx, double vz, double heading, int n)
{
	int i;

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
	go[i].sdata.shield_strength = 255;
	/* FIXME, why name stored twice? probably just use sdata.name is best
	 * but might be because we should know starbase name even if science
	 * doesn't scan it.
	 */
	sprintf(go[i].tsd.starbase.name, "SB-%02d", n);
	sprintf(go[i].sdata.name, "SB-%02d", n);
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
	i  = add_starbase(x, y, z, 0, 0, 0, n);
	lua_pushnumber(lua_state, i < 0 ? -1.0 : (double) go[i].id);
	pthread_mutex_unlock(&universe_mutex);
	return 1;
}

static int add_nebula(double x, double y, double z,
		double vx, double vz, double heading, double r)
{
	int i;

	i = add_generic_object(x, y, z, vx, 0.0, vz, heading, OBJTYPE_NEBULA);
	if (i < 0)
		return i;
	go[i].move = nebula_move;
	go[i].type = OBJTYPE_NEBULA;
	go[i].tsd.nebula.r = r;
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
		double heading, uint32_t ship_id)
{
	int i, s;

	i = add_generic_object(x, y, z, vx, vy, vz, heading, OBJTYPE_LASER);
	if (i < 0)
		return i;
	go[i].move = laser_move;
	go[i].alive = LASER_LIFETIME;
	go[i].tsd.laser.ship_id = ship_id;
	s = lookup_by_id(ship_id);
	go[i].tsd.laser.power = go[s].tsd.ship.phaser_charge;
	go[s].tsd.ship.phaser_charge = 0;
	go[i].tsd.laser.wavelength = go[s].tsd.ship.phaser_wavelength;
	if (orientation) {
		go[i].orientation = *orientation;
	} else {
		union vec3 from = { { 1.0, 0.0, 0.0 } };
		union vec3 to = { { vx, vy, vz } };
		quat_from_u2v(&go[i].orientation, &from, &to, NULL);
	}
	return i;
}

static void laserbeam_move(struct snis_entity *o)
{
	int tid, oid, ttype;
	struct snis_entity *target, *origin;

	o->alive--;
	if (o->alive <= 0) {
		delete_from_clients_and_server(o);
		return;
	}

	/* only deal laser damage every other tick */
	if (universe_timestamp & 0x01)
		return;

	tid = lookup_by_id(o->tsd.laserbeam.target);
	oid = lookup_by_id(o->tsd.laserbeam.origin);
	if (tid < 0 || oid < 0) {
		delete_from_clients_and_server(o);
		return;
	}

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
		calculate_laser_starbase_damage(target, o->tsd.laserbeam.wavelength);
	}

	if (ttype == OBJTYPE_SHIP1 || ttype == OBJTYPE_SHIP2) {
		calculate_laser_damage(target, o->tsd.laserbeam.wavelength,
					(float) o->tsd.laserbeam.power);
		send_ship_damage_packet(target);
		attack_your_attacker(target, lookup_entity_by_id(o->tsd.laserbeam.origin));
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
			delete_from_clients_and_server(target);
			respawn_object(ttype);
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
	int i, s;

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
	int i;
	double x, y, z;

	for (i = 0; i < NBASES; i++) {
		x = ((double) snis_randn(1000)) * XKNOWN_DIM / 1000.0;
		y = ((double) snis_randn(1000) - 500.0) * YKNOWN_DIM / 1000.0;
		z = ((double) snis_randn(1000)) * ZKNOWN_DIM / 1000.0;
		add_starbase(x, y, z, 0.0, 0.0, 0.0, i);
	}
}

static void add_nebulae(void)
{
	int i, j;
	double x, y, z, r;

	for (i = 0; i < NNEBULA; i++) {
		x = ((double) snis_randn(1000)) * XKNOWN_DIM / 1000.0;
		y = ((double) snis_randn(1000) - 500.0) * YKNOWN_DIM / 1000.0;
		z = ((double) snis_randn(1000)) * ZKNOWN_DIM / 1000.0;
		r = (double) snis_randn(NEBULA_RADIUS) +
				(double) MIN_NEBULA_RADIUS;
		j = add_nebula(x, y, z, 0.0, 0.0, 0.0, r);
		nebulalist[i] = j;
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
			int shiptype, int the_faction)
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
	go[i].vx = (float) snis_randn(100) / 400.0 * ship_type[0].max_speed;
	go[i].vz = (float) snis_randn(100) / 400.0 * ship_type[0].max_speed;
	return i;
}

static int l_add_derelict(lua_State *l)
{
	double x, y, z, shiptype, the_faction;
	const char *name;
	int i;

	name = lua_tostring(lua_state, 1);
	x = lua_tonumber(lua_state, 2);
	y = lua_tonumber(lua_state, 3);
	z = lua_tonumber(lua_state, 4);
	shiptype = lua_tonumber(lua_state, 5);
	the_faction = lua_tonumber(lua_state, 6);

	pthread_mutex_lock(&universe_mutex);
	i = add_derelict(name, x, y, z, shiptype, the_faction);
	lua_pushnumber(lua_state, i < 0 ? -1.0 : (double) go[i].id);
	pthread_mutex_unlock(&universe_mutex);
	return 1;
}


static void add_derelicts(void)
{
	int i;
	double x, y, z;

	for (i = 0; i < NDERELICTS; i++) {
		x = (double) snis_randn(1000) * XKNOWN_DIM / 1000.0;
		y = ((double) snis_randn(1000) - 500.0) * YKNOWN_DIM / 1000.0;
		z = (double) snis_randn(1000) * ZKNOWN_DIM / 1000.0;
		add_derelict(NULL, x, y, z, -1, -1);
	}
}

static int add_planet(double x, double y, double z)
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
	return i;
}

static int l_add_planet(lua_State *l)
{
	double x, y, z;
	const char *name;
	int i;

	name = lua_tostring(lua_state, 1);
	x = lua_tonumber(lua_state, 2);
	y = lua_tonumber(lua_state, 3);
	z = lua_tonumber(lua_state, 4);

	pthread_mutex_lock(&universe_mutex);
	i = add_planet(x, y, z);
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

static void add_planets(void)
{
	int i;
	double x, y, z, cx, cy, cz, a, r;

	for (i = 0; i < NPLANETS; i++) {
		cx = ((double) snis_randn(1000)) * XKNOWN_DIM / 1000.0;
		cz = ((double) snis_randn(1000)) * ZKNOWN_DIM / 1000.0;
		cy = ((double) snis_randn(1000) - 500.0) * YKNOWN_DIM / 1000.0;
		a = (double) snis_randn(360) * M_PI / 180.0;
		r = snis_randn(ASTEROID_CLUSTER_RADIUS);
		x = cx + r * sin(a);
		z = cz + r * cos(a);
		y = cy;
		add_planet(x, y, z);
	}
}

static int add_wormhole(double x1, double y1, double z1, double x2, double y2, double z2)
{
	int i;

	i = add_generic_object(x1, 0, z1, 0.0, 0.0, 0.0, 0.0, OBJTYPE_WORMHOLE);
	if (i < 0)
		return i;
	go[i].move = wormhole_move;
	go[i].tsd.wormhole.dest_x = x2;
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
	double x1, z1, x2, z2;

	for (i = 0; i < NWORMHOLE_PAIRS; i++) {
		do {
			x1 = ((double) snis_randn(1000)) * XKNOWN_DIM / 1000.0;
			z1 = ((double) snis_randn(1000)) * ZKNOWN_DIM / 1000.0;
			x2 = ((double) snis_randn(1000)) * XKNOWN_DIM / 1000.0;
			z2 = ((double) snis_randn(1000)) * ZKNOWN_DIM / 1000.0;
		} while (hypot(x1 - x2, z1 - z2) < XKNOWN_DIM / 2.0);
		add_wormhole_pair(&id1, &id2, x1, 0.0, z1, x2, 0.0, z2);
	}
}

static void add_eships(void)
{
	int i;

	for (i = 0; i < NESHIPS; i++)
		add_ship();
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

static void make_universe(void)
{
	pthread_mutex_lock(&universe_mutex);
	snis_object_pool_setup(&pool, MAXGAMEOBJS);

	add_nebulae(); /* do nebula first */
	add_starbases();
	add_asteroids();
	add_derelicts();
	add_planets();
	add_wormholes();
	add_eships();
	add_spacemonsters();
	pthread_mutex_unlock(&universe_mutex);
}

static int add_generic_damcon_object(struct damcon_data *d, int x, int y,
				uint32_t type, damcon_move_function move_fn)
{
	int i;
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
	o->timestamp = universe_timestamp;
	o->type = type; 
	o->move = move_fn;
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

	for (i = 0; i < PARTS_PER_DAMCON_SYSTEM; i++) {
		if (left_side) {
			px = x + dcxo[i] + 30;	
			py = y + dcyo[i];	
		} else {
			px = x - dcxo[i] + 210 + 30;	
			py = y + dcyo[i];	
		}
		p = add_generic_damcon_object(d, px, py, DAMCON_TYPE_SOCKET, NULL);
		d->o[p].timestamp = universe_timestamp + 1;
		d->o[p].tsd.socket.system = system;
		d->o[p].tsd.socket.part = i;
		socket = &d->o[p];

		p = add_generic_damcon_object(d, px, py, DAMCON_TYPE_PART, NULL);
		d->o[p].timestamp = universe_timestamp + 1;
		d->o[p].tsd.part.system = system;
		d->o[p].tsd.part.part = i;
		d->o[p].tsd.part.damage = 0;
		socket->tsd.socket.contents_id = d->o[p].id;
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
	dy = DAMCONYDIM / 5;
	y = dy - DAMCONYDIM / 2;
	i = add_generic_damcon_object(d, x, y, DAMCON_TYPE_WARPDRIVE, NULL);
	d->o[i].timestamp = universe_timestamp + 1;
	add_damcon_sockets(d, x, y, DAMCON_TYPE_WARPDRIVE, 1);
	y += dy;

	i = add_generic_damcon_object(d, x, y, DAMCON_TYPE_SENSORARRAY, NULL);
	add_damcon_sockets(d, x, y, DAMCON_TYPE_SENSORARRAY, 1);
	y += dy;
	d->o[i].timestamp = universe_timestamp + 1;
	i = add_generic_damcon_object(d, x, y, DAMCON_TYPE_COMMUNICATIONS, NULL);
	add_damcon_sockets(d, x, y, DAMCON_TYPE_COMMUNICATIONS, 1);
	x = 2 * DAMCONXDIM / 3 - DAMCONXDIM / 2;
	y = dy - DAMCONYDIM / 2;
	d->o[i].timestamp = universe_timestamp + 1;
	i = add_generic_damcon_object(d, x, y, DAMCON_TYPE_NAVIGATION, NULL);
	add_damcon_sockets(d, x, y, DAMCON_TYPE_NAVIGATION, 0);
	y += dy;
	d->o[i].timestamp = universe_timestamp + 1;
	i = add_generic_damcon_object(d, x, y, DAMCON_TYPE_PHASERBANK, NULL);
	add_damcon_sockets(d, x, y, DAMCON_TYPE_PHASERBANK, 0);
	y += dy;
	d->o[i].timestamp = universe_timestamp + 1;
	i = add_generic_damcon_object(d, x, y, DAMCON_TYPE_TORPEDOSYSTEM, NULL);
	add_damcon_sockets(d, x, y, DAMCON_TYPE_TORPEDOSYSTEM, 0);
	y += dy;
	x = -DAMCONXDIM / 2;
	d->o[i].timestamp = universe_timestamp + 1;
	i = add_generic_damcon_object(d, x, y, DAMCON_TYPE_SHIELDSYSTEM, NULL);
	add_damcon_sockets(d, x, y, DAMCON_TYPE_SHIELDSYSTEM, 1);
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

static void sleep_double(double time)
{
	struct timespec t, x;
	double intpart, fractpart;
	int rc;

	fractpart = modf(time, &intpart);
	t.tv_sec = intpart;
	t.tv_nsec = fractpart * 1000000000;

	do {
#if defined(__APPLE__) || defined(__FreeBSD__)
		rc = nanosleep(&t, &x);
#else
		rc = clock_nanosleep(CLOCK_MONOTONIC, 0, &t, &x);
#endif
		t.tv_sec = x.tv_sec;
		t.tv_nsec = x.tv_nsec;
	} while (rc == EINTR);
}

double time_now_double()
{
	struct timeval time;
	if (gettimeofday(&time,NULL)){
		return 0;
	}
	return (double)time.tv_sec + (double)time.tv_usec * .000001;
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
	set_object_location(o, o->x + dx, o->y, o->z + dz);
	o->timestamp = universe_timestamp + 10;
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
	double dist, dist2, angle, range, range2, A1, A2;
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
	/* distance to target... */
	dist2 = (o->x - ship->x) * (o->x - ship->x) +
		(o->z - ship->z) * (o->z - ship->z);
	if (dist2 > range2) /* too far, no sdata for you. */
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
	if (dr >= 5)
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
	if (save_sdata_bandwidth())
		return;
	if (!should_send_sdata(c, ship, o))
		return;
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
			packed_buffer_new("hb", OPCODE_ROLE_ONSCREEN, new_displaymode),
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
			packed_buffer_new("hb", OPCODE_SCI_DETAILS,
			new_details), ROLE_MAIN);
	return 0;
}

static int process_nav_details(struct game_client *c)
{
	unsigned char buffer[10];
	uint8_t new_details;
	int rc;

	rc = read_and_unpack_buffer(c, buffer, "b", &new_details);
	if (rc)
		return rc;
	/* just turn it around and fan it out to all the right places */
	send_packet_to_requestor_plus_role_on_a_bridge(c, 
			packed_buffer_new("hb", OPCODE_NAV_DETAILS,
			!!(new_details)), ROLE_MAIN);
	return 0;
}

static int process_weapons_manual(struct game_client *c)
{
	unsigned char buffer[10];
	uint8_t new_mode;
	int rc;

	rc = read_and_unpack_buffer(c, buffer, "b", &new_mode);
	if (rc)
		return rc;
	/* just turn it around and fan it out to all the right places */
	send_packet_to_requestor_plus_role_on_a_bridge(c, 
			packed_buffer_new("hb", OPCODE_WEAPONS_MANUAL,
			!!(new_mode)), ROLE_MAIN);
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
			packed_buffer_new("hw", OPCODE_SCI_SELECT_TARGET, id),
			ROLE_SCIENCE);
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
				packed_buffer_new("hww", OPCODE_SCI_SELECT_COORDS, x, z),
				ROLE_SCIENCE);
	return 0;
}

static struct snis_entity *nearest_starbase(struct snis_entity *o)
{
	double c, dist2 = -1.0;
	int i, answer = -1;

	for (i = 0; i <= snis_object_pool_highest_object(pool); i++) {
		if (go[i].type != OBJTYPE_STARBASE)
			continue;
		c = (o->x - go[i].x) * (o->x - go[i].x) +
			(o->z - go[i].z) * (o->z - go[i].z);
		if (dist2 < 0 || c < dist2) {
			dist2 = c;
			answer = i;
		}
	}
	if (answer < 0)
		return NULL;
	return &go[answer];
}

static void comm_dock_function(struct game_client *c, char *txt)
{
	struct snis_entity *o;
	struct snis_entity *sb;
	char msg[100];
	double dist;
	int i;

	printf("comm_dock_function called for %s:%s\n",
		bridgelist[c->bridge].shipname, txt);

	i = lookup_by_id(c->shipid);
	if (i < 0)
		return;

	o = &go[i];
	
	sb = nearest_starbase(o);
	if (!sb)
		return;
	dist = hypot(sb->x - o->x, sb->z - o->z);
	if (dist > STARBASE_DOCKING_DIST) {
		sprintf(msg, "%s, YOU ARE TOO FAR AWAY (%lf).\n",
			bridgelist[c->bridge].shipname, dist);
		send_comms_packet(sb->tsd.starbase.name, msg);
		return;
	}
	if (o->sdata.shield_strength > 15) {
		sprintf(msg, "%s, YOU MUST LOWER SHIELDS FIRST.\n",
			bridgelist[c->bridge].shipname);
		send_comms_packet(sb->tsd.starbase.name, msg);
		return;
	}
	sprintf(msg, "%s, PERMISSION TO DOCK GRANTED.",
		bridgelist[c->bridge].shipname);
	send_comms_packet(sb->tsd.starbase.name, msg);
	sprintf(msg, "%s, WELCOME TO OUR STARBASE, ENJOY YOUR STAY.",
		bridgelist[c->bridge].shipname);
	send_comms_packet(sb->tsd.starbase.name, msg);
	/* TODO make the repair/refuel process a bit less easy */
	sprintf(msg, "%s, YOUR SHIP HAS BEEN REPAIRED AND REFUELED.\n",
		bridgelist[c->bridge].shipname);
	init_player(o);
	send_ship_damage_packet(o);
	o->timestamp = universe_timestamp;
	send_comms_packet(sb->tsd.starbase.name, msg);
	schedule_callback2(event_callback, &callback_schedule,
			"player-docked-event", (double) c->shipid, sb->id);
	return;
}

struct comm_word_to_func {
	char *word;
	void (*fn)(struct game_client *c, char *txt);
};
 
static struct comm_word_to_func comm_verb[] = {
	{ "dock", comm_dock_function },
};

static int lookup_comm_verb(char *w)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(comm_verb); i++) {
		if (strcmp(comm_verb[i].word, w) == 0)
			return i;
	}
	return -1;
}

static void interpret_comms_packet(struct game_client *c, char *txt)
{
	char *w, *save_ptr = NULL;
	int verb;

	printf("Interpreting message from %s: %s\n",
		bridgelist[c->bridge].shipname, txt);

	w = strtok_r(txt, " ,.;:", &save_ptr);
	while (w != NULL) {
		verb = lookup_comm_verb(w);
		if (verb >= 0) {
			comm_verb[verb].fn(c, txt);
			break;
		}
		w = strtok_r(NULL, " ,.;:", &save_ptr);
	}
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
	send_comms_packet(name, txt);
	if (use_real_name)
		interpret_comms_packet(c, txt);
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

		if (p >= ARRAY_SIZE(o->tsd.ship.ai[n].u.patrol.p))
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

static int l_attack_ship(lua_State *l)
{
	int i;
	double attacker_id, victim_id;
	struct snis_entity *attacker;

	pthread_mutex_lock(&universe_mutex);
	attacker_id = lua_tonumber(lua_state, 1);
	victim_id = lua_tonumber(lua_state, 2);

	i = lookup_by_id(attacker_id);
	if (i < 0)
		goto error;
	attacker = &go[i];
		
	i = lookup_by_id(victim_id);
	if (i < 0)
		goto error;

	attacker->tsd.ship.cmd_data.command = DEMON_CMD_ATTACK;
	attacker->tsd.ship.cmd_data.x = 0;
	attacker->tsd.ship.cmd_data.z = 0;
	attacker->tsd.ship.cmd_data.nids1 = 0;
	attacker->tsd.ship.cmd_data.nids2 = 1;
	attacker->tsd.ship.cmd_data.id[0] = victim_id;	
	ship_choose_new_attack_victim(attacker);

	pthread_mutex_unlock(&universe_mutex);
	lua_pushnumber(lua_state, 0.0);
	return 1;
error:
	pthread_mutex_unlock(&universe_mutex);
	lua_pushnumber(lua_state, -1.0);
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
		send_comms_packet(transmitter->tsd.starbase.name, transmission);
		break;
	default:
		send_comms_packet(transmitter->sdata.name, transmission);
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

static int l_set_player_damage(lua_State *l)
{
	const double id = luaL_checknumber(l, 1);
	const char *system = luaL_checkstring(l, 2);
	const double value = luaL_checknumber(l, 3);
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
	if (value < 0 || value > 255)
		goto error;
	bvalue = (uint8_t) value;
	if (strncmp(system, "shield", 6) == 0) {
		o->tsd.ship.damage.shield_damage = bvalue;
		goto done;
	}
	if (strncmp(system, "impulse", 7) == 0) {
		o->tsd.ship.damage.impulse_damage = bvalue;
		goto done;
	}
	if (strncmp(system, "warp", 4) == 0) {
		o->tsd.ship.damage.warp_damage = bvalue;
		goto done;
	}
	if (strncmp(system, "maneuvering", 7) == 0) {
		o->tsd.ship.damage.maneuvering_damage = bvalue;
		goto done;
	}
	if (strncmp(system, "phaser", 6) == 0) {
		o->tsd.ship.damage.phaser_banks_damage = bvalue;
		goto done;
	}
	if (strncmp(system, "sensor", 6) == 0) {
		o->tsd.ship.damage.sensors_damage = bvalue;
		goto done;
	}
	if (strncmp(system, "comms", 5) == 0) {
		o->tsd.ship.damage.comms_damage = bvalue;
		goto done;
	}
	if (strncmp(system, "tractor", 7) == 0) {
		o->tsd.ship.damage.tractor_damage = bvalue;
		goto done;
	}
error:
	lua_pushnil(lua_state);
	return 1;
done:
	lua_pushnumber(lua_state, 0.0);
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
	packed_buffer_append(pb, "hb", OPCODE_LOAD_SKYBOX, (uint8_t) strlen(fileprefix) + 1);
	packed_buffer_append_raw(pb, fileprefix, strlen(fileprefix) + 1);
	send_packet_to_all_clients_on_a_bridge(o->id, pb, ROLE_MAIN);
	lua_pushnumber(lua_state, 0.0);
	return 1;
error:
	lua_pushnil(lua_state);
	return 1;
}

static int l_user_coords(lua_State *l)
{
	const double x = luaL_checknumber(l, 1);
	const double y = luaL_checknumber(l, 2);
	const double z = luaL_checknumber(l, 3);

	lua_pushnumber(lua_state, x);
	lua_pushnumber(lua_state, z);
	lua_pushnumber(lua_state, y);
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
	lua_pushnil(lua_state);
	return 1;
done:
	lua_pushnumber(lua_state, (double) bvalue);
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
		cargo->timestamp = universe_timestamp + 1;
		d->robot->timestamp= universe_timestamp + 1;

		/* find nearest socket... */
		found_socket = -1;
		for (i = 0; i <= snis_object_pool_highest_object(d->pool); i++) {
			if (d->o[i].type != DAMCON_TYPE_SOCKET)
				continue;
			dist2 = (cargo->x - d->o[i].x) * (cargo->x - d->o[i].x) + 
				(cargo->y - d->o[i].y) * (cargo->y - d->o[i].y);

			if (mindist < 0 || mindist > dist2) {
				mindist = dist2;
				if (dist2 < (ROBOT_MAX_GRIP_DIST2) &&
						d->o[i].tsd.socket.contents_id == DAMCON_SOCKET_EMPTY)
					found_socket = i;
			}
		}

		if (found_socket >= 0) {
			cargo->x = d->o[found_socket].x;
			cargo->y = d->o[found_socket].y;
			cargo->heading = 0;
			d->o[found_socket].tsd.socket.contents_id = cargo->id;
			d->o[found_socket].timestamp = universe_timestamp + 1;
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
	item->x = clawx;
	item->y = clawy;
	item->timestamp = universe_timestamp + 1;

	/* See if any socket thinks it has this item, if so, remove it. */
	for (i = 0; i <= snis_object_pool_highest_object(d->pool); i++) {
		socket = &d->o[i];
		if (socket->type != DAMCON_TYPE_SOCKET)
			continue;
		if (socket->tsd.socket.contents_id == item->id) {
			socket->tsd.socket.contents_id = DAMCON_SOCKET_EMPTY;
			socket->timestamp = universe_timestamp + 1;
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
					sizeof(uint16_t);
	unsigned char buffer[bufsize];
	double view_angle;
	uint8_t view_mode;

	rc = read_and_unpack_buffer(c, buffer, "Rb", &view_angle, &view_mode);
	if (rc)
		return rc;
	/* Rebuild packet and send to all clients with main screen role */
	send_packet_to_all_clients_on_a_bridge(c->shipid, 
			packed_buffer_new("hRb", OPCODE_MAINSCREEN_VIEW_MODE,
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
			packed_buffer_new("hb", OPCODE_REQUEST_REDALERT, new_alert_mode), ROLE_ALL);
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
			packed_buffer_new("hb", OPCODE_COMMS_MAINSCREEN,
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

	offset = offsetof(struct demon_cmd_packet, id) - sizeof(uint16_t);

	rc = snis_readsocket(c->socket, buffer,
		sizeof(struct demon_cmd_packet) - sizeof(uint32_t) - sizeof(uint16_t));
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

static int process_toggle_demon_safe_mode(void)
{
	safe_mode = !safe_mode;
	return 0;
}

static int l_clear_all(lua_State *l)
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
		i = add_ship();
		break;
	case OBJTYPE_STARBASE:
		i = add_starbase(x, 0, z, 0, 0, 0, snis_randn(100));
		break;
	case OBJTYPE_PLANET:
		i = add_planet(x, 0.0, z);
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
	if (i >= 0) {
		/* FIXME, the + 10 here is an awful hack to (mostly) get around
		 * the "too far away to care" bandwidth saving optimization in
		 * queue_up_client_updates().  Need a better, more sure way.
		 */
		go[i].timestamp = universe_timestamp + 10;
		go[i].x = x;
		go[i].z = z;
	}
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
	d->robot->timestamp = universe_timestamp + 1;
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

static void send_initiate_warp_packet(struct game_client *c)
{
	send_packet_to_all_clients_on_a_bridge(c->shipid,
			packed_buffer_new("h", OPCODE_INITIATE_WARP),
			ROLE_ALL);
}

static void send_wormhole_limbo_packet(int shipid, uint16_t value)
{
	send_packet_to_all_clients_on_a_bridge(shipid,
			packed_buffer_new("hh", OPCODE_WORMHOLE_LIMBO, value),
			ROLE_ALL);
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
	wfactor = ((double) o->tsd.ship.warpdrive / 255.0) * (XKNOWN_DIM / 2.0);
	quat_rot_vec(&warpvec, &rightvec, &o->orientation);
	bridgelist[b].warpx = o->x + wfactor * warpvec.v.x;
	bridgelist[b].warpy = o->y + wfactor * warpvec.v.y;
	bridgelist[b].warpz = o->z + wfactor * warpvec.v.z;
	o->tsd.ship.warp_time = 85; /* 8.5 seconds */
	pthread_mutex_unlock(&universe_mutex);
	send_initiate_warp_packet(c);
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
	union vec3 rightvec = { { 0.0f, 0.0f, 10.0f } };
	union vec3 offset;
	union vec3 velocity;
	union quat orientation;

	pthread_mutex_lock(&universe_mutex);

	/* Calculate which way weapons is pointed, and velocity of laser. */
	quat_mul(&orientation, &ship->orientation, &ship->tsd.ship.weap_orientation);
	quat_rot_vec(&velocity, &forwardvec, &orientation);
	quat_rot_vec(&offset, &rightvec, &orientation);

	/* Add ship velocity into laser velocity */
	velocity.v.x += ship->vx;
	velocity.v.y += ship->vy;
	velocity.v.z += ship->vz;

	add_laser(ship->x + offset.v.x, ship->y + offset.v.y, ship->z + offset.v.z,
			velocity.v.x, velocity.v.y, velocity.v.z,
			&orientation, 0.0, ship->id);
	add_laser(ship->x - offset.v.x, ship->y - offset.v.y, ship->z - offset.v.z,
			velocity.v.x, velocity.v.y, velocity.v.z,
			&orientation, 0.0, ship->id);
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




static int process_request_tractor_beam(struct game_client *c)
{
	struct snis_entity *tb, *ship = &go[c->ship_index];
	int tid, i;

	pthread_mutex_lock(&universe_mutex);
	if (ship->tsd.ship.ai[0].u.attack.victim_id < 0 && ship->tsd.ship.tractor_beam == -1)
		goto tractorfail;

	if (ship->tsd.ship.tractor_beam != -1) {
		/* turn off the tractor beam */
		i = lookup_by_id(ship->tsd.ship.tractor_beam);
		if (i < 0) {
			/* thing we were tractoring died. */
			ship->tsd.ship.tractor_beam = -1;
			pthread_mutex_unlock(&universe_mutex);
			return 0;
		}
		tb = &go[i];
		delete_from_clients_and_server(tb);
		ship->tsd.ship.tractor_beam = -1;
		pthread_mutex_unlock(&universe_mutex);
		return 0;
	}

	tid = ship->tsd.ship.ai[0].u.attack.victim_id;
	/* TODO: check tractor beam energy here */
	if (0) 
		goto tractorfail;

	add_tractorbeam(ship, tid, 30);
	/* TODO: tractor beam sound here. */
	pthread_mutex_unlock(&universe_mutex);
	return 0;

tractorfail:
	/* TODO: make special tractor failure sound */
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
	add_laser(o->x, 0.0, o->z, vx, 0.0, vz, NULL, o->heading, o->id);
out:
	pthread_mutex_unlock(&universe_mutex);

	return 0;
}

static void process_instructions_from_client(struct game_client *c)
{
	int rc;
	uint16_t opcode;
	static uint16_t last_successful_opcode = 0xffff;

	opcode = 0xffff;
	rc = snis_readsocket(c->socket, &opcode, sizeof(opcode));
	if (rc != 0)
		goto protocol_error;
	opcode = ntohs(opcode);

	switch (opcode) {
		case OPCODE_REQUEST_TORPEDO:
			process_request_torpedo(c);
			break;
		case OPCODE_REQUEST_TRACTORBEAM:
			process_request_tractor_beam(c);
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
		case OPCODE_NAV_DETAILS:
			rc = process_nav_details(c);
			if (rc)
				goto protocol_error;
			break;
		case OPCODE_WEAPONS_MANUAL:
			rc = process_weapons_manual(c);
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
		case OPCODE_ROBOT_AUTO_MANUAL:
			rc = process_robot_auto_manual(c);
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
	client_unlock();
	while (1) {
		process_instructions_from_client(c);
		if (c->socket < 0)
			break;
	}
	log_client_info(SNIS_INFO, c->socket, "client reader thread exiting\n");
	return NULL;
}

static void write_queued_updates_to_client(struct game_client *c)
{
	/* write queued updates to client */
	int rc;
	uint16_t noop = 0xffff;

	struct packed_buffer *buffer;

	/*  packed_buffer_queue_print(&c->client_write_queue); */
	buffer = packed_buffer_queue_combine(&c->client_write_queue, &c->client_write_queue_mutex);
	if (buffer->buffer_size > 0) {
		rc = snis_writesocket(c->socket, buffer->buffer, buffer->buffer_size);
		if (rc != 0) {
			snis_log(SNIS_ERROR, "writesocket failed, rc= %d, errno = %d(%s)\n", 
				rc, errno, strerror(errno));
			goto badclient;
		}
	} else {
		/* no-op, just so we know if client is still there */
		rc = snis_writesocket(c->socket, &noop, sizeof(noop));
		if (rc != 0) {
			snis_log(SNIS_ERROR, "(noop) writesocket failed, rc= %d, errno = %d(%s)\n", 
				rc, errno, strerror(errno));
			goto badclient;
		}
	}
	packed_buffer_free(buffer);
	return;

badclient:
	log_client_info(SNIS_WARN, c->socket, "disconnected, failed writing socket\n");
	shutdown(c->socket, SHUT_RDWR);
	close(c->socket);
	c->socket = -1;
}

static void send_update_ship_packet(struct game_client *c,
	struct snis_entity *o, uint16_t opcode);
static void send_econ_update_ship_packet(struct game_client *c,
	struct snis_entity *o);
static void send_update_asteroid_packet(struct game_client *c,
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
		send_update_sdata_packets(c, o);
		/* TODO: remove the next two lines when send_update_sdata_packets does it already */
		if (o == &go[c->ship_index])
			pack_and_send_ship_sdata_packet(c, o);
		if (!o->alive) {
			send_respawn_time(c, o);
			o->timestamp = universe_timestamp + 1;
		}
		send_update_power_model_data(c, o);
		send_update_coolant_model_data(c, o);
		if (o->tsd.ship.overheating_damage_done)
			send_overheat_ship_damage_packet(o); /* sends to all clients. */
		break;
	case OBJTYPE_SHIP2:
		send_econ_update_ship_packet(c, o);
		send_update_sdata_packets(c, o);
		break;
	case OBJTYPE_ASTEROID:
		send_update_asteroid_packet(c, o);
		send_update_sdata_packets(c, o);
		break;
	case OBJTYPE_DERELICT:
		send_update_derelict_packet(c, o);
		send_update_sdata_packets(c, o);
		break;
	case OBJTYPE_PLANET:
		send_update_planet_packet(c, o);
		send_update_sdata_packets(c, o);
		break;
	case OBJTYPE_WORMHOLE:
		send_update_wormhole_packet(c, o);
		send_update_sdata_packets(c, o);
		break;
	case OBJTYPE_STARBASE:
		send_update_starbase_packet(c, o);
		send_update_sdata_packets(c, o);
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
		send_update_sdata_packets(c, o);
		break;
	case OBJTYPE_LASER:
		send_update_laser_packet(c, o);
		send_update_sdata_packets(c, o);
		break;
	case OBJTYPE_SPACEMONSTER:
		send_update_spacemonster_packet(c, o);
		send_update_sdata_packets(c, o);
		break;
	case OBJTYPE_LASERBEAM:
		send_update_laserbeam_packet(c, o);
		break;
	case OBJTYPE_TRACTORBEAM:
		send_update_tractorbeam_packet(c, o);
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

	/* do not optimize updates for brand new clients, they need everything. */
	if (c->timestamp == 0)
		return 0;

	dx = (ship->x - o->x);
	dz = (ship->z - o->z);
	dist = (dx * dx) + (dz * dz);
	if (dist > threshold && snis_randn(100) < 70)
	 	return 1;	
	return 0;
}

static void queue_netstats(struct game_client *c)
{
	struct timeval now;
	uint32_t elapsed_seconds;

	if ((universe_timestamp & 0x0f) != 0x0f)
		return;
	gettimeofday(&now, NULL);
	elapsed_seconds = now.tv_sec - netstats.start.tv_sec;
	pb_queue_to_client(c, packed_buffer_new("hqqw", OPCODE_UPDATE_NETSTATS,
					netstats.bytes_sent, netstats.bytes_recd,
					elapsed_seconds));
}

static void queue_up_client_damcon_object_update(struct game_client *c,
			struct damcon_data *d, struct snis_damcon_entity *o)
{
	if (o->timestamp > c->timestamp) {
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
	}
}

static void queue_up_client_damcon_update(struct game_client *c)
{
	int i;
	struct damcon_data *d = &bridgelist[c->bridge].damcon;
	
	for (i = 0; i <= snis_object_pool_highest_object(d->pool); i++)
		queue_up_client_damcon_object_update(c, d, &d->o[i]);
}

static void queue_up_client_updates(struct game_client *c)
{
	int i;
	int count;

	count = 0;
	queue_netstats(c);
	pthread_mutex_lock(&universe_mutex);
	for (i = 0; i <= snis_object_pool_highest_object(pool); i++) {
		/* printf("obj %d: a=%d, ts=%u, uts%u, type=%hhu\n",
			i, go[i].alive, go[i].timestamp, universe_timestamp, go[i].type); */
		if (!go[i].alive && i != c->ship_index)
			continue;
		if (go[i].timestamp > c->timestamp ||
			i == c->ship_index ||
			snis_randn(100) < 15) {
			if (too_far_away_to_care(c, &go[i]))
				continue;
			queue_up_client_object_update(c, &go[i]);
			count++;
		}
	}
	queue_up_client_damcon_update(c);
	pthread_mutex_unlock(&universe_mutex);
	/* printf("queued up %d updates for client\n", count); */
}

static void queue_up_client_id(struct game_client *c)
{
	/* tell the client what his ship id is. */
	pb_queue_to_client(c, packed_buffer_new("hw", OPCODE_ID_CLIENT_SHIP, c->shipid));
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
	client_unlock();

	const double maxTimeBehind = 0.5;
	double delta = 1.0/10.0;

	double currentTime = time_now_double();
	double nextTime = currentTime + delta;
	while (1) {
		if (c->socket < 0)
			break;

		currentTime = time_now_double();

		if (currentTime - nextTime > maxTimeBehind)
			nextTime = currentTime;

		if (currentTime >= nextTime) {
			nextTime += delta;

			queue_up_client_updates(c);
			write_queued_updates_to_client(c);
			c->timestamp = universe_timestamp;
		} else {
			double timeToSleep = nextTime-currentTime;
			if (timeToSleep > 0)
				sleep_double(timeToSleep);
		}
		simulate_slow_server(0);
	}
	log_client_info(SNIS_INFO, c->socket, "client writer thread exiting.\n");
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

	pthread_mutex_lock(&universe_mutex);
	for (i = 0; i < nbridges; i++) {
		if (strcmp((const char *) shipname, (const char *) bridgelist[i].shipname) == 0 &&
			strcmp((const char *) password, (const char *) bridgelist[i].password) == 0) {
			pthread_mutex_unlock(&universe_mutex);
			return i;
		}
	}
	pthread_mutex_unlock(&universe_mutex);
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
	uint8_t ai[5];
	int i;
	uint16_t opcode = OPCODE_ECON_UPDATE_SHIP;

	if (c->debug_ai) {
		opcode = OPCODE_ECON_UPDATE_SHIP_DEBUG_AI; 
		memset(ai, 0, 5);
		for (i = 0; i < o->tsd.ship.nai_entries; i++) {
			switch (o->tsd.ship.ai[i].ai_mode) {
			case AI_MODE_IDLE:
				ai[i] = 'I';
				break;
			case AI_MODE_ATTACK:
				ai[i] = 'A';
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

	pb_queue_to_client(c, packed_buffer_new("hwwSSSSSSQwb", opcode,
			o->id, o->alive, o->x, (int32_t) UNIVERSE_DIM,
			o->y, (int32_t) UNIVERSE_DIM, o->z, (int32_t) UNIVERSE_DIM,
			o->vx, (uint32_t) UNIVERSE_DIM,
			o->vy, (uint32_t) UNIVERSE_DIM,
			o->vz, (uint32_t) UNIVERSE_DIM,
			&o->orientation, victim_id, o->tsd.ship.shiptype));
	if (c->debug_ai)
		pb_queue_to_client(c, packed_buffer_new("bbbbb",
				ai[0], ai[1], ai[2], ai[3], ai[4]));
}

static void send_ship_sdata_packet(struct game_client *c, struct ship_sdata_packet *sip)
{
	struct packed_buffer *pb;

	pb = packed_buffer_allocate(sizeof(struct ship_sdata_packet));
	packed_buffer_append(pb, "hwbbbbbbbr", OPCODE_SHIP_SDATA, sip->id, sip->subclass,
		sip->shield_strength, sip->shield_wavelength, sip->shield_width, sip->shield_depth,
		sip->faction, sip->lifeform_count,
		sip->name, (unsigned short) sizeof(sip->name));
	send_packet_to_all_clients_on_a_bridge(c->shipid, pb, ROLE_ALL);
}

static void send_generic_ship_damage_packet(struct snis_entity *o, uint16_t opcode)
{
	struct packed_buffer *pb;

	pb = packed_buffer_allocate(sizeof(struct ship_damage_packet));
	packed_buffer_append(pb, "hwr", opcode, o->id,
		(char *) &o->tsd.ship.damage, sizeof(o->tsd.ship.damage));
	send_packet_to_all_clients(pb, ROLE_ALL);
}

static void send_ship_damage_packet(struct snis_entity *o)
{
	send_generic_ship_damage_packet(o, OPCODE_UPDATE_DAMAGE);
}

static void send_overheat_ship_damage_packet(struct snis_entity *o)
{
	send_generic_ship_damage_packet(o, OPCODE_OVERHEAT_DAMAGE);
}

static void send_comms_packet(char *sender, const char *str)
{
	struct packed_buffer *pb;
	char tmpbuf[100];

	snprintf(tmpbuf, 99, "%s%s", sender, str);
	pb = packed_buffer_allocate(sizeof(struct comms_transmission_packet) + 100);
	packed_buffer_append(pb, "hb", OPCODE_COMMS_TRANSMISSION, (uint8_t) strlen(tmpbuf) + 1);
	packed_buffer_append_raw(pb, tmpbuf, strlen(tmpbuf) + 1);
	send_packet_to_all_clients(pb, ROLE_ALL);
}

static void send_respawn_time(struct game_client *c,
	struct snis_entity *o)
{
	uint8_t seconds = (o->respawn_time - universe_timestamp) / 10;

	pb_queue_to_client(c, packed_buffer_new("hb", OPCODE_UPDATE_RESPAWN_TIME, seconds));
}

static void send_update_power_model_data(struct game_client *c,
		struct snis_entity *o)
{
	struct packed_buffer *pb;

	pb = packed_buffer_allocate(sizeof(uint16_t) +
			sizeof(o->tsd.ship.power_data) + sizeof(uint32_t));
	packed_buffer_append(pb, "hwr", OPCODE_UPDATE_POWER_DATA, o->id,
		(char *) &o->tsd.ship.power_data, (unsigned short) sizeof(o->tsd.ship.power_data)); 
	pb_queue_to_client(c, pb);
}
	
static void send_update_coolant_model_data(struct game_client *c,
		struct snis_entity *o)
{
	struct packed_buffer *pb;

	pb = packed_buffer_allocate(sizeof(uint16_t) +
			sizeof(o->tsd.ship.coolant_data) + sizeof(uint32_t) +
			sizeof(o->tsd.ship.temperature_data));
	packed_buffer_append(pb, "hwr", OPCODE_UPDATE_COOLANT_DATA, o->id,
		(char *) &o->tsd.ship.coolant_data, (unsigned short) sizeof(o->tsd.ship.coolant_data));
	packed_buffer_append(pb, "r",
		(char *) &o->tsd.ship.temperature_data,
			(unsigned short) sizeof(o->tsd.ship.temperature_data)); 
	pb_queue_to_client(c, pb);
}

static void send_update_ship_packet(struct game_client *c,
	struct snis_entity *o, uint16_t opcode)
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
	packed_buffer_append(pb, "hwwSSSSSS", opcode, o->id, o->alive,
			o->x, (int32_t) UNIVERSE_DIM, o->y, (int32_t) UNIVERSE_DIM,
			o->z, (int32_t) UNIVERSE_DIM,
			o->vx, (int32_t) UNIVERSE_DIM,
			o->vy, (int32_t) UNIVERSE_DIM,
			o->vz, (int32_t) UNIVERSE_DIM);
	packed_buffer_append(pb, "RRRwwRRRRbbbwbbbbbbbbbbbbwQQQ",
			o->tsd.ship.yaw_velocity,
			o->tsd.ship.pitch_velocity,
			o->tsd.ship.roll_velocity,
			o->tsd.ship.torpedoes, o->tsd.ship.power,
			o->tsd.ship.gun_heading,
			o->tsd.ship.gun_yaw_velocity,
			o->tsd.ship.sci_heading,
			o->tsd.ship.sci_beam_width,
			tloading, throttle, rpm, fuel, o->tsd.ship.temp,
			o->tsd.ship.scizoom, o->tsd.ship.weapzoom, o->tsd.ship.navzoom,
			o->tsd.ship.mainzoom,
			o->tsd.ship.warpdrive, o->tsd.ship.requested_warpdrive,
			o->tsd.ship.requested_shield, o->tsd.ship.phaser_charge,
			o->tsd.ship.phaser_wavelength, o->tsd.ship.shiptype,
			o->tsd.ship.reverse, o->tsd.ship.ai[0].u.attack.victim_id,
			&o->orientation.vec[0],
			&o->tsd.ship.sciball_orientation.vec[0],
			&o->tsd.ship.weap_orientation.vec[0]);
	pb_queue_to_client(c, pb);
}

static void send_update_damcon_obj_packet(struct game_client *c,
		struct snis_damcon_entity *o)
{
	pb_queue_to_client(c, packed_buffer_new("hwwwSSSRb",
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
	pb_queue_to_client(c, packed_buffer_new("hwwwSSwbb",
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
	pb_queue_to_client(c, packed_buffer_new("hwwwSSRbbb",
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
	pb_queue_to_client(c, packed_buffer_new("hwSSSSSS", OPCODE_UPDATE_ASTEROID, o->id,
					o->x, (int32_t) UNIVERSE_DIM,
					o->y, (int32_t) UNIVERSE_DIM,
					o->z, (int32_t) UNIVERSE_DIM,
					o->vx, (int32_t) UNIVERSE_DIM,
					o->vy, (int32_t) UNIVERSE_DIM,
					o->vz, (int32_t) UNIVERSE_DIM));
}

static void send_update_derelict_packet(struct game_client *c,
	struct snis_entity *o)
{
	pb_queue_to_client(c, packed_buffer_new("hwSSSb", OPCODE_UPDATE_DERELICT, o->id,
					o->x, (int32_t) UNIVERSE_DIM,
					o->y, (int32_t) UNIVERSE_DIM,
					o->z, (int32_t) UNIVERSE_DIM,
					o->tsd.derelict.shiptype));
}


static void send_update_planet_packet(struct game_client *c,
	struct snis_entity *o)
{
	pb_queue_to_client(c, packed_buffer_new("hwSSS", OPCODE_UPDATE_PLANET, o->id,
					o->x, (int32_t) UNIVERSE_DIM,
					o->y, (int32_t) UNIVERSE_DIM,
					o->z, (int32_t) UNIVERSE_DIM));
}

static void send_update_wormhole_packet(struct game_client *c,
	struct snis_entity *o)
{
	pb_queue_to_client(c, packed_buffer_new("hwSS", OPCODE_UPDATE_WORMHOLE,
					o->id, o->x, (int32_t) UNIVERSE_DIM, o->z,
					(int32_t) UNIVERSE_DIM));
}

static void send_update_starbase_packet(struct game_client *c,
	struct snis_entity *o)
{
	pb_queue_to_client(c, packed_buffer_new("hwSSS", OPCODE_UPDATE_STARBASE,
					o->id, o->x, (int32_t) UNIVERSE_DIM,
					o->y, (int32_t) UNIVERSE_DIM,
					o->z, (int32_t) UNIVERSE_DIM));
}

static void send_update_nebula_packet(struct game_client *c,
	struct snis_entity *o)
{
	pb_queue_to_client(c, packed_buffer_new("hwSSS", OPCODE_UPDATE_NEBULA, o->id,
					o->x, (int32_t) UNIVERSE_DIM,
					o->z, (int32_t) UNIVERSE_DIM,
					o->tsd.nebula.r, (int32_t) UNIVERSE_DIM));
}

static void send_update_explosion_packet(struct game_client *c,
	struct snis_entity *o)
{
	pb_queue_to_client(c, packed_buffer_new("hwSSShhhb", OPCODE_UPDATE_EXPLOSION, o->id,
				o->x, (int32_t) UNIVERSE_DIM, o->y, (int32_t) UNIVERSE_DIM,
				o->z, (int32_t) UNIVERSE_DIM,
				o->tsd.explosion.nsparks, o->tsd.explosion.velocity,
				o->tsd.explosion.time, o->tsd.explosion.victim_type));
}

static void send_update_torpedo_packet(struct game_client *c,
	struct snis_entity *o)
{
	pb_queue_to_client(c, packed_buffer_new("hwwSSSSSS", OPCODE_UPDATE_TORPEDO, o->id,
					o->tsd.torpedo.ship_id,
					o->x, (int32_t) UNIVERSE_DIM,
					o->y, (int32_t) UNIVERSE_DIM,
					o->z, (int32_t) UNIVERSE_DIM,
					o->vx, (int32_t) UNIVERSE_DIM,
					o->vy, (int32_t) UNIVERSE_DIM,
					o->vz, (int32_t) UNIVERSE_DIM));
}

static void send_update_laser_packet(struct game_client *c,
	struct snis_entity *o)
{
	pb_queue_to_client(c, packed_buffer_new("hwwSSSSSSQ", OPCODE_UPDATE_LASER,
					o->id, o->tsd.laser.ship_id,
					o->x, (int32_t) UNIVERSE_DIM,
					o->y, (int32_t) UNIVERSE_DIM,
					o->z, (int32_t) UNIVERSE_DIM,
					o->vx, (int32_t) UNIVERSE_DIM,
					o->vy, (int32_t) UNIVERSE_DIM,
					o->vz, (int32_t) UNIVERSE_DIM,
					&o->orientation));
}

static void send_update_laserbeam_packet(struct game_client *c,
	struct snis_entity *o)
{
	pb_queue_to_client(c, packed_buffer_new("hwww", OPCODE_UPDATE_LASERBEAM,
					o->id, o->tsd.laserbeam.origin,
					o->tsd.laserbeam.target));
}

static void send_update_tractorbeam_packet(struct game_client *c,
	struct snis_entity *o)
{
	pb_queue_to_client(c, packed_buffer_new("hwww", OPCODE_UPDATE_TRACTORBEAM,
					o->id, o->tsd.laserbeam.origin,
					o->tsd.laserbeam.target));
}

static void send_update_spacemonster_packet(struct game_client *c,
	struct snis_entity *o)
{
	pb_queue_to_client(c, packed_buffer_new("hwSSS", OPCODE_UPDATE_SPACEMONSTER, o->id,
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
	app.opcode = ntohs(app.opcode);
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

		x = XKNOWN_DIM * (double) rand() / (double) RAND_MAX;
		z = ZKNOWN_DIM * (double) rand() / (double) RAND_MAX;
		pthread_mutex_lock(&universe_mutex);
		c->ship_index = add_player(x, z, 0.0, 0.0, M_PI / 2.0);
		c->shipid = go[c->ship_index].id;
		strcpy(go[c->ship_index].sdata.name, (const char * restrict) app.shipname);
		memset(&bridgelist[nbridges], 0, sizeof(bridgelist[nbridges]));
		strcpy((char *) bridgelist[nbridges].shipname, (const char *) app.shipname);
		strcpy((char *) bridgelist[nbridges].password, (const char *) app.password);
		bridgelist[nbridges].shipid = c->shipid;
		c->bridge = nbridges;
		populate_damcon_arena(&bridgelist[c->bridge].damcon);
	
		nbridges++;
		schedule_callback(event_callback, &callback_schedule,
				"player-respawn-event", (double) c->shipid);
		pthread_mutex_unlock(&universe_mutex);
	} else {
		c->shipid = bridgelist[c->bridge].shipid;
		c->ship_index = lookup_by_id(c->shipid);
	}
	c->debug_ai = 0;
	queue_up_client_id(c);
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

	client_lock();
	if (nclients >= MAXCLIENTS) {
		client_unlock();
		snis_log(SNIS_ERROR, "Too many clients.\n");
		return;
	}
	i = nclients;

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
	nclients++;
	client_count = 0;
	bridgenum = client[i].bridge;

	for (j = 0; j < nclients; j++) {
		if (client[j].bridge == bridgenum)
			client_count++;
	} 
	client_unlock();

	if (client_count == 1)
		snis_queue_add_global_sound(STARSHIP_JOINED);
	else
		snis_queue_add_sound(CREWMEMBER_JOINED, ROLE_ALL,
					bridgelist[bridgenum].shipid);

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

        snis_log(SNIS_INFO, "snis_server starting\n");

	/* Bind "rendezvous" socket to a random port to listen for connections. */
	while (1) {

		/* 
		 * choose a random port in the "Dynamic and/or Private" range
		 * see http://www.iana.org/assignments/port-numbers
		 */
		port = snis_randn(65335 - 49152) + 49151;
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

static void move_damcon_entities_on_bridge(int bridge_number)
{
	int i;
	struct damcon_data *d = &bridgelist[bridge_number].damcon;

	if (!d->pool)
		return;
	for (i = 0; i <= snis_object_pool_highest_object(d->pool); i++)
		if (d->o[i].move)
			d->o[i].move(&d->o[i], d);
}

static void move_damcon_entities(void)
{
	int i;

	for (i = 0; i < nbridges; i++)
		move_damcon_entities_on_bridge(i);
}

static void move_objects(void)
{
	int i;

	pthread_mutex_lock(&universe_mutex);
	universe_timestamp++;
	for (i = 0; i <= snis_object_pool_highest_object(pool); i++) {
		if (go[i].alive) {
			go[i].move(&go[i]);
		} else {
			if (go[i].type == OBJTYPE_SHIP1 &&
				universe_timestamp >= go[i].respawn_time) {
				respawn_player(&go[i]);
				schedule_callback(event_callback, &callback_schedule,
					"player-respawn-event", (double) go[i].id);
			}
		}
	}
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
	add_lua_callable_fn(l_add_starbase, "add_starbase");
	add_lua_callable_fn(l_add_planet, "add_planet");
	add_lua_callable_fn(l_add_nebula, "add_nebula");
	add_lua_callable_fn(l_add_spacemonster, "add_spacemonster");
	add_lua_callable_fn(l_add_derelict, "add_derelict");
	add_lua_callable_fn(l_add_wormhole_pair, "add_wormhole_pair");
	add_lua_callable_fn(l_get_player_ship_ids, "get_player_ship_ids");
	add_lua_callable_fn(l_get_object_location, "get_object_location");
	add_lua_callable_fn(l_move_object, "move_object");
	add_lua_callable_fn(l_attack_ship, "attack_ship");
	add_lua_callable_fn(l_register_callback, "register_callback");
	add_lua_callable_fn(l_register_timer_callback, "register_timer_callback");
	add_lua_callable_fn(l_comms_transmission, "comms_transmission");
	add_lua_callable_fn(l_get_object_name, "get_object_name");
	add_lua_callable_fn(l_get_player_damage, "get_player_damage");
	add_lua_callable_fn(l_set_player_damage, "set_player_damage");
	add_lua_callable_fn(l_load_skybox, "load_skybox");
	add_lua_callable_fn(l_user_coords, "user_coords");
	add_lua_callable_fn(l_ai_push_patrol, "ai_push_patrol");
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

int main(int argc, char *argv[])
{
	int port, rc, i;
	struct timespec thirtieth_second;

	/* need this so that fscanf can read floats properly */
#define LOCALE_THAT_WORKS "en_US.UTF-8"
	if (!setenv("LANG", LOCALE_THAT_WORKS, 1))
		fprintf(stderr, "Failed to setenv LANG to '%s'\n",
				LOCALE_THAT_WORKS);
	if (argc < 5) 
		usage();

	override_asset_dir();

	if (read_ship_types()) {
		fprintf(stderr, "%s: unable to read ship types\n", argv[0]);
		return -1;
	}

	if (read_factions())
		return -1;

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
	register_with_game_lobby(argv[1], port, argv[2], argv[1], argv[3]);

	const double maxTimeBehind = 0.5;
	double delta = 1.0/10.0;

	i = 0;
	double currentTime = time_now_double();
	double nextTime = currentTime + delta;
	while (1) {
		currentTime = time_now_double();

		if (currentTime - nextTime > maxTimeBehind)
			nextTime = currentTime;

		if (currentTime >= nextTime) {
			nextTime += delta;

			/* if ((i % 30) == 0) printf("Moving objects...i = %d\n", i); */
			i++;
			move_objects();
			process_lua_commands();
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
