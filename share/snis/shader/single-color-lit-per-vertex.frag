
varying vec3 v_Color;

void main()
{
	gl_FragColor = vec4(v_Color,1);
	gl_FragColor = filmic_tonemap(gl_FragColor);
}

