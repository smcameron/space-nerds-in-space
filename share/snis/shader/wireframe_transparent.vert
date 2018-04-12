
uniform mat4 u_MVPMatrix;  // A constant representing the combined model/view/projection matrix.
uniform mat4 u_MVMatrix;   // A constant representing the combined model/view matrix.
uniform mat3 u_NormalMatrix;
uniform vec3 u_Color;      // Per-object color information we will pass in.

in vec4 a_Position; // Per-vertex position information we will pass in.
in vec3 a_Normal;   // Per-vertex normal information we will pass in.

out float v_EyeDot;    // This will be passed into the fragment shader.

void main()
{
	// Transform the vertex into eye space.
	vec3 modelViewVertex = vec3(u_MVMatrix * a_Position);

	// Transform the normal's orientation into eye space.
	vec3 modelViewNormal = u_NormalMatrix * a_Normal;

	// Get a lighting direction vector from the light to the vertex.
	vec3 eyeVector = normalize(-modelViewVertex);

	// Calculate the dot product of the light vector and vertex normal. If the normal and light vector are
	// pointing in the same direction then it will get max illumination.
	v_EyeDot = dot(modelViewNormal, eyeVector);

	// gl_Position is a special variable used to store the final position.
	// Multiply the vertex by the matrix to get the final point in normalized screen coordinates.
	gl_Position = u_MVPMatrix * a_Position;
}

