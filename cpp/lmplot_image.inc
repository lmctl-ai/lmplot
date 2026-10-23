/* Included by gnuplot's breaders.c (patches/0008-image-reader.patch) in place of the libgd
 * reader, which lmplot does not link. stb_image (vendor/stb_image.h, MIT) decodes PNG, JPEG,
 * GIF (first frame), BMP, TGA and PNM, so
 *     plot 'render.png' binary filetype=png with rgbimage   (or rgbalpha)
 *     set pixmap 1 'render.png' at screen 0.05,0.1 width screen 0.4
 * work as in a libgd build. Pixels are always expanded to RGBA. */
#define STBI_NO_STDIO_WRITE
#define STBI_NO_HDR
#define STBI_NO_LINEAR
#define STBI_NO_PSD
#define STBI_NO_PIC
#define STB_IMAGE_STATIC
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

static unsigned char *lmplot_img = NULL;
static int lmplot_img_w = 0, lmplot_img_h = 0;

void
gd_filetype_function(int filetype, char *filename)
{
    FILE *fp;
    int n = 0;
    (void) filetype;	/* stb_image detects the format from the bytes */

    if (lmplot_img) {
	stbi_image_free(lmplot_img);
	lmplot_img = NULL;
    }
    fp = loadpath_fopen(filename, "rb");
    if (!fp)
	int_error(NO_CARET, "Can't open data file \"%s\"", filename);
    lmplot_img = stbi_load_from_file(fp, &lmplot_img_w, &lmplot_img_h, &n, 4);
    fclose(fp);
    if (!lmplot_img)
	int_error(NO_CARET, "cannot read image \"%s\": %s", filename, stbi_failure_reason());

    df_pixeldata = lmplot_img;
    df_matrix_file = FALSE;
    df_binary_file = TRUE;

    df_bin_record[0].scan_skip[0] = 0;
    df_bin_record[0].scan_dim[0] = lmplot_img_w;
    df_bin_record[0].scan_dim[1] = lmplot_img_h;

    df_bin_record[0].scan_dir[0] = 1;
    df_bin_record[0].scan_dir[1] = -1;
    df_bin_record[0].scan_generate_coord = TRUE;
    df_bin_record[0].cart_scan[0] = DF_SCAN_POINT;
    df_bin_record[0].cart_scan[1] = DF_SCAN_LINE;

    df_extend_binary_columns(4);
    df_set_read_type(1, DF_UCHAR);
    df_set_read_type(2, DF_UCHAR);
    df_set_read_type(3, DF_UCHAR);
    df_set_read_type(4, DF_UCHAR);
    df_set_skip_before(1,0);

    df_no_use_specs = 4;
    use_spec[0].column = 1;
    use_spec[1].column = 2;
    use_spec[2].column = 3;
    use_spec[3].column = 4;
}

int
df_libgd_get_pixel(int i, int j, int component)
{
    if (!lmplot_img || i < 0 || j < 0 || i >= lmplot_img_w || j >= lmplot_img_h
	|| component < 0 || component > 3)
	return 0;
    return lmplot_img[((size_t) j * lmplot_img_w + i) * 4 + component];
}

TBOOLEAN
df_read_pixmap( t_pixmap *pixmap )
{
    int i, j;
    coordval *pixel;

    /* Create a blank record that gd_filetype_function can write into */
    df_add_binary_records(1, DF_CURRENT_RECORDS);
    gd_filetype_function(0, pixmap->filename);
    pixmap->ncols = df_bin_record[0].scan_dim[0];
    pixmap->nrows = df_bin_record[0].scan_dim[1];
    pixmap->image_data = gp_realloc( pixmap->image_data,
			4. * sizeof(coordval) * pixmap->ncols * pixmap->nrows, "pixmap");

    /* Same layout as the libgd reader: r, g, b in 0..1, alpha in 0..255 */
    pixel = pixmap->image_data;
    for (i=0; i<pixmap->nrows; i++)
    for (j=0; j<pixmap->ncols; j++) {
	*pixel++ = (coordval)df_libgd_get_pixel(j,i,0) / 255.;
	*pixel++ = (coordval)df_libgd_get_pixel(j,i,1) / 255.;
	*pixel++ = (coordval)df_libgd_get_pixel(j,i,2) / 255.;
	*pixel++ = (coordval)df_libgd_get_pixel(j,i,3);
    }
    /* The pixels now live in pixmap->image_data; release the decoded image. */
    stbi_image_free(lmplot_img);
    lmplot_img = NULL;
    df_pixeldata = NULL;
    return TRUE;
}
