uniform vec3 u_TintColor;
uniform float u_CoreBrightness;
uniform float u_PlumeLength;
uniform float u_NoiseSeed;
uniform float u_DiamondSpacing;
uniform float u_DiamondIntensity;

varying vec2 v_TexCoord;
varying vec3 v_ViewPos;
varying vec3 v_Normal;
varying vec3 v_ViewAxis;

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
	vec3 A = normalize(v_ViewAxis);

	/* Camera view alignment with the plume axis:
	 * 0.0 = broadside (view perpendicular to plume)
	 * 1.0 = axial (view aligned along plume axis)
	 */
	float axial_align = abs(dot(A, V));

	/* Decompose V and N into transverse components (perpendicular to A) */
	vec3 V_perp = V - dot(V, A) * A;
	float V_perp_len = length(V_perp);
	vec3 V_perp_dir = V_perp_len > 0.001 ? (V_perp / V_perp_len) : vec3(0.0);

	vec3 N_perp = N - dot(N, A) * A;
	float N_perp_len = length(N_perp);
	vec3 N_perp_dir = N_perp_len > 0.001 ? (N_perp / N_perp_len) : N;

	/* Transverse profile across cylinder width:
	 * 1.0 at projected centerline of plume, 0.0 at lateral silhouette edges.
	 * In axial view (V_perp_len -> 0), entire cross-section is aligned with view.
	 */
	float transverse_NdotV = V_perp_len > 0.001 ? abs(dot(N_perp_dir, V_perp_dir)) : 1.0;

	/* Lateral edge fade softens silhouette edges in broadside view */
	float lateral_edge_fade = smoothstep(0.0, 0.35, transverse_NdotV);

	/* When looking along the plume axis, camera looks down the column;
	 * do not fade out the column when viewed end-on.
	 */
	float edge_fade = mix(lateral_edge_fade, 1.0, smoothstep(0.35, 0.85, axial_align));

	/* Cylinder optical depth increases towards central axis and when looking down column */
	float axial_depth_boost = 1.0 + 1.2 * pow(axial_align, 1.5);
	float optical_depth = mix(pow(transverse_NdotV, 0.6), 1.0, pow(axial_align, 2.0)) * axial_depth_boost;

	float axial = v_TexCoord.x;
	float angular = v_TexCoord.y;

	/* Dynamic tip flutter: rapid per-frame length jitter and turbulent flutter */
	float ang_rad = angular * 6.2831853;
	float tip_jitter = hash21(vec2(floor(u_NoiseSeed), 41.7));
	float tip_flutter = sin(ang_rad * 3.0 + u_NoiseSeed * 25.0) * 0.05 +
			    cos(ang_rad * 5.0 - u_NoiseSeed * 18.0) * 0.03;
	float max_len = max(u_PlumeLength, 0.05);
	float dynamic_len = max_len * (0.86 + 0.22 * tip_jitter + tip_flutter);
	float fade_start = dynamic_len * (0.55 + 0.12 * hash21(vec2(floor(u_NoiseSeed), 93.3)));
	float axial_fade = 1.0 - smoothstep(fade_start, dynamic_len, axial);
	axial_fade *= (1.0 - smoothstep(0.90, 1.0, axial));

	/* Shock diamonds (repeating Mach discs inside the core) */
	float spacing = max(u_DiamondSpacing, 0.04);
	float diamond_coord = axial / spacing;
	float cell = fract(diamond_coord) - 0.5;
	float diamond_shape = clamp(1.0 - 2.8 * abs(cell), 0.0, 1.0);
	diamond_shape = pow(diamond_shape, 2.0);

	/* Shock diamonds are concentrated inside the core */
	float diamond_radial = mix(pow(transverse_NdotV, 2.5), 1.0, pow(axial_align, 2.0));

	/* Falloff as shock diamonds dissipate down the plume */
	float diamond_decay = pow(clamp(1.0 - axial / max_len, 0.0, 1.0), 1.2);
	/* Per-frame procedural supersonic noise shimmer */
	ang_rad = angular * 6.2831853;
	vec2 np = vec2(axial * 30.0 - u_NoiseSeed * 45.0,
		       sin(ang_rad) * 4.0 + cos(ang_rad) * 4.0 + u_NoiseSeed * 10.0);
	float n = noise2d(np);
	float grain = hash21(vec2(axial * 150.0 + u_NoiseSeed * 73.1,
				  sin(ang_rad) * 60.0 + cos(ang_rad) * 60.0 + u_NoiseSeed * 91.3));
	float noise_mix = 0.62 * n + 0.38 * grain;
	float shimmer = 0.55 + 0.90 * noise_mix;

	/* Shock diamond turbulence shimmer */
	float shock_shimmer = 0.80 + 0.40 * grain;
	float shock = diamond_shape * diamond_radial * diamond_decay * u_DiamondIntensity * shock_shimmer;

	/* Warm incandescent nozzle glow at the engine exhaust rim */
	float nozzle_glow = clamp(1.0 - axial * 4.5, 0.0, 1.0);
	nozzle_glow = pow(nozzle_glow, 1.5);
	vec3 nozzle_color = vec3(1.0, 0.38, 0.06);

	/* Saturated gas mantle in faction tint with pronounced turbulence */
	vec3 mantle_color = u_TintColor;
	float mantle_brightness = (0.44 + 0.30 * optical_depth) * u_CoreBrightness * shimmer;
	vec3 mantle_emission = mantle_color * mantle_brightness;

	/* Shock diamond core: white-hot center blending into faction tint */
	float core_hot = pow(diamond_shape, 2.5) * diamond_radial * diamond_decay;
	vec3 diamond_color = mix(u_TintColor, vec3(1.0, 1.0, 1.0), clamp(core_hot * 1.5, 0.0, 1.0));
	vec3 shock_emission = diamond_color * (shock * 0.65);

	/* Warm incandescent combustion glow smoothly transitions into mantle color */
	vec3 base_glow = nozzle_color * (nozzle_glow * 1.6);
	vec3 blended_mantle = mix(mantle_emission, base_glow, nozzle_glow * 0.85);

	/* Combined emission */
	vec3 emission = (blended_mantle + shock_emission + nozzle_color * (nozzle_glow * 0.8)) *
			axial_fade * edge_fade;

	/* Translucent alpha modulated by gas turbulence density variations */
	float alpha_shimmer = 0.65 + 0.70 * noise_mix;
	float gas_alpha = (0.36 * optical_depth * alpha_shimmer + 0.25 * shock + 0.5 * nozzle_glow) *
			  axial_fade * edge_fade;
	float alpha = clamp(gas_alpha, 0.0, 1.0);

	/* Premultiplied alpha for BLEND_FUNC(GL_ONE, GL_ONE_MINUS_SRC_ALPHA) */
	vec4 tonemapped = filmic_tonemap(vec4(max(emission, vec3(0.0)), 1.0));
	gl_FragColor = vec4(tonemapped.rgb * alpha, alpha);
}
