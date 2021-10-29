/*
	Copyright (C) 2020 Stephen M. Cameron
	Author: Stephen M. Cameron

	This file is part of Spacenerds In Space.

	Spacenerds in Space is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.

	Spacenerds in Space is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with Spacenerds in Space; if not, write to the Free Software
	Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/
#if USE_SNIS_XWINDOWS_HACKS
#include <SDL2/SDL.h>

/* This is a series of hacks to make up for SDL2's deficiencies
 * when running under X11.
 */

/* SDL2 cannot reasonably constrain the aspect ratio of a window.
 * This uses xlib to do it via hints.  It returns 0 on success, -1
 * on failure.
 */
int constrain_aspect_ratio_via_xlib(SDL_Window *window, int w, int h);
#else
int constrain_aspect_ratio_via_xlib();
#endif

