#ifndef SNIS_MAIN_LOOP_H__
#define SNIS_MAIN_LOOP_H__
/*
	This file (snis_main_loop.h) is entirely machine-generated. To the extent
	possible under law, the author(s) have dedicated all copyright and related and
	neighboring rights to this software to the public domain worldwide.  This
	software is distributed without any warranty.

	You should have received a copy of the CC0 Public Domain Dedication along with
	this software. If not, see <http://creativecommons.org>.
*/

/*
 * Run a program's main loop.  iteration() does one pass of the loop and
 * returns how many seconds to wait before the next pass, or a negative
 * number to stop.
 *
 * snis_main_loop.c loops and sleeps, and returns when iteration() says stop.
 * snis_main_loop_web.c hands iteration() to the browser to call once per
 * displayed frame, because a browser tab must not block, and never returns.
 */

#ifdef DEFINE_SNIS_MAIN_LOOP_GLOBALS
#define GLOBAL
#else
#define GLOBAL extern
#endif

GLOBAL void snis_run_main_loop(double (*iteration)(void));

#undef GLOBAL
#endif
