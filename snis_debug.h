#ifndef SNIS_DEBUG_H__
#define SNIS_DEBUG_H__
/*
	Copyright (C) 2018 Stephen M. Cameron
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

#include "quat.h"
#include "snis.h"
#include "string-utils.h"
#include "snis_ship_type.h"
#include "ship_registration.h"

void snis_debug_dump_set_label(char *label);
void snis_debug_dump(char *cmd, struct snis_entity go[],
			int nstarbase_models,
			struct docking_port_attachment_point **docking_port_info,
			int (*lookup)(uint32_t object_id),
			void (*printfn)(const char *fmt, ...),
			struct ship_type_entry *ship_type,
			int nshiptypes, struct ship_registry *registry);

#endif
