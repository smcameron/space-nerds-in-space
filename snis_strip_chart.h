#ifndef SNIS_STRIP_CHART_H__
#define SNIS_STRIP_CHART_H__

struct strip_chart;
struct scaling_strip_chart;

extern struct strip_chart *snis_strip_chart_init(int x, int y, int width, int height, char *label, char *warning_msg,
			int color, int warn_color, uint8_t warning_level, int font, int history_size);
extern void snis_strip_chart_draw(struct strip_chart *sc);
extern void snis_strip_chart_update(struct strip_chart *sc, uint8_t value);
extern void snis_strip_chart_update_position(struct strip_chart *sc, int x, int y);
extern int snis_strip_chart_inside(struct strip_chart *sc, int x, int y);
extern int snis_strip_chart_get_x(struct strip_chart *sc);
extern int snis_strip_chart_get_y(struct strip_chart *sc);
extern char *snis_strip_chart_get_label(struct strip_chart *sc);

extern struct scaling_strip_chart *snis_scaling_strip_chart_init(int x, int y,
			int width, int height, char *label, char *warning_msg,
			int color, int warn_color, float warning_level, int font, int history_size);
extern void snis_scaling_strip_chart_draw(struct scaling_strip_chart *sc);
extern void snis_scaling_strip_chart_update(struct scaling_strip_chart *sc, float value);
extern void snis_scaling_strip_chart_update_position(struct scaling_strip_chart *sc, int x, int y);
extern int snis_scaling_strip_chart_inside(struct scaling_strip_chart *sc, int x, int y);
extern int snis_scaling_strip_chart_get_x(struct scaling_strip_chart *sc);
extern int snis_scaling_strip_chart_get_y(struct scaling_strip_chart *sc);
extern char *snis_scaling_strip_chart_get_label(struct scaling_strip_chart *sc);

#endif
