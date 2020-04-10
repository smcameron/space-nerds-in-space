#ifndef SNIS_CULTURE_H__
#define SNIS_CULTURE_H__
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

/* Cribbed from Oolite */
static char *economy_name[] = {
	"Rich Industrial",
	"Average Industrial",
	"Poor Industrial",
	"Mainly Industrial",
	"Mainly Agricultural",
	"Rich Agricultural",
	"Average Agricultural",
	"Poor Agricultural",
};

static char *government_name[] = {
	"Corporate State",
	"Democracy",
	"Confederacy",
	"Communist",
	"Dictatorship",
	"Multi-Governmental",
	"Feudal",
	"Anarchy",
};

/* need to think of something better here... */
static __attribute__((unused)) char *tech_level_name[] = {
	"interstellar age",
	"space age",
	"positronic age",
	"silicon age",
	"steel age",
	"iron age",
	"stone age",
};

#endif
