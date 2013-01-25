#ifndef SNIS_GRAPH_H__
#define SNIS_GRAPH_H__

#ifdef SNIS_GRAPH_DECLARE_GLOBALS
#define SNG_GLOBAL
#define INIT(y) = y
#else
#define SNG_GLOBAL extern
#define INIT(y)
#endif

/* cardinal color indexes into huex array */
#define WHITE 0
#define BLUE 1
#define BLACK 2
#define GREEN 3
#define YELLOW 4
#define RED 5
#define ORANGE 6
#define CYAN 7
#define MAGENTA 8
#define DARKGREEN 9
#define DARKRED 10
#define AMBER 11
#define LIMEGREEN 12

#define NCOLORS 13              /* number of "cardinal" colors */
#define NSPARKCOLORS 25         /* 25 shades from yellow to red for the sparks */
#define NRAINBOWSTEPS (16)
#define NRAINBOWCOLORS (NRAINBOWSTEPS*3)
#define GRAY (NCOLORS + NSPARKCOLORS + NRAINBOWCOLORS)
#define NSHADESOFGRAY 256

typedef void line_drawing_function(GdkDrawable *drawable,
	GdkGC *gc, gint x1, gint y1, gint x2, gint y2);

typedef void rectangle_drawing_function(GdkDrawable *drawable,
        GdkGC *gc, gboolean filled, gint x, gint y, gint width, gint height);

typedef void bright_line_drawing_function(GdkDrawable *drawable,
	 GdkGC *gc, gint x1, gint y1, gint x2, gint y2, int color);
typedef void arc_drawing_function(GdkDrawable *drawable, GdkGC *gc,
	gboolean filled, gint x, gint y, gint width, gint height, gint angle1, gint angle2);

struct sng_dotted_plot_func_context {
        GdkDrawable *drawable;
        GdkGC *gc;
        int i;
};

extern void sng_set_scale(float xscale, float yscale);
extern void sng_scaled_line(GdkDrawable *drawable, GdkGC *gc, gint x1, gint y1, gint x2, gint y2);
extern void sng_scaled_rectangle(GdkDrawable *drawable,
	GdkGC *gc, gboolean filled, gint x, gint y, gint width, gint height);
extern void sng_thick_scaled_line(GdkDrawable *drawable,
	GdkGC *gc, gint x1, gint y1, gint x2, gint y2);
extern void sng_scaled_bright_line(GdkDrawable *drawable,
	GdkGC *gc, gint x1, gint y1, gint x2, gint y2, int color);
extern void sng_unscaled_bright_line(GdkDrawable *drawable,
	GdkGC *gc, gint x1, gint y1, gint x2, gint y2, int color);
extern void sng_scaled_arc(GdkDrawable *drawable, GdkGC *gc,
	gboolean filled, gint x, gint y, gint width, gint height, gint angle1, gint angle2);

extern void sng_scaled_rectangle(GdkDrawable *drawable,
	GdkGC *gc, gboolean filled, gint x, gint y, gint width, gint height);
extern void sng_use_scaled_drawing_functions(void);
extern void sng_use_unscaled_drawing_functions(void);
extern void sng_use_thick_lines(void);

SNG_GLOBAL line_drawing_function *sng_current_draw_line INIT(gdk_draw_line);
SNG_GLOBAL rectangle_drawing_function *sng_current_draw_rectangle INIT(gdk_draw_rectangle);
SNG_GLOBAL bright_line_drawing_function *sng_current_bright_line INIT(sng_unscaled_bright_line);
SNG_GLOBAL arc_drawing_function *sng_current_draw_arc INIT(gdk_draw_arc);
SNG_GLOBAL void sng_dotted_line_plot_func(int x, int y, void *context);
SNG_GLOBAL void sng_electric_line_plot_func(int x, int y, void *context);
SNG_GLOBAL void sng_draw_dotted_line(GdkDrawable *drawable,
	GdkGC *gc, gint x1, gint y1, gint x2, gint y2);
SNG_GLOBAL void sng_draw_electric_line(GdkDrawable *drawable,
	GdkGC *gc, gint x1, gint y1, gint x2, gint y2);

SNG_GLOBAL int sng_abs_xy_draw_letter(GtkWidget *w, GdkGC *gc, struct my_vect_obj **font, 
		unsigned char letter, int x, int y);
SNG_GLOBAL void sng_abs_xy_draw_string(GtkWidget *w, GdkGC *gc, char *s, int font, int x, int y) ;
SNG_GLOBAL void sng_abs_xy_draw_string_with_cursor(GtkWidget *w, GdkGC *gc,
				char *s, int font, int x, int y, int cursor_pos, int cursor_on) ;
SNG_GLOBAL void sng_draw_point(GdkDrawable *drawable, GdkGC *gc, int x, int y);
SNG_GLOBAL void sng_setup_colors(GtkWidget *w);
SNG_GLOBAL void sng_set_foreground(int c);
SNG_GLOBAL void sng_set_gc(GdkGC *gc);
SNG_GLOBAL void sng_draw_circle(GdkDrawable *drawable, GdkGC *gc, gint x, gint y, gint r);
SNG_GLOBAL void sng_draw_laser_line(GdkDrawable *drawable, GdkGC *gc,
			gint x1, gint y1, gint x2, gint y2, int color);

SNG_GLOBAL int sng_device_x(int x);
SNG_GLOBAL int sng_device_y(int y);
SNG_GLOBAL void sng_device_line(GdkDrawable *drawable, GdkGC *gc,
			int x1, int y1, int x2, int y2);

#undef SNG_GLOBAL
#endif
