/* GLES (GLSL 100) version of share/snis/shader/wreck.shader: keep the two in step.
 *
 * A piece of a broken ship: its hull outside, and its interior seen through the tear.
 *
 * See struct material_wreck in material.h and mesh_fracture.h.  A piece is an open shell of the
 * hull it came from, drawn two-sided: a front face is hull, textured and lit as a ship is; a back
 * face -- only ever seen through the torn edge, from outside the shell -- is the interior, dark
 * and lit from its own side, with its normal turned round to face the viewer.
 *
 * ALONG THE TEAR.  a_Edge is each vertex's distance to the torn edge, in hull radii.  Within
 * u_Scorch of it the hull is sooted -- a band made ragged by noise, so it reads as burning and
 * not as a painted stripe -- and within u_EdgeWidth the metal glows at u_EdgeTemp, through the
 * same blackbody ramp as the fireball, as the caller lets it cool.
 *
 * Lit by the star, lambertian as the hulls are, and by a second light (u_AuxLight*), because the
 * fire it came out of lights it.
 *
 * Needs filmic.glsl in front of it.
 */

#define BLACKBODY_MIN_K 1900.0
#define BLACKBODY_MAX_K 40000.0

#if defined(INCLUDE_VS)

uniform mat4 u_MVPMatrix;
uniform mat4 u_ModelMatrix;

attribute vec3 a_Position;
attribute vec3 a_Normal;
attribute vec2 a_TexCoord;
attribute float a_Edge;

varying vec3 v_World;
varying vec3 v_Model;
varying vec3 v_Normal;
varying vec2 v_TexCoord;
varying float v_Edge;

void main()
{
	v_World = (u_ModelMatrix * vec4(a_Position, 1.0)).xyz;
	v_Model = a_Position;
	/* Uniform scale only, so the normal survives the model matrix once renormalised. */
	v_Normal = normalize((u_ModelMatrix * vec4(a_Normal, 0.0)).xyz);
	v_TexCoord = a_TexCoord;
	v_Edge = a_Edge;
	gl_Position = u_MVPMatrix * vec4(a_Position, 1.0);
}

#endif

#if defined(INCLUDE_FS)

uniform sampler2D u_Albedo;
uniform int u_HaveTexture;
uniform sampler2D u_Blackbody;
uniform vec3 u_LightPos;
uniform vec3 u_StarTint;
uniform float u_Ambient;
uniform vec3 u_AuxLightPos;
uniform vec3 u_AuxLightColor;
uniform float u_AuxLightWrap;

uniform float u_Interior;	/* albedo of the inside of the hull */
uniform float u_Scorch;		/* width of the sooted band, hull radii */
uniform float u_EdgeWidth;	/* width of the glowing edge, hull radii */
uniform float u_EdgeTemp;	/* kelvin */
uniform float u_EdgeBrightness;
uniform float u_HullRadius;	/* world units, to put the scorch noise in hull radii */
uniform float u_Dissolve;	/* how far in from the torn edge the piece is eaten, hull radii */
uniform float u_BurnGlow;	/* how hot the burning front runs, 0 to 1 */
uniform float u_Preheat;	/* width of the dull red zone ahead of the front, hull radii */
uniform float u_Time;		/* seconds, for the front's flicker */

varying vec3 v_World;
varying vec3 v_Model;
varying vec3 v_Normal;
varying vec2 v_TexCoord;
varying float v_Edge;


/* Dave Hoskins' hash without sine, as in explosion.shader. */
float hash13(vec3 p)
{
	p = fract(p * 0.1031);
	p += dot(p, p.zyx + 31.32);
	return fract((p.x + p.y) * p.z);
}

float value_noise(vec3 p)
{
	vec3 i = floor(p);
	vec3 f = fract(p);
	vec3 u = f * f * (3.0 - 2.0 * f);

	return mix(mix(mix(hash13(i), hash13(i + vec3(1.0, 0.0, 0.0)), u.x),
			mix(hash13(i + vec3(0.0, 1.0, 0.0)), hash13(i + vec3(1.0, 1.0, 0.0)), u.x),
			u.y),
		mix(mix(hash13(i + vec3(0.0, 0.0, 1.0)), hash13(i + vec3(1.0, 0.0, 1.0)), u.x),
			mix(hash13(i + vec3(0.0, 1.0, 1.0)), hash13(i + vec3(1.0, 1.0, 1.0)), u.x),
			u.y),
		u.z);
}

/* As in explosion.shader: the ramp stops at 1900K, and below it the glow only reddens. */
vec3 blackbody(float kelvin)
{
	float u = (kelvin - BLACKBODY_MIN_K) / (BLACKBODY_MAX_K - BLACKBODY_MIN_K);
	vec3 c = texture2D(u_Blackbody, vec2(clamp(u, 0.0, 1.0), 0.5)).rgb;

	return mix(vec3(1.0, 0.08, 0.0), c, smoothstep(1000.0, BLACKBODY_MIN_K, kelvin));
}

void main()
{
	vec3 n = normalize(v_Normal);
	vec3 albedo;
	vec3 to_light = u_LightPos - v_World;
	vec3 to_aux = u_AuxLightPos - v_World;
	vec3 q = v_Model / max(u_HullRadius, 0.001);
	float diffuse, aux_wrap, ragged, soot, glow_edge, t, front, burning;
	vec3 color;

	/* Going away: everything nearer the torn edge than the front is gone.  The front wanders
	 * by noise so the hole grows raggedly, not as an inset copy of the edge. */
	front = v_Edge + (value_noise(q * 14.0) - 0.5) * 0.6 * u_Dissolve - u_Dissolve;
	if (u_Dissolve > 0.0 && front < 0.0)
		discard;

	if (gl_FrontFacing) {
		albedo = u_HaveTexture != 0 ? texture2D(u_Albedo, v_TexCoord).rgb : vec3(0.6);
	} else {
		/* The interior: bare structure, and much darker than the hull, which is most of
		 * what makes a tear read as a hole into the ship rather than a painted patch. */
		n = -n;
		albedo = vec3(u_Interior);
	}

	/* Soot: fully black at the edge, fading out a ragged band's width in -- the band's width
	 * itself wandering along the tear, and blotched within, so it burns rather than stripes. */
	ragged = 0.45 + 0.9 * value_noise(q * 9.0) * (0.6 + 0.4 * value_noise(q * 31.0));
	soot = 1.0 - smoothstep(0.0, max(u_Scorch * ragged, 0.0001), v_Edge);
	albedo *= mix(1.0, 0.04, soot);

	diffuse = max(dot(n, normalize(to_light)), 0.0);
	aux_wrap = max((dot(n, normalize(to_aux)) + u_AuxLightWrap) / (1.0 + u_AuxLightWrap), 0.0);
	color = albedo * (u_StarTint * diffuse + u_Ambient + u_AuxLightColor * aux_wrap);

	/* The torn metal itself, glowing as it cools.  Squared rather than the true fourth power of
	 * temperature, as the shrapnel does, so the dull red end still shows before it goes out. */
	glow_edge = 1.0 - smoothstep(0.0, max(u_EdgeWidth * (0.5 + ragged), 0.0001), v_Edge);
	t = u_EdgeTemp / 2000.0;
	color += blackbody(u_EdgeTemp) * u_EdgeBrightness * t * t * glow_edge *
			smoothstep(800.0, 1500.0, u_EdgeTemp);

	/* The front itself runs hot, a thin band just ahead of the gap, whatever the edges have
	 * cooled to: that is what makes the piece burn away rather than simply get holes.  It
	 * cools as the burn slows, down to a dull red as the last of the piece goes.
	 *
	 * A steady glow reads as a lit edge, not as fire; what reads as fire is change.  So the
	 * band is broken into hot spots that flare and die along it -- noise drifting through the
	 * piece with time -- and ahead of it a wider, dull red PRE-HEAT zone, metal heating before
	 * it goes, gives the burn a depth to eat into.  Only the brightness moves: where the gap
	 * has reached never goes back, so a hole never closes again. */
	if (u_Dissolve > 0.0) {
		float flicker = value_noise(q * 22.0 + vec3(0.0, 0.0, u_Time * 2.3));
		float hot, preheat;

		flicker = 0.25 + 1.5 * flicker * flicker *
				(0.6 + 0.4 * value_noise(q * 7.0 - vec3(u_Time * 0.9)));
		burning = 1.0 - smoothstep(0.0, max(u_EdgeWidth * 1.5, 0.0001), front);
		hot = mix(1100.0, 1800.0, u_BurnGlow) + 250.0 * (flicker - 1.0);
		color += blackbody(hot) * u_EdgeBrightness * (0.3 + 0.5 * u_BurnGlow) * flicker *
				burning;
		preheat = 1.0 - smoothstep(0.0, max(u_Preheat, 0.0001), front);
		color += blackbody(1050.0) * u_EdgeBrightness * 0.25 * u_BurnGlow * preheat *
				preheat * (1.0 - burning);
	}

	gl_FragColor = vec4(clamp(filmic_tonemap(vec4(color, 1.0)).rgb, 0.0, 1.0), 1.0);
}

#endif
