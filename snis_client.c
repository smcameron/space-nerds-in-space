/*
	Copyright (C) 2010 Stephen M. Cameron
	Author: Stephen M. Cameron

	This file is part of Spacenerds In Space.

	Spacenerds in Space is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.

	Spacenerds in Space is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with Spacenerds in Space; if not, write to the Free Software
	Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/
/* Need _GNU_SOURCE for qsort_r, must be defined before any include directives */
#define _GNU_SOURCE
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <ctype.h>
#include <sys/types.h>
#include <locale.h>
#include <gtk/gtk.h>

#ifndef WITHOUTOPENGL
#include <gtk/gtkgl.h>
#include <GL/glew.h>
#endif

#include <string.h>
#include <sys/time.h>
#include <unistd.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <gdk/gdkevents.h>
#include <gdk/gdkkeysyms.h>
#include <math.h>
#include <errno.h>
#include <pthread.h>
#include <sys/un.h>
#include <sys/socket.h>
#include <netdb.h>
#include <assert.h>
#include <netinet/tcp.h>
#include <netinet/in.h>
#include <getopt.h>
#include <fenv.h>

#include "arraysize.h"
#include "build_bug_on.h"
#include "string-utils.h"
#include "snis_version.h"
#include "snis_ship_type.h"
#include "snis_faction.h"
#include "space-part.h"
#include "mtwist.h"
#include "infinite-taunt.h"
#include "mathutils.h"
#include "quat.h"
#include "arbitrary_spin.h"
#define SNIS_CLIENT_DATA
#include "snis.h"
#undef SNIS_CLIENT_DATA
#include "snis-culture.h"
#include "snis_alloc.h"
#include "my_point.h"
#include "snis_font.h"
#include "snis_typeface.h"
#include "snis_graph.h"
#include "snis_ui_element.h"
#include "snis_gauge.h"
#include "snis_button.h"
#include "snis_label.h"
#include "snis_sliders.h"
#include "snis_text_window.h"
#include "snis_text_input.h"
#include "snis_strip_chart.h"
#include "snis_pull_down_menu.h"
#include "snis_socket_io.h"
#include "ssgl/ssgl.h"
#include "snis_marshal.h"
#include "snis_packet.h"
#include "snis_opcode_def.h"
#include "wwviaudio.h"
#include "sounds.h"
#include "bline.h"
#include "shield_strength.h"
#include "joystick.h"
#include "joystick_config.h"
#include "stacktrace.h"
#include "snis_keyboard.h"
#include "snis_preferences.h"
#include "snis_damcon_systems.h"
#include "build_info.h"
#include "snis-device-io.h"
#include "thrust_attachment.h"
#include "starbase_metadata.h"
#include "solarsystem_config.h"
#include "pronunciation.h"
#include "planetary_atmosphere.h"
#include "rts_unit_data.h"
#include "commodities.h"
#include "scipher.h"

#include "vertex.h"
#include "triangle.h"
#include "material.h"
#include "mesh.h"
#include "stl_parser.h"
#include "entity.h"
#include "matrix.h"
#include "graph_dev.h"
#include "ui_colors.h"
#include "pthread_util.h"
#include "snis_tweak.h"
#include "snis_debug.h"
#include "starmap_adjacency.h"
#include "rootcheck.h"
#include "replacement_assets.h"
#include "snis_asset_dir.h"
#include "snis_bin_dir.h"
#include "shape_collision.h"
#include "planetary_ring_data.h"
#include "planetary_properties.h"
#include "xdg_base_dir_spec.h"
#include "open-simplex-noise.h"
#include "snis_voice_chat.h"

#define SHIP_COLOR CYAN
#define STARBASE_COLOR RED
#define WARPGATE_COLOR WHITE
#define WORMHOLE_COLOR WHITE
#define PLANET_COLOR GREEN
#define BLACK_HOLE_COLOR WHITE
#define ASTEROID_COLOR AMBER
#define WARP_CORE_COLOR YELLOW
#define CARGO_CONTAINER_COLOR YELLOW 
#define DERELICT_COLOR BLUE 
#define PARTICLE_COLOR YELLOW
#define LASER_COLOR GREEN
#define PLAYER_LASER_COLOR GREEN
#define NPC_LASER_COLOR ORANGERED
#define TARGETING_COLOR ORANGERED
#define TORPEDO_COLOR RED
#define MISSILE_COLOR RED
#define SPACEMONSTER_COLOR GREEN
#define NEBULA_COLOR MAGENTA
#define TRACTORBEAM_COLOR BLUE
#define BLOCK_COLOR WHITE

static int SCREEN_WIDTH = 800; // 1366;        /* window width, in pixels */
static int SCREEN_HEIGHT = 600; // 768;       /* window height, in pixels */
#define ASPECT_RATIO (SCREEN_WIDTH/(float)SCREEN_HEIGHT)
static int requested_aspect_x = -1;
static int requested_aspect_y = -1;
static int screen_offset_x = 0;
static int screen_offset_y = 0;

static int da_configured = 0;

static int reload_shaders = 0; /* Flag to trigger reloading of glsl shaders in main render loop */

/* Set to 1 by command line option "--trap-nans" (or "-t").  Will abort when NaNs are produced. */
static int trap_nans = 0;

/* helper function to transform from 800x600 original coord system */
static inline int txx(int x) { return x * SCREEN_WIDTH / 800; }
static inline int txy(int y) { return y * SCREEN_HEIGHT / 600; }

#define VERTICAL_CONTROLS_INVERTED -1
#define VERTICAL_CONTROLS_NORMAL 1
#define BUTTON_HOLD_INTERVAL 100 /* ms */
static int vertical_controls_inverted = VERTICAL_CONTROLS_NORMAL;
static volatile int vertical_controls_timer = 0;

#define MOUSE_MODE_FREE_MOUSE 0
#define MOUSE_MODE_CAPTURED_MOUSE 1
static int desired_mouse_ui_mode = MOUSE_MODE_FREE_MOUSE;
static int current_mouse_ui_mode = MOUSE_MODE_FREE_MOUSE;
static volatile int mouse_mode_timer = 0;

static int display_frame_stats = 0;
static int quickstartmode = 0; /* allows auto connecting to first (only) lobby entry */
static float turret_recoil_amount = 0.0f;
static int global_rts_mode = 0;
static int current_typeface = 2; /* tweakable */

static int mtwist_seed = COMMON_MTWIST_SEED;
static float current_altitude = 1e20;
static int idiot_light_threshold = 10; /* tweakable */
static int dejitter_science_details = 1; /* Tweakable. 1 means de-jitter science details, 0 means don't */
static int monitor_voice_chat = 0;
static int have_talking_stick = 0;
static pthread_mutex_t voip_mutex = PTHREAD_MUTEX_INITIALIZER;

/* Dynamic range compression settings.  By default don't do much to regular
 * audio, but smash the crap out of the VOIP channels to make things more audible
 * and help with people speaking off-mic or with crappy mics or weak pre-amps.
 */
static float compressor_threshold = 0.5; /* tweakable */
static float compressor_limit = 0.98; /* tweakable */
static float voip_compressor_threshold = 0.2; /* tweakable */
static float voip_compressor_limit = 0.3; /* tweakable */

/* color tone mapping gain.  1.185 means white is pretty damn white. Lower values make white darker */
static float tonemapping_gain = 1.10; /* tweakable */

/* I can switch out the line drawing function with these macros */
/* in case I come across something faster than gdk_draw_line */
#define DEFAULT_LINE_STYLE sng_current_draw_line
#define DEFAULT_THICK_LINE_STYLE sng_current_draw_thick_line
#define DEFAULT_RECTANGLE_STYLE sng_current_draw_rectangle
#define DEFAULT_BRIGHT_LINE_STYLE sng_current_draw_bright_line
#define DEFAULT_ARC_STYLE sng_current_draw_arc

#define snis_draw_line DEFAULT_LINE_STYLE
#define snis_draw_thick_line DEFAULT_THICK_LINE_STYLE
#define snis_draw_rectangle DEFAULT_RECTANGLE_STYLE
#define snis_bright_line DEFAULT_BRIGHT_LINE_STYLE
#define snis_draw_arc DEFAULT_ARC_STYLE
static const int max_frame_rate_hz = 60;
static int frame_rate_hz = 30;
static int use_60_fps = 0; /* tweakable, set this to 1, and game will run at 60 fps */
static int red_alert_mode = 0;
#define MAX_UPDATETIME_START_PAUSE 1.5
#define MAX_UPDATETIME_INTERVAL 0.5
static int echo_computer_to_comms = 1; /* tweakable */
static float ambient_light = 0.015;

static char *asset_dir;

#define MAX_JOYSTICKS 10
static char joystick_device[PATH_MAX+1][MAX_JOYSTICKS];
static int joystick_fd[MAX_JOYSTICKS] = { -1 };
static int njoysticks = 0;
static struct joystick_descriptor *joysticks_found;
static struct joystick_config *joystick_cfg = NULL;
static int xjoystick_threshold = 23000; /* tweakable (Maybe this should be done via joystick_config.c instead) */
static int yjoystick_threshold = 23000; /* tweakable (Maybe this should be done via joystick_config.c instead) */

static int physical_io_socket = -1;
static pthread_t physical_io_thread;

static pthread_t natural_language_thread;
static pthread_t demon_fifo_thread;

static struct text_to_speech_queue_entry {
	char *text;
	struct text_to_speech_queue_entry *next;
} *text_to_speech_queue_head, *text_to_speech_queue_tail;
static pthread_t text_to_speech_thread;
static pthread_mutex_t text_to_speech_mutex;
static pthread_cond_t text_to_speech_cond = PTHREAD_COND_INITIALIZER;
static int text_to_speech_thread_time_to_quit = 0;
static float text_to_speech_volume = 0.33;

static GtkWidget *window;
static GdkGC *gc = NULL;               /* our graphics context. */
static GtkWidget *main_da;             /* main drawing area. */
static gint timer_tag;
static int fullscreen = 0;
static int in_the_process_of_quitting = 0;
static int current_quit_selection = 0;
static int final_quit_selection = 0;

static int textscreen_timer = 0;
static int user_defined_menu_active = 0;
static char textscreen[1024] = { 0 };
#define NUM_USER_MENU_BUTTONS 10
static struct button *user_menu_button[NUM_USER_MENU_BUTTONS];
static uint8_t active_custom_buttons;

static char enciphered_text[512] = { 0 };
static char cipher_key[26] = { 0 };
static int cipher_freq[26] = { 0 }; /* Frequency count for each letter of the alphabet in the enciphered message */
static char cipher_alpha_by_freq[27]; /* Letters of enciphered message sorted by frequency, null terminated */

static struct ship_type_entry *ship_type;
static int nshiptypes = 0;

static uint32_t role = ROLE_ALL;

static struct xdg_base_context *xdg_base_ctx = NULL;

static char *password;
static char *shipname;
#define UNKNOWN_ID 0xffffffff
static uint32_t my_ship_id = UNKNOWN_ID;
static uint32_t my_ship_oid = UNKNOWN_ID;
static uint32_t my_home_planet_oid = UNKNOWN_ID;

static int real_screen_width;
static int real_screen_height;
static int suppress_hyperspace_noise = 0;	/* tweakable */
static int warp_limbo_countdown = 0;
static int warp_field_error = 0;
static int damage_limbo_countdown = 0;

static struct entity_context *ecx;
static struct entity_context *sciecx;
static struct entity_context *instrumentecx; /* Used by nav screen, sciplane screen, and demon screen */
static struct entity_context *tridentecx;
static struct entity_context *sciballecx;
static struct entity_context *network_setup_ecx;
static int science_cam_timer = 0;

static int suppress_rocket_noise = 0;		/* tweakable */
static float rocket_noise_volume = 1.0;		/* tweakable */
static int ecx_fake_stars_initialized = 0;
static int nfake_stars = 0;
static volatile int fake_stars_timer = 0;
static volatile int credits_screen_active = 0;
static int watermark_active = 0;

static volatile int login_failed_timer = 0;
static char login_failed_msg[100] = { 0 };

static volatile int displaymode = DISPLAYMODE_LOBBYSCREEN;
static volatile int helpmode = 0;
static volatile float weapons_camera_shake = 0.0f; 
static volatile float main_camera_shake = 0.0f;
static float impulse_camera_shake = 1.0; /* tweakable */
static unsigned char camera_mode;
static unsigned char nav_camera_mode;
static unsigned char nav_has_computer_button = 0; /* tweakable */
static unsigned char planets_shade_other_objects = 1; /* tweakable */
static int main_nav_hybrid = 0; /* tweakable */
static float explosion_multiplier = 5.0; /* tweakable */
static float main_view_azimuth_angle = 0.0; /* tweakable */
static float weapons_azimuth_angle = 0.0; /* tweakable */

/* "Scenic" camera for beauty shots. */
static int external_camera_active = 0; /* tweakable */
static int external_camera_active_timer = 0;
union vec3 external_camera_position, desired_external_camera_position, external_camera_velocity;
union quat external_camera_orientation, desired_external_camera_orientation;
static int external_camera_mode = 0;
static const int num_external_camera_modes = 3;
static const char * const external_camera_mode_name[] = { "FOLLOW", "TRACK", "FREE" };

static struct client_network_stats {
	uint64_t bytes_sent;
	uint64_t bytes_recd;
	uint32_t nobjects, nships;
	uint32_t elapsed_seconds;
	uint32_t faction_population[5];
	uint32_t bytes_recd_per_sec[2];
	uint32_t bytes_sent_per_sec[2];
	int bps_index;
	struct timespec lasttime;
} netstats;

static int nframes = 0;
static int timer = 0;
static struct timeval start_time, end_time;
#define UNIVERSE_TICKS_PER_SECOND 10
static double universe_timestamp_offset = 0;

static volatile int done_with_lobby = 0;
static pthread_t lobby_thread;
static pthread_t gameserver_connect_thread;
static pthread_t read_from_gameserver_thread;
static pthread_t write_to_gameserver_thread;
static struct packed_buffer_queue to_server_queue;
static pthread_mutex_t to_server_queue_mutex;
static pthread_mutex_t to_server_queue_event_mutex;
static pthread_cond_t server_write_cond = PTHREAD_COND_INITIALIZER;
static int have_packets_for_server = 0;

static int lobby_socket = -1;
static int gameserver_sock = -1;
static int lobby_count = 0;
static char lobbyerror[200];
static char *lobbyhost = "localhost";
static int lobbyport = -1;
static int use_default_lobby_port = 1;
static char *serverhost = NULL;
static int serverport = -1;
static int monitorid = -1;
static int avoid_lobby = 0;

static pthread_mutex_t lobby_data_mutex = PTHREAD_MUTEX_INITIALIZER;
static struct ssgl_game_server lobby_game_server[100];
static int ngameservers = 0;

static struct commodity *commodity;
static int ncommodities;

static struct replacement_asset replacement_assets;

static struct ui_element_list *uiobjs = NULL;
static ui_element_drawing_function ui_slider_draw = (ui_element_drawing_function) snis_slider_draw;
static ui_element_button_release_function ui_slider_button_release =
		(ui_element_button_release_function) snis_slider_button_press;
static ui_element_button_release_function ui_button_button_press =
		(ui_element_button_release_function) snis_button_button_press;

static ui_element_drawing_function ui_button_draw = (ui_element_drawing_function) snis_button_draw;
static ui_element_drawing_function ui_strip_chart_draw =
		(ui_element_drawing_function) snis_strip_chart_draw;
static ui_element_drawing_function ui_scaling_strip_chart_draw =
		(ui_element_drawing_function) snis_scaling_strip_chart_draw;
static ui_element_drawing_function ui_label_draw = (ui_element_drawing_function) snis_label_draw;
static ui_element_button_release_function ui_button_button_release =
		(ui_element_button_release_function) snis_button_button_release;
static ui_element_button_release_function ui_pull_down_menu_button_release =
	(ui_element_button_release_function) pull_down_menu_button_press;
static ui_element_drawing_function ui_gauge_draw = (ui_element_drawing_function) gauge_draw;
static ui_element_drawing_function ui_text_window_draw = (ui_element_drawing_function) text_window_draw;
static ui_element_drawing_function ui_text_input_draw = (ui_element_drawing_function)
					snis_text_input_box_draw;
static ui_element_drawing_function ui_pull_down_menu_draw =
		(ui_element_drawing_function) pull_down_menu_draw;
static ui_element_set_focus_function ui_text_input_box_set_focus = (ui_element_set_focus_function)
					snis_text_input_box_set_focus;
static ui_element_button_release_function ui_text_input_button_release = (ui_element_button_release_function)
					snis_text_input_box_button_press;
static ui_element_keypress_function ui_text_input_keypress = (ui_element_keypress_function)
					snis_text_input_box_keypress;
static ui_element_button_release_function ui_text_window_button_release = (ui_element_button_release_function)
					text_window_button_press;
static ui_element_inside_function ui_button_inside = (ui_element_inside_function)
					snis_button_inside;
static ui_element_inside_function ui_gauge_inside = (ui_element_inside_function) gauge_inside;
static ui_element_inside_function ui_pull_down_menu_inside =
	(ui_element_inside_function) pull_down_menu_inside;
static ui_element_inside_function ui_slider_inside = (ui_element_inside_function)
					snis_slider_mouse_inside;

#define MAXTEXTURES 10

#ifdef WITHOUTOPENGL
#define GLuint int
#endif

static volatile int static_textures_loaded = 0; /* blech, volatile global. */
static volatile int per_solarsystem_textures_loaded = 0;
static char *lua_skybox_prefix = NULL;

static struct mesh *torpedo_mesh;
static struct mesh *missile_mesh;
static struct mesh *torpedo_nav_mesh;
static struct mesh *laser_mesh;
static struct mesh *asteroid_mesh[NASTEROID_MODELS];
static struct mesh *unit_cube_mesh;
static struct mesh *planet_sphere_mesh;
static struct mesh *planet_sphere_lp_mesh; /* low poly version */
static struct mesh *atmosphere_mesh;
static struct mesh *atmosphere_lp_mesh; /* low poly version */
static struct mesh *low_poly_sphere_mesh;
static struct mesh *cylindrically_mapped_sphere_mesh;
static struct mesh *planetary_ring_mesh;
static struct mesh *planetary_ring_lp_mesh; /* low poly version */
static struct mesh *nav_planetary_ring_mesh;
static struct mesh **starbase_mesh;
static int nstarbase_models = -1;
static struct starbase_file_metadata *starbase_metadata;
static struct docking_port_attachment_point **docking_port_info;
static struct mesh *ship_turret_mesh;
static struct mesh *ship_turret_base_mesh;
static struct mesh *turret_mesh;
static struct mesh *turret_base_mesh;
static struct mesh *particle_mesh;
static struct mesh *debris_mesh;
static struct mesh *debris2_mesh;
static struct mesh *wormhole_mesh;
static struct mesh *spacemonster_mesh;
static struct mesh *tentacle_segment_mesh;
static struct mesh *laserbeam_mesh;
static struct mesh *phaser_mesh;
static struct mesh *laserbeam_nav_mesh;
static struct mesh *ship_icon_mesh;
static struct mesh *heading_indicator_mesh;
static struct mesh *heading_indicator_tail_mesh;
static struct mesh *cargo_container_mesh;
static struct mesh *nebula_mesh;
static struct mesh *sun_mesh;
static struct mesh *unit_quad;
static struct mesh *thrust_animation_mesh;
static struct mesh *warpgate_mesh;
static struct mesh *warpgate_effect_mesh;
static struct mesh *warp_core_mesh;
#define NDOCKING_PORT_STYLES 3
static struct mesh *docking_port_mesh[NDOCKING_PORT_STYLES];
static struct mesh *warp_tunnel_mesh;
static struct entity *warp_tunnel = NULL;
static union vec3 warp_tunnel_direction;
static struct mesh *nav_axes_mesh = NULL;
static struct mesh *demon3d_axes_mesh = NULL;
static struct mesh *cylinder_mesh;
#define NLIGHTNINGS 20
static int planetary_lightning = 1; /* tweakable */
static struct mesh *planetary_lightning_mesh[NLIGHTNINGS] = { 0 };
static struct entity *sun_entity = NULL;
static int lens_flare_enabled = 1; /* tweakable */
static float lens_flare_intensity = 0.15; /* tweakable */
static float low_poly_threshold = 200.0; /* tweakable */
#ifndef WITHOUTOPENGL
static struct entity *lens_flare_halo = NULL;
static struct entity *anamorphic_flare = NULL;
#define NLENS_FLARE_GHOSTS 10
static struct entity *lens_flare_ghost[NLENS_FLARE_GHOSTS] = { 0 };
static const float lens_flare_ghost_scale[NLENS_FLARE_GHOSTS] = {
	0.3, 1.4, 0.2, 1.0, 1.7, 0.55, 0.30, 0.1, 0.2, 0.17,
};
static const float lens_flare_ghost_displacement[NLENS_FLARE_GHOSTS] = {
	0.3, 0.8, 0.7, 0.5, 1.2, 0.25, 0.8, 1.5, 0.65, 0.4,
};
#endif

static struct mesh **ship_mesh_map;
static struct mesh **derelict_mesh;

#define NNEBULA_MATERIALS 20
static struct material nebula_material[NNEBULA_MATERIALS];
static struct material missile_material;
static struct material red_torpedo_material;
static struct material red_laser_material;
static struct material blue_tractor_material;
static struct material green_phaser_material;
static struct material spark_material;
static struct material flare_material;
static struct material blackhole_spark_material;
static struct material laserflash_material;
static struct material warp_effect_material;
static struct material sun_material;
static struct material lens_flare_ghost_material;
static struct material lens_flare_halo_material;
static struct material anamorphic_flare_material;
static struct material black_hole_material;
static struct material spacemonster_tentacle_material;
static struct material spacemonster_material;
static struct material warpgate_material;
static struct material warpgate_effect_material;
static struct material docking_port_material;
static float atmosphere_brightness = 0.5; /* tweakable */
#define NPLANET_MATERIALS 256
static int planetary_ring_texture_id = -1;
static struct material planetary_ring_material[NPLANETARY_RING_MATERIALS];
static struct material planet_material[NPLANET_MATERIALS];
static struct solarsystem_asset_spec *solarsystem_assets = NULL;
static struct solarsystem_asset_spec *old_solarsystem_assets = NULL;
static char *solarsystem_name = DEFAULT_SOLAR_SYSTEM;
static char dynamic_solarsystem_name[100] = { 0 };
static char old_solarsystem_name[100] = { 0 };

/* star map data -- corresponds to known snis_server instances
 * as discovered by snis_server via ssgl_lobby and relayed to
 * clients via OPCODE_UPDATE_SOLARSYSTEM_LOCATION
 */
static struct starmap_entry starmap[MAXSTARMAPENTRIES];
static int nstarmap_entries = 0;
static int starmap_adjacency[MAXSTARMAPENTRIES][MAX_STARMAP_ADJACENCIES];

/* static char **planet_material_filename = NULL; */
/* int nplanet_materials = -1; */
static struct material shield_material;
static struct material warp_tunnel_material;
#define NASTEROID_TEXTURES 2
static struct material asteroid_material[NASTEROID_TEXTURES];
static struct material wormhole_material;
#define NTHRUSTMATERIALS 5
static struct material thrust_material[NTHRUSTMATERIALS];
static struct material thrust_flare_material[NTHRUSTMATERIALS];
static struct material block_material;
static struct material small_block_material;
static struct material planetary_lightning_material[NLIGHTNINGS];

#ifdef WITHOUTOPENGL
static const int wormhole_render_style = RENDER_SPARKLE;
static const int torpedo_render_style = RENDER_WIREFRAME | RENDER_BRIGHT_LINE | RENDER_NO_FILL;
static const int missile_render_style = RENDER_WIREFRAME | RENDER_BRIGHT_LINE | RENDER_NO_FILL;
static const int laserbeam_render_style = RENDER_WIREFRAME | RENDER_BRIGHT_LINE | RENDER_NO_FILL;
static const int spark_render_style = RENDER_WIREFRAME | RENDER_BRIGHT_LINE | RENDER_NO_FILL;
#else
static const int wormhole_render_style = RENDER_NORMAL;
static const int torpedo_render_style = RENDER_NORMAL;
static const int missile_render_style = RENDER_NORMAL;
static const int laserbeam_render_style = RENDER_NORMAL;
static const int spark_render_style = RENDER_NORMAL;
#endif

static struct my_point_t damcon_robot_points[] = {
#include "damcon-robot-points.h"
};
static struct my_vect_obj damcon_robot;
static struct my_point_t *damcon_robot_spun_points;
static struct my_vect_obj damcon_robot_spun[256];

static struct my_point_t placeholder_system_points[] = {
#include "placeholder-system-points.h"
};
static struct my_vect_obj placeholder_system;

static struct my_point_t placeholder_socket_points[] = {
#include "placeholder-socket-points.h"
};
static struct my_vect_obj placeholder_socket;

static struct my_point_t placeholder_part_points[] = {
#include "placeholder-part-points.h"
};
static struct my_point_t *placeholder_part_spun_points;
static struct my_vect_obj placeholder_part;
static struct my_vect_obj placeholder_part_spun[128];

static struct snis_entity *curr_science_guy = NULL;
static struct snis_entity *prev_science_guy = NULL;
static uint32_t curr_science_waypoint = (uint32_t) -1;
static uint32_t prev_science_waypoint = (uint32_t) -1;

static struct rts_planet_data {
	uint16_t health;
	uint32_t id;
	uint8_t faction;
} rts_planet[2] = {	{ 0, 0, 255 },
			{ 0, 0, 255 }, };

static void to_snis_heading_mark(const union quat *q, double *heading, double *mark)
{
	quat_to_heading_mark(q,heading,mark);
	*heading = game_angle_to_math_angle(*heading);
}

static void set_default_clip_window(void)
{
	sng_set_clip_window(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
}

static double universe_timestamp()
{
	return (time_now_double() - universe_timestamp_offset) * (double)UNIVERSE_TICKS_PER_SECOND;
}

static void format_date(char *buf, int bufsize, double date)
{
	buf[bufsize - 1] = '\0';
	snprintf(buf, bufsize, "%-8.1f", FICTIONAL_DATE(date));
}

static int switched_server = -1;
static char switch_server_location_string[20] = { 0 };
static int switch_warp_gate_number = -1;
static int switched_server2 = -1;
static int writer_thread_should_die = 0;
static int writer_thread_alive = 0;
static int connected_to_gameserver = 0;
static char connecting_to_server_msg[100] = { 0 };

#define MAX_LOBBY_TRIES 3
static void synchronous_update_lobby_info(void)
{
	int i, sock = -1, rc, game_server_count;
	struct ssgl_game_server *game_server = NULL;
	struct ssgl_client_filter filter;
	lobby_count = 0;
	char lobbyerror[255];

	/* Loop, trying to connect to the lobby server... */
	strcpy(lobbyerror, "");
	while (1 && lobby_count < MAX_LOBBY_TRIES) {
		fprintf(stderr, "snis_client: synchronous connecting to lobby %s:%d\n", lobbyhost, lobbyport);
		sock = ssgl_gameclient_connect_to_lobby_port(lobbyhost, lobbyport);
		lobby_count++;
		if (sock >= 0) {
			printf("snis_client: synchronous connected to lobby %s:%d\n", lobbyhost, lobbyport);
			break;
		}
		if (errno)
			snprintf(lobbyerror, sizeof(lobbyerror), "%s (%d)", strerror(errno), errno);
		else
			snprintf(lobbyerror, sizeof(lobbyerror), "%s (%d)", gai_strerror(sock), sock);
		fprintf(stderr, "snis_client: synchronous lobby connection failed: %s\n", lobbyerror);
		ssgl_sleep(5);
	}

	if (sock < 0) {
		fprintf(stderr, "snis_client: synchronous lobby connection failed %d times, giving up.\n", lobby_count);
		goto outta_here;
	}

	strcpy(lobbyerror, "");

	/* Ok, we've connected to the lobby server... */

	memset(&filter, 0, sizeof(filter));
	strcpy(filter.game_type, "SNIS");

	rc = ssgl_recv_game_servers(sock, &game_server, &game_server_count, &filter);
	if (rc) {
		snprintf(lobbyerror, sizeof(lobbyerror), "ssgl_recv_game_servers failed: %s\n", strerror(errno));
		fprintf(stderr, "snis_client: synchronous lobby connection: ssgl_recv_game_server failed: %s\n",
			lobbyerror);
		goto handle_error;
	}

	/* copy the game server data to where the display thread will see it. */
	fprintf(stderr, "snis_client: received %d game servers from lobby server\n", game_server_count);
	if (game_server_count > 100)
		game_server_count = 100;

	pthread_mutex_lock(&lobby_data_mutex);
	for (i = 0; i < game_server_count; i++) {
		fprintf(stderr, "snis_client: received game server '%s' from lobby server\n", game_server[i].location);
		lobby_game_server[i] = game_server[i];
	}
	ngameservers = game_server_count;
	pthread_mutex_unlock(&lobby_data_mutex);

	if (game_server_count > 0) {
		free(game_server);
		game_server_count = 0;
	}

outta_here:
	fprintf(stderr, "snis_client: synchronous lobby connection: lobby socket = %d, done with lobby\n", sock);
	/* close connection to the lobby */
	shutdown(sock, SHUT_RDWR);
	close(sock);
	return;

handle_error:
	if (sock >= 0)
		close(lobby_socket);
	if (game_server_count > 0) {
		free(game_server);
		game_server_count = 0;
		game_server = NULL;
	}
	return;
}

static void *connect_to_lobby_thread(__attribute__((unused)) void *arg)
{
	int i, sock = -1, rc, game_server_count;
	struct ssgl_game_server *game_server = NULL;
	struct ssgl_client_filter filter;
	lobby_count = 0;

try_again:

	printf("Trying to connect to lobby.\n");
	/* Loop, trying to connect to the lobby server... */
	strcpy(lobbyerror, "");
	while (1 && lobby_count < MAX_LOBBY_TRIES && displaymode == DISPLAYMODE_LOBBYSCREEN) {
		printf("snis_client: connecting to lobby %s:%d\n", lobbyhost, lobbyport);
		sock = ssgl_gameclient_connect_to_lobby_port(lobbyhost, lobbyport);
		lobby_count++;
		if (sock >= 0) {
			printf("snis_client: Connected to lobby %s:%d\n", lobbyhost, lobbyport);
			lobby_socket = sock;
			break;
		}
		if (errno)
			snprintf(lobbyerror, sizeof(lobbyerror), "%s (%d)", strerror(errno), errno);
		else
			snprintf(lobbyerror, sizeof(lobbyerror), "%s (%d)",
				gai_strerror(sock), sock);
		printf("snis_client: lobby connection failed: %s\n", lobbyerror);
		ssgl_sleep(5);
	}

	if (lobby_socket < 0) {
		displaymode = DISPLAYMODE_NETWORK_SETUP;
		lobby_count = 0;
		goto outta_here;
	}

	strcpy(lobbyerror, "");

	/* Ok, we've connected to the lobby server... */

	memset(&filter, 0, sizeof(filter));
	strcpy(filter.game_type, "SNIS");
	do {
		rc = ssgl_recv_game_servers(sock, &game_server, &game_server_count, &filter);
		if (rc) {
			snprintf(lobbyerror, sizeof(lobbyerror), "ssgl_recv_game_servers failed: %s\n",
					strerror(errno));
			printf("snis_client: ssgl_recv_game_server failed: %s\n", lobbyerror);
			goto handle_error;
		}
	
		/* copy the game server data to where the display thread will see it. */
		if (game_server_count > 100)
			game_server_count = 100;
		pthread_mutex_lock(&lobby_data_mutex);
		for (i = 0; i < game_server_count; i++)
			lobby_game_server[i] = game_server[i];
		ngameservers = game_server_count;
		pthread_mutex_unlock(&lobby_data_mutex);
		if (game_server_count > 0) {
			free(game_server);
			game_server_count = 0;
		}
		ssgl_sleep(5);  /* just a thread safe sleep. */
	} while (!done_with_lobby);

outta_here:
	printf("lobby socket = %d, done with lobby\n", sock);
	/* close connection to the lobby */
	shutdown(sock, SHUT_RDWR);
	close(sock);
	lobby_socket = -1;
	return NULL;

handle_error:
	if (lobby_socket >= 0)
		close(lobby_socket);
	lobby_socket = -1;
	ssgl_sleep(5);
	if (game_server_count > 0) {
		free(game_server);
		game_server_count = 0;
		game_server = NULL;
	}
	goto try_again;
}

static void connect_to_lobby(void)
{
	int rc;

	rc = create_thread(&lobby_thread, connect_to_lobby_thread, NULL, "snisc-lobbycon", 1);
	if (rc) {
		fprintf(stderr, "Failed to create lobby connection thread.\n");
		fprintf(stderr, "%d %s (%s)\n", rc, strerror(rc), strerror(errno));
	}
	return;
}

static struct snis_object_pool *pool;
static struct snis_object_pool *damcon_pool;
static double *damconscreenx, *damconscreeny;
static int damconscreenxdim = 600;
static int damconscreenydim = 500;
static int damconscreenx0 = 20;
static int damconscreeny0 = 80;

static struct snis_entity go[MAXGAMEOBJS];
#define go_index(snis_entity_ptr) ((snis_entity_ptr) - &go[0])
static struct snis_damcon_entity dco[MAXDAMCONENTITIES];
static struct snis_object_pool *sparkpool;
static struct snis_entity spark[MAXSPARKS];
#define spark_index(snis_entity_ptr) ((snis_entity_ptr) - &spark[0])
static pthread_mutex_t universe_mutex = PTHREAD_MUTEX_INITIALIZER;

static double quat_to_heading(const union quat *q)
{
	union vec3 v = { { 1.0, 0.0, 0.0 } };

	quat_rot_vec_self(&v, q);
	return atan2(-v.v.z, v.v.x);
}

static void print_demon_console_color_msg(int color, const char *fmt, ...);
static void set_object_location(struct snis_entity *o, double x, double y, double z)
{
	/* NaNs in NPC ship navigation have been enough of a problem that we should
	 * just always monitor for them.
	 */
	if (isnan(o->x) || isnan(o->y) || isnan(o->z)) {
		print_demon_console_color_msg(ORANGERED,
			"NaN DETECTED AT %s:%d:SET_OBJECT_LOCATION() x,y,z = %lf,%lf,%lf!",
			__FILE__, __LINE__, x, y, z);
		fprintf(stderr,
			"NaN DETECTED AT %s:%d:SET_OBJECT_LOCATION() x,y,z = %lf,%lf,%lf!\n",
			__FILE__, __LINE__, x, y, z);
	}
	o->x = x;
	o->y = y;
	o->z = z;
}

static int add_generic_object(uint32_t id, uint32_t timestamp, double x, double y, double z,
		double vx, double vy, double vz,
		const union quat *orientation, int type, uint16_t alive, struct entity *entity)
{
	int i;

	i = snis_object_pool_alloc_obj(pool); 	 
	if (i < 0) {
		printf("snis_object_pool_alloc_obj failed\n");
		return -1;
	}

	double t = (timestamp == 0) ? universe_timestamp() : (double)timestamp;

	memset(&go[i], 0, sizeof(go[i]));
	go[i].id = id;
	go[i].nupdates = 1;
	go[i].updatetime[0] = t;
	go[i].o[0] = *orientation;
	go[i].r[0].v.x = x;
	go[i].r[0].v.y = y;
	go[i].r[0].v.z = z;

	/* entity move will update this */
	set_object_location(&go[i], 0, 0, 0);
	go[i].orientation = identity_quat;

	go[i].vx = vx;
	go[i].vz = vz;
	go[i].heading = quat_to_heading(orientation);
	go[i].type = type;
	go[i].alive = alive;
	go[i].entity = entity;
	if (entity) {
		/* not visible until first interpolation */
		update_entity_visibility(entity, 0);
		entity_set_user_data(entity, &go[i]);
	}
	return i;
}

static void update_generic_object(int index, uint32_t timestamp, double x, double y, double z,
				double vx, double vy, double vz,
				const union quat *orientation, uint16_t alive)
{
	struct snis_entity *o = &go[index];

	/* shift old updates to make room for this one */
	int i;
	for (i = SNIS_ENTITY_NUPDATE_HISTORY - 1; i >= 1; i--) {
		o->updatetime[i] = o->updatetime[i-1];
		o->r[i] = o->r[i-1];
		o->o[i] = o->o[i-1];
	}

	double t = (timestamp == 0) ? universe_timestamp() : (double)timestamp;

	/* update the history and leave current for move_objects to take care of */
	o->nupdates++;
	o->updatetime[0] = t;
	o->r[0].v.x = x;
	o->r[0].v.y = y;
	o->r[0].v.z = z;
	if (orientation)
		o->o[0] = *orientation;

	o->vx = vx;
	o->vy = vy;
	o->vz = vz;
	o->heading = 0;
	if (orientation)
		o->heading = quat_to_heading(orientation);

	o->alive = alive;
}

static int lookup_object_by_id(uint32_t id)
{
	int i;
	const int cachebits = 13;
	const int cachesize = 1 << cachebits;
	const int cachemask = cachesize - 1;
	static int *cache = NULL;

	if (cache == NULL) {
		cache = malloc(sizeof(int) * cachesize);
		memset(cache, -1, sizeof(int) * cachesize);
	}

	i = cache[id & cachemask];
	if (i != -1 && go[i].id == id)
		return i;

	for (i = 0; i <= snis_object_pool_highest_object(pool); i++)
		if (go[i].id == id) {
			cache[id & cachemask] = i;
			return i;
		}
	return -1;
}

static struct snis_entity *lookup_entity_by_id(uint32_t id)
{
	int index = lookup_object_by_id(id);
	if (index < 0)
		return NULL;
	return &go[index];
}

static int lookup_damcon_object_by_id(uint32_t id)
{
	int i;

	for (i = 0; i <= snis_object_pool_highest_object(damcon_pool); i++)
		if (dco[i].id == id)
			return i;
	return -1;
}

static struct snis_entity *find_my_ship(void)
{
	if (my_ship_oid != UNKNOWN_ID)
		return &go[my_ship_oid];
	if (my_ship_id == UNKNOWN_ID)
		return NULL;
	my_ship_oid = (uint32_t) lookup_object_by_id(my_ship_id);
	if (my_ship_oid == UNKNOWN_ID)
		return NULL;
	return &go[my_ship_oid];
}

static struct snis_entity *rts_find_my_home_planet(void)
{
	int i;
	struct snis_entity *player;

	if (my_home_planet_oid != UNKNOWN_ID)
		return &go[my_home_planet_oid];

	player = find_my_ship();
	if (!player)
		return NULL;
	for (i = 0; i <= snis_object_pool_highest_object(pool); i++) {
		if (go[i].alive && go[i].type == OBJTYPE_PLANET && go[i].sdata.faction == player->sdata.faction) {
			my_home_planet_oid = i;
			return &go[my_home_planet_oid];
		}
	}
	return NULL;
}

static struct snis_entity *rts_find_my_starbase(int starbase_number)
{
	int i;

	for (i = 0; i <= snis_object_pool_highest_object(pool); i++) {
		if (go[i].type != OBJTYPE_STARBASE || !go[i].alive)
			continue;
		if (go[i].tsd.starbase.starbase_number == starbase_number)
			return &go[i];
	}
	return NULL;
}

static void update_generic_damcon_object(struct snis_damcon_entity *o,
			double x, double y, double velocity, double heading)
{
	o->x = x;
	o->y = y;
	o->velocity = velocity;
	o->heading = heading;
}

static void clear_damcon_pool()
{
	snis_object_pool_free_all_objects(damcon_pool);
	memset(dco, 0, sizeof(dco));
}

static int add_generic_damcon_object(uint32_t id, uint32_t ship_id, double x, double y,
			double velocity, double heading)
{
	int i;
	struct snis_damcon_entity *o;

	i = snis_object_pool_alloc_obj(damcon_pool); 	 
	if (i < 0) {
		printf("snis_object_pool_alloc_obj failed in add_generic_damcon_object\n");
		return -1;
	}
	o = &dco[i];
	memset(o, 0, sizeof(*o));
	o->id = id;
	o->ship_id = ship_id;
	update_generic_damcon_object(o, x, y, velocity, heading);
	return i;
}

static struct navigation_ui {
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
	struct button *custom_button;
	int gauge_radius;
	struct snis_text_input_box *computer_input;
	char input[100];
	int computer_active;
} nav_ui;

static struct damcon_ui {
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
} damcon_ui;

static int update_damcon_object(uint32_t id, uint32_t ship_id, uint32_t type,
			double x, double y, double velocity,
			double heading, uint8_t autonomous_mode)
{
	int i;
	struct snis_damcon_entity *o;
	const int selected = UI_COLOR(damcon_selected_button);
	const int deselected = UI_COLOR(damcon_button);

	i = lookup_damcon_object_by_id(id);
	if (i < 0) {
		i = add_generic_damcon_object(id, ship_id, x, y, velocity, heading);
		if (i < 0)
			return i;
		o = &dco[i];
		o->id = id;
		o->ship_id = ship_id;
		o->type = type;
		if (type == DAMCON_TYPE_ROBOT) {
			damconscreenx = &o->x;
			damconscreeny = &o->y;
			o->tsd.robot.autonomous_mode = !!autonomous_mode;
			snis_button_set_color(damcon_ui.robot_auto_button,
				autonomous_mode ? selected : deselected);
		}
		return 0;
	}
	o = &dco[i];
	update_generic_damcon_object(o, x, y, velocity, heading);
	if (o->type == DAMCON_TYPE_ROBOT) {
		o->tsd.robot.autonomous_mode = !!autonomous_mode;
		snis_button_set_color(damcon_ui.robot_auto_button,
			autonomous_mode ? selected : deselected);
		snis_button_set_color(damcon_ui.robot_manual_button,
			autonomous_mode ? deselected : selected);
	}
	return 0;
}

static int update_damcon_socket(uint32_t id, uint32_t ship_id, uint32_t type,
			double x, double y, uint32_t contents_id, uint8_t system,
			uint8_t part)

{
	int i;
	struct snis_damcon_entity *o;

	i = lookup_damcon_object_by_id(id);
	if (i < 0) {
		i = add_generic_damcon_object(id, ship_id, x, y, 0.0, 0.0);
		if (i < 0)
			return i;
		o = &dco[i];
		o->id = id;
		o->ship_id = ship_id;
		o->type = type;
		o->tsd.socket.contents_id = contents_id;
		o->tsd.socket.system = system;
		o->tsd.socket.part = part;
		return 0;
	}
	o = &dco[i];
	update_generic_damcon_object(o, x, y, 0.0, 0.0);
	o->tsd.socket.contents_id = contents_id;
	o->tsd.socket.system = system;
	o->tsd.socket.part = part;
	return 0;
}

static int update_damcon_part(uint32_t id, uint32_t ship_id, uint32_t type,
			double x, double y, double heading, uint8_t system,
			uint8_t part, uint8_t damage)

{
	int i;
	struct snis_damcon_entity *o;

	i = lookup_damcon_object_by_id(id);
	if (i < 0) {
		i = add_generic_damcon_object(id, ship_id, x, y, 0.0, 0.0);
		if (i < 0)
			return i;
		o = &dco[i];
		o->id = id;
		o->ship_id = ship_id;
		o->type = type;
		o->heading = heading;
		o->tsd.part.system = system;
		o->tsd.part.part = part;
		o->tsd.part.damage = damage;
		return 0;
	}
	o = &dco[i];
	update_generic_damcon_object(o, x, y, 0.0, heading);
	o->tsd.part.system = system;
	o->tsd.part.part = part;
	o->tsd.part.damage = damage;
	return 0;
}

static struct thrust_attachment_point **ship_thrust_attachment_points = NULL;
static struct thrust_attachment_point *missile_thrust_attachment_point = NULL;

static void allocate_ship_thrust_attachment_points(int n)
{
	if (ship_thrust_attachment_points)
		return;
	ship_thrust_attachment_points =
		malloc(sizeof(*ship_thrust_attachment_points) * n);
	memset(ship_thrust_attachment_points, 0,
			sizeof(*ship_thrust_attachment_points) * n);
}

static void read_thrust_attachment_points(char *dir, char *model_path, int shiptype,
			struct thrust_attachment_point **ap, float extra_scaling)
{
	char path[PATH_MAX + 1];
	int i;
	int total_len;

	if (strcmp(ship_type[shiptype].thrust_attachment_file, "!") == 0) {
		/* intentionally no exhaust ports */
		*ap = NULL;
		return;
	}

	if (strcmp(ship_type[shiptype].thrust_attachment_file, "-") == 0) {
		/* Default location, construct the path to *.scad_params.h file
		 * From the model file.
		 */
		total_len = strlen(dir) + strlen(model_path) +
				strlen("scad_params.h") + strlen("/models/") + 2;
		if (total_len >= PATH_MAX) {
			fprintf(stderr, "path '%s' is too long.\n", model_path);
			*ap = NULL;
			return;
		}
		snprintf(path, PATH_MAX, "%s/models/%s", dir, model_path);
		i = strlen(path);
		while (i >= 0 && path[i] != '.') {
			path[i] = '\0';
			i--;
		}
		if (i < 0) {
			fprintf(stderr, "Bad path model path %s\n", model_path);
			*ap = NULL;
			return;
		}
		strcat(path, "scad_params.h");
	} else {
		/* Specialized location of exhaust ports file */
		total_len = strlen(dir) + strlen(model_path) +
			strlen(ship_type[shiptype].thrust_attachment_file) + strlen("/models/") + 10;
		if (total_len >= PATH_MAX) {
			fprintf(stderr, "path '%s/%s' is too long.\n",
					dir, ship_type[shiptype].thrust_attachment_file);
			*ap = NULL;
			return;
		}
		snprintf(path, PATH_MAX, "%s/models/%s", dir, model_path);
		i = strlen(path);
		while (i >= 0 && path[i] != '/') {
			path[i] = '\0';
			i--;
		}
		if (i < 0) {
			fprintf(stderr, "Bad path model path %s\n", model_path);
			*ap = NULL;
			return;
		}
		strcat(path, ship_type[shiptype].thrust_attachment_file);
	}
	/* now read the scad_params.h file. */
	*ap = read_thrust_attachments(replacement_asset_lookup(path, &replacement_assets),
					SHIP_MESH_SCALE * extra_scaling);
	return;
}

static struct thrust_attachment_point *ship_thrust_attachment_point(int shiptype)
{
	/* since range of shiptype is determined runtime by reading a file... */
	if (shiptype < 0 || shiptype >= nshiptypes)
		return NULL;
	return ship_thrust_attachment_points[shiptype];
}

static void add_ship_thrust_entities(struct entity *thrust_entity[], int *nthrust_entities,
		struct entity_context *cx, struct entity *e, int shiptype, int impulse,
		const int thrust_material_index);

static int update_econ_ship(uint32_t id, uint32_t timestamp, double x, double y, double z,
			union quat *orientation, uint32_t victim_id,
			uint8_t shiptype, uint8_t ai[], double threat_level,
			uint8_t npoints, union vec3 *patrol, uint8_t faction, uint8_t rts_order)
{
	int i;
	struct entity *e;
	struct entity *thrust_entity[MAX_THRUST_PORTS * 2];
	int nthrust_ports = 0;
	double vx, vy, vz;

	i = lookup_object_by_id(id);
	if (i < 0) {
		memset(thrust_entity, 0, sizeof(thrust_entity));
		e = add_entity(ecx, ship_mesh_map[shiptype % nshiptypes], x, y, z, SHIP_COLOR);
		if (e)
			add_ship_thrust_entities(thrust_entity, &nthrust_ports, ecx, e, shiptype, 36,
						 faction % NTHRUSTMATERIALS);
		vx = 0.0;
		vy = 0.0;
		vz = 0.0;
		i = add_generic_object(id, timestamp, x, y, z, vx, vy, vz, orientation, OBJTYPE_SHIP2, 1, e);
		if (i < 0) {
			if (e)
				remove_entity(ecx, e);
			return i;
		}
		go[i].entity = e;
		if (e)
			entity_set_user_data(e, &go[i]);
		memcpy(go[i].tsd.ship.thrust_entity, thrust_entity, sizeof(thrust_entity));
		go[i].tsd.ship.nthrust_ports = nthrust_ports;
	} else {
		vx = x - go[i].x;
		vy = y - go[i].y;
		vz = z - go[i].z;

		int j;
		float v = sqrt(vx * vx + vy * vy + vz * vz);
		int throttle = (int) (180.0 * v /
					(float) ship_type[shiptype].max_speed);
		float thrust_size = clampf(throttle / 36.0, 0.1, 5.0);

		update_generic_object(i, timestamp, x, y, z, vx, vy, vz, orientation, 1);

		for (j = 0; j < go[i].tsd.ship.nthrust_ports; j++) {
			struct thrust_attachment_point *ap = ship_thrust_attachment_point(shiptype);
			struct entity *t = go[i].tsd.ship.thrust_entity[j];
			if (t) {
				if ((j & 0x01) == 0) { /* Even are thrust entities, odd are thrust flares */
					union vec3 thrust_scale = { { thrust_size, 1.0, 1.0 } };
					vec3_mul_self(&thrust_scale, ap->port[j / 2].scale);
					update_entity_non_uniform_scale(t, thrust_scale.v.x,
								thrust_scale.v.y, thrust_scale.v.z);
				} else {
					update_entity_scale(t, (0.5 * THRUST_FLARE_SCALE +
							snis_randn(100) * THRUST_FLARE_SCALE * 0.02) *
							thrust_size * ap->port[j / 2].scale);
				}
			}
		}
	}
	go[i].sdata.faction = faction;

	/* Ugh, using ai[0] and ai[1] this way is a pretty putrid hack. */
	go[i].tsd.ship.ai[0].u.attack.victim_id = (int32_t) victim_id;
	go[i].tsd.ship.shiptype = shiptype;
	go[i].tsd.ship.threat_level = (float) threat_level;
	memcpy(go[i].ai, ai, MAX_AI_STACK_ENTRIES);
	ai[MAX_AI_STACK_ENTRIES - 1] = '\0';
	if (npoints > MAX_AI_STACK_ENTRIES)
		npoints = MAX_AI_STACK_ENTRIES;
	go[i].tsd.ship.ai[1].u.patrol.npoints = npoints;
	if (npoints) {
		int j;
		for (j = 0; j < npoints; j++)
			go[i].tsd.ship.ai[1].u.patrol.p[j] = patrol[j];
	}
	if (rts_order != 255) {
		go[i].tsd.ship.ai[0].ai_mode = rts_order;
		go[i].tsd.ship.nai_entries = 1;
	}
	return 0;
}

static int update_power_model_data(uint32_t id, struct power_model_data *pmd)
{
	int i;

	i = lookup_object_by_id(id);
	if (i < 0)
		return -1;
	go[i].tsd.ship.power_data = *pmd;
	return 0;
}

static int update_coolant_model_data(uint32_t id, struct power_model_data *pmd,
			struct ship_damage_data *temperature_data)
{
	int i;

	i = lookup_object_by_id(id);
	if (i < 0)
		return -1;
	go[i].tsd.ship.coolant_data = *pmd;
	go[i].tsd.ship.temperature_data = *temperature_data;
	return 0;
}

static int update_ship_sdata(uint32_t id, uint8_t subclass, char *name,
				uint8_t shield_strength, uint8_t shield_wavelength,
				uint8_t shield_width, uint8_t shield_depth, uint8_t faction, uint8_t lifeform_count)
{
	int i;
	i = lookup_object_by_id(id);
	if (i < 0)
		return i;
	go[i].sdata.subclass = subclass;
	go[i].sdata.shield_strength = shield_strength;
	go[i].sdata.shield_wavelength = shield_wavelength;
	go[i].sdata.shield_width = shield_width;
	go[i].sdata.shield_depth = shield_depth;
	go[i].sdata.faction = faction;
	if (strcmp(name, "-") != 0)
		strcpy(go[i].sdata.name, name);
	if (go[i].type == OBJTYPE_SHIP1 || go[i].type == OBJTYPE_SHIP2)
		go[i].tsd.ship.lifeform_count = lifeform_count;
	if (go[i].type == OBJTYPE_STARBASE)
		go[i].tsd.starbase.lifeform_count = lifeform_count;
	if (!go[i].sdata.science_data_known && displaymode == DISPLAYMODE_SCIENCE)
		wwviaudio_add_sound(SCIENCE_PROBE_SOUND);
	if (go[i].type != OBJTYPE_PLANET &&
		go[i].type != OBJTYPE_STARBASE &&
		go[i].type != OBJTYPE_BLACK_HOLE)
		go[i].sdata.science_data_known = frame_rate_hz * 10; /* only remember for ten secs. */
	else
		go[i].sdata.science_data_known = frame_rate_hz * 60; /* unless planet, starbase or black hole */
		
	return 0;
}

static int update_torpedo(uint32_t id, uint32_t timestamp, double x, double y, double z, uint32_t ship_id)
{
	int i;
	struct entity *e;
	struct snis_entity *myship;

	i = lookup_object_by_id(id);
	if (i < 0) {
		e = add_entity(ecx, torpedo_mesh, x, y, z, TORPEDO_COLOR);
		if (e) {
			set_render_style(e, torpedo_render_style);
			update_entity_material(e, &red_torpedo_material);
		}
		i = add_generic_object(id, timestamp, x, y, z, 0.0, 0.0, 0.0, &identity_quat, OBJTYPE_TORPEDO, 1, e);
		if (i < 0)
			return i;
		go[i].tsd.torpedo.ship_id = ship_id;
		myship = find_my_ship();
		if (myship && myship->id == ship_id)
			weapons_camera_shake = 1.0;
	} else {
		update_generic_object(i, timestamp, x, y, z, 0.0, 0.0, 0.0, &identity_quat, 1);
		e = go[i].entity;
		if (e)
			update_entity_scale(e, 0.01 * snis_randn(100) + 0.5);
	}
	return 0;
}

static void add_thrust_entities(struct entity *thrust_entity[],
		int *nthrust_ports, struct entity_context *cx, struct entity *e,
		struct thrust_attachment_point *ap, int impulse, const int thrust_material_index);

static int update_missile(uint32_t id, uint32_t timestamp, double x, double y, double z,
			union quat *orientation)
{
	int i;
	struct entity *e;
	struct entity *thrust_entity[2];
	int nthrust_ports = 0;
#if 0
	struct snis_entity *myship;
#endif

	i = lookup_object_by_id(id);
	if (i < 0) {
		e = add_entity(ecx, missile_mesh, x, y, z, MISSILE_COLOR);
		if (e) {
			set_render_style(e, missile_render_style);
			update_entity_material(e, &missile_material);
			add_thrust_entities(thrust_entity, &nthrust_ports, ecx, e,
						missile_thrust_attachment_point, 36, 0);
		}
		i = add_generic_object(id, timestamp, x, y, z, 0.0, 0.0, 0.0, orientation, OBJTYPE_MISSILE, 1, e);
		if (i < 0)
			return i;
		go[i].tsd.missile.target_id = (uint32_t) -1;
		if (e) {
			go[i].tsd.missile.thrust_entity[0] = thrust_entity[0];
			go[i].tsd.missile.thrust_entity[1] = thrust_entity[1];
		} else {
			go[i].tsd.missile.thrust_entity[0] = NULL;
			go[i].tsd.missile.thrust_entity[1] = NULL;
		}
#if 0
		myship = find_my_ship();
		if (myship && myship->id == ship_id)
			weapons_camera_shake = 1.0;
#endif
	} else {
		float vx, vy, vz;
		vx = x - go[i].x;
		vy = y - go[i].y;
		vz = z - go[i].z;

		int j;
		float v = sqrt(vx * vx + vy * vy + vz * vz);
		int throttle = (int) (180.0 * v / (float) DEFAULT_MISSILE_VELOCITY);
		float thrust_size = clampf(throttle / 36.0, 0.1, 5.0);

		update_generic_object(i, timestamp, x, y, z, 0.0, 0.0, 0.0, orientation, 1);
		struct thrust_attachment_point *ap = missile_thrust_attachment_point;

		for (j = 0; j < 2; j++) {
			struct entity *t = go[i].tsd.missile.thrust_entity[j];
			if (t) {
				if ((j & 0x01) == 0) { /* Even are thrust entities, odd are thrust flares */
					union vec3 thrust_scale = { { thrust_size, 1.0, 1.0 } };
					vec3_mul_self(&thrust_scale, ap->port[j / 2].scale);
					update_entity_non_uniform_scale(t, thrust_scale.v.x,
								thrust_scale.v.y, thrust_scale.v.z);
				} else {
					update_entity_scale(t, (0.5 * THRUST_FLARE_SCALE +
							snis_randn(100) * THRUST_FLARE_SCALE * 0.02) *
							thrust_size * ap->port[j / 2].scale);
				}
			}
		}
	}
	return 0;
}

static void init_laserbeam_data(struct snis_entity *o);
static void laserbeam_move(struct snis_entity *o);
static int update_laserbeam(uint32_t id, uint32_t timestamp, uint32_t origin, uint32_t target)
{
	int i;

	i = lookup_object_by_id(id);
	if (i < 0) {
		i = add_generic_object(id, timestamp, 0, 0, 0, 0, 0, 0,
			&identity_quat, OBJTYPE_LASERBEAM, 1, NULL);
		if (i < 0)
			return i;
		go[i].tsd.laserbeam.origin = origin;
		go[i].tsd.laserbeam.target = target;
		go[i].tsd.laserbeam.material = &red_laser_material;
		init_laserbeam_data(&go[i]);
		go[i].move = laserbeam_move;
	} /* nothing to do */
	return 0;
}

static int update_tractorbeam(uint32_t id, uint32_t timestamp, uint32_t origin, uint32_t target)
{
	int i;

	i = lookup_object_by_id(id);
	if (i < 0) {
		i = add_generic_object(id, timestamp, 0, 0, 0, 0, 0, 0,
				&identity_quat, OBJTYPE_TRACTORBEAM, 1, NULL);
		if (i < 0)
			return i;
		go[i].tsd.laserbeam.origin = origin;
		go[i].tsd.laserbeam.target = target;
		go[i].tsd.laserbeam.material = &blue_tractor_material;
		init_laserbeam_data(&go[i]);
		go[i].move = laserbeam_move;
	} /* nothing to do */
	return 0;
}


static int update_laser(uint32_t id, uint32_t timestamp, uint8_t power, double x, double y, double z,
			union quat *orientation, uint32_t ship_id)
{
	int i;
	struct entity *e;
	struct snis_entity *myship;

	i = lookup_object_by_id(id);
	if (i < 0) {
		e = add_entity(ecx, phaser_mesh, x, y, z, LASER_COLOR);
		if (e) {
			set_render_style(e, laserbeam_render_style);
			update_entity_material(e, &green_phaser_material);
		}
		i = add_generic_object(id, timestamp, x, y, z, 0.0, 0.0, 0.0, orientation, OBJTYPE_LASER, 1, e);
		if (i < 0)
			return i;
		go[i].tsd.laser.birth_r = go[i].r[0];
		go[i].tsd.laser.ship_id = ship_id;
		go[i].tsd.laser.power = power;
		myship = find_my_ship();
		if (myship && myship->id == ship_id) {
			weapons_camera_shake = 1.0;
			turret_recoil_amount = 2.0f;
		}
	} else {
		update_generic_object(i, timestamp, x, y, z, 0.0, 0.0, 0.0, orientation, 1);
	}
	return 0;
}

static void laserbeam_move(struct snis_entity *o)
{
	int oid, tid;
	struct snis_entity *origin, *target;
	union vec3 offset;
	union vec3 right = { { 1.0f, 0.0f, 0.0f } };
	union vec3 up = { { 0.0f, 0.1f, 0.0f } } ;
	union vec3 target_vector;
	union quat orientation;
	double x, y, z, length;
	const double epsilon = 0.1; /* offset so laserbeam isn't exactly on the camera */

	oid = lookup_object_by_id(o->tsd.laserbeam.origin);
	tid = lookup_object_by_id(o->tsd.laserbeam.target);

	if (oid < 0 || tid < 0) {
		if (o->entity)
			entity_set_mesh(o->entity, NULL);
		return;
	}
	origin = &go[oid];
	target = &go[tid];

	if (o->type == OBJTYPE_TRACTORBEAM) {
#define TRACTORBEAM_OFFSET (-3.0 * SHIP_MESH_SCALE);
		offset.v.x = TRACTORBEAM_OFFSET;
		offset.v.y = TRACTORBEAM_OFFSET;
		offset.v.z = 0;
		quat_rot_vec_self(&offset, &origin->orientation);
		offset.v.x += origin->x;
		offset.v.y += origin->y;
		offset.v.z += origin->z;
	} else {
		offset.v.x = origin->x;
		offset.v.y = origin->y;
		offset.v.z = origin->z;
	}
	/* The + (o->id & 0x7) -4 is to offset consistently from the camera, +1, +2 so they aren't all 0. */
	target_vector.v.x = target->x - offset.v.x + (o->id & 0x7) - 4;
	target_vector.v.y = target->y - offset.v.y + ((o->id + 1) & 0x7) - 4;
	target_vector.v.z = target->z - offset.v.z + ((o->id + 2) & 0x7) - 4;
	length = vec3_magnitude(&target_vector);

	if (target->type == OBJTYPE_PLANET) { /* Account for the radius of the planet */
		float adjusted_length = length - target->tsd.planet.radius;
		vec3_mul_self(&target_vector, adjusted_length / length);
		length = adjusted_length;
	}

	quat_from_u2v(&orientation, &right, &target_vector, &up); /* correct up vector? */
	quat_normalize_self(&orientation);

	x = offset.v.x + 0.5 * target_vector.v.x;
	y = offset.v.y + 0.5 * target_vector.v.y;
	z = offset.v.z + 0.5 * target_vector.v.z;

	if (o->entity) {
		update_entity_pos(o->entity, x, y, z);
		update_entity_orientation(o->entity, &orientation);
		update_entity_material(o->entity, o->tsd.laserbeam.material);
		update_entity_non_uniform_scale(o->entity, length, 5.0 + snis_randn(20), 0.001);
	}

	if (o->tsd.laserbeam.laserflash_entity) {
		/* particle mesh is 50x50, scale it randomly to make it flicker */
		update_entity_scale(o->tsd.laserbeam.laserflash_entity,
					0.02 * (snis_randn(200) + 50.0));
		update_entity_pos(o->tsd.laserbeam.laserflash_entity,
					target->x + epsilon, target->y + epsilon, target->z + epsilon);
	}
}

static void init_laserbeam_data(struct snis_entity *o)
{
	struct laserbeam_data *ld = &o->tsd.laserbeam;
	int color;
	struct snis_entity *shooter, *target;
	union vec3 offset;

	shooter = lookup_entity_by_id(ld->origin);
	target = lookup_entity_by_id(ld->target);
	if (!shooter || !target) {
		o->entity = NULL;
		return;
	}
	if (o->type == OBJTYPE_TRACTORBEAM) {
		color = TRACTORBEAM_COLOR;
		o->tsd.laserbeam.laserflash_entity = NULL;
		offset.v.x = TRACTORBEAM_OFFSET;
		offset.v.y = TRACTORBEAM_OFFSET;
		offset.v.z = 0;
	} else {
		offset.v.x = 0;
		offset.v.y = 0;
		offset.v.z = 0;
		if (shooter)
			color = (shooter->type == OBJTYPE_SHIP2) ?
				NPC_LASER_COLOR : PLAYER_LASER_COLOR;
		else
			color = NPC_LASER_COLOR;
		o->tsd.laserbeam.laserflash_entity =
			add_entity(ecx, particle_mesh, target->x, target->y, target->z, color);
		if (o->tsd.laserbeam.laserflash_entity) {
			update_entity_material(o->tsd.laserbeam.laserflash_entity,
						&laserflash_material);
			/* particle mesh is 50x50, scale it from 0.5 to 1.0, randomly */
			update_entity_scale(o->tsd.laserbeam.laserflash_entity,
						0.01 * (snis_randn(50) + 50.0));
		}
	}
	quat_rot_vec_self(&offset, &shooter->orientation);
	offset.v.x += shooter->x;
	offset.v.y += shooter->y;
	offset.v.z += shooter->z;
	set_object_location(o, (target->x - offset.v.x) / 2.0,
				(target->y - offset.v.y) / 2.0,
				(target->z - offset.v.z) / 2.0);

	o->entity = add_entity(ecx, laserbeam_mesh, o->x, o->y, o->z, color);
	if (o->entity)
		set_render_style(o->entity, laserbeam_render_style);
	laserbeam_move(o);
}

static void spacemonster_move_tentacles(struct snis_entity *o)
{
	int i, j;
	struct mtwist_state *mt;
	float initial_angle, delta_angle, desired_angle, new_angle, current_angle;
	union quat rotation;
	int ntentacles = (o->id % 3) + 3;

	mt = mtwist_init(o->tsd.spacemonster.seed);
	for (i = 0; i < ntentacles; i++) {
		initial_angle = mtwist_float(mt) * 40.0 * M_PI / 180.0 - 20 * M_PI / 180.0;
		delta_angle = mtwist_float(mt) * 20.0 * M_PI / 180.0 - 10.0 * M_PI / 180.0;
		desired_angle = initial_angle;
		quat_init_axis(&rotation, 1, 0, 0, i * -2.0 * M_PI / ntentacles);
		for (j = 1; j < NTENTACLE_SEGMENTS; j++) { /* Note: first segment is skipped. */
			if (o->tsd.spacemonster.tentacle[i][j]) {
				union quat nq;
				current_angle = o->tsd.spacemonster.tentacle_angle[i][j];
				new_angle = current_angle + 0.05 * (desired_angle - current_angle);
				quat_init_axis(&nq, 0, 1, 0, new_angle);
				update_entity_orientation(o->tsd.spacemonster.tentacle[i][j], &nq);
				o->tsd.spacemonster.tentacle_angle[i][j] = new_angle;
			}
			desired_angle = desired_angle + delta_angle;
		}
	}
	mtwist_free(mt);
}

static void init_spacemonster_data(struct snis_entity *o, float tentacle_scale)
{
	int i, j, ntentacles;
	struct entity *parent;
	float shrinkage = 0.98;
	union quat orientation, new_orientation;
	float angle, y, z;
	union quat rotation;
	ntentacles = (o->id % 3) + 3;
	quat_init_axis(&orientation, 0, 1, 0, -10.0 * M_PI / 180.0);
	quat_init_axis(&rotation, 1, 0, 0, -2.0 * M_PI / ntentacles);

	for (i = 0; i < ntentacles; i++) {
		angle = i * (2.0 * M_PI / ntentacles);
		parent = o->entity;
		float tentacle_length = 40.0;
		float tentacle_scale = 1.1;
		for (j = 0; j < NTENTACLE_SEGMENTS; j++) {
			struct entity **e = &o->tsd.spacemonster.tentacle[i][j];
			o->tsd.spacemonster.tentacle_angle[i][j] = -5.0 * M_PI / 180.0;
			if (j == 0) {
				y = 20.0 * sin(angle);
				z = 20.0 * cos(angle);
			} else {
				y = 0.0;
				z = 0.0;
			}
			*e = add_entity(ecx, tentacle_segment_mesh,
				1.0 * tentacle_length, y, z, SPACEMONSTER_COLOR);
			if (*e) {
				update_entity_orientation(*e, &orientation);
				update_entity_parent(ecx, *e, parent);
				update_entity_scale(*e, tentacle_scale);
				update_entity_material(*e, &spacemonster_tentacle_material);
				update_entity_non_uniform_scale(*e, 1.0, tentacle_scale, tentacle_scale);
				parent = *e;
			} else {
				break;
			}
			tentacle_length *= shrinkage;
			tentacle_scale *= shrinkage;
		}
		quat_mul_self(&orientation, &rotation);
		quat_conjugate(&new_orientation, &orientation, &rotation);
		orientation = new_orientation;
	}
}

static int update_spacemonster(uint32_t id, uint32_t timestamp, double x, double y, double z,
				uint32_t seed, union quat *orientation, uint8_t emit_intensity,
				uint8_t head_size, uint8_t tentacle_size)
{
	int i;
	struct entity *e;

	i = lookup_object_by_id(id);
	if (i < 0) {
		float tentacle_scale, head_scale;

		tentacle_scale = 0.5 + (float) tentacle_size / 255.0;
		head_scale = 0.5 + (float) head_size / 255.0;
		e = add_entity(ecx, spacemonster_mesh, x, y, z, SPACEMONSTER_COLOR);
		if (e) {
			set_render_style(e, RENDER_SPARKLE);
			update_entity_material(e, &spacemonster_material);
			update_entity_non_uniform_scale(e, 1.0, head_scale, head_scale);
		}
		/* initial orientation is identity quat to make setting up tentacles easier */
		i = add_generic_object(id, timestamp, x, y, z, 0, 0, 0,
				&identity_quat, OBJTYPE_SPACEMONSTER, 1, e);
		if (i < 0)
			return i;
		go[i].entity = e;
		go[i].tsd.spacemonster.seed = seed;
		go[i].move = spacemonster_move_tentacles;
		init_spacemonster_data(&go[i], tentacle_scale);
		/* Now update the orientation */
		update_generic_object(i, timestamp, x, y, z, 0, 0, 0, orientation, 1);
	} else {
		update_generic_object(i, timestamp, x, y, z, 0, 0, 0, orientation, 1);
		if (go[i].entity)
			update_entity_pos(go[i].entity, x, y, z);
		go[i].tsd.spacemonster.seed = seed;
	}
	/* FIXME: This means all space monsters pulsate together, and the last spacemonster
	 * to update sets the value. This should somehow be per space monster. If there's only
	 * one or two space monsters around, it is not a big deal, but this isn't really how it
	 * should work.
	 */
	spacemonster_material.texture_mapped.emit_intensity =
		fabs(sin(((float) emit_intensity / 255.0) * 2.0 * M_PI));
	spacemonster_tentacle_material.texture_mapped.emit_intensity =
		fabs(sin(((float) emit_intensity / 255.0) * 2.0 * M_PI));
	go[i].tsd.spacemonster.head_size = head_size;
	go[i].tsd.spacemonster.tentacle_size = tentacle_size;
	return 0;
}

static int update_block(uint32_t id, uint32_t timestamp, double x, double y, double z,
		double sizex, double sizey, double sizez, union quat *orientation,
		uint8_t block_material_index, uint8_t health, uint8_t form)
{
	int i, j;
	struct entity *e = NULL;
	double vx, vy, vz;
	union quat capsule_sphere_orientation;

	i = lookup_object_by_id(id);
	if (i >= 0) {
		vx = x - go[i].x; /* verlet integration */
		vy = y - go[i].y;
		vz = z - go[i].z;
		update_generic_object(i, timestamp, x, y, z, vx, vy, vz, orientation, 1);
		go[i].tsd.block.health = health;
		if (go[i].entity) {
			switch (form) {
			case SHAPE_SPHERE:
				shape_init_sphere(&go[i].tsd.block.shape, sizex);
				update_entity_non_uniform_scale(go[i].entity, sizex, sizey, sizez);
				break;
			case SHAPE_CAPSULE:
				shape_init_capsule(&go[i].tsd.block.shape, sizex, sizey);
				quat_init_axis(&capsule_sphere_orientation, 0.0, 1.0, 0.0, M_PI / 2.0);
				update_entity_non_uniform_scale(go[i].entity, sizex, sizey, sizez);
				for (j = 0; j < 2; j++) {
					if (go[i].tsd.block.capsule_sphere[j]) {
						update_entity_non_uniform_scale(go[i].tsd.block.capsule_sphere[j],
							sizey / sizex, 1.0, 1.0);
						update_entity_orientation(go[i].tsd.block.capsule_sphere[j],
							&capsule_sphere_orientation);
					}
				}
				break;
			case SHAPE_CUBOID:
				shape_init_cuboid(&go[i].tsd.block.shape, sizex, sizey, sizez);
				break;
			default:
				update_entity_non_uniform_scale(go[i].entity, sizex, sizey, sizez);
				break;
			}
		}
		return 0;
	}
	switch (form) {
	case SHAPE_SPHERE:
		e = add_entity(ecx, low_poly_sphere_mesh, x, y, z, BLOCK_COLOR);
		break;
	case SHAPE_CUBOID:
		e = add_entity(ecx, unit_cube_mesh, x, y, z, BLOCK_COLOR);
		break;
	case SHAPE_CAPSULE:
		e = add_entity(ecx, cylinder_mesh, x, y, z, BLOCK_COLOR);
		if (!e)
			break;
		break;
	}
	if (!e)
		return -1;
	switch (form) {
	case SHAPE_SPHERE:
		/* half the size for sphere because the radius is 1.0, diameter is 2.0 */
		update_entity_non_uniform_scale(e, 0.5 * sizex, 0.5 * sizey, 0.5 * sizez);
		break;
	case SHAPE_CUBOID:
	case SHAPE_CAPSULE:
	default:
		update_entity_non_uniform_scale(e, sizex, sizey, sizez);
		break;
	}
	if ((block_material_index % 2) == 0)
		update_entity_material(e, &block_material);
	else
		update_entity_material(e, &small_block_material);
	i = add_generic_object(id, timestamp, x, y, z, 0.0, 0.0, 0.0, orientation, OBJTYPE_BLOCK, 1, e);
	if (i < 0)
		return i;
	if (e && form == SHAPE_CAPSULE) {
		for (j = 0; j < 2; j++) {
			go[i].tsd.block.capsule_sphere[j] =
				add_entity(ecx, cylindrically_mapped_sphere_mesh,
					0.5 * (1.0 - (j * 2.0)) * sizex, 0, 0, BLOCK_COLOR);
			if (go[i].tsd.block.capsule_sphere[j]) {
				update_entity_parent(ecx, go[i].tsd.block.capsule_sphere[j], e);
				if ((block_material_index % 2) == 0)
					update_entity_material(go[i].tsd.block.capsule_sphere[j], &block_material);
				else
					update_entity_material(e, &small_block_material);
			}
		}
	}
	switch (go[i].tsd.block.shape.type) {
	case SHAPE_CUBOID:
		shape_init_cuboid(&go[i].tsd.block.shape, sizex, sizey, sizez);
		break;
	case SHAPE_CAPSULE:
		shape_init_capsule(&go[i].tsd.block.shape, sizex, sizey);
		break;
	case SHAPE_SPHERE:
		shape_init_sphere(&go[i].tsd.block.shape, sizex);
		break;
	}
	go[i].tsd.block.health = health;
	return 0;
}

static void update_orientation_history(union quat history[], union quat *new_orientation)
{
	int j;

	/* shift old updates to make room for this one */
	for (j = SNIS_ENTITY_NUPDATE_HISTORY - 1; j >= 1; j--)
		history[j] = history[j-1];
	history[0] = *new_orientation;
}

static int update_turret(uint32_t id, uint32_t timestamp, double x, double y, double z,
			union quat *orientation, union quat *base_orientation, uint8_t health)
{
	int i;
	struct entity *e;
	double vx, vy, vz;

	i = lookup_object_by_id(id);
	if (i >= 0) {
		vx = x - go[i].x; /* verlet integration */
		vy = y - go[i].y;
		vz = z - go[i].z;
		update_generic_object(i, timestamp, x, y, z, vx, vy, vz, orientation, 1);
	} else {
		e = add_entity(ecx, turret_mesh, x, y, z, BLOCK_COLOR);
		if (!e)
			return -1;
		i = add_generic_object(id, timestamp, x, y, z, 0.0, 0.0, 0.0, orientation, OBJTYPE_TURRET, 1, e);
		if (i < 0)
			return i;
		go[i].tsd.turret.turret_base_entity = add_entity(ecx, turret_base_mesh, x, y, z, BLOCK_COLOR);
	}
	go[i].tsd.turret.health = health;
	update_orientation_history(go[i].tsd.turret.base_orientation_history, base_orientation);
	return 0;
}

static int update_asteroid(uint32_t id, uint32_t timestamp, double x, double y, double z)
{
	int i, k, m, s;
	struct entity *e;
	union quat orientation, rotational_velocity;
	struct snis_entity *o;
	union vec3 axis;
	float angle;

	i = lookup_object_by_id(id);
	if (i < 0) {
		/* Note, orientation won't be consistent across clients
		 * due to joining at different times
		 */
		orientation = random_orientation[id % NRANDOM_ORIENTATIONS];
		k = id % (NASTEROID_MODELS * NASTEROID_SCALES);
		m = k % NASTEROID_MODELS;
		s = k % NASTEROID_SCALES;
		e = add_entity(ecx, asteroid_mesh[m], x, y, z, ASTEROID_COLOR);
		if (e) {
			update_entity_scale(e, s ? s * 10 : 3.0);
			update_entity_material(e, &asteroid_material[id % NASTEROID_TEXTURES]);
		}
		i = add_generic_object(id, timestamp, x, y, z, 0, 0, 0,
				&orientation, OBJTYPE_ASTEROID, 1, e);
		if (i < 0)
			return i;
		o = &go[i];

		rotational_velocity = random_spin[id % NRANDOM_SPINS];
		quat_to_axis_v(&rotational_velocity, &axis, &angle);
		if ((i % 20) != 0) /* Most asteroids should spin slowly. Only a few spin rapidly. */
			angle *= 0.2;
		quat_init_axis_v(&rotational_velocity, &axis, angle);
		o->tsd.asteroid.rotational_velocity = rotational_velocity;
	} else {
		update_generic_object(i, timestamp, x, y, z, 0, 0, 0, NULL, 1);
	}
	return 0;
}

static int update_asteroid_minerals(uint32_t id, uint8_t carbon, uint8_t nickeliron,
					uint8_t silicates, uint8_t preciousmetals)
{
	int i;
	struct snis_entity *o;

	i = lookup_object_by_id(id);
	if (i < 0)
		return 0;
	o = &go[i];
	o->tsd.asteroid.carbon = carbon;
	o->tsd.asteroid.nickeliron = nickeliron;
	o->tsd.asteroid.silicates = silicates;
	o->tsd.asteroid.preciousmetals = preciousmetals;
	return 0;
}

static int update_warp_core(uint32_t id, uint32_t timestamp, double x, double y, double z)
{
	int i;
	struct entity *e;
	union quat orientation;
	struct snis_entity *o;

	i = lookup_object_by_id(id);
	if (i < 0) {
		/* Note, orientation won't be consistent across clients
		 * due to joining at different times
		 */
		orientation = random_orientation[id % NRANDOM_ORIENTATIONS];
		e = add_entity(ecx, warp_core_mesh, x, y, z, WARP_CORE_COLOR);
		if (e)
			update_entity_material(e, &warpgate_material); /* re-use warp gate material for now */
		i = add_generic_object(id, timestamp, x, y, z, 0, 0, 0,
				&orientation, OBJTYPE_WARP_CORE, 1, e);
		if (i < 0)
			return i;
		o = &go[i];
		o->tsd.warp_core.rotational_velocity = random_spin[id % NRANDOM_SPINS];
	} else {
		update_generic_object(i, timestamp, x, y, z, 0, 0, 0, NULL, 1);
	}
	return 0;
}

static int update_cargo_container(uint32_t id, uint32_t timestamp, double x, double y, double z,
		uint32_t item, double qty)
{
	int i;
	struct entity *e;
	union quat orientation;
	struct snis_entity *o;

	i = lookup_object_by_id(id);
	if (i < 0) {
		/* Note, orientation potentially won't be consistent across clients
		 * due to joining at different times, and may drift out of sync over time.
		 */
		orientation = random_orientation[id % NRANDOM_ORIENTATIONS];
		e = add_entity(ecx, cargo_container_mesh, x, y, z, CARGO_CONTAINER_COLOR);
		i = add_generic_object(id, timestamp, x, y, z, 0.0, 0.0, 0.0,
				&orientation, OBJTYPE_CARGO_CONTAINER, 1, e);
		if (i < 0)
			return i;
		o = &go[i];
		o->tsd.cargo_container.rotational_velocity = random_spin[id % NRANDOM_SPINS];
		o->tsd.cargo_container.contents.item = item;
		o->tsd.cargo_container.contents.qty = qty;
	} else {
		double vx, vy, vz;

		vx = x - go[i].x;
		vy = y - go[i].y;
		vz = z - go[i].z;
		update_generic_object(i, timestamp, x, y, z, vx, vy, vz, NULL, 1);
		if (item >= 0) {
			go[i].tsd.cargo_container.contents.item = item;
			go[i].tsd.cargo_container.contents.qty = qty;
		}
	}
	return 0;
}

static int update_derelict(uint32_t id, uint32_t timestamp, double x, double y, double z,
				uint8_t ship_kind, uint8_t fuel, uint8_t oxygen, uint32_t orig_ship_id)
{
	int i, m;
	struct entity *e;

	i = lookup_object_by_id(id);
	if (i < 0) {
		m = ship_kind % nshiptypes;
		e = add_entity(ecx, derelict_mesh[m], x, y, z, SHIP_COLOR);
		i = add_generic_object(id, timestamp, x, y, z, 0.0, 0.0, 0.0,
				&identity_quat, OBJTYPE_DERELICT, 1, e);
		if (i < 0)
			return i;
		go[i].tsd.derelict.rotational_velocity = random_spin[id % NRANDOM_SPINS];
	} else {
		update_generic_object(i, timestamp, x, y, z, 0.0, 0.0, 0.0, NULL, 1);
	}
	go[i].tsd.derelict.fuel = fuel;
	go[i].tsd.derelict.oxygen = oxygen;
	go[i].tsd.derelict.orig_ship_id = orig_ship_id;
	return 0;
}

static int update_black_hole(uint32_t id, uint32_t timestamp, double x, double y, double z, double r)
{
	int i;
	struct entity *e;
	union quat orientation = random_orientation[id % NRANDOM_ORIENTATIONS];

	i = lookup_object_by_id(id);
	if (i < 0) {
		e = add_entity(ecx, unit_quad, x, y, z, BLACK_HOLE_COLOR);
		if (e) {
			update_entity_material(e, &black_hole_material);
			update_entity_scale(e, 2.0 * r);
		}
		i = add_generic_object(id, timestamp, x, y, z, 0.0, 0.0, 0.0,
					&orientation, OBJTYPE_BLACK_HOLE, 1, e);
		if (i < 0)
			return i;
		/* Have to set visibility because this normally only gets set on
		 * the first interpolation but because black holes don't move, there
		 * won't be a first interpolation.
		 */
		if (e)
			update_entity_visibility(e, 1);
	} else {
		update_generic_object(i, timestamp, x, y, z, 0.0, 0.0, 0.0, NULL, 1);
	}
	set_object_location(&go[i], x, y, z);
	go[i].tsd.black_hole.radius = r;
	return 0;
}

static void add_planetary_lightning(struct snis_entity *planet);
static void planet_move(struct snis_entity *planet)
{
	if (!planetary_lightning)
		return;
	if (!planet->tsd.planet.atmosphere)
		return;
	if (snis_randn(1000) > 100)
		return;
	add_planetary_lightning(planet);
}

static int update_planet(uint32_t id, uint32_t timestamp, double x, double y, double z,
				union quat *orientation, double r, uint8_t government,
				uint8_t tech_level, uint8_t economy, uint32_t dseed, int hasring,
				uint8_t security, uint16_t contraband,
				uint8_t atm_r,
				uint8_t atm_g,
				uint8_t atm_b,
				double atm_scale,
				double atm_brightness,
				uint8_t has_atmosphere,
				uint16_t atmosphere_type,
				uint8_t solarsystem_planet_type,
				uint8_t ring_selector,
				uint32_t time_left_to_build,
				uint8_t build_unit_type)
{
	int i, m;
	struct entity *e, *atm, *ring;
	union quat rotational_velocity;

	i = lookup_object_by_id(id);
	if (i < 0) {
		quat_init_axis(&rotational_velocity, 0.0, 0.0, 1.0, 0.03 * M_PI / 180.0);

		/* each planet texture has other versions with a variation of the ring materials */
		m = solarsystem_planet_type;
		fprintf(stderr, "zzz %s solarsystem_planet_type = %d\n", solarsystem_name, m);
		if (!hasring) {
			const int ring_materials = (NPLANET_MATERIALS / 2);
			m = m + ring_materials + 6 * (ring_selector % (ring_materials / 6));
		}
		fprintf(stderr, "zzz m = %d, has_atmosphere = %hhu, has ring = %d\n", m, has_atmosphere, hasring);

		e = add_entity(ecx, planet_sphere_mesh, x, y, z, PLANET_COLOR);
		if (e) {
			update_entity_scale(e, r);
			update_entity_material(e, &planet_material[m]);
			entity_set_low_poly_mesh(e, planet_sphere_lp_mesh);
		}


		if (hasring) {
			ring = add_entity(ecx, planetary_ring_mesh, 0, 0, 0, PLANET_COLOR);
			if (ring) {
				update_entity_material(ring,
					planet_material[m].textured_planet.ring_material);

				/* ring must have identity_quat orientation relative to planet
					or ring shadows on planet will not work correctly */
				update_entity_orientation(ring, &identity_quat);

				/* child ring will inherit position and scale from planet */
				update_entity_parent(ecx, ring, e);
				entity_set_low_poly_mesh(ring, planetary_ring_lp_mesh);
			}
		}
		i = add_generic_object(id, timestamp, x, y, z, 0.0, 0.0, 0.0,
					orientation, OBJTYPE_PLANET, 1, e);
		if (i < 0)
			return i;
		go[i].tsd.planet.rotational_velocity = rotational_velocity;
		if (has_atmosphere) {
			atm = add_entity(ecx, atmosphere_mesh, 0.0f, 0.0f, 0.0f, WHITE);
			go[i].tsd.planet.atmosphere = atm;
			if (atm) {
				update_entity_scale(atm, atm_scale);
				entity_update_alpha(atm, 0.5);
				material_init_atmosphere(&go[i].tsd.planet.atm_material);
				go[i].tsd.planet.atm_material.atmosphere.r = (float) atm_r / 255.0f;
				go[i].tsd.planet.atm_material.atmosphere.g = (float) atm_g / 255.0f;
				go[i].tsd.planet.atm_material.atmosphere.b = (float) atm_b / 255.0f;
				go[i].tsd.planet.atm_material.atmosphere.scale = (float) atm_scale;
				go[i].tsd.planet.atm_material.atmosphere.scale = (float) atm_scale;
				go[i].tsd.planet.atm_material.atmosphere.brightness = &atmosphere_brightness;
				go[i].tsd.planet.atm_material.atmosphere.brightness_modifier = atm_brightness;
				if (hasring)
					go[i].tsd.planet.atm_material.atmosphere.ring_material =
						planet_material[m].textured_planet.ring_material;
				else
					go[i].tsd.planet.atm_material.atmosphere.ring_material = NULL;
				update_entity_material(atm, &go[i].tsd.planet.atm_material);
				update_entity_visibility(atm, 1);
				update_entity_parent(ecx, atm, e);
				update_entity_orientation(atm, &identity_quat);
				entity_set_low_poly_mesh(atm, atmosphere_lp_mesh);
			}
		} else {
			go[i].tsd.planet.atmosphere = NULL;
		}
		if (e)
			update_entity_shadecolor(e, (i % NSHADECOLORS) + 1);
	} else {
		update_generic_object(i, timestamp, x, y, z, 0.0, 0.0, 0.0, NULL, 1);
	}
	go[i].tsd.planet.government = government;
	go[i].tsd.planet.tech_level = tech_level;
	go[i].tsd.planet.economy = economy;
	go[i].tsd.planet.description_seed = dseed;
	go[i].tsd.planet.security = security;
	go[i].tsd.planet.radius = r;
	go[i].tsd.planet.contraband = contraband;
	go[i].tsd.planet.atmosphere_r = atm_r;
	go[i].tsd.planet.atmosphere_g = atm_g;
	go[i].tsd.planet.atmosphere_b = atm_b;
	go[i].tsd.planet.atmosphere_scale = atm_scale;
	go[i].tsd.planet.atmosphere_brightness = atm_brightness;
	go[i].tsd.planet.atm_material.atmosphere.brightness_modifier = atm_brightness;
	go[i].tsd.planet.has_atmosphere = has_atmosphere;
	go[i].tsd.planet.atmosphere_type = atmosphere_type;
	go[i].tsd.planet.solarsystem_planet_type = solarsystem_planet_type;
	go[i].tsd.planet.ring_selector = ring_selector;
	go[i].tsd.planet.ring = hasring;
	go[i].tsd.planet.time_left_to_build = time_left_to_build;
	go[i].tsd.planet.build_unit_type = build_unit_type;
	go[i].move = planet_move;
	return 0;
}

static int update_wormhole(uint32_t id, uint32_t timestamp, double x, double y, double z)
{
	int i;
	struct entity *e;
	union quat orientation;

	i = lookup_object_by_id(id);
	if (i < 0) {
		quat_init_axis(&orientation, 1.0, 0.0, 0.0, 0.0);
		e = add_entity(ecx, wormhole_mesh, x, y, z, WORMHOLE_COLOR);
		if (e) {
			set_render_style(e, wormhole_render_style);
			update_entity_material(e, &wormhole_material);
		}
		i = add_generic_object(id, timestamp, x, y, z, 0.0, 0.0, 0.0,
					&orientation, OBJTYPE_WORMHOLE, 1, e);
		if (i < 0)
			return i;
	} else
		update_generic_object(i, timestamp, x, y, z, 0.0, 0.0, 0.0, NULL, 1);
	return 0;
}

static void add_docking_ports_to_starbase(struct snis_entity *sb)
{
	struct docking_port_attachment_point *dp;
	int i, m, starbase_type = (sb->id % nstarbase_models);

	dp = docking_port_info[starbase_type];
	if (!dp)
		return;
	m = dp->docking_port_model;
	if (m == DOCKING_PORT_INVISIBLE_MODEL)
		return;

	for (i = 0; i < dp->nports; i++) {
		struct entity *e;
		union vec3 pos = dp->port[i].pos;
		int m = dp->docking_port_model;
		e = add_entity(ecx, docking_port_mesh[m], pos.v.x, pos.v.y, pos.v.z, SHIP_COLOR);
		if (!e)
			continue;
		update_entity_parent(ecx, e, sb->entity);
		update_entity_orientation(e, &dp->port[i].orientation);
		update_entity_scale(e, dp->port[i].scale);
		update_entity_material(e, &docking_port_material);
	}
}

static int update_starbase(uint32_t id, uint32_t timestamp, double x, double y, double z,
	union quat *orientation, uint8_t occupant[4], uint32_t time_left_to_build, uint8_t build_unit_type,
	uint8_t starbase_number)
{
	int i, m;
	struct entity *e;

	i = lookup_object_by_id(id);
	if (i < 0) {
		m = id % nstarbase_models;
		e = add_entity(ecx, starbase_mesh[m], x, y, z, STARBASE_COLOR);
		i = add_generic_object(id, timestamp, x, y, z, 0.0, 0.0, 0.0,
					orientation, OBJTYPE_STARBASE, 1, e);
		if (i < 0) {
			if (e)
				remove_entity(ecx, e);
			return i;
		}
		add_docking_ports_to_starbase(&go[i]);
	} else {
		update_generic_object(i, timestamp, x, y, z, 0.0, 0.0, 0.0, orientation, 1);
	}
	go[i].tsd.starbase.occupant[0] = occupant[0];
	go[i].tsd.starbase.occupant[1] = occupant[1];
	go[i].tsd.starbase.occupant[2] = occupant[2];
	go[i].tsd.starbase.occupant[3] = occupant[3];
	go[i].tsd.starbase.time_left_to_build = time_left_to_build;
	go[i].tsd.starbase.build_unit_type = build_unit_type;
	go[i].tsd.starbase.starbase_number = starbase_number;
	return 0;
}

static int update_warpgate(uint32_t id, uint32_t timestamp, double x, double y, double z,
	union quat *orientation)
{
	int i;
	struct entity *e, *wge;

	i = lookup_object_by_id(id);
	if (i < 0) {
		e = add_entity(ecx, warpgate_mesh, x, y, z, WARPGATE_COLOR);
		if (e) {
			update_entity_material(e, &warpgate_material);
			i = add_generic_object(id, timestamp, x, y, z, 0.0, 0.0, 0.0,
						orientation, OBJTYPE_WARPGATE, 1, e);
			if (i < 0) {
				remove_entity(ecx, e);
				return i;
			}
			wge = add_entity(ecx, warpgate_effect_mesh, 0, 0, 0, WARPGATE_COLOR);
			if (wge) {
				update_entity_parent(ecx, wge, e);
				update_entity_scale(wge, warpgate_mesh->radius * 0.6);
				update_entity_material(wge, &warpgate_effect_material);
			}
			go[i].tsd.warpgate.warp_gate_effect = wge;
		}
	} else {
		update_generic_object(i, timestamp, x, y, z, 0.0, 0.0, 0.0, orientation, 1);
	}
	return 0;
}

static void nebula_move(struct snis_entity *o)
{
	union quat q;
	float x, y, z, a;
	double ts = universe_timestamp();
	float r;
	const int degree_subdivisions = 16.0; /* make angular quantization less noticeable */
	float angle = (M_PI / 180.0) * ((unsigned int) (o->tsd.nebula.phase_angle * ts * degree_subdivisions *
			o->tsd.nebula.phase_speed) % (degree_subdivisions * 360));
	angle /= (float) degree_subdivisions;
	r = sin(angle) * o->tsd.nebula.r * 0.2 + o->tsd.nebula.r;
	if (o->entity) {
		x = o->tsd.nebula.avx;
		y = o->tsd.nebula.avy;
		z = o->tsd.nebula.avz;
		a = o->tsd.nebula.ava;
		a = a + angle;
		quat_init_axis(&q, x, y, z, a);
		quat_mul_self(&q, &o->tsd.nebula.unrotated_orientation);
		update_entity_scale(o->entity, r * 2.0);
		update_entity_orientation(o->entity, &q);
	}
}

static int update_nebula(uint32_t id, uint32_t timestamp, double x, double y, double z, double r,
			float avx, float avy, float avz, float ava,
			union quat *unrotated_orientation,
			double phase_angle, double phase_speed)
{
	int i;

	i = lookup_object_by_id(id);
	if (i < 0) {
		struct entity *e = add_entity(ecx, nebula_mesh, x, y, z, PLANET_COLOR);
		if (e) {
			update_entity_material(e, &nebula_material[id % NNEBULA_MATERIALS]);
			update_entity_scale(e, r * 2.0);
		}
		i = add_generic_object(id, timestamp, x, y, z, 0.0, 0.0, 0.0,
					&identity_quat, OBJTYPE_NEBULA, 1, e);
		if (i < 0)
			return i;
		go[i].tsd.nebula.avx = avx;
		go[i].tsd.nebula.avy = avy;
		go[i].tsd.nebula.avz = avz;
		go[i].tsd.nebula.ava = ava;
		go[i].tsd.nebula.unrotated_orientation = *unrotated_orientation;
		go[i].tsd.nebula.phase_angle = phase_angle;
		go[i].tsd.nebula.phase_speed = phase_speed;
	} else {
		struct snis_entity *o = &go[i];
		update_generic_object(i, timestamp, x, y, z, 0.0, 0.0, 0.0, NULL, 1);
		if (o->entity)
			update_entity_scale(o->entity, r * 2.0);
	}
	go[i].tsd.nebula.r = r;	
	go[i].alive = 1;
	go[i].move = nebula_move;
	return 0;
}

static void warp_effect_move(struct snis_entity *o)
{
	float t, scale;

	if (o->entity) {
		t = (float) (WARP_EFFECT_LIFETIME - o->alive) / (float) WARP_EFFECT_LIFETIME;
		t = t * M_PI;
		scale = ((sin(t) + 1.0) / 2.0) * WARP_EFFECT_MAX_SIZE;
		update_entity_scale(o->entity, scale);
	}
	if (o->alive)
		o->alive--;
	if (o->alive == 0) {
		remove_entity(ecx, o->entity);
		snis_object_pool_free_object(sparkpool, spark_index(o));
	}
}

static void shield_effect_move(struct snis_entity *o)
{

	int i;

	i = lookup_object_by_id(o->tsd.spark.id);
	if (i < 0) {
		set_object_location(o, o->x + o->vx, o->y + o->vy, o->z + o->vz);
	} else {
		set_object_location(o, go[i].x, go[i].y, go[i].z);
		o->vx = go[i].vx;
		o->vy = go[i].vy;
		o->vz = go[i].vz;
	}
	if (o->alive)
		o->alive--;
	if (o->entity) {
		entity_update_alpha(o->entity, entity_get_alpha(o->entity) * 0.9);
		update_entity_pos(o->entity, o->x, o->y, o->z);
	}
	if (o->tsd.spark.shield_entity) /* don't update shield_entity pos because parent is o->entity */
		entity_update_alpha(o->tsd.spark.shield_entity, entity_get_alpha(o->tsd.spark.shield_entity) * 0.88);
	if (o->alive == 0) {
		if (o->entity) {
			remove_entity(ecx, o->entity);
			o->entity = NULL;
		}
		if (o->tsd.spark.shield_entity) {
			remove_entity(ecx, o->tsd.spark.shield_entity);
			o->tsd.spark.shield_entity = NULL;
		}
		snis_object_pool_free_object(sparkpool, spark_index(o));
	}
}

static void planetary_lightning_move(struct snis_entity *o)
{
	if (o->alive > 1) {
		/* Make the lightning flicker */
		if (o->entity && snis_randn(1000) < 100)
			update_entity_visibility(o->entity, 1);
		else
			update_entity_visibility(o->entity, 0);
		o->alive--;
		return;
	}
	o->alive = 0;
	if (o->entity) {
		remove_entity(ecx, o->entity);
		o->entity = NULL;
	}
	snis_object_pool_free_object(sparkpool, spark_index(o));
	return;
}

static void spark_move(struct snis_entity *o)
{
	union quat orientation;
	float scale;

	set_object_location(o, o->x + o->vx, o->y + o->vy, o->z + o->vz);
	scale = entity_get_scale(o->entity) * o->tsd.spark.shrink_factor;
	if (scale < 0.01)
		o->alive = 0;
	if (o->alive)
		o->alive--;
	if (o->alive == 0) {
		if (o->entity) {
			remove_entity(ecx, o->entity);
			o->entity = NULL;
		}
		if (o->tsd.spark.shield_entity) {
			remove_entity(ecx, o->tsd.spark.shield_entity);
			o->tsd.spark.shield_entity = NULL;
		}
		snis_object_pool_free_object(sparkpool, spark_index(o));
		return;
	}
	/* Apply incremental rotation */
	quat_mul(&orientation, &o->tsd.spark.rotational_velocity, entity_get_orientation(o->entity));
	update_entity_orientation(o->entity, &orientation);
	update_entity_scale(o->entity, scale);
	update_entity_pos(o->entity, o->x, o->y, o->z);
}

static void move_sparks(void)
{
	int i;

	for (i = 0; i <= snis_object_pool_highest_object(sparkpool); i++)
		if (spark[i].alive)
			spark[i].move(&spark[i]);
}

static void spin_wormhole(double timestamp, struct snis_entity *o)
{
	/* -0.5 degree per frame */
	float a = (frame_rate_hz / 30.0) * (-0.5 * M_PI / 180.0);

	/* current rotation is universe timestamp * rotation per timestamp
	   rotational_velocity is frame_rate_hz and universe is 1/10 sec */
	a = a * (float)frame_rate_hz / 10.0 * timestamp;

	quat_init_axis(&o->orientation, 0.0, 0.0, 1.0, a);
	if (o->entity)
		update_entity_orientation(o->entity, &o->orientation);
}

static void arbitrary_spin(double timestamp, struct snis_entity *o, union quat *rotational_velocity)
{
	compute_arbitrary_spin(frame_rate_hz, timestamp, &o->orientation, rotational_velocity);
	if (o->entity)
		update_entity_orientation(o->entity, &o->orientation);
}

static inline void spin_asteroid(double timestamp, struct snis_entity *o)
{
	arbitrary_spin(timestamp, o, &o->tsd.asteroid.rotational_velocity);
}

static inline void spin_warp_core(double timestamp, struct snis_entity *o)
{
	arbitrary_spin(timestamp, o, &o->tsd.warp_core.rotational_velocity);
}

static inline void spin_cargo_container(double timestamp, struct snis_entity *o)
{
	arbitrary_spin(timestamp, o, &o->tsd.cargo_container.rotational_velocity);
}

static inline void spin_derelict(double timestamp, struct snis_entity *o)
{
	arbitrary_spin(timestamp, o, &o->tsd.derelict.rotational_velocity);
}

static inline void spin_planet(double timestamp, struct snis_entity *o)
{
	union quat spun, local_rotation, new_orientation;

	/* For planets, o->orientation is constant, and tsd.planet.rotational_velocity is
	 * some rotational velocity about the Z axis.  We calculate the amount of rotation
	 * for the current time and then transform this into the coord system of the planet
	 * and apply the new orientation to the entity only, leaving o->orientation alone.
	 * Arguably this should be done on the server and o->orientation updated as normal,
	 * but this saves a bit of bandwidth at the cost of being a little too clever.
	 */
	compute_arbitrary_spin(frame_rate_hz, timestamp, &spun, &o->tsd.planet.rotational_velocity);
	quat_conjugate(&local_rotation, &spun, &o->orientation);
	quat_mul(&new_orientation, &local_rotation, &o->orientation);

	/* we do not need to normalize new_orientation because we calculate it
	 * fresh each time from o->orientation which is constant.
	 */
	if (o->entity)
		update_entity_orientation(o->entity, &new_orientation);
}

typedef void(*interpolate_update_func)(double timestamp, struct snis_entity *o, int visible,
	int from_index, int to_index, float t);

static void interpolate_generic_object(double timestamp, struct snis_entity *o, int visible,
	int from_index, int to_index, float t)
{
#ifdef INTERP_DEBUG
	printf("  interpolate_pos_generic_object: from=%d to=%d update_delta=%f, t=%f\n", from_index,
		to_index, o->updatetime[to_index] - o->updatetime[from_index], t);
#endif

	if (from_index == to_index) {
		set_object_location(o, o->r[to_index].v.x, o->r[to_index].v.y, o->r[to_index].v.z);
	} else {
		union vec3 interp_position;
		vec3_lerp(&interp_position, &o->r[from_index], &o->r[to_index], t);
		set_object_location(o, interp_position.v.x, interp_position.v.y, interp_position.v.z);
	}

	if (o->entity) {
		update_entity_visibility(o->entity, visible);
		update_entity_pos(o->entity, o->x, o->y, o->z);
	}
}

static void interpolate_oriented_object(double timestamp, struct snis_entity *o,
	int visible, int from_index, int to_index, float t)
{
#ifdef INTERP_DEBUG
	printf("  interpolate_oriented_generic_object: from=%d to=%d update_delta=%f, t=%f\n", from_index, to_index,
		o->updatetime[to_index] - o->updatetime[from_index], t);
#endif

	if (from_index == to_index) {
		set_object_location(o, o->r[to_index].v.x, o->r[to_index].v.y, o->r[to_index].v.z);
		o->orientation = o->o[to_index];
		if (o->type == OBJTYPE_SHIP1) {
			o->tsd.ship.sciball_orientation = o->tsd.ship.sciball_o[to_index];
			o->tsd.ship.weap_orientation = o->tsd.ship.weap_o[to_index];
		}
	} else {
		/* We used to check for extreme speed here and not interpolate in that
		 * case, but the new warp tunnel means interpolating extreme speed is
		 * now what we actually want.
		 */
		union vec3 interp_position;

		vec3_lerp(&interp_position, &o->r[from_index], &o->r[to_index], t);
		set_object_location(o, interp_position.v.x, interp_position.v.y, interp_position.v.z);

		quat_nlerp(&o->orientation, &o->o[from_index], &o->o[to_index], t);
		o->heading = quat_to_heading(&o->orientation);

		if (o->type == OBJTYPE_SHIP1) {
			quat_nlerp(&o->tsd.ship.sciball_orientation,
					&o->tsd.ship.sciball_o[from_index], &o->tsd.ship.sciball_o[to_index], t);
			quat_nlerp(&o->tsd.ship.weap_orientation,
					&o->tsd.ship.weap_o[from_index], &o->tsd.ship.weap_o[to_index], t);
		} else if (o->type == OBJTYPE_TURRET) {
			quat_nlerp(&o->tsd.turret.base_orientation,
					&o->tsd.turret.base_orientation_history[from_index],
					&o->tsd.turret.base_orientation_history[to_index], t);
			if (o->tsd.turret.turret_base_entity) {
				update_entity_visibility(o->tsd.turret.turret_base_entity, visible);
				update_entity_pos(o->tsd.turret.turret_base_entity, o->x, o->y, o->z);
				update_entity_orientation(o->tsd.turret.turret_base_entity,
								&o->tsd.turret.base_orientation);
			}
		}
	}

	if (o->entity) {
		update_entity_visibility(o->entity, visible);
		update_entity_pos(o->entity, o->x, o->y, o->z);
		update_entity_orientation(o->entity, &o->orientation);
	}
}

static void interpolate_laser(double timestamp, struct snis_entity *o, int visible,
	int from_index, int to_index, float t)
{
	/* do the standard interpolation */
	interpolate_oriented_object(timestamp, o, visible, from_index, to_index, t);

	/* set the scaling based on the object age */
	if (!o->entity)
		return;

	struct mesh *m = entity_get_mesh(o->entity);
	if (!m)
		return;

	/* find out how far the laser has traveled */
	union vec3 pos = { { o->x, o->y, o->z } };
	vec3_sub_self(&pos, &o->tsd.laser.birth_r);
	float dist = vec3_magnitude(&pos);

	/* Make the laser's appearance vary with power, but only for lasers from your
	 * own ship.  That is because otherwise lasers of reasonable power (e.g. not
	 * insta-kill power) would be nearly invisible.
	 */
	float radius_scale = 1.0;
	if (o->tsd.laser.ship_id == my_ship_id)
		radius_scale = fmaxf(0.5, o->tsd.laser.power / 255.0);

	/* laser bolts are forced to 1.0 scale so we can shrink x to distance traveled */
	float length_scale = clampf(dist / m->radius, 0.0, 1.0);
	update_entity_non_uniform_scale(o->entity, length_scale, radius_scale, radius_scale);

#ifdef INTERP_DEBUG
	printf("  interpolate_laser: dist=%f length_scale=%f\n", dist, length_scale);
#endif
}

static void move_object(double timestamp, struct snis_entity *o, interpolate_update_func interp_func)
{
	int nupdates = MIN(SNIS_ENTITY_NUPDATE_HISTORY, o->nupdates);
	int visible = (o->alive > 0); /* default visibility */

	/* interpolate to a point in the past */
	double rendering_offset = 1.5;
	double target_time = timestamp - rendering_offset;

	/* can't do any interpolation with only 1 update, hide until we assume no more updates are coming  */
	if (nupdates <= 1) {
		/* hide this entity until we get another update or the start pause has elapsed */
		visible = visible && (timestamp - o->updatetime[0] > rendering_offset);

#ifdef INTERP_DEBUG
		printf("move_object: not enough updates\n");
		if (visible)
			printf("  showing anyways as update is old\n");
#endif
		interp_func(timestamp, o, visible, 0, 0, 0);
		return;
	}

	/* search in updates to find a "from" index that is in the past and a "to" index that is in the future */
	int from_index = -1;
	int to_index = -1;

	int i;
	for (i = 0; i < nupdates; i++) {
		if (o->updatetime[i] <= target_time) {
			/* found the first older update */
			from_index = i;
			to_index = i - 1;
			break;
		}
	}

	if (from_index < 0) {
		/* all updates are newer, set hidden at oldest update */
#ifdef INTERP_DEBUG
		printf("move_object: all updates newer time=%f so hiding, oldest_update_delta=%f\n", target_time,
			o->updatetime[nupdates-1] - target_time);
#endif
		/* hide until time catches up to the oldtest update */
		interp_func(timestamp, o, 0, nupdates-1, nupdates-1, 0);
		return;
	}

	if (to_index < 0) {
		/* all updates are older, maybe interp into future */
		from_index = 1;
		to_index = 0;

		/* if we are not too far out of date interpolate into the future */
		if (target_time - o->updatetime[to_index] > rendering_offset / 2.0) {
#ifdef INTERP_DEBUG
			printf("move_object: first update too old to interp into future, newest_update_delta=%f\n",
				target_time - o->updatetime[to_index]);
#endif
			interp_func(timestamp, o, visible, to_index, to_index, 0);
			return;
		}

		/* make sure the last update before this isn't too old */
		if (o->updatetime[to_index] - o->updatetime[from_index] > rendering_offset) {
#ifdef INTERP_DEBUG
			printf("move_object: second update to far from first to interp into future, diff=%f\n",
				o->updatetime[to_index] - o->updatetime[from_index]);
#endif
			interp_func(timestamp, o, visible, to_index, to_index, 0);
			return;
		}

#ifdef INTERP_DEBUG
		printf("move_object: first update is older so try into future\n");
#endif
		/* t calculation logic will generate a t>1 which will interp into the future */
	}

	/* make sure the last update before this isn't too old */
	if (o->updatetime[to_index] - o->updatetime[from_index] > rendering_offset) {
#ifdef INTERP_DEBUG
		printf("move_object: updates to far apart to interp, diff=%f\n",
			o->updatetime[to_index] - o->updatetime[from_index]);
#endif
		/* since "to" is in the future we need to wait for that time to be at "to" */
		interp_func(timestamp, o, visible, from_index, from_index, 0);
		return;
	}

#ifdef INTERP_DEBUG
	printf("move_object: target_time=%f\n", target_time);
#endif

	/* calculate where to interpolate to between from and to */
	double t;
	if (o->updatetime[to_index] != o->updatetime[from_index])
		t = (target_time - o->updatetime[from_index]) /
			(o->updatetime[to_index] - o->updatetime[from_index]);
	else
		/* Should not happen, but I've seen it a handful of times at start-up
		 * leading to (temporary) NaNs in object positions.
		 */
		t = 0;
	interp_func(timestamp, o, visible, from_index, to_index, t);
}

static void add_spark(double x, double y, double z, double vx, double vy, double vz, int time, int color,
		struct material *material, float shrink_factor, int debris_chance, float scale_factor);

static void emit_sparks(struct snis_entity *o, double velocity_factor, double size_factor,
			struct material *material, double vxo, double vyo, double vzo)
{
	double vx, vy, vz;
	vx = vxo + velocity_factor * (double) (snis_randn(100) - 50) / 20.0;
	vy = vyo + velocity_factor * (double) (snis_randn(100) - 50) / 20.0;
	vz = vzo + velocity_factor * (double) (snis_randn(100) - 50) / 20.0;

	add_spark(o->x, o->y, o->z, vx, vy, vz, 5, YELLOW, material, 0.95, 0.0, size_factor * 0.25);
}

static void emit_flames(struct snis_entity *o, double velocity_factor, double size_factor)
{
	emit_sparks(o, velocity_factor, size_factor, &spark_material, 0, 0, 0);
}

static void emit_warp_core_sparks(struct snis_entity *o, double velocity_factor, double size_factor)
{
	emit_sparks(o, velocity_factor, size_factor, &warp_effect_material, o->vx, o->vy, o->vz);
}

/* make badly damaged ships "catch on fire" */
static void ship_emit_sparks(struct snis_entity *o)
{

	if (o->type == OBJTYPE_SHIP1)
		return;

	if (o->tsd.ship.damage.shield_damage < 200)
		return;

	if (o->tsd.ship.flames_timer <= 0)
		return;

	o->tsd.ship.flames_timer--;

	emit_flames(o, 1.0, 1.0);
}

static void turret_emit_sparks(struct snis_entity *o)
{
	if (o->type != OBJTYPE_TURRET)
		return;
	if (o->tsd.turret.health > 100)
		return;
	emit_flames(o, 1.0, 1.0);
}

static void block_emit_sparks(struct snis_entity *o)
{
	if (o->type != OBJTYPE_BLOCK)
		return;
	if (o->tsd.block.health > 100)
		return;
	emit_flames(o, 5.0, 1.0);
}

static void update_warp_field_error(void)
{
	if (warp_field_error > 0)
		warp_field_error--;
}

static void warpgate_blink_lights(void)
{
	float u1, u2;
	warpgate_material.texture_mapped.emit_intensity =
		cos(((float) (timer & 0x1f) / 32.0) * 0.5 * M_PI);

	/* Scroll through the warp gate effect texture */
	u1 = (float) (timer & 0x0ff) / (float) 0x0ff;
	u2 = u1 + 0.1;
	if (u2 > 1.0)
		u2 = u2 - 1.0;
	warpgate_effect_material.warp_gate_effect.u1 = u1;
	warpgate_effect_material.warp_gate_effect.u2 = u2;
}

static void docking_port_blink_lights(void)
{
	docking_port_material.texture_mapped.emit_intensity =
		cos(((float) (timer & 0x0f) / 16.0) * 0.5 * M_PI);
}

static void calculate_planetary_altitude(struct snis_entity *o)
{
	int i;
	current_altitude = 1e20;
	struct entity *e;
	struct mesh *m;
	float obj_radius;

	for (i = 0; i <= snis_object_pool_highest_object(pool); i++) {
		const struct snis_entity *p = &go[i];
		if (p->type != OBJTYPE_PLANET)
			continue;
		e = p->entity;
		if (!e)
			continue;
		m = entity_get_mesh(e);
		if (!m)
			continue;
		obj_radius = m->radius * fabs(entity_get_scale(e));
		float a = dist3d(o->x - p->x, o->y - p->y, o->z - p->z) - obj_radius;
		if (a < current_altitude)
			current_altitude = a;
	}
}

/* check if a planet is between two points */
static struct snis_entity *planet_between_points(union vec3 *ray_origin, union vec3 *target)
{
	int i;
	union vec3 ray_direction, sphere_origin;
	float target_dist;
	float planet_dist;

	vec3_sub(&ray_direction, target, ray_origin);
	target_dist = vec3_magnitude(&ray_direction);
	vec3_normalize_self(&ray_direction);

	for (i = 0; i <= snis_object_pool_highest_object(pool); i++) {
		if (!go[i].alive)
			continue;
		if (go[i].type != OBJTYPE_PLANET)
			continue;
		sphere_origin.v.x = go[i].x;
		sphere_origin.v.y = go[i].y;
		sphere_origin.v.z = go[i].z;
		if (!ray_intersects_sphere(ray_origin, &ray_direction,
						&sphere_origin,
						go[i].tsd.planet.radius))
			continue;
		planet_dist = dist3d(sphere_origin.v.x - ray_origin->v.x,
					sphere_origin.v.y - ray_origin->v.y,
					sphere_origin.v.z - ray_origin->v.z);
		if (planet_dist < target_dist) /* planet blocks... */
			return &go[i];
	}
	return NULL; /* no planets blocking */
}

static void update_shading_planet(struct snis_entity *o)
{
	union vec3 p1, p2;

	if (!planets_shade_other_objects) {
		o->shading_planet = NULL;
		return;
	}

	p1.v.x = o->x;
	p1.v.y = o->y;
	p1.v.z = o->z;
	p2.v.x = SUNX;
	p2.v.y = SUNY;
	p2.v.z = SUNZ;
	o->shading_planet = planet_between_points(&p1, &p2);
	if (o->entity)
		entity_set_in_shade(o->entity, (float) 0.9 * (o->shading_planet != NULL) + 0.1);
}

static void move_objects(void)
{
	int i;
	double timestamp = universe_timestamp();

	for (i = 0; i <= snis_object_pool_highest_object(pool); i++) {
		struct snis_entity *o = &go[i];

		if (!snis_object_pool_is_allocated(pool, i))
			continue;
		switch (o->type) {
		case OBJTYPE_SHIP1:
			update_warp_field_error();
			calculate_planetary_altitude(o);
			update_shading_planet(o);
		case OBJTYPE_SHIP2:
			move_object(timestamp, o, &interpolate_oriented_object);
			ship_emit_sparks(o);
			update_shading_planet(o);
			break;
		case OBJTYPE_WORMHOLE:
			move_object(timestamp, o, &interpolate_generic_object);
			spin_wormhole(timestamp, o);
			break;
		case OBJTYPE_STARBASE:
			move_object(timestamp, o, &interpolate_oriented_object);
			update_shading_planet(o);
			break;
		case OBJTYPE_WARPGATE:
			move_object(timestamp, o, &interpolate_oriented_object);
			update_shading_planet(o);
			break;
		case OBJTYPE_SPACEMONSTER:
			move_object(timestamp, o, &interpolate_oriented_object);
			spacemonster_move_tentacles(o);
			update_shading_planet(o);
			break;
		case OBJTYPE_BLOCK:
			move_object(timestamp, o, &interpolate_oriented_object);
			block_emit_sparks(o);
			update_shading_planet(o);
		case OBJTYPE_TURRET:
			move_object(timestamp, o, &interpolate_oriented_object);
			turret_emit_sparks(o);
			update_shading_planet(o);
			break;
		case OBJTYPE_LASER:
			move_object(timestamp, o, &interpolate_laser);
			break;
		case OBJTYPE_TORPEDO:
			move_object(timestamp, o, &interpolate_oriented_object);
			break;
		case OBJTYPE_MISSILE:
			move_object(timestamp, o, &interpolate_oriented_object);
			update_shading_planet(o);
			break;
		case OBJTYPE_ASTEROID:
			move_object(timestamp, o, &interpolate_generic_object);
			spin_asteroid(timestamp, o);
			/* This doesn't work for asteroids for some reason I don't yet understand */
			/* update_shading_planet(o); */
			break;
		case OBJTYPE_WARP_CORE:
			move_object(timestamp, o, &interpolate_generic_object);
			spin_warp_core(timestamp, o);
			emit_warp_core_sparks(o, 0.3, 0.3);
			update_shading_planet(o);
			break;
		case OBJTYPE_CARGO_CONTAINER:
			move_object(timestamp, o, &interpolate_generic_object);
			spin_cargo_container(timestamp, o);
			update_shading_planet(o);
			break;
		case OBJTYPE_DERELICT:
			move_object(timestamp, o, &interpolate_generic_object);
			spin_derelict(timestamp, o);
			update_shading_planet(o);
			break;
		case OBJTYPE_LASERBEAM:
		case OBJTYPE_TRACTORBEAM:
			o->move(o);
			break;
		case OBJTYPE_FLARE:
			move_object(timestamp, o, &interpolate_generic_object);
			break;
		case OBJTYPE_NEBULA:
			/* move_object(timestamp, o, &interpolate_oriented_object); */
			move_object(timestamp, o, &interpolate_generic_object);
			o->move(o);
			break;
		case OBJTYPE_PLANET:
			move_object(timestamp, o, &interpolate_oriented_object);
			spin_planet(timestamp, o);
			o->move(o);
			break;
		case OBJTYPE_SHIELD_EFFECT:
			move_object(timestamp, o, &interpolate_generic_object);
			o->move(o);
			break;
		case OBJTYPE_DOCKING_PORT:
			/* Fallthru.  We do not expect docking port objects on the client.
			 * Docking ports exist on the client only as entities attached to
			 * starbases.
			 */
		default:
			break;
		}
	}
	docking_port_blink_lights();
	warpgate_blink_lights();
}

static void add_spark(double x, double y, double z, double vx, double vy, double vz, int time, int color,
		struct material *material, float shrink_factor, int debris_chance, float scale_factor)
{
	int i, r;
	struct entity *e;
	union quat orientation;

	i = snis_object_pool_alloc_obj(sparkpool);
	if (i < 0)
		return;
	r = snis_randn(100);
	if (r > debris_chance || time < 10) {
		e = add_entity(ecx, particle_mesh, x, y, z, PARTICLE_COLOR);
		if (e) {
			set_render_style(e, spark_render_style);
			update_entity_material(e, material);
			update_entity_scale(e, scale_factor * (float) snis_randn(100) / 25.0f);
		}
	} else if (r > debris_chance + (100 - debris_chance / 2)) {
		e = add_entity(ecx, debris_mesh, x, y, z, color);
	} else {
		e = add_entity(ecx, debris2_mesh, x, y, z, color);
	}
	if (!e) {
		snis_object_pool_free_object(sparkpool, i);
		return;
	}
	memset(&spark[i], 0, sizeof(spark[i]));
	set_object_location(&spark[i], x, y, z);
	spark[i].vx = vx;
	spark[i].vy = vy;
	spark[i].vz = vz;
	spark[i].tsd.spark.shrink_factor = shrink_factor;
	/* calculate a small rotational velocity */
	spark[i].tsd.spark.rotational_velocity = random_spin[i % NRANDOM_SPINS];

	/* Set entity to random orientation */
	orientation = random_orientation[i % NRANDOM_ORIENTATIONS];
	if (e)
		update_entity_orientation(e, &orientation);
	
	spark[i].type = OBJTYPE_SPARK;
	spark[i].alive = time + snis_randn(time);
	spark[i].move = spark_move;
	spark[i].entity = e;
	return;
}

static void add_planetary_lightning(struct snis_entity *planet)
{
	int i;
	union vec3 p, xaxis = { { 0.0, 0.0, 1.0 } };
	union quat orientation;
	struct entity *e;

	i = snis_object_pool_alloc_obj(sparkpool);
	if (i < 0)
		return;
	random_point_on_sphere(planet->tsd.planet.radius * 1.005, &p.v.x, &p.v.y, &p.v.z);
	quat_from_u2v(&orientation, &xaxis, &p, NULL);
	p.v.x += planet->x;
	p.v.y += planet->y;
	p.v.z += planet->z;
	e = add_entity(ecx, planetary_lightning_mesh[i % NLIGHTNINGS], p.v.x, p.v.y, p.v.z, WHITE);
	if (!e) {
		snis_object_pool_free_object(sparkpool, i);
		return;
	}
	update_entity_orientation(e, &orientation);
	update_entity_material(e, &planetary_lightning_material[i % NLIGHTNINGS]);
	update_entity_scale(e, (500.0 + snis_randn(300)) * planet->tsd.planet.radius / MAX_PLANET_RADIUS);
	spark[i].type = OBJTYPE_SPARK;
	spark[i].alive = snis_randn(10) + 10;
	spark[i].move = planetary_lightning_move;
	spark[i].entity = e;
	return;
}

static void add_warp_effect(double x, double y, double z, int arriving, int time,
			union vec3 *direction, float dist)
{
	int i;
	struct entity *e;
	int max_particle_speed = 300;

	i = snis_object_pool_alloc_obj(sparkpool);
	if (i < 0)
		return;
	e = add_entity(ecx, particle_mesh, x, y, z, PARTICLE_COLOR);
	if (e) {
		set_render_style(e, spark_render_style);
		update_entity_material(e, &warp_effect_material);
		update_entity_scale(e, 0.1);
	}
	memset(&spark[i], 0, sizeof(spark[i]));
	set_object_location(&spark[i], x, y, z);
	spark[i].vx = 0;
	spark[i].vy = 0;
	spark[i].vz = 0;
	spark[i].type = OBJTYPE_WARP_EFFECT;
	spark[i].alive = time;
	spark[i].move = warp_effect_move;
	spark[i].entity = e;

	if (max_particle_speed > dist / (time * 6.0))
		max_particle_speed = (int) (dist / (time * 6.0));
	if (!arriving) {
#define NWARP_DEPARTURE_SPARKS 20
		for (i = 0; i < NWARP_DEPARTURE_SPARKS; i++) {
			union vec3 v;
			float speed;

			speed = (float) snis_randn(max_particle_speed);
			vec3_mul(&v, direction, speed);
			add_spark(x, y, z, v.v.x, v.v.y, v.v.z, time * 3, WHITE,
				&warp_effect_material, 0.95, 50, 4.0);
		}
	}
	return;
}

static void add_shield_effect(struct snis_entity *o,
			double radius, union quat *orientation)
{
	int i;
	struct entity *e, *atm;

	i = snis_object_pool_alloc_obj(sparkpool);
	if (i < 0)
		return;
	e = add_entity(ecx, atmosphere_mesh, o->x, o->y, o->z, PARTICLE_COLOR);
	if (e) {
		set_render_style(e, RENDER_NORMAL);
		update_entity_material(e, &shield_material);
		update_entity_scale(e, radius);
		update_entity_orientation(e, orientation);
		update_entity_visibility(e, 1);
		entity_update_alpha(e, 0.25);
	}
	memset(&spark[i], 0, sizeof(spark[i]));
	set_object_location(&spark[i], o->x, o->y, o->z);
	spark[i].vx = o->vx;
	spark[i].vy = o->vy;
	spark[i].vz = o->vz;
	spark[i].type = OBJTYPE_SHIELD_EFFECT;
	spark[i].alive = SHIELD_EFFECT_LIFETIME;
	spark[i].move = shield_effect_move;
	spark[i].entity = e;
	atm = add_entity(ecx, atmosphere_mesh, 0, 0, 0, WHITE);
	if (atm && e) {
		material_init_atmosphere(&spark[i].tsd.spark.atm_material);
		spark[i].tsd.spark.atm_material.atmosphere.r = 0.5;
		spark[i].tsd.spark.atm_material.atmosphere.g = 1.0;
		spark[i].tsd.spark.atm_material.atmosphere.b = 1.0;
		spark[i].tsd.spark.atm_material.atmosphere.scale = 1.05;
		spark[i].tsd.spark.atm_material.atmosphere.brightness = &atmosphere_brightness;
		update_entity_material(atm, &spark[i].tsd.spark.atm_material);
		entity_update_alpha(atm, 0.25);
		update_entity_scale(atm, 1.05);
		update_entity_visibility(atm, 1);
		update_entity_parent(ecx, atm, e);
	}
	spark[i].tsd.spark.shield_entity = atm;
	spark[i].tsd.spark.id = o->id;
	return;
}


static void do_explosion(uint32_t related_id, double x, double y, double z,
				uint16_t nsparks, uint16_t velocity, int time,
				uint8_t victim_type, uint8_t explosion_type)
{
	double angle;
	float v, vx, vy, vz;
	int i, debris_chance, color;
	float spark_multiplier = 1;
	union quat orientation;
	union vec3 vel;
	struct snis_entity *o;

	/* If explosion is near the client, then make more sparks, if far, then fewer.
	 * (If we just make all the explosions have large numbers of sparks, then we
	 * would run out of entities.)
	 */
	o = find_my_ship();
	if (o) {
		float dist, dx, dy, dz;

		dx = o->x - x;
		dy = o->y - y;
		dz = o->z - z;

		dist = sqrt(dx * dx + dy * dy + dz * dz);
		if (dist < XKNOWN_DIM / 20.0)
			spark_multiplier = explosion_multiplier;
		if (dist > XKNOWN_DIM / 5)
			spark_multiplier = 0.2;
	}

	debris_chance = 50;
	switch (victim_type) {
	case OBJTYPE_SHIP1:
	case OBJTYPE_SHIP2:
	case OBJTYPE_DERELICT:
		color = SHIP_COLOR;
		break;
	case OBJTYPE_SPARK:
		color = SHIP_COLOR;
		debris_chance = 0;
		break;
	case OBJTYPE_ASTEROID:
		color = ASTEROID_COLOR;
		break;
	case OBJTYPE_STARBASE:
		color = STARBASE_COLOR;
		break;
	case OBJTYPE_WARPGATE:
		color = WARPGATE_COLOR;
		break;
	default:
		color = GREEN;
		break;
	}

	switch (explosion_type) {
	case EXPLOSION_TYPE_BLACKHOLE:
		if (related_id == (uint32_t) -1) {
			random_quat(&orientation);
		} else {
			i = lookup_object_by_id(related_id);
			if (i < 0)
				random_quat(&orientation);
			else
				orientation = go[i].orientation;
		}
		if (nsparks > 40) /* a big explosion, add one big freakin' stationary spark that fades quickly */
			add_spark(x, y, z, 0, 0, 0, 15, color, &blackhole_spark_material, 0.8, 0, 250.0);
		for (i = 0; i < nsparks; i++) {
			switch (i % 6) {
			case 0: /* Shoot out in mostly +X direction */
				vel.v.x = (35000.0 + snis_randn(1000 * velocity)) / 1000.0;
				vel.v.y = (snis_randn(20.0 * velocity) - 10.0 * velocity) / 1000.0;
				vel.v.z = (snis_randn(20.0 * velocity) - 10.0 * velocity) / 1000.0;
				break;
			case 1: /* Shoot out in mostly -X direction */
				vel.v.x = -(35000.0 + snis_randn(1000 * velocity)) / 1000.0;
				vel.v.y = (snis_randn(20.0 * velocity) - 10.0 * velocity) / 1000.0;
				vel.v.z = (snis_randn(20.0 * velocity) - 10.0 * velocity) / 1000.0;
				break;
			case 2: /* Shoot out radially in Y-Z plane, mostly */
			default:
				angle = (snis_randn(3600) / 10.0) * M_PI / 180.0;
				v = (65000.0 + snis_randn(500 * velocity)) / 1000.0;
				vel.v.x = (snis_randn(30.0 * velocity) - 15.0 * velocity) / 1000.0;
				vel.v.y = sin(angle) * v;
				vel.v.z = cos(angle) * v;
				break;
			}
			quat_rot_vec_self(&vel, &orientation);
			add_spark(x, y, z, vel.v.x, vel.v.y, vel.v.z, time, color, &blackhole_spark_material,
					0.95, 0.0, 20.0);
		}
		break;
	case EXPLOSION_TYPE_REGULAR:
	default:
		if (nsparks > 40) { /* a big explosion, add one big freakin' stationary spark that fades quickly */
			add_spark(x, y, z, 0, 0, 0, 15, color, &spark_material, 0.8, 0, 250.0);
			nsparks *= spark_multiplier;
		}
		for (i = 0; i < nsparks; i++) {
			v = snis_randn(velocity * 2) - velocity;
			random_point_on_sphere(v, &vx, &vy, &vz);
			add_spark(x, y, z, vx, vy, vz, time, color, &spark_material, 0.98, debris_chance, 1.0);
		}
		break;
	}
}

static int update_explosion(uint32_t id, uint32_t timestamp, uint32_t related_id,
		double x, double y, double z,
		uint16_t nsparks, uint16_t velocity, uint16_t time, uint8_t victim_type,
		uint8_t explosion_type)
{
	int i;
	i = lookup_object_by_id(id);
	if (i < 0) {
		i = add_generic_object(id, timestamp, x, y, z, 0.0, 0.0, 0.0,
					&identity_quat, OBJTYPE_EXPLOSION, 1, NULL);
		if (i < 0)
			return i;
		go[i].tsd.explosion.nsparks = nsparks;
		go[i].tsd.explosion.velocity = velocity;
		go[i].tsd.explosion.explosion_type = explosion_type;
		go[i].tsd.explosion.related_id = related_id;
		do_explosion(related_id, x, y, z, nsparks, velocity, (int) time, victim_type, explosion_type);
	}
	return 0;
}

static void scale_points(struct my_point_t *points, int npoints,
			double xscale, double yscale)
{
	int i;

	for (i = 0; i < npoints; i++) {
		if (points[i].x == LINE_BREAK)
			continue;
		if (points[i].x == COLOR_CHANGE)
			continue;
		points[i].x = (int) (xscale * points[i].x);
		points[i].y = (int) (yscale * points[i].y);
	} 
}

static void wakeup_gameserver_writer(void);

void queue_to_server(struct packed_buffer *pb)
{
	if (!pb) {
		stacktrace("snis_client: NULL packed_buffer in queue_to_server()");
		return;
	}
	packed_buffer_queue_add(&to_server_queue, pb, &to_server_queue_mutex);
	wakeup_gameserver_writer();
}

static void request_sci_select_target(uint8_t selection_type, uint32_t id)
{
	queue_to_server(snis_opcode_pkt("bbw", OPCODE_SCI_SELECT_TARGET, selection_type, id));
}

static void request_navigation_yaw_packet(uint8_t yaw)
{
	queue_to_server(snis_opcode_pkt("bb", OPCODE_REQUEST_YAW, yaw));
}

static void request_navigation_pitch_packet(uint8_t pitch)
{
	queue_to_server(snis_opcode_pkt("bb", OPCODE_REQUEST_PITCH, pitch));
}

static void request_navigation_roll_packet(uint8_t roll)
{
	queue_to_server(snis_opcode_pkt("bb", OPCODE_REQUEST_ROLL, roll));
}

static int control_key_pressed = 0;

static void external_camera_dirkey(int h, int v, int r)
{
	float yaw, pitch, roll;
	union vec3 motion;

	if (!control_key_pressed) { /* control key not pressed, rotate */
		yaw = -h * 2.0 * M_PI / 180.0;
		pitch = v * 2.0 * M_PI / 180.0;
		roll = -r * 2.0 * M_PI / 180.0;

		quat_apply_relative_yaw_pitch_roll(&desired_external_camera_orientation, yaw, pitch, roll);
	} else { /* control key pressed, move */
		motion.v.x = -v * 5.0;
		motion.v.y = r * 5.0;
		motion.v.z = h * 5.0;
		quat_rot_vec_self(&motion, &external_camera_orientation);
		vec3_add_self(&external_camera_velocity, &motion);
	}
}

static void navigation_dirkey(int h, int v, int r)
{
	uint8_t yaw, pitch, roll;
	static int last_time = 0;
	int fine;

	fine = 2 * (timer - last_time > 5);
	last_time = timer;

	if (!h && !v && !r)
		return;

	if (h) {
		yaw = h < 0 ? YAW_LEFT + fine : YAW_RIGHT + fine;
		request_navigation_yaw_packet(yaw);
	}
	if (v) {
		pitch = v < 0 ? PITCH_BACK + fine : PITCH_FORWARD + fine;
		request_navigation_pitch_packet(pitch);
	}
	if (r) {
		roll = r < 0 ? ROLL_LEFT + fine : ROLL_RIGHT + fine;
		request_navigation_roll_packet(roll);
	}
}

static double timeval_difference(struct timeval t1, struct timeval t2)
{
			double elapsed_time;
	/* compute and return the elapsed time in millisec */
	    elapsed_time = (t2.tv_sec - t1.tv_sec) * 1000.0;      /* sec to ms */
	    elapsed_time += (t2.tv_usec - t1.tv_usec) / 1000.0;   /* us to ms */
	    return elapsed_time;
}

struct mouse_button_state {
	int state;
#define MOUSE_BUTTON_NO_ACTIVITY 0
#define MOUSE_BUTTON_PRESSED 1
#define MOUSE_BUTTON_HELD 2
#define MOUSE_BUTTON_RELEASED (-1)
	struct timeval time_pressed;
	double press_x, press_y;
	double release_x, release_y;
};

static struct mouse_state {
	int x, y;
	int deltax, deltay; /* for captured mouse on weapons screen */
	struct mouse_button_state button[3];
} mouse;

static void request_demon_rot_packet(uint32_t oid, uint8_t kind, uint8_t amount)
{
	queue_to_server(snis_opcode_subcode_pkt("bbwb", OPCODE_DEMON_ROT, kind, oid, amount));
}

static void request_demon_thrust_packet(uint32_t oid, uint8_t thrust)
{
	queue_to_server(snis_opcode_pkt("bwb",
				OPCODE_DEMON_THRUST, oid, thrust));
}

static struct demon_ui {
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
	struct pull_down_menu *menu;
	struct text_window *console;
	char input[100];
	char error_msg[80];
	double ix, iz, ix2, iz2;
	int captain_of;
	int follow_id;
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
} demon_ui;

static void home_demon_camera(void)
{
	union vec3 right = { { 1.0f, 0.0f, 0.0f } };
	const float homex = 0;
	const float homey = YKNOWN_DIM * 7.0;
	const float homez = 0;
	const union vec3 camera_pos = { { homex, homey, homez, } };
	union vec3 up = { { 0.0, 1.0, 0.0, } };
	union vec3 camera_lookat = { { 0, 0, 0 } };
	vec3_sub_self(&camera_lookat, &camera_pos);

	demon_ui.desired_camera_pos = camera_pos;
	quat_from_u2v(&demon_ui.desired_camera_orientation, &right, &camera_lookat, &up);
}

static void demon_dirkey(int h, int v, int r, int t)
{
	uint8_t yaw, pitch, roll, thrust;
	static int last_time = 0;
	uint32_t oid;
	int fine;

	if (demon_ui.captain_of < 0)
		return;
	if (go[demon_ui.captain_of].type != OBJTYPE_SHIP2)
		return;
	oid = go[demon_ui.captain_of].id;

	fine = 2 * (timer - last_time > 5);
	last_time = timer;

	if (!h && !v && !r && !t)
		return;

	if (h) {
		yaw = h < 0 ? YAW_LEFT + fine : YAW_RIGHT + fine;
		request_demon_rot_packet(oid, OPCODE_DEMON_ROT_YAW, yaw);
	}
	if (v) {
		pitch = v < 0 ? PITCH_BACK + fine : PITCH_FORWARD + fine;
		request_demon_rot_packet(oid, OPCODE_DEMON_ROT_PITCH, pitch);
	}
	if (r) {
		roll = r < 0 ? ROLL_LEFT + fine : ROLL_RIGHT + fine;
		request_demon_rot_packet(oid, OPCODE_DEMON_ROT_ROLL, roll);
	}
	if (t) {
		thrust = THRUST_FORWARDS;
		request_demon_thrust_packet(oid, thrust);
	}
}

static void request_weapons_manual_yaw_packet(uint8_t yaw)
{
	queue_to_server(snis_opcode_pkt("bb", OPCODE_REQUEST_MANUAL_GUNYAW, yaw));
}

static void request_weapons_manual_pitch_packet(uint8_t pitch)
{
	queue_to_server(snis_opcode_pkt("bb", OPCODE_REQUEST_MANUAL_GUNPITCH, pitch));
}

static struct weapons_ui {
	struct gauge *phaser_bank_gauge;
	struct gauge *phaser_wavelength;
	struct slider *wavelen_slider;
	struct button *custom_button;
} weapons;

static void draw_plane_radar(GtkWidget *w, struct snis_entity *o, union quat *aim, float cx, float cy, float r, float range)
{
	int i;

	/* draw background overlay */
	sng_set_foreground_alpha(BLACK, 0.75);
	sng_draw_circle(1, cx, cy, r);
	sng_set_foreground(UI_COLOR(weap_radar));
	sng_draw_circle(0, cx, cy, r);
	for (i=0; i<4; i++) {
		float angle = i * M_PI / 2.0 + M_PI / 4.0;
		snis_draw_line(cx + cos(angle) * r, cy - sin(angle) * r,
				cx + 0.5 * cos(angle) * r, cy - 0.5 * sin(angle) * r);
		snis_draw_arc(0, cx-0.5*r, cy-0.5*r, r, r, angle - M_PI/8.0, angle + M_PI/8.0);
	}

	if ((timer & 0x07) < 4)
		return;

	float range2 = range*range;
	union vec3 ship_pos = {{o->x, o->y, o->z}};

	union quat aim_inverse;
	quat_inverse(&aim_inverse, aim);

	for (i = 0; i <= snis_object_pool_highest_object(pool); i++) {
		if (!go[i].alive)
			continue;

		if (go[i].id == my_ship_id)
			continue; /* skip drawing yourself. */

		if (go[i].type != OBJTYPE_SHIP1 &&
			go[i].type != OBJTYPE_SHIP2 &&
			go[i].type != OBJTYPE_STARBASE &&
			go[i].type != OBJTYPE_WARPGATE &&
			go[i].type != OBJTYPE_ASTEROID &&
			go[i].type != OBJTYPE_SPACEMONSTER &&
			go[i].type != OBJTYPE_FLARE &&
			go[i].type != OBJTYPE_CARGO_CONTAINER)
		{
			continue;
		}

		float dist2 = object_dist2(&go[i], o);
		if (dist2 > range2)
			continue; /* not close enough */

		float dist = sqrt(dist2);
		if (dist > range)
			continue;

		union vec3 contact_pos = {{go[i].x, go[i].y, go[i].z}};

		/* get direction vector, rotate back to basis axis, then normalize */
		union vec3 dir;
		vec3_sub(&dir, &contact_pos, &ship_pos);
		quat_rot_vec_self(&dir, &aim_inverse);
		vec3_normalize(&dir, &dir);

		/* dist to forward vector */
		float d = dist3d(dir.v.x - 1.0, dir.v.y, dir.v.z) / 2.0;
		/* angle off center when looking towards +x */
		float twist = atan2(dir.v.y, dir.v.z);

		float sx = cx + r * cos(twist) * d * 0.98;
		float sy = cy - r * sin(twist) * d * 0.98;

		if (dist < TORPEDO_VELOCITY * TORPEDO_LIFETIME)
			sng_set_foreground(UI_COLOR(weap_in_range));
		else
			sng_set_foreground(UI_COLOR(weap_targeting));
		snis_draw_line(sx, sy - 2, sx, sy + 2);
		snis_draw_line(sx - 2, sy, sx + 2, sy);

		if (curr_science_guy == &go[i]) {
			sng_set_foreground(UI_COLOR(weap_sci_selection));
			snis_draw_rectangle(0, sx-2, sy-2, 4, 4);
		}
	}
}

static void wavelen_updown_button_pressed(int direction);
static void weapons_dirkey(int h, int v)
{
	static int last_time = 0;
	int fine;
	uint8_t yaw, pitch;

	if (!h && !v)
		return;

	fine = 2 * (timer - last_time > 5);
	last_time = timer;

	if (h) {
		yaw = h < 0 ? YAW_LEFT + fine : YAW_RIGHT + fine;
		request_weapons_manual_yaw_packet(yaw);
	}
	if (v) {
		pitch = v < 0 ? PITCH_BACK + fine : PITCH_FORWARD + fine;
		request_weapons_manual_pitch_packet(pitch);
	}
}

static void science_dirkey(int h, int v)
{
	uint8_t yaw;
	int fine;

	if (!h && !v)
		return;
	fine = 2 * control_key_pressed;
	if (v) {
		yaw = v < 0 ? YAW_LEFT : YAW_RIGHT;
		queue_to_server(snis_opcode_pkt("bb",
				OPCODE_REQUEST_SCIBEAMWIDTH, yaw + fine));
	}
	if (h) {
		yaw = h < 0 ? YAW_LEFT : YAW_RIGHT;
		queue_to_server(snis_opcode_pkt("bb",
				OPCODE_REQUEST_SCIYAW, yaw + fine));
	}
}

static void damcon_dirkey(int h, int v)
{
	uint8_t yaw, thrust;

	if (!h && !v)
		return;
	if (v) {
		thrust = v < 0 ? THRUST_BACKWARDS : THRUST_FORWARDS;
		queue_to_server(snis_opcode_pkt("bb",
				OPCODE_REQUEST_ROBOT_THRUST, thrust));
	}
	if (h) {
		yaw = h > 0 ? YAW_LEFT : YAW_RIGHT;
		queue_to_server(snis_opcode_pkt("bb",
				OPCODE_REQUEST_ROBOT_YAW, yaw));
	}
	wakeup_gameserver_writer();
}

static void do_onscreen(uint8_t mode)
{
	queue_to_server(snis_opcode_pkt("bb", OPCODE_ROLE_ONSCREEN, mode));
}

static void do_view_mode_change()
{
	uint8_t new_mode;
	struct snis_entity *o;

	if (!(o = find_my_ship()))
		return;
	if (o->tsd.ship.view_mode == MAINSCREEN_VIEW_MODE_NORMAL)
		new_mode = MAINSCREEN_VIEW_MODE_WEAPONS;
	else
		new_mode = MAINSCREEN_VIEW_MODE_NORMAL;
	queue_to_server(snis_opcode_pkt("bRb", OPCODE_MAINSCREEN_VIEW_MODE,
				0.0, new_mode));
}

static void comms_dirkey(int h, int v);
static inline int nav_ui_computer_active(void);

static void do_dirkey(int h, int v, int r, int t)
{
	v = v * vertical_controls_inverted;

	if (in_the_process_of_quitting) {
		if (h < 0)
			current_quit_selection = 1;
		if (h > 0)
			current_quit_selection = 0;
		return;
	}

	switch (displaymode) {
		case DISPLAYMODE_MAINSCREEN:
			if (external_camera_active) {
				external_camera_dirkey(h, v, r);
				break;
			}
			/* Deliberate fallthrough here */
		case DISPLAYMODE_NAVIGATION:
			if (nav_ui_computer_active()) /* suppress keystrokes typed to computer */
				break;
			navigation_dirkey(h, v, r); 
			break;
		case DISPLAYMODE_WEAPONS:
			weapons_dirkey(h, v); 
			break;
		case DISPLAYMODE_SCIENCE:
			science_dirkey(h, -v); 
			break;
		case DISPLAYMODE_DAMCON:
			damcon_dirkey(h, v);
			break;
		case DISPLAYMODE_DEMON:
			demon_dirkey(h, v, r, t);
			break;
		case DISPLAYMODE_COMMS:
			comms_dirkey(h, v);
			break;
		default:
			break;
	}
	return;
}

static void do_sciball_dirkey(int h, int v, int r)
{
	uint8_t value;
	int fine = 2 * control_key_pressed;

	v = v * vertical_controls_inverted;

	switch (displaymode) {
		case DISPLAYMODE_SCIENCE:
			if (!h && !v && !r)
				return;
			if (v) {
				value = v < 0 ? YAW_LEFT : YAW_RIGHT;
				queue_to_server(snis_opcode_pkt("bb",
						OPCODE_REQUEST_SCIBALL_PITCH, value + fine));
			}
			if (h) {
				value = h < 0 ? YAW_LEFT : YAW_RIGHT;
				queue_to_server(snis_opcode_pkt("bb",
						OPCODE_REQUEST_SCIBALL_YAW, value + fine));
			}
			if (r) {
				value = r < 0 ? YAW_LEFT : YAW_RIGHT;
				queue_to_server(snis_opcode_pkt("bb",
						OPCODE_REQUEST_SCIBALL_ROLL, value + fine));
			}
			break;
		default:
			break;
	}
	return;
}

static void do_torpedo(void)
{
	struct snis_entity *o;

	if (!(o = find_my_ship()))
		return;
	if (o->tsd.ship.torpedoes_loaded <= 0)
		return;
	queue_to_server(snis_opcode_pkt("b", OPCODE_REQUEST_TORPEDO));
}

static void transmit_adjust_control_input(uint8_t value,  uint8_t subcode);
static void fire_missile_button_pressed(__attribute__((unused)) void *notused)
{
	transmit_adjust_control_input(1, OPCODE_ADJUST_CONTROL_FIRE_MISSILE);
}

static void do_mainscreen_camera_mode()
{
	if (displaymode != DISPLAYMODE_MAINSCREEN)
		return;
	queue_to_server(snis_opcode_pkt("bb", OPCODE_CYCLE_MAINSCREEN_POINT_OF_VIEW,
		(unsigned char) (camera_mode + 1) % 3));
}

static void do_nav_camera_mode()
{
	if (displaymode != DISPLAYMODE_NAVIGATION)
		return;
	queue_to_server(snis_opcode_pkt("bb", OPCODE_CYCLE_NAV_POINT_OF_VIEW,
		(unsigned char) (nav_camera_mode + 1) % 5));
}

static void do_laser(void)
{
	queue_to_server(snis_opcode_pkt("b", OPCODE_REQUEST_MANUAL_LASER));
}

static void robot_gripper_button_pressed(void *x);
static void robot_backward_button_pressed(void *x);
static void robot_forward_button_pressed(void *x);
static void robot_left_button_pressed(void *x);
static void robot_right_button_pressed(void *x);
static void robot_auto_button_pressed(void *x);
static void robot_manual_button_pressed(void *x);
static void fire_phaser_button_pressed(__attribute__((unused)) void *notused);
static void fire_torpedo_button_pressed(__attribute__((unused)) void *notused);
static void fire_missile_button_pressed(__attribute__((unused)) void *notused);
static void load_torpedo_button_pressed();

static void do_joystick_engage_warp(__attribute__((unused)) void *notused)
{
	snis_button_trigger_button(nav_ui.engage_warp_button);
}

static void do_joystick_standard_orbit(__attribute__((unused)) void *notused)
{
	snis_button_trigger_button(nav_ui.standard_orbit_button);
}

static void do_joystick_docking_magnets(__attribute__((unused)) void *notused)
{
	snis_button_trigger_button(nav_ui.docking_magnets_button);
}

static void do_joystick_lights(__attribute__((unused)) void *notused)
{
	snis_button_trigger_button(nav_ui.lights_button);
}

static void do_joystick_attitude_indicator(__attribute__((unused)) void *notused)
{
	snis_button_trigger_button(nav_ui.trident_button);
}

static void do_joystick_starmap(__attribute__((unused)) void *notused)
{
	snis_button_trigger_button(nav_ui.starmap_button);
}

static void do_joystick_reverse(__attribute__((unused)) void *notused)
{
	snis_button_trigger_button(nav_ui.reverse_button);
}

static void do_joystick_nudge_warp_up(__attribute__((unused)) void *notused)
{
	snis_slider_nudge(nav_ui.warp_slider, 0.05, 0);
}

static void do_joystick_nudge_warp_down(__attribute__((unused)) void *notused)
{
	snis_slider_nudge(nav_ui.warp_slider, -0.05, 0);
}

static void do_joystick_nav_nudge_zoom_down(__attribute__((unused)) void *notused)
{
	snis_slider_nudge(nav_ui.navzoom_slider, -0.05, 0);
}

static void do_joystick_nav_change_pov(__attribute__((unused)) void *notused)
{
	do_nav_camera_mode();
}

static void do_joystick_nav_nudge_zoom_up(__attribute__((unused)) void *notused)
{
	snis_slider_nudge(nav_ui.navzoom_slider, 0.05, 0);
}

static void do_joystick_weapons_wavelength_up(__attribute__((unused)) void *notused)
{
	snis_slider_nudge(weapons.wavelen_slider, 0.05, 0);
}

static void do_joystick_weapons_wavelength_down(__attribute__((unused)) void *notused)
{
	snis_slider_nudge(weapons.wavelen_slider, -0.05, 0);
}

static void do_joystick_torpedo(__attribute__((unused)) void *x)
{
	fire_torpedo_button_pressed(NULL);
}

static void do_joystick_missile(__attribute__((unused)) void *x)
{
	fire_missile_button_pressed(NULL);
}

static void do_joystick_pitch(__attribute__((unused)) void *x, int value)
{
	if (value < -yjoystick_threshold)
		request_navigation_pitch_packet(PITCH_BACK);
	else if (value > yjoystick_threshold)
		request_navigation_pitch_packet(PITCH_FORWARD);
	else if (value < 0)
		request_navigation_pitch_packet(PITCH_BACK + 2);
	else if (value > 0)
		request_navigation_pitch_packet(PITCH_FORWARD + 2);
}

static void do_joystick_damcon_pitch(__attribute__((unused)) void *x, int value)
{
	if (value < -yjoystick_threshold)
		robot_forward_button_pressed(NULL);
	else if (value > yjoystick_threshold)
		robot_backward_button_pressed(NULL);
}

static void do_joystick_damcon_roll(__attribute__((unused)) void *x, int value)
{
	if (value < -xjoystick_threshold)
		robot_left_button_pressed(NULL);
	else if (value > xjoystick_threshold)
		robot_right_button_pressed(NULL);
}

static void do_joystick_axis_to_slider(struct slider *s, int value)
{
	double v = ((double) -value + JOYSTICK_AXIS_MAX) / (double)(JOYSTICK_AXIS_MAX * 2.0);
	double n = snis_slider_get_input(s);
	if (fabs(n - v) > 0.01) {
		snis_slider_poke_input(s, v, 0);
	}
}

static void do_joystick_throttle(__attribute__((unused)) void *x, int value)
{
	do_joystick_axis_to_slider(nav_ui.throttle_slider, value);
}

static void do_joystick_warp(__attribute__((unused)) void *x, int value)
{
	do_joystick_axis_to_slider(nav_ui.warp_slider, value);
}

static void do_joystick_weapons_wavelength(__attribute__((unused)) void *x, int value)
{
	do_joystick_axis_to_slider(weapons.wavelen_slider, value);
}

static void do_joystick_weapons_pitch(__attribute__((unused)) void *x, int value)
{
	if (value < -yjoystick_threshold)
		request_weapons_manual_pitch_packet(PITCH_BACK);
	else if (value > yjoystick_threshold)
		request_weapons_manual_pitch_packet(PITCH_FORWARD);
	else if (value < 0)
		request_weapons_manual_pitch_packet(PITCH_BACK + 2);
	else if (value > 0)
		request_weapons_manual_pitch_packet(PITCH_FORWARD + 2);
}


static void do_joystick_yaw(__attribute__((unused)) void *x, int value)
{
	if (value < -xjoystick_threshold)
		request_navigation_yaw_packet(YAW_LEFT);
	else if (value > xjoystick_threshold)
		request_navigation_yaw_packet(YAW_RIGHT);
	else if (value < 0)
		request_navigation_yaw_packet(YAW_LEFT + 2);
	else if (value > 0)
		request_navigation_yaw_packet(YAW_RIGHT + 2);
}

static void do_joystick_weapons_yaw(__attribute__((unused)) void *x, int value)
{
	if (value < -xjoystick_threshold)
		request_weapons_manual_yaw_packet(YAW_LEFT);
	else if (value > xjoystick_threshold)
		request_weapons_manual_yaw_packet(YAW_RIGHT);
	else if (value < 0)
		request_weapons_manual_yaw_packet(YAW_LEFT + 2);
	else if (value > 0)
		request_weapons_manual_yaw_packet(YAW_RIGHT + 2);
}


static void do_joystick_roll(__attribute__((unused)) void *x, int value)
{
	if (value < -xjoystick_threshold)
		request_navigation_roll_packet(ROLL_RIGHT);
	else if (value > xjoystick_threshold)
		request_navigation_roll_packet(ROLL_LEFT);
	else if (value < 0)
		request_navigation_roll_packet(ROLL_RIGHT + 2);
	else if (value > 0)
		request_navigation_roll_packet(ROLL_LEFT + 2);
}

/* client joystick status */
static struct js_state jss[MAX_JOYSTICKS] = { { { 0 } } };

static void deal_with_joysticks()
{

	int joystick_throttle = 3;
	int i, j, rc;

	if (!joystick_cfg)
		return;

	for (j = 0; j < njoysticks; j++) {
		if (joystick_fd[j] < 0) /* no joystick */
			continue;

		/* Read events even if we don't use them just to consume them. */
		memset(&jss[j].button, 0, sizeof(jss[j].button));
		rc = get_joystick_state(joystick_fd[j], &jss[j]);
		if (rc != 0)
			continue;

		/* If not on screen which uses joystick, ignore stick */
		if (displaymode != DISPLAYMODE_DAMCON &&
			displaymode != DISPLAYMODE_WEAPONS &&
			displaymode != DISPLAYMODE_NAVIGATION &&
			displaymode != DISPLAYMODE_MAINSCREEN)
			continue;

		/* Fire off joystick button callbacks registered via set_joystick_button_fn() */
		for (i = 0; i < ARRAYSIZE(jss[j].button); i++)
			if (jss[j].button[i] == 1)
				joystick_button(joystick_cfg, NULL, displaymode, j, i);

		/* Throttle back the joystick axis input rate to avoid flooding network */
		if (timer % joystick_throttle != 0)
			return;

		/* Fire off joystick axis callbacks registered via set_joystick_axis_fn() */
		for (i = 0; i < ARRAYSIZE(jss[j].axis); i++)
			joystick_axis(joystick_cfg, NULL, displaymode, j, i, jss[j].axis[i]);
	}
}

static void deal_with_physical_io_devices()
{
	/* FIXME: fill this in. */
}

static int mouse_button_held(int button);
static void deal_with_mouse()
{
	int i;
	/* The majority of mouse events are dealt with in main_da_button_press
	 * and main_da_motion_notify. This uses the information collected in
	 * said functions to provide greater functionality to the mouse keys
	 */

	struct timeval time_now;
	gettimeofday(&time_now, NULL);

	for (i = 0; i < 3; i++)
		if (mouse.button[i].state == MOUSE_BUTTON_PRESSED)
			if (timeval_difference(mouse.button[i].time_pressed, time_now) > BUTTON_HOLD_INTERVAL)
				mouse.button[i].state = MOUSE_BUTTON_HELD;
	for (i = 0; i < 3; i++)
		if (mouse.button[i].state == MOUSE_BUTTON_HELD)
			mouse_button_held(i);
}

static void do_adjust_byte_value(uint8_t value,  uint8_t opcode);
static void do_zoom(int z)
{
	int newval;
	struct snis_entity *o;

	if (!(o = find_my_ship()))
		return;

	switch (displaymode) {
	case DISPLAYMODE_WEAPONS:
		wavelen_updown_button_pressed(z);
		break;
	case DISPLAYMODE_NAVIGATION:
		newval = o->tsd.ship.navzoom + z;
		if (newval < 0)
			newval = 0;
		if (newval > 255)
			newval = 255;
		transmit_adjust_control_input((uint8_t) newval, OPCODE_ADJUST_CONTROL_NAVZOOM);
		break;
	case DISPLAYMODE_MAINSCREEN:
		newval = o->tsd.ship.mainzoom + z;
		if (newval < 0)
			newval = 0;
		if (newval > 255)
			newval = 255;
		transmit_adjust_control_input((uint8_t) newval, OPCODE_ADJUST_CONTROL_MAINZOOM);
		break;
	default:
		break;
	}
}

static void do_pageup(void);
static void do_pagedown(void);
static void sci_mining_bot_pressed(void *x);
static void sci_tractor_pressed(void *x);
static void sci_threed_pressed(void *x);
static void sci_sciplane_pressed(void *x);
static void sci_waypoints_pressed(void *x);
static void sci_details_pressed(void *x);

static struct engineering_ui {
	struct gauge *fuel_gauge;
	struct gauge *amp_gauge;
	struct gauge *voltage_gauge;
	struct gauge *temp_gauge;
	struct gauge *oxygen_gauge;
	struct button *damcon_button;
	struct button *preset_buttons[ENG_PRESET_NUMBER];
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
} eng_ui;

static void deal_with_keyboard()
{
	int h, v, z, r, t;
	int sbh, sbv, sbr; /* sciball keys */

	int keyboard_throttle = 3;
	if (timer % keyboard_throttle != 0)
		return;

	h = 0;
	v = 0;
	r = 0;
	z = 0;
	t = 0;
	sbh = 0;
	sbv = 0;
	sbr = 0;

	if (kbstate.pressed[keyleft])	
		h = -1;
	if (kbstate.pressed[keyright])
		h = 1;
	if (kbstate.pressed[keyup])
		v = -1;
	if (kbstate.pressed[keydown])
		v = 1;
	if (kbstate.pressed[keyrollleft])
		r = 1;
	if (kbstate.pressed[keyrollright])
		r = -1;
	if (kbstate.pressed[keythrust])
		t = 1;
	if (h || v || r || t)
		do_dirkey(h, v, r, t);
	if (kbstate.pressed[key_page_up])
		do_pageup();
	if (kbstate.pressed[key_page_down])
		do_pagedown();

	if (kbstate.pressed[keyzoom])
		z = 10;
	if (kbstate.pressed[keyunzoom])
		z = -10;
	if (z)
		do_zoom(z);

	if (kbstate.pressed[keysciball_rollleft])
		sbr = -1;
	if (kbstate.pressed[keysciball_rollright])
		sbr = +1;
	if (kbstate.pressed[keysciball_pitchup])
		sbv = +1;
	if (kbstate.pressed[keysciball_pitchdown])
		sbv = -1;
	if (kbstate.pressed[keysciball_yawleft])
		sbh = -1;
	if (kbstate.pressed[keysciball_yawright])
		sbh = +1;
	if (displaymode == DISPLAYMODE_SCIENCE) {
		if (kbstate.pressed[key_sci_mining_bot])
			sci_mining_bot_pressed(NULL);
		if (kbstate.pressed[key_sci_tractor_beam])
			sci_tractor_pressed(NULL);
		if (kbstate.pressed[key_sci_lrs])
			sci_threed_pressed(NULL);
		if (kbstate.pressed[key_sci_srs])
			sci_sciplane_pressed(NULL);
		if (kbstate.pressed[key_sci_waypoints])
			sci_waypoints_pressed(NULL);
		if (kbstate.pressed[key_sci_details])
			sci_details_pressed(NULL);
	}

	if (sbh || sbv || sbr)
		do_sciball_dirkey(sbh, sbv, sbr);

	if (displaymode == DISPLAYMODE_ENGINEERING) {
		if (kbstate.pressed[key_eng_preset_1])
			snis_button_trigger_button(eng_ui.preset_buttons[0]);
		if (kbstate.pressed[key_eng_preset_2])
			snis_button_trigger_button(eng_ui.preset_buttons[1]);
		if (kbstate.pressed[key_eng_preset_3])
			snis_button_trigger_button(eng_ui.preset_buttons[2]);
		if (kbstate.pressed[key_eng_preset_4])
			snis_button_trigger_button(eng_ui.preset_buttons[3]);
		if (kbstate.pressed[key_eng_preset_5])
			snis_button_trigger_button(eng_ui.preset_buttons[4]);
		if (kbstate.pressed[key_eng_preset_6])
			snis_button_trigger_button(eng_ui.preset_buttons[5]);
	}
}

static void request_talking_stick(void)
{
	queue_to_server(snis_opcode_subcode_pkt("bb", OPCODE_TALKING_STICK, OPCODE_TALKING_STICK_REQUEST));
}

static void release_talking_stick(void)
{
	queue_to_server(snis_opcode_subcode_pkt("bb", OPCODE_TALKING_STICK, OPCODE_TALKING_STICK_RELEASE));
	pthread_mutex_lock(&voip_mutex);
	have_talking_stick = 0;
	pthread_mutex_unlock(&voip_mutex);
}

static gint key_press_cb(GtkWidget* widget, GdkEventKey* event, gpointer data)
{
	enum keyaction ka;

	ui_element_list_keypress(uiobjs, event);
        if ((event->keyval & 0xff00) == 0) 
		ka = keymap[displaymode][event->keyval];
        else
		ka = ffkeymap[displaymode][event->keyval & 0x00ff];
	if (ka > 0 && ka < NKEYSTATES)
		kbstate.pressed[ka] = 1;

	if (event->keyval == GDK_Control_R ||
		event->keyval == GDK_Control_L) {
		control_key_pressed = 1;
		return TRUE;
	}

	if (event->keyval == GDK_F12) {
		pthread_mutex_lock(&voip_mutex);
			if (!have_talking_stick) {
				pthread_mutex_unlock(&voip_mutex);
				request_talking_stick();
			} else {
				pthread_mutex_unlock(&voip_mutex);
			}
		/* We transmit regardless of whether we have a talking stick.
		 * If we do not have it, snis_server will drop our messages */
		if (control_key_pressed)
			voice_chat_start_recording(VOICE_CHAT_DESTINATION_ALL, 0);
		else
			voice_chat_start_recording(VOICE_CHAT_DESTINATION_CREW, 0);
	}
#if 0
	printf("event->keyval = 0x%08x, GDK_z = %08x, GDK_space = %08x\n", event->keyval, GDK_z, GDK_space);
#endif

        switch (ka) {
	case key_invert_vertical:
			if (control_key_pressed) {
				vertical_controls_inverted *= -1;
				vertical_controls_timer = frame_rate_hz;
			}
			return TRUE;
	case key_mouse_mode:
			if (control_key_pressed) {
				if (current_mouse_ui_mode == MOUSE_MODE_CAPTURED_MOUSE)
					desired_mouse_ui_mode = MOUSE_MODE_FREE_MOUSE;
				else
					desired_mouse_ui_mode = MOUSE_MODE_CAPTURED_MOUSE;
				mouse_mode_timer = frame_rate_hz;
				return TRUE;
			}
			break;
	case key_sci_mining_bot:
			sci_mining_bot_pressed((void *) 0);
			break;
	case key_toggle_space_dust:
			if (nfake_stars == 0)
				nfake_stars = 2000;
			else
				nfake_stars = 0;
			ecx_fake_stars_initialized = 0;
			fake_stars_timer = frame_rate_hz;
			return TRUE;
	case key_toggle_frame_stats:
			display_frame_stats = (display_frame_stats + 1) % 3;
			return TRUE;
        case keyfullscreen: {
			if (fullscreen) {
				gtk_window_unfullscreen(GTK_WINDOW(window));
				fullscreen = 0;
				/* configure_event takes care of resizing drawing area, etc. */
			} else {
				gtk_window_fullscreen(GTK_WINDOW(window));
				fullscreen = 1;
				/* configure_event takes care of resizing drawing area, etc. */
			}
			return TRUE;
		}
	case keyquit:	
			if (helpmode) {
				helpmode = 0;
				break;
			}
			in_the_process_of_quitting = !in_the_process_of_quitting;
			if (!in_the_process_of_quitting)
				current_quit_selection = 0;
			break;
	case keytorpedo:
		fire_torpedo_button_pressed(NULL);
		break;
	case key_weap_fire_missile:
		fire_missile_button_pressed(NULL);
		break;
	case keyphaser:
		if (in_the_process_of_quitting) {
			final_quit_selection = current_quit_selection;
			if (!final_quit_selection)
				in_the_process_of_quitting = 0;
			break;
		}
		do_laser();
		break;
	case key_robot_gripper:
		if (in_the_process_of_quitting) {
			final_quit_selection = current_quit_selection;
			if (!final_quit_selection)
				in_the_process_of_quitting = 0;
			break;
		}
		robot_gripper_button_pressed(NULL);
		break;
	case key_space:
		if (in_the_process_of_quitting) {
			final_quit_selection = current_quit_selection;
			if (!final_quit_selection)
				in_the_process_of_quitting = 0;
			break;
		}
		if (displaymode == DISPLAYMODE_MAINSCREEN) {
			if (external_camera_active) {
				external_camera_mode = (external_camera_mode + 1) % num_external_camera_modes;
				external_camera_velocity.v.x = 0.0;
				external_camera_velocity.v.y = 0.0;
				external_camera_velocity.v.z = 0.0;
				external_camera_active_timer = frame_rate_hz;
			}
		}
		break;
	case key_camera_mode:
		if (displaymode == DISPLAYMODE_MAINSCREEN)
			do_mainscreen_camera_mode();
		else if (displaymode == DISPLAYMODE_NAVIGATION && !nav_ui_computer_active())
			do_nav_camera_mode();
		break;
	case keyf1:
		helpmode = !helpmode;
		break;
	case keyf2:
		helpmode = 0;
		if (displaymode >= DISPLAYMODE_FONTTEST)
			break;
		if (role & ROLE_NAVIGATION) {
			displaymode = DISPLAYMODE_NAVIGATION;
			wwviaudio_add_sound(CHANGESCREEN_SOUND);
		}
		break;
	case keyf3:
		helpmode = 0;
		if (displaymode >= DISPLAYMODE_FONTTEST)
			break;
		if (role & ROLE_WEAPONS) {
			displaymode = DISPLAYMODE_WEAPONS;
			wwviaudio_add_sound(CHANGESCREEN_SOUND);
		}
		break;
	case keyf4:
		helpmode = 0;
		if (displaymode >= DISPLAYMODE_FONTTEST)
			break;
		if (role & ROLE_ENGINEERING) {
			displaymode = DISPLAYMODE_ENGINEERING;
			wwviaudio_add_sound(CHANGESCREEN_SOUND);
		}
		break;
	case keyf5:
		helpmode = 0;
		if (displaymode >= DISPLAYMODE_FONTTEST)
			break;
		if (role & ROLE_DAMCON) {
			displaymode = DISPLAYMODE_DAMCON;
			wwviaudio_add_sound(CHANGESCREEN_SOUND);
		}
		break;
	case keyf6:
		helpmode = 0;
		if (displaymode >= DISPLAYMODE_FONTTEST)
			break;
		if (role & ROLE_SCIENCE) {
			displaymode = DISPLAYMODE_SCIENCE;
			wwviaudio_add_sound(CHANGESCREEN_SOUND);
		}
		break;
	case keyf7:
		helpmode = 0;
		if (displaymode >= DISPLAYMODE_FONTTEST)
			break;
		if (role & ROLE_COMMS) {
			displaymode = DISPLAYMODE_COMMS;
			wwviaudio_add_sound(CHANGESCREEN_SOUND);
		}
		break;
	case keyf8:
		helpmode = 0;
		if (displaymode >= DISPLAYMODE_FONTTEST)
			break;
		if (role & ROLE_MAIN) {
			displaymode = DISPLAYMODE_MAINSCREEN;
			wwviaudio_add_sound(CHANGESCREEN_SOUND);
		}
		break;
	case keyf9:
		helpmode = 0;
		if (displaymode >= DISPLAYMODE_FONTTEST)
			break;
		if (role & ROLE_DEMON) {
			displaymode = DISPLAYMODE_DEMON;
			wwviaudio_add_sound(CHANGESCREEN_SOUND);
		}
		break;
	case keyf10:
		reload_shaders = 1;
		break;
	case keyonscreen:
		if (control_key_pressed)
			do_onscreen((uint8_t) displaymode & 0xff);
		break;
	case keyviewmode:
		/* Toggle main screen between "normal" and "weapons" view */
		do_view_mode_change();
		break;
	case keyrenderswitch: {
		static int r = 0;
		static int valid_combos[] = {
			FLATSHADING_RENDERER,
			WIREFRAME_RENDERER,
			FLATSHADING_RENDERER | WIREFRAME_RENDERER,
			FLATSHADING_RENDERER | WIREFRAME_RENDERER | BLACK_TRIS,
			};
		if (control_key_pressed) {
			r = (r + 1) % ARRAYSIZE(valid_combos);
			set_renderer(ecx, valid_combos[r]);
		}
		break;
		}
	case key_toggle_credits:
		if (control_key_pressed)
			credits_screen_active = !credits_screen_active;
		break;
	case key_toggle_external_camera:
		if (control_key_pressed)
			external_camera_active = !external_camera_active;
		break;
	case key_toggle_watermark:
		if (control_key_pressed)
			watermark_active = !watermark_active;
		break;
	case key_demon_console:
		demon_ui.console_active = !demon_ui.console_active;
		break;
	default:
		break;
	}

	return FALSE;
}

static gint key_release_cb(GtkWidget* widget, GdkEventKey* event, gpointer data)
{
	enum keyaction ka;


	ui_element_list_keyrelease(uiobjs, event);
	if (event->keyval == GDK_Control_R ||
		event->keyval == GDK_Control_L) {
		control_key_pressed = 0;
		return TRUE;
	}

        if ((event->keyval & 0xff00) == 0)
		ka = keymap[displaymode][event->keyval];
        else
		ka = ffkeymap[displaymode][event->keyval & 0x00ff];
	if (ka > 0 && ka < NKEYSTATES)
		kbstate.pressed[ka] = 0;

	if (event->keyval == GDK_F12) {
		voice_chat_stop_recording();
		/* We release even if we don't have, snis_server will know the real deal. */
		release_talking_stick();
	}

	return FALSE;
}

/* keyboard handling stuff ends */
/**********************************/


static void show_fonttest(GtkWidget *w)
{
	sng_abs_xy_draw_string("A B C D E F G H I J K L M", SMALL_FONT, 30, 30);
	sng_abs_xy_draw_string("N O P Q R S T U V W X Y Z", SMALL_FONT, 30, 60);
	sng_abs_xy_draw_string("a b c d e f g h i j k l m", SMALL_FONT, 30, 90);
	sng_abs_xy_draw_string("n o p q r s t u v w x y z", SMALL_FONT, 30, 120);
	sng_abs_xy_draw_string("0 1 2 3 4 5 6 7 8 9 ! , .", SMALL_FONT, 30, 150);
	sng_abs_xy_draw_string("? | - = * / \\ + ( ) \" ' _", SMALL_FONT, 30, 180);

	sng_abs_xy_draw_string("The Quick Fox Jumps Over The Lazy Brown Dog.", SMALL_FONT, 30, 210);
	sng_abs_xy_draw_string("The Quick Fox Jumps Over The Lazy Brown Dog.", BIG_FONT, 30, 280);
	sng_abs_xy_draw_string("The Quick Fox Jumps Over The Lazy Brown Dog.", TINY_FONT, 30, 350);
	sng_abs_xy_draw_string("The quick fox jumps over the lazy brown dog.", NANO_FONT, 30, 380);
	sng_abs_xy_draw_string("THE QUICK FOX JUMPS OVER THE LAZY BROWN DOG.", TINY_FONT, 30, 410);
	sng_abs_xy_draw_string("Well now, what have we here?  James Bond!", NANO_FONT, 30, 425);
	sng_abs_xy_draw_string("The quick fox jumps over the lazy brown dog.", SMALL_FONT, 30, 450);
	sng_abs_xy_draw_string("Copyright (C) 2010 Stephen M. Cameron 0123456789", TINY_FONT, 30, 480);
}

static void show_introscreen(GtkWidget *w)
{
	sng_abs_xy_draw_string("Space Nerds", BIG_FONT, txx(80), txy(200));
	sng_abs_xy_draw_string("In Space", BIG_FONT, txx(180), txy(320));
	sng_abs_xy_draw_string("Copyright (C) 2010 Stephen M. Cameron", NANO_FONT, txx(255), txy(550));
}

static int lobbylast1clickx = -1;
static int lobbylast1clicky = -1;
static int lobby_selected_server = -1;
static struct lobby_ui {
	struct button *lobby_cancel_button;
	struct button *lobby_connect_to_server_button;
} lobby_ui;

static void lobby_connect_to_server_button_pressed()
{
	printf("lobby connect to server button pressed\n");
	if (lobby_selected_server != -1)
		displaymode = DISPLAYMODE_CONNECTING;
}

static void lobby_cancel_button_pressed()
{
	printf("lobby cancel button pressed\n");
	displaymode = DISPLAYMODE_NETWORK_SETUP;
	lobby_selected_server = -1;
	lobby_count = 0;
	if (lobby_socket >= 0) {
		shutdown(lobby_socket, SHUT_RDWR);
		close(lobby_socket);
		lobby_socket = -1;
	}
}

/* lobby_data_mutex should be held. */
static int lobby_lookup_server_by_location(char *location)
{
	/* FIXME, this is racy with connect_to_lobby() */
	int i;

	fprintf(stderr, "snis_client: looking up server '%s', ngameservers = %d\n", location, ngameservers);
	for (i = 0; i < ngameservers; i++) {
		fprintf(stderr, "snis_client: looking up server '%s' vs. %s\n",
				location, lobby_game_server[i].location);
		if (strcmp(lobby_game_server[i].location, location) == 0)
			return i;
	}
	fprintf(stderr, "snis_client: didn't find server '%s'\n", location);
	return -1;
}

static void ui_hide_widget(void *widget);
static void ui_unhide_widget(void *widget);
static void ui_set_widget_displaymode(void *widget, int displaymode)
{
	struct ui_element *uie;

	uie = widget_to_ui_element(uiobjs, widget);
	if (!uie)
		return;
	ui_element_set_displaymode(uie, displaymode);
}

static void ui_set_widget_tooltip(void *widget, char *tooltip)
{
	struct ui_element *uie;

	uie = widget_to_ui_element(uiobjs, widget);
	if (!uie)
		return;
	ui_element_set_tooltip(uie, tooltip);
}

static void show_common_screen(GtkWidget *w, char *title);

static void show_rotating_wombat(void)
{
	const float angle_of_view = 70.0 * M_PI / 180.0;
	const float near = 1.0;
	const float far = 5000.0;
	union vec3 camera_up = { {0, 1, 0}, };
	union vec3 camera_pos = { { -17.0, 7.0, 0 }, };
	union vec3 camera_lookat = { { 0, -5, 0 } };
	struct entity *wombat, *turret_base, *turret;
	union quat orientation;

	if (!network_setup_ecx)
		network_setup_ecx = entity_context_new(3, 3);
	camera_assign_up_direction(network_setup_ecx, camera_up.v.x, camera_up.v.y, camera_up.v.z);
	camera_set_pos(network_setup_ecx, camera_pos.v.x, camera_pos.v.y, camera_pos.v.z);
	camera_look_at(network_setup_ecx, camera_lookat.v.x, camera_lookat.v.y, camera_lookat.v.z);

	set_renderer(network_setup_ecx, FLATSHADING_RENDERER | WIREFRAME_RENDERER | BLACK_TRIS);
	camera_set_parameters(network_setup_ecx, near, far, SCREEN_WIDTH, SCREEN_HEIGHT, angle_of_view);
	calculate_camera_transform(network_setup_ecx);
	entity_context_set_hi_lo_poly_pixel_threshold(network_setup_ecx, low_poly_threshold);
	wombat = add_entity(network_setup_ecx, ship_mesh_map[SHIP_CLASS_WOMBAT], 0.0, 0.0, 0.0, DARKGREEN);
	quat_init_axis(&orientation, 0, 1, 0, (timer % 360) * M_PI / 180);
	update_entity_orientation(wombat, &orientation);
	turret_base = add_entity(network_setup_ecx, ship_turret_base_mesh, -1.5, 3.0, 0.0, DARKGREEN);
	turret = add_entity(network_setup_ecx, ship_turret_mesh, -1.5, 3.0, 0.0, DARKGREEN);
	update_entity_parent(network_setup_ecx, turret_base, wombat);
	update_entity_parent(network_setup_ecx, turret, wombat);
	render_entities(network_setup_ecx);
	remove_all_entity(network_setup_ecx);
}

static void delete_all_objects(void);
static void stop_gameserver_writer_thread(void);

/* We lost snis server.  Go back to network setup screen. */
static void we_lost_snis_server(void)
{
	stop_gameserver_writer_thread();
	if (gameserver_sock != -1) {
		shutdown(gameserver_sock, SHUT_RDWR);
		close(gameserver_sock);
		gameserver_sock = -1;
	}
	connected_to_gameserver = 0;
	lobby_selected_server = -1;
	quickstartmode = 0;
	delete_all_objects();
	clear_damcon_pool();
	displaymode = DISPLAYMODE_NETWORK_SETUP;
	if (lobby_socket != -1)
		close(lobby_socket);
	lobby_socket = -1;
	switched_server = -1;
	switched_server2 = -1;
	synchronous_update_lobby_info(); /* lobby info might be old, so let's update it. */
}

static void show_lobbyscreen(GtkWidget *w)
{
	char msg[100];
	int i, protocol_mismatch;
#define STARTLINE 100
#define LINEHEIGHT 30

	show_common_screen(w, "");
	show_rotating_wombat();
	sng_set_foreground(UI_COLOR(lobby_connecting));
	if (lobby_socket == -1 && switched_server2 == -1) {
		sng_set_foreground(WHITE);
		sng_abs_xy_draw_string("SPACE NERDS IN SPACE", BIG_FONT, txx(20), txy(200));
		sng_abs_xy_draw_string("COPYRIGHT (C) 2020 STEPHEN M. CAMERON, ET AL.", PICO_FONT,
			txx(255), txy(550));
		if (lobby_count >= MAX_LOBBY_TRIES)
			snprintf(msg, sizeof(msg), "GIVING UP ON LOBBY %s:%d -- TRIED %d TIMES.",
				lobbyhost, lobbyport, lobby_count);
		else
			snprintf(msg, sizeof(msg), "CONNECTING TO LOBBY %s:%d... TRIED %d TIMES.",
				lobbyhost, lobbyport, lobby_count);
		sng_abs_xy_draw_string(msg, SMALL_FONT, txx(100), txy(400));
		sng_set_foreground(RED);
		sng_abs_xy_draw_string(lobbyerror, NANO_FONT, txx(100), txy(430));
		sng_set_foreground(WHITE);
	} else {

		/* Switch server rigamarole */
		if (lobby_selected_server == -1) {
			int time_to_switch_servers;
			pthread_mutex_lock(&to_server_queue_event_mutex);
			time_to_switch_servers = (switched_server2 != -1);
			if (time_to_switch_servers) {
				int count = 0;
				do {
					fprintf(stderr, "snis_client: time to switch servers (try = %d)\n", count);
					pthread_mutex_lock(&lobby_data_mutex);
					lobby_selected_server =
						lobby_lookup_server_by_location(switch_server_location_string);
					pthread_mutex_unlock(&lobby_data_mutex);
					/* lobby_selected_server is still racy */
					fprintf(stderr, "snis_client: lobby_selected_server = %d (%s)\n",
						lobby_selected_server, switch_server_location_string);
					if (lobby_selected_server == -1) {
						pthread_mutex_unlock(&to_server_queue_event_mutex);
						fprintf(stderr, "snis_client: didn't find switch server.\n");
						synchronous_update_lobby_info();
						pthread_mutex_lock(&to_server_queue_event_mutex);
					}
					count++;
				} while (lobby_selected_server == -1 && count < 3);
				if (lobby_selected_server != -1) {
					switched_server2 = -1;
					displaymode = DISPLAYMODE_CONNECTING;
				} else {
					/* We got lost, we can't find the server we're supposed to warp to. */
					pthread_mutex_unlock(&to_server_queue_event_mutex);
					we_lost_snis_server();
					return;
				}
			}
			pthread_mutex_unlock(&to_server_queue_event_mutex);
			if (displaymode == DISPLAYMODE_CONNECTING)
				return;
		}

		if (lobby_selected_server != -1 && quickstartmode) {
			displaymode = DISPLAYMODE_CONNECTING;
			return;
		}

		lobby_selected_server = -1;
		sng_center_xy_draw_string("SPACE NERDS IN SPACE LOBBY", TINY_FONT, txx(400), LINEHEIGHT);
		sng_center_xy_draw_string("SELECT A SERVER", NANO_FONT, txx(400), LINEHEIGHT * 2);
		snprintf(msg, sizeof(msg), "CLIENT PROTOCOL VERSION IS %s", SNIS_PROTOCOL_VERSION);
		sng_center_xy_draw_string(msg, NANO_FONT, txx(400), LINEHEIGHT * 3);

		/* Draw column headings */
		i = -1;
		sng_set_foreground(UI_COLOR(lobby_server_heading));
		snprintf(msg, sizeof(msg), "IP ADDRESS/PORT");
		sng_abs_xy_draw_string(msg, NANO_FONT, txx(30), txy(100) + i * LINEHEIGHT);
		snprintf(msg, sizeof(msg), "GAME INSTANCE");
		sng_abs_xy_draw_string(msg, NANO_FONT, txx(150), txy(100) + i * LINEHEIGHT);
		snprintf(msg, sizeof(msg), "PROTOCOL");
		sng_abs_xy_draw_string(msg, NANO_FONT, txx(350), txy(100) + i * LINEHEIGHT);
		snprintf(msg, sizeof(msg), "LOCATION");
		sng_abs_xy_draw_string(msg, NANO_FONT, txx(450), txy(100) + i * LINEHEIGHT);
		snprintf(msg, sizeof(msg), "CONNECTIONS");
		sng_abs_xy_draw_string(msg, NANO_FONT, txx(550), txy(100) + i * LINEHEIGHT);

		/* Draw server info */
		sng_set_foreground(UI_COLOR(lobby_connecting));
		pthread_mutex_lock(&lobby_data_mutex);
		for (i = 0; i < ngameservers; i++) {
			unsigned char *x = (unsigned char *) 
				&lobby_game_server[i].ipaddr;
			if (lobbylast1clickx > txx(30) && lobbylast1clickx < txx(700) &&
				lobbylast1clicky > txy(100) + (-0.5 + i) * LINEHEIGHT &&
				lobbylast1clicky < txy(100) + (0.5 + i) * LINEHEIGHT) {
				lobby_selected_server = i;
				sng_set_foreground(UI_COLOR(lobby_selected_server));
				snis_draw_rectangle(0, txx(25), txy(100) + (-0.5 + i) * LINEHEIGHT,
					txx(600), LINEHEIGHT);
				snis_button_set_position(lobby_ui.lobby_connect_to_server_button,
					txx(650), (int) (txy(100) + (-0.5 + i) * LINEHEIGHT));
				ui_unhide_widget(lobby_ui.lobby_connect_to_server_button);
			} else
				sng_set_foreground(UI_COLOR(lobby_connecting));

			if (quickstartmode) {
				lobby_selected_server = 0;
			}
			 
			snprintf(msg, sizeof(msg), "%hhu.%hhu.%hhu.%hhu/%hu", x[0], x[1], x[2], x[3],
					ntohs(lobby_game_server[i].port));

			/* Check if the IP address we have is bogus.  This can happen if
			 * ssgl_get_primary_ip_addr() fails.
			 */
			if (x[0] == 0 && x[1] == 0 && x[2] == 0 && x[3] == 0) {
				/* Bogus IP addr. Make it blink orange/red */
				if (timer & 0x04)
					sng_set_foreground(BLACK);
				else
					sng_set_foreground(ORANGERED);
			} else {
				sng_set_foreground(UI_COLOR(lobby_connecting));
			}

			sng_abs_xy_draw_string(msg, NANO_FONT, txx(30), txy(100) + i * LINEHEIGHT);
			snprintf(msg, sizeof(msg), "%s", lobby_game_server[i].game_instance);
			sng_abs_xy_draw_string(msg, NANO_FONT, txx(150), txy(100) + i * LINEHEIGHT);
			protocol_mismatch = strncmp(lobby_game_server[i].protocol_version, SNIS_PROTOCOL_VERSION,
							sizeof(lobby_game_server[i].protocol_version)) != 0;
			if (timer & 0x04 || !protocol_mismatch) {
				if (protocol_mismatch)
					sng_set_foreground(ORANGERED);
				snprintf(msg, sizeof(msg), "%s", lobby_game_server[i].protocol_version);
				sng_abs_xy_draw_string(msg, NANO_FONT, txx(350), txy(100) + i * LINEHEIGHT);
			}
			sng_set_foreground(UI_COLOR(lobby_connecting));
			snprintf(msg, sizeof(msg), "%s", lobby_game_server[i].location);
			sng_abs_xy_draw_string(msg, NANO_FONT, txx(450), txy(100) + i * LINEHEIGHT);
			snprintf(msg, sizeof(msg), "%d", lobby_game_server[i].nconnections);
			sng_abs_xy_draw_string(msg, NANO_FONT, txx(550), txy(100) + i * LINEHEIGHT);
		}
		pthread_mutex_unlock(&lobby_data_mutex);
		if (lobby_selected_server != -1)
			snis_button_set_color(lobby_ui.lobby_connect_to_server_button, UI_COLOR(lobby_connect_ok));
		else
			ui_hide_widget(lobby_ui.lobby_connect_to_server_button);
	}
}

static int process_update_power_data(void)
{
	struct packed_buffer pb;
	uint32_t id;
	int rc;
	struct power_model_data pmd;
	unsigned char buffer[sizeof(pmd) + sizeof(uint32_t)];

	rc = snis_readsocket(gameserver_sock, buffer, sizeof(pmd) + sizeof(uint32_t));
	if (rc != 0)
		return rc;
	packed_buffer_init(&pb, buffer, sizeof(buffer));
	packed_buffer_extract(&pb, "wr", &id, &pmd, sizeof(pmd));
	pthread_mutex_lock(&universe_mutex);
	rc = update_power_model_data(id, &pmd);
	pthread_mutex_unlock(&universe_mutex);
	return rc;
}

static int process_update_coolant_data(void)
{
	struct packed_buffer pb;
	uint32_t id;
	int rc;
	struct power_model_data pmd;
	struct ship_damage_data temperature_data;
	unsigned char buffer[sizeof(pmd) + sizeof(temperature_data) + sizeof(uint32_t)];

	rc = snis_readsocket(gameserver_sock, buffer, sizeof(buffer));
	if (rc != 0)
		return rc;
	packed_buffer_init(&pb, buffer, sizeof(buffer));
	packed_buffer_extract(&pb, "wr", &id, &pmd, sizeof(pmd));
	packed_buffer_extract(&pb, "r", &temperature_data, sizeof(temperature_data));
	pthread_mutex_lock(&universe_mutex);
	rc = update_coolant_model_data(id, &pmd, &temperature_data);
	pthread_mutex_unlock(&universe_mutex);
	return rc;
}

static void update_emf_detector(uint8_t emf_value);

static inline int nav_ui_computer_active(void)
{
	return nav_ui.computer_active;
}

static void comms_setup_rts_buttons(int activate, struct snis_entity *player_ship);

static int process_update_ship_packet(uint8_t opcode)
{
	int i;
	unsigned char buffer[146];
	struct packed_buffer pb;
	uint16_t alive;
	uint32_t id, timestamp, torpedoes, power;
	uint32_t fuel, oxygen, victim_id, wallet, viewpoint_object;
	double dx, dy, dz, dyawvel, dpitchvel, drollvel;
	double dsheading, dbeamwidth;
	double hgax, hgay, hgaz; /* high gain antenna aim */
	int rc;
	int type = opcode == OPCODE_UPDATE_SHIP ? OBJTYPE_SHIP1 : OBJTYPE_SHIP2;
	uint8_t tloading, tloaded, throttle, rpm, temp, scizoom, weapzoom, navzoom,
		mainzoom, warpdrive,
		missile_count, phaser_charge, phaser_wavelength, shiptype,
		reverse, trident, in_secure_area, docking_magnets, emf_detector,
		nav_mode, warp_core_status, rts_mode, exterior_lights, alarms_silenced,
		missile_lock_detected, rts_active_button, comms_crypto_mode;
	union quat orientation, sciball_orientation, weap_orientation, hg_ant_orientation;
	union euler ypr;
	struct entity *e;
	struct snis_entity *o;

	assert(sizeof(buffer) > sizeof(struct update_ship_packet) - sizeof(uint8_t));
	rc = snis_readsocket(gameserver_sock, buffer, sizeof(struct update_ship_packet) - sizeof(uint8_t));
	/* printf("process_update_ship_packet, snis_readsocket returned %d\n", rc); */
	if (rc != 0)
		return rc;
	packed_buffer_init(&pb, buffer, sizeof(buffer));
	packed_buffer_extract(&pb, "wwhSSS", &id, &timestamp, &alive,
				&dx, (int32_t) UNIVERSE_DIM, &dy, (int32_t) UNIVERSE_DIM,
				&dz, (int32_t) UNIVERSE_DIM);
	packed_buffer_extract(&pb, "RRRwwRR",
				&dyawvel,
				&dpitchvel,
				&drollvel,
				&torpedoes, &power,
				&dsheading,
				&dbeamwidth);
	packed_buffer_extract(&pb, "bbbwwbbbbbbbbbbbbwQQQQSSSbB8bbww",
			&tloading, &throttle, &rpm, &fuel, &oxygen, &temp,
			&scizoom, &weapzoom, &navzoom, &mainzoom,
			&warpdrive,
			&missile_count, &phaser_charge, &phaser_wavelength, &shiptype,
			&reverse, &trident, &victim_id, &orientation.vec[0],
			&sciball_orientation.vec[0], &weap_orientation.vec[0], &hg_ant_orientation.vec[0],
			&hgax, (int32_t) 1000000,
			&hgay, (int32_t) 1000000,
			&hgaz, (int32_t) 1000000,
			&emf_detector,
			&in_secure_area,
			&docking_magnets, &nav_mode, &warp_core_status, &rts_mode,
			&exterior_lights, &alarms_silenced, &missile_lock_detected,
			&comms_crypto_mode, &rts_active_button, &wallet,
			&viewpoint_object);
	tloaded = (tloading >> 4) & 0x0f;
	tloading = tloading & 0x0f;
	quat_to_euler(&ypr, &orientation);	
	pthread_mutex_lock(&universe_mutex);

	/* Now update the ship... do it inline here instead of a function because
	 * such a function would require 8 million params.
	 */

	i = lookup_object_by_id(id);
	if (i < 0) {
		if (id == my_ship_id)
			e = add_entity(ecx, NULL, dx, dy, dz, SHIP_COLOR);
		else {
			e = add_entity(ecx, ship_mesh_map[shiptype % nshiptypes],
					dx, dy, dz, SHIP_COLOR);
			if (e && !reverse)
				add_ship_thrust_entities(NULL, NULL, ecx, e, shiptype, 36, 0);
		}
		i = add_generic_object(id, timestamp, dx, dy, dz, 0.0, 0.0, 0.0, &orientation, type, alive, e);
		if (i < 0) {
			rc = i;
			goto out;
		}
	} else {
		update_generic_object(i, timestamp, dx, dy, dz, 0.0, 0.0, 0.0, &orientation, alive);
	}
	o = &go[i];
	o->tsd.ship.yaw_velocity = dyawvel;
	o->tsd.ship.pitch_velocity = dpitchvel;
	o->tsd.ship.roll_velocity = drollvel;
	o->tsd.ship.torpedoes = torpedoes;
	o->tsd.ship.power = power;
	o->tsd.ship.sci_heading = dsheading;
	o->tsd.ship.sci_beam_width = dbeamwidth;
	o->tsd.ship.torpedoes_loaded = tloaded;
	o->tsd.ship.torpedoes_loading = tloading;
	o->tsd.ship.throttle = throttle;
	o->tsd.ship.rpm = rpm;
	o->tsd.ship.fuel = fuel;
	o->tsd.ship.oxygen = oxygen;
	o->tsd.ship.temp = temp;
	o->tsd.ship.scizoom = scizoom;
	o->tsd.ship.weapzoom = weapzoom;
	o->tsd.ship.navzoom = navzoom;
	o->tsd.ship.mainzoom = mainzoom;
	o->tsd.ship.warpdrive = warpdrive;
	o->tsd.ship.missile_count = missile_count;
	o->tsd.ship.phaser_charge = phaser_charge;
	o->tsd.ship.phaser_wavelength = phaser_wavelength;
	o->tsd.ship.damcon = NULL;
	o->tsd.ship.shiptype = shiptype;
	o->tsd.ship.in_secure_area = in_secure_area;
	o->tsd.ship.docking_magnets = docking_magnets;
	o->tsd.ship.emf_detector = emf_detector;
	/* FIXME: really update_emf_detector should get called every frame and passed o->tsd.ship.emf_detector. */
	o->tsd.ship.nav_mode = nav_mode;
	o->tsd.ship.warp_core_status = warp_core_status;
	global_rts_mode = rts_mode;
	o->tsd.ship.rts_active_button = rts_active_button;
	comms_setup_rts_buttons(rts_mode, o);

	update_orientation_history(o->tsd.ship.sciball_o, &sciball_orientation);
	update_orientation_history(o->tsd.ship.weap_o, &weap_orientation);
	o->tsd.ship.current_hg_ant_orientation = hg_ant_orientation;
	o->tsd.ship.desired_hg_ant_aim.v.x = hgax;
	o->tsd.ship.desired_hg_ant_aim.v.y = hgay;
	o->tsd.ship.desired_hg_ant_aim.v.z = hgaz;
	if (vec3_magnitude(&o->tsd.ship.desired_hg_ant_aim) > 0.001)
		vec3_normalize_self(&o->tsd.ship.desired_hg_ant_aim);
	o->tsd.ship.exterior_lights = exterior_lights ? 255 : 0; /* It is transmitted in a bit field */
	o->tsd.ship.alarms_silenced = alarms_silenced;
	o->tsd.ship.missile_lock_detected = missile_lock_detected;
	o->tsd.ship.trident = trident;
	o->tsd.ship.wallet = (float) wallet;
	o->tsd.ship.viewpoint_object = viewpoint_object;
	if (my_ship_id == o->id) {
		update_emf_detector(emf_detector);
		snis_button_set_label(nav_ui.trident_button, trident ? "RELATIVE" : "ABSOLUTE");
		snis_button_set_label(nav_ui.starmap_button, nav_mode ? "NAV MODE" : "STARMAP");
		if (!o->tsd.ship.reverse && reverse)
			wwviaudio_add_sound(REVERSE_SOUND);
	}
	o->tsd.ship.reverse = reverse;
	o->tsd.ship.ai[0].u.attack.victim_id = victim_id;
	o->tsd.ship.comms_crypto_mode = comms_crypto_mode;
	rc = 0;
out:
	pthread_mutex_unlock(&universe_mutex);
	return (rc < 0);
}

static int read_and_unpack_buffer(unsigned char *buffer, char *format, ...)
{
        va_list ap;
        struct packed_buffer pb;
        int rc, size = calculate_buffer_size(format);

	rc = snis_readsocket(gameserver_sock, buffer, size);
	if (rc != 0)
		return rc;
        packed_buffer_init(&pb, buffer, size);
        va_start(ap, format);
        rc = packed_buffer_extract_va(&pb, format, ap);
        va_end(ap);
        return rc;
}

static int process_update_damcon_object(void)
{
	unsigned char buffer[sizeof(struct damcon_obj_update_packet) - sizeof(uint8_t)];
	uint32_t id, ship_id, type;
	double x, y, velocity, heading;
	uint8_t autonomous_mode;
	int rc;

	rc = read_and_unpack_buffer(buffer, "wwwSSSRb",
			&id, &ship_id, &type,
			&x, (int32_t) DAMCONXDIM, 
			&y, (int32_t) DAMCONYDIM, 
			&velocity, (int32_t) DAMCONXDIM, 
			&heading,
			&autonomous_mode);
	if (rc)
		return rc;
	pthread_mutex_lock(&universe_mutex);
	rc = update_damcon_object(id, ship_id, type, x, y, velocity, heading, autonomous_mode);
	pthread_mutex_unlock(&universe_mutex);
	return rc;
}

static int process_update_damcon_socket(void)
{
	unsigned char buffer[sizeof(struct damcon_socket_update_packet) - sizeof(uint8_t)];
	uint32_t id, ship_id, type, contents_id;
	double x, y;
	uint8_t system, part;
	int rc;

	rc = read_and_unpack_buffer(buffer, "wwwSSwbb",
		&id, &ship_id, &type,
			&x, (int32_t) DAMCONXDIM, 
			&y, (int32_t) DAMCONYDIM, 
			&contents_id, &system, &part);
	if (rc)
		return rc;
	pthread_mutex_lock(&universe_mutex);
	rc = update_damcon_socket(id, ship_id, type, x, y, contents_id, system, part);
	pthread_mutex_unlock(&universe_mutex);
	return rc;
}

static int process_update_damcon_part(void)
{
	unsigned char buffer[sizeof(struct damcon_part_update_packet) - sizeof(uint8_t)];
	uint32_t id, ship_id, type;
	double x, y, heading;
	uint8_t system, part, damage;
	int rc;

	rc = read_and_unpack_buffer(buffer, "wwwSSRbbb",
		&id, &ship_id, &type,
			&x, (int32_t) DAMCONXDIM, 
			&y, (int32_t) DAMCONYDIM, 
			&heading,
			&system, &part, &damage);
	if (rc != 0)
		return rc;
	pthread_mutex_lock(&universe_mutex);
	rc = update_damcon_part(id, ship_id, type, x, y, heading, system, part, damage);
	pthread_mutex_unlock(&universe_mutex);
	return rc;
}

static int process_mainscreen_view_mode(void)
{
	unsigned char buffer[sizeof(struct request_mainscreen_view_change) -
					sizeof(uint8_t)];
	struct snis_entity *o;
	uint8_t view_mode;
	double view_angle;
	int rc;

	rc = read_and_unpack_buffer(buffer, "Rb", &view_angle, &view_mode);
	if (rc != 0)
		return rc;
	if (!(o = find_my_ship()))
		return 0;
	o->tsd.ship.view_angle = view_angle;
	o->tsd.ship.view_mode = view_mode;
	return 0;
}

static int process_update_econ_ship_packet(uint8_t opcode)
{
	unsigned char buffer[200];
	uint32_t id, timestamp, victim_id;
	uint8_t faction;
	double dx, dy, dz, px, py, pz;
	union quat orientation;
	uint8_t shiptype, ai[MAX_AI_STACK_ENTRIES], npoints, rts_order;
	union vec3 patrol[MAX_PATROL_POINTS];
	double threat_level;
	int rc;

	assert(sizeof(buffer) > sizeof(struct update_econ_ship_packet) - sizeof(uint8_t));
	rc = read_and_unpack_buffer(buffer, "wwSSSQbbb", &id, &timestamp,
				&dx, (int32_t) UNIVERSE_DIM, &dy, (int32_t) UNIVERSE_DIM, 
				&dz, (int32_t) UNIVERSE_DIM,
				&orientation,
				&shiptype, &faction, &rts_order);
	if (rc != 0)
		return rc;
	if (opcode != OPCODE_ECON_UPDATE_SHIP_DEBUG_AI) {
		memset(ai, 0, 5);
		npoints = 0;
		threat_level = 0.0;
		victim_id = (uint32_t) -1;
		goto done;
	}
	BUILD_ASSERT(MAX_AI_STACK_ENTRIES == 5);
	rc = read_and_unpack_buffer(buffer, "wbbbbbSb",
			&victim_id, &ai[0], &ai[1], &ai[2], &ai[3], &ai[4],
			&threat_level, (int32_t) UNIVERSE_DIM, &npoints);
	if (rc != 0)
		return rc;

	if (npoints > MAX_PATROL_POINTS)
		return -1;

	memset(patrol, 0, sizeof(patrol));
	for (int i = 0; i < npoints; i++) {
		rc = read_and_unpack_buffer(buffer + 6 + i * sizeof(uint32_t) * 3, "SSS",
			&px, (int32_t) UNIVERSE_DIM,
			&py, (int32_t) UNIVERSE_DIM,
			&pz, (int32_t) UNIVERSE_DIM);
		if (rc)
			return rc;
		patrol[i].v.x = (float) px;
		patrol[i].v.y = (float) py;
		patrol[i].v.z = (float) pz;
	}

done:
	pthread_mutex_lock(&universe_mutex);
	rc = update_econ_ship(id, timestamp, dx, dy, dz, &orientation, victim_id,
				shiptype, ai, threat_level, npoints, patrol, faction, rts_order);
	pthread_mutex_unlock(&universe_mutex);
	return (rc < 0);
} 

static int process_update_torpedo_packet(void)
{
	unsigned char buffer[100];
	uint32_t id, timestamp, ship_id;
	double dx, dy, dz;
	int rc;

	assert(sizeof(buffer) > sizeof(struct update_torpedo_packet) - sizeof(uint8_t));
	rc = read_and_unpack_buffer(buffer, "wwwSSS", &id, &timestamp, &ship_id,
				&dx, (int32_t) UNIVERSE_DIM, &dy, (int32_t) UNIVERSE_DIM,
				&dz, (int32_t) UNIVERSE_DIM);
	if (rc != 0)
		return rc;
	pthread_mutex_lock(&universe_mutex);
	rc = update_torpedo(id, timestamp, dx, dy, dz, ship_id);
	pthread_mutex_unlock(&universe_mutex);
	return (rc < 0);
} 

static int process_update_missile_packet(void)
{
	unsigned char buffer[100];
	uint32_t id, timestamp;
	double dx, dy, dz;
	union quat orientation;
	int rc;

	assert(sizeof(buffer) > sizeof(struct update_missile_packet) - sizeof(uint8_t));
	rc = read_and_unpack_buffer(buffer, "wwSSSQ", &id, &timestamp,
				&dx, (int32_t) UNIVERSE_DIM, &dy, (int32_t) UNIVERSE_DIM,
				&dz, (int32_t) UNIVERSE_DIM, &orientation);
	if (rc != 0)
		return rc;
	pthread_mutex_lock(&universe_mutex);
	rc = update_missile(id, timestamp, dx, dy, dz, &orientation);
	pthread_mutex_unlock(&universe_mutex);
	return (rc < 0);
}

static int process_warp_limbo_packet(void)
{
	unsigned char buffer[sizeof(struct warp_limbo_packet)];
	int rc;
	uint16_t value;

	rc = read_and_unpack_buffer(buffer, "h", &value);
	if (rc != 0)
		return rc;
	if (suppress_hyperspace_noise)
		warp_limbo_countdown = 0;
	else if (value >= 0 && value <= 40 * frame_rate_hz)
		warp_limbo_countdown = value;
	return 0;
} 

static int process_initiate_warp_packet()
{
	unsigned char buffer[10], enough_oomph;
	int rc;

	rc = read_and_unpack_buffer(buffer, "b", &enough_oomph);
	if (rc != 0)
		return rc;
	if (enough_oomph)
		wwviaudio_add_sound(WARPDRIVE_SOUND);
	else
		wwviaudio_add_sound(WARP_DRIVE_FUMBLE);
	return 0;
}

static int process_wormhole_limbo_packet(void)
{
	return process_warp_limbo_packet();
}

static int process_update_laser_packet(void)
{
	unsigned char buffer[100];
	uint32_t id, timestamp, ship_id;
	uint8_t power;
	double dx, dy, dz;
	union quat orientation;
	int rc;

	assert(sizeof(buffer) > sizeof(struct update_laser_packet) - sizeof(uint8_t));
	rc = read_and_unpack_buffer(buffer, "wwwbSSSQ", &id, &timestamp, &ship_id, &power,
				&dx, (int32_t) UNIVERSE_DIM,
				&dy, (int32_t) UNIVERSE_DIM,
				&dz, (int32_t) UNIVERSE_DIM,
				&orientation);
	if (rc != 0)
		return rc;
	pthread_mutex_lock(&universe_mutex);
	rc = update_laser(id, timestamp, power, dx, dy, dz, &orientation, ship_id);
	pthread_mutex_unlock(&universe_mutex);
	return (rc < 0);
} 

static int process_update_spacemonster(void)
{
	unsigned char buffer[100];
	uint32_t id, timestamp, seed;
	double dx, dy, dz;
	union quat orientation;
	int rc;
	uint8_t emit_intensity, head_size, tentacle_size;

	assert(sizeof(buffer) > sizeof(struct update_spacemonster_packet) - sizeof(uint8_t));
	rc = read_and_unpack_buffer(buffer, "wwSSSwQbbb", &id, &timestamp,
				&dx, (int32_t) UNIVERSE_DIM,
				&dy, (int32_t) UNIVERSE_DIM,
				&dz, (int32_t) UNIVERSE_DIM, &seed, &orientation, &emit_intensity,
				&head_size, &tentacle_size);
	if (rc != 0)
		return rc;
	pthread_mutex_lock(&universe_mutex);
	rc = update_spacemonster(id, timestamp, dx, dy, dz, seed, &orientation, emit_intensity,
					head_size, tentacle_size);
	pthread_mutex_unlock(&universe_mutex);
	return (rc < 0);
}

static int process_red_alert()
{
	int rc;
	unsigned char buffer[10];
	unsigned char alert_value;

	rc = read_and_unpack_buffer(buffer, "b", &alert_value);
	if (rc != 0)
		return rc;
	red_alert_mode = (alert_value != 0);
	return 0;
}

#define MAINSCREEN_COMMS_LINES 12
static struct main_screen_text_data {
	char text[MAINSCREEN_COMMS_LINES][100];
	int last;
	int comms_on_mainscreen;
} main_screen_text;

static int process_comms_mainscreen()
{
	int rc;
	unsigned char buffer[10];
	unsigned char comms_mainscreen;

	rc = read_and_unpack_buffer(buffer, "b", &comms_mainscreen);
	if (rc != 0)
		return rc;
	main_screen_text.comms_on_mainscreen = (comms_mainscreen != 0);
	return 0;
}

static void load_textures(void);

static int process_load_skybox(void)
{
	int rc;
	unsigned char length;
	char string[PATH_MAX + 1];
	unsigned char buffer[PATH_MAX + 1];

	memset(string, 0, sizeof(string));
	rc = read_and_unpack_buffer(buffer, "b", &length);
	if (rc != 0)
		return rc;
	rc = snis_readsocket(gameserver_sock, string, length);
	if (rc != 0)
		return rc;

	string[100] = '\0';
	string[length] = '\0';
	if (lua_skybox_prefix)
		free(lua_skybox_prefix);
	lua_skybox_prefix = NULL;
	if (strncmp(string, "restore-default", sizeof(string)) != 0)
		lua_skybox_prefix = strdup(string);
	printf("snis_client: LOAD SKYBOX: '%s'\n", string);
	strncpy(old_solarsystem_name, solarsystem_name, sizeof(old_solarsystem_name) - 1);
	old_solarsystem_name[99] = '\0';
	old_solarsystem_assets = solarsystem_assets;
	per_solarsystem_textures_loaded = 0;

	return 0;
}

static int process_cycle_camera_point_of_view(unsigned char *camera_mode)
{
	int rc;
	unsigned char buffer[10];
	unsigned char new_mode;

	rc = read_and_unpack_buffer(buffer, "b", &new_mode);
	if (rc != 0)
		return rc;
	pthread_mutex_lock(&universe_mutex);
	*camera_mode = new_mode % 5;
	pthread_mutex_unlock(&universe_mutex);
	return 0;
}

static int process_add_warp_effect(void)
{
	int i, rc, skip_source_effect = 0;
	unsigned char buffer[100];
	uint32_t oid;
	double ox, oy, oz, dx, dy, dz;
	float dist;
	union vec3 direction;
	struct snis_entity *o;

	rc = read_and_unpack_buffer(buffer, "wSSSSSS",
			&oid,
			&ox, (int32_t) UNIVERSE_DIM,
			&oy, (int32_t) UNIVERSE_DIM,
			&oz, (int32_t) UNIVERSE_DIM,
			&dx, (int32_t) UNIVERSE_DIM,
			&dy, (int32_t) UNIVERSE_DIM,
			&dz, (int32_t) UNIVERSE_DIM);
	if (rc)
		return rc;
	pthread_mutex_lock(&universe_mutex);

	direction.v.x = dx - ox;
	direction.v.y = dy - oy;
	direction.v.z = dz - oz;
	dist = vec3_magnitude(&direction);
	vec3_normalize_self(&direction);
	if (oid == my_ship_id) {
		warp_tunnel_direction = direction;
		i = lookup_object_by_id(oid);
		if (i >= 0) {
			o = &go[i];
			if (fabs(ox - o->x) + fabs(oy - o->y) + fabs(oz - o->z) <
				MAX_PLAYER_VELOCITY * 2.0f)
				skip_source_effect = 1;
		}
	}
	if (!skip_source_effect)
		add_warp_effect(ox, oy, oz, 0, WARP_EFFECT_LIFETIME, &direction, dist);
	add_warp_effect(dx, dy, dz, 1, WARP_EFFECT_LIFETIME, &direction, dist);
	pthread_mutex_unlock(&universe_mutex);
	return 0;
}

struct universe_timestamp_sample {
	double offset;
};
static int nuniverse_timestamp_samples = 0;
static struct universe_timestamp_sample universe_timestamp_samples[UPDATE_UNIVERSE_TIMESTAMP_COUNT];

#if defined(__APPLE__)  || defined(__FreeBSD__)
static int universe_timestamp_sample_compare_less(void *vcx, const void *a, const void *b)
#else
static int universe_timestamp_sample_compare_less(const void *a, const void *b, void *vcx)
#endif
{
	const struct universe_timestamp_sample *A = a;
	const struct universe_timestamp_sample *B = b;

	if (A->offset > B->offset)
		return 1;
	if (A->offset < B->offset)
		return -1;
	return 0;
}

static void do_whatever_detonate_does(uint32_t id, double x, double y, double z,
					uint32_t time, double fractional_time)
{
	int i;
	struct snis_entity *o;
	float radius;
	union quat orientation;
	union vec3 u, v;

	i = lookup_object_by_id(id);
	if (i < 0)
		return;
	o = &go[i];
	switch (o->type) {
	case OBJTYPE_SHIP1:
	case OBJTYPE_SHIP2:
		radius = 1.25f * ship_mesh_map[o->tsd.ship.shiptype]->radius;
		break;
	case OBJTYPE_WARPGATE:
	case OBJTYPE_STARBASE:
		radius = 1.25 * entity_get_mesh(o->entity)->radius;
		break;
	default:
		return;
	}

	u.v.x = 0.0f;
	u.v.y = 0.0f;
	u.v.z = 1.0f;

	v.v.x = (float) (x - o->x);
	v.v.y = (float) (y - o->y);
	v.v.z = (float) (z - o->z);

	quat_from_u2v(&orientation, &u, &v, NULL);

	add_shield_effect(o, radius, &orientation);
}

static int process_detonate(void)
{
	unsigned char buffer[100];
	int rc;
	uint32_t id, time;
	double x, y, z, fractional_time;

	rc = read_and_unpack_buffer(buffer, "wSSSwU",
			&id,
			&x, (int32_t) UNIVERSE_DIM,
			&y, (int32_t) UNIVERSE_DIM,
			&z, (int32_t) UNIVERSE_DIM,
			&time,
			&fractional_time, (uint32_t) 5);
	if (rc)
		return rc;
	pthread_mutex_lock(&universe_mutex);
	do_whatever_detonate_does(id, x, y, z, time, fractional_time);
	pthread_mutex_unlock(&universe_mutex);
	return 0;
}

static void init_planetary_atmospheres(void)
{
	planetary_atmosphere_model_init_models(ATMOSPHERE_TYPE_GEN_SEED, NATMOSPHERE_TYPES);
}

static void reload_per_solarsystem_textures(char *old_solarsystem, char *new_solarsystem,
						struct solarsystem_asset_spec *old_assets,
						struct solarsystem_asset_spec *new_assets);
static int read_solarsystem_config(const char *solarsystem_name,
	struct solarsystem_asset_spec **assets);
static int process_set_solarsystem(void)
{
	char solarsystem[100];
	int rc;
	struct solarsystem_asset_spec *assets = NULL;

	rc = snis_readsocket(gameserver_sock, solarsystem, sizeof(solarsystem));
	if (rc != 0)
		return rc;
	solarsystem[99] = '\0';
	strncpy(old_solarsystem_name, solarsystem_name, sizeof(old_solarsystem_name) - 1);
	old_solarsystem_name[99] = '\0';
	strcpy(old_solarsystem_name, solarsystem_name);
	memcpy(dynamic_solarsystem_name, solarsystem, 100);
	printf("SET SOLARSYSTEM TO '%s'\n", dynamic_solarsystem_name);
	solarsystem_name = dynamic_solarsystem_name;
	if (read_solarsystem_config(dynamic_solarsystem_name, &assets)) {
		fprintf(stderr, "Failed re-reading new solarsystem metadata for %s\n",
			dynamic_solarsystem_name);
		return -1;
	}
	old_solarsystem_assets = solarsystem_assets;
	solarsystem_assets = assets;
	per_solarsystem_textures_loaded = 0;
	return 0;
}

static int process_update_universe_timestamp(double update_time)
{
	int rc;
	unsigned char buffer[100];
	uint8_t code;
	uint32_t timestamp;
	double time_delta;

	/* read the timestamp in server ticks and time_delta in seconds */
	rc = read_and_unpack_buffer(buffer, "bwS", &code, &timestamp, &time_delta, 5);
	if (rc)
		return rc;

	pthread_mutex_lock(&universe_mutex);

	if (code == UPDATE_UNIVERSE_TIMESTAMP_START_SAMPLE)
		nuniverse_timestamp_samples = 0;

	universe_timestamp_samples[nuniverse_timestamp_samples].offset =
		update_time - (double)timestamp / (double)UNIVERSE_TICKS_PER_SECOND - time_delta;

	printf("update_universe_timestamp i=%d, update_time=%f, timestamp=%d, time_delta=%f offset=%f\n",
		nuniverse_timestamp_samples, update_time, timestamp, time_delta,
		universe_timestamp_samples[nuniverse_timestamp_samples].offset);
	nuniverse_timestamp_samples++;

	if (code == UPDATE_UNIVERSE_TIMESTAMP_END_SAMPLE) {
		/* sort the samples by offset */
#if defined(__APPLE__)  || defined(__FreeBSD__)
		qsort_r(universe_timestamp_samples, nuniverse_timestamp_samples,
			sizeof(universe_timestamp_samples[0]), 0, universe_timestamp_sample_compare_less);
#else
		qsort_r(universe_timestamp_samples, nuniverse_timestamp_samples,
			sizeof(universe_timestamp_samples[0]), universe_timestamp_sample_compare_less, 0);
#endif
		/* get median offset */
		double median_offset;
		int mid_index = nuniverse_timestamp_samples / 2;
		if (nuniverse_timestamp_samples % 2 == 0)
			median_offset = (universe_timestamp_samples[mid_index - 1].offset +
				universe_timestamp_samples[mid_index].offset) / 2.0;
		else
			median_offset = universe_timestamp_samples[mid_index].offset;

		/* compute standard deviation of the offsets */
		int i;
		double mean_square = 0.0;
		for (i = 0; i < nuniverse_timestamp_samples; i++) {
			mean_square += (universe_timestamp_samples[i].offset - median_offset) *
				(universe_timestamp_samples[i].offset - median_offset);
		}
		double standard_deviation = sqrt(mean_square / (double)(nuniverse_timestamp_samples - 1));

		/* average offsets within one standard deviation */
		int nsoffset = 0;
		double soffset = 0.0;
		for (i = 0; i < nuniverse_timestamp_samples; i++) {
			if (fabs(universe_timestamp_samples[i].offset - median_offset) <= standard_deviation) {
				nsoffset++;
				soffset += universe_timestamp_samples[i].offset;
			}
		}

		double average_offset;
		if (nsoffset == 0)
			average_offset = median_offset;
		else
			average_offset = soffset / (double)nsoffset;

		universe_timestamp_offset = average_offset;

		printf("calc universe_timestamp median=%f sd=%f n_average=%d average_offset=%f\n", median_offset,
			standard_deviation, nsoffset, average_offset);
	}

	pthread_mutex_unlock(&universe_mutex);
	return 0;
}

static int process_latency_check(void)
{
	int rc;
	unsigned char buffer[20];
	uint64_t value[2];

	rc = read_and_unpack_buffer(buffer, "qq", &value[0], &value[1]);
	if (rc)
		return rc;
	/* Just turn it around and send back to server */
	queue_to_server(snis_opcode_pkt("bqq", OPCODE_LATENCY_CHECK, value[0], value[1]));
	return 0;
}

static int process_proximity_alert()
{
	static int last_time = 0;

	if ((timer - last_time) > (30 * 3))
		wwviaudio_add_sound(PROXIMITY_ALERT);
	last_time = timer;
	return 0;
}

static int process_atmospheric_friction()
{
	static int last_time = 0;

	if ((timer - last_time) > (30 * 3))
		wwviaudio_add_sound(ATMOSPHERIC_FRICTION);
	last_time = timer;
	return 0;
}

static int process_collision_notification()
{
	static int last_time = 0;

	if ((timer - last_time) > (30 * 3))
		wwviaudio_add_sound(SPACESHIP_CRASH);
	last_time = timer; 
	return 0;
}

static void delete_object(uint32_t id)
{
	int i;

	if (id == (uint32_t) -1) {
		fprintf(stderr, "snis_client: %s:%s:%d: BUG detected, attempted deletion of id -1\n",
			__FILE__, __func__, __LINE__);
		return;
	}

	i = lookup_object_by_id(id);
	/* perhaps we just joined and so don't know about this object. */
	if (i < 0)
		return;
	go[i].alive = 0;

	/* If science was targeting this guy, it isn't now. */
	if (curr_science_guy == &go[i]) {
		curr_science_guy = NULL;
		prev_science_guy = NULL;
	}
	if (prev_science_guy == &go[i])
		prev_science_guy = NULL;

	/* if demon screen was captain of this thing, it isn't now. */
	if (i == demon_ui.captain_of)
		demon_ui.captain_of = -1;
	if (id == demon_ui.follow_id) {
		demon_ui.follow_id = -1;
		print_demon_console_msg("NO LONGER FOLLOWING DELETED OBJECT");
	}
	remove_entity(ecx, go[i].entity);
	if (go[i].type == OBJTYPE_TURRET && go[i].tsd.turret.turret_base_entity)
		remove_entity(ecx, go[i].tsd.turret.turret_base_entity);
	if (go[i].type == OBJTYPE_BLOCK && go[i].tsd.block.shape.type == SHAPE_CAPSULE) {
		if (go[i].tsd.block.capsule_sphere[0])
			remove_entity(ecx, go[i].tsd.block.capsule_sphere[0]);
		if (go[i].tsd.block.capsule_sphere[1])
			remove_entity(ecx, go[i].tsd.block.capsule_sphere[1]);
		go[i].tsd.block.capsule_sphere[0] = NULL;
		go[i].tsd.block.capsule_sphere[1] = NULL;
		go[i].tsd.block.shape.type = 0;
	}
	go[i].entity = NULL;
	if (go[i].type == OBJTYPE_LASERBEAM || go[i].type == OBJTYPE_TRACTORBEAM) {
		if (go[i].tsd.laserbeam.laserflash_entity) {
			remove_entity(ecx, go[i].tsd.laserbeam.laserflash_entity);
			go[i].tsd.laserbeam.laserflash_entity = NULL;
		}
	}
	go[i].id = -1;
	if (go[i].sdata.science_text) {
		free(go[i].sdata.science_text);
		go[i].sdata.science_text = NULL;
	}
	if (go[i].type == OBJTYPE_PLANET && go[i].tsd.planet.custom_description) {
		free(go[i].tsd.planet.custom_description);
		go[i].tsd.planet.custom_description = NULL;
	}
	snis_object_pool_free_object(pool, i);
}

static void delete_all_objects(void)
{
	int i;
	pthread_mutex_lock(&universe_mutex);
	for (i = 0; i <= snis_object_pool_highest_object(pool); i++) {
		if (go[i].id == -1)
			continue;
		go[i].alive = 0;
		remove_entity(ecx, go[i].entity);
		go[i].entity = NULL;
		go[i].id = -1;
		snis_object_pool_free_object(pool, i);
	}
	pthread_mutex_unlock(&universe_mutex);
}

static void demon_deselect(uint32_t id);
static int process_delete_object_packet(void)
{
	unsigned char buffer[10];
	uint32_t id;
	int rc;

	assert(sizeof(buffer) > sizeof(struct delete_object_packet) - sizeof(uint8_t));
	rc = read_and_unpack_buffer(buffer, "w", &id);
	if (rc != 0)
		return rc;
	pthread_mutex_lock(&universe_mutex);
	demon_deselect(id);
	delete_object(id);
	pthread_mutex_unlock(&universe_mutex);
	return 0;
}

static int process_play_sound_packet(void)
{
	unsigned char buffer[10];
	uint16_t sound_number;
	int rc;

	assert(sizeof(buffer) > sizeof(struct play_sound_packet) - sizeof(uint8_t));
	rc = read_and_unpack_buffer(buffer, "h", &sound_number);
	if (rc != 0)
		return rc;
	wwviaudio_add_sound(sound_number);
	return 0;
}

static int process_ship_sdata_packet(void)
{
	unsigned char buffer[50];
	uint32_t id;
	uint8_t subclass, shstrength, shwavelength, shwidth,
		shdepth, faction, lifeform_count;
	int rc;
	char name[NAMESIZE];

	assert(sizeof(buffer) > sizeof(struct ship_sdata_packet) - sizeof(uint8_t));
	rc = snis_readsocket(gameserver_sock, buffer, sizeof(struct ship_sdata_packet) - sizeof(uint8_t));
	if (rc != 0)
		return rc;
	packed_buffer_unpack_raw(buffer, sizeof(buffer), "wbbbbbbbr", &id, &subclass, &shstrength, &shwavelength,
			&shwidth, &shdepth, &faction, &lifeform_count,
			name, (unsigned short) sizeof(name));
	pthread_mutex_lock(&universe_mutex);
	update_ship_sdata(id, subclass, name, shstrength, shwavelength, shwidth, shdepth, faction, lifeform_count);
	pthread_mutex_unlock(&universe_mutex);
	return 0;
}

static int process_ship_sdata_without_name(void)
{
	unsigned char buffer[50];
	uint32_t id;
	uint8_t subclass, shstrength, shwavelength, shwidth,
		shdepth, faction, lifeform_count;
	int rc;

	assert(sizeof(buffer) > sizeof(struct ship_sdata_without_name_packet) - sizeof(uint8_t));
	rc = snis_readsocket(gameserver_sock, buffer, sizeof(struct ship_sdata_without_name_packet) - sizeof(uint8_t));
	if (rc != 0)
		return rc;
	packed_buffer_unpack_raw(buffer, sizeof(buffer), "wbbbbbbb", &id, &subclass, &shstrength, &shwavelength,
			&shwidth, &shdepth, &faction, &lifeform_count);
	pthread_mutex_lock(&universe_mutex);
	update_ship_sdata(id, subclass, "-", shstrength, shwavelength, shwidth, shdepth, faction, lifeform_count);
	pthread_mutex_unlock(&universe_mutex);
	return 0;
}

static int process_update_ship_cargo_info(void)
{
	uint32_t id, item;
	uint8_t bay;
	double qty;
	int i, rc;
	struct snis_entity *o;
	unsigned char buffer[6 + 9 * MAX_CARGO_BAYS_PER_SHIP];
	uint8_t count;

	rc = read_and_unpack_buffer(buffer, "wb", &id, &count);
	if (rc)
		return rc;
	if (count == 0 || count > MAX_CARGO_BAYS_PER_SHIP)
		return -1;
	pthread_mutex_lock(&universe_mutex);
	i = lookup_object_by_id(id);
	if (i < 0)
		goto out;
	o = &go[i];
	if (o->type != OBJTYPE_SHIP1 && o->type != OBJTYPE_SHIP2)
		goto out;
	for (i = 0; i < count; i++) {
		rc = read_and_unpack_buffer(buffer, "bwS", &bay, &item, &qty, (int32_t) 1000000);
		if (rc)
			goto bail;
		if (bay >= MAX_CARGO_BAYS_PER_SHIP)
			goto bail;
		struct cargo_container_contents *cbc = &o->tsd.ship.cargo[bay].contents;
		cbc->item = item;
		cbc->qty = qty;
	}
out:
	pthread_mutex_unlock(&universe_mutex);
	return 0;
bail:
	pthread_mutex_unlock(&universe_mutex);
	return -1;
}

static int process_role_onscreen_packet(void)
{
	unsigned char buffer[sizeof(struct role_onscreen_packet)];
	uint8_t new_displaymode;
	int rc;

	rc = read_and_unpack_buffer(buffer, "b", &new_displaymode);
	if (rc != 0)
		return rc;

	if (!(role & ROLE_MAIN)) {
		/*
		 * Should "never" happen, server should only send display mode
		 * switching packets to clients that have role MAINSCREEN.
		 */
		printf("Unexpectedly got displaymode packet on "
			"client without mainscreen role.\n");
		return 0;
	}

	if (displaymode == new_displaymode) {
		wwviaudio_add_sound(OFFSCREEN_SOUND);
		displaymode = DISPLAYMODE_MAINSCREEN;
		return 0;
	}
	switch (new_displaymode) {
	case DISPLAYMODE_MAINSCREEN:
	case DISPLAYMODE_NAVIGATION:
	case DISPLAYMODE_WEAPONS:
	case DISPLAYMODE_ENGINEERING:
	case DISPLAYMODE_DAMCON:
	case DISPLAYMODE_SCIENCE:
	case DISPLAYMODE_COMMS:
		wwviaudio_add_sound(ONSCREEN_SOUND);
		displaymode = new_displaymode;
		break;
	default:
		printf("Got unexpected displaymode 0x%08x from server\n",
				new_displaymode);
		break;
	}
	return 0;
}

static struct science_ui {
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
} sci_ui;

static void science_activate_waypoints_widgets(void)
{
	int i;

	ui_hide_widget(sci_ui.scizoom);
	ui_hide_widget(sci_ui.scipower);
	ui_unhide_widget(sci_ui.add_waypoint_button);
	ui_unhide_widget(sci_ui.add_current_pos_button);
	ui_hide_widget(sci_ui.align_to_ship_button);
	ui_hide_widget(sci_ui.align_to_ship_button);

	for (i = 0; i < 3; i++)
		ui_unhide_widget(sci_ui.waypoint_input[i]);
	for (i = 0; i < sci_ui.nwaypoints; i++) {
		ui_unhide_widget(sci_ui.clear_waypoint_button[i]);
		ui_unhide_widget(sci_ui.select_waypoint_button[i]);
	}
	for (i = sci_ui.nwaypoints; i < MAXWAYPOINTS; i++) {
		ui_hide_widget(sci_ui.clear_waypoint_button[i]);
		ui_hide_widget(sci_ui.select_waypoint_button[i]);
	}
}

static void science_deactivate_waypoints_widgets(void)
{
	int i;

	ui_unhide_widget(sci_ui.scizoom);
	ui_unhide_widget(sci_ui.scipower);
	ui_unhide_widget(sci_ui.details_button);
	ui_unhide_widget(sci_ui.launch_mining_bot_button);
	ui_unhide_widget(sci_ui.tractor_button);
	ui_unhide_widget(sci_ui.threed_button);
	ui_unhide_widget(sci_ui.sciplane_button);
	ui_unhide_widget(sci_ui.waypoints_button);
	ui_unhide_widget(sci_ui.align_to_ship_button);
	ui_unhide_widget(sci_ui.align_to_ship_button);

	ui_hide_widget(sci_ui.add_waypoint_button);
	ui_hide_widget(sci_ui.add_current_pos_button);
	for (i = 0; i < 3; i++)
		ui_hide_widget(sci_ui.waypoint_input[i]);
	for (i = 0; i < MAXWAYPOINTS; i++) {
		ui_hide_widget(sci_ui.clear_waypoint_button[i]);
		ui_hide_widget(sci_ui.select_waypoint_button[i]);
	}
}

static int process_sci_details(void)
{
	unsigned char buffer[10];
	uint8_t new_details;
	int rc;

	rc = read_and_unpack_buffer(buffer, "b", &new_details);
	if (rc != 0)
		return rc;
	sci_ui.details_mode = new_details;
	if (new_details == 0)
		new_details = SCI_DETAILS_MODE_SCIPLANE;
	switch (new_details) {
	case SCI_DETAILS_MODE_THREED:
		science_deactivate_waypoints_widgets();
		ui_unhide_widget(sci_ui.align_to_ship_button);
		break;
	case SCI_DETAILS_MODE_DETAILS:
		science_deactivate_waypoints_widgets();
		ui_hide_widget(sci_ui.align_to_ship_button);
		break;
	case SCI_DETAILS_MODE_SCIPLANE:
		science_deactivate_waypoints_widgets();
		ui_hide_widget(sci_ui.align_to_ship_button);
		break;
	case SCI_DETAILS_MODE_WAYPOINTS:
		science_activate_waypoints_widgets();
		ui_hide_widget(sci_ui.align_to_ship_button);
		break;
	default:
		science_deactivate_waypoints_widgets();
		ui_hide_widget(sci_ui.align_to_ship_button);
		break;
	}

	return 0;
}

static int process_sci_select_target_packet(void)
{
	unsigned char buffer[sizeof(struct snis_sci_select_target_packet)];
	uint32_t id;
	uint8_t selection_type;
	int rc, i;

	rc = read_and_unpack_buffer(buffer, "bw", &selection_type, &id);
	if (rc != 0)
		return rc;

	switch (selection_type) {
	case OPCODE_SCI_SELECT_TARGET_TYPE_OBJECT:
		if (id == (uint32_t) -1) { /* deselection */
			curr_science_guy = NULL;
			wwviaudio_add_sound(SCIENCE_DATA_ACQUIRED_SOUND);
			return 0;
		}
		i = lookup_object_by_id(id);
		if (i >= 0) {
			if (curr_science_guy != &go[i])
				science_cam_timer = 0; /* Make the model swoop in on the sci details screen */
			curr_science_guy = &go[i];
			curr_science_waypoint = (uint32_t) -1;
			prev_science_waypoint = (uint32_t) -1;
			wwviaudio_add_sound(SCIENCE_DATA_ACQUIRED_SOUND);
		}
		return 0;
	case OPCODE_SCI_SELECT_TARGET_TYPE_WAYPOINT:
		if (id >= sci_ui.nwaypoints && id != (uint32_t) -1) /* not valid */
			return 0;
		wwviaudio_add_sound(SCIENCE_DATA_ACQUIRED_SOUND);
		if (id == curr_science_waypoint || id == (uint32_t) -1) {
			curr_science_waypoint = -1; /* deselect */
			prev_science_waypoint = -1;
		} else {
			curr_science_waypoint = id;
			prev_science_waypoint = id;
			curr_science_guy = NULL;
			prev_science_guy = NULL;
		}
		return 0;
	default:
		return 0;
	}
}

static int process_update_respawn_time(void)
{
	unsigned char buffer[sizeof(struct respawn_time_packet)];
	struct snis_entity *o;
	int rc;
	uint8_t seconds;

	rc = read_and_unpack_buffer(buffer, "b", &seconds);
	if (rc != 0)
		return rc;
	if (!(o = find_my_ship()))
		return 0;
	o->respawn_time = (uint32_t) seconds;
	return 0;
}

static int process_update_netstats(void)
{
	unsigned char buffer[sizeof(struct netstats_packet)];
	struct timespec ts;
	int rc;
	uint64_t bytes_recd_last_time, bytes_sent_last_time;
	float elapsed_time_ms;
	uint32_t latency_in_usec;

	bytes_recd_last_time = netstats.bytes_recd;
	bytes_sent_last_time = netstats.bytes_sent;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	elapsed_time_ms = (1000.0 * ts.tv_sec + 0.000001 * ts.tv_nsec) -
		(1000.0 * netstats.lasttime.tv_sec + 0.000001 * netstats.lasttime.tv_nsec);
	rc = read_and_unpack_buffer(buffer, "qqwwwwwwwww", &netstats.bytes_sent,
				&netstats.bytes_recd, &netstats.nobjects,
				&netstats.nships, &netstats.elapsed_seconds,
				&latency_in_usec,
				&netstats.faction_population[0],
				&netstats.faction_population[1],
				&netstats.faction_population[2],
				&netstats.faction_population[3],
				&netstats.faction_population[4]);
	if (rc != 0)
		return rc;
	if (elapsed_time_ms < 0.0000001)
		elapsed_time_ms = 0.0000001;
	netstats.bytes_recd_per_sec[netstats.bps_index] = (uint32_t) ((1000.0 *
		(netstats.bytes_recd - bytes_recd_last_time)) / elapsed_time_ms);
	netstats.bytes_sent_per_sec[netstats.bps_index] = (uint32_t) ((1000.0 *
		(netstats.bytes_sent - bytes_sent_last_time)) / elapsed_time_ms);
	snis_scaling_strip_chart_update(demon_ui.bytes_sent_strip_chart,
				(float) netstats.bytes_sent_per_sec[netstats.bps_index]);
	snis_scaling_strip_chart_update(demon_ui.bytes_recd_strip_chart,
				(float) netstats.bytes_recd_per_sec[netstats.bps_index]);
	netstats.bps_index = (netstats.bps_index + 1) & 0x01;
	netstats.lasttime = ts;
	snis_scaling_strip_chart_update(demon_ui.latency_strip_chart, 0.001 * (float) latency_in_usec);
	return 0;
}

static struct comms_ui {
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
} comms_ui;

static void comms_dirkey(int h, int v)
{
	/* note: No point making round trip to server and fanning out
	 * scrolling requests to all comms clients because the comms
	 * clients are not guaranteed to have identical text window
	 * buffer contents due to late joiners, etc.  So just scroll
	 * locally on the client.  Breaks my "onscreen" paradigm here.
	 */
	if (v < 0)
		text_window_scroll_up(comms_ui.tw);
	else if (v > 0)
		text_window_scroll_down(comms_ui.tw);
}

static void do_pageup(void)
{
	switch (displaymode) {
	case DISPLAYMODE_COMMS:
		text_window_page_up(comms_ui.tw);
		break;
	default:
		break;
	}
}

static void do_pagedown(void)
{
	switch (displaymode) {
	case DISPLAYMODE_COMMS:
		text_window_page_down(comms_ui.tw);
		break;
	default:
		break;
	}
}

static int cipher_freq_compare(const void *a, const void *b)
{
	char x, y;
	int f1, f2;

	x = *(char *) a;
	y = *(char *) b;

	if (x < 'A' || x > 'Z' || y < 'A' || y > 'Z')
		return 0;
	f1 = cipher_freq[x - 'A'];
	f2 = cipher_freq[y - 'A'];
	return f2 - f1;
}

/* Sort the letters of the enciphered message by frequency */
static void sort_cipher_alpha_by_freq(int cipher_freq[], char cipher_alpha_by_freq[])
{
	memcpy(cipher_alpha_by_freq, "ABCDEFGHIJKLMNOPQRSTUVWXYZ", 27);
	qsort(cipher_alpha_by_freq, 26, 1, cipher_freq_compare);
}

static void main_screen_add_text(char *msg);
static int process_comm_transmission(void)
{
	unsigned char buffer[sizeof(struct comms_transmission_packet) + 100];
	char string[256];
	uint8_t length, enciphered;
	int i, e, j, rc, n;
	uint32_t comms_channel;
	const char *channel_change_pattern = COMMS_CHANNEL_CHANGE_MSG " %u";
	char *channel_change_msg;
	char enc_msg[80];
	const int linewidth = 75;

	/* REMINDER: If the format of OPCODE_COMMS_TRANSMISSION ever changes, you need to
	 * fix distort_comms_message in snis_server.c too.
	 */
	rc = read_and_unpack_buffer(buffer, "bb", &enciphered, &length);
	if (rc != 0)
		return rc;
	rc = snis_readsocket(gameserver_sock, string, length);
	if (rc != 0)
		return rc;

	switch (enciphered) {
	case OPCODE_COMMS_PLAINTEXT:
		string[79] = '\0';
		string[length] = '\0';
		text_window_add_text(comms_ui.tw, string);
		main_screen_add_text(string);
		if (strstr(string, ": *** HAILING "))
			wwviaudio_add_sound(COMMS_HAIL_SOUND);
		/* This is a little hoaky.  The client doesn't actually know which
		 * channel comms is on, that's 100% server side.  But we can snoop
		 * channel change messages so we can have a channel indicator.
		 */
		channel_change_msg = strstr(string, COMMS_CHANNEL_CHANGE_MSG);
		if (channel_change_msg) {
			n = sscanf(channel_change_msg, channel_change_pattern, &comms_channel);
			if (n == 1)
				comms_ui.channel = comms_channel;
		}
		break;
	case OPCODE_COMMS_ENCIPHERED:
		text_window_add_color_text(comms_ui.tw, "ENCRYPTED MESSAGE RECIEVED", UI_COLOR(comms_encrypted));
		string[255] = '\0';
		string[length] = '\0';
		enc_msg[0] = '\0';
		e = 0;
		for (i = 0; string[i]; i++) {
			e = i % linewidth;
			enc_msg[e] = string[i];
			if (e == linewidth - 1) {
				enc_msg[e + 1] = '\0';
				text_window_add_color_text(comms_ui.tw, enc_msg, UI_COLOR(comms_encrypted));
			}
		}
		if (e != linewidth - 1) {
			enc_msg[e + 1] = '\0';
			text_window_add_color_text(comms_ui.tw, enc_msg, UI_COLOR(comms_encrypted));
		}
		text_window_add_color_text(comms_ui.tw, "[ - - - ]", UI_COLOR(comms_encrypted));
		break;
	case OPCODE_COMMS_UPDATE_ENCIPHERED:
		string[255] = '\0';
		string[length] = '\0';
		memcpy(enciphered_text, string, length + 1);
		memset(cipher_freq, 0, sizeof(cipher_freq));
		for (i = 0; enciphered_text[i]; i++) {
			int c = toupper(enciphered_text[i]);
			if (c >= 'A' && c <= 'Z')
				cipher_freq[c - 'A']++;
		}
		sort_cipher_alpha_by_freq(cipher_freq, cipher_alpha_by_freq);
		break;
	case OPCODE_COMMS_KEY_GUESS:
		string[26] = '\0';
		if (length != 26)
			return -1; /* protocol error */
		for (i = 0; i < 26; i++) {
			char contents[3];
			if (string[i] >= 'A' && string[i] <= 'Z') {
				if (cipher_key[i] == string[i])
					continue;
				/* Clear out any box that already has these contents */
				for (j = 0; j < 26; j++) {
					if (comms_ui.crypt_alpha_text[j][0] == i + 'A') {
						contents[0] = '_';
						contents[1] = '\0';
						contents[2] = '\0';
						snis_text_input_box_set_contents(comms_ui.crypt_alpha[j], contents);
					}
				}
				/* Set the correct box to these contents */
				contents[0] = i + 'A';
				contents[1] = '\0';
				contents[2] = '\0';
				snis_text_input_box_set_contents(comms_ui.crypt_alpha[string[i] - 'A'], contents);
			}
		}
		memcpy(cipher_key, string, 26);
		break;
	default:
		return -1; /* protocol error */
	}
	return 0;
}

static int process_textscreen_op(void)
{
	unsigned char buffer[sizeof(textscreen) + 20];
	uint8_t subcode;
	uint16_t length, timevalue;
	int rc;

	rc = read_and_unpack_buffer(buffer, "b", &subcode);
	if (rc != 0)
		return -1;
	switch (subcode) {
	case OPCODE_TEXTSCREEN_CLEAR:
		textscreen_timer = 0;
		memset(textscreen, 0, sizeof(textscreen));
		break;
	case OPCODE_TEXTSCREEN_TIMEDTEXT:
		rc = read_and_unpack_buffer(buffer, "hh", &length, &timevalue);
		if (rc != 0)
			return rc;
		if (length > 1023)
			return -1;
		if (timevalue > 120)
			return -1;
		memset(buffer, 0, sizeof(buffer));
		rc = snis_readsocket(gameserver_sock, buffer, length);
		if (rc != 0)
			return rc;
		memcpy(textscreen, buffer, sizeof(textscreen));
		textscreen_timer = timevalue * frame_rate_hz;
		break;
	case OPCODE_TEXTSCREEN_MENU:
		rc = read_and_unpack_buffer(buffer, "h", &length);
		if (rc != 0)
			return rc;
		if (length > 1023)
			return -1;
		memset(buffer, 0, sizeof(buffer));
		rc = snis_readsocket(gameserver_sock, buffer, length);
		if (rc != 0)
			return rc;
		memcpy(textscreen, buffer, sizeof(textscreen));
		user_defined_menu_active = 1;
		break;
	default:
		return -1;
	}
	return 0;
}

/* Server can request client to send back expected format for any opcode */
static int process_check_opcode_format(void)
{
	unsigned char buffer[10];
	uint8_t subcode, opcode = OPCODE_NOOP;
	uint16_t len;
	const char *format;
	int rc;

	rc = read_and_unpack_buffer(buffer, "bb", &subcode, &opcode);
	if (rc != 0)
		return -1;
	/* fprintf(stderr, "snis_client: checking opcode, check = %hhu, opcode = %hhu\n",
		subcode, opcode); */
	switch (subcode) {
	case OPCODE_CHECK_OPCODE_UNKNOWN:
		fprintf(stderr, "\n\n\n\n\n\n"
			"snis_client: ==> Server reports opcode unknown: %hhu <==\n"
			"\n\n\n\n", opcode);
		break;
	case OPCODE_CHECK_OPCODE_MATCH:
		/* fprintf(stderr, "snis_client: Opcode %hhu OK.\n", opcode); */
		break;
	case OPCODE_CHECK_OPCODE_QUERY:
		/* Server is asking us to send our format for an opcode for verification */
		format = snis_opcode_format(opcode);
		if (format) {
			struct packed_buffer *pb;

			len = strlen(format);
			pb = packed_buffer_allocate(strlen(format) + 10);
			packed_buffer_append(pb, "bbbh", OPCODE_CHECK_OPCODE_FORMAT,
						OPCODE_CHECK_OPCODE_VERIFY, opcode, len);
			packed_buffer_append_raw(pb, format, len);
			queue_to_server(pb);
		} else {
			fprintf(stderr, "\n\n\n\n\n"
				"snis_client:  ==> Server queried UNKNOWN opcode %hhu <==\n"
				"\n\n\n\n", opcode);
			queue_to_server(packed_buffer_new("bbb", OPCODE_CHECK_OPCODE_FORMAT,
							OPCODE_CHECK_OPCODE_UNKNOWN, opcode));
		}
		break;
	case OPCODE_CHECK_OPCODE_VERIFY: /* We do not expect this from the server. */
		fprintf(stderr, "snis_client: Unexpected CHECK_OPCODE_FORMAT+VERIFY from server.");
		return -1;
	case OPCODE_CHECK_OPCODE_MISMATCH:
		/* Server reports one of this client's opcodes does not have correct format. */
		fprintf(stderr, "\n\n\n\n\n"
			"snis_client:  ==> Server reports format mismatch for opcode %hhu <==\n"
			"\n\n\n\n", opcode);
		break;
	default:
		return -1;
	}
	return 0;
}

static int process_update_solarsystem_location(void)
{
	int i, rc, found;
	double x, y, z;
	unsigned char buffer[1 + 4 + 4 + 4 + SSGL_LOCATIONSIZE];
	char name[SSGL_LOCATIONSIZE];

	rc = read_and_unpack_buffer(buffer, "SSS",
			&x, (int32_t) 1000,
			&y, (int32_t) 1000,
			&z, (int32_t) 1000);
	if (rc != 0)
		return -1;
	rc = snis_readsocket(gameserver_sock, name, SSGL_LOCATIONSIZE);
	if (rc != 0)
		return rc;
	name[SSGL_LOCATIONSIZE - 1] = '\0';
	pthread_mutex_lock(&universe_mutex);
	found = 0;

	for (i = 0; i < nstarmap_entries; i++) {
		if (strncasecmp(starmap[i].name, name, SSGL_LOCATIONSIZE) != 0)
			continue;
		found = 1;
		starmap[i].x = x;
		starmap[i].y = y;
		starmap[i].z = z;
		starmap[i].time_before_expiration = frame_rate_hz * 10; /* 10 seconds */
	}
	if (!found) {
		if (nstarmap_entries < MAXSTARMAPENTRIES) {
			i = nstarmap_entries;
			memcpy(starmap[i].name, name, SSGL_LOCATIONSIZE);
			starmap[i].x = x;
			starmap[i].y = y;
			starmap[i].z = z;
			starmap[i].time_before_expiration = frame_rate_hz * 10; /* 10 seconds */
			nstarmap_entries++;
		}
	}
	starmap_compute_adjacencies(starmap_adjacency, starmap, nstarmap_entries);
	pthread_mutex_unlock(&universe_mutex);
	return 0;
}

static void expire_starmap_entries(void)
{
	int i, j, changed;

	changed = 0;
	for (i = 0; i < nstarmap_entries; /* no increment here */) {
		starmap[i].time_before_expiration--;
		if (starmap[i].time_before_expiration <= 0) {
			for (j = i; j < nstarmap_entries - 1; j++)
				starmap[j] = starmap[j + 1];
			nstarmap_entries--;
			changed = 1;
		} else {
			i++;
		}
	}
	if (changed)
		starmap_compute_adjacencies(starmap_adjacency, starmap, nstarmap_entries);
}

static int process_set_waypoint(void)
{
	int rc;
	uint8_t subcode, row, count;
	uint32_t row32;
	double x, y, z;
	unsigned char buffer[64];

	rc = read_and_unpack_buffer(buffer, "b", &subcode);
	if (rc)
		return rc;
	switch (subcode) {
	case OPCODE_SET_WAYPOINT_ROW:
		rc = read_and_unpack_buffer(buffer, "bSSS", &row,
			&x, (int32_t) UNIVERSE_DIM,
			&y, (int32_t) UNIVERSE_DIM,
			&z, (int32_t) UNIVERSE_DIM);
		if (rc != 0)
			return -1;
		if (row >= MAXWAYPOINTS) {
			fprintf(stderr, "snis_client: Bad row for OPCODE_SET_WAYPOINT: %hhu\n", row);
			return -1;
		}
		sci_ui.waypoint[row][0] = x;
		sci_ui.waypoint[row][1] = y;
		sci_ui.waypoint[row][2] = z;
		return 0;
	case OPCODE_SET_WAYPOINT_COUNT:
		rc = read_and_unpack_buffer(buffer, "b", &count);
		if (rc)
			return rc;
		if (count <= MAXWAYPOINTS) {
			sci_ui.nwaypoints = count;
			if (count <= curr_science_waypoint) {
				curr_science_waypoint = -1;
				prev_science_waypoint = -1;
			}
		}
		return 0;
	case OPCODE_SET_WAYPOINT_UPDATE_SELECTION: /* silent update of selection */
		rc = read_and_unpack_buffer(buffer, "w", &row32);
		if (rc)
			return rc;
		row = row32;
		if (row32 == (uint32_t) -1 || row32 < sci_ui.nwaypoints) {
			curr_science_waypoint = row32;
			prev_science_waypoint = row32;
			if (row32 != (uint32_t) -1) {
				curr_science_guy = NULL;
				prev_science_guy = NULL;
			}
		}
		return 0;
	default:
		fprintf(stderr, "snis_client: Bad subcode for OPCODE_SET_WAYPOINT: %hhu\n", subcode);
		return -1;
	}
}

static int process_rts_func(void)
{
	int i, rc, found = 0;
	uint8_t subcode;
	uint32_t id, health;
	unsigned char buffer[64];
	uint8_t faction;

	rc = read_and_unpack_buffer(buffer, "b", &subcode);
	if (rc)
		return rc;
	switch (subcode) {
	case OPCODE_RTS_FUNC_MAIN_BASE_UPDATE:
		rc = read_and_unpack_buffer(buffer, "bww", &faction, &id, &health);
		if (rc)
			return rc;
		for (i = 0; i < ARRAYSIZE(rts_planet); i++) {
			if (rts_planet[i].id == id && rts_planet[i].faction != 255) {
				rts_planet[i].health = (uint16_t) health;
				found = 1;
				break;
			}
		}
		if (!found) {
			for (i = 0; i < ARRAYSIZE(rts_planet); i++) {
				if (rts_planet[i].faction == 255) {
					int index = lookup_object_by_id(id);
					if (index >= 0 && go[index].type == OBJTYPE_PLANET) {
							rts_planet[i].id = id;
							rts_planet[i].health = (uint16_t) health;
							rts_planet[i].faction = faction;
							go[index].sdata.faction = faction;
					}
					break;
				}
			}
		}
		break;
	default:
		return -1;
	}
	return 0;
}

static int process_demon_rtsmode(void)
{
	int i, rc;
	uint8_t subcode;
	unsigned char buffer[64];

	rc = read_and_unpack_buffer(buffer, "b", &subcode);
	if (rc)
		return rc;
	switch (subcode) {
	case OPCODE_RTSMODE_SUBCMD_ENABLE:
	case OPCODE_RTSMODE_SUBCMD_DISABLE:
		my_home_planet_oid = UNKNOWN_ID; /* Clear this cached value */
		/* Clear the rts_planet array */
		for (i = 0; i < ARRAYSIZE(rts_planet); i++) {
			rts_planet[i].health = 0;
			rts_planet[i].id = 0;
			rts_planet[i].faction = 255;
		}
		break;
	default:
		return -1;
	}
	return 0;
}

static int process_adjust_tts_volume(void)
{
	int rc;
	uint8_t volume;
	uint8_t buffer[10];

	rc = read_and_unpack_buffer(buffer, "b", &volume);
	if (rc)
		return rc;
	if (volume > 255.0)
		volume = 255.0;
	if (volume < 0.0)
		volume = 0.0;
	text_to_speech_volume = (float) volume / 255.0;
	return 0;
}

static void do_text_to_speech(char *text)
{
	char command[PATH_MAX];
	char *fixed_text;
	char *bindir;
	struct stat statbuf;
	int rc;
	static char last_speech[1000] = { 0 };
	static int last_time = { 0 };

	bindir = get_snis_bin_dir();

	/* test that snisbindir is actually a directory. */
	rc = stat(bindir, &statbuf);
	if (rc < 0) {
		fprintf(stderr, "Cannot stat %s: %s\n", bindir, strerror(errno));
		return;
	}
	if (!S_ISDIR(statbuf.st_mode)) {
		fprintf(stderr, "%s is not a directory.\n", bindir);
		return;
	}
	remove_single_quotes(text);
	fixed_text = fix_pronunciation(text);

	/* Suppress temporally proximate duplicate speech requests */
	if (timer - last_time < frame_rate_hz * 6
		&& strncmp(fixed_text, last_speech, sizeof(last_speech) - 1) == 0) {
		free(fixed_text);
		return;
	}

	strncpy(last_speech, fixed_text, sizeof(last_speech) - 1);
	last_time = timer;

	snprintf(command, sizeof(command), "export SNIS_TTS_VOLUME=%1.2f; %s/snis_text_to_speech.sh '%s'",
				text_to_speech_volume, bindir, fixed_text);
	free(fixed_text);
	if (echo_computer_to_comms) {
		char tx[80];
		int chars_left, len;

		len = chars_left  = strlen(text);
		uppercase(text);

		text_window_add_color_text(comms_ui.tw, "COMPUTER:", UI_COLOR(comms_computer_output));
		while (chars_left > 0) {
			strncpy(tx, "- ", 3);
			strncat(tx, text + len - chars_left, 77);
			tx[79] = '\0';
			text_window_add_color_text(comms_ui.tw, tx, UI_COLOR(comms_computer_output));
			chars_left -= strlen(tx) - 2;
		}
	}
	rc = system(command);
	if (rc != 0 && errno != ECHILD)  { /* we have ignored SIGCHLD, so we get ECHILD here */
		fprintf(stderr, "Shell command '%s' returned %d, errno = %d (%s)\n",
			command, rc, errno, strerror(errno));
	}
}

/* Wait for something to appear at the head of the text to speech queue */
static void text_to_speech_wait_for_input(void)
{
	int rc;

	pthread_mutex_lock(&text_to_speech_mutex);
	if (text_to_speech_thread_time_to_quit) {
		pthread_mutex_unlock(&text_to_speech_mutex);
		return;
	}
	while (!text_to_speech_queue_head) {
		rc = pthread_cond_wait(&text_to_speech_cond,
			&text_to_speech_mutex);
		if (text_to_speech_thread_time_to_quit)
			break;
		if (rc != 0)
			printf("text_to_speech_wait_for_input: pthread_cond_wait failed.\n");
		if (text_to_speech_queue_head)
			break;
	}
	pthread_mutex_unlock(&text_to_speech_mutex);
}

/* Thread to process text to speech queue */
static void *text_to_speech_thread_fn(__attribute__((unused)) void *arg)
{
	struct text_to_speech_queue_entry *entry = NULL;

	do {
		text_to_speech_wait_for_input();
		pthread_mutex_lock(&text_to_speech_mutex);
		if (text_to_speech_thread_time_to_quit) {
			pthread_mutex_unlock(&text_to_speech_mutex);
			break;
		}
		if (text_to_speech_queue_head) {
			entry = text_to_speech_queue_head;
			text_to_speech_queue_head = entry->next;
			/* if we consumed the last item, the tail is now NULL too */
			if (!text_to_speech_queue_head)
				text_to_speech_queue_tail = NULL;
		}
		pthread_mutex_unlock(&text_to_speech_mutex);
		if (entry) {
			if (entry->text) {
				do_text_to_speech(entry->text);
				free(entry->text);
			}
			free(entry);
			entry = NULL;
		}
	} while (1);
	return NULL;
}

static void setup_text_to_speech_thread(void)
{
	int rc;

	text_to_speech_queue_head = NULL;
	text_to_speech_queue_tail = NULL;
	pthread_mutex_init(&text_to_speech_mutex, NULL);
	rc = create_thread(&text_to_speech_thread, text_to_speech_thread_fn, NULL, "snisc-tts", 0);
	if (rc)
		fprintf(stderr, "Failed to create text to speech thread.\n");
}

static void stop_text_to_speech_thread(void)
{
	int rc;

	pthread_mutex_lock(&text_to_speech_mutex);
	if (!text_to_speech_thread) {
		pthread_mutex_unlock(&text_to_speech_mutex);
		return;
	}
	text_to_speech_thread_time_to_quit = 1;
	rc = pthread_cond_broadcast(&text_to_speech_cond);
	if (rc)
		printf("huh... pthread_cond_broadcast failed in text_to_speech().\n");
	pthread_mutex_unlock(&text_to_speech_mutex);
}

static void text_to_speech(char *text)
{
	struct text_to_speech_queue_entry *entry;
	int rc;

	/* Create a new entry */
	entry = malloc(sizeof(*entry));
	entry->text = strdup(text);
	entry->next = NULL;

	/* Add new entry to the tail of the queue */
	pthread_mutex_lock(&text_to_speech_mutex);
	if (text_to_speech_queue_tail == NULL)
		text_to_speech_queue_head = entry;
	else
		text_to_speech_queue_tail->next = entry;
	text_to_speech_queue_tail = entry;

	/* Wake up the text to speech queue processing thread */
	rc = pthread_cond_broadcast(&text_to_speech_cond);
	if (rc)
		printf("huh... pthread_cond_broadcast failed in text_to_speech().\n");
	pthread_mutex_unlock(&text_to_speech_mutex);
}

static int process_natural_language_request(void)
{
	unsigned char buffer[256];
	char string[256];
	uint8_t length, subcommand;
	int rc;

	rc = read_and_unpack_buffer(buffer, "bb", &subcommand, &length);
	if (rc != 0)
		return rc;
	if (subcommand != OPCODE_NL_SUBCOMMAND_TEXT_TO_SPEECH)
		return -1;
	memset(string, 0, sizeof(string));
	rc = snis_readsocket(gameserver_sock, string, length);
	if (rc)
		return rc;
	string[255] = '\0';
	string[length] = '\0';
	text_to_speech(string);
	return 0;
}


static int process_ship_damage_packet(int do_damage_limbo)
{
	char buffer[sizeof(struct ship_damage_packet)];
	struct packed_buffer pb;
	uint32_t id;
	int rc, i;
	struct ship_damage_data damage;

	rc = snis_readsocket(gameserver_sock, buffer,
			sizeof(struct ship_damage_packet) - sizeof(uint8_t));
	if (rc != 0)
		return rc;
	packed_buffer_init(&pb, buffer, sizeof(buffer));
	id = packed_buffer_extract_u32(&pb);
	i = lookup_object_by_id(id);
	
	if (i < 0)
		return 0;
	packed_buffer_extract_raw(&pb, (char *) &damage, sizeof(damage));
	pthread_mutex_lock(&universe_mutex);
	go[i].tsd.ship.damage = damage;
	go[i].tsd.ship.flames_timer = 15 * frame_rate_hz; /* 15 secs of possible flames */
	pthread_mutex_unlock(&universe_mutex);
	if (id == my_ship_id && do_damage_limbo) {
		damage_limbo_countdown = 2;
		main_camera_shake = 1.0;
	}
	return 0;
}

static int process_switch_server(void)
{
	char buffer[100];
	struct packed_buffer pb;
	int rc;

	rc = snis_readsocket(gameserver_sock, buffer, 23);
	if (rc != 0)
		return rc;
	packed_buffer_init(&pb, buffer, sizeof(buffer));
	switch_warp_gate_number = packed_buffer_extract_u8(&pb);
	packed_buffer_extract_raw(&pb, switch_server_location_string, 20);
	switch_server_location_string[19] = '\0';
	return 0;
}

static int process_update_block_packet(void)
{
	unsigned char buffer[100];
	uint32_t id, timestamp;
	double dx, dy, dz, dsx, dsy, dsz;
	union quat orientation;
	uint8_t block_material_index, health, form;
	int rc;

	rc = read_and_unpack_buffer(buffer, "wwSSSSSSQbbb", &id, &timestamp,
			&dx, (int32_t) UNIVERSE_DIM,
			&dy, (int32_t) UNIVERSE_DIM,
			&dz, (int32_t) UNIVERSE_DIM,
			&dsx, (int32_t) UNIVERSE_DIM,
			&dsy, (int32_t) UNIVERSE_DIM,
			&dsz, (int32_t) UNIVERSE_DIM,
			&orientation,
			&block_material_index,
			&health,
			&form);
	if (rc != 0)
		return rc;
	pthread_mutex_lock(&universe_mutex);
	rc = update_block(id, timestamp, dx, dy, dz, dsx, dsy, dsz,
				&orientation, block_material_index, health, form);
	pthread_mutex_unlock(&universe_mutex);
	return (rc < 0);
}

static int process_update_turret_packet(void)
{
	unsigned char buffer[100];
	uint32_t id, timestamp;
	double dx, dy, dz;
	union quat orientation, base_orientation;
	int rc;
	uint8_t health;

	rc = read_and_unpack_buffer(buffer, "wwSSSQQb", &id, &timestamp,
			&dx, (int32_t) UNIVERSE_DIM,
			&dy, (int32_t) UNIVERSE_DIM,
			&dz, (int32_t) UNIVERSE_DIM,
			&orientation, &base_orientation, &health);
	if (rc != 0)
		return rc;
	pthread_mutex_lock(&universe_mutex);
	rc = update_turret(id, timestamp, dx, dy, dz, &orientation, &base_orientation, health);
	pthread_mutex_unlock(&universe_mutex);
	return (rc < 0);
}


static int process_update_asteroid_packet(void)
{
	unsigned char buffer[100];
	uint32_t id, timestamp;
	double dx, dy, dz;
	int rc;

	assert(sizeof(buffer) > sizeof(struct update_asteroid_packet) - sizeof(uint8_t));
	rc = read_and_unpack_buffer(buffer, "wwSSS", &id, &timestamp,
			&dx, (int32_t) UNIVERSE_DIM,
			&dy,(int32_t) UNIVERSE_DIM,
			&dz, (int32_t) UNIVERSE_DIM);
	if (rc != 0)
		return rc;
	pthread_mutex_lock(&universe_mutex);
	rc = update_asteroid(id, timestamp, dx, dy, dz);
	pthread_mutex_unlock(&universe_mutex);
	return (rc < 0);
}

static int process_update_asteroid_minerals_packet(void)
{
	unsigned char buffer[100];
	uint32_t id;
	int rc;
	uint8_t carbon, nickeliron, silicates, preciousmetals;

	assert(sizeof(buffer) > sizeof(struct update_asteroid_minerals_packet) - sizeof(uint8_t));
	rc = read_and_unpack_buffer(buffer, "wbbbb", &id,
			&carbon, &nickeliron, &silicates, &preciousmetals);
	if (rc != 0)
		return rc;
	pthread_mutex_lock(&universe_mutex);
	rc = update_asteroid_minerals(id, carbon, nickeliron, silicates, preciousmetals);
	pthread_mutex_unlock(&universe_mutex);
	return (rc < 0);
}

static int process_update_warp_core_packet(void)
{
	unsigned char buffer[100];
	uint32_t id, timestamp;
	double dx, dy, dz;
	int rc;

	assert(sizeof(buffer) > sizeof(struct update_warp_core_packet) - sizeof(uint8_t));
	rc = read_and_unpack_buffer(buffer, "wwSSS", &id, &timestamp,
			&dx, (int32_t) UNIVERSE_DIM,
			&dy, (int32_t) UNIVERSE_DIM,
			&dz, (int32_t) UNIVERSE_DIM);
	if (rc != 0)
		return rc;
	pthread_mutex_lock(&universe_mutex);
	rc = update_warp_core(id, timestamp, dx, dy, dz);
	pthread_mutex_unlock(&universe_mutex);
	return (rc < 0);
}

static int process_update_cargo_container_packet(void)
{
	unsigned char buffer[100];
	uint32_t id, timestamp, item;
	double dx, dy, dz, qty;
	int rc;

	assert(sizeof(buffer) > sizeof(struct update_cargo_container_packet) - sizeof(uint8_t));
	rc = read_and_unpack_buffer(buffer, "wwSSSwS", &id, &timestamp,
			&dx, (int32_t) UNIVERSE_DIM,
			&dy,(int32_t) UNIVERSE_DIM,
			&dz, (int32_t) UNIVERSE_DIM,
			&item, &qty, (int32_t) 1000000);
	if (rc != 0)
		return rc;
	pthread_mutex_lock(&universe_mutex);
	rc = update_cargo_container(id, timestamp, dx, dy, dz, item, qty);
	pthread_mutex_unlock(&universe_mutex);
	return (rc < 0);
} 

static int process_update_cargo_container_position(void)
{
	unsigned char buffer[100];
	uint32_t id, timestamp;
	double dx, dy, dz;
	int rc;

	assert(sizeof(buffer) > sizeof(struct update_cargo_container_position) - sizeof(uint8_t));
	rc = read_and_unpack_buffer(buffer, "wwSSS", &id, &timestamp,
			&dx, (int32_t) UNIVERSE_DIM,
			&dy,(int32_t) UNIVERSE_DIM,
			&dz, (int32_t) UNIVERSE_DIM);
	if (rc != 0)
		return rc;
	pthread_mutex_lock(&universe_mutex);
	rc = update_cargo_container(id, timestamp, dx, dy, dz, -1, -1);
	pthread_mutex_unlock(&universe_mutex);
	return (rc < 0);
} 

static int process_update_derelict_packet(void)
{
	unsigned char buffer[100];
	uint32_t id, timestamp, orig_ship_id;
	double dx, dy, dz;
	uint8_t shiptype, fuel, oxygen;
	int rc;

	assert(sizeof(buffer) > sizeof(struct update_asteroid_packet) - sizeof(uint8_t));
	rc = read_and_unpack_buffer(buffer, "wwSSSbbbw", &id, &timestamp,
			&dx, (int32_t) UNIVERSE_DIM,
			&dy,(int32_t) UNIVERSE_DIM,
			&dz, (int32_t) UNIVERSE_DIM,
			&shiptype, &fuel, &oxygen, &orig_ship_id);
	if (rc != 0)
		return rc;
	pthread_mutex_lock(&universe_mutex);
	rc = update_derelict(id, timestamp, dx, dy, dz, shiptype, fuel, oxygen, orig_ship_id);
	pthread_mutex_unlock(&universe_mutex);
	return (rc < 0);
} 

static int process_update_black_hole_packet(void)
{
	unsigned char buffer[100];
	uint32_t id, timestamp;
	double dx, dy, dz, r;
	int rc;

	assert(sizeof(buffer) > sizeof(struct update_black_hole_packet) - sizeof(uint8_t));
	rc = read_and_unpack_buffer(buffer, "wwSSSS", &id, &timestamp,
			&dx, (int32_t) UNIVERSE_DIM,
			&dy, (int32_t) UNIVERSE_DIM,
			&dz, (int32_t) UNIVERSE_DIM,
			&r, (int32_t) UNIVERSE_DIM);
	if (rc)
		return rc;
	pthread_mutex_lock(&universe_mutex);
	rc = update_black_hole(id, timestamp, dx, dy, dz, r);
	pthread_mutex_unlock(&universe_mutex);
	return (rc < 0);
}

static int process_update_science_text(void)
{
	unsigned char buffer[5 + 257];
	char text[257];
	uint32_t id;
	uint16_t len;
	int rc, i;

	rc = read_and_unpack_buffer(buffer, "wh", &id, &len);
	if (rc)
		return rc;
	if (len > 256)
		return -1;
	rc = snis_readsocket(gameserver_sock, buffer, len);
	if (rc)
		return rc;
	text[len] = '\0';
	memcpy(text, buffer, len);
	pthread_mutex_lock(&universe_mutex);
	i = lookup_object_by_id(id);
	if (i < 0) { /* maybe we just joined and don't know this object yet */
		pthread_mutex_unlock(&universe_mutex);
		return 0;
	}
	if (go[i].sdata.science_text)
		free(go[i].sdata.science_text);
	go[i].sdata.science_text = strndup(text, 256);
	pthread_mutex_unlock(&universe_mutex);
	return 0;
}

static int process_update_planet_description(void)
{
	unsigned char buffer[5 + 257];
	char text[257];
	uint32_t id;
	uint16_t len;
	int rc, i;

	rc = read_and_unpack_buffer(buffer, "wh", &id, &len);
	if (rc)
		return rc;
	if (len > 256)
		return -1;
	rc = snis_readsocket(gameserver_sock, buffer, len);
	if (rc)
		return rc;
	text[len] = '\0';
	memcpy(text, buffer, len);
	pthread_mutex_lock(&universe_mutex);
	i = lookup_object_by_id(id);
	if (i < 0) { /* maybe we just joined and don't know this object yet */
		pthread_mutex_unlock(&universe_mutex);
		return 0;
	}
	if (go[i].type != OBJTYPE_PLANET) {
		pthread_mutex_unlock(&universe_mutex);
		return 0;
	}
	if (go[i].tsd.planet.custom_description)
		free(go[i].tsd.planet.custom_description);
	go[i].tsd.planet.custom_description = strndup(text, 256);
	pthread_mutex_unlock(&universe_mutex);
	return 0;
}

static int process_update_planet_packet(void)
{
	unsigned char buffer[100];
	uint32_t id, timestamp;
	union quat orientation;
	double dr, dx, dy, dz, atm_scale, atm_brightness;
	uint8_t government, tech_level, economy, security, atm_r, atm_g, atm_b;
	uint8_t has_atmosphere, solarsystem_planet_type, build_unit_type;
	uint8_t ring_selector;
	uint16_t atmosphere_type;
	uint32_t dseed, time_left_to_build;
	int hasring;
	uint16_t contraband;
	int rc;

	assert(sizeof(buffer) > sizeof(struct update_asteroid_packet) - sizeof(uint8_t));
	rc = read_and_unpack_buffer(buffer, "wwSSSQSwbbbbhbbbSSbhbbwb", &id, &timestamp,
			&dx, (int32_t) UNIVERSE_DIM,
			&dy,(int32_t) UNIVERSE_DIM,
			&dz, (int32_t) UNIVERSE_DIM,
			&orientation,
			&dr, (int32_t) UNIVERSE_DIM,
			&dseed, &government, &tech_level, &economy, &security,
			&contraband, &atm_r, &atm_b, &atm_g, &atm_scale, (int32_t) UNIVERSE_DIM,
			&atm_brightness, (int32_t) UNIVERSE_DIM,
			&has_atmosphere, &atmosphere_type, &solarsystem_planet_type, &ring_selector,
			&time_left_to_build, &build_unit_type);
	if (rc != 0)
		return rc;
	solarsystem_planet_type = solarsystem_planet_type % PLANET_TYPE_COUNT_SHALL_BE;
	hasring = (dr < 0);
	if (hasring)
		dr = -dr;
	pthread_mutex_lock(&universe_mutex);
	rc = update_planet(id, timestamp, dx, dy, dz, &orientation, dr, government, tech_level,
				economy, dseed, hasring, security, contraband,
				atm_r, atm_b, atm_g, atm_scale, atm_brightness, has_atmosphere,
				atmosphere_type, solarsystem_planet_type, ring_selector,
				time_left_to_build, build_unit_type);
	pthread_mutex_unlock(&universe_mutex);
	return (rc < 0);
} 

static int process_update_wormhole_packet(void)
{
	unsigned char buffer[100];
	uint32_t id, timestamp;
	double dx, dy, dz;
	int rc;

	assert(sizeof(buffer) > sizeof(struct update_wormhole_packet) - sizeof(uint8_t));
	rc = read_and_unpack_buffer(buffer, "wwSSS", &id, &timestamp,
			&dx, (int32_t) UNIVERSE_DIM,
			&dy, (int32_t) UNIVERSE_DIM,
			&dz, (int32_t) UNIVERSE_DIM);
	if (rc != 0)
		return rc;
	pthread_mutex_lock(&universe_mutex);
	rc = update_wormhole(id, timestamp, dx, dy, dz);
	pthread_mutex_unlock(&universe_mutex);
	return (rc < 0);
} 

static int process_update_starbase_packet(void)
{
	unsigned char buffer[100];
	uint32_t id, timestamp;
	double dx, dy, dz;
	union quat orientation;
	uint32_t time_left_to_build;
	uint8_t occupant[4], build_unit_type, starbase_number;
	int rc;

	rc = read_and_unpack_buffer(buffer, "wwSSSQbbbbwbb", &id, &timestamp,
			&dx, (int32_t) UNIVERSE_DIM,
			&dy, (int32_t) UNIVERSE_DIM,
			&dz, (int32_t) UNIVERSE_DIM,
			&orientation,
			&occupant[0],
			&occupant[1],
			&occupant[2],
			&occupant[3],
			&time_left_to_build,
			&build_unit_type,
			&starbase_number);
	if (rc != 0)
		return rc;
	pthread_mutex_lock(&universe_mutex);
	rc = update_starbase(id, timestamp, dx, dy, dz, &orientation, occupant,
				time_left_to_build, build_unit_type, starbase_number);
	pthread_mutex_unlock(&universe_mutex);
	return (rc < 0);
} 

static int process_update_warpgate_packet(void)
{
	unsigned char buffer[100];
	uint32_t id, timestamp;
	double dx, dy, dz;
	union quat orientation;
	int rc;

	rc = read_and_unpack_buffer(buffer, "wwSSSQ", &id, &timestamp,
			&dx, (int32_t) UNIVERSE_DIM,
			&dy, (int32_t) UNIVERSE_DIM,
			&dz, (int32_t) UNIVERSE_DIM,
			&orientation);
	if (rc != 0)
		return rc;
	pthread_mutex_lock(&universe_mutex);
	rc = update_warpgate(id, timestamp, dx, dy, dz, &orientation);
	pthread_mutex_unlock(&universe_mutex);
	return (rc < 0);
}

static int process_update_nebula_packet(void)
{
	unsigned char buffer[100];
	uint32_t id, timestamp;
	double dx, dy, dz, r;
	union quat av, uo;
	float avx, avy, avz, ava;
	double phase_angle, phase_speed;
	int rc;

	assert(sizeof(buffer) > sizeof(struct update_nebula_packet) - sizeof(uint8_t));
	rc = read_and_unpack_buffer(buffer, "wwSSSSQQSS", &id, &timestamp,
			&dx, (int32_t) UNIVERSE_DIM,
			&dy, (int32_t) UNIVERSE_DIM,
			&dz, (int32_t) UNIVERSE_DIM,
			&r, (int32_t) UNIVERSE_DIM,
			&av, &uo, &phase_angle, (int32_t) 360,
			&phase_speed, (int32_t) 100);
	if (rc != 0)
		return rc;
	quat_to_axis(&av, &avx, &avy, &avz, &ava);
	pthread_mutex_lock(&universe_mutex);
	rc = update_nebula(id, timestamp, dx, dy, dz, r, avx, avy, avz, ava,
				&uo, phase_angle, phase_speed);
	pthread_mutex_unlock(&universe_mutex);
	return (rc < 0);
} 

static int process_update_laserbeam(void)
{
	unsigned char buffer[100];
	uint32_t id, timestamp, origin, target;
	int rc;

	rc = read_and_unpack_buffer(buffer, "wwww", &id, &timestamp, &origin, &target);
	if (rc != 0)
		return rc;
	pthread_mutex_lock(&universe_mutex);
	rc = update_laserbeam(id, timestamp, origin, target);
	pthread_mutex_unlock(&universe_mutex);
	return (rc < 0);
} 

static int process_update_tractorbeam(void)
{
	unsigned char buffer[100];
	uint32_t id, timestamp, origin, target;
	int rc;

	rc = read_and_unpack_buffer(buffer, "wwww", &id, &timestamp, &origin, &target);
	if (rc != 0)
		return rc;
	pthread_mutex_lock(&universe_mutex);
	rc = update_tractorbeam(id, timestamp, origin, target);
	pthread_mutex_unlock(&universe_mutex);
	return (rc < 0);
} 

static int process_update_explosion_packet(void)
{
	unsigned char buffer[sizeof(struct update_explosion_packet)];
	uint32_t id, timestamp, related_id;
	double dx, dy, dz;
	uint16_t nsparks, velocity, time;
	uint8_t victim_type, explosion_type;
	int rc;

	assert(sizeof(buffer) > sizeof(struct update_explosion_packet) - sizeof(uint8_t));
	rc = read_and_unpack_buffer(buffer, "wwwSSShhhbb", &id, &timestamp, &related_id,
		&dx, (int32_t) UNIVERSE_DIM, &dy, (int32_t) UNIVERSE_DIM,
		&dz, (int32_t) UNIVERSE_DIM,
		&nsparks, &velocity, &time, &victim_type, &explosion_type);
	if (rc != 0)
		return rc;
	pthread_mutex_lock(&universe_mutex);
	rc = update_explosion(id, timestamp, related_id, dx, dy, dz, nsparks, velocity, time,
				victim_type, explosion_type);
	pthread_mutex_unlock(&universe_mutex);
	return (rc < 0);
}

static int update_flare(uint32_t id, uint32_t timestamp, double x, double y, double z)
{
	int i;
	struct snis_entity *o;

	i = lookup_object_by_id(id);
	if (i < 0) {
		i = add_generic_object(id, timestamp, x, y, z, 0.0, 0.0, 0.0,
					&identity_quat, OBJTYPE_FLARE, 1, NULL);
		if (i < 0)
			return i;
		o = &go[i];
		o->tsd.flare.time_left = FLARE_LIFETIME;
		o->entity = add_entity(ecx, particle_mesh, x, y, z, PARTICLE_COLOR);
		if (o->entity) {
			update_entity_material(o->entity, &flare_material);
			update_entity_scale(o->entity, 5.0);
		}
	} else {
		o = &go[i];
	}
	update_generic_object(i, timestamp, x, y, z, 0, 0, 0, NULL, 1);

	/* Make the flare fade out when it gets near the end of its life */
	if (o->tsd.flare.time_left > 0)
		o->tsd.flare.time_left--;
	if (o->tsd.flare.time_left < 6 && o->tsd.flare.time_left > 0 && o->entity)
		update_entity_scale(o->entity, 5.0 / (6.0 - o->tsd.flare.time_left));

	return 0;
}

static int process_update_flare_packet(void)
{
	unsigned char buffer[sizeof(struct update_flare_packet)];
	uint32_t id, timestamp;
	double vx, vy, vz;
	int rc;

	rc = read_and_unpack_buffer(buffer, "wwSSS", &id, &timestamp,
		&vx, (int32_t) UNIVERSE_DIM, &vy, (int32_t) UNIVERSE_DIM, &vz, (int32_t) UNIVERSE_DIM);
	if (rc != 0)
		return rc;
	pthread_mutex_lock(&universe_mutex);
	rc = update_flare(id, timestamp, vx, vy, vz);
	pthread_mutex_unlock(&universe_mutex);
	return rc < 0;
}

static struct network_setup_ui {
	struct button *connect_to_lobby;
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
	struct button *role_demon;
	struct button *role_text_to_speech;
	struct button *join_ship_checkbox;
	struct button *create_ship_checkbox;
	struct button *faction_checkbox[MAX_FACTIONS];
	struct button *support_button;
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
	int create_ship_v;
	int join_ship_v;
	int faction_checkbox_v[MAX_FACTIONS];
	char lobbyname[60];
	char lobbyportstr[10];
	char solarsystem[60];
	char shipname[SHIPNAME_LEN];
	char password[PASSWORD_LEN];
	int selected_faction;
} net_setup_ui;

static int process_client_id_packet(void)
{
	unsigned char buffer[100];
	uint32_t id;
	int rc;

	assert(sizeof(buffer) > sizeof(struct client_ship_id_packet) - sizeof(uint8_t));
	rc = read_and_unpack_buffer(buffer, "w", &id);
	if (rc)
		return rc;
	my_ship_id = id;
	my_ship_oid = UNKNOWN_ID;
	printf("SET MY SHIP ID to %u\n", my_ship_id);
	printf("Saving default shipname as '%s'\n", shipname);
	/* shipname is set to net_setup_ui.shipname in connect_to_lobby_button_pressed() */
	snis_prefs_save_default_ship_name(xdg_base_ctx, shipname);

	/* We turn this off for two reason.  1. You only do this once for a ship typically, so
	 * saving it "turned on" as a preference doesn't make much sense. 2. If we create a new
	 * ship, leave it turned on, then transit a warp gate, we will end up requesting ship
	 * creation a second time on the warp gate transit.  This 2nd request will fail and we
	 * will get a bridge verification failure.
	 */
	net_setup_ui.create_ship_v = 0;

	snis_prefs_save_checkbox_defaults(xdg_base_ctx, net_setup_ui.role_main_v, net_setup_ui.role_nav_v,
					net_setup_ui.role_weap_v, net_setup_ui.role_eng_v,
					net_setup_ui.role_damcon_v, net_setup_ui.role_sci_v,
					net_setup_ui.role_comms_v, net_setup_ui.role_sound_v,
					net_setup_ui.role_demon_v, net_setup_ui.role_text_to_speech_v,
					net_setup_ui.create_ship_v, net_setup_ui.join_ship_v);
	return 0;
}

static int process_add_player_error(uint8_t *error)
{
	unsigned char buffer[10];
	uint8_t err;
	int rc;

	*error = 255;
	assert(sizeof(buffer) > sizeof(struct client_ship_id_packet) - sizeof(uint8_t));
	rc = read_and_unpack_buffer(buffer, "b", &err);
	if (rc)
		return rc;
	switch (err) {
	case ADD_PLAYER_ERROR_SHIP_DOES_NOT_EXIST:
		snprintf(login_failed_msg, sizeof(login_failed_msg), "NO SHIP BY THAT NAME EXISTS");
		break;
	case ADD_PLAYER_ERROR_SHIP_ALREADY_EXISTS:
		snprintf(login_failed_msg, sizeof(login_failed_msg), "A SHIP WITH THAT NAME ALREADY_EXISTS");
		break;
	case ADD_PLAYER_ERROR_FAILED_VERIFICATION:
		snprintf(login_failed_msg, sizeof(login_failed_msg), "FAILED BRIDGE VERIFICATION");
		break;
	case ADD_PLAYER_ERROR_TOO_MANY_BRIDGES:
		snprintf(login_failed_msg, sizeof(login_failed_msg), "TOO_MANY_BRIDGES");
		break;
	default:
		snprintf(login_failed_msg, sizeof(login_failed_msg), "UNKNOWN ERROR");
		break;
	}
	login_failed_timer = frame_rate_hz * 5;
	*error = err;
	return 0;
}

static void stop_gameserver_writer_thread(void)
{
	fprintf(stderr, "top of stop_gameserver_writer_thread\n");
	pthread_mutex_lock(&to_server_queue_event_mutex);
	writer_thread_should_die = 1;
	pthread_mutex_unlock(&to_server_queue_event_mutex);
	fprintf(stderr, "stop_gameserver_writer_thread 1\n");
	wakeup_gameserver_writer();
	fprintf(stderr, "stop_gameserver_writer_thread 2\n");
	do {
		int alive;
		printf("snis_client: Waiting for writer thread to leave\n");
		pthread_mutex_lock(&to_server_queue_event_mutex);
		alive = writer_thread_alive;
		if (!alive)
			break;
		pthread_mutex_unlock(&to_server_queue_event_mutex);
		fprintf(stderr, "stop_gameserver_writer_thread 3, alive = %d\n", alive);
		sleep(1);
	} while (1);
	writer_thread_should_die = 0;
	pthread_mutex_unlock(&to_server_queue_event_mutex);
	fprintf(stderr, "stop_gameserver_writer_thread 4\n");
}

static int process_request_oneshot_sound(void)
{
	unsigned char buffer[2 + 257];
	char filename[257];
	char path[PATH_MAX];
	uint16_t len;
	int rc;

	rc = read_and_unpack_buffer(buffer, "h", &len);
	if (rc)
		return rc;
	if (len > 256)
		return -1;
	rc = snis_readsocket(gameserver_sock, buffer, len);
	if (rc)
		return rc;
	filename[len] = '\0';
	memcpy(filename, buffer, len);
	snprintf(path, sizeof(path), "%s/%s", asset_dir, filename);
	/* wwviaudio_add_one_shot_sound does disk i/o.  Perhaps we should have a queue
	 * and another thread that processes the queue and calls wwviaudio_add_one_shot_sound()
	 * passing what's on the queue in order to keep disk i/o out of this thread (which is
	 * processing opcodes from the server). But let's try this dumb way first to see if
	 * it's a real problem.
	 */
	rc = wwviaudio_add_one_shot_sound(path);
	if (rc < 0)
		print_demon_console_color_msg(YELLOW, "FAILED TO PLAY %s", path);
	return 0;
}

static int process_opus_audio_data(void)
{
	int rc;
	uint8_t buffer[4008];
	uint16_t audio_chain;
	uint8_t destination;
	uint32_t radio_channel;
	uint16_t datalen;

	rc = read_and_unpack_buffer(buffer, "bhwh",
			&audio_chain, &destination, &radio_channel, &datalen);
	if (rc)
		return rc;
	if (datalen > 4000) {
		fprintf(stderr, "%s: Unexpected opus audio data length of %u\n",
				"snis_client", datalen);
		return -1;
	}
	rc = snis_readsocket(gameserver_sock, buffer, datalen);
	if (rc)
		return rc;
	if (audio_chain < WWVIAUDIO_CHAIN_COUNT)
		voice_chat_play_opus_packet(buffer, datalen, audio_chain);
	return 0;
}

static int process_talking_stick(void)
{
	uint8_t buffer[10];
	uint8_t subcode;
	int rc;

	rc = read_and_unpack_buffer(buffer, "b", &subcode);
	if (rc)
		return rc;
	switch (subcode) {
	case OPCODE_TALKING_STICK_GRANT:
		pthread_mutex_lock(&voip_mutex);
		have_talking_stick = 1;
		pthread_mutex_unlock(&voip_mutex);
		break;
	default:
		fprintf(stderr, "%s: Unexpected OPCODE_TALKING_STICK subcode %hhu\n",
				"snis_client", subcode);
		return -1;
	}
	return 0;
}

static int process_custom_button(void);
static int process_console_op(void);
static int process_client_config(void);

static void *gameserver_reader(__attribute__((unused)) void *arg)
{
	static uint32_t successful_opcodes;
	uint8_t previous_opcode;
	uint8_t last_opcode = 0x00;
	uint8_t opcode = 0xff;
	uint8_t add_player_error;
	int rc = 0;

	printf("snis_client: gameserver reader thread\n");
	while (1) {
		add_player_error = 0;
		previous_opcode = last_opcode;
		last_opcode = opcode;
		/* printf("Client reading from game server %d bytes...\n", sizeof(opcode)); */
		rc = snis_readsocket(gameserver_sock, &opcode, sizeof(opcode));

		/* grab time as close to when we got the packet as possible */
		double update_time = time_now_double();

		if (rc != 0) {
			fprintf(stderr, "snis_readsocket returns %d, errno  %s\n",
				rc, strerror(errno));
			goto protocol_error;
		}
		/* printf("got opcode %hhu\n", opcode); */
		switch (opcode)	{
		case OPCODE_UPDATE_SHIP:
			rc = process_update_ship_packet(opcode);
			break;
		case OPCODE_UPDATE_POWER_DATA:
			rc = process_update_power_data();
			break;
		case OPCODE_UPDATE_COOLANT_DATA:
			rc = process_update_coolant_data();
			break;
		case OPCODE_ECON_UPDATE_SHIP:
		case OPCODE_ECON_UPDATE_SHIP_DEBUG_AI:
			rc = process_update_econ_ship_packet(opcode);
			break;
		case OPCODE_ID_CLIENT_SHIP:
			rc = process_client_id_packet();
			break;
		case OPCODE_ADD_PLAYER_ERROR:
			fprintf(stderr, "snis_client: OPCODE_ADD_PLAYER_ERROR\n");
			rc = process_add_player_error(&add_player_error);
			break;
		case OPCODE_UPDATE_ASTEROID:
			rc = process_update_asteroid_packet();
			break;
		case OPCODE_UPDATE_ASTEROID_MINERALS:
			rc = process_update_asteroid_minerals_packet();
			break;
		case OPCODE_UPDATE_WARP_CORE:
			rc = process_update_warp_core_packet();
			break;
		case OPCODE_UPDATE_BLOCK:
			rc = process_update_block_packet();
			break;
		case OPCODE_UPDATE_TURRET:
			rc = process_update_turret_packet();
			break;
		case OPCODE_UPDATE_CARGO_CONTAINER:
			rc = process_update_cargo_container_packet();
			break;
		case OPCODE_UPDATE_CARGO_CONTAINER_POSITION:
			rc = process_update_cargo_container_position();
			break;
		case OPCODE_UPDATE_DERELICT:
			rc = process_update_derelict_packet();
			break;
		case OPCODE_UPDATE_BLACK_HOLE:
			rc = process_update_black_hole_packet();
			break;
		case OPCODE_UPDATE_SCI_TEXT:
			rc = process_update_science_text();
			break;
		case OPCODE_UPDATE_PLANET_DESCRIPTION:
			rc = process_update_planet_description();
			break;
		case OPCODE_UPDATE_PLANET:
			rc = process_update_planet_packet();
			break;
		case OPCODE_UPDATE_STARBASE:
			rc = process_update_starbase_packet();
			break;
		case OPCODE_UPDATE_WARPGATE:
			rc = process_update_warpgate_packet();
			break;
		case OPCODE_UPDATE_WORMHOLE:
			rc = process_update_wormhole_packet();
			break;
		case OPCODE_UPDATE_NEBULA:
			rc = process_update_nebula_packet();
			break;
		case OPCODE_UPDATE_LASERBEAM:
			rc = process_update_laserbeam();
			break;
		case OPCODE_UPDATE_TRACTORBEAM:
			rc = process_update_tractorbeam();
			break;
		case OPCODE_UPDATE_EXPLOSION:
			rc = process_update_explosion_packet();
			break;
		case OPCODE_UPDATE_FLARE:
			rc = process_update_flare_packet();
			break;
		case OPCODE_UPDATE_LASER:
			rc = process_update_laser_packet();
			break;
		case OPCODE_UPDATE_TORPEDO:
			rc = process_update_torpedo_packet();
			break;
		case OPCODE_UPDATE_MISSILE:
			rc = process_update_missile_packet();
			break;
		case OPCODE_WARP_LIMBO:
			rc = process_warp_limbo_packet();
			break;
		case OPCODE_INITIATE_WARP:
			rc = process_initiate_warp_packet();
			break;
		case OPCODE_WORMHOLE_LIMBO:
			rc = process_wormhole_limbo_packet();
			break;
		case OPCODE_DELETE_OBJECT:
			rc = process_delete_object_packet();
			break;
		case OPCODE_PLAY_SOUND:
			rc = process_play_sound_packet();
			break;
		case OPCODE_SHIP_SDATA:
			rc = process_ship_sdata_packet();
			break;
		case OPCODE_SHIP_SDATA_WITHOUT_NAME:
			rc = process_ship_sdata_without_name();
			break;
		case OPCODE_ROLE_ONSCREEN:
			rc = process_role_onscreen_packet();
			break;
		case OPCODE_SCI_SELECT_TARGET:
			rc = process_sci_select_target_packet();
			break;
		case OPCODE_SCI_DETAILS:
			rc = process_sci_details();
			break;
		case OPCODE_UPDATE_DAMAGE:
			rc = process_ship_damage_packet(1);
			break;
		case OPCODE_SILENT_UPDATE_DAMAGE:
			rc = process_ship_damage_packet(0);
			break;
		case OPCODE_UPDATE_RESPAWN_TIME:
			rc = process_update_respawn_time();
			break;
		case OPCODE_UPDATE_PLAYER:
			break;
		case OPCODE_ACK_PLAYER:
			break;
		case OPCODE_NOOP:
			break;
		case OPCODE_COMMS_TRANSMISSION:
			rc = process_comm_transmission();
			break;
		case OPCODE_NATURAL_LANGUAGE_REQUEST:
			rc = process_natural_language_request();
			break;
		case OPCODE_UPDATE_NETSTATS:
			rc = process_update_netstats();
			break;
		case OPCODE_DAMCON_OBJ_UPDATE:
			rc = process_update_damcon_object();
			break;
		case OPCODE_DAMCON_SOCKET_UPDATE:
			rc = process_update_damcon_socket();
			break;
		case OPCODE_DAMCON_PART_UPDATE:
			rc = process_update_damcon_part();
			break;
		case OPCODE_MAINSCREEN_VIEW_MODE:
			rc = process_mainscreen_view_mode();
			break;
		case OPCODE_UPDATE_SPACEMONSTER:
			rc = process_update_spacemonster();
			break;
		case OPCODE_REQUEST_REDALERT:
			rc = process_red_alert();
			break;
		case OPCODE_COMMS_MAINSCREEN:
			rc = process_comms_mainscreen();
			break;
		case OPCODE_PROXIMITY_ALERT:
			process_proximity_alert();
			break;
		case OPCODE_ATMOSPHERIC_FRICTION:
			process_atmospheric_friction();
			break;
		case OPCODE_COLLISION_NOTIFICATION:
			process_collision_notification();
			break;
		case OPCODE_LOAD_SKYBOX:
			rc = process_load_skybox();
			break;
		case OPCODE_CYCLE_MAINSCREEN_POINT_OF_VIEW:
			rc = process_cycle_camera_point_of_view(&camera_mode);
			break;
		case OPCODE_CYCLE_NAV_POINT_OF_VIEW:
			rc = process_cycle_camera_point_of_view(&nav_camera_mode);
			break;
		case OPCODE_ADD_WARP_EFFECT:
			rc = process_add_warp_effect();
			break;
		case OPCODE_UPDATE_UNIVERSE_TIMESTAMP:
			rc = process_update_universe_timestamp(update_time);
			break;
		case OPCODE_LATENCY_CHECK:
			rc = process_latency_check();
			break;
		case OPCODE_DETONATE:
			rc = process_detonate();
			if (rc)
				goto protocol_error;
			break;
		case OPCODE_SET_SOLARSYSTEM:
			rc = process_set_solarsystem();
			if (rc)
				goto protocol_error;
			break;
		case OPCODE_SWITCH_SERVER:
			rc = process_switch_server();
			if (rc)
				goto protocol_error;
			printf("snis_client: Switch server opcode received\n");
			quickstartmode = 0;
			stop_gameserver_writer_thread();
			delete_all_objects();
			clear_damcon_pool();
			printf("snis_client: writer thread left\n");
			printf("snis_client: **** lobby_selected_server = %d\n", lobby_selected_server);
			switched_server = 1; /* TODO : something better */
			printf("snis_client: **** switched_server = %d\n", switched_server);
			lobby_selected_server = -1;
			close(gameserver_sock);
			gameserver_sock = -1;
			connected_to_gameserver = 0;
			return NULL;
		case OPCODE_TEXTSCREEN_OP:
			rc = process_textscreen_op();
			if (rc)
				goto protocol_error;
			break;
		case OPCODE_CHECK_OPCODE_FORMAT:
			rc = process_check_opcode_format();
			if (rc)
				goto protocol_error;
			break;
		case OPCODE_UPDATE_SOLARSYSTEM_LOCATION:
			rc = process_update_solarsystem_location();
			if (rc)
				goto protocol_error;
			break;
		case OPCODE_SET_WAYPOINT:
			rc = process_set_waypoint();
			if (rc)
				goto protocol_error;
			break;
		case OPCODE_RTS_FUNC:
			rc = process_rts_func();
			if (rc)
				goto protocol_error;
			break;
		case OPCODE_UPDATE_SHIP_CARGO_INFO:
			rc = process_update_ship_cargo_info();
			if (rc)
				goto protocol_error;
			break;
		case OPCODE_DEMON_RTSMODE:
			rc = process_demon_rtsmode();
			if (rc)
				goto protocol_error;
			break;
		case OPCODE_ADJUST_TTS_VOLUME:
			rc = process_adjust_tts_volume();
			if (rc)
				goto protocol_error;
			break;
		case OPCODE_CUSTOM_BUTTON:
			rc = process_custom_button();
			if (rc)
				goto protocol_error;
			break;
		case OPCODE_CONSOLE_OP:
			rc = process_console_op();
			if (rc)
				goto protocol_error;
			break;
		case OPCODE_CLIENT_CONFIG:
			rc = process_client_config();
			if (rc)
				goto protocol_error;
			break;
		case OPCODE_REQUEST_ONESHOT_SOUND:
			rc = process_request_oneshot_sound();
			if (rc)
				goto protocol_error;
			break;
		case OPCODE_OPUS_AUDIO_DATA:
			rc = process_opus_audio_data();
			if (rc)
				goto protocol_error;
			break;
		case OPCODE_TALKING_STICK:
			rc = process_talking_stick();
			if (rc)
				goto protocol_error;
			break;
		default:
			goto protocol_error;
		}
		if (rc) /* protocol error */
			break;
		if (add_player_error) {
			switch (add_player_error) {
			case ADD_PLAYER_ERROR_SHIP_DOES_NOT_EXIST:
				fprintf(stderr,
					"snis_client: failed to add player: ship does not exist\n");
				break;
			case ADD_PLAYER_ERROR_SHIP_ALREADY_EXISTS:
				fprintf(stderr, "snis_client: failed to add player: ship already exists\n");
				break;
			default:
				fprintf(stderr, "snis_client: failed to add player: unknown error %d\n",
					add_player_error);
				goto protocol_error;
			}
			stop_gameserver_writer_thread();
			lobby_selected_server = -1;
			close(gameserver_sock);
			gameserver_sock = -1;
			connected_to_gameserver = 0;
			displaymode = DISPLAYMODE_NETWORK_SETUP;
			return NULL;
		}
		successful_opcodes++;
	}

protocol_error:
	fprintf(stderr, "snis_client: Protocol error in gameserver reader, opcode = %hhu\n", opcode);
	snis_print_last_buffer("snis_client: ", gameserver_sock);
	fprintf(stderr, "snis_client: last opcode was %hhu, before that %hhu\n", last_opcode, previous_opcode);
	fprintf(stderr, "snis_client: total successful opcodes = %u\n", successful_opcodes);
	we_lost_snis_server();
	return NULL;
}

static void wait_for_serverbound_packets(void)
{
	int rc;

	pthread_mutex_lock(&to_server_queue_event_mutex);
	while (!have_packets_for_server) {
		rc = pthread_cond_wait(&server_write_cond,
			&to_server_queue_event_mutex);
		if (rc != 0)
			printf("gameserver_writer: pthread_cond_wait failed.\n");
		if (have_packets_for_server)
			break;
	}
	pthread_mutex_unlock(&to_server_queue_event_mutex);
}

static void write_queued_packets_to_server(void)
{
	struct packed_buffer *buffer;
	int rc;

	pthread_mutex_lock(&to_server_queue_event_mutex);
	buffer = packed_buffer_queue_combine(&to_server_queue, &to_server_queue_mutex);
	if (buffer) {
		rc = snis_writesocket(gameserver_sock, buffer->buffer, buffer->buffer_size);
		packed_buffer_free(buffer);
		if (rc) {
			printf("Failed to write to gameserver\n");
			goto badserver;
		}
	} else {
		printf("Hmm, gameserver_writer awakened, but nothing to write.\n");
	}
	if (have_packets_for_server)
		have_packets_for_server = 0;
	pthread_mutex_unlock(&to_server_queue_event_mutex);
	return;

badserver:
	/* Hmm, we are pretty hosed here... we have a combined packed buffer
	 * to write, but server is disconnected... no place to put our buffer.
	 * What happens next is probably "nothing good."
	 */
	printf("client bailing on server...\n");
	pthread_mutex_unlock(&to_server_queue_event_mutex);
	we_lost_snis_server();
}

static void wakeup_gameserver_writer(void)
{
	int rc;

        pthread_mutex_lock(&to_server_queue_event_mutex);
	have_packets_for_server = 1;
	rc = pthread_cond_broadcast(&server_write_cond);
	if (rc)
		printf("huh... pthread_cond_broadcast failed.\n");
	pthread_mutex_unlock(&to_server_queue_event_mutex);
}

static void *gameserver_writer(__attribute__((unused)) void *arg)
{
	int tmpval;

	writer_thread_alive = 1;
	while (1) {
		wait_for_serverbound_packets();
		write_queued_packets_to_server();

		/* Check if it's time to switch servers .*/
		pthread_mutex_lock(&to_server_queue_event_mutex);
		tmpval = writer_thread_should_die;
		pthread_mutex_unlock(&to_server_queue_event_mutex);
		if (tmpval) {
			printf("snis_client: gameserver writer exiting due to server switch\n");
			break;
		}
	}
	writer_thread_alive = 0;
	return NULL;
}

static int role_to_displaymode(uint32_t role)
{
	int displaymode, j;

	displaymode = DISPLAYMODE_MAINSCREEN;
	for (j = 0; j < 32; j++) {
		if ((1 << j) & role) {
			switch ((1 << j)) {
			case ROLE_MAIN:
				return DISPLAYMODE_MAINSCREEN;
			case ROLE_NAVIGATION:
				return DISPLAYMODE_NAVIGATION;
			case ROLE_WEAPONS:
				return DISPLAYMODE_WEAPONS;
			case ROLE_ENGINEERING:
				return DISPLAYMODE_ENGINEERING;
			case ROLE_SCIENCE:
				return DISPLAYMODE_SCIENCE;
			case ROLE_COMMS:
				return DISPLAYMODE_COMMS;
			case ROLE_DEMON:
				return DISPLAYMODE_DEMON;
			default:
				break;
			}
		}
	}
	return displaymode;
}

static void request_universe_timestamp(void);

static void send_build_info_to_server(void)
{
	char *buildinfo1 = strdup(BUILD_INFO_STRING1);
	char *buildinfo2 = strdup(BUILD_INFO_STRING2);
	struct packed_buffer *pb;
	int len1, len2;

	if (!buildinfo1 || !buildinfo2) { /* Shut clang scan-build's mouth about null ptrs */
		fprintf(stderr, "Out of memory in send_build_info_to_server\n");
		fflush(stderr);
		abort(); /* If we're out of memory at this early point, there's no hope anyway. */
	}

	len1 = strlen(buildinfo1);
	len2 = strlen(buildinfo2);

	if (len1 > 255)
		len1 = 255;
	if (len2 > 255)
		len2 = 255;
	buildinfo1[len1] = '\0';
	buildinfo2[len2] = '\0';

	pb = packed_buffer_allocate(strlen(buildinfo1) + strlen(buildinfo2) + 20);
	packed_buffer_append(pb, "bbw", OPCODE_UPDATE_BUILD_INFO, 0, len1 + 1);
	packed_buffer_append_raw(pb, buildinfo1, (unsigned short) len1 + 1);
	packed_buffer_append(pb, "bbw", OPCODE_UPDATE_BUILD_INFO, 1, len2 + 1);
	packed_buffer_append_raw(pb, buildinfo2, (unsigned short) len2 + 1);
	queue_to_server(pb);
	free(buildinfo2);
	free(buildinfo1);
}

static void snis_client_cross_check_opcodes(void)
{
	uint8_t i;
	uint16_t len;
	struct packed_buffer *pb;

	for (i = snis_first_opcode(); i != 255; i = snis_next_opcode(i)) {
		const char *format = snis_opcode_format(i);
		/* fprintf(stderr, "snis_client: cross checking opcode %hhu, '%s'\n",
				i, snis_opcode_format(i)); */
		len = strlen(format);
		pb = packed_buffer_allocate(strlen(format) + 10);
		packed_buffer_append(pb, "bbbh", OPCODE_CHECK_OPCODE_FORMAT,
					OPCODE_CHECK_OPCODE_VERIFY, i, len);
		packed_buffer_append_raw(pb, format, len);
		queue_to_server(pb);
	}
	/* fprintf(stderr, "snis_client: submitted last cross check\n"); */
}

static void *connect_to_gameserver_thread(__attribute__((unused)) void *arg)
{
	int rc;
	struct addrinfo *gameserverinfo, *i;
	struct addrinfo hints;
	int host_is_numeric, a, b, c, d;
	int port_is_numeric, p;
#if 0
	void *addr;
	char *ipver; 
#endif
	char portstr[50];
	char hoststr[50];
	unsigned char *x;
	struct add_player_packet app;
	int flag = 1;

	fprintf(stderr, "snis_client: connect to gameserver thread\n");
	if (avoid_lobby) {
		strcpy(hoststr, serverhost);
		snprintf(portstr, sizeof(portstr), "%d", serverport);
	} else {
		fprintf(stderr, "snis_client: connect_to_gameserver_thread, lobby_selected_server = %d\n",
				lobby_selected_server);
		x = (unsigned char *) &lobby_game_server[lobby_selected_server].ipaddr;
		snprintf(portstr, sizeof(portstr), "%d", ntohs(lobby_game_server[lobby_selected_server].port));
		snprintf(hoststr, sizeof(hoststr), "%d.%d.%d.%d", x[0], x[1], x[2], x[3]);
	}
	fprintf(stderr, "snis_client: connecting to %s/%s\n", hoststr, portstr);
	strncpy(connecting_to_server_msg, "CONNECTING TO SERVER...",
		sizeof(connecting_to_server_msg) - 1);

	/* Check if the hoststr is numeric... */
	rc = sscanf(hoststr, "%d.%d.%d.%d", &a, &b, &c, &d);
	host_is_numeric = (rc == 4);
	/* Check if the portstr is numeric */
	rc = sscanf(portstr, "%d", &p);
	port_is_numeric = (rc == 1);

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE |
			(AI_NUMERICSERV * port_is_numeric) | (AI_NUMERICHOST * host_is_numeric);
	rc = getaddrinfo(hoststr, portstr, &hints, &gameserverinfo);
	if (rc) {
		fprintf(stderr, "snis_client: Failed looking up %s:%s: %s\n",
			hoststr, portstr, gai_strerror(rc));
		snprintf(connecting_to_server_msg, sizeof(connecting_to_server_msg),
				"snis_client: Failed looking up %s:%s: %s\n",
				hoststr, portstr, gai_strerror(rc));
		goto error;
	}

	for (i = gameserverinfo; i != NULL; i = i->ai_next) {
		if (i->ai_family == AF_INET) {
#if 0
			struct sockaddr_in *ipv4 = (struct sockaddr_in *)i->ai_addr;
			addr = &(ipv4->sin_addr);
			ipver = "IPv4";
#endif
			break;
		}
	}
	if (i == NULL)
		goto error;

	gameserver_sock = socket(AF_INET, SOCK_STREAM, i->ai_protocol); 
	if (gameserver_sock < 0)
		goto error;

	rc = connect(gameserver_sock, i->ai_addr, i->ai_addrlen);
	if (rc < 0)
		goto error;

	rc = setsockopt(gameserver_sock, IPPROTO_TCP, TCP_NODELAY, (char *) &flag, sizeof(int));
	if (rc) {
		fprintf(stderr, "setsockopt(TCP_NODELAY) failed.\n");
		snprintf(connecting_to_server_msg, sizeof(connecting_to_server_msg),
				"setsockopt(TCP_NODELAY) failed.");
	}

	rc = snis_writesocket(gameserver_sock, SNIS_PROTOCOL_VERSION, strlen(SNIS_PROTOCOL_VERSION));
	if (rc < 0) {
		shutdown(gameserver_sock, SHUT_RDWR);
		close(gameserver_sock);
		gameserver_sock = -1;
		goto error;
	}

	displaymode = DISPLAYMODE_CONNECTED;
	done_with_lobby = 1;
	displaymode = role_to_displaymode(role);

	/* Should probably submit this through the packed buffer queue...
	 * but, this works.
	 */
	memset(&app, 0, sizeof(app));
	app.opcode = OPCODE_UPDATE_PLAYER;
	app.new_ship = net_setup_ui.create_ship_v;
	app.warpgate_number = (uint8_t) switch_warp_gate_number;
	app.requested_faction = (uint8_t) net_setup_ui.selected_faction;
	switch_warp_gate_number = -1;
	app.role = htonl(role);
	strncpy((char *) app.shipname, shipname, 19);
	strncpy((char *) app.password, password, 19);

	printf("Notifying server, opcode update player\n");
	if (snis_writesocket(gameserver_sock, &app, sizeof(app)) < 0) {
		fprintf(stderr, "Initial write to gameserver failed.\n");
		snprintf(connecting_to_server_msg, sizeof(connecting_to_server_msg),
				"Initial write to gameserver failed.");
		shutdown(gameserver_sock, SHUT_RDWR);
		close(gameserver_sock);
		gameserver_sock = -1;
		goto error;
	}
	printf("Wrote update player opcode\n");

	printf("starting gameserver reader thread\n");
	rc = create_thread(&read_from_gameserver_thread, gameserver_reader, NULL, "snisc-reader", 1);
	if (rc) {
		fprintf(stderr, "Failed to create gameserver reader thread: %d '%s', '%s'\n",
			rc, strerror(rc), strerror(errno));
		snprintf(connecting_to_server_msg, sizeof(connecting_to_server_msg),
				"Failed to create gameserver reader thread: %d '%s', '%s'",
			rc, strerror(rc), strerror(errno));
	}
	printf("started gameserver reader thread\n");

	pthread_mutex_init(&to_server_queue_mutex, NULL);
	pthread_mutex_init(&to_server_queue_event_mutex, NULL);
	packed_buffer_queue_init(&to_server_queue);

	printf("starting gameserver writer thread\n");
	rc = create_thread(&write_to_gameserver_thread, gameserver_writer, NULL, "snisc-writer", 1);
	if (rc) {
		snprintf(connecting_to_server_msg, sizeof(connecting_to_server_msg),
				"Failed to create gameserver writer thread: %d '%s', '%s'",
			rc, strerror(rc), strerror(errno));
		fprintf(stderr, "Failed to create gameserver writer thread: %d '%s', '%s'\n",
			rc, strerror(rc), strerror(errno));
	}
	printf("started gameserver writer thread\n");

	snis_client_cross_check_opcodes();
	request_universe_timestamp();
	send_build_info_to_server();
	freeaddrinfo(gameserverinfo);
	return NULL;

error:
	/* FIXME, this isn't right... */
	freeaddrinfo(gameserverinfo);
	we_lost_snis_server();

	return NULL;
}

static int connect_to_gameserver(int selected_server)
{
	int rc;

	rc = create_thread(&gameserver_connect_thread,
		connect_to_gameserver_thread, NULL, "snisc-servcon", 1);
	if (rc) {
		fprintf(stderr, "%s: Failed to create thread to connect to gameserver:%d '%s', '%s'\n",
			"snis_client", rc, strerror(rc), strerror(errno));
		return -1;
	}
	return 0;
}

static void show_connecting_screen(GtkWidget *w)
{
	if (strcmp(connecting_to_server_msg, "") == 0)
		strncpy(connecting_to_server_msg, "CONNECTING TO SERVER...",
			sizeof(connecting_to_server_msg) - 1);
	sng_set_foreground(UI_COLOR(lobby_connecting));
	sng_abs_xy_draw_string(connecting_to_server_msg, SMALL_FONT, txx(100), txy(300) + LINEHEIGHT);
	if (!connected_to_gameserver) {
		connected_to_gameserver = 1;
		(void) connect_to_gameserver(lobby_selected_server);
	}
}

static void show_connected_screen(GtkWidget *w)
{
	sng_set_foreground(UI_COLOR(lobby_connecting));
	sng_abs_xy_draw_string("CONNECTED TO SERVER", SMALL_FONT, txx(100), txy(300) + LINEHEIGHT);
	sng_abs_xy_draw_string("DOWNLOADING GAME DATA", SMALL_FONT, txx(100), txy(300) + LINEHEIGHT * 3);
}

static void show_info_message(GtkWidget *w, char *msg)
{
	sng_set_foreground(UI_COLOR(lobby_connecting));
	sng_abs_xy_draw_string(msg, SMALL_FONT, txx(100), txy(300) + LINEHEIGHT * 3);
}

static char *credits_text[] = {
	"S P A C E   N E R D S   I N   S P A C E",
	"",
	"*   *   *",
	"",
	"HTTPS://SMCAMERON.GITHUB.IO/SPACE-NERDS-IN-SPACE/",
	"HTTPS://WWW.PATREON.COM/user?u=9573826",
	"",
	"CREATED BY",
	"STEPHEN M. CAMERON",
	"AND",
	"JEREMY VAN GRINSVEN",
	"",
	"INSPIRED BY",
	"THOM ROBERTSON AND",
	"ARTEMIS: STARSHIP BRIDGE SIMULATOR",
	"",
	"SPECIAL THANKS TO",
	"",
	"TX/RX LABS IN HOUSTON, TX",
	"WITHOUT WHICH THIS GAME",
	"WOULD NOT EXIST",
	"",
	"HACKRVA IN RICHMOND, VA",
	"",
	"QUBODUP & FREEGAMEDEV.NET",
	"",
	"KWADROKE & BRIDGESIM.NET",
	"",
	"*   *   *",
	"",
	"OTHER CONTRIBUTORS",
	"",
	"ANDY CONRAD (HER001)",
	"ANTHONY J. BENTLEY",
	"CHRISTIAN ROBERTS",
	"JIMMY (DUSTEDDK)",
	"EMMANOUEL KAPERNAROS",
	"HARRISON TOTTY",
	"IOAN LOOSLEY",
	"IVAN SANCHEZ ORTEGA",
	"JUSTIN WARWICK",
	"KYLE ROBBERTZE",
	"LUCKI",
	"MCMic",
	"MICHAEL ALDRIDGE",
	"MICHAEL T DEGUZIS",
	"REMI VERSCHELDE",
	"SCOTT BENESH",
	"STEFAN GUSTAVSON",
	"THOMAS GLAMSCH",
	"TOBIAS SIMON",
	"VIKTOR HAHN",
	"ZACHARY SCHULTZ",
	"",
	"SPECIAL THANKS TO THE FOLLOWING PATRONS",
	"OF SPACE NERDS IN SPACE",
	"",
	"ALEC SLOMAN",
	"ANDREW ROACH",
	"BILL LEMMOND",
	"BILLY",
	"MARTIN HAUKELI",
	"DAN HUNSAKER",
	"EMMANOUIL KAPERNAROS",
	"MCMic",
	"Q'S LAB, A RIDGECREST MAKERSPACE",
	"RAYMOND D MERIDETH",
	"RINZLER",
	"STEPHEN WARD",
	"TALAS",
	"",
	"*   *   *",
	"",
};

static void draw_credits_screen(int lines, char *crawl[])
{
	static float z = 1200;
	int i;

	z = z - 2.0;
	if (z < -4500.0) {
		z = 1200;
		credits_screen_active = 0;
	}

	sng_set_foreground(UI_COLOR(damcon_part));
	for (i = 0; i < lines; i++) {
			sng_center_xz_draw_string(crawl[i],
					SMALL_FONT, SCREEN_WIDTH / 2, i * txy(40) + z);
	}
}

static void textscreen_dismiss_button_pressed(void *button_ptr_ptr)
{
	struct button **button = button_ptr_ptr;
	textscreen_timer = 0;
	if (*button)
		ui_hide_widget(*button);
}

static void textscreen_menu_button_pressed(void *button_ptr_ptr)
{
	struct button **button = button_ptr_ptr;
	int i, selection;
	uint8_t byte_selection;

	if (*button) {
		ui_hide_widget(*button);
		selection = button - &user_menu_button[0];
		byte_selection = (uint8_t) selection;
		for (i = 0; i < NUM_USER_MENU_BUTTONS; i++)
			ui_hide_widget(user_menu_button[i]);
		user_defined_menu_active = 0;
		if (selection >= 0 && selection < NUM_USER_MENU_BUTTONS)
			queue_to_server(snis_opcode_subcode_pkt("bbb", OPCODE_TEXTSCREEN_OP,
					OPCODE_TEXTSCREEN_MENU_CHOICE, byte_selection));
	}
}

static void ui_add_button(struct button *b, int active_displaymode, char *tooltip);
static void show_textscreen(GtkWidget *w)
{
	int i;
	static struct button *dismiss_button = NULL;
	char *saveptr = NULL;
	int menucount = 0;

	if (displaymode != DISPLAYMODE_MAINSCREEN)
		return;

	if (!dismiss_button) {
		dismiss_button = snis_button_init(txx(650), txy(520), -1, -1,
			"DISMISS", RED, NANO_FONT, textscreen_dismiss_button_pressed, &dismiss_button);
		snis_button_set_sound(dismiss_button, UISND1);
		ui_add_button(dismiss_button, DISPLAYMODE_INTROSCREEN, ""); /* so it doesn't show up anywhere */
		for (i = 0; i < NUM_USER_MENU_BUTTONS; i++) {
			user_menu_button[i] = snis_button_init(txx(60), txy(135) + i * txy(20), txx(650), -1,
				"M", WHITE, NANO_FONT, textscreen_menu_button_pressed, &user_menu_button[i]);
			ui_add_button(user_menu_button[i], DISPLAYMODE_INTROSCREEN, "");
			snis_button_set_sound(user_menu_button[i], UISND2);
		}
	}
	if (user_defined_menu_active)
		ui_hide_widget(dismiss_button);
	else
		ui_unhide_widget(dismiss_button);
	/* make it show on the current screen, whatever it is */
	ui_set_widget_displaymode(dismiss_button, displaymode);

	if (user_defined_menu_active) {
		menucount = strchrcount(textscreen, '\n') - 1;
		if (menucount > NUM_USER_MENU_BUTTONS)
			menucount = NUM_USER_MENU_BUTTONS;
		for (i = 0; i < NUM_USER_MENU_BUTTONS; i++) {
			if (i < menucount)
				ui_unhide_widget(user_menu_button[i]);
			else
				ui_hide_widget(user_menu_button[i]);
			ui_set_widget_displaymode(user_menu_button[i], displaymode);
		}
	}

	char tmp_textscreen[sizeof(textscreen)];

	switch (displaymode) {
	case DISPLAYMODE_MAINSCREEN:
	case DISPLAYMODE_NAVIGATION:
	case DISPLAYMODE_WEAPONS:
	case DISPLAYMODE_ENGINEERING:
	case DISPLAYMODE_SCIENCE:
	case DISPLAYMODE_COMMS:
	case DISPLAYMODE_DEMON:
	case DISPLAYMODE_DAMCON:
		break;
	default:
		return;
	}
	if (textscreen_timer <= 0 && !user_defined_menu_active)
		return;
	strcpy(tmp_textscreen, textscreen);
	if (textscreen_timer > 0)
		textscreen_timer--;
	if (textscreen_timer == 0 && !user_defined_menu_active)
		ui_hide_widget(dismiss_button);
	char *line;
	int y = 100;

	sng_set_foreground(BLACK);
	snis_draw_rectangle(1, txx(50), txy(50),
			SCREEN_WIDTH - txx(100), SCREEN_HEIGHT - txy(100));
	sng_set_foreground(RED);
	snis_draw_rectangle(FALSE, txx(50), txy(50),
			SCREEN_WIDTH - txx(100), SCREEN_HEIGHT - txy(100));
	sng_set_foreground(WHITE);

	line = strtok_r(tmp_textscreen, "\n", &saveptr);
	if (!line) {
		textscreen_timer = 0;
		return;
	}
	sng_center_xy_draw_string(line, BIG_FONT, SCREEN_WIDTH / 2, txy(y)); y += txy(35);

	i = 0;
	while ((line = strtok_r(NULL, "\n", &saveptr))) {
		if (!user_defined_menu_active) {
			sng_abs_xy_draw_string(line, SMALL_FONT, txx(60), txy(y));
			y += txy(20);
		} else {
			if (i < menucount) {
				ui_unhide_widget(user_menu_button[i]);
				snis_button_set_label(user_menu_button[i], line);
			}
			i++;
		}
	}
}

static void show_watermark(void)
{
	sng_set_foreground(YELLOW);
	sng_abs_xy_draw_string(BUILD_INFO_STRING3, NANO_FONT, txx(25), txy(590));
}

static int blue_rectangle = 1; /* tweakable via console */
static int station_label = 1; /* tweakable via console */

static void show_common_screen(GtkWidget *w, char *title)
{
	int title_color;
	int border_color;
	struct snis_entity *o;
	int rl, pbl;

	o = find_my_ship();

	if (red_alert_mode) {
		title_color = UI_COLOR(common_red_alert);
		border_color = UI_COLOR(common_red_alert);
	} else {
		title_color = UI_COLOR(common_text);
		border_color = UI_COLOR(common_border);
	}

	/* Low oxygen warning is common on all screens */
	if (o && o->tsd.ship.oxygen < UINT32_MAX * 0.1) { /* 10% */
		if (timer & 0x04) {
			sng_set_foreground(UI_COLOR(common_red_alert));
			sng_abs_xy_draw_string("LOW OXYGEN WARNING", SMALL_FONT, txx(25), txy(LINEHEIGHT));
		}
	} else {
		sng_set_foreground(title_color);
		if (station_label)
			sng_abs_xy_draw_string(title, SMALL_FONT, txx(25), txy(LINEHEIGHT));
	}

	if (blue_rectangle) {
		sng_set_foreground(border_color);
		snis_draw_line(1, 1, SCREEN_WIDTH, 0);
		snis_draw_line(SCREEN_WIDTH - 1, 1, SCREEN_WIDTH - 1, SCREEN_HEIGHT - 1);
		snis_draw_line(SCREEN_WIDTH - 1, SCREEN_HEIGHT - 1, 1, SCREEN_HEIGHT - 1);
		snis_draw_line(1, 1, 1, SCREEN_HEIGHT - 1);
	}

	if (external_camera_active_timer > 0 && external_camera_active) {
		char msg[100];
		sng_set_foreground(UI_COLOR(special_options));
		external_camera_active_timer--;
		snprintf(msg, sizeof(msg), "EXTERNAL CAMERA MODE %d (%s)", external_camera_mode,
				external_camera_mode_name[external_camera_mode]);
		sng_center_xy_draw_string(msg, SMALL_FONT, SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2);
	} else {
		if (external_camera_active_timer == 0 && !external_camera_active)
			external_camera_active_timer = frame_rate_hz;
	}
	if (vertical_controls_timer) {
		sng_set_foreground(UI_COLOR(special_options));
		vertical_controls_timer--;
		if (vertical_controls_inverted > 0)
			sng_center_xy_draw_string("VERTICAL CONTROLS NORMAL",
					SMALL_FONT, SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2);
		else
			sng_center_xy_draw_string("VERTICAL CONTROLS INVERTED",
					SMALL_FONT, SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2);
	}
	if (mouse_mode_timer) {
		sng_set_foreground(UI_COLOR(special_options));
		mouse_mode_timer--;
		if (current_mouse_ui_mode == MOUSE_MODE_FREE_MOUSE)
			sng_center_xy_draw_string("MOUSE MODE = NORMAL",
					SMALL_FONT, SCREEN_WIDTH / 2, SCREEN_HEIGHT / 3);
		else
			sng_center_xy_draw_string("MOUSE MODE = CAPTURED",
					SMALL_FONT, SCREEN_WIDTH / 2, SCREEN_HEIGHT / 3);
	}
	if (fake_stars_timer) {
		sng_set_foreground(UI_COLOR(special_options));
		fake_stars_timer--;
		if (nfake_stars > 0)
			sng_center_xy_draw_string("SPACE DUST ENABLED",
					SMALL_FONT, SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2);
		else
			sng_center_xy_draw_string("SPACE DUST DISABLED",
					SMALL_FONT, SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2);
	}
	if (credits_screen_active)
		draw_credits_screen(ARRAYSIZE(credits_text), credits_text);
	if (login_failed_timer) {
		sng_set_foreground(UI_COLOR(special_options));
		login_failed_timer--;
		sng_center_xy_draw_string(login_failed_msg, SMALL_FONT,
						SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2);
		if (login_failed_timer <= 0) {
			/* We failed verfication, so quickstart didn't work, so turn it off. */
			quickstartmode = 0;
			displaymode = DISPLAYMODE_LOBBYSCREEN;
			done_with_lobby = 0;
		}
	}
	if (textscreen_timer > 0 || user_defined_menu_active)
		show_textscreen(w);
	if (watermark_active)
		show_watermark();

	if (monitor_voice_chat) {
		int i;
		rl = voice_chat_recording_level();
		pbl = voice_chat_playback_level();

		for (i = 0; i < 32; i++)
			if ((rl >> i) == 0)
				break;
		rl = i * SCREEN_WIDTH / (2 * 32);
		for (i = 0; i < 32; i++)
			if ((pbl >> i) == 0)
				break;
		pbl = i * SCREEN_WIDTH / (2 * 32);

		sng_set_foreground(RED);
		snis_draw_rectangle(0, 0, 10, SCREEN_WIDTH / 2, 14);
		snis_draw_rectangle(1, 0, 10, rl, 14);
		sng_set_foreground(GREEN);
		snis_draw_rectangle(0, 0, 27, SCREEN_WIDTH / 2, 14);
		snis_draw_rectangle(1, 0, 27, pbl, 14);
	}
}

#define ANGLE_OF_VIEW (45)

static int __attribute__((unused)) normalize_degrees(int degrees)
{
	while (degrees < 0)
		degrees += 360;
	while (degrees > 359)
		degrees -= 360;
	return degrees;
}

static int newzoom(int current_zoom, int desired_zoom)
{
	if (current_zoom != desired_zoom) {
		int delta;

		delta = abs(desired_zoom - current_zoom) / (10 * (use_60_fps + 1));
		if (delta <= 0)
			delta = 1;
		if (desired_zoom < current_zoom)
			current_zoom -= delta;
		else
			current_zoom += delta;
	} 
	return current_zoom;
}

#define NEAR_CAMERA_PLANE 1.6666
#ifndef WITHOUTOPENGL
/* far plane is the whole universe */
#define FAR_CAMERA_PLANE (XKNOWN_DIM)
#else
/* far plane is one sectors, which is about max that can be seen on full zoom */
#define FAR_CAMERA_PLANE (XKNOWN_DIM/10.0)
#endif

static void show_gunsight(GtkWidget *w)
{
	int x1, y1, x2, y2, cx, cy;

	cx = SCREEN_WIDTH / 2;
	cy = SCREEN_HEIGHT / 2;
	x1 = cx - 50;
	x2 = cx + 50;
	y1 = cy - 50;
	y2 = cy + 50;

	sng_set_foreground(UI_COLOR(weap_gunsight));
	snis_draw_line(x1, cy, x1 + 25, cy);
	snis_draw_line(x2 - 25, cy, x2, cy);
	snis_draw_line(cx, y1, cx, y1 + 25);
	snis_draw_line(cx, y2 - 25, cx, y2);
}

static void main_screen_add_text(char *msg)
{
	main_screen_text.last = (main_screen_text.last + 1) % MAINSCREEN_COMMS_LINES;
	strncpy(main_screen_text.text[main_screen_text.last], msg, 99);
}

static void draw_main_screen_text(GtkWidget *w, GdkGC *gc)
{
	int first, i;
	const int comms_lines = MAINSCREEN_COMMS_LINES;

	if (!main_screen_text.comms_on_mainscreen)
		return;

	first = (main_screen_text.last + 1) % comms_lines;

	sng_set_foreground(UI_COLOR(main_text));
	for (i = 0; i < comms_lines; i++) {
		sng_abs_xy_draw_string(main_screen_text.text[first],
				NANO_FONT, 10, SCREEN_HEIGHT - (comms_lines - i) * 18 - 10);
		first = (first + 1) % comms_lines;
	}
}

static void draw_targeting_indicator(GtkWidget *w, GdkGC *gc, int x, int y, int color,
					int ring, float scale, float ratio)
{
	int i;

	sng_set_foreground(color);
	for (i = 0; i < 4; i++) {
		int x1, y1, x2, y2;
		double angle;
		double dx, dy, ddx, ddy;
		int offset = ring ? 0 : 45;

		angle = (M_PI * ((i * 90 + offset + timer * 4) % 360)) / 180.0;

		dx = scale * 15.0 * cos(angle);
		dy = scale * 15.0 * sin(angle);
		ddx = scale * 5.0 * cos(angle + M_PI / 2.0);
		ddy = scale * 5.0 * sin(angle + M_PI / 2.0);

		x1 = x + dx;
		y1 = y + dy;
		x2 = x + ratio * dx;
		y2 = y + ratio * dy;
		/* snis_draw_line(x1, y1, x2, y2); */
		snis_draw_line(x2 - ddx, y2 - ddy, x2 + ddx, y2 + ddy);
		snis_draw_line(x2 - ddx, y2 - ddy, x1, y1);
		snis_draw_line(x1, y1, x2 + ddx, y2 + ddy);
		if (ring)
			sng_draw_circle(0, x, y, 33);
	}
}

static void add_thrust_entities(struct entity *thrust_entity[],
		int *nthrust_ports, struct entity_context *cx, struct entity *e,
		struct thrust_attachment_point *ap, int impulse, const int thrust_material_index)
{
	int i, p;

	if (!ap)
		return;

	assert((thrust_entity && nthrust_ports) || (!thrust_entity && !nthrust_ports));

	/* 180 is about max current with preset and 5x is about right for max size */
	float thrust_size = clampf(impulse / 36.0, 0.1, 5.0);

	if (nthrust_ports)
		*nthrust_ports = 0;
	p = 0;
	for (i = 0; i < ap->nports * 2; i += 2) {
		struct entity *t = add_entity(cx, thrust_animation_mesh,
			ap->port[p].pos.v.x, ap->port[p].pos.v.y, ap->port[p].pos.v.z, WHITE);
		if (thrust_entity) {
			thrust_entity[i] = t;
			(*nthrust_ports)++;
		}
		if (t) {
			update_entity_material(t, &thrust_material[thrust_material_index]);
			update_entity_orientation(t, &identity_quat);
			union vec3 thrust_scale = { { thrust_size, 1.0, 1.0 } };
			vec3_mul_self(&thrust_scale, ap->port[p].scale);
			update_entity_non_uniform_scale(t, thrust_scale.v.x, thrust_scale.v.y, thrust_scale.v.z);
			update_entity_parent(cx, t, e);
		}
		t = add_entity(cx, torpedo_mesh,
			ap->port[p].pos.v.x, ap->port[p].pos.v.y, ap->port[p].pos.v.z, WHITE);
		if (thrust_entity) {
			thrust_entity[i + 1] = t;
			(*nthrust_ports)++;
		}
		if (t) {
			update_entity_material(t, &thrust_flare_material[thrust_material_index]);
			update_entity_orientation(t, &identity_quat);
			update_entity_scale(t, THRUST_FLARE_SCALE * thrust_size * ap->port[p].scale);
			update_entity_parent(cx, t, e);
		}
		p++;
	}
}

/* size is 0.0 to 1.0 */
static void add_ship_thrust_entities(struct entity *thrust_entity[],
		int *nthrust_ports, struct entity_context *cx, struct entity *e,
		int shiptype, int impulse, const int thrust_material_index)
{
	struct thrust_attachment_point *ap = ship_thrust_attachment_point(shiptype);

	if (!ap)
		return;
	add_thrust_entities(thrust_entity, nthrust_ports, cx, e, ap, impulse, thrust_material_index);
}

static void update_warp_tunnel(struct snis_entity *o, struct entity **warp_tunnel)
{
	static int first_time = 1;
	static union vec3 lastp;
	static double lasttime = 0;
	double thistime;
	union vec3 p = { { o->x, o->y, o->z } };
	union vec3 v;
	float m;
	static float max = 0;
	const float max_alpha = 0.85;

	if (first_time) {
		lastp = p;
		first_time = 0;
		lasttime = universe_timestamp() - 1.0;
		return;
	}

	vec3_sub(&v, &p, &lastp);
	m = vec3_magnitude(&v);
	if (m > max)
		max = m;
	lastp = p;

	/* Suppress spurious warp tunnel on switching to main view.
	 * Keep track of the last time we went through here. If we switch away
	 * from the main screen or the weapons screen for awhile and a lot of
	 * time goes by, and we travel some distance, we do not want to interpret
	 * that distance traveled as velocity without accounting for the time it
	 * took.
	 */
	thistime = universe_timestamp();
	if (thistime - lasttime > 0.5) {
		if (*warp_tunnel) {
			remove_entity(ecx, *warp_tunnel);
			*warp_tunnel = NULL;
		}
		lasttime = thistime;
		return;
	}
	lasttime = thistime;

	if (m <= MAX_PLAYER_VELOCITY) {
		if (*warp_tunnel) {
			remove_entity(ecx, *warp_tunnel);
			*warp_tunnel = NULL;
		}
		return;
	}

	if (m > 2500)
		m = 2500;
	float new_alpha = max_alpha * ((m - MAX_PLAYER_VELOCITY * 1.5) /
					(2500 - MAX_PLAYER_VELOCITY * 1.5));
	if (*warp_tunnel == NULL) {
		union vec3 down_x_axis = { { 1, 0, 0 } };
		union vec3 up = { {0, 1, 0} };
		union quat orientation;
		vec3_normalize_self(&v);
		quat_from_u2v(&orientation, &down_x_axis, &warp_tunnel_direction, &up);
		*warp_tunnel = add_entity(ecx, warp_tunnel_mesh, o->x, o->y, o->z, SHIP_COLOR);
		if (*warp_tunnel) {
			update_entity_material(*warp_tunnel, &warp_tunnel_material);
			update_entity_orientation(*warp_tunnel, &orientation);
		} else {
			return;
		}
	}
	if (*warp_tunnel) {
		entity_update_alpha(*warp_tunnel, new_alpha);
		warp_tunnel_material.texture_mapped_unlit.alpha = new_alpha;
	}
}

static void show_lens_flare(struct snis_entity *o, union vec3 *camera_pos, union quat *camera_orientation)
{
#ifndef WITHOUTOPENGL
	union vec3 halopos, to_sun;
	union vec3 ghost_nexus, ghost_pos, ghost_offset;
	union vec3 cam_dir = { { 1.0, 0.0, 0.0 } };
	union vec3 to_camera;
	float dist, factor;
	int i;
	float facing_sun;

	if (!lens_flare_enabled)
		return;

	if (o->shading_planet)
		return;

	if (!sun_entity)
		return;

	if (!entity_onscreen(sun_entity))
		return;

	halopos.v.x = SUNX - camera_pos->v.x;
	halopos.v.y = SUNY - camera_pos->v.y;
	halopos.v.z = SUNZ - camera_pos->v.z;
	dist = vec3_magnitude(&halopos);
	vec3_normalize_self(&halopos);
	to_sun = halopos;
	vec3_mul_self(&halopos, 3.0);
	vec3_add_self(&halopos, camera_pos);

	if (!lens_flare_halo)
		lens_flare_halo = add_entity(ecx, unit_quad, halopos.v.x, halopos.v.y, halopos.v.z, WHITE);
	else
		update_entity_pos(lens_flare_halo, halopos.v.x, halopos.v.y, halopos.v.z);
	if (lens_flare_halo) {
		update_entity_scale(lens_flare_halo, 1.0);
		lens_flare_halo_material.texture_mapped_unlit.alpha = 0.40 * lens_flare_intensity;
		update_entity_material(lens_flare_halo, &lens_flare_halo_material);
	}

	if (!anamorphic_flare)
		anamorphic_flare = add_entity(ecx, unit_quad, halopos.v.x, halopos.v.y, halopos.v.z, WHITE);
	else
		update_entity_pos(anamorphic_flare, halopos.v.x, halopos.v.y, halopos.v.z);
	if (anamorphic_flare) {
		update_entity_non_uniform_scale(anamorphic_flare, 3.0, 0.05, 1.0);
		anamorphic_flare_material.texture_mapped_unlit.alpha = 0.40 * lens_flare_intensity;
		update_entity_material(anamorphic_flare, &anamorphic_flare_material);
	}

	quat_rot_vec_self(&cam_dir, camera_orientation);
	facing_sun = 1.0 - fabsf(vec3_dot(&cam_dir, &to_sun));
	lens_flare_ghost_material.texture_mapped_unlit.alpha = 2.0 * lens_flare_intensity *
				fmap(facing_sun, 0.0, 1.0, 0.05, 0.7);

	vec3_mul_self(&cam_dir, 0.5 * dist);
	vec3_add(&ghost_nexus, camera_pos, &cam_dir);

	factor = -1.0;
	for (i = 0; i < NLENS_FLARE_GHOSTS; i++) {
		if (fabsf(factor) < 0.01) /* Skip ghost at center of screen */
			factor += (2.0 / NLENS_FLARE_GHOSTS);
		ghost_pos = ghost_nexus;
		ghost_offset.v.x = SUNX - ghost_nexus.v.x;
		ghost_offset.v.y = SUNY - ghost_nexus.v.y;
		ghost_offset.v.z = SUNZ - ghost_nexus.v.z;
		vec3_mul_self(&ghost_offset, factor * lens_flare_ghost_displacement[i]);
		factor += (2.0 / NLENS_FLARE_GHOSTS);
		vec3_add_self(&ghost_pos, &ghost_offset);
		vec3_sub(&to_camera, camera_pos, &ghost_pos);
		vec3_normalize_self(&to_camera);
		vec3_mul_self(&to_camera, -10.0);
		vec3_add(&ghost_pos, camera_pos, &to_camera);

		lens_flare_ghost[i] = add_entity(ecx, unit_quad, ghost_pos.v.x, ghost_pos.v.y, ghost_pos.v.z, WHITE);
		if (lens_flare_ghost[i]) {
			update_entity_scale(lens_flare_ghost[i], lens_flare_ghost_scale[i]);
			update_entity_material(lens_flare_ghost[i], &lens_flare_ghost_material);
		}
	}
#endif
}

static void remove_lens_flare_entities(void)
{
#ifndef WITHOUTOPENGL
	int i;

	if (lens_flare_halo) {
		remove_entity(ecx, lens_flare_halo);
		lens_flare_halo = NULL;
	}
	if (anamorphic_flare) {
		remove_entity(ecx, anamorphic_flare);
		anamorphic_flare = NULL;
	}

	for (i = 0; i < NLENS_FLARE_GHOSTS; i++)
		if (lens_flare_ghost[i]) {
			remove_entity(ecx, lens_flare_ghost[i]);
			lens_flare_ghost[i] = NULL;
		}
#endif
}

static void adjust_weapons_azimuth_angle(union quat *cam_orientation)
{
	union quat az;
	if (weapons_azimuth_angle == 0.0) {
		ui_unhide_widget(weapons.wavelen_slider);
		ui_unhide_widget(weapons.phaser_wavelength);
		ui_unhide_widget(weapons.phaser_bank_gauge);
		return;
	}
	ui_hide_widget(weapons.wavelen_slider);
	ui_hide_widget(weapons.phaser_wavelength);
	ui_hide_widget(weapons.phaser_bank_gauge);

	quat_init_axis(&az, 0.0, 1.0, 0.0, weapons_azimuth_angle * M_PI / 180.0);
	quat_mul_self(cam_orientation, &az);
}

static void show_weapons_camera_view(GtkWidget *w)
{
	const float min_angle_of_view = 5.0 * M_PI / 180.0;
	const float max_angle_of_view = ANGLE_OF_VIEW * M_PI / 180.0;
	static int current_zoom = 0;
	float angle_of_view;
	struct snis_entity *o;
	float cx, cy, cz;
	union quat camera_orientation, adjusted_cam_orientation;
	union vec3 recoil = { { -1.0f, 0.0f, 0.0f } };
	char buf[20];
	int i;
	static float current_lights = 1.0;

	static int last_timer = 0;
	int first_frame = (timer != last_timer+1);
	last_timer = timer;

	if (first_frame) {
		turret_recoil_amount = 0;
		weapons_camera_shake = 0;
	}

	if (!(o = find_my_ship()))
		return;

	snis_slider_set_input(weapons.wavelen_slider, o->tsd.ship.phaser_wavelength / 255.0);
	update_warp_tunnel(o, &warp_tunnel);

	/* current_zoom = newzoom(current_zoom, o->tsd.ship.mainzoom); */
	current_zoom = 0;
	angle_of_view = ((255.0 - (float) current_zoom) / 255.0) *
				(max_angle_of_view - min_angle_of_view) + min_angle_of_view;

	cx = o->x;
	cy = o->y;
	cz = o->z;

	quat_mul(&camera_orientation, &o->orientation, &o->tsd.ship.weap_orientation);
	adjusted_cam_orientation = camera_orientation;
	adjust_weapons_azimuth_angle(&adjusted_cam_orientation);

	union vec3 turret_pos = {
		{ -4 * SHIP_MESH_SCALE, 5.45 * SHIP_MESH_SCALE, 0 * SHIP_MESH_SCALE },
	};
	quat_rot_vec_self(&turret_pos, &o->orientation);
	vec3_add_c_self(&turret_pos, cx, cy, cz);

	union vec3 view_offset = { {0, 0.75 * SHIP_MESH_SCALE, 0} };
	quat_rot_vec_self(&view_offset, &adjusted_cam_orientation);

	union vec3 cam_pos = turret_pos;
	vec3_add_self(&cam_pos, &view_offset);

	if (weapons_camera_shake > 0.05) {
		float ryaw, rpitch;

		ryaw = weapons_camera_shake * ((snis_randn(100) - 50) * 0.025f) * M_PI / 180.0;
		rpitch = weapons_camera_shake * ((snis_randn(100) - 50) * 0.025f) * M_PI / 180.0;
		quat_apply_relative_yaw_pitch(&adjusted_cam_orientation, ryaw, rpitch);
		weapons_camera_shake = 0.7f * weapons_camera_shake;
	}

	camera_set_pos(ecx, cam_pos.v.x, cam_pos.v.y, cam_pos.v.z);
	camera_set_orientation(ecx, &adjusted_cam_orientation);
	camera_set_parameters(ecx, NEAR_CAMERA_PLANE * SHIP_MESH_SCALE, FAR_CAMERA_PLANE,
				SCREEN_WIDTH, SCREEN_HEIGHT, angle_of_view);
	set_window_offset(ecx, 0, 0);
	set_lighting(ecx, SUNX, SUNY, SUNZ);
	set_ambient_light(ecx, ambient_light);
	calculate_camera_transform(ecx);
	entity_context_set_hi_lo_poly_pixel_threshold(ecx, low_poly_threshold);

	sng_set_foreground(GREEN);
	if (!ecx_fake_stars_initialized) {
		ecx_fake_stars_initialized = 1;
		entity_init_fake_stars(ecx, nfake_stars, 300.0f * 10.0f);
	}

	render_skybox(ecx);

	pthread_mutex_lock(&universe_mutex);


	/* Add our ship into the scene (on the mainscreen, it is omitted) */
	o->entity = add_entity(ecx, ship_mesh_map[o->tsd.ship.shiptype],
				o->x, o->y, o->z, SHIP_COLOR);
	if (o->entity) {
		float desired_lights = (float) o->tsd.ship.exterior_lights / 255.0;
		current_lights += (desired_lights - current_lights) * 0.05;
		update_entity_orientation(o->entity, &o->orientation);
		entity_update_emit_intensity(o->entity, current_lights);
		set_render_style(o->entity, RENDER_NORMAL);
		entity_set_in_shade(o->entity, 0.9 * (o->shading_planet != NULL) + 0.1);
	}

	/* Add our turret into the mix */
	quat_rot_vec_self(&recoil, &camera_orientation);
	struct entity* turret_entity = add_entity(ecx, ship_turret_mesh,
			turret_pos.v.x + turret_recoil_amount * recoil.v.x * SHIP_MESH_SCALE,
			turret_pos.v.y + turret_recoil_amount * recoil.v.y * SHIP_MESH_SCALE,
			turret_pos.v.z + turret_recoil_amount * recoil.v.z * SHIP_MESH_SCALE,
			SHIP_COLOR);
	turret_recoil_amount = turret_recoil_amount * 0.5f;
	if (turret_entity) {
		update_entity_orientation(turret_entity, &camera_orientation);
		set_render_style(turret_entity, RENDER_NORMAL);
		entity_set_in_shade(turret_entity, 0.9 * (o->shading_planet != NULL) + 0.1);
	}

	if (o->entity && !o->tsd.ship.reverse)
		add_ship_thrust_entities(NULL, NULL, ecx, o->entity,
				o->tsd.ship.shiptype, o->tsd.ship.power_data.impulse.i, 0);

	show_lens_flare(o, &cam_pos, &adjusted_cam_orientation); /* this will be using data from last frame */
	render_entities(ecx);
	remove_lens_flare_entities();

	/* Show targeting aids */
	if (weapons_azimuth_angle == 0) {
		for (i = 0; i <= snis_object_pool_highest_object(pool); i++) {
			struct snis_entity *target = &go[i];
			float sx, sy, dist, ex, ey, ez;
			if (!target->alive)
				continue;
			if (target->type != OBJTYPE_SHIP2 && target->type != OBJTYPE_SHIP1)
				continue;
			if (!target->entity || !entity_onscreen(target->entity))
				continue;
			if (!entity_onscreen(target->entity))
				continue;
			entity_get_pos(target->entity, &ex, &ey, &ez);
			dist = dist3d(o->x - ex, o->y - ey, o->z - ez);
			entity_get_screen_coords(target->entity, &sx, &sy);
			/* BUG: TORPEDO_VELOCITY and TORPEDO_LIFETIME are server-side tweakable
			 * on the demon console, but if tweaked, the client will have no knowledge
			 * of this tweaking, so this next condition will be incorrect.
			 */
			if (dist < TORPEDO_VELOCITY * TORPEDO_LIFETIME) {
				char msg[20];
				draw_targeting_indicator(w, gc, sx, sy, UI_COLOR(weap_in_range), 0, 0.5, 1.5f);
				snprintf(msg, sizeof(msg), "%.0f", dist);
				sng_set_foreground(UI_COLOR(weap_in_range));
				sng_abs_xy_draw_string(msg, PICO_FONT, sx + txx(10), sy);
			} else {
				draw_targeting_indicator(w, gc, sx, sy, UI_COLOR(weap_targeting), 0, 0.5, 1.5f);
			}
		}
	}


	/* Remove our ship from the scene */
	if (turret_entity)
		remove_entity(ecx, turret_entity);
	if (o->entity)
		remove_entity(ecx, o->entity);
	o->entity = NULL;

	/* range is the same as max zoom on old weapons */
	if (weapons_azimuth_angle == 0.0) {
		draw_plane_radar(w, o, &camera_orientation, 0.5 * SCREEN_WIDTH, 0.8333 * SCREEN_HEIGHT,
					0.125 * SCREEN_HEIGHT, XKNOWN_DIM * 0.02);

		/* Draw science selector indicator on main screen */
		if (curr_science_guy) {
			float sx, sy;

			if (curr_science_guy->alive && curr_science_guy->entity &&
				entity_onscreen(curr_science_guy->entity)) {
				entity_get_screen_coords(curr_science_guy->entity, &sx, &sy);
				draw_targeting_indicator(w, gc, sx, sy, UI_COLOR(weap_sci_selection), 0, 1.0f, 2.0f);
			}
		}

		/* show torpedo count */
		if (!o->tsd.ship.torpedoes_loading || (timer & 0x4)) {
			if (o->tsd.ship.torpedoes_loading)
				sng_set_foreground(UI_COLOR(weap_torpedoes_loading));
			else
				sng_set_foreground(UI_COLOR(weap_torpedoes_loaded));
			snprintf(buf, sizeof(buf), "TORP: %03d", o->tsd.ship.torpedoes +
						o->tsd.ship.torpedoes_loading +
						o->tsd.ship.torpedoes_loaded);
			sng_abs_xy_draw_string(buf, NANO_FONT, 570 * SCREEN_WIDTH / 800,
						SCREEN_HEIGHT - 15 * SCREEN_HEIGHT / 600);
			snprintf(buf, sizeof(buf), "MISSILES: %03d", o->tsd.ship.missile_count);
			sng_abs_xy_draw_string(buf, NANO_FONT, 700 * SCREEN_WIDTH / 800,
						SCREEN_HEIGHT - 15 * SCREEN_HEIGHT / 600);
		}

		/* idiot lights for low power */
		sng_set_foreground(UI_COLOR(weap_warning));
		if (o->tsd.ship.power_data.phasers.i < idiot_light_threshold) {
			sng_center_xy_draw_string("LOW PHASER POWER", NANO_FONT,
					0.5 * SCREEN_WIDTH, 65 * SCREEN_HEIGHT / 600);
		}

		/* show security indicator */
		if (o->tsd.ship.in_secure_area && timer & 0x08) {
			sng_set_foreground(UI_COLOR(weap_warning));
			sng_center_xy_draw_string("HIGH SECURITY", NANO_FONT,
				0.5 * SCREEN_WIDTH, 5 * SCREEN_HEIGHT / 6);
			sng_center_xy_draw_string("AREA", NANO_FONT, 0.5 * SCREEN_WIDTH,
					516 * SCREEN_HEIGHT / 600);
		}

		show_gunsight(w);
	}
	pthread_mutex_unlock(&universe_mutex);
}

/* Add player ship entity into the scene for some camera modes */
static struct entity *main_view_add_player_ship_entity(struct snis_entity *o)
{
	struct entity *player_ship;
	static float current_lights = 1.0;

	/* temporarily add ship into scene */
	player_ship = add_entity(ecx, ship_mesh_map[o->tsd.ship.shiptype], o->x, o->y, o->z, SHIP_COLOR);
	if (!player_ship)
		return NULL;

	float desired_lights = (float) o->tsd.ship.exterior_lights / 255.0;
	current_lights += (desired_lights - current_lights) * 0.05;
	update_entity_orientation(player_ship, &o->orientation);
	entity_update_emit_intensity(player_ship, current_lights);
	entity_set_in_shade(player_ship, 0.9 * (o->shading_planet != NULL) + 0.1);

	struct entity *turret_base = add_entity(ecx, ship_turret_base_mesh,
		-4 * SHIP_MESH_SCALE, 5.45 * SHIP_MESH_SCALE, 0 * SHIP_MESH_SCALE,
		SHIP_COLOR);

	if (turret_base) {
		update_entity_orientation(turret_base, &identity_quat);
		update_entity_parent(ecx, turret_base, player_ship);
		entity_set_in_shade(turret_base, 0.9 * (o->shading_planet != NULL) + 0.1);
	}

	struct entity *turret = add_entity(ecx, ship_turret_mesh, 0, 0, 0, SHIP_COLOR);

	if (turret) {
		/* TODO: this should probably happen in interpolate_oriented_object */
		update_entity_orientation(turret, &o->tsd.ship.weap_orientation);
		if (turret_base)
			update_entity_parent(ecx, turret, turret_base);
		entity_set_in_shade(turret, 0.9 * (o->shading_planet != NULL) + 0.1);
	}
	if (!o->tsd.ship.reverse)
		add_ship_thrust_entities(NULL, NULL, ecx, player_ship, o->tsd.ship.shiptype,
				o->tsd.ship.power_data.impulse.i, 0);
	return player_ship;
}

static void point_external_camera_at_ship(void)
{
	struct snis_entity *o;
	union vec3 to_ship;
	union vec3 xaxis = { { 1.0, 0.0, 0.0 } };
	union vec3 ship_up = { { 0.0, 1.0, 0.0 } };

	if (external_camera_mode == 2)
		return;

	o = find_my_ship();
	if (!o)
		return;

	to_ship.v.x = o->x - external_camera_position.v.x;
	to_ship.v.y = o->y - external_camera_position.v.y;
	to_ship.v.z = o->z - external_camera_position.v.z;
	quat_rot_vec_self(&ship_up, &o->orientation);
	quat_from_u2v(&desired_external_camera_orientation, &xaxis, &to_ship, &ship_up);

	if (external_camera_mode == 1)
		return;

	desired_external_camera_position.v.x = 200;
	desired_external_camera_position.v.y = 200;
	desired_external_camera_position.v.z = 200;
	quat_rot_vec_self(&desired_external_camera_position, &o->orientation);
	desired_external_camera_position.v.x += o->x;
	desired_external_camera_position.v.y += o->y;
	desired_external_camera_position.v.z += o->z;
}

static void update_external_camera_position_and_orientation(struct snis_entity *o, int first_frame,
					union vec3 *cam_pos, union quat *cam_orientation)
{
	point_external_camera_at_ship();

	if (first_frame) {
		external_camera_orientation = desired_external_camera_orientation;
		external_camera_position = desired_external_camera_position;
	} else {
		quat_nlerp(&external_camera_orientation, &external_camera_orientation,
				&desired_external_camera_orientation, 0.15 / (use_60_fps + 1));
		vec3_add_self(&desired_external_camera_position, &external_camera_velocity);
		vec3_mul_self(&external_camera_velocity, 0.95); /* Damping */
		vec3_lerp(&external_camera_position, &external_camera_position,
				&desired_external_camera_position, 0.15 / (use_60_fps + 1));
	}
	*cam_orientation = external_camera_orientation;
	*cam_pos = external_camera_position;
}

static void draw_nav_main_idiot_lights(GtkWidget *w, GdkGC *gc, struct snis_entity *ship, int color)
{
	/* idiot lights for low power of various systems */
	sng_set_foreground(UI_COLOR(nav_warning));
	if (ship->tsd.ship.power_data.sensors.i < idiot_light_threshold && (timer & 0x08))
		sng_center_xy_draw_string("LOW SENSOR POWER", NANO_FONT, SCREEN_WIDTH / 2, txy(65));
	if (ship->tsd.ship.power_data.maneuvering.i < idiot_light_threshold && (timer & 0x08))
		sng_center_xy_draw_string("LOW MANEUVERING POWER", NANO_FONT, SCREEN_WIDTH / 2, txy(80));
	if (ship->tsd.ship.power_data.impulse.r2 < idiot_light_threshold && (timer & 0x08))
		sng_center_xy_draw_string("LOW IMPULSE POWER", NANO_FONT, SCREEN_WIDTH / 2, txy(95));
	if (ship->tsd.ship.warp_core_status == WARP_CORE_STATUS_EJECTED)
		sng_center_xy_draw_string("WARP CORE EJECTED", NANO_FONT, SCREEN_WIDTH / 2, txy(110));
	if (displaymode == DISPLAYMODE_NAVIGATION && ship->tsd.ship.power_data.warp.r2 < idiot_light_threshold)
		sng_center_xy_draw_string("LOW WARP POWER", NANO_FONT,
				SCREEN_WIDTH - txx(1.2 * nav_ui.gauge_radius + 10), /* should match gauge x */
				txy(nav_ui.gauge_radius * 2 + 20));
}

static double sample_power_data_impulse_current(void);

static void adjust_main_view_azimuth_angle(union quat *cam_orientation)
{
	union quat az;
	if (main_view_azimuth_angle == 0.0)
		return;

	quat_init_axis(&az, 0.0, 1.0, 0.0, main_view_azimuth_angle * M_PI / 180.0);
	quat_mul_self(cam_orientation, &az);
}

static void show_mainscreen(GtkWidget *w)
{
	const float min_angle_of_view = 5.0 * M_PI / 180.0;
	const float max_angle_of_view = ANGLE_OF_VIEW * M_PI / 180.0;
	static int current_zoom = 0;
	float angle_of_view;
	struct snis_entity *o;
	static union quat camera_orientation = IDENTITY_QUAT_INITIALIZER;
	union quat desired_cam_orientation;
	static union vec3 cam_offset;
	union vec3 cam_pos;
	struct snis_entity *vp;
	struct entity *player_ship = 0;
	double impulse_power;

	if (!(o = find_my_ship()))
		return;
	vp = o;

	/* Set up for alternate viewpoint (e.g. mining robot camera) */
	if (o->tsd.ship.viewpoint_object != o->id && o->tsd.ship.view_mode != MAINSCREEN_VIEW_MODE_WEAPONS) {
		int i;

		i = lookup_object_by_id(o->tsd.ship.viewpoint_object);
		if (i >= 0)
			vp = &go[i];
	}
	if (vp == o)
		update_warp_tunnel(o, &warp_tunnel);

	static int last_timer = 0;
	int first_frame = (timer != last_timer+1);
	last_timer = timer;

	if (first_frame)
		current_zoom = o->tsd.ship.mainzoom;
	else
		current_zoom = newzoom(current_zoom, o->tsd.ship.mainzoom);

	angle_of_view = ((255.0 - (float) current_zoom) / 255.0) *
				(max_angle_of_view - min_angle_of_view) + min_angle_of_view;

	if (!external_camera_active) {
		if (o->tsd.ship.view_mode == MAINSCREEN_VIEW_MODE_NORMAL) {
			desired_cam_orientation = vp->orientation;
			adjust_main_view_azimuth_angle(&desired_cam_orientation);
			switch (camera_mode) {
			case 0:
				camera_orientation = desired_cam_orientation;
				break;
			case 1:
			case 2:
				if (first_frame)
					camera_orientation = desired_cam_orientation;
				else
					quat_nlerp(&camera_orientation, &camera_orientation,
							&desired_cam_orientation, 0.08 / (use_60_fps + 1));
				break;
			}
		} else {
			quat_mul(&camera_orientation, &o->orientation, &o->tsd.ship.weap_orientation);
		}

		union vec3 desired_cam_offset;

		switch (camera_mode) {
		case 0:
			vec3_init(&desired_cam_offset, 0, 0, 0);
			if (vp == o)
				break;
		case 1:
		case 2: {
				vec3_init(&desired_cam_offset, -1.0f, 0.25f, 0.0f);
				vec3_mul_self(&desired_cam_offset,
						200.0f * camera_mode * SHIP_MESH_SCALE);
				quat_rot_vec_self(&desired_cam_offset, &camera_orientation);
				player_ship = main_view_add_player_ship_entity(o);
			}
			break;
		}

		if (first_frame)
			cam_offset = desired_cam_offset;
		else
			vec3_lerp(&cam_offset, &cam_offset, &desired_cam_offset, 0.15 / (use_60_fps + 1));

		/* If impulse power is really cooking, add in some screen shake */
		impulse_power = sample_power_data_impulse_current();
		if (impulse_power > 220.0) {
			float new_camera_shake = impulse_camera_shake * 0.25 * (impulse_power - 220.0) / 35.0;
			if (new_camera_shake > main_camera_shake)
				main_camera_shake = new_camera_shake;
		}

		if (main_camera_shake > 0.05 && vp == o) {
			float ryaw, rpitch;

			ryaw = main_camera_shake * ((snis_randn(100) - 50) * 0.025f) * M_PI / 180.0;
			rpitch = main_camera_shake * ((snis_randn(100) - 50) * 0.025f) * M_PI / 180.0;
			quat_apply_relative_yaw_pitch(&camera_orientation, ryaw, rpitch);
			main_camera_shake = 0.7f * main_camera_shake;
		}

		cam_pos.v.x = vp->x + cam_offset.v.x;
		cam_pos.v.y = vp->y + cam_offset.v.y;
		cam_pos.v.z = vp->z + cam_offset.v.z;
	} else { /* external camera is active */
		cam_pos.v.x = 0;
		cam_pos.v.y = 0;
		cam_pos.v.z = 0;
		update_external_camera_position_and_orientation(o, first_frame, &cam_pos, &camera_orientation);
		player_ship = main_view_add_player_ship_entity(o);
	}

	camera_set_pos(ecx, cam_pos.v.x, cam_pos.v.y, cam_pos.v.z);
	camera_set_orientation(ecx, &camera_orientation);
	camera_set_parameters(ecx, NEAR_CAMERA_PLANE, FAR_CAMERA_PLANE,
				SCREEN_WIDTH, SCREEN_HEIGHT, angle_of_view);
	set_window_offset(ecx, 0, 0);
	set_lighting(ecx, SUNX, SUNY, SUNZ);
	set_ambient_light(ecx, ambient_light);
	calculate_camera_transform(ecx);
	entity_context_set_hi_lo_poly_pixel_threshold(ecx, low_poly_threshold);

	sng_set_foreground(GREEN);
	if (!ecx_fake_stars_initialized) {
		ecx_fake_stars_initialized = 1;
		entity_init_fake_stars(ecx, nfake_stars, 300.0f * 10.0f);
	}

	render_skybox(ecx);

	pthread_mutex_lock(&universe_mutex);
	show_lens_flare(o, &cam_pos, &camera_orientation); /* this will be using data from last frame */
	render_entities(ecx);
	remove_lens_flare_entities();

	/* if we added the ship into the scene, remove it now */
	if (player_ship) {
		remove_entity(ecx, player_ship);
	}

	/* Draw science selector indicator on main screen */
	if (curr_science_guy && vp == o) {
		float sx, sy;

		if (curr_science_guy->alive && curr_science_guy->entity &&
			entity_onscreen(curr_science_guy->entity)) {
			entity_get_screen_coords(curr_science_guy->entity, &sx, &sy);
			draw_targeting_indicator(w, gc, sx, sy, UI_COLOR(main_sci_selection), 0, 1.0f, 2.0f);
		}
	}

	if (current_altitude < 2000 && !external_camera_active) {
		char buf[25];
		snprintf(buf, sizeof(buf), "ALTITUDE: %5.1f", current_altitude);
		sng_abs_xy_draw_string(buf, NANO_FONT, txx(20), txy(20));
	}

	if (o->tsd.ship.view_mode == MAINSCREEN_VIEW_MODE_WEAPONS)
		show_gunsight(w);
	if (vp == o)
		draw_main_screen_text(w, gc);
	draw_nav_main_idiot_lights(w, gc, o, UI_COLOR(main_warning));
	pthread_mutex_unlock(&universe_mutex);
	if (vp == o)
		show_common_screen(w, "");
	else
		show_common_screen(w, "REMOTE CAMERA FEED");
}

/* position and dimensions of science scope */
#define SCIENCE_SCOPE_X (20 * SCREEN_WIDTH / 800)
#define SCIENCE_SCOPE_Y (70 * SCREEN_HEIGHT / 600)
#define SCIENCE_SCOPE_W (500 * SCREEN_HEIGHT / 600)
#define SCIENCE_SCOPE_H SCIENCE_SCOPE_W
#define SCIENCE_SCOPE_R (SCIENCE_SCOPE_H / 2)
#define SCIENCE_SCOPE_CX (SCIENCE_SCOPE_X + SCIENCE_SCOPE_R)
#define SCIENCE_SCOPE_CY (SCIENCE_SCOPE_Y + SCIENCE_SCOPE_R)

static void snis_draw_arrow(GtkWidget *w, GdkGC *gc, gint x, gint y, gint r,
		double heading, double scale);
static void snis_draw_science_guy(GtkWidget *w, GdkGC *gc, struct snis_entity *o,
					float x, float y, double dist, int bw, int pwr,
					double range, int selected,
					int nebula_factor)
{
	int i;

	double da;
	int dr;
	double tx, ty;
	char buffer[50];
	int divisor;

	/* Compute radius of ship blip */
	divisor = hypot((float) bw + 1, 256.0 - pwr);
	dr = (int) dist / (XKNOWN_DIM / divisor);
	dr = dr * MAX_SCIENCE_SCREEN_RADIUS / range;
	if (nebula_factor) {
		dr = dr * 10; 
		dr += 200;
	}

	sng_set_foreground(UI_COLOR(sci_ball_default_blip));
	if (!o->sdata.science_data_known) {
		for (i = 0; i < 10; i++) {
			da = snis_randn(360) * M_PI / 180.0;
			tx = (int) ((double) x + sin(da) * (double) snis_randn(dr));
			ty = (int) ((double) y + cos(da) * (double) snis_randn(dr)); 

			sng_draw_point(tx, ty);
		}
	} else {
		switch(o->type) {
		case OBJTYPE_SHIP2:
		case OBJTYPE_SHIP1:
			sng_set_foreground(UI_COLOR(sci_ball_ship));
			break;
		case OBJTYPE_WARPGATE:
			sng_set_foreground(UI_COLOR(sci_ball_warpgate));
			break;
		case OBJTYPE_STARBASE:
			sng_set_foreground(UI_COLOR(sci_ball_starbase));
			break;
		case OBJTYPE_ASTEROID:
			sng_set_foreground(UI_COLOR(sci_ball_asteroid));
			break;
		case OBJTYPE_DERELICT:
			sng_set_foreground(UI_COLOR(sci_ball_derelict));
			break;
		case OBJTYPE_PLANET:
			sng_set_foreground(UI_COLOR(sci_ball_planet));
			break;
		case OBJTYPE_BLACK_HOLE:
			sng_set_foreground(UI_COLOR(sci_ball_black_hole));
			break;
		case OBJTYPE_TORPEDO:
			sng_set_foreground(UI_COLOR(sci_ball_energy));
			break;
		case OBJTYPE_MISSILE:
			sng_set_foreground(UI_COLOR(sci_ball_energy));
			break;
		case OBJTYPE_LASER:
			sng_set_foreground(UI_COLOR(sci_ball_energy));
			break;
		case OBJTYPE_SPACEMONSTER:
			sng_set_foreground(UI_COLOR(sci_ball_monster));
			break;
		case OBJTYPE_SPARK:
		case OBJTYPE_EXPLOSION:
		default:
			sng_set_foreground(UI_COLOR(sci_ball_default_blip));
		}
		if (o->type == OBJTYPE_SHIP2 || o->type == OBJTYPE_SHIP1) {
			snis_draw_arrow(w, gc, x, y, SCIENCE_SCOPE_R / 2, o->heading, 0.3);
		} else if (o->type == OBJTYPE_PLANET) {
			sng_draw_circle(0, x, y, 20);
		} else {
			snis_draw_line(x - 1, y, x + 1, y);
			snis_draw_line(x, y - 1, x, y + 1);
		}
	}

	if (selected)
		sng_draw_circle(0, x, y, 10 + timer % 4);
	
	if (o->sdata.science_data_known) {
		switch (o->type) {
		case OBJTYPE_SHIP2:
			sng_set_foreground(UI_COLOR(sci_ball_ship));
			if (global_rts_mode) {
				int unit_type;
				char *unit_type_name;
				unit_type = ship_type[o->sdata.subclass].rts_unit_type;
				unit_type_name = unit_type < 0 ? "UNKNOWN" :
						rts_unit_type(unit_type)->name;
				snprintf(buffer, sizeof(buffer), "%s %s\n", o->sdata.name, unit_type_name);
			} else {
				snprintf(buffer, sizeof(buffer), "%s %s\n", o->sdata.name,
					ship_type[o->sdata.subclass].class);
			}
			break;
		case OBJTYPE_SHIP1:
			sng_set_foreground(UI_COLOR(sci_ball_ship));
			snprintf(buffer, sizeof(buffer), "%s %s\n", o->sdata.name,
					ship_type[o->sdata.subclass].class); 
			break;
		case OBJTYPE_WARPGATE:
			sng_set_foreground(UI_COLOR(sci_ball_warpgate));
			snprintf(buffer, sizeof(buffer), "%s %s\n", "WG",  o->sdata.name);
			break;
		case OBJTYPE_STARBASE:
			sng_set_foreground(UI_COLOR(sci_ball_starbase));
			snprintf(buffer, sizeof(buffer), "%s %s\n", "SB",  o->sdata.name);
			break;
		case OBJTYPE_ASTEROID:
			sng_set_foreground(UI_COLOR(sci_ball_asteroid));
			snprintf(buffer, sizeof(buffer), "%s %s\n", "A",  o->sdata.name);
			break;
		case OBJTYPE_DERELICT:
			sng_set_foreground(UI_COLOR(sci_ball_derelict));
			snprintf(buffer, sizeof(buffer), "%s %s\n", "D",  "???");
			break;
		case OBJTYPE_BLACK_HOLE:
			sng_set_foreground(UI_COLOR(sci_ball_black_hole));
			snprintf(buffer, sizeof(buffer), "%s %s\n", "B",  o->sdata.name);
			break;
		case OBJTYPE_SPACEMONSTER:
			sng_set_foreground(UI_COLOR(sci_ball_monster));
			snprintf(buffer, sizeof(buffer), "%s\n", o->sdata.name);
			break;
		case OBJTYPE_PLANET:
			sng_set_foreground(UI_COLOR(sci_ball_planet));
			snprintf(buffer, sizeof(buffer), "%s %s\n", "P",  o->sdata.name);
			break;
		case OBJTYPE_TORPEDO:
		case OBJTYPE_MISSILE:
		case OBJTYPE_SPARK:
		case OBJTYPE_EXPLOSION:
		case OBJTYPE_LASER:
			sng_set_foreground(UI_COLOR(sci_ball_energy));
			strcpy(buffer, "");
			break;
		default:
			sng_set_foreground(UI_COLOR(sci_ball_default_blip));
			snprintf(buffer, sizeof(buffer), "%s %s\n", "?", o->sdata.name);
			break;
		}
		sng_abs_xy_draw_string(buffer, PICO_FONT, x + 8, y - 8);
	}
}

static void snis_draw_3d_science_guy(GtkWidget *w, GdkGC *gc, struct snis_entity *o,
					gint *x, gint *y, double dist, int bw, int pwr,
					double range, int selected, double scale,
					int nebula_factor)
{
	int i;

	double da, db;
	int dr;
	float tx, ty, tz;
	float sx, sy, r;
	char buffer[50];
	int divisor;
	struct entity *e;

	/* Compute radius of ship blip */
	divisor = hypot((float) bw + 1, 256.0 - pwr);
	dr = (int) dist / (XKNOWN_DIM / divisor);
	dr = dr * MAX_SCIENCE_SCREEN_RADIUS / range;
	if (nebula_factor) {
		dr = dr * 10; 
		dr += 200;
	}
	dr *= 100.0;

	transform_point(sciballecx, o->x, o->y, o->z, &sx, &sy);
	*x = (int) sx;
	*y = (int) sy;
	r = hypot(sx - SCIENCE_SCOPE_CX, sy - SCIENCE_SCOPE_CY);
	if (r >= SCIENCE_SCOPE_R)
		return;
	sng_set_foreground(UI_COLOR(sci_ball_default_blip));
	if (!o->sdata.science_data_known) {
		for (i = 0; i < 10; i++) {
			da = snis_randn(360) * M_PI / 180.0;
			db = snis_randn(360) * M_PI / 180.0;
			tx = o->x + sin(da) * (float) snis_randn(dr);
			ty = o->y + cos(da) * (float) snis_randn(dr); 
			tz = o->z + cos(db) * (float) snis_randn(dr); 
			if (transform_point(sciballecx, tx, ty, tz, &sx, &sy))
				continue;
			r = hypot(sx - SCIENCE_SCOPE_CX, sy - SCIENCE_SCOPE_CY);
			if (r >= SCIENCE_SCOPE_R)
				continue;
			sng_draw_point(sx, sy);
		}
	} else {
		switch(o->type) {
		case OBJTYPE_SHIP2:
		case OBJTYPE_SHIP1:
			sng_set_foreground(UI_COLOR(sci_ball_ship));
			break;
		case OBJTYPE_WARPGATE:
			sng_set_foreground(UI_COLOR(sci_ball_warpgate));
			break;
		case OBJTYPE_STARBASE:
			sng_set_foreground(UI_COLOR(sci_ball_starbase));
			break;
		case OBJTYPE_ASTEROID:
		case OBJTYPE_DERELICT:
			sng_set_foreground(UI_COLOR(sci_ball_asteroid));
			break;
		case OBJTYPE_BLACK_HOLE:
			sng_set_foreground(UI_COLOR(sci_ball_black_hole));
			break;
		case OBJTYPE_PLANET:
			sng_set_foreground(UI_COLOR(sci_ball_planet));
			break;
		case OBJTYPE_TORPEDO:
		case OBJTYPE_MISSILE:
		case OBJTYPE_SPARK:
		case OBJTYPE_EXPLOSION:
		case OBJTYPE_LASER:
		default:
			sng_set_foreground(UI_COLOR(sci_ball_energy));
		}
		if (o->type == OBJTYPE_SHIP2 || o->type == OBJTYPE_SHIP1) {
			e = add_entity(sciballecx, ship_icon_mesh, o->x, o->y, o->z, UI_COLOR(sci_ball_ship));
			if (e) {
				update_entity_scale(e, scale);
				update_entity_orientation(e, &o->orientation);
			}
		} else {
			snis_draw_line(*x - 1, *y, *x + 1, *y);
			snis_draw_line(*x, *y - 1, *x, *y + 1);
		}
	}
	if (selected)
		sng_draw_circle(0, sx, sy, 10);

	if (o->sdata.science_data_known) {
		switch (o->type) {
		case OBJTYPE_SHIP2:
		case OBJTYPE_SHIP1:
			sng_set_foreground(UI_COLOR(sci_ball_ship));
			snprintf(buffer, sizeof(buffer), "%s %s\n", o->sdata.name,
				ship_type[o->sdata.subclass].class); 
			break;
		case OBJTYPE_WARPGATE:
			sng_set_foreground(UI_COLOR(sci_ball_warpgate));
			snprintf(buffer, sizeof(buffer), "%s %s\n", "SB",  o->sdata.name);
		case OBJTYPE_STARBASE:
			sng_set_foreground(UI_COLOR(sci_ball_starbase));
			snprintf(buffer, sizeof(buffer), "%s %s\n", "SB",  o->sdata.name);
			break;
		case OBJTYPE_ASTEROID:
			sng_set_foreground(UI_COLOR(sci_ball_asteroid));
			snprintf(buffer, sizeof(buffer), "%s %s\n", "A",  o->sdata.name);
			break;
		case OBJTYPE_DERELICT:
			sng_set_foreground(UI_COLOR(sci_ball_asteroid));
			snprintf(buffer, sizeof(buffer), "%s %s\n", "A",  "???");
			break;
		case OBJTYPE_BLACK_HOLE:
			sng_set_foreground(UI_COLOR(sci_ball_black_hole));
			snprintf(buffer, sizeof(buffer), "%s %s\n", "B",  o->sdata.name);
			break;
		case OBJTYPE_PLANET:
			sng_set_foreground(UI_COLOR(sci_ball_planet));
			snprintf(buffer, sizeof(buffer), "%s %s\n", "P",  o->sdata.name);
			break;
		case OBJTYPE_TORPEDO:
		case OBJTYPE_MISSILE:
		case OBJTYPE_SPARK:
		case OBJTYPE_EXPLOSION:
		case OBJTYPE_LASER:
			sng_set_foreground(UI_COLOR(sci_ball_energy));
			strcpy(buffer, "");
			break;
		default:
			sng_set_foreground(UI_COLOR(sci_ball_default_blip));
			snprintf(buffer, sizeof(buffer), "%s %s\n", "?", o->sdata.name);
			break;
		}
		sng_abs_xy_draw_string(buffer, PICO_FONT, *x + 8, *y - 8);
	}
}


static void snis_draw_arrow(GtkWidget *w, GdkGC *gc, gint x, gint y, gint r,
		double heading, double scale)
{
	int nx, ny, tx1, ty1, tx2, ty2;

	/* Draw ship... */
#define SHIP_SCALE_DOWN 15.0
	nx = cos(heading) * scale * r / SHIP_SCALE_DOWN;
	ny = -sin(heading) * scale * r / SHIP_SCALE_DOWN;
	snis_draw_line(x, y, x + nx, y + ny);
	tx1 = cos(heading + PI / 2.0) * scale * r / (SHIP_SCALE_DOWN * 2.0) - nx / 2.0;
	ty1 = -sin(heading + PI / 2.0) * scale * r / (SHIP_SCALE_DOWN * 2.0) - ny / 2.0;
	snis_draw_line(x, y, x + tx1, y + ty1);
	tx2 = cos(heading - PI / 2.0) * scale * r / (SHIP_SCALE_DOWN * 2.0) - nx / 2.0;
	ty2 = -sin(heading - PI / 2.0) * scale * r / (SHIP_SCALE_DOWN * 2.0) - ny / 2.0;
	snis_draw_line(x, y, x + tx2, y + ty2);
	snis_draw_line(x + nx, y + ny, x + tx1, y + ty1);
	snis_draw_line(x + tx1, y + ty1, x + tx2, y + ty2);
	snis_draw_line(x + tx2, y + ty2, x + nx, y + ny);
}

/* this science_guy[] array is used for mouse clicking. */
struct science_data {
	int sx, sy; /* screen coords on scope */
	struct snis_entity *o;
	int waypoint_index;
};
static struct science_data science_guy[MAXGAMEOBJS] = { {0}, };
static int nscience_guys = 0;

static void science_menu_selection(void *x)
{
	intptr_t id = (intptr_t) x;
	struct snis_entity *selected;

	pthread_mutex_lock(&universe_mutex);
	selected = lookup_entity_by_id((uint32_t) id);
	if (!selected) {
		pthread_mutex_unlock(&universe_mutex);
		return;
	}
	if (curr_science_guy != selected)
		request_sci_select_target(OPCODE_SCI_SELECT_TARGET_TYPE_OBJECT, selected->id);
	else
		request_sci_select_target(OPCODE_SCI_SELECT_TARGET_TYPE_OBJECT, (uint32_t) -1); /* deselect */
	pthread_mutex_unlock(&universe_mutex);
}

/* Populate the science pull down menu with contents of science_guy[] array */
static void populate_science_pull_down_menu(void)
{
	int i;
	struct snis_entity *o;

	pull_down_menu_clear(sci_ui.menu);
	pull_down_menu_add_column(sci_ui.menu, "SHIPS");
	pull_down_menu_add_column(sci_ui.menu, "STARBASES");
	pull_down_menu_add_column(sci_ui.menu, "WARPGATES");
	pull_down_menu_add_column(sci_ui.menu, "PLANETS");
	pull_down_menu_add_column(sci_ui.menu, "ASTEROIDS");

	for (i = 0; i < nscience_guys; i++) {
		o = science_guy[i].o;
		if (o && o->sdata.science_data_known) {
			switch (o->type) {
			case OBJTYPE_PLANET:
				pull_down_menu_add_row(sci_ui.menu, "PLANETS",
						o->sdata.name[0] == '\0' ? "UNKNOWN" : o->sdata.name,
						science_menu_selection, (void *) (intptr_t) o->id);
				break;
			case OBJTYPE_WARPGATE:
				pull_down_menu_add_row(sci_ui.menu, "WARPGATES",
						o->sdata.name[0] == '\0' ? "UNKNOWN" : o->sdata.name,
						science_menu_selection, (void *) (intptr_t) o->id);
				break;
			case OBJTYPE_STARBASE:
				pull_down_menu_add_row(sci_ui.menu, "STARBASES",
						o->sdata.name[0] == '\0' ? "UNKNOWN" : o->sdata.name,
						science_menu_selection, (void *) (intptr_t) o->id);
				break;
			case OBJTYPE_SHIP1:
			case OBJTYPE_SHIP2:
			case OBJTYPE_DERELICT:
				pull_down_menu_add_row(sci_ui.menu, "SHIPS",
						o->sdata.name[0] == '\0' ? "UNKNOWN" : o->sdata.name,
						science_menu_selection, (void *) (intptr_t) o->id);
				break;
			case OBJTYPE_ASTEROID:
				pull_down_menu_add_row(sci_ui.menu, "ASTEROIDS",
						o->sdata.name[0] == '\0' ? "UNKNOWN" : o->sdata.name,
						science_menu_selection, (void *) (intptr_t) o->id);
				break;
			default:
				break;
			}
		}
	}
}

static void draw_3d_laserbeam(GtkWidget *w, GdkGC *gc, struct entity_context *cx, struct snis_entity *o, struct snis_entity *laserbeam, double r)
{
	int rc;
	struct snis_entity *shooter, *shootee;

	shooter = lookup_entity_by_id(laserbeam->tsd.laserbeam.origin);
	if (!shooter)
		return;
	shootee = lookup_entity_by_id(laserbeam->tsd.laserbeam.target);
	if (!shootee)
		return;

	union vec3 center = {{o->x, o->y, o->z}};
	union vec3 vshooter = {{shooter->x, shooter->y, shooter->z}};
	union vec3 vshootee = {{shootee->x, shootee->y, shootee->z}};

	if (shootee->type == OBJTYPE_PLANET) { /* Account for the radius of the planet */
		union vec3 shooter_to_shootee;
		float len;
		vec3_sub(&shooter_to_shootee, &vshootee, &vshooter);
		len = vec3_magnitude(&shooter_to_shootee);
		vec3_mul_self(&shooter_to_shootee, (len - shootee->tsd.planet.radius) / len);
		vec3_add(&vshootee, &vshooter, &shooter_to_shootee);
	}

	union vec3 clip1, clip2;
	rc = sphere_line_segment_intersection(&vshooter, &vshootee, &center, r, &clip1, &clip2);
	if (rc < 0)
		return;
	float sx1, sy1, sx2, sy2;
	if (!transform_line(cx, clip1.v.x, clip1.v.y, clip1.v.z, clip2.v.x, clip2.v.y, clip2.v.z, &sx1, &sy1, &sx2, &sy2)) {
		sng_draw_laser_line(sx1, sy1, sx2, sy2, UI_COLOR(nav_laser));
	}
}

static void snis_draw_3d_dotted_line(struct entity_context *cx,
				float x1, float y1, float z1, float x2, float y2, float z2 )
{
	float sx1, sy1, sx2, sy2;
	if (!transform_line(cx, x1, y1, z1, x2, y2, z2, &sx1, &sy1, &sx2, &sy2))
		sng_draw_dotted_line(sx1, sy1, sx2, sy2);
}

static void snis_draw_3d_line(GtkWidget *w, GdkGC *gc, struct entity_context *cx,
				float x1, float y1, float z1, float x2, float y2, float z2 )
{
	float sx1, sy1, sx2, sy2;
	if (!transform_line(cx, x1, y1, z1, x2, y2, z2, &sx1, &sy1, &sx2, &sy2)) {
		snis_draw_line(sx1, sy1, sx2, sy2);
	}
}

static void snis_draw_3d_moving_line(struct entity_context *cx,
			float x1, float y1, float z1, float x2, float y2, float z2)
{
	int a, b;
	union vec3 v1, v2;

	snis_draw_3d_dotted_line(cx, x1, y1, z1, x2, y2, z2);
	v1.v.x = x2 - x1;
	v1.v.y = y2 - y1;
	v1.v.z = z2 - z1;
	v2 = v1;
	a = timer & 0xf;
	b = (timer + 1) & 0xf;
	vec3_mul_self(&v1, a / 16.0);
	vec3_mul_self(&v2, b / 16.0);
	v1.v.x += x1;
	v1.v.y += y1;
	v1.v.z += z1;
	v2.v.x += x1;
	v2.v.y += y1;
	v2.v.z += z1;
	if (a < b)
		snis_draw_3d_line(NULL, NULL, instrumentecx,
			v1.v.x, v1.v.y, v1.v.z, v2.v.x, v2.v.y, v2.v.z);
}

static void snis_draw_3d_string(struct entity_context *cx, char *string, int font, float x, float y, float z)
{
	float sx, sy;

	transform_point(cx, x, y, z, &sx, &sy);
	if (sx >= 0 && sy >= 0)
		sng_abs_xy_draw_string(string, font, sx, sy);
}

static void draw_3d_mark_arc(GtkWidget *w, GdkGC *gc, struct entity_context *ecx,
			const union vec3 *center, float r, float heading, float mark)
{
	/* break arc into max 5 degree segments */
	int increments = (int) fabs(mark / (5.0 * M_PI / 180.0)) + 1;
	float delta = mark / increments;
	int i;
	union vec3 p1;
	for (i = 0; i <= increments; i++) {
		union vec3 p2;
		heading_mark_to_vec3(r, heading, delta * i, &p2);
		vec3_add_self(&p2, center);

		if (i != 0) {
			snis_draw_3d_line(w, gc, ecx, p1.v.x, p1.v.y, p1.v.z, p2.v.x, p2.v.y, p2.v.z);
		}
		p1 = p2;
	}
}

static void add_basis_ring(struct entity_context *ecx, float x, float y, float z,
		float ax, float ay, float az, float angle, float r, int color);

enum {
	TWEEN_EXP_DECAY,
	TWEEN_LINEAR_DECAY
};

struct tween_state
{
	uint32_t id; /* object id or waypoint index, depending on value of .active */
	float value;
	int mode;
	float delta;
	float min;
	float max;
	char active;
#define TWEEN_INACTIVE 0
#define TWEEN_ACTIVE_OBJECT 1
#define TWEEN_ACTIVE_WAYPOINT 2
};

struct tween_map
{
	int last_index;
	struct tween_state* states;
};

static struct tween_map *tween_init(int max_size)
{
	struct tween_map *result = malloc(sizeof(struct tween_map));
	result->last_index = 0;
	result->states = malloc(sizeof(struct tween_state) * max_size);
	memset(result->states, 0, sizeof(struct tween_state) * max_size);
	return result;
}

static void tween_update(struct tween_map *map)
{
	int i;
	for (i=0; i < map->last_index; i++) {
		if (!map->states[i].active)
			continue;

		switch (map->states[i].mode) {
		case TWEEN_LINEAR_DECAY:
			map->states[i].value += map->states[i].delta;
			break;
		case TWEEN_EXP_DECAY:
			if (map->states[i].delta < 0)
				map->states[i].value += (map->states[i].value - map->states[i].min) * map->states[i].delta;
			else
				map->states[i].value += (map->states[i].max - map->states[i].value) * map->states[i].delta;
			break;
		}
		if (map->states[i].value > map->states[i].max)
			map->states[i].value = map->states[i].max;
		if (map->states[i].value < map->states[i].min)
			map->states[i].value = map->states[i].min;

		if (map->states[i].mode == TWEEN_EXP_DECAY || map->states[i].mode == TWEEN_EXP_DECAY) {
			/* if we have decayed to < 1% then we are not active */
			if (map->states[i].value < (map->states[i].max - map->states[i].min) * 0.01)
				map->states[i].active = TWEEN_INACTIVE;
		}
	}
}

static int __attribute__((unused)) tween_get_value(struct tween_map *map, char active, uint32_t id, float *value)
{
	int i;

	for (i = 0; i < map->last_index; i++)
		if (map->states[i].active == active && map->states[i].id == id) {
			*value = map->states[i].value;
			return 1;
		}
	return 0;
}

static int tween_get_value_and_decay(struct tween_map *map, char active, uint32_t id, float *value)
{
	int i;

	for (i = 0; i < map->last_index; i++)
		if (map->states[i].active == active && map->states[i].id == id) {
			if (map->states[i].delta > 0)
				map->states[i].delta = -map->states[i].delta;
			*value = map->states[i].value;
			return 1;
		}
	return 0;
}


static void tween_add_or_update(struct tween_map *map, char active, uint32_t id, float initial_value,
					int mode, float delta, float min, float max)
{
	int first_free = map->last_index;
        int i;

        for (i=0; i < map->last_index; i++) {
                if (!map->states[i].active) {
			if (i < first_free)
				first_free = i;
			continue;
		}
		if (map->states[i].id == id && map->states[i].active == active) {
			map->states[i].mode = mode;
			map->states[i].delta = delta;
			map->states[i].min = min;
			map->states[i].max = max;
                        return;
		}
        }

	map->states[first_free].id = id;
	map->states[first_free].active = active;
	map->states[first_free].value = initial_value;
	map->states[first_free].mode = mode;
	map->states[first_free].delta = delta;
	map->states[first_free].min = min;
	map->states[first_free].max = max;

	if (first_free >= map->last_index)
		map->last_index = first_free + 1;
}

static struct tween_map *sciplane_tween = 0;

static void draw_sciplane_laserbeam(GtkWidget *w, GdkGC *gc, struct entity_context *cx, struct snis_entity *o, struct snis_entity *laserbeam, double r)
{
	int i, rc, color;
	struct snis_entity *shooter=0, *shootee=0;

	for (i = 0; i < nscience_guys; i++) {
		if (!science_guy[i].o)
			continue;
		if (science_guy[i].o->id == laserbeam->tsd.laserbeam.origin)
			shooter = science_guy[i].o;
		if (science_guy[i].o->id == laserbeam->tsd.laserbeam.target)
			shootee = science_guy[i].o;
		if (shooter && shootee)
			break;
	}

	if (laserbeam->tsd.laserbeam.origin == o->id)
		shooter = o;
	if (laserbeam->tsd.laserbeam.target == o->id)
		shootee = o;

	/* one end of the beam needs to be on the screen */
	if (!shooter && !shootee)
		return;

	if (!shooter) {
		shooter = lookup_entity_by_id(laserbeam->tsd.laserbeam.origin);
		if (!shooter)
			return;
	}
	if (!shootee) {
		shootee = lookup_entity_by_id(laserbeam->tsd.laserbeam.target);
		if (!shootee)
			return;
	}

	if (shooter->type == OBJTYPE_SHIP2)
		color = UI_COLOR(sci_plane_npc_laser);
	else
		color = UI_COLOR(sci_plane_player_laser);


	/* figure out where in 3d space the ends show up on sciplane */
	union vec3 ship_pos = {{o->x, o->y, o->z}};

	/* shooter position */
	double shooter_range, shooter_heading, shooter_mark;
	float shooter_tween = 0;
	union vec3 vshooter;

	union vec3 shooter_dir = {{shooter->x, shooter->y, shooter->z}};
	vec3_sub_self(&shooter_dir, &ship_pos);
	vec3_to_heading_mark(&shooter_dir, &shooter_range, &shooter_heading, &shooter_mark);
	tween_get_value(sciplane_tween, TWEEN_ACTIVE_OBJECT, shooter->id, &shooter_tween);
	heading_mark_to_vec3(shooter_range, shooter_heading, shooter_mark * shooter_tween, &vshooter);
	vec3_add_self(&vshooter, &ship_pos);

	/* shootee position */
	double shootee_range, shootee_heading, shootee_mark;
	float shootee_tween = 0;
	union vec3 vshootee;

	union vec3 shootee_dir = {{shootee->x, shootee->y, shootee->z}};
	vec3_sub_self(&shootee_dir, &ship_pos);
	vec3_to_heading_mark(&shootee_dir, &shootee_range, &shootee_heading, &shootee_mark);
	tween_get_value(sciplane_tween, TWEEN_ACTIVE_OBJECT, shootee->id, &shootee_tween);
	heading_mark_to_vec3(shootee_range, shootee_heading, shootee_mark * shootee_tween, &vshootee);
	vec3_add_self(&vshootee, &ship_pos);

	union vec3 clip1, clip2;
	rc = sphere_line_segment_intersection(&vshooter, &vshootee, &ship_pos, r, &clip1, &clip2);
	if (rc < 0)
		return;
	float sx1, sy1, sx2, sy2;
	if (!transform_line(cx, clip1.v.x, clip1.v.y, clip1.v.z, clip2.v.x, clip2.v.y, clip2.v.z, &sx1, &sy1, &sx2, &sy2)) {
		sng_draw_laser_line(sx1, sy1, sx2, sy2, color);
	}
}

static int science_tooltip_text(struct science_data *sd, char *buffer, int buflen)
{
	struct snis_entity *o = sd->o;

	if (sd->waypoint_index >= 0) {
		snprintf(buffer, buflen, "WAYPOINT %02d", sd->waypoint_index);
		return 1;
	}
	if (!o->sdata.science_data_known && o->type != OBJTYPE_PLANET && o->type != OBJTYPE_STARBASE)
		return 0;

	switch (o->type) {
	case OBJTYPE_SHIP1:
	case OBJTYPE_SHIP2:
		snprintf(buffer, buflen, "SHIP, TYPE: %s, NAME:%s",
				ship_type[o->sdata.subclass].class, o->sdata.name);
		break;
	case OBJTYPE_ASTEROID:
		snprintf(buffer, buflen, "ASTEROID, NAME:%s", o->sdata.name);
		break;
	case OBJTYPE_STARBASE:
		snprintf(buffer, buflen, "STAR BASE %s", o->sdata.name);
		break;
	case OBJTYPE_DEBRIS:
	case OBJTYPE_CARGO_CONTAINER:
		snprintf(buffer, buflen, "UNKNOWN");
		break;
	case OBJTYPE_SPARK:
		snprintf(buffer, buflen, "UNKNOWN ENERGY");
		break;
	case OBJTYPE_TORPEDO:
	case OBJTYPE_LASER:
		snprintf(buffer, buflen, "HIGH ENERGY PLASMA");
		break;
	case OBJTYPE_EXPLOSION:
		snprintf(buffer, buflen, "HIGH ENERGY");
		break;
	case OBJTYPE_NEBULA:
		snprintf(buffer, buflen, "GAS/FINE PARTICULATE MATTER");
		break;
	case OBJTYPE_WORMHOLE:
	case OBJTYPE_BLACK_HOLE:
		snprintf(buffer, buflen, "SPACE-TIME ANOMALY");
		break;
	case OBJTYPE_SPACEMONSTER:
		snprintf(buffer, buflen, "ORGANIC LIFE FORM");
		break;
	case OBJTYPE_PLANET: {
			struct planet_data *p = &o->tsd.planet;
			char *planet_type_str = solarsystem_assets->planet_type[p->solarsystem_planet_type];
			snprintf(buffer, buflen, "PLANET, TYPE:%s, NAME:%s", planet_type_str, o->sdata.name);
			uppercase(buffer);
		}
		break;
	case OBJTYPE_DERELICT:
	case OBJTYPE_BLOCK:
	case OBJTYPE_TURRET:
	case OBJTYPE_WARP_CORE:
	case OBJTYPE_MISSILE:
		snprintf(buffer, buflen, "METALLIC/INORGANIC MATTER");
		break;
	case OBJTYPE_TRACTORBEAM:
	case OBJTYPE_WARP_EFFECT:
	case OBJTYPE_LASERBEAM:
	case OBJTYPE_SHIELD_EFFECT:
	case OBJTYPE_FLARE:
		snprintf(buffer, buflen, "ENERGY");
		break;
	case OBJTYPE_WARPGATE:
		snprintf(buffer, buflen, "WARP GATE %s", o->sdata.name);
		break;
	case OBJTYPE_DOCKING_PORT: /* we don't have docking port objects, on the client they exist only as entities */
	default:
		snprintf(buffer, buflen, "UNKNOWN");
		break;
	}
	return 1;
}

static void draw_tooltip(int mousex, int mousey, char *tooltip);

static void draw_sciplane_display(GtkWidget *w, struct snis_entity *o, double range)
{
	static struct mesh *ring_mesh = 0;
	static struct mesh *heading_ind_line_mesh = 0;

	if (!ring_mesh) {
		ring_mesh = init_circle_mesh(0, 0, 1, 90, 2.0f * M_PI);
		heading_ind_line_mesh = init_line_mesh(0.9, 0, 0, 1, 0, 0);
	}

	static int last_timer = 0;
	int first_frame = (timer != last_timer+1);
	last_timer = timer;

	/* churn the mark tween values */
	tween_update(sciplane_tween);

	float fovy = 30.0 * M_PI / 180.0;
	float dist_to_cam_frac = 1.09 / tan(fovy/2.0); /*range + 8% for labels */

	float mark_popout_extra_dist = range * 0.2;
	float mark_popout_rate = 0.10;
	float mark_popout_zoom_dist_to_cam_frac = 0.4 / tan(fovy/2.0);

	struct entity *e = NULL;
	int science_style = RENDER_NORMAL;

	union vec3 ship_pos = {{o->x, o->y, o->z}};
	union vec3 ship_normal = {{0, 1, 0}};
	quat_rot_vec_self(&ship_normal, &o->orientation);

	/* figure out the location of curr selected target in real space and on sciplane */
	int selected_guy_popout = 0;
	float selected_guy_tween = 0;
	union vec3 selected_pos;
	union vec3 selected_m0_pos;
	if (curr_science_guy && curr_science_guy->sdata.science_data_known) {
		selected_guy_popout = 1;
		vec3_init(&selected_pos, curr_science_guy->x, curr_science_guy->y, curr_science_guy->z);

		/* take current position, find dir from ship to it, get heading/mark, recalc pos with mark=0 */
		double dist, heading, mark;
		union vec3 selected_dir;
		vec3_sub(&selected_dir, &selected_pos, &ship_pos);
		vec3_to_heading_mark(&selected_dir, &dist, &heading, &mark);
		heading_mark_to_vec3(dist, heading, 0, &selected_m0_pos);
		vec3_add_self(&selected_m0_pos, &ship_pos);

		tween_get_value(sciplane_tween, TWEEN_ACTIVE_OBJECT, curr_science_guy->id, &selected_guy_tween);
	} else if (curr_science_waypoint != (uint32_t) -1) {
		double *wp = &sci_ui.waypoint[curr_science_waypoint][0];
		selected_guy_popout = 1;
		vec3_init(&selected_pos, wp[0], wp[1], wp[2]);

		/* take current position, find dir from ship to it, get heading/mark, recalc pos with mark=0 */
		double dist, heading, mark;
		union vec3 selected_dir;
		vec3_sub(&selected_dir, &selected_pos, &ship_pos);
		vec3_to_heading_mark(&selected_dir, &dist, &heading, &mark);
		heading_mark_to_vec3(dist, heading, 0, &selected_m0_pos);
		vec3_add_self(&selected_m0_pos, &ship_pos);

		tween_get_value(sciplane_tween, TWEEN_ACTIVE_WAYPOINT, curr_science_waypoint, &selected_guy_tween);
	}

	/* cam orientation is locked with world */
	static union quat cam_orientation = IDENTITY_QUAT_INITIALIZER;
	static union vec3 camera_lookat = {{0,0,0}};
	static float cam_range_fraction = 0;

	/* tilt camera foward */
	union quat cam_orientation_selected;
	quat_init_axis(&cam_orientation_selected,1,0,0,degrees_to_radians(-30));
	union quat cam_orientation_not_selected;
	quat_init_axis(&cam_orientation_not_selected,1,0,0,degrees_to_radians(-60));

	union quat *desired_cam_orientation;
	union vec3 desired_lookat;
	float desired_cam_range_fraction;

	if (selected_guy_popout) {
		desired_cam_orientation = &cam_orientation_selected;
		desired_cam_range_fraction = mark_popout_zoom_dist_to_cam_frac;
		desired_lookat = selected_pos;
	} else {
		desired_cam_orientation = &cam_orientation_not_selected;
		desired_cam_range_fraction = dist_to_cam_frac;
		desired_lookat = ship_pos;
	}

	/* finish off rotating cam positions */

	if (first_frame) {
		cam_orientation = *desired_cam_orientation;
		cam_range_fraction = desired_cam_range_fraction;
		camera_lookat = desired_lookat;
	} else {
		quat_nlerp(&cam_orientation, &cam_orientation, desired_cam_orientation, 0.15 / (use_60_fps + 1));
		vec3_lerp(&camera_lookat, &camera_lookat, &desired_lookat, 0.15 / (use_60_fps + 1));
		cam_range_fraction = float_lerp(cam_range_fraction, desired_cam_range_fraction,
							0.15 / (use_60_fps + 1));
	}

	union vec3 camera_pos = {{0, 0, cam_range_fraction * range}};
	quat_rot_vec_self(&camera_pos, &cam_orientation);
	vec3_add_self(&camera_pos, &camera_lookat);

	union vec3 camera_up = {{0,1,0}};
	quat_rot_vec_self(&camera_up, &cam_orientation);

	camera_assign_up_direction(instrumentecx, camera_up.v.x, camera_up.v.y, camera_up.v.z);
	camera_set_pos(instrumentecx, camera_pos.v.x, camera_pos.v.y, camera_pos.v.z);
	camera_look_at(instrumentecx, camera_lookat.v.x, camera_lookat.v.y, camera_lookat.v.z);

	set_renderer(instrumentecx, WIREFRAME_RENDERER);
	camera_set_parameters(instrumentecx, range*(cam_range_fraction-1.0), range*(dist_to_cam_frac+1.0),
				SCREEN_WIDTH, SCREEN_HEIGHT, fovy);
	set_window_offset(instrumentecx, 0, 0);
	calculate_camera_transform(instrumentecx);
	entity_context_set_hi_lo_poly_pixel_threshold(instrumentecx, low_poly_threshold);

	e = add_entity(instrumentecx, ring_mesh, o->x, o->y, o->z, UI_COLOR(sci_plane_ring));
	if (e)
		update_entity_scale(e, range);

	add_basis_ring(instrumentecx, o->x, o->y, o->z, 1.0f, 0.0f, 0.0f, 0.0f, range * 0.98,
							UI_COLOR(sci_basis_ring_1));
	add_basis_ring(instrumentecx, o->x, o->y, o->z, 1.0f, 0.0f, 0.0f, 90.0f * M_PI / 180.0, range * 0.98,
							UI_COLOR(sci_basis_ring_2));
	add_basis_ring(instrumentecx, o->x, o->y, o->z, 0.0f, 0.0f, 1.0f, 90.0f * M_PI / 180.0, range * 0.98,
							UI_COLOR(sci_basis_ring_3));

	int i;
	for (i = 0; i < 2; ++i) {
		int color;
		union quat ind_orientation;
		if (i == 0) {
			color = UI_COLOR(sci_plane_ship_vector);
			ind_orientation = o->orientation;
		} else {
			color = UI_COLOR(sci_plane_weapon_vector);
			quat_mul(&ind_orientation, &o->orientation, &o->tsd.ship.weap_orientation);
		}

		/* add heading arrow */
		union vec3 ind_pos = {{range,0,0}};
		quat_rot_vec_self(&ind_pos, &ind_orientation);
		vec3_add_self(&ind_pos, &ship_pos);
		e = add_entity(instrumentecx, heading_indicator_mesh, ind_pos.v.x, ind_pos.v.y, ind_pos.v.z, color);
		if (e) {
			update_entity_scale(e, heading_indicator_mesh->radius*range/100.0);
			update_entity_orientation(e, &ind_orientation);
			set_render_style(e, RENDER_NORMAL);
		}

		/* heading arrow tail */
		e = add_entity(instrumentecx, heading_ind_line_mesh, o->x, o->y, o->z, color);
		if (e) {
			update_entity_scale(e, range);
			update_entity_orientation(e, &ind_orientation);
		}
	}

	/* heading labels */
	sng_set_foreground(UI_COLOR(sci_details_text));
	{
		int font = NANO_FONT;
		char buf[10];
		int i;
		const int slices = 24;

		for (i = 0; i < slices; i++) { /* 10 degree increments */
			float angle = i * 2.0 * M_PI / (float)slices;
			float x3, z3;
			float x1 = cos(angle) * range;
			float z1 = -sin(angle) * range;
			float x2 = x1 * 0.25;
			float z2 = z1 * 0.25;
			x3 = x1 * 1.08 + o->x;
			z3 = z1 * 1.08 + o->z;
			x1 += o->x;
			z1 += o->z;
			x2 += o->x;
			z2 += o->z;

			float sx1, sy1, sx2, sy2, sx3, sy3;
			if (!transform_line(instrumentecx, x1, o->y, z1, x2, o->y, z2, &sx1, &sy1, &sx2, &sy2)) {
				sng_draw_dotted_line(sx1, sy1, sx2, sy2);
			}
			if (!transform_point(instrumentecx, x3, o->y, z3, &sx3, &sy3)) {
				snprintf(buf, sizeof(buf), "%d",
					(int) math_angle_to_game_angle_degrees(i * 360.0/slices));
				sng_center_xy_draw_string(buf, font, sx3, sy3);
			}
		}
	}

	/* scan indicator */
	{
		float tx1, tz1, tx2, tz2, sx1, sy1, sx2, sy2;
		float heading = o->tsd.ship.sci_heading;
		float beam_width = fabs(o->tsd.ship.sci_beam_width);
		float jitter_offset = ((timer % 3) - 1) * M_PI / 180.0;
		float scanner_offset = (((timer % 16) / 15.0)) * beam_width;
		sng_set_foreground(UI_COLOR(sci_plane_beam));

		/* One side of the beam... */
		tx1 = o->x + cos(heading - beam_width / 2 - jitter_offset) * range * 0.05;
		tz1 = o->z - sin(heading - beam_width / 2 - jitter_offset) * range * 0.05;
		tx2 = o->x + cos(heading - beam_width / 2 - jitter_offset) * range;
		tz2 = o->z - sin(heading - beam_width / 2 - jitter_offset) * range;
		if (!transform_line(instrumentecx, tx1, o->y, tz1, tx2, o->y, tz2, &sx1, &sy1, &sx2, &sy2))
			sng_draw_electric_line(sx1, sy1, sx2, sy2);

		/* Central scanning beam... */
		tx1 = o->x + cos(heading - beam_width / 2 + scanner_offset) * range * 0.05;
		tz1 = o->z - sin(heading - beam_width / 2 + scanner_offset) * range * 0.05;
		tx2 = o->x + cos(heading - beam_width / 2 + scanner_offset) * range;
		tz2 = o->z - sin(heading - beam_width / 2 + scanner_offset) * range;
		if (!transform_line(instrumentecx, tx1, o->y, tz1, tx2, o->y, tz2, &sx1, &sy1, &sx2, &sy2))
			sng_draw_electric_line(sx1, sy1, sx2, sy2);

		/* The other side of the beam */
		tx1 = o->x + cos(heading + beam_width / 2 + jitter_offset) * range * 0.05;
		tz1 = o->z - sin(heading + beam_width / 2 + jitter_offset) * range * 0.05;
		tx2 = o->x + cos(heading + beam_width / 2 + jitter_offset) * range;
		tz2 = o->z - sin(heading + beam_width / 2 + jitter_offset) * range;
		if (!transform_line(instrumentecx, tx1, o->y, tz1, tx2, o->y, tz2, &sx1, &sy1, &sx2, &sy2))
			sng_draw_electric_line(sx1, sy1, sx2, sy2);
	}

	/* add my ship */
	e = add_entity(instrumentecx, ship_mesh_map[o->tsd.ship.shiptype], o->x, o->y, o->z, UI_COLOR(sci_plane_self));
	if (e) {
		set_render_style(e, science_style);
		update_entity_scale(e, range/300.0);
		update_entity_orientation(e, &o->orientation);
	}

	render_entities(instrumentecx);

	/* draw all the rest onto the 3d scene */
	{
		int i, pwr;
		double bw, dist2, dist;
		int selected_guy_still_visible = 0, prev_selected_guy_still_visible = 0;
		int nebula_factor = 0;
		int msx, msy;
		int closest_guy = -1;
		float closest_distance = -1;
		float mouse_dist;

		msx = sng_pixelx_to_screenx(mouse.x);
		msy = sng_pixely_to_screeny(mouse.y);

		pwr = o->tsd.ship.power_data.sensors.i;
		/* Draw all the stuff */

		bw = o->tsd.ship.sci_beam_width * 180.0 / M_PI;

		sng_set_foreground(UI_COLOR(sci_plane_default));
		pthread_mutex_lock(&universe_mutex);

		static int nlaserbeams = 0;
		static struct snis_entity* laserbeams[MAXGAMEOBJS] = {0};

		nlaserbeams = 0;
		nscience_guys = 0;
		for (i = 0; i <= snis_object_pool_highest_object(pool); i++) {
			if (!go[i].alive)
				continue;

			if (go[i].type == OBJTYPE_LASERBEAM || go[i].type == OBJTYPE_TRACTORBEAM) {
				laserbeams[nlaserbeams] = &go[i];
				nlaserbeams++;
				continue;
			}

			dist2 = object_dist2(&go[i], o);
			if (go[i].type == OBJTYPE_NEBULA) {
				if (dist2 < go[i].tsd.nebula.r * go[i].tsd.nebula.r)
					nebula_factor++;
				continue;
			}
			if (dist2 > range * range) {
				/* follow selected guy outside bounds */
				if (!(selected_guy_popout && &go[i] == curr_science_guy)) {
					continue; /* not close enough */
				}
			}
			dist = sqrt(dist2);

#if 0
			if (within_nebula(go[i].x, go[i].y))
				continue;
#endif
			union vec3 contact_pos = {{go[i].x, go[i].y, go[i].z}};

			union vec3 dir;
			vec3_sub(&dir, &contact_pos, &ship_pos);
			double heading, mark;
			vec3_to_heading_mark(&dir, 0, &heading, &mark);

			float tween = 0;
			int draw_popout_arc = 0;

			if (go[i].type == OBJTYPE_LASER || go[i].type == OBJTYPE_TORPEDO ||
				go[i].type == OBJTYPE_MISSILE || go[i].type == OBJTYPE_FLARE) {
				/* set projectile tween value to be the same as the popout if they pass in popout area */
				tween = selected_guy_tween;
				draw_popout_arc = 0;
			} else {
				/* get tween from bank and draw a popout arc */
				tween_get_value_and_decay(sciplane_tween, TWEEN_ACTIVE_OBJECT, go[i].id, &tween);
				draw_popout_arc = 1;
			}

			union vec3 display_pos;
			heading_mark_to_vec3(dist, heading, mark * tween, &display_pos);
			vec3_add_self(&display_pos, &ship_pos);

			if ( draw_popout_arc && tween > 0 ) {
				/* show the flyout arc */
				sng_set_foreground(UI_COLOR(sci_plane_popout_arc));
				draw_3d_mark_arc(w, gc, instrumentecx, &ship_pos, dist, heading, mark * tween * 0.9);
			}

			float sx, sy;
			if (!transform_point(instrumentecx, display_pos.v.x, display_pos.v.y, display_pos.v.z,
						&sx, &sy)) {
				if (go[i].id != my_ship_id) /* We already drew ourself */
					snis_draw_science_guy(w, gc, &go[i], sx, sy, dist, bw, pwr, range,
								&go[i] == curr_science_guy, nebula_factor);
			}

			if (go[i].sdata.science_data_known && selected_guy_popout) {
				int popout = 0;

				/* check if contact is close to selected in 3d space */
				double dist_to_selected = vec3_dist(&selected_pos, &contact_pos);
				if (dist_to_selected < mark_popout_extra_dist) {
					popout = 1;
				} else {
					/* check if contact is close to selected on sciplane */
					union vec3 contact_m0_pos;
					heading_mark_to_vec3(dist, heading, 0, &contact_m0_pos);
					vec3_add_self(&contact_m0_pos, &ship_pos);

					double dist_to_selected_m0 = vec3_dist(&selected_m0_pos, &contact_m0_pos);
					if (dist_to_selected_m0 < mark_popout_extra_dist) {
						popout = 1;
					}
				}

				if (popout) {
					/* start the mark tween to popout */
					tween_add_or_update(sciplane_tween, TWEEN_ACTIVE_OBJECT,
								go[i].id, 0, TWEEN_EXP_DECAY, mark_popout_rate, 0, 1);
				}
			}

			/* cache screen coords for mouse picking */
			science_guy[nscience_guys].o = &go[i];
			science_guy[nscience_guys].waypoint_index = -1;
			science_guy[nscience_guys].sx = sx;
			science_guy[nscience_guys].sy = sy;
			mouse_dist = (msx - sx) * (msx - sx) + (msy - sy) * (msy - sy);
			if (mouse_dist < closest_distance || closest_distance < 0) {
				closest_distance = mouse_dist;
				closest_guy = nscience_guys;
			}
			if (&go[i] == curr_science_guy) {
				selected_guy_still_visible = 1;
			}
			if (prev_science_guy == &go[i]) {
				prev_selected_guy_still_visible = 1;
			}
			nscience_guys++;
		}

		if (prev_science_guy && !selected_guy_still_visible && prev_selected_guy_still_visible) {
			/* If we moved the beam off our guy, and back on, select him again. */
			curr_science_guy = prev_science_guy;
			prev_science_guy = NULL;
		} else if (!selected_guy_still_visible && curr_science_guy) {
			prev_science_guy = curr_science_guy;
			curr_science_guy = NULL;
		}

		/* Draw the waypoints */
		for (i = 0; i < sci_ui.nwaypoints; i++) {
			double *wp = &sci_ui.waypoint[i][0];
			dist2 = dist3dsqrd(wp[0] - o->x, wp[1] - o->y, wp[2] - o->z);

			if (dist2 > range * range) {
				/* follow selected guy outside bounds */
				if (!(selected_guy_popout && curr_science_waypoint == i)) {
					continue; /* not close enough */
				}
			}
			dist = sqrt(dist2);

			union vec3 contact_pos = { { wp[0], wp[1], wp[2] } };
			union vec3 dir;
			double heading, mark;
			float tween = 0;
			int draw_popout_arc = 0;

			vec3_sub(&dir, &contact_pos, &ship_pos);
			vec3_to_heading_mark(&dir, 0, &heading, &mark);

			/* get tween from bank and draw a popout arc */
			tween_get_value_and_decay(sciplane_tween, TWEEN_ACTIVE_WAYPOINT, i, &tween);
			draw_popout_arc = 1;

			union vec3 display_pos;
			heading_mark_to_vec3(dist, heading, mark * tween, &display_pos);
			vec3_add_self(&display_pos, &ship_pos);

			if (draw_popout_arc && tween > 0) { /* show the flyout arc */
				sng_set_foreground(UI_COLOR(sci_plane_popout_arc));
				draw_3d_mark_arc(w, gc, instrumentecx, &ship_pos, dist, heading, mark * tween * 0.9);
			}

			float sx, sy;
			if (!transform_point(instrumentecx, display_pos.v.x, display_pos.v.y, display_pos.v.z,
						&sx, &sy)) {
				char buf[20];

				sng_set_foreground(UI_COLOR(sci_waypoint));
				snis_draw_line(sx - 10, sy, sx + 10, sy);
				snis_draw_line(sx, sy - 10, sx, sy + 10);
				snprintf(buf, sizeof(buf), "WP-%02d", i);
				sng_abs_xy_draw_string(buf, PICO_FONT, sx + 10, sy - 10);
				if (i == curr_science_waypoint)
					sng_draw_circle(0, sx, sy, 10);
			}

			if (selected_guy_popout) {
				int popout = 0;

				/* check if contact is close to selected in 3d space */
				double dist_to_selected = vec3_dist(&selected_pos, &contact_pos);
				if (dist_to_selected < mark_popout_extra_dist) {
					popout = 1;
				} else {
					/* check if contact is close to selected on sciplane */
					union vec3 contact_m0_pos;
					heading_mark_to_vec3(dist, heading, 0, &contact_m0_pos);
					vec3_add_self(&contact_m0_pos, &ship_pos);

					double dist_to_selected_m0 = vec3_dist(&selected_m0_pos, &contact_m0_pos);
					if (dist_to_selected_m0 < mark_popout_extra_dist) {
						popout = 1;
					}
				}

				if (popout) {
					/* start the mark tween to popout */
					tween_add_or_update(sciplane_tween, TWEEN_ACTIVE_WAYPOINT,
								i, 0, TWEEN_EXP_DECAY, mark_popout_rate, 0, 1);
				}
			}

			/* cache screen coords for mouse picking */
			science_guy[nscience_guys].o = NULL;
			science_guy[nscience_guys].waypoint_index = i;
			science_guy[nscience_guys].sx = sx;
			science_guy[nscience_guys].sy = sy;
			mouse_dist = (msx - sx) * (msx - sx) + (msy - sy) * (msy - sy);
			if (mouse_dist < closest_distance || closest_distance < 0) {
				closest_distance = mouse_dist;
				closest_guy = nscience_guys;
			}
			nscience_guys++;
		}

		/* Draw tool-tip like help for thing nearest mouse pointer */
		if (closest_guy >= 0 && closest_distance > 0 && closest_distance < 400) {
			char scitooltip[100];
			if (science_tooltip_text(&science_guy[closest_guy], scitooltip, sizeof(scitooltip) - 1))
				draw_tooltip(mouse.x, mouse.y, scitooltip);
		}

		/* draw in the laserbeams */
		for (i = 0; i < nlaserbeams; i++) {
			draw_sciplane_laserbeam(w, gc, instrumentecx, o, laserbeams[i], range);
		}

		pthread_mutex_unlock(&universe_mutex);

		if (nebula_factor) {
			for (i = 0; i < 300; i++) {
				double angle;
				double radius;

				angle = 360.0 * ((double) snis_randn(10000) / 10000.0) * M_PI / 180.0;
				radius = (snis_randn(1000) / 1000.0) / 2.0;
				radius = 1.0 - (radius * radius * radius);
				radius = radius * range;
				radius = radius - ((range / MAX_SCIENCE_SCREEN_RADIUS)  * (snis_randn(50) / 75.0) * range);

				float x1 = o->x + cos(angle) * range;
				float y1 = o->y;
				float z1 = o->z - sin(angle) * range;
				float x2 = o->x + cos(angle) * radius;
				float y2 = o->y;
				float z2 = o->z - sin(angle) * radius;

				float sx1, sy1, sx2, sy2;
				if (!transform_line(instrumentecx, x1, y1, z1, x2, y2, z2, &sx1, &sy1, &sx2, &sy2)) {
					snis_draw_line(sx1, sy1, sx2, sy2);
				}
			}
		}
	}

	remove_all_entity(instrumentecx);
}

static void add_basis_ring(struct entity_context *ecx, float x, float y, float z,
		float ax, float ay, float az, float angle, float r, int color)
{
	union quat q;
	static struct mesh *ring_mesh = 0;
	struct entity *e;

	if (!ring_mesh) {
		ring_mesh = init_circle_mesh(0, 0, 1, 60, 2.0f * M_PI);
		ring_mesh->geometry_mode = MESH_GEOMETRY_POINTS;
	}

	quat_init_axis(&q, ax, ay, az, angle);
	e = add_entity(ecx, ring_mesh, x, y, z, color);
	if (e) {
		update_entity_scale(e, r);
		update_entity_orientation(e, &q);
	}
}

static void add_scanner_beam_orange_slice(struct entity_context *ecx,
					struct snis_entity *o, float r, int color)
{
	static struct mesh *orange_slice = 0;
	struct entity *e;
	union quat q, q1, q2, q3, q4;
	float beam_width = fabs(o->tsd.ship.sci_beam_width);
	float jitter_offset = ((timer % 3) - 1) * M_PI / 360.0;
	float scanner_offset = (((timer % 16) / 15.0)) * beam_width;

	if (!orange_slice) {
		orange_slice = init_circle_mesh(0, 0, 1, 60, M_PI); /* half circle in x-z plane */
		orange_slice->geometry_mode = MESH_GEOMETRY_POINTS;
	}

	quat_init_axis(&q, 0.0f, 0.0f, 1.0f, -M_PI / 2.0); /* rotate 90 degrees around z axis */
	quat_init_axis(&q1, 0.0f, 1.0f, 0.0f,
			o->tsd.ship.sci_heading - o->tsd.ship.sci_beam_width / 2.0 + M_PI / 2.0 - jitter_offset);
	quat_init_axis(&q2, 0.0f, 1.0f, 0.0f,
			o->tsd.ship.sci_heading + o->tsd.ship.sci_beam_width / 2.0 + M_PI / 2.0 + jitter_offset);
	quat_init_axis(&q3, 0.0f, 1.0f, 0.0f,
			o->tsd.ship.sci_heading + o->tsd.ship.sci_beam_width / 2.0 + M_PI / 2.0 - scanner_offset);
	e = add_entity(sciballecx, orange_slice, o->x, o->y, o->z, color);
	if (e) {
		quat_mul(&q4, &q1, &q);
		update_entity_orientation(e, &q4);
		update_entity_scale(e, r);
	}

	e = add_entity(sciballecx, orange_slice, o->x, o->y, o->z, color);
	if (e) {
		quat_mul(&q4, &q2, &q);
		update_entity_orientation(e, &q4);
		update_entity_scale(e, r);
	}
	e = add_entity(sciballecx, orange_slice, o->x, o->y, o->z, color);
	if (e) {
		quat_mul(&q4, &q3, &q);
		update_entity_orientation(e, &q4);
		update_entity_scale(e, r);
	}
}

static void draw_science_3d_waypoints(struct snis_entity *o, double range, float *closest_distance, int *closest_guy)
{
	int i;
	int msx = sng_pixelx_to_screenx(mouse.x);
	int msy = sng_pixely_to_screeny(mouse.y);
	float mouse_dist = -1.0;

	sng_set_foreground(UI_COLOR(sci_waypoint));
	for (i = 0; i < sci_ui.nwaypoints; i++) {
		double *wp = &sci_ui.waypoint[i][0];
		float sx, sy;
		char buf[10];
		if (dist3d(wp[0] - o->x, wp[1] - o->y, wp[2] - o->z) >= range)
			continue;
		transform_point(sciballecx, wp[0], wp[1], wp[2], &sx, &sy);
		snis_draw_line(sx - 10, sy, sx + 10, sy);
		snis_draw_line(sx, sy - 10, sx, sy + 10);
		snprintf(buf, sizeof(buf), "WP-%02d", i);
		sng_abs_xy_draw_string(buf, PICO_FONT, sx + 10, sy - 10);
		/* cache screen coords for mouse picking */
		science_guy[nscience_guys].o = NULL;
		science_guy[nscience_guys].waypoint_index = i;
		science_guy[nscience_guys].sx = sx;
		science_guy[nscience_guys].sy = sy;
		mouse_dist = (msx - sx) * (msx - sx) + (msy - sy) * (msy - sy);
		if (mouse_dist < *closest_distance || *closest_distance < 0) {
			*closest_distance = mouse_dist;
			*closest_guy = nscience_guys;
		}
		nscience_guys++;
		if (i == curr_science_waypoint)
			sng_draw_circle(0, sx, sy, 10);
	}
}

static void draw_all_the_3d_science_guys(GtkWidget *w, struct snis_entity *o, double range, double current_zoom)
{
	int i, x, y, cx, cy, r, bw, pwr;
	double tx, ty, dist2, dist;
	int selected_guy_still_visible = 0;
	int nebula_factor = 0;
	union vec3 ship_pos = { { o->x, o->y, o->z } };
	union vec3 ship_up = { { 0, 1, 0 } };
	double screen_radius = ((((current_zoom) / 255.0) * 0.08) + 0.01) * XKNOWN_DIM * 3;
	union vec3 camera_pos = { { -screen_radius * 1.80, screen_radius * 0.85, 0} };
	float camera_pos_len = vec3_magnitude(&camera_pos);
	quat_rot_vec_self(&ship_up, &o->tsd.ship.sciball_orientation);
	float closest_distance = -1;
	int closest_guy = -1;
	float mouse_dist;
	int msx = sng_pixelx_to_screenx(mouse.x);
	int msy = sng_pixely_to_screeny(mouse.y);

	/* rotate camera to be behind my ship */
	quat_rot_vec_self(&camera_pos, &o->tsd.ship.sciball_orientation);
	vec3_add_self(&camera_pos, &ship_pos);
        set_renderer(sciballecx, WIREFRAME_RENDERER);
	camera_assign_up_direction(sciballecx, ship_up.v.x, ship_up.v.y, ship_up.v.z);
	camera_set_pos(sciballecx, camera_pos.v.x, camera_pos.v.y, camera_pos.v.z);
	camera_look_at(sciballecx, o->x, o->y, o->z);
	camera_set_parameters(sciballecx, camera_pos_len-screen_radius, camera_pos_len+screen_radius,
				SCIENCE_SCOPE_W * 0.95, SCIENCE_SCOPE_W * 0.95,
				60.0 * M_PI / 180.0);
	set_window_offset(sciballecx, SCIENCE_SCOPE_CX - (SCIENCE_SCOPE_W * 0.95) / 2.0,
				SCIENCE_SCOPE_CY - (SCIENCE_SCOPE_W * 0.95) / 2.0);
	calculate_camera_transform(sciballecx);
	entity_context_set_hi_lo_poly_pixel_threshold(sciballecx, low_poly_threshold);

	/* Add basis rings */
	for (i = 1; i <= 3; i++) {
		add_basis_ring(sciballecx, o->x, o->y, o->z, 1.0f, 0.0f, 0.0f, 0.0f,
					screen_radius * (float) i / 3.0f, UI_COLOR(sci_basis_ring_1));
		add_basis_ring(sciballecx, o->x, o->y, o->z, 1.0f, 0.0f, 0.0f, 90.0f * M_PI / 180.0,
					screen_radius * (float) i / 3.0f, UI_COLOR(sci_basis_ring_2));
		add_basis_ring(sciballecx, o->x, o->y, o->z, 0.0f, 0.0f, 1.0f, 90.0f * M_PI / 180.0,
					screen_radius * (float) i / 3.0f, UI_COLOR(sci_basis_ring_3));
	}

	add_scanner_beam_orange_slice(sciballecx, o, screen_radius, UI_COLOR(sci_ball_beam));

	/* Draw player ship and axes */
	{
		union vec3 vx = { { 1.0, 0.0, 0.0 } };
		union vec3 vy = { { 0.0, 1.0, 0.0 } };
		union vec3 vz = { { 0.0, 0.0, 1.0 } };
		float sx, sy;
		struct entity *e;
		quat_rot_vec_self(&vx, &o->orientation);
		quat_rot_vec_self(&vy, &o->orientation);
		quat_rot_vec_self(&vz, &o->orientation);
		vec3_mul_self(&vx, screen_radius * 0.8);
		vec3_mul_self(&vy, screen_radius * 0.8);
		vec3_mul_self(&vz, screen_radius * 0.8);
		sng_set_foreground(UI_COLOR(sci_ball_player));
		snis_draw_3d_line(w, gc, sciballecx, o->x - vx.v.x, o->y - vx.v.y, o->z - vx.v.z,
							o->x + vx.v.x, o->y + vx.v.y, o->z + vx.v.z);
		snis_draw_3d_line(w, gc, sciballecx, o->x - vy.v.x, o->y - vy.v.y, o->z - vy.v.z,
							o->x + vy.v.x, o->y + vy.v.y, o->z + vy.v.z);
		snis_draw_3d_line(w, gc, sciballecx, o->x - vz.v.x, o->y - vz.v.y, o->z - vz.v.z,
							o->x + vz.v.x, o->y + vz.v.y, o->z + vz.v.z);

		e = add_entity(sciballecx, ship_mesh_map[o->tsd.ship.shiptype], o->x, o->y, o->z,
				UI_COLOR(sci_ball_player));
		update_entity_orientation(e, &o->orientation);
		update_entity_scale(e, screen_radius * 0.01);

		transform_point(sciballecx, o->x - vx.v.x, o->y - vx.v.y, o->z - vx.v.z, &sx, &sy);
		sng_abs_xy_draw_string("AFT", PICO_FONT, sx, sy);
		transform_point(sciballecx, o->x + vx.v.x, o->y + vx.v.y, o->z + vx.v.z, &sx, &sy);
		sng_abs_xy_draw_string("FORE", PICO_FONT, sx, sy);
		transform_point(sciballecx, o->x - vy.v.x, o->y - vy.v.y, o->z - vy.v.z, &sx, &sy);
		sng_abs_xy_draw_string("DOWN", PICO_FONT, sx, sy);
		transform_point(sciballecx, o->x + vy.v.x, o->y + vy.v.y, o->z + vy.v.z, &sx, &sy);
		sng_abs_xy_draw_string("UP", PICO_FONT, sx, sy);
		transform_point(sciballecx, o->x - vz.v.x, o->y - vz.v.y, o->z - vz.v.z, &sx, &sy);
		sng_abs_xy_draw_string("PORT", PICO_FONT, sx, sy);
		transform_point(sciballecx, o->x + vz.v.x, o->y + vz.v.y, o->z + vz.v.z, &sx, &sy);
		sng_abs_xy_draw_string("STBD", PICO_FONT, sx, sy);
	}

	cx = SCIENCE_SCOPE_CX;
	cy = SCIENCE_SCOPE_CY;
	r = SCIENCE_SCOPE_R;
	pwr = o->tsd.ship.power_data.sensors.i;
	/* Draw all the stuff */

	sng_set_foreground(UI_COLOR(sci_ball_default_blip));
	pthread_mutex_lock(&universe_mutex);
	nscience_guys = 0;
	for (i = 0; i <= snis_object_pool_highest_object(pool); i++) {
		if (!go[i].alive)
			continue;
#if 0
		if (go[i].type == OBJTYPE_LASERBEAM || go[i].type == OBJTYPE_TRACTORBEAM) {
			draw_science_laserbeam(w, gc, o, &go[i], cx, cy, r, range);
			continue;
		}
#endif

		dist2 = object_dist2(&go[i], o);
		if (go[i].type == OBJTYPE_NEBULA) {
			if (dist2 < go[i].tsd.nebula.r * go[i].tsd.nebula.r)
				nebula_factor++;
			continue;
		}
#if 0
		if (dist2 > range * range)
			continue; /* not close enough */
#endif
		dist = sqrt(dist2);

#if 0
		if (within_nebula(go[i].x, go[i].y))
			continue;
#endif

		tx = (go[i].x - o->x) * (double) r / range;
		ty = (go[i].z - o->z) * (double) r / range;
		x = (int) (tx + (double) cx);
		y = (int) (ty + (double) cy);

		if (go[i].id == my_ship_id)
			continue; /* skip drawing yourself. */
		bw = o->tsd.ship.sci_beam_width * 180.0 / M_PI;

		/* If we moved the beam off our guy, and back on, select him again. */
		if (!curr_science_guy && prev_science_guy == &go[i])
			curr_science_guy = prev_science_guy;

		if (dist < range || go[i].type == OBJTYPE_PLANET ||
					go[i].type == OBJTYPE_NEBULA ||
					go[i].type == OBJTYPE_WARPGATE ||
					go[i].type == OBJTYPE_STARBASE ||
					go[i].type == OBJTYPE_BLACK_HOLE)
			snis_draw_3d_science_guy(w, gc, &go[i], &x, &y, dist, bw, pwr, range,
				&go[i] == curr_science_guy, 100.0 * current_zoom / 255.0, nebula_factor);

		/* cache screen coords for mouse picking */
		science_guy[nscience_guys].o = &go[i];
		science_guy[nscience_guys].waypoint_index = -1;
		science_guy[nscience_guys].sx = x;
		science_guy[nscience_guys].sy = y;
		mouse_dist = (msx - x) * (msx - x) + (msy - y) * (msy - y);
		if (mouse_dist < closest_distance || closest_distance < 0) {
			closest_distance = mouse_dist;
			closest_guy = nscience_guys;
		}
		if (&go[i] == curr_science_guy)
			selected_guy_still_visible = 1;
		nscience_guys++;
	}
	if (!selected_guy_still_visible && curr_science_guy) {
		prev_science_guy = curr_science_guy;
		curr_science_guy = NULL;
	}
	draw_science_3d_waypoints(o, range, &closest_distance, &closest_guy);

	/* Draw tool-tip like help for thing nearest mouse pointer */
	if (closest_guy >= 0 && closest_distance > 0 && closest_distance < 400) {
		char scitooltip[100];
		if (science_tooltip_text(&science_guy[closest_guy], scitooltip, sizeof(scitooltip) - 1))
			draw_tooltip(mouse.x, mouse.y, scitooltip);
	}

	pthread_mutex_unlock(&universe_mutex);
	sng_set_foreground(UI_COLOR(sci_ball_default_blip));
	if (nebula_factor) {
		for (i = 0; i < 300; i++) {
			double angle;
			double radius;

			angle = 360.0 * ((double) snis_randn(10000) / 10000.0) * M_PI / 180.0;
			radius = (snis_randn(1000) / 1000.0) / 2.0;
			radius = 1.0 - (radius * radius * radius);
			radius = radius * SCIENCE_SCOPE_R;
			radius = radius - ((range / MAX_SCIENCE_SCREEN_RADIUS)  * (snis_randn(50) / 75.0) * r);
			snis_draw_line(
				cx + cos(angle) * r,
				cy + sin(angle) * r,
				cx + cos(angle) * radius,
				cy + sin(angle) * radius);
		}
	}
	render_entities(sciballecx);
	remove_all_entity(sciballecx);
}

static void snis_draw_dotted_hline(GdkDrawable *drawable,
         GdkGC *gc, int x1, int y1, int x2, int dots)
{
	int i;

	for (i = x1; i <= x2; i += dots)
		sng_draw_point(i, y1);
}

static void snis_draw_dotted_vline(GdkDrawable *drawable,
         GdkGC *gc, int x1, int y1, int y2, int dots)
{
	int i;

	if (y2 > y1) {
		for (i = y1; i <= y2; i += dots)
			sng_draw_point(x1, i);
	} else { 
		for (i = y2; i <= y1; i += dots)
			sng_draw_point(x1, i);
	}
}

static void load_torpedo_button_pressed()
{
	struct snis_entity *o;

	if (!(o = find_my_ship()))
		return;
	if (o->tsd.ship.torpedoes <= 0)
		return;
	if (o->tsd.ship.torpedoes_loaded >= 2)
		return;
	if (o->tsd.ship.torpedoes_loading != 0)
		return;
	queue_to_server(snis_opcode_pkt("b", OPCODE_LOAD_TORPEDO));
}

static void fire_phaser_button_pressed(__attribute__((unused)) void *notused)
{
	do_laser();
}

static void fire_torpedo_button_pressed(__attribute__((unused)) void *notused)
{
	do_torpedo();
	load_torpedo_button_pressed();
}

static void transmit_adjust_control_input(uint8_t value,  uint8_t subcode)
{
	struct snis_entity *o = find_my_ship();

	if (!o)
		return;
	queue_to_server(snis_opcode_subcode_pkt("bbwb",
					OPCODE_ADJUST_CONTROL_INPUT,
					subcode, o->id, value));
}

static void do_adjust_control_input(struct slider *s, uint8_t subcode)
{
	uint8_t value = (uint8_t) (255.0 * snis_slider_get_input(s));
	transmit_adjust_control_input(value, subcode);
}

static void transmit_apply_preset(uint8_t preset)
{
	struct snis_entity *o = find_my_ship();

	if (!o)
		return;
	queue_to_server(snis_opcode_pkt("bwb", OPCODE_APPLY_ENGINEERING_PRESET,
					o->id, preset));
}

static void transmit_save_preset(uint8_t preset)
{
	struct snis_entity *o = find_my_ship();

	if (!o)
		return;
	queue_to_server(snis_opcode_pkt("bwb", OPCODE_SAVE_ENGINEERING_PRESET,
					o->id, preset));
}

static void do_adjust_byte_value(uint8_t value,  uint8_t opcode)
{
	struct snis_entity *o;

	if (!(o = find_my_ship()))
		return;
	queue_to_server(snis_opcode_pkt("bwb", opcode, o->id, value));
}

static void do_navzoom(struct slider *s)
{
	do_adjust_control_input(s, OPCODE_ADJUST_CONTROL_NAVZOOM);
}

static void do_mainzoom(struct slider *s)
{
	do_adjust_control_input(s, OPCODE_ADJUST_CONTROL_MAINZOOM);
}

static void do_throttle(struct slider *s)
{
	do_adjust_control_input(s, OPCODE_ADJUST_CONTROL_THROTTLE);
}

static void do_scizoom(struct slider *s)
{
	do_adjust_control_input(s, OPCODE_ADJUST_CONTROL_SCIZOOM);
}
	
static void do_warpdrive(struct slider *s)
{
	do_adjust_control_input(s, OPCODE_ADJUST_CONTROL_WARPDRIVE);
}

static void do_shieldadj(struct slider *s)
{
	do_adjust_control_input(s, OPCODE_ADJUST_CONTROL_SHIELD);
}

static void do_maneuvering_pwr(struct slider *s)
{
	do_adjust_control_input(s, OPCODE_ADJUST_CONTROL_MANEUVERING_PWR);
}
	
static void do_tractor_pwr(struct slider *s)
{
	do_adjust_control_input(s, OPCODE_ADJUST_CONTROL_TRACTOR_PWR);
}

static void do_lifesupport_pwr(struct slider *s)
{
	do_adjust_control_input(s, OPCODE_ADJUST_CONTROL_LIFESUPPORT_PWR);
}

static void do_shields_pwr(struct slider *s)
{
	do_adjust_control_input(s, OPCODE_ADJUST_CONTROL_SHIELDS_PWR);
}
	
static void do_impulse_pwr(struct slider *s)
{
	do_adjust_control_input(s, OPCODE_ADJUST_CONTROL_IMPULSE_PWR);
}

static void do_warp_pwr(struct slider *s)
{
	do_adjust_control_input(s, OPCODE_ADJUST_CONTROL_WARP_PWR);
}

static void do_sensors_pwr(struct slider *s)
{
	do_adjust_control_input(s, OPCODE_ADJUST_CONTROL_SENSORS_PWR);
}
	
static void do_phaserbanks_pwr(struct slider *s)
{
	do_adjust_control_input(s, OPCODE_ADJUST_CONTROL_PHASERBANKS_PWR);
}
	
static void do_comms_pwr(struct slider *s)
{
	do_adjust_control_input(s, OPCODE_ADJUST_CONTROL_COMMS_PWR);
}

static void do_maneuvering_coolant(struct slider *s)
{
	do_adjust_control_input(s, OPCODE_ADJUST_CONTROL_MANEUVERING_COOLANT);
}
	
static void do_tractor_coolant(struct slider *s)
{
	do_adjust_control_input(s, OPCODE_ADJUST_CONTROL_TRACTOR_COOLANT);
}

static void do_lifesupport_coolant(struct slider *s)
{
	do_adjust_control_input(s, OPCODE_ADJUST_CONTROL_LIFESUPPORT_COOLANT);
}

static void do_shields_coolant(struct slider *s)
{
	do_adjust_control_input(s, OPCODE_ADJUST_CONTROL_SHIELDS_COOLANT);
}
	
static void do_impulse_coolant(struct slider *s)
{
	do_adjust_control_input(s, OPCODE_ADJUST_CONTROL_IMPULSE_COOLANT);
}

static void do_warp_coolant(struct slider *s)
{
	do_adjust_control_input(s, OPCODE_ADJUST_CONTROL_WARP_COOLANT);
}

static void do_sensors_coolant(struct slider *s)
{
	do_adjust_control_input(s, OPCODE_ADJUST_CONTROL_SENSORS_COOLANT);
}
	
static void do_phaserbanks_coolant(struct slider *s)
{
	do_adjust_control_input(s, OPCODE_ADJUST_CONTROL_PHASERBANKS_COOLANT);
}
	
static void do_comms_coolant(struct slider *s)
{
	do_adjust_control_input(s, OPCODE_ADJUST_CONTROL_COMMS_COOLANT);
}

#define DEFINE_SAMPLER_FUNCTION(f, field, divisor, min) \
static double f(void) \
{ \
	struct snis_entity *o; \
\
	if (!(o = find_my_ship())) \
		return 0.0; \
	return (double) 100.0 * o->field / (divisor) + (min); \
}

DEFINE_SAMPLER_FUNCTION(sample_temp, tsd.ship.temp, UINT8_MAX, 0)
DEFINE_SAMPLER_FUNCTION(sample_warpdrive, tsd.ship.warpdrive, 10.0 * 255.0, 0)
DEFINE_SAMPLER_FUNCTION(sample_scizoom, tsd.ship.scizoom, 255.0, 0)
DEFINE_SAMPLER_FUNCTION(sample_fuel, tsd.ship.fuel, UINT32_MAX, 0)
DEFINE_SAMPLER_FUNCTION(sample_oxygen, tsd.ship.oxygen, UINT32_MAX, 0)
DEFINE_SAMPLER_FUNCTION(sample_phasercharge, tsd.ship.phaser_charge, 255.0, 0)
DEFINE_SAMPLER_FUNCTION(sample_phaser_wavelength, tsd.ship.phaser_wavelength, 255.0 * 2.0, 10.0)
DEFINE_SAMPLER_FUNCTION(sample_navzoom, tsd.ship.navzoom, 255.0, 0.0)
DEFINE_SAMPLER_FUNCTION(sample_mainzoom, tsd.ship.mainzoom, 255.0, 0.0)

static double sample_main_base_health(void)
{
	int i;
	struct snis_entity *o;

	o = find_my_ship();
	if (!o)
		return 0.0;
	for (i = 0; i < ARRAYSIZE(rts_planet); i++) {
		if (o->sdata.faction == rts_planet[i].faction)
			return (double) rts_planet[i].health;
	}
	return 0.0;
}

static double sample_enemy_base_health(void)
{
	int i;
	struct snis_entity *o;

	o = find_my_ship();
	if (!o)
		return 0.0;
	for (i = 0; i < ARRAYSIZE(rts_planet); i++) {
		if (o->sdata.faction != rts_planet[i].faction)
			return (double) rts_planet[i].health;
	}
	return 0.0;
}

static double sample_ship_velocity(void)
{
	struct snis_entity *o;
	static double lastx = 0.0;
	static double lasty = 0.0;
	static double lastz = 0.0;
#define VELOCITY_INDICATOR_SAMPLES 5
	static double lastdist[VELOCITY_INDICATOR_SAMPLES] = { 0.0 };
	double vx, vy, vz;
	double dist;
	int i;

	if (!(o = find_my_ship()))
		return 0.0;
	vx = o->x - lastx;
	vy = o->y - lasty;
	vz = o->z - lastz;

	lastx = o->x;
	lasty = o->y;
	lastz = o->z;

	dist = frame_rate_hz * dist3d(vx, vy, vz);
	if (dist > 10000)
		dist = 0;
	memmove(&lastdist[0], &lastdist[1], (VELOCITY_INDICATOR_SAMPLES - 1) * sizeof(lastdist[0]));
	lastdist[VELOCITY_INDICATOR_SAMPLES - 1] = dist;
	for (i = 0; i < VELOCITY_INDICATOR_SAMPLES - 1; i++)
		dist += lastdist[i];
	dist = dist / (float) VELOCITY_INDICATOR_SAMPLES; /* smooth it */
	return dist;
}

static double sample_phaser_power(void)
{
	struct snis_entity *o;

	if (!(o = find_my_ship()))
		return 0.0;
	return 100.0 * (double) o->tsd.ship.power_data.phasers.i / 255.0;
}

static double sample_sensors_power(void)
{
	struct snis_entity *o;

	if (!(o = find_my_ship()))
		return 0.0;
	return 100.0 * (double) o->tsd.ship.power_data.sensors.i / 255.0;
}

static double sample_power_model_voltage(void)
{
	struct snis_entity *o;

	if (!(o = find_my_ship()))
		return 0.0;

	return 100.0 * o->tsd.ship.power_data.voltage / 255.0;
}

static double __attribute__((unused)) sample_coolant_model_voltage(void)
{
	struct snis_entity *o;

	if (!(o = find_my_ship()))
		return 0.0;

	return 100.0 * o->tsd.ship.coolant_data.voltage / 255.0;
}

static double sample_power_model_current(void)
{
	struct snis_entity *o;
	int total_current = 0;

	if (!(o = find_my_ship()))
		return 0.0;

	total_current += o->tsd.ship.power_data.maneuvering.i;
	total_current += o->tsd.ship.power_data.warp.i;
	total_current += o->tsd.ship.power_data.impulse.i;
	total_current += o->tsd.ship.power_data.sensors.i;
	total_current += o->tsd.ship.power_data.comms.i;
	total_current += o->tsd.ship.power_data.phasers.i;
	total_current += o->tsd.ship.power_data.shields.i;
	total_current += o->tsd.ship.power_data.tractor.i;
	total_current += o->tsd.ship.power_data.lifesupport.i;

	return 100.0 * total_current / (255.0 * 3.5); 
}

static double __attribute__((unused)) sample_coolant_model_current(void)
{
	struct snis_entity *o;
	int total_current = 0;

	if (!(o = find_my_ship()))
		return 0.0;

	total_current += o->tsd.ship.coolant_data.maneuvering.i;
	total_current += o->tsd.ship.coolant_data.warp.i;
	total_current += o->tsd.ship.coolant_data.impulse.i;
	total_current += o->tsd.ship.coolant_data.sensors.i;
	total_current += o->tsd.ship.coolant_data.comms.i;
	total_current += o->tsd.ship.coolant_data.phasers.i;
	total_current += o->tsd.ship.coolant_data.shields.i;
	total_current += o->tsd.ship.coolant_data.tractor.i;
	total_current += o->tsd.ship.coolant_data.lifesupport.i;

	return 100.0 * total_current / (255.0 * 3.5); 
}

#define DEFINE_CURRENT_SAMPLER(system, name) \
static double sample_##system##_##name##_current(void) \
{ \
	struct snis_entity *o; \
\
	if (!(o = find_my_ship())) \
		return 0.0; \
	return o->tsd.ship.system.name.i; \
}

DEFINE_CURRENT_SAMPLER(power_data, warp) /* defines sample_power_data_warp_current */
DEFINE_CURRENT_SAMPLER(power_data, sensors) /* defines sample_power_data_sensors_current */
DEFINE_CURRENT_SAMPLER(power_data, phasers) /* defines sample_power_data_phasers_current */
DEFINE_CURRENT_SAMPLER(power_data, maneuvering) /* defines sample_power_data_maneuvering_current */
DEFINE_CURRENT_SAMPLER(power_data, shields) /* defines sample_power_data_shields_current */
DEFINE_CURRENT_SAMPLER(power_data, comms) /* defines sample_power_data_comms_current */
DEFINE_CURRENT_SAMPLER(power_data, impulse) /* defines sample_power_data_impulse_current */
DEFINE_CURRENT_SAMPLER(power_data, tractor) /* defines sample_power_data_tractor_current */
DEFINE_CURRENT_SAMPLER(power_data, lifesupport) /* defines sample_power_data_lifesupport_current */

DEFINE_CURRENT_SAMPLER(coolant_data, warp) /* defines sample_coolant_data_warp_current */
DEFINE_CURRENT_SAMPLER(coolant_data, sensors) /* defines sample_coolant_data_sensors_current */
DEFINE_CURRENT_SAMPLER(coolant_data, phasers) /* defines sample_coolant_data_phasers_current */
DEFINE_CURRENT_SAMPLER(coolant_data, maneuvering) /* defines sample_coolant_data_maneuvering_current */
DEFINE_CURRENT_SAMPLER(coolant_data, shields) /* defines sample_coolant_data_shields_current */
DEFINE_CURRENT_SAMPLER(coolant_data, comms) /* defines sample_coolant_data_comms_current */
DEFINE_CURRENT_SAMPLER(coolant_data, impulse) /* defines sample_coolant_data_impulse_current */
DEFINE_CURRENT_SAMPLER(coolant_data, tractor) /* defines sample_coolant_data_tractor_current */
DEFINE_CURRENT_SAMPLER(coolant_data, lifesupport) /* defines sample_coolant_data_lifesupport_current */


static void do_phaser_wavelength(struct slider *s)
{
	do_adjust_control_input(s, OPCODE_ADJUST_CONTROL_LASER_WAVELENGTH);
}

static void wavelen_updown_button_pressed(int direction)
{
	uint8_t value;
	struct snis_entity *o;
	int inc;

	if (!(o = find_my_ship()))
		return;
	inc = (int) (256.0 / 50.0);
	value = o->tsd.ship.phaser_wavelength;
	if (direction > 0 && value + inc > 255)
		return;
	if (direction < 0 && value - inc < 0)
		return;
	value += direction < 0 ? -inc : direction > 0 ? inc : 0;
	transmit_adjust_control_input(value, OPCODE_ADJUST_CONTROL_LASER_WAVELENGTH);
}

static double sample_generic_damage_data(int field_offset)
{
	uint8_t *field;
	struct snis_entity *o;

	if (!(o = find_my_ship()))
		return 0.0;
	field = (uint8_t *) &o->tsd.ship.damage + field_offset; 
	return 100.0 * (255 - *field) / 255.0;
}

static double sample_generic_temperature_data(int field_offset)
{
	uint8_t *field;
	struct snis_entity *o;

	if (!(o = find_my_ship()))
		return 0.0;
	field = (uint8_t *) &o->tsd.ship.temperature_data + field_offset; 
	return 100.0 * (*field) / 255.0;
}

#define CREATE_DAMAGE_SAMPLER_FUNC(fieldname) \
	static double sample_##fieldname##_damage(void) \
	{ \
		return sample_generic_damage_data(offsetof(struct ship_damage_data, fieldname##_damage)); \
	}

#define CREATE_TEMPERATURE_SAMPLER_FUNC(fieldname) \
	static double sample_##fieldname##_temperature(void) \
	{ \
		return sample_generic_temperature_data(offsetof(struct ship_damage_data, fieldname##_damage)); \
	}

CREATE_DAMAGE_SAMPLER_FUNC(shield) /* sample_shield_damage defined here */
CREATE_DAMAGE_SAMPLER_FUNC(impulse) /* sample_impulse_damage defined here */
CREATE_DAMAGE_SAMPLER_FUNC(warp) /* sample_warp_damage defined here */
CREATE_DAMAGE_SAMPLER_FUNC(maneuvering) /* sample_maneuvering_damage defined here */
CREATE_DAMAGE_SAMPLER_FUNC(phaser_banks) /* sample_phaser_banks_damage defined here */
CREATE_DAMAGE_SAMPLER_FUNC(sensors) /* sample_sensors_damage defined here */
CREATE_DAMAGE_SAMPLER_FUNC(comms) /* sample_comms_damage defined here */
CREATE_DAMAGE_SAMPLER_FUNC(tractor) /* sample_tractor_damage defined here */
CREATE_DAMAGE_SAMPLER_FUNC(lifesupport) /* sample_lifesupport_damage defined here */

CREATE_TEMPERATURE_SAMPLER_FUNC(shield) /* sample_shield_temperature defined here */
CREATE_TEMPERATURE_SAMPLER_FUNC(impulse) /* sample_impulse_temperature defined here */
CREATE_TEMPERATURE_SAMPLER_FUNC(warp) /* sample_warp_temperature defined here */
CREATE_TEMPERATURE_SAMPLER_FUNC(maneuvering) /* sample_maneuvering_temperature defined here */
CREATE_TEMPERATURE_SAMPLER_FUNC(phaser_banks) /* sample_phaser_banks_temperature defined here */
CREATE_TEMPERATURE_SAMPLER_FUNC(sensors) /* sample_sensors_temperature defined here */
CREATE_TEMPERATURE_SAMPLER_FUNC(comms) /* sample_comms_temperature defined here */
CREATE_TEMPERATURE_SAMPLER_FUNC(tractor) /* sample_tractor_temperature defined here */
CREATE_TEMPERATURE_SAMPLER_FUNC(lifesupport) /* sample_lifesupport_temperature defined here */

static void engage_warp_button_pressed(__attribute__((unused)) void *cookie)
{
	do_adjust_byte_value(0,  OPCODE_ENGAGE_WARP);
	warp_field_error = 3 * PLAYER_WARP_SPINUP_TIME; /* 3x since client clock is 30hz, server's is 10hz */
}

static void docking_magnets_button_pressed(__attribute__((unused)) void *cookie)
{
	do_adjust_byte_value(0,  OPCODE_DOCKING_MAGNETS);
}

static void standard_orbit_button_pressed(__attribute__((unused)) void *cookie)
{
	do_adjust_byte_value(0, OPCODE_REQUEST_STANDARD_ORBIT);
}

static void nav_starmap_button_pressed(__attribute__((unused)) void *cookie)
{
	do_adjust_byte_value(0, OPCODE_REQUEST_STARMAP);
}

static void nav_lights_button_pressed(__attribute__((unused)) void *cookie)
{
	struct snis_entity *o;

	o = find_my_ship();
	if (!o)
		return;
	if (o->tsd.ship.exterior_lights == 0)
		transmit_adjust_control_input((uint8_t) 255, OPCODE_ADJUST_CONTROL_EXTERIOR_LIGHTS);
	else
		transmit_adjust_control_input((uint8_t) 0, OPCODE_ADJUST_CONTROL_EXTERIOR_LIGHTS);
}

static void nav_custom_button_pressed(__attribute__((unused)) void *cookie)
{
	queue_to_server(snis_opcode_pkt("bb", OPCODE_CUSTOM_BUTTON, OPCODE_CUSTOM_BUTTON_SUBCMD_NAV));
}

static void reverse_button_pressed(__attribute__((unused)) void *s)
{
	struct snis_entity *o;

	if (!(o = find_my_ship()))
		return;
	snis_button_set_color(nav_ui.reverse_button, !o->tsd.ship.reverse ?
			UI_COLOR(nav_reverse_button) : UI_COLOR(nav_button));
	do_adjust_byte_value(!o->tsd.ship.reverse,  OPCODE_REQUEST_REVERSE);
}

static void trident_button_pressed(__attribute__((unused)) void *s)
{
	struct snis_entity *o;
	if (!(o = find_my_ship()))
		return;
	do_adjust_byte_value(!o->tsd.ship.trident,  OPCODE_NAV_TRIDENT_MODE);
}

static void ui_add_slider(struct slider *s, int active_displaymode, char *tooltip)
{
	struct ui_element *uie;

	uie = ui_element_init(s, ui_slider_draw, ui_slider_button_release, ui_slider_inside,
						active_displaymode, &displaymode);
	ui_element_set_tooltip(uie, tooltip);
	ui_element_list_add_element(&uiobjs, uie); 
}

static void ui_add_button(struct button *b, int active_displaymode, char *tooltip)
{
	struct ui_element *uie;

	uie = ui_element_init(b, ui_button_draw, ui_button_button_release, ui_button_inside,
						active_displaymode, &displaymode);
	ui_element_set_tooltip(uie, tooltip);
	ui_element_set_button_press_function(uie, ui_button_button_press);
	ui_element_list_add_element(&uiobjs, uie); 
}

static void ui_add_pull_down_menu(struct pull_down_menu *pdm, int active_display_mode)
{
	struct ui_element *uie;

	uie = ui_element_init(pdm, ui_pull_down_menu_draw, ui_pull_down_menu_button_release,
				ui_pull_down_menu_inside, active_display_mode, &displaymode);
	ui_element_set_tooltip(uie, "");
	ui_set_update_mouse_pos_callback(uie, (ui_update_mouse_pos_function)
						pull_down_menu_update_mouse_pos);
	ui_element_list_add_element(&uiobjs, uie);
}

static void ui_add_strip_chart(struct strip_chart *sc, int active_displaymode)
{
	struct ui_element *uie;

	uie = ui_element_init(sc, ui_strip_chart_draw, NULL, NULL,
		active_displaymode, &displaymode);
	ui_element_list_add_element(&uiobjs, uie);
}

static void ui_add_scaling_strip_chart(struct scaling_strip_chart *sc, int active_displaymode)
{
	struct ui_element *uie;

	uie = ui_element_init(sc, ui_scaling_strip_chart_draw, NULL, NULL,
		active_displaymode, &displaymode);
	ui_element_list_add_element(&uiobjs, uie);
}

static void ui_hide_widget(void *widget)
{
	struct ui_element *uie;

	uie = widget_to_ui_element(uiobjs, widget);
	if (!uie)
		return;
	ui_element_hide(uie);
}

static void ui_unhide_widget(void *widget)
{
	struct ui_element *uie;

	uie = widget_to_ui_element(uiobjs, widget);
	if (!uie)
		return;
	ui_element_unhide(uie);
}

static void ui_add_label(struct label *l, int active_displaymode)
{
	struct ui_element *uie;

	uie = ui_element_init(l, ui_label_draw, NULL, NULL,
						active_displaymode, &displaymode);
	ui_element_list_add_element(&uiobjs, uie); 
}

static void ui_add_gauge(struct gauge *g, int active_displaymode)
{
	struct ui_element *uie;

	uie = ui_element_init(g, ui_gauge_draw, NULL, ui_gauge_inside,
						active_displaymode, &displaymode);
	ui_element_list_add_element(&uiobjs, uie); 
}

static void ui_add_text_window(struct text_window *tw, int active_displaymode)
{
	struct ui_element *uie;

	uie = ui_element_init(tw, ui_text_window_draw, ui_text_window_button_release, NULL,
						active_displaymode, &displaymode);
	ui_element_list_add_element(&uiobjs, uie); 
}

static void ui_add_text_input_box(struct snis_text_input_box *t, int active_displaymode)
{
	struct ui_element *uie;

	uie = ui_element_init(t, ui_text_input_draw, ui_text_input_button_release, NULL,
						active_displaymode, &displaymode);
	ui_element_set_focus_callback(uie, ui_text_input_box_set_focus);
	ui_element_get_keystrokes(uie, ui_text_input_keypress, NULL);
	ui_element_list_add_element(&uiobjs, uie); 
}

static void init_lobby_ui()
{
	lobby_ui.lobby_cancel_button = snis_button_init(txx(650), txy(520), -1, -1,
			"CANCEL", UI_COLOR(lobby_cancel), NANO_FONT, lobby_cancel_button_pressed, NULL);
	lobby_ui.lobby_connect_to_server_button = snis_button_init(txx(250), txy(520), -1, -1,
			"CONNECT TO SERVER", UI_COLOR(lobby_connect_not_ok), NANO_FONT,
			lobby_connect_to_server_button_pressed, NULL);
	ui_add_button(lobby_ui.lobby_cancel_button, DISPLAYMODE_LOBBYSCREEN,
			"RETURN TO NETWORK SETUP SCREEN");
	ui_add_button(lobby_ui.lobby_connect_to_server_button, DISPLAYMODE_LOBBYSCREEN,
			"CONNECT TO THE SELECTED SERVER");
	snis_button_set_sound(lobby_ui.lobby_cancel_button, UISND3);
	snis_button_set_sound(lobby_ui.lobby_connect_to_server_button, UISND4);
	ui_hide_widget(lobby_ui.lobby_connect_to_server_button);
}

static double sample_phaser_wavelength(void);

static void weapons_custom_button_pressed(__attribute__((unused)) void *x)
{
	queue_to_server(snis_opcode_pkt("bb", OPCODE_CUSTOM_BUTTON, OPCODE_CUSTOM_BUTTON_SUBCMD_WEAPONS));
}

static void init_weapons_ui(void)
{
	const int phx = 0.35 * SCREEN_WIDTH;
	const int phy = 0.91666 * SCREEN_HEIGHT;
	const int r = 0.075 * SCREEN_HEIGHT;
	const int wlx = SCREEN_WIDTH - phx;
	const int wlsx = 0.39375 * SCREEN_WIDTH; 
	const int wlsy = 0.98 * SCREEN_HEIGHT;
	const int wlsw = 0.2125 * SCREEN_WIDTH;
	const int wlsh = 0.025 * SCREEN_HEIGHT;
	weapons.phaser_bank_gauge = gauge_init(phx, phy, r, 0.0, 100.0, -120.0 * M_PI / 180.0,
			120.0 * 2.0 * M_PI / 180.0, UI_COLOR(weap_gauge_needle), UI_COLOR(weap_gauge),
			10, "CHARGEx10", sample_phasercharge);
	gauge_set_multiplier(weapons.phaser_bank_gauge, 10.0);
	gauge_set_fonts(weapons.phaser_bank_gauge, PICO_FONT, PICO_FONT);
	gauge_add_needle(weapons.phaser_bank_gauge, sample_phaser_power, UI_COLOR(weap_gauge_needle));
	gauge_fill_background(weapons.phaser_bank_gauge, BLACK, 0.75);
	weapons.phaser_wavelength = gauge_init(wlx, phy, r, 10.0, 60.0, -120.0 * M_PI / 180.0,
			120.0 * 2.0 * M_PI / 180.0, UI_COLOR(weap_gauge_needle), UI_COLOR(weap_gauge),
			10, "WAVE LEN", sample_phaser_wavelength);
	gauge_set_fonts(weapons.phaser_wavelength, PICO_FONT, PICO_FONT);
	gauge_fill_background(weapons.phaser_wavelength, BLACK, 0.75);
	weapons.wavelen_slider = snis_slider_init(wlsx, wlsy, wlsw, wlsh, UI_COLOR(weap_slider), "",
				"10", "60", 10, 60, sample_phaser_wavelength,
				do_phaser_wavelength);
	weapons.custom_button = snis_button_init(txx(10), SCREEN_HEIGHT - txy(30), -1, -1, "CUSTOM BUTTON",
					UI_COLOR(weap_gauge),
					NANO_FONT, weapons_custom_button_pressed, NULL);
	snis_button_set_sound(nav_ui.custom_button, UISND21);
	ui_add_slider(weapons.wavelen_slider, DISPLAYMODE_WEAPONS, "PHASER WAVELENGTH CONTROL");
	ui_add_gauge(weapons.phaser_bank_gauge, DISPLAYMODE_WEAPONS);
	ui_set_widget_tooltip(weapons.phaser_bank_gauge,
				"PHASER CHARGE INDICATOR\n"
				"LARGE NEEDLE INDICATES CURRENT PHASER CHARGE\n"
				"SMALL NEEDLE INDICATES PHASER RECHARGE RATE");
	ui_add_gauge(weapons.phaser_wavelength, DISPLAYMODE_WEAPONS);
	ui_set_widget_tooltip(weapons.phaser_wavelength,
				"PHASER WAVELENGTH INDICATOR\n"
				"MATCH PHASER WAVELENGTH TO\n"
				"TARGET SHIELD GENERATOR WAVELENGTH");
	ui_add_button(weapons.custom_button, DISPLAYMODE_WEAPONS, "CUSTOM BUTTON");
}

static void show_death_screen(GtkWidget *w)
{
	char buf[100];

	struct snis_entity *o;

	o = find_my_ship();
	if (!o)
		return;

	sng_set_foreground(UI_COLOR(death_text));

	if (o->tsd.ship.oxygen < 10) {
		snprintf(buf, sizeof(buf), "YOUR CREW HAS DIED");
		sng_abs_xy_draw_string(buf, BIG_FONT, txx(20), txy(150));
		snprintf(buf, sizeof(buf), "BY ASPHYXIATION");
		sng_abs_xy_draw_string(buf, BIG_FONT, txx(20), txy(250));
		snprintf(buf, sizeof(buf), "RESPAWNING IN %d SECONDS", o->respawn_time);
		sng_abs_xy_draw_string(buf, TINY_FONT, txx(20), txy(500));
	} else {
		snprintf(buf, sizeof(buf), "YOUR SHIP");
		sng_abs_xy_draw_string(buf, BIG_FONT, txx(20), txy(150));
		snprintf(buf, sizeof(buf), "HAS BEEN");
		sng_abs_xy_draw_string(buf, BIG_FONT, txx(20), txy(250));
		snprintf(buf, sizeof(buf), "BLOWN TO");
		sng_abs_xy_draw_string(buf, BIG_FONT, txx(20), txy(350));
		snprintf(buf, sizeof(buf), "SMITHEREENS");
		sng_abs_xy_draw_string(buf, BIG_FONT, txx(20), txy(450));
		snprintf(buf, sizeof(buf), "RESPAWNING IN %d SECONDS", o->respawn_time);
		sng_abs_xy_draw_string(buf, TINY_FONT, txx(20), txy(500));
	}
}

static void show_rts_loss_screen(GtkWidget *w)
{
	char buf[100];

	sng_set_foreground(UI_COLOR(death_text));
	snprintf(buf, sizeof(buf), "YOU HAVE");
	sng_abs_xy_draw_string(buf, BIG_FONT, txx(20), txy(150));
	snprintf(buf, sizeof(buf), "BEEN DEFEATED...");
	sng_abs_xy_draw_string(buf, BIG_FONT, txx(20), txy(250));
}

static void show_rts_win_screen(GtkWidget *w)
{
	char buf[100];

	sng_set_foreground(UI_COLOR(death_text));
	snprintf(buf, sizeof(buf), "YOU HAVE DEFEATED");
	sng_abs_xy_draw_string(buf, BIG_FONT, txx(20), txy(150));
	snprintf(buf, sizeof(buf), "THE ENEMY...");
	sng_abs_xy_draw_string(buf, BIG_FONT, txx(20), txy(250));
}

void set_cursor(GtkWidget *window, int cursor_type)
{
	GdkCursor *cursor;
	cursor = gdk_cursor_new(cursor_type);
	gdk_window_set_cursor(window->window, cursor);
	gdk_cursor_destroy(cursor);
}

void set_cross_cursor(GtkWidget *window)
{
	set_cursor(window, GDK_CROSS);
}

void set_normal_cursor(GtkWidget *window)
{
	set_cursor(window, GDK_ARROW);
}

static void maybe_grab_or_ungrab_mouse(GtkWidget *w, GdkEventMask event_mask)
{
	/* If we are not on the weapons screen, free the mouse if not already free. */
	if (displaymode != DISPLAYMODE_WEAPONS) {
		if (current_mouse_ui_mode != MOUSE_MODE_FREE_MOUSE) {
			current_mouse_ui_mode = MOUSE_MODE_FREE_MOUSE;
			(void) gdk_pointer_ungrab(GDK_CURRENT_TIME); /* free the mouse */
			set_normal_cursor(w);
		}
		return;
	}
	if (desired_mouse_ui_mode == current_mouse_ui_mode)
		return;
	if (desired_mouse_ui_mode == MOUSE_MODE_CAPTURED_MOUSE) { /* capture the mouse */
		(void) gdk_pointer_grab(gtk_widget_get_window(w), FALSE, event_mask,
					NULL, NULL, GDK_CURRENT_TIME);
		set_cross_cursor(w);
	} else {
		(void) gdk_pointer_ungrab(GDK_CURRENT_TIME); /* free the mouse */
		set_normal_cursor(w);
	}
	current_mouse_ui_mode = desired_mouse_ui_mode;
}

static void show_manual_weapons(GtkWidget *w)
{
	show_weapons_camera_view(w);
	show_common_screen(w, "WEAPONS");
}

static void send_natural_language_request_to_server(char *msg);
static void nav_computer_data_entered(void *x)
{
	trim_whitespace(nav_ui.input);
	clean_spaces(nav_ui.input);
	if (strcmp(nav_ui.input, "") != 0) /* skip blank lines */
		send_natural_language_request_to_server(nav_ui.input);
	snis_text_input_box_zero(nav_ui.computer_input);
	nav_ui.computer_active = 0;
	ui_unhide_widget(nav_ui.computer_button);
	ui_hide_widget(nav_ui.computer_input);
}

static double sample_warpdrive(void);

static void nav_computer_button_pressed(__attribute__((unused)) void *s)
{
	nav_ui.computer_active = 1;
	ui_hide_widget(nav_ui.computer_button);
	ui_unhide_widget(nav_ui.computer_input);
}

static void nav_camera_pos_button_pressed(__attribute__((unused)) void *s)
{
	do_nav_camera_mode();
}

int nav_ui_docking_magnets_active(__attribute__((unused)) void *notused)
{
	struct snis_entity *o;

	if (!(o = find_my_ship()))
		return 0;

	return o->tsd.ship.docking_magnets;
}

int nav_ui_lights_active(__attribute__((unused)) void *notused)
{
	struct snis_entity *o;

	if (!(o = find_my_ship()))
		return 0;

	return o->tsd.ship.exterior_lights;
}

static void init_nav_ui(void)
{
	int x, y;
	const int gauge_color = UI_COLOR(nav_gauge);
	const int needle_color = UI_COLOR(nav_gauge_needle);
	const int button_color = UI_COLOR(nav_button);
	const int button_y_spacing = 22;

	x = 0;
	nav_ui.gauge_radius = 40;
	
	nav_ui.warp_slider = snis_slider_init(SCREEN_WIDTH - txx(2.2 * nav_ui.gauge_radius + 10),
				txy(2.2 * nav_ui.gauge_radius + 30),
				txx(2.0 * nav_ui.gauge_radius), txy(10), UI_COLOR(nav_slider), "",
				"0", "100", 0.0, 255.0, sample_power_data_warp_current,
				do_warpdrive);
	snis_slider_set_fuzz(nav_ui.warp_slider, 3);
	nav_ui.navzoom_slider = snis_slider_init(txx(4), txy(44), txx(83), txy(10), UI_COLOR(nav_slider), "ZOOM",
				"1", "10", 0.0, 100.0, sample_navzoom,
				do_navzoom);
	nav_ui.throttle_slider = snis_slider_init(SCREEN_WIDTH - txx(12 + x), txy(22), txx(96), txy(8),
				UI_COLOR(nav_slider), "THROTTLE", "1", "10", 0.0, 255.0,
				sample_power_data_impulse_current, do_throttle);
	snis_slider_set_fuzz(nav_ui.throttle_slider, 3);
	snis_slider_set_vertical(nav_ui.throttle_slider, 1);
	nav_ui.warp_gauge = gauge_init(SCREEN_WIDTH - txx(1.2 * nav_ui.gauge_radius + 10),
				txy(nav_ui.gauge_radius + 16),
				txx(nav_ui.gauge_radius), 0.0, 10.0, -120.0 * M_PI / 180.0,
				120.0 * 2.0 * M_PI / 180.0, needle_color, gauge_color,
				10, "WARP", sample_warpdrive);
	gauge_set_fonts(nav_ui.warp_gauge, PICO_FONT, PICO_FONT);
	nav_ui.speedometer = gauge_init(SCREEN_WIDTH - txx(3.6 * nav_ui.gauge_radius + 10),
				txy(nav_ui.gauge_radius + 16),
				txx(nav_ui.gauge_radius), 0.0, 1000.0, -120.0 * M_PI / 180.0,
				120.0 * 2.0 * M_PI / 180.0, needle_color, gauge_color,
				10, "VELx10", sample_ship_velocity);
	gauge_set_multiplier(nav_ui.speedometer, 10.0);
	gauge_set_fonts(nav_ui.speedometer, PICO_FONT, PICO_FONT);
	y = 3 * button_y_spacing;
	nav_ui.engage_warp_button = snis_button_init(SCREEN_WIDTH - txx(nav_ui.gauge_radius * 2.2 + 10),
					txy(nav_ui.gauge_radius * 2 + y),
					-1, -1, "ENGAGE WARP", button_color,
					NANO_FONT, engage_warp_button_pressed, NULL);
	snis_button_set_sound(nav_ui.engage_warp_button, UISND4);
	y += button_y_spacing;
	nav_ui.docking_magnets_button = snis_button_init(SCREEN_WIDTH - txx(nav_ui.gauge_radius * 2.2 + 10),
					txy(nav_ui.gauge_radius * 2 + y),
					-1, -1, "MMAGNETS", button_color,
					NANO_FONT, docking_magnets_button_pressed, NULL);
	snis_button_set_checkbox_function(nav_ui.docking_magnets_button,
			nav_ui_docking_magnets_active,
			NULL);
	snis_button_set_label(nav_ui.docking_magnets_button, "MAGNETS");
	snis_button_set_sound(nav_ui.docking_magnets_button, UISND5);
	y += button_y_spacing;
	nav_ui.standard_orbit_button = snis_button_init(SCREEN_WIDTH - txx(nav_ui.gauge_radius * 2.2 + 10),
					txy(nav_ui.gauge_radius * 2 + y),
					-1, -1, "STANDARD ORBIT", button_color,
					NANO_FONT, standard_orbit_button_pressed, NULL);
	snis_button_set_sound(nav_ui.standard_orbit_button, UISND6);
	y += button_y_spacing;
	nav_ui.starmap_button = snis_button_init(SCREEN_WIDTH - txx(nav_ui.gauge_radius * 2.2 + 10),
					txy(nav_ui.gauge_radius * 2 + y), -1, -1, "STAR MAP",
					button_color,
					NANO_FONT, nav_starmap_button_pressed, NULL);
	snis_button_set_sound(nav_ui.starmap_button, UISND7);
	y += button_y_spacing;
	nav_ui.lights_button = snis_button_init(SCREEN_WIDTH - txx(nav_ui.gauge_radius * 2.2 + 10),
					txy(nav_ui.gauge_radius * 2 + y), -1, -1, "MLIGHTS",
					button_color,
					NANO_FONT, nav_lights_button_pressed, NULL);
	snis_button_set_checkbox_function(nav_ui.lights_button,
			nav_ui_lights_active,
			NULL);
	snis_button_set_label(nav_ui.lights_button, "LIGHTS");
	snis_button_set_sound(nav_ui.lights_button, UISND7);
	y += button_y_spacing;
	nav_ui.custom_button = snis_button_init(SCREEN_WIDTH - txx(nav_ui.gauge_radius * 2.2 + 10),
					txy(nav_ui.gauge_radius * 2 + y), -1, -1, "CUSTOM BUTTON",
					button_color,
					NANO_FONT, nav_custom_button_pressed, NULL);
	snis_button_set_sound(nav_ui.custom_button, UISND21);
	nav_ui.reverse_button = snis_button_init(SCREEN_WIDTH - txx(16.6 + x), txy(3), txx(12), txy(14),
			"R", button_color, NANO_FONT, reverse_button_pressed, NULL);
	snis_button_set_sound(nav_ui.reverse_button, UISND8);
	nav_ui.trident_button = snis_button_init(txx(4), txy(166), -1, -1, "ABSOLUTE", button_color,
			NANO_FONT, trident_button_pressed, NULL);
	snis_button_set_sound(nav_ui.trident_button, UISND9);
	nav_ui.computer_button = snis_button_init(txx(4), txy(570), -1, -1, "COMPUTER", UI_COLOR(nav_button),
			NANO_FONT, nav_computer_button_pressed, NULL);
	snis_button_set_sound(nav_ui.computer_button, UISND10);
	nav_ui.computer_active = 0;
	nav_ui.computer_input = snis_text_input_box_init(txx(10), txy(560), txy(30), txx(550),
					UI_COLOR(nav_warning), TINY_FONT, nav_ui.input, 80, &timer, NULL, NULL);
	snis_text_input_box_set_return(nav_ui.computer_input, nav_computer_data_entered);
	snis_text_input_box_set_dynamic_width(nav_ui.computer_input, txx(100), txx(550));
	nav_ui.camera_pos_button = snis_button_init(txx(4), txy(210), -1, -1, "CAM POS", button_color,
			NANO_FONT, nav_camera_pos_button_pressed, NULL);
	snis_button_set_sound(nav_ui.camera_pos_button, UISND10);
	ui_add_slider(nav_ui.warp_slider, DISPLAYMODE_NAVIGATION, "WARP DRIVER POWER CONTROL");
	ui_add_slider(nav_ui.navzoom_slider, DISPLAYMODE_NAVIGATION, "NAVIGATION ZOOM CONTROL");
	ui_add_slider(nav_ui.throttle_slider, DISPLAYMODE_NAVIGATION, "IMPULSE DRIVE THROTTLE CONTROL");
	ui_add_button(nav_ui.engage_warp_button, DISPLAYMODE_NAVIGATION,
				"ACTIVATE THE WARP DRIVE\nAT THE CURRENT INSTANTANEOUS\nPOWER LEVEL");
	ui_add_button(nav_ui.docking_magnets_button, DISPLAYMODE_NAVIGATION,
				"TOGGLE THE DOCKING MAGNETS ON OR OFF");
	ui_add_button(nav_ui.standard_orbit_button, DISPLAYMODE_NAVIGATION,
				"ENTER OR LEAVE STANDARD ORBIT\nAROUND THE NEAREST PLANET");
	ui_add_button(nav_ui.starmap_button, DISPLAYMODE_NAVIGATION,
				"SWITCH BETWEEN NAVIGATION\nAND STAR MAP SCREENS");
	ui_add_button(nav_ui.lights_button, DISPLAYMODE_NAVIGATION,
				"TOGGLE EXTERIOR LIGHTS ON/OFF");
	ui_add_button(nav_ui.custom_button, DISPLAYMODE_NAVIGATION, "CUSTOM BUTTON");
	ui_add_button(nav_ui.reverse_button, DISPLAYMODE_NAVIGATION,
				"TOGGLE REVERSE THRUST");
	ui_add_button(nav_ui.trident_button, DISPLAYMODE_NAVIGATION,
				"TOGGLE ABSOLUTE OR RELATIVE ORIENTATION");
	ui_add_gauge(nav_ui.warp_gauge, DISPLAYMODE_NAVIGATION);
	ui_set_widget_tooltip(nav_ui.warp_gauge,
			"CURRENT WARP CHARGE INDICATOR\n"
			"ENGAGE WARP WHEN INDICATOR\n"
			"SHOWS DESIRED WARP CHARGE");
	ui_add_gauge(nav_ui.speedometer, DISPLAYMODE_NAVIGATION);
	ui_set_widget_tooltip(nav_ui.speedometer, "CURRENT SPEED INDICATOR");
	ui_add_button(nav_ui.computer_button, DISPLAYMODE_NAVIGATION,
				"ACTIVATE THE COMPUTER");
	ui_add_button(nav_ui.camera_pos_button, DISPLAYMODE_NAVIGATION,
				"CHANGE CAMERA POSITION (SHORTCUT BACKQUOTE KEY)");
	ui_add_text_input_box(nav_ui.computer_input, DISPLAYMODE_NAVIGATION);
	ui_hide_widget(nav_ui.computer_input);
	instrumentecx = entity_context_new(5000, 1000);
	tridentecx = entity_context_new(10, 0);
}

static float angular_rate_indicator_xy(double angle, float xyoffset, float rr, float max_angular_rate, float sign)
{
	const float max_rate_deg = max_angular_rate * 180 / M_PI;
	const float angular_rate_deg = angle * 180.0 / M_PI;
	return xyoffset + rr / max_rate_deg * sign * angular_rate_deg;
}

static float pitch_rate_indicator_y(double angle, float ry, float rr)
{
	return angular_rate_indicator_xy(angle, ry, rr, MAX_PITCH_VELOCITY, -1.0);
}

static float yaw_rate_indicator_x(double angle, float rx, float rr)
{
	return angular_rate_indicator_xy(angle, rx, rr, MAX_YAW_VELOCITY, -1.0);
}

static float roll_rate_indicator_x(double angle, float rx, float rr)
{
	return angular_rate_indicator_xy(angle, rx, rr, MAX_ROLL_VELOCITY, 1.0);
}

static void draw_pitch_rate_indicator(struct snis_entity *o, float rx, float ry, float rr)
{
	int i;
	float iy;
	char buffer[10];
	static float last_iy = 0.0;

	for (i = -5; i <= 5; i++) { /* Draw pitch indicator tick marks */
		iy = pitch_rate_indicator_y(i * M_PI / 180.0, ry, rr);
		snis_draw_line(rx + rr * 1.02, iy, rx + rr * 1.1, iy);
	}

	iy = pitch_rate_indicator_y(o->tsd.ship.pitch_velocity, ry, rr);
	iy = (iy + last_iy) / 2.0; /* give the indicator some hysteresis */
	last_iy = iy;
	snis_draw_line(rx + rr * 1.1, iy, rx + rr * 1.3, iy - rr * 0.1);
	snis_draw_line(rx + rr * 1.1, iy, rx + rr * 1.3, iy + rr * 0.1);
	snis_draw_line(rx + rr * 1.3, iy - rr * 0.1, rx + rr * 1.3, iy + rr * 0.1);
	snprintf(buffer, sizeof(buffer), "%3.1f", o->tsd.ship.pitch_velocity * 1800.0 / M_PI);
	sng_abs_xy_draw_string(buffer, PICO_FONT, rx + rr * 1.4, iy);
}

static void draw_yaw_rate_indicator(struct snis_entity *o, float rx, float ry, float rr)
{
	int i;
	float ix;
	char buffer[10];
	static float last_ix = 0.0;

	for (i = -5; i <= 5; i++) { /* Draw Yaw indicator tick marks */
		ix = yaw_rate_indicator_x(i * M_PI / 180.0, rx, rr);
		snis_draw_line(ix, ry + rr * 1.02, ix, ry + rr * 1.1);
	}
	ix = yaw_rate_indicator_x(o->tsd.ship.yaw_velocity, rx, rr);
	ix = (ix + last_ix) / 2.0; /* give the indicator some hysteresis */
	last_ix = ix;
	snis_draw_line(ix, ry + rr * 1.1, ix - rr * 0.1, ry + rr * 1.3);
	snis_draw_line(ix, ry + rr * 1.1, ix + rr * 0.1, ry + rr * 1.3);
	snis_draw_line(ix - rr * 0.1, ry + rr * 1.3, ix + rr * 0.1, ry + rr * 1.3);
	snprintf(buffer, sizeof(buffer), "%3.1f", o->tsd.ship.yaw_velocity * 1800.0 / M_PI);
	sng_abs_xy_draw_string(buffer, PICO_FONT, ix + rr * 0.4, ry + rr * 1.25);
}

static void draw_roll_rate_indicator(struct snis_entity *o, float rx, float ry, float rr)
{
	int i;
	float ix;
	char buffer[10];
	static float last_ix = 0.0;

	for (i = -5; i <= 5; i++) { /* Draw Roll indicator tick marks */
		ix = roll_rate_indicator_x(i * M_PI / 180.0, rx, rr);
		snis_draw_line(ix, ry - rr * 1.02, ix, ry - rr * 1.1);
	}
	ix = roll_rate_indicator_x(o->tsd.ship.roll_velocity, rx, rr);
	ix = (ix + last_ix) / 2.0; /* give the indicator some hysteresis */
	last_ix = ix;
	snis_draw_line(ix, ry - rr * 1.1, ix - rr * 0.1, ry - rr * 1.3);
	snis_draw_line(ix, ry - rr * 1.1, ix + rr * 0.1, ry - rr * 1.3);
	snis_draw_line(ix - rr * 0.1, ry - rr * 1.3, ix + rr * 0.1, ry - rr * 1.3);
	snprintf(buffer, sizeof(buffer), "%3.1f", o->tsd.ship.roll_velocity * 1800.0 / M_PI);
	sng_abs_xy_draw_string(buffer, PICO_FONT, ix + rr * 0.4, ry - rr * 1.25);
}

static void draw_orientation_trident(GtkWidget *w, GdkGC *gc, struct snis_entity *o, float rx, float ry, float rr)
{
	static struct mesh *xz_ring_mesh = 0;
	int relative_mode = o->tsd.ship.trident;

	if (!xz_ring_mesh) {
		xz_ring_mesh = init_circle_mesh(0, 0, 1, 40, 2.0*M_PI);
	}
	struct material wireframe_material;
	material_init_wireframe_sphere_clip(&wireframe_material);

	struct entity *e;
	union quat cam_orientation;

	if (!relative_mode)
		quat_init_axis(&cam_orientation, 0, 1, 0, 0);
	else
		cam_orientation = o->orientation;

	union vec3 center_pos = {{0, 0, 0}};

	/* given field of view angle, calculate how far away to show a radius 1.0 sphere */
	float fovy = 20.0 * M_PI / 180.0;
	float dist_to_cam = 1.05 / tan(fovy/2.0);

        set_renderer(tridentecx, WIREFRAME_RENDERER);
	camera_set_parameters(tridentecx, dist_to_cam-1.0, dist_to_cam+1.0,
				2.0 * rr * ASPECT_RATIO, 2.0 * rr, fovy);
	entity_context_set_hi_lo_poly_pixel_threshold(tridentecx, low_poly_threshold);
	set_window_offset(tridentecx, rx - rr * ASPECT_RATIO, ry - rr);

	/* figure out the camera positions */
	union vec3 camera_up = { {0, 1, 0} };
	if (relative_mode)
		quat_rot_vec_self(&camera_up, &cam_orientation);

	union vec3 camera_pos = { {-dist_to_cam, 0, 0} };
	if (relative_mode)
		quat_rot_vec_self(&camera_pos, &cam_orientation);
	vec3_add_self(&camera_pos, &center_pos);

	union vec3 camera_lookat = {{0, 0, 0}};
	if (relative_mode)
		quat_rot_vec_self(&camera_lookat, &cam_orientation);
	vec3_add_self(&camera_lookat, &center_pos);

	camera_assign_up_direction(tridentecx, camera_up.v.x, camera_up.v.y, camera_up.v.z);
	camera_set_pos(tridentecx, camera_pos.v.x, camera_pos.v.y, camera_pos.v.z);
	camera_look_at(tridentecx, camera_lookat.v.x, camera_lookat.v.y, camera_lookat.v.z);

	calculate_camera_transform(tridentecx);

	struct material yaw_material = { {
		.color_by_w = { COLOR_LIGHTER(CYAN, 100),
				CYAN,
				COLOR_DARKER(CYAN, 80),
				dist_to_cam - 1.0,
				dist_to_cam,
				dist_to_cam + 1.0 } },
		.type = MATERIAL_COLOR_BY_W,
		.billboard_type = MATERIAL_BILLBOARD_TYPE_NONE };

	struct material pitch_material = { {
		.color_by_w = { COLOR_LIGHTER(GREEN, 100),
				GREEN,
				COLOR_DARKER(GREEN, 80),
				dist_to_cam - 1.0,
				dist_to_cam,
				dist_to_cam + 1.0 } },
		.type = MATERIAL_COLOR_BY_W,
		.billboard_type = MATERIAL_BILLBOARD_TYPE_NONE };

	struct material roll_material = { {
		.color_by_w = { COLOR_LIGHTER(BLUE, 100),
				BLUE,
				COLOR_DARKER(BLUE, 80),
				dist_to_cam - 1.0,
				dist_to_cam,
				dist_to_cam + 1.0 } },
		.type = MATERIAL_COLOR_BY_W,
		.billboard_type = MATERIAL_BILLBOARD_TYPE_NONE };

	/* add yaw axis */
	e = add_entity(tridentecx, xz_ring_mesh, center_pos.v.x, center_pos.v.y, center_pos.v.z, CYAN);
	if (e)
		update_entity_material(e, &yaw_material);

	/* add pitch1 axis */
	union quat pitch_orientation;
	quat_init_axis(&pitch_orientation, 1, 0, 0, M_PI/2.0);
	e = add_entity(tridentecx, xz_ring_mesh, center_pos.v.x, center_pos.v.y, center_pos.v.z, GREEN);
	if (e) {
		update_entity_orientation(e, &pitch_orientation);
		update_entity_material(e, &pitch_material);
	}

	/* add roll axis */
	union quat roll_orientation;
	quat_init_axis(&roll_orientation, 0, 0, 1, M_PI/2.0);
	e = add_entity(tridentecx, xz_ring_mesh, center_pos.v.x, center_pos.v.y, center_pos.v.z, GREEN);
	if (e) {
		update_entity_orientation(e, &roll_orientation);
		update_entity_material(e, &roll_material);
	}

	/* add absolute straight ahead ind, down z axis with y up to match heading = 0 mark 0 */
	union quat ind_orientation;
	quat_init_axis(&ind_orientation, 0, 1, 0, -M_PI/2.0);
	union vec3 ind_pos = {{0.9,0,0}};
	quat_rot_vec_self(&ind_pos, &ind_orientation);
	vec3_add_self(&ind_pos, &center_pos);
	e = add_entity(tridentecx, heading_indicator_mesh, ind_pos.v.x, ind_pos.v.y, ind_pos.v.z, WHITE);
	if (e) {
		update_entity_orientation(e, &ind_orientation);
		update_entity_scale(e, 0.1/heading_indicator_mesh->radius);
	}

	e = add_entity(tridentecx, ship_mesh_map[o->tsd.ship.shiptype],
			center_pos.v.x, center_pos.v.y, center_pos.v.z, UI_COLOR(nav_trident_ship));
	if (e) {
		update_entity_orientation(e, &o->orientation);
		update_entity_scale(e, 0.15 / heading_indicator_mesh->radius);
	}

	render_entities(tridentecx);
	remove_all_entity(tridentecx);

	/* FIXME: These indicators use the yaw/pitch/roll velocities transmitted from
	 * the server which is straighforward, but these values are kind of jumpy and
	 * seemingly (from player's point of view) imprecise. It would probably be a
	 * lot smoother to sample the interpolated orientation quaternions instead.
	 */
	sng_set_foreground(UI_COLOR(nav_gauge_needle));
	draw_pitch_rate_indicator(o, rx, ry, rr);
	draw_yaw_rate_indicator(o, rx, ry, rr);
	draw_roll_rate_indicator(o, rx, ry, rr);

}

#define NAV_DATA_X 530 
#define NAV_DATA_Y 40 
#define NAV_DATA_W ((SCREEN_WIDTH - 5) - NAV_DATA_X)
#define NAV_DATA_H 270 

static void draw_science_graph(GtkWidget *w, struct snis_entity *ship, struct snis_entity *o,
		int x1, int y1, int x2, int y2);

static void draw_3d_nav_starmap(GtkWidget *w, GdkGC *gc)
{
	int i, j, k, our_ss = -1;
	double ox, oy, oz;
	struct entity *e = NULL;
	union vec3 camera_up = { { 0.0, 1.0, 0.0 } };
	union vec3 camera_pos = { { 0.0, 40.0, 5.0 } };
	float x1, y1, z1, x2, y2, z2;
	const float starmap_mesh_scale = 1.0 * SNIS_WARP_GATE_THRESHOLD;
	const float starmap_grid_size = 6.0;
	float cam_dist;
	static float nav_camera_pos_factor = 1.0;
	float new_nav_camera_pos_factor;

	/* Slide the camera towards desired position */
	switch (nav_camera_mode) {
	case 0:
		new_nav_camera_pos_factor = 1.0;
		break;
	case 1:
		new_nav_camera_pos_factor = 0.5;
		break;
	case 2:
		new_nav_camera_pos_factor = 0.25;
		break;
	case 3:
		new_nav_camera_pos_factor = 0.125;
		break;
	case 4:
		new_nav_camera_pos_factor = 2.0;
		break;
	default:
		new_nav_camera_pos_factor = 1.0;
		break;
	}
	float camera_pos_delta = 0.05 * (new_nav_camera_pos_factor - nav_camera_pos_factor);
	nav_camera_pos_factor += camera_pos_delta;
	cam_dist = 100.0 * nav_camera_pos_factor;

	/* Check that there is at least one star */
	if (nstarmap_entries <= 0)
		return;

	pthread_mutex_lock(&universe_mutex);
	/* Find which solarsystem we are in. */
	for (i = 0; i < nstarmap_entries; i++)
		if (strcasecmp(starmap[i].name, solarsystem_name) == 0) {
			our_ss = i;
			break;
		}

	/* Make sure we actually know where we are */
	if (our_ss < 0) {
		pthread_mutex_unlock(&universe_mutex);
		return;
	}

	/* Origin */
	ox = starmap[our_ss].x;
	oy = starmap[our_ss].y;
	oz = starmap[our_ss].z;

	/* Spin the camera slowly around */
	float angle = fmod((10.0 * universe_timestamp() / UNIVERSE_TICKS_PER_SECOND), 360.0) * M_PI / 180.0;
	camera_pos.v.x = sin(angle) * cam_dist;
	camera_pos.v.y = 0;
	camera_pos.v.z = cos(angle) * cam_dist;

	camera_assign_up_direction(instrumentecx, camera_up.v.x, camera_up.v.y, camera_up.v.z);
	camera_set_pos(instrumentecx, camera_pos.v.x, camera_pos.v.y, camera_pos.v.z);
	camera_look_at(instrumentecx, 0.0, 0.0, 0.0);
	set_renderer(instrumentecx, WIREFRAME_RENDERER);
	float near = 0.5;
	float far = 1000.0;
	float angle_of_view = 80;
	camera_set_parameters(instrumentecx, near, far,
				SCREEN_WIDTH, SCREEN_HEIGHT, angle_of_view * M_PI / 180.0);
	calculate_camera_transform(instrumentecx);
	entity_context_set_hi_lo_poly_pixel_threshold(instrumentecx, low_poly_threshold);

	/* Add star entities to the instrumentecx... */
	for (i = 0; i < nstarmap_entries; i++) {
		e = add_entity(instrumentecx, torpedo_nav_mesh,
			starmap[i].x - ox, starmap[i].y - oy, starmap[i].z - oz,
			i == our_ss ? UI_COLOR(starmap_home_star) : UI_COLOR(starmap_star));
		update_entity_scale(e, 0.1);
		entity_set_user_data(e, &starmap[i]); /* so we can draw labels later */
	}

	/* Setup 3d grid entities */
	for (i = 0; i < (int) starmap_grid_size; i++) {
		float x = (i - 0.5 * (starmap_grid_size - 1.0)) * starmap_mesh_scale;
		for (j = 0; j < (int) starmap_grid_size; j++) {
			float y = (j - 0.5 * (starmap_grid_size - 1.0)) * starmap_mesh_scale;
			for (k = 0; k < (int) starmap_grid_size; k++) {
				float z = (k - 0.5 * (starmap_grid_size - 1.0)) * starmap_mesh_scale;
				(void) add_entity(instrumentecx, nav_axes_mesh, x, y, z,
							UI_COLOR(starmap_grid_color));
			}
		}
	}

	/* Draw warp lanes between stars */
	for (i = 0; i < nstarmap_entries; i++) {
		for (j = 0; j < MAX_STARMAP_ADJACENCIES; j++) {
			int n = starmap_adjacency[i][j];
			if (n == -1)
				break;
			x1 = starmap[i].x - ox;
			y1 = starmap[i].y - oy;
			z1 = starmap[i].z - oz;
			x2 = starmap[n].x - ox;
			y2 = starmap[n].y - oy;
			z2 = starmap[n].z - oz;
			if (n == our_ss || i == our_ss)
				sng_set_foreground(UI_COLOR(starmap_warp_lane));
			else
				sng_set_foreground(UI_COLOR(starmap_far_warp_lane));
			snis_draw_3d_line(w, gc, instrumentecx, x1, y1, z1, x2, y2, z2);
		}
	}

	render_entities(instrumentecx);

	/* Draw labels on stars... */
	sng_set_foreground(UI_COLOR(nav_entity_label));
	for (i = 0; i <= get_entity_count(instrumentecx); i++) {
		float sx, sy;
		char buffer[100];
		struct starmap_entry *s;

		e = get_entity(instrumentecx, i);
		if (!e)
			continue;
		if (!entity_onscreen(e))
			continue;
		s = entity_get_user_data(e);
		if (!s)
			continue;
		entity_get_screen_coords(e, &sx, &sy);
		snprintf(buffer, sizeof(buffer), "%s", s->name);
		sng_abs_xy_draw_string(buffer, NANO_FONT, sx + 10, sy - 15);
	}
	pthread_mutex_unlock(&universe_mutex);
	remove_all_entity(instrumentecx);
}

static int nav_entity_too_far_away(double centerx, double centery, double centerz,
					double objx, double objy, double objz,
					double obj_radius, double display_radius)
{
	double dist = dist3d(centerx - objx, centery - objy, centerz - objz);
	if (dist > obj_radius) /* use the distance to the edge and not the center */
		dist -= obj_radius;
	return dist > display_radius;
}

static void draw_nav_contact_offset_and_ring(struct snis_entity *player_ship,
					union vec3 *ship_pos, union vec3 *ship_normal,
					struct entity *contact, union vec3 *contact_pos)
{
	static struct mesh *ring_mesh = NULL;
	static struct mesh *vline_mesh_pos = NULL;
	static struct mesh *vline_mesh_neg = NULL;
	union vec3 ship_plane_proj;
	float proj_distance = 0;
	struct entity *e;

	if (!ring_mesh) {
		ring_mesh = init_circle_mesh(0, 0, 1, 180, 2.0f * M_PI);
		vline_mesh_pos = init_line_mesh(0, 0, 0, 0, 1, 0);
		vline_mesh_neg = init_line_mesh(0, 0, 0, 0, -1, 0);
	}

	/* add line from center disk to contact in z axis */
	/* first find the point where the contact is orthogonally projected onto the ships normal plane */

	/* orthogonal projection of point onto plane
	   q_proj = q - dot( q - p, n) * n
	   q = point to project, p = point on plane, n = normal to plane */
	union vec3 temp1, temp2;
	proj_distance = vec3_dot(vec3_sub(&temp1, contact_pos, ship_pos), ship_normal);
	vec3_sub(&ship_plane_proj, contact_pos, vec3_mul(&temp2, ship_normal, proj_distance));

	float contact_radius = entity_get_mesh(contact)->radius * fabs(entity_get_scale(contact));
	float contact_ring_radius = 0;

	/* contact intersects the ship normal plane so make the radius the size of that intersection */
	if (fabs(proj_distance) < contact_radius)
		contact_ring_radius = sqrt(contact_radius * contact_radius - proj_distance * proj_distance);
	if (contact_ring_radius < contact_radius / 5.0) /* set a lower bound on size */
		contact_ring_radius = contact_radius / 5.0;

	if (proj_distance > 0)
		e = add_entity(instrumentecx, vline_mesh_neg, contact_pos->v.x, contact_pos->v.y,
			contact_pos->v.z, UI_COLOR(nav_ring));
	else
		e = add_entity(instrumentecx, vline_mesh_pos, contact_pos->v.x, contact_pos->v.y,
			contact_pos->v.z, UI_COLOR(nav_ring));
	if (e) {
		update_entity_scale(e, fabs(proj_distance));
		update_entity_orientation(e, &player_ship->orientation);
	}

	e = add_entity(instrumentecx, ring_mesh, ship_plane_proj.v.x,
			ship_plane_proj.v.y, ship_plane_proj.v.z, UI_COLOR(nav_projected_ring));
	if (e) {
		update_entity_scale(e, contact_ring_radius);
		update_entity_orientation(e, &player_ship->orientation);
	}
}

static void draw_3d_nav_waypoints(struct snis_entity *player_ship, union vec3 *ship_pos, union vec3 *ship_normal,
					float display_radius, int current_zoom, int render_style)
{
	int i;

	for (i = 0; i < sci_ui.nwaypoints; i++) {
		double *wp = &sci_ui.waypoint[i][0];

		/* Check if the waypoint is too far away */
		if (nav_entity_too_far_away(player_ship->x, player_ship->y, player_ship->z, wp[0], wp[1], wp[2],
						nav_axes_mesh->radius, display_radius))
			continue; /* not close enough */
		struct entity *contact = NULL;
		contact = add_entity(instrumentecx, nav_axes_mesh, wp[0], wp[1], wp[2], UI_COLOR(nav_entity));
		if (contact) {
			union vec3 contact_pos = { { wp[0], wp[1], wp[2] } };
			float contact_scale = ((255.0 - current_zoom) / 255.0) * 30.0 + 1000.0;
			set_render_style(contact, render_style);
			entity_set_user_data(contact, wp);
			update_entity_scale(contact, entity_get_scale(contact) * contact_scale);
			/* add line from center disk to contact in z axis */
			draw_nav_contact_offset_and_ring(player_ship, ship_pos, ship_normal, contact, &contact_pos);
		}
	}
}

static void draw_attitude_indicator_mark_ring(struct snis_entity *player_ship,
						float mark_angle, float max_radius)
{
	static struct mesh *mark_ring_mesh = 0;
	struct entity *e;
	union vec3 mark_ring_pos;
	float radius;
	union quat orientation;

	mark_ring_pos.v.x = 0;
	mark_ring_pos.v.y = sin(mark_angle) * max_radius;
	mark_ring_pos.v.z = 0;
	radius = max_radius * cos(mark_angle);
	mark_ring_pos.v.x += player_ship->x;
	mark_ring_pos.v.y += player_ship->y;
	mark_ring_pos.v.z += player_ship->z;

	quat_init_axis(&orientation, 1, 0, 0, 0);

	if (!mark_ring_mesh) {
		mark_ring_mesh = init_circle_mesh(0, 0, 1, 60, 2.0f * M_PI);
		mark_ring_mesh->geometry_mode = MESH_GEOMETRY_LINES;
	}
	e = add_entity(instrumentecx, mark_ring_mesh, mark_ring_pos.v.x, mark_ring_pos.v.y, mark_ring_pos.v.z,
				UI_COLOR(nav_mark_ring));
	if (e) {
		update_entity_scale(e, radius);
		update_entity_orientation(e, &orientation);
	}
}

static void draw_attitude_indicator_reticles(GtkWidget *w, GdkGC *gc, struct snis_entity *o,
			union vec3 *ship_normal, float screen_radius)
{
	int i, m;
	float angle;
	union quat q1;
	union vec3 v1, v2, v3, v4, v5, v6, v7, v8;
	char buffer[10];
	int draw_numbers;
	union vec3 ship_direction; /* vector pointing in the direction the ship points */
	float dot;
	double display_heading, display_mark;
	static const float wide = 0.033;
	static const float narrow = 0.02;

	to_snis_heading_mark(&o->orientation, &display_heading, &display_mark);

	ship_direction.v.x = 1.0;
	ship_direction.v.y = 0.0;
	ship_direction.v.z = 0.0;
	quat_rot_vec_self(&ship_direction, &o->orientation);

	v1.v.x = -1.0; /* v1 to v2: Short z-axis-aligned line 1 unit down the X axis */
	v1.v.y = 0.0;
	v1.v.z = narrow;
	v2.v.x = -1.0;
	v2.v.y = 0.0;
	v2.v.z = -narrow;
	vec3_mul_self(&v1, screen_radius);
	vec3_mul_self(&v2, screen_radius);

	v5.v.x = narrow; /* v5 to v6: Short x-axis-aligned line 1 unit down the Z axis */
	v5.v.y = 0.0;
	v5.v.z = 1.0;
	v6.v.x = -narrow;
	v6.v.y = 0.0;
	v6.v.z = 1.0;
	vec3_mul_self(&v5, screen_radius);
	vec3_mul_self(&v6, screen_radius);

	v7.v.x = 1.0; /* v7 to v8: Short y-axis aligned line 1 unit down the X axis */
	v7.v.y = narrow;
	v7.v.z = 0.0;
	v8.v.x = 1.0;
	v8.v.y = -narrow;
	v8.v.z = 0.0;
	vec3_mul_self(&v7, screen_radius);
	vec3_mul_self(&v8, screen_radius);

	draw_attitude_indicator_mark_ring(o, display_mark, screen_radius);

	/* Start this loop at -5 instead of 0 to get one extra tick mark which we special case
	 * for the Heading indicator on the heading ring.
	 */
	for (i = -5; i < 360; i += 5) {
		if ((i % 15) == 0) {
			v1.v.z = wide * screen_radius;
			v2.v.z = -wide * screen_radius;
			v5.v.x = wide * screen_radius;
			v6.v.x = -wide * screen_radius;
			v7.v.y = wide * screen_radius;
			v8.v.y = -wide * screen_radius;
			sng_set_foreground(UI_COLOR(nav_gauge_needle));
			if ((i % 90) == 0)
				draw_numbers = 0;
			else
				draw_numbers = 1;
		} else {
			v1.v.z = narrow * screen_radius;
			v2.v.z = -narrow * screen_radius;
			v5.v.x = narrow * screen_radius;
			v6.v.x = -narrow * screen_radius;
			v7.v.y = narrow * screen_radius;
			v8.v.y = -narrow * screen_radius;
			sng_set_foreground(UI_COLOR(nav_ring));
			draw_numbers = 0;
		}
		angle = M_PI * i / 180.0;
		quat_init_axis(&q1, 0, 0, 1, angle);
		quat_rot_vec(&v3, &v1, &q1);
		quat_rot_vec(&v4, &v2, &q1);

		dot = vec3_dot(&v3, &ship_direction);

		v3.v.x += o->x - ship_normal->v.x;
		v3.v.y += o->y - ship_normal->v.y;
		v3.v.z += o->z - ship_normal->v.z;
		v4.v.x += o->x - ship_normal->v.x;
		v4.v.y += o->y - ship_normal->v.y;
		v4.v.z += o->z - ship_normal->v.z;

		/* This is "mark" (or elevation) */
		m = 0;
		if (i >= 270) {
			m = abs(i - 360);
			snprintf(buffer, sizeof(buffer), "%d", m);
		} else if (i >= 180) {
			m = abs(i - 180);
			snprintf(buffer, sizeof(buffer), "%d", m);
		} else if (i >= 90) {
			m = -abs(i - 180);
			snprintf(buffer, sizeof(buffer), "%d", m);
		} else if (i >= 0) {
			m = -i;
			snprintf(buffer, sizeof(buffer), "%d", m);
		} else {
			snprintf(buffer, sizeof(buffer), "%d", (int) (display_mark * 180.0 / M_PI));
		}
		if (fabs((float) m - 180.0 * display_mark / M_PI) < 2.5) {
			/* Change the color of tick mark if it is close enough to display_mark. */
			sng_set_foreground(UI_COLOR(nav_heading_ring));
		}
		/* if i < 0, we don't try to draw the mark precisely the way we do for heading because:
		 * 1. TBH it's a pain in the ass to do it because there isn't just 1, there are 4,
		 *    so it isn't a very simple matter to do it,
		 * 2. I did try it, but It gets too busy since there are 4 of them.
		 * Instead, we just change the color of the nearest tick mark (above).
		 */
		if (i >= 0) { /* Skip special case when i == -5 */
			snis_draw_3d_line(w, gc, instrumentecx,
					v3.v.x, v3.v.y, v3.v.z, v4.v.x, v4.v.y, v4.v.z);
			if (draw_numbers && dot >= 0)
				snis_draw_3d_string(instrumentecx, buffer, NANO_FONT, v3.v.x, v3.v.y, v3.v.z);

			quat_init_axis(&q1, 1, 0, 0, angle);
			quat_rot_vec(&v3, &v5, &q1);
			quat_rot_vec(&v4, &v6, &q1);

			dot = vec3_dot(&v3, &ship_direction);

			v3.v.x += o->x - ship_normal->v.x;
			v3.v.y += o->y - ship_normal->v.y;
			v3.v.z += o->z - ship_normal->v.z;
			v4.v.x += o->x - ship_normal->v.x;
			v4.v.y += o->y - ship_normal->v.y;
			v4.v.z += o->z - ship_normal->v.z;

			snis_draw_3d_line(w, gc, instrumentecx,
				v3.v.x, v3.v.y, v3.v.z, v4.v.x, v4.v.y, v4.v.z);
			if (draw_numbers && dot > 0)
				snis_draw_3d_string(instrumentecx, buffer, NANO_FONT, v3.v.x, v3.v.y, v3.v.z);
		}

		/* This one, rotating about y axis, is the "heading" */
		if (i < 0) { /* Draw the ship's current heading */
			angle = 0.5 * M_PI + 2.0 * M_PI - display_heading;
			draw_numbers = 1;
		}
		quat_init_axis(&q1, 0, 1, 0, angle);
		quat_rot_vec(&v3, &v7, &q1);
		quat_rot_vec(&v4, &v8, &q1);

		dot = vec3_dot(&v3, &ship_direction);

		v3.v.x += o->x - ship_normal->v.x;
		v3.v.y += o->y - ship_normal->v.y;
		v3.v.z += o->z - ship_normal->v.z;
		v4.v.x += o->x - ship_normal->v.x;
		v4.v.y += o->y - ship_normal->v.y;
		v4.v.z += o->z - ship_normal->v.z;

		sng_set_foreground(UI_COLOR(nav_heading_ring));
		if (i > 0)
			snprintf(buffer, sizeof(buffer), "%d", (360 + 90 - i) % 360);
		else
			snprintf(buffer, sizeof(buffer), "%d", (int) (360 + 90 - 180.0 * angle / M_PI) % 360);
		snis_draw_3d_line(w, gc, instrumentecx,
				v3.v.x, v3.v.y, v3.v.z, v4.v.x, v4.v.y, v4.v.z);
		if (draw_numbers && dot >= 0)
			snis_draw_3d_string(instrumentecx, buffer, NANO_FONT, v3.v.x, v3.v.y, v3.v.z);
	}
}

static void sci_nav_add_tentacles(struct entity_context *ctx, struct snis_entity *o, struct entity *torso)
{
	int i, j, ntentacles;
	struct entity *parent;
	float shrinkage = 0.98;
	union quat orientation;
	float angle, y, z;
	union quat rotation;
	ntentacles = (o->id % 3) + 3;
	quat_init_axis(&orientation, 0, 1, 0, -10.0 * M_PI / 180.0);
	quat_init_axis(&rotation, 1, 0, 0, -2.0 * M_PI / ntentacles);

	for (i = 0; i < ntentacles; i++) {
		angle = i * (2.0 * M_PI / ntentacles);
		parent = torso;
		float tentacle_length = 40.0;
		float tentacle_scale = 1.1;
		for (j = 0; j < NTENTACLE_SEGMENTS; j++) {
			struct entity *e;
			if (j == 0) {
				y = 20.0 * sin(angle);
				z = 20.0 * cos(angle);
			} else {
				y = 0.0;
				z = 0.0;
			}
			e = add_entity(ctx, tentacle_segment_mesh,
				1.0 * tentacle_length, y, z, SPACEMONSTER_COLOR);
			if (e) {
				orientation = *entity_get_orientation(o->tsd.spacemonster.tentacle[i][j]);
				update_entity_orientation(e, &orientation);
				update_entity_parent(ctx, e, parent);
				update_entity_scale(e, tentacle_scale);
				update_entity_material(e, &spacemonster_tentacle_material);
				update_entity_non_uniform_scale(e, 1.0, tentacle_scale, tentacle_scale);
				parent = e;
			} else {
				break;
			}
			tentacle_length *= shrinkage;
			tentacle_scale *= shrinkage;
		}
	}
}

static void draw_3d_nav_display(GtkWidget *w, GdkGC *gc)
{
	static struct mesh *ring_mesh = 0;
	static struct mesh *radar_ring_mesh[8] = { 0 };
	static struct mesh *heading_ind_line_mesh = 0;
	static int current_zoom = 0;
	/* struct entity *targeted_entity = NULL; */
	struct entity *science_entity = NULL;

	if (!ring_mesh) {
		ring_mesh = init_circle_mesh(0, 0, 1, 180, 2.0f * M_PI);
		radar_ring_mesh[0] = init_radar_circle_xz_plane_mesh(0, 0, 0.4, 0, 0);
		radar_ring_mesh[1] = init_radar_circle_xz_plane_mesh(0, 0, 0.6, 18, 0.2);
		radar_ring_mesh[2] = init_radar_circle_xz_plane_mesh(0, 0, 0.8, 18, 0.2);
		radar_ring_mesh[3] = init_radar_circle_xz_plane_mesh(0, 0, 1.0, 36, 0.2);
		radar_ring_mesh[4] = init_radar_circle_xz_plane_mesh(0, 0, 0.3, 36, 0.1);
		radar_ring_mesh[5] = init_radar_circle_xz_plane_mesh(0, 0, 0.2, 18, 0.1);
		radar_ring_mesh[6] = init_radar_circle_xz_plane_mesh(0, 0, 0.1, 18, 0.05);
		radar_ring_mesh[7] = init_radar_circle_xz_plane_mesh(0, 0, 0.05, 0, 0.05);
		heading_ind_line_mesh = init_line_mesh(0.7, 0, 0, 1, 0, 0);
	}

	struct snis_entity *o;
	struct entity *e = NULL;
	double max_possible_screen_radius = 0.09 * XKNOWN_DIM;
	double screen_radius;
	double visible_distance;
	int science_style = RENDER_NORMAL;
	float cam_pos_scale = 1.0;
	int radar_ring_count = 4;

	if (!(o = find_my_ship()))
		return;

	static int last_timer = 0;
	int first_frame = (timer != last_timer+1);
	last_timer = timer;

	if (first_frame)
		current_zoom = o->tsd.ship.navzoom;
	else
		current_zoom = newzoom(current_zoom, o->tsd.ship.navzoom);

	const float min_xknown_pct = 0.001;
	const float max_xknown_pct = 0.080;

	float zoom_pct = (255.0 - current_zoom) / 255.0;

	/* 5th order curve fit correction to make each step of zoom the same relative percent

	   The idea is that for each tick of zoom the range will increase by a similar area of the screen,
	   with just straight calculation of zoom_pct * (max - min) + min each tick is the same range so
	   at the highest zoom level the change on screen is huge and at the lowest the change is almost nothing.

	   This correction is a numeric solution to balance this such that
		 per_tick_ratio = ((zoom + i) / 255 * (max - min) + min) / ((zoom + i + 1) / 255 * (max - min) + min)
	   is near constant for i = 0 to 255

	   This solution is specific to 255 levels of zoom, min=0.001, max=0.080
	*/
	zoom_pct = 0.0830099263 * zoom_pct +
		-0.2213190821 * zoom_pct * zoom_pct +
		1.5740944991 * zoom_pct * zoom_pct * zoom_pct +
		-2.1824091103 * zoom_pct * zoom_pct * zoom_pct * zoom_pct +
		1.7448656221 * zoom_pct * zoom_pct * zoom_pct * zoom_pct * zoom_pct;

	screen_radius = ((zoom_pct * (max_xknown_pct-min_xknown_pct)) + min_xknown_pct) * XKNOWN_DIM;
	visible_distance = (max_possible_screen_radius * o->tsd.ship.power_data.sensors.i) / 255.0;

	const float radius_fadeout_percent = 0.1;
	float ship_radius = ship_mesh_map[o->tsd.ship.shiptype]->radius;
	double ship_scale = 1.0 + zoom_pct * (XKNOWN_DIM*max_xknown_pct*0.05/ship_radius); /*5% of radius at max */

	union vec3 ship_pos = {{o->x, o->y, o->z}};
	union vec3 ship_normal = {{0, 1, 0}};
	quat_rot_vec_self(&ship_normal, &o->orientation);

	static union quat cam_orientation = IDENTITY_QUAT_INITIALIZER;
	if (first_frame) {
		quat_copy(&cam_orientation, &o->orientation);
	} else {
		quat_nlerp(&cam_orientation, &cam_orientation, &o->orientation, 0.1 / (use_60_fps + 1));
	}

	union vec3 camera_up = {{0,1,0}};
	quat_rot_vec_self(&camera_up, &cam_orientation);

	static float nav_camera_pos_factor = 1.0;
	float new_nav_camera_pos_factor;
	switch (nav_camera_mode) {
	case 0:
		new_nav_camera_pos_factor = 1.0;
		cam_pos_scale = 1.0;
		radar_ring_count = 4;
		break;
	case 1:
		new_nav_camera_pos_factor = 0.5;
		cam_pos_scale = 0.90;
		radar_ring_count = 8;
		break;
	case 2:
		new_nav_camera_pos_factor = 0.25;
		cam_pos_scale = 0.90 * 0.90;
		radar_ring_count = 8;
		break;
	case 3:
		new_nav_camera_pos_factor = 0.125;
		cam_pos_scale = 0.90 * 0.90 * 0.90;
		radar_ring_count = 8;
		break;
	case 4:
		new_nav_camera_pos_factor = 1.6;
		cam_pos_scale = 1.0;
		radar_ring_count = 4;
		break;
	default:
		new_nav_camera_pos_factor = 1.0;
		radar_ring_count = 4;
		break;
	}
	float camera_pos_delta = 0.3 * (new_nav_camera_pos_factor - nav_camera_pos_factor);
	nav_camera_pos_factor += camera_pos_delta;

	/* rotate camera to be behind my ship */
	union vec3 camera_pos = { { -screen_radius * 1.85 * nav_camera_pos_factor,
				screen_radius * 0.85 * nav_camera_pos_factor, 0 } };
	float camera_pos_len = vec3_magnitude(&camera_pos);
	quat_rot_vec_self(&camera_pos, &cam_orientation);
	vec3_add_self(&camera_pos, &ship_pos);

	union vec3 camera_lookat = {{screen_radius*0.20, 0, 0}};
	quat_rot_vec_self(&camera_lookat, &cam_orientation);
	vec3_add_self(&camera_lookat, &ship_pos);

	camera_assign_up_direction(instrumentecx, camera_up.v.x, camera_up.v.y, camera_up.v.z);
	camera_set_pos(instrumentecx, camera_pos.v.x, camera_pos.v.y, camera_pos.v.z);
	camera_look_at(instrumentecx, camera_lookat.v.x, camera_lookat.v.y, camera_lookat.v.z);

	set_renderer(instrumentecx, WIREFRAME_RENDERER);
	camera_set_parameters(instrumentecx, 1.0, camera_pos_len+screen_radius*2,
				SCREEN_WIDTH, SCREEN_HEIGHT, ANGLE_OF_VIEW * M_PI / 180.0);
	calculate_camera_transform(instrumentecx);
	entity_context_set_hi_lo_poly_pixel_threshold(instrumentecx, low_poly_threshold);

	int in_nebula = 0;
	int i;

	draw_attitude_indicator_reticles(w, gc, o, &ship_normal, screen_radius);
	for (i = 0; i < radar_ring_count; ++i) {
		e = add_entity(instrumentecx, radar_ring_mesh[i], o->x - ship_normal.v.x, o->y - ship_normal.v.y,
			o->z - ship_normal.v.z, UI_COLOR(nav_ring));
		if (e) {
			update_entity_scale(e, screen_radius);
			update_entity_orientation(e, &o->orientation);
		}
	}

	const int vector_color[] = {
		UI_COLOR(nav_heading_vector),
		UI_COLOR(nav_weapon_vector),
		UI_COLOR(nav_science_vector),
		UI_COLOR(nav_comms_curr_vector),
		UI_COLOR(nav_comms_desired_vector),
	};
	char *arrow_label[] = { "HEAD", "WEAP", "SCI", "COMMS", "COMMS" };
	for (i = 0; i < 5; ++i) {
		int color = vector_color[i];

		union quat ind_orientation;
		switch (i) {
		case 0:
			ind_orientation = o->orientation;
			break;
		case 1:
			quat_mul(&ind_orientation, &o->orientation, &o->tsd.ship.weap_orientation);
			break;
		case 2: {
			union vec3 up = { { 0, 1, 0 } };
			union vec3 xaxis = { { 1, 0, 0 } };

			if (!curr_science_guy && curr_science_waypoint == (uint32_t) -1)
				continue;

			if (curr_science_guy) {
				union vec3 sci_guy = { { curr_science_guy->x - o->x,
							 curr_science_guy->y - o->y,
							 curr_science_guy->z - o->z, } };
				quat_from_u2v(&ind_orientation, &xaxis, &sci_guy, &up);
			} else {
				union vec3 sci_wp = { { sci_ui.waypoint[curr_science_waypoint][0]  - o->x,
							sci_ui.waypoint[curr_science_waypoint][1]  - o->y,
							sci_ui.waypoint[curr_science_waypoint][2]  - o->z, } };
				quat_from_u2v(&ind_orientation, &xaxis, &sci_wp, &up);
			}
			}
			break;
		case 3:
			if (vec3_magnitude(&o->tsd.ship.desired_hg_ant_aim) < 0.01)
				continue; /* antenna aiming is disabled */
			ind_orientation = o->tsd.ship.current_hg_ant_orientation;
			break;
		case 4:
		default: {
			union vec3 r = { { 1.0, 0.0, 0.0 } };
			if (vec3_magnitude(&o->tsd.ship.desired_hg_ant_aim) < 0.01)
				continue; /* antenna aiming is disabled */
			quat_from_u2v(&ind_orientation, &r, &o->tsd.ship.desired_hg_ant_aim, 0);
			break;
			}
		}

		/* heading arrow head */
		union vec3 ind_pos = { {screen_radius * nav_camera_pos_factor * 0.75, 0, 0} };
		quat_rot_vec_self(&ind_pos, &ind_orientation);
		vec3_add_self(&ind_pos, &ship_pos);
		e = add_entity(instrumentecx, heading_indicator_mesh, ind_pos.v.x, ind_pos.v.y, ind_pos.v.z, color);
		if (e) {
			update_entity_scale(e, heading_indicator_mesh->radius *
						(screen_radius / 100.0) * nav_camera_pos_factor * 0.75);
			update_entity_orientation(e, &ind_orientation);
			set_render_style(e, RENDER_NORMAL);
			entity_set_user_data(e, &arrow_label[i]);
			draw_nav_contact_offset_and_ring(o, &ship_pos, &ship_normal, e, &ind_pos);
		}

		/* heading arrow tail */
		ind_pos.v.x = screen_radius * 0.65 * nav_camera_pos_factor * 0.75;
		ind_pos.v.y = 0.0;
		ind_pos.v.z = 0.0;
		quat_rot_vec_self(&ind_pos, &ind_orientation);
		vec3_add_self(&ind_pos, &ship_pos);
		e = add_entity(instrumentecx, heading_indicator_tail_mesh,
					ind_pos.v.x, ind_pos.v.y, ind_pos.v.z, color);
		if (e) {
			update_entity_scale(e, heading_indicator_tail_mesh->radius *
							(screen_radius / 100.0) * nav_camera_pos_factor * 0.75);
			update_entity_orientation(e, &ind_orientation);
			set_render_style(e, RENDER_NORMAL);
			draw_nav_contact_offset_and_ring(o, &ship_pos, &ship_normal, e, &ind_pos);
		}

		/* heading arrow shaft */
		e = add_entity(instrumentecx, heading_ind_line_mesh, o->x, o->y, o->z, color);
		if (e) {
			update_entity_scale(e, screen_radius * nav_camera_pos_factor * 0.75);
			update_entity_orientation(e, &ind_orientation);
		}
	}

	/* draw some static in the region that we can't see because of sensor power */
	if (screen_radius > visible_distance) {
		union vec3 u, v;
		plane_vector_u_and_v_from_normal(&u, &v, &ship_normal);

		/* specs to draw is based on the area of the annulus */
		int specs = 500 * (screen_radius * screen_radius -
				visible_distance * visible_distance) / (screen_radius * screen_radius);

		sng_set_foreground(UI_COLOR(nav_static) < 0 ? GRAY + 192 : UI_COLOR(nav_static));
		for (i=0; i<specs; i++) {
			union vec3 point;
			random_point_in_3d_annulus(visible_distance, screen_radius, &ship_pos, &u, &v, &point);

			float sx, sy;
			if (!transform_point(instrumentecx, point.v.x, point.v.y, point.v.z, &sx, &sy)) {
				sng_draw_point(sx, sy);
			}
		}
	}

	/* Draw all the stuff */
	pthread_mutex_lock(&universe_mutex);

	/* add my ship */
	e = add_entity(instrumentecx, ship_mesh_map[o->tsd.ship.shiptype], o->x, o->y, o->z, UI_COLOR(nav_self));
	if (e) { /* Add turret to ship */
		struct entity *turret, *turret_base;
		union vec3 vertical = { { 0, 1, 0, }, };
		union quat elevation, azimuth;
		quat_decompose_swing_twist(&o->tsd.ship.weap_orientation, &vertical, &elevation, &azimuth);

		set_render_style(e, science_style);
		update_entity_scale(e, ship_scale * cam_pos_scale);
		update_entity_orientation(e, &o->orientation);
		turret = add_entity(instrumentecx, ship_turret_mesh,
					ship_scale * cam_pos_scale * -1.5, ship_scale * cam_pos_scale * 3.0, 0.0,
					UI_COLOR(nav_self));
		turret_base = add_entity(instrumentecx, ship_turret_base_mesh,
					ship_scale * cam_pos_scale * -1.5, ship_scale * cam_pos_scale * 3.0, 0.0,
					UI_COLOR(nav_self));
		if (turret) {
			update_entity_parent(instrumentecx, turret, e);
			update_entity_orientation(turret, &o->tsd.ship.weap_orientation);
		}
		if (turret_base) {
			update_entity_parent(instrumentecx, turret_base, e);
			update_entity_orientation(turret_base, &azimuth);
		}
	}

	struct material wireframe_material;
	material_init_wireframe_sphere_clip(&wireframe_material);
	wireframe_material.wireframe_sphere_clip.center = e;
	wireframe_material.wireframe_sphere_clip.radius = MIN(visible_distance, screen_radius);
	wireframe_material.wireframe_sphere_clip.radius_fade = radius_fadeout_percent;

	float display_radius = MIN(visible_distance, screen_radius) * (1.0 + radius_fadeout_percent);

	for (i = 0; i <= snis_object_pool_highest_object(pool); i++) {
		if (!go[i].alive)
			continue;
		if (go[i].id == my_ship_id)
			continue;
		if (go[i].type == OBJTYPE_LASERBEAM || go[i].type == OBJTYPE_TRACTORBEAM) {
			draw_3d_laserbeam(w, gc, instrumentecx, o, &go[i], display_radius);
			continue;
		}
		if (!go[i].entity)
			continue;

		float obj_radius = 0.0;
		struct mesh *obj_entity_mesh = entity_get_mesh(go[i].entity);
		if (obj_entity_mesh) {
			if (go[i].type == OBJTYPE_PLANET)
				obj_radius = obj_entity_mesh->radius * fabs(entity_get_scale(go[i].entity));
			else
				obj_radius = cam_pos_scale * obj_entity_mesh->radius *
						fabs(entity_get_scale(go[i].entity));
		}
		if (nav_entity_too_far_away(o->x, o->y, o->z, go[i].x, go[i].y, go[i].z,
						obj_radius, display_radius))
			continue; /* not close enough */

		if (in_nebula && snis_randn(1000) < 850)
			continue;

		struct entity *contact = 0;
		struct mesh *m = entity_get_mesh(go[i].entity);

		switch (go[i].type) {
		case OBJTYPE_EXPLOSION:
		case OBJTYPE_DERELICT:
		case OBJTYPE_NEBULA:
			break;
		case OBJTYPE_TORPEDO: {
				union quat orientation;
				random_quat(&orientation); /* make torpedo flip around randomly */
				contact = add_entity(instrumentecx, torpedo_nav_mesh, go[i].x, go[i].y, go[i].z,
							(timer & 0x04) == 0 ? UI_COLOR(nav_torpedo) : BLACK);
				if (contact) {
					update_entity_orientation(contact, &orientation);
					entity_set_user_data(contact, &go[i]); /* for debug */
					update_entity_scale(contact, cam_pos_scale * entity_get_scale(go[i].entity));
				}
			}
			break;
		case OBJTYPE_LASER: {
				contact = add_entity(instrumentecx, laserbeam_nav_mesh, go[i].x, go[i].y, go[i].z,
					UI_COLOR(nav_laser));
				if (contact) {
					set_render_style(contact, science_style | RENDER_BRIGHT_LINE | RENDER_NO_FILL);
					entity_set_user_data(contact, &go[i]); /* for debug */
					update_entity_scale(contact, cam_pos_scale * entity_get_scale(go[i].entity));
					update_entity_orientation(contact, entity_get_orientation(go[i].entity));
				}
			}
			break;
		case OBJTYPE_PLANET: {
				contact = add_entity(instrumentecx, low_poly_sphere_mesh,
							go[i].x, go[i].y, go[i].z, UI_COLOR(nav_planet));
				if (contact) {
					set_render_style(contact, science_style);
					entity_set_user_data(contact, &go[i]);
					update_entity_scale(contact, entity_get_scale(go[i].entity));
					update_entity_orientation(contact, entity_get_orientation(go[i].entity));
				}
				if (go[i].tsd.planet.ring) {
					struct entity *ring =
						add_entity(instrumentecx, nav_planetary_ring_mesh, 0, 0, 0,
								UI_COLOR(nav_planet));
					struct entity *ring2 =
						add_entity(instrumentecx, nav_planetary_ring_mesh, 0, 0, 0,
								UI_COLOR(nav_planet));
					if (ring) {
						update_entity_orientation(ring, &identity_quat);
						update_entity_parent(instrumentecx, ring, contact);
						update_entity_material(ring, &wireframe_material);
					}
					if (ring2) { /* Defeat backface culling by adding an upside down ring */
						union quat upside_down;
						quat_init_axis(&upside_down, 1, 0, 0, 180.0 * M_PI / 180.0);
						update_entity_orientation(ring2, &upside_down);
						update_entity_parent(instrumentecx, ring2, contact);
						update_entity_material(ring2, &wireframe_material);
					}
				}
			}
			break;
		case OBJTYPE_BLACK_HOLE: {
				contact = add_entity(instrumentecx, low_poly_sphere_mesh,
							go[i].x, go[i].y, go[i].z, UI_COLOR(nav_entity));
				if (contact) {
					set_render_style(contact, science_style);
					entity_set_user_data(contact, &go[i]);
					update_entity_scale(contact, cam_pos_scale * entity_get_scale(go[i].entity));
					update_entity_orientation(contact, entity_get_orientation(go[i].entity));
				}
			}
			break;
		case OBJTYPE_WORMHOLE:
		case OBJTYPE_MISSILE:
		case OBJTYPE_ASTEROID:
		case OBJTYPE_STARBASE:
		case OBJTYPE_WARPGATE:
		case OBJTYPE_SHIP2:
		case OBJTYPE_CARGO_CONTAINER:
		case OBJTYPE_SHIP1:
		case OBJTYPE_WARP_CORE:
		case OBJTYPE_SPACEMONSTER: {
				int color = UI_COLOR(nav_entity);
				if (go[i].type == OBJTYPE_SHIP2)
					color = UI_COLOR(nav_ship);
				else if (go[i].type == OBJTYPE_ASTEROID)
					color = UI_COLOR(nav_asteroid);
				else if (go[i].type == OBJTYPE_STARBASE)
					color = UI_COLOR(nav_starbase);
				else if (go[i].type == OBJTYPE_WARPGATE)
					color = UI_COLOR(nav_warpgate);
				contact = add_entity(instrumentecx, m, go[i].x, go[i].y, go[i].z, color);
				if (contact) {
					set_render_style(contact, science_style);
					entity_set_user_data(contact, &go[i]);
					update_entity_scale(contact, cam_pos_scale * entity_get_scale(go[i].entity));
					update_entity_orientation(contact, entity_get_orientation(go[i].entity));
				}
				/* Is it better to have monsters invisible on nav for the element of surprise? */
				if (go[i].type == OBJTYPE_SPACEMONSTER)
					sci_nav_add_tentacles(instrumentecx, &go[i], contact);
				if (go[i].type == OBJTYPE_MISSILE)
					update_entity_color(contact,
							(timer & 0x04) == 0 ? UI_COLOR(nav_torpedo) : BLACK);
			}
#if 0
			if (o->tsd.ship.ai[0].u.attack.victim_id != -1 && go[i].id == o->tsd.ship.ai[0].u.attack.victim_id)
				targeted_entity = contact;
#endif
			break;
		} /* end switch */

		if (contact) {
			int draw_contact_offset_and_ring = 1;
			float contact_scale = 1.0;

			if (curr_science_guy == &go[i])
				science_entity = contact;

			update_entity_material(contact, &wireframe_material);

			switch (go[i].type) {
			case OBJTYPE_PLANET:
				contact_scale = ((255.0 - current_zoom) / 255.0) * 0.0 + 1.0;
				break;
			case OBJTYPE_BLACK_HOLE:
				contact_scale = ((255.0 - current_zoom) / 255.0) * 0.0 + 1.0;
				contact_scale *= 0.9 + 0.1 * sin(4.0 * (timer % 90) * M_PI / 180.0);
				break;
			case OBJTYPE_WARPGATE:
				contact_scale = ((255.0 - current_zoom) / 255.0) * 4.0 + 1.0;
				break;
			case OBJTYPE_STARBASE:
				contact_scale = ((255.0 - current_zoom) / 255.0) * 4.0 + 1.0;
				break;
			case OBJTYPE_ASTEROID:
				contact_scale = ((255.0 - current_zoom) / 255.0) * 3.0 + 1.0;
				break;
			case OBJTYPE_WORMHOLE:
				contact_scale = ((255.0 - current_zoom) / 255.0) * 2.0 + 1.0;
				break;
			case OBJTYPE_TORPEDO:
			case OBJTYPE_MISSILE:
				contact_scale = 20.0 * ((255.0 - current_zoom) / 255.0) * 2.0 + 1.0;
				break;
			case OBJTYPE_LASER:
				contact_scale = ((255.0 - current_zoom) / 255.0) * 3.0 + 1.0;
				draw_contact_offset_and_ring = 0;
				break;
			case OBJTYPE_SHIP2:
			case OBJTYPE_SHIP1:
				contact_scale = ship_mesh_map[SHIP_CLASS_CRUISER]->radius /
							entity_get_mesh(contact)->radius * ship_scale;
				break;
			case OBJTYPE_CARGO_CONTAINER:
				contact_scale = ((255.0 - current_zoom) / 255.0) * 20.0 + 1.0;
				break;
			case OBJTYPE_WARP_CORE:
				contact_scale = ((255.0 - current_zoom) / 255.0) * 120.0 + 1.0;
				break;
			}

			/* update the scale based on current scale */
			if (contact) {
				if (go[i].type == OBJTYPE_PLANET || go[i].type == OBJTYPE_BLACK_HOLE)
					update_entity_scale(contact, entity_get_scale(contact) * contact_scale);
				else
					update_entity_scale(contact, cam_pos_scale *
								 entity_get_scale(contact) * contact_scale);
			}

			/* add line from center disk to contact in z axis */
			if (draw_contact_offset_and_ring && contact) {
				union vec3 contact_pos = { { go[i].x, go[i].y, go[i].z } };
				draw_nav_contact_offset_and_ring(o, &ship_pos, &ship_normal, contact, &contact_pos);
			}
		}
	}

	draw_3d_nav_waypoints(o, &ship_pos, &ship_normal, display_radius, current_zoom, science_style);
	render_entities(instrumentecx);

	/* Draw labels on objects and waypoints */
	sng_set_foreground(UI_COLOR(nav_entity_label));
	for (i = 0; i <= get_entity_count(instrumentecx); i++) {
		float sx, sy;
		char buffer[100];
		struct entity *e;
		struct snis_entity *o;
		double *wp;
		char **arrowlabel;
		int waypoint_diff, arrow_label_diff;

		e = get_entity(instrumentecx, i);
		if (!e)
			continue;
		if (!entity_onscreen(e))
			continue;
		o = entity_get_user_data(e);
		if (!o)
			continue; 

		/* Check if it is a waypoint rather than an object.
		 * This is a little gross, we do it by checking if the
		 * pointer is within the array containing the waypoints.
		 */
		wp = (double *) o;
		waypoint_diff = wp - &sci_ui.waypoint[0][0];
		arrowlabel = (char **) o;
		arrow_label_diff = arrowlabel - &arrow_label[0];
		if (waypoint_diff >= 0 && waypoint_diff < 3 * MAXWAYPOINTS) {
			/* It is a waypoint */
			waypoint_diff = waypoint_diff / 3;
			entity_get_screen_coords(e, &sx, &sy);
			snprintf(buffer, sizeof(buffer), "WAYPOINT-%02d", waypoint_diff);
			sng_abs_xy_draw_string(buffer, NANO_FONT, sx + 10, sy - 15);
		} else if (arrow_label_diff >= 0 && arrow_label_diff < 4) { /* Arrow head */
			entity_get_screen_coords(e, &sx, &sy);
			snprintf(buffer, sizeof(buffer), "%s", *arrowlabel);
			sng_abs_xy_draw_string(buffer, NANO_FONT, sx + 10, sy - 15);
		} else {
			/* It is not a waypoint */
			entity_get_screen_coords(e, &sx, &sy);
			if (o->sdata.science_data_known) {
				snprintf(buffer, sizeof(buffer), "%s", o->sdata.name);
				sng_abs_xy_draw_string(buffer, NANO_FONT, sx + 10, sy - 15);
			}
		}
	}

#if 0
	/* Draw targeting indicator on 3d nav screen */
	if (targeted_entity && entity_onscreen(targeted_entity)) {
		float sx, sy;

		entity_get_screen_coords(targeted_entity, &sx, &sy);
		draw_targeting_indicator(w, gc, sx, sy, TARGETING_COLOR, 0, 1.0f, 2.0f);
	}
#endif
	if (science_entity && entity_onscreen(science_entity)) {
		float sx, sy;

		entity_get_screen_coords(science_entity, &sx, &sy);
		draw_targeting_indicator(w, gc, sx, sy, UI_COLOR(nav_science_select), 0, 1.0f, 2.0f);
	}

	/* Draw warp field error indicator */
	if (warp_field_error > 0) {
		char buf[64];
		float error_fraction = warp_field_error / (3.0 * PLAYER_WARP_SPINUP_TIME);
		float jitter = 0.2 * error_fraction * (snis_randn(100) - 50);
		sng_set_foreground(UI_COLOR(nav_entity_label));
		snprintf(buf, sizeof(buf), "WARP FIELD ERROR: %3.2f%%", error_fraction * 100.0 + jitter);
		sng_abs_xy_draw_string(buf, NANO_FONT, txx(540), txy(130));
	}

	pthread_mutex_unlock(&universe_mutex);

	remove_all_entity(instrumentecx);

	if (current_altitude < MAX_PLANET_RADIUS * 1.5) {
		char buf[25];
		snprintf(buf, sizeof(buf), "ALTITUDE: %5.1f", current_altitude);
		sng_abs_xy_draw_string(buf, NANO_FONT, txx(290), txy(15));
	}

}

static void show_navigation(GtkWidget *w)
{
	char buf[100];
	struct snis_entity *o;
	double display_heading;
	static int current_zoom = 0;
	union euler ypr;

	sng_set_foreground(UI_COLOR(nav_text));

	if (!(o = find_my_ship()))
		return;

	if (nav_has_computer_button)
		ui_unhide_widget(nav_ui.computer_button);
	else
		ui_hide_widget(nav_ui.computer_button);

	snis_slider_set_input(nav_ui.warp_slider, o->tsd.ship.power_data.warp.r1/255.0 );
	snis_slider_set_input(nav_ui.navzoom_slider, o->tsd.ship.navzoom/255.0 );
	snis_slider_set_input(nav_ui.throttle_slider, o->tsd.ship.power_data.impulse.r1/255.0 );
	snis_button_set_color(nav_ui.reverse_button, o->tsd.ship.reverse ?
				UI_COLOR(nav_reverse_button) : UI_COLOR(nav_button));

	current_zoom = newzoom(current_zoom, o->tsd.ship.navzoom);
	snprintf(buf, sizeof(buf), "POSITION:");
	sng_abs_xy_draw_string(buf, NANO_FONT, txx(75), txy(15));
	snprintf(buf, sizeof(buf), "(%5.2lf,", o->x);
	sng_abs_xy_draw_string(buf, NANO_FONT, txx(120), txy(15));
	snprintf(buf, sizeof(buf), "%5.2lf,", o->y);
	sng_abs_xy_draw_string(buf, NANO_FONT, txx(175), txy(15));
	snprintf(buf, sizeof(buf), "%5.2lf)", o->z);
	sng_abs_xy_draw_string(buf, NANO_FONT, txx(230), txy(15));

	double display_mark;
	to_snis_heading_mark(&o->orientation, &display_heading, &display_mark);
	snprintf(buf, sizeof(buf), "HEADING: %3.1lf", radians_to_degrees(display_heading));
	sng_abs_xy_draw_string(buf, NANO_FONT, txx(5), txy(190));
	snprintf(buf, sizeof(buf), "MARK: %3.1lf", radians_to_degrees(display_mark));
	sng_abs_xy_draw_string(buf, NANO_FONT, txx(5), txy(202));

	quat_to_euler(&ypr, &o->orientation);	
	sng_set_foreground(UI_COLOR(nav_text));
	draw_nav_main_idiot_lights(w, gc, o, UI_COLOR(nav_warning));
	draw_orientation_trident(w, gc, o, txx(31), txy(111), txx(31));
	switch (o->tsd.ship.nav_mode) {
	case NAV_MODE_STARMAP:
		draw_3d_nav_starmap(w, gc);
		break;
	case NAV_MODE_NORMAL:
	default:
		draw_3d_nav_display(w, gc);
		break;
	}
	show_common_screen(w, "NAV");
}

static void main_engineering_button_pressed(void *x)
{
	displaymode = DISPLAYMODE_ENGINEERING;
}

static void robot_forward_button_pressed(void *x)
{
	damcon_dirkey(0, -1);
}

static void robot_backward_button_pressed(void *x)
{
	damcon_dirkey(0, 1);
}

static void robot_left_button_pressed(void *x)
{
	damcon_dirkey(-1, 0);
}

static void robot_right_button_pressed(void *x)
{
	damcon_dirkey(1, 0);
}

static void robot_gripper_button_pressed(void *x)
{
	queue_to_server(snis_opcode_pkt("b", OPCODE_REQUEST_ROBOT_GRIPPER));
}

static void robot_auto_button_pressed(void *x)
{
	queue_to_server(snis_opcode_pkt("bb", OPCODE_ROBOT_AUTO_MANUAL, DAMCON_ROBOT_FULLY_AUTONOMOUS));
}

static void robot_manual_button_pressed(void *x)
{
	queue_to_server(snis_opcode_pkt("bb", OPCODE_ROBOT_AUTO_MANUAL, DAMCON_ROBOT_MANUAL_MODE));
}

static void eject_warp_core_button_pressed(void *x)
{
	queue_to_server(snis_opcode_pkt("b", OPCODE_EJECT_WARP_CORE));
	return;
}

static void damcon_custom_button_pressed(void *x)
{
	queue_to_server(snis_opcode_pkt("bb", OPCODE_CUSTOM_BUTTON, OPCODE_CUSTOM_BUTTON_SUBCMD_DAMCON));
}

static void init_damcon_ui(void)
{
	damcon_ui.engineering_button = snis_button_init(txx(630), txy(550), -1, txy(25),
			"ENGINEERING", UI_COLOR(damcon_button),
			NANO_FONT, main_engineering_button_pressed, (void *) 0);
	snis_button_set_sound(damcon_ui.engineering_button, UISND11);
	damcon_ui.robot_controls = snis_label_init(txx(630), txy(30), "ROBOT CONTROLS",
							UI_COLOR(damcon_button), NANO_FONT);
	damcon_ui.robot_forward_button = snis_button_init(txx(650), txy(60), txx(90), txy(25),
			"FORWARD", UI_COLOR(damcon_button), NANO_FONT, robot_forward_button_pressed, (void *) 0);
	damcon_ui.robot_left_button = snis_button_init(txx(630), txy(100), txx(25), txy(25),
			"L", UI_COLOR(damcon_button), NANO_FONT, robot_left_button_pressed, (void *) 0);
	damcon_ui.robot_right_button = snis_button_init(txx(740), txy(100), txx(25), txy(25),
			"R", UI_COLOR(damcon_button), NANO_FONT, robot_right_button_pressed, (void *) 0);
	damcon_ui.robot_backward_button = snis_button_init(txx(650), txy(140), txx(90), txy(25), "BACKWARD",
							UI_COLOR(damcon_button), NANO_FONT,
							robot_backward_button_pressed, (void *) 0);
	damcon_ui.robot_gripper_button = snis_button_init(txx(650), txy(180), txx(90), txy(25), "GRIPPER",
							UI_COLOR(damcon_button), NANO_FONT,
							robot_gripper_button_pressed, (void *) 0);
	snis_button_set_sound(damcon_ui.robot_gripper_button, UISND24);
	damcon_ui.custom_button = snis_button_init(txx(650), txy(220), txx(90), txy(25),
						"CUSTOM BUTTON", UI_COLOR(damcon_button), NANO_FONT,
						damcon_custom_button_pressed, (void *) 0);
	snis_button_set_sound(damcon_ui.custom_button, UISND13);
	damcon_ui.robot_auto_button = snis_button_init(txx(400), txy(30), txx(90), txy(25),
				"AUTO", UI_COLOR(damcon_button), NANO_FONT, robot_auto_button_pressed, (void *) 0);
	snis_button_set_sound(damcon_ui.robot_gripper_button, UISND25);
	damcon_ui.robot_manual_button = snis_button_init(txx(500), txy(30), txx(90), txy(25), "MANUAL",
							UI_COLOR(damcon_selected_button), NANO_FONT,
							robot_manual_button_pressed, (void *) 0);
	snis_button_set_sound(damcon_ui.robot_manual_button, UISND25);
	damcon_ui.eject_warp_core_button = snis_button_init(txx(300), txy(30), txx(90), txy(25),
						"EJECT WARP CORE", UI_COLOR(damcon_button), NANO_FONT,
						eject_warp_core_button_pressed, (void *) 0);
	snis_button_set_sound(damcon_ui.eject_warp_core_button, UISND12); /* FIXME: custom sound here */

	ui_add_button(damcon_ui.engineering_button, DISPLAYMODE_DAMCON, "SWITCH TO ENGINEERING SCREEN");
	ui_add_button(damcon_ui.robot_forward_button, DISPLAYMODE_DAMCON, "MOVE THE ROBOT FORWARD");
	ui_add_button(damcon_ui.robot_left_button, DISPLAYMODE_DAMCON, "TURN THE ROBOT LEFT");
	ui_add_button(damcon_ui.robot_right_button, DISPLAYMODE_DAMCON, "TURN THE ROBOT RIGHT");
	ui_add_button(damcon_ui.robot_backward_button, DISPLAYMODE_DAMCON, "MOVE THE ROBOT BACKWARD");
	ui_add_button(damcon_ui.robot_gripper_button, DISPLAYMODE_DAMCON, "OPERATE THE ROBOT GRIPPER");
	ui_add_button(damcon_ui.custom_button, DISPLAYMODE_DAMCON, "CUSTOM BUTTON");
	ui_add_button(damcon_ui.robot_auto_button, DISPLAYMODE_DAMCON, "SELECT AUTONOMOUS ROBOT OPERATION");
	ui_add_button(damcon_ui.robot_manual_button, DISPLAYMODE_DAMCON, "SELECT MANUAL ROBOT CONTROL");
	ui_add_label(damcon_ui.robot_controls, DISPLAYMODE_DAMCON);
	ui_add_button(damcon_ui.eject_warp_core_button, DISPLAYMODE_DAMCON, "EJECT THE WARP CORE");
}

static int process_custom_button(void)
{
	int rc;
	uint8_t subcmd, active_buttons;
	uint8_t buffer[32];
	uint8_t button_index;
	char text[16];

	rc = read_and_unpack_buffer(buffer, "b", &subcmd);
	if (rc != 0)
		return rc;
	switch (subcmd) {
	case OPCODE_CUSTOM_BUTTON_SUBCMD_ACTIVE_LIST:
		rc = read_and_unpack_buffer(buffer, "b", &active_buttons);
		if (rc != 0)
			return rc;
		active_custom_buttons = active_buttons;
		if (active_custom_buttons & (1 << OPCODE_CUSTOM_BUTTON_SUBCMD_NAV))
			ui_unhide_widget(nav_ui.custom_button);
		else
			ui_hide_widget(nav_ui.custom_button);
		if (active_custom_buttons & (1 << OPCODE_CUSTOM_BUTTON_SUBCMD_WEAPONS))
			ui_unhide_widget(weapons.custom_button);
		else
			ui_hide_widget(weapons.custom_button);
		if (active_custom_buttons & (1 << OPCODE_CUSTOM_BUTTON_SUBCMD_ENG))
			ui_unhide_widget(eng_ui.custom_button);
		else
			ui_hide_widget(eng_ui.custom_button);
		if (active_custom_buttons & (1 << OPCODE_CUSTOM_BUTTON_SUBCMD_DAMCON))
			ui_unhide_widget(damcon_ui.custom_button);
		else
			ui_hide_widget(damcon_ui.custom_button);
		if (active_custom_buttons & (1 << OPCODE_CUSTOM_BUTTON_SUBCMD_SCIENCE))
			ui_unhide_widget(sci_ui.custom_button);
		else
			ui_hide_widget(sci_ui.custom_button);
		if (active_custom_buttons & (1 << OPCODE_CUSTOM_BUTTON_SUBCMD_COMMS))
			ui_unhide_widget(comms_ui.custom_button);
		else
			ui_hide_widget(comms_ui.custom_button);
		break;
	case OPCODE_CUSTOM_BUTTON_SUBCMD_UPDATE_TEXT:
		memset(text, 0, sizeof(text));
		rc = read_and_unpack_buffer(buffer, "bbbbbbbbbbbbbbbb", &button_index,
			&text[0], &text[1], &text[2], &text[3],
			&text[4], &text[5], &text[6], &text[7],
			&text[8], &text[9], &text[10], &text[11],
			&text[12], &text[13], &text[14]);
		if (rc != 0)
			return rc;
		switch (button_index) {
		case 0:
			snis_button_set_label(nav_ui.custom_button, text);
			break;
		case 1:
			snis_button_set_label(weapons.custom_button, text);
			break;
		case 2:
			snis_button_set_label(eng_ui.custom_button, text);
			break;
		case 3:
			snis_button_set_label(damcon_ui.custom_button, text);
			break;
		case 4:
			snis_button_set_label(sci_ui.custom_button, text);
			break;
		case 5:
			snis_button_set_label(comms_ui.custom_button, text);
			break;
		default:
			return -1;
		}
		break;
	default:
		return -1;
	}
	return 0;
}

static int process_console_op(void)
{
	int rc;
	uint8_t subcmd;
	int color;
	uint8_t u8color;
	uint8_t buffer[DEMON_CONSOLE_MSG_MAX];

	rc = read_and_unpack_buffer(buffer, "b", &subcmd);
	if (rc != 0)
		return rc;
	switch (subcmd) {
	case OPCODE_CONSOLE_SUBCMD_ADD_TEXT:
		rc = read_and_unpack_buffer(buffer, "b", &u8color);
		if (rc != 0)
			return rc;
		color = u8color;
		if (color >= NCOLORS)
			color = UI_COLOR(demon_default);
		memset(buffer, 0, sizeof(buffer));
		rc = snis_readsocket(gameserver_sock, buffer, sizeof(buffer));
		if (rc != 0)
			return rc;
		buffer[DEMON_CONSOLE_MSG_MAX - 1] = '\0';
		print_demon_console_color_msg(color, "%s", buffer);
		break;
	default:
		return -1;
	}
	return 0;
}

static void enforce_displaymode_matches_roles(void)
{
	int i, newdisplaymode;

	if ((1 << displaymode) & role)
		return;

	newdisplaymode = ROLE_MAIN;
	for (i = 0; i < 32; i++) {
		if ((1 << i) & role) {
			newdisplaymode = i;
			break;
		}
	}
	displaymode = newdisplaymode;
}

static int process_client_config(void)
{
	int rc;
	uint8_t subcmd, buffer[10];
	uint32_t permitted_roles;

	rc = read_and_unpack_buffer(buffer, "b", &subcmd);
	if (rc != 0)
		return rc;
	switch (subcmd) {
	case OPCODE_CLIENT_SET_PERMITTED_ROLES:
		rc = read_and_unpack_buffer(buffer, "w", &permitted_roles);
		if (rc != 0)
			return rc;
		role = permitted_roles;
		enforce_displaymode_matches_roles();
		break;
	default:
		return -1;
	}
	return 0;
}

static void damcon_button_pressed(void *x)
{
	displaymode = DISPLAYMODE_DAMCON;
}

static void silence_alarms_pressed(void *x)
{
	struct snis_entity *o = find_my_ship();
	if (!o)
		return;
	transmit_adjust_control_input(!o->tsd.ship.alarms_silenced,
		OPCODE_ADJUST_CONTROL_SILENCE_ALARMS);
}

static void eng_custom_button_pressed(void *x)
{
	queue_to_server(snis_opcode_pkt("bb", OPCODE_CUSTOM_BUTTON, OPCODE_CUSTOM_BUTTON_SUBCMD_ENG));
}

static void eng_deploy_flare_button_pressed(void *x)
{
	transmit_adjust_control_input(0, OPCODE_ADJUST_CONTROL_DEPLOY_FLARE);
}

static void preset_button_pressed(void *button_ptr_ptr)
{
	struct button **button = button_ptr_ptr;
	int selection;

	if (*button) {
		selection = button - &eng_ui.preset_buttons[0];
		if ((selection >= 0) && (selection < ENG_PRESET_NUMBER)) {
			transmit_apply_preset((uint8_t) selection);
			snis_button_set_color(eng_ui.preset_buttons[eng_ui.selected_preset], UI_COLOR(eng_button));
			snis_button_set_color(eng_ui.preset_buttons[selection], UI_COLOR(eng_selected_button));
			eng_ui.selected_preset = selection;
			snis_button_enable(eng_ui.preset_save_button);
		}
	}
}

static void preset_button_long_pressed(void *preset)
{
	int selection;

	if (!preset)
		return;
	selection = (struct button **) preset - &eng_ui.preset_buttons[0];
	if (selection < 0 || selection >= ARRAYSIZE(eng_ui.preset_buttons))
		return;
	transmit_save_preset(selection);
	preset_button_pressed(preset);
}

static void preset_save_button_pressed(void *x)
{
	transmit_save_preset(eng_ui.selected_preset);
}

static void init_engineering_ui(void)
{
	int i, x, y, r, xinc, yinc;
	int dm = DISPLAYMODE_ENGINEERING;
	int color = UI_COLOR(eng_gauge);
	const int ccolor = UI_COLOR(eng_coolant_meter); /* coolant color */
	const int tcolor = UI_COLOR(eng_temperature); /* temperature color */
	const int coolant_inc = txy(12);
	const int sh = 0.02 * SCREEN_HEIGHT; /* slider height */
	const int sw = 0.1875 * SCREEN_WIDTH; /* slider width */
	const int powersliderlen = 0.225 * SCREEN_WIDTH;
	const int coolantsliderlen = 0.1875 * SCREEN_WIDTH;
	const int s2x = 0.4375 * SCREEN_WIDTH; /* x start of 2nd bank of sliders */
	struct engineering_ui *eu = &eng_ui;
	char preset_txt[100];

	r = SCREEN_WIDTH / 16;
	eng_ui.gauge_radius = r;
	y = r + txy(50);
	x = r * 1.05;
	xinc = (2.0 * r) * 1.1;
	yinc = txy(36);

	eu->selected_subsystem = -1;
	eu->amp_gauge = gauge_init(x, y, r, 0.0, 100.0, -120.0 * M_PI / 180.0,
			120.0 * 2.0 * M_PI / 180.0, UI_COLOR(eng_gauge_needle), color,
			10, "AMPS", sample_power_model_current);
	gauge_set_fonts(eu->amp_gauge, PICO_FONT, PICO_FONT);
	x += xinc;
	eu->voltage_gauge = gauge_init(x, y, r, 0.0, 200.0, -120.0 * M_PI / 180.0,
			120.0 * 2.0 * M_PI / 180.0, UI_COLOR(eng_gauge_needle), color,
			10, "VOLTS", sample_power_model_voltage);
	gauge_set_fonts(eu->voltage_gauge, PICO_FONT, PICO_FONT);
	x += xinc;
	eu->temp_gauge = gauge_init(x, y, r, 0.0, 100.0, -120.0 * M_PI / 180.0,
			120.0 * 2.0 * M_PI / 180.0, UI_COLOR(eng_gauge_needle), color,
			10, "TEMP", sample_temp);
	gauge_set_fonts(eu->temp_gauge, PICO_FONT, PICO_FONT);
	x += xinc;
	eu->fuel_gauge = gauge_init(x, y, r, 0.0, 100.0, -120.0 * M_PI / 180.0,
			120.0 * 2.0 * M_PI / 180.0, UI_COLOR(eng_gauge_needle), color,
			10, "FUEL", sample_fuel);
	gauge_set_fonts(eu->fuel_gauge, PICO_FONT, PICO_FONT);
	x += xinc;
	eu->oxygen_gauge = gauge_init(x, y, r, 0.0, 100.0, -120.0 * M_PI / 180.0,
			120.0 * 2.0 * M_PI / 180.0, UI_COLOR(eng_gauge_needle), color,
			10, "O2", sample_oxygen);
	gauge_set_fonts(eu->oxygen_gauge, PICO_FONT, PICO_FONT);

	int gx1 = SCREEN_WIDTH - eng_ui.gauge_radius * 5;
	int gy1 = SCREEN_HEIGHT * 0.02;
	int gx2 = SCREEN_WIDTH * 0.90;
	int gy2 = gy1 + eng_ui.gauge_radius * 2.5;
	eu->shield_control_slider = snis_slider_init(gx1, gy2 + sh * 3,
				gx2 - gx1, sh, UI_COLOR(eng_power_meter), "SHIELDS",
				"0", "100", 0.0, 255.0, sample_power_data_shields_current,
				do_shieldadj);
	/* make shield slider have less fuzz just for variety */
	snis_slider_set_fuzz(eu->shield_control_slider, 1);

	y += eng_ui.gauge_radius + txy(30);
	color = UI_COLOR(eng_button);
	x = txx(20);
	for (i = 0; i < ENG_PRESET_NUMBER; ++i) {
		snprintf(preset_txt, 2, "%d", i + 1);
		eu->preset_buttons[i] = snis_button_init(x, y, -1, -1, preset_txt, color, NANO_FONT,
						preset_button_pressed, &eu->preset_buttons[i]);
		snis_button_set_sound(eu->preset_buttons[i], UISND12);
		snis_button_set_long_press_function(eu->preset_buttons[i],
						preset_button_long_pressed, &eu->preset_buttons[i]);
		x += snis_button_get_width(eu->preset_buttons[i]) + txx(5);
	}
	eu->preset_save_button = snis_button_init(snis_button_get_x(eu->preset_buttons[ENG_PRESET_NUMBER-1]) +
						snis_button_get_width(eu->preset_buttons[ENG_PRESET_NUMBER-1]) + txx(5),
						y, -1, -1, "SAVE",
						color, NANO_FONT, preset_save_button_pressed, (void *) 0);
	snis_button_disable(eu->preset_save_button);
	snis_button_set_disabled_color(eu->preset_save_button, UI_COLOR(eng_disabled));
	eu->silence_alarms = snis_button_init(snis_button_get_x(eu->preset_save_button) +
						snis_button_get_width(eu->preset_save_button) + txx(5),
						y, -1, -1, "UNSILENCE ALARMS",
						color, NANO_FONT, silence_alarms_pressed, (void *) 0);
	snis_button_set_sound(eu->silence_alarms, UISND13);
	eu->damcon_button = snis_button_init(txx(630), txy(550), -1, txy(25),
						"DAMAGE CONTROL", color,
						NANO_FONT, damcon_button_pressed, (void *) 0);
	eu->custom_button = snis_button_init(snis_button_get_x(eu->silence_alarms) +
						snis_button_get_width(eu->silence_alarms) + txx(5),
						y, -1, -1, "CUSTOM BUTTON",
						color, NANO_FONT, eng_custom_button_pressed, (void *) 0);
	snis_button_set_sound(eu->custom_button, UISND14);
	eu->deploy_flare = snis_button_init(snis_button_get_x(eu->custom_button) +
						snis_button_get_width(eu->custom_button) + txx(5),
						y, -1, -1, "DEPLOY FLARE",
						color, NANO_FONT, eng_deploy_flare_button_pressed, (void *) 0);
	snis_button_set_sound(eu->deploy_flare, UISND14);
	color = UI_COLOR(eng_power_meter);
	eu->shield_slider = snis_slider_init(20, y += yinc, powersliderlen, sh, color,
				"PWR SHIELDS", "0", "100", 0.0, 255.0,
				sample_power_data_shields_current, do_shields_pwr);
	/* make shield slider have less fuzz just for variety */
	snis_slider_set_fuzz(eu->shield_slider, 1);
	snis_slider_set_label_font(eu->shield_slider, NANO_FONT);
	eu->shield_coolant_slider = snis_slider_init(20, y + coolant_inc, coolantsliderlen, sh,
				ccolor, "COOLANT", "0", "100", 0.0, 255.0,
				sample_coolant_data_shields_current, do_shields_coolant);
	snis_slider_set_label_font(eu->shield_coolant_slider, NANO_FONT);
	eu->phaserbanks_slider = snis_slider_init(20, y += yinc, powersliderlen, sh, color,
				"PWR PHASERS", "0", "100", 0.0, 255.0,
				sample_power_data_phasers_current, do_phaserbanks_pwr);
	snis_slider_set_fuzz(eu->phaserbanks_slider, 4);
	snis_slider_set_label_font(eu->phaserbanks_slider, NANO_FONT);
	eu->phaserbanks_coolant_slider = snis_slider_init(20, y + coolant_inc, coolantsliderlen, sh,
				ccolor, "COOLANT", "0", "100", 0.0, 255.0,
				sample_coolant_data_phasers_current, do_phaserbanks_coolant);
	snis_slider_set_label_font(eu->phaserbanks_coolant_slider, NANO_FONT);
	eu->comm_slider = snis_slider_init(20, y += yinc, powersliderlen, sh, color,
				"PWR COMMS", "0", "100", 0.0, 255.0,
				sample_power_data_comms_current, do_comms_pwr);
	snis_slider_set_fuzz(eu->comm_slider, 3);
	snis_slider_set_label_font(eu->comm_slider, NANO_FONT);
	eu->comm_coolant_slider = snis_slider_init(20, y + coolant_inc, coolantsliderlen, sh,
				ccolor, "COOLANT", "0", "100", 0.0, 255.0,
				sample_coolant_data_comms_current, do_comms_coolant);
	snis_slider_set_label_font(eu->comm_coolant_slider, NANO_FONT);
	eu->sensors_slider = snis_slider_init(20, y += yinc, powersliderlen, sh, color,
				"PWR SENSORS", "0", "100", 0.0, 255.0,
				sample_power_data_sensors_current, do_sensors_pwr);
	/* Make sensors slider more fuzzy just for variety */
	snis_slider_set_fuzz(eu->sensors_slider, 7);
	snis_slider_set_label_font(eu->sensors_slider, NANO_FONT);
	eu->sensors_coolant_slider = snis_slider_init(20, y + coolant_inc, coolantsliderlen, sh,
				ccolor, "COOLANT", "0", "100", 0.0, 255.0,
				sample_coolant_data_sensors_current, do_sensors_coolant);
	snis_slider_set_label_font(eu->sensors_coolant_slider, NANO_FONT);
	eu->impulse_slider = snis_slider_init(20, y += yinc, powersliderlen, sh, color,
				"PWR IMPULSE DR", "0", "100", 0.0, 255.0,
				sample_power_data_impulse_current, do_impulse_pwr);
	snis_slider_set_fuzz(eu->impulse_slider, 3);
	snis_slider_set_label_font(eu->impulse_slider, NANO_FONT);
	eu->impulse_coolant_slider = snis_slider_init(20, y + coolant_inc, coolantsliderlen, sh,
				ccolor, "COOLANT", "0", "100", 0.0, 255.0,
				sample_coolant_data_impulse_current, do_impulse_coolant);
	snis_slider_set_label_font(eu->impulse_coolant_slider, NANO_FONT);
	eu->warp_slider = snis_slider_init(20, y += yinc, powersliderlen, sh, color,
				"PWR WARP DR", "0", "100", 0.0, 255.0,
				sample_power_data_warp_current, do_warp_pwr);
	snis_slider_set_fuzz(eu->warp_slider, 2);
	snis_slider_set_label_font(eu->warp_slider, NANO_FONT);
	eu->warp_coolant_slider = snis_slider_init(20, y + coolant_inc, coolantsliderlen, sh,
				ccolor, "COOLANT", "0", "100", 0.0, 255.0,
				sample_coolant_data_warp_current, do_warp_coolant);
	snis_slider_set_label_font(eu->warp_coolant_slider, NANO_FONT);
	eu->maneuvering_slider = snis_slider_init(20, y += yinc, powersliderlen, sh, color,
				"PWR MANEUVERING", "0", "100", 0.0, 255.0,
				sample_power_data_maneuvering_current, do_maneuvering_pwr);
	snis_slider_set_fuzz(eu->maneuvering_slider, 3);
	snis_slider_set_label_font(eu->maneuvering_slider, NANO_FONT);
	eu->maneuvering_coolant_slider = snis_slider_init(20, y + coolant_inc, coolantsliderlen, sh,
				ccolor, "COOLANT", "0", "100", 0.0, 255.0,
				sample_coolant_data_maneuvering_current, do_maneuvering_coolant);
	snis_slider_set_label_font(eu->maneuvering_coolant_slider, NANO_FONT);
	eu->tractor_slider = snis_slider_init(20, y += yinc, powersliderlen, sh, color,
				"PWR TRACTOR", "0", "100", 0.0, 255.0,
				sample_power_data_tractor_current, do_tractor_pwr);
	snis_slider_set_fuzz(eu->tractor_slider, 2);
	snis_slider_set_label_font(eu->tractor_slider, NANO_FONT);
	eu->tractor_coolant_slider = snis_slider_init(20, y + coolant_inc, coolantsliderlen, sh,
				ccolor, "COOLANT", "0", "100", 0.0, 255.0,
				sample_coolant_data_tractor_current, do_tractor_coolant);
	snis_slider_set_label_font(eu->tractor_coolant_slider, NANO_FONT);
	eu->lifesupport_slider = snis_slider_init(20, y += yinc, powersliderlen, sh, color,
				"PWR LIFE SUPPORT", "0", "100", 0.0, 255.0,
				sample_power_data_lifesupport_current, do_lifesupport_pwr);
	snis_slider_set_fuzz(eu->lifesupport_slider, 2);
	snis_slider_set_label_font(eu->lifesupport_slider, NANO_FONT);
	eu->lifesupport_coolant_slider = snis_slider_init(20, y + coolant_inc, coolantsliderlen, sh,
				ccolor, "COOLANT", "0", "100", 0.0, 255.0,
				sample_coolant_data_lifesupport_current, do_lifesupport_coolant);
	snis_slider_set_label_font(eu->lifesupport_coolant_slider, NANO_FONT);
	ui_add_slider(eu->shield_slider, dm, "SHIELD POWER LIMIT CONTROL");
	ui_add_slider(eu->shield_coolant_slider, dm, "SHIELD COOLANT CONTROL");
	ui_add_slider(eu->phaserbanks_slider, dm, "PHASER POWER LIMIT CONTROL");
	ui_add_slider(eu->phaserbanks_coolant_slider, dm, "PHASER COOLANT CONTROL");
	ui_add_slider(eu->comm_slider, dm, "COMMS POWER LIMIT CONTROL");
	ui_add_slider(eu->comm_coolant_slider, dm, "COMMS COOLANT CONTROL");
	ui_add_slider(eu->sensors_slider, dm, "SENSORS POWER LIMIT CONTROL");
	ui_add_slider(eu->sensors_coolant_slider, dm, "SENSORS COOLANT CONTROL");
	ui_add_slider(eu->impulse_slider, dm, "IMPULSE DRIVE POWER LIMIT CONTROL");
	ui_add_slider(eu->impulse_coolant_slider, dm, "IMPULSE COOLANT CONTROL");
	ui_add_slider(eu->warp_slider, dm, "WARP POWER LIMIT CONTROL");
	ui_add_slider(eu->warp_coolant_slider, dm, "WARP COOLANT CONTROL");
	ui_add_slider(eu->maneuvering_slider, dm, "MANEUVERING THRUSTER POWER LIMIT CONTROL");
	ui_add_slider(eu->maneuvering_coolant_slider, dm, "MANEUVERING THRUSTER COOLANT CONTROL");
	ui_add_slider(eu->tractor_slider, dm, "TRACTOR BEAM POWER CONTROL");
	ui_add_slider(eu->tractor_coolant_slider, dm, "TRACTOR BEAM COOLANT CONTROL");
	ui_add_slider(eu->lifesupport_slider, dm, "LIFE SUPPORT SYSTEM POWER LIMIT CONTROL");
	ui_add_slider(eu->lifesupport_coolant_slider, dm, "LIFE SUPPORT SYSTEM COOLANT CONTROL");
	ui_add_slider(eu->shield_control_slider, dm, "SHIELD POWER CONTROL");
	ui_add_gauge(eu->amp_gauge, dm);
	ui_set_widget_tooltip(eu->amp_gauge, "CURRENT INDICATOR\n"
				"INDICATES AMOUNT OF CURRENT\n"
				"FLOWING FROM POWER SOURCE");
	ui_add_gauge(eu->voltage_gauge, dm);
	ui_set_widget_tooltip(eu->voltage_gauge, "VOLTAGE INDICATOR\n"
				"INDICATES AMOUNT OF VOLTAGE\n"
				"ACROSS POWER SOURCE TERMINALS");
	ui_add_gauge(eu->fuel_gauge, dm);
	ui_set_widget_tooltip(eu->fuel_gauge, "FUEL INDICATOR\n"
				"INDICATES AMOUNT OF\n"
				"FUEL REMAINING");
	ui_add_gauge(eu->temp_gauge, dm);
	ui_set_widget_tooltip(eu->temp_gauge, "TEMPERATURE INDICATOR\n"
				"INDICATES TEMPERATURE\n"
				"OF POWER SOURCE");
	ui_add_gauge(eu->oxygen_gauge, dm);
	ui_set_widget_tooltip(eu->oxygen_gauge, "OXYGEN INDICATOR\n"
				"INDICATES AMOUNT OF OXYGEN\n"
				"REMAINING UNTIL DANGEROUSLY LOW");
	ui_add_button(eu->damcon_button, dm, "SWITCH TO THE DAMAGE CONTROL SCREEN");
	for (i = 0; i < ENG_PRESET_NUMBER; ++i) {
		snprintf(preset_txt, sizeof(preset_txt), "SELECT ENGINEERING PRESET %d\n"
			"PRESS AND HOLD FOR 1 SECOND TO SAVE\n"
			"CURRENT VALUES IN THIS PRESET", i + 1);
		ui_add_button(eu->preset_buttons[i], dm, preset_txt);
	}
	ui_add_button(eu->preset_save_button, dm, "SAVE CURRENT VALUES IN THE ACTIVE PRESET");
	ui_add_button(eu->silence_alarms, dm, "SILENCE/UNSILENCE ENGINEERING SYSTEM ALARMS");
	ui_add_button(eu->custom_button, dm, "CUSTOM BUTTON");
	ui_add_button(eu->deploy_flare, dm, "DEPLOY ANTI-MISSILE FLARE");

	y = eng_ui.gauge_radius * 2 + txy(50) + txy(30);
	eu->shield_damage = snis_slider_init(s2x, y += yinc, sw, sh, color, "SHIELD STATUS", "0", "100",
				0.0, 100.0, sample_shield_damage, NULL);
	snis_slider_set_label_font(eu->shield_damage, NANO_FONT);
	eu->shield_temperature = snis_slider_init(s2x, y + coolant_inc, sw, sh, tcolor,
				"TEMPERATURE", "0", "100", 0.0, 100.0,
				sample_shield_temperature, NULL);
	snis_slider_set_label_font(eu->shield_temperature, NANO_FONT);
	snis_slider_set_color_scheme(eu->shield_temperature, 1);	
	eu->phaser_banks_damage = snis_slider_init(s2x, y += yinc, sw, sh, color, "PHASER STATUS", "0", "100",
				0.0, 100.0, sample_phaser_banks_damage, NULL);
	snis_slider_set_label_font(eu->phaser_banks_damage, NANO_FONT);
	eu->phaser_banks_temperature = snis_slider_init(s2x, y + coolant_inc, sw, sh, tcolor,
				"TEMPERATURE", "0", "100", 0.0, 100.0,
				sample_phaser_banks_temperature, NULL);
	snis_slider_set_label_font(eu->phaser_banks_temperature, NANO_FONT);
	snis_slider_set_color_scheme(eu->phaser_banks_temperature, 1);	
	eu->comms_damage = snis_slider_init(s2x, y += yinc, sw, sh, color, "COMMS STATUS", "0", "100",
				0.0, 100.0, sample_comms_damage, NULL);
	snis_slider_set_label_font(eu->comms_damage, NANO_FONT);
	eu->comms_temperature = snis_slider_init(s2x, y + coolant_inc, sw, sh, tcolor,
				"TEMPERATURE", "0", "100", 0.0, 100.0,
				sample_comms_temperature, NULL);
	snis_slider_set_label_font(eu->comms_temperature, NANO_FONT);
	snis_slider_set_color_scheme(eu->comms_temperature, 1);	
	eu->sensors_damage = snis_slider_init(s2x, y += yinc, sw, sh, color, "SENSORS STATUS", "0", "100",
				0.0, 100.0, sample_sensors_damage, NULL);
	snis_slider_set_label_font(eu->sensors_damage, NANO_FONT);
	eu->sensors_temperature = snis_slider_init(s2x, y + coolant_inc, sw, sh, tcolor,
				"TEMPERATURE", "0", "100", 0.0, 100.0,
				sample_sensors_temperature, NULL);
	snis_slider_set_label_font(eu->sensors_temperature, NANO_FONT);
	snis_slider_set_color_scheme(eu->sensors_temperature, 1);	
	eu->impulse_damage = snis_slider_init(s2x, y += yinc, sw, sh, color, "IMPULSE STATUS", "0", "100",
				0.0, 100.0, sample_impulse_damage, NULL);
	snis_slider_set_label_font(eu->impulse_damage, NANO_FONT);
	eu->impulse_temperature = snis_slider_init(s2x, y + coolant_inc, sw, sh, tcolor,
				"TEMPERATURE", "0", "100", 0.0, 100.0,
				sample_impulse_temperature, NULL);
	snis_slider_set_label_font(eu->impulse_temperature, NANO_FONT);
	snis_slider_set_color_scheme(eu->impulse_temperature, 1);	
	eu->warp_damage = snis_slider_init(s2x, y += yinc, sw, sh, color, "WARP STATUS", "0", "100",
				0.0, 100.0, sample_warp_damage, NULL);
	snis_slider_set_label_font(eu->warp_damage, NANO_FONT);
	eu->warp_temperature = snis_slider_init(s2x, y + coolant_inc, sw, sh, tcolor,
				"TEMPERATURE", "0", "100", 0.0, 100.0,
				sample_warp_temperature, NULL);
	snis_slider_set_label_font(eu->warp_temperature, NANO_FONT);
	snis_slider_set_color_scheme(eu->warp_temperature, 1);	
	eu->maneuvering_damage = snis_slider_init(s2x, y += yinc, sw, sh, color, "MANEUVERING STATUS", "0", "100",
				0.0, 100.0, sample_maneuvering_damage, NULL);
	snis_slider_set_label_font(eu->maneuvering_damage, NANO_FONT);
	eu->maneuvering_temperature = snis_slider_init(s2x, y + coolant_inc, sw, sh, tcolor,
				"TEMPERATURE", "0", "100", 0.0, 100.0,
				sample_maneuvering_temperature, NULL);
	snis_slider_set_label_font(eu->maneuvering_temperature, NANO_FONT);
	snis_slider_set_color_scheme(eu->maneuvering_temperature, 1);	
	eu->tractor_damage = snis_slider_init(s2x, y += yinc, sw, sh, color, "TRACTOR STATUS", "0", "100",
				0.0, 100.0, sample_tractor_damage, NULL);
	snis_slider_set_label_font(eu->tractor_damage, NANO_FONT);
	eu->tractor_temperature = snis_slider_init(s2x, y + coolant_inc, sw, sh, tcolor,
				"TEMPERATURE", "0", "100", 0.0, 100.0,
				sample_tractor_temperature, NULL);
	snis_slider_set_label_font(eu->tractor_temperature, NANO_FONT);
	snis_slider_set_color_scheme(eu->tractor_temperature, 1);	
	eu->lifesupport_damage = snis_slider_init(s2x, y += yinc, sw, sh, color, "LIFE SUPPORT STATUS", "0", "100",
				0.0, 100.0, sample_lifesupport_damage, NULL);
	snis_slider_set_label_font(eu->lifesupport_damage, NANO_FONT);
	eu->lifesupport_temperature = snis_slider_init(s2x, y + coolant_inc, sw, sh, tcolor,
				"TEMPERATURE", "0", "100", 0.0, 100.0,
				sample_lifesupport_temperature, NULL);
	snis_slider_set_label_font(eu->lifesupport_temperature, NANO_FONT);
	snis_slider_set_color_scheme(eu->lifesupport_temperature, 1);
	ui_add_slider(eu->shield_damage, dm, NULL); /* These all have specialized dynamic tooltips */
	ui_add_slider(eu->impulse_damage, dm, NULL);
	ui_add_slider(eu->warp_damage, dm, NULL);
	ui_add_slider(eu->maneuvering_damage, dm, NULL);
	ui_add_slider(eu->phaser_banks_damage, dm, NULL);
	ui_add_slider(eu->sensors_damage, dm, NULL);
	ui_add_slider(eu->comms_damage, dm, NULL);
	ui_add_slider(eu->tractor_damage, dm, NULL);
	ui_add_slider(eu->lifesupport_damage, dm, NULL);
	ui_add_slider(eu->shield_temperature, dm, NULL);
	ui_add_slider(eu->impulse_temperature, dm, NULL);
	ui_add_slider(eu->warp_temperature, dm, NULL);
	ui_add_slider(eu->maneuvering_temperature, dm, NULL);
	ui_add_slider(eu->phaser_banks_temperature, dm, NULL);
	ui_add_slider(eu->sensors_temperature, dm, NULL);
	ui_add_slider(eu->comms_temperature, dm, NULL);
	ui_add_slider(eu->tractor_temperature, dm, NULL);
	ui_add_slider(eu->lifesupport_temperature, dm, NULL);
}

static void draw_tooltip(int mousex, int mousey, char *tooltip)
{
	float bbx1, bby1, bbx2, bby2, width, height;
	int x, y;

	x = sng_pixelx_to_screenx(mousex);
	y = sng_pixely_to_screeny(mousey);

	sng_string_bounding_box(tooltip, PICO_FONT, &bbx1, &bby1, &bbx2, &bby2);
	width = fabsf(bbx2 - bbx1) + 20;
	height = fabsf(bby2 - bby1) + 20;

	if (x + width > SCREEN_WIDTH)
		x = SCREEN_WIDTH - width;
	if (x < 0)
		x = 0;
	y = y + txy(20);
	if (y + height > SCREEN_HEIGHT)
		y = SCREEN_HEIGHT - height;

	sng_set_foreground(BLACK);
	snis_draw_rectangle(1, x, y, width, height);
	sng_set_foreground(UI_COLOR(tooltip));
	snis_draw_rectangle(0, x, y, width, height);
	sng_abs_xy_draw_string(tooltip, PICO_FONT, x + txx(3), y + txy(12));
}

static void show_engineering_damage_report(GtkWidget *w, int subsystem)
{
	struct snis_damcon_entity *o;
	int i, x, y, count;
	char msg[50];

	/* in different order on screen... barf. */
	const int sysmap[] = { 0, 4, 5, 6, 1, 3, 2, 7, 8 };
	/* TODO BUILD ASSERT that sysmap is correct size */

	if (subsystem < 0 || subsystem >= ARRAYSIZE(sysmap))
		return;

	y = 0.3333 * SCREEN_HEIGHT + sysmap[subsystem] * 0.06666 * SCREEN_HEIGHT;
	x = 0.375 * SCREEN_WIDTH;

	sng_set_foreground(BLACK);
	snis_draw_rectangle(1, x - 5, y - 5, 0.55 * SCREEN_WIDTH, 0.10833 * SCREEN_HEIGHT);
	sng_set_foreground(UI_COLOR(eng_temperature));
	snis_draw_rectangle(0, x - 5, y - 5, 0.55 * SCREEN_WIDTH, 0.10833 * SCREEN_HEIGHT);
	count = 0;
	for (i = 0; i <= snis_object_pool_highest_object(damcon_pool); i++) {
		o = &dco[i];
		if (o->type != DAMCON_TYPE_PART)
			continue;
		if (o->tsd.part.system != subsystem)
			continue;
		if ((float) o->tsd.part.damage > 0.75f * 255.0f)
			sng_set_foreground(UI_COLOR(eng_warning_status));
		else if ((float) o->tsd.part.damage > 0.5f * 255.0f)
			sng_set_foreground(UI_COLOR(eng_caution_status));
		else
			sng_set_foreground(UI_COLOR(eng_good_status));
		snprintf(msg, sizeof(msg), "%3.2f%%: %s",
			(1.0f - (float) o->tsd.part.damage / 255.0f) * 100.0f,
			damcon_part_name(o->tsd.part.system, o->tsd.part.part));
		sng_abs_xy_draw_string(msg, NANO_FONT, x + 10, y + 10);
		y = y + 20;
		count++;
		if (count >= DAMCON_PARTS_PER_SYSTEM)
			break;
	}
}

static int engineering_warnings_active()
{
	return snis_slider_alarm_triggered(eng_ui.shield_damage) ||
		snis_slider_alarm_triggered(eng_ui.phaser_banks_damage) ||
		snis_slider_alarm_triggered(eng_ui.comms_damage) ||
		snis_slider_alarm_triggered(eng_ui.sensors_damage) ||
		snis_slider_alarm_triggered(eng_ui.impulse_damage) ||
		snis_slider_alarm_triggered(eng_ui.warp_damage) ||
		snis_slider_alarm_triggered(eng_ui.maneuvering_damage) ||
		snis_slider_alarm_triggered(eng_ui.tractor_damage) ||
		snis_slider_alarm_triggered(eng_ui.lifesupport_damage);
}

static void show_engineering(GtkWidget *w)
{
	struct snis_entity *o;
	int gx1, gy1, gx2, gy2;
	static int alarms_were_active = 0;
	static double fuel_consumption_rate, fuel_time_left;
	static double last_fuel_time;
	static double last_fuel_value;
	double fuel_rate_per_sec, fuel_secs, fuel_mins;
	double fuel_time;

	if (!(o = find_my_ship()))
		return;

	snis_slider_set_input(eng_ui.shield_slider, o->tsd.ship.power_data.shields.r2 / 255.0);
	snis_slider_set_input(eng_ui.phaserbanks_slider, o->tsd.ship.power_data.phasers.r2 / 255.0);
	snis_slider_set_input(eng_ui.comm_slider, o->tsd.ship.power_data.comms.r2 / 255.0);
	snis_slider_set_input(eng_ui.sensors_slider, o->tsd.ship.power_data.sensors.r2 / 255.0);
	snis_slider_set_input(eng_ui.impulse_slider, o->tsd.ship.power_data.impulse.r2 / 255.0);
	snis_slider_set_input(eng_ui.warp_slider, o->tsd.ship.power_data.warp.r2 / 255.0);
	snis_slider_set_input(eng_ui.maneuvering_slider, o->tsd.ship.power_data.maneuvering.r2 / 255.0);
	snis_slider_set_input(eng_ui.tractor_slider, o->tsd.ship.power_data.tractor.r2 / 255.0);
	snis_slider_set_input(eng_ui.lifesupport_slider, o->tsd.ship.power_data.lifesupport.r2 / 255.0);
	snis_slider_set_input(eng_ui.shield_control_slider, o->tsd.ship.power_data.shields.r1/255.0);

	snis_slider_set_input(eng_ui.shield_coolant_slider, o->tsd.ship.coolant_data.shields.r2 / 255.0);
	snis_slider_set_input(eng_ui.phaserbanks_coolant_slider, o->tsd.ship.coolant_data.phasers.r2 / 255.0);
	snis_slider_set_input(eng_ui.comm_coolant_slider, o->tsd.ship.coolant_data.comms.r2 / 255.0);
	snis_slider_set_input(eng_ui.sensors_coolant_slider, o->tsd.ship.coolant_data.sensors.r2 / 255.0);
	snis_slider_set_input(eng_ui.impulse_coolant_slider, o->tsd.ship.coolant_data.impulse.r2 / 255.0);
	snis_slider_set_input(eng_ui.warp_coolant_slider, o->tsd.ship.coolant_data.warp.r2 / 255.0);
	snis_slider_set_input(eng_ui.maneuvering_coolant_slider, o->tsd.ship.coolant_data.maneuvering.r2 / 255.0);
	snis_slider_set_input(eng_ui.tractor_coolant_slider, o->tsd.ship.coolant_data.tractor.r2 / 255.0);
	snis_slider_set_input(eng_ui.lifesupport_coolant_slider, o->tsd.ship.coolant_data.lifesupport.r2 / 255.0);

	if (o->tsd.ship.alarms_silenced)
		snis_button_set_label(eng_ui.silence_alarms, "UNSILENCE ALARMS");
	else
		snis_button_set_label(eng_ui.silence_alarms, "SILENCE ALARMS");

	if ((timer % 30) == 0) { /* Trigger alarm buzzer if something's badly broken */
		if (engineering_warnings_active()) {
			if (!o->tsd.ship.alarms_silenced)
				wwviaudio_add_sound(ALARM_BUZZER);
			alarms_were_active = 1;
		} else {
			/* No alarms currently active */
			if (alarms_were_active) {
				alarms_were_active = 0;
				/* Auto-unsilence alarms when no alarms active. */
				if (o->tsd.ship.alarms_silenced)
					transmit_adjust_control_input(0,
						OPCODE_ADJUST_CONTROL_SILENCE_ALARMS);
			}
		}
	}

	/* idiot lights for low power of various systems */

	float sc_x, sc_y, sc_length, sc_height;
	snis_slider_get_location(eng_ui.shield_control_slider, &sc_x, &sc_y, &sc_length, &sc_height);

	float sc_x_center = sc_x + sc_length / 2.0;
	float sc_y_center = sc_y + sc_height / 2.0;


	sng_set_foreground(UI_COLOR(eng_warning));
	if (o->tsd.ship.power_data.shields.r2 < idiot_light_threshold)
		sng_center_xy_draw_string("LOW SHIELD POWER", NANO_FONT, sc_x_center, sc_y_center);
	else if (o->tsd.ship.power_data.shields.i < idiot_light_threshold)
		sng_center_xy_draw_string("SHIELDS ARE OFF", NANO_FONT, sc_x_center, sc_y_center);

	if (o->tsd.ship.fuel < UINT32_MAX * 0.1) { /* 10% */
		float fg_x, fg_y, fg_r;
		gauge_get_location(eng_ui.fuel_gauge, &fg_x, &fg_y, &fg_r);

		float fg_x_center = fg_x;
		float fg_y_center = fg_y - fg_r * 1.25;

		sng_set_foreground(UI_COLOR(eng_warning));
		if (o->tsd.ship.fuel < UINT32_MAX * 0.01) { /* 1% */
			sng_center_xy_draw_string("OUT Of FUEL", NANO_FONT, fg_x_center, fg_y_center);
		} else {
			sng_center_xy_draw_string("LOW FUEL", NANO_FONT, fg_x_center, fg_y_center);
		}
	}

	if (o->tsd.ship.oxygen < UINT32_MAX * 0.1) { /* 10% */
		float o2_x, o2_y, o2_r;
		gauge_get_location(eng_ui.oxygen_gauge, &o2_x, &o2_y, &o2_r);

		float o2_x_center = o2_x;
		float o2_y_center = o2_y - o2_r * 1.25;

		sng_set_foreground(UI_COLOR(eng_warning));
		if (o->tsd.ship.oxygen < UINT32_MAX * 0.01) { /* 1% */
			sng_center_xy_draw_string("OUT Of OXYGEN", NANO_FONT, o2_x_center, o2_y_center);
		} else {
			sng_center_xy_draw_string("LOW OXYGEN", NANO_FONT, o2_x_center, o2_y_center);
		}
	}


	gx1 = SCREEN_WIDTH - eng_ui.gauge_radius * 5;
	gy1 = SCREEN_HEIGHT * 0.02;
	gx2 = SCREEN_WIDTH * 0.98;
	gy2 = gy1 + eng_ui.gauge_radius * 2.5;
	sng_set_foreground(UI_COLOR(eng_science_graph));
	draw_science_graph(w, o, o, gx1, gy1, gx2, gy2);

	if (o->sdata.shield_strength < 15) {
		sng_set_foreground(UI_COLOR(eng_warning));
		sng_center_xy_draw_string("SHIELDS ARE DOWN", TINY_FONT, (gx1 + gx2) / 2.0, gy1 + (gy2 - gy1) / 3.0);
	}

	/* Fuel consumption rate and time remaining until empty indicators */
	if ((timer % 30) == 0) { /* Only calculate once per second to smooth it out */
		fuel_time = (universe_timestamp() - last_fuel_time);
		if (fuel_time != 0) {
			fuel_consumption_rate = (last_fuel_value - o->tsd.ship.fuel) / fuel_time;
			if (fuel_consumption_rate != 0)
				fuel_time_left = o->tsd.ship.fuel / fuel_consumption_rate;
		}
		last_fuel_value = o->tsd.ship.fuel;
		last_fuel_time = universe_timestamp();
	}
	if (fuel_consumption_rate > 0) {
		char buffer[100];
		fuel_rate_per_sec = 100000.0 * (fuel_consumption_rate  * UNIVERSE_TICKS_PER_SECOND) / (double) UINT_MAX;
		snprintf(buffer, sizeof(buffer), "FUEL/SEC: %.2f", fuel_rate_per_sec);
		sng_set_foreground(UI_COLOR(eng_warning));
		sng_abs_xy_draw_string(buffer, PICO_FONT, txx(350), txy(192));

		fuel_mins = floor(fuel_time_left / (60 * UNIVERSE_TICKS_PER_SECOND));
		fuel_secs = (fuel_time_left - (fuel_mins * 60 * UNIVERSE_TICKS_PER_SECOND)) / UNIVERSE_TICKS_PER_SECOND;

		snprintf(buffer, sizeof(buffer), "TIME TO EMPTY %.0f:%02.0f", fuel_mins, fuel_secs);
		sng_abs_xy_draw_string(buffer, PICO_FONT, txx(350), txy(192) + font_lineheight[PICO_FONT]);
	}

	show_common_screen(w, "ENGINEERING");
}

#if 0
static inline double screenx_to_damconx(double x)
{
	return x - damconscreenx0 - damconscreenxdim / 2.0 + *damconscreenx;
}

static inline double screeny_to_damcony(double y)
{
	return y - damconscreeny0 - damconscreenydim / 2.0 + *damconscreeny;
}
#endif

static inline int damconx_to_screenx(double x)
{
	return x + damconscreenx0 + damconscreenxdim / 2.0 - *damconscreenx;
}

static inline int damcony_to_screeny(double y)
{
	return y + damconscreeny0 + damconscreenydim / 2.0 - *damconscreeny;
}

static int on_damcon_screen(struct snis_damcon_entity *o, struct my_vect_obj *v)
{
	float left, right, top, bottom;
	//int ox, oy;

	if (damconscreenx == NULL || damconscreeny == NULL)
		return 0;

	//ox = damconx_to_screenx(o->x);
	//oy = damcony_to_screeny(o->y);

	left = *damconscreenx - damconscreenxdim / 2.0;
	if (v->bbx2 + o->x < left)
		return 0;
	right = left + damconscreenxdim;
	if (v->bbx1 + o->x > right)
		return 0;
	top = *damconscreeny - damconscreenydim / 2.0;
	if (v->bby2 + o->y < top)
		return 0;
	bottom = top + damconscreenydim;
	if (v->bby1 + o->y > bottom)
		return 0;
	return 1;
}

static void draw_damcon_arena_borders(GtkWidget *w)
{
	int y1, x1;

	sng_set_foreground(UI_COLOR(damcon_arena_border));
	/* top border */
	y1 = damcony_to_screeny(-DAMCONYDIM / 2.0);
	if (y1 >= damconscreeny0 &&
		y1 <= damconscreeny0 + damconscreenydim) {
		snis_draw_line(damconscreenx0, y1,
				damconscreenx0 + damconscreenxdim, y1);
	}

	/* bottom border */
	y1 = damcony_to_screeny(DAMCONYDIM / 2.0);
	if (y1 >= damconscreeny0 &&
		y1 <= damconscreeny0  + damconscreenydim) {
		snis_draw_line(damconscreenx0, y1,
				damconscreenx0 + damconscreenxdim, y1);
	}

	/* left border */
	x1 = damconx_to_screenx(-DAMCONXDIM / 2.0);
	if (x1 > damconscreenx0 &&
		x1 < damconscreenx0 + damconscreenxdim) {
		snis_draw_line(x1, damconscreeny0,
				x1, damconscreeny0 + damconscreenydim);
	}

	/* right border */
	x1 = damconx_to_screenx(DAMCONXDIM / 2.0);
	if (x1 > damconscreenx0 &&
		x1 < damconscreenx0 + damconscreenxdim) {
		snis_draw_line(x1, damconscreeny0,
				x1, damconscreeny0 + damconscreenydim);
	}
}

static void draw_damcon_robot(GtkWidget *w, struct snis_damcon_entity *o)
{
	int x, y;
	int byteangle = (int) (o->heading * 128.0 / M_PI);

	x = o->x + damconscreenx0 + damconscreenxdim / 2.0 - *damconscreenx;
	y = o->y + damconscreeny0 + damconscreenydim / 2.0 - *damconscreeny;
	sng_set_foreground(UI_COLOR(damcon_robot));
	sng_draw_vect_obj(&damcon_robot_spun[byteangle], x, y);
}

static void draw_damcon_system(GtkWidget *w, struct snis_damcon_entity *o)
{
	int x, y;

	if (!on_damcon_screen(o, &placeholder_system))
		return;
	
	x = damconx_to_screenx(o->x);
	y = damcony_to_screeny(o->y);
	sng_set_foreground(UI_COLOR(damcon_system));
	sng_draw_vect_obj(&placeholder_system, x, y);
	sng_abs_xy_draw_string(damcon_system_name(o->type),
				NANO_FONT, x + 75, y);
}

static void draw_damcon_socket_or_part(GtkWidget *w, struct snis_damcon_entity *o, int color)
{
	int x, y;
	char msg[20];

	if (!on_damcon_screen(o, &placeholder_socket))
		return;
	x = damconx_to_screenx(o->x);
	y = damcony_to_screeny(o->y);
	snprintf(msg, sizeof(msg), "%d %d", o->tsd.socket.system, o->tsd.socket.part);
	sng_set_foreground(color);
	sng_draw_vect_obj(&placeholder_socket, x, y);
	sng_abs_xy_draw_string(msg, NANO_FONT, x - 10, y);
}

static void draw_damcon_socket(GtkWidget *w, struct snis_damcon_entity *o)
{
	if (o->tsd.socket.contents_id == DAMCON_SOCKET_EMPTY)
		draw_damcon_socket_or_part(w, o, UI_COLOR(damcon_socket));
}


static struct snis_damcon_entity *damcon_robot_entity = NULL;
static void draw_damcon_part(GtkWidget *w, struct snis_damcon_entity *o)
{
	int x, y;
	char msg[80];
	int byteangle = (int) (o->heading * 64.0 / M_PI);
	int dist = 1000000;

	if (!on_damcon_screen(o, &placeholder_part_spun[byteangle]))
		return;
	x = damconx_to_screenx(o->x);
	y = damcony_to_screeny(o->y);

	if (damcon_robot_entity)
		dist = hypot(o->x - damcon_robot_entity->x,
				o->y - damcon_robot_entity->y);
	else
		dist = 1000000;
	if (dist < 150)
		snprintf(msg, sizeof(msg), "%s%s",
			damcon_part_name(o->tsd.part.system, o->tsd.part.part),
			o->tsd.part.damage == 255 ? " (BADLY DAMAGED)" : "");
	else
		strcpy(msg, "");
	sng_set_foreground(UI_COLOR(damcon_part));
	sng_draw_vect_obj(&placeholder_part_spun[byteangle], x, y);
	sng_center_xy_draw_string(msg, NANO_FONT, x,
			y - 15 - (o->tsd.part.part % 2) * 15);
	if (o->tsd.part.damage < 0.30 * 255.0)
		sng_set_foreground(UI_COLOR(damcon_good));
	else if (o->tsd.part.damage < 0.75 * 255.0)
		sng_set_foreground(UI_COLOR(damcon_caution));
	else {
		if ((timer & 0x8) == 0) /* make red bar blink */
			return;
		sng_set_foreground(UI_COLOR(damcon_warning));
	}
	snis_draw_rectangle(0, x - 30, y + 10, 60, 6);
	snis_draw_rectangle(1, x - 30, y + 10,
			60.0 * (255 - o->tsd.part.damage) / 255.0, 6);
}

static void draw_damcon_waypoint(GtkWidget *w, struct snis_damcon_entity *o)
{
	int x, y;

	sng_set_foreground(UI_COLOR(damcon_part));
	x = damconx_to_screenx(o->x);
	y = damcony_to_screeny(o->y);
	snis_draw_rectangle(0, x - 4, y - 4, 8, 8);
}

static void draw_damcon_object(GtkWidget *w, struct snis_damcon_entity *o)
{
	switch (o->type) {
	case DAMCON_TYPE_ROBOT:
		draw_damcon_robot(w, o);
		damcon_robot_entity = o;
		break;
	case DAMCON_TYPE_WARPDRIVE:
	case DAMCON_TYPE_SENSORARRAY:
	case DAMCON_TYPE_COMMUNICATIONS:
	case DAMCON_TYPE_MANEUVERING:
	case DAMCON_TYPE_PHASERBANK:
	case DAMCON_TYPE_IMPULSE:
	case DAMCON_TYPE_SHIELDSYSTEM:
	case DAMCON_TYPE_TRACTORSYSTEM:
	case DAMCON_TYPE_LIFESUPPORTSYSTEM:
	case DAMCON_TYPE_REPAIR_STATION:
		draw_damcon_system(w, o);
		break;
	case DAMCON_TYPE_SOCKET:
		draw_damcon_socket(w, o);
		break;
	case DAMCON_TYPE_PART:
		draw_damcon_part(w, o);
		break;
	case DAMCON_TYPE_WAYPOINT:
		draw_damcon_waypoint(w, o);
		break;
	default:
		break;
	}
}

static void show_damcon(GtkWidget *w)
{
	int i;

	sng_set_foreground(UI_COLOR(damcon_wall));
	snis_draw_rectangle(0, damconscreenx0, damconscreeny0, damconscreenxdim, damconscreenydim);

	/* clip to damcon screen */
	sng_set_clip_window(damconscreenx0, damconscreeny0,
			damconscreenx0 + damconscreenxdim,
			damconscreeny0 + damconscreenydim);

	/* draw all the stuff on the damcon screen */
	for (i = 0; i <= snis_object_pool_highest_object(damcon_pool); i++)
		draw_damcon_object(w, &dco[i]);
	draw_damcon_arena_borders(w);

	/* restore clipping back to whole window */
	set_default_clip_window();
	show_common_screen(w, "DAMAGE CONTROL");
}

static void sci_details_pressed(void *x)
{
	queue_to_server(snis_opcode_pkt("bb", OPCODE_SCI_DETAILS,
		(unsigned char) SCI_DETAILS_MODE_DETAILS));
}

static void sci_waypoints_pressed(void *x)
{
	queue_to_server(snis_opcode_pkt("bb", OPCODE_SCI_DETAILS,
		(unsigned char) SCI_DETAILS_MODE_WAYPOINTS));
}

static void sci_align_to_ship_pressed(void *x)
{
	queue_to_server(snis_opcode_pkt("b", OPCODE_SCI_ALIGN_TO_SHIP));
}

static void sci_threed_pressed(void *x)
{
	queue_to_server(snis_opcode_pkt("bb", OPCODE_SCI_DETAILS,
		(unsigned char) SCI_DETAILS_MODE_THREED));
}

static void sci_sciplane_pressed(void *x)
{
	queue_to_server(snis_opcode_pkt("bb", OPCODE_SCI_DETAILS,
		(unsigned char) SCI_DETAILS_MODE_SCIPLANE));
}

static void sci_tractor_pressed(void *x)
{
	uint32_t id = curr_science_guy ? curr_science_guy->id : (uint32_t) 0xffffffff;
	queue_to_server(snis_opcode_pkt("bw", OPCODE_REQUEST_TRACTORBEAM, id));
	sci_ui.low_tractor_power_timer = 3 * frame_rate_hz;
}

static void sci_mining_bot_pressed(void *x)
{
	uint32_t id = curr_science_guy ? curr_science_guy->id : (uint32_t) 0xffffffff;
	queue_to_server(snis_opcode_pkt("bw", OPCODE_REQUEST_MINING_BOT, id));
}

static void sci_custom_button_pressed(__attribute__((unused)) void *x)
{
	queue_to_server(snis_opcode_pkt("bb", OPCODE_CUSTOM_BUTTON, OPCODE_CUSTOM_BUTTON_SUBCMD_SCIENCE));
}

static void science_waypoint_entered(void *cookie)
{
	return;
}

static void science_clear_waypoint_pressed(void *cookie)
{
	struct button **b = cookie;
	int index = b - &sci_ui.clear_waypoint_button[0];

	queue_to_server(snis_opcode_subcode_pkt("bbb", OPCODE_SET_WAYPOINT,
						OPCODE_SET_WAYPOINT_CLEAR,
						(uint8_t) index));
}

static void science_select_waypoint_pressed(void *cookie)
{
	struct button **b = cookie;
	int index = b - &sci_ui.select_waypoint_button[0];

	if (index == curr_science_waypoint)
		index = -1;
	request_sci_select_target(OPCODE_SCI_SELECT_TARGET_TYPE_WAYPOINT,
					(uint32_t) index);
}

static void science_add_current_pos_pressed(void *cookie)
{
	struct snis_entity *o = find_my_ship();
	if (!o)
		return;
	queue_to_server(snis_opcode_subcode_pkt("bbSSS", OPCODE_SET_WAYPOINT,
						OPCODE_SET_WAYPOINT_ADD_ROW,
						o->x, (int32_t) UNIVERSE_DIM,
						o->y, (int32_t) UNIVERSE_DIM,
						o->z, (int32_t) UNIVERSE_DIM));
}

static void science_add_waypoint_pressed(void *cookie)
{
	int rc;
	double x, y, z;

	rc = sscanf(sci_ui.waypoint_text[0], "%lf", &x);
	if (rc != 1)
		return;
	rc = sscanf(sci_ui.waypoint_text[1], "%lf", &y);
	if (rc != 1)
		return;
	rc = sscanf(sci_ui.waypoint_text[2], "%lf", &z);
	if (rc != 1)
		return;
	queue_to_server(snis_opcode_subcode_pkt("bbSSS", OPCODE_SET_WAYPOINT,
						OPCODE_SET_WAYPOINT_ADD_ROW,
						x, (int32_t) UNIVERSE_DIM,
						y, (int32_t) UNIVERSE_DIM,
						z, (int32_t) UNIVERSE_DIM));
	snis_text_input_box_set_contents(sci_ui.waypoint_input[0], "");
	snis_text_input_box_set_contents(sci_ui.waypoint_input[1], "");
	snis_text_input_box_set_contents(sci_ui.waypoint_input[2], "");
}

static void init_science_ui(void)
{
	int i;

	const int szx = (350 * SCREEN_WIDTH) / 800;
	const int szy = 35 * SCREEN_HEIGHT / 600;
	const int szw = 300 * SCREEN_WIDTH / 800;
	const int szh = 12 * SCREEN_WIDTH / 800;

	const int spx = szx;
	const int spy = 50 * SCREEN_HEIGHT / 600;

	const int mbbx = 485 * SCREEN_WIDTH / 800;
	const int mbby = 575 * SCREEN_HEIGHT / 600;
	const int mbbw = 60 * SCREEN_WIDTH / 800;
	const int mbbh = 20 * SCREEN_HEIGHT / 600;

	const int trbx = 555 * SCREEN_WIDTH / 800;
	const int trby = 575 * SCREEN_HEIGHT / 600;
	const int trbw = 50 * SCREEN_WIDTH / 800;
	const int trbh = 20 * SCREEN_HEIGHT / 600;

	const int wpx = 615 * SCREEN_WIDTH / 800;
	const int wpy = 575 * SCREEN_HEIGHT / 600;
	const int wpw = 60 * SCREEN_WIDTH / 800;
	const int wph = 20 * SCREEN_HEIGHT / 600;

	const int scpx = 685 * SCREEN_WIDTH / 800;
	const int scpy = trby;
	const int scpw = 23 * SCREEN_WIDTH / 800;
	const int scph = trbh;

	const int thdx = 715 * SCREEN_WIDTH / 800;
	const int thdy = trby;
	const int thdw = 23 * SCREEN_WIDTH / 800;
	const int thdh = trbh;

	const int detx = 745 * SCREEN_WIDTH / 800;
	const int dety = trby;
	const int detw = 50 * SCREEN_WIDTH / 800;
	const int deth = trbh;

	const int atsx = 10 * SCREEN_WIDTH / 800;
	const int atsy = trby;
	const int atsw = 75 * SCREEN_WIDTH / 800;
	const int atsh = trbh;

	const int cbbx = SCREEN_WIDTH - txx(100);
	const int cbby = txy(30);

	sci_ui.low_tractor_power_timer = 0;

	sci_ui.scizoom = snis_slider_init(szx, szy, szw, szh, UI_COLOR(sci_slider), "RANGE", "0", "100",
				0.0, 100.0, sample_scizoom, do_scizoom);
	snis_slider_set_label_font(sci_ui.scizoom, NANO_FONT);
	sci_ui.scipower = snis_slider_init(spx, spy, szw, szh, UI_COLOR(sci_slider), "POWER", "0", "100",
				0.0, 100.0, sample_sensors_power, NULL);
	snis_slider_set_fuzz(sci_ui.scipower, 7);
	snis_slider_set_label_font(sci_ui.scipower, NANO_FONT);
	sci_ui.launch_mining_bot_button = snis_button_init(mbbx, mbby, mbbw, mbbh, "MINING BOT",
			UI_COLOR(sci_button), NANO_FONT, sci_mining_bot_pressed, (void *) 0);
	snis_button_set_sound(sci_ui.launch_mining_bot_button, UISND13);
	sci_ui.custom_button = snis_button_init(cbbx, cbby, -1, -1, "CUSTOM BUTTON", UI_COLOR(sci_button),
						NANO_FONT, sci_custom_button_pressed, (void *) 0);
	snis_button_set_sound(sci_ui.custom_button, UISND14);
	sci_ui.tractor_button = snis_button_init(trbx, trby, trbw, trbh, "TRACTOR",
			UI_COLOR(sci_button), NANO_FONT, sci_tractor_pressed, (void *) 0);
	snis_button_set_sound(sci_ui.tractor_button, UISND14);
	sci_ui.waypoints_button = snis_button_init(wpx, wpy, wpw, wph, "WAYPOINTS",
			UI_COLOR(sci_button), NANO_FONT, sci_waypoints_pressed, (void *) 0);
	snis_button_set_sound(sci_ui.waypoints_button, UISND15);
	sci_ui.sciplane_button = snis_button_init(scpx, scpy, scpw, scph, "SRS",
			UI_COLOR(sci_button), NANO_FONT, sci_sciplane_pressed, (void *) 0);
	snis_button_set_sound(sci_ui.sciplane_button, UISND16);
	sci_ui.threed_button = snis_button_init(thdx, thdy, thdw, thdh, "LRS",
			UI_COLOR(sci_button), NANO_FONT, sci_threed_pressed, (void *) 0);
	snis_button_set_sound(sci_ui.threed_button, UISND17);
	sci_ui.details_button = snis_button_init(detx, dety, detw, deth, "DETAILS",
			UI_COLOR(sci_button), NANO_FONT, sci_details_pressed, (void *) 0);
	snis_button_set_sound(sci_ui.details_button, UISND18);
	sci_ui.align_to_ship_button = snis_button_init(atsx, atsy, atsw, atsh, "ALIGN TO SHIP",
			UI_COLOR(sci_button), NANO_FONT, sci_align_to_ship_pressed, (void *) 0);
	snis_button_set_sound(sci_ui.align_to_ship_button, UISND19);

	sci_ui.menu = create_pull_down_menu(NANO_FONT, SCREEN_WIDTH);
	pull_down_menu_set_color(sci_ui.menu, UI_COLOR(sci_button));
	pull_down_menu_set_background_alpha(sci_ui.menu, 0.75);
	pull_down_menu_add_column(sci_ui.menu, "SCIENCE");

	ui_add_slider(sci_ui.scizoom, DISPLAYMODE_SCIENCE, "SCIENCE SCOPE ZOOM CONTROL");
	ui_add_slider(sci_ui.scipower, DISPLAYMODE_SCIENCE, "SCANNING BEAM POWER INDICATOR");
	ui_add_button(sci_ui.details_button, DISPLAYMODE_SCIENCE, "VIEW DETAILS ABOUT SELECTED TARGET");
	ui_add_button(sci_ui.launch_mining_bot_button, DISPLAYMODE_SCIENCE,
			"LAUNCH THE MINING ROBOT TOWARDS SELECTED TARGET");
	ui_add_button(sci_ui.custom_button, DISPLAYMODE_SCIENCE, "CUSTOM BUTTON");
	ui_add_button(sci_ui.tractor_button, DISPLAYMODE_SCIENCE, "TOGGLE THE TRACTOR BEAM ON OR OFF");
	ui_add_button(sci_ui.threed_button, DISPLAYMODE_SCIENCE, "SELECT LONG RANGE SCANNERS");
	ui_add_button(sci_ui.sciplane_button, DISPLAYMODE_SCIENCE, "SELECT SHORT RANGE SCANNERS");
	ui_add_button(sci_ui.waypoints_button, DISPLAYMODE_SCIENCE, "MANAGE WAYPOINTS");
	ui_add_button(sci_ui.align_to_ship_button, DISPLAYMODE_SCIENCE,
				"ALIGN LONG RANGE SCANNERS TO SHIP'S ORIENTATION");
	ui_hide_widget(sci_ui.align_to_ship_button);
	sciecx = entity_context_new(50, 50);
	sciballecx = entity_context_new(5000, 1000);
	sciplane_tween = tween_init(500);
	sci_ui.details_mode = SCI_DETAILS_MODE_SCIPLANE;

	for (i = 0; i < 3; i++) {
		sci_ui.waypoint_input[i] =
			snis_text_input_box_init(i * txx(135) + txx(10), txy(100),
						txy(30), txx(130),
						UI_COLOR(sci_wireframe), TINY_FONT,
						&sci_ui.waypoint_text[i][0],
						sizeof(sci_ui.waypoint_text[i]), &timer,
						science_waypoint_entered, &sci_ui.waypoint_text[i][0]);
			snis_text_input_box_set_return(sci_ui.waypoint_input[i],
							science_waypoint_entered);
			ui_add_text_input_box(sci_ui.waypoint_input[i], DISPLAYMODE_SCIENCE);
			ui_hide_widget(sci_ui.waypoint_input[i]);
	}

	sci_ui.add_waypoint_button = snis_button_init(txx(3 * 135 + 20), txy(100),
				100 * SCREEN_WIDTH / 800, wph, "ADD WAYPOINT",
				UI_COLOR(sci_button), NANO_FONT, science_add_waypoint_pressed, NULL);
	snis_button_set_sound(sci_ui.add_waypoint_button, UISND20);
	ui_add_button(sci_ui.add_waypoint_button, DISPLAYMODE_SCIENCE,
			"ADDS A WAYPOINT AT SPECIFIED X, Y, Z");
	ui_hide_widget(sci_ui.add_waypoint_button);

	sci_ui.add_current_pos_button = snis_button_init(txx(4 * 135 + 20), txy(100),
				100 * SCREEN_WIDTH / 800, wph, "CURRENT POSITION",
				UI_COLOR(sci_button), NANO_FONT, science_add_current_pos_pressed, NULL);
	snis_button_set_sound(sci_ui.add_current_pos_button, UISND21);
	ui_add_button(sci_ui.add_current_pos_button, DISPLAYMODE_SCIENCE,
			"ADD THE SHIP'S CURRENT POSITION AS A WAYPOINT");
	ui_hide_widget(sci_ui.add_current_pos_button);

	for (i = 0; i < MAXWAYPOINTS; i++) {
		sci_ui.clear_waypoint_button[i] = snis_button_init(txx(10), txy(25 * i) + txy(200),
				40 * SCREEN_WIDTH / 800, wph, "CLEAR",
				UI_COLOR(sci_button), NANO_FONT, science_clear_waypoint_pressed,
				&sci_ui.clear_waypoint_button[i]);
		snis_button_set_sound(sci_ui.clear_waypoint_button[i], UISND22);
		ui_add_button(sci_ui.clear_waypoint_button[i], DISPLAYMODE_SCIENCE, "DELETE THIS WAYPOINT");
		ui_hide_widget(sci_ui.clear_waypoint_button[i]);
		sci_ui.select_waypoint_button[i] = snis_button_init(txx(500), txy(25 * i) + txy(200),
				40 * SCREEN_WIDTH / 800, wph, "SELECT",
				UI_COLOR(sci_button), NANO_FONT, science_select_waypoint_pressed,
				&sci_ui.select_waypoint_button[i]);
		snis_button_set_sound(sci_ui.select_waypoint_button[i], UISND23);
		ui_add_button(sci_ui.select_waypoint_button[i], DISPLAYMODE_SCIENCE, "SELECT THIS WAYPOINT");
		ui_hide_widget(sci_ui.select_waypoint_button[i]);
	}
	ui_add_pull_down_menu(sci_ui.menu, DISPLAYMODE_SCIENCE); /* needs to be last */
}

static void comms_screen_button_pressed(void *x)
{
	unsigned long screen = (unsigned long) x;

	switch (screen) {
	case 0: do_onscreen((uint8_t) DISPLAYMODE_COMMS);
		break;
	case 1: do_onscreen((uint8_t) DISPLAYMODE_NAVIGATION);
		break;
	case 2: do_onscreen((uint8_t) DISPLAYMODE_WEAPONS);
		break;
	case 3: do_onscreen((uint8_t) DISPLAYMODE_ENGINEERING);
		break;
	case 4: do_onscreen((uint8_t) DISPLAYMODE_DAMCON);
		break;
	case 5: do_onscreen((uint8_t) DISPLAYMODE_SCIENCE);
		break;
	case 6: do_onscreen((uint8_t) DISPLAYMODE_MAINSCREEN);
		break;
	default:
		break;
	}
	return;
}

static void comms_custom_button_pressed(__attribute__((unused)) void *x)
{
	queue_to_server(snis_opcode_pkt("bb", OPCODE_CUSTOM_BUTTON, OPCODE_CUSTOM_BUTTON_SUBCMD_COMMS));
}

static void comms_hail_button_pressed(__attribute__((unused)) void *x)
{
	snis_text_input_box_set_contents(comms_ui.comms_input, "/HAIL ");
	ui_set_widget_focus(uiobjs, comms_ui.comms_input);
	return;
}

static void comms_channel_button_pressed(__attribute__((unused)) void *x)
{
	snis_text_input_box_set_contents(comms_ui.comms_input, "/CHANNEL ");
	ui_set_widget_focus(uiobjs, comms_ui.comms_input);
	return;
}

static void comms_transmit_button_pressed(void *x);

static void comms_manifest_button_pressed(__attribute__((unused)) void *x)
{
	snis_text_input_box_set_contents(comms_ui.comms_input, "/MANIFEST");
	ui_set_widget_focus(uiobjs, comms_ui.comms_input);
	comms_transmit_button_pressed(NULL);
	return;
}

static void comms_computer_button_pressed(__attribute__((unused)) void *x)
{
	snis_text_input_box_set_contents(comms_ui.comms_input, "/COMPUTER ");
	ui_set_widget_focus(uiobjs, comms_ui.comms_input);
	return;
}

static void comms_eject_button_pressed(__attribute__((unused)) void *x)
{
	snis_text_input_box_set_contents(comms_ui.comms_input, "/EJECT ");
	ui_set_widget_focus(uiobjs, comms_ui.comms_input);
	return;
}

static void comms_help_button_pressed(__attribute__((unused)) void *x)
{
	snis_text_input_box_set_contents(comms_ui.comms_input, "/HELP");
	ui_set_widget_focus(uiobjs, comms_ui.comms_input);
	comms_transmit_button_pressed(NULL);
	return;
}

static void comms_about_button_pressed(__attribute__((unused)) void *x)
{
	snis_text_input_box_set_contents(comms_ui.comms_input, "/ABOUT");
	comms_transmit_button_pressed(NULL);
	ui_set_widget_focus(uiobjs, comms_ui.comms_input);
	return;
}

static void comms_cryptanalysis_button_pressed(__attribute__((unused)) void *x)
{
	struct snis_entity *o;

	if (!(o = find_my_ship()))
		return;
	queue_to_server(snis_opcode_pkt("bb", OPCODE_COMMS_CRYPTO, !o->tsd.ship.comms_crypto_mode));
}

static void comms_screen_red_alert_pressed(void *x)
{
	unsigned char new_alert_mode;

	new_alert_mode = (red_alert_mode == 0);	
	queue_to_server(snis_opcode_pkt("bb", OPCODE_REQUEST_REDALERT, new_alert_mode));
}

static void comms_hail_mining_bot_pressed(__attribute__((unused)) void *x)
{
	snis_text_input_box_set_contents(comms_ui.comms_input, "/HAIL MINING-BOT");
	comms_transmit_button_pressed(NULL);
	ui_set_widget_focus(uiobjs, comms_ui.comms_input);
	return;
}

static void comms_main_screen_pressed(void *x)
{
	unsigned char new_comms_mode;

	new_comms_mode = (main_screen_text.comms_on_mainscreen == 0);	
	queue_to_server(snis_opcode_pkt("bb", OPCODE_COMMS_MAINSCREEN, new_comms_mode));
}

static void send_comms_packet_to_server(char *msg, uint8_t opcode, uint32_t id)
{
	struct packed_buffer *pb;
	uint8_t len = strlen(msg);

	pb = packed_buffer_allocate(sizeof(struct comms_transmission_packet) + len);
	packed_buffer_append(pb, "bbbw", opcode, OPCODE_COMMS_PLAINTEXT, len, id);
	packed_buffer_append_raw(pb, msg, (unsigned short) len);
	packed_buffer_queue_add(&to_server_queue, pb, &to_server_queue_mutex);
	wakeup_gameserver_writer();
}

static void send_text_to_speech_packet_to_server(char *msg)
{
	struct packed_buffer *pb;
	uint8_t len = strlen(msg);

	pb = packed_buffer_allocate(sizeof(struct comms_transmission_packet) + len);
	packed_buffer_append(pb, "bbb", OPCODE_NATURAL_LANGUAGE_REQUEST,
				OPCODE_NL_SUBCOMMAND_TEXT_TO_SPEECH, len);
	packed_buffer_append_raw(pb, msg, (unsigned short) len);
	packed_buffer_queue_add(&to_server_queue, pb, &to_server_queue_mutex);
	wakeup_gameserver_writer();
}

static void send_rtsmode_change_to_server(unsigned char desired_rts_mode)
{
	queue_to_server(packed_buffer_new("bb", OPCODE_DEMON_RTSMODE, desired_rts_mode));
}

static void send_natural_language_request_to_server(char *msg)
{
	struct packed_buffer *pb;
	uint8_t len = strlen(msg);

	pb = packed_buffer_allocate(sizeof(struct comms_transmission_packet) + len);
	packed_buffer_append(pb, "bbb", OPCODE_NATURAL_LANGUAGE_REQUEST,
				OPCODE_NL_SUBCOMMAND_TEXT_REQUEST, len);
	packed_buffer_append_raw(pb, msg, (unsigned short) len);
	packed_buffer_queue_add(&to_server_queue, pb, &to_server_queue_mutex);
	wakeup_gameserver_writer();
}

static void send_enscript_packet_to_server(char *filename)
{
	struct packed_buffer *pb;
	uint8_t len = strlen(filename);

	pb = packed_buffer_allocate(sizeof(struct lua_enscript_packet) + len);
	packed_buffer_append(pb, "bb", OPCODE_ENSCRIPT, len);
	packed_buffer_append_raw(pb, filename, (unsigned short) len);
	packed_buffer_queue_add(&to_server_queue, pb, &to_server_queue_mutex);
	wakeup_gameserver_writer();
}

static void send_lua_script_packet_to_server(char *script)
{
	struct packed_buffer *pb;
	uint8_t len = strlen(script);
	printf("script = '%s', len = %hhu\n", script, len);

	pb = packed_buffer_allocate(sizeof(struct lua_script_packet) + len);
	packed_buffer_append(pb, "bb", OPCODE_EXEC_LUA_SCRIPT, len);
	packed_buffer_append_raw(pb, script, (unsigned short) len);
	packed_buffer_queue_add(&to_server_queue, pb, &to_server_queue_mutex);
	wakeup_gameserver_writer();
}

static void send_normal_comms_packet_to_server(char *msg)
{
	send_comms_packet_to_server(msg, OPCODE_COMMS_TRANSMISSION, my_ship_id);
}

static void comms_transmit_button_pressed(void *x)
{
	if (strlen(comms_ui.input) == 0)
		return;
	send_normal_comms_packet_to_server(comms_ui.input);
	snis_text_input_box_zero(comms_ui.comms_input);
}

static void comms_rts_button_pressed(void *x)
{
	uint8_t rts_button_number = (uint8_t) (((intptr_t) x) & 0x0ff);
	queue_to_server(snis_opcode_subcode_pkt("bbb", OPCODE_RTS_FUNC,
			OPCODE_RTS_FUNC_COMMS_BUTTON, rts_button_number));
}

static void comms_rts_build_unit_button_pressed(void *x)
{
	struct snis_entity *o, *builder;
	uint8_t rts_build_unit_button_number = (uint8_t) (((intptr_t) x) & 0x0ff);
	uint32_t builder_id;
	int i;

	fprintf(stderr, "RTS Build Unit (%s) button pressed: %hhu\n",
			rts_unit_type(rts_build_unit_button_number)->name,
			rts_build_unit_button_number);
	/* Figure out which starbase or home planet to use. */
	if (!(o = find_my_ship())) {
		fprintf(stderr, "DID NOT FIND MY SHIP\n");
		return;
	}
	if (o->tsd.ship.rts_active_button == 255) {
		fprintf(stderr, "NO ACTIVE RTS BUTTON\n");
		return;
	}
	if (o->tsd.ship.rts_active_button == RTS_FLEET_BUTTON) { /* shouldn't happen */
		fprintf(stderr, "FLEET BUTTON ACTIVE\n");
		return;
	}
	builder = NULL;
	builder_id = (uint32_t) -1;
	if (o->tsd.ship.rts_active_button == RTS_HOME_PLANET_BUTTON) { /* Figure out which planet to use */
		pthread_mutex_lock(&universe_mutex);
		/* First planet with same faction as player is the right one. */
		builder = rts_find_my_home_planet();
		if (builder) {
			builder_id = builder->id;
			fprintf(stderr, "HOME PLANET, BUILDER ID = %u\n", builder->id);
		} else {
			fprintf(stderr, "HOME PLANET NOT FOUND\n");
		}
		pthread_mutex_unlock(&universe_mutex);
	} else { /* Figure out which starbase to use */
		char sbname[10];
		snprintf(sbname, sizeof(sbname), "SB-%02d", o->tsd.ship.rts_active_button);
		pthread_mutex_lock(&universe_mutex);
		for (i = 0; i <= snis_object_pool_highest_object(pool); i++) {
			if (go[i].alive && go[i].type == OBJTYPE_STARBASE) {
				fprintf(stderr, "Looking for '%s' vs '%s'\n",
					sbname, go[i].sdata.name);
				if (strncmp(go[i].sdata.name, sbname, 5) == 0) {
					builder = &go[i];
					break;
				}
			}
		}
		if (builder) {
			fprintf(stderr, "BUILDER STARBASE FOUND, ID = %u\n", builder->id);
			builder_id = builder->id;
			if (builder->tsd.starbase.occupant[3] != o->sdata.faction) {
				fprintf(stderr, "STARBASE IS NOT OCCUPIED BY FACTION\n");
			}
		} else {
			fprintf(stderr, "BUILDER STARBASE NOT FOUND\n");
		}
		pthread_mutex_unlock(&universe_mutex);
	}
	if (builder_id != (uint32_t) -1) {
		fprintf(stderr, "BUILDER ID is %u\n", builder_id);
		queue_to_server(snis_opcode_subcode_pkt("bbwb", OPCODE_RTS_FUNC, OPCODE_RTS_FUNC_BUILD_UNIT,
			builder_id, rts_build_unit_button_number));
	}
}

static void comms_rts_order_command_button_pressed(void *x)
{
	int i;
	uint8_t rts_command_number = (uint8_t) (((intptr_t) x) & 0x0ff);

	/* Make the checkboxes behave as radio buttons. FIXME: state is local to client here. */
	for (i = 0; i < NUM_RTS_ORDER_TYPES; i++) {
		if (i != rts_command_number)
			comms_ui.fleet_order_checkbox[i] = 0;
		else
			comms_ui.fleet_order_checkbox[i] = 1;
		/* Note, we do not have to do anything for the one selected, as the
		 * generic button code will toggle that one. In fact, we should not set
		 * it, since if we do, the button code will afterwards toggle it off
		 * plus the user might be turning it off anyway.
		 */
		printf("comms_ui.fleet_order_checkbox[%d] = %d\n", i, comms_ui.fleet_order_checkbox[i]);
	}
}

static int comms_rts_order_checkbox(void *x)
{
	int *fleet_order_checkbox = x;
	return *fleet_order_checkbox;
}

static void comms_input_entered()
{
	printf("comms input entered\n");
}

static void send_updated_cipher_key_to_server(void)
{
	struct packed_buffer *pb;

	/* Send the updated key guess to the server */
	pb = packed_buffer_allocate(sizeof(struct comms_transmission_packet) + 26);
	packed_buffer_append(pb, "bbbw", OPCODE_COMMS_TRANSMISSION, OPCODE_COMMS_KEY_GUESS, (uint8_t) 26, my_ship_id);
	packed_buffer_append_raw(pb, cipher_key, 26);
	queue_to_server(pb);
}

static void crypto_reset_button_pressed(void *x)
{
	int i;

	memset(cipher_key, '_', 26);
	send_updated_cipher_key_to_server();
	for (i = 0; i < 26; i++)
		snis_text_input_box_set_contents(comms_ui.crypt_alpha[i], "_");
}

static void crypt_alpha_input_entered(void *x)
{
	int letter = (intptr_t) x;
	char value;
	int i;

	if (letter < 0 || letter > 25)
		return;

	value = toupper(comms_ui.crypt_alpha_text[letter][0]);
	if ((value < 'A' || value > 'Z') && value != '_')
		return;
	/* If anything is mapped to this value, clear it now */
	for (i = 0; i < 26; i++) {
		if (cipher_key[i] == letter + 'A')
			cipher_key[i] = '_';
		if (toupper(comms_ui.crypt_alpha_text[i][0]) == value)
			snis_text_input_box_set_contents(comms_ui.crypt_alpha[i], "_");
	}
	if (value >= 'A' && value <= 'Z') {
		char v[3];
		v[0] = value;
		v[1] = '\0';
		cipher_key[value - 'A'] = letter + 'A';
		snis_text_input_box_set_contents(comms_ui.crypt_alpha[letter],  v);
	}
	send_updated_cipher_key_to_server();
}

static void disable_comms_rts_unit_ordering_buttons(void)
{
	int i;

	for (i = 0; i < NUM_RTS_UNIT_TYPES; i++)
		snis_button_disable(comms_ui.rts_order_unit_button[i]);
}

static void enable_comms_rts_unit_ordering_buttons(void)
{
	int i;

	for (i = 0; i < NUM_RTS_UNIT_TYPES; i++)
		snis_button_enable(comms_ui.rts_order_unit_button[i]);
}

static void comms_fleet_ship_button_pressed(__attribute__((unused)) void *x)
{
	int i;
	uint8_t command;
	uint32_t direct_object = (uint32_t) -1;
	uint32_t id = (uint32_t) (intptr_t) x;

	printf("Comms fleet button pressed\n");
	/* Figure out which command is requested */
	command = 255;
	for (i = 0; i < NUM_RTS_ORDER_TYPES; i++) {
		if (comms_ui.fleet_order_checkbox[i]) {
			command = i + AI_MODE_RTS_STANDBY;
			break;
		}
	}
	if (command == 255) /* User has not selected any command */
		return;
	/* Figure out which ship is requested */
	i = lookup_object_by_id(id);
	if (i < 0)
		return;
	queue_to_server(snis_opcode_subcode_pkt("bbwbw", OPCODE_RTS_FUNC,
			OPCODE_RTS_FUNC_COMMAND_UNIT, id, command, direct_object));
}

static void init_comms_ui(void)
{
	int i, j;
	int x = txx(140);
	int y = txy(5);
	int bw = txx(70);
	int bh = txy(25);
	int button_color = UI_COLOR(comms_button);
	int text_color = UI_COLOR(comms_text);
	int red_alert_color = UI_COLOR(comms_red_alert);
	float crypt_alphax, crypt_alphay;

	comms_ui.comms_onscreen_button = snis_button_init(x, y, bw, bh, "COMMS", button_color,
			NANO_FONT, comms_screen_button_pressed, (void *) 0);
	snis_button_set_sound(comms_ui.comms_onscreen_button, UISND9);
	x += bw;
	comms_ui.nav_onscreen_button = snis_button_init(x, y, bw, bh, "NAV", button_color,
			NANO_FONT, comms_screen_button_pressed, (void *) 1);
	snis_button_set_sound(comms_ui.nav_onscreen_button, UISND10);
	x += bw;
	comms_ui.weap_onscreen_button = snis_button_init(x, y, bw, bh, "WEAP", button_color,
			NANO_FONT, comms_screen_button_pressed, (void *) 2);
	snis_button_set_sound(comms_ui.weap_onscreen_button, UISND11);
	x += bw;
	comms_ui.eng_onscreen_button = snis_button_init(x, y, bw, bh, "ENG", button_color,
			NANO_FONT, comms_screen_button_pressed, (void *) 3);
	snis_button_set_sound(comms_ui.eng_onscreen_button, UISND12);
	x += bw;
	comms_ui.damcon_onscreen_button = snis_button_init(x, y, bw, bh, "DAMCON", button_color,
			NANO_FONT, comms_screen_button_pressed, (void *) 4);
	snis_button_set_sound(comms_ui.damcon_onscreen_button, UISND13);
	x += bw;
	comms_ui.sci_onscreen_button = snis_button_init(x, y, bw, bh, "SCI", button_color,
			NANO_FONT, comms_screen_button_pressed, (void *) 5);
	snis_button_set_sound(comms_ui.sci_onscreen_button, UISND14);
	x += bw;
	comms_ui.main_onscreen_button = snis_button_init(x, y, bw, bh, "MAIN", button_color,
			NANO_FONT, comms_screen_button_pressed, (void *) 6);
	snis_button_set_sound(comms_ui.main_onscreen_button, UISND15);
	x += bw;
	comms_ui.custom_button = snis_button_init(x, y, bw, bh, "CUSTOM BUTTON", button_color,
			NANO_FONT, comms_custom_button_pressed, (void *) 0);
	snis_button_set_sound(comms_ui.custom_button, UISND16);
	x = txx(140);
	y = txy(30);
	comms_ui.hail_button = snis_button_init(x, y, bw, bh, "/HAIL", button_color,
			NANO_FONT, comms_hail_button_pressed, (void *) 0);
	snis_button_set_sound(comms_ui.hail_button, UISND15);
	x += bw;
	comms_ui.channel_button = snis_button_init(x, y, bw, bh, "/CHANNEL", button_color,
			NANO_FONT, comms_channel_button_pressed, (void *) 0);
	snis_button_set_sound(comms_ui.channel_button, UISND15);
	x += bw;
	comms_ui.manifest_button = snis_button_init(x, y, bw, bh, "/MANIFEST", button_color,
			NANO_FONT, comms_manifest_button_pressed, (void *) 0);
	snis_button_set_sound(comms_ui.manifest_button, UISND15);
	x += bw;
	comms_ui.computer_button = snis_button_init(x, y, bw, bh, "/COMPUTER", button_color,
			NANO_FONT, comms_computer_button_pressed, (void *) 0);
	snis_button_set_sound(comms_ui.computer_button, UISND15);
	x += bw;
	comms_ui.eject_button = snis_button_init(x, y, bw, bh, "/EJECT", button_color,
			NANO_FONT, comms_eject_button_pressed, (void *) 0);
	snis_button_set_sound(comms_ui.eject_button, UISND15);
	x += bw;
	comms_ui.help_button = snis_button_init(x, y, bw, bh, "/HELP", button_color,
			NANO_FONT, comms_help_button_pressed, (void *) 0);
	snis_button_set_sound(comms_ui.help_button, UISND15);
	x += bw;
	comms_ui.about_button = snis_button_init(x, y, bw, bh, "/ABOUT", button_color,
			NANO_FONT, comms_about_button_pressed, (void *) 0);
	snis_button_set_sound(comms_ui.about_button, UISND15);
	x += bw;
	comms_ui.cryptanalysis_button = snis_button_init(x, y, bw, bh, "CRYPTO", button_color,
			NANO_FONT, comms_cryptanalysis_button_pressed, (void *) 0);
	snis_button_set_sound(comms_ui.cryptanalysis_button, UISND16);

	x = SCREEN_WIDTH - txx(150);
	y = SCREEN_HEIGHT - 60;
	comms_ui.mainscreen_comms = snis_button_init(x, y, txx(80), bh, "MAIN SCREEN", button_color,
			NANO_FONT, comms_main_screen_pressed, NULL);
	snis_button_set_checkbox_function(comms_ui.mainscreen_comms,
					snis_button_generic_checkbox_function,
					&main_screen_text.comms_on_mainscreen);
	snis_button_set_sound(comms_ui.mainscreen_comms, UISND17);
	y -= bh + 4;
	comms_ui.hail_mining_bot_button = snis_button_init(x, y, txx(80), bh, "MINING BOT", button_color,
			NANO_FONT, comms_hail_mining_bot_pressed, NULL);
	snis_button_set_sound(comms_ui.hail_mining_bot_button, UISND16);
	y -= bh + 4;
	comms_ui.red_alert_button = snis_button_init(x, y, txx(80), bh, "RED ALERT", red_alert_color,
			NANO_FONT, comms_screen_red_alert_pressed, NULL);
	snis_button_set_checkbox_function(comms_ui.red_alert_button,
					snis_button_generic_checkbox_function,
					&red_alert_mode);
	snis_button_set_sound(comms_ui.red_alert_button, UISND16);

	comms_ui.tw = text_window_init(txx(10), txy(70), SCREEN_WIDTH - txx(20), 300, 27, text_color);
	text_window_set_chatter_sound(comms_ui.tw, TTY_CHATTER_SOUND);
	comms_ui.comms_input = snis_text_input_box_init(txx(10), txy(520), txy(30), txx(550),
					text_color, TINY_FONT,
					comms_ui.input, 50, &timer,
					comms_input_entered, NULL);
	snis_text_input_box_set_return(comms_ui.comms_input,
					comms_transmit_button_pressed);
	snis_text_input_box_set_dynamic_width(comms_ui.comms_input, txx(100), txx(550));
	comms_ui.comms_transmit_button = snis_button_init(txx(10), txy(560), -1, txy(30),
			"TRANSMIT", button_color,
			TINY_FONT, comms_transmit_button_pressed, NULL);
	snis_button_set_sound(comms_ui.comms_transmit_button, UISND18);
	comms_ui.mainzoom_slider = snis_slider_init(txx(180), txy(560), txx(380), txy(15),
				UI_COLOR(comms_slider), "ZOOM",
				"1", "10", 0.0, 100.0, sample_mainzoom,
				do_mainzoom);
	comms_ui.emf_strip_chart =
		snis_strip_chart_init(txx(705), txy(5), txx(90.0), txy(50.0),
				"EMF", "SCAN DETECTED", UI_COLOR(science_graph_plot_strong),
				UI_COLOR(common_red_alert), 100, NANO_FONT, 900);
	const int rts_button_y = txy(310);
	const int rts_button_spacing = txx(70);
	comms_ui.rts_main_planet_button = snis_button_init(txx(10) + rts_button_spacing * 0, rts_button_y, -1, txy(20),
			"HOME PLANET X", button_color, NANO_FONT,
			comms_rts_button_pressed, (void *) RTS_HOME_PLANET_BUTTON);
	snis_button_set_sound(comms_ui.rts_main_planet_button, UISND18);
	comms_ui.rts_fleet_button = snis_button_init(txx(50) + rts_button_spacing, rts_button_y, -1, txy(20),
			"FLEET", button_color, NANO_FONT,
			comms_rts_button_pressed, (void *) RTS_FLEET_BUTTON);
	snis_button_set_sound(comms_ui.rts_fleet_button, UISND18);
	for (i = 0; i < NUM_RTS_BASES; i++) {
		char name[20];
		snprintf(name, sizeof(name), "SB-%02d ?/? X", i);
		comms_ui.rts_starbase_button[i] = snis_button_init(txx(160) + rts_button_spacing * i,
			rts_button_y, -1, txy(20), name, button_color, NANO_FONT,
			comms_rts_button_pressed, (void *) ((intptr_t) i));
		snis_button_set_sound(comms_ui.rts_starbase_button[i], UISND18);
		snis_button_set_color(comms_ui.rts_starbase_button[i], UI_COLOR(comms_neutral));
	}

	for (i = 0; i < NUM_RTS_UNIT_TYPES; i++) {
		char button_label[20];

		snprintf(button_label, 20, "$%.0f %s", rts_unit_type(i)->cost_to_build, rts_unit_type(i)->name);
		comms_ui.rts_order_unit_button[i] = snis_button_init(txx(22), txy(375) + txy(14) * i, -1, -1,
				button_label, button_color, PICO_FONT,
				comms_rts_build_unit_button_pressed, (void *) (intptr_t) i);
		snis_button_set_disabled_color(comms_ui.rts_order_unit_button[i], UI_COLOR(comms_neutral));
	}

	for (i = 0; i < NUM_RTS_ORDER_TYPES; i++) {
		char button_label[20];

		snprintf(button_label, 20, "$%.0f %s", rts_order_type(i)->cost_to_order, rts_order_type(i)->name);
		comms_ui.rts_order_command_button[i] = snis_button_init(txx(22), txy(375) + txy(14) * i, txx(100), -1,
				button_label, button_color, PICO_FONT, comms_rts_order_command_button_pressed,
				(void *) (intptr_t) i);
		snis_button_set_checkbox_function(comms_ui.rts_order_command_button[i],
					comms_rts_order_checkbox, &comms_ui.fleet_order_checkbox[i]);
	}

	for (i = 0; i < FLEET_BUTTON_COLS; i++) {
		for (j = 0; j < FLEET_BUTTON_ROWS; j++) {
			comms_ui.fleet_unit_button[i][j] = snis_button_init(txx(140) + i * txx(50),
									txy(355) + j * txy(14), txx(45), -1,
									"BLAH", UI_COLOR(comms_good_status), PICO_FONT,
									comms_fleet_ship_button_pressed, NULL);
			ui_add_button(comms_ui.fleet_unit_button[i][j], DISPLAYMODE_COMMS, "ASSIGN ORDERS TO UNIT");
			ui_hide_widget(comms_ui.fleet_unit_button[i][j]);
			snis_button_set_color(comms_ui.fleet_unit_button[i][j], UI_COLOR(comms_good_status));
			snis_button_set_disabled_color(comms_ui.fleet_unit_button[i][j], UI_COLOR(comms_neutral));
		}
	}

	comms_ui.our_base_health = snis_slider_init(txx(20), txy(505), txx(200), txy(10),
				UI_COLOR(comms_slider), "MAIN BASE",
				"1", "100", 0.0, 10000.0, sample_main_base_health, NULL);
	comms_ui.enemy_base_health = snis_slider_init(txx(330), txy(505), txx(200), txy(10),
				UI_COLOR(comms_slider), "ENEMY BASE",
				"1", "100", 0.0, 10000.0, sample_enemy_base_health, NULL);
	snis_slider_set_label_font(comms_ui.our_base_health, PICO_FONT);
	snis_slider_set_label_font(comms_ui.enemy_base_health, PICO_FONT);

	crypt_alphax = txx(20);
	crypt_alphay = txy(430);
	for (i = 0; i < 26; i++) {
		memset(&comms_ui.crypt_alpha_text[i][0], 0, 3);
		comms_ui.crypt_alpha[i] = snis_text_input_box_init(
					crypt_alphax, crypt_alphay, txy(30), txx(30),
					text_color, TINY_FONT,
					&comms_ui.crypt_alpha_text[i][0], 3, &timer,
					crypt_alpha_input_entered, (void *) (intptr_t) i);
		snis_text_input_box_set_return(comms_ui.crypt_alpha[i],
					crypt_alpha_input_entered);
		crypt_alphax += txx((751 - 20) / 13.0);
		if (crypt_alphax >= txx(750)) {
			crypt_alphax = txx(20);
			crypt_alphay += txy(70);
		}
	}


	x = txx(700);
	y = txy(570);
	comms_ui.crypto_reset = snis_button_init(x, y, bw, bh, "RESET", button_color,
			NANO_FONT, crypto_reset_button_pressed, (void *) 1);
	snis_button_set_sound(comms_ui.crypto_reset, UISND10);

	ui_add_text_window(comms_ui.tw, DISPLAYMODE_COMMS);
	ui_add_button(comms_ui.comms_onscreen_button, DISPLAYMODE_COMMS,
			"PROJECT COMMS SCREEN ON THE MAIN VIEW");
	ui_add_button(comms_ui.nav_onscreen_button, DISPLAYMODE_COMMS,
			"PROJECT NAVIGATION SCREEN ON THE MAIN VIEW");
	ui_add_button(comms_ui.weap_onscreen_button, DISPLAYMODE_COMMS,
			"PROJECT WEAPONS SCREEN ON THE MAIN VIEW");
	ui_add_button(comms_ui.eng_onscreen_button, DISPLAYMODE_COMMS,
			"PROJECT ENGINEERING SCREEN ON THE MAIN VIEW");
	ui_add_button(comms_ui.damcon_onscreen_button, DISPLAYMODE_COMMS,
			"PROJECT DAMAGE CONTROL SCREEN ON THE MAIN VIEW");
	ui_add_button(comms_ui.sci_onscreen_button, DISPLAYMODE_COMMS,
			"PROJECT SCIENCE SCREEN ON THE MAIN VIEW");
	ui_add_button(comms_ui.main_onscreen_button, DISPLAYMODE_COMMS,
			"PROJECT MAIN SCREEN ON THE MAIN VIEW");
	ui_add_button(comms_ui.custom_button, DISPLAYMODE_COMMS,
			"CUSTOM BUTTON");
	ui_add_button(comms_ui.hail_button, DISPLAYMODE_COMMS,
			"HAIL ANOTHER SHIP OR STARBASE BY NAME");
	ui_add_button(comms_ui.channel_button, DISPLAYMODE_COMMS,
			"SET THE CHANNEL NUMBER ON WHICH TO XMIT/RECV");
	ui_add_button(comms_ui.manifest_button, DISPLAYMODE_COMMS,
			"SHOW SHIP'S MANIFEST");
	ui_add_button(comms_ui.computer_button, DISPLAYMODE_COMMS,
			"COMMAND THE SHIP'S COMPUTER");
	ui_add_button(comms_ui.eject_button, DISPLAYMODE_COMMS,
			"EJECT CONTENTS OF ONE OF THE SHIPS CARGO BAYS");
	ui_add_button(comms_ui.help_button, DISPLAYMODE_COMMS,
			"SHOW HELP SCREEN FOR COMMS TERMINAL");
	ui_add_button(comms_ui.about_button, DISPLAYMODE_COMMS,
			"SHOW VERSION INFO ABOUT SPACE NERDS IN SPACE");
	ui_add_button(comms_ui.cryptanalysis_button, DISPLAYMODE_COMMS,
			"CRYPTANALYSIS OF ENCRYPTED MESSAGES");
	ui_add_button(comms_ui.red_alert_button, DISPLAYMODE_COMMS,
			"ACTIVATE RED ALERT ALARM");
	ui_add_button(comms_ui.hail_mining_bot_button, DISPLAYMODE_COMMS,
			"HAIL THE MINING BOT IF IT IS DEPLOYED");
	ui_add_button(comms_ui.mainscreen_comms, DISPLAYMODE_COMMS,
			"DISPLAY MOST RECENT COMMS TRANSMISSSIONS ON MAIN SCREEN");
	ui_add_button(comms_ui.comms_transmit_button, DISPLAYMODE_COMMS,
			"TRANSMIT ENTERED TEXT ON CURRENT CHANNEL");
	ui_add_button(comms_ui.rts_fleet_button, DISPLAYMODE_COMMS,
			"TRANSMIT ORDERS TO FLEET");
	ui_add_button(comms_ui.rts_main_planet_button, DISPLAYMODE_COMMS,
			"TRANSMIT ORDERS TO HOME PLANET");
	ui_add_button(comms_ui.crypto_reset, DISPLAYMODE_COMMS,
			"RESET CRYPTO KEY");
	for (i = 0; i < NUM_RTS_BASES; i++) {
		ui_add_button(comms_ui.rts_starbase_button[i], DISPLAYMODE_COMMS,
				"TRANSMIT ORDERS TO STARBASE");
	}

	for (i = 0; i < NUM_RTS_UNIT_TYPES; i++) {
		char help_text[40];
		snprintf(help_text, 40, "$%5.0f ORDER A %s TO BE BUILT", rts_unit_type(i)->cost_to_build,
				rts_unit_type(i)->name);
		ui_add_button(comms_ui.rts_order_unit_button[i], DISPLAYMODE_COMMS, help_text);
	}

	for (i = 0; i < NUM_RTS_ORDER_TYPES; i++)
		ui_add_button(comms_ui.rts_order_command_button[i], DISPLAYMODE_COMMS, rts_order_type(i)->help_text);

	ui_add_text_input_box(comms_ui.comms_input, DISPLAYMODE_COMMS);
	ui_add_slider(comms_ui.mainzoom_slider, DISPLAYMODE_COMMS, "ZOOM CONTROL FOR THE MAIN SCREEN");
	ui_add_slider(comms_ui.our_base_health, DISPLAYMODE_COMMS, "OUR MAIN BASE HEALTH");
	ui_add_slider(comms_ui.enemy_base_health, DISPLAYMODE_COMMS, "ENEMY MAIN BASE HEALTH");
	ui_add_strip_chart(comms_ui.emf_strip_chart, DISPLAYMODE_COMMS);

	for (i = 0; i < 26; i++)
		ui_add_text_input_box(comms_ui.crypt_alpha[i], DISPLAYMODE_COMMS);
	comms_ui.channel = 0;
	ui_set_widget_focus(uiobjs, comms_ui.comms_input);
}

static void comms_activate_rts_buttons(struct snis_entity *player_ship)
{
	int i, j;

	ui_unhide_widget(comms_ui.rts_fleet_button);
	ui_unhide_widget(comms_ui.rts_main_planet_button);
	ui_unhide_widget(comms_ui.our_base_health);
	ui_unhide_widget(comms_ui.enemy_base_health);
	for (i = 0; i < NUM_RTS_BASES; i++)
		ui_unhide_widget(comms_ui.rts_starbase_button[i]);
	if (player_ship->tsd.ship.rts_active_button < RTS_FLEET_BUTTON) {
		for (i = 0; i < NUM_RTS_UNIT_TYPES; i++) /* Unhide starbase and main planet buttons */
			ui_unhide_widget(comms_ui.rts_order_unit_button[i]);
		for (i = 0; i < NUM_RTS_ORDER_TYPES; i++) /* Hide fleet buttons */
			ui_hide_widget(comms_ui.rts_order_command_button[i]);
		for (i = 0; i < FLEET_BUTTON_COLS; i++)
			for (j = 0; j < FLEET_BUTTON_ROWS; j++)
				ui_hide_widget(comms_ui.fleet_unit_button[i][j]);
	} else if (player_ship->tsd.ship.rts_active_button == RTS_FLEET_BUTTON) {
		for (i = 0; i < NUM_RTS_UNIT_TYPES; i++) /* Hide starbase and main planet buttons */
			ui_hide_widget(comms_ui.rts_order_unit_button[i]);
		for (i = 0; i < NUM_RTS_ORDER_TYPES; i++) /* Unhide fleet buttons */
			ui_unhide_widget(comms_ui.rts_order_command_button[i]);
		for (i = 0; i < FLEET_BUTTON_COLS; i++)
			for (j = 0; j < FLEET_BUTTON_ROWS; j++)
				ui_unhide_widget(comms_ui.fleet_unit_button[i][j]);
	} else {
		for (i = 0; i < NUM_RTS_UNIT_TYPES; i++) /* Hide starbase and main planet buttons */
			ui_hide_widget(comms_ui.rts_order_unit_button[i]);
		for (i = 0; i < NUM_RTS_ORDER_TYPES; i++) /* Hide fleet buttons */
			ui_hide_widget(comms_ui.rts_order_command_button[i]);
		for (i = 0; i < FLEET_BUTTON_COLS; i++)
			for (j = 0; j < FLEET_BUTTON_ROWS; j++)
				ui_hide_widget(comms_ui.fleet_unit_button[i][j]);
	}
	if (curr_science_waypoint == (uint32_t) -1) { /* no waypoint selected */
		/* FIXME: this "4" is too fragile */
		snis_button_set_label(comms_ui.rts_order_command_button[4], "NO WAYPOINT");
		snis_button_set_color(comms_ui.rts_order_command_button[4], UI_COLOR(comms_neutral));
	} else {
		snis_button_set_label(comms_ui.rts_order_command_button[4], "GOTO WAYPOINT");
		snis_button_set_color(comms_ui.rts_order_command_button[4], UI_COLOR(comms_good_status));
	}
	text_window_set_visible_lines(comms_ui.tw, 17);
}

static void comms_deactivate_rts_buttons(void)
{
	int i, j;

	ui_hide_widget(comms_ui.rts_fleet_button);
	ui_hide_widget(comms_ui.rts_main_planet_button);
	ui_hide_widget(comms_ui.our_base_health);
	ui_hide_widget(comms_ui.enemy_base_health);
	for (i = 0; i < NUM_RTS_BASES; i++)
		ui_hide_widget(comms_ui.rts_starbase_button[i]);
	for (i = 0; i < NUM_RTS_UNIT_TYPES; i++)
		ui_hide_widget(comms_ui.rts_order_unit_button[i]);
	for (i = 0; i < NUM_RTS_ORDER_TYPES; i++)
		ui_hide_widget(comms_ui.rts_order_command_button[i]);
	for (i = 0; i < FLEET_BUTTON_COLS; i++)
		for (j = 0; j < FLEET_BUTTON_ROWS; j++)
			ui_hide_widget(comms_ui.fleet_unit_button[i][j]);
	text_window_set_visible_lines(comms_ui.tw, 27);
}

static void comms_setup_rts_buttons(int activate, struct snis_entity *player_ship) /* called with universe lock held */
{
	int i, j;
	int us, them;
	const char *spinner = "|/-\\";
	int set_planet_spinner = 0;
	int checked_command = -1;

	if (!activate || player_ship->tsd.ship.comms_crypto_mode) {
		comms_deactivate_rts_buttons();
		return;
	}
	comms_activate_rts_buttons(player_ship);

	/* Modify the starbase/home planet button labels to indication the occupation status of the starbases */
	for (i = 0; i <= snis_object_pool_highest_object(pool); i++) {
		struct snis_entity *starbase = &go[i];
		if (starbase->type == OBJTYPE_STARBASE) {
			/* Count how many occupiers are "us" and how many are "them" */
			us = 0;
			them = 0;
			for (j = 0; j < 4; j++) {
				if (starbase->tsd.starbase.occupant[j] == player_ship->sdata.faction)
					us++;
				else if (starbase->tsd.starbase.occupant[j] != 255)
					them++;
			}
			/* Modify the appropriate button label and color */
			for (j = 0; j < NUM_RTS_BASES; j++) {
				char button_label[20];
				char activity;
				if (starbase->tsd.starbase.starbase_number == j) {
					if (starbase->tsd.starbase.time_left_to_build == 0)
						activity = ' ';
					else
						activity = spinner[(starbase->tsd.starbase.time_left_to_build / 2) % 4];
					snprintf(button_label, sizeof(button_label), "SB-%02d %01d/%01d %c",
							j, us, them, activity);
					snis_button_set_label(comms_ui.rts_starbase_button[j], button_label);
					if (starbase->tsd.starbase.occupant[3] == player_ship->sdata.faction)
						snis_button_set_color(comms_ui.rts_starbase_button[j],
									UI_COLOR(comms_good_status));
					else if (starbase->tsd.starbase.occupant[3] != 255)
						snis_button_set_color(comms_ui.rts_starbase_button[j],
									UI_COLOR(comms_warning));
					else
						snis_button_set_color(comms_ui.rts_starbase_button[j],
									UI_COLOR(comms_neutral));
					break;
				}
			}
		} else if (starbase->type == OBJTYPE_PLANET && starbase->alive &&
				starbase->sdata.faction == player_ship->sdata.faction) {
			/* Modify the Home Planet button with activity spinner */
			char button_label[20];
			char activity = ' ';
			if (set_planet_spinner == 0) { /* only look at the first planet with the right faction */
				if (starbase->tsd.planet.time_left_to_build == 0)
					activity = ' ';
				else
					activity = spinner[(starbase->tsd.planet.time_left_to_build / 2) % 4];
			}
			set_planet_spinner++;
			snprintf(button_label, sizeof(button_label), "HOME PLANET %c", activity);
			snis_button_set_label(comms_ui.rts_main_planet_button, button_label);
		}
	}

	checked_command = -1;
	for (i = 0; i < NUM_RTS_ORDER_TYPES; i++)
		if (comms_ui.fleet_order_checkbox[i]) {
			checked_command = i;
			break;
		}

	/* Modify the fleet unit buttons labels */
	if (player_ship->tsd.ship.rts_active_button == RTS_FLEET_BUTTON) {
		int fleet_unit_button = 0;
		int col = 0;
		int row = 0;
		for (i = 0; i <= snis_object_pool_highest_object(pool); i++) {
			struct snis_entity *ship = &go[i];
			char button_label[20];
			char tooltip[100];
			if (ship->alive && ship->type == OBJTYPE_SHIP2 &&
				ship->sdata.faction == player_ship->sdata.faction) {
				col = fleet_unit_button % FLEET_BUTTON_COLS;
				row = fleet_unit_button / FLEET_BUTTON_COLS;
				char *short_name;
				if (ship->tsd.ship.nai_entries > 0 &&
					ship->tsd.ship.ai[0].ai_mode >= AI_MODE_RTS_FIRST_COMMAND &&
					ship->tsd.ship.ai[0].ai_mode <=
						AI_MODE_RTS_FIRST_COMMAND + NUM_RTS_ORDER_TYPES + 0) {
						/* Special case above for OUT OF FUEL */
					int order_type = ship->tsd.ship.ai[0].ai_mode - AI_MODE_RTS_STANDBY;
					/* Make standby orders and out-of-fuel blink to draw attention to idle units. */
					if ((ship->tsd.ship.ai[0].ai_mode == AI_MODE_RTS_STANDBY ||
						ship->tsd.ship.ai[0].ai_mode == AI_MODE_RTS_OUT_OF_FUEL) && (timer & 0x8) == 0)
						short_name = "";
					else
						short_name = rts_order_type(order_type)->short_name;
					snprintf(button_label, 20, "%s %s", ship->sdata.name, short_name);
					snprintf(tooltip, 100, "ASSIGN ORDERS TO %s (CURRENT: %s)",
							ship->sdata.name, rts_order_type(order_type)->name);
				} else {
					snprintf(button_label, 20, "%s", ship->sdata.name);
					snprintf(tooltip, 100, "ASSIGN ORDERS TO %s (CURRENT: NONE)", ship->sdata.name);
				}
				snis_button_set_label(comms_ui.fleet_unit_button[col][row], button_label);
				ui_set_widget_tooltip(comms_ui.fleet_unit_button[col][row], tooltip);
				snis_button_set_cookie(comms_ui.fleet_unit_button[col][row],
							(void *) (intptr_t) ship->id);
				if (checked_command == -1 ||
					!orders_valid_for_unit_type(checked_command,
								ship_type[ship->tsd.ship.shiptype].rts_unit_type))
					snis_button_disable(comms_ui.fleet_unit_button[col][row]);
				else
					snis_button_enable(comms_ui.fleet_unit_button[col][row]);
				fleet_unit_button++;
			}
		}
		/* Hide buttons that do not have associated ships */
		for (i = fleet_unit_button; i < FLEET_BUTTON_ROWS * FLEET_BUTTON_COLS; i++) {
				col = i % FLEET_BUTTON_COLS;
				row = i / FLEET_BUTTON_COLS;
				ui_hide_widget(comms_ui.fleet_unit_button[col][row]);
		}
	}
}

static void update_emf_detector(uint8_t emf_value)
{
	snis_strip_chart_update(comms_ui.emf_strip_chart, emf_value);
}

static void science_mouse_rotate(int x, int y)
{
	double dist_from_center, dx, dy, scaled_dx, scaled_dy;
	float dxi, dyi, angle, abs_angle, octant;
	int finex = 2 * control_key_pressed;
	int finey = 2 * control_key_pressed;

	if (sci_ui.details_mode != SCI_DETAILS_MODE_THREED)
		return;

	/* get the distance the x and y coords are from the balls center */
	dx = x - SCIENCE_SCOPE_CX;
	dy = y - SCIENCE_SCOPE_CY;
	/* Invert the values to make the action pulling the ball to you rather than pushing it away */
	dx -= dx * 2;
	dy -= dy * 2;

	dist_from_center = sqrt(dx * dx + dy * dy);
	if (dist_from_center < SCIENCE_SCOPE_R) {

		/* Make sure we do nothing if the cursor is pretty much in the center */
		if (dist_from_center > SCIENCE_SCOPE_R / 10) {

			/* Scale the output to be min 0 max 1 */
			scaled_dx = 1 * (dx / SCIENCE_SCOPE_R);
			scaled_dy = 1 * (dy / SCIENCE_SCOPE_R);

			/* Round the values to create numbers that support the sciball input convention */
			/* 0 yaw left 1 yaw right */

			dxi = (float) ceil(scaled_dx);
			dyi = (float) ceil(scaled_dy);

			/* Get the angle from the center to the mouse in radians */
			angle = (float) atan2(scaled_dy, scaled_dx); /* pointing eastward */
			abs_angle = fabsf(angle);
			octant = PI / 8; /* calculate the value we use to create something similar to a */
					 /* D-pad overlaying the ball.*/

			/* Detect if the user should be expecting only a single input instead of both yaw and pitch */
			if (abs_angle > 3 * octant && abs_angle < 5 * octant)
				dxi = -1;

			if (abs_angle < octant || abs_angle > 7 * octant)
				dyi = -1;

			/* Scale to use the sciball functions convention when lighter movements */
			/* would be more fitting */

			if (fabs(scaled_dx) < 0.33 && dxi >= 0)
				finex = 2; /* see YAW_INCREMENT and YAW_INCREMENT_FINE for this convention */

			if (fabs(scaled_dy) < 0.33 && dyi >= 0)
				finey = 2;

			/*send the network requests to the server*/
			if (dxi >= 0)
				queue_to_server(snis_opcode_pkt("bb", OPCODE_REQUEST_SCIBALL_YAW, (int) dxi + finex));

			if (dyi >= 0)
				queue_to_server(snis_opcode_pkt("bb", OPCODE_REQUEST_SCIBALL_PITCH, (int) dyi + finey));
		}
	}
	return;
}

static void science_mouse_click_rotate(int x, int y)
{
	double dist_from_center, dx, dy, yaw, pitch;
	int8_t yawval, pitchval;
	int i, yawcount, pitchcount;

	if (sci_ui.details_mode != SCI_DETAILS_MODE_THREED)
		return;
	dx = x - SCIENCE_SCOPE_CX;
	dy = y - SCIENCE_SCOPE_CY;
	dist_from_center = sqrt(dx * dx + dy * dy);
	if (dist_from_center > SCIENCE_SCOPE_R)
		return;
	yaw = -dx;
	pitch = -dy;
	yawcount = fabs(dx);
	pitchcount = fabs(dy);
	if (yawcount > 0 && pitchcount / yawcount > 3)
		yawcount = 0;
	else if (pitchcount > 0 && yawcount / pitchcount > 3)
		pitchcount = 0;
	if (yawcount > 2)
		yawcount = 2;
	if (pitchcount > 2)
		pitchcount = 2;
	if (yaw > 0)
		yawval = YAW_LEFT;
	else
		yawval = YAW_RIGHT;
	if (pitch > 0)
		pitchval = PITCH_FORWARD;
	else
		pitchval = PITCH_BACK;
	for (i = 0; i < yawcount; i++)
		queue_to_server(snis_opcode_pkt("bb",
			OPCODE_REQUEST_SCIBALL_YAW, yawval));
	for (i = 0; i < pitchcount; i++)
		queue_to_server(snis_opcode_pkt("bb",
			OPCODE_REQUEST_SCIBALL_PITCH, pitchval));
}

#define SCIDIST2 100

static void science_button_held(int button, int x, int y)
{
	x = sng_pixelx_to_screenx(x);
	y = sng_pixely_to_screeny(y);

	int traffic_throttle = (int) ((frame_rate_hz / 15.0) + 0.5);

	switch (button) {
	case 2:
		if (sci_ui.details_mode == SCI_DETAILS_MODE_THREED) {
			/* Throttle back the traffic to avoid flooding network */
			if (timer % traffic_throttle != 0)
				break;
			science_mouse_rotate(x, y);
		}
		break;
	default:
		break;
	}
	return;
}

static void science_button_release(int button, int x, int y)
{
	int i;
	int xdist, ydist, dist2, mindist;
	struct snis_entity *selected;
	int waypoint_selected = -1;
	int near_enough_to_fail = 0;

	x = sng_pixelx_to_screenx(x);
	y = sng_pixely_to_screeny(y);
	if (button == 3) {
		science_mouse_click_rotate(x, y);
		return;
	}

	selected = NULL;
	mindist = -1;
	pthread_mutex_lock(&universe_mutex);
	for (i = 0; i < nscience_guys; i++) {
		xdist = (x - science_guy[i].sx);
		ydist = (y - science_guy[i].sy);
		dist2 = xdist * xdist + ydist * ydist; 
		if (dist2 < SCIDIST2) {
			if (dist2 < mindist || mindist == -1) {
				if (science_guy[i].o) {
					if (curr_science_guy == science_guy[i].o ||
						science_guy[i].o->sdata.science_data_known) {
						selected = science_guy[i].o;
						waypoint_selected = -1;
					}
				} else {
					waypoint_selected = science_guy[i].waypoint_index;
					selected = NULL;
				}
				mindist = dist2;
			}
		}
		if (dist2 < 100 * 100)
			near_enough_to_fail = 1;
	}
	if (selected) {
		if (curr_science_guy != selected)
			request_sci_select_target(OPCODE_SCI_SELECT_TARGET_TYPE_OBJECT, selected->id);
		else
			request_sci_select_target(OPCODE_SCI_SELECT_TARGET_TYPE_OBJECT, (uint32_t) -1); /* deselect */
	} else if (waypoint_selected != -1) {
		if (curr_science_waypoint != waypoint_selected)
			request_sci_select_target(OPCODE_SCI_SELECT_TARGET_TYPE_WAYPOINT,
					(uint32_t) waypoint_selected);
		else
			request_sci_select_target(OPCODE_SCI_SELECT_TARGET_TYPE_WAYPOINT,
					(uint32_t) -1); /* deselect */
	} else if (near_enough_to_fail)
		wwviaudio_add_sound(UISND1);
	pthread_mutex_unlock(&universe_mutex);
	return;
}

#define SCIENCE_DATA_X (SCIENCE_SCOPE_X + SCIENCE_SCOPE_W + 80 * SCREEN_WIDTH / 800)
#define SCIENCE_DATA_Y (SCIENCE_SCOPE_Y + 0)
#define SCIENCE_DATA_W (SCREEN_WIDTH - (20 * SCREEN_WIDTH / 800) - SCIENCE_DATA_X)
#define SCIENCE_DATA_H (SCREEN_HEIGHT - (40 * SCREEN_HEIGHT / 600) - SCIENCE_DATA_Y)

static void draw_science_graph(GtkWidget *w, struct snis_entity *ship, struct snis_entity *o,
		int x1, int y1, int x2, int y2)
{
	int i, x;
	double sx, sy, sy1, sy2, dist;
	int dy1, dy2, bw, probes, dx, pwr;
	int initial_noise;

	sng_set_foreground(UI_COLOR(science_graph_grid));
	snis_draw_rectangle(0, x1, y1, (x2 - x1), (y2 - y1));
	snis_draw_dotted_hline(w->window, gc, x1, y1 + (y2 - y1) / 4, x2, 10);
	snis_draw_dotted_hline(w->window, gc, x1, y1 + (y2 - y1) / 2, x2, 10);
	snis_draw_dotted_hline(w->window, gc, x1, y1 + 3 * (y2 - y1) / 4, x2, 10);

	x = x1 + (x2 - x1) / 4; 
	snis_draw_dotted_vline(w->window, gc, x, y1, y2, 10);
	x += (x2 - x1) / 4; 
	snis_draw_dotted_vline(w->window, gc, x, y1, y2, 10);
	x += (x2 - x1) / 4; 
	snis_draw_dotted_vline(w->window, gc, x, y1, y2, 10);
	
	if (o) {
		if (o != ship) {
			dist = hypot(o->x - ship->x, o->y - ship->y);
			bw = (int) (ship->tsd.ship.sci_beam_width * 180.0 / M_PI);
			pwr = ship->tsd.ship.power_data.sensors.i;
		} else {
			dist = 0.1;
			bw = 5.0;
			pwr = 255; /* never any trouble scanning our own ship */
		}

		sng_set_foreground(UI_COLOR(science_graph_plot_strong));
		if (o->sdata.shield_strength < 64) {
			sng_set_foreground(UI_COLOR(science_graph_plot_weak));
			if ((timer & 0x07) < 4)
				goto skip_data;
		}

		/* TODO, make sample count vary based on sensor power,damage */
		probes = (30 * 10) / (bw / 2 + ((dist * 2.0) / XKNOWN_DIM));
		initial_noise = (int) ((hypot((float) bw, 256.0 - pwr) / 256.0) * 20.0);
		for (i = 0; i < probes; i++) {
			double ss;
			int nx, ny;

			nx = ny = initial_noise;
			if (nx <= 0)
				nx = 1;
			if (ny <= 0)
				ny = 1;
			dy1 = snis_randn(ny) - ny / 2; /* TODO: make this vary based on damage */
			dy2 = snis_randn(ny) - ny / 2;
			dx = snis_randn(nx) - nx / 2;

			x = snis_randn(256);
			ss = shield_strength((uint8_t) x, o->sdata.shield_strength,
						o->sdata.shield_width,
						o->sdata.shield_depth,
						o->sdata.shield_wavelength);
			sx = (int) (((double) x / 255.0) * (double) (x2 - x1)) + x1;
			sy = (int) (((1.0 - ss) * (double) (y2 - y1)) + y1);

			sy1 = sy + dy1;
			if (sy1 < y1)
				sy1 = y1;
			if (sy1 > y2)
				sy1 = y2;
			sy2 = sy + dy2;
			if (sy2 < y1)
				sy2 = y1;
			if (sy2 > y2)
				sy2 = y2;
			sx += dx;
			if (sx < x1)
				sx = x1;
			if (x > x2)
				sx = x2;

			snis_draw_dotted_vline(w->window, gc, sx, sy1, sy2, 4);
		}
	}
skip_data:
	sng_set_foreground(UI_COLOR(science_data_label));
	sng_abs_xy_draw_string("10", NANO_FONT, x1, y2 + txy(10));
	sng_abs_xy_draw_string("20", NANO_FONT, x1 + (x2 - x1) / 4 - 10, y2 + txy(10));
	sng_abs_xy_draw_string("30", NANO_FONT, x1 + 2 * (x2 - x1) / 4 - 10, y2 + txy(10));
	sng_abs_xy_draw_string("40", NANO_FONT, x1 + 3 * (x2 - x1) / 4 - 10, y2 + txy(10));
	sng_abs_xy_draw_string("50", NANO_FONT, x1 + 4 * (x2 - x1) / 4 - 20, y2 + txy(10));
	sng_abs_xy_draw_string("SHIELD PROFILE (NM)", NANO_FONT, x1 + (x2 - x1) / 4 - 10, y2 + txy(20));
}

static void draw_science_data(GtkWidget *w, struct snis_entity *ship, struct snis_entity *o, int waypoint_index)
{
	char buffer[40];
	char buffer2[40];
	int x, y, gx1, gy1, gx2, gy2, yinc;
	double dx, dy, dz;
	static double bearing = 0.0;
	static double mark = 0.0;
	char *the_faction;
	static double lastx = 0.0;
	static double lasty = 0.0;
	static double lastz = 0.0;
	static double lastvel = 0.0;
	static double closing_rate = 0.0;
	static double dejittered_closing_rate = 0.0;
	static double last_closing_rate = 0.0;
	static int eta_mins = 0;
	static int eta_secs = 0;
	static double range = 0.0;
	static double last_range = 1.0;
	static double dejittered_range = 0.0;
	static double velocity = 0.0;
	int update_display = ((timer & 0x0f) == 0 || !dejitter_science_details);

	yinc = 22 * SCREEN_HEIGHT / 600;

	if (!ship)
		return;
	x = SCIENCE_DATA_X + 10 * SCREEN_WIDTH / 800;
	y = SCIENCE_DATA_Y + 15 * SCREEN_HEIGHT / 600;
	sng_set_foreground(UI_COLOR(sci_wireframe));
	snis_draw_rectangle(0, SCIENCE_DATA_X, SCIENCE_DATA_Y,
					SCIENCE_DATA_W, SCIENCE_DATA_H);
	if (waypoint_index != (uint32_t) -1)  {
		snprintf(buffer, sizeof(buffer), "NAME: WAYPOINT-%02d", waypoint_index);
		sng_abs_xy_draw_string(buffer, TINY_FONT, x, y);
	} else {
		snprintf(buffer, sizeof(buffer), "NAME: %s", o ? o->sdata.name : "NO SCAN TARGET SELECTED");
		sng_abs_xy_draw_string(buffer, TINY_FONT, x, y);
		if (o && (o->type == OBJTYPE_SHIP1 ||
			o->type == OBJTYPE_SHIP2 ||
			o->type == OBJTYPE_WARPGATE ||
			o->type == OBJTYPE_STARBASE ||
			o->type == OBJTYPE_DERELICT)) {
			y += yinc;
			the_faction = o ?
				o->sdata.faction >= 0 &&
				o->sdata.faction < nfactions() ?
					faction_name(o->sdata.faction) : "UNKNOWN" : "UNKNOWN";
			snprintf(buffer, sizeof(buffer), "ORIG: %s", the_faction);
			sng_abs_xy_draw_string(buffer, TINY_FONT, x, y);
			y += yinc;
			if (o->type != OBJTYPE_DERELICT)
				snprintf(buffer, sizeof(buffer), "REGISTRATION: %d", o->id);
			else
				snprintf(buffer, sizeof(buffer), "REGISTRATION: %d", o->tsd.derelict.orig_ship_id);
			sng_abs_xy_draw_string(buffer, TINY_FONT, x, y);
		}
	}

	if (o) {
		switch (o->type) {
		case OBJTYPE_SHIP2:
			if (global_rts_mode) {
				int unit_type;
				char *unit_type_name;
				unit_type = ship_type[o->sdata.subclass].rts_unit_type;
				unit_type_name = unit_type < 0 ? "UNKNOWN" :
						rts_unit_type(unit_type)->name,
				snprintf(buffer, sizeof(buffer), "TYPE: %s (%s)",
						ship_type[o->sdata.subclass].class,
					unit_type_name);
			} else {
				snprintf(buffer, sizeof(buffer), "TYPE: %s", ship_type[o->sdata.subclass].class);
			}
			break;
		case OBJTYPE_STARBASE:
			snprintf(buffer, sizeof(buffer), "TYPE: %s", "STARBASE");
			break;
		case OBJTYPE_WARPGATE:
			snprintf(buffer, sizeof(buffer), "TYPE: %s", "WARPGATE");
			break;
		case OBJTYPE_ASTEROID:
			snprintf(buffer, sizeof(buffer), "TYPE: %s", "ASTEROID");
			break;
		case OBJTYPE_DERELICT:
			snprintf(buffer, sizeof(buffer), "TYPE: %s", "DERELICT");
			break;
		case OBJTYPE_WARP_CORE:
			snprintf(buffer, sizeof(buffer), "TYPE: %s", "EJECTED WARP CORE");
			break;
		case OBJTYPE_PLANET:
			snprintf(buffer, sizeof(buffer), "TYPE: %s%s PLANET", o->tsd.planet.ring ? "RINGED " : "",
				solarsystem_assets->planet_type[o->tsd.planet.solarsystem_planet_type]);
			uppercase(buffer);
			break;
		case OBJTYPE_BLACK_HOLE:
			snprintf(buffer, sizeof(buffer), "TYPE: BLACK HOLE");
			break;
		default:
			snprintf(buffer, sizeof(buffer), "TYPE: %s", "UNKNOWN");
			break;
		}
	} else if (waypoint_index != (uint32_t) -1) {
		snprintf(buffer, sizeof(buffer), "TYPE: WAYPOINT");
	} else {
		snprintf(buffer, sizeof(buffer), "TYPE:");
	}
	y += yinc;
	sng_abs_xy_draw_string(buffer, TINY_FONT, x, y);

	if (o)
		snprintf(buffer, sizeof(buffer), "X: %0.2lf", o->x);
	else if (waypoint_index != (uint32_t) -1)
		snprintf(buffer, sizeof(buffer), "X: %0.2lf", sci_ui.waypoint[waypoint_index][0]);
	else
		snprintf(buffer, sizeof(buffer), "X:");
	y += yinc;
	sng_abs_xy_draw_string(buffer, TINY_FONT, x, y);

	if (o)
		snprintf(buffer, sizeof(buffer), "Y: %0.2lf", o->y);
	else if (waypoint_index != (uint32_t) -1)
		snprintf(buffer, sizeof(buffer), "Y: %0.2lf", sci_ui.waypoint[waypoint_index][1]);
	else
		snprintf(buffer, sizeof(buffer), "Y:");
	y += yinc;
	sng_abs_xy_draw_string(buffer, TINY_FONT, x, y);

	if (o)
		snprintf(buffer, sizeof(buffer), "Z: %0.2lf", o->z);
	else if (waypoint_index != (uint32_t) -1)
		snprintf(buffer, sizeof(buffer), "Z: %0.2lf", sci_ui.waypoint[waypoint_index][2]);
	else
		snprintf(buffer, sizeof(buffer), "Z:");
	y += yinc;
	sng_abs_xy_draw_string(buffer, TINY_FONT, x, y);

	if (o) {
		double vx = o->x - lastx;
		double vy = o->y - lasty;
		double vz = o->z - lastz;
		double v = 3.0 * dist3d(vx, vy, vz);
		lastx = o->x;
		lasty = o->y;
		lastz = o->z;
		v = 0.5 * (v + lastvel);
		if (v > 3000)
			v = 0;
		lastvel = v;
		if (update_display)
			velocity = v;
		snprintf(buffer, sizeof(buffer), "VELOCITY: %0.1lf", velocity);
	} else {
		snprintf(buffer, sizeof(buffer), "VELOCITY: 0.0");
	}
	y += yinc;
	sng_abs_xy_draw_string(buffer, TINY_FONT, x, y);

	if (o || waypoint_index != (uint32_t) -1) {
		if (o) {
			dx = o->x - ship->x;
			dy = o->y - ship->y;
			dz = o->z - ship->z;
		} else {
			dx = sci_ui.waypoint[waypoint_index][0] - ship->x;
			dy = sci_ui.waypoint[waypoint_index][1] - ship->y;
			dz = sci_ui.waypoint[waypoint_index][2] - ship->z;
		}

		union quat q_to_o;
		union vec3 dir_forward = {{1, 0, 0}};
		union vec3 dir_to_o = {{dx, dy, dz}};

		if (update_display) {
			quat_from_u2v(&q_to_o, &dir_forward, &dir_to_o, 0);
			to_snis_heading_mark(&q_to_o, &bearing, &mark);
		}

		snprintf(buffer,  sizeof(buffer), "BEARING: %0.1lf", radians_to_degrees(bearing));
		snprintf(buffer2, sizeof(buffer2), "MARK: %0.1lf", radians_to_degrees(mark));
	} else {
		snprintf(buffer,  sizeof(buffer), "BEARING:");
		snprintf(buffer2, sizeof(buffer2), "MARK:");
	}
	y += yinc;
	sng_abs_xy_draw_string(buffer, TINY_FONT, x, y);
	y += yinc;
	sng_abs_xy_draw_string(buffer2, TINY_FONT, x, y);

	if (o || waypoint_index != (uint32_t) -1) {
		range = dist3d(dx, dy, dz);
		if (update_display)
			dejittered_range = (range + last_range / 2); /* avg of last 2 for some hysteresis */
		snprintf(buffer, sizeof(buffer), "RANGE: %8.2lf", dejittered_range);
	} else {
		snprintf(buffer, sizeof(buffer), "RANGE:");
	}
	y += yinc;
	sng_abs_xy_draw_string(buffer, TINY_FONT, x, y);

	if (o || waypoint_index != (uint32_t) -1) {
		/* closing rate is avg of last 2 for hysteresis */
		closing_rate = (frame_rate_hz * (last_range - range) + last_closing_rate) / 2.0;
		last_range = range;
		last_closing_rate = closing_rate;
		if (update_display)
			dejittered_closing_rate = closing_rate;
		snprintf(buffer, sizeof(buffer), "CLOSING RATE: %0.2lf", dejittered_closing_rate);
	} else {
		snprintf(buffer, sizeof(buffer), "CLOSING RATE:");
	}
	y += yinc;
	sng_abs_xy_draw_string(buffer, TINY_FONT, x, y);

	/* Mininum closing rate of 2.0 is somewhat arbitrary, but very low values
	 * will cause strange ETA results seen e.g. while orbiting a planet with
	 * science targeting the planet. If you're not closing > 2.0 we assume
	 * you're not actually trying to get there.
	 */
	if ((o || waypoint_index != (uint32_t) -1) && dejittered_closing_rate > 2.0) {
		if (update_display) {
			eta_mins = (int) (round(dejittered_range) / round(dejittered_closing_rate)) / 60;
			eta_secs = (int) (round(dejittered_range) / round(dejittered_closing_rate)) % 60;
		}
		snprintf(buffer, sizeof(buffer), "ETA: %i MINS %i SECS", eta_mins, eta_secs);
	} else {
		snprintf(buffer, sizeof(buffer), "ETA:");
	}
	y += yinc;
	sng_abs_xy_draw_string(buffer, TINY_FONT, x, y);

	if (o || waypoint_index != (uint32_t) -1) {
		snprintf(buffer, sizeof(buffer), "WARP FACTOR: %2.2lf", 10.0 * range / (XKNOWN_DIM / 2.0));
	} else {
		snprintf(buffer, sizeof(buffer), "WARP FACTOR:");
	}
	y += yinc;
	sng_abs_xy_draw_string(buffer, TINY_FONT, x, y);

	gx1 = x;
	gy1 = y + 20 * SCREEN_HEIGHT / 600;
	gx2 = SCIENCE_DATA_X + SCIENCE_DATA_W - 10 * SCREEN_WIDTH / 800;
	gy2 = SCIENCE_DATA_Y + SCIENCE_DATA_H - 40 * SCREEN_HEIGHT / 600;
	draw_science_graph(w, ship, o, gx1, gy1, gx2, gy2);

	if (o && o->sdata.science_text) {
		sng_set_foreground(UI_COLOR(science_annotation));
		sng_abs_xy_draw_string(o->sdata.science_text, PICO_FONT, txx(20), txy(300));
	}
}

static void draw_science_waypoints(GtkWidget *w)
{
	char buffer[100];
	int i;
	int x, y;

	if (sci_ui.details_mode != SCI_DETAILS_MODE_WAYPOINTS)
		return;
	for (i = 0; i < sci_ui.nwaypoints; i++) {
		ui_unhide_widget(sci_ui.clear_waypoint_button[i]);
		ui_unhide_widget(sci_ui.select_waypoint_button[i]);
	}
	for (i = sci_ui.nwaypoints; i < MAXWAYPOINTS; i++) {
		ui_hide_widget(sci_ui.clear_waypoint_button[i]);
		ui_hide_widget(sci_ui.select_waypoint_button[i]);
	}
	sng_set_foreground(UI_COLOR(sci_wireframe));

	x = snis_text_input_box_get_x(sci_ui.waypoint_input[0]);
	y = snis_text_input_box_get_y(sci_ui.waypoint_input[0]) - font_lineheight[NANO_FONT];
	sng_abs_xy_draw_string("X", NANO_FONT, x, y);
	x = snis_text_input_box_get_x(sci_ui.waypoint_input[1]);
	y = snis_text_input_box_get_y(sci_ui.waypoint_input[1]) - font_lineheight[NANO_FONT];
	sng_abs_xy_draw_string("Y", NANO_FONT, x, y);
	x = snis_text_input_box_get_x(sci_ui.waypoint_input[2]);
	y = snis_text_input_box_get_y(sci_ui.waypoint_input[2]) - font_lineheight[NANO_FONT];
	sng_abs_xy_draw_string("Z", NANO_FONT, x, y);

	sng_abs_xy_draw_string("WAYPOINT", NANO_FONT, txx(100), txy(180));
	sng_abs_xy_draw_string("X", NANO_FONT, txx(250), txy(180));
	sng_abs_xy_draw_string("Y", NANO_FONT, txx(350), txy(180));
	sng_abs_xy_draw_string("Z", NANO_FONT, txx(450), txy(180));
	for (i = 0; i < sci_ui.nwaypoints; i++) {
		if (i != curr_science_waypoint)
			sng_set_foreground(UI_COLOR(sci_wireframe));
		else
			sng_set_foreground(UI_COLOR(sci_selected_waypoint));
		snprintf(buffer, sizeof(buffer), "WP-%02d", i);
		sng_abs_xy_draw_string(buffer, NANO_FONT, txx(100), txy(210 + i * 25));
		snprintf(buffer, sizeof(buffer), "%10.1lf", sci_ui.waypoint[i][0]);
		sng_abs_xy_draw_string(buffer, NANO_FONT, txx(200), txy(210 + i * 25));
		snprintf(buffer, sizeof(buffer), "%10.1lf", sci_ui.waypoint[i][1]);
		sng_abs_xy_draw_string(buffer, NANO_FONT, txx(300), txy(210 + i * 25));
		snprintf(buffer, sizeof(buffer), "%10.1lf", sci_ui.waypoint[i][2]);
		sng_abs_xy_draw_string(buffer, NANO_FONT, txx(400), txy(210 + i * 25));
	}
	sng_set_foreground(UI_COLOR(sci_wireframe));
	snis_draw_rectangle(0, txx(5), txy(70), txx(760), txy(480));
}

static void science_details_draw_atmosphere_data(GtkWidget *w, GdkGC *gc,
				struct planetary_atmosphere_profile *atm)
{
	int i;
	int y, yinc = 20 * SCREEN_HEIGHT / 600;
	y = yinc * 4;
	char buf[100], compound_name[100];

	sng_abs_xy_draw_string("ATMOSPHERIC DATA:", TINY_FONT, 10, y); y += yinc;
	yinc = 15 * SCREEN_HEIGHT / 600;
	snprintf(buf, sizeof(buf), "%30s: %.0f Pa", "PRESSURE", atm->pressure);
	sng_abs_xy_draw_string(buf, NANO_FONT, 10, y); y += yinc;
	snprintf(buf, sizeof(buf), "%30s: %.0f K / %.0f C / %.0f F", "TEMPERATURE",
		atm->temperature, atm->temperature - 273.15, atm->temperature * 9.0 / 5.0 - 459.67);
	sng_abs_xy_draw_string(buf, NANO_FONT, 10, y); y += yinc;
	for (i = 0; i < atm->nmajor; i++) {
		int compound = atm->major_compound[i];
		char *name = atmospheric_compound[compound].name;
		char *symbol = atmospheric_compound[compound].symbol;
		snprintf(compound_name, sizeof(compound_name), "%s (%s)", name, symbol);
		snprintf(buf, sizeof(buf), "%30s: %3.2f%%\n", compound_name, atm->major_fraction[i] * 100.0);
		sng_abs_xy_draw_string(buf, NANO_FONT, 10, y); y += yinc;
	}

	for (i = 0; i < atm->nminor; i++) {
		int compound = atm->minor_compound[i];
		char *name = atmospheric_compound[compound].name;
		char *symbol = atmospheric_compound[compound].symbol;
		snprintf(compound_name, sizeof(compound_name), "%s (%s)", name, symbol);
		snprintf(buf, sizeof(buf), "%30s: %4.2f ppm\n", compound_name, atm->minor_ppm[i]);
		sng_abs_xy_draw_string(buf, NANO_FONT, 10, y); y += yinc;
	}
}

static void draw_science_details(GtkWidget *w, GdkGC *gc)
{
	struct entity *e = NULL;
	struct mesh *m;
	char buf[100];
	float angle;
	union quat orientation;
	int y, yinc = 15 * SCREEN_HEIGHT / 600;
	int i;
	int sdf = NANO_FONT;

	if (science_cam_timer < 1000)
		science_cam_timer += (int) (0.3 * (1000.0 - (float) science_cam_timer));

	if (!curr_science_guy) {
		sng_center_xy_draw_string("NO SCAN TARGET SELECTED", sdf, txx(260), txy(300));
		return;
	}
	if (!curr_science_guy->entity) {
		/* This mostly should not happen. */
		sng_center_xy_draw_string("SCAN TARGET NOT IDENTIFIABLE", sdf, txx(260), txy(300));
		return;
	}

	set_renderer(sciecx, WIREFRAME_RENDERER | BLACK_TRIS);
	m = NULL;
	if (curr_science_guy->type == OBJTYPE_PLANET ||
		curr_science_guy->type == OBJTYPE_BLACK_HOLE)
		m = low_poly_sphere_mesh;
	else if (curr_science_guy->entity)
		m = entity_get_mesh(curr_science_guy->entity);
	angle = (M_PI / 180.0) * (timer % 360);
	if (m) {
		if (curr_science_guy->type == OBJTYPE_STARBASE ||
			curr_science_guy->type == OBJTYPE_PLANET ||
			curr_science_guy->type == OBJTYPE_BLACK_HOLE) {
			e = add_entity(sciecx, m, 0.0, -m->radius, m->radius * 0.2, UI_COLOR(sci_wireframe));
			quat_init_axis(&orientation, 0.0, 0.0, 1.0, angle);
		} else {
			e = add_entity(sciecx, m, 0.0, m->radius * 0.2, -m->radius, UI_COLOR(sci_wireframe));
			quat_init_axis(&orientation, 0.0, 1.0, 0.0, angle);
			if (e && curr_science_guy->type == OBJTYPE_SPACEMONSTER)
				sci_nav_add_tentacles(sciecx, curr_science_guy, e);
		}
		if (e)
			update_entity_orientation(e, &orientation);
		if (curr_science_guy->type == OBJTYPE_STARBASE) {
			camera_set_pos(sciecx, m->radius * 4, 1000 - science_cam_timer % 1000, m->radius * 2);
			camera_assign_up_direction(sciecx, 0.0, 0.0, 1.0);
		} else if (curr_science_guy->type == OBJTYPE_PLANET ||
			curr_science_guy->type == OBJTYPE_BLACK_HOLE) {
			camera_set_pos(sciecx, m->radius * 6, 1000 - science_cam_timer % 1000, m->radius * 2);
			camera_assign_up_direction(sciecx, 0.0, 0.0, 1.0);
		} else {
			camera_assign_up_direction(sciecx, 0.0, 1.0, 0.0);
			camera_set_pos(sciecx, -m->radius * 4, m->radius * 1, 1000 - science_cam_timer % 1000);
		}
		camera_look_at(sciecx, (float) 0, (float) 0, (float) m->radius / 2.0);
		camera_set_parameters(sciecx, 0.5, 8000.0,
					SCREEN_WIDTH, SCREEN_HEIGHT, ANGLE_OF_VIEW * M_PI / 180.0);
		set_lighting(sciecx, -m->radius * 4, 0, m->radius);
		render_entities(sciecx);
	}
	if (e)
		remove_entity(sciecx, e);

	y = SCREEN_HEIGHT - 200 * SCREEN_HEIGHT / 600;
	if (curr_science_guy->type == OBJTYPE_SHIP1 ||
		curr_science_guy->type == OBJTYPE_SHIP2) {
		snprintf(buf, sizeof(buf), "LIFEFORMS: %d", curr_science_guy->tsd.ship.lifeform_count);
	} else {
		if (curr_science_guy->type == OBJTYPE_STARBASE) {
			snprintf(buf, sizeof(buf), "LIFEFORMS: %d", curr_science_guy->tsd.starbase.lifeform_count);
		} else {
			buf[0] = '\0';
		}
	}
	sng_set_foreground(UI_COLOR(sci_details_text));
	if (buf[0])
		sng_abs_xy_draw_string(buf, sdf, 10, y);
	y += yinc;
	if (curr_science_guy->type == OBJTYPE_PLANET) {
		static uint32_t last = 0xffffffff;
		struct mtwist_state *mt;
		static char planet_desc[500];
		char tmpbuf[60];
		int i, len, j;
		char *planet_type_str;
		enum planet_type pt;
		struct planetary_atmosphere_profile *atm =
			planetary_atmosphere_by_index(curr_science_guy->tsd.planet.atmosphere_type);

		science_details_draw_atmosphere_data(w, gc, atm);

		struct planet_data *p = &curr_science_guy->tsd.planet;

		planet_type_str = solarsystem_assets->planet_type[p->solarsystem_planet_type];
		pt = planet_type_from_string(planet_type_str);
		snprintf(buf, sizeof(buf), "MASS: %.2f EU / DIAM: %.2f EU",
				planetary_mass(p->radius, pt), planetary_diameter(p->radius, pt));
		sng_abs_xy_draw_string(buf, sdf, 10, y);
		y += yinc;

		if (p->custom_description) {
			strncpy(planet_desc, p->custom_description, 255);
			planet_desc[256] = '\0';
			for (i = 0; planet_desc[i] != '\0'; i++)
				planet_desc[i] = toupper(planet_desc[i]);
		} else if (p->description_seed != last) {
			mt = mtwist_init(p->description_seed);
			planet_description(mt, planet_desc, 500, 40, pt);
			last = p->description_seed;
			mtwist_free(mt);
			for (i = 0; planet_desc[i] != '\0'; i++)
				planet_desc[i] = toupper(planet_desc[i]);
		}
		snprintf(buf, sizeof(buf), "GOVERNMENT: %s", government_name[p->government]);
		sng_abs_xy_draw_string(buf, sdf, 10, y);
		y += yinc;
		snprintf(buf, sizeof(buf), "TECH LEVEL: %s", tech_level_name[p->tech_level]);
		sng_abs_xy_draw_string(buf, sdf, 10, y);
		y += yinc;
		snprintf(buf, sizeof(buf), "ECONOMY: %s", economy_name[p->economy]);
		sng_abs_xy_draw_string(buf, sdf, 10, y);
		y += yinc;
		switch (p->security) {
		case 0:
			strcpy(buf, "SECURITY: LOW");
			break;
		case 1:
			strcpy(buf, "SECURITY: MEDIUM");
			break;
		case 2:
			strcpy(buf, "SECURITY: HIGH");
			break;
		default:
			strcpy(buf, "SECURITY: UNKNOWN");
			break;
		}
		sng_abs_xy_draw_string(buf, sdf, 10, y);
		y += yinc;

		/* break planet_desc into multiple lines */
		len = strlen(planet_desc);
		j = 0;
		for (i = 0; i <= len; i++) {
			if (planet_desc[i] == '\n' || planet_desc[i] == '\0') {
				tmpbuf[j] = '\0';
				sng_abs_xy_draw_string(tmpbuf, sdf, 10, y);
				y += 15 * SCREEN_HEIGHT / 600;
				j = 0;
			} else {
				tmpbuf[j] = planet_desc[i];
				j++;
			}
		}
	}
	if (curr_science_guy->type == OBJTYPE_ASTEROID) {
		struct asteroid_data *a = &curr_science_guy->tsd.asteroid;
		snprintf(buf, sizeof(buf), "%3.0f%% CARBONACEOUS", 100.0 * a->carbon / 255.0);
		sng_abs_xy_draw_string(buf, sdf, 10, y);
		y += yinc;
		snprintf(buf, sizeof(buf), "%3.0f%% SILICATES", 100.0 * a->silicates / 255.0);
		sng_abs_xy_draw_string(buf, sdf, 10, y);
		y += yinc;
		snprintf(buf, sizeof(buf), "%3.0f%% NICKEL/IRON", 100.0 * a->nickeliron / 255.0);
		sng_abs_xy_draw_string(buf, sdf, 10, y);
		y += yinc;
		snprintf(buf, sizeof(buf), "%3.0f%% PRECIOUS METALS", 100.0 * a->preciousmetals / 255.0);
		sng_abs_xy_draw_string(buf, sdf, 10, y);
		y += yinc;
	}
	if (curr_science_guy->type == OBJTYPE_SHIP2 || curr_science_guy->type == OBJTYPE_SHIP1) {
		struct ship_data *s = &curr_science_guy->tsd.ship;
		if (curr_science_guy->type == OBJTYPE_SHIP1)
			snprintf(buf, sizeof(buf), "WEAPONRY: TORPEDOES, BLASTERS AND MISSILES");
		else if (!ship_type[s->shiptype].has_lasers && !ship_type[s->shiptype].has_torpedoes)
			snprintf(buf, sizeof(buf), "WEAPONRY: NONE");
		else if (ship_type[s->shiptype].has_lasers && ship_type[s->shiptype].has_torpedoes)
			snprintf(buf, sizeof(buf), "WEAPONRY: TORPEDOES AND LASERS");
		else if (ship_type[s->shiptype].has_lasers)
			snprintf(buf, sizeof(buf), "WEAPONRY: LASERS");
		else
			snprintf(buf, sizeof(buf), "WEAPONRY: TORPEDOES");
		sng_abs_xy_draw_string(buf, sdf, 10, y);
		y += yinc;

		snprintf(buf, sizeof(buf), "MASS: %.1f MT", ship_type[s->shiptype].mass_kg / 1000.0);
		sng_abs_xy_draw_string(buf, sdf, 10, y);
		y += yinc;

		for (i = 0; i < ship_type[s->shiptype].ncargo_bays; i++) {
			struct cargo_container_contents *cbc = &s->cargo[i].contents;
			if (cbc->item < 0 || cbc->qty <= 0.0)
				continue;
			if (i == 0) {
				snprintf(buf, sizeof(buf), "PROBABLE CARGO:");
				sng_abs_xy_draw_string(buf, sdf, 10, y);
				y += yinc;
			}
			if (cbc->item >= 0 && cbc->item < ncommodities) {
				snprintf(buf, sizeof(buf), "- %4.2f %s %s", cbc->qty,
					commodity[cbc->item].unit, commodity[cbc->item].scans_as);
				sng_abs_xy_draw_string(buf, sdf, 10, y);
			} else {
				sng_abs_xy_draw_string("UNKNOWN", sdf, 10, y);
			}
			y += yinc;
		}
	} else if (curr_science_guy->type == OBJTYPE_CARGO_CONTAINER) {
		struct cargo_container_contents *ccc = &curr_science_guy->tsd.cargo_container.contents;
		snprintf(buf, sizeof(buf), "PROBABLE CONTENTS:");
		sng_abs_xy_draw_string(buf, sdf, 10, y);
		y += yinc;
		if (ccc->item >= 0 && ccc->item < ncommodities) {
			snprintf(buf, sizeof(buf), "- %4.2f %s %s", ccc->qty,
				commodity[ccc->item].unit, commodity[ccc->item].scans_as);
			sng_abs_xy_draw_string(buf, sdf, 10, y);
		} else {
			sng_abs_xy_draw_string("UNKNOWN", sdf, 10, y);
		}
	}
}

static void draw_science_location_indicator(struct snis_entity *o)
{
	char ssname[12], buf[80];

	strncpy(ssname, solarsystem_name, 11);
	ssname[11] = '\0';
	uppercase(ssname);
	snprintf(buf, sizeof(buf), "%s SYSTEM", ssname);
	sng_abs_xy_draw_string(buf, NANO_FONT, txx(200), txy(10));
	snprintf(buf, sizeof(buf), "POSITION:");
	sng_abs_xy_draw_string(buf, NANO_FONT, txx(360), txy(10));
	snprintf(buf, sizeof(buf), "X: %5.2lf,", o->x);
	sng_abs_xy_draw_string(buf, NANO_FONT, txx(415), txy(10));
	snprintf(buf, sizeof(buf), "Y: %5.2lf,", o->y);
	sng_abs_xy_draw_string(buf, NANO_FONT, txx(480), txy(10));
	snprintf(buf, sizeof(buf), "Z: %5.2lf", o->z);
	sng_abs_xy_draw_string(buf, NANO_FONT, txx(555), txy(10));
}
 
static void show_3d_science(GtkWidget *w, struct snis_entity *o, int current_zoom)
{
	int cx, cy, r;
	double zoom;

	cx = SCIENCE_SCOPE_CX;
	cy = SCIENCE_SCOPE_CY;
	r = SCIENCE_SCOPE_R;
	zoom = (MAX_SCIENCE_SCREEN_RADIUS - MIN_SCIENCE_SCREEN_RADIUS) *
			(current_zoom / 255.0) + MIN_SCIENCE_SCREEN_RADIUS;
	sng_set_foreground(UI_COLOR(sci_ball_ring));
	sng_draw_circle(0, cx, cy, r);
	draw_all_the_3d_science_guys(w, o, zoom * 4.0, current_zoom * 4.0);
}

static void show_science(GtkWidget *w)
{
	struct snis_entity *o;
	double zoom;
	static int current_zoom = 0;

	if (!(o = find_my_ship()))
		return;

	snis_slider_set_input(sci_ui.scizoom, o->tsd.ship.scizoom/255.0 );

	current_zoom = newzoom(current_zoom, o->tsd.ship.scizoom);

	sng_set_foreground(UI_COLOR(sci_coords));
	draw_science_location_indicator(o);
	zoom = (MAX_SCIENCE_SCREEN_RADIUS - MIN_SCIENCE_SCREEN_RADIUS) *
			(current_zoom / 255.0) + MIN_SCIENCE_SCREEN_RADIUS;
	sng_set_foreground(DARKGREEN); /* zzzz check this */
	switch (sci_ui.details_mode) {
	case SCI_DETAILS_MODE_SCIPLANE:
		draw_sciplane_display(w, o, zoom);
		break;
	case SCI_DETAILS_MODE_WAYPOINTS:
		draw_science_waypoints(w);
		break;
	case SCI_DETAILS_MODE_DETAILS:
		draw_science_details(w, gc);
		draw_science_data(w, o, curr_science_guy, curr_science_waypoint);
		break;
	case SCI_DETAILS_MODE_THREED:
		show_3d_science(w, o, current_zoom);
		draw_science_data(w, o, curr_science_guy, curr_science_waypoint);
		break;
	default: /* shouldn't happen */
		draw_science_data(w, o, curr_science_guy, curr_science_waypoint);
		break;
	}
	if (o->tsd.ship.power_data.sensors.i < idiot_light_threshold && (timer & 0x08)) {
		sng_set_foreground(UI_COLOR(sci_warning));
		sng_center_xy_draw_string("LOW SENSOR POWER", NANO_FONT, SCREEN_WIDTH / 2, txy(27));
	}
	if (sci_ui.low_tractor_power_timer > 0) {
		if (o->tsd.ship.power_data.tractor.i < idiot_light_threshold && (timer & 0x08)) {
			int x, y;

			x = snis_button_get_x(sci_ui.tractor_button);
			y = snis_button_get_y(sci_ui.tractor_button);
			sng_set_foreground(UI_COLOR(sci_warning));
			sng_abs_xy_draw_string("LOW TRACTOR POWER", NANO_FONT, x, y - 20);
		}
		sci_ui.low_tractor_power_timer--;
	}
	populate_science_pull_down_menu();
	show_common_screen(w, "SCIENCE");
}

static void update_comms_ui_visibility(struct snis_entity *o)
{
	int i;

	if (o->tsd.ship.comms_crypto_mode) {
		/* RTS button visibility is handled via comms_setup_rts_buttons(); */
		/* custom button visibility is handled in process_custom_button(); */
		ui_hide_widget(comms_ui.tw);
		ui_hide_widget(comms_ui.comms_onscreen_button);
		ui_hide_widget(comms_ui.nav_onscreen_button);
		ui_hide_widget(comms_ui.weap_onscreen_button);
		ui_hide_widget(comms_ui.eng_onscreen_button);
		ui_hide_widget(comms_ui.damcon_onscreen_button);
		ui_hide_widget(comms_ui.sci_onscreen_button);
		ui_hide_widget(comms_ui.main_onscreen_button);
		ui_unhide_widget(comms_ui.cryptanalysis_button);
		ui_hide_widget(comms_ui.comms_transmit_button);
		ui_hide_widget(comms_ui.red_alert_button);
		ui_hide_widget(comms_ui.hail_mining_bot_button);
		ui_hide_widget(comms_ui.mainscreen_comms);
		ui_hide_widget(comms_ui.hail_button);
		ui_hide_widget(comms_ui.channel_button);
		ui_hide_widget(comms_ui.manifest_button);
		ui_hide_widget(comms_ui.computer_button);
		ui_hide_widget(comms_ui.eject_button);
		ui_hide_widget(comms_ui.help_button);
		ui_hide_widget(comms_ui.about_button);
		ui_hide_widget(comms_ui.comms_input);
		ui_hide_widget(comms_ui.mainzoom_slider);

		ui_unhide_widget(comms_ui.crypto_reset);
		for (i = 0; i < 26; i++)
			ui_unhide_widget(comms_ui.crypt_alpha[i]);
	} else {
		/* RTS button visibility is handled via comms_setup_rts_buttons(); */
		/* custom button visibility is handled in process_custom_button(); */
		ui_unhide_widget(comms_ui.tw);
		ui_unhide_widget(comms_ui.comms_onscreen_button);
		ui_unhide_widget(comms_ui.nav_onscreen_button);
		ui_unhide_widget(comms_ui.weap_onscreen_button);
		ui_unhide_widget(comms_ui.eng_onscreen_button);
		ui_unhide_widget(comms_ui.damcon_onscreen_button);
		ui_unhide_widget(comms_ui.sci_onscreen_button);
		ui_unhide_widget(comms_ui.main_onscreen_button);
		ui_unhide_widget(comms_ui.cryptanalysis_button);
		ui_unhide_widget(comms_ui.comms_transmit_button);
		ui_unhide_widget(comms_ui.red_alert_button);
		ui_unhide_widget(comms_ui.hail_mining_bot_button);
		ui_unhide_widget(comms_ui.mainscreen_comms);
		ui_unhide_widget(comms_ui.hail_button);
		ui_unhide_widget(comms_ui.channel_button);
		ui_unhide_widget(comms_ui.manifest_button);
		ui_unhide_widget(comms_ui.computer_button);
		ui_unhide_widget(comms_ui.eject_button);
		ui_unhide_widget(comms_ui.help_button);
		ui_unhide_widget(comms_ui.about_button);
		ui_unhide_widget(comms_ui.comms_input);
		ui_unhide_widget(comms_ui.mainzoom_slider);
		for (i = 0; i < 26; i++)
			ui_hide_widget(comms_ui.crypt_alpha[i]);
		ui_hide_widget(comms_ui.crypto_reset);
		ui_set_widget_focus(uiobjs, comms_ui.comms_input);
	}
}

static void show_comms_cryptanalysis(struct snis_entity *o)
{
	int i, len;
	float x, y, dx;
	struct scipher_key *key = NULL;
	float crypt_alphax, crypt_alphay;

	if (!o->tsd.ship.comms_crypto_mode)
		return;

	y = txy(100);
	x = txx(10);
	dx = txx(12);

	key = scipher_make_key(cipher_key);

	len = strlen(enciphered_text);
	if (len == 0) {
		sng_center_xy_draw_string("NO ENCRYPTED MESSAGES AVAILABLE", TINY_FONT, 400, 300);
		return;
	}
	for (i = 0; i < len; i++) {
		char ciphertext[2];
		char plaintext[2];
		ciphertext[0] = toupper(enciphered_text[i]);
		ciphertext[1] = '\0';
		plaintext[0] = scipher_decipher_char(ciphertext[0], key);
		plaintext[1] = '\0';
		sng_set_foreground(UI_COLOR(comms_text));
		sng_abs_xy_draw_string(ciphertext, TINY_FONT, x, y);
		sng_set_foreground(UI_COLOR(comms_plaintext));
		sng_abs_xy_draw_string(plaintext, TINY_FONT, x, y + dx * 1.5);
		x = x + dx;
		if (x > txx(780)) {
			x = txx(10);
			y = y + dx * 4;
		}
	}
	scipher_key_free(key);

	sng_set_foreground(UI_COLOR(comms_text));
	crypt_alphax = txx(20);
	crypt_alphay = txy(415);
	for (i = 0; i < 26; i++) {
		char label[2];
		label[0] = i + 'A';
		label[1] = '\0';
		sng_abs_xy_draw_string(label, TINY_FONT, crypt_alphax, crypt_alphay);
		crypt_alphax += txx((751 - 20) / 13.0);
		if (crypt_alphax >= txx(750)) {
			crypt_alphax = txx(20);
			crypt_alphay += txy(70);
		}
	}

	for (i = 0; i < 26; i++) {
		char letter[2];
		letter[0] = cipher_alpha_by_freq[i];
		letter[1] = '\0';
		sng_abs_xy_draw_string(letter, TINY_FONT, txx(10 + txx(10) * i), txy(565));
	}

	char english_freq[] = "ETAONIRSH";
	for (i = 0; english_freq[i]; i++) {
		char letter[2];
		letter[0] = english_freq[i];
		letter[1] = '\0';
		sng_abs_xy_draw_string(letter, TINY_FONT, txx(10 + txx(10) * i), txy(585));
	}
}

static void show_comms(GtkWidget *w)
{
	struct snis_entity *o;
	char current_date[32], comms_buffer[100];

	if (!(o = find_my_ship()))
		return;

	update_comms_ui_visibility(o); /* switches between cryptanalysis and normal mode */

	show_comms_cryptanalysis(o);

	float shield_ind_x_center = txx(710);
	float shield_ind_y_center = txy(485);

	if (!o->tsd.ship.comms_crypto_mode) {
		if (o->sdata.shield_strength < 15) {
			sng_set_foreground(UI_COLOR(comms_warning));
			sng_center_xy_draw_string("SHIELDS ARE DOWN", NANO_FONT,
							shield_ind_x_center, shield_ind_y_center);
		} else {
			sng_set_foreground(UI_COLOR(comms_good_status));
			char buf[80];
			snprintf(buf, sizeof(buf), "SHIELDS ARE %d%%", (int)(o->sdata.shield_strength / 2.55));
			sng_center_xy_draw_string(buf, NANO_FONT, shield_ind_x_center, shield_ind_y_center);
		}
	}
	sng_set_foreground(UI_COLOR(comms_text));
	format_date(current_date, sizeof(current_date), universe_timestamp());
	snprintf(comms_buffer, sizeof(comms_buffer), "TIME: %s", current_date);
	sng_abs_xy_draw_string(comms_buffer, TINY_FONT, txx(25), txy(55));
	if (global_rts_mode && !o->tsd.ship.comms_crypto_mode) {
		snprintf(comms_buffer, sizeof(comms_buffer), "FUNDS: $%6.0f", o->tsd.ship.wallet);
		sng_abs_xy_draw_string(comms_buffer, TINY_FONT, txx(625), txy(350));
	}
	if (!o->tsd.ship.comms_crypto_mode) {
		snprintf(comms_buffer, sizeof(comms_buffer), "CHANNEL: %u", comms_ui.channel);
		sng_center_xy_draw_string(comms_buffer, NANO_FONT, shield_ind_x_center, shield_ind_y_center - txy(15));
	}
	if (global_rts_mode) {
		if (o->tsd.ship.rts_active_button != 255) {
			snis_draw_rectangle(0, txx(10), txy(350), SCREEN_WIDTH - txx(200), txy(150));
			if (o->tsd.ship.rts_active_button < NUM_RTS_BASES) {
				struct snis_entity *starbase = rts_find_my_starbase(o->tsd.ship.rts_active_button);
				if (starbase && starbase->tsd.starbase.time_left_to_build != 0) {
					snprintf(comms_buffer, sizeof(comms_buffer),
						"STARBASE %02d: BUILDING %s, %d SECONDS REMAINING",
						starbase->tsd.starbase.starbase_number,
						rts_unit_type(starbase->tsd.starbase.build_unit_type)->name,
						starbase->tsd.starbase.time_left_to_build / 10);
					disable_comms_rts_unit_ordering_buttons();
				} else {
					snprintf(comms_buffer, sizeof(comms_buffer),
						"STARBASE %02d", o->tsd.ship.rts_active_button);
					enable_comms_rts_unit_ordering_buttons();
				}
				sng_abs_xy_draw_string(comms_buffer, TINY_FONT, txx(22), txy(365));
			} else if (o->tsd.ship.rts_active_button == RTS_HOME_PLANET_BUTTON) {
				struct snis_entity *home_planet = rts_find_my_home_planet();
				if (home_planet && home_planet->tsd.planet.time_left_to_build != 0) {
					snprintf(comms_buffer, sizeof(comms_buffer),
						"HOME PLANET: BUILDING %s, %d SECONDS REMAINING",
						rts_unit_type(home_planet->tsd.planet.build_unit_type)->name,
						home_planet->tsd.planet.time_left_to_build / 10);
					disable_comms_rts_unit_ordering_buttons();
				} else  {
					snprintf(comms_buffer, sizeof(comms_buffer),
						"HOME PLANET (faction = %d/%d)",
						o->sdata.faction, home_planet ? home_planet->sdata.faction : 255);
					enable_comms_rts_unit_ordering_buttons();
				}
				sng_abs_xy_draw_string(comms_buffer, TINY_FONT, txx(22), txy(365));
			} else if (o->tsd.ship.rts_active_button == RTS_FLEET_BUTTON) {
				snprintf(comms_buffer, sizeof(comms_buffer), "FLEET");
				sng_abs_xy_draw_string(comms_buffer, TINY_FONT, txx(22), txy(365));
			}
		}
	}
	if (o->tsd.ship.missile_lock_detected && timer & 0x08) {
		sng_set_foreground(BLACK);
		snis_draw_rectangle(1, txx(100), txy(125), txx(SCREEN_WIDTH - 100), txy(175));
		sng_set_foreground(UI_COLOR(common_red_alert));
		sng_center_xy_draw_string("MISSILE LOCK DETECTED", SMALL_FONT, txx(400), txy(150));
	}
	show_common_screen(w, "COMMS");
}

static void send_demon_comms_packet_to_server(char *msg)
{
	if (demon_ui.captain_of < 0) {
		print_demon_console_color_msg(YELLOW, "YOU MUST BE IN CAPTAIN MODE TO DO THAT");
		return;
	}
	send_comms_packet_to_server(msg, OPCODE_DEMON_COMMS_XMIT, go[demon_ui.captain_of].id);
}

static void send_demon_clear_all_packet_to_server(void)
{
	queue_to_server(snis_opcode_pkt("b", OPCODE_DEMON_CLEAR_ALL));
}

static void toggle_demon_ai_debug_mode(void)
{
	queue_to_server(snis_opcode_pkt("b", OPCODE_TOGGLE_DEMON_AI_DEBUG_MODE));
}

static void request_universe_timestamp(void)
{
	queue_to_server(snis_opcode_pkt("b", OPCODE_REQUEST_UNIVERSE_TIMESTAMP));
}

static void toggle_demon_safe_mode(void)
{
	queue_to_server(snis_opcode_pkt("b", OPCODE_TOGGLE_DEMON_SAFE_MODE));
}

static int ux_to_usersx(double ux, float x1, float x2)
{
	return (((ux + 0.5 * XKNOWN_DIM) - x1) / (x2 - x1)) * SCREEN_WIDTH;
}

static int uz_to_usersy(double uz, float y1, float y2)
{
	return (((uz + 0.5 * XKNOWN_DIM) - y1) / (y2 - y1)) * SCREEN_HEIGHT * ASPECT_RATIO;
}

static int ur_to_usersr(double ur, float x1, float x2)
{
	return (ur * SCREEN_WIDTH) / (x2 - x1);
}

static double user_mousex_to_ux(double x, float x1, float x2)
{
	return x1 + (x / real_screen_width) * (x2 - x1);
}

static double user_mousey_to_uz(double y, float y1, float y2)
{
	return y1 + (y / real_screen_height) * (y2 - y1) / ASPECT_RATIO;
}

static int ux_to_demonsx(double ux)
{
	return (ux - demon_ui.ux1) / (demon_ui.ux2 - demon_ui.ux1) * SCREEN_WIDTH;
}

static int uz_to_demonsy(double uz)
{
	return (uz - demon_ui.uy1) / (demon_ui.uy2 - demon_ui.uy1) * SCREEN_HEIGHT * ASPECT_RATIO;
}

static double demon_mousex_to_ux(double x)
{
	return user_mousex_to_ux(x, demon_ui.ux1, demon_ui.ux2);
}

static double demon_mousey_to_uz(double y)
{
	return user_mousey_to_uz(y, demon_ui.uy1, demon_ui.uy2);
}

static double weapons_mousex_to_yaw(double x)
{
	double scaledx;
	double angle;

	scaledx = (0.9 * real_screen_width - x) / (real_screen_width * 0.8);
	angle = scaledx * 2 * M_PI + M_PI; 
	normalize_angle(&angle);
	return angle;
}

static double weapons_mousey_to_pitch(double y)
{
	double scaledy;

	scaledy = (real_screen_height - y) / (1.005 * real_screen_height);
	if (scaledy > 1.0f)
		scaledy = 1.0f;
	else
		if (scaledy < 0.0f)
			scaledy = 0.0f;
	return scaledy * M_PI / 2.0; 
}

static int demon_id_selected(uint32_t id)
{
	int i;

	for (i = 0; i < demon_ui.nselected; i++)
		if (demon_ui.selected_id[i] == id)
			return 1;
	return 0;
}

static void demon_select(uint32_t id)
{
	int old_captain = -1;

	if (demon_ui.nselected >= MAX_DEMON_SELECTABLE)
		return;
	demon_ui.selected_id[demon_ui.nselected] = id;
	demon_ui.nselected++;
	print_demon_console_msg("SELECTED OBJECT %d", id);
	if (demon_ui.buttonmode == DEMON_BUTTON_CAPTAINMODE) {
		int index = lookup_object_by_id(id);

		if (demon_ui.captain_of != -1) {
			queue_to_server(snis_opcode_pkt("bw", OPCODE_DEMON_DISPOSSESS,
				go[demon_ui.captain_of].id));
				old_captain = demon_ui.captain_of;
				demon_ui.captain_of = -1;
		}
		if (index >= 0 && (go[index].type == OBJTYPE_SHIP2 ||
			go[index].type == OBJTYPE_STARBASE)) {
			int new_captain;

			new_captain = lookup_object_by_id(id);
			if (new_captain != old_captain) {
				demon_ui.captain_of = lookup_object_by_id(id);
				demon_ui.exaggerated_scale_active = 0;
				demon_ui.desired_exaggerated_scale = 0.0;
				queue_to_server(snis_opcode_pkt("bw", OPCODE_DEMON_POSSESS,
					go[demon_ui.captain_of].id));
			}
		}
	}
}

static void demon_deselect(uint32_t id)
{
	int i;

	for (i = 0; i < demon_ui.nselected; i++) {
		if (demon_ui.selected_id[i] == id) {
			int index;
			print_demon_console_msg("DESELECTED %u\n", id);
			if (demon_ui.captain_of != -1) {
				index = lookup_object_by_id(id);
				if (demon_ui.captain_of == index) {
					queue_to_server(snis_opcode_pkt("bw", OPCODE_DEMON_DISPOSSESS,
						go[demon_ui.captain_of].id));
					demon_ui.captain_of = -1;
				}
			}
			if (i == demon_ui.nselected - 1) {
				demon_ui.nselected--;
				return;
			}
			demon_ui.nselected--;
			demon_ui.selected_id[i] = demon_ui.selected_id[demon_ui.nselected];
			return;
		}
	}
}

static void demon_select_none(void)
{
	print_demon_console_msg("NOTHING SELECTED");
	demon_ui.nselected = 0;
}

static void demon_button_press(int button, gdouble x, gdouble y)
{
	/* must be right mouse button so as not to conflict with 'EXECUTE' button. */
	if (demon_ui.use_3d) {
		demon_ui.press_mousex = sng_pixelx_to_screenx(x);
		demon_ui.press_mousey = sng_pixely_to_screeny(y);
		switch (button) {
		case 2:
			demon_ui.button2_pressed = 1;
			break;
		case 3:
			demon_ui.button3_pressed = 1;
			break;
		default:
			break;
		}
	} else {
		if (button == 3) {
			demon_ui.ix = demon_mousex_to_ux(x);
			demon_ui.iz = demon_mousey_to_uz(y);
			demon_ui.ix2 = demon_mousex_to_ux(x);
			demon_ui.iz2 = demon_mousey_to_uz(y);
			demon_ui.selectmode = 1;
		}
		if (button == 2) {
			demon_ui.move_from_x = x;
			demon_ui.move_from_y = y;
		}
	}
}

static inline int between(double a, double b, double v)
{
	return ((a <= v && v <= b) || (b <= v && v <= a));
}

static void demon_button_create_item(gdouble x, gdouble y, gdouble z)
{
	double ux, uy, uz;
	uint8_t item_type, data1, data2;

	if (demon_ui.use_3d) {
		ux = x;
		uy = y;
		uz = z;
	} else {
		ux = demon_mousex_to_ux(x);
		uz = demon_mousey_to_uz(z);
		uy = y;
	}

	data1 = 255;
	data2 = 255;

	switch (demon_ui.buttonmode) {
		case DEMON_BUTTON_SHIPMODE:
			item_type = OBJTYPE_SHIP2;
			data1 = demon_ui.shiptype;
			data2 = demon_ui.faction;
			break;
		case DEMON_BUTTON_STARBASEMODE:
			item_type = OBJTYPE_STARBASE;
			break;
		case DEMON_BUTTON_PLANETMODE:
			item_type = OBJTYPE_PLANET;
			break;
		case DEMON_BUTTON_BLACKHOLEMODE:
			item_type = OBJTYPE_BLACK_HOLE;
			break;
		case DEMON_BUTTON_ASTEROIDMODE:
			item_type = OBJTYPE_ASTEROID;
			break;
		case DEMON_BUTTON_NEBULAMODE:
			item_type = OBJTYPE_NEBULA;
			break;
		case DEMON_BUTTON_SPACEMONSTERMODE:
			item_type = OBJTYPE_SPACEMONSTER;
			break;
		default:
			return;
	}
	queue_to_server(snis_opcode_pkt("bbSSSbb", OPCODE_CREATE_ITEM, item_type,
			ux, (int32_t) UNIVERSE_DIM, uy, (int32_t) UNIVERSE_DIM, uz, (int32_t) UNIVERSE_DIM,
			data1, data2));
}

typedef int (*demon_select_test)(uint32_t oid);
typedef void (*demon_select_action)(uint32_t oid);

static void demon_select_and_act(double sx1, double sy1,
		double sx2, double sy2, demon_select_test dtest,
		demon_select_action dact1, demon_select_action dact2,
		int *ndact2)
{
	int i;

	pthread_mutex_lock(&universe_mutex);
	for (i = 0; i <= snis_object_pool_highest_object(pool); i++) {
		int sx, sy;
		struct snis_entity *o = &go[i];

		if (!o->alive)
			continue;
		sx = ux_to_demonsx(o->x + 0.5 * XKNOWN_DIM);
		sy = uz_to_demonsy(o->z + 0.5 * ZKNOWN_DIM);
		if (!between(sx1, sx2, sx) || !between(sy1, sy2, sy))
			continue;
		if (dtest(o->id)) {
			if (dact1)
				dact1(o->id);
		} else {
			if (dact2) {
				(*ndact2)++;
				dact2(o->id);
			}
		}
	}
	pthread_mutex_unlock(&universe_mutex);
}

static void demon_button2_release_3d(int button, gdouble x, gdouble y)
{
	demon_ui.release_mousex = x;
	demon_ui.release_mousey = y;
	demon_ui.button2_released = 1;
}

static void demon_button3_release_3d(int button, gdouble x, gdouble y)
{
	demon_ui.release_mousex = x;
	demon_ui.release_mousey = y;
	demon_ui.button3_released = 1;
}

static void demon_button3_release(int button, gdouble x, gdouble y)
{
	int nselected;
	double ox, oy;
	int sx1, sy1;

	/* If the item creation buttons selected, create item... */ 
	if (demon_ui.buttonmode > DEMON_BUTTON_NOMODE &&
		demon_ui.buttonmode < DEMON_BUTTON_DELETE) {
		demon_button_create_item(x, 0.0, y);
		return;
	}

	/* otherwise, selecting a thing or a location... */
	demon_ui.selectmode = 0;

	if (demon_ui.nselected >= MAX_DEMON_SELECTABLE)
		return;

	ox = x;
	oy = y;
	x = x * SCREEN_WIDTH / real_screen_width;
	y = y * SCREEN_HEIGHT / real_screen_height;

	sx1 = ux_to_demonsx(demon_ui.ix);
	sy1 = uz_to_demonsy(demon_ui.iz);

	if (hypot(sx1 - x, sy1 - y) >= 5) {
		/* multiple selection... */
		nselected = 0;
		demon_select_and_act(sx1, sy1, x, y,
			demon_id_selected,
			demon_deselect, demon_select, &nselected); 
	} else {
		/* single selection... */
		nselected = 0;
		demon_select_and_act(x - 7, y - 7, x + 7, y + 7,
			demon_id_selected,
			demon_deselect, demon_select, &nselected); 
		if (nselected == 0) {
			demon_ui.selectedx = demon_mousex_to_ux(ox);
			demon_ui.selectedz = demon_mousey_to_uz(oy);
		}
	}
}

static void demon_button2_release(int button, gdouble x, gdouble y)
{
	int i;
	double dx, dy, dz;

	if (demon_ui.nselected <= 0)
		return;

	/* Moving objects... */
	dx = demon_mousex_to_ux(x) - demon_mousex_to_ux(demon_ui.move_from_x);
	dy = 0.0;
	dz = demon_mousey_to_uz(y) - demon_mousey_to_uz(demon_ui.move_from_y);
	
	pthread_mutex_lock(&universe_mutex);
	for (i = 0; i < demon_ui.nselected; i++) {
		int index = lookup_object_by_id(demon_ui.selected_id[i]);
		queue_to_server(snis_opcode_pkt("bwSSSQ",
				OPCODE_DEMON_MOVE_OBJECT,
				demon_ui.selected_id[i],
				dx, (int32_t) UNIVERSE_DIM,
				dy, (int32_t) UNIVERSE_DIM,
				dz, (int32_t) UNIVERSE_DIM,
				&go[index].orientation));
	}
	pthread_mutex_unlock(&universe_mutex);
}

static void demon_button_release(int button, gdouble x, gdouble y)
{
	if (demon_ui.use_3d) {
		switch (button) {
		case 2:
			demon_button2_release_3d(button,
				sng_pixelx_to_screenx(x), sng_pixely_to_screeny(y));
			break;
		case 3:
			demon_button3_release_3d(button,
				sng_pixelx_to_screenx(x), sng_pixely_to_screeny(y));
			break;
		default:
			break;
		}
		return;
	}
	switch (button) {
	case 2:
		demon_button2_release(button, x, y);
		return;
	case 3:
		demon_button3_release(button, x, y);
		return;
	default:
		return;
	}
}

static void do_damcon_button_release(int button, gdouble x, gdouble y)
{
	switch (button) {
	case 3:
		robot_gripper_button_pressed(NULL);
		break;
	default:
		break;
	}
}

static void debug_draw_ship_patrol_route(uint8_t npoints, union vec3 patrol[],
	float ux1, float uy1, float ux2, float uy2)
{
	union vec3 *v1, *v2;
	float x1, y1, x2, y2;

	sng_set_foreground(UI_COLOR(demon_patrol_route));
	for (int i = 1; i <= npoints; i++) {
		v1 = &patrol[i - 1];
		v2 = &patrol[i % npoints];

		x1 = (float) ux_to_usersx((double) v1->v.x, ux1, ux2);
		y1 = (float) uz_to_usersy((double) v1->v.z, uy1, uy2);
		x2 = (float) ux_to_usersx((double) v2->v.x, ux1, ux2);
		y2 = (float) uz_to_usersy((double) v2->v.z, uy1, uy2);
		sng_draw_dotted_line(x1, y1, x2, y2);
	}
}

static void draw_selection_marker(int x1, int y1, int x2, int y2, int offset, uint32_t oid)
{
	const int w = 3;
	char oidstr[12];
	offset += (4 - ((timer >> 1) % 4));

	snis_draw_line(x1 - offset, y1 - offset, x1 - offset + w, y1 - offset);
	snis_draw_line(x1 - offset, y1 - offset, x1 - offset, y1 - offset + w);
	snis_draw_line(x2 + offset, y1 - offset, x2 + offset - w, y1 - offset);
	snis_draw_line(x2 + offset, y1 - offset, x2 + offset, y1 - offset + w);
	snis_draw_line(x1 - offset, y2 + offset - w, x1 - offset, y2 + offset);
	snis_draw_line(x1 - offset, y2 + offset, x1 - offset + w, y2 + offset);
	snis_draw_line(x2 + offset, y2 + offset - w, x2 + offset, y2 + offset);
	snis_draw_line(x2 + offset - w, y2 + offset, x2 + offset, y2 + offset);
	snprintf(oidstr, sizeof(oidstr) - 1, "%u", oid);
	sng_abs_xy_draw_string(oidstr, PICO_FONT, x1 + 8, y1 - 8);
}

static void debug_draw_object(GtkWidget *w, struct snis_entity *o,
			float ux1, float uy1, float ux2, float uy2)
{
	int x, y, x1, y1, x2, y2, vx, vy, r;
	struct snis_entity *v = NULL;
	int xoffset = 7;
	int yoffset = 10;
	int tardy;
	char buffer[20];

	vx = 0; /* Some gcc complain about unitialized vars if we don't do this. */
	vy = 0;

	if (!o->alive)
		return;

	tardy = (o->nupdates > 0 && universe_timestamp() - o->updatetime[0] > 50.0 && o->type != OBJTYPE_SPARK);
	x = ux_to_usersx(o->x, ux1, ux2);
	if (x < 0 || x > SCREEN_WIDTH)
		return;
	y = uz_to_usersy(o->z, uy1, uy2);
	if (y < 0 || y > SCREEN_HEIGHT)
		return;
	x1 = x - 1;
	y2 = y + 1;
	y1 = y - 1;
	x2 = x + 1;

	switch (o->type) {
	case OBJTYPE_SHIP1:
		sng_set_foreground(UI_COLOR(demon_self));
		if ((timer & 0x02) && !demon_id_selected(o->id))
			goto done_drawing_item;
		break;
	case OBJTYPE_SHIP2:

#define FACTION_COLORS 8
		if (o->sdata.science_data_known)
			sng_set_foreground((o->sdata.faction % FACTION_COLORS) + 2);
		else
			sng_set_foreground(1); /* unknown faction */
		sng_set_foreground((o->sdata.faction % FACTION_COLORS) + 2);

		if (o->tsd.ship.ai[0].u.attack.victim_id != -1) {
			int vi = lookup_object_by_id(o->tsd.ship.ai[0].u.attack.victim_id);
			if (vi >= 0) {	
				v = &go[vi];
				vx = ux_to_usersx(v->x, ux1, ux2);
				vy = uz_to_usersy(v->z, uy1, uy2);
			}
		}
		break;
	case OBJTYPE_ASTEROID:
		sng_set_foreground(UI_COLOR(demon_asteroid));
		break;
	case OBJTYPE_DERELICT:
		sng_set_foreground(UI_COLOR(demon_derelict));
		break;
	case OBJTYPE_NEBULA:
		sng_set_foreground(UI_COLOR(demon_nebula));
		sng_draw_circle(0, x, y,
			ur_to_usersr(o->tsd.nebula.r, ux1, ux2));
		break;
	case OBJTYPE_STARBASE:
		sng_set_foreground(UI_COLOR(demon_starbase));
		sng_draw_circle(0, x, y, 5);
		break;
	case OBJTYPE_BLACK_HOLE:
		sng_set_foreground(UI_COLOR(demon_black_hole));
		r = ur_to_usersr(o->tsd.black_hole.radius, ux1, ux2);
		sng_draw_circle(0, x, y, r > 5 ? r : 5);
		break;
	case OBJTYPE_PLANET:
		sng_set_foreground(UI_COLOR(demon_planet));
		r = ur_to_usersr(o->tsd.planet.radius, ux1, ux2);
		sng_draw_circle(0, x, y, r > 5 ? r : 5);
		break;
	case OBJTYPE_WORMHOLE:
		sng_set_foreground(UI_COLOR(demon_wormhole));
		sng_draw_circle(0, x, y, 5);
		break;
	case OBJTYPE_SPACEMONSTER:
		sng_set_foreground(UI_COLOR(demon_spacemonster));
		snis_draw_arrow(w, gc, x, y, SCIENCE_SCOPE_R, o->heading, 0.4);
		sng_abs_xy_draw_string("M", PICO_FONT,
			x - cos(o->heading) * txx(8), y + sin(o->heading) * txy(8));
		break;
	default:
		sng_set_foreground(UI_COLOR(demon_default));
	}
	if (tardy)
		sng_set_foreground(timer & 0x04 ? WHITE : BLACK);

	snis_draw_line(x1, y1, x2, y2);
	snis_draw_line(x1, y2, x2, y1);
	if (demon_id_selected(o->id)) {
		draw_selection_marker(x1, y1, x2, y2, 6, o->id);
		if (o->type == OBJTYPE_SHIP1 || o->type == OBJTYPE_SHIP2) {
			snis_draw_arrow(w, gc, x, y, SCIENCE_SCOPE_R, o->heading, 0.4);
		}
		if (o->type == OBJTYPE_SHIP2 && (timer & 0x04))
			debug_draw_ship_patrol_route(o->tsd.ship.ai[1].u.patrol.npoints,
						o->tsd.ship.ai[1].u.patrol.p,
						ux1, uy1, ux2, uy2);
	} else {
		if (o->type == OBJTYPE_SHIP1 || o->type == OBJTYPE_SHIP2) {
			snis_draw_arrow(w, gc, x, y, SCIENCE_SCOPE_R, o->heading, 0.4);
		}
	}

	if (o->type == OBJTYPE_SHIP1 || o->type == OBJTYPE_SHIP2) {
		float threat_level, toughness;
		sng_abs_xy_draw_string(o->sdata.name, NANO_FONT,
					x + xoffset, y + yoffset);
		sng_abs_xy_draw_string(o->ai, NANO_FONT,
					x + xoffset, y + yoffset + 10);
		if (o->tsd.ship.threat_level != 0.0) {
			toughness = (255.0 - o->tsd.ship.damage.shield_damage) / 255.0;
			threat_level = o->tsd.ship.threat_level / (toughness + 0.001);
			snprintf(buffer, sizeof(buffer), "TL: %.1f", threat_level);
			sng_abs_xy_draw_string(buffer, NANO_FONT, x + xoffset, y + yoffset + 20);
		}
		if (o->type == OBJTYPE_SHIP1 && !(timer & 0x02)) {
			sng_set_foreground(MAGENTA);
			sng_draw_circle(0, x, y, txx(20));
		}
	}
	if (v) {
		sng_set_foreground(UI_COLOR(demon_victim_vector));
		sng_draw_dotted_line(x, y, vx, vy);
	}

	if ((o->type == OBJTYPE_SHIP2 || o->type == OBJTYPE_STARBASE) &&
			go_index(o) == demon_ui.captain_of) {
		sng_set_foreground(UI_COLOR(demon_starbase));
		sng_draw_circle(0, x, y, 10 + (timer % 10));
	}
	
done_drawing_item:

	sng_set_foreground(UI_COLOR(demon_default));
	sng_abs_xy_draw_string(demon_ui.error_msg, NANO_FONT, 20, 570);
	return;
}

static struct demon_cmd_def {
	char *verb;
	char *help;
} demon_cmd[] = {
	/* TODO: The client knows these commands. For many of them, it may be
	 * possible to replace them with a send_lua_script_packet_to_server(cmd)
	 * and remove all knowledge on the client side and eliminate special command
	 * specific opcodes, which would be a good thing.
	 */
	{ "MARK", "MARK LOCATION WITH A NAME" },
	{ "NAME", "NAME CURRENTLY SELECTED GROUP OF OBJECTS" },
	{ "ATTACK", "ATTACK G1 G2 - COMMAND GROUP G1 to ATTACK GROUP G2" },
	{ "GOTO", "COMMAND SELECTED SHIP TO GOTO NAMED LOCATION" },
	{ "PATROL", "NOT IMPLEMENTED" },
	{ "HALT", "NOT IMPLEMENTED" },
	{ "IDENTIFY", "SELECT NAMED GROUP" },
	{ "SAY", "CAUSE CURRENTLY CAPTAINED SHIP TO TRANSMIT WHAT YOU LIKE" },
	{ "CLEAR-ALL", "DELETE ALL OBJECTS EXCEPT HUMAN CONTROLLED SHIPS" },
	{ "LUA", "RUN SPECIFIED SERVER-SIDE LUA SCRIPT" },
	{ "AIDEBUG", "TOGGLES AI DEBUGGING INFO" },
	{ "SAFEMODE", "TOGGLES SAFE MODE (prevents enemies from attacking)" },
	{ "HELP", "PRINT THIS HELP INFORMATION" },
	{ "ENSCRIPT", "SAVE (PARTIALLY) UNIVERSE STATE TO LUA SCRIPT" },
	{ "TTS", "TEXT-TO-SPEECH TO SPECIFIED SHIP. E.G.: TTS SHIPNAME: HELLO" },
	{ "RTSMODE-ON", "ENABLE REAL TIME STRATEGY MODE" },
	{ "RTSMODE-OFF", "DISABLE REAL TIME STRATEGY MODE" },
	{ "CONSOLE", "TOGGLE DEMON CONSOLE ON/OFF" },
	{ "SET", "SET TWEAKABLE VARIABLES" },
	{ "VARS", "LIST TWEAKABLE VARIABLES" },
	{ "DESCRIBE", "DESCRIBE A TWEAKABLE VARIABLE" },
	{ "CDUMP", "DUMP CLIENT-SIDE OBJECT" },
	{ "FOLLOW", "FOLLOW AN OBJECT" },
	{ "RELOAD_SHADERS", "RELOAD ALL GLSL SHADERS" },
	/* Note: Server builtin command help isn't here, it's in the server code,
	 * elicited by a call to "send_lua_script_packet_to_server("HELP")"
	 */
};
static int demon_help_mode = 0;
#define DEMON_CMD_DELIM " ,"

static void show_cmd_help(GtkWidget *w, struct demon_cmd_def cmd[], int nitems)
{
	int i;
	char buffer[100];

	sng_set_foreground(UI_COLOR(help_text));
	for (i = 0; i < nitems; i++) {
		snprintf(buffer, sizeof(buffer), "%s", cmd[i].verb);
		sng_abs_xy_draw_string(buffer, PICO_FONT, txx(85), txy(i * 15 + 60));
		snprintf(buffer, sizeof(buffer), "%s", cmd[i].help);
		sng_abs_xy_draw_string(buffer, PICO_FONT, txx(170), txy(i * 15 + 60));
	}
}

static void add_demon_cmd_help_to_console(struct demon_cmd_def cmd[], int nitems)
{
	int i;
	char buffer[100];

	print_demon_console_color_msg(WHITE, "CLIENT COMMANDS");
	for (i = 0; i < nitems; i++) {
		snprintf(buffer, sizeof(buffer), "- %s -- %s", cmd[i].verb, cmd[i].help);
		print_demon_console_msg(buffer);
	}
}

static void demon_cmd_help(GtkWidget *w)
{
	if (!demon_help_mode)
		return;
	show_cmd_help(w, demon_cmd, ARRAYSIZE(demon_cmd));
}

static struct demon_group {
	char name[100];
	uint16_t id[MAX_DEMON_SELECTABLE];
	uint8_t nids;
} demon_group[26];
static int ndemon_groups = 0;

static struct demon_location {
	char name[100];
	double x, z;
} demon_location[26];
static int ndemon_locations = 0;

static int get_demon_location_var(char *name)
{
	int i;
	struct demon_location *dl;

	for (i = 0; i < ndemon_locations; i++) {
		if (strcmp(demon_location[i].name, name) == 0)
			return i;
	}
	if (ndemon_locations >= 26)
		return -1;
	dl = &demon_location[ndemon_locations];
	strcpy(dl->name, name);
	ndemon_locations++;
	return ndemon_locations - 1;
}

static int lookup_demon_group(char *name)
{
	int i;

	for (i = 0; i < ndemon_groups; i++) {
		if (strcmp(demon_group[i].name, name) == 0)
			return i;
	}
	return -1;
}

static int lookup_demon_location(char *name)
{
	int i;

	for (i = 0; i < ndemon_locations; i++) {
		if (strcmp(demon_location[i].name, name) == 0)
			return i;
	}
	return -1;
}

static int get_demon_group_var(char *name)
{
	int i;
	struct demon_group *dg;

	for (i = 0; i < ndemon_groups; i++) {
		if (strcmp(demon_group[i].name, name) == 0)
			return i;
	}
	if (ndemon_groups >= 26)
		return -1;
	dg = &demon_group[ndemon_groups];
	strcpy(dg->name, name);
	ndemon_groups++;
	return ndemon_groups - 1;
}

static void set_demon_group(int n)
{
	int i, count;
	struct demon_group *dg;

	count = demon_ui.nselected;
	if (count > MAX_DEMON_SELECTABLE)
		count = MAX_DEMON_SELECTABLE;

	dg = &demon_group[n];
	for (i = 0; i < count; i++)
		dg->id[i] = demon_ui.selected_id[i];
	dg->nids = count;
}

static void print_demon_console_msg_helper(char *buffer, int color)
{
	if (color >= 0)
		text_window_add_color_text(demon_ui.console, buffer, color);
	else
		text_window_add_text(demon_ui.console, buffer);
	if (demon_ui.log_console) {
		int n = strlen(buffer);
		if (n >= 1 && buffer[n - 1] == '\n')
			buffer[n - 1] = '\0'; /* prevent doubling newlines */
		fprintf(stderr, "%s\n", buffer);
	}
}

void print_demon_console_msg(const char *fmt, ...)
{
	va_list arg_ptr;
	char buffer[256];

	va_start(arg_ptr, fmt);
	vsnprintf(buffer, sizeof(buffer) - 1, fmt, arg_ptr);
	va_end(arg_ptr);
	print_demon_console_msg_helper(buffer, -1);
}

static void print_demon_console_color_msg(int color, const char *fmt, ...)
{
	va_list arg_ptr;
	char buffer[256];

	va_start(arg_ptr, fmt);
	vsnprintf(buffer, sizeof(buffer) - 1, fmt, arg_ptr);
	va_end(arg_ptr);
	print_demon_console_msg_helper(buffer, color);
}

static struct tweakable_var_descriptor client_tweak[] = {
	{ "TTS_VOLUME", "TEXT TO SPEECH VOLUME", &text_to_speech_volume, 'f',
		0.0, 1.0, 0.33, 0, 0, 0 },
	{ "SUPPRESS_ROCKET_NOISE", "1 MEANS SUPPRESS, 0 MEANS DO NOT SUPPRESS", &suppress_rocket_noise, 'i',
		0.0, 1.0, 0.0, 0, 1, 0 },
	{ "ROCKET_NOISE_VOLUME", "VOLUME OF ROCKET NOISE, 0 to 1", &rocket_noise_volume, 'f',
		0.0, 1.0, 1.0, 0, 1, 1 },
	{ "BLUE_RECTANGLE", "0 OR 1 TO TURN OFF OR ON THE BLUE RECTANGLE, RESPECTIVELY",
		&blue_rectangle, 'i', 0.0, 0.0, 0.0, 0, 1, 1 },
	{ "STATION_LABEL", "0 OR 1 TO TURN OFF OR ON THE STATION LABELS, RESPECTIVELY",
		&station_label, 'i', 0.0, 0.0, 0.0, 0, 1, 1 },
	{ "EXTERNAL_CAMERA", "0 OR 1 TO TURN OFF OR ON THE EXTERNAL CAMERA, RESPECTIVELY",
		&external_camera_active, 'i', 0.0, 0.0, 0.0, 0, 1, 0 },
	{ "NAV_HAS_COMPUTER", "0 OR 1 TO ALLOW NAV TO HAVE COMPUTER ACCESS",
		&nav_has_computer_button, 'i', 0.0, 0.0, 0.0, 0, 1, 0 },
	{ "PLANETS_SHADE_OTHER_OBJECTS", "0 OR 1 TO ENABLE/DISABLE PLANET SHADOWS ON OTHER OBJECTS",
		&planets_shade_other_objects, 'i', 0.0, 0.0, 0.0, 0, 1, 1 },
	{ "PLANET_SPECULARITY", "0 OR 1 TO ENABLE/DISABLE PLANET_SPECULARITY",
		&graph_dev_planet_specularity, 'i', 0.0, 0.0, 0.0, 0, 1, 1 },
	{ "MAIN_NAV_HYBRID", "0 OR 1 TO ENABLE/DISABLE MAINSCREEN/NAV HYBRID",
		&main_nav_hybrid, 'i', 0.0, 0.0, 0.0, 0, 1, 0 },
	{ "EXPLOSION_MULTIPLIER", "0.2 TO 5 TO MULIPLY SPARK COUNT OF NEARBY EXPLOSIONS",
		&explosion_multiplier, 'f', 0.2, 5.0, 5.0, 0, 0, 0 },
	{ "MAIN_VIEW_AZIMUTH_ANGLE", "-180 TO 180 DEGREES",
		&main_view_azimuth_angle, 'f', -180.0, 180.0, 0.0, 0, 0, 0 },
	{ "WEAPONS_AZIMUTH_ANGLE", "-180 TO 180 DEGREES",
		&weapons_azimuth_angle, 'f', -180.0, 180.0, 0.0, 0, 0, 0 },
	{ "USE_60_FPS", "0 OR 1 to USE 30 OR 60 FPS, RESPECTIVELY",
		&use_60_fps, 'i', 0.0, 0.0, 0.0, 0, 1, 1 },
	{ "LENS_FLARE", "0 OR 1 TO DISABLE OR ENABLE LENS FLARE EFFECT",
		&lens_flare_enabled, 'i', 0.0, 0.0, 0.0, 0, 1, 1 },
	{ "LENS_FLARE_INTENSITY", "0.0 TO 1.0 TO SET LENS FLARE INTENSITY",
		&lens_flare_intensity, 'f', 0.0, 1.0, 0.15, 0, 0, 0 },
	{ "ATMOSPHERE_RING_SHADOWS", "0 OR 1 TO DISABLE OR ENABLE ATMOSPHERIC RING SHADOWS",
		&graph_dev_atmosphere_ring_shadows, 'i', 0.0, 0.0, 0.0, 0, 1, 1 },
	{ "XJOYSTICK_THRESHOLD", "0 TO 64000 - SETS BOUNDARY BETWEEN FINE AND COARSE",
		&xjoystick_threshold, 'i', 0.0, 0.0, 0.0, 0, 64000, 23000 },
	{ "YJOYSTICK_THRESHOLD", "0 TO 64000 - SETS BOUNDARY BETWEEN FINE AND COARSE",
		&yjoystick_threshold, 'i', 0.0, 0.0, 0.0, 0, 64000, 23000 },
	{ "CURRENT_TYPEFACE", "0 TO 1 - SETS CURRENT TYPEFACE",
		&current_typeface, 'i', 0.0, 0.0, 0.0, 0, 2, 2 },
	{ "IMPULSE_CAMERA_SHAKE", "0.0 TO 2.0 - AMOUNT OF CAMERA SHAKE AT HIGH IMPULSE POWER",
		&impulse_camera_shake, 'f', 0.0, 2.0, 1.0, 0, 0, 0 },
	{ "ATMOSPHERE_BRIGHTNESS", "0.0 TO 1.0, DEFAULT 0.5 - BRIGHTNESS OF ATMOSPHERES",
		&atmosphere_brightness, 'f', 0.0, 1.0, 0.5, 0, 0, 0 },
	{ "SUPPRESS_HYPERSPACE_NOISE", "0 OR 1 - SUPPRESS THE NOISE ON TERMINALS DURING HYPERSPACE",
		&suppress_hyperspace_noise, 'i', 0.0, 0.0, 0.0, 0, 1, 0 },
	{ "IDIOT_LIGHT_THRESHOLD", "0 - 255 - POWER LEVEL BELOW WHICH IDIOT LIGHTS COME ON",
		&idiot_light_threshold, 'i', 0.0, 0.0, 0.0, 0, 255, 10 },
	{ "DEJITTER_SCIENCE", "0 or 1 - 1 == DEJITTER SCIENCE VALUES, 0 == DO NOT DEJITTER",
		&dejitter_science_details, 'i', 0.0, 0.0, 0.0, 0, 1, 1 },
	{ "ECHO_COMPUTER_TO_COMMS", "0 or 1, true or false",
		&echo_computer_to_comms, 'i', 0.0, 0.0, 0.0, 0, 1, 1 },
	{ "AMBIENT_LIGHT", "0.0 to 1.1 - AMBIENT LIGHT",
		&ambient_light, 'f', 0.0, 1.0, 0.015, 0, 0, 0 },
	{ "LOW_POLY_THRESHOLD", "THRESHOLD SIZE IN PIXELS AT WHICH LOW/HIGH POLY MESHES ARE CHOSEN",
		&low_poly_threshold, 'f', 0.0, 10000.0, 200.0, 0, 0, 0 },
	{ "LOG_CONSOLE", "LOG CONSOLE OUTPUT TO STDERR IF 1",
		&demon_ui.log_console, 'i', 0.0, 0.0, 0.0, 0, 1, 0 },
	{ "MONITOR_VOICE_CHAT", "1 - DISPLAY MONITOR OF VOICE CHAT 0 - DO NOT",
		&monitor_voice_chat, 'i', 0.0, 0.0, 0.0, 0, 1, 0 },
	{ "COMPRESSOR_THRESHOLD", "AUDIO DYNAMIC RANGE COMPRESSOR THRESHOLD",
		&compressor_threshold, 'f', 0.01, 0.99, 0.5, 0, 0, 0 },
	{ "COMPRESSOR_LIMIT", "AUDIO DYNAMIC RANGE COMPRESSOR LIMIT",
		&compressor_limit, 'f', 0.1, 0.99, 0.98, 0, 0, 0 },
	{ "VOIP_COMPRESSOR_THRESHOLD", "AUDIO DYNAMIC RANGE VOIP_COMPRESSOR THRESHOLD",
		&voip_compressor_threshold, 'f', 0.01, 0.99, 0.3, 0, 0, 0 },
	{ "VOIP_COMPRESSOR_LIMIT", "AUDIO DYNAMIC RANGE VOIP_COMPRESSOR LIMIT",
		&voip_compressor_limit, 'f', 0.1, 0.99, 0.7, 0, 0, 0 },
	{ "TONEMAPPING_GAIN", "COLOR TONEMAPPING GAIN",
		&tonemapping_gain, 'f', 0.0, 1.19, 1.185, 0, 0, 0 },
	{ "PLANETARY_LIGHTNING", "PLANETARY LIGHTNING ACTIVE 0 - 1",
		&planetary_lightning, 'i', 0.0, 0.0, 0.0, 0, 1, 1 },
	{ "DISPLAY_FRAME_STATS", "DISPLAY FRAME RATE EVEN IF AT ACCEPTABLE LEVELS",
		&display_frame_stats, 'i', 0.0, 0.0, 0.0, 0, 1, 0 },
	{ NULL, NULL, NULL, '\0', 0.0, 0.0, 0.0, 0, 0, 0 },
};

static int set_clientside_variable(char *cmd)
{
	int rc;
	char msg[255];

	msg[0] = '\0';
	rc = tweak_variable(client_tweak, ARRAYSIZE(client_tweak), cmd, msg, sizeof(msg));
	switch (rc) {
	case TWEAK_UNKNOWN_VARIABLE:
		break; /* Suppress message, try it on the server */
	default:
		print_demon_console_msg("%s", msg);
		break;
	}
	return rc;
}

static void client_side_dump(char *cmd)
{
	snis_debug_dump(cmd, go, nstarbase_models, docking_port_info,
			lookup_object_by_id, print_demon_console_msg,
			ship_type, nshiptypes, NULL);
}

static void client_demon_follow(char *cmd)
{
	int rc;
	uint32_t id;

	rc = sscanf(cmd, "%*s %u", &id);
	if (rc != 1) {
		if (demon_ui.follow_id != -1) {
			print_demon_console_msg("NO LONGER FOLLOWING %u", demon_ui.follow_id);
			demon_ui.follow_id = -1;
		} else {
			print_demon_console_color_msg(YELLOW, "INVALID FOLLOW COMMAND");
		}
		return;
	}
	rc = lookup_object_by_id(id);
	if (rc < 0) {
		print_demon_console_color_msg(YELLOW, "FAILED TO LOOKUP ID %u", id);
		return;
	}
	demon_ui.follow_id = id;
	print_demon_console_msg("FOLLOWING ID %u", id);
}

/* Replace "$sel" in demon commands with the list of IDs in the current selection
 * and return in a newly allocated string.
 */
static char *expand_demon_selection_string(char *input)
{
	char *sel;
	char buffer[1000];
	char number[20];
	int i, l, n;
	char *newinput;

	sel = strcasestr(input, "$sel");
	if (!sel)
		return strdup(input);
	if (demon_ui.nselected <= 0) {
		print_demon_console_color_msg(YELLOW, "NO CURRENT SELECTION");
		return strdup(input);
	}
	strcpy(buffer, "");


	for (i = 0; i < demon_ui.nselected; i++) {
		sprintf(number, "%d", demon_ui.selected_id[i]);
		l = strlen(buffer);
		n = sizeof(buffer) - l - 2;
		if (n <= 0)
			break;
		strncat(buffer, number, n);
		strncat(buffer, " ", n - 1);
	}
	buffer[999] = '\0';
	newinput = calloc(strlen(input) + strlen(buffer), 1);
	strncpy(newinput, input, sel - input);
	newinput[sel - input] = '\0';
	strncat(newinput, buffer, sizeof(buffer));
	l = strlen(newinput);
	strncat(newinput, sel + 4, strlen(input) + strlen(buffer) - l - 1);
	return newinput;
}

static int construct_demon_command(char *input,
		struct demon_cmd_packet **cmd, char *errmsg)
{
	char *s;
	int i, l, g, g2, v;
	char *saveptr;
	struct packed_buffer *pb;
	int idcount;
	char *original = NULL;
	char *copy = NULL;
	int rc;

	original = strdup(input); /* save lowercase version for text to speech */
	print_demon_console_color_msg(CYAN, "> %s", input);
	uppercase(input);
	input = expand_demon_selection_string(input);
	if (!input) {
		print_demon_console_color_msg(YELLOW, "NO CURRENT SELECTION");
		goto error;
	}
	saveptr = NULL;
	s = strtok_r(input, DEMON_CMD_DELIM, &saveptr);
	if (s == NULL) {
		strcpy(errmsg, "empty command");
		goto error;
	}

	v = -1;
	for (i = 0; i < ARRAYSIZE(demon_cmd); i++) {
		if (strncmp(demon_cmd[i].verb, s, strlen(s)))
			continue;
		v = i;
		break;
	}

	switch (v) {
		case 0: /* mark */
			s = strtok_r(NULL, DEMON_CMD_DELIM, &saveptr);
			if (s == NULL) {
				sprintf(errmsg, "missing argument to mark command");
				goto error;
			}
			l = get_demon_location_var(s); 
			if (l < 0) {
				sprintf(errmsg, "out of location variables");
				goto error;
			}
			demon_location[l].x = demon_ui.selectedx;
			demon_location[l].z = demon_ui.selectedz;
			break;
		case 1: /* select */
			s = strtok_r(NULL, DEMON_CMD_DELIM, &saveptr);
			if (s == NULL) {
				sprintf(errmsg, "missing argument to name command");
				goto error;
			}
			g = get_demon_group_var(s); 
			if (g < 0) {
				sprintf(errmsg, "out of group variables");
				goto error;
			}
			set_demon_group(g);
			break; 
		case 2: /* attack */
			s = strtok_r(NULL, DEMON_CMD_DELIM, &saveptr);
			if (s == NULL) {
				sprintf(errmsg, "missing 1st argument to attack command");
				goto error;
			}
			g = lookup_demon_group(s);
			if (g < 0) {
				sprintf(errmsg, "No such group '%s'", s);
				goto error;
			}
			s = strtok_r(NULL, DEMON_CMD_DELIM, &saveptr);
			if (s == NULL) {
				sprintf(errmsg, "missing 2nd argument to attack command");
				goto error;
			}
			g2 = lookup_demon_group(s);
			if (g2 < 0) {
				sprintf(errmsg, "no such group '%s'", s);
				goto error;
			}
			/* TODO - finish this */
			print_demon_console_msg("group %d commanded to attack group %d\n", g, g2);
			idcount = demon_group[g].nids + demon_group[g2].nids;
			pb = packed_buffer_allocate(sizeof(struct demon_cmd_packet)
							+ (idcount - 1) * sizeof(uint32_t));
			packed_buffer_append(pb, "bbwwbb", OPCODE_DEMON_COMMAND, DEMON_CMD_ATTACK, 0, 0,
						demon_group[g].nids, demon_group[g2].nids);

			printf("sending attack cmd\n");
			printf("group 1:\n");
			for (i = 0; i < demon_group[g].nids; i++) {
				packed_buffer_append(pb, "w", demon_group[g].id[i]);
				printf("%d ", demon_group[g].id[i]);
			}
			printf("\ngroup 2:\n");
			for (i = 0; i < demon_group[g2].nids; i++) {
				packed_buffer_append(pb, "w", demon_group[g2].id[i]);
				printf("%d ", demon_group[g2].id[i]);
			}
			printf("\nend of attack cmd\n");
			packed_buffer_queue_add(&to_server_queue, pb, &to_server_queue_mutex);
			wakeup_gameserver_writer();
			break; 
		case 3: /* goto */
			s = strtok_r(NULL, DEMON_CMD_DELIM, &saveptr);
			if (s == NULL) {
				sprintf(errmsg, "missing argument to goto command");
				goto error;
			}
			print_demon_console_color_msg(YELLOW, "GOTO COMMAND IS NOT IMPLEMENTED");
			break; 
		case 4: /* patrol */
			s = strtok_r(NULL, DEMON_CMD_DELIM, &saveptr);
			if (s == NULL) {
				sprintf(errmsg, "missing argument to patrol command");
				goto error;
			}
			print_demon_console_color_msg(YELLOW, "PATROL COMMAND IS NOT IMPLEMENTED");
			break; 
		case 5: /* halt */
			s = strtok_r(NULL, DEMON_CMD_DELIM, &saveptr);
			if (s == NULL) {
				sprintf(errmsg, "missing argument to halt command");
				goto error;
			}
			print_demon_console_color_msg(YELLOW, "HALT COMMAND IS NOT IMPLEMENTED");
			break; 
		case 6: /* identify */
			s = strtok_r(NULL, DEMON_CMD_DELIM, &saveptr);
			if (s == NULL) {
				sprintf(errmsg, "missing argument to identify command");
				goto error;
			}
			g = lookup_demon_group(s);
			if (g < 0) {
				l = lookup_demon_location(s);
				if (l < 0) { 
					sprintf(errmsg, "No such group or location '%s'\n", s);
					goto error;
				}
				demon_ui.selectedx = demon_location[l].x;
				demon_ui.selectedz = demon_location[l].z;
			}
			for (i = 0; i < demon_group[g].nids; i++) {
				int n;
				demon_ui.selected_id[i] = demon_group[g].id[i];
				n = lookup_object_by_id(demon_ui.selected_id[i]);
				if (n >= 0)
					print_demon_console_msg("%d %d %s ( %f, %f, %f )", i, demon_ui.selected_id[i],
							go[n].sdata.name, go[n].x, go[n].y, go[n].z);
			}
			demon_ui.nselected = demon_group[g].nids;
			break; 
		case 7: /* say */
			if (!saveptr || strlen(saveptr) == 0)
				goto error;
			send_demon_comms_packet_to_server(saveptr);
			break;
		case 8: send_demon_clear_all_packet_to_server();
			break;
		case 9:
			if (!saveptr || strlen(saveptr) == 0)
				goto error;
			send_lua_script_packet_to_server(saveptr);
			break;
		case 10: toggle_demon_ai_debug_mode();
			break;
		case 11: toggle_demon_safe_mode();
			break;
		case 12: demon_help_mode = 1; 
			add_demon_cmd_help_to_console(demon_cmd, ARRAYSIZE(demon_cmd));
			send_lua_script_packet_to_server("HELP");
			break;
		case 13:
			if (!saveptr || strlen(saveptr) == 0)
				goto error;
			send_enscript_packet_to_server(saveptr);
			break;
		case 14:
			if (!saveptr || strlen(saveptr) == 0)
				goto error;
			send_text_to_speech_packet_to_server(original + (saveptr - input));
			break;
		case 15: /* enable real-time-strategy mode */
			send_rtsmode_change_to_server(1);
			break;
		case 16: /* disable real-time-strategy mode */
			send_rtsmode_change_to_server(0);
			break;
		case 17: /* activate demon console */
			demon_ui.console_active = !demon_ui.console_active;
			break;
		case 18: /* Set tweakable variable possibly client side, possibly server side. */
			uppercase(original);
			switch (set_clientside_variable(original)) {
			case TWEAK_UNKNOWN_VARIABLE:
				send_lua_script_packet_to_server(original); /* Try it on server */
				break;
			default:
				break;
			}
			break;
		case 19: /* "VARS", list tweakable vars */
			uppercase(original);
			tweakable_vars_list(client_tweak, ARRAYSIZE(client_tweak), print_demon_console_msg);
			send_lua_script_packet_to_server(original);
			break;
		case 20: /* "DESCRIBE", describe a tweakable variable */
			uppercase(original);
			rc = tweakable_var_describe(client_tweak, ARRAYSIZE(client_tweak), original,
							print_demon_console_msg, 1);
			if (rc == TWEAK_UNKNOWN_VARIABLE)
				send_lua_script_packet_to_server(original);
			break;
		case 21: /* client side dump object */
			uppercase(original);
			copy = expand_demon_selection_string(original);
			client_side_dump(copy);
			free(copy);
			break;
		case 22: /* FOLLOW object */
			uppercase(original);
			copy = expand_demon_selection_string(original);
			client_demon_follow(copy);
			free(copy);
			break;
		case 23: /* RELOAD_SHADERS */
			reload_shaders = 1;
			break;
		default: /* unknown, maybe it's a builtin server command or a lua script */
			uppercase(original);
			copy = expand_demon_selection_string(original);
			send_lua_script_packet_to_server(copy);
			free(copy);
			break;
	}
	if (original)
		free(original);
	free(input);
	return 0;

error:
	
	if (original)
		free(original);
	free(input);
	return -1;
}

static void clear_empty_demon_variables(void)
{
	int i;

	for (i = 0; i < ndemon_groups;) {
		if (demon_group[i].nids == 0) {
			if (ndemon_groups - 1 > i)
				demon_group[ndemon_groups - 1] =
					demon_group[i];
			ndemon_groups--;
		} else {
			i++;
		}
	}
} 

static void send_demon_text_command(char *command)
{
	struct demon_cmd_packet *demon_cmd;
	int rc;

	demon_help_mode = 0;
	if (strlen(command) == 0)
		return;
	clear_empty_demon_variables();
	strcpy(demon_ui.error_msg, "");
	rc = construct_demon_command(command, &demon_cmd, demon_ui.error_msg);
	if (rc)
		print_demon_console_msg(demon_ui.error_msg);
}

static void demon_exec_button_pressed(void *x)
{
	send_demon_text_command(demon_ui.input);
	snis_text_input_box_zero(demon_ui.demon_input);
}

static void set_demon_button_colors()
{
	const int selected = UI_COLOR(demon_selected_button);
	const int deselected = UI_COLOR(demon_deselected_button);

	snis_button_set_color(demon_ui.demon_home_button, deselected);
	snis_button_set_color(demon_ui.demon_ship_button,
		demon_ui.buttonmode == DEMON_BUTTON_SHIPMODE ? selected : deselected);
	snis_button_set_color(demon_ui.demon_starbase_button,
		demon_ui.buttonmode == DEMON_BUTTON_STARBASEMODE ? selected : deselected);
	snis_button_set_color(demon_ui.demon_planet_button,
		demon_ui.buttonmode == DEMON_BUTTON_PLANETMODE ? selected : deselected);
	snis_button_set_color(demon_ui.demon_black_hole_button,
		demon_ui.buttonmode == DEMON_BUTTON_BLACKHOLEMODE ? selected : deselected);
	snis_button_set_color(demon_ui.demon_asteroid_button,
		demon_ui.buttonmode == DEMON_BUTTON_ASTEROIDMODE ? selected : deselected);
	snis_button_set_color(demon_ui.demon_nebula_button,
		demon_ui.buttonmode == DEMON_BUTTON_NEBULAMODE ? selected : deselected);
	snis_button_set_color(demon_ui.demon_spacemonster_button,
		demon_ui.buttonmode == DEMON_BUTTON_SPACEMONSTERMODE ? selected : deselected);
	snis_button_set_color(demon_ui.demon_captain_button,
		demon_ui.buttonmode == DEMON_BUTTON_CAPTAINMODE ? selected : deselected);
}

static void demon_button_create_item_3d(int button_mode)
{
	union vec3 pos = { { 1.0, 0.0, 0.0 } };
	float distance = demon_ui.exaggerated_scale * XKNOWN_DIM * 0.005 +
			(1.0 - demon_ui.exaggerated_scale) * XKNOWN_DIM * 0.0005;

	quat_rot_vec_self(&pos, &demon_ui.camera_orientation);
	vec3_mul_self(&pos, distance);
	vec3_add_self(&pos, &demon_ui.camera_pos);
	demon_ui.buttonmode = button_mode;
	set_demon_button_colors();
	demon_button_create_item((double) pos.v.x, (double) pos.v.y, (double) pos.v.z);
}

static void demon_modebutton_pressed(int whichmode)
{
	if (demon_ui.use_3d && whichmode != DEMON_BUTTON_CAPTAINMODE) {
		demon_button_create_item_3d(whichmode);
	} else {
		if (demon_ui.buttonmode == whichmode)
			demon_ui.buttonmode = DEMON_BUTTON_NOMODE;
		else
			demon_ui.buttonmode = whichmode;
		set_demon_button_colors();
	}
}

static void demon_home_button_pressed(void *x)
{
	if (demon_ui.use_3d) {
		home_demon_camera();
	} else {
		demon_ui.ux1 = 0;
		demon_ui.uy1 = 0;
		demon_ui.ux2 = XKNOWN_DIM;
		demon_ui.uy2 = ZKNOWN_DIM;
	}
}

static void demon_ship_button_pressed(void *x)
{
	uint8_t data = (uint8_t) (((intptr_t) x) & 0x0ff);
	if (data == 255)
		data = demon_ui.shiptype;
	demon_ui.shiptype = data % (nshiptypes + 1);
	demon_modebutton_pressed(DEMON_BUTTON_SHIPMODE);
}

static void demon_faction_button_pressed(void *x)
{
	int faction = (int) ((intptr_t) x);
	demon_ui.faction = faction % (nfactions() + 1);
}

static void demon_starbase_button_pressed(void *x)
{
	demon_modebutton_pressed(DEMON_BUTTON_STARBASEMODE);
}

static void demon_planet_button_pressed(void *x)
{
	demon_modebutton_pressed(DEMON_BUTTON_PLANETMODE);
}

static void demon_black_hole_button_pressed(void *x)
{
	demon_modebutton_pressed(DEMON_BUTTON_BLACKHOLEMODE);
}

static void demon_asteroid_button_pressed(void *x)
{
	demon_modebutton_pressed(DEMON_BUTTON_ASTEROIDMODE);
}

static void demon_nebula_button_pressed(void *x)
{
	demon_modebutton_pressed(DEMON_BUTTON_NEBULAMODE);
}

static void demon_spacemonster_button_pressed(void *x)
{
	demon_modebutton_pressed(DEMON_BUTTON_SPACEMONSTERMODE);
}

static void demon_captain_button_pressed(void *x)
{
	demon_modebutton_pressed(DEMON_BUTTON_CAPTAINMODE);
}

static void demon_mission_button_pressed(void *x)
{
	char *scriptname = (char *) x;

	send_lua_script_packet_to_server(scriptname);
}

static void demon_utility_button_pressed(void *x)
{
	char *scriptname = (char *) x;
	char command[256];

	if (strncmp(x, "RESETBRIDGE", 11) == 0) {
		struct snis_entity *o;

		o = find_my_ship();
		if (!o) {
			return;
		}
		snprintf(command, sizeof(command), "RESETBRIDGE %u", o->id);
		scriptname = command;
	}
	send_lua_script_packet_to_server(scriptname);
}

static void demon_delete_button_pressed(void *x)
{
	int i;

	pthread_mutex_lock(&universe_mutex);
	for (i = 0; i < demon_ui.nselected; i++) {
		queue_to_server(snis_opcode_pkt("bw",
				OPCODE_DELETE_OBJECT, demon_ui.selected_id[i]));
	}
	pthread_mutex_unlock(&universe_mutex);
}

static void demon_select_none_button_pressed(void *x)
{
	demon_select_none();
}

static void demon_torpedo_button_pressed(void *x)
{
	if (demon_ui.captain_of < 0)
		return;
	if (go[demon_ui.captain_of].type != OBJTYPE_SHIP2)
		return;
	queue_to_server(snis_opcode_pkt("bw", OPCODE_DEMON_FIRE_TORPEDO,
				go[demon_ui.captain_of].id));
}

static void demon_phaser_button_pressed(void *x)
{
	if (demon_ui.captain_of < 0)
		return;
	if (go[demon_ui.captain_of].type != OBJTYPE_SHIP2)
		return;
	queue_to_server(snis_opcode_pkt("bw", OPCODE_DEMON_FIRE_PHASER,
				go[demon_ui.captain_of].id));
}

static void demon_2d3d_button_pressed(void *x)
{
	demon_ui.use_3d = !demon_ui.use_3d;
	if (demon_ui.use_3d) {
		ui_unhide_widget(demon_ui.demon_move_button);
		ui_unhide_widget(demon_ui.demon_scale_button);
	} else {
		ui_hide_widget(demon_ui.demon_move_button);
		ui_hide_widget(demon_ui.demon_scale_button);
	}
}

static void demon_move_button_pressed(void *x)
{
	double avgx, avgy, avgz, dx, dy, dz;
	int i, count;

	if (!demon_ui.use_3d)
		return;

	avgx = 0.0;
	avgy = 0.0;
	avgz = 0.0;

	count = 0;

	pthread_mutex_lock(&universe_mutex);
	for (i = 0; i <= snis_object_pool_highest_object(pool); i++) {
		struct snis_entity *o = &go[i];
		if (!o->alive)
			continue;
		if (!demon_id_selected(o->id))
			continue;
		avgx += o->x;
		avgy += o->y;
		avgz += o->z;
		count++;
	}
	pthread_mutex_unlock(&universe_mutex);

	if (count == 0)
		return;
	avgx /= (double) count;
	avgy /= (double) count;
	avgz /= (double) count;
	dx = demon_ui.camera_pos.v.x - avgx;
	dy = demon_ui.camera_pos.v.y - avgy;
	dz = demon_ui.camera_pos.v.z - avgz;
	for (i = 0; i < demon_ui.nselected; i++) {
		int index = lookup_object_by_id(demon_ui.selected_id[i]);
		if (index >= 0)
			queue_to_server(snis_opcode_pkt("bwSSSQ",
					OPCODE_DEMON_MOVE_OBJECT,
					demon_ui.selected_id[i],
					dx, (int32_t) UNIVERSE_DIM,
					dy, (int32_t) UNIVERSE_DIM,
					dz, (int32_t) UNIVERSE_DIM,
					&go[index].orientation));
	}
}

static void demon_scale_button_pressed(void *x)
{
	if (demon_ui.desired_exaggerated_scale > 0.0) {
		demon_ui.desired_exaggerated_scale = 0.0;
		demon_ui.exaggerated_scale_active = 0;
	} else {
		demon_ui.desired_exaggerated_scale = 1.0;
		demon_ui.exaggerated_scale_active = 1;
	}
}

static int demon_scale_checkbox(void *x)
{
	return demon_ui.exaggerated_scale_active;
}

static void demon_rocket_noise_pressed(void *x)
{
	suppress_rocket_noise = !suppress_rocket_noise;
}

static int demon_rocket_noise_checkbox(void *x)
{
	return !suppress_rocket_noise;
}

static void demon_planet_water_specularity_pressed(void *x)
{
	graph_dev_planet_specularity = !graph_dev_planet_specularity;
}

static void demon_enscript_solarsystem_pressed(void *x)
{
	send_enscript_packet_to_server("ENSCRIPTED_SOLARSYSTEM.LUA");
	print_demon_console_msg("SAVING TO ENSCRIPTED_SOLARSYSTEM.LUA IF ENABLED ON SERVER");
}

static int demon_planet_water_specularity_checkbox(void *x)
{
	return graph_dev_planet_specularity;
}

static void demon_netstats_button_pressed(void *x)
{
	if (!demon_ui.netstats_active) {
		ui_unhide_widget(demon_ui.bytes_sent_strip_chart);
		ui_unhide_widget(demon_ui.bytes_recd_strip_chart);
		ui_unhide_widget(demon_ui.latency_strip_chart);
		demon_ui.netstats_active = 1;
	} else {
		ui_hide_widget(demon_ui.bytes_sent_strip_chart);
		ui_hide_widget(demon_ui.bytes_recd_strip_chart);
		ui_hide_widget(demon_ui.latency_strip_chart);
		demon_ui.netstats_active = 0;
	}
}

static int demon_netstats_checkbox(void *x)
{
	return demon_ui.netstats_active;
}

static void demon_render_style_pressed(void *x)
{
	demon_ui.render_style = !demon_ui.render_style;
}

static int demon_render_style_checkbox(void *x)
{
	return demon_ui.render_style;
}

static void demon_console_pressed(void *x)
{
	demon_ui.console_active = !demon_ui.console_active;
}

static int demon_console_checkbox(void *x)
{
	return demon_ui.console_active;
}

struct demon_screen_menu_text {
#define MAX_MISSION_MENU_ITEMS 50
	int count;
	char *menu_text[MAX_MISSION_MENU_ITEMS], *script[MAX_MISSION_MENU_ITEMS];
	char *tooltip[MAX_MISSION_MENU_ITEMS];
};

/* Read share/snis/luascripts/MISSSIONS/missions_menu.txt
 * Each line contains three fields separated by a comma.
 * The first field is the menu text, and the
 * The second field is the lua script to run.
 * The third optional field is the tooltip text for the menu item.
 */
static struct demon_screen_menu_text *read_menu_file(char *menu_file)
{
	char fname[PATH_MAX + 1];
	char *filename;
	char line[256];
	char menu_text[256];
	char script[256];
	char tooltip[256];
	char *l;
	FILE *f;
	int rc;

	struct demon_screen_menu_text *mm = malloc(sizeof(*mm));
	mm->count = 0;
	memset(mm, 0, sizeof(*mm));

	snprintf(fname, sizeof(fname), "%s/luascripts/%s", asset_dir, menu_file);
	filename = replacement_asset_lookup(fname, &replacement_assets);
	f = fopen(filename, "r");
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
				fprintf(stderr, "Bad line in %s: %s\n", filename, line);
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
			fprintf(stderr, "Demon screen menu %s max item count reached at '%s'\n",
				filename, line);
			break;
		}
	} while (1);
	fclose(f);
	return mm;
}

static void init_demon_ui()
{
	int i, x, y, dy, n;
	struct demon_screen_menu_text *missions, *utility;

	demon_ui.ux1 = 0;
	demon_ui.uy1 = 0;
	demon_ui.ux2 = XKNOWN_DIM;
	demon_ui.uy2 = ZKNOWN_DIM;
	demon_ui.nselected = 0;
	demon_ui.selectedx = -1.0;
	demon_ui.selectedz = -1.0;
	demon_ui.selectmode = 0;
	demon_ui.captain_of = -1;
	demon_ui.follow_id = -1;
	demon_ui.use_3d = 1;
	demon_ui.console_active = 0;
	demon_ui.log_console = 0;
	demon_ui.render_style = DEMON_UI_RENDER_STYLE_WIREFRAME;
	strcpy(demon_ui.error_msg, "");
	memset(demon_ui.selected_id, 0, sizeof(demon_ui.selected_id));
	demon_ui.demon_input = snis_text_input_box_init(txx(10), txy(550), txy(30), txx(550),
					UI_COLOR(demon_input), TINY_FONT,
					demon_ui.input, 50, &timer, NULL, NULL);
	snis_text_input_box_set_dynamic_width(demon_ui.demon_input, txx(100), txx(550));
	snis_text_input_box_set_return(demon_ui.demon_input, demon_exec_button_pressed);
	x = txx(3);
	y = txy(148);
	dy = txy(22);
	n = 0;
	demon_ui.demon_home_button = snis_button_init(x, y + dy * n++, txx(70), txy(20),
			"HOME", UI_COLOR(demon_deselected_button),
			NANO_FONT, demon_home_button_pressed, NULL);
	snis_button_set_sound(demon_ui.demon_home_button, UISND29);
	demon_ui.demon_ship_button = snis_button_init(x, y + dy * n++, txx(70), txy(20),
			"SHIP", UI_COLOR(demon_deselected_button),
			NANO_FONT, demon_ship_button_pressed, (void *) (intptr_t) 255);
	snis_button_set_sound(demon_ui.demon_ship_button, UISND28);
	demon_ui.demon_starbase_button = snis_button_init(x, y + dy * n++, txx(70), txy(20),
			"STARBASE", UI_COLOR(demon_deselected_button),
			NANO_FONT, demon_starbase_button_pressed, NULL);
	snis_button_set_sound(demon_ui.demon_starbase_button, UISND27);
	demon_ui.demon_planet_button = snis_button_init(x, y + dy * n++, txx(70), txy(20),
			"PLANET", UI_COLOR(demon_deselected_button),
			NANO_FONT, demon_planet_button_pressed, NULL);
	snis_button_set_sound(demon_ui.demon_planet_button, UISND26);
	demon_ui.demon_black_hole_button = snis_button_init(x, y + dy * n++, txx(70), txy(20),
			"BLACK HOLE", UI_COLOR(demon_deselected_button),
			NANO_FONT, demon_black_hole_button_pressed, NULL);
	snis_button_set_sound(demon_ui.demon_planet_button, UISND26);
	demon_ui.demon_asteroid_button = snis_button_init(x, y + dy * n++, txx(70), txy(20),
			"ASTEROID", UI_COLOR(demon_deselected_button),
			NANO_FONT, demon_asteroid_button_pressed, NULL);
	snis_button_set_sound(demon_ui.demon_asteroid_button, UISND25);
	demon_ui.demon_nebula_button = snis_button_init(x, y + dy * n++, txx(70), txy(20),
			"NEBULA", UI_COLOR(demon_deselected_button),
			NANO_FONT, demon_nebula_button_pressed, NULL);
	snis_button_set_sound(demon_ui.demon_nebula_button, UISND24);
	demon_ui.demon_spacemonster_button = snis_button_init(x, y + dy * n++, txx(70), txy(20),
			"MONSTER", UI_COLOR(demon_deselected_button),
			NANO_FONT, demon_spacemonster_button_pressed, NULL);
	snis_button_set_sound(demon_ui.demon_spacemonster_button, UISND23);
	demon_ui.demon_captain_button = snis_button_init(x, y + dy * n++, txx(70), txy(20),
			"CAPTAIN", UI_COLOR(demon_deselected_button),
			NANO_FONT, demon_captain_button_pressed, NULL);
	snis_button_set_sound(demon_ui.demon_captain_button, UISND22);
	demon_ui.demon_delete_button = snis_button_init(x, y + dy * n++, txx(70), txy(20),
			"DELETE", UI_COLOR(demon_deselected_button),
			NANO_FONT, demon_delete_button_pressed, NULL);
	snis_button_set_sound(demon_ui.demon_delete_button, UISND21);
	demon_ui.demon_select_none_button = snis_button_init(x, y + dy * n++, txx(70), txy(20),
			"SELECT NONE", UI_COLOR(demon_deselected_button),
			NANO_FONT, demon_select_none_button_pressed, NULL);
	snis_button_set_sound(demon_ui.demon_select_none_button, UISND20);
	demon_ui.demon_torpedo_button = snis_button_init(x, y + dy * n++, txx(70), txy(20),
			"TORPEDO", UI_COLOR(demon_deselected_button),
			NANO_FONT, demon_torpedo_button_pressed, NULL);
	snis_button_set_sound(demon_ui.demon_torpedo_button, UISND19);
	demon_ui.demon_phaser_button = snis_button_init(x, y + dy * n++, txx(70), txy(20),
			"PHASER", UI_COLOR(demon_deselected_button),
			NANO_FONT, demon_phaser_button_pressed, NULL);
	snis_button_set_sound(demon_ui.demon_phaser_button, UISND18);
	demon_ui.demon_2d3d_button = snis_button_init(x, y + dy * n++, txx(70), txy(20),
			"2D/3D", UI_COLOR(demon_deselected_button),
			NANO_FONT, demon_2d3d_button_pressed, NULL);
	snis_button_set_sound(demon_ui.demon_2d3d_button, UISND17);
	demon_ui.demon_move_button = snis_button_init(x, y + dy * n++, txx(70), txy(20),
			"MOVE", UI_COLOR(demon_deselected_button),
			NANO_FONT, demon_move_button_pressed, NULL);
	snis_button_set_sound(demon_ui.demon_move_button, UISND16);
	demon_ui.demon_scale_button = snis_button_init(x, y + dy * n++, txx(70), txy(20),
			"EXAG SCALE", UI_COLOR(demon_deselected_button),
			NANO_FONT, demon_scale_button_pressed, NULL);
	snis_button_set_checkbox_function(demon_ui.demon_scale_button,
			snis_button_generic_checkbox_function,
			&demon_ui.exaggerated_scale_active);
	snis_button_set_sound(demon_ui.demon_scale_button, UISND15);
	demon_ui.demon_console_button = snis_button_init(x, y + dy * n++, txx(70), txy(20),
			"CONSOLE", UI_COLOR(demon_deselected_button),
			NANO_FONT, demon_console_pressed, NULL);
	snis_button_set_checkbox_function(demon_ui.demon_console_button,
			snis_button_generic_checkbox_function,
			&demon_ui.console_active);
	snis_button_set_sound(demon_ui.demon_console_button, UISND14);
#define NETSTATS_SAMPLES 1000
	demon_ui.bytes_recd_strip_chart =
		snis_scaling_strip_chart_init(txx(120), txy(5), txx(550.0), txy(100.0),
				"BYTES/S RECD BY SERVER", "", UI_COLOR(science_graph_plot_strong),
				UI_COLOR(common_red_alert), 200000.0, NANO_FONT, NETSTATS_SAMPLES);
	demon_ui.bytes_sent_strip_chart =
		snis_scaling_strip_chart_init(txx(120), txy(135), txx(550.0), txy(100.0),
				"BYTES/S SENT BY SERVER", "", UI_COLOR(science_graph_plot_strong),
				UI_COLOR(common_red_alert), 200000.0, NANO_FONT, NETSTATS_SAMPLES);
	demon_ui.latency_strip_chart =
		snis_scaling_strip_chart_init(txx(120), txy(265), txx(550.0), txy(100.0),
				"LATENCY (ms)", "", UI_COLOR(science_graph_plot_strong),
				UI_COLOR(common_red_alert), 200000.0, NANO_FONT, NETSTATS_SAMPLES);
	demon_ui.menu = create_pull_down_menu(NANO_FONT, SCREEN_WIDTH);
	pull_down_menu_set_tooltip_drawing_function(demon_ui.menu, draw_tooltip);
	pull_down_menu_set_gravity(demon_ui.menu, 1);
	pull_down_menu_set_color(demon_ui.menu, UI_COLOR(demon_deselected_button));
	pull_down_menu_set_background_alpha(demon_ui.menu, 0.75);
	pull_down_menu_add_column(demon_ui.menu, "META");
	pull_down_menu_add_row(demon_ui.menu, "META", "HOME", demon_home_button_pressed, NULL);
	pull_down_menu_add_tooltip(demon_ui.menu, "META", "HOME",
						"RETURN VIEWPORT TO HOME POSITION AND ORIENTATION");
	pull_down_menu_add_row(demon_ui.menu, "META", "2D/3D", demon_2d3d_button_pressed, NULL);
	/* TODO: make this netstats work */
	pull_down_menu_add_tooltip(demon_ui.menu, "META", "2D/3D",
						"TOGGLE BETWEEN 2D AND 3D RENDERING OF SOLAR SYSTEM");
	pull_down_menu_add_row(demon_ui.menu, "META", "NET STATS", demon_netstats_button_pressed, NULL);
	pull_down_menu_set_checkbox_function(demon_ui.menu, "META", "NET STATS", demon_netstats_checkbox, NULL);
	pull_down_menu_add_tooltip(demon_ui.menu, "META", "NET STATS", "SHOW A GRAPH OF NETWORK STATISTICS");
	pull_down_menu_add_row(demon_ui.menu, "META", "EXAG SCALE", demon_scale_button_pressed, NULL);
	pull_down_menu_set_checkbox_function(demon_ui.menu, "META", "EXAG SCALE", demon_scale_checkbox, NULL);
	pull_down_menu_add_tooltip(demon_ui.menu, "META", "EXAG SCALE", "TOGGLE ON/OFF EXAGGERATED SCALING OF OBJECTS");
	pull_down_menu_add_row(demon_ui.menu, "META", "RENDER STYLE", demon_render_style_pressed, NULL);
	pull_down_menu_set_checkbox_function(demon_ui.menu, "META", "RENDER STYLE", demon_render_style_checkbox, NULL);
	pull_down_menu_add_tooltip(demon_ui.menu, "META", "RENDER STYLE", "CYCLE BETWEEN DIFFERENT RENDERING STYLES");
	pull_down_menu_add_row(demon_ui.menu, "META", "CONSOLE", demon_console_pressed, NULL);
	pull_down_menu_set_checkbox_function(demon_ui.menu, "META", "CONSOLE", demon_console_checkbox, NULL);
	pull_down_menu_add_tooltip(demon_ui.menu, "META", "CONSOLE", "TOGGLE DEBUGGING CONSOLE ON/OFF");
	pull_down_menu_add_row(demon_ui.menu, "META", "ROCKET ENGINE NOISE",
			demon_rocket_noise_pressed, NULL);
	pull_down_menu_set_checkbox_function(demon_ui.menu, "META", "ROCKET ENGINE NOISE",
			demon_rocket_noise_checkbox, NULL);
	pull_down_menu_add_tooltip(demon_ui.menu, "META", "ROCKET ENGINE NOISE", "TOGGLE ROCKET ENGINE NOISE ON/OFF");
	pull_down_menu_add_row(demon_ui.menu, "META", "PLANET WATER SPECULARITY",
			demon_planet_water_specularity_pressed, NULL);
	pull_down_menu_set_checkbox_function(demon_ui.menu, "META", "PLANET WATER SPECULARITY",
			demon_planet_water_specularity_checkbox, NULL);
	pull_down_menu_add_tooltip(demon_ui.menu, "META", "PLANET WATER SPECULARITY",
						"TOGGLE RENDERING OF WATER SPECULARITY ON PLANETS ON/OFF");
	pull_down_menu_add_row(demon_ui.menu, "META", "ENSCRIPT SOLARSYSTEM",
			demon_enscript_solarsystem_pressed, NULL);
	pull_down_menu_add_tooltip(demon_ui.menu, "META", "ENSCRIPT SOLARSYSTEM",
						"SAVE STATE OF MOST OBJECTS TO A LUA SCRIPT");
	pull_down_menu_add_column(demon_ui.menu, "ADD");
	pull_down_menu_add_row(demon_ui.menu, "ADD", "SHIP", demon_ship_button_pressed, (void *)
					(intptr_t) demon_ui.shiptype);
	pull_down_menu_add_row(demon_ui.menu, "ADD", "STARBASE", demon_starbase_button_pressed, NULL);
	pull_down_menu_add_row(demon_ui.menu, "ADD", "PLANET", demon_planet_button_pressed, NULL);
	pull_down_menu_add_row(demon_ui.menu, "ADD", "BLACK HOLE", demon_black_hole_button_pressed, NULL);
	pull_down_menu_add_row(demon_ui.menu, "ADD", "ASTEROID", demon_asteroid_button_pressed, NULL);
	pull_down_menu_add_row(demon_ui.menu, "ADD", "SPACE MONSTER", demon_spacemonster_button_pressed, NULL);
	pull_down_menu_add_row(demon_ui.menu, "ADD", "NEBULA", demon_nebula_button_pressed, NULL);
	pull_down_menu_add_column(demon_ui.menu, "SHIP");
	for (i = 0; i < nshiptypes; i++)
		pull_down_menu_add_row(demon_ui.menu, "SHIP", ship_type[i].class,
			demon_ship_button_pressed, (void *) ((intptr_t) i));
	pull_down_menu_add_row(demon_ui.menu, "SHIP", "RANDOM",
		demon_ship_button_pressed, (void *) ((intptr_t) nshiptypes));
	pull_down_menu_add_column(demon_ui.menu, "FACTION");
	for (i = 0; i < nfactions(); i++)
		pull_down_menu_add_row(demon_ui.menu, "FACTION", faction_name(i),
			demon_faction_button_pressed, (void *) ((intptr_t) i));
	pull_down_menu_add_row(demon_ui.menu, "FACTION", "RANDOM",
		demon_faction_button_pressed, (void *) (intptr_t) nfactions());
	pull_down_menu_add_column(demon_ui.menu, "SELECTION");
	pull_down_menu_add_row(demon_ui.menu, "SELECTION", "DELETE", demon_delete_button_pressed, NULL);
	pull_down_menu_add_row(demon_ui.menu, "SELECTION", "SELECT NONE", demon_select_none_button_pressed, NULL);
	pull_down_menu_add_column(demon_ui.menu, "CAPTAIN");
	pull_down_menu_add_row(demon_ui.menu, "CAPTAIN", "TOGGLE CAPTAIN ON/OFF", demon_captain_button_pressed, NULL);
	pull_down_menu_add_row(demon_ui.menu, "CAPTAIN", "FIRE TORPEDO", demon_torpedo_button_pressed, NULL);
	pull_down_menu_add_row(demon_ui.menu, "CAPTAIN", "FIRE PHASER", demon_phaser_button_pressed, NULL);

	utility = read_menu_file("UTIL/utility_menu.txt");
	pull_down_menu_add_column(demon_ui.menu, "UTILITY");
	for (i = 0; i < utility->count; i++) {
		pull_down_menu_add_row(demon_ui.menu, "UTILITY", utility->menu_text[i],
					demon_utility_button_pressed, utility->script[i]);
		if (utility->tooltip[i])
			pull_down_menu_add_tooltip(demon_ui.menu, "UTILITY",
							utility->menu_text[i], utility->tooltip[i]);
	}

	missions = read_menu_file("MISSIONS/missions_menu.txt");
	pull_down_menu_add_column(demon_ui.menu, "MISSIONS");
	for (i = 0; i < missions->count; i++) {
		pull_down_menu_add_row(demon_ui.menu, "MISSIONS", missions->menu_text[i],
					demon_mission_button_pressed, missions->script[i]);
		/* We never deallocate the missions menu, mm->script[x] is a cookie passed
		 * into pull_down_menu_add_row(), which is passed along to demon_mission_button_pressed(),
		 * and so it must stick around.
		 */
		if (missions->tooltip[i])
			pull_down_menu_add_tooltip(demon_ui.menu, "MISSIONS",
							missions->menu_text[i], missions->tooltip[i]);
	}

	demon_ui.console = text_window_init(txx(100), txy(10), SCREEN_WIDTH - txx(110), 500, 47,
						UI_COLOR(demon_default));
	text_window_blank_background(demon_ui.console, 1);
	text_window_set_background_alpha(demon_ui.console, 0.75);
	text_window_set_font(demon_ui.console, PICO_FONT);
	text_window_slow_printing_effect(demon_ui.console, 0);
	ui_add_button(demon_ui.demon_home_button, DISPLAYMODE_DEMON,
			"RETURN VIEWPORT TO HOME POSITION AND ORIENTATION");
	ui_add_button(demon_ui.demon_ship_button, DISPLAYMODE_DEMON,
			"CREATE A SHIP");
	ui_add_button(demon_ui.demon_starbase_button, DISPLAYMODE_DEMON,
			"CREATE A STARBASE");
	ui_add_button(demon_ui.demon_planet_button, DISPLAYMODE_DEMON,
			"CREATE A PLANET");
	ui_add_button(demon_ui.demon_black_hole_button, DISPLAYMODE_DEMON,
			"CREATE A BLACK HOLE");
	ui_add_button(demon_ui.demon_asteroid_button, DISPLAYMODE_DEMON,
			"CREATE AN ASTEROID");
	ui_add_button(demon_ui.demon_nebula_button, DISPLAYMODE_DEMON,
			"CREATE A NEBULA");
	ui_add_button(demon_ui.demon_spacemonster_button, DISPLAYMODE_DEMON,
			"CREATE A SPACEMONSTER");
	ui_add_button(demon_ui.demon_delete_button, DISPLAYMODE_DEMON,
			"DELETE THE SELECTED OBJECTS");
	ui_add_button(demon_ui.demon_select_none_button, DISPLAYMODE_DEMON,
			"SELECT ZERO OBJECTS");
	ui_add_button(demon_ui.demon_captain_button, DISPLAYMODE_DEMON,
			"BECOME CAPTAIN OF THE SELECTED SHIP");
	ui_add_button(demon_ui.demon_torpedo_button, DISPLAYMODE_DEMON,
			"MAKE THE CURRENTLY CAPTAINED SHIP FIRE A TORPEDO");
	ui_add_button(demon_ui.demon_phaser_button, DISPLAYMODE_DEMON,
			"MAKE THE CURRENTLY CAPTAINED SHIP FIRE A PHASER");
	ui_add_button(demon_ui.demon_2d3d_button, DISPLAYMODE_DEMON,
			"TOGGLE BETWEEN 2D AND 3D MODE");
	ui_add_button(demon_ui.demon_move_button, DISPLAYMODE_DEMON,
			"MOVE SELECTED ITEMS TO YOUR CURRENT LOCATION");
	ui_add_button(demon_ui.demon_scale_button, DISPLAYMODE_DEMON,
			"TOGGLE BETWEEN NORMAL AND EXAGGERATED SCALES");
	ui_add_button(demon_ui.demon_console_button, DISPLAYMODE_DEMON,
			"TOGGLE DEBUGGING CONSOLE ON/OFF");
	ui_add_text_window(demon_ui.console, DISPLAYMODE_DEMON);
	ui_add_pull_down_menu(demon_ui.menu, DISPLAYMODE_DEMON); /* needs to be last */
	ui_unhide_widget(demon_ui.demon_move_button);
	ui_unhide_widget(demon_ui.demon_scale_button);
	ui_add_text_input_box(demon_ui.demon_input, DISPLAYMODE_DEMON);
	ui_add_scaling_strip_chart(demon_ui.bytes_recd_strip_chart, DISPLAYMODE_DEMON);
	ui_add_scaling_strip_chart(demon_ui.bytes_sent_strip_chart, DISPLAYMODE_DEMON);
	ui_add_scaling_strip_chart(demon_ui.latency_strip_chart, DISPLAYMODE_DEMON);
	ui_hide_widget(demon_ui.bytes_recd_strip_chart);
	ui_hide_widget(demon_ui.bytes_sent_strip_chart);
	ui_hide_widget(demon_ui.latency_strip_chart);
	ui_hide_widget(demon_ui.console);
	home_demon_camera();
	demon_ui.camera_orientation = demon_ui.desired_camera_orientation;
	demon_ui.camera_pos = demon_ui.desired_camera_pos;
	demon_ui.exaggerated_scale = 1.0;
	demon_ui.desired_exaggerated_scale = 1.0;
	demon_ui.exaggerated_scale_active = 1;
	demon_ui.netstats_active = 0;
	snis_debug_dump_set_label("CLIENT");
}

static void calculate_new_2d_zoom(int direction, gdouble x, gdouble y, double zoom_amount,
	float *ux1, float *uy1, float *ux2, float *uy2)
{
	double nx1, nx2, ny1, ny2, mux, muy;
	double zoom_factor;

	if (direction == GDK_SCROLL_UP)
		zoom_factor = 1.0 - zoom_amount;
	else
		zoom_factor = 1.0 + zoom_amount;
	mux = x * (*ux2 - *ux1) / (double) real_screen_width;
	muy = y * (*uy2 - *uy1) / (double) real_screen_height;
	mux += *ux1;
	muy += *uy1;
	nx1 = mux - zoom_factor * (mux - *ux1);
	ny1 = muy - zoom_factor * (muy - *uy1);
	nx2 = nx1 + zoom_factor * (*ux2 - *ux1);
	ny2 = ny1 + zoom_factor * (*uy2 - *uy1);
	*ux1 = nx1;
	*uy1 = ny1;
	*ux2 = nx2;
	*uy2 = ny2;
}

static void calculate_new_demon_zoom(int direction, gdouble x, gdouble y)
{
	calculate_new_2d_zoom(direction, x, y, 0.05,
			&demon_ui.ux1, &demon_ui.uy1, &demon_ui.ux2, &demon_ui.uy2);
}

static void demon_3d_scroll(int direction, gdouble x, gdouble y)
{
	union vec3 delta = { { 0 } };
	float scroll_factor;

	scroll_factor = (demon_ui.exaggerated_scale * XKNOWN_DIM * 0.02) +
			(1.0 - demon_ui.exaggerated_scale) * XKNOWN_DIM * 0.001;

	delta.v.x = direction == GDK_SCROLL_UP ? scroll_factor : -scroll_factor;
	quat_rot_vec_self(&delta, &demon_ui.camera_orientation);
	vec3_add(&demon_ui.desired_camera_pos, &demon_ui.camera_pos, &delta);
}

static void show_demon_groups(GtkWidget *w)
{
	int i;
	sng_set_foreground(UI_COLOR(demon_group_text));

	for (i = 0; i < ndemon_groups; i++)
		sng_abs_xy_draw_string(demon_group[i].name,
			NANO_FONT, SCREEN_WIDTH - 50, i * 18 + 40);
}

static void show_2d_universe_grid(GtkWidget *w, float x1, float y1, float x2, float y2)
{
	int x, y;
	double ix, iy;
	char label[10];
	int xoffset = 7;
	int yoffset = 10;
	const char *letters = "ABCDEFGHIJK";

	ix = XKNOWN_DIM / 10.0;
	for (x = 0; x <= 10; x++) {
		int sx1, sy1, sy2;

		sx1 = ux_to_usersx(ix * x - 0.5 * XKNOWN_DIM, x1, x2);
		if (sx1 < 0 || sx1 > SCREEN_WIDTH)
			continue;
		sy1 = uz_to_usersy(0.0 - 0.5 * XKNOWN_DIM, y1, y2);
		if (sy1 < 0)
			sy1 = 0;
		if (sy1 > SCREEN_HEIGHT)
			continue;
		sy2 = uz_to_usersy(ZKNOWN_DIM - 0.5 * XKNOWN_DIM, y1, y2);
		if (sy2 > SCREEN_HEIGHT)
			sy2 = SCREEN_HEIGHT;
		if (sy2 < 0)
			continue;
		snis_draw_dotted_vline(w->window, gc, sx1, sy1, sy2, 5);
	}

	iy = ZKNOWN_DIM / 10.0;
	for (y = 0; y <= 10; y++) {
		int sx1, sy1, sx2;

		sy1 = uz_to_usersy(iy * y - 0.5 * XKNOWN_DIM, y1, y2);
		if (sy1 < 0 || sy1 > SCREEN_HEIGHT)
			continue;
		sx1 = ux_to_usersx(0.0 - 0.5 * XKNOWN_DIM, x1, x2);
		if (sx1 < 0)
			sx1 = 0;
		if (sx1 > SCREEN_WIDTH)
			continue;
		sx2 = ux_to_usersx(XKNOWN_DIM - 0.5 * XKNOWN_DIM, x1, x2);
		if (sx2 > SCREEN_WIDTH)
			sx2 = SCREEN_WIDTH;
		if (sx2 < 0)
			continue;
		snis_draw_dotted_hline(w->window, gc, sx1, sy1, sx2, 5);
	}

	ix = XKNOWN_DIM / 10;
	iy = ZKNOWN_DIM / 10;
	for (x = 0; x < 10; x++)
		for (y = 0; y < 10; y++) {
			int tx, ty;

			snprintf(label, sizeof(label), "%c%d", letters[y], x);
			tx = ux_to_usersx(x * ix - 0.5 * XKNOWN_DIM, x1, x2);
			ty = uz_to_usersy(y * iy - 0.5 * XKNOWN_DIM, y1, y2);
			sng_abs_xy_draw_string(label, NANO_FONT,
				tx + xoffset,ty + yoffset);
		}
}

static void draw_2d_small_cross(float x, float y, int color, int blink)
{
	if (!blink || timer & 0x02) {
		sng_set_foreground(color);
		snis_draw_line(x - 3, y, x + 3, y);
		snis_draw_line(x, y - 3, x, y + 3);
	}
}

static void show_demon_2d(GtkWidget *w)
{
	int i;
	char buffer[100];
	struct snis_entity *o;

	o = find_my_ship();
	if (!o)
		return;

	if (o->alive > 0)
		sng_set_foreground(UI_COLOR(demon_default));
	else
		sng_set_foreground(UI_COLOR(demon_default_dead));

	show_2d_universe_grid(w, demon_ui.ux1, demon_ui.uy1, demon_ui.ux2, demon_ui.uy2);

	pthread_mutex_lock(&universe_mutex);

	for (i = 0; i <= snis_object_pool_highest_object(pool); i++)
		debug_draw_object(w, &go[i], demon_ui.ux1, demon_ui.uy1,
					demon_ui.ux2, demon_ui.uy2);
	for (i = 0; i <= snis_object_pool_highest_object(sparkpool); i++)
		debug_draw_object(w, &spark[i], demon_ui.ux1, demon_ui.uy1,
					demon_ui.ux2, demon_ui.uy2);
	pthread_mutex_unlock(&universe_mutex);
	draw_2d_small_cross(ux_to_usersx(demon_ui.selectedx, demon_ui.ux1, demon_ui.ux2),
				uz_to_usersy(demon_ui.selectedz, demon_ui.uy1, demon_ui.uy2),
				UI_COLOR(demon_cross), 1);
	sng_set_foreground(UI_COLOR(demon_default));
	if (netstats.elapsed_seconds == 0)
		snprintf(buffer, sizeof(buffer), "Waiting for data");
	else 
		snprintf(buffer, sizeof(buffer),
			"TX:%llu RX:%llu T=%lu SECS. BW=%llu B/S SHIPS:%u OBJS:%u %u/%u/%u/%u/%u",
			(unsigned long long) netstats.bytes_sent,
			(unsigned long long) netstats.bytes_recd, 
			(unsigned long) netstats.elapsed_seconds,
			(unsigned long long) (netstats.bytes_recd + netstats.bytes_sent) / netstats.elapsed_seconds,
			netstats.nships, netstats.nobjects,
			netstats.faction_population[0],
			netstats.faction_population[1],
			netstats.faction_population[2],
			netstats.faction_population[3],
			netstats.faction_population[4]);
	sng_abs_xy_draw_string(buffer, NANO_FONT, 10, SCREEN_HEIGHT - 10);

	if (demon_ui.selectmode) {
		int x1, y1, x2, y2;

		x1 = ux_to_demonsx(demon_ui.ix);
		y1 = uz_to_demonsy(demon_ui.iz);
		x2 = ux_to_demonsx(demon_ui.ix2);
		y2 = uz_to_demonsy(demon_ui.iz2);
		sng_set_foreground(UI_COLOR(demon_selection_box));
		sng_draw_dotted_line(x1, y1, x2, y1);
		sng_draw_dotted_line(x1, y2, x2, y2);
		sng_draw_dotted_line(x1, y1, x1, y2);
		sng_draw_dotted_line(x2, y1, x2, y2);
	}
	show_demon_groups(w);
	demon_cmd_help(w);
}

static void demon_draw_ship_patrol_route(int npoints, union vec3 p[])
{
	int i, j;

	for (i = 0; i < npoints; i++) {
		j = (i + 1) % npoints;
		snis_draw_3d_moving_line(instrumentecx,
			p[i].v.x, p[i].v.y, p[i].v.z, p[j].v.x, p[j].v.y, p[j].v.z);
	}
}

static void show_demon_3d(GtkWidget *w)
{
	char buffer[100];
	int i, j, k;
	float angle_of_view = 60.0 * M_PI / 180.0;
	union vec3 camera_pos_delta;
	int color;
	float camera_movement_rate = 0.05;
	const int faction_color[] = { CYAN, YELLOW, ORANGERED, AMBER, };
	float entity_scale;
	struct material *faction_material[4];
	static int initialized_materials = 0;
	static struct material red_material, blue_material, white_material,
		green_material, magenta_material, amber_material,
		orangered_material, yellow_material, cyan_material;
	char oidstr[12];
	float oidstrx, oidstry;
	struct snis_entity *my_ship;

	my_ship = find_my_ship();
	if (!my_ship)
		return;

	if (!initialized_materials) {
		material_init_alpha_by_normal(&cyan_material);
		cyan_material.alpha_by_normal.tint.red = 0.0;
		cyan_material.alpha_by_normal.tint.green = 1.0;
		cyan_material.alpha_by_normal.tint.blue = 1.0;
		cyan_material.alpha_by_normal.alpha = 0.5;
		material_init_alpha_by_normal(&red_material);
		red_material.alpha_by_normal.tint.red = 1.0;
		red_material.alpha_by_normal.tint.green = 0.1;
		red_material.alpha_by_normal.tint.blue = 0.1;
		red_material.alpha_by_normal.alpha = 0.5;
		material_init_alpha_by_normal(&blue_material);
		blue_material.alpha_by_normal.tint.red = 0.0;
		blue_material.alpha_by_normal.tint.green = 0.0;
		blue_material.alpha_by_normal.tint.blue = 1.0;
		blue_material.alpha_by_normal.alpha = 0.5;
		material_init_alpha_by_normal(&white_material);
		white_material.alpha_by_normal.tint.red = 1.0;
		white_material.alpha_by_normal.tint.green = 1.0;
		white_material.alpha_by_normal.tint.blue = 1.0;
		white_material.alpha_by_normal.alpha = 0.5;
		material_init_alpha_by_normal(&green_material);
		green_material.alpha_by_normal.tint.red = 0.0;
		green_material.alpha_by_normal.tint.green = 1.0;
		green_material.alpha_by_normal.tint.blue = 0.0;
		green_material.alpha_by_normal.alpha = 0.5;
		material_init_alpha_by_normal(&magenta_material);
		magenta_material.alpha_by_normal.tint.red = 1.0;
		magenta_material.alpha_by_normal.tint.green = 0.0;
		magenta_material.alpha_by_normal.tint.blue = 1.0;
		magenta_material.alpha_by_normal.alpha = 0.5;
		material_init_alpha_by_normal(&amber_material);
		amber_material.alpha_by_normal.tint.red = 1.0;
		amber_material.alpha_by_normal.tint.green = 0.76;
		amber_material.alpha_by_normal.tint.blue = 0.0;
		amber_material.alpha_by_normal.alpha = 0.5;
		material_init_alpha_by_normal(&orangered_material);
		orangered_material.alpha_by_normal.tint.red = 1.0;
		orangered_material.alpha_by_normal.tint.green = 0.27;
		orangered_material.alpha_by_normal.tint.blue = 0.0;
		orangered_material.alpha_by_normal.alpha = 0.5;
		material_init_alpha_by_normal(&yellow_material);
		yellow_material.alpha_by_normal.tint.red = 1.0;
		yellow_material.alpha_by_normal.tint.green = 1.0;
		yellow_material.alpha_by_normal.tint.blue = 0.0;
		yellow_material.alpha_by_normal.alpha = 0.5;
		faction_material[0] = &cyan_material;
		faction_material[1] = &yellow_material;
		faction_material[2] = &orangered_material;
		faction_material[3] = &amber_material;
	}

	/* If in captain mode or follow mode, then set desired camera position/orientation accordingly */
	if (demon_ui.captain_of >= 0 || demon_ui.follow_id >= 0) {
		struct snis_entity *o;

		if (demon_ui.captain_of >= 0) {
			o = &go[demon_ui.captain_of];
		} else {
			int i = lookup_object_by_id(demon_ui.follow_id);
			if (i >= 0)
				o = &go[i];
			else
				o = NULL;
		}

		if (o) {
			union vec3 rel_cam_pos = { { -50.0, 10.0, 0.0 } };
			union vec3 ship_pos = { { o->x, o->y, o->z } };

			demon_ui.desired_camera_orientation = o->orientation;
			quat_rot_vec_self(&rel_cam_pos, &o->orientation);
			vec3_add(&demon_ui.desired_camera_pos, &ship_pos, &rel_cam_pos);
			camera_movement_rate = 0.1;
		}
	}

	if (my_ship->alive > 0)
		sng_set_foreground(UI_COLOR(demon_default));
	else
		sng_set_foreground(UI_COLOR(demon_default_dead));

	/* Move camera towards desired position */
	vec3_sub(&camera_pos_delta, &demon_ui.desired_camera_pos, &demon_ui.camera_pos);
	vec3_mul_self(&camera_pos_delta, camera_movement_rate);
	vec3_add_self(&demon_ui.camera_pos, &camera_pos_delta);

	/* Move camera towards desired orientation */
	quat_slerp(&demon_ui.camera_orientation,
			&demon_ui.camera_orientation, &demon_ui.desired_camera_orientation, camera_movement_rate);

	/* Move exaggerate scale factor towards desired value */
	if (demon_ui.desired_exaggerated_scale != demon_ui.exaggerated_scale)
		demon_ui.exaggerated_scale += (demon_ui.desired_exaggerated_scale - demon_ui.exaggerated_scale) * 0.1;

	/* Setup 3d universe grid */
	for (i = 0; i < 10; i++) {
		float x = (i * XKNOWN_DIM / 10.0) - XKNOWN_DIM / 2.0 + (XKNOWN_DIM / 20.0);
		for (j = 0; j < 10; j++) {
			float y = (j * XKNOWN_DIM / 10.0) - XKNOWN_DIM / 2.0 + (XKNOWN_DIM / 20.0);
			for (k = 0; k < 10; k++) {
				float z = (k * XKNOWN_DIM / 10.0) - XKNOWN_DIM / 2.0 + (XKNOWN_DIM / 20.0);
				struct entity *e;
				e = add_entity(instrumentecx, demon3d_axes_mesh, x, y, z, UI_COLOR(demon_axes));
				if (((i == 9 || i == 0) + (j == 9 || j == 0) + (k == 9 || k == 0)) == 3) {
					update_entity_scale(e, 3.0);
					update_entity_color(e, UI_COLOR(demon_default));
				}
			}
		}
	}

	if (demon_ui.render_style == DEMON_UI_RENDER_STYLE_WIREFRAME)
		set_renderer(instrumentecx, WIREFRAME_RENDERER);
	else
		set_renderer(instrumentecx, FLATSHADING_RENDERER);
	camera_set_pos(instrumentecx, demon_ui.camera_pos.v.x, demon_ui.camera_pos.v.y, demon_ui.camera_pos.v.z);
	camera_set_orientation(instrumentecx, &demon_ui.camera_orientation);
	camera_set_parameters(instrumentecx, 10.0, XKNOWN_DIM * 2.0, SCREEN_WIDTH, SCREEN_HEIGHT, angle_of_view);
	set_window_offset(instrumentecx, 0, 0);
	calculate_camera_transform(instrumentecx);
	entity_context_set_hi_lo_poly_pixel_threshold(instrumentecx, low_poly_threshold);

	pthread_mutex_lock(&universe_mutex);

	for (i = 0; i <= snis_object_pool_highest_object(pool); i++) {
		struct entity *e = NULL;
		struct snis_entity *o = &go[i];
		struct mesh *m = NULL;
		float scale = XKNOWN_DIM / 10000.0;
		int draw_label = 0;
		char label[100];
		float sx, sy;
		struct material *material;
		int selected;

		selected = 0;
		if (!o->alive)
			continue;
		switch (o->type) {
		case OBJTYPE_PLANET:
			color = WHITE;
			strcpy(label, o->sdata.name);
			draw_label = 1;
			material = &white_material;
			break;
		case OBJTYPE_BLACK_HOLE:
			color = UI_COLOR(demon_black_hole);
			strcpy(label, o->sdata.name);
			draw_label = 1;
			material = &blue_material;
			break;
		case OBJTYPE_SHIP1:
			if (timer & 0x4)
				color = MAGENTA;
			else
				color = GREEN;
			strcpy(label, o->sdata.name);
			material = &magenta_material;
			draw_label = 1;
			break;
		case OBJTYPE_SHIP2:
			color = faction_color[o->sdata.faction % ARRAYSIZE(faction_color)];
			strcpy(label, o->sdata.name);
			material = faction_material[o->sdata.faction % ARRAYSIZE(faction_color)];
			draw_label = 1;
			break;
		case OBJTYPE_ASTEROID:
			color = ORANGE;
			material = &amber_material;
			break;
		case OBJTYPE_STARBASE:
			scale = XKNOWN_DIM / 50000.0;
			color = RED;
			strcpy(label, o->sdata.name);
			draw_label = 1;
			material = &red_material;
			break;
		case OBJTYPE_CARGO_CONTAINER:
			color = GREEN;
			material = &green_material;
			break;
		case OBJTYPE_LASER:
			color = ORANGERED;
			material = &orangered_material;
			break;
		case OBJTYPE_MISSILE:
		case OBJTYPE_TORPEDO:
			color = ORANGERED;
			material = &orangered_material;
			break;
		case OBJTYPE_WARPGATE:
			color = YELLOW;
			scale = XKNOWN_DIM / 100000.0;
			material = &yellow_material;
			break;
		case OBJTYPE_WARP_CORE:
			color = WARP_CORE_COLOR;
			material = &blue_material;
			break;
		case OBJTYPE_SPACEMONSTER:
			color = MAGENTA;
			material = &magenta_material;
			break;
		case OBJTYPE_DERELICT:
			color = BLUE;
			material = &blue_material;
			break;
		case OBJTYPE_FLARE:
			color = YELLOW;
			material = &yellow_material;
			break;
		default:
			color = MAGENTA;
			material = &magenta_material;
			break;
		}
		if (demon_id_selected(o->id)) {
			selected = 1;
			snprintf(oidstr, sizeof(oidstr) - 1, "%u", o->id);
			if ((timer & 0x0f) < 0x03) {
				color = BLACK;
				material = &green_material;
			} else {
				color = MAGENTA;
				material = &magenta_material;
			}
		}

		switch (o->type) {
		case OBJTYPE_PLANET:
			e = add_entity(instrumentecx, low_poly_sphere_mesh,  o->x, o->y, o->z, color);
			if (e) {
				update_entity_scale(e, o->tsd.planet.radius);
				entity_set_user_data(e, o);
				update_entity_material(e, material);
				if (o->entity)
					update_entity_orientation(e, entity_get_orientation(o->entity));
			}
			if (o->tsd.planet.ring) {
				struct entity *ring =
					add_entity(instrumentecx, nav_planetary_ring_mesh, 0, 0, 0, color);
				struct entity *ring2 =
					add_entity(instrumentecx, nav_planetary_ring_mesh, 0, 0, 0, color);
				if (ring) {
					update_entity_orientation(ring, &identity_quat);
					update_entity_parent(instrumentecx, ring, e);
				}
				if (ring2) { /* Defeat backface culling by adding an upside down ring */
					union quat upside_down;
					quat_init_axis(&upside_down, 1, 0, 0, 180.0 * M_PI / 180.0);
					update_entity_orientation(ring2, &upside_down);
					update_entity_parent(instrumentecx, ring2, e);
				}
			}
			transform_point(instrumentecx, o->x, o->y, o->z, &sx, &sy);
			sng_abs_xy_draw_string(label, NANO_FONT, sx + 10, sy - 10);
			break;
		case OBJTYPE_BLACK_HOLE:
			e = add_entity(instrumentecx, low_poly_sphere_mesh,  o->x, o->y, o->z, color);
			if (e) {
				update_entity_scale(e, o->tsd.black_hole.radius * 2.0);
				entity_set_user_data(e, o);
				update_entity_material(e, &blue_material);
			}
			break;
		case OBJTYPE_FLARE:
			e = add_entity(instrumentecx, torpedo_nav_mesh, o->x, o->y, o->z, color);
			if (e) {
				update_entity_scale(e,  demon_ui.exaggerated_scale * scale +
						(1.0 - demon_ui.exaggerated_scale) * entity_get_scale(o->entity));
				update_entity_material(e, &yellow_material);
			}
			break;
		case OBJTYPE_SHIP2:
		case OBJTYPE_SHIP1:
		case OBJTYPE_ASTEROID:
		case OBJTYPE_STARBASE:
		case OBJTYPE_CARGO_CONTAINER:
		case OBJTYPE_LASER:
		case OBJTYPE_WARPGATE:
		case OBJTYPE_TORPEDO:
		case OBJTYPE_MISSILE:
		case OBJTYPE_SPACEMONSTER:
		case OBJTYPE_WARP_CORE:
		case OBJTYPE_DERELICT:
			if (!o->entity && o->type != OBJTYPE_SHIP1)
				break;
			if (o->type != OBJTYPE_SHIP1)
				m = entity_get_mesh(o->entity);
			else
				m = NULL;
			if (!m) {
				if (o->type == OBJTYPE_SHIP1)
					m = ship_mesh_map[o->tsd.ship.shiptype];
				else
					break;
			}
			if (m == torpedo_mesh)
				m = torpedo_nav_mesh;
			e = add_entity(instrumentecx, m, o->x, o->y, o->z, color);
			if (!e)
				break;
			update_entity_material(e, material);
			entity_set_user_data(e, o);
			if (o->entity) {
				entity_scale = demon_ui.exaggerated_scale * scale +
					(1.0 - demon_ui.exaggerated_scale) * entity_get_scale(o->entity);
			} else {
				entity_scale = demon_ui.exaggerated_scale * scale +
					(1.0 - demon_ui.exaggerated_scale);
			}
			update_entity_scale(e, entity_scale);
			update_entity_orientation(e, &o->orientation);
			if (draw_label) {
				union vec3 opos = { { o->x, o->y, o->z } };
				union vec3 dist;
				vec3_sub(&dist, &opos, &demon_ui.camera_pos);
				if (o->type == OBJTYPE_STARBASE || vec3_magnitude(&dist) < XKNOWN_DIM / 10.0) {
					transform_point(instrumentecx, o->x, o->y, o->z, &sx, &sy);
					sng_abs_xy_draw_string(label, NANO_FONT, sx + 10, sy - 10);
				}
			}
		default:
			break;
		}
		if (selected && e && entity_onscreen(e)) {
			entity_get_screen_coords(e, &oidstrx, &oidstry);
			oidstrx += txx(10);
			oidstry -= txy(20);
			sng_abs_xy_draw_string(oidstr, PICO_FONT, oidstrx, oidstry);

			sng_set_foreground(RED);
			if (o->type == OBJTYPE_SHIP2) {
				demon_draw_ship_patrol_route(o->tsd.ship.ai[1].u.patrol.npoints,
						o->tsd.ship.ai[1].u.patrol.p);

				if (o->tsd.ship.ai[0].u.attack.victim_id != -1) {
					int vi = lookup_object_by_id(o->tsd.ship.ai[0].u.attack.victim_id);
					if (vi >= 0) {
						sng_set_foreground(YELLOW);
						snis_draw_3d_moving_line(instrumentecx,
							o->x, o->y, o->z, go[vi].x, go[vi].y, go[vi].z);
					}
				}
			}
		}
	}

	/* Check if user is navigating to something on the screen */
	if (demon_ui.button3_released) {
		int found = 0;

		demon_ui.button3_released = 0;
		for (i = 0; i <= get_entity_count(instrumentecx); i++) {
			const double threshold = real_screen_width * 0.005;
			float sx, sy;
			union vec3 epos, dpos, backoff;
			struct entity *e;
			struct mesh *m;

			e = get_entity(instrumentecx, i);
			if (!entity_onscreen(e))
				continue;
			entity_get_screen_coords(e, &sx, &sy);
			if (fabs(sx - demon_ui.release_mousex) < threshold &&
			    fabs(sy - demon_ui.release_mousey) < threshold) {
				entity_get_pos(e, &epos.v.x, &epos.v.y, &epos.v.z);
				m = entity_get_mesh(e);
				if (m) {
					union vec3 right = { { 1.0, 0.0, 0.0, }, };
					union vec3 up = { { 0.0, 0.0, 1.0 } };
					float factor = (-demon_ui.exaggerated_scale * XKNOWN_DIM / 100.0) +
						-(1.0 - demon_ui.exaggerated_scale) * XKNOWN_DIM / 2000.0;

					vec3_sub(&dpos, &epos, &demon_ui.camera_pos);
					vec3_normalize(&backoff, &dpos);
					vec3_mul_self(&backoff, factor);
					vec3_add_self(&dpos, &backoff);
					vec3_add(&demon_ui.desired_camera_pos, &demon_ui.camera_pos, &dpos);
					vec3_normalize_self(&dpos);
					quat_rot_vec_self(&up, &demon_ui.camera_orientation);
					quat_from_u2v(&demon_ui.desired_camera_orientation, &right, &dpos, &up);
					if (demon_ui.captain_of != -1) {
						double dx, dy, dz;

						dx = demon_ui.desired_camera_pos.v.x - go[demon_ui.captain_of].x;
						dy = demon_ui.desired_camera_pos.v.y - go[demon_ui.captain_of].y;
						dz = demon_ui.desired_camera_pos.v.z - go[demon_ui.captain_of].z;
						queue_to_server(snis_opcode_pkt("bwSSSQ",
								OPCODE_DEMON_MOVE_OBJECT,
								go[demon_ui.captain_of].id,
								dx, (int32_t) UNIVERSE_DIM,
								dy, (int32_t) UNIVERSE_DIM,
								dz, (int32_t) UNIVERSE_DIM,
								&demon_ui.desired_camera_orientation));
					}
					found = 1;
					break;
				}
			}
		}
		if (!found) {
			union vec3 turn;
			union vec3 right = { { 1.0, 0.0, 0.0 } };
			union quat rotation, local_rotation;

			turn.v.z = demon_ui.release_mousex - real_screen_width / 2.0;
			turn.v.y = -demon_ui.release_mousey + real_screen_height / 2.0;
			turn.v.x = real_screen_width / 2.0;

			quat_from_u2v(&rotation, &right, &turn, NULL);
			quat_conjugate(&local_rotation, &rotation, &demon_ui.camera_orientation);
			/* Apply to local orientation */
			quat_mul(&demon_ui.desired_camera_orientation, &local_rotation, &demon_ui.camera_orientation);
			quat_normalize_self(&demon_ui.desired_camera_orientation);
		}
	}

	/* Check if the user is trying to select something */
	if (demon_ui.button2_released) {
		struct snis_entity *o = NULL;
		struct snis_entity *closest = NULL;
		float distance, closest_distance = 1000000;

		demon_ui.button2_released = 0;
		for (i = 0; i <= get_entity_count(instrumentecx); i++) {
			const double threshold = real_screen_width * 0.02;
			float sx, sy, dx, dy;
			struct entity *e;

			e = get_entity(instrumentecx, i);
			if (!entity_onscreen(e))
				continue;
			entity_get_screen_coords(e, &sx, &sy);
			dx = sx - demon_ui.release_mousex;
			dy = sy - demon_ui.release_mousey;
			if (fabsf(dx) < threshold &&
			    fabsf(dy) < threshold) {
				distance = sqrtf(dx * dx + dy * dy);
				if (!closest || distance < closest_distance) {
					o = entity_get_user_data(e);
					if (!o) /* e.g. axes have no associated object */
						continue;
					closest_distance = distance;
					closest = o;
				}
			}
		}
		if (closest) {
			if (demon_id_selected(closest->id))
				demon_deselect(closest->id);
			else
				demon_select(closest->id);
		}
	}
	pthread_mutex_unlock(&universe_mutex);
	render_entities(instrumentecx);
	pthread_mutex_lock(&universe_mutex);
	for (i = 0; i <= get_entity_count(instrumentecx); i++) {
		float sx, sy;
		struct entity *e;
		struct snis_entity *o;

		e = get_entity(instrumentecx, i);
		if (!entity_onscreen(e))
			continue;
		o = entity_get_user_data(e);
		if (!o) /* e.g. axes have no associated object */
			continue;
		if (o->type == OBJTYPE_SHIP1) {
			entity_get_screen_coords(e, &sx, &sy);
			sng_set_foreground(MAGENTA);
			sng_draw_circle(0, sx, sy, txx(20));
			/* TODO: figure out why this sdata.name seems to be empty. */
			sng_abs_xy_draw_string(o->sdata.name, NANO_FONT, sx + txx(5), sy - txy(5));
		}
	}
	pthread_mutex_unlock(&universe_mutex);


	sng_set_foreground(UI_COLOR(demon_default));
	if (netstats.elapsed_seconds == 0)
		snprintf(buffer, sizeof(buffer), "Waiting for data");
	else
		snprintf(buffer, sizeof(buffer),
			"TX:%llu RX:%llu T=%lu SECS. BW=%llu B/S SHIPS:%u OBJS:%u %u/%u/%u/%u/%u",
			(unsigned long long) netstats.bytes_sent,
			(unsigned long long) netstats.bytes_recd,
			(unsigned long) netstats.elapsed_seconds,
			(unsigned long long) (netstats.bytes_recd + netstats.bytes_sent) / netstats.elapsed_seconds,
			netstats.nships, netstats.nobjects,
			netstats.faction_population[0],
			netstats.faction_population[1],
			netstats.faction_population[2],
			netstats.faction_population[3],
			netstats.faction_population[4]);
	sng_abs_xy_draw_string(buffer, NANO_FONT, 10, SCREEN_HEIGHT - 10);

	show_demon_groups(w);
	demon_cmd_help(w);
	remove_all_entity(instrumentecx);
}

static void show_demon_console(GtkWidget *w)
{
	if (demon_ui.console_active)
		ui_unhide_widget(demon_ui.console);
	else
		ui_hide_widget(demon_ui.console);
}

static void show_demon(GtkWidget *w)
{
	char label[100];
	if (demon_ui.use_3d)
		show_demon_3d(w);
	else
		show_demon_2d(w);
	if (demon_ui.captain_of != -1) {
		sng_set_foreground(WHITE);
		sng_center_xy_draw_string("CAPTAIN MODE", SMALL_FONT, SCREEN_WIDTH / 2, txy(20));
	}
	if (demon_ui.shiptype >= 0 && demon_ui.shiptype < nshiptypes)
		snprintf(label, sizeof(label), "SHIP TYPE = %s",
				ship_type[demon_ui.shiptype].class);
	else
		snprintf(label, sizeof(label), "SHIP TYPE = RANDOM");
	sng_abs_xy_draw_string(label, PICO_FONT, SCREEN_WIDTH - txx(100), txy(20));
	if (demon_ui.faction >= 0 && demon_ui.faction < nfactions())
		snprintf(label, sizeof(label), "FACTION = %s", faction_name(demon_ui.faction % nfactions()));
	else
		snprintf(label, sizeof(label), "FACTION = RANDOM");
	sng_abs_xy_draw_string(label, PICO_FONT, SCREEN_WIDTH - txx(100), txy(40));
	show_demon_console(w);
	show_common_screen(w, "DEMON");
}

static void show_warp_hash_screen(GtkWidget *w)
{
	int i;
	int y1, y2;

	sng_set_foreground(UI_COLOR(warp_hash));

	for (i = 0; i < 100; i++) {
		// x1 = snis_randn(SCREEN_WIDTH);
		// x2 = snis_randn(SCREEN_WIDTH);
		y1 = snis_randn(SCREEN_HEIGHT + 50);
		y2 = y1 - 50; // snis_randn(SCREEN_HEIGHT);
		snis_draw_line(0, y1, SCREEN_WIDTH, y2);
	}
}

static void show_warp_limbo_screen(GtkWidget *w)
{
	if (displaymode == DISPLAYMODE_MAINSCREEN) {
		show_mainscreen(w);
	} else {
		if (displaymode == DISPLAYMODE_WEAPONS) {
			show_manual_weapons(w);
			ui_element_list_draw(uiobjs);
			ui_element_list_maybe_draw_tooltips(uiobjs, mouse.x, mouse.y);
		} else {
			show_warp_hash_screen(w);
		}
	}
}

static void lobby_hostname_entered()
{
	printf("lobby hostname entered: %s\n", net_setup_ui.lobbyname);
}

static void shipname_entered()
{
	printf("shipname entered: %s\n", net_setup_ui.shipname);
}

static void password_entered()
{
	printf("password entered\n");
}

static void sanitize_string(char *s)
{
	int i, len;
	const char *forbidden = "\\*?\'$";

	len = strlen(s);
	for (i = 0; i < len; i++)
		if (index(forbidden, s[i]))
			s[i] = 'x';
}

static void connect_to_lobby_button_pressed()
{
	int rc;

	printf("snis_client: connect to lobby pressed\n");
	/* These must be set to connect to the lobby... */
	printf("lobbyname = '%s'\n", net_setup_ui.lobbyname);
	printf("shipname = '%s'\n", net_setup_ui.shipname);
	if (strcmp(net_setup_ui.lobbyname, "") == 0 ||
		strcmp(net_setup_ui.shipname, "") == 0 ||
		strcmp(net_setup_ui.password, "") == 0)
		return;

	printf("snis_client: connecting to lobby...\n");
	displaymode = DISPLAYMODE_LOBBYSCREEN;
	lobbyhost = net_setup_ui.lobbyname;
	rc = sscanf(net_setup_ui.lobbyportstr, "%d", &lobbyport);
	if (rc != 1 || use_default_lobby_port)
		lobbyport = -1; /* let ssgl use default 2419 or $SSGL_PORT if set */
	shipname = net_setup_ui.shipname;
	password = net_setup_ui.password;
	role = 0;
	role |= (ROLE_MAIN * !!net_setup_ui.role_main_v);
	role |= (ROLE_WEAPONS * !!net_setup_ui.role_weap_v);
	role |= (ROLE_NAVIGATION * !!net_setup_ui.role_nav_v);
	role |= (ROLE_ENGINEERING * !!net_setup_ui.role_eng_v);
	role |= (ROLE_DAMCON * !!net_setup_ui.role_damcon_v);
	role |= (ROLE_SCIENCE * !!net_setup_ui.role_sci_v);
	role |= (ROLE_COMMS * !!net_setup_ui.role_comms_v);
	role |= (ROLE_SOUNDSERVER * !!net_setup_ui.role_sound_v);
	role |= (ROLE_DEMON * !!net_setup_ui.role_demon_v);
	role |= (ROLE_TEXT_TO_SPEECH * !!net_setup_ui.role_text_to_speech_v);
	if (role == 0)
		role = ROLE_ALL;

	/* If they select MAIN SCREEN role, then they need to get all the other stations too.
	 * This is so that Ctrl-O (onscreen!) will work right.  There are some things which
	 * are only transmitted from snis_server to snis_client if the client has the right role.
	 * If the main screen is to be able to display every station, it will need all the
	 * information for every role, so it must have every role.
	 */
	if (role & ROLE_MAIN)
		role |= (ROLE_WEAPONS | ROLE_NAVIGATION | ROLE_ENGINEERING |
			ROLE_DAMCON | ROLE_SCIENCE | ROLE_COMMS | ROLE_DEMON);

	login_failed_timer = 0; /* turn off any old login failed messages */
	connect_to_lobby();
}

static void browser_button_pressed(void *v)
{
	int rc;
	char *site = v;
	char cmd[1000];

	snprintf(cmd, sizeof(cmd), "google-chrome %s &", site);
	rc = system(cmd);
	if (rc == 0)
		return;
	snprintf(cmd, sizeof(cmd), "firefox %s &", site);
	rc = system(cmd);
	if (rc == 0)
		return;
}

static struct button *init_net_checkbox_button(int x, int *y, char *txt,
			button_function bf, void *cookie, int (*checkbox_function)(void *))
{
	struct button *b;
	b = snis_button_init(x, *y, txx(140), txy(18), txt, UI_COLOR(network_setup_role),
			NANO_FONT, bf, cookie);
	snis_button_set_checkbox_function(b, checkbox_function, cookie);
	*y = *y + txy(18);
	return b;
}

static void use_default_lobby_port_checkbox_pressed(void *x)
{
	int *i = x;

	*i = !*i;

	if (*i)
		ui_hide_widget(net_setup_ui.lobbyport);
	else
		ui_unhide_widget(net_setup_ui.lobbyport);
}

static void network_checkbox_pressed(void *x)
{
	int *i = x;

	*i = !*i;

	/* If they select MAIN SCREEN role, then they need to get all the other stations too.
	 * This is so that Ctrl-O (onscreen!) will work right.  There are some things which
	 * are only transmitted from snis_server to snis_client if the client has the right role.
	 * If the main screen is to be able to display every station, it will need all the
	 * information for every role, so it must have every role. This is also enforced in
	 * connect_to_lobby_button_pressed(), so if they select MAIN, then deselect some other
	 * things, it will still work.
	 */
	if (i == &net_setup_ui.role_main_v && *i) {
		net_setup_ui.role_nav_v = 1;
		net_setup_ui.role_weap_v = 1;
		net_setup_ui.role_eng_v = 1;
		net_setup_ui.role_damcon_v = 1;
		net_setup_ui.role_sci_v = 1;
		net_setup_ui.role_comms_v = 1;
		net_setup_ui.role_demon_v = 1;
	}
}

static struct button *init_net_role_button(int x, int *y, char *txt, int *value)
{
	return init_net_checkbox_button(x, y, txt, network_checkbox_pressed, value,
			snis_button_generic_checkbox_function);
}

static void init_net_role_buttons(struct network_setup_ui *nsu)
{
	int x, y;

	x = txx(620);
	y = txy(345);

	nsu->role_main_v = 0;
	nsu->role_nav_v = 1;
	nsu->role_weap_v = 1;
	nsu->role_eng_v = 1;
	nsu->role_sci_v = 1;
	nsu->role_comms_v = 1;
	nsu->role_sound_v = 1;
	nsu->role_damcon_v = 1;
	nsu->role_demon_v = 1;
	nsu->role_text_to_speech_v = 1;
	nsu->role_main = init_net_role_button(x, &y, "MAIN SCREEN ROLE", &nsu->role_main_v);
	nsu->role_nav = init_net_role_button(x, &y, "NAVIGATION ROLE", &nsu->role_nav_v);
	nsu->role_weap = init_net_role_button(x, &y, "WEAPONS ROLE", &nsu->role_weap_v);
	nsu->role_eng = init_net_role_button(x, &y, "ENGINEERING ROLE", &nsu->role_eng_v);
	nsu->role_damcon = init_net_role_button(x, &y, "DAMCON ROLE", &nsu->role_damcon_v);
	nsu->role_sci = init_net_role_button(x, &y, "SCIENCE ROLE", &nsu->role_sci_v);
	nsu->role_comms = init_net_role_button(x, &y, "COMMUNICATIONS ROLE", &nsu->role_comms_v);
	nsu->role_sound = init_net_role_button(x, &y, "SOUND SERVER ROLE", &nsu->role_sound_v);
	nsu->role_demon = init_net_role_button(x, &y, "DEMON MODE",
							&nsu->role_demon_v);
	nsu->role_text_to_speech = init_net_role_button(x, &y, "TEXT TO SPEECH",
							&nsu->role_text_to_speech_v);
	ui_add_button(nsu->role_main, DISPLAYMODE_NETWORK_SETUP,
			"WHETHER THIS TERMINAL SHOULD ACT AS MAIN SCREEN.\n"
			"IT IS BEST TO HAVE ONLY ONE MAIN SCREEN TERMINAL\n"
			"PER BRIDGE");
	ui_add_button(nsu->role_nav, DISPLAYMODE_NETWORK_SETUP,
			"WHETHER THIS TERMINAL SHOULD ACT AS NAVIGATION\n"
			"TERMINAL. THE NAVIGATION TERMINAL IS USED TO\n"
			"STEER THE SHIP.");
	ui_add_button(nsu->role_weap, DISPLAYMODE_NETWORK_SETUP,
			"WHETHER THIS TERMINAL SHOULD ACT AS WEAPONS TERMINAL\n"
			"THE WEAPONS TERMINAL IS USED TO AIM AND FIRE THE\n"
			"WEAPONS.");
	ui_add_button(nsu->role_eng, DISPLAYMODE_NETWORK_SETUP,
			"WHETHER THIS TERMINAL SHOULD ACT AS ENGINEERING\n"
			"TERMINAL. THE ENGINEERING TERMINAL IS USED TO\n"
			"CONTROL DISTRIBUTION OF POWER AND COOLANT TO\n"
			"THE VARIOUS SYSTEMS OF THE SHIP.");
	ui_add_button(nsu->role_damcon, DISPLAYMODE_NETWORK_SETUP,
			"WHETHER THIS TERMINAL SHOULD ACT AS DAMAGE CONTROL\n"
			"TERMINAL. THE DAMAGE CONTROL TERMINAL IS USED TO\n"
			"REPAIR THE VARIOUS SYSTEMS OF THE SHIP");
	ui_add_button(nsu->role_sci, DISPLAYMODE_NETWORK_SETUP,
			"WHETHER THIS TERMINAL SHOULD ACT AS SCIENCE\n"
			"TERMINAL. THE SCIENCE TERMINAL IS USED TO SCAN\n"
			"OTHER SHIPS AND ENTITIES FOUND IN SPACE.");
	ui_add_button(nsu->role_comms, DISPLAYMODE_NETWORK_SETUP,
			"WHETHER THIS TERMINAL SHOULD ACT AS COMMUNICATIONS\n"
			"TERMINAL. THE COMMUNICATIONS TERMINAL IS USED TO HAIL\n"
			"AND COMMUNICATE WITH STARBASES, MONITOR FOR SCANS BY\n"
			"OTHER SHIPS, AND CONTROL WHAT IS DISPLAYED ON THE\n"
			"MAIN SCREEN");
	ui_add_button(nsu->role_sound, DISPLAYMODE_NETWORK_SETUP,
			"WHETHER THIS TERMINAL SHOULD ACT AS SOUND SERVER\n"
			"FOR SOUNDS INTENDED TO BE HEARD BY THE WHOLE CREW.\n"
			"IT IS BEST TO HAVE ONLY ONE SOUND SERVER PER\n"
			"BRIDGE CONNECTED TO A DECENT STEREO SOUND SYSTEM");
	ui_add_button(nsu->role_demon, DISPLAYMODE_NETWORK_SETUP,
			"WHETHER THIS TERMINAL SHOULD ACT AS DEMON SCREEN.\n"
			"THE DEMON SCREEN IS LIKE A GAME MASTER/DEBUG SCREEN");
	ui_add_button(nsu->role_text_to_speech, DISPLAYMODE_NETWORK_SETUP,
			"WHETHER THIS TERMINAL SHOULD ACT AS TEXT\n"
			"TO SPEECH SERVER FOR SPEECH INTENDED TO\n"
			"BE HEARD BY THE WHOLE CREW.  IT IS BEST\n"
			"TO HAVE ONLY ONE TEXT TO SPEECH SERVER\n"
			"PER BRIDGE");
}

static void hide_faction_checkboxes(struct network_setup_ui *nsu)
{
	int i;

	for (i = 0; i < MAX_FACTIONS; i++)
		if (nsu->faction_checkbox[i])
			ui_hide_widget(nsu->faction_checkbox[i]);
}

static void unhide_faction_checkboxes(struct network_setup_ui *nsu)
{
	int i;

	for (i = 0; i < MAX_FACTIONS; i++)
		if (nsu->faction_checkbox[i])
			ui_unhide_widget(nsu->faction_checkbox[i]);
}

/* This gets called when the create ship checkbox button is pressed */
static void create_ship_checkbox_cb(void *cookie)
{
	struct network_setup_ui *nsu = cookie;
	nsu->create_ship_v = !nsu->create_ship_v;
	if (nsu->create_ship_v) { /* If create ship is set, unset join ship */
		nsu->join_ship_v = 0;
		unhide_faction_checkboxes(nsu);
	} else {
		hide_faction_checkboxes(nsu);
	}
}

/* This returns the current value of the create ship checkbox */
static int create_ship_checkbox(void *cookie)
{
	struct network_setup_ui *nsu = cookie;
	return nsu->create_ship_v;
}

/* This gets called when the join ship checkbox is pressed */
static void join_ship_checkbox_cb(void *cookie)
{
	/* If join-ship selected, make sure create-ship is turned off. */
	struct network_setup_ui *nsu = cookie;
	nsu->join_ship_v = !nsu->join_ship_v;
	if (nsu->join_ship_v) /* if join ship is is set, unset create ship */
		nsu->create_ship_v = 0;
	hide_faction_checkboxes(nsu);
}

/* This returns the current value of join ship checkbox */
static int join_ship_checkbox(void *cookie)
{
	struct network_setup_ui *nsu = cookie;
	return nsu->join_ship_v;
}

/* This gets called when one of the faction checkboxes is pressed */
static void faction_checkbox_cb(void *cookie)
{
	int i = (int) (intptr_t) cookie;
	if (i < 0 || i >= MAX_FACTIONS)
		return;
	if (net_setup_ui.faction_checkbox_v[i]) {
		net_setup_ui.faction_checkbox_v[i] = 0;
		net_setup_ui.selected_faction = -1;
	} else {
		/* memset everything to zero, then the chosen one to 1 (radio button behavior) */
		memset(net_setup_ui.faction_checkbox_v, 0, sizeof(net_setup_ui.faction_checkbox_v));
		net_setup_ui.faction_checkbox_v[i] = 1;
		net_setup_ui.selected_faction = i;
	}
}

/* This returns the current value of the faction checkboxes from the network setup screen */
static int faction_checkbox(void *cookie)
{
	int i = (int) (intptr_t) cookie;
	if (i < 0 || i >= MAX_FACTIONS)
		return 0;
	return net_setup_ui.faction_checkbox_v[i];
}

static void init_join_create_buttons(struct network_setup_ui *nsu)
{
	nsu->create_ship_v = 1;
	nsu->join_ship_v = 0;
	int x = txx(150);
	int y = txy(400);
	nsu->create_ship_checkbox = init_net_checkbox_button(x, &y,
						"CREATE SHIP", create_ship_checkbox_cb,
						nsu, create_ship_checkbox);
	nsu->join_ship_checkbox = init_net_checkbox_button(x, &y, "JOIN SHIP",
						join_ship_checkbox_cb, nsu, join_ship_checkbox);
	ui_add_button(nsu->create_ship_checkbox, DISPLAYMODE_NETWORK_SETUP,
			"SELECT THIS IF YOU WISH TO CREATE A NEW SHIP FOR THE FIRST TIME");
	ui_add_button(nsu->join_ship_checkbox, DISPLAYMODE_NETWORK_SETUP,
			"SELECT THIS IF YOU WISH TO JOIN A SHIP ALREADY IN PLAY");
}

static void init_faction_buttons(struct network_setup_ui *nsu)
{
	int i, n;
	int x = txx(450);
	int y = txy(400);

	memset(nsu->faction_checkbox, 0, sizeof(nsu->faction_checkbox));
	memset(nsu->faction_checkbox_v, 0, sizeof(nsu->faction_checkbox_v));
	nsu->selected_faction = -1;
	n = nfactions();
	if (n > MAX_FACTIONS)
		n = MAX_FACTIONS;
	for (i = 0; i < n;  i++) {
		nsu->faction_checkbox[i] = init_net_checkbox_button(x, &y, faction_name(i),
					faction_checkbox_cb, (void *) (intptr_t) i, faction_checkbox);
		ui_add_button(nsu->faction_checkbox[i], DISPLAYMODE_NETWORK_SETUP, "");
	}
	hide_faction_checkboxes(nsu);
}

static void ui_add_text_input_box(struct snis_text_input_box *t, int active_displaymode);
static void init_net_setup_ui(void)
{
	int y = txy(10) + txy(LINEHEIGHT * 3);
	int yinc = txy(50);
	int left = txx(20);
	int input_color = UI_COLOR(network_setup_input);
	int active_button_color = UI_COLOR(network_setup_active);
	int inactive_button_color = UI_COLOR(network_setup_inactive);

	char *preferred_shipname = snis_prefs_read_default_ship_name(xdg_base_ctx);

	memset(net_setup_ui.lobbyname, 0, sizeof(net_setup_ui.lobbyname));
	strcpy(net_setup_ui.lobbyname, "localhost");
	strcpy(net_setup_ui.solarsystem, "");
	y += yinc;
	net_setup_ui.lobbyservername =
		snis_text_input_box_init(left, y, txy(30), txx(750), input_color, TINY_FONT,
					net_setup_ui.lobbyname, 50, &timer,
					lobby_hostname_entered, NULL);
	net_setup_ui.default_lobby_port_checkbox = snis_button_init(left, y + yinc, txx(140), txy(18),
					"USE DEFAULT LOBBY PORT", UI_COLOR(network_setup_role), NANO_FONT,
					use_default_lobby_port_checkbox_pressed, &use_default_lobby_port);
	snis_button_set_checkbox_function(net_setup_ui.default_lobby_port_checkbox,
					snis_button_generic_checkbox_function, &use_default_lobby_port);
	net_setup_ui.lobbyport =
		snis_text_input_box_init(left, y + yinc * 2, txy(30), txx(100), input_color, TINY_FONT,
					net_setup_ui.lobbyportstr, 9, &timer, NULL, NULL);
	y += yinc * 6;
	net_setup_ui.shipname_box =
		snis_text_input_box_init(txx(150), y, txy(30), txx(250), input_color, TINY_FONT,
					net_setup_ui.shipname, sizeof(net_setup_ui.shipname) - 1, &timer,
					shipname_entered, NULL);
	y += yinc;
	net_setup_ui.password_box =
		snis_text_input_box_init(txx(150), y, txy(30), txx(250), input_color, TINY_FONT,
					net_setup_ui.password, sizeof(net_setup_ui.password), &timer,
					password_entered, NULL);
	y += yinc;
	net_setup_ui.connect_to_lobby =
		snis_button_init(left, y, -1, -1, "ENTER LOBBY xxxxxxxxxxxxxxx", inactive_button_color,
			TINY_FONT, connect_to_lobby_button_pressed, NULL);
	net_setup_ui.website_button =
		snis_button_init(left + txx(500), y, -1, -1, "WEBSITE",
			active_button_color,
			TINY_FONT, browser_button_pressed, "http://spacenerdsinspace.com");
	net_setup_ui.forum_button =
		snis_button_init(left + txx(600), y, -1, -1, "FORUM",
			active_button_color,
			TINY_FONT, browser_button_pressed, "http://spacenerdsinspace.com/forum.html");
	net_setup_ui.support_button =
		snis_button_init(left + txx(700), y, -1, -1, "DONATE",
			active_button_color,
			TINY_FONT, browser_button_pressed, "http://spacenerdsinspace.com/donate.html");
	net_setup_ui.menu = create_pull_down_menu(NANO_FONT, SCREEN_WIDTH);
	pull_down_menu_set_color(net_setup_ui.menu, active_button_color);
	init_net_role_buttons(&net_setup_ui);
	init_join_create_buttons(&net_setup_ui);
	init_faction_buttons(&net_setup_ui);
	snis_prefs_read_checkbox_defaults(xdg_base_ctx, &net_setup_ui.role_main_v, &net_setup_ui.role_nav_v,
					&net_setup_ui.role_weap_v, &net_setup_ui.role_eng_v,
					&net_setup_ui.role_damcon_v,
					&net_setup_ui.role_sci_v, &net_setup_ui.role_comms_v,
					&net_setup_ui.role_sound_v, &net_setup_ui.role_demon_v,
					&net_setup_ui.role_text_to_speech_v, &net_setup_ui.create_ship_v,
					&net_setup_ui.join_ship_v);
	if (preferred_shipname) {
		snis_text_input_box_set_contents(net_setup_ui.shipname_box, preferred_shipname);
		net_setup_ui.create_ship_v = 0;
	}
	ui_add_button(net_setup_ui.default_lobby_port_checkbox, DISPLAYMODE_NETWORK_SETUP,
			"USE THE DEFAULT LOBBY PORT OR IF UNCHECKED SPECIFY A PORT");
	ui_add_button(net_setup_ui.connect_to_lobby, DISPLAYMODE_NETWORK_SETUP,
			"CONNECT TO THE LOBBY SERVER");
	ui_add_button(net_setup_ui.website_button, DISPLAYMODE_NETWORK_SETUP,
			"SPACE NERDS IN SPACE WEBSITE");
	ui_add_button(net_setup_ui.forum_button, DISPLAYMODE_NETWORK_SETUP,
			"SPACE NERDS IN SPACE FORUM ON BRIDGESIM.NET");
	ui_add_button(net_setup_ui.support_button, DISPLAYMODE_NETWORK_SETUP,
			"HELP SUPPORT SPACE NERDS IN SPACE DEVELOPMENT");

	/* note: the order of these is important for TAB key focus advance */
	ui_add_text_input_box(net_setup_ui.lobbyservername, DISPLAYMODE_NETWORK_SETUP);
	ui_add_text_input_box(net_setup_ui.lobbyport, DISPLAYMODE_NETWORK_SETUP);
	ui_add_text_input_box(net_setup_ui.shipname_box, DISPLAYMODE_NETWORK_SETUP);
	ui_add_text_input_box(net_setup_ui.password_box, DISPLAYMODE_NETWORK_SETUP);
	ui_add_pull_down_menu(net_setup_ui.menu, DISPLAYMODE_NETWORK_SETUP); /* needs to be last */
	ui_hide_widget(net_setup_ui.lobbyport);
} 

static void show_network_setup(GtkWidget *w)
{
	char button_label[100];

	show_common_screen(w, "SPACE NERDS IN SPACE");
	show_rotating_wombat();

	sng_set_foreground(UI_COLOR(network_setup_text));
	sng_abs_xy_draw_string("NETWORK SETUP", SMALL_FONT, txx(25), txy(10 + LINEHEIGHT * 2));
	/* If manual and auto-detected lobbies are the same, hide the manual button. */
	if (strcmp(net_setup_ui.lobbyname, "") != 0)
		ui_unhide_widget(net_setup_ui.connect_to_lobby);
	else
		ui_hide_widget(net_setup_ui.connect_to_lobby);

	snprintf(button_label, sizeof(button_label), "ENTER LOBBY %s", net_setup_ui.lobbyname);
	snis_button_set_label(net_setup_ui.connect_to_lobby, button_label);
	sng_abs_xy_draw_string("LOBBY SERVER NAME OR IP ADDRESS", TINY_FONT, txx(25), txy(130));
	if (!use_default_lobby_port)
		sng_abs_xy_draw_string("LOBBY PORT", TINY_FONT, txx(25), txy(240));
	sng_abs_xy_draw_string("SHIP NAME", TINY_FONT, txx(20), txy(470));
	sng_abs_xy_draw_string("PASSWORD", TINY_FONT, txx(20), txy(520));
	sng_abs_xy_draw_string("NOTE: THE \"PASSWORD\" IS NOT CRYPTOGRAPHICALLY SECURE", NANO_FONT, txx(20), txy(540));

	sanitize_string(net_setup_ui.solarsystem);
	sanitize_string(net_setup_ui.lobbyname);

	if (strcmp(net_setup_ui.shipname, "") != 0 &&
		strcmp(net_setup_ui.password, "") != 0) {
		if (strcmp(net_setup_ui.lobbyname, "") != 0)
			snis_button_set_color(net_setup_ui.connect_to_lobby, UI_COLOR(network_setup_active));
		else
			snis_button_set_color(net_setup_ui.connect_to_lobby, UI_COLOR(network_setup_inactive));
	} else {
		snis_button_set_color(net_setup_ui.connect_to_lobby, UI_COLOR(network_setup_inactive));
	}
}

static void make_science_forget_stuff(void)
{
	int i, j;

	/* After awhile, science forgets... this is so the scientist
	 * can't just scan everything then sit back and relax for the
	 * rest of the game.
	 */
	pthread_mutex_lock(&universe_mutex);
	for (i = 0; i <= snis_object_pool_highest_object(pool); i++) {
		struct snis_entity *o = &go[i];
		if (o->sdata.science_data_known) /* forget after awhile */
			o->sdata.science_data_known--;
		if (!o->sdata.science_data_known) {
			if (o->type == OBJTYPE_SHIP1 || o->type == OBJTYPE_SHIP2) {
				for (j = 0; j < MAX_CARGO_BAYS_PER_SHIP; j++) {
					o->tsd.ship.cargo[j].contents.item = -1;
					o->tsd.ship.cargo[j].contents.qty = 0;
				}
			} else if (o->type == OBJTYPE_CARGO_CONTAINER) {
					o->tsd.cargo_container.contents.item = -1;
					o->tsd.cargo_container.contents.qty = 0;
			}
		}
	}
	pthread_mutex_unlock(&universe_mutex);
}

static int main_da_scroll(GtkWidget *w, GdkEvent *event, gpointer p)
{
	struct _GdkEventScroll *e = (struct _GdkEventScroll *) event;
	struct snis_entity *o;
	int16_t newval = 0;
	/* Science zoom scroll sensitivity table */
	const double szx[] = { 0, 10, 30, 50, 100, 150, 200, 250, 300 };
	const double szy[] = { 1, 2,  4,  8,  16,  30,  30,  30, 30};
	BUILD_ASSERT(sizeof(szx) == sizeof(szy));

	if (!(o = find_my_ship()))
		return 0;
	switch (displaymode) {
	case DISPLAYMODE_SCIENCE:
		if (e->direction == GDK_SCROLL_UP)
			newval = o->tsd.ship.scizoom -
				(int) table_interp(o->tsd.ship.scizoom, szx, szy, ARRAYSIZE(szx));
		if (e->direction == GDK_SCROLL_DOWN)
			newval = o->tsd.ship.scizoom +
				(int) table_interp(o->tsd.ship.scizoom, szx, szy, ARRAYSIZE(szx));
		if (newval < 0)
			newval = 0;
		if (newval > 255)
			newval = 255;
		transmit_adjust_control_input((uint8_t) newval, OPCODE_ADJUST_CONTROL_SCIZOOM);
		return 0;
	case DISPLAYMODE_DEMON:
		if (demon_ui.console_active) {
			if (e->direction == 0)
				text_window_scroll_up(demon_ui.console);
			else if (e->direction > 0)
				text_window_scroll_down(demon_ui.console);
			return 0;
		}
		if (demon_ui.use_3d)
			demon_3d_scroll(e->direction, e->x, e->y);
		else
			calculate_new_demon_zoom(e->direction, e->x, e->y);
		return 0;
	case DISPLAYMODE_NAVIGATION:
		if (e->direction == GDK_SCROLL_UP)
			do_zoom(10);
		if (e->direction == GDK_SCROLL_DOWN)
			do_zoom(-10);
		return 0;
	case DISPLAYMODE_WEAPONS:
		if (e->direction == GDK_SCROLL_UP)
			do_zoom(10);
		if (e->direction == GDK_SCROLL_DOWN)
			do_zoom(-10);
		return 0;
	case DISPLAYMODE_MAINSCREEN:
		if (e->direction == GDK_SCROLL_UP)
			do_zoom(10);
		if (e->direction == GDK_SCROLL_DOWN)
			do_zoom(-10);
		return 0;
	case DISPLAYMODE_COMMS:
		if (e->direction == GDK_SCROLL_UP)
			comms_dirkey(0, -1);
		if (e->direction == GDK_SCROLL_DOWN)
			comms_dirkey(0, 1);
		return 0;
	default:
		return 0;
	}
	return 0;
}

static char *help_text[] = {

	/* 0 - Main screen help text */
	"MAIN SCREEN HELP\n\n"
	"  CONTROLS\n\n"
	"    F1: HELP F2: NAVIGATION F3: WEAPONS F4: ENGINEERING F5: DAMAGE CONTROL\n"
	"    F6: SCIENCE F7: COMMS F8: MAIN SCREEN F9: DEMON SCREEN\n\n"
	"  * USE ARROW KEYS TO TURN SHIP\n"
	"  * AWSD and QE KEYS ALSO WORK\n\n"
	"  * SHIFT-W KEY TOGGLES BETWEEN WEAPONS VIEW\n"
	"    AND MAIN VIEW\n\n"
	"  * BACKQUOTE KEY CYCLES THROUGH CAMERA VIEW MODES\n"
	"  * CTRL-I INVERTS VERTICAL KEYBOARD CONTROLS\n"
	"  * 'f' TOGGLES SPACE DUST EFFECT\n"
	"  * CTRL-C TOGGLES CREDITS SCREEN\n"
	"  * CTRL-R CYCLES RENDER MODES (FOR DEBUGGING ONLY)\n"
	"\nPRESS ESC OR F1 AGAIN TO EXIT HELP\n",

	/* 1 - Navigation help text */
	"NAVIGATION HELP\n\n"
	"  CONTROLS\n\n"
	"    F1: HELP F2: NAVIGATION F3: WEAPONS F4: ENGINEERING F5: DAMAGE CONTROL\n"
	"    F6: SCIENCE F7: COMMS F8: MAIN SCREEN F9: DEMON SCREEN\n\n"
	"  *   YAW: LEFT/RIGHT ARROW KEYS, A/D KEYS, OR USE MOUSE WHEEL\n"
	"  * PITCH: UP/DOWN ARROW KEYS, W/S KEYS (CTRL-I INVERTS CONTROLS)\n"
	"  *  ROLL: Q/E KEYS\n"
	"  * AWSD KEYS ALSO WORK\n"
	"  * USE THROTTLE SLIDER TO SET THROTTLE\n"
	"  * R BUTTON ABOVE THROTTLE REVERSES THRUST\n"
	"  * USE WARP FOR FAST TRAVEL\n"
	"  * USE PLUS/MINUS KEYS TO ZOOM VIEW\n"
	"  * USE BACKQUOTE KEY TO CYCLE CAMERA POSITION\n"
	"  * CTRL-I INVERTS VERTICAL KEYBOARD CONTROLS\n"
	"  * USE STANDARD ORBIT TO ORBIT NEARBY PLANET\n"
	"  * STAR MAP INDICATES WARP GATE-REACHABLE STARS\n"
	"  * TO DOCK:  HAVE COMMS HAIL STARBASE AND\n"
	"        REQUEST PERMISSION TO DOCK.  YOU MUST BE\n"
	"        NEARBY WITH SHIELDS LOWERED. WITH PERMISSION\n"
	"        GRANTED, ENGAGE DOCKING MAGNETS AND APPROACH\n"
	"        DOCKING PORTS.  PERMISSION WINDOW IS 3 MINUTES.\n"
	"  * TO UNDOCK, DISENGAGE DOCKING MAGNETS.\n"
	"        DOCKING PERMISSION IS REVOKED UPON UNDOCKING\n"
	"        AND MUST BE REQUESTED AGAIN TO RE-DOCK.\n\n"
	"  GREEN ARROW INDICATES DIRECTION OF SCIENCE SELECTION.\n\n"
	"  CYAN ARROW INDICATES DIRECTION WEAPONS ARE AIMED.\n\n"
	"\nPRESS ESC OR F1 AGAIN TO EXIT HELP\n",

	/* 2 - Weapons help text */
	"WEAPONS HELP\n\n"
	"  CONTROLS\n\n"
	"    F1: HELP F2: NAVIGATION F3: WEAPONS F4: ENGINEERING F5: DAMAGE CONTROL\n"
	"    F6: SCIENCE F7: COMMS F8: MAIN SCREEN F9: DEMON SCREEN\n\n"
	"  * USE ARROW KEYS OR MOUSE TO AIM WEAPONS\n"
	"  * AWSD KEYS ALSO WORK\n"
	"  * FIRE PHASERS WITH SPACE BAR OR LEFT MOUSE BUTTON\n"
	"  * FIRE TORPEDOES WITH Z OR RIGHT MOUSE BUTTON\n"
	"  * FIRE MISSILES WITH N\n"
	"  * PLUS/MINUS KEYS SET PHASER WAVELENGTH\n"
	"    (OR USE MOUSE WHEEL)\n"
	"  * MATCH PHASER WAVELENGTH TO WEAKNESSES\n"
	"    IN ENEMY SHIELDS\n"
	"  * CTRL-I INVERTS VERTICAL KEYBOARD CONTROLS\n"
	"  * CTRL-M TOGGLE WEAPONS MOUSE BEHAVIOR\n"
	"  * CTRL-S TOGGLES SPACE DUST EFFECT\n"
	"\nPRESS ESC OR F1 AGAIN TO EXIT HELP\n",

	/* 3 - Engineering help text */
	"ENGINEERING HELP\n\n"
	"  CONTROLS\n\n"
	"    F1: HELP F2: NAVIGATION F3: WEAPONS F4: ENGINEERING F5: DAMAGE CONTROL\n"
	"    F6: SCIENCE F7: COMMS F8: MAIN SCREEN F9: DEMON SCREEN\n\n"
	"  * USE SLIDERS ON LEFT SIDE OF SCREEN\n"
	"    TO LIMIT POWER CONSUMPTION OF SHIP\n"
	"    SYSTEMS AND TO ALLOCATE COOLANT\n"
	"  * HEALTH AND TEMPERATURE OF SYSTEMS\n"
	"    IS INDICATED ON RIGHT SIDE OF SCREEN\n"
	"  * OVERHEATING SYSTEMS FLASH RED AND\n"
	"    CAUSE DAMAGE TO THEMSELVES\n"
	"  * 1 AND 2 BUTTONS ARE PRESETS\n"
	"  * LIFE SUPPORT SYSTEM PRODUCES OXYGEN\n"
	"  * IF OXYGEN LEVELS REACH ZERO, CREW ASPHYXIATES\n"
	"  * IF SHIELDS ARE DESTROYED, SHIP IS DESTROYED\n"
	"\nPRESS ESC OR F1 AGAIN TO EXIT HELP\n",

	/* 4 - Science help text */
	"SCIENCE HELP\n\n"
	"  CONTROLS\n\n"
	"    F1: HELP F2: NAVIGATION F3: WEAPONS F4: ENGINEERING F5: DAMAGE CONTROL\n"
	"    F6: SCIENCE F7: COMMS F8: MAIN SCREEN F9: DEMON SCREEN\n\n"
	"  * USE MOUSE WHEEL TO ZOOM/UNZOOM\n"
	"  * USE LEFT/RIGHT ARROWS TO ROTATE SCANNING BEAM (USE CTRL FOR FINE ADJUSTMENT)\n"
	"  * USE UP/DOWN ARROWS TO FOCUS/WIDEN SCANNING BEAM (USE CTRL FOR FINE ADJUSTMENT)\n"
	"  * SELECT TARGETS WITH MOUSE TO EXAMINE\n"
	"    USE DETAILS BUTTON FOR MORE INFO\n"
	"  * LRS LONG RANGE SCANNER\n"
	"       * PRESS AND HOLD RIGHT MOUSE BUTTON WITHIN VIEWSCREEN TO ROTATE VIEW\n"
	"       * OR USE COMMA, PERIOD, SLASH, L-KEY, K-KEY, SEMICOLON TO ROTATE VIEW\n"
	"       * USE CTRL KEY FOR FINE ADJUSTMENT\n"
	"  * SRS SHORT RANGE SCANNER\n"
	"  * MINING ROBOT\n"
	"       SELECT AN ASTEROID OR DERELICT SHIP THEN LAUNCH THE MINING ROBOT\n"
	"       COMMS MAY HAIL THE ROBOT TO FURTHER CONTROL ITS ACTIONS\n"
	"  * TRACTOR BEAM\n"
	"       SELECT A NEARBY TARGET AND ENGAGE THE TRACTOR BEAM TO DRAW\n"
	"       THE TARGET TOWARDS THE SHIP\n"
	"  * SET WAYPOINTS TO AID NAVIGATION\n"
	"\nPRESS ESC OR F1 AGAIN TO EXIT HELP\n",


	/* 5 - Comms help text */
	"COMMUNICATIONS\n\n"
	"  ENTER TEXT TO TRANSMIT ON CURRENT CHANNEL\n\n"
	"  CONTROLS\n"
	"    F1: HELP F2: NAVIGATION F3: WEAPONS F4: ENGINEERING F5: DAMAGE CONTROL\n"
	"    F6: SCIENCE F7: COMMS F8: MAIN SCREEN F9: DEMON SCREEN\n\n"
	"  * ZOOM CONTROLS MAIN SCREEN ZOOM\n"
	"  * RED ALERT SOUNDS RED ALERT ALARM\n"
	"  * TOP ROW OF BUTTONS CONTROLS MAIN SCREEN\n\n"
	"  COMMANDS\n"
	"  * COMMANDS ARE PRECEDED BY FORWARD SLASH ->  /\n"
	"  * /help\n"
	"  * /channel channel-number - change current channel\n"
	"  * /computer <english request for computer>\n"
	"  * /hail ship-name - hail ship on current channel\n"
	"\nPRESS ESC OR F1 AGAIN TO EXIT HELP\n",

	/* 6 - Demon screen help text */
	"DEMON SCREEN HELP\n\n"
	"    F1: HELP F2: NAVIGATION F3: WEAPONS F4: ENGINEERING F5: DAMAGE CONTROL\n"
	"    F6: SCIENCE F7: COMMS F8: MAIN SCREEN F9: DEMON SCREEN\n\n"
	"THE DEMON, (AKA GAMEMASTER) SCREEN ALLOWS A\n"
	"USER TO MANIPULATE THE GAME UNIVERSE\n\n"
	"* USE THE MOUSE WHEEL TO ZOOM IN AND OUT\n"
	"* SELECT BUTTONS ON LEFT SIDE OF SCREEN AND\n"
	"  USE LEFT MOUSE BUTTON TO ADD NEW ITEMS\n"
	"* UNSELECT BUTTONS ON LEFT SIDE OF SCREEN AND\n"
	"  USE OR DRAG RIGHT MOUSE BUTTON TO SELECT ITEMS\n"
	"* USE MIDDLE MOUSE BUTTON TO MOVE SELECTED ITEMS\n"
	"* USE TEXT BOX TO ENTER COMMANDS\n"
	"* USE \"SELECT NONE\" BUTTON TO DE-SELECT ITEMS\n"
	"* USE \"CAPTAIN\" BUTTON TO TAKE CONTROL OF SHIPS\n"
	"  USE ARROW KEYS TO CONTROL \"CAPTAINED\" SHIPS\n"
	"  USE PHASER AND TORPEDO BUTTONS WHILE\n"
	"  \"CAPTAINING SHIPS.\n"
	"* USE \"HELP\" COMMAND IN TEXT BOX FOR MORE INFO\n"
	"\nPRESS ESC TO OR F1 AGAIN EXIT HELP\n",

	/* 7 - Damage control help text */
	"DAMAGE CONTROL HELP\n"
	"    F1: HELP F2: NAVIGATION F3: WEAPONS F4: ENGINEERING F5: DAMAGE CONTROL\n"
	"    F6: SCIENCE F7: COMMS F8: MAIN SCREEN F9: DEMON SCREEN\n\n"
	"    * MANUAL CONTROL\n"
	"       * SELECT MANUAL THEN USE ARROW KEYS TO CONTROL ROBOT\n"
	"       * USE SPACE BAR TO PICK UP AND REPAIR OR DROP MODULES\n"
	"       * USE REPAIR STATION TO REPAIR BADLY DAMAGED MODULES\n"
	"    * AUTOMATIC CONTROL\n"
	"       * SELECT AUTO TO ALLOW THE ROBOT TO CONTROL ITSELF\n"
	"\nPRESS ESC OR F1 AGAIN TO EXIT HELP\n",

	/* 8 - Fonttest */
	"\nFONT TEST SCREEN HELP\n"
	"ABCDEFGHIJKLMNOPQRSTUVWXYZ\n"
	"abcdefghijklmnopqrstuvwxyz\n"
	"0123456789\n"
	"\nPRESS ESC OR F1 AGAIN TO EXIT HELP\n",

	/* 9 - Intro Screen */
	"\nINTRO SCREEN HELP\n"
	"\nPRESS ESC OR F1 AGAIN TO EXIT HELP\n",

	/* 10 - Lobby screen */
	"\nLOBBY SCREEN HELP\n\n"
	" * 1. CLICK ON THE SERVER YOU WISH TO CONNECT TO.\n\n"
	"   2. ONCE YOU SELECT A SERVER, A CONNECT BUTTON\n"
	"      WILL APPEAR NEXT TO IT.  CLICK ON THIS CONNECT\n"
	"      BUTTON TO CONNECT TO THE SERVER.\n\n"
	" * CLICK ON THE CANCEL BUTTON TO RETURN TO\n"
	"   THE NETWORK SETUP SCREEN.\n\n"
	"\nPRESS ESC OR F1 AGAIN TO EXIT HELP\n",

	/* 11 - Connecting screen */
	"\nCONNECTING SCREEN HELP\n"
	"\nPRESS ESC OR F1 AGAIN TO EXIT HELP\n",

	/* 12 - Connected screen */
	"\nCONNECTED SCREEN HELP\n"
	"\nPRESS ESC OR F1 AGAIN TO EXIT HELP\n",

	/* 13 - Network Setup screen */
	"\nNETWORK SETUP SCREEN HELP\n\n"
	"* LOBBY SERVER NAME OR IP ADDRESS\n"
	"     ENTER THE HOST NAME OR IP ADDRESS OF THE COMPUTER\n"
	"     RUNNING THE LOBBY PROCESS. IF YOU ARE RUNNING THE\n"
	"     LOBBY PROCESS ON THIS COMPUTER, ENTER localhost\n"
	"* START LOBBY SERVER"
	"     ONLY CLICK THIS BUTTON IF YOU WANT TO START THE\n"
	"     LOBBY SERVER ON THIS HOST AND HAVE NOT ALREADY\n"
	"     STARTED IT IT. IF YOU USED A SCRIPT LIKE QUICKSTART\n"
	"     OR SNIS_LAUNCHER, YOU DO NOT NEED TO CLICK THIS\n"
	"* GAME SERVER NICKNAME\n"
	"     ENTER WHATVER YOU LIKE FOR A SERVER NICKNAME HERE\n"
	"     THIS ONLY HAS AN EFFECT IF YOU START A GAME SERVER\n"
	"     USING THE START GAME SERVER BUTTON\n"
	"     ORDINARILY YOU WOULD INSTEAD START A GAME SERVER\n"
	"     USING THE SNIS_LAUNCHER SCRIPT\n"
	"* START GAME SERVER\n"
	"     USE THIS TO START A GAME SERVER.  NORMALLY YOU\n"
	"     DO NOT NEED TO USE THIS, INSTEAD START A GAME\n"
	"     SERVER USING THE SNIS_LAUNCHER OR QUICKSTART SCRIPT\n"
	"* CREATE SHIP\n"
	"     SELECT THIS IF YOU WISH TO CREATE A NEW SHIP\n"
	"     IF YOU HAVE ALREADY CREATED A SHIP WITH A PARTICULAR NAME\n"
	"     DO NOT TRY TO CREATE THE SAME SHIP MORE THAN ONCE\n"
	"* JOIN SHIP\n"
	"     IF OTHER PLAYERS ARE ALREADY ABOARD THE SHIP, SELECT\n"
	"     JOIN SHIP. IF YOU ARE THE FIRST PLAYER ABOARD, DO NOT\n"
	"     SELECT JOIN SHIP.\n"
	"* ROLES\n"
	"     SELECT THE ROLES (STATIONS) YOU WISH YOUR COMPUTER TO ACT AS.\n"
	"* SHIP NAME AND PASSWORD\n"
	"     ENTER THE NAME OF YOUR SHIP AND PASSWORD FOR YOUR SHIP\n"
	"* CONNECT TO LOBBY\n"
	"     PRESS THE CONNECT TO LOBBY BUTTON TO CONNECT TO THE LOBBY\n"
	"\nPRESS ESC OR F1 AGAIN TO EXIT HELP\n",
};

static void draw_help_text(GtkWidget *w, char *text)
{
	int line = 0;
	int i, y = 70;
	char buffer[256];
	int buflen = 0;

	strcpy(buffer, "");

	i = 0;
	do {
		if (text[i] == '\n' || text[i] == '\0') {
			if (line >= 0 && line < 40) {
				buffer[buflen] = '\0';
				sng_abs_xy_draw_string(buffer, NANO_FONT, 60, y);
				y += font_lineheight[NANO_FONT];
				strcpy(buffer, "");
				buflen = 0;
				line++;
				if (text[i] == '\0')
					break;
				i++;
				continue;
			} else {
				if (line >= 40)
					break;
			}
		}
		buffer[buflen++] = text[i++];
	} while (1);
}

static void draw_help_screen(GtkWidget *w)
{
	sng_set_foreground(BLACK);
	snis_draw_rectangle(1, 50, 50, SCREEN_WIDTH - 100, SCREEN_HEIGHT - 100);
	sng_set_foreground(UI_COLOR(help_text));
	snis_draw_rectangle(0, 50, 50, SCREEN_WIDTH - 100, SCREEN_HEIGHT - 100);
	if (displaymode < 0 || displaymode >= ARRAYSIZE(help_text)) {
		char msg[100];
		snprintf(msg, sizeof(msg), "\nUNKNOWN SCREEN %d, NO HELP AVAILABLE\n", displaymode);
		draw_help_text(w, msg);
		return;
	}
	draw_help_text(w, help_text[displaymode]);
}

#define QUIT_BUTTON_WIDTH (200 * SCREEN_WIDTH / 800)
#define QUIT_BUTTON_HEIGHT (50 * SCREEN_HEIGHT / 600)
#define QUIT_BUTTON_X (130 * SCREEN_WIDTH / 800)
#define QUIT_BUTTON_Y (420 * SCREEN_HEIGHT / 600)
#define NOQUIT_BUTTON_X (480 * SCREEN_WIDTH / 800)

static void draw_quit_screen(GtkWidget *w)
{
	int x;
	static int quittimer = 0;

	quittimer++;

	sng_set_foreground(BLACK);
	snis_draw_rectangle(1, txx(100), txy(100),
			SCREEN_WIDTH - txx(200), SCREEN_HEIGHT - txy(200));
	sng_set_foreground(UI_COLOR(quit_border));
	snis_draw_rectangle(FALSE, txx(100), txy(100),
			SCREEN_WIDTH - txx(200), SCREEN_HEIGHT - txy(200));
	sng_set_foreground(UI_COLOR(quit_text));
	sng_abs_xy_draw_string("Quit?", BIG_FONT, txx(300), txy(280));

	if (current_quit_selection == 1) {
		x = QUIT_BUTTON_X;
		sng_set_foreground(UI_COLOR(quit_selection));
	} else {
		x = NOQUIT_BUTTON_X;
		sng_set_foreground(UI_COLOR(quit_unselected));
	}
	sng_abs_xy_draw_string("Quit Now", SMALL_FONT, txx(150), txy(450));
	if (current_quit_selection == 0)
		sng_set_foreground(UI_COLOR(quit_selection));
	else
		sng_set_foreground(UI_COLOR(quit_unselected));
	sng_abs_xy_draw_string("Don't Quit", SMALL_FONT, txx(500), txy(450));

	if ((quittimer & 0x04)) {
		sng_set_foreground(UI_COLOR(quit_selection));
		snis_draw_rectangle(FALSE, x, QUIT_BUTTON_Y,
			QUIT_BUTTON_WIDTH, QUIT_BUTTON_HEIGHT);
	}
}

#define FRAME_INDEX_MAX 10

static void do_display_frame_stats(const float frame_rates[], const float frame_times[], const int nframes)
{
/* This works out to either 29 or 58 FPS */
#define ACCEPTABLE_FRAME_RATE (frame_rate_hz - 1.0 - use_60_fps)
	float avg_frame_rate = 0;
	float avg_frame_time = 0;
	int i;
	for (i = 0; i < nframes; i++) {
		avg_frame_rate += frame_rates[i];
		avg_frame_time += frame_times[i];
	}
	avg_frame_rate /= (float) nframes;
	avg_frame_time /= (float) nframes;

	if (display_frame_stats > 0 || avg_frame_rate > 1.0 / ACCEPTABLE_FRAME_RATE) {
		if (avg_frame_rate <= 1.0 / ACCEPTABLE_FRAME_RATE)
			sng_set_foreground(WHITE);
		else
			sng_set_foreground(ORANGERED);
		char stat_buffer[30];
		snprintf(stat_buffer, sizeof(stat_buffer), "FPS %5.2f", 1.0 / avg_frame_rate);
		sng_abs_xy_draw_string(stat_buffer, NANO_FONT, txx(2), txy(10));
		snprintf(stat_buffer, sizeof(stat_buffer), "T %0.2f ms", avg_frame_time * 1000.0);
		sng_abs_xy_draw_string(stat_buffer, NANO_FONT, txx(92), txy(10));
		snprintf(stat_buffer, sizeof(stat_buffer), "%8.0f", universe_timestamp());
		sng_abs_xy_draw_string(stat_buffer, NANO_FONT, SCREEN_WIDTH-85, 10);
	}
	if (display_frame_stats > 1)
		graph_dev_display_debug_menu_show();
}

static void maybe_reload_shaders(void)
{
	if (!reload_shaders)
		return;
	fprintf(stderr, "Reloading shaders\n");
	graph_dev_reload_all_shaders();
	reload_shaders = 0;
}

static int main_da_expose(GtkWidget *w, GdkEvent *event, gpointer p)
{
	static double last_frame_time = 0;
	static int how_long_to_wait = -1; /* 4 seconds */
	static int frame_index = 0;
	static float frame_rates[FRAME_INDEX_MAX];
	static float frame_times[FRAME_INDEX_MAX];
	int player_lost_rts;
	int player_won_rts;
	int i;
	/* If main_da_configure has not been called when gc != null, then return immediately.
	 * Otherwise, OpenGL will not have been initialized and snis_client will segfault because
	 * the OpenGL functions won't have been properly initialized. da_configured will be set
	 * to 1 when main_da_configure executes with a non-null gc.
	 */
	if (!da_configured) {
		return 0;
	}

	double start_time = time_now_double();

	struct snis_entity *o;

	make_science_forget_stuff();

	maybe_reload_shaders();
	load_textures();

#ifndef WITHOUTOPENGL
	GdkGLContext *gl_context = gtk_widget_get_gl_context(main_da);
	GdkGLDrawable *gl_drawable = gtk_widget_get_gl_drawable(main_da);

	if (!gdk_gl_drawable_gl_begin(gl_drawable, gl_context))
		g_assert_not_reached();

	graph_dev_start_frame();
#endif

	sng_set_foreground(WHITE);
	
	/* show_mouse_debug(); */

	if (warp_limbo_countdown) {
		warp_limbo_countdown--;
		if (in_the_process_of_quitting)
			draw_quit_screen(w);
		show_warp_limbo_screen(w);
		goto end_of_drawing;
	} else if (damage_limbo_countdown) {
		show_warp_hash_screen(w);
		damage_limbo_countdown--;
		if (in_the_process_of_quitting)
			draw_quit_screen(w);
		goto end_of_drawing;
	}

	if (displaymode < DISPLAYMODE_FONTTEST) {
		if (!(o = find_my_ship())) {
			char msg[100];
			if (how_long_to_wait == -1)
				how_long_to_wait = 4 * frame_rate_hz;
			if (how_long_to_wait > 0) {
				snprintf(msg, sizeof(msg) - 1,
						"PREPARE FOR THE JUMP TO LIGHTSPEED, SPACE NERD");
				how_long_to_wait--;
			}
			if (how_long_to_wait == 0) {
				snprintf(msg, sizeof(msg) - 1,
						"ERROR: CANNOT FIND POINTER TO PLAYER SHIP, SORRY!");
			}
			show_info_message(w, msg);
			if (in_the_process_of_quitting)
				draw_quit_screen(w);
			goto end_of_drawing;
		} else {
			how_long_to_wait = frame_rate_hz * 4; /* 4 seconds */
		}
		if (o->alive == 0 && displaymode != DISPLAYMODE_DEMON) {
			red_alert_mode = 0;
			show_death_screen(w);
			if (in_the_process_of_quitting)
				draw_quit_screen(w);
			goto end_of_drawing;
		}
		if (global_rts_mode) {
			player_lost_rts = 0;
			player_won_rts = 0;
			for (i = 0; i < ARRAYSIZE(rts_planet); i++) {
				if (rts_planet[i].health == 0) {
					if (rts_planet[i].faction == o->sdata.faction)
						player_lost_rts = 1;
					else
						player_won_rts = 1;
				}
			}
			if (player_lost_rts)
				player_won_rts = 0;
			if (player_lost_rts) {
				show_rts_loss_screen(w);
				goto end_of_drawing;
			} else if (player_won_rts) {
				show_rts_win_screen(w);
				goto end_of_drawing;
			}
		}
	}
	maybe_grab_or_ungrab_mouse(w, GDK_BUTTON_PRESS_MASK |
					GDK_BUTTON_RELEASE_MASK |
					GDK_BUTTON3_MOTION_MASK |
					GDK_POINTER_MOTION_MASK);
	switch (displaymode) {
	case DISPLAYMODE_FONTTEST:
		show_fonttest(w);
		break;
	case DISPLAYMODE_INTROSCREEN:
		show_introscreen(w);
		break;
	case DISPLAYMODE_LOBBYSCREEN:
		show_lobbyscreen(w);
		break;
	case DISPLAYMODE_CONNECTING:
		show_connecting_screen(w);
		break;
	case DISPLAYMODE_CONNECTED:
		show_connected_screen(w);
		break;
	case DISPLAYMODE_MAINSCREEN:
		show_mainscreen(w);
		break;
	case DISPLAYMODE_NAVIGATION:
		if (main_nav_hybrid)
			show_mainscreen(w);
		show_navigation(w);
		break;
	case DISPLAYMODE_WEAPONS:
		show_manual_weapons(w);
		break;
	case DISPLAYMODE_ENGINEERING:
		show_engineering(w);
		break;
	case DISPLAYMODE_SCIENCE:
		show_science(w);
		break;
	case DISPLAYMODE_COMMS:
		show_comms(w);
		break;
	case DISPLAYMODE_DEMON:
		show_demon(w);
		break;
	case DISPLAYMODE_DAMCON:
		show_damcon(w);
		break;
	case DISPLAYMODE_NETWORK_SETUP:
		show_network_setup(w);
		break;
	default:
		show_fonttest(w);
		break;
	}
	ui_element_list_draw(uiobjs);
	ui_element_list_maybe_draw_tooltips(uiobjs, mouse.x, mouse.y);

	/* this has to come after ui_element_list_draw() to avoid getting clobbered */
	if (displaymode == DISPLAYMODE_ENGINEERING)
		show_engineering_damage_report(w, eng_ui.selected_subsystem);

	if (helpmode)
		draw_help_screen(w);
	if (in_the_process_of_quitting)
		draw_quit_screen(w);

	do_display_frame_stats(frame_rates, frame_times, FRAME_INDEX_MAX);

end_of_drawing:

	graph_dev_end_frame();

#ifndef WITHOUTOPENGL
	gdk_gl_drawable_wait_gdk(gl_drawable);
	gdk_gl_drawable_wait_gl(gl_drawable);
#endif

#ifndef WITHOUTOPENGL
	/* swap buffer if we're using double-buffering */
	if (gdk_gl_drawable_is_double_buffered(gl_drawable))     
		gdk_gl_drawable_swap_buffers(gl_drawable); 
	else {
		/* All programs should call glFlush whenever 
		 * they count on having all of their previously 
		 * issued commands completed.
		 */
		glFlush();
	}

	/* Delimits the end of the OpenGL execution. */
	gdk_gl_drawable_gl_end(gl_drawable);
#endif

	double end_time = time_now_double();

	frame_rates[frame_index] = start_time - last_frame_time;
	frame_times[frame_index] = end_time - start_time;
	frame_index = (frame_index + 1) % FRAME_INDEX_MAX;
	last_frame_time = start_time;

	return 0;
}

static void really_quit(void);

static void maybe_play_rocket_sample(void)
{
	double volume, thruster_volume;
	float begin, end;
	static float last_volume = 0.0;
	static float last_thruster_volume = 0.0;
	struct snis_entity *o = find_my_ship();
	const double xv[] = { 0.0, 0.75, 1.0, 2.0 };
	const double yv[] = { 0.0, 0.25, 1.0, 1.0 };

	if (suppress_rocket_noise)
		return;
	if (!(role & ROLE_SOUNDSERVER))
		return;
	if ((timer & 0x00f) != 0)
		return;
	volume = sample_power_data_impulse_current() / 255.0;
	if (o) {
		if (o->alive == 0) /* ship is dead, do not make rocket noises */
			return;
		thruster_volume = (fabs(o->tsd.ship.yaw_velocity) / MAX_YAW_VELOCITY +
					fabs(o->tsd.ship.pitch_velocity) / MAX_PITCH_VELOCITY +
					fabs(o->tsd.ship.roll_velocity) / MAX_ROLL_VELOCITY) / 3.0;
	} else {
		thruster_volume = 0;
	}
	volume = table_interp(volume, xv, yv, 4);

	/* If main thruster is really pushing, make some extra creaking noises */
	if (volume >= 0.94 && snis_randn(1000) < 50)
		wwviaudio_add_sound(HULL_CREAK_0 + (snis_randn(1000) % NHULL_CREAK_SOUNDS));

	if (volume < 0.01 && thruster_volume < 0.01) { /* Don't waste CPU playing silence. */
		last_volume = volume;
		last_thruster_volume = thruster_volume;
		return;
	}
	begin = snis_randn(1000);
	/* Rocket sample is 20 seconds long. We want to choose 1 second sample
	 * somewhere after the 1st second, and ending before the last second.
	 * We start new samples every 16/30ths of a second, so there is about a
	 * half second of overlap, in general, there will be two samples playing
	 * at once. This helps mask the half-second periodicity in the
	 * starting/ending of new/old bits of sound.
	 */
	begin = 1.0 / 20.0 + (begin / 1000.0) * 0.90;
	end = begin + 1.0 / 20.0;
	wwviaudio_add_sound_segment(ROCKET_SAMPLE, last_volume * rocket_noise_volume,
					volume * rocket_noise_volume, begin, end, NULL, NULL);
	last_volume = volume;

	/* Thruster volume sample is 10 seconds long, choose 1 second randomly from those 10 seconds. */
	begin = snis_randn(900) / 1000.0;
	end = begin + 0.05;
	wwviaudio_add_sound_segment(THRUSTER_SAMPLE, last_thruster_volume * rocket_noise_volume,
					thruster_volume * rocket_noise_volume, begin, end, NULL, NULL);
	last_thruster_volume = thruster_volume;
}

static void notify_server_of_displaymode_change(void)
{
	static int old_displaymode = -1;
	if (displaymode == old_displaymode)
		return;
	queue_to_server(snis_opcode_subcode_pkt("bbb", OPCODE_CLIENT_CONFIG,
			OPCODE_CLIENT_NOTIFY_CURRENT_STATION, (uint8_t) displaymode));
	old_displaymode = displaymode;
}

static void adjust_audio_dynamic_range_compression(void)
{
	static float old_threshold = -1.0;
	static float old_limit = -1.0;
	static float old_voip_threshold = -1.0;
	static float old_voip_limit = -1.0;

	if (old_threshold != compressor_threshold || old_limit != compressor_limit) {
		if (compressor_limit < compressor_threshold)
			compressor_limit = compressor_threshold;
		old_threshold = compressor_threshold;
		old_limit = compressor_limit;
		wwviaudio_set_compressor_params(compressor_threshold, compressor_limit);
	}
	if (old_voip_threshold != voip_compressor_threshold || old_voip_limit != voip_compressor_limit) {
		if (voip_compressor_limit < voip_compressor_threshold)
			voip_compressor_limit = voip_compressor_threshold;
		old_voip_threshold = voip_compressor_threshold;
		old_voip_limit = voip_compressor_limit;
		wwviaudio_set_voip_compressor_params(voip_compressor_threshold, voip_compressor_limit);
	}
}

static void adjust_tonemapping_gain(void)
{
	static float old_tonemapping_gain = -1;

	if (old_tonemapping_gain == tonemapping_gain)
		return;
	old_tonemapping_gain = tonemapping_gain;
	graph_dev_set_tonemapping_gain(tonemapping_gain);
}

gint advance_game(gpointer data)
{
	int time_to_switch_servers;
	static int skip = 0;

	/* Bit of a hack to enable 60 fps.  advance game now gets called at 60Hz always, but
	 * skip every other update if we want to run the game at 30Hz.
	 * Also, we increment timer only every other frame when running at 60 fps because
	 * there are many, many places where we assume that timer updates at 30Hz.
	 */
	if (!use_60_fps) {
		if (skip) {
			skip = !skip;
			return TRUE;
		}
		frame_rate_hz = 30;
		skip = !skip;
		timer++;
	} else {
		frame_rate_hz = 60;
		if (!skip)
			timer++;
		skip = !skip;
	}

	if (red_alert_mode && (role & ROLE_SOUNDSERVER)) {
		if ((timer % 45) == 0 && (timer % (45 * 6)) < (45 * 3))
			wwviaudio_add_sound(RED_ALERT_SOUND);
	}

	deal_with_joysticks();
	deal_with_keyboard();
	deal_with_mouse();
	deal_with_physical_io_devices();
	sng_set_font_family(current_typeface);
	adjust_audio_dynamic_range_compression();
	adjust_tonemapping_gain();

	if (in_the_process_of_quitting) {
		gdk_threads_enter();
		gtk_widget_queue_draw(main_da);
		gdk_threads_leave();
		if (final_quit_selection)
			really_quit();
	}

	gdk_threads_enter();
	gtk_widget_queue_draw(main_da);
	pthread_mutex_lock(&universe_mutex);
	move_sparks();
	move_objects();
	expire_starmap_entries();
	notify_server_of_displaymode_change();
	pthread_mutex_unlock(&universe_mutex);
	nframes++;

	GtkAllocation alloc;
	gtk_widget_get_allocation(main_da, &alloc);
	gdk_window_invalidate_rect(gtk_widget_get_root_window(main_da), &alloc, FALSE);

	gdk_threads_leave();

	/* Check if it's time to switch servers */
	pthread_mutex_lock(&to_server_queue_event_mutex);
	time_to_switch_servers = (switched_server != -1);
	if (time_to_switch_servers) {
		fprintf(stderr, "snis_client: switch server hack\n");
		/* this is a hack */
		switched_server2 = switched_server;
		switched_server = -1;
		displaymode = DISPLAYMODE_LOBBYSCREEN;
	}
	pthread_mutex_unlock(&to_server_queue_event_mutex);
	maybe_play_rocket_sample();

	return TRUE;
}

static unsigned int load_texture(char *filename, int linear_colorspace)
{
	char fname[PATH_MAX + 1];

	snprintf(fname, sizeof(fname), "%s/%s", asset_dir, filename);
	return graph_dev_load_texture(replacement_asset_lookup(fname, &replacement_assets), linear_colorspace);
}

static unsigned int load_texture_no_mipmaps(char *filename, int linear_colorspace)
{
	char fname[PATH_MAX + 1];

	snprintf(fname, sizeof(fname), "%s/%s", asset_dir, filename);
	return graph_dev_load_texture_no_mipmaps(replacement_asset_lookup(fname,
				&replacement_assets), linear_colorspace);
}

static unsigned int load_cubemap_textures(int is_inside, char *filenameprefix, int linear_colorspace)
{
	/*
	 * SNIS wants skybox textures in six files named like this:
	 * blah0.png
	 * blah1.png
	 * blah2.png
	 * blah3.png
	 * blah4.png
	 * blah5.png
         *
	 * and those images should be laid out like this:
	 *
	 *  +------+
	 *  |  4   |
	 *  |      |
	 *  +------+------+------+------+
	 *  |  0   |  1   |  2   |  3   |
	 *  |      |      |      |      |
	 *  +------+------+------+------+
	 *  |  5   |
	 *  |      |
	 *  +------+
	 *
	 *  Why?  No reason other than that's how I did it in cosmic-space-boxinator
	 *  See: https://github.com/smcameron/cosmic-space-boxinator
	 *
	 *  Opengl does it a bit differenty, so we have the arbitrariness you see below.
	 */
	int i;
	char filename[6][PATH_MAX + 1], fname[PATH_MAX + 1];

	for (i = 0; i < 6; i++) {
		snprintf(fname, sizeof(fname), "%s/%s%d.png", asset_dir, filenameprefix, i);
		strcpy(filename[i], replacement_asset_lookup(fname, &replacement_assets));
	}
	return graph_dev_load_cubemap_texture(is_inside, linear_colorspace,
					filename[1], filename[3], filename[4],
					filename[5], filename[0], filename[2]);
}

static void expire_cubemap_texture(int is_inside, char *filenameprefix)
{
	int i;
	char filename[6][PATH_MAX + 1], fname[PATH_MAX + 1];

	for (i = 0; i < 6; i++) {
		snprintf(fname, sizeof(fname), "%s/%s%d.png", asset_dir, filenameprefix, i);
		strcpy(filename[i], replacement_asset_lookup(fname, &replacement_assets));
	}
	graph_dev_expire_cubemap_texture(is_inside, filename[1], filename[3], filename[4],
					filename[5], filename[0], filename[2]);
}

static void load_skybox_textures(char *filenameprefix)
{
	/*
	 * SNIS wants skybox textures in six files named like this:
	 * blah0.png
	 * blah1.png
	 * blah2.png
	 * blah3.png
	 * blah4.png
	 * blah5.png
         *
	 * and those images should be laid out like this:
	 *
	 *  +------+
	 *  |  4   |
	 *  |      |
	 *  +------+------+------+------+
	 *  |  0   |  1   |  2   |  3   |
	 *  |      |      |      |      |
	 *  +------+------+------+------+
	 *  |  5   |
	 *  |      |
	 *  +------+
	 *
	 *  Why?  No reason other than that's how I did it in cosmic-space-boxinator
	 *  See: https://github.com/smcameron/cosmic-space-boxinator
	 *
	 *  Opengl does it a bit differenty, so we have the arbitrariness you see below.
	 */
	int i;
	char filename[6][PATH_MAX + 1], fname[PATH_MAX + 1];

	for (i = 0; i < 6; i++) {
		snprintf(fname, sizeof(fname), "%s/%s%d.png", asset_dir, filenameprefix, i);
		strcpy(filename[i], replacement_asset_lookup(fname, &replacement_assets));
	}
	graph_dev_load_skybox_texture(filename[3], filename[1], filename[4],
					filename[5], filename[0], filename[2]);
}

static void expire_skybox_texture(char *filenameprefix)
{
	int i;
	char filename[6][PATH_MAX + 1], fname[PATH_MAX + 1];

	for (i = 0; i < 6; i++) {
		snprintf(fname, sizeof(fname), "%s/%s%d.png", asset_dir, filenameprefix, i);
		strcpy(filename[i], replacement_asset_lookup(fname, &replacement_assets));
	}
	graph_dev_expire_cubemap_texture(1, filename[3], filename[1], filename[4],
					filename[5], filename[0], filename[2]);
}

static void set_planetary_lightning_material_uv_coords(void)
{
	int i;

	for (i = 0; i < NLIGHTNINGS; i++) {
		/* Extract uv coords from meshes */
		struct material *m = &planetary_lightning_material[i];
		m->planetary_lightning.u1 = planetary_lightning_mesh[i]->tex[0].u;
		m->planetary_lightning.v1 = planetary_lightning_mesh[i]->tex[1].v;
		m->planetary_lightning.width = 0.25;
	}
}

static void init_meshes();

/* call back for configure_event (for window resize) */
static gint main_da_configure(GtkWidget *w, GdkEventConfigure *event)
{
	GdkRectangle cliprect;

	/* first time through, gc is null, because gc can't be set without */
	/* a window, but, the window isn't there yet until it's shown, but */
	/* the show generates a configure... chicken and egg.  And we can't */
	/* proceed without gc != NULL...  but, it's ok, because 1st time thru */
	/* we already sort of know the drawing area/window size. */

	if (gc == NULL)
		return TRUE;

	real_screen_width =  w->allocation.width;
	real_screen_height =  w->allocation.height;
	sng_set_screen_size(real_screen_width, real_screen_height);

	gdk_gc_set_clip_origin(gc, 0, 0);
	cliprect.x = 0;	
	cliprect.y = 0;	
	cliprect.width = real_screen_width;	
	cliprect.height = real_screen_height;	
	gdk_gc_set_clip_rectangle(gc, &cliprect);

#ifndef WITHOUTOPENGL
	GdkGLContext *gl_context = gtk_widget_get_gl_context(main_da);
	GdkGLDrawable *gl_drawable = gtk_widget_get_gl_drawable(main_da);

	/* Delimits the begining of the OpenGL execution. */
	if (!gdk_gl_drawable_gl_begin(gl_drawable, gl_context))
		g_assert_not_reached();

	/* specify the lower left corner of our viewport, as well as width/height
	 * of the viewport
	 */
	GtkAllocation alloc;
	gtk_widget_get_allocation(main_da, &alloc);

	glClearColor(0.0, 0.0, 0.0, 0.0);

	/* Delimits the end of the OpenGL execution. */
	gdk_gl_drawable_gl_end(gl_drawable);
#endif

	static int gl_is_setup = 0;
	if (!gl_is_setup) {
		char shader_dir[PATH_MAX];
		snprintf(shader_dir, sizeof(shader_dir), "%s/%s", asset_dir, "shader");
		graph_dev_setup(shader_dir);
		gl_is_setup = 1;
	}

	load_textures();

	static int meshes_loaded = 0;
	if (!meshes_loaded) {
		init_meshes();
		meshes_loaded = 1;
	}
	set_planetary_lightning_material_uv_coords();

	static int static_exc_loaded = 0;
	if (!static_exc_loaded) {
		struct entity *e = add_entity(ecx, sun_mesh, SUNX, SUNY, SUNZ, WHITE);
		if (e) {
			update_entity_material(e, &sun_material);
			sun_entity = e;
		}

		static_exc_loaded = 1;
	}

	/* Indicate that the da has been configured fully (with a non-null gc) */
	da_configured = 1;
	return TRUE;
}

static int read_solarsystem_config(const char *solarsystem_name,
	struct solarsystem_asset_spec **assets)
{
	char path[PATH_MAX];

	printf("Reading solarsystem specifications...");
	fflush(stdout);
	snprintf(path, sizeof(path), "%s/solarsystems/%s/assets.txt", asset_dir, solarsystem_name);
	if (*assets)
		solarsystem_asset_spec_free(*assets);
	*assets = solarsystem_asset_spec_read(replacement_asset_lookup(path, &replacement_assets));
	if (!*assets)
		return -1;
	if ((*assets)->spec_errors || (*assets)->spec_warnings)
		fprintf(stderr, "snis_client: solarsystem asset spec %s: %d errors, %d warnings.\n",
			path, (*assets)->spec_errors, (*assets)->spec_warnings);
	printf("done\n");
	fflush(stdout);
	return 0;
}

static struct mesh **allocate_starbase_mesh_ptrs(int nstarbase_meshes)
{
	struct mesh **m = malloc(sizeof(*m) * nstarbase_meshes);
	memset(m, 0, sizeof(*m) * nstarbase_meshes);
	return m;
}

static void init_thrust_material(struct material *thrust_material, char *image_filename)
{
	material_init_textured_particle(thrust_material);
	thrust_material->textured_particle.texture_id = load_texture(image_filename, 0);
	thrust_material->textured_particle.radius = 1.5;
	thrust_material->textured_particle.time_base = 0.1;
}

static int load_static_textures(void)
{
	if (static_textures_loaded)
		return 0;

	struct planetary_ring_data *ring_data;

	material_init_textured_particle(&green_phaser_material);
	green_phaser_material.textured_particle.texture_id = load_texture("textures/green-burst.png", 0);
	green_phaser_material.textured_particle.radius = 0.75;
	green_phaser_material.textured_particle.time_base = 0.25;

	material_init_texture_mapped_unlit(&red_laser_material);
	red_laser_material.billboard_type = MATERIAL_BILLBOARD_TYPE_AXIS;
	red_laser_material.texture_mapped_unlit.texture_id = load_texture("textures/red-laser-texture.png", 0);
	red_laser_material.texture_mapped_unlit.do_blend = 1;

	material_init_texture_mapped_unlit(&blue_tractor_material);
	blue_tractor_material.billboard_type = MATERIAL_BILLBOARD_TYPE_AXIS;
	blue_tractor_material.texture_mapped_unlit.texture_id = load_texture("textures/blue-tractor-texture.png", 0);
	blue_tractor_material.texture_mapped_unlit.do_blend = 1;

	material_init_texture_mapped_unlit(&red_torpedo_material);
	red_torpedo_material.billboard_type = MATERIAL_BILLBOARD_TYPE_SPHERICAL;
	red_torpedo_material.rotate_randomly = 1;
	red_torpedo_material.texture_mapped_unlit.texture_id = load_texture("textures/red-torpedo-texture.png", 0);
	red_torpedo_material.texture_mapped_unlit.do_blend = 1;

	material_init_texture_mapped_unlit(&spark_material);
	spark_material.billboard_type = MATERIAL_BILLBOARD_TYPE_SPHERICAL;
	spark_material.rotate_randomly = 1;
	spark_material.texture_mapped_unlit.texture_id = load_texture("textures/spark-texture.png", 0);
	spark_material.texture_mapped_unlit.do_blend = 1;

	material_init_texture_mapped_unlit(&flare_material);
	flare_material.billboard_type = MATERIAL_BILLBOARD_TYPE_SPHERICAL;
	flare_material.rotate_randomly = 1;
	flare_material.texture_mapped_unlit.texture_id = load_texture("textures/spark-texture.png", 0);
	flare_material.texture_mapped_unlit.do_blend = 1;

	material_init_texture_mapped_unlit(&blackhole_spark_material);
	blackhole_spark_material.billboard_type = MATERIAL_BILLBOARD_TYPE_SPHERICAL;
	blackhole_spark_material.rotate_randomly = 1;
	blackhole_spark_material.texture_mapped_unlit.texture_id = load_texture("textures/green-spark.png", 0);
	blackhole_spark_material.texture_mapped_unlit.do_blend = 1;

	material_init_texture_mapped_unlit(&laserflash_material);
	laserflash_material.billboard_type = MATERIAL_BILLBOARD_TYPE_SPHERICAL;
	laserflash_material.rotate_randomly = 1;
	laserflash_material.texture_mapped_unlit.texture_id = load_texture("textures/laserflash.png", 0);
	laserflash_material.texture_mapped_unlit.do_blend = 1;

	material_init_texture_mapped_unlit(&warp_effect_material);
	warp_effect_material.billboard_type = MATERIAL_BILLBOARD_TYPE_SPHERICAL;
	warp_effect_material.rotate_randomly = 1;
	warp_effect_material.texture_mapped_unlit.texture_id = load_texture("textures/warp-effect.png", 0);
	warp_effect_material.texture_mapped_unlit.do_blend = 1;

	int i;
	planetary_ring_texture_id = load_texture_no_mipmaps("textures/planetary-ring0.png", 0);

	ring_data = calloc(NPLANETARY_RING_MATERIALS, sizeof(*ring_data));
	init_planetary_ring_data(ring_data, NPLANETARY_RING_MATERIALS, PLANETARY_RING_MTWIST_SEED);
	for (i = 0; i < NPLANETARY_RING_MATERIALS; i++) { /* Copy ring data into material struct */
		material_init_textured_planet_ring(&planetary_ring_material[i]);
		planetary_ring_material[i].textured_planet_ring.texture_id = planetary_ring_texture_id;
		planetary_ring_material[i].textured_planet_ring.alpha = ring_data[i].alpha;
		planetary_ring_material[i].textured_planet_ring.texture_v = ring_data[i].texture_v;
		planetary_ring_material[i].textured_planet_ring.inner_radius = ring_data[i].inner_radius;
		planetary_ring_material[i].textured_planet_ring.outer_radius = ring_data[i].outer_radius;
	}
	free(ring_data);

	material_init_textured_shield(&shield_material);
	shield_material.textured_shield.texture_id = load_cubemap_textures(0, "textures/shield-effect-", 1);

	for (i = 0; i < NNEBULA_MATERIALS; i++) {
		char filename[20];
		snprintf(filename, sizeof(filename), "nebula%d.mat", i);

		material_nebula_read_from_file(asset_dir, filename, &nebula_material[i]);
		nebula_material[i].nebula.alpha *= 0.125;
	}

	material_init_texture_cubemap(&asteroid_material[0]);
	asteroid_material[0].texture_cubemap.texture_id = load_cubemap_textures(0, "textures/asteroid1-", 0);
	material_init_texture_cubemap(&asteroid_material[1]);
	asteroid_material[1].texture_cubemap.texture_id = load_cubemap_textures(0, "textures/asteroid2-", 0);

	material_init_texture_mapped_unlit(&wormhole_material);
	wormhole_material.texture_mapped_unlit.texture_id = load_texture("textures/wormhole.png", 0);
	wormhole_material.texture_mapped_unlit.do_cullface = 0;
	wormhole_material.texture_mapped_unlit.do_blend = 1;
	wormhole_material.texture_mapped_unlit.tint = sng_get_color(MAGENTA);
	wormhole_material.texture_mapped_unlit.alpha = 0.5;

	init_thrust_material(&thrust_material[0], "textures/thrustblue.png");
	init_thrust_material(&thrust_material[1], "textures/thrustred.png");
	init_thrust_material(&thrust_material[2], "textures/thrustgreen.png");
	init_thrust_material(&thrust_material[3], "textures/thrustyellow.png");
	init_thrust_material(&thrust_material[4], "textures/thrustviolet.png");

	material_init_texture_mapped_unlit(&thrust_flare_material[0]);
	thrust_flare_material[0].billboard_type = MATERIAL_BILLBOARD_TYPE_SPHERICAL;
	thrust_flare_material[0].rotate_randomly = 1;
	thrust_flare_material[0].texture_mapped_unlit.texture_id = load_texture("textures/thrust_flare.png", 0);
	thrust_flare_material[0].texture_mapped_unlit.do_blend = 1;

	thrust_flare_material[1] = thrust_flare_material[0];
	thrust_flare_material[2] = thrust_flare_material[0];
	thrust_flare_material[3] = thrust_flare_material[0];
	thrust_flare_material[4] = thrust_flare_material[0];

	thrust_flare_material[0].texture_mapped_unlit.tint.red = 0.8;
	thrust_flare_material[0].texture_mapped_unlit.tint.blue = 1.0;
	thrust_flare_material[0].texture_mapped_unlit.tint.green = 0.8;

	thrust_flare_material[1].texture_mapped_unlit.tint.red = 1.0;
	thrust_flare_material[1].texture_mapped_unlit.tint.blue = 0.8;
	thrust_flare_material[1].texture_mapped_unlit.tint.green = 0.8;

	thrust_flare_material[2].texture_mapped_unlit.tint.red = 0.8;
	thrust_flare_material[2].texture_mapped_unlit.tint.blue = 0.8;
	thrust_flare_material[2].texture_mapped_unlit.tint.green = 1.0;

	thrust_flare_material[3].texture_mapped_unlit.tint.red = 1.0;
	thrust_flare_material[3].texture_mapped_unlit.tint.blue = 0.8;
	thrust_flare_material[3].texture_mapped_unlit.tint.green = 1.0;

	thrust_flare_material[4].texture_mapped_unlit.tint.red = 1.0;
	thrust_flare_material[4].texture_mapped_unlit.tint.blue = 1.0;
	thrust_flare_material[4].texture_mapped_unlit.tint.green = 0.8;

	material_init_texture_mapped_unlit(&warp_tunnel_material);
	warp_tunnel_material.texture_mapped_unlit.texture_id = load_texture("textures/warp-tunnel.png", 0);
	warp_tunnel_material.texture_mapped_unlit.do_cullface = 0;
	warp_tunnel_material.texture_mapped_unlit.do_blend = 1;
	warp_tunnel_material.texture_mapped_unlit.alpha = 0.25;

	material_init_texture_mapped(&block_material);
	block_material.texture_mapped.texture_id = load_texture("textures/spaceplate.png", 0);
	block_material.texture_mapped.emit_texture_id = load_texture("textures/spaceplateemit.png", 0);
	material_init_texture_mapped(&small_block_material);
	small_block_material.texture_mapped.texture_id = load_texture("textures/spaceplate_small.png", 0);
	small_block_material.texture_mapped.emit_texture_id = load_texture("textures/spaceplate_small_emit.png", 0);

	material_init_texture_mapped_unlit(&black_hole_material);
	black_hole_material.billboard_type = MATERIAL_BILLBOARD_TYPE_SPHERICAL;
	black_hole_material.texture_mapped_unlit.texture_id = load_texture("textures/black_hole.png", 0);
	black_hole_material.texture_mapped_unlit.do_blend = 1;
	black_hole_material.texture_mapped_unlit.do_cullface = 0;
	black_hole_material.texture_mapped_unlit.alpha = 1.0;

	material_init_texture_mapped(&spacemonster_tentacle_material);
	spacemonster_tentacle_material.texture_mapped.texture_id =
		load_texture("textures/spacemonster_tentacle_texture.png", 0);
	spacemonster_tentacle_material.texture_mapped.emit_texture_id =
		load_texture("textures/spacemonster_tentacle_emit.png", 0);

	material_init_texture_mapped(&missile_material);
	missile_material.texture_mapped.texture_id =
		load_texture("textures/missile_texture.png", 0);

	material_init_texture_mapped(&spacemonster_material);
	spacemonster_material.texture_mapped.texture_id =
		load_texture("textures/spacemonster_texture.png", 0);
	spacemonster_material.texture_mapped.emit_texture_id =
		load_texture("textures/spacemonster_emit.png", 0);
	material_init_texture_mapped(&warpgate_material);
	warpgate_material.texture_mapped.texture_id =
		load_texture("textures/warpgate_texture.png", 0);
	warpgate_material.texture_mapped.emit_texture_id =
		load_texture("textures/warpgate_emit.png", 0);
	material_init_warp_gate_effect(&warpgate_effect_material);
	warpgate_effect_material.warp_gate_effect.texture_id =
		warp_tunnel_material.texture_mapped_unlit.texture_id; /* share this texture */
	material_init_texture_mapped(&docking_port_material);
	docking_port_material.texture_mapped.texture_id =
		load_texture("textures/docking_port_texture.png", 0);
	docking_port_material.texture_mapped.emit_texture_id =
		load_texture("textures/docking_port_emit.png", 0);

	material_init_texture_mapped_unlit(&lens_flare_ghost_material);
	lens_flare_ghost_material.billboard_type = MATERIAL_BILLBOARD_TYPE_SPHERICAL;
	lens_flare_ghost_material.texture_mapped_unlit.texture_id = load_texture("textures/lens_flare_ghost.png", 0);
	lens_flare_ghost_material.texture_mapped_unlit.do_blend = 1;
	lens_flare_ghost_material.texture_mapped_unlit.alpha = 0.10;
	material_init_texture_mapped_unlit(&lens_flare_halo_material);
	lens_flare_halo_material.billboard_type = MATERIAL_BILLBOARD_TYPE_SPHERICAL;
	lens_flare_halo_material.texture_mapped_unlit.texture_id = load_texture("textures/lens_flare_halo.png", 0);
	lens_flare_halo_material.texture_mapped_unlit.do_blend = 1;
	lens_flare_halo_material.texture_mapped_unlit.alpha = 0.20;
	material_init_texture_mapped_unlit(&anamorphic_flare_material);
	anamorphic_flare_material.billboard_type = MATERIAL_BILLBOARD_TYPE_SPHERICAL;
	anamorphic_flare_material.texture_mapped_unlit.texture_id = load_texture("textures/anamorphic_flare.png", 0);
	anamorphic_flare_material.texture_mapped_unlit.do_blend = 1;
	anamorphic_flare_material.texture_mapped_unlit.alpha = 0.20;
	for (i = 0; i < NLIGHTNINGS; i++) {
		struct material *m = &planetary_lightning_material[i];
		material_init_planetary_lightning(m);
		if (i == 0)
			m->planetary_lightning.texture_id =
				load_texture("textures/planetary-lightning-texture.png", 1);
		else /* They all share the stame texture */
			m->planetary_lightning.texture_id =
				planetary_lightning_material[0].planetary_lightning.texture_id;
	}

	static_textures_loaded = 1;

	return 1;
}

static int load_per_solarsystem_textures()
{
	int i, j;
	char path[PATH_MAX];

	if (per_solarsystem_textures_loaded)
		return 0;
	if (strcmp(old_solarsystem_name, "") == 0)
		strcpy(old_solarsystem_name, DEFAULT_SOLAR_SYSTEM);
	reload_per_solarsystem_textures(old_solarsystem_name, solarsystem_name,
					old_solarsystem_assets, solarsystem_assets);
	fprintf(stderr, "load_per_solarsystem_textures, loading\n");
	if (!lua_skybox_prefix)
		snprintf(path, sizeof(path), "solarsystems/%s/%s",
			solarsystem_name, solarsystem_assets->skybox_prefix);
	else
		snprintf(path, sizeof(path), "textures/%s", lua_skybox_prefix);
	load_skybox_textures(path);

	material_init_texture_mapped_unlit(&sun_material);
	sun_material.billboard_type = MATERIAL_BILLBOARD_TYPE_SPHERICAL;
	snprintf(path, sizeof(path), "solarsystems/%s/%s", solarsystem_name, solarsystem_assets->sun_texture);
	sun_material.texture_mapped_unlit.texture_id = load_texture(path, 0);
	sun_material.texture_mapped_unlit.do_blend = 1;

	for (i = 0; i < solarsystem_assets->nplanet_textures; i++) {
		snprintf(path, sizeof(path), "solarsystems/%s/%s", solarsystem_name,
				solarsystem_assets->planet_texture[i]);
		material_init_textured_planet(&planet_material[i]);
		planet_material[i].textured_planet.texture_id = load_cubemap_textures(0, path, 0);
		planet_material[i].textured_planet.water_color_r = (float) solarsystem_assets->water_color[i].r / 255.0;
		planet_material[i].textured_planet.water_color_g = (float) solarsystem_assets->water_color[i].g / 255.0;
		planet_material[i].textured_planet.water_color_b = (float) solarsystem_assets->water_color[i].b / 255.0;
		planet_material[i].textured_planet.sun_color_r = (float) solarsystem_assets->sun_color.r / 255.0;
		planet_material[i].textured_planet.sun_color_g = (float) solarsystem_assets->sun_color.g / 255.0;
		planet_material[i].textured_planet.sun_color_b = (float) solarsystem_assets->sun_color.b / 255.0;
		if (i < NPLANET_MATERIALS / 2)
			planet_material[i].textured_planet.ring_material =
					&planetary_ring_material[i % NPLANETARY_RING_MATERIALS];
		else
			planet_material[i].textured_planet.ring_material = NULL;
		if (strcmp(solarsystem_assets->planet_normalmap[i], "no-normal-map") == 0) {
			planet_material[i].textured_planet.normalmap_id = 0;
		} else {
			snprintf(path, sizeof(path), "solarsystems/%s/%s", solarsystem_name,
					solarsystem_assets->planet_normalmap[i]);
			planet_material[i].textured_planet.normalmap_id = load_cubemap_textures(0, path, 1);
		}
	}
	j = 0;
	for (i = solarsystem_assets->nplanet_textures; i < NPLANET_MATERIALS; i++) {
		if (i == NPLANET_MATERIALS / 2)
			j = 0;
		planet_material[i] = planet_material[j];
		if (i < NPLANET_MATERIALS / 2)
			planet_material[i].textured_planet.ring_material =
					&planetary_ring_material[i % NPLANETARY_RING_MATERIALS];
		else
			planet_material[i].textured_planet.ring_material = NULL;
		j = (j + 1) % solarsystem_assets->nplanet_textures;
	}
	per_solarsystem_textures_loaded = 1;
	return 1;
}

static void expire_per_solarsystem_textures(char *old_solarsystem, struct solarsystem_asset_spec *assets)
{
	int i;
	char path[PATH_MAX];

	fprintf(stderr, "xxxxxxx asset_dir='%s', old_solarsystem='%s'\n", asset_dir, old_solarsystem);
	snprintf(path, sizeof(path), "solarsystems/%s/%s", old_solarsystem, assets->skybox_prefix);
	expire_skybox_texture(path);

	snprintf(path, sizeof(path), "%s/solarsystems/%s/%s", asset_dir, old_solarsystem, assets->sun_texture);
	graph_dev_expire_texture(path);

	for (i = 0; i < assets->nplanet_textures; i++) {
		snprintf(path, sizeof(path), "solarsystems/%s/%s", old_solarsystem, assets->planet_texture[i]);
		expire_cubemap_texture(0, path);
		if (strcmp(assets->planet_normalmap[i], "no-normal-map") != 0) {
			snprintf(path, sizeof(path), "solarsystems/%s/%s", old_solarsystem,
					assets->planet_normalmap[i]);
			expire_cubemap_texture(0, path);
		}
	}
}

static void reload_per_solarsystem_textures(char *old_solarsystem, char *new_solarsystem,
						struct solarsystem_asset_spec *old_assets,
						struct solarsystem_asset_spec *new_assets)
{
	fprintf(stderr, "Re-loading per solarsystem textures\n");
	if (old_assets) {
		expire_per_solarsystem_textures(old_solarsystem, old_assets);
		if (new_assets != old_assets)
			solarsystem_asset_spec_free(old_assets);
	}
	solarsystem_assets = new_assets;
	per_solarsystem_textures_loaded = 0;
	// load_per_solarsystem_textures();
}

static void load_textures(void)
{
	int loaded_something;
	loaded_something = load_static_textures();
	loaded_something += load_per_solarsystem_textures();
	(void) loaded_something; /* To suppress scan-build from complaining about dead stores */

#ifndef WITHOUTOPENGL
	if (loaded_something)
		glFinish();
#endif
}

static int main_da_button_press(GtkWidget *w, GdkEventButton *event,
	__attribute__((unused)) void *unused)
{
	if (event->button < 1 || event->button > 3)
		return TRUE;

	mouse.button[event->button - 1].state = MOUSE_BUTTON_PRESSED;
	gettimeofday(&mouse.button[event->button - 1].time_pressed, NULL);
	mouse.button[event->button - 1].press_x = event->x;
	mouse.button[event->button - 1].press_y = event->y;

	switch (displaymode) {
		case DISPLAYMODE_DEMON:
			demon_button_press(event->button, event->x, event->y);
			break;
		default:
			break;
	}
	ui_element_list_button_press(uiobjs, event->x, event->y);
	return TRUE;
}

static int main_da_button_release(GtkWidget *w, GdkEventButton *event,
	__attribute__((unused)) void *unused)
		
{
	if (event->button < 1 || event->button > 3)
		return TRUE;

	mouse.button[event->button - 1].state = MOUSE_BUTTON_RELEASED;
	mouse.button[event->button - 1].release_x = event->x;
	mouse.button[event->button - 1].release_y = event->y;

	if (display_frame_stats) {
		if (graph_dev_graph_dev_debug_menu_click(event->x, event->y))
			return TRUE;
	}

	if (in_the_process_of_quitting) {
		int sx = sng_pixelx_to_screenx(event->x);
		int sy = sng_pixely_to_screeny(event->y);
		if (sx > QUIT_BUTTON_X && sx < QUIT_BUTTON_X + QUIT_BUTTON_WIDTH &&
			sy > QUIT_BUTTON_Y && sy < QUIT_BUTTON_Y + QUIT_BUTTON_HEIGHT) {
			final_quit_selection = 1;
			in_the_process_of_quitting = 1;
		}
		if (sx > NOQUIT_BUTTON_X && sx < NOQUIT_BUTTON_X + QUIT_BUTTON_WIDTH &&
			sy > QUIT_BUTTON_Y && sy < QUIT_BUTTON_Y + QUIT_BUTTON_HEIGHT) {
			final_quit_selection = 0;
			in_the_process_of_quitting = 0;
		}
		return TRUE;
	}

	switch (displaymode) {
	case DISPLAYMODE_LOBBYSCREEN:
		lobbylast1clickx = sng_pixelx_to_screenx(event->x);
		lobbylast1clicky = sng_pixely_to_screeny(event->y);
		break;
	case DISPLAYMODE_SCIENCE:
		science_button_release(event->button, event->x, event->y);
		break;
	case DISPLAYMODE_DEMON:
		demon_button_release(event->button, event->x, event->y);
		break;
	case DISPLAYMODE_WEAPONS:
		if (event->button == 1)
			do_laser();
		else if (event->button == 3)
			fire_torpedo_button_pressed(NULL);
		else if ((event->button == 2) || (event->button == 4))
			fire_missile_button_pressed(NULL);
		break;
	case DISPLAYMODE_DAMCON:
		do_damcon_button_release(event->button, event->x, event->y);
		break;
	default:
		break;
	}
	ui_element_list_button_release(uiobjs, event->x, event->y);
	return TRUE;
}

static void smooth_mousexy(float x, float y, float *nx, float *ny)
{
	const float smoothness = 200.0f;
	const float interval = 1.0f / 30.0f;
	double d = 1 - exp(log(0.5) * smoothness * interval);
	static float smoothx = 0, smoothy = 0;

	smoothx += (x - smoothx) * d;
	smoothy += (y - smoothy) * d;
	*nx = smoothx;
	*ny = smoothy;
}

static int mouse_button_held(int button)
{
	int sx, sy;

	sx = (int) mouse.x;
	sy = (int) mouse.y;

	switch (displaymode) {
	case DISPLAYMODE_SCIENCE:
		science_button_held(button, sx, sy);
		break;
	default:
		break;
	}
	return TRUE;
}

static int main_da_motion_notify(GtkWidget *w, GdkEventMotion *event,
	__attribute__((unused)) void *unused)
{
	float pitch, yaw;
	float smoothx, smoothy;
	int sx, sy;

	mouse.x = event->x;
	mouse.y = event->y;
	if (current_mouse_ui_mode == MOUSE_MODE_CAPTURED_MOUSE) {
		int cx, cy, window_origin_x, window_origin_y;

		/* Find window origin in real screen coords */
		window_origin_x = event->x_root - event->x;
		window_origin_y = event->y_root - event->y;

		/* Find center of window in real screen coords */
		cx = window_origin_x + real_screen_width / 2;
		cy = window_origin_y + real_screen_height / 2;

		/* If the mouse is at the center of the screen, then do nothing,
		 * as this is due to events from us warping to the center of the screen.
		 */
		if (event->x_root == cx && event->y_root == cy) {
			mouse.deltax = 0;
			mouse.deltay = 0;
			mouse.x = event->x;
			mouse.y = event->y;
			return TRUE;
		}
		/* If the mouse is not at the center of the screen, then record the
		 * delta from the center and warp it to the center of the screen,
		 */
		mouse.deltax = event->x - real_screen_width / 2;
		mouse.deltay = event->y - real_screen_height / 2;
		/* I suspect this might have trouble with multi-monitor setups */
		gdk_display_warp_pointer(gdk_display_get_default(), gdk_screen_get_default(), cx, cy);
	}

	switch (displaymode) {
	case DISPLAYMODE_DEMON:
		demon_ui.ix2 = demon_mousex_to_ux(event->x);
		demon_ui.iz2 = demon_mousey_to_uz(event->y);
		break;
	case DISPLAYMODE_WEAPONS:
		/* FIXME: throttle this network traffic */
		if (current_mouse_ui_mode == MOUSE_MODE_FREE_MOUSE) {
			smooth_mousexy(event->x, event->y, &smoothx, &smoothy);
			yaw = weapons_mousex_to_yaw(smoothx);
			pitch = weapons_mousey_to_pitch(smoothy);
			queue_to_server(snis_opcode_pkt("bRR", OPCODE_REQUEST_WEAPONS_YAW_PITCH,
						yaw, pitch));
		} else { /* Captured mouse */
			int yaw, pitch, v;
			const int dead_threshold = 2;
			const int fine_threshold = 7;
			if (mouse.deltax < -dead_threshold) {
				yaw = YAW_LEFT + 2 * (mouse.deltax > -fine_threshold);
				request_weapons_manual_yaw_packet(yaw);
			} else if (mouse.deltax > dead_threshold) {
				yaw = YAW_RIGHT + 2 * (mouse.deltax < fine_threshold);
				request_weapons_manual_yaw_packet(yaw);
			}
			if (mouse.deltay < -dead_threshold) {
				v = vertical_controls_inverted < 0 ? PITCH_BACK : PITCH_FORWARD;
				pitch = v + 2 * (mouse.deltay > -fine_threshold);
				request_weapons_manual_pitch_packet(pitch);
			} else if (mouse.deltay > dead_threshold) {
				v = vertical_controls_inverted < 0 ? PITCH_FORWARD : PITCH_BACK;
				pitch = v + 2 * (mouse.deltay < fine_threshold);
				request_weapons_manual_pitch_packet(pitch);
			}
		}
		break;
	case DISPLAYMODE_ENGINEERING:
		sx = (int) event->x;
		sy = (int) event->y;
		if (snis_slider_mouse_inside(eng_ui.shield_damage, sx, sy))
			eng_ui.selected_subsystem = 0;
		else if (snis_slider_mouse_inside(eng_ui.impulse_damage, sx, sy))
			eng_ui.selected_subsystem = 1;
		else if (snis_slider_mouse_inside(eng_ui.warp_damage, sx, sy))
			eng_ui.selected_subsystem = 2;
		else if (snis_slider_mouse_inside(eng_ui.maneuvering_damage, sx, sy))
			eng_ui.selected_subsystem = 3;
		else if (snis_slider_mouse_inside(eng_ui.phaser_banks_damage, sx, sy))
			eng_ui.selected_subsystem = 4;
		else if (snis_slider_mouse_inside(eng_ui.sensors_damage, sx, sy))
			eng_ui.selected_subsystem = 5;
		else if (snis_slider_mouse_inside(eng_ui.comms_damage, sx, sy))
			eng_ui.selected_subsystem = 6;
		else if (snis_slider_mouse_inside(eng_ui.tractor_damage, sx, sy))
			eng_ui.selected_subsystem = 7;
		else if (snis_slider_mouse_inside(eng_ui.lifesupport_damage, sx, sy))
			eng_ui.selected_subsystem = 8;
		else
			eng_ui.selected_subsystem = -1;
		break;
	default:
		break;
	}
	return TRUE;
}

static gboolean delete_event(GtkWidget *widget, 
	GdkEvent *event, gpointer data)
{
    /* If you return FALSE in the "delete_event" signal handler,
     * GTK will emit the "destroy" signal. Returning TRUE means
     * you don't want the window to be destroyed.
     * This is useful for popping up 'are you sure you want to quit?'
     * type dialogs. */

    /* Change TRUE to FALSE and the main window will be destroyed with
     * a "delete_event". */
    gettimeofday(&end_time, NULL);
    printf("%d frames / %d seconds, %g frames/sec\n", 
		nframes, (int) (end_time.tv_sec - start_time.tv_sec),
		(0.0 + nframes) / (0.0 + end_time.tv_sec - start_time.tv_sec));
    return FALSE;
}

static void destroy(GtkWidget *widget, gpointer data)
{
    gtk_main_quit ();
}

static void really_quit(void)
{
	int i;
	float seconds;

	/* prevent divide by zero */
	if (nframes < 1)
		nframes = 1;
	if (netstats.elapsed_seconds < 1)
		netstats.elapsed_seconds = 1;

	gettimeofday(&end_time, NULL);

	seconds = (0.0 + end_time.tv_sec - start_time.tv_sec);
	if (seconds < 1.0)
		seconds = 1.0;

	printf("%d frames / %d seconds, %g frames/sec\n",
		nframes, (int) (end_time.tv_sec - start_time.tv_sec),
		(0.0 + nframes) / seconds);
	printf("server netstats: %"PRIu64" bytes sent, %"PRIu64" bytes recd, secs = %"PRIu64", bw = %"PRIu64" bytes/sec\n",
			netstats.bytes_sent, netstats.bytes_recd, (uint64_t) netstats.elapsed_seconds,
			(netstats.bytes_sent + netstats.bytes_recd) / netstats.elapsed_seconds);
        wwviaudio_cancel_all_sounds();
        wwviaudio_stop_portaudio();
	for (i = 0; i < njoysticks; i++)
		close_joystick(joystick_fd[i]);
	stop_text_to_speech_thread();
	exit(1); /* probably bad form... oh well. */
}

static void usage(void)
{
	fprintf(stderr, "usage: snis_client [--aspect-ratio x,y] [--monitor m ] --lobbyhost lobbyhost \\\n"
			"                   [ --solarsystem solarsystem ] --starship starshipname --pw password\n");
	fprintf(stderr, "       snis_client [--aspect-ratio x,y] [--monitor m ] [ --solarsystem solarsystem ]\\\n"
			"                    --nolobby --serverhost serverhost --serverport serverport\\\n"
			"                    --starship starshipname --pw password\n");
	fprintf(stderr, "       Example: ./snis_client --lobbyhost localhost --starship Enterprise --pw tribbles\n");
	fprintf(stderr, "Note: serverhost and serverport are mutually exclusive with lobbyhost\n");
	exit(1);
}

static void read_ogg_clip(int sound, char *directory, char *filename)
{
	char path[PATH_MAX];

	snprintf(path, sizeof(path), "%s/sounds/%s", directory, filename);
	wwviaudio_read_ogg_clip(sound, replacement_asset_lookup(path, &replacement_assets));
}

static int read_ship_types(void)
{
	char path[PATH_MAX];

	snprintf(path, sizeof(path), "%s/%s", asset_dir, "ship_types.txt");

	ship_type = snis_read_ship_types(path, &nshiptypes);
	if (!ship_type) {
		fprintf(stderr, "Unable to read ship types from %s", path);
		if (errno)
			fprintf(stderr, "%s: %s\n", path, strerror(errno));
		return -1;
	}
	return 0;
}

static int read_factions(void)
{
	char path[PATH_MAX];

	snprintf(path, sizeof(path), "%s/%s", asset_dir, "factions.txt");

	if (snis_read_factions(path)) {
		fprintf(stderr, "Unable to read factions from %s", path);
		if (errno)
			fprintf(stderr, "%s: %s\n", path, strerror(errno));
		return -1;
	}
	return 0;
}

static void read_sound_clips(void)
{
	char *d = asset_dir;

	printf("Decoding audio data..."); fflush(stdout);
	read_ogg_clip(EXPLOSION_SOUND, d, "big_explosion.ogg");
	read_ogg_clip(TORPEDO_LAUNCH_SOUND, d, "flak_hit.ogg");
	read_ogg_clip(LASER_FIRE_SOUND, d, "bigshotlaser.ogg");
	read_ogg_clip(ONSCREEN_SOUND, d, "onscreen.ogg");
	read_ogg_clip(OFFSCREEN_SOUND, d, "offscreen.ogg");
	read_ogg_clip(CHANGESCREEN_SOUND, d, "changescreen.ogg");
	read_ogg_clip(SLIDER_SOUND, d, "ui23.ogg");
	read_ogg_clip(SCIENCE_DATA_ACQUIRED_SOUND, d, "science-data-acquired.ogg");
	read_ogg_clip(SCIENCE_PROBE_SOUND, d, "science-probe.ogg");
	read_ogg_clip(TTY_CHATTER_SOUND, d, "tty-chatter.ogg");
	read_ogg_clip(WARPDRIVE_SOUND, d, "short-warpdrive.ogg");
	read_ogg_clip(TORPEDO_LOAD_SOUND, d, "torpedo-loading.ogg");
	read_ogg_clip(RED_ALERT_SOUND, d, "red-alert.ogg");
	read_ogg_clip(STARSHIP_JOINED, d, "new-starship.ogg");
	read_ogg_clip(CREWMEMBER_JOINED, d, "crewmember-has-joined.ogg");
	read_ogg_clip(HULL_BREACH_IMMINENT, d, "warning-hull-breach-imminent.ogg");
	read_ogg_clip(FUEL_LEVELS_CRITICAL, d, "fuel-levels-critical.ogg");
	read_ogg_clip(INCOMING_FIRE_DETECTED, d, "incoming-fire-detected.ogg");
	read_ogg_clip(LASER_FAILURE, d, "laser-fail.ogg");
	read_ogg_clip(PROXIMITY_ALERT, d, "proximity-alert.ogg");
	read_ogg_clip(SPACESHIP_CRASH, d, "spaceship-crash.ogg");
	read_ogg_clip(REVERSE_SOUND, d, "reverse.ogg");
	read_ogg_clip(COMMS_HAIL_SOUND, d, "comms-hail.ogg");
	read_ogg_clip(TRANSPORTER_SOUND, d, "transporter-sound.ogg");
	read_ogg_clip(DANGEROUS_RADIATION, d, "dangerous-radiation.ogg");
	read_ogg_clip(ENTERING_SECURE_AREA, d, "entering-high-security-area.ogg");
	read_ogg_clip(LEAVING_SECURE_AREA, d, "leaving-high-security-area.ogg");
	read_ogg_clip(ROBOT_INSERT_COMPONENT, d, "robot-insert-component.ogg");
	read_ogg_clip(ROBOT_REMOVE_COMPONENT, d, "robot-remove-component.ogg");
	read_ogg_clip(WARP_DRIVE_FUMBLE, d, "warp-drive-fumble.ogg");
	read_ogg_clip(ATMOSPHERIC_FRICTION, d, "atmospheric-friction.ogg");
	read_ogg_clip(DOCKING_SOUND, d, "docking-sound.ogg");
	read_ogg_clip(DOCKING_SYSTEM_ENGAGED, d, "docking-system-engaged.ogg");
	read_ogg_clip(DOCKING_SYSTEM_DISENGAGED, d, "docking-system-disengaged.ogg");
	read_ogg_clip(PERMISSION_TO_DOCK_GRANTED, d, "permission-to-dock-granted.ogg");
	read_ogg_clip(PERMISSION_TO_DOCK_DENIED, d, "permission-to-dock-denied.ogg");
	read_ogg_clip(PERMISSION_TO_DOCK_EXPIRED, d, "permission-to-dock-has-expired.ogg");
	read_ogg_clip(WELCOME_TO_STARBASE, d, "welcome-to-starbase.ogg");
	read_ogg_clip(MINING_BOT_DEPLOYED, d, "mining-bot-deployed.ogg");
	read_ogg_clip(MINING_BOT_STOWED, d, "mining-bot-stowed.ogg");
	read_ogg_clip(MINING_BOT_STANDING_BY, d, "mining-bot-standing-by.ogg");
	read_ogg_clip(HULL_CREAK_0, d, "hull-creak-0.ogg");
	read_ogg_clip(HULL_CREAK_1, d, "hull-creak-1.ogg");
	read_ogg_clip(HULL_CREAK_2, d, "hull-creak-2.ogg");
	read_ogg_clip(HULL_CREAK_3, d, "hull-creak-3.ogg");
	read_ogg_clip(HULL_CREAK_4, d, "hull-creak-4.ogg");
	read_ogg_clip(HULL_CREAK_5, d, "hull-creak-5.ogg");
	read_ogg_clip(HULL_CREAK_6, d, "hull-creak-6.ogg");
	read_ogg_clip(HULL_CREAK_7, d, "hull-creak-7.ogg");
	read_ogg_clip(HULL_CREAK_8, d, "hull-creak-8.ogg");
	read_ogg_clip(HULL_CREAK_9, d, "hull-creak-9.ogg");
	read_ogg_clip(UISND1, d, "ui1.ogg");
	read_ogg_clip(UISND2, d, "ui2.ogg");
	read_ogg_clip(UISND3, d, "ui3.ogg");
	read_ogg_clip(UISND4, d, "ui4.ogg");
	read_ogg_clip(UISND5, d, "ui5.ogg");
	read_ogg_clip(UISND6, d, "ui6.ogg");
	read_ogg_clip(UISND7, d, "ui7.ogg");
	read_ogg_clip(UISND8, d, "ui8.ogg");
	read_ogg_clip(UISND9, d, "ui9.ogg");
	read_ogg_clip(UISND10, d, "ui10.ogg");
	read_ogg_clip(UISND11, d, "ui11.ogg");
	read_ogg_clip(UISND12, d, "ui12.ogg");
	read_ogg_clip(UISND13, d, "ui13.ogg");
	read_ogg_clip(UISND14, d, "ui14.ogg");
	read_ogg_clip(UISND15, d, "ui15.ogg");
	read_ogg_clip(UISND16, d, "ui16.ogg");
	read_ogg_clip(UISND17, d, "ui17.ogg");
	read_ogg_clip(UISND18, d, "ui18.ogg");
	read_ogg_clip(UISND19, d, "ui19.ogg");
	read_ogg_clip(UISND20, d, "ui20.ogg");
	read_ogg_clip(UISND21, d, "ui21.ogg");
	read_ogg_clip(UISND22, d, "ui22.ogg");
	read_ogg_clip(UISND23, d, "ui23.ogg");
	read_ogg_clip(UISND24, d, "ui24.ogg");
	read_ogg_clip(UISND25, d, "ui25.ogg");
	read_ogg_clip(UISND26, d, "ui26.ogg");
	read_ogg_clip(UISND27, d, "ui27.ogg");
	read_ogg_clip(UISND28, d, "ui28.ogg");
	read_ogg_clip(UISND29, d, "ui29.ogg");
	read_ogg_clip(SPACEMONSTER_SLAP, d, "spacemonster_slap.ogg");
	read_ogg_clip(ALARM_BUZZER, d, "alarm_buzzer.ogg");
	read_ogg_clip(ROCKET_SAMPLE, d, "atlas_rocket_sample.ogg");
	read_ogg_clip(THRUSTER_SAMPLE, d, "maneuvering_thruster.ogg");
	read_ogg_clip(MISSILE_LAUNCH, d, "missile_launch.ogg");
	read_ogg_clip(TOO_FAR_AWAY, d, "too-far-away.ogg");
	read_ogg_clip(LOWER_SHIELDS_HIT_LIGHTS, d, "lower-shields-and-hit-lights.ogg");
	read_ogg_clip(CLEAR_TO_DEPART, d, "wombat-clear-to-depart.ogg");
	printf("Done.\n");
}

static void setup_sound(void)
{
	char *sound_device_str;
	int sound_device = -1;

	sound_device_str = getenv("SNIS_AUDIO_DEVICE");
	if (sound_device_str) {
		int rc = 0;

		rc = sscanf(sound_device_str, "%d", &sound_device);
		if (rc != 1)
			sound_device = -1;
	}
	if (sound_device > -1) {
		printf("Using manually selected sound device %d\n", sound_device);
		wwviaudio_set_sound_device(sound_device);
	}
	if (wwviaudio_initialize_portaudio(MAX_CONCURRENT_SOUNDS, NSOUND_CLIPS) != 0) {
		printf("Failed to initialize sound system\n");
		return;
	}
	wwviaudio_list_devices();
	read_sound_clips();
	snis_button_set_default_sound(UISND22);
}

static void check_for_screensaver(void)
{
	static char *cmd = "ps -efa | grep screensaver | grep -v grep >/dev/null 2>&1";
	if (system(cmd) != 0)
		return;

	/* if there were a sane, universal means to inhibit screensavers, */
	/* here is where it would go.  Instead, a message: */

	fprintf(stderr, "\n\n\n");
	fprintf(stderr, "  Screen saver and joystick detected.  Since joystick events\n");
	fprintf(stderr, "  aren't going through X11, there is a good chance your screen saver\n");
	fprintf(stderr, "  will unpleasantly interrupt your game.  There are several\n");
	fprintf(stderr, "  popular screen savers (xscreensaver, gnome-screensaver and\n");
	fprintf(stderr, "  kscreensaver being the three main ones).  Each of these\n");
	fprintf(stderr, "  has its own methods by which an application may temporarily\n");
	fprintf(stderr, "  inhibit it.  If the screen saver guys can come up with a sane\n");
	fprintf(stderr, "  method for an application to inhibit a screen saver to which\n");
	fprintf(stderr, "  they can all agree, then I might add such code to wordwarvi.\n");
	fprintf(stderr, "  As things currently stand, you should probably disable your\n");
	fprintf(stderr, "  screen saver manually, by whatever means they've given you,\n");
	fprintf(stderr, "  or at least be prepared to have your game interrupted by the\n");
	fprintf(stderr, "  screen saver from time to time.\n");
	fprintf(stderr, "\n  -- steve\n");
	fprintf(stderr, "\n\n\n");

	/* Another possibility might be to get joystick events through X somehow. */
	/* A quick google indicates there is such a thing as an Xorg X11 joystick */
	/* input driver.  However, getting at this through gtk might not be possible */
	/* and doing plain old X code is not something I want to mess with.  Also, */
	/* strongly suspect that this joystick driver isn't in most distros, and in*/
	/* none which I use.  So this is probably a dead end.  It would probably be */
	/* the cleanest solution in the long run, however, as then the joystick events */
	/* would inhibit the screen saver in precisely the same way that keyboard and */
	/* mouse events do. */

	/* Another possibility would be synthesizing events somehow.  There is a libXTst */
	/* for testing X which could probably be used to synthesize events and thus */
	/* possibly inhibit the screensaver, but I don't really know how to do it. */ 

	/* Then there is crap like what's described here: */
	/* http://www.jwz.org/xscreensaver/faq.html#dvd */
	/* which advocates doing this, triggered by a timer,  while asserting */
	/* that it's fast enough:
 
		if (playing && !paused) {
			system ("xscreensaver-command -deactivate >&- 2>&- &");
		}  */

	/* I should probably cut jwz some slack though, because he's jwz, and because */
	/* he did a lot of work on xemacs.  And it may be fast enough, for all I know */
	/* but I'm not interested in a screen saver specific solution.  This just papers */
	/* over the current mess and makes it seem ok, when it isn't ok. */

	/* Similarly for gnome-screensaver there is a "gnome-screensaver-command --poke"  */
	/* command.  There's quite likely something similar (but different) for kscreensaver. */
	/* which I can't be bothered to look up, on account of I hate KDE, because it */
	/* seems designed by insane people who actually *like* the way the Windows UI works. */

	/* All of that is of course bullshit up with which I will not put.  There needs to be */ 
	/* a universal, screen saver agnostic, sane way that doesn't involve starting a new process. */
	/* Nothing less is acceptable. */

	/* Getting joystick events through X and gtk is probably the */
	/* correct answer (and currently impossible, AFAICT, or at least very */
	/* impractical) */
}


static void process_physical_device_io(unsigned short opcode, unsigned short value)
{
	double d;

	d = (double) value / (double) ((unsigned short) 0xFFFF);

	gdk_threads_enter();
	switch (opcode) {
	case DEVIO_OPCODE_ENG_PWR_SHIELDS:
		snis_slider_poke_input(eng_ui.shield_slider, d, 1);
		break;
	case DEVIO_OPCODE_ENG_PWR_PHASERS:
		snis_slider_poke_input(eng_ui.phaserbanks_slider, d, 1);
		break;
	case DEVIO_OPCODE_ENG_PWR_COMMS	:
		snis_slider_poke_input(eng_ui.comm_slider, d, 1);
		break;
	case DEVIO_OPCODE_ENG_PWR_SENSORS:
		snis_slider_poke_input(eng_ui.sensors_slider, d, 1);
		break;
	case DEVIO_OPCODE_ENG_PWR_IMPULSE:
		snis_slider_poke_input(eng_ui.impulse_slider, d, 1);
		break;
	case DEVIO_OPCODE_ENG_PWR_WARP:
		snis_slider_poke_input(eng_ui.warp_slider, d, 1);
		break;
	case DEVIO_OPCODE_ENG_PWR_MANEUVERING:
		snis_slider_poke_input(eng_ui.maneuvering_slider, d, 1);
		break;
	case DEVIO_OPCODE_ENG_PWR_TRACTOR:
		snis_slider_poke_input(eng_ui.tractor_slider, d, 1);
		break;
	case DEVIO_OPCODE_ENG_PWR_LIFESUPPORT:
		snis_slider_poke_input(eng_ui.lifesupport_slider, d, 1);
		break;
	case DEVIO_OPCODE_ENG_COOL_SHIELDS:
		snis_slider_poke_input(eng_ui.shield_coolant_slider, d, 1);
		break;
	case DEVIO_OPCODE_ENG_COOL_PHASERS:
		snis_slider_poke_input(eng_ui.phaserbanks_coolant_slider, d, 1);
		break;
	case DEVIO_OPCODE_ENG_COOL_COMMS:
		snis_slider_poke_input(eng_ui.comm_coolant_slider, d, 1);
		break;
	case DEVIO_OPCODE_ENG_COOL_SENSORS:
		snis_slider_poke_input(eng_ui.sensors_coolant_slider, d, 1);
		break;
	case DEVIO_OPCODE_ENG_COOL_IMPULSE:
		snis_slider_poke_input(eng_ui.impulse_coolant_slider, d, 1);
		break;
	case DEVIO_OPCODE_ENG_COOL_WARP:
		snis_slider_poke_input(eng_ui.warp_coolant_slider, d, 1);
		break;
	case DEVIO_OPCODE_ENG_COOL_MANEUVERING:
		snis_slider_poke_input(eng_ui.maneuvering_coolant_slider, d, 1);
		break;
	case DEVIO_OPCODE_ENG_COOL_TRACTOR:
		snis_slider_poke_input(eng_ui.tractor_coolant_slider, d, 1);
		break;
	case DEVIO_OPCODE_ENG_COOL_LIFESUPPORT:
		snis_slider_poke_input(eng_ui.lifesupport_coolant_slider, d, 1);
		break;
	case DEVIO_OPCODE_ENG_SHIELD_LEVEL:
		snis_slider_poke_input(eng_ui.shield_control_slider, d, 1);
		break;
	case DEVIO_OPCODE_ENG_PRESET:
		if ((value >= 0) && (value < ENG_PRESET_NUMBER))
			snis_button_trigger_button(eng_ui.preset_buttons[value]);
		break;
	case DEVIO_OPCODE_ENG_PRESET_SAVE:
		snis_button_trigger_button(eng_ui.preset_save_button);
		break;
	case DEVIO_OPCODE_ENG_DAMAGE_CTRL:
		damcon_button_pressed((void *) 0);
		break;
	case DEVIO_OPCODE_ENG_SILENCE_ALARMS:
		silence_alarms_pressed((void *) 0);
		break;
	case DEVIO_OPCODE_ENG_FLARE:
		eng_deploy_flare_button_pressed((void *) 0);
		break;
	case DEVIO_OPCODE_NAV_YAW_LEFT:
		navigation_dirkey(-1, 0, 0);
		break;
	case DEVIO_OPCODE_NAV_YAW_RIGHT:
		navigation_dirkey(1, 0, 0);
		break;
	case DEVIO_OPCODE_NAV_PITCH_DOWN:
		navigation_dirkey(0, 1, 0);
		break;
	case DEVIO_OPCODE_NAV_PITCH_UP:
		navigation_dirkey(0, -1, 0);
		break;
	case DEVIO_OPCODE_NAV_ROLL_LEFT:
		navigation_dirkey(0, 0, -1);
		break;
	case DEVIO_OPCODE_NAV_ROLL_RIGHT:
		navigation_dirkey(0, 0, 1);
		break;
	case DEVIO_OPCODE_NAV_REVERSE:
		reverse_button_pressed((void *) 0);
		break;
	case DEVIO_OPCODE_NAV_ZOOM:
		snis_slider_poke_input(nav_ui.navzoom_slider, d, 1);
		break;
	case DEVIO_OPCODE_NAV_WARP_POWER:
		snis_slider_poke_input(nav_ui.warp_slider, d, 1);
		break;
	case DEVIO_OPCODE_NAV_WARP_ENGAGE:
		engage_warp_button_pressed((void *) 0);
		break;
	case DEVIO_OPCODE_NAV_DOCKING_MAGNETS:
		docking_magnets_button_pressed((void *) 0);
		break;
	case DEVIO_OPCODE_NAV_STANDARD_ORBIT:
		standard_orbit_button_pressed((void *) 0);
		break;
	case DEVIO_OPCODE_NAV_STARMAP:
		nav_starmap_button_pressed((void *) 0);
		break;
	case DEVIO_OPCODE_NAV_LIGHTS:
		nav_lights_button_pressed((void *) 0);
		break;
	case DEVIO_OPCODE_NAV_THROTTLE:
		snis_slider_poke_input(nav_ui.throttle_slider, d, 1);
		break;
	case DEVIO_OPCODE_WEAPONS_YAW_LEFT:
		weapons_dirkey(-1, 0);
		break;
	case DEVIO_OPCODE_WEAPONS_YAW_RIGHT:
		weapons_dirkey(1, 0);
		break;
	case DEVIO_OPCODE_WEAPONS_PITCH_UP:
		weapons_dirkey(0, -1);
		break;
	case DEVIO_OPCODE_WEAPONS_PITCH_DOWN:
		weapons_dirkey(0, 1);
		break;
	case DEVIO_OPCODE_WEAPONS_FIRE_PHASERS:
		fire_phaser_button_pressed(NULL);
		break;
	case DEVIO_OPCODE_WEAPONS_FIRE_TORPEDO:
		fire_torpedo_button_pressed(NULL);
		break;
	case DEVIO_OPCODE_WEAPONS_FIRE_MISSILE:
		fire_missile_button_pressed(NULL);
		break;
	case DEVIO_OPCODE_WEAPONS_WAVELENGTH:
		snis_slider_poke_input(weapons.wavelen_slider, d, 1);
		break;
	case DEVIO_OPCODE_DMGCTRL_LEFT:
		robot_left_button_pressed((void *) 0);
		break;
	case DEVIO_OPCODE_DMGCTRL_RIGHT:
		robot_right_button_pressed((void *) 0);
		break;
	case DEVIO_OPCODE_DMGCTRL_FORWARD:
		robot_forward_button_pressed((void *) 0);
		break;
	case DEVIO_OPCODE_DMGCTRL_BACKWARD:
		robot_backward_button_pressed((void *) 0);
		break;
	case DEVIO_OPCODE_DMGCTRL_GRIPPER:
		robot_gripper_button_pressed((void *) 0);
		break;
	case DEVIO_OPCODE_DMGCTRL_AUTO:
		robot_auto_button_pressed((void *) 0);
		break;
	case DEVIO_OPCODE_DMGCTRL_MANUAL:
		robot_manual_button_pressed((void *) 0);
		break;
	case DEVIO_OPCODE_DMGCTRL_ENGINEERING:
		main_engineering_button_pressed((void *) 0);
		break;
	case DEVIO_OPCODE_DMGCTRL_EJECT_WARP_CORE_BUTTON:
		eject_warp_core_button_pressed((void *) 0);
		break;
	case DEVIO_OPCODE_SCIENCE_RANGE:
		snis_slider_poke_input(sci_ui.scizoom, d, 1);
		break;
	case DEVIO_OPCODE_SCIENCE_TRACTOR:
		sci_tractor_pressed((void *) 0);
		break;
	case DEVIO_OPCODE_SCIENCE_LAUNCH_MINING_BOT:
		sci_mining_bot_pressed((void *) 0);
		break;
	case DEVIO_OPCODE_SCIENCE_SRS:
		sci_sciplane_pressed((void *) 0);
		break;
	case DEVIO_OPCODE_SCIENCE_ALIGN_TO_SHIP:
		sci_align_to_ship_pressed((void *) 0);
		break;
	case DEVIO_OPCODE_SCIENCE_LRS:
		sci_threed_pressed((void *) 0);
		break;
	case DEVIO_OPCODE_SCIENCE_DETAILS:
		sci_details_pressed((void *) 0);
		break;
	case DEVIO_OPCODE_SCIENCE_WAYPOINTS:
		sci_waypoints_pressed((void *) 0);
		break;
	case DEVIO_OPCODE_SELECT_WAYPOINT:
		if (value >= 0 && value < sci_ui.nwaypoints && value < MAXWAYPOINTS)
			science_select_waypoint_pressed(&sci_ui.select_waypoint_button[value]);
		break;
	case DEVIO_OPCODE_CURRPOS_WAYPOINT:
		science_add_current_pos_pressed(NULL);
		break;
	case DEVIO_OPCODE_CLEAR_WAYPOINT:
		if (value >= 0 && value < sci_ui.nwaypoints && value < MAXWAYPOINTS)
			science_clear_waypoint_pressed(&sci_ui.clear_waypoint_button[value]);
		break;
	case DEVIO_OPCODE_COMMS_COMMS_ONSCREEN:
		comms_screen_button_pressed((void *) 0);
		break;
	case DEVIO_OPCODE_COMMS_NAV_ONSCREEN:
		comms_screen_button_pressed((void *) 1);
		break;
	case DEVIO_OPCODE_COMMS_WEAP_ONSCREEN:
		comms_screen_button_pressed((void *) 2);
		break;
	case DEVIO_OPCODE_COMMS_ENG_ONSCREEN:
		comms_screen_button_pressed((void *) 3);
		break;
	case DEVIO_OPCODE_COMMS_DAMCON_ONSCREEN:
		comms_screen_button_pressed((void *) 4);
		break;
	case DEVIO_OPCODE_COMMS_SCI_ONSCREEN:
		comms_screen_button_pressed((void *) 5);
		break;
	case DEVIO_OPCODE_COMMS_MAIN_ONSCREEN:
		comms_screen_button_pressed((void *) 6);
		break;
	case DEVIO_OPCODE_COMMS_TRANSMIT:
		comms_transmit_button_pressed(NULL);
		break;
	case DEVIO_OPCODE_COMMS_RED_ALERT:
		comms_screen_red_alert_pressed(NULL);
		break;
	case DEVIO_OPCODE_COMMS_HAIL_MINING_BOT:
		comms_hail_mining_bot_pressed(NULL);
		break;
	case DEVIO_OPCODE_COMMS_MAINSCREEN_COMMS:
		comms_main_screen_pressed(NULL);
		break;
	case DEVIO_OPCODE_COMMS_MAINZOOM:
		snis_slider_poke_input(comms_ui.mainzoom_slider, d, 1);
		break;
	}
	gdk_threads_leave();
}

static void *monitor_physical_io_devices(__attribute__((unused)) void *arg)
{
	socklen_t len;
	int bytecount;
	struct sockaddr_un client_addr;
	unsigned short opcode, value;

#define PHYS_IO_BUF_SIZE (sizeof(unsigned short) * 2)
	char buf[PHYS_IO_BUF_SIZE];

	for (;;) {
		len = sizeof(client_addr);
		bytecount = recvfrom(physical_io_socket, buf, sizeof(buf), 0,
				(struct sockaddr *) &client_addr, &len);
		if (bytecount < 0) {
			fprintf(stderr, "recvfrom returned %d (%s) in %s\n",
				bytecount, strerror(errno), __func__); 
			fprintf(stderr, "Physical io device monitor thread exiting\n");
			return NULL;
		}
#if 0
		fprintf(stderr, "phys io monitor recvd %d bytes\n", bytecount);
		for (int i = 0; i < bytecount; i++) {
			fprintf(stderr, "%02hhu ", buf[i]);
		}
		fprintf(stderr, "\n");
#endif
		if (bytecount != sizeof(unsigned short) * 2) {
			fprintf(stderr, "unexpected bytecount from physical device\n");
			return NULL;
		}
		memcpy(&opcode, &buf[0], sizeof(opcode));
		memcpy(&value, &buf[2], sizeof(value));
		process_physical_device_io(ntohs(opcode), ntohs(value));
	}
	return NULL;
}

static void setup_physical_io_socket(void)
{
#ifdef __linux
	struct sockaddr_un addr;
	int rc;

	memset(&addr, 0, sizeof(addr));
	addr.sun_family = AF_UNIX;

	/* Note: addr.sun_path[0] is set to 0 by above memset.  Setting the
	 * name starting at addr.sun_path[1], below -- after the initial 0 byte --
	 * is a convention to set up a name in the linux-specific abstract socket
	 * name space.  See "The Linux Programming Interface", by Michael Kerrisk,
	 * Ch. 57, p. 1175-1176.  To get this to work on non-linux, we'll need
	 * to do something else.
	 */
	strncpy(&addr.sun_path[1], "snis-phys-io", sizeof(addr.sun_path) - 2);

	physical_io_socket = socket(AF_UNIX, SOCK_DGRAM, 0);
	if (physical_io_socket == -1) {
		fprintf(stderr,
			"Failed creating physical io socket, physical input devices will not work.\n");
		return;
	}
	if (bind(physical_io_socket, (struct sockaddr *) &addr,
			sizeof(struct sockaddr_un)) == -1) {
		fprintf(stderr, "Failed to bind to linux abstract socket address 'snis-phys-io'.\n");
		fprintf(stderr, "Perhaps another snis_client process is running.\n");
		/* FIXME: is this how to deal with the socket at this point? */
		physical_io_socket = -1;
	} else {
		rc = create_thread(&physical_io_thread,
			monitor_physical_io_devices, NULL, "snisc-devio", 0);
		if (rc) {
			fprintf(stderr, "Failed to create physical device monitor thread.\n");
			fprintf(stderr, "Physical input devices will not work.\n");
			close(physical_io_socket);
			physical_io_socket = -1;
		}
	}
#endif
}

struct input_fifo_thread_data {
	char fifoname[PATH_MAX];
	void (*process_input)(char *);
	char threadname[20];
};

static void *monitor_input_fifo(void *arg)
{
	struct input_fifo_thread_data *d = arg;
	char *rc, line[256];
	FILE *f;

	do {
		f = fopen(d->fifoname, "r");
		if (!f) {
			fprintf(stderr, "snis_client: Failed to open '%s': %s\n",
				d->fifoname, strerror(errno));
			break;
		}

		do {
			rc = fgets(line, 255, f);
			if (!rc)
				break;
			trim_whitespace(line);
			clean_spaces(line);
			if (strcmp(line, "") == 0) /* skip blank lines */
				continue;
			d->process_input(line);
		} while (1);
		fclose(f);
	} while (1);
	fprintf(stderr, "snis_client: %s thread exiting.\n", d->threadname);
	free(d);
	return NULL;
}

static void setup_input_fifo(char *fifoname, char *threadname,
				pthread_t *thread, void (process_input)(char *))
{
	int rc;

	struct input_fifo_thread_data *d;

	d = malloc(sizeof(*d));
	strcpy(d->fifoname, fifoname);
	strcpy(d->threadname, threadname);
	d->process_input = process_input;

	rc = mkfifo(fifoname, 0644);
	if (rc != 0 && errno != EEXIST) {
		fprintf(stderr, "snis_client: mkfifo(%s): %s\n", fifoname, strerror(errno));
	}
	rc = create_thread(thread, monitor_input_fifo, d, threadname, 0);
	if (rc)
		fprintf(stderr, "Failed to create %s fifo monitor thread.\n", threadname);
}

#define SNIS_NL_FIFO "/tmp/snis-natural-language-fifo"
static void setup_natural_language_fifo(void)
{
	setup_input_fifo(SNIS_NL_FIFO, "snis-nl-fifo", &natural_language_thread,
				send_natural_language_request_to_server);
}

#define SNIS_DEMON_FIFO "/tmp/snis-demon-fifo"
static void setup_demon_fifo(void)
{
	setup_input_fifo(SNIS_DEMON_FIFO, "snisdemonfifo", &demon_fifo_thread,
				send_demon_text_command);
}

static void setup_joysticks(GtkWidget *window)
{
	int rc, i;
	char *joystick_name[MAX_JOYSTICKS];
	char joystick_config_file[PATH_MAX];

	rc = discover_joysticks(&joysticks_found, &njoysticks);
	if (rc)
		return;

	for (i = 0; i < njoysticks; i++) {
		joystick_name[i] = strdup(joysticks_found[i].name);
		snprintf(joystick_device[i], PATH_MAX, "/dev/input/by-id/%s", joysticks_found[i].name);
		joystick_fd[i] = open_joystick(joystick_device[i], window->window);
		if (joystick_fd[i] < 0) {
			printf("Cannot open joystick %d (%s): %s. Ignoring.\n",
				i, joystick_device[i], strerror(errno));
		} else {
			/* pull all the events off the joystick */
			memset(&jss[i].button, 0, sizeof(jss[i].button));
			memset(&jss[i].axis, 0, sizeof(jss[i].axis));
			int rc = get_joystick_state(joystick_fd[i], &jss[i]);
			if (rc != 0) {
				printf("Joystick '%s' not sending events, ignoring...\n", joysticks_found[i].name);
				close_joystick(joystick_fd[i]);
				joystick_fd[i] = -1;
				return;
			}
		}
	}
	/* Set up joystick callbacks according to config file */
	joystick_cfg = new_joystick_config();
	set_joystick_axis_fn(joystick_cfg, "yaw", do_joystick_yaw);
	set_joystick_axis_fn(joystick_cfg, "roll", do_joystick_roll);
	set_joystick_axis_fn(joystick_cfg, "pitch", do_joystick_pitch);
	set_joystick_button_fn(joystick_cfg, "phaser", fire_phaser_button_pressed);
	set_joystick_button_fn(joystick_cfg, "torpedo", do_joystick_torpedo);
	set_joystick_button_fn(joystick_cfg, "missile", do_joystick_missile);
	set_joystick_axis_fn(joystick_cfg, "weapons-yaw", do_joystick_weapons_yaw);
	set_joystick_axis_fn(joystick_cfg, "weapons-pitch", do_joystick_weapons_pitch);
	set_joystick_axis_fn(joystick_cfg, "damcon-pitch", do_joystick_damcon_pitch);
	set_joystick_axis_fn(joystick_cfg, "damcon-roll", do_joystick_damcon_roll);
	set_joystick_axis_fn(joystick_cfg, "throttle", do_joystick_throttle);
	set_joystick_axis_fn(joystick_cfg, "warp", do_joystick_warp);
	set_joystick_axis_fn(joystick_cfg, "weapons-wavelength", do_joystick_weapons_wavelength);
	set_joystick_button_fn(joystick_cfg, "damcon-gripper", robot_gripper_button_pressed);
	set_joystick_button_fn(joystick_cfg, "nav-engage-warp", do_joystick_engage_warp);
	set_joystick_button_fn(joystick_cfg, "nav-standard-orbit", do_joystick_standard_orbit);
	set_joystick_button_fn(joystick_cfg, "nav-docking-magnets", do_joystick_docking_magnets);
	set_joystick_button_fn(joystick_cfg, "nav-attitude-indicator-abs-rel", do_joystick_attitude_indicator);
	set_joystick_button_fn(joystick_cfg, "nav-starmap", do_joystick_starmap);
	set_joystick_button_fn(joystick_cfg, "nav-reverse", do_joystick_reverse);
	set_joystick_button_fn(joystick_cfg, "nav-lights", do_joystick_lights);
	set_joystick_button_fn(joystick_cfg, "nav-nudge-warp-up", do_joystick_nudge_warp_up);
	set_joystick_button_fn(joystick_cfg, "nav-nudge-warp-down", do_joystick_nudge_warp_down);
	set_joystick_button_fn(joystick_cfg, "nav-nudge-zoom-up", do_joystick_nav_nudge_zoom_up);
	set_joystick_button_fn(joystick_cfg, "nav-nudge-zoom-down", do_joystick_nav_nudge_zoom_down);
	set_joystick_button_fn(joystick_cfg, "weapons-wavelength-up", do_joystick_weapons_wavelength_up);
	set_joystick_button_fn(joystick_cfg, "weapons-wavelength-down", do_joystick_weapons_wavelength_down);
	set_joystick_button_fn(joystick_cfg, "nav-change-pov", do_joystick_nav_change_pov);
	snprintf(joystick_config_file, sizeof(joystick_config_file), "%s/joystick_config.txt", asset_dir);
	read_joystick_config(joystick_cfg, joystick_config_file, joystick_name, njoysticks);

	if (njoysticks > 0)
		check_for_screensaver();
}

static struct mesh *snis_read_model(char *directory, char *filename)
{
	char path[PATH_MAX];
	struct mesh *m;

	snprintf(path, sizeof(path), "%s/models/%s", directory, filename);
	m = read_mesh(replacement_asset_lookup(path, &replacement_assets));
	if (!m) {
		printf("Failed to read model from file '%s'\n", path);
		printf("Assume form of . . . A SPHERICAL COW!\n");
		m = mesh_unit_spherified_cube(8);
		if (!m)
			printf("...or possibly a spherical cow dump!\n");
		else
			mesh_scale(m, 20.0f);
	}
	return m;
}

static struct mesh *make_derelict_mesh(struct mesh *source)
{
	struct mesh *m = mesh_duplicate(source);

	mesh_derelict(m, 10.0);
	return m;
}

static void init_meshes()
{
	int i;
	char *d = asset_dir;
	struct mtwist_state *mt; 
	char missile_thrust_attachment_path[PATH_MAX];
	struct osn_context *osn;

	mt = mtwist_init(mtwist_seed);
	if (!mt) {
		fprintf(stderr, "out of memory at %s:%d... crash likely.\n",
				__FILE__, __LINE__);
		fflush(stderr);
		return;
	}

	turret_mesh = snis_read_model(d, "laser_turret.stl");
	turret_base_mesh = snis_read_model(d, "laser_turret_base.stl");
	ship_turret_mesh = snis_read_model(d, "spaceship_turret.stl");
	ship_turret_base_mesh = snis_read_model(d, "spaceship_turret_base.stl");
	mesh_scale(ship_turret_mesh, SHIP_MESH_SCALE);
	mesh_scale(ship_turret_base_mesh, SHIP_MESH_SCALE);
	torpedo_nav_mesh = snis_read_model(d, "torpedo.stl");
	missile_mesh = snis_read_model(d, "missile.stl");
	mesh_scale(missile_mesh, SHIP_MESH_SCALE);
	mesh_cylindrical_yz_uv_map(missile_mesh);
	snprintf(missile_thrust_attachment_path, sizeof(missile_thrust_attachment_path),
						"%s/models/missile.scad_params.h", d);
	missile_thrust_attachment_point =
		read_thrust_attachments(missile_thrust_attachment_path, SHIP_MESH_SCALE);
#ifndef WITHOUTOPENGL
	torpedo_mesh = mesh_fabricate_billboard(50.0f, 50.0f);
#else
	torpedo_mesh = torpedo_nav_mesh;
#endif
	laser_mesh = snis_read_model(d, "laser.stl");

	open_simplex_noise(77374223LL, &osn);
	for (i = 0; i < NASTEROID_MODELS; i++) {
		char filename[100];

		if (i == 0)
			snprintf(filename, sizeof(filename), "asteroid.stl");
		else
			snprintf(filename, sizeof(filename), "asteroid%d.stl", i + 1);
		printf("reading '%s'\n", filename);
		asteroid_mesh[i] = snis_read_model(d, filename);
		mesh_distort(asteroid_mesh[i], 0.25, osn);
		mesh_set_average_vertex_normals(asteroid_mesh[i]);
		mesh_graph_dev_init(asteroid_mesh[i]);
	}

	unit_cube_mesh = mesh_unit_cube(2);
	mesh_scale(unit_cube_mesh, 0.5);
	mesh_unit_cube_uv_map(unit_cube_mesh);

#ifndef WITHOUTOPENGL
	atmosphere_mesh = mesh_unit_spherified_cube(64);
	planet_sphere_mesh = mesh_unit_spherified_cube(64);
#else
	atmosphere_mesh = mesh_unit_spherified_cube(16);
	planet_sphere_mesh = mesh_unit_spherified_cube(16);
#endif
	atmosphere_lp_mesh = mesh_unit_spherified_cube(8);
	planet_sphere_lp_mesh = mesh_unit_spherified_cube(8);
	low_poly_sphere_mesh = snis_read_model(d, "uv_sphere.stl");
	mesh_cylindrical_xy_uv_map(low_poly_sphere_mesh);
	cylindrically_mapped_sphere_mesh = mesh_duplicate(low_poly_sphere_mesh);
	mesh_cylindrical_xy_uv_map(cylindrically_mapped_sphere_mesh);
	warp_tunnel_mesh = mesh_tube(XKNOWN_DIM, 450.0, 20);
	nav_axes_mesh = mesh_fabricate_axes();
	mesh_scale(nav_axes_mesh, SNIS_WARP_GATE_THRESHOLD * 0.05);
	demon3d_axes_mesh = mesh_fabricate_axes();
	mesh_scale(demon3d_axes_mesh, 0.002 * XKNOWN_DIM);
	planetary_ring_mesh = mesh_fabricate_planetary_ring(MIN_RING_RADIUS, MAX_RING_RADIUS, 360);
	planetary_ring_lp_mesh = mesh_fabricate_planetary_ring(MIN_RING_RADIUS, MAX_RING_RADIUS, 64);
	nav_planetary_ring_mesh = mesh_fabricate_planetary_ring(MIN_RING_RADIUS * 1.5, MAX_RING_RADIUS * 0.75, 36);

	for (i = 0; i < nstarbase_models; i++) {
		char *filename = starbase_metadata[i].model_file;
		printf("reading starbase model %d of %d '%s'\n", i + 1, nstarbase_models, filename);
		starbase_mesh[i] = snis_read_model(d, filename);
		mesh_scale(starbase_mesh[i], STARBASE_SCALE_FACTOR);
	}

	allocate_ship_thrust_attachment_points(nshiptypes);
	for (i = 0; i < nshiptypes; i++) {
		ship_mesh_map[i] = snis_read_model(d, ship_type[i].model_file);
		read_thrust_attachment_points(d, ship_type[i].model_file, i,
						&ship_thrust_attachment_points[i],
						ship_type[i].extra_scaling);
		for (int j = 0; j < ship_type[i].nrotations; j++) {
			char axis = ship_type[i].axis[j];
			union quat q;
			if (axis == 's') { /* scale, not rotate */
				mesh_scale(ship_mesh_map[i], ship_type[i].angle[j]);
				continue;
			}
			quat_init_axis(&q, (float) ('x' == axis),
					   (float) ('y' == axis),
					   (float) ('z' == axis), ship_type[i].angle[j]);
			mesh_rotate(ship_mesh_map[i], &q);
		}
		mesh_scale(ship_mesh_map[i], SHIP_MESH_SCALE * ship_type[i].extra_scaling);
		derelict_mesh[i] = make_derelict_mesh(ship_mesh_map[i]);
	}

#ifndef WITHOUTOPENGL
	particle_mesh = mesh_fabricate_billboard(50.0f, 50.0f);
#else
	particle_mesh = snis_read_model(d, "tetrahedron.stl");
#endif
	debris_mesh = snis_read_model(d, "flat-tetrahedron.stl");
	debris2_mesh = snis_read_model(d, "big-flat-tetrahedron.stl");
	wormhole_mesh = snis_read_model(d, "wormhole.stl");
	mesh_map_xy_to_uv(wormhole_mesh);
#ifdef WITHOUTOPENGL
	mesh_distort(wormhole_mesh, 0.25, osn);
	wormhole_mesh->geometry_mode = MESH_GEOMETRY_POINTS;
#else
	mesh_scale(wormhole_mesh, 3.0f);
#endif
	spacemonster_mesh = snis_read_model(d, "space_monster_torso.stl");
	mesh_cylindrical_yz_uv_map(spacemonster_mesh);
	tentacle_segment_mesh = snis_read_model(d, "space_monster_tentacle_segment.stl");
	mesh_cylindrical_yz_uv_map(tentacle_segment_mesh);

	laserbeam_nav_mesh = snis_read_model(d, "long-triangular-prism.stl");
#ifndef WITHOUTOPENGL
	laserbeam_mesh = mesh_fabricate_billboard(1.0, 1.0);
	phaser_mesh = init_burst_rod_mesh(1000, LASER_VELOCITY * 2 / 3, 0.5, 0.5);
#else
	laserbeam_mesh = laserbeam_nav_mesh;
	phaser_mesh = laserbeam_nav_mesh;
#endif
	ship_icon_mesh = snis_read_model(d, "ship-icon.stl");
	heading_indicator_mesh = snis_read_model(d, "heading_indicator.stl");
	heading_indicator_tail_mesh = snis_read_model(d, "heading_indicator_tail.stl");
	cargo_container_mesh = snis_read_model(d, "cargocontainer/cargocontainer.obj");
	docking_port_mesh[0] = snis_read_model(d, "docking_port.stl");
	mesh_cylindrical_yz_uv_map(docking_port_mesh[0]);
	docking_port_mesh[1] = snis_read_model(d, "docking_port2.stl");
	mesh_cylindrical_yz_uv_map(docking_port_mesh[1]);
	/* model 2 is the "invisible" docking port for starbases that have their docking ports
	 * integrated into the starbase model (e.g. starbase/starbase.obj).
	 */
	docking_port_mesh[DOCKING_PORT_INVISIBLE_MODEL] = snis_read_model(d, "tetrahedron.stl");
	nebula_mesh = mesh_fabricate_billboard(2, 2);
	sun_mesh = mesh_fabricate_billboard(30000, 30000);
	unit_quad = mesh_fabricate_billboard(1, 1);
	thrust_animation_mesh = init_thrust_mesh(70, 200, 1.3, 1);
	warpgate_mesh = snis_read_model(d, "warpgate.stl");
	mesh_cylindrical_yz_uv_map(warpgate_mesh);
	warpgate_effect_mesh = mesh_fabricate_disc(1.0, 32);
	warp_core_mesh = snis_read_model(d, "warp-core.stl");
	mesh_cylindrical_yz_uv_map(warp_core_mesh);
	cylinder_mesh = snis_read_model(d, "cylinder.stl");
	mesh_cylindrical_yz_uv_map(cylinder_mesh);

	for (i = 0; i < NLIGHTNINGS; i++) {
		float u1, v1, u2, v2;

		u1 = mtwist_float(mt) * 0.75;
		u2 = u1 + 0.25;
		v1 = mtwist_float(mt) * 0.75;
		v2 = v1 + 0.25;
		planetary_lightning_mesh[i] = mesh_fabricate_billboard_with_uv_map(1.0, 1.0, u1, v1, u2, v2);
	}

	open_simplex_noise_free(osn);
	mtwist_free(mt);
}

static void init_vects(void)
{
	int i;

	setup_vect(placeholder_system, placeholder_system_points);
	setup_vect(placeholder_socket, placeholder_socket_points);
	setup_vect(placeholder_part, placeholder_part_points);
	spin_points(placeholder_part_points, ARRAYSIZE(placeholder_part_points),
				&placeholder_part_spun_points, 128, 0, 0);
	scale_points(damcon_robot_points,
			ARRAYSIZE(damcon_robot_points), 0.5, 0.5);
	setup_vect(damcon_robot, damcon_robot_points);
	spin_points(damcon_robot_points, ARRAYSIZE(damcon_robot_points),
			&damcon_robot_spun_points, 256, 0, 0);
	for (i = 0; i < 256; i++) {
		damcon_robot_spun[i].p =
			&damcon_robot_spun_points[i * ARRAYSIZE(damcon_robot_points)];
		damcon_robot_spun[i].npoints = ARRAYSIZE(damcon_robot_points);
		calculate_bbox(&damcon_robot_spun[i]);
	}
	for (i = 0; i < 128; i++) {
		placeholder_part_spun[i].p =
			&placeholder_part_spun_points[i * ARRAYSIZE(placeholder_part_points)];
		placeholder_part_spun[i].npoints = ARRAYSIZE(placeholder_part_points);
		calculate_bbox(&placeholder_part_spun[i]);
	}
}

static void init_gl(int argc, char *argv[], GtkWidget *drawing_area)
{
#ifndef WITHOUTOPENGL
	gtk_gl_init(&argc, &argv);

	/* prepare GL */
	GdkGLConfig *gl_config = gdk_gl_config_new_by_mode(GDK_GL_MODE_RGBA |
				       GDK_GL_MODE_DEPTH | GDK_GL_MODE_DOUBLE);
	if (!gl_config)
		g_assert_not_reached();

	if (!gtk_widget_set_gl_capability(drawing_area, gl_config, NULL, TRUE,
						GDK_GL_RGBA_TYPE))
		g_assert_not_reached();
#endif
	
}

static void prevent_zombies(void)
{
	struct sigaction sa;

	sa.sa_handler = SIG_IGN;
	sa.sa_flags = SA_NOCLDWAIT;
	if (sigaction(SIGCHLD, &sa, NULL) != 0)
		fprintf(stderr, "Failed to ignore SIGCHLD, beware of zombies: %s\n",
				strerror(errno));
}

static void set_random_seed(void)
{
	char *seed = getenv("SNISRAND");
	int i, rc;

	if (!seed)
		return;

	rc = sscanf(seed, "%d", &i);
	if (rc != 1)
		return;

	snis_srand((unsigned int) i);
	srand(i);
	mtwist_seed = (uint32_t) i;
}

static void take_your_locale_and_shove_it(void)
{
	/* Need this so that fscanf reads floats properly. */
#define LOCALE_THAT_WORKS "C"
	if (setenv("LANG", LOCALE_THAT_WORKS, 1) <  0)
		fprintf(stderr, "Failed to setenv LANG to '%s'\n",
			LOCALE_THAT_WORKS);
	setlocale(LC_ALL, "C");
}

static void figure_aspect_ratio(int requested_x, int requested_y,
				int *x, int *y)
{
	int sw, sh, monitors;
	GdkScreen *s;
	GdkRectangle bounds;

	s = gdk_screen_get_default();
	if (!s)
		return;

	monitors = gdk_screen_get_n_monitors(s);
	if (monitorid == -1 || monitorid >= monitors)
		monitorid = gdk_screen_get_primary_monitor(s);

	gdk_screen_get_monitor_geometry(s, monitorid, &bounds);
	sw = bounds.width;
	sh = bounds.height;
	screen_offset_x = bounds.x;
	screen_offset_y = bounds.y;


	if (requested_x <= 0 || requested_y <= 0) {
		*x = sw;
		*y = sh;
		return;
	}

	/*
	 * The requested long axis gets full screen dimension in that axis,
	 * and the other axis gets adjusted.  If that makes the other
	 * axis exceed screen dimensions, then further adjustments are made.
	 */
	if (requested_x > requested_y) {
		*x = sw;
		*y = (int) ((double) sw * (double) requested_y / (double) requested_x);
		if (*y > sh) {
			*y = sh;
			*x = (int) ((double) sh * (double) requested_x / (double) requested_y);
		}
	} else {
		*y = sh;
		*x = (int) ((double) sh * (double) requested_x / (double) requested_y);
		if (*x > sw) {
			*y = (int) ((double) sw * (double) requested_y / (double) requested_x);
			*x = sw;
		}
	}
}

static void init_colors(void)
{
	char color_file[PATH_MAX];
	char *alternate = getenv("SNIS_COLORS");
	if (alternate && strlen(alternate) + strlen(asset_dir) < PATH_MAX - 3) {
		snprintf(color_file, sizeof(color_file), "%s/%s", asset_dir, alternate);
	} else {
		if (strlen(asset_dir) + strlen("user_colors.cfg") < PATH_MAX - 3)
			snprintf(color_file, sizeof(color_file), "%s/%s", asset_dir, "user_colors.cfg");
		else {
			fprintf(stderr, "Path to color config file too long, skipping.\n");
			return;
		}
	}
	sng_setup_colors(main_da, color_file);
}

static void check_lobby_serverhost_options()
{
	if (serverport != -1) {
		if (serverport < 0 || serverport > 0x0ffff) {
			fprintf(stderr, "snis_client: Bad serverport %d\n", serverport);
			usage();
		}
		if (strcmp(lobbyhost, "localhost") != 0) {
			fprintf(stderr,
				"snis_client: lobbyhost and serverport are incompatible options\n");
			usage();
		}
		if (serverhost == NULL && !avoid_lobby) {
			fprintf(stderr,
				"snis_client: serverhost requires --nolobby option as well\n");
			usage();
		}
		if (!avoid_lobby) {
			fprintf(stderr,
				"snis_client: serverport requires --nolobby option as well\n");
			usage();
		}
		if (serverhost == NULL) {
			fprintf(stderr,
				"snis_client: serverport option requires serverhost option as well\n");
			usage();
		}
	}
}

static struct option long_options[] = {
	{ "allroles", no_argument, NULL, 'A' },
	{ "soundserver", no_argument, NULL, 'a' },
	{ "comms", no_argument, NULL, 'C' },
	{ "engineering", no_argument, NULL, 'E' },
	{ "fullscreen", no_argument, NULL, 'f' },
	{ "nolobby", no_argument, NULL, 'L' },
	{ "lobbyhost", required_argument, NULL, 'l' },
	{ "navigation", no_argument, NULL, 'N' },
	{ "starship", required_argument, NULL, 'n' },
	{ "main", no_argument, NULL, 'M' },
	{ "monitor", required_argument, NULL, 'm' },
	{ "serverport", required_argument, NULL, 'p' },
	{ "pw", required_argument, NULL, 'P' },
	{ "quickstart", no_argument, NULL, 'q' },
	{ "aspect-ratio", required_argument, NULL, 'r' },
	{ "science", no_argument, NULL, 'S' },
	{ "serverhost", required_argument, NULL, 's' },
	{ "version", no_argument, NULL, 'v' },
	{ "weapons", no_argument, NULL, 'W' },
	{ "solarsystem", required_argument, NULL, '*'},
	{ "joystick", required_argument, NULL, 'j'},
	{ "trap-nans", no_argument, NULL, 't'},
	{ 0, 0, 0, 0 },
};

static void process_options(int argc, char *argv[])
{
	int x, y, rc, c, value;

	role = 0;
	x = -1;
	y = -1;
	while (1) {
		int option_index;
		c = getopt_long(argc, argv, "AaCEfj:Ll:Nn:Mm:p:P:qr:Ss:tvW*:", long_options, &option_index);
		if (c == -1)
			break;
		switch (c) {
		case 'v':
			printf("snis_client v. %s\n", SNIS_VERSION);
			break;
		case 'q':
			quickstartmode = 1;
			break;
		case 'l':
			lobbyhost = optarg;
			if (!lobbyhost)
				usage();
			break;
		case 'L':
			avoid_lobby = 1;
			break;
		case 's':
			serverhost = optarg;
			if (!serverhost)
				usage();
			break;
		case 'm':
			if (!optarg)
				usage();
			rc = sscanf(optarg, "%d", &value);
			if (rc == 1)
				monitorid = value;
			else
				usage();
			break;
		case 'p':
			if (!optarg)
				usage();
			rc = sscanf(optarg, "%d", &value);
			if (rc == 1)
				serverport = value;
			else
				usage();
			break;
		case 'P':
			password = optarg;
			if (!password)
				usage();
			break;
		case 'n':
			shipname = optarg;
			if (!shipname)
				usage();
			break;
		case 'j':
			if (njoysticks < MAX_JOYSTICKS) {
				strcpy(joystick_device[njoysticks], optarg);
				njoysticks++;
			}
			break;
		case 'A':
			role |= ROLE_ALL;
			break;
		case 'M':
			role |= ROLE_MAIN;
			break;
		case 'N':
			role |= ROLE_NAVIGATION;
			break;
		case 'W':
			role |= ROLE_WEAPONS;
			break;
		case 'E':
			role |= ROLE_ENGINEERING;
			break;
		case 'S':
			role |= ROLE_SCIENCE;
			break;
		case 'C':
			role |= ROLE_COMMS;
			break;
		case 'a':
			role |= ROLE_SOUNDSERVER;
			break;
		case 'f':
			fullscreen = 1;
			break;
		case 'r':
			if (!optarg)
				usage();
			rc = sscanf(optarg, "%d%*[,:]%d", &x, &y);
			if (rc != 2)
				usage();
			requested_aspect_x = x;
			requested_aspect_y = y;
			break;
		case 'y':
			if (!optarg)
				usage();
			rc = scanf(optarg, "%d", &y);
			if (rc != 1)
				usage();
			requested_aspect_y = y;
			break;
		case '*':
			if (!optarg)
				usage();
			solarsystem_name = optarg;
			break;
		case 't':
			trap_nans = 1;
			break;
		default:
			usage();
		}
	}
	if (role == 0)
		role = ROLE_ALL;
}

static void setup_ship_mesh_maps(void)
{
	ship_mesh_map = malloc(sizeof(*ship_mesh_map) * nshiptypes);
	derelict_mesh = malloc(sizeof(*derelict_mesh) * nshiptypes);
	if (!ship_mesh_map || !derelict_mesh) {
		fprintf(stderr, "out of memory at %s:%d\n", __FILE__, __LINE__);
		exit(1);
	}
	memset(ship_mesh_map, 0, sizeof(*ship_mesh_map) * nshiptypes);
	memset(derelict_mesh, 0, sizeof(*derelict_mesh) * nshiptypes);
}

static void setup_screen_parameters(void)
{
	GdkScreen *s;

	s = gdk_screen_get_default();
	if (s)
		figure_aspect_ratio(requested_aspect_x, requested_aspect_y,
					&SCREEN_WIDTH, &SCREEN_HEIGHT);
	if (requested_aspect_x >= 0 || requested_aspect_y >= 0)
		fullscreen = 0; /* can't request aspect ratio AND fullscreen */
	real_screen_width = SCREEN_WIDTH;
	real_screen_height = SCREEN_HEIGHT;

	damconscreenxdim = 600 * SCREEN_WIDTH / 800;
	damconscreenydim = 500 * SCREEN_HEIGHT / 600;
	damconscreenx0 = 20 * SCREEN_WIDTH / 800;
	damconscreeny0 = 80 * SCREEN_HEIGHT / 600;
	sng_set_extent_size(SCREEN_WIDTH, SCREEN_HEIGHT);
	sng_set_screen_size(real_screen_width, real_screen_height);
}

static void setup_window_geometry(GtkWidget *window)
{
	/* clamp window aspect ratio to constant */
	GdkGeometry geom;
	geom.min_aspect = (gdouble) SCREEN_WIDTH / (gdouble) SCREEN_HEIGHT;
	geom.max_aspect = geom.min_aspect;
	gtk_window_set_geometry_hints(GTK_WINDOW(window), NULL, &geom, GDK_HINT_ASPECT);
}

static void setup_gtk_window_and_drawing_area(GtkWidget **window, GtkWidget **vbox, GtkWidget **main_da)
{
	*window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	setup_window_geometry(*window);
	/* These scalings are arbitrary, just played with it until it looked "good." */
	snis_typefaces_init_with_scaling((float) SCREEN_WIDTH / 1050.0, (float) SCREEN_HEIGHT / 500.0);

	gtk_container_set_border_width(GTK_CONTAINER(*window), 0);
	*vbox = gtk_vbox_new(FALSE, 0);
	gtk_window_move(GTK_WINDOW(*window), screen_offset_x, screen_offset_y);
	*main_da = gtk_drawing_area_new();

	g_signal_connect(G_OBJECT(*window), "delete_event",
		G_CALLBACK(delete_event), NULL);
	g_signal_connect(G_OBJECT(*window), "destroy",
		G_CALLBACK(destroy), NULL);
	g_signal_connect(G_OBJECT(*window), "key_press_event",
		G_CALLBACK(key_press_cb), "window");
	g_signal_connect(G_OBJECT(*window), "key_release_event",
		G_CALLBACK(key_release_cb), "window");
	g_signal_connect(G_OBJECT(*main_da), "expose_event",
		G_CALLBACK(main_da_expose), NULL);
	g_signal_connect(G_OBJECT(*main_da), "configure_event",
		G_CALLBACK(main_da_configure), NULL);
	g_signal_connect(G_OBJECT(*main_da), "scroll_event",
		G_CALLBACK(main_da_scroll), NULL);
	gtk_widget_add_events(*main_da, GDK_BUTTON_PRESS_MASK);
	gtk_widget_add_events(*main_da, GDK_BUTTON_RELEASE_MASK);
	gtk_widget_add_events(*main_da, GDK_BUTTON3_MOTION_MASK);
	gtk_widget_add_events(*main_da, GDK_POINTER_MOTION_MASK);
	g_signal_connect(G_OBJECT(*main_da), "button_press_event",
		G_CALLBACK(main_da_button_press), NULL);
	g_signal_connect(G_OBJECT(*main_da), "button_release_event",
		G_CALLBACK(main_da_button_release), NULL);
	g_signal_connect(G_OBJECT(*main_da), "motion_notify_event",
		G_CALLBACK(main_da_motion_notify), NULL);

	gtk_container_add(GTK_CONTAINER(*window), *vbox);
	gtk_box_pack_start(GTK_BOX(*vbox), *main_da, TRUE /* expand */, TRUE /* fill */, 0);

	gtk_window_set_default_size(GTK_WINDOW(*window), real_screen_width, real_screen_height);
}

static void lobby_chosen(void *x)
{
	uint32_t ipaddr = (uint32_t) (intptr_t) x;
	char msg[100];

	snprintf(msg, sizeof(msg), "%d.%d.%d.%d",
		(ntohl(ipaddr) & 0xff000000) >> 24,
		(ntohl(ipaddr) & 0x00ff0000) >> 16,
		(ntohl(ipaddr) & 0x0000ff00) >> 8,
		(ntohl(ipaddr) & 0x000000ff));
	snis_text_input_box_set_contents(net_setup_ui.lobbyservername, msg);
	connect_to_lobby_button_pressed();
}

/* This gets called from a thread created by ssgl_register_for_bcast_packets() with a mutex held. */
static void lobby_list_change_notification(struct ssgl_lobby_descriptor *list, int nitems)
{
	int i;
	char msg[255];
	struct ssgl_lobby_descriptor lobbylist[256] = { { 0 } };
	struct pull_down_menu *newmenu;
	int nlobbies = 0;

	/* Make a local copy of the list */
	if (nitems >= ARRAYSIZE(lobbylist))
		nitems = ARRAYSIZE(lobbylist);
	memcpy(lobbylist, list, sizeof(lobbylist[0]) * nitems);
	nlobbies = nitems;

	/* Copy the lobby info to a new pull down menu */
	newmenu = create_pull_down_menu(NANO_FONT, SCREEN_WIDTH);
	pull_down_menu_set_color(newmenu, UI_COLOR(network_setup_active));
	pull_down_menu_add_column(newmenu, "SELECT LOBBY");
	for (i = 0; i < nlobbies; i++) {
		snprintf(msg, sizeof(msg), "%d.%d.%d.%d:%d %s",
			(ntohl(lobbylist[i].ipaddr) & 0xff000000) >> 24,
			(ntohl(lobbylist[i].ipaddr) & 0x00ff0000) >> 16,
			(ntohl(lobbylist[i].ipaddr) & 0x0000ff00) >> 8,
			(ntohl(lobbylist[i].ipaddr) & 0x000000ff),
			ntohs(lobbylist[i].port),
			lobbylist[i].hostname);
		pull_down_menu_add_row(newmenu, "SELECT LOBBY", msg, lobby_chosen,
					(void *) (intptr_t) lobbylist[i].ipaddr);
	}

	/* Swap in the new network setup menu */
	pull_down_menu_copy(net_setup_ui.menu, newmenu);
	pull_down_menu_clear(newmenu);
	free(newmenu);
}

static void maybe_connect_to_lobby(void)
{
	if (avoid_lobby) {
		done_with_lobby = 1;
		lobby_socket = -1;
		displaymode = DISPLAYMODE_CONNECTING;
		return;
	}
	if (displaymode != DISPLAYMODE_NETWORK_SETUP || quickstartmode) {
			connect_to_lobby();
			if (quickstartmode)
				displaymode = DISPLAYMODE_LOBBYSCREEN;
	}
}

static void read_replacement_assets(struct replacement_asset *r, char *asset_dir)
{
	int rc;
	char p[PATH_MAX];

	sprintf(p, "%s/replacement_assets.txt", asset_dir);
	errno = 0;
	rc = replacement_asset_read(p, asset_dir, r);
	if (rc < 0 && errno != EEXIST)
		fprintf(stderr, "%s: Warning:  %s\n", p, strerror(errno));
}

static char *stl_parser_asset_replacer(char *path)
{
	return replacement_asset_lookup(path, &replacement_assets);
}

int main(int argc, char *argv[])
{
	GtkWidget *vbox;
	int i;

	refuse_to_run_as_root("snis_client");
	displaymode = DISPLAYMODE_NETWORK_SETUP;

	xdg_base_ctx = xdg_base_context_new("space-nerds-in-space", ".space-nerds-in-space");
	take_your_locale_and_shove_it();
	ignore_sigpipe();
	prevent_zombies();
	set_random_seed();
	process_options(argc, argv);
#ifndef __APPLE__
	if (trap_nans)
		feenableexcept(FE_DIVBYZERO | FE_INVALID | FE_OVERFLOW);
#endif
	check_lobby_serverhost_options();
	asset_dir = override_asset_dir();
	setup_sound();
	snis_opcode_def_init();
	memset(&main_screen_text, 0, sizeof(main_screen_text));
	snis_object_pool_setup(&pool, MAXGAMEOBJS);
	snis_object_pool_setup(&sparkpool, MAXSPARKS);
	snis_object_pool_setup(&damcon_pool, MAXDAMCONENTITIES);
	memset(dco, 0, sizeof(dco));
	damconscreenx = NULL;
	damconscreeny = NULL;

	read_replacement_assets(&replacement_assets, asset_dir);
	stl_parser_set_asset_replacement_function(stl_parser_asset_replacer);

	char commodity_path[PATH_MAX];
	snprintf(commodity_path, sizeof(commodity_path), "%s/%s", asset_dir, "commodities.txt");
	commodity = read_commodities(commodity_path, &ncommodities);

	if (read_ship_types())
		return -1;
	if (read_factions())
		return -1;
	setup_ship_mesh_maps();
	setup_rts_unit_type_to_ship_type_map(ship_type, nshiptypes);
	init_vects();
	initialize_random_orientations_and_spins(COMMON_MTWIST_SEED);
	init_planetary_atmospheres();
	if (read_solarsystem_config(solarsystem_name, &solarsystem_assets)) {
		fprintf(stderr, "Failed reading solarsystem metadata\n");
		exit(1);
	}
	if (read_starbase_model_metadata(asset_dir, "starbase_models.txt",
			&nstarbase_models, &starbase_metadata)) {
		fprintf(stderr, "Failed reading starbase model metadata\n");
		exit(1);
	}
	starbase_mesh = allocate_starbase_mesh_ptrs(nstarbase_models);
	docking_port_info = read_docking_port_info(starbase_metadata, nstarbase_models,
							STARBASE_SCALE_FACTOR);
	maybe_connect_to_lobby();
	gtk_init(&argc, &argv);
	setup_screen_parameters();
	init_keymap();
	read_keymap_config_file();
	setup_gtk_window_and_drawing_area(&window, &vbox, &main_da);
	init_gl(argc, argv, main_da);
        gtk_widget_show (vbox);
        gtk_widget_show (main_da);
        gtk_widget_show (window);

	init_colors();

#ifndef WITHOUTOPENGL
	/* must be set or the window will not show */
	GdkColor black = { 0, 0, 0, 0 };
	gtk_widget_modify_bg(main_da, GTK_STATE_NORMAL, &black);
#endif

        gc = gdk_gc_new(GTK_WIDGET(main_da)->window);
	sng_set_context(GTK_WIDGET(main_da)->window, gc);
	sng_set_foreground(WHITE);

	timer_tag = g_timeout_add(1000 / max_frame_rate_hz, advance_game, NULL);

#ifndef GLIB_VERSION_2_32
	/* this is only needed in glibc versions before 2.32 */

	/* Apparently (some versions of?) portaudio calls g_thread_init(). */
	/* It may only be called once, and subsequent calls abort, so */
	/* only call it if the thread system is not already initialized. */
	if (!g_thread_supported ())
		g_thread_init(NULL);
#endif
	gdk_threads_init();

	gettimeofday(&start_time, NULL);
	universe_timestamp_offset = time_now_double(); /* until we get real time from server */

	snis_slider_set_sound(SLIDER_SOUND);
	text_window_set_timer(&timer);
	ui_set_tooltip_drawing_function(draw_tooltip);
	init_lobby_ui();
	init_nav_ui();
	init_engineering_ui();
	init_damcon_ui();
	init_weapons_ui();
	init_science_ui();
	init_comms_ui();
	init_demon_ui();
	init_net_setup_ui();
	setup_joysticks(window);
	setup_physical_io_socket();
	setup_natural_language_fifo();
	setup_demon_fifo();
	setup_text_to_speech_thread();
	voice_chat_setup_threads();
	ecx = entity_context_new(5000, 5000);

	snis_slider_mouse_position_query(&mouse.x, &mouse.y);

	ssgl_register_for_bcast_packets(lobby_list_change_notification); /* must happen after init_net_setup_ui() */
	snis_protocol_debugging(1);

	set_default_clip_window();

	if (fullscreen)
		gtk_window_fullscreen(GTK_WINDOW(window));

	gtk_main ();
        wwviaudio_cancel_all_sounds();
        wwviaudio_stop_portaudio();
	for (i = 0; i < njoysticks; i++)
		close_joystick(joystick_fd[i]);
	return 0;
}
