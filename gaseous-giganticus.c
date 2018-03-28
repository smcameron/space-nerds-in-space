/*
	Copyright (C) 2014 Stephen M. Cameron
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
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <errno.h>
#include <limits.h>
#include <pthread.h>
#include <getopt.h>
#include <locale.h>
#include <sys/time.h>

#include "mtwist.h"
#include "mathutils.h"
#include "quat.h"
#include "open-simplex-noise.h"
#include "png_utils.h"
#include "pthread_util.h"

static struct osn_context *ctx;

#define NPARTICLES 8000000
static int particle_count = NPARTICLES;

static int nthreads = 4;
static int user_threads = -1;
static const int image_threads = 6; /* for 6 faces of cubemap, don't change this */
static char *default_output_file_prefix = "gasgiant-";
static char *default_input_file = "gasgiant-input.png";
static char *vf_dump_file = NULL;
static int restore_vf_data = 0;
static char *output_file_prefix;
static char *input_file;
static char *cubemap_prefix = NULL;
static int nofade = 0;
static int stripe = 0;
static int sinusoidal = 1;
static int use_wstep = 0;
static int wstep_period = 10;
static float wstep = 0.0f;
#define FBM_DEFAULT_FALLOFF (0.5)
static float fbm_falloff = FBM_DEFAULT_FALLOFF; 
static float gain = 1.0;
static float ff[] = {
	1.0,
	FBM_DEFAULT_FALLOFF,
	FBM_DEFAULT_FALLOFF * FBM_DEFAULT_FALLOFF,
	FBM_DEFAULT_FALLOFF * FBM_DEFAULT_FALLOFF * FBM_DEFAULT_FALLOFF,
	FBM_DEFAULT_FALLOFF * FBM_DEFAULT_FALLOFF * FBM_DEFAULT_FALLOFF * FBM_DEFAULT_FALLOFF,
	FBM_DEFAULT_FALLOFF * FBM_DEFAULT_FALLOFF * FBM_DEFAULT_FALLOFF * FBM_DEFAULT_FALLOFF *
			FBM_DEFAULT_FALLOFF,
	FBM_DEFAULT_FALLOFF * FBM_DEFAULT_FALLOFF * FBM_DEFAULT_FALLOFF * FBM_DEFAULT_FALLOFF *
			FBM_DEFAULT_FALLOFF * FBM_DEFAULT_FALLOFF,
};
static int noise_levels = 4;
static const int max_noise_levels = 7;
static int cloudmode = 0;
static float fade_rate = -1.0;
static int save_texture_sequence = 0;

#define DIM 1024 /* dimensions of cube map face images */
#define VFDIM 2048 /* dimension of velocity field. (2 * DIM) is reasonable */
static int vfdim = VFDIM;
#define FDIM ((float) (DIM))
#define XDIM DIM
#define YDIM DIM

static int niterations = 1000;
static const float default_noise_scale = 2.6;
static float noise_scale;
static float speed_multiplier = 1.0;
static float velocity_factor = 1200.0;
static float num_bands = 6.0f;
static float band_speed_factor = 2.9f;
static int vertical_bands = 1;
static float opacity = 1.0;
static float opacity_limit = 0.2;
static float pole_attenuation = 0.5;

static char *start_image;
static char *cubemap_image[6] = { 0 };
static int start_image_width, start_image_height, start_image_has_alpha, start_image_bytes_per_row;
static unsigned char *output_image[6];
static int image_save_period = 20;
static float w_offset = 0.0;
static int random_mode;
static int large_pixels = 0;

/* velocity field for 6 faces of a cubemap */
static struct velocity_field {
	union vec3 v[6][VFDIM][VFDIM];
} vf;

struct color {
	float r, g, b, a;
};

struct color darkest_color;
const struct color hot_pink = { 1.0f, 0.07843f, 0.57647f, 0.01 };
static int use_hot_pink = 0;

/* face, i, j -- coords on a cube map */
struct fij {
	int f, i, j;
};

/* particles have color, and exist on the surface of a sphere. */
static struct particle {
	union vec3 pos;
	struct color c;
	struct fij fij;
} *particle;

struct movement_thread_info {
	int first_particle, last_particle;
	pthread_t thread;
	struct particle *p; /* ptr to particle[], above */
	struct velocity_field *vf; /* ptr to vf, above. */
};

union cast {
	double d;
	long l;
};

#define MAXVORTICES 200
static struct vortex {
	union vec3 p;
	union quat q, reverseq;
	float r;
	float angular_vel;
} vort[MAXVORTICES];
static int nvortices = 0;
static float vortex_size = 0.04;
static float vortex_size_variance =  0.02;
static float vortex_band_threshold = 0.2;

static inline int float2int(double d)
{
	volatile union cast c;
	c.d = d + 6755399441055744.0;
	return c.l;
}

static float alphablendcolor(float underchannel, float underalpha, float overchannel, float overalpha)
{
	return overchannel * overalpha + underchannel * underalpha * (1.0 - overalpha);
}

static struct color combine_color(struct color *oc, struct color *c)
{
	struct color nc;

	nc.a = (c->a + oc->a * (1.0f - c->a));
	nc.r = alphablendcolor(oc->r, oc->a, c->r, c->a) / nc.a;
	nc.b = alphablendcolor(oc->b, oc->a, c->b, c->a) / nc.a;
	nc.g = alphablendcolor(oc->g, oc->a, c->g, c->a) / nc.a;
	return nc;
}

static void fade_out_background(int f, struct color *c)
{
	int p, i, j;
	struct color oc, nc;
	unsigned char *pixel;

	for (j = 0; j < DIM; j++) {
		for (i = 0; i < DIM; i++) {
			p = j * DIM + i;
			pixel = &output_image[f][p * 4];
			if (!cloudmode) {
				oc.r = (float) pixel[0] / 255.0f;
				oc.g = (float) pixel[1] / 255.0f;
				oc.b = (float) pixel[2] / 255.0f;
				oc.a = 1.0;
				nc = combine_color(&oc, c);
				pixel[0] = float2int(255.0f * nc.r) & 0xff;
				pixel[1] = float2int(255.0f * nc.g) & 0xff;
				pixel[2] = float2int(255.0f * nc.b) & 0xff;
			} else {
				pixel[3] = float2int((float) pixel[3] * 0.9);
			}
		}
	}
}

static union vec3 fij_to_xyz(int f, int i, int j, const int dim);
static inline float fbmnoise4(float x, float y, float z, float w);

static void paint_particle(int face, int i, int j, struct color *c)
{
	unsigned char *pixel;
	int p;
	struct color oc, nc;

	if (i < 0 || i > DIM - 1 || j < 0 || j > DIM - 1) {
		/* FIXME: We get a handful of these, don't know why.
		 * Some are 1024, some seem to be -MAXINT
		 */
		/* printf("i, j = %d, %d!!!!!1\n", i, j);  */
		return;
	}
	p = j * DIM + i;
	pixel = &output_image[face][p * 4];
#if 0
	pixel[0] = (unsigned char) (255.0f * c->r);
	pixel[1] = (unsigned char) (255.0f * c->g);
	pixel[2] = (unsigned char) (255.0f * c->b);

	pixel[3] = 255;
	return;
#else
	/* FIXME, this is inefficient */
	oc.r = (float) pixel[0] / 255.0f;
	oc.g = (float) pixel[1] / 255.0f;
	oc.b = (float) pixel[2] / 255.0f;
	oc.a = 1.0;
	nc = combine_color(&oc, c);
	if (!cloudmode) {
		pixel[0] = float2int(255.0f * nc.r) & 0xff;
		pixel[1] = float2int(255.0f * nc.g) & 0xff;
		pixel[2] = float2int(255.0f * nc.b) & 0xff;
		pixel[3] = 255;
	} else {
		union vec3 v;
		float n, m;
		v = fij_to_xyz(face, i, j, DIM);
		vec3_normalize_self(&v);
		vec3_mul_self(&v, 3.6 * noise_scale);
		n = fbmnoise4(v.v.x, v.v.y, v.v.z, (w_offset + 10.0) * 3.33f);
		if (n > 0.5f)
			n = n * (1.0 + n - 0.5);
		if (n < 0.0f)
			n = n * (1.0 + n + 0.25);
		if (n > 0.666f)
			n = 0.666f;
		else if (n < -0.333f)
			n = -0.333f;

		// else if (n < -1.0f)
			//n = -1.0f;
		// n = 0.5f * (n + 1.0f);
		n = n + 0.334f;
		m = (nc.r + nc.g + nc.b) / 3.0f;
#if 0
		pixel[0] = float2int(255.0f * m) & 0xff;
		//pixel[0] = 255;
		pixel[1] = pixel[0];
		pixel[2] = pixel[0];
		pixel[3] = float2int(n * (float) pixel[0]) & 0xff;
#endif
		pixel[0] = 255;
		pixel[1] = 255;
		pixel[2] = 255;
		pixel[3] = float2int(255.0f * m * n) & 0xff;
	}
#endif
}

/* convert from cubemap coords to cartesian coords on surface of sphere */
static union vec3 fij_to_xyz(int f, int i, int j, const int dim)
{
	union vec3 answer;

	switch (f) {
	case 0:
		answer.v.x = (float) (i - dim / 2) / (float) dim;
		answer.v.y = -(float) (j - dim / 2) / (float) dim;
		answer.v.z = 0.5;
		break;
	case 1:
		answer.v.x = 0.5;
		answer.v.y = -(float) (j - dim / 2) / (float) dim;
		answer.v.z = -(float) (i - dim / 2) / (float) dim;
		break;
	case 2:
		answer.v.x = -(float) (i - dim / 2) / (float) dim;
		answer.v.y = -(float) (j - dim / 2) / (float) dim;
		answer.v.z = -0.5;
		break;
	case 3:
		answer.v.x = -0.5;
		answer.v.y = -(float) (j - dim / 2) / (float) dim;
		answer.v.z = (float) (i - dim / 2) / (float) dim;
		break;
	case 4:
		answer.v.x = (float) (i - dim / 2) / (float) dim;
		answer.v.y = 0.5;
		answer.v.z = (float) (j - dim / 2) / (float) dim;
		break;
	case 5:
		answer.v.x = (float) (i - dim / 2) / (float) dim;
		answer.v.y = -0.5;
		answer.v.z = -(float) (j - dim / 2) / (float) dim;
		break;
	}
	vec3_normalize_self(&answer);
	return answer;
}

/* convert from cartesian coords on surface of a sphere to cubemap coords */
static struct fij xyz_to_fij(const union vec3 *p, const int dim)
{
	struct fij answer;
	union vec3 t;
	int f, i, j;
	float d;
	const float fdim = (float) dim;

	vec3_normalize(&t, p);

	if (fabs(t.v.x) > fabs(t.v.y)) {
		if (fabs(t.v.x) > fabs(t.v.z)) {
			/* x is longest leg */
			d = fabs(t.v.x);
			if (t.v.x < 0) {
				f = 3;
				i = float2int((t.v.z / d) * fdim * 0.5 + 0.5 * (float) fdim);
			} else {
				f = 1;
				i = float2int((-t.v.z / d)  * fdim * 0.5 + 0.5 * fdim);
			}
		} else {
			/* z is longest leg */
			d = fabs(t.v.z);
			if (t.v.z < 0) {
				f = 2;
				i = float2int((-t.v.x / d) * fdim * 0.5 + 0.5 * fdim);
			} else {
				f = 0;
				i = float2int((t.v.x / d) * fdim * 0.5 + 0.5 * fdim);
#if 0
				/* FIXME: we get this sometimes, not sure why. */
				if (i < 0 || i > 1023)
					printf("i = %d!!!!!!!!!!!!!!!!\n", i);
#endif
			}
		}
		j = float2int((-t.v.y / d) * fdim * 0.5 + 0.5 * fdim);
	} else {
		/* x is not longest leg, y or z must be. */
		if (fabs(t.v.y) > fabs(t.v.z)) {
			/* y is longest leg */
			d = fabs(t.v.y);
			if (t.v.y < 0) {
				f = 5;
				j = float2int((-t.v.z / d) * fdim * 0.5 + 0.5 * fdim);
			} else {
				f = 4;
				j = float2int((t.v.z / d) * fdim * 0.5 + 0.5 * fdim);
			}
			i = float2int((t.v.x / d) * fdim * 0.5 + 0.5 * fdim);
		} else {
			/* z is longest leg */
			d = fabs(t.v.z);
			if (t.v.z < 0) {
				f = 2;
				i = float2int((-t.v.x / d) * fdim * 0.5 + 0.5 * fdim);
			} else {
				f = 0;
				i = float2int((t.v.x / d) * fdim * 0.5 + 0.5 * fdim);
			}
			j = float2int((-t.v.y / d) * fdim * 0.5 + 0.5 * fdim);
		}
	}

	answer.f = f;
	answer.i = i;
	answer.j = j;

	/* FIXME: some other problem makes these checks necessary for now. */
	if (answer.f < 0)
		answer.f = 0;
	else if (answer.f > 5)
		answer.f = 5;
	if (answer.i < 0)
		answer.i = 0;
	else if (answer.i >= dim)
		answer.i = dim - 1;
	if (answer.j < 0)
		answer.j = 0;
	else if (answer.j >= dim)
		answer.j = dim - 1;

	return answer;
}

static const float face_to_xdim_multiplier[] = { 0.25, 0.5, 0.75, 0.0, 0.25, 0.25 };
static const float face_to_ydim_multiplier[] = { 1.0 / 3.0, 1.0 / 3.0, 1.0 / 3.0, 1.0 / 3.0,
						0.0, 2.0 / 3.0 };

/* place particles randomly on the surface of a sphere */
static void init_particles(struct particle **pp, const int nparticles)
{
	float x, y, z, xo, yo;
	const int bytes_per_pixel = start_image_has_alpha ? 4 : 3;
	unsigned char *pixel;
	int pn, px, py;
	struct fij fij = { 0, 0, 0 };
	struct particle *p;

	printf("Initializing %d particles", nparticles); fflush(stdout);
	*pp = malloc(sizeof(**pp) * nparticles);
	p = *pp;

	for (int i = 0; i < nparticles; i++) {
		random_point_on_sphere((float) XDIM / 2.0f, &x, &y, &z);
		p[i].pos.v.x = x;
		p[i].pos.v.y = y;
		p[i].pos.v.z = z;
		if (cubemap_prefix) {
			fij = xyz_to_fij(&p[i].pos, DIM);
			if (fij.i < 0 || fij.i > DIM || fij.j < 0 || fij.j > DIM) {
				printf("BAD fij: %d,%d\n", fij.i, fij.j);
			}
			xo = fij.i * start_image_width / (float) XDIM;
			yo = fij.j * start_image_height / (float) YDIM;
			px = ((int) xo) * bytes_per_pixel;
			py = ((int) yo) * start_image_bytes_per_row;
		} else if (!stripe && !sinusoidal) {
			fij = xyz_to_fij(&p[i].pos, DIM);
			if (fij.i < 0 || fij.i > DIM || fij.j < 0 || fij.j > DIM) {
				printf("BAD fij: %d,%d\n", fij.i, fij.j);
			}
			/* The divide by 4.0 or 3.0 below is there because the faces are laid out like:
			 *   #
			 *  ####   (3 high, and 4 wide.)
			 *   #
			 */
			xo = XDIM * face_to_xdim_multiplier[fij.f] + fij.i / 4.0;
			yo = YDIM * face_to_ydim_multiplier[fij.f] + fij.j / 3.0;
			xo = xo / (float) XDIM;
			yo = yo / (float) YDIM;
			px = ((int) (xo * start_image_width)) * bytes_per_pixel;
			py = (((int) (yo * start_image_height)) * start_image_bytes_per_row);
			//printf("x,y = %d, %d, pn = %d\n", px, py, pn);
		} else if (stripe) {
			if (vertical_bands)
				yo = (z + (float) DIM / 2.0) / (float) DIM;
			else
				yo = (y + (float) DIM / 2.0) / (float) DIM;
			xo = 0.5; 
			px = ((int) (xo * start_image_width)) * bytes_per_pixel;
			py = (((int) (yo * start_image_height)) * start_image_bytes_per_row);
			//printf("xo = %f, yo=%f, px = %d, py = %d\n", xo, yo, px, py);
		} else { /* sinusoidal */
			if (!vertical_bands) {
				float abs_cos_lat, longitude, latitude, x1, x2;

				latitude = asinf(y / ((float) DIM / 2.0f));
				yo = ((latitude + 0.5 * M_PI) / M_PI);
				abs_cos_lat = fabs(cosf(latitude));
				x2 = abs_cos_lat * 0.5 + 0.5;
				x1 = -abs_cos_lat * 0.5 + 0.5;
				longitude = atan2f(z, x);
				xo = (cos(longitude) * abs_cos_lat * 0.5f) * (x2 - x1) + 0.5f;
				px = ((int) (xo * start_image_width)) * bytes_per_pixel;
				py = (((int) (yo * start_image_height)) * start_image_bytes_per_row);
			} else {
				float abs_cos_lat, longitude, latitude, x1, x2;

				latitude = asinf(z / ((float) DIM / 2.0f));
				yo = ((latitude + 0.5 * M_PI) / M_PI);
				abs_cos_lat = fabs(cosf(latitude));
				x2 = abs_cos_lat * 0.5 + 0.5;
				x1 = -abs_cos_lat * 0.5 + 0.5;
				longitude = atan2f(y, x);
				xo = (cos(longitude) * abs_cos_lat * 0.5f) * (x2 - x1) + 0.5f;
				px = ((int) (xo * start_image_width)) * bytes_per_pixel;
				py = (((int) (yo * start_image_height)) * start_image_bytes_per_row);
			}
		}
		pn = py + px;
		//printf("pn = %d\n", pn);
		if (!cubemap_prefix)
			pixel = (unsigned char *) &start_image[pn];
		else
			pixel = (unsigned char *) &cubemap_image[fij.f][pn];
		p[i].c.r = ((float) pixel[0]) / 255.0f;
		p[i].c.g = ((float) pixel[1]) / 255.0f;
		p[i].c.b = ((float) pixel[2]) / 255.0f;
		p[i].c.a = start_image_has_alpha ? (float) pixel[3] / 255.0 : 1.0;
		p[i].c.a = 1.0; //start_image_has_alpha ? (float) pixel[3] / 255.0 : 1.0;
	}
	printf("\n");
}

static inline float fbmnoise4(float x, float y, float z, float w)
{
	return	ff[0] * open_simplex_noise4(ctx, x, y, z, w) +
		ff[1] * open_simplex_noise4(ctx, 2.0f * x, 2.0f * y, 2.0f * z, 2.0f * w) +
		ff[2] * open_simplex_noise4(ctx, 4.0f * x, 4.0f * y, 4.0f * z, 4.0f * w) +
		ff[3] * open_simplex_noise4(ctx, 8.0f * x, 8.0f * y, 8.0f * z, 8.0f * w) +
		ff[4] * open_simplex_noise4(ctx, 16.0f * x, 16.0f * y, 16.0f * z, 16.0f * w) +
		ff[5] * open_simplex_noise4(ctx, 32.0f * x, 32.0f * y, 32.0f * z, 32.0f * w) +
		ff[6] * open_simplex_noise4(ctx, 64.0f * x, 64.0f * y, 64.0f * z, 64.0f * w);
}

/* compute the noise gradient at the given point on the surface of a sphere */
static union vec3 noise_gradient(union vec3 position, float w, float noise_scale)
{
	union vec3 g;
	const float dx = noise_scale * (0.05f / (float) DIM);
	const float dy = noise_scale * (0.05f / (float) DIM);
	const float dz = noise_scale * (0.05f / (float) DIM);

	g.v.x = fbmnoise4(position.v.x + dx, position.v.y, position.v.z, w) -
		fbmnoise4(position.v.x - dx, position.v.y, position.v.z, w);
	g.v.y = fbmnoise4(position.v.x, position.v.y + dy, position.v.z, w) -
		fbmnoise4(position.v.x, position.v.y - dy, position.v.z, w);
	g.v.z = fbmnoise4(position.v.x, position.v.y, position.v.z + dz, w) -
		fbmnoise4(position.v.x, position.v.y, position.v.z - dz, w);
	return g;
}

static union vec3 curl2(union vec3 pos, union vec3 normalized_pos,
			const float pos_magnitude, union vec3 noise_gradient)
{
	union vec3 pos_plus_noise, proj_ng, rotated_ng;
	union quat rotation;

	/* project noise gradient onto sphere surface */
	vec3_add(&pos_plus_noise, &pos, &noise_gradient);
	vec3_normalize_self(&pos_plus_noise);
	vec3_mul_self(&pos_plus_noise, pos_magnitude);
	vec3_sub(&proj_ng, &pos_plus_noise, &pos);

	/* rotate projected noise gradient 90 degrees about pos. */
	quat_init_axis_v(&rotation, &normalized_pos, M_PI / 2.0);
	quat_rot_vec(&rotated_ng, &proj_ng, &rotation);
	return rotated_ng;
}

static float calculate_band_speed(float latitude)
{
	return ((1 - pole_attenuation) + pole_attenuation *
		cosf(latitude)) * cosf(latitude * num_bands) * band_speed_factor;
}

struct velocity_field_thread_info {
	pthread_t thread;
	int f; /* face */
	float w;
	struct velocity_field *vf;
};

/* Update 1 face of the 6 velocity maps (1 face for each side of the cubemap) */ 
static void *update_velocity_field_thread_fn(void *info)
{
	struct velocity_field_thread_info *t = info;
	int f = t->f;
	float w = t->w;
	struct velocity_field *vf = t->vf;

	int i, j, vortex;
	union vec3 v, c, ng;

	for (i = 0; i < vfdim; i++) {
		for (j = 0; j < vfdim; j++) {
			float band_speed, angle;
			union vec3 ov, bv;

			v = fij_to_xyz(f, i, j, vfdim);
			ov = v;
			vec3_mul_self(&v, noise_scale);
			ng = noise_gradient(v, w * noise_scale, noise_scale);
			c = curl2(v, ov, noise_scale, ng);
			vec3_mul(&vf->v[f][i][j], &c, velocity_factor);

			/* calculate counter rotating band influence */
			if (num_bands != 0) {
				if (vertical_bands) {
					angle = asinf(ov.v.z);
					band_speed = calculate_band_speed(angle);
					bv.v.x = -ov.v.y;
					bv.v.y = ov.v.x;
					bv.v.z = 0;
				} else {
					angle = asinf(ov.v.y);
					band_speed = calculate_band_speed(angle);
					bv.v.x = ov.v.z;
					bv.v.z = -ov.v.x;
					bv.v.y = 0;
				}
				vec3_normalize_self(&bv);
				vec3_mul_self(&bv, band_speed);
				vec3_add_self(&vf->v[f][i][j], &bv);
			}

			/* Calculate artificial vortex influence. */
			for (vortex = 0; vortex < nvortices; vortex++) {
				union vec3 diff;
				union vec3 p2d, vortex_vel, rotated_vortex_vel;
				float dist;
				struct vortex *vor = &vort[vortex];
				vec3_sub(&diff, &vor->p, &ov);
				dist = vec3_magnitude(&diff);
				if (dist > vor->r)
					continue; /* This vortex's influence is zero. */
				/* rotate this postition to front and center */
				quat_rot_vec(&p2d, &ov, &vor->q);

				/* do a 2d rotation about z axis by at most 5 degrees */
				float angle = (vor->angular_vel  * M_PI / 180.0) * sin(M_PI * dist / vor->r);
				float nx = p2d.v.x * cos(angle) - p2d.v.y * sin(angle);
				float ny = p2d.v.x * sin(angle) + p2d.v.y * cos(angle);
				vortex_vel.v.x = XDIM * (nx - p2d.v.x);
				vortex_vel.v.y = XDIM * (ny - p2d.v.y);
				vortex_vel.v.z = 0.0f;

				/* Now rotate the velocity back from front and center and add contribution */
				quat_rot_vec(&rotated_vortex_vel, &vortex_vel, &vor->reverseq);
				vec3_add_self(&vf->v[f][i][j], &rotated_vortex_vel);
			}
		}
	}
	return NULL;
}

/* compute velocity field for all cells in cubemap.  It is scaled curl of gradient of noise field */
static void update_velocity_field(struct velocity_field *vf, float noise_scale, float w, int *use_wstep)
{
	struct velocity_field_thread_info t[6];
	void *status;
	int f, rc;
	struct timeval vfbegin, vfend;

	gettimeofday(&vfbegin, NULL);
	printf("Calculating velocity field"); fflush(stdout);
	for (f = 0; f < 6; f++) {
		t[f].f = f;
		t[f].w = w;
		t[f].vf = vf;
		rc = create_thread(&t[f].thread, update_velocity_field_thread_fn, &t[f], "veloc-field", 0);
		if (rc)
			fprintf(stderr, "%s: create_thread failed: %s\n",
					__func__, strerror(errno));
	}
	for (f = 0; f < 6; f++) {
		int rc = pthread_join(t[f].thread, &status);
		if (rc)
			fprintf(stderr, "%s: pthread_join failed: %s\n",
					__func__, strerror(errno));
	}
	gettimeofday(&vfend, NULL);
	printf("\nvelocity field computed in %lu seconds, running simulation\n",
		vfend.tv_sec - vfbegin.tv_sec);
	if (*use_wstep)
		(*use_wstep)++;
}

static void check_vf_dump_file(char *filename)
{
	struct stat statbuf;

	if (!filename)
		return;

	int rc = stat(filename, &statbuf);
	if (rc == 0 && !restore_vf_data) /* file exists... */
		printf("File %s already exists, velocity field will not be dumped.\n",
				filename);
}

static void dump_velocity_field(char *filename_template, struct velocity_field *vf, int use_wstep)
{
	int fd;
	int bytes_left = (int) sizeof(*vf);
	int bytes_written = 0;
	char *filename = NULL;

	if (!filename_template || restore_vf_data)
		return;

	printf("\n"); fflush(stdout);

	if (!use_wstep) {
		filename = strdup(filename_template);
	} else {
		filename = malloc(strlen(filename_template) + 100);
		sprintf(filename, "%s-%d", filename_template, use_wstep);
	}
	fd = open(filename, O_RDWR | O_CREAT | O_EXCL, 0644);
	if (fd < 0) {
		fprintf(stderr, "Cannot create '%s': %s. Velocity field not dumped.\n",
			filename, strerror(errno));
		free(filename);
		return;
	}

	/* FIXME: this is quick and dirty, not endian clean, etc. */
	do {
		unsigned char *x = (unsigned char *) vf;
		int rc = write(fd, &x[bytes_written], bytes_left);
		if (rc < 0 && errno == EINTR)
			continue;
		if (rc < 0) {
			fprintf(stderr, "Error writing to '%s': %s\n", filename, strerror(errno));
			fprintf(stderr, "Velocity field dump failed.\n");
			break;
		}
		bytes_written += rc;
		bytes_left -= rc;
	} while (bytes_left > 0);
	close(fd);
	printf("Velocity field dumped to %s\n", filename);
	free(filename);
}

static int restore_velocity_field(char *filename, struct velocity_field *vf)
{
	int fd;
	int bytes_left = (int) sizeof(*vf);
	int bytes_read = 0;

	if (!filename || !restore_vf_data)
		return -1;

	printf("\n"); fflush(stdout);
	fd = open(filename, O_RDONLY);
	if (fd < 0) {
		fprintf(stderr, "Cannot open '%s' for reading: %s\n",
			filename, strerror(errno));
		return -1;
	}

	/* FIXME: this is quick and dirty, not endian clean, etc. */
	do {
		unsigned char *x = (unsigned char *) vf;
		int rc = read(fd, &x[bytes_read], bytes_left);
		if (rc < 0 && errno == EINTR)
			continue;
		if (rc < 0) {
			fprintf(stderr, "Error reading '%s': %s\n", filename, strerror(errno));
			fprintf(stderr, "Velocity field restoration failed.\n");
			close(fd);
			return -1;
		}
		bytes_read += rc;
		bytes_left -= rc;
	} while (bytes_left > 0);
	close(fd);
	printf("Velocity field restored from %s\n", filename);
	return 0;
}

/* move a particle according to velocity field at its current location */
static void move_particle(struct particle *p, struct velocity_field *vf)
{
	struct fij fij;

	fij = xyz_to_fij(&p->pos, vfdim);
	p->fij = xyz_to_fij(&p->pos, DIM);
	vec3_add_self(&p->pos, &vf->v[fij.f][fij.i][fij.j]);
	vec3_normalize_self(&p->pos);
	vec3_mul_self(&p->pos, (float) vfdim / 2.0f);
}

static void *move_particles_thread_fn(void *info)
{
	struct movement_thread_info *thr = info;

	for (int i = thr->first_particle; i <= thr->last_particle; i++)
		move_particle(&thr->p[i], thr->vf);
	return NULL;
}

static void move_particles(struct particle *p, struct movement_thread_info *thr,
			struct velocity_field *vf)
{
	int rc;

	thr->vf = vf;
	thr->p = p;
	rc = create_thread(&thr->thread, move_particles_thread_fn, thr, "move-particles", 0);
	if (rc)
		fprintf(stderr, "%s: create_thread failed: %s\n",
				__func__, strerror(errno));
}

struct image_thread_info {
	pthread_t thread;
	struct particle *p;
	int face;
	int nparticles;
};

static const int xo[] = { 1, 1, 0, -1, -1, -1, 0, 1 };
static const int yo[] = { 0, 1, 1, 1, 0, -1, -1, -1 };

static void *update_output_image_thread_fn(void *info)
{
	struct image_thread_info *t = info;
	struct particle *p = t->p;
	int i, j;

	if (!nofade)
		fade_out_background(t->face, &darkest_color);
	for (i = 0; i < t->nparticles; i++) {
		if (p[i].fij.f != t->face)
			continue;
		p[i].c.a = opacity;
		paint_particle(t->face, p[i].fij.i, p[i].fij.j, &p[i].c);
		if (large_pixels) {
			for (j = 0; j < 8; j++)
				paint_particle(t->face, p[i].fij.i + xo[j], p[i].fij.j + yo[j], &p[i].c);
		}
	}
	return NULL;
}

static void update_output_images(int image_threads, struct particle p[], const int nparticles)
{
	struct image_thread_info t[image_threads];
	int i, rc;
	void *status;

	for (i = 0; i < image_threads; i++) {
		t[i].face = i;
		t[i].p = p;
		t[i].nparticles = nparticles;
		rc = create_thread(&t[i].thread, update_output_image_thread_fn, &t[i], "update-image", 0);
		if (rc)
			fprintf(stderr, "%s: create_thread failed: %s\n",
					__func__, strerror(errno));
	}

	for (i = 0; i < image_threads; i++) {
		int rc = pthread_join(t[i].thread, &status);
		if (rc)
			fprintf(stderr, "%s: pthread_join failed: %s\n",
					__func__, strerror(errno));
	}
	if (opacity > opacity_limit)
		opacity = 0.95 * opacity;
}

static void save_output_images(int sequence_number)
{
	int i;
	char fname[PATH_MAX];

	for (i = 0; i < 6; i++) {
		if (sequence_number < 0)
			sprintf(fname, "%s%d.png", output_file_prefix, i);
		else
			sprintf(fname, "%s%04d-%d.png", output_file_prefix, i, sequence_number);
		if (png_utils_write_png_image(fname, output_image[i], DIM, DIM, 1, 0))
			fprintf(stderr, "Failed to write %s\n", fname);
	}
	printf("o");
	fflush(stdout);
}

static void find_darkest_pixel(unsigned char *image, int w, int h,
				struct color *darkest_color, int bpr)
{
	int x, y, n;
	float r, g, b, min;

	if (use_hot_pink) {
		*darkest_color = hot_pink;
		return;
	}

	min = 3.0f;
	for (x = 0; x < w; x++) {
		for (y = 0; y < h; y++) {
			n = y * bpr + x * 3;
			r = (float) image[n + 0] / 255.0f;
			g = (float) image[n + 1] / 255.0f;
			b = (float) image[n + 2] / 255.0f;
			if (r + g + b < min) {
				darkest_color->r = r;
				darkest_color->g = g;
				darkest_color->b = b;
				min = r + g + b;
			}
		}
	}
	if (!cloudmode)
		darkest_color->a = 0.01;
	else
		darkest_color->a = 0.5;
	if (fade_rate > 0)
		darkest_color->a = fade_rate;
}

static char *load_image(const char *filename, int *w, int *h, int *a, int *bytes_per_row)
{
	char *i;
	char msg[100];

	i = png_utils_read_png_image(filename, 0, 0, 0, w, h, a, msg, sizeof(msg));
	if (!i) {
		fprintf(stderr, "%s: cannot load image: %s\n", filename, msg);
		exit(1);
	}
	*bytes_per_row = *w * (3 + !!(*a));
	/* align to 4 byte boundary */
	if (*bytes_per_row & 0x03)
		*bytes_per_row += 4 - (*bytes_per_row & 0x03);
	find_darkest_pixel((unsigned char *) i, *w, *h, &darkest_color, *bytes_per_row);
	return i;
}

void allocate_output_images(void)
{
	int i;

	for (i = 0; i < 6; i++) {
		output_image[i] = malloc(4 * DIM * DIM);
		memset(output_image[i], 0, 4 * DIM * DIM);
	}
}

static void wait_for_movement_threads(struct movement_thread_info ti[], int nthreads)
{
	int i;
	void *status;

	for (i = 0; i < nthreads; i++) {
		int rc = pthread_join(ti[i].thread, &status);
		if (rc)
			fprintf(stderr, "%s: pthread_join failed: %s\n",
					__func__, strerror(errno));
	}
}

static void usage(void)
{
	fprintf(stderr, "usage: gaseous-giganticus [-b bands] [-i inputfile] [-o outputfile]\n");
	fprintf(stderr, "       [-w w-offset] [-h] [-n] [-v velocity factor] [-B band-vel-factor]\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "Options:\n");
	fprintf(stderr, "   -a, --pole-attenuation: attenuate band velocity near poles.  If\n");
	fprintf(stderr, "                 attenuation is zero, band velocity at poles will be\n");
	fprintf(stderr, "                 relatively high due to short distance around band.\n");
	fprintf(stderr, "                 Default is 0.5. Min is 0.0, max is 1.0.\n");
	fprintf(stderr, "   -b, --bands : Number of counter rotating bands.  Default is 6.0\n");
	fprintf(stderr, "   -d, --dump-velocity-field : dump velocity field data to specified file\n");
	fprintf(stderr, "   -D, --faderate : Rate at which particles fade, default = 0.01\n");
	fprintf(stderr, "   -c, --count : Number of iterations to run the simulation.\n");
	fprintf(stderr, "                 Default is 1000\n");
	fprintf(stderr, "   -C, --cloudmode: modulate image output by to produce clouds\n");
	fprintf(stderr, "   -f, --fbm-falloff: Use specified falloff for FBM noise.  Default is 0.5\n");
	fprintf(stderr, "   -g, --gain, 2nd and later octaves are multiplied by pow(fbm-falloff, (octave-1)*gain)\n");
	fprintf(stderr, "   -F, --vfdim: Set size of velocity field.  Default:2048. Min: 16. Max: 2048\n");
	fprintf(stderr, "   -i, --input : Input image filename.  Must be RGB or RGBA png file.\n");
	fprintf(stderr, "   -I, --image-save-period: Interval of simulation iterations after which\n");
	fprintf(stderr, "         to output images.  Default is every 20 iterations\n");
	fprintf(stderr, "   -m, --speed-multiplier:  band_speed_factor and velocity_factor are\n");
	fprintf(stderr, "         by this number.  It is a single option to affect both by a\n");
	fprintf(stderr, "         multiplier which is a bit easier than setting an absolute value\n");
	fprintf(stderr, "         for them individually.\n");
	fprintf(stderr, "   -o, --output : Output image filename template.\n");
	fprintf(stderr, "               Example: 'out-' will produces 6 output files\n");
	fprintf(stderr, "               out-0.png, out-1.png, ..., out-5.png\n");
	fprintf(stderr, "   -O, --opacity: Specify minimum opacity of particles between 0 and 1.\n");
	fprintf(stderr, "                  default is 0.2\n");
	fprintf(stderr, "   -w, --w-offset: w dimension offset in 4D open simplex noise field\n");
	fprintf(stderr, "                   Use -w to avoid (or obtain) repetitive results.\n");
	fprintf(stderr, "   -h, --hot-pink: Gradually fade pixels to hot pink.  This will allow\n");
	fprintf(stderr, "                   divergences in the velocity field to be clearly seen,\n");
	fprintf(stderr, "                   as pixels that contain no particles wil not be painted\n");
	fprintf(stderr, "                   and will become hot pink.\n");
	fprintf(stderr, "   -k, --cubemap: input 6 RGB or RGBA png cubemap images to initialize particle color\n");
	fprintf(stderr, "   -l, --large-pixels: particles are 3x3 pixels instead of 1 pixel.  Allows\n");
	fprintf(stderr, "                       for fewer particles and thus faster rendering, but this\n");
	fprintf(stderr, "                       WILL leave visible artifacts at cubemap face boundaries.\n");
	fprintf(stderr, "   -L, --noise-levels: Number of fractal noise levels to consider.  Default 4, max %d\n",
			max_noise_levels);
	fprintf(stderr, "   -p, --particles: Use specified number of particles. Default is 8000000.\n");
	fprintf(stderr, "   -P, --plainmap  Do not use sinusoidal image mapping, instead repeat image\n");
	fprintf(stderr, "                   on six sides of a cubemap.\n");
	fprintf(stderr, "   -n, --no-fade:  Do not fade the image at all, divergences will be hidden\n");
	fprintf(stderr, "   -N, --sequence-number:  Do not overwrite output images, instead embed a\n");
	fprintf(stderr, "                           sequence number in the filenames.\n");
	fprintf(stderr, "   -v, --velocity-factor: Multiply velocity field by this number when\n");
	fprintf(stderr, "                   moving particles.  Default is 1200.0\n");
	fprintf(stderr, "   -V, --vertical-bands:  Make bands rotate around Y axis instead of X\n");
	fprintf(stderr, "                          Also affects --stripe option.\n");
	fprintf(stderr, "   -B, --band-vel-factor: Multiply band velocity by this number when\n");
	fprintf(stderr, "                   computing velocity field.  Default is 2.9\n");
	fprintf(stderr, "   -s, --stripe: Begin with stripes from a vertical strip of input image\n");
	fprintf(stderr, "   -S, --sinusoidal: Use sinusoidal projection for input image\n");
	fprintf(stderr, "                 Note: sinusoidal is the default projection.\n");
	fprintf(stderr, "                 Note: --stripe and --sinusoidal are mutually exclusive\n");
	fprintf(stderr, "   -t, --threads: Use the specified number of CPU threads up to the\n");
	fprintf(stderr, "                   number of online CPUs\n");
	fprintf(stderr, "   -W, --wstep: w coordinate of noise field is incremented by specified\n");
	fprintf(stderr, "                amount periodically and velocity field is recalculated\n");
	fprintf(stderr, "   -x, --vortices: how many artificial circular vortices to add into the v-field\n");
	fprintf(stderr, "   --vortex-band-threshold: controls distribution of vortices (see man page)\n");
	fprintf(stderr, "   --vortex-size: radius of vortices as a fraction of planet radius. Default 0.04\n");
	fprintf(stderr, "   --vortex-size-variance: Range of vortex sizes, default is plus or minus 0.02\n");
	fprintf(stderr, "   -z, --noise-scale: default is %f\n", default_noise_scale);
	fprintf(stderr, "\n");
	fprintf(stderr, "Example:\n");
        fprintf(stderr, "\n");
	fprintf(stderr, "   ./gaseous-giganticus -V --sinusoidal --noise-scale 2.5 "
			"--velocity-factor 1300 \\\n"
			"        -i image.png -o p13 --bands 10\n");
	fprintf(stderr, "\n");
	exit(1);
}

#define VORTEX_SIZE_OPTION 1000
#define VORTEX_SIZE_VARIANCE_OPTION 1001
#define VORTEX_BAND_THRESHOLD_OPTION 1002

static struct option long_options[] = {
	{ "pole-attenuation", required_argument, NULL, 'a' },
	{ "bands", required_argument, NULL, 'b' },
	{ "count", required_argument, NULL, 'c' },
	{ "cloudmode", required_argument, NULL, 'C' },
	{ "dump-velocity-field", required_argument, NULL, 'd' },
	{ "faderate", required_argument, NULL, 'D' },
	{ "input", required_argument, NULL, 'i' },
	{ "image-save-period", required_argument, NULL, 'I' },
	{ "cubemap", required_argument, NULL, 'k' },
	{ "large-pixels", no_argument, NULL, 'l' },
	{ "noise-levels", no_argument, NULL, 'L' },
	{ "output", required_argument, NULL, 'o' },
	{ "opacity", required_argument, NULL, 'O' },
	{ "w-offset", required_argument, NULL, 'w' },
	{ "fbm-falloff", required_argument, NULL, 'f' },
	{ "gain", required_argument, NULL, 'g' },
	{ "hot-pink", no_argument, NULL, 'h' },
	{ "help", no_argument, NULL, 'H' },
	{ "no-fade", no_argument, NULL, 'n' },
	{ "sequence-number", no_argument, NULL, 'N' },
	{ "velocity-factor", required_argument, NULL, 'v' },
	{ "vertical-bands", required_argument, NULL, 'V' },
	{ "band-vel-factor", required_argument, NULL, 'B' },
	{ "restore-velocity-field", required_argument, NULL, 'r' },
	{ "random", no_argument, NULL, 'R' },
	{ "stripe", no_argument, NULL, 's' },
	{ "sinusoidal", no_argument, NULL, 'S' },
	{ "threads", required_argument, NULL, 't' },
	{ "particles", required_argument, NULL, 'p' },
	{ "plainmap", required_argument, NULL, 'P' },
	{ "speed-multiplier", required_argument, NULL, 'm' },
	{ "vfdim", required_argument, NULL, 'F' },
	{ "wstep", required_argument, NULL, 'W' },
	{ "wstep-period", required_argument, NULL, 'q' },
	{ "vortices", required_argument, NULL, 'x' },
	{ "vortex-band-threshold", required_argument, NULL, VORTEX_BAND_THRESHOLD_OPTION },
	{ "vortex-size", required_argument, NULL, VORTEX_SIZE_OPTION },
	{ "vortex-size-variance", required_argument, NULL, VORTEX_SIZE_VARIANCE_OPTION },
	{ "noise-scale", required_argument, NULL, 'z' },
	{ 0, 0, 0, 0 },
};

static void process_float_option(char *option_name, char *option_value, float *value)
{
	float tmpf;

	if (sscanf(option_value, "%f", &tmpf) == 1) {
		*value = tmpf;
	} else {
		fprintf(stderr, "Bad %s option '%s'\n", option_name, option_value);
		usage();
	}
}

static void process_int_option(char *option_name, char *option_value, int *value)
{
	int tmp;

	if (sscanf(option_value, "%d", &tmp) == 1) {
		*value = tmp;
	} else {
		fprintf(stderr, "Bad %s option '%s'\n", option_name, option_value);
		usage();
	}
}

static void set_automatic_options(int random_mode)
{
	struct timeval tv;

	if (!random_mode)
		return;

	/* set Sinusoidal mode */
	sinusoidal = 1;
	stripe = 0;
	/* set horizontal bands */
	vertical_bands = 0;
	gettimeofday(&tv, NULL);
	srand(tv.tv_usec);
	noise_scale = ((float) rand() / (float) RAND_MAX) * 3.0 + 1.0;
	velocity_factor = ((float) rand() / (float) RAND_MAX) * 1500.0 + 500.0;
	band_speed_factor = ((float) rand() / (float) RAND_MAX) * 3.0;
	num_bands = ((float) rand() / (float) RAND_MAX) * 10.0 + 5.0;
	w_offset = ((float) rand() / (float) RAND_MAX) * 300.0;

	printf("\n");
	printf("Auto parameters:\n");
	printf("  --noise-scale %f\n", noise_scale);
	printf("  --velocity-factor %f\n", velocity_factor);
	printf("  --band-vel-factor %f\n", band_speed_factor);
	printf("  --num_bands %f\n", num_bands);
	printf("  --w-offset %f\n", w_offset);
	printf("\n");
}

static void process_options(int argc, char *argv[])
{
	int i, c;

	output_file_prefix = default_output_file_prefix;
	input_file = default_input_file;

	while (1) {
		int option_index;
		c = getopt_long(argc, argv, "a:B:b:c:Cd:D:f:g:F:hHi:k:I:lL:nNm:o:O:p:PRr:sSt:Vv:w:W:x:z:",
				long_options, &option_index);
		if (c == -1)
			break;
		switch (c) {
		case 'a':
			process_float_option("pole-attenuation", optarg, &pole_attenuation);
			if (pole_attenuation < 0.0f)
				pole_attenuation = 0.0f;
			if (pole_attenuation > 1.0f)
				pole_attenuation = 1.0f;
			break;
		case 'B':
			process_float_option("band-vel-factor", optarg, &band_speed_factor);
			break;
		case 'b':
			process_float_option("num-bands", optarg, &num_bands);
			break;
		case 'c':
			process_int_option("count", optarg, &niterations);
			break;
		case 'C':
			cloudmode = 1;
			break;
		case 'd':
			vf_dump_file = optarg;
			restore_vf_data = 0;
			break;
		case 'D':
			process_float_option("faderate", optarg, &fade_rate);
			break;
		case 'r':
			vf_dump_file = optarg;
			restore_vf_data = 1;
			break;
		case 'R':
			random_mode = 1;
			break;
		case 'f':
			process_float_option("fbm-falloff", optarg, &fbm_falloff);
			ff[0] = 1.0;
			ff[1] = fbm_falloff;
			ff[2] = ff[1] * fbm_falloff;
			ff[3] = ff[2] * fbm_falloff;
			ff[4] = ff[3] * fbm_falloff;
			ff[5] = ff[4] * fbm_falloff;
			ff[6] = ff[5] * fbm_falloff;
			break;
		case 'g':
			process_float_option("gain", optarg, &gain);
			ff[0] = 1.0;
			ff[1] = fbm_falloff; /* same as pow(fbm_fallof, 0.0 * gain); */
			ff[2] = pow(fbm_falloff, 1.0 * gain);
			ff[3] = pow(fbm_falloff, 2.0 * gain);
			ff[4] = pow(fbm_falloff, 3.0 * gain);
			ff[5] = pow(fbm_falloff, 4.0 * gain);
			ff[6] = pow(fbm_falloff, 5.0 * gain);
			break;
		case 'F':
			process_int_option("vfdim", optarg, &vfdim);
			if (vfdim < 16) {
				vfdim = 16;
				fprintf(stderr, "Bad value of vfdim specified, using 16.\n");
			}
			if (vfdim > 2048) {
				vfdim = 2048;
				fprintf(stderr, "Bad value of vfdim specified, using 2048.\n");
			}
			break;
		case 'h':
			use_hot_pink = 1;
			break;
		case 'H':
			usage(); /* does not return */
			break;
		case 'i':
			input_file = optarg;
			break;
		case 'I':
			process_int_option("image-save-period", optarg, &image_save_period);
			break;
		case 'k':
			cubemap_prefix = optarg;
			break;
		case 'm':
			process_float_option("speed-multiplier", optarg, &speed_multiplier);
			break;
		case 'n':
			nofade = 1;
			break;
		case 'N':
			save_texture_sequence = 1;
			break;
		case 'l':
			large_pixels = 1;
			break;
		case 'L':
			process_int_option("noise-levels", optarg, &noise_levels);
			if (noise_levels < 1) {
				fprintf(stderr, "Noise levels value of %d is out of range, using 1\n", noise_levels);
				noise_levels = 1;
			} else if (noise_levels > max_noise_levels) {
				fprintf(stderr, "Noise levels value of %d is out of range, using %d\n",
					noise_levels, max_noise_levels);
				noise_levels = max_noise_levels;
			}
			break;
		case 'o':
			output_file_prefix = optarg;
			break;
		case 'O':
			process_float_option("opacity", optarg, &opacity_limit);
			if (opacity_limit < 0.0)
				opacity_limit = 0.0;
			if (opacity_limit > 1.0)
				opacity_limit = 1.0;
			break;
		case 'p':
			process_int_option("particles", optarg, &particle_count);
			break;
		case 'P': /* plain mapping of image to 6 sides of cubemap */
			stripe = 0;
			sinusoidal = 0;
			break;
		case 's':
			stripe = 1;
			sinusoidal = 0;
			break;
		case 'S':
			sinusoidal = 1;
			stripe = 0;
			break;
		case 't':
			process_int_option("threads", optarg, &user_threads);
			break;
		case 'w':
			process_float_option("w-offset", optarg, &w_offset);
			break;
		case 'v':
			process_float_option("velocity-factor", optarg, &velocity_factor);
			break;
		case 'V':
			vertical_bands = 0;
			break;
		case 'W':
			process_float_option("wstep", optarg, &wstep);
			use_wstep = 1;
			break;
		case 'q': /* running out of letters, so, 'q'. */
			process_int_option("wstep-period", optarg, &wstep_period);
			break;
		case 'x':
			process_int_option("vortices", optarg, &nvortices);
			if (nvortices > MAXVORTICES) {
				nvortices = MAXVORTICES;
				fprintf(stderr, "Requested %d vortices, maximum is %d\n", nvortices, MAXVORTICES);
			}
			break;
		case 'z':
			process_float_option("noise-scale", optarg, &noise_scale);
			break;
		case VORTEX_SIZE_OPTION:
			process_float_option("vortex-size", optarg, &vortex_size);
			break;
		case VORTEX_SIZE_VARIANCE_OPTION:
			process_float_option("vortex-size", optarg, &vortex_size_variance);
			break;
		case VORTEX_BAND_THRESHOLD_OPTION:
			process_float_option("vortex-band-threshold", optarg, &vortex_band_threshold);
			if (vortex_band_threshold < 0.05)
				vortex_band_threshold = 0.05;
			if (vortex_band_threshold > 1.0)
				vortex_band_threshold = 1.0;
			break;
		default:
			fprintf(stderr, "unknown option '%s'\n",
				option_index > 0 && option_index - 1 < argc &&
				argv[option_index - 1] ? argv[option_index - 1] : "(none)");
			usage();
		}
	}

	/* Scale so that vfdim doesn't change the effect of these */
	band_speed_factor = ((float) vfdim / 2048.0) * band_speed_factor * speed_multiplier;
	velocity_factor = ((float) vfdim / 2048.0) * velocity_factor * speed_multiplier;

	for (i = noise_levels; i < max_noise_levels; i++)
		ff[i] = 0.0;

	return;
}

static void load_cubemap_images(const char *prefix)
{
	char filename[PATH_MAX];
	int i;

	for (i = 0; i < 6; i++) {
		sprintf(filename, "%s%d.png", prefix, i);
		cubemap_image[i] = load_image(filename, &start_image_width, &start_image_height,
						&start_image_has_alpha, &start_image_bytes_per_row);
	}
}

/* Return a value between -1.0 and 1.0 with a bias towards zero.
 * Not a normal distribution, but sorta close enough for our purposes.
 */
static float random_squared(void)
{
	float x, y;

	x = (2.0f * (float) rand() / (float) RAND_MAX) - 1.0f;
	y = (2.0f * (float) rand() / (float) RAND_MAX) - 1.0f;
	return x * y;
}

static void create_vortex(int i)
{
	const union vec3 right_at_ya = { { 0.0f, 0.0f, 1.0f } };
	const union vec3 up = { { 0.0f, 1.0f, 0.0f } };
	float angle, band_speed;


	if (num_bands > 0) {
		/* I don't think this is actually correct.  I think
		 * I need to sample the velocity field around the vortex
		 * to determine the already present rotation and make the
		 * vortex rotate in the same direction... this doesn't
		 * quite do that.  And the velocity field is not built yet
		 * when this gets called (that might be change-able though.).
		 *
		 * A better method might be to pre-sample the noise field, then
		 * disturb the sampled noise gradient to make the vortices prior to
		 * constructing the velocity field.
		 */
		do {
			random_point_on_sphere(1.0, &vort[i].p.v.x, &vort[i].p.v.y, &vort[i].p.v.z);
			vort[i].r = vortex_size + random_squared() * vortex_size_variance;
			if (vertical_bands)
				angle = asinf(vort[i].p.v.z);
			else
				angle = asinf(vort[i].p.v.y);
			band_speed = calculate_band_speed(angle);
		} while (fabs(band_speed) > vortex_band_threshold * band_speed_factor &&
			fabs(angle) > 15.0 * M_PI / 180.0); /* exclude vortice within 15 deg of poles */
		if (calculate_band_speed(angle + 0.05) < calculate_band_speed(angle - 0.05))
			vort[i].angular_vel = 2.5;
		else
			vort[i].angular_vel = -2.5;
	} else {
		random_point_on_sphere(1.0, &vort[i].p.v.x, &vort[i].p.v.y, &vort[i].p.v.z);
		vort[i].r = vortex_size + random_squared() * vortex_size_variance;
		/* This will be right half the time :/ */
		if (rand() > (RAND_MAX >> 1))
			vort[i].angular_vel = 2.5;
		else
			vort[i].angular_vel = -2.5;
	}
	quat_from_u2v(&vort[i].q, &vort[i].p, &right_at_ya, &up);
	quat_from_u2v(&vort[i].reverseq, &right_at_ya, &vort[i].p, &up);
}

static void create_vortices()
{
	int i;

	for (i = 0; i < nvortices; i++)
		create_vortex(i);
}

int main(int argc, char *argv[])
{
	int i, t;
	struct movement_thread_info *ti;
	int last_imaged_iteration = -1;
	int sequence_number = -1;
	particle_count = NPARTICLES;
	struct timeval movebegin, moveend, move_elapsed;
	struct timeval imagebegin, imageend, image_elapsed;

	move_elapsed.tv_sec = 0;
	move_elapsed.tv_usec = 0;
	image_elapsed.tv_sec = 0;
	image_elapsed.tv_usec = 0;

	open_simplex_noise(3141592, &ctx);

	setlocale(LC_ALL, "");
	noise_scale = default_noise_scale;

	process_options(argc, argv);
	create_vortices();
	set_automatic_options(random_mode);

	check_vf_dump_file(vf_dump_file);

	int num_online_cpus = sysconf(_SC_NPROCESSORS_ONLN);
	if (num_online_cpus > 0)
		nthreads = num_online_cpus;
	if (user_threads > 0 && user_threads < num_online_cpus)
		nthreads = user_threads;
	printf("Using %d threads for particle motion\n", nthreads);

	int tparticles = particle_count / nthreads;

	ti = malloc(sizeof(*ti) * nthreads);

	for (t = 0; t < nthreads; t++) {
		ti[t].first_particle = t * tparticles;
		ti[t].last_particle = (t + 1) * tparticles - 1;
	}
	/* last thread picks up extra (particle_count - (nthreads * tparticles)) */
	t = nthreads - 1;
	ti[t].last_particle = particle_count - 1;

	printf("Allocating output image space\n");
	allocate_output_images();
	start_image_has_alpha = 0;
	if (cubemap_prefix) {
		printf("Loading cubemap images\n");
		load_cubemap_images(cubemap_prefix);
	} else {
		printf("Loading image\n");
		start_image = load_image(input_file, &start_image_width, &start_image_height,
						&start_image_has_alpha, &start_image_bytes_per_row);
	}
#if 0	
	write_png_image("blah.png", (unsigned char *) start_image, start_image_width,
			start_image_height, start_image_has_alpha);
#endif
	printf("width, height, bytes per row = %d,%d,%d\n",
			start_image_width, start_image_height, start_image_bytes_per_row);
	init_particles(&particle, particle_count);
	if (restore_velocity_field(vf_dump_file, &vf))
		update_velocity_field(&vf, noise_scale, w_offset, &use_wstep);
	dump_velocity_field(vf_dump_file, &vf, use_wstep);

	for (i = 0; i < niterations; i++) {
		if ((i % 50) == 0)
			printf(" m:%lus i:%lus\n%5d / %5d ",
				move_elapsed.tv_sec, image_elapsed.tv_sec, i, niterations);
		else
			printf(".");
		fflush(stdout);

		gettimeofday(&movebegin, NULL);
		for (t = 0; t < nthreads; t++)
			move_particles(particle, &ti[t], &vf);
		wait_for_movement_threads(ti, nthreads);
		gettimeofday(&moveend, NULL);
		move_elapsed.tv_sec += moveend.tv_sec - movebegin.tv_sec;
		imagebegin = moveend;
		update_output_images(image_threads, particle, particle_count);
		gettimeofday(&imageend, NULL);
		image_elapsed.tv_sec += imageend.tv_sec - imagebegin.tv_sec;
		if ((i % image_save_period) == 0) {
			sequence_number += save_texture_sequence;
			save_output_images(sequence_number);
			last_imaged_iteration = i;
		}
		if (use_wstep && (i % wstep_period == 0)) {
			w_offset += wstep;
			update_velocity_field(&vf, noise_scale, w_offset, &use_wstep);
			dump_velocity_field(vf_dump_file, &vf, use_wstep);
		}
	}
	if (last_imaged_iteration != i - 1) {
		sequence_number += save_texture_sequence;
		save_output_images(sequence_number);
	}
	printf("\n%5d / %5d -- done.\n", i, niterations);
	open_simplex_noise_free(ctx);
	return 0;
}

