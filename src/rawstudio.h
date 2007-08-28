/*
 * Copyright (C) 2006, 2007 Anders Brander <anders@brander.dk> and 
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
#ifndef RS_RAWSTUDIO_H
#define RS_RAWSTUDIO_H

#include <gtk/gtk.h>
#include <glib.h>
#include <lcms.h>
#include "dcraw_api.h"
#include "rs-arch.h"
#include "rs-cms.h"
#include "rs-curve.h"

#define PITCH(width) ((((width)+15)/16)*16)

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
	MASK_CURVE = 64,
	MASK_ALL = 127
};

#define MASK_WB (MASK_WARMTH|MASK_TINT)

enum {
	MAKE_UNKNOWN = 0,
	MAKE_CANON,
	MAKE_NIKON,
	MAKE_MINOLTA,
	MAKE_PANASONIC,
};

enum {
	MASK_OVER = 128,
	MASK_UNDER = 64,
};

enum {
	RS_CMS_PROFILE_IN,
	RS_CMS_PROFILE_DISPLAY,
	RS_CMS_PROFILE_EXPORT
};

enum {
	ROI_GRID_NONE = 0,
	ROI_GRID_GOLDEN,
	ROI_GRID_THIRDS,
	ROI_GRID_GOLDEN_TRIANGLES1,
	ROI_GRID_GOLDEN_TRIANGLES2,
	ROI_GRID_HARMONIOUS_TRIANGLES1,
	ROI_GRID_HARMONIOUS_TRIANGLES2,
};

#if __GNUC__ >= 3
#define likely(x) __builtin_expect (!!(x), 1)
#define unlikely(x) __builtin_expect (!!(x), 0)
#define align(x) __attribute__ ((aligned (x)))
#define __deprecated __attribute__ ((deprecated))
#else
#define likely(x) (x)
#define unlikely(x) (x)
#define align(x)
#define __deprecated
#endif

typedef struct _RSStore RSStore;

/* Opaque definition, declared in rs-batch.h */
typedef struct _RS_QUEUE_ELEMENT RS_QUEUE_ELEMENT;
typedef struct _RS_QUEUE RS_QUEUE;

/* Defined in rs-color-transform.c */
typedef struct _RS_COLOR_TRANSFORM RS_COLOR_TRANSFORM;

typedef struct {double coeff[3][3]; } RS_MATRIX3;
typedef struct {int coeff[3][3]; } RS_MATRIX3Int;
typedef struct {double coeff[4][4]; } RS_MATRIX4;
typedef struct {int coeff[4][4]; } RS_MATRIX4Int;

typedef struct {
	gint w;
	gint h;
	gint rowstride;
	guint channels;
	guint pixelsize; /* the size of a pixel in CHARS */
	guint orientation;
	guchar *pixels;
} RS_IMAGE8;

typedef struct _rs_image16 {
	gint w;
	gint h;
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
	GtkWidget *curve;
} RS_SETTINGS;

typedef struct {
	gdouble exposure;
	gdouble saturation;
	gdouble hue;
	gdouble contrast;
	gdouble warmth;
	gdouble tint;
	guint curve_nknots;
	gfloat *curve_knots;
} RS_SETTINGS_DOUBLE;

typedef struct _metadata {
	gint make;
	gchar *make_ascii;
	gchar *model_ascii;
	gushort orientation;
	gfloat aperture;
	gushort iso;
	gfloat shutterspeed;
	guint thumbnail_start;
	guint thumbnail_length;
	guint preview_start;
	guint preview_length;
	guint16 preview_planar_config;
	guint preview_width;
	guint preview_height;
	guint16 preview_bits [3];
	gdouble cam_mul[4];
	gdouble contrast;
	gdouble saturation;
	gdouble sharpness;
	gdouble color_tone;
	gshort focallength;
	RS_MATRIX4 adobe_coeff;
	gpointer data;
} RS_METADATA;

typedef struct _photo {
	gchar *filename;
	RS_IMAGE16 *input;
	RS_SETTINGS_DOUBLE *settings[3];
	gint priority;
	guint orientation;
	RS_METADATA *metadata;
	RS_RECT *crop;
	gdouble angle;
	gboolean exported;
} RS_PHOTO;

typedef struct {
	gboolean in_use;
	RS_PHOTO *photo;
	RS_SETTINGS_DOUBLE *settings_buffer;
	RS_SETTINGS *settings[3];
	gint current_setting;
	RS_IMAGE16 *histogram_dataset;
	guint histogram_table[3][256];
	GtkImage *histogram_image;
	RS_COLOR_TRANSFORM *histogram_transform;
	GArray *batch_queue;
	RS_QUEUE *queue;
	RS_CMS *cms;
	RSStore *store;
	GtkWidget *preview;
} RS_BLOB;

enum {
	FILETYPE_RAW,
	FILETYPE_JPEG,
	FILETYPE_PNG,
	FILETYPE_TIFF8,
	FILETYPE_TIFF16,
};

typedef struct _rs_filetype {
	gchar *id;
	gint filetype;
	const gchar *ext;
	gchar *description;
	RS_PHOTO *(*load)(const gchar *);
	GdkPixbuf *(*thumb)(const gchar *);
	void (*load_meta)(const gchar *, RS_METADATA *);
	gboolean (*save)(RS_PHOTO *photo, const gchar *filename, gint filetype, gint width, gint height, gdouble scale, gint snapshot, RS_CMS *cms);
	struct _rs_filetype *next;
} RS_FILETYPE;

void rs_local_cachedir(gboolean new_value);
void rs_load_gdk(gboolean new_value);
void rs_update_preview(RS_BLOB *rs);
void rs_reset(RS_BLOB *rs);
void rs_settings_to_rs_settings_double(RS_SETTINGS *rs_settings, RS_SETTINGS_DOUBLE *rs_settings_double);
void rs_settings_double_to_rs_settings(RS_SETTINGS_DOUBLE *rs_settings_double, RS_SETTINGS *rs_settings);
void rs_settings_reset(RS_SETTINGS *rss, guint mask);
RS_PHOTO *rs_photo_new();
void rs_photo_free(RS_PHOTO *photo);
gboolean rs_photo_save(RS_PHOTO *photo, const gchar *filename, gint filetype,
	gint width, gint height, gdouble scale, gint snapshot, RS_CMS *cms);
void rs_settings_double_copy(RS_SETTINGS_DOUBLE *in, RS_SETTINGS_DOUBLE *out, gint mask);
RS_METADATA *rs_metadata_new();
void rs_metadata_free(RS_METADATA *metadata);
void rs_metadata_normalize_wb(RS_METADATA *meta);
void rs_photo_flip(RS_PHOTO *photo);
void rs_photo_mirror(RS_PHOTO *photo);
void rs_photo_rotate(RS_PHOTO *photo, gint quarterturns, gdouble angle);
void rs_photo_set_crop(RS_PHOTO *photo, RS_RECT *crop);
RS_RECT *rs_photo_get_crop(RS_PHOTO *photo);
RS_BLOB *rs_new();
void rs_free(RS_BLOB *rs);
void rs_photo_close(RS_PHOTO *photo);
RS_FILETYPE *rs_filetype_get(const gchar *filename, gboolean load);
gchar *rs_dotdir_get(const gchar *filename);
gchar *rs_thumb_get_name(const gchar *src);
void rs_white_black_point(RS_BLOB *rs);
void rs_set_wb_auto(RS_BLOB *rs);
void rs_set_wb_from_color(RS_BLOB *rs, gdouble r, gdouble g, gdouble b);
void rs_set_wb_from_mul(RS_BLOB *rs, gdouble *mul);
void rs_set_wb(RS_BLOB *rs, gfloat warmth, gfloat tint);
void rs_render_pixel_to_srgb(RS_BLOB *rs, gint x, gint y, guchar *dest);
void rs_apply_settings_from_double(RS_SETTINGS *rss, RS_SETTINGS_DOUBLE *rsd, gint mask);
void rs_rect_scale(RS_RECT *in, RS_RECT *out, gdouble scale);
void rs_rect_union(RS_RECT *a, RS_RECT *b, RS_RECT *destination);
void rs_rect_normalize(RS_RECT *in, RS_RECT *out);
gboolean rs_shutdown(GtkWidget *dummy1, GdkEvent *dummy2, RS_BLOB *rs);
#if !GLIB_CHECK_VERSION(2,8,0)
int g_mkdir_with_parents (const gchar *pathname, int mode);
int g_access (const gchar *filename, int mode);
#endif

extern void
(*rs_photo_open_dcraw_apply_black_and_shift)(dcraw_data *raw, RS_PHOTO *photo) __rs_optimized;

extern void
rs_photo_open_dcraw_apply_black_and_shift_c(dcraw_data *raw, RS_PHOTO *photo);

#if defined (__i386__) || defined (__x86_64__)
extern void
rs_photo_open_dcraw_apply_black_and_shift_mmx(dcraw_data *raw, RS_PHOTO *photo);
#endif

/* Contains a list of supported filetypes */
extern RS_FILETYPE *filetypes;

#endif /* RS_RAWSTUDIO_H */
