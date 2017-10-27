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
#ifndef SNIS_RTS_UNIT_DATA_H_
#define SNIS_RTS_UNIT_DATA_H_

#include <stdlib.h>

#define NUM_RTS_UNIT_TYPES 6
#define NUM_RTS_ORDER_TYPES 8

struct rts_unit_data {
	char *name;
	char *class; /* Name of ship model to use, matches snis_ship_type.h ->class */
	char *short_name_prefix;
	float cost_to_build;
	int time_to_build; /* in 10ths of secs */
	float toughness;
	float damage_factor;
	float fuel_capacity;
	float fuel_consumption_unit;
};

struct rts_order_data {
	char *name;
	char *short_name;
	char *help_text;
	float cost_to_order;
	int user_selectable;
};

struct rts_unit_data *rts_unit_type(int n);
struct rts_order_data *rts_order_type(int n);
int rts_allocate_unit_number(int unit_type, int faction);
int rts_unit_type_to_ship_type(int unit_type);
void set_rts_unit_type_to_ship_type(int unit_type, int ship_type);

#endif

