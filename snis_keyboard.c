#include <stdlib.h>
#include <string.h>
#include <gdk/gdkevents.h>
#include <gdk/gdkkeysyms.h>

#include "arraysize.h"
#include "snis_keyboard.h"
#include "build_bug_on.h"
#include "string-utils.h"


struct keyname_value_entry keyname_value_map[] = {
	{ "a", GDK_a },
	{ "b", GDK_b },
	{ "c", GDK_c },
	{ "d", GDK_d },
	{ "e", GDK_e },
	{ "f", GDK_f },
	{ "g", GDK_g },
	{ "h", GDK_h },
	{ "i", GDK_i },
	{ "j", GDK_j },
	{ "k", GDK_k },
	{ "l", GDK_l },
	{ "m", GDK_m },
	{ "n", GDK_n },
	{ "o", GDK_o },
	{ "p", GDK_p },
	{ "q", GDK_q },
	{ "r", GDK_r },
	{ "s", GDK_s },
	{ "t", GDK_t },
	{ "u", GDK_u },
	{ "v", GDK_v },
	{ "w", GDK_w },
	{ "x", GDK_x },
	{ "y", GDK_y },
	{ "z", GDK_z },
	{ "A", GDK_A },
	{ "B", GDK_B },
	{ "C", GDK_C },
	{ "D", GDK_D },
	{ "E", GDK_E },
	{ "F", GDK_F },
	{ "G", GDK_G },
	{ "H", GDK_H },
	{ "I", GDK_I },
	{ "J", GDK_J },
	{ "K", GDK_K },
	{ "L", GDK_L },
	{ "M", GDK_M },
	{ "N", GDK_N },
	{ "O", GDK_O },
	{ "P", GDK_P },
	{ "Q", GDK_Q },
	{ "R", GDK_R },
	{ "S", GDK_S },
	{ "T", GDK_T },
	{ "U", GDK_U },
	{ "V", GDK_V },
	{ "W", GDK_W },
	{ "X", GDK_X },
	{ "Y", GDK_Y },
	{ "Z", GDK_Z },
	{ "0", GDK_0 },
	{ "1", GDK_1 },
	{ "2", GDK_2 },
	{ "3", GDK_3 },
	{ "4", GDK_4 },
	{ "5", GDK_5 },
	{ "6", GDK_6 },
	{ "7", GDK_7 },
	{ "8", GDK_8 },
	{ "9", GDK_9 },
	{ "-", GDK_minus },
	{ "+", GDK_plus },
	{ "=", GDK_equal },
	{ "?", GDK_question },
	{ ".", GDK_period },
	{ ",", GDK_comma },
	{ "<", GDK_less },
	{ ">", GDK_greater },
	{ ":", GDK_colon },
	{ ";", GDK_semicolon },
	{ "@", GDK_at },
	{ "*", GDK_asterisk },
	{ "$", GDK_dollar },
	{ "%", GDK_percent },
	{ "&", GDK_ampersand },
	{ "'", GDK_apostrophe },
	{ "(", GDK_parenleft },
	{ ")", GDK_parenright },
	{ "space", GDK_space },
	{ "enter", GDK_Return },
	{ "return", GDK_Return },
	{ "backspace", GDK_BackSpace },
	{ "delete", GDK_Delete },
	{ "pause", GDK_Pause },
	{ "scrolllock", GDK_Scroll_Lock },
	{ "escape", GDK_Escape },
	{ "sysreq", GDK_Sys_Req },
	{ "left", GDK_Left },
	{ "right", GDK_Right },
	{ "up", GDK_Up },
	{ "down", GDK_Down },
	{ "kp_home", GDK_KP_Home },
	{ "kp_down", GDK_KP_Down },
	{ "kp_up", GDK_KP_Up },
	{ "kp_left", GDK_KP_Left },
	{ "kp_right", GDK_KP_Right },
	{ "kp_end", GDK_KP_End },
	{ "kp_delete", GDK_KP_Delete },
	{ "kp_insert", GDK_KP_Insert },
	{ "home", GDK_Home },
	{ "down", GDK_Down },
	{ "up", GDK_Up },
	{ "left", GDK_Left },
	{ "right", GDK_Right },
	{ "end", GDK_End },
	{ "delete", GDK_Delete },
	{ "insert", GDK_Insert },
	{ "kp_0", GDK_KP_0 },
	{ "kp_1", GDK_KP_1 },
	{ "kp_2", GDK_KP_2 },
	{ "kp_3", GDK_KP_3 },
	{ "kp_4", GDK_KP_4 },
	{ "kp_5", GDK_KP_5 },
	{ "kp_6", GDK_KP_6 },
	{ "kp_7", GDK_KP_7 },
	{ "kp_8", GDK_KP_8 },
	{ "kp_9", GDK_KP_9 },
	{ "f1", GDK_F1 },
	{ "f2", GDK_F2 },
	{ "f3", GDK_F3 },
	{ "f4", GDK_F4 },
	{ "f5", GDK_F5 },
	{ "f6", GDK_F6 },
	{ "f7", GDK_F7 },
	{ "f8", GDK_F8 },
	{ "f9", GDK_F9 },
	{ "f10", GDK_F10 },
	{ "f11", GDK_F11 },
	{ "f12", GDK_F12 },
};

enum keyaction keymap[256];
enum keyaction ffkeymap[256];
unsigned char *keycharmap[256];
struct keyboard_state kbstate = { {0} };

char *keyactionstring[] = {
	"none", "down", "up", "left", "right",
	"torpedo", "transform", "fullscreen", "thrust",
	"quit", "pause", "reverse",
	"mainscreen", "navigation", "weapons", "science",
	"damage", "debug", "demon", "f8", "f9", "f10",
	"onscreen", "viewmode", "zoom", "unzoom", "phaser",
	"rendermode", "keyrollleft", "keyrollright",
	"keysciball_yawleft",
	"keysciball_yawright",
	"keysciball_pitchup",
	"keysciball_pitchdown",
	"keysciball_rollleft",
	"keysciball_rollright",
	"key_invert_vertical",
	"key_toggle_frame_stats",
	"key_camera_mode",
	"key_page_up",
	"key_page_down",
	"key_toggle_space_dust",
	"key_toggle_credits",
};

void init_keymap()
{
	memset(keymap, 0, sizeof(keymap));
	memset(ffkeymap, 0, sizeof(ffkeymap));
	memset(keycharmap, 0, sizeof(keycharmap));

	keymap[GDK_j] = keydown;
	ffkeymap[GDK_Down & 0x00ff] = keydown;

	keymap[GDK_k] = keyup;
	ffkeymap[GDK_Up & 0x00ff] = keyup;

	keymap[GDK_l] = keyright;
	ffkeymap[GDK_Right & 0x00ff] = keyright;
	keymap[GDK_period] = keyright;
	keymap[GDK_greater] = keyright;

	keymap[GDK_h] = keyleft;
	ffkeymap[GDK_Left & 0x00ff] = keyleft;
	keymap[GDK_comma] = keyleft;
	keymap[GDK_less] = keyleft;
	keymap[GDK_q] = keyrollleft;
	keymap[GDK_e] = keyrollright;
	keymap[GDK_c] = key_toggle_credits;

	keymap[GDK_space] = keyphaser;
	keymap[GDK_z] = keytorpedo;

	keymap[GDK_b] = keytransform;
	keymap[GDK_x] = keythrust;
	keymap[GDK_r] = keyrenderswitch;
	ffkeymap[GDK_F1 & 0x00ff] = keypausehelp;
	ffkeymap[GDK_Escape & 0x00ff] = keyquit;

	keymap[GDK_O] = keyonscreen;
	keymap[GDK_o] = keyonscreen;
	keymap[GDK_w] = keyup;
	keymap[GDK_a] = keyleft;
	keymap[GDK_s] = keydown;
	keymap[GDK_d] = keyright;

	keymap[GDK_i] = key_invert_vertical;
	ffkeymap[GDK_KEY_Pause & 0x00ff] = key_toggle_frame_stats;
	keymap[GDK_f] = key_toggle_space_dust;

	keymap[GDK_k] = keysciball_rollleft;
	keymap[GDK_semicolon] = keysciball_rollright;
	keymap[GDK_comma] = keysciball_yawleft;
	keymap[GDK_slash] = keysciball_yawright;
	keymap[GDK_l] = keysciball_pitchdown;
	keymap[GDK_period] = keysciball_pitchup;
	keymap[GDK_1] = key_camera_mode;

	keymap[GDK_W] = keyviewmode;
	keymap[GDK_KEY_plus] = keyzoom;
	keymap[GDK_KEY_equal] = keyzoom;
	keymap[GDK_KEY_minus] = keyunzoom;
	ffkeymap[GDK_KEY_KP_Add & 0x00ff] = keyzoom;
	ffkeymap[GDK_KEY_KP_Subtract & 0x00ff] = keyunzoom;

	ffkeymap[GDK_F1 & 0x00ff] = keyf1;
	ffkeymap[GDK_F2 & 0x00ff] = keyf2;
	ffkeymap[GDK_F3 & 0x00ff] = keyf3;
	ffkeymap[GDK_F4 & 0x00ff] = keyf4;
	ffkeymap[GDK_F5 & 0x00ff] = keyf5;
	ffkeymap[GDK_F6 & 0x00ff] = keyf6;
	ffkeymap[GDK_F7 & 0x00ff] = keyf7;
	ffkeymap[GDK_F8 & 0x00ff] = keyf8;
	ffkeymap[GDK_F9 & 0x00ff] = keyf9;
	ffkeymap[GDK_F10 & 0x00ff] = keyf10;

	ffkeymap[GDK_F11 & 0x00ff] = keyfullscreen;

	ffkeymap[GDK_Page_Down & 0x00ff] = key_page_down;
	ffkeymap[GDK_Page_Up & 0x00ff] = key_page_up;
}

int remapkey(char *keyname, char *actionname)
{
	enum keyaction i;
	unsigned int j;
	int index;

	BUILD_ASSERT(ARRAYSIZE(keyactionstring) == NKEYSTATES);

	for (i = keynone; i <= NKEYSTATES; i++) {
		if (strcmp(keyactionstring[i], actionname) != 0)
			continue;

		for (j = 0; j < ARRAYSIZE(keyname_value_map); j++) {
			if (strcmp(keyname_value_map[j].name, keyname) == 0) {
				if ((keyname_value_map[j].value & 0xff00) != 0) {
					index = keyname_value_map[j].value & 0x00ff;
					ffkeymap[index] = i;
				} else
					keymap[keyname_value_map[j].value] = i;
				return 0;
			}
		}
	}
	return 1;
}

void read_keymap_config_file(void)
{
	FILE *f;
	char line[256];
	char *s, *homedir;
	char filename[PATH_MAX], keyname[256], actionname[256];
	int lineno, rc;

	homedir = getenv("HOME");
	if (homedir == NULL)
		return;

	sprintf(filename, "%s/.space-nerds-in-space/snis-keymap.txt", homedir);
	f = fopen(filename, "r");
	if (!f)
		return;

	lineno = 0;
	while (!feof(f)) {
		s = fgets(line, 256, f);
		if (!s)
			break;
		s = trim_whitespace(s);
		lineno++;
		if (strcmp(s, "") == 0)
			continue;
		if (s[0] == '#') /* comment? */
			continue;
		rc = sscanf(s, "map %s %s", keyname, actionname);
		if (rc == 2) {
			if (remapkey(keyname, actionname) == 0)
				continue;
		}
		fprintf(stderr, "%s: syntax error at line %d:'%s'\n",
			filename, lineno, line);
	}
}
