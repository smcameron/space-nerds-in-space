/*
	This file (snis_asset_fetch_web.c) is entirely machine-generated. To the extent
	possible under law, the author(s) have dedicated all copyright and related and
	neighboring rights to this software to the public domain worldwide.  This
	software is distributed without any warranty.

	You should have received a copy of the CC0 Public Domain Dedication along with
	this software. If not, see <http://creativecommons.org>.
*/

/*
 * Browser implementation of snis_asset_fetch.h.
 *
 * Only the default solarsystem is bundled into snis_client.data.  The others
 * are served as plain files beside the page (see "make WEB=1 web"):
 *
 *	solarsystems/<name>.files	the files in the solarsystem, one per line
 *	solarsystems/<name>/...		the files themselves
 *
 * The first time the server moves the client to a solarsystem, this fetches
 * its files into the in-memory filesystem, where the rest of the client reads
 * them as usual.  The browser's cache keeps them for next time.
 *
 * Fetching is a synchronous XMLHttpRequest, which browsers allow on worker
 * threads but not on the main thread, so this must not be called from the
 * main thread.  The client calls it from its reader thread.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <emscripten.h>
#include <emscripten/threading.h>

#define DEFINE_SNIS_ASSET_FETCH_GLOBALS
#include "snis_asset_fetch.h"

/* Fetch url into a newly malloc'ed buffer.  Returns its length, or -1. */
EM_JS(int, js_fetch_sync, (const char *url, unsigned char **data), {
	var xhr = new XMLHttpRequest();
	var bytes, p;

	try {
		xhr.open('GET', UTF8ToString(url), false);
		xhr.responseType = 'arraybuffer';
		xhr.send(null);
	} catch(e) {
		console.log('snis: fetching ' + UTF8ToString(url) + ' failed: ' + e);
		return -1;
	}
	if (xhr.status != 200 || !xhr.response)
		return -1;
	bytes = new Uint8Array(xhr.response);
	p = _malloc(bytes.length + 1);
	if (!p)
		return -1;
	HEAPU8.set(bytes, p);
	HEAPU8[p + bytes.length] = 0;
	HEAPU32[data >> 2] = p;
	return bytes.length;
})

/* Solarsystem names come from the server, so allow only plain names */
static int valid_solarsystem_name(const char *name)
{
	const char *c;

	if (name[0] == '\0' || strlen(name) > 99)
		return 0;
	for (c = name; *c; c++)
		if (!((*c >= 'a' && *c <= 'z') || (*c >= 'A' && *c <= 'Z') ||
			(*c >= '0' && *c <= '9') || *c == '-' || *c == '_'))
			return 0;
	return 1;
}

/* A path from the file list must stay inside the solarsystem's directory */
static int valid_relative_path(const char *path)
{
	if (path[0] == '\0' || path[0] == '/' || strstr(path, ".."))
		return 0;
	return 1;
}

/* Make the directories leading to path */
static int make_parent_directories(const char *path)
{
	char dir[PATH_MAX];
	char *slash;

	snprintf(dir, sizeof(dir), "%s", path);
	for (slash = strchr(dir + 1, '/'); slash; slash = strchr(slash + 1, '/')) {
		*slash = '\0';
		if (mkdir(dir, 0755) != 0 && errno != EEXIST) {
			fprintf(stderr, "snis_asset_fetch: cannot make %s: %s\n", dir, strerror(errno));
			return -1;
		}
		*slash = '/';
	}
	return 0;
}

static int fetch_file(const char *url, const char *path)
{
	unsigned char *data = NULL;
	FILE *f;
	int len, rc = 0;

	len = js_fetch_sync(url, &data);
	if (len < 0) {
		fprintf(stderr, "snis_asset_fetch: cannot fetch %s\n", url);
		return -1;
	}
	if (make_parent_directories(path) != 0) {
		free(data);
		return -1;
	}
	f = fopen(path, "w");
	if (!f) {
		fprintf(stderr, "snis_asset_fetch: cannot write %s: %s\n", path, strerror(errno));
		free(data);
		return -1;
	}
	if (len > 0 && fwrite(data, len, 1, f) != 1)
		rc = -1;
	if (fclose(f) != 0)
		rc = -1;
	free(data);
	return rc;
}

static int fetch_one(const char *asset_dir, const char *solarsystem, const char *relpath)
{
	char url[PATH_MAX], path[PATH_MAX];

	snprintf(url, sizeof(url), "solarsystems/%s/%s", solarsystem, relpath);
	snprintf(path, sizeof(path), "%s/solarsystems/%s/%s", asset_dir, solarsystem, relpath);
	return fetch_file(url, path);
}

int snis_fetch_solarsystem_assets(const char *asset_dir, const char *solarsystem)
{
	char path[PATH_MAX], url[PATH_MAX];
	unsigned char *list = NULL;
	char *line, *next;
	struct stat statbuf;
	int len, rc = 0;

	if (!valid_solarsystem_name(solarsystem)) {
		fprintf(stderr, "snis_asset_fetch: bad solarsystem name '%s'\n", solarsystem);
		return -1;
	}

	/* assets.txt is fetched last, so if it is here, everything is */
	snprintf(path, sizeof(path), "%s/solarsystems/%s/assets.txt", asset_dir, solarsystem);
	if (stat(path, &statbuf) == 0)
		return 0;

	if (emscripten_is_main_runtime_thread()) {
		fprintf(stderr, "snis_asset_fetch: cannot fetch %s on the main thread\n", solarsystem);
		return -1;
	}

	fprintf(stderr, "snis_asset_fetch: fetching solarsystem %s\n", solarsystem);
	snprintf(url, sizeof(url), "solarsystems/%s.files", solarsystem);
	len = js_fetch_sync(url, &list);
	if (len < 0) {
		fprintf(stderr, "snis_asset_fetch: no solarsystem %s on the web server\n", solarsystem);
		return -1;
	}

	for (line = (char *) list; line && *line; line = next) {
		next = strchr(line, '\n');
		if (next)
			*next++ = '\0';
		if (strncmp(line, "./", 2) == 0)
			line += 2;
		if (line[0] == '\0' || strcmp(line, "assets.txt") == 0)
			continue;
		if (!valid_relative_path(line)) {
			fprintf(stderr, "snis_asset_fetch: skipping bad path '%s'\n", line);
			continue;
		}
		if (fetch_one(asset_dir, solarsystem, line) != 0)
			rc = -1;
	}
	free(list);
	if (rc == 0)
		rc = fetch_one(asset_dir, solarsystem, "assets.txt");
	return rc;
}
