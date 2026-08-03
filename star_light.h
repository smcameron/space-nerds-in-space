#ifndef STAR_LIGHT_H_
#define STAR_LIGHT_H_
/*
	Star-coloured lighting: derive a star-tinted direct-light colour and a
	complementary, star-tinted ambient colour from a star's blackbody RGB.

	Pure maths, no GL/SDL dependencies, so the same derivation is shared by the
	realtime preview tool, the renderer (entity.c), and later the game.
*/

/*
	Given a star's linear RGB colour (0..1), the scene's scalar ambient level
	`ambient` (the existing dark-grey ambient, applied to all three channels),
	and three adjustable strengths -- `light_tint` (how far the sunlit term leans
	toward the star colour), `dark_tint` (how far the shaded term leans toward the
	star's complement), and `shadow_darkening` (how far the shaded term is darkened,
	0 for not at all) -- compute:

	  out_light[3] = white sunlight tinted toward the star colour.
	  out_ambient[3] = the absolute shaded/ambient colour: the dark ambient
	  mixed toward the star's complement for hue, then rescaled so its Oklab
	  lightness matches `ambient` (so the shadow stays as dark as the neutral
	  ambient rather than taking on the complement's brightness), then deepened
	  for blue stars.

	`shadow_darkening` is unconditional: it applies to every star, not only to blue
	ones.  It used to be scaled by how far past the white point the star sat, which
	made it inert for anything at or below white -- most stars -- and, once star
	colours could be tinted off the blackbody locus, let a purely artistic hue choice
	move the scene's brightness.  Deciding how dark a system's shadows are is now the
	caller's business.

	It multiplies the shaded term AFTER the lightness match, so it darkens without
	disturbing the hue that dark_tint picked.

	With light_tint == 0, dark_tint == 0 and shadow_darkening == 0 this yields
	out_light = {1,1,1} and out_ambient = {ambient, ambient, ambient} -- identical
	to the untinted look.
*/
void star_light_colors(const float star_rgb[3], float ambient,
			float light_tint, float dark_tint, float shadow_darkening,
			float out_light[3], float out_ambient[3]);

/*
	Blackbody colour approximation (Tanner Helland), Kelvin -> RGB 0..1.  Good
	enough to preview star colours from cool red (~2500K) through white to hot
	blue (~30000K).
*/
void star_light_blackbody_color(float kelvin, float *r, float *g, float *b);

/*
	The star's rendered colour: its blackbody colour at `kelvin`, multiplied channel-wise by
	`tint`, renormalized so the brightest channel is 1.

	The blackbody locus runs red -> white -> blue and its green channel always lies between
	red and blue, so it cannot express a magenta, purple or green cast at any temperature.
	Some hand-painted sun textures have exactly that: one shipped star measures green BELOW
	red with blue highest, which is magenta and simply off the locus.  The tint is the escape
	hatch, and it is deliberately a multiplier rather than a hue rotation -- rotating the hue
	of a near-white colour barely moves it, and these casts need a saturation change too.

	Note that temperature and tint are not jointly identifiable: any temperature plus a
	suitable tint yields any colour.  The convention is that the temperature names the nearest
	real blackbody and the tint is the declared artistic deviation from it, which keeps the
	temperature meaningful and the deviation visible.

	Renormalizing means the tint sets hue and saturation only, never brightness -- that stays
	with the core brightness and ambient controls.  A tint of {1,1,1} is exactly
	star_light_blackbody_color().
*/
void star_light_tinted_blackbody_color(float kelvin, const float tint[3],
					float *r, float *g, float *b);

/*
	The star's surface brightness in LINEAR HDR units, from its temperature.

	Stefan-Boltzmann: a blackbody radiates as the fourth power of its temperature, so a
	20000K star is some four hundred times brighter per unit area than a 5800K one.  That
	single fact is what makes a hot star look hot -- rendered in linear HDR and tonemapped,
	it gets a wider saturated white core and a further-reaching glow without any of those
	being separate parameters.  Measured against a simulated HDR render, the apparent white
	core runs from 1.1 disc radii at 2800K to 5.3 at 30000K purely from this.

	STAR_LIGHT_REFERENCE_BRIGHTNESS is the value at STAR_LIGHT_REFERENCE_KELVIN, and it sets
	the overall exposure of every star in the game: raise it and every star grows a bigger
	white core.  It is an aesthetic choice, not a measurement.

	NOTE the scale changed when the star's emission became a single PSF-blurred profile
	normalised to 1 at the centre, instead of a unit disc with a small glow added beside it.
	The brightness now multiplies a profile whose peak is 1 rather than one whose glow term
	peaked around 0.06, so the numbers are some fifty times smaller than they used to be.
	Values fitted from the nine shipped sun textures run from 2 to 27; 30 is the sun-like
	anchor those imply.  A config carrying an old four-digit brightness will render as a
	vastly oversized white ball, which is the intended loud failure rather than a subtle one.
*/
#define STAR_LIGHT_REFERENCE_KELVIN 5800.0
#define STAR_LIGHT_REFERENCE_BRIGHTNESS 30.0
float star_light_star_brightness(float kelvin);

/*
	How far the star's glow remains visible, as a multiple of its disc radius.

	The glow is a power law, so it never truly reaches zero; this returns where it falls below
	one part in 255 and so stops mattering to an 8-bit display.  The caller needs it to size the
	billboard: too small and the glow is clipped off square at the quad's edge, too large and
	the star wastes fill rate.  Closed form, from inverting the falloff.

	psf_width and psf_falloff are the optics constants from struct material_sun; brightness
	is the star's, from the config or star_light_star_brightness().
*/
float star_light_glow_extent(float brightness, float psf_width, float psf_falloff);

#endif
