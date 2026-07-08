

#if defined(INCLUDE_VS)
	varying vec4 v_TintColor;
	varying vec2 v_TexCoord;      // This will be passed into the fragment shader.

	uniform mat4 u_MVPMatrix;  // A constant representing the combined model/view/projection matrix.
	uniform vec4 u_TintColor;

	attribute vec3 a_Position; // Per-vertex position information we will pass in.
	attribute vec2 a_TexCoord; // Per-vertex texture coord we will pass in.

	void main()
	{
		v_TintColor = u_TintColor;
		v_TexCoord = a_TexCoord;
		gl_Position = u_MVPMatrix * vec4(a_Position, 1.0);
	}
#endif

#if defined(INCLUDE_FS)
	varying vec4 v_TintColor;
	varying vec2 v_TexCoord;      // This will be passed into the fragment shader.
	uniform sampler2D u_AlbedoTex;

	void main()
	{
		gl_FragColor = texture2D(u_AlbedoTex, v_TexCoord);

		/* tint with alpha pre multiply */
		gl_FragColor.rgb *= v_TintColor.rgb;
		gl_FragColor *= v_TintColor.a;
		gl_FragColor = filmic_tonemap(gl_FragColor);
	}
#endif

