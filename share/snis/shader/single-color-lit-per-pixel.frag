
uniform vec3 u_LightPos;       // The position of the light in eye space.
uniform int u_in_shade;        // 0 means not full shade, 1 means full shade

varying vec3 v_Position;       // Interpolated position for this fragment.
varying vec3 v_Color;          // This is the color from the vertex shader interpolated across the triangle per fragment
varying vec3 v_Normal;         // Interpolated normal for this fragment.
uniform float u_Ambient;       // ambient light.  0.1 is an ok value

void main()
{
	// Get a lighting direction vector from the light to the vertex.
	vec3 lightVector = normalize(u_LightPos - v_Position);

	// Calculate the dot product of the light vector and vertex normal. If the normal and light vector are
	// pointing in the same direction then it will get max illumination.
	float dot = dot(v_Normal, lightVector);

	// mimic the original snis software render lighting
	dot = (1.0 - u_in_shade) * ((dot + 1.0) / 2.0);

	// ambient light
	float diffuse = max(dot, u_Ambient);

	// Multiply the color by the diffuse illumination level to get final output color.
	gl_FragColor = vec4(v_Color * diffuse, 1.0);
	gl_FragColor = filmic_tonemap(gl_FragColor);
}

