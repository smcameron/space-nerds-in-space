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
#ifndef GRAPH_DEV_TEXTURE_CACHE_H__
#define GRAPH_DEV_TEXTURE_CACHE_H__

/* The texture cache and its background image loader, shared by the two graph_dev backends.
 *
 * THIS HEADER IS INTERNAL to graph_dev_opengl.c and graph_dev_gles.c.  Nothing else should
 * include it: the public interface is graph_dev.h, and everything here is either state those
 * two files share or the one hook each of them has to supply.
 *
 * Why this is the piece that came out first.  The two backends were 88% identical line for
 * line, but most of that is rendering, where an apparent similarity can hide a real
 * difference in what a GL version guarantees.  Texture loading is not like that: it is
 * resource management, it was byte-identical across all thirty-three functions, and the
 * calls it makes are common to desktop GL and GLES alike.  So it can be shared without
 * anyone having to decide whether two similar-looking lines meant the same thing.
 *
 * The seam is one function wide.  cubemap_texture_to_gpu() differs by three lines --
 * GL_TEXTURE_WRAP_R does not exist in GLES, and the sRGB internal formats are spelled
 * differently there -- so each backend supplies it and the shared code calls it.
 */

#include <pthread.h>
#include <time.h>

#ifdef USE_GLES
#include <glad/gles2.h>
#else
#include <glad/gl.h>
#endif

#include "graph_dev.h"	/* struct graph_dev_image_load_request */

#define MAX_LOADED_TEXTURES 300
#define NCUBEMAP_TEXTURES 6
#define MAX_LOADED_CUBEMAP_TEXTURES 40
#define TEX_RELOAD_DELAY 1.0
#define CUBEMAP_TEX_RELOAD_DELAY 1.0

struct loaded_texture {
	GLuint texture_id;
	char *filename;
	time_t mtime;
	double last_mtime_change;
	int expired;
	int use_mipmaps;
	int linear_colorspace;
};

struct loaded_cubemap_texture {
	GLuint texture_id;
	int is_inside;
	char *filename[NCUBEMAP_TEXTURES];
	time_t mtime;
	double last_mtime_change;
	int expired;
	int linear_colorspace;
};

/* The cache itself.  Exposed only because the backend's cubemap_texture_to_gpu() has to
 * record what it uploaded; nothing else outside graph_dev/texture_cache.c touches these. */
extern int nloaded_cubemap_textures;
extern struct loaded_cubemap_texture loaded_cubemap_textures[MAX_LOADED_CUBEMAP_TEXTURES];

/* Guards the load-status table AND the two cache arrays above.  Held across the whole of an
 * upload, since a worker thread may be adding to the cache while the render thread reads it. */
extern pthread_mutex_t finished_loading_mutex;

/* THE HOOK.  Each backend implements this; it is the only part of texture loading that a GL
 * version has an opinion about.  Returns the texture id, or 0.  Called with
 * finished_loading_mutex NOT held. */
unsigned int graph_dev_cubemap_texture_to_gpu(struct graph_dev_image_load_request *r);

/* THE OTHER HOOK, and the same story: an ordinary 2D upload, differing only in how the two
 * spell their sRGB internal formats -- and in that GLES has to ask whether it has the sRGB
 * extension at all.  Returns non-zero on success. */
int graph_dev_texture_to_gpu_id(GLuint texture_number, char *image_data,
				int w, int h, int hasAlpha, int use_mipmaps, int linear_colorspace);

/* The shared side, for the backends' own use.  The rest of what this file implements is
 * declared in graph_dev.h, being public. */
void graph_dev_set_up_image_loader_work_queues(void);
void graph_dev_send_completed_textures_to_gpu(void);
void graph_dev_free_image_load_request(struct graph_dev_image_load_request *r);
void graph_dev_gen_texture(int count, GLuint *texture_name);
void mark_texture_load_complete(GLuint texture_name);

#endif
