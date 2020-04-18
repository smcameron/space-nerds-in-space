#ifndef SNIS_SLIDERS_H__
#define SNIS_SLIDERS_H__

#ifdef DEFINE_SLIDERS_GLOBALS
#define GLOBAL
#else
#define GLOBAL extern
#endif

struct slider;

typedef void (*slider_clicked_function)(struct slider *s);
typedef double (*slider_monitor_function)(void);


/* Create and return a new slider.
 * x and y are the position of the slider on the screen
 * length and height are the dimensions of the slider
 * color
 * label is the label of the slider
 * l1 and l2 are labels at either end of the slider.
 * r1 and r2 are determine the upper and lower bound of the sampled values
 * gmf() is a sampling function called to determine the current value of
 * whatever the slider is monitoring.
 * clicked() is the function called whenever the slider is clicked with the mouse.
 */
GLOBAL struct slider *snis_slider_init(float x, float y, float length, float height, int color,
		char *label, char *l1, char *l2, double r1, double r2,
		slider_monitor_function gmf, slider_clicked_function clicked);

/* Sets a slider to be drawn vertically or horizontally if v is non-zero or zero respectively */
GLOBAL void snis_slider_set_vertical(struct slider *s, int v);
GLOBAL void snis_slider_draw(int wn, struct slider *s); /* draws a slider */

/* Gets the current value of a slider, between 0 and 1 */
GLOBAL double snis_slider_get_value(struct slider *s);

/* Gets the current value of the knob on the slider, between 0 and 1 */
GLOBAL double snis_slider_get_input(struct slider *s);

/* Given x and y (mouse position) return 0 if not within the slider 1 otherwise
 * Also, if 1, set the input value of the slider and call the slider's clicked()
 * function if it is not null.
 */
GLOBAL int snis_slider_button_press(int wn, struct slider *s, int x, int y);

/* Set an associated sound with the slider */
GLOBAL void snis_slider_set_sound(int sound);

/* Set a slider's input knob to a particular value between 0 and 1 */
GLOBAL void snis_slider_set_input(struct slider *s, double input);

/* Set a slider's input knob to a particular value between 0 and 1 and
 * also trigger the slider's clicked() function if non null and play
 * any associated sound if with_sound is non zero.  This essentially
 * simulates the player clicking on the slider.
 */
GLOBAL void snis_slider_poke_input(struct slider *s, double input, int with_sound);

/* Nudge a slider's input up or down some fraction between -1.0 and 1.0,
 * triggering the slider's clicked() function and playing any associated
 * sound if with_sound is non-zero
 */
GLOBAL void snis_slider_nudge(struct slider *s, double fraction, int with_sound);

/* Set's a slider's color scheme for good, caution, and warning, (green, yellow, red)
 * as normal (reversed == 0) or reversed (reversed != 0)
 * This is used for status indication.  For "normal", high values are good, and low
 * values are bad (e.g. fuel gauge), for "reversed", high values are bad, and low
 * values are good (e.g. temperature gauge).
 */
GLOBAL void snis_slider_set_color_scheme(struct slider *s, int reversed);

/* Set the font for a slider's label */
GLOBAL void snis_slider_set_label_font(struct slider *s, int font);

/* Set the amount of fuzzing to do on the sliders value indicator.
 * The value of fuzz should be between 0 and 255.
 * High amounts of fuzz introduce more randomness into the indicator
 * If fuzz is zero then no randomness is introduced into the indicator.
 * The purpose of this fuzzing is twofold. 1) It makes it clearer to the user
 * that the indicated value is what is being measured, not what is being
 * requested. 2) the noisy value can be sampled vi snis_slider_get_value(),
 * and applied to effects in the client so that the measured noise is also
 * manifested in other ways, e.g. on the NAV screen, if sensor power is low
 * and noisy, then you see this noise also in the rendering on the nav
 * screen as things flicker in and out.
 */
GLOBAL void snis_slider_set_fuzz(struct slider *s, int fuzz);

/* Returns true if the mouse is inside the slider. Used for tooltips */
GLOBAL int snis_slider_mouse_inside(int wn, struct slider *s, int x, int y);

/* Returns the position and dimensions of a slider in x, y, length and height */
GLOBAL void snis_slider_get_location(struct slider *s, float *x, float *y, float *length, float *height);

/* Returns true if the slider value is in the "danger zone", which means less 15% or greater then 90%
 * if the color scheme is "normal" or "reversed", respectively. See also: snis_slider_set_color_scheme().
 */
GLOBAL int snis_slider_alarm_triggered(struct slider *s);

GLOBAL void snis_slider_mouse_position_query(int *x, int *y); /* Allows sliders to query mouse position */

#undef GLOBAL
#endif
