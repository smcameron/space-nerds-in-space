#ifndef SNIS_UI_H__
#define SNIS_UI_H__

#include <stdint.h>
#include <sys/time.h>

#include <SDL.h>

#include "rts_unit_data.h"
#include "snis_button.h"
#include "snis_packet.h"
#include "snis_faction.h"
#include "snis_gauge.h"
#include "snis_label.h"
#include "snis_pull_down_menu.h"
#include "snis_process_options.h"
#include "snis_sliders.h"
#include "snis_strip_chart.h"
#include "snis_text_input.h"
#include "snis_text_window.h"
#include "quat.h"
#define SNIS_CLIENT_DATA
#include "snis.h"
#undef SNIS_CLIENT_DATA

struct navigation_ui {
	struct slider *warp_slider;
	struct slider *navzoom_slider;
	struct slider *throttle_slider;
	struct gauge *warp_gauge;
	struct gauge *speedometer;
	struct button *engage_warp_button;
	struct button *docking_magnets_button;
	struct button *standard_orbit_button;
	struct button *reverse_button;
	struct button *trident_button;
	struct button *computer_button;
	struct button *starmap_button;
	struct button *lights_button;
	struct button *camera_pos_button;
	struct button *eject_warp_core_button;
	struct button *custom_button;
	int gauge_radius;
	struct snis_text_input_box *computer_input;
	char input[100];
	int computer_active;
};

struct damcon_ui {
	struct label *robot_controls;
	struct button *engineering_button;
	struct button *robot_forward_button;
	struct button *robot_backward_button;
	struct button *robot_left_button;
	struct button *robot_right_button;
	struct button *robot_gripper_button;
	struct button *robot_auto_button;
	struct button *robot_manual_button;
	struct button *eject_warp_core_button;
	struct button *custom_button;
};

struct demon_ui {
	/* (ux1, uy1), (ux2, uy2) are the universe (x,z) coordinates that correspond to
	 * the upper left and lower right corners of the screen.
	 */
	float ux1, uy1, ux2, uy2;
	double selectedx, selectedz;
	double press_mousex, press_mousey;
	double release_mousex, release_mousey;
	int button2_pressed;
	int button2_released;
	int button3_pressed;
	int button3_released;
	int nselected;
#define MAX_DEMON_SELECTABLE 256
	uint32_t selected_id[MAX_DEMON_SELECTABLE];
	struct button *demon_home_button;
	struct button *demon_ship_button;
	struct button *demon_starbase_button;
	struct button *demon_planet_button;
	struct button *demon_black_hole_button;
	struct button *demon_asteroid_button;
	struct button *demon_nebula_button;
	struct button *demon_spacemonster_button;
	struct button *demon_captain_button;
	struct button *demon_delete_button;
	struct button *demon_select_none_button;
	struct button *demon_torpedo_button;
	struct button *demon_phaser_button;
	struct button *demon_2d3d_button;
	struct button *demon_move_button;
	struct button *demon_scale_button;
	struct button *demon_console_button;
	struct snis_text_input_box *demon_input;
	struct scaling_strip_chart *bytes_recd_strip_chart;
	struct scaling_strip_chart *bytes_sent_strip_chart;
	struct scaling_strip_chart *latency_strip_chart;
	struct scaling_strip_chart *outgoing_client_queue_length_chart;
	struct scaling_strip_chart *incoming_client_queue_length_chart;
	struct scaling_strip_chart *outgoing_server_queue_length_chart;
	struct scaling_strip_chart *incoming_server_queue_length_chart;
	struct pull_down_menu *menu;
	struct text_window *console;
	char input[100];
	char error_msg[80];
	double ix, iz, ix2, iz2;
	uint32_t captain_of;
	uint32_t follow_id;
	int selectmode;
	int buttonmode;
	int shiptype;
	int faction;
	int render_style;
#define DEMON_UI_RENDER_STYLE_WIREFRAME 0
#define DEMON_UI_RENDER_STYLE_ALPHA_BY_NORMAL 1
	int move_from_x, move_from_y; /* mouse coords where move begins */
#define DEMON_BUTTON_NOMODE 0
#define DEMON_BUTTON_SHIPMODE 1
#define DEMON_BUTTON_STARBASEMODE 2
#define DEMON_BUTTON_PLANETMODE 3
#define DEMON_BUTTON_ASTEROIDMODE 4
#define DEMON_BUTTON_NEBULAMODE 5
#define DEMON_BUTTON_SPACEMONSTERMODE 6
#define DEMON_BUTTON_BLACKHOLEMODE 7
#define DEMON_BUTTON_DELETE 8
#define DEMON_BUTTON_SELECTNONE 9
#define DEMON_BUTTON_CAPTAINMODE 10
	int use_3d;
	union vec3 camera_pos;
	union quat camera_orientation;
	union vec3 desired_camera_pos;
	union quat desired_camera_orientation;
	float exaggerated_scale;
	float desired_exaggerated_scale;
	int exaggerated_scale_active;
	int netstats_active;
	int console_active;
	int log_console;
};

struct weapons_ui {
	struct gauge *phaser_bank_gauge;
	struct gauge *phaser_wavelength;
	struct slider *wavelen_slider;
	struct button *custom_button;
};

struct engineering_ui {
	struct gauge *fuel_gauge;
	struct gauge *amp_gauge;
	struct gauge *voltage_gauge;
	struct gauge *temp_gauge;
	struct gauge *oxygen_gauge;
	struct button *damcon_button;
	struct button *preset_buttons[ENG_PRESET_NUMBER];
	struct timeval preset_press_time[ENG_PRESET_NUMBER];
	struct button *preset_save_button;
	struct button *silence_alarms;
	struct button *deploy_flare;
	struct button *custom_button;
	struct slider *shield_slider;
	struct slider *shield_coolant_slider;
	struct slider *maneuvering_slider;
	struct slider *maneuvering_coolant_slider;
	struct slider *warp_slider;
	struct slider *warp_coolant_slider;
	struct slider *impulse_slider;
	struct slider *impulse_coolant_slider;
	struct slider *sensors_slider;
	struct slider *sensors_coolant_slider;
	struct slider *comm_slider;
	struct slider *comm_coolant_slider;
	struct slider *phaserbanks_slider;
	struct slider *phaserbanks_coolant_slider;
	struct slider *tractor_slider;
	struct slider *tractor_coolant_slider;
	struct slider *lifesupport_slider;
	struct slider *lifesupport_coolant_slider;
	struct slider *shield_control_slider;

	struct slider *shield_damage;
	struct slider *impulse_damage;
	struct slider *warp_damage;
	struct slider *maneuvering_damage;
	struct slider *phaser_banks_damage;
	struct slider *sensors_damage;
	struct slider *comms_damage;
	struct slider *tractor_damage;
	struct slider *lifesupport_damage;

	struct slider *shield_temperature;
	struct slider *impulse_temperature;
	struct slider *warp_temperature;
	struct slider *maneuvering_temperature;
	struct slider *phaser_banks_temperature;
	struct slider *sensors_temperature;
	struct slider *comms_temperature;
	struct slider *tractor_temperature;
	struct slider *lifesupport_temperature;

	int selected_subsystem;
	int selected_preset;
	int gauge_radius;
};

struct science_ui {
	/* details mode is one of define SCI_DETAILS_MODE_THREED,
	 * SCI_DETAILS_MODE_DETAILS, SCI_DETAILS_MODE_SCIPLANE,
	 * SCI_DETAILS_MODE_WAYPOINTS
	 */
	int details_mode;
	struct slider *scizoom;
	struct slider *scipower;
	struct button *details_button;
	struct button *threed_button;
	struct button *sciplane_button;
	struct button *tractor_button;
	struct button *align_to_ship_button;
	struct button *launch_mining_bot_button;
	struct button *waypoints_button;
	struct button *add_waypoint_button;
	struct button *add_current_pos_button;
	struct button *custom_button;
	struct snis_text_input_box *waypoint_input[3];
	char waypoint_text[3][15];
	struct button *clear_waypoint_button[MAXWAYPOINTS];
	struct button *select_waypoint_button[MAXWAYPOINTS];
	double waypoint[MAXWAYPOINTS][3];
	int nwaypoints;
	struct pull_down_menu *menu;
	int low_tractor_power_timer;
	int align_sciball_to_ship; /* mirrors player's o->tsd.ship.align_sciball_to_ship */
};

struct lobby_ui {
	struct button *lobby_cancel_button;
	struct button *lobby_connect_to_server_button;
};

struct comms_ui {
	struct text_window *tw;
	struct button *comms_onscreen_button;
	struct button *nav_onscreen_button;
	struct button *weap_onscreen_button;
	struct button *eng_onscreen_button;
	struct button *damcon_onscreen_button;
	struct button *sci_onscreen_button;
	struct button *main_onscreen_button;
	struct button *custom_button;
	struct button *cryptanalysis_button;
	struct button *comms_transmit_button;
	struct button *red_alert_button;
	struct button *hail_mining_bot_button;
	struct button *mainscreen_comms;
	struct button *rts_starbase_button[NUM_RTS_BASES];
	struct button *rts_fleet_button;
	struct button *rts_main_planet_button;
	struct button *rts_order_unit_button[NUM_RTS_UNIT_TYPES];
	struct button *rts_order_command_button[NUM_RTS_ORDER_TYPES];
	struct button *hail_button;
	struct button *channel_button;
	struct button *manifest_button;
	struct button *computer_button;
	struct button *eject_button;
	struct button *help_button;
	struct button *about_button;
	struct button *crypto_reset;
	struct snis_text_input_box *crypt_alpha[26];
	char crypt_alpha_text[26][3];
#define FLEET_BUTTON_COLS 9
#define FLEET_BUTTON_ROWS 10
	struct button *fleet_unit_button[FLEET_BUTTON_COLS][FLEET_BUTTON_ROWS];
	int fleet_order_checkbox[NUM_RTS_ORDER_TYPES];
	struct snis_text_input_box *comms_input;
	struct slider *mainzoom_slider;
	char input[100];
	uint32_t channel;
	struct strip_chart *emf_strip_chart;
	struct slider *our_base_health, *enemy_base_health;
};

struct network_setup_ui {
	struct button *connect_to_lobby;
	struct button *connect_to_snis_server;
	struct snis_text_input_box *snis_server_name_input;
	struct snis_text_input_box *snis_server_port_input;
	struct label *ss_name_label, *ss_port_label;
	struct snis_text_input_box *lobbyservername;
	struct snis_text_input_box *lobbyport;
	struct snis_text_input_box *shipname_box;
	struct snis_text_input_box *password_box;
	struct button *default_lobby_port_checkbox;
	struct button *role_main;
	struct button *role_nav;
	struct button *role_weap;
	struct button *role_eng;
	struct button *role_damcon;
	struct button *role_sci;
	struct button *role_comms;
	struct button *role_sound;
	struct button *role_projector;
	struct button *role_demon;
	struct button *role_text_to_speech;
	struct button *join_ship_checkbox;
	struct button *create_ship_checkbox;
	struct button *faction_checkbox[MAX_FACTIONS];
	struct button *support_button;
	struct button *launcher_button;
	struct button *website_button;
	struct button *forum_button;
	struct pull_down_menu *menu;
	int role_main_v;
	int role_nav_v;
	int role_weap_v;
	int role_eng_v;
	int role_damcon_v;
	int role_sci_v;
	int role_comms_v;
	int role_sound_v;
	int role_demon_v;
	int role_text_to_speech_v;
	int role_projector_v;
	int create_ship_v;
	int join_ship_v;
	int faction_checkbox_v[MAX_FACTIONS];
	char lobbyname[sizeof(((struct snis_process_options *) 0)->lobbyhost)];
	char lobbyportstr[sizeof(((struct snis_process_options *) 0)->lobbyport)];
	char snis_server_name[60];
	char snis_server_port[15];
	char solarsystem[60];
	char shipname[SHIPNAME_LEN];
	char password[PASSWORD_LEN];
	int selected_faction;
};

struct options_ui {
	struct button *launcher_btn;
	struct label *client_opts, *multiverse_opts, *server_opts;
	struct button *allow_remote_networks_btn;
	struct button *autowrangle_btn;
	struct button *mv_fixed_port_number_btn; /* mv_ means multiverse here. */
	struct button *ss_default_port_range_btn; /* ss_ means snis_server here. */
	struct button *ss_allow_remote_networks_btn;
	struct button *no_lobby_btn;
	struct button *solarsystem_enforcement;
	struct label *lobbyhost_label;
	struct label *lobbyport_label;
	struct snis_text_input_box *lobbyhost_input;
	struct snis_text_input_box *lobbyport_input;
	struct snis_text_input_box *mv_port_number_input;
	struct snis_text_input_box *ss_port_range_input;
	struct button *NAT_ghetto_mode_btn;
	struct button *leave_no_orphans_btn;
	char mv_port_number_text[15];
	char ss_port_range_text[15];
};

struct launcher_ui {
	struct button *start_ssgl_btn;
	struct button *start_snis_multiverse_btn;
	struct button *autowrangle_checkbox;
	struct button *start_snis_server_btn;
	struct button *connect_client_btn;
	struct button *stop_all_snis_btn;
	struct button *update_assets_btn;
	struct button *options_btn;
	struct button *quit_btn;
	struct gauge *ssgl_gauge;
	struct gauge *multiverse_gauge;
	struct gauge *snis_server_gauge;
	struct gauge *snis_client_gauge;
	struct button *restart_btn;
	struct button *website_button;
	struct button *forum_button;
	struct button *support_button;
	int ssgl_count;
	int multiverse_count;
	int snis_server_count;
	int snis_client_count;
};

#endif
