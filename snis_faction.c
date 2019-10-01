#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>

#include "mtwist.h"
#include "quat.h"
#define DEFINE_FACTION_GLOBALS 1
#include "snis_faction.h"
#include "string-utils.h"

#undef DEFINE_FACTION_GLOBALS

struct faction {
	char *name;
	union vec3 center;
	float hostility[MAX_FACTIONS]; /* Range 0.0 to 1.0. 0.0 == good friends, 1.0 == terrible foes */
#define DEFAULT_FACTION_HOSTILITY 0.03f
};

static struct faction *fact;
static int nfacts;

int nfactions(void)
{
	return nfacts;
}

static int lookup_faction(char *name)
{
	int i;

	for (i = 0; i < nfacts; i++)
		if (strcmp(name, fact[i].name) == 0)
			return i;
	return -1;
}

static int process_hostility(char *line)
{
	int n;
	char a[255], b[255];
	int hostility;
	int f1, f2;

	n = sscanf(line, "hostility %[A-Za-z] %[A-Za-z] %d", a, b, &hostility);
	if (n != 3)
		return -1;
	uppercase(a);
	uppercase(b);
	f1 = lookup_faction(a);
	if (f1 < 0) {
		fprintf(stderr, "Bad faction '%s'\n", a);
		return -1;
	}
	f2 = lookup_faction(b);
	if (f2 < 0) {
		fprintf(stderr, "Bad faction '%s'\n", b);
		return -1;
	}
	fact[f1].hostility[f2] = (float) hostility / 100.0f;
	fact[f2].hostility[f1] = (float) hostility / 100.0f;
	return 0;
}

static void initialize_faction_hostility()
{
	int i, j;

	for (i = 0; i < MAX_FACTIONS; i++)
		for (j = 0; j < MAX_FACTIONS; j++)
			if (i == j)
				fact[i].hostility[j] = 0;
			else
				fact[i].hostility[j] = DEFAULT_FACTION_HOSTILITY;
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
		fclose(f);
		return -1;
	}

	initialize_faction_hostility();

	while (!feof(f) && n < MAX_FACTIONS) {
		c = fgets(line, sizeof(line) - 1, f);
		if (!c)
			break;
		if (strlen(line) == 0)
			continue;
		line[strlen(line) - 1] = '\0';
		linecount++;

		if (line[0] == '#') /* skip comments */
			continue;
	
		if (strncmp(line, "hostility ", 10) == 0) {
			if (process_hostility(line)) {
				fprintf(stderr, "bad faction hostility %s:%d\n",
					filename, linecount);
				return -1;
			}
			continue;
		}

		fact[n].name = malloc(strlen(line) + 1);
		if (!fact[n].name) {
			fprintf(stderr, "out of memory at %s:%d\n", filename, linecount);
			fclose(f);
			return -1;
		}
		strcpy(fact[n].name, line);
		uppercase(fact[n].name);
		c = fgets(line, sizeof(line) - 1, f);
		if (!c)
			break;
		if (strlen(line) == 0)
			continue;
		line[strlen(line) - 1] = '\0';
		r = sscanf(line, "%d %d %d", &x, &y, &z);
		if (r != 3) {
			fprintf(stderr, "bad line '%s' at %s:%d\n", line, filename, linecount);
			fclose(f);
			return -1;
		}
		linecount++;
		fact[n].center.v.x = (float) x;
		fact[n].center.v.y = (float) y;
		fact[n].center.v.z = (float) z;
		nfacts++;
		n++;
	}
	fclose(f);
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
	int winner = 0;
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

float faction_hostility(int f1, int f2)
{
	if (f1 < 0 || f2 < 0 || f1 >= nfacts || f2 >= nfacts)
		return 0.0f;
	return fact[f1].hostility[f2];
}

