/*
	Copyright (C) 2019 Stephen M. Cameron
	Author: Stephen M. Cameron

	This file is part of Spacenerds In Space.

	Spacenerds in Space is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.

	Spacenerds in Space is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with Spacenerds in Space; if not, write to the Free Software
	Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

#include "xdg_base_dir_spec.h"
#include "string-utils.h"

struct xdg_base_context {
	char *appname;
	char *legacy_path;
	char *xdg_data_home;
	char *home;
};

struct xdg_base_context *xdg_base_context_new(char *appname, char *legacy_path)
{
	struct xdg_base_context *cx;
	char *s;
	char *home;

	if (!appname)
		return NULL;
	cx = malloc(sizeof(*cx));
	cx->appname = strdup(appname);
	cx->legacy_path = legacy_path ? strdup(legacy_path) : NULL;
	s = getenv("XDG_DATA_HOME");
	cx->xdg_data_home = (s && s[0] == '/') ? strdup(s) : NULL;
	home = getenv("HOME");
	cx->home = home ? strdup(home) : NULL;

	return cx;
}

void xdg_base_context_free(struct xdg_base_context *cx)
{
	if (!cx)
		return;
	if (cx->appname)
		free(cx->appname);
	if (cx->legacy_path)
		free(cx->legacy_path);
	if (cx->xdg_data_home)
		free(cx->xdg_data_home);
	if (cx->home)
		free(cx->home);
	free(cx);
}

char *xdg_base_slurp_file(struct xdg_base_context *cx, char *filename)
{
	char p[PATH_MAX];
	struct stat sb;
	int rc, bytes;
	char *file_contents;

	/* Try $XDG_DATA_HOME/appname/filename */
	if (cx->xdg_data_home && cx->appname) {
		snprintf(p, sizeof(p), "%s/%s/%s", cx->xdg_data_home, cx->appname, filename);
		rc = stat(p, &sb);
		if (rc == 0) {
			file_contents = slurp_file(p, &bytes);
			if (file_contents)
				return file_contents;
		}
	}
	/* Try $HOME/.local/share/cx->appname/filename */
	if (cx->home && cx->appname) {
		snprintf(p, sizeof(p), "%s/.local/share/%s/%s", cx->home, cx->appname, filename);
		rc = stat(p, &sb);
		if (rc == 0) {
			file_contents = slurp_file(p, &bytes);
			if (file_contents)
				return file_contents;
		}
	}
	/* try legacy path, if any */
	if (cx->legacy_path && cx->home) {
		snprintf(p, sizeof(p), "%s/%s/%s", cx->home, cx->legacy_path, filename);
		rc = stat(p, &sb);
		if (rc == 0) {
			file_contents = slurp_file(p, &bytes);
			if (file_contents)
				return file_contents;
		}
	}
	return NULL;
}

static int do_mkdir(char *path)
{
	int rc;

	rc = mkdir(path, 0700);
	if (rc != 0 && errno != EEXIST) {
		fprintf(stderr, "Failed to create directory %s: %s\n", path, strerror(errno));
		return -1;
	}
	return 0;
}

FILE *xdg_base_fopen_for_write(struct xdg_base_context *cx, char *filename)
{
	char dir[PATH_MAX];
	char p[PATH_MAX];
	FILE *f;

	if (cx->xdg_data_home && cx->appname) {
		snprintf(dir, sizeof(dir), "%s/%s", cx->xdg_data_home, cx->appname);
		if (!do_mkdir(dir)) {
			snprintf(p, sizeof(p), "%s/%s", dir, filename);
			f = fopen(p, "w");
			return f;
		}
	}
	if (cx->home && cx->appname) {
		snprintf(dir, sizeof(dir), "%s/.local/share/%s", cx->home, cx->appname);
		if (!do_mkdir(dir)) {
			snprintf(p, sizeof(p), "%s/%s", dir, filename);
			f = fopen(p, "w");
			return f;
		}
	}
	if (cx->legacy_path && cx->home) {
		snprintf(dir, sizeof(dir), "%s/%s", cx->home, cx->legacy_path);
		if (!do_mkdir(dir)) {
			snprintf(p, sizeof(p), "%s/%s", dir, filename);
			f = fopen(p, "w");
			return f;
		}
	}
	return NULL;
}

FILE *xdg_base_fopen_for_read(struct xdg_base_context *cx, char *filename)
{
	char p[PATH_MAX];
	FILE *f;

	if (cx->xdg_data_home && cx->appname) {
		snprintf(p, sizeof(p), "%s/%s/%s", cx->xdg_data_home, cx->appname, filename);
		f = fopen(p, "r");
		if (f)
			return f;
	}
	if (cx->home && cx->appname) {
		snprintf(p, sizeof(p), "%s/.local/share/%s/%s", cx->home, cx->appname, filename);
		f = fopen(p, "r");
		if (f)
			return f;
	}
	if (cx->legacy_path && cx->home) {
		snprintf(p, sizeof(p), "%s/%s/%s", cx->home, cx->legacy_path, filename);
		f = fopen(p, "r");
		if (f)
			return f;
	}
	return NULL;
}

int xdg_base_open_for_overwrite(struct xdg_base_context *cx, char *filename)
{
	char dir[PATH_MAX];
	char p[PATH_MAX];
	int fd;

	if (cx->xdg_data_home && cx->appname) {
		snprintf(dir, sizeof(dir), "%s/%s", cx->xdg_data_home, cx->appname);
		if (!do_mkdir(dir)) {
			snprintf(p, sizeof(p), "%s/%s", dir, filename);
			fd = open(p, O_CREAT | O_TRUNC | O_WRONLY, 0644);
			return fd;
		}
	}
	if (cx->home && cx->appname) {
		snprintf(dir, sizeof(dir), "%s/.local/share/%s", cx->home, cx->appname);
		if (!do_mkdir(dir)) {
			snprintf(p, sizeof(p), "%s/%s", dir, filename);
			fd = open(p, O_CREAT | O_TRUNC | O_WRONLY, 0644);
			return fd;
		}
	}
	if (cx->legacy_path && cx->home) {
		snprintf(dir, sizeof(dir), "%s/%s", cx->home, cx->legacy_path);
		if (!do_mkdir(dir)) {
			snprintf(p, sizeof(p), "%s/%s", dir, filename);
			fd = open(p, O_CREAT | O_TRUNC | O_WRONLY, 0644);
			return fd;
		}
	}
	return -1;
}

char *xdg_base_data_filename(struct xdg_base_context *cx, char *basename, char *filename, size_t filenamelen)
{
	char p[PATH_MAX];

	if (cx->xdg_data_home && cx->appname)
		snprintf(p, sizeof(p), "%s/%s/%s", cx->xdg_data_home, cx->appname, basename);
	else if (cx->home && cx->appname)
		snprintf(p, sizeof(p), "%s/.local/share/%s/%s", cx->home, cx->appname, basename);
	else if (cx->legacy_path && cx->home)
		snprintf(p, sizeof(p), "%s/%s/%s", cx->home, cx->legacy_path, basename);
	else
		return NULL;

	snprintf(filename, filenamelen, "%s", p);
	return filename;

}

#ifdef TEST_XDG_BASE_DIR

int main(int argc, char *argv[])
{
	/* TODO: write some tests */
	printf("text_xdg_base_dir: All 0 tests pass\n");
	return 0;
}
#endif
