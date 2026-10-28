/*
	This file (joystick_web.c) is entirely machine-generated. To the extent
	possible under law, the author(s) have dedicated all copyright and related and
	neighboring rights to this software to the public domain worldwide.  This
	software is distributed without any warranty.

	You should have received a copy of the CC0 Public Domain Dedication along with
	this software. If not, see <http://creativecommons.org>.
*/

/*
 * Browser implementation of joystick.h, used instead of joystick.c when
 * building with "make WEB=1".
 *
 * A browser has no /dev/input, but SDL reads gamepads and joysticks through
 * the browser's Gamepad API.  A browser only reveals a gamepad once the
 * player presses one of its buttons, which is usually after snis_client has
 * looked for joysticks at startup.  So this always reports one joystick,
 * named "web-gamepad", which reads the first gamepad SDL knows of, and
 * reads as centred, with nothing pressed, until there is one.
 *
 * Browsers present most gamepads in the "standard gamepad" layout, so one
 * entry in joystick_config.txt, for "web-gamepad", covers them.
 */

#include <string.h>
#include <stdlib.h>

#include <SDL.h>

#include "joystick.h"

#define WEB_GAMEPAD_NAME "web-gamepad"

static SDL_Joystick *gamepad;

int discover_joysticks(struct joystick_descriptor **joystick, int *njoysticks)
{
	if (SDL_InitSubSystem(SDL_INIT_JOYSTICK) != 0)
		return -1;
	*joystick = calloc(1, sizeof(**joystick));
	if (!*joystick)
		return -1;
	(*joystick)[0].name = strdup(WEB_GAMEPAD_NAME);
	*njoysticks = 1;
	return 0;
}

int open_joystick(__attribute__((unused)) char *joystick_device,
		__attribute__((unused)) void *window)
{
	return 0;
}

int read_joystick_event(__attribute__((unused)) int fd,
		__attribute__((unused)) struct js_event *jse)
{
	return 0;
}

void close_joystick(__attribute__((unused)) int fd)
{
	if (gamepad)
		SDL_JoystickClose(gamepad);
	gamepad = NULL;
}

int get_joystick_state(__attribute__((unused)) int fd, struct js_state *state)
{
	int i, n;

	SDL_JoystickUpdate();
	if (gamepad && !SDL_JoystickGetAttached(gamepad)) {
		SDL_JoystickClose(gamepad);
		gamepad = NULL;
	}
	if (!gamepad && SDL_NumJoysticks() > 0)
		gamepad = SDL_JoystickOpen(0);
	if (!gamepad) {
		memset(state->axis, 0, sizeof(state->axis));
		memset(state->button, 0, sizeof(state->button));
		return 0;
	}
	n = SDL_JoystickNumAxes(gamepad);
	for (i = 0; i < n && i < MAX_JOYSTICK_AXES; i++)
		state->axis[i] = SDL_JoystickGetAxis(gamepad, i);
	n = SDL_JoystickNumButtons(gamepad);
	for (i = 0; i < n && i < MAX_JOYSTICK_BUTTONS; i++)
		state->button[i] = SDL_JoystickGetButton(gamepad, i);
	return 0;
}
