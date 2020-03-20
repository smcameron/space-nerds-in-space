/*
	Copyright (C) 2014 Jeremy Van Grinsven
	Copyright (C) 2017 Stephen M. Cameron 

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
	attribute vec4 a_Position; // Per-vertex position information we will pass in.
	attribute vec2 a_TexCoord; // Per-vertex texture coord we will pass in.
	attribute vec3 a_Normal;   // Per-vertex normal we pass in.

	varying vec4 v_TintColor;
	varying vec2 v_TexCoord;
	varying vec3 v_Position;		// Fragment position in eye space
	varying vec3 v_LightDir;		// normalized vector from fragment to light in eye space
	varying float v_darkside_shading;	// Shading multiplier for ring, 1.0 on light side, 0.2 on dark side
	varying vec3 v_Normal;			// Normal at fragment (i.e. normal to the ring plane)
	varying float v_sameside;		// 1.0 if eye and light are on same side of ring plane, 0.0 otherwise

	uniform mat4 u_MVMatrix;  // A constant representing the combined model/view matrix.
	uniform mat4 u_MVPMatrix;  // A constant representing the combined model/view/projection matrix.
	uniform mat3 u_NormalMatrix;
	uniform vec4 u_TintColor;
	uniform vec3 u_LightPos; // light position in eye space
	uniform float u_ring_texture_v; // v coord in ring texture

	void main()
	{
		v_Position = vec3(u_MVMatrix * a_Position);		// Transform the vertex into eye space.
		v_Normal = normalize(u_NormalMatrix * a_Normal);	// Transform the normal's orientation into eye space.
		v_LightDir = normalize(u_LightPos - v_Position);	// Find direction from vertex to light

		// Check if the light source is on the same side of the surface as the eye.
		// Dot product of vector to point with v_Normal will be positive or negative
		// depending on which side of the plane defined by v_Normal the point lies.
		// Find the dot product for eye, and for light.  If they are the same sign,
		// then they are on the same side (the light side) othewise they are on
		// opposite sides (camera is on dark side).
		float lightdot = dot(v_LightDir, v_Normal);
		float eyedot = dot(-v_Position, v_Normal);

		v_sameside = lightdot * eyedot;			// > 0 means same side, < 0 means opposite
		v_sameside = v_sameside / abs(v_sameside);	// Scale to either +1.0 or to -1.0
		v_sameside = (v_sameside + 1.0) / 2.0;		// transform -1.0 -> 0.0, and +1.0 -> +1.0
		v_darkside_shading = v_sameside * 1.0 + (1.0 - v_sameside) * 0.2; // either 1.0 or 0.2

		v_TintColor = u_TintColor;
		v_TexCoord = a_TexCoord;
		v_TexCoord.y = u_ring_texture_v;
		gl_Position = u_MVPMatrix * a_Position;

	}
#endif

#if defined(INCLUDE_FS)
	varying vec4 v_TintColor;
	varying vec2 v_TexCoord;
	varying vec3 v_Position;		// Fragment position in eye space
	varying vec3 v_LightDir;		// normalized vector from fragment to light in eye space
	varying float v_darkside_shading;	// Shading multiplier for ring, 1.0 on light side, 0.2 on dark side
	varying vec3 v_Normal;			// Normal at fragment (i.e. normal to the ring plane)
	varying float v_sameside;		// 1.0 if eye and light are on same side of ring plane, 0.0 otherwise

	uniform sampler2D u_AlbedoTex;
	uniform vec4 u_Sphere; /* eye space occluding sphere, x,y,z = center, w = radius^2 */
	uniform float u_ring_inner_radius;
	uniform float u_ring_outer_radius;

	float map(in float x, float min1, float max1, float min2, float max2)
	{
		return min2 + (x - min1) * (max2 - min2) / (max1 - min1);
	}

	/* Returns 1.0 if no intersect (not in shadow), 0.0 if intersect (in shadow) */
	float sphere_ray_intersect(in vec4 sphere, in vec3 ray_pos, in vec3 ray_dir)
	{
		vec3 dir_sphere = ray_pos - sphere.xyz;
		const float in_shadow = 0.0;
		const float not_in_shadow = 1.0;

		float b = 2.0 * dot(dir_sphere, ray_dir);
		float c = dot(dir_sphere, dir_sphere) - sphere.w;

		float disc = b * b - 4.0 * c;
		if (disc < 0.0)
			return not_in_shadow;

		float sqrt_disc = sqrt(disc);
		float t0 = (-b - sqrt_disc) / 2.0;
		if (t0 >= 0)
			return in_shadow;
		float t1 = (-b + sqrt_disc) / 2.0;
		return 1.0 - (max(sign(t1), 0));	// if (t1 >= 0) return 0.0; else return 1.0;
	}

	void main()
	{
		vec4 shadow_tint = vec4(1.0);
		vec2 txcoord;
		vec3 spec_color = vec3(0.4);
		vec4 shadow_color = vec4(0.25, 0.1, 0.1, 1.0);
		const float shininess = 14.0;
		float not_in_shadow, in_shadow;

		not_in_shadow = sphere_ray_intersect(u_Sphere, v_Position, normalize(v_LightDir));
		in_shadow = 1.0 - not_in_shadow;
		shadow_tint = not_in_shadow * shadow_tint + in_shadow * shadow_color;

		txcoord.y = v_TexCoord.y;
		float inner, outer;

		inner = map(u_ring_inner_radius, 1.0, 4.0, 0.0, 1.0);
		outer = map(u_ring_outer_radius, 1.0, 4.0, 0.0, 1.0);
		txcoord.x = max(0.0, min(1.0, map(v_TexCoord.x, inner, outer, 0.0, 1.0)));

                // blinn phong half vector specular
                vec3 view_dir = normalize(-v_Position);
                vec3 half_dir = normalize(v_LightDir + view_dir);
                float n_dot_h = max(0, dot(v_Normal, half_dir));
                float n_dot_h2 = max(0, dot(-v_Normal, half_dir)); /* Consider both sides */
                n_dot_h = max(n_dot_h, n_dot_h2);
                float spec = pow(n_dot_h, shininess);

		gl_FragColor = shadow_tint * texture2D(u_AlbedoTex, txcoord);
		gl_FragColor.rgb *= v_darkside_shading + (0.5 * (1.0 - v_darkside_shading) * 0.5 * abs(gl_FragColor.a - 0.5));
		gl_FragColor.rgb += not_in_shadow * v_sameside * spec * spec_color * gl_FragColor.a;

		/* tint with alpha pre multiply */
		gl_FragColor.rgb *= v_TintColor.rgb;
		gl_FragColor *= v_TintColor.a;
		gl_FragColor = filmic_tonemap(gl_FragColor);
	}
#endif

