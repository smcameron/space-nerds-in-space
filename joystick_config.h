#ifndef JOYSTICK_CONFIG_H_
#define JOYSTICK_CONFIG_H_
/*
	Copyright (C) 2017 Stephen M. Cameron
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

/* This provides a way to read a joystick config from a file.
 * It does not know anything about hardware, it's strictly about
 * the config, that is, providing a configurable mapping from (device, button/axis)
 * tuples to C functions.
 *
 * The config file should look something like this:
 *
 * ----8<-----8<------
 * # Joystick config file
 * device:my_joystick
 *  mode 1 axis 1 secondary-pitch
 *  mode 1 axis 2 secondary-yaw
 *  mode 1 axis 3 secondary-roll
 *  mode 0 axis 1 pitch
 *  mode 0 axis 2 yaw
 *  mode 0 axis 3 roll
 *  mode 0 button 1 shoot
 *  mode 0 button 2 thrust 0
 * device:my_other_joystick
 *  mode 0 axis 1 yaw
 *  mode 0 axis 2 pitch
 *  mode 0 axis 3 roll
 *  mode 0 button 0 shoot
 *  mode 0 button 1 thrust
 *  mode 1 axis 1 secondary-pitch
 *  mode 1 axis 2 secondary-yaw
 *  mode 1 axis 3 secondary-roll
 * ----8<-----8<------
 *
 * The "my_joystick" and "my_other_joystick" must match entries in joysticks_found[]
 * passed into read_joystick_config.  Most typically, they will be the strings found
 * in filenames within /dev/input/by-id/, E.g. "usb-Thrustmaster_T.16000M-joystick".
 *
 * The "yaw", "pitch", "roll", "shoot", "thrust", etc. must match the "function_name" parameters
 * passed  into set_joystick_axis_fn() and set_joystick_button_fn() functions. These are
 * made up by you to fit the purposes of your program.
 *
 * An optional deadzone value may be appended to axis mappings. The default is 6000.
 * It is common to use a deadzone of 0 for e.g., throttle mappings.
 *
 * The mode numbers are a way to have the same button do different things in different
 * situations. For example, maybe you have a "tank" and an "airplane" mode, and when
 * in tank mode, tilting the joystick left or right yaws the tank, but in airplane
 * mode it rolls the plane.
 *
 * The general sequence is:
 *
 * // get a new empty config
 * struct joystick_config *cfg = new_joystick_config();
 *
 * // Set up a mapping of string names to functions to be called
 * set_joystick_axis_fn(cfg, "yaw", my_yaw_fn);
 * set_joystick_axis_fn(cfg, "roll", my_roll_fn);
 * set_joystick_axis_fn(cfg, "pitch", my_pitch_fn);
 * set_joystick_axis_fn(cfg, "blah", my_blah_axis_fn);
 * set_joystick_button_fn(cfg, "shoot", my_shoot_fn);
 * set_joystick_button_fn(cfg, "thrust", my_thrust_fn);
 * set_joystick_button_fn(cfg, "blah", my_blah_button_fn);
 *
 * // Figure out what hardware devices I have (e.g. scandir() on /dev/input/by-id)
 * // and make an array of strings containing those device names. (You'll expect
 * // those names to appear in your config file.)
 * discover_joysticks(my_joysticks, &njoysticks); // assume my_joysticks is array of strings that gets filled in
 *
 * read_joystick_config(cfg, "my_joystick_config_file.txt", my_joysticks, njoysticks);
 *
 * At this point, you can poll the joysticks hardware (however you do that)
 * to and call
 *
 * joystick_axis(cfg, NULL, mode, device, axis, value);
 * joystick_button(cfg, NULL, mode, device, button);
 *
 * The context parameter (NULL, above) may be used for whatever you want,
 * or not at all.
 *
 * The mode is a low numbered integer defined by you.
 *
 * The "device" is an integer indexing my_joysticks as it was passed to read_joystick_config().
 * The button and axis params are which button/axis was pressed/moved.
 *
 * joystick_axis() and joystick_button() will call your my_yaw_fn, my_roll_fn, my_pitch_fn, and
 * so on in such a way that conforms to the config file (i.e. it will have mapped
 * (device/button,axis) to your C functions in accordance with the config file.)
 *
 */

struct joystick_config;
typedef void (*joystick_button_fn)(int wn, void *x);
typedef void (*joystick_axis_fn)(void *x, int value);

/* Call this to get a new, empty joystick config. */
struct joystick_config *new_joystick_config(void);

/* Set up named joystick button and axis functions. The names set here can appear in the config file */
void set_joystick_axis_fn(struct joystick_config *cfg, char *function_name, joystick_axis_fn axis_fn);
void set_joystick_button_fn(struct joystick_config *cfg, char *function_name, joystick_button_fn button_fn);

int read_joystick_config(struct joystick_config *cfg, char *filename, char *joysticks_found[], int njoysticks_found);
void joystick_axis(struct joystick_config *cfg, void *context, int mode, int device, int axis, int value);
void joystick_button(struct joystick_config *cfg, void *context, int mode, int device, int button);
void free_joystick_config(struct joystick_config *cfg);

#endif
