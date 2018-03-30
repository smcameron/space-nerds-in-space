
varying vec4 v_TintColor;
#if defined(TEXTURED_ALPHA_BY_NORMAL)
varying vec2 v_TexCoord;      // This will be passed into the fragment shader.
#endif

uniform float u_Invert;

#if defined(INCLUDE_VS)
	uniform mat4 u_MVPMatrix;  // A constant representing the combined model/view/projection matrix.
	uniform mat4 u_MVMatrix;
	uniform vec4 u_TintColor;
	uniform mat3 u_NormalMatrix;
	varying float v_EyeDot;

	attribute vec4 a_Position; // Per-vertex position information we will pass in.
	attribute vec3 a_Normal;
#if defined(TEXTURED_ALPHA_BY_NORMAL)
	attribute vec2 a_TexCoord; // Per-vertex texture coord we will pass in.
#endif
	void main()
	{
		// Transform the vertex into eye space.
		vec3 modelViewVertex = vec3(u_MVMatrix * a_Position);

		// Transform the normal's orientation into eye space.
		vec3 modelViewNormal = normalize(u_NormalMatrix * a_Normal);

		vec3 eyeVector = normalize(-modelViewVertex);
		v_EyeDot = dot(modelViewNormal, eyeVector);

		v_TintColor = u_TintColor;
#if defined(TEXTURED_ALPHA_BY_NORMAL)
		v_TexCoord = a_TexCoord;
#endif
		gl_Position = u_MVPMatrix * a_Position;
	}
#endif

#if defined(INCLUDE_FS)
#if defined(TEXTURED_ALPHA_BY_NORMAL)
	uniform sampler2D u_AlbedoTex;
#endif
	varying float v_EyeDot;

	void main()
	{
#if defined(TEXTURED_ALPHA_BY_NORMAL)
		gl_FragColor = texture2D(u_AlbedoTex, v_TexCoord);
#else
		gl_FragColor = vec4(1.0, 1.0, 1.0, 1.0);
#endif
		gl_FragColor.rgb = vec3(1.0, 1.0, 1.0);
		gl_FragColor.rgb *= v_TintColor.rgb; /* tint with alpha pre multiply */
		/* This max/min gets rid of back faces (maps negatives to 0.0)) */
		float factor = max(min(sign(v_EyeDot), v_EyeDot * v_EyeDot * v_EyeDot), 0.0);
		float alpha = v_TintColor.a * (factor * u_Invert +
			(1.0 - u_Invert) * (1.0 - factor));
		gl_FragColor *= alpha;
		// gl_FragColor *= v_TintColor.a * (v_EyeDot * (1.0 - u_Invert) + u_Invert * (1.0 - v_EyeDot));
		// gl_FragColor *= v_TintColor.a * max((1.0 - abs(v_EyeDot)), 0.2);
	}
#endif

