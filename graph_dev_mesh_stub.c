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

/* This is used by snis_server, which needs to use a few meshes for some calculations,
 * but does not actually display anything, so these function do not need to do anything.
 */
#include "graph_dev_mesh_stub.h"

void mesh_graph_dev_init(__attribute__((unused)) struct mesh *m)
{
}

void mesh_graph_dev_cleanup(__attribute__((unused)) struct mesh *m)
{
}

