#ifndef READ_MENU_FILE_H__
#define READ_MENU_FILE_H__

struct menu_text {
#define MAX_MISSION_MENU_ITEMS 50
	int count;
	char *menu_text[MAX_MISSION_MENU_ITEMS], *script[MAX_MISSION_MENU_ITEMS];
	char *tooltip[MAX_MISSION_MENU_ITEMS];
};

/* Read a menu file (e.g. share/snis/luascripts/MISSSIONS/missions_menu.txt)
 * Each line contains three fields separated by a comma.
 * The first field is the menu text, and the
 * The second field is the lua script to run.
 * The third optional field is the tooltip text for the menu item.
 */
struct menu_text *read_menu_file(char *menu_file);

#endif

