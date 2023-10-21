/*
	Copyright (C) 2012 Stephen M. Cameron
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

/*
	The idea is the lists of sliders, buttons and gauges and the displaymode
	crap can all be put into this generalized UI element stuff, so we can more
	easily, treat various different UI elements in a generic way.
*/

#ifndef SNIS_UI_ELEMENT_H__
#define SNIS_UI_ELEMENT_H__

#include <SDL.h>
#include "xdg_base_dir_spec.h"

struct ui_element;
struct ui_element_list;

typedef void (*ui_element_drawing_function)(void *);
typedef int (*ui_element_button_release_function)(void *element, int x, int y);
typedef void (*ui_element_set_focus_function)(void *element, int has_focus);
typedef int (*ui_element_keypress_function)(void *element, SDL_Event *event);
typedef int (*ui_element_inside_function)(void *element, int physical_x, int physical_y);
typedef void (*ui_tooltip_drawing_function)(int x, int y, char *tooltip);
typedef void (*ui_update_mouse_pos_function)(void *element, int physical_x, int physical_y);
typedef void (*ui_element_update_position_function)(void *element, int x, int y);
typedef char * (*ui_element_get_label_function)(void *element);
typedef void (*ui_element_draw_position_function)(int x, int y);

/*
	struct ui_element_functions defines an interface of function pointers that a UI element
	should provide to be useable with this generic ui element thing.  Not every function must
	be provided, some are optional (signified by NULL).
	FIXME: not sure the optional ones are all documented.
*/
struct ui_element_functions {
	ui_element_drawing_function draw;			/* Fn that draws the UI element */
	ui_element_button_release_function button_release;	/* Fn to call on mouse btn released in the UI element */
	ui_element_button_release_function button_press;	/* Fn to call on mouse btn pressed in the UI element */
	ui_element_inside_function inside;			/* Fn to test whether (x,y) is inside a UI element */
	ui_element_update_position_function update_pos_fn;	/* Fn to update UI element offset for moveable ones */
	ui_element_set_focus_function set_focus;		/* Fn to give a UI element keyboard focus */
	ui_element_keypress_function keypress_fn;		/* Fn to call when keypress is received */
	ui_element_keypress_function keyrelease_fn;		/* optional Fn to call when keyrelease received */
	ui_update_mouse_pos_function update_mouse_pos;		/* optional Fn to call with mouse pos */
	ui_element_get_label_function get_label;		/* optional Fn to get UI element's label */
};

#ifdef DEFINE_UI_ELEMENT_LIST_GLOBALS
#define GLOBAL
#else
#define GLOBAL extern
#endif

#define TOOLTIP_DELAY (30) /* 1 second */

/* Returns a pointer to a newly allocated struct_ui_element which is associated
 * with the passed in void *element, initialized with the passed in function pointers,
 * default location, active displaymode, and a pointer to the global displaymode
 * variable.
 */
GLOBAL struct ui_element *ui_element_init(void *element,
			struct ui_element_functions fns,
			int defaultx,
			int defaulty,
			int active_displaymode, volatile int *displaymode);

/* Calls the associated elements draw function */
GLOBAL void ui_element_draw(struct ui_element *element);

/* Draws a ui element's tooltip at the mouse cursor if certain conditions are met:
 * that the mouse has been inside the element for sufficiently long, that it has a
 * tooltip.
 */
GLOBAL void ui_element_maybe_draw_tooltip(struct ui_element *element, int mousex, int mousey);

/* Adds a ui_element to a linked list of ui_elements. */
GLOBAL void ui_element_list_add_element(struct ui_element_list **list,
					struct ui_element *element);

/* Frees a linked list of ui elements. */
GLOBAL void ui_element_list_free(struct ui_element_list *list);
/* Draws all the ui elements in a list if they are not hidden and have the current displaymode
 * as read from the displaymode pointer passed in via ui_element_init(). */
GLOBAL void ui_element_list_draw(struct ui_element_list *list);
/* Draws tool tips for all the ui elements in a list by calling ui_element_maybe_draw_tooltip()
 * on each element in the list. (Generally the conditions are such that only one or zero are
 * actually drawn, barring overlapping ui elements (which are permitted but unusual)). */
GLOBAL void ui_element_list_maybe_draw_tooltips(struct ui_element_list *list, int mousex, int mousey);
/* Signal mouse btn release to all the ui elements in a list so they can check if it occurs
 * within their boundaries, and if so, subsequently call ->button_release() */
GLOBAL void ui_element_list_button_release(struct ui_element_list *list, int x, int y);
/* Signal mouse btn press to all the ui elements in a list so they can check if it occurs
 * within their boundaries, and if so, subsequently call ->button_press() */
GLOBAL void ui_element_list_button_press(struct ui_element_list *list, int x, int y);
/* Update the position offset of the UI element. This is used to enable user moveable UI
 * elements. */
GLOBAL void ui_element_update_position_offset(struct ui_element *element, int xoffset, int yoffset);
/* Returns the current offset of the UI element in xoffset and yoffset. */
GLOBAL void ui_element_get_position_offset(struct ui_element *element, int *xoffset, int *yoffset);
/* Sets the default position of a UI element so that user moveable ui elements may be
 * reset to default positions */
GLOBAL void ui_element_set_default_position(struct ui_element *element, int x, int y);
/* Resets ui element posiiton offsets to zero and element position to default */
GLOBAL void ui_element_reset_position_offset(struct ui_element *element);
/* Calls ui_element_reset_position_offset() for all ui elements in a linked list */
GLOBAL void ui_element_list_reset_position_offsets(struct ui_element_list *list);
/* Find the element in the linked list of elements and give it keyboard focus
 * while removing keyboard focus from all other elements in the linked list */
GLOBAL void ui_set_focus(struct ui_element_list *list, struct ui_element *e);
/* Removes keyboard focus from all ui elements in a linked list */
GLOBAL void ui_element_list_clear_focus(struct ui_element_list *list);
/* Find the ui element in the linked list associated with the given widget and
 * give it keyboard focus.  The difference between this and ui_set_focus is
 * this one uses the widget pointer, not the ui_element pointer. */
GLOBAL void ui_set_widget_focus(struct ui_element_list *list, void *widget);
/* Set a ui element's tooltip */
GLOBAL void ui_element_set_tooltip(struct ui_element *e, char *tooltip);
/* Scan a linked list of ui elements and for each one that: has keyboard focus,
 * has a keypress_fn, has the current displaymode, and is not hidden, call its
 * keypress function. Or, if it was the TAB key pressed, then advance the keyboard
 * focus to the next ui element in the list (thus the order of the list is
 * important as it controls the order that keyboard focus advances.) If focus is
 * advanced, the keypress is consumed, and 1 is returned. Otherwise, 0 is returned.
 * OBSOLETE, use ui_element_list_event() instead.
 */
GLOBAL int ui_element_list_keypress(struct ui_element_list *list, SDL_Event *event);
/* Same as ui_element_list_keypress, except for key release events, and without the
 * TAB keyboard focus advancing feature. * OBSOLETE, use ui_element_list_event()
 * instead. */
GLOBAL int ui_element_list_keyrelease(struct ui_element_list *list, SDL_Event *event);
/* Calls either ui_element_list_keyrelease() or ui_element_keypress() or nothing,
 * depending on the type of SDL_Event. */
GLOBAL int ui_element_list_event(struct ui_element_list *list, SDL_Event *event);
/* Marks a ui element as hidden (suppresses drawing it) */
GLOBAL void ui_element_hide(struct ui_element *e);
/* Marks a ui element as not hidden (drawing is not suppressed) */
GLOBAL void ui_element_unhide(struct ui_element *e);
/* Set the current displaymode for the ui element */
GLOBAL void ui_element_set_displaymode(struct ui_element *e, int displaymode);
/* Search a list of ui elements for one that is associated with the given widget */
GLOBAL struct ui_element *widget_to_ui_element(struct ui_element_list *list, void *widget);
/* Sets the tooltip drawing function. fn ptr used to isolate drawing code from this code */
GLOBAL void ui_set_tooltip_drawing_function(ui_tooltip_drawing_function f);
/* Sets the widget position drawing function, used to print coords when moving widgets */
GLOBAL void ui_set_widget_position_drawing_function(ui_element_draw_position_function f);
/* Do, or do not show element position when drawing element (value == 1 or 0) */
GLOBAL void ui_element_show_position(struct ui_element *e, int value);
/* Returns the first ui element in the list that (x, y) is inside. Useful for implementing user
 * moveable widgets. */
GLOBAL struct ui_element *ui_element_list_find_by_position(struct ui_element_list *list, int x, int y);
/* Saves widget position offsets in a file: snis_ui_position_offsets.txt
 * The directory is determined via XDG Base Directory Specification.  */
GLOBAL int ui_element_list_save_position_offsets(struct ui_element_list *list, struct xdg_base_context *cx);
/* Restores widget positions from a file: snis_ui_position_offsets.txt
 * Directory is determined via XDG Base Directory Specification */
GLOBAL int ui_element_list_restore_position_offsets(struct ui_element_list *list, struct xdg_base_context *cx);

#undef GLOBAL
#endif
