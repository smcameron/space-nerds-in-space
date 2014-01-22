#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <GL/glew.h>

#include "shader.h"

static char *read_file(const char *filename)
{
	char *buffer = NULL;
	int string_size, read_size;
	FILE *handler = fopen(filename, "r");

	if (handler) {
		/* seek the last byte of the file */
		fseek(handler, 0, SEEK_END);
		/* offset from the first to the last byte, or in other words, filesize */
		string_size = ftell(handler);
		/* go back to the start of the file */
		rewind(handler);

		/* allocate a string that can hold it all */
		buffer = (char *) malloc(sizeof(char) * (string_size + 1));
		/* read it all in one operation */
		read_size = fread(buffer, sizeof(char), string_size, handler);
		/* fread doesnt set it so put a \0 in the last position
		   and buffer is now officialy a string */
		buffer[string_size] = '\0';

		if (string_size != read_size) {
			/* something went wrong, throw away the memory and set
			   the buffer to NULL */
			free(buffer);
			buffer = NULL;
		}
	}

	return buffer;
}

GLuint load_shaders(const char *vertex_file_path, const char *fragment_file_path)
{
	/* Create the shaders */
	GLuint vertexShaderID = glCreateShader(GL_VERTEX_SHADER);
	GLuint fragmentShaderID = glCreateShader(GL_FRAGMENT_SHADER);

	/* Read the Vertex Shader code from the file */
	char *vertexShaderCode = read_file(vertex_file_path);
	if (!vertexShaderCode) {
		printf("Can't to open vertex shader '%s'\n", vertex_file_path);
		return 0;
	}

	/* Read the Fragment Shader code from the file */
	char *fragmentShaderCode = read_file(fragment_file_path);
	if (!fragmentShaderCode) {
		printf("Can't to open fragment shader '%s'\n", fragment_file_path);
		free(vertexShaderCode);
		return 0;
	}

	GLint result = GL_FALSE;
	int infoLogLength;

	/* Compile Vertex Shader */
	printf("Compiling shader: %s\n", vertex_file_path);
	char const *vertexSourcePointer = vertexShaderCode;
	glShaderSource(vertexShaderID, 1, &vertexSourcePointer , NULL);
	glCompileShader(vertexShaderID);

	/* Check Vertex Shader */
	glGetShaderiv(vertexShaderID, GL_COMPILE_STATUS, &result);
	if (result == GL_FALSE) {
		glGetShaderiv(vertexShaderID, GL_INFO_LOG_LENGTH, &infoLogLength);
		if (infoLogLength > 0) {
			char *vertexShaderErrorMessage = malloc(infoLogLength+1);
			glGetShaderInfoLog(vertexShaderID, infoLogLength, NULL, vertexShaderErrorMessage);
			printf("Shader '%s' compile error: %s\n", vertex_file_path, vertexShaderErrorMessage);
			free(vertexShaderErrorMessage);
		}
	}

	/* Compile Fragment Shader */
	printf("Compiling shader : %s\n", fragment_file_path);
	char const *fragmentSourcePointer = fragmentShaderCode;
	glShaderSource(fragmentShaderID, 1, &fragmentSourcePointer , NULL);
	glCompileShader(fragmentShaderID);

	/* Check Fragment Shader */
	glGetShaderiv(fragmentShaderID, GL_COMPILE_STATUS, &result);
	if (result == GL_FALSE) {
		glGetShaderiv(fragmentShaderID, GL_INFO_LOG_LENGTH, &infoLogLength);
		if (infoLogLength > 0) {
			char *fragmentShaderErrorMessage = malloc(infoLogLength+1);
			glGetShaderInfoLog(fragmentShaderID, infoLogLength, NULL, fragmentShaderErrorMessage);
			printf("Shader '%s' compile error: %s\n", fragment_file_path, fragmentShaderErrorMessage);
			free(fragmentShaderErrorMessage);
		}
	}

	/* Link the program */
	printf("Linking program\n");
	GLuint programID = glCreateProgram();
	glAttachShader(programID, vertexShaderID);
	glAttachShader(programID, fragmentShaderID);
	glLinkProgram(programID);

	/* Check the program */
	glGetProgramiv(programID, GL_LINK_STATUS, &result);
	if (result == GL_FALSE) {
		glGetProgramiv(programID, GL_INFO_LOG_LENGTH, &infoLogLength);
		if (infoLogLength > 0) {
			char *programErrorMessage = malloc(infoLogLength+1);
			glGetProgramInfoLog(programID, infoLogLength, NULL, programErrorMessage);
			printf("Shader link error: %s\n", programErrorMessage);
			free(programErrorMessage);
		}
	}

	glDeleteShader(vertexShaderID);
	glDeleteShader(fragmentShaderID);

	free(vertexShaderCode);
	free(fragmentShaderCode);

	return programID;
}


