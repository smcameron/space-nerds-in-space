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
#include <sys/types.h>
#include <stdint.h>
#include <malloc.h>
#include <gtk/gtk.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <gdk/gdkkeysyms.h>
#include <math.h>
#include <errno.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netdb.h>
#include <assert.h>

#include "snis.h"
#include "mathutils.h"
#include "snis_alloc.h"
#include "my_point.h"
#include "snis_font.h"
#include "snis_socket_io.h"
#include "ssgl/ssgl.h"
#include "snis_marshal.h"
#include "snis_packet.h"

#define SCREEN_WIDTH 800        /* window width, in pixels */
#define SCREEN_HEIGHT 600       /* window height, in pixels */


typedef void line_drawing_function(GdkDrawable *drawable,
         GdkGC *gc, gint x1, gint y1, gint x2, gint y2);

typedef void bright_line_drawing_function(GdkDrawable *drawable,
         GdkGC *gc, gint x1, gint y1, gint x2, gint y2, int color);

typedef void rectangle_drawing_function(GdkDrawable *drawable,
        GdkGC *gc, gboolean filled, gint x, gint y, gint width, gint height);

typedef void explosion_function(int x, int y, int ivx, int ivy, int v, int nsparks, int time);

typedef void arc_drawing_function(GdkDrawable *drawable, GdkGC *gc,
	gboolean filled, gint x, gint y, gint width, gint height, gint angle1, gint angle2);

line_drawing_function *current_draw_line = gdk_draw_line;
rectangle_drawing_function *current_draw_rectangle = gdk_draw_rectangle;
bright_line_drawing_function *current_bright_line = NULL;
explosion_function *explosion = NULL;
arc_drawing_function *current_draw_arc = gdk_draw_arc;

/* I can switch out the line drawing function with these macros */
/* in case I come across something faster than gdk_draw_line */
#define DEFAULT_LINE_STYLE current_draw_line
#define DEFAULT_RECTANGLE_STYLE current_draw_rectangle
#define DEFAULT_BRIGHT_LINE_STYLE current_bright_line
#define DEFAULT_DRAW_ARC current_draw_arc

#define snis_draw_line DEFAULT_LINE_STYLE
#define snis_draw_rectangle DEFAULT_RECTANGLE_STYLE
#define snis_bright_line DEFAULT_BRIGHT_LINE_STYLE
#define snis_draw_arc DEFAULT_DRAW_ARC
int thicklines = 0;
int frame_rate_hz = 30;

GtkWidget *window;
GdkGC *gc = NULL;               /* our graphics context. */
GtkWidget *main_da;             /* main drawing area. */
gint timer_tag;  
int fullscreen = 0;
int in_the_process_of_quitting = 0;

uint8_t role = 255;
char *password;
char *shipname;
#define UNKNOWN_ID 0xffffffff
uint32_t my_ship_id = UNKNOWN_ID;
uint32_t my_ship_oid = UNKNOWN_ID;

float xscale_screen;
float yscale_screen;
int real_screen_width;
int real_screen_height;

#define DISPLAYMODE_FONTTEST 0
#define DISPLAYMODE_INTROSCREEN 1
#define DISPLAYMODE_LOBBYSCREEN 2
#define DISPLAYMODE_CONNECTING 3
#define DISPLAYMODE_CONNECTED 4
#define DISPLAYMODE_FINDSERVER 5
#define DISPLAYMODE_MAINSCREEN 6
#define DISPLAYMODE_HELM 7
#define DISPLAYMODE_WEAPONS 8
#define DISPLAYMODE_ENGINEERING 9
#define DISPLAYMODE_SCIENCE 10
#define DISPLAYMODE_COMMS 11
#define DISPLAYMODE_DEBUG 12

int displaymode = DISPLAYMODE_LOBBYSCREEN;

/* cardinal color indexes into huex array */
#define WHITE 0
#define BLUE 1
#define BLACK 2
#define GREEN 3
#define YELLOW 4
#define RED 5
#define ORANGE 6
#define CYAN 7
#define MAGENTA 8
#define DARKGREEN 9
#define DARKRED 10

#define NCOLORS 11              /* number of "cardinal" colors */
#define NSPARKCOLORS 25         /* 25 shades from yellow to red for the sparks */
#define NRAINBOWSTEPS (16)
#define NRAINBOWCOLORS (NRAINBOWSTEPS*3)

GdkColor huex[NCOLORS + NSPARKCOLORS + NRAINBOWCOLORS]; /* all the colors we have to work with are in here */

int nframes = 0;
int timer = 0;
struct timeval start_time, end_time;

/* There are 4 home-made "fonts" in the game, all the same "typeface", but 
 * different sizes */
struct my_vect_obj **gamefont[4];
/* indexes into the gamefont array */
#define BIG_FONT 0
#define SMALL_FONT 1
#define TINY_FONT 2
#define NANO_FONT 3

/* sizes of the fonts... in arbitrary units */
#define BIG_FONT_SCALE 14 
#define SMALL_FONT_SCALE 5 
#define TINY_FONT_SCALE 3 
#define NANO_FONT_SCALE 2 

/* spacing of letters between the fonts, pixels */
#define BIG_LETTER_SPACING (10)
#define SMALL_LETTER_SPACING (5)
#define TINY_LETTER_SPACING (3)
#define NANO_LETTER_SPACING (2)

/* for getting at the font scales and letter spacings, given  only font numbers */
int font_scale[] = { BIG_FONT_SCALE, SMALL_FONT_SCALE, TINY_FONT_SCALE, NANO_FONT_SCALE };
int letter_spacing[] = { BIG_LETTER_SPACING, SMALL_LETTER_SPACING, TINY_LETTER_SPACING, NANO_LETTER_SPACING };

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

double sine[361];
double cosine[361];

void init_trig_arrays(void)
{
	int i;

	for (i = 0; i <= 360; i++) {
		sine[i] = sin((double)i * M_PI * 2.0 / 360.0);
		cosine[i] = cos((double)i * M_PI * 2.0 / 360.0);
	}
}

static void *connect_to_lobby_thread(__attribute__((unused)) void *arg)
{
	int i, sock, rc, game_server_count;
	struct ssgl_game_server *game_server = NULL;
	struct ssgl_client_filter filter;

try_again:

	/* Loop, trying to connect to the lobby server... */
	strcpy(lobbyerror, "");
	while (1) {
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
	strcpy(lobbyerror, "");

	/* Ok, we've connected to the lobby server... */

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
        pthread_attr_init(&lobby_attr);
        pthread_attr_setdetachstate(&lobby_attr, PTHREAD_CREATE_JOINABLE);
	(void) pthread_create(&lobby_thread, &lobby_attr, connect_to_lobby_thread, NULL);
	return;
}

static struct snis_object_pool *pool;
static struct snis_entity go[MAXGAMEOBJS];
static struct snis_object_pool *sparkpool;
static struct snis_entity spark[MAXSPARKS];
static pthread_mutex_t universe_mutex = PTHREAD_MUTEX_INITIALIZER;

static int add_generic_object(uint32_t id, double x, double y, double vx, double vy, double heading, int type, uint32_t alive)
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
	go[i].vx = vx;
	go[i].vy = vy;
	go[i].heading = heading;
	go[i].type = type;
	go[i].alive = alive;
	return i;
}

static void update_generic_object(int index, double x, double y, double vx, double vy, double heading, uint32_t alive)
{
	go[index].x = x;
	go[index].y = y;
	go[index].vx = vx;
	go[index].vy = vy;
	go[index].heading = heading;
	go[index].alive = alive;
}

static int __attribute__((unused)) add_player(uint32_t id, double x, double y, double vx, double vy, double heading, uint32_t alive)
{
	return add_generic_object(id, x, y, vx, vy, heading, OBJTYPE_SHIP1, alive);
}

static int lookup_object_by_id(uint32_t id)
{
	int i;

	for (i = 0; i <= snis_object_pool_highest_object(pool); i++)
		if (go[i].id == id)
			return i;
	return -1;
}

static int update_ship(uint32_t id, double x, double y, double vx, double vy, double heading, uint32_t alive,
			uint32_t torpedoes, uint32_t energy, uint32_t shields, double gun_heading)
{
	int i;
	i = lookup_object_by_id(id);
	if (i < 0) {
		i = add_generic_object(id, x, y, vx, vy, heading, OBJTYPE_SHIP1, alive);
		if (i < 0)
			return i;
	} else {
		update_generic_object(i, x, y, vx, vy, heading, alive); 
	}
	go[i].tsd.ship.torpedoes = torpedoes;
	go[i].tsd.ship.energy = energy;
	go[i].tsd.ship.shields = shields;
	go[i].tsd.ship.gun_heading = gun_heading;
	return 0;
}

static int update_torpedo(uint32_t id, double x, double y, double vx, double vy)
{
	int i;
	i = lookup_object_by_id(id);
	if (i < 0) {
		i = add_generic_object(id, x, y, vx, vy, 0.0, OBJTYPE_TORPEDO, 1);
		if (i < 0)
			return i;
	} else {
		update_generic_object(i, x, y, vx, vy, 0.0, 1); 
	}
	return 0;
}

static int update_planet(uint32_t id, double x, double y)
{
	int i;
	i = lookup_object_by_id(id);
	if (i < 0) {
		i = add_generic_object(id, x, y, 0.0, 0.0, 0.0, OBJTYPE_PLANET, 1);
		if (i < 0)
			return i;
	} else {
		update_generic_object(i, x, y, 0.0, 0.0, 0.0, 1);
	}
	return 0;
}

static int update_starbase(uint32_t id, double x, double y)
{
	int i;
	i = lookup_object_by_id(id);
	if (i < 0) {
		i = add_generic_object(id, x, y, 0.0, 0.0, 0.0, OBJTYPE_STARBASE, 1);
		if (i < 0)
			return i;
	} else {
		update_generic_object(i, x, y, 0.0, 0.0, 0.0, 1);
	}
	return 0;
}


static void spark_move(struct snis_entity *o)
{
	o->x += o->vx;
	o->y += o->vy;
	o->alive--;
	if (o->alive <= 0)
		snis_object_pool_free_object(sparkpool, o->index);
}

static void move_sparks(void)
{
	int i;

	for (i = 0; i <= snis_object_pool_highest_object(sparkpool); i++)
		if (spark[i].alive)
			spark[i].move(&spark[i]);
}

void add_spark(double x, double y, double vx, double vy, int time)
{
	int i;

	i = snis_object_pool_alloc_obj(sparkpool);
	if (i < 0)
		return;
	memset(&spark[i], 0, sizeof(spark[i]));
	spark[i].index = i;
	spark[i].x = x;
	spark[i].y = y;
	spark[i].vx = vx;
	spark[i].vy = vy;
	spark[i].type = OBJTYPE_SPARK;
	printf("adding spark, time = %d\n", time);
	spark[i].alive = time;
	spark[i].move = spark_move;
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

static void do_explosion(double x, double y, uint16_t nsparks, uint16_t velocity, int time)
{
	double angle, v, vx, vy;
	int i;

	for (i = 0; i < nsparks; i++) {
		angle = ((double) snis_randn(360) * M_PI / 180.0);
		v = snis_randn(velocity * 2) - velocity;
		vx = v * cos(angle);
		vy = v * sin(angle);
		add_spark(x, y, vx, vy, time);
	}
}

static int update_explosion(uint32_t id, double x, double y,
		uint16_t nsparks, uint16_t velocity, uint16_t time)
{
	int i;
	i = lookup_object_by_id(id);
	if (i < 0) {
		i = add_generic_object(id, x, y, 0.0, 0.0, 0.0, OBJTYPE_EXPLOSION, 1);
		if (i < 0)
			return i;
		go[i].tsd.explosion.nsparks = nsparks;
		go[i].tsd.explosion.velocity = velocity;
		do_explosion(x, y, nsparks, velocity, (int) time);
	}
	return 0;
}

static int __attribute__((unused)) add_planet(uint32_t id, double x, double y, double vx, double vy, double heading, uint32_t alive)
{
	return add_generic_object(id, x, y, vx, vy, heading, OBJTYPE_PLANET, alive);
}

static int __attribute__((unused)) add_starbase(uint32_t id, double x, double y, double vx, double vy, double heading, uint32_t alive)
{
	return add_generic_object(id, x, y, vx, vy, heading, OBJTYPE_STARBASE, alive);
}

static int __attribute__((unused)) add_laser(uint32_t id, double x, double y, double vx, double vy, double heading, uint32_t alive)
{
	return add_generic_object(id, x, y, vx, vy, heading, OBJTYPE_LASER, alive);
}

static int __attribute__((unused)) add_torpedo(uint32_t id, double x, double y, double vx, double vy, double heading, uint32_t alive)
{
	return add_generic_object(id, x, y, vx, vy, heading, OBJTYPE_TORPEDO, alive);
}

void spin_points(struct my_point_t *points, int npoints, 
	struct my_point_t **spun_points, int nangles,
	int originx, int originy)
{
	int i, j;
	double angle;
	double angle_inc;
	double startx, starty, start_angle, new_angle, newx, newy;
	double magnitude, diff;

	*spun_points = (struct my_point_t *) 
		malloc(sizeof(*spun_points) * npoints * nangles);
	if (*spun_points == NULL)
		return;

	angle_inc = (2.0*3.1415927) / (nangles);

	for (i = 0; i < nangles; i++) {
		angle = angle_inc * (double) i;
		printf("Rotation angle = %f\n", angle * 180.0/3.1415927);
		for (j=0;j<npoints;j++) {
			startx = (double) (points[j].x - originx);
			starty = (double) (points[j].y - originy);
			magnitude = sqrt(startx*startx + starty*starty);
			diff = (startx - 0);
			if (diff < 0 && diff > -0.00001)
				start_angle = 0.0;
			else if (diff > 00 && diff < 0.00001)
				start_angle = 3.1415927;
			else
				start_angle = atan2(starty, startx);
			new_angle = start_angle + angle;
			newx = cos(new_angle) * magnitude + originx;
			newy = sin(new_angle) * magnitude + originy;
			(*spun_points)[i*npoints + j].x = newx;
			(*spun_points)[i*npoints + j].y = newy;
			printf("s=%f,%f, e=%f,%f, angle = %f/%f\n", 
				startx, starty, newx, newy, 
				start_angle * 180/3.1415927, new_angle * 360.0 / (2.0*3.1415927)); 
		}
	} 
}

void scaled_line(GdkDrawable *drawable,
	GdkGC *gc, gint x1, gint y1, gint x2, gint y2)
{
	gdk_draw_line(drawable, gc, x1*xscale_screen, y1*yscale_screen,
		x2*xscale_screen, y2*yscale_screen);
}

void scaled_arc(GdkDrawable *drawable, GdkGC *gc,
	gboolean filled, gint x, gint y, gint width, gint height, gint angle1, gint angle2)
{
	gdk_draw_arc(drawable, gc, filled, x * xscale_screen, y * yscale_screen,
			width * xscale_screen, height * yscale_screen, angle1, angle2);
}

void thick_scaled_line(GdkDrawable *drawable,
	GdkGC *gc, gint x1, gint y1, gint x2, gint y2)
{
	int sx1,sy1,sx2,sy2,dx,dy;

	if (abs(x1-x2) > abs(y1-y2)) {
		dx = 0;
		dy = 1;
	} else {
		dx = 1;
		dy = 0;
	}
	sx1 = x1*xscale_screen;
	sx2 = x2*xscale_screen;
	sy1 = y1*yscale_screen;	
	sy2 = y2*yscale_screen;	
	
	gdk_draw_line(drawable, gc, sx1,sy1,sx2,sy2);
	gdk_draw_line(drawable, gc, sx1-dx,sy1-dy,sx2-dx,sy2-dy);
	gdk_draw_line(drawable, gc, sx1+dx,sy1+dy,sx2+dx,sy2+dy);
}

void scaled_rectangle(GdkDrawable *drawable,
	GdkGC *gc, gboolean filled, gint x, gint y, gint width, gint height)
{
	gdk_draw_rectangle(drawable, gc, filled, x*xscale_screen, y*yscale_screen,
		width*xscale_screen, height*yscale_screen);
}

void scaled_bright_line(GdkDrawable *drawable,
	GdkGC *gc, gint x1, gint y1, gint x2, gint y2, int color)
{
	int sx1,sy1,sx2,sy2,dx,dy;

	if (abs(x1-x2) > abs(y1-y2)) {
		dx = 0;
		dy = 1;
	} else {
		dx = 1;
		dy = 0;
	}
	sx1 = x1*xscale_screen;
	sx2 = x2*xscale_screen;
	sy1 = y1*yscale_screen;	
	sy2 = y2*yscale_screen;	
	
	gdk_gc_set_foreground(gc, &huex[WHITE]);
	gdk_draw_line(drawable, gc, sx1,sy1,sx2,sy2);
	gdk_gc_set_foreground(gc, &huex[color]);
	gdk_draw_line(drawable, gc, sx1-dx,sy1-dy,sx2-dx,sy2-dy);
	gdk_draw_line(drawable, gc, sx1+dx,sy1+dy,sx2+dx,sy2+dy);
}

void unscaled_bright_line(GdkDrawable *drawable,
	GdkGC *gc, gint x1, gint y1, gint x2, gint y2, int color)
{
	int dx,dy;

	if (abs(x1-x2) > abs(y1-y2)) {
		dx = 0;
		dy = 1;
	} else {
		dx = 1;
		dy = 0;
	}
	
	gdk_gc_set_foreground(gc, &huex[WHITE]);
	gdk_draw_line(drawable, gc, x1,y1,x2,y2);
	gdk_gc_set_foreground(gc, &huex[color]);
	gdk_draw_line(drawable, gc, x1-dx,y1-dy,x2-dx,y2-dy);
	gdk_draw_line(drawable, gc, x1+dx,y1+dy,x2+dx,y2+dy);
}

/* Draws a letter in the given font at an absolute x,y coords on the screen. */
static int abs_xy_draw_letter(GtkWidget *w, struct my_vect_obj **font, 
		unsigned char letter, int x, int y)
{
	int i, x1, y1, x2, y2;
	int minx, maxx, diff;

	if (letter == ' ' || letter == '\n' || letter == '\t' || font[letter] == NULL)
		return abs(font['Z']->p[0].x - font['Z']->p[1].x);

	for (i = 0; i < font[letter]->npoints-1; i++) {
		if (font[letter]->p[i+1].x == LINE_BREAK)
			i += 2;
		x1 = x + font[letter]->p[i].x;
		y1 = y + font[letter]->p[i].y;
		x2 = x + font[letter]->p[i + 1].x;
		y2 = y + font[letter]->p[i + 1].y;

		if (i == 0) {
			minx = x1;
			maxx = x1;
		}

		if (x1 < minx)
			minx = x1;
		if (x2 < minx)
			minx = x2;
		if (x1 > maxx)
			maxx = x1;
		if (x2 > maxx)
			maxx = x2;
		
		if (x1 > 0 && x2 > 0)
			snis_draw_line(w->window, gc, x1, y1, x2, y2); 
	}
	diff = abs(maxx - minx);
	/* if (diff == 0)
		return (abs(font['Z']->p[0].x - font['Z']->p[1].x) / 4); */
	return diff; 
}

/* Used for floating labels in the game. */
/* Draws a string at an absolute x,y position on the screen. */ 
static void abs_xy_draw_string(GtkWidget *w, char *s, int font, int x, int y) 
{

	int i, dx;	
	int deltax = 0;

	for (i=0;s[i];i++) {
		dx = (font_scale[font]) + abs_xy_draw_letter(w, gamefont[font], s[i], x + deltax, y);  
		deltax += dx;
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
	{ "f9", GDK_F9 }, 
	{ "f9", GDK_F9 }, 
	{ "f10", GDK_F10 }, 
	{ "f11", GDK_F11 }, 
	{ "f12", GDK_F12 }, 
};

enum keyaction { keynone, keydown, keyup, keyleft, keyright, 
		keytorpedo, keytransform, 
		keyquarter, keypause, key2, key3, key4, key5, key6,
		key7, key8, keysuicide, keyfullscreen, keythrust, 
		keysoundeffects, keymusic, keyquit, keytogglemissilealarm,
		keypausehelp, keyreverse, keyf1, keyf2, keyf3, keyf4, keyf5,
		keyf6, keyf7
};

enum keyaction keymap[256];
enum keyaction ffkeymap[256];
unsigned char *keycharmap[256];

char *keyactionstring[] = {
	"none", "down", "up", "left", "right", 
	"laser", "bomb", "chaff", "gravitybomb",
	"quarter", "pause", "2x", "3x", "4x", "5x", "6x",
	"7x", "8x", "suicide", "fullscreen", "thrust", 
	"soundeffect", "music", "quit", "missilealarm", "help", "reverse"
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

	keymap[GDK_space] = keytorpedo;
	keymap[GDK_z] = keytorpedo;

	keymap[GDK_b] = keytransform;
	keymap[GDK_x] = keythrust;
	keymap[GDK_p] = keypause;
	ffkeymap[GDK_F1 & 0x00ff] = keypausehelp;
	keymap[GDK_q] = keyquarter;
	keymap[GDK_m] = keymusic;
	keymap[GDK_s] = keysoundeffects;
	ffkeymap[GDK_Escape & 0x00ff] = keyquit;
	keymap[GDK_1] = keytogglemissilealarm;

	keymap[GDK_2] = key2;
	keymap[GDK_3] = key3;
	keymap[GDK_4] = key4;
	keymap[GDK_5] = key5;
	keymap[GDK_6] = key6;
	keymap[GDK_7] = key7;
	keymap[GDK_8] = key8;
	keymap[GDK_9] = keysuicide;

	ffkeymap[GDK_F1 & 0x00ff] = keyf1;
	ffkeymap[GDK_F2 & 0x00ff] = keyf2;
	ffkeymap[GDK_F3 & 0x00ff] = keyf3;
	ffkeymap[GDK_F4 & 0x00ff] = keyf4;
	ffkeymap[GDK_F5 & 0x00ff] = keyf5;
	ffkeymap[GDK_F6 & 0x00ff] = keyf6;
	ffkeymap[GDK_F7 & 0x00ff] = keyf7;

	ffkeymap[GDK_F11 & 0x00ff] = keyfullscreen;
}

static void wakeup_gameserver_writer(void);

static void helm_dirkey(int h, int v)
{
	struct packed_buffer *pb;
	uint8_t yaw, thrust;

	if (!h && !v)
		return;

	if (h) {
		yaw = h < 0 ? YAW_LEFT : YAW_RIGHT;
		pb = packed_buffer_allocate(sizeof(struct request_yaw_packet));
		packed_buffer_append_u16(pb, OPCODE_REQUEST_YAW);
		packed_buffer_append_u8(pb, yaw);
		packed_buffer_queue_add(&to_server_queue, pb, &to_server_queue_mutex);
	}
	if (v) {
		thrust = v < 0 ? THRUST_BACKWARDS : THRUST_FORWARDS;
		pb = packed_buffer_allocate(sizeof(struct request_thrust_packet));
		packed_buffer_append_u16(pb, OPCODE_REQUEST_THRUST);
		packed_buffer_append_u8(pb, thrust);
		packed_buffer_queue_add(&to_server_queue, pb, &to_server_queue_mutex);
	}
	wakeup_gameserver_writer();
}

static void weapons_dirkey(int h, int v)
{
	struct packed_buffer *pb;
	uint8_t yaw;

	if (!h && !v)
		return;

	if (h) {
		yaw = h < 0 ? YAW_LEFT : YAW_RIGHT;
		pb = packed_buffer_allocate(sizeof(struct request_yaw_packet));
		packed_buffer_append_u16(pb, OPCODE_REQUEST_GUNYAW);
		packed_buffer_append_u8(pb, yaw);
		packed_buffer_queue_add(&to_server_queue, pb, &to_server_queue_mutex);
		wakeup_gameserver_writer();
	}
}

static void do_dirkey(int h, int v)
{
	switch (displaymode) {
		case DISPLAYMODE_HELM:
			helm_dirkey(h, v); 
			break;
		case DISPLAYMODE_WEAPONS:
			weapons_dirkey(h, v); 
			break;
		default:
			break;
	}
	return;
}

static void do_torpedo(void)
{
	struct packed_buffer *pb;

	switch (displaymode) {
	case DISPLAYMODE_WEAPONS:
		pb = packed_buffer_allocate(sizeof(struct request_yaw_packet));
		packed_buffer_append_u16(pb, OPCODE_REQUEST_TORPEDO);
		packed_buffer_queue_add(&to_server_queue, pb, &to_server_queue_mutex);
		wakeup_gameserver_writer();
		break;
	default:
		break;
	}
}

static gint key_press_cb(GtkWidget* widget, GdkEventKey* event, gpointer data)
{
	enum keyaction ka;
        if ((event->keyval & 0xff00) == 0) 
                ka = keymap[event->keyval];
        else
                ka = ffkeymap[event->keyval & 0x00ff];

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
	case keyquit:	in_the_process_of_quitting = !in_the_process_of_quitting;
			break;
	case keyleft:
		do_dirkey(-1, 0);
		break;
	case keyright:
		do_dirkey(1, 0);
		break;
	case keyup:
		do_dirkey(0, -1);
		break;
	case keydown:
		do_dirkey(0, 1);
		break;
	case keytorpedo:
		do_torpedo();
		break;
	case keyf1:
		if (displaymode < DISPLAYMODE_CONNECTED)
			break;
		displaymode = DISPLAYMODE_MAINSCREEN;
		break;
	case keyf2:
		if (displaymode < DISPLAYMODE_CONNECTED)
			break;
		displaymode = DISPLAYMODE_HELM;
		break;
	case keyf3:
		if (displaymode < DISPLAYMODE_CONNECTED)
			break;
		displaymode = DISPLAYMODE_WEAPONS;
		break;
	case keyf4:
		if (displaymode < DISPLAYMODE_CONNECTED)
			break;
		displaymode = DISPLAYMODE_ENGINEERING;
		break;
	case keyf5:
		if (displaymode < DISPLAYMODE_CONNECTED)
			break;
		displaymode = DISPLAYMODE_SCIENCE;
		break;
	case keyf6:
		if (displaymode < DISPLAYMODE_CONNECTED)
			break;
		displaymode = DISPLAYMODE_COMMS;
		break;
	case keyf7:
		if (displaymode < DISPLAYMODE_CONNECTED)
			break;
		displaymode = DISPLAYMODE_DEBUG;
		break;
	default:
		break;
	}
	return FALSE;
}

static gint key_release_cb(GtkWidget* widget, GdkEventKey* event, gpointer data)
{
	return FALSE;
}

/* keyboard handling stuff ends */
/**********************************/


static void show_fonttest(GtkWidget *w)
{
	abs_xy_draw_string(w, "A B C D E F G H I J K L M", SMALL_FONT, 30, 30); 
	abs_xy_draw_string(w, "N O P Q R S T U V W X Y Z", SMALL_FONT, 30, 60); 
	abs_xy_draw_string(w, "a b c d e f g h i j k l m", SMALL_FONT, 30, 90); 
	abs_xy_draw_string(w, "n o p q r s t u v w x y z", SMALL_FONT, 30, 120); 
	abs_xy_draw_string(w, "0 1 2 3 4 5 6 7 8 9 ! , .", SMALL_FONT, 30, 150); 
	abs_xy_draw_string(w, "? | - = * / \\ + ( ) \" ' _", SMALL_FONT, 30, 180); 

	abs_xy_draw_string(w, "The Quick Fox Jumps Over The Lazy Brown Dog.", SMALL_FONT, 30, 210); 
	abs_xy_draw_string(w, "The Quick Fox Jumps Over The Lazy Brown Dog.", BIG_FONT, 30, 280); 
	abs_xy_draw_string(w, "The Quick Fox Jumps Over The Lazy Brown Dog.", TINY_FONT, 30, 350); 
	abs_xy_draw_string(w, "The quick fox jumps over the lazy brown dog.", NANO_FONT, 30, 380); 
	abs_xy_draw_string(w, "THE QUICK FOX JUMPS OVER THE LAZY BROWN DOG.", TINY_FONT, 30, 410); 
	abs_xy_draw_string(w, "Well now, what have we here?  James Bond!", NANO_FONT, 30, 425); 
	abs_xy_draw_string(w, "The quick fox jumps over the lazy brown dog.", SMALL_FONT, 30, 450); 
	abs_xy_draw_string(w, "Copyright (C) 2010 Stephen M. Cameron 0123456789", TINY_FONT, 30, 480); 
}

static void show_introscreen(GtkWidget *w)
{
	abs_xy_draw_string(w, "Space Nerds", BIG_FONT, 80, 200); 
	abs_xy_draw_string(w, "In Space", BIG_FONT, 180, 320); 
	abs_xy_draw_string(w, "Copyright (C) 2010 Stephen M. Cameron", NANO_FONT, 255, 550); 
}

int lobbylast1clickx = -1;
int lobbylast1clicky = -1;
int lobby_selected_server = -1;
static void show_lobbyscreen(GtkWidget *w)
{
	char msg[100];
	int i;
#define STARTLINE 100
#define LINEHEIGHT 30

	gdk_gc_set_foreground(gc, &huex[WHITE]);
	if (lobby_socket == -1) {
		abs_xy_draw_string(w, "Space Nerds", BIG_FONT, 80, 200); 
		abs_xy_draw_string(w, "In Space", BIG_FONT, 180, 320); 
		abs_xy_draw_string(w, "Copyright (C) 2010 Stephen M. Cameron", NANO_FONT, 255, 550); 
		sprintf(msg, "Connecting to lobby... tried %d times.",
			lobby_count);
		abs_xy_draw_string(w, msg, SMALL_FONT, 100, 400);
		abs_xy_draw_string(w, lobbyerror, NANO_FONT, 100, 430);
	} else {
		if (lobby_selected_server != -1 &&
			lobbylast1clickx > 200 && lobbylast1clickx < 620 &&
			lobbylast1clicky > 520 && lobbylast1clicky < 520 + LINEHEIGHT * 2) {
			displaymode = DISPLAYMODE_CONNECTING;
			return;
		}
		lobby_selected_server = -1;
		sprintf(msg, "Connected to lobby on socket %d\n", lobby_socket);
		abs_xy_draw_string(w, msg, TINY_FONT, 30, LINEHEIGHT);
		sprintf(msg, "Total game servers: %d\n", ngameservers);
		abs_xy_draw_string(w, msg, TINY_FONT, 30, LINEHEIGHT + 20);
		for (i = 0; i < ngameservers; i++) {
			unsigned char *x = (unsigned char *) 
				&lobby_game_server[i].ipaddr;
			if (lobbylast1clickx > 30 && lobbylast1clickx < 700 &&
				lobbylast1clicky > 100 + (-0.5 + i) * LINEHEIGHT &&
				lobbylast1clicky < 100 + (0.5 + i) * LINEHEIGHT) {
				lobby_selected_server = i;
				gdk_gc_set_foreground(gc, &huex[GREEN]);
				snis_draw_rectangle(w->window, gc, 0, 25, 100 + (-0.5 + i) * LINEHEIGHT,
					725, LINEHEIGHT);
			} else
				gdk_gc_set_foreground(gc, &huex[WHITE]);
			 
			sprintf(msg, "%hu.%hu.%hu.%hu/%hu", x[0], x[1], x[2], x[3], lobby_game_server[i].port);
			abs_xy_draw_string(w, msg, TINY_FONT, 30, 100 + i * LINEHEIGHT);
			sprintf(msg, "%s", lobby_game_server[i].game_instance);
			abs_xy_draw_string(w, msg, TINY_FONT, 350, 100 + i * LINEHEIGHT);
			sprintf(msg, "%s", lobby_game_server[i].server_nickname);
			abs_xy_draw_string(w, msg, TINY_FONT, 450, 100 + i * LINEHEIGHT);
			sprintf(msg, "%s", lobby_game_server[i].location);
			abs_xy_draw_string(w, msg, TINY_FONT, 650, 100 + i * LINEHEIGHT);
		}
		if (lobby_selected_server != -1) {
			gdk_gc_set_foreground(gc, &huex[WHITE]);
			snis_draw_rectangle(w->window, gc, 0, 200, 520, 400, LINEHEIGHT * 2);
			abs_xy_draw_string(w, "CONNECT TO SERVER", TINY_FONT, 280, 520 + LINEHEIGHT);
		}
	}
}

static int process_update_ship_packet(void)
{
	unsigned char buffer[100];
	struct packed_buffer pb;
	uint32_t id, alive, torpedoes, shields, energy;
	uint32_t x, y, heading, gun_heading;
	int32_t vx, vy;
	double dx, dy, dheading, dgheading, dvx, dvy;
	int rc;

	assert(sizeof(buffer) > sizeof(struct update_ship_packet) - sizeof(uint16_t));
	rc = snis_readsocket(gameserver_sock, buffer, sizeof(struct update_ship_packet) - sizeof(uint16_t));
	/* printf("process_update_ship_packet, snis_readsocket returned %d\n", rc); */
	if (rc != 0)
		return rc;

	pb.buffer_size = sizeof(buffer);;
	pb.buffer = buffer;
	pb.buffer_cursor = 0;

	id = packed_buffer_extract_u32(&pb);
	alive = packed_buffer_extract_u32(&pb);
	x = packed_buffer_extract_u32(&pb);
	y = packed_buffer_extract_u32(&pb);
	vx = (int32_t) packed_buffer_extract_u32(&pb);
	vy = (int32_t) packed_buffer_extract_u32(&pb);
	heading = packed_buffer_extract_u32(&pb);
	torpedoes = packed_buffer_extract_u32(&pb);
	energy = packed_buffer_extract_u32(&pb);
	shields = packed_buffer_extract_u32(&pb);
	gun_heading = packed_buffer_extract_u32(&pb);

	dx = ((double) x * (double) XUNIVERSE_DIMENSION) / (double ) UINT32_MAX;
	dy = ((double) y * (double) YUNIVERSE_DIMENSION) / (double ) UINT32_MAX;
	dvx = ((double) vx * (double) XUNIVERSE_DIMENSION) / (double) INT32_MAX;
	dvy = ((double) vy * (double) YUNIVERSE_DIMENSION) / (double) INT32_MAX;
	dheading = (double) heading * 360.0 / (double) UINT32_MAX;
	dgheading = (double) gun_heading * 360.0 / (double) UINT32_MAX;

	pthread_mutex_lock(&universe_mutex);
	rc = update_ship(id, dx, dy, dvx, dvy, dheading, alive, torpedoes, energy, shields, dgheading);
	pthread_mutex_unlock(&universe_mutex);
	return (rc < 0);

} 

static int process_update_torpedo_packet(void)
{
	unsigned char buffer[100];
	struct packed_buffer pb;
	uint32_t id;
	uint32_t x, y;
	int32_t vx, vy;
	double dx, dy, dvx, dvy;
	int rc;

	assert(sizeof(buffer) > sizeof(struct update_torpedo_packet) - sizeof(uint16_t));
	rc = snis_readsocket(gameserver_sock, buffer, sizeof(struct update_torpedo_packet) -
					sizeof(uint16_t));
	if (rc != 0)
		return rc;

	pb.buffer_size = sizeof(buffer);;
	pb.buffer = buffer;
	pb.buffer_cursor = 0;

	id = packed_buffer_extract_u32(&pb);
	x = packed_buffer_extract_u32(&pb);
	y = packed_buffer_extract_u32(&pb);
	vx = (int32_t) packed_buffer_extract_u32(&pb);
	vy = (int32_t) packed_buffer_extract_u32(&pb);

	dx = ((double) x * (double) XUNIVERSE_DIMENSION) / (double ) UINT32_MAX;
	dy = ((double) y * (double) YUNIVERSE_DIMENSION) / (double ) UINT32_MAX;
	dvx = ((double) vx * (double) XUNIVERSE_DIMENSION) / (double) INT32_MAX;
	dvy = ((double) vy * (double) YUNIVERSE_DIMENSION) / (double) INT32_MAX;

	pthread_mutex_lock(&universe_mutex);
	rc = update_torpedo(id, dx, dy, dvx, dvy);
	pthread_mutex_unlock(&universe_mutex);
	return (rc < 0);
} 

static void delete_object(uint32_t id)
{
	int i;

	i = lookup_object_by_id(id);
	/* perhaps we just joined and so don't know about this object. */
	if (i < 0)
		return;
	go[i].alive = 0;
	snis_object_pool_free_object(pool, i);
}

static int process_delete_object_packet(void)
{
	unsigned char buffer[10];
	struct packed_buffer pb;
	uint32_t id;
	int rc;

	assert(sizeof(buffer) > sizeof(struct delete_object_packet) - sizeof(uint16_t));
	rc = snis_readsocket(gameserver_sock, buffer, sizeof(struct delete_object_packet) -
					sizeof(uint16_t));
	if (rc != 0)
		return rc;

	pb.buffer_size = sizeof(buffer);;
	pb.buffer = buffer;
	pb.buffer_cursor = 0;
	id = packed_buffer_extract_u32(&pb);

	pthread_mutex_lock(&universe_mutex);
	delete_object(id);
	pthread_mutex_unlock(&universe_mutex);
	return 0;
}

static int process_update_planet_packet(void)
{
	unsigned char buffer[100];
	struct packed_buffer pb;
	uint32_t id;
	uint32_t x, y;
	double dx, dy;
	int rc;

	assert(sizeof(buffer) > sizeof(struct update_planet_packet) - sizeof(uint16_t));
	rc = snis_readsocket(gameserver_sock, buffer, sizeof(struct update_planet_packet) - sizeof(uint16_t));
	if (rc != 0)
		return rc;

	pb.buffer_size = sizeof(buffer);;
	pb.buffer = buffer;
	pb.buffer_cursor = 0;

	id = packed_buffer_extract_u32(&pb);
	x = packed_buffer_extract_u32(&pb);
	y = packed_buffer_extract_u32(&pb);

	dx = ((double) x * (double) XUNIVERSE_DIMENSION) / (double ) UINT32_MAX;
	dy = ((double) y * (double) YUNIVERSE_DIMENSION) / (double ) UINT32_MAX;

	pthread_mutex_lock(&universe_mutex);
	rc = update_planet(id, dx, dy);
	pthread_mutex_unlock(&universe_mutex);
	return (rc < 0);
} 

static int process_update_starbase_packet(void)
{
	unsigned char buffer[100];
	struct packed_buffer pb;
	uint32_t id;
	uint32_t x, y;
	double dx, dy;
	int rc;

	assert(sizeof(buffer) > sizeof(struct update_starbase_packet) - sizeof(uint16_t));
	rc = snis_readsocket(gameserver_sock, buffer, sizeof(struct update_starbase_packet) - sizeof(uint16_t));
	if (rc != 0)
		return rc;

	pb.buffer_size = sizeof(buffer);;
	pb.buffer = buffer;
	pb.buffer_cursor = 0;

	id = packed_buffer_extract_u32(&pb);
	x = packed_buffer_extract_u32(&pb);
	y = packed_buffer_extract_u32(&pb);

	dx = ((double) x * (double) XUNIVERSE_DIMENSION) / (double ) UINT32_MAX;
	dy = ((double) y * (double) YUNIVERSE_DIMENSION) / (double ) UINT32_MAX;

	pthread_mutex_lock(&universe_mutex);
	rc = update_starbase(id, dx, dy);
	pthread_mutex_unlock(&universe_mutex);
	return (rc < 0);
} 

static int process_update_explosion_packet(void)
{
	unsigned char buffer[sizeof(struct update_explosion_packet)];
	struct packed_buffer pb;
	uint32_t id;
	uint32_t x, y;
	double dx, dy;
	uint16_t nsparks, velocity, time;
	int rc;

	assert(sizeof(buffer) > sizeof(struct update_explosion_packet) - sizeof(uint16_t));
	rc = snis_readsocket(gameserver_sock, buffer, sizeof(struct update_explosion_packet)
				- sizeof(uint16_t));
	if (rc != 0)
		return rc;

	pb.buffer_size = sizeof(buffer);;
	pb.buffer = buffer;
	pb.buffer_cursor = 0;

	id = packed_buffer_extract_u32(&pb);
	x = packed_buffer_extract_u32(&pb);
	y = packed_buffer_extract_u32(&pb);
	dx = ((double) x * (double) XUNIVERSE_DIMENSION) / (double ) UINT32_MAX;
	dy = ((double) y * (double) YUNIVERSE_DIMENSION) / (double ) UINT32_MAX;
	nsparks = packed_buffer_extract_u16(&pb);
	velocity = packed_buffer_extract_u16(&pb);
	time = packed_buffer_extract_u16(&pb);

	pthread_mutex_lock(&universe_mutex);
	rc = update_explosion(id, dx, dy, nsparks, velocity, time);
	pthread_mutex_unlock(&universe_mutex);
	return (rc < 0);
}

static int process_client_id_packet(void)
{
	unsigned char buffer[100];
	struct packed_buffer pb;
	uint32_t id;
	int rc;

	assert(sizeof(buffer) > sizeof(struct client_ship_id_packet) - sizeof(uint16_t));
	rc = snis_readsocket(gameserver_sock, buffer, sizeof(struct client_ship_id_packet) - sizeof(uint16_t));

	pb.buffer_size = sizeof(buffer);
	pb.buffer = buffer;
	pb.buffer_cursor = 0;

	id = packed_buffer_extract_u32(&pb);
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
			/* printf("processing update ship...\n"); */
			rc = process_update_ship_packet();
			if (rc != 0)
				goto protocol_error;
			break;
		case OPCODE_ID_CLIENT_SHIP:
			rc = process_client_id_packet();
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
		case OPCODE_UPDATE_EXPLOSION:
			rc = process_update_explosion_packet();
			if (rc)
				goto protocol_error;
			break;
		case OPCODE_UPDATE_LASER:
			break;
		case OPCODE_UPDATE_TORPEDO:
			/* printf("processing update ship...\n"); */
			rc = process_update_torpedo_packet();
			if (rc != 0)
				goto protocol_error;
			break;
		case OPCODE_DELETE_OBJECT:
			rc = process_delete_object_packet();
			if (rc != 0)
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
		default:
			goto protocol_error;
		}
	}

protocol_error:
	printf("Protocol error in gameserver reader\n");
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
		if (have_packets_for_server) {
			have_packets_for_server = 0;
			pthread_mutex_unlock(&to_server_queue_event_mutex);
			break;
		}
	}
}

static void write_queued_packets_to_server(void)
{
	struct packed_buffer *buffer;
	int rc;

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
	return;

badserver:
	/* Hmm, we are pretty hosed here... we have a combined packed buffer
	 * to write, but server is disconnected... no place to put our buffer.
	 * What happens next is probably "nothing good."
	 */
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

	rc = snis_writesocket(gameserver_sock, SNIS_PROTOCOL_VERSION, strlen(SNIS_PROTOCOL_VERSION));
	if (rc < 0)
		goto error;;

	displaymode = DISPLAYMODE_CONNECTED;
	done_with_lobby = 1;
	displaymode = DISPLAYMODE_MAINSCREEN;

	/* Should probably submit this through the packed buffer queue...
	 * but, this works.
	 */
	memset(&app, 0, sizeof(app));
	app.opcode = htons(OPCODE_UPDATE_PLAYER);
	app.role = role;
	strncpy((char *) app.shipname, shipname, 19);
	strncpy((char *) app.password, password, 19);

	printf("Notifying server, opcode update player\n");
	snis_writesocket(gameserver_sock, &app, sizeof(app));
	printf("Wrote update player opcode\n");

        pthread_attr_init(&gameserver_reader_attr);
        pthread_attr_init(&gameserver_writer_attr);
        pthread_attr_setdetachstate(&gameserver_reader_attr, PTHREAD_CREATE_JOINABLE);
        pthread_attr_setdetachstate(&gameserver_writer_attr, PTHREAD_CREATE_JOINABLE);
	printf("starting gameserver reader thread\n");
	rc = pthread_create(&read_from_gameserver_thread, &gameserver_reader_attr, gameserver_reader, NULL);
	printf("started gameserver reader thread\n");
	printf("starting gameserver writer thread\n");
	rc = pthread_create(&write_to_gameserver_thread, &gameserver_writer_attr, gameserver_writer, NULL);
	printf("started gameserver writer thread\n");

error:
	/* FIXME, this isn't right... */
	freeaddrinfo(gameserverinfo);
	return NULL;
}

void connect_to_gameserver(int selected_server)
{
        pthread_attr_init(&gameserver_connect_attr);
        pthread_attr_setdetachstate(&gameserver_connect_attr, PTHREAD_CREATE_JOINABLE);
	(void) pthread_create(&gameserver_connect_thread, &gameserver_connect_attr, connect_to_gameserver_thread, NULL);
	return;
}

static void show_connecting_screen(GtkWidget *w)
{
	static int connected_to_gameserver = 0;
	gdk_gc_set_foreground(gc, &huex[WHITE]);
	abs_xy_draw_string(w, "CONNECTING TO SERVER...", SMALL_FONT, 100, 300 + LINEHEIGHT);
	if (!connected_to_gameserver) {
		connected_to_gameserver = 1;
		connect_to_gameserver(lobby_selected_server);
	}
}

static void show_connected_screen(GtkWidget *w)
{
	gdk_gc_set_foreground(gc, &huex[WHITE]);
	abs_xy_draw_string(w, "CONNECTED TO SERVER", SMALL_FONT, 100, 300 + LINEHEIGHT);
	abs_xy_draw_string(w, "DOWNLOADING GAME DATA", SMALL_FONT, 100, 300 + LINEHEIGHT * 3);
}

static void show_common_screen(GtkWidget *w, char *title)
{
	gdk_gc_set_foreground(gc, &huex[GREEN]);
	abs_xy_draw_string(w, title, SMALL_FONT, 25, 10 + LINEHEIGHT);
	gdk_gc_set_foreground(gc, &huex[WHITE]);
	snis_draw_line(w->window, gc, 0, 0, SCREEN_WIDTH, 0);
	snis_draw_line(w->window, gc, SCREEN_WIDTH, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
	snis_draw_line(w->window, gc, SCREEN_WIDTH, SCREEN_HEIGHT, 0, SCREEN_HEIGHT);
	snis_draw_line(w->window, gc, 0, 0, 0, SCREEN_HEIGHT);
}

static void show_mainscreen(GtkWidget *w)
{
	show_common_screen(w, "Main Screen");	
}

static void snis_draw_circle(GdkDrawable *drawable, GdkGC *gc, gint x, gint y, gint r)
{
	snis_draw_arc(drawable, gc, 0, x - r, y - r, r * 2, r * 2, 0, 360*64);
}

static void snis_draw_torpedo(GdkDrawable *drawable, GdkGC *gc, gint x, gint y, gint r)
{
	int i, dx, dy;

	gdk_gc_set_foreground(gc, &huex[WHITE]);
	for (i = 0; i < 10; i++) {
		dx = x + snis_randn(r * 2) - r; 
		dy = y + snis_randn(r * 2) - r; 
		snis_draw_line(drawable, gc, x, y, dx, dy);
	}
}

static void snis_draw_arrow(GdkDrawable *drawable, GdkGC *gc, gint x, gint y, gint r,
		double heading, double scale)
{
	int nx, ny, tx1, ty1, tx2, ty2;

	/* Draw ship... */
#define SHIP_SCALE_DOWN 15.0
	nx = sin(heading) * scale * r / SHIP_SCALE_DOWN;
	ny = -cos(heading) * scale * r / SHIP_SCALE_DOWN;
	snis_draw_line(drawable, gc, x, y, x + nx, y + ny);
	tx1 = sin(heading + PI / 2.0) * scale * r / (SHIP_SCALE_DOWN * 2.0) - nx / 2.0;
	ty1 = -cos(heading + PI / 2.0) * scale * r / (SHIP_SCALE_DOWN * 2.0) - ny / 2.0;
	snis_draw_line(drawable, gc, x, y, x + tx1, y + ty1);
	tx2 = sin(heading - PI / 2.0) * scale * r / (SHIP_SCALE_DOWN * 2.0) - nx / 2.0;
	ty2 = -cos(heading - PI / 2.0) * scale * r / (SHIP_SCALE_DOWN * 2.0) - ny / 2.0;
	snis_draw_line(drawable, gc, x, y, x + tx2, y + ty2);
	snis_draw_line(drawable, gc, x + nx, y + ny, x + tx1, y + ty1);
	snis_draw_line(drawable, gc, x + tx1, y + ty1, x + tx2, y + ty2);
	snis_draw_line(drawable, gc, x + tx2, y + ty2, x + nx, y + ny);
}

static void snis_draw_reticule(GdkDrawable *drawable, GdkGC *gc, gint x, gint y, gint r,
		double heading)
{
	int i;
	// int nx, ny, 
	int tx1, ty1, tx2, ty2;
	static double beam = 0.0;

	beam += 2.0 * PI / 180.0;

	for (i = r; i > r / 4; i -= r / 5)
		snis_draw_circle(drawable, gc, x, y, i);

	for (i = 0; i < 36; i++) { /* 10 degree increments */
		int x1 = (int) (cos((10.0 * i) * 3.1415927 / 180.0) * r);
		int y1 = (int) (sin((10.0 * i) * 3.1415927 / 180.0) * r);
		int x2 = x1 * 0.25;
		int y2 = y1 * 0.25;
		x1 += x;
		x2 += x;
		y1 += y;
		y2 += y;
		snis_draw_line(drawable, gc, x1, y1, x2, y2);
	}

	/* draw the ship */
	snis_draw_arrow(drawable, gc, x, y, r, heading, 1.0);
	
	tx1 = x + sin(heading) * r * 0.85;
	ty1 = y - cos(heading) * r * 0.85;
	tx2 = x + sin(heading) * r;
	ty2 = y - cos(heading) * r;
	gdk_gc_set_foreground(gc, &huex[RED]);
	snis_draw_line(drawable, gc, tx1, ty1, tx2, ty2);
	tx1 = x + sin(beam) * r * 0.15;
	ty1 = y - cos(beam) * r * 0.15;
	tx2 = x + sin(beam) * r;
	ty2 = y - cos(beam) * r;
	snis_draw_line(drawable, gc, tx1, ty1, tx2, ty2);
}

static void draw_all_the_guys(GtkWidget *w, struct snis_entity *o)
{
	int i, cx, cy, r, rx, ry, rw, rh;

	rx = 20;
	ry = 70;
	rw = 500;
	rh = 500;
	cx = rx + (rw / 2);
	cy = ry + (rh / 2);
	r = rh / 2;
	gdk_gc_set_foreground(gc, &huex[DARKRED]);
	/* Draw all the stuff */
#define NAVSCREEN_RADIUS (XUNIVERSE_DIMENSION / 20.0)
#define NR2 (NAVSCREEN_RADIUS * NAVSCREEN_RADIUS)
	pthread_mutex_lock(&universe_mutex);

	for (i = 0; i <= snis_object_pool_highest_object(pool); i++) {
		int x, y;
		double tx, ty;
		double dist2;

		if (!go[i].alive)
			continue;

		dist2 = ((go[i].x - o->x) * (go[i].x - o->x)) +
			((go[i].y - o->y) * (go[i].y - o->y));
		if (dist2 > NR2)
			continue; /* not close enough */
	

		tx = (go[i].x - o->x) * (double) r / NAVSCREEN_RADIUS;
		ty = (go[i].y - o->y) * (double) r / NAVSCREEN_RADIUS;
		x = (int) (tx + (double) cx);
		y = (int) (ty + (double) cy);

		if (go[i].id == my_ship_id)
			continue; /* skip drawing yourself. */
		else {
			switch (go[i].type) {
			case OBJTYPE_PLANET:
				gdk_gc_set_foreground(gc, &huex[BLUE]);
				snis_draw_circle(w->window, gc, x, y, r / 10);
				break;
			case OBJTYPE_STARBASE:
				gdk_gc_set_foreground(gc, &huex[MAGENTA]);
				snis_draw_circle(w->window, gc, x, y, r / 20);
				break;
			case OBJTYPE_TORPEDO:
				snis_draw_torpedo(w->window, gc, x, y, r / 20);
				break;
			default:
				gdk_gc_set_foreground(gc, &huex[WHITE]);
				snis_draw_arrow(w->window, gc, x, y, r, go[i].heading, 0.5);
			}
		}
	}
	pthread_mutex_unlock(&universe_mutex);
}

static void draw_all_the_sparks(GtkWidget *w, struct snis_entity *o)
{
	int i, cx, cy, r, rx, ry, rw, rh;

	rx = 20;
	ry = 70;
	rw = 500;
	rh = 500;
	cx = rx + (rw / 2);
	cy = ry + (rh / 2);
	r = rh / 2;
	gdk_gc_set_foreground(gc, &huex[DARKRED]);
	/* Draw all the stuff */
#define NAVSCREEN_RADIUS (XUNIVERSE_DIMENSION / 20.0)
#define NR2 (NAVSCREEN_RADIUS * NAVSCREEN_RADIUS)
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

		tx = (spark[i].x - o->x) * (double) r / NAVSCREEN_RADIUS;
		ty = (spark[i].y - o->y) * (double) r / NAVSCREEN_RADIUS;
		x = (int) (tx + (double) cx);
		y = (int) (ty + (double) cy);

		gdk_gc_set_foreground(gc, &huex[WHITE]);
		snis_draw_line(w->window, gc, x - 1, y - 1, x + 1, y + 1);
		snis_draw_line(w->window, gc, x - 1, y + 1, x + 1, y - 1);
	}
	pthread_mutex_unlock(&universe_mutex);
}


static void show_navigation(GtkWidget *w)
{
	char buf[100];
	struct snis_entity *o;
	int rx, ry, rw, rh, cx, cy;
	int r;

	show_common_screen(w, "Navigation");
	gdk_gc_set_foreground(gc, &huex[GREEN]);

	if (my_ship_id == UNKNOWN_ID)
		return;
	if (my_ship_oid == UNKNOWN_ID)
		my_ship_oid = (uint32_t) lookup_object_by_id(my_ship_id);
	if (my_ship_oid == UNKNOWN_ID)
		return;
	o = &go[my_ship_oid];
	sprintf(buf, "Location: (%5.2lf, %5.2lf)  Heading: %3.1lf", o->x, o->y,
			360.0 * o->heading / (2.0 * 3.1415927));
	abs_xy_draw_string(w, buf, TINY_FONT, 250, 10 + LINEHEIGHT);
	sprintf(buf, "vx: %5.2lf", o->vx);
	abs_xy_draw_string(w, buf, TINY_FONT, 600, LINEHEIGHT * 3);
	sprintf(buf, "vy: %5.2lf", o->vy);
	abs_xy_draw_string(w, buf, TINY_FONT, 600, LINEHEIGHT * 4);

	rx = 20;
	ry = 70;
	rw = 500;
	rh = 500;
	cx = rx + (rw / 2);
	cy = ry + (rh / 2);
	r = rh / 2;
	gdk_gc_set_foreground(gc, &huex[DARKRED]);
	snis_draw_reticule(w->window, gc, cx, cy, r, o->heading);

	draw_all_the_guys(w, o);
	draw_all_the_sparks(w, o);
}

static void show_weapons(GtkWidget *w)
{
	// char buf[100];
	struct snis_entity *o;
	int rx, ry, rw, rh, cx, cy;
	int r;

	show_common_screen(w, "Weapons");
	gdk_gc_set_foreground(gc, &huex[GREEN]);

	if (my_ship_id == UNKNOWN_ID)
		return;
	if (my_ship_oid == UNKNOWN_ID)
		my_ship_oid = (uint32_t) lookup_object_by_id(my_ship_id);
	if (my_ship_oid == UNKNOWN_ID)
		return;
	o = &go[my_ship_oid];
/*
	sprintf(buf, "Location: (%5.2lf, %5.2lf)  Heading: %3.1lf", o->x, o->y,
			360.0 * o->heading / (2.0 * 3.1415927));
	abs_xy_draw_string(w, buf, TINY_FONT, 250, 10 + LINEHEIGHT);
	sprintf(buf, "vx: %5.2lf", o->vx);
	abs_xy_draw_string(w, buf, TINY_FONT, 600, LINEHEIGHT * 3);
	sprintf(buf, "vy: %5.2lf", o->vy);
	abs_xy_draw_string(w, buf, TINY_FONT, 600, LINEHEIGHT * 4);
*/

	rx = 20;
	ry = 70;
	rw = 500;
	rh = 500;
	cx = rx + (rw / 2);
	cy = ry + (rh / 2);
	r = rh / 2;
	gdk_gc_set_foreground(gc, &huex[BLUE]);
	snis_draw_reticule(w->window, gc, cx, cy, r, o->tsd.ship.gun_heading);
	draw_all_the_guys(w, o);
	draw_all_the_sparks(w, o);
}

static void show_engineering(GtkWidget *w)
{
	show_common_screen(w, "Engineering");
}

static void show_science(GtkWidget *w)
{
	show_common_screen(w, "Science");
}

static void show_comms(GtkWidget *w)
{
	show_common_screen(w, "Comms");
}

static void debug_draw_object(GtkWidget *w, struct snis_entity *o)
{
	int x, y, x1, y1, x2, y2;

	if (!o->alive)
		return;

	x = (int) ((double) SCREEN_WIDTH * o->x / XUNIVERSE_DIMENSION);
	y = (int) ((double) SCREEN_HEIGHT * o->y / YUNIVERSE_DIMENSION);
	x1 = x - 1;
	y2 = y + 1;
	y1 = y - 1;
	x2 = x + 1;

	switch (o->type) {
	case OBJTYPE_SHIP1:
		if (o->id == my_ship_id)
			gdk_gc_set_foreground(gc, &huex[GREEN]);
		else
			gdk_gc_set_foreground(gc, &huex[WHITE]);
		break;
	case OBJTYPE_PLANET:
		gdk_gc_set_foreground(gc, &huex[BLUE]);
		break;
	case OBJTYPE_STARBASE:
		gdk_gc_set_foreground(gc, &huex[MAGENTA]);
		break;
	default:
		gdk_gc_set_foreground(gc, &huex[WHITE]);
	}
	snis_draw_line(w->window, gc, x1, y1, x2, y2);
	snis_draw_line(w->window, gc, x1, y2, x2, y1);
}

static void show_debug(GtkWidget *w)
{
	int i;

	show_common_screen(w, "Debug");

	pthread_mutex_lock(&universe_mutex);

	for (i = 0; i <= snis_object_pool_highest_object(pool); i++)
		debug_draw_object(w, &go[i]);
	for (i = 0; i <= snis_object_pool_highest_object(sparkpool); i++)
		debug_draw_object(w, &spark[i]);
#if 0
	for (i = 0; i <= snis_object_pool_highest_object(pool); i++) {
		int x, y, x1, y1, x2, y2;

		if (!go[i].alive)
			continue;

		x = (int) ((double) SCREEN_WIDTH * go[i].x / XUNIVERSE_DIMENSION);
		y = (int) ((double) SCREEN_HEIGHT * go[i].y / YUNIVERSE_DIMENSION);
		x1 = x - 1;
		y2 = y + 1;
		y1 = y - 1;
		x2 = x + 1;

		if (go[i].id == my_ship_id)
			gdk_gc_set_foreground(gc, &huex[GREEN]);
		else {
			switch (go[i].type) {
			case OBJTYPE_PLANET:
				gdk_gc_set_foreground(gc, &huex[BLUE]);
				break;
			case OBJTYPE_STARBASE:
				gdk_gc_set_foreground(gc, &huex[MAGENTA]);
				break;
			default:
				gdk_gc_set_foreground(gc, &huex[WHITE]);
			}
		}
		snis_draw_line(w->window, gc, x1, y1, x2, y2);
		snis_draw_line(w->window, gc, x1, y2, x2, y1);
	}
#endif
	pthread_mutex_unlock(&universe_mutex);
}

static int main_da_expose(GtkWidget *w, GdkEvent *event, gpointer p)
{
        gdk_gc_set_foreground(gc, &huex[WHITE]);
	
	role = ROLE_MAIN;

#if 0	
	for (i = 0; i <= highest_object_number;i++) {
		if (!game_state.go[i].alive)
			continue;
		if (onscreen(&game_state.go[i]))
			game_state.go[i].draw(&game_state.go[i], main_da); 
	}
#endif
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
	case DISPLAYMODE_HELM:
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
	case DISPLAYMODE_DEBUG:
		show_debug(w);
		break;
	default:
		show_fonttest(w);
		break;
	}
	return 0;
}

void really_quit(void);

gint advance_game(gpointer data)
{
	timer++;

#if 0
	for (i = 0; i <= highest_object_number; i++) {
		if (!game_state.go[i].alive)
			continue;
		game_state.go[i].move(&game_state.go[i]);
	}
	move_viewport();
#endif	
	gdk_threads_enter();
	gtk_widget_queue_draw(main_da);
	move_sparks();
	nframes++;
	gdk_threads_leave();
	if (in_the_process_of_quitting)
		really_quit();
	return TRUE;
}

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
	xscale_screen = (float) real_screen_width / (float) SCREEN_WIDTH;
	yscale_screen = (float) real_screen_height / (float) SCREEN_HEIGHT;
	if (real_screen_width == 800 && real_screen_height == 600) {
		current_draw_line = gdk_draw_line;
		current_draw_rectangle = gdk_draw_rectangle;
		current_bright_line = unscaled_bright_line;
		current_draw_arc = gdk_draw_arc;
	} else {
		current_draw_line = scaled_line;
		current_draw_rectangle = scaled_rectangle;
		current_bright_line = scaled_bright_line;
		current_draw_arc = scaled_arc;
		if (thicklines)
			current_draw_line = thick_scaled_line;
	}
	gdk_gc_set_clip_origin(gc, 0, 0);
	cliprect.x = 0;	
	cliprect.y = 0;	
	cliprect.width = real_screen_width;	
	cliprect.height = real_screen_height;	
	gdk_gc_set_clip_rectangle(gc, &cliprect);
	return TRUE;
}

static int main_da_button_press(GtkWidget *w, GdkEventButton *event,
	__attribute__((unused)) void *unused)
		
{
	switch (displaymode) {
	case DISPLAYMODE_LOBBYSCREEN:
		lobbylast1clickx = (int) ((0.0 + event->x) / (0.0 + real_screen_width) * SCREEN_WIDTH);
		lobbylast1clicky = (int) ((0.0 + event->y) / (0.0 + real_screen_height) * SCREEN_HEIGHT);
		
		return TRUE;
		break;
	default:
		return FALSE;
	}
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
	gettimeofday(&end_time, NULL);
	printf("%d frames / %d seconds, %g frames/sec\n",
		nframes, (int) (end_time.tv_sec - start_time.tv_sec),
		(0.0 + nframes) / (0.0 + end_time.tv_sec - start_time.tv_sec));
	exit(1); /* probably bad form... oh well. */
}

static void usage(void)
{
	fprintf(stderr, "usage: snis_client lobbyhost starship password\n");
	fprintf(stderr, "       Example: ./snis_client localhost Enterprise tribbles\n");
	exit(1);
}

int main(int argc, char *argv[])
{
	GtkWidget *vbox;
	int i;

	if (argc < 3)
		usage();

	lobbyhost = argv[1];
	shipname = argv[2];
	password = argv[3];

	snis_object_pool_setup(&pool, MAXGAMEOBJS);
	snis_object_pool_setup(&sparkpool, MAXSPARKS);

	ignore_sigpipe();

	connect_to_lobby();
	real_screen_width = SCREEN_WIDTH;
	real_screen_height = SCREEN_HEIGHT;

	gtk_set_locale();
	gtk_init (&argc, &argv);

	init_keymap();
#if 0
	init_vects();
	init_player();
	init_game_state(the_player);
#endif

	/* Make the line segment lists from the stroke_t structures. */
	snis_make_font(&gamefont[BIG_FONT], font_scale[BIG_FONT], font_scale[BIG_FONT]);
	snis_make_font(&gamefont[SMALL_FONT], font_scale[SMALL_FONT], font_scale[SMALL_FONT]);
	snis_make_font(&gamefont[TINY_FONT], font_scale[TINY_FONT], font_scale[TINY_FONT]);
	snis_make_font(&gamefont[NANO_FONT], font_scale[NANO_FONT], font_scale[NANO_FONT]);

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
	gtk_widget_add_events(main_da, GDK_BUTTON_PRESS_MASK);
	g_signal_connect(G_OBJECT (main_da), "button_press_event",
                      G_CALLBACK (main_da_button_press), NULL);

	gdk_color_parse("white", &huex[WHITE]);
	gdk_color_parse("blue", &huex[BLUE]);
	gdk_color_parse("black", &huex[BLACK]);
	gdk_color_parse("green", &huex[GREEN]);
	gdk_color_parse("darkgreen", &huex[DARKGREEN]);
	gdk_color_parse("yellow", &huex[YELLOW]);
	gdk_color_parse("red", &huex[RED]);
	gdk_color_parse("orange", &huex[ORANGE]);
	gdk_color_parse("cyan", &huex[CYAN]);
	gdk_color_parse("MAGENTA", &huex[MAGENTA]);
	gdk_color_parse("darkred", &huex[DARKRED]);

	gtk_container_add (GTK_CONTAINER (window), vbox);
	gtk_box_pack_start(GTK_BOX (vbox), main_da, TRUE /* expand */, TRUE /* fill */, 0);

	gtk_widget_modify_bg(main_da, GTK_STATE_NORMAL, &huex[BLACK]);

        gtk_window_set_default_size(GTK_WINDOW(window), real_screen_width, real_screen_height);

        gtk_widget_show (vbox);
        gtk_widget_show (main_da);
        gtk_widget_show (window);

	for (i=0;i<NCOLORS+NSPARKCOLORS + NRAINBOWCOLORS;i++)
		gdk_colormap_alloc_color(gtk_widget_get_colormap(main_da), &huex[i], FALSE, FALSE);
        gc = gdk_gc_new(GTK_WIDGET(main_da)->window);
        gdk_gc_set_foreground(gc, &huex[BLUE]);
        gdk_gc_set_foreground(gc, &huex[WHITE]);

	timer_tag = g_timeout_add(1000 / frame_rate_hz, advance_game, NULL);

	/* Apparently (some versions of?) portaudio calls g_thread_init(). */
	/* It may only be called once, and subsequent calls abort, so */
	/* only call it if the thread system is not already initialized. */
	if (!g_thread_supported ())
		g_thread_init(NULL);
	gdk_threads_init();

	gettimeofday(&start_time, NULL);

	gtk_main ();
	return 0;
}
