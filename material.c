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

#include <stdio.h>
#include <string.h>
#include <limits.h>

#include "mtwist.h"
#include "quat.h"
#include "snis_graph.h"
#include "material.h"
#include "graph_dev.h"
#include "build_bug_on.h"

static unsigned int load_texture(const char *asset_dir, char *filename, int linear_colorspace)
{
	char fname[PATH_MAX + 1];

	snprintf(fname, sizeof(fname), "%s/textures/%s", asset_dir, filename);
	return graph_dev_load_texture(fname, linear_colorspace);
}

static const char *gnu_basename(const char *path)
{
	char *base = strrchr(path, '/');
	return base ? base + 1 : path;
}

static const char *get_texture_filename(unsigned int texture_id)
{
	return gnu_basename(graph_dev_get_texture_filename(texture_id));
}

int material_nebula_read_from_file(const char *asset_dir, const char *filename,
					struct material *nebula)
{
	FILE *f;
	int rc;
	int i;
	char full_filename[PATH_MAX + 1];
	struct material_nebula *mt = &nebula->nebula;

	nebula->type = MATERIAL_NEBULA;
	nebula->billboard_type = MATERIAL_BILLBOARD_TYPE_NONE;
	nebula->rotate_randomly = 0;
	snprintf(full_filename, sizeof(full_filename), "%s/materials/%s", asset_dir, filename);

	f = fopen(full_filename, "r");
	if (!f) {
		fprintf(stderr, "material_nebula: Error opening file '%s'\n", full_filename);
		return -1;
	}

	for (i = 0; i < MATERIAL_NEBULA_NPLANES; i++) {
		char texture_filename[PATH_MAX];
		BUILD_ASSERT(sizeof(texture_filename) == 4096);
		rc = fscanf(f, "texture %4096s\n", texture_filename);
		if (rc != 1) {
			fprintf(stderr, "material_nebula: Error reading 'texture' from file '%s'\n", full_filename);
			fclose(f);
			return -1;
		}
		mt->texture_id[i] = load_texture(asset_dir, texture_filename, 1);

		rc = fscanf(f, "orientation %f %f %f %f\n", &mt->orientation[i].q.q0, &mt->orientation[i].q.q1,
			&mt->orientation[i].q.q2, &mt->orientation[i].q.q3);
		if (rc != 4) {
			fprintf(stderr, "material_nebula: Error reading 'orientation' from file '%s'\n", full_filename);
			fclose(f);
			return -1;
		}
	}

	rc = fscanf(f, "alpha %f\n", &mt->alpha);
	if (rc != 1) {
		fprintf(stderr, "material_nebula: Error reading 'alpha' from file '%s'\n", full_filename);
		fclose(f);
		return -1;
	}

	rc = fscanf(f, "tint %f %f %f\n", &mt->tint.red, &mt->tint.green, &mt->tint.blue);
	if (rc != 3) {
		fprintf(stderr, "material_nebula: Error reading 'tint' from file '%s'\n", full_filename);
		fclose(f);
		return -1;
	}

	fclose(f);
	return 0;
}

int material_nebula_write_to_file(const char *asset_dir, const char *filename,
					struct material *nebula)
{
	FILE *f;
	int i;
	char full_filename[PATH_MAX + 1];
	struct material_nebula *mt = &nebula->nebula;

	snprintf(full_filename, sizeof(full_filename), "%s/materials/%s", asset_dir, filename);

	f = fopen(full_filename, "w");
	if (!f) {
		fprintf(stderr, "material_nebula: Error opening file '%s' to write\n", full_filename);
		return -1;
	}

	for (i = 0; i < MATERIAL_NEBULA_NPLANES; i++) {
		const char *texture_filename = get_texture_filename(mt->texture_id[i]);
		fprintf(f, "texture %s\n", texture_filename);
		fprintf(f, "orientation %f %f %f %f\n", mt->orientation[i].q.q0, mt->orientation[i].q.q1,
			mt->orientation[i].q.q2, mt->orientation[i].q.q3);
	}

	fprintf(f, "alpha %f\n", mt->alpha);
	fprintf(f, "tint %f %f %f\n", mt->tint.red, mt->tint.green, mt->tint.blue);

	fclose(f);

	return 0;
}

void material_init_texture_mapped(struct material *m)
{
	m->type = MATERIAL_TEXTURE_MAPPED;
	m->billboard_type = MATERIAL_BILLBOARD_TYPE_NONE;
	m->texture_mapped.texture_id = 0;
	m->texture_mapped.emit_texture_id = 0;
	m->texture_mapped.normalmap_id = 0;
	m->texture_mapped.emit_intensity = 1.0;
	m->texture_mapped.specular_power = 512.0;
	m->texture_mapped.specular_intensity = 0.2;
	m->rotate_randomly = 0;
}

void material_init_sun(struct material *m)
{
	m->type = MATERIAL_SUN;
	m->billboard_type = MATERIAL_BILLBOARD_TYPE_SPHERICAL;
	m->sun.color = sng_get_color(WHITE);
	m->sun.disc_radius = 0.1;
	m->sun.edge_softness = 0.03; /* limb width as a fraction of the disc radius; this is an alpha fade at
				     * the rim (semi-transparent by nature), so keep it small -- just enough
				     * to antialias the edge, not a wide see-through gradient */
	m->sun.core_brightness = 4.0;
	m->sun.bloom_brightness = 0.6; /* dimmer than the disc, so the opaque disc reads as a solid body and the
					* glow is clearly a fainter halo around it (1.0 would match the disc edge and
					* make a flat-core disc dissolve into the glow) */
	m->sun.bloom_radius = 0.05;
	m->sun.bloom_falloff = 2.5;
	m->rotate_randomly = 0;
}

void material_init_texture_mapped_unlit(struct material *m)
{
	m->type = MATERIAL_TEXTURE_MAPPED_UNLIT;
	m->billboard_type = MATERIAL_BILLBOARD_TYPE_NONE;
	m->texture_mapped_unlit.texture_id = 0;
	m->texture_mapped_unlit.do_cullface = 1;
	m->texture_mapped_unlit.do_blend = 0;
	m->texture_mapped_unlit.alpha = 1.0;
	m->texture_mapped_unlit.tint = sng_get_color(WHITE);
	m->rotate_randomly = 0;
}

void material_init_texture_cubemap(struct material *m)
{
	m->type = MATERIAL_TEXTURE_CUBEMAP;
	m->billboard_type = MATERIAL_BILLBOARD_TYPE_NONE;
	m->texture_cubemap.texture_id = 0;
	m->texture_cubemap.do_cullface = 1;
	m->texture_cubemap.do_blend = 0;
	m->texture_cubemap.alpha = 1.0;
	m->texture_cubemap.tint = sng_get_color(WHITE);
	m->rotate_randomly = 0;
}

void material_init_nebula(struct material *m)
{
	m->type = MATERIAL_NEBULA;
	m->billboard_type = MATERIAL_BILLBOARD_TYPE_NONE;
	int i;
	for (i = 0; i < MATERIAL_NEBULA_NPLANES; i++) {
		m->nebula.texture_id[i] = 0;
		m->nebula.orientation[i] = identity_quat;
	}
	m->nebula.alpha = 1.0;
	m->nebula.tint = sng_get_color(WHITE);
	m->rotate_randomly = 0;
}

void material_init_textured_particle(struct material *m)
{
	m->type = MATERIAL_TEXTURED_PARTICLE;
	m->billboard_type = MATERIAL_BILLBOARD_TYPE_NONE;
	m->textured_particle.texture_id = 0;
	m->textured_particle.radius = 1.0;
	m->textured_particle.time_base = 1.0;
	m->rotate_randomly = 0;
}

void material_init_textured_planet(struct material *m)
{
	m->type = MATERIAL_TEXTURED_PLANET;
	m->billboard_type = MATERIAL_BILLBOARD_TYPE_NONE;
	m->textured_planet.texture_id = 0;
	m->textured_planet.ring_material = 0;
	m->textured_planet.water_color_r = 0.1;
	m->textured_planet.water_color_g = 0.33;
	m->textured_planet.water_color_b = 1.0;
	m->textured_planet.sun_color_r = 1.0;
	m->textured_planet.sun_color_g = 1.0;
	m->textured_planet.sun_color_b = 0.7;
	m->rotate_randomly = 0;
}

void material_init_textured_shield(struct material *m)
{
	m->type = MATERIAL_TEXTURED_SHIELD;
	m->billboard_type = MATERIAL_BILLBOARD_TYPE_NONE;
	m->textured_shield.texture_id = 0;
	m->rotate_randomly = 0;
}

void material_init_atmosphere(struct material *m)
{
	m->type = MATERIAL_ATMOSPHERE;
	m->billboard_type = MATERIAL_BILLBOARD_TYPE_NONE;
	m->atmosphere.r = 81.0f / 255.0f;
	m->atmosphere.g = 81.0f / 255.0f;
	m->atmosphere.b = 1.0f;
	m->atmosphere.scale = 1.03f;
	m->atmosphere.brightness = NULL;
	m->rotate_randomly = 0;
}

void material_init_alpha_by_normal(struct material *m)
{
	m->type = MATERIAL_ALPHA_BY_NORMAL;
	m->billboard_type = MATERIAL_BILLBOARD_TYPE_NONE;
	m->alpha_by_normal.texture_id = -1; /* texture is optional for alpha by normal */
	m->alpha_by_normal.do_cullface = 1;
	m->alpha_by_normal.texture_id = 0;
	m->alpha_by_normal.invert = 0;
	m->alpha_by_normal.alpha = 1.0;
	m->alpha_by_normal.tint = sng_get_color(WHITE);
	m->rotate_randomly = 0;
}

void material_init_textured_planet_ring(struct material *m)
{
	m->type = MATERIAL_TEXTURED_PLANET_RING;
	m->billboard_type = MATERIAL_BILLBOARD_TYPE_NONE;
	m->textured_planet_ring.texture_id = 0;
	m->textured_planet_ring.alpha = 1.0;
	m->textured_planet_ring.texture_v = 0.0f;
	m->textured_planet_ring.tint = sng_get_color(WHITE);
	m->rotate_randomly = 0;
}

void material_init_wireframe_sphere_clip(struct material *m)
{
	m->type = MATERIAL_WIREFRAME_SPHERE_CLIP;
	m->billboard_type = MATERIAL_BILLBOARD_TYPE_NONE;
	m->wireframe_sphere_clip.center = 0;
	m->wireframe_sphere_clip.radius = 0;
	m->wireframe_sphere_clip.radius_fade = 0;
	m->rotate_randomly = 0;
}

void material_init_planetary_lightning(struct material *m)
{
	m->type = MATERIAL_PLANETARY_LIGHTNING;
	m->billboard_type = MATERIAL_BILLBOARD_TYPE_NONE;
	m->planetary_lightning.texture_id = 0;
	m->rotate_randomly = 0;
}

void material_init_warp_gate_effect(struct material *m)
{
	m->type = MATERIAL_WARP_GATE_EFFECT;
	m->billboard_type = MATERIAL_BILLBOARD_TYPE_NONE;
	m->warp_gate_effect.texture_id = 0;
	m->warp_gate_effect.u1 = 0.0;
	m->warp_gate_effect.u2 = 0.01;
	m->rotate_randomly = 0;
}

void material_init_black_hole(struct material *m)
{
	m->type = MATERIAL_BLACK_HOLE;
	m->billboard_type = MATERIAL_BILLBOARD_TYPE_SPHERICAL;
	m->black_hole.disc_radius = 0.4;  /* overwritten by material_black_hole_set_geometry() */
	m->black_hole.edge_softness = 0.01; /* just enough ramp to antialias the rim, no more: the
					     * whole point of drawing this procedurally is that the
					     * edge sits at a known angle, and a wide fade would put
					     * it back where it was with the painted blob */
	/* Deliberately faint.  A black hole should read as an absence -- found by the way the
	 * starfield bends around it and by the hole it punches in that field, not by glowing.  A
	 * bright rim makes it look like an ordinary lit object with a highlight, so this is set
	 * just high enough to define the horizon's edge against a dark sky and no higher.  Tuned
	 * by eye in shadow_lab. */
	m->black_hole.ring_brightness = 0.2;
	m->black_hole.ring_width = 0.05;
	/* The Einstein ring.  Left out until material_black_hole_set_geometry() says how far out it
	 * belongs, since that depends on the lens strength and not on the material.  The brightness
	 * and width carry over the values the lensed skybox used to draw it with. */
	m->black_hole.einstein_radius = 0.0;
	m->black_hole.glow_brightness = 0.04;
	m->black_hole.glow_width = 0.06;
	m->black_hole.ring_color = sng_get_color(WHITE);
	m->rotate_randomly = 0;
}

float material_black_hole_set_geometry(struct material *m, float lens_strength)
{
	/* Below a lens strength of 1 the Einstein ring falls inside the horizon and is covered by
	 * the disc, so the horizon is the outermost thing the quad has to hold.  Taking the larger
	 * of the two keeps the billboard from collapsing onto the disc in that case. */
	float outer = lens_strength > 1.0 ? lens_strength : 1.0;
	float half_size = BLACK_HOLE_BILLBOARD_MARGIN * outer;

	/* Half the quad spans half_size horizon radii and reaches UV 0.5, so a feature at k horizon
	 * radii sits at 0.5 * k / half_size. */
	m->black_hole.disc_radius = 0.5 / half_size;
	m->black_hole.einstein_radius = 0.5 * lens_strength / half_size;
	return half_size;
}
