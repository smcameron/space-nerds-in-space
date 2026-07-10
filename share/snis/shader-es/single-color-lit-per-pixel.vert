
uniform mat4 u_MVPMatrix;      // A constant representing the combined model/view/projection matrix.
uniform mat4 u_MVMatrix;       // A constant representing the combined model/view matrix.
uniform mat3 u_NormalMatrix;
uniform vec3 u_Color;          // Per-object color information we will pass in.
uniform int u_in_shade;

attribute vec4 a_Position;     // Per-vertex position information we will pass in.
attribute vec3 a_Normal;       // Per-vertex normal information we will pass in.

varying vec3 v_Position;       // This will be passed into the fragment shader.
varying vec3 v_Color;          // This will be passed into the fragment shader.
varying vec3 v_Normal;         // This will be passed into the fragment shader.

// The entry point for our vertex shader.
void main()
{
	// Transform the vertex into eye space.
	v_Position = vec3(u_MVMatrix * a_Position);

	// Pass through the color.
	v_Color = u_Color;

	// Transform the normal's orientation into eye space.
	v_Normal = normalize(u_NormalMatrix * a_Normal);

	// gl_Position is a special variable used to store the final position.
	// Multiply the vertex by the matrix to get the final point in normalized screen coordinates.
	gl_Position = u_MVPMatrix * a_Position;
}

