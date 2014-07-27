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

varying vec4 v_TintColor;
varying vec2 v_TexCoord;
varying vec3 v_Position;
varying vec3 v_LightDir;

#if defined(INCLUDE_VS)
	uniform mat4 u_MVMatrix;  // A constant representing the combined model/view matrix.
	uniform mat4 u_MVPMatrix;  // A constant representing the combined model/view/projection matrix.
	uniform vec4 u_TintColor;
	uniform vec3 u_LightPos; // light position in eye space
	uniform float u_ring_texture_v; // v coord in ring texture

	attribute vec4 a_Position; // Per-vertex position information we will pass in.
	attribute vec2 a_TexCoord; // Per-vertex texture coord we will pass in.

	void main()
	{
		v_Position = vec3(u_MVMatrix * a_Position);
		v_LightDir = normalize(u_LightPos - v_Position);

		v_TintColor = u_TintColor;
		v_TexCoord = a_TexCoord;
		v_TexCoord.y = u_ring_texture_v;
		gl_Position = u_MVPMatrix * a_Position;

	}
#endif

#if defined(INCLUDE_FS)
	uniform sampler2D u_AlbedoTex;
	uniform vec4 u_Sphere; /* eye space occluding sphere, x,y,z = center, w = radius^2 */

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

		gl_FragColor = shadow_tint * texture2D(u_AlbedoTex, v_TexCoord);

		/* tint with alpha pre multiply */
		gl_FragColor.rgb *= v_TintColor.rgb;
		gl_FragColor *= v_TintColor.a;
	}
#endif

