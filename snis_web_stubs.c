
/*
	This file (snis_web_stubs.c) is entirely machine-generated. To the extent
	possible under law, the author(s) have dedicated all copyright and related and
	neighboring rights to this software to the public domain worldwide.  This
	software is distributed without any warranty.

	You should have received a copy of the CC0 Public Domain Dedication along with
	this software. If not, see <http://creativecommons.org>.
*/

/*
 * Things the browser build of snis_client cannot do, replaced with versions
 * that do nothing.  Each of these is linked instead of the real one only when
 * building with "make WEB=1".
 */

#include <stdint.h>

#include "ssgl/ssgl.h"

/* A browser cannot listen for UDP broadcasts, so there is no discovery of
 * lobbies on the local network.  See ssgl/ssgl_register_for_bcast_packet.c
 */
int ssgl_register_for_bcast_packets(__attribute__((unused))
		void (*lobby_change_notify)(struct ssgl_lobby_descriptor *, int))
{
	return 0;
}
