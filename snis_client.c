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
#if !defined(__APPLE__) && !defined(__FreeBSD__)
/* Apple gets what it needs for malloc from stdlib.h */
#include <malloc.h>
#endif
#include <gtk/gtk.h>

#ifndef WITHOUTOPENGL
#include <gtk/gtkgl.h>
#include <GL/glew.h>
#include <GL/glu.h>
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
#if !defined(__APPLE__) && !defined(__FreeBSD__)
#include <linux/tcp.h>
#else
#include <netinet/tcp.h>
#include <netinet/in.h>
#endif

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
#include "snis.h"
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
#include "snis_socket_io.h"
#include "ssgl/ssgl.h"
#include "snis_marshal.h"
#include "snis_packet.h"
#include "wwviaudio.h"
#include "sounds.h"
#include "bline.h"
#include "shield_strength.h"
#include "joystick.h"
#include "stacktrace.h"
#include "snis_keyboard.h"
#include "snis_damcon_systems.h"
#include "build_info.h"
#include "snis-device-io.h"

#include "vertex.h"
#include "triangle.h"
#include "material.h"
#include "mesh.h"
#include "stl_parser.h"
#include "entity.h"
#include "matrix.h"
#include "graph_dev.h"

#define SHIP_COLOR CYAN
#define STARBASE_COLOR RED
#define WORMHOLE_COLOR WHITE
#define PLANET_COLOR GREEN
#define ASTEROID_COLOR AMBER
#define CARGO_CONTAINER_COLOR YELLOW 
#define DERELICT_COLOR BLUE 
#define PARTICLE_COLOR YELLOW
#define LASER_COLOR GREEN
#define PLAYER_LASER_COLOR GREEN
#define NPC_LASER_COLOR ORANGERED
#define TARGETING_COLOR ORANGERED
#define SCIENCE_SELECT_COLOR GREEN
#define TORPEDO_COLOR RED
#define SPACEMONSTER_COLOR GREEN
#define NEBULA_COLOR MAGENTA
#define TRACTORBEAM_COLOR BLUE

#define ARRAYSIZE(x) (sizeof(x) / sizeof((x)[0]))

#define SCREEN_WIDTH 800        /* window width, in pixels */
#define SCREEN_HEIGHT 600       /* window height, in pixels */
#define ASPECT_RATIO (SCREEN_WIDTH/(float)SCREEN_HEIGHT)

#define VERTICAL_CONTROLS_INVERTED -1
#define VERTICAL_CONTROLS_NORMAL 1
static int vertical_controls_inverted = VERTICAL_CONTROLS_NORMAL;
static volatile int vertical_controls_timer = 0;
static int display_frame_stats = 0;
static int quickstartmode = 0; /* allows auto connecting to first (only) lobby entry */
static float turret_recoil_amount = 0.0f;

static int mtwist_seed = 59377;

typedef void explosion_function(int x, int y, int ivx, int ivy, int v, int nsparks, int time);

explosion_function *explosion = NULL;

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
int frame_rate_hz = 30;
int red_alert_mode = 0;
#define MAX_UPDATETIME_START_PAUSE 1.5
#define MAX_UPDATETIME_INTERVAL 0.5

#ifndef PREFIX
#define PREFIX .
#warn "PREFIX defaulted to ."
#endif

#define STRPREFIX(s) str(s)
#define str(s) #s

char *default_asset_dir = STRPREFIX(PREFIX) "/share/snis";
char *asset_dir;

typedef void (*joystick_button_fn)(void *x);
char joystick_device[PATH_MAX+1];
int joystick_fd = -1;
int joystickx, joysticky;

static int physical_io_socket = -1;
static pthread_t physical_io_thread;
static pthread_attr_t physical_io_thread_attr;

GtkWidget *window;
GdkGC *gc = NULL;               /* our graphics context. */
GtkWidget *main_da;             /* main drawing area. */
gint timer_tag;  
int fullscreen = 0;
int in_the_process_of_quitting = 0;
int current_quit_selection = 0;
int final_quit_selection = 0;

struct ship_type_entry *ship_type;
int nshiptypes = 0;

uint32_t role = ROLE_ALL;

char *password;
char *shipname;
#define UNKNOWN_ID 0xffffffff
uint32_t my_ship_id = UNKNOWN_ID;
uint32_t my_ship_oid = UNKNOWN_ID;

int real_screen_width;
int real_screen_height;
int warp_limbo_countdown = 0;
int damage_limbo_countdown = 0;

struct entity_context *ecx;
struct entity_context *sciecx;
struct entity_context *navecx;
struct entity_context *tridentecx;
struct entity_context *sciballecx;

static int ecx_fake_stars_initialized = 0;

static volatile int displaymode = DISPLAYMODE_LOBBYSCREEN;
static volatile int helpmode = 0;
static volatile int helpmodeline = 0;
static volatile float weapons_camera_shake = 0.0f; 
static unsigned char camera_mode;

struct client_network_stats {
	uint64_t bytes_sent;
	uint64_t bytes_recd;
	uint32_t nobjects, nships;
	uint32_t elapsed_seconds;
	uint32_t faction_population[5];
} netstats;

int nframes = 0;
int timer = 0;
struct timeval start_time, end_time;
#define UNIVERSE_TICKS_PER_SECOND 10
static double universe_timestamp_offset = 0;

volatile int done_with_lobby = 0;
pthread_t lobby_thread; pthread_attr_t lobby_attr;
pthread_t gameserver_connect_thread; pthread_attr_t gameserver_connect_attr;
pthread_t read_from_gameserver_thread; pthread_attr_t gameserver_reader_attr;

pthread_t write_to_gameserver_thread; pthread_attr_t gameserver_writer_attr;
struct packed_buffer_queue to_server_queue;
pthread_mutex_t to_server_queue_mutex;
pthread_mutex_t to_server_queue_event_mutex;
pthread_cond_t server_write_cond = PTHREAD_COND_INITIALIZER;
int have_packets_for_server = 0;

int lobby_socket = -1;
int gameserver_sock = -1;
int lobby_count = 0;
char lobbyerror[200];
char *lobbyhost = "localhost";
struct ssgl_game_server lobby_game_server[100];
int ngameservers = 0;

struct ui_element_list *uiobjs = NULL;
ui_element_drawing_function ui_slider_draw = (ui_element_drawing_function) snis_slider_draw;
ui_element_button_press_function ui_slider_button_press = (ui_element_button_press_function) snis_slider_button_press;

ui_element_drawing_function ui_button_draw = (ui_element_drawing_function) snis_button_draw;
ui_element_drawing_function ui_label_draw = (ui_element_drawing_function) snis_label_draw;
ui_element_button_press_function ui_button_button_press = (ui_element_button_press_function) snis_button_button_press;
ui_element_drawing_function ui_gauge_draw = (ui_element_drawing_function) gauge_draw;
ui_element_drawing_function ui_text_window_draw = (ui_element_drawing_function) text_window_draw;
ui_element_drawing_function ui_text_input_draw = (ui_element_drawing_function)
					snis_text_input_box_draw;
ui_element_set_focus_function ui_text_input_box_set_focus = (ui_element_set_focus_function)
					snis_text_input_box_set_focus;
ui_element_button_press_function ui_text_input_button_press = (ui_element_button_press_function)
					snis_text_input_box_button_press;
ui_element_keypress_function ui_text_input_keypress = (ui_element_keypress_function)
					snis_text_input_box_keypress;

#define MAXTEXTURES 10

#ifdef WITHOUTOPENGL
#define GLuint int
#endif

char skybox_texture_prefix[255];
volatile int textures_loaded = 0; /* blech, volatile global. */

struct mesh *torpedo_mesh;
struct mesh *torpedo_nav_mesh;
struct mesh *laser_mesh;
struct mesh *asteroid_mesh[NASTEROID_MODELS];
struct mesh *sphere_mesh;
struct mesh *planetary_ring_mesh;
struct mesh *starbase_mesh[NSTARBASE_MODELS];
struct mesh *ship_turret_mesh;
struct mesh *ship_turret_base_mesh;
struct mesh *particle_mesh;
struct mesh *debris_mesh;
struct mesh *debris2_mesh;
struct mesh *wormhole_mesh;
struct mesh *spacemonster_mesh;
struct mesh *laserbeam_mesh;
struct mesh *phaser_mesh;
struct mesh *laserbeam_nav_mesh;
struct mesh *ship_icon_mesh;
struct mesh *heading_indicator_mesh;
struct mesh *cargo_container_mesh;
struct mesh *nebula_mesh;
struct mesh *sun_mesh;
struct mesh *thrust_animation_mesh;

struct mesh **ship_mesh_map;
struct mesh **derelict_mesh;

#define NNEBULA_MATERIALS 20
static struct material nebula_material[NNEBULA_MATERIALS];
static struct material red_torpedo_material;
static struct material red_laser_material;
static struct material blue_tractor_material;
static struct material green_phaser_material;
static struct material spark_material;
static struct material warp_effect_material;
static struct material sun_material;
#define NPLANETARY_RING_MATERIALS 2
static struct material planetary_ring_material[NPLANETARY_RING_MATERIALS];
static struct material planet_material[NPLANET_MATERIALS * (NPLANETARY_RING_MATERIALS + 1)];
static struct material shield_material;
#define NASTEROID_TEXTURES 2
static struct material asteroid_material[NASTEROID_TEXTURES];
static struct material wormhole_material;
static struct material thrust_material;
#ifdef WITHOUTOPENGL
const int wormhole_render_style = RENDER_SPARKLE;
const int torpedo_render_style = RENDER_WIREFRAME | RENDER_BRIGHT_LINE | RENDER_NO_FILL;
const int laserbeam_render_style = RENDER_WIREFRAME | RENDER_BRIGHT_LINE | RENDER_NO_FILL;
const int spark_render_style = RENDER_WIREFRAME | RENDER_BRIGHT_LINE | RENDER_NO_FILL;
#else
const int wormhole_render_style = RENDER_NORMAL;
const int torpedo_render_style = RENDER_NORMAL;
const int laserbeam_render_style = RENDER_NORMAL;
const int spark_render_style = RENDER_NORMAL;
#endif

struct my_point_t snis_logo_points[] = {
#include "snis-logo.h"
};
struct my_vect_obj snis_logo;

struct my_point_t damcon_robot_points[] = {
#include "damcon-robot-points.h"
};
struct my_vect_obj damcon_robot;
struct my_point_t *damcon_robot_spun_points;
struct my_vect_obj damcon_robot_spun[256];

struct my_point_t placeholder_system_points[] = {
#include "placeholder-system-points.h"
};
struct my_vect_obj placeholder_system;

struct my_point_t placeholder_socket_points[] = {
#include "placeholder-socket-points.h"
};
struct my_vect_obj placeholder_socket;

struct my_point_t placeholder_part_points[] = {
#include "placeholder-part-points.h"
};
struct my_point_t *placeholder_part_spun_points;
struct my_vect_obj placeholder_part;
struct my_vect_obj placeholder_part_spun[128];

static struct snis_entity *curr_science_guy = NULL;
static struct snis_entity *prev_science_guy = NULL;

#define NRANDOM_ORIENTATIONS 20
static union quat random_orientation[NRANDOM_ORIENTATIONS];
#define NRANDOM_SPINS 20
static union quat random_spin[NRANDOM_SPINS];
static void initialize_random_orientations_and_spins(void)
{
	int i;
	struct mtwist_state *mt;

	mt = mtwist_init(mtwist_seed);
	for (i = 0; i < NRANDOM_ORIENTATIONS; i++) {
		float angle = mtwist_float(mt) * 2.0 * M_PI;
		consistent_random_axis_quat(mt, &random_orientation[i], angle);
	}
	for (i = 0; i < NRANDOM_SPINS; i++) {
		float angular_speed = ((float) mtwist_int(mt, 100) / 10.0 - 5.0) * M_PI / 180.0;
		consistent_random_axis_quat(mt, &random_spin[i], angular_speed);
	}
	mtwist_free(mt);
}

void to_snis_heading_mark(const union quat *q, double *heading, double *mark)
{
	quat_to_heading_mark(q,heading,mark);
	*heading = game_angle_to_math_angle(*heading);
}

static inline double to_uheading(double heading)
{
	return game_angle_to_math_angle(heading);
}

static void set_default_clip_window(void)
{
	sng_set_clip_window(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
}

static double universe_timestamp()
{
	return (time_now_double() - universe_timestamp_offset) * (double)UNIVERSE_TICKS_PER_SECOND;
}

#define MAX_LOBBY_TRIES 3
static void *connect_to_lobby_thread(__attribute__((unused)) void *arg)
{
	int i, sock = -1, rc, game_server_count;
	struct ssgl_game_server *game_server = NULL;
	struct ssgl_client_filter filter;

try_again:

	/* Loop, trying to connect to the lobby server... */
	strcpy(lobbyerror, "");
	while (1 && lobby_count < MAX_LOBBY_TRIES && displaymode == DISPLAYMODE_LOBBYSCREEN) {
		sock = ssgl_gameclient_connect_to_lobby(lobbyhost);
		lobby_count++;
		if (sock >= 0) {
			lobby_socket = sock;
			break;
		}
		if (errno)
			sprintf(lobbyerror, "%s (%d)", strerror(errno), errno);
		else
			sprintf(lobbyerror, "%s (%d)", 
				gai_strerror(sock), sock);
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
			sprintf(lobbyerror, "ssgl_recv_game_servers failed: %s\n", strerror(errno));
			goto handle_error;
		}
	
		/* copy the game server data to where the display thread will see it. */
		if (game_server_count > 100)
			game_server_count = 100;
		for (i = 0; i < game_server_count; i++)
			lobby_game_server[i] = game_server[i];
		ngameservers = game_server_count;
		if (game_server_count > 0) {
			free(game_server);
			game_server_count = 0;
		}
		ssgl_sleep(5);  /* just a thread safe sleep. */
	} while (!done_with_lobby);

outta_here:
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

static void connect_to_lobby()
{
	int rc;
        pthread_attr_init(&lobby_attr);
        pthread_attr_setdetachstate(&lobby_attr, PTHREAD_CREATE_DETACHED);
	rc = pthread_create(&lobby_thread, &lobby_attr, connect_to_lobby_thread, NULL);
	if (rc) {
		fprintf(stderr, "Failed to create lobby connection thread.\n");
		fprintf(stderr, "%d %s (%s)\n", rc, strerror(rc), strerror(errno));
	}
	return;
}

static struct snis_object_pool *pool;
static struct snis_object_pool *damcon_pool;
static double *damconscreenx, *damconscreeny;
static const int damconscreenxdim = 600;
static const int damconscreenydim = 500;
static const int damconscreenx0 = 20;
static const int damconscreeny0 = 80;

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

	go[i].birth_r = go[i].r[0];

	/* entity move will update this */
	go[i].x = 0;
	go[i].y = 0;
	go[i].z = 0;
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

	for (i = 0; i <= snis_object_pool_highest_object(pool); i++)
		if (go[i].id == id)
			return i;
	return -1;
}

static struct snis_entity *lookup_entity_by_id(uint32_t id)
{
	int index;

	if (id < 0)
		return NULL;

	index = lookup_object_by_id(id);
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

static void update_generic_damcon_object(struct snis_damcon_entity *o,
			double x, double y, double velocity, double heading)
{
	o->x = x;
	o->y = y;
	o->velocity = velocity;
	o->heading = heading;
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
} damcon_ui;

static int update_damcon_object(uint32_t id, uint32_t ship_id, uint32_t type,
			double x, double y, double velocity,
			double heading, uint8_t autonomous_mode)

{
	int i;
	struct snis_damcon_entity *o;
	const int selected = WHITE;
	const int deselected = AMBER;

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

struct thrust_attachment_point {
	int nports;
	struct _port {
		float scale;
		union vec3 pos;
	} port[5];
};

static struct thrust_attachment_point ship_thrust_attachment_points[] = {

	/* cruiser */
	{ 0 }, /* no exhaust ports */
	/* destroyer */
	#include "share/snis/models/destroyer.scad_params.h"
	,
	/* freighter */
	#include "share/snis/models/freighter.scad_params.h"
	,
	/* tanker */
	{ 0 }, /* no exhaust ports */
	/* transport */
	{ 0 }, /* no exhaust ports */
	/* battlestar */
	#include "share/snis/models/battlestar.scad_params.h"
	,
	/* spaceship */
	#include "share/snis/models/spaceship.scad_params.h"
	,
	/* asteroid-miner */
	#include "share/snis/models/asteroid-miner.scad_params.h"
	,
	/* spaceship2 */
	#include "share/snis/models/spaceship2.scad_params.h"
	,
	/* spaceship3 */
	{ 0 }, /* no exhaust ports */
	/* dragonhawk */
	#include "share/snis/models/dragonhawk.scad_params.h"
	,
	/* skorpio */
	#include "share/snis/models/skorpio.scad_params.h"
	,
	/* disruptor */
	#include "share/snis/models/disruptor.scad_params.h"
	,
	/* research_vessel */
	#include "share/snis/models/research-vessel.scad_params.h"
	,
	/* conqueror */
	#include "share/snis/models/conqueror.scad_params.h"
	,
	/* scrambler */
	#include "share/snis/models/scrambler.scad_params.h"
	,
	/* swordfish */
	#include "share/snis/models/swordfish.scad_params.h"
	,
	/* wombat */
	#include "share/snis/models/wombat.scad_params.h"
	,
	/* dreadknight */
	#include "share/snis/models/dreadknight/dreadknight-exhaust-plumes.h"
	,
	/* vanquisher */
	#include "share/snis/models/vanquisher.scad_params.h"
	,
	/* enforcer */
	#include "share/snis/models/enforcer.scad_params.h"
	,
};

static struct thrust_attachment_point *ship_thrust_attachment_point(int shiptype)
{
	/* since range of shiptype is determined runtime by reading a file... */
	if (shiptype < 0 || shiptype >= ARRAYSIZE(ship_thrust_attachment_points))
		return NULL;
	return &ship_thrust_attachment_points[shiptype];
}


static void add_ship_thrust_entities(struct entity *thrust_entity[], int *nthrust_entities,
		struct entity_context *cx, struct entity *e, int shiptype, int impulse);

static int update_econ_ship(uint32_t id, uint32_t timestamp, double x, double y, double z,
			union quat *orientation, uint16_t alive, uint32_t victim_id,
			uint8_t shiptype, uint8_t ai[], double threat_level,
			uint8_t npoints, union vec3 *patrol)
{
	int i;
	struct entity *e;
	struct entity *thrust_entity[MAX_THRUST_PORTS];
	int nthrust_ports = 0;
	double vx, vy, vz;

	i = lookup_object_by_id(id);
	if (i < 0) {
		e = add_entity(ecx, ship_mesh_map[shiptype % nshiptypes], x, y, z, SHIP_COLOR);
		if (e)
			add_ship_thrust_entities(thrust_entity, &nthrust_ports, ecx, e, shiptype, 36);
		vx = 0.0;
		vy = 0.0;
		vz = 0.0;
		i = add_generic_object(id, timestamp, x, y, z, vx, vy, vz, orientation, OBJTYPE_SHIP2, alive, e);
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

		update_generic_object(i, timestamp, x, y, z, vx, vy, vz, orientation, alive);

		for (j = 0; j < go[i].tsd.ship.nthrust_ports; j++) {
			struct thrust_attachment_point *ap = ship_thrust_attachment_point(shiptype);
			struct entity *t = go[i].tsd.ship.thrust_entity[j];
			union vec3 thrust_scale = { { thrust_size, 1.0, 1.0 } };
			vec3_mul_self(&thrust_scale, ap->port[j].scale);
			update_entity_non_uniform_scale(t, thrust_scale.v.x,
							thrust_scale.v.y, thrust_scale.v.z);
		}
	}

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
	strcpy(go[i].sdata.name, name);
	if (go[i].type == OBJTYPE_SHIP1 || go[i].type == OBJTYPE_SHIP2)
		go[i].tsd.ship.lifeform_count = lifeform_count;
	if (go[i].type == OBJTYPE_STARBASE)
		go[i].tsd.starbase.lifeform_count = lifeform_count;
	if (go[i].type != OBJTYPE_PLANET && go[i].type != OBJTYPE_STARBASE)
		go[i].sdata.science_data_known = 30 * 10; /* only remember for ten secs. */
	else
		go[i].sdata.science_data_known = 30 * 60; /* unless planet or starbase */
		
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
	}
	return 0;
}

static void init_laserbeam_data(struct snis_entity *o);
static void update_laserbeam_segments(struct snis_entity *o);
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
		go[i].move = update_laserbeam_segments;
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
		go[i].move = update_laserbeam_segments;
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

static void init_spacemonster_data(struct snis_entity *o, double y)
{
	int i;
	struct spacemonster_data *sd = &o->tsd.spacemonster;

	sd->zz = y;
	sd->front = 0;
	sd->x = malloc(sizeof(*o->tsd.spacemonster.x) *
					MAX_SPACEMONSTER_SEGMENTS);
	sd->y = malloc(sizeof(*o->tsd.spacemonster.x) *
					MAX_SPACEMONSTER_SEGMENTS);
	sd->z = malloc(sizeof(*o->tsd.spacemonster.x) *
					MAX_SPACEMONSTER_SEGMENTS);
	sd->entity = malloc(sizeof(*o->tsd.spacemonster.entity) *
					MAX_SPACEMONSTER_SEGMENTS);
	for (i = 0; i < MAX_SPACEMONSTER_SEGMENTS; i++) {
		sd->x[i] = o->x;
		sd->y[i] = 0.0;
		sd->z[i] = o->z;
		sd->entity[i] = add_entity(ecx, spacemonster_mesh, o->x, 0, o->z,
						SPACEMONSTER_COLOR);
		if (sd->entity[i])
			set_render_style(sd->entity[i], RENDER_SPARKLE);
	}
}

static __attribute__((unused)) struct mesh *init_sector_mesh(int extra_extent)
{
	int nlines = (2 * extra_extent + 2) * (2 * extra_extent + 2) * 4;
        struct mesh *my_mesh = malloc(sizeof(*my_mesh));

	my_mesh->geometry_mode = MESH_GEOMETRY_LINES;
	my_mesh->nvertices = 0;
	my_mesh->ntriangles = 0;
	my_mesh->nlines = 0;
	my_mesh->t = 0;
	my_mesh->v = malloc(sizeof(*my_mesh->v) * nlines * 2);
	my_mesh->l = malloc(sizeof(*my_mesh->l) * nlines);
	my_mesh->radius = sqrt((extra_extent + 1) * 2);

	int i, j;
	for (i = -1 - extra_extent; i < 1 + extra_extent; ++i) {
		for (j = -1 -extra_extent; j < 1 + extra_extent; ++j) {

			/* line left */
			mesh_add_point(my_mesh, i,  0, j);
			mesh_add_point(my_mesh, i + 1, 0, j);
			mesh_add_line_last_2(my_mesh, MESH_LINE_DOTTED);

			/* line bottom */
			mesh_add_point(my_mesh, i, 0, j);
			mesh_add_point(my_mesh, i, 0, j + 1);
			mesh_add_line_last_2(my_mesh, MESH_LINE_DOTTED);

			if (j == extra_extent) {
				/* line right */
				mesh_add_point(my_mesh, i, 0, j + 1);
				mesh_add_point(my_mesh, i + 1, 0, j + 1);
				mesh_add_line_last_2(my_mesh, MESH_LINE_DOTTED);
			}
			if (i == extra_extent) {
				/* line top */
				mesh_add_point(my_mesh, i + 1, 0, j);
				mesh_add_point(my_mesh, i + 1, 0, j + 1);
				mesh_add_line_last_2(my_mesh, MESH_LINE_DOTTED);
			}
		}
	}
	return my_mesh;
}

static void free_spacemonster_data(struct snis_entity *o)
{
	int i;
	struct spacemonster_data *sd = &o->tsd.spacemonster;

	if (o->type != OBJTYPE_SPACEMONSTER)
		return;

	if (sd->x) {
		free(sd->x);
		sd->x = NULL;
	}
	if (sd->y) {
		free(sd->y);
		sd->y = NULL;
	}
	if (sd->z) {
		free(sd->z);
		sd->z = NULL;
	}

	if (sd->entity) {
		for (i = 0; i < MAX_SPACEMONSTER_SEGMENTS; i++)
			if (sd->entity[i])
				remove_entity(ecx, sd->entity[i]);
		free(sd->entity);
		sd->entity = NULL;
	}
}

static void update_laserbeam_segments(struct snis_entity *o)
{
	double x1, y1, z1, x2, y2, z2, dx, dy, dz;
	double lastd;
	int i, oid, tid;
	struct snis_entity *origin, *target;
	struct laserbeam_data *ld = &o->tsd.laserbeam;
	double yaw;
	union vec3 right = { { 1.0f, 0.0f, 0.0f } };
	union vec3 up = { { 0.0f, 0.1f, 0.0f } } ;
	union vec3 target_vector;
	union quat orientation;

	oid = lookup_object_by_id(o->tsd.laserbeam.origin);
	tid = lookup_object_by_id(o->tsd.laserbeam.target);

	if (oid < 0 || tid < 0) {
		for (i = 0; i < MAX_LASERBEAM_SEGMENTS; i++)
			if (ld->entity[i])
				entity_set_mesh(ld->entity[i], NULL);
		return;
	}
	origin = &go[oid];
	target = &go[tid];

	target_vector.v.x = target->x - origin->x;
	target_vector.v.y = target->y - origin->y;
	target_vector.v.z = target->z - origin->z;

	quat_from_u2v(&orientation, &right, &target_vector, &up); /* correct up vector? */
	quat_normalize_self(&orientation);
	
	x1 = origin->x;
	y1 = origin->y;
	z1 = origin->z;
	x2 = target->x;
	y2 = target->y;
	z2 = target->z;

	yaw = atan2(z2 - z1, x2 - x1);

	x1 += cos(yaw) * 15.0;
	z1 -= sin(yaw) * 15.0;
	x2 -= cos(yaw) * 15.0;
	z2 += sin(yaw) * 15.0;

	dx = (x2 - x1) / MAX_LASERBEAM_SEGMENTS;
	dy = (y2 - y1) / MAX_LASERBEAM_SEGMENTS;
	dz = (z2 - z1) / MAX_LASERBEAM_SEGMENTS;

	for (i = 0; i < MAX_LASERBEAM_SEGMENTS; i++) {
		lastd = (snis_randn(50) - 25) / 100.0;
		ld->x[i] = x1 + (i + lastd) * dx;
		ld->y[i] = y1 + (i + lastd) * dy;
		ld->z[i] = z1 + (i + lastd) * dz; 
		if (ld->entity[i]) {
			update_entity_pos(ld->entity[i], ld->x[i], ld->y[i], ld->z[i]);
			update_entity_orientation(ld->entity[i], &orientation);
			update_entity_material(ld->entity[i], o->tsd.laserbeam.material);
		}
	}
}

static void init_laserbeam_data(struct snis_entity *o)
{
	struct laserbeam_data *ld = &o->tsd.laserbeam;
	int i, color;
	struct snis_entity *shooter;

	ld->x = malloc(sizeof(*o->tsd.laserbeam.x) *
					MAX_LASERBEAM_SEGMENTS);
	ld->y = malloc(sizeof(*o->tsd.laserbeam.y) *
					MAX_LASERBEAM_SEGMENTS);
	ld->z = malloc(sizeof(*o->tsd.laserbeam.z) *
					MAX_LASERBEAM_SEGMENTS);
	ld->entity = malloc(sizeof(*o->tsd.laserbeam.entity) *
					MAX_LASERBEAM_SEGMENTS);
	shooter = lookup_entity_by_id(ld->origin);
	if (o->type == OBJTYPE_TRACTORBEAM) {
		color = TRACTORBEAM_COLOR;
	} else {
		if (shooter)
			color = (shooter->type == OBJTYPE_SHIP2) ?
				NPC_LASER_COLOR : PLAYER_LASER_COLOR;
		else
			color = NPC_LASER_COLOR;
	}
	for (i = 0; i < MAX_LASERBEAM_SEGMENTS; i++) {
		ld->x[i] = o->x;
		ld->y[i] = o->y;
		ld->z[i] = 0.0;
		ld->entity[i] = add_entity(ecx, laserbeam_mesh, o->x, 0, -o->y, color);
		if (ld->entity[i])
			set_render_style(ld->entity[i], laserbeam_render_style);
	}
	update_laserbeam_segments(o);
}

static void free_laserbeam_data(struct snis_entity *o)
{
	int i;
	struct laserbeam_data *ld = &o->tsd.laserbeam;

	if (o->type != OBJTYPE_LASERBEAM && o->type != OBJTYPE_TRACTORBEAM)
		return;

	if (ld->x) {
		free(ld->x);
		ld->x = NULL;
	}
	if (ld->y) {
		free(ld->y);
		ld->y = NULL;
	}
	if (ld->z) {
		free(ld->z);
		ld->z = NULL;
	}

	if (ld->entity) {
		for (i = 0; i < MAX_LASERBEAM_SEGMENTS; i++)
			if (ld->entity[i])
				remove_entity(ecx, ld->entity[i]);
		free(ld->entity);
		ld->entity = NULL;
	}
}

static int update_spacemonster(uint32_t id, uint32_t timestamp, double x, double y, double z)
{
	int i;
	struct entity *e;

	i = lookup_object_by_id(id);
	if (i < 0) {
		e = add_entity(ecx, spacemonster_mesh, x, 0, z, SPACEMONSTER_COLOR);
		if (e)
			set_render_style(e, RENDER_SPARKLE);
		i = add_generic_object(id, timestamp, x, 0, z, 0, 0, 0,
				&identity_quat, OBJTYPE_SPACEMONSTER, 1, e);
		if (i < 0)
			return i;
		go[i].entity = e;
		init_spacemonster_data(&go[i], y);
	} else {
		struct spacemonster_data *sd;
		int n;

		update_generic_object(i, timestamp, x, 0, z, 0, 0, 0, &identity_quat, 1);
		if (go[i].entity)
			update_entity_pos(go[i].entity, x, y, z);
		sd = &go[i].tsd.spacemonster;
		sd->zz = y;
		n = (sd->front + 1) % MAX_SPACEMONSTER_SEGMENTS;
		sd->front = n;
		sd->x[n] = x;
		sd->y[n] = y;
		sd->z[n] = z;
		if (sd->entity[sd->front])
			update_entity_pos(sd->entity[sd->front], x, y, z);
	}
	return 0;
}

static int update_asteroid(uint32_t id, uint32_t timestamp, double x, double y, double z,
	double vx, double vy, double vz)
{
	int i, k, m, s;
	struct entity *e;
	union quat orientation;
	struct snis_entity *o;

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
			update_entity_scale(e, s ? s * 3.5 : 1.0);
			update_entity_material(e, &asteroid_material[id % NASTEROID_TEXTURES]);
		}
		i = add_generic_object(id, timestamp, x, y, z, vx, vy, vz,
				&orientation, OBJTYPE_ASTEROID, 1, e);
		if (i < 0)
			return i;
		o = &go[i];
		o->tsd.asteroid.rotational_velocity = random_spin[id % NRANDOM_SPINS];
	} else
		update_generic_object(i, timestamp, x, y, z, vx, vy, vz, NULL, 1);
	return 0;
}

static int update_cargo_container(uint32_t id, uint32_t timestamp, double x, double y, double z)
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
	} else {
		double vx, vy, vz;

		vx = x - go[i].x;
		vy = y - go[i].y;
		vz = z - go[i].z;
		update_generic_object(i, timestamp, x, y, z, vx, vy, vz, NULL, 1);
	}
	return 0;
}

static int update_derelict(uint32_t id, uint32_t timestamp, double x, double y, double z, uint8_t ship_kind)
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
	} else
		update_generic_object(i, timestamp, x, y, z, 0.0, 0.0, 0.0, NULL, 1);
	return 0;
}

static int update_planet(uint32_t id, uint32_t timestamp, double x, double y, double z, double r, uint8_t government,
				uint8_t tech_level, uint8_t economy, uint32_t dseed, int hasring,
				uint8_t security)
{
	int i, m;
	struct entity *e, *ring;
	union quat orientation;

	i = lookup_object_by_id(id);
	if (i < 0) {
		/* Orientation should be consistent across clients because planets don't move */
		orientation = random_orientation[id % NRANDOM_ORIENTATIONS];

		/* each planet texture has another version with a variation of the ring materials */
		int ring_m = id % NPLANETARY_RING_MATERIALS;
		m = id % NPLANET_MATERIALS + (NPLANET_MATERIALS * (ring_m + 1) * (hasring ? 1 : 0));

		e = add_entity(ecx, sphere_mesh, x, y, z, PLANET_COLOR);
		if (e) {
			update_entity_scale(e, r);
			update_entity_material(e, &planet_material[m]);
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
			}
		}
		i = add_generic_object(id, timestamp, x, y, z, 0.0, 0.0, 0.0,
					&orientation, OBJTYPE_PLANET, 1, e);
		if (i < 0)
			return i;
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

static int update_starbase(uint32_t id, uint32_t timestamp, double x, double y, double z)
{
	int i, m;
	struct entity *e;
	union quat orientation;

	i = lookup_object_by_id(id);
	if (i < 0) {
		quat_init_axis(&orientation, 1.0, 0.0, 0.0, 0.0);
		m = id % NSTARBASE_MODELS;
		e = add_entity(ecx, starbase_mesh[m], x, y, z, STARBASE_COLOR);
		i = add_generic_object(id, timestamp, x, y, z, 0.0, 0.0, 0.0,
					&orientation, OBJTYPE_STARBASE, 1, e);
		if (i < 0)
			return i;
	} else
		update_generic_object(i, timestamp, x, y, z, 0.0, 0.0, 0.0, NULL, 1);
	return 0;
}

static int update_nebula(uint32_t id, uint32_t timestamp, double x, double y, double z, double r)
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
	} else {
		struct snis_entity *o = &go[i];
		update_generic_object(i, timestamp, x, y, z, 0.0, 0.0, 0.0, NULL, 1);
		if (o->entity)
			update_entity_scale(o->entity, r * 2.0);
	}
	go[i].tsd.nebula.r = r;	
	go[i].alive = 1;
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
	o->alive--;
	if (o->alive <= 0) {
		remove_entity(ecx, o->entity);
		snis_object_pool_free_object(sparkpool, spark_index(o));
	}
}

static void shield_effect_move(struct snis_entity *o)
{
	o->x += o->vx;
	o->y += o->vy;
	o->z += o->vz;
	o->alive--;
	entity_update_alpha(o->entity, entity_get_alpha(o->entity) * 0.9);
	if (o->alive <= 0) {
		remove_entity(ecx, o->entity);
		snis_object_pool_free_object(sparkpool, spark_index(o));
	}
}

static void spark_move(struct snis_entity *o)
{
	union quat orientation;
	float scale;

	o->x += o->vx;
	o->y += o->vy;
	o->z += o->vz;
	o->alive--;
	if (o->alive <= 0) {
		remove_entity(ecx, o->entity);
		o->entity = NULL;
		snis_object_pool_free_object(sparkpool, spark_index(o));
		return;
	}

	/* Apply incremental rotation */
	quat_mul(&orientation, &o->tsd.spark.rotational_velocity, entity_get_orientation(o->entity));
	update_entity_orientation(o->entity, &orientation);
	scale = entity_get_scale(o->entity);
	update_entity_scale(o->entity, scale * o->tsd.spark.shrink_factor);
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
	float a = -0.5 * M_PI / 180.0;

	/* current rotation is universe timestamp * rotation per timestamp
	   rotational_velocity is frame_rate_hz and universe is 1/10 sec */
	a = a * (float)frame_rate_hz / 10.0 * timestamp;

	quat_init_axis(&o->orientation, 0.0, 0.0, 1.0, a);
	if (o->entity)
		update_entity_orientation(o->entity, &o->orientation);
}

static void spin_starbase(double timestamp, struct snis_entity *o)
{
	/* 0.5 degree per frame */
	float a = 0.5 * M_PI / 180.0;

	/* current rotation is universe timestamp * rotation per timestamp
	   rotational_velocity is frame_rate_hz and universe is 1/10 sec */
	a = a * (float)frame_rate_hz / 10.0 * timestamp;

	quat_init_axis(&o->orientation, 0.0, 0.0, 1.0, a);
	if (o->entity)
		update_entity_orientation(o->entity, &o->orientation);
}

static void arbitrary_spin(double timestamp, struct snis_entity *o, union quat *rotational_velocity)
{
	/* reduce to axis and angle */
	float x, y, z, a;
	quat_to_axis(rotational_velocity, &x, &y, &z, &a);

	/* current rotation is universe timestamp * rotation per timestamp
	   rotational_velocity is frame_rate_hz and universe is 1/10 sec */
	a = a * (float)frame_rate_hz / 10.0 * timestamp;

	quat_init_axis(&o->orientation, x, y, z, a);
	if (o->entity)
		update_entity_orientation(o->entity, &o->orientation);
}

static inline void spin_asteroid(double timestamp, struct snis_entity *o)
{
	arbitrary_spin(timestamp, o, &o->tsd.asteroid.rotational_velocity);
}

static inline void spin_cargo_container(double timestamp, struct snis_entity *o)
{
	arbitrary_spin(timestamp, o, &o->tsd.cargo_container.rotational_velocity);
}

static inline void spin_derelict(double timestamp, struct snis_entity *o)
{
	arbitrary_spin(timestamp, o, &o->tsd.derelict.rotational_velocity);
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
		o->x = o->r[to_index].v.x;
		o->y = o->r[to_index].v.y;
		o->z = o->r[to_index].v.z;
	} else {
		union vec3 interp_position;
		vec3_lerp(&interp_position, &o->r[from_index], &o->r[to_index], t);
		o->x = interp_position.v.x;
		o->y = interp_position.v.y;
		o->z = interp_position.v.z;
	}

	if (o->entity) {
		update_entity_visibility(o->entity, visible);
		update_entity_pos(o->entity, o->x, o->y, o->z);
	}
}

static void interpolate_orientated_object(double timestamp, struct snis_entity *o,
	int visible, int from_index, int to_index, float t)
{
#ifdef INTERP_DEBUG
	printf("  interpolate_orientated_generic_object: from=%d to=%d update_delta=%f, t=%f\n", from_index, to_index,
		o->updatetime[to_index] - o->updatetime[from_index], t);
#endif

	if (from_index == to_index) {
		o->x = o->r[to_index].v.x;
		o->y = o->r[to_index].v.y;
		o->z = o->r[to_index].v.z;
		o->orientation = o->o[to_index];
		if (o->type == OBJTYPE_SHIP1) {
			o->tsd.ship.sciball_orientation = o->tsd.ship.sciball_o[to_index];
			o->tsd.ship.weap_orientation = o->tsd.ship.weap_o[to_index];
		}
	} else {
		/* If ship is warping (seen as extreme speed here) do not try interpolating */
		if (vec3_dist_sqrd(&o->r[from_index], &o->r[to_index]) > 1000.0 * 1000.0) {
			o->x = o->r[from_index].v.x;
			o->y = o->r[from_index].v.y;
			o->z = o->r[from_index].v.z;
		} else {
			union vec3 interp_position;

			vec3_lerp(&interp_position, &o->r[from_index], &o->r[to_index], t);
			o->x = interp_position.v.x;
			o->y = interp_position.v.y;
			o->z = interp_position.v.z;
		}

		quat_nlerp(&o->orientation, &o->o[from_index], &o->o[to_index], t);
		o->heading = quat_to_heading(&o->orientation);

		if (o->type == OBJTYPE_SHIP1) {
			quat_nlerp(&o->tsd.ship.sciball_orientation,
					&o->tsd.ship.sciball_o[from_index], &o->tsd.ship.sciball_o[to_index], t);
			quat_nlerp(&o->tsd.ship.weap_orientation,
					&o->tsd.ship.weap_o[from_index], &o->tsd.ship.weap_o[to_index], t);
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
	interpolate_orientated_object(timestamp, o, visible, from_index, to_index, t);

	/* set the scaling based on the object age */
	if (!o->entity)
		return;

	struct mesh *m = entity_get_mesh(o->entity);
	if (!m)
		return;

	/* find out how far the laser has traveled */
	union vec3 pos = { { o->x, o->y, o->z } };
	vec3_sub_self(&pos, &o->birth_r);
	float dist = vec3_magnitude(&pos);

	float radius_scale = fmaxf(0.5, o->tsd.laser.power / 255.0);

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

	/* search in updates to find a "from" index that is in the past and a "to" index that is in the figure */
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
		/* all updates are older, maybe interp interp into future */
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
	double t = (target_time - o->updatetime[from_index]) /
			(o->updatetime[to_index] - o->updatetime[from_index]);

	interp_func(timestamp, o, visible, from_index, to_index, t);
}

void add_spark(double x, double y, double z, double vx, double vy, double vz, int time, int color,
		struct material *material, float shrink_factor, int debris_chance, float scale_factor);

/* make badly damaged ships "catch on fire" */
static void ship_emit_sparks(struct snis_entity *o)
{
	double vx, vy, vz;

	if (o->type == OBJTYPE_SHIP1)
		return;

	if (o->tsd.ship.damage.shield_damage < 200)
		return;

	if (o->tsd.ship.flames_timer <= 0)
		return;

	o->tsd.ship.flames_timer--;

	vx = (double) snis_randn(50) / 20.0;
	vy = (double) snis_randn(50) / 20.0;
	vz = (double) snis_randn(50) / 20.0;

	add_spark(o->x, o->y, o->z, vx, vy, vz, 5, YELLOW, &spark_material, 0.95, 0.0, 0.25);
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
		case OBJTYPE_SHIP2:
			move_object(timestamp, o, &interpolate_orientated_object);
			ship_emit_sparks(o);
			break;
		case OBJTYPE_WORMHOLE:
			move_object(timestamp, o, &interpolate_generic_object);
			spin_wormhole(timestamp, o);
			break;
		case OBJTYPE_STARBASE:
			move_object(timestamp, o, &interpolate_generic_object);
			spin_starbase(timestamp, o);
			break;
		case OBJTYPE_LASER:
			move_object(timestamp, o, &interpolate_laser);
			break;
		case OBJTYPE_TORPEDO:
			move_object(timestamp, o, &interpolate_orientated_object);
			break;
		case OBJTYPE_ASTEROID:
			move_object(timestamp, o, &interpolate_generic_object);
			spin_asteroid(timestamp, o);
			break;
		case OBJTYPE_CARGO_CONTAINER:
			move_object(timestamp, o, &interpolate_generic_object);
			spin_cargo_container(timestamp, o);
			break;
		case OBJTYPE_DERELICT:
			move_object(timestamp, o, &interpolate_generic_object);
			spin_derelict(timestamp, o);
			break;
		case OBJTYPE_LASERBEAM:
		case OBJTYPE_TRACTORBEAM:
			o->move(o);
			break;
		case OBJTYPE_PLANET:
		case OBJTYPE_NEBULA:
			move_object(timestamp, o, &interpolate_orientated_object);
			break;
		default:
			break;
		}
	}
}

void add_spark(double x, double y, double z, double vx, double vy, double vz, int time, int color,
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
	spark[i].x = x;
	spark[i].y = y;
	spark[i].z = z;
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

void add_warp_effect(double x, double y, double z, int arriving, int time,
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
	spark[i].x = x;
	spark[i].y = y;
	spark[i].z = z;
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

void add_shield_effect(double x, double y, double z,
			double vx, double vy, double vz,
			double radius, union quat *orientation)
{
	int i;
	struct entity *e;

	i = snis_object_pool_alloc_obj(sparkpool);
	if (i < 0)
		return;
	e = add_entity(ecx, sphere_mesh, x, y, z, PARTICLE_COLOR);
	if (e) {
		set_render_style(e, RENDER_NORMAL);
		update_entity_material(e, &shield_material);
		update_entity_scale(e, radius);
		update_entity_orientation(e, orientation);
		update_entity_visibility(e, 1);
		entity_update_alpha(e, 0.7);
	}
	memset(&spark[i], 0, sizeof(spark[i]));
	spark[i].x = x;
	spark[i].y = y;
	spark[i].z = z;
	spark[i].vx = vx;
	spark[i].vy = vx;
	spark[i].vz = vx;
	spark[i].type = OBJTYPE_SHIELD_EFFECT;
	spark[i].alive = SHIELD_EFFECT_LIFETIME;
	spark[i].move = shield_effect_move;
	spark[i].entity = e;
	return;
}


static void do_explosion(double x, double y, double z, uint16_t nsparks, uint16_t velocity, int time,
				uint8_t victim_type)
{
	double zangle, angle, v, vx, vy, vz;
	int i, color;

	switch (victim_type) {
	case OBJTYPE_SHIP1:
	case OBJTYPE_SHIP2:
	case OBJTYPE_DERELICT:
		color = SHIP_COLOR;
		break;
	case OBJTYPE_ASTEROID:
		color = ASTEROID_COLOR;
		break;
	case OBJTYPE_STARBASE:
		color = STARBASE_COLOR;
		break;
	default:
		color = GREEN;
		break;
	}

	for (i = 0; i < nsparks; i++) {
		angle = ((double) snis_randn(360) * M_PI / 180.0);
		zangle = ((double) snis_randn(360) * M_PI / 180.0);
		v = snis_randn(velocity * 2) - velocity;
		vx = v * cos(angle);
		vy = v * cos(zangle) / 3.0;
		vz = v * -sin(angle);
		add_spark(x, y, z, vx, vy, vz, time, color, &spark_material, 0.95, 50, 1.0);
	}
}

static int update_explosion(uint32_t id, uint32_t timestamp, double x, double y, double z,
		uint16_t nsparks, uint16_t velocity, uint16_t time, uint8_t victim_type)
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
		do_explosion(x, y, z, nsparks, velocity, (int) time, victim_type);
	}
	return 0;
}

void scale_points(struct my_point_t *points, int npoints,
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

/**********************************/
/* keyboard handling stuff begins */

struct keyname_value_entry {
	char *name;
	unsigned int value;
} keyname_value_map[] = {
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
}

static int remapkey(char *keyname, char *actionname)
{
	enum keyaction i;
	unsigned int j;
	int index;

	BUILD_ASSERT(ARRAY_SIZE(keyactionstring) == NKEYSTATES);

	for (i = keynone; i <= NKEYSTATES; i++) {
		if (strcmp(keyactionstring[i], actionname) != 0)
			continue;

		for (j=0;j<ARRAY_SIZE(keyname_value_map);j++) {
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

char *trim_whitespace(char *s)
{
	char *x, *z;

	for (x = s; *x == ' ' || *x == '\t'; x++)
		;
	z = x + (strlen(x) - 1);

	while (z >= x && (*z == ' ' ||  *z == '\t' || *z == '\n')) {
		*z = '\0';
		z--;
	}
	return x;
}

static void read_keymap_config_file(void)
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

static void wakeup_gameserver_writer(void);

static void queue_to_server(struct packed_buffer *pb)
{
	if (!pb) {
		stacktrace("snis_client: NULL packed_buffer in queue_to_server()");
		return;
	}
	packed_buffer_queue_add(&to_server_queue, pb, &to_server_queue_mutex);
	wakeup_gameserver_writer();
}

static void request_sci_select_target(uint32_t id)
{
	queue_to_server(packed_buffer_new("bw", OPCODE_SCI_SELECT_TARGET, id));
}

static void request_sci_select_coords(double ux, double uy)
{
	queue_to_server(packed_buffer_new("bSS", OPCODE_SCI_SELECT_COORDS,
			ux, (int32_t) UNIVERSE_DIM, uy, (int32_t) UNIVERSE_DIM));
}

static void request_navigation_yaw_packet(uint8_t yaw)
{
	queue_to_server(packed_buffer_new("bb", OPCODE_REQUEST_YAW, yaw));
}

static void request_navigation_thrust_packet(uint8_t thrust)
{
	queue_to_server(packed_buffer_new("bb", OPCODE_REQUEST_THRUST, thrust));
}

static void request_navigation_pitch_packet(uint8_t pitch)
{
	queue_to_server(packed_buffer_new("bb", OPCODE_REQUEST_PITCH, pitch));
}

static void request_navigation_roll_packet(uint8_t roll)
{
	queue_to_server(packed_buffer_new("bb", OPCODE_REQUEST_ROLL, roll));
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

static void request_demon_yaw_packet(uint32_t oid, uint8_t yaw)
{
	queue_to_server(packed_buffer_new("bwb", OPCODE_DEMON_YAW, oid, yaw));
}

static void request_demon_thrust_packet(uint32_t oid, uint8_t thrust)
{
	queue_to_server(packed_buffer_new("bwb",
				OPCODE_DEMON_THRUST, oid, thrust));
}

static struct demon_ui {
	float ux1, uy1, ux2, uy2;
	double selectedx, selectedz;
	int nselected;
#define MAX_DEMON_SELECTABLE 256
	uint32_t selected_id[MAX_DEMON_SELECTABLE];
	struct button *demon_exec_button;
	struct button *demon_ship_button;
	struct button *demon_starbase_button;
	struct button *demon_planet_button;
	struct button *demon_nebula_button;
	struct button *demon_spacemonster_button;
	struct button *demon_captain_button;
	struct button *demon_delete_button;
	struct button *demon_select_none_button;
	struct button *demon_torpedo_button;
	struct button *demon_phaser_button;
	struct snis_text_input_box *demon_input;
	char input[100];
	char error_msg[80];
	double ix, iz, ix2, iz2;
	int captain_of;
	int selectmode;
	int buttonmode;
	int move_from_x, move_from_y; /* mouse coords where move begins */
#define DEMON_BUTTON_NOMODE 0
#define DEMON_BUTTON_SHIPMODE 1
#define DEMON_BUTTON_STARBASEMODE 2
#define DEMON_BUTTON_PLANETMODE 3
#define DEMON_BUTTON_NEBULAMODE 4
#define DEMON_BUTTON_SPACEMONSTERMODE 5
#define DEMON_BUTTON_DELETE 6
#define DEMON_BUTTON_SELECTNONE 7
#define DEMON_BUTTON_CAPTAINMODE 8

} demon_ui;

static void demon_dirkey(int h, int v)
{
	uint8_t yaw, thrust;
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

	if (!h && !v)
		return;

	if (h) {
		yaw = h < 0 ? YAW_LEFT + fine : YAW_RIGHT + fine;
		request_demon_yaw_packet(oid, yaw);
	}
	if (v) {
		thrust = v < 0 ? THRUST_BACKWARDS : THRUST_FORWARDS;
		request_demon_thrust_packet(oid, thrust);
	}
}


static void request_weapons_yaw_packet(uint8_t yaw)
{
	queue_to_server(packed_buffer_new("bb", OPCODE_REQUEST_GUNYAW, yaw));
}

static void request_weapons_manual_yaw_packet(uint8_t yaw)
{
	queue_to_server(packed_buffer_new("bb", OPCODE_REQUEST_MANUAL_GUNYAW, yaw));
}

static void request_weapons_manual_pitch_packet(uint8_t pitch)
{
	queue_to_server(packed_buffer_new("bb", OPCODE_REQUEST_MANUAL_GUNPITCH, pitch));
}

struct weapons_ui {
	struct gauge *phaser_bank_gauge;
	struct gauge *phaser_wavelength;
	struct slider *wavelen_slider;
} weapons;

static void draw_plane_radar(GtkWidget *w, struct snis_entity *o, union quat *aim, float cx, float cy, float r, float range)
{
	int i;

	/* draw background overlay */
	sng_set_foreground_alpha(BLACK, 0.75);
	sng_draw_circle(1, cx, cy, r);
	sng_set_foreground(AMBER);
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

	union quat aim_conj;
	quat_conj(&aim_conj, aim);

	for (i = 0; i <= snis_object_pool_highest_object(pool); i++) {
		if (!go[i].alive)
			continue;

		if (go[i].id == my_ship_id)
			continue; /* skip drawing yourself. */

		if (go[i].type != OBJTYPE_SHIP1 &&
			go[i].type != OBJTYPE_SHIP2 &&
			go[i].type != OBJTYPE_STARBASE &&
			go[i].type != OBJTYPE_ASTEROID &&
			go[i].type != OBJTYPE_SPACEMONSTER &&
			go[i].type != OBJTYPE_CARGO_CONTAINER)
		{
			continue;
		}

		float dist2 = dist3dsqrd(go[i].x - o->x, go[i].y - o->y, go[i].z - o->z);
		if (dist2 > range2)
			continue; /* not close enough */

		float dist = sqrt(dist2);
		if (dist > range)
			continue;

		union vec3 contact_pos = {{go[i].x, go[i].y, go[i].z}};

		/* get direction vector, rotate back to basis axis, then normalize */
		union vec3 dir;
		vec3_sub(&dir, &contact_pos, &ship_pos);
		quat_rot_vec_self(&dir, &aim_conj);
		vec3_normalize(&dir, &dir);

		/* dist to forward vector */
		float d = dist3d(dir.v.x - 1.0, dir.v.y, dir.v.z) / 2.0;
		/* angle off center when looking towards +x */
		float twist = atan2(dir.v.y, dir.v.z);

		float sx = cx + r * cos(twist) * d * 0.98;
		float sy = cy - r * sin(twist) * d * 0.98;

		sng_set_foreground(ORANGERED);
		snis_draw_line(sx, sy - 2, sx, sy + 2);
		snis_draw_line(sx - 2, sy, sx + 2, sy);

		if (curr_science_guy == &go[i]) {
			sng_set_foreground(GREEN);
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

	if (!h && !v)
		return;
	if (v) {
		yaw = v < 0 ? YAW_LEFT : YAW_RIGHT;
		queue_to_server(packed_buffer_new("bb",
				OPCODE_REQUEST_SCIBEAMWIDTH, yaw));
	}
	if (h) {
		yaw = h < 0 ? YAW_LEFT : YAW_RIGHT;
		queue_to_server(packed_buffer_new("bb",
				OPCODE_REQUEST_SCIYAW, yaw));
	}
}

static void damcon_dirkey(int h, int v)
{
	uint8_t yaw, thrust;

	if (!h && !v)
		return;
	if (v) {
		thrust = v < 0 ? THRUST_BACKWARDS : THRUST_FORWARDS;
		queue_to_server(packed_buffer_new("bb",
				OPCODE_REQUEST_ROBOT_THRUST, thrust));
	}
	if (h) {
		yaw = h > 0 ? YAW_LEFT : YAW_RIGHT;
		queue_to_server(packed_buffer_new("bb",
				OPCODE_REQUEST_ROBOT_YAW, yaw));
	}
	wakeup_gameserver_writer();
}

static void do_onscreen(uint8_t mode)
{
	queue_to_server(packed_buffer_new("bb", OPCODE_ROLE_ONSCREEN, mode));
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
	queue_to_server(packed_buffer_new("bRb", OPCODE_MAINSCREEN_VIEW_MODE,
				0.0, new_mode));
}

static void do_dirkey(int h, int v, int r)
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
		case DISPLAYMODE_NAVIGATION:
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
			demon_dirkey(h, v);
			break;
		default:
			break;
	}
	return;
}

static void do_sciball_dirkey(int h, int v, int r)
{
	uint8_t value;

	v = v * vertical_controls_inverted;

	switch (displaymode) {
		case DISPLAYMODE_SCIENCE:
			if (!h && !v && !r)
				return;
			if (v) {
				value = v < 0 ? YAW_LEFT : YAW_RIGHT;
				queue_to_server(packed_buffer_new("bb",
						OPCODE_REQUEST_SCIBALL_PITCH, value));
			}
			if (h) {
				value = h < 0 ? YAW_LEFT : YAW_RIGHT;
				queue_to_server(packed_buffer_new("bb",
						OPCODE_REQUEST_SCIBALL_YAW, value));
			}
			if (r) {
				value = r < 0 ? YAW_LEFT : YAW_RIGHT;
				queue_to_server(packed_buffer_new("bb",
						OPCODE_REQUEST_SCIBALL_ROLL, value));
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

	if (displaymode != DISPLAYMODE_WEAPONS)
		return;
	if (!(o = find_my_ship()))
		return;
	if (o->tsd.ship.torpedoes_loaded <= 0)
		return;
	queue_to_server(packed_buffer_new("b", OPCODE_REQUEST_TORPEDO));
}

static void do_mainscreen_camera_mode()
{
	if (displaymode != DISPLAYMODE_MAINSCREEN)
		return;
	queue_to_server(packed_buffer_new("bb", OPCODE_CYCLE_MAINSCREEN_POINT_OF_VIEW,
		(unsigned char) (camera_mode + 1) % 3));
}

static void robot_gripper_button_pressed(void *x);
static void do_laser(void)
{
	switch (displaymode) {
	case DISPLAYMODE_WEAPONS: 
		queue_to_server(packed_buffer_new("b", OPCODE_REQUEST_MANUAL_LASER));
		break;
	case DISPLAYMODE_DAMCON:
		robot_gripper_button_pressed(NULL);
		break;
	default:
		break;
	}
}

static void robot_backward_button_pressed(void *x);
static void robot_forward_button_pressed(void *x);
static void robot_left_button_pressed(void *x);
static void robot_right_button_pressed(void *x);
static void robot_auto_button_pressed(void *x);
static void robot_manual_button_pressed(void *x);

static void fire_phaser_button_pressed(__attribute__((unused)) void *notused);
static void joystick_button_zero(__attribute__((unused)) void *x)
{
	switch (displaymode) {
	case DISPLAYMODE_WEAPONS:
		fire_phaser_button_pressed(NULL);
		break;
	case DISPLAYMODE_DAMCON:
		robot_gripper_button_pressed(NULL);
		break;
	default:
		break;
	}	
}

static void fire_torpedo_button_pressed(__attribute__((unused)) void *notused);
static void joystick_button_one(__attribute__((unused)) void *x)
{
	switch (displaymode) {
	case DISPLAYMODE_WEAPONS:
		fire_torpedo_button_pressed(NULL);
		break;
	default:
		break;
	}	
}

static void load_torpedo_button_pressed();
static void joystick_button_two(__attribute__((unused)) void *x)
{
	switch (displaymode) {
	case DISPLAYMODE_WEAPONS:
		load_torpedo_button_pressed();
		break;
	default:
		break;
	}	
}

static joystick_button_fn do_joystick_button[] = {
		joystick_button_zero,
		joystick_button_one,
		joystick_button_two,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
};

/* client joystick status */
static struct wwvi_js_event jse = { { 0 } };

static void deal_with_joystick()
{

#define FRAME_RATE_HZ 30 
	static const int joystick_throttle = (int) ((FRAME_RATE_HZ / 15.0) + 0.5);
	int i, rc;

	if (joystick_fd < 0) /* no joystick */
		return;

#define XJOYSTICK_THRESHOLD 23000
#define YJOYSTICK_THRESHOLD 23000
#define XJOYSTICK_THRESHOLD_FINE 6000

	/* Read events even if we don't use them just to consume them. */
	memset(&jse.button, 0, sizeof(jse.button));
	rc = get_joystick_status(&jse);
	if (rc != 0)
		return;

	/* If not on screen which uses joystick, ignore stick */
	if (displaymode != DISPLAYMODE_DAMCON &&
		displaymode != DISPLAYMODE_WEAPONS &&
		displaymode != DISPLAYMODE_NAVIGATION)
		return;

	/* Check joystick buttons (need to throttle this?) */
	for (i = 0; i < ARRAYSIZE(jse.button); i++) {
		if (jse.button[i] == 1 && do_joystick_button[i])
			do_joystick_button[i](NULL);
	}

	/* Throttle back the joystick axis input rate to avoid flooding network */
	if (timer % joystick_throttle != 0)
		return;

	switch (displaymode) {
	case DISPLAYMODE_DAMCON:
		if (jse.stick_x < -XJOYSTICK_THRESHOLD)
			robot_left_button_pressed(NULL);
			break;
		if (jse.stick_x > XJOYSTICK_THRESHOLD)
			robot_right_button_pressed(NULL);
			break;
		if (jse.stick_y < -YJOYSTICK_THRESHOLD)
			robot_forward_button_pressed(NULL);
			break;
		if (jse.stick_y > YJOYSTICK_THRESHOLD)
			robot_backward_button_pressed(NULL);
			break;
		break;
	case DISPLAYMODE_WEAPONS:
		if (jse.stick_x < -XJOYSTICK_THRESHOLD) {
			request_weapons_yaw_packet(YAW_LEFT);
			break;
		}
		if (jse.stick_x > XJOYSTICK_THRESHOLD) {
			request_weapons_yaw_packet(YAW_RIGHT);
			break;
		}
		if (jse.stick_x < -XJOYSTICK_THRESHOLD_FINE) {
			request_weapons_yaw_packet(YAW_LEFT + 2);
			break;
		}
		if (jse.stick_x > XJOYSTICK_THRESHOLD_FINE) {
			request_weapons_yaw_packet(YAW_RIGHT + 2);
			break;
		}
		break;
	case DISPLAYMODE_NAVIGATION:
		if (jse.stick_x < -XJOYSTICK_THRESHOLD) {
			request_navigation_yaw_packet(YAW_LEFT);
			goto nav_check_y_stick;
		}
		if (jse.stick_x > XJOYSTICK_THRESHOLD) {
			request_navigation_yaw_packet(YAW_RIGHT);
			goto nav_check_y_stick;
		}
		if (jse.stick_x < -XJOYSTICK_THRESHOLD_FINE) {
			request_navigation_yaw_packet(YAW_LEFT + 2);
			goto nav_check_y_stick;
		}
		if (jse.stick_x > XJOYSTICK_THRESHOLD_FINE) {
			request_navigation_yaw_packet(YAW_RIGHT + 2);
			goto nav_check_y_stick;
		}
nav_check_y_stick:
		if (jse.stick2_y > YJOYSTICK_THRESHOLD) {
			request_navigation_thrust_packet(THRUST_FORWARDS);
			break;
		}
		if (jse.stick2_y < -YJOYSTICK_THRESHOLD) {
			request_navigation_thrust_packet(THRUST_BACKWARDS);
			break;
		}
		break;
	default:
		break;
	}
}

static void deal_with_physical_io_devices()
{
	/* FIXME: fill this in. */
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
                do_adjust_byte_value((uint8_t) newval, OPCODE_REQUEST_NAVZOOM);
		break;
	case DISPLAYMODE_MAINSCREEN:
		newval = o->tsd.ship.mainzoom + z;
		if (newval < 0)
			newval = 0;
		if (newval > 255)
			newval = 255;
                do_adjust_byte_value((uint8_t) newval, OPCODE_REQUEST_MAINZOOM);
		break;
	default:
		break;
	}
}

static void deal_with_keyboard()
{
	int h, v, z, r;
	int sbh, sbv, sbr; /* sciball keys */

	static const int keyboard_throttle = (int) ((FRAME_RATE_HZ / 15.0) + 0.5);
	if (timer % keyboard_throttle != 0)
		return;

	h = 0;
	v = 0;
	r = 0;
	z = 0;
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
	if (h || v || r)
		do_dirkey(h, v, r);

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

	if (sbh || sbv || sbr)
		do_sciball_dirkey(sbh, sbv, sbr);
}

static int control_key_pressed = 0;

static gint key_press_cb(GtkWidget* widget, GdkEventKey* event, gpointer data)
{
	enum keyaction ka;

	ui_element_list_keypress(uiobjs, event);
        if ((event->keyval & 0xff00) == 0) 
                ka = keymap[event->keyval];
        else
                ka = ffkeymap[event->keyval & 0x00ff];
	if (ka > 0 && ka < NKEYSTATES)
		kbstate.pressed[ka] = 1;

	if (event->keyval == GDK_Control_R ||
		event->keyval == GDK_Control_L) {
		control_key_pressed = 1;
		return TRUE;
	}

#if 0
	printf("event->keyval = 0x%08x, GDK_z = %08x, GDK_space = %08x\n", event->keyval, GDK_z, GDK_space);
#endif

        switch (ka) {
	case key_invert_vertical:
			if (control_key_pressed) {
				vertical_controls_inverted *= -1;
				vertical_controls_timer = FRAME_RATE_HZ;
			}
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
		do_torpedo();
		load_torpedo_button_pressed();
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
	case key_camera_mode:
		do_mainscreen_camera_mode();
		break;
	case keyf1:
		if (!helpmode)
			helpmodeline = 0;
		helpmode = 1;
		break;
	case keyf2:
		if (displaymode >= DISPLAYMODE_FONTTEST)
			break;
		if (role & ROLE_NAVIGATION) {
			displaymode = DISPLAYMODE_NAVIGATION;
			wwviaudio_add_sound(CHANGESCREEN_SOUND);
		}
		break;
	case keyf3:
		if (displaymode >= DISPLAYMODE_FONTTEST)
			break;
		if (role & ROLE_WEAPONS) {
			displaymode = DISPLAYMODE_WEAPONS;
			wwviaudio_add_sound(CHANGESCREEN_SOUND);
		}
		break;
	case keyf4:
		if (displaymode >= DISPLAYMODE_FONTTEST)
			break;
		if (role & ROLE_ENGINEERING) {
			displaymode = DISPLAYMODE_ENGINEERING;
			wwviaudio_add_sound(CHANGESCREEN_SOUND);
		}
		break;
	case keyf5:
		if (displaymode >= DISPLAYMODE_FONTTEST)
			break;
		if (role & ROLE_DAMCON) {
			displaymode = DISPLAYMODE_DAMCON;
			wwviaudio_add_sound(CHANGESCREEN_SOUND);
		}
		break;
	case keyf6:
		if (displaymode >= DISPLAYMODE_FONTTEST)
			break;
		if (role & ROLE_SCIENCE) {
			displaymode = DISPLAYMODE_SCIENCE;
			wwviaudio_add_sound(CHANGESCREEN_SOUND);
		}
		break;
	case keyf7:
		if (displaymode >= DISPLAYMODE_FONTTEST)
			break;
		if (role & ROLE_COMMS) {
			displaymode = DISPLAYMODE_COMMS;
			wwviaudio_add_sound(CHANGESCREEN_SOUND);
		}
		break;
	case keyf8:
		if (displaymode >= DISPLAYMODE_FONTTEST)
			break;
		if (role & ROLE_MAIN) {
			displaymode = DISPLAYMODE_MAINSCREEN;
			wwviaudio_add_sound(CHANGESCREEN_SOUND);
		}
		break;
	case keyf9:
		if (displaymode >= DISPLAYMODE_FONTTEST)
			break;
		if (role & ROLE_DEMON) {
			displaymode = DISPLAYMODE_DEMON;
			wwviaudio_add_sound(CHANGESCREEN_SOUND);
		}
		break;
	case keyf10:
		if (displaymode >= DISPLAYMODE_FONTTEST)
			break;
		if (role & ROLE_MAIN) {
			displaymode = DISPLAYMODE_GLMAIN;
			wwviaudio_add_sound(CHANGESCREEN_SOUND);
		}
		break;
	case keyonscreen:
		if (control_key_pressed)
			do_onscreen((uint8_t) displaymode & 0xff);
		break;
	case keyviewmode:
		if (displaymode != DISPLAYMODE_MAINSCREEN &&
			displaymode != DISPLAYMODE_WEAPONS &&
			displaymode != DISPLAYMODE_NAVIGATION)
			break;
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
		if (displaymode != DISPLAYMODE_MAINSCREEN)
			break;
		r = (r + 1) % ARRAY_SIZE(valid_combos);
		set_renderer(ecx, valid_combos[r]);
		break;
		}
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
                ka = keymap[event->keyval];
        else
                ka = ffkeymap[event->keyval & 0x00ff];
	if (ka > 0 && ka < NKEYSTATES)
		kbstate.pressed[ka] = 0;

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
	sng_abs_xy_draw_string("Space Nerds", BIG_FONT, 80, 200);
	sng_abs_xy_draw_string("In Space", BIG_FONT, 180, 320);
	sng_abs_xy_draw_string("Copyright (C) 2010 Stephen M. Cameron", NANO_FONT, 255, 550);
}

int lobbylast1clickx = -1;
int lobbylast1clicky = -1;
int lobby_selected_server = -1;
static struct lobby_ui {
	struct button *lobby_cancel_button;
} lobby_ui;

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

static void show_lobbyscreen(GtkWidget *w)
{
	char msg[100];
	int i;
#define STARTLINE 100
#define LINEHEIGHT 30

	sng_set_foreground(WHITE);
	if (lobby_socket == -1) {
		sng_abs_xy_draw_string("Space Nerds", BIG_FONT, 80, 200);
		sng_abs_xy_draw_string("In Space", BIG_FONT, 180, 320);
		sng_abs_xy_draw_string("Copyright (C) 2010 Stephen M. Cameron", NANO_FONT, 255, 550);
		if (lobby_count >= MAX_LOBBY_TRIES)
			sprintf(msg, "Giving up on lobby... tried %d times.",
				lobby_count);
		else
			sprintf(msg, "Connecting to lobby... tried %d times.",
				lobby_count);
		sng_abs_xy_draw_string(msg, SMALL_FONT, 100, 400);
		sng_abs_xy_draw_string(lobbyerror, NANO_FONT, 100, 430);
	} else {
		if (lobby_selected_server != -1 &&
			lobbylast1clickx > 200 && lobbylast1clickx < 620 &&
			lobbylast1clicky > 520 && lobbylast1clicky < 520 + LINEHEIGHT * 2) {
			displaymode = DISPLAYMODE_CONNECTING;
			return;
		}

		if (lobby_selected_server != -1 && quickstartmode) {
			displaymode = DISPLAYMODE_CONNECTING;
			return;
		}

		lobby_selected_server = -1;
		sprintf(msg, "Connected to lobby on socket %d\n", lobby_socket);
		sng_abs_xy_draw_string(msg, TINY_FONT, 30, LINEHEIGHT);
		sprintf(msg, "Total game servers: %d\n", ngameservers);
		sng_abs_xy_draw_string(msg, TINY_FONT, 30, LINEHEIGHT + 20);
		for (i = 0; i < ngameservers; i++) {
			unsigned char *x = (unsigned char *) 
				&lobby_game_server[i].ipaddr;
			if (lobbylast1clickx > 30 && lobbylast1clickx < 700 &&
				lobbylast1clicky > 100 + (-0.5 + i) * LINEHEIGHT &&
				lobbylast1clicky < 100 + (0.5 + i) * LINEHEIGHT) {
				lobby_selected_server = i;
				sng_set_foreground(GREEN);
				snis_draw_rectangle(0, 25, 100 + (-0.5 + i) * LINEHEIGHT,
					725, LINEHEIGHT);
			} else
				sng_set_foreground(WHITE);

			if (quickstartmode) {
				lobby_selected_server = 0;
			}
			 
			sprintf(msg, "%hu.%hu.%hu.%hu/%hu", x[0], x[1], x[2], x[3], lobby_game_server[i].port);
			sng_abs_xy_draw_string(msg, TINY_FONT, 30, 100 + i * LINEHEIGHT);
			sprintf(msg, "%s", lobby_game_server[i].game_instance);
			sng_abs_xy_draw_string(msg, TINY_FONT, 350, 100 + i * LINEHEIGHT);
			sprintf(msg, "%s", lobby_game_server[i].server_nickname);
			sng_abs_xy_draw_string(msg, TINY_FONT, 450, 100 + i * LINEHEIGHT);
			sprintf(msg, "%s", lobby_game_server[i].location);
			sng_abs_xy_draw_string(msg, TINY_FONT, 650, 100 + i * LINEHEIGHT);
			sprintf(msg, "%d", lobby_game_server[i].nconnections);
			sng_abs_xy_draw_string(msg, TINY_FONT, 700, 100 + i * LINEHEIGHT);
		}
		if (lobby_selected_server != -1)
			sng_set_foreground(GREEN);
		else
			sng_set_foreground(RED);
		/* This should be a real button, but I'm too lazy to fix it now. */
		snis_draw_rectangle(0, 250, 520, 300, LINEHEIGHT * 2);
		sng_abs_xy_draw_string("CONNECT TO SERVER", TINY_FONT, 280, 520 + LINEHEIGHT);
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

static int process_update_ship_packet(uint8_t opcode)
{
	int i;
	unsigned char buffer[144];
	struct packed_buffer pb;
	uint16_t alive;
	uint32_t id, timestamp, torpedoes, power;
	uint32_t fuel, victim_id;
	double dx, dy, dz, dyawvel, dpitchvel, drollvel;
	double dgunyawvel, dsheading, dbeamwidth;
	int rc;
	int type = opcode == OPCODE_UPDATE_SHIP ? OBJTYPE_SHIP1 : OBJTYPE_SHIP2;
	uint8_t tloading, tloaded, throttle, rpm, temp, scizoom, weapzoom, navzoom,
		mainzoom, warpdrive, requested_warpdrive,
		requested_shield, phaser_charge, phaser_wavelength, shiptype,
		reverse, in_secure_area;
	union quat orientation, sciball_orientation, weap_orientation;
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
	packed_buffer_extract(&pb, "RRRwwRRR",
				&dyawvel,
				&dpitchvel,
				&drollvel,
				&torpedoes, &power,
				&dgunyawvel,
				&dsheading,
				&dbeamwidth);
	packed_buffer_extract(&pb, "bbbwbbbbbbbbbbbbwQQQb",
			&tloading, &throttle, &rpm, &fuel, &temp,
			&scizoom, &weapzoom, &navzoom, &mainzoom,
			&warpdrive, &requested_warpdrive,
			&requested_shield, &phaser_charge, &phaser_wavelength, &shiptype,
			&reverse, &victim_id, &orientation.vec[0],
			&sciball_orientation.vec[0], &weap_orientation.vec[0], &in_secure_area);
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
			if (e)
				add_ship_thrust_entities(NULL, NULL, ecx, e, shiptype, 36);
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
	o->tsd.ship.gun_yaw_velocity = dgunyawvel;
	o->tsd.ship.sci_heading = dsheading;
	o->tsd.ship.sci_beam_width = dbeamwidth;
	o->tsd.ship.torpedoes_loaded = tloaded;
	o->tsd.ship.torpedoes_loading = tloading;
	o->tsd.ship.throttle = throttle;
	o->tsd.ship.rpm = rpm;
	o->tsd.ship.fuel = fuel;
	o->tsd.ship.temp = temp;
	o->tsd.ship.scizoom = scizoom;
	o->tsd.ship.weapzoom = weapzoom;
	o->tsd.ship.navzoom = navzoom;
	o->tsd.ship.mainzoom = mainzoom;
	o->tsd.ship.requested_warpdrive = requested_warpdrive;
	o->tsd.ship.requested_shield = requested_shield;
	o->tsd.ship.warpdrive = warpdrive;
	o->tsd.ship.phaser_charge = phaser_charge;
	o->tsd.ship.phaser_wavelength = phaser_wavelength;
	o->tsd.ship.damcon = NULL;
	o->tsd.ship.shiptype = shiptype;
	o->tsd.ship.in_secure_area = in_secure_area;

	/* shift old updates to make room for this one */
	int j;
	for (j = SNIS_ENTITY_NUPDATE_HISTORY - 1; j >= 1; j--) {
		o->tsd.ship.sciball_o[j] = o->tsd.ship.sciball_o[j-1];
		o->tsd.ship.weap_o[j] = o->tsd.ship.weap_o[j-1];
	}
	o->tsd.ship.sciball_o[0] = sciball_orientation;
	o->tsd.ship.weap_o[0] = weap_orientation;

	if (!o->tsd.ship.reverse && reverse)
		wwviaudio_add_sound(REVERSE_SOUND);
	o->tsd.ship.reverse = reverse;
	o->tsd.ship.ai[0].u.attack.victim_id = victim_id;
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
	uint16_t alive;
	uint32_t id, timestamp, victim_id;
	double dx, dy, dz, px, py, pz;
	union quat orientation;
	uint8_t shiptype, ai[MAX_AI_STACK_ENTRIES], npoints;
	union vec3 patrol[MAX_PATROL_POINTS];
	double threat_level;
	int rc;

	assert(sizeof(buffer) > sizeof(struct update_econ_ship_packet) - sizeof(uint8_t));
	rc = read_and_unpack_buffer(buffer, "wwhSSSQwb", &id, &timestamp, &alive,
				&dx, (int32_t) UNIVERSE_DIM, &dy, (int32_t) UNIVERSE_DIM, 
				&dz, (int32_t) UNIVERSE_DIM,
				&orientation,
				&victim_id, &shiptype);
	if (rc != 0)
		return rc;
	if (opcode != OPCODE_ECON_UPDATE_SHIP_DEBUG_AI) {
		memset(ai, 0, 5);
		npoints = 0;
		threat_level = 0.0;
		goto done;
	}
	BUILD_ASSERT(MAX_AI_STACK_ENTRIES == 5);
	rc = read_and_unpack_buffer(buffer, "bbbbbSb",
			&ai[0], &ai[1], &ai[2], &ai[3], &ai[4],
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
	rc = update_econ_ship(id, timestamp, dx, dy, dz, &orientation, alive, victim_id,
				shiptype, ai, threat_level, npoints, patrol);
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

static int process_warp_limbo_packet(void)
{
	unsigned char buffer[sizeof(struct warp_limbo_packet)];
	int rc;
	uint16_t value;

	rc = read_and_unpack_buffer(buffer, "h", &value);
	if (rc != 0)
		return rc;
	if (value >= 0 && value <= 40 * frame_rate_hz)
		warp_limbo_countdown = value;
	return 0;
} 

static void process_initiate_warp_packet()
{
	wwviaudio_add_sound(WARPDRIVE_SOUND);
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
	uint32_t id, timestamp;
	double dx, dy, dz;
	int rc;

	assert(sizeof(buffer) > sizeof(struct update_spacemonster_packet) - sizeof(uint8_t));
	rc = read_and_unpack_buffer(buffer, "wwSSS", &id, &timestamp,
				&dx, (int32_t) UNIVERSE_DIM, &dy, (int32_t) UNIVERSE_DIM,
				&dz, (int32_t) UNIVERSE_DIM);
	if (rc != 0)
		return rc;
	pthread_mutex_lock(&universe_mutex);
	rc = update_spacemonster(id, timestamp, dx, dy, dz);
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

static struct main_screen_text_data {
	char text[4][100];
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
	strcpy(skybox_texture_prefix, string);
	textures_loaded = 0;

	return 0;
}

static int process_cycle_mainscreen_point_of_view(void)
{
	int rc;
	unsigned char buffer[10];
	unsigned char new_mode;

	rc = read_and_unpack_buffer(buffer, "b", &new_mode);
	if (rc != 0)
		return rc;
	pthread_mutex_lock(&universe_mutex);
	camera_mode = new_mode % 3;
	pthread_mutex_unlock(&universe_mutex);
	return 0;
}

static int process_add_warp_effect(void)
{
	int rc;
	unsigned char buffer[100];
	double ox, oy, oz, dx, dy, dz;
	float dist;
	union vec3 direction;

	rc = read_and_unpack_buffer(buffer, "SSSSSS",
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

	add_shield_effect(o->x, o->y, o->z, o->vx, o->vy, o->vz, radius, &orientation);
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

static int process_proximity_alert()
{
	static int last_time = 0;

	if ((timer - last_time) > (30 * 3))
		wwviaudio_add_sound(PROXIMITY_ALERT);
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

	i = lookup_object_by_id(id);
	/* perhaps we just joined and so don't know about this object. */
	if (i < 0)
		return;
	go[i].alive = 0;
	remove_entity(ecx, go[i].entity);
	go[i].entity = NULL;
	free_spacemonster_data(&go[i]);
	free_laserbeam_data(&go[i]);
	go[i].id = -1;
	snis_object_pool_free_object(pool, i);
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
	packed_buffer_unpack(buffer, "wbbbbbbbr",&id, &subclass, &shstrength, &shwavelength,
			&shwidth, &shdepth, &faction, &lifeform_count,
			name, (unsigned short) sizeof(name));
	pthread_mutex_lock(&universe_mutex);
	update_ship_sdata(id, subclass, name, shstrength, shwavelength, shwidth, shdepth, faction, lifeform_count);
	pthread_mutex_unlock(&universe_mutex);
	return 0;
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
	int details_mode;
#define SCI_DETAILS_MODE_THREED 1
#define SCI_DETAILS_MODE_DETAILS 2
#define SCI_DETAILS_MODE_SCIPLANE 3
	struct slider *scizoom;
	struct slider *scipower;
	struct button *details_button;
	struct button *threed_button;
	struct button *sciplane_button;
	struct button *tractor_button;
} sci_ui;

static int process_sci_details(void)
{
	unsigned char buffer[10];
	uint8_t new_details;
	int rc;

	rc = read_and_unpack_buffer(buffer, "b", &new_details);
	if (rc != 0)
		return rc;
	if (new_details == 0)
		new_details = SCI_DETAILS_MODE_SCIPLANE;
	sci_ui.details_mode = new_details;
	return 0;
}

static struct navigation_ui {
	struct slider *warp_slider;
	struct slider *navzoom_slider;
	struct slider *throttle_slider;
	struct gauge *warp_gauge;
	struct button *engage_warp_button;
	struct button *reverse_button;
} nav_ui;

static int process_sci_select_target_packet(void)
{
	unsigned char buffer[sizeof(struct snis_sci_select_target_packet)];
	uint32_t id;
	int rc, i;

	rc = read_and_unpack_buffer(buffer, "w", &id);
	if (rc != 0)
		return rc;

	if (id == (uint32_t) -1) { /* deselection */
		curr_science_guy = NULL;
		wwviaudio_add_sound(SCIENCE_DATA_ACQUIRED_SOUND);
		return 0;
	}

	i = lookup_object_by_id(id);
	if (i >= 0) {
		curr_science_guy = &go[i];
		wwviaudio_add_sound(SCIENCE_DATA_ACQUIRED_SOUND);
	}
	return 0;
}

static int process_sci_select_coords_packet(void)
{
	unsigned char buffer[sizeof(struct snis_sci_select_coords_packet)];
	struct snis_entity *o;
	double ux, uz;
	int rc;

	rc = read_and_unpack_buffer(buffer, "SS",
			&ux, (int32_t) UNIVERSE_DIM,
			&uz, (int32_t) UNIVERSE_DIM); 
	if (rc != 0)
		return rc;
	if (!(o = find_my_ship()))
		return 0;
	o->sci_coordx = ux;	
	o->sci_coordz = uz;	
	return 0;
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
	int rc;

	rc = read_and_unpack_buffer(buffer, "qqwwwwwwww", &netstats.bytes_sent,
				&netstats.bytes_recd, &netstats.nobjects,
				&netstats.nships, &netstats.elapsed_seconds,
				&netstats.faction_population[0],
				&netstats.faction_population[1],
				&netstats.faction_population[2],
				&netstats.faction_population[3],
				&netstats.faction_population[4]);
	if (rc != 0)
		return rc;
	return 0;
}

struct comms_ui {
	struct text_window *tw;
	struct button *comms_onscreen_button;
	struct button *nav_onscreen_button;
	struct button *weap_onscreen_button;
	struct button *eng_onscreen_button;
	struct button *damcon_onscreen_button;
	struct button *sci_onscreen_button;
	struct button *main_onscreen_button;
	struct button *comms_transmit_button;
	struct button *red_alert_button;
	struct button *mainscreen_comms;
	struct snis_text_input_box *comms_input;
	struct slider *mainzoom_slider;
	char input[100];
} comms_ui;

static void main_screen_add_text(char *msg);
static int process_comm_transmission(void)
{
	unsigned char buffer[sizeof(struct comms_transmission_packet) + 100];
	char string[256];
	uint8_t length;
	int rc;

	rc = read_and_unpack_buffer(buffer, "b", &length);
	if (rc != 0)
		return rc;
	rc = snis_readsocket(gameserver_sock, string, length);
	string[79] = '\0';
	string[length] = '\0';
	text_window_add_text(comms_ui.tw, string);
	main_screen_add_text(string);
	if (strstr(string, ": *** HAILING "))
		wwviaudio_add_sound(COMMS_HAIL_SOUND);
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
	go[i].tsd.ship.flames_timer = 15 * 30; /* 15 secs of possible flames */
	pthread_mutex_unlock(&universe_mutex);
	if (id == my_ship_id && do_damage_limbo) 
		damage_limbo_countdown = 2;
	return 0;
}

static int process_update_asteroid_packet(void)
{
	unsigned char buffer[100];
	uint32_t id, timestamp;
	double dx, dy, dz, dvx, dvy, dvz;
	int rc;

	assert(sizeof(buffer) > sizeof(struct update_asteroid_packet) - sizeof(uint8_t));
	rc = read_and_unpack_buffer(buffer, "wwSSSSSS", &id, &timestamp,
			&dx, (int32_t) UNIVERSE_DIM,
			&dy,(int32_t) UNIVERSE_DIM,
			&dz, (int32_t) UNIVERSE_DIM,
			&dvx, (int32_t) UNIVERSE_DIM,
			&dvy, (int32_t) UNIVERSE_DIM,
			&dvz, (int32_t) UNIVERSE_DIM);
	if (rc != 0)
		return rc;
	pthread_mutex_lock(&universe_mutex);
	rc = update_asteroid(id, timestamp, dx, dy, dz, dvx, dvy, dvz);
	pthread_mutex_unlock(&universe_mutex);
	return (rc < 0);
} 

static int process_update_cargo_container_packet(void)
{
	unsigned char buffer[100];
	uint32_t id, timestamp;
	double dx, dy, dz;
	int rc;

	assert(sizeof(buffer) > sizeof(struct update_cargo_container_packet) - sizeof(uint8_t));
	rc = read_and_unpack_buffer(buffer, "wwSSS", &id, &timestamp,
			&dx, (int32_t) UNIVERSE_DIM,
			&dy,(int32_t) UNIVERSE_DIM,
			&dz, (int32_t) UNIVERSE_DIM);
	if (rc != 0)
		return rc;
	pthread_mutex_lock(&universe_mutex);
	rc = update_cargo_container(id, timestamp, dx, dy, dz);
	pthread_mutex_unlock(&universe_mutex);
	return (rc < 0);
} 

static int process_update_derelict_packet(void)
{
	unsigned char buffer[100];
	uint32_t id, timestamp;
	double dx, dy, dz;
	uint8_t shiptype;
	int rc;

	assert(sizeof(buffer) > sizeof(struct update_asteroid_packet) - sizeof(uint8_t));
	rc = read_and_unpack_buffer(buffer, "wwSSSb", &id, &timestamp,
			&dx, (int32_t) UNIVERSE_DIM,
			&dy,(int32_t) UNIVERSE_DIM,
			&dz, (int32_t) UNIVERSE_DIM, &shiptype);
	if (rc != 0)
		return rc;
	pthread_mutex_lock(&universe_mutex);
	rc = update_derelict(id, timestamp, dx, dy, dz, shiptype);
	pthread_mutex_unlock(&universe_mutex);
	return (rc < 0);
} 

static int process_update_planet_packet(void)
{
	unsigned char buffer[100];
	uint32_t id, timestamp;
	double dr, dx, dy, dz;
	uint8_t government, tech_level, economy, security;
	uint32_t dseed;
	int hasring;
	int rc;

	assert(sizeof(buffer) > sizeof(struct update_asteroid_packet) - sizeof(uint8_t));
	rc = read_and_unpack_buffer(buffer, "wwSSSSwbbbb", &id, &timestamp,
			&dx, (int32_t) UNIVERSE_DIM,
			&dy,(int32_t) UNIVERSE_DIM,
			&dz, (int32_t) UNIVERSE_DIM,
			&dr, (int32_t) UNIVERSE_DIM,
			&dseed, &government, &tech_level, &economy, &security);
	if (rc != 0)
		return rc;
	hasring = (dr < 0);
	if (hasring)
		dr = -dr;
	pthread_mutex_lock(&universe_mutex);
	rc = update_planet(id, timestamp, dx, dy, dz, dr, government, tech_level,
				economy, dseed, hasring, security);
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
	int rc;

	assert(sizeof(buffer) > sizeof(struct update_starbase_packet) - sizeof(uint8_t));
	rc = read_and_unpack_buffer(buffer, "wwSSS", &id, &timestamp,
			&dx, (int32_t) UNIVERSE_DIM, &dy, (int32_t) UNIVERSE_DIM, &dz, (int32_t) UNIVERSE_DIM);
	if (rc != 0)
		return rc;
	pthread_mutex_lock(&universe_mutex);
	rc = update_starbase(id, timestamp, dx, dy, dz);
	pthread_mutex_unlock(&universe_mutex);
	return (rc < 0);
} 

static int process_update_nebula_packet(void)
{
	unsigned char buffer[100];
	uint32_t id, timestamp;
	double dx, dy, dz, r;
	int rc;

	assert(sizeof(buffer) > sizeof(struct update_nebula_packet) - sizeof(uint8_t));
	rc = read_and_unpack_buffer(buffer, "wwSSSS", &id, &timestamp,
			&dx, (int32_t) UNIVERSE_DIM,
			&dy, (int32_t) UNIVERSE_DIM,
			&dz, (int32_t) UNIVERSE_DIM,
			&r, (int32_t) UNIVERSE_DIM);
	if (rc != 0)
		return rc;
	pthread_mutex_lock(&universe_mutex);
	rc = update_nebula(id, timestamp, dx, dy, dz, r);
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
	uint32_t id, timestamp;
	double dx, dy, dz;
	uint16_t nsparks, velocity, time;
	uint8_t victim_type;
	int rc;

	assert(sizeof(buffer) > sizeof(struct update_explosion_packet) - sizeof(uint8_t));
	rc = read_and_unpack_buffer(buffer, "wwSSShhhb", &id, &timestamp,
		&dx, (int32_t) UNIVERSE_DIM, &dy, (int32_t) UNIVERSE_DIM,
		&dz, (int32_t) UNIVERSE_DIM,
		&nsparks, &velocity, &time, &victim_type);
	if (rc != 0)
		return rc;
	pthread_mutex_lock(&universe_mutex);
	rc = update_explosion(id, timestamp, dx, dy, dz, nsparks, velocity, time, victim_type);
	pthread_mutex_unlock(&universe_mutex);
	return (rc < 0);
}

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
	printf("SET MY SHIP ID to %u\n", my_ship_id);
	return 0;
}

static void *gameserver_reader(__attribute__((unused)) void *arg)
{
	uint8_t last_opcode;
	uint8_t opcode = 0xff;
	int rc;

	printf("gameserver reader thread\n");
	while (1) {
		last_opcode = opcode;
		/* printf("Client reading from game server %d bytes...\n", sizeof(opcode)); */
		rc = snis_readsocket(gameserver_sock, &opcode, sizeof(opcode));

		/* grab time as close to when we got the packet as possible */
		double update_time = time_now_double();

		/* printf("rc = %d, errno  %s\n", rc, strerror(errno)); */
		if (rc != 0)
			goto protocol_error;
		/* printf("got opcode %hhu\n", opcode); */
		switch (opcode)	{
		case OPCODE_UPDATE_SHIP:
		case OPCODE_UPDATE_SHIP2:
			/* printf("processing update ship...\n"); */
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
		case OPCODE_UPDATE_ASTEROID:
			rc = process_update_asteroid_packet();
			break;
		case OPCODE_UPDATE_CARGO_CONTAINER:
			rc = process_update_cargo_container_packet();
			break;
		case OPCODE_UPDATE_DERELICT:
			rc = process_update_derelict_packet();
			break;
		case OPCODE_UPDATE_PLANET:
			rc = process_update_planet_packet();
			break;
		case OPCODE_UPDATE_STARBASE:
			rc = process_update_starbase_packet();
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
		case OPCODE_UPDATE_LASER:
			rc = process_update_laser_packet();
			break;
		case OPCODE_UPDATE_TORPEDO:
			rc = process_update_torpedo_packet();
			break;
		case OPCODE_WARP_LIMBO:
			rc = process_warp_limbo_packet();
			break;
		case OPCODE_INITIATE_WARP:
			process_initiate_warp_packet();
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
		case OPCODE_ROLE_ONSCREEN:
			rc = process_role_onscreen_packet();
			break;
		case OPCODE_SCI_SELECT_TARGET:
			rc = process_sci_select_target_packet();
			break;
		case OPCODE_SCI_DETAILS:
			rc = process_sci_details();
			break;
		case OPCODE_SCI_SELECT_COORDS:
			rc = process_sci_select_coords_packet();
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
		case OPCODE_COLLISION_NOTIFICATION:
			process_collision_notification();
			break;
		case OPCODE_LOAD_SKYBOX:
			rc = process_load_skybox();
			break;
		case OPCODE_CYCLE_MAINSCREEN_POINT_OF_VIEW:
			rc = process_cycle_mainscreen_point_of_view();
			break;
		case OPCODE_ADD_WARP_EFFECT:
			rc = process_add_warp_effect();
			break;
		case OPCODE_UPDATE_UNIVERSE_TIMESTAMP:
			rc = process_update_universe_timestamp(update_time);
			break;
		case OPCODE_DETONATE:
			rc = process_detonate();
			if (rc)
				goto protocol_error;
			break;
		default:
			goto protocol_error;
		}
		if (rc) /* protocol error */
			break;
	}

protocol_error:
	printf("Protocol error in gameserver reader, opcode = %hu\n", opcode);
	snis_print_last_buffer(gameserver_sock);	
	printf("last opcode was %hhu\n", last_opcode);
	close(gameserver_sock);
	gameserver_sock = -1;
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
	printf("client baling on server...\n");
	packed_buffer_free(buffer);
	shutdown(gameserver_sock, SHUT_RDWR);
	close(gameserver_sock);
	gameserver_sock = -1;
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
	while (1) {
		wait_for_serverbound_packets();
		write_queued_packets_to_server();
	}
	return NULL;
}

int role_to_displaymode(uint32_t role)
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
	char *buildinfo1 = BUILD_INFO_STRING1;
	char *buildinfo2 = BUILD_INFO_STRING2;
	struct packed_buffer *pb;

	pb = packed_buffer_allocate(strlen(buildinfo1) + strlen(buildinfo2) + 20);
	packed_buffer_append(pb, "bbw", OPCODE_UPDATE_BUILD_INFO, 0, strlen(buildinfo1) + 1);
	packed_buffer_append_raw(pb, buildinfo1, (unsigned short) strlen(buildinfo1) + 1);
	packed_buffer_append(pb, "bbw", OPCODE_UPDATE_BUILD_INFO, 1, strlen(buildinfo2) + 1);
	packed_buffer_append_raw(pb, buildinfo2, (unsigned short) strlen(buildinfo2) + 1);
	queue_to_server(pb);
}

static void *connect_to_gameserver_thread(__attribute__((unused)) void *arg)
{
	int rc;
	struct addrinfo *gameserverinfo, *i;
	struct addrinfo hints;
#if 0
	void *addr;
	char *ipver; 
#endif
	char portstr[50];
	char hoststr[50];
	unsigned char *x = (unsigned char *) &lobby_game_server[lobby_selected_server].ipaddr;
	struct add_player_packet app;
	int flag = 1;

	sprintf(portstr, "%d", ntohs(lobby_game_server[lobby_selected_server].port));
	sprintf(hoststr, "%d.%d.%d.%d", x[0], x[1], x[2], x[3]);
	printf("connecting to %s/%s\n", hoststr, portstr);

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE | AI_NUMERICSERV | AI_NUMERICHOST;
	rc = getaddrinfo(hoststr, portstr, &hints, &gameserverinfo);
	if (rc)
		goto error;

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
	if (rc)
		fprintf(stderr, "setsockopt(TCP_NODELAY) failed.\n");

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
	app.role = htonl(role);
	strncpy((char *) app.shipname, shipname, 19);
	strncpy((char *) app.password, password, 19);

	printf("Notifying server, opcode update player\n");
	if (snis_writesocket(gameserver_sock, &app, sizeof(app)) < 0) {
		fprintf(stderr, "Initial write to gameserver failed.\n");
		shutdown(gameserver_sock, SHUT_RDWR);
		close(gameserver_sock);
		gameserver_sock = -1;
		goto error;
	}
	printf("Wrote update player opcode\n");

        pthread_attr_init(&gameserver_reader_attr);
        pthread_attr_init(&gameserver_writer_attr);
        pthread_attr_setdetachstate(&gameserver_reader_attr, PTHREAD_CREATE_DETACHED);
        pthread_attr_setdetachstate(&gameserver_writer_attr, PTHREAD_CREATE_DETACHED);
	printf("starting gameserver reader thread\n");
	rc = pthread_create(&read_from_gameserver_thread, &gameserver_reader_attr, gameserver_reader, NULL);
	if (rc) {
		fprintf(stderr, "Failed to create gameserver reader thread: %d '%s', '%s'\n",
			rc, strerror(rc), strerror(errno));
	}
	printf("started gameserver reader thread\n");

	pthread_mutex_init(&to_server_queue_mutex, NULL);
	pthread_mutex_init(&to_server_queue_event_mutex, NULL);
	packed_buffer_queue_init(&to_server_queue);

	printf("starting gameserver writer thread\n");
	rc = pthread_create(&write_to_gameserver_thread, &gameserver_writer_attr, gameserver_writer, NULL);
	if (rc) {
		fprintf(stderr, "Failed to create gameserver writer thread: %d '%s', '%s'\n",
			rc, strerror(rc), strerror(errno));
	}
	printf("started gameserver writer thread\n");

	request_universe_timestamp();
	send_build_info_to_server();
error:
	/* FIXME, this isn't right... */
	freeaddrinfo(gameserverinfo);

	return NULL;
}

void connect_to_gameserver(int selected_server)
{
	int rc;

        pthread_attr_init(&gameserver_connect_attr);
        pthread_attr_setdetachstate(&gameserver_connect_attr, PTHREAD_CREATE_DETACHED);
	rc = pthread_create(&gameserver_connect_thread, &gameserver_connect_attr, connect_to_gameserver_thread, NULL);
	if (rc) {
		fprintf(stderr, "Failed to create thread to connect to gameserver: %d '%s', '%s'\n",
			rc, strerror(rc), strerror(errno));
	}

	return;
}

static void show_connecting_screen(GtkWidget *w)
{
	static int connected_to_gameserver = 0;
	sng_set_foreground(WHITE);
	sng_abs_xy_draw_string("CONNECTING TO SERVER...", SMALL_FONT, 100, 300 + LINEHEIGHT);
	if (!connected_to_gameserver) {
		connected_to_gameserver = 1;
		connect_to_gameserver(lobby_selected_server);
	}
}

static void show_connected_screen(GtkWidget *w)
{
	sng_set_foreground(WHITE);
	sng_abs_xy_draw_string("CONNECTED TO SERVER", SMALL_FONT, 100, 300 + LINEHEIGHT);
	sng_abs_xy_draw_string("DOWNLOADING GAME DATA", SMALL_FONT, 100, 300 + LINEHEIGHT * 3);
}

static void show_common_screen(GtkWidget *w, char *title)
{
	int title_color;
	int border_color;

	if (red_alert_mode) {
		title_color = RED;
		border_color = RED;
	} else {
		title_color = GREEN;
		border_color = BLUE;
	}
	sng_set_foreground(title_color);
	sng_abs_xy_draw_string(title, SMALL_FONT, 25, 10 + LINEHEIGHT);
	sng_set_foreground(border_color);
	snis_draw_line(1, 1, SCREEN_WIDTH, 0);
	snis_draw_line(SCREEN_WIDTH - 1, 1, SCREEN_WIDTH - 1, SCREEN_HEIGHT - 1);
	snis_draw_line(SCREEN_WIDTH - 1, SCREEN_HEIGHT - 1, 1, SCREEN_HEIGHT - 1);
	snis_draw_line(1, 1, 1, SCREEN_HEIGHT - 1);

	if (vertical_controls_timer) {
		sng_set_foreground(WHITE);
		vertical_controls_timer--;
		if (vertical_controls_inverted > 0)
			sng_center_xy_draw_string("VERTICAL CONTROLS NORMAL",
					SMALL_FONT, SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2);
		else
			sng_center_xy_draw_string("VERTICAL CONTROLS INVERTED",
					SMALL_FONT, SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2);
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

		delta = abs(desired_zoom - current_zoom) / 10;
		if (delta <= 0)
			delta = 1;
		if (desired_zoom < current_zoom)
			current_zoom -= delta;
		else
			current_zoom += delta;
	} 
	return current_zoom;
}

#define NEAR_CAMERA_PLANE 1.0
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

	sng_set_foreground(GREEN);
	snis_draw_line(x1, cy, x1 + 25, cy);
	snis_draw_line(x2 - 25, cy, x2, cy);
	snis_draw_line(cx, y1, cx, y1 + 25);
	snis_draw_line(cx, y2 - 25, cx, y2);
}

static void main_screen_add_text(char *msg)
{
	main_screen_text.last = (main_screen_text.last + 1) % 4;
	strncpy(main_screen_text.text[main_screen_text.last], msg, 99);
}

static void draw_main_screen_text(GtkWidget *w, GdkGC *gc)
{
	int first, i;

	if (!main_screen_text.comms_on_mainscreen)
		return;

	first = (main_screen_text.last + 1) % 4;;

	sng_set_foreground(GREEN);
	for (i = 0; i < 4; i++) {
		sng_abs_xy_draw_string(main_screen_text.text[first],
				NANO_FONT, 10, SCREEN_HEIGHT - (4 - i) * 18 - 10);
		first = (first + 1) % 4;
	}
}

static void draw_targeting_indicator(GtkWidget *w, GdkGC *gc, int x, int y, int color, int ring)
{
	int i;

	sng_set_foreground(color);
	for (i = 0; i < 4; i++) {
		int x1, y1, x2, y2;
		double angle;
		double dx, dy, ddx, ddy;
		int offset = ring ? 0 : 45;

		angle = (M_PI * ((i * 90 + offset + timer * 4) % 360)) / 180.0;

		dx = 15.0 * cos(angle);
		dy = 15.0 * sin(angle);
		ddx = 5.0 * cos(angle + M_PI / 2.0);
		ddy = 5.0 * sin(angle + M_PI / 2.0);

		x1 = x + dx;
		y1 = y + dy;
		x2 = x + 2.0 * dx;
		y2 = y + 2.0 * dy;
		/* snis_draw_line(x1, y1, x2, y2); */
		snis_draw_line(x2 - ddx, y2 - ddy, x2 + ddx, y2 + ddy);
		snis_draw_line(x2 - ddx, y2 - ddy, x1, y1);
		snis_draw_line(x1, y1, x2 + ddx, y2 + ddy);
		if (ring)
			sng_draw_circle(0, x, y, 33);
	}
}

/* size is 0.0 to 1.0 */
static void add_ship_thrust_entities(struct entity *thrust_entity[],
		int *nthrust_ports, struct entity_context *cx, struct entity *e,
		int shiptype, int impulse)
{
	int i;
	struct thrust_attachment_point *ap = ship_thrust_attachment_point(shiptype);

	if (!ap)
		return;

	/* 180 is about max current with preset and 5x is about right for max size */
	float thrust_size = clampf(impulse / 36.0, 0.1, 5.0);

	if (nthrust_ports)
		*nthrust_ports = 0;
	for (i = 0; i < ap->nports; i++) {
		struct entity *t = add_entity(cx, thrust_animation_mesh,
			ap->port[i].pos.v.x, ap->port[i].pos.v.y, ap->port[i].pos.v.z, WHITE);
		if (thrust_entity) {
			thrust_entity[i] = t;
			(*nthrust_ports)++;
		}
		if (t) {
			update_entity_material(t, &thrust_material);
			update_entity_orientation(t, &identity_quat);
			union vec3 thrust_scale = { { thrust_size, 1.0, 1.0 } };
			vec3_mul_self(&thrust_scale, ap->port[i].scale);
			update_entity_non_uniform_scale(t, thrust_scale.v.x, thrust_scale.v.y, thrust_scale.v.z);
			update_entity_parent(cx, t, e);
		}
	}
}

static void show_weapons_camera_view(GtkWidget *w)
{
	const float min_angle_of_view = 5.0 * M_PI / 180.0;
	const float max_angle_of_view = ANGLE_OF_VIEW * M_PI / 180.0;
	static int current_zoom = 0;
	float angle_of_view;
	struct snis_entity *o;
	float cx, cy, cz;
	union quat camera_orientation;
	union vec3 recoil = { { -1.0f, 0.0f, 0.0f } };
	char buf[20];

	static int last_timer = 0;
	int first_frame = (timer != last_timer+1);
	last_timer = timer;

	if (first_frame) {
		turret_recoil_amount = 0;
		weapons_camera_shake = 0;
	}

	if (!(o = find_my_ship()))
		return;

	/* current_zoom = newzoom(current_zoom, o->tsd.ship.mainzoom); */
	current_zoom = 0;
	angle_of_view = ((255.0 - (float) current_zoom) / 255.0) *
				(max_angle_of_view - min_angle_of_view) + min_angle_of_view;

	cx = o->x;
	cy = o->y;
	cz = o->z;

	quat_mul(&camera_orientation, &o->orientation, &o->tsd.ship.weap_orientation);

	union vec3 turret_pos = { {-4, 5.45, 0} };
	quat_rot_vec_self(&turret_pos, &o->orientation);
	vec3_add_c_self(&turret_pos, cx, cy, cz);

	union vec3 view_offset = { {0, 0.75, 0} };
	quat_rot_vec_self(&view_offset, &camera_orientation);

	union vec3 cam_pos = turret_pos;
	vec3_add_self(&cam_pos, &view_offset);

	if (weapons_camera_shake > 0.05) {
		float ryaw, rpitch;

		ryaw = weapons_camera_shake * ((snis_randn(100) - 50) * 0.025f) * M_PI / 180.0;
		rpitch = weapons_camera_shake * ((snis_randn(100) - 50) * 0.025f) * M_PI / 180.0;
		quat_apply_relative_yaw_pitch(&camera_orientation, ryaw, rpitch);
		weapons_camera_shake = 0.7f * weapons_camera_shake;
	}

	camera_set_pos(ecx, cam_pos.v.x, cam_pos.v.y, cam_pos.v.z);
	camera_set_orientation(ecx, &camera_orientation);
	camera_set_parameters(ecx, NEAR_CAMERA_PLANE, FAR_CAMERA_PLANE,
				SCREEN_WIDTH, SCREEN_HEIGHT, angle_of_view);
	set_window_offset(ecx, 0, 0);
	set_lighting(ecx, SUNX, SUNY, SUNZ);
	calculate_camera_transform(ecx);

	sng_set_foreground(GREEN);
	if (!ecx_fake_stars_initialized) {
		ecx_fake_stars_initialized = 1;
		entity_init_fake_stars(ecx, 2000, 300.0f * 10.0f);
	}

	render_skybox(ecx);

	pthread_mutex_lock(&universe_mutex);


	/* Add our ship into the scene (on the mainscreen, it is omitted) */
	o->entity = add_entity(ecx, ship_mesh_map[o->tsd.ship.shiptype],
				o->x, o->y, o->z, SHIP_COLOR);
	if (o->entity) {
		update_entity_orientation(o->entity, &o->orientation);
		set_render_style(o->entity, RENDER_NORMAL);
	}

	/* Add our turret into the mix */
	quat_rot_vec_self(&recoil, &camera_orientation);
	struct entity* turret_entity = add_entity(ecx, ship_turret_mesh,
				turret_pos.v.x + turret_recoil_amount * recoil.v.x,
				turret_pos.v.y + turret_recoil_amount * recoil.v.y,
				turret_pos.v.z + turret_recoil_amount * recoil.v.z,
				SHIP_COLOR);
	turret_recoil_amount = turret_recoil_amount * 0.5f;
	if (turret_entity) {
		update_entity_orientation(turret_entity, &camera_orientation);
		set_render_style(turret_entity, RENDER_NORMAL);
	}

	if (o->entity)
		add_ship_thrust_entities(NULL, NULL, ecx, o->entity,
				o->tsd.ship.shiptype, o->tsd.ship.power_data.impulse.i);

	render_entities(ecx);

	/* Remove our ship from the scene */
	remove_entity(ecx, turret_entity);
	remove_entity(ecx, o->entity);
	o->entity = NULL;

	/* range is the same as max zoom on old weapons */
	draw_plane_radar(w, o, &camera_orientation, 400, 500, 75, XKNOWN_DIM * 0.02);

#if 0
	/* Draw targeting indicator on main screen */
	if (o->tsd.ship.ai[0].u.attack.victim_id != -1) {
		float sx, sy;
		struct snis_entity *target = lookup_entity_by_id(o->tsd.ship.ai[0].u.attack.victim_id);

		if (target && target->alive && target->entity && entity_onscreen(target->entity)) {
			entity_get_screen_coords(target->entity, &sx, &sy);
			draw_targeting_indicator(w, gc, sx, sy, TARGETING_COLOR, 0);
		}
	}
#endif
	/* Draw science selector indicator on main screen */
	if (curr_science_guy) {
		float sx, sy;

		if (curr_science_guy->alive && curr_science_guy->entity &&
			entity_onscreen(curr_science_guy->entity)) {
			entity_get_screen_coords(curr_science_guy->entity, &sx, &sy);
			draw_targeting_indicator(w, gc, sx, sy, SCIENCE_SELECT_COLOR, 0);
		}
	}

	/* show torpedo count */
	if (!o->tsd.ship.torpedoes_loading || (timer & 0x4)) {
		if (o->tsd.ship.torpedoes_loading)
			sng_set_foreground(RED);
		else
			sng_set_foreground(AMBER);
		sprintf(buf, "TORP: %03d", o->tsd.ship.torpedoes +
					o->tsd.ship.torpedoes_loading +
					o->tsd.ship.torpedoes_loaded);
		sng_abs_xy_draw_string(buf, NANO_FONT, 570, SCREEN_HEIGHT - 15);
	}

	/* idiot lights for low power */
	const int low_power_threshold = 10;
	sng_set_foreground(RED);
	if (o->tsd.ship.power_data.phasers.i < low_power_threshold) {
		sng_abs_xy_draw_string("LOW PHASER POWER", NANO_FONT, 320, 65);
	}

	/* show security indicator */
	if (o->tsd.ship.in_secure_area && timer & 0x08) {
		sng_set_foreground(RED);
		sng_center_xy_draw_string("HIGH SECURITY", NANO_FONT, 400, 500);
		sng_center_xy_draw_string("AREA", NANO_FONT, 400, 516);
	}

	show_gunsight(w);
	pthread_mutex_unlock(&universe_mutex);
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

	if (!(o = find_my_ship()))
		return;

	static int last_timer = 0;
	int first_frame = (timer != last_timer+1);
	last_timer = timer;

	if (first_frame)
		current_zoom = o->tsd.ship.mainzoom;
	else
		current_zoom = newzoom(current_zoom, o->tsd.ship.mainzoom);

	angle_of_view = ((255.0 - (float) current_zoom) / 255.0) *
				(max_angle_of_view - min_angle_of_view) + min_angle_of_view;

	if (o->tsd.ship.view_mode == MAINSCREEN_VIEW_MODE_NORMAL) {
		switch (camera_mode) {
		case 0:
			camera_orientation = o->orientation;
			desired_cam_orientation = camera_orientation;
			break;
		case 1:
		case 2:
			desired_cam_orientation = o->orientation;
			if (first_frame)
				camera_orientation = desired_cam_orientation;
			else
				quat_nlerp(&camera_orientation, &camera_orientation,
						&desired_cam_orientation, 0.08);
			break;
		}
	} else {
		quat_mul(&camera_orientation, &o->orientation, &o->tsd.ship.weap_orientation);
	}

	struct entity *player_ship = 0;
	union vec3 desired_cam_offset;

	switch (camera_mode) {
	case 0:
		vec3_init(&desired_cam_offset, 0, 0, 0);
		break;
	case 1:
	case 2: {
			vec3_init(&desired_cam_offset, -1.0f, 0.25f, 0.0f);
			vec3_mul_self(&desired_cam_offset, 200.0f * camera_mode);
			quat_rot_vec_self(&desired_cam_offset, &camera_orientation);

			/* temporarily add ship into scene for camera mode 1 & 2 */
			player_ship = add_entity(ecx, ship_mesh_map[o->tsd.ship.shiptype],
					o->x, o->y, o->z, SHIP_COLOR);
			if (player_ship)
				update_entity_orientation(player_ship, &o->orientation);

			struct entity *turret_base = add_entity(ecx, ship_turret_base_mesh, -4, 5.45, 0, SHIP_COLOR);

			if (turret_base) {
				update_entity_orientation(turret_base, &identity_quat);
				if (player_ship)
					update_entity_parent(ecx, turret_base, player_ship);
			}

			struct entity *turret = add_entity(ecx, ship_turret_mesh, 0, 0, 0, SHIP_COLOR);

			if (turret) {
				update_entity_orientation(turret, &o->tsd.ship.weap_orientation);
				if (turret_base)
					update_entity_parent(ecx, turret, turret_base);
			}

			if (player_ship)
				add_ship_thrust_entities(NULL, NULL, ecx,
					player_ship, o->tsd.ship.shiptype,
					o->tsd.ship.power_data.impulse.i);

			break;
		}
	}

	if (first_frame)
		cam_offset = desired_cam_offset;
	else
		vec3_lerp(&cam_offset, &cam_offset, &desired_cam_offset, 0.15);

	cam_pos.v.x = o->x + cam_offset.v.x;
	cam_pos.v.y = o->y + cam_offset.v.y;
	cam_pos.v.z = o->z + cam_offset.v.z;

	camera_set_pos(ecx, cam_pos.v.x, cam_pos.v.y, cam_pos.v.z);
	camera_set_orientation(ecx, &camera_orientation);
	camera_set_parameters(ecx, NEAR_CAMERA_PLANE, FAR_CAMERA_PLANE,
				SCREEN_WIDTH, SCREEN_HEIGHT, angle_of_view);
	set_window_offset(ecx, 0, 0);
	set_lighting(ecx, SUNX, SUNY, SUNZ);
	calculate_camera_transform(ecx);

	sng_set_foreground(GREEN);
	if (!ecx_fake_stars_initialized) {
		ecx_fake_stars_initialized = 1;
		entity_init_fake_stars(ecx, 2000, 300.0f * 10.0f);
	}

	render_skybox(ecx);

	pthread_mutex_lock(&universe_mutex);
	render_entities(ecx);

	/* if we added the ship into the scene, remove it now */
	if (player_ship) {
		remove_entity(ecx, player_ship);
	}

#if 0
	/* Draw targeting indicator on main screen */
	if (o->tsd.ship.ai[0].u.attack.victim_id != -1) {
		float sx, sy;
		struct snis_entity *target = lookup_entity_by_id(o->tsd.ship.ai[0].u.attack.victim_id);

		if (target && target->alive && target->entity &&
			entity_onscreen(target->entity)) {
			entity_get_screen_coords(target->entity, &sx, &sy);
			draw_targeting_indicator(w, gc, sx, sy, TARGETING_COLOR, 0);
		}
	}
#endif
	/* Draw science selector indicator on main screen */
	if (curr_science_guy) {
		float sx, sy;

		if (curr_science_guy->alive && curr_science_guy->entity &&
			entity_onscreen(curr_science_guy->entity)) {
			entity_get_screen_coords(curr_science_guy->entity, &sx, &sy);
			draw_targeting_indicator(w, gc, sx, sy, SCIENCE_SELECT_COLOR, 0);
		}
	}

#if 0
	/* Debug info */
	sng_set_foreground(GREEN);
	for (i = 0; i <= get_entity_count(ecx); i++) {
		struct entity *e;
		struct snis_entity *oo;
		float sx, sy;
		char buffer[100];

		e = get_entity(ecx, i);
		oo = entity_get_user_data(e);
		if (!oo)
			continue;
		entity_get_screen_coords(e, &sx, &sy);
		sprintf(buffer, "%3.1f,%6.1f,%6.1f,%6.1f",
				oo->heading * 180.0 / M_PI, oo->x, oo->y, oo->z);
		sng_abs_xy_draw_string(buffer, NANO_FONT, sx + 10, sy);
	}
	{
		char buffer[100];
		sprintf(buffer, "%3.1f,%6.1f,%6.1f,%6.1f",
				o->heading * 180.0 / M_PI, o->x, o->y, o->z);
		sng_abs_xy_draw_string(buffer, NANO_FONT, 0, 10);
	}
#endif

	if (o->tsd.ship.view_mode == MAINSCREEN_VIEW_MODE_WEAPONS)
		show_gunsight(w);
	draw_main_screen_text(w, gc);
	pthread_mutex_unlock(&universe_mutex);
	show_common_screen(w, "");	
}

/* position and dimensions of science scope */
#define SCIENCE_SCOPE_X 20
#define SCIENCE_SCOPE_Y 70 
#define SCIENCE_SCOPE_W 500
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

	sng_set_foreground(GREEN);
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
			sng_set_foreground(LIMEGREEN);
			break;
		case OBJTYPE_STARBASE:
			sng_set_foreground(WHITE);
			break;
		case OBJTYPE_ASTEROID:
			sng_set_foreground(AMBER);
			break;
		case OBJTYPE_DERELICT:
			sng_set_foreground(ORANGERED);
			break;
		case OBJTYPE_PLANET:
			sng_set_foreground(BLUE);
			break;
		case OBJTYPE_TORPEDO:
			sng_set_foreground(ORANGERED);
			break;
		case OBJTYPE_LASER:
			sng_set_foreground(LASER_COLOR);
			break;
		case OBJTYPE_SPARK:
		case OBJTYPE_EXPLOSION:
		default:
			sng_set_foreground(LIMEGREEN);
		}
		if (o->type == OBJTYPE_SHIP2 || o->type == OBJTYPE_SHIP1) {
			snis_draw_arrow(w, gc, x, y, SCIENCE_SCOPE_R / 2, o->heading, 0.3);
		} else {
			snis_draw_line(x - 1, y, x + 1, y);
			snis_draw_line(x, y - 1, x, y + 1);
		}
	}

	if (selected)
		sng_draw_circle(0, x, y, 10);
	
	if (o->sdata.science_data_known) {
		switch (o->type) {
		case OBJTYPE_SHIP2:
		case OBJTYPE_SHIP1:
			sng_set_foreground(LIMEGREEN);
			sprintf(buffer, "%s %s\n", o->sdata.name,
					ship_type[o->sdata.subclass].class); 
			break;
		case OBJTYPE_STARBASE:
			sng_set_foreground(WHITE);
			sprintf(buffer, "%s %s\n", "SB",  o->sdata.name); 
			break;
		case OBJTYPE_ASTEROID:
			sng_set_foreground(AMBER);
			sprintf(buffer, "%s %s\n", "A",  o->sdata.name); 
			break;
		case OBJTYPE_DERELICT:
			sng_set_foreground(ORANGERED);
			sprintf(buffer, "%s %s\n", "D",  "???"); 
			break;
		case OBJTYPE_PLANET:
			sng_set_foreground(BLUE);
			sprintf(buffer, "%s %s\n", "P",  o->sdata.name); 
			break;
		case OBJTYPE_TORPEDO:
		case OBJTYPE_SPARK:
		case OBJTYPE_EXPLOSION:
		case OBJTYPE_LASER:
			sng_set_foreground(LIMEGREEN);
			strcpy(buffer, "");
			break;
		default:
			sng_set_foreground(GREEN);
			sprintf(buffer, "%s %s\n", "?", o->sdata.name); 
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
	sng_set_foreground(GREEN);
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
			sng_set_foreground(LIMEGREEN);
			break;
		case OBJTYPE_STARBASE:
			sng_set_foreground(WHITE);
			break;
		case OBJTYPE_ASTEROID:
		case OBJTYPE_DERELICT:
			sng_set_foreground(AMBER);
			break;
		case OBJTYPE_PLANET:
			sng_set_foreground(BLUE);
			break;
		case OBJTYPE_TORPEDO:
		case OBJTYPE_SPARK:
		case OBJTYPE_EXPLOSION:
		case OBJTYPE_LASER:
		default:
			sng_set_foreground(LIMEGREEN);
		}
		if (o->type == OBJTYPE_SHIP2 || o->type == OBJTYPE_SHIP1) {
			e = add_entity(sciballecx, ship_icon_mesh, o->x, o->y, o->z, LIMEGREEN);
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
			sng_set_foreground(LIMEGREEN);
			sprintf(buffer, "%s %s\n", o->sdata.name,
				ship_type[o->sdata.subclass].class); 
			break;
		case OBJTYPE_STARBASE:
			sng_set_foreground(WHITE);
			sprintf(buffer, "%s %s\n", "SB",  o->sdata.name); 
			break;
		case OBJTYPE_ASTEROID:
			sng_set_foreground(AMBER);
			sprintf(buffer, "%s %s\n", "A",  o->sdata.name); 
			break;
		case OBJTYPE_DERELICT:
			sng_set_foreground(AMBER);
			sprintf(buffer, "%s %s\n", "A",  "???"); 
			break;
		case OBJTYPE_PLANET:
			sng_set_foreground(BLUE);
			sprintf(buffer, "%s %s\n", "P",  o->sdata.name); 
			break;
		case OBJTYPE_TORPEDO:
		case OBJTYPE_SPARK:
		case OBJTYPE_EXPLOSION:
		case OBJTYPE_LASER:
			sng_set_foreground(LIMEGREEN);
			strcpy(buffer, "");
			break;
		default:
			sng_set_foreground(GREEN);
			sprintf(buffer, "%s %s\n", "?", o->sdata.name); 
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
};
static struct science_data science_guy[MAXGAMEOBJS] = { {0}, };
static int nscience_guys = 0;

static void draw_3d_laserbeam(GtkWidget *w, GdkGC *gc, struct entity_context *cx, struct snis_entity *o, struct snis_entity *laserbeam, double r)
{
	int rc, color;
	struct snis_entity *shooter, *shootee;

	shooter = lookup_entity_by_id(laserbeam->tsd.laserbeam.origin);
	if (!shooter)
		return;
	shootee = lookup_entity_by_id(laserbeam->tsd.laserbeam.target);
	if (!shootee)
		return;

	if (shooter->type == OBJTYPE_SHIP2)
		color = NPC_LASER_COLOR;
	else
		color = PLAYER_LASER_COLOR;

	union vec3 center = {{o->x, o->y, o->z}};
	union vec3 vshooter = {{shooter->x, shooter->y, shooter->z}};
	union vec3 vshootee = {{shootee->x, shootee->y, shootee->z}};

	union vec3 clip1, clip2;
	rc = sphere_line_segment_intersection(&vshooter, &vshootee, &center, r, &clip1, &clip2);
	if (rc < 0)
		return;
	float sx1, sy1, sx2, sy2;
	if (!transform_line(cx, clip1.v.x, clip1.v.y, clip1.v.z, clip2.v.x, clip2.v.y, clip2.v.z, &sx1, &sy1, &sx2, &sy2)) {
		sng_draw_laser_line(sx1, sy1, sx2, sy2, color);
	}
}

static void __attribute__((unused)) snis_draw_3d_dotted_line(GtkWidget *w, GdkGC *gc,
				struct entity_context *cx,
				float x1, float y1, float z1, float x2, float y2, float z2 )
{
	float sx1, sy1, sx2, sy2;
	if (!transform_line(cx, x1, y1, z1, x2, y2, z2, &sx1, &sy1, &sx2, &sy2)) {
		sng_draw_dotted_line(sx1, sy1, sx2, sy2);
	}
}

static void snis_draw_3d_line(GtkWidget *w, GdkGC *gc, struct entity_context *cx,
				float x1, float y1, float z1, float x2, float y2, float z2 )
{
	float sx1, sy1, sx2, sy2;
	if (!transform_line(cx, x1, y1, z1, x2, y2, z2, &sx1, &sy1, &sx2, &sy2)) {
		snis_draw_line(sx1, sy1, sx2, sy2);
	}
}

static void draw_3d_mark_arc(GtkWidget *w, GdkGC *gc, struct entity_context *ecx,
			const union vec3 *center, float r, float heading, float mark)
{
	/* break arc into max 5 degree segments */
	int increments = abs(mark / (5.0 * M_PI / 180.0))+1;
	float delta = mark/increments;
	int i;
	union vec3 p1;
	for (i=0;i<=increments; i++){
		union vec3 p2;
		heading_mark_to_vec3(r, heading, delta * i, &p2);
		vec3_add_self(&p2, center);

		if (i!=0) {
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
	char active;
	uint32_t id;
	float value;
	int mode;
	float delta;
	float min;
	float max;
};

struct tween_map
{
	int last_index;
	struct tween_state* states;
};

static struct tween_map* tween_init(int max_size)
{
	struct tween_map* result = malloc(sizeof(struct tween_map));
	result->last_index=0;
	result->states = malloc(sizeof(struct tween_state) * max_size);
	memset(result->states,0,sizeof(struct tween_state) * max_size);
	return result;
}

static void tween_update(struct tween_map* map)
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
			if (map->states[i].value < (map->states[i].max - map->states[i].min) * 0.01) {
				map->states[i].active = 0;
			}
		}
	}
}

static int __attribute__((unused)) tween_get_value(struct tween_map* map, uint32_t id, float *value)
{
	int i;
	for (i=0; i < map->last_index; i++) {
		if (map->states[i].active && map->states[i].id == id) {
			*value = map->states[i].value;
			return 1;
		}
	}
	return 0;
}

static int tween_get_value_and_decay(struct tween_map* map, uint32_t id, float *value)
{
	int i;
	for (i=0; i < map->last_index; i++) {
		if (map->states[i].active && map->states[i].id == id) {
			if (map->states[i].delta>0) {
				map->states[i].delta = -map->states[i].delta;
			}
			*value = map->states[i].value;
			return 1;
		}
	}
	return 0;
}


static void tween_add_or_update(struct tween_map* map, uint32_t id, float initial_value,
					int mode, float delta, float min, float max)
{
	int first_free = map->last_index;
        int i;
        for (i=0; i < map->last_index; i++) {
                if (!map->states[i].active) {
			if (i<first_free) {
				first_free=i;
			}
			continue;
		}
		if (map->states[i].id == id) {
			map->states[i].mode = mode;
			map->states[i].delta = delta;
			map->states[i].min = min;
			map->states[i].max = max;
                        return;
		}
        }

	map->states[first_free].id = id;
	map->states[first_free].active = 1;
	map->states[first_free].value = initial_value;
	map->states[first_free].mode = mode;
	map->states[first_free].delta = delta;
	map->states[first_free].min = min;
	map->states[first_free].max = max;

	if (first_free>=map->last_index) {
		map->last_index = first_free+1;
	}
}

static struct tween_map* sciplane_tween = 0;

static void draw_sciplane_laserbeam(GtkWidget *w, GdkGC *gc, struct entity_context *cx, struct snis_entity *o, struct snis_entity *laserbeam, double r)
{
	int i, rc, color;
	struct snis_entity *shooter=0, *shootee=0;

	for (i=0; i<nscience_guys; i++) {
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
		color = NPC_LASER_COLOR;
	else
		color = PLAYER_LASER_COLOR;


	/* figure out where in 3d space the ends show up on sciplane */
	union vec3 ship_pos = {{o->x, o->y, o->z}};

	/* shooter position */
	double shooter_range, shooter_heading, shooter_mark;
	float shooter_tween = 0;
	union vec3 vshooter;

	union vec3 shooter_dir = {{shooter->x, shooter->y, shooter->z}};
	vec3_sub_self(&shooter_dir, &ship_pos);
	vec3_to_heading_mark(&shooter_dir, &shooter_range, &shooter_heading, &shooter_mark);
	tween_get_value(sciplane_tween, shooter->id, &shooter_tween);
	heading_mark_to_vec3(shooter_range, shooter_heading, shooter_mark * shooter_tween, &vshooter);
	vec3_add_self(&vshooter, &ship_pos);

	/* shootee position */
	double shootee_range, shootee_heading, shootee_mark;
	float shootee_tween = 0;
	union vec3 vshootee;

	union vec3 shootee_dir = {{shootee->x, shootee->y, shootee->z}};
	vec3_sub_self(&shootee_dir, &ship_pos);
	vec3_to_heading_mark(&shootee_dir, &shootee_range, &shootee_heading, &shootee_mark);
	tween_get_value(sciplane_tween, shootee->id, &shootee_tween);
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

		/* take currnet position, find dir from ship to it, get heading/mark, recalc pos with mark=0 */
		double dist, heading, mark;
		union vec3 selected_dir;
		vec3_sub(&selected_dir, &selected_pos, &ship_pos);
		vec3_to_heading_mark(&selected_dir, &dist, &heading, &mark);
		heading_mark_to_vec3(dist, heading, 0, &selected_m0_pos);
		vec3_add_self(&selected_m0_pos, &ship_pos);

		tween_get_value(sciplane_tween, curr_science_guy->id, &selected_guy_tween);
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
		quat_nlerp(&cam_orientation, &cam_orientation, desired_cam_orientation, 0.15);
		vec3_lerp(&camera_lookat, &camera_lookat, &desired_lookat, 0.15);
		cam_range_fraction = float_lerp(cam_range_fraction, desired_cam_range_fraction, 0.15);
	}

	union vec3 camera_pos = {{0, 0, cam_range_fraction * range}};
	quat_rot_vec_self(&camera_pos, &cam_orientation);
	vec3_add_self(&camera_pos, &camera_lookat);

	union vec3 camera_up = {{0,1,0}};
	quat_rot_vec_self(&camera_up, &cam_orientation);

	camera_assign_up_direction(navecx, camera_up.v.x, camera_up.v.y, camera_up.v.z);
	camera_set_pos(navecx, camera_pos.v.x, camera_pos.v.y, camera_pos.v.z);
	camera_look_at(navecx, camera_lookat.v.x, camera_lookat.v.y, camera_lookat.v.z);

        set_renderer(navecx, WIREFRAME_RENDERER);
	camera_set_parameters(navecx, range*(cam_range_fraction-1.0), range*(dist_to_cam_frac+1.0),
				SCREEN_WIDTH, SCREEN_HEIGHT, fovy);
	set_window_offset(navecx, 0, 0);
	calculate_camera_transform(navecx);

	e = add_entity(navecx, ring_mesh, o->x, o->y, o->z, DARKRED);
	if (e)
		update_entity_scale(e, range);

	add_basis_ring(navecx, o->x, o->y, o->z, 1.0f, 0.0f, 0.0f, 0.0f, range * 0.98, RED);
	add_basis_ring(navecx, o->x, o->y, o->z, 1.0f, 0.0f, 0.0f, 90.0f * M_PI / 180.0, range * 0.98, DARKGREEN);
	add_basis_ring(navecx, o->x, o->y, o->z, 0.0f, 0.0f, 1.0f, 90.0f * M_PI / 180.0, range * 0.98, BLUE);

	int i;
	for (i=0; i<2; ++i) {
		int color;
		union quat ind_orientation;
		if (i==0) {
			color = RED;
			ind_orientation = o->orientation;
		} else {
			color = CYAN;
			quat_mul(&ind_orientation, &o->orientation, &o->tsd.ship.weap_orientation);
		}

		/* add heading arrow */
		union vec3 ind_pos = {{range,0,0}};
		quat_rot_vec_self(&ind_pos, &ind_orientation);
		vec3_add_self(&ind_pos, &ship_pos);
		e = add_entity(navecx, heading_indicator_mesh, ind_pos.v.x, ind_pos.v.y, ind_pos.v.z, color);
		if (e) {
			update_entity_scale(e, heading_indicator_mesh->radius*range/100.0);
			update_entity_orientation(e, &ind_orientation);
			set_render_style(e, RENDER_NORMAL);
		}

		/* heading arrow tail */
		e = add_entity(navecx, heading_ind_line_mesh, o->x, o->y, o->z, color);
		if (e) {
			update_entity_scale(e, range);
			update_entity_orientation(e, &ind_orientation);
		}
	}

	/* heading labels */
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
			if (!transform_line(navecx, x1, o->y, z1, x2, o->y, z2, &sx1, &sy1, &sx2, &sy2)) {
				sng_draw_dotted_line(sx1, sy1, sx2, sy2);
			}
			if (!transform_point(navecx, x3, o->y, z3, &sx3, &sy3)) {
				sprintf(buf, "%d", (int)math_angle_to_game_angle_degrees(i * 360.0/slices));
				sng_center_xy_draw_string(buf, font, sx3, sy3);
			}
		}
	}

	/* scan indicator */
	{
		float tx1, tz1, tx2, tz2, sx1, sy1, sx2, sy2;
		float heading = o->tsd.ship.sci_heading;
		float beam_width = fabs(o->tsd.ship.sci_beam_width);
		sng_set_foreground(LIMEGREEN);

		tx1 = o->x + cos(heading - beam_width / 2) * range * 0.05;
		tz1 = o->z - sin(heading - beam_width / 2) * range * 0.05;
		tx2 = o->x + cos(heading - beam_width / 2) * range;
		tz2 = o->z - sin(heading - beam_width / 2) * range;
		if (!transform_line(navecx, tx1, o->y, tz1, tx2, o->y, tz2, &sx1, &sy1, &sx2, &sy2)) {
			sng_draw_electric_line(sx1, sy1, sx2, sy2);
		}

		tx1 = o->x + cos(heading + beam_width / 2) * range * 0.05;
		tz1 = o->z - sin(heading + beam_width / 2) * range * 0.05;
		tx2 = o->x + cos(heading + beam_width / 2) * range;
		tz2 = o->z - sin(heading + beam_width / 2) * range;
		if (!transform_line(navecx, tx1, o->y, tz1, tx2, o->y, tz2, &sx1, &sy1, &sx2, &sy2)) {
			sng_draw_electric_line(sx1, sy1, sx2, sy2);
		}
	}

	/* add my ship */
	e = add_entity(navecx, ship_mesh_map[o->tsd.ship.shiptype], o->x, o->y, o->z, SHIP_COLOR);
	if (e) {
		set_render_style(e, science_style);
		update_entity_scale(e, range/300.0);
		update_entity_orientation(e, &o->orientation);
	}

	render_entities(navecx);

	/* draw all the rest onto the 3d scene */
	{
		int i, pwr;
		double bw, dist2, dist;
		int selected_guy_still_visible = 0, prev_selected_guy_still_visible = 0;
		int nebula_factor = 0;

		pwr = o->tsd.ship.power_data.sensors.i;
		/* Draw all the stuff */

		bw = o->tsd.ship.sci_beam_width * 180.0 / M_PI;

		sng_set_foreground(GREEN);
		pthread_mutex_lock(&universe_mutex);

		static int nlaserbeams = 0;
		static struct snis_entity* laserbeams[MAXGAMEOBJS] = {0};

		nlaserbeams = 0;
		nscience_guys = 0;
		for (i = 0; i <= snis_object_pool_highest_object(pool); i++) {
			if (!go[i].alive)
				continue;

			if (go[i].id == my_ship_id)
				continue; /* skip drawing yourself. */

			if (go[i].type == OBJTYPE_LASERBEAM || go[i].type == OBJTYPE_TRACTORBEAM) {
				laserbeams[nlaserbeams] = &go[i];
				nlaserbeams++;
				continue;
			}

			dist2 = dist3dsqrd(go[i].x - o->x, go[i].y - o->y, go[i].z - o->z);
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

			if (go[i].type == OBJTYPE_LASER || go[i].type == OBJTYPE_TORPEDO) {
				/* set projectile tween value to be the same as the popout if they pass in popout area */
				tween = selected_guy_tween;
				draw_popout_arc = 0;
			} else {
				/* get tween from bank and draw a popout arc */
				tween_get_value_and_decay(sciplane_tween, go[i].id, &tween);
				draw_popout_arc = 1;
			}

			union vec3 display_pos;
			heading_mark_to_vec3(dist, heading, mark * tween, &display_pos);
			vec3_add_self(&display_pos, &ship_pos);

			if ( draw_popout_arc && tween > 0 ) {
				/* show the flyout arc */
				sng_set_foreground(DARKTURQUOISE);
				draw_3d_mark_arc(w, gc, navecx, &ship_pos, dist, heading, mark * tween * 0.9);
			}

			float sx, sy;
			if (!transform_point(navecx, display_pos.v.x, display_pos.v.y, display_pos.v.z, &sx, &sy)) {
				snis_draw_science_guy(w, gc, &go[i], sx, sy, dist, bw, pwr, range, &go[i] == curr_science_guy, nebula_factor);
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
					tween_add_or_update(sciplane_tween, go[i].id, 0, TWEEN_EXP_DECAY, mark_popout_rate, 0, 1);
				}
			}

			/* cache screen coords for mouse picking */
			science_guy[nscience_guys].o = &go[i];
			science_guy[nscience_guys].sx = sx;
			science_guy[nscience_guys].sy = sy;
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
		}
		else if (!selected_guy_still_visible && curr_science_guy) {
			prev_science_guy = curr_science_guy;
			curr_science_guy = NULL;
		}

		/* draw in the laserbeams */
		for (i = 0; i < nlaserbeams; i++) {
			draw_sciplane_laserbeam(w, gc, navecx, o, laserbeams[i], range);
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
				if (!transform_line(navecx, x1, y1, z1, x2, y2, z2, &sx1, &sy1, &sx2, &sy2)) {
					snis_draw_line(sx1, sy1, sx2, sy2);
				}
			}
		}
	}

	remove_all_entity(navecx);
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
	union quat q, q1, q2, q3;

	if (!orange_slice) {
		orange_slice = init_circle_mesh(0, 0, 1, 60, M_PI); /* half circle in x-z plane */
		orange_slice->geometry_mode = MESH_GEOMETRY_POINTS;
	}

	quat_init_axis(&q, 0.0f, 0.0f, 1.0f, -M_PI / 2.0); /* rotate 90 degrees around z axis */
	quat_init_axis(&q1, 0.0f, 1.0f, 0.0f,
			o->tsd.ship.sci_heading - o->tsd.ship.sci_beam_width / 2.0 + M_PI / 2.0);
	quat_init_axis(&q2, 0.0f, 1.0f, 0.0f,
			o->tsd.ship.sci_heading + o->tsd.ship.sci_beam_width / 2.0 + M_PI / 2.0);
	e = add_entity(sciballecx, orange_slice, o->x, o->y, o->z, color);
	if (e) {
		quat_mul(&q3, &q1, &q);
		update_entity_orientation(e, &q3);
		update_entity_scale(e, r);
	}

	e = add_entity(sciballecx, orange_slice, o->x, o->y, o->z, color);
	if (e) {
		quat_mul(&q3, &q2, &q);
		update_entity_orientation(e, &q3);
		update_entity_scale(e, r);
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

	/* Add basis rings */
	for (i = 1; i <= 3; i++) {
		add_basis_ring(sciballecx, o->x, o->y, o->z, 1.0f, 0.0f, 0.0f, 0.0f,
					screen_radius * (float) i / 3.0f, RED);
		add_basis_ring(sciballecx, o->x, o->y, o->z, 1.0f, 0.0f, 0.0f, 90.0f * M_PI / 180.0,
					screen_radius * (float) i / 3.0f, DARKGREEN);
		add_basis_ring(sciballecx, o->x, o->y, o->z, 0.0f, 0.0f, 1.0f, 90.0f * M_PI / 180.0,
					screen_radius * (float) i / 3.0f, BLUE);
	}

	add_scanner_beam_orange_slice(sciballecx, o, screen_radius, AMBER);

	cx = SCIENCE_SCOPE_CX;
	cy = SCIENCE_SCOPE_CY;
	r = SCIENCE_SCOPE_R;
	pwr = o->tsd.ship.power_data.sensors.i;
	/* Draw all the stuff */

#if 1
	/* Draw selected coordinate */
	dist = hypot(o->x - o->sci_coordx, o->z - o->sci_coordz);
	if (dist < range) {
		tx = (o->sci_coordx - o->x) * (double) r / range;
		ty = (o->sci_coordz - o->z) * (double) r / range;
		x = (int) (tx + (double) cx);
		y = (int) (ty + (double) cy);
		snis_draw_line(x - 5, y, x + 5, y);
		snis_draw_line(x, y - 5, x, y + 5);
	}
#endif

	/* FIXME this is quite likely wrong */
        tx = sin(o->tsd.ship.sci_heading) * range;
        ty = -cos(o->tsd.ship.sci_heading) * range;

	sng_set_foreground(GREEN);
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

		dist2 = dist3dsqrd(go[i].x - o->x, go[i].y - o->y, go[i].z - o->z);
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
					go[i].type == OBJTYPE_STARBASE)
			snis_draw_3d_science_guy(w, gc, &go[i], &x, &y, dist, bw, pwr, range,
				&go[i] == curr_science_guy, 100.0 * current_zoom / 255.0, nebula_factor);

		/* cache screen coords for mouse picking */
		science_guy[nscience_guys].o = &go[i];
		science_guy[nscience_guys].sx = x;
		science_guy[nscience_guys].sy = y;
		if (&go[i] == curr_science_guy)
			selected_guy_still_visible = 1;
		nscience_guys++;
	}
	if (!selected_guy_still_visible && curr_science_guy) {
		prev_science_guy = curr_science_guy;
		curr_science_guy = NULL;
	}
	pthread_mutex_unlock(&universe_mutex);
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
	queue_to_server(packed_buffer_new("b", OPCODE_LOAD_TORPEDO));
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

static void do_adjust_byte_value(uint8_t value,  uint8_t opcode)
{
	struct snis_entity *o;

	if (!(o = find_my_ship()))
		return;
	queue_to_server(packed_buffer_new("bwb", opcode, o->id, value));
}

static void do_adjust_slider_value(struct slider *s,  uint8_t opcode)
{
	uint8_t value = (uint8_t) (255.0 * snis_slider_get_input(s));
	do_adjust_byte_value(value, opcode);
}

static void do_navzoom(struct slider *s)
{
	do_adjust_slider_value(s, OPCODE_REQUEST_NAVZOOM);
}

static void do_mainzoom(struct slider *s)
{
	do_adjust_slider_value(s, OPCODE_REQUEST_MAINZOOM);
}

static void do_throttle(struct slider *s)
{
	do_adjust_slider_value(s, OPCODE_REQUEST_THROTTLE);
}

static void do_scizoom(struct slider *s)
{
	do_adjust_slider_value(s, OPCODE_REQUEST_SCIZOOM);
}
	
static void do_warpdrive(struct slider *s)
{
	do_adjust_slider_value(s, OPCODE_REQUEST_WARPDRIVE);
}

static void do_shieldadj(struct slider *s)
{
	do_adjust_slider_value(s, OPCODE_REQUEST_SHIELD);
}

static void do_maneuvering_pwr(struct slider *s)
{
	do_adjust_slider_value(s, OPCODE_REQUEST_MANEUVERING_PWR);
}
	
static void do_tractor_pwr(struct slider *s)
{
	do_adjust_slider_value(s, OPCODE_REQUEST_TRACTOR_PWR);
}

static void do_shields_pwr(struct slider *s)
{
	do_adjust_slider_value(s, OPCODE_REQUEST_SHIELDS_PWR);
}
	
static void do_impulse_pwr(struct slider *s)
{
	do_adjust_slider_value(s, OPCODE_REQUEST_IMPULSE_PWR);
}

static void do_warp_pwr(struct slider *s)
{
	do_adjust_slider_value(s, OPCODE_REQUEST_WARP_PWR);
}

static void do_sensors_pwr(struct slider *s)
{
	do_adjust_slider_value(s, OPCODE_REQUEST_SENSORS_PWR);
}
	
static void do_phaserbanks_pwr(struct slider *s)
{
	do_adjust_slider_value(s, OPCODE_REQUEST_PHASERBANKS_PWR);
}
	
static void do_comms_pwr(struct slider *s)
{
	do_adjust_slider_value(s, OPCODE_REQUEST_COMMS_PWR);
}

static void do_maneuvering_coolant(struct slider *s)
{
	do_adjust_slider_value(s, OPCODE_REQUEST_MANEUVERING_COOLANT);
}
	
static void do_tractor_coolant(struct slider *s)
{
	do_adjust_slider_value(s, OPCODE_REQUEST_TRACTOR_COOLANT);
}

static void do_shields_coolant(struct slider *s)
{
	do_adjust_slider_value(s, OPCODE_REQUEST_SHIELDS_COOLANT);
}
	
static void do_impulse_coolant(struct slider *s)
{
	do_adjust_slider_value(s, OPCODE_REQUEST_IMPULSE_COOLANT);
}

static void do_warp_coolant(struct slider *s)
{
	do_adjust_slider_value(s, OPCODE_REQUEST_WARP_COOLANT);
}

static void do_sensors_coolant(struct slider *s)
{
	do_adjust_slider_value(s, OPCODE_REQUEST_SENSORS_COOLANT);
}
	
static void do_phaserbanks_coolant(struct slider *s)
{
	do_adjust_slider_value(s, OPCODE_REQUEST_PHASERBANKS_COOLANT);
}
	
static void do_comms_coolant(struct slider *s)
{
	do_adjust_slider_value(s, OPCODE_REQUEST_COMMS_COOLANT);
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
DEFINE_SAMPLER_FUNCTION(sample_phasercharge, tsd.ship.phaser_charge, 255.0, 0)
DEFINE_SAMPLER_FUNCTION(sample_phaser_wavelength, tsd.ship.phaser_wavelength, 255.0 * 2.0, 10.0)
DEFINE_SAMPLER_FUNCTION(sample_navzoom, tsd.ship.navzoom, 255.0, 0.0)
DEFINE_SAMPLER_FUNCTION(sample_mainzoom, tsd.ship.mainzoom, 255.0, 0.0)

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

DEFINE_CURRENT_SAMPLER(coolant_data, warp) /* defines sample_coolant_data_warp_current */
DEFINE_CURRENT_SAMPLER(coolant_data, sensors) /* defines sample_coolant_data_sensors_current */
DEFINE_CURRENT_SAMPLER(coolant_data, phasers) /* defines sample_coolant_data_phasers_current */
DEFINE_CURRENT_SAMPLER(coolant_data, maneuvering) /* defines sample_coolant_data_maneuvering_current */
DEFINE_CURRENT_SAMPLER(coolant_data, shields) /* defines sample_coolant_data_shields_current */
DEFINE_CURRENT_SAMPLER(coolant_data, comms) /* defines sample_coolant_data_comms_current */
DEFINE_CURRENT_SAMPLER(coolant_data, impulse) /* defines sample_coolant_data_impulse_current */
DEFINE_CURRENT_SAMPLER(coolant_data, tractor) /* defines sample_coolant_data_tractor_current */


static void do_phaser_wavelength(__attribute__((unused)) struct slider *s)
{
	do_adjust_slider_value(s, OPCODE_REQUEST_LASER_WAVELENGTH);
}

static void wavelen_updown_button_pressed(int direction)
{
	uint8_t value = (uint8_t) (255.0 * sample_phaser_wavelength());
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
	do_adjust_byte_value(value, OPCODE_REQUEST_LASER_WAVELENGTH);
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

CREATE_TEMPERATURE_SAMPLER_FUNC(shield) /* sample_shield_temperature defined here */
CREATE_TEMPERATURE_SAMPLER_FUNC(impulse) /* sample_impulse_temperature defined here */
CREATE_TEMPERATURE_SAMPLER_FUNC(warp) /* sample_warp_temperature defined here */
CREATE_TEMPERATURE_SAMPLER_FUNC(maneuvering) /* sample_maneuvering_temperature defined here */
CREATE_TEMPERATURE_SAMPLER_FUNC(phaser_banks) /* sample_phaser_banks_temperature defined here */
CREATE_TEMPERATURE_SAMPLER_FUNC(sensors) /* sample_sensors_temperature defined here */
CREATE_TEMPERATURE_SAMPLER_FUNC(comms) /* sample_comms_temperature defined here */
CREATE_TEMPERATURE_SAMPLER_FUNC(tractor) /* sample_tractor_temperature defined here */

static void engage_warp_button_pressed(__attribute__((unused)) void *cookie)
{
	do_adjust_byte_value(0,  OPCODE_ENGAGE_WARP);
}

static void reverse_button_pressed(__attribute__((unused)) void *s)
{
	struct snis_entity *o;

	if (!(o = find_my_ship()))
		return;
	snis_button_set_color(nav_ui.reverse_button, !o->tsd.ship.reverse ? RED : AMBER);
	do_adjust_byte_value(!o->tsd.ship.reverse,  OPCODE_REQUEST_REVERSE);
}

static void ui_add_slider(struct slider *s, int active_displaymode)
{
	struct ui_element *uie;

	uie = ui_element_init(s, ui_slider_draw, ui_slider_button_press,
						active_displaymode, &displaymode);
	ui_element_list_add_element(&uiobjs, uie); 
}

static void ui_add_button(struct button *b, int active_displaymode)
{
	struct ui_element *uie;

	uie = ui_element_init(b, ui_button_draw, ui_button_button_press,
						active_displaymode, &displaymode);
	ui_element_list_add_element(&uiobjs, uie); 
}

static void ui_add_label(struct label *l, int active_displaymode)
{
	struct ui_element *uie;

	uie = ui_element_init(l, ui_label_draw, NULL,
						active_displaymode, &displaymode);
	ui_element_list_add_element(&uiobjs, uie); 
}

static void ui_add_gauge(struct gauge *g, int active_displaymode)
{
	struct ui_element *uie;

	uie = ui_element_init(g, ui_gauge_draw, NULL,
						active_displaymode, &displaymode);
	ui_element_list_add_element(&uiobjs, uie); 
}

static void ui_add_text_window(struct text_window *tw, int active_displaymode)
{
	struct ui_element *uie;

	uie = ui_element_init(tw, ui_text_window_draw, NULL,
						active_displaymode, &displaymode);
	ui_element_list_add_element(&uiobjs, uie); 
}

static void ui_add_text_input_box(struct snis_text_input_box *t, int active_displaymode)
{
	struct ui_element *uie;

	uie = ui_element_init(t, ui_text_input_draw, ui_text_input_button_press,
						active_displaymode, &displaymode);
	ui_element_set_focus_callback(uie, ui_text_input_box_set_focus);
	ui_element_get_keystrokes(uie, ui_text_input_keypress, NULL);
	ui_element_list_add_element(&uiobjs, uie); 
}

static void init_lobby_ui()
{
	lobby_ui.lobby_cancel_button = snis_button_init(650, 520, 100, LINEHEIGHT * 2,
			"CANCEL", GREEN, NANO_FONT, lobby_cancel_button_pressed, NULL);
	ui_add_button(lobby_ui.lobby_cancel_button, DISPLAYMODE_LOBBYSCREEN);
}

static double sample_phaser_wavelength(void);
static void init_weapons_ui(void)
{
	weapons.phaser_bank_gauge = gauge_init(280, 550, 45, 0.0, 100.0, -120.0 * M_PI / 180.0,
			120.0 * 2.0 * M_PI / 180.0, RED, AMBER,
			10, "CHARGE", sample_phasercharge);
	gauge_add_needle(weapons.phaser_bank_gauge, sample_phaser_power, RED);
	gauge_fill_background(weapons.phaser_bank_gauge, BLACK, 0.75);
	weapons.phaser_wavelength = gauge_init(520, 550, 45, 10.0, 60.0, -120.0 * M_PI / 180.0,
			120.0 * 2.0 * M_PI / 180.0, RED, AMBER,
			10, "WAVE LEN", sample_phaser_wavelength);
	gauge_fill_background(weapons.phaser_wavelength, BLACK, 0.75);
	weapons.wavelen_slider = snis_slider_init(315, 588, 170, 15, AMBER, "",
				"10", "60", 10, 60, sample_phaser_wavelength,
				do_phaser_wavelength);
	ui_add_slider(weapons.wavelen_slider, DISPLAYMODE_WEAPONS);
	ui_add_gauge(weapons.phaser_bank_gauge, DISPLAYMODE_WEAPONS);
	ui_add_gauge(weapons.phaser_wavelength, DISPLAYMODE_WEAPONS);
}

static void show_death_screen(GtkWidget *w)
{
	char buf[100];

	sng_set_foreground(RED);
	sprintf(buf, "YOUR SHIP");
	sng_abs_xy_draw_string(buf, BIG_FONT, 20, 150);
	sprintf(buf, "HAS BEEN");
	sng_abs_xy_draw_string(buf, BIG_FONT, 20, 250);
	sprintf(buf, "BLOWN TO");
	sng_abs_xy_draw_string(buf, BIG_FONT, 20, 350);
	sprintf(buf, "SMITHEREENS");
	sng_abs_xy_draw_string(buf, BIG_FONT, 20, 450);
	sprintf(buf, "RESPAWNING IN %d SECONDS", go[my_ship_oid].respawn_time);
	sng_abs_xy_draw_string(buf, TINY_FONT, 20, 500);
}

static void show_manual_weapons(GtkWidget *w)
{
	show_weapons_camera_view(w);
	show_common_screen(w, "WEAPONS");
}

static double sample_warpdrive(void);
static void init_nav_ui(void)
{
	int x, y, gauge_radius;

	x = 0;
	y = -340;
	gauge_radius = 80;
	
	nav_ui.warp_slider = snis_slider_init(590 + x, SCREEN_HEIGHT - 40 + y, 160, 15, AMBER, "",
				"0", "100", 0.0, 255.0, sample_power_data_warp_current,
				do_warpdrive);
	snis_slider_set_fuzz(nav_ui.warp_slider, 3);
	nav_ui.navzoom_slider = snis_slider_init(10, 80, 200, 15, AMBER, "ZOOM",
				"1", "10", 0.0, 100.0, sample_navzoom,
				do_navzoom);
	nav_ui.throttle_slider = snis_slider_init(SCREEN_WIDTH - 30 + x, 40, 230, 15,
				AMBER, "THROTTLE", "1", "10", 0.0, 255.0, sample_power_data_impulse_current,
				do_throttle);
	snis_slider_set_fuzz(nav_ui.throttle_slider, 3);
	snis_slider_set_vertical(nav_ui.throttle_slider, 1);
	nav_ui.warp_gauge = gauge_init(SCREEN_WIDTH - gauge_radius - 40, gauge_radius + 5,
				gauge_radius, 0.0, 10.0, -120.0 * M_PI / 180.0,
				120.0 * 2.0 * M_PI / 180.0, RED, AMBER,
				10, "WARP", sample_warpdrive);
	nav_ui.engage_warp_button = snis_button_init(620 + x, 520 + y, 125, 25, "ENGAGE WARP", AMBER,
				NANO_FONT, engage_warp_button_pressed, NULL);
	nav_ui.reverse_button = snis_button_init(SCREEN_WIDTH - 40 + x, 5, 30, 25, "R", AMBER,
			NANO_FONT, reverse_button_pressed, NULL);
	ui_add_slider(nav_ui.warp_slider, DISPLAYMODE_NAVIGATION);
	ui_add_slider(nav_ui.navzoom_slider, DISPLAYMODE_NAVIGATION);
	ui_add_slider(nav_ui.throttle_slider, DISPLAYMODE_NAVIGATION);
	ui_add_button(nav_ui.engage_warp_button, DISPLAYMODE_NAVIGATION);
	ui_add_button(nav_ui.reverse_button, DISPLAYMODE_NAVIGATION);
	ui_add_gauge(nav_ui.warp_gauge, DISPLAYMODE_NAVIGATION);
	navecx = entity_context_new(5000, 1000);
	tridentecx = entity_context_new(10, 0);
}

void draw_orientation_trident(GtkWidget *w, GdkGC *gc, struct snis_entity *o, float rx, float ry, float rr)
{
	static struct mesh *xz_ring_mesh = 0;
	if (!xz_ring_mesh) {
		xz_ring_mesh = init_circle_mesh(0, 0, 1, 40, 2.0*M_PI);
	}

	struct entity *e;

	union quat cam_orientation = o->orientation;
	union vec3 center_pos = {{0, 0, 0}};

	/* given field of view angle, calculate how far away to show a radius 1.0 sphere */
	float fovy = 20.0 * M_PI / 180.0;
	float dist_to_cam = 1.05 / tan(fovy/2.0);

        set_renderer(tridentecx, WIREFRAME_RENDERER);
	camera_set_parameters(tridentecx, dist_to_cam-1.0, dist_to_cam+1.0,
				rr*ASPECT_RATIO, rr, fovy);
	set_window_offset(tridentecx, rx-rr*ASPECT_RATIO/2.0, ry-rr/2.0);

	/* figure out the camera positions */
	union vec3 camera_up = { {0, -1, 0} };
	quat_rot_vec_self(&camera_up, &cam_orientation);

	union vec3 camera_pos = { {dist_to_cam, 0, 0} };
	quat_rot_vec_self(&camera_pos, &cam_orientation);
	vec3_add_self(&camera_pos, &center_pos);

	union vec3 camera_lookat = {{0, 0, 0}};
	quat_rot_vec_self(&camera_lookat, &cam_orientation);
	vec3_add_self(&camera_lookat, &center_pos);

	camera_assign_up_direction(tridentecx, camera_up.v.x, camera_up.v.y, camera_up.v.z);
	camera_set_pos(tridentecx, camera_pos.v.x, camera_pos.v.y, camera_pos.v.z);
	camera_look_at(tridentecx, camera_lookat.v.x, camera_lookat.v.y, camera_lookat.v.z);

	calculate_camera_transform(tridentecx);

	struct material yaw_material = {
		.color_by_w = { COLOR_LIGHTER(CYAN, 100),
				CYAN,
				COLOR_DARKER(CYAN, 80),
				dist_to_cam - 1.0,
				dist_to_cam,
				dist_to_cam + 1.0 },
		.type = MATERIAL_COLOR_BY_W,
		.billboard_type = MATERIAL_BILLBOARD_TYPE_NONE };

	struct material pitch_material = {
		.color_by_w = { COLOR_LIGHTER(GREEN, 100),
				GREEN,
				COLOR_DARKER(GREEN, 80),
				dist_to_cam - 1.0,
				dist_to_cam,
				dist_to_cam + 1.0 },
		.type = MATERIAL_COLOR_BY_W,
		.billboard_type = MATERIAL_BILLBOARD_TYPE_NONE };

	/* add yaw axis */
	e = add_entity(tridentecx, xz_ring_mesh, center_pos.v.x, center_pos.v.y, center_pos.v.z, CYAN);
	if (e)
		update_entity_material(e, &yaw_material);

	/* add pitch1 axis */
	union quat pitch1_orientation;
	quat_init_axis(&pitch1_orientation, 1, 0, 0, M_PI/2.0);
	e = add_entity(tridentecx, xz_ring_mesh, center_pos.v.x, center_pos.v.y, center_pos.v.z, GREEN);
	if (e) {
		update_entity_orientation(e, &pitch1_orientation);
		update_entity_material(e, &pitch_material);
	}

	/* add pitch2 axis */
	union quat pitch2_orientation;
	quat_init_axis(&pitch2_orientation, 0, 0, 1, M_PI/2.0);
	e = add_entity(tridentecx, xz_ring_mesh, center_pos.v.x, center_pos.v.y, center_pos.v.z, GREEN);
	if (e) {
		update_entity_orientation(e, &pitch2_orientation);
		update_entity_material(e, &pitch_material);
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

	render_entities(tridentecx);

	remove_all_entity(tridentecx);
}

#if 0
#define NAV_SCOPE_X 20
#define NAV_SCOPE_Y 70 
#define NAV_SCOPE_W 500
#define NAV_SCOPE_H NAV_SCOPE_W
#define NAV_SCOPE_R (NAV_SCOPE_H / 2)
#define NAV_SCOPE_CX (NAV_SCOPE_X + NAV_SCOPE_R)
#define NAV_SCOPE_CY (NAV_SCOPE_Y + NAV_SCOPE_R)
#endif
#define NAV_DATA_X 530 
#define NAV_DATA_Y 40 
#define NAV_DATA_W ((SCREEN_WIDTH - 5) - NAV_DATA_X)
#define NAV_DATA_H 270 

static void draw_science_graph(GtkWidget *w, struct snis_entity *ship, struct snis_entity *o,
		int x1, int y1, int x2, int y2);

static void draw_3d_nav_display(GtkWidget *w, GdkGC *gc)
{
	static struct mesh *ring_mesh = 0;
	static struct mesh *radar_ring_mesh[4] = {0, 0, 0, 0};
	static struct mesh *heading_ind_line_mesh = 0;
	static struct mesh *vline_mesh_pos = 0;
	static struct mesh *vline_mesh_neg = 0;
	static struct mesh *forward_line_mesh = 0;
	static int current_zoom = 0;
	/* struct entity *targeted_entity = NULL; */
	struct entity *science_entity = NULL;

	if (!ring_mesh) {
		ring_mesh = init_circle_mesh(0, 0, 1, 180, 2.0f * M_PI);
		radar_ring_mesh[0] = init_radar_circle_xz_plane_mesh(0, 0, 0.4, 0, 0);
		radar_ring_mesh[1] = init_radar_circle_xz_plane_mesh(0, 0, 0.6, 18, 0.2);
		radar_ring_mesh[2] = init_radar_circle_xz_plane_mesh(0, 0, 0.8, 18, 0.2);
		radar_ring_mesh[3] = init_radar_circle_xz_plane_mesh(0, 0, 1.0, 36, 0.2);
		vline_mesh_pos = init_line_mesh(0, 0, 0, 0, 1, 0);
		vline_mesh_neg = init_line_mesh(0, 0, 0, 0, -1, 0);
		heading_ind_line_mesh = init_line_mesh(0.7, 0, 0, 1, 0, 0);
		forward_line_mesh = init_line_mesh(1, 0, 0, 0.5, 0, 0);
	}

	struct snis_entity *o;
	struct entity *e = NULL;
	double max_possible_screen_radius = 0.09 * XKNOWN_DIM;
	double screen_radius;
	double visible_distance;
	int science_style = RENDER_NORMAL;

	if (!(o = find_my_ship()))
		return;

	static int last_timer = 0;
	int first_frame = (timer != last_timer+1);
	last_timer = timer;

	if (first_frame)
		current_zoom = o->tsd.ship.navzoom;
	else
		current_zoom = newzoom(current_zoom, o->tsd.ship.navzoom);

	/* idiot lights for low power of various systems */
	const int low_power_threshold = 10;
	sng_set_foreground(RED);
	if (o->tsd.ship.power_data.sensors.i < low_power_threshold) {
		sng_abs_xy_draw_string("LOW SENSOR POWER", NANO_FONT, 320, 65);
	}
	if (o->tsd.ship.power_data.maneuvering.i < low_power_threshold) {
		sng_abs_xy_draw_string("LOW MANEUVERING POWER", NANO_FONT, 320, 80);
	}
	if (o->tsd.ship.power_data.impulse.r2 < low_power_threshold) {
		sng_abs_xy_draw_string("LOW IMPULSE POWER", NANO_FONT, 320, 95);
	}
	if (o->tsd.ship.power_data.warp.r2 < low_power_threshold) {
		sng_abs_xy_draw_string("LOW WARP POWER", NANO_FONT, 610, 228);
	}

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
		quat_nlerp(&cam_orientation, &cam_orientation, &o->orientation, 0.1);
	}

	union vec3 camera_up = {{0,1,0}};
	quat_rot_vec_self(&camera_up, &cam_orientation);

	/* rotate camera to be behind my ship */
	union vec3 camera_pos = {{ -screen_radius * 1.85, screen_radius * 0.85, 0}};
	float camera_pos_len = vec3_magnitude(&camera_pos);
	quat_rot_vec_self(&camera_pos, &cam_orientation);
	vec3_add_self(&camera_pos, &ship_pos);

	union vec3 camera_lookat = {{screen_radius*0.20, 0, 0}};
	quat_rot_vec_self(&camera_lookat, &cam_orientation);
	vec3_add_self(&camera_lookat, &ship_pos);

	camera_assign_up_direction(navecx, camera_up.v.x, camera_up.v.y, camera_up.v.z);
	camera_set_pos(navecx, camera_pos.v.x, camera_pos.v.y, camera_pos.v.z);
	camera_look_at(navecx, camera_lookat.v.x, camera_lookat.v.y, camera_lookat.v.z);

	set_renderer(navecx, WIREFRAME_RENDERER);
	camera_set_parameters(navecx, 1.0, camera_pos_len+screen_radius*2,
				SCREEN_WIDTH, SCREEN_HEIGHT, ANGLE_OF_VIEW * M_PI / 180.0);
	calculate_camera_transform(navecx);

	int in_nebula = 0;
	int i;

	for (i=0; i<4; ++i) {
		e = add_entity(navecx, radar_ring_mesh[i], o->x - ship_normal.v.x, o->y - ship_normal.v.y,
			o->z - ship_normal.v.z, DARKRED);
		if (e) {
			update_entity_scale(e, screen_radius);
			update_entity_orientation(e, &o->orientation);
		}
	}

	for (i=0; i<2; ++i) {
		int color = (i==0) ? CYAN : GREEN;

		union quat ind_orientation;
		if (i == 0)
			quat_mul(&ind_orientation, &o->orientation, &o->tsd.ship.weap_orientation);
		else {
			if (!curr_science_guy)
				continue;

			union vec3 up = { { 0, 1, 0 } };
			union vec3 xaxis = { { 1, 0, 0 } };
			union vec3 to_science_guy = { { curr_science_guy->x - o->x,
						  curr_science_guy->y - o->y,
						  curr_science_guy->z - o->z, } };
			quat_from_u2v(&ind_orientation, &xaxis, &to_science_guy, &up);
		}

		/* heading arrow head */
		union vec3 ind_pos = {{screen_radius,0,0}};
		quat_rot_vec_self(&ind_pos, &ind_orientation);
		vec3_add_self(&ind_pos, &ship_pos);
		e = add_entity(navecx, heading_indicator_mesh, ind_pos.v.x, ind_pos.v.y, ind_pos.v.z, color);
		if (e) {
			update_entity_scale(e, heading_indicator_mesh->radius*screen_radius/100.0);
			update_entity_orientation(e, &ind_orientation);
			set_render_style(e, RENDER_NORMAL);
		}

		/* heading arrow tail */
		e = add_entity(navecx, heading_ind_line_mesh, o->x, o->y, o->z, color);
		if (e) {
			update_entity_scale(e, screen_radius);
			update_entity_orientation(e, &ind_orientation);
		}
	}

	/* ship forward vector */
	e = add_entity(navecx, forward_line_mesh, o->x, o->y, o->z, WHITE);
	if (e) {
		update_entity_scale(e, screen_radius);
		update_entity_orientation(e, &o->orientation);
	}

	double sector_size = XKNOWN_DIM / 10.0;
	if (current_zoom > 100 ) {
		/* turn on fine sector lines */
		sector_size /= 10.0;
	}

	/* add the dynamic starts */
	/* TODO */

	/* draw some static in the region that we can't see because of sensor power */
	if (screen_radius > visible_distance) {
		union vec3 u, v;
		plane_vector_u_and_v_from_normal(&u, &v, &ship_normal);

		/* specs to draw is based on the area of the annulus */
		int specs = 500 * (screen_radius * screen_radius -
				visible_distance * visible_distance) / (screen_radius * screen_radius);

		sng_set_foreground(GRAY+192);
		for (i=0; i<specs; i++) {
			union vec3 point;
			random_point_in_3d_annulus(visible_distance, screen_radius, &ship_pos, &u, &v, &point);

			float sx, sy;
			if (!transform_point(navecx, point.v.x, point.v.y, point.v.z, &sx, &sy)) {
				sng_draw_point(sx, sy);
			}
		}
	}

	/* Draw all the stuff */
	pthread_mutex_lock(&universe_mutex);

	/* add my ship */
	e = add_entity(navecx, ship_mesh_map[o->tsd.ship.shiptype], o->x, o->y, o->z, SHIP_COLOR);
	if (e) {
		set_render_style(e, science_style);
		update_entity_scale(e, ship_scale);
		update_entity_orientation(e, &o->orientation);
	}

	struct material wireframe_material;
	material_init_wireframe_sphere_clip(&wireframe_material);
	wireframe_material.wireframe_sphere_clip.center = e;
	wireframe_material.wireframe_sphere_clip.radius = MIN(visible_distance, screen_radius);
	wireframe_material.wireframe_sphere_clip.radius_fade = radius_fadeout_percent;

	float display_radius = MIN(visible_distance, screen_radius) * (1.0 + radius_fadeout_percent);

	for (i = 0; i <= snis_object_pool_highest_object(pool); i++) {
		double dist;

		if (!go[i].alive)
			continue;

		if (go[i].id == my_ship_id) {
			continue;
		}

		if (go[i].type == OBJTYPE_LASERBEAM || go[i].type == OBJTYPE_TRACTORBEAM) {
			draw_3d_laserbeam(w, gc, navecx, o, &go[i], display_radius);
			continue;
		}

		if (!go[i].entity)
			continue;

		dist = dist3d(go[i].x - o->x, go[i].y - o->y, go[i].z - o->z);

		/* use the distance to the edge and not the center */
		struct mesh *obj_entity_mesh = entity_get_mesh(go[i].entity);
		if (obj_entity_mesh) {
			float obj_radius = obj_entity_mesh->radius * fabs(entity_get_scale(go[i].entity));
			if (dist > obj_radius) {
				dist -= obj_radius;
			}
		}

		if (dist > display_radius)
			continue; /* not close enough */

		if (in_nebula && snis_randn(1000) < 850)
			continue;

		struct entity *contact = 0;

		switch (go[i].type) {
		case OBJTYPE_EXPLOSION:
		case OBJTYPE_DERELICT:
			break;
		case OBJTYPE_WORMHOLE:
		case OBJTYPE_LASER:
		case OBJTYPE_TORPEDO:
		case OBJTYPE_ASTEROID:
		case OBJTYPE_PLANET:
		case OBJTYPE_STARBASE:
		case OBJTYPE_SHIP2:
		case OBJTYPE_CARGO_CONTAINER:
		case OBJTYPE_SHIP1:
		{
			struct mesh *m = entity_get_mesh(go[i].entity);

			if (go[i].type == OBJTYPE_TORPEDO) {
				contact = add_entity(navecx, torpedo_nav_mesh, go[i].x, go[i].y, go[i].z, ORANGERED);
				if (contact) {
					set_render_style(contact, science_style | RENDER_BRIGHT_LINE | RENDER_NO_FILL);
					entity_set_user_data(contact, &go[i]); /* for debug */
				}
			} else if (go[i].type == OBJTYPE_LASER) {
				contact = add_entity(navecx, laserbeam_nav_mesh, go[i].x, go[i].y, go[i].z,
					LASER_COLOR);
				if (contact) {
					set_render_style(contact, science_style | RENDER_BRIGHT_LINE | RENDER_NO_FILL);
					entity_set_user_data(contact, &go[i]); /* for debug */
				}
			} else {
				contact = add_entity(navecx, m, go[i].x, go[i].y, go[i].z, GREEN);
				if (contact) {
					set_render_style(contact, science_style);
					entity_set_user_data(contact, &go[i]);
				}
			}
			if (contact) {
				update_entity_scale(contact, entity_get_scale(go[i].entity));
				update_entity_orientation(contact, entity_get_orientation(go[i].entity));
			}
#if 0
			if (o->tsd.ship.ai[0].u.attack.victim_id != -1 && go[i].id == o->tsd.ship.ai[0].u.attack.victim_id)
				targeted_entity = contact;
#endif
			if (curr_science_guy == &go[i])
				science_entity = contact;
			break;
		}
		case OBJTYPE_SPACEMONSTER: /* invisible to instruments */
		case OBJTYPE_NEBULA:
			break;
		}

		if ( contact ) {
			int draw_contact_offset_and_ring = 1;
			float contact_scale = 1.0;

			update_entity_material(contact, &wireframe_material);

			switch (go[i].type) {
			case OBJTYPE_PLANET:
				contact_scale = ((255.0 - current_zoom) / 255.0) * 0.0 + 1.0;
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
			}

			/* update the scale based on current scale */
			if (contact)
				update_entity_scale(contact, entity_get_scale(contact) * contact_scale);

			/* add line from center disk to contact in z axis */
			if (draw_contact_offset_and_ring && contact) {
				union vec3 contact_pos = { { go[i].x, go[i].y, go[i].z } };
				union vec3 ship_plane_proj;
				float proj_distance = 0;

				/* first find the point where the contact is orthogonally projected onto the ships normal plane */

				/* orthogonal projection of point onto plane
				   q_proj = q - dot( q - p, n) * n
				   q = point to project, p = point on plane, n = normal to plane */
				union vec3 temp1, temp2;
				proj_distance = vec3_dot(vec3_sub(&temp1, &contact_pos, &ship_pos), &ship_normal);
				vec3_sub(&ship_plane_proj, &contact_pos, vec3_mul(&temp2, &ship_normal, proj_distance));

				float contact_radius = entity_get_mesh(contact)->radius * fabs(entity_get_scale(contact));
				float contact_ring_radius = 0;

				if ( fabs(proj_distance) < contact_radius) {
					/* contact intersacts the ship normal plane so make the radius the size of that intersection */
					contact_ring_radius = sqrt(contact_radius*contact_radius - proj_distance*proj_distance);
				}
				if (contact_ring_radius < contact_radius/5.0) {
					/* set a lower bound on size */
					contact_ring_radius = contact_radius/5.0;
				}

				if (proj_distance > 0)
					e = add_entity(navecx, vline_mesh_neg, contact_pos.v.x, contact_pos.v.y,
						contact_pos.v.z, DARKRED);
				else
					e = add_entity(navecx, vline_mesh_pos, contact_pos.v.x, contact_pos.v.y,
						contact_pos.v.z, DARKRED);
				if (e) {
					update_entity_scale(e, abs(proj_distance));
					update_entity_orientation(e, &o->orientation);
				}

				e = add_entity(navecx, ring_mesh, ship_plane_proj.v.x,
						ship_plane_proj.v.y, ship_plane_proj.v.z, RED);
				if (e) {
					update_entity_scale(e, contact_ring_radius);
					update_entity_orientation(e, &o->orientation);
				}
			}
		}
	}
	render_entities(navecx);

	draw_orientation_trident(w, gc, o, 75, 175, 100);

	/* Draw labels on ships... */
	sng_set_foreground(GREEN);
	for (i = 0; i <= get_entity_count(navecx); i++) {
		float sx, sy;
		char buffer[100];
		struct entity *e;
		struct snis_entity *o;

		e = get_entity(navecx, i);
		if (!e)
			continue;
		o = entity_get_user_data(e);
		if (!o)
			continue; 
		entity_get_screen_coords(e, &sx, &sy);
		if (o->sdata.science_data_known) {
			sprintf(buffer, "%s", o->sdata.name);
			sng_abs_xy_draw_string(buffer, NANO_FONT, sx + 10, sy - 15);
		}
#if 0
		sprintf(buffer, "%3.1f,%6.1f,%6.1f,%6.1f",
				o->heading * 180.0 / M_PI, o->x, o->y, o->z);
		sng_abs_xy_draw_string(buffer, NANO_FONT, sx + 10, sy);
#endif
	}

#if 0
	/* Draw targeting indicator on 3d nav screen */
	if (targeted_entity && entity_onscreen(targeted_entity)) {
		float sx, sy;

		entity_get_screen_coords(targeted_entity, &sx, &sy);
		draw_targeting_indicator(w, gc, sx, sy, TARGETING_COLOR, 0);
	}
#endif
	if (science_entity && entity_onscreen(science_entity)) {
		float sx, sy;

		entity_get_screen_coords(science_entity, &sx, &sy);
		draw_targeting_indicator(w, gc, sx, sy, SCIENCE_SELECT_COLOR, 0);
	}

	pthread_mutex_unlock(&universe_mutex);

	remove_all_entity(navecx);
}

static void show_navigation(GtkWidget *w)
{
	char buf[100];
	struct snis_entity *o;
	int sectorx, sectorz;
	double display_heading;
	static int current_zoom = 0;
	union euler ypr;

	sng_set_foreground(GREEN);

	if (!(o = find_my_ship()))
		return;

	snis_slider_set_input(nav_ui.warp_slider, o->tsd.ship.power_data.warp.r1/255.0 );
	snis_slider_set_input(nav_ui.navzoom_slider, o->tsd.ship.navzoom/255.0 );
	snis_slider_set_input(nav_ui.throttle_slider, o->tsd.ship.power_data.impulse.r1/255.0 );
	snis_button_set_color(nav_ui.reverse_button, o->tsd.ship.reverse ? RED : AMBER);

	current_zoom = newzoom(current_zoom, o->tsd.ship.navzoom);
	sectorx = floor(10.0 * o->x / (double) XKNOWN_DIM);
	sectorz = floor(10.0 * o->z / (double) ZKNOWN_DIM);
	sprintf(buf, "SECTOR: %c%d (%5.2lf, %5.2lf, %5.2lf)", sectorz + 'A', sectorx, o->x, o->y, o->z);
	sng_abs_xy_draw_string(buf, NANO_FONT, 200, LINEHEIGHT);

	double display_mark;
	to_snis_heading_mark(&o->orientation, &display_heading, &display_mark);
	sprintf(buf, "HEADING: %3.1lf MARK: %3.1lf", radians_to_degrees(display_heading), radians_to_degrees(display_mark));
	sng_abs_xy_draw_string(buf, NANO_FONT, 200, 1.5 * LINEHEIGHT);

	quat_to_euler(&ypr, &o->orientation);	
	sng_set_foreground(GREEN);
	draw_3d_nav_display(w, gc);
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
	queue_to_server(packed_buffer_new("b", OPCODE_REQUEST_ROBOT_GRIPPER));
}

static void robot_auto_button_pressed(void *x)
{
	queue_to_server(packed_buffer_new("bb", OPCODE_ROBOT_AUTO_MANUAL, 1));
}

static void robot_manual_button_pressed(void *x)
{
	queue_to_server(packed_buffer_new("bb", OPCODE_ROBOT_AUTO_MANUAL, 0));
}

static void init_damcon_ui(void)
{
	damcon_ui.engineering_button = snis_button_init(630, 550, 140, 25, "ENGINEERING", AMBER,
			NANO_FONT, main_engineering_button_pressed, (void *) 0);
	damcon_ui.robot_controls = snis_label_init(630, 30, "ROBOT CONTROLS", AMBER, NANO_FONT);
	
	damcon_ui.robot_forward_button = snis_button_init(650, 60, 90, 25, "FORWARD", AMBER, NANO_FONT,
							robot_forward_button_pressed, (void *) 0);
	damcon_ui.robot_left_button = snis_button_init(630, 100, 25, 25, "L", AMBER, NANO_FONT,
							robot_left_button_pressed, (void *) 0);
	damcon_ui.robot_right_button = snis_button_init(740, 100, 25, 25, "R", AMBER, NANO_FONT,
							robot_right_button_pressed, (void *) 0);
	damcon_ui.robot_backward_button = snis_button_init(650, 140, 90, 25, "BACKWARD", AMBER, NANO_FONT,
							robot_backward_button_pressed, (void *) 0);
	damcon_ui.robot_gripper_button = snis_button_init(650, 180, 90, 25, "GRIPPER", AMBER, NANO_FONT,
							robot_gripper_button_pressed, (void *) 0);
	damcon_ui.robot_auto_button = snis_button_init(400, 30, 90, 25, "AUTO", AMBER, NANO_FONT,
							robot_auto_button_pressed, (void *) 0);
	damcon_ui.robot_manual_button = snis_button_init(500, 30, 90, 25, "MANUAL", WHITE, NANO_FONT,
							robot_manual_button_pressed, (void *) 0);

	ui_add_button(damcon_ui.engineering_button, DISPLAYMODE_DAMCON);
	ui_add_button(damcon_ui.robot_forward_button, DISPLAYMODE_DAMCON);
	ui_add_button(damcon_ui.robot_left_button, DISPLAYMODE_DAMCON);
	ui_add_button(damcon_ui.robot_right_button, DISPLAYMODE_DAMCON);
	ui_add_button(damcon_ui.robot_backward_button, DISPLAYMODE_DAMCON);
	ui_add_button(damcon_ui.robot_gripper_button, DISPLAYMODE_DAMCON);
	ui_add_button(damcon_ui.robot_auto_button, DISPLAYMODE_DAMCON);
	ui_add_button(damcon_ui.robot_manual_button, DISPLAYMODE_DAMCON);
	ui_add_label(damcon_ui.robot_controls, DISPLAYMODE_DAMCON);
}

struct engineering_ui {
	struct gauge *fuel_gauge;
	struct gauge *amp_gauge;
	struct gauge *voltage_gauge;
	struct gauge *temp_gauge;
	struct button *damcon_button;
	struct button *preset1_button;
	struct button *preset2_button;
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
	struct slider *shield_control_slider;

	struct slider *shield_damage;
	struct slider *impulse_damage;
	struct slider *warp_damage;
	struct slider *maneuvering_damage;
	struct slider *phaser_banks_damage;
	struct slider *sensors_damage;
	struct slider *comms_damage;
	struct slider *tractor_damage;

	struct slider *shield_temperature;
	struct slider *impulse_temperature;
	struct slider *warp_temperature;
	struct slider *maneuvering_temperature;
	struct slider *phaser_banks_temperature;
	struct slider *sensors_temperature;
	struct slider *comms_temperature;
	struct slider *tractor_temperature;

	int selected_subsystem;
} eng_ui;

static void damcon_button_pressed(void *x)
{
	displaymode = DISPLAYMODE_DAMCON;
}

static void preset1_button_pressed(void *x)
{
	/* a "normal" preset, note only one poke has sound to avoid noise */
	snis_slider_poke_input(eng_ui.shield_slider, 0.95, 1);
	snis_slider_poke_input(eng_ui.shield_coolant_slider, 1.0, 0);
	snis_slider_poke_input(eng_ui.maneuvering_slider, 0.95, 0);
	snis_slider_poke_input(eng_ui.maneuvering_coolant_slider, 1.0, 0);
	snis_slider_poke_input(eng_ui.warp_slider, 0.0, 0);
	snis_slider_poke_input(eng_ui.warp_coolant_slider, 0.0, 0);
	snis_slider_poke_input(eng_ui.impulse_slider, 0.95, 0);
	snis_slider_poke_input(eng_ui.impulse_coolant_slider, 1.0, 0);
	snis_slider_poke_input(eng_ui.sensors_slider, 0.95, 0);
	snis_slider_poke_input(eng_ui.sensors_coolant_slider, 1.0, 0);
	snis_slider_poke_input(eng_ui.comm_slider, 0.0, 0);
	snis_slider_poke_input(eng_ui.comm_coolant_slider, 0.0, 0);
	snis_slider_poke_input(eng_ui.phaserbanks_slider, 0.95, 0);
	snis_slider_poke_input(eng_ui.phaserbanks_coolant_slider, 1.0, 0);
	snis_slider_poke_input(eng_ui.tractor_slider, 0.0, 0);
	snis_slider_poke_input(eng_ui.tractor_coolant_slider, 0.0, 0);
	snis_slider_poke_input(eng_ui.shield_control_slider, 1.0, 0);
}

static void preset2_button_pressed(void *x)
{
	/* an "all stop" preset, note only one poke has sound to avoid noise */
	snis_slider_poke_input(eng_ui.shield_slider, 0.0, 1);
	snis_slider_poke_input(eng_ui.shield_coolant_slider, 0.3, 0);
	snis_slider_poke_input(eng_ui.maneuvering_slider, 0.0, 0);
	snis_slider_poke_input(eng_ui.maneuvering_coolant_slider, 0.3, 0);
	snis_slider_poke_input(eng_ui.warp_slider, 0.0, 0);
	snis_slider_poke_input(eng_ui.warp_coolant_slider, 0.3, 0);
	snis_slider_poke_input(eng_ui.impulse_slider, 0.0, 0);
	snis_slider_poke_input(eng_ui.impulse_coolant_slider, 0.3, 0);
	snis_slider_poke_input(eng_ui.sensors_slider, 0.0, 0);
	snis_slider_poke_input(eng_ui.sensors_coolant_slider, 0.3, 0);
	snis_slider_poke_input(eng_ui.comm_slider, 0.0, 0);
	snis_slider_poke_input(eng_ui.comm_coolant_slider, 0.3, 0);
	snis_slider_poke_input(eng_ui.phaserbanks_slider, 0.0, 0);
	snis_slider_poke_input(eng_ui.phaserbanks_coolant_slider, 0.3, 0);
	snis_slider_poke_input(eng_ui.tractor_slider, 0.0, 0);
	snis_slider_poke_input(eng_ui.tractor_coolant_slider, 0.3, 0);
	snis_slider_poke_input(eng_ui.shield_control_slider, 0.0, 0);
}

static void init_engineering_ui(void)
{
	int y;
	int r = 59;
	int x = r * 1.1;
	int xinc = (2.0 * r) * 1.1;
	int yinc = 38; 
	int dm = DISPLAYMODE_ENGINEERING;
	int color = AMBER;
	const int ccolor = COLOR_LIGHTER(BLUE, 25); /* coolant color */
	const int tcolor = AMBER; /* temperature color */
	const int coolant_inc = 19;
	const int sh = 12; /* slider height */
	const int powersliderlen = 180; 
	const int coolantsliderlen = 150;
	struct engineering_ui *eu = &eng_ui;

	eu->selected_subsystem = -1;
	y = 140;
	eu->amp_gauge = gauge_init(x, y, r, 0.0, 100.0, -120.0 * M_PI / 180.0,
			120.0 * 2.0 * M_PI / 180.0, RED, color,
			10, "AMPS", sample_power_model_current);
	x += xinc;
	eu->voltage_gauge = gauge_init(x, y, r, 0.0, 200.0, -120.0 * M_PI / 180.0,
			120.0 * 2.0 * M_PI / 180.0, RED, color,
			10, "VOLTS", sample_power_model_voltage);
	x += xinc;
	eu->temp_gauge = gauge_init(x, y, r, 0.0, 100.0, -120.0 * M_PI / 180.0,
			120.0 * 2.0 * M_PI / 180.0, RED, color,
			10, "TEMP", sample_temp);
	x += xinc;
	eu->fuel_gauge = gauge_init(x, y, r, 0.0, 100.0, -120.0 * M_PI / 180.0,
			120.0 * 2.0 * M_PI / 180.0, RED, color,
			10, "FUEL", sample_fuel);

	eu->shield_control_slider = snis_slider_init(540, 270, 160, sh, AMBER, "SHIELDS",
				"0", "100", 0.0, 255.0, sample_power_data_shields_current,
				do_shieldadj);
	/* make shield slider have less fuzz just for variety */
	snis_slider_set_fuzz(eu->shield_control_slider, 1);

	y = 220;
	eu->damcon_button = snis_button_init(20, y + 30, 160, 25, "DAMAGE CONTROL", color,
			NANO_FONT, damcon_button_pressed, (void *) 0);
	eu->preset1_button = snis_button_init(200, y + 30, 25, 25, "1", color,
			NANO_FONT, preset1_button_pressed, (void *) 0);
	eu->preset2_button = snis_button_init(240, y + 30, 25, 25, "2", color,
			NANO_FONT, preset2_button_pressed, (void *) 0);
	y += yinc;
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
	ui_add_slider(eu->shield_slider, dm);
	ui_add_slider(eu->shield_coolant_slider, dm);
	ui_add_slider(eu->phaserbanks_slider, dm);
	ui_add_slider(eu->phaserbanks_coolant_slider, dm);
	ui_add_slider(eu->comm_slider, dm);
	ui_add_slider(eu->comm_coolant_slider, dm);
	ui_add_slider(eu->sensors_slider, dm);
	ui_add_slider(eu->sensors_coolant_slider, dm);
	ui_add_slider(eu->impulse_slider, dm);
	ui_add_slider(eu->impulse_coolant_slider, dm);
	ui_add_slider(eu->warp_slider, dm);
	ui_add_slider(eu->warp_coolant_slider, dm);
	ui_add_slider(eu->maneuvering_slider, dm);
	ui_add_slider(eu->maneuvering_coolant_slider, dm);
	ui_add_slider(eu->tractor_slider, dm);
	ui_add_slider(eu->tractor_coolant_slider, dm);
	ui_add_slider(eu->shield_control_slider, dm);
	ui_add_gauge(eu->amp_gauge, dm);
	ui_add_gauge(eu->voltage_gauge, dm);
	ui_add_gauge(eu->fuel_gauge, dm);
	ui_add_gauge(eu->temp_gauge, dm);
	ui_add_button(eu->damcon_button, dm);
	ui_add_button(eu->preset1_button, dm);
	ui_add_button(eu->preset2_button, dm);

	y = 220 + yinc;
	eu->shield_damage = snis_slider_init(350, y += yinc, 150, sh, color, "SHIELD STATUS", "0", "100",
				0.0, 100.0, sample_shield_damage, NULL);
	snis_slider_set_label_font(eu->shield_damage, NANO_FONT);
	eu->shield_temperature = snis_slider_init(350, y + coolant_inc, 150, sh, tcolor,
				"TEMPERATURE", "0", "100", 0.0, 100.0,
				sample_shield_temperature, NULL);
	snis_slider_set_label_font(eu->shield_temperature, NANO_FONT);
	snis_slider_set_color_scheme(eu->shield_temperature, 1);	
	eu->phaser_banks_damage = snis_slider_init(350, y += yinc, 150, sh, color, "PHASER STATUS", "0", "100",
				0.0, 100.0, sample_phaser_banks_damage, NULL);
	snis_slider_set_label_font(eu->phaser_banks_damage, NANO_FONT);
	eu->phaser_banks_temperature = snis_slider_init(350, y + coolant_inc, 150, sh, tcolor,
				"TEMPERATURE", "0", "100", 0.0, 100.0,
				sample_phaser_banks_temperature, NULL);
	snis_slider_set_label_font(eu->phaser_banks_temperature, NANO_FONT);
	snis_slider_set_color_scheme(eu->phaser_banks_temperature, 1);	
	eu->comms_damage = snis_slider_init(350, y += yinc, 150, sh, color, "COMMS STATUS", "0", "100",
				0.0, 100.0, sample_comms_damage, NULL);
	snis_slider_set_label_font(eu->comms_damage, NANO_FONT);
	eu->comms_temperature = snis_slider_init(350, y + coolant_inc, 150, sh, tcolor,
				"TEMPERATURE", "0", "100", 0.0, 100.0,
				sample_comms_temperature, NULL);
	snis_slider_set_label_font(eu->comms_temperature, NANO_FONT);
	snis_slider_set_color_scheme(eu->comms_temperature, 1);	
	eu->sensors_damage = snis_slider_init(350, y += yinc, 150, sh, color, "SENSORS STATUS", "0", "100",
				0.0, 100.0, sample_sensors_damage, NULL);
	snis_slider_set_label_font(eu->sensors_damage, NANO_FONT);
	eu->sensors_temperature = snis_slider_init(350, y + coolant_inc, 150, sh, tcolor,
				"TEMPERATURE", "0", "100", 0.0, 100.0,
				sample_sensors_temperature, NULL);
	snis_slider_set_label_font(eu->sensors_temperature, NANO_FONT);
	snis_slider_set_color_scheme(eu->sensors_temperature, 1);	
	eu->impulse_damage = snis_slider_init(350, y += yinc, 150, sh, color, "IMPULSE STATUS", "0", "100",
				0.0, 100.0, sample_impulse_damage, NULL);
	snis_slider_set_label_font(eu->impulse_damage, NANO_FONT);
	eu->impulse_temperature = snis_slider_init(350, y + coolant_inc, 150, sh, tcolor,
				"TEMPERATURE", "0", "100", 0.0, 100.0,
				sample_impulse_temperature, NULL);
	snis_slider_set_label_font(eu->impulse_temperature, NANO_FONT);
	snis_slider_set_color_scheme(eu->impulse_temperature, 1);	
	eu->warp_damage = snis_slider_init(350, y += yinc, 150, sh, color, "WARP STATUS", "0", "100",
				0.0, 100.0, sample_warp_damage, NULL);
	snis_slider_set_label_font(eu->warp_damage, NANO_FONT);
	eu->warp_temperature = snis_slider_init(350, y + coolant_inc, 150, sh, tcolor,
				"TEMPERATURE", "0", "100", 0.0, 100.0,
				sample_warp_temperature, NULL);
	snis_slider_set_label_font(eu->warp_temperature, NANO_FONT);
	snis_slider_set_color_scheme(eu->warp_temperature, 1);	
	eu->maneuvering_damage = snis_slider_init(350, y += yinc, 150, sh, color, "MANEUVERING STATUS", "0", "100",
				0.0, 100.0, sample_maneuvering_damage, NULL);
	snis_slider_set_label_font(eu->maneuvering_damage, NANO_FONT);
	eu->maneuvering_temperature = snis_slider_init(350, y + coolant_inc, 150, sh, tcolor,
				"TEMPERATURE", "0", "100", 0.0, 100.0,
				sample_maneuvering_temperature, NULL);
	snis_slider_set_label_font(eu->maneuvering_temperature, NANO_FONT);
	snis_slider_set_color_scheme(eu->maneuvering_temperature, 1);	
	eu->tractor_damage = snis_slider_init(350, y += yinc, 150, sh, color, "TRACTOR STATUS", "0", "100",
				0.0, 100.0, sample_tractor_damage, NULL);
	snis_slider_set_label_font(eu->tractor_damage, NANO_FONT);
	eu->tractor_temperature = snis_slider_init(350, y + coolant_inc, 150, sh, tcolor,
				"TEMPERATURE", "0", "100", 0.0, 100.0,
				sample_tractor_temperature, NULL);
	snis_slider_set_label_font(eu->tractor_temperature, NANO_FONT);
	snis_slider_set_color_scheme(eu->tractor_temperature, 1);	
	ui_add_slider(eu->shield_damage, dm);
	ui_add_slider(eu->impulse_damage, dm);
	ui_add_slider(eu->warp_damage, dm);
	ui_add_slider(eu->maneuvering_damage, dm);
	ui_add_slider(eu->phaser_banks_damage, dm);
	ui_add_slider(eu->sensors_damage, dm);
	ui_add_slider(eu->comms_damage, dm);
	ui_add_slider(eu->tractor_damage, dm);
	ui_add_slider(eu->shield_temperature, dm);
	ui_add_slider(eu->impulse_temperature, dm);
	ui_add_slider(eu->warp_temperature, dm);
	ui_add_slider(eu->maneuvering_temperature, dm);
	ui_add_slider(eu->phaser_banks_temperature, dm);
	ui_add_slider(eu->sensors_temperature, dm);
	ui_add_slider(eu->comms_temperature, dm);
	ui_add_slider(eu->tractor_temperature, dm);
}

static void show_engineering_damage_report(GtkWidget *w, int subsystem)
{
	struct snis_damcon_entity *o;
	int i, x, y, count;
	char msg[50];

	/* in different order on screen... barf. */
	const int sysmap[] = { 0, 4, 5, 6, 1, 3, 2, 7 };

	if (subsystem < 0 || subsystem >= ARRAYSIZE(sysmap))
		return;

	y = 200 + sysmap[subsystem] * 40;
	x = 300;

	sng_set_foreground(BLACK);
	snis_draw_rectangle(1, x - 5, y - 5, 440, 65);
	sng_set_foreground(AMBER);
	snis_draw_rectangle(0, x - 5, y - 5, 440, 65);
	count = 0;
	for (i = 0; i <= snis_object_pool_highest_object(damcon_pool); i++) {
		o = &dco[i];
		if (o->type != DAMCON_TYPE_PART)
			continue;
		if (o->tsd.part.system != subsystem)
			continue;
		if ((float) o->tsd.part.damage > 0.75f * 255.0f)
			sng_set_foreground(ORANGERED);
		else if ((float) o->tsd.part.damage > 0.5f * 255.0f)
			sng_set_foreground(YELLOW);
		else
			sng_set_foreground(GREEN);
		sprintf(msg, "%3.2f%%: %s",
			(1.0f - (float) o->tsd.part.damage / 255.0f) * 100.0f,
			damcon_part_name(o->tsd.part.system, o->tsd.part.part));
		sng_abs_xy_draw_string(msg, NANO_FONT, x + 10, y + 10);
		y = y + 20;
		count++;
		if (count >= DAMCON_PARTS_PER_SYSTEM)
			break;
	}
}

static void show_engineering(GtkWidget *w)
{
	struct snis_entity *o;
	int gx1, gy1, gx2, gy2;

	if (!(o = find_my_ship()))
		return;

	snis_slider_set_input(eng_ui.shield_slider, o->tsd.ship.power_data.shields.r2/255.0 );
	snis_slider_set_input(eng_ui.phaserbanks_slider, o->tsd.ship.power_data.phasers.r2/255.0 );
	snis_slider_set_input(eng_ui.comm_slider, o->tsd.ship.power_data.comms.r2/255.0 );
	snis_slider_set_input(eng_ui.sensors_slider, o->tsd.ship.power_data.sensors.r2/255.0 );
	snis_slider_set_input(eng_ui.impulse_slider, o->tsd.ship.power_data.impulse.r2/255.0 );
	snis_slider_set_input(eng_ui.warp_slider, o->tsd.ship.power_data.warp.r2/255.0 );
	snis_slider_set_input(eng_ui.maneuvering_slider, o->tsd.ship.power_data.maneuvering.r2/255.0 );
	snis_slider_set_input(eng_ui.tractor_slider, o->tsd.ship.power_data.tractor.r2/255.0 );
	snis_slider_set_input(eng_ui.shield_control_slider, o->tsd.ship.power_data.shields.r1/255.0 );

	snis_slider_set_input(eng_ui.shield_coolant_slider, o->tsd.ship.coolant_data.shields.r2/255.0 );
	snis_slider_set_input(eng_ui.phaserbanks_coolant_slider, o->tsd.ship.coolant_data.phasers.r2/255.0 );
	snis_slider_set_input(eng_ui.comm_coolant_slider, o->tsd.ship.coolant_data.comms.r2/255.0 );
	snis_slider_set_input(eng_ui.sensors_coolant_slider, o->tsd.ship.coolant_data.sensors.r2/255.0 );
	snis_slider_set_input(eng_ui.impulse_coolant_slider, o->tsd.ship.coolant_data.impulse.r2/255.0 );
	snis_slider_set_input(eng_ui.warp_coolant_slider, o->tsd.ship.coolant_data.warp.r2/255.0 );
	snis_slider_set_input(eng_ui.maneuvering_coolant_slider, o->tsd.ship.coolant_data.maneuvering.r2/255.0 );
	snis_slider_set_input(eng_ui.tractor_coolant_slider, o->tsd.ship.coolant_data.tractor.r2/255.0 );

	/* idiot lights for low power of various systems */
	const int low_power_threshold = 10;

	float sc_x, sc_y, sc_length, sc_height;
	snis_slider_get_location(eng_ui.shield_control_slider, &sc_x, &sc_y, &sc_length, &sc_height);

	float sc_x_center = sc_x + sc_length / 2.0;
	float sc_y_center = sc_y + sc_height / 2.0;


	sng_set_foreground(RED);
	if (o->tsd.ship.power_data.shields.r2 < low_power_threshold) {
		sng_center_xy_draw_string("LOW SHIELD POWER", NANO_FONT, sc_x_center, sc_y_center);
	}
	else if (o->tsd.ship.power_data.shields.i < low_power_threshold) {
		sng_center_xy_draw_string("SHIELDS ARE OFF", NANO_FONT, sc_x_center, sc_y_center);
	}

	if (o->tsd.ship.fuel < UINT32_MAX * 0.1) { /* 10% */
		float fg_x, fg_y, fg_r;
		gauge_get_location(eng_ui.fuel_gauge, &fg_x, &fg_y, &fg_r);

		float fg_x_center = fg_x;
		float fg_y_center = fg_y - fg_r * 1.25;

		sng_set_foreground(RED);
		if (o->tsd.ship.fuel < UINT32_MAX * 0.01) { /* 1% */
			sng_center_xy_draw_string("OUT Of FUEL", NANO_FONT, fg_x_center, fg_y_center);
		} else {
			sng_center_xy_draw_string("LOW FUEL", NANO_FONT, fg_x_center, fg_y_center);
		}
	}

	gx1 = NAV_DATA_X + 10;
	gy1 = 15;
	gx2 = NAV_DATA_X + NAV_DATA_W - 10;
	gy2 = NAV_DATA_Y + NAV_DATA_H - 80;
	sng_set_foreground(AMBER);
	draw_science_graph(w, o, o, gx1, gy1, gx2, gy2);

	sng_set_foreground(RED);
	if (o->sdata.shield_strength < 15) {
		sng_center_xy_draw_string("SHIELDS ARE DOWN", TINY_FONT, (gx1 + gx2) / 2.0, gy1 + (gy2 - gy1) / 3.0);
	}

	show_common_screen(w, "ENGINEERING");
}

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

	sng_set_foreground(RED);
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

#if 0
	if (!on_damcon_screen(o, &damcon_robot_spun[byteangle]))
		return;
#endif

	x = o->x + damconscreenx0 + damconscreenxdim / 2.0 - *damconscreenx;
	y = o->y + damconscreeny0 + damconscreenydim / 2.0 - *damconscreeny;
	sng_set_foreground(GREEN);
	sng_draw_vect_obj(&damcon_robot_spun[byteangle], x, y);
}

static void draw_damcon_system(GtkWidget *w, struct snis_damcon_entity *o)
{
	int x, y;

	if (!on_damcon_screen(o, &placeholder_system))
		return;
	
	x = damconx_to_screenx(o->x);
	y = damcony_to_screeny(o->y);
	sng_set_foreground(WHITE);
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
	sprintf(msg, "%d %d", o->tsd.socket.system, o->tsd.socket.part);
	sng_set_foreground(color);
	sng_draw_vect_obj(&placeholder_socket, x, y);
	sng_abs_xy_draw_string(msg, NANO_FONT, x - 10, y);
#if 0
	sng_set_foreground(AMBER);
	snis_draw_line(x, y - 20, x, y + 20);
	snis_draw_line(x - 20, y, x + 20, y);
#endif
}

static void draw_damcon_socket(GtkWidget *w, struct snis_damcon_entity *o)
{
	if (o->tsd.socket.contents_id == DAMCON_SOCKET_EMPTY)
		draw_damcon_socket_or_part(w, o, WHITE);
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
		sprintf(msg, "%s%s",
			damcon_part_name(o->tsd.part.system, o->tsd.part.part),
			o->tsd.part.damage == 255 ? " (IRREPARABLY DAMAGED)" : "");
	else
		strcpy(msg, "");
	sng_set_foreground(YELLOW);
	sng_draw_vect_obj(&placeholder_part_spun[byteangle], x, y);
	sng_center_xy_draw_string(msg, NANO_FONT, x,
			y - 15 - (o->tsd.part.part % 2) * 15);
	if (o->tsd.part.damage < 0.30 * 255.0)
		sng_set_foreground(GREEN);
	else if (o->tsd.part.damage < 0.75 * 255.0)
		sng_set_foreground(YELLOW);
	else {
		if ((timer & 0x8) == 0) /* make red bar blink */
			return;
		sng_set_foreground(RED);
	}
	snis_draw_rectangle(0, x - 30, y + 10, 60, 6);
	snis_draw_rectangle(1, x - 30, y + 10,
			60.0 * (255 - o->tsd.part.damage) / 255.0, 6);
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
	case DAMCON_TYPE_REPAIR_STATION:
		draw_damcon_system(w, o);
		break;
	case DAMCON_TYPE_SOCKET:
		draw_damcon_socket(w, o);
		break;
	case DAMCON_TYPE_PART:
		draw_damcon_part(w, o);
		break;
	default:
		break;
	}
}

static void show_damcon(GtkWidget *w)
{
	int i;

	sng_set_foreground(AMBER);
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
	queue_to_server(packed_buffer_new("bb", OPCODE_SCI_DETAILS,
		(unsigned char) SCI_DETAILS_MODE_DETAILS));
}

static void sci_threed_pressed(void *x)
{
	queue_to_server(packed_buffer_new("bb", OPCODE_SCI_DETAILS,
		(unsigned char) SCI_DETAILS_MODE_THREED));
}

static void sci_sciplane_pressed(void *x)
{
	queue_to_server(packed_buffer_new("bb", OPCODE_SCI_DETAILS,
		(unsigned char) SCI_DETAILS_MODE_SCIPLANE));
}

static void sci_tractor_pressed(void *x)
{
	uint32_t id = curr_science_guy ? curr_science_guy->id : (uint32_t) 0xffffffff;

	queue_to_server(packed_buffer_new("bw", OPCODE_REQUEST_TRACTORBEAM, id));
}

static void init_science_ui(void)
{
	sci_ui.scizoom = snis_slider_init(350, 35, 300, 12, DARKGREEN, "RANGE", "0", "100",
				0.0, 100.0, sample_scizoom, do_scizoom);
	snis_slider_set_label_font(sci_ui.scizoom, NANO_FONT);
	sci_ui.scipower = snis_slider_init(350, 50, 300, 12, DARKGREEN, "POWER", "0", "100",
				0.0, 100.0, sample_sensors_power, NULL);
	snis_slider_set_fuzz(sci_ui.scipower, 7);
	snis_slider_set_label_font(sci_ui.scipower, NANO_FONT);
	sci_ui.tractor_button = snis_button_init(530, 575, 85, 20, "TRACTOR",
			GREEN, NANO_FONT, sci_tractor_pressed, (void *) 0);
	sci_ui.sciplane_button = snis_button_init(620, 575, 40, 20, "SRS",
			GREEN, NANO_FONT, sci_sciplane_pressed, (void *) 0);
	sci_ui.threed_button = snis_button_init(665, 575, 40, 20, "LRS",
			GREEN, NANO_FONT, sci_threed_pressed, (void *) 0);
	sci_ui.details_button = snis_button_init(710, 575, 75, 20, "DETAILS",
			GREEN, NANO_FONT, sci_details_pressed, (void *) 0);
	ui_add_slider(sci_ui.scizoom, DISPLAYMODE_SCIENCE);
	ui_add_slider(sci_ui.scipower, DISPLAYMODE_SCIENCE);
	ui_add_button(sci_ui.details_button, DISPLAYMODE_SCIENCE);
	ui_add_button(sci_ui.tractor_button, DISPLAYMODE_SCIENCE);
	ui_add_button(sci_ui.threed_button, DISPLAYMODE_SCIENCE);
	ui_add_button(sci_ui.sciplane_button, DISPLAYMODE_SCIENCE);
	sciecx = entity_context_new(50, 10);
	sciballecx = entity_context_new(5000, 1000);
	sciplane_tween = tween_init(500);
	sci_ui.details_mode = SCI_DETAILS_MODE_SCIPLANE;
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

static void comms_screen_red_alert_pressed(void *x)
{
	unsigned char new_alert_mode;

	new_alert_mode = (red_alert_mode == 0);	
	queue_to_server(packed_buffer_new("bb", OPCODE_REQUEST_REDALERT, new_alert_mode));
}

static void comms_main_screen_pressed(void *x)
{
	unsigned char new_comms_mode;

	new_comms_mode = (main_screen_text.comms_on_mainscreen == 0);	
	queue_to_server(packed_buffer_new("bb", OPCODE_COMMS_MAINSCREEN, new_comms_mode));
}

static void send_comms_packet_to_server(char *msg, uint8_t opcode, uint32_t id)
{
	struct packed_buffer *pb;
	uint8_t len = strlen(msg);

	pb = packed_buffer_allocate(sizeof(struct comms_transmission_packet) + len);
	packed_buffer_append(pb, "bbw", opcode, len, id);
	packed_buffer_append_raw(pb, msg, (unsigned short) len);
	packed_buffer_queue_add(&to_server_queue, pb, &to_server_queue_mutex);
	wakeup_gameserver_writer();
}

static void send_lua_script_packet_to_server(char *script)
{
	struct packed_buffer *pb;
	uint8_t len = strlen(script);

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

static void comms_input_entered()
{
	printf("comms input entered\n");
}

static void init_comms_ui(void)
{
	int x = 200;
	int y = 20;

	comms_ui.comms_onscreen_button = snis_button_init(x, y, 75, 25, "COMMS", GREEN,
			NANO_FONT, comms_screen_button_pressed, (void *) 0);
	x += 75;
	comms_ui.nav_onscreen_button = snis_button_init(x, y, 75, 25, "NAV", GREEN,
			NANO_FONT, comms_screen_button_pressed, (void *) 1);
	x += 75;
	comms_ui.weap_onscreen_button = snis_button_init(x, y, 75, 25, "WEAP", GREEN,
			NANO_FONT, comms_screen_button_pressed, (void *) 2);
	x += 75;
	comms_ui.eng_onscreen_button = snis_button_init(x, y, 75, 25, "ENG", GREEN,
			NANO_FONT, comms_screen_button_pressed, (void *) 3);
	x += 75;
	comms_ui.damcon_onscreen_button = snis_button_init(x, y, 75, 25, "DAMCON", GREEN,
			NANO_FONT, comms_screen_button_pressed, (void *) 4);
	x += 75;
	comms_ui.sci_onscreen_button = snis_button_init(x, y, 75, 25, "SCI", GREEN,
			NANO_FONT, comms_screen_button_pressed, (void *) 5);
	x += 75;
	comms_ui.main_onscreen_button = snis_button_init(x, y, 75, 25, "MAIN", GREEN,
			NANO_FONT, comms_screen_button_pressed, (void *) 7);
	x = SCREEN_WIDTH - 150;
	y = SCREEN_HEIGHT - 90;
	comms_ui.red_alert_button = snis_button_init(x, y, 120, 25, "RED ALERT", RED,
			NANO_FONT, comms_screen_red_alert_pressed, (void *) 6);
	y = SCREEN_HEIGHT - 60;
	comms_ui.mainscreen_comms = snis_button_init(x, y, 120, 25, "MAIN SCREEN", GREEN,
			NANO_FONT, comms_main_screen_pressed, (void *) 8);
	comms_ui.tw = text_window_init(10, 70, SCREEN_WIDTH - 20, 40, 20, GREEN);
	comms_ui.comms_input = snis_text_input_box_init(10, 520, 30, 550, GREEN, TINY_FONT,
					comms_ui.input, 50, &timer,
					comms_input_entered, NULL);
	snis_text_input_box_set_return(comms_ui.comms_input,
					comms_transmit_button_pressed);
	comms_ui.comms_transmit_button = snis_button_init(10, 550, 160, 30, "TRANSMIT", GREEN,
			TINY_FONT, comms_transmit_button_pressed, NULL);
	comms_ui.mainzoom_slider = snis_slider_init(180, 560, 380, 15, GREEN, "ZOOM",
				"1", "10", 0.0, 100.0, sample_mainzoom,
				do_mainzoom);
	ui_add_text_window(comms_ui.tw, DISPLAYMODE_COMMS);
	ui_add_button(comms_ui.comms_onscreen_button, DISPLAYMODE_COMMS);
	ui_add_button(comms_ui.nav_onscreen_button, DISPLAYMODE_COMMS);
	ui_add_button(comms_ui.weap_onscreen_button, DISPLAYMODE_COMMS);
	ui_add_button(comms_ui.eng_onscreen_button, DISPLAYMODE_COMMS);
	ui_add_button(comms_ui.damcon_onscreen_button, DISPLAYMODE_COMMS);
	ui_add_button(comms_ui.sci_onscreen_button, DISPLAYMODE_COMMS);
	ui_add_button(comms_ui.main_onscreen_button, DISPLAYMODE_COMMS);
	ui_add_button(comms_ui.red_alert_button, DISPLAYMODE_COMMS);
	ui_add_button(comms_ui.mainscreen_comms, DISPLAYMODE_COMMS);
	ui_add_button(comms_ui.comms_transmit_button, DISPLAYMODE_COMMS);
	ui_add_text_input_box(comms_ui.comms_input, DISPLAYMODE_COMMS);
	ui_add_slider(comms_ui.mainzoom_slider, DISPLAYMODE_COMMS);
}

#define SCIDIST2 100
static int science_button_press(int x, int y)
{
	int i;
	int xdist, ydist, dist2;
	struct snis_entity *selected;
	struct snis_entity *o;
	double ur, ux, uz;
	int cx, cy, r;
	double dx, dy;

	selected = NULL;
	pthread_mutex_lock(&universe_mutex);
	for (i = 0; i < nscience_guys; i++) {
		xdist = (x - science_guy[i].sx);
		ydist = (y - science_guy[i].sy);
		dist2 = xdist * xdist + ydist * ydist; 
		if (dist2 < SCIDIST2) {
			if (curr_science_guy == science_guy[i].o || science_guy[i].o->sdata.science_data_known) {
				selected = science_guy[i].o;
			}
		}
	}
	if (selected) {
		if (curr_science_guy != selected)
			request_sci_select_target(selected->id);
		else
			request_sci_select_target((uint32_t) -1); /* deselect */
	} else {
		o = &go[my_ship_oid];
		cx = SCIENCE_SCOPE_CX;
		cy = SCIENCE_SCOPE_CY;
		r = SCIENCE_SCOPE_R;
		ur = (MAX_SCIENCE_SCREEN_RADIUS - MIN_SCIENCE_SCREEN_RADIUS) *
				(o->tsd.ship.scizoom / 255.0) +
				MIN_SCIENCE_SCREEN_RADIUS;
		dx = x - cx;
		dy = y - cy;
		ux = o->x + dx * ur / r;
		uz = o->z + dy * ur / r;
		request_sci_select_coords(ux, uz);
	}
	pthread_mutex_unlock(&universe_mutex);

	return 0;
}

#define SCIENCE_DATA_X (SCIENCE_SCOPE_X + SCIENCE_SCOPE_W + 20)
#define SCIENCE_DATA_Y (SCIENCE_SCOPE_Y + 0)
#define SCIENCE_DATA_W (SCREEN_WIDTH - 20 - SCIENCE_DATA_X)
#define SCIENCE_DATA_H (SCREEN_HEIGHT - 40 - SCIENCE_DATA_Y)

static void draw_science_graph(GtkWidget *w, struct snis_entity *ship, struct snis_entity *o,
		int x1, int y1, int x2, int y2)
{
	int i, x;
	double sx, sy, sy1, sy2, dist;
	int dy1, dy2, bw, probes, dx, pwr;
	int initial_noise;

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
			dist = hypot(o->x - go[my_ship_oid].x, o->y - go[my_ship_oid].y);
			bw = (int) (go[my_ship_oid].tsd.ship.sci_beam_width * 180.0 / M_PI);
			pwr = ship->tsd.ship.power_data.sensors.i;
		} else {
			dist = 0.1;
			bw = 5.0;
			pwr = 255; /* never any trouble scanning our own ship */
		}

		sng_set_foreground(LIMEGREEN);
		if (o->sdata.shield_strength < 64) {
			sng_set_foreground(RED);
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
	sng_set_foreground(GREEN);
	sng_abs_xy_draw_string("10", NANO_FONT, x1, y2 + 10);
	sng_abs_xy_draw_string("20", NANO_FONT, x1 + (x2 - x1) / 4 - 10, y2 + 10);
	sng_abs_xy_draw_string("30", NANO_FONT, x1 + 2 * (x2 - x1) / 4 - 10, y2 + 10);
	sng_abs_xy_draw_string("40", NANO_FONT, x1 + 3 * (x2 - x1) / 4 - 10, y2 + 10);
	sng_abs_xy_draw_string("50", NANO_FONT, x1 + 4 * (x2 - x1) / 4 - 20, y2 + 10);
	sng_abs_xy_draw_string("Shield Profile (nm)", NANO_FONT, x1 + (x2 - x1) / 4 - 10, y2 + 30);
}

static void draw_science_data(GtkWidget *w, struct snis_entity *ship, struct snis_entity *o)
{
	char buffer[40];
	char buffer2[40];
	int x, y, gx1, gy1, gx2, gy2;
	double dx, dy, dz, range;
	char *the_faction;

	if (!ship)
		return;
	x = SCIENCE_DATA_X + 10;
	y = SCIENCE_DATA_Y + 15;
	snis_draw_rectangle(0, SCIENCE_DATA_X, SCIENCE_DATA_Y,
					SCIENCE_DATA_W, SCIENCE_DATA_H);
	sprintf(buffer, "NAME: %s", o ? o->sdata.name : "");
	sng_abs_xy_draw_string(buffer, TINY_FONT, x, y);
	if (o && (o->type == OBJTYPE_SHIP1 ||
		o->type == OBJTYPE_SHIP2 ||
		o->type == OBJTYPE_STARBASE)) {
		y += 25;
		the_faction = o ? 
			o->sdata.faction >= 0 &&
			o->sdata.faction < nfactions() ?
				faction_name(o->sdata.faction) : "UNKNOWN" : "UNKNOWN";
		sprintf(buffer, "ORIG: %s", the_faction);
		sng_abs_xy_draw_string(buffer, TINY_FONT, x, y);
	}

	if (o) {
		switch (o->type) {
		case OBJTYPE_SHIP2:
			sprintf(buffer, "TYPE: %s", ship_type[o->sdata.subclass].class); 
			break;
		case OBJTYPE_STARBASE:
			sprintf(buffer, "TYPE: %s", "STARBASE"); 
			break;
		case OBJTYPE_ASTEROID:
			sprintf(buffer, "TYPE: %s", "ASTEROID"); 
			break;
		case OBJTYPE_DERELICT:
			sprintf(buffer, "TYPE: %s", "DERELICT");
			break;
		default:
			sprintf(buffer, "TYPE: %s", "UNKNOWN"); 
			break;
		}
	} else  {
		sprintf(buffer, "TYPE:"); 
	}
	y += 25;
	sng_abs_xy_draw_string(buffer, TINY_FONT, x, y);

	if (o)
		sprintf(buffer, "X: %0.2lf", o->x);
	else
		sprintf(buffer, "X:");
	y += 25;
	sng_abs_xy_draw_string(buffer, TINY_FONT, x, y);

	if (o)
		sprintf(buffer, "Y: %0.2lf", o->y);
	else
		sprintf(buffer, "Y:");
	y += 25;
	sng_abs_xy_draw_string(buffer, TINY_FONT, x, y);

	if (o)
		sprintf(buffer, "Z: %0.2lf", o->z);
	else
		sprintf(buffer, "Z:");
	y += 25;
	sng_abs_xy_draw_string(buffer, TINY_FONT, x, y);

	if (o) { 
		dx = o->x - go[my_ship_oid].x;
		dy = o->y - go[my_ship_oid].y;
		dz = o->z - go[my_ship_oid].z;

		union quat q_to_o;
		union vec3 dir_forward = {{1, 0, 0}};
		union vec3 dir_to_o = {{dx, dy, dz}};
		quat_from_u2v(&q_to_o, &dir_forward, &dir_to_o, 0);

		double bearing=0, mark=0;
		to_snis_heading_mark(&q_to_o, &bearing, &mark);

		sprintf(buffer,  "BEARING: %0.1lf", radians_to_degrees(bearing));
		sprintf(buffer2, "MARK: %0.1lf", radians_to_degrees(mark));
	} else {
		sprintf(buffer,  "BEARING:");
		sprintf(buffer2, "MARK:");
	}
	y += 25;
	sng_abs_xy_draw_string(buffer, TINY_FONT, x, y);
	y += 25;
	sng_abs_xy_draw_string(buffer2, TINY_FONT, x, y);

	if (o) {
		range = dist3d(dx, dy, dz);
		sprintf(buffer, "RANGE: %8.2lf", range);
	} else {
		sprintf(buffer, "RANGE:");
	}
	y += 25;
	sng_abs_xy_draw_string(buffer, TINY_FONT, x, y);

	if (o) {
		sprintf(buffer, "WARP FACTOR: %2.2lf", 10.0 * range / (XKNOWN_DIM / 2.0));
	} else {
		sprintf(buffer, "WARP FACTOR:");
	}
	y += 25;
	sng_abs_xy_draw_string(buffer, TINY_FONT, x, y);
#if 0
	sprintf(buffer, "STRENGTH: %hhu", o->sdata.shield_strength);
	y += 25;
	sng_abs_xy_draw_string(w, gd, buffer, TINY_FONT, x, y);
	sprintf(buffer, "WAVELENGTH: %hhu", o->sdata.shield_wavelength);
	y += 25;
	sng_abs_xy_draw_string(buffer, TINY_FONT, x, y);
	sprintf(buffer, "WIDTH: %hhu", o->sdata.shield_width);
	y += 25;
	sng_abs_xy_draw_string(buffer, TINY_FONT, x, y);
#endif

	gx1 = x;
	gy1 = y + 20;
	gx2 = SCIENCE_DATA_X + SCIENCE_DATA_W - 10;
	gy2 = SCIENCE_DATA_Y + SCIENCE_DATA_H - 40;
	draw_science_graph(w, ship, o, gx1, gy1, gx2, gy2);
}

static void draw_science_details(GtkWidget *w, GdkGC *gc)
{
	struct entity *e = NULL;
	struct mesh *m;
	char buf[100];
	float angle;
	union quat orientation;
	int y;

	if (!curr_science_guy || !curr_science_guy->entity)
		return;

	set_renderer(sciecx, WIREFRAME_RENDERER | BLACK_TRIS);
	m = entity_get_mesh(curr_science_guy->entity);
	e = add_entity(sciecx, m, 0, 0, 0, GREEN);
	angle = (M_PI / 180.0) * (timer % 360);
	quat_init_axis(&orientation, 0.0, 1.0, 0.0, angle);
	if (e)
		update_entity_orientation(e, &orientation);
	camera_set_pos(sciecx, -m->radius * 4, 20, 0);
	camera_look_at(sciecx, (float) 0, (float) 0, (float) m->radius / 2.0);
	camera_set_parameters(sciecx, 0.5, 8000.0,
				SCREEN_WIDTH, SCREEN_HEIGHT, ANGLE_OF_VIEW * M_PI / 180.0);
	set_lighting(sciecx, -m->radius * 4, 0, m->radius);
	render_entities(sciecx);
	if (e)
		remove_entity(sciecx, e);

	y = SCREEN_HEIGHT - 200;
	if (curr_science_guy->type == OBJTYPE_SHIP1 ||
		curr_science_guy->type == OBJTYPE_SHIP2) {
		sprintf(buf, "LIFEFORMS: %d", curr_science_guy->tsd.ship.lifeform_count);
	} else {
		if (curr_science_guy->type == OBJTYPE_STARBASE) {
			sprintf(buf, "LIFEFORMS: %d", curr_science_guy->tsd.starbase.lifeform_count);
		} else {
			buf[0] = '\0';
		}
	}
	sng_set_foreground(GREEN);
	if (buf[0])
		sng_abs_xy_draw_string(buf, TINY_FONT, 10, y);
	y += 20;
	if (curr_science_guy->type == OBJTYPE_PLANET) {
		static uint32_t last = 0xffffffff;
		struct mtwist_state *mt;
		static char planet_desc[500];
		char tmpbuf[60];
		int i, len, j;

		struct planet_data *p = &curr_science_guy->tsd.planet;

		if (p->description_seed != last) {
			mt = mtwist_init(p->description_seed);
			planet_description(mt, planet_desc, 500, 40);
			last = p->description_seed;
			mtwist_free(mt);
			for (i = 0; planet_desc[i] != '\0'; i++)
				planet_desc[i] = toupper(planet_desc[i]);
		}
		sprintf(buf, "GOVERNMENT: %s", government_name[p->government]);
		sng_abs_xy_draw_string(buf, TINY_FONT, 10, y);
		y += 20;
		sprintf(buf, "TECH LEVEL: %d", p->tech_level);
		sng_abs_xy_draw_string(buf, TINY_FONT, 10, y);
		y += 20;
		sprintf(buf, "ECONOMY: %s", economy_name[p->economy]);
		sng_abs_xy_draw_string(buf, TINY_FONT, 10, y);
		y += 20;
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
		sng_abs_xy_draw_string(buf, TINY_FONT, 10, y);
		y += 20;

		/* break planet_desc into multiple lines */
		len = strlen(planet_desc);
		j = 0;
		for (i = 0; i < len; i++) {
			if (planet_desc[i] == '\n' || planet_desc[i] == '\0') {
				tmpbuf[j] = '\0';
				sng_abs_xy_draw_string(tmpbuf, NANO_FONT, 10, y);
				y += 15;
				j = 0;
			} else {
				tmpbuf[j] = planet_desc[i];
				j++;
			}
		}
	}
}
 
static void show_science(GtkWidget *w)
{
	struct snis_entity *o;
	char buf[80];
	double zoom;
	static int current_zoom = 0;

	if (!(o = find_my_ship()))
		return;

	snis_slider_set_input(sci_ui.scizoom, o->tsd.ship.scizoom/255.0 );

	current_zoom = newzoom(current_zoom, o->tsd.ship.scizoom);

	if ((timer & 0x3f) == 0)
		wwviaudio_add_sound(SCIENCE_PROBE_SOUND);
	sng_set_foreground(GREEN);
	sprintf(buf, "LOC: (%5.2lf, %5.2lf, %5.2lf)", o->x, o->y, o->z);
	sng_abs_xy_draw_string(buf, TINY_FONT, 200, LINEHEIGHT * 0.5);
	zoom = (MAX_SCIENCE_SCREEN_RADIUS - MIN_SCIENCE_SCREEN_RADIUS) *
			(current_zoom / 255.0) + MIN_SCIENCE_SCREEN_RADIUS;
	sng_set_foreground(DARKGREEN);
	if (sci_ui.details_mode == SCI_DETAILS_MODE_SCIPLANE) {
		draw_sciplane_display(w, o, zoom);
	} else {
		draw_science_details(w, gc);
		draw_science_data(w, o, curr_science_guy);
	}
	show_common_screen(w, "SCIENCE");
}

static void show_3d_science(GtkWidget *w)
{
	int /* rx, ry, rw, rh, */ cx, cy, r;
	struct snis_entity *o;
	char buf[80];
	double zoom;
	static int current_zoom = 0;

	if (!(o = find_my_ship()))
		return;

	snis_slider_set_input(sci_ui.scizoom, o->tsd.ship.scizoom/255.0 );

	current_zoom = newzoom(current_zoom, o->tsd.ship.scizoom);

	if ((timer & 0x3f) == 0)
		wwviaudio_add_sound(SCIENCE_PROBE_SOUND);
	sng_set_foreground(CYAN);
	sprintf(buf, "LOC: (%5.2lf, %5.2lf, %5.2lf)", o->x, o->y, o->z);
	sng_abs_xy_draw_string(buf, TINY_FONT, 200, LINEHEIGHT * 0.5);
	cx = SCIENCE_SCOPE_CX;
	cy = SCIENCE_SCOPE_CY;
	r = SCIENCE_SCOPE_R;
	zoom = (MAX_SCIENCE_SCREEN_RADIUS - MIN_SCIENCE_SCREEN_RADIUS) *
			(current_zoom / 255.0) + MIN_SCIENCE_SCREEN_RADIUS;
	sng_set_foreground(DARKGREEN);
	if (sci_ui.details_mode == SCI_DETAILS_MODE_THREED) {
		sng_set_foreground(DARKRED);
		sng_draw_circle(0, cx, cy, r);
		draw_all_the_3d_science_guys(w, o, zoom * 4.0, current_zoom * 4.0);
	} else {
		draw_science_details(w, gc);
	}
	draw_science_data(w, o, curr_science_guy);
	show_common_screen(w, "SCIENCE");
}


static void show_comms(GtkWidget *w)
{
	struct snis_entity *o;

	if (!(o = find_my_ship()))
		return;

	float shield_ind_x_center = 710;
	float shield_ind_y_center = 495;

	if (o->sdata.shield_strength < 15) {
		sng_set_foreground(RED);
		sng_center_xy_draw_string("SHIELDS ARE DOWN", NANO_FONT, shield_ind_x_center, shield_ind_y_center);
	} else {
		sng_set_foreground(GREEN);
		char buf[80];
		sprintf(buf, "SHIELDS ARE %d%%", (int)(o->sdata.shield_strength / 2.55));
		sng_center_xy_draw_string(buf, NANO_FONT, shield_ind_x_center, shield_ind_y_center);
	}

	show_common_screen(w, "COMMS");
}

static void send_demon_comms_packet_to_server(char *msg)
{
	if (demon_ui.captain_of < 0)
		return;
	printf("demon_ui.captain_of = %d\n", demon_ui.captain_of);
	send_comms_packet_to_server(msg, OPCODE_DEMON_COMMS_XMIT, go[demon_ui.captain_of].id);
}

static void send_demon_clear_all_packet_to_server(void)
{
	queue_to_server(packed_buffer_new("b", OPCODE_DEMON_CLEAR_ALL));
}

static void toggle_demon_ai_debug_mode(void)
{
	queue_to_server(packed_buffer_new("b", OPCODE_TOGGLE_DEMON_AI_DEBUG_MODE));
}

static void request_universe_timestamp(void)
{
	queue_to_server(packed_buffer_new("b", OPCODE_REQUEST_UNIVERSE_TIMESTAMP));
}

static void toggle_demon_safe_mode(void)
{
	queue_to_server(packed_buffer_new("b", OPCODE_TOGGLE_DEMON_SAFE_MODE));
}


static int ux_to_demonsx(double ux)
{
	return ((ux - demon_ui.ux1) / (demon_ui.ux2 - demon_ui.ux1)) * SCREEN_WIDTH;
}

static int uz_to_demonsy(double uz)
{
	return ((uz - demon_ui.uy1) / (demon_ui.uy2 - demon_ui.uy1)) * SCREEN_HEIGHT * ASPECT_RATIO;
}

static int ur_to_demonsr(double ur)
{
	return ((ur * SCREEN_WIDTH) / (demon_ui.ux2 - demon_ui.ux1));
}

static double demon_mousex_to_ux(double x)
{
	return demon_ui.ux1 + (x / real_screen_width) * (demon_ui.ux2 - demon_ui.ux1);
}

static double demon_mousey_to_uz(double y)
{
	return demon_ui.uy1 + (y / real_screen_height) * (demon_ui.uy2 - demon_ui.uy1) / ASPECT_RATIO;
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

	scaledy = (0.9 * real_screen_height - y) / (real_screen_height * 0.8);
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
	if (demon_ui.nselected >= MAX_DEMON_SELECTABLE)
		return;
	demon_ui.selected_id[demon_ui.nselected] = id;
	demon_ui.nselected++;
	if (demon_ui.buttonmode == DEMON_BUTTON_CAPTAINMODE) {
		int index = lookup_object_by_id(id);

		if (demon_ui.captain_of != -1) {
			queue_to_server(packed_buffer_new("bw", OPCODE_DEMON_DISPOSSESS,
				go[demon_ui.captain_of].id));
				demon_ui.captain_of = -1;
		}
		if (index >= 0 && (go[index].type == OBJTYPE_SHIP2 ||
			go[index].type == OBJTYPE_STARBASE)) {
			demon_ui.captain_of = lookup_object_by_id(id);
			queue_to_server(packed_buffer_new("bw", OPCODE_DEMON_POSSESS,
				go[demon_ui.captain_of].id));
		}
	}
}

static void demon_deselect(uint32_t id)
{
	int i;
	for (i = 0; i < demon_ui.nselected; i++) {
		if (demon_ui.selected_id[i] == id) {
			if (demon_ui.captain_of == id && id != -1) {
				queue_to_server(packed_buffer_new("bw", OPCODE_DEMON_DISPOSSESS,
					go[demon_ui.captain_of].id));
				demon_ui.captain_of = -1;
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
	demon_ui.nselected = 0;
}

static void demon_button_press(int button, gdouble x, gdouble y)
{
	/* must be right mouse button so as not to conflict with 'EXECUTE' button. */
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

static inline int between(double a, double b, double v)
{
	return ((a <= v && v <= b) || (b <= v && v <= a));
}

static void demon_button_create_item(gdouble x, gdouble y)
{
	double ux, uz;
	uint8_t item_type;

	ux = demon_mousex_to_ux(x);
	uz = demon_mousey_to_uz(y);

	switch (demon_ui.buttonmode) {
		case DEMON_BUTTON_SHIPMODE:
			item_type = OBJTYPE_SHIP2;
			break;
		case DEMON_BUTTON_STARBASEMODE:
			item_type = OBJTYPE_STARBASE;
			break;
		case DEMON_BUTTON_PLANETMODE:
			item_type = OBJTYPE_PLANET;
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
	queue_to_server(packed_buffer_new("bbSS", OPCODE_CREATE_ITEM, item_type,
			ux, (int32_t) UNIVERSE_DIM, uz, (int32_t) UNIVERSE_DIM));
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
		sx = ux_to_demonsx(o->x);
		sy = uz_to_demonsy(o->z);
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

static void demon_button3_release(int button, gdouble x, gdouble y)
{
	int nselected;
	double ox, oy;
	int sx1, sy1;

	/* If the item creation buttons selected, create item... */ 
	if (demon_ui.buttonmode > DEMON_BUTTON_NOMODE &&
		demon_ui.buttonmode < DEMON_BUTTON_DELETE) {
		demon_button_create_item(x, y);
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
		pthread_mutex_unlock(&universe_mutex);
	}
}

static void demon_button2_release(int button, gdouble x, gdouble y)
{
	int i;
	double dx, dz;

	if (demon_ui.nselected <= 0)
		return;

	/* Moving objects... */
	dx = demon_mousex_to_ux(x) - demon_mousex_to_ux(demon_ui.move_from_x);
	dz = demon_mousey_to_uz(y) - demon_mousey_to_uz(demon_ui.move_from_y);
	
	pthread_mutex_lock(&universe_mutex);
	for (i = 0; i < demon_ui.nselected; i++) {
		queue_to_server(packed_buffer_new("bwSS",
				OPCODE_DEMON_MOVE_OBJECT,
				demon_ui.selected_id[i],
				dx, (int32_t) UNIVERSE_DIM,
				dz, (int32_t) UNIVERSE_DIM));
	}
	pthread_mutex_unlock(&universe_mutex);
}

static void demon_button_release(int button, gdouble x, gdouble y)
{
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

static void debug_draw_ship_patrol_route(uint8_t npoints, union vec3 patrol[])
{
	union vec3 *v1, *v2;
	float x1, y1, x2, y2;

	sng_set_foreground(WHITE);
	for (int i = 1; i <= npoints; i++) {
		v1 = &patrol[i - 1];
		v2 = &patrol[i % npoints];

		x1 = (float) ux_to_demonsx((double) v1->v.x);
		y1 = (float) uz_to_demonsy((double) v1->v.z);
		x2 = (float) ux_to_demonsx((double) v2->v.x);
		y2 = (float) uz_to_demonsy((double) v2->v.z);
		sng_draw_dotted_line(x1, y1, x2, y2);
	}
}

static void debug_draw_object(GtkWidget *w, struct snis_entity *o)
{
	int x, y, x1, y1, x2, y2, vx, vy, r;
	struct snis_entity *v = NULL;
	int xoffset = 7;
	int yoffset = 10;
	int tardy;
	char buffer[20];

	if (!o->alive)
		return;

	tardy = (o->nupdates > 0 && universe_timestamp() - o->updatetime[0] > 50.0 && o->type != OBJTYPE_SPARK);
	x = ux_to_demonsx(o->x);
	if (x < 0 || x > SCREEN_WIDTH)
		return;
	y = uz_to_demonsy(o->z);
	if (y < 0 || y > SCREEN_HEIGHT)
		return;
	x1 = x - 1;
	y2 = y + 1;
	y1 = y - 1;
	x2 = x + 1;

	switch (o->type) {
	case OBJTYPE_SHIP1:
		sng_set_foreground(RED);
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
				vx = ux_to_demonsx(v->x);
				vy = uz_to_demonsy(v->z);
			}
		}
		break;
	case OBJTYPE_ASTEROID:
		sng_set_foreground(ASTEROID_COLOR);
		break;
	case OBJTYPE_DERELICT:
		sng_set_foreground(DERELICT_COLOR);
		break;
	case OBJTYPE_NEBULA:
		sng_set_foreground(NEBULA_COLOR);
		sng_draw_circle(0, x, y,
			ur_to_demonsr(o->tsd.nebula.r));
		break;
	case OBJTYPE_STARBASE:
		sng_set_foreground(STARBASE_COLOR);
		sng_draw_circle(0, x, y, 5);
		break;
	case OBJTYPE_PLANET:
		sng_set_foreground(PLANET_COLOR);
		r = ur_to_demonsr(o->tsd.planet.radius);
		sng_draw_circle(0, x, y, r > 5 ? r : 5);
		break;
	case OBJTYPE_WORMHOLE:
		sng_set_foreground(WORMHOLE_COLOR);
		sng_draw_circle(0, x, y, 5);
		break;
	default:
		sng_set_foreground(WHITE);
	}
	if (tardy)
		sng_set_foreground(timer & 0x04 ? WHITE : BLACK);

	snis_draw_line(x1, y1, x2, y2);
	snis_draw_line(x1, y2, x2, y1);
	if (demon_id_selected(o->id)) {
		if (timer & 0x02) {
			snis_draw_line(x1 - 6, y1 - 6, x2 + 6, y1 - 6);
			snis_draw_line(x1 - 6, y2 + 6, x2 + 6, y2 + 6);
			snis_draw_line(x1 - 6, y1 - 6, x1 - 6, y2 + 6);
			snis_draw_line(x2 + 6, y1 - 6, x2 + 6, y2 + 6);
			if (o->type == OBJTYPE_SHIP1 || o->type == OBJTYPE_SHIP2) {
				snis_draw_arrow(w, gc, x, y, SCIENCE_SCOPE_R, o->heading, 0.4);
			}
		}
		if (o->type == OBJTYPE_SHIP2 && (timer & 0x04))
			debug_draw_ship_patrol_route(o->tsd.ship.ai[1].u.patrol.npoints,
						o->tsd.ship.ai[1].u.patrol.p);
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
			sprintf(buffer, "TL: %.1f", threat_level);
			sng_abs_xy_draw_string(buffer, NANO_FONT, x + xoffset, y + yoffset + 20);
		}
	}
	if (v) {
		sng_set_foreground(RED);
		sng_draw_dotted_line(x, y, vx, vy);
	}

	if ((o->type == OBJTYPE_SHIP2 || o->type == OBJTYPE_STARBASE) &&
			go_index(o) == demon_ui.captain_of) {
		sng_set_foreground(RED);
		sng_draw_circle(0, x, y, 10 + (timer % 10));
	}
	
done_drawing_item:

	sng_set_foreground(GREEN);
	sng_abs_xy_draw_string(demon_ui.error_msg, NANO_FONT, 20, 570);
	return;
}

static struct demon_cmd_def {
	char *verb;
	char *help;
} demon_cmd[] = {
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
};
static int demon_help_mode = 0;
#define DEMON_CMD_DELIM " ,"

static void demon_cmd_help(GtkWidget *w)
{
	int i;
	char buffer[100];

	sng_set_foreground(WHITE);
	if (!demon_help_mode)
		return;
	for (i = 0; i < ARRAYSIZE(demon_cmd); i++) {
		sprintf(buffer, "%s", demon_cmd[i].verb);
		sng_abs_xy_draw_string(buffer, NANO_FONT, 85, i * 18 + 60);
		sprintf(buffer, "%s", demon_cmd[i].help);
		sng_abs_xy_draw_string(buffer, NANO_FONT, 170, i * 18 + 60);
	}
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

static int construct_demon_command(char *input,
		struct demon_cmd_packet **cmd, char *errmsg)
{
	char *s;
	int i, l, g, g2, found, v;
	char *saveptr;
	struct packed_buffer *pb;
	int idcount;

	uppercase(input);
	saveptr = NULL;
	s = strtok_r(input, DEMON_CMD_DELIM, &saveptr);
	if (s == NULL) {
		strcpy(errmsg, "empty command");
		return -1;
	}

	found = 0;
	for (i = 0; i < ARRAYSIZE(demon_cmd); i++) {
		if (strncmp(demon_cmd[i].verb, s, strlen(s)))
			continue;
		found = 1;
		v = i;
		break;
	}
	if (!found) {
		sprintf(errmsg, "Unknown verb '%s'", s);
		return -1;
	}

	switch (v) {
		case 0: /* mark */
			s = strtok_r(NULL, DEMON_CMD_DELIM, &saveptr);
			if (s == NULL) {
				sprintf(errmsg, "missing argument to mark command");
				return -1;
			}
			l = get_demon_location_var(s); 
			if (l < 0) {
				sprintf(errmsg, "out of location variables");
				return -1;
			}
			demon_location[l].x = demon_ui.selectedx;
			demon_location[l].z = demon_ui.selectedz;
			break;
		case 1: /* select */
			s = strtok_r(NULL, DEMON_CMD_DELIM, &saveptr);
			if (s == NULL) {
				sprintf(errmsg, "missing argument to name command");
				return -1;
			}
			g = get_demon_group_var(s); 
			if (g < 0) {
				sprintf(errmsg, "out of group variables");
				return -1;
			}
			set_demon_group(g);
			break; 
		case 2: /* attack */
			s = strtok_r(NULL, DEMON_CMD_DELIM, &saveptr);
			if (s == NULL) {
				sprintf(errmsg, "missing 1st argument to attack command");
				return -1;
			}
			g = lookup_demon_group(s);
			if (g < 0) {
				sprintf(errmsg, "No such group '%s'", s);
				return -1;
			}
			s = strtok_r(NULL, DEMON_CMD_DELIM, &saveptr);
			if (s == NULL) {
				sprintf(errmsg, "missing 2nd argument to attack command");
				return -1;
			}
			g2 = lookup_demon_group(s);
			if (g2 < 0) {
				sprintf(errmsg, "no such group '%s'", s);
				return -1;
			}
			/* TODO - finish this */
			printf("group %d commanded to attack group %d\n", g, g2);
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
				return -1;
			}
			break; 
		case 4: /* patrol */
			s = strtok_r(NULL, DEMON_CMD_DELIM, &saveptr);
			if (s == NULL) {
				sprintf(errmsg, "missing argument to patrol command");
				return -1;
			}
			break; 
		case 5: /* halt */
			s = strtok_r(NULL, DEMON_CMD_DELIM, &saveptr);
			if (s == NULL) {
				sprintf(errmsg, "missing argument to halt command");
				return -1;
			}
			break; 
		case 6: /* identify */
			s = strtok_r(NULL, DEMON_CMD_DELIM, &saveptr);
			if (s == NULL) {
				sprintf(errmsg, "missing argument to identify command");
				return -1;
			}
			g = lookup_demon_group(s);
			if (g < 0) {
				l = lookup_demon_location(s);
				if (l < 0) { 
					sprintf(errmsg, "No such group or location '%s'\n", s);
					return -1;
				}
				demon_ui.selectedx = demon_location[l].x;
				demon_ui.selectedz = demon_location[l].z;
			}
			for (i = 0; i < demon_group[g].nids; i++)
				demon_ui.selected_id[i] = demon_group[g].id[i];
			demon_ui.nselected = demon_group[g].nids;
			break; 
		case 7: /* say */
			send_demon_comms_packet_to_server(saveptr);
			break;
		case 8: send_demon_clear_all_packet_to_server();
			break;
		case 9: send_lua_script_packet_to_server(saveptr);
			break;
		case 10: toggle_demon_ai_debug_mode();
			break;
		case 11: toggle_demon_safe_mode();
			break;
		case 12: demon_help_mode = 1; 
			break;
		default: /* unknown */
			sprintf(errmsg, "Unknown ver number %d\n", v);
			return -1;
	}
	return 0;
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

static void demon_exec_button_pressed(void *x)
{
	struct demon_cmd_packet *demon_cmd;
	int rc;

	demon_help_mode = 0;
	if (strlen(demon_ui.input) == 0)
		return;
	clear_empty_demon_variables();
	strcpy(demon_ui.error_msg, "");
	rc = construct_demon_command(demon_ui.input, &demon_cmd, demon_ui.error_msg);
	if (rc) {
		printf("Error msg: %s\n", demon_ui.error_msg);
	} else {
		printf("Command is ok\n");
	}
	snis_text_input_box_zero(demon_ui.demon_input);
}

static void set_demon_button_colors()
{
	const int selected = WHITE;
	const int deselected = GREEN;

	snis_button_set_color(demon_ui.demon_ship_button,
		demon_ui.buttonmode == DEMON_BUTTON_SHIPMODE ? selected : deselected);
	snis_button_set_color(demon_ui.demon_starbase_button,
		demon_ui.buttonmode == DEMON_BUTTON_STARBASEMODE ? selected : deselected);
	snis_button_set_color(demon_ui.demon_planet_button,
		demon_ui.buttonmode == DEMON_BUTTON_PLANETMODE ? selected : deselected);
	snis_button_set_color(demon_ui.demon_nebula_button,
		demon_ui.buttonmode == DEMON_BUTTON_NEBULAMODE ? selected : deselected);
	snis_button_set_color(demon_ui.demon_spacemonster_button,
		demon_ui.buttonmode == DEMON_BUTTON_SPACEMONSTERMODE ? selected : deselected);
	snis_button_set_color(demon_ui.demon_captain_button,
		demon_ui.buttonmode == DEMON_BUTTON_CAPTAINMODE ? selected : deselected);
}

static void demon_modebutton_pressed(int whichmode)
{
	if (demon_ui.buttonmode == whichmode)
		demon_ui.buttonmode = DEMON_BUTTON_NOMODE;
	else
		demon_ui.buttonmode = whichmode;
	set_demon_button_colors();
}

static void demon_ship_button_pressed(void *x)
{
	demon_modebutton_pressed(DEMON_BUTTON_SHIPMODE);
}

static void demon_starbase_button_pressed(void *x)
{
	demon_modebutton_pressed(DEMON_BUTTON_STARBASEMODE);
}

static void demon_planet_button_pressed(void *x)
{
	demon_modebutton_pressed(DEMON_BUTTON_PLANETMODE);
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

static void demon_delete_button_pressed(void *x)
{
	int i;

	pthread_mutex_lock(&universe_mutex);
	for (i = 0; i < demon_ui.nselected; i++) {
		queue_to_server(packed_buffer_new("bw",
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
	queue_to_server(packed_buffer_new("bw", OPCODE_DEMON_FIRE_TORPEDO,
				go[demon_ui.captain_of].id));
}

static void demon_phaser_button_pressed(void *x)
{
	if (demon_ui.captain_of < 0)
		return;
	if (go[demon_ui.captain_of].type != OBJTYPE_SHIP2)
		return;
	queue_to_server(packed_buffer_new("bw", OPCODE_DEMON_FIRE_PHASER,
				go[demon_ui.captain_of].id));
}

static void init_demon_ui()
{
	demon_ui.ux1 = 0;
	demon_ui.uy1 = 0;
	demon_ui.ux2 = XKNOWN_DIM;
	demon_ui.uy2 = ZKNOWN_DIM;
	demon_ui.nselected = 0;
	demon_ui.selectedx = -1.0;
	demon_ui.selectedz = -1.0;
	demon_ui.selectmode = 0;
	demon_ui.captain_of = -1;
	strcpy(demon_ui.error_msg, "");
	memset(demon_ui.selected_id, 0, sizeof(demon_ui.selected_id));
	demon_ui.demon_input = snis_text_input_box_init(10, 520, 30, 550, GREEN, TINY_FONT,
					demon_ui.input, 50, &timer, NULL, NULL);
	snis_text_input_box_set_return(demon_ui.demon_input,
					demon_exec_button_pressed); 
	demon_ui.demon_exec_button = snis_button_init(570, 520, 160, 30, "EXECUTE", GREEN,
			TINY_FONT, demon_exec_button_pressed, NULL);
	demon_ui.demon_ship_button = snis_button_init(3, 60, 70, 20, "SHIP", GREEN,
			PICO_FONT, demon_ship_button_pressed, NULL);
	demon_ui.demon_starbase_button = snis_button_init(3, 85, 70, 20, "STARBASE", GREEN,
			PICO_FONT, demon_starbase_button_pressed, NULL);
	demon_ui.demon_planet_button = snis_button_init(3, 110, 70, 20, "PLANET", GREEN,
			PICO_FONT, demon_planet_button_pressed, NULL);
	demon_ui.demon_nebula_button = snis_button_init(3, 135, 70, 20, "NEBULA", GREEN,
			PICO_FONT, demon_nebula_button_pressed, NULL);
	demon_ui.demon_spacemonster_button = snis_button_init(3, 160, 70, 20, "MONSTER", GREEN,
			PICO_FONT, demon_spacemonster_button_pressed, NULL);
	demon_ui.demon_captain_button = snis_button_init(3, 180, 70, 20, "CAPTAIN", GREEN,
			PICO_FONT, demon_captain_button_pressed, NULL);
	demon_ui.demon_delete_button = snis_button_init(3, 210, 70, 20, "DELETE", GREEN,
			PICO_FONT, demon_delete_button_pressed, NULL);
	demon_ui.demon_select_none_button = snis_button_init(3, 235, 70, 20, "SELECT NONE", GREEN,
			PICO_FONT, demon_select_none_button_pressed, NULL);
	demon_ui.demon_torpedo_button = snis_button_init(3, 260, 70, 20, "TORPEDO", GREEN,
			PICO_FONT, demon_torpedo_button_pressed, NULL);
	demon_ui.demon_phaser_button = snis_button_init(3, 285, 70, 20, "PHASER", GREEN,
			PICO_FONT, demon_phaser_button_pressed, NULL);
	ui_add_button(demon_ui.demon_exec_button, DISPLAYMODE_DEMON);
	ui_add_button(demon_ui.demon_ship_button, DISPLAYMODE_DEMON);
	ui_add_button(demon_ui.demon_starbase_button, DISPLAYMODE_DEMON);
	ui_add_button(demon_ui.demon_planet_button, DISPLAYMODE_DEMON);
	ui_add_button(demon_ui.demon_nebula_button, DISPLAYMODE_DEMON);
	ui_add_button(demon_ui.demon_spacemonster_button, DISPLAYMODE_DEMON);
	ui_add_button(demon_ui.demon_delete_button, DISPLAYMODE_DEMON);
	ui_add_button(demon_ui.demon_select_none_button, DISPLAYMODE_DEMON);
	ui_add_button(demon_ui.demon_captain_button, DISPLAYMODE_DEMON);
	ui_add_button(demon_ui.demon_torpedo_button, DISPLAYMODE_DEMON);
	ui_add_button(demon_ui.demon_phaser_button, DISPLAYMODE_DEMON);
	ui_add_text_input_box(demon_ui.demon_input, DISPLAYMODE_DEMON);
}

static void calculate_new_demon_zoom(int direction, gdouble x, gdouble y)
{
	double nx1, nx2, ny1, ny2, mux, muy;
	const double zoom_amount = 0.05;
	double zoom_factor;

	if (direction == GDK_SCROLL_UP)
		zoom_factor = 1.0 - zoom_amount;
	else
		zoom_factor = 1.0 + zoom_amount;
	mux = x * (demon_ui.ux2 - demon_ui.ux1) / (double) real_screen_width;
	muy = y * (demon_ui.uy2 - demon_ui.uy1) / (double) real_screen_height;
	mux += demon_ui.ux1;
	muy += demon_ui.uy1;
	nx1 = mux - zoom_factor * (mux - demon_ui.ux1);
	ny1 = muy - zoom_factor * (muy - demon_ui.uy1);
	nx2 = nx1 + zoom_factor * (demon_ui.ux2 - demon_ui.ux1);
	ny2 = ny1 + zoom_factor * (demon_ui.uy2 - demon_ui.uy1);
	demon_ui.ux1 = nx1;
	demon_ui.uy1 = ny1;
	demon_ui.ux2 = nx2;
	demon_ui.uy2 = ny2;
}

static void show_demon_groups(GtkWidget *w)
{
	int i;
	sng_set_foreground(GREEN);

	for (i = 0; i < ndemon_groups; i++)
		sng_abs_xy_draw_string(demon_group[i].name,
			NANO_FONT, SCREEN_WIDTH - 50, i * 18 + 40);
}

static void show_demon(GtkWidget *w)
{
	int x, y, i;
	double ix, iy;
	const char *letters = "ABCDEFGHIJK";
	char label[10];
	int xoffset = 7;
	int yoffset = 10;
	char buffer[100];

	if (go[my_ship_oid].alive > 0)
		sng_set_foreground(GREEN);
	else
		sng_set_foreground(RED);

	ix = XKNOWN_DIM / 10.0;
	for (x = 0; x <= 10; x++) {
		int sx1, sy1, sy2;

		sx1 = ux_to_demonsx(ix * x);
		if (sx1 < 0 || sx1 > SCREEN_WIDTH)
			continue;
		sy1 = uz_to_demonsy(0.0);
		if (sy1 < 0)
			sy1 = 0;
		if (sy1 > SCREEN_HEIGHT)
			continue;
		sy2 = uz_to_demonsy(ZKNOWN_DIM);
		if (sy2 > SCREEN_HEIGHT)
			sy2 = SCREEN_HEIGHT;
		if (sy2 < 0)
			continue;
		snis_draw_dotted_vline(w->window, gc, sx1, sy1, sy2, 5);
	}

	iy = ZKNOWN_DIM / 10.0;
	for (y = 0; y <= 10; y++) {
		int sx1, sy1, sx2;

		sy1 = uz_to_demonsy(iy * y);
		if (sy1 < 0 || sy1 > SCREEN_HEIGHT)
			continue;
		sx1 = ux_to_demonsx(0.0);
		if (sx1 < 0)
			sx1 = 0;
		if (sx1 > SCREEN_WIDTH)
			continue;
		sx2 = ux_to_demonsx(XKNOWN_DIM);
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
			tx = ux_to_demonsx(x * ix);
			ty = uz_to_demonsy(y * iy);
			sng_abs_xy_draw_string(label, NANO_FONT,
				tx + xoffset,ty + yoffset);
		}
	pthread_mutex_lock(&universe_mutex);

	for (i = 0; i <= snis_object_pool_highest_object(pool); i++)
		debug_draw_object(w, &go[i]);
	for (i = 0; i <= snis_object_pool_highest_object(sparkpool); i++)
		debug_draw_object(w, &spark[i]);
	pthread_mutex_unlock(&universe_mutex);

	if (timer & 0x02) {
		x = ux_to_demonsx(demon_ui.selectedx);
		y = uz_to_demonsy(demon_ui.selectedz);
		sng_set_foreground(BLUE);
		snis_draw_line(x - 3, y, x + 3, y);
		snis_draw_line(x, y - 3, x, y + 3);
	}

	sng_set_foreground(GREEN);
	if (netstats.elapsed_seconds == 0)
		sprintf(buffer, "Waiting for data");
	else 
		sprintf(buffer,
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
		sng_set_foreground(WHITE);
		sng_draw_dotted_line(x1, y1, x2, y1);
		sng_draw_dotted_line(x1, y2, x2, y2);
		sng_draw_dotted_line(x1, y1, x1, y2);
		sng_draw_dotted_line(x2, y1, x2, y2);
	}
	show_demon_groups(w);
	demon_cmd_help(w);
	show_common_screen(w, "DEMON");
}

struct warp_star {
	float x, y, lx, ly, vx, vy;
};

void init_warp_star(struct warp_star *star)
{
	float dx, dy;
	star->x = (float) snis_randn(SCREEN_WIDTH);
	star->y = (float) snis_randn(SCREEN_HEIGHT);

	/* cheesy avoid divide by zero. */
	if (abs(star->x) < 0.00001)
		star->x += 0.003;
	if (abs(star->y) < 0.00001)
		star->y += 0.003;

	dx = star->x - (SCREEN_WIDTH/2);
	dy = star->y - (SCREEN_HEIGHT/2);
	if (abs(dx) > abs(dy)) {
		star->vx = dx/abs(dx);
		star->vy = dy/abs(dx); 
	} else {
		star->vx = dx/abs(dy);
		star->vy = dy/abs(dy); 
	}
	star->lx = star->x;
	star->ly = star->y;
}

static void show_warp_effect(GtkWidget *w)
{
#define WARP_STARS 1000
	static int initialized = 0;
	static struct warp_star star[WARP_STARS];
	static int warp_start = 0;
	int x, y, x2, y2, i;
	if (!initialized) {
		for (i = 0; i < WARP_STARS; i++) {
			init_warp_star(&star[i]);
		}		
		initialized = 1;
	}

	/* Flash the screen white, rapidly fading to black
	 * at beginning of warp animation. This is to disguise
	 * the fact that the warp stars don't match the stars
	 * on the screen at the start of warp
	 */
	if (warp_start == 0)
		warp_start = 10;

	if (warp_start > 1) {
		sng_set_foreground(GRAY + warp_start * 25);
		snis_draw_rectangle(1, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
		warp_start--;
	}

	if (warp_limbo_countdown == 1) {
		warp_start = 0;
	}

	/* Flash the screen white, rapidly fading to black
	 * at end of warp animation. This is to disguise
	 * the fact that the warp stars don't match the stars
	 * or anything else on the screen at the end of warp.
	 * But also show the mainscreen on top of the fading
	 * to sort of "fade in" the final view.
	 */
	if (warp_limbo_countdown > 0 && warp_limbo_countdown < 10) {
		sng_set_foreground(GRAY + warp_limbo_countdown * 25);
		snis_draw_rectangle(1, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
		show_mainscreen(w);
		return;
	}

	sng_set_foreground(WHITE);
	for (i = 0; i < WARP_STARS; i++) {
		star[i].lx = star[i].x;
		star[i].x += star[i].vx;
		star[i].ly = star[i].y;
		star[i].y += star[i].vy;
		if (star[i].x < 0 || star[i].x > SCREEN_WIDTH ||
			star[i].y < 0 || star[i].y > SCREEN_HEIGHT)
			init_warp_star(&star[i]);
		star[i].vx *= 1.2;
		star[i].vy *= 1.2;
		x = (int) star[i].x;
		y = (int) star[i].y;
		x2 = (int) star[i].lx;
		y2 = (int) star[i].ly;
		snis_draw_thick_line(x, y, x2, y2);
	}
}

static void show_warp_hash_screen(GtkWidget *w)
{
	int i;
	int y1, y2;

	sng_set_foreground(WHITE);

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
	if (displaymode == DISPLAYMODE_MAINSCREEN)
		show_warp_effect(w);
	else
		show_warp_hash_screen(w);
}

struct network_setup_ui {
	struct button *start_lobbyserver;
	struct button *start_gameserver;
	struct button *connect_to_lobby;
	struct snis_text_input_box *lobbyservername;
	struct snis_text_input_box *gameservername;
	struct snis_text_input_box *shipname_box;
	struct snis_text_input_box *password_box;
	struct button *role_main;
	struct button *role_nav;
	struct button *role_weap;
	struct button *role_eng;
	struct button *role_damcon;
	struct button *role_sci;
	struct button *role_comms;
	struct button *role_sound;
	struct button *role_demon;
	int role_main_v;
	int role_nav_v;
	int role_weap_v;
	int role_eng_v;
	int role_damcon_v;
	int role_sci_v;
	int role_comms_v;
	int role_sound_v;
	int role_demon_v;
	char lobbyname[60];
	char servername[60];
	char shipname[22];
	char password[10];
} net_setup_ui;

static void lobby_hostname_entered()
{
	printf("lobby hostname entered: %s\n", net_setup_ui.lobbyname);
}

static void gameserver_hostname_entered()
{
	printf("game server hostname entered: %s\n", net_setup_ui.servername);
}

static void shipname_entered()
{
	printf("shipname entered: %s\n", net_setup_ui.lobbyname);
}

static void password_entered()
{
	printf("password entered: %s\n", net_setup_ui.lobbyname);
}

static void start_lobbyserver_button_pressed()
{
	char *snisbindir;
	char bindir[PATH_MAX];
	char cmd[PATH_MAX];
	struct stat statbuf;
	int rc;
	pid_t child;
	const char errorstr[] = "Failed to exec ssgl_server.\n";

	snisbindir = getenv("SNISBINDIR");
	if (!snisbindir) {
		snisbindir = STRPREFIX(PREFIX);
		snprintf(bindir, sizeof(bindir), "%s/bin", snisbindir);
	} else {
		strcpy(bindir, snisbindir);
	}

	/* test that snisbindir is actually a directory. */
	rc = stat(bindir, &statbuf);
	if (rc < 0) {
		fprintf(stderr, "Cannot stat %s: %s\n", bindir, strerror(errno));
		return;
	}
	if (!S_ISDIR(statbuf.st_mode)) {
		fprintf(stderr, "%s is not a directory.\n", snisbindir);
		return;
	}

	printf("start lobby server button pressed.\n");
	snprintf(cmd, sizeof(cmd), "%s/ssgl_server", bindir);

	child = fork();
	if (child < 0) {
		fprintf(stderr, "Failed to fork lobby server process: %s\n", strerror(errno));
		return;
	}
	if (child == 0) { /* This is the child process */
		printf("execl'ing %s\n", cmd);
		fflush(stdout);
		execl(cmd,  "ssgl_server", NULL);
		/*
		 * if execl returns at all, there was an error, and btw, be careful, very
		 * limited stuff that we can safely call, similar to limitations of signal
		 * handlers.  E.g. fprintf is not safe to call here, and exit(3) is not safe,
		 * but write(2) and _exit(2) are ok.   Compiler with -O3 warns if I ignore
		 * return value of write(2) though there's not much I can do with it.
		 * Casting return value to void does not prevent the warning.
		 */
		if (write(2, errorstr, sizeof(errorstr)) != sizeof(errorstr))
			_exit(-2);
		else
			_exit(-1);
	}
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

static void start_gameserver_button_pressed()
{
	char command[PATH_MAX], bindir[PATH_MAX];
	char *snisbindir;
	pid_t child;
	const char errorstr[] = "Failed to exec snis_server.\n";

	/* FIXME this is probably not too cool. */
	sanitize_string(net_setup_ui.servername);
	sanitize_string(net_setup_ui.lobbyname);

	/* These must be set in order to start the game server. */
	if (strcmp(net_setup_ui.servername, "") == 0 || 
		strcmp(net_setup_ui.lobbyname, "") == 0)
		return;

	snisbindir = getenv("SNISBINDIR");
	if (!snisbindir) {
		snisbindir = STRPREFIX(PREFIX);
		snprintf(bindir, sizeof(bindir), "%s/bin", snisbindir);
	} else {
		strcpy(bindir, snisbindir);
	}

	memset(command, 0, sizeof(command));
	snprintf(command, 200, "%s/snis_server %s SNIS '%s' . &",
			bindir, net_setup_ui.lobbyname, net_setup_ui.servername);
	printf("start game server button pressed.\n");

	snprintf(command, sizeof(command), "%s/snis_server", bindir);
	child = fork();
	if (child < 0) {
		fprintf(stderr, "Failed to fork SNIS game server process: %s\n", strerror(errno));
		return;
	}
	if (child == 0) { /* This is the child process */
		execl(command, "snis_server", net_setup_ui.lobbyname, "SNIS",
				net_setup_ui.servername, ".", NULL);
		/*
		 * if execl returns at all, there was an error, and btw, be careful, very
		 * limited stuff that we can safely call, similar to limitations of signal
		 * handlers.  E.g. fprintf is not safe to call here, and exit(3) is not safe,
		 * but write(2) and _exit(2) are ok.   Compiler with -O3 warns if I ignore
		 * return value of write(2) though there's not much I can do with it.
		 * Casting return value to void does not prevent the warning.
		 */
		if (write(2, errorstr, sizeof(errorstr)) != sizeof(errorstr))
			_exit(-2);
		else
			_exit(-1);
	}
}

static void connect_to_lobby_button_pressed()
{
	printf("connect to lobby pressed\n");
	/* These must be set to connect to the lobby... */
	if (strcmp(net_setup_ui.lobbyname, "") == 0 ||
		strcmp(net_setup_ui.shipname, "") == 0 ||
		strcmp(net_setup_ui.password, "") == 0)
		return;

	printf("connecting to lobby...\n");
	displaymode = DISPLAYMODE_LOBBYSCREEN;
	lobbyhost = net_setup_ui.lobbyname;
	shipname = net_setup_ui.shipname;
	password = net_setup_ui.shipname;
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
	if (role == 0)
		role = ROLE_ALL;
	connect_to_lobby();
}

static struct button *init_net_role_button(int x, int *y, char *txt, int *value)
{
	struct button *b;
	b = snis_button_init(x, *y, 225, 23, txt, GREEN,
			NANO_FONT, NULL, NULL);
	snis_button_checkbox(b, value);
	*y = *y + 23;
	return b;
}

static void ui_add_button(struct button *b, int active_displaymode);

static void init_net_role_buttons(struct network_setup_ui *nsu)
{
	int x, y;

	x = 520;
	y = 345;

	nsu->role_main_v = 0;
	nsu->role_nav_v = 1;
	nsu->role_weap_v = 1;
	nsu->role_eng_v = 1;
	nsu->role_sci_v = 1;
	nsu->role_comms_v = 1;
	nsu->role_sound_v = 1;
	nsu->role_damcon_v = 1;
	nsu->role_demon_v = 1;
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
	ui_add_button(nsu->role_main, DISPLAYMODE_NETWORK_SETUP);
	ui_add_button(nsu->role_nav, DISPLAYMODE_NETWORK_SETUP);
	ui_add_button(nsu->role_weap, DISPLAYMODE_NETWORK_SETUP);
	ui_add_button(nsu->role_eng, DISPLAYMODE_NETWORK_SETUP);
	ui_add_button(nsu->role_damcon, DISPLAYMODE_NETWORK_SETUP);
	ui_add_button(nsu->role_sci, DISPLAYMODE_NETWORK_SETUP);
	ui_add_button(nsu->role_comms, DISPLAYMODE_NETWORK_SETUP);
	ui_add_button(nsu->role_sound, DISPLAYMODE_NETWORK_SETUP);
	ui_add_button(nsu->role_demon, DISPLAYMODE_NETWORK_SETUP);
}

static void ui_add_text_input_box(struct snis_text_input_box *t, int active_displaymode);
static void init_net_setup_ui(void)
{
	int y = 10 + LINEHEIGHT * 3;

	memset(net_setup_ui.lobbyname, 0, sizeof(net_setup_ui.lobbyname));
	strcpy(net_setup_ui.lobbyname, "localhost");
	strcpy(net_setup_ui.servername, "");
	y += 50;
	net_setup_ui.lobbyservername =
		snis_text_input_box_init(20, y, 30, 750, GREEN, TINY_FONT,
					net_setup_ui.lobbyname, 50, &timer,
					lobby_hostname_entered, NULL);
	y += 50;
	net_setup_ui.start_lobbyserver =	
		snis_button_init(20, y, 300, 25, "START LOBBY SERVER", GREEN,
			TINY_FONT, start_lobbyserver_button_pressed, NULL);
	y += 100;
	net_setup_ui.gameservername =
		snis_text_input_box_init(20, y, 30, 750, GREEN, TINY_FONT,
					net_setup_ui.servername, 50, &timer,
					gameserver_hostname_entered, NULL);
	y += 50;
	net_setup_ui.start_gameserver = 
		snis_button_init(20, y, 300, 25, "START GAME SERVER", RED,
			TINY_FONT, start_gameserver_button_pressed, NULL);
	y += 100;
	net_setup_ui.shipname_box =
		snis_text_input_box_init(150, y, 30, 250, GREEN, TINY_FONT,
					net_setup_ui.shipname, 50, &timer,
					shipname_entered, NULL);
	y += 50;
	net_setup_ui.password_box =
		snis_text_input_box_init(150, y, 30, 250, GREEN, TINY_FONT,
					net_setup_ui.password, 50, &timer,
					password_entered, NULL);
	y += 50;
	net_setup_ui.connect_to_lobby = 
		snis_button_init(20, y, 300, 25, "CONNECT TO LOBBY", RED,
			TINY_FONT, connect_to_lobby_button_pressed, NULL);
	init_net_role_buttons(&net_setup_ui);
	ui_add_button(net_setup_ui.start_lobbyserver, DISPLAYMODE_NETWORK_SETUP);
	ui_add_button(net_setup_ui.start_gameserver, DISPLAYMODE_NETWORK_SETUP);
	ui_add_button(net_setup_ui.connect_to_lobby, DISPLAYMODE_NETWORK_SETUP);

	/* note: the order of these is important for TAB key focus advance */
	ui_add_text_input_box(net_setup_ui.password_box, DISPLAYMODE_NETWORK_SETUP);
	ui_add_text_input_box(net_setup_ui.shipname_box, DISPLAYMODE_NETWORK_SETUP);
	ui_add_text_input_box(net_setup_ui.gameservername, DISPLAYMODE_NETWORK_SETUP);
	ui_add_text_input_box(net_setup_ui.lobbyservername, DISPLAYMODE_NETWORK_SETUP);
} 

static void show_network_setup(GtkWidget *w)
{
	show_common_screen(w, "SPACE NERDS IN SPACE");
	sng_set_foreground(DARKGREEN);
	sng_draw_vect_obj(&snis_logo, 100, 500);
	sng_set_foreground(GREEN);
	sng_abs_xy_draw_string("NETWORK SETUP", SMALL_FONT, 25, 10 + LINEHEIGHT * 2);
	sng_abs_xy_draw_string("LOBBY SERVER NAME OR IP ADDRESS", TINY_FONT, 25, 130);
	sng_abs_xy_draw_string("GAME SERVER NICKNAME", TINY_FONT, 25, 280);
	sng_abs_xy_draw_string("SHIP NAME", TINY_FONT, 20, 470);
	sng_abs_xy_draw_string("PASSWORD", TINY_FONT, 20, 520);

	sanitize_string(net_setup_ui.servername);
	sanitize_string(net_setup_ui.lobbyname);
	if (strcmp(net_setup_ui.servername, "") != 0 &&
		strcmp(net_setup_ui.lobbyname, "") != 0)
		snis_button_set_color(net_setup_ui.start_gameserver, GREEN);
	else
		snis_button_set_color(net_setup_ui.start_gameserver, RED);

	if (strcmp(net_setup_ui.lobbyname, "") != 0 &&
		strcmp(net_setup_ui.shipname, "") != 0 &&
		strcmp(net_setup_ui.password, "") != 0)
		snis_button_set_color(net_setup_ui.connect_to_lobby, GREEN);
	else
		snis_button_set_color(net_setup_ui.connect_to_lobby, RED);
}

static void make_science_forget_stuff(void)
{
	int i;

	/* After awhile, science forgets... this is so the scientist
	 * can't just scan everything then sit back and relax for the
	 * rest of the game.
	 */
	pthread_mutex_lock(&universe_mutex);
	for (i = 0; i <= snis_object_pool_highest_object(pool); i++) {
		struct snis_entity *o = &go[i];
		if (o->sdata.science_data_known) /* forget after awhile */
			o->sdata.science_data_known--;
	}
	pthread_mutex_unlock(&universe_mutex);
}

static int main_da_scroll(GtkWidget *w, GdkEvent *event, gpointer p)
{
	struct _GdkEventScroll *e = (struct _GdkEventScroll *) event;
	struct snis_entity *o;
	int16_t newval = 0;

	if (!(o = find_my_ship()))
		return 0;
	switch (displaymode) {
	case DISPLAYMODE_SCIENCE:
		if (e->direction == GDK_SCROLL_UP)
			newval = o->tsd.ship.scizoom - 30;
		if (e->direction == GDK_SCROLL_DOWN)
			newval = o->tsd.ship.scizoom + 30;
		if (newval < 0)
			newval = 0;
		if (newval > 255)
			newval = 255;
		do_adjust_byte_value((uint8_t) newval, OPCODE_REQUEST_SCIZOOM);
		return 0;
	case DISPLAYMODE_DEMON:
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
	default:
		return 0;
	}
	return 0;
}

static char *help_text[] = {

	/* Main screen help text */
	"MAIN SCREEN\n\n"
	"  CONTROLS\n\n"
	"  * USE ARROW KEYS TO TURN SHIP\n\n"
	"  * SHIFT-W KEY TOGGLES BETWEEN WEAPONS VIEW\n"
	"    AND MAIN VIEW\n\n"
	"  * 1 KEY TOGGLES CAMERA VIEW MODES\n"
	"\nPRESS ESC TO EXIT HELP\n",

	/* Navigation help text */
	"NAVIGATION\n\n"
	"  CONTROLS\n\n"
	"  *   YAW: LEFT/RIGHT, a/d, OR USE MOUSE WHEEL\n"
	"  * PITCH: UP/DOWN, w/s (CTRL-I INVERTS CONTROLS)\n"
	"  *  ROLL: Q/E\n"
	"  * R BUTTON ABOVE THROTTLE REVERSES THRUST\n"
	"  * USE WARP FOR FAST TRAVEL\n"
	"  * USE PLUS/MINUS KEYS TO ZOOM VIEW\n"
	"  * USE THROTTLE SLIDER TO SET THROTTLE\n"
	"\nPRESS ESC TO EXIT HELP\n",

	/* Weapons help text */
	"WEAPONS\n\n"
	"  CONTROLS\n\n"
	"  * USE ARROW KEYS OR MOUSE TO AIM WEAPONS\n"
	"  * FIRE WITH SPACE BAR OR MOUSE BUTTON\n"
	"  * PLUS/MINUS KEYS SET PHASER WAVELENGTH\n"
	"    (OR USE MOUSE WHEEL)\n"
	"  * MATCH PHASER WAVELENGTH TO WEAKNESSES\n"
	"    IN ENEMY SHIELDS\n"
	"  * CTRL-I INVERTS VERTICAL KEYBOARD CONTROLS\n"
	"\nPRESS ESC TO EXIT HELP\n",

	/* Engineering help text */
	"ENGINEERING\n\n"
	"  CONTROLS\n\n"
	"  * USE SLIDERS ON LEFT SIDE OF SCREEN\n"
        "    TO LIMIT POWER CONSUMPTION OF SHIP\n"
        "    SYSTEMS AND TO ALLOCATE COOLANT\n"
        "  * HEALTH AND TEMPERATURE OF SYSTEMS\n"
	"    IS INDICATED ON RIGHT SIDE OF SCREEN\n"
	"  * OVERHEATING SYSTEMS FLASH RED AND\n"
	"    CAUSE DAMAGE TO THEMSELVES\n"
	"\nPRESS ESC TO EXIT HELP\n",

	/* Science help text */
	"SCIENCE\n\n"
	"  CONTROLS\n\n"
	"  * USE MOUSE WHEEL TO ZOOM/UNZOOM\n"
	"  * USE UP/DOWN ARROWS TO FOCUS/WIDEN\n"
	"    SCANNING BEAM\n"
	"  * SELECT TARGETS WITH MOUSE TO EXAMINE\n"
	"  * USE DETAILS BUTTON FOR MORE INFO\n"
	"  * WARP DRIVE CALCULATIONS IN LOWER LEFT\n"
	"\nPRESS ESC TO EXIT HELP\n",


	/* Comms help text */
	"COMMUNICATIONS\n"
	"  CONTROLS\n"
	"  * ZOOM CONTROLS MAIN SCREEN ZOOM\n"
	"  * RED ALERT SOUNDS RED ALERT ALARM\n"
	"  * TOP ROW OF BUTTONS CONTROLS MAIN SCREEN\n"
	"  COMMANDS\n"
	"  * COMMANDS ARE PRECEDED BY FORWARD SLASH ->  /\n"
	"  * /help\n"
	"  * /channel channel-number - change current channel\n"
	"  * /hail ship-name - hail ship on current channel\n"
	"\nPRESS ESC TO EXIT HELP\n",

	/* Demon screen help text */
	"DEMON\n\n"
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
	"* USE \"HELP\" COMAND IN TEXT BOX FOR MORE INFO\n"
	"\nPRESS ESC TO EXIT HELP\n",

	/* Damage control help text */
	"TO DO\n"
	"   HELP TEXT FOR DAMAGE CONTROL",

	"help text 8",
	"help text 9",
	"TO DO\n"
	"   HELP TEXT FOR LOBBY CONNECTION SCREEN",
	"help text 11",
	"help text 12",
	"help text 13",
	"TO DO\n"
	"   HELP TEXT FOR NETWORK SETUP SCREEN\n",
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
			if (line >= helpmodeline && line < helpmodeline + 20) {
				buffer[buflen] = '\0';
				sng_abs_xy_draw_string(buffer, TINY_FONT, 60, y);
				y += 19;
				strcpy(buffer, "");
				buflen = 0;
				line++;
				if (text[i] == '\0')
					break;
				i++;
				continue;
			} else {
				if (line >= helpmodeline + 20)
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
	sng_set_foreground(GREEN);
	snis_draw_rectangle(0, 50, 50, SCREEN_WIDTH - 100, SCREEN_HEIGHT - 100);
	if (displaymode < 0 || displaymode >= ARRAYSIZE(help_text)) {
		draw_help_text(w, "Unknown screen, no help available");
		return;
	}
	draw_help_text(w, help_text[displaymode]);
}

#define QUIT_BUTTON_WIDTH 200
#define QUIT_BUTTON_HEIGHT 50
#define QUIT_BUTTON_X 130
#define QUIT_BUTTON_Y 420
#define NOQUIT_BUTTON_X 480

static void draw_quit_screen(GtkWidget *w)
{
	int x;
	static int quittimer = 0;

	quittimer++;

	sng_set_foreground(BLACK);
	snis_draw_rectangle(1, 100, 100, SCREEN_WIDTH - 200, SCREEN_HEIGHT - 200);
	sng_set_foreground(RED);
	snis_draw_rectangle(FALSE, 100, 100, SCREEN_WIDTH-200, SCREEN_HEIGHT-200);
	sng_set_foreground(WHITE);
	sng_abs_xy_draw_string("Quit?", BIG_FONT, 300, 280);

	if (current_quit_selection == 1) {
		x = QUIT_BUTTON_X;
		sng_set_foreground(WHITE);
	} else {
		x = NOQUIT_BUTTON_X;
		sng_set_foreground(RED);
	}
	sng_abs_xy_draw_string("Quit Now", SMALL_FONT, 150, 450);
	if (current_quit_selection == 0)
		sng_set_foreground(WHITE);
	else
		sng_set_foreground(RED);
	sng_abs_xy_draw_string("Don't Quit", SMALL_FONT, 500, 450);

	if ((quittimer & 0x04)) {
		sng_set_foreground(WHITE);
		snis_draw_rectangle(FALSE, x, QUIT_BUTTON_Y,
			QUIT_BUTTON_WIDTH, QUIT_BUTTON_HEIGHT);
	}
}

#define FRAME_INDEX_MAX 10

static int main_da_expose(GtkWidget *w, GdkEvent *event, gpointer p)
{
	static double last_frame_time = 0;
	static int frame_index = 0;
	static float frame_rates[FRAME_INDEX_MAX];
	static float frame_times[FRAME_INDEX_MAX];

	double start_time = time_now_double();

	struct snis_entity *o;

	make_science_forget_stuff();

	if (displaymode == DISPLAYMODE_GLMAIN)	
		return 0;

	load_textures();

#ifndef WITHOUTOPENGL
	GdkGLContext *gl_context = gtk_widget_get_gl_context(main_da);
	GdkGLDrawable *gl_drawable = gtk_widget_get_gl_drawable(main_da);

	if (!gdk_gl_drawable_gl_begin(gl_drawable, gl_context))
		g_assert_not_reached();

	graph_dev_start_frame();
#endif

	sng_set_foreground(WHITE);
	
#if 0	
	for (i = 0; i <= highest_object_number;i++) {
		if (!game_state.go[i].alive)
			continue;
		if (onscreen(&game_state.go[i]))
			game_state.go[i].draw(&game_state.go[i], main_da); 
	}
#endif

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
		if (!(o = find_my_ship()))
			goto end_of_drawing;
		if (o->alive <= 0 && displaymode != DISPLAYMODE_DEMON) {
			red_alert_mode = 0;
			show_death_screen(w);
			if (in_the_process_of_quitting)
				draw_quit_screen(w);
			goto end_of_drawing;
		}
	}
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
	case DISPLAYMODE_FINDSERVER:
		show_fonttest(w);
		break;
	case DISPLAYMODE_MAINSCREEN:
		show_mainscreen(w);
		break;
	case DISPLAYMODE_NAVIGATION:
		show_navigation(w);
		break;
	case DISPLAYMODE_WEAPONS:
		show_manual_weapons(w);
		break;
	case DISPLAYMODE_ENGINEERING:
		show_engineering(w);
		break;
	case DISPLAYMODE_SCIENCE:
		if (sci_ui.details_mode == SCI_DETAILS_MODE_THREED)
			show_3d_science(w);
		else
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

	/* this has to come after ui_element_list_draw() to avoid getting clobbered */
	if (displaymode == DISPLAYMODE_ENGINEERING)
		show_engineering_damage_report(w, eng_ui.selected_subsystem);

	if (helpmode)
		draw_help_screen(w);
	if (in_the_process_of_quitting)
		draw_quit_screen(w);

	if (display_frame_stats > 0) {
		float avg_frame_rate=0;
		float avg_frame_time=0;
		int i;
		for (i=0; i<FRAME_INDEX_MAX; i++) {
			avg_frame_rate += frame_rates[i];
			avg_frame_time += frame_times[i];
		}
		avg_frame_rate /= (float)FRAME_INDEX_MAX;
		avg_frame_time /= (float)FRAME_INDEX_MAX;

		sng_set_foreground(WHITE);
		char stat_buffer[30];
		sprintf(stat_buffer, "fps %5.2f", 1.0/avg_frame_rate);
		sng_abs_xy_draw_string(stat_buffer, NANO_FONT, 2, 10);
		sprintf(stat_buffer, "t %0.2f ms", avg_frame_time*1000.0);
		sng_abs_xy_draw_string(stat_buffer, NANO_FONT, 92, 10);
		sprintf(stat_buffer, "%8.0f", universe_timestamp());
		sng_abs_xy_draw_string(stat_buffer, NANO_FONT, SCREEN_WIDTH-85, 10);
	}
	if (display_frame_stats > 1) {
		graph_dev_display_debug_menu_show();
	}

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

	if (display_frame_stats > 0) {
		double end_time = time_now_double();

		frame_rates[frame_index] = start_time - last_frame_time;
		frame_times[frame_index] = end_time - start_time;
		frame_index = (frame_index+1) % FRAME_INDEX_MAX;
		last_frame_time = start_time;
	}
	return 0;
}

void really_quit(void);

gint advance_game(gpointer data)
{
	timer++;

	if (red_alert_mode && (role & ROLE_SOUNDSERVER) && (timer % 45) == 0)
		wwviaudio_add_sound(RED_ALERT_SOUND);

	deal_with_joystick();
	deal_with_keyboard();
	deal_with_physical_io_devices();

	if (in_the_process_of_quitting) {
		gdk_threads_enter();
		gtk_widget_queue_draw(main_da);
		gdk_threads_leave();
		if (final_quit_selection)
			really_quit();
		return TRUE;
	}

	gdk_threads_enter();
	gtk_widget_queue_draw(main_da);
	move_sparks();
	pthread_mutex_lock(&universe_mutex);
	move_objects();
	pthread_mutex_unlock(&universe_mutex);
	nframes++;

	GtkAllocation alloc;
	gtk_widget_get_allocation(main_da, &alloc);
	gdk_window_invalidate_rect(gtk_widget_get_root_window(main_da), &alloc, FALSE);

	gdk_threads_leave();
	return TRUE;
}

static unsigned int load_texture(char *filename)
{
	char fname[PATH_MAX + 1];

	sprintf(fname, "%s/textures/%s", asset_dir, filename);
	return graph_dev_load_texture(fname);
}

static unsigned int load_cubemap_textures(int is_inside, char *filenameprefix)
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
	char filename[6][PATH_MAX + 1];

	for (i = 0; i < 6; i++)
		sprintf(filename[i], "%s/textures/%s%d.png", asset_dir, filenameprefix, i);

	return graph_dev_load_cubemap_texture(is_inside, filename[1], filename[3], filename[4],
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
	char filename[6][PATH_MAX + 1];

	for (i = 0; i < 6; i++)
		sprintf(filename[i], "%s/textures/%s%d.png", asset_dir, filenameprefix, i);

	graph_dev_load_skybox_texture(filename[3], filename[1], filename[4],
					filename[5], filename[0], filename[2]);
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

	static int static_exc_loaded = 0;
	if (!static_exc_loaded) {
		struct entity *e = add_entity(ecx, sun_mesh, SUNX, SUNY, SUNZ, WHITE);
		if (e)
			update_entity_material(e, &sun_material);

		static_exc_loaded = 1;
	}

	return TRUE;
}

static void load_textures(void)
{
	if (textures_loaded)
		return;
	load_skybox_textures(skybox_texture_prefix);

	material_init_textured_particle(&green_phaser_material);
	green_phaser_material.textured_particle.texture_id = load_texture("green-burst.png");
	green_phaser_material.textured_particle.radius = 0.75;
	green_phaser_material.textured_particle.time_base = 0.25;

	material_init_texture_mapped_unlit(&red_laser_material);
	red_laser_material.billboard_type = MATERIAL_BILLBOARD_TYPE_AXIS;
	red_laser_material.texture_mapped_unlit.texture_id = load_texture("red-laser-texture.png");
	red_laser_material.texture_mapped_unlit.do_blend = 1;

	material_init_texture_mapped_unlit(&blue_tractor_material);
	blue_tractor_material.billboard_type = MATERIAL_BILLBOARD_TYPE_AXIS;
	blue_tractor_material.texture_mapped_unlit.texture_id = load_texture("blue-tractor-texture.png");
	blue_tractor_material.texture_mapped_unlit.do_blend = 1;

	material_init_texture_mapped_unlit(&red_torpedo_material);
	red_torpedo_material.billboard_type = MATERIAL_BILLBOARD_TYPE_SPHERICAL;
	red_torpedo_material.texture_mapped_unlit.texture_id = load_texture("red-torpedo-texture.png");
	red_torpedo_material.texture_mapped_unlit.do_blend = 1;

	material_init_texture_mapped_unlit(&spark_material);
	spark_material.billboard_type = MATERIAL_BILLBOARD_TYPE_SPHERICAL;
	spark_material.texture_mapped_unlit.texture_id = load_texture("spark-texture.png");
	spark_material.texture_mapped_unlit.do_blend = 1;

	material_init_texture_mapped_unlit(&warp_effect_material);
	warp_effect_material.billboard_type = MATERIAL_BILLBOARD_TYPE_SPHERICAL;
	warp_effect_material.texture_mapped_unlit.texture_id = load_texture("warp-effect.png");
	warp_effect_material.texture_mapped_unlit.do_blend = 1;

	material_init_texture_mapped_unlit(&sun_material);
	sun_material.billboard_type = MATERIAL_BILLBOARD_TYPE_SPHERICAL;
	sun_material.texture_mapped_unlit.texture_id = load_texture("sun.png");
	sun_material.texture_mapped_unlit.do_blend = 1;

	int i;
	for (i = 0; i < NPLANETARY_RING_MATERIALS; i++) {
		char filename[25];
		sprintf(filename, "planetary-ring%d.png", i);

		material_init_textured_planet_ring(&planetary_ring_material[i]);
		planetary_ring_material[i].textured_planet_ring.texture_id = load_texture(filename);
		planetary_ring_material[i].textured_planet_ring.alpha = 0.5;
	}

	for (i = 0; i < NPLANET_MATERIALS; i++) {
		char filename[25];
		sprintf(filename, "planet-texture%d-", i);

		material_init_textured_planet(&planet_material[i]);
		planet_material[i].textured_planet.texture_id = load_cubemap_textures(0, filename);
		planet_material[i].textured_planet.ring_material = 0;

		int k;
		for (k = 0; k < NPLANETARY_RING_MATERIALS; k++) {
			int pm_index = (k + 1) * NPLANET_MATERIALS + i;
			planet_material[pm_index] = planet_material[i];
			planet_material[pm_index].textured_planet.ring_material = &planetary_ring_material[k];
		}
	}

	material_init_textured_shield(&shield_material);
	shield_material.textured_shield.texture_id = load_cubemap_textures(0, "shield-effect-");

	for (i = 0; i < NNEBULA_MATERIALS; i++) {
		char filename[20];
		sprintf(filename, "nebula%d.mat", i);

		material_nebula_read_from_file(asset_dir, filename, &nebula_material[i]);
	}

	material_init_texture_cubemap(&asteroid_material[0]);
	asteroid_material[0].texture_cubemap.texture_id = load_cubemap_textures(0, "asteroid1-");
	material_init_texture_cubemap(&asteroid_material[1]);
	asteroid_material[1].texture_cubemap.texture_id = load_cubemap_textures(0, "asteroid2-");

	material_init_texture_mapped_unlit(&wormhole_material);
	wormhole_material.texture_mapped_unlit.texture_id = load_texture("wormhole.png");
	wormhole_material.texture_mapped_unlit.do_cullface = 0;
	wormhole_material.texture_mapped_unlit.do_blend = 1;
	wormhole_material.texture_mapped_unlit.tint = sng_get_color(MAGENTA);
	wormhole_material.texture_mapped_unlit.alpha = 0.5;

	material_init_textured_particle(&thrust_material);
	thrust_material.textured_particle.texture_id = load_texture("thrust.png");
	thrust_material.textured_particle.radius = 1.5;
	thrust_material.textured_particle.time_base = 0.1;

	textures_loaded = 1;
}

static int main_da_button_press(GtkWidget *w, GdkEventButton *event,
	__attribute__((unused)) void *unused)
{
	switch (displaymode) {
		case DISPLAYMODE_DEMON:
			demon_button_press(event->button, event->x, event->y);
			break;
		default:
			break;
	}
	return TRUE;
}

static int main_da_button_release(GtkWidget *w, GdkEventButton *event,
	__attribute__((unused)) void *unused)
		
{
	if (display_frame_stats) {
		if (graph_dev_graph_dev_debug_menu_click(event->x, event->y))
			return TRUE;
	}

	if (in_the_process_of_quitting) {
		int sx = (int) ((0.0 + event->x) / (0.0 + real_screen_width) * SCREEN_WIDTH);
		int sy = (int) ((0.0 + event->y) / (0.0 + real_screen_height) * SCREEN_HEIGHT);
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
		lobbylast1clickx = (int) ((0.0 + event->x) / (0.0 + real_screen_width) * SCREEN_WIDTH);
		lobbylast1clicky = (int) ((0.0 + event->y) / (0.0 + real_screen_height) * SCREEN_HEIGHT);
		
		break;
	case DISPLAYMODE_SCIENCE:
		science_button_press((int) ((0.0 + event->x) / (0.0 + real_screen_width) * SCREEN_WIDTH),
				(int) ((0.0 + event->y) / (0.0 + real_screen_height) * SCREEN_HEIGHT));
		break;
	case DISPLAYMODE_DEMON:
		demon_button_release(event->button, event->x, event->y);
		break;
	case DISPLAYMODE_WEAPONS:
		if (event->button == 1)
			do_laser();
		if (event->button == 3) {
			do_torpedo();
			load_torpedo_button_pressed();
		}
		break;
	default:
		break;
	}
	ui_element_list_button_press(uiobjs,
		(int) ((0.0 + event->x) / (0.0 + real_screen_width) * SCREEN_WIDTH),
		(int) ((0.0 + event->y) / (0.0 + real_screen_height) * SCREEN_HEIGHT));
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

static int main_da_motion_notify(GtkWidget *w, GdkEventMotion *event,
	__attribute__((unused)) void *unused)
{
	float pitch, yaw;
	float smoothx, smoothy;
	int sx, sy;

	switch (displaymode) {
	case DISPLAYMODE_DEMON:
		demon_ui.ix2 = demon_mousex_to_ux(event->x);
		demon_ui.iz2 = demon_mousey_to_uz(event->y);
		break;
	case DISPLAYMODE_WEAPONS:
		/* FIXME: throttle this network traffic */
		smooth_mousexy(event->x, event->y, &smoothx, &smoothy);
		yaw = weapons_mousex_to_yaw(smoothx);
		pitch = weapons_mousey_to_pitch(smoothy);
		queue_to_server(packed_buffer_new("bRR", OPCODE_REQUEST_WEAPONS_YAW_PITCH,
					yaw, pitch));
		break;
	case DISPLAYMODE_ENGINEERING:
		sx = (int) ((0.0 + event->x) / (0.0 + real_screen_width) * SCREEN_WIDTH);
		sy = (int) ((0.0 + event->y) / (0.0 + real_screen_height) * SCREEN_HEIGHT);
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

void really_quit(void)
{

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
	close_joystick();
	exit(1); /* probably bad form... oh well. */
}

static void usage(void)
{
	fprintf(stderr, "usage: snis_client --lobbyhost lobbyhost --starship starshipname --pw password\n");
	fprintf(stderr, "       Example: ./snis_client --lobbyhost localhost --starship Enterprise --pw tribbles\n");
	exit(1);
}

static void read_ogg_clip(int sound, char *directory, char *filename)
{
	char path[PATH_MAX];

	sprintf(path, "%s/sounds/%s", directory, filename);
	wwviaudio_read_ogg_clip(sound, path);
}

static void override_asset_dir(void)
{
	char *d;

	asset_dir = default_asset_dir;
	d = getenv("SNIS_ASSET_DIR");
	if (!d)
		return;
	asset_dir = d;
}

static int read_ship_types(void)
{
	char path[PATH_MAX];

	sprintf(path, "%s/%s", asset_dir, "ship_types.txt");

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

	sprintf(path, "%s/%s", asset_dir, "factions.txt");

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
	read_ogg_clip(TORPEDO_LAUNCH_SOUND, d, "flak_gun_sound.ogg");
	read_ogg_clip(LASER_FIRE_SOUND, d, "bigshotlaser.ogg");
	read_ogg_clip(ONSCREEN_SOUND, d, "onscreen.ogg");
	read_ogg_clip(OFFSCREEN_SOUND, d, "offscreen.ogg");
	read_ogg_clip(CHANGESCREEN_SOUND, d, "changescreen.ogg");
	read_ogg_clip(SLIDER_SOUND, d, "slider-noise.ogg");
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
	printf("Done.\n");
}

static void setup_sound(void)
{
	if (wwviaudio_initialize_portaudio(MAX_CONCURRENT_SOUNDS, NSOUND_CLIPS) != 0) {
		printf("Failed to initialize sound system\n");
		return;
	}
	read_sound_clips();
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
	case DEVIO_OPCODE_ENG_SHIELD_LEVEL:
		snis_slider_poke_input(eng_ui.shield_control_slider, d, 1);
		break;
	case DEVIO_OPCODE_ENG_PRESET1_BUTTON:
		preset1_button_pressed((void *) 0);
		break;
	case DEVIO_OPCODE_ENG_PRESET2_BUTTON:
		preset2_button_pressed((void *) 0);
		break;
	case DEVIO_OPCODE_ENG_DAMAGE_CTRL:
		damcon_button_pressed((void *) 0);
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
	case DEVIO_OPCODE_SCIENCE_RANGE:
		snis_slider_poke_input(sci_ui.scizoom, d, 1);
		break;
	case DEVIO_OPCODE_SCIENCE_TRACTOR:
		sci_tractor_pressed((void *) 0);
		break;
	case DEVIO_OPCODE_SCIENCE_SRS:
		sci_sciplane_pressed((void *) 0);
		break;
	case DEVIO_OPCODE_SCIENCE_LRS:
		sci_threed_pressed((void *) 0);
		break;
	case DEVIO_OPCODE_SCIENCE_DETAILS:
		sci_details_pressed((void *) 0);
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
		comms_screen_button_pressed((void *) 7);
		break;
	case DEVIO_OPCODE_COMMS_TRANSMIT:
		comms_transmit_button_pressed((void *) 0);
		break;
	case DEVIO_OPCODE_COMMS_RED_ALERT:
		comms_screen_red_alert_pressed((void *) 0);
		break;
	case DEVIO_OPCODE_COMMS_MAINSCREEN_COMMS:
		comms_main_screen_pressed((void *) 8);
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
		fprintf(stderr, "Failed to bind to linux abstrack socket address 'snis-phys-io'.\n");
		fprintf(stderr, "Perhaps another snis_client process is running.\n");
		/* FIXME: is this how to deal with the socket at this point? */
		physical_io_socket = -1;
	} else {
		rc = pthread_create(&physical_io_thread, &physical_io_thread_attr,
			monitor_physical_io_devices, NULL);
		if (rc) {
			fprintf(stderr, "Failed to create physical device monitor thread.\n");
			fprintf(stderr, "Physical input devices will not work.\n");
			close(physical_io_socket);
			physical_io_socket = -1;
		}
	}
#endif
}

static void setup_joystick(GtkWidget *window)
{
	strcpy(joystick_device, JOYSTICK_DEVNAME);
	joystickx = 0;
	joysticky = 0;
	set_joystick_x_axis(joystickx);
	set_joystick_x_axis(joysticky);

	joystick_fd = open_joystick(joystick_device, window->window);
	if (joystick_fd < 0)
                printf("No joystick...\n");
        else {
		/* pull all the events off the joystick */
		memset(&jse.button, 0, sizeof(jse.button));
		int rc = get_joystick_status(&jse);
		if (rc != 0) {
			printf("Joystick '%s' not sending events, ignoring...\n", joystick_device);
			joystick_fd = -1;
			return;
		}

		if ( jse.stick_x <= -32767 || jse.stick_y <= -32767) {
			printf("Joystick '%s' stuck at neg limits, ignoring...\n", joystick_device);
			joystick_fd = -1;
			return;
		}

                check_for_screensaver();
	}
}

static struct mesh *snis_read_model(char *directory, char *filename)
{
	char path[PATH_MAX];
	int l = strlen(filename);

	sprintf(path, "%s/models/%s", directory, filename);
	if (strcasecmp(&filename[l - 3], "obj") == 0)
		return read_obj_file(path);
	else if (strcasecmp(&filename[l - 3], "stl") == 0)
		return read_stl_file(path);
	else {
		printf("bad path '%s', filename='%s', filename[l - 3] = '%s'\n",
			path, filename, &filename[l - 4]);
		return NULL;
	}
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

	mt = mtwist_init(mtwist_seed);
	if (!mt) {
		fprintf(stderr, "out of memory at %s:%d... crash likely.\n",
				__FILE__, __LINE__);
		fflush(stderr);
		return;
	}

	ship_turret_mesh = snis_read_model(d, "spaceship_turret.stl");
	ship_turret_base_mesh = snis_read_model(d, "spaceship_turret_base.stl");
	torpedo_nav_mesh = snis_read_model(d, "torpedo.stl");
#ifndef WITHOUTOPENGL
	torpedo_mesh = mesh_fabricate_billboard(0, 0, 50.0f, 50.0f);
#else
	torpedo_mesh = torpedo_nav_mesh;
#endif
	laser_mesh = snis_read_model(d, "laser.stl");

	for (i = 0; i < NASTEROID_MODELS; i++) {
		char filename[100];

		if (i == 0)
			sprintf(filename, "asteroid.stl");
		else
			sprintf(filename, "asteroid%d.stl", i + 1);
		printf("reading '%s'\n", filename);
		asteroid_mesh[i] = snis_read_model(d, filename);
		mesh_distort(asteroid_mesh[i], 0.10);
	}

	sphere_mesh = mesh_unit_icosphere(4);
	planetary_ring_mesh = mesh_fabricate_planetary_ring(2.0, 3.0);

	for (i = 0; i < NSTARBASE_MODELS; i++) {
		char filename[100];

		if (i == 0)
			sprintf(filename, "starbase/starbase.obj");
		else if (i == 1)
			sprintf(filename, "starbase%d/starbase%d.obj", i + 1, i + 1);
		else
			sprintf(filename, "starbase%d.stl", i + 1);
		printf("reading '%s'\n", filename);
		starbase_mesh[i] = snis_read_model(d, filename);
	}

	for (i = 0; i < nshiptypes; i++) {
		ship_mesh_map[i] = snis_read_model(d, ship_type[i].model_file);
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
		derelict_mesh[i] = make_derelict_mesh(ship_mesh_map[i]);
	}

#ifndef WITHOUTOPENGL
	particle_mesh = mesh_fabricate_billboard(0, 0, 50.0f, 50.0f);
#else
	particle_mesh = snis_read_model(d, "tetrahedron.stl");
#endif
	debris_mesh = snis_read_model(d, "flat-tetrahedron.stl");
	debris2_mesh = snis_read_model(d, "big-flat-tetrahedron.stl");
	wormhole_mesh = snis_read_model(d, "wormhole.stl");
	mesh_map_xy_to_uv(wormhole_mesh);
#ifdef WITHOUTOPENGL
	mesh_distort(wormhole_mesh, 0.15);
	wormhole_mesh->geometry_mode = MESH_GEOMETRY_POINTS;
#else
	mesh_scale(wormhole_mesh, 3.0f);
#endif
	spacemonster_mesh = snis_read_model(d, "spacemonster.stl");
	spacemonster_mesh->geometry_mode = MESH_GEOMETRY_POINTS;
	laserbeam_nav_mesh = snis_read_model(d, "long-triangular-prism.stl");
#ifndef WITHOUTOPENGL
	laserbeam_mesh = mesh_fabricate_billboard(0, 0, 200, 5);
	phaser_mesh = init_burst_rod_mesh(1000, LASER_VELOCITY * 2 / 3, 0.5, 0.5);
#else
	laserbeam_mesh = laserbeam_nav_mesh;
	phaser_mesh = laserbeam_nav_mesh;
#endif
	ship_icon_mesh = snis_read_model(d, "ship-icon.stl");
	heading_indicator_mesh = snis_read_model(d, "heading_indicator.stl");
	cargo_container_mesh = snis_read_model(d, "cargocontainer/cargocontainer.obj");
	nebula_mesh = mesh_fabricate_billboard(0, 0, 2, 2);
	sun_mesh = mesh_fabricate_billboard(0, 0, 30000, 30000);
	thrust_animation_mesh = init_thrust_mesh(10, 7, 3, 1);

	mtwist_free(mt);
}

static void init_vects(void)
{
	int i;

	setup_vect(snis_logo, snis_logo_points);
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

int main(int argc, char *argv[])
{
	GtkWidget *vbox;
	int i;

	/* Need this so that fscanf reads floats properly. */
#define LOCALE_THAT_WORKS "en_US.UTF-8"
	if (setenv("LANG", LOCALE_THAT_WORKS, 1) <  0)
		fprintf(stderr, "Failed to setenv LANG to '%s'\n",
			LOCALE_THAT_WORKS);

	set_random_seed();

	displaymode = DISPLAYMODE_NETWORK_SETUP;

	/* *Sigh*  why am I too lazy to use getopt,
	 * but not too lazy for this crap?
	 */

	role = 0;
	for (i = 1; i < argc; i++) {
		if (strcmp(argv[i], "--version") == 0) {
			printf("snis_client v. %s\n", SNIS_VERSION);
			continue;
		}
		if (strcmp(argv[i], "--quickstart") == 0) {
			quickstartmode = 1;
			continue;
		}
		if (strcmp(argv[i], "--lobbyhost") == 0) {
			if ((i + 1) >= argc)
				usage();
			lobbyhost = argv[i + 1];
			i++;
			continue;
		}
		if (strcmp(argv[i], "--pw") == 0) {
			if ((i + 1) >= argc)
				usage();
			password = argv[i + 1];
			i++;
			continue;
		}
		if (strcmp(argv[i], "--starship") == 0) {
			if ((i + 1) >= argc)
				usage();
			shipname = argv[i + 1];
			i++;
			continue;
		}
		if (strcmp(argv[i], "--allroles") == 0) {
			role |= ROLE_ALL;
			continue;
		}
		if (strcmp(argv[i], "--main") == 0) {
			role |= ROLE_MAIN;
			continue;
		}
		if (strcmp(argv[i], "--navigation") == 0) {
			role |= ROLE_NAVIGATION;
			continue;
		}
		if (strcmp(argv[i], "--weapons") == 0) {
			role |= ROLE_WEAPONS;
			continue;
		}
		if (strcmp(argv[i], "--engineering") == 0) {
			role |= ROLE_ENGINEERING;
			continue;
		}
		if (strcmp(argv[i], "--science") == 0) {
			role |= ROLE_SCIENCE;
			continue;
		}
		if (strcmp(argv[i], "--comms") == 0) {
			role |= ROLE_COMMS;
			continue;
		}
		if (strcmp(argv[i], "--soundserver") == 0) {
			role |= ROLE_SOUNDSERVER;
			continue;
		}
		if (strcmp(argv[i], "--fullscreen") == 0) {
			fullscreen = 1;
			continue;
		}
		usage();
	}
	if (role == 0)
		role = ROLE_ALL;

	strcpy(skybox_texture_prefix, "orange-haze");
	override_asset_dir();

	memset(&main_screen_text, 0, sizeof(main_screen_text));
	snis_object_pool_setup(&pool, MAXGAMEOBJS);
	snis_object_pool_setup(&sparkpool, MAXSPARKS);
	snis_object_pool_setup(&damcon_pool, MAXDAMCONENTITIES);
	memset(dco, 0, sizeof(dco));
	damconscreenx = NULL;
	damconscreeny = NULL;

	ignore_sigpipe();
	prevent_zombies();

	setup_sound();
	if (read_ship_types()) {
                fprintf(stderr, "%s: unable to read ship types\n", argv[0]);
                return -1;
        }

	if (read_factions())
		return -1;

	ship_mesh_map = malloc(sizeof(*ship_mesh_map) * nshiptypes);
	derelict_mesh = malloc(sizeof(*derelict_mesh) * nshiptypes);
	if (!ship_mesh_map || !derelict_mesh) {
		fprintf(stderr, "out of memory at %s:%d\n", __FILE__, __LINE__);
		exit(1);
	}
	memset(ship_mesh_map, 0, sizeof(*ship_mesh_map) * nshiptypes);
	memset(derelict_mesh, 0, sizeof(*derelict_mesh) * nshiptypes);

	if (displaymode != DISPLAYMODE_NETWORK_SETUP || quickstartmode) {
		connect_to_lobby();
		if (quickstartmode)
			displaymode = DISPLAYMODE_LOBBYSCREEN;
	}

	real_screen_width = SCREEN_WIDTH;
	real_screen_height = SCREEN_HEIGHT;
	sng_set_extent_size(SCREEN_WIDTH, SCREEN_HEIGHT);
	sng_set_screen_size(real_screen_width, real_screen_height);

	gtk_set_locale();
	gtk_init (&argc, &argv);

	init_keymap();
	read_keymap_config_file();
	init_vects();
	initialize_random_orientations_and_spins();
#if 0
	init_player();
	init_game_state(the_player);
#endif

	snis_typefaces_init();

	window = gtk_window_new (GTK_WINDOW_TOPLEVEL);		
	gtk_container_set_border_width (GTK_CONTAINER (window), 0);
	vbox = gtk_vbox_new(FALSE, 0);
        main_da = gtk_drawing_area_new();

	g_signal_connect (G_OBJECT (window), "delete_event",
		G_CALLBACK (delete_event), NULL);
	g_signal_connect (G_OBJECT (window), "destroy",
		G_CALLBACK (destroy), NULL);
	g_signal_connect(G_OBJECT (window), "key_press_event",
		G_CALLBACK (key_press_cb), "window");
	g_signal_connect(G_OBJECT (window), "key_release_event",
		G_CALLBACK (key_release_cb), "window");
	g_signal_connect(G_OBJECT (main_da), "expose_event",
		G_CALLBACK (main_da_expose), NULL);
        g_signal_connect(G_OBJECT (main_da), "configure_event",
		G_CALLBACK (main_da_configure), NULL);
        g_signal_connect(G_OBJECT (main_da), "scroll_event",
		G_CALLBACK (main_da_scroll), NULL);
	gtk_widget_add_events(main_da, GDK_BUTTON_PRESS_MASK);
	gtk_widget_add_events(main_da, GDK_BUTTON_RELEASE_MASK);
	gtk_widget_add_events(main_da, GDK_BUTTON3_MOTION_MASK);
	gtk_widget_add_events(main_da, GDK_POINTER_MOTION_MASK);
	g_signal_connect(G_OBJECT (main_da), "button_press_event",
                      G_CALLBACK (main_da_button_press), NULL);
	g_signal_connect(G_OBJECT (main_da), "button_release_event",
                      G_CALLBACK (main_da_button_release), NULL);
	g_signal_connect(G_OBJECT (main_da), "motion_notify_event",
                      G_CALLBACK (main_da_motion_notify), NULL);

	gtk_container_add (GTK_CONTAINER (window), vbox);
	gtk_box_pack_start(GTK_BOX (vbox), main_da, TRUE /* expand */, TRUE /* fill */, 0);

        gtk_window_set_default_size(GTK_WINDOW(window), real_screen_width, real_screen_height);

	init_gl(argc, argv, main_da);
        gtk_widget_show (vbox);
        gtk_widget_show (main_da);
        gtk_widget_show (window);

	sng_setup_colors(main_da);

#ifndef WITHOUTOPENGL
	/* must be set or the window will not show */
	GdkColor black = { 0, 0, 0, 0 };
	gtk_widget_modify_bg(main_da, GTK_STATE_NORMAL, &black);
#endif

        gc = gdk_gc_new(GTK_WIDGET(main_da)->window);
	sng_set_context(GTK_WIDGET(main_da)->window, gc);
	sng_set_foreground(WHITE);

	timer_tag = g_timeout_add(1000 / frame_rate_hz, advance_game, NULL);

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
	text_window_set_chatter_sound(TTY_CHATTER_SOUND);
	text_window_set_timer(&timer);
	init_lobby_ui();
	init_nav_ui();
	init_engineering_ui();
	init_damcon_ui();
	init_weapons_ui();
	init_science_ui();
	init_comms_ui();
	init_demon_ui();
	init_net_setup_ui();
	setup_joystick(window);
	setup_physical_io_socket();
	ecx = entity_context_new(5000, 1000);

	snis_protocol_debugging(1);

	set_default_clip_window();

	if (fullscreen)
		gtk_window_fullscreen(GTK_WINDOW(window));

	gtk_main ();
        wwviaudio_cancel_all_sounds();
        wwviaudio_stop_portaudio();
	close_joystick();
	return 0;
}
