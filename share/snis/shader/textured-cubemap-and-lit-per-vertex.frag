#version 120

uniform samplerCube myTexture;

varying vec4 v_Color;
varying vec3 v_TexCoord;

void main()
{
	gl_FragColor = v_Color * textureCube(myTexture, v_TexCoord);
}

