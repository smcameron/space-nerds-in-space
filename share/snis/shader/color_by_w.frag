
in vec3 v_Color;      // Per-vertex color information

out vec4 f_FragColor;

void main()
{
	f_FragColor = vec4(v_Color, 1);
}

