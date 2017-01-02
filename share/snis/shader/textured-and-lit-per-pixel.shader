
#define USE_SPECULAR 1

#if !defined(AMBIENT)
#define AMBIENT 0.1
#endif

#ifdef USE_CUBEMAP
	#define TEX_SAMPLER samplerCube
	#define TEX_READ textureCube
	#define UV_TYPE vec3
#else
	#define TEX_SAMPLER sampler2D
	#define TEX_READ texture2D
	#define UV_TYPE vec2
#endif

varying vec3 v_Position;
varying UV_TYPE v_TexCoord;
varying vec3 v_Normal;

#ifdef USE_SPECULAR
uniform float u_SpecularPower; /* 512 is a good value */
uniform float u_SpecularIntensity; /* between 0 and 1, 1 is very shiny, 0 is flat */
#endif

#ifdef USE_NORMAL_MAP
	varying vec4 v_Tangent;
#endif

#if defined(INCLUDE_VS)
	uniform mat4 u_MVPMatrix;
	uniform mat4 u_MVMatrix;
	uniform mat3 u_NormalMatrix;

	attribute vec4 a_Position;
	#if !defined(USE_CUBEMAP)
		attribute vec2 a_TexCoord;
	#endif
	attribute vec3 a_Normal;
	#ifdef USE_NORMAL_MAP
		attribute vec4 a_Tangent;
	#endif

	void main()
	{
		v_Normal = normalize(u_NormalMatrix * a_Normal);
		#ifdef USE_NORMAL_MAP
			v_Tangent = vec4(vec3(u_MVMatrix * vec4(a_Tangent.xyz, 0)), a_Tangent.w);
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
	uniform TEX_SAMPLER u_AlbedoTex;
	uniform vec3 u_LightPos;
	vec3 u_LightColor = vec3(1);
	uniform vec4 u_TintColor;

	#ifdef USE_NORMAL_MAP
		uniform TEX_SAMPLER u_NormalTex;
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
	#endif

	void main()
	{
		UV_TYPE uv = v_TexCoord;

		vec3 light_dir = normalize(u_LightPos - v_Position);

		#ifdef USE_NORMALMAP
			// construct bitagnet and tbn matrix to convert normal map
			vec3 bitangent = cross(v_Normal, v_Tangent.xyz) * v_Tangent.w;
			vec3 normal_map = TEX_READ(u_NormalTex, uv).rgb;
			vec3 normal = normalize(normal_map.x * v_Tangent.xyz +
				normal_map.y * bitangent + normal_map.z * v_Normal);
		#else
			vec3 normal = v_Normal;
		#endif

		// albedo from texture
		vec4 albedo = TEX_READ(u_AlbedoTex, uv);

		// diffuse is light dot normal
		float diffuse = max(AMBIENT, dot(normal, light_dir));

		// base diffuse color
		vec3 color = albedo.rgb * u_LightColor * diffuse;

		#ifdef USE_EMIT_MAP
			color = max(color, TEX_READ(u_EmitTex, uv).rgb);
		#endif
		#ifdef USE_SPECULAR
			// blinn phong half vector specular
			vec3 view_dir = normalize(-v_Position);
			vec3 half_dir = normalize(light_dir + view_dir);
			float n_dot_h = max(0, dot(normal, half_dir));
			float spec = pow(n_dot_h, u_SpecularPower);

			color += u_SpecularColor * u_SpecularIntensity * spec;
		#endif

		gl_FragColor = clamp(vec4(color, albedo.a), 0.0, 1.0);

		/* tint with alpha pre multiply */
		gl_FragColor.rgb *= u_TintColor.rgb;
		gl_FragColor *= u_TintColor.a;
	}
#endif

