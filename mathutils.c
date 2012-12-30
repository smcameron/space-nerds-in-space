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
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

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

void normalize_angle(double *angle)
{
	/* FIXME, there's undoubtedly a better way to normalize radians */
	while (*angle > (360.0 * PI / 180.0))
		*angle -= (360.0 * PI / 180.0);
	while (*angle < 0)
		*angle += (360.0 * PI / 180.0);
}

double interpolate(double x, double x1, double y1, double x2, double y2)
{
	/* return corresponding y on line x1,y1,x2,y2 for value x
	 *
	 *      (y2 -y1)/(x2 - x1) = (y - y1) / (x - x1)     by similar triangles.
	 *      (x -x1) * (y2 -y1)/(x2 -x1) = y - y1         a little algebra...
	 *      y = (x - x1) * (y2 - y1) / (x2 -x1) + y1;    I think there's one more step
	 *                                                   which would optimize this a bit more.
	 *                                                   but I forget how it goes. 
	 *
	 * Calling this with x2 == x1 is your own damn fault.
	 *
	 */
	return (x - x1) * (y2 - y1) / (x2 -x1) + y1;
}

double table_interp(double x, double xv[], double yv[], int nv)
{
	int i;

	for (i = 0; i < nv - 1; i++) {
		if (xv[i] <= x && xv[i + 1] > x)
			return interpolate(x, xv[i], yv[i], xv[i + 1], yv[i + 1]);
	}
	/* if you get here, it's your own damn fault. */
	printf("tabe_interp: x value %g is not in table, your program is buggy.\n", x);
	return 0.0;
}

static double double_modulus(double a, double b)
{
	return a - floor(a / b) * b;
}

/*
 * convert an angle between the following two systems. 
 *         game                           math
 *          0                             PI/2
 *          |                              |
 *  3*PI/2--+--PI/2                  PI ---+--- 0
 *          |                              |
 *         PI                             3*PI/2 
 *
 * Note this function happens to be its own inverse.
 */
double math_angle_to_game_angle(double angle)
{
	double a;

	a = (2.0 * M_PI - angle) + M_PI / 2.0;
	if (a < 0)
		a += 2.0 * M_PI;
	if (a >= 2.0 * M_PI)
		a -= 2.0 * M_PI;
	return double_modulus(a, 2.0 * M_PI);
}

double game_angle_to_math_angle(double angle)
{
	return math_angle_to_game_angle(angle); 
}

