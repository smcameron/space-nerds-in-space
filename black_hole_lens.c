/*
	Copyright (C) 2026 Stephen M. Cameron
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

#include <math.h>
#include <stddef.h>

#include "mathutils.h"
#include "quat.h"

#define DEFINE_BLACK_HOLE_LENS_GLOBALS
#include "black_hole_lens.h"
#undef DEFINE_BLACK_HOLE_LENS_GLOBALS

/* How far out a hole's distortion is worth counting, as a multiple of its Einstein radius.  The
 * deflection is einstein^2 / theta, so it has no outer edge to find: it just thins out forever,
 * and integrated over the sky it diverges, so a cutoff has to be chosen and it may as well be
 * chosen by what can be seen.  By three Einstein radii the sky is displaced by a third of one,
 * which is where the arcs stop being arcs and become a faint smear.  Only the ranking depends on
 * this, and only for a hole near the edge of the view: a hole fully in view scores its whole
 * area either way.
 */
#define BLACK_HOLE_LENS_EXTENT 3.0

float black_hole_lens_swirl_of(uint32_t id, float swirl)
{
	float mag = swirl * (1.0 - 0.4 * (float) (id % 5) / 4.0);

	return (id & 1) ? -mag : mag;
}

int black_hole_lens_select(const struct black_hole_lens_hole *hole, int nholes,
			const struct black_hole_lens_view *view,
			float lens_strength, float swirl,
			struct graph_dev_gravitational_lens *lens, int *chosen, float *coverage)
{
	struct lens_candidate {
		struct graph_dev_gravitational_lens lens;
		float coverage;		/* fraction of the window its distortion falls on */
		float einstein;		/* only a tie-break; see the insertion below */
		float distance;		/* camera to hole, for ordering the slots once chosen */
		int hole;
	} best[MAX_GRAVITATIONAL_LENSES], c;
	union vec3 fwd, up, right, to_hole;
	float tan_half_y, tan_half_x, pixels_per_radian;
	int i, j, n, nbest = 0;

	for (i = 0; i < nholes; i++)
		if (coverage)
			coverage[i] = 0.0;

	if (nholes <= 0 || view->screen_width <= 0 || view->screen_height <= 0)
		return 0;

	/* The camera's basis, matching camera_set_orientation(): forward is the rotated x axis and
	 * up the rotated y axis, which makes right = forward x up the rotated z axis. */
	fwd.v.x = 1.0; fwd.v.y = 0.0; fwd.v.z = 0.0;
	up.v.x = 0.0; up.v.y = 1.0; up.v.z = 0.0;
	right.v.x = 0.0; right.v.y = 0.0; right.v.z = 1.0;
	quat_rot_vec_self(&fwd, &view->cam_orientation);
	quat_rot_vec_self(&up, &view->cam_orientation);
	quat_rot_vec_self(&right, &view->cam_orientation);

	tan_half_y = tanf(0.5 * view->fov);
	tan_half_x = tan_half_y * (float) view->screen_width / (float) view->screen_height;
	pixels_per_radian = 0.5 * (float) view->screen_height / tan_half_y;

	for (i = 0; i < nholes; i++) {
		float dist, theta_obj, einstein, x, y, z, sx, sy, extent, cov;

		vec3_sub(&to_hole, &hole[i].pos, &view->cam_pos);
		dist = vec3_magnitude(&to_hole);
		if (dist <= hole[i].radius) /* inside it; no sensible lens to describe */
			continue;
		vec3_normalize_self(&to_hole);
		z = vec3_dot(&to_hole, &fwd);
		if (z <= 0.0) /* behind the camera; nothing to project and nothing to see */
			continue;

		theta_obj = atan2f(hole[i].radius, dist);
		einstein = lens_strength * theta_obj;

		/* Score by how much of the window the distortion actually falls on, not by how big
		 * the hole is: a slot spent on a hole that is off the edge, or mostly off it, buys
		 * nothing, and the hole being looked straight at should have one.  The distortion is
		 * a disc of BLACK_HOLE_LENS_EXTENT Einstein radii about the hole, so project the
		 * hole and clip that disc to the window.
		 *
		 * The disc's radius is an angle times a constant pixels-per-radian, which is exact
		 * only on the view axis and stretches away from it; the centre is projected properly.
		 * Since this decides a ranking and not a rendering that is close enough -- and it is
		 * what keeps a hole just barely in front of the camera, whose projection runs off to
		 * infinity, from scoring anything.
		 *
		 * A hole entirely in view scores pi * (extent * einstein)^2, so ranking by coverage
		 * agrees with ranking by Einstein radius whenever nothing is clipped.  It only
		 * differs at the edges of the window, which is where ranking by size gets it wrong.
		 */
		x = vec3_dot(&to_hole, &right);
		y = vec3_dot(&to_hole, &up);
		sx = (0.5 + 0.5 * (x / z) / tan_half_x) * (float) view->screen_width;
		sy = (0.5 - 0.5 * (y / z) / tan_half_y) * (float) view->screen_height;
		extent = BLACK_HOLE_LENS_EXTENT * einstein * pixels_per_radian;
		cov = disc_rectangle_intersection_area(sx, sy, extent, 0.0, 0.0,
				view->screen_width, view->screen_height) /
					((float) view->screen_width * (float) view->screen_height);
		if (coverage)
			coverage[i] = cov;
		if (cov <= 0.0)
			continue;

		c.lens.direction[0] = to_hole.v.x;
		c.lens.direction[1] = to_hole.v.y;
		c.lens.direction[2] = to_hole.v.z;
		c.lens.einstein_radius = einstein;
		c.lens.shadow_radius = theta_obj;
		c.lens.swirl = black_hole_lens_swirl_of(hole[i].id, swirl);
		c.coverage = cov;
		c.einstein = einstein;
		c.distance = dist;
		c.hole = i;

		/* Keep the best few by coverage, breaking ties on Einstein radius -- two holes close
		 * enough to flood the window both score 1.0, and then the bigger one should win.  An
		 * insertion into a list of three rather than a sort of everything, so however many
		 * holes are in the sky this costs one pass and no allocation. */
		if (nbest < MAX_GRAVITATIONAL_LENSES)
			nbest++;
		else if (cov < best[nbest - 1].coverage ||
			(cov == best[nbest - 1].coverage && einstein <= best[nbest - 1].einstein))
			continue;
		for (j = nbest - 1; j > 0 && (best[j - 1].coverage < c.coverage ||
				(best[j - 1].coverage == c.coverage &&
					best[j - 1].einstein < c.einstein)); j--)
			best[j] = best[j - 1];
		best[j] = c;
	}

	/* Which holes get a slot is one question, what order they go into the slots is another.
	 * The shader bends the ray by one lens at a time in slot order, so they have to run in the
	 * order the light meets them -- nearest first -- or a far hole lenses the sky as though the
	 * near hole in front of it were not there.  Insertion sort: nbest is at most three. */
	for (i = 1; i < nbest; i++) {
		c = best[i];
		for (j = i; j > 0 && best[j - 1].distance > c.distance; j--)
			best[j] = best[j - 1];
		best[j] = c;
	}

	for (n = 0; n < nbest; n++) {
		lens[n] = best[n].lens;
		if (chosen)
			chosen[n] = best[n].hole;
	}
	return n;
}
