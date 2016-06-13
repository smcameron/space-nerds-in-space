#include <stdlib.h>
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>

#define DEFINE_UI_ELEMENT_GLOBALS
#include "snis_ui_element.h"

struct ui_element {
	void *element;
	int active_displaymode;
	volatile int *displaymode;
	ui_element_drawing_function draw;
	ui_element_button_press_function button_press;
	ui_element_set_focus_function set_focus;
	int has_focus;
	ui_element_keypress_function keypress_fn, keyrelease_fn;
	int hidden;
};

struct ui_element_list {
	struct ui_element *element;
	struct ui_element_list *next;
};

struct ui_element *ui_element_init(void *element,
			ui_element_drawing_function draw,
			ui_element_button_press_function button_press,
			int active_displaymode, volatile int *displaymode)
{
	struct ui_element *e;

	e = malloc(sizeof(*e));
	e->element = element;
	e->draw = draw;
	e->button_press = button_press;
	e->active_displaymode = active_displaymode;
	e->displaymode = displaymode;
	e->set_focus = NULL;
	e->has_focus = 0;
	e->keypress_fn = NULL;	
	e->keyrelease_fn = NULL;
	e->hidden = 0;
	return e;
}

void ui_element_draw(struct ui_element *element)
{
	element->draw(element);
}

void ui_element_list_add_element(struct ui_element_list **list,
					struct ui_element *element)
{
	struct ui_element_list *l;

	if (*list == NULL) {
		*list = malloc(sizeof(**list));
		(*list)->element = element;
		(*list)->next = NULL;
		return;
	}
	l = malloc(sizeof(*l));
	l->element = element;
	l->next = *list;
	*list = l;
}

void ui_element_list_free(struct ui_element_list *list)
{
	struct ui_element_list *n;

	if (list == NULL)
		return;
	n = list->next;
	free(list->element);
	free(list);
	ui_element_list_free(n);
}

void ui_element_list_draw(struct ui_element_list *list)
{
	for (; list != NULL; list = list->next) {
		struct ui_element *e = list->element;
		if (e->draw && e->active_displaymode == *e->displaymode && !e->hidden)
			e->draw(e->element);
	}
}

static void ui_set_focus(struct ui_element_list *list, struct ui_element *e)
{
	if (!e->set_focus)
		return;

	for (; list != NULL; list = list->next) {
		if (!list->element->set_focus)
			continue;
		if (list->element == e) {
			e->set_focus(e->element, 1);
			e->has_focus = 1;
		} else {
			list->element->set_focus(list->element->element, 0);
			list->element->has_focus = 0;
		}
	}
}

void ui_element_list_button_press(struct ui_element_list *list, int x, int y)
{
	int hit;
	struct ui_element *e;
	struct ui_element_list *i;

	for (i = list; i != NULL; i = i->next) {
		e = i->element;
		if (e->button_press && e->active_displaymode == *e->displaymode && !e->hidden) {
			hit = e->button_press(e->element, x, y);
			if (hit) {
				ui_set_focus(list, e);
				break;
			}
		}
	}
}

void ui_element_set_focus_callback(struct ui_element *e,
					ui_element_set_focus_function set_focus)
{
	e->set_focus = set_focus;
}

void ui_element_set_displaymode(struct ui_element *e, int displaymode)
{
	e->active_displaymode = displaymode;
}

static void advance_focus(struct ui_element_list *list)
{
	struct ui_element_list *i;
	int found = 0;
	int done = 0;

	for (i = list; i; i = i->next) {
		struct ui_element *e = i->element;
		if (e->set_focus && e->has_focus &&
				e->active_displaymode == *e->displaymode) {
			e->has_focus = 0;
			e->set_focus(e->element, 0);
			found = 1;
			continue;
		}
		if (found) {
			if (e->active_displaymode == *e->displaymode &&
				e->set_focus && !e->hidden) {
				e->has_focus = 1;
				e->set_focus(e->element, 1);
				done = 1;
				break;				
			}
		}
		
	}

	if (done)
		return;

	/* Start from the beginning of the list */
	for (i = list; i; i = i->next) {
		struct ui_element *e = i->element;
		if (e->active_displaymode == *e->displaymode && e->set_focus && !e->hidden) {
			e->has_focus = 1;
			e->set_focus(e->element, 1);
			break;
		}
	}
}

void ui_element_list_keypress(struct ui_element_list *list, GdkEventKey *event)
{
	struct ui_element_list *i;

	if (event->type == GDK_KEY_PRESS && (event->keyval & ~0x7f) != 0) {
		switch (event->keyval) {
		case GDK_KEY_Tab:
			advance_focus(list);
			return;
		default:
			break;
		}
	}

	for (i = list; i; i = i->next) {
		if (!i->element->has_focus)
			continue;
		if (!i->element->keypress_fn)
			continue;
		if (*i->element->displaymode != i->element->active_displaymode)
			continue;
		if (i->element->hidden)
			continue;
		i->element->keypress_fn(i->element->element, event);
		break;
	}
}

void ui_element_list_keyrelease(struct ui_element_list *list, GdkEventKey *event)
{
	struct ui_element_list *i;

	for (i = list; i; i = i->next) {
		if (!i->element->has_focus)
			continue;
		if (!i->element->keyrelease_fn)
			continue;
		if (*i->element->displaymode != i->element->active_displaymode)
			continue;
		if (!i->element->hidden)
			continue;
		i->element->keyrelease_fn(i->element->element, event);
		break;
	}
}

void ui_element_get_keystrokes(struct ui_element *e,
				ui_element_keypress_function keypress_fn,
				ui_element_keypress_function keyrelease_fn)
{
	e->keypress_fn = keypress_fn;
	e->keyrelease_fn = keyrelease_fn;
}

void ui_element_hide(struct ui_element *e)
{
	e->hidden = 1;
}

void ui_element_unhide(struct ui_element *e)
{
	e->hidden = 0;
}

struct ui_element *widget_to_ui_element(struct ui_element_list *list, void *widget)
{
	for (; list != NULL; list = list->next) {
		if (list->element->element == widget)
			return list->element;
	}
	return NULL;
}

