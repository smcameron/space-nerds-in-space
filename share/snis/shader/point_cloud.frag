#version 120

uniform vec3 u_Color;      // Per-object color information we will pass in.

void main()
{
	gl_FragColor = vec4(u_Color, 1);
}

