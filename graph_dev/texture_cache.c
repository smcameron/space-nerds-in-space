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

/* The texture cache and its background image loader.
 *
 * Lifted verbatim out of graph_dev_opengl.c, which is where it was written; graph_dev_gles.c
 * carried a byte-identical copy of all thirty-three functions.  Nothing here is new, and the
 * point of the commit that created this file was that nothing should be: two copies of a
 * resource manager is a place for them to drift.
 *
 * PNG decoding happens on a worker thread and the upload to the GPU happens on the render
 * thread, because a GL context belongs to one thread.  So a texture exists in three states --
 * requested, decoded, uploaded -- and the load-status table is what the rest of the renderer
 * asks when it needs to know whether a texture is safe to draw with yet.  Drawing with one
 * that is not gives you the error texture rather than a stall.
 *
 * See graph_dev/texture_cache.h for the one hook the backends supply and why.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <assert.h>
#include <sys/stat.h>
#include <limits.h>
#include <pthread.h>

#ifdef USE_GLES
#include <glad/gles2.h>
#else
#include <glad/gl.h>
#endif
#include <SDL.h>

#include "arraysize.h"
#include "mathutils.h"
#include "png_utils.h"
#include "graph_dev.h"
#include "snis_graph.h"
#include "snis_profile.h"
#include "workqueue.h"
#include "string-utils.h"
#include "texture_cache.h"

#define IMAGE_LOADER_QUEUE_DEPTH 300
#define IMAGE_LOADER_THREAD_COUNT 4
static struct work_queue *image_loader_wq;  /* requests to load PNG images */
static struct work_queue *loaded_images_wq; /* decoded image data waiting to go to the GPU */

struct texture_loading_status {
	GLuint texture_id;
	unsigned char finished_loading;
	unsigned char in_use;
};
static struct texture_loading_status texture_load_status[MAX_LOADED_TEXTURES] = { 0 };
pthread_mutex_t finished_loading_mutex = PTHREAD_MUTEX_INITIALIZER;

static int nloaded_textures;
static struct loaded_texture loaded_textures[MAX_LOADED_TEXTURES];
static char *error_texture_file;
static int no_texture_mode;

int nloaded_cubemap_textures;
struct loaded_cubemap_texture loaded_cubemap_textures[MAX_LOADED_CUBEMAP_TEXTURES];

/* return true if a texture is finished loading. finished_loading_mutex must be held. */
static int texture_finished_loading(GLuint texture_name)
{
	for (int i = 0; i < MAX_LOADED_TEXTURES; i++)
		if (texture_load_status[i].in_use && texture_load_status[i].texture_id == texture_name)
			return texture_load_status[i].finished_loading;
	return 0;
}

static void set_texture_load_status(GLuint texture_name, unsigned char load_status)
{
	/* finished_loading_mutex must be held. */
	int first_unused = -1;
	for (int i = 0; i < MAX_LOADED_TEXTURES; i++) {
		if (first_unused == -1 && texture_load_status[i].in_use == 0) {
			first_unused = i;
			continue;
		}
		if (texture_load_status[i].in_use && texture_load_status[i].texture_id == texture_name) {
			texture_load_status[i].finished_loading = load_status;
			return;
		}
	}
	if (first_unused != -1) {
		texture_load_status[first_unused].texture_id = texture_name;
		texture_load_status[first_unused].in_use = 1;
		texture_load_status[first_unused].finished_loading = load_status;
		return;
	}
	fprintf(stderr, "Too many textures at %s:%d\n", __FILE__, __LINE__);
	abort();
}

static void mark_texture_load_pending(GLuint texture_name)
{
	/* finished_loading_mutex must be held. */
	set_texture_load_status(texture_name, 0);
}

void mark_texture_load_complete(GLuint texture_name)
{
	/* finished_loading_mutex must be held. */
	set_texture_load_status(texture_name, 1);
}

static void mark_texture_load_unused(GLuint texture_name)
{
	/* finished_loading_mutex must be held. */
	for (int i = 0; i < MAX_LOADED_TEXTURES; i++) {
		if (texture_load_status[i].in_use && texture_load_status[i].texture_id == texture_name) {
			texture_load_status[i].in_use = 0;
			texture_load_status[i].texture_id = -1;
			texture_load_status[i].finished_loading = 0;
			break;
		}
	}
}

static void graph_dev_gen_texture_maybe_lock(int count, GLuint *texture_name, int lock)
{
	PROFILE_ZONE_START("graph_dev_gen_texture_maybe_lock");
	glGenTextures(count, texture_name);

	if (lock)
		pthread_mutex_lock(&finished_loading_mutex);
	for (int i = 0; i < count; i++)
		mark_texture_load_pending(texture_name[i]);
	if (lock)
		pthread_mutex_unlock(&finished_loading_mutex);
	PROFILE_ZONE_END();
}

void graph_dev_gen_texture(int count, GLuint *texture_name)
{
	PROFILE_ZONE_START("graph_dev_gen_texture");
	graph_dev_gen_texture_maybe_lock(count, texture_name, 1);
	PROFILE_ZONE_END();
}

static void graph_dev_gen_texture_no_lock(int count, GLuint *texture_name)
{
	PROFILE_ZONE_START("graph_dev_gen_texture_no_lock");
	graph_dev_gen_texture_maybe_lock(count, texture_name, 0);
	PROFILE_ZONE_END();
}


/* If any textures loads (PNG decoding) have completed, send them to the GPU */
void graph_dev_send_completed_textures_to_gpu(void)
{
	PROFILE_ZONE_START("graph_dev_send_completed_textures_to_gpu");
	do {
		struct graph_dev_image_load_request *r = work_queue_dequeue(loaded_images_wq);
		if (!r) {
			PROFILE_ZONE_END();
			return;
		}

		switch (r->request_type) {
		case GRAPH_DEV_IMAGE_LOAD:
		case GRAPH_DEV_CUBEMAP_LOAD:
			(void) graph_dev_texture_to_gpu(r);
			break;
		default:
			fprintf(stderr, "Unknown graph dev image load request type %d\n",
				r->request_type);
			break;
		}
	} while (1);
	/* unreachable */
}

static void enqueue_image_load_request(struct graph_dev_image_load_request *r)
{
	PROFILE_ZONE_START("enqueue_image_load_request");
	work_queue_enqueue(image_loader_wq, r);
	PROFILE_ZONE_END();
}

static void enqueue_image_load_completion(struct graph_dev_image_load_request *r)
{
	PROFILE_ZONE_START("enqueue_image_load_completion");
	work_queue_enqueue(loaded_images_wq, r);
	PROFILE_ZONE_END();
}

static void process_image_load_request_normal(struct graph_dev_image_load_request *r)
{
	PROFILE_ZONE_START("process_image_load_request_normal");
	r->image_data[0] = png_utils_read_png_image(r->filename[0],
				r->flipVertical, r->flipHorizontal, r->pre_multiply_alpha,
				&r->w[0], &r->h[0], &r->hasAlpha[0], r->whynot, sizeof(r->whynot));
	if (!r->image_data[0]) {
		fprintf(stderr, "Failed to decode image file '%s: %s\n",
			r->filename[0], r->whynot);
		graph_dev_free_image_load_request(r);
		PROFILE_ZONE_END();
		return;
	}
	/* Put the data on the queue for the main thread to upload to the GPU */
	enqueue_image_load_completion(r);
	PROFILE_ZONE_END();
}

static void process_image_load_request_cubemap(struct graph_dev_image_load_request *r)
{
	PROFILE_ZONE_START("process_image_load_request_cubemap");
	for (int i = 0; i < 6; i++) {
		r->image_data[i] = png_utils_read_png_image(r->filename[i], 0, r->is_inside, 1,
			&r->w[i], &r->h[i], &r->hasAlpha[i], r->whynot, sizeof(r->whynot));
		if (!r->image_data[i]) {
			fprintf(stderr, "Failed to decode image file '%s: %s\n",
				r->filename[i], r->whynot);
			graph_dev_free_image_load_request(r);
			PROFILE_ZONE_END();
			return;
		}
	}
	/* Put the data on the queue for the main thread to upload to the GPU */
	enqueue_image_load_completion(r);
	PROFILE_ZONE_END();
}

/* Process a request to load an image */
static void process_image_load_request(void *work)
{
	PROFILE_ZONE_START("process_image_load_request");
	struct graph_dev_image_load_request *r = work;
	switch (r->request_type) {
	case GRAPH_DEV_IMAGE_LOAD:
		process_image_load_request_normal(r);
		break;
	case GRAPH_DEV_CUBEMAP_LOAD:
		process_image_load_request_cubemap(r);
		break;
	default:
		fprintf(stderr, "Bad image load request type %d, discarding\n", r->request_type);
		graph_dev_free_image_load_request(r);
		break;
	}
	PROFILE_ZONE_END();
}

/* Set up work queues for loading texture data concurrently with main loop */
void graph_dev_set_up_image_loader_work_queues(void)
{
	image_loader_wq = work_queue_init("png-decode", IMAGE_LOADER_QUEUE_DEPTH,
		IMAGE_LOADER_THREAD_COUNT, process_image_load_request);
	loaded_images_wq = work_queue_init("txtr2gpu", IMAGE_LOADER_QUEUE_DEPTH, 0, NULL);
}

void graph_dev_expire_all_textures(void)
{
	int i;
	PROFILE_ZONE_START("graph_dev_expire_all_textures");

	pthread_mutex_lock(&finished_loading_mutex);
	for (i = 0; i < nloaded_textures; i++)
		loaded_textures[i].expired = 1;
	for (i = 0; i < nloaded_cubemap_textures; i++)
		loaded_cubemap_textures[i].expired = 1;
	pthread_mutex_unlock(&finished_loading_mutex);

	PROFILE_ZONE_END();
}

void graph_dev_expire_texture(char *filename)
{
	int i;
	PROFILE_ZONE_START("graph_dev_expire_texture");

	pthread_mutex_lock(&finished_loading_mutex);
	for (i = 0; i < nloaded_textures; i++)
		if (strcmp(loaded_textures[i].filename, filename) == 0) {
			loaded_textures[i].expired = 1;
			break;
		}
	pthread_mutex_unlock(&finished_loading_mutex);
	PROFILE_ZONE_END();
}

static time_t get_file_modify_time(const char *filename)
{
	struct stat s;
	if (stat(filename, &s) != 0)
		return 0;
	return s.st_mtime;
}

int graph_dev_reload_cubemap_textures(void)
{
	int failed = 0;

	/* Build a list of requests to reload all the cubemap textures */
	pthread_mutex_lock(&finished_loading_mutex);
	int n = nloaded_cubemap_textures;
	struct graph_dev_image_load_request **r = calloc(n, sizeof(*r));
	if (!r) {
		pthread_mutex_unlock(&finished_loading_mutex);
		return -1;
	}
	for (int i = 0; i < n; i++) {
		r[i] = calloc(1, sizeof(*r[i]));
		if (!r[i])
			continue;
		r[i]->texture_id = loaded_cubemap_textures[i].texture_id;
		r[i]->request_type = GRAPH_DEV_CUBEMAP_LOAD;
		r[i]->is_inside = loaded_cubemap_textures[i].is_inside;
		r[i]->linear_colorspace = loaded_cubemap_textures[i].linear_colorspace;
		for (int j = 0; j < 6; j++) {
			r[i]->filename[j] = strdup(loaded_cubemap_textures[i].filename[j]);
		}
		r[i]->flipVertical = 1;
		r[i]->flipHorizontal = 0;
		r[i]->pre_multiply_alpha = 1;
		r[i]->use_mipmaps = 1;

	}
	pthread_mutex_unlock(&finished_loading_mutex);

	/* Submit list of requests to work queue */
	for (int i = 0; i < n; i++)
		enqueue_image_load_request(r[i]);
	/* the indivdual pointers within r[] will get freed after the work is processed
	 * but we need to free r itself now.
	 */
	free(r);
	return failed;
}

void graph_dev_free_image_load_request(struct graph_dev_image_load_request *r)
{
	if (!r)
		return;
	for (int i = 0; i < 6; i++) {
		if (r->filename[i])
			free(r->filename[i]);
		if (r->image_data[i])
			free(r->image_data[i]);
	}
	free(r);
}

unsigned int graph_dev_texture_to_gpu(struct graph_dev_image_load_request *r)
{
	if (r->request_type == GRAPH_DEV_CUBEMAP_LOAD)
		return graph_dev_cubemap_texture_to_gpu(r);

	if (graph_dev_texture_to_gpu_id(r->texture_id, r->image_data[0], r->w[0], r->h[0], r->hasAlpha[0],
				r->use_mipmaps, r->linear_colorspace)) {
		glDeleteTextures(1, (GLuint *) &r->texture_id);
		fprintf(stderr, "Failed to load texture to gpu from '%s'\n", r->filename[0]);
		return 0;
	}

	pthread_mutex_lock(&finished_loading_mutex);
	int n = (r->loaded_texture_index != -1) ?  r->loaded_texture_index : nloaded_textures;
	loaded_textures[n].texture_id = r->texture_id;
	if (loaded_textures[n].filename)
		free(loaded_textures[n].filename);
	loaded_textures[n].filename = strdup(r->filename[0]);
	loaded_textures[n].mtime = get_file_modify_time(r->filename[0]);
	loaded_textures[n].last_mtime_change = 0;
	loaded_textures[n].expired = 0;
	loaded_textures[n].use_mipmaps = r->use_mipmaps;
	loaded_textures[n].linear_colorspace = r->linear_colorspace;

	if (r->loaded_texture_index == -1)
		nloaded_textures++;
	int tid = r->texture_id;
	mark_texture_load_complete(tid);
	pthread_mutex_unlock(&finished_loading_mutex);
	graph_dev_free_image_load_request(r);
	return (unsigned int) tid;
}

const char *graph_dev_get_texture_filename(unsigned int texture_id)
{
	int i;
	pthread_mutex_lock(&finished_loading_mutex);
	for (i = 0; i < nloaded_textures; i++) {
		if (texture_id == loaded_textures[i].texture_id) {
			char *fname = loaded_textures[i].filename;
			pthread_mutex_unlock(&finished_loading_mutex);
			/* FIXME: Racy to allow this to be used, why do I need this?
			 * digging into it, it appears only to be used (eventually) by
			 * material_nebula_write_to_file(), which isn't used anywhere?
			 * Probably used at one point to create the data in
			 * share/snis/material/nebula*.mat
			 */
			return fname;
		}
	}
	pthread_mutex_unlock(&finished_loading_mutex);
	return "";
}

int graph_dev_reload_textures(void)
{
	/* Build a list of requests to re-load all the textures */
	pthread_mutex_lock(&finished_loading_mutex);
	int n = nloaded_textures;
	struct graph_dev_image_load_request **r = calloc(n, sizeof(*r));
	for (int i = 0; i < n; i++) {
		r[i] = calloc(1, sizeof(*r[i]));
		r[i]->texture_id = loaded_textures[i].texture_id;
		r[i]->loaded_texture_index = i;
		r[i]->request_type = GRAPH_DEV_IMAGE_LOAD;
		r[i]->filename[0] = strdup(loaded_textures[i].filename);
		r[i]->flipVertical = 1;
		r[i]->flipHorizontal = 0;
		r[i]->pre_multiply_alpha = 1;
		r[i]->linear_colorspace = loaded_textures[i].linear_colorspace;
		r[i]->use_mipmaps = loaded_textures[i].use_mipmaps;
	}
	pthread_mutex_unlock(&finished_loading_mutex);

	/* Submit the list of requests to re-load all the textures to work queue */
	for (int i = 0; i < n; i++)
		enqueue_image_load_request(r[i]);

	/* the indivdual pointers within r[] will get freed after the work is processed
	 * but we need to free r itself now.
	 */
	free(r);
	return 0;
}

int graph_dev_reload_changed_textures(void)
{
	int n = 0;

	/* Build a list of requests to reload changed textures */
	pthread_mutex_lock(&finished_loading_mutex);
	struct graph_dev_image_load_request **r = calloc(nloaded_textures, sizeof(*r));
	for (int i = 0; i < nloaded_textures; i++) {
		time_t mtime = get_file_modify_time(loaded_textures[i].filename);
		if (loaded_textures[i].mtime != mtime) {
			loaded_textures[i].mtime = mtime;
			loaded_textures[i].last_mtime_change = time_now_double();
		} else if (loaded_textures[i].last_mtime_change > 0 &&
			time_now_double() - loaded_textures[i].last_mtime_change >= TEX_RELOAD_DELAY) {
			printf("reloading texture '%s'\n", loaded_textures[i].filename);
			r[n] = calloc(1, sizeof(*r[n]));
			r[n]->texture_id = loaded_textures[i].texture_id;
			r[n]->loaded_texture_index = i;
			r[n]->request_type = GRAPH_DEV_IMAGE_LOAD;
			r[n]->filename[0] = strdup(loaded_textures[i].filename);
			r[n]->flipVertical = 1;
			r[n]->flipHorizontal = 0;
			r[n]->pre_multiply_alpha = 1;
			r[n]->linear_colorspace = loaded_textures[i].linear_colorspace;
			r[n]->use_mipmaps = loaded_textures[i].use_mipmaps;
			n++;
			loaded_textures[i].last_mtime_change = 0;
		}
	}
	pthread_mutex_unlock(&finished_loading_mutex);

	/* Submit list of requests to image loader work queue */
	for (int i = 0; i < n; i++)
		enqueue_image_load_request(r[i]);

	/* the indivdual pointers within r[] will get freed after the work is processed
	 * but we need to free r itself now.
	 */
	free(r);

	return 0;
}

int graph_dev_reload_changed_cubemap_textures(void)
{
	int n = 0;
	pthread_mutex_lock(&finished_loading_mutex);
	struct graph_dev_image_load_request **r = calloc(nloaded_cubemap_textures, sizeof(*r));
	for (int i = 0; i < nloaded_cubemap_textures; i++) {
		time_t mtime = get_file_modify_time(loaded_cubemap_textures[i].filename[5]);
		if (loaded_cubemap_textures[i].mtime != mtime) {
			loaded_cubemap_textures[i].mtime = mtime;
			loaded_cubemap_textures[i].last_mtime_change = time_now_double();
		} else if (loaded_cubemap_textures[i].last_mtime_change > 0 &&
			time_now_double() - loaded_cubemap_textures[i].last_mtime_change >=
					CUBEMAP_TEX_RELOAD_DELAY) {
			printf("reloading cubemap texture '%s'\n",
				loaded_cubemap_textures[i].filename[0]);
			loaded_cubemap_textures[i].last_mtime_change = 0;
			r[n] = calloc(1, sizeof(*r[n]));
			r[n]->texture_id = loaded_cubemap_textures[i].texture_id;
			r[n]->loaded_texture_index = i;
			r[n]->request_type = GRAPH_DEV_CUBEMAP_LOAD;
			r[n]->is_inside = loaded_cubemap_textures[i].is_inside;
			r[n]->linear_colorspace = loaded_cubemap_textures[i].linear_colorspace;
			for (int j = 0; j < 6; j++)
				r[n]->filename[j] = strdup(loaded_cubemap_textures[i].filename[j]);
			r[n]->flipVertical = 1;
			r[n]->flipHorizontal = 0;
			r[n]->pre_multiply_alpha = 1;
			r[n]->use_mipmaps = 1;
			n++;
		}
	}
	pthread_mutex_unlock(&finished_loading_mutex);

	for (int i = 0; i < n; i++)
		enqueue_image_load_request(r[i]);
	free(r);
	return 0;
}

void graph_dev_set_error_texture(const char *error_texture_png)
{
	error_texture_file = strdup(error_texture_png);
}

void graph_dev_set_no_texture_mode()
{
	no_texture_mode = 1;
	if (!error_texture_file) {
		fprintf(stderr, "BUG at %s:%s:%d: error_texture_file is not set, but no_texture_mode set\n",
			__FILE__, __func__, __LINE__);
		fflush(stderr);
	}
}

int graph_dev_texture_ready(int i)
{
	if (i < 0 || i >= MAX_LOADED_TEXTURES)
		return 0;
	if (i == 0)
		return 1;
	pthread_mutex_lock(&finished_loading_mutex);
	int x = texture_finished_loading(i);
	pthread_mutex_unlock(&finished_loading_mutex);
	return x;
}

int graph_dev_textures_ready(int *tids)
{
	pthread_mutex_lock(&finished_loading_mutex);
	for (int i = 0; tids[i] != -1; i++) {
		if (tids[i] == 0)
			continue;
		if (!texture_finished_loading(tids[i])) {
			pthread_mutex_unlock(&finished_loading_mutex);
			return 0;
		}
	}
	pthread_mutex_unlock(&finished_loading_mutex);
	return 1;
}

/* Enqueue request to load a texture.  The texture will be loaded in another thread,
 * and the image data will appear in the loaded_images_wq work queue later on where
 * the main rendering thread can upload it to the GPU
 */
static unsigned int graph_dev_load_texture_helper(const char *filename, int linear_colorspace, int use_mipmaps)
{
	GLuint texture_id;
	int i;

	/* See if we already loaded this texture */
	pthread_mutex_lock(&finished_loading_mutex);
	for (i = 0; i < nloaded_textures; i++) {
		if (strcmp(filename, loaded_textures[i].filename) == 0) {
			loaded_textures[i].expired = 0;
			int tid = (int) loaded_textures[i].texture_id;
			pthread_mutex_unlock(&finished_loading_mutex);
			return tid;
		}
	}

	/* See if we can re-use an expired texture id (not the actual texture_name though) */
	int index = -1;
	for (i = 0; i < nloaded_textures; i++) {
		if (loaded_textures[i].expired) {
			glBindTexture(GL_TEXTURE_2D, 0);
			glDeleteTextures(1, &loaded_textures[i].texture_id);
			fprintf(stderr, "Replacing %s with %s\n", loaded_textures[i].filename, filename);
			if (loaded_textures[i].filename)
				free(loaded_textures[i].filename);
			loaded_textures[i].filename = strdup(filename);
			loaded_textures[i].mtime = get_file_modify_time(filename);
			loaded_textures[i].last_mtime_change = 0;
			loaded_textures[i].expired = 0;
			loaded_textures[i].use_mipmaps = use_mipmaps;
			loaded_textures[i].linear_colorspace = linear_colorspace;
			texture_id = loaded_textures[i].texture_id;
			mark_texture_load_unused(texture_id);
			index = i;
			break;
		}
	}

	graph_dev_gen_texture_no_lock(1, &texture_id);
	mark_texture_load_pending(texture_id);
	pthread_mutex_unlock(&finished_loading_mutex);

	/* Queue up the image load request */
	struct graph_dev_image_load_request *r = calloc(1, sizeof(*r));
	r->texture_id = (int) texture_id;
	r->loaded_texture_index = index;
	r->request_type = GRAPH_DEV_IMAGE_LOAD;
	r->filename[0] = strdup(filename);
	r->flipVertical = 1;
	r->flipHorizontal = 0;
	r->pre_multiply_alpha = 1;
	r->linear_colorspace = linear_colorspace;
	r->use_mipmaps = use_mipmaps;

	enqueue_image_load_request(r);
	return texture_id;
}

unsigned int graph_dev_load_texture(const char *filename, int linear_colorspace)
{
	return graph_dev_load_texture_helper(filename, linear_colorspace, 1);
}

unsigned int graph_dev_load_texture_no_mipmaps(const char *filename, int linear_colorspace)
{
	return graph_dev_load_texture_helper(filename, linear_colorspace, 0);
}

unsigned int graph_dev_load_cubemap_texture(
	int is_inside,
	int linear_colorspace,
	const char *texture_filename_pos_x,
	const char *texture_filename_neg_x,
	const char *texture_filename_pos_y,
	const char *texture_filename_neg_y,
	const char *texture_filename_pos_z,
	const char *texture_filename_neg_z)
{
	int loaded_texture_index = -1;
	const char *tex_filenames[] = {
		texture_filename_pos_x, texture_filename_neg_x,
		texture_filename_pos_y, texture_filename_neg_y,
		texture_filename_pos_z, texture_filename_neg_z };

	/* Check if we already loaded this texture */
	pthread_mutex_lock(&finished_loading_mutex);
	for (int i = 0; i < nloaded_cubemap_textures; i++) {
		if (loaded_cubemap_textures[i].is_inside == is_inside) {
			int match = 1;
			for (int j = 0; j < NCUBEMAP_TEXTURES; j++) {
				if (strcmp(tex_filenames[j], loaded_cubemap_textures[i].filename[j]) != 0) {
					match = 0;
					break;
				}
			}
			if (match) {
				loaded_cubemap_textures[i].expired = 0;
				pthread_mutex_unlock(&finished_loading_mutex);
				return loaded_cubemap_textures[i].texture_id;
			}
		}
	}

	/* See if we can re-use an expired texture (not the texture name itself though) */
	GLuint cube_texture_id = (GLuint) -1;
	for (int i = 0; i < nloaded_cubemap_textures; i++) {
		if (loaded_cubemap_textures[i].expired) {
			cube_texture_id = loaded_cubemap_textures[i].texture_id;
			loaded_texture_index = i;
			glDeleteTextures(1, &cube_texture_id);
			loaded_cubemap_textures[i].is_inside = is_inside;
			loaded_cubemap_textures[i].expired = 0;
			for (int j = 0; j < NCUBEMAP_TEXTURES; j++) {
				fprintf(stderr, "Replacing %s with %s\n",
					loaded_cubemap_textures[i].filename[j], tex_filenames[j]);
				if (loaded_cubemap_textures[i].filename[j])
					free(loaded_cubemap_textures[i].filename[j]);
				loaded_cubemap_textures[i].filename[j] = strdup(tex_filenames[j]);
			}
			loaded_cubemap_textures[i].linear_colorspace = linear_colorspace;
			break;
		}
	}

	if (nloaded_cubemap_textures >= MAX_LOADED_CUBEMAP_TEXTURES) {
		printf("Unable to load cubemap texture '%s': max of %d textures are already loaded\n",
			texture_filename_pos_x, nloaded_cubemap_textures);
		pthread_mutex_unlock(&finished_loading_mutex);
		return 0;
	}

	if (cube_texture_id != (GLuint) -1)
		mark_texture_load_unused(cube_texture_id);
	cube_texture_id = -1;
	graph_dev_gen_texture_no_lock(1, &cube_texture_id);

	mark_texture_load_pending(cube_texture_id);
	pthread_mutex_unlock(&finished_loading_mutex);

	struct graph_dev_image_load_request *r = calloc(1, sizeof(*r));
	r->texture_id = cube_texture_id;
	r->loaded_texture_index = loaded_texture_index;
	r->request_type = GRAPH_DEV_CUBEMAP_LOAD;
	r->is_inside = is_inside;
	r->linear_colorspace = linear_colorspace;
	for (int i = 0; i < 6; i++)
		r->filename[i] = strdup(tex_filenames[i]);
	r->flipVertical = 1;
	r->flipHorizontal = 0;
	r->pre_multiply_alpha = 1;
	r->use_mipmaps = 1;

	enqueue_image_load_request(r);
	return (unsigned int) cube_texture_id;
}

/* Lives here rather than with the shared drawing code because the list it walks is this
 * file's: the cubemap cache it is expiring from. */
void graph_dev_expire_cubemap_texture(int is_inside,
					const char *texture_filename_pos_x,
					const char *texture_filename_neg_x,
					const char *texture_filename_pos_y,
					const char *texture_filename_neg_y,
					const char *texture_filename_pos_z,
					const char *texture_filename_neg_z)
{
	int i, j;
	PROFILE_ZONE_START("graph_dev_expire_cubemap_texture");

	const char *tex_filenames[] = {
		texture_filename_pos_x, texture_filename_neg_x,
		texture_filename_pos_y, texture_filename_neg_y,
		texture_filename_pos_z, texture_filename_neg_z };

	for (i = 0; i < nloaded_cubemap_textures; i++) {
		if (loaded_cubemap_textures[i].is_inside == is_inside) {
			int match = 1;
			for (j = 0; j < NCUBEMAP_TEXTURES; j++) {
				if (strcmp(tex_filenames[j], loaded_cubemap_textures[i].filename[j]) != 0) {
					match = 0;
					break;
				}
			}
			if (match) {
				loaded_cubemap_textures[i].expired = 1;
				PROFILE_ZONE_END();
				return;
			}
		}
	}
	PROFILE_ZONE_END();
}
