
uniform float u_DiscRadius;     // event horizon radius in UV (0..0.5); world-scale, set per frame
uniform float u_EdgeSoftness;   // rim ramp width as a fraction of the disc radius
uniform float u_RingBrightness; // photon-ring emission scale; 0 for a bare disc
uniform float u_RingWidth;      // rim glow width as a fraction of the disc radius
uniform vec3 u_RingColor;

in vec2 v_TexCoord;

out vec4 f_FragColor;

/* A black hole's event horizon: an utterly opaque black disc with a thin bright rim.
 *
 * This is drawn procedurally rather than sampled from a painted texture because the edge has to
 * land at a *known angle*.  The skybox's gravitational lensing floors its deflection at exactly
 * this radius -- the thin-lens mapping is singular at dead centre, and this disc is what covers
 * the singularity -- so a fuzzy edge that fades out wherever the artwork happens to fade leaves
 * the two disagreeing about where the horizon is, and the Einstein ring then sits at no
 * particular distance from anything.  Computed here, the edge is exact at any zoom, with no
 * texel to magnify into a soft band as you fly up to it.
 *
 * Premultiplied alpha: the caller blends GL_ONE, GL_ONE_MINUS_SRC_ALPHA.  Inside the disc that
 * makes coverage=1, rgb=0 replace the background with black; outside it makes coverage=0 with a
 * non-zero rgb add as pure light, which is what the rim glow wants.
 */
void main()
{
	/* Distance from the billboard centre in UV space (0 at centre, ~0.5 at the edge). */
	float r = length(v_TexCoord - vec2(0.5));

	/* Coverage: fully opaque inside the horizon, falling to nothing across a ramp only wide
	 * enough to antialias the rim.  Clamped to a floor so a zero softness cannot collapse the
	 * smoothstep into a divide by zero (and so the edge is never aliased outright). */
	float edge = max(u_EdgeSoftness * u_DiscRadius, 0.0008);
	float coverage = 1.0 - smoothstep(u_DiscRadius - edge, u_DiscRadius + edge, r);

	/* The photon ring: light on grazing orbits, piled up just outside the horizon.  A
	 * Lorentzian peaked on the disc edge, which has a much longer tail than a gaussian and so
	 * reads as a glow rather than as a drawn circle.  Suppressed by coverage so it only shows
	 * outside the opaque part -- inside, nothing escapes, and a rim bleeding inward would
	 * undo the crisp edge this shader exists to provide. */
	float w = max(u_RingWidth * u_DiscRadius, 0.0001);
	float dr = r - u_DiscRadius;
	float ring = (w * w) / (dr * dr + w * w);
	vec3 emission = u_RingColor * (u_RingBrightness * ring * (1.0 - coverage));

	f_FragColor = vec4(emission, coverage);
}
