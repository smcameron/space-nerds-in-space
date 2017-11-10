/*
	Copyright (C) 2017 Stephen M. Cameron
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
#include <string.h>
#include <limits.h>
#include <errno.h>

#include "joystick_config.h"
#include "string-utils.h"

#define MAX_DEVICES 6
#define MAX_BUTTONS 30
#define MAX_AXES 30
#define MAX_FUNCTIONS 256
#define MAX_MODES 20

struct button_function_map_entry {
	char *name;
	joystick_button_fn fn;
};

struct axis_function_map_entry {
	char *name;
	joystick_axis_fn fn;
};

struct joystick_config {
	int njoysticks;
	char *joystick_device[PATH_MAX];
	joystick_button_fn button[MAX_MODES][MAX_DEVICES][MAX_BUTTONS];
	joystick_axis_fn axis[MAX_MODES][MAX_DEVICES][MAX_AXES];
	struct button_function_map_entry button_fn_map[MAX_FUNCTIONS];
	struct axis_function_map_entry axis_fn_map[MAX_FUNCTIONS];
	int nbutton_fns;
	int naxis_fns;
};

void set_joystick_axis_fn(struct joystick_config *cfg, char *function_name, joystick_axis_fn axis_fn)
{
	if (cfg->naxis_fns < MAX_FUNCTIONS) {
		cfg->axis_fn_map[cfg->naxis_fns].name = strdup(function_name);
		cfg->axis_fn_map[cfg->naxis_fns].fn = axis_fn;
		cfg->naxis_fns++;
	}
}

void set_joystick_button_fn(struct joystick_config *cfg, char *function_name, joystick_button_fn button_fn)
{
	if (cfg->nbutton_fns < MAX_FUNCTIONS) {
		cfg->button_fn_map[cfg->nbutton_fns].name = strdup(function_name);
		cfg->button_fn_map[cfg->nbutton_fns].fn = button_fn;
		cfg->nbutton_fns++;
	}
}

joystick_button_fn lookup_button_fn(struct joystick_config *cfg, char *name)
{
	int i;

	for (i = 0; i < cfg->nbutton_fns; i++)
		if (strcmp(cfg->button_fn_map[i].name, name) == 0)
			return cfg->button_fn_map[i].fn;
	return NULL;
}

joystick_axis_fn lookup_axis_fn(struct joystick_config *cfg, char *name)
{
	int i;

	for (i = 0; i < cfg->naxis_fns; i++)
		if (strcmp(cfg->axis_fn_map[i].name, name) == 0)
			return cfg->axis_fn_map[i].fn;
	return NULL;
}

static void squash_comments(char *s)
{
	int i, len;

	len = strlen(s);
	for (i = 0; i < len; i++) {
		if (s[i] == '#') {
			s[i] = '\0';
			return;
		}
	}
}

static int parse_joystick_cfg_line(struct joystick_config *cfg, char *filename, char *line, int ln,
	char *joysticks_found[], int njoysticks_found, int *current_device)
{
	int rc;
	char device[1000];
	int mode, axis, button;
	char function[1000];
	joystick_axis_fn jaf;
	joystick_button_fn jbf;

	rc = sscanf(line, "device: %s", device);
	if (rc == 1) {
		int i;

		for (i = 0; i < njoysticks_found; i++) {
			/* I wonder if I need a regexp here to handle variable parts of the names? */
			if (strcmp(device, joysticks_found[i]) == 0) {
				*current_device = i;
				return 0;
			}
		}
		/* If we get here, it means we a reading a config for a device we do not have installed */
		*current_device = -1;
		return 0;
	}
	rc = sscanf(line, " mode %d axis %d %s", &mode, &axis, function);
	if (rc == 3) {
		if (mode < 0 || mode >= MAX_MODES) {
			fprintf(stderr, "%s:%d Bad mode %d (must be between 0 and %d)\n",
				filename, ln, mode, MAX_MODES - 1);
			return 0; /* just keep going. */
		}
		if (axis < 0 || axis >= MAX_AXES) {
			fprintf(stderr, "%s:%d Bad axis %d (must be between 0 and %d)\n",
				filename, ln, axis, MAX_AXES - 1);
			return 0; /* just keep going. */
		}
		jaf = lookup_axis_fn(cfg, function);
		if (jaf == NULL) {
			fprintf(stderr, "%s:%d Bad function '%s'\n", filename, ln, function);
			return 0; /* just keep going. */
		}
		if (*current_device == -1)
			return 0; /* valid syntax, but not a device we care about */
		cfg->axis[mode][*current_device][axis] = jaf;
		return 0;
	}
	rc = sscanf(line, " mode %d button %d %s", &mode, &button, function);
	if (rc == 3) {
		if (mode < 0 || mode >= MAX_MODES) {
			fprintf(stderr, "%s:%d Bad mode %d (must be between 0 and %d)\n",
				filename, ln, mode, MAX_MODES - 1);
			return 0; /* just keep going. */
		}
		if (button < 0 || button > MAX_BUTTONS) {
			fprintf(stderr, "%s:%d Bad button %d (must be between 0 and %d)\n",
				filename, ln, button, MAX_BUTTONS - 1);
			return 0; /* just keep going. */
		}
		jbf = lookup_button_fn(cfg, function);
		if (jbf == NULL) {
			fprintf(stderr, "%s:%d Bad function '%s'\n", filename, ln, function);
			return 0; /* just keep going. */
		}
		if (*current_device == -1)
			return 0; /* valid syntax, but not a device we care about */
		cfg->button[mode][*current_device][button] = jbf;
		return 0;
	}
	/* Do not report blank lines as syntax errors. */
	if (strcmp(line, "") == 0 || strcmp(line, " ") == 0)
		return 0;
	fprintf(stderr, "%s:%d: Syntax error '%s'\n", filename, ln, line);
	return 0; /* just keep going. */
}

struct joystick_config *new_joystick_config(void)
{
	struct joystick_config *cfg = malloc(sizeof(*cfg));

	memset(cfg, 0, sizeof(*cfg));
	return cfg;
}

void free_joystick_config(struct joystick_config *cfg)
{
	int i;

	for (i = 0; i < cfg->naxis_fns; i++)
		free(cfg->axis_fn_map[i].name);
	for (i = 0; i < cfg->nbutton_fns; i++)
		free(cfg->button_fn_map[i].name);
	free(cfg);
}

int read_joystick_config(struct joystick_config *cfg, char *filename, char *joysticks_found[], int njoysticks_found)
{
	FILE *f;
	char line[1000];
	char *l;
	int rc, ln = 0;
	int current_device = -1;

	f = fopen(filename, "r");
	if (!f) {
		fprintf(stderr, "Cannot open '%s': %s\n", filename, strerror(errno));
		return -1;
	}
	rc = 0;
	while (!feof(f)) {
		l = fgets(line, 1000, f);
		if (!l)
			break;
		line[strlen(line) - 1] = '\0';
		ln++;
		squash_comments(line);
		trim_whitespace(line);
		rc += parse_joystick_cfg_line(cfg, filename, line, ln, joysticks_found,
						njoysticks_found, &current_device);
	}
	return rc;
}

void joystick_button(struct joystick_config *cfg, void *context, int mode, int device, int button)
{
	if (mode < 0 || mode >= MAX_MODES)
		return;
	if (device < 0 || device >= MAX_DEVICES)
		return;
	if (button < 0 || button >= MAX_BUTTONS)
		return;
	if (cfg->button[mode][device][button])
		cfg->button[mode][device][button](context);
}

void joystick_axis(struct joystick_config *cfg, void *context, int mode, int device, int axis, int value)
{
	if (mode < 0 || mode >= MAX_MODES)
		return;
	if (device < 0 || device >= MAX_DEVICES)
		return;
	if (axis < 0 || axis >= MAX_AXES)
		return;
	if (cfg->axis[mode][device][axis])
		cfg->axis[mode][device][axis](context, value);
}
