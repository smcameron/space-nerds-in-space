uniform mat4 u_MVPMatrix;
uniform mat4 u_MVMatrix;

attribute vec4 a_Position;
attribute vec3 a_Normal;
attribute vec2 a_TexCoord;

varying vec2 v_TexCoord;
varying vec3 v_ViewPos;
varying vec3 v_Normal;

void main()
{
	v_TexCoord = a_TexCoord;
	v_ViewPos = (u_MVMatrix * a_Position).xyz;
	v_Normal = mat3(u_MVMatrix[0].xyz, u_MVMatrix[1].xyz, u_MVMatrix[2].xyz) * a_Normal;
	gl_Position = u_MVPMatrix * a_Position;
}
