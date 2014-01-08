#version 120

uniform sampler2D myTexture;

varying vec4 v_TintColor;
varying vec2 v_TexCoord;

void main()
{
	gl_FragColor = v_TintColor * texture2D(myTexture, v_TexCoord);
}

