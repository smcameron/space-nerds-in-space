
uniform mat4 u_MVPMatrix;  // A constant representing the combined model/view/projection matrix.
uniform float u_PointSize;

in vec4 a_Position; // Per-vertex position information we will pass in.

void main()
{
	gl_PointSize = u_PointSize;
	gl_Position = u_MVPMatrix * a_Position;
}

