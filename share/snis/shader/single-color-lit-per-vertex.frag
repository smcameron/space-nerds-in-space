
in vec3 v_Color;

out vec4 f_FragColor;

#ifdef USE_CSM
uniform sampler2DArrayShadow u_ShadowMap;
uniform int u_NumCascades;
uniform int u_ShadowMapEnabled; /* 0 when no shadow map is available this frame */
uniform int u_ShadowDebug;      /* 1 = shadow factor, 2 = cascade index */
uniform int u_ShadowPcfRadius;  /* PCF kernel half-width for the nearest cascade */
uniform float u_CascadeSplitFar[MAX_SHADOW_CASCADES]; /* view-space far distance per cascade */
uniform float u_ShadowBlend;    /* cross-cascade blend band, fraction of far distance */
uniform float u_Ambient;
in vec4 v_ShadowCoord[MAX_SHADOW_CASCADES];
in float v_LightLevel;
in vec3 v_BaseColor;
in float v_ViewDepth;           /* positive view-space distance from the camera */

/* Percentage-closer filter: average the depth comparison over a (2r+1)^2 grid of shadow
 * texels.  The loop bounds are the compile-time maximum so they stay constant; taps
 * outside the requested radius are skipped. */
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

/* Sample one cascade's lit factor (1.0 if the fragment falls outside its footprint).  The
 * PCF radius tapers by one per cascade so the world-space penumbra stays ~constant: the
 * near cascade (small texels) gets the widest kernel, far cascades (big texels) a narrow
 * one. */
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

/* Depth-based cascade selection with a blend band near each split (and a soft fade to lit
 * at the outer coverage edge).  1.0 = lit, 0.0 = shadowed; reports the primary cascade. */
float csm_shadow_factor(out int cascade_used)
{
	cascade_used = -1;
	if (u_ShadowMapEnabled == 0)
		return 1.0;
	for (int k = 0; k < u_NumCascades; k++) {
		if (v_ViewDepth <= u_CascadeSplitFar[k]) {
			cascade_used = k;
			float f = csm_sample(k);
			if (u_ShadowBlend > 0.0) {
				float band = u_ShadowBlend * u_CascadeSplitFar[k];
				float edge = u_CascadeSplitFar[k] - band;
				if (v_ViewDepth > edge) {
					float t = (v_ViewDepth - edge) / band;
					float next = (k + 1 < u_NumCascades) ? csm_sample(k + 1) : 1.0;
					f = mix(f, next, t);
				}
			}
			return f;
		}
	}
	return 1.0;
}
#endif

void main()
{
#ifdef USE_CSM
	int csm_cascade;
	float lit = csm_shadow_factor(csm_cascade);
	float diffuse = max(v_LightLevel * lit, u_Ambient);
	f_FragColor = vec4(v_BaseColor * diffuse, 1.0);
#else
	f_FragColor = vec4(v_Color, 1);
#endif
	f_FragColor = filmic_tonemap(f_FragColor);

#ifdef USE_CSM
	if (u_ShadowDebug == 1) {
		/* Shadow factor: white = lit, black = shadowed. */
		f_FragColor = vec4(vec3(lit), 1.0);
	} else if (u_ShadowDebug == 2) {
		/* Cascade index: red=0 (nearest), green=1, blue=2, yellow=3,
		 * gray = outside all cascades.  Darkened where shadowed. */
		vec3 cc;
		if (csm_cascade == 0)
			cc = vec3(1.0, 0.3, 0.3);
		else if (csm_cascade == 1)
			cc = vec3(0.3, 1.0, 0.3);
		else if (csm_cascade == 2)
			cc = vec3(0.3, 0.3, 1.0);
		else if (csm_cascade == 3)
			cc = vec3(1.0, 1.0, 0.3);
		else
			cc = vec3(0.15);
		f_FragColor = vec4(cc * max(lit, 0.4), 1.0);
	}
#endif
}

