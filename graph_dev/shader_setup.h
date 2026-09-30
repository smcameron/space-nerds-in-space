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
#ifndef GRAPH_DEV_SHADER_SETUP_H__
#define GRAPH_DEV_SHADER_SETUP_H__

/* Compiling the shaders whose setup is the same on both GL versions.
 *
 * Twelve of them, byte-identical in the two backends, and what had kept them apart was not the
 * code at all: they name UNIVERSAL_SHADER_HEADER and shader_directory, which were defined
 * separately in each file.  Once those moved to graph_dev/context.h -- two string literals
 * behind one #ifdef -- the functions themselves had nothing left that differed.
 *
 * The shaders NOT here are the ones whose setup genuinely differs: the textured and lit paths,
 * the sun, the ring band, and the shadow map, which desktop GL has and GLES does not.
 */

#include "context.h"

struct graph_dev_gl_shader_common {
	struct graph_dev_gl_shader_metadata meta;
	GLuint program_id;
	GLuint vao_id;
};

struct graph_dev_gl_atmosphere_shader {
	struct graph_dev_gl_shader_metadata meta;
	GLuint program_id;
	GLuint vao_id;
	GLint mvp_matrix_id;
	GLint mv_matrix_id;
	GLint normal_matrix_id;
	GLint vertex_position_id;
	GLint vertex_normal_id;
	GLint light_pos_id;
	GLint color_id;
	GLfloat alpha;
	GLint shadow_annulus_texture_id;
	GLint shadow_annulus_center_id;
	GLint shadow_annulus_normal_id;
	GLint shadow_annulus_radius_id;
	GLint shadow_annulus_tint_color_id;
	GLint ring_texture_v_id;
	GLint atmosphere_brightness_id;
	GLint light_color_id;   /* star-tinted direct light colour (u_LightColor) */
	GLint ambient_color_id; /* absolute, complement-tinted ambient colour (u_AmbientColor) */
	GLint filmic_tonemapping_id;
	GLint tonemapping_gain_id;
};

struct graph_dev_gl_black_hole_shader {
	struct graph_dev_gl_shader_metadata meta;
	GLuint program_id;
	GLuint vao_id;
	GLint mvp_matrix_id;
	GLint vertex_position_id;
	GLint texture_coord_id;
	GLint disc_radius_id;
	GLint edge_softness_id;
	GLint ring_brightness_id;
	GLint ring_width_id;
	GLint einstein_radius_id;
	GLint glow_brightness_id;
	GLint glow_width_id;
	GLint ring_color_id;
};

struct graph_dev_gl_color_by_w_shader {
	struct graph_dev_gl_shader_metadata meta;
	GLuint program_id;
	GLuint vao_id;
	GLint mvp_id;
	GLint position_id;
	GLint near_color_id;
	GLint near_w_id;
	GLint center_color_id;
	GLint center_w_id;
	GLint far_color_id;
	GLint far_w_id;
};

struct graph_dev_gl_filled_wireframe_shader {
	struct graph_dev_gl_shader_metadata meta;
	GLuint program_id;
	GLuint vao_id;
	GLint viewport_id;
	GLint mvp_matrix_id;
	GLint position_id;
	GLint tvertex0_id;
	GLint tvertex1_id;
	GLint tvertex2_id;
	GLint edge_mask_id;
	GLint line_color_id;
	GLint triangle_color_id;
};

struct graph_dev_gl_fs_effect_shader { /* For full screen effect shaders */
	struct graph_dev_gl_shader_metadata meta;
	GLuint program_id;
	GLuint vao_id;
	GLint mvp_matrix_id;
	GLint vertex_position_id;
	GLint texture_coord_id;
	GLint tint_color_id;
	GLint viewport_id;
	GLint texture0_id;
	GLint texture1_id;
	GLint texture2_id;
};

struct graph_dev_gl_line_single_color_shader {
	struct graph_dev_gl_shader_metadata meta;
	GLuint program_id;
	GLuint vao_id;
	GLint mvp_matrix_id;
	GLint viewport_id;
	GLint multi_one_id;
	GLint vertex_position_id;
	GLint line_vertex0_id;
	GLint line_vertex1_id;
	GLint dot_size_id;
	GLint dot_pitch_id;
	GLint line_color_id;
};

struct graph_dev_gl_point_cloud_shader {
	struct graph_dev_gl_shader_metadata meta;
	GLuint program_id;
	GLuint vao_id;
	GLint mvp_matrix_id;
	GLint vertex_position_id;
	GLint point_size_id;
	GLint color_id;
	GLint time_id;
	GLint camera_pos_id;  /* world-space eye, for a per-point distance fade */
	GLint fade_params_id; /* (near0, near1, far0, far1); w <= 0 leaves points flat */
};

struct graph_dev_gl_single_color_shader {
	struct graph_dev_gl_shader_metadata meta;
	GLuint program_id;
	GLuint vao_id;
	GLint mvp_matrix_id;
	GLint vertex_position_id;
	GLint color_id;
};

struct graph_dev_gl_exhaust_plume_shader {
	struct graph_dev_gl_shader_metadata meta;
	GLuint program_id;
	GLuint vao_id;
	GLint mvp_matrix_id;
	GLint mv_matrix_id;
	GLint vertex_position_id;
	GLint vertex_normal_id;
	GLint texture_coord_id;
	GLint tint_color_id;
	GLint core_brightness_id;
	GLint plume_length_id;
	GLint noise_seed_id;
	GLint diamond_spacing_id;
	GLint diamond_intensity_id;
	GLint filmic_tonemapping_id;
	GLint tonemapping_gain_id;
};

/* A ship's death: see ship_death.h.  Its shaders take the light in world space: the star's
 * position, its tinted colour and the ambient. */
struct graph_dev_gl_shrapnel_shader {
	struct graph_dev_gl_shader_metadata meta;
	GLuint program_id;
	GLuint vao_id;
	GLint mvp_matrix_id;
	GLint model_matrix_id;
	GLint vertex_position_id;
	GLint vertex_normal_id;
	GLint light_pos_id;
	GLint star_tint_id;
	GLint ambient_id;
	GLint blackbody_id;
	GLint temperature_id;
	GLint brightness_id;
	GLint albedo_id;
	GLint filmic_tonemapping_id;
	GLint tonemapping_gain_id;
};

struct graph_dev_gl_wreck_shader {
	struct graph_dev_gl_shader_metadata meta;
	GLuint program_id;
	GLuint vao_id;
	GLint mvp_matrix_id;
	GLint model_matrix_id;
	GLint vertex_position_id;
	GLint vertex_normal_id;
	GLint texture_coord_id;
	GLint edge_id;			/* a_Edge: the vertex's distance to the tear */
	GLint light_pos_id;
	GLint star_tint_id;
	GLint ambient_id;
	GLint aux_light_pos_id;
	GLint aux_light_color_id;
	GLint aux_light_wrap_id;
	GLint albedo_id;		/* the hull's texture */
	GLint have_texture_id;
	GLint blackbody_id;
	GLint interior_id;
	GLint scorch_id;
	GLint edge_width_id;
	GLint edge_temp_id;
	GLint edge_brightness_id;
	GLint hull_radius_id;
	GLint dissolve_id;
	GLint burn_glow_id;
	GLint preheat_id;
	GLint time_id;
	GLint filmic_tonemapping_id;
	GLint tonemapping_gain_id;
};

/* A batch of smoke, flame and sparks: see particle_batch.h.  The mesh is already in world
 * space and already faces the camera, and each vertex carries its particle's look. */
struct graph_dev_gl_particles_shader {
	struct graph_dev_gl_shader_metadata meta;
	GLuint program_id;
	GLuint vao_id;
	GLint mvp_matrix_id;
	GLint vertex_position_id;
	GLint vertex_normal_id;		/* not a normal: (opacity, emission, kelvin) */
	GLint texture_coord_id;		/* where on its quad, in radii */
	GLint edge_id;			/* a_Edge: its kind, and its seed as a fraction */
	GLint cam_right_id;
	GLint cam_up_id;
	GLint cam_back_id;
	GLint light_pos_id;
	GLint star_tint_id;
	GLint ambient_id;
	GLint albedo_id;
	GLint time_id;
	GLint blackbody_id;
	GLint filmic_tonemapping_id;
	GLint tonemapping_gain_id;
};

/* The fireball: a volume raymarched inside a camera facing billboard.  See explosion.shader. */
struct graph_dev_gl_explosion_shader {
	struct graph_dev_gl_shader_metadata meta;
	GLuint program_id;
	GLuint vao_id;
	GLint mvp_matrix_id;
	GLint model_matrix_id;
	GLint vertex_position_id;
	GLint eye_pos_id;
	GLint light_pos_id;
	GLint star_tint_id;
	GLint ambient_id;
	GLint blackbody_id;
	GLint age_id;
	GLint seed_id;
	GLint peak_temp_id;
	GLint cooling_id;
	GLint brightness_id;
	GLint radiance_id;
	GLint density_id;
	GLint edge_id;
	GLint lumpiness_id;
	GLint frequency_id;
	GLint roll_id;
	GLint smoke_start_id;
	GLint smoke_albedo_id;
	GLint dilution_id;
	GLint shred_id;
	GLint steps_id;
	GLint scene_depth_id;
	GLint viewport_id;
	GLint near_far_id;
	GLint camera_forward_id;
	GLint filmic_tonemapping_id;
	GLint tonemapping_gain_id;
};

struct graph_dev_gl_skybox_shader {
	struct graph_dev_gl_shader_metadata meta;
	GLuint program_id;
	GLuint vao_id;
	GLint mvp_id;
	GLint vertex_id;
	GLint texture_id;
	GLuint cube_texture_id;
	GLint filmic_tonemapping_id;
	GLint tonemapping_gain_id;
	GLint lens_dir_id;
	GLint lens_params_id;
};

struct graph_dev_gl_textured_particle_shader {
	struct graph_dev_gl_shader_metadata meta;
	GLuint program_id;
	GLuint vao_id;
	GLint mvp_matrix_id;
	GLint camera_up_vec_id;
	GLint camera_right_vec_id;
	GLint time_id;
	GLint radius_id;
	GLint multi_one_id;
	GLint start_position_id;
	GLint start_tint_color_id;
	GLint start_apm_id;
	GLint end_position_id;
	GLint end_tint_color_id;
	GLint end_apm_id;
	GLint texture_id; /* param to vertex shader */
	GLint filmic_tonemapping_id;
	GLint tonemapping_gain_id;
};

struct graph_dev_gl_trans_wireframe_shader {
	struct graph_dev_gl_shader_metadata meta;
	GLuint program_id;
	GLuint vao_id;
	GLint mvp_matrix_id;
	GLint mv_matrix_id;
	GLint normal_matrix_id;
	GLint vertex_position_id;
	GLint vertex_normal_id;
	GLint color_id;

	GLint clip_sphere_id;
	GLint clip_sphere_radius_fade_id;
};

struct graph_dev_gl_vertex_color_shader {
	struct graph_dev_gl_shader_metadata meta;
	GLuint program_id;
	GLuint vao_id;
	GLint mvp_matrix_id;
	GLint vertex_position_id;
	GLint vertex_color_id;
};



void setup_atmosphere_shader(struct graph_dev_gl_atmosphere_shader *shader, int with_ring_shadow);
void setup_fs_effect_shader(const char *basename, struct graph_dev_gl_fs_effect_shader *shader);
void setup_textured_particle_shader(struct graph_dev_gl_textured_particle_shader *shader);
void setup_point_cloud_shader(const char *basename, struct graph_dev_gl_point_cloud_shader *shader);
void setup_skybox_shader(struct graph_dev_gl_skybox_shader *shader);
void setup_trans_wireframe_shader(const char *basename, struct graph_dev_gl_trans_wireframe_shader *shader);
void setup_filled_wireframe_shader(struct graph_dev_gl_filled_wireframe_shader *shader);
void setup_color_by_w_shader(struct graph_dev_gl_color_by_w_shader *shader);
void setup_line_single_color_shader(struct graph_dev_gl_line_single_color_shader *shader);
void setup_black_hole_shader(struct graph_dev_gl_black_hole_shader *shader);
void setup_exhaust_plume_shader(struct graph_dev_gl_exhaust_plume_shader *shader);
void setup_shrapnel_shader(struct graph_dev_gl_shrapnel_shader *shader);
void setup_wreck_shader(struct graph_dev_gl_wreck_shader *shader);
void setup_particles_shader(struct graph_dev_gl_particles_shader *shader);
void setup_explosion_shader(struct graph_dev_gl_explosion_shader *shader);
void setup_single_color_shader(struct graph_dev_gl_single_color_shader *shader);
void setup_vertex_color_shader(struct graph_dev_gl_vertex_color_shader *shader);

void activate_shader(const void *vptr);

#endif
