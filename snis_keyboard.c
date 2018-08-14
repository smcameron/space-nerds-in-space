#include <stdlib.h>
#include <string.h>
#include <gdk/gdkevents.h>
#include <gdk/gdkkeysyms.h>

#include "arraysize.h"
#include "snis_keyboard.h"
#include "build_bug_on.h"
#include "string-utils.h"

const char *displaymode_name[] = {
	"mainscreen",
	"navigation",
	"weapons",
	"engineering",
	"science",
	"comms",
	"demon",
	"damcon",
	"fonttest",
	"introscreen",
	"lobbyscreen",
	"connecting",
	"connected",
	"network_setup",
};

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
	{ "kp_pgdn", GDK_KP_Page_Down },
	{ "kp_pgup", GDK_KP_Page_Up },
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
	{ "tilde", GDK_KEY_asciitilde },
	{ "quoteright", GDK_KEY_quoteright },
	{ "quoteleft", GDK_KEY_quoteleft },
};

enum keyaction keymap[DISPLAYMODE_COUNT][256];
enum keyaction ffkeymap[DISPLAYMODE_COUNT][256];
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
	"key_toggle_watermark",
	"key_mouse_mode",
	"key_sci_mining_bot",
	"key_sci_tractor_beam",
	"key_sci_waypoints",
	"key_sci_lrs",
	"key_sci_srs",
	"key_sci_details",
	"key_weap_fire_missile",
};

void zero_keymaps(void)
{
	memset(keymap, 0, sizeof(keymap));
	memset(ffkeymap, 0, sizeof(ffkeymap));
}

static void mapkey(int displaymodes, unsigned int keysym, enum keyaction key)
{
	int i;

	for (i = 0; i < DISPLAYMODE_COUNT; i++) {
		if (displaymodes & (0x1 << i))
			keymap[i][keysym] = key; /* map key on this station */
		else
			continue;
	}
}


static void ffmapkey(int displaymodes, unsigned int keysym, enum keyaction key)
{
	int i;

	for (i = 0; i < DISPLAYMODE_COUNT; i++) {
		if (displaymodes & (0x1 << i))
			ffkeymap[i][keysym & 0x00ff] = key; /* map key on this station */
		else
			continue;
	}
}


void init_keymap(void)
{

	const unsigned short all = 0x03fff; /* all 14 displaymodes */
	zero_keymaps();

	mapkey(all, GDK_j, keydown);
	ffmapkey(all, GDK_Down, keydown);

	mapkey(all, GDK_k, keyup);
	ffmapkey(all, GDK_Up, keyup);

	mapkey(all, GDK_l, keyright);
	ffmapkey(all, GDK_Right, keyright);
	mapkey(all, GDK_period, keyright);
	mapkey(all, GDK_greater, keyright);

	mapkey(all, GDK_h, keyleft);
	ffmapkey(all, GDK_Left, keyleft);
	mapkey(all, GDK_comma, keyleft);
	mapkey(all, GDK_less, keyleft);
	mapkey(all, GDK_q, keyrollleft);
	mapkey(all, GDK_e, keyrollright);
	mapkey(all, GDK_c, key_toggle_credits);
	mapkey(all, GDK_M, key_toggle_watermark);
	mapkey(all, GDK_m, key_mouse_mode);	/* FIXME: Note this is overridden by key_sci_mining_bot, below */
						/* Proper fix is probably something like make keymap into a 2D array */
						/* with displaymode as first index, key as second index. */
	mapkey(all, GDK_n, key_weap_fire_missile);
	mapkey(all, GDK_space, keyphaser);
	mapkey(all, GDK_z, keytorpedo);

	mapkey(all, GDK_b, keytransform);
	mapkey(all, GDK_x, keythrust);
	mapkey(all, GDK_r, keyrenderswitch);

	ffmapkey(all, GDK_F1, keypausehelp);
	ffmapkey(all, GDK_Escape, keyquit);

	mapkey(all, GDK_O, keyonscreen);
	mapkey(all, GDK_o, keyonscreen);
	mapkey(all, GDK_w, keyup);
	mapkey(all, GDK_a, keyleft);
	mapkey(all, GDK_s, keydown);
	mapkey(all, GDK_d, keyright);
	ffmapkey(all, GDK_KP_Up, keyup);
	ffmapkey(all, GDK_KP_Down, keydown);
	ffmapkey(all, GDK_KP_Left, keyleft);
	ffmapkey(all, GDK_KP_Right, keyright);
	ffmapkey(all, GDK_KP_Home, keyrollleft);
	ffmapkey(all, GDK_KP_Page_Up, keyrollright);

	mapkey(all, GDK_i, key_invert_vertical);
	ffmapkey(all, GDK_KEY_Pause, key_toggle_frame_stats);
	mapkey(all, GDK_f, key_toggle_space_dust);

	mapkey(all, GDK_k, keysciball_rollleft);
	mapkey(all, GDK_semicolon, keysciball_rollright);
	mapkey(all, GDK_comma, keysciball_yawleft);
	mapkey(all, GDK_slash, keysciball_yawright);
	mapkey(all, GDK_l, keysciball_pitchdown);
	mapkey(all, GDK_period, keysciball_pitchup);
	mapkey(all, GDK_KEY_quoteleft, key_camera_mode);

	mapkey(all, GDK_W, keyviewmode);
	mapkey(all, GDK_KEY_plus, keyzoom);
	mapkey(all, GDK_KEY_equal, keyzoom);
	mapkey(all, GDK_KEY_minus, keyunzoom);
	mapkey(all, GDK_KEY_m, key_sci_mining_bot);
	mapkey(all, GDK_KEY_t, key_sci_tractor_beam);

	/* keymap[GDK_KEY_l] = key_sci_lrs; */ /* <-- this interferes with keysciball_pitchdown, above */

	/* NOTE: The following 3 keys interfere with AWSD on Nav, Weapons, and Mainscreen.
	 * The real fix is to have per-screen keybindings, not game-wide keybindings.
	 */

	/* keymap[GDK_KEY_s] = key_sci_srs; */
	/* keymap[GDK_KEY_w] = key_sci_waypoints; */
	/* keymap[GDK_KEY_d] = key_sci_details; */

	ffmapkey(all, GDK_KEY_KP_Add, keyzoom);
	ffmapkey(all, GDK_KEY_KP_Subtract, keyunzoom);
	ffmapkey(all, GDK_F1, keyf1);
	ffmapkey(all, GDK_F2, keyf2);
	ffmapkey(all, GDK_F3, keyf3);
	ffmapkey(all, GDK_F4, keyf4);
	ffmapkey(all, GDK_F5, keyf5);
	ffmapkey(all, GDK_F6, keyf6);
	ffmapkey(all, GDK_F7, keyf7);
	ffmapkey(all, GDK_F8, keyf8);
	ffmapkey(all, GDK_F9, keyf9);
	ffmapkey(all, GDK_F10, keyf10);

	ffmapkey(all, GDK_F11, keyfullscreen);
	ffmapkey(all, GDK_Page_Down, key_page_down);
	ffmapkey(all, GDK_Page_Up, key_page_up);
}

int remapkey(char *stations, char *keyname, char *actionname)
{
	enum keyaction i;
	unsigned int j;
	int index;
	int displaymodes = 0;

	BUILD_ASSERT(ARRAYSIZE(keyactionstring) == NKEYSTATES);

	if (strcmp(stations, "all") == 0) {
		displaymodes = 0x3fff;
	} else {
		for (i = 0; i < ARRAYSIZE(displaymode_name); i++) {
			char *x = strstr(stations, displaymode_name[i]);
			if (!x)
				continue;
			displaymodes |= (1 << i);
		}
	}

	for (i = keynone; i <= NKEYSTATES; i++) {
		if (strcmp(keyactionstring[i], actionname) != 0)
			continue;

		for (j = 0; j < ARRAYSIZE(keyname_value_map); j++) {
			if (strcmp(keyname_value_map[j].name, keyname) == 0) {
				if ((keyname_value_map[j].value & 0xff00) != 0) {
					index = keyname_value_map[j].value & 0x00ff;
					ffmapkey(displaymodes, index, i);
				} else
					mapkey(displaymodes, keyname_value_map[j].value, i);
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
	char filename[PATH_MAX], keyname[256], actionname[256], stations[256];
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
		rc = sscanf(s, "map %s %s %s", stations, keyname, actionname);
		if (rc == 2) {
			if (remapkey(stations, keyname, actionname) == 0)
				continue;
		}
		fprintf(stderr, "%s: syntax error at line %d:'%s'\n",
			filename, lineno, line);
	}
	fclose(f);
}
