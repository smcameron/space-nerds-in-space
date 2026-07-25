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
#include "triangle.h"
#include "open-simplex-noise.h"
#include "snis_ship_type.h"
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
enum scene_object_kind { SCENE_SHIP, SCENE_PLANET, SCENE_ASTEROID, SCENE_STARBASE };
struct scene_object {
	enum scene_object_kind kind;
	struct mesh *mesh;
	struct material *material;
	union vec3 pos;
	union quat orientation;
	/* Heading angles, in radians.  orientation is rebuilt from these rather than being
	 * rotated in place; see orientation_from_angles(). */
	float yaw, pitch, roll;
	/* This ship's gun turret: where it sits on the hull, in the model's own (already
	 * scaled) coordinates, and where it is currently aimed.  Every ship carries its own,
	 * so possessing a different ship aims a different turret and the others hold their
	 * aim.  See turret_mount_point(). */
	union vec3 turret_mount;
	float turret_azimuth, turret_elevation;
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
/* The camera's heading, which cam_orientation is rebuilt from each frame.  See
 * orientation_from_angles() for why the orientation is not rotated in place. */
static float cam_yaw, cam_pitch, cam_roll;
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

/* Turret view (F2 while possessing a ship): a faithful copy of the gun turret rig that
 * snis_client's show_weapons_camera_view() sets up, so the CSM can be evaluated at the scale
 * the gunner actually sees.  This is the one view in the game where the camera sits *inside*
 * the shadow-casting geometry, so it stresses the cascade fit differently from every other
 * view.  Everything here mirrors snis_client.c: the mount point, the eye offset above the
 * turret, the near plane and the field of view.  The values are quoted rather than shared
 * because they are #defines private to snis_client.c. */
/* snis_client hardcodes the mount at (-4, 5.45, 0) * SHIP_MESH_SCALE, because the game only
 * ever puts a turret on the player's ship.  Here any ship can be possessed, so derive the
 * mount from the hull's bounding box instead: on the centreline, a third of the way back from
 * the nose, sitting just clear of the top surface.  The two fractions below reproduce the
 * game's authored position on the wombat to the centimetre, and place the turret sensibly on
 * hulls of any size or shape. */
#define TURRET_MOUNT_LENGTHWISE (0.325) /* station along the hull, as a fraction from the tail */
#define TURRET_EYE_LIFT (0.75 * SHIP_MESH_SCALE)  /* view_offset, in the turret's own frame */
#define TURRET_NEAR_PLANE (1.6666 * SHIP_MESH_SCALE) /* NEAR_CAMERA_PLANE * SHIP_MESH_SCALE */
#define TURRET_FOV (45.0 * M_PI / 180.0)          /* ANGLE_OF_VIEW, at zoom 0 */
static int turret_view = 0;    /* 1 = look through the possessed ship's turret */
/* The turret's aim in the ship's local frame, i.e. snis_client's weap_orientation.  The mouse
 * drives this instead of the ship's heading while in turret view, exactly as the weapons
 * station works: the gunner aims, they do not steer.
 *
 * A turret has two degrees of freedom and no roll, so it is held as azimuth and elevation and
 * the quaternion is rebuilt from them.  That also makes its travel limits expressible: the
 * elevation range is the game's, from snis_server.c's turret_move(), which stops the barrel
 * before it can swing down through the hull it is bolted to. */
#define TURRET_ELEVATION_LOWER_LIMIT (-30.0 * M_PI / 180.0) /* snis_server.c turret_move() */
#define TURRET_ELEVATION_UPPER_LIMIT (90.0 * M_PI / 180.0)
static struct mesh *turret_mesh;

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
#define SUN_BILLBOARD_MARGIN 1.1 /* billboard is this much larger than disc + bloom, so both fit with a
				   * small transparent border and the whole falloff is on-screen */
static float sun_bloom_apparent = 0.1; /* bloom reach (where it hits zero) as a fraction of camera distance */
static float sun_temperature = 5800.0; /* Kelvin; sets the star's blackbody colour */

/* Star-coloured lighting (see star_light.c / set_star_light_tint()): tint every lit object's
 * sun-lit term toward the star's colour by sun_light_tint, its shaded/ambient term toward the
 * star's complement by sun_dark_tint, and deepen the shadow for blue stars by sun_shadow_contrast.
 * All three 0 = the old untinted look. */
static float sun_light_tint = 0.45;
static float sun_dark_tint = 0.06;
static float sun_shadow_contrast = 1.25;
static struct mesh *sun_mesh;
static struct material sun_material;

/* Blackbody colour approximation (Tanner Helland), Kelvin -> RGB 0..1.  Good enough to preview
 * star colours from cool red (~2500K) through white to hot blue (~30000K). */
static void blackbody_color(float kelvin, float *r, float *g, float *b)
{
	float t = kelvin / 100.0;
	float rr, gg, bb;

	if (t <= 66.0) {
		rr = 255.0;
		gg = 99.4708025861 * logf(t) - 161.1195681661;
	} else {
		rr = 329.698727446 * powf(t - 60.0, -0.1332047592);
		gg = 288.1221695283 * powf(t - 60.0, -0.0755148492);
	}
	if (t >= 66.0)
		bb = 255.0;
	else if (t <= 19.0)
		bb = 0.0;
	else
		bb = 138.5177312231 * logf(t - 10.0) - 305.0447927307;
	*r = clampf(rr, 0.0, 255.0) / 255.0;
	*g = clampf(gg, 0.0, 255.0) / 255.0;
	*b = clampf(bb, 0.0, 255.0) / 255.0;
}

static void update_sun_color(void)
{
	blackbody_color(sun_temperature, &sun_material.sun.color.red,
			&sun_material.sun.color.green, &sun_material.sun.color.blue);
}

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
static struct mesh *starbase_mesh;
static struct mesh *asteroid_mesh;
static struct material asteroid_material;

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

/* Load a model that may not be present, quietly.  The ship .obj models live in the source
 * tree, but the downloaded art assets (the turret .stl among them) only exist under the
 * user's asset directory, so try the working directory first and fall back to that.  Returns
 * NULL rather than exiting: every caller of this treats the model as optional. */
static struct mesh *read_optional_model(const char *relative_path)
{
	char path[PATH_MAX];
	const char *home;
	struct mesh *m;

	m = read_mesh((char *) relative_path);
	if (m)
		return m;

	home = getenv("HOME");
	if (!home)
		return NULL;
	snprintf(path, sizeof(path), "%s/.local/share/space-nerds-in-space/%s", home,
			relative_path);
	m = read_mesh(path);
	if (!m)
		fprintf(stderr, "shadow_lab: optional model '%s' not found\n", relative_path);
	return m;
}

/* Highest point of the hull directly above (px, pz): drop a vertical ray through the model and
 * keep the highest triangle it hits.  Returns 0 if the ray misses the hull entirely.
 *
 * Sampling nearby vertices instead is not good enough.  On a coarse hull -- the conqueror is
 * 465 vertices for a 70 unit ship -- the triangle actually covering the mount point can have
 * all three of its corners outside any sensible sampling window, so the sampled height comes
 * out below the real surface and the turret ends up buried in the hull. */
static int hull_top_above(struct mesh *hull, float px, float pz, float *top)
{
	int i, found = 0;
	float best = 0.0;

	for (i = 0; i < hull->ntriangles; i++) {
		struct vertex *a = hull->t[i].v[0];
		struct vertex *b = hull->t[i].v[1];
		struct vertex *c = hull->t[i].v[2];
		float l1, l2, l3, y;
		/* Barycentric coordinates of (px, pz) in the triangle seen from above. */
		float d = (b->z - c->z) * (a->x - c->x) + (c->x - b->x) * (a->z - c->z);

		if (fabsf(d) < 1e-9)
			continue; /* edge-on in plan view, so it covers no area to hit */
		l1 = ((b->z - c->z) * (px - c->x) + (c->x - b->x) * (pz - c->z)) / d;
		l2 = ((c->z - a->z) * (px - c->x) + (a->x - c->x) * (pz - c->z)) / d;
		l3 = 1.0 - l1 - l2;
		if (l1 < 0.0 || l2 < 0.0 || l3 < 0.0)
			continue; /* the ray passes outside this triangle */
		y = l1 * a->y + l2 * b->y + l3 * c->y;
		if (!found || y > best) {
			best = y;
			found = 1;
		}
	}
	*top = best;
	return found;
}

/* Work out where a gun turret sits on a hull.
 *
 * The game has nothing to read here: no turret mount appears in ship_types.txt or in any asset
 * file (the .scad_params.h files next to the models are thrust attachment points, for engine
 * flares).  snis_client simply hardcodes the position, in two places with slightly different
 * values, and gets away with it because only the player's ship ever carries a turret.
 *
 * So derive it.  Pick a station a third of the way back from the nose, on the centreline, find
 * the hull's surface directly above that point, and lift by the distance from the turret
 * model's origin to its underside so it rests on the skin rather than sinking into it.  On the
 * wombat this lands within a couple of centimetres of the position the game hardcodes. */
static void turret_mount_point(struct mesh *hull, union vec3 *mount)
{
	float minx, miny, minz, maxx, maxy, maxz;
	float station, top, underside;

	mesh_aabb(hull, &minx, &miny, &minz, &maxx, &maxy, &maxz);
	station = minx + TURRET_MOUNT_LENGTHWISE * (maxx - minx);
	if (!hull_top_above(hull, station, 0.0, &top))
		top = maxy; /* nothing above that point; fall back to the bounding box */

	underside = 0.0;
	if (turret_mesh) {
		float tminx, tminy, tminz, tmaxx, tmaxy, tmaxz;

		mesh_aabb(turret_mesh, &tminx, &tminy, &tminz, &tmaxx, &tmaxy, &tmaxz);
		underside = -tminy;
	}
	vec3_init(mount, station, top + underside, 0.0);
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
	o->yaw = 0.0;
	o->pitch = 0.0;
	o->roll = 0.0;
	o->turret_azimuth = 0.0;
	o->turret_elevation = 0.0;
	vec3_init(&o->turret_mount, 0.0, 0.0, 0.0);
	if (kind == SCENE_SHIP && m)
		turret_mount_point(m, &o->turret_mount);
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
/* The rest of the game's world-scale ladder.  Shadow tuning has to hold across all of it, not
 * just at ship scale, so the scene carries one of each.  Every factor below is quoted from the
 * game so the lab measures what the game renders:
 *
 *   turret          spaceship_turret.stl x SHIP_MESH_SCALE       ->     0.8 units thick
 *   player ship     wombat x SHIP_MESH_SCALE                     ->    15 unit radius
 *   starbase        starbase mesh x STARBASE_SCALE_FACTOR        ->   210 unit radius
 *   asteroid        asteroid.stl x { 3, 10, 20 }                 ->  96 / 319 / 639 radius
 *
 * That is a range of nearly 800:1 between the smallest caster and the largest.  Asteroids are
 * the extreme case: NASTEROID_SCALES (snis.h) is a *count* of three entity scales, applied in
 * snis_client.c as "s ? s * 10 : 3.0" for s in 0..2 -- so the largest asteroid has a radius
 * over 40x the player ship's, and its diameter is a third of the entire shadow coverage
 * distance.  Starbases instead take their factor at mesh load, like ships do. */
#define STARBASE_SCALE_FACTOR (2.0)   /* snis.h */
#define ASTEROID_SCALE_SMALL (3.0)    /* snis_client.c: s ? s * 10 : 3.0, for s = 0, 1, 2 */
#define ASTEROID_SCALE_MEDIUM (10.0)
#define ASTEROID_SCALE_LARGE (20.0)
/* Matches snis_client's init_meshes(), which distorts the asteroid models with this seed. */
#define ASTEROID_NOISE_SEED (77374223LL)
#define ASTEROID_DISTORTION (0.25)

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

/* Asteroids are cubemap-textured in the game rather than uv-mapped, so they need the same
 * six-file cubemap load snis_client does.  The face ordering is snis_client's
 * load_cubemap_textures(): SNIS numbers its cube faces differently from OpenGL, hence the
 * shuffle.  Returns 0 if the textures are missing, which leaves the asteroid untextured. */
static unsigned int load_cubemap(const char *prefix)
{
	const char *asset_dir = "share/snis/textures";
	char filename[6][PATH_MAX + 1];
	int i;

	for (i = 0; i < 6; i++)
		snprintf(filename[i], sizeof(filename[i]), "%s/%s%d.png", asset_dir, prefix, i);

	return graph_dev_load_cubemap_texture(0, 0, filename[1], filename[3], filename[4],
						filename[5], filename[0], filename[2]);
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
		/* Match snis_client's model load (see its init_meshes(): mesh_scale(ship_mesh_map[i],
		 * SHIP_MESH_SCALE * extra_scaling)).  Without this the lab's ships are twice their
		 * in-game size, and since the shadow tunables below are absolute world-unit values,
		 * every shadow-resolution result measured here would be twice as good as the game's.
		 * extra_scaling is per ship type in ship_types.txt and is not modelled here. */
		mesh_scale(ship_mesh[i], SHIP_MESH_SCALE);
		if (ship_mesh[i]->radius > max_radius)
			max_radius = ship_mesh[i]->radius;
	}

	/* The gun turret, mounted on the possessed ship in turret view.  snis_client scales this
	 * by SHIP_MESH_SCALE at load too, and the turret mount point above is expressed in the
	 * same already-scaled world units.  Optional: the turret view still works without it,
	 * just with no close-up caster, so a missing model must not kill the lab. */
	turret_mesh = read_optional_model("share/snis/models/spaceship_turret.stl");
	if (turret_mesh)
		mesh_scale(turret_mesh, SHIP_MESH_SCALE);

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
		/* Game-legal absolute sizes and distances, so what is measured here transfers to
		 * the game.  disc_occlusion_fraction() takes only angular quantities, so the
		 * penumbra depends on the sun's and planet's *angular* radii and their angular
		 * separation -- nothing spatial survives.  The star is snis_client's default
		 * (star_diameter 5625, so radius 2812.5) at a game-typical 100000; the planet is
		 * MIN_PLANET_RADIUS (800, snis.h).  That leaves the ship->planet distance as the
		 * single tuned dial: 3570 puts the penumbra band at roughly the ship cluster's
		 * span, so several ships straddle the terminator at once.  Umbra still forms
		 * easily (planet subtends ~12.8 degrees against the sun's ~1.6).  All are
		 * adjustable live (planet radius/distance 3/4, 5/6; sun distance/radius U/O, G/H). */
		float planet_r = 800.0;
		struct scene_object *p = add_scene_object(SCENE_PLANET, planet_mesh, NULL,
				0.0, -3570.0, 0.0, planet_r, GRAY50);
		if (p) {
			p->color = GRAY50;
			/* The planet must not cast into the CSM depth map: planet->ship shadowing is
			 * handled analytically (in_shade), and a planet-sized hard-edged caster in the
			 * shadow map otherwise sweeps across and blacks out whole ships as the cascades
			 * refit.  This matches the plan's "planets cast never" policy. */
			p->no_cast_shadow = 1;
			planet_index = (int) (p - scene);
			scene_center = p->pos; /* orbit the sun about the planet */
			sun_distance = 100000.0; /* game-typical; SUN_DIST_LIMIT is 30000 (snis.h) */
			sun_radius = 2812.5;     /* = snis_client's default star_diameter (5625) / 2 */
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

	/* A starbase and the three asteroid scales, at their true game sizes, spread far enough
	 * apart not to intersect.  These exist to check that one CSM calibration holds across the
	 * whole size ladder: fly up to each and compare the HUD's cascade texel against the
	 * feature you are looking at.  They are ordinary opaque casters, so they also shadow each
	 * other and the ships.  Positions are absolute, as they are in the game. */
	/* The first entry in the game's starbase_models.txt, and an .obj, so read_obj_file()
	 * builds its texture-mapped material from starbase.mtl and it renders as it does in
	 * game.  (The .stl starbases later in that list carry no material at all.) */
	starbase_mesh = read_optional_model("share/snis/models/starbase/starbase.obj");
	if (starbase_mesh) {
		mesh_scale(starbase_mesh, STARBASE_SCALE_FACTOR);
		add_scene_object(SCENE_STARBASE, starbase_mesh, NULL, 700.0, 0.0, 0.0, 1.0, RED);
	}

	asteroid_mesh = read_optional_model("share/snis/models/asteroid.stl");
	if (asteroid_mesh) {
		struct osn_context *osn;
		struct material *mat;

		/* snis_client distorts the asteroid models at load and re-averages their normals;
		 * do the same so the surface detail that self-shadows is the game's, not a
		 * smooth STL. */
		if (open_simplex_noise(ASTEROID_NOISE_SEED, &osn) == 0) {
			mesh_distort(asteroid_mesh, ASTEROID_DISTORTION, osn);
			open_simplex_noise_free(osn);
		}
		mesh_set_average_vertex_normals(asteroid_mesh);
		/* mesh_scale() and mesh_distort() re-upload the mesh themselves, but
		 * mesh_set_average_vertex_normals() does not, so the smoothed normals have to be
		 * pushed explicitly or the GPU keeps the STL's flat per-face ones and the asteroid
		 * renders faceted.  snis_client's init_meshes() ends the same sequence this way. */
		mesh_graph_dev_init(asteroid_mesh);

		/* The .stl carries no material, so supply the cubemap one the game uses. */
		material_init_texture_cubemap(&asteroid_material);
		asteroid_material.texture_cubemap.texture_id = load_cubemap("asteroid1-");
		mat = asteroid_material.texture_cubemap.texture_id ? &asteroid_material : NULL;

		add_scene_object(SCENE_ASTEROID, asteroid_mesh, mat, -450.0, 0.0, 350.0,
				ASTEROID_SCALE_SMALL, AMBER);
		add_scene_object(SCENE_ASTEROID, asteroid_mesh, mat, 300.0, 0.0, -900.0,
				ASTEROID_SCALE_MEDIUM, AMBER);
		add_scene_object(SCENE_ASTEROID, asteroid_mesh, mat, -1600.0, 0.0, 1400.0,
				ASTEROID_SCALE_LARGE, AMBER);
	}

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
	 * remain adjustable live.  Six cascades split purely logarithmically (lambda 1.0, see
	 * entity.c) hold shadow quality roughly constant with viewing distance, which is what
	 * lets the coverage reach 14000 -- far enough for a starbase or a large asteroid -- for
	 * almost no cost near the camera.  A small blend band hides the cascade seams and fades
	 * the last cascade to lit at the coverage edge. */
	set_shadow_map_max_distance(14000.0);
	set_shadow_map_num_cascades(6);
	set_shadow_map_split_lambda(1.0);
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

/* Build an orientation from heading angles, in the basis camera_basis() uses: yaw about up
 * (+y), then pitch about the yawed right axis (+z), then roll about the nose (+x).
 *
 * Everything that is aimed here keeps its angles as scalars and calls this each frame, rather
 * than rotating a stored quaternion by the frame's mouse delta.  Composing incremental
 * rotations does not commute, so moving the mouse in a circle does not return to where it
 * started -- it leaves a little roll behind every lap, which is what made the horizon tilt.
 * Rebuilding from angles has no history to accumulate error in, and it makes the turret's
 * limits expressible, since they are limits on the angles.  (The representation was already
 * quaternions; it was the integration that drifted, not the quaternions.) */
static void orientation_from_angles(union quat *q, float yaw, float pitch, float roll)
{
	union quat qy, qp, qr;

	quat_init_axis(&qy, 0.0, 1.0, 0.0, yaw);
	quat_init_axis(&qp, 0.0, 0.0, 1.0, pitch);
	quat_init_axis(&qr, 1.0, 0.0, 0.0, roll);
	quat_mul(q, &qy, &qp);
	quat_mul_self(q, &qr);
	quat_normalize_self(q);
}

/* Pitch has to stop short of straight up or down: at exactly +/-90 degrees yaw and roll act on
 * the same axis and the heading becomes ambiguous. */
#define MAX_PITCH (89.0 * M_PI / 180.0)

static float clamp_pitch(float pitch)
{
	if (pitch > MAX_PITCH)
		return MAX_PITCH;
	if (pitch < -MAX_PITCH)
		return -MAX_PITCH;
	return pitch;
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

/* Step the cascade split lambda, snapping to the step grid so repeated presses cannot
 * accumulate floating-point drift.  The step is deliberately fine: in compute_cascade_splits()
 * the uniform term is around 200x the logarithmic one, so lambda spends its whole range being
 * dominated by uniform spacing and only the last stretch below 1.0 does anything interesting
 * -- and 1.0 exactly, a pure logarithmic split, has to be reachable.  Shift steps coarsely to
 * cross the dead zone quickly. */
static void adjust_split_lambda(float delta)
{
	float lambda = get_shadow_map_split_lambda() + delta;

	set_shadow_map_split_lambda(roundf(lambda * 100.0f) / 100.0f);
}

/* Steer from the right-drag mouse and Q/E, shared by the free camera and the possessed ship.
 * Accumulates into the caller's angles and rebuilds the orientation from them. */
static void apply_look_controls(union quat *orientation, float *yaw, float *pitch, float *roll)
{
	const Uint8 *keys = SDL_GetKeyboardState(NULL);

	*yaw -= mouse_accum_dx * mouse_sensitivity;
	*pitch = clamp_pitch(*pitch - mouse_accum_dy * mouse_sensitivity);
	mouse_accum_dx = 0.0;
	mouse_accum_dy = 0.0;

	if (keys[SDL_SCANCODE_Q])
		*roll += roll_rate;
	if (keys[SDL_SCANCODE_E])
		*roll -= roll_rate;

	orientation_from_angles(orientation, *yaw, *pitch, *roll);
}

/* Free-fly camera: look controls plus WASD/RF translation along its own basis. */
static void fly_controls(union vec3 *pos, union quat *orientation, float speed)
{
	const Uint8 *keys = SDL_GetKeyboardState(NULL);
	union vec3 fwd, up, right, step;

	if (keys[SDL_SCANCODE_LSHIFT] || keys[SDL_SCANCODE_RSHIFT])
		speed *= 5.0;

	apply_look_controls(orientation, &cam_yaw, &cam_pitch, &cam_roll);
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
static void fly_ship(struct scene_object *ship, int steer_with_mouse)
{
	const Uint8 *keys = SDL_GetKeyboardState(NULL);
	union vec3 fwd, up, right, step;

	/* Steer the ship by its heading angles.  In turret view the mouse aims the turret
	 * instead, and the caller has already consumed the accumulated motion, so only roll
	 * and throttle apply. */
	if (steer_with_mouse) {
		apply_look_controls(&ship->orientation, &ship->yaw, &ship->pitch, &ship->roll);
	} else {
		if (keys[SDL_SCANCODE_Q])
			ship->roll += roll_rate;
		if (keys[SDL_SCANCODE_E])
			ship->roll -= roll_rate;
		orientation_from_angles(&ship->orientation, ship->yaw, ship->pitch, ship->roll);
	}

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

/* Is the turret rig actually in use?  turret_view is only meaningful while a ship is
 * possessed; the free camera has no turret to look through. */
static int turret_view_active(void)
{
	return turret_view && controlled_slot >= 0 && controlled_slot < ship_slot_count;
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

	if (turret_view) {
		union vec3 mount = ship->turret_mount;
		union vec3 eye = { { 0.0, TURRET_EYE_LIFT, 0.0 } };

		/* Aim the turret with the mouse, in the ship's local frame, then fly the ship
		 * without mouse steering.  Consume the mouse motion here so fly_ship() does not
		 * also apply it.  Elevation is clamped to the turret's real travel, so the barrel
		 * cannot be swung down through the hull. */
		union quat turret_orientation;

		ship->turret_azimuth -= mouse_accum_dx * mouse_sensitivity;
		ship->turret_elevation -= mouse_accum_dy * mouse_sensitivity;
		if (ship->turret_elevation < TURRET_ELEVATION_LOWER_LIMIT)
			ship->turret_elevation = TURRET_ELEVATION_LOWER_LIMIT;
		if (ship->turret_elevation > TURRET_ELEVATION_UPPER_LIMIT)
			ship->turret_elevation = TURRET_ELEVATION_UPPER_LIMIT;
		mouse_accum_dx = 0.0;
		mouse_accum_dy = 0.0;
		orientation_from_angles(&turret_orientation, ship->turret_azimuth,
					ship->turret_elevation, 0.0);
		fly_ship(ship, 0);

		/* Exactly snis_client's show_weapons_camera_view(): the camera orientation is the
		 * ship's composed with the turret's, the mount point is in ship-local coordinates,
		 * and the eye sits a little above the turret in the turret's own frame. */
		quat_mul(&cam_orientation, &ship->orientation, &turret_orientation);
		quat_rot_vec_self(&mount, &ship->orientation);
		quat_rot_vec_self(&eye, &cam_orientation);
		vec3_add(&cam_pos, &ship->pos, &mount);
		vec3_add_self(&cam_pos, &eye);
		chase_initialized = 0; /* re-snap the chase rig when we drop back out */
		return;
	}

	fly_ship(ship, 1);

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
	"  GUN TURRET VIEW (F2 WHILE POSSESSING A SHIP)\n"
	"  - F2               TOGGLE TURRET / CHASE VIEW\n"
	"  - RIGHT-DRAG AIMS THE TURRET; THE SHIP DOES NOT STEER, AS AT THE REAL WEAPONS\n"
	"    STATION.  W/S THROTTLE, Q/E ROLL, ARROW KEYS SWING THE SUN VS YOUR AIM.\n"
	"  - COPIES snis_client's show_weapons_camera_view(): MOUNT, EYE OFFSET, NEAR\n"
	"    PLANE 0.83, 45 DEG FOV.  ONLY VIEW WITH THE CAMERA INSIDE THE CASTER.\n"
	"  - EVERY SHIP CARRIES ITS OWN TURRET AND KEEPS ITS AIM, SO YOU CAN AIM ONE, TAB\n"
	"    TO THE FREE CAMERA AND WATCH ITS SHADOW TRACK THE HULL FROM OUTSIDE.\n"
	"  - CASCADE 0 HUD LINE: TEXEL IS THE FINEST SHADOW THE MAP HOLDS (TURRET IS ~0.8\n"
	"    UNITS THICK).  AIM-TO-SUN MATTERS: THE ORTHO BOX IS FITTED ONLY 1% PAST THE\n"
	"    FRUSTUM'S FAR-FROM-LIGHT CORNER.  16:9 HERE VS 4:3 IN GAME MAKES LAB TEXELS\n"
	"    ~15 PERCENT COARSER; THE HUD SHOWS THIS WINDOW'S TRUE FIGURE.\n\n"
	"  SUN (ANALYTIC PLANET UMBRA / PENUMBRA)\n"
	"  - ARROW KEYS       ORBIT SUN AZIMUTH / ELEVATION (SHIFT = FASTER)\n"
	"  - U / O            SUN DISTANCE CLOSER / FARTHER\n"
	"  - SHIFT+U / SHIFT+O  PAST-WHITE SHADOW CONTRAST WEAKER / STRONGER (DEEPENS BLUE-STAR SHADOWS)\n"
	"  - G / H            SUN RADIUS SMALLER / LARGER (WIDER RADIUS = WIDER PENUMBRA)\n"
	"  - 7 / 8            STAR TEMPERATURE COOLER / HOTTER (BLACKBODY COLOUR)\n"
	"  - SHIFT+7 / SHIFT+8  LIGHT (SUN-LIT) TINT WEAKER / STRONGER (LIT FACES -> STAR COLOUR)\n"
	"  - 9 / I            SUN CORE BRIGHTNESS DIMMER / BRIGHTER (WHITE-HOT SPREAD)\n"
	"  - SHIFT+9 / SHIFT+I  DARK (SHADED) TINT WEAKER / STRONGER (SHADED FACES -> COMPLEMENT)\n"
	"  - T / Y            SUN BLOOM DIMMER / BRIGHTER\n"
	"  - C / V            SUN BLOOM EDGE SOFTER / SHARPER (CURVATURE, RADIUS UNCHANGED)\n"
	"  - Z / X            SUN BLOOM RADIUS SMALLER / LARGER (REACH; DISC STAYS PHYSICAL)\n"
	"  - J                CYCLE SUN DISC EDGE SOFTNESS (LIMB WIDTH; ALPHA FADE, SO SEMI-TRANSPARENT)\n"
	"  - P                CYCLE PLANET SHADE MODE: SOFT / BINARY / OFF\n"
	"  - B                TOGGLE THE PER-SHIP SHADE PANEL\n"
	"  - 3 / 4            PLANET RADIUS SMALLER / LARGER\n"
	"  - 5 / 6            PLANET CLOSER / FARTHER FROM THE SHIP CLUSTER\n"
	"                     (FARTHER + TIGHTER CLUSTER = WIDER, SOFTER PENUMBRA)\n"
	"  - LOWER THE SUN (DOWN ARROW) TO SWING THE PLANET BETWEEN IT AND THE SHIPS.\n"
	"    NOTE: in_shade ONLY DARKENS A SHIP'S SUN-FACING SIDE, WHICH IS HIDDEN\n"
	"    BEHIND THE PLANET IN A DIRECT UMBRA TEST.  WATCH THE PER-SHIP PANEL: SOFT\n"
	"    RAMPS THROUGH THE PENUMBRA, BINARY JUMPS AT 0.5, OFF STAYS 0.\n\n"
	"  SHADOWS\n"
	"  - \\                TOGGLE SHADOWS ON / OFF\n"
	"  - 0 / 1 / 2        DEBUG: OFF / SHADOW-FACTOR / CASCADE-INDEX\n"
	"  - [ / ]            SHADOW COVERAGE DISTANCE DOWN / UP\n"
	"  - SHIFT+[ / SHIFT+]  AMBIENT (SHADED-SIDE) FLOOR DOWN / UP\n"
	"  - - / =            CASCADE COUNT DOWN / UP (1-6)\n"
	"  - ; / '            SPLIT LAMBDA DOWN / UP BY 0.01 (SHIFT = 0.1); 1.0 = PURE LOG\n"
	"  - , / .            DEPTH-BIAS SLOPE DOWN / UP\n"
	"  - SHIFT+, / SHIFT+.  NORMAL-OFFSET BIAS DOWN / UP (TEXELS; KILLS GRAZING ACNE\n"
	"                     WITHOUT DETACHING SHADOWS)\n"
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

/* Recompute what entity.c's compute_cascade_splits() and compute_shadow_light_matrix() will
 * derive for the nearest cascade, so the HUD can report the shadow map's actual world-space
 * resolution.  Duplicated here rather than exported from entity.c to keep this lab tool from
 * adding surface area to the core engine; it must track entity.c if the split scheme changes.
 *
 * split0 is the far edge of cascade 0, radius is its bounding sphere (which sets the ortho
 * window), and texel is that window divided by the shadow map's resolution -- the smallest
 * shadow feature the map can represent.  Compare it against the size of what you expect to
 * cast: the turret is about 0.8 world units thick. */
#define LAB_SHADOW_MAP_TEXTURE_SIZE 4096 /* must match entity.c's ENTITY_SHADOW_MAP_TEXTURE_SIZE */
static void compute_cascade0_stats(float near_d, float fov, float *split0, float *radius,
					float *texel)
{
	float far_d = get_shadow_map_max_distance();
	int n = get_shadow_map_num_cascades();
	float lambda = get_shadow_map_split_lambda();
	float aspect = (float) SCREEN_WIDTH / (float) SCREEN_HEIGHT;
	float p, log_split, uni_split, cz, hn, wn, hf, wf, dn, df;

	if (n < 1)
		n = 1;
	if (far_d <= near_d) {
		*split0 = 0.0;
		*radius = 0.0;
		*texel = 0.0;
		return;
	}
	p = 1.0 / (float) n;
	log_split = near_d * powf(far_d / near_d, p);
	uni_split = near_d + (far_d - near_d) * p;
	*split0 = lambda * log_split + (1.0 - lambda) * uni_split;

	/* Bounding sphere of the frustum slice [near_d, split0].  The centre is the mean of the
	 * eight corners, which for a symmetric frustum lies on the view axis midway between the
	 * two caps, so only the distance to a near and a far corner is needed. */
	hn = tanf(fov * 0.5) * near_d;
	wn = aspect * hn;
	hf = tanf(fov * 0.5) * (*split0);
	wf = aspect * hf;
	cz = 0.5 * (near_d + *split0);
	dn = sqrtf(wn * wn + hn * hn + (near_d - cz) * (near_d - cz));
	df = sqrtf(wf * wf + hf * hf + (*split0 - cz) * (*split0 - cz));
	*radius = dn > df ? dn : df;
	*texel = 2.0 * *radius / (float) LAB_SHADOW_MAP_TEXTURE_SIZE;
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
		snprintf(buffer, sizeof(buffer),
			"CONTROL SHIP %d (TAB)   SPEED %.0f%%   W/S THROTTLE   VIEW %s (F2)",
			controlled_slot,
			ship_max_speed > 0.0 ? 100.0 * ship_speed / ship_max_speed : 0.0,
			turret_view_active() ? "TURRET" : "CHASE");
	sng_abs_xy_draw_string(buffer, TINY_FONT, 10, y); y += dy;
	snprintf(buffer, sizeof(buffer), "SUN AZ %.0f EL %.0f DIST %.0f RADIUS %.0f",
		radians_to_degrees(sun_azimuth), radians_to_degrees(sun_elevation),
		sun_distance, sun_radius);
	sng_abs_xy_draw_string(buffer, TINY_FONT, 10, y); y += dy;
	snprintf(buffer, sizeof(buffer), "SUN %.0fK  CORE %.1f  BLOOM %.1f SHARP %.2f RADIUS %.2f EDGE %.2f",
		sun_temperature, sun_material.sun.core_brightness, sun_material.sun.bloom_brightness,
		sun_material.sun.bloom_falloff, sun_bloom_apparent, sun_material.sun.edge_softness);
	sng_abs_xy_draw_string(buffer, TINY_FONT, 10, y); y += dy;
	snprintf(buffer, sizeof(buffer),
		"STAR-LIGHT  LIGHT-TINT %.2f  DARK-TINT %.2f  CONTRAST %.2f  AMBIENT %.3f",
		sun_light_tint, sun_dark_tint, sun_shadow_contrast, ambient_light);
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
	snprintf(buffer, sizeof(buffer),
		"BIAS slope %.1f  NORMAL-OFFSET %.2f texels   PCF %dx%d   BLEND %.2f",
		bias_factor, graph_dev_get_shadow_normal_offset(),
		2 * graph_dev_get_shadow_pcf_radius() + 1, 2 * graph_dev_get_shadow_pcf_radius() + 1,
		graph_dev_get_shadow_blend());
	sng_abs_xy_draw_string(buffer, TINY_FONT, 10, y); y += dy;

	/* Cascade 0's world-space resolution, and how close the aim is to the sun.  These are the
	 * two numbers that decide whether a small close-up caster registers at all: TEXEL is the
	 * finest shadow feature the map can hold (the turret is ~0.8 units thick), and the ortho
	 * volume is fitted only 1% past the frustum's far-from-light corner, so geometry behind
	 * the camera drops out of the map as AIM-TO-SUN goes small. */
	{
		float split0, radius, texel, near_d, fov, aim_deg;
		union vec3 fwd, up, right, to_sun;

		if (turret_view_active()) {
			near_d = TURRET_NEAR_PLANE;
			fov = TURRET_FOV;
		} else {
			near_d = 1.0;
			fov = FOV;
		}
		compute_cascade0_stats(near_d, fov, &split0, &radius, &texel);
		camera_basis(&cam_orientation, &fwd, &up, &right);
		vec3_sub(&to_sun, &sun_pos, &cam_pos);
		vec3_normalize_self(&to_sun);
		aim_deg = radians_to_degrees(acosf(clampf(vec3_dot(&fwd, &to_sun), -1.0, 1.0)));
		snprintf(buffer, sizeof(buffer),
			"CASCADE 0  FAR %.1f  RADIUS %.1f  TEXEL %.4f   AIM-TO-SUN %.0f DEG",
			split0, radius, texel, aim_deg);
		sng_abs_xy_draw_string(buffer, TINY_FONT, 10, y); y += dy;
	}

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
	float near_plane, far_plane, fov;

	glClearColor(0.0, 0.0, 0.0, 0.0);
	graph_dev_start_frame();

	if (!cx) {
		cx = entity_context_new(MAX_SCENE_OBJECTS + 8, 8);
		set_renderer(cx, FLATSHADING_RENDERER);
	}
	set_ambient_light(cx, ambient_light);
	/* Derive the star-tinted light / complementary ambient from the current star colour and the
	 * live tint/contrast knobs (Shift+7/8 and Shift+9/i).  Uses cx->ambient set just above. */
	set_star_light_tint(cx, sun_material.sun.color.red, sun_material.sun.color.green,
		sun_material.sun.color.blue, sun_light_tint, sun_dark_tint, sun_shadow_contrast);

	update_camera();
	update_sun();

	camera_basis(&cam_orientation, &fwd, &up, &right);
	vec3_add(&at, &cam_pos, &fwd);

	/* In turret view, match the game's weapons-station camera exactly: the near plane feeds
	 * the cascade split scheme, so using the lab's own 1.0 here would measure a frustum the
	 * gunner never sees. */
	if (turret_view_active()) {
		near_plane = TURRET_NEAR_PLANE;
		fov = TURRET_FOV;
	} else {
		near_plane = 1.0;
		fov = FOV;
	}
	/* The far plane must enclose the sun, which is a normal entity at its true distance and is
	 * frustum-culled like everything else -- entity.c exempts nothing, not even planets and
	 * stars.  snis_client simply sets the far plane to the whole universe (FAR_CAMERA_PLANE =
	 * XKNOWN_DIM) and relies on render_entities() splitting the draw into two depth passes to
	 * keep precision usable across that range; do the same here rather than deriving the far
	 * plane from the scene's size, which silently clipped the star once the ships were scaled
	 * down.  This does not affect the shadow cascades: render_shadow_map() clamps its own far
	 * distance to the shadow coverage distance. */
	far_plane = 2.0 * (vec3_dist(&cam_pos, &sun_pos) + sun_radius);
	if (far_plane < scene_scale * 200.0)
		far_plane = scene_scale * 200.0;
	camera_set_parameters(cx, near_plane, far_plane, SCREEN_WIDTH, SCREEN_HEIGHT, fov);
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
		/* Prefer the scene object's own material, then fall back to whatever the model
		 * brought with it.  read_obj_file() builds a texture-mapped material from the .obj's
		 * mtllib, but add_entity() starts every entity with a null material_ptr, so without
		 * this the .obj models render untextured -- which is how the ships and the starbase
		 * had been rendering.  .stl models carry no material and need one supplied. */
		if (o->material)
			update_entity_material(e, o->material);
		else if (o->mesh && o->mesh->material)
			update_entity_material(e, o->mesh->material);
		if (o->no_cast_shadow)
			update_entity_shadow_casting(e, 0);
		/* Everything but the planet receives the analytic planet umbra/penumbra shading
		 * (0.0 lit .. 1.0 umbra), matching snis_client's object_in_shade(); the planet is
		 * lit by its own terminator (surface normal vs. sun) and needs no in-shade term.
		 * Only ships are listed in the HUD shade panel, which is a per-ship readout. */
		if (o->kind != SCENE_PLANET) {
			float frac = compute_planet_shade_fraction(&o->pos);

			entity_set_in_shade(e, frac);
			if (frac > deepest_planet_shade)
				deepest_planet_shade = frac;
			if (o->kind == SCENE_SHIP && ship_shade_count < MAX_SCENE_OBJECTS)
				ship_shade[ship_shade_count++] = frac;
		}
	}

	/* The possessed ship's gun turret.  This is the small close-up caster the turret view
	 * exists to test: about 0.8 world units thick once scaled, against a cascade texel of
	 * roughly 0.13 and a slope-scaled depth bias of comparable size.  Mounted for both the
	 * chase and turret views so it can be inspected from outside and then looked through. */
	for (i = 0; turret_mesh && i < ship_slot_count; i++) {
		struct scene_object *ship = &scene[ship_slots[i]];
		union vec3 mount = ship->turret_mount;
		union vec3 turret_pos;
		union quat turret_local, turret_world;
		struct entity *e;

		quat_rot_vec_self(&mount, &ship->orientation);
		vec3_add(&turret_pos, &ship->pos, &mount);
		orientation_from_angles(&turret_local, ship->turret_azimuth,
					ship->turret_elevation, 0.0);
		quat_mul(&turret_world, &ship->orientation, &turret_local);
		e = add_entity(cx, turret_mesh, turret_pos.v.x, turret_pos.v.y, turret_pos.v.z,
				CYAN); /* snis_client's SHIP_COLOR */
		if (e) {
			update_entity_orientation(e, &turret_world);
			entity_set_in_shade(e, compute_planet_shade_fraction(&ship->pos));
		}
	}

	/* The sun (MATERIAL_SUN): billboard sized to the bloom's screen extent but never smaller
	 * than the disc; the shader's disc radius is set from sun_radius / billboard size so the
	 * disc is world-scale and the bloom is screen-scale.  It must not cast shadows. */
	if (sun_mesh) {
		union vec3 to_cam;
		float cam_dist, billboard_world, bloom_world;
		struct entity *e;

		vec3_sub(&to_cam, &sun_pos, &cam_pos);
		cam_dist = vec3_magnitude(&to_cam);
		/* Size the billboard to just contain the disc plus the bloom's reach, with a small margin.
		 * sun_bloom_apparent is the bloom's reach as a fraction of camera distance (constant on
		 * screen); bloom_world is that reach in world units.  The billboard spans 2 * margin *
		 * (sun_radius + bloom_world), so disc_radius + bloom_radius = 1 / (2 * margin) < 0.5 in UV:
		 * the entire glow is visible and the RADIUS control no longer inflates the billboard without
		 * bound (which used to leave only the bloom's flat centre on screen). */
		bloom_world = sun_bloom_apparent * cam_dist;
		billboard_world = 2.0 * SUN_BILLBOARD_MARGIN * (sun_radius + bloom_world);
		sun_material.sun.disc_radius = sun_radius / billboard_world;
		sun_material.sun.bloom_radius = bloom_world / billboard_world;
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
	case SDLK_F2:
		/* Look through the possessed ship's gun turret.  Meaningless without a ship, so
		 * ignore it in free-camera mode rather than leaving a latched flag behind. */
		if (controlled_slot >= 0)
			turret_view = !turret_view;
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
	case SDLK_LEFTBRACKET: /* Shift = lower the ambient (shaded-side) floor */
		if (keysym->mod & KMOD_SHIFT) {
			ambient_light -= 0.005;
			if (ambient_light < 0.0)
				ambient_light = 0.0;
		} else {
			set_shadow_map_max_distance(get_shadow_map_max_distance() * 0.8);
		}
		break;
	case SDLK_RIGHTBRACKET: /* Shift = raise the ambient (shaded-side) floor */
		if (keysym->mod & KMOD_SHIFT) {
			ambient_light += 0.005;
			if (ambient_light > 1.0)
				ambient_light = 1.0;
		} else {
			set_shadow_map_max_distance(get_shadow_map_max_distance() * 1.25);
		}
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
		adjust_split_lambda((keysym->mod & KMOD_SHIFT) ? -0.1 : -0.01);
		break;
	case SDLK_QUOTE:
		adjust_split_lambda((keysym->mod & KMOD_SHIFT) ? 0.1 : 0.01);
		break;
	case SDLK_COMMA:
		if (keysym->mod & KMOD_SHIFT)
			graph_dev_set_shadow_normal_offset(
				graph_dev_get_shadow_normal_offset() - 0.25);
		else
			adjust_shadow_bias(-0.5, 0.0);
		break;
	case SDLK_PERIOD:
		if (keysym->mod & KMOD_SHIFT)
			graph_dev_set_shadow_normal_offset(
				graph_dev_get_shadow_normal_offset() + 0.25);
		else
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
	case SDLK_u: /* Shift = weaker past-white shadow contrast */
		if (keysym->mod & KMOD_SHIFT) {
			sun_shadow_contrast -= 0.05;
			if (sun_shadow_contrast < 0.0)
				sun_shadow_contrast = 0.0;
		} else {
			sun_distance *= 0.9;
		}
		break;
	case SDLK_o: /* Shift = stronger past-white shadow contrast */
		if (keysym->mod & KMOD_SHIFT) {
			sun_shadow_contrast += 0.05;
			if (sun_shadow_contrast > 3.0)
				sun_shadow_contrast = 3.0;
		} else {
			sun_distance *= 1.111111;
		}
		break;
	case SDLK_g:
		sun_radius *= 0.9;
		if (sun_radius < 1.0)
			sun_radius = 1.0;
		break;
	case SDLK_h:
		sun_radius *= 1.111111;
		break;
	case SDLK_7: /* cooler (redder) star; Shift = weaker light (sun-lit) tint */
		if (keysym->mod & KMOD_SHIFT) {
			sun_light_tint -= 0.01;
			if (sun_light_tint < 0.0)
				sun_light_tint = 0.0;
		} else {
			sun_temperature *= 0.95;
			if (sun_temperature < 1900.0) /* blackbody blue channel cuts off below here */
				sun_temperature = 1900.0;
			update_sun_color();
		}
		break;
	case SDLK_8: /* hotter (bluer) star; Shift = stronger light (sun-lit) tint */
		if (keysym->mod & KMOD_SHIFT) {
			sun_light_tint += 0.01;
			if (sun_light_tint > 1.0)
				sun_light_tint = 1.0;
		} else {
			sun_temperature *= 1.0526316;
			if (sun_temperature > 40000.0)
				sun_temperature = 40000.0;
			update_sun_color();
		}
		break;
	case SDLK_9: /* core dimmer (less white-hot spread); floor at 1.0 so the centre never goes
		      * below the limb (a value < 1 inverts the gradient and darkens the middle).
		      * Shift = weaker dark (shaded) tint */
		if (keysym->mod & KMOD_SHIFT) {
			sun_dark_tint -= 0.01;
			if (sun_dark_tint < 0.0)
				sun_dark_tint = 0.0;
		} else {
			sun_material.sun.core_brightness *= 0.9;
			if (sun_material.sun.core_brightness < 1.0)
				sun_material.sun.core_brightness = 1.0;
		}
		break;
	case SDLK_i: /* core brighter (more white-hot spread); Shift = stronger dark (shaded) tint */
		if (keysym->mod & KMOD_SHIFT) {
			sun_dark_tint += 0.01;
			if (sun_dark_tint > 1.0)
				sun_dark_tint = 1.0;
		} else {
			sun_material.sun.core_brightness *= 1.111111;
			if (sun_material.sun.core_brightness > 100.0)
				sun_material.sun.core_brightness = 100.0;
		}
		break;
	case SDLK_t: /* bloom dimmer (snaps to a true zero so the glow can be turned fully off) */
		sun_material.sun.bloom_brightness *= 0.9;
		if (sun_material.sun.bloom_brightness < 0.05)
			sun_material.sun.bloom_brightness = 0.0;
		break;
	case SDLK_y: /* bloom brighter (recover from a true zero) */
		if (sun_material.sun.bloom_brightness < 0.05)
			sun_material.sun.bloom_brightness = 0.1;
		else
			sun_material.sun.bloom_brightness *= 1.111111;
		break;
	case SDLK_c: /* bloom edge softer (lower k), radius unchanged; floor at 0.5 -- below that even
		      * the smoothstep bloom drops nearly vertically at its outer edge (a hard outer ring) */
		sun_material.sun.bloom_falloff *= 0.9;
		if (sun_material.sun.bloom_falloff < 0.5)
			sun_material.sun.bloom_falloff = 0.5;
		break;
	case SDLK_v: /* bloom edge sharper (higher k) */
		sun_material.sun.bloom_falloff *= 1.111111;
		break;
	case SDLK_z: /* bloom radius (reach) smaller */
		sun_bloom_apparent *= 0.9;
		break;
	case SDLK_x: /* bloom radius (reach) larger; cap it so the billboard cannot inflate past the
		      * point where the opaque disc shrinks to a speck in a screen-filling glow */
		sun_bloom_apparent *= 1.111111;
		if (sun_bloom_apparent > 2.0)
			sun_bloom_apparent = 2.0;
		break;
	case SDLK_j: /* cycle the disc edge softness (limb width); it is the alpha fade at the rim, so
		      * it is semi-transparent by nature -- wrap back to a small value past 0.2 */
		sun_material.sun.edge_softness *= 1.4;
		if (sun_material.sun.edge_softness > 0.2)
			sun_material.sun.edge_softness = 0.01;
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
		if (controlled_slot < 0)
			turret_view = 0; /* the free camera has no turret to look through */
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
	update_sun_color(); /* blackbody colour from sun_temperature */
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
	/* Match the game's tonemapping gain (graph_dev defaults to 1.18; snis_client uses 1.10),
	 * so the star-light tint looks the same here as it will in game. */
	graph_dev_set_tonemapping_gain(1.10);
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
