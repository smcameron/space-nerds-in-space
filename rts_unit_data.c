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
#include <stdlib.h>

#include "arraysize.h"
#include "rts_unit_data.h"
#include "build_bug_on.h"

#define NOMINAL_FUEL_CAPACITY (1024 * 1024 * 2)
#define NOMINAL_FUEL_DURATION (180 /* seconds */)
#define NOMINAL_FUEL_UNIT (NOMINAL_FUEL_CAPACITY / (10 * (NOMINAL_FUEL_DURATION)))
#define FUEL_UNIT(multiplier) ((multiplier) * NOMINAL_FUEL_UNIT)

static struct rts_unit_data unit_data[] = {
	{ "SCOUT SHIP", "SPACEFARER", "S", 949.0, 100, 0.1, 0.3, NOMINAL_FUEL_CAPACITY, FUEL_UNIT(0.5)},
	{ "HEAVY BOMBER", "DISRUPTOR", "HB", 3199.0, 300, 0.7, 0.7, NOMINAL_FUEL_CAPACITY, FUEL_UNIT(3.0) },
	{ "GUN SHIP", "DREADKNIGHT", "G", 3499.0, 350, 0.4, 0.9, NOMINAL_FUEL_CAPACITY, FUEL_UNIT(1.5) },
	{ "TROOP SHIP", "TRANSPORT", "TS", 499.0, 100, 0.1, 0.2, NOMINAL_FUEL_CAPACITY, FUEL_UNIT(1.0) },
	{ "TURRET", "VANQUISHER", "T", 4399.0, 400, 0.5, 0.9, NOMINAL_FUEL_CAPACITY, FUEL_UNIT(0.3)  },
	{ "RESUPPLY SHIP", "TANKER", "R", 1599.0, 150, 0.3, 0.1, NOMINAL_FUEL_CAPACITY, FUEL_UNIT(0.1) },
};

static struct rts_order_data order_data[] = {
	{ "STAND BY", "S", "ORDER UNIT TO STAND BY", 10.0, 1, },
	{ "GUARD BASE", "G", "ORDER UNIT TO GUARD BASE", 400.0, 1, },
	{ "ESCORT", "E", "ORDER UNIT TO ESCORT FRIENDLY UNITS", 500.0, 1, },
	{ "ATK NEAR ENEMY", "AN", "$500 ORDER UNIT TO ATTACK NEAREST ENEMY UNITS", 500.0, 1, },
	{ "MOVE TO WAYPOINT", "MTW", "$200 ORDER UNIT TO MOVE TO THE SPECIFIED WAYPOINT", 200.0, 1, },
	{ "OCCUPY NEAR BASE", "OB", "$200 ORDER UNIT TO MOVE TO AND OCCUPY THE NEAREST UNOCCUPIED STARBASE", 200.0, },
	{ "ATK MAIN BASE", "AMB", "$300 ORDER UNIT TO ATTACK THE ENEMY HOME PLANET", 300.0, 1, },
	{ "RESUPPLY", "R", "$300 ORDER UNIT TO RESUPPLY DEPLETED UNITS", 300.0, 1, },
	{ "OUT OF FUEL", "E", "OUT OF FUEL", 0, 0, }, /* OUT OF FUEL is not counted in RTS_ORDER_TYPES */
};

static int orders_ok_for_unit_type[ARRAYSIZE(order_data)][ARRAYSIZE(unit_data)] = {
	{ 1, 1, 1, 1, 1, 1 }, /* STAND BY */
	{ 1, 1, 1, 1, 1, 0 }, /* GUARD BASE */
	{ 1, 1, 1, 1, 1, 0 }, /* ESCORT */
	{ 1, 1, 1, 1, 1, 0 }, /* ATK NEAR ENEMY */
	{ 1, 1, 1, 1, 1, 1 }, /* MOVE TO WAYPOINT */
	{ 0, 0, 0, 1, 0, 0 }, /* OCCUPY NEAR BASE */
	{ 1, 1, 1, 1, 1, 1 }, /* ATK MAIN BASE */
	{ 0, 0, 0, 0, 0, 1 }, /* RESUPPLY */
	{ 1, 1, 1, 1, 1, 0 }, /* OUT OF FUEL */
};

struct rts_unit_data *rts_unit_type(int n)
{
	BUILD_ASSERT(ARRAYSIZE(unit_data) == NUM_RTS_UNIT_TYPES);
	if (n < 0 || (size_t) n >= ARRAYSIZE(unit_data))
		return NULL;
	return &unit_data[n];
}

struct rts_order_data *rts_order_type(int n)
{
	BUILD_ASSERT(ARRAYSIZE(order_data) >= NUM_RTS_ORDER_TYPES);
	if (n < 0 || (size_t) n >= ARRAYSIZE(order_data))
		return NULL;
	return &order_data[n];
}

/* assume no more than 10 faction (really, should be no more than 2 for RTS)
 * unit_numbers is just some monotonically increasing counters used in generating
 * names for ships, eg. "TS10", for troop ship 10. They are per faction, so there
 * can be for example, multiple "TS10" ships, one per faction.
 */
static int unit_numbers[NUM_RTS_UNIT_TYPES][10];

static int unit_type_to_ship_type[NUM_RTS_UNIT_TYPES];

/* Note, this is not thread safe */
int rts_allocate_unit_number(int unit_type, int faction)
{
	unit_type = unit_type % NUM_RTS_UNIT_TYPES;
	faction = faction % 10;
	unit_numbers[unit_type][faction]++;
	return unit_numbers[unit_type][faction];
}

int rts_unit_type_to_ship_type(int unit_type)
{
	if (unit_type < 0 || (size_t) unit_type >= ARRAYSIZE(unit_type_to_ship_type))
		return -1;
	return unit_type_to_ship_type[unit_type];
}

void set_rts_unit_type_to_ship_type(int unit_type, int ship_type)
{
	if (unit_type < 0 || (size_t) unit_type >= ARRAYSIZE(unit_type_to_ship_type))
		return;
	unit_type_to_ship_type[unit_type] = ship_type;
}

int orders_valid_for_unit_type(int orders, int unit_type)
{
	if (unit_type < 0 || unit_type >= NUM_RTS_UNIT_TYPES)
		return 0;
	if (orders < 0 || (size_t) orders >= ARRAYSIZE(order_data))
		return 0;
	return orders_ok_for_unit_type[orders][unit_type];
}
