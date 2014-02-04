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

uniform samplerCube myTexture;
uniform sampler2D annulusTexture;
uniform vec4 u_TintColor;
uniform vec3 u_LightPos;   // The position of the light in eye space.

uniform vec3 u_AnnulusCenter; // center of disk in eye space
uniform vec3 u_AnnulusNormal; // disk plane normal in eye space
uniform vec4 u_AnnulusRadius; // x=inside r, y=inside r^2, z=outside r, w=outside r^2
uniform vec4 u_AnnulusTintColor;

varying vec3 v_Position;
varying vec3 v_Normal;
varying vec3 v_TexCoord;

bool intersect_plane(vec3 plane_normal, vec3 plane_pos, vec3 ray_pos, vec3 ray_dir, out float t)
{
	float denom = dot(plane_normal, ray_dir);
	if (abs(denom) > 0.000001) {
		vec3 plane_dir = plane_pos - ray_pos;
		t = dot(plane_normal, plane_dir) / denom;
		return t >= 0;
	}
	return false;
}

bool intersect_disc(vec3 disc_normal, vec3 disc_center, float r_squared, vec3 ray_pos, vec3 ray_dir, out float dist2)
{
	float t = 0;
	if (intersect_plane(disc_normal, disc_center, ray_pos, ray_dir, t)) {
		vec3 plane_intersect = ray_pos + ray_dir * t;
		vec3 v = plane_intersect - disc_center;
		dist2 = dot(v, v);
		return dist2 <= r_squared;
	}
	return false;
}

#define AMBIENT 0.1

void main()
{
	/* Get a lighting direction vector from the light to the vertex. */
	vec3 light_dir = normalize(u_LightPos - v_Position);

	/* Calculate the dot product of the light vector and vertex normal. If the normal and light vector are
	   pointing in the same direction then it will get max illumination. */
	float direct = dot(v_Normal, light_dir);

	float shadow = 1.0;

	float intersect_r_squared;
	if (direct > AMBIENT && intersect_disc(u_AnnulusNormal, u_AnnulusCenter, u_AnnulusRadius.w /* r3^2 */,
			v_Position, light_dir, intersect_r_squared))
	{
		if (intersect_r_squared > u_AnnulusRadius.y /* r1^2 */ ) {
			/* figure out a texture coord on the ring that samples from u=0.5, v=1.0 to 0.5 */
			float v = (sqrt(intersect_r_squared) / u_AnnulusRadius.z + 1.0) / 2.0;

			vec4 ring_color = u_AnnulusTintColor * texture2D(annulusTexture, vec2(0.5, v));

			/* how much we will shadow based on transparancy, so 1.0=no shadow, 0.0=full */
			shadow  = 1.0 - ring_color.a;
		}
	}

	/* make diffuse light atleast ambient */
	float diffuse = max(direct * shadow, AMBIENT);

	vec4 tint_color = u_TintColor * vec4(diffuse, diffuse, diffuse, 1.0);

	gl_FragColor = tint_color * textureCube(myTexture, v_TexCoord);
}
