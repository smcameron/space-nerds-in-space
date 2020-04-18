#ifndef SNIS_UI_ELEMENT_H__
#define SNIS_UI_ELEMENT_H__

#include <SDL.h>

struct ui_element;
struct ui_element_list;

typedef void (*ui_element_drawing_function)(int wn, void *);
typedef int (*ui_element_button_release_function)(int wn, void *element, int x, int y);
typedef void (*ui_element_set_focus_function)(void *element, int has_focus);
typedef int (*ui_element_keypress_function)(int wn, void *element, SDL_Event *event);
typedef int (*ui_element_inside_function)(int wn, void *element, int physical_x, int physical_y);
typedef void (*ui_tooltip_drawing_function)(int wn, int x, int y, char *tooltip);
typedef void (*ui_update_mouse_pos_function)(int wn, void *element, int physical_x, int physical_y);

#ifdef DEFINE_UI_ELEMENT_LIST_GLOBALS
#define GLOBAL
#else
#define GLOBAL extern
#endif

#define TOOLTIP_DELAY (30) /* 1 second */

GLOBAL struct ui_element *ui_element_init(void *element,
			ui_element_drawing_function draw,
			ui_element_button_release_function button_release,
			ui_element_inside_function inside,
			int active_displaymode);
GLOBAL void ui_element_draw(int wn, struct ui_element *element);
GLOBAL void ui_element_maybe_draw_tooltip(int wn, struct ui_element *element, int mousex, int mousey);
GLOBAL void ui_element_list_add_element(struct ui_element_list **list,
					struct ui_element *element);
GLOBAL void ui_element_list_free(struct ui_element_list *list);
GLOBAL void ui_element_list_draw(int wn, struct ui_element_list *list, int current_displaymode);
GLOBAL void ui_element_list_maybe_draw_tooltips(int wn, struct ui_element_list *list,
						int mousex, int mousey, int current_displaymode);
GLOBAL void ui_element_list_button_release(int wn, struct ui_element_list *list, int x, int y, int current_displaymode);
GLOBAL void ui_element_list_button_press(int wn, struct ui_element_list *list, int x, int y, int current_displaymode);
GLOBAL void ui_element_set_focus_callback(struct ui_element *e,
						ui_element_set_focus_function set_focus);
GLOBAL void ui_set_focus(struct ui_element_list *list, struct ui_element *e, int has_focus);
GLOBAL void ui_element_list_clear_focus(struct ui_element_list *list);
GLOBAL void ui_set_widget_focus(struct ui_element_list *list, void *widget);
GLOBAL void ui_set_update_mouse_pos_callback(struct ui_element *e, ui_update_mouse_pos_function f);
GLOBAL void ui_element_get_keystrokes(struct ui_element *e, 
				ui_element_keypress_function keypress_fn,
				ui_element_keypress_function keyrelease_fn);
GLOBAL void ui_element_set_tooltip(struct ui_element *e, char *tooltip);

GLOBAL int ui_element_list_keypress(int wn, struct ui_element_list *list, SDL_Event *event, int current_displaymode);
GLOBAL int ui_element_list_keyrelease(int wn, struct ui_element_list *list, SDL_Event *event, int current_displaymode);
GLOBAL int ui_element_list_event(int wn, struct ui_element_list *list, SDL_Event *event, int current_displaymode);
GLOBAL void ui_element_hide(struct ui_element *e);
GLOBAL void ui_element_unhide(struct ui_element *e);
GLOBAL void ui_element_set_displaymode(struct ui_element *e, int displaymode);
GLOBAL struct ui_element *widget_to_ui_element(struct ui_element_list *list, void *widget);
GLOBAL void ui_set_tooltip_drawing_function(ui_tooltip_drawing_function f);
GLOBAL void ui_element_set_button_press_function(struct ui_element *e, ui_element_button_release_function button_press);

#undef GLOBAL
#endif
