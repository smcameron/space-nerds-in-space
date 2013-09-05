#include <stdlib.h>
#include <math.h>
#include <errno.h>
#include <string.h>
#include <gtk/gtk.h>
#ifndef WITHOUTOPENGL
#include <gtk/gtkgl.h>
#include <GL/gl.h>
#include <png.h>
#endif

#include "snis_font.h"
#define SNIS_GRAPH_DECLARE_GLOBALS
#include "snis_graph.h"
#undef SNIS_GRAPH_DECLARE_GLOBALS

#include "liang-barsky.h"
#include "bline.h"


#define TOTAL_COLORS (NCOLORS + NSPARKCOLORS + NRAINBOWCOLORS + NSHADESOFGRAY)
GdkColor huex[TOTAL_COLORS]; 

extern struct my_vect_obj **gamefont[];
extern int font_scale[];

static struct snis_graph_context {
	float xscale, yscale;
	struct liang_barsky_clip_window c;
	int screen_height;
	GdkGC *gc;
	int hue; /* current color, index into huex[] and glhue[] */
} sgc;

void sng_fixup_gl_y_coordinate(int screen_height)
{
	sgc.screen_height = screen_height;
}

static int sng_rand(int n)
{
	return rand() % n;
}

void sng_set_scale(float xscale, float yscale)
{
	sgc.xscale = xscale;
	sgc.yscale = yscale;
}

void sng_set_clip_window(int x1, int y1, int x2, int y2)
{
	sgc.c.x1 = x1;
	sgc.c.y1 = y1;
	sgc.c.x2 = x2;
	sgc.c.y2 = y2;
}

int sng_device_x(int x)
{
	return (int) (x * sgc.xscale);
}

int sng_device_y(int y)
{
	return (int) (y * sgc.yscale);
}

void sng_gl_draw_line(GdkDrawable *drawable, GdkGC *gc, int x1, int y1, int x2, int y2)
{
#ifndef WITHOUTOPENGL
	GdkColor *h = &huex[sgc.hue];

        glBegin(GL_LINES);
        glColor3us(h->red, h->green, h->blue);
        glVertex2i(x1, sgc.screen_height - y1);
        glVertex2i(x2, sgc.screen_height - y2);
        glEnd();
#else
	gdk_draw_line(drawable, gc, x1, y1, x2, y2);
#endif
}

void sng_scaled_line(GdkDrawable *drawable,
        GdkGC *gc, gint x1, gint y1, gint x2, gint y2)
{
	if (!clip_line(&sgc.c, &x1, &y1, &x2, &y2))
		return;
        sng_gl_draw_line(drawable, gc, x1 * sgc.xscale, y1 * sgc.yscale,
                x2 * sgc.xscale, y2 * sgc.yscale);
}

void sng_thick_scaled_line(GdkDrawable *drawable,
	GdkGC *gc, gint x1, gint y1, gint x2, gint y2)
{
	int sx1, sy1, sx2, sy2, dx, dy;

	if (!clip_line(&sgc.c, &x1, &y1, &x2, &y2))
		return;

	if (abs(x1 - x2) > abs(y1 - y2)) {
		dx = 0;
		dy = 1;
	} else {
		dx = 1;
		dy = 0;
	}
	sx1 = x1 * sgc.xscale;
	sx2 = x2 * sgc.xscale;
	sy1 = y1 * sgc.yscale;	
	sy2 = y2 * sgc.yscale;	
	
	sng_gl_draw_line(drawable, gc, sx1, sy1, sx2, sy2);
	sng_gl_draw_line(drawable, gc, sx1 - dx, sy1 - dy, sx2 - dx, sy2 - dy);
	sng_gl_draw_line(drawable, gc, sx1 + dx, sy1 + dy, sx2 + dx, sy2 + dy);
}

static int clip_rectangle(int *x, int *y, int *width, int *height)
{
	int x2, y2;

	x2 = *x + *width;
	y2 = *y + *height;
	if (x2 < sgc.c.x1)
		return 0;
	if (*x > sgc.c.x2)
		return 0;
	if (y2 < sgc.c.y1)
		return 0;
	if (*y > sgc.c.y2)
		return 0;

	if (*x < sgc.c.x1)
		*x = sgc.c.x1;
	if (x2 > sgc.c.x2)
		x2 = sgc.c.x2;
	if (*y < sgc.c.y1)
		*y = sgc.c.y1;
	if (y2 > sgc.c.y2)
		y2 = sgc.c.y2;

	*width = x2 - *x;
	*height = y2 - *y;
	return 1;
}

static void gl_draw_rectangle(GdkDrawable *drawable, GdkGC *gc, gboolean filled,
		gint x, gint y, gint width, gint height)
{
#ifndef WITHOUTOPENGL
	int x2, y2;
	GdkColor *h = &huex[sgc.hue];

	x2 = x + width;
	y2 = y + height;
	if (filled)
		glBegin(GL_POLYGON);
	else
		glBegin(GL_LINE_LOOP);
        glColor3us(h->red, h->green, h->blue);
        glVertex2i(x, sgc.screen_height - y);
        glVertex2i(x2, sgc.screen_height - y);
        glVertex2i(x2, sgc.screen_height - y2);
        glVertex2i(x, sgc.screen_height - y2);
	glEnd();
#else
	gdk_draw_rectangle(drawable, gc, filled, x, y, width, height);
#endif
}

void sng_scaled_rectangle(GdkDrawable *drawable,
	GdkGC *gc, gboolean filled, gint x, gint y, gint width, gint height)
{
	if (!clip_rectangle(&x, &y, &width, &height))
		return;
	gl_draw_rectangle(drawable, gc, filled, x * sgc.xscale, y * sgc.yscale,
		width * sgc.xscale, height * sgc.yscale);
}

void sng_scaled_bright_line(GdkDrawable *drawable,
	GdkGC *gc, gint x1, gint y1, gint x2, gint y2, int color)
{
	int sx1, sy1, sx2, sy2, dx, dy;

	if (abs(x1 - x2) > abs(y1 - y2)) {
		dx = 0;
		dy = 1;
	} else {
		dx = 1;
		dy = 0;
	}

	if (!clip_line(&sgc.c, &x1, &y1, &x2, &y2))
		return;

	sx1 = x1 * sgc.xscale;
	sx2 = x2 * sgc.xscale;
	sy1 = y1 * sgc.yscale;	
	sy2 = y2 * sgc.yscale;	

	sng_set_foreground(WHITE);	
	sng_gl_draw_line(drawable, gc, sx1, sy1, sx2, sy2);
	sng_set_foreground(color);
	sng_gl_draw_line(drawable, gc, sx1 - dx, sy1 - dy, sx2 - dx, sy2 - dy);
	sng_gl_draw_line(drawable, gc, sx1 + dx, sy1 + dy, sx2 + dx, sy2 + dy);
}

void sng_unscaled_bright_line(GdkDrawable *drawable,
	GdkGC *gc, gint x1, gint y1, gint x2, gint y2, int color)
{
	int dx,dy;

	if (abs(x1-x2) > abs(y1-y2)) {
		dx = 0;
		dy = 1;
	} else {
		dx = 1;
		dy = 0;
	}
	
	if (!clip_line(&sgc.c, &x1, &y1, &x2, &y2))
		return;

	sng_set_foreground(WHITE);
	sng_gl_draw_line(drawable, gc, x1,y1,x2,y2);
	sng_set_foreground(color);
	sng_gl_draw_line(drawable, gc, x1-dx,y1-dy,x2-dx,y2-dy);
	sng_gl_draw_line(drawable, gc, x1+dx,y1+dy,x2+dx,y2+dy);
}

void sng_scaled_arc(GdkDrawable *drawable, GdkGC *gc,
	gboolean filled, gint x, gint y, gint width, gint height, gint angle1, gint angle2)
{
	gdk_draw_arc(drawable, gc, filled, x * sgc.xscale, y * sgc.yscale,
			width * sgc.xscale, height * sgc.yscale, angle1, angle2);
}

void sng_use_unscaled_drawing_functions(void)
{
	sng_current_draw_line = sng_scaled_line;
	sng_current_draw_rectangle = sng_scaled_rectangle;
	sng_current_bright_line = sng_unscaled_bright_line;
	sng_current_draw_arc = gdk_draw_arc;
}

void sng_use_scaled_drawing_functions(void)
{
	sng_current_draw_line = sng_scaled_line;
	sng_current_draw_rectangle = sng_scaled_rectangle;
	sng_current_bright_line = sng_scaled_bright_line;
	sng_current_draw_arc = sng_scaled_arc;
}

void sng_use_thick_lines(void)
{
	sng_current_draw_line = sng_thick_scaled_line;
}

static void gl_draw_point(GdkDrawable *drawable, GdkGC *gc, int x, int y)
{
#ifndef WITHOUTOPENGL
	GdkColor *h = &huex[sgc.hue];

	glBegin(GL_POINTS);
        glColor3us(h->red, h->green, h->blue);
	glVertex2i(x, sgc.screen_height - y);
	glEnd();
#else
	gdk_draw_point(drawable, gc, x, y);
#endif
}

void sng_dotted_line_plot_func(int x, int y, void *context)
{
	struct sng_dotted_plot_func_context *c = context;

	c->i = (c->i + 1) % 10;
	if (c->i != 0)
		return;
	gl_draw_point(c->drawable, c->gc, x, y);
}

void sng_electric_line_plot_func(int x, int y, void *context)
{
	struct sng_dotted_plot_func_context *c = context;

	if (sng_rand(100) < 10)
		gl_draw_point(c->drawable, c->gc, x, y);
}

static void sng_bright_electric_line_plot_func(int x, int y, void *context)
{
	struct sng_dotted_plot_func_context *c = context;

	if (sng_rand(100) < 20) {
		sng_set_foreground(c->i);
		gl_draw_point(c->drawable, c->gc, x, y);
	}
}

void sng_draw_dotted_line(GdkDrawable *drawable,
	GdkGC *gc, gint x1, gint y1, gint x2, gint y2)
{
	struct sng_dotted_plot_func_context context;

	context.drawable = drawable;
	context.gc = gc;
	context.i = 0;

	if (!clip_line(&sgc.c, &x1, &y1, &x2, &y2))
		return;

	bline(x1 * sgc.xscale, y1 * sgc.yscale, x2 * sgc.xscale, y2 * sgc.yscale,
			sng_dotted_line_plot_func, &context);
}

void sng_draw_electric_line(GdkDrawable *drawable,
	GdkGC *gc, gint x1, gint y1, gint x2, gint y2)
{
	struct sng_dotted_plot_func_context context;

	context.drawable = drawable;
	context.gc = gc;
	context.i = 0;

	if (!clip_line(&sgc.c, &x1, &y1, &x2, &y2))
		return;

	bline(x1 * sgc.xscale, y1 * sgc.yscale, x2 * sgc.xscale, y2 * sgc.yscale,
			sng_electric_line_plot_func, &context);
}

static void sng_draw_bright_white_electric_line(GdkDrawable *drawable,
	GdkGC *gc, gint x1, gint y1, gint x2, gint y2, int color)
{
	struct sng_dotted_plot_func_context context;

	context.drawable = drawable;
	context.gc = gc;
	context.i = color;

	if (!clip_line(&sgc.c, &x1, &y1, &x2, &y2))
		return;

	bline(x1 * sgc.xscale, y1 * sgc.yscale, x2 * sgc.xscale, y2 * sgc.yscale,
			sng_bright_electric_line_plot_func, &context);
}

void sng_draw_laser_line(GdkDrawable *drawable, GdkGC *gc,
	gint x1, gint y1, gint x2, gint y2, int color)
{
	int dx, dy;

	if (abs(x1 - x2) > abs(y1 - y2)) {
		dx = 0;
		dy = 1;
	} else {
		dx = 1;
		dy = 0;
	}

	if (!clip_line(&sgc.c, &x1, &y1, &x2, &y2))
		return;

	sng_draw_bright_white_electric_line(drawable, gc, x1, y1, x2, y2, color);
	sng_set_foreground(color);
	sng_draw_electric_line(drawable, gc, x1 - dx, y1 - dy, x2 - dx, y2 - dy);
	sng_draw_electric_line(drawable, gc, x1 + dx, y1 + dy, x2 + dx, y2 + dy);
}

void sng_draw_vect_obj(GtkWidget *w, GdkGC *gc, struct my_vect_obj *v, int x, int y)
{
	int i, x1, y1, x2, y2;

	for (i = 0; i < v->npoints-1; i++) {
		if (v->p[i+1].x == LINE_BREAK)
			i += 2;
		x1 = x + v->p[i].x;
		y1 = y + v->p[i].y;
		x2 = x + v->p[i + 1].x;
		y2 = y + v->p[i + 1].y;
		sng_current_draw_line(w->window, gc, x1, y1, x2, y2); 
	}
}

/* Draws a letter in the given font at an absolute x,y coords on the screen. */
int sng_abs_xy_draw_letter(GtkWidget *w, GdkGC *gc, struct my_vect_obj **font, 
		unsigned char letter, int x, int y)
{
	int i, x1, y1, x2, y2;
	int minx, maxx, diff;

	if (letter == ' ' || letter == '\n' || letter == '\t' || font[letter] == NULL)
		return abs(font['Z']->p[0].x - font['Z']->p[1].x);

	minx = x + font[letter]->p[0].x;
	maxx = minx;
	for (i = 0; i < font[letter]->npoints-1; i++) {
		if (font[letter]->p[i+1].x == LINE_BREAK)
			i += 2;
		x1 = x + font[letter]->p[i].x;
		y1 = y + font[letter]->p[i].y;
		x2 = x + font[letter]->p[i + 1].x;
		y2 = y + font[letter]->p[i + 1].y;

		if (x1 < minx)
			minx = x1;
		if (x2 < minx)
			minx = x2;
		if (x1 > maxx)
			maxx = x1;
		if (x2 > maxx)
			maxx = x2;
		
		if (x1 > 0 && x2 > 0)
			sng_current_draw_line(w->window, gc, x1, y1, x2, y2); 
	}
	diff = abs(maxx - minx);
	/* if (diff == 0)
		return (abs(font['Z']->p[0].x - font['Z']->p[1].x) / 4); */
	return diff; 
}

/* Used for floating labels in the game. */
/* Draws a string at an absolute x,y position on the screen. */ 
void sng_abs_xy_draw_string(GtkWidget *w, GdkGC *gc, char *s, int font, int x, int y) 
{

	int i, dx;	
	int deltax = 0;

	for (i=0;s[i];i++) {
		dx = (font_scale[font]) + sng_abs_xy_draw_letter(w, gc, gamefont[font], s[i], x + deltax, y);  
		deltax += dx;
	}
}

void sng_abs_xy_draw_string_with_cursor(GtkWidget *w, GdkGC *gc, char *s,
			int font, int x, int y, int cursor_pos, int cursor_on) 
{

	int i, dx;	
	int deltax = 0;

	if (!cursor_on) {
		sng_abs_xy_draw_string(w, gc, s, font, x, y);
		return;
	}

	for (i = 0; s[i]; i++) {
		if (i == cursor_pos)
			sng_abs_xy_draw_letter(w, gc, gamefont[font], '_', x + deltax, y);
		dx = (font_scale[font]) + sng_abs_xy_draw_letter(w, gc, gamefont[font], s[i], x + deltax, y);  
		deltax += dx;
	}
	if (i == cursor_pos)
		sng_abs_xy_draw_letter(w, gc, gamefont[font], '_', x + deltax, y);
}

void sng_draw_point(GdkDrawable *drawable, GdkGC *gc, int x, int y)
{
	gl_draw_point(drawable, gc, x * sgc.xscale, y * sgc.yscale);
}

void sng_setup_colors(GtkWidget *w)
{
	int i;

	gdk_color_parse("white", &huex[WHITE]);
	gdk_color_parse("blue", &huex[BLUE]);
	gdk_color_parse("black", &huex[BLACK]);
	gdk_color_parse("green", &huex[GREEN]);
	gdk_color_parse("lime green", &huex[LIMEGREEN]);
	gdk_color_parse("darkgreen", &huex[DARKGREEN]);
	gdk_color_parse("yellow", &huex[YELLOW]);
	gdk_color_parse("red", &huex[RED]);
	gdk_color_parse("orange", &huex[ORANGE]);
	gdk_color_parse("cyan", &huex[CYAN]);
	gdk_color_parse("MAGENTA", &huex[MAGENTA]);
	gdk_color_parse("darkred", &huex[DARKRED]);
	gdk_color_parse("orange", &huex[AMBER]);
	gdk_color_parse("darkturquoise", &huex[DARKTURQUOISE]);
	gdk_color_parse("orangered", &huex[ORANGERED]);
	for (i = 0; i < NSHADESOFGRAY; i++) {
		huex[GRAY + i].red = (i * 32767 * 2) / 256;
		huex[GRAY + i].green = (i * 32767 * 2) / 256;
		huex[GRAY + i].blue = (i * 32767 * 2) / 256;
	}
	for (i = 0; i < NCOLORS + NSPARKCOLORS + NRAINBOWCOLORS + NSHADESOFGRAY; i++)
                gdk_colormap_alloc_color(gtk_widget_get_colormap(w),
				&huex[i], FALSE, FALSE);
	gtk_widget_modify_bg(w, GTK_STATE_NORMAL, &huex[BLACK]);
}

void sng_set_foreground(int c)
{
	sgc.hue = c;
#ifdef WITHOUTOPENGL
	gdk_gc_set_foreground(sgc.gc, &huex[c]);
#endif
}

void sng_set_gc(GdkGC *gc)
{
	sgc.gc = gc;
}

static void gl_draw_circle(int x, int y,  int r)
{
#ifndef WITHOUTOPENGL
	int i;
	GdkColor *h = &huex[sgc.hue];
 
	glBegin(GL_LINE_LOOP);
        glColor3us(h->red, h->green, h->blue);
	for (i = 0; i < 360; i += 2)
		glVertex2f(sgc.xscale * (x + cos(i * M_PI / 180.0) * r),
			(sgc.screen_height - y * sgc.yscale) + sin(i * M_PI / 180.0) * r * sgc.yscale);
	glEnd();
#endif
}

void sng_draw_circle(GdkDrawable *drawable, GdkGC *gc, gint x, gint y, gint r)
{
#ifndef WITHOUTOPENGL
	gl_draw_circle(x, y, r);
#else
	sng_current_draw_arc(drawable, gc, 0, x - r, y - r, r * 2, r * 2, 0, 360*64);
#endif
}

void sng_device_line(GdkDrawable *drawable, GdkGC *gc, int x1, int y1, int x2, int y2)
{
	sng_gl_draw_line(drawable, gc, x1, y1, x2, y2);
}

void sng_bright_device_line(GdkDrawable *drawable,
	GdkGC *gc, gint x1, gint y1, gint x2, gint y2, int color)
{
	int dx, dy;

	if (abs(x1 - x2) > abs(y1 - y2)) {
		dx = 0;
		dy = 1;
	} else {
		dx = 1;
		dy = 0;
	}
#if 0
	if (!clip_line(&sgc.c, &x1, &y1, &x2, &y2))
		return;
#endif

	sng_set_foreground(WHITE);
	sng_gl_draw_line(drawable, gc, x1, y1, x2, y2);
	sng_set_foreground(color);
	sng_gl_draw_line(drawable, gc, x1 - dx, y1 - dy, x2 - dx, y2 - dy);
	sng_gl_draw_line(drawable, gc, x1 + dx, y1 + dy, x2 + dx, y2 + dy);
}

void sng_filled_tri(GdkDrawable *drawable, GdkGC *gc,
			int x1, int y1, int x2, int y2, int x3, int y3)
{
#ifndef WITHOUTOPENGL
	GdkColor *h = &huex[sgc.hue];

        glBegin(GL_TRIANGLES);
        glColor3us(h->red, h->green, h->blue);
        glVertex2i(x1, sgc.screen_height - y1);
        glVertex2i(x2, sgc.screen_height - y2);
        glVertex2i(x3, sgc.screen_height - y3);
        glEnd();
#else
	GdkPoint tri[3];

	tri[0].x = x1;
	tri[0].y = y1;
	tri[1].x = x2;
	tri[1].y = y2;
	tri[2].x = x3;
	tri[2].y = y3;

	gdk_draw_polygon(drawable, gc, 1, tri, 3);
#endif
}

int sng_load_png_texture(const char * filename, int *w, int *h, char *whynot, int whynotlen)
{
#ifndef WITHOUTOPENGL
	int i, bit_depth, color_type, row_bytes;
	png_byte header[8];
	png_uint_32 tw, th;
	png_structp png_ptr = NULL;
	png_infop info_ptr = NULL;
	png_infop end_info = NULL;
	png_byte *image_data = NULL;
	png_bytep *row = NULL;
	GLuint texture = -1;

	FILE *fp = fopen(filename, "rb");
	if (!fp) {
		snprintf(whynot, whynotlen, "Failed to open '%s': %s",
			filename, strerror(errno));
		return -1;
	}

	fread(header, 1, 8, fp);
	if (png_sig_cmp(header, 0, 8)) {
		snprintf(whynot, whynotlen, "'%s' isn't a png file.",
			filename);
		goto cleanup;
	}

	png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING,
							NULL, NULL, NULL);
	if (!png_ptr) {
		snprintf(whynot, whynotlen,
			"png_create_read_struct() returned NULL");
		goto cleanup;
	}

	info_ptr = png_create_info_struct(png_ptr);
	if (!info_ptr) {
		snprintf(whynot, whynotlen,
			"png_create_info_struct() returned NULL");
		goto cleanup;
	}

	end_info = png_create_info_struct(png_ptr);
	if (!end_info) {
		snprintf(whynot, whynotlen,
			"2nd png_create_info_struct() returned NULL");
		goto cleanup;
	}

	if (setjmp(png_jmpbuf(png_ptr))) {
		snprintf(whynot, whynotlen, "libpng encounted an error");
		goto cleanup;
	}

	png_init_io(png_ptr, fp);
	png_set_sig_bytes(png_ptr, 8);
	png_read_info(png_ptr, info_ptr);
	png_get_IHDR(png_ptr, info_ptr, &tw, &th, &bit_depth, &color_type, NULL, NULL, NULL);

	if (w)
		*w = tw;
	if (h)
		*h = th;

	png_read_update_info(png_ptr, info_ptr);
	row_bytes = png_get_rowbytes(png_ptr, info_ptr);

	/* align to 4 byte boundary */
	if (row_bytes & 0x03)
		row_bytes += 4 - (row_bytes & 0x03);

	image_data = malloc(row_bytes * th * sizeof(png_byte) + 15);
	if (!image_data) {
		snprintf(whynot, whynotlen,
			"malloc failed in load_png_texture");
		goto cleanup;
	}

	row = malloc(th * sizeof(png_bytep));
	if (!row) {
		snprintf(whynot, whynotlen, "malloc failed in load_png_texture");
		goto cleanup;
	}

	for (i = 0; i < th; i++)
		row[i] = image_data + i * row_bytes;

	png_read_image(png_ptr, row);

	glGenTextures(1, &texture);
	if (texture == -1) {
		snprintf(whynot, whynotlen, "glGenTextures failed.");
		goto cleanup;
	}
	glBindTexture(GL_TEXTURE_2D, texture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, tw, th, 0, GL_RGBA, GL_UNSIGNED_BYTE, image_data);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

cleanup:
	if (row)
		free(row);
	if (image_data)
		free(image_data);
	png_destroy_read_struct(&png_ptr, &info_ptr, &end_info);
	fclose(fp);
	return texture;
#else
	snprintf(whynot, whynotlen, "load_png_texture: compiled without opengl support.");
	return -1;
#endif
}

