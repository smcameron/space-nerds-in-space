/* 
	(C) Copyright 2007,2008, Stephen M. Cameron.

	This file is part of Space Nerds In Space.

	Space Nerds In Space is free software; you can redistribute it and/or modify
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

#include "compat.h"
#include "joystick.h"

#ifdef __WIN32__ /* Pretty sure all the windows code is broken now. */

#define DIRECTINPUT_VERSION 0x0800
#include <dinput.h>

/* window handle */
#include <gdk/gdkwin32.h>

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
		dipr.lMin = JOYSTICK_AXIS_MIN;
		dipr.lMax = JOYSTICK_AXIS_MAX;

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
#include <dirent.h>
#include <limits.h>
#include <string.h>
#include <errno.h>

int open_joystick(char *joystick_device, __attribute__((unused)) void *window)
{
	int joystick_fd;
	joystick_fd = open(joystick_device, JOYSTICK_DEV_OPEN_FLAGS);
	if (joystick_fd < 0)
		return joystick_fd;

	/* maybe ioctls to interrogate features here? */

	return joystick_fd;
}

int read_joystick_event(int fd, struct js_event *jse)
{
	int bytes;

	bytes = read(fd, jse, sizeof(*jse));

	if (bytes == -1)
		return 0;

	if (bytes == sizeof(*jse))
		return 1;

	printf("Unexpected bytes from joystick:%d\n", bytes);

	return -1;
}

void close_joystick(int fd)
{
	close(fd);
}

int get_joystick_state(int fd, struct js_state *state)
{
	int rc;
	struct js_event jse;

	if (fd < 0)
		return -1;
	while ((rc = read_joystick_event(fd, &jse) == 1)) {
		jse.type &= ~JS_EVENT_INIT; /* ignore synthetic events */
		if (jse.type == JS_EVENT_AXIS) {
			if (jse.number < MAX_JOYSTICK_AXES)
				state->axis[jse.number] = jse.value;
		} else if (jse.type == JS_EVENT_BUTTON) {
			if (jse.number < MAX_JOYSTICK_BUTTONS) {
				switch (jse.value) {
				case 0:
				case 1:
					state->button[jse.number] = jse.value;
					break;
				default:
					break;
				}
			}
		}
	}
	return 0;
}

static int joystick_name_filter(const struct dirent *entry)
{
	const char *must_match = "-joystick"; /* old interface */
	const char *must_not_match = "-event-joystick"; /* new evdev interface */
	const char *start1, *start2;

	/* name too short to possibly be a joystick */
	if (strlen(entry->d_name) < strlen(must_match))
		return 0;

	/* Check that name ends in "-joystick" */
	start1 = entry->d_name + strlen(entry->d_name) - strlen(must_match);
	if (strncmp(start1, must_match, strlen(must_match)) != 0)
		return 0;

	/* Check that name does not end in "-event--joystick" (skip evdev interface) */
	if (strlen(entry->d_name) < strlen(must_not_match))
		return 1;
	start2 = entry->d_name + strlen(entry->d_name) - strlen(must_not_match);
	if (strncmp(start2, must_not_match, strlen(must_match)) == 0)
		return 0;

	/* Name ends in "-joystick" and does not end in "-event-joystick", so
	 * we think it is a device we want to talk to.
	 */
	return 1;
}

int discover_joysticks(struct joystick_descriptor **joystick, int *njoysticks)
{
	struct dirent **namelist;
	int i, rc;

	/* Joysticks show up in /dev/input/by-id/usb-Thrustmaster_T.16000M-joystick -> ../js0
	 * and also using new evdev interface as /dev/input/by-id/usb-Thrustmaster_T.16000M-event-joystick
	 * I'm ignoring the evdev interface and just using the old interface. I notice that the
	 * thrustmaster device seems to show up without any instance information in the name. I
	 * wonder what will happen if you plug two of them in? What will udev do? I suppose I will cross
	 * that bridge when/if I come to it.
	 */
	rc = scandir("/dev/input/by-id", &namelist, joystick_name_filter, alphasort);
	if (rc < 0) {
		fprintf(stderr, "scandir failed: %s\n", strerror(errno));
		return rc;
	}

	*joystick = malloc(sizeof(**joystick) * rc);
	memset(*joystick, 0, sizeof(**joystick) * rc);
	for (i = 0; i < rc; i++) {
		printf("%s\n", namelist[i]->d_name);
		(*joystick)[i].name = namelist[i]->d_name;
	}
	free(namelist);
	*njoysticks = rc;
	return 0;
}

#endif /* __WIN32__ */

#ifdef JOYSTICK_TEST
/* a little test program */
int main(int argc, char *argv[])
{
	int i, rc;
	int done = 0;
	struct js_event jse;
	struct joystick_descriptor *joystick;
	int njoysticks;
	int *fd;
	char filename[PATH_MAX];

	if (discover_joysticks(&joystick, &njoysticks) != 0) {
		fprintf(stderr, "Failed to discover joysticks.\n");
		return -1;
	}

	fd = malloc(sizeof(*fd) * njoysticks);
	printf("Discovered %d joysticks\n", njoysticks);
	for (i = 0; i < njoysticks; i++) {
		printf("%3d: %s\n", i, joystick[i].name);
		snprintf(filename, PATH_MAX, "%s/%s", "/dev/input/by-id",
			joystick[i].name);
		fd[i] = open_joystick(filename, NULL);
		if (fd[i] < 0) {
			fprintf(stderr, "Failed to open device %s: %s\n",
				filename, strerror(errno));
		}
	}

	while (!done) {
		for (i = 0; i < njoysticks; i++) {
			rc = read_joystick_event(fd[i], &jse);
			if (rc == 1) {
				printf("Device: %d, time %8u, value %8hd, type: %3u, axis/button: %u\n",
					i, jse.time, jse.value, jse.type, jse.number);
			}
		}
		usleep(1000);
	}
	free(fd);
}
#endif
