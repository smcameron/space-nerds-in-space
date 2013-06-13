/*
    (C) Copyright 2007,2008, Stephen M. Cameron.

    This file is part of Space Nerds In Space.

    wordwarvi is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    Space Nerds In Space is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Space Nerds In Space; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

 */
#ifndef __JOYSTICK_H__
#define __JOYSTICK_H__

#include <stdio.h>
#ifdef __WIN32__
#include <gdk/gdktypes.h>
#endif

#define JOYSTICK_DEVNAME "/dev/input/js0"

#define JS_EVENT_BUTTON         0x01    /* button pressed/released */
#define JS_EVENT_AXIS           0x02    /* joystick moved */
#define JS_EVENT_INIT           0x80    /* initial state of device */


struct js_event {
	unsigned int time;	/* event timestamp in milliseconds */
	short value;   /* value */
	unsigned char type;     /* event type */
	unsigned char number;   /* axis/button number */
};

struct wwvi_js_event {
	int button[11];
	int stick_x;
	int stick_y;
	int stick2_x;
	int stick2_y;
};

#ifdef __WIN32__
extern int open_joystick(char *joystick_device, GdkWindow *window);
#else
extern int open_joystick(char *joystick_device, void *window);
#endif
extern int read_joystick_event(struct js_event *jse);
extern void set_joystick_y_axis(int axis);
extern void set_joystick_x_axis(int axis);
extern void close_joystick();
extern int get_joystick_status(struct wwvi_js_event *wjse);

#endif
