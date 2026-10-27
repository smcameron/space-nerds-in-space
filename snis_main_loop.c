/*
	This file (snis_main_loop.c) is entirely machine-generated. To the extent
	possible under law, the author(s) have dedicated all copyright and related and
	neighboring rights to this software to the public domain worldwide.  This
	software is distributed without any warranty.

	You should have received a copy of the CC0 Public Domain Dedication along with
	this software. If not, see <http://creativecommons.org>.
*/

#include "mathutils.h"

#define DEFINE_SNIS_MAIN_LOOP_GLOBALS
#include "snis_main_loop.h"

void snis_run_main_loop(double (*iteration)(void))
{
	double time_to_sleep;

	while ((time_to_sleep = iteration()) >= 0.0)
		if (time_to_sleep > 0.0)
			sleep_double(time_to_sleep);
}
