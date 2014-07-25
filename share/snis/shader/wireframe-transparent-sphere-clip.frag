
uniform vec3 u_Color; // Per-object color information we will pass in.
uniform vec4 u_ClipSphere; // clipping sphere, w=radius squared
uniform float u_ClipSphereRadiusFade; // percent fade to black distance past clip radius

varying vec2 v_FragRadius; // this fragments distance to sphere center
varying float v_EyeDot;
varying float v_ClipSphereDot;

#define CULL_BACKFACE_CLIP_AND_CAMERA 0

void main()
{
#if CULL_BACKFACE_CLIP_AND_CAMERA
	if (v_EyeDot <= 0 && v_ClipSphereDot <= 0)
#else
	if (v_EyeDot <= 0)
#endif
		discard;

	// reverse linear interp trick
	float frag_radius = v_FragRadius.x * v_FragRadius.y;

	if (frag_radius > u_ClipSphere.w * (1.0 + u_ClipSphereRadiusFade))
		discard;

	float t = clamp(1.0 - (frag_radius - u_ClipSphere.w) / (u_ClipSphere.w * u_ClipSphereRadiusFade), 0.0, 1.0);

	gl_FragColor = vec4(mix(vec3(0), u_Color, t), 1);
}

