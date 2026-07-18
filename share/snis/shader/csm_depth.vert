
/* Depth-only vertex shader used to render the shadow map from the light's
 * point of view.  u_ShadowMVP maps model space directly to the light's clip
 * space (orthographic projection * light view * model). */

uniform mat4 u_ShadowMVP;

in vec4 a_Position;

void main()
{
	gl_Position = u_ShadowMVP * a_Position;
}
