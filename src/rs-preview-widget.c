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

#include "rs-preview-widget.h"
#include "rs-math.h"
#include "rs-image.h"
#include "color.h"
#include "rawstudio.h"
#include "gtk-interface.h"
#include "gtk-helper.h"
#include "rs-color-transform.h"
#include "config.h"
#include "conf_interface.h"
#include "toolbox.h"
#include <gettext.h>

typedef enum {
	NORMAL           = 0x000F, /* 0000 0000 0000 1111 */
	WB_PICKER        = 0x0001, /* 0000 0000 0000 0001 */

	STRAIGHTEN       = 0x0030, /* 0000 0000 0011 0000 */
	STRAIGHTEN_START = 0x0020, /* 0000 0000 0010 0000 */
	STRAIGHTEN_MOVE  = 0x0010, /* 0000 0000 0001 0000 */

	CROP             = 0x3FC0, /* 0011 1111 1100 0000 */
	CROP_START       = 0x2000, /* 0010 0000 0000 0000 */
	CROP_IDLE        = 0x1000, /* 0001 0000 0000 0000 */
	CROP_MOVE_ALL    = 0x0080, /* 0000 0000 1000 0000 */
	CROP_MOVE_CORNER = 0x0040, /* 0000 0000 0100 0000 */
	DRAW_ROI         = 0x10C0, /* 0001 0000 1100 0000 */

	MOVE             = 0x4000, /* 0100 0000 0000 0000 */
} STATE;

typedef enum {
	CROP_NEAR_INSIDE  = 0x10, /* 0001 0000 */ 
	CROP_NEAR_OUTSIDE = 0x20, /* 0010 0000 */
	CROP_NEAR_N       = 0x8,  /* 0000 1000 */
	CROP_NEAR_S       = 0x4,  /* 0000 0100 */
	CROP_NEAR_W       = 0x2,  /* 0000 0010 */
	CROP_NEAR_E       = 0x1,  /* 0000 0001 */
	CROP_NEAR_NW      = CROP_NEAR_N | CROP_NEAR_W,
	CROP_NEAR_NE      = CROP_NEAR_N | CROP_NEAR_E,
	CROP_NEAR_SE      = CROP_NEAR_S | CROP_NEAR_E,
	CROP_NEAR_SW      = CROP_NEAR_S | CROP_NEAR_W,
	CROP_NEAR_CORNER  = CROP_NEAR_N | CROP_NEAR_S | CROP_NEAR_W | CROP_NEAR_E,
	CROP_NEAR_NOTHING = CROP_NEAR_INSIDE | CROP_NEAR_OUTSIDE,
} CROP_NEAR;

typedef struct {
	gint x;
	gint y;
} RS_COORD;

static GdkCursor *cur_fleur = NULL;
static GdkCursor *cur_watch = NULL;
static GdkCursor *cur_normal = NULL;
static GdkCursor *cur_nw = NULL;
static GdkCursor *cur_ne = NULL;
static GdkCursor *cur_se = NULL;
static GdkCursor *cur_sw = NULL;
static GdkCursor *cur_pencil = NULL;

struct _RSPreviewWidget
{
	GtkVBox parent;

	RS_PHOTO *photo;
	STATE state; /* Current state */
	RS_COORD last; /* Used by motion() to detect pointer movement */
	GtkWidget *tool;

	/* Zoom */
	GtkAdjustment *zoom;
	gboolean zoom_to_fit;
	gulong zoom_signal; /* The signal, we need this to block it */
	gint dirty; /* Dirty flag, used for multiple things */
	gint width; /* Width for zoom-to-fit */
	gint height; /* Height for zoom-to-fit */
	guint zoom_timeout_helper;

	/* Drawing for BOTH windows */
	RS_IMAGE8 *buffer[2]; /* Off-screen buffer */
	GtkWidget *scrolledwindow; /* Our scroller */
	GtkWidget *viewport[2]; /* We have to embed out drawingareas in viewports to scroll */
	GtkWidget *drawingarea[2]; /* This is were we draw */
	GdkRectangle visible[2]; /* The visible part of the image */
	gint snapshot[2]; /* Which snapshot to display */
	GdkColor bgcolor; /* Background color of widget */
	GdkPixmap *blitter;
	GtkWidget *pane; /* Pane used for split-view */
	GtkToggleButton *split; /* Split-screen */
	gboolean split_continuous;
	GtkToggleButton *exposure_mask; /* Exposure mask */
	GtkAdjustment *hadjustment[2];
	GtkAdjustment *vadjustment[2];

	/* Buffer for LEFT window */
	RS_IMAGE8 *buffer_shaded;
	
	/* Scaling */
	RS_IMAGE16 *scaled; /* The image scaled to suit */
	RS_MATRIX3 affine; /* From real to screen */
	RS_MATRIX3 inverse_affine; /* from screen to real */
	guint orientation;

	/* Crop */
	RS_COORD opposite;
	RS_RECT roi;
	RS_RECT roi_scaled;
	guint roi_grid;
	CROP_NEAR near;
	gfloat crop_aspect;
	GString *crop_text;
	PangoLayout *crop_text_layout;
	GtkWidget *crop_size_label;

	/* Background thread */
	GThread *bg_thread; /* Thread for background renderer */
	gboolean bg_abort; /* Abort signal for background thread */

	/* Color */
	RS_COLOR_TRANSFORM *rct[2];

	/* Straighten */
	RS_COORD straighten_start;
	RS_COORD straighten_end;
	gfloat straighten_angle;
};

/* Define the boiler plate stuff using the predefined macro */
G_DEFINE_TYPE (RSPreviewWidget, rs_preview_widget, GTK_TYPE_VBOX);

#define SCALE	(1<<0)
#define SCREEN	(1<<1)
#define BUFFER	(1<<2)
#define SHADED	(1<<3)
#define ALL		(0xffff)
#define DIRTY(a, b) do { (a) |= (b); } while (0)
#define UNDIRTY(a, b) do { (a) &= ~(b); } while (0)
#define ISDIRTY(a, b) (!!((a)&(b)))

enum {
	WB_PICKED,
	MOTION_SIGNAL,
	ROI_CHANGED, /* FIXME: todo */
	ROI_ENDED, /* FIXME: todo */
	LINE_CHANGED, /* FIXME: todo */
	LINE_ENDED, /* FIXME: todo */
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

static void rs_preview_widget_redraw(RSPreviewWidget *preview, GdkRectangle *rect);
static void crop_aspect_changed(gpointer active, gpointer user_data);
static void crop_grid_changed(gpointer active, gpointer user_data);
static void crop_apply_clicked(GtkButton *button, gpointer user_data);
static void crop_cancel_clicked(GtkButton *button, gpointer user_data);
static void crop_end(RSPreviewWidget *preview, gboolean accept);
static void adjustment_change(GtkAdjustment *do_not_use_this, RSPreviewWidget *preview);
static gboolean drawingarea_expose(GtkWidget *widget, GdkEventExpose *event, RSPreviewWidget *preview);
static gboolean scroller_size_allocate_helper(RSPreviewWidget *preview);
static gboolean scroller_size_allocate(GtkWidget *widget, GtkAllocation *allocation, RSPreviewWidget *preview);
static void zoom_in_clicked (GtkButton *button, RSPreviewWidget *preview);
static void zoom_out_clicked (GtkButton *button, RSPreviewWidget *preview);
static void zoom_fit_clicked (GtkButton *button, RSPreviewWidget *preview);
static void zoom_100_clicked (GtkButton *button, RSPreviewWidget *preview);
static void zoom_changed (GtkAdjustment *adjustment, RSPreviewWidget *preview);
static gchar *scale_format(GtkScale *scale, gdouble value, RSPreviewWidget *preview);
static void split_toggled(GtkToggleButton *togglebutton, RSPreviewWidget *preview);
static void exposure_mask_toggled(GtkToggleButton *togglebutton, RSPreviewWidget *preview);
static gboolean button(GtkWidget *widget, GdkEventButton *event, RSPreviewWidget *preview);
static gboolean button_right(GtkWidget *widget, GdkEventButton *event, RSPreviewWidget *preview);
static gboolean motion(GtkWidget *widget, GdkEventMotion *event, RSPreviewWidget *preview);
static void render_scale(RSPreviewWidget *preview);

/**
 * Class initializer
 */
static void
rs_preview_widget_class_init(RSPreviewWidgetClass *klass)
{
	GtkWidgetClass *widget_class;
	GtkObjectClass *object_class;
	widget_class = GTK_WIDGET_CLASS(klass);
	object_class = GTK_OBJECT_CLASS(klass);

	signals[WB_PICKED] = g_signal_new ("wb-picked",
		G_TYPE_FROM_CLASS (klass),
		G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
		0, /* Is this right? */
		NULL, 
		NULL,                
		g_cclosure_marshal_VOID__POINTER,
		G_TYPE_NONE, 1, G_TYPE_POINTER);
	signals[MOTION_SIGNAL] = g_signal_new ("motion",
		G_TYPE_FROM_CLASS (klass),
		G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
		0, /* Is this right? */
		NULL,
		NULL,
		g_cclosure_marshal_VOID__POINTER,
		G_TYPE_NONE, 1, G_TYPE_POINTER);
}

/**
 * Instance initialization
 */
static void
rs_preview_widget_init(RSPreviewWidget *preview)
{
	GtkWidget *hbox;
	GtkWidget *zoom, *zoom_in, *zoom_out, *zoom_fit, *zoom_100;
	GtkWidget *align[2];

	/* Initialize cursors */
	if (!cur_fleur) cur_fleur = gdk_cursor_new(GDK_FLEUR);
	if (!cur_watch) cur_watch = gdk_cursor_new(GDK_WATCH);
	if (!cur_nw) cur_nw = gdk_cursor_new(GDK_TOP_LEFT_CORNER);
	if (!cur_ne) cur_ne = gdk_cursor_new(GDK_TOP_RIGHT_CORNER);
	if (!cur_se) cur_se = gdk_cursor_new(GDK_BOTTOM_RIGHT_CORNER);
	if (!cur_sw) cur_sw = gdk_cursor_new(GDK_BOTTOM_LEFT_CORNER);
	if (!cur_pencil) cur_pencil = gdk_cursor_new(GDK_PENCIL);
	
	/* We need some adjustments (all values are bogus!) */
	preview->hadjustment[0] = GTK_ADJUSTMENT(gtk_adjustment_new(0.0, 0.0, 100.0, 1.0, 10.0, 10.0));
	preview->hadjustment[1] = GTK_ADJUSTMENT(gtk_adjustment_new(0.0, 0.0, 100.0, 1.0, 10.0, 10.0));
	preview->vadjustment[0] = GTK_ADJUSTMENT(gtk_adjustment_new(0.0, 0.0, 100.0, 1.0, 10.0, 10.0));
	preview->vadjustment[1] = GTK_ADJUSTMENT(gtk_adjustment_new(0.0, 0.0, 100.0, 1.0, 10.0, 10.0));

	g_signal_connect(G_OBJECT(preview->hadjustment[0]), "value-changed", G_CALLBACK(adjustment_change), preview);
	g_signal_connect(G_OBJECT(preview->vadjustment[0]), "value-changed", G_CALLBACK(adjustment_change), preview);
	g_signal_connect(G_OBJECT(preview->hadjustment[0]), "changed", G_CALLBACK(adjustment_change), preview);
	g_signal_connect(G_OBJECT(preview->vadjustment[0]), "changed", G_CALLBACK(adjustment_change), preview);

	/* Let's have scrollbars! */
	preview->scrolledwindow = gtk_scrolled_window_new (
		preview->hadjustment[0],
		preview->vadjustment[0]);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (preview->scrolledwindow),
		GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	g_signal_connect(G_OBJECT(preview->scrolledwindow), "size-allocate", G_CALLBACK(scroller_size_allocate), preview);

	/* Make the two views "stick" together */
	preview->viewport[0] = gtk_viewport_new (
		preview->hadjustment[0],
		preview->vadjustment[0]);
	preview->viewport[1] = gtk_viewport_new (
		preview->hadjustment[1],
		preview->vadjustment[1]);

	/* We need a place to draw */
	preview->drawingarea[0] = gtk_drawing_area_new();
	preview->drawingarea[1] = gtk_drawing_area_new();
	GTK_WIDGET_UNSET_FLAGS (preview->drawingarea[0], GTK_DOUBLE_BUFFERED);
	GTK_WIDGET_UNSET_FLAGS (preview->drawingarea[1], GTK_DOUBLE_BUFFERED);
	g_signal_connect(G_OBJECT(preview->drawingarea[0]), "expose-event", G_CALLBACK(drawingarea_expose), preview);
	g_signal_connect(G_OBJECT(preview->drawingarea[1]), "expose-event", G_CALLBACK(drawingarea_expose), preview);

	g_signal_connect_after (G_OBJECT (preview->drawingarea[0]), "button_press_event",
		G_CALLBACK (button), preview);
	g_signal_connect_after (G_OBJECT (preview->drawingarea[0]), "button_release_event",
		G_CALLBACK (button), preview);
	g_signal_connect (G_OBJECT (preview->drawingarea[0]), "motion_notify_event",
		G_CALLBACK (motion), preview);
	g_signal_connect_after (G_OBJECT (preview->drawingarea[1]), "button_press_event",
		G_CALLBACK (button_right), preview);

	/* Let's align the image to the center if smaller than available space */
	align[0] = gtk_alignment_new(0.5f, 0.5f, 0.0f, 0.0f);
	align[1] = gtk_alignment_new(0.5f, 0.5f, 0.0f, 0.0f);

	gtk_container_add (GTK_CONTAINER (align[0]), preview->drawingarea[0]);
	gtk_container_add (GTK_CONTAINER (align[1]), preview->drawingarea[1]);
	gtk_container_add (GTK_CONTAINER (preview->viewport[0]), align[0]);
	gtk_container_add (GTK_CONTAINER (preview->viewport[1]), align[1]);

	/* The pane for split view */
	preview->pane = gtk_hpaned_new();
	/* Don't add the viewport to the right pane YET, look at rs_preview_widget_set_split() */
	gtk_paned_pack1(GTK_PANED(preview->pane), preview->viewport[0], TRUE, TRUE);

	gtk_container_add (GTK_CONTAINER (preview->scrolledwindow), preview->pane);

	/* Let us know about pointer movements */
	gtk_widget_set_events(GTK_WIDGET(preview->drawingarea[0]), 0
		| GDK_BUTTON_PRESS_MASK
		| GDK_BUTTON_RELEASE_MASK
		| GDK_POINTER_MOTION_MASK);
	gtk_widget_set_events(GTK_WIDGET(preview->drawingarea[1]), 0
		| GDK_BUTTON_PRESS_MASK
		| GDK_BUTTON_RELEASE_MASK
		| GDK_POINTER_MOTION_MASK);

	hbox = gtk_hbox_new(FALSE, 0);

	/* Split-toggle */
	preview->split = GTK_TOGGLE_BUTTON(gtk_check_button_new_with_label(_("Split")));
	g_signal_connect(G_OBJECT(preview->split), "toggled", G_CALLBACK(split_toggled), preview);
	rs_conf_get_boolean_with_default(CONF_SPLIT_CONTINUOUS, &preview->split_continuous, TRUE);

	/* Exposure-mask-toggle */
	preview->exposure_mask = GTK_TOGGLE_BUTTON(gtk_check_button_new_with_label(_("Exp. mask")));
	gui_tooltip_window(GTK_WIDGET(preview->exposure_mask), _("Toggle exposure mask"), NULL);
	g_signal_connect(G_OBJECT(preview->exposure_mask), "toggled", G_CALLBACK(exposure_mask_toggled), preview);

	/* zoom adjustment */
	preview->zoom = GTK_ADJUSTMENT(gtk_adjustment_new(0.5f, 0.05f, 1.0f, 0.05f, 0.1f, 0.0f));
	preview->zoom_signal = g_signal_connect(G_OBJECT(preview->zoom), "value-changed", G_CALLBACK(zoom_changed), preview);

	/* zoom scale */
	zoom = gtk_hscale_new(preview->zoom);
	gtk_scale_set_value_pos(GTK_SCALE(zoom), GTK_POS_RIGHT);
	gtk_scale_set_digits(GTK_SCALE(zoom), 2);
	g_signal_connect (G_OBJECT(zoom), "format-value", G_CALLBACK(scale_format), preview);
	gui_tooltip_window(zoom, _("Set zoom"), NULL);

	/* zoom buttons */
	zoom_out = gtk_button_new();
	gtk_button_set_image(GTK_BUTTON(zoom_out), gtk_image_new_from_stock(GTK_STOCK_ZOOM_OUT, GTK_ICON_SIZE_MENU));
	gui_tooltip_window(zoom_out, _("Zoom out"), NULL);

	zoom_in = gtk_button_new();
	gtk_button_set_image(GTK_BUTTON(zoom_in), gtk_image_new_from_stock(GTK_STOCK_ZOOM_IN, GTK_ICON_SIZE_MENU));
	gui_tooltip_window(zoom_in, _("Zoom in"), NULL);

	zoom_fit = gtk_button_new();
	gtk_button_set_image(GTK_BUTTON(zoom_fit), gtk_image_new_from_stock(GTK_STOCK_ZOOM_FIT, GTK_ICON_SIZE_MENU));
	gui_tooltip_window(zoom_fit, _("Zoom to fit"), NULL);

	zoom_100 = gtk_button_new();
	gtk_button_set_image(GTK_BUTTON(zoom_100), gtk_image_new_from_stock(GTK_STOCK_ZOOM_100, GTK_ICON_SIZE_MENU));
	gui_tooltip_window(zoom_100, _("Zoom to 100%"), NULL);

	g_signal_connect(G_OBJECT(zoom_out), "clicked", G_CALLBACK(zoom_out_clicked), preview);
	g_signal_connect(G_OBJECT(zoom_in), "clicked", G_CALLBACK(zoom_in_clicked), preview);
	g_signal_connect(G_OBJECT(zoom_fit), "clicked", G_CALLBACK(zoom_fit_clicked), preview);
	g_signal_connect(G_OBJECT(zoom_100), "clicked", G_CALLBACK(zoom_100_clicked), preview);

	gtk_box_pack_start (GTK_BOX (hbox), GTK_WIDGET(preview->exposure_mask), FALSE, FALSE, 0);
#ifdef EXPERIMENTAL
	gtk_box_pack_start (GTK_BOX (hbox), GTK_WIDGET(preview->split), FALSE, FALSE, 0);
#endif
	gtk_box_pack_start (GTK_BOX (hbox), gtk_vseparator_new(), FALSE, FALSE, 1);
	gtk_box_pack_start (GTK_BOX (hbox), gtk_label_new(_("Zoom:")), FALSE, FALSE, 1);
	gtk_box_pack_start (GTK_BOX (hbox), zoom, TRUE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (hbox), zoom_out, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (hbox), zoom_in, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (hbox), zoom_fit, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (hbox), zoom_100, FALSE, FALSE, 0);

	gtk_box_pack_start (GTK_BOX (preview), hbox, FALSE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (preview), preview->scrolledwindow, TRUE, TRUE, 0);

	/* Try to set some sane defaults */
	preview->photo = NULL;
	preview->scaled = NULL;
	preview->buffer[0] = NULL;
	preview->buffer[1] = NULL;
	preview->buffer_shaded = NULL;
	preview->snapshot[0] = 0;
	preview->snapshot[1] = 1;
	preview->state = WB_PICKER;
	preview->blitter = NULL;
	matrix3_identity(&preview->affine);
	preview->roi.x1 = 100;
	preview->roi.y1 = 100;
	preview->roi.x2 = 300;
	preview->roi.y2 = 300;
	preview->crop_text = g_string_new("");
	preview->crop_text_layout = gtk_widget_create_pango_layout(preview->drawingarea[0], "");
	preview->bg_thread = NULL;
	preview->bg_abort = FALSE;
	preview->straighten_angle = 0.0f;
	preview->last.x = -100;
	preview->last.y = -100;
	ORIENTATION_RESET(preview->orientation);
	preview->zoom_timeout_helper = 0;

	preview->rct[0] = rs_color_transform_new();
	rs_color_transform_set_output_format(preview->rct[0], 8);
	preview->rct[1] = rs_color_transform_new();
	rs_color_transform_set_output_format(preview->rct[1], 8);

	rs_preview_widget_set_zoom_to_fit(preview);

	/* Default black background color */
	COLOR_BLACK(preview->bgcolor);
	gtk_widget_modify_bg(preview->viewport[0], GTK_STATE_NORMAL, &preview->bgcolor);
	gtk_widget_modify_bg(preview->viewport[1], GTK_STATE_NORMAL, &preview->bgcolor);
	gtk_widget_modify_bg(preview->drawingarea[0], GTK_STATE_NORMAL, &preview->bgcolor);
	gtk_widget_modify_bg(preview->drawingarea[1], GTK_STATE_NORMAL, &preview->bgcolor);
}

/**
 * Creates a new RSPreviewWidget
 * @return A new RSPreviewWidget
 */
GtkWidget *
rs_preview_widget_new(void)
{
	return g_object_new (RS_PREVIEW_TYPE_WIDGET, NULL);
}

/**
 * Sets the zoom level of a RSPreviewWidget
 * @param preview A RSPreviewWidget
 * @param zoom New zoom level (0.0 - 2.0)
 */
void
rs_preview_widget_set_zoom(RSPreviewWidget *preview, gdouble zoom)
{
	g_return_if_fail (RS_IS_PREVIEW_WIDGET(preview));

	gtk_adjustment_set_value(preview->zoom, zoom);
}

/**
 * gets the zoom level of a RSPreviewWidget
 * @param preview A RSPreviewWidget
 * @return Current zoom level
 */
gdouble
rs_preview_widget_get_zoom(RSPreviewWidget *preview)
{
	g_return_val_if_fail (RS_IS_PREVIEW_WIDGET(preview), 1.0f);

	return gtk_adjustment_get_value(preview->zoom);
}

/**
 * Select zoom-to-fit of a RSPreviewWidget
 * @param preview A RSPreviewWidget
 */
void
rs_preview_widget_set_zoom_to_fit(RSPreviewWidget *preview)
{
	g_return_if_fail (RS_IS_PREVIEW_WIDGET(preview));

	if (!preview->zoom_to_fit)
	{
		preview->zoom_to_fit = TRUE;
		DIRTY(preview->dirty, SCALE);
		rs_preview_widget_redraw(preview, NULL);
	}
}

/**
 * Increases the zoom of a RSPreviewWidget by 0.1
 * @param preview A RSPreviewWidget
 */
void
rs_preview_widget_zoom_in(RSPreviewWidget *preview)
{
	gdouble zoom;

	g_return_if_fail (RS_IS_PREVIEW_WIDGET(preview));

	zoom = rs_preview_widget_get_zoom(preview);
	rs_preview_widget_set_zoom(preview, zoom+0.1f);
}

/**
 * Decreases the zoom of a RSPreviewWidget by 0.1
 * @param preview A RSPreviewWidget
 */
void
rs_preview_widget_zoom_out(RSPreviewWidget *preview)
{
	gdouble zoom;

	g_return_if_fail (RS_IS_PREVIEW_WIDGET(preview));

	zoom = rs_preview_widget_get_zoom(preview);
	rs_preview_widget_set_zoom(preview, zoom-0.1f);
}

static void
input_changed(RS_IMAGE16 *image, RSPreviewWidget *preview)
{
	gdk_threads_enter();
	/* Still relevant? */
	if (image == preview->photo->input)
	{
		DIRTY(preview->dirty, SCALE);
		rs_preview_widget_update(preview);
	}
	gdk_threads_leave();
}

static GThreadPool *pool = NULL;

static void
demosaic_worker(gpointer data, gpointer user_data)
{
	RS_IMAGE16 *image = (RS_IMAGE16 *) data;
	RSPreviewWidget *preview = RS_PREVIEW_WIDGET(user_data);

	g_usleep(100000); /* Wait a second before starting! */

	/* Check if this is still relevant */
	if ((preview->photo && (image == preview->photo->input)) && (image->filters != 0) && (image->fourColorFilters != 0))
		rs_image16_demosaic(image, RS_DEMOSAIC_PPG);
	rs_image16_unref(image);
}

/**
 * Sets active photo of a RSPreviewWidget
 * @param preview A RSPreviewWidget
 * @param photo A RS_PHOTO
 */
void
rs_preview_widget_set_photo(RSPreviewWidget *preview, RS_PHOTO *photo)
{
	if (!pool)
		pool = g_thread_pool_new(demosaic_worker, preview, 1, TRUE, NULL);

	g_return_if_fail (RS_IS_PREVIEW_WIDGET(preview));

	preview->photo = photo;
	if (preview->photo && preview->photo->input->filters && preview->photo->input->fourColorFilters)
	{
		photo->input->preview = TRUE;
		rs_image16_ref(photo->input); /* The thread will unref */
		g_thread_pool_push(pool, photo->input, NULL);

		/* Start demosaic */

		g_signal_connect(G_OBJECT(photo->input), "pixeldata-changed", G_CALLBACK(input_changed), preview);
	}
	DIRTY(preview->dirty, SCALE);

	rs_preview_widget_update(preview);
}

/**
 * Sets the background color of a RSPreviewWidget
 * @param preview A RSPreviewWidget
 * @param color The new background color
 */
void
rs_preview_widget_set_bgcolor(RSPreviewWidget *preview, GdkColor *color)
{
	g_return_if_fail (RS_IS_PREVIEW_WIDGET(preview));
	g_return_if_fail (color != NULL);

	preview->bgcolor = *color;

	gtk_widget_modify_bg(preview->drawingarea[0], GTK_STATE_NORMAL, &preview->bgcolor);
	gtk_widget_modify_bg(preview->drawingarea[0]->parent->parent, GTK_STATE_NORMAL, &preview->bgcolor);
	gtk_widget_modify_bg(preview->drawingarea[1], GTK_STATE_NORMAL, &preview->bgcolor);
	gtk_widget_modify_bg(preview->drawingarea[1]->parent->parent, GTK_STATE_NORMAL, &preview->bgcolor);

	return;
}

/**
 * Sets the CMS transform function used
 * @param preview A RSPreviewWidget
 * @param transform The transform to use
 */
void
rs_preview_widget_set_cms(RSPreviewWidget *preview, void *transform)
{
	g_return_if_fail (RS_IS_PREVIEW_WIDGET(preview));
	rs_color_transform_set_cms_transform(preview->rct[0], transform);
	rs_color_transform_set_cms_transform(preview->rct[1], transform);
	return;
}

/**
 * Enables or disables split-view
 * @param preview A RSPreviewWidget
 * @param split_screen Enables split-view if TRUE, disables if FALSE
 */
void
rs_preview_widget_set_split(RSPreviewWidget *preview, gboolean split_screen)
{
	g_return_if_fail (RS_IS_PREVIEW_WIDGET(preview));

	gtk_toggle_button_set_active(preview->split, split_screen);
	return;
}

/**
 * Sets the active snapshot of a RSPreviewWidget
 * @param preview A RSPreviewWidget
 * @param view Which view to set (0..1)
 * @param snapshot Which snapshot to view (0..2)
 */
void rs_preview_widget_set_snapshot(RSPreviewWidget *preview, const guint view, const gint snapshot)
{
	g_return_if_fail (RS_IS_PREVIEW_WIDGET(preview));
	g_return_if_fail (view<2);
	g_return_if_fail ((snapshot>=0) && (snapshot<=2));

	preview->snapshot[view] = snapshot;
	preview->snapshot[view] = snapshot;

	rs_preview_widget_update(preview);
}

/**
 * Enables or disables the exposure mask
 * @param preview A RSPreviewWidget
 * @param show_exposure_mask Set to TRUE to enabled
 */
void
rs_preview_widget_set_show_exposure_mask(RSPreviewWidget *preview, gboolean show_exposure_mask)
{
	g_return_if_fail (RS_IS_PREVIEW_WIDGET(preview));

	gtk_toggle_button_set_active(preview->exposure_mask, show_exposure_mask);
	return;
}

/**
 * Gets the status of whether the exposure mask is displayed
 * @param preview A RSPreviewWidget
 * @return TRUE is exposure mask is displayed, FALSE otherwise
 */
gboolean
rs_preview_widget_get_show_exposure_mask(RSPreviewWidget *preview, gboolean show_exposure_mask)
{
	g_return_val_if_fail(RS_IS_PREVIEW_WIDGET(preview), FALSE);

	return preview->exposure_mask->active;
}

/**
 * Tells the preview widget to update itself
 * @param preview A RSPreviewWidget
 */
void
rs_preview_widget_update(RSPreviewWidget *preview)
{
	g_return_if_fail(preview);
	g_return_if_fail(preview->rct);

	if (!preview->photo)
		return;

	/* Prepare renderer */
	rs_color_transform_set_from_settings(preview->rct[0], preview->photo->settings[preview->snapshot[0]], MASK_ALL);

	/* Prepare both if we're in split-mode */
	if (preview->split->active)
		rs_color_transform_set_from_settings(preview->rct[1], preview->photo->settings[preview->snapshot[1]], MASK_ALL);

	/* Catch rotation */
	if (preview->orientation != preview->photo->orientation)
	{
		preview->orientation = preview->photo->orientation;
		DIRTY(preview->dirty, SCALE);
	}

	DIRTY(preview->dirty, SCREEN);
	DIRTY(preview->dirty, BUFFER);

	rs_preview_widget_redraw(preview, preview->visible);
}

/**
 * Puts a RSPreviewWIdget in crop-mode
 * @param preview A RSPreviewWidget
 */
void
rs_preview_widget_crop_start(RSPreviewWidget *preview)
{
	GtkWidget *vbox;
	GtkWidget *roi_size_hbox;
	GtkWidget *label;
	GtkWidget *roi_grid_hbox;
	GtkWidget *roi_grid_label;
	GtkWidget *roi_grid_combobox;
	GtkWidget *aspect_hbox;
	GtkWidget *aspect_label;
	GtkWidget *button_box;
	GtkWidget *apply_button;
	GtkWidget *cancel_button;
	RS_CONFBOX *grid_confbox;
	RS_CONFBOX *aspect_confbox;

	/* predefined aspects */
	/* aspect MUST be => 1.0 */
	const static gdouble aspect_freeform = 0.0f;
	const static gdouble aspect_32 = 3.0f/2.0f;
	const static gdouble aspect_43 = 4.0f/3.0f;
	const static gdouble aspect_1008 = 10.0f/8.0f;
	const static gdouble aspect_1610 = 16.0f/10.0f;
	const static gdouble aspect_83 = 8.0f/3.0f;
	const static gdouble aspect_11 = 1.0f;
	static gdouble aspect_iso216;
	static gdouble aspect_golden;
	aspect_iso216 = sqrt(2.0f);
	aspect_golden = (1.0f+sqrt(5.0f))/2.0f;

	vbox = gtk_vbox_new(FALSE, 4);

	label = gtk_label_new(_("Size"));
	gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);

	roi_size_hbox = gtk_hbox_new(FALSE, 0);

	/* Default aspect (freeform) */
	preview->crop_aspect = 0.0f;

	preview->crop_size_label = gtk_label_new(_("-"));
	gtk_box_pack_start (GTK_BOX (roi_size_hbox), label, TRUE, TRUE, 4);
	gtk_box_pack_start (GTK_BOX (roi_size_hbox), preview->crop_size_label, FALSE, TRUE, 4);
	gtk_box_pack_start (GTK_BOX (vbox), roi_size_hbox, FALSE, TRUE, 0);

	roi_grid_hbox = gtk_hbox_new(FALSE, 0);
	roi_grid_label = gtk_label_new(_("Grid"));
	gtk_misc_set_alignment(GTK_MISC(roi_grid_label), 0.0, 0.5);

	grid_confbox = gui_confbox_new(CONF_ROI_GRID);
	gui_confbox_set_callback(grid_confbox, preview, crop_grid_changed);
	gui_confbox_add_entry(grid_confbox, "none", _("None"), (gpointer) ROI_GRID_NONE);
	gui_confbox_add_entry(grid_confbox, "goldensections", _("Golden sections"), (gpointer) ROI_GRID_GOLDEN);
	gui_confbox_add_entry(grid_confbox, "ruleofthirds", _("Rule of thirds"), (gpointer) ROI_GRID_THIRDS);
	gui_confbox_add_entry(grid_confbox, "goldentriangles1", _("Golden triangles #1"), (gpointer) ROI_GRID_GOLDEN_TRIANGLES1);
	gui_confbox_add_entry(grid_confbox, "goldentriangles2", _("Golden triangles #2"), (gpointer) ROI_GRID_GOLDEN_TRIANGLES2);
	gui_confbox_add_entry(grid_confbox, "harmonioustriangles1", _("Harmonious triangles #1"), (gpointer) ROI_GRID_HARMONIOUS_TRIANGLES1);
	gui_confbox_add_entry(grid_confbox, "harmonioustriangles2", _("Harmonious triangles #2"), (gpointer) ROI_GRID_HARMONIOUS_TRIANGLES2);
	gui_confbox_load_conf(grid_confbox, "none");

	roi_grid_combobox = gui_confbox_get_widget(grid_confbox);

	gtk_box_pack_start (GTK_BOX (roi_grid_hbox), roi_grid_label, TRUE, TRUE, 4);
	gtk_box_pack_start (GTK_BOX (roi_grid_hbox), roi_grid_combobox, FALSE, TRUE, 4);

	aspect_hbox = gtk_hbox_new(FALSE, 0);
	aspect_label = gtk_label_new(_("Aspect"));
	gtk_misc_set_alignment(GTK_MISC(aspect_label), 0.0, 0.5);

	aspect_confbox = gui_confbox_new(CONF_CROP_ASPECT);
	gui_confbox_set_callback(aspect_confbox, preview, crop_aspect_changed);
	gui_confbox_add_entry(aspect_confbox, "freeform", _("Freeform"), (gpointer) &aspect_freeform);
	gui_confbox_add_entry(aspect_confbox, "iso216", _("ISO paper (A4)"), (gpointer) &aspect_iso216);
	gui_confbox_add_entry(aspect_confbox, "3:2", _("3:2 (35mm)"), (gpointer) &aspect_32);
	gui_confbox_add_entry(aspect_confbox, "4:3", _("4:3"), (gpointer) &aspect_43);
	gui_confbox_add_entry(aspect_confbox, "10:8", _("10:8 (SXGA)"), (gpointer) &aspect_1008);
	gui_confbox_add_entry(aspect_confbox, "16:10", _("16:10 (Wide XGA)"), (gpointer) &aspect_1610);
	gui_confbox_add_entry(aspect_confbox, "8:3", _("8:3 (Dualhead XGA)"), (gpointer) &aspect_83);
	gui_confbox_add_entry(aspect_confbox, "1:1", _("1:1"), (gpointer) &aspect_11);
	gui_confbox_add_entry(aspect_confbox, "goldenrectangle", _("Golden rectangle"), (gpointer) &aspect_golden);
	gui_confbox_load_conf(aspect_confbox, "freeform");

	gtk_box_pack_start (GTK_BOX (aspect_hbox), aspect_label, TRUE, TRUE, 4);
	gtk_box_pack_start (GTK_BOX (aspect_hbox),
		gui_confbox_get_widget(aspect_confbox), FALSE, TRUE, 4);

	button_box = gtk_hbox_new(FALSE, 0);
	apply_button = gtk_button_new_with_label(_("Apply"));
	g_signal_connect (G_OBJECT(apply_button), "clicked", G_CALLBACK (crop_apply_clicked), preview);
	cancel_button = gtk_button_new_with_label(_("Cancel"));
	g_signal_connect (G_OBJECT(cancel_button), "clicked", G_CALLBACK (crop_cancel_clicked), preview);
	gtk_box_pack_start (GTK_BOX (button_box), apply_button, TRUE, TRUE, 4);
	gtk_box_pack_start (GTK_BOX (button_box), cancel_button, TRUE, TRUE, 4);

	gtk_box_pack_start (GTK_BOX (vbox), roi_grid_hbox, FALSE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (vbox), aspect_hbox, FALSE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (vbox), button_box, FALSE, TRUE, 0);
	preview->tool = gui_toolbox_add_tool_frame(vbox, _("Crop"));

	preview->state = CROP_START;
}

/*
 *  * Removes crop from the loaded photo
 *   * @param preview A RSpreviewWidget
 *    */
void
rs_preview_widget_uncrop(RSPreviewWidget *preview)
{
	g_assert(RS_IS_PREVIEW_WIDGET(preview));
	if (!preview->photo) return;

	rs_photo_set_crop(preview->photo, NULL);
	DIRTY(preview->dirty, SCALE);
	rs_preview_widget_redraw(preview, preview->visible);
}

/*
 * Puts a RSPreviewWidget in straighten-mode
 * @param preview A RSPreviewWidget
 */
extern void
rs_preview_widget_straighten(RSPreviewWidget *preview)
{
	g_assert(RS_IS_PREVIEW_WIDGET(preview));

	preview->state = STRAIGHTEN_START;
}

/*
 * Removes straighten from the loaded photo
 * @param preview A RSPreviewWidget
 */
extern void
rs_preview_widget_unstraighten(RSPreviewWidget *preview)
{
	g_assert(RS_IS_PREVIEW_WIDGET(preview));

	preview->photo->angle = 0.0f;
	DIRTY(preview->dirty, SCALE);
	rs_preview_widget_redraw(preview, preview->visible);
}

/**
 * Rescales the preview widget if needed
 * @param preview A RSPreviewWidget
 */
static void
render_scale(RSPreviewWidget *preview)
{
	GdkDrawable *window;
	gdouble zoom;

	if (!ISDIRTY(preview->dirty, SCALE)) return;

	g_return_if_fail(preview->photo);
	g_return_if_fail(preview->photo->input);
	window = GDK_DRAWABLE(preview->drawingarea[0]->window);

	/* Free the scaled buffer if we got one */
	if (preview->scaled) rs_image16_free(preview->scaled);

	if (preview->zoom_to_fit)
	{
		gint width = preview->width;

		/* We only got roughly half the width if we're split */
		if (preview->split->active)
			width = width/2-10; /* Yes, 10 is a magic number - may not work for all themes */

		preview->scaled = rs_image16_transform(preview->photo->input, NULL,
			&preview->affine, &preview->inverse_affine, preview->photo->crop, width, preview->height,
			TRUE, -1.0f, preview->photo->angle, preview->photo->orientation, &zoom);

		/* Set the zoom slider to current zoom - block signal first to avoid
		circular resizing */
		g_signal_handler_block(G_OBJECT(preview->zoom), preview->zoom_signal);
		gtk_adjustment_set_value(preview->zoom, zoom);
		g_signal_handler_unblock(G_OBJECT(preview->zoom), preview->zoom_signal);
	}
	else
		preview->scaled = rs_image16_transform(preview->photo->input, NULL,
			&preview->affine, &preview->inverse_affine, preview->photo->crop, -1, -1, TRUE,
			rs_preview_widget_get_zoom(preview),
			preview->photo->angle, preview->photo->orientation, NULL);

	/* Set the size of our drawingareas and viewports to match the scaled buffer */
	gtk_widget_set_size_request(preview->drawingarea[0], preview->scaled->w, preview->scaled->h);
	gtk_widget_set_size_request(preview->drawingarea[1], preview->scaled->w, preview->scaled->h);
	gtk_widget_set_size_request(preview->viewport[0], preview->scaled->w, preview->scaled->h);
	gtk_widget_set_size_request(preview->viewport[1], preview->scaled->w, preview->scaled->h);

	/* Free the preview buffer if any */
	if (preview->buffer[0]) rs_image8_free(preview->buffer[0]);
	if (preview->buffer[1]) rs_image8_free(preview->buffer[1]);

	/* Allocate new 8 bit buffers */
	preview->buffer[0] = rs_image8_new(preview->scaled->w, preview->scaled->h, 3, 3);
	preview->buffer[1] = rs_image8_new(preview->scaled->w, preview->scaled->h, 3, 3);

	/* Resize our blit-buffer */
	if (preview->blitter)
		g_object_unref(preview->blitter);
	preview->blitter = gdk_pixmap_new(window, preview->scaled->w, preview->scaled->h, -1);

	UNDIRTY(preview->dirty, SCALE);
	DIRTY(preview->dirty, SCREEN);
	DIRTY(preview->dirty, BUFFER);
}

static void
render_buffer(RSPreviewWidget *preview, GdkRectangle *rect)
{
	GdkRectangle fullrect[2];
	/* If we got no rect, replace by a rect covering everything */
	if (!rect)
	{
		fullrect[0].x = fullrect[1].x = 0;
		fullrect[0].y = fullrect[1].y = 0;
		fullrect[0].width = fullrect[1].width = preview->scaled->w;
		fullrect[0].height = fullrect[1].height = preview->scaled->h;
		rect = fullrect;
	}
	preview->rct[0]->transform(preview->rct[0], rect[0].width, rect[0].height,
		GET_PIXEL(preview->scaled, rect[0].x, rect[0].y),
		preview->scaled->rowstride,
		GET_PIXEL(preview->buffer[0], rect[0].x, rect[0].y),
		preview->buffer[0]->rowstride);

	if (unlikely(preview->exposure_mask->active))
		rs_image8_render_exposure_mask(preview->buffer[0], -1);

	if (unlikely(preview->split->active))
	{
		preview->rct[1]->transform(preview->rct[1], rect[1].width, rect[1].height,
			GET_PIXEL(preview->scaled, rect[1].x, rect[1].y),
			preview->scaled->rowstride,
			GET_PIXEL(preview->buffer[1], rect[1].x, rect[1].y),
			preview->buffer[1]->rowstride);
		if (unlikely(preview->exposure_mask->active))
			rs_image8_render_exposure_mask(preview->buffer[1], -1);
	}
	DIRTY(preview->dirty, SHADED);
}

static void
render_screen(RSPreviewWidget *preview, GdkRectangle *rect)
{
	GdkGC *gc = gdk_gc_new(GDK_DRAWABLE(preview->drawingarea[0]->window));

	if (unlikely(preview->state & STRAIGHTEN_MOVE))
	{
		/* Draw image */
		gdk_draw_rgb_image(preview->blitter, gc,
			rect[0].x, rect[0].y, rect[0].width, rect[0].height, GDK_RGB_DITHER_NONE,
			GET_PIXEL(preview->buffer[0], rect[0].x, rect[0].y),
			preview->buffer[0]->rowstride);

		/* Draw straighten line */
		gdk_draw_line(preview->blitter, dashed,
			preview->straighten_start.x, preview->straighten_start.y,
			preview->straighten_end.x, preview->straighten_end.y);

		/* Blit to screen */
		gdk_draw_drawable(GDK_DRAWABLE(preview->drawingarea[0]->window), gc, preview->blitter,
			rect[0].x, rect[0].y,
			rect[0].x, rect[0].y,
			rect[0].width, rect[0].height);
	}
	else if (unlikely(preview->state & DRAW_ROI))
	{
		gint x1, y1, x2, y2;
		gint text_width, text_height;

		if (ISDIRTY(preview->dirty, BUFFER))
			render_buffer(preview, rect);
		if (ISDIRTY(preview->dirty, SHADED))
		{
			preview->buffer_shaded = rs_image8_render_shaded(preview->buffer[0], preview->buffer_shaded);
			UNDIRTY(preview->dirty, SHADED);
		}

		g_string_printf(preview->crop_text, "%d x %d",
			preview->roi.x2-preview->roi.x1+1,
			preview->roi.y2-preview->roi.y1+1);

		gtk_label_set_text(GTK_LABEL(preview->crop_size_label), preview->crop_text->str);

		pango_layout_set_text(preview->crop_text_layout, preview->crop_text->str, -1);

		pango_layout_get_pixel_size(preview->crop_text_layout, &text_width, &text_height);

		/* Compute scaled ROI */
		matrix3_affine_transform_point_int(&preview->affine,
			preview->roi.x1, preview->roi.y1, &x1, &y1);
		matrix3_affine_transform_point_int(&preview->affine,
			preview->roi.x2, preview->roi.y2, &x2, &y2);

		/* Draw shaded image */
		gdk_draw_rgb_image(preview->blitter, gc,
			rect[0].x, rect[0].y, rect[0].width, rect[0].height, GDK_RGB_DITHER_NONE,
			GET_PIXEL(preview->buffer_shaded, rect[0].x, rect[0].y),
			preview->buffer_shaded->rowstride);

		/* Draw normal image inside ROI */
		gdk_draw_rgb_image(preview->blitter, gc, /* ROI */
			x1, y1,
			x2-x1, y2-y1,
			GDK_RGB_DITHER_NONE,
			GET_PIXEL(preview->buffer[0], x1, y1),
			preview->buffer[0]->rowstride);

		/* Draw ROI */
		gdk_draw_rectangle(preview->blitter, dashed, FALSE,
			x1, y1,
			x2-x1-1,
			y2-y1-1);

		if ((preview->scaled->h-text_height-4) > y2)
		{
			gdk_draw_layout(preview->blitter, dashed,
				x1+(x2-x1-text_width)/2,
				y2+2,
				preview->crop_text_layout);
		}
		else
		{
			gdk_draw_layout(preview->blitter, dashed,
				x1+(x2-x1-text_width)/2,
				y2-text_height-2,
				preview->crop_text_layout);
		}
		switch(preview->roi_grid)
		{
			case ROI_GRID_NONE:
				break;
			case ROI_GRID_GOLDEN:
			{
				gdouble goldenratio = ((1+sqrt(5))/2);
				gint t, golden;

				/* vertical */
				golden = ((x2-x1)/goldenratio);

				t = (x1+golden);
				gdk_draw_line(preview->blitter, grid, t, y1, t, y2);
				t = (x2-golden);
				gdk_draw_line(preview->blitter, grid, t, y1, t, y2);

				/* horizontal */
				golden = ((y2-y1)/goldenratio);

				t = (y1+golden);
				gdk_draw_line(preview->blitter, grid, x1, t, x2, t);
				t = (y2-golden);
				gdk_draw_line(preview->blitter, grid, x1, t, x2, t);
				break;
			}

			case ROI_GRID_THIRDS:
			{
				gint t;

				/* vertical */
				t = ((x2-x1+1)/3*1+x1);
				gdk_draw_line(preview->blitter, grid, t, y1, t, y2);
				t = ((x2-x1+1)/3*2+x1);
				gdk_draw_line(preview->blitter, grid, t, y1, t, y2);

				/* horizontal */
				t = ((y2-y1+1)/3*1+y1);
				gdk_draw_line(preview->blitter, grid, x1, t, x2, t);
				t = ((y2-y1+1)/3*2+y1);
				gdk_draw_line(preview->blitter, grid, x1, t, x2, t);
				break;
			}

			case ROI_GRID_GOLDEN_TRIANGLES1:
			{
				gdouble goldenratio = ((1+sqrt(5))/2);
				gint t, golden;

				golden = ((x2-x1)/goldenratio);

				gdk_draw_line(preview->blitter, grid, x1, y1, x2, y2);

				t = (x2-golden);
				gdk_draw_line(preview->blitter, grid, x1, y2, t, y1);

				t = (x1+golden);
				gdk_draw_line(preview->blitter, grid, x2, y1, t, y2);
				break;
			}

			case ROI_GRID_GOLDEN_TRIANGLES2:
			{
				gdouble goldenratio = ((1+sqrt(5))/2);
				gint t, golden;

				golden = ((x2-x1)/goldenratio);

				gdk_draw_line(preview->blitter, grid, x2, y1, x1, y2);

				t = (x2-golden);
				gdk_draw_line(preview->blitter, grid, x1, y1, t, y2);

				t = (x1+golden);
				gdk_draw_line(preview->blitter, grid, x2, y2, t, y1);
				break;
			}

			case ROI_GRID_HARMONIOUS_TRIANGLES1:
			{
				gdouble goldenratio = ((1+sqrt(5))/2);
				gint t, golden;

				golden = ((x2-x1)/goldenratio);

				gdk_draw_line(preview->blitter, grid, x1, y1, x2, y2);

				t = (x1+golden);
				gdk_draw_line(preview->blitter, grid, x1, y2, t, y1);

				t = (x2-golden);
				gdk_draw_line(preview->blitter, grid, x2, y1, t, y2);
				break;
			}

			case ROI_GRID_HARMONIOUS_TRIANGLES2:
			{
				gdouble goldenratio = ((1+sqrt(5))/2);
				gint t, golden;

				golden = ((x2-x1)/goldenratio);

				gdk_draw_line(preview->blitter, grid, x1, y2, x2, y1);

				t = (x1+golden);
				gdk_draw_line(preview->blitter, grid, x1, y1, t, y2);

				t = (x2-golden);
				gdk_draw_line(preview->blitter, grid, x2, y2, t, y1);
				break;
			}
		}

		/* Blit to screen */
		gdk_draw_drawable(GDK_DRAWABLE(preview->drawingarea[0]->window), gc, preview->blitter,
			rect[0].x, rect[0].y,
			rect[0].x, rect[0].y,
			rect[0].width, rect[0].height);
	}
	else
	{
		gint i, n = 1;

		/* Do both if we're split */
		if (unlikely(preview->split->active))
			n = 2;

		for(i=0;i<n;i++)
			gdk_draw_rgb_image(GDK_DRAWABLE(preview->drawingarea[i]->window), gc,
				rect[i].x, rect[i].y, rect[i].width, rect[i].height, GDK_RGB_DITHER_NONE,
				GET_PIXEL(preview->buffer[i], rect[i].x, rect[i].y),
				preview->buffer[i]->rowstride);
	}

	/* Draw second window if we're split */
	if (unlikely(preview->split->active))
		gdk_draw_rgb_image(GDK_DRAWABLE(preview->drawingarea[1]->window), gc,
			rect[1].x, rect[1].y, rect[1].width, rect[1].height, GDK_RGB_DITHER_NONE,
			GET_PIXEL(preview->buffer[1], rect[1].x, rect[1].y),
			preview->buffer[1]->rowstride);
}

/**
 * Render buffer in a seperate thread.
 * Do _NOT_ use any glib or gtk calls in this function, g_thread_init() has _NOT_ been called!
 * @param data A RSPreviewWidget
 * @return Always NULL, ignore
 */
static gpointer
background_renderer(gpointer data)
{
	RSPreviewWidget *preview = RS_PREVIEW_WIDGET(data);
	gint row = 0;
	preview->bg_abort = FALSE;

	/* Main loop, do one line at a time. Watch for bg_abort as often as possible */
	while(row < preview->scaled->h)
	{
		if (preview->bg_abort) break;
		/* Give time to others - "poor mans low priority" */
		g_thread_yield();
		if (preview->bg_abort) break;
		preview->rct[0]->transform(preview->rct[0], preview->scaled->w, 1,
			preview->scaled->pixels + row * preview->scaled->rowstride,
			preview->scaled->rowstride,
			preview->buffer[0]->pixels + row * preview->buffer[0]->rowstride,
			preview->buffer[0]->rowstride);
		if (preview->bg_abort) break;
		g_thread_yield();

		/* Render exposure mask if needed */
		if (preview->exposure_mask->active)
		{
			if (preview->bg_abort) break;
			rs_image8_render_exposure_mask(preview->buffer[0], row);
			if (preview->bg_abort) break;
			g_thread_yield();
		}

		/* Render second view if needed */
		if (preview->split->active)
		{
			if (preview->bg_abort) break;
			preview->rct[1]->transform(preview->rct[1], preview->scaled->w, 1,
				preview->scaled->pixels + row * preview->scaled->rowstride,
				preview->scaled->rowstride,
				preview->buffer[1]->pixels + row * preview->buffer[1]->rowstride,
				preview->buffer[1]->rowstride);
			if (preview->bg_abort) break;
			if (preview->exposure_mask->active)
			{
				if (preview->bg_abort) break;
				rs_image8_render_exposure_mask(preview->buffer[1], row);
				if (preview->bg_abort) break;
				g_thread_yield();
			}
			if (preview->bg_abort) break;
		}
		row++;
	}

	if (preview->bg_abort == FALSE)
		UNDIRTY(preview->dirty, BUFFER);

	return(NULL);
}

/**
 * Redraws the preview widget.
 * @param preview A RSPreviewWidget
 * @param rect The rectangle to redraw, use preview->visible to only redraw visible pixels
 */
static void
rs_preview_widget_redraw(RSPreviewWidget *preview, GdkRectangle *rect)
{
	GdkRectangle fullrect[2];

	g_return_if_fail (RS_IS_PREVIEW_WIDGET(preview));
	if (!preview->photo) return;
	g_return_if_fail (preview->photo);
	g_return_if_fail (preview->photo->input);

	/* Abort the background renderer if any */
	if (preview->bg_thread)
	{
		preview->bg_abort = TRUE;
		g_thread_join(preview->bg_thread);
		preview->bg_thread = NULL;
	}

	/* Rescale if needed */
	if (ISDIRTY(preview->dirty, SCALE))
		render_scale(preview);

	/* Please ignore rect if we are set to zoom to fit */
	if (preview->zoom_to_fit)
		rect = NULL;

	/* Do some sanity checking */
	if (rect && ((rect->x+rect->width) > preview->scaled->w))
		rect = NULL;
	if (rect && ((rect->y+rect->height) > preview->scaled->h))
		rect = NULL;

	/* If we got no rect, replace by a rect covering everything */
	if (!rect)
	{
		fullrect[0].x = fullrect[1].x = 0;
		fullrect[0].y = fullrect[1].y = 0;
		fullrect[0].width = fullrect[1].width = preview->scaled->w;
		fullrect[0].height = fullrect[1].height = preview->scaled->h;
		rect = fullrect;
	}

	if (ISDIRTY(preview->dirty, SCREEN))
	{
		if (ISDIRTY(preview->dirty, BUFFER))
		{
			render_buffer(preview, rect);

			/* If zoom to fit is enabled, we should have rendered the complete buffer by now */
			if (preview->zoom_to_fit)
				UNDIRTY(preview->dirty, BUFFER);

		}

		render_screen(preview, rect);
		UNDIRTY(preview->dirty, SCREEN);

		/* Start background render thread if needed */
		if (ISDIRTY(preview->dirty, BUFFER))
			preview->bg_thread = g_thread_create_full(background_renderer, preview, 0, TRUE, TRUE, G_THREAD_PRIORITY_LOW, NULL);
	}
}

/* Internal callbacks */

static void
crop_aspect_changed(gpointer active, gpointer user_data)
{
	RSPreviewWidget *preview = RS_PREVIEW_WIDGET(user_data);
	preview->crop_aspect = *((gdouble *)active);
	DIRTY(preview->dirty, SCREEN);
	rs_preview_widget_redraw(preview, NULL);
	return;
}

static void
crop_grid_changed(gpointer active, gpointer user_data)
{
	RSPreviewWidget *preview = RS_PREVIEW_WIDGET(user_data);
	preview->roi_grid = GPOINTER_TO_INT(active);
	DIRTY(preview->dirty, SCREEN);
	rs_preview_widget_redraw(preview, NULL);
	return;
}

static void
crop_apply_clicked(GtkButton *button, gpointer user_data)
{
	RSPreviewWidget *preview = RS_PREVIEW_WIDGET(user_data);
	crop_end(preview, TRUE);
	return;
}

static void
crop_cancel_clicked(GtkButton *button, gpointer user_data)
{
	RSPreviewWidget *preview = RS_PREVIEW_WIDGET(user_data);
	crop_end(preview, FALSE);
	return;
}

static void
crop_end(RSPreviewWidget *preview, gboolean accept)
{
	if (accept)
	{
		rs_photo_set_crop(preview->photo, &preview->roi);
		DIRTY(preview->dirty, SCALE);
	}
	gtk_widget_destroy(preview->tool);
	preview->state = WB_PICKER;
	gdk_window_set_cursor(preview->drawingarea[0]->window, cur_normal);

	DIRTY(preview->dirty, SCREEN);
	rs_preview_widget_redraw(preview, preview->visible);
}

static void
adjustment_change(GtkAdjustment *do_not_use_this, RSPreviewWidget *preview)
{
	gdouble h, v;

	/* Read values from main (left) adjusters */
	h = gtk_adjustment_get_value(preview->hadjustment[0]);
	v = gtk_adjustment_get_value(preview->vadjustment[0]);

	if (preview->split_continuous)
	{
		gint position = 0.0;
		gint handle_size = 0.0;
		gdouble page_size = 0.0;
		gdouble upper = 0.0;

		/* Calculate and apply offset */
		gtk_widget_style_get (preview->pane, "handle-size", &handle_size, NULL);
		position = gtk_paned_get_position(GTK_PANED(preview->pane));
		h = h + position + handle_size;

		/* Make sure we don't scroll too far in right viewport */
		g_object_get(G_OBJECT(preview->hadjustment[1]),
			"upper", &upper,
			"page-size", &page_size,
			NULL);
		if (h > (upper-page_size))
			h = upper-page_size;
	}

	/* Synchronize secondary (rigth) adjusters */
	gtk_adjustment_set_value(preview->hadjustment[1], h);
	gtk_adjustment_set_value(preview->vadjustment[1], v);
}

static gboolean
drawingarea_expose(GtkWidget *widget, GdkEventExpose *event, RSPreviewWidget *preview)
{
	gint i,n=1;

	DIRTY(preview->dirty, SCREEN);

	/* Do both if we're split */
	if (preview->split->active)
		n = 2;

	/* Find exposed regions for both panes */
	for(i=0;i<n;i++)
	{
		/* Get adjustments */
		GtkAdjustment *vadj = gtk_viewport_get_vadjustment(GTK_VIEWPORT(preview->viewport[i]));
		GtkAdjustment *hadj = gtk_viewport_get_hadjustment(GTK_VIEWPORT(preview->viewport[i]));

		/* Sets the visible area */
		preview->visible[i].x = (gint) hadj->value;
		preview->visible[i].y = (gint) vadj->value;
		preview->visible[i].width = (gint) hadj->page_size+0.5f;
		preview->visible[i].height = (gint) vadj->page_size+0.5f;
	}

	/* Redraw the exposed area */
	rs_preview_widget_redraw(preview, preview->visible);

	return TRUE;
}

static gboolean
scroller_size_allocate_helper(RSPreviewWidget *preview)
{
	gboolean ret = FALSE;

	gdk_threads_enter();
	if (gtk_events_pending())
		ret = TRUE;
	else
	{
		/* Redraw if we have zoom-to-fit enabled */
		if (preview->zoom_to_fit)
		{
			DIRTY(preview->dirty, SCALE);
			rs_preview_widget_redraw(preview, NULL);
		}

		/* Rescale ROI if needed */
		if (preview->state & DRAW_ROI)
		{
			matrix3_affine_transform_point_int(&preview->affine,
				preview->roi.x1, preview->roi.y1,
				&preview->roi_scaled.x1, &preview->roi_scaled.y1);
			matrix3_affine_transform_point_int(&preview->affine,
				preview->roi.x2, preview->roi.y2,
				&preview->roi_scaled.x2, &preview->roi_scaled.y2);
		}
		preview->zoom_timeout_helper = 0;
	}
	gdk_threads_leave();
	return ret;
}

static gboolean
scroller_size_allocate(GtkWidget *widget, GtkAllocation *allocation, RSPreviewWidget *preview)
{
	g_return_val_if_fail (RS_IS_PREVIEW_WIDGET(preview), TRUE);

	/* Get the allocated size to use for zoom-to-fit */
	preview->width = allocation->width-6; /* Yep, 6 is a magic number, but it's certainly better than 06! :) */
	preview->height = allocation->height-6;

	if (preview->zoom_timeout_helper == 0)
		preview->zoom_timeout_helper = g_timeout_add(200, (GSourceFunc) scroller_size_allocate_helper, preview);

	return TRUE;
}

static void
zoom_in_clicked(GtkButton *button, RSPreviewWidget *preview)
{
	g_return_if_fail (RS_IS_PREVIEW_WIDGET(preview));

	rs_preview_widget_zoom_in(preview);
}

static void
zoom_out_clicked(GtkButton *button, RSPreviewWidget *preview)
{
	g_return_if_fail (RS_IS_PREVIEW_WIDGET(preview));

	rs_preview_widget_zoom_out(preview);
}

static void
zoom_fit_clicked(GtkButton *button, RSPreviewWidget *preview)
{
	g_return_if_fail (RS_IS_PREVIEW_WIDGET(preview));

	rs_preview_widget_set_zoom_to_fit(preview);
}

static void
zoom_100_clicked(GtkButton *button, RSPreviewWidget *preview)
{
	g_return_if_fail (RS_IS_PREVIEW_WIDGET(preview));

	rs_preview_widget_set_zoom(preview, 1.0f);
}

static void
zoom_changed(GtkAdjustment *adjustment, RSPreviewWidget *preview)
{
	g_return_if_fail (RS_IS_PREVIEW_WIDGET(preview));

	preview->zoom_to_fit = FALSE;
	DIRTY(preview->dirty, SCALE);
	rs_preview_widget_redraw(preview, NULL);
}

static gchar *
scale_format(GtkScale *scale, gdouble value, RSPreviewWidget *preview)
{
	g_return_val_if_fail (RS_IS_PREVIEW_WIDGET(preview), NULL);

	if (preview->zoom_to_fit)
		return g_strdup_printf("(%d%%)", (gint)(value * 100.0f + 0.5f));
	else
		return g_strdup_printf("%d%%", (gint)(value * 100.0f + 0.5f));
}

static void
split_toggled(GtkToggleButton *togglebutton, RSPreviewWidget *preview)
{
	g_return_if_fail(RS_IS_PREVIEW_WIDGET(preview));

	/* Add the second viewport */
	if (preview->viewport[1]->parent == NULL)
		gtk_paned_pack2(GTK_PANED(preview->pane), preview->viewport[1], TRUE, TRUE);

	/* Show the second view if needed */
	if (preview->split->active)
		gtk_widget_show_all(preview->viewport[1]);
	else
		gtk_widget_hide_all(preview->viewport[1]);

	/* If we're zoom-to-fit we need to rescale to make room */
	if (preview->zoom_to_fit)
		DIRTY(preview->dirty, SCALE);

	DIRTY(preview->dirty, SCREEN|BUFFER);
	rs_preview_widget_update(preview);

	/* Adjust split if we're enabled */
	if (preview->split->active)
	{
		/* We need GTK to draw everything before we can calculate split */
		GUI_CATCHUP();
		adjustment_change(NULL, preview);
	}
}

static void
exposure_mask_toggled(GtkToggleButton *togglebutton, RSPreviewWidget *preview)
{
	g_return_if_fail(RS_IS_PREVIEW_WIDGET(preview));

	DIRTY(preview->dirty, SCREEN|BUFFER);
	rs_preview_widget_update(preview);
}

static void
make_cbdata(RSPreviewWidget *preview, RS_PREVIEW_CALLBACK_DATA *cbdata, gdouble screen_x, gdouble screen_y)
{
	gdouble x,y;
	gint row, col;
	gushort *pixel;
	gdouble r=0.0f, g=0.0f, b=0.0f;

	/* Get the real coordinates */
	matrix3_affine_transform_point(&preview->inverse_affine,
		screen_x, screen_y,
		&x, &y);

	cbdata->pixel = rs_image16_get_pixel(preview->scaled, (gint) screen_x, (gint) screen_y, TRUE);

	cbdata->x = (gint) (x+0.5f);
	cbdata->y = (gint) (y+0.5f);

	/* Render current pixel */
	preview->rct[0]->transform(preview->rct[0],
		1, 1,
		cbdata->pixel, 1,
		&cbdata->pixel8, 1);

	/* Find average pixel values from 3x3 pixels */
	for(row=-1; row<2; row++)
	{
		for(col=-1; col<2; col++)
		{
			pixel = rs_image16_get_pixel(preview->scaled, screen_x+col, screen_y+row, TRUE);
			r += pixel[R]/65535.0;
			g += pixel[G]/65535.0;
			b += pixel[B]/65535.0;
		}
	}
	cbdata->pixelfloat[R] = (gfloat) r/9.0f;
	cbdata->pixelfloat[G] = (gfloat) g/9.0f;
	cbdata->pixelfloat[B] = (gfloat) b/9.0f;
}

static void
straighten(GtkMenuItem *menuitem, RSPreviewWidget *preview)
{
	preview->state = STRAIGHTEN_START;
}

static void
unstraighten(GtkMenuItem *menuitem, RSPreviewWidget *preview)
{
	preview->photo->angle = 0.0f;
	DIRTY(preview->dirty, SCALE);
	rs_preview_widget_redraw(preview, preview->visible);
}

static void
crop(GtkMenuItem *menuitem, RSPreviewWidget *preview)
{
	rs_preview_widget_crop_start(preview);
}

static void
uncrop(GtkMenuItem *menuitem, RSPreviewWidget *preview)
{
	if (rs_photo_get_crop(preview->photo))
	{
		rs_photo_set_crop(preview->photo, NULL);
		DIRTY(preview->dirty, SCALE);
		rs_preview_widget_redraw(preview, preview->visible);
	}
}

static void
split(GtkCheckMenuItem *checkmenuitem, RSPreviewWidget *preview)
{
	rs_preview_widget_set_split(preview, checkmenuitem->active);
}

static void
split_continuous(GtkCheckMenuItem *checkmenuitem, RSPreviewWidget *preview)
{
	preview->split_continuous = checkmenuitem->active;
	rs_conf_set_boolean(CONF_SPLIT_CONTINUOUS, preview->split_continuous);
	adjustment_change(NULL, preview);
}

static void
exposure_mask(GtkCheckMenuItem *checkmenuitem, RSPreviewWidget *preview)
{
	rs_preview_widget_set_show_exposure_mask(preview, checkmenuitem->active);
}

static void
right_snapshot_a(GtkMenuItem *menuitem, RSPreviewWidget *preview)
{
	preview->snapshot[1] = 0;
	rs_preview_widget_update(preview);
}

static void
right_snapshot_b(GtkMenuItem *menuitem, RSPreviewWidget *preview)
{
	preview->snapshot[1] = 1;
	rs_preview_widget_update(preview);
}

static void
right_snapshot_c(GtkMenuItem *menuitem, RSPreviewWidget *preview)
{
	preview->snapshot[1] = 2;
	rs_preview_widget_update(preview);
}

static gboolean
button_right(GtkWidget *widget, GdkEventButton *event, RSPreviewWidget *preview)
{
	/* Pop-up-menu */
	if ((event->type == GDK_BUTTON_PRESS)
		&& (event->button==3))
	{
		GtkWidget *item, *menu = gtk_menu_new();
		gint i=0;

		item = gtk_menu_item_new_with_label (_(" A "));
		gtk_widget_show (item);
		gtk_menu_attach (GTK_MENU (menu), item, 0, 1, i, i+1); i++;
		g_signal_connect (item, "activate", G_CALLBACK (right_snapshot_a), preview);

		item = gtk_menu_item_new_with_label (_(" B "));
		gtk_widget_show (item);
		gtk_menu_attach (GTK_MENU (menu), item, 0, 1, i, i+1); i++;
		g_signal_connect (item, "activate", G_CALLBACK (right_snapshot_b), preview);

		item = gtk_menu_item_new_with_label (_(" C "));
		gtk_widget_show (item);
		gtk_menu_attach (GTK_MENU (menu), item, 0, 1, i, i+1); i++;
		g_signal_connect (item, "activate", G_CALLBACK (right_snapshot_c), preview);

		gtk_menu_popup(GTK_MENU(menu), NULL, NULL, NULL, NULL, 0, GDK_CURRENT_TIME);
	}

	return FALSE;
}

static void
crop_find_size_from_aspect(RS_RECT *roi, gdouble aspect, CROP_NEAR state)
{
	const gdouble original_w = (gdouble) abs(roi->x2 - roi->x1 + 1);
	const gdouble original_h = (gdouble) abs(roi->y2 - roi->y1 + 1);
	gdouble corrected_w, corrected_h;
	gdouble original_aspect = original_w/original_h;

	if (aspect == 0.0)
		return;

	if (original_aspect > 1.0)
	{ /* landscape */
		if (original_aspect > aspect)
		{
			corrected_h = original_h;
			corrected_w = original_h * aspect;
		}
		else
		{
			corrected_w = original_w;
			corrected_h = original_w / aspect;
		}
	}
	else
	{ /* portrait */
		if ((1.0/original_aspect) > aspect)
		{
			corrected_w = original_w;
			corrected_h = original_w * aspect;
		}
		else
		{
			corrected_h = original_h;
			corrected_w = original_h / aspect;
		}
	}

	switch(state)
	{
		case CROP_NEAR_NW: /* x1,y1 */
			roi->x1 = roi->x2 - ((gint)corrected_w) + 1;
			roi->y1 = roi->y2 - ((gint)corrected_h) + 1;
			break;
		case CROP_NEAR_NE: /* x2,y1 */
			roi->x2 = roi->x1 + ((gint)corrected_w) - 1;
			roi->y1 = roi->y2 - ((gint)corrected_h) + 1;
			break;
		case CROP_NEAR_SE: /* x2,y2 */
			roi->x2 = roi->x1 + ((gint)corrected_w) - 1;
			roi->y2 = roi->y1 + ((gint)corrected_h) - 1;
			break;
		case CROP_NEAR_SW: /* x1,y2 */
			roi->x1 = roi->x2 - ((gint)corrected_w) + 1;
			roi->y2 = roi->y1 + ((gint)corrected_h) - 1;
			break;
		default: /* Shut up GCC! */
			break;
	}
}

static CROP_NEAR
crop_near(RS_RECT *roi, gint x, gint y)
{
	CROP_NEAR near = CROP_NEAR_NOTHING;
#define NEAR(aim, target) (ABS((target)-(aim))<9)
	if (NEAR(y, roi->y1)) /* N */
	{
		if (NEAR(x,roi->x1)) /* NW */
			near = CROP_NEAR_NW;
		else if (NEAR(x,roi->x2)) /* NE */
			near = CROP_NEAR_NE;
		else if ((x > roi->x1) && (x < roi->x2)) /* N */
			near = CROP_NEAR_N;
	}
	else if (NEAR(y, roi->y2)) /* S */
	{
		if (NEAR(x,roi->x1)) /* SW */
			near = CROP_NEAR_SW;
		else if (NEAR(x,roi->x2)) /* SE */
			near = CROP_NEAR_SE;
		else if ((x > roi->x1) && (x < roi->x2))
			near = CROP_NEAR_S;
	}
	else if (NEAR(x, roi->x1)
		&& (y > roi->y1)
		&& (y < roi->y2)) /* West */
		near = CROP_NEAR_W;
	else if (NEAR(x, roi->x2)
		&& (y > roi->y1)
		&& (y < roi->y2)) /* East */
		near = CROP_NEAR_E;

	if (near == CROP_NEAR_NOTHING)
	{
		if (((x>roi->x1) && (x<roi->x2))
			&& ((y>roi->y1) && (y<roi->y2))
			&& (((roi->x2-roi->x1)>2) && ((roi->y2-roi->y1)>2)))
			near = CROP_NEAR_INSIDE;
		else
			near = CROP_NEAR_OUTSIDE;
	}
	return near;
#undef NEAR
}

static gboolean
button(GtkWidget *widget, GdkEventButton *event, RSPreviewWidget *preview)
{
	const gint x = (gint) (event->x+0.5f);
	const gint y = (gint) (event->y+0.5f);
	RS_PREVIEW_CALLBACK_DATA cbdata;

	if (!preview->photo) return FALSE;

	/* White balance picker */
	if ((event->type == GDK_BUTTON_PRESS)
		&& (event->button == 1)
		&& (preview->state & WB_PICKER)
		&& g_signal_has_handler_pending(preview, signals[WB_PICKED], 0, FALSE))
	{
		make_cbdata(preview, &cbdata, event->x, event->y);
		g_signal_emit (G_OBJECT (preview), signals[WB_PICKED], 0, &cbdata);
	}
	/* Begin straighten */
	else if ((event->type == GDK_BUTTON_PRESS)
		&& (event->button==1)
		&& (preview->state & STRAIGHTEN_START))
	{
		preview->straighten_start.x = (gint) (event->x+0.5f);
		preview->straighten_start.y = (gint) (event->y+0.5f);
		preview->state = STRAIGHTEN_MOVE;
	}
	/* Move straighten */
	else if ((event->type == GDK_BUTTON_RELEASE)
		&& (event->button==1)
		&& (preview->state & STRAIGHTEN_MOVE))
	{
		preview->straighten_end.x = (gint) (event->x+0.5f);
		preview->straighten_end.y = (gint) (event->y+0.5f);
		preview->photo->angle += preview->straighten_angle;
		preview->state = WB_PICKER;

		DIRTY(preview->dirty, SCALE);
		rs_preview_widget_redraw(preview, preview->visible);
	}
	/* Cancel */
	else if ((event->type == GDK_BUTTON_PRESS)
		&& (event->button==3)
		&& (!(preview->state & NORMAL)))
	{
		DIRTY(preview->dirty, SCREEN);
		if (preview->state & CROP)
		{
			if (crop_near(&preview->roi_scaled, x, y) == CROP_NEAR_INSIDE)
			{
				crop_end(preview, TRUE);
			}
			else
				crop_end(preview, FALSE);
		}
		preview->state = WB_PICKER;
		gdk_window_set_cursor(preview->drawingarea[0]->window, cur_normal);
		rs_preview_widget_update(preview);
	}
	/* Crop begin */
	else if ((event->type == GDK_BUTTON_PRESS)
		&& (event->button==1)
		&& (preview->state & CROP_START))
	{
		preview->opposite.x = x;
		preview->opposite.y = y;
		matrix3_affine_transform_point_int(&preview->inverse_affine,
			x, y, &preview->roi.x1, &preview->roi.y1);
		matrix3_affine_transform_point_int(&preview->inverse_affine,
			x, y, &preview->roi.x2, &preview->roi.y2);
		preview->state = CROP_MOVE_CORNER;
	}
	/* Crop release */
	else if ((event->type == GDK_BUTTON_RELEASE)
		&& (event->button==1)
		&& (preview->state & CROP))
	{
		preview->state = CROP_IDLE;
	}
	/* Crop move corner */
	else if ((event->type == GDK_BUTTON_PRESS)
		&& (event->button==1)
		&& (preview->state & CROP_IDLE))
	{
		const CROP_NEAR state = crop_near(&preview->roi_scaled, x, y);

		switch(state)
		{
			case CROP_NEAR_NW:
				preview->opposite.x = preview->roi_scaled.x2;
				preview->opposite.y = preview->roi_scaled.y2;
				preview->state = CROP_MOVE_CORNER;
				break;
			case CROP_NEAR_NE:
				preview->opposite.x = preview->roi_scaled.x1;
				preview->opposite.y = preview->roi_scaled.y2;
				preview->state = CROP_MOVE_CORNER;
				break;
			case CROP_NEAR_SE:
				preview->opposite.x = preview->roi_scaled.x1;
				preview->opposite.y = preview->roi_scaled.y1;
				preview->state = CROP_MOVE_CORNER;
				break;
			case CROP_NEAR_SW:
				preview->opposite.x = preview->roi_scaled.x2;
				preview->opposite.y = preview->roi_scaled.y1;
				preview->state = CROP_MOVE_CORNER;
				break;
			case CROP_NEAR_N:
			case CROP_NEAR_S:
			case CROP_NEAR_W:
			case CROP_NEAR_E:
				preview->opposite.x = x;
				preview->opposite.y = y;
				preview->state = CROP_MOVE_ALL;
				break;
			default:
				preview->opposite.x = x;
				preview->opposite.y = y;
				preview->state = CROP_MOVE_CORNER;
				break;
		}
	}
	/* Move image */
	else if ((event->button==2))
	{
		if (event->type == GDK_BUTTON_PRESS)
		{	
			gdk_window_set_cursor(preview->drawingarea[0]->window, cur_fleur);
			preview->state |= MOVE;
		}
		else if (event->type == GDK_BUTTON_RELEASE)
		{
			gdk_window_set_cursor(preview->drawingarea[0]->window, cur_normal);
			preview->state ^= MOVE;
		}
	}
	/* Pop-up-menu */
	else if ((event->type == GDK_BUTTON_PRESS)
		&& (event->button==3)
		&& (preview->state & NORMAL))
	{
		GtkWidget *i, *menu = gtk_menu_new();
		gint n = 0;

		i = gtk_menu_item_new_with_label (_("Crop"));
		gtk_widget_show (i);
		gtk_menu_attach (GTK_MENU (menu), i, 0, 1, n, n+1); n++;
		g_signal_connect (i, "activate", G_CALLBACK (crop), preview);
		if (preview->photo->crop)
		{
			i = gtk_menu_item_new_with_label (_("Uncrop"));
			gtk_widget_show (i);
			gtk_menu_attach (GTK_MENU (menu), i, 0, 1, n, n+1); n++;
			g_signal_connect (i, "activate", G_CALLBACK (uncrop), preview);
		}
		i = gtk_menu_item_new_with_label (_("Straighten"));
		gtk_widget_show (i);
		gtk_menu_attach (GTK_MENU (menu), i, 0, 1, n, n+1); n++;
		g_signal_connect (i, "activate", G_CALLBACK (straighten), preview);
		if (preview->photo->angle != 0.0)
		{
			i = gtk_menu_item_new_with_label (_("Unstraighten"));
			gtk_widget_show (i);
			gtk_menu_attach (GTK_MENU (menu), i, 0, 1, n, n+1); n++;
			g_signal_connect (i, "activate", G_CALLBACK (unstraighten), preview);
		}
		i = gtk_separator_menu_item_new();
		gtk_widget_show (i);
		gtk_menu_attach (GTK_MENU (menu), i, 0, 1, n, n+1); n++;
		i = gtk_check_menu_item_new_with_label(_("Exposure Mask"));
		gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(i), preview->exposure_mask->active);
		gtk_widget_show (i);
		gtk_menu_attach (GTK_MENU (menu), i, 0, 1, n, n+1); n++;
		g_signal_connect (i, "toggled", G_CALLBACK (exposure_mask), preview);
		i = gtk_check_menu_item_new_with_label(_("Split"));
		gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(i), preview->split->active);
		gtk_widget_show (i);
		gtk_menu_attach (GTK_MENU (menu), i, 0, 1, n, n+1); n++;
		g_signal_connect (i, "toggled", G_CALLBACK (split), preview);
		i = gtk_check_menu_item_new_with_label(_("Split continuous"));
		gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(i), preview->split_continuous);
		gtk_widget_show (i);
		gtk_menu_attach (GTK_MENU (menu), i, 0, 1, n, n+1); n++;
		g_signal_connect (i, "toggled", G_CALLBACK (split_continuous), preview);

		gtk_menu_popup(GTK_MENU(menu), NULL, NULL, NULL, NULL, 0, GDK_CURRENT_TIME);
	}

	return TRUE;
}

static gboolean
motion(GtkWidget *widget, GdkEventMotion *event, RSPreviewWidget *preview)
{
	gint x, y;
	GdkModifierType mask;
	RS_PREVIEW_CALLBACK_DATA cbdata;
	CROP_NEAR near = NORMAL;

	static RS_COORD coord_last_abs = {-1, -1}; /* This is safe as static as long as we only got one pointer! */
	RS_COORD coord_abs = {(gint) event->x_root, (gint) event->y_root};
	RS_COORD coord_diff = {0, 0};

	/* Calculate relative movement */
	if (coord_last_abs.x!=-1)
	{
		coord_diff.x = coord_last_abs.x - coord_abs.x;
		coord_diff.y = coord_last_abs.y - coord_abs.y;
	}
	coord_last_abs.x = coord_abs.x;
	coord_last_abs.y = coord_abs.y;

	if (preview->state & MOVE)
	{
		GtkAdjustment *adj;
		gdouble val;

		if (coord_diff.x != 0)
		{
			adj = gtk_viewport_get_hadjustment(GTK_VIEWPORT(preview->viewport[0]));
			val = gtk_adjustment_get_value(adj) + coord_diff.x;
			if (val > (preview->scaled->w - adj->page_size))
				val = preview->scaled->w - adj->page_size;
			gtk_adjustment_set_value(adj, val);
		}
		if (coord_diff.y != 0)
		{
			adj = gtk_viewport_get_vadjustment(GTK_VIEWPORT(preview->viewport[0]));
			val = gtk_adjustment_get_value(adj) + coord_diff.y;
			if (val > (preview->scaled->h - adj->page_size))
				val = preview->scaled->h - adj->page_size;
			gtk_adjustment_set_value(adj, val);
		}
	}

	gdk_window_get_pointer(widget->window, &x, &y, &mask);

	/* Keep coordinates within window */
	if (x < 0)
		x = 0;
	else if (x > (widget->allocation.width-1))
		x = widget->allocation.width-1;
	if (y < 0)
		y = 0;
	else if (y > (widget->allocation.height-1))
		y = widget->allocation.height-1;

	if ((x==preview->last.x) && (y==preview->last.y)) /* Have we actually changed? */
		return(FALSE);
	preview->last.x = x;
	preview->last.y = y;

	if ((preview->state & CROP) && (preview->state != CROP_START))
	{
		/* Near anything important? */
		near = crop_near(&preview->roi_scaled, x, y);

		/* Set cursor accordingly */
		switch(near)
		{
			case CROP_NEAR_NW:
				gdk_window_set_cursor(preview->drawingarea[0]->window, cur_nw);
				break;
			case CROP_NEAR_NE:
				gdk_window_set_cursor(preview->drawingarea[0]->window, cur_ne);
				break;
			case CROP_NEAR_SE:
				gdk_window_set_cursor(preview->drawingarea[0]->window, cur_se);
				break;
			case CROP_NEAR_SW:
				gdk_window_set_cursor(preview->drawingarea[0]->window, cur_sw);
				break;
			case CROP_NEAR_N:
			case CROP_NEAR_S:
			case CROP_NEAR_W:
			case CROP_NEAR_E:
				gdk_window_set_cursor(preview->drawingarea[0]->window, cur_fleur);
				break;
			default:
				gdk_window_set_cursor(preview->drawingarea[0]->window, cur_normal);
				break;
		}
	}
	if ((mask & GDK_BUTTON1_MASK) && (preview->state & STRAIGHTEN_MOVE))
	{
		gdouble degrees;
		gint vx, vy;

		preview->straighten_end.x = x;
		preview->straighten_end.y = y;
		vx = preview->straighten_start.x - preview->straighten_end.x;
		vy = preview->straighten_start.y - preview->straighten_end.y;
		DIRTY(preview->dirty, SCREEN);
		rs_preview_widget_redraw(preview, preview->visible);
		degrees = -atan2(vy,vx)*180/M_PI;
		if (degrees>=0.0)
		{
			if ((degrees>45.0) && (degrees<=135.0))
				degrees -= 90.0;
			else if (degrees>135.0)
				degrees -= 180.0;
		}
		else /* <0.0 */
		{
			if ((degrees < -45.0) && (degrees >= -135.0))
				degrees += 90.0;
			else if (degrees<-135.0)
				degrees += 180.0;
		}
		preview->straighten_angle = degrees;
	}
	if ((mask & GDK_BUTTON1_MASK) && (preview->state & CROP_MOVE_CORNER))
	{
		CROP_NEAR corner = CROP_NEAR_NOTHING;
		gint *target_x, *target_y;
		gint *opposite_x, *opposite_y;

		if (y < preview->opposite.y) /* N */
		{
			if (x < preview->opposite.x) /* W */
			{
				target_x = &preview->roi.x1;
				target_y = &preview->roi.y1;
				opposite_x = &preview->roi.x2;
				opposite_y = &preview->roi.y2;
				corner = CROP_NEAR_NW;
			}
			else /* E */
			{
				target_x = &preview->roi.x2;
				target_y = &preview->roi.y1;
				opposite_x = &preview->roi.x1;
				opposite_y = &preview->roi.y2;
				corner = CROP_NEAR_NE;
			}
		}
		else /* S */
		{
			if (x < preview->opposite.x) /* W */
			{
				target_x = &preview->roi.x1;
				target_y = &preview->roi.y2;
				opposite_x = &preview->roi.x2;
				opposite_y = &preview->roi.y1;
				corner = CROP_NEAR_SW;
			}
			else /* E */
			{
				target_x = &preview->roi.x2;
				target_y = &preview->roi.y2;
				opposite_x = &preview->roi.x1;
				opposite_y = &preview->roi.y1;
				corner = CROP_NEAR_SE;
			}
		}

		/* Fill in oppposite corner */
		matrix3_affine_transform_point_int(&preview->inverse_affine,
			preview->opposite.x,preview->opposite.y,
			opposite_x, opposite_y);

		/* Fillin current corner */
		matrix3_affine_transform_point_int(&preview->inverse_affine,
			x,y,
			target_x, target_y);

		/* Do aspect restriction here */
		crop_find_size_from_aspect(&preview->roi, preview->crop_aspect, corner);

		/* Scale ROI back to screen */
		matrix3_affine_transform_point_int(&preview->affine,
			preview->roi.x1, preview->roi.y1,
			&preview->roi_scaled.x1, &preview->roi_scaled.y1);
		matrix3_affine_transform_point_int(&preview->affine,
			preview->roi.x2, preview->roi.y2,
			&preview->roi_scaled.x2, &preview->roi_scaled.y2);

		DIRTY(preview->dirty, SCREEN);
		rs_preview_widget_redraw(preview, preview->visible);
	}
	if ((mask & GDK_BUTTON1_MASK) && (preview->state & CROP_MOVE_ALL))
	{
		gint dist_x, dist_y;
		gint w, h;
#if 0
		dist_x = x - preview->opposite.x; /* X distance on screen */
		dist_y = y - preview->opposite.y; /* Y ... */

		/* Calculate distance in real coordinates */
		matrix3_affine_transform_point_int(&preview->inverse_affine,
			dist_x, dist_y,
			&dist_x, &dist_y);
#else
		gint x1,x2,y1,y2;
		matrix3_affine_transform_point_int(&preview->inverse_affine,
			x, y,
			&x1, &y1);
		matrix3_affine_transform_point_int(&preview->inverse_affine,
			preview->opposite.x, preview->opposite.y,
			&x2, &y2);
		dist_x = x1 - x2; /* Real distance */
		dist_y = y1 - y2;
#endif		
		/* Find our real borders */
		matrix3_affine_transform_point_int(&preview->inverse_affine,
			preview->scaled->w-1, preview->scaled->h-1, &w, &h);

		/* check borders */
		if ((preview->roi.x1 + dist_x) < 0)
			dist_x = 0 - preview->roi.x1;
		if ((preview->roi.y1 + dist_y) < 0)
			dist_y = 0 - preview->roi.y1;
		if (((preview->roi.x2 + dist_x) > w))
			dist_x = w - preview->roi.x2;
		if (((preview->roi.y2 + dist_y) > h))
			dist_y = h - preview->roi.y2;

		/* Move ROI */
		preview->roi.x1 += dist_x;
		preview->roi.y1 += dist_y;
		preview->roi.x2 += dist_x;
		preview->roi.y2 += dist_y;

		/* Set new opposite coordinates */
		preview->opposite.x = x;
		preview->opposite.y = y;

		DIRTY(preview->dirty, SCREEN);
		rs_preview_widget_redraw(preview, preview->visible);

		/* Scale ROI back to screen */
		matrix3_affine_transform_point_int(&preview->affine,
			preview->roi.x1, preview->roi.y1,
			&preview->roi_scaled.x1, &preview->roi_scaled.y1);
		matrix3_affine_transform_point_int(&preview->affine,
			preview->roi.x2, preview->roi.y2,
			&preview->roi_scaled.x2, &preview->roi_scaled.y2);
	}

	/* If anyone is listening, go ahead and emit signal */
	if (g_signal_has_handler_pending(preview, signals[MOTION_SIGNAL], 0, FALSE))
	{
		make_cbdata(preview, &cbdata, x, y);
		g_signal_emit (G_OBJECT (preview), signals[MOTION_SIGNAL], 0, &cbdata);
	}

	return FALSE;
}
