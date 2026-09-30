/* GLES (GLSL 100) version of share/snis/shader/explosion.shader: keep the two in step.
 *
 * A ship's death: a fireball that cools into smoke and thins away to nothing.
 *
 * See struct material_explosion in material.h for what each uniform means.
 *
 * ONE VOLUME, RAYMARCHED INSIDE A BILLBOARD.  The billboard is a camera facing quad scaled to
 * the ball's radius; the vertex shader widens it just enough to hold the sphere's silhouette in
 * perspective, and the fragment shader intersects its view ray with the sphere and marches
 * through it front to back, emission and absorption together.
 *
 * THE SHAPE is round blobs, not wisps.  Inverted cellular noise gives a field that is highest
 * at scattered points and falls off roundly around each, and letting it bite into the edge of
 * the ball gives the cauliflower outline of a real fireball.  Value noise adds finer detail
 * on top.  Everything is sampled in the ball's own unit coordinates, so the lumps grow with
 * the ball, and the sample point is pulled inward along the radius as the explosion ages so
 * that the lumps appear to roll outward.
 *
 * THE LIFE is one number, u_Age, from 0 to 1.  Heat decays exponentially with it and is
 * hottest at the centre, so the ball goes white, yellow, orange and dull red from the outside
 * in; once it is too cool to glow it is only smoke, lit by the star.  From u_SmokeStart an
 * erosion threshold climbs through the density, breaking the smoke into shreds, and it is
 * built so that at u_Age = 1 no density survives anywhere: the explosion ends in nothing, not
 * in a faint ball that has to be popped out of existence.
 *
 * PREMULTIPLIED ALPHA.  Alpha is the smoke's real coverage; it hides what is behind it.  The
 * colour is tonemapped unpremultiplied and multiplied back, so a thin wisp is not washed out
 * by the tonemapper's toe the way tonemapping the premultiplied value would.
 */

#define BLACKBODY_MIN_K 1900.0
#define BLACKBODY_MAX_K 40000.0

/* A bound, not the count: the count is u_Steps, and the loop breaks there.  GLSL ES 1.00
 * wants a constant loop bound, and this file has to survive that port. */
/* Fewer than the desktop's 64: a GLES GPU marches a quarter of the pixels at most this many
 * steps, whatever the game asks for. */
#define EXPLOSION_MAX_STEPS 16

#if defined(INCLUDE_VS)

uniform mat4 u_MVPMatrix;
uniform mat4 u_ModelMatrix;
uniform vec3 u_EyePos;

attribute vec3 a_Position;

varying vec3 v_World;
varying vec3 v_Center;
varying float v_Radius;

void main()
{
	vec3 center = (u_ModelMatrix * vec4(0.0, 0.0, 0.0, 1.0)).xyz;
	float radius = length(u_ModelMatrix[0].xyz);
	float d = length(center - u_EyePos);
	/* A sphere of radius r seen from d subtends more than r at its centre plane: its
	 * silhouette is at r / sqrt(1 - (r/d)^2) there.  Widen the quad to that, or the ball is
	 * clipped square at close range.  Capped, since from inside the ball it has no edge. */
	float k = radius / max(d, radius * 1.001);
	float widen = min(1.0 / sqrt(1.0 - k * k), 8.0);
	vec3 world = (u_ModelMatrix * vec4(a_Position * widen, 1.0)).xyz;

	v_World = world;
	v_Center = center;
	v_Radius = radius;
	gl_Position = u_MVPMatrix * vec4(a_Position * widen, 1.0);
}

#endif

#if defined(INCLUDE_FS)

uniform vec3 u_EyePos;
uniform vec3 u_LightPos;
uniform vec3 u_StarTint;
uniform float u_Ambient;
uniform sampler2D u_Blackbody;

uniform float u_Age;
uniform float u_Seed;
uniform float u_PeakTemp;
uniform float u_Cooling;
uniform float u_Brightness;
uniform float u_Radiance;
uniform float u_Density;
uniform float u_Edge;
uniform float u_Lumpiness;
uniform float u_Frequency;
uniform float u_Roll;
uniform float u_SmokeStart;
uniform float u_SmokeAlbedo;
uniform float u_Dilution;
uniform float u_Shred;
uniform int u_Steps;
/* The opaque scene's depth, and what it takes to turn it into distance along a ray. */

varying vec3 v_World;
varying vec3 v_Center;
varying float v_Radius;


/* Dave Hoskins' hash without sine: sin() based hashes fall apart at mediump. */
float hash13(vec3 p)
{
	p = fract(p * 0.1031);
	p += dot(p, p.zyx + 31.32);
	return fract((p.x + p.y) * p.z);
}

vec3 hash33(vec3 p)
{
	p = fract(p * vec3(0.1031, 0.1030, 0.0973));
	p += dot(p, p.yxz + 33.33);
	return fract((p.xxy + p.yxx) * p.zyx);
}

float value_noise(vec3 p)
{
	vec3 i = floor(p);
	vec3 f = fract(p);
	vec3 u = f * f * f * (f * (f * 6.0 - 15.0) + 10.0);

	return mix(mix(mix(hash13(i), hash13(i + vec3(1.0, 0.0, 0.0)), u.x),
			mix(hash13(i + vec3(0.0, 1.0, 0.0)), hash13(i + vec3(1.0, 1.0, 0.0)), u.x),
			u.y),
		mix(mix(hash13(i + vec3(0.0, 0.0, 1.0)), hash13(i + vec3(1.0, 0.0, 1.0)), u.x),
			mix(hash13(i + vec3(0.0, 1.0, 1.0)), hash13(i + vec3(1.0, 1.0, 1.0)), u.x),
			u.y),
		u.z);
}

/* 1 at a feature point, falling roundly to 0 a cell away: a field of blobs.
 *
 * Searches the 2x2x2 block of cells on the sample's side, not the full 3x3x3: 8 hashes rather
 * than 27, and this is the inner loop of the whole effect.  A nearer point in a skipped cell
 * is possible, so the feature points are kept off the cell walls, which makes the miss rare
 * and small -- a slightly flattened blob, not a seam. */
float blobs(vec3 p)
{
	vec3 i = floor(p);
	vec3 f = fract(p);
	vec3 o = step(0.5, f) - 1.0;
	float d = 1.0;

	/* GLSL 100: a loop's counter is declared in its own header. */
	for (int z = 0; z <= 1; z++)
		for (int y = 0; y <= 1; y++)
			for (int x = 0; x <= 1; x++) {
				vec3 cell = o + vec3(float(x), float(y), float(z));
				vec3 r = cell + 0.15 + 0.7 * hash33(i + cell) - f;
				d = min(d, dot(r, r));
			}
	return 1.0 - sqrt(d);
}

/* Where a point in the ball's unit coordinates reads the fields.  Rolling outward is a zoom
 * INTO the field: every lump drifts away from the centre and swells as it goes.  Not a shift
 * along the radius, which is singular at the centre and draws concentric rings there. */
vec3 field_coord(vec3 p)
{
	return p * u_Frequency / (1.0 + u_Roll * u_Age) + vec3(u_Seed * 17.0, u_Seed * 31.0, 0.0);
}

/* The big lumps, 0..1. */
float lumps_at(vec3 q)
{
	return 0.7 * blobs(q) + 0.3 * blobs(q * 2.1 + 5.3);
}

/* The fine detail, 0..1. */
float detail_at(vec3 q)
{
	return 0.65 * value_noise(q * 2.5) + 0.35 * value_noise(q * 5.1);
}

/* SOOT, 0..1: fuel-rich pockets, burning cooler and darker and thicker than the gas round them.
 * A ball of evenly glowing gas reads as a lit cotton ball, however bright; what reads as fire is
 * contrast -- dark filaments of soot wound through bright billows.  Coarser than the detail and
 * independent of the lumps, so the dark runs across them rather than following their folds. */
float soot_at(vec3 q)
{
	return smoothstep(0.45, 0.8, value_noise(q * 1.3 + vec3(7.1, 3.7, 11.3)));
}

/* How far erosion has climbed.  Set once per fragment in main(); it depends on uniforms only.
 *
 * In vacuum the fading is the dilution: the gas coasts outward and its density falls as the
 * cube of the radius, which the caller passes in.  Erosion only shreds what is left and
 * guarantees the end.  The body below peaks at exactly 1.0 + 0.25 = 1.25, so a threshold that
 * climbs to 1.25 leaves nothing: that is the "ends in nothing" promise, and why the two numbers
 * must move together. */
float erosion;

/* The density's two parts: the shape of the ball, pushed in between the lumps, and whether
 * the erosion has eaten this spot.  Both are zero wherever the lumps alone say so, which is
 * what lets the caller skip the detail there. */
float surface_of(float lumps)
{
	return 1.0 - u_Lumpiness * (1.0 - lumps);
}

float density_of(float r, float lumps, float detail)
{
	float surface = surface_of(lumps);
	float shape = 1.0 - smoothstep(surface - u_Edge, surface, r);
	float body = 0.6 * lumps + 0.4 * detail + 0.25 - erosion;

	return shape * clamp(body * 3.0, 0.0, 1.0) * u_Dilution;
}

vec3 blackbody(float kelvin)
{
	float u = (kelvin - BLACKBODY_MIN_K) / (BLACKBODY_MAX_K - BLACKBODY_MIN_K);
	vec3 c = texture2D(u_Blackbody, vec2(clamp(u, 0.0, 1.0), 0.5)).rgb;

	/* The ramp stops at 1900K, which is the orange of a candle; a cooling fireball's rims
	 * go on down through a deep red, and without this they would stay orange until the
	 * glow gave out.  Blackbody hue barely changes below here, it only reddens. */
	return mix(vec3(1.0, 0.08, 0.0), c, smoothstep(1000.0, BLACKBODY_MIN_K, kelvin));
}

void main()
{
	vec3 rd = normalize(v_World - u_EyePos);
	vec3 ro = (u_EyePos - v_Center) / v_Radius;
	float b = dot(ro, rd);
	float c = dot(ro, ro) - 1.0;
	float h = b * b - c;
	float t0, t1, dt, t, transmittance;
	vec3 light_dir, color;
	float heat_now;
	int steps;

	if (h <= 0.0)
		discard;
	h = sqrt(h);
	t0 = max(-b - h, 0.0);
	t1 = -b + h;
	/* GLES2 cannot read the depth buffer back, so unlike the desktop's the march does not stop
	 * at a solid thing inside the ball: it is drawn through it. */
	if (t1 <= t0)
		discard;

	erosion = pow(clamp((u_Age - u_SmokeStart) / (1.0 - u_SmokeStart), 0.0, 1.0), u_Shred) *
			1.25;
	light_dir = normalize(u_LightPos - v_Center);
	heat_now = exp(-u_Cooling * u_Age);
	/* Thin smoke needs fewer samples: there is no hard surface for a coarse step to miss.  And
	 * the thin smoke is also the big smoke, filling the most of the screen -- so the cost falls
	 * as the ball grows instead of climbing with it.  The cube root of the dilution is the
	 * ratio of the ball's radius when the fire went out to its radius now. */
	steps = int(max(float(u_Steps) * (0.3 + 0.7 * pow(u_Dilution, 1.0 / 3.0)) + 0.5, 4.0));
	dt = (t1 - t0) / float(steps);
	/* Jitter the start per pixel so the steps do not show as rings.  Interleaved gradient
	 * noise (Jimenez) rather than a hash: its error is spread evenly at the finest scale,
	 * where a white noise hash clumps into a visible sponge-like grain. */
	t = t0 + dt * fract(52.9829189 * fract(dot(gl_FragCoord.xy, vec2(0.06711056, 0.00583715))));
	transmittance = 1.0;
	color = vec3(0.0);

	for (int i = 0; i < EXPLOSION_MAX_STEPS; i++) {
		vec3 p, q;
		float lumps, detail, soot, dens, a, r, kelvin, glow, shadow;
		vec3 emit, lit;

		if (i >= steps || transmittance < 0.01)
			break;
		p = ro + rd * t;
		t += dt;
		r = length(p);
		q = field_coord(p);
		lumps = lumps_at(q);
		/* Outside the lumpy edge, or eaten however the detail falls: nothing here, and
		 * no need to pay for the detail to find that out. */
		if (r >= surface_of(lumps) || 0.6 * lumps + 0.65 <= erosion)
			continue;
		detail = detail_at(q);
		dens = density_of(r, lumps, detail);
		if (dens <= 0.0)
			continue;
		soot = soot_at(q);
		dens *= 1.0 + soot;
		a = 1.0 - exp(-dens * u_Density * dt);

		/* Hottest at the centre and in the heart of each lump, cooler toward the ball's
		 * edge, in the folds between lumps and in the soot.  The spread has to be wide: it
		 * is what shows a white-hot core through orange billows with dark, dull red folds,
		 * and too narrow a spread paints the whole ball one flat colour -- which, bright
		 * enough, the tonemapper then flattens to a pale yellow. */
		kelvin = u_PeakTemp * heat_now * (1.0 - 0.85 * r * r) * (0.35 + 0.65 * detail) *
				(0.35 + 0.65 * lumps) * (1.0 - 0.6 * soot);
		glow = u_Brightness * pow(max(kelvin / u_PeakTemp, 0.0), u_Radiance) *
				smoothstep(900.0, 1700.0, kelvin);
		emit = blackbody(kelvin) * glow;

		/* Starlight on the smoke.  One sample toward the star, of the big lumps only,
		 * stands in for the light march: the lumps are what cast shadows big enough to
		 * see, and the detail there would cost as much again as everything else. */
		{
			vec3 p1 = p + light_dir * 0.3;

			shadow = exp(-density_of(length(p1), lumps_at(field_coord(p1)), 0.5) *
					0.3 * u_Density);
		}
		lit = u_SmokeAlbedo * vec3(0.9, 0.85, 0.8) * (u_StarTint * shadow + u_Ambient);

		color += transmittance * a * (emit + lit);
		transmittance *= 1.0 - a;
	}

	{
		float alpha = 1.0 - transmittance;
		vec3 straight;

		if (alpha <= 0.001)
			discard;
		straight = color / alpha;
		gl_FragColor = vec4(clamp(filmic_tonemap(vec4(straight, 1.0)).rgb, 0.0, 1.0) * alpha,
					alpha);
	}
}

#endif
