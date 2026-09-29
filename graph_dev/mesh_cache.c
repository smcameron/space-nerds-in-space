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

/* See graph_dev/mesh_cache.h. */

#include <stdlib.h>
#include <string.h>
#include <stddef.h>

#include "opengl_cap.h"
#include "snis_profile.h"
#include "mesh_cache.h"

void mesh_graph_dev_cleanup(struct mesh *m)
{
	PROFILE_ZONE_START("mesh_graph_dev_cleanup");
	if (m->graph_ptr) {
		struct mesh_gl_info *ptr = m->graph_ptr;

		glDeleteBuffers(1, &ptr->vertex_buffer);
		glDeleteBuffers(1, &ptr->triangle_vertex_buffer);
		glDeleteBuffers(1, &ptr->wireframe_lines_vertex_buffer);
		glDeleteBuffers(1, &ptr->triangle_normal_lines_buffer);
		glDeleteBuffers(1, &ptr->triangle_tangent_lines_buffer);
		glDeleteBuffers(1, &ptr->triangle_bitangent_lines_buffer);
		glDeleteBuffers(1, &ptr->line_vertex_buffer);
		glDeleteBuffers(1, &ptr->particle_vertex_buffer);
		glDeleteBuffers(1, &ptr->particle_index_buffer);

		free(ptr);
		m->graph_ptr = 0;
	}
	PROFILE_ZONE_END();
}

void mesh_graph_dev_init(struct mesh *m)
{
	PROFILE_ZONE_START("mesh_graph_dev_init");

	struct mesh_gl_info *ptr = m->graph_ptr;
	if (!ptr) {
		ptr = malloc(sizeof(struct mesh_gl_info));
		memset(ptr, 0, sizeof(*ptr));
		m->graph_ptr = ptr;
	}

	if (m->geometry_mode == MESH_GEOMETRY_TRIANGLES) {
		/* setup the triangle mesh buffers */
		int i;
		size_t v_size = sizeof(struct vertex_buffer_data) * m->ntriangles * 3;
		size_t vt_size = sizeof(struct vertex_triangle_buffer_data) * m->ntriangles * 3;
		struct vertex_buffer_data *g_v_buffer_data = malloc(v_size);
		struct vertex_triangle_buffer_data *g_vt_buffer_data = malloc(vt_size);

#if DEBUG_NORMALS
		float normal_line_length = m->radius / 20.0;
		size_t nl_size = sizeof(struct vertex_buffer_data) * m->ntriangles * 3 * 2;
		struct vertex_buffer_data *g_nl_buffer_data = malloc(nl_size * 3);
		memset(g_nl_buffer_data, 0, nl_size * 3);
		struct vertex_buffer_data *g_tl_buffer_data = &g_nl_buffer_data[m->ntriangles * 3 * 2];
		struct vertex_buffer_data *g_bl_buffer_data = &g_nl_buffer_data[m->ntriangles * 3 * 2 * 2];
#endif

		ptr->ntriangles = m->ntriangles;
		ptr->npoints = m->ntriangles * 3; /* can be rendered as a point cloud too */

		for (i = 0; i < m->ntriangles; i++) {
			int j = 0;
			for (j = 0; j < 3; j++) {
				int v_index = i * 3 + j;
				g_v_buffer_data[v_index].position.v.x = m->t[i].v[j]->x;
				g_v_buffer_data[v_index].position.v.y = m->t[i].v[j]->y;
				g_v_buffer_data[v_index].position.v.z = m->t[i].v[j]->z;

				g_vt_buffer_data[v_index].normal.v.x = m->t[i].vnormal[j].x;
				g_vt_buffer_data[v_index].normal.v.y = m->t[i].vnormal[j].y;
				g_vt_buffer_data[v_index].normal.v.z = m->t[i].vnormal[j].z;

				g_vt_buffer_data[v_index].tvertex0.v.x = m->t[i].v[0]->x;
				g_vt_buffer_data[v_index].tvertex0.v.y = m->t[i].v[0]->y;
				g_vt_buffer_data[v_index].tvertex0.v.z = m->t[i].v[0]->z;

				g_vt_buffer_data[v_index].tvertex1.v.x = m->t[i].v[1]->x;
				g_vt_buffer_data[v_index].tvertex1.v.y = m->t[i].v[1]->y;
				g_vt_buffer_data[v_index].tvertex1.v.z = m->t[i].v[1]->z;

				g_vt_buffer_data[v_index].tvertex2.v.x = m->t[i].v[2]->x;
				g_vt_buffer_data[v_index].tvertex2.v.y = m->t[i].v[2]->y;
				g_vt_buffer_data[v_index].tvertex2.v.z = m->t[i].v[2]->z;

				g_vt_buffer_data[v_index].tangent.v.x = m->t[i].vtangent[j].x;
				g_vt_buffer_data[v_index].tangent.v.y = m->t[i].vtangent[j].y;
				g_vt_buffer_data[v_index].tangent.v.z = m->t[i].vtangent[j].z;

				g_vt_buffer_data[v_index].bitangent.v.x = m->t[i].vbitangent[j].x;
				g_vt_buffer_data[v_index].bitangent.v.y = m->t[i].vbitangent[j].y;
				g_vt_buffer_data[v_index].bitangent.v.z = m->t[i].vbitangent[j].z;

				/* bias the edge distance to make the coplanar edges not draw */
				if ((j == 1 || j == 2) && (m->t[i].flag & TRIANGLE_1_2_COPLANAR))
					g_vt_buffer_data[v_index].wireframe_edge_mask.v.x = 1000;
				else
					g_vt_buffer_data[v_index].wireframe_edge_mask.v.x = 0;

				if ((j == 0 || j == 2) && (m->t[i].flag & TRIANGLE_0_2_COPLANAR))
					g_vt_buffer_data[v_index].wireframe_edge_mask.v.y = 1000;
				else
					g_vt_buffer_data[v_index].wireframe_edge_mask.v.y = 0;

				if ((j == 0 || j == 1) && (m->t[i].flag & TRIANGLE_0_1_COPLANAR))
					g_vt_buffer_data[v_index].wireframe_edge_mask.v.z = 1000;
				else
					g_vt_buffer_data[v_index].wireframe_edge_mask.v.z = 0;

				if (m->tex) {
					g_vt_buffer_data[v_index].texture_coord.v.x = m->tex[v_index].u;
					g_vt_buffer_data[v_index].texture_coord.v.y = m->tex[v_index].v;
				} else {
					g_vt_buffer_data[v_index].texture_coord.v.x = 0;
					g_vt_buffer_data[v_index].texture_coord.v.y = 0;
				}

#if DEBUG_NORMALS
				/* draw a line for each vertex normal, tangent, and bitangent */
				int nl_index = i * 6 + j * 2;

				/* normal */
				g_nl_buffer_data[nl_index].position.v.x = m->t[i].v[j]->x;
				g_nl_buffer_data[nl_index].position.v.y = m->t[i].v[j]->y;
				g_nl_buffer_data[nl_index].position.v.z = m->t[i].v[j]->z;

				g_nl_buffer_data[nl_index + 1].position.v.x =
					m->t[i].v[j]->x + normal_line_length * m->t[i].vnormal[j].x;
				g_nl_buffer_data[nl_index + 1].position.v.y =
					m->t[i].v[j]->y + normal_line_length * m->t[i].vnormal[j].y;
				g_nl_buffer_data[nl_index + 1].position.v.z =
					m->t[i].v[j]->z + normal_line_length * m->t[i].vnormal[j].z;

				/* tangent */
				g_tl_buffer_data[nl_index].position.v.x = m->t[i].v[j]->x;
				g_tl_buffer_data[nl_index].position.v.y = m->t[i].v[j]->y;
				g_tl_buffer_data[nl_index].position.v.z = m->t[i].v[j]->z;
				g_tl_buffer_data[nl_index + 1].position.v.x =
					m->t[i].v[j]->x + normal_line_length * m->t[i].vtangent[j].x;
				g_tl_buffer_data[nl_index + 1].position.v.y =
					m->t[i].v[j]->y + normal_line_length * m->t[i].vtangent[j].y;
				g_tl_buffer_data[nl_index + 1].position.v.z =
					m->t[i].v[j]->z + normal_line_length * m->t[i].vtangent[j].z;

				/* bitangent */
				g_bl_buffer_data[nl_index].position.v.x = m->t[i].v[j]->x;
				g_bl_buffer_data[nl_index].position.v.y = m->t[i].v[j]->y;
				g_bl_buffer_data[nl_index].position.v.z = m->t[i].v[j]->z;
				g_bl_buffer_data[nl_index + 1].position.v.x =
					m->t[i].v[j]->x + normal_line_length * m->t[i].vbitangent[j].x;
				g_bl_buffer_data[nl_index + 1].position.v.y =
					m->t[i].v[j]->y + normal_line_length * m->t[i].vbitangent[j].y;
				g_bl_buffer_data[nl_index + 1].position.v.z =
					m->t[i].v[j]->z + normal_line_length * m->t[i].vbitangent[j].z;
#endif
			}
		}

		LOAD_BUFFER(GL_ARRAY_BUFFER, ptr->vertex_buffer, v_size, g_v_buffer_data);
		LOAD_BUFFER(GL_ARRAY_BUFFER, ptr->triangle_vertex_buffer, vt_size, g_vt_buffer_data);
#if DEBUG_NORMALS
		LOAD_BUFFER(GL_ARRAY_BUFFER, ptr->triangle_normal_lines_buffer, nl_size, g_nl_buffer_data);
		LOAD_BUFFER(GL_ARRAY_BUFFER, ptr->triangle_tangent_lines_buffer, nl_size, g_tl_buffer_data);
		LOAD_BUFFER(GL_ARRAY_BUFFER, ptr->triangle_bitangent_lines_buffer, nl_size, g_bl_buffer_data);
#endif

		free(g_v_buffer_data);
		free(g_vt_buffer_data);
#if DEBUG_NORMALS
		free(g_nl_buffer_data);
#endif

		/* setup the line buffers used for wireframe */
		size_t wfl_size = sizeof(struct vertex_wireframe_line_buffer_data) * m->ntriangles * 3 * 2;
		struct vertex_wireframe_line_buffer_data *g_wfl_buffer_data = malloc(wfl_size);

		/* map the edge combinatinos to the triangle coplanar flag */
		static const int tri_coplaner_flags[3][3] = {
			{0, TRIANGLE_0_1_COPLANAR, TRIANGLE_0_2_COPLANAR},
			{TRIANGLE_0_1_COPLANAR, 0, TRIANGLE_1_2_COPLANAR},
			{TRIANGLE_0_2_COPLANAR, TRIANGLE_1_2_COPLANAR, 0} };

		ptr->nwireframe_lines = 0;

		for (i = 0; i < m->ntriangles; i++) {
			int j0 = 0;
			for (j0 = 0; j0 < 3; j0++) {
				int j1 = (j0 + 1) % 3;

				if (!(m->t[i].flag & tri_coplaner_flags[j0][j1])) {
					int index = 2 * ptr->nwireframe_lines;

					/* add the line from vertex j0 to j1 */
					g_wfl_buffer_data[index].position.v.x = m->t[i].v[j0]->x;
					g_wfl_buffer_data[index].position.v.y = m->t[i].v[j0]->y;
					g_wfl_buffer_data[index].position.v.z = m->t[i].v[j0]->z;

					g_wfl_buffer_data[index + 1].position.v.x = m->t[i].v[j1]->x;
					g_wfl_buffer_data[index + 1].position.v.y = m->t[i].v[j1]->y;
					g_wfl_buffer_data[index + 1].position.v.z = m->t[i].v[j1]->z;

					/* the line normal is the same as the triangle */
					g_wfl_buffer_data[index].normal.v.x = m->t[i].n.x;
					g_wfl_buffer_data[index].normal.v.y = m->t[i].n.y;
					g_wfl_buffer_data[index].normal.v.z = m->t[i].n.z;
					g_wfl_buffer_data[index + 1].normal.v.x = m->t[i].n.x;
					g_wfl_buffer_data[index + 1].normal.v.y = m->t[i].n.y;
					g_wfl_buffer_data[index + 1].normal.v.z = m->t[i].n.z;

					ptr->nwireframe_lines++;
				}
			}
		}

		LOAD_BUFFER(GL_ARRAY_BUFFER, ptr->wireframe_lines_vertex_buffer,
			sizeof(struct vertex_wireframe_line_buffer_data) * ptr->nwireframe_lines * 2,
			g_wfl_buffer_data);

		free(g_wfl_buffer_data);
	}

	if (m->geometry_mode == MESH_GEOMETRY_LINES || m->geometry_mode == MESH_GEOMETRY_PARTICLE_ANIMATION) {
		/* setup the line buffers */
		int i;
		size_t v_size = sizeof(struct vertex_buffer_data) * m->nvertices * 2;
		struct vertex_buffer_data *g_v_buffer_data = malloc(v_size);

		size_t vl_size = sizeof(struct vertex_line_buffer_data) * m->nvertices * 2;
		struct vertex_line_buffer_data *g_vl_buffer_data = malloc(vl_size);

		ptr->nlines = 0;

		for (i = 0; i < m->nlines; i++) {
			struct vertex *vstart = m->l[i].start;
			struct vertex *vend = m->l[i].end;

			if (m->l[i].flag & MESH_LINE_STRIP) {
				struct vertex *vcurr = vstart;
				struct vertex *v1;

				while (vcurr <= vend) {
					struct vertex *v2 = vcurr;

					if (v2 != vstart) {
						int index = ptr->nlines * 2;
						g_v_buffer_data[index].position.v.x = v1->x;
						g_v_buffer_data[index].position.v.y = v1->y;
						g_v_buffer_data[index].position.v.z = v1->z;

						g_v_buffer_data[index + 1].position.v.x = v2->x;
						g_v_buffer_data[index + 1].position.v.y = v2->y;
						g_v_buffer_data[index + 1].position.v.z = v2->z;

						g_vl_buffer_data[index].multi_one[0] =
							g_vl_buffer_data[index + 1].multi_one[0] = 0; /* is dotted */

						g_vl_buffer_data[index].line_vertex0.v.x =
							g_vl_buffer_data[index + 1].line_vertex0.v.x = v1->x;
						g_vl_buffer_data[index].line_vertex0.v.y =
							g_vl_buffer_data[index + 1].line_vertex0.v.y = v1->y;
						g_vl_buffer_data[index].line_vertex0.v.z =
							g_vl_buffer_data[index + 1].line_vertex0.v.z = v1->z;

						g_vl_buffer_data[index].line_vertex1.v.x =
							g_vl_buffer_data[index + 1].line_vertex1.v.x = v2->x;
						g_vl_buffer_data[index].line_vertex1.v.y =
							g_vl_buffer_data[index + 1].line_vertex1.v.y = v2->y;
						g_vl_buffer_data[index].line_vertex1.v.z =
							g_vl_buffer_data[index + 1].line_vertex1.v.z = v2->z;

						ptr->nlines++;
					}
					v1 = v2;
					++vcurr;
				}
			} else {
				int is_dotted = m->l[i].flag & MESH_LINE_DOTTED;

				int index = ptr->nlines * 2;
				g_v_buffer_data[index].position.v.x = vstart->x;
				g_v_buffer_data[index].position.v.y = vstart->y;
				g_v_buffer_data[index].position.v.z = vstart->z;

				g_v_buffer_data[index + 1].position.v.x = vend->x;
				g_v_buffer_data[index + 1].position.v.y = vend->y;
				g_v_buffer_data[index + 1].position.v.z = vend->z;

				g_vl_buffer_data[index].multi_one[0] =
					g_vl_buffer_data[index + 1].multi_one[0] = is_dotted ? 255 : 0;

				g_vl_buffer_data[index].line_vertex0.v.x =
					g_vl_buffer_data[index + 1].line_vertex0.v.x = vstart->x;
				g_vl_buffer_data[index].line_vertex0.v.y =
					g_vl_buffer_data[index + 1].line_vertex0.v.y = vstart->y;
				g_vl_buffer_data[index].line_vertex0.v.z =
					g_vl_buffer_data[index + 1].line_vertex0.v.z = vstart->z;

				g_vl_buffer_data[index].line_vertex1.v.x =
					g_vl_buffer_data[index + 1].line_vertex1.v.x = vend->x;
				g_vl_buffer_data[index].line_vertex1.v.y =
					g_vl_buffer_data[index + 1].line_vertex1.v.y = vend->y;
				g_vl_buffer_data[index].line_vertex1.v.z =
					g_vl_buffer_data[index + 1].line_vertex1.v.z = vend->z;

				ptr->nlines++;
			}
		}

		LOAD_BUFFER(GL_ARRAY_BUFFER, ptr->vertex_buffer, sizeof(struct vertex_buffer_data) * 2 * ptr->nlines,
			g_v_buffer_data);
		LOAD_BUFFER(GL_ARRAY_BUFFER, ptr->line_vertex_buffer,
			sizeof(struct vertex_line_buffer_data) * 2 * ptr->nlines, g_vl_buffer_data);

		ptr->npoints = ptr->nlines * 2; /* can be rendered as a point cloud too */

		free(g_v_buffer_data);
		free(g_vl_buffer_data);
	}

	if (m->geometry_mode == MESH_GEOMETRY_POINTS) {
		/* setup the point buffers */
		size_t v_size = sizeof(struct vertex_buffer_data) * m->nvertices;
		struct vertex_buffer_data *g_v_buffer_data = malloc(v_size);

		ptr->npoints = m->nvertices;

		int i;
		for (i = 0; i < m->nvertices; i++) {
			g_v_buffer_data[i].position.v.x = m->v[i].x;
			g_v_buffer_data[i].position.v.y = m->v[i].y;
			g_v_buffer_data[i].position.v.z = m->v[i].z;
		}

		LOAD_BUFFER(GL_ARRAY_BUFFER, ptr->vertex_buffer, v_size, g_v_buffer_data);

		free(g_v_buffer_data);
	}

	if (m->geometry_mode == MESH_GEOMETRY_PARTICLE_ANIMATION) {
		ptr->nparticles = m->nvertices / 2;

		size_t v_size = sizeof(struct vertex_particle_buffer_data) * ptr->nparticles * 4;
		struct vertex_particle_buffer_data *g_v_buffer_data = malloc(v_size);

		size_t i_size = sizeof(GLushort) * ptr->nparticles * 6;
		GLushort *g_i_buffer_data = malloc(i_size);

		/* two triangles from four vertices
		   V3 (0,1) +---+ V2 (1,1)
			    +\  +
			    + \ +
			    +  \+
		   V0 (0,0) +---+ V1 (1,0)
		*/
		int i;
		for (i = 0; i < m->nvertices; i += 2) {
			int v_index = i * 2;

			GLubyte tint_red = (int)(m->l[i / 2].tint_color.red * 255) & 255;
			GLubyte tint_green = (int)(m->l[i / 2].tint_color.green * 255) & 255;
			GLubyte tint_blue = (int)(m->l[i / 2].tint_color.blue * 255) & 255;
			GLubyte additivity = (int)(m->l[i / 2].additivity * 255) & 255;
			GLubyte opacity = (int)(m->l[i / 2].opacity * 255) & 255;
			GLubyte time_offset = (int)(m->l[i / 2].time_offset * 255) & 255;

			/* texture coord is different for all four vertices */
			g_v_buffer_data[v_index + 0].multi_one[0] = 0;
			g_v_buffer_data[v_index + 0].multi_one[1] = 0;

			g_v_buffer_data[v_index + 1].multi_one[0] = 255;
			g_v_buffer_data[v_index + 1].multi_one[1] = 0;

			g_v_buffer_data[v_index + 2].multi_one[0] = 255;
			g_v_buffer_data[v_index + 2].multi_one[1] = 255;

			g_v_buffer_data[v_index + 3].multi_one[0] = 0;
			g_v_buffer_data[v_index + 3].multi_one[1] = 255;

			g_v_buffer_data[v_index + 0].multi_one[2] =
				g_v_buffer_data[v_index + 1].multi_one[2] =
				g_v_buffer_data[v_index + 2].multi_one[2] =
				g_v_buffer_data[v_index + 3].multi_one[2] = time_offset;

			/* the rest of the attributes are the same for all four */
			g_v_buffer_data[v_index + 0].start_position.v.x =
				g_v_buffer_data[v_index + 1].start_position.v.x =
				g_v_buffer_data[v_index + 2].start_position.v.x =
				g_v_buffer_data[v_index + 3].start_position.v.x = m->v[i].x;
			g_v_buffer_data[v_index + 0].start_position.v.y =
				g_v_buffer_data[v_index + 1].start_position.v.y =
				g_v_buffer_data[v_index + 2].start_position.v.y =
				g_v_buffer_data[v_index + 3].start_position.v.y = m->v[i].y;
			g_v_buffer_data[v_index + 0].start_position.v.z =
				g_v_buffer_data[v_index + 1].start_position.v.z =
				g_v_buffer_data[v_index + 2].start_position.v.z =
				g_v_buffer_data[v_index + 3].start_position.v.z = m->v[i].z;

			g_v_buffer_data[v_index + 0].start_tint_color[0] =
				g_v_buffer_data[v_index + 1].start_tint_color[0] =
				g_v_buffer_data[v_index + 2].start_tint_color[0] =
				g_v_buffer_data[v_index + 3].start_tint_color[0] = tint_red;
			g_v_buffer_data[v_index + 0].start_tint_color[1] =
				g_v_buffer_data[v_index + 1].start_tint_color[1] =
				g_v_buffer_data[v_index + 2].start_tint_color[1] =
				g_v_buffer_data[v_index + 3].start_tint_color[1] = tint_green;
			g_v_buffer_data[v_index + 0].start_tint_color[2] =
				g_v_buffer_data[v_index + 1].start_tint_color[2] =
				g_v_buffer_data[v_index + 2].start_tint_color[2] =
				g_v_buffer_data[v_index + 3].start_tint_color[2] = tint_blue;

			g_v_buffer_data[v_index + 0].start_apm[0] =
				g_v_buffer_data[v_index + 1].start_apm[0] =
				g_v_buffer_data[v_index + 2].start_apm[0] =
				g_v_buffer_data[v_index + 3].start_apm[0] = additivity;
			g_v_buffer_data[v_index + 0].start_apm[1] =
				g_v_buffer_data[v_index + 1].start_apm[1] =
				g_v_buffer_data[v_index + 2].start_apm[1] =
				g_v_buffer_data[v_index + 3].start_apm[1] = opacity;

			g_v_buffer_data[v_index + 0].end_position.v.x =
				g_v_buffer_data[v_index + 1].end_position.v.x =
				g_v_buffer_data[v_index + 2].end_position.v.x =
				g_v_buffer_data[v_index + 3].end_position.v.x = m->v[i + 1].x;
			g_v_buffer_data[v_index + 0].end_position.v.y =
				g_v_buffer_data[v_index + 1].end_position.v.y =
				g_v_buffer_data[v_index + 2].end_position.v.y =
				g_v_buffer_data[v_index + 3].end_position.v.y = m->v[i + 1].y;
			g_v_buffer_data[v_index + 0].end_position.v.z =
				g_v_buffer_data[v_index + 1].end_position.v.z =
				g_v_buffer_data[v_index + 2].end_position.v.z =
				g_v_buffer_data[v_index + 3].end_position.v.z = m->v[i + 1].z;

			g_v_buffer_data[v_index + 0].end_tint_color[0] =
				g_v_buffer_data[v_index + 1].end_tint_color[0] =
				g_v_buffer_data[v_index + 2].end_tint_color[0] =
				g_v_buffer_data[v_index + 3].end_tint_color[0] = tint_red;
			g_v_buffer_data[v_index + 0].end_tint_color[1] =
				g_v_buffer_data[v_index + 1].end_tint_color[1] =
				g_v_buffer_data[v_index + 2].end_tint_color[1] =
				g_v_buffer_data[v_index + 3].end_tint_color[1] = tint_green;
			g_v_buffer_data[v_index + 0].end_tint_color[2] =
				g_v_buffer_data[v_index + 1].end_tint_color[2] =
				g_v_buffer_data[v_index + 2].end_tint_color[2] =
				g_v_buffer_data[v_index + 3].end_tint_color[2] = tint_blue;

			g_v_buffer_data[v_index + 0].end_apm[0] =
				g_v_buffer_data[v_index + 1].end_apm[0] =
				g_v_buffer_data[v_index + 2].end_apm[0] =
				g_v_buffer_data[v_index + 3].end_apm[0] = additivity;
			g_v_buffer_data[v_index + 0].end_apm[1] =
				g_v_buffer_data[v_index + 1].end_apm[1] =
				g_v_buffer_data[v_index + 2].end_apm[1] =
				g_v_buffer_data[v_index + 3].end_apm[1] = opacity;

			/* setup six indices for our two triangles */
			int i_index = i * 3;
			g_i_buffer_data[i_index + 0] = v_index + 0;
			g_i_buffer_data[i_index + 1] = v_index + 1;
			g_i_buffer_data[i_index + 2] = v_index + 3;
			g_i_buffer_data[i_index + 3] = v_index + 1;
			g_i_buffer_data[i_index + 4] = v_index + 2;
			g_i_buffer_data[i_index + 5] = v_index + 3;
		}

		LOAD_BUFFER(GL_ARRAY_BUFFER, ptr->particle_vertex_buffer, v_size, g_v_buffer_data);
		LOAD_BUFFER(GL_ELEMENT_ARRAY_BUFFER, ptr->particle_index_buffer, i_size, g_i_buffer_data);

		free(g_v_buffer_data);
		free(g_i_buffer_data);
	}

	PROFILE_ZONE_END();
}
