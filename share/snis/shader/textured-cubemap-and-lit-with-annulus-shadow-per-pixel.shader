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
		Jeremy Van Grinsven, Stephen M. Cameron
*/
#if defined(INCLUDE_VS)
	varying vec3 v_Position;
	varying vec3 v_Normal;
	varying vec3 v_TexCoord;

	#if defined(USE_NORMAL_MAP)
	varying vec3 v_Tangent;
	varying vec3 v_BiTangent;
	varying mat3 tbn;
	#endif

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
	varying vec3 v_Position;
	varying vec3 v_Normal;
	varying vec3 v_TexCoord;

	#if defined(USE_NORMAL_MAP)
	varying vec3 v_Tangent;
	varying vec3 v_BiTangent;
	varying mat3 tbn;
	#endif

	uniform samplerCube u_AlbedoTex;
	uniform vec4 u_TintColor;
	uniform vec3 u_LightPos;   // The position of the light in eye space.
	uniform float u_Ambient;

#if defined(USE_NORMAL_MAP)
	uniform samplerCube u_NormalMapTex;
#endif
#if defined USE_SPECULAR
	uniform vec3 u_WaterColor; // Color of water used in specular calculations
	uniform vec3 u_SunColor; // Color of sun used in specular calculations
#endif

#if defined(USE_ANNULUS_SHADOW)
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
#endif

	void main()
	{
		/* Get a lighting direction vector from the light to the vertex. */
		vec3 light_dir = normalize(u_LightPos - v_Position);

		/* Calculate the dot product of the light vector and vertex normal. If the normal and light vector are
		   pointing in the same direction then it will get max illumination. */
		float direct = dot(normalize(v_Normal), light_dir);

		float shadow = 1.0;

#if defined(USE_ANNULUS_SHADOW)
		float intersect_r_squared;
		if (direct > u_Ambient && intersect_disc(u_AnnulusNormal, u_AnnulusCenter, u_AnnulusRadius.w /* r3^2 */,
				v_Position, light_dir, intersect_r_squared))
		{
			if (intersect_r_squared > u_AnnulusRadius.y /* r1^2 */ ) {
				float ir = sqrt(u_AnnulusRadius.y);
				/* figure out a texture coord on the ring that samples from u=0 to 1, v is given */
				float u = (sqrt(intersect_r_squared) - ir) /
						(u_AnnulusRadius.z - ir);

				vec4 ring_color = u_AnnulusTintColor * texture2D(u_AnnulusAlbedoTex, vec2(u, u_ring_texture_v));

				/* how much we will shadow based on transparancy, so 1.0=no shadow, 0.0=full */
				shadow  = 1.0 - ring_color.a;
			}
		}
#endif

#if defined(USE_NORMAL_MAP)
		vec3 norm_sample = normalize(textureCube(u_NormalMapTex, v_TexCoord).xyz * 2.0 - 1.0);
		vec3 pixel_normal = tbn * norm_sample;
		float normal_map_shadow = max(0.0, dot(pixel_normal, light_dir));
		float diffuse = max(shadow * normal_map_shadow, u_Ambient);

		/* If the normal is straight up, and the color is mostly blue, consider it water,
		 * which then has a specular component.
		 */
		vec3 straight_up = vec3(0.0, 0.0, 1.0);
		float straight_up_normal = step(0.98, dot(norm_sample, straight_up));

                // blinn phong half vector specular
                vec3 view_dir = normalize(-v_Position);
                vec3 half_dir = normalize(light_dir + view_dir);
                float n_dot_h = max(0, clamp(dot(v_Normal, half_dir), 0.0, 1.0));
#if defined(USE_SPECULAR)
		float SpecularPower = 512.0;
		float SpecularIntensity = 0.9;
                float spec = pow(n_dot_h, SpecularPower);

		/* Because we are presuming that the specular reflection is due to reflecting off water,
		 * the reflectance varies with the angle of incidence (from Encyclopedia Brittannica).
		 * If the light is coming straight down on the water, most of it gets transmitted into
		 * the water and very little is reflected, but if it's coming in at a glancing angle, then
		 * most of it is reflected and very little is transmitted into the water.
		 *
		 * Sun's elevation angle (in degrees) 	90 	50 	40 	30 	20 	10 	5
		 * reflectance (percent)	 	3 	3 	4 	6 	12 	27 	42
		 * cos(angle)				0	.643	.766	.866	.939	.984	.996
		 *
		 * For now just hand wavy approximate this with a smoothstep on "direct", which is the cos(angle),
		 * and also exaggerate the brightness a little for looks.  Not really accurate, just kind of a
		 * hack that looks ok.
		 */
		float reflectance = min(1.0, smoothstep(0.0, 0.8, 1.0 - direct) * 1.2 + 0.1);

		vec3 specular_color = straight_up_normal * reflectance * u_SunColor * SpecularIntensity * spec;
#endif
#else
		/* make diffuse light atleast ambient */
		float diffuse = max(direct * shadow, u_Ambient);
#if defined(USE_SPECULAR)
#endif
#endif


		gl_FragColor = textureCube(u_AlbedoTex, v_TexCoord);
#if defined(USE_SPECULAR)
		vec3 white = vec3(1.0, 1.0, 1.0);
		float not_clouds = 1.0 - smoothstep(0.8, 1.0, dot(gl_FragColor.rgb, white));
		float mostly_blue = smoothstep(0.75, 0.8, dot(normalize(u_WaterColor), normalize(gl_FragColor.rgb)));
		gl_FragColor.rgb += specular_color * mostly_blue * not_clouds;
#endif
		gl_FragColor.rgb *= diffuse;

		/* tint with alpha pre multiply */
		gl_FragColor.rgb *= u_TintColor.rgb;
		gl_FragColor *= u_TintColor.a;
		gl_FragColor = filmic_tonemap(gl_FragColor);
	}
#endif

