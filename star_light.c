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

void star_light_colors(const float star_rgb[3], float ambient,
			float tint_k, float contrast_q,
			float out_light[3], float out_ambient[3])
{
	int i;
	/* blueness: 0 at or below the white point, growing as the star goes blue
	 * (its blue channel exceeds its red channel).  Drives the past-white
	 * contrast term. */
	float blueness = clampf_local(star_rgb[2] - star_rgb[0], 0.0f, 1.0f);
	float contrast = 1.0f - blueness * contrast_q;

	for (i = 0; i < 3; i++) {
		float s = star_rgb[i];
		/* light: white sunlight tinted toward the star colour. */
		out_light[i] = (1.0f - tint_k) * 1.0f + tint_k * s;
		/* ambient: the DARK ambient tinted toward the star's complement
		 * (never toward white), then deepened for blue stars.  The
		 * complement fades to ~black near the white point, so raw-rgb
		 * mixing keeps the tint negligible there and definite only at the
		 * red/blue extremes. */
		float complement = 1.0f - s;
		out_ambient[i] = ((1.0f - tint_k) * ambient + tint_k * complement) * contrast;
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
