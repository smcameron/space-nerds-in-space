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
#include <stdint.h>
#include <math.h>
#include <sys/time.h>
#include <errno.h>
#include <time.h>

#include "mtwist.h"

#define DEFINE_MATHUTILS_GLOBALS 1
#include "mathutils.h"

double time_now_double()
{
	struct timeval time;
	if (gettimeofday(&time, NULL))
		return 0;
	return (double)time.tv_sec + (double)time.tv_usec * .000001;
}

void sleep_double(double time)
{
	struct timespec t, x;
	double intpart, fractpart;
	int rc;

	fractpart = modf(time, &intpart);
	t.tv_sec = intpart;
	t.tv_nsec = fractpart * 1000000000;

	do {
#if defined(__APPLE__) || defined(__FreeBSD__)
		rc = nanosleep(&t, &x);
#else
		rc = clock_nanosleep(CLOCK_MONOTONIC, 0, &t, &x);
#endif
		t.tv_sec = x.tv_sec;
		t.tv_nsec = x.tv_nsec;
	} while (rc == EINTR);
}

double degrees_to_radians(double degrees)
{
	return degrees * PI /  180.0;
}

double radians_to_degrees(double radians)
{
	return radians * 180.0 / PI;
}

double hypot2(double x, double y)
{
	return x * x + y * y;
}

double hypot3d(double x, double y, double z)
{
	return sqrt(x * x + y * y + z * z);
}

static uint32_t snis_rand_next = 1;

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
	return n * snis_rand() / (SNIS_RAND_MAX + 1);
}

float snis_random_float(void)
{
	return (2.0f * ((float) snis_rand() / 32768.0f) - 1.0f);
}

void normalize_angle(double *angle)
{
	/* FIXME, there's undoubtedly a better way to normalize radians */
	while (*angle > (360.0 * PI / 180.0))
		*angle -= (360.0 * PI / 180.0);
	while (*angle < 0)
		*angle += (360.0 * PI / 180.0);
}

double clamp(double a, double min_val, double max_val)
{
	return (a < min_val ? min_val : (a > max_val ? max_val : a));
}

float clampf(float a, float min_val, float max_val)
{
	return (a < min_val ? min_val : (a > max_val ? max_val : a));
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

double table_interp(double x, const double xv[], const double yv[], int nv)
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
 *          0                             90
 *          |                              |
 *    270 --+-- 90                  180 ---+--- 0
 *          |                              |
 *         180                            270
 *
 * Note this function happens to be its own inverse.
 */
double math_angle_to_game_angle_degrees(double angle)
{
	double a;

	a = (360.0 - angle) + 90.0;
	if (a < 0)
		a += 360.0;
	if (a >= 360.0)
		a -= 360.0;
	return double_modulus(a, 360.0);
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

/* given two points, (x1,y1) and (x2, y2) find eqn of line Ax + By = C */
void line_eqn_from_two_points(double x1, double y1, double x2, double y2,
				double *A, double *B, double *C)
{
	/* knowing that (y1 - y2)x + (x2 - x1)y + (x1y2 - x2y1) = 0 ... */
	*A = y1 - y2;
	*B = x2 - x1;
	*C = -(x1 * y2 - x2 * y1);
}

/* Given two line eqns, A1x + B1y = C1 and A2x + B2y = C2, find the intersection
 * point.  If lines are ~parallel and thus do not intersect, return -1, otherwise
 * return 0 */
int line_intersection(double A1, double B1, double C1,
				double A2, double B2, double C2, double *x, double *y)
{
	double delta = A1 * B2 - A2 * B1;

	if (fabs(delta) < 0.0000001)
		return -1;

	*x = (B2 * C1 - B1 * C2) / delta;	
	*y = (A1 * C2 - A2 * C1) / delta;
	return 0;
}

/* Given 2 points (x1,y1), (x2,y2), find equation of the line which is perpendicular
 * to the line passing through the two points, and which intersects the midpoint
 * between the two points. */          
void perpendicular_line_from_two_points(double x1, double y1, double x2, double y2,
						double *A, double *B, double *C)
{
	double dx, dy, mx, my, px, py;

	/* Find midpoint between p1 and p2. */
	dx = (x2 - x1);
	dy = (y2 - y1);
	mx = x1 + dx / 2.0;
	my = y1 + dy / 2.0;

	/* Find point on line perpendicular to (x1,y1) - (x2,y2); */
	px = mx + dy;
	py = my + dx;

	/* Find eqn of line through (mx,my) and (px,py) */
	line_eqn_from_two_points(mx, my, px, py, A, B, C);
}

/* Given three points on the edge of a circle, find the circle x, y, r.  If no solution,
   returns -1, otherwise 0 */
int circle_from_three_points(double x1, double y1, double x2, double y2, double x3, double y3,
				double *x, double *y, double *r)
{
	double a1, b1, c1, a2, b2, c2, dx, dy;

	perpendicular_line_from_two_points(x1, y1, x2, y2, &a1, &b1, &c1);
	perpendicular_line_from_two_points(x2, y2, x3, y3, &a2, &b2, &c2);
	if (line_intersection(a1, b1, c1, a2, b2, c2, x, y))
		return -1;
	dx = *x - x1;
	dy = *y - y1;
	*r = sqrt(dx * dx + dy * dy);
	return 0;
}

/*
 * Pick random point on the surface of sphere of given radius with
 * uniform distribution (harder than I initially thought).
 */
void random_point_on_sphere(float radius, float *x, float *y, float *z)
{
	float x1, x2, s;

	/* The Marsaglia 1972 rejection method */
	do {
		x1 = snis_random_float();
		x2 = snis_random_float();
		s = x1 * x1 + x2 * x2;
	} while (s > 1.0f);

	*x = 2.0f * x1 * sqrt(1.0f - s);
	*y = 2.0f * x2 * sqrt(1.0f - s);
	*z = 1.0f - 2.0f * s;

	*x *= radius;
	*y *= radius;
	*z *= radius;
}

/*
 * Pick random point on the surface of sphere of given radius with
 * uniform distribution (harder than I initially thought).
 */
void consistent_random_point_on_sphere(struct mtwist_state *mt,
				float radius, float *x, float *y, float *z)
{
	float x1, x2, s;

	/* The Marsaglia 1972 rejection method */
	do {
		x1 = 2.0 * (mtwist_float(mt) - 0.5);
		x2 = 2.0 * (mtwist_float(mt) - 0.5);
		s = x1 * x1 + x2 * x2;
	} while (s > 1.0f);

	*x = 2.0f * x1 * sqrt(1.0f - s);
	*y = 2.0f * x2 * sqrt(1.0f - s);
	*z = 1.0f - 2.0f * s;

	*x *= radius;
	*y *= radius;
	*z *= radius;
}

void random_dpoint_on_sphere(float radius, double *x, double *y, double *z)
{
	float x1, x2, s;

	/* The Marsaglia 1972 rejection method */
	do {
		x1 = snis_random_float();
		x2 = snis_random_float();
		s = x1 * x1 + x2 * x2;
	} while (s > 1.0f);

	*x = 2.0f * x1 * sqrt(1.0f - s);
	*y = 2.0f * x2 * sqrt(1.0f - s);
	*z = fabs(1.0f - 2.0f * s);

	*x *= radius;
	*y *= radius;
	*z *= radius;
}

static inline float dist3dsqrd(const float x, const float y, const float z)
{
	return x * x + y * y + z * z;
}

/* return random point inside sphere of specified radius */
void random_point_in_sphere(float radius, float *x, float *y, float *z,
				float *dsqrd)
{
	const float rsqrd = radius * radius;

	do {
		*x = snis_random_float() * radius;
		*y = snis_random_float() * radius;
		*z = snis_random_float() * radius;
		*dsqrd = dist3dsqrd(*x, *y, *z);
	} while (*dsqrd > rsqrd);
}

static inline int between(double a, double p, double b)
{
	if (a <= p && p <= b)
		return 1;
	if (b <= p && p <= a)
		return 1;
	return 0;
}

static inline int point_between(double ax, double ay, double px, double py, double bx, double by)
{
	return between(ax, px, bx) && between(ay, py, by);
}

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
	double *ix1, double *iy1, double *ix2, double *iy2)
{
	double line_seg_length;
	double dx, dy, ex, ey, LEC, t, dt, r2;
	double da, db;
	int first_point_inside = 0;
	int second_point_inside = 0;

	r2 = r * r;
	da = (cx - x1) * (cx - x1) + (cy - y1) * (cy - y1);
	if (da <= r2)
		first_point_inside = 1;
	db = (cx - x2) * (cx - x2) + (cy - y2) * (cy - y2);
	if (db <= r2)
		second_point_inside = 1;
	if (first_point_inside && second_point_inside) {
		*ix1 = x1;
		*iy1 = y1;
		*ix2 = x2;
		*iy2 = y2;
		return 0;
	}

	line_seg_length = sqrt((x2 - x1) * (x2 - x1) + (y2 - y1) * (y2 - y1));

	/* compute the direction vector d for the line segment */
	dx = (x2 - x1) / line_seg_length;
	dy = (y2 - y1) / line_seg_length;

	/* Now the line equation is x = dx*t + x1, y = dy*t + y1 with 0 <= t <= 1. */

	/* compute the value t of the closest point to the circle center (Cx, Cy) */
	t = dx * (cx - x1) + dy * (cy - y1);

	/* This is the projection of C on the line segment. */

	/* compute the coordinates of the point E on line and closest to C */
	ex = t * dx + x1;
	ey = t * dy + y1;

	/* compute the euclidean distance from E to C */
	LEC = sqrt((ex - cx) * (ex - cx) + (ey - cy) * (ey - cy));

	/* test if the line intersects the circle */
	if (LEC >= r)
		return -1;

	/* compute distance from t to circle intersection point */
	dt = sqrt(r2 - LEC * LEC);

	/* compute first intersection point */
	*ix1 = (t - dt) * dx + x1;
	*iy1 = (t - dt) * dy + y1; 

	/* compute second intersection point */
	*ix2 = (t + dt) * dx + x1; 
	*iy2 = (t + dt) * dy + y1; 

	if (first_point_inside) {
		if (point_between(x1, y1, *ix1, *iy1, x2, y2)) {
			*ix2 = x1;
			*iy2 = y1;
		} else {
			*ix1 = *ix2;
			*iy1 = *iy2;
			*ix2 = x1;
			*iy2 = y1;
		}
		return 1;
	}
	if (second_point_inside) {
		if (point_between(x1, y1, *ix1, *iy1, x2, y2)) {
			*ix1 = *ix2;
			*iy1 = *iy2;
			*ix2 = x2;
			*iy2 = y2;
		} else {
			*ix2 = x2;
			*iy2 = y2;
		}
		return 1;
	}

	/*
	 * Both points are outside the circle, and we have two co-linear points
	 * that intersect the circle.  Either both of those interesections are
	 * between (x1,y1) (x2,y2), in which case the line crosses the circle, or
	 * not, in which case the segment is totally outside the circle.
	 */
	if (point_between(x1, y1, *ix1, *iy1, x2, y2))
		return 2;
	return -1;
}

float float_lerp(float from, float to, float t)
{
	return from + t * (to - from);
}

double short_angular_distance(double from, double to)
{
	double distance;

	distance = to - from;
	if (fabs(distance) < M_PI)
		return distance;
	return 2.0 * M_PI - distance;
}

float sigmoid(float x, float sigma, float lambda)
{
	return powf(x, lambda) / (powf(x, lambda) + powf(sigma, lambda));
}

int imax(int a, int b)
{
	if (a > b)
		return a;
	return b;
}

int imin(int a, int b)
{
	if (a < b)
		return a;
	return b;
}

/* Map (scale and offset) value x from range (min1, max1) to range (min2, max2) */
float fmap(float x, float min1, float max1, float min2, float max2)
{
	return min2 + (x - min1) * (max2 - min2) / (max1 - min1);
}
