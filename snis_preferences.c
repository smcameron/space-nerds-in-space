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
#include "snis_tweak.h"

static char default_ship_name[SHIPNAME_LEN];

#define DEFAULT_SHIP_NAME_TXT "default_ship_name.txt"
#define ROLE_DEFAULTS_TXT "role_defaults.txt"
#define CLIENT_TWEAKS_TXT "snis_client_tweaks.txt"

char *snis_prefs_read_default_ship_name(struct xdg_base_context *cx)
{
	char *name = xdg_base_slurp_file(cx, DEFAULT_SHIP_NAME_TXT);
	if (!name)
		return NULL;
	strlcpy(default_ship_name, name, SHIPNAME_LEN);
	free(name);
	return default_ship_name;
}

void snis_prefs_save_default_ship_name(struct xdg_base_context *cx, char *name)
{
	int rc, fd, bytes_to_write;

	strlcpy(default_ship_name, name, SHIPNAME_LEN);

	fd = xdg_base_open_for_overwrite(cx, DEFAULT_SHIP_NAME_TXT);
	if (fd < 0) {
		fprintf(stderr, "Failed to open file %s: %s\n", DEFAULT_SHIP_NAME_TXT, strerror(errno));
		return;
	}
	bytes_to_write = strlen(default_ship_name);
	rc = write(fd, default_ship_name, bytes_to_write);
	if (rc != bytes_to_write) {
		fprintf(stderr, "Failed to write to %s: %s\n", DEFAULT_SHIP_NAME_TXT, strerror(errno));
	}
	fsync(fd);
	close(fd);
}

void snis_prefs_save_checkbox_defaults(struct xdg_base_context *cx, int role_main_v, int role_nav_v, int role_weap_v,
					int role_eng_v, int role_damcon_v, int role_sci_v,
					int role_comms_v, int role_sound_v, int role_demon_v,
					int role_text_to_speech_v, int create_ship_v, int join_ship_v)
{
	FILE *f = xdg_base_fopen_for_write(cx, ROLE_DEFAULTS_TXT);
	if (!f) {
		fprintf(stderr, "%s:%d: Failed to open %s for write: %s. Not writing preference defaults.\n",
			__FILE__, __LINE__, ROLE_DEFAULTS_TXT, strerror(errno));
		return;
	}
	fprintf(f, "%d %d %d %d %d %d %d %d %d %d %d %d\n",
		role_main_v, role_nav_v, role_weap_v, role_eng_v, role_damcon_v,
		role_sci_v, role_comms_v, role_sound_v, role_demon_v,
		role_text_to_speech_v, create_ship_v, join_ship_v);
	fclose(f);
}

void snis_prefs_read_checkbox_defaults(struct xdg_base_context *cx, int *role_main_v, int *role_nav_v, int *role_weap_v,
					int *role_eng_v, int *role_damcon_v, int *role_sci_v,
					int *role_comms_v, int *role_sound_v, int *role_demon_v,
					int *role_text_to_speech_v, int *create_ship_v, int *join_ship_v)
{
	int rc;
	int value[12] = { 0 };

	FILE *f = xdg_base_fopen_for_read(cx, ROLE_DEFAULTS_TXT);
	if (!f)
		return;
	rc = fscanf(f, "%d %d %d %d %d %d %d %d %d %d %d %d",
		&value[0], &value[1], &value[2], &value[3],
		&value[4], &value[5], &value[6], &value[7],
		&value[8], &value[9], &value[10], &value[11]);
	if (rc != 12) {
		fclose(f);
		return;
	}
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

void snis_prefs_read_client_tweaks(struct xdg_base_context *cx,
		int (*tweak_fn)(char *cmd, int suppress_unknown_var_error))
{
	char buffer[1024];
	char *x;
	size_t i;
	FILE *f = xdg_base_fopen_for_read(cx, CLIENT_TWEAKS_TXT);

	if (!f)
		return;
	do {
		x = fgets(buffer, sizeof(buffer), f);
		if (!x) {
			if (errno == EINTR)
				continue;
			else
				break;
		}
		/* remove the newline */
		i = strlen(buffer);
		buffer[i - 1] = '\0';
		uppercase(buffer);
		tweak_fn(buffer, 0);
	} while (1);
	fclose(f);
}

void snis_prefs_save_client_tweaks(struct xdg_base_context *cx, struct tweakable_var_descriptor *tweaks, int count)
{
	FILE *f = xdg_base_fopen_for_write(cx, CLIENT_TWEAKS_TXT);
	if (!f)
		return;
	tweakable_vars_export_tweaked_vars(f, tweaks, count);
	fclose(f);
}
