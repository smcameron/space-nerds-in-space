/*
        Copyright (C) 2010 Stephen M. Cameron
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

#define DEFINE_MATHUTILS_GLOBALS 1
#include "mathutils.h"

double degrees_to_radians(double degrees)
{
	return degrees * PI /  180.0;
}

double radians_to_degrees(double radians)
{
	return radians * 180.0 / PI;
}

static unsigned long snis_rand_next = 1;

int snis_rand(void)
{
	snis_rand_next = snis_rand_next * 1103515245 + 12345;
	return ((unsigned) (snis_rand_next / 65536) % 32768);
}

void snis_srand(unsigned seed)
{
	snis_rand_next = seed;
}

int snis_randn(int n)
{
	return n * snis_rand() / SNIS_RAND_MAX;
}

