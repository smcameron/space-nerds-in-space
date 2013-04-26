#ifndef __SNIS_LABEL_H__
#define __SNIS_LABEL_H__

#ifdef DEFINE_LABEL_GLOBALS
#define GLOBAL
#else
#define GLOBAL extern
#endif

struct label;

GLOBAL struct label *snis_label_init(int x, int y, char *label,
					int color, int font);
GLOBAL void snis_label_draw(GtkWidget *w, GdkGC *gc, struct label *l);

GLOBAL void snis_label_set_color(struct label *l, int color);

#undef GLOBAL
#endif
