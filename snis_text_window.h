#ifndef SNIS_TExT_WINDOW_H__
#define SNIS_TExT_WINDOW_H__

#ifdef DEFINE_SNIS_TEXT_WINDOW_GLOBALS
#define GLOBAL
#else
#define GLOBAL extern
#endif

struct text_window;

GLOBAL int text_window_entry_count(struct text_window *tw);
GLOBAL void text_window_add_text(struct text_window *tw, char *text);
GLOBAL struct text_window *text_window_init(int x, int y, int w,
			int total_lines, int visible_lines, int color);
GLOBAL void text_window_draw(struct text_window *tw);
GLOBAL void text_window_set_timer(volatile int *timer);
GLOBAL void text_window_set_chatter_sound(int chatter_sound);
GLOBAL void text_window_scroll_up(struct text_window *tw);
GLOBAL void text_window_scroll_down(struct text_window *tw);
GLOBAL void text_window_page_up(struct text_window *tw);
GLOBAL void text_window_page_down(struct text_window *tw);
GLOBAL int text_window_button_press(struct text_window *tw, int x, int y);

#endif
