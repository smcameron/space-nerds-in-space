/*
	Copyright (C) 2014 Stephen M. Cameron
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
#include <ctype.h>

#include "commodities.h"

#if 0
struct commodity {
	char name[40];
	char unit[40];
	float base_price;
	float volatility;
	float legality;
	float government_adjust;
	float economy_adjust;
	float tech_adjust;
};
#endif

static void clean_spaces(char *line)
{
	char *s, *d;
	int skip_spaces = 1;

	s = line;
	d = line;

	while (*s) {
		if ((*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r') && skip_spaces) {
			s++;
			continue;
		}
		skip_spaces = 0;
		if (*s == '\t' || *s == '\n' || *s == '\r')
			*s = ' ';
		if (*s == ' ')
			skip_spaces = 1;
		*d = *s;
		s++;
		d++;
	}
	*d = '\0';
}

static int parse_error(char *filename, char *line, int ln, char *bad_word)
{
	if (bad_word)
		printf("Error at line %s:%d at %s\n", filename, ln, bad_word);
	else
		printf("Error at line %s:%d\n", filename, ln);
	return -1;
}

static int parse_float_field(char *filename, char *line, int ln, float *value)
{
	char *c;
	char word[100];
	int rc;

	c = strtok(NULL, ",");
	if (!c)
		return parse_error(filename, line, ln, NULL);
	strcpy(word, c);
	clean_spaces(word);
	rc = sscanf(word, "%f", value);
	if (rc != 1)
		return parse_error(filename, line, ln, word);
	return 0;
}

static void uppercase(char *w)
{
	char *i;

	for (i = w; *i; i++)
		*i = toupper(*i);
}

static int parse_line(char *filename, char *line, int ln, struct commodity *c)
{
	char *x;
	char word[100];
	int rc;

	if (line[0] == '#')
		return 1;
	clean_spaces(line);
	if (strcmp(line, "") == 0)
		return 1;

	/* Name */
	x = strtok(line, ",");
	if (!x)
		return parse_error(filename, line, ln, NULL);
	strcpy(word, x);
	clean_spaces(word);
	uppercase(word);
	strcpy(c->name, word);

	/* unit */
	x = strtok(NULL, ",");
	if (!x)
		return parse_error(filename, line, ln, NULL);
	strcpy(word, x);
	clean_spaces(word);
	uppercase(word);
	strcpy(c->unit, word);

	rc = parse_float_field(filename, line, ln, &c->base_price);
	if (rc)
		return rc;
	rc = parse_float_field(filename, line, ln, &c->volatility);
	if (rc)
		return rc;
	rc = parse_float_field(filename, line, ln, &c->legality);
	if (rc)
		return rc;
	rc = parse_float_field(filename, line, ln, &c->government_adjust);
	if (rc)
		return rc;
	rc = parse_float_field(filename, line, ln, &c->economy_adjust);
	if (rc)
		return rc;
	rc = parse_float_field(filename, line, ln, &c->tech_adjust);
	if (rc)
		return rc;
	return 0;
}

#define MAX_COMMODITIES 100

struct commodity *read_commodities(char *filename, int *ncommodities)
{
	FILE *f;
	char line[1000];
	char *l;
	struct commodity c, *clist;
	int rc, ln = 0;
	int n = 0;

	n = 0;
	clist = malloc(sizeof(*clist) * MAX_COMMODITIES);
	memset(clist, 0, sizeof(*clist) * MAX_COMMODITIES);

	f = fopen(filename, "r");
	if (!f)
		return NULL;

	while (!feof(f)) {
		l = fgets(line, 1000, f);
		if (!l)
			break;
		line[strlen(line) - 1] = '\0';
		ln++;
		rc = parse_line(filename, line, ln, &c);
		switch (rc) {
		case 0:
			clist[n] = c;
			n++;
#if 0
			printf("%s:%s:%f:%f:%f:%f:%f:%f\n",
				c.name, c.unit,
				c.base_price,
				c.volatility,
				c.legality,
				c.government_adjust,
				c.economy_adjust,
				c.tech_adjust);
#endif
			continue;
		case 1: /* comment */
			continue;
		case -1:
			return NULL;
		}
	}
	*ncommodities = n;
	return clist;
}
