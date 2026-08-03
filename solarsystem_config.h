#ifndef SOLARSYSTEM_CONFIG__
#define SOLARSYSTEM_CONFIG__
/*
	Copyright (C) 2015 Stephen M. Cameron
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

#define PLANET_TYPE_COUNT_SHALL_BE 6

/* The traditional width of the sun billboard, in world units.  Every solar system has always
 * drawn its star on a quad this size: 5625 world units of star spread over 96 of a 512 pixel
 * texture works out at exactly 30000.  It is the invariant the star's size is inferred from
 * when a config does not state one -- see solarsystem_star_diameter(). */
#define SOLARSYSTEM_SUN_BILLBOARD_SIZE 30000.0

struct atmosphere_color_rgb {
	unsigned char r, g, b;
};

struct solarsystem_asset_spec {
	char *sun_texture;
	char *skybox_prefix;
	char **planet_texture;
	char **planet_normalmap;
	char **planet_type;
	float *atmosphere_brightness; /* array of atmosphere brightnesses for planets between 0 and 1 */
	struct atmosphere_color_rgb *atmosphere_color;
	struct atmosphere_color_rgb *water_color; /* for use in specular calculations */
	double x, y, z; /* Location of solarsystem (separate coord sys from rest of objects) */
	int nplanet_textures;
	int spec_errors, spec_warnings;
	int random_seed;
	float star_diameter_pixels; /* diameter of the star's visible disc within the sun texture, in pixels */
	float star_diameter; /* diameter of the star in world units (e.g. for planet shadow calculations) */
	int star_diameter_specified; /* 0 means star_diameter was inferred, not stated; see below */

	/* How the star itself is drawn.  SOLARSYSTEM_SUN_STYLE_TEXTURE is the original: sun_texture
	 * on a plain billboard, with everything about the star's appearance baked into the image.
	 * SOLARSYSTEM_SUN_STYLE_SHADER draws it procedurally instead (MATERIAL_SUN, see
	 * share/snis/shader/sun.frag), which is what the parameters below feed.  The shader's
	 * defaults are fitted to the shipped sun.png, so the two look alike -- see section 5 of
	 * doc/star-rendering-and-lighting-notes.txt. */
	int sun_style;
	float star_temperature;		/* Kelvin; blackbody colour of the star's disc and of its light */
	/* Per-channel multiplier on that blackbody colour, renormalized so it changes hue and
	 * saturation but not brightness.  Defaults to {1,1,1}, which is no change at all.
	 *
	 * It exists because the blackbody locus cannot express a magenta, purple or green cast --
	 * its green channel always lies between red and blue -- and some hand-painted sun textures
	 * have one.  Applies to the star's disc and to the light it casts alike, so a purple star
	 * lights the scene purple.  See star_light_tinted_blackbody_color(). */
	float star_tint[3];
	float sun_edge_softness;	/* limb width, as a fraction of the disc radius */
	/* The star's surface brightness in linear HDR units, which sets the size of its white-hot
	 * core and the reach of its glow.  When unspecified it follows the temperature by
	 * Stefan-Boltzmann (T^4), which is the physical answer and right for a star nobody has
	 * art-directed.
	 *
	 * It is separable from the temperature because the temperature also sets the star's COLOUR,
	 * and one number cannot serve both: choosing a temperature to match a texture's hue fixes
	 * a brightness you may not want, and choosing it for brightness breaks the hue -- which is
	 * then patched with star_tint, which fights back.  The shipped textures are hand-painted
	 * and their glow sizes do not follow T^4 at all: one wants nearly 300 times the brightness
	 * its temperature implies. */
	float star_brightness;
	int star_brightness_specified; /* 0 means follow the temperature */

	/* Star-coloured lighting (star_light.c).  All three at 0 reproduces the untinted look. */
	float star_light_tint;		/* how far lit surfaces lean toward the star's colour */
	float star_dark_tint;		/* how far shaded surfaces lean toward its complement */
	/* How far the shaded side is darkened, 0 for not at all.  Unconditional -- it applies to
	 * every star, unlike its predecessor, which only bit on stars past the white point and so
	 * was inert for most of them.  A plain number a system sets for whatever reason it likes:
	 * nothing infers it from the star's colour or temperature any more. */
	float star_shadow_darkening;

	/* How many of the star rendering keys above the file actually set.  Zero means every
	 * value here is a default, which matters because the defaults are fitted to one
	 * particular sun texture: a system that says nothing gets that star's parameters
	 * regardless of what its own sun texture looks like.  Tools can say so rather than
	 * leaving someone to wonder why every star came out identical. */
	int star_keys_specified;
};

#define SOLARSYSTEM_SUN_STYLE_TEXTURE 0
#define SOLARSYSTEM_SUN_STYLE_SHADER 1

/* The star's diameter in world units.  A config that states one is believed.  None of the
 * shipped ones do, so in practice this infers it from the sun texture, holding the billboard at
 * SOLARSYSTEM_SUN_BILLBOARD_SIZE: the disc covers star_diameter_pixels of the texture's width,
 * so the star is that same fraction of the billboard.
 *
 * Measuring a texture's disc therefore states the star's size as a side effect, and a config
 * that only measures its own texture keeps looking exactly as it always has.  It reproduces the
 * historical default exactly -- 96 px of a 512 px texture is 5625 units -- which is what makes
 * this safe to apply to solar systems written long before the key existed.
 *
 * texture_width is the sun texture's width in pixels; pass 0 if it is not known, and the stated
 * or default diameter is returned unchanged.
 */
float solarsystem_star_diameter(const struct solarsystem_asset_spec *a, float texture_width);

struct solarsystem_asset_spec *solarsystem_asset_spec_read(char *filename);
void solarsystem_asset_spec_free(struct solarsystem_asset_spec *s);

#endif
