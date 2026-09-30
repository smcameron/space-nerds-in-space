/* GLES (GLSL 100) version of share/snis/shader/particles.shader: keep the two in step.
 *
 * Smoke, flame and sparks off burning wreckage, a whole batch of them in one draw.
 *
 * See particle_batch.h.  The mesh is already in world space and already faces the camera, and
 * each vertex carries its particle's look: the texture coordinate is where it sits on its quad,
 * in radii from the middle; the normal is (opacity, emission, kelvin); w is the kind plus the
 * seed as a fraction.
 *
 * SMOKE is shaded as if it were a soft sphere, or stretched, a soft capsule -- a normal made up
 * from where the fragment sits on the disc -- so the star lights one side of it and leaves the
 * other in shade, which is what makes a trail of puffs read as smoke in space and not as a string
 * of flat blots.  The edge is eaten by noise so each puff is a ragged wisp, and it glows warm
 * while it is new, from the hot front it came off.
 *
 * Premultiplied alpha, as the fireball is.
 *
 * Needs filmic.glsl in front of it.
 */

#define BLACKBODY_MIN_K 1900.0
#define BLACKBODY_MAX_K 40000.0

#if defined(INCLUDE_VS)

uniform mat4 u_MVPMatrix;

attribute vec3 a_Position;
attribute vec3 a_Normal;
attribute vec2 a_TexCoord;
attribute float a_Edge;

varying vec2 v_Uv;
varying vec3 v_Look;
varying float v_KindSeed;
varying vec3 v_World;
varying float v_Reach;

void main()
{
	v_Uv = a_TexCoord;
	/* Every corner is as far along as the quad reaches, so this is the same at all four. */
	v_Reach = abs(a_TexCoord.x);
	v_Look = a_Normal;
	v_KindSeed = a_Edge;
	v_World = a_Position;
	gl_Position = u_MVPMatrix * vec4(a_Position, 1.0);
}

#endif

#if defined(INCLUDE_FS)

uniform vec3 u_CamRight;	/* world space, for the smoke's made-up sphere */
uniform vec3 u_CamUp;
uniform vec3 u_CamBack;
uniform vec3 u_LightPos;
uniform vec3 u_StarTint;
uniform float u_Ambient;
uniform float u_Albedo;
uniform float u_Time;		/* seconds, wrapped; runs the flames' licking */
uniform sampler2D u_Blackbody;

varying vec2 v_Uv;
varying vec3 v_Look;
varying float v_KindSeed;
varying vec3 v_World;
varying float v_Reach;	/* half the quad's length, in radii: 1 unless it is stretched */


/* Dave Hoskins' hash without sine, as in explosion.shader. */
float hash12(vec2 p)
{
	vec3 p3 = fract(vec3(p.xyx) * 0.1031);

	p3 += dot(p3, p3.yzx + 33.33);
	return fract((p3.x + p3.y) * p3.z);
}

float value_noise(vec2 p)
{
	vec2 i = floor(p);
	vec2 f = fract(p);
	vec2 u = f * f * (3.0 - 2.0 * f);

	return mix(mix(hash12(i), hash12(i + vec2(1.0, 0.0)), u.x),
		mix(hash12(i + vec2(0.0, 1.0)), hash12(i + vec2(1.0, 1.0)), u.x), u.y);
}

/* As in explosion.shader: the ramp stops at 1900K, and below it the glow only reddens. */
vec3 blackbody(float kelvin)
{
	float u = (kelvin - BLACKBODY_MIN_K) / (BLACKBODY_MAX_K - BLACKBODY_MIN_K);
	vec3 c = texture2D(u_Blackbody, vec2(clamp(u, 0.0, 1.0), 0.5)).rgb;

	return mix(vec3(1.0, 0.08, 0.0), c, smoothstep(1000.0, BLACKBODY_MIN_K, kelvin));
}

vec4 smoke(vec2 c, float seed)
{
	vec2 offset = vec2(seed * 713.0, seed * 371.0);
	/* A stretched puff -- a stretch of a trail -- is a capsule: round at the ends, the same
	 * across all along its middle.  An unstretched one is the plain disc. */
	float r;

	c.x = sign(c.x) * max(abs(c.x) - (max(v_Reach, 1.0) - 1.0), 0.0);
	r = length(c);
	float rim, density, lambert, alpha;
	vec3 n, light_dir, lit, emit;

	/* The ragged edge: the disc's radius wandering by noise around its rim. */
	rim = r + (value_noise(c * 3.0 + offset) - 0.5) * 0.45;
	density = (1.0 - smoothstep(0.35, 0.95, rim)) *
			(0.65 + 0.35 * value_noise(c * 7.0 + offset.yx));
	alpha = clamp(density * v_Look.x, 0.0, 1.0);
	if (alpha <= 0.002)
		return vec4(0.0);

	/* A made-up sphere's normal, from the camera's own basis. */
	n = normalize(u_CamRight * c.x + u_CamUp * c.y +
			u_CamBack * sqrt(max(1.0 - min(r * r, 1.0), 0.0)));
	light_dir = normalize(u_LightPos - v_World);
	/* Half wrapped: smoke scatters light round into its shadowed side. */
	lambert = max(dot(n, light_dir) * 0.6 + 0.4, 0.0);
	lit = u_Albedo * vec3(0.9, 0.86, 0.8) * (u_StarTint * lambert + u_Ambient);
	emit = blackbody(v_Look.z) * v_Look.y;

	return vec4(clamp(filmic_tonemap(vec4(lit + emit, 1.0)).rgb, 0.0, 1.0) * alpha, alpha);
}

/* A tongue of flame, stretched along its quad from its base at the far negative end to its tip at
 * the positive: widest at the base, wavering from side to side and torn by noise toward the tip.
 * The noise runs out along it with time, so the fire licks outward while the tongue stays put.
 * Soft all through -- a gaussian across it, not an edge -- and hot only in its core: yellow there,
 * orange to red at its fringes and its tip.  Mostly light added, with a little alpha so it dims
 * what it is in front of. */
vec4 flame(vec2 c, float seed)
{
	float reach = max(v_Reach, 1.0);
	float s, width, across, n, density, core;
	vec3 emit;

	/* 0 at the base, 1 at the tip. */
	s = clamp((c.x + reach) / (2.0 * reach), 0.0, 1.0);
	/* It wavers more the further it gets from its root. */
	c.y += (value_noise(vec2(c.x * 0.7 - u_Time * 5.0 + seed * 13.0, seed * 7.0)) - 0.5) *
			1.2 * s;
	width = mix(1.0, 0.35, s);
	across = abs(c.y) / width;
	n = value_noise(vec2(c.x * 1.1 - u_Time * 8.0 - seed * 57.0, c.y * 2.0 + seed * 31.0));
	density = exp(-across * across * 2.2) * (0.6 + 0.4 * n) *
			smoothstep(0.0, 0.8 / reach, s) *
			(1.0 - smoothstep(0.3, 1.0, s + (n - 0.5) * 0.6));
	if (density <= 0.002)
		return vec4(0.0);
	core = density * density;
	emit = blackbody(v_Look.z * (0.7 + 0.4 * core) * (1.0 - 0.25 * s)) * v_Look.y * density;
	return vec4(clamp(filmic_tonemap(vec4(emit, 1.0)).rgb, 0.0, 1.0),
			clamp(v_Look.x * density, 0.0, 1.0));
}

/* A spark: a hot point, smeared along its quad by its motion into a capsule -- a white-hot core in
 * an orange glow.  The light it gives is spread along the streak, but only by the square root of
 * its length: in proportion, a fast spark would all but vanish, where a real one at the edge of
 * seeing is still a bright line.  All light, no alpha. */
vec4 spark(vec2 c)
{
	float reach = max(v_Reach, 1.0);
	float along = max(abs(c.x) - (reach - 1.0), 0.0);
	float d2 = along * along + c.y * c.y;
	float glow = exp(-d2 * 3.0);
	float core = exp(-d2 * 16.0);
	vec3 emit;

	if (glow <= 0.002)
		return vec4(0.0);
	emit = (blackbody(v_Look.z) * glow * 0.5 + blackbody(v_Look.z * 1.4) * core) *
			v_Look.y / sqrt(reach);
	return vec4(clamp(filmic_tonemap(vec4(emit, 1.0)).rgb, 0.0, 1.0), 0.0);
}

void main()
{
	float kind = floor(v_KindSeed);
	float seed = v_KindSeed - kind;
	vec4 c;

	if (kind > 1.5)
		c = spark(v_Uv);
	else if (kind > 0.5)
		c = flame(v_Uv, seed);
	else
		c = smoke(v_Uv, seed);

	if (c.a <= 0.002 && dot(c.rgb, vec3(1.0)) <= 0.002)
		discard;
	gl_FragColor = c;
}

#endif
