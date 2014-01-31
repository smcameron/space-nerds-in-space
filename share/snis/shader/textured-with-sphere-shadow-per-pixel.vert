#version 120
/*
	Copyright (C) 2014 Jeremy Van Grinsven

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

	Author:
		Jeremy Van Grinsven
*/

uniform mat4 u_MVMatrix;  // A constant representing the combined model/view matrix.
uniform mat4 u_MVPMatrix;  // A constant representing the combined model/view/projection matrix.
uniform vec4 u_TintColor;
uniform vec3 u_LightPos; // light position in eye space

attribute vec4 a_Position; // Per-vertex position information we will pass in.
attribute vec2 a_tex_coord; // Per-vertex texture coord we will pass in.

varying vec4 v_TintColor;
varying vec2 v_TexCoord;      // This will be passed into the fragment shader.
varying vec3 v_Position;
varying vec3 v_LightDir;

void main()
{
	v_Position = vec3(u_MVMatrix * a_Position);
	v_LightDir = normalize(u_LightPos - v_Position);

	v_TintColor = u_TintColor;
	v_TexCoord = a_tex_coord;
	gl_Position = u_MVPMatrix * a_Position;

}

