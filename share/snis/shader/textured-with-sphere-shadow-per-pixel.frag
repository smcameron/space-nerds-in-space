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

uniform sampler2D myTexture;
uniform vec4 u_Sphere; /* eye space occluding sphere, x,y,z = center, w = radius^2 */

varying vec4 v_TintColor;
varying vec2 v_TexCoord;
varying vec3 v_Position;
varying vec3 v_LightDir;

bool sphere_ray_intersect(in vec4 sphere, in vec3 ray_pos, in vec3 ray_dir)
{
	vec3 dir_sphere = ray_pos - sphere.xyz;

	float b = 2.0 * dot(dir_sphere, ray_dir);
	float c = dot(dir_sphere, dir_sphere) - sphere.w;

	float disc = b * b - 4.0 * c;
	if (disc < 0.0)
		return false;

	float sqrt_disc = sqrt(disc);
	float t0 = (-b - sqrt_disc) / 2.0;
	if (t0 >= 0)
		return true;
	float t1 = (-b + sqrt_disc) / 2.0;
	return t1 >= 0;
}

void main()
{
	vec4 shadow_tint = vec4(1.0);

	if (sphere_ray_intersect(u_Sphere, v_Position, normalize(v_LightDir)))
		shadow_tint = vec4(0.4,0.4,0.4,1.0);

	gl_FragColor = shadow_tint * v_TintColor * texture2D(myTexture, v_TexCoord);
}

