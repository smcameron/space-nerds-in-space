/*
	Copyright (C) 2013 Jeremy Van Grinsven
	Author: Jeremy Van Grinsven

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

#ifndef INCLUDE_graph_device_H
#define INCLUDE_graph_device_H

#include <stdint.h>

struct mesh;
struct entity_context;
struct entity;
struct entity_transform;
union vec3;
struct mat44;
struct mat33;

struct graph_dev_color { /* This exactly mimics GdkColor */
	uint32_t pixel;
	uint16_t red;
	uint16_t green;
	uint16_t blue;
};

extern int graph_dev_setup(const char *shader_dir);
extern void graph_dev_reload_all_shaders(void);
extern void graph_dev_start_frame(void);
extern void graph_dev_end_frame(void);
extern void graph_dev_set_color(struct graph_dev_color *color, float a);
extern void graph_dev_set_screen_size(int width, int height);
extern void graph_dev_set_extent_scale(float x_scale, float y_scale);
extern void graph_dev_set_3d_viewport(int x_offset, int y_offset, int width, int height);
extern void graph_dev_clear_depth_bit(void);

#define GRAPH_DEV_RENDER_FAR_TO_NEAR 0
#define GRAPH_DEV_RENDER_NEAR_TO_FAR 1
extern int graph_dev_entity_render_order(struct entity *e);

extern void graph_dev_draw_entity(struct entity_context *cx, struct entity *e, union vec3 *eye_light_pos,
	const struct entity_transform *transform);
extern void graph_dev_draw_3d_line(struct entity_context *cx, const struct mat44 *mat_vp,
	float x1, float y1, float z1, float x2, float y2, float z2);

extern void graph_dev_draw_line(float x1, float y1, float x2, float y2);
extern void graph_dev_draw_rectangle(int filled, float x, float y, float width, float height);
extern void graph_dev_draw_point(float x, float y);
extern void graph_dev_draw_arc(int filled, float x, float y, float width, float height,
	float angle1, float angle2);

extern int graph_dev_load_skybox_texture(
	const char *texture_filename_pos_x,
	const char *texture_filename_neg_x,
	const char *texture_filename_pos_y,
	const char *texture_filename_neg_y,
	const char *texture_filename_pos_z,
	const char *texture_filename_neg_z);

extern unsigned int graph_dev_load_cubemap_texture(
	int is_inside,
	int linear_colorspace,
        const char *texture_filename_pos_x,
        const char *texture_filename_neg_x,
        const char *texture_filename_pos_y,
        const char *texture_filename_neg_y,
        const char *texture_filename_pos_z,
        const char *texture_filename_neg_z);

struct graph_dev_image_load_request {
#define GRAPH_DEV_IMAGE_LOAD 1
#define GRAPH_DEV_CUBEMAP_LOAD 2
	/* inputs */
	int texture_id;
	int request_type;
	char *filename[6]; /* 6 for cubemaps, normal requests only use 1 */
	int flipVertical, flipHorizontal, pre_multiply_alpha, linear_colorspace;
	int use_mipmaps;

	/* outputs */
	char *image_data[6]; /* 6 for cubemaps, normal requests only use 1 */
	int w, h, hasAlpha;
	char whynot[256];
};

extern unsigned int graph_dev_load_texture(const char *filename, int linear_colorspace);
extern unsigned int graph_dev_load_texture_deferred(const char *filename, int linear_colorspace);
extern unsigned int graph_dev_load_texture_deferred_no_mipmaps(const char *filename, int linear_colorspace);
extern unsigned int graph_dev_load_texture_no_mipmaps(const char *filename, int linear_colorspace);
extern unsigned int graph_dev_texture_to_gpu(struct graph_dev_image_load_request *r);
extern unsigned int graph_dev_texture_to_gpu_no_mipmaps(int texture_id, const char *filename, char *image_data,
				int w, int h, int hasAlpha, int linear_colorspace);
extern const char *graph_dev_get_texture_filename(unsigned int);
extern void graph_dev_draw_skybox(const struct mat44 *mat_vp);
extern int graph_dev_reload_changed_textures(void);
extern int graph_dev_reload_changed_cubemap_textures(void);
extern void graph_dev_expire_all_textures(void);
extern void graph_dev_expire_texture(char *filename);
extern void graph_dev_expire_cubemap_texture(int is_inside,
	const char *texture_filename_pos_x,
	const char *texture_filename_neg_x,
	const char *texture_filename_pos_y,
	const char *texture_filename_neg_y,
	const char *texture_filename_pos_z,
	const char *texture_filename_neg_z);

extern void graph_dev_display_debug_menu_show(void);
extern int graph_dev_graph_dev_debug_menu_click(int x, int y);

/* graph_dev_grab_framebuffer only implemented for opengl backend */
extern void graph_dev_grab_framebuffer(unsigned char **buffer, int *width, int *height);

extern int graph_dev_planet_specularity; /* Set to 1 to enable, 0 to disable planet specularity */
extern int graph_dev_atmosphere_ring_shadows; /* Set to 1 to enable, 0 to disable atmosphere ring shadows */
extern void graph_dev_set_tonemapping_gain(float tmg);
extern void graph_dev_set_error_texture(const char *error_texture_png);
extern void graph_dev_set_no_texture_mode(void);

extern int graph_dev_texture_ready(int i);
extern int graph_dev_textures_ready(int *i);

void graph_dev_free_image_load_request(struct graph_dev_image_load_request *r);

void graph_dev_set_up_image_loader_work_queues(void);

struct graph_dev_image_load_request *graph_dev_get_completed_image_load_request(void);

#endif

