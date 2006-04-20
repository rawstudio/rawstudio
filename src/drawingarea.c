#include <gtk/gtk.h>
#include "dcraw_api.h"
#include "matrix.h"
#include "rawstudio.h"
#include "gtk-interface.h"
gint start_x, start_y;

gboolean
drawingarea_expose (GtkWidget *widget, GdkEventExpose *event, RS_BLOB *rs)
{
	GtkAdjustment *vadj;
	GtkAdjustment *hadj;
	vadj = gtk_viewport_get_vadjustment((GtkViewport *) widget->parent->parent);
	hadj = gtk_viewport_get_hadjustment((GtkViewport *) widget->parent->parent);
	rs->preview_exposed->x1 = (gint) hadj->value;
	rs->preview_exposed->y1 = (gint) vadj->value;
	rs->preview_exposed->x2 = ((gint) hadj->page_size)+rs->preview_exposed->x1;
	rs->preview_exposed->y2 = ((gint) vadj->page_size)+rs->preview_exposed->y1;
	if (rs->preview_done)
		gdk_draw_drawable(widget->window, widget->style->fg_gc[GTK_WIDGET_STATE (widget)],
			rs->preview_backing,
			event->area.x, event->area.y,
			event->area.x, event->area.y,
			event->area.width, event->area.height);
	else
		update_preview_region(rs, rs->preview_exposed->x1, rs->preview_exposed->y1,
			rs->preview_exposed->x2, rs->preview_exposed->y2);
	return(TRUE);
}

gboolean
drawingarea_configure (GtkWidget *widget, GdkEventExpose *event, RS_BLOB *rs)
{
	if (rs->preview_backing)
		g_object_unref(rs->preview_backing);
	rs->preview_backing = gdk_pixmap_new(widget->window,
		widget->allocation.width,
		widget->allocation.height, -1);
	update_preview(rs); /* evil hack to catch bogus configure events */
	return(TRUE);
}

gboolean
gui_drawingarea_move_callback(GtkWidget *widget, GdkEventMotion *event, RS_BLOB *rs)
{
	GtkAdjustment *vadj;
	GtkAdjustment *hadj;
	gint v,h;
	gint x,y;

	x = (gint) event->x_root;
	y = (gint) event->y_root;

	vadj = gtk_viewport_get_vadjustment((GtkViewport *) widget->parent->parent);
	hadj = gtk_viewport_get_hadjustment((GtkViewport *) widget->parent->parent);
	if (hadj->page_size < rs->preview->w)
	{
		h = (gint) gtk_adjustment_get_value(hadj) + (start_x-x);
		if (h <= (rs->preview->w - hadj->page_size))
			gtk_adjustment_set_value(hadj, h);
	}
	if (vadj->page_size < rs->preview->h)
	{
		v = (gint) gtk_adjustment_get_value(vadj) + (start_y-y);
		if (v <= (rs->preview->h - vadj->page_size))
			gtk_adjustment_set_value(vadj, v);
	}
	start_x = x;
	start_y = y;
	return (TRUE);
}

gboolean
gui_drawingarea_button(GtkWidget *widget, GdkEventButton *event, RS_BLOB *rs)
{
	static GdkCursor *cursor;
	static gint operation = OP_NONE;
	static gint signal;
	gint x,y;

	x = (gint) event->x;
	y = (gint) event->y;

	if (event->type == GDK_BUTTON_PRESS)
	{ /* start */
		switch(event->button)
		{
			case 1:
				rs_set_warmth_from_color(rs, x, y);
				break;
			case 2:
				operation = OP_MOVE;
				cursor = gdk_cursor_new(GDK_FLEUR);
				gdk_window_set_cursor(rs->preview_drawingarea->window, cursor);
				start_x = (gint) event->x_root;
				start_y = (gint) event->y_root;
				signal = g_signal_connect (G_OBJECT (rs->preview_drawingarea), "motion_notify_event",
					G_CALLBACK (gui_drawingarea_move_callback), rs);
				break;
		}
	}
	else
	{ /* end */
		switch(operation)
		{
			case OP_MOVE:
				g_signal_handler_disconnect(rs->preview_drawingarea, signal);
				update_preview_region(rs,
					rs->preview_exposed->x1, rs->preview_exposed->y1,
					rs->preview_exposed->x2, rs->preview_exposed->y2);
				gdk_window_set_cursor(rs->preview_drawingarea->window, NULL);
				gdk_cursor_unref(cursor);
				operation = OP_NONE;
				break;
		}
	}
	return(TRUE);
}
