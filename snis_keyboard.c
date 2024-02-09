#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <errno.h>
#include <SDL.h>

#include "arraysize.h"
#include "snis_keyboard.h"
#include "build_bug_on.h"
#include "string-utils.h"

#include "snis_packet.h"

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
	{ "a", SDLK_a },
	{ "b", SDLK_b },
	{ "c", SDLK_c },
	{ "d", SDLK_d },
	{ "e", SDLK_e },
	{ "f", SDLK_f },
	{ "g", SDLK_g },
	{ "h", SDLK_h },
	{ "i", SDLK_i },
	{ "j", SDLK_j },
	{ "k", SDLK_k },
	{ "l", SDLK_l },
	{ "m", SDLK_m },
	{ "n", SDLK_n },
	{ "o", SDLK_o },
	{ "p", SDLK_p },
	{ "q", SDLK_q },
	{ "r", SDLK_r },
	{ "s", SDLK_s },
	{ "t", SDLK_t },
	{ "u", SDLK_u },
	{ "v", SDLK_v },
	{ "w", SDLK_w },
	{ "x", SDLK_x },
	{ "y", SDLK_y },
	{ "z", SDLK_z },
	{ "0", SDLK_0 },
	{ "1", SDLK_1 },
	{ "2", SDLK_2 },
	{ "3", SDLK_3 },
	{ "4", SDLK_4 },
	{ "5", SDLK_5 },
	{ "6", SDLK_6 },
	{ "7", SDLK_7 },
	{ "8", SDLK_8 },
	{ "9", SDLK_9 },
	{ "-", SDLK_MINUS },
	{ "+", SDLK_PLUS },
	{ "=", SDLK_EQUALS },
	{ "?", SDLK_QUESTION },
	{ ".", SDLK_PERIOD },
	{ "/", SDLK_SLASH },
	{ ",", SDLK_COMMA },
	{ "<", SDLK_LESS },
	{ ">", SDLK_GREATER },
	{ ":", SDLK_COLON },
	{ ";", SDLK_SEMICOLON },
	{ "@", SDLK_AT },
	{ "*", SDLK_ASTERISK },
	{ "$", SDLK_DOLLAR },
	{ "%", SDLK_PERCENT },
	{ "&", SDLK_AMPERSAND },
	{ "'", SDLK_QUOTE },
	{ "(", SDLK_LEFTPAREN },
	{ ")", SDLK_RIGHTPAREN },
	{ "[", SDLK_LEFTBRACKET },
	{ "]", SDLK_RIGHTBRACKET },
	{ "space", SDLK_SPACE },
	{ "enter", SDLK_RETURN },
	{ "return", SDLK_RETURN },
	{ "backspace", SDLK_BACKSPACE },
	{ "delete", SDLK_DELETE },
	{ "pause", SDLK_PAUSE },
	{ "scrolllock", SDLK_SCROLLLOCK },
	{ "escape", SDLK_ESCAPE },
	{ "sysreq", SDLK_SYSREQ },
	{ "left", SDLK_LEFT },
	{ "right", SDLK_RIGHT },
	{ "up", SDLK_UP },
	{ "down", SDLK_DOWN },
	{ "home", SDLK_HOME },
	{ "end", SDLK_END },
	{ "delete", SDLK_DELETE },
	{ "insert", SDLK_INSERT },
	{ "kp_0", SDLK_KP_0 },
	{ "kp_1", SDLK_KP_1 },
	{ "kp_2", SDLK_KP_2 },
	{ "kp_3", SDLK_KP_3 },
	{ "kp_4", SDLK_KP_4 },
	{ "kp_5", SDLK_KP_5 },
	{ "kp_6", SDLK_KP_6 },
	{ "kp_7", SDLK_KP_7 },
	{ "kp_8", SDLK_KP_8 },
	{ "kp_9", SDLK_KP_9 },
	{ "f1", SDLK_F1 },
	{ "f2", SDLK_F2 },
	{ "f3", SDLK_F3 },
	{ "f4", SDLK_F4 },
	{ "f5", SDLK_F5 },
	{ "f6", SDLK_F6 },
	{ "f7", SDLK_F7 },
	{ "f8", SDLK_F8 },
	{ "f9", SDLK_F9 },
	{ "f10", SDLK_F10 },
	{ "f11", SDLK_F11 },
	{ "f12", SDLK_F12 },
	{ "quoteright", SDLK_QUOTE },
	{ "quoteleft", SDLK_BACKQUOTE },
};

enum keyaction keymap[DISPLAYMODE_COUNT][256];
enum keyaction ffkeymap[DISPLAYMODE_COUNT][256];
struct keyboard_state kbstate = { {0} };

char *keyactionstring[] = {
	"none", "down", "up", "left", "right",
	"torpedo", "key_starmap", "fullscreen", "thrust",
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
	"key_space",
	"key_robot_gripper",
	"key_demon_console",
	"key_toggle_external_camera",
	"key_eng_preset_0",
	"key_eng_preset_1",
	"key_eng_preset_2",
	"key_eng_preset_3",
	"key_eng_preset_4",
	"key_eng_preset_5",
	"key_eng_preset_6",
	"key_eng_preset_7",
	"key_eng_preset_8",
	"key_eng_preset_9",
	"key_left_shift",
	"key_right_shift",
	"key_increase_warp",
	"key_decrease_warp",
	"key_increase_impulse",
	"key_decrease_impulse",
	"key_engage_warp",
	"key_docking_magnets",
	"key_exterior_lights",
	"key_standard_orbit",
	"key_weap_wavelen_0",
	"key_weap_wavelen_1",
	"key_weap_wavelen_2",
	"key_weap_wavelen_3",
	"key_weap_wavelen_4",
	"key_weap_wavelen_5",
	"key_weap_wavelen_6",
	"key_weap_wavelen_7",
	"key_weap_wavelen_8",
	"key_weap_wavelen_9",
	"key_weap_wavlen_nudge_up",
	"key_weap_wavlen_nudge_down",
};

#ifdef DEBUG_KEYMAP
static char *get_keyname(unsigned int keyvalue)
{
	static char tmpbuf[20];
	for (int i = 0; i < (int) ARRAYSIZE(keyname_value_map); i++)
		if (keyname_value_map[i].value == keyvalue)
			return keyname_value_map[i].name;
	snprintf(tmpbuf, sizeof(tmpbuf), "%d?", keyvalue);
	return tmpbuf;
}

static void print_keymap(char *name, enum keyaction keymap[DISPLAYMODE_COUNT][256])
{
	for (int i = 0; i < DISPLAYMODE_COUNT; i++) {
		for (int j = 0; j < 256; j++) {
			if (keymap[i][j] == keynone)
				continue;
			fprintf(stderr, "%s[%s][%s] = %s\n",
				name,
				displaymode_name[i],
				get_keyname(j),
				keyactionstring[keymap[i][j]]);
		}
	}
}
#endif

void zero_keymaps(void)
{
	memset(keymap, 0, sizeof(keymap));
	memset(ffkeymap, 0, sizeof(ffkeymap));
}

static void mapkey(int displaymodes, unsigned int keysym, enum keyaction key)
{
	int i;

	if (keysym >= 256) {
		fprintf(stderr, "We got a problem keysym is %u\n", keysym);
		return;
	}
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
	const unsigned short nav = 1 << DISPLAYMODE_NAVIGATION;
	const unsigned short weap = 1 << DISPLAYMODE_WEAPONS;
	const unsigned short eng = 1 << DISPLAYMODE_ENGINEERING;
	const unsigned short damcon = 1 << DISPLAYMODE_DAMCON;
	const unsigned short sci = 1 << DISPLAYMODE_SCIENCE;
	const unsigned short comms = 1 << DISPLAYMODE_COMMS;
	const unsigned short mainscreen = 1 << DISPLAYMODE_MAINSCREEN;
	const unsigned short demon = 1 << DISPLAYMODE_DEMON;

	zero_keymaps();

	mapkey(all & ~comms, SDLK_j, keydown);
	ffmapkey(all, SDLK_DOWN, keydown);

	mapkey(all & ~comms, SDLK_k, keyup);
	ffmapkey(all, SDLK_UP, keyup);

	mapkey(all, SDLK_l, keyright);
	ffmapkey(all, SDLK_RIGHT, keyright);
	mapkey(all, SDLK_PERIOD, keyright);
	mapkey(all, SDLK_GREATER, keyright);

	mapkey(all, SDLK_h, keyleft);
	ffmapkey(all, SDLK_LEFT, keyleft);
	mapkey(all, SDLK_COMMA, keyleft);
	mapkey(all, SDLK_LESS, keyleft);
	mapkey(nav | mainscreen, SDLK_q, keyrollleft);
	mapkey(nav | mainscreen, SDLK_e, keyrollright);
	mapkey(mainscreen, SDLK_c, key_toggle_credits);
	mapkey(mainscreen, SDLK_x, key_toggle_external_camera);
	mapkey(all, SDLK_m, key_toggle_watermark);
	mapkey(weap, SDLK_m, key_mouse_mode);
	mapkey(weap, SDLK_n, key_weap_fire_missile);
	mapkey(weap, SDLK_0, key_weap_wavelen_0);
	mapkey(weap, SDLK_1, key_weap_wavelen_1);
	mapkey(weap, SDLK_2, key_weap_wavelen_2);
	mapkey(weap, SDLK_3, key_weap_wavelen_3);
	mapkey(weap, SDLK_4, key_weap_wavelen_4);
	mapkey(weap, SDLK_5, key_weap_wavelen_5);
	mapkey(weap, SDLK_6, key_weap_wavelen_6);
	mapkey(weap, SDLK_7, key_weap_wavelen_7);
	mapkey(weap, SDLK_8, key_weap_wavelen_8);
	mapkey(weap, SDLK_9, key_weap_wavelen_9);
	mapkey(weap, SDLK_EQUALS, key_weap_wavelen_nudge_up);
	mapkey(weap, SDLK_MINUS, key_weap_wavelen_nudge_down);
	mapkey(all, SDLK_SPACE, key_space);
	mapkey(weap, SDLK_SPACE, keyphaser);
	mapkey(damcon, SDLK_SPACE, key_robot_gripper);
	mapkey(weap, SDLK_z, keytorpedo);

	mapkey(demon, SDLK_x, keythrust);
	mapkey(mainscreen | weap, SDLK_r, keyrenderswitch);

	ffmapkey(all, SDLK_F1, keypausehelp);
	mapkey(all, SDLK_ESCAPE, keyquit);

	mapkey(all, SDLK_o, keyonscreen);
	mapkey(nav | mainscreen | weap | damcon, SDLK_w, keyup);
	mapkey(nav | mainscreen | weap | damcon, SDLK_a, keyleft);
	mapkey(nav | mainscreen | weap | damcon, SDLK_s, keydown);
	mapkey(nav | mainscreen | weap | damcon, SDLK_d, keyright);
	ffmapkey(nav | mainscreen | weap | damcon, SDLK_UP, keyup);
	ffmapkey(nav | mainscreen | weap | damcon, SDLK_DOWN, keydown);
	ffmapkey(nav | mainscreen | weap | damcon, SDLK_LEFT, keyleft);
	ffmapkey(nav | mainscreen | weap | damcon, SDLK_RIGHT, keyright);
	ffmapkey(nav | mainscreen | weap, SDLK_HOME, keyrollleft);
	ffmapkey(nav | mainscreen | weap, SDLK_PAGEUP, keyrollright);

	mapkey(nav | mainscreen | weap, SDLK_i, key_invert_vertical);
	ffmapkey(all, SDLK_PAUSE, key_toggle_frame_stats);
	mapkey(weap | mainscreen, SDLK_f, key_toggle_space_dust);

	mapkey(sci, SDLK_k, keysciball_rollleft);
	ffmapkey(sci, SDLK_HOME, keysciball_rollleft);
	mapkey(sci, SDLK_SEMICOLON, keysciball_rollright);
	ffmapkey(sci, SDLK_PAGEUP, keysciball_rollright);
	mapkey(sci, SDLK_COMMA, keysciball_yawleft);
	ffmapkey(sci, SDLK_KP_4, keysciball_yawleft);
	mapkey(sci, SDLK_SLASH, keysciball_yawright);
	ffmapkey(sci, SDLK_KP_6, keysciball_yawright);
	mapkey(sci, SDLK_l, keysciball_pitchdown);
	ffmapkey(sci, SDLK_KP_8, keysciball_pitchdown);
	mapkey(sci, SDLK_PERIOD, keysciball_pitchup);
	ffmapkey(sci, SDLK_KP_2, keysciball_pitchup);
	mapkey(nav | mainscreen, SDLK_BACKQUOTE, key_camera_mode);
	mapkey(demon, SDLK_BACKQUOTE, key_demon_console);

	mapkey(nav | mainscreen | weap, SDLK_w, keyviewmode);
	mapkey(nav | mainscreen | sci, SDLK_PLUS, keyzoom);
	mapkey(nav | mainscreen | sci, SDLK_EQUALS, keyzoom);
	mapkey(nav | mainscreen | sci, SDLK_MINUS, keyunzoom);
	mapkey(sci, SDLK_m, key_sci_mining_bot);
	mapkey(sci, SDLK_t, key_sci_tractor_beam);
	mapkey(sci, SDLK_l, key_sci_lrs); /* interferes with pitchdown */
	mapkey(sci, SDLK_s, key_sci_srs);
	mapkey(sci, SDLK_w, key_sci_waypoints);
	mapkey(sci, SDLK_d, key_sci_details);

	ffmapkey(nav | mainscreen | sci, SDLK_PLUS, keyzoom);
	ffmapkey(nav | mainscreen | sci, SDLK_MINUS, keyunzoom);
	ffmapkey(all, SDLK_F1, keyf1);
	ffmapkey(all, SDLK_F2, keyf2);
	ffmapkey(all, SDLK_F3, keyf3);
	ffmapkey(all, SDLK_F4, keyf4);
	ffmapkey(all, SDLK_F5, keyf5);
	ffmapkey(all, SDLK_F6, keyf6);
	ffmapkey(all, SDLK_F7, keyf7);
	ffmapkey(all, SDLK_F8, keyf8);
	ffmapkey(all, SDLK_F9, keyf9);
	ffmapkey(all, SDLK_F10, keyf10);
	ffmapkey(all, SDLK_LSHIFT, key_left_shift);
	ffmapkey(all, SDLK_RSHIFT, key_right_shift);

	ffmapkey(all, SDLK_F11, keyfullscreen);
	ffmapkey(all, SDLK_PAGEDOWN, key_page_down);
	ffmapkey(all, SDLK_PAGEUP, key_page_up);

	mapkey(eng, SDLK_0, key_eng_preset_0);
	mapkey(eng, SDLK_1, key_eng_preset_1);
	mapkey(eng, SDLK_2, key_eng_preset_2);
	mapkey(eng, SDLK_3, key_eng_preset_3);
	mapkey(eng, SDLK_4, key_eng_preset_4);
	mapkey(eng, SDLK_5, key_eng_preset_5);
	mapkey(eng, SDLK_6, key_eng_preset_6);
	mapkey(eng, SDLK_7, key_eng_preset_7);
	mapkey(eng, SDLK_8, key_eng_preset_8);
	mapkey(eng, SDLK_9, key_eng_preset_9);
	ffmapkey(eng, SDLK_KP_0, key_eng_preset_0);
	ffmapkey(eng, SDLK_KP_1, key_eng_preset_1);
	ffmapkey(eng, SDLK_KP_2, key_eng_preset_2);
	ffmapkey(eng, SDLK_KP_3, key_eng_preset_3);
	ffmapkey(eng, SDLK_KP_4, key_eng_preset_4);
	ffmapkey(eng, SDLK_KP_5, key_eng_preset_5);
	ffmapkey(eng, SDLK_KP_6, key_eng_preset_6);
	ffmapkey(eng, SDLK_KP_7, key_eng_preset_7);
	ffmapkey(eng, SDLK_KP_8, key_eng_preset_8);
	ffmapkey(eng, SDLK_KP_9, key_eng_preset_9);

	mapkey(nav, SDLK_COMMA, key_decrease_warp);
	mapkey(nav, SDLK_PERIOD, key_increase_warp);
	mapkey(nav, SDLK_LEFTBRACKET, key_decrease_impulse);
	mapkey(nav, SDLK_RIGHTBRACKET, key_increase_impulse);
	mapkey(nav, SDLK_SLASH, key_engage_warp);
	mapkey(nav, SDLK_BACKSPACE, keyreverse);
	mapkey(nav, SDLK_x, key_docking_magnets);
	mapkey(nav, SDLK_i, key_exterior_lights);
	mapkey(nav, SDLK_b, key_standard_orbit);
	mapkey(nav, SDLK_t, key_starmap);

#ifdef DEBUG_KEYMAP
	print_keymap("keymap", keymap);
	print_keymap("ffkeymap", ffkeymap);
#endif

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

	for (i = keynone; i < NKEYSTATES; i++) {
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

void read_keymap_config_file(struct xdg_base_context *xdg_base_ctx)
{
	FILE *f;
	char line[256];
	char *s;
	char keyname[256], actionname[256], stations[256];
	int lineno, rc;

	errno = 0;
	f = xdg_base_fopen_for_read(xdg_base_ctx, "snis-keymap.txt");
	if (!f) {
		if (errno != ENOENT) /* it's normal for this file to not exist */
			fprintf(stderr, "Failed to open snis-keymap.txt: %s\n", strerror(errno));
		return;
	}

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
		if (rc == 3) {
			if (remapkey(stations, keyname, actionname) == 0)
				continue;
		}
		fprintf(stderr, "%s: syntax error at line %d:'%s'\n",
			"snis-keymap.txt", lineno, line);
	}
	fclose(f);
}
