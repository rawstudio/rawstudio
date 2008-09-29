/*
 * Copyright (C) 2006-2008 Anders Brander <anders@brander.dk> and 
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
#include <stdint.h>
#include "dcraw_api.h"
#include "rs-arch.h"
#include "rs-cms.h"

/* Check for thread support */
#if (!defined(G_THREADS_ENABLED) || defined(G_THREADS_IMPL_NONE))
#error GLib was not compiled with thread support, Rawstudio needs threads - sorry.
#endif

#define ORIENTATION_RESET(orientation) orientation = 0
#define ORIENTATION_90(orientation) orientation = (orientation&4) | ((orientation+1)&3)
#define ORIENTATION_180(orientation) orientation = (orientation^2)
#define ORIENTATION_270(orientation) orientation = (orientation&4) | ((orientation+3)&3)
#define ORIENTATION_FLIP(orientation) orientation = (orientation^4)
#define ORIENTATION_MIRROR(orientation) orientation = ((orientation&4)^4) | ((orientation+2)&3)

enum {
	MASK_EXPOSURE = 1,
	MASK_SATURATION = 2,
	MASK_HUE = 4,
	MASK_CONTRAST = 8,
	MASK_WARMTH = 16,
	MASK_TINT = 32,
	MASK_CURVE = 64,
	MASK_SHARPEN = 128,
	MASK_ALL = 0x00ffffff,
};

#define MASK_WB (MASK_WARMTH|MASK_TINT)

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

/* The problem with the align GNU extension, is that it doesn't work
 * reliably with local variables, depending on versions and targets.
 * So better use a tricky define to ensure alignment even in these
 * cases. */
#define RS_DECLARE_ALIGNED(type, name, sizex, sizey, alignment) \
	type name##_s[(sizex)*(sizey)+(alignment)-1];	\
	type * name = (type *)(((uintptr_t)name##_s+(alignment - 1))&~((uintptr_t)(alignment)-1))

typedef struct _RSStore RSStore;

/* Opaque definition, declared in rs-batch.h */
typedef struct _RS_QUEUE RS_QUEUE;

/* Defined in rs-color-transform.c */
typedef struct _RSColorTransform RSColorTransform;

/* Defined in rs-metadata.h */
typedef struct _RSMetadata RSMetadata;

/* Defined in rs-image.h */
typedef struct _rs_image16 RS_IMAGE16;

typedef struct {double coeff[3][3]; } RS_MATRIX3;
typedef struct {int coeff[3][3]; } RS_MATRIX3Int;
typedef struct {double coeff[4][4]; } RS_MATRIX4;
typedef struct {int coeff[4][4]; } RS_MATRIX4Int;

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
	GtkObject *sharpen;
	GtkWidget *curve;
} RS_SETTINGS;

typedef struct {
	gdouble exposure;
	gdouble saturation;
	gdouble hue;
	gdouble contrast;
	gdouble warmth;
	gdouble tint;
	gdouble sharpen;
	guint curve_nknots;
	gfloat *curve_knots;
} RS_SETTINGS_DOUBLE;

typedef struct _photo {
	GObject parent;
	gchar *filename;
	RS_IMAGE16 *input;
	RS_SETTINGS_DOUBLE *settings[3];
	gint priority;
	guint orientation;
	RSMetadata *metadata;
	RS_RECT *crop;
	gdouble angle;
	gboolean exported;
	gboolean dispose_has_run;
} RS_PHOTO;

typedef struct {
	gboolean mute_signals_to_photo;
	RS_PHOTO *photo;
	RS_SETTINGS_DOUBLE *settings_buffer;
	RS_SETTINGS *settings[3];
	gint current_setting;
	RS_IMAGE16 *histogram_dataset;
	GtkWidget *histogram;
	RSColorTransform *histogram_transform;
	RS_QUEUE *queue;
	RS_CMS *cms;
	RSStore *store;

	/* These should be moved to a future RS_WINDOW */
	GtkWidget *window;
	GtkWidget *iconbox;
	GtkWidget *toolbox;
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
	gboolean (*save)(RS_PHOTO *photo, const gchar *filename, gint filetype, gint width, gint height, gboolean keep_aspect, gdouble scale, gint snapshot, RS_CMS *cms);
	struct _rs_filetype *next;
} RS_FILETYPE;

void rs_settings_to_rs_settings_double(RS_SETTINGS *rs_settings, RS_SETTINGS_DOUBLE *rs_settings_double);
void rs_settings_double_to_rs_settings(RS_SETTINGS_DOUBLE *rs_settings_double, RS_SETTINGS *rs_settings, gint mask);
void rs_settings_reset(RS_SETTINGS *rss, guint mask);
gboolean rs_photo_save(RS_PHOTO *photo, const gchar *filename, gint filetype,
	gint width, gint height, gboolean keep_aspect, gdouble scale, gint snapshot, RS_CMS *cms);
RS_SETTINGS_DOUBLE *rs_settings_double_new(void);
void rs_settings_double_copy(const RS_SETTINGS_DOUBLE *in, RS_SETTINGS_DOUBLE *out, gint mask);
void rs_settings_double_free(RS_SETTINGS_DOUBLE *rssd);
RS_BLOB *rs_new();
void rs_free(RS_BLOB *rs);
void rs_set_photo(RS_BLOB *rs, RS_PHOTO *photo);
void rs_set_snapshot(RS_BLOB *rs, gint snapshot);
void rs_white_black_point(RS_BLOB *rs);
void rs_apply_settings_from_double(RS_SETTINGS *rss, RS_SETTINGS_DOUBLE *rsd, gint mask);

/* Contains a list of supported filetypes */
extern RS_FILETYPE *filetypes;

#endif /* RS_RAWSTUDIO_H */
