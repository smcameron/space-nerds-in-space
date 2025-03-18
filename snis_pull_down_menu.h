#ifndef SNIS_PULL_DOWN_MENU_H__
#define SNIS_PULL_DOWN_MENU_H__

struct pull_down_menu;

#define MAX_PULL_DOWN_COLUMNS 10
#define MAX_PULL_DOWN_ROWS_PER_COLUMN 30

typedef void (*pull_down_menu_tooltip_drawing_function)(int x, int y, char *tooltip);

struct pull_down_menu *create_pull_down_menu(int font, int screen_width);
void pull_down_menu_draw(struct pull_down_menu *pdm);
int pull_down_menu_add_column(struct pull_down_menu *pdm, char *column);
int pull_down_menu_add_row(struct pull_down_menu *pdm, char *column, char *row, void (*func)(void *), void *cookie);
int pull_down_menu_button_press(struct pull_down_menu *pdm, int x, int y);
int pull_down_menu_inside(struct pull_down_menu *pdm, int physical_x, int physical_y);
void pull_down_menu_update_mouse_pos(struct pull_down_menu *m, int physical_x, int physical_y);
void pull_down_menu_set_color(struct pull_down_menu *m, int color);
void pull_down_menu_set_highlight_color(struct pull_down_menu *m, int color);
void pull_down_menu_set_disabled_color(struct pull_down_menu *m, int color);
void pull_down_menu_set_background_alpha(struct pull_down_menu *m, float alpha);
void pull_down_menu_set_checkbox_function(struct pull_down_menu *pdm, char *column, char *row,
						int (*checkbox_value)(void *cookie), void *cookie);
void pull_down_menu_clear(struct pull_down_menu *pdm);
void pull_down_menu_copy(struct pull_down_menu *dest, struct pull_down_menu *src);
void pull_down_menu_set_gravity(struct pull_down_menu *pdm, int right); /* 0 means left, 1 means right */
void pull_down_menu_set_tooltip_drawing_function(struct pull_down_menu *pdm, pull_down_menu_tooltip_drawing_function f);
int pull_down_menu_add_tooltip(struct pull_down_menu *pdm, char *column, char *row, char *tooltip);
void pull_down_menu_add_from_file(struct pull_down_menu *pdm, char *column, char *file, void (*menu_button_fn)(void *));
void pull_down_menu_item_disable(struct pull_down_menu *pdm, char *column, char *row);
void pull_down_menu_item_enable(struct pull_down_menu *pdm, char *column, char *row);

/* Normally if the mouse is not near the pull down menu, it will be invisible.
 * To aid discoverability, we can set a timer for how long the menu will be
 * visible before going into hiding.
 */
void pull_down_menu_set_visible_timer(struct pull_down_menu *pdm, int seconds);


#endif
