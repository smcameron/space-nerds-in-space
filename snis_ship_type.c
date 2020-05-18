#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdint.h>

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

	nalloced = 30;
	st = malloc(sizeof(*st) * nalloced);
	if (!st)
		return NULL;

	f = fopen(filename, "r");
	if (!f)
		return NULL;

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

	while (!feof(f)) {
		x = fgets(line, sizeof(line) - 1, f);
		if (!x) {
			if (feof(f))
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
			rot[0] = 0.0;
			rot[1] = 0.0;
			rot[2] = 0.0;
			rot[3] = 0.0;
			axis[0] = '\0';
			axis[1] = '\0';
			axis[2] = '\0';
			axis[3] = '\0';
			nrots = 0;
			continue;
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
		} else if (has_prefix("x rotation:", line)) {
			if (!parse_float_field(&rot[nrots], filename, line, linecount)) {
				axis[nrots] = 'x';
				if (nrots < 4)
					nrots++;
			}
		} else if (has_prefix("y rotation:", line)) {
			if (!parse_float_field(&rot[nrots], filename, line, linecount)) {
				axis[nrots] = 'y';
				if (nrots < 4)
					nrots++;
			}
		} else if (has_prefix("z rotation:", line)) {
			if (!parse_float_field(&rot[nrots], filename, line, linecount)) {
				axis[nrots] = 'z';
				if (nrots < 4)
					nrots++;
			}
		} else if (has_prefix("scale:", line)) {
			if (!parse_float_field(&rot[nrots], filename, line, linecount)) {
				axis[nrots] = 's';
				if (nrots < 4)
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
			if (!st[n].class) {
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

static void usage(void)
{
	fprintf(stderr, "Usage: test_snis_ship_types ship_types.txt\n");
	exit(1);
}

int main(int argc, char *argv[])
{
	struct ship_type_entry *ste;
	int count = 0;

	if (argc < 2)
		usage();

	ste = snis_read_ship_types(argv[1], &count);
	if (!ste) {
		fprintf(stderr, "snis_read_ship_types(\"%s\") returned NULL!\n", argv[1]);
		exit(1);
	}
	fprintf(stderr, "Read %d ship types\n", count);
	return 0;
}
#endif

