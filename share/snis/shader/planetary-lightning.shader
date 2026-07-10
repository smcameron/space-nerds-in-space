

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
	uniform vec2 u_u1v1;
	uniform float u_width;

	out vec4 f_FragColor;

	void main()
	{
		f_FragColor = texture(u_AlbedoTex, v_TexCoord);
		vec2 center = vec2(u_u1v1.x + 0.5f * u_width, u_u1v1.y + 0.5f * u_width);
		float d = (0.5f * u_width - length(v_TexCoord - center)) / (0.5 * u_width);
		f_FragColor.rgb *= v_TintColor.rgb;
		f_FragColor *= v_TintColor.a;
		f_FragColor.rgb *= 0.5 * vec3(d, d, d);
		f_FragColor.a *= 0.3 * d;
	}
#endif

