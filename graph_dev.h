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
#include <SDL.h>

struct mesh;
struct entity_context;
struct entity;
struct entity_transform;
union vec3;
struct mat44;
struct mat44d;
struct mat33;

struct graph_dev_color { /* This exactly mimics GdkColor */
	uint32_t pixel;
	uint16_t red;
	uint16_t green;
	uint16_t blue;
};

extern int graph_dev_setup(const char *asset_dir);
extern void graph_dev_reload_all_shaders(void);
extern void graph_dev_start_frame(void);
extern void graph_dev_end_frame(void);
extern void graph_dev_set_color(struct graph_dev_color *color, float a);
extern void graph_dev_set_screen_size(int width, int height);
extern void graph_dev_set_extent_scale(float x_scale, float y_scale);
extern void graph_dev_set_3d_viewport(int x_offset, int y_offset, int width, int height);
extern void graph_dev_clear_depth_bit(void);

/* Cascaded shadow mapping.  graph_dev_shadow_map_enabled is 1 to render and
 * receive shadow maps, 0 to disable (e.g. the GLES backend).  The caller
 * (render_entities) computes one world-space -> light clip-space matrix per
 * cascade and passes them via graph_dev_set_shadow_cascades(); it then renders
 * the casters into each cascade between graph_dev_shadow_map_begin() and
 * graph_dev_shadow_map_end(), selecting the target cascade layer with
 * graph_dev_shadow_map_set_cascade() before drawing that cascade's casters. */
extern int graph_dev_shadow_map_enabled;
extern void graph_dev_set_shadow_cascades(const struct mat44d *world_to_lightclip, int n);
extern void graph_dev_shadow_map_begin(void);
extern void graph_dev_shadow_map_set_cascade(int cascade);
extern void graph_dev_draw_shadow_caster(const struct mat44d *model, struct mesh *m);
extern void graph_dev_shadow_map_end(void);
/* Shadow debug visualization: 0 = off, 1 = shadow factor (white lit / black shadowed),
 * 2 = cascade index tint.  Normally driven by the SNIS_SHADOW_DEBUG env var; this lets
 * tools (e.g. shadow_lab) change it at runtime. */
extern void graph_dev_set_shadow_debug(int mode);
/* Depth-pass slope-scaled polygon-offset bias used when rendering the shadow map.
 * Exposed so shadow-acne / peter-panning can be tuned interactively.  Defaults 2.0, 4.0. */
extern void graph_dev_set_shadow_bias(float factor, float units);
extern void graph_dev_get_shadow_bias(float *factor, float *units);
/* Normal-offset bias, in shadow-map texels: the shadow lookup is lifted off the surface along
 * its normal, scaled by how steeply the light strikes it.  Suppresses shadow acne at grazing
 * angles without the peter-panning the depth-pass slope bias causes, because it does not move
 * the sample along the light ray.  0 disables it; default 1.5. */
extern void graph_dev_set_shadow_normal_offset(float texels);
extern float graph_dev_get_shadow_normal_offset(void);
/* PCF kernel half-width when sampling the shadow map: 0 = single (2x2 hardware) tap,
 * 1 = 3x3, 2 = 5x5, up to a shader-defined maximum.  Applied in every cascade: with a
 * logarithmic split each cascade's texel is proportional to the distance it covers, so a
 * fixed radius in texels is a fixed penumbra on screen. */
extern void graph_dev_set_shadow_pcf_radius(int radius);
extern int graph_dev_get_shadow_pcf_radius(void);
/* Per-frame cascade split far-distances (view space), used for depth-based cascade
 * selection and cross-cascade blending in the lit shaders. */
extern void graph_dev_set_shadow_cascade_splits(const float *split_far, int n);
/* Cross-cascade blend band as a fraction (0..~0.3) of each cascade's far distance;
 * 0 disables blending (hard cascade boundaries). */
extern void graph_dev_set_shadow_blend(float fraction);

/* How an entity's in-shade value pulls its ambient down as well as its direct light.
 *
 * Ambient stands in for light bounced off everything else in the neighbourhood, so a body deep
 * in a shadow that covers the whole neighbourhood should receive less of it -- there is less
 * lit surface around to do the bouncing.  Without this, a fully shadowed body still sits at the
 * full ambient floor and the shadow can never look like more than a flat grey wash.
 *
 * The ramp runs on the entity's RAW in-shade value, which may exceed 1: entity_set_in_shade()
 * takes an unclipped figure so that a deliberately overstated shadow has somewhere to put its
 * surplus, and the direct term is clipped separately when it reaches the shader.  Ambient is
 * left alone up to `lo`, falls to `floor` times its natural value by `hi`, and stays there.
 *
 * floor = 1.0 disables it, which is the default: this changes how every lit thing in the game
 * looks, so it stays inert until something asks for it. */
extern void graph_dev_set_shade_ambient_ramp(float lo, float hi, float floor);
extern void graph_dev_get_shade_ambient_ramp(float *lo, float *hi, float *floor);
extern float graph_dev_get_shadow_blend(void);

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
	int w[6], h[6], hasAlpha[6];
	int is_inside; /* used by cubemaps for skybox vs. planet */
	char whynot[256];
	int loaded_texture_index;
};

/** called before SDL is asked to create the window - can be used to preset
 * driver specific hints, etc.
 *
 * @param flags pointer to the window flags to be used - can be mutated.
*/
extern void graph_dev_prepare_for_window(uint32_t *window_flags);
extern void graph_dev_create_context(SDL_Window *window);

extern unsigned int graph_dev_load_texture(const char *filename, int linear_colorspace);
extern unsigned int graph_dev_load_texture_no_mipmaps(const char *filename, int linear_colorspace);
extern unsigned int graph_dev_texture_to_gpu(struct graph_dev_image_load_request *r);
extern unsigned int graph_dev_texture_to_gpu_no_mipmaps(int texture_id, const char *filename, char *image_data,
				int w, int h, int hasAlpha, int linear_colorspace);
extern const char *graph_dev_get_texture_filename(unsigned int);
extern void graph_dev_draw_skybox(const struct mat44 *mat_vp);

#if MOVING_STARFIELD
/* Draws the star field once, without depth, before the scene.  A no-op if no field is up. */
extern void graph_dev_draw_star_field(struct entity_context *cx, const struct mat44 *mat_mvp);
#endif

/* Gravitational lensing of the skybox.  Each lens bends the starfield around a black hole:
 * an Einstein ring, tangentially smeared arcs outside it, and an inverted second image
 * within.  The opaque disc itself is not drawn here -- it is the black hole's own billboard,
 * drawn as an ordinary entity after the skybox, which is also what covers the singularity at
 * the centre of the mapping.
 *
 * The caller works out the geometry, since it is the only one that knows where the camera and
 * the black holes are; graph_dev just hands the numbers to the shader.  Slots beyond n are
 * disabled by zeroing their Einstein radius, so unused ones cost nothing but a loop iteration.
 *
 * The caller must pass the lenses sorted nearest first.  The shader bends each ray by one lens
 * at a time in slot order, so that a far hole lenses an image the near ones have already moved
 * rather than the two deflections merely adding; that is only right if the slots run in the
 * order the light meets them.  Distance itself is never needed here, only the ordering, which
 * is why there is no distance field below.
 */
#define MAX_GRAVITATIONAL_LENSES 3
/* Prepended to the skybox shader so its uniform arrays are sized from the constant above
 * rather than from a second copy of the number that could drift away from it. */
#define GRAVITATIONAL_LENS_STRINGIFY_(x) #x
#define GRAVITATIONAL_LENS_STRINGIFY(x) GRAVITATIONAL_LENS_STRINGIFY_(x)
#define GRAVITATIONAL_LENS_HEADER \
	"#define MAX_GRAVITATIONAL_LENSES " \
	GRAVITATIONAL_LENS_STRINGIFY(MAX_GRAVITATIONAL_LENSES) "\n"
struct graph_dev_gravitational_lens {
	float direction[3];	/* unit vector from the camera toward the lens, world space */
	float einstein_radius;	/* angular Einstein radius, radians; <= 0 disables the slot */
	float shadow_radius;	/* angular radius of the opaque disc, radians */
	float swirl;		/* signed frame-dragging strength; 0 for none */
};
extern void graph_dev_set_gravitational_lenses(int n,
			const struct graph_dev_gravitational_lens *lens);
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

void graph_dev_clear_window(void);

#endif

