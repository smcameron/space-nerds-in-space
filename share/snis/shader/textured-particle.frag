
uniform sampler2D u_AlbedoTex;

in vec2 v_TexCoord;
in vec4 v_TintColor;
out vec4 f_FragColor;

void main()
{
	f_FragColor = filmic_tonemap(v_TintColor * texture(u_AlbedoTex, v_TexCoord));
}

