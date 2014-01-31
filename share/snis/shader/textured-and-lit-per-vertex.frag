#version 120

uniform sampler2D myTexture;
uniform vec4 u_TintColor;

varying vec4 v_Color;
varying vec2 v_TexCoord;

void main()
{
	gl_FragColor = u_TintColor * v_Color * texture2D(myTexture, v_TexCoord);
}

