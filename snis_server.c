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
#include <sys/socket.h>
#include <netdb.h>
#include <errno.h>
#include <time.h>
#include <ctype.h>
#include <math.h>
#include <linux/tcp.h>

#include "ssgl/ssgl.h"
#include "snis.h"
#include "mathutils.h"
#include "snis_alloc.h"
#include "snis_marshal.h"
#include "snis_socket_io.h"
#include "snis_packet.h"

#define CLIENT_UPDATE_PERIOD_NSECS 500000000
#define MAXCLIENTS 100
struct game_client {
	int socket;
	pthread_t read_thread;
	pthread_t write_thread;
	pthread_attr_t read_attr, write_attr;

	struct packed_buffer_queue client_write_queue;
	pthread_mutex_t client_write_queue_mutex;
	uint32_t shipid;
	uint8_t role;
	uint32_t timestamp;
} client[MAXCLIENTS];
int nclients = 0;
static pthread_mutex_t client_mutex = PTHREAD_MUTEX_INITIALIZER;

struct bridge_credentials {
	unsigned char shipname[20];
	unsigned char password[20];
	uint32_t shipid;
} bridgelist[MAXCLIENTS];
int nbridges = 0;
static pthread_mutex_t universe_mutex = PTHREAD_MUTEX_INITIALIZER;

static pthread_mutex_t listener_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t listener_started;
int listener_port = -1;
pthread_t lobbythread;

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

static void generic_move(__attribute__((unused)) struct snis_entity *o)
{
	return;
}

static void torpedo_move(struct snis_entity *o)
{
	int i;

	o->x += o->vx;
	o->y += o->vy;
	o->timestamp = universe_timestamp;
	/* o->alive--; */

	for (i = 0; i <= snis_object_pool_highest_object(pool); i++)
		if (go[i].alive && i != o->index && o->alive < TORPEDO_LIFETIME - 3) {
			printf("Hit!\n");
			o->alive = 0;
		}

#if 0
	/* FIXME: need to figure out how to delete object on all clients.
	 * For now, torpedoes are immortal, like everything else in my
	 * universe.  I'm such a nice god, aren't I?
	 */
	if (o->alive <= 0) {
		snis_queue_delete_object(o->id);
		snis_object_pool_free_object(pool, o->index);
	}
#endif
}

static void ship_move(struct snis_entity *o)
{
	int v;
	if (snis_randn(100) < 5) {
		o->heading = degrees_to_radians(0.0 + snis_randn(360)); 
		v = snis_randn(50);
		o->vx = v * sin(o->heading);
		o->vy = v * -cos(o->heading);
	}
	o->x += o->vx;
	o->y += o->vy;
	o->timestamp = universe_timestamp;
}

static void normalize_heading(double *heading)
{
	/* FIXME, there's undoubtedly a better way to normalize radians */
	while (*heading > (360.0 * PI / 180.0))
		*heading -= (360.0 * PI / 180.0);
	while (*heading < 0)
		*heading += (360.0 * PI / 180.0); 
}

static void damp_yaw_velocity(double *yv)
{
	if (fabs(*yv) < (0.3 * PI / 180.0))
		*yv = 0.0;
	else
		*yv *= 0.85;
}

static void player_move(struct snis_entity *o)
{
	o->vy = o->tsd.ship.velocity * cos(o->heading);
	o->vx = o->tsd.ship.velocity * -sin(o->heading);
	o->x += o->vx;
	o->y += o->vy;
	o->heading += o->tsd.ship.yaw_velocity;
	o->tsd.ship.gun_heading += o->tsd.ship.gun_yaw_velocity;

	normalize_heading(&o->heading);
	normalize_heading(&o->tsd.ship.gun_heading);
	o->timestamp = universe_timestamp;

	damp_yaw_velocity(&o->tsd.ship.yaw_velocity);
	damp_yaw_velocity(&o->tsd.ship.gun_yaw_velocity);

	/* Damp velocity */
	if (fabs(o->tsd.ship.velocity) < MIN_PLAYER_VELOCITY)
		o->tsd.ship.velocity = 0.0;
	else
		o->tsd.ship.velocity *= PLAYER_VELOCITY_DAMPING;
}

static void starbase_move(struct snis_entity *o)
{
	/* FIXME, fill this in. */
}

static int add_generic_object(double x, double y, double vx, double vy, double heading, int type)
{
	int i;

	i = snis_object_pool_alloc_obj(pool); 	 
	printf("allocated object %d\n", i);
	if (i < 0)
		return -1;
	memset(&go[i], 0, sizeof(go[i]));
	go[i].id = get_new_object_id();
	go[i].index = i;
	go[i].alive = 1;
	go[i].x = x;
	go[i].y = y;
	go[i].vx = vx;
	go[i].vy = vy;
	go[i].heading = heading;
	go[i].type = type;
	go[i].timestamp = universe_timestamp + 1;
	go[i].move = generic_move;
	return i;
}

static int add_player(double x, double y, double vx, double vy, double heading)
{
	int i;

	i = add_generic_object(x, y, vx, vy, heading, OBJTYPE_SHIP1);
	if (i < 0)
		return i;
	go[i].move = player_move;
	go[i].tsd.ship.torpedoes = 1000;
	go[i].tsd.ship.shields = 0;
	go[i].tsd.ship.energy = 0;
	go[i].tsd.ship.yaw_velocity = 0.0;
	go[i].tsd.ship.gun_yaw_velocity = 0.0;
	go[i].tsd.ship.gun_heading = 0.0;
	go[i].tsd.ship.velocity = 0.0;
	return i;
}

static int add_ship1(double x, double y, double vx, double vy, double heading)
{
	int i;

	i = add_generic_object(x, y, vx, vy, heading, OBJTYPE_SHIP1);
	if (i < 0)
		return i;
	go[i].move = ship_move;
	go[i].tsd.ship.torpedoes = 0;
	go[i].tsd.ship.shields = 0;
	go[i].tsd.ship.energy = 0;
	go[i].tsd.ship.yaw_velocity = 0.0;
	return i;
	return i;
}

static int add_planet(double x, double y, double vx, double vy, double heading)
{
	return add_generic_object(x, y, vx, vy, heading, OBJTYPE_PLANET);
}

static int add_starbase(double x, double y, double vx, double vy, double heading)
{
	int i;

	i = add_generic_object(x, y, vx, vy, heading, OBJTYPE_STARBASE);
	if (i < 0)
		return i;
	go[i].move = starbase_move;
	return i;
}

static int __attribute__((unused)) add_laser(double x, double y, double vx, double vy, double heading)
{
	return add_generic_object(x, y, vx, vy, heading, OBJTYPE_LASER);
}

static int add_torpedo(double x, double y, double vx, double vy, double heading)
{
	int i;
	i = add_generic_object(x, y, vx, vy, heading, OBJTYPE_TORPEDO);
	if (i < 0)
		return i;
	go[i].move = torpedo_move;
	go[i].alive = TORPEDO_LIFETIME;
	return i;
}

static void __attribute__((unused)) add_starbases(void)
{
	int i;
	double x, y;

	for (i = 0; i < NBASES; i++) {
		x = ((double) snis_randn(1000)) * XUNIVERSE_DIMENSION / 1000.0;
		y = ((double) snis_randn(1000)) * YUNIVERSE_DIMENSION / 1000.0;
		add_starbase(x, y, 0.0, 0.0, 0.0);
	}
}

static void add_planets(void)
{
	int i;
	double x, y;

	for (i = 0; i < NPLANETS; i++) {
		x = ((double) snis_randn(1000)) * XUNIVERSE_DIMENSION / 1000.0;
		y = ((double) snis_randn(1000)) * YUNIVERSE_DIMENSION / 1000.0;
		add_planet(x, y, 0.0, 0.0, 0.0);
	}
}

static void add_eships(void)
{
	int i;
	double x, y, heading;

	for (i = 0; i < NESHIPS; i++) {
		x = ((double) snis_randn(1000)) * XUNIVERSE_DIMENSION / 1000.0;
		y = ((double) snis_randn(1000)) * YUNIVERSE_DIMENSION / 1000.0;
		heading = degrees_to_radians(0.0 + snis_randn(360)); 
		add_ship1(x, y, 0.0, 0.0, heading);
		
	}
}

static void make_universe(void)
{
	pthread_mutex_lock(&universe_mutex);
	snis_object_pool_setup(&pool, MAXGAMEOBJS);

	add_starbases();
	add_planets();
	add_eships();
	pthread_mutex_unlock(&universe_mutex);
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

static void do_thrust(struct game_client *c, int thrust)
{
	struct snis_entity *ship = &go[c->shipid];

	if (thrust > 0) {
		if (ship->tsd.ship.velocity < MAX_PLAYER_VELOCITY)
			ship->tsd.ship.velocity += PLAYER_VELOCITY_INCREMENT;
	} else {
		if (ship->tsd.ship.velocity > -MAX_PLAYER_VELOCITY)
			ship->tsd.ship.velocity -= PLAYER_VELOCITY_INCREMENT;
	}
}

typedef void (*do_yaw_function)(struct game_client *c, int yaw);

static void do_yaw(struct game_client *c, int yaw)
{
	struct snis_entity *ship = &go[c->shipid];

	if (yaw > 0) {
		if (ship->tsd.ship.yaw_velocity < MAX_YAW_VELOCITY)
			ship->tsd.ship.yaw_velocity += YAW_INCREMENT;
	} else {
		if (ship->tsd.ship.yaw_velocity > -MAX_YAW_VELOCITY)
			ship->tsd.ship.yaw_velocity -= YAW_INCREMENT;
	}
}

static void do_gun_yaw(struct game_client *c, int yaw)
{
	/* FIXME combine this with do_yaw somehow */
	struct snis_entity *ship = &go[c->shipid];

	if (yaw > 0) {
		if (ship->tsd.ship.gun_yaw_velocity < MAX_YAW_VELOCITY)
			ship->tsd.ship.gun_yaw_velocity += YAW_INCREMENT;
	} else {
		if (ship->tsd.ship.gun_yaw_velocity > -MAX_YAW_VELOCITY)
			ship->tsd.ship.gun_yaw_velocity -= YAW_INCREMENT;
	}
}

static int process_request_thrust(struct game_client *c)
{
	unsigned char buffer[10];
	struct packed_buffer pb;
	uint8_t thrust;
	int rc;

	rc = snis_readsocket(c->socket, buffer, sizeof(struct request_thrust_packet) - sizeof(uint16_t));
	if (rc)
		return rc;
	pb.buffer_size = sizeof(buffer);
	pb.buffer = buffer;
	pb.buffer_cursor = 0;

	thrust = packed_buffer_extract_u8(&pb);
	switch (thrust) {
	case THRUST_FORWARDS:
		do_thrust(c, 1);
		break;
	case THRUST_BACKWARDS:
		do_thrust(c, -1);
		break;
	default:
		break;
	}
	return 0;
}

static int process_request_yaw(struct game_client *c, do_yaw_function yaw_func)
{
	unsigned char buffer[10];
	struct packed_buffer pb;
	uint8_t yaw;
	int rc;

	rc = snis_readsocket(c->socket, buffer, sizeof(struct request_yaw_packet) - sizeof(uint16_t));
	if (rc)
		return rc;
	pb.buffer_size = sizeof(buffer);
	pb.buffer = buffer;
	pb.buffer_cursor = 0;

	yaw = packed_buffer_extract_u8(&pb);
	switch (yaw) {
	case YAW_LEFT:
		yaw_func(c, -1);
		break;
	case YAW_RIGHT:
		yaw_func(c, 1);
		break;
	default:
		break;
	}
	return 0;
}

static int process_request_torpedo(struct game_client *c)
{
	struct snis_entity *ship = &go[c->shipid];
	double vx, vy;

	if (ship->tsd.ship.torpedoes <= 0)
		return 0;

	vx = TORPEDO_VELOCITY * sin(ship->tsd.ship.gun_heading);
	vy = TORPEDO_VELOCITY * -cos(ship->tsd.ship.gun_heading);
	pthread_mutex_lock(&universe_mutex);
	add_torpedo(ship->x, ship->y, vx, vy, ship->tsd.ship.gun_heading); 
	ship->tsd.ship.torpedoes--;
	pthread_mutex_unlock(&universe_mutex);
	return 0;
}

static void process_instructions_from_client(struct game_client *c)
{
	int rc;
	uint16_t opcode;

	rc = snis_readsocket(c->socket, &opcode, sizeof(opcode));
	if (rc != 0)
		goto protocol_error;
	opcode = ntohs(opcode);

	switch (opcode) {
		case OPCODE_REQUEST_TORPEDO:
			process_request_torpedo(c);
			break;
		case OPCODE_REQUEST_YAW:
			rc = process_request_yaw(c, do_yaw);
			if (rc)
				goto protocol_error;
			break;
		case OPCODE_REQUEST_GUNYAW:
			rc = process_request_yaw(c, do_gun_yaw);
			if (rc)
				goto protocol_error;
			break;
		case OPCODE_REQUEST_THRUST:
			rc = process_request_thrust(c);
			if (rc)
				goto protocol_error;
			break;
		case OPCODE_NOOP:
			break;
		default:
			goto protocol_error;
	}
	return;

protocol_error:
	printf("Protocol error in process_instructions_from_client\n");
	shutdown(c->socket, SHUT_RDWR);
	close(c->socket);
	c->socket = -1;
	return;
}

static void *per_client_read_thread(__attribute__((unused)) void /* struct game_client */ *client)
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
	printf("client reader thread exiting\n");
	return NULL;
}

static void write_queued_updates_to_client(struct game_client *c)
{
	/* write queued updates to client */
	int rc;
	uint16_t noop = 0xffff;

	struct packed_buffer *buffer;

	printf("writing queued updates to client, c=%p, c->client_write_queue = %p\n", (void *) c,
			(void *) &c->client_write_queue);
	/*  packed_buffer_queue_print(&c->client_write_queue); */
	buffer = packed_buffer_queue_combine(&c->client_write_queue, &c->client_write_queue_mutex);
	if (buffer->buffer_size > 0) {
		printf("Writing data to client\n");
		rc = snis_writesocket(c->socket, buffer->buffer, buffer->buffer_size);
		if (rc != 0) {
			printf("writesocket failed, rc= %d, errno = %d(%s)\n", 
				rc, errno, strerror(errno));
			goto badclient;
		}
	} else {
		/* no-op, just so we know if client is still there */
		rc = snis_writesocket(c->socket, &noop, sizeof(noop));
		if (rc != 0) {
			printf("(noop) writesocket failed, rc= %d, errno = %d(%s)\n", 
				rc, errno, strerror(errno));
			goto badclient;
		}
	}
	packed_buffer_free(buffer);
	return;

badclient:
	shutdown(c->socket, SHUT_RDWR);
	close(c->socket);
	c->socket = -1;
}

static void send_update_ship_packet(struct game_client *c,
	struct snis_entity *o);
static void send_update_planet_packet(struct game_client *c,
	struct snis_entity *o);
static void send_update_starbase_packet(struct game_client *c,
	struct snis_entity *o);
static void send_update_torpedo_packet(struct game_client *c,
	struct snis_entity *o);
static void send_update_laser_packet(struct game_client *c,
	struct snis_entity *o);

static void queue_up_client_object_update(struct game_client *c, struct snis_entity *o)
{
	switch(o->type) {
	case OBJTYPE_SHIP1:
		send_update_ship_packet(c, o);
		break;
	case OBJTYPE_SHIP2:
		break;
	case OBJTYPE_PLANET:
		send_update_planet_packet(c, o);
		break;
	case OBJTYPE_STARBASE:
		send_update_starbase_packet(c, o);
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
	default:
		break;
	}
}

static void queue_up_client_updates(struct game_client *c)
{
	int i;
	int count;

	count = 0;
	printf("server: queue_up_client_updates\n");
	pthread_mutex_lock(&universe_mutex);
	for (i = 0; i <= snis_object_pool_highest_object(pool); i++) {
		/* printf("obj %d: a=%d, ts=%u, uts%u, type=%hhu\n",
			i, go[i].alive, go[i].timestamp, universe_timestamp, go[i].type); */
		if (go[i].alive && go[i].timestamp > c->timestamp) {
			queue_up_client_object_update(c, &go[i]);
			count++;
		}
	}
	pthread_mutex_unlock(&universe_mutex);
	printf("queued up %d updates for client\n", count);
}

static void queue_up_client_id(struct game_client *c)
{
	/* tell the client what his ship id is. */
	struct packed_buffer *pb;

	pb = packed_buffer_allocate(sizeof(struct update_ship_packet));
	packed_buffer_append_u16(pb, OPCODE_ID_CLIENT_SHIP);
	packed_buffer_append_u32(pb, c->shipid);
	packed_buffer_queue_add(&c->client_write_queue, pb, &c->client_write_queue_mutex);
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
		printf("server: top of loop in per_client_write_thread\n");
		rc = clock_gettime(CLOCK_MONOTONIC, &time1);
		printf("server: queuing up client updates\n");
		queue_up_client_updates(c);
		printf("server: queued up client updates, writing updates to client\n");
		write_queued_updates_to_client(c);
		if (c->socket < 0)
			break;
		c->timestamp = universe_timestamp;
		printf("server: wrote updates to client, sleeping.\n");
		rc = clock_gettime(CLOCK_MONOTONIC, &time2);
		/* snis_sleep(&time1, &time2, &tenth_second); */ /* sleep for 1/10th sec - (time2 - time1) */
		sleep_tenth_second();
		printf("server: awakened.\n");
	}
	printf("client writer thread exiting.\n");
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
	printf("protocol read...'%s'\n", protocol_version);
	if (strcmp(protocol_version, SNIS_PROTOCOL_VERSION) != 0)
		return -1;
	printf("protocol verified.\n");
	return 0;
}

static int lookup_player(unsigned char *shipname, unsigned char *password)
{
	int i;

	pthread_mutex_lock(&universe_mutex);
	for (i = 0; i < nbridges; i++) {
		if (strcmp((const char *) shipname, (const char *) bridgelist[i].shipname) == 0 &&
			strcmp((const char *) password, (const char *) bridgelist[i].password) == 0) {
			pthread_mutex_unlock(&universe_mutex);
			return bridgelist[i].shipid;
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
	for (i = 0; i < len; i++)
		if (!isalnum(word[i]))
			return 1;
	return 0;
}

static void send_update_ship_packet(struct game_client *c,
	struct snis_entity *o)
{
	struct packed_buffer *pb;
	uint32_t x, y;
	int32_t vx, vy;
	uint32_t heading, gun_heading;

	x = (uint32_t) ((o->x / XUNIVERSE_DIMENSION) * (double) UINT32_MAX);
	y = (uint32_t) ((o->y / YUNIVERSE_DIMENSION) * (double) UINT32_MAX);
	vx = (int32_t) ((o->vx / XUNIVERSE_DIMENSION) * (double) INT32_MAX);
	vy = (int32_t) ((o->vy / YUNIVERSE_DIMENSION) * (double) INT32_MAX);
	heading = (uint32_t) (o->heading / 360.0 * (double) UINT32_MAX);
	gun_heading = (uint32_t) (o->tsd.ship.gun_heading / 360.0 * (double) UINT32_MAX);

	pb = packed_buffer_allocate(sizeof(struct update_ship_packet));
	packed_buffer_append_u16(pb, OPCODE_UPDATE_SHIP);
	packed_buffer_append_u32(pb, o->id);
	packed_buffer_append_u32(pb, o->alive);
	packed_buffer_append_u32(pb, x);
	packed_buffer_append_u32(pb, y);
	packed_buffer_append_u32(pb, (uint32_t) vx);
	packed_buffer_append_u32(pb, (uint32_t) vy);
	packed_buffer_append_u32(pb, heading);
	packed_buffer_append_u32(pb, o->tsd.ship.torpedoes);
	packed_buffer_append_u32(pb, o->tsd.ship.energy);
	packed_buffer_append_u32(pb, o->tsd.ship.shields);
	packed_buffer_append_u32(pb, gun_heading);

	packed_buffer_queue_add(&c->client_write_queue, pb, &c->client_write_queue_mutex);
}

static void send_update_planet_packet(struct game_client *c,
	struct snis_entity *o)
{
	struct packed_buffer *pb;
	uint32_t x, y;

	printf("Sending planet %u\n", o->id);
	x = (uint32_t) ((o->x / XUNIVERSE_DIMENSION) * (double) UINT32_MAX);
	y = (uint32_t) ((o->y / YUNIVERSE_DIMENSION) * (double) UINT32_MAX);
	pb = packed_buffer_allocate(sizeof(struct update_planet_packet));
	packed_buffer_append_u16(pb, OPCODE_UPDATE_PLANET);
	packed_buffer_append_u32(pb, o->id);
	packed_buffer_append_u32(pb, x);
	packed_buffer_append_u32(pb, y);
	packed_buffer_queue_add(&c->client_write_queue, pb, &c->client_write_queue_mutex);
}

static void send_update_starbase_packet(struct game_client *c,
	struct snis_entity *o)
{
	struct packed_buffer *pb;
	uint32_t x, y;

	printf("Sending starbase %u\n", o->id);
	x = (uint32_t) ((o->x / XUNIVERSE_DIMENSION) * (double) UINT32_MAX);
	y = (uint32_t) ((o->y / YUNIVERSE_DIMENSION) * (double) UINT32_MAX);
	pb = packed_buffer_allocate(sizeof(struct update_starbase_packet));
	packed_buffer_append_u16(pb, OPCODE_UPDATE_STARBASE);
	packed_buffer_append_u32(pb, o->id);
	packed_buffer_append_u32(pb, x);
	packed_buffer_append_u32(pb, y);
	packed_buffer_queue_add(&c->client_write_queue, pb, &c->client_write_queue_mutex);
}

static void send_update_torpedo_packet(struct game_client *c,
	struct snis_entity *o)
{
	struct packed_buffer *pb;
	uint32_t x, y, vx, vy;

	x = (uint32_t) ((o->x / XUNIVERSE_DIMENSION) * (double) UINT32_MAX);
	y = (uint32_t) ((o->y / YUNIVERSE_DIMENSION) * (double) UINT32_MAX);
	vx = (uint32_t) ((o->vx / XUNIVERSE_DIMENSION) * (double) UINT32_MAX);
	vy = (uint32_t) ((o->vy / YUNIVERSE_DIMENSION) * (double) UINT32_MAX);
	pb = packed_buffer_allocate(sizeof(struct update_torpedo_packet));
	packed_buffer_append_u16(pb, OPCODE_UPDATE_TORPEDO);
	packed_buffer_append_u32(pb, o->id);
	packed_buffer_append_u32(pb, x);
	packed_buffer_append_u32(pb, y);
	packed_buffer_append_u32(pb, vx);
	packed_buffer_append_u32(pb, vy);
	packed_buffer_queue_add(&c->client_write_queue, pb, &c->client_write_queue_mutex);
}

static void send_update_laser_packet(struct game_client *c,
	struct snis_entity *o)
{
}

static int add_new_player(struct game_client *c)
{
	int rc;
	struct add_player_packet app;

	rc = snis_readsocket(c->socket, &app, sizeof(app));
	if (rc)
		return rc;
	app.opcode = ntohs(app.opcode);
	if (app.opcode != OPCODE_UPDATE_PLAYER) {
		printf("bad opcode %d\n", app.opcode);
		goto protocol_error;
	}
	app.shipname[19] = '\0';
	app.password[19] = '\0';
	if (app.role > ROLE_MAXROLE) {
		printf("server: role out of range: %d\n", app.role);
		goto protocol_error;
	}

	if (insane(app.shipname, 20) || insane(app.password, 20)) {
		printf("Bad ship name or password\n");
		goto protocol_error;
	}

	c->shipid = lookup_player(app.shipname, app.password);
	c->role = app.role;
	if (c->shipid == -1) { /* did not find our bridge, have to make a new one. */
		double x, y;

		x = XUNIVERSE_DIMENSION * (double) rand() / (double) RAND_MAX;
		y = YUNIVERSE_DIMENSION * (double) rand() / (double) RAND_MAX;
		pthread_mutex_lock(&universe_mutex);
		c->shipid = add_player(x, y, 0.0, 0.0, 0.0);
		memset(&bridgelist[nbridges], 0, sizeof(bridgelist[nbridges]));
		strcpy((char *) bridgelist[nbridges].shipname, (const char *) app.shipname);
		strcpy((char *) bridgelist[nbridges].password, (const char *) app.password);
		bridgelist[nbridges].shipid = c->shipid;
	
		nbridges++;
		
		pthread_mutex_unlock(&universe_mutex);
	}
	queue_up_client_id(c);
	return 0;

protocol_error:
	printf("server: protocol error, closing socket %d\n", c->socket);
	close(c->socket);
	return -1;
}

/* Creates a thread for each incoming connection... */
static void service_connection(int connection)
{
	int i, flag = 1;

	printf("snis_server: servicing snis_client connection %d\n", connection);
        /* get connection moved off the stack so that when the thread needs it,
	 * it's actually still around. 
	 */

	i = setsockopt(connection, IPPROTO_TCP, TCP_NODELAY, (char *) &flag, sizeof(int));
	if (i < 0)
		printf("setsockopt failed.\n");

	client_lock();
	if (nclients >= MAXCLIENTS) {
		client_unlock();
		printf("Too many clients.\n");
		return;
	}
	i = nclients;

	if (verify_client_protocol(connection)) {
		printf("protocol error\n");
		close(connection);
		return;
	}

	client[i].socket = connection;
	client[i].timestamp = 0;  /* newborn client, needs everything */

	printf("add new player\n");

	pthread_mutex_init(&client[i].client_write_queue_mutex, NULL);
	packed_buffer_queue_init(&client[i].client_write_queue);

	add_new_player(&client[i]);

	pthread_attr_init(&client[i].read_attr);
	pthread_attr_setdetachstate(&client[i].read_attr, PTHREAD_CREATE_JOINABLE);
	pthread_attr_init(&client[i].write_attr);
	pthread_attr_setdetachstate(&client[i].write_attr, PTHREAD_CREATE_JOINABLE);
        (void) pthread_create(&client[i].read_thread,
		&client[i].read_attr, per_client_read_thread, (void *) &client[i]);
        (void) pthread_create(&client[i].write_thread,
		&client[i].write_attr, per_client_write_thread, (void *) &client[i]);
	nclients++;
	client_unlock();


	printf("bottom of 'service connection'\n");
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

        printf("snis_server starting\n");

	/* Bind "rendezvous" socket to a random port to listen for connections. */
	while (1) {

		/* 
		 * choose a random port in the "Dynamic and/or Private" range
		 * see http://www.iana.org/assignments/port-numbers
		 */
		port = snis_randn(65335 - 49152) + 49151;
		printf("Trying port %d\n", port);
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
			fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(s));
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
	printf("snis_server listening for connections on port %d\n", port);
	listener_port = port;

	/* Listen for incoming connections... */
	rc = listen(rendezvous, SOMAXCONN);
	if (rc < 0) {
		fprintf(stderr, "listen() failed: %s\n", strerror(errno));
		exit(1);
	}

	/* Notify other threads that the listener thread is ready... */
	(void) pthread_mutex_lock(&listener_mutex);
	pthread_cond_signal(&listener_started);
	(void) pthread_mutex_unlock(&listener_mutex);

	while (1) {
		remote_addr_len = sizeof(remote_addr);
		printf("Accepting connection...\n");
		connection = accept(rendezvous, (struct sockaddr *) &remote_addr, &remote_addr_len);
		printf("accept returned %d\n", connection);
		if (connection < 0) {
			/* handle failed connection... */
			fprintf(stderr, "accept() failed: %s\n", strerror(errno));
			ssgl_sleep(1);
			continue;
		}
		if (remote_addr_len != sizeof(remote_addr)) {
			fprintf(stderr, "strange socket address length %d\n", remote_addr_len);
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

	/* Setup to wait for the listener thread to become ready... */
	pthread_cond_init (&listener_started, NULL);
        (void) pthread_mutex_lock(&listener_mutex);

	/* Create the listener thread... */
        pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
        (void) pthread_create(&thread, &attr, listener_thread, NULL);

	/* Wait for the listener thread to become ready... */
	pthread_cond_wait(&listener_started, &listener_mutex);
	(void) pthread_mutex_unlock(&listener_mutex);
	printf("Listener started.\n");
	
	return listener_port;
}


static void move_objects(void)
{
	int i;

	pthread_mutex_lock(&universe_mutex);
	universe_timestamp++;
	for (i = 0; i <= snis_object_pool_highest_object(pool); i++)
		if (go[i].alive)
			go[i].move(&go[i]);
	pthread_mutex_unlock(&universe_mutex);

}

static void register_with_game_lobby(int port,
	char *servernick, char *gameinstance, char *location)
{
	struct ssgl_game_server gs;

	memset(&gs, 0, sizeof(gs));
	printf("port = %hu\n", port);
	gs.port = htons(port);
	printf("gs.port = %hu\n", gs.port);
		
	strncpy(gs.server_nickname, servernick, 14);
	strncpy(gs.game_instance, gameinstance, 19);
	strncpy(gs.location, location, 19);
	strcpy(gs.game_type, "SNIS");

	if (ssgl_get_primary_host_ip_addr(&gs.ipaddr) != 0)
		fprintf(stderr, "Failed to get local ip address.\n");

	printf("Registering game server\n");
#define LOBBYHOST "localhost"
	(void) ssgl_register_gameserver(LOBBYHOST, &gs, &lobbythread);
	printf("Game server registered.\n");
	return;	
}

void usage(void)
{
	fprintf(stderr, "snis_server gameinstance servernick location\n");
	fprintf(stderr, "For example: snis_server 'steves game' zuul Houston\n");
	exit(0);
}

int main(int argc, char *argv[])
{
	int port, rc, i;
	struct timespec time1;
	struct timespec time2;
	struct timespec thirtieth_second;

	if (argc < 4) 
		usage();

	memset(&thirtieth_second, 0, sizeof(thirtieth_second));
	thirtieth_second.tv_nsec = 33333333; /* 1/30th second */

	make_universe();
	port = start_listener_thread();

	ignore_sigpipe();	
	register_with_game_lobby(port, argv[2], argv[1], argv[3]);

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

	if (rc) /* satisfy compiler */
		return 0; 
	return 0;
}
