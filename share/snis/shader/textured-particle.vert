
uniform mat4 u_MVPMatrix;		// A constant representing the combined model/view/projection matrix.

uniform vec3 u_CameraUpVec;		// model space camera up vector
uniform vec3 u_CameraRightVec;		// model space camera right vector

uniform float u_Time;			// 0.0 to 1.0 animation position
uniform float u_Radius;			// size of billboard vertex offset

attribute vec4 a_MultiOne;		// 0,1=texture coordinate 0,1; 2=time offset

attribute vec3 a_StartPosition;		// start position
attribute vec3 a_StartTintColor;	// start tint color
attribute vec2 a_StartAPM;		// start APM 0=additivity; 1=opacity
attribute vec3 a_EndPosition;		// end position
attribute vec3 a_EndTintColor;		// end tint color
attribute vec2 a_EndAPM;		// end APM 0=additivity; 1=opacity

varying vec2 v_TexCoord;		// texture coordinate passed to fragment shader
varying vec4 v_TintColor;		// tint color passed to fragment shader

void main()
{
	// unpack attributes
	vec2 texture_coord = a_MultiOne.xy;
	float time_offset = a_MultiOne.z;

	// calulate the time based on current and offset, wrapping around if greater than 1
	float time = u_Time + time_offset;
	if (time > 1.0) time -= 1.0;

	v_TexCoord = texture_coord;

	v_TintColor.rgb = mix(a_StartTintColor, a_EndTintColor, time);
	v_TintColor.a = 1.0 - mix(a_StartAPM.x, a_EndAPM.x, time); // additivity
	v_TintColor *= mix(a_StartAPM.y, a_EndAPM.y, time); // opacity

	// calculate the vertex based on the texture coord showing which way to offset
	float diameter = u_Radius * 2.0;
	vec3 cornerPosition = mix(a_StartPosition, a_EndPosition, time) +
		((texture_coord.x - 0.5) * diameter) * u_CameraRightVec +
		((texture_coord.y - 0.5) * diameter) * u_CameraUpVec;

	gl_Position = u_MVPMatrix * vec4(cornerPosition, 1.0);
}

