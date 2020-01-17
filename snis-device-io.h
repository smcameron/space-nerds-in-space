#ifndef __SNIS_DEVICE_IO_H
#define __SNIS_DEVICE_IO_H
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

/* everything you can do on the engineering screen */
#define DEVIO_OPCODE_ENG_PWR_SHIELDS		0
#define DEVIO_OPCODE_ENG_PWR_PHASERS		1
#define DEVIO_OPCODE_ENG_PWR_COMMS		2
#define DEVIO_OPCODE_ENG_PWR_SENSORS		3
#define DEVIO_OPCODE_ENG_PWR_IMPULSE		4
#define DEVIO_OPCODE_ENG_PWR_WARP		5
#define DEVIO_OPCODE_ENG_PWR_MANEUVERING	6
#define DEVIO_OPCODE_ENG_PWR_TRACTOR		7
#define DEVIO_OPCODE_ENG_COOL_SHIELDS		8
#define DEVIO_OPCODE_ENG_COOL_PHASERS		9
#define DEVIO_OPCODE_ENG_COOL_COMMS		10
#define DEVIO_OPCODE_ENG_COOL_SENSORS		11
#define DEVIO_OPCODE_ENG_COOL_IMPULSE		12
#define DEVIO_OPCODE_ENG_COOL_WARP		13
#define DEVIO_OPCODE_ENG_COOL_MANEUVERING	14
#define DEVIO_OPCODE_ENG_COOL_TRACTOR		15
#define DEVIO_OPCODE_ENG_SHIELD_LEVEL		16
#define DEVIO_OPCODE_ENG_PRESET1_BUTTON		17
#define DEVIO_OPCODE_ENG_PRESET2_BUTTON		18
#define DEVIO_OPCODE_ENG_DAMAGE_CTRL		19
#define DEVIO_OPCODE_ENG_PWR_LIFESUPPORT	20
#define DEVIO_OPCODE_ENG_COOL_LIFESUPPORT	21
#define DEVIO_OPCODE_ENG_SILENCE_ALARMS		22
#define DEVIO_OPCODE_ENG_CHAFF			23

/* everything you can do on the nav screen */
#define DEVIO_OPCODE_NAV_YAW_LEFT		100
#define DEVIO_OPCODE_NAV_YAW_RIGHT		101
#define DEVIO_OPCODE_NAV_PITCH_DOWN		102
#define DEVIO_OPCODE_NAV_PITCH_UP		103
#define DEVIO_OPCODE_NAV_ROLL_LEFT		104
#define DEVIO_OPCODE_NAV_ROLL_RIGHT		105
#define DEVIO_OPCODE_NAV_REVERSE		106
#define DEVIO_OPCODE_NAV_ZOOM			107
#define DEVIO_OPCODE_NAV_WARP_POWER		108
#define DEVIO_OPCODE_NAV_WARP_ENGAGE		109
#define DEVIO_OPCODE_NAV_THROTTLE		110
#define DEVIO_OPCODE_NAV_DOCKING_MAGNETS	111
#define DEVIO_OPCODE_NAV_STANDARD_ORBIT		112
#define DEVIO_OPCODE_NAV_STARMAP		113
#define DEVIO_OPCODE_NAV_LIGHTS			114

/* everything you can do on the weapons screen */
#define DEVIO_OPCODE_WEAPONS_YAW_LEFT		200
#define DEVIO_OPCODE_WEAPONS_YAW_RIGHT		201
#define DEVIO_OPCODE_WEAPONS_PITCH_UP		202
#define DEVIO_OPCODE_WEAPONS_PITCH_DOWN		203
#define DEVIO_OPCODE_WEAPONS_FIRE_PHASERS	204
#define DEVIO_OPCODE_WEAPONS_FIRE_TORPEDO	205
#define DEVIO_OPCODE_WEAPONS_WAVELENGTH		206
#define DEVIO_OPCODE_WEAPONS_FIRE_MISSILE	207

/* everything you can do on the damage control screen */
#define DEVIO_OPCODE_DMGCTRL_LEFT		300
#define DEVIO_OPCODE_DMGCTRL_RIGHT		301
#define DEVIO_OPCODE_DMGCTRL_FORWARD		302
#define DEVIO_OPCODE_DMGCTRL_BACKWARD		303
#define DEVIO_OPCODE_DMGCTRL_GRIPPER		304
#define DEVIO_OPCODE_DMGCTRL_AUTO		305
#define DEVIO_OPCODE_DMGCTRL_MANUAL		306
#define DEVIO_OPCODE_DMGCTRL_ENGINEERING	307
#define DEVIO_OPCODE_DMGCTRL_EJECT_WARP_CORE_BUTTON	308

/* everything you can do on the science screen */
#define DEVIO_OPCODE_SCIENCE_RANGE		400
#define DEVIO_OPCODE_SCIENCE_TRACTOR		401
#define DEVIO_OPCODE_SCIENCE_SRS		402
#define DEVIO_OPCODE_SCIENCE_LRS		403
#define DEVIO_OPCODE_SCIENCE_DETAILS		404
#define DEVIO_OPCODE_SCIENCE_ALIGN_TO_SHIP	405
#define DEVIO_OPCODE_SCIENCE_LAUNCH_MINING_BOT	406
#define DEVIO_OPCODE_SCIENCE_WAYPOINTS		407
#define DEVIO_OPCODE_SELECT_WAYPOINT		408
#define DEVIO_OPCODE_CLEAR_WAYPOINT		409
#define DEVIO_OPCODE_CURRPOS_WAYPOINT		410

/* everything you can do on the comms screen */
#define DEVIO_OPCODE_COMMS_COMMS_ONSCREEN	500
#define DEVIO_OPCODE_COMMS_NAV_ONSCREEN		501
#define DEVIO_OPCODE_COMMS_WEAP_ONSCREEN	502
#define DEVIO_OPCODE_COMMS_ENG_ONSCREEN		503
#define DEVIO_OPCODE_COMMS_DAMCON_ONSCREEN	504
#define DEVIO_OPCODE_COMMS_SCI_ONSCREEN		505
#define DEVIO_OPCODE_COMMS_MAIN_ONSCREEN	506
#define DEVIO_OPCODE_COMMS_TRANSMIT		507
#define DEVIO_OPCODE_COMMS_RED_ALERT		508
#define DEVIO_OPCODE_COMMS_MAINSCREEN_COMMS	509
#define DEVIO_OPCODE_COMMS_MAINZOOM		510
#define DEVIO_OPCODE_COMMS_HAIL_MINING_BOT	511

struct snis_device_io_connection;
int snis_device_io_setup(struct snis_device_io_connection **c);
int snis_device_io_send(struct snis_device_io_connection *c,
			unsigned short opcode, unsigned short value);
void snis_device_io_shutdown(struct snis_device_io_connection *c);

#endif
