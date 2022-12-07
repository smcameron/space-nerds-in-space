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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ship_registration.h"

void ship_registry_init(struct ship_registry *r)
{
	r->nentries = 0;
	r->nallocated = 0;
	r->entry = NULL;
}

static void ship_registry_add_entry_helper(struct ship_registry *r, uint32_t id, char entry_type,
			char *entry, float bounty, uint32_t bounty_collection_site, int owner)
{
	static const int allocation_unit = 1024;
	struct ship_registry_entry *e;
	int i;

	if (r->nentries >= r->nallocated) {
		e = realloc(r->entry, (r->nallocated + allocation_unit) * sizeof(r->entry[0]));
		if (e)
			r->entry = e;
		else
			return; /* silently fail */
		for (i = 0; i < allocation_unit; i++)
			memset(&r->entry[r->nentries + i], 0, sizeof(r->entry[0]));
		r->nallocated += allocation_unit;
	}
	r->entry[r->nentries].entry = strdup(entry);
	r->entry[r->nentries].type = entry_type;
	r->entry[r->nentries].id = id;
	r->entry[r->nentries].bounty = bounty;
	r->entry[r->nentries].bounty_collection_site = bounty_collection_site;
	r->entry[r->nentries].owner = owner;
	r->nentries++;
}

void ship_registry_add_entry(struct ship_registry *r, uint32_t id, char entry_type, char *entry)
{
	ship_registry_add_entry_helper(r, id, entry_type, entry, 0.0, (uint32_t) -1, -1);
}

void ship_registry_add_bounty(struct ship_registry *r, uint32_t id, char *entry,
				float bounty, uint32_t bounty_collection_site)
{
	ship_registry_add_entry_helper(r, id, SHIP_REG_TYPE_BOUNTY, entry, bounty, bounty_collection_site, -1);
}

void ship_registry_add_owner(struct ship_registry *r, uint32_t id, int owner)
{
	ship_registry_add_entry_helper(r, id, SHIP_REG_TYPE_OWNER, "", 0.0, (uint32_t) -1, owner);
}

void ship_registry_delete_ship_entries(struct ship_registry *r, uint32_t id)
{
	int s, d; /* dest, src */

	d = 0;
	/* Remove entries with specified id */
	for (s = 0; s < r->nentries; s++) {
		if (r->entry[s].id == id) {
			if (r->entry[s].entry) {
				free(r->entry[s].entry);
				r->entry[s].entry = NULL;
			}
			continue; /* Don't copy */
		}
		if (d == s) {
			d++;
			continue; /* Don't copy items on top of themselves */
		}
		memcpy(&r->entry[d], &r->entry[s], sizeof(r->entry[0]));
		d++;
	}
	/* Zero out any trailing entries */
	for (s = d; s < r->nentries; s++)
		/* Note, we do not need to free any r->entry[s].entry, because those
		 * pointers are still valid, but were copied to earlier positions in
		 * the array.
		 */
		memset(&r->entry[s], 0, sizeof(r->entry[0]));
	r->nentries = d;
}

void ship_registry_delete_bounty_entries_by_site(struct ship_registry *r, uint32_t collection_site)
{
	int s, d; /* dest, src */

	d = 0;
	/* Remove entries with specified id */
	for (s = 0; s < r->nentries; s++) {
		if (r->entry[s].type == SHIP_REG_TYPE_BOUNTY &&
			r->entry[s].bounty_collection_site == collection_site) {
			if (r->entry[s].entry) {
				free(r->entry[s].entry);
				r->entry[s].entry = NULL;
			}
			continue; /* Don't copy */
		}
		if (d == s) {
			d++;
			continue; /* Don't copy items on top of themselves */
		}
		memcpy(&r->entry[d], &r->entry[s], sizeof(r->entry[0]));
		d++;
	}
	/* Zero out any trailing entries */
	for (s = d; s < r->nentries; s++)
		/* Note, we do not need to free any r->entry[s].entry, because those
		 * pointers are still valid, but were copied to earlier positions in
		 * the array.
		 */
		memset(&r->entry[s], 0, sizeof(r->entry[0]));
	r->nentries = d;
}

int ship_registry_get_next_entry(struct ship_registry *r, uint32_t id, int n)
{
	int i;

	for (i = n; i < r->nentries; i++) {
		if (r->entry[i].id == id)
			return i;
	}
	return -1;
}

void free_ship_registry(struct ship_registry *r)
{
	int i;

	if (!r->entry) {
		r->nallocated = 0;
		r->nentries = 0;
		return;
	}

	for (i = 0; i < r->nentries; i++) {
		if (r->entry[i].entry) {
			free(r->entry[i].entry);
			r->entry[i].entry = NULL;
		}
	}
	free(r->entry);
	r->nallocated = 0;
	r->nentries = 0;
	r->entry = NULL;
}

int ship_registry_ship_has_bounty(struct ship_registry *r, uint32_t id)
{
	int i;

	for (i = 0; i < r->nentries; i++)
		if (r->entry[i].id == id && r->entry[i].type == SHIP_REG_TYPE_BOUNTY)
			return 1;
	return 0;
}

