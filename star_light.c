/*
	Star-coloured lighting: derive a star-tinted direct-light colour and a
	complementary, star-tinted ambient colour from a star's blackbody RGB.

	See star_light.h for the interface contract.  This module is pure maths with
	no GL/SDL dependencies so it can be unit-tested and shared by the preview
	tool, entity.c, and the game.
*/

#include <math.h>

#include "star_light.h"

static float clampf_local(float x, float lo, float hi)
{
	if (x < lo)
		return lo;
	if (x > hi)
		return hi;
	return x;
}

/* Oklab lightness (Bjorn Ottosson) of a linear-RGB colour.  We match the shadow's lightness in
 * Oklab, not Rec.709 luminance, because Rec.709 badly underweights blue (B weight 0.07), so a
 * red star's blue/cyan complement had to carry a large blue channel to reach a given luminance
 * and read far too bright; Oklab rates blue much lighter (L of pure blue ~0.45), so matching it
 * pulls that blue shadow down to sit level with a warm star's orange shadow. */
static float oklab_lightness(float r, float g, float b)
{
	float l = 0.4122214708f * r + 0.5363325363f * g + 0.0514459929f * b;
	float m = 0.2119034982f * r + 0.6806995451f * g + 0.1073969566f * b;
	float s = 0.0883024619f * r + 0.2817188376f * g + 0.6299787005f * b;

	l = cbrtf(l);
	m = cbrtf(m);
	s = cbrtf(s);
	return 0.2104542553f * l + 0.7936177850f * m - 0.0040720468f * s;
}

void star_light_colors(const float star_rgb[3], float ambient,
			float light_tint, float dark_tint, float shadow_darkening,
			float out_light[3], float out_ambient[3])
{
	int i;
	float mixed[3];
	float mix_l, target_l, scale;
	/* Unconditional: every star's shadows can be darkened, not just blue ones. */
	float darken = 1.0f - clampf_local(shadow_darkening, 0.0f, 1.0f);

	/* Shaded term, in two steps.  First the mix: the dark ambient floor mixed toward
	 * the star's full complement by dark_tint.  This picks the right HUE (a red star
	 * -> cyan shadow, a blue star -> orange shadow) but the wrong LIGHTNESS: the
	 * complement of a saturated star is a bright colour, so a red star gave a luminous
	 * cyan shaded side instead of a dark one. */
	for (i = 0; i < 3; i++) {
		float complement = 1.0f - star_rgb[i];
		mixed[i] = (1.0f - dark_tint) * ambient + dark_tint * complement;
	}

	/* Second, rescale that mix so its Oklab lightness matches the ambient level: keep
	 * the hue, fix the lightness, so the shaded side stays as dark as the neutral
	 * ambient while carrying the complement's colour.  Oklab lightness scales as the
	 * cube root of a uniform RGB scale, so to hit the target lightness the multiplier
	 * is (target_l / mix_l)^3.  A neutral mix (white star or dark_tint = 0) already
	 * sits at the ambient lightness, so this is a no-op there and -- because it scales
	 * luminance, not chroma -- a neutral grey stays neutral and no hue is invented. */
	mix_l = oklab_lightness(mixed[0], mixed[1], mixed[2]);
	target_l = oklab_lightness(ambient, ambient, ambient);
	if (mix_l > 1e-6f) {
		float ratio = target_l / mix_l;
		scale = ratio * ratio * ratio;
	} else {
		scale = 0.0f;
	}

	for (i = 0; i < 3; i++) {
		float s = star_rgb[i];
		/* light: white sunlight tinted toward the star colour. */
		out_light[i] = (1.0f - light_tint) * 1.0f + light_tint * s;
		/* ambient: the hue-preserving, lightness-matched shadow, then darkened.
		 * dark_tint = 0 or a white star leaves the hue neutral; shadow_darkening = 0
		 * leaves the level at vec3(ambient). */
		out_ambient[i] = mixed[i] * scale * darken;
	}
}

void star_light_blackbody_color(float kelvin, float *r, float *g, float *b)
{
	float t = kelvin / 100.0f;
	float rr, gg, bb;

	if (t <= 66.0f) {
		rr = 255.0f;
		gg = 99.4708025861f * logf(t) - 161.1195681661f;
	} else {
		rr = 329.698727446f * powf(t - 60.0f, -0.1332047592f);
		gg = 288.1221695283f * powf(t - 60.0f, -0.0755148492f);
	}
	if (t >= 66.0f)
		bb = 255.0f;
	else if (t <= 19.0f)
		bb = 0.0f;
	else
		bb = 138.5177312231f * logf(t - 10.0f) - 305.0447927307f;
	*r = clampf_local(rr, 0.0f, 255.0f) / 255.0f;
	*g = clampf_local(gg, 0.0f, 255.0f) / 255.0f;
	*b = clampf_local(bb, 0.0f, 255.0f) / 255.0f;
}

void star_light_tinted_blackbody_color(float kelvin, const float tint[3],
					float *r, float *g, float *b)
{
	float c[3], max;
	int i;

	star_light_blackbody_color(kelvin, &c[0], &c[1], &c[2]);
	for (i = 0; i < 3; i++)
		c[i] *= tint ? tint[i] : 1.0;
	max = c[0] > c[1] ? c[0] : c[1];
	if (c[2] > max)
		max = c[2];
	if (max > 0.0) {
		for (i = 0; i < 3; i++)
			c[i] /= max;
	}
	*r = c[0];
	*g = c[1];
	*b = c[2];
}

float star_light_star_brightness(float kelvin)
{
	float t = kelvin / STAR_LIGHT_REFERENCE_KELVIN;

	if (t <= 0.0f)
		return 0.0f;
	return STAR_LIGHT_REFERENCE_BRIGHTNESS * t * t * t * t;
}

float star_light_glow_extent(float brightness, float psf_width, float psf_falloff)
{
	/* The star is brightness / (1 + (s/w)^p), with s in star radii.  Solve for the radius at
	 * which it drops below one 255th of white, which is where it stops mattering to an 8-bit
	 * display.  Never less than the disc itself. */
	const float visible = 1.0f / 255.0f;
	float ratio, extent;

	if (brightness <= visible || psf_falloff <= 0.0f || psf_width <= 0.0f)
		return 1.0f;
	ratio = brightness / visible - 1.0f;
	extent = psf_width * powf(ratio, 1.0f / psf_falloff);
	return extent < 1.0f ? 1.0f : extent;
}
