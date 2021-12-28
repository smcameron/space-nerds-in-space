#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>

#include <GL/glew.h>

#include "shader.h"
#include "stacktrace.h"
#include "string-utils.h"

static void print_glsl_code_context(char *code[], int code_fragments, int lineno, int charno)
{
	int i, j;
	int l = 1;
	int c = 0;
	int code_frag = 0;

	i = 0;
	while (1) {
		/* Replace tabs with spaces so the arrows work out. */
		if (code[code_frag][i] == '\t')
			code[code_frag][i] = ' ';

		if (code[code_frag][i] == '\0') {
			code_frag++;
			if (code_frag >= code_fragments)
				break;
			i = 0;
		}
		if (l >= lineno - 1 && l <= lineno + 1) {
			if (c == 1)
				fprintf(stderr, "%4d: ", l);
			fprintf(stderr, "%c", code[code_frag][i]);
			if (code[code_frag][i] == '\n' && l == lineno) {
				fprintf(stderr, "    : ");
				for (j = 0; j < charno - 1; j++)
					fprintf(stderr, ".");
				fprintf(stderr, "^\n");
			}
		}
		if (code[code_frag][i] == '\n') {
			c = 0;
			l++;
		}
		c++;
		if (l > lineno + 1)
			break;
		i++;
	}
}

static void print_glsl_compile_error(char *error_message, char *shadercode[], int nshader_parts)
{
	int rc;
	char *n, *e = error_message;
	int lineno, charno, len;
	char tmp[1024];
	int highest_error = -1;

	fprintf(stderr, "GLSL Error:\n");

	lineno = 0;
	while (e) {
		n = strstr(e, "\n");
		if (!n)
			break;
		len = n - e;
		if ((size_t) len > sizeof(tmp) - 1)
			len = sizeof(tmp) - 1;
		strlcpy(tmp, e, sizeof(tmp));
		tmp[len] = '\0';
		rc = sscanf(tmp, "%*d:%d(%d)", &lineno, &charno);
		if (rc == 2 && lineno > highest_error) {
			print_glsl_code_context(shadercode, nshader_parts, lineno, charno);
			highest_error = lineno;
		}
		fprintf(stderr, "%s\n", tmp);
		e = n + 1;
	}
}

static char *read_file(const char *shader_directory, const char *filename)
{
	char fname[PATH_MAX];
	char *buffer = NULL;
	int string_size, read_size;
	FILE *handler;

	if (shader_directory)
		snprintf(fname, sizeof(fname), "%s/%s", shader_directory, filename);
	else
		strcpy(fname, filename);

	handler = fopen(fname, "r");
	if (!handler)
		return NULL;

	/* seek the last byte of the file */
	fseek(handler, 0, SEEK_END);
	/* offset from the first to the last byte, or in other words, filesize */
	string_size = ftell(handler);
	/* go back to the start of the file */
	rewind(handler);

	/* allocate a string that can hold it all */
	buffer = (char *) malloc(sizeof(char) * (string_size + 1));
	/* read it all in one operation */
	/* TODO: rewrite this for better error handling */
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
	fclose(handler);
	return buffer;
}

GLuint load_concat_shaders(const char *shader_directory,
	const char *vertex_header, int nvertex_files, const char *vertex_file_path[],
	const char *fragment_header, int nfragment_files, const char *fragment_file_path[])
{
	GLuint program_id = 0;
	int i;
#define MAX_VERTEX_FILES 5
#define MAX_FRAGMENT_FILES 5

	if (nvertex_files > MAX_VERTEX_FILES) {
		fprintf(stderr, "%s:%d: too many vertex files %d > %d\n", __FILE__, __LINE__,
			nvertex_files, MAX_VERTEX_FILES);
		stacktrace("Too many vertex files");
		return 0;
	}
	if (nfragment_files > MAX_FRAGMENT_FILES) {
		fprintf(stderr, "%s:%d: too many fragment files %d > %d\n", __FILE__, __LINE__,
			nfragment_files, MAX_FRAGMENT_FILES);
		stacktrace("Too many fragment files");
		return 0;
	}

	/* Create the shaders */
	GLuint vertex_shader_id = glCreateShader(GL_VERTEX_SHADER);
	GLuint fragment_shader_id = glCreateShader(GL_FRAGMENT_SHADER);

	int nvertex_shader = 0;
	char *vertex_shader_filename[MAX_VERTEX_FILES + 1];
	GLchar *vertex_shader_code[MAX_VERTEX_FILES + 1];

	int nfragment_shader = 0;
	char *fragment_shader_filename[MAX_FRAGMENT_FILES + 1];
	GLchar *fragment_shader_code[MAX_FRAGMENT_FILES + 1];

	/* Build the Vertex Shader code */
	if (vertex_header) {
		vertex_shader_filename[nvertex_shader] = strdup("string_vertex_header");
		vertex_shader_code[nvertex_shader] = strdup(vertex_header);
		nvertex_shader++;
	}

	for (i = 0; i < nvertex_files; i++) {
		char *file_shader_code = read_file(shader_directory, vertex_file_path[i]);
		if (!file_shader_code) {
			fprintf(stderr, "Can't open vertex shader '%s'\n", vertex_file_path[i]);
			goto cleanup;
		}

		vertex_shader_filename[nvertex_shader] = strdup(vertex_file_path[i]);
		vertex_shader_code[nvertex_shader] = file_shader_code;
		nvertex_shader++;
	}

	/* Read the Fragment Shader code from the file */
	if (fragment_header) {
		fragment_shader_filename[nfragment_shader] = strdup("string_fragment_header");
		fragment_shader_code[nfragment_shader] = strdup(fragment_header);
		nfragment_shader++;
	}

	for (i = 0; i < nfragment_files; i++) {
		char *file_shader_code = read_file(shader_directory, fragment_file_path[i]);
		if (!file_shader_code) {
			fprintf(stderr, "Can't open fragment shader '%s'\n", fragment_file_path[i]);
			goto cleanup;
		}

		fragment_shader_filename[nfragment_shader] = strdup(fragment_file_path[i]);
		fragment_shader_code[nfragment_shader] = file_shader_code;
		nfragment_shader++;
	}

	GLint result = GL_FALSE;
	int info_log_length;

	/* Compile Vertex Shader */
	glShaderSource(vertex_shader_id, nvertex_shader, (const GLchar **)&vertex_shader_code[0], NULL);
	glCompileShader(vertex_shader_id);

	/* Check Vertex Shader */
	glGetShaderiv(vertex_shader_id, GL_COMPILE_STATUS, &result);
	if (result == GL_FALSE) {
		fprintf(stderr, "Vertex shader compile error in files:\n");
		for (i = 0; i < nvertex_shader; i++) {
			fprintf(stderr, "    %d: \"%s\"\n", i, vertex_shader_filename[i]);
		}
		glGetShaderiv(vertex_shader_id, GL_INFO_LOG_LENGTH, &info_log_length);
		if (info_log_length > 0) {
			GLchar *error_message;
			error_message = malloc(sizeof(GLchar) * info_log_length + 1);
			glGetShaderInfoLog(vertex_shader_id, info_log_length, NULL, error_message);
			print_glsl_compile_error(error_message, vertex_shader_code, nvertex_shader);
			free(error_message);
		} else {
			fprintf(stderr, "shader.c: error: unknown from glGetShaderiv\n");
		}
		goto cleanup;
	}

	/* Compile Fragment Shader */
	glShaderSource(fragment_shader_id, nfragment_shader, (const GLchar **)&fragment_shader_code[0], NULL);
	glCompileShader(fragment_shader_id);

	/* Check Fragment Shader */
	glGetShaderiv(fragment_shader_id, GL_COMPILE_STATUS, &result);
	if (result == GL_FALSE) {
		fprintf(stderr, "Fragment shader compile error in files:\n");
		for (i = 0; i < nfragment_shader; i++) {
			fprintf(stderr, "    %d: \"%s\"\n", i, fragment_shader_filename[i]);
		}
		glGetShaderiv(fragment_shader_id, GL_INFO_LOG_LENGTH, &info_log_length);
		if (info_log_length > 0) {
			char *error_message;
			error_message = malloc(sizeof(GLchar) * info_log_length + 1);
			glGetShaderInfoLog(fragment_shader_id, info_log_length, NULL, &error_message[0]);
			print_glsl_compile_error(error_message, fragment_shader_code, nfragment_shader);
			free(error_message);
		} else {
			fprintf(stderr, "shader.c: error: unknown from glGetShaderiv\n");
		}
		goto cleanup;
	}

	/* Link the program */
	program_id = glCreateProgram();
	glAttachShader(program_id, vertex_shader_id);
	glAttachShader(program_id, fragment_shader_id);
	glLinkProgram(program_id);

	/* Check the program */
	glGetProgramiv(program_id, GL_LINK_STATUS, &result);
	if (result == GL_FALSE) {
		fprintf(stderr, "Shader link error in files:\n");
		for (i = 0; i < nvertex_shader; i++) {
			fprintf(stderr, "    vert %d: \"%s\"\n", i, vertex_shader_filename[i]);
		}
		for (i = 0; i < nfragment_shader; i++) {
			fprintf(stderr, "    frag %d: \"%s\"\n", i, fragment_shader_filename[i]);
		}

		glGetProgramiv(program_id, GL_INFO_LOG_LENGTH, &info_log_length);
		if (info_log_length > 0) {
			GLchar *error_message;
			error_message = malloc(sizeof(GLchar) * info_log_length + 1);
			glGetProgramInfoLog(program_id, info_log_length, NULL, &error_message[0]);
			fprintf(stderr, "Error:\n%s\n", error_message);
			free(error_message);

			glDeleteProgram(program_id);
			program_id = 0;
		} else {
			fprintf(stderr, "shader.c: error: unknown from glGetProgramiv()\n");
		}
		goto cleanup;
	}

cleanup:
	glDeleteShader(vertex_shader_id);
	glDeleteShader(fragment_shader_id);

	for (i = 0; i < nvertex_shader; i++) {
		free(vertex_shader_filename[i]);
		free(vertex_shader_code[i]);
	}
	for (i = 0; i < nfragment_shader; i++) {
		free(fragment_shader_filename[i]);
		free(fragment_shader_code[i]);
	}

	return program_id;
}

GLuint load_shaders(const char *shader_directory,
		const char *vertex_file_path, const char *fragment_file_path,
		const char *defines)
{
	return load_concat_shaders(shader_directory, defines, 1, &vertex_file_path,
					defines, 1, &fragment_file_path);
}

