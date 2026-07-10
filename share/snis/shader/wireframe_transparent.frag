
uniform vec3 u_Color;      // Per-object color information we will pass in.

in float v_EyeDot;

out vec4 f_FragColor;

void main()
{
	if (v_EyeDot <= 0.0)
		discard;

	f_FragColor = vec4(u_Color, 1);
}

