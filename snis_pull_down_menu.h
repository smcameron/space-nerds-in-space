#ifndef SNIS_PULL_DOWN_MENU_H__
#define SNIS_PULL_DOWN_MENU_H__

struct pull_down_menu;

#define MAX_PULL_DOWN_COLUMNS 10
#define MAX_PULL_DOWN_ROWS_PER_COLUMN 30

struct pull_down_menu *create_pull_down_menu(int font);
void pull_down_menu_draw(struct pull_down_menu *pdm);
int pull_down_menu_add_column(struct pull_down_menu *pdm, char *column);
int pull_down_menu_add_row(struct pull_down_menu *pdm, char *column, char *row, void (*func)(void *), void *cookie);
int pull_down_menu_button_press(struct pull_down_menu *pdm, int x, int y);
int pull_down_menu_inside(struct pull_down_menu *pdm, int physical_x, int physical_y);
void pull_down_menu_update_mouse_pos(struct pull_down_menu *m, int physical_x, int physical_y);
void pull_down_menu_set_color(struct pull_down_menu *m, int color);
void pull_down_menu_set_checkbox_function(struct pull_down_menu *pdm, char *column, char *row,
						int (*checkbox_value)(void *cookie), void *cookie);

#endif
