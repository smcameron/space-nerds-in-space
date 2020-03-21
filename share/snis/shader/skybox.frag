
uniform samplerCube texture;

varying vec3 texCoord;

void main (void) {
	gl_FragColor = filmic_tonemap(textureCube(texture, texCoord));
}

