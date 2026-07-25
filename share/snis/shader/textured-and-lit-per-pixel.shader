
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

	/* Cascaded shadow map varyings and helpers come from csm.shader. */

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
			csm_set_shadow_coords(a_Position);
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
	/* Star-coloured lighting: white sunlight tinted toward the star's colour, and the
	 * absolute (complement-tinted) shaded colour that supersedes the old scalar u_Ambient
	 * in the combine below.  Defaults from the renderer (light = white, ambient =
	 * vec3(u_Ambient)) reproduce the untinted look. */
	uniform vec3 u_LightColor;
	uniform vec3 u_AmbientColor;
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

	/* Cascaded shadow map sampling comes from csm.shader. */

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

		// diffuse is light dot normal (shadowed direct term); the star-tinted sunlight and the
		// absolute complement-tinted ambient are combined per channel so the lit faces lean
		// toward the star colour and the shaded faces toward its complement.
		float direct = (1.0 - u_in_shade) * clamp(dot(normal, light_dir), 0.0, 1.0);

		// base diffuse color
		vec3 color = albedo.rgb * max(u_AmbientColor, u_LightColor * direct * shadow);

		#ifdef USE_EMIT_MAP
			color = max(color, u_EmitIntensity * texture(u_EmitTex, uv).rgb);
		#endif
		#ifdef USE_SPECULAR
			// blinn phong half vector specular
			vec3 view_dir = normalize(-v_Position);
			vec3 half_dir = normalize(light_dir + view_dir);
			float n_dot_h = max(0.0, clamp(dot(normal, half_dir), 0.0, 1.0));
			float spec = pow(n_dot_h, u_SpecularPower);

			// A specular highlight is a reflection of the light source, so it carries the
			// star-tinted light colour (not the surface albedo) -- keeps highlights coherent
			// with the diffuse tint while staying brighter than the body.
			color += u_LightColor * u_SpecularColor * u_SpecularIntensity * spec * shadow;
		#endif

		f_FragColor = clamp(vec4(color, albedo.a), 0.0, 1.0);

		/* tint with alpha pre multiply */
		f_FragColor.rgb *= u_TintColor.rgb;
		f_FragColor *= u_TintColor.a;
		f_FragColor = filmic_tonemap(f_FragColor);

		#ifdef USE_CSM
			f_FragColor = csm_debug_color(f_FragColor, csm_cascade, shadow);
		#endif
	}
#endif

