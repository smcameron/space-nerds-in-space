/*
	Copyright (C) 2026 Stephen M. Cameron

	This file is part of Spacenerds In Space.

	Spacenerds In Space is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.

	Spacenerds In Space is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with Spacenerds In Space; if not, write to the Free Software
	Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#if defined(INCLUDE_VS)
	in vec3 a_Position;

	out vec4 v_TintColor;
	out vec3 v_RingPos;

	uniform mat4 u_MVPMatrix;
	uniform vec4 u_TintColor;
	uniform vec3 u_CamRingPos;
	uniform vec4 u_DustParams; /* x=planet_radius, y=half_h, z=opacity, w=cam_mode */

	void main()
	{
		vec3 local_pos;

		if (u_DustParams.w > 0.5) {
			/* Camera-enclosing box scaled in world units around cam_pos */
			local_pos = a_Position;
			v_RingPos = u_CamRingPos + a_Position * (20.0 / max(1.0, u_DustParams.x));
		} else {
			/* Planet-centered annular slab in normalized planet-radius units */
			local_pos = vec3(a_Position.x, a_Position.y, a_Position.z * u_DustParams.y);
			v_RingPos = local_pos;
		}
		v_TintColor = u_TintColor;
		gl_Position = u_MVPMatrix * vec4(local_pos, 1.0);
	}
#endif

#if defined(INCLUDE_FS)
	in vec4 v_TintColor;
	in vec3 v_RingPos;

	uniform sampler2D u_AlbedoTex;
	uniform float u_in_shade;
	uniform float u_ring_texture_v;
	uniform float u_ring_inner_radius;
	uniform float u_ring_outer_radius;
	uniform vec3 u_CamRingPos;
	uniform vec3 u_LightRingPos;
	uniform vec4 u_DustParams; /* x=planet_radius, y=half_h, z=opacity, w=cam_mode */
	uniform vec4 u_DustDist;   /* x=fade_dist, y=draw_dist */

	out vec4 f_FragColor;

	float hash12(vec2 p)
	{
		vec3 p3 = fract(vec3(p.xyx) * 0.1031);

		p3 += dot(p3, p3.yzx + 33.33);
		return fract((p3.x + p3.y) * p3.z);
	}

	float unit_sphere_shadow(vec3 p, vec3 ldir)
	{
		float b = dot(p, ldir);
		float c = dot(p, p) - 1.0;
		float disc = b * b - c;

		if (disc < 0.0)
			return 0.0;
		float t0 = -b - sqrt(disc);
		return (t0 >= 0.0) ? 1.0 : 0.0;
	}

	void main()
	{
		float planet_radius = max(1.0, u_DustParams.x);
		float half_h = max(1e-4, u_DustParams.y);
		float opacity = max(0.0, u_DustParams.z);
		float max_dist_norm = u_DustDist.y / planet_radius;

		vec3 ro = u_CamRingPos;
		vec3 rd = normalize(v_RingPos - u_CamRingPos);

		/* Intersect ray with slab |z| <= half_h */
		float t_min = 0.0;
		float t_max = max_dist_norm;
		if (abs(rd.z) > 1e-6) {
			float tz0 = (-half_h - ro.z) / rd.z;
			float tz1 = (half_h - ro.z) / rd.z;
			t_min = max(t_min, min(tz0, tz1));
			t_max = min(t_max, max(tz0, tz1));
		} else if (abs(ro.z) > half_h) {
			discard;
		}
		if (t_max <= t_min)
			discard;

		/* Clamp against planet sphere |P| = 1.0 */
		float b_p = dot(ro, rd);
		float c_p = dot(ro, ro) - 1.0;
		float disc_p = b_p * b_p - c_p;
		if (disc_p >= 0.0) {
			float tp0 = -b_p - sqrt(disc_p);
			if (tp0 > 0.0)
				t_max = min(t_max, tp0);
		}
		if (t_max <= t_min)
			discard;

		/* Clamp against outer ring cylinder |P.xy| <= u_ring_outer_radius */
		float a_c = dot(rd.xy, rd.xy);
		if (a_c > 1e-7) {
			float b_c = dot(ro.xy, rd.xy);
			float c_c = dot(ro.xy, ro.xy) - u_ring_outer_radius * u_ring_outer_radius;
			float disc_c = b_c * b_c - a_c * c_c;
			if (disc_c < 0.0)
				discard;
			float sq_c = sqrt(disc_c);
			t_min = max(t_min, (-b_c - sq_c) / a_c);
			t_max = min(t_max, (-b_c + sq_c) / a_c);
			if (t_max <= t_min)
				discard;
		}

		const int STEPS = 12;
		float dt = (t_max - t_min) / float(STEPS);
		float dt_world = dt * planet_radius;
		float jitter = hash12(gl_FragCoord.xy);
		float extinction = opacity * 0.0009;
		float ring_span = max(1e-4, u_ring_outer_radius - u_ring_inner_radius);

		vec3 accum_rgb = vec3(0.0);
		float accum_a = 0.0;

		for (int i = 0; i < STEPS; i++) {
			float t = t_min + (float(i) + jitter) * dt;
			vec3 p = ro + rd * t;
			float r_xy = length(p.xy);

			if (r_xy > u_ring_inner_radius && r_xy < u_ring_outer_radius) {
				float tx_u = clamp((r_xy - u_ring_inner_radius) / ring_span, 0.0, 1.0);
				vec4 tex = texture(u_AlbedoTex, vec2(tx_u, u_ring_texture_v));
				float z_norm = p.z / half_h;
				float v_dens = exp(-2.2 * z_norm * z_norm);
				float t_world = t * planet_radius;
				float dist_fade = 1.0 - smoothstep(u_DustDist.x, u_DustDist.y, t_world);

				vec3 ldir = normalize(u_LightRingPos - p);
				float in_shadow = unit_sphere_shadow(p, ldir);
				vec3 sample_rgb = tex.rgb * mix(vec3(1.0), vec3(0.25, 0.1, 0.1), in_shadow);

				float sigma = tex.a * v_dens * dist_fade * extinction;
				float step_a = 1.0 - exp(-sigma * dt_world);

				accum_rgb += (1.0 - accum_a) * sample_rgb * step_a;
				accum_a += (1.0 - accum_a) * step_a;
			}
		}

		if (accum_a <= 0.001)
			discard;

		accum_rgb *= (1.0 - u_in_shade);
		accum_rgb *= v_TintColor.rgb;
		accum_rgb *= v_TintColor.a;
		accum_a *= v_TintColor.a;
		f_FragColor = filmic_tonemap(vec4(accum_rgb, accum_a));
	}
#endif
