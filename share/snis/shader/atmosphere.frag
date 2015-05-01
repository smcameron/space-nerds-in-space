
uniform vec3 u_LightPos;       // The position of the light in eye space.

varying vec3 v_Position;       // Interpolated position for this fragment.
varying vec3 v_Color;          // This is the color from the vertex shader interpolated across the triangle per fragment
varying vec3 v_Normal;         // Interpolated normal for this fragment.

#if !defined(AMBIENT)
#define AMBIENT 0.01
#endif

void main()
{
	// Get a lighting direction vector from the light to the vertex.
	vec3 lightVector = normalize(u_LightPos - v_Position);

	// Calculate the dot product of the light vector and vertex normal
	// and subtract it from 1.  This gives a very crude approximation of
	// how much atmosphere you are looking through, and thus an extremely
	// crude approximation of how much scattered light you would see
	// assuming uniform illumination.
	float eyedot = (1.0 - abs(dot(normalize(v_Normal), normalize(v_Position))));
	float eyedot2 = eyedot * eyedot;

	// Dot product of surface normal with light vector is how much light is
	// reflected, combine (mulitply) this with eyedot to get a crude approximation
	// of scattered light.
	float lightdot = dot(normalize(v_Normal), normalize(lightVector));

	// As atmosphere thins out at edges, scattered light must fall off.
	// Here is a super duper crude way to do that:
	float attenuation = min((1.0 - eyedot), 0.25);
	attenuation = 15.0 * attenuation * attenuation;

	vec3 xColor = vec3(0.7, 0.7, 1.0); // kinda blue-ish

	gl_FragColor = vec4(xColor * attenuation * lightdot * eyedot2 * 0.7,
					attenuation * lightdot * eyedot2 * 0.7);
}

