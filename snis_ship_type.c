#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <limits.h>

#include "snis_ship_type.h"

struct ship_type_entry *snis_read_ship_types(char *filename, int *count)
{
	FILE *f;
	char line[255], class[255], model_file[PATH_MAX];
	double max_speed;
	int crew_max;
	char *x;
	int scancount;
	int integer;
	int linecount = 0, n = 0;
	int nalloced = 0;
	struct ship_type_entry *st;

	nalloced = 20;
	st = malloc(sizeof(*st) * nalloced);
	if (!st)
		return NULL;

	f = fopen(filename, "r");
	if (!f)
		return NULL;

	while (!feof(f)) {
		x = fgets(line, sizeof(line) - 1, f);
		line[strlen(line) - 1] = '\0'; /* cut off trailing \n */
		if (!x) {
			if (feof(f))
				break;
		}
		linecount++;

		if (line[0] == '#') /* skip comment lines */
			continue;

		scancount = sscanf(line, "%s %s %d %d\n", class, model_file, &integer, &crew_max);
		if (scancount != 4) {
			fprintf(stderr, "Error at line %d in %s: '%s'\n",
				linecount, filename, line);
			if (scancount > 0)
				fprintf(stderr, "converted %d items\n", scancount);
			continue;
		}
		max_speed = (double) integer / 100.0;

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
		}
		st[n].class = strdup(class);
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
		st[n].max_speed = max_speed;
		st[n].crew_max = crew_max;
		n++;
	}
	*count = n;
	return st;
}

void snis_free_ship_type(struct ship_type_entry *st, int count)
{
	int i;

	for (i = 0; i < count; i++)
		free(st[i].class);
	free(st);
}

