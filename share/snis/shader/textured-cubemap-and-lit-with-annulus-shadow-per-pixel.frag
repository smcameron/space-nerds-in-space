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

uniform vec3 u_AnnulusCenter; // center of disk in eye space
uniform vec3 u_AnnulusNormal; // disk plane normal in eye space
uniform vec4 u_AnnulusRadius; // x=inside r, y=inside r^2, z=outside r, w=outside r^2
uniform vec4 u_AnnulusTintColor;

varying float v_InShadow; // 1 if all vertices are in shadow
varying vec4 v_TintColor;
varying vec3 v_TexCoord;
varying vec3 v_Position;
varying vec3 v_LightDir;

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

void main()
{
	vec4 shadow_tint = vec4(1.0);

	float intersect_r_squared;
	if (v_InShadow < 0.999 && intersect_disc(u_AnnulusNormal, u_AnnulusCenter, u_AnnulusRadius.w /* r3^2 */,
			v_Position, normalize(v_LightDir), intersect_r_squared))
	{
		if (intersect_r_squared > u_AnnulusRadius.y /* r1^2 */ ) {
			/* figure out a texture coord on the ring that samples from u=0.5, v=1.0 to 0.5 */
			float v = (sqrt(intersect_r_squared) / u_AnnulusRadius.z + 1.0) / 2.0;

			vec4 ring_color = u_AnnulusTintColor * texture2D(annulusTexture, vec2(0.5, v));

			/* shade is how much we will shadow based on transparancy, so 1.0=no shadow, 0.0=full */
			float shade = max((1.0 - ring_color.a), 0.1);

			shadow_tint = vec4(shade, shade, shade, 1.0);
		}
	}
	/* don't double count the shadows and drop below ambient light... */
	vec4 tmpshade = shadow_tint * v_TintColor;
	vec4 tmpshade2 = vec4(max(tmpshade.r, 0.1), max(tmpshade.g, 0.1), max(tmpshade.b, 0.1),
				tmpshade.a);
	gl_FragColor = tmpshade2 * textureCube(myTexture, v_TexCoord);
}
