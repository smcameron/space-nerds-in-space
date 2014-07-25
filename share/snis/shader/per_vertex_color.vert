
uniform mat4 u_MVPMatrix;  // A constant representing the combined model/view/projection matrix.

attribute vec4 a_Position; // Per-vertex position information we will pass in.
attribute vec4 a_Color;    // Per-vertex color information we will pass in.

varying vec4 v_Color;      // Per-vertex color information we will pass down.

void main()
{
	v_Color = a_Color;
	gl_Position = u_MVPMatrix * a_Position;
}

