
in vec3 v_Color;

out vec4 f_FragColor;

void main()
{
	f_FragColor = vec4(v_Color,1);
	f_FragColor = filmic_tonemap(f_FragColor);
}

