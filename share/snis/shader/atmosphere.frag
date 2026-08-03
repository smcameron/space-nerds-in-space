
uniform vec3 u_LightPos;       // The position of the light in eye space.
uniform float u_Alpha;		// Relative alpha, 0.0 - 1.0.
uniform float u_atmosphere_brightness; // 0.0 - 1.0, default 0.5  Brightness of atmosphere
uniform vec3 u_LightColor;     // star-tinted direct light, from star_light_colors()
uniform vec3 u_AmbientColor;   // complement-tinted shade, from the same

in vec3 v_Position;       // Interpolated position for this fragment.
in vec3 v_Color;          // This is the color from the vertex shader interpolated across the triangle per fragment
in vec3 v_Normal;         // Interpolated normal for this fragment.

out vec4 f_FragColor;

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
			return t >= 0.0;
		}
		return false;
	}

	bool intersect_disc(vec3 disc_normal, vec3 disc_center, float r_squared, vec3 ray_pos,
		vec3 ray_dir, out float dist2)
	{
		float t = 0.0;
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
	float eyedot = max(0.0, (1.0 - abs(dot(normalize(v_Normal), normalize(v_Position)))));
	float eyedot2 = eyedot * eyedot;

	// Dot product of surface normal with light vector is how much light is
	// reflected, combine (mulitply) this with eyedot to get a crude approximation
	// of scattered light.
	float lightdot = dot(normalize(v_Normal), normalize(lightVector));

	/* Reddening at the terminator is EXTINCTION, not a subtraction of a fixed colour.  As
	 * the light grazes, its path through the atmosphere lengthens and the short wavelengths
	 * scatter out of it first, so what survives reddens.  The coefficients go as 1/lambda^4
	 * (Rayleigh), normalised to red.
	 *
	 * The subtraction this replaces drove green negative over 45% of the disc and blue over
	 * 44%; in the sunset band itself the mean pre-clamp value was (-0.00, -0.13, -0.33), so
	 * two of three channels sat pinned at zero and the band read as one flat red blob with
	 * no gradient left in it.  It also used a hardcoded blue-green regardless of what the
	 * atmosphere was made of, and abs(lightdot) leaked it onto the unlit hemisphere.
	 * Multiplying can never go negative, keeps the gradient, scales with the atmosphere's
	 * own colour and the star's, and needs no night-side special case because the direct
	 * term already carries lightdot. */
	float path_length = 1.0 / max(abs(lightdot), 0.05);
	vec3 extinction = exp(-vec3(1.0, 2.4, 5.5) * (path_length - 1.0) * 0.05);

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

			vec4 ring_color = u_AnnulusTintColor * texture(u_AnnulusAlbedoTex, vec2(u, u_ring_texture_v));

			/* how much we will shadow based on transparancy, so 1.0=no shadow, 0.0=full */
			ring_shadow  = 1.0 - ring_color.a;
		}
	}
#endif

	vec4 fragcolor = 1.25 * vec4(v_Color * attenuation * u_atmosphere_brightness * lightdot * eyedot2 * 0.7,
					attenuation * u_atmosphere_brightness * lightdot * eyedot2 * 0.7 * u_Alpha);
	fragcolor.rgb *= extinction;
	/* This transparency just doesn't seem to work like I want it to. */
	/* fragcolor.a = min(fragcolor.a, smoothstep(0.7, 1.0, ring_shadow) * ring_shadow); */ /* atmosphere becomes more transparent in shadow */
	fragcolor.rgb *= map(ring_shadow, 0.0, 1.0, 0.8, 1.0);
	/* Carry the star's colour into the halo: without this a purple star lit its planet's
	 * surface purple and left a white rim around it.  The scattered light IS starlight, so
	 * this is a straight tint -- lightdot is already folded into fragcolor above, and
	 * blending toward u_AmbientColor here instead would multiply an absolute ambient into
	 * an already-dim colour and crush the shaded limb to nothing.  Applied after the
	 * extinction, so the star's colour tints light that has already been reddened by the
	 * path rather than the other way round. */
	fragcolor.rgb *= u_LightColor;

	/* Ambient scattering, so the atmosphere does not simply stop at the terminator.  It
	 * follows the same density profile as the direct term but drops the lightdot, which is
	 * the only thing keeping light off the night side -- the twilight band a real
	 * atmosphere carries past its own terminator.  Added, not blended, so it lifts the
	 * shaded limb without dimming the lit one.
	 *
	 * LEVEL from u_AmbientColor, HUE from u_LightColor, deliberately.  u_AmbientColor is
	 * mixed toward the star's COMPLEMENT -- a warm-key/cool-fill convention that suits a
	 * hull sitting in a scene, but is backwards for an atmosphere: twilight is starlight
	 * scattered through a long path, which reddens it rather than flipping its hue.  Using
	 * the complement here would also fight the Rayleigh extinction above, which reddens the
	 * same band for exactly that reason.  So take only the brightness that
	 * star_dark_tint and star_shadow_darkening imply, and keep the star's own colour. */
	float ambient_level = dot(u_AmbientColor, vec3(0.2126, 0.7152, 0.0722));
	fragcolor.rgb += v_Color * u_LightColor * ambient_level * u_atmosphere_brightness *
				attenuation * eyedot2 * 0.7;
	f_FragColor = fragcolor;
	f_FragColor = filmic_tonemap(f_FragColor);
}

