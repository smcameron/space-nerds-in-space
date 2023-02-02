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

#include <sys/types.h>
#include <sys/socket.h>
#include <errno.h>

#define DEFINE_SSGL_SOCKET_IO_GLOBALS
#include "ssgl_socket_io.h"

/* TODO: use select() and by this method allow timeouts so dead clients/servers
 * don't hang the other end.
 */

/* Function to read from a socket, restarting if EINTR... */
int ssgl_readsocket(int fd, void *buffer, int buflen)
{

	char *c = buffer;
	int rc, len = buflen;

	do {
		rc = recv(fd, c, len, 0);
		if (rc == 0) /* other side closed conn */
			return -1;
		if (rc == len)
			return 0;
		if (rc < 0) {
			if (errno == EINTR)
				continue;
			else
				return rc;
		}
		len -= rc;
		c += rc;
	} while (1);
}

/* Function to write to a socket, restarting if EINTR... */
int ssgl_writesocket(int fd, void *buffer, int buflen)
{
	char *c = buffer;
	int rc, len = buflen;

	do {
		rc = send(fd, c, len, 0);
		if (rc == len)
			return 0;
		if (rc < 0) {
			if (errno == EINTR)
				continue;
			else
				return rc;
		}
		len -= rc;
		c += rc;
	} while (1);
}

 
