#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "planetary_ring_data.h"
#include "mtwist.h"
#include "quat.h"
#include "mathutils.h"
#include "png_utils.h"

#define PLANETARY_RING_ALPHA_TABLE_SIZE 64
static uint8_t planetary_ring_alpha_table[PLANETARY_RING_ALPHA_TABLE_SIZE][PLANETARY_RING_ALPHA_TABLE_SIZE];

static void init_planetary_ring_alpha_table(const char *ring_texture_path)
{
	char whynot[256];
	char *pixels;
	int w = 0, h = 0, has_alpha = 0;
	int bytes_per_pixel, row_bytes;
	int gx, gy;

	memset(planetary_ring_alpha_table, 0, sizeof(planetary_ring_alpha_table));
	if (!ring_texture_path)
		return;

	/* Flip vertically to match OpenGL texture coordinates (v=0 at bottom). */
	pixels = png_utils_read_png_image(ring_texture_path, 1, 0, 0,
					&w, &h, &has_alpha, whynot, sizeof(whynot));
	if (!pixels) {
		fprintf(stderr, "Failed to load planetary ring texture '%s': %s\n",
			ring_texture_path, whynot);
		return;
	}
	if (w <= 0 || h <= 0) {
		free(pixels);
		return;
	}

	bytes_per_pixel = has_alpha ? 4 : 3;
	row_bytes = (w * bytes_per_pixel + 3) & ~3;

	for (gy = 0; gy < PLANETARY_RING_ALPHA_TABLE_SIZE; gy++) {
		int y0 = (gy * h) / PLANETARY_RING_ALPHA_TABLE_SIZE;
		int y1 = ((gy + 1) * h) / PLANETARY_RING_ALPHA_TABLE_SIZE;

		if (y0 >= h)
			y0 = h - 1;
		if (y1 <= y0)
			y1 = y0 + 1;
		if (y1 > h)
			y1 = h;

		for (gx = 0; gx < PLANETARY_RING_ALPHA_TABLE_SIZE; gx++) {
			int x0 = (gx * w) / PLANETARY_RING_ALPHA_TABLE_SIZE;
			int x1 = ((gx + 1) * w) / PLANETARY_RING_ALPHA_TABLE_SIZE;
			uint32_t sum = 0;
			uint32_t count = 0;
			int x, y;

			if (x0 >= w)
				x0 = w - 1;
			if (x1 <= x0)
				x1 = x0 + 1;
			if (x1 > w)
				x1 = w;

			for (y = y0; y < y1; y++) {
				const unsigned char *row = (const unsigned char *) pixels + y * row_bytes;

				for (x = x0; x < x1; x++) {
					sum += has_alpha ? row[x * 4 + 3] : 255;
					count++;
				}
			}
			planetary_ring_alpha_table[gy][gx] = (uint8_t) ((sum + count / 2) / count);
		}
	}

	free(pixels);
}

void init_planetary_ring_data(struct planetary_ring_data ring_data[],
				int nplanetary_rings, uint32_t mtwist_seed,
				const char *ring_texture_path)
{

	int i;
	struct mtwist_state *mt;
	float r1, r2;

	init_planetary_ring_alpha_table(ring_texture_path);

	mt = mtwist_init(mtwist_seed);

	for (i = 0; i < nplanetary_rings; i++) {
		ring_data[i].alpha = 1.0;
		/* texture_v is the vertical offset into the texture in share/snis/planetary-ring0.png.
		 * We are choosing a horizontal row of pixels in this texture to serve as a cross section
		 * of the planetary ring.
		 */
		ring_data[i].texture_v = (float) i / (float) nplanetary_rings;

		r1 = fabsf(mtwist_float(mt) * (MAX_RING_RADIUS - MIN_RING_RADIUS - 0.5f)) + MIN_RING_RADIUS + 0.5f;
		r2 = fabsf(mtwist_float(mt) * (MAX_RING_RADIUS - MIN_RING_RADIUS - 0.5f)) + MIN_RING_RADIUS + 0.5f;

		if (r1 > r2) {
			ring_data[i].inner_radius = r2;
			ring_data[i].outer_radius = r1;
		} else {
			ring_data[i].inner_radius = r1;
			ring_data[i].outer_radius = r2;
		}
	}
	/* Because of the way that planet rings are chosen based on object id
	 * and because of the way planets are generated and object ids are handed
	 * out we want to scramble the order of ring_data[i].texture_v
	 * so that consecutively generated planets will not have rings that are
	 * too similar.
	 */
	for (i = 0; i < nplanetary_rings; i++) {
		int n = ((int) (mtwist_float(mt) * nplanetary_rings * 100)) % nplanetary_rings;
		float x = ring_data[n].texture_v;
		ring_data[n].texture_v = ring_data[i].texture_v;
		ring_data[i].texture_v = x;
	}
	mtwist_free(mt);
}


/* Empirical unscaled model radii matching snis_server.c and snis_client.c */
static const float ring_asteroid_radius[] = {
	31.835129f,
	43.656696f,
	50.246250f,
	45.869514f,
};

static uint32_t ring_hash_u32(uint32_t x)
{
	x ^= x >> 16;
	x *= 0x7feb352dU;
	x ^= x >> 15;
	x *= 0x846ca68bU;
	x ^= x >> 16;
	return x;
}

static uint32_t ring_cell_hash(uint32_t planet_seed, int ix, int iy,
				int sub_index, uint32_t salt)
{
	uint32_t h = ring_hash_u32(planet_seed ^ salt);

	h = ring_hash_u32(h ^ (uint32_t) ix);
	h = ring_hash_u32(h ^ (uint32_t) iy);
	h = ring_hash_u32(h ^ (uint32_t) sub_index);
	return h;
}

static float ring_hash_float(uint32_t h)
{
	return (float) (h & 0xffffffU) / (float) 0x1000000U;
}

static void ring_uniform_unit_vec(uint32_t h1, uint32_t h2, union vec3 *out)
{
	float z = 2.0f * ring_hash_float(h1) - 1.0f;
	float phi = 2.0f * (float) M_PI * ring_hash_float(h2);
	float r = sqrtf(fmaxf(0.0f, 1.0f - z * z));

	out->v.x = r * cosf(phi);
	out->v.y = r * sinf(phi);
	out->v.z = z;
}

float planetary_ring_sample_density(float u, float texture_v)
{
	float x, y, fx, fy;
	float v00, v10, v01, v11, v0, v1;
	int x0, y0, x1, y1;

	if (u <= 0.0f || u >= 1.0f)
		return 0.0f;

	x = clampf(u, 0.0f, 1.0f) * 63.0f;
	y = clampf(texture_v, 0.0f, 1.0f) * 63.0f;
	x0 = (int) x;
	y0 = (int) y;
	if (x0 >= 63)
		x0 = 63;
	if (y0 >= 63)
		y0 = 63;
	x1 = (x0 < 63) ? x0 + 1 : 63;
	y1 = (y0 < 63) ? y0 + 1 : 63;
	fx = x - (float) x0;
	fy = y - (float) y0;

	v00 = (float) planetary_ring_alpha_table[y0][x0];
	v10 = (float) planetary_ring_alpha_table[y0][x1];
	v01 = (float) planetary_ring_alpha_table[y1][x0];
	v11 = (float) planetary_ring_alpha_table[y1][x1];

	v0 = v00 + fx * (v10 - v00);
	v1 = v01 + fx * (v11 - v01);
	return (v0 + fy * (v1 - v0)) / 255.0f;
}

void planetary_ring_basis(const union quat *planet_orientation,
			union vec3 *u_axis, union vec3 *v_axis, union vec3 *normal)
{
	union vec3 ref;
	union vec3 z_axis = { { 0.0f, 0.0f, 1.0f } };
	float dot;

	quat_rot_vec(normal, &z_axis, planet_orientation);
	vec3_normalize_self(normal);

	if (fabsf(normal->v.x) < 0.9f) {
		ref.v.x = 1.0f;
		ref.v.y = 0.0f;
		ref.v.z = 0.0f;
	} else {
		ref.v.x = 0.0f;
		ref.v.y = 1.0f;
		ref.v.z = 0.0f;
	}

	dot = vec3_dot(&ref, normal);
	u_axis->v.x = ref.v.x - dot * normal->v.x;
	u_axis->v.y = ref.v.y - dot * normal->v.y;
	u_axis->v.z = ref.v.z - dot * normal->v.z;
	vec3_normalize_self(u_axis);

	vec3_cross(v_axis, normal, u_axis);
	vec3_normalize_self(v_axis);
}

static int ring_cell_rock_with_basis(const union vec3 *planet_pos,
				const union vec3 *u_axis,
				const union vec3 *v_axis,
				const union vec3 *normal,
				float planet_radius,
				const struct planetary_ring_data *ring,
				uint32_t planet_seed, int ix, int iy, int sub_index,
				struct planetary_ring_rock *out)
{
	float jx, jy, jz, cx, cy, dist_mult, u, density, spawn_u, size_u;
	float desired_r, base_angle, spin_mag;
	union vec3 base_axis;
	uint32_t h_spawn, h_model, h_size, h_rate;

	if (!ring || planet_radius <= 0.0f)
		return 0;

	if (sub_index < 0 || sub_index > 2)
		return 0;

	jx = (ring_hash_float(ring_cell_hash(planet_seed, ix, iy, sub_index, 1)) - 0.5f) * 0.76f;
	jy = (ring_hash_float(ring_cell_hash(planet_seed, ix, iy, sub_index, 2)) - 0.5f) * 0.76f;
	cx = ((float) ix + 0.5f + jx) * PLANETARY_RING_CELL_SIZE;
	cy = ((float) iy + 0.5f + jy) * PLANETARY_RING_CELL_SIZE;

	dist_mult = hypotf(cx, cy) / planet_radius;
	if (dist_mult <= ring->inner_radius || dist_mult >= ring->outer_radius)
		return 0;

	u = fmap(dist_mult, ring->inner_radius, ring->outer_radius, 0.0f, 1.0f);
	density = planetary_ring_sample_density(u, ring->texture_v);
	if (density < 0.15f)
		return 0;

	h_spawn = ring_cell_hash(planet_seed, ix, iy, sub_index, 3);
	spawn_u = ring_hash_float(h_spawn);
	if (sub_index == 0) {
		if (spawn_u >= clampf(density * 0.95f, 0.0f, 0.92f))
			return 0;
	} else if (sub_index == 1) {
		if (density < 0.35f || spawn_u >= density * 0.80f)
			return 0;
	} else {
		if (density < 0.65f || spawn_u >= density * 0.60f)
			return 0;
	}

	if (!out)
		return 1;

	jz = (ring_hash_float(ring_cell_hash(planet_seed, ix, iy, sub_index, 4)) - 0.5f) *
		2.0f * PLANETARY_RING_HALF_THICKNESS;

	out->pos.v.x = planet_pos->v.x + cx * u_axis->v.x + cy * v_axis->v.x + jz * normal->v.x;
	out->pos.v.y = planet_pos->v.y + cx * u_axis->v.y + cy * v_axis->v.y + jz * normal->v.y;
	out->pos.v.z = planet_pos->v.z + cx * u_axis->v.z + cy * v_axis->v.z + jz * normal->v.z;

	h_model = ring_cell_hash(planet_seed, ix, iy, sub_index, 5);
	out->model = (int) (h_model % (sizeof(ring_asteroid_radius) / sizeof(ring_asteroid_radius[0])));
	out->material_index = (int) (ring_cell_hash(planet_seed, ix, iy, sub_index, 6) & 1U);

	h_size = ring_cell_hash(planet_seed, ix, iy, sub_index, 7);
	size_u = ring_hash_float(h_size);
	size_u = size_u * size_u;
	if (sub_index == 0) {
		desired_r = (5.0f + 15.0f * size_u) * (0.70f + 0.30f * density);
		if (desired_r > PLANETARY_RING_MAX_ROCK_RADIUS)
			desired_r = PLANETARY_RING_MAX_ROCK_RADIUS;
	} else {
		desired_r = 1.5f + 4.0f * size_u;
	}
	out->radius = desired_r;
	out->scale = desired_r / ring_asteroid_radius[out->model];

	ring_uniform_unit_vec(ring_cell_hash(planet_seed, ix, iy, sub_index, 8),
			ring_cell_hash(planet_seed, ix, iy, sub_index, 9), &base_axis);
	base_angle = 2.0f * (float) M_PI *
		ring_hash_float(ring_cell_hash(planet_seed, ix, iy, sub_index, 10));
	quat_init_axis_v(&out->base_orientation, &base_axis, base_angle);

	ring_uniform_unit_vec(ring_cell_hash(planet_seed, ix, iy, sub_index, 11),
			ring_cell_hash(planet_seed, ix, iy, sub_index, 12), &out->spin_axis);
	h_rate = ring_cell_hash(planet_seed, ix, iy, sub_index, 13);
	spin_mag = 0.08f + 0.40f * ring_hash_float(h_rate);
	out->spin_rate = (h_rate & 0x80000000U) ? spin_mag : -spin_mag;

	return 1;
}

int planetary_ring_cell_rock(const union vec3 *planet_pos,
			const union quat *planet_orientation,
			float planet_radius,
			const struct planetary_ring_data *ring,
			uint32_t planet_seed, int ix, int iy, int sub_index,
			struct planetary_ring_rock *out)
{
	union vec3 u_axis, v_axis, normal;

	planetary_ring_basis(planet_orientation, &u_axis, &v_axis, &normal);
	return ring_cell_rock_with_basis(planet_pos, &u_axis, &v_axis, &normal,
					planet_radius, ring, planet_seed,
					ix, iy, sub_index, out);
}

void planetary_ring_rock_spin_orientation(const struct planetary_ring_rock *rock,
					double time_sec, union quat *out)
{
	union quat spin_q;
	double angle = fmod((double) rock->spin_rate * time_sec, 2.0 * M_PI);

	quat_init_axis_v(&spin_q, &rock->spin_axis, (float) angle);
	quat_mul(out, &rock->base_orientation, &spin_q);
}

int planetary_ring_cell_dust(const union vec3 *planet_pos,
			const union quat *planet_orientation,
			float planet_radius,
			const struct planetary_ring_data *ring,
			uint32_t planet_seed, int ix, int iy, int sub_index,
			float half_thickness,
			struct planetary_ring_dust_puff *out)
{
	union vec3 u_axis, v_axis, normal;
	float jx, jy, cx, cy, dist_mult, u, density, spawn_u, z_u, z_frac, jz;
	float vert_falloff, size_u;

	if (!ring || planet_radius <= 0.0f || half_thickness <= 0.0f)
		return 0;

	if (sub_index < 0 || sub_index > 2)
		return 0;

	jx = (ring_hash_float(ring_cell_hash(planet_seed, ix, iy, sub_index, 101)) - 0.5f) * 0.85f;
	jy = (ring_hash_float(ring_cell_hash(planet_seed, ix, iy, sub_index, 102)) - 0.5f) * 0.85f;
	cx = ((float) ix + 0.5f + jx) * PLANETARY_RING_DUST_CELL_SIZE;
	cy = ((float) iy + 0.5f + jy) * PLANETARY_RING_DUST_CELL_SIZE;

	dist_mult = hypotf(cx, cy) / planet_radius;
	if (dist_mult <= ring->inner_radius || dist_mult >= ring->outer_radius)
		return 0;

	u = fmap(dist_mult, ring->inner_radius, ring->outer_radius, 0.0f, 1.0f);
	density = planetary_ring_sample_density(u, ring->texture_v);
	if (density < 0.12f)
		return 0;

	spawn_u = ring_hash_float(ring_cell_hash(planet_seed, ix, iy, sub_index, 103));
	if (sub_index == 0) {
		if (spawn_u >= clampf(density * 0.95f, 0.0f, 0.92f))
			return 0;
	} else if (sub_index == 1) {
		if (density < 0.25f || spawn_u >= density * 0.80f)
			return 0;
	} else {
		if (density < 0.45f || spawn_u >= density * 0.65f)
			return 0;
	}

	if (!out)
		return 1;

	z_u = ring_hash_float(ring_cell_hash(planet_seed, ix, iy, sub_index, 104)) * 2.0f - 1.0f;
	if (sub_index == 0)
		z_frac = z_u * 0.25f;
	else if (sub_index == 1)
		z_frac = z_u * 0.65f;
	else
		z_frac = z_u;

	jz = z_frac * half_thickness;
	vert_falloff = expf(-0.5f * (z_frac / 0.45f) * (z_frac / 0.45f));

	planetary_ring_basis(planet_orientation, &u_axis, &v_axis, &normal);
	out->pos.v.x = planet_pos->v.x + cx * u_axis.v.x + cy * v_axis.v.x + jz * normal.v.x;
	out->pos.v.y = planet_pos->v.y + cx * u_axis.v.y + cy * v_axis.v.y + jz * normal.v.y;
	out->pos.v.z = planet_pos->v.z + cx * u_axis.v.z + cy * v_axis.v.z + jz * normal.v.z;

	size_u = ring_hash_float(ring_cell_hash(planet_seed, ix, iy, sub_index, 105));
	out->radius = (220.0f + 160.0f * size_u) * (0.75f + 0.25f * density);
	out->alpha = clampf(density * vert_falloff * (0.08f + 0.07f * size_u), 0.0f, 0.25f);
	out->seed = ring_hash_float(ring_cell_hash(planet_seed, ix, iy, sub_index, 106));

	return 1;
}

int planetary_ring_find_rock_collision(const union vec3 *obj_pos, float obj_radius,
				const union vec3 *planet_pos,
				const union quat *planet_orientation,
				float planet_radius,
				const struct planetary_ring_data *ring,
				uint32_t planet_seed,
				struct planetary_ring_rock *hit_rock)
{
	union vec3 u_axis, v_axis, normal, rel;
	float dz, ux, vy, dist_plane, inner_r, outer_r, pad;
	float best_pen = -1.0f;
	int ix0, iy0, di, dj;
	int found = 0;

	if (!ring || planet_radius <= 0.0f)
		return 0;

	planetary_ring_basis(planet_orientation, &u_axis, &v_axis, &normal);
	vec3_sub(&rel, obj_pos, planet_pos);
	dz = vec3_dot(&rel, &normal);
	pad = PLANETARY_RING_MAX_ROCK_RADIUS + obj_radius;
	if (fabsf(dz) > PLANETARY_RING_HALF_THICKNESS + pad)
		return 0;

	ux = vec3_dot(&rel, &u_axis);
	vy = vec3_dot(&rel, &v_axis);
	dist_plane = hypotf(ux, vy);
	inner_r = ring->inner_radius * planet_radius;
	outer_r = ring->outer_radius * planet_radius;
	if (dist_plane < inner_r - pad || dist_plane > outer_r + pad)
		return 0;

	ix0 = (int) floorf(ux / PLANETARY_RING_CELL_SIZE);
	iy0 = (int) floorf(vy / PLANETARY_RING_CELL_SIZE);

	for (di = -1; di <= 1; di++) {
		for (dj = -1; dj <= 1; dj++) {
			struct planetary_ring_rock rock;
			float d2, rsum, pen;

			if (!ring_cell_rock_with_basis(planet_pos, &u_axis, &v_axis,
							&normal, planet_radius, ring,
							planet_seed, ix0 + di, iy0 + dj,
							0, &rock))
				continue;

			d2 = vec3_dist_sqrd(obj_pos, &rock.pos);
			rsum = rock.radius + obj_radius;
			if (d2 <= rsum * rsum) {
				pen = rsum - sqrtf(d2);
				if (!found || pen > best_pen) {
					best_pen = pen;
					if (hit_rock)
						*hit_rock = rock;
					found = 1;
				}
			}
		}
	}

	return found;
}

int collides_with_planetary_ring(const union vec3 *object_position, const union vec3 *planet_position,
				const union quat *planet_orientation, float planet_radius,
				float ring_inner_radius, float ring_outer_radius,
				float ring_texture_v)
{
	float d, dist_to_plane, inner, outer, dist, u;
	union vec3 ring_normal;

	inner = fmap(ring_inner_radius, 1.0, 4.0, 0.0, 1.0);
	outer = fmap(ring_outer_radius, 1.0, 4.0, 0.0, 1.0);
	dist = vec3_dist(object_position, planet_position) / planet_radius;
	d = fmap(dist, 1.0, 4.0, 0.0, 1.0);

	if (d < inner)
		return 0;
	if (d > outer)
		return 0;

	ring_normal.v.x = 0.0;
	ring_normal.v.y = 0.0;
	ring_normal.v.z = 1.0;

	quat_rot_vec_self(&ring_normal, planet_orientation);
	dist_to_plane = fabsf(plane_to_point_dist(*planet_position, ring_normal, *object_position));
	if (dist_to_plane >= PLANETARY_RING_DAMAGE_DIST)
		return 0;

	u = fmap(d, inner, outer, 0.0f, 1.0f);
	return planetary_ring_sample_density(u, ring_texture_v) >= 0.15f;
}

