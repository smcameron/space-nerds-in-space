
uniform mat4 u_MVPMatrix;  // A constant representing the combined model/view/projection matrix.

attribute vec4 a_Position; // Per-vertex position information we will pass in.
attribute vec4 a_Color;    // Per-vertex color information we will pass in.

varying vec4 v_Color;      // Per-vertex color information we will pass down.

void main()
{
	v_Color = a_Color;
	gl_Position = u_MVPMatrix * a_Position;
	/* 2D points (dotted and "electric" lines) are drawn with this shader.
	 * OpenGL ES leaves the size of a point undefined unless the vertex
	 * shader writes it, and some GPUs then draw nothing at all.  Desktop
	 * GL takes it from glPointSize() instead, which defaults to 1. */
	gl_PointSize = 1.0;
}

