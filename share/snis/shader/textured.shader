

#if defined(INCLUDE_VS)
	out vec4 v_TintColor;
	out vec2 v_TexCoord;      // This will be passed into the fragment shader.

	uniform mat4 u_MVPMatrix;  // A constant representing the combined model/view/projection matrix.
	uniform vec4 u_TintColor;

	in vec3 a_Position; // Per-vertex position information we will pass in.
	in vec2 a_TexCoord; // Per-vertex texture coord we will pass in.

	void main()
	{
		v_TintColor = u_TintColor;
		v_TexCoord = a_TexCoord;
		gl_Position = u_MVPMatrix * vec4(a_Position, 1.0);
	}
#endif

#if defined(INCLUDE_FS)
	in vec4 v_TintColor;
	in vec2 v_TexCoord;      // This will be passed into the fragment shader.
	uniform sampler2D u_AlbedoTex;
	out vec4 f_FragColor;

	void main()
	{
		f_FragColor = texture(u_AlbedoTex, v_TexCoord);

		/* tint with alpha pre multiply */
		f_FragColor.rgb *= v_TintColor.rgb;
		f_FragColor *= v_TintColor.a;
		f_FragColor = filmic_tonemap(f_FragColor);
	}
#endif

