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
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <limits.h>
#include <errno.h>

#include "snis_packet.h"
#include "string-utils.h"
#include "snis_preferences.h"

static char default_ship_name[SHIPNAME_LEN];

char *snis_prefs_read_default_ship_name(void)
{
	int n, bytes;
	char *homedir;
	char path[PATH_MAX];

	homedir = getenv("HOME");
	if (!homedir)
		return NULL;

	snprintf(path, PATH_MAX, "%s/.space-nerds-in-space/default_ship_name.txt", homedir);
	char *name = slurp_file(path, &bytes);
	if (!name)
		return NULL;
	n = strlen(name);
	if (n > SHIPNAME_LEN - 1)
		n = SHIPNAME_LEN - 1;
	strncpy(default_ship_name, name, n);
	default_ship_name[n] = '\0';
	free(name);
	return default_ship_name;
}

void snis_prefs_save_default_ship_name(char *name)
{
	char *homedir;
	char path[PATH_MAX];
	char filename[PATH_MAX];
	int rc, fd, bytes_to_write;

	homedir = getenv("HOME");
	if (!homedir)
		return;

	int n = strlen(name);
	n = strlen(name) + 1;
	if (n > SHIPNAME_LEN)
		n = SHIPNAME_LEN;
	strncpy(default_ship_name, name, n);
	default_ship_name[n] = '\0';
	snprintf(path, PATH_MAX, "%s/.space-nerds-in-space", homedir);
	rc = mkdir(path, 0755);
	if (rc != 0 && errno != EEXIST) {
		fprintf(stderr, "Failed to create directory %s: %s\n", path, strerror(errno));
		return;
	}

	snprintf(filename, PATH_MAX, "%s/%s", path, "default_ship_name.txt");
	fd = open(filename, O_CREAT | O_TRUNC | O_WRONLY, 0644);
	if (fd < 0) {
		fprintf(stderr, "Failed to open file %s: %s\n", filename, strerror(errno));
		return;
	}
	bytes_to_write = n - 1;
	rc = write(fd, default_ship_name, bytes_to_write);
	if (rc != bytes_to_write) {
		fprintf(stderr, "Failed to write to %s: %s\n", filename, strerror(errno));
	}
	fsync(fd);
	close(fd);
}

void snis_prefs_save_checkbox_defaults(int role_main_v, int role_nav_v, int role_weap_v,
					int role_eng_v, int role_damcon_v, int role_sci_v,
					int role_comms_v, int role_sound_v, int role_demon_v,
					int role_text_to_speech_v, int create_ship_v, int join_ship_v)
{
	char *homedir;
	char path[PATH_MAX];
	char filename[PATH_MAX];
	int rc;
	FILE *f;

	homedir = getenv("HOME");
	if (!homedir)
		return;
	snprintf(path, PATH_MAX, "%s/.space-nerds-in-space", homedir);
	rc = mkdir(path, 0755);
	if (rc != 0 && errno != EEXIST) {
		fprintf(stderr, "Failed to create directory %s: %s\n", path, strerror(errno));
		return;
	}
	snprintf(filename, PATH_MAX, "%s/%s", path, "role_defaults.txt");
	f = fopen(filename, "w");
	fprintf(f, "%d %d %d %d %d %d %d %d %d %d %d %d\n",
		role_main_v, role_nav_v, role_weap_v, role_eng_v, role_damcon_v,
		role_sci_v, role_comms_v, role_sound_v, role_demon_v,
		role_text_to_speech_v, create_ship_v, join_ship_v);
	fclose(f);
}

void snis_prefs_read_checkbox_defaults(int *role_main_v, int *role_nav_v, int *role_weap_v,
					int *role_eng_v, int *role_damcon_v, int *role_sci_v,
					int *role_comms_v, int *role_sound_v, int *role_demon_v,
					int *role_text_to_speech_v, int *create_ship_v, int *join_ship_v)
{
	char *homedir;
	char path[PATH_MAX];
	int rc;
	int value[12] = { 0 };
	FILE *f;

	homedir = getenv("HOME");
	if (!homedir)
		return;
	snprintf(path, PATH_MAX, "%s/.space-nerds-in-space/role_defaults.txt", homedir);
	f = fopen(path, "r");
	if (!f)
		return;
	rc = fscanf(f, "%d %d %d %d %d %d %d %d %d %d %d %d",
		&value[0], &value[1], &value[2], &value[3],
		&value[4], &value[5], &value[6], &value[7],
		&value[8], &value[9], &value[10], &value[11]);
	if (rc != 12)
		return;
	fclose(f);
	*role_main_v = value[0];
	*role_nav_v = value[1];
	*role_weap_v = value[2];
	*role_eng_v = value[3];
	*role_damcon_v = value[4];
	*role_sci_v = value[5];
	*role_comms_v = value[6];
	*role_sound_v = value[7];
	*role_demon_v = value[8];
	*role_text_to_speech_v = value[9];
	*create_ship_v = value[10];
	*join_ship_v = value[11];
	return;
}
