#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdint.h>
#include <unistd.h>

#include "snis_ship_type.h"
#include "string-utils.h"
#include "rts_unit_data.h"
#include "corporations.h"

static int parse_double(char *string, double *d)
{
	int rc;
	double n;

	rc = sscanf(string, "%lf", &n);
	if (rc == 1)
		*d = n;
	return rc;
}

static int parse_float(char *string, float *f)
{
	int rc;
	float n;

	rc = sscanf(string, "%f", &n);
	if (rc == 1)
		*f = n;
	return rc;
}

static int parse_int(char *string, int *i)
{
	int rc, j;

	rc = sscanf(string, "%d", &j);
	if (rc == 1)
		*i = j;
	return rc;
}

static int parse_float_field(float *var, char *filename, char *line, int linecount)
{
	int rc;

	rc = parse_float(get_field(line), var);
	if (rc != 1)
		fprintf(stderr, "Error at line %d in %s: '%s'\n", linecount, filename, line);
	return rc != 1;
}

static int parse_double_field(double *var, char *filename, char *line, int linecount)
{
	int rc;

	rc = parse_double(get_field(line), var);
	if (rc != 1)
		fprintf(stderr, "Error at line %d in %s: '%s'\n", linecount, filename, line);
	return rc != 1;
}

static int parse_int_field(int *var, char *filename, char *line, int linecount)
{
	int rc;

	rc = parse_int(get_field(line), var);
	if (rc != 1)
		fprintf(stderr, "Error at line %d in %s: '%s'\n", linecount, filename, line);
	return rc != 1;
}

/* Parse n (2 or 3) whitespace separated floats into v.  v is untouched on error. */
static int parse_float_tuple_field(float *v, int n, char *filename, char *line, int linecount)
{
	int i, rc;
	float t[3];

	rc = sscanf(get_field(line), "%f %f %f", &t[0], &t[1], &t[2]);
	if (rc != n) {
		fprintf(stderr, "Error at line %d in %s: '%s' (expected %d numbers)\n",
			linecount, filename, line, n);
		return 1;
	}
	for (i = 0; i < n; i++)
		v[i] = t[i];
	return 0;
}

/* A hard point as written in ship_types.txt: unscaled model units and degrees */
struct hard_point_spec {
	int present;
	float pos[3];
	float forward[3];
	float up[3];
	float elevation[2];
	float azimuth[2];
};

static void init_hard_point_spec(struct hard_point_spec *s)
{
	memset(s, 0, sizeof(*s));
	s->forward[0] = 1.0;
	s->up[1] = 1.0;
	s->elevation[0] = 0.0;
	s->elevation[1] = 90.0;
	s->azimuth[0] = -540.0; /* +/- 540 degrees, i.e. no limit */
	s->azimuth[1] = 540.0;
}

/* Convert a hard point spec into a struct hard_point: scale the position, convert degrees to
 * radians, and make forward and up an orthonormal pair (up is adjusted to be perpendicular to
 * forward).  Returns 0 on success, 1 if the spec is unusable, in which case hp->present is 0.
 */
static int finish_hard_point(struct hard_point_spec *s, float scale, struct hard_point *hp,
				char *class, char *filename, int linecount)
{
	float f[3], u[3], len, dot;
	int i;

	memset(hp, 0, sizeof(*hp));
	if (!s->present)
		return 0;

	len = sqrtf(s->forward[0] * s->forward[0] + s->forward[1] * s->forward[1] +
			s->forward[2] * s->forward[2]);
	if (len < 1e-6) {
		fprintf(stderr, "%s:%d: hard point forward vector for class %s is zero length\n",
			filename, linecount, class);
		return 1;
	}
	for (i = 0; i < 3; i++)
		f[i] = s->forward[i] / len;

	dot = s->up[0] * f[0] + s->up[1] * f[1] + s->up[2] * f[2];
	for (i = 0; i < 3; i++)
		u[i] = s->up[i] - dot * f[i];
	len = sqrtf(u[0] * u[0] + u[1] * u[1] + u[2] * u[2]);
	if (len < 1e-6) {
		fprintf(stderr, "%s:%d: hard point up vector for class %s is zero or parallel to forward\n",
			filename, linecount, class);
		return 1;
	}
	for (i = 0; i < 3; i++)
		u[i] = u[i] / len;

	if (s->elevation[0] > s->elevation[1] || s->azimuth[0] > s->azimuth[1]) {
		fprintf(stderr, "%s:%d: hard point for class %s has lower limit above upper limit\n",
			filename, linecount, class);
		return 1;
	}

	hp->pos.v.x = s->pos[0] * scale;
	hp->pos.v.y = s->pos[1] * scale;
	hp->pos.v.z = s->pos[2] * scale;
	hp->forward.v.x = f[0];
	hp->forward.v.y = f[1];
	hp->forward.v.z = f[2];
	hp->up.v.x = u[0];
	hp->up.v.y = u[1];
	hp->up.v.z = u[2];
	hp->elevation_lower_limit = s->elevation[0] * M_PI / 180.0;
	hp->elevation_upper_limit = s->elevation[1] * M_PI / 180.0;
	hp->azimuth_lower_limit = s->azimuth[0] * M_PI / 180.0;
	hp->azimuth_upper_limit = s->azimuth[1] * M_PI / 180.0;
	hp->present = 1;
	return 0;
}

struct ship_type_entry *snis_read_ship_types(char *filename, int *count)
{
	FILE *f;
	char line[255], class[255], model_file[PATH_MAX], thrust_attach[PATH_MAX];
	double toughness, max_speed;
	int warpchance;
	int crew_max;
	int ncargo_bays;
	char *x;
	int i;
	int max_speed_100x;
	int linecount = 0, n = 0;
	int nalloced = 0;
	struct ship_type_entry *st;
	float extra_scaling;
	int nrots;
	char axis[4];
	float rot[4];
	int has_lasers, has_torpedoes, has_missiles;
	float relative_mass;
	int errorcount = 0;
#define DEFAULT_SCRAP_VALUE 2500.0f
#define DEFAULT_SHIP_DECIMATION_FACTOR 8
#define DEFAULT_SHIP_MIN_TRIS 64
	float scrap_value = DEFAULT_SCRAP_VALUE;
	int decimation_factor = DEFAULT_SHIP_DECIMATION_FACTOR;
	int min_tris = DEFAULT_SHIP_MIN_TRIS;
	struct hard_point_spec hps;
	int in_hard_point = 0;

	nalloced = 30;
	st = malloc(sizeof(*st) * nalloced);
	if (!st)
		return NULL;

	f = fopen(filename, "r");
	if (!f) {
		free(st);
		return NULL;
	}

	class[0] = '\0';
	model_file[0] = '\0';
	thrust_attach[0] = '\0';
	toughness = 0.0;
	max_speed_100x = 0;
	warpchance = 0;
	crew_max = 0;
	ncargo_bays = 0;
	has_lasers = 0;
	has_torpedoes = 0;
	has_missiles = 0;
	extra_scaling = 1.0;
	relative_mass = 1.0;
	rot[0] = 0.0;
	rot[1] = 0.0;
	rot[2] = 0.0;
	rot[3] = 0.0;
	axis[0] = '\0';
	axis[1] = '\0';
	axis[2] = '\0';
	axis[3] = '\0';
	nrots = 0;
	init_hard_point_spec(&hps);

	while (!feof(f)) {
		x = fgets(line, sizeof(line) - 1, f);
		if (!x) {
			int en = errno;
			if (feof(f))
				break;
			fprintf(stderr, "%s: Error reading file: %s\n", filename, strerror(en));
			break;
		}
		if (strlen(line) == 0)
			continue;
		line[strlen(line) - 1] = '\0'; /* cut off trailing \n */
		linecount++;

		if (line[0] == '#') /* skip comment lines */
			continue;
		remove_trailing_whitespace(line);
		clean_spaces(line);
		if (strcmp(line, "") == 0)
			continue; /* skip blank lines */

		/* Inside a "hard point {" block, only hard point keys are valid, and "}" ends
		 * the hard point rather than the ship type.
		 */
		if (in_hard_point) {
			if (has_prefix("ship type {", line)) {
				fprintf(stderr, "%s:%d: unterminated hard point block\n",
					filename, linecount);
				errorcount++;
				in_hard_point = 0;
				/* fall through to handle "ship type {" normally */
			} else {
				if (has_prefix("}", line))
					in_hard_point = 0;
				else if (has_prefix("position:", line))
					errorcount += parse_float_tuple_field(hps.pos, 3,
								filename, line, linecount);
				else if (has_prefix("forward:", line))
					errorcount += parse_float_tuple_field(hps.forward, 3,
								filename, line, linecount);
				else if (has_prefix("up:", line))
					errorcount += parse_float_tuple_field(hps.up, 3,
								filename, line, linecount);
				else if (has_prefix("elevation:", line))
					errorcount += parse_float_tuple_field(hps.elevation, 2,
								filename, line, linecount);
				else if (has_prefix("azimuth:", line))
					errorcount += parse_float_tuple_field(hps.azimuth, 2,
								filename, line, linecount);
				else {
					fprintf(stderr, "Error at line %d in %s: '%s'\n",
						linecount, filename, line);
					errorcount++;
				}
				continue;
			}
		}

		if (has_prefix("ship type {", line)) {
			class[0] = '\0';
			model_file[0] = '\0';
			thrust_attach[0] = '\0';
			toughness = 0.0;
			max_speed_100x = 0;
			warpchance = 0;
			crew_max = 0;
			ncargo_bays = 0;
			has_lasers = 0;
			has_torpedoes = 0;
			has_missiles = 0;
			extra_scaling = 1.0;
			relative_mass = 1.0;
			decimation_factor = DEFAULT_SHIP_DECIMATION_FACTOR;
			min_tris = DEFAULT_SHIP_MIN_TRIS;
			rot[0] = 0.0;
			rot[1] = 0.0;
			rot[2] = 0.0;
			rot[3] = 0.0;
			axis[0] = '\0';
			axis[1] = '\0';
			axis[2] = '\0';
			axis[3] = '\0';
			nrots = 0;
			init_hard_point_spec(&hps);
			continue;
		} else if (has_prefix("hard point {", line)) {
			if (hps.present) {
				fprintf(stderr,
					"%s:%d: only one hard point per ship type is supported, using the last one\n",
					filename, linecount);
				errorcount++;
				init_hard_point_spec(&hps);
			}
			hps.present = 1;
			in_hard_point = 1;
		} else if (has_prefix("class:", line)) {
			strlcpy(class, get_field(line), sizeof(class));
		} else if (has_prefix("model file:", line)) {
			strlcpy(model_file, get_field(line), sizeof(model_file));
		} else if (has_prefix("thrust attach:", line)) {
			strlcpy(thrust_attach, get_field(line), sizeof(thrust_attach));
		} else if (has_prefix("toughness:", line)) {
			errorcount += parse_double_field(&toughness, filename, line, linecount);
		} else if (has_prefix("max speed:", line)) {
			errorcount += parse_int_field(&max_speed_100x, filename, line, linecount);
		} else if (has_prefix("warp chance:", line)) {
			errorcount += parse_int_field(&warpchance, filename, line, linecount);
		} else if (has_prefix("crew max:", line)) {
			errorcount += parse_int_field(&crew_max, filename, line, linecount);
		} else if (has_prefix("cargo bays:", line)) {
			errorcount += parse_int_field(&ncargo_bays, filename, line, linecount);
		} else if (has_prefix("has lasers:", line)) {
			errorcount += parse_int_field(&has_lasers, filename, line, linecount);
		} else if (has_prefix("has torpedoes:", line)) {
			errorcount += parse_int_field(&has_torpedoes, filename, line, linecount);
		} else if (has_prefix("has missiles:", line)) {
			errorcount += parse_int_field(&has_missiles, filename, line, linecount);
		} else if (has_prefix("extra scaling:", line)) {
			errorcount += parse_float_field(&extra_scaling, filename, line, linecount);
		} else if (has_prefix("relative mass:", line)) {
			errorcount += parse_float_field(&relative_mass, filename, line, linecount);
		} else if (has_prefix("scrap_value:", line)) {
			errorcount += parse_float_field(&scrap_value, filename, line, linecount);
		} else if (has_prefix("decimation factor:", line)) {
			errorcount += parse_int_field(&decimation_factor, filename, line, linecount);
		} else if (has_prefix("min tris:", line)) {
			errorcount += parse_int_field(&min_tris, filename, line, linecount);
		} else if (has_prefix("x rotation:", line)) {
			if (!parse_float_field(&rot[nrots], filename, line, linecount)) {
				axis[nrots] = 'x';
				if (nrots < 3)
					nrots++;
			}
		} else if (has_prefix("y rotation:", line)) {
			if (!parse_float_field(&rot[nrots], filename, line, linecount)) {
				axis[nrots] = 'y';
				if (nrots < 3)
					nrots++;
			}
		} else if (has_prefix("z rotation:", line)) {
			if (!parse_float_field(&rot[nrots], filename, line, linecount)) {
				axis[nrots] = 'z';
				if (nrots < 3)
					nrots++;
			}
		} else if (has_prefix("scale:", line)) {
			if (!parse_float_field(&rot[nrots], filename, line, linecount)) {
				axis[nrots] = 's';
				if (nrots < 3)
					nrots++;
			}
		} else if (has_prefix("}", line)) {
			for (i = 0; i < nrots; i++) {
				if (axis[i] != 'x' && axis[i] != 'y' && axis[i] != 'z' && axis[i] != 's') {
					fprintf(stderr, "Bad axis '%c' at line %d in %s: '%s'\n",
						axis[i], linecount, filename, line);
					axis[i] = 'x';
					rot[i] = 0.0;
				}
				if (axis[i] != 's')
					rot[i] = rot[i] * M_PI / 180.0;
			}

			max_speed = (double) max_speed_100x / 100.0;
			if (toughness < 0.05) {
				fprintf(stderr, "%s:%d: toughness %lf for class %s out of range\n",
					filename, linecount, toughness, class);
				toughness = 0.05;
			}
			if (toughness > 1.0) {
				fprintf(stderr, "%s:%d: toughness %lf for class %s out of range\n",
					filename, linecount, toughness, class);
				toughness = 1.0;
			}

			if (decimation_factor < 1) {
				fprintf(stderr, "%s:%d: decimation factor %d for class %s out of range, using %d\n",
					filename, linecount, decimation_factor, class,
					DEFAULT_SHIP_DECIMATION_FACTOR);
				decimation_factor = DEFAULT_SHIP_DECIMATION_FACTOR;
			}

			/* exceeded allocated capacity */
			if (n >= nalloced) {
				struct ship_type_entry *newst;
				nalloced += 100;
				newst = realloc(st, nalloced * sizeof(*st));
				if (!newst) {
					fprintf(stderr, "out of memory at %s:%d\n", __FILE__, __LINE__);
					*count = n;
					return st;
				}
				st = newst;
			}
			st[n].thrust_attachment_file = strdup(thrust_attach);
			st[n].class = strdup(class);
			st[n].rts_unit_type = -1; /* unknown */
			if (!st[n].class) {
				fprintf(stderr, "out of memory at %s:%d\n", __FILE__, __LINE__);
				*count = n;
				return st;
			}
			st[n].model_file = strdup(model_file);
			if (!st[n].model_file) {
				fprintf(stderr, "out of memory at %s:%d\n", __FILE__, __LINE__);
				*count = n;
				return st;
			}
			st[n].toughness = toughness;
			st[n].max_shield_strength =
				(uint8_t) (255.0f * ((st[n].toughness * 0.8f) + 0.2f));
			st[n].max_speed = max_speed;
			st[n].warpchance = warpchance;
			st[n].crew_max = crew_max;
			st[n].ncargo_bays = ncargo_bays;
			st[n].has_lasers = has_lasers;
			st[n].has_torpedoes = has_torpedoes;
			st[n].has_missiles = has_missiles;
			st[n].extra_scaling = extra_scaling;
			st[n].relative_mass = relative_mass;
			st[n].mass_kg = relative_mass * 330000.0; /* max takeoff weight of 747 in kg */
			st[n].scrap_value = scrap_value;
			st[n].decimation_factor = decimation_factor;
			st[n].min_tris = min_tris;
			errorcount += finish_hard_point(&hps, SHIP_MESH_SCALE * extra_scaling,
						&st[n].hard_point, class, filename, linecount);

			st[n].nrotations = nrots;
			for (i = 0; i < nrots; i++) {
				st[n].axis[i] = axis[i];
				st[n].angle[i] = rot[i];
			}
			/* TODO: manufacturer is an index into the corporations in corporations.c
			 * We should probably read this from the file, and the corporations should
			 * probably also be read from a file. For now, we'll just assign them.
			 */
			st[n].manufacturer = (n % num_spacecraft_manufacturers()) + 1;
			n++;
		} else {
			fprintf(stderr, "Error at line %d in %s: '%s'\n", linecount, filename, line);
		}
	}
	if (in_hard_point)
		fprintf(stderr, "%s:%d: unterminated hard point block at end of file\n",
			filename, linecount);
	*count = n;
	fclose(f);
	return st;
}

void snis_free_ship_type(struct ship_type_entry *st, int count)
{
	int i;

	for (i = 0; i < count; i++)
		free(st[i].class);
	free(st);
}

void setup_rts_unit_type_to_ship_type_map(struct ship_type_entry *st, int count)
{
	int i, j;

	for (i = 0; i < NUM_RTS_UNIT_TYPES; i++)
		for (j = 0; j < count; j++)
			if (strcmp(st[j].class, rts_unit_type(i)->class) == 0) {
				set_rts_unit_type_to_ship_type(i, j);
				st[j].rts_unit_type = i;
				break;
			}
}

#ifdef TEST_SNIS_SHIP_TYPE

int main(int argc, char *argv[])
{
	struct ship_type_entry *ste;
	char *filename = argc > 1 ? argv[1] : "share/snis/ship_types.txt";
	int count = 0;

	ste = snis_read_ship_types(filename, &count);
	if (!ste) {
		fprintf(stderr, "snis_read_ship_types(\"%s\") returned NULL!\n", filename);
		exit(1);
	}
	fprintf(stderr, "Read %d ship types\n", count);
	for (int i = 0; i < count; i++) {
		struct hard_point *hp = &ste[i].hard_point;

		if (!hp->present)
			continue;
		fprintf(stderr, "%s hard point: pos (%g, %g, %g) forward (%g, %g, %g) up (%g, %g, %g)\n",
			ste[i].class, hp->pos.v.x, hp->pos.v.y, hp->pos.v.z,
			hp->forward.v.x, hp->forward.v.y, hp->forward.v.z,
			hp->up.v.x, hp->up.v.y, hp->up.v.z);
		fprintf(stderr, "    elevation %g to %g, azimuth %g to %g degrees\n",
			hp->elevation_lower_limit * 180.0 / M_PI,
			hp->elevation_upper_limit * 180.0 / M_PI,
			hp->azimuth_lower_limit * 180.0 / M_PI,
			hp->azimuth_upper_limit * 180.0 / M_PI);
	}
	for (int i = 0; i < count; i++) {
		if (ste[i].decimation_factor < 1 || ste[i].min_tris < 1) {
			fprintf(stderr, "%s: invalid decimation_factor=%d or min_tris=%d\n",
				ste[i].class, ste[i].decimation_factor, ste[i].min_tris);
			exit(1);
		}
	}
	snis_free_ship_type(ste, count);

	/* Verify default and custom decimation_factor / min_tris and no bleed across entries */
	{
		char tmpname[] = "/tmp/test_ship_types_XXXXXX";
		int fd = mkstemp(tmpname);
		FILE *tf;

		if (fd < 0) {
			perror("mkstemp");
			exit(1);
		}
		tf = fdopen(fd, "w");
		if (!tf) {
			perror("fdopen");
			close(fd);
			unlink(tmpname);
			exit(1);
		}
		fprintf(tf,
			"ship type {\n"
			"	class: TEST_CUSTOM\n"
			"	model file: test.stl\n"
			"	toughness: 0.5\n"
			"	decimation factor: 4\n"
			"	min tris: 128\n"
			"}\n"
			"ship type {\n"
			"	class: TEST_DEFAULT\n"
			"	model file: test2.stl\n"
			"	toughness: 0.5\n"
			"}\n");
		fclose(tf);
		ste = snis_read_ship_types(tmpname, &count);
		unlink(tmpname);
		if (!ste || count != 2) {
			fprintf(stderr, "Expected 2 test ship types, got %d\n", count);
			exit(1);
		}
		if (ste[0].decimation_factor != 4 || ste[0].min_tris != 128) {
			fprintf(stderr, "TEST_CUSTOM mismatch: decimation_factor=%d min_tris=%d\n",
				ste[0].decimation_factor, ste[0].min_tris);
			exit(1);
		}
		if (ste[1].decimation_factor != DEFAULT_SHIP_DECIMATION_FACTOR ||
		    ste[1].min_tris != DEFAULT_SHIP_MIN_TRIS) {
			fprintf(stderr, "TEST_DEFAULT mismatch: decimation_factor=%d min_tris=%d\n",
				ste[1].decimation_factor, ste[1].min_tris);
			exit(1);
		}
		snis_free_ship_type(ste, count);
	}
	return 0;
}
#endif

