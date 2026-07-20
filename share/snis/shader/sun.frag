
uniform vec3 u_Color;          // blackbody colour from the star's temperature
uniform float u_DiscRadius;    // disc radius in UV (0..0.5); world-scale, set per frame
uniform float u_EdgeSoftness;  // disc edge softness as a fraction of the disc radius
uniform float u_CoreBrightness; // core emission scale (HDR; whitens via the tonemap)
uniform float u_BloomBrightness;
uniform float u_BloomRadius;    // bloom half-brightness radius in UV, from the disc edge
uniform float u_BloomFalloff;   // bloom edge sharpness: exponent k of 1/(1 + d^k)

in vec2 v_TexCoord;

out vec4 f_FragColor;

void main()
{
	/* Distance from the billboard centre in UV space (0 at centre, ~0.5 at the edge). */
	float r = length(v_TexCoord - vec2(0.5));

	/* Core: flat inside the disc with a soft edge at u_DiscRadius.  The disc radius is set per
	 * frame from the star's world radius over the billboard size, so the disc is world-scale
	 * (grows as the camera approaches). */
	float edge = max(u_EdgeSoftness * u_DiscRadius, 0.001);
	float core = 1.0 - smoothstep(u_DiscRadius - edge, u_DiscRadius + edge, r);

	/* Bloom: a soft glow reaching outward from the disc edge.  Butterworth falloff
	 * 1/(1 + d^k) with d = (distance past the disc edge) / u_BloomRadius: at d = 1 the glow is
	 * always 0.5 regardless of k, so u_BloomRadius alone sets the half-brightness radius (how far
	 * it reaches) and u_BloomFalloff (k) sets only the edge sharpness -- the two are orthogonal.
	 * Only the annulus outside the disc is visible, so it reads as a limb glow that fades out. */
	float d = max(r - u_DiscRadius, 0.0) / max(u_BloomRadius, 0.0001);
	float glow = 1.0 / (1.0 + pow(d, u_BloomFalloff));
	/* Fade to zero before the billboard edge so a low sharpness does not hard-clip. */
	float x = clamp((r - u_DiscRadius) / max(0.5 - u_DiscRadius, 0.0001), 0.0, 1.0);
	float bloom = glow * (1.0 - smoothstep(0.7, 1.0, x));

	/* One emission field pushed through the shared filmic tonemap: the bright core clips toward
	 * white while the limb and bloom keep the star's colour, consistent with the rest of the
	 * scene.  Coverage is opaque over the core (hides the background) and additive in the bloom
	 * (adds light) via the premultiplied-alpha blend the caller sets up. */
	vec3 emission = u_Color * (u_CoreBrightness * core + u_BloomBrightness * bloom);
	float coverage = clamp(core, 0.0, 1.0);
	f_FragColor = filmic_tonemap(vec4(emission, coverage));
}
