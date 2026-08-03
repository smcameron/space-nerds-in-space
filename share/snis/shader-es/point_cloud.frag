
uniform vec4 u_Color;      // Per-object color information we will pass in.

varying float v_Alpha;
varying vec3 v_Tint;
varying float v_Round;

void main()
{
	float shape = 1.0;

	if (v_Round > 0.0) {
		/* A point sprite is a square, which is plainly a square once a star is more than
		 * a couple of pixels across.  gl_PointCoord runs 0..1 over the sprite, so measure
		 * out from its centre and fall off before the corners.  The falloff starts well
		 * inside the edge: a hard cut would just trade a square for an aliased circle,
		 * and the soft rim doubles as the antialiasing these have never had. */
		float d = length(gl_PointCoord - vec2(0.5)) * 2.0;
		shape = 1.0 - smoothstep(0.45, 1.0, d);
	}

	/* Premultiplied: this is drawn with GL_ONE / GL_ONE_MINUS_SRC_ALPHA when blended. */
	gl_FragColor = vec4(u_Color.rgb * v_Tint * v_Alpha * shape, u_Color.a * v_Alpha * shape);
}
