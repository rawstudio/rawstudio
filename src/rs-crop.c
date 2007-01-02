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

#include <glib.h>
#include "rawstudio.h"
#include "gtk-interface.h"

gboolean rs_crop_motion_callback(GtkWidget *widget, GdkEventMotion *event, RS_BLOB *rs);
gboolean rs_crop_button_callback(GtkWidget *widget, GdkEventButton *event, RS_BLOB *rs);
gboolean rs_crop_resize_callback(GtkWidget *widget, GdkEventMotion *event, RS_BLOB *rs);

gint state;
static gint motion, button_press, button_release;
static gint start_x, start_y;
static RS_RECT last = {0,0,0,0}; /* Initialize with more meaningfull values */

enum {
	STATE_CROP,
	STATE_CROP_MOVE_N,
	STATE_CROP_MOVE_E,
	STATE_CROP_MOVE_S,
	STATE_CROP_MOVE_W,
	STATE_CROP_MOVE_NW,
	STATE_CROP_MOVE_NE,
	STATE_CROP_MOVE_SE,
	STATE_CROP_MOVE_SW,
	STATE_CROP_MOVE_NEW,
};

void
rs_crop_start(RS_BLOB *rs)
{
	if (!rs->photo) return;
	rs->roi_scaled.x1 = 0;
	rs->roi_scaled.y1 = 0;
	rs->roi_scaled.x2 = rs->photo->scaled->w-1;
	rs->roi_scaled.y2 = rs->photo->scaled->h-1;
	rs_rect_scale(&rs->roi_scaled, &rs->roi, 1.0/GETVAL(rs->scale));
	rs_mark_roi(rs, TRUE);
	state = STATE_CROP;

	motion = g_signal_connect (G_OBJECT (rs->preview_drawingarea),
		"motion_notify_event",
		G_CALLBACK (rs_crop_motion_callback), rs);
	button_press = g_signal_connect (G_OBJECT (rs->preview_drawingarea),
		"button_press_event",
		G_CALLBACK (rs_crop_button_callback), rs);
	button_release = g_signal_connect (G_OBJECT (rs->preview_drawingarea),
		"button_release_event",
		G_CALLBACK (rs_crop_button_callback), rs);

	update_preview(rs, FALSE, FALSE);
	return;
}

void
rs_crop_uncrop(RS_BLOB *rs)
{
	if (!rs->photo) return;
	if (rs->photo->crop)
	{
		g_free(rs->photo->crop);
		rs->photo->crop = NULL;
	}
	update_preview(rs, FALSE, TRUE);
	return;
}

#define NEAR 10
gboolean
rs_crop_motion_callback(GtkWidget *widget, GdkEventMotion *event, RS_BLOB *rs)
{
	gint x = (gint) event->x;
	gint y = (gint) event->y;
	extern GdkCursor *cur_normal;
	extern GdkCursor *cur_n;
	extern GdkCursor *cur_e;
	extern GdkCursor *cur_s;
	extern GdkCursor *cur_w;
	extern GdkCursor *cur_nw;
	extern GdkCursor *cur_ne;
	extern GdkCursor *cur_se;
	extern GdkCursor *cur_sw;

	if (abs(x-rs->roi_scaled.x1)<10) /* west block */
	{
		if (abs(y-rs->roi_scaled.y1)<10)
			gdk_window_set_cursor(rs->preview_drawingarea->window, cur_nw);
		else if (abs(y-rs->roi_scaled.y2)<10)
			gdk_window_set_cursor(rs->preview_drawingarea->window, cur_sw);
		else if ((y>rs->roi_scaled.y1) && (y<rs->roi_scaled.y2))
			gdk_window_set_cursor(rs->preview_drawingarea->window, cur_w);
		else
			gdk_window_set_cursor(rs->preview_drawingarea->window, cur_normal);
	}
	else if (abs(x-rs->roi_scaled.x2)<10) /* east block */
	{
		if (abs(y-rs->roi_scaled.y1)<10)
			gdk_window_set_cursor(rs->preview_drawingarea->window, cur_ne);
		else if (abs(y-rs->roi_scaled.y2)<10)
			gdk_window_set_cursor(rs->preview_drawingarea->window, cur_se);
		else if ((y>rs->roi_scaled.y1) && (y<rs->roi_scaled.y2))
			gdk_window_set_cursor(rs->preview_drawingarea->window, cur_e);
		else
			gdk_window_set_cursor(rs->preview_drawingarea->window, cur_normal);
	}
	else if ((x>rs->roi_scaled.x1) && (x<rs->roi_scaled.x2)) /* poles */
	{
		if (abs(y-rs->roi_scaled.y1)<10)
			gdk_window_set_cursor(rs->preview_drawingarea->window, cur_n);
		else if (abs(y-rs->roi_scaled.y2)<10)
			gdk_window_set_cursor(rs->preview_drawingarea->window, cur_s);
		else
			gdk_window_set_cursor(rs->preview_drawingarea->window, cur_normal);
	}
	else
		gdk_window_set_cursor(rs->preview_drawingarea->window, cur_normal);

	return(FALSE);
}
#undef NEAR

gboolean
rs_crop_resize_callback(GtkWidget *widget, GdkEventMotion *event, RS_BLOB *rs)
{
	gint x = (gint) event->x;
	gint y = (gint) event->y;
	RS_RECT region;

	switch (state)
	{
		case STATE_CROP_MOVE_NEW:
			rs->roi_scaled.x1 = start_x;
			rs->roi_scaled.x2 = start_x;
			rs->roi_scaled.y1 = start_y;
			rs->roi_scaled.y2 = start_y;
			if(x>start_x)
			{
				if (y>start_y)
					state=STATE_CROP_MOVE_SW;
				else
					state=STATE_CROP_MOVE_NW;
			}
			else
			{
				if (y>start_y)
					state=STATE_CROP_MOVE_SE;
				else
					state=STATE_CROP_MOVE_NE;
			}

		case STATE_CROP_MOVE_N:
			rs->roi_scaled.y1 = y;
			break;
		case STATE_CROP_MOVE_E:
			rs->roi_scaled.x2 = x;
			break;
		case STATE_CROP_MOVE_S:
			rs->roi_scaled.y2 = y;
			break;
		case STATE_CROP_MOVE_W:
			rs->roi_scaled.x1 = x;
			break;
		case STATE_CROP_MOVE_NW:
			rs->roi_scaled.x1 = x;
			rs->roi_scaled.y1 = y;
			break;
		case STATE_CROP_MOVE_NE:
			rs->roi_scaled.x2 = x;
			rs->roi_scaled.y1 = y;
			break;
		case STATE_CROP_MOVE_SE:
			rs->roi_scaled.x2 = x;
			rs->roi_scaled.y2 = y;
			break;
		case STATE_CROP_MOVE_SW:
			rs->roi_scaled.x1 = x;
			rs->roi_scaled.y2 = y;
			break;
	}
	if (rs->roi_scaled.x1 < 0)
		rs->roi_scaled.x1 = 0;
	if (rs->roi_scaled.x2 < 0)
		rs->roi_scaled.x2 = 0;
	if (rs->roi_scaled.y1 < 0)
		rs->roi_scaled.y1 = 0;
	if (rs->roi_scaled.y2 < 0)
		rs->roi_scaled.y2 = 0;

	if (rs->roi_scaled.x1 > rs->photo->preview->w)
		rs->roi_scaled.x1 = rs->photo->preview->w-1;
	if (rs->roi_scaled.x2 > rs->photo->preview->w)
		rs->roi_scaled.x2 = rs->photo->preview->w-1;
	if (rs->roi_scaled.y1 > rs->photo->preview->h)
		rs->roi_scaled.y1 = rs->photo->preview->h-1;
	if (rs->roi_scaled.y2 > rs->photo->preview->h)
		rs->roi_scaled.y2 = rs->photo->preview->h-1;

	if (rs->roi_scaled.x1 > rs->roi_scaled.x2)
	{
		SWAP(rs->roi_scaled.x1, rs->roi_scaled.x2);
		switch (state)
		{
			case STATE_CROP_MOVE_E:
				state = STATE_CROP_MOVE_W;
				break;
			case STATE_CROP_MOVE_W:
				state = STATE_CROP_MOVE_E;
				break;
			case STATE_CROP_MOVE_NW:
				state = STATE_CROP_MOVE_NE;
				break;
			case STATE_CROP_MOVE_NE:
				state = STATE_CROP_MOVE_NW;
				break;
			case STATE_CROP_MOVE_SW:
				state = STATE_CROP_MOVE_SE;
				break;
			case STATE_CROP_MOVE_SE:
				state = STATE_CROP_MOVE_SW;
				break;
		}
	}
	if (rs->roi_scaled.y1 > rs->roi_scaled.y2)
	{
		SWAP(rs->roi_scaled.y1, rs->roi_scaled.y2);
		switch (state)
		{
			case STATE_CROP_MOVE_N:
				state = STATE_CROP_MOVE_S;
				break;
			case STATE_CROP_MOVE_S:
				state = STATE_CROP_MOVE_N;
				break;
			case STATE_CROP_MOVE_NW:
				state = STATE_CROP_MOVE_SW;
				break;
			case STATE_CROP_MOVE_NE:
				state = STATE_CROP_MOVE_SE;
				break;
			case STATE_CROP_MOVE_SW:
				state = STATE_CROP_MOVE_NW;
				break;
			case STATE_CROP_MOVE_SE:
				state = STATE_CROP_MOVE_NE;
				break;
		}
	}
	region.x1 = (rs->roi_scaled.x1 < last.x1) ? rs->roi_scaled.x1 : last.x1;
	region.x2 = (rs->roi_scaled.x2 > last.x2) ? rs->roi_scaled.x2 : last.x2;
	region.y1 = (rs->roi_scaled.y1 < last.y1) ? rs->roi_scaled.y1 : last.y1;
	region.y2 = (rs->roi_scaled.y2 > last.y2) ? rs->roi_scaled.y2 : last.y2;
	last.x1 = rs->roi_scaled.x1;
	last.x2 = rs->roi_scaled.x2;
	last.y1 = rs->roi_scaled.y1;
	last.y2 = rs->roi_scaled.y2;

	matrix3_affine_transform_point_int(&rs->photo->inverse_affine,
		rs->roi_scaled.x1, rs->roi_scaled.y1,
		&rs->roi.x1, &rs->roi.y1);
	matrix3_affine_transform_point_int(&rs->photo->inverse_affine,
		rs->roi_scaled.x2, rs->roi_scaled.y2,
		&rs->roi.x2, &rs->roi.y2);
	update_preview_region(rs, &region, FALSE);
	return(TRUE);
}

gboolean
rs_crop_button_callback(GtkWidget *widget, GdkEventButton *event, RS_BLOB *rs)
{
	static gint signal;
	extern GdkCursor *cur_normal;
	gint x = (gint) event->x;
	gint y = (gint) event->y;

	if (event->type == GDK_BUTTON_PRESS)
	{
		if (((event->button==1) && (state == STATE_CROP)) && rs->preview_done)
		{
			if (abs(x-rs->roi_scaled.x1)<10) /* west block */
			{
				if (abs(y-rs->roi_scaled.y1)<10)
					state = STATE_CROP_MOVE_NW;
				else if (abs(y-rs->roi_scaled.y2)<10)
					state = STATE_CROP_MOVE_SW;
				else if ((y>rs->roi_scaled.y1) && (y<rs->roi_scaled.y2))
					state = STATE_CROP_MOVE_W;
				else
					state = STATE_CROP_MOVE_NEW;
			}
			else if (abs(x-rs->roi_scaled.x2)<10) /* east block */
			{
				if (abs(y-rs->roi_scaled.y1)<10)
					state = STATE_CROP_MOVE_NE;
				else if (abs(y-rs->roi_scaled.y2)<10)
					state = STATE_CROP_MOVE_SE;
				else if ((y>rs->roi_scaled.y1) && (y<rs->roi_scaled.y2))
					state = STATE_CROP_MOVE_E;
				else
					state = STATE_CROP_MOVE_NEW;
			}
			else if ((x>rs->roi_scaled.x1) && (x<rs->roi_scaled.x2)) /* poles */
			{
				if (abs(y-rs->roi_scaled.y1)<10)
					state = STATE_CROP_MOVE_N;
				else if (abs(y-rs->roi_scaled.y2)<10)
					state = STATE_CROP_MOVE_S;
				else
					state = STATE_CROP_MOVE_NEW;
			}
			else
				state = STATE_CROP_MOVE_NEW;

			if (state==STATE_CROP_MOVE_NEW)
			{
				start_x = x;
				start_y = y;
			}				
			if (state!=STATE_CROP)
			{
				signal = g_signal_connect (G_OBJECT (rs->preview_drawingarea),
					"motion_notify_event",
					G_CALLBACK (rs_crop_resize_callback), rs);
				last = *rs->preview_exposed;
			}
			return(TRUE);
		}
		else if ((event->button==3) && (state == STATE_CROP))
		{
			if (((x>rs->roi_scaled.x1) && (x<rs->roi_scaled.x2)) && ((y>rs->roi_scaled.y1) && (y<rs->roi_scaled.y2)))
			{
				if (!rs->photo->crop)
					rs->photo->crop = (RS_RECT *) g_malloc(sizeof(RS_RECT));
				rs->photo->crop->x1 = rs->roi.x1;
				rs->photo->crop->y1 = rs->roi.y1;
				rs->photo->crop->x2 = rs->roi.x2;
				rs->photo->crop->y2 = rs->roi.y2;
			}
			rs_mark_roi(rs, FALSE);
			g_signal_handler_disconnect(rs->preview_drawingarea, motion);
			g_signal_handler_disconnect(rs->preview_drawingarea, button_press);
			g_signal_handler_disconnect(rs->preview_drawingarea, button_release);
			update_preview(rs, FALSE, TRUE);
			gdk_window_set_cursor(rs->preview_drawingarea->window, cur_normal);
			return(TRUE);
		}
	}
	else /* release */
	{
		switch(state)
		{
			case STATE_CROP_MOVE_N:
			case STATE_CROP_MOVE_E:
			case STATE_CROP_MOVE_S:
			case STATE_CROP_MOVE_W:
			case STATE_CROP_MOVE_NW:
			case STATE_CROP_MOVE_NE:
			case STATE_CROP_MOVE_SE:
			case STATE_CROP_MOVE_SW:
				g_signal_handler_disconnect(rs->preview_drawingarea, signal);
				state = STATE_CROP;
				return(TRUE);
				break;
		}
	}
	return(FALSE);
}
