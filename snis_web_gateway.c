/*
	This file (snis_web_gateway.c) is entirely machine-generated. To the extent
	possible under law, the author(s) have dedicated all copyright and related and
	neighboring rights to this software to the public domain worldwide.  This
	software is distributed without any warranty.

	You should have received a copy of the CC0 Public Domain Dedication along with
	this software. If not, see <http://creativecommons.org>.
*/

/*
 * snis_web_gateway: everything a web browser needs to play, on one port.
 *
 * - It serves the browser client's files (bin/web, from "make WEB=1 web"),
 *   with the two headers browsers require before they allow threads.
 * - It relays WebSockets to TCP, because a browser cannot open a TCP
 *   connection.  The client asks for /relay/<host>/<port>, and the gateway
 *   connects to that host and port and passes bytes both ways.  It relays
 *   only to addresses it has been told about: those given with --allow, the
 *   lobby given with --lobby, and every game server that lobby lists.
 * - It speaks both http and https on the same port, telling them apart by
 *   the first byte the browser sends.  Browsers allow threads only in a
 *   "secure context", which http://localhost is but http://192.168.1.10 is
 *   not, so players on other machines must use https.  Unless told to use a
 *   certificate of your own (--cert, --key), the gateway makes a self-signed
 *   one the first time it runs, and each player accepts the browser's
 *   warning about it once.
 *
 * Nothing in snis_server, snis_multiverse or ssgl_server knows about any of
 * this: to them a browser is a client like any other.
 */

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <errno.h>
#include <getopt.h>
#include <limits.h>
#include <signal.h>
#include <stdint.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <pthread.h>
#include <ifaddrs.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>

#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/x509v3.h>
#include <openssl/pem.h>
#include <openssl/rand.h>

#include "ssgl/ssgl.h"

#define P "snis_web_gateway"
#define DEFAULT_PORT 8088
#define MAX_HEADER 16384
#define MAX_ALLOWED 256
#define LOBBY_POLL_SECS 5
#define WEBSOCKET_GUID "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"

/* Command line options */
static int listen_port = DEFAULT_PORT;
static char *document_root = "bin/web";
static char *lobby_host = NULL;
static char *cert_file = NULL;
static char *key_file = NULL;
static int tls_disabled = 0;
static int verbose = 0;

static SSL_CTX *ssl_ctx = NULL;

/* The addresses the relay may connect to.  Those from --allow and --lobby
 * are fixed; those the lobby lists are replaced each time it is polled.
 */
struct allowed_address {
	struct in_addr addr;
	uint16_t port; /* host byte order */
	int from_lobby;
};

static struct allowed_address allowed[MAX_ALLOWED];
static int nallowed = 0;
static pthread_mutex_t allowed_lock = PTHREAD_MUTEX_INITIALIZER;

/* A browser's connection: a socket, with TLS on it or not */
struct conn {
	int fd;
	SSL *ssl;
	char peer[INET_ADDRSTRLEN + 8];
};

static int conn_read(struct conn *c, void *buf, int len)
{
	if (c->ssl)
		return SSL_read(c->ssl, buf, len);
	return recv(c->fd, buf, len, 0);
}

/* Read exactly len bytes.  Returns 0, or -1 if the connection ends first. */
static int conn_read_all(struct conn *c, void *buf, int len)
{
	unsigned char *p = buf;
	int n;

	while (len > 0) {
		n = conn_read(c, p, len);
		if (n <= 0)
			return -1;
		p += n;
		len -= n;
	}
	return 0;
}

static int conn_write_all(struct conn *c, const void *buf, int len)
{
	const unsigned char *p = buf;
	int n;

	while (len > 0) {
		if (c->ssl)
			n = SSL_write(c->ssl, p, len);
		else
			n = send(c->fd, p, len, MSG_NOSIGNAL);
		if (n <= 0)
			return -1;
		p += n;
		len -= n;
	}
	return 0;
}

static int conn_printf(struct conn *c, const char *fmt, ...)
{
	char buf[2048];
	va_list ap;
	int n;

	va_start(ap, fmt);
	n = vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);
	if (n < 0 || n >= (int) sizeof(buf))
		return -1;
	return conn_write_all(c, buf, n);
}

static void conn_close(struct conn *c)
{
	if (c->ssl) {
		SSL_shutdown(c->ssl);
		SSL_free(c->ssl);
	}
	close(c->fd);
	free(c);
}

/* The allowlist */

/* Add an address to the allowlist.  The caller holds allowed_lock. */
static void allow_address_locked(struct in_addr addr, uint16_t port, int from_lobby)
{
	int i;

	for (i = 0; i < nallowed; i++)
		if (allowed[i].addr.s_addr == addr.s_addr && allowed[i].port == port)
			return;
	if (nallowed >= MAX_ALLOWED) {
		fprintf(stderr, "%s: too many allowed addresses\n", P);
		return;
	}
	allowed[nallowed].addr = addr;
	allowed[nallowed].port = port;
	allowed[nallowed].from_lobby = from_lobby;
	nallowed++;
}

static void allow_address(struct in_addr addr, uint16_t port, int from_lobby)
{
	char a[INET_ADDRSTRLEN];

	pthread_mutex_lock(&allowed_lock);
	allow_address_locked(addr, port, from_lobby);
	pthread_mutex_unlock(&allowed_lock);
	inet_ntop(AF_INET, &addr, a, sizeof(a));
	fprintf(stderr, "%s: relaying to %s:%hu allowed\n", P, a, port);
}

/* Replace the addresses the lobby listed last time with those it lists now,
 * in one step, so that a server listed both times is never refused.
 */
static void replace_lobby_addresses(struct ssgl_game_server *game_server, int count)
{
	struct in_addr addr;
	int i, j, before, after = 0;

	pthread_mutex_lock(&allowed_lock);
	for (i = 0, j = 0; i < nallowed; i++)
		if (!allowed[i].from_lobby)
			allowed[j++] = allowed[i];
	before = nallowed - j;
	nallowed = j;
	for (i = 0; i < count; i++) {
		addr.s_addr = game_server[i].ipaddr;
		allow_address_locked(addr, ntohs(game_server[i].port), 1);
		addr.s_addr = game_server[i].real_ipaddr;
		allow_address_locked(addr, ntohs(game_server[i].port), 1);
	}
	for (i = 0; i < nallowed; i++)
		after += allowed[i].from_lobby;
	pthread_mutex_unlock(&allowed_lock);
	if (before != after)
		fprintf(stderr, "%s: the lobby lists %d game server%s\n", P, count, count == 1 ? "" : "s");
}

static int address_is_allowed(struct in_addr addr, uint16_t port)
{
	int i, answer = 0;

	pthread_mutex_lock(&allowed_lock);
	for (i = 0; i < nallowed; i++)
		if (allowed[i].addr.s_addr == addr.s_addr && allowed[i].port == port) {
			answer = 1;
			break;
		}
	pthread_mutex_unlock(&allowed_lock);
	return answer;
}

/* Look up host's IPv4 addresses.  Returns how many were stored in addr[]. */
static int lookup_host(const char *host, struct in_addr *addr, int max)
{
	struct addrinfo hints, *info, *i;
	int n = 0;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	if (getaddrinfo(host, NULL, &hints, &info) != 0)
		return 0;
	for (i = info; i && n < max; i = i->ai_next)
		addr[n++] = ((struct sockaddr_in *) i->ai_addr)->sin_addr;
	freeaddrinfo(info);
	return n;
}

/* Parse "host:port" and allow each of host's addresses */
static int allow_host_port(const char *arg, int default_port)
{
	char host[256];
	struct in_addr addr[16];
	const char *colon;
	int port, i, n;

	colon = strrchr(arg, ':');
	if (colon) {
		snprintf(host, sizeof(host), "%.*s", (int) (colon - arg), arg);
		port = atoi(colon + 1);
	} else {
		snprintf(host, sizeof(host), "%s", arg);
		port = default_port;
	}
	if (port <= 0 || port > 65535) {
		fprintf(stderr, "%s: bad port in '%s'\n", P, arg);
		return -1;
	}
	n = lookup_host(host, addr, 16);
	if (n == 0) {
		fprintf(stderr, "%s: cannot look up '%s'\n", P, host);
		return -1;
	}
	for (i = 0; i < n; i++)
		allow_address(addr[i], port, 0);
	return 0;
}

/* Poll the lobby for its list of game servers, and allow relaying to each */
static void *lobby_thread(__attribute__((unused)) void *arg)
{
	struct ssgl_client_filter filter;
	struct ssgl_game_server *game_server;
	int sock, count, rc;

	memset(&filter, 0, sizeof(filter));
	snprintf(filter.game_type, sizeof(filter.game_type), "SNIS");
	for (;;) {
		sock = ssgl_gameclient_connect_to_lobby(lobby_host);
		if (sock < 0) {
			fprintf(stderr, "%s: cannot connect to lobby %s, will retry\n", P, lobby_host);
			ssgl_sleep(LOBBY_POLL_SECS);
			continue;
		}
		do {
			game_server = NULL;
			count = 0;
			rc = ssgl_recv_game_servers(sock, &game_server, &count, &filter);
			if (rc)
				break;
			replace_lobby_addresses(game_server, count);
			free(game_server);
			ssgl_sleep(LOBBY_POLL_SECS);
		} while (1);
		close(sock);
		fprintf(stderr, "%s: lost the lobby, reconnecting\n", P);
		ssgl_sleep(LOBBY_POLL_SECS);
	}
	return NULL;
}

/* TLS */

/* Make a self-signed certificate naming this machine: "localhost", its host
 * name, 127.0.0.1 and each of its IPv4 addresses.
 */
static int make_self_signed_certificate(const char *certpath, const char *keypath)
{
	char san[2048], hostname[256], a[INET_ADDRSTRLEN];
	struct ifaddrs *ifaddr, *ifa;
	EVP_PKEY *pkey = NULL;
	X509 *x509 = NULL;
	X509_EXTENSION *ext;
	X509V3_CTX v3ctx;
	X509_NAME *name;
	unsigned char serial[16];
	BIGNUM *bn;
	FILE *f;
	int rc = -1, n;

	n = snprintf(san, sizeof(san), "DNS:localhost,IP:127.0.0.1");
	if (gethostname(hostname, sizeof(hostname)) == 0) {
		hostname[sizeof(hostname) - 1] = '\0';
		n += snprintf(san + n, sizeof(san) - n, ",DNS:%s", hostname);
	}
	if (getifaddrs(&ifaddr) == 0) {
		for (ifa = ifaddr; ifa; ifa = ifa->ifa_next) {
			if (!ifa->ifa_addr || ifa->ifa_addr->sa_family != AF_INET)
				continue;
			inet_ntop(AF_INET, &((struct sockaddr_in *) ifa->ifa_addr)->sin_addr,
					a, sizeof(a));
			if (strcmp(a, "127.0.0.1") == 0)
				continue;
			if (n < (int) sizeof(san) - 32)
				n += snprintf(san + n, sizeof(san) - n, ",IP:%s", a);
		}
		freeifaddrs(ifaddr);
	}

	pkey = EVP_EC_gen("P-256");
	x509 = X509_new();
	if (!pkey || !x509)
		goto out;
	X509_set_version(x509, 2);
	RAND_bytes(serial, sizeof(serial));
	serial[0] &= 0x7f;
	bn = BN_bin2bn(serial, sizeof(serial), NULL);
	BN_to_ASN1_INTEGER(bn, X509_get_serialNumber(x509));
	BN_free(bn);
	X509_gmtime_adj(X509_getm_notBefore(x509), -24 * 3600);
	X509_gmtime_adj(X509_getm_notAfter(x509), 10L * 365 * 24 * 3600);
	X509_set_pubkey(x509, pkey);
	name = X509_get_subject_name(x509);
	X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_ASC,
			(unsigned char *) "Space Nerds In Space web gateway", -1, -1, 0);
	X509_set_issuer_name(x509, name);
	X509V3_set_ctx(&v3ctx, x509, x509, NULL, NULL, 0);
	ext = X509V3_EXT_conf_nid(NULL, &v3ctx, NID_subject_alt_name, san);
	if (!ext)
		goto out;
	X509_add_ext(x509, ext, -1);
	X509_EXTENSION_free(ext);
	ext = X509V3_EXT_conf_nid(NULL, &v3ctx, NID_basic_constraints, "critical,CA:FALSE");
	if (ext) {
		X509_add_ext(x509, ext, -1);
		X509_EXTENSION_free(ext);
	}
	if (!X509_sign(x509, pkey, EVP_sha256()))
		goto out;

	f = fopen(keypath, "w");
	if (!f)
		goto out;
	fchmod(fileno(f), 0600);
	PEM_write_PrivateKey(f, pkey, NULL, NULL, 0, NULL, NULL);
	fclose(f);
	f = fopen(certpath, "w");
	if (!f)
		goto out;
	PEM_write_X509(f, x509);
	fclose(f);
	fprintf(stderr, "%s: made a self-signed certificate for %s\n", P, san);
	rc = 0;
out:
	if (rc)
		ERR_print_errors_fp(stderr);
	X509_free(x509);
	EVP_PKEY_free(pkey);
	return rc;
}

/* Where to keep the certificate we make: beside the downloaded assets */
static void default_certificate_paths(char *certpath, char *keypath, int len)
{
	char dir[PATH_MAX];
	char *home = getenv("HOME");
	char *xdg = getenv("XDG_DATA_HOME");

	if (xdg)
		snprintf(dir, sizeof(dir), "%s/space-nerds-in-space", xdg);
	else
		snprintf(dir, sizeof(dir), "%s/.local/share/space-nerds-in-space", home ? home : ".");
	mkdir(dir, 0755);
	snprintf(certpath, len, "%s/web-gateway-cert.pem", dir);
	snprintf(keypath, len, "%s/web-gateway-key.pem", dir);
}

static int set_up_tls(void)
{
	static char certpath[PATH_MAX], keypath[PATH_MAX];
	struct stat statbuf;

	if (!cert_file) {
		default_certificate_paths(certpath, keypath, PATH_MAX);
		if (stat(certpath, &statbuf) != 0 || stat(keypath, &statbuf) != 0)
			if (make_self_signed_certificate(certpath, keypath) != 0)
				return -1;
		cert_file = certpath;
		key_file = keypath;
	}
	ssl_ctx = SSL_CTX_new(TLS_server_method());
	if (!ssl_ctx)
		goto fail;
	SSL_CTX_set_min_proto_version(ssl_ctx, TLS1_2_VERSION);
	if (SSL_CTX_use_certificate_chain_file(ssl_ctx, cert_file) != 1)
		goto fail;
	if (SSL_CTX_use_PrivateKey_file(ssl_ctx, key_file, SSL_FILETYPE_PEM) != 1)
		goto fail;
	fprintf(stderr, "%s: https certificate %s\n", P, cert_file);
	return 0;
fail:
	ERR_print_errors_fp(stderr);
	return -1;
}

/* HTTP */

struct request {
	char method[16];
	char path[1024];
	char upgrade[32];
	char websocket_key[64];
	char websocket_protocol[128];
	char if_modified_since[64];
};

/* Read the request head, up to the blank line, and pick out what we use */
static int read_request(struct conn *c, struct request *r)
{
	char buf[MAX_HEADER + 1];
	char *line, *next, *colon, *value;
	int len = 0, n;

	memset(r, 0, sizeof(*r));
	while (len < MAX_HEADER) {
		n = conn_read(c, buf + len, 1);
		if (n <= 0)
			return -1;
		len++;
		if (len >= 4 && memcmp(buf + len - 4, "\r\n\r\n", 4) == 0)
			break;
	}
	if (len >= MAX_HEADER)
		return -1;
	buf[len] = '\0';

	if (sscanf(buf, "%15s %1023s", r->method, r->path) != 2)
		return -1;
	line = strstr(buf, "\r\n");
	for (line = line ? line + 2 : NULL; line && *line; line = next) {
		next = strstr(line, "\r\n");
		if (!next)
			break;
		*next = '\0';
		next += 2;
		colon = strchr(line, ':');
		if (!colon)
			continue;
		*colon = '\0';
		for (value = colon + 1; *value == ' ' || *value == '\t'; value++)
			;
		if (strcasecmp(line, "Upgrade") == 0)
			snprintf(r->upgrade, sizeof(r->upgrade), "%s", value);
		else if (strcasecmp(line, "Sec-WebSocket-Key") == 0)
			snprintf(r->websocket_key, sizeof(r->websocket_key), "%s", value);
		else if (strcasecmp(line, "Sec-WebSocket-Protocol") == 0)
			snprintf(r->websocket_protocol, sizeof(r->websocket_protocol), "%s", value);
		else if (strcasecmp(line, "If-Modified-Since") == 0)
			snprintf(r->if_modified_since, sizeof(r->if_modified_since), "%s", value);
	}
	return 0;
}

static void send_error(struct conn *c, struct request *r, int code, const char *reason)
{
	conn_printf(c, "HTTP/1.1 %d %s\r\n"
			"Content-Type: text/plain\r\n"
			"Content-Length: %d\r\n"
			"Connection: close\r\n\r\n%s\n",
			code, reason, (int) strlen(reason) + 1, reason);
	if (verbose)
		fprintf(stderr, "%s: %s: %s %s: %d %s\n", P, c->peer, r->method, r->path, code, reason);
}

static const char *content_type(const char *path)
{
	static const struct {
		const char *ext, *type;
	} types[] = {
		{ ".html", "text/html; charset=utf-8" },
		{ ".js", "text/javascript" },
		{ ".wasm", "application/wasm" },
		{ ".png", "image/png" },
		{ ".ogg", "audio/ogg" },
		{ ".txt", "text/plain; charset=utf-8" },
		{ ".files", "text/plain; charset=utf-8" },
		{ ".json", "application/json" },
		{ ".ico", "image/x-icon" },
	};
	const char *dot = strrchr(path, '.');
	size_t i;

	if (dot)
		for (i = 0; i < sizeof(types) / sizeof(types[0]); i++)
			if (strcmp(dot, types[i].ext) == 0)
				return types[i].type;
	return "application/octet-stream";
}

/* Only plain relative paths, so nothing outside the document root is served */
static int path_is_safe(const char *path)
{
	const char *c;

	if (path[0] != '/' || strstr(path, ".."))
		return 0;
	for (c = path; *c; c++)
		if (!((*c >= 'a' && *c <= 'z') || (*c >= 'A' && *c <= 'Z') ||
			(*c >= '0' && *c <= '9') || *c == '/' || *c == '.' ||
			*c == '-' || *c == '_'))
			return 0;
	return 1;
}

static void serve_file(struct conn *c, struct request *r)
{
	char path[PATH_MAX], date[64], buf[65536];
	struct stat statbuf;
	struct tm tm;
	char *query;
	int fd, n, head;

	query = strchr(r->path, '?');
	if (query)
		*query = '\0';
	if (strcmp(r->path, "/") == 0)
		snprintf(r->path, sizeof(r->path), "/snis_client.html");
	if (!path_is_safe(r->path)) {
		send_error(c, r, 404, "Not Found");
		return;
	}
	snprintf(path, sizeof(path), "%s%s", document_root, r->path);
	fd = open(path, O_RDONLY);
	if (fd < 0 || fstat(fd, &statbuf) != 0 || !S_ISREG(statbuf.st_mode)) {
		if (fd >= 0)
			close(fd);
		send_error(c, r, 404, "Not Found");
		return;
	}
	gmtime_r(&statbuf.st_mtime, &tm);
	strftime(date, sizeof(date), "%a, %d %b %Y %H:%M:%S GMT", &tm);

	/* Browsers check with us before reusing a cached file, and we say
	 * "not modified" if it isn't, so a big asset file is fetched once.
	 */
	if (strcmp(r->if_modified_since, date) == 0) {
		conn_printf(c, "HTTP/1.1 304 Not Modified\r\n"
				"Cross-Origin-Opener-Policy: same-origin\r\n"
				"Cross-Origin-Embedder-Policy: require-corp\r\n"
				"Connection: close\r\n\r\n");
		close(fd);
		return;
	}
	conn_printf(c, "HTTP/1.1 200 OK\r\n"
			"Content-Type: %s\r\n"
			"Content-Length: %lld\r\n"
			"Last-Modified: %s\r\n"
			"Cache-Control: no-cache\r\n"
			"Cross-Origin-Opener-Policy: same-origin\r\n"
			"Cross-Origin-Embedder-Policy: require-corp\r\n"
			"Connection: close\r\n\r\n",
			content_type(path), (long long) statbuf.st_size, date);
	head = strcmp(r->method, "HEAD") == 0;
	while (!head && (n = read(fd, buf, sizeof(buf))) > 0)
		if (conn_write_all(c, buf, n) != 0)
			break;
	close(fd);
	if (verbose)
		fprintf(stderr, "%s: %s: %s %s\n", P, c->peer, r->method, r->path);
}

/* WebSockets (RFC 6455) */

static int websocket_send_frame(struct conn *c, int opcode, const void *data, int len)
{
	unsigned char header[10];
	int hlen;

	header[0] = 0x80 | opcode; /* FIN, never fragmented */
	if (len < 126) {
		header[1] = len;
		hlen = 2;
	} else if (len < 65536) {
		header[1] = 126;
		header[2] = (len >> 8) & 0xff;
		header[3] = len & 0xff;
		hlen = 4;
	} else {
		header[1] = 127;
		memset(header + 2, 0, 4);
		header[6] = (len >> 24) & 0xff;
		header[7] = (len >> 16) & 0xff;
		header[8] = (len >> 8) & 0xff;
		header[9] = len & 0xff;
		hlen = 10;
	}
	if (conn_write_all(c, header, hlen) != 0)
		return -1;
	return conn_write_all(c, data, len);
}

/* Read one frame from the browser, and act on it.  Data goes to the server's
 * socket.  Returns 0 to carry on, or -1 when the connection is over.
 */
static int websocket_relay_frame(struct conn *c, int server_fd)
{
	unsigned char header[2], ext[8], mask[4];
	static const int max_payload = 1 << 24;
	unsigned char *payload;
	uint64_t len;
	int opcode, i, rc = 0;

	if (conn_read_all(c, header, 2) != 0)
		return -1;
	opcode = header[0] & 0x0f;
	len = header[1] & 0x7f;
	if (!(header[1] & 0x80)) /* frames from a browser are always masked */
		return -1;
	if (len == 126) {
		if (conn_read_all(c, ext, 2) != 0)
			return -1;
		len = ((uint64_t) ext[0] << 8) | ext[1];
	} else if (len == 127) {
		if (conn_read_all(c, ext, 8) != 0)
			return -1;
		for (len = 0, i = 0; i < 8; i++)
			len = (len << 8) | ext[i];
	}
	if (len > (uint64_t) max_payload)
		return -1;
	if (conn_read_all(c, mask, 4) != 0)
		return -1;
	payload = malloc(len + 1);
	if (!payload)
		return -1;
	if (conn_read_all(c, payload, len) != 0) {
		free(payload);
		return -1;
	}
	for (i = 0; i < (int) len; i++)
		payload[i] ^= mask[i & 3];

	switch (opcode) {
	case 0x0: /* continuation */
	case 0x1: /* text */
	case 0x2: /* binary */
		if (len > 0 && send(server_fd, payload, len, MSG_NOSIGNAL) != (ssize_t) len)
			rc = -1;
		break;
	case 0x8: /* close */
		websocket_send_frame(c, 0x8, payload, len >= 2 ? 2 : 0);
		rc = -1;
		break;
	case 0x9: /* ping */
		rc = websocket_send_frame(c, 0xA, payload, len);
		break;
	default: /* pong, or anything else, is ignored */
		break;
	}
	free(payload);
	return rc;
}

static void websocket_accept_key(const char *key, char *accept, int len)
{
	char buf[128];
	unsigned char digest[EVP_MAX_MD_SIZE];
	unsigned int dlen = 0;
	unsigned char b64[64];

	snprintf(buf, sizeof(buf), "%s%s", key, WEBSOCKET_GUID);
	EVP_Digest(buf, strlen(buf), digest, &dlen, EVP_sha1(), NULL);
	EVP_EncodeBlock(b64, digest, dlen);
	snprintf(accept, len, "%s", b64);
}

/* Connect to the relay target named by a "/relay/<host>/<port>" path, if allowed */
static int connect_to_target(struct conn *c, const char *path)
{
	char host[256];
	struct in_addr addr[16];
	struct sockaddr_in sa;
	int port, n, i, fd, flag = 1;

	if (sscanf(path, "/relay/%255[^/]/%d", host, &port) != 2 || port <= 0 || port > 65535)
		return -1;
	n = lookup_host(host, addr, 16);
	for (i = 0; i < n; i++)
		if (address_is_allowed(addr[i], port))
			break;
	if (i >= n) {
		fprintf(stderr, "%s: %s: relaying to %s:%d is not allowed\n", P, c->peer, host, port);
		return -1;
	}
	fd = socket(AF_INET, SOCK_STREAM, 0);
	if (fd < 0)
		return -1;
	memset(&sa, 0, sizeof(sa));
	sa.sin_family = AF_INET;
	sa.sin_addr = addr[i];
	sa.sin_port = htons(port);
	if (connect(fd, (struct sockaddr *) &sa, sizeof(sa)) != 0) {
		fprintf(stderr, "%s: %s: cannot connect to %s:%d: %s\n",
			P, c->peer, host, port, strerror(errno));
		close(fd);
		return -1;
	}
	setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag));
	fprintf(stderr, "%s: %s: relaying to %s:%d\n", P, c->peer, host, port);
	return fd;
}

static void relay_websocket(struct conn *c, struct request *r)
{
	char accept[64], protocol[160] = "";
	unsigned char buf[65536];
	struct pollfd pfd[2];
	int server_fd, n;

	if (r->websocket_key[0] == '\0' || strncmp(r->path, "/relay/", 7) != 0) {
		send_error(c, r, 400, "Bad Request");
		return;
	}
	server_fd = connect_to_target(c, r->path);
	if (server_fd < 0) {
		send_error(c, r, 403, "Forbidden");
		return;
	}
	websocket_accept_key(r->websocket_key, accept, sizeof(accept));
	/* snis_net_transport_web.c asks for the "binary" subprotocol */
	if (strstr(r->websocket_protocol, "binary"))
		snprintf(protocol, sizeof(protocol), "Sec-WebSocket-Protocol: binary\r\n");
	if (conn_printf(c, "HTTP/1.1 101 Switching Protocols\r\n"
			"Upgrade: websocket\r\n"
			"Connection: Upgrade\r\n"
			"Sec-WebSocket-Accept: %s\r\n%s\r\n", accept, protocol) != 0)
		goto out;

	pfd[0].fd = c->fd;
	pfd[0].events = POLLIN;
	pfd[1].fd = server_fd;
	pfd[1].events = POLLIN;
	for (;;) {
		/* TLS may already hold a decrypted frame that poll() can't see */
		if (!(c->ssl && SSL_pending(c->ssl) > 0)) {
			if (poll(pfd, 2, -1) < 0) {
				if (errno == EINTR)
					continue;
				break;
			}
			if (pfd[1].revents & (POLLIN | POLLHUP | POLLERR)) {
				n = recv(server_fd, buf, sizeof(buf), 0);
				if (n <= 0)
					break;
				if (websocket_send_frame(c, 0x2, buf, n) != 0)
					break;
			}
			if (!(pfd[0].revents & (POLLIN | POLLHUP | POLLERR)))
				continue;
		}
		if (websocket_relay_frame(c, server_fd) != 0)
			break;
	}
	websocket_send_frame(c, 0x8, "\x03\xe8", 2); /* 1000: normal closure */
out:
	close(server_fd);
	fprintf(stderr, "%s: %s: relay closed\n", P, c->peer);
}

/* One browser connection */
static void *connection_thread(void *arg)
{
	struct conn *c = arg;
	struct request r;
	unsigned char first;

	/* A TLS connection starts with a handshake record, type 0x16 */
	if (recv(c->fd, &first, 1, MSG_PEEK) != 1)
		goto out;
	if (first == 0x16) {
		if (!ssl_ctx)
			goto out;
		c->ssl = SSL_new(ssl_ctx);
		if (!c->ssl)
			goto out;
		SSL_set_fd(c->ssl, c->fd);
		if (SSL_accept(c->ssl) != 1) {
			if (verbose)
				fprintf(stderr, "%s: %s: TLS handshake failed\n", P, c->peer);
			SSL_free(c->ssl);
			c->ssl = NULL;
			goto out;
		}
	}
	if (read_request(c, &r) != 0)
		goto out;
	if (strcasecmp(r.upgrade, "websocket") == 0)
		relay_websocket(c, &r);
	else if (strcmp(r.method, "GET") == 0 || strcmp(r.method, "HEAD") == 0)
		serve_file(c, &r);
	else
		send_error(c, &r, 405, "Method Not Allowed");
out:
	conn_close(c);
	return NULL;
}

static void usage(void)
{
	fprintf(stderr,
		"usage: %s [options]\n"
		"  --port N          port to listen on for http and https (default %d)\n"
		"  --root DIR        directory to serve (default bin/web)\n"
		"  --allow HOST:PORT let browsers connect to this snis_server (may be repeated)\n"
		"  --lobby HOST      let browsers connect to this lobby and every server it lists\n"
		"  --cert FILE --key FILE  use this certificate instead of a self-signed one\n"
		"  --no-tls          http only (enough for a browser on this machine)\n"
		"  --verbose\n"
		"\nExample, for a snis_server started with --nolobby --port 2501:\n"
		"  %s --allow localhost:2501\n"
		"then open http://localhost:%d/ here, or https://<this machine>:%d/ elsewhere.\n",
		P, DEFAULT_PORT, P, DEFAULT_PORT, DEFAULT_PORT);
	exit(1);
}

static void process_options(int argc, char *argv[])
{
	static struct option long_options[] = {
		{ "port", required_argument, 0, 'p' },
		{ "root", required_argument, 0, 'r' },
		{ "allow", required_argument, 0, 'a' },
		{ "lobby", required_argument, 0, 'l' },
		{ "cert", required_argument, 0, 'c' },
		{ "key", required_argument, 0, 'k' },
		{ "no-tls", no_argument, 0, 'n' },
		{ "verbose", no_argument, 0, 'v' },
		{ "help", no_argument, 0, 'h' },
		{ 0, 0, 0, 0 },
	};
	int c;

	while ((c = getopt_long(argc, argv, "p:r:a:l:c:k:nvh", long_options, NULL)) != -1) {
		switch (c) {
		case 'p':
			listen_port = atoi(optarg);
			break;
		case 'r':
			document_root = optarg;
			break;
		case 'a':
			if (allow_host_port(optarg, 0) != 0)
				exit(1);
			break;
		case 'l':
			lobby_host = optarg;
			break;
		case 'c':
			cert_file = optarg;
			break;
		case 'k':
			key_file = optarg;
			break;
		case 'n':
			tls_disabled = 1;
			break;
		case 'v':
			verbose = 1;
			break;
		default:
			usage();
		}
	}
	if (optind < argc || listen_port <= 0 || listen_port > 65535 || (!cert_file != !key_file))
		usage();
}

int main(int argc, char *argv[])
{
	struct sockaddr_in sa, peer;
	socklen_t peerlen;
	pthread_attr_t attr;
	pthread_t thread;
	struct conn *c;
	int listener, fd, flag = 1;

	process_options(argc, argv);
	signal(SIGPIPE, SIG_IGN);

	if (lobby_host) {
		if (allow_host_port(lobby_host, ntohs(ssgl_get_gamelobby_port("tcp"))) != 0)
			return 1;
		if (pthread_create(&thread, NULL, lobby_thread, NULL) != 0) {
			fprintf(stderr, "%s: cannot start lobby thread\n", P);
			return 1;
		}
		pthread_detach(thread);
	}
	if (nallowed == 0)
		fprintf(stderr, "%s: warning: no --allow or --lobby, so there is nothing to relay to\n", P);
	if (!tls_disabled && set_up_tls() != 0) {
		fprintf(stderr, "%s: cannot set up https (use --no-tls for http only)\n", P);
		return 1;
	}

	listener = socket(AF_INET, SOCK_STREAM, 0);
	if (listener < 0) {
		perror(P ": socket");
		return 1;
	}
	setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(flag));
	memset(&sa, 0, sizeof(sa));
	sa.sin_family = AF_INET;
	sa.sin_addr.s_addr = htonl(INADDR_ANY);
	sa.sin_port = htons(listen_port);
	if (bind(listener, (struct sockaddr *) &sa, sizeof(sa)) != 0 || listen(listener, 64) != 0) {
		fprintf(stderr, "%s: cannot listen on port %d: %s\n", P, listen_port, strerror(errno));
		return 1;
	}
	fprintf(stderr, "%s: serving %s on port %d (%s)\n", P, document_root, listen_port,
		tls_disabled ? "http" : "http and https");

	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
	for (;;) {
		peerlen = sizeof(peer);
		fd = accept(listener, (struct sockaddr *) &peer, &peerlen);
		if (fd < 0) {
			if (errno != EINTR)
				perror(P ": accept");
			continue;
		}
		c = calloc(1, sizeof(*c));
		if (!c) {
			close(fd);
			continue;
		}
		c->fd = fd;
		inet_ntop(AF_INET, &peer.sin_addr, c->peer, INET_ADDRSTRLEN);
		snprintf(c->peer + strlen(c->peer), sizeof(c->peer) - strlen(c->peer),
			":%d", ntohs(peer.sin_port));
		if (pthread_create(&thread, &attr, connection_thread, c) != 0) {
			close(fd);
			free(c);
		}
	}
	return 0;
}
