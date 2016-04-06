/*
	(c) copyright 2014 jeremy van grinsven

	this file is part of space nerds in space.

	space nerds in space is free software; you can redistribute it and/or modify
	it under the terms of the gnu general public license as published by
	the free software foundation; either version 2 of the license, or
	(at your option) any later version.

	space nerds in space is distributed in the hope that it will be useful,
	but without any warranty; without even the implied warranty of
	merchantability or fitness for a particular purpose.  see the
	gnu general public license for more details.

	you should have received a copy of the gnu general public license
	along with space nerds in space; if not, write to the free software
	foundation, inc., 51 franklin st, fifth floor, boston, ma  02110-1301  usa
*/

#include <GL/glew.h>
#include "opengl_cap.h"

#include <stdlib.h>

int msaa_framebuffer_supported()
{
	static int suppress = -1;
	if (suppress == -1) {
		if (getenv("SNIS_SUPPRESS_MSAA") != NULL)
			suppress = 1;
		else
			suppress = 0;
	}
	if (suppress)
		return 0;
	return GLEW_ARB_multisample;
}

int msaa_render_to_fbo_supported()
{
	static int suppress = -1;
	if (suppress == -1) {
		if (getenv("SNIS_SUPPRESS_FBO") != NULL)
			suppress = 1;
		else
			suppress = 0;
	}
	if (suppress)
		return 0;
	return GLEW_EXT_framebuffer_multisample && msaa_max_samples() > 0;
}

int msaa_max_samples()
{
	GLint samples = 0;
	glGetIntegerv(GL_MAX_SAMPLES, &samples);
	return samples;
}

int fbo_render_to_texture_supported()
{
	static int suppress = -1;
	if (suppress == -1) {
		if (getenv("SNIS_SUPPRESS_RENDER_TO_TEXTURE") != NULL)
			suppress = 1;
		else
			suppress = 0;
	}
	if (suppress)
		return 0;
	return GLEW_EXT_framebuffer_object && GLEW_EXT_framebuffer_blit;
}

int framebuffer_srgb_supported()
{
	/* see if extension exists */
	if (!GLEW_EXT_framebuffer_sRGB)
		return 0;

	/* test the current framebuffer if it is capable */
	GLboolean boolmode;
	glGetBooleanv(GL_FRAMEBUFFER_SRGB_CAPABLE_EXT, &boolmode);
	GLenum ret = glGetError();
	if (ret != 0)
		return 0;

	return boolmode;
}

int texture_srgb_supported()
{
	return GLEW_EXT_texture_sRGB;
}


