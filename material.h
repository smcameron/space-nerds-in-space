/*
	Copyright (C) 2010 Jeremy Van Grinsven
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
#ifndef MATERIAL_H__
#define MATERIAL_H__

struct mesh;
union quat;
struct sng_color;
struct material;
struct entity;

#define MATERIAL_COLOR 0
#define MATERIAL_COLOR_BY_W 1
#define MATERIAL_LASER 2
#define MATERIAL_BILLBOARD 3
#define MATERIAL_TEXTURE_MAPPED 4
#define MATERIAL_TEXTURE_CUBEMAP 5
#define MATERIAL_NEBULA 6
#define MATERIAL_TEXTURE_MAPPED_UNLIT 7
#define MATERIAL_TEXTURED_PARTICLE 8
#define MATERIAL_TEXTURED_PLANET 9
#define MATERIAL_TEXTURED_PLANET_RING 10
#define MATERIAL_WIREFRAME_SPHERE_CLIP 11

struct material_color_by_w {
	int near_color;
	int center_color;
	int far_color;

	float near_w;
	float center_w;
	float far_w;
};

#define MATERIAL_BILLBOARD_TYPE_SCREEN 0
#define MATERIAL_BILLBOARD_TYPE_SPHERICAL 1
#define MATERIAL_BILLBOARD_TYPE_AXIS 2

struct material_billboard {
	int billboard_type;
	int texture_id;
};

struct material_texture_mapped {
	int texture_id;
};

struct material_texture_mapped_unlit {
	int do_cullface;
	int do_blend;
	int texture_id;
	float alpha;
	struct sng_color tint;
};

struct material_texture_cubemap {
	int texture_id;
};

#define MATERIAL_NEBULA_NPLANES 6

struct material_nebula {
	int texture_id[MATERIAL_NEBULA_NPLANES];
	union quat orientation[MATERIAL_NEBULA_NPLANES];
	float alpha;
	struct sng_color tint;
};

struct material_textured_particle {
	int texture_id;
	float radius;
	float time_base;
};

struct material_textured_planet_ring {
	int texture_id;
	float alpha;
	struct sng_color tint;
};

struct material_textured_planet {
	int texture_id;
	struct material *ring_material;
};

struct material_wireframe_sphere_clip {
	struct entity *center;
	float radius;
	float radius_fade;
};

struct material {
	__extension__ union {
		struct material_color_by_w color_by_w;
		struct material_billboard billboard;
		struct material_texture_mapped texture_mapped;
		struct material_texture_mapped_unlit texture_mapped_unlit;
		struct material_texture_cubemap texture_cubemap;
		struct material_nebula nebula;
		struct material_textured_particle textured_particle;
		struct material_textured_planet textured_planet;
		struct material_textured_planet_ring textured_planet_ring;
		struct material_wireframe_sphere_clip wireframe_sphere_clip;
	};
	int type;
};


extern int material_nebula_read_from_file(const char *asset_dir, const char *filename,
						struct material *nebula);
extern int material_nebula_write_to_file(const char *asset_dir, const char *filename,
						struct material *nebula);

#endif
