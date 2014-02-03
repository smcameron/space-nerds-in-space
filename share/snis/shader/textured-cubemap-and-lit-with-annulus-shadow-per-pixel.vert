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

uniform mat4 u_MVPMatrix;  // A constant representing the combined model/view/projection matrix.
uniform mat4 u_MVMatrix;   // A constant representing the combined model/view matrix.
uniform mat3 u_NormalMatrix;
uniform vec4 u_TintColor;
uniform vec3 u_LightPos;   // The position of the light in eye space.

attribute vec4 a_Position; // Per-vertex position information we will pass in.
attribute vec3 a_Normal;   // Per-vertex normal information we will pass in.

varying float v_InShadow;
varying vec3 v_TexCoord;
varying vec4 v_TintColor;
varying vec3 v_Position;
varying vec3 v_LightDir;

void main()
{
	// Transform the vertex into eye space.
	v_Position = vec3(u_MVMatrix * a_Position);

	// Transform the normal's orientation into eye space.
	vec3 modelViewNormal = normalize(u_NormalMatrix * a_Normal);

	// Get a lighting direction vector from the light to the vertex.
	v_LightDir = normalize(u_LightPos - v_Position);

	// Calculate the dot product of the light vector and vertex normal. If the normal and light vector are
	// pointing in the same direction then it will get max illumination.
	float dot = dot(modelViewNormal, v_LightDir);

	// give 10% ambient
	float diffuse = max(dot, 0.1);

	v_InShadow = dot <= 0.0 ? 1.0 : 0.0;

	// Multiply the color by the illumination level. It will be interpolated across the triangle.
	v_TintColor = u_TintColor * vec4(diffuse, diffuse, diffuse, 1.0);
	v_TexCoord = a_Position.xyz;

	// gl_Position is a special variable used to store the final position.
	// Multiply the vertex by the matrix to get the final point in normalized screen coordinates.
	gl_Position = u_MVPMatrix * a_Position;
}
