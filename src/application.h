/*
 * Copyright (C) 2006-2009 Anders Brander <anders@brander.dk> and 
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
#ifndef APPLICATION_H
#define APPLICATION_H

#include <rawstudio.h>
#include <gtk/gtk.h>
#include <glib.h>
#include <lcms.h>
#include <stdint.h>
#include "rs-arch.h"
#include "rs-cms.h"
#include "rs-library.h"

/* Check for thread support */
#if (!defined(G_THREADS_ENABLED) || defined(G_THREADS_IMPL_NONE))
#error GLib was not compiled with thread support, Rawstudio needs threads - sorry.
#endif

typedef struct _RSStore RSStore;

/* Opaque definition, declared in rs-batch.h */
typedef struct _RS_QUEUE RS_QUEUE;

typedef struct _photo {
	GObject parent;
	gchar *filename;
	RS_IMAGE16 *input;
	RSSettings *settings[3];
	gulong settings_signal[3];
	gint priority;
	guint orientation;
	RSMetadata *metadata;
	RS_RECT *crop;
	gdouble angle;
	gboolean exported;
	gboolean dispose_has_run;
} RS_PHOTO;

typedef struct {
	RS_PHOTO *photo;
	RSSettings *settings_buffer;
	GtkWidget *curve[3];
	gint current_setting;
	RS_QUEUE *queue;
	RS_CMS *cms;
	RSStore *store;
	RS_LIBRARY *library;

	/* These should be moved to a future RS_WINDOW */
	GtkWidget *window;
	GtkWidget *iconbox;
	GtkWidget *tools;
	GtkWidget *toolbox;
	GtkWidget *preview;

	/* Generic filter chain */
	RSFilter *filter_input;
	RSFilter *filter_demosaic;
	RSFilter *filter_lensfun;
	RSFilter *filter_rotate;
	RSFilter *filter_crop;
	RSFilter *filter_end;
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

gboolean rs_photo_save(RS_PHOTO *photo, RSOutput *output,
	gint width, gint height, gboolean keep_aspect, gdouble scale, gint snapshot, RS_CMS *cms);
RS_BLOB *rs_new();
void rs_free(RS_BLOB *rs);
void rs_set_photo(RS_BLOB *rs, RS_PHOTO *photo);
void rs_white_black_point(RS_BLOB *rs);

#endif /* APPLICATION_H */
