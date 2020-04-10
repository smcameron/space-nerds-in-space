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

#define MAXCATEGORIES 26
#define MAXCOMMODITIES_PER_CATEGORY 26
static char *category[MAXCATEGORIES];
static char *category_description[MAXCATEGORIES];
static int ncategories = 0;

static int lookup_category(const char *c)
{
	int i;

	for (i = 0; i < ncategories; i++)
		if (strcmp(category[i], c) == 0)
			return i;
	return -1;
}

static int add_category(char *c)
{
	int len = strlen(c);
	char *n;
	char *comma;

	n = strchr(c, ':');
	if (!n) {
		fprintf(stderr, "Bad category, expected colon: '%s'\n", c);
		return -1;
	}
	if ((n - c) >= len) {
		fprintf(stderr, "Bad category, expected category name: '%s'\n", c);
		return -1;
	}
	comma = strchr(n, ',');
	if (!comma) {
		fprintf(stderr, "Bad category, expected comma and description: '%s'\n", c);
		return -1;
	}
	if ((comma - c) >= len - 2) {
		fprintf(stderr, "Bad category, expected category description: '%s'\n", c);
		return -1;
	}
	*comma = '\0';
	if (ncategories >= MAXCATEGORIES) {
		fprintf(stderr, "Too many categories, dropping '%s'\n", n + 1);
		return -1;
	}
	category[ncategories] = strdup(n + 1);
	category_description[ncategories] = strdup(comma + 1);
	uppercase(category[ncategories]);
	uppercase(category_description[ncategories]);
	clean_spaces(category_description[ncategories]);
	ncategories++;
	return ncategories - 1;
}

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
	strnzcpy(word, c, sizeof(word));
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
	strnzcpy(word, c, sizeof(word));
	clean_spaces(word);
	rc = sscanf(word, "%d", value);
	if (rc != 1)
		return parse_error(filename, line, ln, word);
	return 0;
}

static void sanity_check_float(char *filename, int ln, char *quantity, float v, float min, float max)
{
	if (v >= min && v <= max)
		return;
	fprintf(stderr, "%s:%d %s has value %f, out of range %f - %f\n",
		filename, ln, quantity, v, min, max);
}

static void sanity_check_commodity_values(char *filename, int ln, struct commodity *c)
{
	sanity_check_float(filename, ln, "base_price", c->base_price, 0.0, 1e6);
	sanity_check_float(filename, ln, "volatility", c->volatility, 0.0, 1.0);
	sanity_check_float(filename, ln, "legality", c->volatility, 0.0, 1.0);
	sanity_check_float(filename, ln, "econ sensitivity", c->econ_sensitivity, 0.0, 1.0);
	sanity_check_float(filename, ln, "govt sensitivity", c->govt_sensitivity, 0.0, 1.0);
	sanity_check_float(filename, ln, "tech sensitivity", c->tech_sensitivity, 0.0, 1.0);
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

	if (strncmp(line, "category:", 9) == 0) {
		rc = add_category(line);
		if (rc < 0)
			return -1;
		return 1;
	}

	/* Category */
	x = strtok_r(line, ",", &saveptr);
	if (!x)
		return parse_error(filename, line, ln, NULL);
	strnzcpy(word, x, sizeof(word));
	clean_spaces(word);
	uppercase(word);
	rc = lookup_category(word);
	if (rc < 0) {
		fprintf(stderr, "Bad category: '%s'\n", word);
		return parse_error(filename, line, ln, NULL);
	}
	c->category = rc;

	/* Name */
	x = strtok_r(NULL, ",", &saveptr);
	if (!x)
		return parse_error(filename, line, ln, NULL);
	strnzcpy(word, x, sizeof(word));
	clean_spaces(word);
	uppercase(word);
	strnzcpy(c->name, word, sizeof(c->name));
	if (strcmp(c->name, word) != 0) {
		fprintf(stderr, "%s:%d: '%s' truncated to %lu chars.\n", filename, ln, word,
			(unsigned long) strlen(c->name));
		fprintf(stderr, "   '%s' -> '%s'\n", word, c->name);
	}

	/* unit */
	x = strtok_r(NULL, ",", &saveptr);
	if (!x)
		return parse_error(filename, line, ln, NULL);
	strnzcpy(word, x, sizeof(word));
	clean_spaces(word);
	uppercase(word);
	strnzcpy(c->unit, word, sizeof(c->unit));
	if (strcmp(c->unit, word) != 0) {
		fprintf(stderr, "%s:%d: '%s' truncated to %lu chars.\n", filename, ln, word,
			(unsigned long) strlen(c->unit));
		fprintf(stderr, "   '%s' -> '%s'\n", word, c->unit);
	}

	/* scans_as */
	x = strtok_r(NULL, ",", &saveptr);
	if (!x)
		return parse_error(filename, line, ln, NULL);
	strnzcpy(word, x, sizeof(word));
	clean_spaces(word);
	uppercase(word);
	strnzcpy(c->scans_as, word, sizeof(c->scans_as));
	if (strcmp(c->scans_as, word) != 0) {
		fprintf(stderr, "%s:%d: '%s' truncated to %lu chars.\n", filename, ln, word,
			(unsigned long) strlen(c->scans_as));
		fprintf(stderr, "   '%s' -> '%s'\n", word, c->scans_as);
	}

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
	sanity_check_commodity_values(filename, ln, c);
	return 0;
}

static void sanity_test_commodities(struct commodity *commodity, int ncommodities)
{
	int i;
	int count[MAXCATEGORIES] = { 0 };

	for (i = 0; i < ncommodities; i++) {
		if (commodity[i].category < 0 || commodity[i].category >= ncategories) {
			fprintf(stderr, "Commodity '%s' has bad category\n", commodity[i].name);
			continue;
		}
		count[commodity[i].category]++;
	}

	for (i = 0; i < ncategories; i++) {
		if (count[i] > MAXCOMMODITIES_PER_CATEGORY) {
			fprintf(stderr, "Category %s has too many commodities %d (max %d)\n",
				category[i], count[i], MAXCOMMODITIES_PER_CATEGORY);
		}
	}
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
	if (!f) {
		if (clist)
			free(clist);
		return NULL;
	}

	while (!feof(f)) {
		l = fgets(line, 1000, f);
		if (!l)
			break;
		if (strlen(line) == 0)
			continue;
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
			fclose(f);
			return NULL;
		}
	}
	*ncommodities = n;
	fclose(f);
	sanity_test_commodities(clist, *ncommodities);
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

int add_commodity(struct commodity **c, int *ncommodities, const char *category, const char *name, const char *unit,
			const char *scans_as, float base_price, float volatility, float legality,
			float econ_sensitivity, float govt_sensitivity, float tech_sensitivity, int odds)
{
	struct commodity *newc;
	int n = *ncommodities + 1;

	/* TODO: Batch reallocs instead of doing it every single time. */
	newc = realloc(*c, n * sizeof(newc[0]));
	if (!newc)
		return -1;
	*c = newc;
	newc = &(*c)[n - 1];

	newc->category = lookup_category(category);
	strnzcpy(newc->name, name, sizeof(newc->name));
	strnzcpy(newc->unit, unit, sizeof(newc->unit));
	strnzcpy(newc->scans_as, scans_as, sizeof(newc->scans_as));
	newc->base_price = base_price;
	newc->volatility = volatility;
	newc->legality = legality;
	newc->econ_sensitivity = econ_sensitivity;
	newc->govt_sensitivity = govt_sensitivity;
	newc->tech_sensitivity = tech_sensitivity;
	newc->odds = odds;
	*ncommodities = n;
	return n - 1;
}

const char *commodity_category(int cat)
{
	if (cat < 0 || cat >= ncategories)
		return NULL;
	return category[cat];
}

const char *commodity_category_description(int cat)
{
	if (cat < 0 || cat >= ncategories)
		return NULL;
	return category_description[cat];
}

const int ncommodity_categories(void)
{
	return ncategories;
}

#ifdef TESTCOMMODITIES

static void test_price(struct commodity *c, float e, float g, float t)
{
	float p;

	p = commodity_calculate_price(c, e, t, g);
	printf("%s %s %s %0.2f %0.2f %0.2f %3.2f\n", c->name, c->unit, c->scans_as, e, t, g, p);
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
		for (e = 0.0; e <= 1.0; e += 0.1)
			for (g = 0.0; g <= 1.0; g += 0.1)
				for (t = 0.0; t <= 1.0; t += 0.1)
					test_price(&c[i], e, g, t);
	return 0;
}
#endif
