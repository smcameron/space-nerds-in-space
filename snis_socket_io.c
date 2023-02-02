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

#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/socket.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <stdint.h>
#include <sys/time.h>
#include <stdlib.h>

#define DEFINE_SNIS_SOCKET_IO_GLOBALS
#include "snis_socket_io.h"

#define MAX_DEBUGGABLE_SOCKETS 100
static int protocol_debugging_enabled = 1;
struct debug_buf {
		int len;
		unsigned char buf[100];
};

static struct debug_buf *dbgbuf[MAX_DEBUGGABLE_SOCKETS] = { 0 };

static struct network_stats *netstats = NULL;

/* TODO: use select() and by this method allow timeouts so dead clients/servers
 * don't hang the other end.
 */

/* Function to read from a socket, restarting if EINTR... */
int snis_readsocket(int fd, void *buffer, int buflen)
{

	char *c = buffer;
	int rc, len = buflen;

	do {
		rc = recv(fd, c, len, 0);
		/*printf("recv returned %d, errno = %s\n", rc, strerror(errno)); */
		if (rc == 0) { /* other side closed conn */
			fprintf(stderr, "pid(%d): readsocket other side closed conn\n", getpid());
			return -1;
		}
		if (rc == len) {
			if (netstats)
				netstats->bytes_recd += rc;
			if (protocol_debugging_enabled && fd < MAX_DEBUGGABLE_SOCKETS) {
				int dbglen = len;
				if (dbglen > 100)
					dbglen = 100;
				memcpy(dbgbuf[fd]->buf, buffer, dbglen);
				dbgbuf[fd]->len = dbglen;
			}
			return 0;
		}
		if (rc < 0) {
			if (errno == EINTR)
				continue;
			else
				return rc;
		}
		len -= rc;
		c += rc;
		if (netstats)
			netstats->bytes_recd += rc;
	} while (1);
}

/* Function to write to a socket, restarting if EINTR... */
int snis_writesocket(int fd, void *buffer, int buflen)
{
	char *c = buffer;
	int rc, len = buflen;

	do {
		rc = send(fd, c, len, 0);
		if (rc == len) {
			if (netstats)
				netstats->bytes_sent += rc;
			return 0;
		}
		if (rc < 0) {
			if (errno == EINTR)
				continue;
			else
				return rc;
		}
		len -= rc;
		c += rc;
		if (netstats)
			netstats->bytes_sent += rc;
	} while (1);
}

void ignore_sigpipe(void)
{
	(void) signal(SIGPIPE, SIG_IGN);
}
	
void snis_collect_netstats(struct network_stats *ns)
{
	netstats = ns;
	netstats->bytes_sent = 0;
	netstats->bytes_recd = 0;
	gettimeofday(&netstats->start, NULL);
}

void snis_protocol_debugging(int enable)
{
	int i;

	protocol_debugging_enabled = enable;

	if (enable) {
		for (i = 0; i < MAX_DEBUGGABLE_SOCKETS; i++)
			dbgbuf[i] = calloc(1, sizeof(*dbgbuf[i]));
	}
}

void snis_print_last_buffer(char *title, int socket)
{
	int i;

	if (!protocol_debugging_enabled)
		return;

	if (socket < 0)
		return;

	if (socket >= MAX_DEBUGGABLE_SOCKETS)
		return;

	fprintf(stderr, "%s: Last buffer length read from socket %d was %d\n%s: ", title, socket,
			dbgbuf[socket]->len, title);
	for (i = 0; i < dbgbuf[socket]->len; i++)
		fprintf(stderr, "%02x ", dbgbuf[socket]->buf[i]);
	fprintf(stderr, "\n");
}
