
#define USE_SPECULAR 1

#ifdef USE_CUBEMAP
	#define TEX_SAMPLER samplerCube
	#define UV_TYPE vec3
#else
	#define TEX_SAMPLER sampler2D
	#define UV_TYPE vec2
#endif

#ifdef USE_SPECULAR
uniform float u_SpecularPower; /* 512 is a good value */
uniform float u_SpecularIntensity; /* between 0 and 1, 1 is very shiny, 0 is flat */
#endif

#if defined(INCLUDE_VS)
	out vec3 v_Position;
	out UV_TYPE v_TexCoord;
	out vec3 v_Normal;

	#ifdef USE_NORMAL_MAP
		out vec3 v_Tangent;
		out vec3 v_BiTangent;
		out mat3 tbn;
	#endif

	uniform mat4 u_MVPMatrix;
	uniform mat4 u_MVMatrix;
	uniform mat3 u_NormalMatrix;

	#ifdef USE_CSM
		out vec4 v_ShadowCoord[MAX_SHADOW_CASCADES];
		uniform mat4 u_ShadowMVP[MAX_SHADOW_CASCADES];
		uniform int u_NumCascades;
	#endif

	in vec4 a_Position;
	#if !defined(USE_CUBEMAP)
		in vec2 a_TexCoord;
	#endif
	in vec3 a_Normal;
	#ifdef USE_NORMAL_MAP
		in vec3 a_Tangent;
		in vec3 a_BiTangent;
	#endif

	void main()
	{
		v_Normal = normalize(u_NormalMatrix * a_Normal);
		#ifdef USE_NORMAL_MAP
			v_Tangent = normalize(u_NormalMatrix * a_Tangent);
			v_BiTangent = normalize(u_NormalMatrix * a_BiTangent);
			tbn = mat3(v_Tangent, v_BiTangent, v_Normal);
		#endif

		v_Position = vec3(u_MVMatrix * a_Position);
		#ifdef USE_CUBEMAP
			v_TexCoord = a_Position;
		#else
			v_TexCoord = a_TexCoord;
		#endif
		#ifdef USE_CSM
			for (int csm_i = 0; csm_i < u_NumCascades; csm_i++)
				v_ShadowCoord[csm_i] = u_ShadowMVP[csm_i] * a_Position;
		#endif
		gl_Position = u_MVPMatrix * a_Position;
	}

#endif

#if defined(INCLUDE_FS)
	in vec3 v_Position;
	in UV_TYPE v_TexCoord;
	in vec3 v_Normal;

	#ifdef USE_NORMAL_MAP
		in vec3 v_Tangent;
		in vec3 v_BiTangent;
		in mat3 tbn;
	#endif

	uniform TEX_SAMPLER u_AlbedoTex;
	uniform vec3 u_LightPos;
	vec3 u_LightColor = vec3(1);
	uniform vec4 u_TintColor;
	uniform float u_in_shade;
	uniform float u_Ambient;

	#ifdef USE_NORMAL_MAP
		uniform TEX_SAMPLER u_NormalMapTex;
	#endif
	#if defined(USE_SPECULAR) || defined(USE_SPECULAR_MAP)
		#ifdef USE_SPECULAR_MAP
			uniform TEX_SAMPLER u_SpecularTex;
		#else
			vec3 u_SpecularColor = vec3(1);
		#endif
	#endif
	#ifdef USE_EMIT_MAP
		uniform TEX_SAMPLER u_EmitTex;
		uniform float u_EmitIntensity;
	#endif

	out vec4 f_FragColor;

	#ifdef USE_CSM
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
	#endif

	void main()
	{
		UV_TYPE uv = v_TexCoord;

		vec3 light_dir = normalize(u_LightPos - v_Position);

		#ifdef USE_CSM
			int csm_cascade;
			/* v_Position is eye-space; the view-space distance is -z. */
			float shadow = csm_shadow_factor(csm_cascade, -v_Position.z);
		#else
			float shadow = 1.0;
		#endif

		#ifdef USE_NORMAL_MAP
			// Hmm, this still needs work.
			// vec3 normal = normalize(tbn * normalize(texture(u_NormalMapTex, uv).xyz * 2.0 - 1.0));
			vec3 normal = normalize(tbn * (texture(u_NormalMapTex, uv).xyz * 2.0 - 1.0));
			// vec3 normal = tbn * normalize(texture(u_NormalMapTex, uv).xyz);
		#else
			vec3 normal = v_Normal;
		#endif

		// albedo from texture
		vec4 albedo = texture(u_AlbedoTex, uv);

		// diffuse is light dot normal (shadowed direct term with an ambient floor)
		float direct = (1.0 - u_in_shade) * clamp(dot(normal, light_dir), 0.0, 1.0);
		float diffuse = max(u_Ambient, direct * shadow);

		// base diffuse color
		vec3 color = albedo.rgb * u_LightColor * diffuse;

		#ifdef USE_EMIT_MAP
			color = max(color, u_EmitIntensity * texture(u_EmitTex, uv).rgb);
		#endif
		#ifdef USE_SPECULAR
			// blinn phong half vector specular
			vec3 view_dir = normalize(-v_Position);
			vec3 half_dir = normalize(light_dir + view_dir);
			float n_dot_h = max(0.0, clamp(dot(normal, half_dir), 0.0, 1.0));
			float spec = pow(n_dot_h, u_SpecularPower);

			color += u_SpecularColor * u_SpecularIntensity * spec * shadow;
		#endif

		f_FragColor = clamp(vec4(color, albedo.a), 0.0, 1.0);

		/* tint with alpha pre multiply */
		f_FragColor.rgb *= u_TintColor.rgb;
		f_FragColor *= u_TintColor.a;
		f_FragColor = filmic_tonemap(f_FragColor);

		#ifdef USE_CSM
			if (u_ShadowDebug == 1) {
				/* Shadow factor: white = lit, black = shadowed. */
				f_FragColor = vec4(vec3(shadow), 1.0);
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
				f_FragColor = vec4(cc * max(shadow, 0.4), 1.0);
			}
		#endif
	}
#endif

