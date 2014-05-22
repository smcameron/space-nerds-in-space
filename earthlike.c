#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <png.h>
#include <errno.h>
#include <limits.h>
#include <pthread.h>

#include "mtwist.h"
#include "mathutils.h"
#include "quat.h"

#define MAXBUMPS 100000

static struct bump {
	union vec3 p;
	float r, h;
} bumplist[MAXBUMPS];
static int totalbumps = 0;

#define DIM 1024
static unsigned char *output_image[6];
static union vec3 vertex[6][DIM][DIM];
static const char *output_file_prefix = "heightmap";

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
			for (j = 0; j < DIM; j++)
				vertex[f][i][j] = fij_to_xyz(f, i, j, DIM);
}

static inline void distort_vertex(union vec3 *v, float d, const float r, float h)
{
	union vec3 distortion;

	float nr = 0.5 * (cos(M_PI * d / r) + 1.0f) * h;

	vec3_normalize(&distortion, v);
	vec3_mul_self(&distortion, nr);
	vec3_add_self(v, &distortion);
}

struct thread_info {
	pthread_t thread;
	int f;
};

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
				d2 = (p.v.x - p2->v.x) * (p.v.x - p2->v.x) +
					(p.v.y - p2->v.y) * (p.v.y - p2->v.y) +
					(p.v.z - p2->v.z) * (p.v.z - p2->v.z);
				if (d2 > b->r * b->r)
					continue;
				d = sqrtf(d2);
				distort_vertex(&vertex[f][i][j], d, b->r, b->h);
			}
		}
	}
	return NULL;
}

static void render_all_bumps(void)
{
	int rc, f;
	void *status;
	struct thread_info t[6];

	for (f = 0; f < 6; f++) {
		t[f].f = f;
		rc = pthread_create(&t[f].thread, NULL, render_bumps_on_face_fn, &t[f]);
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

static void add_bump(union vec3 p, float r, float h)
{
	struct bump *b;

	if (totalbumps >= MAXBUMPS)
		return;

	b = &bumplist[totalbumps];
	b->p = p;
	b->r = r;
	b->h = h;
	totalbumps++;
}
	
static void recursive_add_bump(union vec3 pos, float r, float h,
				float shrink, float rlimit)
{
	const int nbumps = 3;
	float hoffset;

	add_bump(pos, r, h);
	if (r * shrink < rlimit)
		return;
	for (int i = 0; i < nbumps; i++) {
		union vec3 d;
		d.v.x = snis_random_float() * r;
		d.v.y = snis_random_float() * r;
		d.v.z = snis_random_float() * r;
		vec3_add_self(&d, &pos);
		vec3_normalize_self(&d);
		hoffset = snis_random_float() * h * shrink * 0.5;
		recursive_add_bump(d, r * shrink, h * shrink * 0.5 + hoffset, shrink, rlimit);
	}
}

static void add_bumps(const int nbumps)
{
	int i;

	for (i = 0; i < nbumps; i++) {
		union vec3 p;
		float r = 0.5 * (snis_random_float() + 1.0f) * 0.9;

		random_point_on_sphere(1.0, &p.v.x, &p.v.y, &p.v.z);
		recursive_add_bump(p, r, 0.04, 0.52, 0.01);
		printf(".");
		fflush(stdout);
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
}

static void paint_height_maps(float min, float max)
{
	int f, i, j;
	float r; 
	unsigned char c;
	int p;

	for (f = 0; f <6; f++) {
		for (i = 0; i < DIM; i++) {
			for (j = 0; j < DIM; j++) {
				p = (j * DIM + i) * 4; 
				r = vec3_magnitude(&vertex[f][i][j]);
				r = (r - min) / (max - min);
				c = (unsigned char) (r * 255.0f);
				if (r > 0.10) {
					output_image[f][p + 0] = c;
					output_image[f][p + 1] = c;
					output_image[f][p + 2] = c;
				} else {
					output_image[f][p + 0] = 20;
					output_image[f][p + 1] = 100;
					output_image[f][p + 2] = 200;
				}
				output_image[f][p + 3] = 255;
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

static void save_output_images(void)
{
	int i;
	char fname[PATH_MAX];

	for (i = 0; i < 6; i++) {
		sprintf(fname, "%s%d.png", output_file_prefix, i);
		if (write_png_image(fname, output_image[i], DIM, DIM, 1))
			fprintf(stderr, "Failed to write %s\n", fname);
	}
	printf("o");
	fflush(stdout);
}

int main(int argc, char *argv[])
{
	float min, max;

	allocate_output_images();
	initialize_vertices();
	find_min_max_height(&min, &max);
	add_bumps(40);
	printf("total bumps = %d\n", totalbumps);
	render_all_bumps();
	find_min_max_height(&min, &max);
	printf("min h = %f, max h = %f\n", min, max);
	paint_height_maps(min, max);
	save_output_images();
	return 0;
}

