#version 120

uniform samplerCube texture;

varying vec3 texCoord;

void main (void) {
	gl_FragColor = textureCube(texture, texCoord);
	//gl_FragColor = vec4(1,0,0,1);
}

