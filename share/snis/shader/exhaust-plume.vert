uniform mat4 u_MVPMatrix;
uniform mat4 u_MVMatrix;

in vec4 a_Position;
in vec3 a_Normal;
in vec2 a_TexCoord;

out vec2 v_TexCoord;
out vec3 v_ViewPos;
out vec3 v_Normal;

void main()
{
	v_TexCoord = a_TexCoord;
	v_ViewPos = (u_MVMatrix * a_Position).xyz;
	v_Normal = mat3(u_MVMatrix) * a_Normal;
	gl_Position = u_MVPMatrix * a_Position;
}
