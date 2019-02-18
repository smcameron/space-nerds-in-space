
uniform mat4 u_MVPMatrix;  // A constant representing the combined model/view/projection matrix.
uniform mat4 u_MVMatrix;   // A constant representing the combined model/view matrix.
uniform mat3 u_NormalMatrix;
uniform vec3 u_Color;      // Per-object color information we will pass in.
uniform vec4 u_ClipSphere; // clipping sphere, x,y,z=center in eye space

attribute vec4 a_Position; // Per-vertex position information we will pass in.
attribute vec3 a_Normal;   // Per-vertex normal information we will pass in.

varying vec2 v_FragRadius; // fragment distance to sphere center
varying float v_EyeDot;
varying float v_ClipSphereDot;

void main()
{
	vec4 pos = u_MVPMatrix * a_Position;

	// Transform the vertex into eye space.
	vec3 modelViewVertex = vec3(u_MVMatrix * a_Position);

	// Transform the normal's orientation into eye space.
	vec3 modelViewNormal = normalize(u_NormalMatrix * a_Normal);

	vec3 eyeVector = normalize(-modelViewVertex);
	vec3 clipSphereVector = normalize(u_ClipSphere.xyz - modelViewVertex);

	v_EyeDot = dot(modelViewNormal, eyeVector);
	v_ClipSphereDot = dot(modelViewNormal, clipSphereVector);

	// distance from this vertex and the center of our clipping sphere, using linear interp trick
	v_FragRadius = vec2(pos.w * distance(u_ClipSphere.xyz, modelViewVertex), 1.0 / pos.w);

	// gl_Position is a special variable used to store the final position.
	// Multiply the vertex by the matrix to get the final point in normalized screen coordinates.
	gl_Position = pos;
}

