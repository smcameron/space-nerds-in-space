/*
	Copyright (C) 2025 Stephen M. Cameron
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

#include "snis_process_options.h"

static const struct snis_process_options default_options = {
	.ssgl_server = {
		.port = 2914, /* host byte order */
		.primary_ip_probe_addr = 0,
	},
	.snis_multiverse = {
		.autowrangle = 1,
		.allow_remote_networks = 0,
		.lobbyhost = "localhost",
		.nickname = "nickname",
		.location = "narnia",
		.exempt = "default",
		.minport = 49152,
		.maxport = 65355,
	},
	.snis_server = {
		.allow_remote_networks = 0,
		.enable_enscript = 0,
		.lobbyhost = "localhost",
		.nickname = "nickname",
		.location = "narnia",
		.multiverse_location = "narnia",
		.solarsystem = "default",
		.minport = 49152,
		.maxport = 65355,
		.nolobby = 0,
	},
};

struct snis_process_options snis_process_options_default(void)
{
	return default_options;
}
