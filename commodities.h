#ifndef COMMODITIES_H__
#define COMMODITIES_H__
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

struct commodity {
	char name[40];
	char unit[20];
	char scans_as[20];
	int category;
	float base_price;
	float volatility;
	float legality;
	float econ_sensitivity;
	float govt_sensitivity;
	float tech_sensitivity;

	/* odds: relative odds that this commodity will appear.  The probability that
	 * a commodity will appear is its odds / (sum of all the odds).  This allows
	 * a non-uniform sampling, so that some items are rarer and others more
	 * plentiful.
	 */
	int odds;
};

struct commodity *read_commodities(char *filename, int *ncommodities);

/* economy, tech_level, government will be between 0.0 and 1.0 indicating the
 * "wealthiness", "techiness", and "government stability" of the planet,
 * respectively.
 */
float commodity_calculate_price(struct commodity *c,
		float economy, float tech_level, float government);

int add_commodity(struct commodity **c, int *ncommodities, const char *category, const char *name, const char *unit,
			const char *scans_as, float base_price, float volatility, float legality,
			float econ_sensitivity, float govt_sensitivity, float tech_sensitivity, int odds);

const char *commodity_category(int cat);
const char *commodity_category_description(int cat);
const int ncommodity_categories(void);

#endif
