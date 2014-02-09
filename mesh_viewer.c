
#include <SDL/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "mtwist.h"
#include "vertex.h"
#include "snis_graph.h"
#include "graph_dev.h"
#include "quat.h"
#include "material.h"
#include "entity.h"
#include "mesh.h"
#include "stl_parser.h"
#include "mathutils.h"
#include "snis_typeface.h"

#define FOV (30.0 * M_PI / 180.0)
#define FPS 60

#define SCREEN_WIDTH 800	/* window width, in pixels */
#define SCREEN_HEIGHT 600	/* window height, in pixels */
#define ASPECT_RATIO (SCREEN_WIDTH/(float)SCREEN_HEIGHT)

static int real_screen_width;
static int real_screen_height;

static int display_frame_stats = 1;

/* Color depth in bits of our window. */
static int bpp;

static struct mesh *snis_read_model(char *filename)
{
	int l = strlen(filename);

	if (strcasecmp(&filename[l - 3], "obj") == 0)
		return read_obj_file(filename);
	else if (strcasecmp(&filename[l - 3], "stl") == 0)
		return read_stl_file(filename);
	else {
		printf("bad filename='%s', filename[l - 3] = '%s'\n",
			filename, &filename[l - 4]);
		return NULL;
	}
}

static void quit(int code)
{
	SDL_Quit();

	exit(code);
}

static SDL_Surface *screen;

static void handle_key_down(SDL_keysym *keysym)
{
	switch (keysym->sym) {
	case SDLK_ESCAPE:
		quit(0);
		break;
	case SDLK_F11:
		SDL_WM_ToggleFullScreen(screen);
		break;
	case SDLK_PAUSE:
		display_frame_stats = (display_frame_stats + 1) % 3;
		break;
	default:
		break;
	}

}

static int isDragging;
static int isDraggingLight;
static union quat last_orientation = IDENTITY_QUAT_INITIALIZER;
static union quat lobby_orientation = IDENTITY_QUAT_INITIALIZER;
static union quat light_orientation = IDENTITY_QUAT_INITIALIZER;
static int lobby_zoom = 255;

struct arc_ball {
	union vec3 s;
	union vec3 e;
} arc_ball;

/* derived from http://nehe.gamedev.net/tutorial/arcball_rotation/19003/
==========================================================================
		    OpenGL Lesson 48:  ArcBall Rotation
==========================================================================

  Authors Name: Terence J. Grant

  Disclaimer:

  This program may crash your system or run poorly depending on your
  hardware.  The program and code contained in this archive was scanned
  for virii and has passed all test before it was put online.  If you
  use this code in project of your own, send a shout out to the author!

==========================================================================
			NeHe Productions 1997-2004
==========================================================================
*/
static void map_to_sphere(int width, int height, const union vec2 *new_pt, union vec3 *new_vec)
{
	union vec2 temp_pt;
	float length;

	float adjust_width = 1.0f / ((width - 1.0f) * 0.5f);
	float adjust_height = 1.0f / ((height - 1.0f) * 0.5f);

	/* Copy paramter into temp point */
	temp_pt = *new_pt;

	/* Adjust point coords and scale down to range of [-1 ... 1] */
	temp_pt.v.x = (temp_pt.v.x * adjust_width) - 1.0f;
	temp_pt.v.y = 1.0f - (temp_pt.v.y * adjust_height);

	/* Compute the square of the length of the vector to the point from the center */
	length = (temp_pt.v.x * temp_pt.v.x) + (temp_pt.v.y * temp_pt.v.y);

	/* If the point is mapped outside of the sphere... (length > radius squared) */
	if (length > 1.0f) {
		float norm;

		/* Compute a normalizing factor (radius / sqrt(length)) */
		norm = 1.0f / sqrt(length);

		/* Return the "normalized" vector, a point on the sphere */
		new_vec->v.x = temp_pt.v.x * norm;
		new_vec->v.y = temp_pt.v.y * norm;
		new_vec->v.z = 0.0f;
	} else /* Else it's on the inside */ {
		/* Return a vector to a point mapped inside the sphere sqrt(radius squared - length) */
		new_vec->v.x = temp_pt.v.x;
		new_vec->v.y = temp_pt.v.y;
		new_vec->v.z = sqrt(1.0f - length);
	}
}

static int main_da_motion_notify(int x, int y)
{
	if (isDragging) {
		/* update drag */
		union vec2 point = { { x, y } };
		map_to_sphere(real_screen_width, real_screen_height, &point, &arc_ball.e);

		union quat this_quat;
		quat_from_u2v(&this_quat, &arc_ball.s, &arc_ball.e, 0);

		if (isDraggingLight)
			quat_mul(&light_orientation, &this_quat, &last_orientation);
		else
			quat_mul(&lobby_orientation, &this_quat, &last_orientation);
	}
	return 0;
}

static int main_da_button_press(int button, int x, int y)
{
	if (button == 3) {
		/* start drag */
		isDragging = 1;

		SDLMod mod = SDL_GetModState();
		isDraggingLight = mod & (KMOD_LCTRL | KMOD_RCTRL);

		if (isDraggingLight)
			last_orientation = light_orientation;
		else
			last_orientation = lobby_orientation;

		union vec2 point = { { x, y } };
		map_to_sphere(real_screen_width, real_screen_height, &point, &arc_ball.s);

	}
	return 0;
}

static int main_da_scroll(int direction)
{
	if (direction)
		lobby_zoom += 10;
	else
		lobby_zoom -= 10;

	if (lobby_zoom < 0)
		lobby_zoom = 0;
	if (lobby_zoom > 255)
		lobby_zoom = 255;
	return 1;
}

static int main_da_button_release(int button, int x, int y)
{
	if (button == 4) {
		main_da_scroll(1);
		return 1;
	} else if (button == 5) {
		main_da_scroll(0);
		return 1;
	}

	if (button == 1 && display_frame_stats) {
		if (graph_dev_graph_dev_debug_menu_click(x, y))
			return 1;
	}

	if (button == 3) {
		if (isDragging) {
			/* end drag */
			isDragging = 0;
			isDraggingLight = 0;
		}
	}

	return 1;
}

static int sdl_button_to_int(int sdl_button)
{
	switch (sdl_button) {
	case SDL_BUTTON_LEFT:
		return 1;
	case SDL_BUTTON_MIDDLE:
		return 2;
	case SDL_BUTTON_RIGHT:
		return 3;
	case SDL_BUTTON_WHEELUP:
		return 4;
	case SDL_BUTTON_WHEELDOWN:
		return 5;
	}
	return 0;
}

static void process_events()
{
	/* Our SDL event placeholder. */
	SDL_Event event;

	/* Grab all the events off the queue. */
	while (SDL_PollEvent(&event)) {

		switch (event.type) {
		case SDL_KEYDOWN:
			/* Handle key presses. */
			handle_key_down(&event.key.keysym);
			break;
		case SDL_QUIT:
			/* Handle quit requests (like Ctrl-c). */
			quit(0);
			break;
		case SDL_VIDEORESIZE:
			real_screen_width = event.resize.w;
			real_screen_height = event.resize.h;

			screen = SDL_SetVideoMode(real_screen_width, real_screen_height,
					bpp, SDL_OPENGL | SDL_RESIZABLE);
			sng_set_screen_size(real_screen_width, real_screen_height);
			break;
		case SDL_MOUSEBUTTONDOWN: {
				int button = sdl_button_to_int(event.button.button);
				if (button > 0)
					main_da_button_press(button, event.button.x, event.button.y);
			}
			break;
		case SDL_MOUSEBUTTONUP: {
				int button = sdl_button_to_int(event.button.button);
				if (button > 0)
					main_da_button_release(button, event.button.x, event.button.y);
			}
			break;
		case SDL_MOUSEMOTION:
			main_da_motion_notify(event.motion.x, event.motion.y);
			break;
		}
	}

}


static struct mesh *target_mesh;
static struct mesh *light_mesh;

#define FRAME_INDEX_MAX 10

static void draw_screen()
{
	static double last_frame_time;
	static int frame_index;
	static float frame_rates[FRAME_INDEX_MAX];
	static float frame_times[FRAME_INDEX_MAX];

	double start_time = time_now_double();

	glClearColor(0.0, 0.0, 0.0, 0.0);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	graph_dev_start_frame();

	static struct entity_context *cx;
	if (!cx)
		cx = entity_context_new(50, 50);

	float r = target_mesh->radius / tan(FOV / 2.0); /* 50% size for middle zoom */
	float r_cam = r * lobby_zoom / 255.0;
	
	camera_set_parameters(cx, 0.1f, 1000.0f, SCREEN_WIDTH, SCREEN_HEIGHT, FOV);
	camera_set_pos(cx, r_cam, 0, 0);
	camera_look_at(cx, 0, 0, 0);
	camera_assign_up_direction(cx, 0, 1, 0);

	union vec3 light_pos = { { 1.01 * r, 0, 0 } };
	quat_rot_vec_self(&light_pos, &light_orientation);
	set_lighting(cx, light_pos.v.x, light_pos.v.y, light_pos.v.z);

	calculate_camera_transform(cx);

	struct entity *e = add_entity(cx, target_mesh, 0, 0, 0, WHITE);
	update_entity_orientation(e, &lobby_orientation);

	if (isDraggingLight) {
		union vec3 light_dir = { { 0.75 * r_cam, 0, 0 } };
		quat_rot_vec_self(&light_dir, &light_orientation);
		sng_set_foreground(WHITE);
		render_line(cx, light_dir.v.x, light_dir.v.y, light_dir.v.z, 0, 0, 0);

		e = add_entity(cx, light_mesh, light_dir.v.x, light_dir.v.y, light_dir.v.z, WHITE);
	} else {
		e = add_entity(cx, light_mesh, light_pos.v.x, light_pos.v.y, light_pos.v.z, WHITE);
	}

	render_entities(cx);

	remove_all_entity(cx);

	if (display_frame_stats > 0) {
		float avg_frame_rate = 0;
		float avg_frame_time = 0;
		int i;
		for (i = 0; i < FRAME_INDEX_MAX; i++) {
			avg_frame_rate += frame_rates[i];
			avg_frame_time += frame_times[i];
		}
		avg_frame_rate /= (float)FRAME_INDEX_MAX;
		avg_frame_time /= (float)FRAME_INDEX_MAX;

		sng_set_foreground(WHITE);
		char stat_buffer[30];
		sprintf(stat_buffer, "fps %5.2f", 1.0/avg_frame_rate);
		sng_abs_xy_draw_string(stat_buffer, NANO_FONT, 2, 10);
		sprintf(stat_buffer, "t %0.2f ms", avg_frame_time * 1000.0);
		sng_abs_xy_draw_string(stat_buffer, NANO_FONT, 92, 10);
	}
	if (display_frame_stats > 1)
		graph_dev_display_debug_menu_show();

	graph_dev_end_frame();

	/*
	 * Swap the buffers. This this tells the driver to
	 * render the next frame from the contents of the
	 * back-buffer, and to set all rendering operations
	 * to occur on what was the front-buffer.
	 *
	 * Double buffering prevents nasty visual tearing
	 * from the application drawing on areas of the
	 * screen that are being updated at the same time.
	 */
	SDL_GL_SwapBuffers();

	if (display_frame_stats > 0) {
		double end_time = time_now_double();

		frame_rates[frame_index] = start_time - last_frame_time;
		frame_times[frame_index] = end_time - start_time;
		frame_index = (frame_index + 1) % FRAME_INDEX_MAX;
		last_frame_time = start_time;
	}
}

int main(int argc, char *argv[])
{
	if (argc < 2) {
		printf("%s <mesh_file>\n", argv[0]);
		exit(-1);
	}

	/* Information about the current video settings. */
	const SDL_VideoInfo *info = NULL;

	/* First, initialize SDL's video subsystem. */
	if (SDL_Init(SDL_INIT_VIDEO) < 0) {
		/* Failed, exit. */
		fprintf(stderr, "Video initialization failed: %s\n", SDL_GetError());
		quit(1);
	}

	/* Let's get some video information. */
	info = SDL_GetVideoInfo();

	if (!info) {
		/* This should probably never happen. */
		fprintf(stderr, "Video query failed: %s\n", SDL_GetError());
		quit(1);
	}

	/*
	 * Set our width/height to 640/480 (you would
	 * of course let the user decide this in a normal
	 * app). We get the bpp we will request from
	 * the display. On X11, VidMode can't change
	 * resolution, so this is probably being overly
	 * safe. Under Win32, ChangeDisplaySettings
	 * can change the bpp.
	 */
	bpp = info->vfmt->BitsPerPixel;

	/*
	 * Now, we want to setup our requested
	 * window attributes for our OpenGL window.
	 * We want *at least* 5 bits of red, green
	 * and blue. We also want at least a 16-bit
	 * depth buffer.
	 *
	 * The last thing we do is request a double
	 * buffered window. '1' turns on double
	 * buffering, '0' turns it off.
	 *
	 * Note that we do not use SDL_DOUBLEBUF in
	 * the flags to SDL_SetVideoMode. That does
	 * not affect the GL attribute state, only
	 * the standard 2D blitting setup.
	 */
	SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 5);
	SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 5);
	SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 5);
	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 16);
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

	/*
	 * Set the video mode
	 */
	screen = SDL_SetVideoMode(SCREEN_WIDTH, SCREEN_HEIGHT, bpp, SDL_OPENGL | SDL_RESIZABLE);
	if (screen == 0) {
		/*
		 * This could happen for a variety of reasons,
		 * including DISPLAY not being set, the specified
		 * resolution not being available, etc.
		 */
		fprintf(stderr, "Video mode set failed: %s\n", SDL_GetError());
		quit(1);
	}

	real_screen_width = SCREEN_WIDTH;
	real_screen_height = SCREEN_HEIGHT;

	sng_setup_colors(0);

	snis_typefaces_init();
	graph_dev_setup();

	sng_set_extent_size(SCREEN_WIDTH, SCREEN_HEIGHT);
	sng_set_screen_size(real_screen_width, real_screen_height);
	sng_set_clip_window(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);

	target_mesh = snis_read_model(argv[1]);
	if (!target_mesh) {
		printf("unable to read model file '%s'\n", argv[1]);
		exit(-1);
	}

	light_mesh = mesh_fabricate_billboard(0, 0, 10, 10); //target_mesh->radius / 10.0, target_mesh->radius / 10.0);

	struct material light_material;
	light_material.type = MATERIAL_BILLBOARD;
	light_material.billboard.billboard_type = MATERIAL_BILLBOARD_TYPE_SCREEN;
	light_material.billboard.texture_id = graph_dev_load_texture("share/snis/textures/sun.png");

	light_mesh->material = &light_material;

	const double maxTimeBehind = 0.5;
	double delta = 1.0/(double)FPS;

	double currentTime = time_now_double();
	double nextTime = currentTime + delta;
	while (1) {
		currentTime = time_now_double();

		if (currentTime - nextTime > maxTimeBehind)
			nextTime = currentTime;

		if (currentTime >= nextTime) {
			nextTime += delta;

			/* Process incoming events. */
			process_events();
			/* Draw the screen. */
			draw_screen();
		} else {
			double timeToSleep = nextTime-currentTime;
			if (timeToSleep > 0)
				sleep_double(timeToSleep);
		}
	}

	/* Never reached. */
	return 0;
}
