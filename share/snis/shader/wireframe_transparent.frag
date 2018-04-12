
uniform vec3 u_Color;      // Per-object color information we will pass in.

in float v_EyeDot;

void main()
{
	if (v_EyeDot <= 0)
		discard;

	gl_FragColor = vec4(u_Color, 1);
}

