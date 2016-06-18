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
#include "string-utils.h"
#include "nonuniform_random_sampler.h"

static int parse_error(char *filename, char *line, int ln, char *bad_word)
{
	if (bad_word)
		printf("Error at line %s:%d at %s\n", filename, ln, bad_word);
	else
		printf("Error at line %s:%d\n", filename, ln);
	return -1;
}

static int parse_float_field(char *filename, char *line, int ln, float *value, char **saveptr)
{
	char *c;
	char word[100];
	int rc;

	c = strtok_r(NULL, ",", saveptr);
	if (!c)
		return parse_error(filename, line, ln, NULL);
	strcpy(word, c);
	clean_spaces(word);
	rc = sscanf(word, "%f", value);
	if (rc != 1)
		return parse_error(filename, line, ln, word);
	return 0;
}

static int parse_int_field(char *filename, char *line, int ln, int *value, char **saveptr)
{
	char *c;
	char word[100];
	int rc;

	c = strtok_r(NULL, ",", saveptr);
	if (!c)
		return parse_error(filename, line, ln, NULL);
	strcpy(word, c);
	clean_spaces(word);
	rc = sscanf(word, "%d", value);
	if (rc != 1)
		return parse_error(filename, line, ln, word);
	return 0;
}

static int parse_line(char *filename, char *line, int ln, struct commodity *c)
{
	char *x, *saveptr;
	char word[100];
	int rc;

	if (line[0] == '#')
		return 1;
	clean_spaces(line);
	if (strcmp(line, "") == 0)
		return 1;

	/* Name */
	x = strtok_r(line, ",", &saveptr);
	if (!x)
		return parse_error(filename, line, ln, NULL);
	strcpy(word, x);
	clean_spaces(word);
	uppercase(word);
	strcpy(c->name, word);

	/* unit */
	x = strtok_r(NULL, ",", &saveptr);
	if (!x)
		return parse_error(filename, line, ln, NULL);
	strcpy(word, x);
	clean_spaces(word);
	uppercase(word);
	strcpy(c->unit, word);

	rc = parse_float_field(filename, line, ln, &c->base_price, &saveptr);
	if (rc)
		return rc;
	rc = parse_float_field(filename, line, ln, &c->volatility, &saveptr);
	if (rc)
		return rc;
	rc = parse_float_field(filename, line, ln, &c->legality, &saveptr);
	if (rc)
		return rc;
	rc = parse_float_field(filename, line, ln, &c->econ_sensitivity, &saveptr);
	if (rc)
		return rc;
	rc = parse_float_field(filename, line, ln, &c->govt_sensitivity, &saveptr);
	if (rc)
		return rc;
	rc = parse_float_field(filename, line, ln, &c->tech_sensitivity, &saveptr);
	if (rc)
		return rc;
	rc = parse_int_field(filename, line, ln, &c->odds, &saveptr);
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

/* economy, tech_level, government will be between 0.0 and 1.0 indicating the
 * "wealthiness", "techiness", and "government stability" of the planet,
 * respectively.
 */
float commodity_calculate_price(struct commodity *c,
		float economy, float tech_level, float government)
{
	float econ_price_factor, tech_level_factor, government_factor, price;

	econ_price_factor = 2.0f * (economy - 0.5f) * c->econ_sensitivity;
	tech_level_factor = 2.0f * (tech_level - 0.5f) * c->tech_sensitivity;
	government_factor = 2.0f * (government - 0.5f) * c->govt_sensitivity;

	price = c->base_price +
		c->base_price * econ_price_factor +
		c->base_price * tech_level_factor +
		c->base_price * government_factor;

	/* TODO: add some random variability, but not enough to swamp. */
	return price;
}

#ifdef TESTCOMMODITIES

static void test_price(struct commodity *c, float e, float g, float t)
{
	float p;

	p = commodity_calculate_price(c, e, t, g);
	printf("%s %0.2f %0.2f %0.2f %3.2f\n", c->name, e, t, g, p);
}

int main(int argc, char *argv[])
{
	struct commodity *c;
	float e, g, t;
	int i, ncommodities;

	if (argc < 2) {
		fprintf(stderr, "usage: test-commodities commodities-file\n");
		return 1;
	}
	c = read_commodities(argv[1], &ncommodities);
	if (!c) {
		fprintf(stderr, "Failed to read commodities file %s\n", argv[1]);
		return 1;
	}

	for (i = 0; i < ncommodities; i++)
		for (e = -1.0; e <= 1.0; e += 0.1)
			for (g = -1.0; g <= 1.0; g += 0.1)
				for (t = -1.0; t <= 1.0; t += 0.1)
					test_price(&c[i], e, g, t);
	return 0;
}
#endif
