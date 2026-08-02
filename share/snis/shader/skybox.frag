
uniform samplerCube s_texture;

/* Gravitational lensing.  u_LensDir[i] is a unit vector from the camera toward a black hole;
 * u_LensParams[i] is (angular Einstein radius, angular radius of the opaque disc, swirl).  A
 * slot with a zero Einstein radius contributes exactly nothing, which is how unused ones are
 * disabled: GLSL ES 1.00 wants a constant loop bound, so every slot is always visited and
 * there is nothing to branch around.  See graph_dev_set_gravitational_lenses(). */
uniform vec3 u_LensDir[MAX_GRAVITATIONAL_LENSES];
uniform vec3 u_LensParams[MAX_GRAVITATIONAL_LENSES];

in vec3 texCoord;

out vec4 f_FragColor;

void main (void) {
	vec3 dir = normalize(texCoord);
	vec3 disp = vec3(0.0);
	int i;

	/* Uniform, so the branch is coherent across the whole draw: with no black hole in view
	 * the skybox costs exactly what it always did.  The caller packs active lenses first. */
	if (u_LensParams[0].x > 0.0) {
		for (i = 0; i < MAX_GRAVITATIONAL_LENSES; i++) {
			vec3 b = u_LensDir[i];
			float einstein = u_LensParams[i].x;
			float esq = einstein * einstein;

			/* Component of the view direction perpendicular to the lens direction.  Its
			 * length is sin(theta), which stands in for the angle theta itself: that is
			 * good to well under a percent wherever the deflection is large enough to
			 * see, and it keeps this loop free of transcendentals entirely. */
			vec3 p = dir - b * dot(dir, b);
			/* Floor theta so the divide is safe and so the singularity at dead centre
			 * stays underneath the black hole's own billboard, which is drawn over it. */
			float theta = max(length(p), max(u_LensParams[i].y, 0.0001));
			vec3 phat = p / theta;

			/* Frame dragging: swing the deflection sideways, hardest near the hole and
			 * falling off as 1/theta^2, so the arcs spiral instead of sitting in neat
			 * radial symmetry.  Leaning on normalize() rather than a rotation by sin/cos
			 * costs nothing and turns the angle into atan(twist), which saturates rather
			 * than spinning without limit as theta shrinks.  The strength is a by-eye
			 * knob either way, so the remapping is free. */
			vec3 axis = phat + cross(b, phat) * (u_LensParams[i].z * esq / (theta * theta));

			/* Thin-lens deflection.  Light arriving along theta left the sky at
			 * theta - einstein^2 / theta, so pull the sample that far toward the hole.
			 * Everything interesting falls out of this one term: far away it vanishes and
			 * the sky is untouched; at theta == einstein the sample lands dead centre, so
			 * the single point behind the hole smears around the whole circle and the
			 * Einstein ring appears without being drawn; inside that the offset overshoots,
			 * carrying the sample across to the far side, which is the inverted second
			 * image -- no branch and no second texture fetch for any of it.
			 *
			 * Accumulating a displacement and normalizing once at the end (rather than
			 * bending dir per lens) is what lets several lenses superpose without any
			 * data-dependent control flow, which GLSL ES 1.00 will not give us. */
			disp -= axis * (esq / (theta * max(length(axis), 0.0001)));
		}
		dir = normalize(dir + disp);
	}

	f_FragColor = filmic_tonemap(texture(s_texture, dir));
}
