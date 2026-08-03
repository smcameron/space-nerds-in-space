uniform mat4 u_MVPMatrix;  // A constant representing the combined model/view/projection matrix.
uniform float u_PointSize;

/* Star-field extras.  These are inert for ordinary point clouds: u_FadeParams.w at or below
 * zero disables the whole block, leaving a flat field of same-size, same-colour points.
 *
 * u_CameraPos is world space, and so are the vertices, so the vertex shader can work out
 * how far away each star is.  u_FadeParams is (near0, near1, far0, far1): fade up from
 * near0 to near1, hold, then fade down from far0 to far1.  The far end hides stars
 * recycling in and out at the shell boundary.  The near end matters more than it looks --
 * a star that passes close to the camera sweeps quickly and reads as debris, and fading it
 * out as it approaches is the cheapest honest answer, since a star that close is not a
 * star.
 *
 * Brightness, size and colour vary per star, hashed from its own position rather than
 * carried as a vertex attribute: no change to the vertex format, and a recycled star is a
 * different star anyway, so its re-roll is not a discontinuity.  The magnitude is skewed so
 * that faint stars are common and bright ones rare, which is roughly how the sky looks. */
uniform vec3 u_CameraPos;
uniform vec4 u_FadeParams;

in vec4 a_Position; // Per-vertex position information we will pass in.

out float v_Alpha;
out vec3 v_Tint;
out float v_Round;

void main()
{
	float size = u_PointSize;
	v_Alpha = 1.0;
	v_Tint = vec3(1.0);
	/* Only the star field asks to be round; every other point cloud on this shader
	 * keeps the square sprites it has always drawn. */
	v_Round = 0.0;

	if (u_FadeParams.w > 0.0) {
		float dist = length(a_Position.xyz - u_CameraPos);
		float h = fract(sin(dot(a_Position.xyz, vec3(12.9898, 78.233, 37.719))) * 43758.5453);
		float h2 = fract(h * 7919.0);
		/* Two different skews off the same hash, so a bigger star is also a brighter
		 * one.  Size is skewed hard: the median star is barely over a pixel and only a
		 * few percent are large, which is what keeps a dense field from turning into
		 * porridge.  Brightness is skewed far less, or nearly everything lands under
		 * the visible threshold once the fade has taken its cut. */
		float mag = pow(h, 4.0);
		float bright = pow(h, 1.5);
		float fade = smoothstep(u_FadeParams.x, u_FadeParams.y, dist) *
				(1.0 - smoothstep(u_FadeParams.z, u_FadeParams.w, dist));

		/* The 1.3 is a straight brightness lift, clamped because the bright end
		 * would otherwise run past white.  About a quarter of the stars saturate at
		 * the top, which costs nothing: size still tells them apart, and a star that
		 * bright saturating is what a star that bright does. */
		v_Alpha = clamp((0.30 + 0.70 * bright) * 1.3, 0.0, 1.0) * fade;
		/* A crude main-sequence spread, warm through white to blue. */
		v_Tint = mix(vec3(1.0, 0.80, 0.65), vec3(0.72, 0.83, 1.0), h2);
		size = u_PointSize * (0.5 + 3.5 * mag);
		v_Round = 1.0;
	}

	gl_PointSize = size;
	gl_Position = u_MVPMatrix * a_Position;
}
