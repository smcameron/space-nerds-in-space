
uniform mat4 MVP;

in vec3 vertex;
out vec3 texCoord;

void main() {
	texCoord = vertex;
	gl_Position = MVP * vec4(vertex, 1.0);
}

