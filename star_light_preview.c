/*
	star_light_preview: a standalone realtime preview for the star-coloured
	lighting maths (star_light.c).  It draws a stack of touching horizontal
	strips across the window, x-axis = star colour temperature, so the derived
	light colour, shaded/ambient colour and the star's own blackbody colour can
	be eyeballed side by side across the whole temperature range while the tint
	and contrast knobs are adjusted live.  Use it to pick the default tint (k)
	and past-white contrast (q) before wiring the maths into the renderer.

	THIS FILE IS AI-GENERATED AND IS PLACED IN THE PUBLIC DOMAIN.

	This is a self-contained ancillary tool: it links the SNIS engine only for
	its window/graphics scaffolding and adds no logic to the core game.  Per
	CONTRIBUTING.md, AI-generated ancillary tooling is permitted when clearly
	marked as such and dedicated to the public domain.  The author(s) disclaim
	all copyright and neighboring rights to this file to the fullest extent
	permitted by law.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <locale.h>

#ifdef __APPLE__
#include <SDL2.h>
#else
#include <SDL.h>
#endif

#include "opengl_cap.h"
#include "snis_graph.h"
#include "graph_dev.h"
#include "snis_typeface.h"
#include "mathutils.h"
#include "png_utils.h"
#include "snis_xwindows_hacks.h"
#include "star_light.h"

#define FPS 60

static int SCREEN_WIDTH = 1600;
static int SCREEN_HEIGHT = 900;
static int real_screen_width;
static int real_screen_height;
static SDL_Window *screen;
static int helpmode = 0;
static int snapshot_number = 0;

/* Temperature sweep across the x-axis.  Interpolated on a log scale so the
 * red/orange low-temperature range (where the colour changes fastest) gets a
 * fair share of the width instead of being crushed into the left edge. */
#define TEMP_MIN 1900.0f
#define TEMP_MAX 40000.0f
#define NCOLS 256 /* colour bins across the width; each is a flat vertical stripe */

/* Live knobs (defaults mirror the intended shadow_lab starting values). */
static float light_tint = 0.45f;
static float dark_tint = 0.06f;
static float shadow_darkening = 0.0f;
static float ambient = 0.015f;

/* Game filmic tonemap preview.  The lit shaders in game end with filmic_tonemap()
 * (Jim Hejl / Uncharted-2 approximation, gamma baked in), so a raw-linear preview
 * of the light/shade colours would read differently from how a lit surface looks
 * in game.  Apply the same curve to the illumination strips so the tuning transfers.
 * The game (snis_client) drives the gain from a slider defaulting to 1.10; graph_dev's
 * own default is 1.18, which is what shadow_lab currently inherits -- default to the
 * game's 1.10 here and keep it adjustable so both can be checked. */
static int apply_tonemap = 1;
static float tonemap_gain = 1.10f;

/* Surface albedo the LIGHT/DARK strips are previewed on.  The strips show the
 * illumination hitting a surface of this reflectance, so albedo 1.0 is a white
 * surface (the worst case for cast visibility -- the tonemap flattens near white)
 * and a lower value previews the cast on a typical mid-grey hull, where the curve
 * is steeper and the colour reads more. */
static float surface_albedo = 1.0f;

/* Exactly mirrors FILMIC_TONEMAPPING in graph_dev_opengl.c with u_FilmicTonemapping
 * enabled (full replace).  Operates per channel in place. */
static void game_filmic_tonemap(float rgb[3])
{
	int i;

	for (i = 0; i < 3; i++) {
		float x = rgb[i] - 0.004f;

		if (x < 0.0f)
			x = 0.0f;
		rgb[i] = tonemap_gain * (x * (6.2f * x + 0.5f)) / (x * (6.2f * x + 1.7f) + 0.06f);
	}
}

static float cursor_x = -1.0f; /* extent-space x of the mouse, for the temperature readout */

/* The strip stack, top to bottom.  STRIP_STAR is flanked by a DARK (ambient)
 * strip directly above and a LIGHT strip directly below, and the light/dark
 * pair repeats, so every pairwise comparison (star/light, star/dark, light/dark)
 * is edge-to-edge somewhere.  The pure-white top cap and the original-ambient
 * bottom cap are neutral references that make the colour casts read acutely. */
enum strip_kind {
	STRIP_WHITE,
	STRIP_LIGHT_A,
	STRIP_DARK_A,
	STRIP_STAR,
	STRIP_LIGHT_B,
	STRIP_DARK_B,
	STRIP_ORIG_AMBIENT,
	NUM_STRIPS
};

static const char *strip_label[NUM_STRIPS] = {
	"WHITE",
	"LIGHT",
	"DARK (AMBIENT)",
	"STAR",
	"LIGHT",
	"DARK (AMBIENT)",
	"ORIGINAL AMBIENT",
};

static float temp_at_col(int col)
{
	float f = (NCOLS <= 1) ? 0.0f : (float) col / (float) (NCOLS - 1);
	return TEMP_MIN * powf(TEMP_MAX / TEMP_MIN, f);
}

static float temp_at_x(float x)
{
	float f = (SCREEN_WIDTH <= 1) ? 0.0f : x / (float) SCREEN_WIDTH;
	if (f < 0.0f)
		f = 0.0f;
	if (f > 1.0f)
		f = 1.0f;
	return TEMP_MIN * powf(TEMP_MAX / TEMP_MIN, f);
}

static void set_rgb(float r, float g, float b)
{
	struct graph_dev_color color;
	color.pixel = 0;
	color.red = (uint16_t) (clampf(r, 0.0f, 1.0f) * 65535.0f);
	color.green = (uint16_t) (clampf(g, 0.0f, 1.0f) * 65535.0f);
	color.blue = (uint16_t) (clampf(b, 0.0f, 1.0f) * 65535.0f);
	graph_dev_set_color(&color, -1);
}

/* Fill in the three preview colours (and the star's own colour) for one strip
 * kind at one temperature. */
static void strip_color(enum strip_kind kind, float kelvin, float out[3])
{
	float star[3], light[3], amb[3];

	star_light_blackbody_color(kelvin, &star[0], &star[1], &star[2]);
	star_light_colors(star, ambient, light_tint, dark_tint, shadow_darkening, light, amb);

	switch (kind) {
	case STRIP_WHITE:
		/* Pure white reference; left raw (a true display-white bar). */
		out[0] = out[1] = out[2] = 1.0f;
		break;
	case STRIP_LIGHT_A:
	case STRIP_LIGHT_B:
		/* Fully-lit surface of reflectance surface_albedo: the light colour
		 * times the albedo, tonemapped (matches albedo.rgb * light in game). */
		out[0] = surface_albedo * light[0];
		out[1] = surface_albedo * light[1];
		out[2] = surface_albedo * light[2];
		if (apply_tonemap)
			game_filmic_tonemap(out);
		break;
	case STRIP_DARK_A:
	case STRIP_DARK_B:
		/* Shaded surface: the ambient colour times the albedo, tonemapped. */
		out[0] = surface_albedo * amb[0];
		out[1] = surface_albedo * amb[1];
		out[2] = surface_albedo * amb[2];
		if (apply_tonemap)
			game_filmic_tonemap(out);
		break;
	case STRIP_STAR:
		/* The star's own colour; the sun billboard clamps per channel (no
		 * filmic), so show it raw. */
		out[0] = star[0]; out[1] = star[1]; out[2] = star[2];
		break;
	case STRIP_ORIG_AMBIENT:
		/* What a shaded surface looks like today (untinted ambient) at the
		 * same albedo, tonemapped the same way -- a fair reference for the
		 * DARK strips. */
		out[0] = out[1] = out[2] = surface_albedo * ambient;
		if (apply_tonemap)
			game_filmic_tonemap(out);
		break;
	default:
		out[0] = out[1] = out[2] = 0.0f;
		break;
	}
}

#define HEADER_H 118 /* extent-space band at the top reserved for the readout text */

static void draw_strips(void)
{
	int s, c;
	float strips_top = HEADER_H;
	float strips_h = (float) SCREEN_HEIGHT - strips_top;
	float strip_h = strips_h / (float) NUM_STRIPS;
	float col_w = (float) SCREEN_WIDTH / (float) NCOLS;

	for (s = 0; s < NUM_STRIPS; s++) {
		float y = strips_top + s * strip_h;
		for (c = 0; c < NCOLS; c++) {
			float x = c * col_w;
			float kelvin = temp_at_col(c);
			float rgb[3];

			strip_color((enum strip_kind) s, kelvin, rgb);
			set_rgb(rgb[0], rgb[1], rgb[2]);
			/* +1 so adjacent bins overlap and leave no seam. */
			sng_current_draw_rectangle(1, x, y, col_w + 1.0f, strip_h + 1.0f);
		}
		/* Strip label, dropped just inside the left edge of each strip. */
		sng_set_foreground(s == STRIP_WHITE ? BLACK : WHITE);
		sng_abs_xy_draw_string((char *) strip_label[s], NANO_FONT, 8, y + 16);
	}
}

/* A handful of temperature gridlines with labels, drawn over the strips. */
static void draw_temp_ticks(void)
{
	static const float ticks[] = { 2000, 3000, 4000, 5800, 6500, 10000, 20000, 40000 };
	int i;
	int n = (int) (sizeof(ticks) / sizeof(ticks[0]));
	float strips_top = HEADER_H;

	for (i = 0; i < n; i++) {
		float f = logf(ticks[i] / TEMP_MIN) / logf(TEMP_MAX / TEMP_MIN);
		float x = f * (float) SCREEN_WIDTH;
		char buf[32];

		if (f < 0.0f || f > 1.0f)
			continue;
		sng_set_foreground(GRAY50);
		sng_current_draw_line(x, strips_top, x, (float) SCREEN_HEIGHT);
		snprintf(buf, sizeof(buf), "%.0fK", ticks[i]);
		sng_set_foreground(WHITE);
		sng_abs_xy_draw_string(buf, NANO_FONT, x + 3, strips_top - 6);
	}
}

static void draw_header(void)
{
	char buf[160];
	float y = 20;
	float dy = 24;

	sng_set_foreground(WHITE);
	sng_abs_xy_draw_string("STAR-LIGHT PREVIEW - F1 FOR HELP", TINY_FONT, 10, y);
	y += dy;

	snprintf(buf, sizeof(buf),
		"LIGHT-TINT %.3f   DARK-TINT %.3f   DARKENING %.3f   AMBIENT %.4f   ALBEDO %.2f",
		light_tint, dark_tint, shadow_darkening, ambient, surface_albedo);
	sng_abs_xy_draw_string(buf, TINY_FONT, 10, y);
	y += dy;

	if (apply_tonemap)
		snprintf(buf, sizeof(buf), "GAME FILMIC TONEMAP ON  gain %.3f (in-game default 1.10)",
			tonemap_gain);
	else
		snprintf(buf, sizeof(buf), "TONEMAP OFF (RAW LINEAR - not what the game shows)");
	sng_set_foreground(apply_tonemap ? GRAY75 : AMBER);
	sng_abs_xy_draw_string(buf, TINY_FONT, 10, y);
	y += dy;

	if (cursor_x >= 0.0f) {
		float t = temp_at_x(cursor_x);
		float star[3], light[3], amb[3];

		star_light_blackbody_color(t, &star[0], &star[1], &star[2]);
		star_light_colors(star, ambient, light_tint, dark_tint, shadow_darkening, light, amb);
		snprintf(buf, sizeof(buf),
			"CURSOR %.0fK  star %.2f %.2f %.2f  light %.2f %.2f %.2f  dark %.3f %.3f %.3f (linear uniforms)",
			t, star[0], star[1], star[2],
			light[0], light[1], light[2], amb[0], amb[1], amb[2]);
		sng_set_foreground(WHITE);
		sng_abs_xy_draw_string(buf, TINY_FONT, 10, y);
	}
}

static const char * const help_text[] = {
	"STAR-LIGHT PREVIEW",
	"",
	"Strips (top to bottom): WHITE, LIGHT, DARK, STAR, LIGHT, DARK, ORIGINAL AMBIENT.",
	"x-axis is star colour temperature (log scale, ~1900K left .. 40000K right).",
	"LIGHT  = sunlight tinted toward the star colour.",
	"DARK   = shaded/ambient colour tinted toward the star's complement, deepened for blue stars.",
	"STAR   = the star's own blackbody colour.",
	"",
	"LEFT / RIGHT         decrease / increase LIGHT tint",
	"SHIFT+LEFT / RIGHT   decrease / increase DARK tint",
	"DOWN / UP            decrease / increase SHADOW DARKENING",
	"[ / ]                decrease / increase AMBIENT A",
	", / .                decrease / increase surface ALBEDO (1.0 = white, lower = grey hull)",
	"T                    toggle the game filmic tonemap (LIGHT/DARK strips)",
	"- / =                decrease / increase the tonemap gain",
	"0                    reset tints, q, A, albedo to defaults",
	"P              save a PNG snapshot",
	"F1             toggle this help",
	"ESC / Q        quit",
	NULL,
};

static void draw_help(void)
{
	int i;
	float y = 70;
	float dy = 26;

	sng_set_foreground(BLACK);
	sng_current_draw_rectangle(1, 50, 50, SCREEN_WIDTH - 100, SCREEN_HEIGHT - 100);
	sng_set_foreground(GREEN);
	sng_current_draw_rectangle(0, 50, 50, SCREEN_WIDTH - 100, SCREEN_HEIGHT - 100);
	for (i = 0; help_text[i] != NULL; i++) {
		sng_abs_xy_draw_string((char *) help_text[i], TINY_FONT, 70, y);
		y += dy;
	}
}

static void draw_screen(void)
{
	glClearColor(0.0, 0.0, 0.0, 0.0);
	graph_dev_start_frame();

	if (helpmode) {
		draw_help();
	} else {
		draw_strips();
		draw_temp_ticks();
		draw_header();
	}

	graph_dev_end_frame();
	SDL_GL_SwapWindow(screen);
}

static void quit(int code)
{
	SDL_Quit();
	exit(code);
}

static void enable_sdl_fullscreen_sanity(void)
{
	setenv("SDL_VIDEO_MINIMIZE_ON_FOCUS_LOSS", "0", 0);
}

static void take_snapshot(void)
{
	unsigned char *buffer;
	int width, height;
	char filename[128];

	snprintf(filename, sizeof(filename), "star-light-preview-%05d.png", snapshot_number++);
	graph_dev_grab_framebuffer(&buffer, &width, &height);
	png_utils_write_png_image(filename, buffer, width, height, 1, 1);
	free(buffer);
	printf("Saved snapshot: %s %dx%d\n", filename, width, height);
}

static void handle_key_down(SDL_Keysym *keysym)
{
	switch (keysym->sym) {
	case SDLK_F1:
		helpmode = !helpmode;
		break;
	case SDLK_ESCAPE:
	case SDLK_q:
		quit(0);
		break;
	case SDLK_LEFT: /* Shift = dark tint, else light tint */
		if (keysym->mod & KMOD_SHIFT)
			dark_tint = clampf(dark_tint - 0.01f, 0.0f, 1.0f);
		else
			light_tint = clampf(light_tint - 0.01f, 0.0f, 1.0f);
		break;
	case SDLK_RIGHT:
		if (keysym->mod & KMOD_SHIFT)
			dark_tint = clampf(dark_tint + 0.01f, 0.0f, 1.0f);
		else
			light_tint = clampf(light_tint + 0.01f, 0.0f, 1.0f);
		break;
	case SDLK_DOWN:
		shadow_darkening = clampf(shadow_darkening - 0.02f, 0.0f, 1.0f);
		break;
	case SDLK_UP:
		shadow_darkening = clampf(shadow_darkening + 0.02f, 0.0f, 1.0f);
		break;
	case SDLK_LEFTBRACKET:
		ambient = clampf(ambient - 0.005f, 0.0f, 1.0f);
		break;
	case SDLK_RIGHTBRACKET:
		ambient = clampf(ambient + 0.005f, 0.0f, 1.0f);
		break;
	case SDLK_COMMA:
		surface_albedo = clampf(surface_albedo - 0.05f, 0.05f, 1.0f);
		break;
	case SDLK_PERIOD:
		surface_albedo = clampf(surface_albedo + 0.05f, 0.05f, 1.0f);
		break;
	case SDLK_0:
		light_tint = 0.45f;
		dark_tint = 0.06f;
		shadow_darkening = 0.0f;
		ambient = 0.015f;
		surface_albedo = 1.0f;
		break;
	case SDLK_t:
		apply_tonemap = !apply_tonemap;
		break;
	case SDLK_MINUS:
		tonemap_gain = clampf(tonemap_gain - 0.01f, 0.5f, 2.0f);
		break;
	case SDLK_EQUALS:
		tonemap_gain = clampf(tonemap_gain + 0.01f, 0.5f, 2.0f);
		break;
	case SDLK_p:
		take_snapshot();
		break;
	default:
		break;
	}
}

static void process_events(void)
{
	SDL_Event event;

	while (SDL_PollEvent(&event)) {
		switch (event.type) {
		case SDL_KEYDOWN:
			handle_key_down(&event.key.keysym);
			break;
		case SDL_QUIT:
			quit(0);
			break;
		case SDL_WINDOWEVENT:
			if (event.window.event == SDL_WINDOWEVENT_RESIZED ||
				event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
				SDL_GL_GetDrawableSize(screen, &real_screen_width, &real_screen_height);
				sng_set_screen_size(real_screen_width, real_screen_height);
			}
			break;
		case SDL_MOUSEMOTION:
			/* Map the mouse to extent space for the temperature readout. */
			cursor_x = (real_screen_width > 0) ?
				(float) event.motion.x * (float) SCREEN_WIDTH / (float) real_screen_width :
				-1.0f;
			break;
		default:
			break;
		}
	}
}

int main(int argc, char *argv[])
{
	uint32_t window_flags;

	(void) argc;
	(void) argv;
	setlocale(LC_ALL, "C");
	enable_sdl_fullscreen_sanity();

	if (SDL_Init(SDL_INIT_VIDEO) < 0) {
		fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
		return 1;
	}

	window_flags = SDL_WINDOW_RESIZABLE;
	graph_dev_prepare_for_window(&window_flags);

	real_screen_width = 3 * SCREEN_WIDTH / 4;
	real_screen_height = 3 * SCREEN_HEIGHT / 4;
	screen = SDL_CreateWindow("Star Light Preview", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
			real_screen_width, real_screen_height, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
	if (!screen) {
		fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
		return 1;
	}

	graph_dev_create_context(screen);
	sng_setup_colors(NULL);
	snis_typefaces_init();
	sng_set_font_family(0);
	graph_dev_setup(NULL);

	SDL_GL_GetDrawableSize(screen, &real_screen_width, &real_screen_height);
	sng_set_extent_size(SCREEN_WIDTH, SCREEN_HEIGHT);
	sng_set_screen_size(real_screen_width, real_screen_height);
	sng_set_clip_window(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);

	const double delta = 1.0 / (double) FPS;
	double next_time = time_now_double() + delta;

	while (1) {
		double current_time = time_now_double();

		if (current_time >= next_time) {
			next_time += delta;
			process_events();
			draw_screen();
		} else {
			double to_sleep = next_time - current_time;
			if (to_sleep > 0)
				sleep_double(to_sleep);
		}
	}
	return 0;
}
