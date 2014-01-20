#version 120

uniform sampler2D u_Texture;

varying vec2 v_TextureCoord;
varying vec4 v_TintColor;

void main()
{
	gl_FragColor = v_TintColor * texture2D(u_Texture, v_TextureCoord);
}

