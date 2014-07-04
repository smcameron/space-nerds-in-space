#ifndef SHADER_H
#define SHADER_H

GLuint load_shaders(const char *shader_directory,
			const char *vertex_file_path, const char *fragment_file_path,
			const char *defines);

GLuint load_concat_shaders(const char *shader_directory,
	const char *vertex_header, int nvertex_files, const char *vertex_file_path[],
	const char *fragment_header, int nfragment_files, const char *fragment_file_path[]);

#endif
