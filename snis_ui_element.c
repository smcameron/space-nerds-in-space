#include <stdlib.h>
#include <gtk/gtk.h>

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
	return e;
}

void ui_element_draw(GtkWidget *w, GdkGC *gc, struct ui_element *element)
{
	element->draw(w, gc, element);
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

void ui_element_list_draw(GtkWidget *w, GdkGC *gc,
					struct ui_element_list *list)
{
	for (; list != NULL; list = list->next) {
		struct ui_element *e = list->element;
		if (e->draw && e->active_displaymode == *e->displaymode)
			e->draw(w, gc, e->element);
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
		if (e->button_press && e->active_displaymode == *e->displaymode) {
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

