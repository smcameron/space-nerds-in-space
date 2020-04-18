#ifndef SNIS_TExT_WINDOW_H__
#define SNIS_TExT_WINDOW_H__

#ifdef DEFINE_SNIS_TEXT_WINDOW_GLOBALS
#define GLOBAL
#else
#define GLOBAL extern
#endif

struct text_window;

GLOBAL int text_window_entry_count(struct text_window *tw);
GLOBAL void text_window_add_text(struct text_window *tw, const char *text);
GLOBAL void text_window_add_color_text(struct text_window *tw, const char *text, int color);
GLOBAL struct text_window *text_window_init(int x, int y, int w,
			int total_lines, int visible_lines, int color);
GLOBAL void text_window_draw(int wn, struct text_window *tw);
GLOBAL void text_window_set_timer(volatile int *timer);
GLOBAL void text_window_set_chatter_sound(struct text_window *tw, int chatter_sound);
GLOBAL void text_window_scroll_up(struct text_window *tw);
GLOBAL void text_window_scroll_down(struct text_window *tw);
GLOBAL void text_window_page_up(struct text_window *tw);
GLOBAL void text_window_page_down(struct text_window *tw);
GLOBAL int text_window_button_press(int wn, struct text_window *tw, int x, int y);
GLOBAL void text_window_blank_background(struct text_window *tw, int do_blank);
GLOBAL void text_window_set_background_alpha(struct text_window *tw, float alpha);
GLOBAL void text_window_set_font(struct text_window *tw, int font);
GLOBAL void text_window_slow_printing_effect(struct text_window *tw, int value);
GLOBAL void text_window_set_visible_lines(struct text_window *tw, int visible_lines);

#endif
