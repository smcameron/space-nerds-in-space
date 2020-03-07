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
#define MATERIAL_TEXTURE_MAPPED 4
#define MATERIAL_TEXTURE_CUBEMAP 5
#define MATERIAL_NEBULA 6
#define MATERIAL_TEXTURE_MAPPED_UNLIT 7
#define MATERIAL_TEXTURED_PARTICLE 8
#define MATERIAL_TEXTURED_PLANET 9
#define MATERIAL_TEXTURED_PLANET_RING 10
#define MATERIAL_WIREFRAME_SPHERE_CLIP 11
#define MATERIAL_TEXTURED_SHIELD 12
#define MATERIAL_ATMOSPHERE 13
#define MATERIAL_ALPHA_BY_NORMAL 14

#define MATERIAL_BILLBOARD_TYPE_NONE 0
#define MATERIAL_BILLBOARD_TYPE_SCREEN 1
#define MATERIAL_BILLBOARD_TYPE_SPHERICAL 2
#define MATERIAL_BILLBOARD_TYPE_AXIS 3

struct material_color_by_w {
	int near_color;
	int center_color;
	int far_color;

	float near_w;
	float center_w;
	float far_w;
};

struct material_texture_mapped {
	int texture_id;
	int emit_texture_id;
	int normalmap_id;
	float specular_power;
	float specular_intensity;
	float emit_intensity; /* Range 0.0 - 1.0, and later multiplied by per-entity emit_intensity */
};

struct material_texture_mapped_unlit {
	int texture_id;
	int do_cullface;
	int do_blend;
	float alpha;
	struct sng_color tint;
};

struct material_texture_cubemap {
	int texture_id;
	int do_cullface;
	int do_blend;
	float alpha;
	struct sng_color tint;
};

#define MATERIAL_NEBULA_NPLANES 6

struct material_nebula {
	int texture_id[MATERIAL_NEBULA_NPLANES];
	union quat orientation[MATERIAL_NEBULA_NPLANES];
	float alpha;
	struct sng_color tint;
};

struct material_alpha_by_normal {
	int texture_id;
	/* Alpha is related to eye-direction dot surface normal */
	int invert;	/* 0 means planes facing eye direction are more opaque, */
			/* and oblique surfaces more transparent, 1 means the reverse, */
			/* planes facing eye direcction are more transparent, oblique */
			/* surfaces more opaque. */
	int do_cullface;
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
	float texture_v;
	float inner_radius;
	float outer_radius;
	struct sng_color tint;
};

struct material_textured_planet {
	int texture_id;
	int normalmap_id;
	struct material *ring_material;
	float water_color_r, water_color_g, water_color_b; /* for specular calculations */
	float sun_color_r, sun_color_g, sun_color_b; /* for specular calculations */
};

struct material_textured_shield {
	int texture_id;
};

struct material_atmosphere {
	float r, g, b, scale, *brightness, brightness_modifier;
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
		struct material_texture_mapped texture_mapped;
		struct material_texture_mapped_unlit texture_mapped_unlit;
		struct material_texture_cubemap texture_cubemap;
		struct material_nebula nebula;
		struct material_textured_particle textured_particle;
		struct material_textured_planet textured_planet;
		struct material_textured_shield textured_shield;
		struct material_textured_planet_ring textured_planet_ring;
		struct material_wireframe_sphere_clip wireframe_sphere_clip;
		struct material_atmosphere atmosphere;
		struct material_alpha_by_normal alpha_by_normal;
	};
	int type;
	int billboard_type;
};

extern void material_init_texture_mapped(struct material *m);
extern void material_init_texture_mapped_unlit(struct material *m);
extern void material_init_texture_cubemap(struct material *m);
extern void material_init_nebula(struct material *m);
extern void material_init_textured_particle(struct material *m);
extern void material_init_textured_planet(struct material *m);
extern void material_init_textured_shield(struct material *m);
extern void material_init_textured_planet_ring(struct material *m);
extern void material_init_wireframe_sphere_clip(struct material *m);
extern void material_init_atmosphere(struct material *m);
extern void material_init_alpha_by_normal(struct material *m);

extern int material_nebula_read_from_file(const char *asset_dir, const char *filename,
						struct material *nebula);
extern int material_nebula_write_to_file(const char *asset_dir, const char *filename,
						struct material *nebula);

#endif
