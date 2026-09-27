uniform mat4 u_MVPMatrix;
uniform mat4 u_MVMatrix;

attribute vec4 a_Position;
attribute vec3 a_Normal;
attribute vec2 a_TexCoord;

varying vec2 v_TexCoord;
varying vec3 v_ViewPos;
varying vec3 v_Normal;
varying vec3 v_ViewAxis;

void main()
{
	mat3 mv3 = mat3(u_MVMatrix[0].xyz, u_MVMatrix[1].xyz, u_MVMatrix[2].xyz);
	v_TexCoord = a_TexCoord;
	v_ViewPos = (u_MVMatrix * a_Position).xyz;
	v_Normal = mv3 * a_Normal;
	v_ViewAxis = normalize(mv3 * vec3(-1.0, 0.0, 0.0));
	gl_Position = u_MVPMatrix * a_Position;
}
