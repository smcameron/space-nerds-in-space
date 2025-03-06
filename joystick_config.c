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
#include <sys/types.h>
#include <regex.h>

#include "joystick_config.h"
#include "string-utils.h"
#include "arraysize.h"

#define MAX_DEVICES 6
#define MAX_BUTTONS 40
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
	int invert[MAX_MODES][MAX_DEVICES][MAX_AXES];
	int deadzone[MAX_MODES][MAX_DEVICES][MAX_AXES];
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

static int regex_match(char *regex_pattern, char *text)
{
	int rc;
	regmatch_t match;
	regex_t re;

	rc = regcomp(&re, regex_pattern, 0);
	if (rc) {
		fprintf(stderr, "Failed to compile regular expression '%s'\n", regex_pattern);
		return 0;
	}
	rc = regexec(&re, text, 1, &match, 0);
	if (rc == 0) /* match */
		return 1;
	return 0;
}

/* If this ever needs to be 2 digits, some re-working will be required */
#define MODE_ALL (9)

/* This function turns lines like "mode main" to "mode 0", "mode science" to "mode 4" */
static void translate_mode_names(char *line)
{
	int i, rc, m;
	char *s;
	const struct mode_table_entry {
		char *name;
		int number;
	} mode[] = {
		{ "main", 0 },
		{ "navigation", 1 },
		{ "weapons", 2 },
		{ "engineering", 3 },
		{ "science", 4 },
		{ "comms", 5 },
		{ "demon", 6 },
		{ "damcon", 7 },
		{ "all", MODE_ALL },
	};
	char modename[255];

	/* Find the first word after "mode" */
	rc = sscanf(line, " mode %s%*[^	 ]", modename);
	if (rc != 1)
		return;

	/* Figure out which mode number it is... */
	m = -1;
	for (i = 0; (size_t) i < ARRAYSIZE(mode); i++) {
		if (strcmp(mode[i].name, modename) == 0) {
			m = i;
			break;
		}
	}
	if (m < 0)
		return;
	s = strstr(line, mode[m].name);
	if (!s)
		return;

	/* Replace the mode name with its number, or 'x' for all. */
	*s = mode[m].number + '0';
	char *start = s + 1;
	s = s + strlen(mode[m].name);
	int len = strlen(s);
	memmove(start, s, len + 1);

	/* Replace the mode name with its number, or 'x' for "all" */
}

static int parse_joystick_cfg_line(struct joystick_config *cfg, char *filename, char *line, int ln,
	char *joysticks_found[], int njoysticks_found, int *current_device)
{
	int rc;
	char device[1000];
	int axis, button, invert, deadzone;
	char function[1000];
	joystick_axis_fn jaf;
	joystick_button_fn jbf;
	static int mode = -1;
	int tmpmode;
	unsigned char dummy;

	rc = sscanf(line, "device: %s", device);
	if (rc == 1) {
		int i;

		for (i = 0; i < njoysticks_found; i++) {
			if (regex_match(device, joysticks_found[i])) {
				*current_device = i;
				return 0;
			}
		}
		/* If we get here, it means we a reading a config for a device we do not have installed */
		*current_device = -1;
		return 0;
	}
	translate_mode_names(line);

	/* Check for mode lines by themselves, %c is to check if we hit EOL */
	rc = sscanf(line, " mode %d%c", &tmpmode, &dummy);
	if (rc == 1 || (rc == 2 && dummy == '\n')) {
		mode = tmpmode;
		return 0;
	}

	invert = 1; /* not inverted */
	rc = sscanf(line, " mode %d invert axis %d %s %d", &mode, &axis, function, &deadzone);
	if (rc >= 3)
		invert = -1; /* inverted */
	else
		rc = sscanf(line, " mode %d axis %d %s %d", &mode, &axis, function, &deadzone);

	/* Maybe mode is implicit */
	if (rc < 3 && mode >= 0) {
		rc = sscanf(line, " invert axis %d %s %d", &axis, function, &deadzone);
		if (rc >= 2)
			invert = -1; /* inverted */
		else
			rc = sscanf(line, " axis %d %s %d", &axis, function, &deadzone);
		if (rc >= 2)
			rc++; /* plus 1 for the implicit "mode" */
	}

	if (rc >= 3) {
#define DEFAULT_DEADZONE_VALUE 6000
		if (rc < 4)
			deadzone = DEFAULT_DEADZONE_VALUE;
		if (mode < 0 || (mode >= MAX_MODES && mode != MODE_ALL)) {
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
		if (mode != MODE_ALL) {
			cfg->axis[mode][*current_device][axis] = jaf;
			cfg->invert[mode][*current_device][axis] = invert;
			cfg->deadzone[mode][*current_device][axis] = deadzone;
		} else {
			fprintf(stderr, "Setting joystick axis for all modes\n");
			for (int i = 0; i < 8; i++) {
				cfg->axis[i][*current_device][axis] = jaf;
				cfg->invert[i][*current_device][axis] = invert;
				cfg->deadzone[i][*current_device][axis] = deadzone;
			}
		}
		return 0;
	}
	rc = sscanf(line, " mode %d button %d %s", &mode, &button, function);

	/* Maybe mode is implicit */
	if (rc != 3 && mode >= 0) {
		rc = sscanf(line, " button %d %s", &button, function);
		if (rc >= 2)
			rc++; /* plus 1 for implicit mode */
	}

	if (rc == 3) {
		if (mode < 0 || (mode >= MAX_MODES && mode != MODE_ALL)) {
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
		if (mode != MODE_ALL) {
			cfg->button[mode][*current_device][button] = jbf;
		} else {
			fprintf(stderr, "Setting joystick button for all modes\n");
			for (int i = 0; i < 8; i++)
				cfg->button[i][*current_device][button] = jbf;
		}
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
		if (strlen(line) == 0)
			continue;
		line[strlen(line) - 1] = '\0';
		ln++;
		squash_comments(line);
		trim_whitespace(line);
		rc += parse_joystick_cfg_line(cfg, filename, line, ln, joysticks_found,
						njoysticks_found, &current_device);
	}
	fclose(f);
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
	if (cfg->axis[mode][device][axis] &&
		((value >= cfg->deadzone[mode][device][axis])
		|| (value <= -cfg->deadzone[mode][device][axis])))
		cfg->axis[mode][device][axis](context, value * cfg->invert[mode][device][axis]);
}
