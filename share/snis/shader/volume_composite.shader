/* A volume effect -- the fireball -- rendered at reduced resolution, laid back over the full
 * resolution frame.
 *
 * The buffer holds premultiplied colour and alpha, cleared to nothing, so an effect drawn into it
 * composites exactly as it would have drawn straight into the frame, and bilinear filtering on
 * the way up is correct for premultiplied values.  Blended by the caller with
 * GL_ONE, GL_ONE_MINUS_SRC_ALPHA.
 *
 * One triangle covering the window, its corners made from gl_VertexID: no vertex buffer.
 */

#if defined(INCLUDE_VS)

out vec2 v_Uv;

void main()
{
	vec2 corner[3] = vec2[3](vec2(-1.0, -1.0), vec2(3.0, -1.0), vec2(-1.0, 3.0));

	v_Uv = corner[gl_VertexID] * 0.5 + 0.5;
	gl_Position = vec4(corner[gl_VertexID], 0.0, 1.0);
}

#endif

#if defined(INCLUDE_FS)

uniform sampler2D u_Volume;

in vec2 v_Uv;
out vec4 f_FragColor;

void main()
{
	f_FragColor = texture(u_Volume, v_Uv);
}

#endif
