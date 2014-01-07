#version 120

uniform sampler2D myTexture;

varying vec2 v_TexCoord;

void main()
{
	gl_FragColor = texture2D(myTexture, v_TexCoord);
}

