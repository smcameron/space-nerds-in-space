/* 
    (C) Copyright 2007,2008, Stephen M. Cameron.

    This file is part of wordwarvi.

    wordwarvi is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    wordwarvi is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with wordwarvi; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

 */

#include "compat.h"
#include "joystick.h"

#ifdef __WIN32__

#define DIRECTINPUT_VERSION 0x0800
#include <dinput.h>

/* window handle */
#include <gdk/gdkwin32.h>

/* joystick axes limits */
#define MIN_AXIS -32767
#define MAX_AXIS 32767

/* direct input interface */
LPDIRECTINPUT8 dinput = NULL;

/* joystick interface */
LPDIRECTINPUTDEVICE8 joystick = NULL;

/* device enumeration context */
struct DEVINFO
{
	GUID productGuid;
	GUID deviceID;
	const CHAR *vendor;
};

/* device enumeration callback */
BOOL CALLBACK EnumDevCallback(const DIDEVICEINSTANCE *dev, VOID *ctx)
{
	/* take the first joystick we get */
	if (GET_DIDEVICE_TYPE(dev->dwDevType) == DI8DEVTYPE_JOYSTICK ||
		GET_DIDEVICE_TYPE(dev->dwDevType) == DI8DEVTYPE_GAMEPAD)
	{
		struct DEVINFO *info = (struct DEVINFO *)ctx;
		info->productGuid = dev->guidProduct;
		info->deviceID = dev->guidInstance;
		info->vendor = dev->tszInstanceName;
		return DIENUM_STOP;
	}
	return DIENUM_CONTINUE;
}

/* device objects enumeration callback */
BOOL CALLBACK EnumObjectsCallback(const DIDEVICEOBJECTINSTANCE *devobj, VOID *ctx)
{
	/* set DIPROP_RANGE for the axis in order to scale min/max values */
	if (devobj->dwType & DIDFT_AXIS)
	{
		HRESULT hr;
		DIPROPRANGE dipr;

		dipr.diph.dwSize = sizeof(DIPROPRANGE);
		dipr.diph.dwHeaderSize = sizeof(DIPROPHEADER);
		dipr.diph.dwHow = DIPH_BYID;
		dipr.diph.dwObj = devobj->dwType;
		dipr.lMin = MIN_AXIS;
		dipr.lMax = MAX_AXIS;

		hr = IDirectInputDevice_SetProperty(joystick, DIPROP_RANGE, &dipr.diph);
		if (FAILED(hr))
			return DIENUM_STOP;
	}
	return DIENUM_CONTINUE;
}

int open_joystick(char *joystick_device, GdkWindow *window)
{
	HINSTANCE hi = 0;
	HRESULT hr = 0;
	HWND hwin = 0;
	struct DEVINFO devinfo = {0};

	/* create interface */
	hi = GetModuleHandle(NULL);
	hr = DirectInput8Create(hi, DIRECTINPUT_VERSION, &IID_IDirectInput8, (VOID**)&dinput, NULL);
	if (FAILED(hr))
		return -1;

	/* look for a joystick */
	hr = IDirectInput8_EnumDevices(dinput, DI8DEVCLASS_GAMECTRL, EnumDevCallback, &devinfo, DIEDFL_ATTACHEDONLY);
	if(FAILED(hr))
		return -1;

	/* obtain joystick interface */
	hr = IDirectInput8_CreateDevice(dinput, &devinfo.deviceID, &joystick, NULL);
	if(FAILED(hr))
		return -1;

	/* set data format to "simple joystick" */
	hr = IDirectInputDevice2_SetDataFormat(joystick, &c_dfDIJoystick2);
	if(FAILED(hr))
		return -1;

	/* set the cooperative level */
#ifdef __WIN32__
	hwin = GDK_WINDOW_HWND(window);
#endif
	hr = IDirectInputDevice2_SetCooperativeLevel(joystick, hwin, DISCL_EXCLUSIVE | DISCL_FOREGROUND);
	if(FAILED(hr))
		return -1;

	/* enumerate axes, buttons, povs */
	hr = IDirectInputDevice2_EnumObjects(joystick, EnumObjectsCallback, NULL, DIDFT_ALL);
	if(FAILED(hr))
		return -1;

	return 0;
}

int read_joystick_event(struct js_event *jse)
{
	/* not implemented */
	return 0;
}

void set_joystick_y_axis(int axis)
{
	/* fixme: add axis selection */
}

void set_joystick_x_axis(int axis)
{
	/* fixme: add axis selection */
}

void close_joystick()
{
	if(joystick) {
		IDirectInputDevice2_Unacquire(joystick);
		joystick = NULL;
	}

	if (dinput) {
		IDirectInput8_Release(dinput);
		dinput = NULL;
	}
}

int get_joystick_status(struct wwvi_js_event *wjse)
{
	HRESULT hr;
	DIJOYSTATE2 js;
	int i;

	if (!joystick || !wjse)
		return -1;

	/* poll the device to read the current state  */
	hr = IDirectInputDevice2_Poll(joystick);
	if (FAILED(hr)) {
		/* input stream has been interrupted, re-acquire and try again */
		hr = IDirectInputDevice2_Acquire(joystick);
		while(hr == DIERR_INPUTLOST)
			hr = IDirectInputDevice2_Acquire(joystick);

		/* other errors may occur when the app is minimized
		or in the process of switching, so just try again later */
		if (FAILED(hr))
			return -1;

		/* try to poll again */
		hr = IDirectInputDevice2_Poll(joystick);
		if (FAILED(hr))
			return -1;
	}

	/* get the device state */
	hr = IDirectInputDevice2_GetDeviceState(joystick, sizeof(DIJOYSTATE2), &js);
	if(FAILED(hr))
		return -1;

	/* write out state */
	wjse->stick_x = js.lX;
	wjse->stick_y = js.lY;
	for (i = 0; i < 11; ++i) {
		wjse->button[i] = (js.rgbButtons[i] & 0x80) ? 1 : 0;
	}

	return 0;
}

#else

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

static int joystick_fd = -1;

/* These are sensible on Logitech Dual Action Rumble and xbox360 controller. */
static int joystick_x_axis = 0;
static int joystick_y_axis = 1;

int open_joystick(char *joystick_device, __attribute__((unused)) void *window)
{
	if (joystick_device == NULL)
		joystick_device = JOYSTICK_DEVNAME;
	joystick_fd = open(joystick_device, JOYSTICK_DEV_OPEN_FLAGS);
	if (joystick_fd < 0)
		return joystick_fd;

	/* maybe ioctls to interrogate features here? */

	return joystick_fd;
}

int read_joystick_event(struct js_event *jse)
{
	int bytes;

	bytes = read(joystick_fd, jse, sizeof(*jse)); 

	if (bytes == -1)
		return 0;

	if (bytes == sizeof(*jse))
		return 1;

	printf("Unexpected bytes from joystick:%d\n", bytes);

	return -1;
}

void close_joystick()
{
	close(joystick_fd);
}

int get_joystick_status(struct wwvi_js_event *wjse)
{
	int rc;
	struct js_event jse;
	if (joystick_fd < 0)
		return -1;

	/* memset(wjse, 0, sizeof(*wjse)); */
	while ((rc = read_joystick_event(&jse) == 1)) {
		jse.type &= ~JS_EVENT_INIT; /* ignore synthetic events */
		if (jse.type == JS_EVENT_AXIS) {
			if (jse.number == joystick_x_axis)
				wjse->stick_x = jse.value;
			if (jse.number == joystick_y_axis)
				wjse->stick_y = jse.value;
		} else if (jse.type == JS_EVENT_BUTTON) {
			if (jse.number < 11) {
				switch (jse.value) {
				case 0:
				case 1: wjse->button[jse.number] = jse.value;
					break;
				default:
					break;
				}
			}
		}
	}
	/* printf("%d\n", wjse->stick1_y); */
	return 0;
}

void set_joystick_y_axis(int axis)
{
	joystick_y_axis = axis;
}

void set_joystick_x_axis(int axis)
{
	joystick_x_axis = axis;
}

#endif /* __WIN32__ */

#if 0
/* a little test program */
int main(int argc, char *argv[])
{
	int fd, rc;
	int done = 0;

	struct js_event jse;

	fd = open_joystick();
	if (fd < 0) {
		printf("open failed.\n");
		exit(1);
	}

	while (!done) {
		rc = read_joystick_event(&jse);
		usleep(1000);
		if (rc == 1) {
			printf("Event: time %8u, value %8hd, type: %3u, axis/button: %u\n", 
				jse.time, jse.value, jse.type, jse.number);
		}
	}
}
#endif
