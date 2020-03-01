
uniform mat4 u_MVPMatrix;  // A constant representing the combined model/view/projection matrix.
uniform mat4 u_MVMatrix;   // A constant representing the combined model/view matrix.
uniform mat3 u_NormalMatrix;
uniform vec3 u_Color;      // Per-object color information we will pass in.
uniform float u_in_shade;  // 1.0 means in full shade.  0.0 means not in shade.
			   // This is used for macro shading of whole objects
			   // e.g. a ship shaded by a planet
uniform float u_Ambient;   // Ambient light, 0.1 is ok value

uniform vec3 u_LightPos;   // The position of the light in eye space.

attribute vec4 a_Position; // Per-vertex position information we will pass in.
attribute vec3 a_Normal;   // Per-vertex normal information we will pass in.

varying vec3 v_Color;      // This will be passed into the fragment shader.

void main()                // The entry point for our vertex shader.
{
	// Transform the vertex into eye space.
	vec3 modelViewVertex = vec3(u_MVMatrix * a_Position);

	// Transform the normal's orientation into eye space.
	vec3 modelViewNormal = normalize(u_NormalMatrix * a_Normal);

	// Get a lighting direction vector from the light to the vertex.
	vec3 lightVector = normalize(u_LightPos - modelViewVertex);

	// Calculate the dot product of the light vector and vertex normal. If the normal and light vector are
	// pointing in the same direction then it will get max illumination.
	float dot = (1 - u_in_shade) * dot(modelViewNormal, lightVector);

	// mimic the original snis software render lighting
	/* dot = (dot + 1.0) / 2.0; */

	// ambient
	float diffuse = max(dot, u_Ambient);

	// Multiply the color by the illumination level. It will be interpolated across the triangle.
	v_Color = u_Color * diffuse;

	// gl_Position is a special variable used to store the final position.
	// Multiply the vertex by the matrix to get the final point in normalized screen coordinates.
	gl_Position = u_MVPMatrix * a_Position;
}

