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
	star's complement), and `contrast_q` (how much a past-white / blue star deepens
	the shadows) -- compute:

	  out_light[3] = white sunlight tinted toward the star colour.
	  out_ambient[3] = the absolute shaded/ambient colour: the dark ambient
	  mixed toward the star's complement for hue, then rescaled so its Oklab
	  lightness matches `ambient` (so the shadow stays as dark as the neutral
	  ambient rather than taking on the complement's brightness), then deepened
	  for blue stars.

	With light_tint == 0, dark_tint == 0 and contrast_q == 0 this yields
	out_light = {1,1,1} and out_ambient = {ambient, ambient, ambient} -- identical
	to the untinted look.
*/
void star_light_colors(const float star_rgb[3], float ambient,
			float light_tint, float dark_tint, float contrast_q,
			float out_light[3], float out_ambient[3]);

/*
	Blackbody colour approximation (Tanner Helland), Kelvin -> RGB 0..1.  Good
	enough to preview star colours from cool red (~2500K) through white to hot
	blue (~30000K).
*/
void star_light_blackbody_color(float kelvin, float *r, float *g, float *b);

#endif
