#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#define DEFINE_FACTION_GLOBALS 1
#include "quat.h"
#include "snis_faction.h"

#undef DEFINE_FACTION_GLOBALS

#define MAX_FACTIONS 20

struct faction {
	char *name;
	union vec3 center;
};

static struct faction *fact;
static int nfacts;

int nfactions(void)
{
	return nfacts;
}

int snis_read_factions(char *filename)
{
	FILE *f;
	int r, n = 0;
	char line[256];
	int linecount = 0;
	int32_t x, y, z;
	char *c;

	nfacts = 0;
	f = fopen(filename, "r");
	if (!f)
		return -1;

	fact = malloc(MAX_FACTIONS * sizeof(*fact));
	if (!fact) {
		fprintf(stderr, "out of memory at %s:%d\n", __FILE__, __LINE__);
		return -1;
	}

	while (!feof(f) && n < MAX_FACTIONS) {
		c = fgets(line, sizeof(line) - 1, f);
		if (!c)
			break;
		linecount++;

		if (line[0] == '#') /* skip comments */
			continue;

		fact[n].name = malloc(strlen(line) + 1);
		if (!fact[n].name) {
			fprintf(stderr, "out of memory at %s:%d\n", filename, linecount);
			return -1;
		}
		strcpy(fact[n].name, line);
		c = fgets(line, sizeof(line) - 1, f);
		if (!c)
			break;
		r = sscanf(line, "%d %d %d", &x, &y, &z);
		if (r != 3) {
			fprintf(stderr, "bad line '%s' at %s:%d\n", line, filename, linecount);
			return -1;
		}
		linecount++;
		fact[n].center.v.x = (float) x;
		fact[n].center.v.y = (float) y;
		fact[n].center.v.z = (float) z;
		n++;
	}
	nfacts = n;
	return 0;
}

void snis_free_factions(void)
{
	int i;

	for (i = 0; i < nfacts; i++)
		free(fact[i].name);
	free(fact);
}

char *faction_name(int faction_number)
{
	return fact[faction_number].name;
}

union vec3 faction_center(int faction_number)
{
	return fact[faction_number].center;
}

int nearest_faction(union vec3 v)
{
	int i;
	double len;
	int winner;
	union vec3 distv;

	for (i = 0; i < nfactions(); i++) {
		double dist;

		vec3_sub(&distv, &v, &fact[i].center);
		dist = vec3_len2(&distv);
		if (i == 0 || dist < len)  {
			len = dist;
			winner = i;
		}
	}
	return winner;
}

