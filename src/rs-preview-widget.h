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

#ifndef RS_PREVIEW_WIDGET_H
#define RS_PREVIEW_WIDGET_H

#include <gtk/gtk.h>
#include "application.h"

typedef struct _RSPreviewWidget            RSPreviewWidget;
typedef struct _RSPreviewWidgetClass       RSPreviewWidgetClass;

struct _RSPreviewWidgetClass
{
	GtkTableClass parent_class;
};

typedef struct _rs_preview_callback_data {
	gushort *pixel;
	guchar pixel8[3];
	gfloat pixelfloat[3];
	gint x;
	gint y;
} RS_PREVIEW_CALLBACK_DATA;

extern GType rs_preview_widget_get_type (void);

/**
 * Creates a new RSPreviewWidget
 * @return A new RSPreviewWidget
 */
extern GtkWidget *rs_preview_widget_new(GtkWidget *toolbox);

/**
 * Select zoom-to-fit of a RSPreviewWidget
 * @param preview A RSPreviewWidget
 * @param zoom_to_fit Set to TRUE to enable zoom-to-fit.
 */
extern void rs_preview_widget_set_zoom_to_fit(RSPreviewWidget *preview, gboolean zoom_to_fit);

/**
 * Sets active photo of a RSPreviewWidget
 * @param preview A RSPreviewWidget
 * @param photo A RS_PHOTO
 */
extern void rs_preview_widget_set_photo(RSPreviewWidget *preview, RS_PHOTO *photo);

/**
 * Set input filter for a RSPreviewWidget
 * @param preview A RSPreviewWidget
 * @param filter A filter to listen for
 * @param fast_filter A fast, never updating filter to listen for
 */
extern void rs_preview_widget_set_filter(RSPreviewWidget *preview, RSFilter *filter, RSFilter *fast_filter);

/**
 * Sets the CMS profile used in preview
 * @param preview A RSPreviewWidget
 * @param profile The profile to use
 */
extern void rs_preview_widget_set_profile(RSPreviewWidget *preview, RSIccProfile *profile);

/**
 * Sets the background color of a RSPreviewWidget
 * @param preview A RSPreviewWidget
 * @param color The new background color
 */
extern void rs_preview_widget_set_bgcolor(RSPreviewWidget *preview, GdkColor *color);

/**
 * Enables or disables split-view
 * @param preview A RSPreviewWidget
 * @param split_screen Enables split-view if TRUE, disables if FALSE
 */
extern void rs_preview_widget_set_split(RSPreviewWidget *preview, gboolean split_screen);

/**
 * Enables or disables lights out mode
 * @param preview A RSPreviewWidget
 * @param lightsout Enables lights out mode if TRUE, disables if FALSE
 */
void
rs_preview_widget_set_lightsout(RSPreviewWidget *preview, gboolean lightsout);

/**
 * Sets the active snapshot of a RSPreviewWidget
 * @param preview A RSPreviewWidget
 * @param view Which view to set (0..1)
 * @param snapshot Which snapshot to view (0..2)
 */
extern void rs_preview_widget_set_snapshot(RSPreviewWidget *preview, const guint view, const gint snapshot);

/**
 * Enables or disables the exposure mask
 * @param preview A RSPreviewWidget
 * @param show_exposure_mask Set to TRUE to enabled
 */
extern void rs_preview_widget_set_show_exposure_mask(RSPreviewWidget *preview, gboolean show_exposure_mask);

/**
 * Gets the status of whether the exposure mask is displayed
 * @param preview A RSPreviewWidget
 * @return TRUE is exposure mask is displayed, FALSE otherwise
 */
extern gboolean rs_preview_widget_get_show_exposure_mask(RSPreviewWidget *preview, gboolean show_exposure_mask);

/**
 * Tells the preview widget to update itself
 * @param preview A RSPreviewWidget
 * @param full_redraw Set to TRUE to redraw everything, FALSE to only redraw the image.
 */
extern void rs_preview_widget_update(RSPreviewWidget *preview, gboolean full_redraw);

/**
 * Puts a RSPreviewWIdget in crop-mode
 * @param preview A RSPreviewWidget
 */
extern void
rs_preview_widget_crop_start(RSPreviewWidget *preview);

/*
 * Removes crop from the loaded photo
 * @param preview A RSpreviewWidget
 */
extern void
rs_preview_widget_uncrop(RSPreviewWidget *preview);

/*
 * Puts a RSPreviewWidget in straighten-mode
 * @param preview A RSPreviewWidget
 */
extern void
rs_preview_widget_straighten(RSPreviewWidget *preview);

/*
 * Removes straighten from the loaded photo
 * @param preview A RSPreviewWidget
 */
extern void
rs_preview_widget_unstraighten(RSPreviewWidget *preview);

/*
 * Enables quick mode in display
 * @param preview A RSPreviewWidget
 */
extern void
rs_preview_widget_quick_start(RSPreviewWidget *preview, gboolean keep_quick);

/*
 * Disables quick mode in display and redraws screen
 * @param preview A RSPreviewWidget
 */
extern void
rs_preview_widget_quick_end(RSPreviewWidget *preview);

extern void 
rs_preview_widget_update_display_colorspace(RSPreviewWidget *preview, gboolean force);

#define RS_PREVIEW_TYPE_WIDGET             (rs_preview_widget_get_type ())
#define RS_PREVIEW_WIDGET(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), RS_PREVIEW_TYPE_WIDGET, RSPreviewWidget))
#define RS_PREVIEW_WIDGET_CLASS(obj)       (G_TYPE_CHECK_CLASS_CAST ((obj), RS_PREVIEW_WIDGET, RSPreviewWidgetClass))
#define RS_IS_PREVIEW_WIDGET(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), RS_PREVIEW_TYPE_WIDGET))
#define RS_IS_PREVIEW_WIDGET_CLASS(obj)    (G_TYPE_CHECK_CLASS_TYPE ((obj), RS_PREVIEW_TYPE_WIDGET))
#define RS_PREVIEW_WIDGET_GET_CLASS        (G_TYPE_INSTANCE_GET_CLASS ((obj), RS_PREVIEW_TYPE_WIDGET, RSPreviewWidgetClass))

#endif /* RS_PREVIEW_WIDGET_H */
