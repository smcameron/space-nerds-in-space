#version 120

uniform sampler2D myTexture;

varying vec4 v_Color;
varying vec2 v_TexCoord;

void main()
{
	gl_FragColor = v_Color * texture2D(myTexture, v_TexCoord);
}

