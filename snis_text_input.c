#include <stdlib.h>
#include <string.h>

#include "snis_font.h"
#include "snis_typeface.h"
#include "snis_graph.h"

#define DEFINE_SNIS_TEXT_INPUT_GLOBALS
#include "snis_text_input.h"

struct snis_text_input_box {
	int x, y;
	int color, font;
	char *buffer;
	int buflen;
	snis_text_input_box_callback callback, return_function;
	void *cookie;
	int cursor_pos;
	volatile int *timer;
	int height, width, minwidth, maxwidth, desiredwidth;
	int has_focus;
};

struct snis_text_input_box *snis_text_input_box_init(int x, int y,
					int height, int width,
					int color, int font,
					char *buffer, int buflen,
					volatile int *timer,
					snis_text_input_box_callback callback,
					void *cookie)
{
	struct snis_text_input_box *t;

	t = malloc(sizeof(*t));
	t->x = x;
	t->y = y;
	t->color = color;
	t->font = font;
	t->buffer = buffer;
	t->buflen = buflen - 1;
	t->cursor_pos = strlen(t->buffer);
	t->timer = timer;
	t->callback = callback;
	t->return_function = NULL;
	t->cookie = cookie;
	t->height = height;
	t->width = width;
	t->minwidth = width;
	t->maxwidth = width;
	t->desiredwidth = width;
	t->has_focus = 0;
	return t;
}

void snis_text_input_box_draw(struct snis_text_input_box *t)
{
	static int twice_cursor_width = 0;
	int cursor_on = t->has_focus && (*t->timer & 0x04);

	sng_set_foreground(t->color);

	if (twice_cursor_width == 0) {
		float x1, y1, x2, y2;
		sng_string_bounding_box("_", t->font, &x1, &y1, &x2, &y2);
		twice_cursor_width = (int) x2 * 2;
	}
	if (t->minwidth == t->maxwidth) {
		t->width = t->minwidth;
		t->desiredwidth = t->minwidth;
	} else {
		float x1, y1, x2, y2;
		sng_string_bounding_box(t->buffer, t->font, &x1, &y1, &x2, &y2);
		int text_width = (int) x2 + twice_cursor_width;
		if (text_width >= t->width - 5) {
			if (text_width + 10 < t->maxwidth)
				t->desiredwidth = text_width + 10;
			else
				t->desiredwidth = t->maxwidth;
		} else {
			if (text_width < t->width - 10) {
				if (text_width < t->minwidth - 10)
					t->desiredwidth = t->minwidth;
				else
					t->desiredwidth = text_width + 10;
			}
		}
	}
	if (t->width != t->desiredwidth) {
		int delta = (t->desiredwidth - t->width) / 2;
		if (delta < 3)
			t->width = t->desiredwidth;
		else
			t->width += delta;
	}
	sng_current_draw_rectangle(0, t->x, t->y, t->width, t->height);
	if (t->has_focus)
		sng_current_draw_rectangle(0, t->x - 1, t->y - 1,
						t->width + 2, t->height + 2);
	sng_abs_xy_draw_string_with_cursor(t->buffer, t->font,
				t->x + 4, t->y + font_lineheight[t->font],
				t->cursor_pos, cursor_on);
}

void snis_text_input_box_set_focus(struct snis_text_input_box *t, int has_focus)
{
	t->has_focus = has_focus;
	if (has_focus) {
		/* TODO SDL2 SDL_SetTextInputRect(SDL_Rect* rect) */
		SDL_StartTextInput();
	} else {
		SDL_StopTextInput();
	}
}

int snis_text_input_box_button_press(struct snis_text_input_box *t, int x, int y)
{
	int hit;

	x = sng_pixelx_to_screenx(x);
	y = sng_pixely_to_screeny(y);
	if (x < t->x || x > t->x + t->width ||
                y < t->y || y > t->y + t->height)
                hit = 0;
	else
		hit = 1;
	/* put stuff for cursor positioning here. */
	return hit;
}

static void do_backspace(struct snis_text_input_box *t)
{
	int i;
	int len = strlen(t->buffer);

	if (t->cursor_pos == 0)
		return;

	if (t->cursor_pos == len && len > 0) {
		t->buffer[len - 1] = '\0';
		t->cursor_pos--;
		return;
	}
		
	for (i = t->cursor_pos - 1; i < len; i++)
		t->buffer[i] = t->buffer[i + 1];
	t->cursor_pos--;
}

static void do_delete(struct snis_text_input_box *t)
{
	int i;
	int len = strlen(t->buffer);

	if (t->cursor_pos == len) {
		do_backspace(t);
		return;
	}

	for (i = t->cursor_pos; i < len; i++)
		t->buffer[i] = t->buffer[i + 1];
}

static void do_rightarrow(struct snis_text_input_box *t)
{
	if (t->cursor_pos < t->buflen && t->cursor_pos < strlen(t->buffer))
		t->cursor_pos++;
}

static void do_leftarrow(struct snis_text_input_box *t)
{
	if (t->cursor_pos > 0)
		t->cursor_pos--;
}

int snis_text_input_box_keypress(struct snis_text_input_box *t, SDL_Event *event)
{
	switch (event->type) {
	case SDL_KEYDOWN:
		/* TODO verify Numpad numbers still work with SDL */
		switch (event->key.keysym.sym) {
		case SDLK_BACKSPACE:
		case SDLK_KP_BACKSPACE:
			do_backspace(t);
			return 1;
		case SDLK_DELETE:
			do_delete(t);
			return 1;
		case SDLK_RIGHT:
			do_rightarrow(t);
			return 1;
		case SDLK_LEFT:
			do_leftarrow(t);
			return 1;
		case SDLK_RETURN:
			if (t->return_function) {
				t->return_function(t->cookie);
				return 1;
			}
			break;
		default:
			break;
		}
		return 0;
	case SDL_KEYUP:
		switch (event->key.keysym.sym) {
		case SDLK_KP_BACKSPACE:
		case SDLK_BACKSPACE:
		case SDLK_DELETE:
		case SDLK_RIGHT:
		case SDLK_LEFT:
		case SDLK_RETURN:
			return 1;
		}
		break;
	case SDL_TEXTINPUT:
		{
			char c = event->text.text[0];
			int currentlen = strlen(t->buffer);
			if (currentlen == t->cursor_pos) {
				t->buffer[t->cursor_pos + 1] = '\0';
				t->buffer[t->cursor_pos] = c;
				if (t->cursor_pos < t->buflen - 1)
					t->cursor_pos++;
				return 1;
			} else {
				if (currentlen < t->buflen - 2) {
					if (currentlen - t->cursor_pos >= 0) {
						memmove(&t->buffer[t->cursor_pos + 1], &t->buffer[t->cursor_pos],
							currentlen - t->cursor_pos);
						t->buffer[t->cursor_pos] = c;
						t->cursor_pos++;
						return 1;
					} else {
						fprintf(stderr,
							"Bug detected at %s:%d, currentlen = %d, t->cursor_pos = %d, "
							"expected t->cursor_pos <= currentlen, but it isn't.\n",
							__FILE__, __LINE__, currentlen, t->cursor_pos);
						t->cursor_pos = currentlen; /* Fix this anomalous condition */
						return 1;
					}
				}
			}
		}
		break;
	}
	return 0;
}

int snis_text_input_box_keyrelease(struct snis_text_input_box *t, SDL_Event *event)
{
	return 0;
}

void snis_text_input_box_zero(struct snis_text_input_box *t)
{
	t->cursor_pos = 0;
	memset(t->buffer, 0, t->buflen);
}

void snis_text_input_box_set_return(struct snis_text_input_box *t, 
		snis_text_input_box_callback return_function)
{
	t->return_function = return_function;
}

void snis_text_input_box_set_dynamic_width(struct snis_text_input_box *t,
		int minwidth, int maxwidth)
{
	if (minwidth > maxwidth)
		return;
	t->minwidth = minwidth;
	t->maxwidth = maxwidth;
	t->desiredwidth = t->width;
}

void snis_text_input_box_set_contents(struct snis_text_input_box *t, char *contents)
{
	memset(t->buffer, 0, t->buflen + 1);
	strncpy(t->buffer, contents, t->buflen);
	t->cursor_pos = strlen(t->buffer);
}

int snis_text_input_box_get_x(struct snis_text_input_box *t) { return t->x; }
int snis_text_input_box_get_y(struct snis_text_input_box *t) { return t->y; }

char *snis_text_input_box_get_buffer(struct snis_text_input_box *t) { return t->buffer; }
