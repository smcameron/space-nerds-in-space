#ifndef SHIP_REGISTRATION_H__
#define SHIP_REGISTRATION_H__
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

#include <stdint.h>

struct ship_registry_entry {
	uint32_t id;
#define SHIP_REG_TYPE_REGISTRATION 'r'
#define SHIP_REG_TYPE_BOUNTY 'b'
#define SHIP_REG_TYPE_OWNER 'o'
#define SHIP_REG_TYPE_COMMENT 'c'
#define SHIP_REG_TYPE_CAPTAIN 'C'
	int owner;
	char *entry;
	float bounty;
	uint32_t bounty_collection_site;
	char type;
};

struct ship_registry {
	int nentries;
	int nallocated;
	struct ship_registry_entry *entry;
};

void ship_registry_init(struct ship_registry *r);
void ship_registry_add_entry(struct ship_registry *r, uint32_t id, char entry_type, char *entry);
void ship_registry_add_bounty(struct ship_registry *r, uint32_t id, char *entry,
				float bounty, uint32_t bounty_collection_site);
void ship_registry_add_owner(struct ship_registry *r, uint32_t id, int owner);
void ship_registry_delete_ship_entries(struct ship_registry *r, uint32_t id);
int ship_registry_get_next_entry(struct ship_registry *r, uint32_t id, int n);
void ship_registry_delete_bounty_entries_by_site(struct ship_registry *r, uint32_t collection_site);
void free_ship_registry(struct ship_registry *r);
int ship_registry_ship_has_bounty(struct ship_registry *r, uint32_t id);


#endif

