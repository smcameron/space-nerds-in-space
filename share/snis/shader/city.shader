/*
	Copyright (C) 2014 Stephen M. Cameron

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
		Stephen M. Cameron
*/
#if defined(INCLUDE_VS)
	out vec3 v_Position;
	out vec2 v_TexCoord;
	out vec3 v_Normal;
	out vec4 v_TintColor;

	uniform mat4 u_MVPMatrix;
	uniform mat4 u_MVMatrix;
	uniform mat3 u_NormalMatrix;
	uniform vec4 u_TintColor;

#if defined(USE_CSM)
	/* Also declared in the fragment stage, which lights with it; the vertex stage needs it
	 * to work out how steeply the light strikes each vertex. */
	uniform vec3 u_LightPos;
#endif

	in vec4 a_Position;
	in vec2 a_TexCoord;
	in vec3 a_Normal;

	void main()
	{
		v_Normal = normalize(u_NormalMatrix * a_Normal);
		v_Position = vec3(u_MVMatrix * a_Position);
		v_TexCoord = a_TexCoord;
		v_TintColor = u_TintColor;

#if defined(USE_CSM)
		csm_set_shadow_coords(a_Position, normalize(a_Normal),
			csm_grazing(v_Normal, u_LightPos - v_Position));
#endif

		gl_Position = u_MVPMatrix * a_Position;
	}
#endif

#if defined(INCLUDE_FS)
	in vec3 v_Position;
	in vec2 v_TexCoord;
	in vec3 v_Normal;
	in vec4 v_TintColor;

	uniform sampler2D u_AlbedoTex;
	uniform sampler2D u_EmitTex;
	uniform vec3 u_LightPos;
	uniform vec3 u_LightColor;
	uniform vec3 u_AmbientColor;
	uniform float u_in_shade;

#if defined(USE_ANNULUS_SHADOW)
	uniform sampler2D u_AnnulusAlbedoTex;
	uniform vec3 u_AnnulusCenter; /* center of disk in eye space */
	uniform vec3 u_AnnulusNormal; /* disk plane normal in eye space */
	uniform vec4 u_AnnulusRadius; /* x=inside r, y=inside r^2, z=outside r, w=outside r^2 */
	uniform vec4 u_AnnulusTintColor;
	uniform float u_ring_texture_v;

	bool intersect_plane(vec3 plane_normal, vec3 plane_pos, vec3 ray_pos, vec3 ray_dir, out float t)
	{
		float denom = dot(plane_normal, ray_dir);
		if (abs(denom) > 0.000001) {
			vec3 plane_dir = plane_pos - ray_pos;
			t = dot(plane_normal, plane_dir) / denom;
			return t >= 0.0;
		}
		return false;
	}

	bool intersect_disc(vec3 disc_normal, vec3 disc_center, float r_squared, vec3 ray_pos,
		vec3 ray_dir, out float dist2)
	{
		float t = 0.0;
		if (intersect_plane(disc_normal, disc_center, ray_pos, ray_dir, t)) {
			vec3 plane_intersect = ray_pos + ray_dir * t;
			vec3 v = plane_intersect - disc_center;
			dist2 = dot(v, v);
			return dist2 <= r_squared;
		}
		return false;
	}
#endif

	out vec4 f_FragColor;

	void main()
	{
		vec3 light_dir = normalize(u_LightPos - v_Position);
		float light_dot = dot(v_Normal, light_dir);

		/* Direct sunlight term: closer to 1.0 in direct light, 0 or negative in dark */
		float direct = clamp(light_dot, 0.0, 1.0);

		/* Start from whatever fraction of the star is not hidden by some body between
		 * here and it -- a planet's umbra, or a ring around the star. */
		float shadow = 1.0 - u_in_shade;

#if defined(USE_ANNULUS_SHADOW)
		float intersect_r_squared;
		if (direct > 0.0 && intersect_disc(u_AnnulusNormal, u_AnnulusCenter,
				u_AnnulusRadius.w /* r3^2 */,
				v_Position, light_dir, intersect_r_squared))
		{
			if (intersect_r_squared > u_AnnulusRadius.y /* r1^2 */ ) {
				float ir = sqrt(u_AnnulusRadius.y);
				float u = (sqrt(intersect_r_squared) - ir) /
						(u_AnnulusRadius.z - ir);

				vec4 ring_color = u_AnnulusTintColor * texture(u_AnnulusAlbedoTex,
						vec2(u, u_ring_texture_v));

				/* how much we will shadow based on transparency, so 1.0=no shadow, 0.0=full */
				shadow *= 1.0 - ring_color.a;
			}
		}
#endif

#if defined(USE_CSM)
		int csm_cascade;
		shadow *= csm_shadow_factor(csm_cascade, -v_Position.z);
#endif

		/* Emittance fade: fully visible in darkness, fades out as direct light rises.
		 * Fully faded out at direct >= 0.1, fully visible at direct <= 0.0. */
		float emit_fade = clamp((0.1 - light_dot) / 0.1, 0.0, 1.0);

		vec4 albedo = texture(u_AlbedoTex, v_TexCoord);
		vec4 emit = texture(u_EmitTex, v_TexCoord);

		/* Diffuse contribution (day side), modulated by shadow */
		vec3 diff_color = albedo.rgb * u_LightColor * direct * shadow;
		float diff_alpha = albedo.a * direct * shadow;

		/* Emittance contribution (night side) */
		vec3 emit_color = emit.rgb * emit_fade;
		float emit_alpha = emit.a * emit_fade;

		/* Combined premultiplied color and alpha */
		vec3 color = diff_color * diff_alpha + emit_color * emit_alpha;
		float alpha = clamp(diff_alpha + emit_alpha, 0.0, 1.0);

		color *= v_TintColor.rgb * v_TintColor.a;
		alpha *= v_TintColor.a;

		f_FragColor = filmic_tonemap(clamp(vec4(color, alpha), 0.0, 1.0));

#if defined(USE_CSM)
		f_FragColor = csm_debug_color(f_FragColor, csm_cascade, shadow);
#endif
	}
#endif
