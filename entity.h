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

#ifdef DEFINE_ENTITY_GLOBALS
#define GLOBAL
#else
#define GLOBAL extern
#endif

GLOBAL struct entity_context *entity_context_new(int maxobjs);
GLOBAL struct entity *add_entity(struct entity_context *cx,
	struct mesh *m, float x, float y, float z, int color);
GLOBAL void remove_entity(struct entity_context *cx,
	struct entity *e);
GLOBAL void remove_all_entity(struct entity_context *cx);
GLOBAL void update_entity_pos(struct entity *e, float x, float y, float z);
GLOBAL void update_entity_rotation(struct entity *e, float rx, float ry, float rz);
GLOBAL float entity_get_scale(struct entity *e);
GLOBAL void update_entity_scale(struct entity *e, float scale);
GLOBAL void update_entity_color(struct entity *e, int color);
GLOBAL void update_entity_shadecolor(struct entity *e, int color);
GLOBAL struct mesh *entity_get_mesh(struct entity *e);
GLOBAL void entity_set_mesh(struct entity *e, struct mesh *m);
GLOBAL void wireframe_render_entity(GtkWidget *w, GdkGC *gc,
		struct entity_context *cx, struct entity *e);
GLOBAL void wireframe_render_point_cloud(GtkWidget *w, GdkGC *gc,
		struct entity_context *cx, struct entity *e);
GLOBAL void wireframe_render_point_line(GtkWidget *w, GdkGC *gc, struct entity_context *cx,
		struct entity *e);
GLOBAL void render_entity(GtkWidget *w, GdkGC *gc,
		struct entity_context *cx, struct entity *e);
GLOBAL void render_entities(GtkWidget *w, GdkGC *gc, struct entity_context *cx);
GLOBAL void camera_set_pos(struct entity_context *cx, float x, float y, float z);
GLOBAL void camera_look_at(struct entity_context *cx, float x, float y, float z);
GLOBAL void camera_get_look_at(struct entity_context *cx, float *x, float *y, float *z);
GLOBAL void camera_assign_up_direction(struct entity_context *cx, float x, float y, float z);
GLOBAL void camera_get_up_direction(struct entity_context *cx, float *x, float *y, float *z);
GLOBAL void camera_set_parameters(struct entity_context *cx,
		float near, float far, int xvpixels, int yvpixels, float angle_of_view);
GLOBAL void camera_get_parameters(struct entity_context *cx,
		float *near, float *far, int *xvpixels, int *yvpixels, float *angle_of_view);
GLOBAL void set_lighting(struct entity_context *cx, double x, double y, double z);
GLOBAL void entity_init_fake_stars(struct entity_context *cx, int nstars, float radius);
GLOBAL void entity_free_fake_stars(struct entity_context *cx);
GLOBAL void set_renderer(struct entity_context *cx, int renderer);
GLOBAL int get_renderer(struct entity_context *cx);
#define WIREFRAME_RENDERER (1 << 0)
#define FLATSHADING_RENDERER (1 << 1)
#define BLACK_TRIS (1 << 2)

GLOBAL void set_render_style(struct entity *e, int render_style);
GLOBAL void entity_context_free(struct entity_context *cx);
#define RENDER_NORMAL 0
#define RENDER_POINT_CLOUD (1 << 0)
#define RENDER_WIREFRAME (1 << 1)
#define RENDER_BRIGHT_LINE (1 << 2)
#define RENDER_NO_FILL (1 << 3)
#define RENDER_SPARKLE (1 << 4)
#define RENDER_ILDA (1 << 5) /* for laser projectors */
#define RENDER_POINT_LINE (1 << 6)
#define RENDER_DISABLE_CLIP (1 << 7)
GLOBAL void entity_get_screen_coords(struct entity *e, float *x, float *y);

GLOBAL int get_entity_count(struct entity_context *cx);
GLOBAL struct entity *get_entity(struct entity_context *cx, int n);
GLOBAL void entity_set_user_data(struct entity *e, void *user_data);
GLOBAL void *entity_get_user_data(struct entity *e);

#endif	
