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
#include <stdio.h>
#include <math.h>
#include <gtk/gtk.h>

#include "mathutils.h"
#include "vertex.h"
#include "triangle.h"
#include "mesh.h"
#include "stl_parser.h"
#include "entity.h"

#include "my_point.h"
#include "snis_font.h"
#include "snis_typeface.h"

#include "snis_graph.h"

#define MAX_ENTITIES 5000

struct entity {
	struct mesh *m;
	float x, y, z; /* world coords */
};

static int nentities = 0;
static struct entity entity_list[MAX_ENTITIES];

struct entity *add_entity(struct mesh *m, float x, float y, float z)
{
	if (nentities < MAX_ENTITIES) {
		entity_list[nentities].m = m;
		entity_list[nentities].x = x;
		entity_list[nentities].y = y;
		entity_list[nentities].z = z;
		nentities++;
		return &entity_list[nentities - 1];
	}
	return NULL;
}

void render_triangle(GtkWidget *w, GdkGC *gc, struct triangle *t)
{
	struct vertex *v1, *v2, *v3;

	v1 = t->v[0];
	v2 = t->v[1];
	v3 = t->v[3];
	sng_current_draw_line(w->window, gc, v1->sx, v1->sy, v2->sx, v2->sy); 
	sng_current_draw_line(w->window, gc, v2->sx, v2->sy, v3->sx, v3->sy); 
	sng_current_draw_line(w->window, gc, v3->sx, v3->sy, v1->sx, v1->sy); 
}

void render_entity(GtkWidget *w, GdkGC *gc, struct entity *e)
{
	int i;

	for (i = 0; i < e->m->ntriangles; i++)
		render_triangle(w, gc, &e->m->t[i]);
}

static void transform_entity(struct entity *e)
{
}

void render_entities(GtkWidget *w, GdkGC *gc)
{
	int i;
	for (i = 0; i < nentities; i++)
		transform_entity(&entity_list[i]);
	for (i = 0; i < nentities; i++)
		render_entity(w, gc, &entity_list[i]);
}

