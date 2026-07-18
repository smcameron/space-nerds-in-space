
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
		out vec4 v_ShadowCoord;
		uniform mat4 u_ShadowMVP;
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
			v_ShadowCoord = u_ShadowMVP * a_Position;
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
		in vec4 v_ShadowCoord;
		uniform sampler2DShadow u_ShadowMap;
		uniform int u_ShadowMapEnabled; /* 0 when no shadow map is available this frame */
		uniform int u_ShadowDebug;      /* 1 to visualize the shadow factor */

		/* Returns 1.0 for fully lit, 0.0 for fully shadowed. */
		float csm_shadow_factor()
		{
			if (u_ShadowMapEnabled == 0)
				return 1.0;
			vec3 sc = v_ShadowCoord.xyz / v_ShadowCoord.w;
			sc = sc * 0.5 + 0.5;
			if (sc.x < 0.0 || sc.x > 1.0 || sc.y < 0.0 || sc.y > 1.0 || sc.z > 1.0)
				return 1.0;
			return texture(u_ShadowMap, sc);
		}
	#endif

	void main()
	{
		UV_TYPE uv = v_TexCoord;

		vec3 light_dir = normalize(u_LightPos - v_Position);

		#ifdef USE_CSM
			float shadow = csm_shadow_factor();
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
				/* Shadow map coverage: blue = outside the shadow map footprint,
				 * red/green gradient = the shadow map's [0,1] uv inside it. */
				vec3 sc = v_ShadowCoord.xyz / v_ShadowCoord.w;
				sc = sc * 0.5 + 0.5;
				if (u_ShadowMapEnabled == 0)
					f_FragColor = vec4(0.3, 0.3, 0.0, 1.0);
				else if (sc.x < 0.0 || sc.x > 1.0 || sc.y < 0.0 || sc.y > 1.0)
					f_FragColor = vec4(0.0, 0.0, 0.4, 1.0);
				else
					f_FragColor = vec4(sc.x, sc.y, 0.0, 1.0);
			}
		#endif
	}
#endif

