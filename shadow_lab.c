/*
	shadow_lab: a standalone scene sandbox for iterating on the cascaded shadow
	mapping (CSM) shaders.  It loads a small scene of ships and a planet, lets you
	fly a free camera around, and (in later phases) possess ships, orbit the sun,
	and toggle the shadow debug views live.  It is deliberately not a physically
	plausible simulation; it exists only to make the shadow shaders quick to evaluate.

	THIS FILE IS AI-GENERATED AND IS PLACED IN THE PUBLIC DOMAIN.

	This is a self-contained ancillary tool: it links the SNIS engine but adds no
	logic to the core game.  Per CONTRIBUTING.md, AI-generated ancillary tooling is
	permitted when clearly marked as such and dedicated to the public domain.  The
	author(s) disclaim all copyright and neighboring rights to this file to the
	fullest extent permitted by law.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <limits.h>
#include <getopt.h>
#include <locale.h>

#ifdef __APPLE__
#include <SDL2.h>
#else
#include <SDL.h>
#endif

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
#include "snis_cardinal_colors.h"
#include "opengl_cap.h"
#include "build_info.h"
#include "png_utils.h"
#include "snis_xwindows_hacks.h"

#define DEFAULT_FOV (40.0 * M_PI / 180.0)
#define FPS 60

static int SCREEN_WIDTH = 1600;
static int SCREEN_HEIGHT = 900;
static int screen_offset_x;
static int screen_offset_y;
static int window_manager_can_constrain_aspect_ratio = 0;
static float original_aspect_ratio;
static int time_to_set_window_size = 0;
static int real_screen_width;
static int real_screen_height;
static char *program;
static float FOV = DEFAULT_FOV;
static int display_frame_stats = 1;
static int frame_counter = 0;
static int reload_shaders = 0;
static int helpmode = 0;
static SDL_Window *screen;

/* The scene.  A fixed-capacity list of objects rebuilt into the entity context each frame. */
#define MAX_SCENE_OBJECTS 64
enum scene_object_kind { SCENE_SHIP, SCENE_PLANET };
struct scene_object {
	enum scene_object_kind kind;
	struct mesh *mesh;
	struct material *material;
	union vec3 pos;
	union quat orientation;
	float scale;
	int color;
	int no_cast_shadow;
};
static struct scene_object scene[MAX_SCENE_OBJECTS];
static int scene_object_count;
static int planet_index = -1; /* scene[] index of the planet, for live radius/distance edits */
static float scene_scale = 1000.0; /* characteristic spacing of the scene, set at load time */

/* Free-fly camera. */
static union vec3 cam_pos = { { -2500.0, 1500.0, -2500.0 } };
static union quat cam_orientation = IDENTITY_QUAT_INITIALIZER;
static float move_speed; /* per-frame translation, derived from scene_scale */

/* Possession: the fly controls drive either the free camera (slot -1) or one ship, which the
 * camera then chases.  Tab cycles free camera -> ship 0 -> ... -> free camera. */
static int controlled_slot = -1;
static int ship_slots[MAX_SCENE_OBJECTS]; /* scene[] indices of ships, in possess order */
static int ship_slot_count;

/* Arcade "atmospheric" flight for the possessed ship: it flies where its nose points, with
 * throttle acceleration and drag (momentum) rather than realistic 6-DOF space flight.  The
 * chase camera trails behind and above and eases toward its target, catching up to the ship's
 * heading faster the quicker you go. */
static float ship_speed;      /* current speed along the nose, world units per frame */
static float ship_max_speed;  /* set from the scene scale in build_scene */
static float ship_accel;      /* throttle acceleration per frame */
/* The chase rig's orientation trails the ship's a little; the camera is placed behind and
 * above within this (lagged) frame and looks at the ship, so a turn briefly shows the ship
 * from the side before the camera swings in behind it. */
static union quat chase_cam_orientation = IDENTITY_QUAT_INITIALIZER;
static int chase_initialized;  /* 0 to snap the rig to the ship on the first possessed frame */
static float mouse_sensitivity = 0.003;
static float roll_rate = 0.02;
static float ambient_light = 0.015; /* match snis_client's default so lighting mirrors the game */
static int shadow_debug_mode = 0;   /* 0 = off, 1 = shadow factor, 2 = cascade index */
static int mouse_look_active = 0;
static float mouse_accum_dx = 0.0;
static float mouse_accum_dy = 0.0;

/* Sun.  Stored as an orbit (azimuth/elevation/distance) about the scene centre so it can be
 * swept live with the arrow keys; sun_pos is derived from it each frame.  sun_radius is the
 * star's world-unit radius that drives the analytic planet-shadow penumbra (mirrors
 * snis_client's star_radius). */
static union vec3 scene_center = { { 0.0, 0.0, 0.0 } };
static union vec3 sun_pos = { { 40000.0, 60000.0, 30000.0 } };
static float sun_azimuth = 0.6;
static float sun_elevation = 0.5;
static float sun_distance = 90000.0;
static float sun_radius = 2812.5; /* = default star_diameter (5625) / 2 */
static float deepest_planet_shade = 0.0; /* deepest analytic ship shading this frame, for the HUD */

/* The sun is drawn with the dedicated sun shader (MATERIAL_SUN): a world-scale solid disc plus
 * a screen-scale procedural bloom.  The billboard is sized to the bloom's on-screen extent (a
 * fraction of the camera distance) but never smaller than the disc, and the shader's disc
 * radius is set each frame from sun_radius / billboard size so the disc stays world-scale
 * (grows as you approach) while the bloom stays a constant apparent size. */
#define SUN_BLOOM_APPARENT 0.4 /* the sun billboard's on-screen size as a fraction of camera distance */
static struct mesh *sun_mesh;
static struct material sun_material;

/* Planet-shadow test controls (independent of the CSM depth-map shadows). */
enum planet_shade_mode { PLANET_SHADE_SOFT, PLANET_SHADE_BINARY, PLANET_SHADE_OFF };
static int planet_shade_mode = PLANET_SHADE_SOFT;
static int show_shade_panel = 1; /* per-ship lit/penumbra/umbra readout (viewing-independent) */
static const char * const planet_shade_mode_name[] = { "SOFT", "BINARY", "OFF" };

/* Per-ship analytic shade this frame, recorded during the draw loop for the HUD panel.  The
 * in_shade term only darkens a ship's sun-facing side, which is often hidden behind the
 * planet in an umbra test, so this numeric readout is the reliable way to see the mode
 * (SOFT ramps through the penumbra; BINARY jumps at 0.5; OFF stays 0). */
static float ship_shade[MAX_SCENE_OBJECTS];
static int ship_shade_count;

static struct mesh *planet_mesh;

/* Analytic planet umbra/penumbra shading, mirroring snis_client's update_shading_planet():
 * returns the deepest fraction (0.0 = fully lit, 1.0 = full umbra) of the sun's disc that any
 * scene planet blocks as seen from pos, via the angular overlap of the two discs. */
static float compute_planet_shade_fraction(const union vec3 *pos)
{
	int i;
	union vec3 to_sun, to_planet;
	float sun_dist, planet_dist;
	double alpha_sun, alpha_planet, theta, occlusion, deepest;

	vec3_sub(&to_sun, &sun_pos, (union vec3 *) pos);
	sun_dist = vec3_magnitude(&to_sun);
	if (sun_dist <= 0.0)
		return 0.0;
	alpha_sun = asin(clamp(sun_radius / sun_dist, 0.0, 1.0));
	deepest = 0.0;
	for (i = 0; i < scene_object_count; i++) {
		struct scene_object *p = &scene[i];

		if (p->kind != SCENE_PLANET)
			continue;
		vec3_sub(&to_planet, &p->pos, (union vec3 *) pos);
		planet_dist = vec3_magnitude(&to_planet);
		if (planet_dist >= sun_dist) /* planet is farther away than the sun */
			continue;
		if (planet_dist <= p->scale) { /* inside the planet (scale holds its radius) */
			occlusion = 1.0;
		} else {
			alpha_planet = asin(clamp(p->scale / planet_dist, 0.0, 1.0));
			theta = acos(clamp(vec3_dot(&to_sun, &to_planet) /
						(sun_dist * planet_dist), -1.0, 1.0));
			occlusion = disc_occlusion_fraction(alpha_sun, alpha_planet, theta);
		}
		if (occlusion > deepest)
			deepest = occlusion;
	}

	switch (planet_shade_mode) {
	case PLANET_SHADE_BINARY: /* the old hack: a hard on/off at half occlusion */
		return deepest > 0.5 ? 1.0 : 0.0;
	case PLANET_SHADE_OFF:
		return 0.0;
	case PLANET_SHADE_SOFT:
	default:
		return (float) deepest;
	}
}

static struct mesh *snis_read_model(char *path)
{
	float minx, miny, minz, maxx, maxy, maxz;
	struct mesh *m;

	m = read_mesh(path);
	if (!m) {
		fprintf(stderr, "shadow_lab: bad mesh file '%s'\n", path);
		return NULL;
	}
	mesh_aabb(m, &minx, &miny, &minz, &maxx, &maxy, &maxz);
	printf("%s aabb = (%f,%f,%f), (%f, %f, %f) radius=%f\n",
		path, minx, miny, minz, maxx, maxy, maxz, m->radius);
	return m;
}

static struct scene_object *add_scene_object(enum scene_object_kind kind, struct mesh *m,
		struct material *material, float x, float y, float z, float scale, int color)
{
	struct scene_object *o;

	if (scene_object_count >= MAX_SCENE_OBJECTS) {
		fprintf(stderr, "shadow_lab: too many scene objects (max %d)\n", MAX_SCENE_OBJECTS);
		return NULL;
	}
	o = &scene[scene_object_count++];
	o->kind = kind;
	o->mesh = m;
	o->material = material;
	o->pos.v.x = x;
	o->pos.v.y = y;
	o->pos.v.z = z;
	o->orientation = (union quat) IDENTITY_QUAT_INITIALIZER;
	o->scale = scale;
	o->color = color;
	o->no_cast_shadow = 0;
	return o;
}

/* The scene's ships.  Each model is authored in its own frame; the game brings it into the
 * nose-along-+x / up-along-+y game frame with per-model rotations from ship_types.txt, so we
 * replicate those here (only the wombat needs any) or the possessed ship faces the wrong way.
 * Angles are passed to quat_init_axis() exactly as the game does. */
struct lab_ship_model {
	const char *path;
	char axis[3];   /* rotation axes applied in order (matching ship_types.txt) */
	float angle[3]; /* matching angles in degrees, as written in ship_types.txt */
	int nrot;
};
static const struct lab_ship_model phase1_ship_models[] = {
	{ "share/snis/models/wombat/snis3006.obj", { 'x', 'y' }, { -90.0, 90.0 }, 2 },
	{ "share/snis/models/disruptor/disruptor.obj", { 0 }, { 0 }, 0 },
	{ "share/snis/models/enforcer/enforcer.obj", { 0 }, { 0 }, 0 },
	{ "share/snis/models/conqueror/conqueror.obj", { 0 }, { 0 }, 0 },
};

static void apply_model_rotations(struct mesh *m, const struct lab_ship_model *spec)
{
	int j;

	for (j = 0; j < spec->nrot; j++) {
		union quat q;
		float radians = spec->angle[j] * M_PI / 180.0; /* ship_types.txt angles are degrees */

		quat_init_axis(&q, (float) (spec->axis[j] == 'x'), (float) (spec->axis[j] == 'y'),
				(float) (spec->axis[j] == 'z'), radians);
		mesh_rotate(m, &q);
	}
}

static void build_scene(void)
{
	int i;
	int nships = sizeof(phase1_ship_models) / sizeof(phase1_ship_models[0]);
	struct mesh *ship_mesh[8];
	float max_radius = 0.0;
	float spacing;

	for (i = 0; i < nships; i++) {
		ship_mesh[i] = snis_read_model((char *) phase1_ship_models[i].path);
		if (!ship_mesh[i])
			exit(1);
		apply_model_rotations(ship_mesh[i], &phase1_ship_models[i]);
		if (ship_mesh[i]->radius > max_radius)
			max_radius = ship_mesh[i]->radius;
	}

	/* Space the ships out relative to the largest one, but keep the cluster inside the
	 * shadow map's coverage distance so all cascades are exercised at once. */
	spacing = max_radius * 4.0;
	if (spacing < 120.0)
		spacing = 120.0;
	if (spacing > 1200.0)
		spacing = 1200.0;
	scene_scale = spacing;

	/* The occluder for the analytic planet umbra/penumbra test: lowering the sun (down arrow)
	 * swings it between the sun and the ship cluster, sweeping the ships through the penumbra
	 * into the umbra. */
	planet_mesh = mesh_unit_spherified_cube(64);
	if (planet_mesh) {
		/* Absolute values tuned to balance a roughly plausible scale against seeing the
		 * effect: a 1000-unit planet 2500 below the cluster, lit by a 4000-unit sun at
		 * 40000.  The penumbra's spatial width grows with the ships' distance from the
		 * planet, so the cluster spans a soft terminator without an extreme sun.  All are
		 * adjustable live (planet radius/distance 3/4, 5/6; sun distance/radius U/O, G/H). */
		float planet_r = 500.0;
		struct scene_object *p = add_scene_object(SCENE_PLANET, planet_mesh, NULL,
				0.0, -2500.0, 0.0, planet_r, GRAY50);
		if (p) {
			p->color = GRAY50;
			/* The planet must not cast into the CSM depth map: planet->ship shadowing is
			 * handled analytically (in_shade), and a planet-sized hard-edged caster in the
			 * shadow map otherwise sweeps across and blacks out whole ships as the cascades
			 * refit.  This matches the plan's "planets cast never" policy. */
			p->no_cast_shadow = 1;
			planet_index = (int) (p - scene);
			scene_center = p->pos; /* orbit the sun about the planet */
			sun_distance = 40000.0;
			sun_radius = 2000.0; /* 4000-unit diameter */
		}
	}

	/* A tight ship cluster so several ships sit inside the penumbra band at once and shadow
	 * each other for the CSM test. */
	add_scene_object(SCENE_SHIP, ship_mesh[0 % nships], NULL, 0.0, 0.0, 0.0, 1.0, WHITE);
	if (nships > 1)
		add_scene_object(SCENE_SHIP, ship_mesh[1], NULL, spacing * 0.6, spacing * 0.2, 0.0, 1.0, AMBER);
	if (nships > 2)
		add_scene_object(SCENE_SHIP, ship_mesh[2], NULL,
				-spacing * 0.4, spacing * 0.05, spacing * 0.6, 1.0, WHITE);
	if (nships > 3)
		add_scene_object(SCENE_SHIP, ship_mesh[3], NULL,
				spacing * 0.25, -spacing * 0.25, spacing * 0.8, 1.0, WHITE);

	/* Record the ships, in order, as the possess-cycle slots. */
	ship_slot_count = 0;
	for (i = 0; i < scene_object_count; i++) {
		if (scene[i].kind == SCENE_SHIP && ship_slot_count < MAX_SCENE_OBJECTS)
			ship_slots[ship_slot_count++] = i;
	}

	move_speed = spacing * 0.02;
	/* Possessed-ship flight: a slower top speed than the free camera so a ship can be eased
	 * through the narrow penumbra band, reached over about a second of throttle. */
	ship_max_speed = spacing * 0.008;
	ship_accel = ship_max_speed * 0.06;

	/* Seed the shadow tunables to the values tuned here (and now the game defaults); all
	 * remain adjustable live.  Six cascades with a mild log split over ~4500 units of
	 * camera-local coverage give crisp near shadows that still reach distance; a small blend
	 * band hides the cascade seams and fades the last cascade to lit at the coverage edge. */
	set_shadow_map_max_distance(4500.0);
	set_shadow_map_num_cascades(6);
	set_shadow_map_split_lambda(0.6);
	graph_dev_set_shadow_bias(2.5, 4.0);
	graph_dev_set_shadow_pcf_radius(1);
	graph_dev_set_shadow_blend(0.2);

	/* Aim the camera at the ship cluster from behind and above. */
	{
		union vec3 base_fwd = { { 1.0, 0.0, 0.0 } };
		union vec3 up = { { 0.0, 1.0, 0.0 } };
		union vec3 desired_fwd;

		cam_pos.v.x = -2.0 * spacing;
		cam_pos.v.y = 1.3 * spacing;
		cam_pos.v.z = -2.0 * spacing;
		vec3_sub(&desired_fwd, &(union vec3){ { 0.0, 0.0, 0.0 } }, &cam_pos);
		vec3_normalize_self(&desired_fwd);
		quat_from_u2v(&cam_orientation, &base_fwd, &desired_fwd, &up);
	}
}

/* Extract the camera's forward/up/right basis vectors from its orientation quaternion. */
static void camera_basis(const union quat *o, union vec3 *fwd, union vec3 *up, union vec3 *right)
{
	fwd->v.x = 1.0; fwd->v.y = 0.0; fwd->v.z = 0.0;
	up->v.x = 0.0; up->v.y = 1.0; up->v.z = 0.0;
	right->v.x = 0.0; right->v.y = 0.0; right->v.z = 1.0;
	quat_rot_vec_self(fwd, o);
	quat_rot_vec_self(up, o);
	quat_rot_vec_self(right, o);
}

/* Apply a rotation of 'angle' radians about a body-frame axis to the camera orientation. */
static void adjust_shadow_bias(float dfactor, float dunits)
{
	float factor, units;

	graph_dev_get_shadow_bias(&factor, &units);
	factor += dfactor;
	units += dunits;
	if (factor < 0.0)
		factor = 0.0;
	if (units < 0.0)
		units = 0.0;
	graph_dev_set_shadow_bias(factor, units);
}

static void quat_rotate_local(union quat *o, float ax, float ay, float az, float angle)
{
	union quat delta;

	if (angle == 0.0)
		return;
	quat_init_axis(&delta, ax, ay, az, angle);
	quat_mul(o, o, &delta);
	quat_normalize_self(o);
}

/* Rotate about a world-space axis (pre-multiply), for aircraft-style yaw that turns the
 * heading without accumulating roll the way a local-axis yaw does. */
static void quat_rotate_world(union quat *o, float ax, float ay, float az, float angle)
{
	union quat delta;

	if (angle == 0.0)
		return;
	quat_init_axis(&delta, ax, ay, az, angle);
	quat_mul(o, &delta, o);
	quat_normalize_self(o);
}

/* Yaw/pitch/roll an orientation from the right-drag mouse and Q/E, shared by the free camera
 * and the possessed ship. */
static void apply_look_controls(union quat *orientation)
{
	const Uint8 *keys = SDL_GetKeyboardState(NULL);
	float roll = 0.0;

	quat_rotate_local(orientation, 0.0, 1.0, 0.0, -mouse_accum_dx * mouse_sensitivity);
	quat_rotate_local(orientation, 0.0, 0.0, 1.0, -mouse_accum_dy * mouse_sensitivity);
	mouse_accum_dx = 0.0;
	mouse_accum_dy = 0.0;

	if (keys[SDL_SCANCODE_Q])
		roll += roll_rate;
	if (keys[SDL_SCANCODE_E])
		roll -= roll_rate;
	quat_rotate_local(orientation, 1.0, 0.0, 0.0, roll);
}

/* Free-fly camera: look controls plus WASD/RF translation along its own basis. */
static void fly_controls(union vec3 *pos, union quat *orientation, float speed)
{
	const Uint8 *keys = SDL_GetKeyboardState(NULL);
	union vec3 fwd, up, right, step;

	if (keys[SDL_SCANCODE_LSHIFT] || keys[SDL_SCANCODE_RSHIFT])
		speed *= 5.0;

	apply_look_controls(orientation);
	camera_basis(orientation, &fwd, &up, &right);

	if (keys[SDL_SCANCODE_W]) {
		step = fwd; vec3_mul_self(&step, speed); vec3_add_self(pos, &step);
	}
	if (keys[SDL_SCANCODE_S]) {
		step = fwd; vec3_mul_self(&step, -speed); vec3_add_self(pos, &step);
	}
	if (keys[SDL_SCANCODE_D]) {
		step = right; vec3_mul_self(&step, speed); vec3_add_self(pos, &step);
	}
	if (keys[SDL_SCANCODE_A]) {
		step = right; vec3_mul_self(&step, -speed); vec3_add_self(pos, &step);
	}
	if (keys[SDL_SCANCODE_R]) {
		step = up; vec3_mul_self(&step, speed); vec3_add_self(pos, &step);
	}
	if (keys[SDL_SCANCODE_F]) {
		step = up; vec3_mul_self(&step, -speed); vec3_add_self(pos, &step);
	}
}

/* Arcade flight for a possessed ship: turn with the mouse/Q-E, throttle with W (forward) and
 * S (brake/reverse), coast with drag, and move along the nose.  The ship goes where it points
 * (atmosphere-like) but keeps momentum. */
static void fly_ship(struct scene_object *ship)
{
	const Uint8 *keys = SDL_GetKeyboardState(NULL);
	union vec3 fwd, up, right, step;
	float roll = 0.0;

	/* Aircraft-style steering: yaw about world up (heading, no roll drift), pitch about the
	 * ship's own right axis, and roll only when asked.  Right-drag mouse steers. */
	quat_rotate_world(&ship->orientation, 0.0, 1.0, 0.0, -mouse_accum_dx * mouse_sensitivity);
	quat_rotate_local(&ship->orientation, 0.0, 0.0, 1.0, -mouse_accum_dy * mouse_sensitivity);
	mouse_accum_dx = 0.0;
	mouse_accum_dy = 0.0;
	if (keys[SDL_SCANCODE_Q])
		roll += roll_rate;
	if (keys[SDL_SCANCODE_E])
		roll -= roll_rate;
	quat_rotate_local(&ship->orientation, 1.0, 0.0, 0.0, roll);

	if (keys[SDL_SCANCODE_W])
		ship_speed += ship_accel;
	if (keys[SDL_SCANCODE_S])
		ship_speed -= ship_accel * 1.5; /* brake / reverse harder than it accelerates */
	ship_speed *= 0.985;                    /* drag: coast to a stop when off the throttle */
	if (ship_speed > ship_max_speed)
		ship_speed = ship_max_speed;
	if (ship_speed < -ship_max_speed * 0.4) /* reverse is slower than forward */
		ship_speed = -ship_max_speed * 0.4;

	camera_basis(&ship->orientation, &fwd, &up, &right);
	step = fwd; vec3_mul_self(&step, ship_speed); vec3_add_self(&ship->pos, &step);
}

/* Drive whatever is currently controlled.  Free camera: fly the camera directly.  A possessed
 * ship: fly the ship and trail it with a lagging third-person chase camera, so flying a ship
 * through the planet's penumbra updates its analytic shade live. */
static void update_camera(void)
{
	struct scene_object *ship;
	union vec3 fwd, up, right, aim, look_dir, offset;
	union vec3 base_fwd = { { 1.0, 0.0, 0.0 } };
	float dist, lift;

	if (controlled_slot < 0 || controlled_slot >= ship_slot_count) {
		fly_controls(&cam_pos, &cam_orientation, move_speed);
		return;
	}

	ship = &scene[ship_slots[controlled_slot]];
	fly_ship(ship);

	/* The rig orientation trails the ship's by a small fixed amount (the lag), snapping to it
	 * on the first possessed frame.  Everything below is derived from this one frame, so the
	 * camera stays on the behind-and-above axis rather than drifting off to one side. */
	if (!chase_initialized) {
		chase_cam_orientation = ship->orientation;
		chase_initialized = 1;
	} else {
		union quat prev = chase_cam_orientation;

		quat_nlerp(&chase_cam_orientation, &prev, &ship->orientation, 0.15);
	}

	/* Behind and above the ship in the lagged rig frame, looking at the ship (aimed a little
	 * low so it sits slightly above centre) with the rig's own up so the view banks with it.
	 * A turn briefly shows the ship from the side until the rig swings in behind. */
	camera_basis(&chase_cam_orientation, &fwd, &up, &right);
	dist = scene_scale * 0.5;
	lift = scene_scale * 0.18;
	cam_pos = ship->pos;
	offset = fwd; vec3_mul_self(&offset, -dist); vec3_add_self(&cam_pos, &offset);
	offset = up; vec3_mul_self(&offset, lift); vec3_add_self(&cam_pos, &offset);
	/* Aim ahead of the ship (and a little down) so the space in front of it is centred and
	 * clearly visible, with the ship itself sitting in the lower part of the frame. */
	aim = ship->pos;
	offset = fwd; vec3_mul_self(&offset, dist * 0.8); vec3_add_self(&aim, &offset);
	offset = up; vec3_mul_self(&offset, -lift * 0.2); vec3_add_self(&aim, &offset);
	vec3_sub(&look_dir, &aim, &cam_pos);
	vec3_normalize_self(&look_dir);
	quat_from_u2v(&cam_orientation, &base_fwd, &look_dir, &up);
}

/* Sweep the sun around the scene centre with the arrow keys (azimuth/elevation) and derive
 * its world position.  Held continuously for a smooth sweep through the planet's penumbra. */
static void update_sun(void)
{
	const Uint8 *keys = SDL_GetKeyboardState(NULL);
	float rate = 0.01;
	float ce, se;

	if (keys[SDL_SCANCODE_LSHIFT] || keys[SDL_SCANCODE_RSHIFT])
		rate *= 4.0;
	if (keys[SDL_SCANCODE_LEFT])
		sun_azimuth -= rate;
	if (keys[SDL_SCANCODE_RIGHT])
		sun_azimuth += rate;
	if (keys[SDL_SCANCODE_UP])
		sun_elevation += rate;
	if (keys[SDL_SCANCODE_DOWN])
		sun_elevation -= rate;
	if (sun_elevation > 1.55)
		sun_elevation = 1.55;
	if (sun_elevation < -1.55)
		sun_elevation = -1.55;

	ce = cosf(sun_elevation);
	se = sinf(sun_elevation);
	sun_pos.v.x = scene_center.v.x + sun_distance * ce * cosf(sun_azimuth);
	sun_pos.v.y = scene_center.v.y + sun_distance * se;
	sun_pos.v.z = scene_center.v.z + sun_distance * ce * sinf(sun_azimuth);
}

static char *help_text =
	"SHADOW LAB\n\n"
	"  A sandbox for iterating on the cascaded shadow mapping shaders.\n\n"
	"  FREE CAMERA\n"
	"  - W / S            MOVE FORWARD / BACK\n"
	"  - A / D            STRAFE LEFT / RIGHT\n"
	"  - R / F            MOVE UP / DOWN\n"
	"  - Q / E            ROLL LEFT / RIGHT\n"
	"  - HOLD RIGHT MOUSE MOVE MOUSE TO LOOK AROUND\n"
	"  - SHIFT            MOVE FASTER\n\n"
	"  POSSESSED SHIP (TAB TO POSSESS NEXT SHIP / RETURN TO FREE CAMERA)\n"
	"  - W / S            THROTTLE FORWARD / BRAKE-REVERSE (MOMENTUM + DRAG)\n"
	"  - Q / E            ROLL;  RIGHT-DRAG MOUSE TO STEER (YAW / PITCH)\n"
	"  - THE SHIP FLIES WHERE ITS NOSE POINTS; A CHASE CAM TRAILS IT.  FLY A SHIP\n"
	"    THROUGH THE PENUMBRA TO WATCH ITS SHADE (PANEL) RAMP LIT -> UMBRA.\n\n"
	"  SUN (ANALYTIC PLANET UMBRA / PENUMBRA)\n"
	"  - ARROW KEYS       ORBIT SUN AZIMUTH / ELEVATION (SHIFT = FASTER)\n"
	"  - U / O            SUN DISTANCE CLOSER / FARTHER\n"
	"  - G / H            SUN RADIUS SMALLER / LARGER (WIDER RADIUS = WIDER PENUMBRA)\n"
	"  - P                CYCLE PLANET SHADE MODE: SOFT / BINARY / OFF\n"
	"  - B                TOGGLE THE PER-SHIP SHADE PANEL\n"
	"  - 3 / 4            PLANET RADIUS SMALLER / LARGER\n"
	"  - 5 / 6            PLANET CLOSER / FARTHER FROM THE SHIP CLUSTER\n"
	"                     (FARTHER + TIGHTER CLUSTER = WIDER, SOFTER PENUMBRA)\n"
	"  - LOWER THE SUN (DOWN ARROW) TO SWING THE PLANET BETWEEN IT AND THE SHIPS.\n"
	"    NOTE: in_shade ONLY DARKENS A SHIP'S SUN-FACING SIDE, WHICH IS HIDDEN\n"
	"    BEHIND THE PLANET IN A DIRECT UMBRA TEST - THE DARKENING YOU SEE THERE IS\n"
	"    THE ORDINARY DAY/NIGHT TERMINATOR.  WATCH THE PER-SHIP PANEL: SOFT RAMPS\n"
	"    THROUGH THE PENUMBRA, BINARY JUMPS AT 0.5, OFF STAYS 0.  TO SEE IT ON THE\n"
	"    HULL, VIEW A SHIP'S SUN-FACING FLANK NEAR THE SHADOW EDGE (OBLIQUE ANGLE).\n\n"
	"  SHADOWS\n"
	"  - \\                TOGGLE SHADOWS ON / OFF\n"
	"  - 0 / 1 / 2        DEBUG: OFF / SHADOW-FACTOR / CASCADE-INDEX\n"
	"  - [ / ]            SHADOW COVERAGE DISTANCE DOWN / UP\n"
	"  - - / =            CASCADE COUNT DOWN / UP (1-6)\n"
	"  - ; / '            SPLIT LAMBDA DOWN / UP (log vs uniform)\n"
	"  - , / .            DEPTH-BIAS SLOPE DOWN / UP\n"
	"  - n / m            PCF NEAR KERNEL SMALLER / LARGER (tapers per cascade)\n"
	"  - k / l            CROSS-CASCADE BLEND BAND SMALLER / LARGER\n\n"
	"  OTHER\n"
	"  - F1               TOGGLE THIS HELP\n"
	"  - F10              RELOAD SHADERS\n"
	"  - F11              TOGGLE FULLSCREEN\n"
	"  - ESC              QUIT\n\n"
	"  CASCADE-INDEX TINT: red=0 (nearest) green=1 blue=2 yellow=3 cyan=4 magenta=5 gray=none\n\n"
	"PRESS F1 TO EXIT HELP\n";

static void draw_help_text(const char *text)
{
	int line = 0;
	int i, y = 70;
	char buffer[1024];
	int buflen = 0;

	buffer[0] = '\0';
	i = 0;
	do {
		if (text[i] == '\n' || text[i] == '\0') {
			buffer[buflen] = '\0';
			sng_abs_xy_draw_string(buffer, TINY_FONT, 60, y);
			y += 19;
			buffer[0] = '\0';
			buflen = 0;
			line++;
			if (text[i] == '\0')
				break;
			i++;
			continue;
		}
		buffer[buflen++] = text[i++];
	} while (1);
}

static void draw_help_screen(void)
{
	sng_set_foreground(BLACK);
	sng_current_draw_rectangle(1, 50, 50, SCREEN_WIDTH - 100, SCREEN_HEIGHT - 100);
	sng_set_foreground(GREEN);
	sng_current_draw_rectangle(0, 50, 50, SCREEN_WIDTH - 100, SCREEN_HEIGHT - 100);
	draw_help_text(help_text);
}

static void draw_hud(void)
{
	char buffer[160];
	float bias_factor;
	int y = 24;
	const int dy = 24;
	static const char * const debug_name[] = { "OFF", "SHADOW-FACTOR", "CASCADE-INDEX" };
	const char *state = deepest_planet_shade >= 0.999 ? "UMBRA" :
				deepest_planet_shade > 0.001 ? "PENUMBRA" : "LIT";

	graph_dev_get_shadow_bias(&bias_factor, NULL);

	sng_set_foreground(WHITE);
	sng_abs_xy_draw_string("SHADOW LAB - F1 FOR HELP", TINY_FONT, 10, y); y += dy;
	if (controlled_slot < 0)
		snprintf(buffer, sizeof(buffer), "CONTROL FREE CAMERA (TAB)   CAM (%.0f, %.0f, %.0f)",
			cam_pos.v.x, cam_pos.v.y, cam_pos.v.z);
	else
		snprintf(buffer, sizeof(buffer), "CONTROL SHIP %d (TAB)   SPEED %.0f%%   W/S THROTTLE",
			controlled_slot,
			ship_max_speed > 0.0 ? 100.0 * ship_speed / ship_max_speed : 0.0);
	sng_abs_xy_draw_string(buffer, TINY_FONT, 10, y); y += dy;
	snprintf(buffer, sizeof(buffer), "SUN AZ %.0f EL %.0f DIST %.0f RADIUS %.0f",
		radians_to_degrees(sun_azimuth), radians_to_degrees(sun_elevation),
		sun_distance, sun_radius);
	sng_abs_xy_draw_string(buffer, TINY_FONT, 10, y); y += dy;
	if (planet_index >= 0) {
		float pr = scene[planet_index].scale;
		float pd = -scene[planet_index].pos.v.y; /* cluster sits near the origin */

		snprintf(buffer, sizeof(buffer), "PLANET R %.0f  DIST %.0f  GAP %.0f", pr, pd, pd - pr);
		sng_abs_xy_draw_string(buffer, TINY_FONT, 10, y); y += dy;
	}
	snprintf(buffer, sizeof(buffer), "PLANET SHADE MODE %s   DEEPEST %s (%.2f)",
		planet_shade_mode_name[planet_shade_mode], state, deepest_planet_shade);
	sng_abs_xy_draw_string(buffer, TINY_FONT, 10, y); y += dy;
	snprintf(buffer, sizeof(buffer), "SHADOWS %s   DEBUG %s",
		graph_dev_shadow_map_enabled ? "ON" : "OFF",
		debug_name[shadow_debug_mode % 3]);
	sng_abs_xy_draw_string(buffer, TINY_FONT, 10, y); y += dy;
	snprintf(buffer, sizeof(buffer), "COVERAGE %.0f  CASCADES %d  LAMBDA %.2f",
		get_shadow_map_max_distance(), get_shadow_map_num_cascades(),
		get_shadow_map_split_lambda());
	sng_abs_xy_draw_string(buffer, TINY_FONT, 10, y); y += dy;
	snprintf(buffer, sizeof(buffer), "BIAS slope %.1f   PCF %dx%d   BLEND %.2f",
		bias_factor,
		2 * graph_dev_get_shadow_pcf_radius() + 1, 2 * graph_dev_get_shadow_pcf_radius() + 1,
		graph_dev_get_shadow_blend());
	sng_abs_xy_draw_string(buffer, TINY_FONT, 10, y); y += dy;

	/* Per-ship analytic shade, colour-coded by state.  This is the reliable readout: the
	 * in_shade darkening lands on each ship's sun-facing side, which is usually hidden
	 * behind the planet during an umbra test, so the render alone can look unchanged. */
	if (show_shade_panel) {
		int s;

		y += dy / 2;
		sng_set_foreground(WHITE);
		sng_abs_xy_draw_string("SHIP SHADE (0.00 LIT .. 1.00 UMBRA):", TINY_FONT, 10, y);
		y += dy;
		for (s = 0; s < ship_shade_count; s++) {
			float f = ship_shade[s];
			const char *st = f >= 0.999 ? "UMBRA" : f > 0.001 ? "PENUMBRA" : "LIT";

			sng_set_foreground(f >= 0.999 ? RED : f > 0.001 ? AMBER : GREEN);
			snprintf(buffer, sizeof(buffer), "  SHIP %d: %.2f  %s", s, f, st);
			sng_abs_xy_draw_string(buffer, TINY_FONT, 10, y);
			y += dy;
		}
		sng_set_foreground(WHITE);
	}
}

static struct entity_context *cx;

static void draw_screen(void)
{
	int i;
	union vec3 fwd, up, right, at;
	float near_plane, far_plane;

	glClearColor(0.0, 0.0, 0.0, 0.0);
	graph_dev_start_frame();

	if (!cx) {
		cx = entity_context_new(MAX_SCENE_OBJECTS + 8, 8);
		set_renderer(cx, FLATSHADING_RENDERER);
	}
	set_ambient_light(cx, ambient_light);

	update_camera();
	update_sun();

	camera_basis(&cam_orientation, &fwd, &up, &right);
	vec3_add(&at, &cam_pos, &fwd);

	near_plane = 1.0;
	far_plane = scene_scale * 200.0;
	camera_set_parameters(cx, near_plane, far_plane, SCREEN_WIDTH, SCREEN_HEIGHT, FOV);
	camera_set_pos(cx, cam_pos.v.x, cam_pos.v.y, cam_pos.v.z);
	camera_look_at(cx, at.v.x, at.v.y, at.v.z);
	camera_assign_up_direction(cx, up.v.x, up.v.y, up.v.z);
	set_lighting(cx, sun_pos.v.x, sun_pos.v.y, sun_pos.v.z);
	calculate_camera_transform(cx);

	deepest_planet_shade = 0.0;
	ship_shade_count = 0;
	for (i = 0; i < scene_object_count; i++) {
		struct scene_object *o = &scene[i];
		struct entity *e = add_entity(cx, o->mesh, o->pos.v.x, o->pos.v.y, o->pos.v.z, o->color);
		if (!e)
			continue;
		update_entity_orientation(e, &o->orientation);
		update_entity_scale(e, o->scale);
		if (o->material)
			update_entity_material(e, o->material);
		if (o->no_cast_shadow)
			update_entity_shadow_casting(e, 0);
		/* Ships receive the analytic planet umbra/penumbra shading (0.0 lit .. 1.0 umbra),
		 * matching snis_client's object_in_shade(); the planet is lit by its own terminator
		 * (surface normal vs. sun) and needs no in-shade term. */
		if (o->kind == SCENE_SHIP) {
			float frac = compute_planet_shade_fraction(&o->pos);
			entity_set_in_shade(e, frac);
			if (frac > deepest_planet_shade)
				deepest_planet_shade = frac;
			if (ship_shade_count < MAX_SCENE_OBJECTS)
				ship_shade[ship_shade_count++] = frac;
		}
	}

	/* The sun (MATERIAL_SUN): billboard sized to the bloom's screen extent but never smaller
	 * than the disc; the shader's disc radius is set from sun_radius / billboard size so the
	 * disc is world-scale and the bloom is screen-scale.  It must not cast shadows. */
	if (sun_mesh) {
		union vec3 to_cam;
		float cam_dist, billboard_world;
		struct entity *e;

		vec3_sub(&to_cam, &sun_pos, &cam_pos);
		cam_dist = vec3_magnitude(&to_cam);
		billboard_world = SUN_BLOOM_APPARENT * cam_dist;
		if (billboard_world < 2.0 * sun_radius)
			billboard_world = 2.0 * sun_radius;
		sun_material.sun.disc_radius = sun_radius / billboard_world;
		e = add_entity(cx, sun_mesh, sun_pos.v.x, sun_pos.v.y, sun_pos.v.z, WHITE);
		if (e) {
			update_entity_scale(e, billboard_world);
			update_entity_material(e, &sun_material);
			update_entity_shadow_casting(e, 0);
		}
	}

	render_skybox(cx);
	render_entities(cx);
	remove_all_entity(cx);

	draw_hud();
	if (helpmode)
		draw_help_screen();

	graph_dev_end_frame();
	glFinish();
	SDL_GL_SwapWindow(screen);
	frame_counter++;
}

static void quit(int code)
{
	SDL_Quit();
	exit(code);
}

static void handle_key_down(SDL_Keysym *keysym)
{
	static int fullscreen = 0;

	switch (keysym->sym) {
	case SDLK_F1:
		helpmode = !helpmode;
		break;
	case SDLK_F10:
		reload_shaders = 1;
		break;
	case SDLK_F11:
		fullscreen = !fullscreen;
		SDL_SetWindowFullscreen(screen, fullscreen * SDL_WINDOW_FULLSCREEN_DESKTOP);
		break;
	case SDLK_ESCAPE:
		quit(0);
		break;
	case SDLK_PAUSE:
		display_frame_stats = (display_frame_stats + 1) % 3;
		break;
	case SDLK_BACKSLASH:
		graph_dev_shadow_map_enabled = !graph_dev_shadow_map_enabled;
		break;
	case SDLK_0:
		shadow_debug_mode = 0;
		graph_dev_set_shadow_debug(shadow_debug_mode);
		break;
	case SDLK_1:
		shadow_debug_mode = 1;
		graph_dev_set_shadow_debug(shadow_debug_mode);
		break;
	case SDLK_2:
		shadow_debug_mode = 2;
		graph_dev_set_shadow_debug(shadow_debug_mode);
		break;
	case SDLK_LEFTBRACKET:
		set_shadow_map_max_distance(get_shadow_map_max_distance() * 0.8);
		break;
	case SDLK_RIGHTBRACKET:
		set_shadow_map_max_distance(get_shadow_map_max_distance() * 1.25);
		break;
	case SDLK_MINUS:
	case SDLK_KP_MINUS:
		set_shadow_map_num_cascades(get_shadow_map_num_cascades() - 1);
		break;
	case SDLK_EQUALS:
	case SDLK_KP_PLUS:
		set_shadow_map_num_cascades(get_shadow_map_num_cascades() + 1);
		break;
	case SDLK_SEMICOLON:
		set_shadow_map_split_lambda(get_shadow_map_split_lambda() - 0.05);
		break;
	case SDLK_QUOTE:
		set_shadow_map_split_lambda(get_shadow_map_split_lambda() + 0.05);
		break;
	case SDLK_COMMA:
		adjust_shadow_bias(-0.5, 0.0);
		break;
	case SDLK_PERIOD:
		adjust_shadow_bias(0.5, 0.0);
		break;
	case SDLK_n:
		graph_dev_set_shadow_pcf_radius(graph_dev_get_shadow_pcf_radius() - 1);
		break;
	case SDLK_m:
		graph_dev_set_shadow_pcf_radius(graph_dev_get_shadow_pcf_radius() + 1);
		break;
	case SDLK_k:
		graph_dev_set_shadow_blend(graph_dev_get_shadow_blend() - 0.02);
		break;
	case SDLK_l:
		graph_dev_set_shadow_blend(graph_dev_get_shadow_blend() + 0.02);
		break;
	case SDLK_u:
		sun_distance *= 0.9;
		break;
	case SDLK_o:
		sun_distance *= 1.111111;
		break;
	case SDLK_g:
		sun_radius *= 0.9;
		if (sun_radius < 1.0)
			sun_radius = 1.0;
		break;
	case SDLK_h:
		sun_radius *= 1.111111;
		break;
	case SDLK_p:
		planet_shade_mode = (planet_shade_mode + 1) % 3;
		break;
	case SDLK_b:
		show_shade_panel = !show_shade_panel;
		break;
	case SDLK_TAB:
		/* Cycle: free camera -> ship 0 -> ... -> last ship -> free camera. */
		controlled_slot++;
		if (controlled_slot >= ship_slot_count)
			controlled_slot = -1;
		ship_speed = 0.0;       /* start from rest */
		chase_initialized = 0;  /* snap the chase cam on the first frame, then lag */
		break;
	case SDLK_3: /* planet smaller */
		if (planet_index >= 0)
			scene[planet_index].scale *= 0.9;
		break;
	case SDLK_4: /* planet larger */
		if (planet_index >= 0)
			scene[planet_index].scale *= 1.111111;
		break;
	case SDLK_5: /* planet closer to the ship cluster */
		if (planet_index >= 0) {
			scene[planet_index].pos.v.y += scene_scale * 2.0;
			if (scene[planet_index].pos.v.y > -scene[planet_index].scale * 1.5)
				scene[planet_index].pos.v.y = -scene[planet_index].scale * 1.5;
			scene_center = scene[planet_index].pos;
		}
		break;
	case SDLK_6: /* planet farther from the ship cluster */
		if (planet_index >= 0) {
			scene[planet_index].pos.v.y -= scene_scale * 2.0;
			scene_center = scene[planet_index].pos;
		}
		break;
	default:
		break;
	}
}

static void set_mouse_look(int active)
{
	if (active == mouse_look_active)
		return;
	mouse_look_active = active;
	SDL_SetRelativeMouseMode(active ? SDL_TRUE : SDL_FALSE);
	mouse_accum_dx = 0.0;
	mouse_accum_dy = 0.0;
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
				if (window_manager_can_constrain_aspect_ratio) {
					SDL_GL_GetDrawableSize(screen,
						&real_screen_width, &real_screen_height);
				} else {
					int width, height;

					SDL_GL_GetDrawableSize(screen, &width, &height);
					if (real_screen_width != width || real_screen_height != height) {
						real_screen_width = width;
						real_screen_height = height;
						time_to_set_window_size = 1;
					}
				}
				sng_set_screen_size(real_screen_width, real_screen_height);
			}
			break;
		case SDL_MOUSEBUTTONDOWN:
			if (event.button.button == SDL_BUTTON_RIGHT)
				set_mouse_look(1);
			break;
		case SDL_MOUSEBUTTONUP:
			if (event.button.button == SDL_BUTTON_RIGHT)
				set_mouse_look(0);
			break;
		case SDL_MOUSEMOTION:
			if (mouse_look_active) {
				mouse_accum_dx += event.motion.xrel;
				mouse_accum_dy += event.motion.yrel;
			}
			break;
		}
	}
}

static void enable_sdl_fullscreen_sanity(void)
{
	setenv("SDL_VIDEO_MINIMIZE_ON_FOCUS_LOSS", "0", 0);
}

static void figure_aspect_ratio(SDL_Window *window, int requested_x, int requested_y,
				int *x, int *y)
{
	SDL_GL_GetDrawableSize(window, &real_screen_width, &real_screen_height);
	*x = real_screen_width;
	*y = real_screen_height;
	screen_offset_x = 0;
	screen_offset_y = 0;

	int sw, sh, monitors;
	SDL_Rect bounds;

	monitors = SDL_GetNumVideoDisplays();
	if (monitors < 0)
		return;

	SDL_GetDisplayBounds(0, &bounds);
	sw = bounds.w;
	sh = bounds.h;
	screen_offset_x = bounds.x;
	screen_offset_y = bounds.y;

	if (requested_x <= 0 || requested_y <= 0) {
		*x = sw;
		*y = sh;
		return;
	}
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

static void maybe_resize_window(SDL_Window *window)
{
	if (window_manager_can_constrain_aspect_ratio)
		return;
	if (!time_to_set_window_size)
		return;
	SDL_SetWindowSize(window, real_screen_width, real_screen_height);
	time_to_set_window_size = 0;
}

static void setup_skybox(const char *skybox_prefix)
{
	const char *asset_dir = "share/snis/textures";
	int i;
	char filename[6][PATH_MAX + 1];

	for (i = 0; i < 6; i++)
		snprintf(filename[i], sizeof(filename[i]), "%s/%s%d.png", asset_dir, skybox_prefix, i);

	graph_dev_load_skybox_texture(filename[3], filename[1], filename[4],
					filename[5], filename[0], filename[2]);
}

static void setup_sun_billboard(void)
{
	sun_mesh = mesh_fabricate_billboard(1.0, 1.0); /* unit billboard, scaled to size per frame */
	material_init_sun(&sun_material);
	sun_material.sun.color = sng_get_color(WHITE);
	sun_material.sun.bloom_color = sng_get_color(WHITE);
	sun_material.sun.bloom_intensity = 1.0;
	sun_material.sun.bloom_falloff = 3.0;
}

int main(int argc, char *argv[])
{
	setlocale(LC_ALL, "C");
	program = argc >= 0 ? argv[0] : "shadow_lab";
	enable_sdl_fullscreen_sanity();

	if (SDL_Init(SDL_INIT_VIDEO) < 0) {
		fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
		quit(1);
	}

	uint32_t windowFlags = SDL_WINDOW_RESIZABLE;
	graph_dev_prepare_for_window(&windowFlags);

	screen = SDL_CreateWindow("Shadow Lab", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
			3 * SCREEN_WIDTH / 4, 3 * SCREEN_HEIGHT / 4, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
	if (!screen) {
		fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
		quit(1);
	}

	graph_dev_create_context(screen);

	figure_aspect_ratio(screen, -1, -1, &real_screen_width, &real_screen_height);
	SCREEN_WIDTH = real_screen_width;
	SCREEN_HEIGHT = real_screen_height;
	real_screen_width = (3 * real_screen_width) / 4;
	real_screen_height = (3 * real_screen_height) / 4;
	original_aspect_ratio = (float) real_screen_width / (float) real_screen_height;

	sng_setup_colors(NULL);
	snis_typefaces_init();
	sng_set_font_family(0);
	graph_dev_setup(NULL);
	setup_skybox("orange-haze");
	setup_sun_billboard();

	SDL_SetWindowSize(screen, real_screen_width, real_screen_height);
	window_manager_can_constrain_aspect_ratio =
		(constrain_aspect_ratio_via_xlib(screen, SCREEN_WIDTH, SCREEN_HEIGHT) == 0);
	sng_set_extent_size(SCREEN_WIDTH, SCREEN_HEIGHT);
	sng_set_screen_size(real_screen_width, real_screen_height);
	sng_set_clip_window(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);

	build_scene();

	const double maxTimeBehind = 0.5;
	double delta = 1.0 / (double) FPS;
	unsigned long frame = 0;
	double currentTime = time_now_double();
	double nextTime = currentTime + delta;

	while (1) {
		currentTime = time_now_double();
		if (currentTime - nextTime > maxTimeBehind)
			nextTime = currentTime;

		if (currentTime >= nextTime) {
			nextTime += delta;
			process_events();
			draw_screen();
			if (frame % FPS == 0) {
				graph_dev_reload_changed_textures();
				graph_dev_reload_changed_cubemap_textures();
			}
			if (reload_shaders) {
				graph_dev_reload_all_shaders();
				reload_shaders = 0;
			}
			frame++;
		} else {
			double timeToSleep = nextTime - currentTime;
			if (timeToSleep > 0)
				sleep_double(timeToSleep);
		}
		maybe_resize_window(screen);
	}
	return 0;
}
