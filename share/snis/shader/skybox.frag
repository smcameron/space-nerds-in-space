#version 120

uniform samplerCube texture;

varying vec3 texCoord;

void main (void) {
	gl_FragColor = textureCube(texture, texCoord);
}

