/*
        Copyright (C) 2010 Stephen M. Cameron
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
#ifndef ENTITY_H__
#define ENTITY_H__

struct entity;
struct entity_context;
struct mat44;

#ifdef DEFINE_ENTITY_GLOBALS
#define GLOBAL
#else
#define GLOBAL extern
#endif

GLOBAL struct entity_context *entity_context_new(int maxobjs, int maxchildren);
GLOBAL struct entity *add_entity(struct entity_context *cx,
	struct mesh *m, float x, float y, float z, int color);
GLOBAL void remove_entity(struct entity_context *cx,
	struct entity *e);
GLOBAL void remove_all_entity(struct entity_context *cx);
GLOBAL void update_entity_parent(struct entity_context *cx, struct entity *child, struct entity *parent);
GLOBAL void update_entity_pos(struct entity *e, float x, float y, float z);
GLOBAL void update_entity_orientation(struct entity *e, const union quat *orientation);
GLOBAL union quat *entity_get_orientation(struct entity *e);
GLOBAL void entity_get_pos(struct entity *e, float *x, float *y, float *z);
GLOBAL float entity_get_scale(struct entity *e);
GLOBAL void update_entity_scale(struct entity *e, float scale);
GLOBAL void entity_get_non_uniform_scale(struct entity *e, float *x_scale, float *y_scale, float *z_scale);
GLOBAL void update_entity_non_uniform_scale(struct entity *e, float x_scale, float y_scale, float z_scale);
GLOBAL void update_entity_color(struct entity *e, int color);
GLOBAL void update_entity_shadecolor(struct entity *e, int color);
GLOBAL void update_entity_visibility(struct entity *e, int visible);
GLOBAL struct mesh *entity_get_mesh(struct entity *e);
GLOBAL void entity_set_mesh(struct entity *e, struct mesh *m);
GLOBAL void render_entities(struct entity_context *cx);
GLOBAL void render_line(struct entity_context *cx, float x1, float y1, float z1, float x2, float y2, float z2);
GLOBAL void render_skybox(struct entity_context *cx);
GLOBAL void camera_set_pos(struct entity_context *cx, float x, float y, float z);
GLOBAL void camera_get_pos(struct entity_context *cx, float *x, float *y, float *z);
GLOBAL void camera_look_at(struct entity_context *cx, float x, float y, float z);
GLOBAL void camera_get_look_at(struct entity_context *cx, float *x, float *y, float *z);
GLOBAL void camera_assign_up_direction(struct entity_context *cx, float x, float y, float z);
GLOBAL void camera_get_up_direction(struct entity_context *cx, float *x, float *y, float *z);
GLOBAL void camera_set_parameters(struct entity_context *cx,
		float near, float far, int xvpixels, int yvpixels, float angle_of_view);
GLOBAL void camera_get_parameters(struct entity_context *cx,
		float *near, float *far, int *xvpixels, int *yvpixels, float *angle_of_view);
GLOBAL void camera_set_orientation(struct entity_context *cx, union quat *q);
GLOBAL void set_lighting(struct entity_context *cx, double x, double y, double z);
GLOBAL void set_ambient_light(struct entity_context *cx, float ambient);
GLOBAL void entity_init_fake_stars(struct entity_context *cx, int nstars, float radius);
GLOBAL void entity_free_fake_stars(struct entity_context *cx);
GLOBAL void set_renderer(struct entity_context *cx, int renderer);
GLOBAL int get_renderer(struct entity_context *cx);
#define WIREFRAME_RENDERER (1 << 0)
#define FLATSHADING_RENDERER (1 << 1)
#define BLACK_TRIS (1 << 2)
GLOBAL void calculate_camera_transform(struct entity_context *cx);
GLOBAL struct mat44d get_camera_v_transform(struct entity_context *cx);
GLOBAL struct mat44d get_camera_vp_transform(struct entity_context *cx);
GLOBAL int transform_vertices(const struct mat44 *matrix, struct vertex *v, int len);
GLOBAL int transform_point(struct entity_context *cx, float x, float y, float z, float *sx, float *sy);
GLOBAL int transform_line(struct entity_context *cx, float x1, float y1, float z1,
	float x2, float y2, float z2, float *sx1, float *sy1, float *sx2, float *sy2);

GLOBAL void set_render_style(struct entity *e, int render_style);
GLOBAL void entity_context_free(struct entity_context *cx);
#define RENDER_NORMAL 0
#define RENDER_WIREFRAME (1 << 1)
#define RENDER_BRIGHT_LINE (1 << 2)
#define RENDER_NO_FILL (1 << 3)
#define RENDER_SPARKLE (1 << 4)
#define RENDER_ILDA (1 << 5) /* for laser projectors */
GLOBAL void entity_get_screen_coords(struct entity *e, float *x, float *y);

GLOBAL int get_entity_count(struct entity_context *cx);
GLOBAL struct entity *get_entity(struct entity_context *cx, int n);
GLOBAL void entity_set_user_data(struct entity *e, void *user_data);
GLOBAL void *entity_get_user_data(struct entity *e);
GLOBAL void set_window_offset(struct entity_context *cx, float x, float y);
GLOBAL int entity_onscreen(struct entity *e);
GLOBAL void update_entity_material(struct entity *e, struct material *material_ptr);
GLOBAL struct material *entity_get_material(struct entity *e);
GLOBAL float entity_get_alpha(struct entity *e);
GLOBAL void entity_update_alpha(struct entity *e, float alpha);
GLOBAL void entity_update_emit_intensity(struct entity *e, float intensity);
GLOBAL float entity_get_emit_intensity(struct entity *e);
GLOBAL float entity_get_in_shade(struct entity *e);
GLOBAL void entity_set_in_shade(struct entity *e, float in_shade);
GLOBAL void entity_set_ambient(struct entity *e, float ambient);

#endif	
