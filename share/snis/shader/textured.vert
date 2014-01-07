#version 120

uniform mat4 u_MVPMatrix;  // A constant representing the combined model/view/projection matrix.

attribute vec3 a_Position; // Per-vertex position information we will pass in.
attribute vec2 a_tex_coord; // Per-vertex texture coord we will pass in.

varying vec2 v_TexCoord;      // This will be passed into the fragment shader.

void main()                // The entry point for our vertex shader.
{
	v_TexCoord = a_tex_coord;
	gl_Position = u_MVPMatrix * vec4(a_Position, 1.0);
}

