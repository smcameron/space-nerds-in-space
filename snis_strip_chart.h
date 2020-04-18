#ifndef SNIS_STRIP_CHART_H__
#define SNIS_STRIP_CHART_H__

struct strip_chart;
struct scaling_strip_chart;

extern struct strip_chart *snis_strip_chart_init(int x, int y, int width, int height, char *label, char *warning_msg,
			int color, int warn_color, uint8_t warning_level, int font, int history_size);
extern void snis_strip_chart_draw(int wn, struct strip_chart *sc);
extern void snis_strip_chart_update(struct strip_chart *sc, uint8_t value);

extern struct scaling_strip_chart *snis_scaling_strip_chart_init(int x, int y,
			int width, int height, char *label, char *warning_msg,
			int color, int warn_color, float warning_level, int font, int history_size);
extern void snis_scaling_strip_chart_draw(int wn, struct scaling_strip_chart *sc);
extern void snis_scaling_strip_chart_update(struct scaling_strip_chart *sc, float value);

#endif
