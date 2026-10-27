#ifndef SNIS_NET_TRANSPORT_H__
#define SNIS_NET_TRANSPORT_H__
/*
	This file (snis_net_transport.h) is entirely machine-generated. To the extent
	possible under law, the author(s) have dedicated all copyright and related and
	neighboring rights to this software to the public domain worldwide.  This
	software is distributed without any warranty.

	You should have received a copy of the CC0 Public Domain Dedication along with
	this software. If not, see <http://creativecommons.org>.
*/

/*
 * Connects/closes sockets and sends/receives the byte stream between
 * snis_client and snis_server.
 *
 * snis_net_transport.c implements this with ordinary TCP sockets.
 * Everything above this layer (snis_readsocket(), snis_writesocket(), the
 * protocol) is the same.
 */

#ifdef DEFINE_SNIS_NET_TRANSPORT_GLOBALS
#define GLOBAL
#else
#define GLOBAL extern
#endif

#define SNIS_NET_LOOKUP_FAILED (-2)
#define SNIS_NET_CONNECT_FAILED (-1)

/* Connect to host:port.  Returns a descriptor usable with the functions below,
 * or SNIS_NET_LOOKUP_FAILED or SNIS_NET_CONNECT_FAILED, in which case a
 * message is left in errmsg.
 */
GLOBAL int snis_net_connect(const char *host, const char *port, char *errmsg, int errmsglen);

/* Like recv(2) and send(2), without flags. */
GLOBAL int snis_net_recv(int fd, void *buffer, int len);
GLOBAL int snis_net_send(int fd, void *buffer, int len);

/* Shut down and close a descriptor returned by snis_net_connect() */
GLOBAL void snis_net_close(int fd);

#undef GLOBAL
#endif
