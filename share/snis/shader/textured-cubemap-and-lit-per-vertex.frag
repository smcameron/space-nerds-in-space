#version 120

uniform samplerCube myTexture;

varying vec4 v_TintColor;
varying vec3 v_TexCoord;

void main()
{
	gl_FragColor = v_TintColor * textureCube(myTexture, v_TexCoord);
}

