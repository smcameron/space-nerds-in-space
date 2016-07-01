/*
	Copyright (C) 2015 Stephen M. Cameron
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

/*****************************************************************************

This program is used to persist player ships across instances of snis_server.
Each instance of snis_server connects to snis_multiverse (which it locates by
way of ssgl_server) and periodically sends player ship information which is
persisted in a simple database by snis_multiverse.

*****************************************************************************/
#define _GNU_SOURCE
#include "snis_multiverse.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <errno.h>
#include <pthread.h>
#include <assert.h>
#include <stdarg.h>
#include <limits.h>
#include <dirent.h>

#include "build_bug_on.h"
#include "snis_marshal.h"
#include "quat.h"
#include "ssgl/ssgl.h"
#include "snis_log.h"
#include "snis_socket_io.h"
#include "snis.h"
#include "mathutils.h"
#include "snis_packet.h"
#include "snis.h"
#include "snis_hash.h"
#include "quat.h"
#include "string-utils.h"
#include "key_value_parser.h"
#include "build_bug_on.h"
#include "snis_bridge_update_packet.h"

static char *database_root = "./snisdb";
static const int database_mode = 0744;
static pthread_mutex_t data_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t listener_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t listener_started;
static pthread_mutex_t service_mutex = PTHREAD_MUTEX_INITIALIZER;
int listener_port = -1;
static int default_snis_multiverse_port = -1; /* -1 means choose a random port */
pthread_t lobbythread;
char *lobbyserver = NULL;
static int snis_log_level = 2;
int nconnections = 0;

#define MAX_BRIDGES 1024
#define MAX_CONNECTIONS 100
#define MAX_STARSYSTEMS 100

static struct starsystem_info { /* one of these per connected snis_server */
	int socket;
	int refcount;
	pthread_t read_thread;
	pthread_t write_thread;
	pthread_attr_t read_attr, write_attr;
	struct packed_buffer_queue write_queue;
	pthread_mutex_t write_queue_mutex;
	char starsystem_name[20]; /* corresponds to "location" parameter of snis_server */
} starsystem[MAX_STARSYSTEMS] = { { 0 } };
static int nstarsystems = 0;

static struct bridge_info {
	unsigned char pwdhash[20];
	int occupied;
	int32_t initialized;
	struct timeval created_on;
	struct timeval last_seen;
	struct snis_entity entity;
} ship[MAX_BRIDGES];
int nbridges = 0;

#define INCLUDE_BRIDGE_INFO_FIELDS 1
#include "snis_entity_key_value_specification.h"

static void remove_starsystem(struct starsystem_info *ss)
{
}

/* assumes service_mutex held. */
static void put_starsystem(struct starsystem_info *ss)
{
	assert(ss->refcount > 0);
	ss->refcount--;
	if (ss->refcount == 0)
		remove_starsystem(ss);
}

/* assumes service_mutex held. */
static void get_starsystem(struct starsystem_info *ss)
{
	ss->refcount++;
}

static void lock_get_starsystem(struct starsystem_info *ss)
{
	pthread_mutex_lock(&service_mutex);
	get_starsystem(ss);
	pthread_mutex_unlock(&service_mutex);
}

static void lock_put_starsystem(struct starsystem_info *ss)
{
	pthread_mutex_lock(&service_mutex);
	put_starsystem(ss);
	pthread_mutex_unlock(&service_mutex);
}

static int make_dir(char *path)
{
	int rc = mkdir(path, database_mode);
	if (rc != 0 && errno != EEXIST)
		return rc;
	return 0;
}

static void sync_file(FILE *f)
{
	int rc, fd;

	fd = fileno(f);
	if (fd < 0) {
		fprintf(stderr, "snis_multiverse: sync_file: can't get file descriptor\n");
		return;
	}
	rc = fsync(fd);
	if (rc < 0)
		fprintf(stderr, "snis_multiverse: fsync: %s\n", strerror(errno));
}

static void sync_dir(char *path)
{
	DIR *d = opendir(path);
	int fd;

	if (!d) {
		fprintf(stderr, "snis_multiverse: open_dir(%s): %s\n", path, strerror(errno));
		return;
	}
	fd = dirfd(d);
	if (fd < 0) {
		fprintf(stderr, "snis_multiverse: dirfd(%s): %s\n", path, strerror(errno));
		return;
	}
	fsync(fd);
	closedir(d);
}

static int write_bridge_info(FILE *f, struct bridge_info *b)
{
	void *base_address[] = { &b->entity, b };

	return key_value_write_lines(f, snis_entity_kvs, base_address);
}

static void print_hash(char *s, unsigned char *pwdhash)
{
	int i;

	fprintf(stderr, "snis_multiverse: %s", s);
	for (i = 0; i < 20; i++)
		fprintf(stderr, "%02x", pwdhash[i]);
	fprintf(stderr, "\n");
}

static int save_bridge_info(struct bridge_info *b)
{
	int rc;
	char dirpath[PATH_MAX], path[PATH_MAX], path2[PATH_MAX];
	char dir1[5], dir2[5];
	unsigned char hexpwdhash[41];
	FILE *f;

	fprintf(stderr, "save_bridge_info 1\n");
	memset(dir1, 0, sizeof(dir1));
	memset(dir2, 0, sizeof(dir2));

	snis_format_sha1_hash(b->pwdhash, hexpwdhash, 41);
	memcpy(&dir1[0], &hexpwdhash[0], 4);
	memcpy(&dir2[0], &hexpwdhash[4], 4);

	sprintf(path, "%s/%s", database_root, dir1);
	if (make_dir(path)) {
		fprintf(stderr, "snis_multiverse: mkdir failed: %s: %s\n", path, strerror(errno));
		return -1;
	}
	sprintf(dirpath, "%s/%s/%s", database_root, dir1, dir2);
	if (make_dir(dirpath)) {
		fprintf(stderr, "snis_multiverse: mkdir failed: %s: %s\n", dirpath, strerror(errno));
		return -1;
	}
	sprintf(path, "%s/%s.update", dirpath, hexpwdhash);
	sprintf(path2, "%s/%s.data", dirpath, hexpwdhash);
	f = fopen(path, "w+");
	if (!f) {
		fprintf(stderr, "snis_multiverse: fopen %s failed: %s\n", path, strerror(errno));
		return -1;
	}
	rc = write_bridge_info(f, b);
	if (rc) {
		fprintf(stderr, "snis_multiverse: failed to write to %s\n", path);
		fclose(f);
		return -1;
	}
	sync_file(f);
	fclose(f);
	rc = rename(path, path2);
	sync_dir(dirpath);
	if (rc) {
		fprintf(stderr, "snis_multiverse: failed to rename %s to %s: %s\n",
			path, path2, strerror(errno));
		return -1;
	}
	fprintf(stderr, "snis_multiverse: wrote bridge info to %s\n", path2);
	return 0;
}

static int restore_bridge_info(const char *filename, struct bridge_info *b, unsigned char *pwdhash)
{
	char *contents;
	int rc;
	void *base_address[] = { &b->entity, b };

	contents = slurp_file(filename, NULL);
	if (!contents)
		return -1;
	memcpy(b->pwdhash, pwdhash, 20);
	rc = key_value_parse_lines(snis_entity_kvs, contents, base_address);
	free(contents);
	return rc;
}

static void usage()
{
	fprintf(stderr, "usage: snis_multiverse lobbyserver servernick location\n");
	exit(1);
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

static void log_client_info(int level, int connection, char *info)
{
	char client_ip[50];

	if (level < snis_log_level)
		return;

	memset(client_ip, 0, sizeof(client_ip));
	get_peer_name(connection, client_ip);
	snis_log(level, "snis_multiverse: connection from %s: %s",
			client_ip, info);
}

static int verify_client_protocol(int connection)
{
	int rc;
	char protocol_version[sizeof(SNIS_MULTIVERSE_VERSION) + 1];
	const int len = sizeof(SNIS_MULTIVERSE_VERSION) - 1;

	fprintf(stderr, "snis_multiverse: writing client protocol version (len = %d) zzz 1\n", len);
	rc = snis_writesocket(connection, SNIS_MULTIVERSE_VERSION, len);
	if (rc < 0)
		return -1;
	fprintf(stderr, "snis_multiverse: snis_writesocket returned %d\n", rc);
	fprintf(stderr, "snis_multiverse: reading client protocol version zzz (len = %d)2\n", len);
	memset(protocol_version, 0, sizeof(protocol_version));
	rc = snis_readsocket(connection, protocol_version, len);
	if (rc < 0) {
		fprintf(stderr, "snis_multiverse: snis_readsocket failed: %d (%d:%s)\n",
			rc, errno, strerror(errno));
		return rc;
	}
	fprintf(stderr, "snis_multiverse: verify client protocol zzz 3\n");
	protocol_version[len] = '\0';
	snis_log(SNIS_INFO, "snis_multiverse: protocol read...'%s'\n", protocol_version);
	if (strcmp(protocol_version, SNIS_MULTIVERSE_VERSION) != 0) {
		fprintf(stderr, "snis_multiverse: verify client protocol zzz 4\n");
		return -1;
	}
	fprintf(stderr, "snis_multiverse: verify client protocol zzz 5\n");
	snis_log(SNIS_INFO, "snis_multiverse: protocol verified.\n");
	return 0;
}

static int lookup_bridge(void)
{
	return 0;
}

/* TODO: this is very similar to snis_server's version of same function */
static int __attribute__((unused)) read_and_unpack_buffer(struct starsystem_info *ss, unsigned char *buffer, char *format, ...)
{
	va_list ap;
	struct packed_buffer pb;
	int rc, size = calculate_buffer_size(format);

	rc = snis_readsocket(ss->socket, buffer, size);
	if (rc != 0)
		return rc;
	packed_buffer_init(&pb, buffer, size);
	va_start(ap, format);
	rc = packed_buffer_extract_va(&pb, format, ap);
	va_end(ap);
	return rc;
}

static int read_and_unpack_fixed_size_buffer(struct starsystem_info *ss,
			unsigned char *buffer, int size, char *format, ...)
{
	va_list ap;
	struct packed_buffer pb;
	int rc;
	fprintf(stderr, "KKKKKKKKKKKK size = %d\n", size);

	rc = snis_readsocket(ss->socket, buffer, size);
	if (rc != 0)
		return rc;
	packed_buffer_init(&pb, buffer, size);
	va_start(ap, format);
	fprintf(stderr, "KKKKKKKK format = '%s'\n", format);
	rc = packed_buffer_extract_va(&pb, format, ap);
	va_end(ap);
	return rc;
}

static int lookup_ship_by_hash(unsigned char *hash)
{
	int i;
	unsigned char phash[100];

	snis_format_sha1_hash(hash, phash, 100);
	fprintf(stderr, "snis_multiverse: looking up hash '%s'\n", phash);

	for (i = 0; i < nbridges; i++) {
		snis_format_sha1_hash(ship[i].pwdhash, phash, 100);
		fprintf(stderr, "snis_multiverse: checking against '%s'\n", phash);
		if (memcmp(ship[i].pwdhash, hash, 20) == 0) {
			fprintf(stderr, "snis_multiverse: match hash '%s'\n", phash);
			return i;
		}
	}
	fprintf(stderr, "snis_multiverse: no match found\n");
	return -1;
}

static int update_bridge(struct starsystem_info *ss)
{
	unsigned char pwdhash[20];
	int i, rc;
	unsigned char buffer[250];
	struct packed_buffer pb;
	struct snis_entity *o;

#define bytes_to_read (sizeof(struct update_ship_packet) - 9 + 25 + 5 + \
			sizeof(struct power_model_data) + \
			sizeof(struct power_model_data) - 1)

	fprintf(stderr, "snis_multiverse: update bridge 1\n");
	memset(buffer, 0, sizeof(buffer));
	memset(pwdhash, 0, sizeof(pwdhash));
	rc = read_and_unpack_fixed_size_buffer(ss, buffer, 20, "r", pwdhash, (uint16_t) 20);
	if (rc != 0)
		return rc;
	print_hash("update bridge 2, read 20 bytes: ", pwdhash);
	BUILD_ASSERT(sizeof(buffer) > bytes_to_read);
	memset(buffer, 0, sizeof(buffer));
	rc = snis_readsocket(ss->socket, buffer, bytes_to_read);
	if (rc != 0)
		return rc;
	fprintf(stderr, "snis_multiverse: update bridge 3\n");
	pthread_mutex_lock(&data_mutex);
	i = lookup_ship_by_hash(pwdhash);
	if (i < 0) {
		fprintf(stderr, "snis_multiverse: Unknown ship hash\n");
		pthread_mutex_unlock(&data_mutex);
		return rc;
	}
	fprintf(stderr, "snis_multiverse: update bridge 4\n");

	o = &ship[i].entity;
	if (!o->tsd.ship.damcon) {
		o->tsd.ship.damcon = malloc(sizeof(*o->tsd.ship.damcon));
		memset(o->tsd.ship.damcon, 0, sizeof(*o->tsd.ship.damcon));
	}

	packed_buffer_init(&pb, buffer, bytes_to_read);
	unpack_bridge_update_packet(o, &pb);
	ship[i].initialized = 1;
	pthread_mutex_unlock(&data_mutex);
	rc = 0;
	fprintf(stderr, "snis_multiverse: update bridge 10\n");
	return rc;
}

static int create_new_ship(unsigned char pwdhash[20])
{
	fprintf(stderr, "snis_multiverse: create_new_ship 1 (nbridges = %d, MAX=%d)\n", nbridges, MAX_BRIDGES);
	if (nbridges >= MAX_BRIDGES)
		return -1;
	memset(&ship[nbridges], 0, sizeof(ship[nbridges]));
	memcpy(ship[nbridges].pwdhash, pwdhash, 20);
	nbridges++;
	fprintf(stderr, "snis_multiverse: added new bridge, nbridges = %d\n", nbridges);
	return 0;
}

static void send_bridge_update_to_snis_server(struct starsystem_info *ss, unsigned char *pwdhash)
{
	int i;
	struct packed_buffer *pb;
	struct snis_entity *o;

	pthread_mutex_lock(&data_mutex);
	i = lookup_ship_by_hash(pwdhash);
	if (i < 0)
		return;
	o = &ship[i].entity;

	/* Update the ship */
	pb = build_bridge_update_packet(o, pwdhash);
	pthread_mutex_unlock(&data_mutex);
	packed_buffer_queue_add(&ss->write_queue, pb, &ss->write_queue_mutex);
}

static int verify_existence(struct starsystem_info *ss, int should_already_exist)
{
	unsigned char pwdhash[20];
	unsigned char buffer[200];
	struct packed_buffer *pb;
	int i, rc;
	unsigned char pass;
	unsigned char printable_hash[100];
	int send_update = 0;

	fprintf(stderr, "snis_multiverse: verify_existence 1: should %salready exist\n",
			should_already_exist ? "" : "not ");
	rc = read_and_unpack_fixed_size_buffer(ss, buffer, 20, "r", pwdhash, (uint16_t) 20);
	if (rc != 0)
		return rc;
	snis_format_sha1_hash(pwdhash, printable_hash, 100);
	printable_hash[40] = '\0';
	pthread_mutex_lock(&data_mutex);
	fprintf(stderr, "snis_multiverse: lookup hash %s\n", printable_hash);
	i = lookup_ship_by_hash(pwdhash);
	fprintf(stderr, "snis_multiverse: verify existence pwdhash lookup returns %d\n", i);
	if (i < 0) {
		if (should_already_exist) { /* It doesn't exist, but it should */
			fprintf(stderr, "snis_multiverse: hash %s does not exist, but should.\n", printable_hash);
			pass = SNISMV_VERIFICATION_RESPONSE_FAIL;
		} else { /* doesn't exist, but we asked to create it, so that is expected */
			fprintf(stderr, "snis_multiverse: hash %s does not exist, as expected.\n", printable_hash);
			pass = SNISMV_VERIFICATION_RESPONSE_PASS;
			rc = create_new_ship(pwdhash);
			if (rc) /* We failed to create it. */
				pass = SNISMV_VERIFICATION_RESPONSE_TOO_MANY_BRIDGES;
		}
	} else {
		/* It exists, pass if it should exist, fail otherwise */
		if (should_already_exist) {
			fprintf(stderr, "snis_multiverse: hash %s exists, as expected.\n", printable_hash);
			pass = SNISMV_VERIFICATION_RESPONSE_PASS;
			send_update = 1;
		} else {
			fprintf(stderr, "snis_multiverse: hash %s exists, but should not.\n", printable_hash);
			pass = SNISMV_VERIFICATION_RESPONSE_FAIL;
		}
	}
	print_hash("checking hash ", pwdhash);
	fprintf(stderr, "snis_multiverse: verify existence pass=%d\n", pass);
	pthread_mutex_unlock(&data_mutex);
	pb = packed_buffer_allocate(22);
	packed_buffer_append(pb, "bbr", SNISMV_OPCODE_VERIFICATION_RESPONSE, pass, pwdhash, 20);
	packed_buffer_queue_add(&ss->write_queue, pb, &ss->write_queue_mutex);

	if (send_update)
		send_bridge_update_to_snis_server(ss, pwdhash);

	return 0;
}

static void process_instructions_from_snis_server(struct starsystem_info *ss)
{
	uint8_t opcode;
	int rc;

	rc = snis_readsocket(ss->socket, &opcode, 1);
	if (rc < 0)
		goto bad_client;
	switch (opcode) {
	case SNISMV_OPCODE_NOOP:
		break;
	case SNISMV_OPCODE_LOOKUP_BRIDGE:
		rc = lookup_bridge();
		if (rc)
			goto bad_client;
		break;
	case SNISMV_OPCODE_UPDATE_BRIDGE:
		rc = update_bridge(ss);
		if (rc)
			goto bad_client;
		break;
	case SNISMV_OPCODE_VERIFY_CREATE:
		rc = verify_existence(ss, 0);
		if (rc)
			goto bad_client;
		break;
	case SNISMV_OPCODE_VERIFY_EXISTS:
		rc = verify_existence(ss, 1);
		if (rc)
			goto bad_client;
		break;
	default:
		fprintf(stderr, "snis_multiverse: unknown opcode %hhu from socket %d\n",
			opcode,  ss->socket);
		goto bad_client;
	}
	return;

bad_client:
	fprintf(stderr, "snis_multiverse: bad client, disconnecting socket %d\n", ss->socket);
	shutdown(ss->socket, SHUT_RDWR);
	close(ss->socket);
	ss->socket = -1;
}

static void *starsystem_read_thread(void /* struct starsystem_info */ *starsystem)
{
	struct starsystem_info *ss = (struct starsystem_info *) starsystem;

	lock_get_starsystem(ss);
	while (1) {
		process_instructions_from_snis_server(ss);
		if (ss->socket < 0)
			break;
	}
	lock_put_starsystem(ss);
	return NULL;
}

static void write_queued_updates_to_snis_server(struct starsystem_info *ss)
{
	int rc;
	uint8_t noop = SNISMV_OPCODE_NOOP;

	struct packed_buffer *buffer;

	buffer = packed_buffer_queue_combine(&ss->write_queue, &ss->write_queue_mutex);
	if (buffer) {
		fprintf(stderr, "snis_multiverse: writing buffer %d bytes.\n", buffer->buffer_size);
		rc = snis_writesocket(ss->socket, buffer->buffer, buffer->buffer_size);
		fprintf(stderr, "snis_multiverse: wrote buffer %d bytes, rc = %d\n",
				buffer->buffer_size, rc);
		packed_buffer_free(buffer);
		if (rc != 0) {
			snis_log(SNIS_ERROR, "snis_multiverse: writesocket failed, rc= %d, errno = %d(%s)\n",
				rc, errno, strerror(errno));
			goto badclient;
		}
	} else {
		/* Nothing to write, send a no-op just so we know if client is still there */
		fprintf(stderr, "snis_multiverse: writing noop.\n");
		rc = snis_writesocket(ss->socket, &noop, sizeof(noop));
		fprintf(stderr, "snis_multiverse: wrote noop, rc = %d.\n", rc);
		if (rc != 0) {
			fprintf(stderr, "snis_multiverse: (noop) writesocket failed, rc= %d, errno = %d(%s)\n",
				rc, errno, strerror(errno));
			goto badclient;
		}
	}
	fprintf(stderr, "snis_multiverse: done writing for now\n");
	return;

badclient:
	fprintf(stderr, "snis_multiverse: bad client, disconnecting\n");
	shutdown(ss->socket, SHUT_RDWR);
	close(ss->socket);
	ss->socket = -1;
}

static void *starsystem_write_thread(void /* struct starsystem_info */ *starsystem)
{
	struct starsystem_info *ss = (struct starsystem_info *) starsystem;

	lock_get_starsystem(ss);
	while (1) {
		if (ss->socket < 0)
			break;
		write_queued_updates_to_snis_server(ss);
		sleep(5);
	}
	lock_put_starsystem(ss);
	return NULL;
}

static void add_new_starsystem(struct starsystem_info *ss)
{
}

/* Creates a thread for each incoming connection... */
static void service_connection(int connection)
{
	int i, rc, flag = 1;
	int thread_count, iterations;

	fprintf(stderr, "snis_multiverse: zzz 1\n");

	log_client_info(SNIS_INFO, connection, "snis_multiverse: servicing connection\n");
	/* get connection moved off the stack so that when the thread needs it,
	 * it's actually still around.
	 */

	fprintf(stderr, "snis_multiverse: zzz 2\n");
	i = setsockopt(connection, IPPROTO_TCP, TCP_NODELAY, (char *) &flag, sizeof(int));
	if (i < 0)
		snis_log(SNIS_ERROR, "snis_multiverse: setsockopt failed: %s.\n", strerror(errno));
	fprintf(stderr, "snis_multiverse: zzz 2.5\n");

	if (verify_client_protocol(connection)) {
		log_client_info(SNIS_ERROR, connection, "snis_multiverse: disconnected, protocol violation\n");
		close(connection);
		fprintf(stderr, "snis_multiverse, client verification failed.\n");
		return;
	}
	fprintf(stderr, "snis_multiverse: zzz 3\n");

	pthread_mutex_lock(&service_mutex);
	fprintf(stderr, "snis_multiverse: zzz 5\n");

	/* Set i to a free slot in starsystem[], or to nstarsystems if none free */
	for (i = 0; i < nstarsystems; i++)
		if (starsystem[i].refcount == 0)
			break;
	fprintf(stderr, "snis_multiverse: zzz 6\n");
	if (i == nstarsystems) {
		if (nstarsystems < MAX_STARSYSTEMS) {
			nstarsystems++;
		} else {
			fprintf(stderr, "snis_multiverse: Too many starsystems.\n");
			pthread_mutex_unlock(&service_mutex);
			shutdown(connection, SHUT_RDWR);
			close(connection);
			connection = -1;
			return;
		}
	}

	starsystem[i].socket = connection;

	log_client_info(SNIS_INFO, connection, "snis_multiverse: snis_server connected\n");

	pthread_mutex_init(&starsystem[i].write_queue_mutex, NULL);
	packed_buffer_queue_init(&starsystem[i].write_queue);

	add_new_starsystem(&starsystem[i]);

	pthread_attr_init(&starsystem[i].read_attr);
	pthread_attr_setdetachstate(&starsystem[i].read_attr, PTHREAD_CREATE_DETACHED);
	pthread_attr_init(&starsystem[i].write_attr);
	pthread_attr_setdetachstate(&starsystem[i].write_attr, PTHREAD_CREATE_DETACHED);

	/* initialize refcount to 1 to keep starsystem[i] from getting reaped. */
	starsystem[i].refcount = 1;

	/* create threads... */
	fprintf(stderr, "snis_multiverse: zzz 7\n");
	rc = pthread_create(&starsystem[i].read_thread,
		&starsystem[i].read_attr, starsystem_read_thread, (void *) &starsystem[i]);
	if (rc) {
		snis_log(SNIS_ERROR, "per client read thread, pthread_create failed: %d %s %s\n",
			rc, strerror(rc), strerror(errno));
	}
	pthread_setname_np(starsystem[i].read_thread, "snismv-rdr");
	rc = pthread_create(&starsystem[i].write_thread,
		&starsystem[i].write_attr, starsystem_write_thread, (void *) &starsystem[i]);
	if (rc) {
		snis_log(SNIS_ERROR, "per client write thread, pthread_create failed: %d %s %s\n",
			rc, strerror(rc), strerror(errno));
	}
	pthread_setname_np(starsystem[i].write_thread, "snismv-wrtr");
	pthread_mutex_unlock(&service_mutex);
	fprintf(stderr, "snis_multiverse: zzz 8\n");

	/* Wait for at least one of the threads to prevent premature reaping */
	iterations = 0;
	thread_count = 0;
	do {
		pthread_mutex_lock(&service_mutex);
		thread_count = starsystem[i].refcount;
		pthread_mutex_unlock(&service_mutex);
		if (thread_count < 2)
			usleep(1000);
		iterations++;
		if (iterations > 1000 && (iterations % 1000) == 0)
			printf("Way too many iterations at %s:%d\n", __FILE__, __LINE__);
	} while (thread_count < 2);

	fprintf(stderr, "snis_multiverse: zzz 9\n");
	/* release this thread's reference */
	lock_put_starsystem(&starsystem[i]);

	snis_log(SNIS_INFO, "snis_multiverse: bottom of 'service connection'\n");
	fprintf(stderr, "snis_multiverse: zzz 10\n");
}

/* This thread listens for incoming client connections, and
 * on establishing a connection, starts a thread for that
 * connection.
 */
static void *listener_thread(__attribute__((unused)) void *unused)
{
	int rendezvous, connection, rc;
	struct sockaddr_in remote_addr;
	socklen_t remote_addr_len;
	uint16_t port;
	struct addrinfo hints;
	struct addrinfo *result, *rp;
	int s;
	char portstr[20];
	char *snis_multiverse_port_var;

	snis_log(SNIS_INFO, "snis_multiverse: snis_multiverse starting\n");
	snis_multiverse_port_var = getenv("SNISMULTIVERSEPORT");
	default_snis_multiverse_port = -1;
	if (snis_multiverse_port_var != NULL) {
		int rc, value;
		rc = sscanf(snis_multiverse_port_var, "%d", &value);
		if (rc == 1) {
			default_snis_multiverse_port = value & 0x0ffff;
			printf("snis_multiverse: Using SNISMULTIVERSEPORT value %d\n",
				default_snis_multiverse_port);
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
		if (default_snis_multiverse_port == -1)
			port = snis_randn(65335 - 49152) + 49151;
		else
			port = default_snis_multiverse_port;
		snis_log(SNIS_INFO, "snis_multiverse: Trying port %d\n", port);
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
			fprintf(stderr, "snis_multiverse: snis_multiverse: getaddrinfo: %s\n", gai_strerror(s));
			snis_log(SNIS_ERROR, "snis_multiverse: snis_multiverse: getaddrinfo: %s\n", gai_strerror(s));
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
	snis_log(SNIS_INFO, "snis_multiverse listening for connections on port %d\n", port);
	listener_port = port;

	/* Listen for incoming connections... */
	rc = listen(rendezvous, SOMAXCONN);
	if (rc < 0) {
		fprintf(stderr, "snis_multiverse: listen() failed, exiting: %s\n", strerror(errno));
		snis_log(SNIS_ERROR, "snis_multiverse: listen() failed, exiting: %s\n", strerror(errno));
		exit(1);
	}

	/* Notify other threads that the listener thread is ready... */
	(void) pthread_mutex_lock(&listener_mutex);
	pthread_cond_signal(&listener_started);
	(void) pthread_mutex_unlock(&listener_mutex);

	while (1) {
		remote_addr_len = sizeof(remote_addr);
		snis_log(SNIS_INFO, "snis_multiverse: Accepting connection...\n");
		connection = accept(rendezvous, (struct sockaddr *) &remote_addr, &remote_addr_len);
		snis_log(SNIS_INFO, "snis_multiverse: accept returned %d\n", connection);
		if (connection < 0) {
			/* handle failed connection... */
			snis_log(SNIS_WARN, "snis_multiverse: accept() failed: %s\n", strerror(errno));
			ssgl_sleep(1);
			continue;
		}
		if (remote_addr_len != sizeof(remote_addr)) {
			snis_log(SNIS_ERROR, "snis_multiverse: strange socket address length %d\n", remote_addr_len);
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
	pthread_cond_init(&listener_started, NULL);
	(void) pthread_mutex_lock(&listener_mutex);

	/* Create the listener thread... */
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
	rc = pthread_create(&thread, &attr, listener_thread, NULL);
	if (rc) {
		snis_log(SNIS_ERROR, "snis_multiverse: Failed to create listener thread, pthread_create: %d %s %s\n",
				rc, strerror(rc), strerror(errno));
	}
	pthread_setname_np(thread, "snismv-listen");

	/* Wait for the listener thread to become ready... */
	pthread_cond_wait(&listener_started, &listener_mutex);
	(void) pthread_mutex_unlock(&listener_mutex);
	snis_log(SNIS_INFO, "snis_multiverse: Listener started.\n");
	return listener_port;
}

static int restore_data_from_file(const char *path)
{
	int i;
	char *hexpwdhash;
	unsigned char pwdhash[20];
	char *tmppath = NULL;

	if (nbridges >= MAX_BRIDGES)
		goto error;

	/* We expect path to be of the form 'something/something/something/hexpwdhash.data' */
	i = strlen(path) - 1;
	if (path[i] == '/') /* ends with slash... nope. */
		goto error;

	tmppath = strdup(path);
	for (; i >= 0; i--) {
		if (tmppath[i] == '.')
			tmppath[i] = '\0';  /* Cut off the ".data" at the end. */
		if (tmppath[i] == '/') {
			hexpwdhash = &tmppath[i + 1];
			break;
		}
	}
	if (i < 0) {
		fprintf(stderr, "bridge data filename '%s' doesn't look right.\n", path);
		goto error;
	}
	if (strlen(hexpwdhash) != 40) {
		fprintf(stderr, "bridge data filename '%s' is not 40 chars long.\n", path);
		goto error;
	}
	for (i = 0; i < 40; i++) {
		if (strchr("0123456789abcdef", hexpwdhash[i]) == NULL) {
			fprintf(stderr, "bridge data filename '%s' is not composed of 40 hex chars\n", path);
			goto error;
		}
	}
	/* Filename seems good, convert to pwdhash */
	memset(pwdhash, 0, sizeof(pwdhash));
	snis_scan_hash(hexpwdhash, 40, pwdhash, 20);
	if (restore_bridge_info(path, &ship[nbridges], pwdhash)) {
		fprintf(stderr, "Failed to read bridge info from %s\n", path);
		goto error;
	}
	nbridges++;
	free(tmppath);
	return 0;
error:
	if (tmppath)
		free(tmppath);
	return -1;
}

static int restore_data_from_path(const char *path);

static int restore_data_from_directory(const char *path)
{
	struct dirent **namelist;
	char newpath[PATH_MAX];
	int i, n, rc;

	rc = 0;
	n = scandir(path, &namelist, NULL, alphasort);
	if (n < 0) {
		fprintf(stderr, "scandir(%s): %s\n", path, strerror(errno));
		return n;
	}
	for (i = 0; i < n; i++) {
		sprintf(newpath, "%s/%s", path, namelist[i]->d_name);
		rc += restore_data_from_path(newpath);
		free(namelist[i]);
	}
	free(namelist);
	return rc;
}

static int avoid_dots(const char *path)
{
	int len;

	/* exclude ".", "..", and things ending in "/.", "/.." */
	if (strcmp(path, ".") == 0)
		return 1;
	if (strcmp(path, "..") == 0)
		return 1;
	len = strlen(path);
	if (len >= 2 && strcmp(&path[len - 2], "/.") == 0)
		return 1;
	if (len >= 3 && strcmp(&path[len - 3], "/..") == 0)
		return 1;
	return 0;
}

static int restore_data_from_path(const char *path)
{
	struct stat statbuf;
	int rc;

	if (avoid_dots(path))
		return 0;

	rc = stat(path, &statbuf);
	if (rc < 0) {
		fprintf(stderr, "stat(%s): %s\n", path, strerror(errno));
		return rc;
	}
	if (S_ISDIR(statbuf.st_mode))
		return restore_data_from_directory(path);
	return restore_data_from_file(path); /* regular file, or, not a directory anyhow. */
}

static int restore_data(void)
{
	int rc;

	assert(nbridges == 0);
	pthread_mutex_lock(&data_mutex);
	rc = restore_data_from_path(database_root);
	pthread_mutex_unlock(&data_mutex);
	return rc;
}

static void checkpoint_data(void)
{
	int i;

	pthread_mutex_lock(&data_mutex);
	for (i = 0; i < nbridges; i++)
		save_bridge_info(&ship[i]);
	pthread_mutex_unlock(&data_mutex);
}

static void create_database_root_or_die(char *database_root)
{
	if (mkdir(database_root, database_mode) != 0) {
		if (errno != EEXIST) {
			fprintf(stderr, "snis_multiverse: Can't mkdir %s: %s\n",
				database_root, strerror(errno));
			exit(1);
		}
	}
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
	struct ssgl_game_server gameserver;
	int rc;
	pthread_t lobby_thread;

	open_log_file();
	if (restore_data()) {
		fprintf(stderr, "Failed to restore data from checkpoint database\n");
		return -1;
	}

	if (argc < 4)
		usage();

	printf("SNIS multiverse server\n");

	create_database_root_or_die(database_root);

	snis_protocol_debugging(1);
	ignore_sigpipe();
	rc = start_listener_thread();
	if (rc < 0) {
		fprintf(stderr, "snis_multiverse: start_listener_thread() failed.\n");
		return -1;
	}
	/* Set up the game server structure that we will send to the lobby server. */
	memset(&gameserver, 0, sizeof(gameserver));
	if (ssgl_get_primary_host_ip_addr(&gameserver.ipaddr) != 0)
		snis_log(SNIS_WARN, "snis_multiverse: Failed to get local ip address.\n");
	gameserver.port = htons(listener_port);
#define COPYINARG(field, arg) strncpy(gameserver.field, argv[arg], sizeof(gameserver.field) - 1)
	COPYINARG(server_nickname, 2);
	strcpy(gameserver.game_type, "SNIS-MVERSE");
	strcpy(gameserver.game_instance, "-");
	COPYINARG(location, 3);

	/* create a thread to contact and update the lobby server... */
	(void) ssgl_register_gameserver(argv[1], &gameserver, &lobby_thread, &nconnections);

	do {
		/* do whatever it is that your game server does here... */
		ssgl_sleep(30);
		checkpoint_data();
	} while (1);
	return 0;
}

