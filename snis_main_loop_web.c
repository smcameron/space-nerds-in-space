/*
	This file (snis_main_loop_web.c) is entirely machine-generated. To the extent
	possible under law, the author(s) have dedicated all copyright and related and
	neighboring rights to this software to the public domain worldwide.  This
	software is distributed without any warranty.

	You should have received a copy of the CC0 Public Domain Dedication along with
	this software. If not, see <http://creativecommons.org>.
*/

#include <emscripten.h>

#define DEFINE_SNIS_MAIN_LOOP_GLOBALS
#include "snis_main_loop.h"

static double (*main_loop_iteration)(void);

static void browser_frame(void)
{
	/* The browser decides when the next frame is, so ignore how long
	 * iteration() would like to sleep.
	 */
	if (main_loop_iteration() < 0.0)
		emscripten_cancel_main_loop();
}

void snis_run_main_loop(double (*iteration)(void))
{
	main_loop_iteration = iteration;
	emscripten_set_main_loop(browser_frame, 0, 1);
}
