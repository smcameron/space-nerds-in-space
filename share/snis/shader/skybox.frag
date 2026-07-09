
uniform samplerCube s_texture;

in vec3 texCoord;

out vec4 f_FragColor;

void main (void) {
	f_FragColor = filmic_tonemap(texture(s_texture, texCoord));
}

