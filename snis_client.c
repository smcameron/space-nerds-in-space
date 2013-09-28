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
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <sys/types.h>
#include <stdint.h>
#include <malloc.h>
#include <gtk/gtk.h>

#ifndef WITHOUTOPENGL
#include <gtk/gtkgl.h>
#include <GL/gl.h>
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
#include <sys/socket.h>
#include <netdb.h>
#include <assert.h>
#include <linux/tcp.h>

#include "space-part.h"
#include "snis.h"
#include "mathutils.h"
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

#include "vertex.h"
#include "triangle.h"
#include "mesh.h"
#include "stl_parser.h"
#include "entity.h"

#define SHIP_COLOR CYAN
#define STARBASE_COLOR RED
#define WORMHOLE_COLOR WHITE
#define PLANET_COLOR GREEN
#define ASTEROID_COLOR AMBER
#define DERELICT_COLOR BLUE 
#define PARTICLE_COLOR YELLOW
#define LASER_COLOR GREEN
#define PLAYER_LASER_COLOR GREEN
#define NPC_LASER_COLOR ORANGERED
#define TORPEDO_COLOR RED
#define SPACEMONSTER_COLOR GREEN
#define NEBULA_COLOR MAGENTA
#define TRACTORBEAM_COLOR BLUE

#define ARRAYSIZE(x) (sizeof(x) / sizeof((x)[0]))

#define SCREEN_WIDTH 800        /* window width, in pixels */
#define SCREEN_HEIGHT 600       /* window height, in pixels */

__attribute__((unused)) static double max_speed[];

typedef void explosion_function(int x, int y, int ivx, int ivy, int v, int nsparks, int time);

explosion_function *explosion = NULL;

/* I can switch out the line drawing function with these macros */
/* in case I come across something faster than gdk_draw_line */
#define DEFAULT_LINE_STYLE sng_current_draw_line
#define DEFAULT_RECTANGLE_STYLE sng_current_draw_rectangle
#define DEFAULT_BRIGHT_LINE_STYLE sng_current_bright_line
#define DEFAULT_DRAW_ARC sng_current_draw_arc

#define snis_draw_line DEFAULT_LINE_STYLE
#define snis_draw_rectangle DEFAULT_RECTANGLE_STYLE
#define snis_bright_line DEFAULT_BRIGHT_LINE_STYLE
#define snis_draw_arc DEFAULT_DRAW_ARC
int thicklines = 0;
int frame_rate_hz = 30;
int red_alert_mode = 0;

typedef void (*joystick_button_fn)(void *x);
char joystick_device[PATH_MAX+1];
int joystick_fd = -1;
int joystickx, joysticky;

GtkWidget *window;
GdkGC *gc = NULL;               /* our graphics context. */
GtkWidget *main_da;             /* main drawing area. */
gint timer_tag;  
int fullscreen = 0;
int in_the_process_of_quitting = 0;
int current_quit_selection = 0;
int final_quit_selection = 0;

uint32_t role = ROLE_ALL;
char *password;
char *shipname;
#define UNKNOWN_ID 0xffffffff
uint32_t my_ship_id = UNKNOWN_ID;
uint32_t my_ship_oid = UNKNOWN_ID;

float xscale_screen = 1.0;
float yscale_screen= 1.0;
int real_screen_width;
int real_screen_height;
int warp_limbo_countdown = 0;
int damage_limbo_countdown = 0;

struct entity_context *ecx;
struct entity_context *sciecx;

struct nebula_entry {
	double x, y, r, r2;
	uint32_t id;
} nebulaentry[NNEBULA];
int nnebula;

static volatile int displaymode = DISPLAYMODE_LOBBYSCREEN;
static volatile int helpmode = 0;
static volatile int helpmodeline = 0;

struct client_network_stats {
	uint64_t bytes_sent;
	uint64_t bytes_recd;
	uint32_t elapsed_seconds;
} netstats;

int nframes = 0;
int timer = 0;
struct timeval start_time, end_time;

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
int texture[MAXTEXTURES];
int ntextures = 0;

double sine[361];
double cosine[361];

struct mesh *torpedo_mesh;
struct mesh *laser_mesh;
struct mesh *asteroid_mesh[NASTEROID_MODELS * NASTEROID_SCALES];
struct mesh *planet_mesh[NPLANET_MODELS];
struct mesh *starbase_mesh[NSTARBASE_MODELS];
struct mesh *ship_mesh;
struct mesh *freighter_mesh;
struct mesh *cruiser_mesh;
struct mesh *tanker_mesh;
struct mesh *destroyer_mesh;
struct mesh *transport_mesh;
struct mesh *dragonhawk_mesh;
struct mesh *skorpio_mesh;
struct mesh *disruptor_mesh;
struct mesh *research_vessel_mesh;
struct mesh *battlestar_mesh;
struct mesh *particle_mesh;
struct mesh *debris_mesh;
struct mesh *debris2_mesh;
struct mesh *wormhole_mesh;
struct mesh *spacemonster_mesh;
struct mesh *asteroidminer_mesh;
struct mesh *spaceship2_mesh;
struct mesh *scout_mesh;
struct mesh *laserbeam_mesh;

struct mesh *derelict_mesh[NDERELICT_MESHES];

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

void init_trig_arrays(void)
{
	int i;

	for (i = 0; i <= 360; i++) {
		sine[i] = sin((double)i * M_PI * 2.0 / 360.0);
		cosine[i] = cos((double)i * M_PI * 2.0 / 360.0);
	}
}

static void set_default_clip_window(void)
{
	sng_set_clip_window(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
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
static struct snis_damcon_entity dco[MAXDAMCONENTITIES];
static struct snis_object_pool *sparkpool;
static struct snis_entity spark[MAXSPARKS];
static pthread_mutex_t universe_mutex = PTHREAD_MUTEX_INITIALIZER;

static int add_generic_object(uint32_t id, double x, double y, double vx, double vy, double heading, int type, uint32_t alive, struct entity *entity)
{
	int i;

	i = snis_object_pool_alloc_obj(pool); 	 
	if (i < 0) {
		printf("snis_object_pool_alloc_obj failed\n");
		return -1;
	}
	memset(&go[i], 0, sizeof(go[i]));
	go[i].index = i;
	go[i].id = id;
	go[i].x = x;
	go[i].y = y;
	go[i].z = 0.0;
	go[i].vx = vx;
	go[i].vy = vy;
	go[i].heading = heading;
	go[i].type = type;
	go[i].alive = alive;
	go[i].entity = entity;
	return i;
}

static void update_generic_object(int index, double x, double y, double z,
				double vx, double vy, double heading, uint32_t alive)
{
	struct snis_entity *o = &go[index];

	o->x = x;
	o->y = y;
	o->z = z;
	o->vx = vx;
	o->vy = vy;
	o->heading = heading;
	o->alive = alive;
	if (o->entity) {
		update_entity_pos(o->entity, x, z, -y);
		update_entity_rotation(o->entity, M_PI / 2.0,
			heading + M_PI - (o->type == OBJTYPE_SHIP1) * (M_PI / 2.0), 0);
	}
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
	o->index = i;
	o->id = id;
	o->ship_id = ship_id;
	update_generic_damcon_object(o, x, y, velocity, heading);
	return i;
}

static int update_damcon_object(uint32_t id, uint32_t ship_id, uint32_t type,
			double x, double y, double velocity,
			double heading)

{
	int i;
	struct snis_damcon_entity *o;

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
		}
		return 0;
	}
	o = &dco[i];
	update_generic_damcon_object(o, x, y, velocity, heading);
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


static int update_econ_ship(uint32_t id, double x, double y, double z, double vx,
			double vy, double heading, uint32_t alive, uint32_t victim_id,
			uint8_t shiptype)
{
	int i;
	struct entity *e;

	i = lookup_object_by_id(id);
	if (i < 0) {
		switch (shiptype) {
		case SHIP_CLASS_CRUISER:
			e = add_entity(ecx, cruiser_mesh, x, z, -y, SHIP_COLOR);
			break;
		case SHIP_CLASS_DESTROYER:
			e = add_entity(ecx, destroyer_mesh, x, z, -y, SHIP_COLOR);
			break;
		case SHIP_CLASS_TRANSPORT:
			e = add_entity(ecx, transport_mesh, x, z, -y, SHIP_COLOR);
			break;
		case SHIP_CLASS_DRAGONHAWK:
			e = add_entity(ecx, dragonhawk_mesh, x, z, -y, SHIP_COLOR);
			break;
		case SHIP_CLASS_SKORPIO:
			e = add_entity(ecx, skorpio_mesh, x, z, -y, SHIP_COLOR);
			break;
		case SHIP_CLASS_DISRUPTOR:
			e = add_entity(ecx, disruptor_mesh, x, z, -y, SHIP_COLOR);
			break;
		case SHIP_CLASS_RESEARCH_VESSEL:
			e = add_entity(ecx, research_vessel_mesh, x, z, -y, SHIP_COLOR);
			break;
		case SHIP_CLASS_FREIGHTER:
			e = add_entity(ecx, freighter_mesh, x, z, -y, SHIP_COLOR);
			break;
		case SHIP_CLASS_TANKER:
			e = add_entity(ecx, tanker_mesh, x, z, -y, SHIP_COLOR);
			break;
		case SHIP_CLASS_BATTLESTAR:
			e = add_entity(ecx, battlestar_mesh, x, z, -y, SHIP_COLOR);
			break;
		case SHIP_CLASS_ASTEROIDMINER:
			e = add_entity(ecx, asteroidminer_mesh, x, z, -y, SHIP_COLOR);
			break;
		case SHIP_CLASS_SCOUT:
			e = add_entity(ecx, scout_mesh, x, z, -y, SHIP_COLOR);
			break;
		case SHIP_CLASS_SCIENCE:
			e = add_entity(ecx, spaceship2_mesh, x, z, -y, SHIP_COLOR);
			break;
		case SHIP_CLASS_STARSHIP:
			e = add_entity(ecx, ship_mesh, x, z, -y, SHIP_COLOR);
			break;
		default:
			e = add_entity(ecx, cruiser_mesh, x, z, -y, SHIP_COLOR);
			break;
		}
		i = add_generic_object(id, x, y, vx, vy, heading, OBJTYPE_SHIP2, alive, e);
		if (i < 0)
			return i;
		go[i].entity = e;
	} else {
		update_generic_object(i, x, y, z, vx, vy, heading, alive); 
	}
	go[i].z = z;
	go[i].tsd.ship.victim_id = (int32_t) victim_id;
	go[i].tsd.ship.shiptype = shiptype;
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

static int update_ship(uint32_t id, double x, double y, double z, double vx, double vy, double heading, uint32_t alive,
			uint32_t torpedoes, uint32_t power, 
			double gun_heading, double sci_heading, double sci_beam_width, int type,
			uint8_t tloading, uint8_t tloaded, uint8_t throttle, uint8_t rpm, uint32_t
			fuel, uint8_t temp, uint8_t scizoom, uint8_t weapzoom,
			uint8_t navzoom, uint8_t warpdrive,
			uint8_t requested_warpdrive, uint8_t requested_shield,
			uint8_t phaser_charge, uint8_t phaser_wavelength, uint8_t shiptype,
			uint8_t reverse, uint32_t victim_id)
{
	int i;
	struct entity *e;

	i = lookup_object_by_id(id);
	if (i < 0) {
		switch (shiptype) {
		case SHIP_CLASS_FREIGHTER:
			e = add_entity(ecx, freighter_mesh, x, z, -y, SHIP_COLOR);
			break;
		default:
			if (id == my_ship_id)	
				e = add_entity(ecx, NULL, x, z, -y, SHIP_COLOR);
			else
				e = add_entity(ecx, ship_mesh, x, z, -y, SHIP_COLOR);
			break;
		}
		i = add_generic_object(id, x, y, vx, vy, heading, type, alive, e);
		if (i < 0)
			return i;
	} else {
		update_generic_object(i, x, y, z, vx, vy, heading, alive); 
	}
	go[i].z = z;
	go[i].tsd.ship.torpedoes = torpedoes;
	go[i].tsd.ship.power = power;
	go[i].tsd.ship.gun_heading = gun_heading;
	go[i].tsd.ship.sci_heading = sci_heading;
	go[i].tsd.ship.sci_beam_width = sci_beam_width;
	go[i].tsd.ship.torpedoes_loaded = tloaded;
	go[i].tsd.ship.torpedoes_loading = tloading;
	go[i].tsd.ship.throttle = throttle;
	go[i].tsd.ship.rpm = rpm;
	go[i].tsd.ship.fuel = fuel;
	go[i].tsd.ship.temp = temp;
	go[i].tsd.ship.scizoom = scizoom;
	go[i].tsd.ship.weapzoom = weapzoom;
	go[i].tsd.ship.navzoom = navzoom;
	go[i].tsd.ship.requested_warpdrive = requested_warpdrive;
	go[i].tsd.ship.requested_shield = requested_shield;
	go[i].tsd.ship.warpdrive = warpdrive;
	go[i].tsd.ship.phaser_charge = phaser_charge;
	go[i].tsd.ship.phaser_wavelength = phaser_wavelength;
	go[i].tsd.ship.damcon = NULL;
	go[i].tsd.ship.shiptype = shiptype;
	go[i].tsd.ship.reverse = reverse;
	go[i].tsd.ship.victim_id = victim_id;
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
		
	go[i].sdata.science_data_requested = 0; /* request is fullfilled */
	return 0;
}

static int update_torpedo(uint32_t id, double x, double y, double z,
			double vx, double vy, uint32_t ship_id)
{
	int i;
	struct entity *e;
	i = lookup_object_by_id(id);
	if (i < 0) {
		e = add_entity(ecx, torpedo_mesh, x, z, -y, TORPEDO_COLOR);
		set_render_style(e, RENDER_WIREFRAME | RENDER_BRIGHT_LINE);
		i = add_generic_object(id, x, y, vx, vy, 0.0, OBJTYPE_TORPEDO, 1, e);
		if (i < 0)
			return i;
		go[i].tsd.torpedo.ship_id = ship_id;
	} else {
		update_generic_object(i, x, y, z, vx, vy, 0.0, 1); 
		update_entity_pos(go[i].entity, x, z, -y);
	}
	return 0;
}

static void init_laserbeam_data(struct snis_entity *o);
static void update_laserbeam_segments(struct snis_entity *o);
static int update_laserbeam(uint32_t id, uint32_t origin, uint32_t target)
{
	int i;

	i = lookup_object_by_id(id);
	if (i < 0) {
		i = add_generic_object(id, 0, 0, 0, 0, 0.0, OBJTYPE_LASERBEAM, 1, NULL);
		if (i < 0)
			return i;
		go[i].tsd.laserbeam.origin = origin;
		go[i].tsd.laserbeam.target = target;
		init_laserbeam_data(&go[i]);
		go[i].move = update_laserbeam_segments;
	} /* nothing to do */
	return 0;
}

static int update_tractorbeam(uint32_t id, uint32_t origin, uint32_t target)
{
	int i;

	i = lookup_object_by_id(id);
	if (i < 0) {
		i = add_generic_object(id, 0, 0, 0, 0, 0.0, OBJTYPE_TRACTORBEAM, 1, NULL);
		if (i < 0)
			return i;
		go[i].tsd.laserbeam.origin = origin;
		go[i].tsd.laserbeam.target = target;
		init_laserbeam_data(&go[i]);
		go[i].move = update_laserbeam_segments;
	} /* nothing to do */
	return 0;
}


static int update_laser(uint32_t id, double x, double y, double z,
			double vx, double vy, uint32_t ship_id)
{
	int i;
	struct entity *e;

	i = lookup_object_by_id(id);
	if (i < 0) {
		e = add_entity(ecx, laser_mesh, x, z, -y, LASER_COLOR);
		set_render_style(e, RENDER_WIREFRAME | RENDER_BRIGHT_LINE);
		i = add_generic_object(id, x, y, vx, vy, 0.0, OBJTYPE_LASER, 1, e);
		if (i < 0)
			return i;
		go[i].tsd.laser.ship_id = ship_id;
	} else {
		update_generic_object(i, x, y, z, vx, vy, 0.0, 1); 
	}
	return 0;
}

static void init_spacemonster_data(struct snis_entity *o, double z)
{
	int i;
	struct spacemonster_data *sd = &o->tsd.spacemonster;

	sd->zz = z;
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
		sd->y[i] = o->y;
		sd->z[i] = 0.0;
		sd->entity[i] = add_entity(ecx, spacemonster_mesh, o->x, 0, -o->y,
						SPACEMONSTER_COLOR);
		set_render_style(sd->entity[i], RENDER_POINT_CLOUD | RENDER_SPARKLE);
	}
		
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
	double rx, ry, rz;
	double angle;

	oid = lookup_object_by_id(o->tsd.laserbeam.origin);
	tid = lookup_object_by_id(o->tsd.laserbeam.target);

	if (oid < 0 || tid < 0)
		return;
	origin = &go[oid];
	target = &go[tid];

	x1 = origin->x;
	y1 = origin->y;
	z1 = origin->z;
	x2 = target->x;
	y2 = target->y;
	z2 = target->z;

	angle = atan2(y2 - y1, x2 - x1);

	x1 += cos(angle) * 5.0;
	y1 -= sin(angle) * 5.0;
	x2 -= cos(angle) * 5.0;
	y2 += sin(angle) * 5.0;

	dx = (x2 - x1) / MAX_LASERBEAM_SEGMENTS;
	dy = (y2 - y1) / MAX_LASERBEAM_SEGMENTS;
	dz = (z2 - z1) / MAX_LASERBEAM_SEGMENTS;

	
	for (i = 0; i < MAX_LASERBEAM_SEGMENTS; i++) {
		lastd = (snis_randn(50) - 25) / 100.0;
#if 0
		rx = snis_randn(360) * M_PI / 180.0;
		ry = snis_randn(360) * M_PI / 180.0;
		rz = snis_randn(360) * M_PI / 180.0;
		rx = M_PI / 2.0;
		ry = o->heading + M_PI;
		rz = 0;
#endif
		rx = 0;
		ry = angle;
		rz = 0;
		ld->x[i] = x1 + (i + lastd) * dx;
		ld->y[i] = y1 + (i + lastd) * dy;
		ld->z[i] = z1 + (i + lastd) * dz; 
		update_entity_pos(ld->entity[i], ld->x[i], ld->z[i], -ld->y[i]);
		update_entity_rotation(ld->entity[i], rx, ry, rz);
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
		set_render_style(ld->entity[i], RENDER_WIREFRAME | RENDER_BRIGHT_LINE | RENDER_NO_FILL);
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
			remove_entity(ecx, ld->entity[i]);
		free(ld->entity);
		ld->entity = NULL;
	}
}

static int update_spacemonster(uint32_t id, double x, double y, double z)
{
	int i;
	struct entity *e;

	i = lookup_object_by_id(id);
	if (i < 0) {
		e = add_entity(ecx, spacemonster_mesh, x, 0, -y, SPACEMONSTER_COLOR);
		set_render_style(e, RENDER_POINT_CLOUD | RENDER_SPARKLE);
		i = add_generic_object(id, x, y, 0, 0, 0.0, OBJTYPE_SPACEMONSTER, 1, e);
		if (i < 0)
			return i;
		go[i].entity = e;
		init_spacemonster_data(&go[i], z);
	} else {
		struct spacemonster_data *sd;
		int n;

		update_generic_object(i, x, y, 0, 0, 0, 0.0, 1); 
		update_entity_pos(go[i].entity, x, z, -y);
		sd = &go[i].tsd.spacemonster;
		sd->zz = z;
		n = (sd->front + 1) % MAX_SPACEMONSTER_SEGMENTS;
		sd->front = n;
		sd->x[n] = x;
		sd->y[n] = y;
		sd->z[n] = z;
		update_entity_pos(sd->entity[sd->front], x, z, -y);
	}
	return 0;
}

static int update_asteroid(uint32_t id, double x, double y, double z)
{
	int i, m;
	struct entity *e;

	i = lookup_object_by_id(id);
	if (i < 0) {
		m = id % (NASTEROID_MODELS * NASTEROID_SCALES);
		e = add_entity(ecx, asteroid_mesh[m], x, z, -y, ASTEROID_COLOR);
		i = add_generic_object(id, x, y, 0.0, 0.0, 0.0, OBJTYPE_ASTEROID, 1, e);
		if (i < 0)
			return i;
		go[i].z = z;
	} else {
		int axis;
		float angle;

		update_generic_object(i, x, y, z, 0.0, 0.0, 0.0, 1);
		update_entity_pos(go[i].entity, x, z, -y);

		/* make asteroids spin */
		angle = (timer % (360 * ((id % 12) + 3))) * M_PI / 180.0;
		axis = (id % 3);
		update_entity_rotation(go[i].entity, (axis == 0) * angle,
					(axis == 1) * angle, (axis == 2) * angle);
	}
	return 0;
}

static int update_derelict(uint32_t id, double x, double y, double z, uint8_t ship_type)
{
	int i, m;
	struct entity *e;

	i = lookup_object_by_id(id);
	if (i < 0) {
		m = ship_type % NDERELICT_MESHES;
		e = add_entity(ecx, derelict_mesh[m], x, z, -y, SHIP_COLOR);
		i = add_generic_object(id, x, y, 0.0, 0.0, 0.0, OBJTYPE_DERELICT, 1, e);
		if (i < 0)
			return i;
		go[i].z = z;
	} else {
		int axis;
		float angle;

		update_generic_object(i, x, y, z, 0.0, 0.0, 0.0, 1);
		update_entity_pos(go[i].entity, x, z, -y);

		/* make it spin */
		angle = (timer % (360 * ((id % 12) + 3))) * M_PI / 180.0;
		axis = (id % 3);
		update_entity_rotation(go[i].entity, (axis == 0) * angle,
					(axis == 1) * angle, (axis == 2) * angle);
	}
	return 0;
}

static int update_planet(uint32_t id, double x, double y, double z)
{
	int i, m;
	struct entity *e;

	i = lookup_object_by_id(id);
	if (i < 0) {
		int axis;
		float angle;

		m = id % NPLANET_MODELS;
		e = add_entity(ecx, planet_mesh[m], x, z, -y, PLANET_COLOR);
		i = add_generic_object(id, x, y, 0.0, 0.0, 0.0, OBJTYPE_PLANET, 1, e);
		if (i < 0)
			return i;
		go[i].z = z;
		angle = (20 % (360 * ((id % 12) + 3))) * M_PI / 180.0;
		axis = (id % 3);
		update_entity_rotation(go[i].entity, (axis == 0) * angle,
					(axis == 1) * angle, (axis == 2) * angle);
	} else {

		update_generic_object(i, x, y, z, 0.0, 0.0, 0.0, 1);
		update_entity_pos(go[i].entity, x, z, -y);
	}
	return 0;
}

static int update_wormhole(uint32_t id, double x, double y)
{
	int i;
	struct entity *e;
	struct snis_entity *o;

	i = lookup_object_by_id(id);
	if (i < 0) {
		e = add_entity(ecx, wormhole_mesh, x, 0, -y, WORMHOLE_COLOR);
		set_render_style(e, RENDER_POINT_CLOUD | RENDER_SPARKLE);
		i = add_generic_object(id, x, y, 0.0, 0.0, 0.0, OBJTYPE_WORMHOLE, 1, e);
		if (i < 0)
			return i;
	} else {
		o = &go[i];
		/* Don't call update_generic_object as it will reset orientation
		 * and orientation is handled client side via spin_starbase
		 */
		o->x = x;
		o->y = y;
		if (o->entity)
			update_entity_pos(o->entity, x, 0, -y);
	}
	return 0;
}

static int update_starbase(uint32_t id, double x, double y)
{
	int i, m;
	struct entity *e;
	struct snis_entity *o;

	i = lookup_object_by_id(id);
	if (i < 0) {
		m = id % NSTARBASE_MODELS;
		e = add_entity(ecx, starbase_mesh[m], x, 0, -y, STARBASE_COLOR);
		i = add_generic_object(id, x, y, 0.0, 0.0, 0.0, OBJTYPE_STARBASE, 1, e);
		if (i < 0)
			return i;
	} else {
		o = &go[i];
		/* Don't call update_generic_object as it will reset orientation
		 * and orientation is handled client side via spin_starbase
		 */
		o->x = x;
		o->y = y;
		if (o->entity)
			update_entity_pos(o->entity, x, 0, -y);
	}
	return 0;
}

static void add_nebula_entry(uint32_t id, double x, double y, double r)
{
	if (nnebula >= NNEBULA) {
		printf("Bug at %s:%d\n", __FILE__, __LINE__);
		return;
	}
	nebulaentry[nnebula].id = id;
	nebulaentry[nnebula].x = x;
	nebulaentry[nnebula].y = y;
	nebulaentry[nnebula].r2 = r * r;
	nebulaentry[nnebula].r = r;
	nnebula++;
}

static void delete_nebula_entry(uint32_t id)
{
	int i;

	for (i = 0; i < nnebula; i++) {
		if (nebulaentry[i].id == id)
			break;
	}
	if (i >= nnebula)
		return;

	nnebula--;
	if (i == nnebula - 1) {
		return;
	}
	memmove(&nebulaentry[i], &nebulaentry[i + 1],
		(sizeof(nebulaentry[0]) * (nnebula - i)));
}

static int update_nebula(uint32_t id, double x, double y, double r)
{
	int i;

	i = lookup_object_by_id(id);
	if (i < 0) {
		i = add_generic_object(id, x, y, 0.0, 0.0, 0.0, OBJTYPE_NEBULA, 1, NULL);
		if (i < 0)
			return i;
		add_nebula_entry(go[i].id, x, y, r);
	} else {
		update_generic_object(i, x, y, 0, 0.0, 0.0, 0.0, 1);
	}
	go[i].tsd.nebula.r = r;	
	go[i].alive = 1;
	return 0;
}

static void spark_move(struct snis_entity *o)
{
	o->x += o->vx;
	o->y += o->vy;
	o->tsd.spark.z += o->tsd.spark.vz;
	o->tsd.spark.rx += o->tsd.spark.avx;
	o->tsd.spark.ry += o->tsd.spark.avy;
	o->tsd.spark.rz += o->tsd.spark.avz;
	normalize_angle(&o->tsd.spark.rx);
	normalize_angle(&o->tsd.spark.ry);
	normalize_angle(&o->tsd.spark.rz);
	o->alive--;
	if (o->alive <= 0) {
		remove_entity(ecx, o->entity);
		snis_object_pool_free_object(sparkpool, o->index);
	}
}

static void move_sparks(void)
{
	int i;

	for (i = 0; i <= snis_object_pool_highest_object(sparkpool); i++)
		if (spark[i].alive) {
			spark[i].move(&spark[i]);
			update_entity_pos(spark[i].entity, spark[i].x,
						spark[i].tsd.spark.z, -spark[i].y);
			update_entity_rotation(spark[i].entity,
						spark[i].tsd.spark.rx,
						spark[i].tsd.spark.ry,
						spark[i].tsd.spark.rz);
		}
}

static void spin_wormhole(struct snis_entity *o)
{
	float angle;

	angle = ((timer * 5) % 360) * M_PI / 180.0;
	update_entity_rotation(o->entity, 0.0, 0.0, angle);
}

static void spin_starbase(struct snis_entity *o)
{
	float angle;

	angle = ((timer / 2) % 360) * M_PI / 180.0;
	update_entity_rotation(o->entity, 0.0, 0.0, angle);
}

static void move_objects(void)
{
	int i;

	for (i = 0; i <= snis_object_pool_highest_object(pool); i++) {
		struct snis_entity *o = &go[i];

		if (!o->alive)
			continue;
		switch (o->type) {
		case OBJTYPE_WORMHOLE:
			spin_wormhole(o);
			break;
		case OBJTYPE_STARBASE:
			spin_starbase(o);
			break;
		case OBJTYPE_LASER:
		case OBJTYPE_TORPEDO:
			/* predictive movement, this is probably */
			/* too dumb to work right */
			o->x += o->vx / 2.0;
			o->y += o->vy / 2.0;
			break;
		case OBJTYPE_LASERBEAM:
		case OBJTYPE_TRACTORBEAM:
			o->move(o);
			break;
		default:
			break;
		}
	}
}

void add_spark(double x, double y, double z, double vx, double vy, double vz, int time, int color)
{
	int i, r;
	struct entity *e;

	i = snis_object_pool_alloc_obj(sparkpool);
	if (i < 0)
		return;
	r = snis_randn(100);
	if (r < 50 || time < 10) {
		e = add_entity(ecx, particle_mesh, x, z, -y, PARTICLE_COLOR);
		set_render_style(e, RENDER_WIREFRAME | RENDER_BRIGHT_LINE);
	} else if (r < 75) {
		e = add_entity(ecx, debris_mesh, x, z, -y, color);
	} else {
		e = add_entity(ecx, debris2_mesh, x, z, -y, color);
	}
	memset(&spark[i], 0, sizeof(spark[i]));
	spark[i].index = i;
	spark[i].x = x;
	spark[i].y = y;
	spark[i].z = z;
	spark[i].tsd.spark.z = z;
	spark[i].vx = vx;
	spark[i].vy = vy;
	spark[i].tsd.spark.vz = vz;
	spark[i].tsd.spark.rx = M_PI / 180.0 * snis_randn(180);
	spark[i].tsd.spark.ry = M_PI / 180.0 * snis_randn(180);
	spark[i].tsd.spark.rz = M_PI / 180.0 * snis_randn(180);
	spark[i].tsd.spark.avx = (M_PI / 180.0) * (snis_randn(30) - 15); 
	spark[i].tsd.spark.avy = (M_PI / 180.0) * (snis_randn(30) - 15); 
	spark[i].tsd.spark.avz = (M_PI / 180.0) * (snis_randn(30) - 15); 
	spark[i].type = OBJTYPE_SPARK;
	spark[i].alive = time + snis_randn(time);
	spark[i].move = spark_move;
	spark[i].entity = e;
	return;
}

#if 0
void sphere_explode(int x, int y, int ivx, int ivy, int v,
	int nsparks, int time)
{
	int vx, vy;
	int angle;
	int shell;
	int velocity;
	int delta_angle;
	float point_dist;
	int angle_offset;

	angle_offset = snis_randn(360);

	if (nsparks < 30)
		nsparks = 40;
	point_dist = v * 2 * M_PI / (nsparks / 4);
	delta_angle = (int) ((point_dist / (2.0 * M_PI * v)) * 360.0);

	v = v / (1.0 + snis_randn(100) / 100.0);
	for (shell = 0; shell < 90; shell += 7) {
		for (angle = 0; angle < 360; angle += delta_angle) {
			velocity = (int) (cosine[shell] * (double) v);
			vx = cosine[(angle + angle_offset) % 360] * velocity + ivx;
			vy = sine[(angle + angle_offset) % 360] * velocity + ivy;
			add_spark(x, y, vx, vy, time);
		}
		delta_angle = (int) ((point_dist / (2.0 * velocity * M_PI)) * 360.0);
	}
}
#endif

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
		vy = v * sin(angle);
		vz = v * cos(zangle) / 3.0;
		add_spark(x, y, z, vx, vy, vz, time, color);
	}
}

static int update_explosion(uint32_t id, double x, double y, double z,
		uint16_t nsparks, uint16_t velocity, uint16_t time, uint8_t victim_type)
{
	int i;
	i = lookup_object_by_id(id);
	if (i < 0) {
		i = add_generic_object(id, x, y, 0.0, 0.0, 0.0, OBJTYPE_EXPLOSION, 1, NULL);
		if (i < 0)
			return i;
		go[i].tsd.explosion.nsparks = nsparks;
		go[i].tsd.explosion.velocity = velocity;
		go[i].z = z;
		do_explosion(x, y, z, nsparks, velocity, (int) time, victim_type);
	}
	return 0;
}

static int __attribute__((unused)) add_asteroid(uint32_t id, double x, double y, double vx, double vy, double heading, uint32_t alive)
{
	return add_generic_object(id, x, y, vx, vy, heading, OBJTYPE_ASTEROID, alive, NULL);
}

static int __attribute__((unused)) add_starbase(uint32_t id, double x, double y, double vx, double vy, double heading, uint32_t alive)
{
	return add_generic_object(id, x, y, vx, vy, heading, OBJTYPE_STARBASE, alive, NULL);
}

static int __attribute__((unused)) add_torpedo(uint32_t id, double x, double y, double vx, double vy, double heading, uint32_t alive)
{
	return add_generic_object(id, x, y, vx, vy, heading, OBJTYPE_TORPEDO, alive, NULL);
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
	"onscreen", "viewmode", "zoom", "unzoom",
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
	queue_to_server(packed_buffer_new("hw", OPCODE_SCI_SELECT_TARGET, id));
}

static void request_sci_select_coords(double ux, double uy)
{
	queue_to_server(packed_buffer_new("hSS", OPCODE_SCI_SELECT_COORDS,
			ux, (int32_t) UNIVERSE_DIM, uy, (int32_t) UNIVERSE_DIM));
}

static void request_navigation_yaw_packet(uint8_t yaw)
{
	queue_to_server(packed_buffer_new("hb", OPCODE_REQUEST_YAW, yaw));
}

static void request_navigation_thrust_packet(uint8_t thrust)
{
	queue_to_server(packed_buffer_new("hb", OPCODE_REQUEST_THRUST, thrust));
}

static void navigation_dirkey(int h, int v)
{
	uint8_t yaw;
	static int last_time = 0;
	int fine;

	fine = 2 * (timer - last_time > 5);
	last_time = timer;

	if (!h && !v)
		return;

	if (h) {
		yaw = h < 0 ? YAW_LEFT + fine : YAW_RIGHT + fine;
		request_navigation_yaw_packet(yaw);
	}
#if 0
	if (v) {
		thrust = v < 0 ? THRUST_BACKWARDS : THRUST_FORWARDS;
		request_navigation_thrust_packet(thrust);
	}
#endif
}

static void request_demon_yaw_packet(uint32_t oid, uint8_t yaw)
{
	queue_to_server(packed_buffer_new("hwb", OPCODE_DEMON_YAW, oid, yaw));
}

static void request_demon_thrust_packet(uint32_t oid, uint8_t thrust)
{
	queue_to_server(packed_buffer_new("hwb",
				OPCODE_DEMON_THRUST, oid, thrust));
}

static struct demon_ui {
	float ux1, uy1, ux2, uy2;
	double selectedx, selectedy;
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
	double ix, iy, ix2, iy2;
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
	queue_to_server(packed_buffer_new("hb", OPCODE_REQUEST_GUNYAW, yaw));
}

static void weapons_dirkey(int h, int v)
{
	static int last_time = 0;
	int fine;
	uint8_t yaw;

	if (!h && !v)
		return;

	fine = 2 * (timer - last_time > 5);
	last_time = timer;
	if (h) {
		yaw = h < 0 ? YAW_LEFT + fine : YAW_RIGHT + fine;
		request_weapons_yaw_packet(yaw);
	}
}

static void science_dirkey(int h, int v)
{
	uint8_t yaw;

	if (!h && !v)
		return;
	if (v) {
		yaw = v < 0 ? YAW_LEFT : YAW_RIGHT;
		queue_to_server(packed_buffer_new("hb",
				OPCODE_REQUEST_SCIBEAMWIDTH, yaw));
	}
	if (h) {
		yaw = h < 0 ? YAW_LEFT : YAW_RIGHT;
		queue_to_server(packed_buffer_new("hb",
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
		queue_to_server(packed_buffer_new("hb",
				OPCODE_REQUEST_ROBOT_THRUST, thrust));
	}
	if (h) {
		yaw = h < 0 ? YAW_LEFT : YAW_RIGHT;
		queue_to_server(packed_buffer_new("hb",
				OPCODE_REQUEST_ROBOT_YAW, yaw));
	}
	wakeup_gameserver_writer();
}

static void do_onscreen(uint8_t mode)
{
	queue_to_server(packed_buffer_new("hb", OPCODE_ROLE_ONSCREEN, mode));
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
	queue_to_server(packed_buffer_new("hSb", OPCODE_MAINSCREEN_VIEW_MODE,
				0.0, (int32_t) 360, new_mode));
}

static void do_dirkey(int h, int v)
{
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
			navigation_dirkey(h, v); 
			break;
		case DISPLAYMODE_WEAPONS:
			weapons_dirkey(h, v); 
			break;
		case DISPLAYMODE_SCIENCE:
			science_dirkey(h, v); 
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

static void do_torpedo(void)
{
	struct snis_entity *o;

	if (displaymode != DISPLAYMODE_WEAPONS)
		return;
	if (!(o = find_my_ship()))
		return;
	if (o->tsd.ship.torpedoes_loaded <= 0)
		return;
	queue_to_server(packed_buffer_new("h", OPCODE_REQUEST_TORPEDO));
}

static void do_tractor_beam(void)
{
	struct snis_entity *o;

	if (displaymode != DISPLAYMODE_WEAPONS)
		return;
	if (!(o = find_my_ship()))
		return;
	queue_to_server(packed_buffer_new("h", OPCODE_REQUEST_TRACTORBEAM));
}

static void do_laser(void)
{
	if (displaymode != DISPLAYMODE_WEAPONS)
		return;
	queue_to_server(packed_buffer_new("h", OPCODE_REQUEST_LASER));
}

static void robot_backward_button_pressed(void *x);
static void robot_forward_button_pressed(void *x);
static void robot_left_button_pressed(void *x);
static void robot_right_button_pressed(void *x);
static void robot_gripper_button_pressed(void *x);

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

static void deal_with_joystick()
{

#define FRAME_RATE_HZ 30 
	static const int joystick_throttle = (int) ((FRAME_RATE_HZ / 15.0) + 0.5);
	static struct wwvi_js_event jse;
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

static void do_adjust_byte_value(uint8_t value,  uint16_t opcode);
static void do_zoom(int z)
{
	int newval;
	struct snis_entity *o;

	if (!(o = find_my_ship()))
		return;

	switch (displaymode) {
	case DISPLAYMODE_WEAPONS:
		newval = o->tsd.ship.weapzoom + z;
		if (newval < 0)
			newval = 0;
		if (newval > 255)
			newval = 255;
                do_adjust_byte_value((uint8_t) newval, OPCODE_REQUEST_WEAPZOOM);
		break;
	case DISPLAYMODE_NAVIGATION:
		newval = o->tsd.ship.navzoom + z;
		if (newval < 0)
			newval = 0;
		if (newval > 255)
			newval = 255;
                do_adjust_byte_value((uint8_t) newval, OPCODE_REQUEST_NAVZOOM);
		break;
	default:
		break;
	}
}

static void deal_with_keyboard()
{
	int h, v, z;
	static const int keyboard_throttle = (int) ((FRAME_RATE_HZ / 15.0) + 0.5);
	if (timer % keyboard_throttle != 0)
		return;

	h = 0;
	v = 0;
	z = 0;

	if (kbstate.pressed[keyleft])	
		h = -1;
	if (kbstate.pressed[keyright])
		h = 1;
	if (kbstate.pressed[keyup])
		v = -1;
	if (kbstate.pressed[keydown])
		v = 1;
	if (h || v)
		do_dirkey(h, v);

	if (kbstate.pressed[keyzoom])
		z = 10;
	if (kbstate.pressed[keyunzoom])
		z = -10;
	if (z)
		do_zoom(z);
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
	sng_abs_xy_draw_string(w, gc, "A B C D E F G H I J K L M", SMALL_FONT, 30, 30); 
	sng_abs_xy_draw_string(w, gc, "N O P Q R S T U V W X Y Z", SMALL_FONT, 30, 60); 
	sng_abs_xy_draw_string(w, gc, "a b c d e f g h i j k l m", SMALL_FONT, 30, 90); 
	sng_abs_xy_draw_string(w, gc, "n o p q r s t u v w x y z", SMALL_FONT, 30, 120); 
	sng_abs_xy_draw_string(w, gc, "0 1 2 3 4 5 6 7 8 9 ! , .", SMALL_FONT, 30, 150); 
	sng_abs_xy_draw_string(w, gc, "? | - = * / \\ + ( ) \" ' _", SMALL_FONT, 30, 180); 

	sng_abs_xy_draw_string(w, gc, "The Quick Fox Jumps Over The Lazy Brown Dog.", SMALL_FONT, 30, 210); 
	sng_abs_xy_draw_string(w, gc, "The Quick Fox Jumps Over The Lazy Brown Dog.", BIG_FONT, 30, 280); 
	sng_abs_xy_draw_string(w, gc, "The Quick Fox Jumps Over The Lazy Brown Dog.", TINY_FONT, 30, 350); 
	sng_abs_xy_draw_string(w, gc, "The quick fox jumps over the lazy brown dog.", NANO_FONT, 30, 380); 
	sng_abs_xy_draw_string(w, gc, "THE QUICK FOX JUMPS OVER THE LAZY BROWN DOG.", TINY_FONT, 30, 410); 
	sng_abs_xy_draw_string(w, gc, "Well now, what have we here?  James Bond!", NANO_FONT, 30, 425); 
	sng_abs_xy_draw_string(w, gc, "The quick fox jumps over the lazy brown dog.", SMALL_FONT, 30, 450); 
	sng_abs_xy_draw_string(w, gc, "Copyright (C) 2010 Stephen M. Cameron 0123456789", TINY_FONT, 30, 480); 
}

static void show_introscreen(GtkWidget *w)
{
	sng_abs_xy_draw_string(w, gc, "Space Nerds", BIG_FONT, 80, 200); 
	sng_abs_xy_draw_string(w, gc, "In Space", BIG_FONT, 180, 320); 
	sng_abs_xy_draw_string(w, gc, "Copyright (C) 2010 Stephen M. Cameron", NANO_FONT, 255, 550); 
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
		sng_abs_xy_draw_string(w, gc, "Space Nerds", BIG_FONT, 80, 200); 
		sng_abs_xy_draw_string(w, gc, "In Space", BIG_FONT, 180, 320); 
		sng_abs_xy_draw_string(w, gc, "Copyright (C) 2010 Stephen M. Cameron", NANO_FONT, 255, 550); 
		if (lobby_count >= MAX_LOBBY_TRIES)
			sprintf(msg, "Giving up on lobby... tried %d times.",
				lobby_count);
		else
			sprintf(msg, "Connecting to lobby... tried %d times.",
				lobby_count);
		sng_abs_xy_draw_string(w, gc, msg, SMALL_FONT, 100, 400);
		sng_abs_xy_draw_string(w, gc, lobbyerror, NANO_FONT, 100, 430);
	} else {
		if (lobby_selected_server != -1 &&
			lobbylast1clickx > 200 && lobbylast1clickx < 620 &&
			lobbylast1clicky > 520 && lobbylast1clicky < 520 + LINEHEIGHT * 2) {
			displaymode = DISPLAYMODE_CONNECTING;
			return;
		}
		lobby_selected_server = -1;
		sprintf(msg, "Connected to lobby on socket %d\n", lobby_socket);
		sng_abs_xy_draw_string(w, gc, msg, TINY_FONT, 30, LINEHEIGHT);
		sprintf(msg, "Total game servers: %d\n", ngameservers);
		sng_abs_xy_draw_string(w, gc, msg, TINY_FONT, 30, LINEHEIGHT + 20);
		for (i = 0; i < ngameservers; i++) {
			unsigned char *x = (unsigned char *) 
				&lobby_game_server[i].ipaddr;
			if (lobbylast1clickx > 30 && lobbylast1clickx < 700 &&
				lobbylast1clicky > 100 + (-0.5 + i) * LINEHEIGHT &&
				lobbylast1clicky < 100 + (0.5 + i) * LINEHEIGHT) {
				lobby_selected_server = i;
				sng_set_foreground(GREEN);
				snis_draw_rectangle(w->window, gc, 0, 25, 100 + (-0.5 + i) * LINEHEIGHT,
					725, LINEHEIGHT);
			} else
				sng_set_foreground(WHITE);
			 
			sprintf(msg, "%hu.%hu.%hu.%hu/%hu", x[0], x[1], x[2], x[3], lobby_game_server[i].port);
			sng_abs_xy_draw_string(w, gc, msg, TINY_FONT, 30, 100 + i * LINEHEIGHT);
			sprintf(msg, "%s", lobby_game_server[i].game_instance);
			sng_abs_xy_draw_string(w, gc, msg, TINY_FONT, 350, 100 + i * LINEHEIGHT);
			sprintf(msg, "%s", lobby_game_server[i].server_nickname);
			sng_abs_xy_draw_string(w, gc, msg, TINY_FONT, 450, 100 + i * LINEHEIGHT);
			sprintf(msg, "%s", lobby_game_server[i].location);
			sng_abs_xy_draw_string(w, gc, msg, TINY_FONT, 650, 100 + i * LINEHEIGHT);
			sprintf(msg, "%d", lobby_game_server[i].nconnections);
			sng_abs_xy_draw_string(w, gc, msg, TINY_FONT, 700, 100 + i * LINEHEIGHT);
		}
		if (lobby_selected_server != -1)
			sng_set_foreground(GREEN);
		else
			sng_set_foreground(RED);
		/* This should be a real button, but I'm too lazy to fix it now. */
		snis_draw_rectangle(w->window, gc, 0, 250, 520, 300, LINEHEIGHT * 2);
		sng_abs_xy_draw_string(w, gc, "CONNECT TO SERVER", TINY_FONT, 280, 520 + LINEHEIGHT);
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

static int process_update_ship_packet(uint16_t opcode)
{
	unsigned char buffer[100];
	struct packed_buffer pb;
	uint32_t id, alive, torpedoes, power;
	uint32_t fuel, victim_id;
	double dx, dy, dz, dheading, dgheading, dsheading, dbeamwidth, dvx, dvy;
	int rc;
	int type = opcode == OPCODE_UPDATE_SHIP ? OBJTYPE_SHIP1 : OBJTYPE_SHIP2;
	uint8_t tloading, tloaded, throttle, rpm, temp, scizoom, weapzoom, navzoom,
		warpdrive, requested_warpdrive,
		requested_shield, phaser_charge, phaser_wavelength, shiptype,
		reverse;

	assert(sizeof(buffer) > sizeof(struct update_ship_packet) - sizeof(uint16_t));
	rc = snis_readsocket(gameserver_sock, buffer, sizeof(struct update_ship_packet) - sizeof(uint16_t));
	/* printf("process_update_ship_packet, snis_readsocket returned %d\n", rc); */
	if (rc != 0)
		return rc;
	packed_buffer_init(&pb, buffer, sizeof(buffer));
	packed_buffer_extract(&pb, "wwSSSSS", &id, &alive,
				&dx, (int32_t) UNIVERSE_DIM, &dy, (int32_t) UNIVERSE_DIM,
				&dz, (int32_t) UNIVERSE_DIM,
				&dvx, (int32_t) UNIVERSE_DIM, &dvy, (int32_t) UNIVERSE_DIM);
	packed_buffer_extract(&pb, "UwwUUU", &dheading, (uint32_t) 360,
				&torpedoes, &power, &dgheading, (uint32_t) 360,
				&dsheading, (uint32_t) 360, &dbeamwidth, (uint32_t) 360);
	packed_buffer_extract(&pb, "bbbwbbbbbbbbbbbw", &tloading, &throttle, &rpm, &fuel, &temp,
			&scizoom, &weapzoom, &navzoom,
			&warpdrive, &requested_warpdrive,
			&requested_shield, &phaser_charge, &phaser_wavelength, &shiptype,
			&reverse, &victim_id);
	tloaded = (tloading >> 4) & 0x0f;
	tloading = tloading & 0x0f;
	pthread_mutex_lock(&universe_mutex);
	rc = update_ship(id, dx, dy, dz, dvx, dvy, dheading, alive, torpedoes, power,
				dgheading, dsheading, dbeamwidth, type,
				tloading, tloaded, throttle, rpm, fuel, temp, scizoom,
				weapzoom, navzoom, warpdrive, requested_warpdrive, requested_shield,
				phaser_charge, phaser_wavelength, shiptype, reverse, victim_id);
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
	unsigned char buffer[sizeof(struct damcon_obj_update_packet) - sizeof(uint16_t)];
	uint32_t id, ship_id, type;
	double x, y, velocity, heading;
	int rc;

	rc = read_and_unpack_buffer(buffer, "wwwSSSS",
			&id, &ship_id, &type,
			&x, (int32_t) DAMCONXDIM, 
			&y, (int32_t) DAMCONYDIM, 
			&velocity, (int32_t) DAMCONXDIM, 
			&heading, (int32_t) 360);
	if (rc)
		return rc;
	pthread_mutex_lock(&universe_mutex);
	rc = update_damcon_object(id, ship_id, type, x, y, velocity, heading);
	pthread_mutex_unlock(&universe_mutex);
	return rc;
}

static int process_update_damcon_socket(void)
{
	unsigned char buffer[sizeof(struct damcon_socket_update_packet) - sizeof(uint16_t)];
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
	unsigned char buffer[sizeof(struct damcon_part_update_packet) - sizeof(uint16_t)];
	uint32_t id, ship_id, type;
	double x, y, heading;
	uint8_t system, part, damage;
	int rc;

	rc = read_and_unpack_buffer(buffer, "wwwSSUbbb",
		&id, &ship_id, &type,
			&x, (int32_t) DAMCONXDIM, 
			&y, (int32_t) DAMCONYDIM, 
			&heading, (uint32_t) 360, 
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
					sizeof(uint16_t)];
	struct snis_entity *o;
	uint8_t view_mode;
	double view_angle;
	int rc;

	rc = read_and_unpack_buffer(buffer, "Sb", &view_angle, (int32_t) 360,
				&view_mode);
	if (rc != 0)
		return rc;
	if (!(o = find_my_ship()))
		return 0;
	o->tsd.ship.view_angle = view_angle;
	o->tsd.ship.view_mode = view_mode;
	return 0;
}

static int process_update_econ_ship_packet(uint16_t opcode)
{
	unsigned char buffer[100];
	uint32_t id, alive, victim_id;
	double dx, dy, dz, dheading, dv, dvx, dvy;
	uint8_t shiptype;
	int rc;

	assert(sizeof(buffer) > sizeof(struct update_econ_ship_packet) - sizeof(uint16_t));
	rc = read_and_unpack_buffer(buffer, "wwSSSUUwb", &id, &alive,
				&dx, (int32_t) UNIVERSE_DIM, &dy, (int32_t) UNIVERSE_DIM, 
				&dz, (int32_t) UNIVERSE_DIM,
				&dv, (uint32_t) UNIVERSE_DIM, &dheading, (uint32_t) 360,
				&victim_id, &shiptype);
	if (rc != 0)
		return rc;
	dvx = cos(dheading) * dv;
	dvy = sin(dheading) * dv;
	pthread_mutex_lock(&universe_mutex);
	rc = update_econ_ship(id, dx, dy, dz, dvx, dvy, dheading, alive, victim_id, shiptype);
	pthread_mutex_unlock(&universe_mutex);
	return (rc < 0);
} 

static int process_update_torpedo_packet(void)
{
	unsigned char buffer[100];
	uint32_t id, ship_id;
	double dx, dy, dz, dvx, dvy;
	int rc;

	assert(sizeof(buffer) > sizeof(struct update_torpedo_packet) - sizeof(uint16_t));
	rc = read_and_unpack_buffer(buffer, "wwSSSSS", &id, &ship_id,
				&dx, (int32_t) UNIVERSE_DIM, &dy, (int32_t) UNIVERSE_DIM,
				&dz, (int32_t) UNIVERSE_DIM,
				&dvx, (int32_t) UNIVERSE_DIM, &dvy, (int32_t) UNIVERSE_DIM);
	if (rc != 0)
		return rc;
	pthread_mutex_lock(&universe_mutex);
	rc = update_torpedo(id, dx, dy, dz, dvx, dvy, ship_id);
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
	uint32_t id, ship_id;
	double dx, dy, dz, dvx, dvy;
	int rc;

	assert(sizeof(buffer) > sizeof(struct update_laser_packet) - sizeof(uint16_t));
	rc = read_and_unpack_buffer(buffer, "wwSSSSS", &id, &ship_id,
				&dx, (int32_t) UNIVERSE_DIM, &dy, (int32_t) UNIVERSE_DIM,
				&dz, (int32_t) UNIVERSE_DIM,
				&dvx, (int32_t) UNIVERSE_DIM, &dvy, (int32_t) UNIVERSE_DIM);
	if (rc != 0)
		return rc;
	pthread_mutex_lock(&universe_mutex);
	rc = update_laser(id, dx, dy, dz, dvx, dvy, ship_id);
	pthread_mutex_unlock(&universe_mutex);
	return (rc < 0);
} 

static int process_update_spacemonster(void)
{
	unsigned char buffer[100];
	uint32_t id;
	double dx, dy, dz;
	int rc;

	assert(sizeof(buffer) > sizeof(struct update_spacemonster_packet) - sizeof(uint16_t));
	rc = read_and_unpack_buffer(buffer, "wSSS", &id,
				&dx, (int32_t) UNIVERSE_DIM, &dy, (int32_t) UNIVERSE_DIM,
				&dz, (int32_t) UNIVERSE_DIM);
	if (rc != 0)
		return rc;
	pthread_mutex_lock(&universe_mutex);
	rc = update_spacemonster(id, dx, dy, dz);
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
	if (go[i].type == OBJTYPE_NEBULA)
		delete_nebula_entry(go[i].id);
	go[i].id = -1;
	snis_object_pool_free_object(pool, i);
}

static void demon_deselect(uint32_t id);
static int process_delete_object_packet(void)
{
	unsigned char buffer[10];
	uint32_t id;
	int rc;

	assert(sizeof(buffer) > sizeof(struct delete_object_packet) - sizeof(uint16_t));
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

	assert(sizeof(buffer) > sizeof(struct play_sound_packet) - sizeof(uint16_t));
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

	assert(sizeof(buffer) > sizeof(struct ship_sdata_packet) - sizeof(uint16_t));
	rc = snis_readsocket(gameserver_sock, buffer, sizeof(struct ship_sdata_packet) - sizeof(uint16_t));
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
	struct slider *scizoom;
	struct button *details_button;
} sci_ui;

static int process_sci_details(void)
{
	unsigned char buffer[10];
	uint8_t new_details;
	int rc;

	rc = read_and_unpack_buffer(buffer, "b", &new_details);
	if (rc != 0)
		return rc;
	sci_ui.details_mode = new_details;
	return 0;
}

static struct snis_entity *curr_science_guy = NULL;
static struct snis_entity *prev_science_guy = NULL;
static int process_sci_select_target_packet(void)
{
	unsigned char buffer[sizeof(struct snis_sci_select_target_packet)];
	uint32_t id;
	int rc, i;

	rc = read_and_unpack_buffer(buffer, "w", &id);
	if (rc != 0)
		return rc;
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
	double ux, uy;
	int rc;

	rc = read_and_unpack_buffer(buffer, "SS",
			&ux, (int32_t) UNIVERSE_DIM,
			&uy, (int32_t) UNIVERSE_DIM); 
	if (rc != 0)
		return rc;
	if (!(o = find_my_ship()))
		return 0;
	o->sci_coordx = ux;	
	o->sci_coordy = uy;	
	return 0;
}

static void zero_nav_sliders(void);
static void zero_weapons_sliders(void);
static void zero_engineering_sliders(void);
static void zero_science_sliders(void);

static void zero_slider_inputs(void)
{
	zero_nav_sliders();
	zero_weapons_sliders();
	zero_engineering_sliders();
	zero_science_sliders();
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
	if (o->respawn_time == 0)
		zero_slider_inputs();
	return 0;
}

static int process_update_netstats(void)
{
	unsigned char buffer[sizeof(struct netstats_packet)];
	int rc;

	rc = read_and_unpack_buffer(buffer, "qqw", &netstats.bytes_sent,
				&netstats.bytes_recd, &netstats.elapsed_seconds);
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
	struct snis_text_input_box *comms_input;
	char input[100];
} comms_ui;

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
	return 0;
}

static int process_ship_damage_packet(void)
{
	char buffer[sizeof(struct ship_damage_packet)];
	struct packed_buffer pb;
	uint32_t id;
	int rc, i;
	struct ship_damage_data damage;

	rc = snis_readsocket(gameserver_sock, buffer,
			sizeof(struct ship_damage_packet) - sizeof(uint16_t));
	if (rc != 0)
		return rc;
	packed_buffer_init(&pb, buffer, sizeof(buffer));
	id = packed_buffer_extract_u32(&pb);
	i = lookup_object_by_id(id);
	
	if (i < 0)
		return -1;
	packed_buffer_extract_raw(&pb, (char *) &damage, sizeof(damage));
	pthread_mutex_lock(&universe_mutex);
	go[i].tsd.ship.damage = damage;
	pthread_mutex_unlock(&universe_mutex);
	if (id == my_ship_id) 
		damage_limbo_countdown = 2;
	return 0;
}

static int process_update_asteroid_packet(void)
{
	unsigned char buffer[100];
	uint32_t id;
	double dx, dy, dz;
	int rc;

	assert(sizeof(buffer) > sizeof(struct update_asteroid_packet) - sizeof(uint16_t));
	rc = read_and_unpack_buffer(buffer, "wSSS", &id,
			&dx, (int32_t) UNIVERSE_DIM,
			&dy,(int32_t) UNIVERSE_DIM,
			&dz, (int32_t) UNIVERSE_DIM);
	if (rc != 0)
		return rc;
	pthread_mutex_lock(&universe_mutex);
	rc = update_asteroid(id, dx, dy, dz);
	pthread_mutex_unlock(&universe_mutex);
	return (rc < 0);
} 

static int process_update_derelict_packet(void)
{
	unsigned char buffer[100];
	uint32_t id;
	double dx, dy, dz;
	uint8_t shiptype;
	int rc;

	assert(sizeof(buffer) > sizeof(struct update_asteroid_packet) - sizeof(uint16_t));
	rc = read_and_unpack_buffer(buffer, "wSSSb", &id,
			&dx, (int32_t) UNIVERSE_DIM,
			&dy,(int32_t) UNIVERSE_DIM,
			&dz, (int32_t) UNIVERSE_DIM, &shiptype);
	if (rc != 0)
		return rc;
	pthread_mutex_lock(&universe_mutex);
	rc = update_derelict(id, dx, dy, dz, shiptype);
	pthread_mutex_unlock(&universe_mutex);
	return (rc < 0);
} 

static int process_update_planet_packet(void)
{
	unsigned char buffer[100];
	uint32_t id;
	double dx, dy, dz;
	int rc;

	assert(sizeof(buffer) > sizeof(struct update_asteroid_packet) - sizeof(uint16_t));
	rc = read_and_unpack_buffer(buffer, "wSSS", &id,
			&dx, (int32_t) UNIVERSE_DIM,
			&dy,(int32_t) UNIVERSE_DIM,
			&dz, (int32_t) UNIVERSE_DIM);
	if (rc != 0)
		return rc;
	pthread_mutex_lock(&universe_mutex);
	rc = update_planet(id, dx, dy, dz);
	pthread_mutex_unlock(&universe_mutex);
	return (rc < 0);
} 

static int process_update_wormhole_packet(void)
{
	unsigned char buffer[100];
	uint32_t id;
	double dx, dy;
	int rc;

	assert(sizeof(buffer) > sizeof(struct update_wormhole_packet) - sizeof(uint16_t));
	rc = read_and_unpack_buffer(buffer, "wSS", &id,
			&dx, (int32_t) UNIVERSE_DIM, &dy, (int32_t) UNIVERSE_DIM);
	if (rc != 0)
		return rc;
	pthread_mutex_lock(&universe_mutex);
	rc = update_wormhole(id, dx, dy);
	pthread_mutex_unlock(&universe_mutex);
	return (rc < 0);
} 

static int process_update_starbase_packet(void)
{
	unsigned char buffer[100];
	uint32_t id;
	double dx, dy;
	int rc;

	assert(sizeof(buffer) > sizeof(struct update_starbase_packet) - sizeof(uint16_t));
	rc = read_and_unpack_buffer(buffer, "wSS", &id,
			&dx, (int32_t) UNIVERSE_DIM, &dy, (int32_t) UNIVERSE_DIM);
	if (rc != 0)
		return rc;
	pthread_mutex_lock(&universe_mutex);
	rc = update_starbase(id, dx, dy);
	pthread_mutex_unlock(&universe_mutex);
	return (rc < 0);
} 

static int process_update_nebula_packet(void)
{
	unsigned char buffer[100];
	uint32_t id;
	double dx, dy, r;
	int rc;

	assert(sizeof(buffer) > sizeof(struct update_nebula_packet) - sizeof(uint16_t));
	rc = read_and_unpack_buffer(buffer, "wSSS", &id,
			&dx, (int32_t) UNIVERSE_DIM,
			&dy, (int32_t) UNIVERSE_DIM,
			&r, (int32_t) UNIVERSE_DIM);
	if (rc != 0)
		return rc;
	pthread_mutex_lock(&universe_mutex);
	rc = update_nebula(id, dx, dy, r);
	pthread_mutex_unlock(&universe_mutex);
	return (rc < 0);
} 

static int process_update_laserbeam(void)
{
	unsigned char buffer[100];
	uint32_t id, origin, target;
	int rc;

	rc = read_and_unpack_buffer(buffer, "www", &id, &origin, &target);
	if (rc != 0)
		return rc;
	pthread_mutex_lock(&universe_mutex);
	rc = update_laserbeam(id, origin, target);
	pthread_mutex_unlock(&universe_mutex);
	return (rc < 0);
} 

static int process_update_tractorbeam(void)
{
	unsigned char buffer[100];
	uint32_t id, origin, target;
	int rc;

	rc = read_and_unpack_buffer(buffer, "www", &id, &origin, &target);
	if (rc != 0)
		return rc;
	pthread_mutex_lock(&universe_mutex);
	rc = update_tractorbeam(id, origin, target);
	pthread_mutex_unlock(&universe_mutex);
	return (rc < 0);
} 

static int process_update_explosion_packet(void)
{
	unsigned char buffer[sizeof(struct update_explosion_packet)];
	uint32_t id;
	double dx, dy, dz;
	uint16_t nsparks, velocity, time;
	uint8_t victim_type;
	int rc;

	assert(sizeof(buffer) > sizeof(struct update_explosion_packet) - sizeof(uint16_t));
	rc = read_and_unpack_buffer(buffer, "wSSShhhb", &id,
		&dx, (int32_t) UNIVERSE_DIM, &dy, (int32_t) UNIVERSE_DIM,
		&dz, (int32_t) UNIVERSE_DIM,
		&nsparks, &velocity, &time, &victim_type);
	if (rc != 0)
		return rc;
	pthread_mutex_lock(&universe_mutex);
	rc = update_explosion(id, dx, dy, dz, nsparks, velocity, time, victim_type);
	pthread_mutex_unlock(&universe_mutex);
	return (rc < 0);
}

static int process_client_id_packet(void)
{
	unsigned char buffer[100];
	uint32_t id;
	int rc;

	assert(sizeof(buffer) > sizeof(struct client_ship_id_packet) - sizeof(uint16_t));
	rc = read_and_unpack_buffer(buffer, "w", &id);
	if (rc)
		return rc;
	my_ship_id = id;
	printf("SET MY SHIP ID to %u\n", my_ship_id);
	return 0;
}

static void *gameserver_reader(__attribute__((unused)) void *arg)
{
	uint16_t opcode;
	int rc;

	printf("gameserver reader thread\n");
	while (1) {
		/* printf("Client reading from game server %d bytes...\n", sizeof(opcode)); */
		rc = snis_readsocket(gameserver_sock, &opcode, sizeof(opcode));
		/* printf("rc = %d, errno  %s\n", rc, strerror(errno)); */
		if (rc != 0)
			goto protocol_error;
		opcode = ntohs(opcode);
		/* printf("got opcode %hd\n", opcode); */
		switch (opcode)	{
		case OPCODE_UPDATE_SHIP:
		case OPCODE_UPDATE_SHIP2:
			/* printf("processing update ship...\n"); */
			rc = process_update_ship_packet(opcode);
			if (rc != 0)
				goto protocol_error;
			break;
		case OPCODE_UPDATE_POWER_DATA:
			rc = process_update_power_data();
			if (rc != 0)
				goto protocol_error;
			break;
		case OPCODE_ECON_UPDATE_SHIP:
			rc = process_update_econ_ship_packet(opcode);
			if (rc != 0)
				goto protocol_error;
			break;
		case OPCODE_ID_CLIENT_SHIP:
			rc = process_client_id_packet();
			if (rc)
				goto protocol_error;
			break;
		case OPCODE_UPDATE_ASTEROID:
			rc = process_update_asteroid_packet();
			if (rc)
				goto protocol_error;
			break;
		case OPCODE_UPDATE_DERELICT:
			rc = process_update_derelict_packet();
			if (rc)
				goto protocol_error;
			break;
		case OPCODE_UPDATE_PLANET:
			rc = process_update_planet_packet();
			if (rc)
				goto protocol_error;
			break;
		case OPCODE_UPDATE_STARBASE:
			rc = process_update_starbase_packet();
			if (rc)
				goto protocol_error;
			break;
		case OPCODE_UPDATE_WORMHOLE:
			rc = process_update_wormhole_packet();
			if (rc)
				goto protocol_error;
			break;
		case OPCODE_UPDATE_NEBULA:
			rc = process_update_nebula_packet();
			if (rc)
				goto protocol_error;
			break;
		case OPCODE_UPDATE_LASERBEAM:
			rc = process_update_laserbeam();
			if (rc)
				goto protocol_error;
			break;
		case OPCODE_UPDATE_TRACTORBEAM:
			rc = process_update_tractorbeam();
			if (rc)
				goto protocol_error;
			break;
		case OPCODE_UPDATE_EXPLOSION:
			rc = process_update_explosion_packet();
			if (rc)
				goto protocol_error;
			break;
		case OPCODE_UPDATE_LASER:
			rc = process_update_laser_packet();
			if (rc != 0)
				goto protocol_error;
			break;
		case OPCODE_UPDATE_TORPEDO:
			/* printf("processing update ship...\n"); */
			rc = process_update_torpedo_packet();
			if (rc != 0)
				goto protocol_error;
			break;
		case OPCODE_WARP_LIMBO:
			rc = process_warp_limbo_packet();
			if (rc != 0)
				goto protocol_error;
			break;
		case OPCODE_INITIATE_WARP:
			process_initiate_warp_packet();
			break;
		case OPCODE_WORMHOLE_LIMBO:
			rc = process_wormhole_limbo_packet();
			if (rc != 0)
				goto protocol_error;
			break;
		case OPCODE_DELETE_OBJECT:
			rc = process_delete_object_packet();
			if (rc != 0)
				goto protocol_error;
			break;
		case OPCODE_PLAY_SOUND:
			rc = process_play_sound_packet();
			if (rc != 0)
				goto protocol_error;
			break;
		case OPCODE_SHIP_SDATA:
			rc = process_ship_sdata_packet();
			if (rc != 0)
				goto protocol_error; 
			break;
		case OPCODE_ROLE_ONSCREEN:
			rc = process_role_onscreen_packet();
			if (rc)
				goto protocol_error;
			break;
		case OPCODE_SCI_SELECT_TARGET:
			rc = process_sci_select_target_packet();
			if (rc)
				goto protocol_error;
			break;
		case OPCODE_SCI_DETAILS:
			rc = process_sci_details();
			if (rc)
				goto protocol_error;
			break;
		case OPCODE_SCI_SELECT_COORDS:
			rc = process_sci_select_coords_packet();
			if (rc)
				goto protocol_error;
			break;
		case OPCODE_UPDATE_DAMAGE:
			rc = process_ship_damage_packet();
			if (rc)
				goto protocol_error;
			break;
		case OPCODE_UPDATE_RESPAWN_TIME:
			rc = process_update_respawn_time();
			if (rc)
				goto protocol_error;
			break;
		case OPCODE_UPDATE_PLAYER:
			break;
		case OPCODE_ACK_PLAYER:
			break;
		case OPCODE_POS_SHIP:
			break;
		case OPCODE_POS_STARBASE:
			break;
		case OPCODE_POS_LASER:
			break;
		case OPCODE_NOOP:
			break;
		case OPCODE_COMMS_TRANSMISSION:
			rc = process_comm_transmission();
			if (rc)
				goto protocol_error;
			break;
		case OPCODE_UPDATE_NETSTATS:
			rc = process_update_netstats();
			if (rc)
				goto protocol_error;
			break;
		
		case OPCODE_DAMCON_OBJ_UPDATE:
			rc = process_update_damcon_object();
			if (rc)
				goto protocol_error;
			break;
		case OPCODE_DAMCON_SOCKET_UPDATE:
			rc = process_update_damcon_socket();
			if (rc)
				goto protocol_error;
			break;
		case OPCODE_DAMCON_PART_UPDATE:
			rc = process_update_damcon_part();
			if (rc)
				goto protocol_error;
			break;
		case OPCODE_MAINSCREEN_VIEW_MODE:
			rc = process_mainscreen_view_mode();
			if (rc)
				goto protocol_error;
			break;
		case OPCODE_UPDATE_SPACEMONSTER:
			rc = process_update_spacemonster();
			if (rc)
				goto protocol_error;
			break;
		case OPCODE_REQUEST_REDALERT:
			rc = process_red_alert();
			if (rc)
				goto protocol_error;
			break;
		case OPCODE_PROXIMITY_ALERT:
			process_proximity_alert();
			break;
		case OPCODE_COLLISION_NOTIFICATION:
			process_collision_notification();
			break;
		default:
			goto protocol_error;
		}
	}

protocol_error:
	printf("Protocol error in gameserver reader, opcode = %hu\n", opcode);
	snis_print_last_buffer(gameserver_sock);	
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
	if (buffer->buffer_size > 0) {
		rc = snis_writesocket(gameserver_sock, buffer->buffer, buffer->buffer_size);
		if (rc) {
			printf("Failed to write to gameserver\n");
			goto badserver;
		}
	} else {
		printf("Hmm, gameserver_writer awakened, but nothing to write.\n");
	}
	packed_buffer_free(buffer);
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

        pthread_mutex_init(&to_server_queue_mutex, NULL);
        pthread_mutex_init(&to_server_queue_event_mutex, NULL);
        packed_buffer_queue_init(&to_server_queue);

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
	app.opcode = htons(OPCODE_UPDATE_PLAYER);
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
	printf("starting gameserver writer thread\n");
	rc = pthread_create(&write_to_gameserver_thread, &gameserver_writer_attr, gameserver_writer, NULL);
	if (rc) {
		fprintf(stderr, "Failed to create gameserver writer thread: %d '%s', '%s'\n",
			rc, strerror(rc), strerror(errno));
	}
	printf("started gameserver writer thread\n");

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
	sng_abs_xy_draw_string(w, gc, "CONNECTING TO SERVER...", SMALL_FONT, 100, 300 + LINEHEIGHT);
	if (!connected_to_gameserver) {
		connected_to_gameserver = 1;
		connect_to_gameserver(lobby_selected_server);
	}
}

static void show_connected_screen(GtkWidget *w)
{
	sng_set_foreground(WHITE);
	sng_abs_xy_draw_string(w, gc, "CONNECTED TO SERVER", SMALL_FONT, 100, 300 + LINEHEIGHT);
	sng_abs_xy_draw_string(w, gc, "DOWNLOADING GAME DATA", SMALL_FONT, 100, 300 + LINEHEIGHT * 3);
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
	sng_abs_xy_draw_string(w, gc, title, SMALL_FONT, 25, 10 + LINEHEIGHT);
	sng_set_foreground(border_color);
	snis_draw_line(w->window, gc, 1, 1, SCREEN_WIDTH, 0);
	snis_draw_line(w->window, gc, SCREEN_WIDTH - 1, 1, SCREEN_WIDTH - 1, SCREEN_HEIGHT - 1);
	snis_draw_line(w->window, gc, SCREEN_WIDTH - 1, SCREEN_HEIGHT - 1, 1, SCREEN_HEIGHT - 1);
	snis_draw_line(w->window, gc, 1, 1, 1, SCREEN_HEIGHT - 1);
}

/* FIXME: make angle of view be calculated from camera parameters */
#define ANGLE_OF_VIEW (70)

static int normalize_degrees(int degrees)
{
	while (degrees < 0)
		degrees += 360;
	while (degrees > 359)
		degrees -= 360;
	return degrees;
}

static void show_mainscreen_starfield(GtkWidget *w, double heading)
{
#ifdef WITHOUTOPENGL
	static int stars_initialized = 0;
	static int stary[720];
	int i, first_angle, last_angle;
	float x, dx;
	float fa;

	if (!stars_initialized) {
		for (i = 0; i < 720; i++)
			stary[i] = snis_randn(SCREEN_HEIGHT);
		stars_initialized = 1;
	}


	fa = (heading * 180 / M_PI) - ANGLE_OF_VIEW / 2.0;
	if (fa < 0)
		fa += 360.0;
	first_angle = fa;
	last_angle = (heading * 180 / M_PI) + ANGLE_OF_VIEW / 2.0;
	first_angle = normalize_degrees(first_angle);
	last_angle = normalize_degrees(last_angle);

	sng_set_foreground(WHITE);

	dx = (float) SCREEN_WIDTH / ANGLE_OF_VIEW;
	x = 0.0 - (fa - (float) first_angle) * dx;
	for (i = first_angle; i != last_angle; i = (i + 1) % 360) {
		snis_draw_line(w->window, gc, (int) x, stary[i], (int) x + (snis_randn(10) < 7), stary[i]);
		snis_draw_line(w->window, gc, (int) x, stary[360 + i],
					(int) x + (snis_randn(10) < 7), stary[360 + i]);
		x += dx;
	}
#endif
}

static void begin_2d_gl(void);
static void end_2d_gl(void);
#ifndef WITHOUTOPENGL
static void begin_3d_gl(double camera_look_heading)
{
	glEnable(GL_TEXTURE_2D);
	glPushMatrix();
	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glLoadIdentity();
	gluPerspective(33.0, (float) SCREEN_WIDTH / SCREEN_HEIGHT, 0.5, 8000.0);
	gluLookAt(0, 0, 0, cos(camera_look_heading), 0.0, sin(camera_look_heading),
				0.0, 1.0, 0.0); 
	glMatrixMode(GL_MODELVIEW);
	/* glDisable(GL_DEPTH_TEST); */
}

static void end_3d_gl(void)
{
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glPopMatrix();
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	glPopMatrix();
}
#endif

static void render_skybox(GtkWidget *w, double camera_look_heading)
{

	/* TODO:  Probably there is a better way than creating these every frame */

#ifndef WITHOUTOPENGL
#define SKYBOXRAD 200.0
	static const float v[8][3] = {
		{ -SKYBOXRAD, SKYBOXRAD, -SKYBOXRAD },
		{ SKYBOXRAD, SKYBOXRAD, -SKYBOXRAD },
		{ SKYBOXRAD, -SKYBOXRAD, -SKYBOXRAD },
		{ -SKYBOXRAD, -SKYBOXRAD, -SKYBOXRAD },
		{ -SKYBOXRAD, SKYBOXRAD, SKYBOXRAD },
		{ SKYBOXRAD, SKYBOXRAD, SKYBOXRAD },
		{ SKYBOXRAD, -SKYBOXRAD, SKYBOXRAD },
		{ -SKYBOXRAD, -SKYBOXRAD, SKYBOXRAD },
	};

	static const float n[6][3] = {
		{ 0.0, 0.0, 1.0 },
		{ 1.0, 0.0, 0.0 },
		{ 0.0, -1.0, 0.0 },
		{ -1.0, 0.0, 0.0 },
		{ 0.0, 1.0, 0.0 },
		{ 0.0, 0.0, -1.0 },
	};

	end_2d_gl();
	begin_3d_gl(camera_look_heading);

	glColor4ub(255, 255, 255, 255);
	const static GLfloat light0_position[] = {1.0, 1.0, 1.0, 0.0};
	glLightfv(GL_LIGHT0, GL_POSITION, light0_position);
	glEnable(GL_DEPTH_TEST);    
	/* glEnable(GL_LIGHTING);
	glEnable(GL_LIGHT0); */
	glEnable(GL_TEXTURE_2D);
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

	glBindTexture(GL_TEXTURE_2D, texture[2]);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

	glBegin(GL_QUADS);
	glNormal3fv(&n[0][0]);
	glTexCoord2f(1, 0); glVertex3fv(&v[1][0]);
	glTexCoord2f(0, 0); glVertex3fv(&v[0][0]);
	glTexCoord2f(0, 1); glVertex3fv(&v[3][0]);
	glTexCoord2f(1, 1); glVertex3fv(&v[2][0]);
	glEnd();

	glBindTexture(GL_TEXTURE_2D, texture[1]);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

	glBegin(GL_QUADS);
	glNormal3fv(&n[1][0]);
	glTexCoord2f(1, 0); glVertex3fv(&v[0][0]);
	glTexCoord2f(1, 1); glVertex3fv(&v[3][0]);
	glTexCoord2f(0, 1); glVertex3fv(&v[7][0]);
	glTexCoord2f(0, 0); glVertex3fv(&v[4][0]);
	glEnd();

	glBindTexture(GL_TEXTURE_2D, texture[4]);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

	glBegin(GL_QUADS);
	glNormal3fv(&n[2][0]);
	glTexCoord2f(0, 0); glVertex3fv(&v[1][0]);
	glTexCoord2f(1, 0); glVertex3fv(&v[0][0]);
	glTexCoord2f(1, 1); glVertex3fv(&v[4][0]);
	glTexCoord2f(0, 1); glVertex3fv(&v[5][0]);
	glEnd();

	glBindTexture(GL_TEXTURE_2D, texture[3]);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

	glBegin(GL_QUADS);
	glNormal3fv(&n[3][0]);
	glTexCoord2f(1, 0); glVertex3fv(&v[5][0]);
	glTexCoord2f(0, 0); glVertex3fv(&v[1][0]);
	glTexCoord2f(0, 1); glVertex3fv(&v[2][0]);
	glTexCoord2f(1, 1); glVertex3fv(&v[6][0]);
	glEnd();

	glBindTexture(GL_TEXTURE_2D, texture[5]);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

	glBegin(GL_QUADS);
	glNormal3fv(&n[4][0]);
	glTexCoord2f(0, 0); glVertex3fv(&v[3][0]);
	glTexCoord2f(1, 0); glVertex3fv(&v[2][0]);
	glTexCoord2f(1, 1); glVertex3fv(&v[6][0]);
	glTexCoord2f(0, 1); glVertex3fv(&v[7][0]);
	glEnd();


	glBindTexture(GL_TEXTURE_2D, texture[0]);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

	glBegin(GL_QUADS);
	glNormal3fv(&n[5][0]);
	glTexCoord2f(1, 0); glVertex3fv(&v[4][0]);
	glTexCoord2f(0, 0); glVertex3fv(&v[5][0]);
	glTexCoord2f(0, 1); glVertex3fv(&v[6][0]);
	glTexCoord2f(1, 1); glVertex3fv(&v[7][0]);
	glEnd();

	glDisable(GL_DEPTH_TEST);    
	glDisable(GL_LIGHTING);
	glDisable(GL_LIGHT0);

	end_3d_gl();
	begin_2d_gl();
#endif
}

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
	snis_draw_line(w->window, gc, x1, cy, x1 + 25, cy);
	snis_draw_line(w->window, gc, x2 - 25, cy, x2, cy);
	snis_draw_line(w->window, gc, cx, y1, cx, y1 + 25);
	snis_draw_line(w->window, gc, cx, y2 - 25, cx, y2);
}

static void show_mainscreen(GtkWidget *w)
{
	static int fake_stars_initialized = 0;
	struct snis_entity *o;
	float cx, cy, cz, lx, ly;
	double camera_look_heading;


	if (!(o = find_my_ship()))
		return;
	if (o->tsd.ship.view_mode == MAINSCREEN_VIEW_MODE_NORMAL)
		camera_look_heading = o->heading + o->tsd.ship.view_angle;
	else
		camera_look_heading = o->tsd.ship.gun_heading;

	render_skybox(w, camera_look_heading);

	show_mainscreen_starfield(w, camera_look_heading);

	cx = (float) o->x;
	cy = (float) -o->y;
	cz = -10.0;
	lx = cx + sin(camera_look_heading) * 500.0;
	ly = cy + cos(camera_look_heading) * 500.0;
	camera_set_pos(ecx, cx, (float) cz, cy);
	camera_look_at(ecx, lx, (float) 0.0, ly);
	camera_set_parameters(ecx, (float) 20, (float) 300, (float) 16, (float) 12,
				SCREEN_WIDTH, SCREEN_HEIGHT, ANGLE_OF_VIEW);
	set_lighting(ecx, 0, sin(((timer / 4) % 360) * M_PI / 180),
			cos(((timer / 4) % 360) * M_PI / 180));
	sng_set_foreground(GREEN);
	if (!fake_stars_initialized) {
		fake_stars_initialized = 1;
		entity_init_fake_stars(ecx, 2000, 300.0f * 10.0f);
	}
	pthread_mutex_lock(&universe_mutex);
	render_entities(w, gc, ecx);
	if (o->tsd.ship.view_mode == MAINSCREEN_VIEW_MODE_WEAPONS)
		show_gunsight(w);
	pthread_mutex_unlock(&universe_mutex);
	show_common_screen(w, "");	
}

static void snis_draw_torpedo(GdkDrawable *drawable, GdkGC *gc, gint x, gint y, gint r)
{
	int i, dx, dy;

	sng_set_foreground(WHITE);
	for (i = 0; i < 10; i++) {
		dx = x + snis_randn(r * 2) - r; 
		dy = y + snis_randn(r * 2) - r; 
		snis_draw_line(drawable, gc, x, y, dx, dy);
	}
	/* sng_draw_circle(drawable, gc, x, y, (int) (SCREEN_WIDTH * 150.0 / XKNOWN_DIM)); */
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
					gint x, gint y, double dist, int bw, int pwr,
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
			float r;
			da = snis_randn(360) * M_PI / 180.0;
			tx = (int) ((double) x + sin(da) * (double) snis_randn(dr));
			ty = (int) ((double) y + cos(da) * (double) snis_randn(dr)); 

			r = hypot(tx - SCIENCE_SCOPE_CX, ty - SCIENCE_SCOPE_CY);
			if (r >= SCIENCE_SCOPE_R)
				continue;
			sng_draw_point(w->window, gc, tx, ty);
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
			snis_draw_arrow(w, gc, x, y, SCIENCE_SCOPE_R / 2,
				o->heading, 0.3);
		} else {
			snis_draw_line(w->window, gc, x - 1, y, x + 1, y);
			snis_draw_line(w->window, gc, x, y - 1, x, y + 1);
		}
	}

	if (selected)
		sng_draw_circle(w->window, gc, x, y, 10);
	
	if (o->sdata.science_data_known) {
		switch (o->type) {
		case OBJTYPE_SHIP2:
		case OBJTYPE_SHIP1:
			sng_set_foreground(LIMEGREEN);
			sprintf(buffer, "%s %s\n", o->sdata.name, shipclass[o->sdata.subclass]); 
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
		sng_abs_xy_draw_string(w, gc, buffer, PICO_FONT, x + 8, y - 8);
	}
}

static void snis_draw_science_spark(GdkDrawable *drawable, GdkGC *gc, gint x, gint y, double dist)
{
	int i;

	double da;
	int dr;
	double tx, ty;

	dr = (int) dist / (XKNOWN_DIM / 100.0);
	for (i = 0; i < 20; i++) {
		da = snis_randn(360) * M_PI / 180.0;
#if 1
		tx = (int) ((double) x + sin(da) * (double) snis_randn(dr));
		ty = (int) ((double) y + cos(da) * (double) snis_randn(dr)); 
#else
		tx = x;
		ty = y;
#endif
		sng_draw_point(drawable, gc, tx, ty);
	}
}

static void snis_draw_arrow(GtkWidget *w, GdkGC *gc, gint x, gint y, gint r,
		double heading, double scale)
{
	int nx, ny, tx1, ty1, tx2, ty2;

	/* Draw ship... */
#define SHIP_SCALE_DOWN 15.0
	nx = sin(heading) * scale * r / SHIP_SCALE_DOWN;
	ny = -cos(heading) * scale * r / SHIP_SCALE_DOWN;
	snis_draw_line(w->window, gc, x, y, x + nx, y + ny);
	tx1 = sin(heading + PI / 2.0) * scale * r / (SHIP_SCALE_DOWN * 2.0) - nx / 2.0;
	ty1 = -cos(heading + PI / 2.0) * scale * r / (SHIP_SCALE_DOWN * 2.0) - ny / 2.0;
	snis_draw_line(w->window, gc, x, y, x + tx1, y + ty1);
	tx2 = sin(heading - PI / 2.0) * scale * r / (SHIP_SCALE_DOWN * 2.0) - nx / 2.0;
	ty2 = -cos(heading - PI / 2.0) * scale * r / (SHIP_SCALE_DOWN * 2.0) - ny / 2.0;
	snis_draw_line(w->window, gc, x, y, x + tx2, y + ty2);
	snis_draw_line(w->window, gc, x + nx, y + ny, x + tx1, y + ty1);
	snis_draw_line(w->window, gc, x + tx1, y + ty1, x + tx2, y + ty2);
	snis_draw_line(w->window, gc, x + tx2, y + ty2, x + nx, y + ny);
}

static void draw_degree_marks_with_labels(GtkWidget *w, GdkGC *gc,
		gint x, gint y, gint r, int font)
{
	char buf[10];
	int i;

	for (i = 0; i < 36; i++) { /* 10 degree increments */
		int x3, y3;
		int x1 = (int) (cos((10.0 * i) * M_PI / 180.0) * r);
		int y1 = (int) (sin((10.0 * i) * M_PI / 180.0) * r);
		int x2 = x1 * 0.25;
		int y2 = y1 * 0.25;
		x3 = x1 * 1.08 + x - 15;
		y3 = y1 * 1.08 + y;
		x1 += x;
		x2 += x;
		y1 += y;
		y2 += y;
		sng_draw_dotted_line(w->window, gc, x1, y1, x2, y2);
		sprintf(buf, "%3d", (90 + i * 10) % 360);
		sng_abs_xy_draw_string(w, gc, buf, font, x3, y3);
	}
}

static void snis_draw_science_reticule(GtkWidget *w, GdkGC *gc, gint x, gint y, gint r,
		double heading, double beam_width)
{
	int tx1, ty1, tx2, ty2;

	sng_draw_circle(w->window, gc, x, y, r);
	draw_degree_marks_with_labels(w, gc, x, y, r, NANO_FONT);
	/* draw the ship */
	snis_draw_arrow(w, gc, x, y, r, heading, 1.0);

	tx1 = x + sin(heading - beam_width / 2) * r * 0.05;
	ty1 = y - cos(heading - beam_width / 2) * r * 0.05;
	tx2 = x + sin(heading - beam_width / 2) * r;
	ty2 = y - cos(heading - beam_width / 2) * r;
	sng_set_foreground(GREEN);
	sng_draw_electric_line(w->window, gc, tx1, ty1, tx2, ty2);
	tx1 = x + sin(heading + beam_width / 2) * r * 0.05;
	ty1 = y - cos(heading + beam_width / 2) * r * 0.05;
	tx2 = x + sin(heading + beam_width / 2) * r;
	ty2 = y - cos(heading + beam_width / 2) * r;
	sng_draw_electric_line(w->window, gc, tx1, ty1, tx2, ty2);
}

static void snis_draw_heading_on_reticule(GtkWidget *w, GdkGC *gc, gint x, gint y, gint r,
			double heading, int color, int dotted)
{
	int tx1, ty1, tx2, ty2;

	tx1 = x + sin(heading) * r * 0.85;
	ty1 = y - cos(heading) * r * 0.85;
	tx2 = x + sin(heading) * r;
	ty2 = y - cos(heading) * r;
	sng_set_foreground(color);
	snis_draw_line(w->window, gc, tx1, ty1, tx2, ty2);
	if (dotted)
		sng_draw_dotted_line(w->window, gc, x, y, tx2, ty2);
}

static void snis_draw_headings_on_reticule(GtkWidget *w, GdkGC *gc, gint x, gint y, gint r,
		struct snis_entity *o)
{
	snis_draw_heading_on_reticule(w, gc, x, y, r, o->heading, RED,
			displaymode == DISPLAYMODE_NAVIGATION);
	snis_draw_heading_on_reticule(w, gc, x, y, r, o->tsd.ship.gun_heading, DARKTURQUOISE,
			displaymode == DISPLAYMODE_WEAPONS);
	snis_draw_heading_on_reticule(w, gc, x, y, r, o->tsd.ship.sci_heading, GREEN, 0);
}

static void snis_draw_ship_on_reticule(GtkWidget *w, GdkGC *gc, gint x, gint y, gint r,
		struct snis_entity *o)
{
	sng_set_foreground(CYAN);
	snis_draw_arrow(w, gc, x, y, r, o->heading, 1.5);
	snis_draw_arrow(w, gc, x, y, r, o->tsd.ship.gun_heading, 0.75);
}

static void snis_draw_reticule(GtkWidget *w, GdkGC *gc, gint x, gint y, gint r,
		double heading, int c1, int c2)
{
	int i;

	sng_set_foreground(c1);
	for (i = r; i > r / 4; i -= r / 5)
		sng_draw_circle(w->window, gc, x, y, i);
	sng_set_foreground(c2);
	draw_degree_marks_with_labels(w, gc, x, y, r, NANO_FONT);
}

static int within_nebula(double x, double y)
{
	double dist2;
	int i;

	for (i = 0; i < nnebula; i++) {
		dist2 = (x - nebulaentry[i].x) *
			(x - nebulaentry[i].x) +
			(y - nebulaentry[i].y) *
			(y - nebulaentry[i].y);
		if (dist2 < nebulaentry[i].r2)
			return 1;
	}
	return 0;
}

static void draw_nebula_noise(GtkWidget *w, int cx, int cy, int r)
{
	int i, radius, x1, y1;
	double angle;

	sng_set_foreground(WHITE);
	for (i = 0; i < 50; i++) {
		angle = 0.001 * snis_randn(1000) * 2 * M_PI;
		radius = snis_randn(r);
		x1 = cos(angle) * radius + cx;
		y1 = sin(angle) * radius + cy;
		snis_draw_line(w->window, gc, x1, y1, x1 + 1, y1);
       }
}

struct snis_radar_extent {
	int rx, ry, rw, rh;
};

static double radarx_to_ux(struct snis_entity *o,
		double x, struct snis_radar_extent *extent, double screen_radius)
{
	int cx = extent->rx + (extent->rw / 2);

	return screen_radius * (x - cx) / ((double) extent->rh / 2.0) + o->x;
}

static double radary_to_uy(struct snis_entity *o,
		double y, struct snis_radar_extent *extent, double screen_radius)
{
	int cy = extent->ry + (extent->rh / 2);

	return screen_radius * (y - cy) / ((double) extent->rh / 2.0) + o->y;
}

static void draw_targeting_indicator(GtkWidget *w, GdkGC *gc, int x, int y)
{
	int i;

	sng_set_foreground(ORANGERED);
	for (i = 0; i < 4; i++) {
		int x1, y1, x2, y2;
		double angle;
		double dx, dy, ddx, ddy;

		angle = (M_PI * ((i * 90 + timer * 4) % 360)) / 180.0;

		dx = 15.0 * cos(angle);
		dy = 15.0 * sin(angle);
		ddx = 5.0 * cos(angle + M_PI / 2.0);
		ddy = 5.0 * sin(angle + M_PI / 2.0);

		x1 = x + dx;
		y1 = y + dy;
		x2 = x + 2.0 * dx;
		y2 = y + 2.0 * dy;
#if 0
		x1 = (int) ((double) x + 10.0 * cos(angle));
		y1 = (int) ((double) y + 10.0 * sin(angle));
		x2 = (int) ((double) x + 20.0 * cos(angle));
		y2 = (int) ((double) y + 20.0 * sin(angle));
#endif
		/* snis_draw_line(w->window, gc, x1, y1, x2, y2); */
		snis_draw_line(w->window, gc, x2 - ddx, y2 - ddy, x2 + ddx, y2 + ddy);
		snis_draw_line(w->window, gc, x2 - ddx, y2 - ddy, x1, y1);
		snis_draw_line(w->window, gc, x1, y1, x2 + ddx, y2 + ddy);
	}
}

static void draw_torpedo_leading_indicator(GtkWidget *w, GdkGC *gc,
			struct snis_entity *ship, struct snis_entity *target,
			int x, int y, double dist2, int r, double screen_radius)
{
	double time_to_target, svx, svy, targx, targy;

	time_to_target = sqrt(dist2) / TORPEDO_VELOCITY;
	svx = target->vx * (double) r / screen_radius;
	svy = target->vy * (double) r / screen_radius;
	targx = x + svx * time_to_target;
	targy = y + svy * time_to_target;
	sng_set_foreground(ORANGERED);
	snis_draw_line(w->window, gc, targx - 5, targy, targx + 5, targy);
	snis_draw_line(w->window, gc, targx, targy - 5, targx, targy + 5);
}

static void draw_laserbeam(GtkWidget *w, GdkGC *gc, struct snis_entity *ship,
		double x1, double y1, double x2, double y2,
		struct snis_radar_extent *extent, double screen_radius, int r, int color)
{
	double ix1, iy1, ix2, iy2;
	int cx, cy, tx, ty, lx1, ly1, lx2, ly2;
	int nintersections;

	nintersections = circle_line_segment_intersection(x1, y1, x2, y2,
				ship->x, ship->y, screen_radius,
				&ix1, &iy1, &ix2, &iy2);

	if (nintersections == -1) /* no intersections, all points outside circle */
		return;

	/* otherwise, 3 cases, but do the same in all of them
	 * 1. no intersections, all points inside circle
	 * 2. 1 intersection, 1 point inside circle
	 * 3. 2 intersections 
	 *
	 * in any case, draw a line from *ix1,*iy1 to *ix2,*iy2
	 * scaling to radar coords first.
	 */
	cx = extent->rx + (extent->rw / 2);
	cy = extent->ry + (extent->rh / 2);
	tx = (ix1 - ship->x) * (double) r / screen_radius;
	ty = (iy1 - ship->y) * (double) r / screen_radius;
	lx1 = (int) (tx + (double) cx);
	ly1 = (int) (ty + (double) cy);
	tx = (ix2 - ship->x) * (double) r / screen_radius;
	ty = (iy2 - ship->y) * (double) r / screen_radius;
	lx2 = (int) (tx + (double) cx);
	ly2 = (int) (ty + (double) cy);
	sng_draw_laser_line(w->window, gc, lx1, ly1, lx2, ly2, color);
}

static void draw_all_the_guys(GtkWidget *w, struct snis_entity *o, struct snis_radar_extent* extent, double screen_radius,
				double visible_distance)
{
	int i, cx, cy, r, in_nebula;
	char buffer[200];

	visible_distance *= visible_distance;
	in_nebula = within_nebula(o->x, o->y);
	cx = extent->rx + (extent->rw / 2);
	cy = extent->ry + (extent->rh / 2);
	r = extent->rh / 2;
	sng_set_foreground(DARKRED);

	/* Draw all the stuff */
#define NR2 (screen_radius * screen_radius)
	pthread_mutex_lock(&universe_mutex);

	for (i = 0; i <= snis_object_pool_highest_object(pool); i++) {
		int x, y;
		double tx, ty, nx, ny;
		double dist2;

		if (!go[i].alive)
			continue;

		if (go[i].type == OBJTYPE_LASERBEAM || go[i].type == OBJTYPE_TRACTORBEAM) {
			int oid, tid, color;
			struct snis_entity *shooter = lookup_entity_by_id(go[i].tsd.laserbeam.origin);
			if (go[i].type == OBJTYPE_TRACTORBEAM) {
				color = TRACTORBEAM_COLOR;
			} else {
				if (shooter && shooter->type == OBJTYPE_SHIP2)
					color = NPC_LASER_COLOR;
				else
					color = PLAYER_LASER_COLOR;
			}

			oid = lookup_object_by_id(go[i].tsd.laserbeam.origin);
			tid = lookup_object_by_id(go[i].tsd.laserbeam.target);
			draw_laserbeam(w, gc, o, go[oid].x, go[oid].y, go[tid].x, go[tid].y,
					extent, screen_radius, r, color);
			continue;
		}
		nx = in_nebula * (0.01 * snis_randn(100) - 0.5) * 0.1 * screen_radius;
		ny = in_nebula * (0.01 * snis_randn(100) - 0.5) * 0.1 * screen_radius;
		dist2 = ((go[i].x - o->x + nx) * (go[i].x - o->x + nx)) +
			((go[i].y - o->y + ny) * (go[i].y - o->y + ny));
		if (dist2 > NR2 || dist2 > visible_distance)
			continue; /* not close enough */

		tx = (go[i].x + nx - o->x) * (double) r / screen_radius;
		ty = (go[i].y + ny - o->y) * (double) r / screen_radius;
		x = (int) (tx + (double) cx);
		y = (int) (ty + (double) cy);

		if (in_nebula && snis_randn(1000) < 850)
			continue;

		if (go[i].id == my_ship_id)
			continue; /* skip drawing yourself. */
		else {
			double alter_angle; /* FIXME this is an ugly hack */
			alter_angle = 0.0;
			switch (go[i].type) {
			case OBJTYPE_ASTEROID:
			case OBJTYPE_DERELICT:
				sng_set_foreground(ASTEROID_COLOR);
				sng_draw_circle(w->window, gc, x, y, r / 30);
				break;
			case OBJTYPE_PLANET:
				sng_set_foreground(PLANET_COLOR);
				sng_draw_circle(w->window, gc, x, y, r / 30);
				break;
			case OBJTYPE_STARBASE:
				sng_set_foreground(STARBASE_COLOR);
				sng_draw_circle(w->window, gc, x, y, r / 30);
				break;
			case OBJTYPE_WORMHOLE:
				sng_set_foreground(WORMHOLE_COLOR);
				sng_draw_circle(w->window, gc, x, y, r / 30);
				break;
			case OBJTYPE_LASER:
				sng_draw_laser_line(w->window, gc, x, y,
					x - go[i].vx * (double) r / (2 * screen_radius),
					y - go[i].vy * (double) r / (2 * screen_radius),
					NPC_LASER_COLOR);
				break;
			case OBJTYPE_TORPEDO:
				snis_draw_torpedo(w->window, gc, x, y, r / 30);
				break;
			case OBJTYPE_EXPLOSION:
				break;
			case OBJTYPE_SHIP2:
				alter_angle = M_PI / 2.0;
			case OBJTYPE_SHIP1:
				sng_set_foreground(WHITE);
				snis_draw_arrow(w, gc, x, y, r,
					go[i].heading + alter_angle, 0.5);
				sng_set_foreground(GREEN);
				if (go[i].sdata.science_data_known) {
					sprintf(buffer, "%s", go[i].sdata.name);
					sng_abs_xy_draw_string(w, gc, buffer, NANO_FONT, x + 10, y - 10);
				}
#if 0
				time_to_target = sqrt(dist2) / TORPEDO_VELOCITY;
				svx = go[i].vx * (double) r / screen_radius;
				svy = go[i].vy * (double) r / screen_radius;
				targx = x + svx * time_to_target;
				targy = y + svy * time_to_target;
				sng_set_foreground(ORANGERED);
				snis_draw_line(w->window, gc, targx - 5, targy, targx + 5, targy);
				snis_draw_line(w->window, gc, targx, targy - 5, targx, targy + 5);
#endif
				break;
			case OBJTYPE_SPACEMONSTER: /* invisible to instruments */
			case OBJTYPE_NEBULA:
				break;
			default:
				sng_set_foreground(WHITE);
				snis_draw_arrow(w, gc, x, y, r, go[i].heading, 0.5);
			}
			if (go[i].id == o->tsd.ship.victim_id) {
				draw_targeting_indicator(w, gc, x, y);
				draw_torpedo_leading_indicator(w, gc, o, &go[i],
								x, y, dist2, r, screen_radius);
			}
		}
	}
	pthread_mutex_unlock(&universe_mutex);
	if (in_nebula)
		draw_nebula_noise(w, cx, cy, r); 
}

/* this science_guy[] array is used for mouse clicking. */
struct science_data {
	int sx, sy; /* screen coords on scope */
	struct snis_entity *o;
};
static struct science_data science_guy[MAXGAMEOBJS] = { {0}, };
static int nscience_guys = 0;

static void draw_science_laserbeam(GtkWidget *w, GdkGC *gc, struct snis_entity *o,
		struct snis_entity *laserbeam, int cx, int cy, double r, double range)
{
	double tx1, ty1, tx2, ty2, ix1, iy1, ix2, iy2;
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
	tx1 = ((shooter->x - o->x) * (double) r / range) + (double) cx;
	ty1 = ((shooter->y - o->y) * (double) r / range) + (double) cy;
	tx2 = ((shootee->x - o->x) * (double) r / range) + (double) cx;
	ty2 = ((shootee->y - o->y) * (double) r / range) + (double) cy;

	rc = circle_line_segment_intersection(tx1, ty1, tx2, ty2,
			(double) cx, (double) cy, r, &ix1, &iy1, &ix2, &iy2);
	if (rc < 0)
		return;
	sng_draw_laser_line(w->window, gc, (int) tx1, (int) ty1, (int) tx2, (int) ty2, color);
}

static void draw_all_the_science_guys(GtkWidget *w, struct snis_entity *o, double range)
{
	int i, x, y, cx, cy, r, bw, pwr;
	double tx, ty, dist2, dist;
	int selected_guy_still_visible = 0;
	int nebula_factor = 0;

	cx = SCIENCE_SCOPE_CX;
	cy = SCIENCE_SCOPE_CY;
	r = SCIENCE_SCOPE_R;
	pwr = o->tsd.ship.power_data.sensors.i;
	/* Draw all the stuff */

	/* Draw selected coordinate */
	dist = hypot(o->x - o->sci_coordx, o->y - o->sci_coordy);
	if (dist < range) {
		tx = (o->sci_coordx - o->x) * (double) r / range;
		ty = (o->sci_coordy - o->y) * (double) r / range;
		x = (int) (tx + (double) cx);
		y = (int) (ty + (double) cy);
		snis_draw_line(w->window, gc, x - 5, y, x + 5, y);
		snis_draw_line(w->window, gc, x, y - 5, x, y + 5);
	}

        tx = sin(o->tsd.ship.sci_heading) * range;
        ty = -cos(o->tsd.ship.sci_heading) * range;

	sng_set_foreground(GREEN);
	pthread_mutex_lock(&universe_mutex);
	nscience_guys = 0;
	for (i = 0; i <= snis_object_pool_highest_object(pool); i++) {
		if (!go[i].alive)
			continue;

		if (go[i].type == OBJTYPE_LASERBEAM || go[i].type == OBJTYPE_TRACTORBEAM) {
			draw_science_laserbeam(w, gc, o, &go[i], cx, cy, r, range);
			continue;
		}

		dist2 = ((go[i].x - o->x) * (go[i].x - o->x)) +
			((go[i].y - o->y) * (go[i].y - o->y));
		if (go[i].type == OBJTYPE_NEBULA) {
			if (dist2 < go[i].tsd.nebula.r * go[i].tsd.nebula.r)
				nebula_factor++;
			continue;
		}
		if (dist2 > range * range)
			continue; /* not close enough */
		dist = sqrt(dist2);

#if 0
		if (within_nebula(go[i].x, go[i].y))
			continue;
#endif

		tx = (go[i].x - o->x) * (double) r / range;
		ty = (go[i].y - o->y) * (double) r / range;
		x = (int) (tx + (double) cx);
		y = (int) (ty + (double) cy);

		if (go[i].id == my_ship_id)
			continue; /* skip drawing yourself. */
		bw = o->tsd.ship.sci_beam_width * 180.0 / M_PI;

		/* If we moved the beam off our guy, and back on, select him again. */
		if (!curr_science_guy && prev_science_guy == &go[i])
			curr_science_guy = prev_science_guy;

		snis_draw_science_guy(w, gc, &go[i], x, y, dist, bw, pwr, range, &go[i] == curr_science_guy, nebula_factor);

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
			snis_draw_line(w->window, gc,
				cx + cos(angle) * r,
				cy + sin(angle) * r,
				cx + cos(angle) * radius,
				cy + sin(angle) * radius);
		}
	}
}

static void draw_all_the_science_nebulae(GtkWidget *w, struct snis_entity *o, double range)
{
	int i, j;
	double dist2, dist, range2;
	double a, r, x, y, d2;
	int sx, sy, cx, cy;

	cx = SCIENCE_SCOPE_CX;	
	cy = SCIENCE_SCOPE_CY;	

	sng_set_foreground(GREEN);
	range2 = range * range;
	for (i = 0; i < nnebula; i++) {
		dist2 = (o->x - nebulaentry[i].x) *
			(o->x - nebulaentry[i].x) +
			(o->y - nebulaentry[i].y) *
			(o->y - nebulaentry[i].y);
		dist = sqrt(dist2);
		if (dist - nebulaentry[i].r > range)
			continue;
		for (j = 0; j < 80; j++) {
			a = snis_randn(360) * M_PI / 180.0;
			r = snis_randn((int) nebulaentry[i].r + 200);
			x = nebulaentry[i].x + r * cos(a);
			y = nebulaentry[i].y + r * sin(a);
			d2 = (x - o->x) * (x - o->x) +
				(y - o->y) * (y - o->y);
			if (d2 > range2)
				continue;
			sx = (x - o->x) * (double) SCIENCE_SCOPE_R / range + cx;
			sy = (y - o->y) * (double) SCIENCE_SCOPE_R / range + cy;
			d2 = (cx - sx) * (cx - sx) + (cy - sy) * (cy - sy);
			if (d2 > SCIENCE_SCOPE_R * SCIENCE_SCOPE_R)
				continue;
			sng_draw_point(w->window, gc, sx, sy);
		}
	}
}

static void draw_all_the_science_sparks(GtkWidget *w, struct snis_entity *o, double range)
{
	int i, cx, cy, r;

	cx = SCIENCE_SCOPE_CX;	
	cy = SCIENCE_SCOPE_CY;	
	r = SCIENCE_SCOPE_R;
	sng_set_foreground(DARKRED);
	/* Draw all the stuff */
	pthread_mutex_lock(&universe_mutex);

	for (i = 0; i <= snis_object_pool_highest_object(sparkpool); i++) {
		int x, y;
		double tx, ty;
		double dist2, dist;

		if (!spark[i].alive)
			continue;

		dist2 = ((spark[i].x - o->x) * (spark[i].x - o->x)) +
			((spark[i].y - o->y) * (spark[i].y - o->y));
		if (dist2 > range * range)
			continue; /* not close enough */
		dist = sqrt(dist2);
	

		tx = (spark[i].x - o->x) * (double) r / range;
		ty = (spark[i].y - o->y) * (double) r / range;
		x = (int) (tx + (double) cx);
		y = (int) (ty + (double) cy);

		sng_set_foreground(GREEN);
		snis_draw_science_spark(w->window, gc, x, y, dist);
	}
	pthread_mutex_unlock(&universe_mutex);
}


static void draw_all_the_sparks(GtkWidget *w, struct snis_entity *o, struct snis_radar_extent* extent, double screen_radius)
{
	int i, cx, cy, r;

	cx = extent->rx + (extent->rw / 2);
	cy = extent->ry + (extent->rh / 2);
	r = extent->rh / 2;
	sng_set_foreground(DARKRED);
	/* Draw all the stuff */
	pthread_mutex_lock(&universe_mutex);

	for (i = 0; i <= snis_object_pool_highest_object(sparkpool); i++) {
		int x, y;
		double tx, ty;
		double dist2;

		if (!spark[i].alive)
			continue;

		dist2 = ((spark[i].x - o->x) * (spark[i].x - o->x)) +
			((spark[i].y - o->y) * (spark[i].y - o->y));
		if (dist2 > NR2)
			continue; /* not close enough */

		tx = (spark[i].x - o->x) * (double) r / screen_radius;
		ty = (spark[i].y - o->y) * (double) r / screen_radius;
		x = (int) (tx + (double) cx);
		y = (int) (ty + (double) cy);

		sng_set_foreground(WHITE);
		snis_draw_line(w->window, gc, x - 1, y - 1, x + 1, y + 1);
		snis_draw_line(w->window, gc, x - 1, y + 1, x + 1, y - 1);
	}
	pthread_mutex_unlock(&universe_mutex);
}

static void snis_draw_dotted_hline(GdkDrawable *drawable,
         GdkGC *gc, int x1, int y1, int x2, int dots)
{
	int i;

	for (i = x1; i <= x2; i += dots)
		sng_draw_point(drawable, gc, i, y1);
}

static void snis_draw_dotted_vline(GdkDrawable *drawable,
         GdkGC *gc, int x1, int y1, int y2, int dots)
{
	int i;

	if (y2 > y1) {
		for (i = y1; i <= y2; i += dots)
			sng_draw_point(drawable, gc, x1, i);
	} else { 
		for (i = y2; i <= y1; i += dots)
			sng_draw_point(drawable, gc, x1, i);
	}
}

static void snis_draw_radar_sector_labels(GtkWidget *w,
         GdkGC *gc, struct snis_entity *o, int cx, int cy, int r, double range)
{
	/* FIXME, this algorithm is really fricken dumb. */
	int x, y;
	double xincrement = (XKNOWN_DIM / 10.0);
	double yincrement = (YKNOWN_DIM / 10.0);
	int x1, y1;
	const char *letters = "ABCDEFGHIJK";
	char label[10];
	int xoffset = 7;
	int yoffset = 10;

	for (x = 0; x < 10; x++) {
		if ((x * xincrement) <= (o->x - range * 0.9))
			continue;
		if ((x * xincrement) >= (o->x + range * 0.9))
			continue;
		for (y = 0; y < 10; y++) {
			if ((y * yincrement) <= (o->y - range * 0.9))
				continue;
			if ((y * yincrement) >= (o->y + range * 0.9))
				continue;
			x1 = (int) (((double) r) / range * (x * xincrement - o->x)) + cx + xoffset;
			y1 = (int) (((double) r) / range * (y * yincrement - o->y)) + cy + yoffset;
			snprintf(label, sizeof(label), "%c%d", letters[y], x);
			sng_abs_xy_draw_string(w, gc, label, NANO_FONT, x1, y1);
		}
	}
}

static void snis_draw_radar_grid(GdkDrawable *drawable,
         GdkGC *gc, struct snis_entity *o, int cx, int cy, int r, double range, int small_grids)
{
	/* FIXME, this algorithm is really fricken dumb. */
	int x, y;
	double increment = (XKNOWN_DIM / 10.0);
	double lx1, ly1, lx2, ly2;
	int x1, y1, x2, y2; 
	int xlow, xhigh, ylow, yhigh;
	double range2 = (range * range);

	xlow = (int) ((double) r / range * (0 -o->x)) + cx;
	xhigh = (int) ((double) r / range * (XKNOWN_DIM - o->x)) + cx;
	ylow = (int) (((double) r) / range * (0.0 - o->y)) + cy;
	yhigh = (int) (((double) r) / range * (YKNOWN_DIM - o->y)) + cy;
	/* vertical lines */
	increment = (XKNOWN_DIM / 10.0);
	for (x = 0; x <= 10; x++) {
		if ((x * increment) <= (o->x - range))
			continue;
		if ((x * increment) >= (o->x + range))
			continue;
		/* find y intersections with radar circle by pyth. theorem. */
		lx1 = x * increment - o->x;
		ly1 = sqrt((range2 - (lx1 * lx1)));
		ly2 = -ly1;

		x1 = (int) (((double) r) / range * lx1) + cx;
		y1 = (int) (((double) r) / range * ly1) + cy;
		y2 = (int) (((double) r) / range * ly2) + cy;

		if (y1 < ylow)
			y1 = ylow;
		if (y1 > yhigh)
			y1 = yhigh;
		if (y2 < ylow)
			y2 = ylow;
		if (y2 > yhigh)
			y2 = yhigh;
		snis_draw_dotted_vline(drawable, gc, x1, y2, y1, 5);
	}
	/* horizontal lines */
	increment = (YKNOWN_DIM / 10.0);
	for (y = 0; y <= 10; y++) {
		if ((y * increment) <= (o->y - range))
			continue;
		if ((y * increment) >= (o->y + range))
			continue;
		/* find x intersections with radar circle by pyth. theorem. */
		ly1 = y * increment - o->y;
		lx1 = sqrt((range2 - (ly1 * ly1)));
		lx2 = -lx1;
		y1 = (int) (((double) r) / range * ly1) + cy;
		x1 = (int) (((double) r) / range * lx1) + cx;
		x2 = (int) (((double) r) / range * lx2) + cx;
		if (x1 < xlow)
			x1 = xlow;
		if (x1 > xhigh)
			x1 = xhigh;
		if (x2 < xlow)
			x2 = xlow;
		if (x2 > xhigh)
			x2 = xhigh;
		snis_draw_dotted_hline(drawable, gc, x2, y1, x1, 5);
	}

	if (!small_grids)
		return;

	increment = (XKNOWN_DIM / 100.0);
	/* vertical lines */
	for (x = 0; x <= 100; x++) {
		if ((x * increment) <= (o->x - range))
			continue;
		if ((x * increment) >= (o->x + range))
			continue;
		/* find y intersections with radar circle by pyth. theorem. */
		lx1 = x * increment - o->x;
		ly1 = sqrt((range2 - (lx1 * lx1)));
		ly2 = -ly1;

		x1 = (int) (((double) r) / range * lx1) + cx;
		y1 = (int) (((double) r) / range * ly1) + cy;
		y2 = (int) (((double) r) / range * ly2) + cy;
		if (y1 < ylow)
			y1 = ylow;
		if (y1 > yhigh)
			y1 = yhigh;
		if (y2 < ylow)
			y2 = ylow;
		if (y2 > yhigh)
			y2 = yhigh;
		snis_draw_dotted_vline(drawable, gc, x1, y2, y1, 10);
	}
	/* horizontal lines */
	increment = (YKNOWN_DIM / 100.0);
	for (y = 0; y <= 100; y++) {
		if ((y * increment) <= (o->y - range))
			continue;
		if ((y * increment) >= (o->y + range))
			continue;
		/* find x intersections with radar circle by pyth. theorem. */
		ly1 = y * increment - o->y;
		lx1 = sqrt((range2 - (ly1 * ly1)));
		lx2 = -lx1;
		y1 = (int) (((double) r) / range * ly1) + cy;
		x1 = (int) (((double) r) / range * lx1) + cx;
		x2 = (int) (((double) r) / range * lx2) + cx;
		if (x1 < xlow)
			x1 = xlow;
		if (x1 > xhigh)
			x1 = xhigh;
		if (x2 < xlow)
			x2 = xlow;
		if (x2 > xhigh)
			x2 = xhigh;
		snis_draw_dotted_hline(drawable, gc, x2, y1, x1, 10);
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
	queue_to_server(packed_buffer_new("h", OPCODE_LOAD_TORPEDO));
}

static void fire_phaser_button_pressed(__attribute__((unused)) void *notused)
{
	do_laser();
}

static void fire_torpedo_button_pressed(__attribute__((unused)) void *notused)
{
	do_torpedo();
}

static void tractor_beam_button_pressed(__attribute__((unused)) void *notused)
{
	do_tractor_beam();
}

static void do_adjust_byte_value(uint8_t value,  uint16_t opcode)
{
	struct snis_entity *o;

	if (!(o = find_my_ship()))
		return;
	queue_to_server(packed_buffer_new("hwb", opcode, o->id, value));
}

static void do_adjust_slider_value(struct slider *s,  uint16_t opcode)
{
	uint8_t value = (uint8_t) (255.0 * snis_slider_get_input(s));
	do_adjust_byte_value(value, opcode);
}

static void do_weapzoom(struct slider *s)
{
	do_adjust_slider_value(s, OPCODE_REQUEST_WEAPZOOM);
}

static void do_navzoom(struct slider *s)
{
	do_adjust_slider_value(s, OPCODE_REQUEST_NAVZOOM);
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
DEFINE_SAMPLER_FUNCTION(sample_weapzoom, tsd.ship.weapzoom, 255.0, 0.0)
DEFINE_SAMPLER_FUNCTION(sample_navzoom, tsd.ship.navzoom, 255.0, 0.0)

static double sample_power_model_voltage(void)
{
	struct snis_entity *o;

	if (!(o = find_my_ship()))
		return 0.0;

	return 100.0 * o->tsd.ship.power_data.voltage / 255.0;
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

	return 100.0 * total_current / (255.0 * 3.5); 
}

#define DEFINE_CURRENT_SAMPLER(name) \
static double sample_##name##_current(void) \
{ \
	struct snis_entity *o; \
\
	if (!(o = find_my_ship())) \
		return 0.0; \
	return o->tsd.ship.power_data.name.i; \
}

DEFINE_CURRENT_SAMPLER(warp) /* defines sample_warp_current */
DEFINE_CURRENT_SAMPLER(sensors) /* defines sample_sensors_current */
DEFINE_CURRENT_SAMPLER(phasers) /* defines sample_phasers_current */
DEFINE_CURRENT_SAMPLER(maneuvering) /* defines sample_maneuvering_current */
DEFINE_CURRENT_SAMPLER(shields) /* defines sample_shields_current */
DEFINE_CURRENT_SAMPLER(comms) /* defines sample_comms_current */
DEFINE_CURRENT_SAMPLER(impulse) /* defines sample_impulse_current */
DEFINE_CURRENT_SAMPLER(tractor) /* defines sample_tractor_current */

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

static void wavelen_up_button_pressed(__attribute__((unused)) void *s)
{
	wavelen_updown_button_pressed(1);
}

static void wavelen_down_button_pressed(__attribute__((unused)) void *s)
{
	wavelen_updown_button_pressed(-1);
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

#define CREATE_DAMAGE_SAMPLER_FUNC(fieldname) \
	static double sample_##fieldname(void) \
	{ \
		return sample_generic_damage_data(offsetof(struct ship_damage_data, fieldname)); \
	}

CREATE_DAMAGE_SAMPLER_FUNC(shield_damage) /* sample_shield_damage defined here */
CREATE_DAMAGE_SAMPLER_FUNC(impulse_damage) /* sample_impulse_damage defined here */
CREATE_DAMAGE_SAMPLER_FUNC(warp_damage) /* sample_warp_damage defined here */
CREATE_DAMAGE_SAMPLER_FUNC(torpedo_tubes_damage) /* sample_torpedo_tubes_damage defined here */
CREATE_DAMAGE_SAMPLER_FUNC(phaser_banks_damage) /* sample_phaser_banks_damage defined here */
CREATE_DAMAGE_SAMPLER_FUNC(sensors_damage) /* sample_sensors_damage defined here */
CREATE_DAMAGE_SAMPLER_FUNC(comms_damage) /* sample_comms_damage defined here */
CREATE_DAMAGE_SAMPLER_FUNC(tractor_damage) /* sample_tractor_damage defined here */

static struct navigation_ui {
	struct slider *warp_slider;
	struct slider *shield_slider;
	struct slider *navzoom_slider;
	struct slider *throttle_slider;
	struct gauge *warp_gauge;
	struct button *engage_warp_button;
	struct button *warp_up_button;
	struct button *warp_down_button;
	struct button *reverse_button;
} nav_ui;

static void zero_nav_sliders(void)
{
	snis_slider_set_input(nav_ui.warp_slider, 0);	
	snis_slider_set_input(nav_ui.shield_slider, 0);	
	snis_slider_set_input(nav_ui.navzoom_slider, 0);	
}

static void engage_warp_button_pressed(__attribute__((unused)) void *cookie)
{
	do_adjust_byte_value(0,  OPCODE_ENGAGE_WARP);
}

static void warp_updown_button_pressed(int direction)
{
	int value;
	struct snis_entity *o;
	double inc;

	if (!(o = find_my_ship()))
		return;
	inc = 2.5;
	value = o->tsd.ship.requested_warpdrive;
	if (direction > 0 && value + inc > 255)
		return;
	if (direction < 0 && value - inc < 0)
		return;
	value += direction < 0 ? -inc : direction > 0 ? inc : 0;
	snis_slider_set_input(nav_ui.warp_slider, (double) value / 255.0);
	do_adjust_slider_value(nav_ui.warp_slider, OPCODE_REQUEST_WARPDRIVE);
}

static void warp_up_button_pressed(__attribute__((unused)) void *s)
{
	warp_updown_button_pressed(1);
}

static void warp_down_button_pressed(__attribute__((unused)) void *s)
{
	warp_updown_button_pressed(-1);
}

static void reverse_button_pressed(__attribute__((unused)) void *s)
{
	struct snis_entity *o;

	if (!(o = find_my_ship()))
		return;
	snis_button_set_color(nav_ui.reverse_button, !o->tsd.ship.reverse ? RED : AMBER);
	do_adjust_byte_value(!o->tsd.ship.reverse,  OPCODE_REQUEST_REVERSE);
}

struct weapons_ui {
	struct button *fire_torpedo, *load_torpedo, *fire_phaser, *tractor_beam;
	struct gauge *phaser_bank_gauge;
	struct gauge *phaser_wavelength;
	struct slider *wavelen_slider;
	struct slider *weapzoom_slider;
	struct button *wavelen_up_button;
	struct button *wavelen_down_button;
} weapons;

static void zero_weapons_sliders(void)
{
	snis_slider_set_input(weapons.weapzoom_slider, 0);
	snis_slider_set_input(weapons.wavelen_slider, 0);
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
	int y = 440;

	weapons.fire_phaser = snis_button_init(550, y, 200, 25, "FIRE PHASER", RED,
			TINY_FONT, fire_phaser_button_pressed, NULL);
	y += 35;
	weapons.load_torpedo = snis_button_init(550, y, 200, 25, "LOAD TORPEDO", GREEN,
			TINY_FONT, load_torpedo_button_pressed, NULL);
	y += 35;
	weapons.fire_torpedo = snis_button_init(550, y, 200, 25, "FIRE TORPEDO", RED,
			TINY_FONT, fire_torpedo_button_pressed, NULL);
	y += 35;
	weapons.tractor_beam = snis_button_init(550, y, 200, 25, "TRACTOR BEAM", RED,
			TINY_FONT, tractor_beam_button_pressed, NULL);
	weapons.phaser_bank_gauge = gauge_init(650, 100, 90, 0.0, 100.0, -120.0 * M_PI / 180.0,
			120.0 * 2.0 * M_PI / 180.0, RED, WHITE,
			10, "CHARGE", sample_phasercharge);
	weapons.phaser_wavelength = gauge_init(650, 300, 90, 10.0, 60.0, -120.0 * M_PI / 180.0,
			120.0 * 2.0 * M_PI / 180.0, RED, WHITE,
			10, "WAVE LEN", sample_phaser_wavelength);
	weapons.wavelen_down_button = snis_button_init(550, 400, 60, 25, "DOWN", WHITE,
			NANO_FONT, wavelen_down_button_pressed, NULL);
	weapons.wavelen_up_button = snis_button_init(700, 400, 30, 25, "UP", WHITE,
			NANO_FONT, wavelen_up_button_pressed, NULL);
	weapons.wavelen_slider = snis_slider_init(620, 400, 70, AMBER, "",
				"10", "60", 10, 60, sample_phaser_wavelength,
				do_phaser_wavelength);
	weapons.weapzoom_slider = snis_slider_init(5, SCREEN_HEIGHT - 20, 160, AMBER, "ZOOM",
				"1", "10", 0.0, 100.0, sample_weapzoom,
				do_weapzoom);
	ui_add_button(weapons.fire_phaser, DISPLAYMODE_WEAPONS);
	ui_add_button(weapons.load_torpedo, DISPLAYMODE_WEAPONS);
	ui_add_button(weapons.fire_torpedo, DISPLAYMODE_WEAPONS);
	ui_add_button(weapons.tractor_beam, DISPLAYMODE_WEAPONS);
	ui_add_button(weapons.wavelen_up_button, DISPLAYMODE_WEAPONS);
	ui_add_button(weapons.wavelen_down_button, DISPLAYMODE_WEAPONS);
	ui_add_slider(weapons.wavelen_slider, DISPLAYMODE_WEAPONS);
	ui_add_slider(weapons.weapzoom_slider, DISPLAYMODE_WEAPONS);
	ui_add_gauge(weapons.phaser_bank_gauge, DISPLAYMODE_WEAPONS);
	ui_add_gauge(weapons.phaser_wavelength, DISPLAYMODE_WEAPONS);
}

static void show_death_screen(GtkWidget *w)
{
	char buf[100];

	sng_set_foreground(RED);
	sprintf(buf, "YOUR SHIP");
	sng_abs_xy_draw_string(w, gc, buf, BIG_FONT, 20, 150);
	sprintf(buf, "HAS BEEN");
	sng_abs_xy_draw_string(w, gc, buf, BIG_FONT, 20, 250);
	sprintf(buf, "BLOWN TO");
	sng_abs_xy_draw_string(w, gc, buf, BIG_FONT, 20, 350);
	sprintf(buf, "SMITHEREENS");
	sng_abs_xy_draw_string(w, gc, buf, BIG_FONT, 20, 450);
	sprintf(buf, "RESPAWNING IN %d SECONDS", go[my_ship_oid].respawn_time);
	sng_abs_xy_draw_string(w, gc, buf, TINY_FONT, 20, 500);
}

static void show_weapons(GtkWidget *w)
{
	char buf[100];
	struct snis_entity *o;
	int cx, cy;
	int r;
	int buttoncolor;
	double screen_radius;
	double max_possible_screen_radius;
	double visible_distance;

	sng_set_foreground(GREEN);

	if (!(o = find_my_ship()))
		return;
	sprintf(buf, "PHOTON TORPEDOES: %03d", o->tsd.ship.torpedoes);
	sng_abs_xy_draw_string(w, gc, buf, NANO_FONT, 250, 15);
	sprintf(buf, "TORPEDOES LOADED: %03d", o->tsd.ship.torpedoes_loaded);
	sng_abs_xy_draw_string(w, gc, buf, NANO_FONT, 250, 15 + 0.5 * LINEHEIGHT);
	sprintf(buf, "TORPEDOES LOADING: %03d", o->tsd.ship.torpedoes_loading);
	sng_abs_xy_draw_string(w, gc, buf, NANO_FONT, 250, 15 + LINEHEIGHT);
/*
	sprintf(buf, "vx: %5.2lf", o->vx);
	sng_abs_xy_draw_string(w, gc, buf, TINY_FONT, 600, LINEHEIGHT * 3);
	sprintf(buf, "vy: %5.2lf", o->vy);
	sng_abs_xy_draw_string(w, gc, buf, TINY_FONT, 600, LINEHEIGHT * 4);
*/

	buttoncolor = RED;
	if (o->tsd.ship.torpedoes > 0 && o->tsd.ship.torpedoes_loading == 0 &&
		o->tsd.ship.torpedoes_loaded < 2)
		buttoncolor = GREEN;
	snis_button_set_color(weapons.load_torpedo, buttoncolor);
	buttoncolor = RED;
	if (o->tsd.ship.torpedoes_loaded)
		buttoncolor = GREEN;
	snis_button_set_color(weapons.fire_torpedo, buttoncolor);

	static struct snis_radar_extent extent = { 40, 90, 470, 470 };
	cx = extent.rx + (extent.rw / 2);
	cy = extent.ry + (extent.rh / 2);
	r = extent.rh / 2;
	sng_set_foreground(GREEN);
	screen_radius = ((((255.0 - o->tsd.ship.weapzoom) / 255.0) * 0.08) + 0.01) * XKNOWN_DIM;
	max_possible_screen_radius = 0.09 * XKNOWN_DIM;
	visible_distance = (max_possible_screen_radius * o->tsd.ship.power_data.sensors.i) / 255.0;
	snis_draw_radar_sector_labels(w, gc, o, cx, cy, r, screen_radius);
	snis_draw_radar_grid(w->window, gc, o, cx, cy, r, screen_radius, o->tsd.ship.weapzoom > 100);
	sng_set_foreground(BLUE);
	snis_draw_reticule(w, gc, cx, cy, r, o->tsd.ship.gun_heading, BLUE, BLUE);
	snis_draw_headings_on_reticule(w, gc, cx, cy, r, o);
	snis_draw_ship_on_reticule(w, gc, cx, cy, r, o);

	draw_all_the_guys(w, o, &extent, screen_radius, visible_distance);
	draw_all_the_sparks(w, o, &extent, screen_radius);
	show_common_screen(w, "WEAPONS");
}

static double sample_warpdrive(void);
static void init_nav_ui(void)
{
	nav_ui.shield_slider = snis_slider_init(540, 270, 160, AMBER, "SHIELDS",
				"0", "100", 0.0, 255.0, sample_shields_current,
				do_shieldadj);
	nav_ui.warp_slider = snis_slider_init(500, SCREEN_HEIGHT - 40, 200, AMBER, "Warp",
				"0", "100", 0.0, 255.0, sample_warp_current,
				do_warpdrive);
	nav_ui.navzoom_slider = snis_slider_init(5, SCREEN_HEIGHT - 20, 160, AMBER, "ZOOM",
				"1", "10", 0.0, 100.0, sample_navzoom,
				do_navzoom);
	nav_ui.throttle_slider = snis_slider_init(SCREEN_WIDTH - 30, SCREEN_HEIGHT - 250, 230,
				AMBER, "THROTTLE", "1", "10", 0.0, 255.0, sample_impulse_current,
				do_throttle);
	snis_slider_set_vertical(nav_ui.throttle_slider, 1);
	nav_ui.warp_gauge = gauge_init(650, 410, 100, 0.0, 10.0, -120.0 * M_PI / 180.0,
				120.0 * 2.0 * M_PI / 180.0, RED, AMBER,
				10, "WARP", sample_warpdrive);
	nav_ui.engage_warp_button = snis_button_init(570, 520, 150, 25, "ENGAGE WARP", AMBER,
				NANO_FONT, engage_warp_button_pressed, NULL);
	nav_ui.warp_up_button = snis_button_init(500, 490, 40, 25, "UP", AMBER,
			NANO_FONT, warp_up_button_pressed, NULL);
	nav_ui.warp_down_button = snis_button_init(500, 520, 60, 25, "DOWN", AMBER,
			NANO_FONT, warp_down_button_pressed, NULL);
	nav_ui.reverse_button = snis_button_init(SCREEN_WIDTH - 40, 315, 30, 25, "R", AMBER,
			NANO_FONT, reverse_button_pressed, NULL);
	ui_add_slider(nav_ui.warp_slider, DISPLAYMODE_NAVIGATION);
	ui_add_slider(nav_ui.shield_slider, DISPLAYMODE_NAVIGATION);
	ui_add_slider(nav_ui.navzoom_slider, DISPLAYMODE_NAVIGATION);
	ui_add_slider(nav_ui.throttle_slider, DISPLAYMODE_NAVIGATION);
	ui_add_button(nav_ui.engage_warp_button, DISPLAYMODE_NAVIGATION);
	ui_add_button(nav_ui.warp_up_button, DISPLAYMODE_NAVIGATION);
	ui_add_button(nav_ui.warp_down_button, DISPLAYMODE_NAVIGATION);
	ui_add_button(nav_ui.reverse_button, DISPLAYMODE_NAVIGATION);
	ui_add_gauge(nav_ui.warp_gauge, DISPLAYMODE_NAVIGATION);
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

static void show_navigation(GtkWidget *w)
{
	char buf[100];
	struct snis_entity *o;
	int cx, cy, gx1, gy1, gx2, gy2;
	int r, sectorx, sectory;
	double screen_radius, max_possible_screen_radius, visible_distance;

	sng_set_foreground(GREEN);

	if (!(o = find_my_ship()))
		return;
	sectorx = floor(10.0 * o->x / (double) XKNOWN_DIM);
	sectory = floor(10.0 * o->y / (double) YKNOWN_DIM);
	sprintf(buf, "SECTOR: %c%d (%5.2lf, %5.2lf)", sectory + 'A', sectorx, o->x, o->y);
	sng_abs_xy_draw_string(w, gc, buf, NANO_FONT, 200, LINEHEIGHT);
	sprintf(buf, "HEADING: %3.1lf", 360.0 * o->heading / (2.0 * M_PI));
	sng_abs_xy_draw_string(w, gc, buf, NANO_FONT, 200, 1.5 * LINEHEIGHT);
#if 0
	sprintf(buf, "vx: %5.2lf", o->vx);
	sng_abs_xy_draw_string(w, gc, buf, TINY_FONT, 600, LINEHEIGHT * 3);
	sprintf(buf, "vy: %5.2lf", o->vy);
	sng_abs_xy_draw_string(w, gc, buf, TINY_FONT, 600, LINEHEIGHT * 4);
#endif
	static struct snis_radar_extent extent = { 40, 90, 470, 470 };
	cx = extent.rx + (extent.rw / 2);
	cy = extent.ry + (extent.rh / 2);
	r = extent.rh / 2;
	sng_set_foreground(GREEN);
	screen_radius = ((((255.0 - o->tsd.ship.navzoom) / 255.0) * 0.08) + 0.01) * XKNOWN_DIM;
	max_possible_screen_radius = 0.09 * XKNOWN_DIM;
	visible_distance = (max_possible_screen_radius * o->tsd.ship.power_data.sensors.i) / 255.0;
	snis_draw_radar_sector_labels(w, gc, o, cx, cy, r, screen_radius);
	snis_draw_radar_grid(w->window, gc, o, cx, cy, r, screen_radius, o->tsd.ship.navzoom > 100);
	sng_set_foreground(DARKRED);
	snis_draw_reticule(w, gc, cx, cy, r, o->heading, DARKRED, RED);
	snis_draw_headings_on_reticule(w, gc, cx, cy, r, o);
	snis_draw_ship_on_reticule(w, gc, cx, cy, r, o);

	draw_all_the_guys(w, o, &extent, screen_radius, visible_distance);
	draw_all_the_sparks(w, o, &extent, screen_radius);

	gx1 = NAV_DATA_X + 10;
	gy1 = 15;
	gx2 = NAV_DATA_X + NAV_DATA_W - 10;
	gy2 = NAV_DATA_Y + NAV_DATA_H - 80;
	sng_set_foreground(AMBER);
	draw_science_graph(w, o, o, gx1, gy1, gx2, gy2);
	show_common_screen(w, "NAV");
}

struct damcon_ui {
	struct label *robot_controls;
	struct button *engineering_button;
	struct button *robot_forward_button;
	struct button *robot_backward_button;
	struct button *robot_left_button;
	struct button *robot_right_button;
	struct button *robot_gripper_button;
} damcon_ui;

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
	queue_to_server(packed_buffer_new("h", OPCODE_REQUEST_ROBOT_GRIPPER));
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

	ui_add_button(damcon_ui.engineering_button, DISPLAYMODE_DAMCON);
	ui_add_button(damcon_ui.robot_forward_button, DISPLAYMODE_DAMCON);
	ui_add_button(damcon_ui.robot_left_button, DISPLAYMODE_DAMCON);
	ui_add_button(damcon_ui.robot_right_button, DISPLAYMODE_DAMCON);
	ui_add_button(damcon_ui.robot_backward_button, DISPLAYMODE_DAMCON);
	ui_add_button(damcon_ui.robot_gripper_button, DISPLAYMODE_DAMCON);
	ui_add_label(damcon_ui.robot_controls, DISPLAYMODE_DAMCON);
}

struct engineering_ui {
	struct gauge *fuel_gauge;
	struct gauge *amp_gauge;
	struct gauge *voltage_gauge;
	struct gauge *temp_gauge;
	struct button *damcon_button;
	struct slider *shield_slider;
	struct slider *maneuvering_slider;
	struct slider *warp_slider;
	struct slider *impulse_slider;
	struct slider *sensors_slider;
	struct slider *comm_slider;
	struct slider *phaserbanks_slider;
	struct slider *throttle_slider;
	struct slider *tractor_slider;

	struct slider *shield_damage;
	struct slider *impulse_damage;
	struct slider *warp_damage;
	struct slider *torpedo_tubes_damage;
	struct slider *phaser_banks_damage;
	struct slider *sensors_damage;
	struct slider *comms_damage;
	struct slider *tractor_damage;

} eng_ui;

static void zero_engineering_sliders(void)
{
	snis_slider_set_input(eng_ui.shield_slider, 0);
	snis_slider_set_input(eng_ui.maneuvering_slider, 0);
	snis_slider_set_input(eng_ui.warp_slider, 0);
	snis_slider_set_input(eng_ui.impulse_slider, 0);
	snis_slider_set_input(eng_ui.sensors_slider, 0);
	snis_slider_set_input(eng_ui.comm_slider, 0);
	snis_slider_set_input(eng_ui.phaserbanks_slider, 0);
	snis_slider_set_input(eng_ui.tractor_slider, 0);
}

static void damcon_button_pressed(void *x)
{
	displaymode = DISPLAYMODE_DAMCON;
}

static void init_engineering_ui(void)
{
	int y;
	int x = 100;
	int r = 90;
	int xinc = 190;
	int yinc = 35; 
	int dm = DISPLAYMODE_ENGINEERING;
	int color = AMBER;

	struct engineering_ui *eu = &eng_ui;
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

	y = 220;
	eu->damcon_button = snis_button_init(20, y + 30, 160, 25, "DAMAGE CONTROL", color,
			NANO_FONT, damcon_button_pressed, (void *) 0);
	y += yinc;
	eu->shield_slider = snis_slider_init(20, y += yinc, 150, color, "SHIELDS", "0", "100",
				0.0, 255.0, sample_shields_current, do_shields_pwr);
	eu->phaserbanks_slider = snis_slider_init(20, y += yinc, 150, color, "PHASERS", "0", "100",
				0.0, 255.0, sample_phasers_current, do_phaserbanks_pwr);
	eu->comm_slider = snis_slider_init(20, y += yinc, 150, color, "COMMS", "0", "100",
				0.0, 255.0, sample_comms_current, do_comms_pwr);
	eu->sensors_slider = snis_slider_init(20, y += yinc, 150, color, "SENSORS", "0", "100",
				0.0, 255.0, sample_sensors_current, do_sensors_pwr);
	eu->impulse_slider = snis_slider_init(20, y += yinc, 150, color, "IMPULSE DR", "0", "100",
				0.0, 255.0, sample_impulse_current, do_impulse_pwr);
	eu->warp_slider = snis_slider_init(20, y += yinc, 150, color, "WARP DR", "0", "100",
				0.0, 255.0, sample_warp_current, do_warp_pwr);
	eu->maneuvering_slider = snis_slider_init(20, y += yinc, 150, color, "MANEUVERING", "0", "100",
				0.0, 255.0, sample_maneuvering_current, do_maneuvering_pwr);
	eu->tractor_slider = snis_slider_init(20, y += yinc, 150, color, "TRACTOR", "0", "100",
				0.0, 255.0, sample_tractor_current, do_tractor_pwr);
	ui_add_slider(eu->shield_slider, dm);
	ui_add_slider(eu->phaserbanks_slider, dm);
	ui_add_slider(eu->comm_slider, dm);
	ui_add_slider(eu->sensors_slider, dm);
	ui_add_slider(eu->impulse_slider, dm);
	ui_add_slider(eu->warp_slider, dm);
	ui_add_slider(eu->maneuvering_slider, dm);
	ui_add_slider(eu->tractor_slider, dm);
	ui_add_gauge(eu->amp_gauge, dm);
	ui_add_gauge(eu->voltage_gauge, dm);
	ui_add_gauge(eu->fuel_gauge, dm);
	ui_add_gauge(eu->temp_gauge, dm);
	ui_add_button(eu->damcon_button, dm);

	y = 220 + yinc;
	eu->shield_damage = snis_slider_init(350, y += yinc, 150, color, "SHIELD STATUS", "0", "100",
				0.0, 100.0, sample_shield_damage, NULL);
	eu->impulse_damage = snis_slider_init(350, y += yinc, 150, color, "IMPULSE STATUS", "0", "100",
				0.0, 100.0, sample_impulse_damage, NULL);
	eu->warp_damage = snis_slider_init(350, y += yinc, 150, color, "WARP STATUS", "0", "100",
				0.0, 100.0, sample_warp_damage, NULL);
	eu->torpedo_tubes_damage = snis_slider_init(350, y += yinc, 150, color, "TORPEDO STATUS", "0", "100",
				0.0, 100.0, sample_torpedo_tubes_damage, NULL);
	eu->phaser_banks_damage = snis_slider_init(350, y += yinc, 150, color, "PHASER STATUS", "0", "100",
				0.0, 100.0, sample_phaser_banks_damage, NULL);
	eu->sensors_damage = snis_slider_init(350, y += yinc, 150, color, "SENSORS STATUS", "0", "100",
				0.0, 100.0, sample_sensors_damage, NULL);
	eu->comms_damage = snis_slider_init(350, y += yinc, 150, color, "COMMS STATUS", "0", "100",
				0.0, 100.0, sample_comms_damage, NULL);
	eu->tractor_damage = snis_slider_init(350, y += yinc, 150, color, "TRACTOR STATUS", "0", "100",
				0.0, 100.0, sample_tractor_damage, NULL);
	ui_add_slider(eu->shield_damage, dm);
	ui_add_slider(eu->impulse_damage, dm);
	ui_add_slider(eu->warp_damage, dm);
	ui_add_slider(eu->torpedo_tubes_damage, dm);
	ui_add_slider(eu->phaser_banks_damage, dm);
	ui_add_slider(eu->sensors_damage, dm);
	ui_add_slider(eu->comms_damage, dm);
	ui_add_slider(eu->tractor_damage, dm);
}

static void show_engineering(GtkWidget *w)
{
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
		snis_draw_line(w->window, gc, damconscreenx0, y1,
				damconscreenx0 + damconscreenxdim, y1);
	}

	/* bottom border */
	y1 = damcony_to_screeny(DAMCONYDIM / 2.0);
	if (y1 >= damconscreeny0 &&
		y1 <= damconscreeny0  + damconscreenydim) {
		snis_draw_line(w->window, gc, damconscreenx0, y1,
				damconscreenx0 + damconscreenxdim, y1);
	}

	/* left border */
	x1 = damconx_to_screenx(-DAMCONXDIM / 2.0);
	if (x1 > damconscreenx0 &&
		x1 < damconscreenx0 + damconscreenxdim) {
		snis_draw_line(w->window, gc, x1, damconscreeny0,
				x1, damconscreeny0 + damconscreenydim);
	}

	/* right border */
	x1 = damconx_to_screenx(DAMCONXDIM / 2.0);
	if (x1 > damconscreenx0 &&
		x1 < damconscreenx0 + damconscreenxdim) {
		snis_draw_line(w->window, gc, x1, damconscreeny0,
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
	sng_draw_vect_obj(w, gc, &damcon_robot_spun[byteangle], x, y);
}

static void draw_damcon_system(GtkWidget *w, struct snis_damcon_entity *o)
{
	int x, y;
	static char *system_label[] = {
		"WARP DRIVE",
		"SENSOR ARRAY",
		"COMMUNICATIONS",
		"NAVIGATION",
		"PHASER BANKS",
		"PHOTON WEAPONS",
		"SHIELD SYSTEM",
	};

	if (!on_damcon_screen(o, &placeholder_system))
		return;
	
	x = damconx_to_screenx(o->x);
	y = damcony_to_screeny(o->y);
	sng_set_foreground(WHITE);
	sng_draw_vect_obj(w, gc, &placeholder_system, x, y);
	sng_abs_xy_draw_string(w, gc, system_label[o->type - 1],
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
	sng_draw_vect_obj(w, gc, &placeholder_socket, x, y);
	sng_abs_xy_draw_string(w, gc, msg, NANO_FONT, x - 10, y);
	sng_set_foreground(AMBER);
	snis_draw_line(w->window, gc, x, y - 20, x, y + 20);
	snis_draw_line(w->window, gc, x - 20, y, x + 20, y);
}

static void draw_damcon_socket(GtkWidget *w, struct snis_damcon_entity *o)
{
	if (o->tsd.socket.contents_id == DAMCON_SOCKET_EMPTY)
		draw_damcon_socket_or_part(w, o, WHITE);
}

static void draw_damcon_part(GtkWidget *w, struct snis_damcon_entity *o)
{
	int x, y;
	char msg[20];
	int byteangle = (int) (o->heading * 64.0 / M_PI);

	if (!on_damcon_screen(o, &placeholder_part_spun[byteangle]))
		return;
	x = damconx_to_screenx(o->x);
	y = damcony_to_screeny(o->y);
	sprintf(msg, "%d %d", o->tsd.socket.system, o->tsd.socket.part);
	sng_set_foreground(YELLOW);
	sng_draw_vect_obj(w, gc, &placeholder_part_spun[byteangle], x, y);
	sng_abs_xy_draw_string(w, gc, msg, NANO_FONT, x - 10, y);
	sng_set_foreground(AMBER);
	snis_draw_line(w->window, gc, x, y - 20, x, y + 20);
	snis_draw_line(w->window, gc, x - 20, y, x + 20, y);
}

static void draw_damcon_object(GtkWidget *w, struct snis_damcon_entity *o)
{
	switch (o->type) {
	case DAMCON_TYPE_ROBOT:
		draw_damcon_robot(w, o);
		break;
	case DAMCON_TYPE_WARPDRIVE:
	case DAMCON_TYPE_SENSORARRAY:
	case DAMCON_TYPE_COMMUNICATIONS:
	case DAMCON_TYPE_NAVIGATION:
	case DAMCON_TYPE_PHASERBANK:
	case DAMCON_TYPE_TORPEDOSYSTEM:
	case DAMCON_TYPE_SHIELDSYSTEM:
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
	sng_current_draw_rectangle(w->window, gc, 0,
		damconscreenx0, damconscreeny0, damconscreenxdim, damconscreenydim);

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

static void zero_science_sliders(void)
{
	snis_slider_set_input(sci_ui.scizoom, 0);
}

static void sci_details_pressed(void *x)
{
	queue_to_server(packed_buffer_new("hb", OPCODE_SCI_DETAILS,
		(unsigned char) !sci_ui.details_mode));
}

static void init_science_ui(void)
{
	sci_ui.scizoom = snis_slider_init(350, 50, 300, DARKGREEN, "Range", "0", "100",
				0.0, 100.0, sample_scizoom, do_scizoom);
	sci_ui.details_button = snis_button_init(450, 570, 75, 25, "DETAILS",
			GREEN, NANO_FONT, sci_details_pressed, (void *) 0);
	ui_add_slider(sci_ui.scizoom, DISPLAYMODE_SCIENCE);
	ui_add_button(sci_ui.details_button, DISPLAYMODE_SCIENCE);
	sciecx = entity_context_new(50);
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
	queue_to_server(packed_buffer_new("hb", OPCODE_REQUEST_REDALERT, new_alert_mode));
}

static void send_comms_packet_to_server(char *msg, uint16_t opcode, uint32_t id)
{
	struct packed_buffer *pb;
	uint8_t len = strlen(msg);

	pb = packed_buffer_allocate(sizeof(struct comms_transmission_packet) + len);
	packed_buffer_append(pb, "hbw", opcode, len, id);
	packed_buffer_append_raw(pb, msg, (unsigned short) len);
	packed_buffer_queue_add(&to_server_queue, pb, &to_server_queue_mutex);
	wakeup_gameserver_writer();
}

static void send_lua_script_packet_to_server(char *script)
{
	struct packed_buffer *pb;
	uint8_t len = strlen(script);

	pb = packed_buffer_allocate(sizeof(struct lua_script_packet) + len);
	packed_buffer_append(pb, "hb", OPCODE_EXEC_LUA_SCRIPT, len);
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
	y = SCREEN_HEIGHT - 60;
	comms_ui.red_alert_button = snis_button_init(x, y, 120, 25, "RED ALERT", RED,
			NANO_FONT, comms_screen_red_alert_pressed, (void *) 6);
	comms_ui.tw = text_window_init(10, 70, SCREEN_WIDTH - 20, 40, 20, GREEN);
	comms_ui.comms_input = snis_text_input_box_init(10, 520, 30, 550, GREEN, TINY_FONT,
					comms_ui.input, 50, &timer,
					comms_input_entered, NULL);
	snis_text_input_box_set_return(comms_ui.comms_input,
					comms_transmit_button_pressed);
	comms_ui.comms_transmit_button = snis_button_init(10, 550, 160, 30, "TRANSMIT", GREEN,
			TINY_FONT, comms_transmit_button_pressed, NULL);
	ui_add_text_window(comms_ui.tw, DISPLAYMODE_COMMS);
	ui_add_button(comms_ui.comms_onscreen_button, DISPLAYMODE_COMMS);
	ui_add_button(comms_ui.nav_onscreen_button, DISPLAYMODE_COMMS);
	ui_add_button(comms_ui.weap_onscreen_button, DISPLAYMODE_COMMS);
	ui_add_button(comms_ui.eng_onscreen_button, DISPLAYMODE_COMMS);
	ui_add_button(comms_ui.damcon_onscreen_button, DISPLAYMODE_COMMS);
	ui_add_button(comms_ui.sci_onscreen_button, DISPLAYMODE_COMMS);
	ui_add_button(comms_ui.main_onscreen_button, DISPLAYMODE_COMMS);
	ui_add_button(comms_ui.red_alert_button, DISPLAYMODE_COMMS);
	ui_add_button(comms_ui.comms_transmit_button, DISPLAYMODE_COMMS);
	ui_add_text_input_box(comms_ui.comms_input, DISPLAYMODE_COMMS);
}

static int weapons_button_press(int x, int y)
{
	double ux, uy;
	static struct snis_radar_extent extent = { 40, 90, 470, 470 };
	struct snis_entity *o = &go[my_ship_oid];
	double screen_radius;
	int i, cx, cy;
	double dist2, mindist2, sxdist2, rdist2;
	int minindex;

	screen_radius = ((((255.0 - o->tsd.ship.weapzoom) / 255.0) * 0.08) + 0.01) * XKNOWN_DIM;
	ux = radarx_to_ux(o, x, &extent, screen_radius);
	uy = radary_to_uy(o, y, &extent, screen_radius);

	minindex = -1;
	mindist2 = 0;
	dist2 = 0;

	/* TODO: make sure that click is actually on the radar screen. */
	cx = extent.rx + (extent.rw / 2);
	cy = extent.ry + (extent.rh / 2);
	sxdist2 = (cx - x) * (cx - x) + (cy - y) * (cy - y);
	rdist2 = (extent.rw / 2) * (extent.rw / 2) +
		(extent.rh / 2) * (extent.rh / 2);
	if (sxdist2 > rdist2)
		return 0;

	pthread_mutex_lock(&universe_mutex);
	for (i = 0; i <= snis_object_pool_highest_object(pool); i++) {
		if (!go[i].alive)
			continue;
		dist2 = (ux - go[i].x) * (ux - go[i].x) +
			(uy - go[i].y) * (uy - go[i].y);
		/* if (dist2 > 5000.0)
			continue; */
		if (minindex == -1 || dist2 < mindist2) {
			mindist2 = dist2;
			minindex = i;
		}
	}
	if (minindex >= 0)
		queue_to_server(packed_buffer_new("hw", OPCODE_WEAP_SELECT_TARGET, go[minindex].id));
	pthread_mutex_unlock(&universe_mutex);
	return 0;
}

#define SCIDIST2 100
static int science_button_press(int x, int y)
{
	int i;
	int xdist, ydist, dist2;
	struct snis_entity *selected;
	struct snis_entity *o;
	double ur, ux, uy;
	int cx, cy, r;
	double dx, dy;

	selected = NULL;
	pthread_mutex_lock(&universe_mutex);
	for (i = 0; i < nscience_guys; i++) {
		xdist = (x - science_guy[i].sx);
		ydist = (y - science_guy[i].sy);
		dist2 = xdist * xdist + ydist * ydist; 
		if (dist2 < SCIDIST2 && science_guy[i].o->sdata.science_data_known) {
			// curr_science_guy = science_guy[i].o;
			selected = science_guy[i].o;
		}
	}
	if (selected)
		request_sci_select_target(selected->id);
	else {
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
		uy = o->y + dy * ur / r;
		request_sci_select_coords(ux, uy);
	}
	pthread_mutex_unlock(&universe_mutex);

	return 0;
}

#define SCIENCE_DATA_X (SCIENCE_SCOPE_X + SCIENCE_SCOPE_W + 20)
#define SCIENCE_DATA_Y (SCIENCE_SCOPE_Y + 20)
#define SCIENCE_DATA_W (SCREEN_WIDTH - 20 - SCIENCE_DATA_X)
#define SCIENCE_DATA_H (SCREEN_HEIGHT - 20 - SCIENCE_DATA_Y)

static void draw_science_graph(GtkWidget *w, struct snis_entity *ship, struct snis_entity *o,
		int x1, int y1, int x2, int y2)
{
	int i, x;
	double sx, sy, sy1, sy2, dist;
	int dy1, dy2, bw, probes, dx, pwr;
	int initial_noise;

	sng_current_draw_rectangle(w->window, gc, 0, x1, y1, (x2 - x1), (y2 - y1));
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
	sng_abs_xy_draw_string(w, gc, "10", NANO_FONT, x1, y2 + 10);
	sng_abs_xy_draw_string(w, gc, "20", NANO_FONT, x1 + (x2 - x1) / 4 - 10, y2 + 10);
	sng_abs_xy_draw_string(w, gc, "30", NANO_FONT, x1 + 2 * (x2 - x1) / 4 - 10, y2 + 10);
	sng_abs_xy_draw_string(w, gc, "40", NANO_FONT, x1 + 3 * (x2 - x1) / 4 - 10, y2 + 10);
	sng_abs_xy_draw_string(w, gc, "50", NANO_FONT, x1 + 4 * (x2 - x1) / 4 - 20, y2 + 10);
	sng_abs_xy_draw_string(w, gc, "Shield Profile (nm)", NANO_FONT, x1 + (x2 - x1) / 4 - 10, y2 + 30);
}

static void draw_science_warp_data(GtkWidget *w, struct snis_entity *ship)
{
	double bearing, dx, dy;
	char buffer[40];

	if (!ship)
		return;
	sng_set_foreground(GREEN);
	dx = ship->x - ship->sci_coordx;
	dy = ship->y - ship->sci_coordy;
	bearing = atan2(dx, dy) * 180 / M_PI;
	if (bearing < 0)
		bearing = -bearing;
	else
		bearing = 360.0 - bearing;
	sng_abs_xy_draw_string(w, gc, "WARP DATA:", NANO_FONT, 10, SCREEN_HEIGHT - 40);
	sprintf(buffer, "BEARING: %3.2lf", bearing);
	sng_abs_xy_draw_string(w, gc, buffer, NANO_FONT, 10, SCREEN_HEIGHT - 25);
	sprintf(buffer, "WARP FACTOR: %2.2lf", 10.0 * hypot(dy, dx) / (XKNOWN_DIM / 2.0));
	sng_abs_xy_draw_string(w, gc, buffer, NANO_FONT, 10, SCREEN_HEIGHT - 10);
}
 
static void draw_science_data(GtkWidget *w, struct snis_entity *ship, struct snis_entity *o)
{
	char buffer[40];
	int x, y, gx1, gy1, gx2, gy2;
	double bearing, dx, dy, range;
	char *the_faction;

	if (!ship)
		return;
	x = SCIENCE_DATA_X + 10;
	y = SCIENCE_DATA_Y + 15;
	sng_current_draw_rectangle(w->window, gc, 0, SCIENCE_DATA_X, SCIENCE_DATA_Y,
					SCIENCE_DATA_W, SCIENCE_DATA_H);
	sprintf(buffer, "NAME: %s", o ? o->sdata.name : "");
	sng_abs_xy_draw_string(w, gc, buffer, TINY_FONT, x, y);
	if (o && (o->type == OBJTYPE_SHIP1 ||
		o->type == OBJTYPE_SHIP2 ||
		o->type == OBJTYPE_STARBASE)) {
		y += 25;
		the_faction = o ? 
			o->sdata.faction >= 0 &&
			o->sdata.faction < ARRAY_SIZE(faction) ?
				faction[o->sdata.faction] : "UNKNOWN" : "UNKNOWN";
		sprintf(buffer, "ORIG: %s", the_faction);
		sng_abs_xy_draw_string(w, gc, buffer, TINY_FONT, x, y);
	}

	if (o) {
		switch (o->type) {
		case OBJTYPE_SHIP2:
			sprintf(buffer, "TYPE: %s", shipclass[o->sdata.subclass]); 
			break;
		case OBJTYPE_STARBASE:
			sprintf(buffer, "TYPE: %s", "STARBASE"); 
			break;
		case OBJTYPE_ASTEROID:
		case OBJTYPE_DERELICT:
			sprintf(buffer, "TYPE: %s", "ASTEROID"); 
			break;
		default:
			sprintf(buffer, "TYPE: %s", "UNKNOWN"); 
			break;
		}
	} else  {
		sprintf(buffer, "TYPE:"); 
	}
	y += 25;
	sng_abs_xy_draw_string(w, gc, buffer, TINY_FONT, x, y);

	if (o)
		sprintf(buffer, "X: %8.2lf", o->x);
	else
		sprintf(buffer, "X:");
	y += 25;
	sng_abs_xy_draw_string(w, gc, buffer, TINY_FONT, x, y);

	if (o)
		sprintf(buffer, "Y: %8.2lf", o->y);
	else
		sprintf(buffer, "Y:");
	y += 25;
	sng_abs_xy_draw_string(w, gc, buffer, TINY_FONT, x, y);

	if (o) { 
		dx = go[my_ship_oid].x - o->x;
		dy = go[my_ship_oid].y - o->y;
		bearing = atan2(dx, dy) * 180 / M_PI;
		if (bearing < 0)
			bearing = -bearing;
		else
			bearing = 360.0 - bearing;
		sprintf(buffer, "BEARING: %3.2lf", bearing);
	} else {
		sprintf(buffer, "BEARING");
	}
	y += 25;
	sng_abs_xy_draw_string(w, gc, buffer, TINY_FONT, x, y);

	if (o) {
		range = sqrt(dx * dx + dy * dy);
		sprintf(buffer, "RANGE: %8.2lf", range);
	} else {
		sprintf(buffer, "RANGE:");
	}
	y += 25;
	sng_abs_xy_draw_string(w, gc, buffer, TINY_FONT, x, y);

	if (o)
		sprintf(buffer, "HEADING: %3.2lf", o->heading);
	else
		sprintf(buffer, "HEADING:");
	y += 25;
	sng_abs_xy_draw_string(w, gc, buffer, TINY_FONT, x, y);
#if 0
	sprintf(buffer, "STRENGTH: %hhu", o->sdata.shield_strength);
	y += 25;
	sng_abs_xy_draw_string(w, gd, buffer, TINY_FONT, x, y);
	sprintf(buffer, "WAVELENGTH: %hhu", o->sdata.shield_wavelength);
	y += 25;
	sng_abs_xy_draw_string(w, gc, buffer, TINY_FONT, x, y);
	sprintf(buffer, "WIDTH: %hhu", o->sdata.shield_width);
	y += 25;
	sng_abs_xy_draw_string(w, gc, buffer, TINY_FONT, x, y);
#endif

	gx1 = x;
	gy1 = y + 25;
	gx2 = SCIENCE_DATA_X + SCIENCE_DATA_W - 10;
	gy2 = SCIENCE_DATA_Y + SCIENCE_DATA_H - 40;
	draw_science_graph(w, ship, o, gx1, gy1, gx2, gy2);
}

static void draw_science_details(GtkWidget *w, GdkGC *gc)
{
	struct entity *e = NULL;
	struct mesh *m;
	char buf[100];

	if (!curr_science_guy || !curr_science_guy->entity)
		return;

	m = entity_get_mesh(curr_science_guy->entity);
	e = add_entity(sciecx, m, 0, 0, 0, GREEN);
	update_entity_rotation(e, M_PI / 2.0, 0, (M_PI / 180.0) * (timer % 360));
	set_render_style(e, RENDER_WIREFRAME);
	camera_set_pos(sciecx, -m->radius * 4, -20, 0);
	camera_look_at(sciecx, (float) 0, (float) 0, (float) -m->radius / 2.0);
	camera_set_parameters(sciecx, (float) 20, (float) 300,
				(float) 16, (float) 12,
				SCREEN_WIDTH, SCREEN_HEIGHT, ANGLE_OF_VIEW);
	render_entities(w, gc, sciecx);
	remove_entity(sciecx, e);
	if (curr_science_guy->type == OBJTYPE_SHIP1 ||
		curr_science_guy->type == OBJTYPE_SHIP2) {
		sprintf(buf, "LIFEFORMS: %d", curr_science_guy->tsd.ship.lifeform_count);
	} else {
		if (curr_science_guy->type == OBJTYPE_STARBASE) {
			sprintf(buf, "LIFEFORMS: %d", curr_science_guy->tsd.starbase.lifeform_count);
		} else {
			sprintf(buf, "LIFEFORMS: 0");
		}
	}
	sng_abs_xy_draw_string(w, gc, buf, TINY_FONT, 250, SCREEN_HEIGHT - 50);
}
 
static void show_science(GtkWidget *w)
{
	int /* rx, ry, rw, rh, */ cx, cy, r;
	struct snis_entity *o;
	char buf[80];
	double zoom;

	if (!(o = find_my_ship()))
		return;
	if ((timer & 0x3f) == 0)
		wwviaudio_add_sound(SCIENCE_PROBE_SOUND);
	sng_set_foreground(GREEN);
	sprintf(buf, "Location: (%5.2lf, %5.2lf)  Heading: %3.1lf", o->x, o->y,
			360.0 * o->tsd.ship.sci_heading / (2.0 * 3.1415927));
	sng_abs_xy_draw_string(w, gc, buf, TINY_FONT, 250, 10 + LINEHEIGHT);
#if 0
	rx = SCIENCE_SCOPE_X;
	ry = SCIENCE_SCOPE_Y;
	rw = SCIENCE_SCOPE_W;
	rh = SCIENCE_SCOPE_H;
#endif
	cx = SCIENCE_SCOPE_CX;
	cy = SCIENCE_SCOPE_CY;
	r = SCIENCE_SCOPE_R;
	zoom = (MAX_SCIENCE_SCREEN_RADIUS - MIN_SCIENCE_SCREEN_RADIUS) *
			(o->tsd.ship.scizoom / 255.0) +
			MIN_SCIENCE_SCREEN_RADIUS;
	sng_set_foreground(DARKGREEN);
	if (!sci_ui.details_mode) {
		snis_draw_radar_sector_labels(w, gc, o, cx, cy, r, zoom);
		snis_draw_radar_grid(w->window, gc, o, cx, cy, r, zoom, o->tsd.ship.scizoom < 50);
		sng_set_foreground(DARKRED);
		snis_draw_science_reticule(w, gc, cx, cy, r,
				o->tsd.ship.sci_heading, fabs(o->tsd.ship.sci_beam_width));
		draw_all_the_science_guys(w, o, zoom);
		draw_all_the_science_sparks(w, o, zoom);
		draw_all_the_science_nebulae(w, o, zoom);
	} else {
		draw_science_details(w, gc);
	}
	draw_science_warp_data(w, o);
	draw_science_data(w, o, curr_science_guy);
	show_common_screen(w, "SCIENCE");
}

static void show_comms(GtkWidget *w)
{
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
	queue_to_server(packed_buffer_new("h", OPCODE_DEMON_CLEAR_ALL));
}

static int ux_to_demonsx(double ux)
{
	return ((ux - demon_ui.ux1) / (demon_ui.ux2 - demon_ui.ux1)) * SCREEN_WIDTH;
}

static int uy_to_demonsy(double uy)
{
	return ((uy - demon_ui.uy1) / (demon_ui.uy2 - demon_ui.uy1)) * SCREEN_HEIGHT;
}

static int ur_to_demonsr(double ur)
{
	return ((ur * SCREEN_WIDTH) / (demon_ui.ux2 - demon_ui.ux1));
}

static double demon_mousex_to_ux(double x)
{
	return demon_ui.ux1 + (x / real_screen_width) * (demon_ui.ux2 - demon_ui.ux1);
}

static double demon_mousey_to_uy(double y)
{
	return demon_ui.uy1 + (y / real_screen_height) * (demon_ui.uy2 - demon_ui.uy1);
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
			queue_to_server(packed_buffer_new("hw", OPCODE_DEMON_DISPOSSESS,
				go[demon_ui.captain_of].id));
				demon_ui.captain_of = -1;
		}
		if (index >= 0 && (go[index].type == OBJTYPE_SHIP2 ||
			go[index].type == OBJTYPE_STARBASE)) {
			demon_ui.captain_of = lookup_object_by_id(id);
			queue_to_server(packed_buffer_new("hw", OPCODE_DEMON_POSSESS,
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
				queue_to_server(packed_buffer_new("hw", OPCODE_DEMON_DISPOSSESS,
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
		demon_ui.iy = demon_mousey_to_uy(y);
		demon_ui.ix2 = demon_mousex_to_ux(x);
		demon_ui.iy2 = demon_mousey_to_uy(y);
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
	double ux, uy;
	uint8_t item_type;

	ux = demon_mousex_to_ux(x);
	uy = demon_mousey_to_uy(y);

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
	queue_to_server(packed_buffer_new("hbSS", OPCODE_CREATE_ITEM, item_type,
			ux, (int32_t) UNIVERSE_DIM, uy, (int32_t) UNIVERSE_DIM));
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
		sy = uy_to_demonsy(o->y);
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
	sy1 = uy_to_demonsy(demon_ui.iy);

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
			demon_ui.selectedy = demon_mousey_to_uy(oy);
		}
		pthread_mutex_unlock(&universe_mutex);
	}
}

static void demon_button2_release(int button, gdouble x, gdouble y)
{
	int i;
	double dx, dy;

	if (demon_ui.nselected <= 0)
		return;

	/* Moving objects... */
	dx = demon_mousex_to_ux(x) - demon_mousex_to_ux(demon_ui.move_from_x);
	dy = demon_mousey_to_uy(y) - demon_mousey_to_uy(demon_ui.move_from_y);
	
	pthread_mutex_lock(&universe_mutex);
	for (i = 0; i < demon_ui.nselected; i++) {
		queue_to_server(packed_buffer_new("hwSS",
				OPCODE_DEMON_MOVE_OBJECT,
				demon_ui.selected_id[i],
				dx, (int32_t) UNIVERSE_DIM,
				dy, (int32_t) UNIVERSE_DIM));
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

static void debug_draw_object(GtkWidget *w, struct snis_entity *o)
{
	int x, y, x1, y1, x2, y2, vx, vy;
	struct snis_entity *v = NULL;
	int xoffset = 7;
	int yoffset = 10;

	if (!o->alive)
		return;

	x = ux_to_demonsx(o->x);
	if (x < 0 || x > SCREEN_WIDTH)
		return;
	y = uy_to_demonsy(o->y);
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
		sng_set_foreground(SHIP_COLOR);
		if (o->tsd.ship.victim_id != -1) {
			int vi = lookup_object_by_id(o->tsd.ship.victim_id);
			if (vi >= 0) {	
				v = &go[vi];
				vx = ux_to_demonsx(v->x);
				vy = uy_to_demonsy(v->y);
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
		sng_draw_circle(w->window, gc, x, y,
			ur_to_demonsr(o->tsd.nebula.r));
		break;
	case OBJTYPE_STARBASE:
		sng_set_foreground(STARBASE_COLOR);
		sng_draw_circle(w->window, gc, x, y, 5);
		break;
	case OBJTYPE_PLANET:
		sng_set_foreground(PLANET_COLOR);
		sng_draw_circle(w->window, gc, x, y, 5);
		break;
	case OBJTYPE_WORMHOLE:
		sng_set_foreground(WORMHOLE_COLOR);
		sng_draw_circle(w->window, gc, x, y, 5);
		break;
	default:
		sng_set_foreground(WHITE);
	}
	snis_draw_line(w->window, gc, x1, y1, x2, y2);
	snis_draw_line(w->window, gc, x1, y2, x2, y1);
	if (demon_id_selected(o->id)) {
		if (timer & 0x02) {
			snis_draw_line(w->window, gc, x1 - 6, y1 - 6, x2 + 6, y1 - 6);
			snis_draw_line(w->window, gc, x1 - 6, y2 + 6, x2 + 6, y2 + 6);
			snis_draw_line(w->window, gc, x1 - 6, y1 - 6, x1 - 6, y2 + 6);
			snis_draw_line(w->window, gc, x2 + 6, y1 - 6, x2 + 6, y2 + 6);
			if (o->type == OBJTYPE_SHIP1 || o->type == OBJTYPE_SHIP2) {
				snis_draw_arrow(w, gc, x, y, SCIENCE_SCOPE_R, o->heading +
					(o->type == OBJTYPE_SHIP2) * 90.0 * M_PI / 180.0, 0.4);
			}
		}
	} else {
		if (o->type == OBJTYPE_SHIP1 || o->type == OBJTYPE_SHIP2) {
			snis_draw_arrow(w, gc, x, y, SCIENCE_SCOPE_R, o->heading +
					(o->type == OBJTYPE_SHIP2) * 90.0 * M_PI / 180.0, 0.4);
		}
	}

	if (o->type == OBJTYPE_SHIP1 || o->type == OBJTYPE_SHIP2) {
		sng_abs_xy_draw_string(w, gc, o->sdata.name, NANO_FONT,
					x + xoffset, y + yoffset);
	}
	if (v) {
		sng_set_foreground(RED);
		sng_draw_dotted_line(w->window, gc, x, y, vx, vy);
	}

	if ((o->type == OBJTYPE_SHIP2 || o->type == OBJTYPE_STARBASE) &&
			o->index == demon_ui.captain_of) {
		sng_set_foreground(RED);
		sng_draw_circle(w->window, gc, x, y, 10 + (timer % 10));
	}
	
done_drawing_item:

	sng_set_foreground(GREEN);
	sng_abs_xy_draw_string(w, gc, demon_ui.error_msg, NANO_FONT, 20, 570);
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
		sng_abs_xy_draw_string(w, gc, buffer, NANO_FONT, 85, i * 18 + 60);
		sprintf(buffer, "%s", demon_cmd[i].help);
		sng_abs_xy_draw_string(w, gc, buffer, NANO_FONT, 170, i * 18 + 60);
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
	double x, y;
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

static void uppercase(char *s)
{
	while (*s) {
		*s = toupper(*s);
		s++;
	}
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
			demon_location[l].y = demon_ui.selectedy;
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
			packed_buffer_append(pb, "hbwwbb", OPCODE_DEMON_COMMAND, DEMON_CMD_ATTACK, 0, 0,
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
				demon_ui.selectedy = demon_location[l].y;
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
		case 10: demon_help_mode = 1; 
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
		queue_to_server(packed_buffer_new("hw",
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
	queue_to_server(packed_buffer_new("hw", OPCODE_DEMON_FIRE_TORPEDO,
				go[demon_ui.captain_of].id));
}

static void demon_phaser_button_pressed(void *x)
{
	if (demon_ui.captain_of < 0)
		return;
	if (go[demon_ui.captain_of].type != OBJTYPE_SHIP2)
		return;
	queue_to_server(packed_buffer_new("hw", OPCODE_DEMON_FIRE_PHASER,
				go[demon_ui.captain_of].id));
}

static void init_demon_ui()
{
	demon_ui.ux1 = 0;
	demon_ui.uy1 = 0;
	demon_ui.ux2 = XKNOWN_DIM;
	demon_ui.uy2 = YKNOWN_DIM;
	demon_ui.nselected = 0;
	demon_ui.selectedx = -1.0;
	demon_ui.selectedy = -1.0;
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
		sng_abs_xy_draw_string(w, gc, demon_group[i].name,
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
		sy1 = uy_to_demonsy(0.0);
		if (sy1 < 0)
			sy1 = 0;
		if (sy1 > SCREEN_HEIGHT)
			continue;
		sy2 = uy_to_demonsy(YKNOWN_DIM);
		if (sy2 > SCREEN_HEIGHT)
			sy2 = SCREEN_HEIGHT;
		if (sy2 < 0)
			continue;
		snis_draw_dotted_vline(w->window, gc, sx1, sy1, sy2, 5);
	}

	iy = YKNOWN_DIM / 10.0;
	for (y = 0; y <= 10; y++) {
		int sx1, sy1, sx2;

		sy1 = uy_to_demonsy(iy * y);
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
	iy = YKNOWN_DIM / 10;
	for (x = 0; x < 10; x++)
		for (y = 0; y < 10; y++) {
			int tx, ty;

			snprintf(label, sizeof(label), "%c%d", letters[y], x);
			tx = ux_to_demonsx(x * ix);
			ty = uy_to_demonsy(y * iy);
			sng_abs_xy_draw_string(w, gc, label, NANO_FONT,
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
		y = uy_to_demonsy(demon_ui.selectedy);
		sng_set_foreground(BLUE);
		snis_draw_line(w->window, gc, x - 3, y, x + 3, y);
		snis_draw_line(w->window, gc, x, y - 3, x, y + 3);
	}

	sng_set_foreground(GREEN);
	if (netstats.elapsed_seconds == 0)
		sprintf(buffer, "Waiting for data");
	else 
		sprintf(buffer, "TX:%llu RX:%llu T=%lu SECS. BW=%llu BYTES/SEC",
			(unsigned long long) netstats.bytes_sent,
			(unsigned long long) netstats.bytes_recd, 
			(unsigned long) netstats.elapsed_seconds,
			(unsigned long long) (netstats.bytes_recd + netstats.bytes_sent) / netstats.elapsed_seconds);
	sng_abs_xy_draw_string(w, gc, buffer, TINY_FONT, 10, SCREEN_HEIGHT - 10); 

	if (demon_ui.selectmode) {
		int x1, y1, x2, y2;

		x1 = ux_to_demonsx(demon_ui.ix);
		y1 = uy_to_demonsy(demon_ui.iy);
		x2 = ux_to_demonsx(demon_ui.ix2);
		y2 = uy_to_demonsy(demon_ui.iy2);
		sng_set_foreground(WHITE);
		sng_draw_dotted_line(w->window, gc, x1, y1, x2, y1);
		sng_draw_dotted_line(w->window, gc, x1, y2, x2, y2);
		sng_draw_dotted_line(w->window, gc, x1, y1, x1, y2);
		sng_draw_dotted_line(w->window, gc, x2, y1, x2, y2);
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
		sng_scaled_rectangle(w->window, gc, 1, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
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
		sng_scaled_rectangle(w->window, gc, 1, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
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
		sng_thick_scaled_line(w->window, gc, x, y, x2, y2);
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
		snis_draw_line(w->window, gc, 0, y1, SCREEN_WIDTH, y2);
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
	printf("start lobby server button pressed.\n");
	/* I should probably do this with fork and exec, or clone, not system */
	if (system("./ssgl/ssgl_server &") < 0)
		printf("Failed to exec lobby server process.\n");
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
	char command[220];

	/* FIXME this is probably not too cool. */
	sanitize_string(net_setup_ui.servername);
	sanitize_string(net_setup_ui.lobbyname);

	/* These must be set in order to start the game server. */
	if (strcmp(net_setup_ui.servername, "") == 0 || 
		strcmp(net_setup_ui.lobbyname, "") == 0)
		return;

	memset(command, 0, sizeof(command));
	snprintf(command, 200, "./snis_server %s SNIS '%s' . &",
			net_setup_ui.lobbyname, net_setup_ui.servername);
	printf("start game server button pressed.\n");
	if (system(command) < 0)
		printf("Failed to exec game server process.\n");
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
	nsu->role_nav_v = 0;
	nsu->role_weap_v = 0;
	nsu->role_eng_v = 0;
	nsu->role_sci_v = 0;
	nsu->role_comms_v = 0;
	nsu->role_sound_v = 0;
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
	sng_draw_vect_obj(w, gc, &snis_logo, 100, 500);
	sng_set_foreground(GREEN);
	sng_abs_xy_draw_string(w, gc, "NETWORK SETUP", SMALL_FONT, 25, 10 + LINEHEIGHT * 2);
	sng_abs_xy_draw_string(w, gc, "LOBBY SERVER NAME OR IP ADDRESS", TINY_FONT, 25, 130);
	sng_abs_xy_draw_string(w, gc, "GAME SERVER NICKNAME", TINY_FONT, 25, 280);
	sng_abs_xy_draw_string(w, gc, "SHIP NAME", TINY_FONT, 20, 470);
	sng_abs_xy_draw_string(w, gc, "PASSWORD", TINY_FONT, 20, 520);

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
			do_dirkey(-1, 0);
		if (e->direction == GDK_SCROLL_DOWN)
			do_dirkey(1, 0);
		return 0;
	case DISPLAYMODE_WEAPONS:
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
	"  * W KEY TOGGLES BETWEEN WEAPONS VIEW\n"
	"    AND MAIN VIEW\n"
	"\nPRESS ESC TO EXIT HELP\n",

	/* Navigation help text */
	"NAVIGATION\n\n"
	"  CONTROLS\n\n"
	"  * USE ARROW KEYS TO TURN SHIP\n"
	"    (OR USE MOUSE WHEEL)\n"
	"  * VERTICAL SLIDER ON RIGHT OF SCREEN\n"
	"    CONTROLS THROTTLE\n"
	"  * R BUTTON ABOVE THROTTLE REVERSES THRUST\n"
	"  * USE WARP FOR FAST TRAVEL\n"
	"  * USE PLUS/MINUS KEYS TO ZOOM SCANNER VIEW\n"
	"  * USE SHIELD SLIDER TO SET SHIELD LEVEL\n"
	"\nPRESS ESC TO EXIT HELP\n",

	/* Weapons help text */
	"WEAPONS\n\n"
	"  CONTROLS\n\n"
	"  * USE ARROW KEYS TO AIM WEAPONS\n"
	"  * FIRE WHEN TARGET SELECTED\n"
	"  * PLUS/MINUS KEYS ZOOM SCANNER VIEW\n"
	"    (OR USE MOUSE WHEEL)\n"
	"  * MATCH PHASER WAVELENGTH TO WEAKNESSES\n"
	"    IN ENEMY SHIELDS\n"
	"\nPRESS ESC TO EXIT HELP\n",

	/* Engineering help text */
	"ENGINEERING\n\n"
	"  CONTROLS\n\n"
	"  * USE SLIDERS ON LEFT SIDE OF SCREEN\n"
        "    TO LIMIT POWER CONSUMPTION OF SHIP\n"
        "    SYSTEMS\n"
        "  * HEALTH OF SYSTEMS IS INDICATED ON RIGHT\n"
	"    SIDE OF SCREEN\n"
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
	"TO DO\n"
	"  HELP TEXT FOR COMMS",

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
				sng_abs_xy_draw_string(w, gc, buffer, TINY_FONT, 60, y);
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
	sng_scaled_rectangle(w->window, gc, 1, 50, 50, SCREEN_WIDTH - 100, SCREEN_HEIGHT - 100);
	sng_set_foreground(GREEN);
	sng_scaled_rectangle(w->window, gc, 0, 50, 50, SCREEN_WIDTH - 100, SCREEN_HEIGHT - 100);
	if (displaymode < 0 || displaymode >= ARRAYSIZE(help_text)) {
		draw_help_text(w, "Unknown screen, no help available");
		return;
	}
	draw_help_text(w, help_text[displaymode]);
}

static void draw_quit_screen(GtkWidget *w)
{
	int x;
	static int quittimer = 0;

	quittimer++;

	sng_set_foreground(BLACK);
	sng_scaled_rectangle(w->window, gc, 1, 100, 100, SCREEN_WIDTH - 200, SCREEN_HEIGHT - 200);
	sng_set_foreground(RED);
	sng_scaled_rectangle(w->window, gc, FALSE, 100, 100, SCREEN_WIDTH-200, SCREEN_HEIGHT-200);
	sng_set_foreground(WHITE);
	sng_abs_xy_draw_string(w, gc, "Quit?", BIG_FONT, 300, 280);

	if (current_quit_selection == 1) {
		x = 130;
		sng_set_foreground(WHITE);
	} else {
		x = 480;
		sng_set_foreground(RED);
	}
	sng_abs_xy_draw_string(w, gc, "Quit Now", SMALL_FONT, 150, 450);
	if (current_quit_selection == 0)
		sng_set_foreground(WHITE);
	else
		sng_set_foreground(RED);
	sng_abs_xy_draw_string(w, gc, "Don't Quit", SMALL_FONT, 500, 450);

	if ((quittimer & 0x04)) {
		sng_set_foreground(WHITE);
		sng_scaled_rectangle(w->window, gc, FALSE, x, 420, 200, 50);
	}
}

static void begin_2d_gl(void)
{
#ifndef WITHOUTOPENGL
	glDisable(GL_TEXTURE_2D);
	glPushMatrix();
	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glLoadIdentity();
	glOrtho(0.0f, (float) real_screen_width, 0.0f, (float) real_screen_height, -1.0, 1.0);
	glMatrixMode(GL_MODELVIEW);
	glDisable(GL_DEPTH_TEST);
#endif
}

static void end_2d_gl(void)
{
#ifndef WITHOUTOPENGL
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glPopMatrix();
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	glPopMatrix();
#endif
}

static int main_da_expose(GtkWidget *w, GdkEvent *event, gpointer p)
{
	struct snis_entity *o;

	make_science_forget_stuff();

	if (displaymode == DISPLAYMODE_GLMAIN)	
		return 0;

#ifndef WITHOUTOPENGL
	GdkGLContext *gl_context = gtk_widget_get_gl_context(main_da);
	GdkGLDrawable *gl_drawable = gtk_widget_get_gl_drawable(main_da);

	if (!gdk_gl_drawable_gl_begin(gl_drawable, gl_context))
		g_assert_not_reached();

	glClearColor(0.0, 0.0, 0.0, 0.0);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
#endif
	begin_2d_gl();

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
		show_weapons(w);
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
	ui_element_list_draw(w, gc, uiobjs);

	if (helpmode)
		draw_help_screen(w);
	if (in_the_process_of_quitting)
		draw_quit_screen(w);

end_of_drawing:
#ifndef WITHOUTOPENGL
	gdk_gl_drawable_wait_gdk(gl_drawable);
	gdk_gl_drawable_wait_gl(gl_drawable);
#endif

	end_2d_gl();

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

#ifndef WITHOUTOPENGL

static int load_texture(const char *filename)
{
	int rc, texw, texh;
	char whynot[100];
	rc = sng_load_png_texture(filename, &texw, &texh, whynot, sizeof(whynot));
	if (rc < 0) {
		fprintf(stderr, "texture %s failed to load: %s\n",
			filename, whynot);
		return rc;
	}
	texture[ntextures++] = rc;
	return rc;
}

static void load_textures(void)
{
	load_texture("share/snis/textures/image0.png");
	load_texture("share/snis/textures/image1.png");
	load_texture("share/snis/textures/image2.png");
	load_texture("share/snis/textures/image3.png");
	load_texture("share/snis/textures/image4.png");
	load_texture("share/snis/textures/image5.png");
}
#endif

/* call back for configure_event (for window resize) */
static gint main_da_configure(GtkWidget *w, GdkEventConfigure *event)
{
	GdkRectangle cliprect;
#ifndef WITHOUTOPENGL
	static int textures_loaded = 0; /* blech, static. */
#endif

	/* first time through, gc is null, because gc can't be set without */
	/* a window, but, the window isn't there yet until it's shown, but */
	/* the show generates a configure... chicken and egg.  And we can't */
	/* proceed without gc != NULL...  but, it's ok, because 1st time thru */
	/* we already sort of know the drawing area/window size. */
 
	if (gc == NULL)
		return TRUE;

	real_screen_width =  w->allocation.width;
	real_screen_height =  w->allocation.height;
	xscale_screen = (float) real_screen_width / (float) SCREEN_WIDTH;
	yscale_screen = (float) real_screen_height / (float) SCREEN_HEIGHT;
	sng_set_scale(xscale_screen, yscale_screen);
	if (real_screen_width == 800 && real_screen_height == 600) {
		sng_use_unscaled_drawing_functions();
	} else {
		sng_use_scaled_drawing_functions();
		if (thicklines)
			sng_use_thick_lines();
	}
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
	glViewport(0, 0, alloc.width, alloc.height);

	const static GLfloat light0_position[] = {1.0, 1.0, 1.0, 0.0};
	glLightfv(GL_LIGHT0, GL_POSITION, light0_position);
	glEnable(GL_DEPTH_TEST);    
	/* glEnable(GL_LIGHTING);
	glEnable(GL_LIGHT0);  */

	/* Delimits the end of the OpenGL execution. */
	gdk_gl_drawable_gl_end(gl_drawable);
	sng_fixup_gl_y_coordinate(real_screen_height);

	if (!textures_loaded) {
		load_textures();
		textures_loaded = 1;
	}
#endif
	return TRUE;
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
		weapons_button_press((int) ((0.0 + event->x) / (0.0 + real_screen_width) * SCREEN_WIDTH),
				(int) ((0.0 + event->y) / (0.0 + real_screen_height) * SCREEN_HEIGHT));
		break;
	default:
		break;
	}
	ui_element_list_button_press(uiobjs,
		(int) ((0.0 + event->x) / (0.0 + real_screen_width) * SCREEN_WIDTH),
		(int) ((0.0 + event->y) / (0.0 + real_screen_height) * SCREEN_HEIGHT));
	return TRUE;
}

static int main_da_motion_notify(GtkWidget *w, GdkEventMotion *event,
	__attribute__((unused)) void *unused)
{
	demon_ui.ix2 = demon_mousex_to_ux(event->x);
	demon_ui.iy2 = demon_mousey_to_uy(event->y);
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
	printf("server netstats: %llu bytes sent, %llu bytes recd, secs = %llu, bw = %llu bytes/sec\n",
			netstats.bytes_sent, netstats.bytes_recd, (unsigned long long) netstats.elapsed_seconds,
			(netstats.bytes_sent + netstats.bytes_recd) / netstats.elapsed_seconds);
        wwviaudio_cancel_all_sounds();
        wwviaudio_stop_portaudio();
	close_joystick();
	exit(1); /* probably bad form... oh well. */
}

static void usage(void)
{
	fprintf(stderr, "usage: snis_client lobbyhost starship password\n");
	fprintf(stderr, "       Example: ./snis_client localhost Enterprise tribbles\n");
	exit(1);
}

static void read_ogg_clip(int sound, char *directory, char *filename)
{
	char path[PATH_MAX];

	sprintf(path, "%s/sounds/%s", directory, filename);
	wwviaudio_read_ogg_clip(sound, path);
}

static void read_sound_clips(void)
{
	char *default_asset_dir = "share/snis";
	char *d;

	d = getenv("SNIS_ASSET_DIR");
	if (!d)
		d = default_asset_dir;
	
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
        else
                check_for_screensaver();
}

static struct mesh *snis_read_stl_file(char *directory, char *filename)
{
	char path[PATH_MAX];

	sprintf(path, "%s/models/%s", directory, filename);
	return read_stl_file(path);
}

static struct mesh *make_derelict_mesh(struct mesh *source)
{
	struct mesh *m = mesh_duplicate(source);

	mesh_derelict(m, 10.0);
	return m;
}

static void init_meshes(void)
{
	int i;
	char *default_asset_dir = "share/snis";
	char *d;

	d = getenv("SNIS_ASSET_DIR");
	if (!d)
		d = default_asset_dir;

	ship_mesh = snis_read_stl_file(d, "spaceship.stl");
	torpedo_mesh = snis_read_stl_file(d, "torpedo.stl");
	laser_mesh = snis_read_stl_file(d, "laser.stl");

	for (i = 0; i < NASTEROID_MODELS; i++) {
		char filename[100];

		if (i == 0)
			sprintf(filename, "asteroid.stl");
		else
			sprintf(filename, "asteroid%d.stl", i + 1);
		printf("reading '%s'\n", filename);
		asteroid_mesh[i] = snis_read_stl_file(d, filename);
		mesh_distort(asteroid_mesh[i], 0.10);
	}

	for (i = 0; i < NASTEROID_MODELS; i++) {
		int j;

		for (j = 1; j < NASTEROID_SCALES; j++) {
			float scale = j * 1.5;
			int k = j * NASTEROID_MODELS + i;
			asteroid_mesh[k] = mesh_duplicate(asteroid_mesh[i]);
			mesh_scale(asteroid_mesh[k], scale);
		}
	}

	for (i = 0; i < NPLANET_MODELS; i++) {
		char filename[100];

		sprintf(filename, "planet%d.stl", i + 1);
		printf("reading '%s'\n", filename);
		planet_mesh[i] = snis_read_stl_file(d, filename);
	}

	for (i = 0; i < NSTARBASE_MODELS; i++) {
		char filename[100];

		if (i == 0)
			sprintf(filename, "starbase.stl");
		else
			sprintf(filename, "starbase%d.stl", i + 1);
		printf("reading '%s'\n", filename);
		starbase_mesh[i] = snis_read_stl_file(d, filename);
	}

	freighter_mesh = snis_read_stl_file(d, "freighter.stl");
	cruiser_mesh = snis_read_stl_file(d, "cruiser.stl");
	tanker_mesh = snis_read_stl_file(d, "tanker.stl");
	destroyer_mesh = snis_read_stl_file(d, "destroyer.stl");
	transport_mesh = snis_read_stl_file(d, "transport.stl");
	dragonhawk_mesh = snis_read_stl_file(d, "dragonhawk.stl");
	skorpio_mesh = snis_read_stl_file(d, "skorpio.stl");
	disruptor_mesh = snis_read_stl_file(d, "disruptor.stl");
	research_vessel_mesh = snis_read_stl_file(d, "research-vessel.stl");
	battlestar_mesh = snis_read_stl_file(d, "battlestar.stl");
	particle_mesh = snis_read_stl_file(d, "tetrahedron.stl");
	debris_mesh = snis_read_stl_file(d, "flat-tetrahedron.stl");
	debris2_mesh = snis_read_stl_file(d, "big-flat-tetrahedron.stl");
	wormhole_mesh = snis_read_stl_file(d, "wormhole.stl");
	mesh_distort(wormhole_mesh, 0.15);
	spacemonster_mesh = snis_read_stl_file(d, "spacemonster.stl");
	asteroidminer_mesh = snis_read_stl_file(d, "asteroid-miner.stl");
	spaceship2_mesh = snis_read_stl_file(d, "spaceship2.stl");
	scout_mesh = snis_read_stl_file(d, "spaceship3.stl");
	laserbeam_mesh = snis_read_stl_file(d, "long-triangular-prism.stl");

	/* Note: these must match defines of SHIPTYPEs in snis.h */
	derelict_mesh[0] = make_derelict_mesh(cruiser_mesh);
	derelict_mesh[1] = make_derelict_mesh(destroyer_mesh);
	derelict_mesh[2] = make_derelict_mesh(freighter_mesh);
	derelict_mesh[3] = make_derelict_mesh(tanker_mesh);
	derelict_mesh[4] = make_derelict_mesh(transport_mesh);
	derelict_mesh[5] = make_derelict_mesh(battlestar_mesh);
	derelict_mesh[6] = make_derelict_mesh(ship_mesh);
	derelict_mesh[7] = make_derelict_mesh(asteroidminer_mesh);
	derelict_mesh[8] = make_derelict_mesh(spaceship2_mesh);
	derelict_mesh[9] = make_derelict_mesh(scout_mesh);
	derelict_mesh[10] = make_derelict_mesh(dragonhawk_mesh);
	derelict_mesh[11] = make_derelict_mesh(skorpio_mesh);
	derelict_mesh[12] = make_derelict_mesh(disruptor_mesh);
	derelict_mesh[13] = make_derelict_mesh(research_vessel_mesh);
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

#if 0
	/* only called once in practice */
	g_signal_connect(drawing_area, "configure-event", G_CALLBACK(main_da), NULL);
	/* called every time we need to redraw due to becoming visible or being resized */
	g_signal_connect(drawing_area, "expose-event", G_CALLBACK(expose_cb), NULL);
#endif

#if 0
	const gdouble TIMEOUT_PERIOD = 1000 / 60;
	/* idleCb called every timeoutPeriod */
	g_timeout_add(TIMEOUT_PERIOD, idle_cb, drawing_area);
#endif
	sng_fixup_gl_y_coordinate(SCREEN_HEIGHT);
#endif
	
}

int main(int argc, char *argv[])
{
	GtkWidget *vbox;
	int i;

	/* Need this so that fscanf reads floats properly. */
#define LOCALE_THAT_WORKS "en_US.UTF-8"
	if (!setenv("LANG", LOCALE_THAT_WORKS, 1))
		fprintf(stderr, "Failed to setenv LANG to '%s'\n",
			LOCALE_THAT_WORKS);

	if (argc > 1 && argc < 4)
		usage();

	if (argc >= 4) {
		lobbyhost = argv[1];
		shipname = argv[2];
		password = argv[3];
	} else {
		displaymode = DISPLAYMODE_NETWORK_SETUP;
	}

	role = 0;
	for (i = 4; i < argc; i++) {
		if (strcmp(argv[i], "--allroles") == 0)
			role |= ROLE_ALL;
		if (strcmp(argv[i], "--main") == 0)
			role |= ROLE_MAIN;
		if (strcmp(argv[i], "--navigation") == 0)
			role |= ROLE_NAVIGATION;
		if (strcmp(argv[i], "--weapons") == 0)
			role |= ROLE_WEAPONS;
		if (strcmp(argv[i], "--engineering") == 0)
			role |= ROLE_ENGINEERING;
		if (strcmp(argv[i], "--science") == 0)
			role |= ROLE_SCIENCE;
		if (strcmp(argv[i], "--comms") == 0)
			role |= ROLE_COMMS;
		if (strcmp(argv[i], "--soundserver") == 0)
			role |= ROLE_SOUNDSERVER;
	}
	if (role == 0)
		role = ROLE_ALL;

	snis_object_pool_setup(&pool, MAXGAMEOBJS);
	snis_object_pool_setup(&sparkpool, MAXSPARKS);
	snis_object_pool_setup(&damcon_pool, MAXDAMCONENTITIES);
	memset(dco, 0, sizeof(dco));
	damconscreenx = NULL;
	damconscreeny = NULL;

	ignore_sigpipe();

	setup_sound();

	if (displaymode != DISPLAYMODE_NETWORK_SETUP)
		connect_to_lobby();

	real_screen_width = SCREEN_WIDTH;
	real_screen_height = SCREEN_HEIGHT;
	sng_set_scale(xscale_screen, yscale_screen);

	gtk_set_locale();
	gtk_init (&argc, &argv);

	init_keymap();
	read_keymap_config_file();
	init_vects();
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

        gc = gdk_gc_new(GTK_WIDGET(main_da)->window);
	sng_set_gc(gc);
	sng_set_foreground(WHITE);

	timer_tag = g_timeout_add(1000 / frame_rate_hz, advance_game, NULL);

	init_meshes();

	/* Apparently (some versions of?) portaudio calls g_thread_init(). */
	/* It may only be called once, and subsequent calls abort, so */
	/* only call it if the thread system is not already initialized. */
	if (!g_thread_supported ())
		g_thread_init(NULL);
	gdk_threads_init();

	gettimeofday(&start_time, NULL);

	snis_slider_set_sound(SLIDER_SOUND);
	text_window_set_chatter_sound(TTY_CHATTER_SOUND);
	text_window_set_timer(&timer);
	init_trig_arrays();
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
	ecx = entity_context_new(5000);

	snis_protocol_debugging(1);

	set_default_clip_window();


	gtk_main ();
        wwviaudio_cancel_all_sounds();
        wwviaudio_stop_portaudio();
	close_joystick();
	return 0;
}
