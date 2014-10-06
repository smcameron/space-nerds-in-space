#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <png.h>
#include <errno.h>
#include <limits.h>
#include <pthread.h>
#include <getopt.h>
#include <locale.h>

#include "mtwist.h"
#include "mathutils.h"
#include "quat.h"
#include "open-simplex-noise.h"

static struct osn_context *ctx;

#define MAXBUMPS 100000
#define MAXCRATERS 1000
#define RADII (3.0f)
static float sealevel = 0.08;

static char *landfile = "land.png";
static char *waterfile = "water.png";
static char *heightfile = "heightdata.png";
static int nbumps = 3;
static int ncraters = 0;
static float scatterfactor = 1.8f;
static float rlimit = 0.005;
static int random_seed = 31415;
static float shrink_factor = 0.55;
static int initial_bumps = 60;

static struct bump {
	union vec3 p;
	float r, h;
	union quat texelq;
	int tx, ty; /* origin of texel region in sample data */
	int tr; /* radius of texel region in sample data */
	float ts; /* scaling factor to get from 3d dist to texel dist */
} bumplist[MAXBUMPS], craterlist[MAXCRATERS];
static int totalbumps = 0;

#define DIM 1024
static unsigned char *output_image[6];
static unsigned char *normal_image[6];
static unsigned char *land, *water;
static int landw, landh, landa, landbpr;
static int waterw, waterh, watera, waterbpr; 
static union vec3 vertex[6][DIM][DIM];
static union vec3 normal[6][DIM][DIM];
static float noise[6][DIM][DIM];
static char *output_file_prefix = "heightmap";
static char *normal_file_prefix = "normalmap";
static char *sampledata;
static int samplew, sampleh, samplea, sample_bytes_per_row;
static float minn, maxn; /* min and max noise values encountered */
static float crater_min_height, crater_max_height;

static inline float fbmnoise4(float x, float y, float z)
{
	const float fbm_falloff = 0.5;
	const float f1 = fbm_falloff;
	const float f2 = fbm_falloff * fbm_falloff;
	const float f3 = fbm_falloff * fbm_falloff * fbm_falloff;

	return 1.0 * open_simplex_noise4(ctx, x, y, z, 1.0) +
		f1 * open_simplex_noise4(ctx, 2.0f * x, 2.0f * y, 2.0f * z, 1.0) +
		f2 * open_simplex_noise4(ctx, 4.0f * x, 4.0f * y, 4.0f * z, 1.0) +
		f3 * open_simplex_noise4(ctx, 8.0f * x, 8.0f * y, 8.0f * z, 1.0);
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

static void initialize_vertices(void)
{
	int f, i, j;

	for (f = 0; f < 6; f++)
		for (i = 0; i < DIM; i++)
			for (j = 0; j < DIM; j++) {
				const union vec3 v = fij_to_xyz(f, i, j, DIM);
				vertex[f][i][j] = v;
				noise[f][i][j] = fbmnoise4(v.v.x, v.v.y, v.v.z);
				if (f == 0 && i == 0 && j == 0) {
					minn = noise[0][0][0];
					maxn = noise[0][0][0];
				} else {
					if (noise[f][i][j] < minn)
						minn = noise[f][i][j];
					if (noise[f][i][j] > maxn)
						maxn = noise[f][i][j];
				}
			}
	printf("maxn = %f, minn = %f\n", maxn, minn);
}

static inline void distort_vertex(union vec3 *v, float d, struct bump *b)
{
	union vec3 distortion;
	union vec3 texelv;
	const float r = b->r;
	const float h = b->h;
	float m;
	int x, y, p;
	unsigned char *c;

	float nr = 0.5 * (cos(M_PI * d / r) + 1.0f) * h;

	vec3_normalize(&distortion, v);
	quat_rot_vec(&texelv, &distortion, &b->texelq);
	vec3_mul_self(&texelv, b->ts);
	x = (int) texelv.v.x + b->tx;
	y = (int) texelv.v.y + b->ty;
	if (x < 0 || x > samplew || y < 0 || y > sampleh)
		printf("out of range (%d, %d)\n", x, y);
	p = y * sample_bytes_per_row + x * 3;
	c = (unsigned char *) &sampledata[p]; 
	m = (float) *c / 255.0f;
	vec3_mul_self(&distortion, nr * m);
	vec3_add_self(v, &distortion);
}

static inline void crater_vertex(union vec3 *v, float d, struct bump *b)
{
	union vec3 distortion;
	const float r = b->r;
	const float r1 = b->r * 0.80;
	const float r2 = b->r * 0.75;

	vec3_normalize(&distortion, v);
	if (d <= r2) { /* Central part of crater, just flatten it. */
		vec3_mul(v, &distortion, crater_min_height);
		return;
	}
	if (d <= r1) { /* Inner crater wall... */
		const float p = (d - r2) / (r1 - r2);
		const float a = p * M_PI;
		const float h = (0.5f - 0.5f * cosf(a)) * b->h;
		const float oh = vec3_magnitude(v);
		float m;
		//const float m = p * (vec3_magnitude(v) - 1.0f) + h;
		//printf("h = %f\n", 1.0f + h);
		//if (1.0f + h < oh)
		if (crater_min_height + h < oh)
			m = oh;
		else
			m = crater_min_height + h;
		vec3_mul(v, &distortion, m);
		return;
	}
	if (d <= r) {
		/* Outer crater wall */
		const float p = (d - r1) / (r - r1);
		const float a = p * M_PI;
		const float h = (0.5f + 0.5 * cosf(a)) * b->h;
		//const float m = p * (vec3_magnitude(v) - 1.0f) + h;
		const float oh = vec3_magnitude(v);
		float m;
		//printf("oh = %f, h = %f\n", oh, h);
		//m = h;
		m = h;
		if (crater_min_height + h < oh)
			m = oh;
		else
			m = crater_min_height + h;
		vec3_mul(v, &distortion, m);
		return;
	}
}

struct thread_info {
	pthread_t thread;
	int f;
};

typedef void *(*render_on_face_fn)(void *info);

static void *render_bumps_on_face_fn(void *info)
{
	int f, i, j, k;
	union vec3 p, *p2;
	float d2, d;
	struct bump *b;
	struct thread_info *t = info;

	f = t->f; 

	for (i = 0; i < DIM; i++) {
		if (i % (DIM / 8) == 0) {
			printf("%d", f);
			fflush(stdout);
		}
		for (j = 0; j < DIM; j++) {
			p = fij_to_xyz(f, i, j, DIM);
			for (k = 0; k < totalbumps; k++) {
				b = &bumplist[k];
				p2 = &b->p;
				float dx = fabs(p.v.x - p2->v.x);
				if (dx > b->r)
					continue;
				float dy = fabs(p.v.y - p2->v.y);
				if (dy > b->r)
					continue;
				float dz = fabs(p.v.z - p2->v.z);
				if (dz > b->r)
					continue;
				d2 = dx * dx + dy * dy + dz * dz;
				if (d2 > b->r * b->r)
					continue;
				d = sqrtf(d2);
				distort_vertex(&vertex[f][i][j], d, b);
			}
		}
	}
	return NULL;
}

static void *render_craters_on_face_fn(void *info)
{
	int f, i, j, k;
	union vec3 p, *p2;
	float d2, d;
	struct bump *b;
	struct thread_info *t = info;

	f = t->f;

	for (i = 0; i < DIM; i++) {
		if (i % (DIM / 8) == 0) {
			printf("%d", f);
			fflush(stdout);
		}
		for (j = 0; j < DIM; j++) {
			p = fij_to_xyz(f, i, j, DIM);
			for (k = 0; k < ncraters; k++) {
				b = &craterlist[k];
				p2 = &b->p;
				float dx = fabs(p.v.x - p2->v.x);
				if (dx > b->r)
					continue;
				float dy = fabs(p.v.y - p2->v.y);
				if (dy > b->r)
					continue;
				float dz = fabs(p.v.z - p2->v.z);
				if (dz > b->r)
					continue;
				d2 = dx * dx + dy * dy + dz * dz;
				if (d2 > b->r * b->r)
					continue;
				d = sqrtf(d2);
				crater_vertex(&vertex[f][i][j], d, b);
			}
		}
	}
	return NULL;
}

static void multithread_face_render(render_on_face_fn render)
{
	int rc, f;
	void *status;
	struct thread_info t[6];

	for (f = 0; f < 6; f++) {
		t[f].f = f;
		rc = pthread_create(&t[f].thread, NULL, render, &t[f]);
		if (rc)
			fprintf(stderr, "%s: pthread_create failed: %s\n",
					__func__, strerror(errno));
	}
	for (f = 0; f < 6; f++) {
		int rc = pthread_join(t[f].thread, &status);
		if (rc)
			fprintf(stderr, "%s: pthread_join failed: %s\n",
				__func__, strerror(errno));
	}
}

static void render_all_bumps(void)
{
	if (totalbumps > 0)
		multithread_face_render(render_bumps_on_face_fn);
}

static void render_all_craters(float min, float max)
{
	crater_min_height = min;
	crater_max_height = max;
	if (ncraters > 0)
		multithread_face_render(render_craters_on_face_fn);
}

static void add_bump(union vec3 p, float r, float h)
{
	struct bump *b;
	const union vec3 right_at_ya = { { 0.0f, 0.0f, 1.0f } };
	const union vec3 up = { { 0.0f, 1.0f, 0.0f } };

	if (totalbumps >= MAXBUMPS)
		return;

	b = &bumplist[totalbumps];
	b->p = p;
	b->r = r;
	b->h = h;
	b->tx = (int) ((float) samplew / RADII + 0.5 * snis_random_float() *
			(RADII - 2.0f) / RADII * (float) samplew);
	b->ty = (int) ((float) sampleh / RADII + 0.5 * snis_random_float() *
			(RADII - 2.0f) / RADII * (float) sampleh);
	if (samplew < sampleh)
		b->ts = samplew / RADII;
	else
		b->ts = sampleh / RADII;
	quat_from_u2v(&b->texelq, &p, &right_at_ya, &up);
	totalbumps++;
}

static void add_crater(int i, union vec3 p, float r, float h)
{
	struct bump *b;
	const union vec3 right_at_ya = { { 0.0f, 0.0f, 1.0f } };
	const union vec3 up = { { 0.0f, 1.0f, 0.0f } };

	b = &craterlist[i];
	b->p = p;
	b->r = r;
	b->h = h;
	b->tx = (int) ((float) samplew / RADII + 0.5 * snis_random_float() *
			(RADII - 2.0f) / RADII * (float) samplew);
	b->ty = (int) ((float) sampleh / RADII + 0.5 * snis_random_float() *
			(RADII - 2.0f) / RADII * (float) sampleh);
	if (samplew < sampleh)
		b->ts = samplew / RADII;
	else
		b->ts = sampleh / RADII;
	quat_from_u2v(&b->texelq, &p, &right_at_ya, &up);
}

static void recursive_add_bump(union vec3 pos, float r, float h,
				float shrink, float rlimit)
{
	float hoffset;

	add_bump(pos, r, h);
	if (r * shrink < rlimit)
		return;
	for (int i = 0; i < nbumps; i++) {
		union vec3 d;
		d.v.x = snis_random_float() * r * scatterfactor;
		d.v.y = snis_random_float() * r * scatterfactor;
		d.v.z = snis_random_float() * r * scatterfactor;
		vec3_add_self(&d, &pos);
		vec3_normalize_self(&d);
		hoffset = snis_random_float() * h * shrink * 0.5;
		recursive_add_bump(d, r * shrink, h * (shrink + 0.5 * (1.0 - shrink)) + hoffset, shrink, rlimit);
	}
}

static void add_bumps(const int nbumps)
{
	int i;

	for (i = 0; i < nbumps; i++) {
		union vec3 p;
		float r = 0.5 * (snis_random_float() + 1.0f) * 0.4;

		random_point_on_sphere(1.0, &p.v.x, &p.v.y, &p.v.z);
		recursive_add_bump(p, r, 0.10, shrink_factor, rlimit);
		printf(".");
		fflush(stdout);
	}
}

static void add_craters(const int nc)
{
	int i;

	for (i = 0; i < nc; i++) {
		float r = 0.5 * (snis_random_float() + 1.0f);
		float r2 = 0.5 * (snis_random_float() + 1.0f);
		r = r * r2 * 0.1;
		union vec3 p;
		random_point_on_sphere(1.0, &p.v.x, &p.v.y, &p.v.z);
		add_crater(i, p, r, 0.005f);
	}
}

static void find_min_max_height(float *min, float *max)
{
	int f, i, j;
	float mmin, mmax;
	float h;

	mmin = 1000000.0f;
	mmax = 0.0f;

	for (f = 0; f < 6; f++) {
		for (i = 0; i < DIM; i++) {
			for (j = 0; j < DIM; j++) {
				h = vec3_magnitude(&vertex[f][i][j]);
				if (h < mmin)
					mmin = h;
				if (h > mmax)
					mmax = h;
			}
		}
	}
	*min = mmin;
	*max = mmax;
}

void allocate_output_images(void)
{
	int i;

	for (i = 0; i < 6; i++) {
		output_image[i] = malloc(4 * DIM * DIM);
		memset(output_image[i], 0, 4 * DIM * DIM);
	}
	for (i = 0; i < 6; i++) {
		normal_image[i] = malloc(4 * DIM * DIM);
		memset(normal_image[i], 0, 4 * DIM * DIM);
	}
}

static void paint_height_maps(float min, float max)
{
	int f, i, j;
	float r; 
	unsigned char c;
	int p;

	for (f = 0; f < 6; f++) {
		for (i = 0; i < DIM; i++) {
			for (j = 0; j < DIM; j++) {
				p = (j * DIM + i) * 4; 
				r = vec3_magnitude(&vertex[f][i][j]);
				r = (r - min) / (max - min);
				c = (unsigned char) (r * 255.0f);
				output_image[f][p + 0] = c;
				output_image[f][p + 1] = c;
				output_image[f][p + 2] = c;
				output_image[f][p + 3] = 255;
			}
		}
	}
}

static void color_output(int f, int p, float r, float n)
{
	float y;
	int colorindex;


	if (r > sealevel) {
		y = (r - sealevel) / (1.0 - sealevel);
		colorindex = (int) (y * landh);
		colorindex = colorindex * landbpr;
		colorindex = colorindex + 3 * (int) (n * landw);
		output_image[f][p + 0] = land[colorindex + 0];
		output_image[f][p + 1] = land[colorindex + 1];
		output_image[f][p + 2] = land[colorindex + 2];
		return;
	}
	y = r / sealevel;
	colorindex = (int) (y * waterh);
	colorindex = colorindex * waterbpr;
	colorindex = colorindex + 3 * (int) (n * waterw);
	output_image[f][p + 0] = water[colorindex + 0];
	output_image[f][p + 1] = water[colorindex + 1];
	output_image[f][p + 2] = water[colorindex + 2];
	return;
}

static void paint_terrain_colors(float min, float max)
{
	int f, i, j;
	float r, n; 
	int p;

	for (f = 0; f < 6; f++) {
		for (i = 0; i < DIM; i++) {
			for (j = 0; j < DIM; j++) {
				p = (j * DIM + i) * 4; 
				r = vec3_magnitude(&vertex[f][i][j]);
				r = (r - min) / (max - min);
				n = (noise[f][i][j] - minn) / (maxn - minn);
				color_output(f, p, r, n);
			}
		}
	}
}


static void paint_normal_maps(float min, float max)
{
	int f, i, j;
	float rad; 
	char red, green, blue;
	int p;

	for (f = 0; f < 6; f++) {
		for (i = 0; i < DIM; i++) {
			for (j = 0; j < DIM; j++) {
				p = (j * DIM + i) * 4; 
				rad = vec3_magnitude(&vertex[f][i][j]);
				rad = (rad - min) / (max - min);
				if (rad > sealevel) {
					red = normal[f][i][j].v.x * 255;
					green = normal[f][i][j].v.y * 255;
					blue = normal[f][i][j].v.z * 255;
					normal_image[f][p + 0] = red;
					normal_image[f][p + 1] = green;
					normal_image[f][p + 2] = blue;
				} else {
					normal_image[f][p + 0] = 127;
					normal_image[f][p + 1] = 127;
					normal_image[f][p + 2] = 255;
				}
				normal_image[f][p + 3] = 255;
			}
		}
	}
}


static int write_png_image(const char *filename, unsigned char *pixels, int w, int h, int has_alpha)
{
	png_structp png_ptr;
	png_infop info_ptr;
	png_byte **row;
	int x, y, rc, colordepth = 8;
	int bytes_per_pixel = has_alpha ? 4 : 3;
	FILE *f;

	f = fopen(filename, "w");
	if (!f) {
		fprintf(stderr, "fopen: %s:%s\n", filename, strerror(errno));
		return -1;
	}
	png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
	if (!png_ptr)
		goto cleanup1;
	info_ptr = png_create_info_struct(png_ptr);
	if (!info_ptr)
		goto cleanup2;
	if (setjmp(png_jmpbuf(png_ptr))) /* oh libpng, you're old as dirt, aren't you. */
		goto cleanup2;

	png_set_IHDR(png_ptr, info_ptr, (size_t) w, (size_t) h, colordepth,
			has_alpha ? PNG_COLOR_TYPE_RGBA : PNG_COLOR_TYPE_RGB,
			PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT,
			PNG_FILTER_TYPE_DEFAULT);

	row = png_malloc(png_ptr, h * sizeof(*row));
	for (y = 0; y < h; y++) {
		row[y] = png_malloc(png_ptr, w * bytes_per_pixel);
		for (x = 0; x < w; x++) {
			unsigned char *r = (unsigned char *) row[y];
			unsigned char *src = (unsigned char *)
				&pixels[y * w * bytes_per_pixel + x * bytes_per_pixel];
			unsigned char *dest = &r[x * bytes_per_pixel];
			memcpy(dest, src, bytes_per_pixel);
		}
	}

	png_init_io(png_ptr, f);
	png_set_rows(png_ptr, info_ptr, row);
	png_write_png(png_ptr, info_ptr, PNG_TRANSFORM_PACKING, NULL);

	for (y = 0; y < h; y++)
		png_free(png_ptr, row[y]);
	png_free(png_ptr, row);
	rc = 0;
cleanup2:
	png_destroy_write_struct(&png_ptr, &info_ptr);
cleanup1:
	fclose(f);
	return rc;
}

/* Copied and modified from snis_graph.c sng_load_png_texture(), see snis_graph.c */
char *load_png_image(const char *filename, int flipVertical, int flipHorizontal,
	int pre_multiply_alpha,
	int *w, int *h, int *hasAlpha, char *whynot, int whynotlen)
{
	int i, j, bit_depth, color_type, row_bytes, image_data_row_bytes;
	png_byte header[8];
	png_uint_32 tw, th;
	png_structp png_ptr = NULL;
	png_infop info_ptr = NULL;
	png_infop end_info = NULL;
	png_byte *image_data = NULL;

	FILE *fp = fopen(filename, "rb");
	if (!fp) {
		snprintf(whynot, whynotlen, "Failed to open '%s': %s",
			filename, strerror(errno));
		return 0;
	}

	if (fread(header, 1, 8, fp) != 8) {
		snprintf(whynot, whynotlen, "Failed to read 8 byte header from '%s'\n",
				filename);
		goto cleanup;
	}
	if (png_sig_cmp(header, 0, 8)) {
		snprintf(whynot, whynotlen, "'%s' isn't a png file.",
			filename);
		goto cleanup;
	}

	png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING,
							NULL, NULL, NULL);
	if (!png_ptr) {
		snprintf(whynot, whynotlen,
			"png_create_read_struct() returned NULL");
		goto cleanup;
	}

	info_ptr = png_create_info_struct(png_ptr);
	if (!info_ptr) {
		snprintf(whynot, whynotlen,
			"png_create_info_struct() returned NULL");
		goto cleanup;
	}

	end_info = png_create_info_struct(png_ptr);
	if (!end_info) {
		snprintf(whynot, whynotlen,
			"2nd png_create_info_struct() returned NULL");
		goto cleanup;
	}

	if (setjmp(png_jmpbuf(png_ptr))) {
		snprintf(whynot, whynotlen, "libpng encounted an error");
		goto cleanup;
	}

	png_init_io(png_ptr, fp);
	png_set_sig_bytes(png_ptr, 8);

	/*
	 * PNG_TRANSFORM_STRIP_16 |
	 * PNG_TRANSFORM_PACKING  forces 8 bit
	 * PNG_TRANSFORM_EXPAND forces to expand a palette into RGB
	 */
	png_read_png(png_ptr, info_ptr, PNG_TRANSFORM_STRIP_16 | PNG_TRANSFORM_PACKING | PNG_TRANSFORM_EXPAND, NULL);

	png_get_IHDR(png_ptr, info_ptr, &tw, &th, &bit_depth, &color_type, NULL, NULL, NULL);

	if (bit_depth != 8) {
		snprintf(whynot, whynotlen, "load_png_texture only supports 8-bit image channel depth");
		goto cleanup;
	}

	if (color_type != PNG_COLOR_TYPE_RGB && color_type != PNG_COLOR_TYPE_RGB_ALPHA) {
		snprintf(whynot, whynotlen, "load_png_texture only supports RGB and RGBA");
		goto cleanup;
	}

	if (w)
		*w = tw;
	if (h)
		*h = th;
	int has_alpha = (color_type == PNG_COLOR_TYPE_RGB_ALPHA);
	if (hasAlpha)
		*hasAlpha = has_alpha;

	row_bytes = png_get_rowbytes(png_ptr, info_ptr);
	image_data_row_bytes = row_bytes;

	/* align to 4 byte boundary */
	if (image_data_row_bytes & 0x03)
		image_data_row_bytes += 4 - (image_data_row_bytes & 0x03);

	png_bytepp row_pointers = png_get_rows(png_ptr, info_ptr);

	image_data = malloc(image_data_row_bytes * th * sizeof(png_byte) + 15);
	if (!image_data) {
		snprintf(whynot, whynotlen, "malloc failed in load_png_texture");
		goto cleanup;
	}

	int bytes_per_pixel = (color_type == PNG_COLOR_TYPE_RGB_ALPHA ? 4 : 3);

	for (i = 0; i < th; i++) {
		png_byte *src_row;
		png_byte *dest_row = image_data + i * image_data_row_bytes;

		if (flipVertical)
			src_row = row_pointers[th - i - 1];
		else
			src_row = row_pointers[i];

		if (flipHorizontal) {
			for (j = 0; j < tw; j++) {
				png_byte *src = src_row + bytes_per_pixel * j;
				png_byte *dest = dest_row + bytes_per_pixel * (tw - j - 1);
				memcpy(dest, src, bytes_per_pixel);
			}
		} else {
			memcpy(dest_row, src_row, row_bytes);
		}

		if (has_alpha && pre_multiply_alpha) {
			for (j = 0; j < tw; j++) {
				png_byte *pixel = dest_row + bytes_per_pixel * j;
				float alpha = pixel[3] / 255.0;
				pixel[0] = pixel[0] * alpha;
				pixel[1] = pixel[1] * alpha;
				pixel[2] = pixel[2] * alpha;
			}
		}
	}

	png_destroy_read_struct(&png_ptr, &info_ptr, &end_info);
	fclose(fp);
	return (char *) image_data;

cleanup:
	if (image_data)
		free(image_data);
	png_destroy_read_struct(&png_ptr, &info_ptr, &end_info);
	fclose(fp);
	return 0;
}

static char *load_image(const char *filename, int *w, int *h, int *a, int *bytes_per_row)
{
	char *i;
	char msg[100];

	i = load_png_image(filename, 0, 0, 0, w, h, a, msg, sizeof(msg));
	if (!i) {
		fprintf(stderr, "%s: cannot load image: %s\n", filename, msg);
		exit(1);
	}
	*bytes_per_row = *w * 3;
	/* align to 4 byte boundary */
	if (*bytes_per_row & 0x03)
		*bytes_per_row += 4 - (*bytes_per_row & 0x03);
	return i;
}

static void save_images(const char *prefix, unsigned char *image[])
{
	int i;
	char fname[PATH_MAX];

	for (i = 0; i < 6; i++) {
		sprintf(fname, "%s%d.png", prefix, i);
		if (write_png_image(fname, image[i], DIM, DIM, 1))
			fprintf(stderr, "Failed to write %s\n", fname);
	}
	printf("o");
	fflush(stdout);
}

static void save_output_images(void)
{
	save_images(output_file_prefix, output_image);
}

static void save_normal_maps(void)
{
	save_images(normal_file_prefix, normal_image);
}

static void calculate_normal(int f, int i, int j)
{
	int i1, i2, j1, j2;
	int p1, p2, dzdx, dzdy;
	union vec3 n;

	i1 = i - 1;
	if (i1 < 0)
		i1 = i;
	i2 = i + 1;
	if (i2 >= DIM)
		i2 = i;
	j1 = j - 1;
	if (j1 < 0)
		j1 = j;
	j2 = j + 1;
	if (j2 >= DIM)
		j2 = j;

	p1 = (j * DIM + i1) * 4; 
	p2 = (j * DIM + i2) * 4; 
	dzdx = (int) output_image[f][p1] - (int) output_image[f][p2];
	p1 = (j1 * DIM + i) * 4; 
	p2 = (j2 * DIM + i) * 4; 
	dzdy = (int) output_image[f][p2] - (int) output_image[f][p1];
	n.v.x = (float) dzdx / 127.0f + 0.5;
	n.v.y = (float) dzdy / 127.0f + 0.5;
	n.v.z = 1.0f;
	normal[f][i][j] = n;
}

static void calculate_normals(void)
{
	int f, i, j;

	printf("calculating normals\n");
	for (f = 0; f < 6; f++) {
		for (i = 0; i < DIM; i++) {
			for (j = 0; j < DIM; j++) {
				calculate_normal(f, i, j);
			}
		}
	}
}

static struct option long_options[] = {
	{ "bumps", required_argument, NULL, 'b' },
	{ "craters", required_argument, NULL, 'c' },
	{ "height", required_argument, NULL, 'h' },
	{ "ibumps", required_argument, NULL, 'i' },
	{ "land", required_argument, NULL, 'l' },
	{ "oceanlevel", required_argument, NULL, 'O' },
	{ "output", required_argument, NULL, 'o' },
	{ "normal", required_argument, NULL, 'n' },
	{ "rlimit", required_argument, NULL, 'r' },
	{ "seed", required_argument, NULL, 'S' },
	{ "scatter", required_argument, NULL, 's' },
	{ "shrink", required_argument, NULL, 'k' },
	{ "water", required_argument, NULL, 'w' },
	{ 0 },
};

static void usage(void)
{
	fprintf(stderr, "usage: earthlike [ -h heightdata.png ] [ -l land.png ] [ -w water.png ]\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "Options:\n");
	fprintf(stderr, "   -b, bumps : number of bumps in recursive bump placement, default = 3\n");
	fprintf(stderr, "   -c, craters : number of craters to add.\n");
	fprintf(stderr, "   -h, height : png file containing height map data to sample for terrain\n");
	fprintf(stderr, "   -k, shrink : factor to shrink each recursive iteration.  Default is 0.55\n");
	fprintf(stderr, "   -l, land : png file containing land color data to sample for terrain\n");
	fprintf(stderr, "   -n, normal : filename prefix for normal map images, default is 'normalmap'\n");
	fprintf(stderr, "   -o, output : filename prefix for output images, default is 'heightmap'\n");
	fprintf(stderr, "   -O, oceanlevel : set sealevel, default is 0.08\n");
	fprintf(stderr, "   -r, rlimit : limit to how small bumps may get before stopping recursion\n");
	fprintf(stderr, "                default rlimit is 0.005\n");
	fprintf(stderr, "   -s, scatter : float amount to scatter bumps, default = 1.8 * radius\n");
	fprintf(stderr, "   -S, seed : set initial random seed.  Default is 31415\n");
	fprintf(stderr, "   -w, water : png file containing water color data to sample for oceans\n");
	fprintf(stderr, "\n");
	exit(1);
}

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

static void process_options(int argc, char *argv[])
{
	int c;

	while (1) {
		int option_index;
		c = getopt_long(argc, argv, "b:c:h:i:n:O:k:l:o:r:s:S:w:", long_options, &option_index);
		if (c == -1)
			break;
		switch (c) {
		case 'b':
			process_int_option("bumps", optarg, &nbumps);
			break;
		case 'c':
			process_int_option("craters", optarg, &ncraters);
			if (ncraters < 0)
				ncraters = 0;
			if (ncraters > MAXCRATERS)
				ncraters = MAXCRATERS;
			break;
		case 'h':
			heightfile = optarg;
			break;
		case 'i':
			process_int_option("ibumps", optarg, &initial_bumps);
			break;
		case 'n':
			normal_file_prefix = optarg;
			break;
		case 'o':
			output_file_prefix = optarg;
			break;
		case 'k':
			process_float_option("shrink", optarg, &shrink_factor);
			break;
		case 'l':
			landfile = optarg;
			break;
		case 'O':
			process_float_option("oceanlevel", optarg, &sealevel);
			break;
		case 'r':
			process_float_option("rlimit", optarg, &rlimit);
			break;
		case 's':
			process_float_option("scatterfactor", optarg, &scatterfactor);
			break;
		case 'S':
			process_int_option("seed", optarg, &random_seed);
			break;
		case 'w':
			waterfile = optarg;
			break;
		default:
			fprintf(stderr, "Unknown option.\n");
			usage();
		}
	}
}

int main(int argc, char *argv[])
{
	float min, max;

	setlocale(LC_ALL, "C");
	process_options(argc, argv);

	snis_srand((unsigned int) random_seed);
	open_simplex_noise(random_seed, &ctx);
	sampledata = load_image(heightfile, &samplew, &sampleh, &samplea,
					&sample_bytes_per_row);
	land = load_image(landfile, &landw, &landh, &landa, &landbpr);
	water = load_image(waterfile, &waterw, &waterh, &watera, &waterbpr);
	allocate_output_images();
	initialize_vertices();
	find_min_max_height(&min, &max);
	add_bumps(initial_bumps);
	add_craters(ncraters);
	printf("total bumps = %d\n", totalbumps);
	render_all_bumps();
	render_all_craters(min, max);
	find_min_max_height(&min, &max);
	printf("min h = %f, max h = %f\n", min, max);
	paint_height_maps(min, max);
	calculate_normals();
	paint_normal_maps(min, max);
	printf("painting terrain colors\n");
	paint_terrain_colors(min, max);
	save_output_images();
	save_normal_maps();
	open_simplex_noise_free(ctx);
	return 0;
}

