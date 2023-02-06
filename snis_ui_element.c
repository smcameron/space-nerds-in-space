#include <stdlib.h>
#include <string.h>
#include <errno.h>

#define DEFINE_UI_ELEMENT_GLOBALS
#include "snis_ui_element.h"

struct ui_element {
	void *element;
	int active_displaymode;
	int has_focus;
	volatile int *displaymode;
	ui_element_drawing_function draw;
	ui_element_button_release_function button_release;
	ui_element_button_release_function button_press;
	ui_element_set_focus_function set_focus;
	ui_element_update_position_function set_position;
	ui_element_get_label_function get_label;
	ui_element_keypress_function keypress_fn, keyrelease_fn;
	ui_element_inside_function inside_fn;
	ui_update_mouse_pos_function update_mouse_pos;
	int hidden;
	int tooltip_timer;
	char *tooltip;
	int defaultx, defaulty; /* default position of widget */
	int xoffset, yoffset; /* offset from default position */
	int show_widget_position; /* used when moving widgets */
};

struct ui_element_list {
	struct ui_element *element;
	struct ui_element_list *next;
};

static void (*draw_tooltip)(int mousex, int mousey, char *tooltip);
static void (*draw_widget_position)(int x, int y);

struct ui_element *ui_element_init(void *element,
			struct ui_element_functions fns,
			int defaultx,
			int defaulty,
			int active_displaymode, volatile int *displaymode)
{
	struct ui_element *e;

	e = malloc(sizeof(*e));
	e->element = element;
	e->draw = fns.draw;
	e->get_label = fns.get_label;
	e->button_release = fns.button_release;
	e->button_press = fns.button_press;
	e->inside_fn = fns.inside;
	e->active_displaymode = active_displaymode;
	e->displaymode = displaymode;
	e->set_focus = fns.set_focus;
	e->has_focus = 0;
	e->keypress_fn = fns.keypress_fn;
	e->keyrelease_fn = fns.keyrelease_fn;
	e->hidden = 0;
	e->tooltip = NULL;
	e->tooltip_timer = TOOLTIP_DELAY;
	e->update_mouse_pos = fns.update_mouse_pos;
	e->set_position = fns.update_pos_fn;
	e->defaultx = defaultx;
	e->defaulty = defaulty;
	e->xoffset = 0;
	e->yoffset = 0;
	e->show_widget_position = 0;
	return e;
}

void ui_element_show_position(struct ui_element *e, int value)
{
	e->show_widget_position = !!value;
}

void ui_element_draw(struct ui_element *element)
{
	element->draw(element);
	if (element->show_widget_position && draw_widget_position)
		draw_widget_position(element->defaultx + element->xoffset, element->defaulty + element->yoffset);
}

void ui_element_maybe_draw_tooltip(struct ui_element *element, int mousex, int mousey)
{
	if (element->update_mouse_pos)
		element->update_mouse_pos(element->element, mousex, mousey);
	if (!element->inside_fn) {
		return;
	}
	if (!element->tooltip)
		return;
	if (!element->inside_fn(element->element, mousex, mousey)) {
		element->tooltip_timer = TOOLTIP_DELAY;
		return;
	}
	if (element->tooltip_timer > 0)
		element->tooltip_timer--;
	if (element->tooltip_timer > 0)
		return;
	if (element->show_widget_position) /* Suppress tooltip when moving widgets */
		return;
	if (draw_tooltip)
		draw_tooltip(mousex, mousey, element->tooltip);
}

void ui_element_list_add_element(struct ui_element_list **list,
					struct ui_element *element)
{
	struct ui_element_list *l, *i;

	if (*list == NULL) {
		*list = malloc(sizeof(**list));
		(*list)->element = element;
		(*list)->next = NULL;
		return;
	}
	l = malloc(sizeof(*l));
	l->element = element;
	l->next = NULL;
	/* We need to add it to the end of the list, not the beginning
	 * so that the focus order is correct.
	 */
	for (i = *list; i->next != NULL;)
		i = i->next;
	i->next = l;
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
		if (e->draw && e->active_displaymode == *e->displaymode && !e->hidden) {
			if (e->set_position)
				e->set_position(e->element, e->defaultx + e->xoffset, e->defaulty + e->yoffset);
			e->draw(e->element);
			if (e->show_widget_position && draw_widget_position)
				draw_widget_position(e->defaultx + e->xoffset, e->defaulty + e->yoffset);
		}
	}
}

void ui_element_list_maybe_draw_tooltips(struct ui_element_list *list, int mousex, int mousey)
{
	for (; list != NULL; list = list->next) {
		struct ui_element *e = list->element;
		if (e->draw && e->active_displaymode == *e->displaymode && !e->hidden)
			ui_element_maybe_draw_tooltip(e, mousex, mousey);
	}
}

void ui_element_set_tooltip(struct ui_element *element, char *tooltip)
{
	if (element->tooltip) {
		free(element->tooltip);
		element->tooltip = NULL;
	}
	if (!tooltip)
		return;
	if (strcmp(tooltip, "") == 0)
		return;
	element->tooltip = strdup(tooltip);
}

void ui_set_focus(struct ui_element_list *list, struct ui_element *e)
{
	if (!e)
		return;
	if (!e->set_focus)
		return;
	if (e->has_focus)
		return;

	for (; list != NULL; list = list->next) {
		if (!list->element->set_focus)
			continue;
		if (list->element->active_displaymode == e->active_displaymode) {
			/* clear focus of other ui elements in the same displaymode */
			list->element->set_focus(list->element->element, 0);
			list->element->has_focus = 0;
		}
	}
	e->set_focus(e->element, 1);
	e->has_focus = 1;
}


void ui_element_list_clear_focus(struct ui_element_list *list)
{
	struct ui_element *e;

	for (; list != NULL; list = list->next) {
		e = list->element;
		if (e->has_focus) {
			e->has_focus = 0;
			if (e->set_focus)
				e->set_focus(e->element, 0);
		}
	}
}

void ui_element_list_button_release(struct ui_element_list *list, int x, int y)
{
	int hit;
	struct ui_element *e, *candidate = NULL;
	struct ui_element_list *i;

	/* We need to go through the entire list because we need to find the last (topmost) element */
	for (i = list; i != NULL; i = i->next) {
		e = i->element;
		if (e->button_release && e->active_displaymode == *e->displaymode && !e->hidden) {
			/* If we have the inside_fn, use it so that we can set the focus before
			 * triggering the button action in case the button action wants to set the
			 * focus, otherwise, if we set the focus afterwards, it will undo the
			 * button action's focus setting.
			 */
			if (e->inside_fn) {
				hit = e->inside_fn(e->element, x, y);
				if (hit)
					candidate = e;
			} else {
				hit = e->button_release(e->element, x, y);
				if (hit)
					candidate = e;
			}
		}
	}
	if (candidate) {
		if (candidate->inside_fn) {
			if (candidate->inside_fn(candidate->element, x, y)) {
				ui_set_focus(list, candidate);
				candidate->button_release(candidate->element, x, y);
			}
		} else {
			if (candidate->button_press(candidate->element, x, y))
				ui_set_focus(list, e);
		}
	}
}

/* Note button press doesn't set the UI element focus, release does that.
 * The main purpose of press is to start a timer in the button so it can
 * distinguish a long-press from a normal press.
 */
void ui_element_list_button_press(struct ui_element_list *list, int x, int y)
{
	int hit;
	struct ui_element *e, *candidate = NULL;
	struct ui_element_list *i;

	/* We need to go through the entire list because we need to find the last (topmost) element */
	for (i = list; i != NULL; i = i->next) {
		e = i->element;
		if (e->button_press && e->active_displaymode == *e->displaymode && !e->hidden) {
			/* If we have the inside_fn, use it so that we can set the focus before
			 * triggering the button action in case the button action wants to set the
			 * focus, otherwise, if we set the focus afterwards, it will undo the
			 * button action's focus setting.
			 */
			if (e->inside_fn) {
				hit = e->inside_fn(e->element, x, y);
				if (hit) {
					candidate = e;
				}
			} else {
				candidate = e;
			}
		}
	}
	if (candidate) {
		if (candidate->inside_fn) {
			if (candidate->inside_fn(candidate->element, x, y))
				candidate->button_press(candidate->element, x, y);
		} else {
			candidate->button_press(candidate->element, x, y);
		}
	}
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

int ui_element_list_keypress(struct ui_element_list *list, SDL_Event *event)
{
	struct ui_element_list *i;

	if (event->type == SDL_KEYDOWN) {
		switch (event->key.keysym.sym) {
		case SDLK_TAB:
		case SDLK_KP_TAB:
			advance_focus(list);
			return 1;
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
		return i->element->keypress_fn(i->element->element, event);
	}
	return 0;
}

int ui_element_list_keyrelease(struct ui_element_list *list, SDL_Event *event)
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
		return i->element->keyrelease_fn(i->element->element, event);
		break;
	}
	return 0;
}

int ui_element_list_event(struct ui_element_list *list, SDL_Event *event)
{
	switch (event->type) {
	case SDL_KEYDOWN:
	case SDL_TEXTINPUT:
		return ui_element_list_keypress(list, event);
	case SDL_KEYUP:
		return ui_element_list_keyrelease(list, event);
	default:
		return 0;
	}
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

void ui_set_widget_focus(struct ui_element_list *list, void *widget)
{
	struct ui_element *uie = widget_to_ui_element(list, widget);
	if (uie)
		ui_set_focus(list, uie);
}

void ui_set_tooltip_drawing_function(ui_tooltip_drawing_function f)
{
	draw_tooltip = f;
}

void ui_set_widget_position_drawing_function(ui_element_draw_position_function f)
{
	draw_widget_position = f;
}

void ui_element_update_position_offset(struct ui_element *element, int xoffset, int yoffset)
{
	element->xoffset = xoffset;
	element->yoffset = yoffset;
}

void ui_element_get_position_offset(struct ui_element *element, int *xoffset, int *yoffset)
{
	*xoffset = element->xoffset;
	*yoffset = element->yoffset;
}

void ui_element_set_default_position(struct ui_element *element, int x, int y)
{
	element->defaultx = x;
	element->defaulty = y;
}

void ui_element_reset_position_offset(struct ui_element *element)
{
	if (!element->set_position)
		return;
	element->xoffset = 0;
	element->yoffset = 0;
	element->set_position(element->element, element->defaultx, element->defaulty);
}

void ui_element_list_reset_position_offsets(struct ui_element_list *list)
{
	for (; list != NULL; list = list->next) {
		struct ui_element *e = list->element;
		ui_element_reset_position_offset(e);
	}
}

struct ui_element *ui_element_list_find_by_position(struct ui_element_list *list, int x, int y)
{
	for (; list != NULL; list = list->next) {
		struct ui_element *e = list->element;
		if (e->draw && e->active_displaymode == *e->displaymode && !e->hidden &&
			e->set_position && e->inside_fn) {
			if (e->inside_fn(e->element, x, y))
				return e;
		}
	}
	return NULL;
}

int ui_element_list_save_position_offsets(struct ui_element_list *list, struct xdg_base_context *cx)
{
	FILE *f;

	f = xdg_base_fopen_for_write(cx, "snis_ui_position_offsets.txt");
	if (!f)
		return -1;
	/* TODO: this is the dumbest thing that might work. Not very robust.
	 * There is no attempt to keep straight which numbers go with which
	 * widgets, it just assumes they're the same every time.
	 */
	for (; list != NULL; list = list->next)
		fprintf(f, "%d %d # %s\n", list->element->xoffset, list->element->yoffset,
			list->element->get_label != NULL ?
			list->element->get_label(list->element->element) : "UNKNOWN");
	fclose(f);
	return 0;
}

int ui_element_list_restore_position_offsets(struct ui_element_list *list, struct xdg_base_context *cx)
{
	FILE *f;
	int a, b, rc;
	char line[512];
	int lc = 0;

	f = xdg_base_fopen_for_read(cx, "snis_ui_position_offsets.txt");
	if (!f)
		return -1;
	/* TODO: this is the dumbest thing that might work. Not very robust.
	 * There is no attempt to keep straight which numbers go with which
	 * widgets, it just assumes they're the same every time.
	 */
	for (; list != NULL; list = list->next) {
		errno = 0;
		if (fgets(line, sizeof(line), f) == NULL) {
			if (errno)
				fprintf(stderr, "snis_ui_position_offsets.txt: unexpected error:%s\n", strerror(errno));
			else
				fprintf(stderr, "snis_ui_position_offsets.txt: unexpected EOF\n");
			fclose(f);
			return -1;
		}
		lc++;
		rc = sscanf(line, "%d %d\n", &a, &b);
		if (rc != 2) {
			fprintf(stderr, "Bad line in snis_ui_position_offsets.txt: %d:%s\n", lc, line);
			return -1;
		}
		list->element->xoffset = a;
		list->element->yoffset = b;
	}
	fclose(f);
	return 0;
}

