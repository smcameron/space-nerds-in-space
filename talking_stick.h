#ifndef TALKING_STICK_H__
#define TALKING_STICK_H__
/*
	Copyright (C) 2020 Stephen M. Cameron
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

/* This sets up "talking stick system" where there N talking sticks which can be
 * be assigned to clients to prevent more than N of them from accessing a resource
 * at once (e.g. N VOIP channels).
 */

#define NO_TALKING_STICK (-1)
void talking_stick_setup(int count); /* Setup count talking sticks */
int16_t talking_stick_get(void); /* Get a talking stick (or -1 if none are available) */
void talking_stick_release(int16_t stick); /* Release a talking stick */

#endif
