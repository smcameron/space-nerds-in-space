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

/* See graph_dev/shader_setup.h. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef USE_GLES
#include <glad/gles2.h>
#else
#include <glad/gl.h>
#endif

#include "shader.h"
#include "graph_dev.h"		/* GRAVITATIONAL_LENS_HEADER */
#include "shader_setup.h"

static GLuint drawstate_active_program = 0;

void
activate_shader(const void *vptr)
{
	const struct graph_dev_gl_shader_common *shader = (const struct graph_dev_gl_shader_common *)vptr;
	if (drawstate_active_program == shader->program_id) {
		return;
	}
	glUseProgram(shader->program_id);
	graph_dev_bind_vao(shader->vao_id);

	drawstate_active_program = shader->program_id;
}


void setup_atmosphere_shader(struct graph_dev_gl_atmosphere_shader *shader, int with_ring_shadow)
{

	maybe_unload_shader(&shader->meta, &shader->program_id);
	/* Create and compile our GLSL program from the shaders */
	shader->program_id = load_shaders(shader_directory,
				"atmosphere.vert", "atmosphere.frag",
				with_ring_shadow ?
				UNIVERSAL_SHADER_HEADER FILMIC_TONEMAPPING "\n#define USE_ANNULUS_SHADOW 1\n" :
				UNIVERSAL_SHADER_HEADER FILMIC_TONEMAPPING);

	/* create the VAO for this shader */
	graph_dev_gen_vao(&shader->vao_id);

	/* Get a handle for our "MVP" uniform */
	shader->mvp_matrix_id = glGetUniformLocation(shader->program_id, "u_MVPMatrix");
	shader->mv_matrix_id = glGetUniformLocation(shader->program_id, "u_MVMatrix");
	shader->normal_matrix_id = glGetUniformLocation(shader->program_id, "u_NormalMatrix");
	shader->light_pos_id = glGetUniformLocation(shader->program_id, "u_LightPos");
	shader->atmosphere_brightness_id = glGetUniformLocation(shader->program_id, "u_atmosphere_brightness");
	shader->light_color_id = glGetUniformLocation(shader->program_id, "u_LightColor");
	shader->ambient_color_id = glGetUniformLocation(shader->program_id, "u_AmbientColor");

	/* Get a handle for our buffers */
	shader->vertex_position_id = glGetAttribLocation(shader->program_id, "a_Position");
	shader->vertex_normal_id = glGetAttribLocation(shader->program_id, "a_Normal");
	shader->color_id = glGetUniformLocation(shader->program_id, "u_Color");
	shader->alpha = glGetUniformLocation(shader->program_id, "u_Alpha");
	shader->filmic_tonemapping_id = glGetUniformLocation(shader->program_id, "u_FilmicTonemapping");
	shader->tonemapping_gain_id = glGetUniformLocation(shader->program_id, "u_TonemappingGain");

	if (with_ring_shadow) {
		shader->shadow_annulus_texture_id = glGetUniformLocation(shader->program_id, "u_AnnulusAlbedoTex");
		shader->shadow_annulus_center_id = glGetUniformLocation(shader->program_id, "u_AnnulusCenter");
		shader->shadow_annulus_normal_id = glGetUniformLocation(shader->program_id, "u_AnnulusNormal");
		shader->shadow_annulus_radius_id = glGetUniformLocation(shader->program_id, "u_AnnulusRadius");
		shader->shadow_annulus_tint_color_id = glGetUniformLocation(shader->program_id, "u_AnnulusTintColor");
		shader->ring_texture_v_id = glGetUniformLocation(shader->program_id, "u_ring_texture_v");
	}
}


void setup_fs_effect_shader(const char *basename,
	struct graph_dev_gl_fs_effect_shader *shader)
{
	const char *vert_header =
		UNIVERSAL_SHADER_HEADER
		"#define INCLUDE_VS 1\n";
	const char *frag_header =
		UNIVERSAL_SHADER_HEADER
		"#define INCLUDE_FS 1\n";

	/* Create and compile our GLSL program from the shaders */
	char shader_filename[255];
	snprintf(shader_filename, sizeof(shader_filename), "%s.shader", basename);

	const char *filenames[] = { shader_filename };

	maybe_unload_shader(&shader->meta, &shader->program_id);
	shader->program_id = load_concat_shaders(shader_directory, vert_header, 1, filenames,
		frag_header, 1, filenames);
	/* create the VAO for this shader */
	graph_dev_gen_vao(&shader->vao_id);

	activate_shader(shader);

	shader->mvp_matrix_id = glGetUniformLocation(shader->program_id, "u_MVPMatrix");
	shader->vertex_position_id = glGetAttribLocation(shader->program_id, "a_Position");
	shader->texture_coord_id = glGetAttribLocation(shader->program_id, "a_TexCoord");
	shader->tint_color_id = glGetUniformLocation(shader->program_id, "u_TintColor");
	shader->viewport_id = glGetUniformLocation(shader->program_id, "u_Viewport");
	shader->texture0_id = glGetUniformLocation(shader->program_id, "texture0Sampler");
	if (shader->texture0_id >= 0)
		glUniform1i(shader->texture0_id, 0);
	shader->texture1_id = glGetUniformLocation(shader->program_id, "texture1Sampler");
	if (shader->texture1_id >= 0)
		glUniform1i(shader->texture1_id, 1);
	shader->texture2_id = glGetUniformLocation(shader->program_id, "texture2Sampler");
	if (shader->texture2_id >= 0)
		glUniform1i(shader->texture2_id, 2);
}


void setup_textured_particle_shader(struct graph_dev_gl_textured_particle_shader *shader)
{
	maybe_unload_shader(&shader->meta, &shader->program_id);
	/* Create and compile our GLSL program from the shaders */
	shader->program_id = load_shaders(shader_directory,
				"textured-particle.vert", "textured-particle.frag",
				UNIVERSAL_SHADER_HEADER FILMIC_TONEMAPPING);
	/* create the VAO for this shader */
	graph_dev_gen_vao(&shader->vao_id);

	activate_shader(shader);

	shader->mvp_matrix_id = glGetUniformLocation(shader->program_id, "u_MVPMatrix");
	shader->camera_up_vec_id = glGetUniformLocation(shader->program_id, "u_CameraUpVec");
	shader->camera_right_vec_id = glGetUniformLocation(shader->program_id, "u_CameraRightVec");
	shader->time_id = glGetUniformLocation(shader->program_id, "u_Time");
	shader->radius_id = glGetUniformLocation(shader->program_id, "u_Radius");
	shader->texture_id = glGetUniformLocation(shader->program_id, "u_AlbedoTex");
	shader->filmic_tonemapping_id = glGetUniformLocation(shader->program_id, "u_FilmicTonemapping");
	shader->tonemapping_gain_id = glGetUniformLocation(shader->program_id, "u_TonemappingGain");
	glUniform1i(shader->texture_id, 0);

	shader->multi_one_id = glGetAttribLocation(shader->program_id, "a_MultiOne");
	shader->start_position_id = glGetAttribLocation(shader->program_id, "a_StartPosition");
	shader->start_tint_color_id = glGetAttribLocation(shader->program_id, "a_StartTintColor");
	shader->start_apm_id = glGetAttribLocation(shader->program_id, "a_StartAPM");
	shader->end_position_id = glGetAttribLocation(shader->program_id, "a_EndPosition");
	shader->end_tint_color_id = glGetAttribLocation(shader->program_id, "a_EndTintColor");
	shader->end_apm_id = glGetAttribLocation(shader->program_id, "a_StartAPM");
}


void setup_point_cloud_shader(const char *basename, struct graph_dev_gl_point_cloud_shader *shader)
{
	char vert_filename[PATH_MAX];
	char frag_filename[PATH_MAX];
	snprintf(vert_filename, sizeof(vert_filename), "%s.vert", basename);
	snprintf(frag_filename, sizeof(frag_filename), "%s.frag", basename);

	maybe_unload_shader(&shader->meta, &shader->program_id);
	/* Create and compile our GLSL program from the shaders */
	shader->program_id = load_shaders(shader_directory, vert_filename, frag_filename,
				UNIVERSAL_SHADER_HEADER);
	/* create the VAO for this shader */
	graph_dev_gen_vao(&shader->vao_id);

	/* Get a handle for our "MVP" uniform */
	shader->mvp_matrix_id = glGetUniformLocation(shader->program_id, "u_MVPMatrix");

	/* Get a handle for our buffers */
	shader->vertex_position_id = glGetAttribLocation(shader->program_id, "a_Position");
	shader->point_size_id = glGetUniformLocation(shader->program_id, "u_PointSize");
	shader->color_id = glGetUniformLocation(shader->program_id, "u_Color");
	shader->time_id = glGetUniformLocation(shader->program_id, "u_Time");
	shader->camera_pos_id = glGetUniformLocation(shader->program_id, "u_CameraPos");
	shader->fade_params_id = glGetUniformLocation(shader->program_id, "u_FadeParams");
}


void setup_skybox_shader(struct graph_dev_gl_skybox_shader *shader)
{
	maybe_unload_shader(&shader->meta, &shader->program_id);
	/* Create and compile our GLSL program from the shaders */
	shader->program_id = load_shaders(shader_directory, "skybox.vert", "skybox.frag",
						UNIVERSAL_SHADER_HEADER FILMIC_TONEMAPPING
						GRAVITATIONAL_LENS_HEADER);
	/* create the VAO for this shader */
	graph_dev_gen_vao(&shader->vao_id);

	activate_shader(shader);

	/* Get a handle for our "MVP" uniform */
	shader->mvp_id = glGetUniformLocation(shader->program_id, "MVP");
	shader->texture_id = glGetUniformLocation(shader->program_id, "s_texture");
	shader->filmic_tonemapping_id = glGetUniformLocation(shader->program_id, "u_FilmicTonemapping");
	shader->tonemapping_gain_id = glGetUniformLocation(shader->program_id, "u_TonemappingGain");
	shader->lens_dir_id = glGetUniformLocation(shader->program_id, "u_LensDir");
	shader->lens_params_id = glGetUniformLocation(shader->program_id, "u_LensParams");
	glUniform1i(shader->texture_id, 0);

	/* Get a handle for our buffers */
	shader->vertex_id = glGetAttribLocation(shader->program_id, "vertex");
}


void setup_trans_wireframe_shader(const char *basename, struct graph_dev_gl_trans_wireframe_shader *shader)
{
	char vert_filename[PATH_MAX];
	char frag_filename[PATH_MAX];
	snprintf(vert_filename, sizeof(vert_filename), "%s.vert", basename);
	snprintf(frag_filename, sizeof(frag_filename), "%s.frag", basename);

	maybe_unload_shader(&shader->meta, &shader->program_id);
	/* Create and compile our GLSL program from the shaders */
	shader->program_id = load_shaders(shader_directory, vert_filename, frag_filename,
						UNIVERSAL_SHADER_HEADER);
	/* create the VAO for this shader */
	graph_dev_gen_vao(&shader->vao_id);

	shader->mvp_matrix_id = glGetUniformLocation(shader->program_id, "u_MVPMatrix");
	shader->mv_matrix_id = glGetUniformLocation(shader->program_id, "u_MVMatrix");
	shader->normal_matrix_id = glGetUniformLocation(shader->program_id, "u_NormalMatrix");
	shader->vertex_position_id = glGetAttribLocation(shader->program_id, "a_Position");
	shader->vertex_normal_id = glGetAttribLocation(shader->program_id, "a_Normal");
	shader->color_id = glGetUniformLocation(shader->program_id, "u_Color");
	shader->clip_sphere_id = glGetUniformLocation(shader->program_id, "u_ClipSphere");
	shader->clip_sphere_radius_fade_id = glGetUniformLocation(shader->program_id, "u_ClipSphereRadiusFade");
}


void setup_filled_wireframe_shader(struct graph_dev_gl_filled_wireframe_shader *shader)
{
	maybe_unload_shader(&shader->meta, &shader->program_id);
	/* Create and compile our GLSL program from the shaders */
	shader->program_id = load_shaders(shader_directory,
					"wireframe_filled.vert", "wireframe_filled.frag",
					UNIVERSAL_SHADER_HEADER);
	/* create the VAO for this shader */
	graph_dev_gen_vao(&shader->vao_id);

	shader->viewport_id = glGetUniformLocation(shader->program_id, "Viewport");
	shader->mvp_matrix_id = glGetUniformLocation(shader->program_id, "ModelViewProjectionMatrix");

	shader->position_id = glGetAttribLocation(shader->program_id, "position");
	shader->tvertex0_id = glGetAttribLocation(shader->program_id, "tvertex0");
	shader->tvertex1_id = glGetAttribLocation(shader->program_id, "tvertex1");
	shader->tvertex2_id = glGetAttribLocation(shader->program_id, "tvertex2");
	shader->edge_mask_id = glGetAttribLocation(shader->program_id, "edge_mask");

	shader->line_color_id = glGetUniformLocation(shader->program_id, "line_color");
	shader->triangle_color_id = glGetUniformLocation(shader->program_id, "triangle_color");
}


void setup_color_by_w_shader(struct graph_dev_gl_color_by_w_shader *shader)
{
	maybe_unload_shader(&shader->meta, &shader->program_id);
	/* Create and compile our GLSL program from the shaders */
	shader->program_id = load_shaders(shader_directory, "color_by_w.vert", "color_by_w.frag",
					UNIVERSAL_SHADER_HEADER);
	/* create the VAO for this shader */
	graph_dev_gen_vao(&shader->vao_id);

	/* Get a handle for our "MVP" uniform */
	shader->mvp_id = glGetUniformLocation(shader->program_id, "u_MVPMatrix");

	/* Get a handle for our buffers */
	shader->position_id = glGetAttribLocation(shader->program_id, "a_Position");
	shader->near_color_id = glGetUniformLocation(shader->program_id, "u_NearColor");
	shader->near_w_id = glGetUniformLocation(shader->program_id, "u_NearW");
	shader->center_color_id = glGetUniformLocation(shader->program_id, "u_CenterColor");
	shader->center_w_id = glGetUniformLocation(shader->program_id, "u_CenterW");
	shader->far_color_id = glGetUniformLocation(shader->program_id, "u_FarColor");
	shader->far_w_id = glGetUniformLocation(shader->program_id, "u_FarW");
}



void setup_line_single_color_shader(struct graph_dev_gl_line_single_color_shader *shader)
{
	maybe_unload_shader(&shader->meta, &shader->program_id);
	/* Create and compile our GLSL program from the shaders */
	shader->program_id = load_shaders(shader_directory,
				"line-single-color.vert", "line-single-color.frag",
				UNIVERSAL_SHADER_HEADER);
	/* create the VAO for this shader */
	graph_dev_gen_vao(&shader->vao_id);

	shader->mvp_matrix_id = glGetUniformLocation(shader->program_id, "u_MVPMatrix");
	shader->viewport_id = glGetUniformLocation(shader->program_id, "u_Viewport");
	shader->dot_size_id = glGetUniformLocation(shader->program_id, "u_DotSize");
	shader->dot_pitch_id = glGetUniformLocation(shader->program_id, "u_DotPitch");
	shader->line_color_id = glGetUniformLocation(shader->program_id, "u_LineColor");

	shader->multi_one_id = glGetAttribLocation(shader->program_id, "a_MultiOne");
	shader->vertex_position_id = glGetAttribLocation(shader->program_id, "a_Position");
	shader->line_vertex0_id = glGetAttribLocation(shader->program_id, "a_LineVertex0");
	shader->line_vertex1_id = glGetAttribLocation(shader->program_id, "a_LineVertex1");
}


void setup_black_hole_shader(struct graph_dev_gl_black_hole_shader *shader)
{
	maybe_unload_shader(&shader->meta, &shader->program_id);
	shader->program_id = load_shaders(shader_directory,
				"black_hole.vert", "black_hole.frag", UNIVERSAL_SHADER_HEADER);
	graph_dev_gen_vao(&shader->vao_id);

	shader->mvp_matrix_id = glGetUniformLocation(shader->program_id, "u_MVPMatrix");
	shader->vertex_position_id = glGetAttribLocation(shader->program_id, "a_Position");
	shader->texture_coord_id = glGetAttribLocation(shader->program_id, "a_TexCoord");
	shader->disc_radius_id = glGetUniformLocation(shader->program_id, "u_DiscRadius");
	shader->edge_softness_id = glGetUniformLocation(shader->program_id, "u_EdgeSoftness");
	shader->ring_brightness_id = glGetUniformLocation(shader->program_id, "u_RingBrightness");
	shader->ring_width_id = glGetUniformLocation(shader->program_id, "u_RingWidth");
	shader->einstein_radius_id = glGetUniformLocation(shader->program_id, "u_EinsteinRadius");
	shader->glow_brightness_id = glGetUniformLocation(shader->program_id, "u_GlowBrightness");
	shader->glow_width_id = glGetUniformLocation(shader->program_id, "u_GlowWidth");
	shader->ring_color_id = glGetUniformLocation(shader->program_id, "u_RingColor");
}

void setup_exhaust_plume_shader(struct graph_dev_gl_exhaust_plume_shader *shader)
{
	maybe_unload_shader(&shader->meta, &shader->program_id);
	shader->program_id = load_shaders(shader_directory,
				"exhaust-plume.vert", "exhaust-plume.frag",
				UNIVERSAL_SHADER_HEADER FILMIC_TONEMAPPING);
	graph_dev_gen_vao(&shader->vao_id);

	shader->mvp_matrix_id = glGetUniformLocation(shader->program_id, "u_MVPMatrix");
	shader->mv_matrix_id = glGetUniformLocation(shader->program_id, "u_MVMatrix");
	shader->vertex_position_id = glGetAttribLocation(shader->program_id, "a_Position");
	shader->vertex_normal_id = glGetAttribLocation(shader->program_id, "a_Normal");
	shader->texture_coord_id = glGetAttribLocation(shader->program_id, "a_TexCoord");
	shader->tint_color_id = glGetUniformLocation(shader->program_id, "u_TintColor");
	shader->core_brightness_id = glGetUniformLocation(shader->program_id, "u_CoreBrightness");
	shader->plume_length_id = glGetUniformLocation(shader->program_id, "u_PlumeLength");
	shader->noise_seed_id = glGetUniformLocation(shader->program_id, "u_NoiseSeed");
	shader->diamond_spacing_id = glGetUniformLocation(shader->program_id, "u_DiamondSpacing");
	shader->diamond_intensity_id = glGetUniformLocation(shader->program_id, "u_DiamondIntensity");
	shader->filmic_tonemapping_id = glGetUniformLocation(shader->program_id, "u_FilmicTonemapping");
	shader->tonemapping_gain_id = glGetUniformLocation(shader->program_id, "u_TonemappingGain");
}

/* One of a ship's death's .shader files -- both stages in one, picked by INCLUDE_VS and
 * INCLUDE_FS -- with the filmic tonemapper in front of it. */
static GLuint load_ship_death_shader(const char *filename)
{
	const char *vert_header = UNIVERSAL_SHADER_HEADER "#define INCLUDE_VS 1\n"
					FILMIC_TONEMAPPING;
	const char *frag_header = UNIVERSAL_SHADER_HEADER "#define INCLUDE_FS 1\n"
					FILMIC_TONEMAPPING;
	const char *filenames[] = { filename };

	return load_concat_shaders(shader_directory, vert_header, 1, filenames,
				frag_header, 1, filenames);
}

void setup_shrapnel_shader(struct graph_dev_gl_shrapnel_shader *shader)
{
	GLuint id;

	maybe_unload_shader(&shader->meta, &shader->program_id);
	id = load_ship_death_shader("shrapnel.shader");
	shader->program_id = id;
	graph_dev_gen_vao(&shader->vao_id);

	shader->mvp_matrix_id = glGetUniformLocation(id, "u_MVPMatrix");
	shader->model_matrix_id = glGetUniformLocation(id, "u_ModelMatrix");
	shader->vertex_position_id = glGetAttribLocation(id, "a_Position");
	shader->vertex_normal_id = glGetAttribLocation(id, "a_Normal");
	shader->light_pos_id = glGetUniformLocation(id, "u_LightPos");
	shader->star_tint_id = glGetUniformLocation(id, "u_StarTint");
	shader->ambient_id = glGetUniformLocation(id, "u_Ambient");
	shader->blackbody_id = glGetUniformLocation(id, "u_Blackbody");
	shader->temperature_id = glGetUniformLocation(id, "u_Temperature");
	shader->brightness_id = glGetUniformLocation(id, "u_Brightness");
	shader->albedo_id = glGetUniformLocation(id, "u_Albedo");
	shader->filmic_tonemapping_id = glGetUniformLocation(id, "u_FilmicTonemapping");
	shader->tonemapping_gain_id = glGetUniformLocation(id, "u_TonemappingGain");
}

void setup_wreck_shader(struct graph_dev_gl_wreck_shader *shader)
{
	GLuint id;

	maybe_unload_shader(&shader->meta, &shader->program_id);
	id = load_ship_death_shader("wreck.shader");
	shader->program_id = id;
	graph_dev_gen_vao(&shader->vao_id);

	shader->mvp_matrix_id = glGetUniformLocation(id, "u_MVPMatrix");
	shader->model_matrix_id = glGetUniformLocation(id, "u_ModelMatrix");
	shader->vertex_position_id = glGetAttribLocation(id, "a_Position");
	shader->vertex_normal_id = glGetAttribLocation(id, "a_Normal");
	shader->texture_coord_id = glGetAttribLocation(id, "a_TexCoord");
	shader->edge_id = glGetAttribLocation(id, "a_Edge");
	shader->light_pos_id = glGetUniformLocation(id, "u_LightPos");
	shader->star_tint_id = glGetUniformLocation(id, "u_StarTint");
	shader->ambient_id = glGetUniformLocation(id, "u_Ambient");
	shader->aux_light_pos_id = glGetUniformLocation(id, "u_AuxLightPos");
	shader->aux_light_color_id = glGetUniformLocation(id, "u_AuxLightColor");
	shader->aux_light_wrap_id = glGetUniformLocation(id, "u_AuxLightWrap");
	shader->albedo_id = glGetUniformLocation(id, "u_Albedo");
	shader->have_texture_id = glGetUniformLocation(id, "u_HaveTexture");
	shader->blackbody_id = glGetUniformLocation(id, "u_Blackbody");
	shader->interior_id = glGetUniformLocation(id, "u_Interior");
	shader->scorch_id = glGetUniformLocation(id, "u_Scorch");
	shader->edge_width_id = glGetUniformLocation(id, "u_EdgeWidth");
	shader->edge_temp_id = glGetUniformLocation(id, "u_EdgeTemp");
	shader->edge_brightness_id = glGetUniformLocation(id, "u_EdgeBrightness");
	shader->hull_radius_id = glGetUniformLocation(id, "u_HullRadius");
	shader->dissolve_id = glGetUniformLocation(id, "u_Dissolve");
	shader->burn_glow_id = glGetUniformLocation(id, "u_BurnGlow");
	shader->preheat_id = glGetUniformLocation(id, "u_Preheat");
	shader->time_id = glGetUniformLocation(id, "u_Time");
	shader->filmic_tonemapping_id = glGetUniformLocation(id, "u_FilmicTonemapping");
	shader->tonemapping_gain_id = glGetUniformLocation(id, "u_TonemappingGain");
}

void setup_particles_shader(struct graph_dev_gl_particles_shader *shader)
{
	GLuint id;

	maybe_unload_shader(&shader->meta, &shader->program_id);
	id = load_ship_death_shader("particles.shader");
	shader->program_id = id;
	graph_dev_gen_vao(&shader->vao_id);

	shader->mvp_matrix_id = glGetUniformLocation(id, "u_MVPMatrix");
	shader->vertex_position_id = glGetAttribLocation(id, "a_Position");
	shader->vertex_normal_id = glGetAttribLocation(id, "a_Normal");
	shader->texture_coord_id = glGetAttribLocation(id, "a_TexCoord");
	shader->edge_id = glGetAttribLocation(id, "a_Edge");
	shader->cam_right_id = glGetUniformLocation(id, "u_CamRight");
	shader->cam_up_id = glGetUniformLocation(id, "u_CamUp");
	shader->cam_back_id = glGetUniformLocation(id, "u_CamBack");
	shader->light_pos_id = glGetUniformLocation(id, "u_LightPos");
	shader->star_tint_id = glGetUniformLocation(id, "u_StarTint");
	shader->ambient_id = glGetUniformLocation(id, "u_Ambient");
	shader->albedo_id = glGetUniformLocation(id, "u_Albedo");
	shader->time_id = glGetUniformLocation(id, "u_Time");
	shader->blackbody_id = glGetUniformLocation(id, "u_Blackbody");
	shader->filmic_tonemapping_id = glGetUniformLocation(id, "u_FilmicTonemapping");
	shader->tonemapping_gain_id = glGetUniformLocation(id, "u_TonemappingGain");
}

void setup_single_color_shader(struct graph_dev_gl_single_color_shader *shader)
{
	maybe_unload_shader(&shader->meta, &shader->program_id);
	/* Create and compile our GLSL program from the shaders */
	shader->program_id = load_shaders(shader_directory,
				"single_color.vert", "single_color.frag",
				UNIVERSAL_SHADER_HEADER);
	/* create the VAO for this shader */
	graph_dev_gen_vao(&shader->vao_id);

	/* Get a handle for our "MVP" uniform */
	shader->mvp_matrix_id = glGetUniformLocation(shader->program_id, "u_MVPMatrix");

	/* Get a handle for our buffers */
	shader->vertex_position_id = glGetAttribLocation(shader->program_id, "a_Position");
	shader->color_id = glGetUniformLocation(shader->program_id, "u_Color");
}


void setup_vertex_color_shader(struct graph_dev_gl_vertex_color_shader *shader)
{
	maybe_unload_shader(&shader->meta, &shader->program_id);
	shader->program_id = load_shaders(shader_directory,
				"per_vertex_color.vert", "per_vertex_color.frag",
				UNIVERSAL_SHADER_HEADER);
	/* create the VAO for this shader */
	graph_dev_gen_vao(&shader->vao_id);

	shader->mvp_matrix_id = glGetUniformLocation(shader->program_id, "u_MVPMatrix");

	shader->vertex_position_id = glGetAttribLocation(shader->program_id, "a_Position");
	shader->vertex_color_id = glGetAttribLocation(shader->program_id, "a_Color");
}
