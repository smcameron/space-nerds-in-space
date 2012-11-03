#ifndef __SNIS_H__
#define __SNIS_H__
/*
        Copyright (C) 2010 Stephen M. Cameron 
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

#define SNIS_PROTOCOL_VERSION "SNIS001"

#define XUNIVERSE_DIMENSION 10000.0
#define YUNIVERSE_DIMENSION 10000.0
#define MAXGAMEOBJS 5000

#define NBASES 5
#define NPLANETS 100
#define NESHIPS 50

#define MAXPLAYERS 10

#define OBJTYPE_SHIP1 0
#define OBJTYPE_SHIP2 1
#define OBJTYPE_PLANET 2 
#define OBJTYPE_STARBASE 3
#define OBJTYPE_DEBRIS 4
#define OBJTYPE_SPARK 5
#define OBJTYPE_TORPEDO 6
#define OBJTYPE_LASER 7

struct ship_data {
	uint32_t torpedoes;
	uint32_t energy;
	uint32_t shields;
	char shipname[100];
};

struct starbase_data {
	uint32_t sheilds;
};

struct laser_data {
	uint32_t power;
	uint32_t color;
};

struct torpedo_data {
	uint32_t power;
};

union type_specific_data {
	struct ship_data ship;
	struct laser_data laser;
	struct torpedo_data torpedo;
	struct starbase_data starbase;
};

struct snis_entity;
typedef void (*move_function)(struct snis_entity *o);
	
struct snis_entity {
	uint32_t index;
	uint32_t id;
	double x, y;
	double vx, vy;
	double heading;
	uint32_t alive;
	uint32_t type;
	uint32_t timestamp;
	union type_specific_data tsd;
	move_function move;
};

#endif
