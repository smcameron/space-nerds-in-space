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

varying vec3 v_Position;
varying vec3 v_Normal;
varying vec3 v_TexCoord;

#if defined(USE_NORMAL_MAP)
varying vec3 v_Tangent;
varying vec3 v_BiTangent;
varying mat3 tbn;
#endif

#if defined(INCLUDE_VS)
	uniform mat4 u_MVPMatrix;  // A constant representing the combined model/view/projection matrix.
	uniform mat4 u_MVMatrix;   // A constant representing the combined model/view matrix.
	uniform mat3 u_NormalMatrix;

	attribute vec4 a_Position; // Per-vertex position information we will pass in.
	attribute vec3 a_Normal;   // Per-vertex normal, tangent, and bitangent information we will pass in.
#if defined(USE_NORMAL_MAP)
	attribute vec3 a_Tangent;
	attribute vec3 a_BiTangent;
#endif

	void main()
	{
		/* Transform the vertex into eye space. */
		v_Position = vec3(u_MVMatrix * a_Position);

		/* Transform the normal's, Tangent's and BiTangent's orientations into eye space. */
		v_Normal = normalize(u_NormalMatrix * a_Normal);
#if defined(USE_NORMAL_MAP)
		v_Tangent = normalize(u_NormalMatrix * a_Tangent);
		v_BiTangent = normalize(u_NormalMatrix * a_BiTangent);
		tbn = mat3(v_Tangent, v_BiTangent, v_Normal);
#endif

		v_TexCoord = a_Position.xyz;

		/* gl_Position is a special variable used to store the final position.
		   Multiply the vertex by the matrix to get the final point in normalized screen coordinates. */
		gl_Position = u_MVPMatrix * a_Position;
	}
#endif

#if defined(INCLUDE_FS)
	uniform samplerCube u_AlbedoTex;
	uniform vec4 u_TintColor;
	uniform vec3 u_LightPos;   // The position of the light in eye space.
#if defined(USE_NORMAL_MAP)
	uniform samplerCube u_NormalMapTex;
#endif

	uniform sampler2D u_AnnulusAlbedoTex;
	uniform vec3 u_AnnulusCenter; // center of disk in eye space
	uniform vec3 u_AnnulusNormal; // disk plane normal in eye space
	uniform vec4 u_AnnulusRadius; // x=inside r, y=inside r^2, z=outside r, w=outside r^2
	uniform vec4 u_AnnulusTintColor;
	uniform float u_ring_texture_v;

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

	bool intersect_disc(vec3 disc_normal, vec3 disc_center, float r_squared, vec3 ray_pos,
		vec3 ray_dir, out float dist2)
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

#if !defined(AMBIENT)
	#define AMBIENT 0.1
#endif

	void main()
	{
		/* Get a lighting direction vector from the light to the vertex. */
		vec3 light_dir = normalize(u_LightPos - v_Position);
		float ir = sqrt(u_AnnulusRadius.y);

		/* Calculate the dot product of the light vector and vertex normal. If the normal and light vector are
		   pointing in the same direction then it will get max illumination. */
		float direct = dot(normalize(v_Normal), light_dir);

		float shadow = 1.0;

		float intersect_r_squared;
		if (direct > AMBIENT && intersect_disc(u_AnnulusNormal, u_AnnulusCenter, u_AnnulusRadius.w /* r3^2 */,
				v_Position, light_dir, intersect_r_squared))
		{
			if (intersect_r_squared > u_AnnulusRadius.y /* r1^2 */ ) {
				/* figure out a texture coord on the ring that samples from u=0 to 1, v is given */
				float u = (sqrt(intersect_r_squared) - ir) /
						(u_AnnulusRadius.z - ir);

				vec4 ring_color = u_AnnulusTintColor * texture2D(u_AnnulusAlbedoTex, vec2(u, u_ring_texture_v));

				/* how much we will shadow based on transparancy, so 1.0=no shadow, 0.0=full */
				shadow  = 1.0 - ring_color.a;
			}
		}
#if defined(USE_NORMAL_MAP)
		vec3 pixel_normal = tbn * normalize(textureCube(u_NormalMapTex, v_TexCoord).xyz * 2.0 - 1.0);
		float normal_map_shadow = max(0.0, dot(pixel_normal, light_dir));
		float diffuse = max(shadow * normal_map_shadow, AMBIENT);
#else
		/* make diffuse light atleast ambient */
		float diffuse = max(direct * shadow, AMBIENT);
#endif

		gl_FragColor = textureCube(u_AlbedoTex, v_TexCoord);
		gl_FragColor.rgb *= diffuse;

		/* tint with alpha pre multiply */
		gl_FragColor.rgb *= u_TintColor.rgb;
		gl_FragColor *= u_TintColor.a;
	}
#endif

