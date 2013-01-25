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

#ifdef DEFINE_ENTITY_GLOBALS
#define GLOBAL
#else
#define GLOBAL extern
#endif

GLOBAL struct entity *add_entity(struct mesh *m, float x, float y, float z);
GLOBAL void wireframe_render_entity(GtkWidget *w, GdkGC *gc, struct entity *e);
GLOBAL void render_entity(GtkWidget *w, GdkGC *gc, struct entity *e);
GLOBAL void render_entities(GtkWidget *w, GdkGC *gc);
GLOBAL void camera_set_pos(float x, float y, float z);
GLOBAL void camera_look_at(float x, float y, float z);
GLOBAL void camera_set_parameters(float near, float far, float width, float height,
					int xvpixels, int yvpixels);

#endif	
