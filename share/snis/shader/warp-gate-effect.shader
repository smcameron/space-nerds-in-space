

#if defined(INCLUDE_VS)
	varying vec4 v_TintColor;
	varying vec2 v_TexCoord;      // This will be passed into the fragment shader.

	uniform mat4 u_MVPMatrix;  // A constant representing the combined model/view/projection matrix.
	uniform vec4 u_TintColor;

	attribute vec3 a_Position; // Per-vertex position information we will pass in.
	attribute vec2 a_TexCoord; // Per-vertex texture coord we will pass in.
	uniform vec2 u_u1v1;

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
	uniform vec2 u_u1v1;

	float map(in float x, float min1, float max1, float min2, float max2)
	{
		return min2 + (x - min1) * (max2 - min2) / (max1 - min1);
	}

	void main()
	{
		vec2 texcoord = vec2(map(v_TexCoord.x, 0.0, 1.0, u_u1v1.x, u_u1v1.y), v_TexCoord.y);
		gl_FragColor = texture2D(u_AlbedoTex, texcoord);
		/* tint with alpha pre multiply */
		gl_FragColor.rgb *= v_TintColor.rgb;
		gl_FragColor *= v_TintColor.a;
		gl_FragColor = filmic_tonemap(gl_FragColor);
		gl_FragColor *= 0.25 * (1.0 + sin(10 * 3.1415927 * (abs(u_u1v1.x - 0.5) + 0.5) * v_TexCoord.x)) +
				0.25 * (1.0 + cos(5 * u_u1v1.y * 0.1 * v_TexCoord.y));
	}
#endif

