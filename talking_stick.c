/*
	Copyright (C) 2020 Stephen M. Cameron
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

#include <stdlib.h>
#include <pthread.h>
#include <stdint.h>

#include "talking_stick.h"

static int16_t *talking_stick = NULL;
static pthread_mutex_t talking_stick_mutex = PTHREAD_MUTEX_INITIALIZER;
static int ntalking_sticks = 0;

void talking_stick_setup(int count)
{
	int i;

	talking_stick = calloc(count, sizeof(*talking_stick));
	ntalking_sticks = count;
	for (i = 0; i < ntalking_sticks; i++)
		talking_stick[i] = NO_TALKING_STICK;
}

int16_t talking_stick_get(void)
{
	int16_t i;

	pthread_mutex_lock(&talking_stick_mutex);
	for (i = 0; i < ntalking_sticks; i++)
		if (talking_stick[i] == NO_TALKING_STICK) {
			talking_stick[i] = 1;
			pthread_mutex_unlock(&talking_stick_mutex);
			return i;
		}
	pthread_mutex_unlock(&talking_stick_mutex);
	return NO_TALKING_STICK;
}

void talking_stick_release(int16_t stick)
{
	if (stick < 0 || stick >= ntalking_sticks)
		return;
	pthread_mutex_lock(&talking_stick_mutex);
	talking_stick[stick] = NO_TALKING_STICK;
	pthread_mutex_unlock(&talking_stick_mutex);
}

