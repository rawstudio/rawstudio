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
#include "rawstudio.h"

static gboolean rs_straighten_motion_callback(GtkWidget *widget, GdkEventMotion *event, RS_BLOB *rs);
static gboolean rs_straighten_button(GtkWidget *widget, GdkEventButton *event, RS_BLOB *rs);

static gdouble angle;
static gint start_x, start_y;
static gint button_press, button_release;

void
rs_straighten_start(RS_BLOB *rs)
{
	extern GdkCursor *cur_pencil;
	if (!rs->photo) return;

	button_press = g_signal_connect (G_OBJECT (rs->preview_drawingarea),
		"button_press_event",
		G_CALLBACK (rs_straighten_button), rs);
	button_release = g_signal_connect (G_OBJECT (rs->preview_drawingarea),
		"button_release_event",
		G_CALLBACK (rs_straighten_button), rs);

	gdk_window_set_cursor(rs->preview_drawingarea->window, cur_pencil);

	return;
}

void
rs_straighten_unstraighten(RS_BLOB *rs)
{
	rs->photo->angle = 0.0;
	update_preview(rs, FALSE, TRUE);

	return;
}

static gboolean
rs_straighten_motion_callback(GtkWidget *widget, GdkEventMotion *event, RS_BLOB *rs)
{
	extern GdkGC *dashed;
	const gint vx = start_x - event->x;
	const gint vy = start_y - event->y;
	gdouble degrees;

	gdk_draw_drawable(rs->preview_drawingarea->window,
		rs->preview_drawingarea->style->fg_gc[GTK_WIDGET_STATE (rs->preview_drawingarea)],
		rs->preview_backing,
		rs->preview_exposed->x1, rs->preview_exposed->y1,
		rs->preview_exposed->x1, rs->preview_exposed->y1,
		rs->preview_exposed->x2-rs->preview_exposed->x1+1,
		rs->preview_exposed->y2-rs->preview_exposed->y1+1);
	gdk_draw_line(rs->preview_drawingarea->window, dashed,
		start_x, start_y,
		event->x, event->y);

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
	angle = degrees;

	return(TRUE);
}

static gboolean
rs_straighten_button(GtkWidget *widget, GdkEventButton *event, RS_BLOB *rs)
{
	extern GdkCursor *cur_normal;
	extern GdkCursor *cur_pencil;
	static gint motion;

	if (event->type == GDK_BUTTON_PRESS)
	{
		if ((event->button==1) && rs->preview_done)
		{
			start_x = (gint) event->x;
			start_y = (gint) event->y;
			gdk_window_set_cursor(rs->preview_drawingarea->window, cur_pencil);
			motion = g_signal_connect (G_OBJECT (rs->preview_drawingarea),
				"motion_notify_event",
				G_CALLBACK (rs_straighten_motion_callback), rs);
			return(TRUE);
		}
		else if (event->button==3)
		{
			gdk_window_set_cursor(rs->preview_drawingarea->window, cur_normal);
			g_signal_handler_disconnect(rs->preview_drawingarea, button_press);
			g_signal_handler_disconnect(rs->preview_drawingarea, button_release);
			update_preview(rs, FALSE, FALSE);
			return(TRUE);
		}
	}
	else /* release */
	{
		if (event->button==1)
		{
			gdk_window_set_cursor(rs->preview_drawingarea->window, cur_normal);
			g_signal_handler_disconnect(rs->preview_drawingarea, button_press);
			g_signal_handler_disconnect(rs->preview_drawingarea, button_release);
			g_signal_handler_disconnect(rs->preview_drawingarea, motion);
			rs->photo->angle += angle;
			update_preview(rs, FALSE, TRUE);
			return(TRUE);
		}
	}
	return(FALSE);
}
