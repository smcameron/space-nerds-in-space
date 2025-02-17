#ifndef SNIS_KEYBOARD_H_
#define SNIS_KEYBOARD_H_

#include "xdg_base_dir_spec.h"

enum keyaction {
		keynone		= 0,
		keydown		= 1,
		keyup		= 2,
		keyleft		= 3,
		keyright	= 4,
                keytorpedo	= 5,
		key_starmap	= 6,
                keyfullscreen	= 7,
		keythrust	= 8,
                keyquit		= 9,
                keypausehelp	= 10,
		keyreverse	= 11,
		keyf1		= 12,
		keyf2		= 13,
		keyf3		= 14,
		keyf4		= 15,
		keyf5		= 16,
                keyf6		= 17,
		keyf7		= 18,
		keyf8		= 19,
		keyf9		= 20,
		keyf10		= 21,
		keyonscreen	= 22,
		keyviewmode	= 23,
		keyzoom		= 24,
		keyunzoom	= 25,
		keyphaser	= 26,
		keyrenderswitch	= 27,
		keyrollleft = 28,
		keyrollright = 29,
		keysciball_yawleft = 30,
		keysciball_yawright = 31,
		keysciball_pitchup = 32,
		keysciball_pitchdown= 33,
		keysciball_rollleft = 34,
		keysciball_rollright = 35,
		key_invert_vertical = 36,
		key_toggle_frame_stats = 37,
		key_camera_mode = 38,
		key_page_up = 39,
		key_page_down = 40,
		key_toggle_space_dust = 41,
		key_toggle_credits = 42,
		key_toggle_watermark = 43,
		key_mouse_mode = 44,
		key_sci_mining_bot = 45,
		key_sci_tractor_beam = 46,
		key_sci_waypoints = 47,
		key_sci_lrs = 48,
		key_sci_srs = 49,
		key_sci_details = 50,
		key_weap_fire_missile = 51,
		key_space = 52,
		key_robot_gripper = 53,
		key_demon_console = 54,
		key_toggle_external_camera = 55,
		key_eng_preset_0 = 56,
		key_eng_preset_1 = 57,
		key_eng_preset_2 = 58,
		key_eng_preset_3 = 59,
		key_eng_preset_4 = 60,
		key_eng_preset_5 = 61,
		key_eng_preset_6 = 62,
		key_eng_preset_7 = 63,
		key_eng_preset_8 = 64,
		key_eng_preset_9 = 65,
		key_left_shift = 66,
		key_right_shift = 67,
		key_decrease_warp = 68,
		key_increase_warp = 69,
		key_decrease_impulse = 70,
		key_increase_impulse = 71,
		key_engage_warp = 72,
		key_docking_magnets = 73,
		key_exterior_lights = 74,
		key_standard_orbit = 75,
		key_weap_wavelen_0 = 76,
		key_weap_wavelen_1 = 77,
		key_weap_wavelen_2 = 78,
		key_weap_wavelen_3 = 79,
		key_weap_wavelen_4 = 80,
		key_weap_wavelen_5 = 81,
		key_weap_wavelen_6 = 82,
		key_weap_wavelen_7 = 83,
		key_weap_wavelen_8 = 84,
		key_weap_wavelen_9 = 85,
		key_weap_wavelen_nudge_up,
		key_weap_wavelen_nudge_down,
#define NKEYSTATES 88
};

struct keyboard_state {
	int pressed[NKEYSTATES];
};

struct keyname_value_entry {
	char *name;
	unsigned int value;
};

extern struct keyname_value_entry keyname_value_map[];

#define DISPLAYMODE_COUNT 15
extern enum keyaction keymap[DISPLAYMODE_COUNT][256];
extern enum keyaction ffkeymap[DISPLAYMODE_COUNT][256];
extern struct keyboard_state kbstate;

extern char *keyactionstring[];

extern void init_keymap(void);
extern int remapkey(char *stations, char *keyname, char *actionname);
extern void read_keymap_config_file(struct xdg_base_context *xdg_base_ctx);

#endif

