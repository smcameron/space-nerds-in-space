
uniform sampler2D u_AlbedoTex;

in vec2 v_TexCoord;
in vec4 v_TintColor;

void main()
{
	gl_FragColor = v_TintColor * texture2D(u_AlbedoTex, v_TexCoord);
}

