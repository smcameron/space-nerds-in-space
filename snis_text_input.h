#ifndef SNIS_TEXT_INPUT_H__
#define SNIS_TEXT_INPUT_H__

#ifdef DEFINE_SNIS_TEXT_INPUT_GLOBALS
#define GLOBAL
#else
#define GLOBAL extern
#endif

struct snis_text_input_box;
typedef void (*snis_text_input_box_callback)(void *cookie);

GLOBAL struct snis_text_input_box *snis_text_input_box_init(int x, int y,
					int height, int width,
					int color, int font,
					char *buffer, int buflen,
					volatile int *timer,
					snis_text_input_box_callback callback,
					void *cookie);

GLOBAL void snis_text_input_box_draw(GtkWidget *w, GdkGC *gc, struct snis_text_input_box *t);

GLOBAL void snis_text_input_box_set_focus(struct snis_text_input_box *t, int has_focus);
GLOBAL int snis_text_input_box_button_press(struct snis_text_input_box *t, int x, int y);
GLOBAL int snis_text_input_box_keypress(struct snis_text_input_box *t, GdkEventKey *event);
GLOBAL int snis_text_input_box_keyrelease(struct snis_text_input_box *t, GdkEventKey *event);
GLOBAL void snis_text_input_box_zero(struct snis_text_input_box *t);
	
#undef GLOBAL
#endif
