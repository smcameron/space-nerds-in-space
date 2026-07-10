
uniform vec4 u_Color;      // Per-object color information we will pass in.
out vec4 f_FragColor;

void main()
{
	f_FragColor = u_Color;
}

