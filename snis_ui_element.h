#ifndef SNIS_UI_ELEMENT_H__
#define SNIS_UI_ELEMENT_H__

struct ui_element;
struct ui_element_list;

typedef void (*ui_element_drawing_function)(GtkWidget *w, GdkGC *gc, void *);
typedef int (*ui_element_button_press_function)(void *element, int x, int y);
typedef void (*ui_element_set_focus_function)(void *element, int has_focus);
typedef int (*ui_element_keypress_function)(void *element, GdkEventKey *event);

#ifdef DEFINE_UI_ELEMENT_LIST_GLOBALS
#define GLOBAL
#else
#define GLOBAL extern
#endif

GLOBAL struct ui_element *ui_element_init(void *element,
			ui_element_drawing_function draw,
			ui_element_button_press_function button_press,
			int active_displaymode, volatile int *displaymode);
GLOBAL void ui_element_draw(GtkWidget *w, GdkGC *gc, struct ui_element *element);
GLOBAL void ui_element_list_add_element(struct ui_element_list **list,
					struct ui_element *element);
GLOBAL void ui_element_list_free(struct ui_element_list *list);
GLOBAL void ui_element_list_draw(GtkWidget *w, GdkGC *gc,
					struct ui_element_list *list);
GLOBAL void ui_element_list_button_press(struct ui_element_list *list, int x, int y);
GLOBAL void ui_element_set_focus_callback(struct ui_element *e,
						ui_element_set_focus_function set_focus);
GLOBAL void ui_element_get_keystrokes(struct ui_element *e, 
				ui_element_keypress_function keypress_fn,
				ui_element_keypress_function keyrelease_fn);

GLOBAL void ui_element_list_keypress(struct ui_element_list *list, GdkEventKey *event);
GLOBAL void ui_element_list_keyrelease(struct ui_element_list *list, GdkEventKey *event);

#undef GLOBAL
#endif
