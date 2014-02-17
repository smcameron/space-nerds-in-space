#version 120
/*
	Copyright © 2014 Jeremy Van Grinsven

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

uniform mat4 u_MVPMatrix;
uniform vec2 u_Viewport;
uniform vec4 u_TintColor;

attribute vec4 a_Position; // Per-vertex position information we will pass in.
attribute vec2 a_tex_coord; // Per-vertex texture coord we will pass in.

// The inverse of the texture dimensions along X and Y
varying vec2 texcoordOffset;

varying vec4 vertColor;
varying vec2 vertTexcoord;

void main(void)
{
	texcoordOffset = 1.0 / u_Viewport;
	vertColor = u_TintColor;
	vertTexcoord = a_tex_coord;

	gl_Position = u_MVPMatrix * a_Position;
}

