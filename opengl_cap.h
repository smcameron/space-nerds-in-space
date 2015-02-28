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

#ifndef INCLUDE_opengl_cap_h
#define INCLUDE_opengl_cap_h

#include <GL/glew.h>

extern int msaa_framebuffer_supported();

extern int msaa_render_to_fbo_supported();

extern int msaa_max_samples();

extern int fbo_render_to_texture_supported();

extern int framebuffer_srgb_supported();

extern int texture_srgb_supported();

#endif
