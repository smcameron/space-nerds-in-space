uniform vec3 u_TintColor;
uniform float u_CoreBrightness;
uniform float u_PlumeLength;
uniform float u_NoiseSeed;
uniform float u_DiamondSpacing;
uniform float u_DiamondIntensity;

in vec2 v_TexCoord;
in vec3 v_ViewPos;
in vec3 v_Normal;

out vec4 f_FragColor;

float hash21(vec2 p)
{
	p = fract(p * vec2(234.34, 435.345));
	p += dot(p, p + 34.23);
	return fract(p.x * p.y);
}

float noise2d(vec2 p)
{
	vec2 i = floor(p);
	vec2 f = fract(p);
	vec2 u = f * f * (3.0 - 2.0 * f);
	float a = hash21(i);
	float b = hash21(i + vec2(1.0, 0.0));
	float c = hash21(i + vec2(0.0, 1.0));
	float d = hash21(i + vec2(1.0, 1.0));
	return mix(mix(a, b, u.x), mix(c, d, u.x), u.y);
}

void main()
{
	vec3 V = normalize(-v_ViewPos);
	vec3 N = normalize(v_Normal);
	float NdotV = abs(dot(N, V));

	/* Soft edge fade eliminating hard polygon silhouette edges */
	float edge_fade = smoothstep(0.0, 0.4, NdotV);
	/* Cylinder optical depth increases towards the central axis */
	float optical_depth = pow(NdotV, 0.6);

	float axial = v_TexCoord.x;
	float angular = v_TexCoord.y;

	/* Fade out towards the tip based on throttle (u_PlumeLength) */
	float max_len = max(u_PlumeLength, 0.05);
	float axial_fade = 1.0 - smoothstep(max_len * 0.65, max_len, axial);
	axial_fade *= (1.0 - smoothstep(0.85, 1.0, axial));

	/* Shock diamonds (repeating Mach discs inside the core) */
	float spacing = max(u_DiamondSpacing, 0.04);
	float diamond_coord = axial / spacing;
	float cell = fract(diamond_coord) - 0.5;
	float diamond_shape = clamp(1.0 - 2.8 * abs(cell), 0.0, 1.0);
	diamond_shape = pow(diamond_shape, 2.0);

	/* Shock diamonds are concentrated inside the core */
	float diamond_radial = pow(NdotV, 2.5);

	/* Falloff as shock diamonds dissipate down the plume */
	float diamond_decay = pow(clamp(1.0 - axial / max_len, 0.0, 1.0), 1.2);
	float shock = diamond_shape * diamond_radial * diamond_decay * u_DiamondIntensity;

	/* Warm incandescent nozzle glow at the engine exhaust rim */
	float nozzle_glow = clamp(1.0 - axial * 14.0, 0.0, 1.0);
	nozzle_glow = pow(nozzle_glow, 2.2);
	vec3 nozzle_color = vec3(1.0, 0.65, 0.35);

	/* Per-frame procedural supersonic noise shimmer */
	float ang_rad = angular * 6.2831853;
	vec2 np = vec2(axial * 25.0 - u_NoiseSeed * 35.0, sin(ang_rad) * 3.5 + cos(ang_rad) * 3.5);
	float n = noise2d(np);
	float grain = hash21(vec2(axial * 120.0 + u_NoiseSeed * 67.3,
				  sin(ang_rad) * 50.0 + cos(ang_rad) * 40.0 + u_NoiseSeed * 91.1));
	float shimmer = 0.85 + 0.3 * (0.65 * n + 0.35 * grain);

	/* Saturated gas mantle in faction tint */
	vec3 mantle_color = u_TintColor;
	float mantle_brightness = (0.28 + 0.22 * optical_depth) * u_CoreBrightness * shimmer;
	vec3 mantle_emission = mantle_color * mantle_brightness;

	/* Shock diamond core: white-hot center blending into faction tint */
	float core_hot = pow(diamond_shape, 2.5) * pow(NdotV, 3.5) * diamond_decay;
	vec3 diamond_color = mix(u_TintColor, vec3(1.0, 1.0, 1.0), clamp(core_hot * 1.5, 0.0, 1.0));
	vec3 shock_emission = diamond_color * (shock * 1.3);

	/* Combined emission */
	vec3 emission = (mantle_emission + shock_emission + nozzle_color * nozzle_glow * 1.4) *
			axial_fade * edge_fade;

	/* Translucent alpha allowing background to show through the gas */
	float gas_alpha = (0.32 * optical_depth + 0.38 * shock + 0.5 * nozzle_glow) *
			  axial_fade * edge_fade;
	float alpha = clamp(gas_alpha, 0.0, 1.0);

	/* Premultiplied alpha for BLEND_FUNC(GL_ONE, GL_ONE_MINUS_SRC_ALPHA) */
	vec4 tonemapped = filmic_tonemap(vec4(max(emission, vec3(0.0)), 1.0));
	f_FragColor = vec4(tonemapped.rgb * alpha, alpha);
}
