#include <stdlib.h>
#include <string.h>
#include <gtk/gtk.h>

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
	int active_displaymode, *displaymode;
	int lineheight;
	int thumb_pos;
	int print_slowly;
	int printing_pos;
	char **text;
	int color;
};

#define MAXTEXTWINDOWS 20
static int ntextwindows = 0;
static struct text_window *textwindowlist[MAXTEXTWINDOWS];
static int default_timer = 0;
static volatile int *textwindow_timer = &default_timer;
static int tty_chatter_sound = -1;

void text_window_add(struct text_window *tw)
{
	if (ntextwindows >= MAXTEXTWINDOWS)
		return;
	textwindowlist[ntextwindows] = tw;
	ntextwindows++;
}

int text_window_entry_count(struct text_window *tw)
{
	int rc;

	rc = tw->last_entry - tw->first_entry + 1;
	if (rc <= 0)
		rc += tw->total_lines;
	return rc;
}

void text_window_add_text(struct text_window *tw, char *text)
{
	strncpy(tw->text[tw->last_entry], text, 79);
	tw->last_entry = (tw->last_entry + 1) % tw->total_lines;
	if (tw->last_entry == tw->first_entry)
		tw->first_entry = (tw->first_entry + 1) % tw->total_lines;
	if (tw->visible_lines > text_window_entry_count(tw))
		tw->top_line = tw->last_entry - text_window_entry_count(tw); 
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

struct text_window *text_window_init(int x, int y, int w,
			int total_lines, int visible_lines,
			int active_displaymode, int *displaymode, int color)
{
	int i;

	struct text_window *tw;

	tw = malloc(sizeof(*tw));
	tw->x = x;
	tw->y = y;
	tw->w = w;
	tw->total_lines = total_lines;
	tw->visible_lines = visible_lines;
	tw->displaymode = displaymode;
	tw->active_displaymode = active_displaymode;
	tw->color = color;
	tw->text = malloc(sizeof(*tw) * total_lines);
	for (i = 0; i < total_lines; i++) {
		tw->text[i] = malloc(80);
		memset(tw->text[i], 0, 80);
	}
	tw->lineheight = 20;
	tw->last_entry = 0;
	tw->top_line = 0;
	tw->h = tw->lineheight * tw->visible_lines + 10;
	tw->font = TINY_FONT;
	tw->print_slowly = 1;
	tw->printing_pos = 0;
	return tw;
}

void text_window_draw(GtkWidget *w, GdkGC *gc, struct text_window *tw)
{
	int i, j;
	int thumb_pos, thumb_top, thumb_bottom, twec;

	sng_set_foreground(tw->color);
	/* draw outer rectangle */
	sng_current_draw_rectangle(w->window, gc, 0, tw->x, tw->y, tw->w, tw->h);
	/* draw scroll bar */
	sng_current_draw_rectangle(w->window, gc, 0, tw->x + tw->w - 15, tw->y + 5, 10, tw->h - 10);

	twec = text_window_entry_count(tw);
	if (twec == 0) {
		thumb_pos = 0;
		thumb_top = tw->y + 5;
		thumb_bottom = tw->y + tw->h - 10;
	} else {
		float f;
		/* figure which line the thumb pos is on. */
		thumb_pos = (tw->top_line + (tw->visible_lines / 2) - tw->first_entry) % tw->total_lines;
		if (thumb_pos < 0)
			thumb_pos += tw->total_lines;
		/* figure where that is on the screen */
		f = (float) thumb_pos / (float) text_window_entry_count(tw);
		thumb_pos = (int) (f * tw->h) + tw->y;
		thumb_top = (int) (((float) (tw->visible_lines / 2.0) / (float) twec) * 
				-tw->lineheight * tw->visible_lines + thumb_pos);
		if (thumb_top < tw->y + 5)
			thumb_top = tw->y + 5;
		thumb_bottom = (int) (((float) (tw->visible_lines / 2.0) / (float) twec) * 
				tw->lineheight * tw->visible_lines + thumb_pos);
		if (thumb_bottom > tw->y + tw->h - 10)
			thumb_bottom = tw->y + tw->h - 10;
			
	}
	sng_current_draw_rectangle(w->window, gc, 0,
			tw->x + tw->w - 13, thumb_pos - tw->lineheight / 2,
			6, tw->lineheight);
	sng_current_draw_rectangle(w->window, gc, 0,
			tw->x + tw->w - 11, thumb_top,
			2, thumb_bottom - thumb_top);

	j = 0;
	for (i = tw->top_line;
		j < tw->visible_lines && j < text_window_entry_count(tw);
		i = (i + 1) % tw->total_lines) {
			int len = strlen(tw->text[i]);

			if (!tw->print_slowly || i != tw->last_entry -1) {
				sng_abs_xy_draw_string(w, gc, tw->text[i], tw->font, tw->x + 10,
						tw->y + j * tw->lineheight + tw->lineheight);
			} else {
				char tmpbuf[100];	
				strncpy(tmpbuf, tw->text[i], 99);
				if (tw->printing_pos < len - 1) {
					if (((*textwindow_timer >> 2) & 1) == 0) {
						tmpbuf[tw->printing_pos] = '_';
						tmpbuf[tw->printing_pos + 1] = '\0';
					} else {
						tmpbuf[tw->printing_pos] = '\0';
					}
					tw->printing_pos++;
					if (tty_chatter_sound != -1)
						wwviaudio_add_sound(tty_chatter_sound);
				} else {
					if (((*textwindow_timer >> 2) & 0x01) == 0) 
						strcat(tmpbuf, "_");
				}
				sng_abs_xy_draw_string(w, gc, tmpbuf, tw->font, tw->x + 10,
						tw->y + j * tw->lineheight + tw->lineheight);
			}
			j++;
	}
}

void text_window_draw_all(GtkWidget *w, GdkGC *gc)
{
	int i;

	for (i = 0; i < ntextwindows; i++)
		if (textwindowlist[i]->active_displaymode == *textwindowlist[i]->displaymode)
			text_window_draw(w, gc, textwindowlist[i]);
}

void text_window_set_timer(volatile int *timer)
{
	textwindow_timer = timer;
}

void text_window_set_chatter_sound(int chatter_sound)
{
	tty_chatter_sound = chatter_sound;
}

