
uniform mat4 u_MVPMatrix;  // A constant representing the combined model/view/projection matrix.
uniform float u_PointSize;
uniform vec4 u_Color;
uniform float u_Time; // 0 - 1 in seconds period

attribute vec4 a_Position; // Per-vertex position information we will pass in.

varying vec4 v_Color;

/* Return a pseudo-random number 0 - 1 for a two element seed
   http://stackoverflow.com/questions/12964279/whats-the-origin-of-this-glsl-rand-one-liner */
float rand(vec2 co)
{
	return fract(sin(dot(co.xy, vec2(12.9898,78.233))) * 43758.5453);
}

void main()
{
	vec3 base_color = u_Color.rgb * rand(vec2(u_Time, fract(abs(a_Position.x + a_Position.y + a_Position.z))));
	v_Color = vec4(base_color, u_Color.a);

	gl_PointSize = u_PointSize;
	gl_Position = u_MVPMatrix * a_Position;
}

