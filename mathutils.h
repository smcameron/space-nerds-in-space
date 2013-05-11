#ifndef __MATHUTILS_H__
#define __MATHUTILS_H__

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

#define PI (3.14159265)

#ifndef DEFINE_MATHUTILS_GLOBALS
#define GLOBAL extern
#else 
#define GLOBAL
#endif

GLOBAL double degrees_to_radians(double degrees);
GLOBAL double radians_to_degrees(double radians);

#define SNIS_RAND_MAX (32767)

GLOBAL int snis_rand(void); /* like rand() */
GLOBAL void snis_srand(unsigned seed); /* like srand() */
GLOBAL int snis_randn(int n); /* returns n * snis_rand() / SNIS_RAND_MAX */
GLOBAL void normalize_angle(double *angle);
GLOBAL double interpolate(double x, double x1, double y1, double x2, double y2);
GLOBAL double table_interp(double x, double xv[], double yv[], int nv);
GLOBAL double game_angle_to_math_angle(double angle);
GLOBAL double math_angle_to_game_angle(double angle);

/* given two points, (x1,y1) and (x2, y2) find eqn of line Ax + By = C */
GLOBAL void line_eqn_from_two_points(double x1, double y1, double x2, double y2,
				double *A, double *B, double *C);

/* Given two line eqns, A1x + B1y = C1 and A2x + B2y = C2, find the intersection
 * point.  If lines are parallel and thus do not intersect, return -1, otherwise
 * return 0 */
GLOBAL int line_intersection(double A1, double B1, double C1,
		double A2, double B2, double C2, double *x, double *y);

#endif
