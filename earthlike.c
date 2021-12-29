#define _GNU_SOURCE
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
#include "png_utils.h"
#include "crater.h"
#include "pthread_util.h"

static struct osn_context *ctx;

#define MAXBUMPS 100000
#define MAXCRATERS 2000
#define RADII (3.0f)
static float sealevel = 0.08;
static int height_histogram[256] = { 0 };
static float fraction_land = 0.35;

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
static float initial_bump_size = 0.4;
static unsigned char crater_base_level = 127;

static struct bump {
	union vec3 p;
	float r, h;
	union quat texelq;
	int tx, ty; /* origin of texel region in sample data */
	float ts; /* scaling factor to get from 3d dist to texel dist */
	char *sampledata;
	int samplew, sampleh, sample_bytes_per_row;
	int is_crater;
} bumplist[MAXBUMPS], craterlist[MAXCRATERS];
static int totalbumps = 0;

#define MAXDIM 2048
int dim = 1024;
static unsigned char *output_image[6];
static unsigned char *normal_image[6];
static unsigned char *heightmap_image[6];
static unsigned char *land, *water;
static int landw, landh, landa, landbpr;
static int waterw, waterh, watera, waterbpr; 
static union vec3 vertex[6][MAXDIM][MAXDIM];
static union vec3 normal[6][MAXDIM][MAXDIM];
static float noise[6][MAXDIM][MAXDIM];
static char *output_file_prefix = "terrainmap";
static char *normal_file_prefix = "normalmap";
static char *heightmap_file_prefix = "heightmap";
static char *sampledata;
static int samplew, sampleh, samplea, sample_bytes_per_row;
static float minn, maxn; /* min and max noise values encountered */
static float crater_min_height, crater_max_height;

static unsigned char get_sampledata(int x, int y, int bytes_per_row, char *sampledata)
{
	int p;
	unsigned char *c;

	p = y * bytes_per_row + x * 3;
	c = (unsigned char *) &sampledata[p];
	return *c;
}

static void set_sampledata(int x, int y, int bytes_per_row, char *sampledata, unsigned char value)
{
	int i, p;
	unsigned char *c;

	p = y * bytes_per_row + x * 3;
	c = (unsigned char *) &sampledata[p];
	for (i = 0; i < 3; i++)
		c[i] = value;
}

static void scale_sampledata(char *sampledata, int samplew, int sampleh, int sample_bytes_per_row)
{
	int x, y;
	float lowest, highest, diff;
	lowest = 1000;
	highest = 0;

	printf("Scaling sample height data\n");
	for (y = 0; y < sampleh; y++) {
		for (x = 0; x < samplew; x++) {
			unsigned char c = get_sampledata(x, y, sample_bytes_per_row, sampledata);
			if (c > highest)
				highest = c;
			if (c < lowest)
				lowest = c;
		}
	}
	diff = (highest - lowest);
	for (y = 0; y < sampleh; y++) {
		for (x = 0; x < samplew; x++) {
			unsigned char c = get_sampledata(x, y, sample_bytes_per_row, sampledata);
			float v = c;
			v = v - lowest;
			v = 100 + 155.0 * v / diff;
			c = (unsigned char) v;
			set_sampledata(x, y, sample_bytes_per_row, sampledata, c);
		}
	}
	printf("scale sampledata done\n");
}

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
		for (i = 0; i < dim; i++)
			for (j = 0; j < dim; j++) {
				const union vec3 v = fij_to_xyz(f, i, j, dim);
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
	nr = nr * (!b->is_crater) + (1.0 * b->is_crater);

	vec3_normalize(&distortion, v);
	quat_rot_vec(&texelv, &distortion, &b->texelq);
	vec3_mul_self(&texelv, b->ts);
	x = (int) texelv.v.x + b->tx;
	y = (int) texelv.v.y + b->ty;
	if (x < 0 || x > b->samplew || y < 0 || y > b->sampleh) {
		printf("out of range (%d, %d)\n", x, y);
		return;
	}
	p = y * b->sample_bytes_per_row + x * 3;
	c = (unsigned char *) &b->sampledata[p];
	m = (float) *c;
	m = (m - 127.0) / 256.0; /* essentially, convert to signed. */
	vec3_mul_self(&distortion, nr * m);
	vec3_add_self(v, &distortion);
}

struct thread_info {
	pthread_t thread;
	struct bump *bumplist;
	int bumpcount;
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

	for (i = 0; i < dim; i++) {
		if (i % (dim / 8) == 0) {
			printf("%d", f);
			fflush(stdout);
		}
		for (j = 0; j < dim; j++) {
			p = fij_to_xyz(f, i, j, dim);
			for (k = 0; k < t->bumpcount; k++) {
				b = &t->bumplist[k];
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

static void multithread_face_render(render_on_face_fn render, struct bump *bumplist, int bumpcount)
{
	int rc, f;
	void *status;
	struct thread_info t[6];

	for (f = 0; f < 6; f++) {
		t[f].f = f;
		t[f].bumplist = bumplist;
		t[f].bumpcount = bumpcount;
		rc = create_thread(&t[f].thread, render, &t[f], "face-render", 0);
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
	printf("  6 faces rendered.\n");
}

static void render_all_bumps(void)
{
	if (totalbumps > 0) {
		printf("Rendering bumps on 6 faces with 6 threads\n");
		multithread_face_render(render_bumps_on_face_fn, bumplist, totalbumps);
	}
}

static void render_all_craters(float min, float max)
{
	crater_min_height = min;
	crater_max_height = max;
	if (ncraters > 0) {
		printf("Rendering craters on 6 faces with 6 threads\n");
		multithread_face_render(render_bumps_on_face_fn, craterlist, ncraters);
	}
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
	b->sampledata = sampledata;
	b->samplew = samplew * MAXDIM / dim;
	b->sampleh = sampleh * MAXDIM / dim;
	b->sample_bytes_per_row = sample_bytes_per_row;
	b->is_crater = 0;
	quat_from_u2v(&b->texelq, &p, &right_at_ya, &up);
	totalbumps++;
}

static void add_crater(int i, union vec3 p, float r, float h)
{
	struct bump *b;
	const union vec3 right_at_ya = { { 0.0f, 0.0f, 1.0f } };
	const union vec3 up = { { 0.0f, 1.0f, 0.0f } };
	float crater_r;

	b = &craterlist[i];
	b->is_crater = 1;
	b->p = p;
	b->r = r * (float) MAXDIM / (float) dim;
	b->h = h;
	b->tx = 512;
	b->ty = 512;
	b->ts = 512;
	b->sampledata = malloc(3 * 1024 * 1024);
	memset(b->sampledata, crater_base_level, 3 * 1024 * 1024);
	b->samplew = 1024;
	b->sampleh = 1024;
	b->sample_bytes_per_row = 3 * b->samplew;
	crater_r = (0.5 * (snis_random_float)() + 1.0);
	crater_r = (-logf(1.0 - crater_r) / 10.0) * 35.0 + 1.0;
	create_crater_heightmap((unsigned char *) b->sampledata, 1024, 1024, 512, 512, (int) crater_r, 6);
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

static void add_bumps(const int initial_bumps)
{
	int i;
	float h;

	if (nbumps == 1) /* If we're not doing any recursive bumps, then make the bumps taller */
		h = 0.3 * MAXDIM / dim;
	else
		h = 0.1 * MAXDIM / dim;
	printf("adding bumps:");
	for (i = 0; i < initial_bumps; i++) {
		union vec3 p;
		float r = 0.5 * (snis_random_float() + 1.0f) * initial_bump_size;

		random_point_on_sphere(1.0, &p.v.x, &p.v.y, &p.v.z);
		recursive_add_bump(p, r, h, shrink_factor, rlimit);
		printf(".");
		fflush(stdout);
	}
	printf("\n");
}

static void add_craters(const int nc)
{
	int i;

	printf("adding craters:");
	for (i = 0; i < nc; i++) {
		union vec3 p;
		random_point_on_sphere(1.0, &p.v.x, &p.v.y, &p.v.z);
		add_crater(i, p, 1.0, 0.005f);
		printf("o"); fflush(stdout);
	}
	printf("\n");
}

static void find_min_max_height(float *min, float *max)
{
	int f, i, j;
	float mmin, mmax;
	float h;

	printf("Finding min and max height\n");
	mmin = 1000000.0f;
	mmax = 0.0f;

	for (f = 0; f < 6; f++) {
		for (i = 0; i < dim; i++) {
			for (j = 0; j < dim; j++) {
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
		output_image[i] = malloc(4 * dim * dim);
		memset(output_image[i], 0, 4 * dim * dim);
	}
	for (i = 0; i < 6; i++) {
		normal_image[i] = malloc(4 * dim * dim);
		memset(normal_image[i], 0, 4 * dim * dim);
	}
	for (i = 0; i < 6; i++) {
		heightmap_image[i] = malloc(4 * dim * dim);
		memset(heightmap_image[i], 128, 4 * dim * dim);
	}
}

static void paint_height_maps(float min, float max)
{
	int f, i, j;
	float r; 
	unsigned char c;
	int p;

	for (f = 0; f < 6; f++) {
		for (i = 0; i < dim; i++) {
			for (j = 0; j < dim; j++) {
				p = (j * dim + i) * 4;
				r = vec3_magnitude(&vertex[f][i][j]);
				r = (r - min) / (max - min);
				c = (unsigned char) (r * 255.0f);
				output_image[f][p + 0] = c;
				output_image[f][p + 1] = c;
				output_image[f][p + 2] = c;
				output_image[f][p + 3] = 255;
				height_histogram[c]++;
			}
		}
	}
}

static void set_sealevel(float fraction_land)
{
	int i, cutoff, so_far = 0;

	cutoff = (int) ((1.0 - fraction_land) * 6.0 * dim * dim);
	for (i = 0; i < 256; i++) {
		so_far += height_histogram[i];
		if (so_far >= cutoff) {
			sealevel = ((float) i / (float) 256);
			return;
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
		for (i = 0; i < dim; i++) {
			for (j = 0; j < dim; j++) {
				p = (j * dim + i) * 4;
				r = vec3_magnitude(&vertex[f][i][j]);
				r = (r - min) / (max - min);
				n = (noise[f][i][j] - minn) / (maxn - minn);
				color_output(f, p, r, n);
			}
		}
	}
}


static void paint_normal_and_height_maps(float min, float max)
{
	int f, i, j;
	float rad; 
	char red, green, blue;
	int p;

	for (f = 0; f < 6; f++) {
		for (i = 0; i < dim; i++) {
			for (j = 0; j < dim; j++) {
				p = (j * dim + i) * 4;
				rad = vec3_magnitude(&vertex[f][i][j]);
				rad = (rad - min) / (max - min);
				heightmap_image[f][p + 0] = (unsigned char) (255.0 * rad);
				heightmap_image[f][p + 1] = (unsigned char) (255.0 * rad);
				heightmap_image[f][p + 2] = (unsigned char) (255.0 * rad);
				heightmap_image[f][p + 3] = 255;
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

static char *load_image(const char *filename, int *w, int *h, int *a, int *bytes_per_row)
{
	char *i;
	char msg[100];

	i = png_utils_read_png_image(filename, 0, 0, 0, w, h, a, msg, sizeof(msg));
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
		if (png_utils_write_png_image(fname, image[i], dim, dim, 1, 0))
			fprintf(stderr, "Failed to write %s\n", fname);
	}
}

static void save_output_images(void)
{
	save_images(output_file_prefix, output_image);
}

static void save_normal_maps(void)
{
	save_images(normal_file_prefix, normal_image);
}

static void save_height_maps(void)
{
	save_images(heightmap_file_prefix, heightmap_image);
}

static void calculate_normal(int f, int i, int j)
{
	int i1, i2, j1, j2;
	int p1, p2, dzdx[3], dzdy[3];
	union vec3 n;

	i1 = i - 1;
	if (i1 < 0)
		i1 = i;
	i2 = i + 1;
	if (i2 >= dim)
		i2 = i;
	j1 = j - 1;
	if (j1 < 0)
		j1 = j;
	j2 = j + 1;
	if (j2 >= dim)
		j2 = j;

	/* Average over the surrounding 3x3 pixels in x and in y, emphasizing the central regions
	 * (Sobel filter,  https://en.wikipedia.org/wiki/Sobel_operator )
	 */
	p1 = (j1 * dim + i1) * 4;
	p2 = (j1 * dim + i2) * 4;
	dzdx[0] = (int) output_image[f][p1] - (int) output_image[f][p2];
	p1 = (j * dim + i1) * 4;
	p2 = (j * dim + i2) * 4;
	dzdx[1] = (int) output_image[f][p1] - (int) output_image[f][p2];
	p1 = (j2 * dim + i1) * 4;
	p2 = (j2 * dim + i2) * 4;
	dzdx[2] = (int) output_image[f][p1] - (int) output_image[f][p2];

	p1 = (j1 * dim + i1) * 4;
	p2 = (j2 * dim + i1) * 4;
	dzdy[0] = (int) output_image[f][p2] - (int) output_image[f][p1];
	p1 = (j1 * dim + i) * 4;
	p2 = (j2 * dim + i) * 4;
	dzdy[1] = (int) output_image[f][p2] - (int) output_image[f][p1];
	p1 = (j1 * dim + i2) * 4;
	p2 = (j2 * dim + i2) * 4;
	dzdy[2] = (int) output_image[f][p2] - (int) output_image[f][p1];

	dzdx[0] = dzdx[0] + 2 * dzdx[1] + dzdx[1];
	dzdy[0] = -dzdy[0] - 2 * dzdy[1] - dzdy[1];

	n.v.x = ((float) dzdx[0] / 4.0) / 127.0f + 0.5;
	n.v.y = ((float) dzdy[0] / 4.0) / 127.0f + 0.5;
	n.v.z = 1.0f;
	normal[f][i][j] = n;
}

static void calculate_normals(void)
{
	int f, i, j;

	printf("calculating normals\n");
	for (f = 0; f < 6; f++) {
		for (i = 0; i < dim; i++) {
			for (j = 0; j < dim; j++) {
				calculate_normal(f, i, j);
			}
		}
	}
}

static struct option long_options[] = {
	{ "bumps", required_argument, NULL, 'b' },
	{ "initialbumps", required_argument, NULL, 'B' },
	{ "craters", required_argument, NULL, 'c' },
	{ "dimension", required_argument, NULL, 'd' },
	{ "height", required_argument, NULL, 'h' },
	{ "heightmap-output", required_argument, NULL, 'H' },
	{ "ibumps", required_argument, NULL, 'i' },
	{ "land", required_argument, NULL, 'l' },
	{ "land-fraction", required_argument, NULL, 'L' },
	{ "oceanlevel", required_argument, NULL, 'O' },
	{ "output", required_argument, NULL, 'o' },
	{ "normal", required_argument, NULL, 'n' },
	{ "rlimit", required_argument, NULL, 'r' },
	{ "seed", required_argument, NULL, 'S' },
	{ "scatter", required_argument, NULL, 's' },
	{ "shrink", required_argument, NULL, 'k' },
	{ "water", required_argument, NULL, 'w' },
	{ "bumpsize", required_argument, NULL, 'z'},
	{ 0 },
};

static void usage(void)
{
	fprintf(stderr, "usage: earthlike [ -h heightdata.png ] [ -l land.png ] [ -w water.png ]\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "Options:\n");
	fprintf(stderr, "   -b, bumps : number of bumps in recursive bump placement, default = 3\n");
	fprintf(stderr, "   -B, initialbumps: number of bumps in initial pass.  default = 60\n");
	fprintf(stderr, "   -d, dimension: size of output images.  Default is 1024 for 1024x1024 images.\n");
	fprintf(stderr, "                 Minimum is 128, maximum is %d\n", MAXDIM);
	fprintf(stderr, "   -c, craters : number of craters to add.\n");
	fprintf(stderr, "   -h, height : png file containing height map data to sample for terrain\n");
	fprintf(stderr, "   -H, heightmap-output : prefix of filename for height map output data\n");
	fprintf(stderr, "                Default is to append '-heightmap' to the output filename\n");
	fprintf(stderr, "                prefex (-o option). If the -o option is not specified, then\n");
	fprintf(stderr, "                'heightmap' is used.\n");
	fprintf(stderr, "   -k, shrink : factor to shrink each recursive iteration.  Default is 0.55\n");
	fprintf(stderr, "   -l, land : png file containing land color data to sample for terrain\n");
	fprintf(stderr, "   -L, land fraction: Range 0.0 to 1.0, approximate fraction of planet to be land\n");
	fprintf(stderr, "   -n, normal : filename prefix for normal map images, default is to append\n");
	fprintf(stderr, "                '-normalmap' to the output filename prefex (-o option). If the\n");
	fprintf(stderr, "                -o option is not specified, then 'normalmap' is used.\n");
	fprintf(stderr, "   -o, output : filename prefix for output images, default is 'terrainmap'\n");
	fprintf(stderr, "   -O, oceanlevel : set sealevel, default is 0.08\n");
	fprintf(stderr, "   -r, rlimit : limit to how small bumps may get before stopping recursion\n");
	fprintf(stderr, "                default rlimit is 0.005\n");
	fprintf(stderr, "   -s, scatter : float amount to scatter bumps, default = 1.8 * radius\n");
	fprintf(stderr, "   -S, seed : set initial random seed.  Default is 31415\n");
	fprintf(stderr, "   -w, water : png file containing water color data to sample for oceans\n");
	fprintf(stderr, "   -z, bumpsize: initial bump size.  default = 0.4\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "Examples:\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "  For earthlike planets:\n");
	fprintf(stderr, "  ./earthlike -B 400 -h heightdata.png -l land.png -w water.png --bumpsize 0.5 \\\n");
	fprintf(stderr, "              -L 0.35 -o myplanet\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "  For more rocky, mars-like planets (a.png and b.png should both be land images):\n");
	fprintf(stderr, "  ./earthlike -B 400 -b 1 -h heightdata.png -l ~/a.png -w ~/b.png -L 1.0 --bumpsize 0.5 \\\n");
	fprintf(stderr, "         --craters 2000 -o myplanet\n");
	fprintf(stderr, "\n");
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
	int tmpdim;

	while (1) {
		int option_index;
		c = getopt_long(argc, argv, "B:L:b:d:c:H:h:i:n:O:k:l:o:r:s:S:w:", long_options, &option_index);
		if (c == -1)
			break;
		switch (c) {
		case 'b':
			process_int_option("bumps", optarg, &nbumps);
			break;
		case 'B':
			process_int_option("initialbumps", optarg, &initial_bumps);
			break;
		case 'c':
			process_int_option("craters", optarg, &ncraters);
			if (ncraters < 0)
				ncraters = 0;
			if (ncraters > MAXCRATERS)
				ncraters = MAXCRATERS;
			break;
		case 'd':
			process_int_option("dimension", optarg, &tmpdim);
			dim = tmpdim;
			if (dim < 128)
				dim = 128;
			if (dim > MAXDIM)
				dim = MAXDIM;
			if (dim != tmpdim)
				printf("Dimension %d out of range, adjusted to %d\n", tmpdim, dim);
			break;
		case 'h':
			heightfile = optarg;
			break;
		case 'H':
			heightmap_file_prefix = optarg;
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
		case 'z':
			process_float_option("bumpsize", optarg, &initial_bump_size);
			break;
		case 'L':
			process_float_option("land_fraction", optarg, &fraction_land);
			if (fraction_land < 0.0 || fraction_land > 1.0) {
				fprintf(stderr, "land-fraction must be between 0 and 1.\n");
				exit(1);
			}
			break;
		default:
			fprintf(stderr, "Unknown option.\n");
			usage();
		}
	}

	/* Set a reasonable default normalmap file name if none is specified */
	if (strcmp(normal_file_prefix, "normalmap") == 0 &&
		strcmp(output_file_prefix, "terrainmap") != 0) {
		char new_normalmap_prefix[PATH_MAX];

		snprintf(new_normalmap_prefix, PATH_MAX, "%s-normalmap", output_file_prefix);
		normal_file_prefix = strdup(new_normalmap_prefix);
	}

	/* Set a reasonable default heightmap file name if none is specified */
	if (strcmp(normal_file_prefix, "heightmap") == 0 &&
		strcmp(output_file_prefix, "terrainmap") != 0) {
		char new_normalmap_prefix[PATH_MAX];

		snprintf(new_normalmap_prefix, PATH_MAX, "%s-heightmap", output_file_prefix);
		normal_file_prefix = strdup(new_normalmap_prefix);
	}
}

static void print_histogram_bar(int nchars, int bucket, int value)
{
	int i;

	printf("%3d %10d: ", bucket, value);
	for (i = 0; i < nchars; i++)
		printf("#");
	printf("\n");
}

static void print_height_histogram(int h[], int n)
{
	int i;
	int max = 0;

	for (i = 0; i < n; i++) {
		if (h[i] > max)
			max = h[i];
	}
	for (i = 0; i < n; i++) {
		float ratio = (float) h[i] / (float) max;
		int nchars = (int) (ratio * 60.0);
		print_histogram_bar(nchars, i, h[i]);
	}
}

int main(int argc, char *argv[])
{
	float min, max;

	setlocale(LC_ALL, "C");
	process_options(argc, argv);

	snis_srand((unsigned int) random_seed);
	open_simplex_noise(random_seed, &ctx);
	printf("Loading %s\n", heightfile);
	sampledata = load_image(heightfile, &samplew, &sampleh, &samplea,
					&sample_bytes_per_row);
	if (sampledata)
		scale_sampledata(sampledata, samplew, sampleh, sample_bytes_per_row);
	printf("Loading %s\n", landfile);
	land = (unsigned char *) load_image(landfile, &landw, &landh, &landa, &landbpr);
	printf("Loading %s\n", waterfile);
	water = (unsigned char *) load_image(waterfile, &waterw, &waterh, &watera, &waterbpr);
	printf("Allocating output images.\n");
	allocate_output_images();
	printf("Initializing vertices.\n");
	initialize_vertices();
	find_min_max_height(&min, &max);
	add_bumps(initial_bumps);
	add_craters(ncraters);
	printf("Rendering %d total bumps\n", totalbumps);
	render_all_bumps();
	render_all_craters(min, max);
	find_min_max_height(&min, &max);
	printf("min h = %f, max h = %f\n", min, max);
	paint_height_maps(min, max);
	set_sealevel(fraction_land);
	calculate_normals();
	paint_normal_and_height_maps(min, max);
	printf("painting terrain colors\n");
	paint_terrain_colors(min, max);
	printf("Writing color textures\n");
	save_output_images();
	printf("Writing normal maps\n");
	save_normal_maps();
	printf("Writing height maps\n");
	save_height_maps();
	open_simplex_noise_free(ctx);
	printf("Done.\n");
	print_height_histogram(height_histogram, 256);
	return 0;
}

