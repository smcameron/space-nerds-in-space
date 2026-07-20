
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

	/* Disc coverage: opaque inside u_DiscRadius with a soft edge.  The disc radius is set per
	 * frame from the star's world radius over the billboard size, so the disc is world-scale
	 * (grows as the camera approaches). */
	float edge = max(u_EdgeSoftness * u_DiscRadius, 0.001);
	float core = 1.0 - smoothstep(u_DiscRadius - edge, u_DiscRadius + edge, r);

	/* Bloom: a compact soft glow reaching outward from the disc edge, half-brightness at
	 * u_BloomRadius and exactly zero by u_BloomRadius * (1 + w).  It has no tail, so there is no
	 * separate outer fade/window to read as a second glow.  u_BloomRadius alone sets the
	 * half-brightness radius (how far it reaches) and u_BloomFalloff sets only the transition
	 * width w = 1/(1 + falloff) (edge sharpness); the 50% point stays at u_BloomRadius either
	 * way, so the two controls are orthogonal.  Only the annulus outside the disc is visible. */
	float e = max(r - u_DiscRadius, 0.0);
	float w = 1.0 / (1.0 + u_BloomFalloff);
	float bloom = 1.0 - smoothstep(u_BloomRadius * (1.0 - w), u_BloomRadius * (1.0 + w), e);

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
	 * blend set up by the caller).  Clamp per channel rather than tonemapping: a bright channel
	 * saturates to white (the hot centre) while the dim limb keeps its true hue -- a per-channel
	 * tonemap would boost midtones and shift the colour yellow, and its x^2 term overflows to
	 * NaN at extreme brightness. */
	vec3 emission = mix(bloom_emission, core_emission, core);
	float coverage = clamp(core, 0.0, 1.0);
	f_FragColor = clamp(vec4(emission, coverage), 0.0, 1.0);
}
