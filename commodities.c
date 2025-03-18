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

#define DEBUG_COMMODITIES 0

#define MAXCATEGORIES 26
#define MAXCOMMODITIES_PER_CATEGORY 26
static char *category[MAXCATEGORIES];
static char *category_description[MAXCATEGORIES];
static int ncategories = 0;
static float category_econ_sensitivity[MAXCATEGORIES];
static float category_govt_sensitivity[MAXCATEGORIES];
static float category_tech_sensitivity[MAXCATEGORIES];

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
	char name[100];
	char desc[100];
	float e, g, t;
	int rc;

	if (ncategories >= MAXCATEGORIES) {
		fprintf(stderr, "Too many categories, dropping '%s'\n", c);
		return -1;
	}
	rc = sscanf(c, "category: %[a-zA-Z ], %[a-zA-Z ], %f, %f, %f", name, desc, &e, &g, &t);
	if (rc != 5) {
		fprintf(stderr, "Bad category: '%s'\n", c);
		return -1;
	}
	category[ncategories] = strdup(name);
	category_description[ncategories] = strdup(desc);
	uppercase(category[ncategories]);
	uppercase(category_description[ncategories]);
	clean_spaces(category_description[ncategories]);
	clean_spaces(category[ncategories]);
	category_econ_sensitivity[ncategories] = e;
	category_govt_sensitivity[ncategories] = g;
	category_tech_sensitivity[ncategories] = t;
	ncategories++;
	return ncategories - 1;
}

static int parse_error(char *filename, char *line, int ln, char *bad_word)
{
	printf("%s:%d %s\n", filename, ln, line);
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
	strlcpy(word, c, sizeof(word));
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
	strlcpy(word, c, sizeof(word));
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
		sanity_check_float(filename, ln, "econ sensitivity",
					category_econ_sensitivity[ncategories - 1], -1.0, 1.0);
		sanity_check_float(filename, ln, "govt sensitivity",
					category_govt_sensitivity[ncategories - 1], -1.0, 1.0);
		sanity_check_float(filename, ln, "tech sensitivity",
					category_tech_sensitivity[ncategories - 1], -1.0, 1.0);
		return 1;
	}

	/* Category */
	x = strtok_r(line, ",", &saveptr);
	if (!x)
		return parse_error(filename, line, ln, NULL);
	strlcpy(word, x, sizeof(word));
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
	strlcpy(word, x, sizeof(word));
	clean_spaces(word);
	uppercase(word);
	strlcpy(c->name, word, sizeof(c->name));
	if (strcmp(c->name, word) != 0) {
		fprintf(stderr, "%s:%d: '%s' truncated to %lu chars.\n", filename, ln, word,
			(unsigned long) strlen(c->name));
		fprintf(stderr, "   '%s' -> '%s'\n", word, c->name);
	}

	/* unit */
	x = strtok_r(NULL, ",", &saveptr);
	if (!x)
		return parse_error(filename, line, ln, NULL);
	strlcpy(word, x, sizeof(word));
	clean_spaces(word);
	uppercase(word);
	strlcpy(c->unit, word, sizeof(c->unit));
	if (strcmp(c->unit, word) != 0) {
		fprintf(stderr, "%s:%d: '%s' truncated to %lu chars.\n", filename, ln, word,
			(unsigned long) strlen(c->unit));
		fprintf(stderr, "   '%s' -> '%s'\n", word, c->unit);
	}

	/* scans_as */
	x = strtok_r(NULL, ",", &saveptr);
	if (!x)
		return parse_error(filename, line, ln, NULL);
	strlcpy(word, x, sizeof(word));
	clean_spaces(word);
	uppercase(word);
	strlcpy(c->scans_as, word, sizeof(c->scans_as));
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
			if (clist)
				free(clist);
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
	float es, gs, ts;

	es = category_econ_sensitivity[c->category];
	gs = category_govt_sensitivity[c->category];
	ts = category_tech_sensitivity[c->category];

#if DEBUG_COMMODITIES
	printf("es = %f, gs = %f, ts = %f\n", es, gs, ts);
	printf("econ = %f, govt = %f, tech = %f\n", economy, government, tech_level);
#endif

	/* Scale to between -1.0 and +1.0 range */
	economy = -2.0f * (economy - 0.5f);
	government = -2.0f * (government - 0.5f);
	tech_level = -2.0f * (tech_level - 0.5f);

	/* economy: +1 means Rich Industrial, -1 means poor agricultural */
	/* government: +1 means Corporate State, -1 means Anarchy */
	/* Tech level: +1 means interstellar age, -1 means stone age */

	econ_price_factor = economy * es;
	government_factor = government * gs;
	tech_level_factor = tech_level * ts;
#if DEBUG_COMMODITIES
	printf("ef = %f, gf = %f, tf = %f\n", econ_price_factor, government_factor, tech_level_factor);
#endif

	price = c->base_price +
		c->base_price * econ_price_factor +
		c->base_price * government_factor +
		c->base_price * tech_level_factor;
#if DEBUG_COMMODITIES
	printf("bp = %f + e(%f), + g(%f) + t(%f) = %f\n",
			c->base_price, econ_price_factor, government_factor, tech_level_factor, price);
#endif

	/* TODO: add some random variability, but not enough to swamp. */
	return price;
}

int add_commodity(struct commodity **c, int *ncommodities, const char *category, const char *name, const char *unit,
			const char *scans_as, float base_price, float volatility, float legality, int odds)
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
	strlcpy(newc->name, name, sizeof(newc->name));
	strlcpy(newc->unit, unit, sizeof(newc->unit));
	strlcpy(newc->scans_as, scans_as, sizeof(newc->scans_as));
	newc->base_price = base_price;
	newc->volatility = volatility;
	newc->legality = legality;
	newc->odds = odds;
	*ncommodities = n;
	return n - 1;
}

int lookup_commodity(struct commodity *c, int ncommodities, const char *commodity_name)
{
	int i;

	for (i = 0; i < ncommodities; i++)
		if (strcasecmp(commodity_name, c[i].name) == 0)
			return i;
	return -1;
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

int ncommodity_categories(void)
{
	return ncategories;
}

void flatten_commodity(struct commodity *c, struct flattened_commodity *fc)
{
	snprintf(fc->name, sizeof(fc->name), "%s", c->name);
	snprintf(fc->unit, sizeof(fc->unit), "%s", c->unit);
	snprintf(fc->scans_as, sizeof(fc->scans_as), "%s", c->scans_as);
	snprintf(fc->category, sizeof(fc->category), "%d", c->category);
	snprintf(fc->base_price, sizeof(fc->base_price), "%g", c->base_price);
	snprintf(fc->volatility, sizeof(fc->volatility), "%g", c->volatility);
	snprintf(fc->legality, sizeof(fc->legality), "%g", c->legality);
}

#ifdef TESTCOMMODITIES

#include "arraysize.h"
#include "snis-culture.h"

static int test_price(struct commodity *c, float e, float g, float t)
{
	float p;

	p = commodity_calculate_price(c, e, t, g);
	if (p < 0.0) {
		fprintf(stderr, "NEGATIVE PRICE %s %s %s %0.2f %0.2f %0.2f %3.2f\n",
			c->name, c->unit, c->scans_as, e, t, g, p);
		return 1;
	}
	printf("%s %s %s %0.2f %0.2f %0.2f %3.2f\n", c->name, c->unit, c->scans_as, e, t, g, p);
	return 0;
}

static void print_flattened_commodity(struct flattened_commodity *fc)
{
	printf("\"%s\", \"%s\", \"%s\", \"%s\", \"%s\", \"%s\", \"%s\"\n",
		fc->name, fc->unit, fc->scans_as, fc->category, fc->base_price,
		fc->volatility, fc->legality);
}

static void test_flatten_commodities(struct commodity *c, int ncommodities)
{
	struct flattened_commodity fc;
	for (int i = 0; i < ncommodities; i++) {
		flatten_commodity(&c[i], &fc);
		printf("%4d: ", i);
		print_flattened_commodity(&fc);
	}
}

int main(int argc, char *argv[])
{
	struct commodity *c;
	float e, g, t;
	int i, ncommodities;
	int sum = 0;

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
					sum += test_price(&c[i], e, g, t);

	for (i = 0; i < ncategories; i++) {
		int j;
		struct commodity c;
		c.category = i;
		c.base_price = 1.0;
		for (j = 0; j < ARRAYSIZE(economy_name); j++) {
			e = (float) j / (float) ARRAYSIZE(economy_name);
			printf("%s that normally costs $1.00 would cost %f in economy %s\n",
					category[i], commodity_calculate_price(&c, e, 0.5, 0.5), economy_name[j]);
		}
		printf("------------------------\n");
		for (j = 0; j < ARRAYSIZE(government_name); j++) {
			g = (float) j / (float) ARRAYSIZE(government_name);
			printf("%s that normally costs $1.00 would cost %f in government %s\n",
					category[i], commodity_calculate_price(&c, 0.5, g, 0.5), government_name[j]);
		}
		printf("------------------------\n");
		for (j = 0; j < ARRAYSIZE(tech_level_name); j++) {
			t = (float) j / (float) ARRAYSIZE(tech_level_name);
			printf("%s that normally costs $1.00 would cost %f in tech level %s\n",
					category[i], commodity_calculate_price(&c, 0.5, 0.5, t), tech_level_name[j]);
		}
		printf("========================\n");
	}
	test_flatten_commodities(c, ncommodities);
	if (sum)
		printf("%d price failures\n", sum);
	return sum;
}
#endif
