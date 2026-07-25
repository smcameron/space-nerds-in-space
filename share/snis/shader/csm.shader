/*
	Cascaded shadow map sampling, shared by every lit shader that receives shadows.

	This file is concatenated ahead of the shader that uses it (see load_concat_shaders()),
	so its declarations and functions are in scope by the time that shader's main() runs.
	Everything is behind USE_CSM, so concatenating it into a shader built without that
	define costs nothing.

	The header injected by graph_dev_opengl.c supplies MAX_SHADOW_CASCADES, SHADOW_MAP_SIZE
	and CSM_PCF_MAX_RADIUS (see SHADOW_CASCADES_HEADER), so C and GLSL agree on the array
	sizes and the PCF loop bound.

	A consuming vertex shader calls csm_set_shadow_coords(a_Position) from its main().
	A consuming fragment shader calls csm_shadow_factor(cascade, -v_Position.z) to get a
	0..1 lit factor, multiplies its direct lighting term by it, and may finish with
	csm_debug_color() to honour the shadow debug views.
*/

#ifdef USE_CSM

#if defined(INCLUDE_VS)

	out vec4 v_ShadowCoord[MAX_SHADOW_CASCADES];
	uniform mat4 u_ShadowMVP[MAX_SHADOW_CASCADES];
	uniform int u_NumCascades;

	/* Project a model-space vertex into every active cascade's light clip space. */
	void csm_set_shadow_coords(vec4 model_position)
	{
		for (int csm_i = 0; csm_i < u_NumCascades; csm_i++)
			v_ShadowCoord[csm_i] = u_ShadowMVP[csm_i] * model_position;
	}

#endif

#if defined(INCLUDE_FS)

	in vec4 v_ShadowCoord[MAX_SHADOW_CASCADES];
	uniform sampler2DArrayShadow u_ShadowMap;
	uniform int u_NumCascades;
	uniform int u_ShadowMapEnabled; /* 0 when no shadow map is available this frame */
	uniform int u_ShadowDebug;      /* 1 = shadow factor, 2 = cascade index */
	uniform int u_ShadowPcfRadius;  /* PCF kernel half-width for the nearest cascade */
	uniform float u_CascadeSplitFar[MAX_SHADOW_CASCADES]; /* view-space far distance per cascade */
	uniform float u_ShadowBlend;    /* cross-cascade blend band, fraction of far distance */

	/* Percentage-closer filter: average the depth comparison over a (2r+1)^2 grid of
	 * shadow texels.  Loop bounds are the compile-time maximum so they stay constant;
	 * taps outside the requested radius are skipped. */
	float csm_pcf(int cascade, vec3 sc, int radius)
	{
		if (radius <= 0)
			return texture(u_ShadowMap, vec4(sc.xy, float(cascade), sc.z));
		float texel = 1.0 / float(SHADOW_MAP_SIZE);
		float sum = 0.0;
		float count = 0.0;
		for (int y = -CSM_PCF_MAX_RADIUS; y <= CSM_PCF_MAX_RADIUS; y++) {
			if (y < -radius || y > radius)
				continue;
			for (int x = -CSM_PCF_MAX_RADIUS; x <= CSM_PCF_MAX_RADIUS; x++) {
				if (x < -radius || x > radius)
					continue;
				vec2 off = vec2(float(x), float(y)) * texel;
				sum += texture(u_ShadowMap, vec4(sc.xy + off, float(cascade), sc.z));
				count += 1.0;
			}
		}
		return sum / count;
	}

	/* Sample one cascade's lit factor (1.0 if outside its footprint).  The PCF radius
	 * tapers by one per cascade so the world-space penumbra stays ~constant. */
	float csm_sample(int cascade)
	{
		vec3 sc = v_ShadowCoord[cascade].xyz / v_ShadowCoord[cascade].w;
		sc = sc * 0.5 + 0.5;
		if (sc.x < 0.0 || sc.x > 1.0 || sc.y < 0.0 || sc.y > 1.0 || sc.z > 1.0)
			return 1.0;
		int radius = u_ShadowPcfRadius - cascade;
		if (radius < 0)
			radius = 0;
		if (radius > CSM_PCF_MAX_RADIUS)
			radius = CSM_PCF_MAX_RADIUS;
		return csm_pcf(cascade, sc, radius);
	}

	/* Depth-based cascade selection with a blend band near each split (and a soft fade
	 * to lit at the outer coverage edge).  1.0 = lit, 0.0 = shadowed; reports the
	 * primary cascade.  view_depth is the positive view-space distance. */
	float csm_shadow_factor(out int cascade_used, float view_depth)
	{
		cascade_used = -1;
		if (u_ShadowMapEnabled == 0)
			return 1.0;
		for (int k = 0; k < u_NumCascades; k++) {
			if (view_depth <= u_CascadeSplitFar[k]) {
				cascade_used = k;
				float f = csm_sample(k);
				if (u_ShadowBlend > 0.0) {
					float band = u_ShadowBlend * u_CascadeSplitFar[k];
					float edge = u_CascadeSplitFar[k] - band;
					if (view_depth > edge) {
						float t = (view_depth - edge) / band;
						float next = (k + 1 < u_NumCascades) ? csm_sample(k + 1) : 1.0;
						f = mix(f, next, t);
					}
				}
				return f;
			}
		}
		return 1.0;
	}

	/* Replace the shaded colour with a shadow debug visualization, if one is selected.
	 * Mode 1 is the raw shadow factor; mode 2 tints by cascade index:
	 * red=0 (nearest) green=1 blue=2 yellow=3 cyan=4 magenta=5 gray = outside all
	 * cascades, darkened where shadowed.  Returns color unchanged when debug is off. */
	vec4 csm_debug_color(vec4 color, int cascade, float shadow)
	{
		if (u_ShadowDebug == 1)
			return vec4(vec3(shadow), 1.0);
		if (u_ShadowDebug != 2)
			return color;

		vec3 cc;
		if (cascade == 0)
			cc = vec3(1.0, 0.3, 0.3);
		else if (cascade == 1)
			cc = vec3(0.3, 1.0, 0.3);
		else if (cascade == 2)
			cc = vec3(0.3, 0.3, 1.0);
		else if (cascade == 3)
			cc = vec3(1.0, 1.0, 0.3);
		else if (cascade == 4)
			cc = vec3(0.3, 1.0, 1.0);
		else if (cascade == 5)
			cc = vec3(1.0, 0.3, 1.0);
		else
			cc = vec3(0.15);
		return vec4(cc * max(shadow, 0.4), 1.0);
	}

#endif

#endif
