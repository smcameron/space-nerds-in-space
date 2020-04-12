#include <stdlib.h>
#include <string.h>

#include "snis_font.h"
#include "snis_typeface.h"
#include "snis_graph.h"
#include "snis_text_window.h"
#include "wwviaudio.h"

struct text_window {
	int x, y, w, h;
	int font;
	int total_lines;
	int first_entry, last_entry;
	int top_line;
	int visible_lines;
	int lineheight;
	int thumb_pos;
	int print_slowly;
	int printing_pos;
	char **text;
	int *textcolor;
	int color;
	int do_blank;
	int tty_chatter_sound;
	float alpha;
};

static int default_timer = 0;
static volatile int *textwindow_timer = &default_timer;

int text_window_entry_count(struct text_window *tw)
{
	int rc;

	rc = tw->last_entry - tw->first_entry + 1;
	if (rc <= 0)
		rc += tw->total_lines;
	return rc;
}

void text_window_add_color_text(struct text_window *tw, const char *text, int color)
{
	strncpy(tw->text[tw->last_entry], text, 79);
	tw->text[tw->last_entry][79] = '\0';
	tw->textcolor[tw->last_entry] = color;
	tw->last_entry = (tw->last_entry + 1) % tw->total_lines;
	if (tw->last_entry == tw->first_entry)
		tw->first_entry = (tw->first_entry + 1) % tw->total_lines;
	if (tw->visible_lines > text_window_entry_count(tw))
		tw->top_line = tw->last_entry - text_window_entry_count(tw) + 1;
	else
		tw->top_line = tw->last_entry - tw->visible_lines; 
	if (tw->top_line < 0)
		tw->top_line += tw->total_lines;
#if 0
	printf("top = %d, f=%d l=%d, c=%d\n", tw->top_line, tw->first_entry, tw->last_entry,
			text_window_entry_count(tw));
#endif
	tw->printing_pos = 0;
}

void text_window_add_text(struct text_window *tw, const char *text)
{
	text_window_add_color_text(tw, text, tw->color);
}

struct text_window *text_window_init(int x, int y, int w,
			int total_lines, int visible_lines, int color)
{
	int i;

	struct text_window *tw;

	tw = malloc(sizeof(*tw));
	tw->x = x;
	tw->y = y;
	tw->w = w;
	tw->total_lines = total_lines;
	tw->visible_lines = visible_lines;
	tw->color = color;
	tw->text = malloc(sizeof(*tw->text) * total_lines);
	for (i = 0; i < total_lines; i++) {
		tw->text[i] = malloc(80);
		memset(tw->text[i], 0, 80);
	}
	tw->textcolor = malloc(sizeof(*tw->textcolor) * total_lines);
	tw->lineheight = font_lineheight[NANO_FONT];
	tw->thumb_pos = 0;
	tw->first_entry = 0;
	tw->last_entry = 0;
	tw->top_line = 0;
	tw->h = tw->lineheight * tw->visible_lines + 10;
	tw->font = NANO_FONT;
	tw->print_slowly = 1;
	tw->printing_pos = 0;
	tw->do_blank = 0;
	tw->tty_chatter_sound = -1;
	tw->alpha = -1.0; /* no alpha */
	return tw;
}

void text_window_set_font(struct text_window *tw, int font)
{
	tw->font = font;
	tw->lineheight = font_lineheight[font];
	tw->h = tw->lineheight * tw->visible_lines + 10;
}

void text_window_draw(struct text_window *tw)
{
	int i, j;
	int thumb_top, thumb_bottom, twec;

	if (tw->do_blank) {
		sng_set_foreground_alpha(BLACK, tw->alpha);
		sng_current_draw_rectangle(1, tw->x, tw->y, tw->w, tw->h);
	}
	sng_set_foreground(tw->color);
	/* draw outer rectangle */
	sng_current_draw_rectangle(0, tw->x, tw->y, tw->w, tw->h);
	/* draw scroll bar */
	sng_current_draw_rectangle(0, tw->x + tw->w - 15, tw->y + 5, 10, tw->h - 10);

	twec = text_window_entry_count(tw);
	if (twec == 0) {
		tw->thumb_pos = 0;
		thumb_top = tw->y + 5;
		thumb_bottom = tw->y + tw->h - 10;
	} else {
		float f;
		/* figure which line the thumb pos is on. */
		tw->thumb_pos = (tw->top_line + (tw->visible_lines / 2) - tw->first_entry) % tw->total_lines;
		if (tw->thumb_pos < 0)
			tw->thumb_pos += tw->total_lines;
		/* figure where that is on the screen */
		f = (float) tw->thumb_pos / (float) text_window_entry_count(tw);
		tw->thumb_pos = (int) (f * tw->h) + tw->y;
		thumb_top = (int) (((float) (tw->visible_lines / 2.0) / (float) twec) * 
				-tw->lineheight * tw->visible_lines + tw->thumb_pos);
		if (thumb_top < tw->y + 5)
			thumb_top = tw->y + 5;
		thumb_bottom = (int) (((float) (tw->visible_lines / 2.0) / (float) twec) * 
				tw->lineheight * tw->visible_lines + tw->thumb_pos);
		if (thumb_bottom > tw->y + tw->h - 10)
			thumb_bottom = tw->y + tw->h - 10;
			
	}
	sng_current_draw_rectangle(0,
			tw->x + tw->w - 13, tw->thumb_pos - tw->lineheight / 2,
			6, tw->lineheight);
	sng_current_draw_rectangle(0,
			tw->x + tw->w - 11, thumb_top,
			2, thumb_bottom - thumb_top);

	j = 0;
	for (i = tw->top_line;
		j < tw->visible_lines && j < text_window_entry_count(tw);
		i = (i + 1) % tw->total_lines) {
			int len = strlen(tw->text[i]);

			sng_set_foreground(tw->textcolor[i]);
			if (!tw->print_slowly || i != tw->last_entry -1) {
				sng_abs_xy_draw_string(tw->text[i], tw->font, tw->x + 10,
						tw->y + j * tw->lineheight + tw->lineheight);
			} else {
				char tmpbuf[100];	
				strncpy(tmpbuf, tw->text[i], 99);
				tmpbuf[99] = '\0';
				if (tw->printing_pos < len - 1) {
					if (((*textwindow_timer >> 2) & 1) == 0) {
						tmpbuf[tw->printing_pos] = '_';
						tmpbuf[tw->printing_pos + 1] = '\0';
					} else {
						tmpbuf[tw->printing_pos] = '\0';
					}
					tw->printing_pos++;
					if (tw->tty_chatter_sound != -1)
						wwviaudio_add_sound(tw->tty_chatter_sound);
				} else {
					if (((*textwindow_timer >> 2) & 0x01) == 0) 
						strcat(tmpbuf, "_");
				}
				sng_abs_xy_draw_string(tmpbuf, tw->font, tw->x + 10,
						tw->y + j * tw->lineheight + tw->lineheight);
			}
			j++;
	}
#if 0
	{
		char tmpbuf[100];
		sprintf(tmpbuf, "FE %d, LE %d, TOP %d, VIS %d\n",
			tw->first_entry, tw->last_entry, tw->top_line, tw->visible_lines);
		sng_abs_xy_draw_string(tmpbuf, tw->font, tw->x, tw->y - tw->lineheight);
	}
#endif
}

void text_window_set_timer(volatile int *timer)
{
	textwindow_timer = timer;
}

void text_window_set_chatter_sound(struct text_window *tw, int chatter_sound)
{
	tw->tty_chatter_sound = chatter_sound;
}

void text_window_scroll_down(struct text_window *tw)
{
	int top_line;
	int last_top_line = tw->last_entry - tw->visible_lines;

	if (text_window_entry_count(tw) < tw->visible_lines)
		return;

	if (last_top_line < 0)
		last_top_line += tw->total_lines;

	top_line = tw->top_line;
	if (top_line == last_top_line)
		return;
	top_line++;
	if (top_line == tw->total_lines)
		top_line = 0;
	tw->top_line = top_line;
}

void text_window_scroll_up(struct text_window *tw)
{
	int top_line;

	top_line = tw->top_line;
	if (top_line == tw->first_entry)
		return;
	top_line--;
	if (top_line < 0)
		top_line = tw->total_lines - 1;
	tw->top_line = top_line;
}

void text_window_page_up(struct text_window *tw)
{
	int i;

	for (i = 0; i < tw->visible_lines; i++)
		text_window_scroll_up(tw);
}

void text_window_page_down(struct text_window *tw)
{
	int i;

	for (i = 0; i < tw->visible_lines; i++)
		text_window_scroll_down(tw);
}

int text_window_button_press(struct text_window *tw, int x, int y)
{
	int i, left, right, top, bottom;

	x = sng_pixelx_to_screenx(x);
	y = sng_pixelx_to_screenx(y);

	left = tw->x + tw->w - 15;
	right = tw->x + tw->w - 5;
	top = tw->y + 5;
	bottom = tw->y + tw->h - 10;

	/* is it in the scroll bar area? */
	if (x < left)
		return 0;
	if (x > right)
		return 0;
	if (y < top)
		return 0;
	if (y > bottom)
		return 0;

	/* It is in the scroll bar area */
	if (y > tw->thumb_pos + 10)
		for (i = 0; i < 5; i++)
			text_window_scroll_down(tw);
	else if (y < tw->thumb_pos - 10)
		for (i = 0; i < 5; i++)
			text_window_scroll_up(tw);
	return 0;
}

void text_window_blank_background(struct text_window *tw, int do_blank)
{
	tw->do_blank = do_blank;
}

void text_window_set_background_alpha(struct text_window *tw, float alpha)
{
	if (alpha > 1.0)
		alpha = 1.0;
	if (alpha < -1.0)
		alpha = -1.0;
	tw->alpha =  alpha;
}

void text_window_slow_printing_effect(struct text_window *tw, int value)
{
	tw->print_slowly = !!value;
}

void text_window_set_visible_lines(struct text_window *tw, int visible_lines)
{
	tw->visible_lines = visible_lines;
	tw->h = tw->lineheight * tw->visible_lines + 10;
}
