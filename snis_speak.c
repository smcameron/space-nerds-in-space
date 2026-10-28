/*
	Copyright (C) 2017 Stephen M. Cameron
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

/* Moved here from do_text_to_speech() in snis_client.c */

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "snis_bin_dir.h"
#define DEFINE_SNIS_SPEAK_GLOBALS
#include "snis_speak.h"

void snis_speak(const char *text, float volume)
{
	char command[PATH_MAX];
	char *bindir;
	struct stat statbuf;
	int rc;

	bindir = get_snis_bin_dir();

	/* test that snisbindir is actually a directory. */
	rc = stat(bindir, &statbuf);
	if (rc < 0) {
		fprintf(stderr, "Cannot stat %s: %s\n", bindir, strerror(errno));
		return;
	}
	if (!S_ISDIR(statbuf.st_mode)) {
		fprintf(stderr, "%s is not a directory.\n", bindir);
		return;
	}
	snprintf(command, sizeof(command), "export SNIS_TTS_VOLUME=%1.2f; %s/snis_text_to_speech.sh '%s'",
				volume, bindir, text);
	rc = system(command);
	if (rc != 0 && errno != ECHILD)  { /* we have ignored SIGCHLD, so we get ECHILD here */
		fprintf(stderr, "Shell command '%s' returned %d, errno = %d (%s)\n",
			command, rc, errno, strerror(errno));
	}
}
