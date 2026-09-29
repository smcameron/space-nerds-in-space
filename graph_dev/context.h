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
#ifndef GRAPH_DEV_CONTEXT_H__
#define GRAPH_DEV_CONTEXT_H__

/* The renderer's window-level state, and the drawing that needs nothing but it.
 *
 * Third of the shared pieces, after the texture cache and the mesh cache, and the one that
 * finally lets identical CODE move rather than only identical data handling.  The obstacle was
 * never that these functions differed -- they are byte-identical in both backends -- but that
 * they read sgc, which was a file static in each, so there were two of it and neither could see
 * the other's.
 *
 * The struct itself was already identical, line for line, in both files.  Declaring it once here
 * does not make anything global that was not global before; it stops there being two of it.
 *
 * WHAT BELONGS HERE.  Only things that need no judgement about which GL version is underneath:
 * the 2D overlay drawing, which is vertex colours and triangles and nothing else; the viewport
 * and screen-size bookkeeping; and a few helpers that are pure but for their arguments.  Anything
 * that touches a shader, a vertex array object, or a format enum stays in the backends, because
 * those are where desktop GL and GLES genuinely part company.
 */

#include "mesh_cache.h"
#include "snis_graph.h"
#include <limits.h>

#include "matrix.h"
#include "star_light.h"

/* Also identical in both, and maybe_unload_shader() below takes one. */
struct graph_dev_gl_shader_metadata {
	GLuint *program_id;
};
struct entity_context;
struct entity;
struct mesh;
/* Moved here whole rather than forward declared: it was identical in both backends, and the two
 * setup functions below fill one in. */
struct graph_dev_primitive {
	int nvertices;
	GLuint vertex_buffer;
	GLuint triangle_vertex_buffer;
};

/* How many 2D vertices are batched before a flush.  Here rather than in a backend because the
 * buffer they size lives in the shared context. */
#define BUFFERED_VERTICES_2D 2000
#define VERTEX_BUFFER_2D_SIZE (BUFFERED_VERTICES_2D * sizeof(struct vertex_color_buffer_data))

struct graph_dev_gl_context {
	int screen_x, screen_y;
	float x_scale, y_scale;
	struct graph_dev_color *hue; /* current color */
	int alpha_blend;
	float alpha;
	GLuint fbo_current;

	int active_vp; /* 0=none, 1=2d, 2=3d */
	int vp_x_3d, vp_y_3d, vp_width_3d, vp_height_3d;
	GLuint fbo_2d;
	struct mat44 ortho_2d_mvp;

	int nvertex_2d;
	GLbyte vertex_type_2d[BUFFERED_VERTICES_2D];
	struct vertex_color_buffer_data vertex_data_2d[BUFFERED_VERTICES_2D];
	GLuint vertex_buffer_2d;

	struct mesh_gl_info gl_info_3d_line;
	GLuint fbo_3d;
	int texture_unit_active;
	GLuint texture_unit_bind[4];
	GLenum src_blend_func;
	GLenum dest_blend_func;
	GLint vp_x, vp_y;
	GLsizei vp_width, vp_height;
};

/* One of these, shared.  Both backends write it; neither owns it. */

/* THE SHADER PREAMBLE, and the one place in the shared code that has to know which GL it is
 * being built for.
 *
 * These files are compiled once per build with the same flags as the backend in use -- that is
 * already how they choose a glad header -- so the choice can be made here, at compile time, and
 * every shader string stays the compile-time concatenation it always was.  Making it a runtime
 * string instead would have looked tidier and would have changed how every shader in the game is
 * assembled, for nothing.
 *
 * What actually differs between the two backends is two string literals.  Everything built on
 * top of them -- twenty-odd shader setup functions -- is identical, and could not be shared
 * while these were defined separately in each. */
#ifdef USE_GLES
#define OPENGL_VERSION_STRING "#version 100\n"
#define UNIVERSAL_SHADER_HEADER \
	OPENGL_VERSION_STRING \
	"precision highp float;\n"
#define GRAPH_DEV_DEFAULT_SHADER_DIRECTORY "share/snis/shader-es"
#else
#define OPENGL_VERSION_STRING "#version 150\n"
#define UNIVERSAL_SHADER_HEADER \
	OPENGL_VERSION_STRING
#define GRAPH_DEV_DEFAULT_SHADER_DIRECTORY "share/snis/shader"
#endif

/*
 * Filmic tonemapping cribbed from oolite:
 *
 *      gamma correction
 *      using Jim Hejl's filmic tonemapping and gamma correction approximation.
 *      Normally this would require HDR, but I think it works extremely well in Oolite.
 *      Formula taken from https://www.gdcvault.com/play/1012351/Uncharted-2-HDR
 *      jump to 27:40 in the video. Note the pow 1.0/2.2 is baked into these numbers
 *
 * Perhaps that it normally requires HDR is the reason it doesn't seem to look so
 * great in SNIS.
 */
#define FILMIC_TONEMAPPING \
	"uniform float u_FilmicTonemapping;\n" \
	"uniform float u_TonemappingGain;\n" \
	"vec4 filmic_tonemap(vec4 color) {\n" \
	"	float dont_tonemap = 1.0 - u_FilmicTonemapping;\n" \
	"	vec3 x = max(vec3(0.0), color.rgb - 0.004);\n" \
	"	x = u_TonemappingGain * (x * (6.2 * x + 0.5)) / (x * (6.2 * x + 1.7) + 0.06);\n" \
	"	return dont_tonemap * color + vec4(u_FilmicTonemapping * x, color.a);\n" \
	"}\n\n"

/* Where the shaders are read from.  One copy, set at init by whichever backend is running. */
extern char shader_directory[PATH_MAX];

extern struct graph_dev_gl_context sgc;

/* Blend state is cached in sgc, so setting it goes through here rather than glBlendFunc: a
 * redundant state change is the one kind of GL call that costs and shows nothing. */
#define BLEND_FUNC(src_blend, dest_blend) \
	do { \
		PROFILE_ZONE_START("BLEND_FUNC"); \
		if (sgc.src_blend_func != src_blend || sgc.dest_blend_func != dest_blend) { \
			glBlendFunc(src_blend, dest_blend); \
			sgc.src_blend_func = src_blend; \
			sgc.dest_blend_func = dest_blend; \
		} \
		PROFILE_ZONE_END(); \
	} while (0)

/* SUPPLIED BY THE BACKEND.  The drawing below reaches back into these four, and each is a place
 * where desktop GL and GLES really do differ -- vertex array objects, glDrawBuffers, and the two
 * mesh rasterisers' shader setup.  They are the seam, and keeping it this narrow is what let the
 * rest of this file be shared unchanged. */
/* Where colour goes.  Desktop GL names its draw buffers explicitly; GLES2 has exactly one and
 * no glDrawBuffers to name it with, so there it is a no-op.  A hook rather than an #ifdef so the
 * code that decides WHEN to switch can be shared with the code that cannot. */
#define GRAPH_DEV_DRAW_BUFFER_BACK 0
#define GRAPH_DEV_DRAW_BUFFER_COLOR0 1
void graph_dev_select_draw_buffer(int which);

/* Vertex array objects.  Core on the desktop, an OES extension on GLES with its own entry
 * points and its own possibility of being absent -- so every use goes through these three and
 * the caller never has to know which it is talking to. */
void graph_dev_gen_vao(GLuint *vao);
void graph_dev_bind_vao(GLuint vao);
void graph_dev_unbind_vao(void);

void draw_vertex_buffer_2d(void);
void enable_3d_viewport(void);
void graph_dev_raster_line_mesh(struct entity *e, const struct mat44 *mat_mvp, struct mesh *m,
				struct sng_color *line_color);


void add_vertex_2d(float x, float y, struct graph_dev_color *color, GLubyte alpha, GLenum mode);
void make_room_in_vertex_buffer_2d(int nvertices);
void note_texture_bound_outside_cache(GLuint tex_id);
void setup_cubemap_cube(struct graph_dev_primitive *obj);
void setup_textured_unit_quad(struct graph_dev_primitive *obj);
void maybe_unload_shader(struct graph_dev_gl_shader_metadata *meta, GLuint *program_id);
int selected_debug_item_checkbox(int n, int x, int y, int *toggle);
void graph_dev_draw_rectangle(int filled, float x, float y, float width, float height);
void debug_menu_draw_item(char *item, int itemnumber, int grayed, int checked);
void graph_dev_compute_star_light(const struct entity_context *cx,
				float light_color[3], float ambient_color[3]);

#endif
