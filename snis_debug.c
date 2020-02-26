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
#include <string.h>

#include "snis_debug.h"
#include "ship_registration.h"
#include "corporations.h"

static char *label = "";

void snis_debug_dump_set_label(char *s)
{
	label = s;
}

static void dump_registry(void (*printfn)(const char *fmt, ...), struct ship_registry *registry)
{
	int i;

	printfn("DUMPING SHIP REGISTRY:");
	for (i = 0; i < registry->nentries; i++) {
		switch (registry->entry[i].type) {
		case SHIP_REG_TYPE_REGISTRATION:
		case SHIP_REG_TYPE_COMMENT:
		case SHIP_REG_TYPE_CAPTAIN:
			printfn("- %u %c %s", registry->entry[i].id, registry->entry[i].type,
				registry->entry[i].entry);
			break;
		case SHIP_REG_TYPE_BOUNTY:
			printfn("- %u %c $%.0f, %u %s", registry->entry[i].id, registry->entry[i].type,
				registry->entry[i].bounty,
				registry->entry[i].bounty_collection_site,
				registry->entry[i].entry);
			break;
		case SHIP_REG_TYPE_OWNER:
			printfn("- %u %c %s", registry->entry[i].id, registry->entry[i].type,
				corporation_get_name(registry->entry[i].owner));
			break;
		default:
			printf("- UNKNOWN REGISTRY ENTRY TYPE %c", registry->entry[i].type);
			break;
		}
	}
}

void snis_debug_dump(char *cmd, struct snis_entity go[], int nstarbase_models,
			struct docking_port_attachment_point **docking_port_info,
			int (*lookup)(uint32_t object_id), void (*printfn)(const char *fmt, ...),
			struct ship_type_entry *ship_type, int nshiptypes, struct ship_registry *registry)
{
	int i, j, rc;
	uint32_t id;
	char *t;
	struct snis_entity *o;
	char fnptraddr[32];
	char arg2[256];
	static uint32_t last_object = (uint32_t) -1;

	rc = sscanf(cmd, "%*s %s", arg2);
	if (rc == 1 && strcasecmp(arg2, "registry") == 0 && registry) {
		dump_registry(printfn, registry);
		return;
	}

	rc = sscanf(cmd, "%*s %u", &id);
	if (rc != 1) {
		if (last_object != -1) {
			id = last_object;
			printfn("%s - DUMPING LAST OBJECT ID %u", label, id);
		} else {
			printfn("%s - INVALID DUMP COMMAND", label);
			return;
		}
	} else {
		last_object = id;
	}

	i = lookup(id);
	if (i < 0) {
		printfn("%s - OBJECT ID %u NOT FOUND", label, id);
		return;
	}
	o = &go[i];
	printfn("%s DUMP - %u  %s X,Y,Z,T = %.2f, %.2f, %.2f, %d",
			label, id, o->sdata.name, o->x, o->y, o->z, o->type);
#ifdef SNIS_CLIENT_DATA
	printfn("-- NUPDATES %d", o->nupdates);
	for (i = 0; i < SNIS_ENTITY_NUPDATE_HISTORY; i++)
		printfn("---- update time %.2f", o->updatetime[i]);
#endif
	printfn("-- VX, VY, VZ = %.2f, %.2f, %.2f", o->vx, o->vy, o->vz);
	printfn("-- HEADING = %.2f", o->heading);
	printfn("-- ALIVE = %hu", o->alive);
#ifdef SNIS_SERVER_DATA
	printfn("-- TIMESTAMP = %u", o->timestamp);
#endif
	printfn("-- RESPAWN TIME = %u", o->respawn_time);
	format_function_pointer(fnptraddr, (void (*)(void)) o->move);
	printfn("-- MOVE FN %s", fnptraddr);
	printfn("-- SDATA NAME = %s", o->sdata.name);
	printfn("-- SDATA SCIENCE TEXT = %s", o->sdata.science_text ? o->sdata.science_text : "NULL");
#ifdef SNIS_CLIENT_DATA
	printfn("-- SDATA SCIENCE DATA KNOWN = %hu", o->sdata.science_data_known);
#endif
	printfn("-- SDATA SUBCLASS = %hhu", o->sdata.subclass);
	printfn("-- SDATA SHIELD STRENGTH = %hhu", o->sdata.shield_strength);
	printfn("-- SDATA SHIELD WAVELENGTH = %hhu", o->sdata.shield_wavelength);
	printfn("-- SDATA SHIELD WIDTH = %hhu", o->sdata.shield_width);
	printfn("-- SDATA SHIELD DEPTH = %hhu", o->sdata.shield_depth);
	printfn("-- SDATA FACTION = %hhu", o->sdata.faction);

	switch (o->type) {
	case OBJTYPE_SHIP1:
		t = "PLAYER SHIP";
		printfn("TYPE: %s", t);
		break;
	case OBJTYPE_SHIP2:
		t = "NPC SHIP";
		printfn("TYPE: %s, SHIPTYPE: %s (%d)", t,
			o->tsd.ship.shiptype >= 0 && o->tsd.ship.shiptype < nshiptypes ?
				ship_type[o->tsd.ship.shiptype].class : "UNKNOWN",
			o->tsd.ship.shiptype);
		printfn("DESTINATION XYZ = %.2f, %.2f, %.2f",
			o->tsd.ship.dox, o->tsd.ship.doy, o->tsd.ship.doz);
		printfn("TORPEDOES: %u", o->tsd.ship.torpedoes);
		printfn("POWER: %u", o->tsd.ship.power);
		printfn("VELOCITY: %.2f", o->tsd.ship.velocity);
		printfn("FUEL = %u", o->tsd.ship.fuel);
		printfn("SHIELD DAMAGE: %u", o->tsd.ship.damage.shield_damage);
		printfn("THREAT LEVEL = %.2f", o->tsd.ship.threat_level);
#ifdef SNIS_SERVER_DATA
		printfn("TARGETING SYSTEM = %hhu", o->tsd.ship.targeted_system);
#endif
		printfn("- AI STACK");
		for (i = 0; i < o->tsd.ship.nai_entries; i++) {
			printfn("--- %s",
				ai_mode_name[o->tsd.ship.ai[i].ai_mode]);
			switch (o->tsd.ship.ai[i].ai_mode) {
			case AI_MODE_IDLE:
				break;
			case AI_MODE_ATTACK:
				printfn("-----  VICTIM %u",
						o->tsd.ship.ai[i].u.attack.victim_id);
				break;
			case AI_MODE_TRAVEL:
				break;
			case AI_MODE_FLEE:
				printfn("----- ASSAILANT %u",
						o->tsd.ship.ai[i].u.flee.assailant);
				printfn("----- WARP COUNTDOWN %d",
						o->tsd.ship.ai[i].u.flee.warp_countdown);
				break;
			case AI_MODE_PATROL:
				for (j = 0; j < o->tsd.ship.ai[i].u.patrol.npoints; j++) {
					char c;
					if (j == o->tsd.ship.ai[i].u.patrol.dest)
						c = '>';
					else
						c = '-';
					printfn("-----%c %.2f, %.2f, %.2f\n",
						c, o->tsd.ship.ai[i].u.patrol.p[j].v.x,
						o->tsd.ship.ai[i].u.patrol.p[j].v.y,
						o->tsd.ship.ai[i].u.patrol.p[j].v.z);
				}
				break;
			case AI_MODE_FLEET_MEMBER:
				printfn("----- fleet number %d, position %d",
						o->tsd.ship.ai[i].u.fleet.fleet,
						o->tsd.ship.ai[i].u.fleet.fleet_position);
				break;
			case AI_MODE_FLEET_LEADER:
				printfn("----- fleet number %d, position %d",
						o->tsd.ship.ai[i].u.fleet.fleet,
						o->tsd.ship.ai[i].u.fleet.fleet_position);
				for (j = 0; j < o->tsd.ship.ai[i].u.fleet.patrol.npoints; j++) {
					char c;
					if (j == o->tsd.ship.ai[i].u.fleet.patrol.dest)
						c = '>';
					else
						c = '-';
					printfn("-----%c %.2f, %.2f, %.2f\n",
						c, o->tsd.ship.ai[i].u.fleet.patrol.p[j].v.x,
						o->tsd.ship.ai[i].u.fleet.patrol.p[j].v.y,
						o->tsd.ship.ai[i].u.fleet.patrol.p[j].v.z);
				}
				break;
			case AI_MODE_HANGOUT:
				printfn("----- time to go %d\n",
						o->tsd.ship.ai[i].u.hangout.time_to_go);
				break;
			case AI_MODE_COP:
				for (j = 0; j < o->tsd.ship.ai[i].u.cop.npoints; j++) {
					char c;
					if (j == o->tsd.ship.ai[i].u.cop.dest)
						c = '>';
					else
						c = '-';
					printfn("-----%c %.2f, %.2f, %.2f\n",
						c, o->tsd.ship.ai[i].u.cop.p[j].v.x,
						o->tsd.ship.ai[i].u.cop.p[j].v.y,
						o->tsd.ship.ai[i].u.cop.p[j].v.z);
				}
				break;
			case AI_MODE_MINING_BOT:
				switch (o->tsd.ship.ai[i].u.mining_bot.mode) {
				case MINING_MODE_APPROACH_ASTEROID:
					printfn("-- MODE APPROACH ASTEROID");
					break;
				case MINING_MODE_LAND_ON_ASTEROID:
					printfn("-- MODE LAND ON ASTEROID");
					break;
				case MINING_MODE_MINE:
					printfn("-- MODE MINE");
					break;
				case MINING_MODE_RETURN_TO_PARENT:
					printfn("-- MODE RETURN TO PARENT");
					break;
				case MINING_MODE_STANDBY_TO_TRANSPORT_ORE:
					printfn("-- MODE STANDBY TO TRANSPORT ORE");
					break;
				case MINING_MODE_STOW_BOT:
					printfn("-- MODE STOW BOT");
					break;
				case MINING_MODE_IDLE:
					printfn("-- MODE IDLE");
					break;
				default:
					printfn("-- MINING MODE UNKNOWN");
					break;
				}
				printfn("----- PARENT SHIP %d",
						o->tsd.ship.ai[i].u.mining_bot.parent_ship);
				printfn("----- ASTEROID %d",
						o->tsd.ship.ai[i].u.mining_bot.parent_ship);
				printfn("----- COUNTDOWN %hu",
						o->tsd.ship.ai[i].u.mining_bot.parent_ship);
				printfn(
						"----- AU %hhu, PL %hhu, GE %hhu, U %hhu, O2 %hhu, FUEL %hhu",
						o->tsd.ship.ai[i].u.mining_bot.gold,
						o->tsd.ship.ai[i].u.mining_bot.platinum,
						o->tsd.ship.ai[i].u.mining_bot.germanium,
						o->tsd.ship.ai[i].u.mining_bot.uranium,
						o->tsd.ship.ai[i].u.mining_bot.oxygen,
						o->tsd.ship.ai[i].u.mining_bot.fuel);
				printfn("----- DESTINATION: %s",
						o->tsd.ship.ai[i].u.mining_bot.object_or_waypoint ?
							"WAYPOINT" : "OBJECT");
				if (o->tsd.ship.ai[i].u.mining_bot.object_or_waypoint)
					printfn("----- WAYPOINT XYZ %.2f, %.2f, %.2f",
						o->tsd.ship.ai[i].u.mining_bot.wpx,
						o->tsd.ship.ai[i].u.mining_bot.wpy,
						o->tsd.ship.ai[i].u.mining_bot.wpz);
				printfn("----- TOWING: %s, %d",
					o->tsd.ship.ai[i].u.mining_bot.towing ? "YES" : "NO",
					o->tsd.ship.ai[i].u.mining_bot.towed_object);
				printfn("----- ORPHAN TIME: %d",
					o->tsd.ship.ai[i].u.mining_bot.orphan_time);
				if (i == 0) {
					printfn("----- SHIP ID CHIP %u",
						o->tsd.ship.tractor_beam);
				}
				break;
			case AI_MODE_TOW_SHIP:
				printfn("----- DISABLED SHIP %u",
					o->tsd.ship.ai[i].u.tow_ship.disabled_ship);
				printfn("----- STARBASE DISPATCHER %u",
					o->tsd.ship.ai[i].u.tow_ship.starbase_dispatcher);
				printfn("----- SHIP CONNECTED %d",
					o->tsd.ship.ai[i].u.tow_ship.ship_connected);
				break;
			case AI_MODE_RTS_STANDBY:
				break;
			case AI_MODE_RTS_GUARD_BASE:
				printfn("----- GUARD BASE ID %d",
					o->tsd.ship.ai[i].u.guard_base.base_id);
				break;
			case AI_MODE_RTS_ESCORT:
				break;
			case AI_MODE_RTS_ATK_NEAR_ENEMY:
				printfn("----- NEAREST ENEMY %d",
					o->tsd.ship.ai[i].u.atk_near_enemy.enemy_id);
				break;
			case AI_MODE_RTS_MOVE_TO_WAYPOINT:
				printfn("----- waypoint %d, bridge ship id %d",
						o->tsd.ship.ai[i].u.goto_waypoint.waypoint,
						o->tsd.ship.ai[i].u.goto_waypoint.bridge_ship_id);
				break;
			case AI_MODE_RTS_OCCUPY_NEAR_BASE:
				printfn("----- OCCUPY BASE ID %d",
					o->tsd.ship.ai[i].u.occupy_base.base_id);
				break;
			case AI_MODE_RTS_ATK_MAIN_BASE:
				printfn("----- ATTACK MAIN BASE ID %d",
					o->tsd.ship.ai[i].u.atk_main_base.base_id);
				break;
			case AI_MODE_RTS_RESUPPLY:
				printfn("----- UNIT TO RESUPPLY %d",
					o->tsd.ship.ai[i].u.resupply.unit_to_resupply);
				break;
			case AI_MODE_RTS_OUT_OF_FUEL:
				break;
			default:
				break;
			}
		}
		printfn("- END OF AI STACK");
		break;
	case OBJTYPE_ASTEROID:
		t = "ASTEROID";
		printfn("TYPE: %s", t);
		break;
	case OBJTYPE_STARBASE:
		t = "STARBASE";
		printfn("-- TYPE: %s", t);
		printfn("-- UNDER ATTACK: %hhu", o->tsd.starbase.under_attack);
		printfn("-- LIFEFORM COUNT: %hhu", o->tsd.starbase.lifeform_count);
		printfn("-- SECURITY: %hhu", o->tsd.starbase.security);
		printfn("-- STARBASE NUMBER: %hhu", o->tsd.starbase.starbase_number);
		printfn("-- ASSOCIATED PLANET: %d", o->tsd.starbase.associated_planet_id);
		printfn("-- NATTACKERS: %d", o->tsd.starbase.nattackers);
		for (i = 0; i < o->tsd.starbase.nattackers; i++)
			printfn("----  ATTACKER %d: %d", i,
				o->tsd.starbase.attacker[i]);
		printfn("-- NEXT LASER TIME: %u", o->tsd.starbase.next_laser_time);
		printfn("-- NEXT TORPEDO TIME: %u", o->tsd.starbase.next_torpedo_time);
		printfn("-- NEXT TORPEDO TIME: %u", o->tsd.starbase.next_torpedo_time);
		int model = o->id % nstarbase_models;
		printfn("-- MODEL: %d", model);
		for (i = 0; i < docking_port_info[model]->nports; i++)
			printfn("-- DOCKING PORT[%d] %d", i,
				o->tsd.starbase.docking_port[i]);
		for (i = 0; i < docking_port_info[model]->nports; i++) {
			printfn("-- EXPECTED DOCKER[%d] %d", i,
					o->tsd.starbase.expected_docker[i]);
			printfn("-- EXPECTED DOCKER TIMER[%d] %d", i,
					o->tsd.starbase.expected_docker_timer[i]);
		}
		printfn("-- SPIN RATE 10ths DEG / SEC: %d",
				o->tsd.starbase.spin_rate_10ths_deg_per_sec);
		for (i = 0; i < 4; i++)
			printfn("-- occupant[%d] %hhu", i,
					o->tsd.starbase.occupant[i]);
		printfn("-- TIME_LEFT_TO_BUILD %d",
				o->tsd.starbase.time_left_to_build);
		printfn("-- BUILD UNIT TYPE %hhu",
				o->tsd.starbase.build_unit_type);
		/* TODO: debug info for marketplace data, bid_price, part_price */
		break;
	case OBJTYPE_DEBRIS:
		t = "DEBRIS";
		printfn("TYPE: %s", t);
		break;
	case OBJTYPE_SPARK:
		t = "SPARK";
		printfn("TYPE: %s", t);
		break;
	case OBJTYPE_TORPEDO:
		t = "TORPEDO";
		printfn("TYPE: %s", t);
		break;
	case OBJTYPE_LASER:
		t = "LASER";
		printfn("TYPE: %s", t);
		break;
	case OBJTYPE_EXPLOSION:
		t = "EXPLOSION";
		printfn("TYPE: %s", t);
		break;
	case OBJTYPE_NEBULA:
		t = "NEBULA";
		printfn("TYPE: %s", t);
		break;
	case OBJTYPE_WORMHOLE:
		t = "WORMHOLE";
		printfn("TYPE: %s", t);
		printfn("DEST: %.2f, %.2f, %.2f",
			o->tsd.wormhole.dest_x,
			o->tsd.wormhole.dest_y,
			o->tsd.wormhole.dest_z);
		break;
	case OBJTYPE_SPACEMONSTER:
		t = "SPACEMONSTER";
		printfn("TYPE: %s", t);
		printfn("MOVEMENT COUNTDOWN: %d", o->tsd.spacemonster.movement_countdown);
		printfn("MODE: %hhu", o->tsd.spacemonster.mode);
		printfn("SEED: %u", o->tsd.spacemonster.seed);
		printfn("EMIT INTENSITY: %d", o->tsd.spacemonster.emit_intensity);
		printfn("HEAD SIZE: %d", o->tsd.spacemonster.head_size);
		printfn("TENTACLE SIZE: %d", o->tsd.spacemonster.tentacle_size);
		printfn("INTEREST: %u", o->tsd.spacemonster.interest);
		printfn("DESIRED V: %.2f, %.2f, %.2f",
			o->tsd.spacemonster.dvx,
			o->tsd.spacemonster.dvy,
			o->tsd.spacemonster.dvz);
		printfn("ANGR, HNGR, FEAR, TGHNS, HLTH %hhu, %hhu, %hhu, %hhu, %hhu",
			o->tsd.spacemonster.anger,
			o->tsd.spacemonster.hunger,
			o->tsd.spacemonster.fear,
			o->tsd.spacemonster.toughness,
			o->tsd.spacemonster.health);
		printfn("HOME: %u", o->tsd.spacemonster.home);
		printfn("ANTAGONIST: %d", o->tsd.spacemonster.current_antagonist);
		printfn("NANTAGONISTS, NFRIENDS %hhu, %hhu",
			o->tsd.spacemonster.nantagonists, o->tsd.spacemonster.nfriends);
		for (i = 0; i < o->tsd.spacemonster.nantagonists; i++)
			printfn("--- ANTAGONIST[%d] %u", i, o->tsd.spacemonster.antagonist[i]);
		for (i = 0; i < o->tsd.spacemonster.nfriends; i++)
			printfn("--- FRIEND[%d] %u", i, o->tsd.spacemonster.friend[i]);
		printfn("NEAREST ASTEROID, SPACEMONSTER, SHIP %u, %u, %u",
			o->tsd.spacemonster.nearest_asteroid,
			o->tsd.spacemonster.nearest_spacemonster,
			o->tsd.spacemonster.nearest_ship);
		printfn("DIST TO NEAREST ASTR, MNSTR, SHIP %.2f, %.2f, %.2f",
			o->tsd.spacemonster.asteroid_dist,
			o->tsd.spacemonster.spacemonster_dist,
			o->tsd.spacemonster.ship_dist);
		printfn("DEST %.2f, %.2f, %.2f",
			o->tsd.spacemonster.dest.v.x,
			o->tsd.spacemonster.dest.v.y,
			o->tsd.spacemonster.dest.v.z);
		printfn("DECISION AGE %d", o->tsd.spacemonster.decision_age);
		break;
	case OBJTYPE_PLANET:
		t = "PLANET";
		printfn("TYPE: %s", t);
		printfn("DESC SEED: %u", o->tsd.planet.description_seed);
		printfn("GOVT: %hhu", o->tsd.planet.government);
		printfn("TECH LVL: %hhu", o->tsd.planet.tech_level);
		printfn("ECONOMY: %hhu", o->tsd.planet.economy);
		printfn("SECURITY: %hhu", o->tsd.planet.security);
		printfn("RADIUS: %.2f", o->tsd.planet.radius);
		printfn("HAS ATMOSPHERE: %hhu", o->tsd.planet.has_atmosphere);
		printfn("RING SELECTOR: %hhu", o->tsd.planet.ring_selector);
		printfn("SOLARSYS PLANET TYPE: %hhu", o->tsd.planet.solarsystem_planet_type);
		printfn("RING: %hhu", o->tsd.planet.ring);
		printfn("ATMOSPHERE RGB: %hhu  %hhu  %hhu",
			o->tsd.planet.atmosphere_r,
			o->tsd.planet.atmosphere_g,
			o->tsd.planet.atmosphere_b);
		printfn("ATMOSPHERE TYPE: %hu", o->tsd.planet.atmosphere_type);
		printfn("ATMOSPHERE SCALE: %.2f", o->tsd.planet.atmosphere_scale);
		printfn("CONTRABAND: %hu", o->tsd.planet.contraband);
		printfn("ATMOSPHERE PTR: %p", (void *) o->tsd.planet.atmosphere);
		printfn("TIME LEFT TO BUILD: %u", o->tsd.planet.time_left_to_build);
		printfn("BUILD UNIT TYPE: %hhu", o->tsd.planet.build_unit_type);
		break;
	case OBJTYPE_LASERBEAM:
		t = "LASERBEAM";
		printfn("TYPE: %s", t);
		break;
	case OBJTYPE_DERELICT:
		t = "DERELICT";
		printfn("TYPE: %s", t);
		printfn("-- PERSISTENT: %hhu", o->tsd.derelict.persistent);
		printfn("-- FUEL: %hhu", o->tsd.derelict.fuel);
		printfn("-- OXYGEN: %hhu", o->tsd.derelict.oxygen);
		printfn("-- SHIPS LOG: %s",
				o->tsd.derelict.ships_log ? o->tsd.derelict.ships_log : "NULL");
		printfn("-- ORIG_SHIP_ID: %u", o->tsd.derelict.orig_ship_id);
		break;
	case OBJTYPE_TRACTORBEAM:
		t = "TRACTORBEAM";
		printfn("TYPE: %s", t);
		break;
	case OBJTYPE_CARGO_CONTAINER:
		t = "CARGO CONTAINER";
		printfn("TYPE: %s", t);
		printfn("ITEM: %d", o->tsd.cargo_container.contents.item);
		printfn("QTY: %f", o->tsd.cargo_container.contents.qty);
		printfn("PERSISTENT: %d", o->tsd.cargo_container.persistent);
		break;
	case OBJTYPE_WARP_EFFECT:
		t = "WARP EFFECT";
		printfn("TYPE: %s", t);
		printfn("WARPGATE NUMBER: %u", o->tsd.warpgate.warpgate_number);
		break;
	case OBJTYPE_SHIELD_EFFECT:
		t = "SHEILD EFFECT";
		printfn("TYPE: %s", t);
		break;
	case OBJTYPE_DOCKING_PORT:
		t = "DOCKING PORT";
		printfn("TYPE: %s", t);
		printfn("-- PARENT: %u", o->tsd.docking_port.parent);
		printfn("-- DOCKED GUY: %u", o->tsd.docking_port.docked_guy);
		printfn("-- PORT NUMBER: %hhu", o->tsd.docking_port.portnumber);
		printfn("-- MODEL: %hhu", o->tsd.docking_port.model);
		break;
	case OBJTYPE_WARPGATE:
		t = "WARP GATE";
		printfn("TYPE: %s", t);
		break;
	case OBJTYPE_BLOCK:
		t = "BLOCK";
		printfn("TYPE: %s", t);
		printfn("PARENT: %u", o->tsd.block.parent_id);
		switch (o->tsd.block.shape.type) {
		case SHAPE_SPHERE:
			printfn("SHAPE: SPHERE");
			printfn("- RADIUS: %.2f", o->tsd.block.shape.sphere.radius);
			break;
		case SHAPE_CUBOID:
			printfn("SHAPE: CUBOID");
			printfn("- SCALE: %.2f %.2f %.2f",
				o->tsd.block.shape.cuboid.sx, o->tsd.block.shape.cuboid.sy,
				o->tsd.block.shape.cuboid.sz);
			break;
		case SHAPE_CAPSULE:
			printfn("SHAPE: CAPSULE");
			printfn("- LENGTH: %.2f", o->tsd.block.shape.capsule.length);
			printfn("- RADIUS: %.2f", o->tsd.block.shape.capsule.radius);
			break;
		default:
			printfn("SHAPE: UNKNOWN (%hhu)", o->tsd.block.shape.type);
			break;
		}
		printfn("- OVERALL RADIUS: %.2f", o->tsd.block.shape.overall_radius);
		printfn("OFFSET FROM PARENT: %.2f %.2f %.2f",
			o->tsd.block.dx, o->tsd.block.dy, o->tsd.block.dz);
		printfn("ROOT ID: %u", o->tsd.block.root_id);
		printfn("MATERIAL INDEX: %hhu",
			o->tsd.block.block_material_index);
		printfn("HEALTH: %hhu", o->tsd.block.health);
		break;
	case OBJTYPE_TURRET:
		t = "TURRET";
		printfn("TYPE: %s", t);
		printfn("PARENT ID: %u", o->tsd.turret.parent_id);
		printfn("ROOT ID: %u", o->tsd.turret.root_id);
#ifdef SNIS_SERVER_DATA
		printfn("OFFSET FROM PARENT: %.2f %.2f %.2f",
			o->tsd.turret.dx, o->tsd.turret.dy, o->tsd.turret.dz);
		printfn("CURRENT TARGET: %u", o->tsd.turret.current_target_id);
		printfn("FIRE COUNTDOWN: %hhu", o->tsd.turret.fire_countdown);
		printfn("FIRE COUNTDOWN RESET: %hhu",
			o->tsd.turret.fire_countdown_reset_value);
#endif
		printfn("HEALTH: %hhu", o->tsd.turret.health);
		break;
	case OBJTYPE_WARP_CORE:
		t = "WARP CORE";
		printfn("TYPE: %s", t);
		break;
	case OBJTYPE_BLACK_HOLE:
		t = "BLACK HOLE";
		printfn("TYPE: %s", t);
		break;
	case OBJTYPE_MISSILE:
		t = "MISSILE";
		printfn("TYPE: %s", t);
		break;
	}
}

