/*
	Copyright (C) 2026 Stephen M. Cameron
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
#include <assert.h>
#include <unistd.h>

#include "snis_wled.h"
#include "snis_packet.h"

int main(int argc, char *argv[])
{
	int rc;

	(void) argc;
	(void) argv;

	printf("Testing snis_wled init...\n");
	rc = snis_wled_init("127.0.0.1", 21324);
	assert(rc == 0);

	printf("Testing solid color command (red alert)...\n");
	snis_wled_handle_command(WLED_CMD_SOLID, 255, 0, 0, 0);

	printf("Testing tick loop...\n");
	for (int i = 0; i < 30; i++)
		snis_wled_tick();

	printf("Testing flash command (white flash explosion)...\n");
	snis_wled_handle_command(WLED_CMD_FLASH, 255, 255, 255, 100);

	for (int i = 0; i < 30; i++)
		snis_wled_tick();

	printf("Testing off command...\n");
	snis_wled_handle_command(WLED_CMD_OFF, 0, 0, 0, 0);

	for (int i = 0; i < 5; i++)
		snis_wled_tick();

	printf("Testing shutdown...\n");
	snis_wled_shutdown();

	printf("All snis_wled tests passed.\n");
	return 0;
}
