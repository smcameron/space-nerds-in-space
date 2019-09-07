#ifndef __SNIS_LABEL_H__
#define __SNIS_LABEL_H__

#ifdef DEFINE_LABEL_GLOBALS
#define GLOBAL
#else
#define GLOBAL extern
#endif

struct label;

/* Returns a new label.
 * x, y: position of label.
 * label: value of label, e.g.: "RPM"
 * color: color of label
 * font: font of label
 */
GLOBAL struct label *snis_label_init(int x, int y, char *label,
					int color, int font);
GLOBAL void snis_label_draw(struct label *l); /* draw the label */
GLOBAL void snis_label_set_color(struct label *l, int color); /* Set the color of a label */

#undef GLOBAL
#endif
