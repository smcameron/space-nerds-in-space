
varying vec3 v_TexCoord;
varying vec4 v_LightColor;

#if defined(INCLUDE_VS)
	uniform mat4 u_MVPMatrix;  // A constant representing the combined model/view/projection matrix.
	uniform mat4 u_MVMatrix;   // A constant representing the combined model/view matrix.
	uniform mat3 u_NormalMatrix;
	uniform vec3 u_LightPos;   // The position of the light in eye space.
	uniform vec4 u_TintColor;

	attribute vec4 a_Position; // Per-vertex position information we will pass in.
	attribute vec3 a_Normal;   // Per-vertex normal information we will pass in.

	void main()
	{
		// Transform the vertex into eye space.
		vec3 position = vec3(u_MVMatrix * a_Position);

		// Transform the normal's orientation into eye space.
		vec3 normal = normalize(u_NormalMatrix * a_Normal);

		// Get a lighting direction vector from the light to the vertex.
		vec3 light_dir = normalize(u_LightPos - position);

		// Calculate the dot product of the light vector and vertex normal. If the normal and light vector are
		// pointing in the same direction then it will get max illumination.
		float diffuse = max(0.1, dot(normal, light_dir));

		// Multiply the color by the illumination level. It will be interpolated across the triangle.
		v_LightColor = vec4(u_TintColor.rgb * diffuse, u_TintColor.a);
		v_TexCoord = a_Position.xyz;

		gl_Position = u_MVPMatrix * a_Position;
	}
#endif

#if defined(INCLUDE_FS)
	uniform samplerCube u_AlbedoTex;

	void main()
	{
		gl_FragColor = v_LightColor * textureCube(u_AlbedoTex, v_TexCoord);
	}
#endif

