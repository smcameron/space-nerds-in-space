/*
 * Copyright © 2010-2011 Alexandros Frantzis
 *
 * This file is part of the glmark2 OpenGL (ES) 2.0 benchmark.
 *
 * glmark2 is free software: you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or (at your option) any later
 * version.
 *
 * glmark2 is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along with
 * glmark2.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors:
 *  Alexandros Frantzis (glmark2)
 */

// Wireframe shader based on:
// J. A. Bærentzen, S. Munk-Lund, M. Gjøl, and B. D. Larsen,
// “Two methods for antialiased wireframe drawing with hidden
// line removal,” in Proceedings of the Spring Conference in
// Computer Graphics, 2008.
//
// We are not using geometry shaders, though, as they are not
// available in GLES 2.0.

attribute vec3 position;
// Coordinates of the triangle vertices this vertex belongs to
attribute vec3 tvertex0;
attribute vec3 tvertex1;
attribute vec3 tvertex2;
attribute vec3 edge_mask;

uniform vec2 Viewport;
uniform mat4 ModelViewProjectionMatrix;

varying vec4 dist;

void main(void)
{
	// Get the clip coordinates of all vertices
	vec4 pos  = ModelViewProjectionMatrix * vec4(position, 1.0);
	vec4 pos0 = ModelViewProjectionMatrix * vec4(tvertex0, 1.0);
	vec4 pos1 = ModelViewProjectionMatrix * vec4(tvertex1, 1.0);
	vec4 pos2 = ModelViewProjectionMatrix * vec4(tvertex2, 1.0);

	// Get the screen coordinates of all vertices
	vec3 p  = vec3(0.5 * Viewport * (pos.xy / pos.w), 0.0);
	vec3 p0 = vec3(0.5 * Viewport * (pos0.xy / pos0.w), 0.0);
	vec3 p1 = vec3(0.5 * Viewport * (pos1.xy / pos1.w), 0.0);
	vec3 p2 = vec3(0.5 * Viewport * (pos2.xy / pos2.w), 0.0);

	// Get the vectors representing the edges of the current
	// triangle primitive. 'vN' is the edge opposite vertex N.
	vec3 v0 = p2 - p1;
	vec3 v1 = p2 - p0;
	vec3 v2 = p1 - p0;

	// Calculate the distance of the current vertex from all
	// the triangle edges. The distance of point p from line
	// v is length(cross(p - p1, v)) / length(v), where
	// p1 is any of the two edge points of v.
	float d0 = length(cross(p - p1, v0)) / length(v0);
	float d1 = length(cross(p - p2, v1)) / length(v1);
	float d2 = length(cross(p - p0, v2)) / length(v2);

	// OpenGL(ES) performs perspective-correct interpolation
	// (it divides by .w) but we want linear interpolation. To
	// work around this, we premultiply by pos.w here and then
	// multiple with the inverse (stored in dist.w) in the fragment
	// shader to undo this operation.
	dist = vec4(pos.w * (d0 + edge_mask.x), pos.w * (d1 + edge_mask.y), pos.w * (d2 + edge_mask.z), 1.0 / pos.w);

	gl_Position = pos;
}

