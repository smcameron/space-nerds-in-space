/* GLES (GLSL 100) version of share/snis/shader/shrapnel.shader: keep the two in step.
 *
 * One shard of shrapnel: dark metal lit by the star, glowing with its own heat.
 *
 * See struct material_shrapnel in material.h.  Two terms and no more: the star's light on the
 * cold metal, lambertian as the hulls are lit, and incandescence from the shard's temperature,
 * through the same blackbody ramp the fireball uses -- so a shard leaving the fireball is the
 * fireball's colour, and cools the way it does.
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

varying vec3 v_World;
varying vec3 v_Normal;

void main()
{
	v_World = (u_ModelMatrix * vec4(a_Position, 1.0)).xyz;
	/* Uniform scale only, so the normal survives the model matrix once renormalised. */
	v_Normal = normalize((u_ModelMatrix * vec4(a_Normal, 0.0)).xyz);
	gl_Position = u_MVPMatrix * vec4(a_Position, 1.0);
}

#endif

#if defined(INCLUDE_FS)

uniform vec3 u_LightPos;
uniform vec3 u_StarTint;
uniform float u_Ambient;
uniform sampler2D u_Blackbody;

uniform float u_Temperature;
uniform float u_Brightness;
uniform float u_Albedo;

varying vec3 v_World;
varying vec3 v_Normal;


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
	vec3 to_light = u_LightPos - v_World;
	float diffuse = max(dot(n, normalize(to_light)), 0.0);
	vec3 metal = u_Albedo * vec3(0.55, 0.53, 0.5) * (u_StarTint * diffuse + u_Ambient);
	/* Radiance climbs steeply with heat; squared rather than the true fourth power so the
	 * dull red end is still visible before it goes out. */
	float t = u_Temperature / 2000.0;
	vec3 glow = blackbody(u_Temperature) * u_Brightness * t * t *
			smoothstep(800.0, 1500.0, u_Temperature);

	gl_FragColor = vec4(clamp(filmic_tonemap(vec4(metal + glow, 1.0)).rgb, 0.0, 1.0), 1.0);
}

#endif
