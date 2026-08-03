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
#define MATERIAL_PLANETARY_LIGHTNING 15
#define MATERIAL_WARP_GATE_EFFECT 16
#define MATERIAL_SUN 17
#define MATERIAL_BLACK_HOLE 18

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

/* A star drawn as a camera-facing billboard.  The whole star is one emission field --
 * blackbody colour times a radial intensity (bright flat core, then a bloom from the disc edge
 * out) -- pushed through the shared filmic tonemap, so the hot core desaturates to white while
 * the limb and bloom keep the star's colour.  disc_radius is the disc's radius in the
 * billboard's 0..0.5 UV space, set per frame (star world radius / billboard world size) so the
 * disc stays world-scale while the billboard is sized to the bloom's screen extent. */
struct material_sun {
	struct sng_color color;  /* the star's chromaticity, brightest channel 1 */
	float brightness;        /* surface brightness in LINEAR HDR units, from the temperature */
	float disc_radius;       /* disc radius in UV (0..0.5), set per frame */
	float edge_softness;     /* limb softness of the ALPHA only, as a fraction of the disc radius */
	/* The two below describe the OPTICS, not the star: one setting serves every star, because
	 * the power-law point spread function is scale free and brightness alone moves where it
	 * crosses white.  They are material fields rather than shader constants only so shadow_lab
	 * can tune them.  There is no amplitude any more -- the profile IS the star's disc convolved
	 * with the PSF, normalised to 1 at the centre, so its overall level is just the brightness. */
	float psf_width;         /* point spread width, in star radii */
	float psf_falloff;       /* power-law exponent of the PSF's tail */
};

/* An event horizon: a flat, wholly opaque black disc, plus a thin bright rim standing in for the
 * photon ring.  Drawn procedurally rather than from a texture because the disc's edge has to
 * land at a known angle -- the skybox's gravitational lensing floors its deflection at exactly
 * this radius and relies on the disc to cover the singularity there (see
 * graph_dev_set_gravitational_lenses()).  A painted blob's edge is wherever its alpha happens to
 * fade out, which is both fuzzy and resolution-bound; this one is exact at any zoom.
 *
 * The Einstein ring is drawn here too, out at lens_strength disc radii, even though the arcs
 * that go with it belong to the skybox and only appear for a hole holding one of the shader's
 * lens slots.  Otherwise a hole that loses its slot loses its halo as well, and reads as a
 * different sort of object rather than as the same one drawn more cheaply.
 *
 * Both radii are in the billboard's 0..0.5 UV space, and the quad has to be sized to hold the
 * outer one and its glow -- see material_black_hole_set_geometry(), which works both out
 * together, since getting them to disagree is exactly the bug this material exists to avoid. */
struct material_black_hole {
	float disc_radius;      /* event horizon radius in UV (0..0.5) */
	float edge_softness;    /* rim ramp width as a fraction of the disc radius */
	float ring_brightness;  /* photon-ring rim emission scale; 0 for a bare disc */
	float ring_width;       /* rim glow width as a fraction of the disc radius */
	float einstein_radius;  /* Einstein ring radius in UV (0..0.5); 0 to leave it out */
	float glow_brightness;  /* Einstein ring emission scale; 0 for no halo */
	float glow_width;       /* Einstein ring width as a fraction of the Einstein radius */
	struct sng_color ring_color;
};

/* Set disc_radius and einstein_radius for a hole whose Einstein radius is lens_strength times
 * its horizon radius, and return the billboard's half-size as a multiple of that horizon radius,
 * which is what the caller scales the entity by:
 *
 *	update_entity_scale(e, 2.0 * material_black_hole_set_geometry(m, strength) * radius);
 *
 * The quad has to stand off past the outermost ring by BLACK_HOLE_BILLBOARD_MARGIN or the glow
 * is cut square at the quad's edge; how far out that is depends on the lens strength, which is
 * why this is a function and not a constant. */
#define BLACK_HOLE_BILLBOARD_MARGIN 1.5
extern float material_black_hole_set_geometry(struct material *m, float lens_strength);

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

struct material_planetary_lightning {
	int texture_id;
	float u1, v1, width;
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
};

struct material_textured_shield {
	int texture_id;
};

struct material_atmosphere {
	float r, g, b, scale, *brightness;
	struct material *ring_material;
	float brightness_modifier;
};

struct material_wireframe_sphere_clip {
	struct entity *center;
	float radius;
	float radius_fade;
};

struct material_warp_gate_effect {
	int texture_id;
	float u1, u2;  /* For scrolling texture */
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
		struct material_planetary_lightning planetary_lightning;
		struct material_warp_gate_effect warp_gate_effect;
		struct material_sun sun;
		struct material_black_hole black_hole;
	};
	int type;
	int billboard_type;
	int rotate_randomly; /* If true, will rotate spherical billboards randomly about axis to camera */
};

extern void material_init_texture_mapped(struct material *m);
extern void material_init_texture_mapped_unlit(struct material *m);
extern void material_init_sun(struct material *m);
extern void material_init_black_hole(struct material *m);
extern void material_init_texture_cubemap(struct material *m);
extern void material_init_nebula(struct material *m);
extern void material_init_textured_particle(struct material *m);
extern void material_init_textured_planet(struct material *m);
extern void material_init_textured_shield(struct material *m);
extern void material_init_textured_planet_ring(struct material *m);
extern void material_init_wireframe_sphere_clip(struct material *m);
extern void material_init_atmosphere(struct material *m);
extern void material_init_alpha_by_normal(struct material *m);
extern void material_init_planetary_lightning(struct material *m);
extern void material_init_warp_gate_effect(struct material *m);

extern int material_nebula_read_from_file(const char *asset_dir, const char *filename,
						struct material *nebula);
extern int material_nebula_write_to_file(const char *asset_dir, const char *filename,
						struct material *nebula);

#endif
