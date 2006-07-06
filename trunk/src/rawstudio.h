/*
 * Copyright (C) 2006 Anders Brander <anders@brander.dk> and 
 * Anders Kvist <akv@lnxbx.dk>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#define PITCH(width) ((((width)+31)/32)*32)

#define SWAP( a, b ) a ^= b ^= a ^= b

#define DOTDIR ".rawstudio"
#define HISTOGRAM_DATASET_WIDTH (250)

#define ORIENTATION_RESET(orientation) orientation = 0
#define ORIENTATION_90(orientation) orientation = (orientation&4) | ((orientation+1)&3)
#define ORIENTATION_180(orientation) orientation = (orientation^2)
#define ORIENTATION_270(orientation) orientation = (orientation&4) | ((orientation+3)&3)
#define ORIENTATION_FLIP(orientation) orientation = (orientation^4)
#define ORIENTATION_MIRROR(orientation) orientation = ((orientation&4)^4) | ((orientation+2)&3)

#define GETVAL(adjustment) \
	gtk_adjustment_get_value((GtkAdjustment *) adjustment)
#define SETVAL(adjustment, value) \
	gtk_adjustment_set_value((GtkAdjustment *) adjustment, value)

enum {
	MASK_EXPOSURE = 1,
	MASK_SATURATION = 2,
	MASK_HUE = 4,
	MASK_CONTRAST = 8,
	MASK_WARMTH = 16,
	MASK_TINT = 32,
	MASK_ALL = 63
};

#define MASK_WB (MASK_WARMTH|MASK_TINT)

enum {
	MAKE_UNKNOWN = 0,
	MAKE_CANON,
	MAKE_NIKON,
};

enum {
	MASK_OVER = 128,
	MASK_UNDER = 64,
};

enum {
_MMX = 1,
_SSE = 2,
_CMOV = 4,
_3DNOW = 8
};

#if __GNUC__ >= 3
#define likely(x) __builtin_expect (!!(x), 1)
#define unlikely(x) __builtin_expect (!!(x), 0)
#define align(x) __attribute__ ((aligned (x)))
#else
#define likely(x) (x)
#define unlikely(x) (x)
#define align(x)
#endif

typedef struct {
	guint w;
	guint h;
	gint pitch;
	gint rowstride;
	guint channels;
	guint pixelsize; /* the size of a pixel in CHARS */
	guint orientation;
	guchar *pixels;
} RS_IMAGE8;

typedef struct {
	guint w;
	guint h;
	gint pitch;
	gint rowstride;
	guint channels;
	guint pixelsize; /* the size of a pixel in SHORTS */
	guint orientation;
	gushort *pixels;
} RS_IMAGE16;

typedef struct {
	gint x1;
	gint y1;
	gint x2;
	gint y2;
} RS_RECT;

typedef struct {
	GtkObject *exposure;
	GtkObject *saturation;
	GtkObject *hue;
	GtkObject *contrast;
	GtkObject *warmth;
	GtkObject *tint;
} RS_SETTINGS;

typedef struct {
	gdouble exposure;
	gdouble saturation;
	gdouble hue;
	gdouble contrast;
	gdouble warmth;
	gdouble tint;
} RS_SETTINGS_DOUBLE;

typedef struct _metadata {
	gint make;
	gushort orientation;
	gfloat aperture;
	gushort iso;
	gfloat shutterspeed;
	guint thumbnail_start;
	guint thumbnail_length;
	guint preview_start;
	guint preview_length;
	gdouble cam_mul[4];
} RS_METADATA;

typedef struct _photo {
	gboolean active;
	const gchar *filename;
	RS_IMAGE16 *input;
	RS_IMAGE16 *scaled;
	RS_IMAGE8 *preview;
	RS_IMAGE8 *mask;
	RS_SETTINGS *settings[3];
	gint current_setting;
	gint priority;
	guint orientation;
	RS_METADATA *metadata;
	RS_MATRIX4Int mati;
	RS_MATRIX4 mat;
	gfloat pre_mul[4];
} RS_PHOTO;

typedef struct {
	gboolean in_use;
	RS_PHOTO *photo;
	RS_SETTINGS_DOUBLE *settings_buffer;
	GtkObject *scale;
	gdouble gamma;
	guint preview_scale;
	RS_RECT *preview_exposed;
	RS_IMAGE16 *histogram_dataset;
	guint histogram_table[3][256];
	GtkImage *histogram_image;
	GtkWidget *preview_drawingarea;
	gboolean preview_idle_render;
	gboolean preview_done;
	GdkPixmap *preview_backing;
	gint preview_idle_render_lastrow;
	gboolean show_exposure_overlay;
	GArray *batch_queue;
} RS_BLOB;

enum {
	FILETYPE_RAW,
	FILETYPE_GDK,
};

typedef struct {
	const gchar *ext;
	gint filetype;
	void (*load)(RS_PHOTO *, const gchar *);
	GdkPixbuf *(*thumb)(const gchar *);
	void (*load_meta)(const gchar *, RS_METADATA *);
} RS_FILETYPE;

void rs_local_cachedir(gboolean new_value);
void rs_load_gdk(gboolean new_value);
void update_preview(RS_BLOB *rs);
void update_preview_region(RS_BLOB *rs, RS_RECT *region);
inline void rs_render(RS_PHOTO *photo, gint width, gint height, gushort *in,
	gint in_rowstride, gint in_channels, guchar *out, gint out_rowstride);
void rs_reset(RS_BLOB *rs);
void rs_settings_reset(RS_SETTINGS *rss, guint mask);
RS_PHOTO *rs_photo_new();
void rs_photo_free(RS_PHOTO *photo);
RS_METADATA *rs_metadata_new();
void rs_metadata_free(RS_METADATA *metadata);
RS_BLOB *rs_new();
void rs_free(RS_BLOB *rs);
void rs_photo_close(RS_PHOTO *photo);
RS_FILETYPE *rs_filetype_get(const gchar *filename, gboolean load);
gchar *rs_dotdir_get(const gchar *filename);
gchar *rs_thumb_get_name(const gchar *src);
void rs_set_wb_auto(RS_BLOB *rs);
void rs_set_wb_from_pixels(RS_BLOB *rs, gint x, gint y);
void rs_set_wb_from_color(RS_BLOB *rs, gdouble r, gdouble g, gdouble b);
void rs_set_wb_from_mul(RS_BLOB *rs, gdouble *mul);
void rs_set_wb(RS_BLOB *rs, gfloat warmth, gfloat tint);
void rs_apply_settings_from_double(RS_SETTINGS *rss, RS_SETTINGS_DOUBLE *rsd, gint mask);
gboolean rs_shutdown(GtkWidget *dummy1, GdkEvent *dummy2, RS_BLOB *rs);
#if !GLIB_CHECK_VERSION(2,8,0)
int g_mkdir_with_parents (const gchar *pathname, int mode);
#endif
