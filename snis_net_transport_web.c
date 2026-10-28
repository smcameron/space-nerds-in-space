/*
	This file (snis_net_transport_web.c) is entirely machine-generated. To the extent
	possible under law, the author(s) have dedicated all copyright and related and
	neighboring rights to this software to the public domain worldwide.  This
	software is distributed without any warranty.

	You should have received a copy of the CC0 Public Domain Dedication along with
	this software. If not, see <http://creativecommons.org>.
*/

/*
 * WebSocket implementation of snis_net_transport.h, for the browser build.
 *
 * A browser cannot open a TCP connection, so the client opens a WebSocket
 * to /relay/<host>/<port> on the web server the page came from, which is
 * snis_web_gateway, and the gateway connects to host and port and passes
 * bytes both ways.  The protocol is an unframed byte stream, so the gateway
 * needs to know nothing about it, and snis_server needs no changes.
 *
 * The rest of the client reads with blocking calls from its own threads
 * (see gameserver_reader() in snis_client.c), so this file makes the
 * WebSocket look like a blocking socket:
 *
 * - All WebSockets are owned by one pthread, the "socket thread", which does
 *   nothing but sit in the browser's event loop.  Incoming messages arrive
 *   there and are appended to a per-socket buffer.
 * - snis_net_recv() waits on a condition variable until that buffer has data.
 * - snis_net_send() and the connect and close calls are run on the socket
 *   thread with emscripten_proxy_sync().
 *
 * Nothing here involves the main (rendering) thread, so a thread blocked in
 * snis_net_recv() while holding a lock the main thread wants can never
 * prevent its own data from arriving.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include <time.h>

#include <emscripten.h>
#include <emscripten/eventloop.h>
#include <emscripten/proxying.h>

#define DEFINE_SNIS_NET_TRANSPORT_GLOBALS
#include "snis_net_transport.h"

#define MAX_WEB_SOCKETS 8
#define FIRST_WEB_SOCKET_FD 1000 /* Keep well clear of real descriptors */
#define CONNECT_TIMEOUT_SECS 10

enum web_socket_state {
	WEB_SOCKET_UNUSED,
	WEB_SOCKET_CONNECTING,
	WEB_SOCKET_OPEN,
	WEB_SOCKET_CLOSED,
};

struct web_socket {
	enum web_socket_state state;
	unsigned char *data; /* received, not yet consumed by snis_net_recv() */
	int start, len, size;
};

static struct web_socket web_socket[MAX_WEB_SOCKETS];
static pthread_mutex_t web_socket_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t web_socket_cond = PTHREAD_COND_INITIALIZER;

static pthread_t socket_thread;
static pthread_once_t socket_thread_once = PTHREAD_ONCE_INIT;

/* The JavaScript side.  These run on the socket thread. */

EM_JS(void, js_web_socket_open, (int id, const char *host, const char *port), {
	var scheme = (self.location && self.location.protocol == 'https:') ? 'wss://' : 'ws://';
	var url = scheme + self.location.host + '/relay/' +
		encodeURIComponent(UTF8ToString(host)) + '/' + encodeURIComponent(UTF8ToString(port));
	var ws;

	self.snisWebSockets = self.snisWebSockets || {};
	try {
		ws = new WebSocket(url, ['binary']);
	} catch(e) {
		console.log('snis: WebSocket ' + url + ' failed: ' + e);
		_snis_web_socket_closed(id);
		return;
	}
	ws.binaryType = 'arraybuffer';
	ws.onopen = function() { _snis_web_socket_opened(id); };
	ws.onmessage = function(e) {
		var bytes = new Uint8Array(e.data);
		var p = _malloc(bytes.length);

		HEAPU8.set(bytes, p);
		_snis_web_socket_received(id, p, bytes.length);
		_free(p);
	};
	ws.onclose = function() { _snis_web_socket_closed(id); };
	ws.onerror = function() { _snis_web_socket_closed(id); };
	self.snisWebSockets[id] = ws;
});

EM_JS(int, js_web_socket_send, (int id, const void *buffer, int len), {
	var ws = self.snisWebSockets[id];

	if (!ws || ws.readyState != 1)
		return -1;
	/* slice() copies out of the shared heap, which WebSocket.send() will not accept */
	ws.send(HEAPU8.slice(buffer, buffer + len));
	return len;
});

EM_JS(void, js_web_socket_close, (int id), {
	var ws = self.snisWebSockets[id];

	if (ws) {
		ws.onclose = null;
		ws.onerror = null;
		ws.onmessage = null;
		ws.close();
		delete self.snisWebSockets[id];
	}
});

/* Called from the JavaScript above, on the socket thread */

EMSCRIPTEN_KEEPALIVE void snis_web_socket_opened(int id)
{
	pthread_mutex_lock(&web_socket_lock);
	if (web_socket[id].state == WEB_SOCKET_CONNECTING)
		web_socket[id].state = WEB_SOCKET_OPEN;
	pthread_cond_broadcast(&web_socket_cond);
	pthread_mutex_unlock(&web_socket_lock);
}

EMSCRIPTEN_KEEPALIVE void snis_web_socket_closed(int id)
{
	pthread_mutex_lock(&web_socket_lock);
	if (web_socket[id].state != WEB_SOCKET_UNUSED)
		web_socket[id].state = WEB_SOCKET_CLOSED;
	pthread_cond_broadcast(&web_socket_cond);
	pthread_mutex_unlock(&web_socket_lock);
}

EMSCRIPTEN_KEEPALIVE void snis_web_socket_received(int id, unsigned char *bytes, int count)
{
	struct web_socket *ws = &web_socket[id];

	pthread_mutex_lock(&web_socket_lock);
	if (ws->state != WEB_SOCKET_OPEN)
		goto out;
	if (ws->start > 0 && ws->len > 0)
		memmove(ws->data, ws->data + ws->start, ws->len);
	ws->start = 0;
	if (ws->len + count > ws->size) {
		int newsize = ws->size ? ws->size : 65536;
		unsigned char *newdata;

		while (newsize < ws->len + count)
			newsize *= 2;
		newdata = realloc(ws->data, newsize);
		if (!newdata) {
			fprintf(stderr, "snis_net_transport_web: out of memory\n");
			ws->state = WEB_SOCKET_CLOSED;
			goto out;
		}
		ws->data = newdata;
		ws->size = newsize;
	}
	memcpy(ws->data + ws->len, bytes, count);
	ws->len += count;
out:
	pthread_cond_broadcast(&web_socket_cond);
	pthread_mutex_unlock(&web_socket_lock);
}

/* The socket thread */

static void *socket_thread_main(__attribute__((unused)) void *arg)
{
	/* Return to the browser's event loop, but keep this thread alive to
	 * receive WebSocket events and proxied calls.
	 */
	emscripten_exit_with_live_runtime();
	return NULL;
}

static void start_socket_thread(void)
{
	int rc;

	rc = pthread_create(&socket_thread, NULL, socket_thread_main, NULL);
	if (rc)
		fprintf(stderr, "snis_net_transport_web: cannot create socket thread: %s\n",
			strerror(rc));
}

struct open_args {
	int id;
	const char *host, *port;
};

static void do_open(void *arg)
{
	struct open_args *a = arg;

	js_web_socket_open(a->id, a->host, a->port);
}

struct send_args {
	int id;
	void *buffer;
	int len;
	int rc;
};

static void do_send(void *arg)
{
	struct send_args *a = arg;

	a->rc = js_web_socket_send(a->id, a->buffer, a->len);
}

static void do_close(void *arg)
{
	js_web_socket_close((int) (intptr_t) arg);
}

static int fd_to_id(int fd)
{
	int id = fd - FIRST_WEB_SOCKET_FD;

	if (id < 0 || id >= MAX_WEB_SOCKETS)
		return -1;
	return id;
}

int snis_net_connect(const char *host, const char *port, char *errmsg, int errmsglen)
{
	struct open_args args;
	struct timespec deadline;
	int id, rc;

	pthread_once(&socket_thread_once, start_socket_thread);

	pthread_mutex_lock(&web_socket_lock);
	for (id = 0; id < MAX_WEB_SOCKETS; id++)
		if (web_socket[id].state == WEB_SOCKET_UNUSED)
			break;
	if (id >= MAX_WEB_SOCKETS) {
		pthread_mutex_unlock(&web_socket_lock);
		snprintf(errmsg, errmsglen, "Too many WebSockets open");
		return SNIS_NET_CONNECT_FAILED;
	}
	web_socket[id].state = WEB_SOCKET_CONNECTING;
	web_socket[id].start = 0;
	web_socket[id].len = 0;
	pthread_mutex_unlock(&web_socket_lock);

	args.id = id;
	args.host = host;
	args.port = port;
	emscripten_proxy_sync(emscripten_proxy_get_system_queue(), socket_thread, do_open, &args);

	clock_gettime(CLOCK_REALTIME, &deadline);
	deadline.tv_sec += CONNECT_TIMEOUT_SECS;
	pthread_mutex_lock(&web_socket_lock);
	rc = 0;
	while (web_socket[id].state == WEB_SOCKET_CONNECTING && rc != ETIMEDOUT)
		rc = pthread_cond_timedwait(&web_socket_cond, &web_socket_lock, &deadline);
	if (web_socket[id].state != WEB_SOCKET_OPEN) {
		pthread_mutex_unlock(&web_socket_lock);
		fprintf(stderr, "snis_client: WebSocket connect to %s:%s failed\n", host, port);
		snprintf(errmsg, errmsglen, "WebSocket connect to %s:%s failed", host, port);
		snis_net_close(id + FIRST_WEB_SOCKET_FD);
		return SNIS_NET_CONNECT_FAILED;
	}
	pthread_mutex_unlock(&web_socket_lock);
	return id + FIRST_WEB_SOCKET_FD;
}

int snis_net_recv(int fd, void *buffer, int len)
{
	struct web_socket *ws;
	int id = fd_to_id(fd);
	int n;

	if (id < 0) {
		errno = EBADF;
		return -1;
	}
	ws = &web_socket[id];
	pthread_mutex_lock(&web_socket_lock);
	while (ws->len == 0 && ws->state == WEB_SOCKET_OPEN)
		pthread_cond_wait(&web_socket_cond, &web_socket_lock);
	if (ws->len == 0) { /* closed */
		pthread_mutex_unlock(&web_socket_lock);
		return 0;
	}
	n = len < ws->len ? len : ws->len;
	memcpy(buffer, ws->data + ws->start, n);
	ws->start += n;
	ws->len -= n;
	pthread_mutex_unlock(&web_socket_lock);
	return n;
}

int snis_net_send(int fd, void *buffer, int len)
{
	struct send_args args;
	int id = fd_to_id(fd);

	if (id < 0) {
		errno = EBADF;
		return -1;
	}
	args.id = id;
	args.buffer = buffer;
	args.len = len;
	args.rc = -1;
	emscripten_proxy_sync(emscripten_proxy_get_system_queue(), socket_thread, do_send, &args);
	if (args.rc < 0)
		errno = ECONNRESET;
	return args.rc;
}

void snis_net_close(int fd)
{
	int id = fd_to_id(fd);

	if (id < 0)
		return;
	emscripten_proxy_sync(emscripten_proxy_get_system_queue(), socket_thread,
				do_close, (void *) (intptr_t) id);
	pthread_mutex_lock(&web_socket_lock);
	web_socket[id].state = WEB_SOCKET_UNUSED;
	web_socket[id].start = 0;
	web_socket[id].len = 0;
	pthread_cond_broadcast(&web_socket_cond);
	pthread_mutex_unlock(&web_socket_lock);
}
