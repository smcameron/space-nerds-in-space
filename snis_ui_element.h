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

struct ui_element_functions {
	ui_element_drawing_function draw;
	ui_element_button_release_function button_release;
	ui_element_inside_function inside;
	ui_element_update_position_function update_pos_fn;
};

#ifdef DEFINE_UI_ELEMENT_LIST_GLOBALS
#define GLOBAL
#else
#define GLOBAL extern
#endif

#define TOOLTIP_DELAY (30) /* 1 second */

GLOBAL struct ui_element *ui_element_init(void *element,
			struct ui_element_functions fns,
			int defaultx,
			int defaulty,
			int active_displaymode, volatile int *displaymode);
GLOBAL void ui_element_draw(struct ui_element *element);
GLOBAL void ui_element_maybe_draw_tooltip(struct ui_element *element, int mousex, int mousey);
GLOBAL void ui_element_list_add_element(struct ui_element_list **list,
					struct ui_element *element);
GLOBAL void ui_element_list_free(struct ui_element_list *list);
GLOBAL void ui_element_list_draw(struct ui_element_list *list);
GLOBAL void ui_element_list_maybe_draw_tooltips(struct ui_element_list *list, int mousex, int mousey);
GLOBAL void ui_element_list_button_release(struct ui_element_list *list, int x, int y);
GLOBAL void ui_element_list_button_press(struct ui_element_list *list, int x, int y);
GLOBAL void ui_element_update_position_offset(struct ui_element *element, int xoffset, int yoffset);
GLOBAL void ui_element_get_position_offset(struct ui_element *element, int *xoffset, int *yoffset);
GLOBAL void ui_element_set_default_position(struct ui_element *element, int x, int y);
GLOBAL void ui_element_reset_position_offset(struct ui_element *element);
GLOBAL void ui_element_list_reset_position_offsets(struct ui_element_list *list);
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

GLOBAL int ui_element_list_keypress(struct ui_element_list *list, SDL_Event *event);
GLOBAL int ui_element_list_keyrelease(struct ui_element_list *list, SDL_Event *event);
GLOBAL int ui_element_list_event(struct ui_element_list *list, SDL_Event *event);
GLOBAL void ui_element_hide(struct ui_element *e);
GLOBAL void ui_element_unhide(struct ui_element *e);
GLOBAL void ui_element_set_displaymode(struct ui_element *e, int displaymode);
GLOBAL struct ui_element *widget_to_ui_element(struct ui_element_list *list, void *widget);
GLOBAL void ui_set_tooltip_drawing_function(ui_tooltip_drawing_function f);
GLOBAL void ui_element_set_button_press_function(struct ui_element *e, ui_element_button_release_function button_press);
GLOBAL struct ui_element *ui_element_get_by_element(struct ui_element_list *list, void *element);
GLOBAL struct ui_element *ui_element_list_find_by_position(struct ui_element_list *list, int x, int y);
GLOBAL int ui_element_list_save_position_offsets(struct ui_element_list *list, struct xdg_base_context *cx);
GLOBAL int ui_element_list_restore_position_offsets(struct ui_element_list *list, struct xdg_base_context *cx);

#undef GLOBAL
#endif
