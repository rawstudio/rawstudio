/*
 * * Copyright (C) 2006-2011 Anders Brander <anders@brander.dk>, 
 * * Anders Kvist <akv@lnxbx.dk> and Klaus Post <klauspost@gmail.com>
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
#include <stdint.h>
#include <sqlite3.h>

/* Check for thread support */
#if (!defined(G_THREADS_ENABLED) || defined(G_THREADS_IMPL_NONE))
#error GLib was not compiled with thread support, Rawstudio needs threads - sorry.
#endif

typedef struct _RSStore RSStore;

/* Opaque definition, declared in rs-batch.h */
typedef struct _RS_QUEUE RS_QUEUE;

typedef enum {
	MAIN_SIGNAL_NONE,
	MAIN_SIGNAL_LOADING,
	MAIN_SIGNAL_CANCEL_LOAD,
	MAIN_SIGNAL_IDLE
} RS_MAIN_SIGNAL;

typedef struct _photo {
	GObject parent;
	gchar *filename;
	RS_IMAGE16 *input;
	RSFilterResponse *input_response;
	RSSettings *settings[3];
	gulong settings_signal[3];
	gint priority;
	guint orientation;
	RSMetadata *metadata;
	RS_RECT *crop;
	gdouble angle;
	gboolean exported;
	gboolean enfuse;
	RSColorSpace *embedded_profile;
	RSDcpFile *dcp;
	RSIccProfile *icc;
	gboolean dispose_has_run;
	RSFilter *thumbnail_filter;
	RS_RECT *proposed_crop;
	RSFilter *auto_wb_filter;
	gdouble *auto_wb_mul;
	RS_MAIN_SIGNAL* signal;
	gint time_offset;
	gdouble lon;
	gdouble lat;
	gdouble ele;
} RS_PHOTO;

typedef struct {
	RS_PHOTO *photo;
	RSSettings *settings_buffer;
	RSDcpFile *dcp_buffer;
	RSIccProfile *icc_buffer;
	RS_RECT crop_buffer;
	gdouble angle_buffer;
	guint orientation_buffer;
	gint time_offset_buffer;
	gdouble coord_lon_buffer;
	gdouble coord_lat_buffer;
	gint current_setting;
	RS_QUEUE *queue;
	RSStore *store;
	RS_MAIN_SIGNAL signal; 
	gchar *post_open_event;
	GHashTable *enfuse_cache;
	gboolean slideshow_running;

	/* These should be moved to a future RS_WINDOW */
	GtkWidget *window;
	gboolean window_fullscreen;
	GtkWidget *iconbox;
	GtkWidget *tools;
	GtkWidget *toolbox;
	GtkWidget *preview;
	GtkWidget *window_preview_screen;

	/* Generic filter chain */
	RSFilter *filter_input;
	RSFilter *filter_demosaic;
	RSFilter *filter_fuji_rotate;
	RSFilter *filter_demosaic_cache;
	RSFilter *filter_end;
} RS_BLOB;

gboolean rs_photo_save(RS_PHOTO *photo, RSFilter *prior_to_resample, RSOutput *output,
	gint width, gint height, gboolean keep_aspect, gdouble scale, gint snapshot);
gboolean rs_photo_copy_to_clipboard(RS_PHOTO *photo, RSFilter *prior_to_resample, gint width, gint height, gboolean keep_aspect, gdouble scale, gint snapshot);
RS_BLOB *rs_new(void);
void rs_free(RS_BLOB *rs);
/* Cheater function to get the main blob - use carefully! */
RS_BLOB* rs_get_blob(void);
void rs_set_photo(RS_BLOB *rs, RS_PHOTO *photo);
void rs_white_black_point(RS_BLOB *rs);

#endif /* APPLICATION_H */
