
uniform float u_DiscRadius;     // event horizon radius in UV (0..0.5); world-scale, set per frame
uniform float u_EdgeSoftness;   // rim ramp width as a fraction of the disc radius
uniform float u_RingBrightness; // photon-ring emission scale; 0 for a bare disc
uniform float u_RingWidth;      // rim glow width as a fraction of the disc radius
uniform vec3 u_RingColor;

varying vec2 v_TexCoord;

/* A black hole's event horizon: an utterly opaque black disc with a thin bright rim.
 *
 * Kept structurally identical to share/snis/shader/black_hole.frag so the two stay easy to
 * diff; only the dialect differs (varying/gl_FragColor).  See that file for why this is drawn
 * procedurally rather than sampled from a painted texture.
 */
void main()
{
	/* Distance from the billboard centre in UV space (0 at centre, ~0.5 at the edge). */
	float r = length(v_TexCoord - vec2(0.5));

	/* Coverage: fully opaque inside the horizon, falling to nothing across a ramp only wide
	 * enough to antialias the rim. */
	float edge = max(u_EdgeSoftness * u_DiscRadius, 0.0008);
	float coverage = 1.0 - smoothstep(u_DiscRadius - edge, u_DiscRadius + edge, r);

	/* The photon ring: light on grazing orbits, piled up just outside the horizon.  Suppressed
	 * by coverage so it only shows outside the opaque part. */
	float w = max(u_RingWidth * u_DiscRadius, 0.0001);
	float dr = r - u_DiscRadius;
	float ring = (w * w) / (dr * dr + w * w);
	vec3 emission = u_RingColor * (u_RingBrightness * ring * (1.0 - coverage));

	gl_FragColor = vec4(emission, coverage);
}
