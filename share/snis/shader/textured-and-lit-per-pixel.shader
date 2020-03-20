
#define USE_SPECULAR 1

#ifdef USE_CUBEMAP
	#define TEX_SAMPLER samplerCube
	#define TEX_READ textureCube
	#define UV_TYPE vec3
#else
	#define TEX_SAMPLER sampler2D
	#define TEX_READ texture2D
	#define UV_TYPE vec2
#endif

#ifdef USE_SPECULAR
uniform float u_SpecularPower; /* 512 is a good value */
uniform float u_SpecularIntensity; /* between 0 and 1, 1 is very shiny, 0 is flat */
#endif

#if defined(INCLUDE_VS)
	varying vec3 v_Position;
	varying UV_TYPE v_TexCoord;
	varying vec3 v_Normal;

	#ifdef USE_NORMAL_MAP
		varying vec3 v_Tangent;
		varying vec3 v_BiTangent;
		varying mat3 tbn;
	#endif

	uniform mat4 u_MVPMatrix;
	uniform mat4 u_MVMatrix;
	uniform mat3 u_NormalMatrix;

	attribute vec4 a_Position;
	#if !defined(USE_CUBEMAP)
		attribute vec2 a_TexCoord;
	#endif
	attribute vec3 a_Normal;
	#ifdef USE_NORMAL_MAP
		attribute vec3 a_Tangent;
		attribute vec3 a_BiTangent;
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
		gl_Position = u_MVPMatrix * a_Position;
	}

#endif

#if defined(INCLUDE_FS)
	varying vec3 v_Position;
	varying UV_TYPE v_TexCoord;
	varying vec3 v_Normal;

	#ifdef USE_NORMAL_MAP
		varying vec3 v_Tangent;
		varying vec3 v_BiTangent;
		varying mat3 tbn;
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

	void main()
	{
		UV_TYPE uv = v_TexCoord;

		vec3 light_dir = normalize(u_LightPos - v_Position);

		#ifdef USE_NORMAL_MAP
			// Hmm, this still needs work.
			// vec3 normal = normalize(tbn * normalize(TEX_READ(u_NormalMapTex, uv).xyz * 2.0 - 1.0));
			vec3 normal = normalize(tbn * (TEX_READ(u_NormalMapTex, uv).xyz * 2.0 - 1.0));
			// vec3 normal = tbn * normalize(TEX_READ(u_NormalMapTex, uv).xyz);
		#else
			vec3 normal = v_Normal;
		#endif

		// albedo from texture
		vec4 albedo = TEX_READ(u_AlbedoTex, uv);

		// diffuse is light dot normal
		float diffuse = max(u_Ambient, (1.0 - u_in_shade) * clamp(dot(normal, light_dir), 0.0, 1.0));

		// base diffuse color
		vec3 color = albedo.rgb * u_LightColor * diffuse;

		#ifdef USE_EMIT_MAP
			color = max(color, u_EmitIntensity * TEX_READ(u_EmitTex, uv).rgb);
		#endif
		#ifdef USE_SPECULAR
			// blinn phong half vector specular
			vec3 view_dir = normalize(-v_Position);
			vec3 half_dir = normalize(light_dir + view_dir);
			float n_dot_h = max(0, clamp(dot(normal, half_dir), 0.0, 1.0));
			float spec = pow(n_dot_h, u_SpecularPower);

			color += u_SpecularColor * u_SpecularIntensity * spec;
		#endif

		gl_FragColor = clamp(vec4(color, albedo.a), 0.0, 1.0);

		/* tint with alpha pre multiply */
		gl_FragColor.rgb *= u_TintColor.rgb;
		gl_FragColor *= u_TintColor.a;
		gl_FragColor = filmic_tonemap(gl_FragColor);
	}
#endif

