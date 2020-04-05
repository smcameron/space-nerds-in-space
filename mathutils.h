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

#include "mtwist.h"

#define PI (3.14159265)

#ifndef DEFINE_MATHUTILS_GLOBALS
#define GLOBAL extern
#else 
#define GLOBAL
#endif

GLOBAL double time_now_double();
GLOBAL void sleep_double(double time);

GLOBAL double degrees_to_radians(double degrees);
GLOBAL double radians_to_degrees(double radians);
GLOBAL double hypot2(double x, double y);
GLOBAL double hypot3d(double x, double y, double z);

#define SNIS_RAND_MAX (32767)

GLOBAL int snis_rand(void); /* like rand() */
GLOBAL void snis_srand(unsigned seed); /* like srand() */
GLOBAL int snis_randn(int n); /* returns n * snis_rand() / SNIS_RAND_MAX */
GLOBAL float snis_random_float(); /* return random number -1 <= n <= 1 */
GLOBAL void normalize_angle(double *angle);
GLOBAL double interpolate(double x, double x1, double y1, double x2, double y2);
GLOBAL double table_interp(double x, const double xv[], const double yv[], int nv);
GLOBAL double game_angle_to_math_angle(double angle);
GLOBAL double math_angle_to_game_angle(double angle);
GLOBAL double math_angle_to_game_angle_degrees(double angle);
GLOBAL double clamp(double a, double min_val, double max_val);
GLOBAL float clampf(float a, float min_val, float max_val);

/* given two points, (x1,y1) and (x2, y2) find eqn of line Ax + By = C */
GLOBAL void line_eqn_from_two_points(double x1, double y1, double x2, double y2,
				double *A, double *B, double *C);

/* Given two line eqns, A1x + B1y = C1 and A2x + B2y = C2, find the intersection
 * point.  If lines are parallel and thus do not intersect, return -1, otherwise
 * return 0 */
GLOBAL int line_intersection(double A1, double B1, double C1,
		double A2, double B2, double C2, double *x, double *y);

/* Given 2 points (x1,y1), (x2,y2), find equation of the line which is perpendicular
 * to the line passing through the two points, and which intersects the midpoint
 * between the two points. */
GLOBAL void perpendicular_line_from_two_points(double x1, double y1, double x2, double y2,
				double *A, double *B, double *C);

/* Given three points on the edge of a circle, find the circle x, y, r.  If no solution,
   returns -1, otherwise 0 */
GLOBAL int circle_from_three_points(double x1, double y1, double x2, double y2, double x3, double y3,
		double *x, double *y, double *r);

/* Return random point on surface of sphere of given radius */
GLOBAL void random_point_on_sphere(float radius, float *x, float *y, float *z);
GLOBAL void consistent_random_point_on_sphere(struct mtwist_state *mt,
				float radius, float *x, float *y, float *z);
GLOBAL void random_dpoint_on_sphere(float radius, double *x, double *y, double *z);

/* return random point inside sphere of specified radius */
GLOBAL void random_point_in_sphere(float radius, float *x, float *y, float *z,
                                float *dist3dsqrd);
/*
 * circle line intersection code adapted from:
 * http://stackoverflow.com/questions/1073336/circle-line-collision-detection
 *
 * with changes to handle cases:
 *    both points inside circle
 *    neither point inside circle, no intersections
 *    neither point inside circle, two intersections
 *    first point inside, 2nd outside, 1 intersection
 *    first point outside, 2nd inside, 1 intersection 
 *
 * Returns number of intersections (-1, 0, 1 or 2), and intersection values in
 * 	(*ix1,*iy1) and * (*ix2,*iy2)
 * 
 * -1 means no intersections, both endpoints are outside the circle
 * 0 means no intersections, both endpoints are inside the circle
 * 1 means 1 intersection, 1 point inside the circle
 *         (*ix1,*iy1, and *ix2,*iy2 will all be filled in.)
 * 2 means 2 intersections
 *
 */
int circle_line_segment_intersection(double x1, double y1, double x2, double y2,
	double cx, double cy, double r,
	double *ix1, double *iy1, double *ix2, double *iy2);

float float_lerp(float from, float to, float t);

/* Given angles from and to (radians) returns the difference between them
 * going the short way around the circle
 */
double short_angular_distance(double from, double to);

/* See: http://www.jfurness.uk/sigmoid-functions-in-game-design/
 *
 * This function maps real numbers 0 <= x <= +infinity to the range 0 - 1.0
 * in a "sigmoid" ("S") shape that is controlled by sigma and lambda.
 * (Approximately) sigma controls how fast the curve ramps up to 1.0
 * and lambda controls the "shape" of the curve with values < 1.0
 * making the curve steep at first then slowing, and values > 1.0
 * making the curve gradual, then steep, then gradual.
 */
float sigmoid(float x, float sigma, float lambda);

int imax(int a, int b);
int imin(int a, int b);

/* Map (scale and offset) value x from range (min1, max1) to range (min2, max2) */
float fmap(float x, float min1, float max1, float min2, float max2);

#undef GLOBAL
#endif
