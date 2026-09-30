/* GLES (GLSL 100) version of share/snis/shader/volume_composite.shader: keep the two in step.
 *
 * A volume effect -- the fireball -- rendered at reduced resolution, laid back over the full
 * resolution frame.
 *
 * The buffer holds premultiplied colour and alpha, cleared to nothing, so an effect drawn into it
 * composites exactly as it would have drawn straight into the frame, and bilinear filtering on
 * the way up is correct for premultiplied values.  Blended by the caller with
 * GL_ONE, GL_ONE_MINUS_SRC_ALPHA.
 *
 * One triangle covering the window.  GLSL 100 has no gl_VertexID to make its corners from, so
 * they come from a small vertex buffer, a_Corner: (-1, -1), (3, -1), (-1, 3).
 */

#if defined(INCLUDE_VS)

attribute vec2 a_Corner;

varying vec2 v_Uv;

void main()
{
	v_Uv = a_Corner * 0.5 + 0.5;
	gl_Position = vec4(a_Corner, 0.0, 1.0);
}

#endif

#if defined(INCLUDE_FS)

uniform sampler2D u_Volume;

varying vec2 v_Uv;

void main()
{
	gl_FragColor = texture2D(u_Volume, v_Uv);
}

#endif
