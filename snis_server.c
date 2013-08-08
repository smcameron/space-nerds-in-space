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
#include <linux/tcp.h>
#include <stddef.h>
#include <stdarg.h>
#include <limits.h>

#include "ssgl/ssgl.h"
#include "snis.h"
#include "snis_log.h"
#include "mathutils.h"
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
	double warpx, warpy;
} bridgelist[MAXCLIENTS];
int nbridges = 0;
static pthread_mutex_t universe_mutex = PTHREAD_MUTEX_INITIALIZER;

static pthread_mutex_t listener_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t listener_started;
int listener_port = -1;
pthread_t lobbythread;
char *lobbyserver = NULL;
static int snis_log_level = 2;

static int nebulalist[NNEBULA] = { 0 };

static inline void client_lock()
{
        (void) pthread_mutex_lock(&client_mutex);
}

static inline void client_unlock()
{
        (void) pthread_mutex_unlock(&client_mutex);
}


int nframes = 0;
int timer = 0;
struct timeval start_time, end_time;

static struct snis_object_pool *pool;
static struct snis_entity go[MAXGAMEOBJS];
static uint32_t universe_timestamp = 1;

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
	double angle;

	angle = (universe_timestamp * M_PI / 180) / o->tsd.asteroid.r +
			o->tsd.asteroid.angle_offset;
	angle = angle * ASTEROID_SPEED;

	o->x = o->tsd.asteroid.r * sin(angle) + XKNOWN_DIM / 2.0;
	o->y = o->tsd.asteroid.r * cos(angle) + YKNOWN_DIM / 2.0;
	o->timestamp = universe_timestamp;
}

static void send_wormhole_limbo_packet(int shipid, uint16_t value);
static void wormhole_move(struct snis_entity *o)
{
	int i;
	double dist2;
	double a, r;

	for (i = 0; i <= snis_object_pool_highest_object(pool); i++) {
		struct snis_entity *t;
		t = &go[i];

		if (t == o)
			continue;

		switch (t->type) {
		case OBJTYPE_ASTEROID:
			/* because of functional movement of asteroids. *sigh* */
		case OBJTYPE_WORMHOLE:
			continue;
		default:
			dist2 = ((t->x - o->x) * (t->x - o->x)) +
				((t->y - o->y) * (t->y - o->y));
			if (dist2 < 30.0 * 30.0) {
				a = snis_randn(360) * M_PI / 180.0;
				r = 60.0;
				t->x = o->tsd.wormhole.dest_x + cos(a) * r; 
				t->y = o->tsd.wormhole.dest_y + sin(a) * r; 
				t->timestamp = universe_timestamp;
				if (t->type == OBJTYPE_SHIP1)
					send_wormhole_limbo_packet(t->id, 5 * 30);
			}
		}
	}
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

static int add_explosion(double x, double y, uint16_t velocity,
				uint16_t nsparks, uint16_t time, uint8_t victim_type);

static void normalize_coords(struct snis_entity *o)
{
	/* This algorihm is like that big ball in "The Prisoner". */

	if (o->x < -UNIVERSE_LIMIT)
		goto fixit;
	if (o->x > UNIVERSE_LIMIT)
		goto fixit;
	if (o->y < -UNIVERSE_LIMIT)
		goto fixit;
	if (o->y > UNIVERSE_LIMIT)
		goto fixit;
	return;
fixit:
	o->x = snis_randn(XKNOWN_DIM);
	o->y = snis_randn(YKNOWN_DIM);
}

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
	const double twp = TORPEDO_WEAPONS_FACTOR;

	ss = shield_strength(snis_randn(255), o->sdata.shield_strength,
				o->sdata.shield_width,
				o->sdata.shield_depth,
				o->sdata.shield_wavelength);

	o->tsd.ship.damage.shield_damage = roll_damage(twp, ss, o->tsd.ship.damage.shield_damage);
	o->tsd.ship.damage.impulse_damage = roll_damage(twp, ss, o->tsd.ship.damage.impulse_damage);
	o->tsd.ship.damage.warp_damage = roll_damage(twp, ss, o->tsd.ship.damage.warp_damage);
	o->tsd.ship.damage.torpedo_tubes_damage = roll_damage(twp, ss, o->tsd.ship.damage.torpedo_tubes_damage);
	o->tsd.ship.damage.phaser_banks_damage = roll_damage(twp, ss, o->tsd.ship.damage.phaser_banks_damage);
	o->tsd.ship.damage.sensors_damage = roll_damage(twp, ss, o->tsd.ship.damage.sensors_damage);
	o->tsd.ship.damage.comms_damage = roll_damage(twp, ss, o->tsd.ship.damage.comms_damage);

	if (o->tsd.ship.damage.shield_damage == 255) { 
		o->respawn_time = universe_timestamp + 30 * 10;
		o->alive = 0;
	}
}

static void calculate_laser_damage(struct snis_entity *o, uint8_t wavelength)
{
	int damage, i;
	unsigned char *x = (unsigned char *) &o->tsd.ship.damage;
	double ss;

	ss = shield_strength(wavelength, o->sdata.shield_strength,
				o->sdata.shield_width,
				o->sdata.shield_depth,
				o->sdata.shield_wavelength);

	for (i = 0; i < sizeof(o->tsd.ship.damage); i++) {
		damage = (int) x[i] + ((double) snis_randn(40) * (1 - ss));
		if (damage > 255)
			damage = 255;
		x[i] = damage;
	}
	if (o->tsd.ship.damage.shield_damage == 255) {
		o->respawn_time = universe_timestamp + 30 * 10;
		o->alive = 0;
	}
}

static void delete_object(struct snis_entity *o)
{
	snis_object_pool_free_object(pool, o->index);
	o->id = -1;
	o->alive = 0;
}

static void send_ship_damage_packet(struct snis_entity *o);
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

static void attack_your_attacker(struct snis_entity *attackee, struct snis_entity *attacker)
{
	if (!attacker)
		return;

	if (attackee->type != OBJTYPE_SHIP2)
		return;

	if (snis_randn(100) >= 75)
		return;

	attackee->tsd.ship.victim_id = attacker->id;
}

static void torpedo_move(struct snis_entity *o)
{
	int i, otype;

	o->x += o->vx;
	o->y += o->vy;
	normalize_coords(o);
	o->timestamp = universe_timestamp;
	o->alive--;
	for (i = 0; i <= snis_object_pool_highest_object(pool); i++) {
		double dist2;
		otype = go[i].type;

		if (!go[i].alive)
			continue;
		if (i == o->index)
			continue;
		if (o->alive >= TORPEDO_LIFETIME - 3)
			continue;
		if (otype != OBJTYPE_SHIP1 && otype != OBJTYPE_SHIP2 &&
				otype != OBJTYPE_STARBASE &&
				otype != OBJTYPE_ASTEROID)
			continue;
		if (go[i].id == o->tsd.torpedo.ship_id)
			continue; /* can't torpedo yourself. */
		dist2 = ((go[i].x - o->x) * (go[i].x - o->x)) +
			((go[i].y - o->y) * (go[i].y - o->y));

		if (dist2 > TORPEDO_DETONATE_DIST2)
			continue; /* not close enough */

		
		/* hit!!!! */
		o->alive = 0;

		if (otype == OBJTYPE_STARBASE) {
			go[i].tsd.starbase.under_attack = 1;
			continue;
		}

		if (otype == OBJTYPE_SHIP1 || otype == OBJTYPE_SHIP2) {
			calculate_torpedo_damage(&go[i]);
			send_ship_damage_packet(&go[i]);
			attack_your_attacker(&go[i], lookup_entity_by_id(o->tsd.torpedo.ship_id));
		} else if (otype == OBJTYPE_ASTEROID && fabs(go[i].z) < 100.0) {
			go[i].alive = 0;
		}

		if (!go[i].alive) {
			(void) add_explosion(go[i].x, go[i].y, 50, 50, 50, go[i].type);
			/* TODO -- these should be different sounds */
			/* make sound for players that got hit */
			/* make sound for players that did the hitting */
			snis_queue_add_sound(EXPLOSION_SOUND, ROLE_SOUNDSERVER, o->tsd.torpedo.ship_id);
			if (otype != OBJTYPE_SHIP1) {
				snis_queue_delete_object(&go[i]);
				delete_object(&go[i]);
				respawn_object(otype);
			} else {
				snis_queue_add_sound(EXPLOSION_SOUND,
					ROLE_SOUNDSERVER, go[i].id);
			}
		} else {
			(void) add_explosion(go[i].x, go[i].y, 50, 5, 5, go[i].type);
			snis_queue_add_sound(DISTANT_TORPEDO_HIT_SOUND, ROLE_SOUNDSERVER, go[i].id);
			snis_queue_add_sound(TORPEDO_HIT_SOUND, ROLE_SOUNDSERVER, o->tsd.torpedo.ship_id);
		}
		continue;
	}

	if (o->alive <= 0) {
		snis_queue_delete_object(o);
		delete_object(o);
	}
}

static double point_to_line_dist(double lx1, double ly1, double lx2, double ly2, double px, double py)
{
	double normal_length = hypot(lx1 - lx2, ly1 - ly2);
	return fabs((px - lx1) * (ly2 - ly1) - (py - ly1) * (lx2 - lx1)) / normal_length;
}

static int laser_point_collides(double lx1, double ly1, double lx2, double ly2, double px, double py)
{
	if (px < lx1 && px < lx2)
		return 0;
	if (px > lx1 && px > lx2)
		return 0;
	if (py < ly1 && py < ly2)
		return 0;
	if (py > ly1 && py > ly2)
		return 0;
	return (point_to_line_dist(lx1, ly1, lx2, ly2, px, py) < 350.0);
}

static void laser_move(struct snis_entity *o)
{
	int i;
	uint8_t otype;

	o->x += o->vx;
	o->y += o->vy;
	normalize_coords(o);
	o->timestamp = universe_timestamp;
	o->alive--;
	for (i = 0; i <= snis_object_pool_highest_object(pool); i++) {
		/* double dist2; */

		if (!go[i].alive)
			continue;
		if (i == o->index)
			continue;
		otype = go[i].type;
		if (otype != OBJTYPE_SHIP1 && otype != OBJTYPE_SHIP2 &&
			otype != OBJTYPE_STARBASE && otype != OBJTYPE_ASTEROID)
			continue;
		if (go[i].id == o->tsd.laser.ship_id)
			continue; /* can't laser yourself. */
		/* dist2 = ((go[i].x - o->x) * (go[i].x - o->x)) +
			((go[i].y - o->y) * (go[i].y - o->y)); */

		if (!laser_point_collides(o->x + o->vx, o->y + o->vy, o->x - o->vx, o->y - o->vy,
						go[i].x, go[i].y))
		/* if (dist2 > LASER_DETONATE_DIST2) */
			continue; /* not close enough */

		
		/* hit!!!! */
		o->alive = 0;

		if (otype == OBJTYPE_STARBASE) {
			go[i].tsd.starbase.under_attack = 1;
			continue;
		}

		if (otype == OBJTYPE_SHIP1 || otype == OBJTYPE_SHIP2) {
			calculate_laser_damage(&go[i], o->tsd.laser.wavelength);
			send_ship_damage_packet(&go[i]);
			attack_your_attacker(&go[i], lookup_entity_by_id(o->tsd.laser.ship_id));
		}

		if (otype == OBJTYPE_ASTEROID && fabs(go[i].z) < 100.0) {
			go[i].alive = 0;
		}

		if (!go[i].alive) {
			(void) add_explosion(go[i].x, go[i].y, 50, 50, 50, otype);
			/* TODO -- these should be different sounds */
			/* make sound for players that got hit */
			/* make sound for players that did the hitting */
			snis_queue_add_sound(EXPLOSION_SOUND,
					ROLE_SOUNDSERVER, o->tsd.laser.ship_id);
			if (go[i].type != OBJTYPE_SHIP1) {
				snis_queue_delete_object(&go[i]);
				delete_object(&go[i]);
				respawn_object(otype);
			} else {
				snis_queue_add_sound(EXPLOSION_SOUND,
							ROLE_SOUNDSERVER, go[i].id);
			}
		} else {
			(void) add_explosion(go[i].x, go[i].y, 50, 5, 5, otype);
			snis_queue_add_sound(DISTANT_PHASER_HIT_SOUND, ROLE_SOUNDSERVER, go[i].id);
			snis_queue_add_sound(PHASER_HIT_SOUND, ROLE_SOUNDSERVER, o->tsd.torpedo.ship_id);
		}
		continue;
	}

	if (o->alive <= 0) {
		snis_queue_delete_object(o);
		delete_object(o);
	}
}

static int find_nearest_victim(struct snis_entity *o)
{
	int i, victim_id;
	double dist2, dx, dy, lowestdist;

	/* assume universe mutex is held */
	victim_id = -1;
	lowestdist = 1e60;  /* very big number */
	for (i = 0; i <= snis_object_pool_highest_object(pool); i++) {

		if (i == o->index) /* don't victimize self */
			continue;

		if (o->sdata.faction == 0) { /* neutral */
			/* only travel to planets and starbases */
			if (go[i].type != OBJTYPE_STARBASE && go[i].type != OBJTYPE_PLANET)
				continue;
		} else {
			/* only victimize players, other ships and starbases */
			if (go[i].type != OBJTYPE_STARBASE && go[i].type != OBJTYPE_SHIP1 &&
				go[i].type != OBJTYPE_SHIP2)
				continue;
		}

		if (!go[i].alive) /* skip the dead guys */
			continue;

		if (o->sdata.faction != 0) {
			if (go[i].type == OBJTYPE_SHIP2 || go[i].type == OBJTYPE_STARBASE) {
				/* don't attack neutrals */
				if (go[i].sdata.faction == 0)
					continue;
				/* Even factions attack odd factions, and vice versa */
				/* TODO: something better here. */
				if ((o->sdata.faction % 2) == (go[i].sdata.faction % 2))
					continue;
			}
		}

		dx = go[i].x - o->x;
		dy = go[i].y - o->y;
		dist2 = dx * dx + dy * dy;
		if (go[i].type == OBJTYPE_SHIP1 && o->sdata.faction != 0)
			dist2 = dist2 * 0.5; /* prioritize hitting player... */
		if (victim_id == -1 || dist2 < lowestdist) {
			victim_id = go[i].id;
			lowestdist = dist2;
		}
	}
	return victim_id;
}

static void send_comms_packet(char *sender, char *str);
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

static int add_torpedo(double x, double y, double vx, double vy, double heading, uint32_t ship_id);
static int add_laser(double x, double y, double vx, double vy, double heading, uint32_t ship_id);
static uint8_t update_phaser_banks(int current, int max);

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

		dist2 = hypot(o->x - e->x, o->y - e->y); 
		if (d < 0 || d > dist2) {
			d = dist2;
			eid = go[index].id;
		}
	}
		
	if (eid >= 0) {
		int a, r;

		a = snis_randn(360);
		r = snis_randn(LASER_RANGE - 400) + 400;
		o->tsd.ship.victim_id = eid;
		o->tsd.ship.dox = r * cos(a * M_PI / 180.0);
		o->tsd.ship.doy = r * sin(a * M_PI / 180.0);
	} else {
		o->tsd.ship.victim_id = -1;
	}
}

static void spacemonster_move(struct snis_entity *o)
{

	/* FIXME: make this better */
	o->vx = cos(o->heading) * 10.0;
	o->vy = sin(o->heading) * 10.0;
	o->tsd.spacemonster.zz =
		sin(M_PI *((universe_timestamp * 10 + o->id) % 360) / 180.0) * 35.0;
	o->x += o->vx;
	o->y += o->vy;
	if (snis_randn(1000) < 50) {
		o->heading += (snis_randn(40) - 20) * M_PI / 180.0;
		normalize_angle(&o->heading);
	}
	o->timestamp = universe_timestamp;
}

static int in_nebula(double x, double y)
{
	double dist2;
	int i, j;
	struct snis_entity *n;

	for (i = 0; i < NNEBULA; i++) {
		j = nebulalist[i];
		if (j < 0)
			continue;
		n = &go[j];
		dist2 = (x - n->x) * (x - n->x) + (y - n->y) * (y - n->y);
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

static int add_laserbeam(uint32_t origin, uint32_t target, int alive);
static void ship_move(struct snis_entity *o)
{
	double heading_diff, yaw_vel;
	struct snis_entity *v;
	double destx, desty, dx, dy, d;
	int close_enough = 0;
	double maxv;

	switch (o->tsd.ship.cmd_data.command) {
	case DEMON_CMD_ATTACK:
		if (o->tsd.ship.victim_id == (uint32_t) -1 || snis_randn(1000) < 50) {
			int a, r;

			a = snis_randn(360);
			r = snis_randn(LASER_RANGE - 400) + 400;
			ship_choose_new_attack_victim(o);
			v = lookup_entity_by_id(o->tsd.ship.victim_id);
			o->tsd.ship.dox = r * cos(a * M_PI / 180.0);
			o->tsd.ship.doy = r * sin(a * M_PI / 180.0);
		}
		break;
	default:
		if (o->tsd.ship.victim_id == (uint32_t) -1 ||
			snis_randn(1000) < 50) {
			int a, r;
			a = snis_randn(360);
			r = snis_randn(LASER_RANGE - 400) + 400;
			o->tsd.ship.victim_id = find_nearest_victim(o);
			o->tsd.ship.dox = r * cos(a * M_PI / 180.0);
			o->tsd.ship.doy = r * sin(a * M_PI / 180.0);
		}
		break;
	}

	maxv = max_speed[o->tsd.ship.shiptype];
	v = lookup_entity_by_id(o->tsd.ship.victim_id);
	if (v) {
		destx = v->x + o->tsd.ship.dox;
		desty = v->y + o->tsd.ship.doy;
		dx = destx - o->x;
		dy = desty - o->y;
		d = sqrt(dx * dx + dy * dy);
		o->tsd.ship.desired_heading = atan2(dy, dx);
		o->tsd.ship.desired_velocity = (d / maxv) * maxv + snis_randn(5);
		if (o->tsd.ship.desired_velocity > maxv)
			o->tsd.ship.desired_velocity = maxv;
		if (fabs(dx) < 1000 && fabs(dy) < 1000) {
			o->tsd.ship.desired_velocity = 0;
			dx = v->x - o->x;
			dy = v->y - o->y;
			o->tsd.ship.desired_heading = atan2(dy, dx);
			close_enough = 1;
		}
		if (snis_randn(1000) < 20) {
			int dist;
			double angle;

			angle = snis_randn(360) * M_PI / 180.0;
			dist = snis_randn(LASER_RANGE - 400) + 400;
			o->tsd.ship.dox = cos(angle) * dist; 
			o->tsd.ship.doy = sin(angle) * dist; 
		}
	}
#if 0
	/* Decide where to go... */
	if (snis_randn(100) < 5) {
		dx = v
		o->tsd.ship.desired_heading = degrees_to_radians(0.0 + snis_randn(360)); 
		o->tsd.ship.desired_velocity = snis_randn(20);
	}
#endif

	/* Adjust heading towards desired heading */
	heading_diff = o->tsd.ship.desired_heading - o->heading;
	if (heading_diff < -M_PI)
		heading_diff += 2.0 * M_PI;
	if (heading_diff > M_PI)
		heading_diff -= 2.0 * M_PI;
	if (heading_diff > MAX_YAW_VELOCITY * 0.5) {
		yaw_vel = MAX_YAW_VELOCITY * 0.5 ;
	} else {
		if (heading_diff < -MAX_YAW_VELOCITY * 0.5) {
			yaw_vel = -MAX_YAW_VELOCITY * 0.5;
		} else {
			yaw_vel = heading_diff * 0.2;
		}
	}
	if (fabs(heading_diff) < (M_PI / 180.0))
		yaw_vel = heading_diff;
	o->heading += yaw_vel;
	normalize_angle(&o->heading);

	/* Adjust velocity towards desired velocity */
	o->tsd.ship.velocity = o->tsd.ship.velocity +
			(o->tsd.ship.desired_velocity - o->tsd.ship.velocity) * 0.1;
	if (fabs(o->tsd.ship.velocity - o->tsd.ship.desired_velocity) < 1.0)
		o->tsd.ship.velocity = o->tsd.ship.desired_velocity;

	/* set vx, vy, move x, y */
	o->vy = o->tsd.ship.velocity * sin(o->heading);
	o->vx = o->tsd.ship.velocity * cos(o->heading);
	o->x += o->vx;
	o->y += o->vy;
	normalize_coords(o);
	o->timestamp = universe_timestamp;

	if (close_enough && o->tsd.ship.victim_id != (uint32_t) -1) {
		double range;
		v = lookup_entity_by_id(o->tsd.ship.victim_id);

		range = hypot(o->x - v->x, o->y - v->y);

		/* neutrals do not attack planets or starbases, and only select ships
		 * when attacked.
		 */
		if (o->sdata.faction != 0 || 
			(v->type != OBJTYPE_STARBASE && v->type == OBJTYPE_PLANET)) {

			if (snis_randn(1000) < 50 && range <= TORPEDO_RANGE &&
				o->tsd.ship.next_torpedo_time <= universe_timestamp &&
				o->tsd.ship.torpedoes > 0) {
				double dist, flight_time, tx, ty, vx, vy, angle;
				int inside_nebula = in_nebula(o->x, o->y) || in_nebula(v->x, v->y);

				dist = hypot(v->x - o->x, v->y - o->y);
				flight_time = dist / TORPEDO_VELOCITY;
				tx = v->x + (v->vx * flight_time);
				ty = v->y + (v->vy * flight_time);

				angle = atan2(tx - o->x, ty - o->y);
				if (inside_nebula)
					angle += (M_PI / 180.0 / 25.0) * (snis_randn(100) - 50);
				else
					angle += (M_PI / 180.0 / 5.0) * (snis_randn(100) - 50);
				vx = TORPEDO_VELOCITY * sin(angle);
				vy = TORPEDO_VELOCITY * cos(angle);
				add_torpedo(o->x, o->y, vx, vy, o->heading, o->id);
				o->tsd.ship.torpedoes--;
				o->tsd.ship.next_torpedo_time = universe_timestamp +
					ENEMY_TORPEDO_FIRE_INTERVAL;
				check_for_incoming_fire(v);
			} else { 
				if (snis_randn(1000) < 150 &&
					o->tsd.ship.next_laser_time <= universe_timestamp) {
					o->tsd.ship.next_laser_time = universe_timestamp +
						ENEMY_LASER_FIRE_INTERVAL;
					add_laserbeam(o->id, v->id, 30);
					check_for_incoming_fire(v);
				}
			}
			if (v->type == OBJTYPE_SHIP1 && snis_randn(1000) < 25)
				taunt_player(o, v);
		}
	}
	o->tsd.ship.phaser_charge = update_phaser_banks(o->tsd.ship.phaser_charge, 255);
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

static uint8_t update_phaser_banks(int current, int max)
{
	double delta;
	if (current == max)
		return (uint8_t) current;

	delta = (max - current) / 10.0;
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

	o->tsd.ship.power_data.voltage = (unsigned char)
		(255.0 * power_model_actual_voltage(m) / power_model_nominal_voltage(m));
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
	double range, tx, ty, angle, A1, A2;

	/* calculate the range of the science scope based on power. */
	range = (MAX_SCIENCE_SCREEN_RADIUS - MIN_SCIENCE_SCREEN_RADIUS) *
		(ship->tsd.ship.scizoom / 255.0) + MIN_SCIENCE_SCREEN_RADIUS;
	ship->tsd.ship.scibeam_range = range;

	tx = sin(ship->tsd.ship.sci_heading) * range;
	ty = -cos(ship->tsd.ship.sci_heading) * range;

	angle = atan2(ty, tx);
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

static void auto_select_enemies(struct snis_entity *o)
{
	uint32_t new_victim_id = -1;
	const double acceptable_angle = 8.0 * M_PI / 180.0;
	const double acceptable_range2 = (LASER_RANGE * LASER_RANGE);
	double angle, range2, a1, minrange2 = UNIVERSE_DIM * 10.0;
	int i, found;

	if ((universe_timestamp & 0x3) != 0) /* throttle this computation */
		return;

	found = 0;
	for (i = 0; i <= snis_object_pool_highest_object(pool); i++) {
		struct snis_entity *t = &go[i];

		if (t->alive <= 0)
			continue;

		if (t->id == o->id) /* avoid self selection */
			continue;

		switch (t->type) {
		case OBJTYPE_SHIP1:
		case OBJTYPE_SHIP2:
		case OBJTYPE_ASTEROID:
		case OBJTYPE_STARBASE: 
			range2 = hypot2(t->x - o->x, t->y - o->y);
			if (range2 > acceptable_range2)
				continue;
			angle = atan2(t->y - o->y, t->x - o->x);
			angle += M_PI / 2.0;
			if (angle < 0.0)
				angle += 2.0 * M_PI;
			else if (angle > 2 * M_PI)
				angle -= 2.0 * M_PI;
			a1 = o->tsd.ship.gun_heading;
			if (a1 < 0.0)
				a1 += 2.0 * M_PI;
		
			angle = (a1 > angle) * (a1 - angle) + (a1 <= angle) * (angle - a1);
			if (angle > acceptable_angle)
				continue;
			if (!found || range2 < minrange2) {
				found = 1;
				minrange2 = range2;
				new_victim_id = t->id;
			}
			break;
		default:
			break;
		}
	}
	o->tsd.ship.victim_id = new_victim_id;
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

static void player_move(struct snis_entity *o)
{
	int desired_rpm, desired_temp, diff;
	int max_phaserbank, current_phaserbank;

	if (o->tsd.ship.damage.shield_damage > 200 && (universe_timestamp % (10 * 5)) == 0)
		snis_queue_add_sound(HULL_BREACH_IMMINENT, ROLE_SOUNDSERVER, o->id);
	if (o->tsd.ship.fuel < FUEL_CONSUMPTION_UNIT * 30 * 60 && (universe_timestamp % (20 * 5)) == 15)
		snis_queue_add_sound(FUEL_LEVELS_CRITICAL, ROLE_SOUNDSERVER, o->id);

	do_power_model_computations(o);
	if (o->tsd.ship.fuel > FUEL_CONSUMPTION_UNIT) {
		power_model_enable(o->tsd.ship.power_model);
		o->tsd.ship.fuel -= (int) (FUEL_CONSUMPTION_UNIT *
			power_model_total_current(o->tsd.ship.power_model) / MAX_CURRENT);
	} else {
		power_model_disable(o->tsd.ship.power_model);
	}
	do_thrust(o);
	o->vy = o->tsd.ship.velocity * -cos(o->heading);
	o->vx = o->tsd.ship.velocity * sin(o->heading);
	o->x += o->vx;
	o->y += o->vy;
	normalize_coords(o);
	o->heading += o->tsd.ship.yaw_velocity;
	o->tsd.ship.gun_heading += o->tsd.ship.gun_yaw_velocity;
	o->tsd.ship.sci_heading += o->tsd.ship.sci_yaw_velocity;
	o->tsd.ship.shields = universe_timestamp % 100;

	normalize_angle(&o->heading);
	normalize_angle(&o->tsd.ship.gun_heading);
	normalize_angle(&o->tsd.ship.sci_heading);
	o->timestamp = universe_timestamp;

	damp_yaw_velocity(&o->tsd.ship.yaw_velocity, YAW_DAMPING);
	damp_yaw_velocity(&o->tsd.ship.gun_yaw_velocity, GUN_YAW_DAMPING);
	damp_yaw_velocity(&o->tsd.ship.sci_yaw_velocity, SCI_YAW_DAMPING);

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
	max_phaserbank = o->tsd.ship.power_data.phasers.i;
	o->tsd.ship.phaser_charge = update_phaser_banks(current_phaserbank, max_phaserbank);

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
				o->x = bridgelist[b].warpx;
				o->y = bridgelist[b].warpy;
				normalize_coords(o);
			}
		}
	}
	calculate_ship_scibeam_info(o);
	auto_select_enemies(o);
}

static void demon_ship_move(struct snis_entity *o)
{
	o->vy = o->tsd.ship.velocity * sin(o->heading);
	o->vx = o->tsd.ship.velocity * cos(o->heading);
	o->x -= o->vx;
	o->y -= o->vy;
	normalize_coords(o);
	o->heading += o->tsd.ship.yaw_velocity;

	normalize_angle(&o->heading);
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

static void coords_to_location_string(double x, double y, char *buffer, int buflen)
{
	int sectorx, sectory;
	static char *military_alphabet[] = {
		"ALPHA", "BRAVO", "CHARLIE", "DELTA", "ECHO",
		"FOXTROT", "GOLF", "HOTEL", "INDIA", "JULIETT",
	};
	static char *military_numerals[] = {
		"ZERO ZERO", "ZERO ONE", "ZERO TWO", "ZERO THREE",
		"ZERO FOUR", "ZERO FIVE",
		"ZERO SIX", "ZERO SEVEN", "ZERO EIGHT", "ZERO NINER", "ONE ZERO", };

	sectorx = floor((x / (double) XKNOWN_DIM) * 10.0);
	sectory = floor((y / (double) YKNOWN_DIM) * 10.0);

	if (sectorx >= 0 && sectorx <= 9 && sectory >= 0 && sectory <= 10)
		snprintf(buffer, buflen, "SECTOR %s %s",
			military_alphabet[sectory], military_numerals[sectorx]);
	else
		snprintf(buffer, buflen, "(%8.2lf, %8.2lf)", x, y);
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
		coords_to_location_string(o->x, o->y, location, sizeof(location) - 1);
		sprintf(buf, "LOCATION %s", location);
		send_comms_packet("-  ", buf);
	}
}

static void explosion_move(struct snis_entity *o)
{
	if (o->alive > 0)
		o->alive--;

	if (o->alive <= 0) {
		snis_queue_delete_object(o);
		delete_object(o);
	}
}

static int add_generic_object(double x, double y, double vx, double vy, double heading, int type)
{
	int i;
	char *n;

	i = snis_object_pool_alloc_obj(pool); 	 
	if (i < 0)
		return -1;
	n = random_name();
	memset(&go[i], 0, sizeof(go[i]));
	go[i].id = get_new_object_id();
	go[i].index = i;
	go[i].alive = 1;
	go[i].x = x;
	go[i].y = y;
	go[i].z = 0.0;
	go[i].vx = vx;
	go[i].vy = vy;
	go[i].heading = heading;
	go[i].type = type;
	go[i].timestamp = universe_timestamp + 1;
	go[i].move = generic_move;
	strncpy(go[i].sdata.name, n, sizeof(go[i].sdata.name) - 1);
	go[i].sdata.shield_strength = snis_randn(256);
	go[i].sdata.shield_wavelength = snis_randn(256);
	go[i].sdata.shield_width = snis_randn(128);
	go[i].sdata.shield_depth = snis_randn(255);
	go[i].sdata.faction = snis_randn(ARRAY_SIZE(faction));
	free(n);
	return i;
}

#define DECLARE_POWER_MODEL_SAMPLER(name, which) \
static float sample_##name##_##which(void *cookie) \
{ \
	struct snis_entity *o = cookie; \
	float v = 255.0 - (float) o->tsd.ship.power_data.name.which; \
\
	if (v > 250.0) \
		v = 10000.0; \
	v =  v * 10000.0; \
	return v; \
	/* return (float) 255.0 / (float) (o->tsd.ship.power_data.name.which + 1.0); */ \
	/* return (float) (256.0 - (float) o->tsd.ship.power_data.name.which) / 256.0; */  \
}

DECLARE_POWER_MODEL_SAMPLER(warp, r1) /* declares sample_warp_r1 */
DECLARE_POWER_MODEL_SAMPLER(warp, r2) /* declares sample_warp_r2 */
DECLARE_POWER_MODEL_SAMPLER(warp, r3) /* declares sample_warp_r3 */
DECLARE_POWER_MODEL_SAMPLER(sensors, r1) /* declares sample_sensors_r1 */
DECLARE_POWER_MODEL_SAMPLER(sensors, r2) /* declares sample_sensors_r2 */
DECLARE_POWER_MODEL_SAMPLER(sensors, r3) /* declares sample_sensors_r3 */
DECLARE_POWER_MODEL_SAMPLER(phasers, r1) /* declares sample_phasers_r1 */
DECLARE_POWER_MODEL_SAMPLER(phasers, r2) /* declares sample_phasers_r2 */
DECLARE_POWER_MODEL_SAMPLER(phasers, r3) /* declares sample_phasers_r3 */
DECLARE_POWER_MODEL_SAMPLER(maneuvering, r1) /* declares sample_maneuvering_r1 */
DECLARE_POWER_MODEL_SAMPLER(maneuvering, r2) /* declares sample_maneuvering_r2 */
DECLARE_POWER_MODEL_SAMPLER(maneuvering, r3) /* declares sample_maneuvering_r3 */
DECLARE_POWER_MODEL_SAMPLER(shields, r1) /* declares sample_shields_r1 */
DECLARE_POWER_MODEL_SAMPLER(shields, r2) /* declares sample_shields_r2 */
DECLARE_POWER_MODEL_SAMPLER(shields, r3) /* declares sample_shields_r3 */
DECLARE_POWER_MODEL_SAMPLER(comms, r1) /* declares sample_comms_r1 */
DECLARE_POWER_MODEL_SAMPLER(comms, r2) /* declares sample_comms_r2 */
DECLARE_POWER_MODEL_SAMPLER(comms, r3) /* declares sample_comms_r3 */
DECLARE_POWER_MODEL_SAMPLER(impulse, r1) /* declares sample_impulse_r1 */
DECLARE_POWER_MODEL_SAMPLER(impulse, r2) /* declares sample_impulse_r2 */
DECLARE_POWER_MODEL_SAMPLER(impulse, r3) /* declares sample_impulse_r3 */

static void init_power_model(struct snis_entity *o)
{
	struct power_model *pm;
	struct power_device *d;

/*
	if (o->tsd.ship.power_model)
		free_power_model(o->tsd.ship.power_model);
*/
	memset(&o->tsd.ship.power_data, 0, sizeof(o->tsd.ship.power_data));

	pm = new_power_model(MAX_CURRENT, MAX_VOLTAGE, INTERNAL_RESIST);
	o->tsd.ship.power_model = pm; 

	/* Warp */
	o->tsd.ship.power_data.warp.r1 = 0;
	o->tsd.ship.power_data.warp.r2 = 0;
	o->tsd.ship.power_data.warp.r3 = 200;
	d = new_power_device(o, sample_warp_r1, sample_warp_r2, sample_warp_r3);
	power_model_add_device(pm, d);

	/* Sensors */
	o->tsd.ship.power_data.sensors.r1 = 255;
	o->tsd.ship.power_data.sensors.r2 = 0;
	o->tsd.ship.power_data.sensors.r3 = 200;
	d = new_power_device(o, sample_sensors_r1, sample_sensors_r2, sample_sensors_r3);
	power_model_add_device(pm, d);

	/* Phasers */
	o->tsd.ship.power_data.phasers.r1 = 255;
	o->tsd.ship.power_data.phasers.r2 = 0;
	o->tsd.ship.power_data.phasers.r3 = 200;
	d = new_power_device(o, sample_phasers_r1, sample_phasers_r2, sample_phasers_r3);
	power_model_add_device(pm, d);

	/* Maneuvering */
	o->tsd.ship.power_data.maneuvering.r1 = 255;
	o->tsd.ship.power_data.maneuvering.r2 = 0;
	o->tsd.ship.power_data.maneuvering.r3 = 200;
	d = new_power_device(o, sample_maneuvering_r1, sample_maneuvering_r2, sample_maneuvering_r3);
	power_model_add_device(pm, d);

	/* Shields */
	o->tsd.ship.power_data.shields.r1 = 255;
	o->tsd.ship.power_data.shields.r2 = 0;
	o->tsd.ship.power_data.shields.r3 = 200;
	d = new_power_device(o, sample_shields_r1, sample_shields_r2, sample_shields_r3);
	power_model_add_device(pm, d);

	/* Comms */
	o->tsd.ship.power_data.comms.r1 = 255;
	o->tsd.ship.power_data.comms.r2 = 0;
	o->tsd.ship.power_data.comms.r3 = 200;
	d = new_power_device(o, sample_comms_r1, sample_comms_r2, sample_comms_r3);
	power_model_add_device(pm, d);

	/* Impulse */
	o->tsd.ship.power_data.impulse.r1 = 0; //255;
	o->tsd.ship.power_data.impulse.r2 = 0;
	o->tsd.ship.power_data.impulse.r3 = 200;
	d = new_power_device(o, sample_impulse_r1, sample_impulse_r2, sample_impulse_r3);
	power_model_add_device(pm, d);
}

static void init_player(struct snis_entity *o)
{
	o->move = player_move;
	o->tsd.ship.torpedoes = INITIAL_TORPEDO_COUNT;
	o->tsd.ship.shields = 100.0;
	o->tsd.ship.power = 100.0;
	o->tsd.ship.yaw_velocity = 0.0;
	o->tsd.ship.gun_yaw_velocity = 0.0;
	o->tsd.ship.gun_heading = 0.0;
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
	o->tsd.ship.torpedoes_loaded = 0;
	o->tsd.ship.torpedoes_loading = 0;
	o->tsd.ship.phaser_bank_charge = 0;
	o->tsd.ship.scizoom = 0;
	o->tsd.ship.weapzoom = 0;
	o->tsd.ship.navzoom = 0;
	o->tsd.ship.warpdrive = 0;
	o->tsd.ship.requested_warpdrive = 0;
	o->tsd.ship.requested_shield = 0;
	o->tsd.ship.phaser_wavelength = 0;
	o->tsd.ship.victim_id = -1;
	o->tsd.ship.power_data.shields.r1 = 0;
	o->tsd.ship.power_data.phasers.r1 = 0;
	o->tsd.ship.power_data.sensors.r1 = 0;
	o->tsd.ship.power_data.comms.r1 = 0;
	o->tsd.ship.power_data.warp.r1 = 0;
	o->tsd.ship.power_data.maneuvering.r1 = 0;
	o->tsd.ship.power_data.impulse.r1 = 0;
	o->tsd.ship.warp_time = -1;
	o->tsd.ship.scibeam_range = 0;
	o->tsd.ship.scibeam_a1 = 0;
	o->tsd.ship.scibeam_a2 = 0;
	o->tsd.ship.reverse = 0;
	memset(&o->tsd.ship.damage, 0, sizeof(o->tsd.ship.damage));
	init_power_model(o);
}

static int add_player(double x, double y, double vx, double vy, double heading)
{
	int i;

	i = add_generic_object(x, y, vx, vy, heading, OBJTYPE_SHIP1);
	if (i < 0)
		return i;
	init_player(&go[i]);
	return i;
}

static void respawn_player(struct snis_entity *o)
{
	o->x = XKNOWN_DIM * (double) rand() / (double) RAND_MAX;
	o->y = YKNOWN_DIM * (double) rand() / (double) RAND_MAX;
	o->vx = 0;
	o->vy = 0;
	o->heading = 0;
	init_player(o);
	o->alive = 1;
	send_ship_damage_packet(o);
}

static int add_ship(void)
{
	int i;
	double x, y, heading;

	x = ((double) snis_randn(1000)) * XKNOWN_DIM / 1000.0;
	y = ((double) snis_randn(1000)) * YKNOWN_DIM / 1000.0;
	heading = degrees_to_radians(0.0 + snis_randn(360)); 
	i = add_generic_object(x, y, 0.0, 0.0, heading, OBJTYPE_SHIP2);
	if (i < 0)
		return i;
	go[i].move = ship_move;
	go[i].tsd.ship.torpedoes = INITIAL_TORPEDO_COUNT;
	go[i].tsd.ship.shields = 100.0;
	go[i].tsd.ship.power = 100.0;
	go[i].tsd.ship.yaw_velocity = 0.0;
	go[i].tsd.ship.desired_velocity = 0;
	go[i].tsd.ship.desired_heading = 0;
	go[i].tsd.ship.velocity = 0;
	go[i].tsd.ship.shiptype = snis_randn(ARRAY_SIZE(shipclass));
	go[i].tsd.ship.victim_id = (uint32_t) -1;
	memset(&go[i].tsd.ship.damage, 0, sizeof(go[i].tsd.ship.damage));
	memset(&go[i].tsd.ship.power_data, 0, sizeof(go[i].tsd.ship.power_data));
	return i;
}

static int add_spacemonster(double x, double y)
{
	int i;
	double heading;

	heading = degrees_to_radians(0.0 + snis_randn(360)); 
	i = add_generic_object(x, y, 0.0, 0.0, heading, OBJTYPE_SPACEMONSTER);
	if (i < 0)
		return i;
	go[i].tsd.spacemonster.zz = 0.0;
	go[i].move = spacemonster_move;
	return i;
}

static int add_asteroid(double x, double y, double vx, double vy, double heading)
{
	int i;

	i = add_generic_object(x, y, vx, vy, heading, OBJTYPE_ASTEROID);
	if (i < 0)
		return i;
	if (snis_randn(100) < 50)
		go[i].z = (double) snis_randn(3000) - 1500;
	else
		go[i].z = (double) snis_randn(70) - 35;
	go[i].sdata.shield_strength = 0;
	go[i].sdata.shield_wavelength = 0;
	go[i].sdata.shield_width = 0;
	go[i].sdata.shield_depth = 0;
	go[i].move = asteroid_move;
	go[i].tsd.asteroid.r = hypot(x - XKNOWN_DIM / 2.0, y - YKNOWN_DIM / 2.0);
	go[i].tsd.asteroid.angle_offset = atan2(x - XKNOWN_DIM / 2.0,
							y - YKNOWN_DIM / 2.0);
	return i;
}

static int add_starbase(double x, double y, double vx, double vy, double heading, int n)
{
	int i;

	i = add_generic_object(x, y, vx, vy, heading, OBJTYPE_STARBASE);
	if (i < 0)
		return i;
	go[i].move = starbase_move;
	go[i].type = OBJTYPE_STARBASE;
	go[i].tsd.starbase.last_time_called_for_help = 0;
	go[i].tsd.starbase.under_attack = 0;
	sprintf(go[i].tsd.starbase.name, "SB-%02d", n);
	return i;
}

static int add_nebula(double x, double y, double vx, double vy, double heading, double r)
{
	int i;

	i = add_generic_object(x, y, vx, vy, heading, OBJTYPE_NEBULA);
	if (i < 0)
		return i;
	go[i].move = nebula_move;
	go[i].type = OBJTYPE_NEBULA;
	go[i].tsd.nebula.r = r;
	return i;
}

static int add_explosion(double x, double y, uint16_t velocity,
				uint16_t nsparks, uint16_t time, uint8_t victim_type)
{
	int i;

	i = add_generic_object(x, y, 0, 0, 0, OBJTYPE_EXPLOSION);
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

static int add_laser(double x, double y, double vx, double vy, double heading, uint32_t ship_id)
{
	int i, s;

	i = add_generic_object(x, y, vx, vy, heading, OBJTYPE_LASER);
	if (i < 0)
		return i;
	go[i].move = laser_move;
	go[i].alive = LASER_LIFETIME;
	go[i].tsd.laser.ship_id = ship_id;
	s = lookup_by_id(ship_id);
	go[i].tsd.laser.power = go[s].tsd.ship.phaser_charge;
	go[s].tsd.ship.phaser_charge = 0;
	go[i].tsd.laser.wavelength = go[s].tsd.ship.phaser_wavelength;
	return i;
}

static void laserbeam_dead(struct snis_entity *o)
{
	snis_queue_delete_object(o);
	delete_object(o);
}

static void laserbeam_move(struct snis_entity *o)
{
	int tid, oid, ttype;
	struct snis_entity *target, *origin;

	o->alive--;
	if (o->alive <= 0) {
		laserbeam_dead(o);
		return;
	}

	/* only deal laser damage every 16th tick */
	if (universe_timestamp & 0x0f)
		return;

	tid = lookup_by_id(o->tsd.laserbeam.target);
	oid = lookup_by_id(o->tsd.laserbeam.origin);
	if (tid < 0 || oid < 0) {
		laserbeam_dead(o);
		return;
	}
	target = &go[tid];
	origin = &go[oid];
	ttype = target->type;
	
	if (ttype == OBJTYPE_STARBASE) {
		target->tsd.starbase.under_attack = 1;
		return;
	}

	if (ttype == OBJTYPE_SHIP1 || ttype == OBJTYPE_SHIP2) {
		calculate_laser_damage(target, o->tsd.laser.wavelength);
		send_ship_damage_packet(target);
		attack_your_attacker(target, lookup_entity_by_id(o->tsd.laser.ship_id));
	}

	if (ttype == OBJTYPE_ASTEROID)
		target->alive = 0;

	if (!target->alive) {
		(void) add_explosion(target->x, target->y, 50, 50, 50, ttype);
		/* TODO -- these should be different sounds */
		/* make sound for players that got hit */
		/* make sound for players that did the hitting */

		if (origin->type == OBJTYPE_SHIP1)
			snis_queue_add_sound(EXPLOSION_SOUND,
					ROLE_SOUNDSERVER, origin->id);
		if (ttype != OBJTYPE_SHIP1) {
			snis_queue_delete_object(target);
			delete_object(target);
			respawn_object(ttype);
		} else {
			snis_queue_add_sound(EXPLOSION_SOUND,
						ROLE_SOUNDSERVER, target->id);
		}
	} else {
		(void) add_explosion(target->x, target->y, 50, 5, 5, ttype);
	}
	return;
}

static int add_laserbeam(uint32_t origin, uint32_t target, int alive)
{
	int i, s;

	i = add_generic_object(0, 0, 0, 0, 0, OBJTYPE_LASERBEAM);
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

static int add_torpedo(double x, double y, double vx, double vy, double heading, uint32_t ship_id)
{
	int i;
	i = add_generic_object(x, y, vx, vy, heading, OBJTYPE_TORPEDO);
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
	double x, y;

	for (i = 0; i < NBASES; i++) {
		x = ((double) snis_randn(1000)) * XKNOWN_DIM / 1000.0;
		y = ((double) snis_randn(1000)) * YKNOWN_DIM / 1000.0;
		add_starbase(x, y, 0.0, 0.0, 0.0, i);
	}
}

static void add_nebulae(void)
{
	int i, j;
	double x, y, r;

	for (i = 0; i < NNEBULA; i++) {
		x = ((double) snis_randn(1000)) * XKNOWN_DIM / 1000.0;
		y = ((double) snis_randn(1000)) * YKNOWN_DIM / 1000.0;
		r = (double) snis_randn(NEBULA_RADIUS) +
				(double) MIN_NEBULA_RADIUS;
		j = add_nebula(x, y, 0.0, 0.0, 0.0, r);
		nebulalist[i] = j;
	}
}

static void add_asteroids(void)
{
	int i, j;
	double x, y, cx, cy, a, r;

	for (i = 0; i < NASTEROID_CLUSTERS; i++) {
		cx = ((double) snis_randn(1000)) * XKNOWN_DIM / 1000.0;
		cy = ((double) snis_randn(1000)) * YKNOWN_DIM / 1000.0;
		for (j = 0; j < NASTEROIDS / NASTEROID_CLUSTERS; j++) {
			a = (double) snis_randn(360) * M_PI / 180.0;
			r = snis_randn(ASTEROID_CLUSTER_RADIUS);
			x = cx + r * sin(a);
			y = cy + r * cos(a);
			add_asteroid(x, y, 0.0, 0.0, 0.0);
		}
	}
}

static int add_planet(double x, double y)
{
	int i;

	i = add_generic_object(x, y, 0, 0, 0, OBJTYPE_PLANET);
	if (i < 0)
		return i;
	if (snis_randn(100) < 50)
		go[i].z = (double) snis_randn(3000) - 1500;
	else
		go[i].z = (double) snis_randn(70) - 35;
	go[i].sdata.shield_strength = 0;
	go[i].sdata.shield_wavelength = 0;
	go[i].sdata.shield_width = 0;
	go[i].sdata.shield_depth = 0;
	go[i].move = generic_move;
	return i;
}

static void add_planets(void)
{
	int i;
	double x, y, cx, cy, a, r;

	for (i = 0; i < NPLANETS; i++) {
		cx = ((double) snis_randn(1000)) * XKNOWN_DIM / 1000.0;
		cy = ((double) snis_randn(1000)) * YKNOWN_DIM / 1000.0;
		a = (double) snis_randn(360) * M_PI / 180.0;
		r = snis_randn(ASTEROID_CLUSTER_RADIUS);
		x = cx + r * sin(a);
		y = cy + r * cos(a);
		add_planet(x, y);
	}
}


static int add_wormhole(double x1, double y1, double x2, double y2)
{
	int i;

	i = add_generic_object(x1, y1, 0.0, 0.0, 0.0, OBJTYPE_WORMHOLE);
	if (i < 0)
		return i;
	go[i].move = wormhole_move;
	go[i].tsd.wormhole.dest_x = x2;
	go[i].tsd.wormhole.dest_y = y2;
	return i;
}

static void add_wormholes(void)
{
	int i;
	double x1, y1, x2, y2;

	for (i = 0; i < NWORMHOLE_PAIRS; i++) {
		do {
			x1 = ((double) snis_randn(1000)) * XKNOWN_DIM / 1000.0;
			y1 = ((double) snis_randn(1000)) * YKNOWN_DIM / 1000.0;
			x2 = ((double) snis_randn(1000)) * XKNOWN_DIM / 1000.0;
			y2 = ((double) snis_randn(1000)) * YKNOWN_DIM / 1000.0;
		} while (hypot(x1 - x2, y1 - y2) < XKNOWN_DIM / 2.0);
		add_wormhole(x1, y1, x2, y2);
		add_wormhole(x2, y2, x1, y1);
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
	double x, y;

	for (i = 0; i < NSPACEMONSTERS; i++) {
		x = ((double) snis_randn(1000)) * XKNOWN_DIM / 1000.0;
		y = ((double) snis_randn(1000)) * YKNOWN_DIM / 1000.0;
		add_spacemonster(x, y);
	}
}

static void make_universe(void)
{
	pthread_mutex_lock(&universe_mutex);
	snis_object_pool_setup(&pool, MAXGAMEOBJS);

	add_nebulae(); /* do nebula first */
	add_starbases();
	add_asteroids();
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

static void __attribute__((unused)) timespec_subtract(struct timespec *have, struct timespec *takeaway, struct timespec *leaves)
{
	leaves->tv_nsec = have->tv_nsec - takeaway->tv_nsec;
	if (have->tv_nsec < takeaway->tv_nsec) {
		leaves->tv_nsec += 1000000000;
		leaves->tv_sec = have->tv_sec - takeaway->tv_sec - 1;
	} else {
		leaves->tv_sec = have->tv_sec - takeaway->tv_sec;
	}
}

static void sleep_tenth_second(void)
{
	struct timespec t, x;
	int rc;

	t.tv_sec = 0;
	t.tv_nsec = 99999999; 
	x.tv_sec = 0;
	x.tv_nsec = 0;

	do {
		rc = clock_nanosleep(CLOCK_MONOTONIC, 0,
				(const struct timespec *) &t, &x);
	} while (rc == EINTR);
}

static void sleep_thirtieth_second(void)
{
	struct timespec t, x;
	int rc;

	t.tv_sec = 0;
	t.tv_nsec = 99999999; 
	x.tv_sec = 0;
	x.tv_nsec = 0;

	do {
		rc = clock_nanosleep(CLOCK_MONOTONIC, 0, &t, &x);
	} while (rc == EINTR);
}

#if 0
/* Sleep for enough time, x, such that (end - begin + x) == total*/
static void snis_sleep(struct timespec *begin, struct timespec *end, struct timespec *total)
{
#if 0

	/* this code seems to be buggy. */
	int rc;
	struct timespec used, diff, remain;

	timespec_subtract(end, begin, &used);
	if (used.tv_nsec >= total->tv_nsec)
		return; 

	timespec_subtract(total, &used, &remain);
	do {
		diff = remain;
		rc = clock_nanosleep(CLOCK_MONOTONIC, 0, &diff, &remain);	
	} while (rc == EINTR);
#else
	int rc;

	do {
		rc = clock_nanosleep(CLOCK_MONOTONIC, 0,
			(const struct timespec *) total, begin);
	} while (rc == EINTR);
#endif
}
#endif

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
	double dx, dy;
	int i, rc;

	rc = read_and_unpack_buffer(c, buffer, "wSS", &oid,
			&dx, (int32_t) UNIVERSE_DIM,
			&dy, (int32_t) UNIVERSE_DIM);
	if (rc)
		return rc;
	if (!(c->role & ROLE_DEMON))
		return 0;
	pthread_mutex_lock(&universe_mutex);
	i = lookup_by_id(oid);
	if (i < 0 || !go[i].alive)
		return 0;
	o = &go[i];
	o->x += dx;
	o->y += dy;
	o->timestamp = universe_timestamp + 10;
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
		(o->y - ship->y) * (o->y - ship->y);
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
	angle = atan2(o->y - ship->y, o->x - ship->x);
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
	send_packet_to_all_clients_on_a_bridge(c->shipid, 
			packed_buffer_new("hb", OPCODE_SCI_DETAILS,
			!!(new_details)), ROLE_SCIENCE);
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
	ship->tsd.ship.victim_id = go[index].id;
	return 0;
}

static int process_sci_select_coords(struct game_client *c)
{
	unsigned char buffer[sizeof(struct snis_sci_select_coords_packet)];
	uint32_t x, y;
	int rc;

	rc = read_and_unpack_buffer(c, buffer, "ww", &x, &y);
	if (rc)
		return rc;
	/* just turn it around and fan it out to all the right places */
	send_packet_to_all_clients_on_a_bridge(c->shipid,
				packed_buffer_new("hww", OPCODE_SCI_SELECT_COORDS, x, y),
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
			(o->y - go[i].y) * (o->y - go[i].y);
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
	dist = hypot(sb->x - o->x, sb->y - o->y);
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

static void update_command_data(uint32_t id, struct command_data *cmd_data)
{
	struct snis_entity *o;
	int i;

	i = lookup_by_id(id);
	if (i < 0)
		goto out;

	o = &go[i];

	if (o->type != OBJTYPE_SHIP2)
		goto out;

	switch (cmd_data->command) {
		case DEMON_CMD_ATTACK:
			o->tsd.ship.cmd_data = *cmd_data;
			o->tsd.ship.victim_id = (uint32_t) -1;
			ship_choose_new_attack_victim(o);
			break;
		default:
			goto out;	
	}

out:
	pthread_mutex_unlock(&universe_mutex);
	return;
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

	rc = read_and_unpack_buffer(c, buffer, "Sb",
				&view_angle, (int32_t) 360, &view_mode);
	if (rc)
		return rc;
	/* Rebuild packet and send to all clients with main screen role */
	send_packet_to_all_clients_on_a_bridge(c->shipid, 
			packed_buffer_new("hSb", OPCODE_MAINSCREEN_VIEW_MODE,
					view_angle, (int32_t) 360, view_mode),
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

static int process_demon_command(struct game_client *c)
{
	unsigned char buffer[sizeof(struct demon_cmd_packet) + 255 * sizeof(uint32_t)];
	struct packed_buffer pb;
	int i, rc;
	uint32_t ix, iy;
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
	packed_buffer_extract(&pb, "bwwbb", &cmd_data.command, &ix, &iy, &nids1, &nids2);
	rc = snis_readsocket(c->socket, buffer + offset, (nids1 + nids2) * sizeof(uint32_t));
	if (rc)
		return rc;

	for (i = 0; i < nids1; i++)
		packed_buffer_extract(&pb, "w", &id1[i]);
	for (i = 0; i < nids2; i++)
		packed_buffer_extract(&pb, "w", &id2[i]);

	printf("Demon command received, opcode = %02x\n", cmd_data.command);
	printf("  x = %08x, y = %08x\n", ix, iy);
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
	cmd_data.y = iy;
	cmd_data.nids1 = nids1;
	cmd_data.nids2 = nids2;

	for (i = 0; i < nids1; i++)
		update_command_data(id1[i], &cmd_data);
	
	return 0;
}

static int process_create_item(struct game_client *c)
{
	unsigned char buffer[10];
	unsigned char item_type;
	double x, y, r;
	int rc, i = -1;

	rc = read_and_unpack_buffer(c, buffer, "bSS", &item_type,
			&x, (int32_t) UNIVERSE_DIM, &y, (int32_t) UNIVERSE_DIM);
	if (rc)
		return rc;

	pthread_mutex_lock(&universe_mutex);
	switch (item_type) {
	case OBJTYPE_SHIP2:
		i = add_ship();
		break;
	case OBJTYPE_STARBASE:
		i = add_starbase(x, y, 0, 0, 0, snis_randn(100));
		break;
	case OBJTYPE_PLANET:
		i = add_planet(x, y);
		break;
	case OBJTYPE_NEBULA:
		r = (double) snis_randn(NEBULA_RADIUS) +
				(double) MIN_NEBULA_RADIUS;
		i = add_nebula(x, y, 0, 0, 0, r);
		break;
	case OBJTYPE_SPACEMONSTER:
		i = add_spacemonster(x, y);
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
		go[i].y = y;
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
	snis_queue_delete_object(o);
	delete_object(o);
out:
	pthread_mutex_unlock(&universe_mutex);

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
	bridgelist[b].warpx = o->x + wfactor * sin(o->heading);
	bridgelist[b].warpy = o->y + wfactor * -cos(o->heading);
	o->tsd.ship.warp_time = 85; /* 8.5 seconds */
	pthread_mutex_unlock(&universe_mutex);
	send_initiate_warp_packet(c);
	return 0;
}

static int process_request_warp_pwr(struct game_client *c)
{
	return process_request_bytevalue_pwr(c, offsetof(struct snis_entity, tsd.ship.power_data.warp.r2), no_limit); 
}

static int process_request_impulse_pwr(struct game_client *c)
{
	return process_request_bytevalue_pwr(c, offsetof(struct snis_entity, tsd.ship.power_data.impulse.r2), no_limit); 
}

static int process_request_sensors_pwr(struct game_client *c)
{
	return process_request_bytevalue_pwr(c, offsetof(struct snis_entity, tsd.ship.power_data.sensors.r2), no_limit); 
}

static int process_request_comms_pwr(struct game_client *c)
{
	return process_request_bytevalue_pwr(c, offsetof(struct snis_entity, tsd.ship.power_data.comms.r2), no_limit); 
}

static int process_request_shields_pwr(struct game_client *c)
{
	return process_request_bytevalue_pwr(c, offsetof(struct snis_entity, tsd.ship.power_data.shields.r2), no_limit); 
}

static int process_request_phaserbanks_pwr(struct game_client *c)
{
	return process_request_bytevalue_pwr(c, offsetof(struct snis_entity, tsd.ship.power_data.phasers.r2), no_limit); 
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
		yaw_func(c, -1);
		break;
	case YAW_RIGHT:
		yaw_func(c, 1);
		break;
	case YAW_LEFT_FINE:
		yaw_func(c, -2);
		break;
	case YAW_RIGHT_FINE:
		yaw_func(c, 2);
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
		do_demon_yaw(o, -1);
		break;
	case YAW_RIGHT:
		do_demon_yaw(o, 1);
		break;
	case YAW_LEFT_FINE:
		do_demon_yaw(o, -2);
		break;
	case YAW_RIGHT_FINE:
		do_demon_yaw(o, 2);
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
	double vx, vy;

	if (ship->tsd.ship.torpedoes_loaded <= 0)
		return 0;

	vx = TORPEDO_VELOCITY * sin(ship->tsd.ship.gun_heading);
	vy = TORPEDO_VELOCITY * -cos(ship->tsd.ship.gun_heading);
	pthread_mutex_lock(&universe_mutex);
	add_torpedo(ship->x, ship->y, vx, vy, ship->tsd.ship.gun_heading, ship->id); 
	ship->tsd.ship.torpedoes_loaded--;
	snis_queue_add_sound(TORPEDO_LAUNCH_SOUND, ROLE_SOUNDSERVER, ship->id);
	pthread_mutex_unlock(&universe_mutex);
	return 0;
}

static int process_demon_fire_torpedo(struct game_client *c)
{
	struct snis_entity *o;
	unsigned char buffer[10];
	double vx, vy;
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
	vy = TORPEDO_VELOCITY * sin(o->heading);
	add_torpedo(o->x, o->y, vx, vy, o->heading, o->id);
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
	if (ship->tsd.ship.victim_id < 0)
		goto laserfail;

	tid = ship->tsd.ship.victim_id;
	if (ship->tsd.ship.phaser_charge < (ship->tsd.ship.power_data.phasers.i * 3) / 4)
		goto laserfail;
	add_laserbeam(ship->id, tid, 30);
	snis_queue_add_sound(LASER_FIRE_SOUND, ROLE_SOUNDSERVER, ship->id);
	pthread_mutex_unlock(&universe_mutex);
	return 0;

laserfail:
	snis_queue_add_sound(LASER_FAILURE, ROLE_SOUNDSERVER, ship->id);
	pthread_mutex_unlock(&universe_mutex);
	return 0;
}

static int process_demon_fire_phaser(struct game_client *c)
{
	struct snis_entity *o;
	unsigned char buffer[10];
	double vx, vy;
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
	vy = LASER_VELOCITY * sin(o->heading);
	add_laser(o->x, o->y, vx, vy, o->heading, o->id);
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
		case OPCODE_REQUEST_GUNYAW:
			rc = process_request_yaw(c, do_gun_yaw);
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
		break;
	case OBJTYPE_SHIP2:
		send_econ_update_ship_packet(c, o);
		send_update_sdata_packets(c, o);
		break;
	case OBJTYPE_ASTEROID:
		send_update_asteroid_packet(c, o);
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
	default:
		break;
	}
}

static int too_far_away_to_care(struct game_client *c, struct snis_entity *o)
{
	struct snis_entity *ship = &go[c->ship_index];
	double dx, dy, dist;
	const double threshold = (XKNOWN_DIM / 2) * (XKNOWN_DIM / 2);

	/* do not optimize updates for brand new clients, they need everything. */
	if (c->timestamp == 0)
		return 0;

	dx = (ship->x - o->x);
	dy = (ship->y - o->y);
	dist = (dx * dx) + (dy * dy);
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

static void *per_client_write_thread(__attribute__((unused)) void /* struct game_client */ *client)
{
	struct timespec time1;
	struct timespec time2;
	struct timespec tenth_second;
	int rc;
	struct game_client *c = (struct game_client *) client;

	memset(&tenth_second, 0, sizeof(tenth_second));
	tenth_second.tv_sec = 1;
	tenth_second.tv_nsec = 999999999;
	tenth_second.tv_nsec = 999999999;

	/* Wait for client[] array to get fully updated before proceeding. */
	client_lock();
	client_unlock();
	while (1) {
		rc = clock_gettime(CLOCK_MONOTONIC, &time1);
		queue_up_client_updates(c);
		write_queued_updates_to_client(c);
		if (c->socket < 0)
			break;
		c->timestamp = universe_timestamp;
		rc = clock_gettime(CLOCK_MONOTONIC, &time2);
		/* snis_sleep(&time1, &time2, &tenth_second); */ /* sleep for 1/10th sec - (time2 - time1) */
		sleep_tenth_second();
	}
	log_client_info(SNIS_INFO, c->socket, "client writer thread exiting.\n");
	if (rc) /* satisfy the whining compiler */
		return NULL;
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
	double dv = sqrt((o->vx * o->vx) + (o->vy * o->vy));

	pb_queue_to_client(c, packed_buffer_new("hwwSSUUwb", OPCODE_ECON_UPDATE_SHIP,
			o->id, o->alive, o->x, (int32_t) UNIVERSE_DIM,
			o->y, (int32_t) UNIVERSE_DIM,
			dv, (uint32_t) UNIVERSE_DIM, o->heading, (uint32_t) 360,
			o->tsd.ship.victim_id, o->tsd.ship.shiptype));
}

static void send_ship_sdata_packet(struct game_client *c, struct ship_sdata_packet *sip)
{
	struct packed_buffer *pb;

	pb = packed_buffer_allocate(sizeof(struct ship_sdata_packet));
	packed_buffer_append(pb, "hwbbbbbbr", OPCODE_SHIP_SDATA, sip->id, sip->subclass,
		sip->shield_strength, sip->shield_wavelength, sip->shield_width, sip->shield_depth,
		sip->faction, sip->name, (unsigned short) sizeof(sip->name));
	send_packet_to_all_clients_on_a_bridge(c->shipid, pb, ROLE_ALL);
}

static void send_ship_damage_packet(struct snis_entity *o)
{
	struct packed_buffer *pb;

	pb = packed_buffer_allocate(sizeof(struct ship_damage_packet));
	packed_buffer_append(pb, "hwr", OPCODE_UPDATE_DAMAGE, o->id,
		(char *) &o->tsd.ship.damage, sizeof(o->tsd.ship.damage));
	send_packet_to_all_clients(pb, ROLE_ALL);
}

static void send_comms_packet(char *sender, char *str)
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
	packed_buffer_append(pb, "hwwSSSS", opcode, o->id, o->alive,
			o->x, (int32_t) UNIVERSE_DIM, o->y, (int32_t) UNIVERSE_DIM,
			o->vx, (int32_t) UNIVERSE_DIM, o->vy, (int32_t) UNIVERSE_DIM);
	packed_buffer_append(pb, "UwwUUUbbbwbbbbbbbbbbbw", o->heading, (uint32_t) 360,
			o->tsd.ship.torpedoes, o->tsd.ship.power,
			o->tsd.ship.gun_heading, (uint32_t) 360,
			o->tsd.ship.sci_heading, (uint32_t) 360,
			o->tsd.ship.sci_beam_width, (uint32_t) 360,
			tloading, throttle, rpm, fuel, o->tsd.ship.temp,
			o->tsd.ship.scizoom, o->tsd.ship.weapzoom, o->tsd.ship.navzoom,
			o->tsd.ship.warpdrive, o->tsd.ship.requested_warpdrive,
			o->tsd.ship.requested_shield, o->tsd.ship.phaser_charge,
			o->tsd.ship.phaser_wavelength, o->tsd.ship.shiptype,
			o->tsd.ship.reverse, o->tsd.ship.victim_id);
	pb_queue_to_client(c, pb);
}

static void send_update_damcon_obj_packet(struct game_client *c,
		struct snis_damcon_entity *o)
{
	pb_queue_to_client(c, packed_buffer_new("hwwwSSSS",
					OPCODE_DAMCON_OBJ_UPDATE,   
					o->id, o->ship_id, o->type,
					o->x, (int32_t) DAMCONXDIM,
					o->y, (int32_t) DAMCONYDIM,
					o->velocity,  (int32_t) DAMCONXDIM,
		/* send desired_heading as heading to client to enable drifting */
					o->tsd.robot.desired_heading, (int32_t) 360));
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
	pb_queue_to_client(c, packed_buffer_new("hwwwSSUbbb",
					OPCODE_DAMCON_PART_UPDATE,   
					o->id, o->ship_id, o->type,
					o->x, (int32_t) DAMCONXDIM,
					o->y, (int32_t) DAMCONYDIM,
					o->heading, (uint32_t) 360,
					o->tsd.part.system,
					o->tsd.part.part,
					o->tsd.part.damage));
}

static void send_update_asteroid_packet(struct game_client *c,
	struct snis_entity *o)
{
	pb_queue_to_client(c, packed_buffer_new("hwSSS", OPCODE_UPDATE_ASTEROID, o->id,
					o->x, (int32_t) UNIVERSE_DIM,
					o->y, (int32_t) UNIVERSE_DIM,
					o->z, (int32_t) UNIVERSE_DIM));
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
					o->id, o->x, (int32_t) UNIVERSE_DIM, o->y,
					(int32_t) UNIVERSE_DIM));
}

static void send_update_starbase_packet(struct game_client *c,
	struct snis_entity *o)
{
	pb_queue_to_client(c, packed_buffer_new("hwSS", OPCODE_UPDATE_STARBASE,
					o->id, o->x, (int32_t) UNIVERSE_DIM, o->y,
					(int32_t) UNIVERSE_DIM));
}

static void send_update_nebula_packet(struct game_client *c,
	struct snis_entity *o)
{
	pb_queue_to_client(c, packed_buffer_new("hwSSS", OPCODE_UPDATE_NEBULA, o->id,
					o->x, (int32_t) UNIVERSE_DIM,
					o->y, (int32_t) UNIVERSE_DIM,
					o->tsd.nebula.r, (int32_t) UNIVERSE_DIM));
}

static void send_update_explosion_packet(struct game_client *c,
	struct snis_entity *o)
{
	pb_queue_to_client(c, packed_buffer_new("hwSShhhb", OPCODE_UPDATE_EXPLOSION, o->id,
				o->x, (int32_t) UNIVERSE_DIM, o->y, (int32_t) UNIVERSE_DIM,
				o->tsd.explosion.nsparks, o->tsd.explosion.velocity,
				o->tsd.explosion.time, o->tsd.explosion.victim_type));
}

static void send_update_torpedo_packet(struct game_client *c,
	struct snis_entity *o)
{
	pb_queue_to_client(c, packed_buffer_new("hwwSSSS", OPCODE_UPDATE_TORPEDO, o->id,
					o->tsd.torpedo.ship_id,
					o->x, (int32_t) UNIVERSE_DIM,
					o->y, (int32_t) UNIVERSE_DIM,
					o->vx, (int32_t) UNIVERSE_DIM,
					o->vy, (int32_t) UNIVERSE_DIM));
}

static void send_update_laser_packet(struct game_client *c,
	struct snis_entity *o)
{
	pb_queue_to_client(c, packed_buffer_new("hwwSSSS", OPCODE_UPDATE_LASER,
					o->id, o->tsd.laser.ship_id,
					o->x, (int32_t) UNIVERSE_DIM,
					o->y, (int32_t) UNIVERSE_DIM,
					o->vx, (int32_t) UNIVERSE_DIM,
					o->vy, (int32_t) UNIVERSE_DIM));
}

static void send_update_laserbeam_packet(struct game_client *c,
	struct snis_entity *o)
{
	pb_queue_to_client(c, packed_buffer_new("hwww", OPCODE_UPDATE_LASERBEAM,
					o->id, o->tsd.laserbeam.origin,
					o->tsd.laserbeam.target));
}

static void send_update_spacemonster_packet(struct game_client *c,
	struct snis_entity *o)
{
	pb_queue_to_client(c, packed_buffer_new("hwSSS", OPCODE_UPDATE_SPACEMONSTER, o->id,
					o->x, (int32_t) UNIVERSE_DIM,
					o->y, (int32_t) UNIVERSE_DIM,
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
		double x, y;

		x = XKNOWN_DIM * (double) rand() / (double) RAND_MAX;
		y = YKNOWN_DIM * (double) rand() / (double) RAND_MAX;
		pthread_mutex_lock(&universe_mutex);
		c->ship_index = add_player(x, y, 0.0, 0.0, 0.0);
		c->shipid = go[c->ship_index].id;
		strcpy(go[c->ship_index].sdata.name, (const char * restrict) app.shipname);
		memset(&bridgelist[nbridges], 0, sizeof(bridgelist[nbridges]));
		strcpy((char *) bridgelist[nbridges].shipname, (const char *) app.shipname);
		strcpy((char *) bridgelist[nbridges].password, (const char *) app.password);
		bridgelist[nbridges].shipid = c->shipid;
		c->bridge = nbridges;
		populate_damcon_arena(&bridgelist[c->bridge].damcon);
	
		nbridges++;
		
		pthread_mutex_unlock(&universe_mutex);
	} else {
		c->shipid = bridgelist[c->bridge].shipid;
		c->ship_index = lookup_by_id(c->shipid);
	}
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
			}
		}
	}
	move_damcon_entities();
	pthread_mutex_unlock(&universe_mutex);

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

int main(int argc, char *argv[])
{
	int port, rc, i;
	struct timespec time1;
	struct timespec time2;
	struct timespec thirtieth_second;

	if (argc < 5) 
		usage();

	open_log_file();

	snis_protocol_debugging(1);

	memset(&thirtieth_second, 0, sizeof(thirtieth_second));
	thirtieth_second.tv_nsec = 33333333; /* 1/30th second */

	make_universe();
	port = start_listener_thread();

	ignore_sigpipe();	
	snis_collect_netstats(&netstats);
	register_with_game_lobby(argv[1], port, argv[2], argv[1], argv[3]);

	i = 0;
	while (1) {
		rc = clock_gettime(CLOCK_MONOTONIC, &time1);
		/* if ((i % 30) == 0) printf("Moving objects...i = %d\n", i); */
		i++;
		move_objects();
		rc = clock_gettime(CLOCK_MONOTONIC, &time2);
		/* snis_sleep(&time1, &time2, &thirtieth_second); */
		sleep_thirtieth_second();
	}

	snis_close_logfile();

	if (rc) /* satisfy compiler */
		return 0; 
	return 0;
}
