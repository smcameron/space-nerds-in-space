#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#define DEFINE_FACTION_GLOBALS 1
#include "snis_faction.h"
#undef DEFINE_FACTION_GLOBALS

#define MAX_FACTIONS 20

int snis_read_factions(char *filename)
{
	FILE *f;
	int n = 0;
	char line[256];
	int linecount = 0;
	char *x;

	nfactions = 0;
	f = fopen(filename, "r");
	if (!f)
		return -1;

	faction = malloc(MAX_FACTIONS * sizeof(*faction));
	if (!faction) {
		fprintf(stderr, "out of memory at %s:%d\n", __FILE__, __LINE__);
		return -1;
	}

	while (!feof(f) && n < MAX_FACTIONS) {
		x = fgets(line, sizeof(line) - 1, f);
		if (!x)
			break;
		linecount++;
		faction[n] = malloc(strlen(line) + 1);
		if (!faction[n]) {
			fprintf(stderr, "out of memory at %s:%d\n", filename, linecount);
			return -1;
		}
		strcpy(faction[n], line);
		n++;
	}
	nfactions = n;
	return 0;
}

void snis_free_factions(void)
{
	int i;

	for (i = 0; i < nfactions; i++)
		free(faction[i]);
}


