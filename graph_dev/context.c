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

/* See graph_dev/context.h. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#ifdef USE_GLES
#include <glad/gles2.h>
#else
#include <glad/gl.h>
#endif

#include "mathutils.h"
#include "quat.h"
#include "graph_dev.h"
#include "context.h"
#include "snis_graph.h"
#include "material.h"
#include "entity.h"
#include "entity_private.h"
#include "snis_typeface.h"
#include "snis_profile.h"

struct graph_dev_gl_context sgc;
char shader_directory[PATH_MAX];

void setup_cubemap_cube(struct graph_dev_primitive *obj)
{
	/* cube vertices in triangle strip for vertex buffer object */
	static const struct vertex_buffer_data cube_v_data[] = {
		{ .position = { { -10.0f,  10.0f, -10.0f } } },
		{ .position = { { -10.0f, -10.0f, -10.0f } } },
		{ .position = { { 10.0f, -10.0f, -10.0f } } },
		{ .position = { { 10.0f, -10.0f, -10.0f } } },
		{ .position = { { 10.0f,  10.0f, -10.0f } } },
		{ .position = { { -10.0f,  10.0f, -10.0f } } },

		{ .position = { { -10.0f, -10.0f,  10.0f } } },
		{ .position = { { -10.0f, -10.0f, -10.0f } } },
		{ .position = { { -10.0f,  10.0f, -10.0f } } },
		{ .position = { { -10.0f,  10.0f, -10.0f } } },
		{ .position = { { -10.0f,  10.0f,  10.0f } } },
		{ .position = { { -10.0f, -10.0f,  10.0f } } },

		{ .position = { { 10.0f, -10.0f, -10.0f } } },
		{ .position = { { 10.0f, -10.0f,  10.0f } } },
		{ .position = { { 10.0f,  10.0f,  10.0f } } },
		{ .position = { { 10.0f,  10.0f,  10.0f } } },
		{ .position = { { 10.0f,  10.0f, -10.0f } } },
		{ .position = { { 10.0f, -10.0f, -10.0f } } },

		{ .position = { { -10.0f, -10.0f,  10.0f } } },
		{ .position = { { -10.0f,  10.0f,  10.0f } } },
		{ .position = { { 10.0f,  10.0f,  10.0f } } },
		{ .position = { { 10.0f,  10.0f,  10.0f } } },
		{ .position = { { 10.0f, -10.0f,  10.0f } } },
		{ .position = { { -10.0f, -10.0f,  10.0f } } },

		{ .position = { { -10.0f,  10.0f, -10.0f } } },
		{ .position = { { 10.0f,  10.0f, -10.0f } } },
		{ .position = { { 10.0f,  10.0f,  10.0f } } },
		{ .position = { { 10.0f,  10.0f,  10.0f } } },
		{ .position = { { -10.0f,  10.0f,  10.0f } } },
		{ .position = { { -10.0f,  10.0f, -10.0f } } },

		{ .position = { { -10.0f, -10.0f, -10.0f } } },
		{ .position = { { -10.0f, -10.0f,  10.0f } } },
		{ .position = { { 10.0f, -10.0f, -10.0f } } },
		{ .position = { { 10.0f, -10.0f, -10.0f } } },
		{ .position = { { -10.0f, -10.0f,  10.0f } } },
		{ .position = { { 10.0f, -10.0f,  10.0f } } } };

	glGenBuffers(1, &obj->vertex_buffer);
	glBindBuffer(GL_ARRAY_BUFFER, obj->vertex_buffer);
	glBufferData(GL_ARRAY_BUFFER, sizeof(cube_v_data), cube_v_data, GL_STATIC_DRAW);

	obj->nvertices = sizeof(cube_v_data)/sizeof(struct vertex_buffer_data);
}


void setup_textured_unit_quad(struct graph_dev_primitive *obj)
{
	static const struct vertex_buffer_data quad_v_data[] = {
		{ { { -1.0f, -1.0f, 0.0f } } },
		{ { { 1.0f, 1.0f, 0.0f } } },
		{ { { -1.0f, 1.0f, 0.0f } } },

		{ { { -1.0f, -1.0f, 0.0f } } },
		{ { { 1.0f, -1.0f, 0.0f } } },
		{ { { 1.0f, 1.0f, 0.0f } } } };

	static const struct vertex_triangle_buffer_data quad_vt_data[] = {
		{ .texture_coord = { { 0.0f, 0.0f } } },
		{ .texture_coord = { { 1.0f, 1.0f } } },
		{ .texture_coord = { { 0.0f, 1.0f } } },

		{ .texture_coord = { { 0.0f, 0.0f } } },
		{ .texture_coord = { { 1.0f, 0.0f } } },
		{ .texture_coord = { { 1.0f, 1.0f } } } };

	glGenBuffers(1, &obj->vertex_buffer);
	glBindBuffer(GL_ARRAY_BUFFER, obj->vertex_buffer);
	glBufferData(GL_ARRAY_BUFFER, sizeof(quad_v_data), quad_v_data, GL_STATIC_DRAW);

	glGenBuffers(1, &obj->triangle_vertex_buffer);
	glBindBuffer(GL_ARRAY_BUFFER, obj->triangle_vertex_buffer);
	glBufferData(GL_ARRAY_BUFFER, sizeof(quad_vt_data), quad_vt_data, GL_STATIC_DRAW);

	obj->nvertices = sizeof(quad_v_data)/sizeof(struct vertex_buffer_data);
}


void maybe_unload_shader(struct graph_dev_gl_shader_metadata *meta, GLuint *program_id)
{
	PROFILE_ZONE_START("maybe_unload_shader");
	if (meta->program_id && *meta->program_id != (GLuint) -1) /* Shader is currently loaded? */
		glDeleteProgram(*meta->program_id); /* Unload shader */
	meta->program_id = program_id;
	*meta->program_id = -1;
	PROFILE_ZONE_END();
}







extern int graph_dev_entity_render_order(struct entity *e)
{
	int does_blending = 0;

	if (!e->material_ptr)
		return GRAPH_DEV_RENDER_NEAR_TO_FAR;

	switch (e->material_ptr->type) {
	case MATERIAL_NEBULA:
	case MATERIAL_TEXTURED_PARTICLE:
	case MATERIAL_TEXTURED_PLANET_RING:
	case MATERIAL_TEXTURED_SHIELD:
	case MATERIAL_ALPHA_BY_NORMAL:
	case MATERIAL_PLANETARY_LIGHTNING:
	case MATERIAL_WARP_GATE_EFFECT:
	case MATERIAL_SUN:
	case MATERIAL_BLACK_HOLE:
	case MATERIAL_CITY:
	case MATERIAL_EXHAUST_PLUME:
		does_blending = 1;
		break;
	case MATERIAL_TEXTURE_MAPPED_UNLIT:
		does_blending = e->material_ptr->texture_mapped_unlit.do_blend;
		break;
	case MATERIAL_TEXTURE_CUBEMAP:
		does_blending = e->material_ptr->texture_cubemap.do_blend;
		break;
	}

	if (does_blending)
		return GRAPH_DEV_RENDER_FAR_TO_NEAR;
	else
		return GRAPH_DEV_RENDER_NEAR_TO_FAR;
}


void graph_dev_clear_depth_bit(void)
{
	PROFILE_ZONE_START("graph_dev_clear_depth_bit");
	glClear(GL_DEPTH_BUFFER_BIT);
	PROFILE_ZONE_END();
}


void graph_dev_clear_window(void)
{
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT);
}


void graph_dev_compute_star_light(const struct entity_context *cx,
	float light_color[3], float ambient_color[3])
{
	star_light_colors(cx->star_color, cx->ambient, cx->star_light_tint,
		cx->star_dark_tint, cx->star_shadow_darkening, light_color, ambient_color);
}


void note_texture_bound_outside_cache(GLuint tex_id)
{
	sgc.texture_unit_bind[sgc.texture_unit_active] = tex_id;
}


void graph_dev_set_screen_size(int width, int height)
{
	sgc.active_vp = 0;
	sgc.screen_x = width;
	sgc.screen_y = height;
}


void graph_dev_set_extent_scale(float x_scale, float y_scale)
{
	sgc.x_scale = x_scale;
	sgc.y_scale = y_scale;
}


void graph_dev_set_3d_viewport(int x_offset, int y_offset, int width, int height)
{
	sgc.active_vp = 0;
	sgc.vp_x_3d = x_offset;
	sgc.vp_y_3d = sgc.screen_y - height - y_offset;
	sgc.vp_width_3d = width;
	sgc.vp_height_3d = height;
}


void graph_dev_grab_framebuffer(unsigned char **buffer, int *width, int *height)
{
	*buffer = malloc(4 * sgc.screen_x * sgc.screen_y);
	*width = sgc.screen_x;
	*height = sgc.screen_y;
	glReadPixels(0, 0, sgc.screen_x, sgc.screen_y,
			GL_RGBA, GL_UNSIGNED_BYTE, *buffer);
}


void graph_dev_set_color(struct graph_dev_color *color, float a)
{
	sgc.hue = color;

	if (a >= 0) {
		sgc.alpha_blend = 1;
		sgc.alpha = a;
	} else {
		sgc.alpha_blend = 0;
	}
}


void make_room_in_vertex_buffer_2d(int nvertices)
{
	PROFILE_ZONE_START("make_room_in_vertex_buffer_2d");
	if (sgc.nvertex_2d + nvertices > BUFFERED_VERTICES_2D) {
		/* buffer needs to be emptied to fit next batch */
		draw_vertex_buffer_2d();
	}
	PROFILE_ZONE_END();
}


void add_vertex_2d(float x, float y, struct graph_dev_color *color, GLubyte alpha, GLenum mode)
{
	PROFILE_ZONE_START("add_vertex_2d");
	struct vertex_color_buffer_data *vertex = &sgc.vertex_data_2d[sgc.nvertex_2d];

	/* setup the vertex and color */
	vertex->position[0] = x;
	vertex->position[1] = sgc.screen_y - y;

	vertex->color[0] = color->red >> 8;
	vertex->color[1] = color->green >> 8;
	vertex->color[2] = color->blue >> 8;
	vertex->color[3] = alpha;

	sgc.vertex_type_2d[sgc.nvertex_2d] = mode;

	sgc.nvertex_2d += 1;
	PROFILE_ZONE_END();
}


void graph_dev_draw_point(float x, float y)
{
	PROFILE_ZONE_START("graph_dev_draw_point");
	make_room_in_vertex_buffer_2d(1);

	add_vertex_2d(x, y, sgc.hue, 255, GL_POINTS);
	PROFILE_ZONE_END();
}


void graph_dev_draw_line(float x1, float y1, float x2, float y2)
{
	PROFILE_ZONE_START("graph_dev_draw_line");
	make_room_in_vertex_buffer_2d(2);

	add_vertex_2d(x1, y1, sgc.hue, 255, GL_LINES);
	add_vertex_2d(x2, y2, sgc.hue, 255, GL_LINES);
	PROFILE_ZONE_END();
}


void graph_dev_draw_arc(int filled, float x, float y, float width, float height, float angle1, float angle2)
{
	PROFILE_ZONE_START("graph_dev_draw_arc");
	float max_angle_delta = 2.0 * M_PI / 180.0; /*some ratio to height and width? */
	float rx = width/2.0;
	float ry = height/2.0;
	float cx = x + rx;
	float cy = y + ry;

	int i;

	int segments = (int)((angle2 - angle1) / max_angle_delta) + 1;
	float delta = (angle2 - angle1) / segments;

	GLubyte alpha = 255;

	if (sgc.alpha_blend) {
		/* must empty the vertex buffer to draw this primitive with blending */
		draw_vertex_buffer_2d();

		glEnable(GL_BLEND);
		BLEND_FUNC(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		alpha = 255 * sgc.alpha;
	}

	if (filled)
		make_room_in_vertex_buffer_2d(segments * 3);
	else
		make_room_in_vertex_buffer_2d(segments + 1);

	float x1 = 0, y1 = 0;
	for (i = 0; i <= segments; i++) {
		float a = angle1 + delta * (float)i;
		float x2 = cx + cos(a) * rx;
		float y2 = cy + sin(a) * ry;

		if (!filled || i > 0) {
			if (filled) {
				add_vertex_2d(x2, y2, sgc.hue, alpha, GL_TRIANGLES);
				add_vertex_2d(x1, y1, sgc.hue, alpha, GL_TRIANGLES);
				add_vertex_2d(cx, cy, sgc.hue, alpha, GL_TRIANGLES);
			} else {
				add_vertex_2d(x2, y2, sgc.hue, alpha, (i != segments ? GL_LINE_STRIP : -1));
			}
		}
		x1 = x2;
		y1 = y2;
	}

	if (sgc.alpha_blend) {
		/* must draw the vertex buffer to complete the blending */
		draw_vertex_buffer_2d();

		glDisable(GL_BLEND);
	}

	PROFILE_ZONE_END();
}


void graph_dev_draw_3d_line(__attribute__((unused)) struct entity_context *cx, const struct mat44 *mat_vp,
	float x1, float y1, float z1, float x2, float y2, float z2)
{
	PROFILE_ZONE_START("graph_dev_draw_3d_line");

	draw_vertex_buffer_2d();

	enable_3d_viewport();

	/* setup fake line entity to render this */
	struct vertex_buffer_data g_v_buffer_data[2];
	g_v_buffer_data[0].position.v.x = x1;
	g_v_buffer_data[0].position.v.y = y1;
	g_v_buffer_data[0].position.v.z = z1;
	g_v_buffer_data[1].position.v.x = x2;
	g_v_buffer_data[1].position.v.y = y2;
	g_v_buffer_data[1].position.v.z = z2;

	struct vertex_line_buffer_data g_vl_buffer_data[2];

	int is_dotted = 0;

	g_vl_buffer_data[0].multi_one[0] =
		g_vl_buffer_data[1].multi_one[0] = is_dotted ? 255 : 0;

	g_vl_buffer_data[0].line_vertex0.v.x =
		g_vl_buffer_data[1].line_vertex0.v.x = x1;
	g_vl_buffer_data[0].line_vertex0.v.y =
		g_vl_buffer_data[1].line_vertex0.v.y = y1;
	g_vl_buffer_data[0].line_vertex0.v.z =
		g_vl_buffer_data[1].line_vertex0.v.z = z1;

	g_vl_buffer_data[0].line_vertex1.v.x =
		g_vl_buffer_data[1].line_vertex1.v.x = x2;
	g_vl_buffer_data[0].line_vertex1.v.y =
		g_vl_buffer_data[1].line_vertex1.v.y = y2;
	g_vl_buffer_data[0].line_vertex1.v.z =
		g_vl_buffer_data[1].line_vertex1.v.z = z2;

	sgc.gl_info_3d_line.nlines = 1;

	glBindBuffer(GL_ARRAY_BUFFER, sgc.gl_info_3d_line.vertex_buffer);
	glBufferData(GL_ARRAY_BUFFER, sizeof(g_v_buffer_data), g_v_buffer_data, GL_STREAM_DRAW);

	glBindBuffer(GL_ARRAY_BUFFER, sgc.gl_info_3d_line.line_vertex_buffer);
	glBufferData(GL_ARRAY_BUFFER, sizeof(g_vl_buffer_data), g_vl_buffer_data, GL_STREAM_DRAW);

	struct mesh m;
	m.graph_ptr = &sgc.gl_info_3d_line;

	struct entity e;
	e.material_ptr = 0;
	e.m = &m;

	struct sng_color line_color = sng_get_foreground();
	graph_dev_raster_line_mesh(&e, mat_vp, &m, &line_color);

	PROFILE_ZONE_END();
}


int selected_debug_item_checkbox(int n, int x, int y, int *toggle)
{
	if (x > 15 && x < 35 && y >= 35 + n * 20 && y <= 50 + n * 20) {
		if (toggle)
			*toggle = !*toggle;
		return 1;
	}
	return 0;
}


void debug_menu_draw_item(char *item, int itemnumber, int grayed, int checked)
{
	int x = 15;
	int y = 35 + itemnumber * 20;

	if (grayed)
		sng_set_foreground(GRAY75);
	else
		sng_set_foreground(WHITE);

	graph_dev_draw_rectangle(0, x, y, 15, 15);
	if (checked)
		graph_dev_draw_rectangle(1, x + 2, y + 2, 11, 11);
	sng_abs_xy_draw_string(item, NANO_FONT, (x + 20) / sgc.x_scale, (y + 10) / sgc.y_scale);
}

void graph_dev_draw_rectangle(int filled, float x, float y, float width, float height)
{
	PROFILE_ZONE_START("graph_dev_draw_rectangle");

	int x2, y2;
	GLubyte alpha = 255;

	x2 = x + width;
	y2 = y + height;

	glDisable(GL_DEPTH_TEST);
	if (sgc.alpha_blend) {
		/* must empty the vertex buffer to draw this primitive with blending */
		draw_vertex_buffer_2d();

		glEnable(GL_BLEND);
		BLEND_FUNC(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		alpha = 255 * sgc.alpha;
	}

	if (filled) {
		/* filled rectangle with two triangles
		  0 ------- 1
		    |\  1 |
		    | \   |
		    |  \  |
		    |   \ |
		    | 2  \|
		  2 ------- 3
		*/

		make_room_in_vertex_buffer_2d(6);

		/* triangle 1 = 0, 3, 1 */
		add_vertex_2d(x, y, sgc.hue, alpha, GL_TRIANGLES);
		add_vertex_2d(x2, y2, sgc.hue, alpha, GL_TRIANGLES);
		add_vertex_2d(x2, y, sgc.hue, alpha, GL_TRIANGLES);

		/* triangle 2 = 0, 2, 3 */
		add_vertex_2d(x, y, sgc.hue, alpha, GL_TRIANGLES);
		add_vertex_2d(x, y2, sgc.hue, alpha, GL_TRIANGLES);
		add_vertex_2d(x2, y2, sgc.hue, alpha, GL_TRIANGLES);
	} else {
		/* not filled */
		make_room_in_vertex_buffer_2d(5);

		add_vertex_2d(x, y, sgc.hue, alpha, GL_LINE_STRIP);
		add_vertex_2d(x2, y, sgc.hue, alpha, GL_LINE_STRIP);
		add_vertex_2d(x2, y2, sgc.hue, alpha, GL_LINE_STRIP);
		add_vertex_2d(x, y2, sgc.hue, alpha, GL_LINE_STRIP);
		add_vertex_2d(x, y, sgc.hue, alpha, -1 /* primitive end */);
	}

	if (sgc.alpha_blend) {
		/* must draw the vertex buffer to complete the blending */
		draw_vertex_buffer_2d();

		glDisable(GL_BLEND);
	}
	PROFILE_ZONE_END();
}
