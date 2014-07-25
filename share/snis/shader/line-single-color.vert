/*
 * Copyright © 2014 Jeremy Van Grinsven
 *
 * This file is part of Spacenerds In Space.

 * Spacenerds in Space is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Spacenerds in Space is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Spacenerds in Space; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Authors:
 *  Alexandros Frantzis (glmark2)
 */

uniform mat4 u_MVPMatrix;
uniform vec2 u_Viewport;

attribute vec4 a_MultiOne;
attribute vec4 a_Position;
// Coordinates of the line vertices this vertex belongs to
attribute vec4 a_LineVertex0;
attribute vec4 a_LineVertex1;

varying float v_IsDotted;
varying vec4 v_Dist;

void main(void)
{
	// Get the clip coordinates of all vertices
	vec4 pos  = u_MVPMatrix * a_Position;
	vec4 pos0 = u_MVPMatrix * a_LineVertex0;
	vec4 pos1 = u_MVPMatrix * a_LineVertex1;

	// Get the screen coordinates of all vertices
	vec2 p  = 0.5 * u_Viewport * (pos.xy / pos.w);
	vec2 p0 = 0.5 * u_Viewport * (pos0.xy / pos0.w);
	vec2 p1 = 0.5 * u_Viewport * (pos1.xy / pos1.w);

	// Calculate the distance of the current vertex from all
	// the line ends.
	float d0 = length(p - p0);
	float d1 = length(p - p1);

	// OpenGL(ES) performs perspective-correct interpolation
	// (it divides by .w) but we want linear interpolation. To
	// work around this, we premultiply by pos.w here and then
	// multiple with the inverse (stored in dist.w) in the fragment
	// shader to undo this operation.
	v_Dist = vec4(pos.w * d0, pos.w * d1, 0, 1.0 / pos.w);

	v_IsDotted = a_MultiOne.x;

	gl_Position = pos;
}

