#version 120

uniform samplerCube myTexture;

varying vec3 v_Color;
varying vec3 v_TexCoord;

void main()
{
	gl_FragColor = vec4(v_Color, 1.0) * textureCube(myTexture, v_TexCoord);
}

