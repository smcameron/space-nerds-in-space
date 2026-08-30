/*
	Copyright (C) 2026 Stephen M. Cameron

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
	varying vec3 v_Position;
	varying vec2 v_TexCoord;
	varying vec3 v_Normal;
	varying vec4 v_TintColor;

	uniform mat4 u_MVPMatrix;
	uniform mat4 u_MVMatrix;
	uniform mat3 u_NormalMatrix;
	uniform vec4 u_TintColor;

	attribute vec4 a_Position;
	attribute vec2 a_TexCoord;
	attribute vec3 a_Normal;

	void main()
	{
		v_Normal = normalize(u_NormalMatrix * a_Normal);
		v_Position = vec3(u_MVMatrix * a_Position);
		v_TexCoord = a_TexCoord;
		v_TintColor = u_TintColor;
		gl_Position = u_MVPMatrix * a_Position;
	}
#endif

#if defined(INCLUDE_FS)
	varying vec3 v_Position;
	varying vec2 v_TexCoord;
	varying vec3 v_Normal;
	varying vec4 v_TintColor;

	uniform sampler2D u_AlbedoTex;
	uniform sampler2D u_EmitTex;
	uniform vec3 u_LightPos;
	uniform vec3 u_LightColor;
	uniform vec3 u_AmbientColor;
	uniform float u_in_shade;

	void main()
	{
		vec3 light_dir = normalize(u_LightPos - v_Position);
		float light_dot = dot(v_Normal, light_dir);

		/* Direct sunlight term: closer to 1.0 in direct light, 0 or negative in dark */
		float direct = clamp(light_dot, 0.0, 1.0);

		/* Start from whatever fraction of the star is not hidden by some body between
		 * here and it -- a planet's umbra. */
		float shadow = 1.0 - u_in_shade;

		/* Emittance fade: fully visible in darkness, fades out as direct light rises.
		 * Fully faded out at direct >= 0.1, fully visible at direct <= 0.0. */
		float emit_fade = clamp((0.1 - light_dot) / 0.1, 0.0, 1.0);

		vec4 albedo = texture2D(u_AlbedoTex, v_TexCoord);
		vec4 emit = texture2D(u_EmitTex, v_TexCoord);

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

		gl_FragColor = filmic_tonemap(clamp(vec4(color, alpha), 0.0, 1.0));
	}
#endif
