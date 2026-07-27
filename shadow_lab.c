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
#include "black_hole_lens.h"
#include "quat.h"
#include "material.h"
#include "entity.h"
#include "mesh.h"
#include "stl_parser.h"
#include "mathutils.h"
#include "snis_typeface.h"
#include "snis_cardinal_colors.h"
#include "star_light.h"
#include "opengl_cap.h"
#include "build_info.h"
#include "png_utils.h"
#include "arraysize.h"
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
static int help_page = 0;
static SDL_Window *screen;

/* The scene.  A fixed-capacity list of objects rebuilt into the entity context each frame. */
#define MAX_SCENE_OBJECTS 64
enum scene_object_kind { SCENE_SHIP, SCENE_PLANET, SCENE_ATMOSPHERE, SCENE_ASTEROID,
				SCENE_STARBASE, SCENE_BLACK_HOLE };
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
	/* Entity alpha.  Only the atmosphere shell uses anything but 1.0; snis_client draws it
	 * at 0.5, and MATERIAL_ATMOSPHERE multiplies its own u_Alpha by this. */
	float alpha;
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
static float sun_temperature = 5980.0; /* Kelvin; sets the star's blackbody colour.  A fit of sun.png's
					* glow hue to the blackbody locus lands on 5980K, which is
					* long-standing default was already the texture's colour. */
static float sun_bloom_reach_radii = 0.0; /* last frame's bloom reach in star radii, for the HUD.  This is
					   * the reach expressed the way the TEXTURE expresses it -- a
					   * multiple of the star's radius rather than of the camera
					   * distance -- so it is the number to compare against the
					   * texture's measured 3.73 when matching the two. */

/* Star-coloured lighting (see star_light.c / set_star_light_tint()): tint every lit object's
 * sun-lit term toward the star's colour by sun_light_tint, its shaded/ambient term toward the
 * star's complement by sun_dark_tint, and deepen the shadow for blue stars by sun_shadow_contrast.
 * All three 0 = the old untinted look. */
static float sun_light_tint = 0.45;
static float sun_dark_tint = 0.06;
static float sun_shadow_contrast = 1.25;
static struct mesh *sun_mesh;
static struct material sun_material;

/* The textured-billboard sun -- what snis_client draws today -- kept here alongside the shader
 * sun purely so the two can be compared.  The shader's parameters are meant to REPRODUCE this
 * image, so being able to flip between them in place (or stand them side by side) is what makes
 * the fit checkable rather than a matter of opinion.
 *
 * Built exactly as snis_client's load_per_solarsystem_textures() builds it: texture mapped
 * unlit, spherical billboard, alpha blended.  Sized by snis_client's update_sun_size() rule --
 * 2 * sun_radius * texture_width / sun_texture_disc_pixels world units -- so the star's disc
 * WITHIN the texture comes out 2 * sun_radius across, the same size as the shader's disc.  The
 * rest of the texture is glow, which is why this billboard is so much larger than the star.
 *
 * The two differ in kind, not just in tuning: the texture is entirely world-scale, so its glow
 * grows as you approach, while the shader's bloom is screen-scale and holds a constant apparent
 * size.  They can only agree at one camera distance.
 */
enum sun_style { SUN_STYLE_SHADER, SUN_STYLE_TEXTURE, SUN_STYLE_SIDE_BY_SIDE };
static const char * const sun_style_name[] = { "SHADER", "TEXTURE", "SIDE BY SIDE" };
static int sun_style = SUN_STYLE_SHADER;
static struct material sun_texture_material;
static float sun_texture_width = 512.0;		/* pixels, read from the PNG header */
static float sun_texture_disc_pixels = 96.0;	/* diameter of the disc within the texture, in pixels;
						 * solarsystem_config's star_diameter_pixels default */

/* The black holes, for iterating on the skybox's gravitational lensing (see
 * graph_dev_set_gravitational_lenses() and share/snis/shader/skybox.frag).  Off by default:
 * this is a shadow lab, and extra bodies in the sky would get in the way of the work it
 * already does.  It comes up showing one, which is the arrangement all the tuning below was
 * done against; Shift+` adds the rest.
 *
 * Hole 0 is parked ANTI-SOLAR -- directly away from the sun as seen from the ship cluster.
 * Putting it in front of the sun is the tempting choice and the wrong one: the sun is a 3D
 * entity drawn after the skybox, so it is not lensed at all, and all an eclipse buys you is the
 * sun's bloom washing over the very region where the ring and the arcs live.  Anti-solar puts
 * it against the darkest part of the sky with nothing competing, and it stays there as the
 * arrow keys swing the sun.  The others are spread around that bearing.
 *
 * The radii are game-legal (MIN/MAX_BLACK_HOLE_RADIUS is 500..2000, snis.h) and the distances
 * are an order beyond the planet's 3570, so the apparent sizes are representative of the game
 * rather than of whatever happens to look good here.  lens_strength is the Einstein radius as
 * a multiple of the disc's apparent radius.
 *
 * These values were tuned by eye here and are the ones the game should start from.  They are
 * deliberately restrained: the thing should read as an absence, found by the way the starfield
 * bends around it and by the hole it punches in that field, not by glowing.  At radius 500 and
 * 25000 out the disc is only about 1.2 degrees across, which is small enough that you notice
 * the distortion before you notice the object -- which is the point.  The two ring emissions
 * are turned right down for the same reason; brighter and it starts to look like an ordinary
 * lit object rather than a hole.
 *
 * Why more than one: the shader has MAX_GRAVITATIONAL_LENSES (3) slots, while snis_server makes
 * NBLACK_HOLES (2) per solar system and mission scripts add more on top -- OPERATION_MONKEYWRENCH
 * alone adds two.  A single hole cannot show any of what follows from that, so the layout below
 * is built to make it visible: five holes against three slots means the selection in
 * update_black_holes() visibly binds, and holes 1 and 2 are placed to overlap on screen at
 * mismatched depth, which is the one arrangement where the billboards and the lensing disagree
 * about what is in front of what.  See the note on that in update_black_holes(). */
#define NUM_LAB_BLACK_HOLES 5
static const struct black_hole_layout {
	float azimuth;		/* degrees off the anti-solar bearing, about that bearing's up axis */
	float elevation;	/* degrees above it */
	float radius;
	float distance;
} black_hole_layout[NUM_LAB_BLACK_HOLES] = {
	{   0.0,  0.0,  500.0, 25000.0 }, /* as it has always been: the tuning reference */
	{   8.0,  0.0, 2000.0, 60000.0 }, /* far and large, overlapping ... */
	{   9.0,  0.0,  500.0, 18000.0 }, /* ... this one, near and small, in front of it */
	{ -20.0,  0.0, 1200.0, 35000.0 },
	{   0.0, 15.0,  800.0, 45000.0 },
};
static int black_hole_enabled = 0;
static int black_hole_count = 1; /* how many are live, 1..NUM_LAB_BLACK_HOLES; Shift+` cycles */
/* Per-hole, seeded from the layout.  Only hole 0 is live-tunable (HOME/END): it is the
 * reference, and leaving the rest fixed keeps the deliberate 1/2 overlap steady while you tune. */
static float black_hole_radius[NUM_LAB_BLACK_HOLES];
static float black_hole_distance[NUM_LAB_BLACK_HOLES];
static float black_hole_lens_strength = 1.7;
static float black_hole_swirl = 0.1;
static float black_hole_ring_glow = 0.04; /* Einstein ring's emission, in black_hole.frag */

/* The star field: distant stars, a separate thing from snis_client's space dust.  The dust
 * fills a sphere the camera sits inside and streaks past to sell speed; the field is held at
 * arm's length so it cannot streak, and reads as a sky.  Off by default -- this is a shadow
 * lab, and a sky full of points is one more thing between the eye and the shadows.
 *
 * The radius is the CLOSEST a star may come, so it is really a drift-rate dial: parallax rate
 * is speed over distance, so no star sweeps faster than speed/radius. */
static int nstar_field_stars;
static float star_field_radius = 8750.0;
static int star_field_initialized;
static int black_hole_index[NUM_LAB_BLACK_HOLES]; /* scene[] indices, for position/scale updates */
static int black_hole_lensed[NUM_LAB_BLACK_HOLES]; /* won a lens slot this frame; HUD readout */
static float black_hole_coverage[NUM_LAB_BLACK_HOLES]; /* fraction of the window it distorts */

/* How far out the distortion is worth counting, as a multiple of the Einstein radius.  The
 * deflection is einstein^2 / theta, so it has no outer edge to find: it just thins out forever,
 * and asking where some percentile of it lies has no answer, since the effect integrated over
 * the sky diverges.  A cutoff has to be chosen, so choose it by what can be seen: by three
 * Einstein radii out the sky is displaced by a third of one, which is where the arcs stop being
 * arcs and become a faint smear.  Only the ranking depends on this, and only when a hole is
 * near the edge of the window: a hole fully in view scores as its area either way. */
#define BLACK_HOLE_LENS_EXTENT 3.0
static struct mesh *black_hole_mesh;
static struct material black_hole_material;

/* Which black hole a scene slot is, or -1.  A scan, because there are five of them. */
static int black_hole_slot(int scene_index)
{
	int i;

	for (i = 0; i < NUM_LAB_BLACK_HOLES; i++)
		if (black_hole_index[i] == scene_index)
			return i;
	return -1;
}

static void update_sun_color(void)
{
	star_light_blackbody_color(sun_temperature, &sun_material.sun.color.red,
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
static struct material planet_material;
static struct material atmosphere_material;
/* MATERIAL_ATMOSPHERE takes a POINTER to this, so it has to outlive build_scene(). */
static float atmosphere_brightness = 1.0;

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
	o->alpha = 1.0;
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
		struct material *pmat;
		struct scene_object *p, *atm;

		/* Use the game's textured-planet material, not a bare mesh.  A NULL material falls
		 * through to single_color_lit_shader, which is one of the few shaders that always
		 * honoured the star light tint -- so a grey sphere here responded to the tint while
		 * the game's actual planets, on the cubemap shader, ignored it entirely.  That let
		 * star light tint be tuned against a preview no planet in the game rendered like.
		 * Keep this on the real material so the discrepancy cannot come back. */
		material_init_textured_planet(&planet_material);
		planet_material.textured_planet.texture_id = load_cubemap("planet-texture4-");
		pmat = planet_material.textured_planet.texture_id ? &planet_material : NULL;
		p = add_scene_object(SCENE_PLANET, planet_mesh, pmat,
				0.0, -3570.0, 0.0, planet_r, GRAY50);

		/* The atmosphere shell, built the way snis_client builds it (see the has_atmosphere
		 * branch of update_planet()): a slightly larger sphere at alpha 0.5 carrying
		 * MATERIAL_ATMOSPHERE.  It is here for the same reason the planet now carries its
		 * real material -- the atmosphere is tinted by the star's colour, and a preview
		 * that omits it cannot show a purple star leaving a white halo. */
		material_init_atmosphere(&atmosphere_material);
		atmosphere_material.atmosphere.brightness = &atmosphere_brightness;
		atmosphere_material.atmosphere.brightness_modifier = 1.0;
		atmosphere_material.atmosphere.ring_material = NULL;
		atm = add_scene_object(SCENE_ATMOSPHERE, planet_mesh, &atmosphere_material,
				0.0, -3570.0, 0.0, planet_r * atmosphere_material.atmosphere.scale,
				WHITE);
		if (atm) {
			atm->alpha = 0.5;
			atm->no_cast_shadow = 1;
		}
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

	/* The black holes: camera-facing billboards carrying the procedural event-horizon material
	 * (MATERIAL_BLACK_HOLE). The disc's edge has to sit at a known angle for the lensing to
	 * line up against it -- see black_hole.frag.
	 * Positions and scales are refreshed every frame by update_black_holes(), so the values
	 * here only have to be non-degenerate.  They must not cast into the CSM depth map -- a
	 * caster this size sweeping across would black out the whole scene, the same reason the
	 * planet is excluded.
	 *
	 * All of them share one material and one mesh.  disc_radius is a ratio in the billboard's
	 * UV space rather than an absolute size, so the same value serves every hole whatever its
	 * radius; snis_client will share a single material the same way.  Those radii and the
	 * matching billboard scale are set together by update_black_holes(), since the lens strength
	 * they depend on can be changed while running. */
	for (i = 0; i < NUM_LAB_BLACK_HOLES; i++)
		black_hole_index[i] = -1;
	black_hole_mesh = mesh_fabricate_billboard(1.0, 1.0);
	if (black_hole_mesh) {
		material_init_black_hole(&black_hole_material);
		for (i = 0; i < NUM_LAB_BLACK_HOLES; i++) {
			struct scene_object *bh;

			black_hole_radius[i] = black_hole_layout[i].radius;
			black_hole_distance[i] = black_hole_layout[i].distance;
			bh = add_scene_object(SCENE_BLACK_HOLE, black_hole_mesh,
					&black_hole_material, 0.0, 0.0, 0.0,
					2.0 * black_hole_radius[i], WHITE);
			if (bh) {
				bh->no_cast_shadow = 1;
				black_hole_index[i] = (int) (bh - scene);
			}
		}
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
	graph_dev_set_shadow_bias(2.0, 4.0);
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

/* Park the black holes around the line from the ship cluster out to the sun, so they keep the
 * sun behind them as update_sun() swings the sun around, and hand the best few to the skybox
 * shader.
 *
 * The shader wants angles, not positions: theta_obj is the disc's apparent angular radius, the
 * Einstein radius is a multiple of it (which is what puts the ring outside the disc rather than
 * buried under it), and the disc radius doubles as the shader's floor on theta -- the mapping
 * is singular at dead centre, and the billboard drawn over it is what hides that.
 *
 * There can be more holes than the shader has slots, which is the situation snis_client will be
 * in.  The selection below is the one the client is to use, so that what the lab shows is what
 * the game will do: rank by angular Einstein radius, and drop anything behind the camera.
 *
 * Ranking by angle and not by distance is the whole point.  Radius runs 500..2000, a fourfold
 * spread, so a large far hole can subtend more sky than a small near one and bend four times the
 * solid angle despite being further away -- which is exactly what layout entries 1 and 2 are set
 * up to demonstrate.  Distance ranking picks the wrong one of that pair.
 *
 * Note what this does NOT fix.  The lensing is a distortion of the skybox, so it sits at
 * infinity with no depth of its own, while the billboards are depth-sorted normally.  Where two
 * holes overlap on screen the nearer one's disc correctly covers the farther one's arcs, but the
 * nearer one's ARCS are painted over by the farther one's disc, which is wrong.  Entries 1 and 2
 * overlap deliberately so that this is visible here rather than discovered in the game.  Fixing
 * it means lensing as a depth-aware post-process, which is a different and much larger design. */
static void update_black_holes(void)
{
	struct graph_dev_gravitational_lens lens[MAX_GRAVITATIONAL_LENSES];
	struct black_hole_lens_hole hole[NUM_LAB_BLACK_HOLES];
	struct black_hole_lens_view view;
	float coverage[NUM_LAB_BLACK_HOLES];
	int chosen[MAX_GRAVITATIONAL_LENSES];
	int map[NUM_LAB_BLACK_HOLES]; /* compacted hole -> layout slot, for the HUD */
	union vec3 antisolar, base_up, base_right, anchor;
	float half_size;
	int i, n, nholes = 0;

	for (i = 0; i < NUM_LAB_BLACK_HOLES; i++) {
		black_hole_lensed[i] = 0;
		black_hole_coverage[i] = 0.0;
	}

	/* Anti-solar, anchored on the ship cluster at the origin rather than on scene_center (the
	 * planet, 3570 away): the cluster is where you actually watch this from, so aiming from
	 * there is what keeps the sun squarely behind the camera instead of merely behind the
	 * planet.  See the note by the globals for why anti-solar and not in front of the sun.
	 *
	 * Kept up to date even while the holes are switched off, so that switching them on has
	 * current positions to aim the camera at rather than wherever they were last left. */
	vec3_init(&anchor, 0.0, 0.0, 0.0);
	vec3_sub(&antisolar, &anchor, &sun_pos);
	vec3_normalize_self(&antisolar);
	/* A basis about that bearing, for the layout's azimuth/elevation offsets.  World up is only
	 * a seed for the cross products; if the sun ever swings to straight overhead and makes it
	 * degenerate, any other axis does, so fall back to one that cannot be parallel to it. */
	vec3_init(&base_up, 0.0, 1.0, 0.0);
	if (fabsf(vec3_dot(&antisolar, &base_up)) > 0.999)
		vec3_init(&base_up, 1.0, 0.0, 0.0);
	vec3_cross(&base_right, &antisolar, &base_up);
	vec3_normalize_self(&base_right);
	vec3_cross(&base_up, &base_right, &antisolar);
	vec3_normalize_self(&base_up);

	/* All the holes share one material, so the geometry is worked out once here rather than per
	 * hole.  It has to be redone every frame even so, because the lens strength is a live knob
	 * (SHIFT+PAGEUP / SHIFT+PAGEDOWN) and it is what puts the Einstein ring at its radius. */
	half_size = material_black_hole_set_geometry(&black_hole_material, black_hole_lens_strength);
	black_hole_material.black_hole.glow_brightness = black_hole_ring_glow;

	for (i = 0; i < NUM_LAB_BLACK_HOLES; i++) {
		union quat q;
		union vec3 dir;

		if (black_hole_index[i] < 0)
			continue;
		dir = antisolar;
		quat_init_axis_v(&q, &base_up, degrees_to_radians(black_hole_layout[i].azimuth));
		quat_rot_vec_self(&dir, &q);
		quat_init_axis_v(&q, &base_right, degrees_to_radians(black_hole_layout[i].elevation));
		quat_rot_vec_self(&dir, &q);
		vec3_mul_self(&dir, black_hole_distance[i]);
		vec3_add(&scene[black_hole_index[i]].pos, &anchor, &dir);
		/* The billboard stands off past the outermost ring so the glow has somewhere to go
		 * instead of being clipped at the quad's edge, and the UV radii inside the material
		 * are scaled back by the same factor so the horizon itself stays exactly
		 * black_hole_radius across.  That is what makes the disc's apparent radius equal the
		 * shadow_radius handed to the lens below, which is the whole reason for drawing it
		 * procedurally -- so both come from the one call above and cannot drift apart. */
		scene[black_hole_index[i]].scale = 2.0 * half_size * black_hole_radius[i];
	}

	if (!black_hole_enabled) {
		graph_dev_set_gravitational_lenses(0, NULL);
		return;
	}

	/* The same frustum render_scene() is about to set up, since the whole point of the scoring
	 * is where things land in this window.  Zooming into the turret view narrows the field of
	 * view and so changes which holes are worth a slot. */
	view.cam_pos = cam_pos;
	view.cam_orientation = cam_orientation;
	view.fov = turret_view_active() ? TURRET_FOV : FOV;
	view.screen_width = SCREEN_WIDTH;
	view.screen_height = SCREEN_HEIGHT;

	/* Compact the live holes down, since a slot in the layout can be empty.  The id handed to
	 * the selector is the hole's own index, which is what its swirl has always been derived
	 * from, and map[] carries the index back for the HUD. */
	for (i = 0; i < black_hole_count; i++) {
		if (black_hole_index[i] < 0)
			continue;
		hole[nholes].pos = scene[black_hole_index[i]].pos;
		hole[nholes].radius = black_hole_radius[i];
		hole[nholes].id = i;
		map[nholes] = i;
		nholes++;
	}

	n = black_hole_lens_select(hole, nholes, &view, black_hole_lens_strength, black_hole_swirl,
					lens, chosen, coverage);

	for (i = 0; i < nholes; i++)
		black_hole_coverage[map[i]] = coverage[i];
	for (i = 0; i < n; i++)
		black_hole_lensed[map[chosen[i]]] = 1;
	graph_dev_set_gravitational_lenses(n, n ? lens : NULL);
}

/* Split into chunks purely because a single literal this long is past what C99 requires a
 * compiler to accept (4095 characters).  The chunk boundaries are NOT page breaks: help_walk()
 * counts lines straight through them, so the split point is arbitrary and only the total
 * matters, and a page break can land anywhere. */
static const char * const help_text[] = {
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
	"  - F3 / SHIFT+F3    CYCLE SUN STYLE: SHADER / TEXTURE / SIDE BY SIDE\n"
	"                     THE TEXTURE IS WHAT THE GAME DRAWS TODAY (sun.png ON A PLAIN\n"
	"                     BILLBOARD).  THE SHADER'S SETTINGS ARE MEANT TO REPRODUCE IT,\n"
	"                     SO FLIP BETWEEN THEM IN PLACE TO CHECK THE MATCH -- A FLICKER\n"
	"                     SHOWS UP DIFFERENCES A SIDE-BY-SIDE LETS THE EYE AVERAGE AWAY.\n"
	"                     THEY CAN ONLY AGREE AT ONE CAMERA DISTANCE: THE TEXTURE IS\n"
	"                     ENTIRELY WORLD-SCALE, SO ITS GLOW GROWS AS YOU CLOSE IN, WHILE\n"
	"                     THE SHADER'S BLOOM HOLDS A CONSTANT APPARENT SIZE.  THE TEXTURE\n"
	"                     ALSO HAS STARBURST RAYS THE SHADER HAS NO TERM FOR.\n"
	"  - SHIFT+G / SHIFT+H  SUN TEXTURE DISC WIDTH IN PIXELS SMALLER / LARGER\n"
	"                     (solarsystem_config's star diameter pixels: HOW MUCH OF THE\n"
	"                     TEXTURE IS STAR RATHER THAN GLOW.  RAISING IT SHRINKS THE\n"
	"                     BILLBOARD FOR THE SAME STAR, SO THE TEXTURE'S GLOW SHRINKS TOO.)\n"
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
	"  - F5 / SHIFT+F5    STAR FIELD OFF / ON; SHIFT DOUBLES HOW MANY STARS\n"
	"                     DISTANT STARS, NOT THE GAME'S SPACE DUST -- THE TWO ARE SEPARATE\n"
	"                     AND EITHER MAY BE UP.  DUST FILLS A SPHERE YOU SIT INSIDE AND\n"
	"                     STREAKS PAST TO SELL SPEED; THE FIELD IS HELD FAR ENOUGH OFF THAT\n"
	"                     IT CANNOT STREAK, AND READS AS A SKY.\n"
	"  - F6 / SHIFT+F6    NEAREST STAR CLOSER / FURTHER\n"
	"                     THE HUD'S NEAREST IS A FLOOR ON DISTANCE, NOT A CEILING, AND SO\n"
	"                     IS A CEILING ON DRIFT: PARALLAX RATE IS SPEED OVER DISTANCE, SO NO\n"
	"                     STAR SWEEPS FASTER THAN SPEED/NEAREST HOWEVER LONG YOU FLY AT IT.\n"
	"                     TOO LOW AND THE SKY BECOMES FALLING SNOW.\n"
	"  - 3 / 4            PLANET RADIUS SMALLER / LARGER\n"
	"  - 5 / 6            PLANET CLOSER / FARTHER FROM THE SHIP CLUSTER\n"
	"                     (FARTHER + TIGHTER CLUSTER = WIDER, SOFTER PENUMBRA)\n"
	,
	"  - LOWER THE SUN (DOWN ARROW) TO SWING THE PLANET BETWEEN IT AND THE SHIPS.\n"
	"    NOTE: in_shade ONLY DARKENS A SHIP'S SUN-FACING SIDE, WHICH IS HIDDEN\n"
	"    BEHIND THE PLANET IN A DIRECT UMBRA TEST.  WATCH THE PER-SHIP PANEL: SOFT\n"
	"    RAMPS THROUGH THE PENUMBRA, BINARY JUMPS AT 0.5, OFF STAYS 0.\n\n"
	,
	"  BLACK HOLES (SKYBOX GRAVITATIONAL LENSING)\n"
	"  - `                TOGGLE ON / OFF (AIMS THE FREE CAMERA AT HOLE 0 AS IT COMES ON)\n"
	"  - SHIFT+`          CYCLE HOW MANY ARE UP, 1 TO 5.  THE SHADER HAS ONLY 3 LENS\n"
	"                     SLOTS, SO PAST 3 THE SELECTION BINDS AND THE HUD MARKS EACH\n"
	"                     HOLE LENS OR ---.  A --- HOLE STILL DRAWS ITS DISC AND PHOTON\n"
	"                     RING, IT JUST DOES NOT BEND THE SKY.  HOLES 1 AND 2 OVERLAP ON\n"
	"                     SCREEN AT MISMATCHED DEPTH ON PURPOSE: THE NEAR ONE'S ARCS ARE\n"
	"                     WRONGLY COVERED BY THE FAR ONE'S DISC, BECAUSE THE LENSING IS\n"
	"                     A DISTORTION OF THE SKYBOX AND SO HAS NO DEPTH OF ITS OWN.\n"
	"  - SELECTION RANKS BY HOW MUCH OF THE WINDOW EACH HOLE'S DISTORTION FALLS ON, SHOWN\n"
	"    AS A PERCENTAGE IN THE HUD, SO A HOLE OFF THE EDGE OR HALF OFF IT DOES NOT TAKE\n"
	"    A SLOT FROM THE ONE YOU ARE LOOKING AT.  WITH NOTHING CLIPPED THIS IS THE SAME\n"
	"    ORDER AS THE ANGULAR EINSTEIN RADIUS, WHICH IS NOT THE SAME AS BY DISTANCE:\n"
	"    RADIUS RUNS 500..2000, SO A LARGE FAR HOLE CAN SUBTEND MORE SKY THAN A SMALL\n"
	"    NEAR ONE -- WHICH IS WHAT HOLES 1 AND 2 SHOW.\n"
	"  - WHICH HOLES GET SLOTS IS RANKED BY COVERAGE, BUT THE SLOTS THEMSELVES ARE FILLED\n"
	"    NEAREST FIRST, BECAUSE THE SHADER BENDS THE RAY BY ONE HOLE AT A TIME IN SLOT\n"
	"    ORDER.  SO A FAR HOLE LENSES SKY THE NEAR ONE HAS ALREADY MOVED, RATHER THAN THE\n"
	"    TWO DEFLECTIONS MERELY ADDING.  THIS ONLY SHOWS WHERE TWO HOLES' DISTORTIONS\n"
	"    OVERLAP -- AS 1 AND 2 DO -- AND IT DOES NOT FIX THE DISC-OVER-ARCS NOTE ABOVE,\n"
	"    WHICH IS AN OCCLUSION PROBLEM, NOT AN ORDERING ONE.\n"
	"  - HOME / END       HOLE 0 CLOSER / FARTHER (HOLE 0 ONLY; THE REST STAY PUT SO THE\n"
	"                     1 / 2 OVERLAP HOLDS STILL WHILE YOU TUNE)\n"
	"  - SHIFT+HOME / SHIFT+END  HOLE 0 RADIUS SMALLER / LARGER (CLAMPED TO THE GAME'S\n"
	"                     LEGAL 500..2000)\n"
	"  - / / SHIFT+/      LENS STRENGTH DOWN / UP (EINSTEIN RADIUS AS A MULTIPLE OF THE\n"
	"                     DISC; BELOW 1.0 THE RING HIDES INSIDE THE DISC)\n"
	"  - PGDN / PGUP      FRAME-DRAG SWIRL DOWN / UP (SPIRALS THE ARCS; 0 = NONE)\n"
	"  - INS / DEL        PHOTON RING DIMMER / BRIGHTER (THE DISC'S OWN RIM)\n"
	"  - SHIFT+INS / SHIFT+DEL  PHOTON RING TIGHTER / WIDER\n"
	"  - SHIFT+PGDN / SHIFT+PGUP  EINSTEIN RING DIMMER / BRIGHTER (THE OUTER RING, AT\n"
	"                     THE LENS STRENGTH TIMES THE HORIZON RADIUS -- A DIFFERENT\n"
	"                     RING AT A DIFFERENT RADIUS FROM THE PHOTON RING ABOVE)\n"
	"  - PARKED ANTI-SOLAR, DIRECTLY AWAY FROM THE SUN, SO NOTHING COMPETES WITH THEM:\n"
	"    THE SUN IS NOT LENSED (IT IS AN ENTITY DRAWN AFTER THE SKYBOX), SO PUTTING\n"
	"    IT BEHIND A HOLE WOULD ONLY WASH THE RING OUT WITH BLOOM.\n"
	"  - THE DISC IS A BILLBOARD RUNNING black_hole.frag -- PROCEDURAL, SO ITS EDGE\n"
	"    LANDS AT EXACTLY THE ANGLE THE LENSING FLOORS ITS DEFLECTION AT.  IT DRAWS\n"
	"    BOTH RINGS, SO A HOLE WITH NO LENS SLOT STILL WEARS ITS HALO.  THE ARCS AND\n"
	"    THE INVERTED SECOND IMAGE INSIDE THE RING ARE NOT DRAWN AT ALL: THEY COME\n"
	"    OUT OF ONE TERM IN skybox.frag, AND THEY ARE WHAT A SLOT ACTUALLY BUYS.\n"
	"    HUD SHOWS DISC / RING IN DEGREES: THOSE TRANSFER TO THE GAME, NOT DISTANCE.\n\n"
	/* Split here only to keep each string literal under the 4095 characters ISO C99
	 * guarantees; the chunks are drawn one after another, so this is not a page break. */
	,
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
	"  - n / m            PCF KERNEL SMALLER / LARGER (1x1 TO 7x7, ALL CASCADES)\n"
	"  - k / l            CROSS-CASCADE BLEND BAND SMALLER / LARGER\n\n"
	"  OTHER\n"
	"  - F1               SHOW THIS HELP, THEN PAGE THROUGH IT, THEN CLOSE IT\n"
	"  - F10              RELOAD SHADERS\n"
	"  - F11              TOGGLE FULLSCREEN\n"
	"  - ESC              CLOSE THIS HELP, OR QUIT WHEN IT IS NOT UP\n\n"
	"  CASCADE-INDEX TINT: red=0 (nearest) green=1 blue=2 yellow=3 cyan=4 magenta=5 gray=none\n\n"
	,
};

#define HELP_LINE_HEIGHT 19

/* Walks every line of the help text, counting straight through the chunk boundaries, and
 * returns the total.  When draw is set, the lines in [first, last) are also drawn, starting at
 * y -- so the pass that lays a page out is the same one that counts the lines, and the two
 * cannot disagree about where a page ends. */
static int help_walk(int draw, int first, int last, int y)
{
	unsigned int chunk;
	int line = 0;

	for (chunk = 0; chunk < ARRAYSIZE(help_text); chunk++) {
		const char *text = help_text[chunk];
		char buffer[1024];
		int buflen = 0;
		int i = 0;

		do {
			if (text[i] == '\n' || text[i] == '\0') {
				/* At the terminator, only emit a line if one was actually
				 * accumulated: chunks end with a newline, and drawing the empty
				 * remainder would leave a stray blank line at every boundary. */
				if (text[i] == '\n' || buflen > 0) {
					if (draw && line >= first && line < last) {
						buffer[buflen] = '\0';
						sng_abs_xy_draw_string(buffer, TINY_FONT, 60, y);
						y += HELP_LINE_HEIGHT;
					}
					line++;
				}
				buflen = 0;
				if (text[i] == '\0')
					break;
				i++;
				continue;
			}
			if (buflen < (int) sizeof(buffer) - 1)
				buffer[buflen++] = text[i];
			i++;
		} while (1);
	}
	return line;
}

/* How many lines fit between the top of the text and the footer.  SCREEN_HEIGHT is the fixed
 * drawing extent, not the window -- a resize rescales the output rather than changing the
 * coordinate space -- so this is constant in practice.  It is computed rather than hardcoded so
 * that it stays right if the box inset or the line height is ever changed. */
static int help_lines_per_page(void)
{
	int usable = SCREEN_HEIGHT - 140 - HELP_LINE_HEIGHT; /* box inset, top pad, footer */
	int n = usable / HELP_LINE_HEIGHT;

	return n < 1 ? 1 : n;
}

static int help_page_count(void)
{
	int total = help_walk(0, 0, 0, 0);
	int per = help_lines_per_page();

	return total > 0 ? (total + per - 1) / per : 1;
}

static void draw_help_screen(void)
{
	char buffer[80];
	int per = help_lines_per_page();
	int pages = help_page_count();

	if (help_page >= pages) /* belt and braces; the page count does not change at runtime */
		help_page = pages - 1;

	sng_set_foreground(BLACK);
	sng_current_draw_rectangle(1, 50, 50, SCREEN_WIDTH - 100, SCREEN_HEIGHT - 100);
	sng_set_foreground(GREEN);
	sng_current_draw_rectangle(0, 50, 50, SCREEN_WIDTH - 100, SCREEN_HEIGHT - 100);
	help_walk(1, help_page * per, (help_page + 1) * per, 70);

	snprintf(buffer, sizeof(buffer), "PAGE %d OF %d   F1 %s   ESC CLOSES HELP",
		help_page + 1, pages,
		help_page + 1 < pages ? "NEXT PAGE" : "CLOSES HELP");
	sng_abs_xy_draw_string(buffer, TINY_FONT, 60, SCREEN_HEIGHT - 62);
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

/* The shader sun's billboard spans the world-scale disc plus the screen-scale bloom's reach,
 * with a small margin.  sun_bloom_apparent is that reach as a fraction of the camera distance
 * (so it is constant on screen); bloom_world is the same reach in world units.  Sizing the
 * billboard this way keeps disc_radius + bloom_radius = 1 / (2 * margin) < 0.5 in UV, so the
 * whole falloff is on screen and the RADIUS control cannot inflate the billboard without bound
 * (which used to leave only the bloom's flat centre visible, making SHARP appear to do nothing).
 */
static float shader_sun_billboard_size(float cam_dist)
{
	return 2.0 * SUN_BILLBOARD_MARGIN * (sun_radius + sun_bloom_apparent * cam_dist);
}

/* snis_client's update_sun_size() rule: scale the texture so the disc drawn inside it comes out
 * 2 * sun_radius world units across. */
static float texture_sun_billboard_size(void)
{
	if (sun_texture_disc_pixels < 1.0)
		return 2.0 * sun_radius;
	return 2.0 * sun_radius * sun_texture_width / sun_texture_disc_pixels;
}

static void add_sun_entity(struct entity_context *cx, const union vec3 *pos,
				struct material *m, float billboard_world)
{
	struct entity *e = add_entity(cx, sun_mesh, pos->v.x, pos->v.y, pos->v.z, WHITE);

	if (e) {
		update_entity_scale(e, billboard_world);
		update_entity_material(e, m);
		update_entity_shadow_casting(e, 0); /* a star must not cast into the shadow map */
	}
}

/* Draw the sun in the current style.  SIDE BY SIDE displaces the two billboards perpendicular
 * to the line of sight so both are visible at once -- only the DRAWN positions move, the light
 * still comes from sun_pos, so the scene's lighting and shadows are unaffected.  Flipping
 * between SHADER and TEXTURE in place (the other two styles) is the more sensitive comparison,
 * since a flicker shows up differences that a side-by-side lets the eye average away.
 */
static void draw_sun(struct entity_context *cx, const union vec3 *camera_pos)
{
	union vec3 to_sun, right, offset, pos;
	float cam_dist, shader_size, texture_size, separation;

	if (!sun_mesh)
		return;

	vec3_sub(&to_sun, &sun_pos, camera_pos);
	cam_dist = vec3_magnitude(&to_sun);
	shader_size = shader_sun_billboard_size(cam_dist);
	texture_size = texture_sun_billboard_size();
	sun_material.sun.disc_radius = sun_radius / shader_size;
	sun_material.sun.bloom_radius = (sun_bloom_apparent * cam_dist) / shader_size;
	sun_bloom_reach_radii = (sun_bloom_apparent * cam_dist) / sun_radius;

	if (sun_style != SUN_STYLE_SIDE_BY_SIDE) {
		if (sun_style == SUN_STYLE_TEXTURE)
			add_sun_entity(cx, &sun_pos, &sun_texture_material, texture_size);
		else
			add_sun_entity(cx, &sun_pos, &sun_material, shader_size);
		return;
	}

	/* Half the larger billboard either side of sun_pos, so the two do not overlap whichever
	 * is bigger.  If the sun is straight up the perpendicular is degenerate; fall back to
	 * drawing the shader sun alone rather than stacking them on top of each other. */
	right.v.x = 0.0;
	right.v.y = 1.0;
	right.v.z = 0.0;
	vec3_cross(&offset, &to_sun, &right);
	if (vec3_magnitude(&offset) < 0.0001) {
		add_sun_entity(cx, &sun_pos, &sun_material, shader_size);
		return;
	}
	vec3_normalize_self(&offset);
	separation = 0.55 * (shader_size > texture_size ? shader_size : texture_size);

	vec3_mul(&right, &offset, -separation);
	vec3_add(&pos, &sun_pos, &right);
	add_sun_entity(cx, &pos, &sun_material, shader_size);

	vec3_mul(&right, &offset, separation);
	vec3_add(&pos, &sun_pos, &right);
	add_sun_entity(cx, &pos, &sun_texture_material, texture_size);
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
		"SUN STYLE %s (F3)   TEXTURE %.0fPX  DISC %.0fPX   BLOOM REACH %.2f RADII (TEXTURE IS 3.73)",
		sun_style_name[sun_style], sun_texture_width, sun_texture_disc_pixels,
		sun_bloom_reach_radii);
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
	if (black_hole_index[0] >= 0) {
		/* DISC and RING are the apparent angular sizes, in degrees: they are what the
		 * lensing is actually made of, and the only numbers here that transfer to the
		 * game unchanged, since distance and radius do not survive the projection.
		 * This first line is hole 0, the one HOME/END tune. */
		float dist = vec3_dist(&cam_pos, &scene[black_hole_index[0]].pos);
		float disc = radians_to_degrees(atan2f(black_hole_radius[0], dist > 1.0 ? dist : 1.0));
		int i, len;

		snprintf(buffer, sizeof(buffer),
			"BLACK HOLE %s (`)  R %.0f  DIST %.0f  DISC %.2f DEG  RING %.2f DEG  "
			"STRENGTH %.1f  SWIRL %.1f",
			black_hole_enabled ? "ON" : "OFF", black_hole_radius[0],
			black_hole_distance[0], disc, disc * black_hole_lens_strength,
			black_hole_lens_strength, black_hole_swirl);
		sng_abs_xy_draw_string(buffer, TINY_FONT, 10, y); y += dy;
		snprintf(buffer, sizeof(buffer),
			"BLACK HOLE  PHOTON-RING %.2f WIDTH %.3f  EINSTEIN-RING %.2f  EDGE %.3f",
			black_hole_material.black_hole.ring_brightness,
			black_hole_material.black_hole.ring_width, black_hole_ring_glow,
			black_hole_material.black_hole.edge_softness);
		sng_abs_xy_draw_string(buffer, TINY_FONT, 10, y); y += dy;
		/* One entry per live hole: the Einstein radius in degrees, the percentage of the
		 * window its distortion falls on -- which is what the selection ranks on -- and
		 * whether it won one of the shader's slots.  A hole marked --- still draws its disc
		 * and photon ring, it just does not bend the sky, which is the degradation the game
		 * will show when a mission puts more holes in view than there are slots to lens them
		 * with.  A hole off the edge of the window reads 0% and never takes a slot. */
		len = snprintf(buffer, sizeof(buffer), "BLACK HOLES %d OF %d (SHIFT+`)  %d LENS SLOTS ",
				black_hole_count, NUM_LAB_BLACK_HOLES, MAX_GRAVITATIONAL_LENSES);
		for (i = 0; i < black_hole_count && len > 0 && (size_t) len < sizeof(buffer); i++) {
			float d, deg;

			if (black_hole_index[i] < 0)
				continue;
			d = vec3_dist(&cam_pos, &scene[black_hole_index[i]].pos);
			deg = radians_to_degrees(atan2f(black_hole_radius[i], d > 1.0 ? d : 1.0));
			len += snprintf(buffer + len, sizeof(buffer) - len, " [%d]%.2fDEG %.0f%%%s", i,
					deg * black_hole_lens_strength,
					100.0 * black_hole_coverage[i],
					black_hole_lensed[i] ? "LENS" : "---");
		}
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
	/* The radius is reported as the two faces of the shell it really describes, since a bare
	 * "radius" reads as an outer bound and it is a floor. */
	snprintf(buffer, sizeof(buffer),
		"STAR FIELD %s (F5)  STARS %d  NEAREST %.0f  FURTHEST %.0f (F6)",
		nstar_field_stars ? "ON" : "OFF", nstar_field_stars, star_field_radius,
		3.0 * star_field_radius);
	sng_abs_xy_draw_string(buffer, TINY_FONT, 10, y); y += dy;

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
	update_black_holes(); /* after update_sun(): they are parked around the line out to the sun */

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
		struct entity *e;

		if (o->kind == SCENE_BLACK_HOLE) {
			int slot = black_hole_slot(i);

			if (!black_hole_enabled || slot < 0 || slot >= black_hole_count)
				continue;
		}
		e = add_entity(cx, o->mesh, o->pos.v.x, o->pos.v.y, o->pos.v.z, o->color);
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
		if (o->alpha < 1.0)
			entity_update_alpha(e, o->alpha);
		/* Everything but the planet receives the analytic planet umbra/penumbra shading
		 * (0.0 lit .. 1.0 umbra), matching snis_client's object_in_shade(); the planet is
		 * lit by its own terminator (surface normal vs. sun) and needs no in-shade term.
		 * Only ships are listed in the HUD shade panel, which is a per-ship readout.
		 * The black hole is excluded too: it is an unlit billboard that the shading would
		 * not touch anyway, and letting it into deepest_planet_shade would report a number
		 * about the scenery rather than about the scene. */
		if (o->kind != SCENE_PLANET && o->kind != SCENE_ATMOSPHERE &&
				o->kind != SCENE_BLACK_HOLE) {
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

	draw_sun(cx, &cam_pos);

#if MOVING_STARFIELD
	if (!star_field_initialized) {
		star_field_initialized = 1;
		entity_init_star_field(cx, nstar_field_stars, star_field_radius);
	}
	/* The lab rebuilds its scene every frame, so the field's entity has to be put back --
	 * reusing the mesh, so the stars keep their world positions and the sky holds still. */
	entity_readd_star_field(cx);
#endif

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
		/* F1 opens the help, then pages through it, then closes it -- so repeatedly
		 * pressing the key that brought the help up eventually puts it away again,
		 * without needing a second key to leaf through it. */
		if (!helpmode) {
			helpmode = 1;
			help_page = 0;
		} else if (help_page + 1 < help_page_count()) {
			help_page++;
		} else {
			helpmode = 0;
		}
		break;
	case SDLK_F2:
		/* Look through the possessed ship's gun turret.  Meaningless without a ship, so
		 * ignore it in free-camera mode rather than leaving a latched flag behind. */
		if (controlled_slot >= 0)
			turret_view = !turret_view;
		break;
	case SDLK_F3:
		/* Cycle the sun's rendering style for the A/B: the shader, the game's texture, or
		 * both side by side.  Shift cycles backwards, so a two-way flip between any pair
		 * of adjacent styles is F3 / Shift-F3. */
		if (keysym->mod & KMOD_SHIFT)
			sun_style = (sun_style + ARRAYSIZE(sun_style_name) - 1) %
					ARRAYSIZE(sun_style_name);
		else
			sun_style = (sun_style + 1) % ARRAYSIZE(sun_style_name);
		break;
	case SDLK_F10:
		reload_shaders = 1;
		break;
	case SDLK_F11:
		fullscreen = !fullscreen;
		SDL_SetWindowFullscreen(screen, fullscreen * SDL_WINDOW_FULLSCREEN_DESKTOP);
		break;
	case SDLK_ESCAPE:
		/* Esc dismisses the help if it is up, and only quits otherwise.  Reaching for Esc
		 * to close the help is the reflex, and having that drop the whole lab -- losing
		 * every live tweak with it -- is a rotten trade for one keystroke. */
		if (helpmode)
			helpmode = 0;
		else
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
	case SDLK_BACKQUOTE:
		/* Shift cycles how many are up, 1..NUM_LAB_BLACK_HOLES, leaving the on/off state
		 * alone.  Five against three lens slots is what makes the selection bind, and the
		 * overlapping pair only exists once you are past two. */
		if (keysym->mod & KMOD_SHIFT) {
			black_hole_count = black_hole_count % NUM_LAB_BLACK_HOLES + 1;
			break;
		}
		black_hole_enabled = !black_hole_enabled;
		/* Swing the free camera onto hole 0 as they come on, or you switch them on and see
		 * nothing: they are parked out along the sun's bearing, which is nowhere near where
		 * the camera starts out looking.  Aim by the angle scalars rather than by
		 * cam_orientation, because apply_look_controls() rebuilds the orientation from
		 * them every frame and would throw away anything written to the quaternion.
		 * Leave a possessed ship's camera alone -- it is rigged to the ship. */
		if (black_hole_enabled && black_hole_index[0] >= 0 && controlled_slot < 0) {
			union vec3 d;

			vec3_sub(&d, &scene[black_hole_index[0]].pos, &cam_pos);
			vec3_normalize_self(&d);
			/* Inverse of orientation_from_angles() acting on the +x base heading:
			 * fwd = (cos(pitch)cos(yaw), sin(pitch), -cos(pitch)sin(yaw)). */
			cam_pitch = clamp_pitch(asinf(d.v.y));
			cam_yaw = atan2f(-d.v.z, d.v.x);
			cam_roll = 0.0;
		}
		break;
	/* HOME/END tune hole 0 only.  It is the reference the values above were set against, and
	 * leaving the others fixed keeps the deliberate 1/2 overlap steady while you move it. */
	case SDLK_HOME: /* closer, so it looms larger; Shift = smaller hole */
		if (keysym->mod & KMOD_SHIFT) {
			black_hole_radius[0] *= 0.9;
			/* MIN_BLACK_HOLE_RADIUS (snis.h): below this is not a size the game can
			 * actually produce, so tuning against it would not transfer. */
			if (black_hole_radius[0] < 500.0)
				black_hole_radius[0] = 500.0;
		} else {
			black_hole_distance[0] *= 0.9;
			if (black_hole_distance[0] < 2.0 * black_hole_radius[0])
				black_hole_distance[0] = 2.0 * black_hole_radius[0];
		}
		break;
	case SDLK_END: /* farther; Shift = larger hole (MAX_BLACK_HOLE_RADIUS) */
		if (keysym->mod & KMOD_SHIFT) {
			black_hole_radius[0] *= 1.111111;
			if (black_hole_radius[0] > 2000.0)
				black_hole_radius[0] = 2000.0;
		} else {
			black_hole_distance[0] *= 1.111111;
		}
		break;
	case SDLK_INSERT: /* photon ring (the disc's own rim) dimmer; Shift = tighter to the disc */
		if (keysym->mod & KMOD_SHIFT) {
			black_hole_material.black_hole.ring_width *= 0.9;
			if (black_hole_material.black_hole.ring_width < 0.005)
				black_hole_material.black_hole.ring_width = 0.005;
		} else {
			black_hole_material.black_hole.ring_brightness *= 0.9;
			if (black_hole_material.black_hole.ring_brightness < 0.02)
				black_hole_material.black_hole.ring_brightness = 0.0;
		}
		break;
	case SDLK_DELETE: /* photon ring brighter (recovers from a true zero); Shift = wider */
		if (keysym->mod & KMOD_SHIFT) {
			black_hole_material.black_hole.ring_width *= 1.111111;
			if (black_hole_material.black_hole.ring_width > 0.5)
				black_hole_material.black_hole.ring_width = 0.5;
		} else {
			if (black_hole_material.black_hole.ring_brightness < 0.02)
				black_hole_material.black_hole.ring_brightness = 0.02;
			else
				black_hole_material.black_hole.ring_brightness *= 1.111111;
			if (black_hole_material.black_hole.ring_brightness > 8.0)
				black_hole_material.black_hole.ring_brightness = 8.0;
		}
		break;
	case SDLK_SLASH: /* Einstein radius as a multiple of the disc; Shift = larger */
		if (keysym->mod & KMOD_SHIFT) {
			black_hole_lens_strength += 0.1;
			if (black_hole_lens_strength > 6.0)
				black_hole_lens_strength = 6.0;
		} else {
			black_hole_lens_strength -= 0.1;
			/* Below 1.0 the ring falls inside the disc and is simply not visible. */
			if (black_hole_lens_strength < 0.0)
				black_hole_lens_strength = 0.0;
		}
		break;
	case SDLK_PAGEDOWN: /* frame dragging: negative and positive swirl are mirror images.
			     * Shift = dimmer Einstein ring (the outer one, drawn on the billboard;
			     * distinct from the disc's own rim on INSERT/DELETE) */
		if (keysym->mod & KMOD_SHIFT) {
			black_hole_ring_glow *= 0.9;
			if (black_hole_ring_glow < 0.01)
				black_hole_ring_glow = 0.0;
		} else {
			black_hole_swirl -= 0.1;
			if (black_hole_swirl < -4.0)
				black_hole_swirl = -4.0;
		}
		break;
	case SDLK_PAGEUP: /* Shift = brighter Einstein ring (recovers from a true zero) */
		if (keysym->mod & KMOD_SHIFT) {
			if (black_hole_ring_glow < 0.01)
				black_hole_ring_glow = 0.01;
			else
				black_hole_ring_glow *= 1.111111;
			if (black_hole_ring_glow > 4.0)
				black_hole_ring_glow = 4.0;
		} else {
			black_hole_swirl += 0.1;
			if (black_hole_swirl > 4.0)
				black_hole_swirl = 4.0;
		}
		break;
	case SDLK_g: /* Shift = the disc within the sun texture is fewer pixels across */
		if (keysym->mod & KMOD_SHIFT) {
			sun_texture_disc_pixels -= 4.0;
			if (sun_texture_disc_pixels < 4.0)
				sun_texture_disc_pixels = 4.0;
		} else {
			sun_radius *= 0.9;
			if (sun_radius < 1.0)
				sun_radius = 1.0;
		}
		break;
	case SDLK_h: /* Shift = the disc within the sun texture is more pixels across */
		if (keysym->mod & KMOD_SHIFT) {
			/* Larger disc = smaller billboard for the same star, so the texture's glow
			 * shrinks with it.  This is the dial that decides how much of the texture is
			 * star and how much is glow, and getting it wrong is what made the textured
			 * sun's bloom look enormous. */
			sun_texture_disc_pixels += 4.0;
			if (sun_texture_disc_pixels > sun_texture_width)
				sun_texture_disc_pixels = sun_texture_width;
		} else {
			sun_radius *= 1.111111;
		}
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
	case SDLK_F5: /* star field off / on; Shift = twice as many.  A function key because every
		       * letter is taken: WASD/QE/RF are continuous movement, polled through
		       * SDL_GetKeyboardState() rather than appearing in this switch. */
		if (keysym->mod & KMOD_SHIFT) {
			nstar_field_stars = nstar_field_stars ? nstar_field_stars * 2 : 1000;
			if (nstar_field_stars > 64000)
				nstar_field_stars = 64000;
		} else {
			nstar_field_stars = nstar_field_stars ? 0 : 32000;
		}
		star_field_initialized = 0;
		break;
	case SDLK_F6: /* nearest star closer / further (Shift).  Sets how fast the field can drift,
		       * which is what decides whether it reads as a sky or as falling snow. */
		if (keysym->mod & KMOD_SHIFT)
			star_field_radius *= 1.25;
		else
			star_field_radius *= 0.8;
		if (star_field_radius < 200.0)
			star_field_radius = 200.0;
		if (star_field_radius > 1000000.0)
			star_field_radius = 1000000.0;
		star_field_initialized = 0;
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
	const char *sun_texture = "share/snis/textures/sun.png";
	char whynot[256];
	char *pixels;
	int w = 0, h = 0, hasalpha = 0;

	sun_mesh = mesh_fabricate_billboard(1.0, 1.0); /* unit billboard, scaled to size per frame */
	material_init_sun(&sun_material);
	update_sun_color(); /* blackbody colour from sun_temperature */

	/* Start from the least-squares fit of the glow term to sun.png's measured radial profile
	 * (doc/star-rendering-and-lighting-notes.txt section 5), so the A/B opens on the candidate
	 * match rather than on material.c's generic defaults.  These are a numerical fit over area;
	 * the eye weights the bright limb far more heavily, so they are a starting point to be
	 * confirmed by flipping F3, not a verdict. */
	sun_material.sun.bloom_brightness = 0.62;
	sun_material.sun.bloom_falloff = 1.32;

	/* The game's textured sun, for the A/B comparison. */
	material_init_texture_mapped_unlit(&sun_texture_material);
	sun_texture_material.billboard_type = MATERIAL_BILLBOARD_TYPE_SPHERICAL;
	sun_texture_material.texture_mapped_unlit.texture_id = graph_dev_load_texture(sun_texture, 0);
	sun_texture_material.texture_mapped_unlit.do_blend = 1;

	/* The billboard's size depends on the texture's width, so read it from the PNG header
	 * rather than assuming 512, exactly as snis_client's update_sun_size() does. */
	pixels = png_utils_read_png_image(sun_texture, 0, 0, 0, &w, &h, &hasalpha,
						whynot, sizeof(whynot));
	if (pixels) {
		free(pixels);
		sun_texture_width = w;
	} else {
		fprintf(stderr, "shadow_lab: failed to size sun texture %s: %s\n", sun_texture, whynot);
	}
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
