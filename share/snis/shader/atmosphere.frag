
uniform vec3 u_LightPos;       // The position of the light in eye space.
uniform float u_Alpha;		// Relative alpha, 0.0 - 1.0.
uniform float u_atmosphere_brightness; // 0.0 - 1.0, default 0.5  Brightness of atmosphere

varying vec3 v_Position;       // Interpolated position for this fragment.
varying vec3 v_Color;          // This is the color from the vertex shader interpolated across the triangle per fragment
varying vec3 v_Normal;         // Interpolated normal for this fragment.

#if !defined(AMBIENT)
#define AMBIENT 0.01
#endif

float map(in float x, float min1, float max1, float min2, float max2)
{
	return min2 + (x - min1) * (max2 - min2) / (max1 - min1);
}

#if defined(USE_ANNULUS_SHADOW)
	uniform sampler2D u_AnnulusAlbedoTex;
	uniform vec3 u_AnnulusCenter; // center of disk in eye space
	uniform vec3 u_AnnulusNormal; // disk plane normal in eye space
	uniform vec4 u_AnnulusRadius; // x=inside r, y=inside r^2, z=outside r, w=outside r^2
	uniform vec4 u_AnnulusTintColor;
	uniform float u_ring_texture_v;

	bool intersect_plane(vec3 plane_normal, vec3 plane_pos, vec3 ray_pos, vec3 ray_dir, out float t)
	{
		float denom = dot(plane_normal, ray_dir);
		if (abs(denom) > 0.000001) {
			vec3 plane_dir = plane_pos - ray_pos;
			t = dot(plane_normal, plane_dir) / denom;
			return t >= 0;
		}
		return false;
	}

	bool intersect_disc(vec3 disc_normal, vec3 disc_center, float r_squared, vec3 ray_pos,
		vec3 ray_dir, out float dist2)
	{
		float t = 0;
		if (intersect_plane(disc_normal, disc_center, ray_pos, ray_dir, t)) {
			vec3 plane_intersect = ray_pos + ray_dir * t;
			vec3 v = plane_intersect - disc_center;
			dist2 = dot(v, v);
			return dist2 <= r_squared;
		}
		return false;
	}
#endif

void main()
{
	// Get a lighting direction vector from the light to the vertex.
	vec3 lightVector = u_LightPos - v_Position;

	// Calculate the dot product of the light vector and vertex normal
	// and subtract it from 1.  This gives a very crude approximation of
	// how much atmosphere you are looking through, and thus an extremely
	// crude approximation of how much scattered light you would see
	// assuming uniform illumination.
	float eyedot = (1.0 - abs(dot(normalize(v_Normal), normalize(v_Position))));
	float eyedot2 = eyedot * eyedot;

	// Dot product of surface normal with light vector is how much light is
	// reflected, combine (mulitply) this with eyedot to get a crude approximation
	// of scattered light.
	float lightdot = u_atmosphere_brightness * dot(normalize(v_Normal), normalize(lightVector));

	// Subtract some blue-green (opposite of orange)  as we get near the
	// terminator to get a sunset effect.
	float oranginess = pow(1.0 * abs(1.0 - abs(lightdot)), 12.0);
	vec4 notorange = 0.5 * vec4(0.0, 0.4, 1.0, 0.7 * u_Alpha);

	// As atmosphere thins out at edges, scattered light must fall off.
	// Here is a super duper crude way to do that:
	float attenuation = min((1.0 - eyedot), 0.25);
	attenuation = 15.0 * attenuation * attenuation;

	float ring_shadow = 1.0;

#if defined(USE_ANNULUS_SHADOW)
	float direct = 3.75 * attenuation * lightdot *eyedot2 * 0.7;
	float intersect_r_squared;
	if (intersect_disc(u_AnnulusNormal, u_AnnulusCenter, u_AnnulusRadius.w /* r3^2 */,
			v_Position, lightVector, intersect_r_squared))
	{
		if (intersect_r_squared > u_AnnulusRadius.y /* r1^2 */ ) {
			float ir = sqrt(u_AnnulusRadius.y);
			/* figure out a texture coord on the ring that samples from u=0 to 1, v is given */
			float u = (sqrt(intersect_r_squared) - ir) /
					(u_AnnulusRadius.z - ir);

			vec4 ring_color = u_AnnulusTintColor * texture2D(u_AnnulusAlbedoTex, vec2(u, u_ring_texture_v));

			/* how much we will shadow based on transparancy, so 1.0=no shadow, 0.0=full */
			ring_shadow  = 1.0 - ring_color.a;
		}
	}
#endif

	vec4 fragcolor = 3.75 * vec4(v_Color * attenuation * lightdot * eyedot2 * 0.7,
					attenuation * lightdot * eyedot2 * 0.7 * u_Alpha) - attenuation * oranginess * notorange;
	/* This transparency just doesn't seem to work like I want it to. */
	/* fragcolor.a = min(fragcolor.a, smoothstep(0.7, 1.0, ring_shadow) * ring_shadow); */ /* atmosphere becomes more transparent in shadow */
	fragcolor.rgb *= map(ring_shadow, 0.0, 1.0, 0.8, 1.0);
	gl_FragColor = fragcolor;
}

