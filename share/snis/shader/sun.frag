
uniform vec3 u_Color;         // disc colour
uniform vec3 u_BloomColor;    // additive bloom colour
uniform float u_DiscRadius;   // disc radius in UV (0..0.5); world-scale, set per frame
uniform float u_BloomIntensity;
uniform float u_BloomFalloff;  // higher = tighter bloom

in vec2 v_TexCoord;

out vec4 f_FragColor;

void main()
{
	/* Distance from the billboard centre in UV space (0 at centre, ~0.5 at the edge). */
	float r = length(v_TexCoord - vec2(0.5));

	/* Solid disc within u_DiscRadius, with a short soft edge.  The disc radius is set per
	 * frame from the sun's world radius over the billboard's world size, so the disc stays
	 * world-scale (grows as the camera approaches). */
	float edge = 0.01 + 0.25 * u_DiscRadius;
	float disc = 1.0 - smoothstep(u_DiscRadius, u_DiscRadius + edge, r);

	/* Additive bloom running from the disc edge (u_DiscRadius) out to the billboard edge (0.5),
	 * brightest at the edge and fading outward.  Starting it at the disc edge (rather than the
	 * centre) keeps it from hiding behind the disc as the disc grows; the caller sizes the
	 * billboard so this ring is a constant on-screen width, so the bloom stays screen-scale. */
	float bloom_span = max(0.5 - u_DiscRadius, 0.001);
	float bloom_t = clamp((r - u_DiscRadius) / bloom_span, 0.0, 1.0);
	float bloom = pow(1.0 - bloom_t, u_BloomFalloff) * u_BloomIntensity;

	vec3 color = u_Color * disc + u_BloomColor * bloom;
	float a = clamp(disc + bloom, 0.0, 1.0);
	f_FragColor = vec4(color, a);
}
