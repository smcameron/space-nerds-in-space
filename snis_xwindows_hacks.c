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
#include <stdio.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xos.h>
#include <SDL2/SDL_syswm.h> /* for SDL_GetWindowWMInfo */

#include "snis_xwindows_hacks.h"

/* This is a series of hacks to make up for SDL2's deficiencies
 * when running under X11.
 */

/* SDL2 doesn't have a reasonable way to constrain a window to a chosen aspect ratio */
void constrain_aspect_ratio_via_xlib(SDL_Window *window, int w, int h)
{
	SDL_SysWMinfo info;
	Display *display;
	Window xwindow;
	long supplied_return;
	XSizeHints *hints;
	Status s;

	SDL_VERSION(&info.version);
	if (!SDL_GetWindowWMInfo(window, &info)) {
		fprintf(stderr, "SDL_GetWindowWMInfo failed.\n");
		return;
	}

	if (info.subsystem != SDL_SYSWM_X11) {
		fprintf(stderr, "Apparently not X11, no aspect ratio constraining for you!\n");
		return;
	}
	display = info.info.x11.display;
	xwindow = info.info.x11.window;
	hints = XAllocSizeHints();
	if (!hints) {
		fprintf(stderr, "Failed to allocate size hints\n");
		return;
	}
	s = XGetWMSizeHints(display, xwindow, hints, &supplied_return, XA_WM_SIZE_HINTS);
	if (s) {
		fprintf(stderr, "XGetWMSizeHints failed\n");
		XFree(hints);
		return;
	}
	hints->min_aspect.x = w;
	hints->min_aspect.y = h;
	hints->max_aspect.x = w;
	hints->max_aspect.y = h;
	hints->flags = PAspect;
	XSetWMNormalHints(display, xwindow, hints);
	XFree(hints);
}

#else
void constrain_aspect_ratio_via_xlib() {}
#endif
