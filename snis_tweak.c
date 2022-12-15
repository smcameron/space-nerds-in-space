/*
	Copyright (C) 2018 Stephen M. Cameron
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
#include <stdint.h>
#include <string.h>
#include <ctype.h>
#include <regex.h>
#include <math.h>

#include "snis_tweak.h"
#include "arraysize.h"
#include "string-utils.h"

int find_tweakable_var_descriptor(struct tweakable_var_descriptor *desc, int count, char *name)
{
	int i;

	for (i = 0; i < count; i++) {
		if (!desc[i].name)
			return -1;
		if (strcmp(name, desc[i].name) == 0)
			return i;
	}
	return -1;
}

int tweak_variable(struct tweakable_var_descriptor *tweak, int count, char *cmd,
			char *msg, int msgsize)
{
	int rc;
	char variable[255], valuestr[255];
	struct tweakable_var_descriptor *v;
	float f;
	int i;
	uint8_t b;
	char *arg;

	arg = get_abbreviated_command_arg("SET", cmd);
	if (!arg) {
		if (msg)
			snprintf(msg, msgsize, "SET: INVALID SET COMMAND: %s", cmd);
		return TWEAK_BAD_SYNTAX;
	}
	rc = sscanf(arg, "%[^= ]%*[ =]%[^= ]", variable, valuestr);
	if (rc != 2) {
		if (msg)
			snprintf(msg, msgsize, "SET: INVALID ARG TO SET COMMAND: %s", cmd);
		return TWEAK_BAD_SYNTAX;
	}
	rc = find_tweakable_var_descriptor(tweak, count, variable);
	if (rc < 0) {
		if (msg)
			snprintf(msg, msgsize, "SET: INVALID VARIABLE: %s", variable);
		return TWEAK_UNKNOWN_VARIABLE;
	}
	v = &tweak[rc];
	if (v->readonly) {
		if (msg)
			snprintf(msg, msgsize, "SET: VARIABLE %s IS READ ONLY", variable);
		return TWEAK_READONLY_VARIABLE;
	}
	switch (v->type) {
	case 'f':
		if (strcmp(valuestr, "DEFAULT") == 0) {
			f = v->defaultf;
		} else {
			rc = sscanf(valuestr, "%f", &f);
			if (rc != 1) {
				if (msg)
					snprintf(msg, msgsize, "SET: UNPARSEABLE FLOAT VALUE");
				return TWEAK_BAD_SYNTAX;
			}
		}
		if (f < v->minf || f > v->maxf) {
			if (msg)
				snprintf(msg, msgsize, "SET: FLOAT VALUE OUT OF RANGE");
			return TWEAK_OUT_OF_RANGE;
		}
		*((float *) v->address) = f;
		if (msg)
			snprintf(msg, msgsize, "%s SET TO %f", variable, f);
		break;
	case 'b':
		if (strcmp(valuestr, "DEFAULT") == 0) {
			b = v->defaulti;
		} else {
			rc = sscanf(valuestr, "%hhu", &b);
			if (rc != 1) {
				snprintf(msg, msgsize, "SET: UNPARSEABLE BYTE VALUE");
				return TWEAK_BAD_SYNTAX;
			}
		}
		if (b < v->mini || b > v->maxi) {
			if (msg)
				snprintf(msg, msgsize, "SET: BYTE VALUE OUT OF RANGE");
			return TWEAK_OUT_OF_RANGE;
		}
		*((uint8_t *) v->address) = b;
		if (msg)
			snprintf(msg, msgsize, "%s SET TO %hhu", variable, b);
		break;
	case 'i':
		if (strcmp(valuestr, "DEFAULT") == 0) {
			i = v->defaulti;
		} else {
			rc = sscanf(valuestr, "%d", &i);
			if (rc != 1) {
				if (msg)
					snprintf(msg, msgsize, "SET: UNPARSEABLE INT VALUE");
				return TWEAK_BAD_SYNTAX;
			}
		}
		if (i < v->mini || i > v->maxi) {
			if (msg)
				snprintf(msg, msgsize, "SET: INT VALUE OUT OF RANGE");
			return TWEAK_OUT_OF_RANGE;
		}
		*((int *) v->address) = i;
		if (msg)
			snprintf(msg, msgsize, "%s SET TO %d", variable, i);
		break;
	default:
		if (msg)
			snprintf(msg, msgsize, "VARIABLE NOT SET, UNKNOWN TYPE.");
		return TWEAK_UNKNOWN_TYPE;
	}
	return 0;
}

void tweakable_vars_list(struct tweakable_var_descriptor *tweak, char *regexp_pattern,
				int count, void (*printfn)(const char *, ...))
{
	int i, rc;
	struct tweakable_var_descriptor *v;
	regex_t re;
	char *pattern = NULL; /* Assume there is no regexp pattern */

	if (regexp_pattern) {
		/* Cut off any leading "VARS " and extract the regex pattern following it.*/
		pattern = get_abbreviated_command_arg("VARS", regexp_pattern);
		if (!pattern) {
			printfn("FAILED TO EXTRACT REGEXP PATTERN");
			return;
		}
		rc = regcomp(&re, pattern, REG_NOSUB | REG_ICASE);
		if (rc) {
			printfn("FAILED TO COMPILE REGEXP '%s'", regexp_pattern);
			return;
		}
	}

	for (i = 0; i < count; i++) {
		v = &tweak[i];
		if (!v->name)
			break;
		if (pattern) {
			rc = regexec(&re, v->name, 0, NULL, 0);
			if (rc) { /* no match on name, try description */
				rc = regexec(&re, v->description, 0, NULL, 0);
				if (rc) /* no match on description */
					continue;
			}
		}
		switch (v->type) {
		case 'f':
			printfn("%s = %.2f (D=%.2f, MN=%.2f, MX=%.2f)%s", v->name,
					*((float *) v->address), v->defaultf, v->minf, v->maxf,
						v->readonly ? " READ ONLY" : "");
			break;
		case 'b':
			printfn("%s = %hhu (D=%d, MN=%d, MX=%d)%s", v->name,
					*((uint8_t *) v->address), v->defaulti, v->mini, v->maxi,
						v->readonly ? " READ ONLY" : "");
			break;
		case 'i':
			printfn("%s = %d (D=%d,MN=%d,MX=%d)%s", v->name,
					*((int32_t *) v->address), v->defaulti, v->mini, v->maxi,
						v->readonly ? " READ ONLY" : "");
			break;
		default:
			printfn("%s = ? (unknown type '%c')", v->name, v->type);
			break;
		}
	}
}

int tweakable_var_describe(struct tweakable_var_descriptor *tweak, int count, char *cmd,
				void (*printfn)(const char *, ...), int suppress_unknown_var)
{
	int rc;
	char variable[255];
	struct tweakable_var_descriptor *v;
	char *arg;

	arg = get_abbreviated_command_arg("DESCRIBE", cmd);
	if (!arg) {
		printfn("DESCRIBE: BAD COMMAND '%s'", cmd);
		return TWEAK_BAD_SYNTAX;
	}
	rc = sscanf(arg, "%s", variable);
	if (rc != 1) {
		printfn("DESCRIBE: INVALID USAGE. USE E.G., DESCRIBE MAX_PLAYER_VELOCITY");
		return TWEAK_BAD_SYNTAX;
	}
	rc = find_tweakable_var_descriptor(tweak, count, variable);
	if (rc < 0) {
		if (!suppress_unknown_var)
			printfn("DESCRIBE: INVALID VARIABLE NAME");
		return TWEAK_UNKNOWN_VARIABLE;
	}
	v = &tweak[rc];
	printfn(variable);
	printfn("   DESC: %s", v->description);
	printfn("   TYPE: %c", toupper(v->type));
	switch (v->type) {
	case 'b':
		printfn("   CURRENT VALUE: %hhu%s", *((uint8_t *) v->address), v->readonly ? " READ ONLY" : "");
		printfn("   DEFAULT VALUE: %d", v->defaulti);
		printfn("   MINIMUM VALUE: %d", v->mini);
		printfn("   MAXIMUM VALUE: %d", v->maxi);
		break;
	case 'i':
		printfn("   CURRENT VALUE: %d%s", *((int *) v->address), v->readonly ? " READ ONLY" : "");
		printfn("   DEFAULT VALUE: %d", v->defaulti);
		printfn("   MINIMUM VALUE: %d", v->mini);
		printfn("   MAXIMUM VALUE: %d", v->maxi);
		break;
	case 'f':
		printfn("   CURRENT VALUE: %f%s", *((float *) v->address), v->readonly ? " READ ONLY" : "");
		printfn("   DEFAULT VALUE: %f", v->defaultf);
		printfn("   MINIMUM VALUE: %f", v->minf);
		printfn("   MAXIMUM VALUE: %f", v->maxf);
		break;
	default:
		printfn("   VARIABLE HAS UNKNOWN TYPE");
		return TWEAK_UNKNOWN_TYPE;
	}
	return 0;
}

/* Saves tweaked variables (variables which have a value that is different from the default)
 * into a script which can later be run to restore the tweaked values.
 * f should be a FILE * opened for writing.
 */
void tweakable_vars_export_tweaked_vars(FILE *f, struct tweakable_var_descriptor *tweak, int count)
{
	for (int i = 0; i < count; i++) {
		if (tweak[i].readonly)
			continue;
		switch (tweak[i].type) {
		case 'f': {
			float *v = tweak[i].address;
			if (fabsf(*v - tweak[i].defaultf) > 0.00001)
				fprintf(f, "set %s=%g\n", tweak[i].name, *v);
			break;
		}
		case 'i': {
			int *v = tweak[i].address;
			if (*v != tweak[i].defaulti)
				fprintf(f, "set %s=%d\n", tweak[i].name, *v);
			break;
		}
		case 'b': {
			uint8_t *v = tweak[i].address;
			if (*v != (uint8_t) tweak[i].defaulti)
				fprintf(f, "set %s=%hhu\n", tweak[i].name, *v);
			break;
		}
		default:
			break;
		}
	}
}

int tweakable_vars_print_tweaked_vars(struct tweakable_var_descriptor *tweak, int count,
                                void (*printfn)(const char *fmt, ...))
{
	int tweak_count = 0;
	for (int i = 0; i < count; i++) {
		if (tweak[i].readonly)
			continue;
		switch (tweak[i].type) {
		case 'f': {
			float *v = tweak[i].address;
			if (fabsf(*v - tweak[i].defaultf) > 0.00001) {
				printfn("%s = %g", tweak[i].name, *v);
				tweak_count++;
			}
			break;
		}
		case 'i': {
			int *v = tweak[i].address;
			if (*v != tweak[i].defaulti) {
				printfn("%s = %d", tweak[i].name, *v);
				tweak_count++;
			}
			break;
		}
		case 'b': {
			uint8_t *v = tweak[i].address;
			if (*v != (uint8_t) tweak[i].defaulti) {
				printfn("%s = %hhu", tweak[i].name, *v);
				tweak_count++;
			}
			break;
		}
		default:
			break;
		}
	}
	return tweak_count;
}
