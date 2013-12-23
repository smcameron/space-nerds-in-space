#version 120

uniform mat4 MVP;

attribute vec3 vertex;

varying vec3 texCoord;

void main() {
	texCoord = vertex;
	gl_Position = MVP * vec4(vertex, 1.0);
}

