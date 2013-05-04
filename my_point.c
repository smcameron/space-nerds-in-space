/* 
    (C) Copyright 2007,2008,2010,2013 Stephen M. Cameron.

    This file is part of Space Nerds In Space.

    Space Nerds In Space is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    Space Nerds In Space is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Space Nerds In Space; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

 */
#include <stdlib.h>
#include <math.h>

#define DEFINE_MY_POINT_GLOBALS
#include "my_point.h"
#undef DEFINE_MY_POINT_GLOBALS

void rotate_points(struct my_point_t *points, int npoints,
			struct my_point_t *rotated_points, double angle,
			int originx, int originy)
{
	int i;
	double startx, starty, start_angle, new_angle, newx, newy;
	double magnitude, diff;

	for (i = 0; i < npoints; i++) {

		if (points[i].x == LINE_BREAK ||
			points[i].x == COLOR_CHANGE) {
			rotated_points[i].x = points[i].x;
			rotated_points[i].y = points[i].y;
			continue;
		}
		
		startx = (double) (points[i].x - originx);
		starty = (double) (points[i].y - originy);
		magnitude = sqrt(startx * startx + starty * starty);
		diff = (startx - 0);
		if (diff < 0 && diff > -0.00001)
			start_angle = 0.0;
		else if (diff > 00 && diff < 0.00001)
			start_angle = M_PI;
		else
			start_angle = atan2(starty, startx);
		new_angle = start_angle + angle;
		newx = cos(new_angle) * magnitude + originx;
		newy = sin(new_angle) * magnitude + originy;
		rotated_points[i].x = newx;
		rotated_points[i].y = newy;
		/* printf("s=%f,%f, e=%f,%f, angle = %f/%f\n", 
			startx, starty, newx, newy, 
			start_angle * 180/M_PI, new_angle * 360.0 / (2.0 * M_PI));  */
	}
}

void spin_points(struct my_point_t *points, int npoints, 
	struct my_point_t **spun_points, int nangles,
	int originx, int originy)
{
	int i;
	double angle;
	double angle_inc;

	*spun_points = (struct my_point_t *) 
		malloc(sizeof(*spun_points) * npoints * nangles);
	memset(*spun_points, 0, sizeof(*spun_points) * npoints * nangles);
	if (*spun_points == NULL)
		return;

	angle_inc = (2.0 * M_PI) / (nangles);

	for (i = 0; i < nangles; i++) {
		angle = angle_inc * (double) i;
		/* printf("Rotation angle = %f\n", angle * 180.0 / M_PI); */
		rotate_points(points, npoints, &(*spun_points)[i * npoints],
				angle, originx, originy);
	} 
}

void calculate_bbox(struct my_vect_obj *v)
{
	int i, xmin, ymin, xmax, ymax;

	xmin = v->p[0].x;
	ymin = v->p[0].y;
	xmax = v->p[0].x;
	ymax = v->p[0].y;

	for (i = 0; i < v->npoints; i++) {
		if (v->p[i].x < xmin)
			xmin = v->p[i].x;
		if (v->p[i].y < ymin)
			ymin = v->p[i].y;
		if (v->p[i].x > xmax)
			xmax = v->p[i].x;
		if (v->p[i].y > ymax)
			ymax = v->p[i].y;
	}
	v->bbx1 = xmin;
	v->bby1 = ymin;
	v->bbx2 = xmax;
	v->bby2 = ymax;
}

