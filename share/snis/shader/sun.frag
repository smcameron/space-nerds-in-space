
uniform vec3 u_Color;          // the star's chromaticity, brightest channel 1
uniform float u_Brightness;    // the star's surface brightness in LINEAR HDR units
uniform float u_DiscRadius;    // disc radius in UV (0..0.5); world-scale, set per frame
uniform float u_EdgeSoftness;  // alpha-only limb softness, as a fraction of the disc radius
uniform float u_PsfWidth;      // the optics' point spread width, in star radii
uniform float u_PsfFalloff;    // power-law exponent of the point spread function's tail

in vec2 v_TexCoord;

out vec4 f_FragColor;

/* There are deliberately NO diffraction spikes here.  Two versions were built and both were
 * rejected, for reasons worth recording so they are not rebuilt a third time.
 *
 * A hundred fine rays, taken from the shipped textures, is not diffraction at all -- spikes
 * come from a few support vanes, N of them giving N spikes when N is even and 2N when odd.
 * It also aliases: at a hundred spokes the angular period three star radii out is only about
 * five pixels at ordinary on-screen star sizes, so it shimmers whenever the camera moves.
 * The textures get away with it because they are prefiltered raster; a procedural shader
 * cannot without explicit anti-aliasing.
 *
 * Four or six vanes is physically right and looks it -- and looks like cinematic lens flare,
 * nothing like this game's hand-painted stars.  Shortened and weakened enough to match the
 * artwork, the spikes stop being worth their complexity.
 *
 * The painted rays are a painterly convention without a good procedural analogue at this
 * fidelity, so the star is drawn as its point spread function alone.
 */

/* A star, rendered the way a real HDR pipeline would: build the emission in LINEAR units,
 * at the star's true brightness, then run it through the same filmic curve the rest of the
 * scene uses.  Everything that makes a star look like a star falls out of that rather than
 * being dialled in separately.
 *
 * The emission is ONE profile -- the star's disc already convolved with the optics' point
 * spread function -- and not a hard disc with a halo added beside it.  That split was the
 * structural error in the previous version: it left the disc's own light unblurred, so the
 * limb was a cliff.  Measured against the true convolution, the old form fell from 0.95 to
 * 0.002 across the limb where the convolution passes smoothly through 0.43, a factor of
 * 445 in one texel.  At high brightness both sides of that cliff clipped to white and hid
 * it; as soon as the brightnesses were matched to the textures it showed as a flat, hard
 * edged disc.  Note that widening u_EdgeSoftness cannot fix it -- while the emission is
 * multiplied by u_Brightness, everything with coverage above 1/u_Brightness still clips to
 * white, so a softer edge only makes the white disc bigger.  Hence edge softness now
 * applies to the ALPHA alone, where it belongs: the star does occlude sharply.
 *
 * The tonemap is what makes the star's colour behave.  Near the centre every channel is
 * driven far past white and clips, so the core is white whatever colour the star is;
 * further out the dimmer channels fall back through the curve first, so the glow takes on
 * the star's hue and keeps gaining saturation outward.  A hard clamp cannot do this.
 *
 * The profile is a power-law tail, which is what optical scatter actually produces.  Being
 * scale free is what lets one setting serve every star.  See section 5 of
 * doc/star-rendering-and-lighting-notes.txt for the fit and its limits.
 */
void main()
{
	/* Offset from the billboard centre in UV space, and the radius in STAR RADII. */
	float r = length(v_TexCoord - vec2(0.5));
	float s = r / max(u_DiscRadius, 1e-6);

	vec3 emission = u_Color * u_Brightness / (1.0 + pow(s / u_PsfWidth, u_PsfFalloff));

	/* Premultiplied alpha: the disc occludes what is behind it, the glow only adds light. */
	float edge = max(u_EdgeSoftness * u_DiscRadius, 0.001);
	float coverage = 1.0 - smoothstep(u_DiscRadius - edge, u_DiscRadius + edge, r);

	f_FragColor = clamp(filmic_tonemap(vec4(max(emission, vec3(0.0)), coverage)), 0.0, 1.0);
}
