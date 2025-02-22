#ifndef __SNIS_BUTTON_H__
#define __SNIS_BUTTON_H__

#ifdef DEFINE_BUTTON_GLOBALS
#define GLOBAL
#else
#define GLOBAL extern
#endif

typedef void (*button_function)(void *cookie);

struct button;

/* Returns a new button with
 * position at x, y, and the given width and height, with the given label,
 * color, and font.
 * When the button is released, the button_function button_release() will be called and
 * passed the cooking pointer value.
 */
GLOBAL struct button *snis_button_init(int x, int y, int width, int height, char *label,
			int color, int font, button_function button_release, void *cookie);

/* Draws the specified button */
GLOBAL void snis_button_draw(struct button *b);

/* Triggers the button if x and y are inside the button and the button is enabled */
GLOBAL int snis_button_button_release(struct button *b, int x, int y);

/* Note, the button's actions are not triggered on press, but on release.
 * The press function is used to start a timer to know if a long press occurs
 */
GLOBAL int snis_button_button_press(struct button *b, int x, int y);

/* Calls the button's callback function button_release() and plays any associated sound */
GLOBAL int snis_button_trigger_button(struct button *b);
/* Calls the button's callback function long_press_button_release() and plays any associated sound */
GLOBAL int snis_button_trigger_long_press(struct button *b);

GLOBAL void snis_button_set_color(struct button *b, int color); /* Sets the color of the button */
GLOBAL int snis_button_get_color(struct button *b); /* Returns the color of the button */

/* By default, buttons have a visible border. You can turn it on or off with this. */
GLOBAL void snis_button_set_visible_border(struct button *b, int visible_border);

/* Sets the button's checkbox function and a cookie to be passed to the checkbox function.
 * If the checkbox function is not null, then the button will be drawn with a checkbox
 * and if the checkbox function returns 0, the checkbox will not be checked, otherwise it
 * will be checked.
 */
GLOBAL void snis_button_set_checkbox_function(struct button *b,
		int (*checkbox_function)(void *), void *cookie);
GLOBAL void snis_button_set_long_press_function(struct button *b,
		button_function long_press, void *cookie);

GLOBAL void snis_button_set_label(struct button *b, char *label); /* Sets button label */
GLOBAL char *snis_button_get_label(struct button *b);
GLOBAL int snis_button_get_x(struct button *b); /* These 4 functions get x, y, width, height */
GLOBAL int snis_button_get_y(struct button *b);
GLOBAL int snis_button_get_width(struct button *b);
GLOBAL int snis_button_get_height(struct button *b);
GLOBAL void snis_button_set_position(struct button *b, int x, int y); /* Sets button position */

/* Sets button's associated sound. -1 means none. */
GLOBAL void snis_button_set_sound(struct button *b, int sound);

/* Sets a default sound for all buttons if none specified for a button. */
GLOBAL void snis_button_set_default_sound(int sound);

/* Returns 1 if x,y are inside the button, 0 otherwise */
GLOBAL int snis_button_inside(struct button *b, int x, int y);

/* Makes a button unpressable, and possibly draws it in a different color */
GLOBAL void snis_button_disable(struct button *b);

/* Makes a button pressable */
GLOBAL void snis_button_enable(struct button *b);

/* Sets the color to use to draw the button when it is disabled */
GLOBAL void snis_button_set_disabled_color(struct button *b, int color);

/* Returns the color to use to draw the button when it is disabled */
GLOBAL int snis_button_get_disabled_color(struct button *b);

/* Sets a button's cookie to use when calling the associated button function */
GLOBAL void snis_button_set_cookie(struct button *b, void *cookie);

/* A generic function which treats x as a pointer to an int,
 * and returns the value of *((int *) x)
 * This is because it is very common for a checkbox cookie to be
 * a pointer to an int with value 0 or 1, so to avoid having to write
 * the same very common checkbox cookie sampling function over and over,
 * this one is provided. It's meant to be passed to
 * snis_button_set_checkbox_function() as the 2nd param.
 */
GLOBAL int snis_button_generic_checkbox_function(void *x);

#undef GLOBAL
#endif
