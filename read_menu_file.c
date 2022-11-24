#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "string-utils.h"
#include "read_menu_file.h"

/* Read share/snis/luascripts/MISSSIONS/missions_menu.txt
 * Each line contains three fields separated by a comma.
 * The first field is the menu text, and the
 * The second field is the lua script to run.
 * The third optional field is the tooltip text for the menu item.
 */
struct menu_text *read_menu_file(char *menu_file)
{
	char line[256];
	char menu_text[256];
	char script[256];
	char tooltip[256];
	char *l;
	FILE *f;
	int rc;

	struct menu_text *mm = malloc(sizeof(*mm));
	mm->count = 0;
	memset(mm, 0, sizeof(*mm));

	f = fopen(menu_file, "r");
	if (!f) {
		free(mm);
		return NULL;
	}
	do {
		l = fgets(line, 255, f);
		if (!l)
			break;
		trim_whitespace(line);
		clean_spaces(line);
		if (strcmp(line, "") == 0) /* skip blank lines */
			continue;
		if (line[0] == '#')
			continue; /* skip comments */
		rc = sscanf(line, "%[^,]%*[, ]%[^,]%*[, ]%[^,\n]", menu_text, script, tooltip);
		if (rc != 3) {
			rc = sscanf(line, "%[^,]%*[, ]%[^,\n]", menu_text, script);
			if (rc != 2) {
				fprintf(stderr, "Bad line in %s: %s\n", menu_file, line);
				continue;
			}
		}
		mm->menu_text[mm->count] = strdup(menu_text);
		mm->script[mm->count] = strdup(script);
		if (rc == 3)
			mm->tooltip[mm->count] = strdup(tooltip);
		else
			mm->tooltip[mm->count] = NULL;
		mm->count++;
		if (mm->count == MAX_MISSION_MENU_ITEMS) {
			fprintf(stderr, "menu %s max item count reached at '%s'\n",
				menu_file, line);
			break;
		}
	} while (1);
	fclose(f);
	return mm;
}

void free_menu_text(struct menu_text *mt)
{
	int i;

	for (i = 0; i < mt->count; i++) {
		free(mt->tooltip[i]);
		free(mt->menu_text[i]);
		free(mt->script[i]);
	}
	free(mt);
}

