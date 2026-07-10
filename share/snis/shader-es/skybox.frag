
uniform samplerCube s_texture;

varying vec3 texCoord;

void main (void) {
	gl_FragColor = filmic_tonemap(textureCube(s_texture, texCoord));
}

