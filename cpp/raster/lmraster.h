/* lmraster: a small anti-aliased software rasterizer used by lmplot's native
 * png terminal. C interface so gnuplot's C terminal code can drive it.
 * Colors are 0xAARRGGBB with straight (non-premultiplied) alpha, alpha 255 =
 * opaque. Coordinates are device pixels, origin top-left, y down; pixel (i,j)
 * covers [i,i+1) x [j,j+1). */
#ifndef LMRASTER_H
#define LMRASTER_H

#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct lmr lmr;

enum { LMR_CAP_BUTT = 0, LMR_CAP_ROUND = 1, LMR_CAP_SQUARE = 2 };
enum { LMR_JOIN_MITER = 0, LMR_JOIN_ROUND = 1, LMR_JOIN_BEVEL = 2 };
enum { LMR_FAMILY_SANS = 0, LMR_FAMILY_SERIF = 1, LMR_FAMILY_MONO = 2, LMR_FAMILY_SYMBOL = 3 };

typedef struct lmr_font {
    int family;  /* LMR_FAMILY_* */
    int bold;
    int italic;
} lmr_font;

lmr *lmr_create(int width, int height, unsigned bg_argb);
void lmr_destroy(lmr *r);
int lmr_width(const lmr *r);
int lmr_height(const lmr *r);
unsigned lmr_get_pixel(const lmr *r, int x, int y);
void lmr_set_pixel(lmr *r, int x, int y, unsigned argb);

/* Fill one closed polygon (xy = n x,y pairs), nonzero winding. */
void lmr_fill_polygon(lmr *r, const double *xy, int n, unsigned argb);
/* Same, but not anti-aliased: a pixel is set iff its centre is inside. Adjacent polygons never
 * leave seams or overlap (pm3d surfaces, boxes). */
void lmr_fill_polygon_crisp(lmr *r, const double *xy, int n, unsigned argb);
/* Fill the polygon with a hatch pattern (pattern 0..9, see gnuplot "fill pattern"),
 * hatch lines in argb over an optional background (bg_argb alpha 0 = none). */
void lmr_fill_polygon_pattern(lmr *r, const double *xy, int n, unsigned argb, unsigned bg_argb, int pattern);
/* Stroke an open polyline. dash = on/off lengths (ndash may be 0 for solid). */
void lmr_stroke(lmr *r, const double *xy, int n, double width, unsigned argb, int cap, int join,
                const double *dash, int ndash);
/* Blend a w x h rectangle of straight-alpha ARGB pixels onto the canvas through the
 * affine map from image space (0..w, 0..h) to the device defined by three corners:
 * top-left (x0,y0), top-right (x1,y1), bottom-left (x2,y2). Nearest-neighbour. */
void lmr_draw_image(lmr *r, const unsigned *argb, int w, int h, double x0, double y0, double x1, double y1,
                    double x2, double y2);

/* Text. px is the em size in pixels. angle in radians, counter-clockwise on screen.
 * (x,y) is the left end of the baseline. */
double lmr_text_width(lmr_font font, double px, const char *utf8);
void lmr_text_draw(lmr *r, lmr_font font, double px, const char *utf8, double x, double y, double angle,
                   unsigned argb);
/* Nominal metrics for the font at px: ascent above and descent below the baseline (positive). */
void lmr_font_metrics(lmr_font font, double px, double *ascent, double *descent);

/* Write a PNG (RGBA, or RGB when fully opaque). Returns 0 on success. */
int lmr_write_png(const lmr *r, FILE *fp);
/* Bounding box of non-background pixels; for "crop". Returns 0 if the canvas is blank. */
int lmr_content_bbox(const lmr *r, unsigned bg_argb, int *x0, int *y0, int *x1, int *y1);
/* New canvas holding the sub-rectangle [x0,x1] x [y0,y1] of r. */
lmr *lmr_crop(const lmr *r, int x0, int y0, int x1, int y1);

extern const unsigned char lmr_font_sans[], lmr_font_sans_bold[], lmr_font_serif[], lmr_font_mono[];

#ifdef __cplusplus
}
#endif
#endif
