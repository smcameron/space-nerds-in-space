
uniform vec3 u_Color;          // blackbody colour from the star's temperature
uniform float u_DiscRadius;    // disc radius in UV (0..0.5); world-scale, set per frame
uniform float u_EdgeSoftness;  // disc edge softness as a fraction of the disc radius
uniform float u_CoreBrightness; // core emission scale; drives the centre above 1 so it clamps to white
uniform float u_BloomBrightness; // bloom peak scale at the disc edge (0 = no bloom at all)
uniform float u_BloomRadius;    // bloom reach in UV: distance past the disc edge where it hits zero
uniform float u_BloomFalloff;   // bloom shape: exponent k of pow(1 - d, k) over that reach

varying vec2 v_TexCoord;

/* A star: a world-scale emissive disc with a white-hot core and a screen-scale bloom.
 *
 * Kept structurally identical to share/snis/shader/sun.frag so the two stay easy to diff; only
 * the dialect differs (varying/gl_FragColor).  See that file, and section 1 of
 * doc/star-rendering-and-lighting-notes.txt, for why the emission is clamped through a soft
 * knee rather than pushed through the scene's filmic tonemap.
 */
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

	/* Bloom: a single monotonic glow from the disc edge out to exactly zero one u_BloomRadius
	 * further on -- no plateau, no tail.  u_BloomRadius sets ONLY the reach and u_BloomFalloff
	 * ONLY the shape.  The falloff is applied to a smoothstep of d rather than to d directly so
	 * the glow has zero slope at both ends and cannot cut a hard outer ring. */
	float d = clamp((r - u_DiscRadius) / max(u_BloomRadius, 0.0001), 0.0, 1.0);
	float bloom = pow(1.0 - smoothstep(0.0, 1.0, d), u_BloomFalloff);

	/* Core emission: white-hot at the centre grading to the star's colour at the limb.  A high
	 * centre brightness drives every channel above 1 so it saturates to white; toward the limb
	 * it falls to ~1 so the blackbody colour shows. */
	float disc_t = clamp(r / max(u_DiscRadius, 0.0001), 0.0, 1.0);
	float core_level = 1.0 + (u_CoreBrightness - 1.0) * (1.0 - disc_t) * (1.0 - disc_t);
	vec3 core_emission = u_Color * core_level;
	vec3 bloom_emission = u_Color * u_BloomBrightness * bloom;

	/* Inside the disc the opaque core, outside the additive bloom (premultiplied-alpha blend set
	 * up by the caller).  The soft knee keeps the white plateau's edge gradual, which is what
	 * stops it reading as a Mach-band dark ring. */
	vec3 emission = mix(bloom_emission, core_emission, core);
	emission = soft_knee(emission, 0.75);
	float coverage = clamp(core, 0.0, 1.0);
	gl_FragColor = clamp(vec4(emission, coverage), 0.0, 1.0);
}
