/*
	Copyright (C) 2026 Stephen M. Cameron
	Author: Stephen M. Cameron

	This file is part of Spacenerds In Space.

	Spacenerds in Space is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.

	Spacenerds in Space is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with Spacenerds in Space; if not, write to the Free Software
	Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/
#ifndef GRAPH_DEV_MESH_CACHE_H__
#define GRAPH_DEV_MESH_CACHE_H__

/* A mesh on the GPU: the vertex buffers, and the one function that fills them.
 *
 * Companion to graph_dev/texture_cache.c and extracted for the same reason.  This is resource
 * management, not rendering: it takes a struct mesh and hands back buffer objects, and every
 * call it makes -- glGenBuffers, glBindBuffer, glBufferData -- means the same thing in desktop
 * GL and in GLES.  It was 464 identical lines in each backend, and identical is the whole
 * argument: two copies of code that must not diverge, and nothing to stop them.
 *
 * The buffer LAYOUTS live here rather than in the .c because the backends' drawing code sets its
 * attribute pointers with offsetof into them -- about a hundred times each.  That is the seam:
 * this file decides what a mesh looks like in memory, and each backend decides what to do with
 * it, which differs because the two GL versions differ about shaders and vertex arrays.
 */

#include <stdint.h>

#include "quat.h"
#include "mesh.h"
#include "vertex.h"
#include "triangle.h"

struct vertex_buffer_data {
	union vec3 position;
};

struct vertex_triangle_buffer_data {
	union vec3 normal;
	union vec3 tvertex0;
	union vec3 tvertex1;
	union vec3 tvertex2;
	union vec3 wireframe_edge_mask;
	union vec2 texture_coord;
	union vec3 tangent;
	union vec3 bitangent;
	/* The vertex's w, which the renderers otherwise ignore: a broken ship's pieces carry
	 * each vertex's distance to the torn edge in it (see mesh_fracture.h), and a particle
	 * batch its particles' kind and seed (see particle_batch.h).  Their shaders read it as
	 * a_Edge; every other mesh has a value no shader reads. */
	float w;
};

struct vertex_color_buffer_data {
	GLfloat position[2];
	GLubyte color[4];
};

struct vertex_line_buffer_data {
	GLubyte multi_one[4];
	union vec3 line_vertex0;
	union vec3 line_vertex1;
};

struct vertex_particle_buffer_data {
	GLubyte multi_one[4];
	union vec3 start_position;
	GLubyte start_tint_color[3];
	GLubyte start_apm[2];
	union vec3 end_position;
	GLubyte end_tint_color[3];
	GLubyte end_apm[2];
};

struct vertex_wireframe_line_buffer_data {
	union vec3 position;
	union vec3 normal;
};

struct mesh_gl_info {
	/* common buffer to hold vertex positions */
	GLuint vertex_buffer;

	int ntriangles;
	/* uses vertex_buffer for data */
	GLuint triangle_vertex_buffer;

	GLuint triangle_normal_lines_buffer;
	GLuint triangle_tangent_lines_buffer;
	GLuint triangle_bitangent_lines_buffer;

	int nwireframe_lines;
	GLuint wireframe_lines_vertex_buffer;

	int npoints;
	/* uses vertex_buffer for data */

	int nlines;
	/* uses vertex_buffer for data */
	GLuint line_vertex_buffer;

	int nparticles;
	GLuint particle_vertex_buffer;
	GLuint particle_index_buffer;
};

/* load/reload an array buffer using stream draw if it is being overwritten */
#define LOAD_BUFFER(buffer_type, buffer_id, buffer_size, buffer_data) \
	do { \
		PROFILE_ZONE_START("LOAD_BUFFER"); \
		GLenum usage; \
		if ((buffer_id) == 0) { \
			usage = GL_STATIC_DRAW; \
			glGenBuffers(1, &(buffer_id)); \
		} else \
			usage = GL_STREAM_DRAW; \
		glBindBuffer((buffer_type), (buffer_id)); \
		glBufferData((buffer_type), (buffer_size), (buffer_data), usage); \
		PROFILE_ZONE_END(); \
	} while (0)

#endif
