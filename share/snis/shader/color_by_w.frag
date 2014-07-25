
varying vec3 v_Color;      // Per-vertex color information

void main()
{
	gl_FragColor = vec4(v_Color, 1);
}

