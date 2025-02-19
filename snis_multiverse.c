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
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <errno.h>
#include <pthread.h>
#include <assert.h>
#include <stdarg.h>
#include <limits.h>
#include <dirent.h>
#include <getopt.h>
#include <signal.h>

#include "build_bug_on.h"
#include "snis_marshal.h"
#include "quat.h"
#include "ssgl/ssgl.h"
#include "snis_log.h"
#include "snis_socket_io.h"
#define SNIS_SERVER_DATA 1
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
#include "pthread_util.h"
#include "rootcheck.h"
#include "starmap_adjacency.h"
#include "string-utils.h"
#include "replacement_assets.h"
#include "snis_asset_dir.h"
#include "snis_bin_dir.h"
#include "snis_licenses.h"
#include "snis_version.h"
#include "build_info.h"

static char *asset_dir;
static char *lobby, *nick, *location;
#define DEFAULT_DATABASE_ROOT "./snisdb"
static char *database_root = DEFAULT_DATABASE_ROOT;
static char snis_db_dir[PATH_MAX] = { 0 };
static const int database_mode = 0744;
static pthread_mutex_t data_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t listener_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t listener_started;
static pthread_mutex_t service_mutex = PTHREAD_MUTEX_INITIALIZER;
static int listener_port = -1;
static int default_snis_multiverse_port = -1; /* -1 means choose a random port */
static int snis_log_level = 2;
static int nconnections = 0;
static int debuglevel = 0;

#define MAX_BRIDGES 1024
#define MAX_CONNECTIONS 100
#define MAX_STARSYSTEMS MAXSTARMAPENTRIES

static struct starsystem_info { /* one of these per connected snis_server */
	int socket;			/* socket to snis_server */
	int refcount;			/* reference count of threads accessing this star system */
	pthread_t read_thread;		/* thread to read from snis_server */
	pthread_t write_thread;		/* thread to write to snis_server */
	struct packed_buffer_queue write_queue; /* queue of data to be sent to snis server */
	pthread_mutex_t write_queue_mutex;	/* protects the queue */
	char starsystem_name[SSGL_LOCATIONSIZE]; /* corresponds to "location" parameter of snis_server */
	int bridge_count;		/* How many bridges are in this star system */
	int shutdown_countdown;		/* When this reaches zero (and some other conditions), shut down snis_server */
					/* see maybe_shutdown_snis_server() */
} starsystem[MAX_STARSYSTEMS] = { { 0 } };
static int nstarsystems = 0;

static struct bridge_info {
	unsigned char pwdhash[PWDHASHLEN];	/* hash of password, shipname, salt */
	int32_t initialized;			/* Have we unpacked data from snis server for this bridge? */
	struct snis_entity entity;		/* used to hold some of the bridge state */
	char starsystem_name[SSGL_LOCATIONSIZE];/* Current location of this bridge */

	/* persistent_bridge_data contains per-bridge data which is not
	 * present in snis_entity, e.g. engineering presets.
	 * See snis_bridge_update_packet.h. */
	struct persistent_bridge_data persistent_bridge_data;

} ship[MAX_BRIDGES];
int nbridges = 0;

static char *exempt_server[MAX_STARSYSTEMS] = { 0 };
static int nexempt_servers = 0;

/* star map data -- corresponds to known snis_server instances
 * as discovered by digging through share/snis/solarsystems
 */
static struct starmap_entry starmap[MAXSTARMAPENTRIES];
static int nstarmap_entries = 0;
static int starmap_adjacency[MAXSTARMAPENTRIES][MAX_STARMAP_ADJACENCIES];
static int autowrangle = 0;

static struct replacement_asset replacement_assets;

static char snis_multiverse_lockfile[PATH_MAX] = { 0 };

static void cleanup_lockfile(void)
{
	/* This may be called from a signal handler, so do not call any non-reentrant functions */
	if (snis_multiverse_lockfile[0] != '\0')
		(void) rmdir(snis_multiverse_lockfile);
}

/* Clean up the lock file on SIGTERM.
 * TODO: figure out how to share this lockfile related code with the very similar but
 * slightly different code in snis_server.c
 */
static void sigterm_handler(__attribute__((unused)) int sig,
			__attribute__((unused)) siginfo_t *siginfo,
			__attribute__((unused)) void *context)
{
	static const char buffer[] = "snis_multiverse: Received SIGTERM, exiting.\n";
	int rc;

	cleanup_lockfile();

	/* Need to check return value of write() here even though we can't do
	 * anything with that info at this point, otherwise the compiler complains
	 * if we just ignore write() return value.
	 */
	rc = write(2, buffer, sizeof(buffer) - 1);  /* We're assuming 2 is still hooked to stderr */
	if (rc < 0)
		_exit(2);
	_exit(1);
}

static void catch_sigterm(void)
{
	struct sigaction action;

	memset(&action, 0, sizeof(action));
	action.sa_sigaction = &sigterm_handler;
	action.sa_flags = SA_SIGINFO;
	if (sigaction(SIGTERM, &action, NULL) < 0)
		fprintf(stderr, "%s: Failed to register SIGTERM handler.\n", "snis_multiverse");
}

static void create_lock_or_die(void)
{
	int rc;

	snprintf(snis_multiverse_lockfile, sizeof(snis_multiverse_lockfile) - 1, "/tmp/snis_multiverse_lock");
	catch_sigterm();
	rc = mkdir(snis_multiverse_lockfile, 0755);
	if (rc != 0) {
		fprintf(stderr, "%s: Failed to create lockdir %s, exiting.\n",
				"snis_multiverse", snis_multiverse_lockfile);
		exit(1);
	}
	atexit(cleanup_lockfile);
	fprintf(stderr, "%s: Created lockfile %s, proceeding.\n", "snis_multiverse", snis_multiverse_lockfile);
}

#define INCLUDE_BRIDGE_INFO_FIELDS 1
#include "snis_entity_key_value_specification.h"

static void exempt_snis_server(char *name)
{
	if (nexempt_servers >= MAX_STARSYSTEMS) {
		fprintf(stderr,
			"snis_multiverse: Too many exempt starsystems, not exempting '%s'\n",
			name);
		return;
	}
	exempt_server[nexempt_servers] = strdup(name);
	nexempt_servers++;
}

static int snis_server_is_exempt(char *name)
{
	int i;

	for (i = 0; i < nexempt_servers; i++)
		if (strcasecmp(exempt_server[i], name) == 0)
			return 1;
	return 0;
}

static void extract_starmap_entry(char *path, char *starname)
{
	int rc, bytes;
	char *content;
	char *starlocation;
	float x, y, z;

	printf("Extracting starmap entry from %s... ", path);
	content = slurp_file(path, &bytes);
	if (!content)
		return;
	starlocation = strstr(content, "\nstar location: ");
	if (!starlocation)
		return;
	rc = sscanf(starlocation + 1, "star location: %f%*[, ]%f%*[, ]%f%*[, ]", &x, &y, &z);
	if (rc != 3)
		return;
	snprintf(starmap[nstarmap_entries].name,
		sizeof(starmap[nstarmap_entries].name) - 1, "%s", starname);
	starmap[nstarmap_entries].x = x;
	starmap[nstarmap_entries].y = y;
	starmap[nstarmap_entries].z = z;
	nstarmap_entries++;
	printf("%s %f, %f, %f\n", starname, x, y, z);
	return;
}

static void construct_starmap(void)
{
	struct dirent **namelist;
	char newpath[PATH_MAX];
	int i, n;
	char path[PATH_MAX];

	sprintf(path, "%s/%s", asset_dir, "solarsystems");
	n = scandir(replacement_asset_lookup(path, &replacement_assets), &namelist, NULL, alphasort);
	if (n < 0) {
		fprintf(stderr, "snis_multiverse: scandir(%s): %s\n", path, strerror(errno));
		return;
	}
	for (i = 0; i < n; i++) {
		if (strcmp(namelist[i]->d_name, ".") == 0)
			continue;
		if (strcmp(namelist[i]->d_name, "..") == 0)
			continue;
		snprintf(newpath, sizeof(newpath), "%s/%s/assets.txt", path, namelist[i]->d_name);
		extract_starmap_entry(newpath, namelist[i]->d_name);
		free(namelist[i]);
	}
	free(namelist);
	starmap_compute_adjacencies(starmap_adjacency, starmap, nstarmap_entries);
	return;
}

/* assumes service_mutex held. */
static void put_starsystem(struct starsystem_info *ss)
{
	assert(ss->refcount > 0);
	ss->refcount--;
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

	/* snis_entity_key_value_specification.h assumes there are 6 presets in
	 * an array with elements 0-5. If that is not the case, we detect it here.
	 */
	BUILD_ASSERT(ENG_PRESET_NUMBER == 10);

	fprintf(f, "starsystem:%s\n", b->starsystem_name);
	return key_value_write_lines(f, snis_entity_kvs, base_address);
}

static void print_hash(char *s, unsigned char *pwdhash)
{
	int i;

	fprintf(stderr, "snis_multiverse: %s", s);
	for (i = 0; i < PWDHASHLEN; i++)
		fprintf(stderr, "%02x", pwdhash[i]);
	fprintf(stderr, "\n");
}

static int save_bridge_info(struct bridge_info *b)
{
	int rc;
	char dirpath[PATH_MAX], path[PATH_MAX], path2[PATH_MAX];
	char dir1[5], dir2[5];
	unsigned char hexpwdhash[PWDHASHLEN * 2 + 1];
	FILE *f;

	if (debuglevel > 0)
		fprintf(stderr, "save_bridge_info 1\n");
	memset(dir1, 0, sizeof(dir1));
	memset(dir2, 0, sizeof(dir2));

	snis_format_hash(b->pwdhash, PWDHASHLEN, hexpwdhash, PWDHASHLEN * 2 + 1);
	memcpy(&dir1[0], &hexpwdhash[PWDSALTLEN], 4);
	memcpy(&dir2[0], &hexpwdhash[PWDSALTLEN + 4], 4);

	snprintf(path, sizeof(path), "%s/%s", database_root, dir1);
	if (make_dir(path)) {
		fprintf(stderr, "snis_multiverse: mkdir failed: %s: %s\n", path, strerror(errno));
		return -1;
	}
	snprintf(dirpath, sizeof(dirpath), "%s/%s/%s", database_root, dir1, dir2);
	if (make_dir(dirpath)) {
		fprintf(stderr, "snis_multiverse: mkdir failed: %s: %s\n", dirpath, strerror(errno));
		return -1;
	}
	snprintf(path, sizeof(path), "%s/%s.update", dirpath, hexpwdhash);
	snprintf(path2, sizeof(path2), "%s/%s.data", dirpath, hexpwdhash);
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
	if (debuglevel > 0)
		fprintf(stderr, "snis_multiverse: wrote bridge info to %s\n", path2);
	return 0;
}

static int restore_bridge_info(const char *filename, struct bridge_info *b, unsigned char *pwdhash)
{
	char *contents;
	int rc;
	void *base_address[] = { &b->entity, b };
	char starsystem[100];
	char *nextline;

	contents = slurp_file(filename, NULL);
	if (!contents)
		return -1;
	memcpy(b->pwdhash, pwdhash, PWDHASHLEN);

	/* Figure out this bridge's current starsystem, then read all the bridge data
	 * We need to be careful not to overwrite the starsystem with old data.
	 */
	memset(b->starsystem_name, 0, sizeof(b->starsystem_name));
	rc = sscanf(contents, "starsystem:%s", starsystem);
	if (rc == 1) {
		snprintf(b->starsystem_name, sizeof(b->starsystem_name) - 1, "%s", starsystem);
		nextline = strchr(contents, '\n');
		rc = key_value_parse_lines(snis_entity_kvs, nextline + 1, base_address);
	} else {
		snprintf(b->starsystem_name, sizeof(b->starsystem_name) - 1, "%s", "UNKNOWN");
		rc = key_value_parse_lines(snis_entity_kvs, contents, base_address);
	}
	free(contents);
	return rc;
}

static void usage(void)
{
	fprintf(stderr, "usage: snis_multiverse [--autowrangle] -l lobbyserver -n servernick \\\n");
	fprintf(stderr, "          -L location [ --exempt snis-server-location]\n");
	fprintf(stderr, "       snis_multiverse --acknowledgments\n");
	exit(1);
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

static void log_client_info(int level, int connection, char *info)
{
	char client_ip[50];

	if (level < snis_log_level)
		return;

	memset(client_ip, 0, sizeof(client_ip));
	get_peer_name(connection, client_ip);
	fprintf(stderr, "snis_multiverse: connection from %s: %s",
			client_ip, info);
}

static int verify_client_protocol(int connection)
{
	int rc;
	char protocol_version[sizeof(SNIS_MULTIVERSE_VERSION) + 1];
	const int len = sizeof(SNIS_MULTIVERSE_VERSION) - 1;

	rc = snis_writesocket(connection, SNIS_MULTIVERSE_VERSION, len);
	if (rc < 0) {
		fprintf(stderr, "snis_multiverse:verify_client_protocol(), failed writing to client, (%d:%s)\n",
				errno, strerror(errno));
		return -1;
	}
	memset(protocol_version, 0, sizeof(protocol_version));
	rc = snis_readsocket(connection, protocol_version, len);
	if (rc < 0) {
		fprintf(stderr, "snis_multiverse:verify_client_protocol() failed to read from client: %d (%d:%s)\n",
			rc, errno, strerror(errno));
		return rc;
	}
	protocol_version[len] = '\0';
	snis_log(SNIS_INFO, "snis_multiverse: protocol read...'%s'\n", protocol_version);
	if (strcmp(protocol_version, SNIS_MULTIVERSE_VERSION) != 0) {
		fprintf(stderr, "snis_multiverse: protocol version mismatch, got '%s', expected '%s'\n",
			protocol_version, SNIS_MULTIVERSE_VERSION);
		return -1;
	}
	snis_log(SNIS_INFO, "snis_multiverse: protocol verified.\n");
	return 0;
}

static int lookup_bridge(void)
{
	return 0;
}

/* TODO: this is very similar to snis_server's version of same function */
static int read_and_unpack_buffer(struct starsystem_info *ss, unsigned char *buffer, char *format, ...)
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

	rc = snis_readsocket(ss->socket, buffer, size);
	if (rc != 0)
		return rc;
	packed_buffer_init(&pb, buffer, size);
	va_start(ap, format);
	rc = packed_buffer_extract_va(&pb, format, ap);
	va_end(ap);
	return rc;
}

static int lookup_ship_by_hash(unsigned char *hash)
{
	int i;
	unsigned char phash[PWDHASHLEN * 2 + 1];

	snis_format_hash(hash, PWDHASHLEN, phash, PWDHASHLEN * 2 + 1);
	if (debuglevel > 0)
		fprintf(stderr, "snis_multiverse: looking up hash '%s'\n", phash);

	for (i = 0; i < nbridges; i++) {
		snis_format_hash(ship[i].pwdhash, PWDHASHLEN, phash, PWDHASHLEN * 2 + 1);
		if (debuglevel > 0)
			fprintf(stderr, "snis_multiverse: checking against '%s'\n", phash);
		if (memcmp(ship[i].pwdhash, hash, PWDHASHLEN) == 0) {
			if (debuglevel > 0)
				fprintf(stderr, "snis_multiverse: match hash '%s'\n", phash);
			return i;
		}
	}
	if (debuglevel > 0)
		fprintf(stderr, "snis_multiverse: no match found\n");
	return -1;
}

static int update_bridge(struct starsystem_info *ss)
{
	unsigned char pwdhash[PWDHASHLEN];
	int i, rc;
	unsigned char buffer[430];
	struct packed_buffer pb;
	struct snis_entity *o;

#define bytes_to_read (UPDATE_BRIDGE_PACKET_SIZE - PWDHASHLEN - 1)

	if (debuglevel > 0)
		fprintf(stderr, "snis_multiverse: update bridge 1, expecting %d bytes\n", bytes_to_read);
	memset(buffer, 0, sizeof(buffer));
	memset(pwdhash, 0, sizeof(pwdhash));
	rc = read_and_unpack_fixed_size_buffer(ss, buffer, PWDHASHLEN, "r", pwdhash, (uint16_t) PWDHASHLEN);
	if (rc != 0)
		return rc;
	if (debuglevel > 0)
		print_hash("update bridge 2: ", pwdhash);
	BUILD_ASSERT(sizeof(buffer) > bytes_to_read);
	memset(buffer, 0, sizeof(buffer));
	rc = snis_readsocket(ss->socket, buffer, bytes_to_read);
	if (rc != 0)
		return rc;
	if (debuglevel > 0)
		fprintf(stderr, "snis_multiverse: update bridge 3\n");
	pthread_mutex_lock(&data_mutex);
	i = lookup_ship_by_hash(pwdhash);
	if (i < 0) {
		fprintf(stderr, "snis_multiverse: Unknown ship hash\n");
		pthread_mutex_unlock(&data_mutex);
		return rc;
	}
	if (debuglevel > 0)
		fprintf(stderr, "snis_multiverse: update bridge 4\n");

	o = &ship[i].entity;
	if (!o->tsd.ship.damcon) {
		o->tsd.ship.damcon = malloc(sizeof(*o->tsd.ship.damcon));
		memset(o->tsd.ship.damcon, 0, sizeof(*o->tsd.ship.damcon));
	}

	packed_buffer_init(&pb, buffer, bytes_to_read);
	unpack_bridge_update_packet(o, &ship[i].persistent_bridge_data, &pb);
	ship[i].initialized = 1;

	/* Update the ship's starsystem */
	snprintf(ship[i].starsystem_name, sizeof(ship[i].starsystem_name) - 1, "%s", ss->starsystem_name);

	pthread_mutex_unlock(&data_mutex);
	rc = 0;
	if (debuglevel > 0)
		fprintf(stderr, "snis_multiverse: update bridge 10\n");
	return rc;
}

static int create_new_ship(unsigned char pwdhash[PWDHASHLEN])
{
	fprintf(stderr, "snis_multiverse: create_new_ship 1 (nbridges = %d, MAX=%d)\n", nbridges, MAX_BRIDGES);
	if (nbridges >= MAX_BRIDGES)
		return -1;
	memset(&ship[nbridges], 0, sizeof(ship[nbridges]));
	memcpy(ship[nbridges].pwdhash, pwdhash, PWDHASHLEN);
	nbridges++;
	fprintf(stderr, "snis_multiverse: added new bridge, nbridges = %d\n", nbridges);
	return 0;
}

static void send_bridge_update_to_snis_server(struct starsystem_info *ss,
				struct persistent_bridge_data *bd, unsigned char *pwdhash)
{
	int i;
	struct packed_buffer *pb;
	struct snis_entity *o;

	pthread_mutex_lock(&data_mutex);
	i = lookup_ship_by_hash(pwdhash);
	if (i < 0) {
		pthread_mutex_unlock(&data_mutex);
		return;
	}
	o = &ship[i].entity;

	/* Update the ship */
	pb = build_bridge_update_packet(o, bd, pwdhash);
	if (packed_buffer_length(pb) != UPDATE_BRIDGE_PACKET_SIZE) {
		fprintf(stderr, "snis_multiverse: bridge packet size is wrong (actual: %d, nominal: %d)\n",
			packed_buffer_length(pb), UPDATE_BRIDGE_PACKET_SIZE);
		abort();
	}
	pthread_mutex_unlock(&data_mutex);
	packed_buffer_queue_add(&ss->write_queue, pb, &ss->write_queue_mutex);
}

static int verify_existence(struct starsystem_info *ss, int should_already_exist)
{
	unsigned char pwdhash[PWDHASHLEN];
	unsigned char buffer[200];
	struct packed_buffer *pb;
	int i, rc;
	unsigned char pass;
	unsigned char printable_hash[PWDHASHLEN * 2 + 1];
	int send_update = 0;
	struct persistent_bridge_data *bd = NULL;

	fprintf(stderr, "snis_multiverse: verify_existence 1: should %salready exist\n",
			should_already_exist ? "" : "not ");
	rc = read_and_unpack_fixed_size_buffer(ss, buffer, PWDHASHLEN, "r", pwdhash, (uint16_t) PWDHASHLEN);
	if (rc != 0)
		return rc;
	snis_format_hash(pwdhash, PWDHASHLEN, printable_hash, PWDHASHLEN * 2 + 1);
	printable_hash[PWDHASHLEN * 2] = '\0';
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
			bd = &ship[i].persistent_bridge_data;
			send_update = 1;
		} else {
			fprintf(stderr, "snis_multiverse: hash %s exists, but should not.\n", printable_hash);
			pass = SNISMV_VERIFICATION_RESPONSE_FAIL;
		}
	}
	print_hash("checking hash ", pwdhash);
	fprintf(stderr, "snis_multiverse: verify existence pass=%d\n", pass);
	pthread_mutex_unlock(&data_mutex);
	pb = packed_buffer_allocate(PWDHASHLEN + 2);
	packed_buffer_append(pb, "bbr", SNISMV_OPCODE_VERIFICATION_RESPONSE, pass, pwdhash, PWDHASHLEN);
	packed_buffer_queue_add(&ss->write_queue, pb, &ss->write_queue_mutex);

	if (send_update)
		send_bridge_update_to_snis_server(ss, bd, pwdhash);

	return 0;
}

static int update_bridge_count(struct starsystem_info *ss)
{
	uint8_t buffer[16];
	uint32_t bridge_count;
	int rc;

	rc = read_and_unpack_buffer(ss, buffer, "w", &bridge_count);
	if (rc)
		return rc;
	ss->bridge_count = bridge_count;
	if (bridge_count > 0)
		ss->shutdown_countdown = -1;
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
	case SNISMV_OPCODE_UPDATE_BRIDGE_COUNT:
		rc = update_bridge_count(ss);
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
		if (debuglevel > 0)
			fprintf(stderr, "snis_multiverse: writing buffer %d bytes.\n", buffer->buffer_size);
		rc = snis_writesocket(ss->socket, buffer->buffer, buffer->buffer_size);
		if (debuglevel > 0)
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
		if (debuglevel > 0)
			fprintf(stderr, "snis_multiverse: writing noop.\n");
		rc = snis_writesocket(ss->socket, &noop, sizeof(noop));
		if (debuglevel > 0)
			fprintf(stderr, "snis_multiverse: wrote noop, rc = %d.\n", rc);
		if (rc != 0) {
			fprintf(stderr, "snis_multiverse: (noop) writesocket failed, rc= %d, errno = %d(%s)\n",
				rc, errno, strerror(errno));
			goto badclient;
		}
	}
	if (debuglevel > 0)
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

/* Creates a thread for each incoming connection... */
static void service_connection(int connection)
{
	int i, rc, flag = 1;
	int thread_count, iterations;
	char starsystem_name[SSGL_LOCATIONSIZE + 1];

	log_client_info(SNIS_INFO, connection, "snis_multiverse: servicing connection\n");
	/* get connection moved off the stack so that when the thread needs it,
	 * it's actually still around.
	 */

	i = setsockopt(connection, IPPROTO_TCP, TCP_NODELAY, (char *) &flag, sizeof(int));
	if (i < 0)
		snis_log(SNIS_ERROR, "snis_multiverse: setsockopt(TCP_NODELAY) failed: %s.\n", strerror(errno));
	uint8_t iptos_lowdelay = IPTOS_LOWDELAY;
	i = setsockopt(connection, IPPROTO_IP, IPTOS_LOWDELAY, &iptos_lowdelay, sizeof(iptos_lowdelay));
	if (i < 0)
		snis_log(SNIS_ERROR, "snis_multiverse: setsockopt(IPTOS_LOWDELAY) failed: %s.\n", strerror(errno));

	if (verify_client_protocol(connection)) {
		log_client_info(SNIS_ERROR, connection, "snis_multiverse: disconnected, protocol violation\n");
		close(connection);
		fprintf(stderr, "snis_multiverse, client verification failed.\n");
		return;
	}

	memset(starsystem_name, 0, sizeof(starsystem_name));
	rc = snis_readsocket(connection, starsystem_name, SSGL_LOCATIONSIZE);
	if (rc < 0) {
		log_client_info(SNIS_ERROR, connection,
			"snis_multiverse: disconnected, failed to read starsystem name\n");
		close(connection);
		fprintf(stderr, "snis_multiverse, client verification failed.\n");
		return;
	}

	pthread_mutex_lock(&service_mutex);

	/* Set i to a free slot in starsystem[], or to nstarsystems if none free */
	for (i = 0; i < nstarsystems; i++)
		if (starsystem[i].refcount == 0)
			break;
	if (i == nstarsystems) {
		if (nstarsystems < MAX_STARSYSTEMS) {
			nstarsystems++;
		} else {
			fprintf(stderr, "snis_multiverse: Too many starsystems.\n");
			pthread_mutex_unlock(&service_mutex);
			shutdown(connection, SHUT_RDWR);
			close(connection);
			return;
		}
	}

	starsystem[i].bridge_count = 1; /* Assume 1 for now. SNISMV_OPCODE_UPDATE_BRIDGE_COUNT will update later */
	starsystem[i].shutdown_countdown = -1;
	starsystem[i].socket = connection;
	snprintf(starsystem[i].starsystem_name, sizeof(starsystem[i].starsystem_name) - 1,
			"%s", starsystem_name);

	log_client_info(SNIS_INFO, connection, "snis_multiverse: snis_server connected\n");

	pthread_mutex_init(&starsystem[i].write_queue_mutex, NULL);
	packed_buffer_queue_init(&starsystem[i].write_queue);

	/* initialize refcount to 1 to keep starsystem[i] from getting reaped. */
	starsystem[i].refcount = 1;

	/* create threads... */
	rc = create_thread(&starsystem[i].read_thread,
		starsystem_read_thread, (void *) &starsystem[i], "snismv-rdr", 1);
	if (rc) {
		snis_log(SNIS_ERROR, "per client read thread, create_thread failed: %d %s %s\n",
			rc, strerror(rc), strerror(errno));
	}
	rc = create_thread(&starsystem[i].write_thread,
		starsystem_write_thread, (void *) &starsystem[i], "snismv-wrtr", 1);
	if (rc) {
		snis_log(SNIS_ERROR, "per client write thread, create_thread failed: %d %s %s\n",
			rc, strerror(rc), strerror(errno));
	}
	pthread_mutex_unlock(&service_mutex);

	/* Wait for at least one of the threads to prevent premature reaping */
	iterations = 0;
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

	/* release this thread's reference */
	lock_put_starsystem(&starsystem[i]);

	snis_log(SNIS_INFO, "snis_multiverse: bottom of 'service connection'\n");
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
	int rc;

	/* Setup to wait for the listener thread to become ready... */
	pthread_cond_init(&listener_started, NULL);
	(void) pthread_mutex_lock(&listener_mutex);

	/* Create the listener thread... */
	rc = create_thread(&listener_thread, listener_thread_fn, NULL, "snismv-listen", 1);
	if (rc) {
		snis_log(SNIS_ERROR, "snis_multiverse: Failed to create listener thread, create_thread: %d %s %s\n",
				rc, strerror(rc), strerror(errno));
	}

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
	unsigned char pwdhash[PWDHASHLEN];
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
	if (strlen(hexpwdhash) != PWDHASHLEN * 2) {
		fprintf(stderr, "hexpwdhash '%s' is not %d chars long.\n", hexpwdhash, PWDHASHLEN * 2);
		goto error;
	}
	for (i = 0; i < PWDHASHLEN * 2; i++) {
		if (strchr("0123456789abcdef", hexpwdhash[i]) == NULL) {
			fprintf(stderr, "bridge data filename '%s' is not composed of hex chars\n", path);
			goto error;
		}
	}
	/* Filename seems good, convert to pwdhash */
	memset(pwdhash, 0, sizeof(pwdhash));
	snis_scan_hash(hexpwdhash, PWDHASHLEN * 2, pwdhash, PWDHASHLEN);
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
		snprintf(newpath, sizeof(newpath), "%s/%s", path, namelist[i]->d_name);
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

static void acknowledgments(void)
{
	print_snis_credits_text();
	print_snis_licenses();
}

#define OPT_ACKNOWLEDGMENTS 1000
static struct option long_options[] = {
	{ "acknowledgments", no_argument, NULL, OPT_ACKNOWLEDGMENTS },
	{ "acknowledgements", no_argument, NULL, OPT_ACKNOWLEDGMENTS },
	{ "autowrangle", no_argument, NULL, 'a' },
	{ "exempt", required_argument, NULL, 'e'},
	{ "lobby", required_argument, NULL, 'l'},
	{ "servernick", required_argument, NULL, 'n'},
	{ "location", required_argument, NULL, 'L'},
	{ "version", no_argument, NULL, 'v' },
	{ 0, 0, 0, 0 },
};

static void parse_options(int argc, char *argv[], char **lobby, char **nick, char **location)
{
	int c;

	*lobby = NULL;
	*location = NULL;
	*nick = NULL;

	if ((argc < 4) &&
		(argc != 2 || (strcmp(argv[1], "--acknowledgments") != 0 &&
				strcmp(argv[1], "--acknowledgements") != 0 &&
				strcmp(argv[1], "--version") != 0 &&
				strcmp(argv[1], "-v") != 0)))
		usage();
	while (1) {
		int option_index;
		c = getopt_long(argc, argv, "ae:L:l:n:v", long_options, &option_index);
		if (c == -1)
			break;
		switch (c) {
		case OPT_ACKNOWLEDGMENTS:
			acknowledgments();
			exit(0);
			break; /* not reached */
		case 'e':
			exempt_snis_server(optarg);
			break;
		case 'a':
			autowrangle = 1;
			break;
		case 'h':
			usage(); /* no return */
			break;
		case 'l':
			*lobby = optarg;
			break;
		case 'L':
			*location = optarg;
			break;
		case 'n':
			*nick = optarg;
			break;
		case 'v':
			printf("snis_multiverse ");
			printf("%s\n", BUILD_INFO_STRING1);
			printf("%s\n", BUILD_INFO_STRING2);
			exit(0);
		default:
			usage(); /* no return */
			break;
		}
	}

	if (!*lobby || !*nick || !*location)
		usage(); /* no return */
}

static int snis_server_lockfile_exists(char *starsystem_name)
{
	char name[PATH_MAX], snis_server_lockfile[PATH_MAX];
	struct stat statbuf;
	int rc;

	strcpy(name, starsystem_name);
	uppercase(name);
	snprintf(snis_server_lockfile, sizeof(snis_server_lockfile) - 1, "/tmp/snis_server_lock.%s", name);
	rc = stat(snis_server_lockfile, &statbuf);
	if (rc < 0 && errno == ENOENT)
		return 0;
	return 1;
}

static void start_snis_server(char *starsystem_name)
{
	char cmd[PATH_MAX];
	char *bindir;
	char snis_server_location[PATH_MAX];
	char upper_snis_server_location[PATH_MAX];
	int rc;
	char *snis_log_dir = getenv("SNIS_LOG_DIR");
	if (!snis_log_dir)
		snis_log_dir = snis_db_dir;

	snprintf(snis_server_location, sizeof(snis_server_location) - 1, "%s", starsystem_name);
	strcpy(upper_snis_server_location, snis_server_location);
	uppercase(upper_snis_server_location);
	bindir = get_snis_bin_dir();
	snprintf(cmd, sizeof(cmd) - 1,
			"%s/snis_server -l %s -L %s -m %s -s %s > %s/snis_server.%s.log 2>&1 &", bindir,
			lobby, upper_snis_server_location, location, snis_server_location,
			snis_log_dir, starsystem_name);

	fprintf(stderr, "snis_multiverse: STARTING SNIS SERVER for %s\n", starsystem_name);
	fprintf(stderr, "snis_multiverse: Executing cmd: %s\n", cmd);
	/* TODO: We should really fork() and exec() this ourself without getting bash involved. */
	rc = system(cmd);
	if (rc) {
		fprintf(stderr, "snis_multiverse: Failed to start snis_server: %s\n", strerror(errno));
		fprintf(stderr, "cmd was: %s\n", cmd);
		return;
	}
}

static void maybe_startup_snis_server(int n)
{
	int i;

	pthread_mutex_lock(&service_mutex);
	/* Check if snis_server for starmap[i].name is already running or about to be running ... */
	for (i = 0; i < nstarsystems; i++) {
		if (strcmp(starsystem[i].starsystem_name, starmap[n].name) == 0) {
			if (starsystem[i].socket >= 0) {
				pthread_mutex_unlock(&service_mutex);
				return;
			}
			if (snis_server_lockfile_exists(starsystem[i].starsystem_name)) {
				pthread_mutex_unlock(&service_mutex);
				return;
			}
			break;
		}
	}
	pthread_mutex_unlock(&service_mutex);
	start_snis_server(starmap[n].name);
}

static void maybe_shutdown_snis_server(int n)
{
	int i, rc;
	uint8_t shutdown_opcode = SNISMV_OPCODE_SHUTDOWN_SNIS_SERVER;

	if (snis_server_is_exempt(starmap[n].name))
		return;

	pthread_mutex_lock(&service_mutex);
	/* Check if snis_server for starmap[i].name is already not running ... */
	for (i = 0; i < nstarsystems; i++) {
		if (strcmp(starsystem[i].starsystem_name, starmap[n].name) == 0) {
			if (starsystem[i].socket < 0) {
				pthread_mutex_unlock(&service_mutex);
				return; /* It's already not running. */
			}
			if (!snis_server_lockfile_exists(starsystem[i].starsystem_name)) {
				pthread_mutex_unlock(&service_mutex);
				return; /* It's already not running */
			}
			break;
		}
	}
	if (i >= nstarsystems) {
		pthread_mutex_unlock(&service_mutex);
		return;
	}
	if (starsystem[i].shutdown_countdown == -1) {
		starsystem[i].shutdown_countdown = 10;
		fprintf(stderr, "snis_multiverse, STARTED SHUTDOWN COUNTER FOR snis_server %s, countdown = %d\n",
				starmap[n].name, starsystem[i].shutdown_countdown);
	}
	if (starsystem[i].shutdown_countdown > 0)
		starsystem[i].shutdown_countdown--;
	if (starsystem[i].shutdown_countdown > 0) {
		pthread_mutex_unlock(&service_mutex);
		return;
	}

	printf("snis_multiverse: SHUTTING DOWN SNIS SERVER for %s\n", starmap[n].name);
	rc = snis_writesocket(starsystem[i].socket, &shutdown_opcode, sizeof(shutdown_opcode));
	pthread_mutex_unlock(&service_mutex);
	if (rc)
		fprintf(stderr, "snis_multiverse: Failed to write shutdown opcode to snis_server.\n");
}

static void wrangle_snis_server_processes(void)
{
	int i, j, k;
	char active_starsystems[MAXSTARMAPENTRIES];
	char adj_starsystem[MAXSTARMAPENTRIES];
	char bridge_location[MAXSTARMAPENTRIES][SSGL_LOCATIONSIZE];
	int nbridge_locations = 0;
	int found;

	if (!autowrangle)
		return;

	/* Make a list of the names of star systems with active bridges in them */
	pthread_mutex_lock(&data_mutex);
	for (i = 0; i < nbridges; i++) {
		if (strcmp(ship[i].starsystem_name, "UNKNOWN") == 0)
			continue;
		found = 0;
		for (j = 0; j < nbridge_locations; j++) {
			if (strcmp(bridge_location[j], ship[i].starsystem_name) == 0) {
				found = 1;
				break;
			}
		}
		if (!found) {
			memcpy(bridge_location[nbridge_locations], ship[i].starsystem_name, SSGL_LOCATIONSIZE);
			nbridge_locations++;
		}
	}
	pthread_mutex_unlock(&data_mutex);

	/* Make a map of active starsystems */
	memset(active_starsystems, 0, sizeof(active_starsystems));
	pthread_mutex_lock(&service_mutex);
	for (i = 0; i < nbridge_locations; i++) {
		for (j = 0; j < nstarmap_entries; j++) {
			if (strcmp(starmap[j].name, bridge_location[i]) == 0) {
				active_starsystems[j] = 1;
				break;
			}
			/* But if a starsystem has refcount 0, or bridge_count 0, then it's inactive */
			for (k = 0; k < nstarsystems; k++) {
				if (strcasecmp(starsystem[k].starsystem_name, starmap[j].name) == 0) {
					if (starsystem[k].refcount == 0)
						active_starsystems[j] = 0;
					if (starsystem[k].bridge_count == 0)
						active_starsystems[j] = 0;
				}
			}
		}
	}
	pthread_mutex_unlock(&service_mutex);


	/* Make a map of starsystems which are active, or adjacent to active starsystems */
	memset(adj_starsystem, 0, sizeof(adj_starsystem));
	for (i = 0; i < nstarmap_entries; i++) {
		if (active_starsystems[i]) {
			adj_starsystem[i] = 1;
			for (j = 0; j < nstarmap_entries; j++) {
				if (j == i)
					continue;
				if (starmap_stars_are_adjacent(starmap_adjacency, i, j))
					adj_starsystem[j] = 1;
			}
		}
	}

	/* printf("snis_multiverse: ---------------------------\n"); */
	for (i = 0; i < nstarmap_entries; i++) {
		if (adj_starsystem[i]) {
			/* printf("snis_multiverse: ACTIVE STAR SYSTEM %d '%s'\n", i, starmap[i].name); */
			maybe_startup_snis_server(i);
		} else {
			/* printf("snis_multiverse:        STAR SYSTEM %d '%s'\n", i, starmap[i].name); */
			maybe_shutdown_snis_server(i);
		}
	}
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

/* Also sets snis_db_dir... */
static void maybe_override_database_root(void)
{
	char *sdd = getenv("SNIS_DB_DIR");

	if (!sdd) {
		char *xdg_data_home = getenv("XDG_DATA_HOME");
		if (!xdg_data_home || xdg_data_home[0] != '/') {
			char *home = getenv("HOME");
			if (!home) {
				fprintf(stderr, "HOME is not set!\n");
				/* legacy, shouldn't get here as $HOME ought to be set */
				snprintf(snis_db_dir, sizeof(snis_db_dir), ".");
			} else {
				snprintf(snis_db_dir, PATH_MAX, "%s/.local/share/space-nerds-in-space", home);
			}
		} else {
			snprintf(snis_db_dir, PATH_MAX, "%s/space-nerds-in-space", xdg_data_home);
		}
	} else {
		snprintf(snis_db_dir, sizeof(snis_db_dir), "%s", sdd);
	}

	fprintf(stderr, "snis_db_dir = %s\n", snis_db_dir);

	database_root = malloc(PATH_MAX);
	snprintf(database_root, PATH_MAX, "%s/%s", snis_db_dir, DEFAULT_DATABASE_ROOT);
}

int main(int argc, char *argv[])
{
	struct ssgl_game_server gameserver;
	int i, rc;
	pthread_t lobby_thread;
	struct in_addr ip;

	maybe_override_database_root();
	asset_dir = override_asset_dir();
	refuse_to_run_as_root("snis_multiverse");
	create_lock_or_die();
	parse_options(argc, argv, &lobby, &nick, &location);
	read_replacement_assets(&replacement_assets, asset_dir);
	construct_starmap();

	open_log_file();
	printf("SNIS multiverse server\n");
	create_database_root_or_die(database_root);
	if (restore_data()) {
		fprintf(stderr, "Failed to restore data from checkpoint database\n");
		return -1;
	}
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
		fprintf(stderr, "snis_multiverse: Failed to get local ip address.\n");
	ip.s_addr = gameserver.ipaddr;
	fprintf(stderr, "snis_multiverse: Starting to run on %s:%d\n", inet_ntoa(ip), listener_port);
	gameserver.port = htons(listener_port);

	snprintf(gameserver.server_nickname, sizeof(gameserver.server_nickname) - 1, "%s", nick);
	snprintf(gameserver.location, sizeof(gameserver.location) - 1, "%s", location);
	strcpy(gameserver.protocol_version, SNIS_PROTOCOL_VERSION);
	strcpy(gameserver.game_type, "SNIS-MVERSE");
	strcpy(gameserver.game_instance, "-");

	/* create a thread to contact and update the lobby server... */
	(void) ssgl_register_gameserver(lobby, &gameserver, &lobby_thread, &nconnections);

	i = 0;
	do {
		ssgl_sleep(1);
		wrangle_snis_server_processes(); /* Wrangle snis_server processes at 1Hz */
		if ((i % 30) == 0)
			checkpoint_data(); /* Checkpoint data every 30 secs */
		i = (i + 1) % 30;
	} while (1);
	return 0;
}

