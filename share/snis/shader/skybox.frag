
uniform samplerCube texture;

in vec3 texCoord;

void main (void) {
	gl_FragColor = textureCube(texture, texCoord);
}

