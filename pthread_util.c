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
#include <stdlib.h>
#include <pthread.h>
#include <string.h>

#include "pthread_util.h"

struct pthread_setname_trampoline_args {
	const char *thread_name;
	void *(*thread_start) (void *);
	void *thread_args;
	pthread_t *thread;
};

/* On MacOS X we can only set a threads name from inside that thread.
 * Because of this, we have pthread_create specify a trampoline function
 * that calls the actual routine that the thread will be running.
 */
void *pthread_setname_trampoline(void *args)
{
	struct pthread_setname_trampoline_args *t_args = args;
	const char *thread_name = t_args->thread_name;
	void *thread_args = t_args->thread_args;
	void *rc;

#ifdef __APPLE__
	pthread_setname_np(thread_name);
#else
#ifdef _GNU_SOURCE
	pthread_setname_np(*t_args->thread, thread_name);
#endif
#endif
	rc = t_args->thread_start(thread_args);
	free(args);
	return rc;
}

int create_thread(pthread_t *thread,  void *(*start_routine) (void *), void *arg, char *name, int detached)
{
	int rc;
	pthread_attr_t attr;
	char *thread_name = NULL;
	struct pthread_setname_trampoline_args *t_args;

	if (name) {
		thread_name = strdup(name);
		if (strlen(thread_name) >= 15)
			thread_name[15] = '\0';
	}
	t_args = malloc(sizeof(struct pthread_setname_trampoline_args));
	t_args->thread_name = thread_name;
	t_args->thread_start = start_routine;
	t_args->thread_args = arg;
	t_args->thread = thread;
	if (detached) {
		pthread_attr_init(&attr);
		pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
		rc = pthread_create(thread, &attr, pthread_setname_trampoline, t_args);
		pthread_attr_destroy(&attr);
	} else {
		rc = pthread_create(thread, NULL, pthread_setname_trampoline, t_args);
	}
	return rc;
}
