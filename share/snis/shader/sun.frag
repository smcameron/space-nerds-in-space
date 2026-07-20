
uniform vec3 u_Color;          // blackbody colour from the star's temperature
uniform float u_DiscRadius;    // disc radius in UV (0..0.5); world-scale, set per frame
uniform float u_EdgeSoftness;  // disc edge softness as a fraction of the disc radius
uniform float u_CoreBrightness; // core emission scale; drives the centre above 1 so it clamps to white
uniform float u_BloomBrightness; // bloom peak scale at the disc edge (0 = no bloom at all)
uniform float u_BloomRadius;    // bloom reach in UV: distance past the disc edge where it hits zero
uniform float u_BloomFalloff;   // bloom shape: exponent k of pow(1 - d, k) over that reach

in vec2 v_TexCoord;

out vec4 f_FragColor;

/* Soft highlight knee: channels below `knee` pass through unchanged (so the halo and the star's
 * colour are untouched); above it they roll off smoothly toward 1 instead of hitting a hard clamp.
 * A hard clamp turns the overexposed core into a flat white plateau with a hard edge, and that edge
 * reads as a Mach-band "dark ring".  The soft knee gives the plateau a gradual edge so the illusion
 * is much weaker (measured: it roughly halves the brightness gradient at the core/halo boundary). */
vec3 soft_knee(vec3 x, float knee)
{
	vec3 over = max(x - vec3(knee), vec3(0.0));
	return min(x, vec3(knee)) + (1.0 - knee) * (1.0 - exp(-over / (1.0 - knee)));
}

void main()
{
	/* Distance from the billboard centre in UV space (0 at centre, ~0.5 at the edge). */
	float r = length(v_TexCoord - vec2(0.5));

	/* Disc coverage: opaque inside u_DiscRadius with a soft edge.  The disc radius is set per
	 * frame from the star's world radius over the billboard size, so the disc is world-scale
	 * (grows as the camera approaches). */
	float edge = max(u_EdgeSoftness * u_DiscRadius, 0.001);
	float core = 1.0 - smoothstep(u_DiscRadius - edge, u_DiscRadius + edge, r);

	/* Bloom: a single monotonic glow that starts at the disc edge and falls to exactly zero one
	 * u_BloomRadius further out -- no plateau and no tail, so it cannot read as a second ring or a
	 * screen-filling wash.  d is the normalised distance across that reach (0 at the disc edge, 1
	 * where it vanishes); pow(1 - d, k) shapes it: k > 1 concentrates the glow near the disc, k < 1
	 * spreads it toward the outer edge.  u_BloomRadius sets ONLY the reach and u_BloomFalloff sets
	 * ONLY the shape, so the two controls are orthogonal, and the whole falloff is always visible
	 * (the caller sizes the billboard to just contain disc + bloom).  The falloff is applied to a
	 * smoothstep of d, not to d directly, so the glow has zero slope both where it leaves the disc
	 * and where it vanishes -- a raw pow(1 - d, k) drops almost vertically at its outer edge for
	 * small k, cutting a hard outer ring. */
	float d = clamp((r - u_DiscRadius) / max(u_BloomRadius, 0.0001), 0.0, 1.0);
	float bloom = pow(1.0 - smoothstep(0.0, 1.0, d), u_BloomFalloff);

	/* Core emission: white-hot at the centre grading to the star's colour at the limb.  A high
	 * centre brightness drives every channel above 1 so it clamps to white; toward the limb the
	 * brightness falls to ~1 so the blackbody colour shows.  u_CoreBrightness sets how bright the
	 * centre is and therefore how far the white-hot region spreads.  A flat core instead pins red
	 * and green to white everywhere; the gradient keeps the limb colour smooth as the star cools. */
	float disc_t = clamp(r / max(u_DiscRadius, 0.0001), 0.0, 1.0);
	float core_level = 1.0 + (u_CoreBrightness - 1.0) * (1.0 - disc_t) * (1.0 - disc_t);
	vec3 core_emission = u_Color * core_level;
	vec3 bloom_emission = u_Color * u_BloomBrightness * bloom;

	/* Inside the disc show the opaque core; outside show the additive bloom (premultiplied-alpha
	 * blend set up by the caller).  The bright core is rolled off with a soft knee rather than a
	 * hard clamp: a bright channel still saturates toward white (the hot centre) while the dim limb
	 * and the halo keep their true hue, but the white plateau now has a gradual edge instead of a
	 * hard one, which is what kills the Mach-band ring.  (A per-channel filmic tonemap was tried
	 * instead but it boosts midtones and shifts the colour yellow, and overflows to NaN.) */
	vec3 emission = mix(bloom_emission, core_emission, core);
	emission = soft_knee(emission, 0.75);
	float coverage = clamp(core, 0.0, 1.0);
	f_FragColor = clamp(vec4(emission, coverage), 0.0, 1.0);
}
