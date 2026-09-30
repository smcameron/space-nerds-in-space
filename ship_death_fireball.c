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
#include <math.h>

#include "quat.h"
#include "vertex.h"
#include "triangle.h"
#include "mesh.h"
#include "snis_graph.h"
#include "material.h"
#include "star_light.h"

#define DEFINE_SHIP_DEATH_FIREBALL_GLOBALS
#include "ship_death_fireball.h"

/* Shared by every fireball: a unit quad scaled to the radius -- the shader widens it for
 * perspective itself -- and the flash's glare. */
static struct mesh *billboard;
static struct mesh *flash_mesh;

int ship_death_fireball_setup(void)
{
	struct ship_death_fireball_tuning *tu = &ship_death_fireball_tuning;

	tu->lifetime = 3.0f;
	tu->size = 3.0f;
	tu->start = 0.05f;
	tu->burst = 0.015f;
	tu->brightness = 6.0f;
	tu->flash_gain = 6.0f;
	tu->flash_time = 0.06f;
	tu->light = 3.0f;
	tu->flash_size = 40.0f;
	tu->flash_glare = 60.0f;
	material_init_explosion(&tu->material);

	billboard = mesh_fabricate_billboard(2.0f, 2.0f);
	flash_mesh = mesh_fabricate_billboard(1.0f, 1.0f);
	if (!billboard || !flash_mesh) {
		ship_death_fireball_teardown();
		return -1;
	}
	return 0;
}

void ship_death_fireball_teardown(void)
{
	if (billboard)
		mesh_free(billboard);
	billboard = NULL;
	if (flash_mesh)
		mesh_free(flash_mesh);
	flash_mesh = NULL;
}

/* How far the gas has gone by a given age, for a unit coasting speed.  It starts at rest and is
 * pushed up to speed by the blast's pressure, which weakens as the gas spreads: so the speed
 * closes on its coasting value as 1 - e^(-age / burst), and this is that integrated.  Flat at
 * the flash, then a curve, then a straight line with the burst spent. */
static float travel(float age)
{
	float burst = ship_death_fireball_tuning.burst;

	if (burst < 0.001f)
		return age;
	return age - burst * (1.0f - expf(-age / burst));
}

/* Radius at a given age: start of size at the flash, size as the fire goes out at smoke_start,
 * and coasting on at a constant speed after that. */
static float radius_at(const struct ship_death_fireball *fb, float age)
{
	struct ship_death_fireball_tuning *tu = &ship_death_fireball_tuning;
	float fire_out = tu->material.explosion.smoke_start;

	if (fire_out < 0.01f)
		fire_out = 0.01f;
	return tu->size * fb->ship_radius *
		(tu->start + (1.0f - tu->start) * travel(age) / travel(fire_out));
}

/* The same gas in a bigger ball: density goes as the inverse cube of the radius.  Only once the
 * fire is out, though -- the fireball is still being fed by the ship, and diluting it would have
 * it going translucent while it should be at its most solid. */
static float dilution_at(const struct ship_death_fireball *fb, float age)
{
	float fire_out = ship_death_fireball_tuning.material.explosion.smoke_start;
	float ratio;

	if (age <= fire_out)
		return 1.0f;
	ratio = radius_at(fb, fire_out) / radius_at(fb, age);
	return ratio * ratio * ratio;
}

void ship_death_fireball_init(struct ship_death_fireball *fb, const union vec3 *pos,
				float ship_radius, float seed)
{
	fb->pos = *pos;
	fb->ship_radius = ship_radius;
	fb->seed = seed;
	material_init_sun(&fb->flash_material);
	fb->flash_material.sun.disc_radius = 0.004;
	ship_death_fireball_set_age(fb, 0.0f);
}

void ship_death_fireball_set_age(struct ship_death_fireball *fb, float age)
{
	struct ship_death_fireball_tuning *tu = &ship_death_fireball_tuning;

	fb->age = age;
	fb->material = tu->material;
	fb->material.explosion.age = age;
	fb->material.explosion.seed = fb->seed;
	fb->radius_now = radius_at(fb, age);
	fb->material.explosion.dilution = dilution_at(fb, age);
	fb->flash_now = 1.0f + tu->flash_gain * expf(-age * tu->lifetime / tu->flash_time);
	fb->material.explosion.brightness = tu->brightness * fb->flash_now;
}

float ship_death_fireball_radius_at_time(const struct ship_death_fireball *fb, float seconds)
{
	return radius_at(fb, seconds / ship_death_fireball_tuning.lifetime);
}

/* The flash's glare, for as long as it is worth drawing: white at the peak temperature's colour,
 * dying with the flash. */
static void add_flash(struct ship_death_fireball *fb, struct ship_death_frame *f)
{
	struct ship_death_fireball_tuning *tu = &ship_death_fireball_tuning;
	float glare = tu->flash_glare * (fb->flash_now - 1.0f) / fmaxf(tu->flash_gain, 0.001f);
	struct ship_death_drawable *d;
	float r, g, b;

	/* Stopped while still bright: the star shader's pinpoint disc has real coverage, and once
	 * the glare no longer drowns it, it shows as a grey dot on the fire.  A fraction of the
	 * peak rather than a fixed level, so turning the glare up does not bring the dot back; at
	 * the defaults it is gone about 0.11s in. */
	if (!flash_mesh || glare < 0.15f * tu->flash_glare)
		return;
	star_light_blackbody_color(fb->material.explosion.peak_temp, &r, &g, &b);
	fb->flash_material.sun.color.red = r;
	fb->flash_material.sun.color.green = g;
	fb->flash_material.sun.color.blue = b;
	fb->flash_material.sun.brightness = glare;
	d = ship_death_frame_add(f, flash_mesh, &fb->flash_material, &fb->pos,
				tu->flash_size * fb->ship_radius);
	if (!d)
		return;
	d->no_shade = 1;
	d->no_cast_shadow = 1;
}

void ship_death_fireball_draw(struct ship_death_fireball *fb, struct ship_death_frame *f)
{
	struct ship_death_drawable *d;

	/* At age 1 the shader's erosion has already left nothing, so stopping here is not a pop --
	 * it is only not paying to march through empty space. */
	if (!billboard || fb->age >= 1.0f)
		return;
	d = ship_death_frame_add(f, billboard, &fb->material, &fb->pos, fb->radius_now);
	if (!d)
		return;
	/* Self-lit and self-shaded: an analytic shade term would darken the fire, and as a shadow
	 * caster the quad would cast a square. */
	d->no_shade = 1;
	d->no_cast_shadow = 1;
	add_flash(fb, f);
}

/* Hottest and brightest at the flash, reddening and dying as the fire cools -- the same heat curve
 * the shader burns with, so the light cannot disagree with the fire.
 *
 * Spread over the ball's surface and seen from d away, the light goes as (R/d)^2, which is also
 * why it stays put rather than soaring as the ball grows: a bigger ball is a cooler one. */
int ship_death_fireball_light(const struct ship_death_fireball *fb, const union vec3 *pos,
				union vec3 *light_pos, float color[3], float *wrap)
{
	struct ship_death_fireball_tuning *tu = &ship_death_fireball_tuning;
	float heat = expf(-fb->material.explosion.cooling * fb->age);
	float kelvin = fb->material.explosion.peak_temp * heat * 0.6f;
	float glow = tu->brightness * fb->flash_now * powf(heat, fb->material.explosion.radiance);
	float r, g, b, dist, falloff;
	union vec3 to;

	if (fb->age >= 1.0f || kelvin < 900.0f || tu->light <= 0.0f)
		return 0;
	star_light_blackbody_color(kelvin, &r, &g, &b);
	glow *= tu->light * fminf((kelvin - 900.0f) / 800.0f, 1.0f);
	vec3_sub(&to, pos, &fb->pos);
	dist = fmaxf(vec3_magnitude(&to), fb->radius_now);
	falloff = fminf(glow * (fb->radius_now / dist) * (fb->radius_now / dist), 50.0f);
	*light_pos = fb->pos;
	color[0] = r * falloff;
	color[1] = g * falloff;
	color[2] = b * falloff;
	/* Seen from outside, the fire is a broad source, and wraps part way round what it lights.
	 * From INSIDE it is all around: the glowing gas is on every side, so every face is lit.
	 * Without this, wreckage caught in the fireball shows its unlit outer faces as black holes
	 * punched in the glow.  A wrap of 3 inside the ball -- even a face turned straight away from
	 * the centre looks out into glowing gas, and gets half -- easing to the outside value by
	 * twice its radius out. */
	*wrap = 0.3f + 2.7f * fminf(fmaxf(2.0f - dist / fb->radius_now, 0.0f), 1.0f);
	return 1;
}

/* The gas between eye and p, through a ball of the fireball's mean density at its age.  The mean
 * is what explosion.shader's density comes to with the noise averaged out: the dilution, times
 * how much of the body the erosion has left -- its body averages about 0.78 before erosion, and
 * the shader triples it and clamps -- and the edge taken in to where the lumps put it on average.
 * A chord through that, in the ball's radii, at the shader's extinction. */
float ship_death_fireball_transmittance(const struct ship_death_fireball *fb,
				const union vec3 *eye, const union vec3 *p)
{
	const struct material_explosion *m = &fb->material.explosion;
	float radius = fb->radius_now * (1.0f - 0.5f * m->lumpiness);
	float erosion, body, len, b, c, h, t0, t1;
	union vec3 rd, ro;

	if (!billboard || fb->age >= 1.0f || radius <= 0.0f)
		return 1.0f;
	erosion = powf(fminf(fmaxf((fb->age - m->smoke_start) / (1.0f - m->smoke_start),
			0.0f), 1.0f), m->shred) * 1.25f;
	body = fminf(fmaxf((0.78f - erosion) * 3.0f, 0.0f), 1.0f) * m->dilution;
	if (body <= 0.0f)
		return 1.0f;
	/* Where the ray from the eye to p is inside the ball, short of p. */
	vec3_sub(&rd, p, eye);
	len = vec3_magnitude(&rd);
	if (len <= 0.0f)
		return 1.0f;
	vec3_mul_self(&rd, 1.0f / len);
	vec3_sub(&ro, eye, &fb->pos);
	b = vec3_dot(&ro, &rd);
	c = vec3_dot(&ro, &ro) - radius * radius;
	h = b * b - c;
	if (h <= 0.0f)
		return 1.0f;
	h = sqrtf(h);
	t0 = fmaxf(-b - h, 0.0f);
	t1 = fminf(-b + h, len);
	if (t1 <= t0)
		return 1.0f;
	return expf(-body * m->density * (t1 - t0) / fb->radius_now);
}
