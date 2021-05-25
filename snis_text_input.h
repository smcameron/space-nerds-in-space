#ifndef SNIS_TEXT_INPUT_H__
#define SNIS_TEXT_INPUT_H__

#ifdef DEFINE_SNIS_TEXT_INPUT_GLOBALS
#define GLOBAL
#else
#define GLOBAL extern
#endif

#include <SDL.h>

struct snis_text_input_box;
typedef void (*snis_text_input_box_callback)(void *cookie);

/* Returns a new snis_text_input_box.
 * x, y, height, width: position and dimensions
 * color: color
 * font: font
 * buffer, buflen: where and how much to store any input
 * timer(): function returning periodically increasing integer value.
 *	  This is used to make the cursor blink.
 * callback(): function to call when user presses enter.
 * cookie: pointer to pass through to callback(). Use as you see fit.
 */
GLOBAL struct snis_text_input_box *snis_text_input_box_init(int x, int y,
					int height, int width,
					int color, int font,
					char *buffer, int buflen,
					volatile int *timer,
					snis_text_input_box_callback callback,
					void *cookie);

/* Draw the text input box */
GLOBAL void snis_text_input_box_draw(struct snis_text_input_box *t);

/* Set the text input to have focus (has_focus != 0) or not (has_focus == 0) */
GLOBAL void snis_text_input_box_set_focus(struct snis_text_input_box *t, int has_focus);

/* Returns 1 if x,y are inside the text box, 0 otherwise */
GLOBAL int snis_text_input_box_button_press(struct snis_text_input_box *t, int x, int y);

/* Pass keypress and keyrelease events into the text box. Currently only keypress events
 * do anything. Both return 0 always. */
GLOBAL int snis_text_input_box_keypress(struct snis_text_input_box *t, SDL_Event *event);
GLOBAL int snis_text_input_box_keyrelease(struct snis_text_input_box *t, SDL_Event *event);

/* Zero out the buffer associated with the text box */
GLOBAL void snis_text_input_box_zero(struct snis_text_input_box *t);

/* Set the callback function to call when RETURN is pressed */
GLOBAL void snis_text_input_box_set_return(struct snis_text_input_box *t,
		snis_text_input_box_callback return_function);

/* Sets the text input box to have dynamic width depending on what text it contains.
 * As you type, the width of the box will dynamically increase or decrease to fit
 * the text within the constraints of minwidth and maxwidth.
 */
GLOBAL void snis_text_input_box_set_dynamic_width(struct snis_text_input_box *t,
		int minwidth, int maxwidth);

/* Set the contents of the buffer associated with the text box */
GLOBAL void snis_text_input_box_set_contents(struct snis_text_input_box *t, char *contents);

/* Get x and y position of text box */
GLOBAL int snis_text_input_box_get_x(struct snis_text_input_box *t);
GLOBAL int snis_text_input_box_get_y(struct snis_text_input_box *t);
GLOBAL void snis_text_input_box_update_position(struct snis_text_input_box *t, int x, int y);

/* Get the pointer to the text box's associated buffer */
GLOBAL char *snis_text_input_box_get_buffer(struct snis_text_input_box *t);

#undef GLOBAL
#endif
