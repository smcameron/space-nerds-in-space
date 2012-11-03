/*

Copyright (c) 2010 Stephen M. Cameron 

Permission is hereby granted, free of charge, to any person
obtaining a copy of this software and associated documentation
files (the "Software"), to deal in the Software without
restriction, including without limitation the rights to use,
copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the
Software is furnished to do so, subject to the following
conditions:

The above copyright notice and this permission notice shall be
included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
OTHER DEALINGS IN THE SOFTWARE.

 */
#include <string.h>
#include <stdint.h>
#include <pthread.h>

#include "ssgl.h"

#define DEFINE_SSGL_SANITIZE_GLOBALS
#include "ssgl_sanitize.h"

static void fill_trailing_zeroes(char *buf, int bufsize)
{
	int len;

	buf[bufsize - 1] = '\0';
	len = strlen(buf);
	if (bufsize - len > 0)
		memset(&buf[len], 0, bufsize - len);
}

int ssgl_sanitize_game_server_entry(struct ssgl_game_server *gs)
{
	/* If any bytes of the IP address are 255, reject.
	 * Do not want to get tricked into broadcasting.
	 */
	unsigned char *x = (unsigned char *) &gs->ipaddr;

	if (x[0] == 0xff || x[1] == 0xff ||
		x[2] == 0xff || x[3] == 0xff)
		return -1;

	/* Make sure all the strings are null terminated and that the 
	 * characters following the terminating NULL are also zeroed.
	 */

	fill_trailing_zeroes(gs->game_type, sizeof(gs->game_type));
	fill_trailing_zeroes(gs->game_instance, sizeof(gs->game_instance));
	fill_trailing_zeroes(gs->server_nickname, sizeof(gs->server_nickname));
	fill_trailing_zeroes(gs->location, sizeof(gs->location));
	
	return 0;	
}

