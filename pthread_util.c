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
#define _GNU_SOURCE
#include <stdio.h>
#include <pthread.h>
#include <string.h>

#include "pthread_util.h"

int create_thread(pthread_t *thread,  void *(*start_routine) (void *), void *arg, char *name, int detached)
{
	int rc;
	pthread_attr_t attr;
	char *thread_name;

	if (name) {
		thread_name = strdup(name);
		if (strlen(thread_name) >= 15)
			thread_name[15] = '\0';
	}
	if (detached) {
		pthread_attr_init(&attr);
		pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
		rc = pthread_create(thread, &attr, start_routine, arg);
		pthread_attr_destroy(&attr);
	} else {
		rc = pthread_create(thread, NULL, start_routine, arg);
	}
	if (!rc && name)
		pthread_setname_np(*thread, thread_name);
	return rc;
}
