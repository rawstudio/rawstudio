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

#include <gtk/gtk.h>
#include <config.h>
#include "gettext.h"
#include "rawstudio.h"
#include "rs-crop.h"
#include "rs-straighten.h"
#include "color.h"
#include "gtk-interface.h"
#include "conf_interface.h"
#include "drawingarea.h"
#include "rs-image.h"

static gint start_x, start_y;
static gboolean drawingarea_expose (GtkWidget *widget, GdkEventExpose *event, RS_BLOB *rs);
static gboolean drawingarea_configure (GtkWidget *widget, GdkEventExpose *event, RS_BLOB *rs);
static gboolean gui_drawingarea_move_callback(GtkWidget *widget, GdkEventMotion *event, RS_BLOB *rs);
static gboolean gui_drawingarea_button(GtkWidget *widget, GdkEventButton *event, RS_BLOB *rs);

GdkPixmap *blitter = NULL;
GdkCursor *cur_fleur;
static GdkCursor *cur_watch;
GdkCursor *cur_normal;
GdkCursor *cur_nw;
GdkCursor *cur_ne;
GdkCursor *cur_se;
GdkCursor *cur_sw;
GdkCursor *cur_pencil;

static gboolean
drawingarea_expose (GtkWidget *widget, GdkEventExpose *event, RS_BLOB *rs)
{
	GtkAdjustment *vadj;
	GtkAdjustment *hadj;
	vadj = gtk_viewport_get_vadjustment((GtkViewport *) widget->parent->parent);
	hadj = gtk_viewport_get_hadjustment((GtkViewport *) widget->parent->parent);
	rs->preview_exposed->x1 = (gint) hadj->value;
	rs->preview_exposed->y1 = (gint) vadj->value;
	rs->preview_exposed->x2 = ((gint) hadj->page_size)+rs->preview_exposed->x1-1;
	rs->preview_exposed->y2 = ((gint) vadj->page_size)+rs->preview_exposed->y1-1;
	update_preview_region(rs, rs->preview_exposed, FALSE);
	return(TRUE);
}

static gboolean
drawingarea_configure (GtkWidget *widget, GdkEventExpose *event, RS_BLOB *rs)
{
	if (rs->preview_backing)
		g_object_unref(rs->preview_backing);
	rs->preview_backing = gdk_pixmap_new(widget->window,
		widget->allocation.width,
		widget->allocation.height, -1);
	if (blitter)
		g_object_unref(blitter);
	blitter = gdk_pixmap_new(widget->window,
		widget->allocation.width,
		widget->allocation.height, -1);

	update_preview(rs, TRUE, FALSE); /* evil hack to catch bogus configure events */
	return(FALSE);
}

static gboolean
gui_drawingarea_motion_callback(GtkWidget *widget, GdkEventMotion *event, RS_BLOB *rs)
{
	gint x = (gint) event->x;
	gint y = (gint) event->y;
	gushort *pixel;

	if (!rs->photo) return FALSE;

	/* Draw RGB-values at bottom of screen */
	gui_set_values(rs, x, y);

	/* Set marker in curve widget */
	pixel = rs_image16_get_pixel(rs->photo->scaled, x, y, TRUE);
#if 0
	if (pixel)
	{
		gfloat luma = ((gfloat)pixel[R])*RLUM + ((gfloat)pixel[G])*GLUM + ((gfloat)pixel[B])*BLUM;
		luma /= 65535.0;
		rs_curve_widget_set_marker(RS_CURVE_WIDGET(rs->settings[rs->current_setting]->curve), luma);
	}
#endif
	return(FALSE);
}

static gboolean
gui_drawingarea_move_callback(GtkWidget *widget, GdkEventMotion *event, RS_BLOB *rs)
{
	GtkAdjustment *vadj;
	GtkAdjustment *hadj;
	gint v,h;
	gint x,y;

	if (!rs->photo) return(TRUE);
	if (!rs->photo->preview) return(TRUE);

	x = (gint) event->x_root;
	y = (gint) event->y_root;

	vadj = gtk_viewport_get_vadjustment((GtkViewport *) widget->parent->parent);
	hadj = gtk_viewport_get_hadjustment((GtkViewport *) widget->parent->parent);
	if (hadj->page_size < rs->photo->preview->w)
	{
		h = (gint) gtk_adjustment_get_value(hadj) + (start_x-x);
		if (h <= (rs->photo->preview->w - hadj->page_size))
			gtk_adjustment_set_value(hadj, h);
	}
	if (vadj->page_size < rs->photo->preview->h)
	{
		v = (gint) gtk_adjustment_get_value(vadj) + (start_y-y);
		if (v <= (rs->photo->preview->h - vadj->page_size))
			gtk_adjustment_set_value(vadj, v);
	}
	start_x = x;
	start_y = y;
	return (TRUE);
}

/* callbacks from popup-menu */
static void
gui_drawingarea_popup_crop(GtkMenuItem *menuitem, RS_BLOB *rs)
{
	rs_crop_start(rs);
	return;
}

static void
gui_drawingarea_popup_uncrop(GtkMenuItem *menuitem, RS_BLOB *rs)
{
	rs_crop_uncrop(rs);
	return;
}

static void
gui_drawingarea_popup_straighten(GtkMenuItem *menuitem, RS_BLOB *rs)
{
	rs_straighten_start(rs);
	return;
}

static void
gui_drawingarea_popup_unstraighten(GtkMenuItem *menuitem, RS_BLOB *rs)
{
	rs_straighten_unstraighten(rs);
	return;
}

static gboolean
gui_drawingarea_button(GtkWidget *widget, GdkEventButton *event, RS_BLOB *rs)
{
	static gint operation = OP_NONE;
	static gint signal;
	gint x,y;

	x = (gint) event->x;
	y = (gint) event->y;

	if (event->type == GDK_BUTTON_PRESS)
	{
		if (event->button==1)
			rs_set_wb_from_pixels(rs, x, y);
		else if (event->button==2)
		{
			if(!gui_is_busy())
			{
				operation = OP_MOVE;
				gdk_window_set_cursor(rs->preview_drawingarea->window, cur_fleur);
				start_x = (gint) event->x_root;
				start_y = (gint) event->y_root;
				signal = g_signal_connect (G_OBJECT (rs->preview_drawingarea), "motion_notify_event",
				G_CALLBACK (gui_drawingarea_move_callback), rs);
			}
			else
			{
				operation = OP_BUSY;
				gdk_window_set_cursor(rs->preview_drawingarea->window, cur_watch);
			}
		}
		else if ((event->button==3) && !gui_is_busy())
		{
			GtkWidget *i, *menu = gtk_menu_new();
			gint n=0;

			i = gtk_menu_item_new_with_label (_("Crop"));
			gtk_widget_show (i);
			gtk_menu_attach (GTK_MENU (menu), i, 0, 1, n, n+1); n++;
			g_signal_connect (i, "activate", G_CALLBACK (gui_drawingarea_popup_crop), rs);
			if (rs->photo->crop)
			{
				i = gtk_menu_item_new_with_label (_("Uncrop"));
				gtk_widget_show (i);
				gtk_menu_attach (GTK_MENU (menu), i, 0, 1, n, n+1); n++;
				g_signal_connect (i, "activate", G_CALLBACK (gui_drawingarea_popup_uncrop), rs);
			}
			i = gtk_menu_item_new_with_label (_("Straighten"));
			gtk_widget_show (i);
			gtk_menu_attach (GTK_MENU (menu), i, 0, 1, n, n+1); n++;
			g_signal_connect (i, "activate", G_CALLBACK (gui_drawingarea_popup_straighten), rs);
			if (rs->photo->angle != 0.0)
			{
				i = gtk_menu_item_new_with_label (_("Unstraighten"));
				gtk_widget_show (i);
				gtk_menu_attach (GTK_MENU (menu), i, 0, 1, n, n+1); n++;
				g_signal_connect (i, "activate", G_CALLBACK (gui_drawingarea_popup_unstraighten), rs);
			}

			gtk_menu_popup(GTK_MENU(menu), NULL, NULL, NULL, NULL, 0, GDK_CURRENT_TIME);
		}
	}
	else /* release */
	{
		switch(operation)
		{
			case OP_MOVE:
				g_signal_handler_disconnect(rs->preview_drawingarea, signal);
				gdk_window_set_cursor(rs->preview_drawingarea->window, cur_normal);
				operation = OP_NONE;
				break;
			case OP_BUSY:
				gdk_window_set_cursor(rs->preview_drawingarea->window, cur_normal);
				operation = OP_NONE;
				break;
		}
	}
	return(FALSE);
}

/* hack to resize a bit nicer */
static gboolean
zoom_to_fit_helper(RS_BLOB *rs)
{
	if (gtk_events_pending())
		return(TRUE);
	else
	{
		rs_zoom_to_fit(rs);
		return(FALSE);
	}
}

static void
gui_scroller_size(GtkWidget *widget, GtkAllocation *allocation, RS_BLOB *rs)
{
	rs->preview_width = allocation->width;
	rs->preview_height = allocation->height;
	if (rs->zoom_to_fit)
		g_timeout_add(10, (GSourceFunc) zoom_to_fit_helper, rs);
	return;
}

GtkWidget *
gui_drawingarea_make(RS_BLOB *rs)
{
	GtkWidget *scroller;
	GtkWidget *viewport;
	GtkWidget *align;
	GdkColor color;

	/* initialize cursors */
	cur_fleur = gdk_cursor_new(GDK_FLEUR);
	cur_watch = gdk_cursor_new(GDK_WATCH);
	cur_nw = gdk_cursor_new(GDK_TOP_LEFT_CORNER);
	cur_ne = gdk_cursor_new(GDK_TOP_RIGHT_CORNER);
	cur_se = gdk_cursor_new(GDK_BOTTOM_RIGHT_CORNER);
	cur_sw = gdk_cursor_new(GDK_BOTTOM_LEFT_CORNER);
	cur_pencil = gdk_cursor_new(GDK_PENCIL);
	cur_normal = NULL;

	scroller = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scroller),
		GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

	viewport = gtk_viewport_new (NULL, NULL);
	gtk_container_add (GTK_CONTAINER (scroller), viewport);

	align = gtk_alignment_new(0.5, 0.5, 0.0, 0.0);
	gtk_container_add (GTK_CONTAINER (viewport), align);

	rs->preview_drawingarea = gtk_drawing_area_new();
	
	g_signal_connect (GTK_OBJECT (rs->preview_drawingarea), "expose-event",
		GTK_SIGNAL_FUNC (drawingarea_expose), rs);
	g_signal_connect (GTK_OBJECT (rs->preview_drawingarea), "configure-event",
		GTK_SIGNAL_FUNC (drawingarea_configure), rs);
	g_signal_connect_after (G_OBJECT (rs->preview_drawingarea), "button_press_event",
		G_CALLBACK (gui_drawingarea_button), rs);
	g_signal_connect_after (G_OBJECT (rs->preview_drawingarea), "button_release_event",
		G_CALLBACK (gui_drawingarea_button), rs);
	g_signal_connect (G_OBJECT (scroller), "size-allocate",
		G_CALLBACK (gui_scroller_size), rs);
	g_signal_connect (G_OBJECT (rs->preview_drawingarea), "motion_notify_event",
		G_CALLBACK (gui_drawingarea_motion_callback), rs);
	gtk_widget_set_events(rs->preview_drawingarea, 0
		| GDK_BUTTON_PRESS_MASK
		| GDK_BUTTON_RELEASE_MASK
		| GDK_POINTER_MOTION_MASK);
	gtk_container_add (GTK_CONTAINER (align), (GtkWidget *) rs->preview_drawingarea);

	COLOR_BLACK(color);
	rs_conf_get_color(CONF_PREBGCOLOR, &color);
	gtk_widget_modify_bg(viewport, GTK_STATE_NORMAL, &color);
	gtk_widget_modify_bg(rs->preview_drawingarea, GTK_STATE_NORMAL, &color);

	return(scroller);
}
