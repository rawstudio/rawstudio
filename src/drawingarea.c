/*
 * Copyright (C) 2006 Anders Brander <anders@brander.dk> and 
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
#include "color.h"
#include "gtk-interface.h"
#include "conf_interface.h"

static gdouble angle;
static gint start_x, start_y;
static void draw_region_crop(RS_BLOB *rs, RS_RECT *region);
static gboolean drawingarea_expose (GtkWidget *widget, GdkEventExpose *event, RS_BLOB *rs);
static gboolean drawingarea_configure (GtkWidget *widget, GdkEventExpose *event, RS_BLOB *rs);
static gboolean gui_drawingarea_move_callback(GtkWidget *widget, GdkEventMotion *event, RS_BLOB *rs);
static gboolean gui_drawingarea_button(GtkWidget *widget, GdkEventButton *event, RS_BLOB *rs);

GdkPixmap *blitter = NULL;
static RS_RECT last = {0,0,0,0};

static GdkCursor *cur_fleur;
static GdkCursor *cur_watch;
static GdkCursor *cur_normal;
static GdkCursor *cur_n;
static GdkCursor *cur_e;
static GdkCursor *cur_s;
static GdkCursor *cur_w;
static GdkCursor *cur_nw;
static GdkCursor *cur_ne;
static GdkCursor *cur_se;
static GdkCursor *cur_sw;
static GdkCursor *cur_pencil;

static void
draw_region_crop(RS_BLOB *rs, RS_RECT *region)
{
	extern GdkGC *dashed;
	extern GdkGC *grid;
	RS_RECT target;
	RS_RECT target_roi;
	GdkGC *gc = rs->preview_drawingarea->style->fg_gc[GTK_WIDGET_STATE (rs->preview_drawingarea)];
	rs_rect_union(rs->preview_exposed, region, &target);
	rs_rect_union(rs->preview_exposed, &rs->roi_scaled, &target_roi);

	gdk_draw_drawable(blitter, gc,
		rs->preview_backing_crop,
		target.x1, target.y1,
		target.x1, target.y1,
		target.x2-target.x1+1,
		target.y2-target.y1+1);
	gdk_draw_drawable(blitter, gc,
		rs->preview_backing,
		target_roi.x1, target_roi.y1,
		target_roi.x1, target_roi.y1,
		target_roi.x2-target_roi.x1+1,
		target_roi.y2-target_roi.y1+1);
	gdk_draw_rectangle(blitter, dashed, FALSE,
		rs->roi_scaled.x1, rs->roi_scaled.y1,
		rs->roi_scaled.x2-rs->roi_scaled.x1,
		rs->roi_scaled.y2-rs->roi_scaled.y1);

	gint crop_grid = CROP_GRID_NONE;

	if (crop_grid)
	{
		gint x1, x2, y1, y2;
		x1 = rs->roi_scaled.x1;
		y1 = rs->roi_scaled.y1;
		x2 = rs->roi_scaled.x2;
		y2 = rs->roi_scaled.y2;
		
		/* We should support all these http://powerretouche.com/Divine_proportion_tutorial.htm */
		switch(crop_grid)
		{
			case CROP_GRID_GOLDEN:
			{
				gdouble goldenratio = ((1+sqrt(5))/2);
				gint t, golden;

				/* vertical */
				golden = ((x2-x1)/goldenratio);

				t = (x1+golden);
				gdk_draw_line(blitter, grid, t, y1, t, y2);
				t = (x2-golden);
				gdk_draw_line(blitter, grid, t, y1, t, y2);

				/* horizontal */
				golden = ((y2-y1)/goldenratio);
	
				t = (y1+golden);
				gdk_draw_line(blitter, grid, x1, t, x2, t);
				t = (y2-golden);
				gdk_draw_line(blitter, grid, x1, t, x2, t);

			} break;
			
			case CROP_GRID_THIRDS:
			{
				gint t;

				/* vertical */
				t = ((x2-x1+1)/3*1+x1);
				gdk_draw_line(blitter, grid, t, y1, t, y2);
				t = ((x2-x1+1)/3*2+x1);
				gdk_draw_line(blitter, grid, t, y1, t, y2);

				/* horizontal */
				t = ((y2-y1+1)/3*1+y1);
				gdk_draw_line(blitter, grid, x1, t, x2, t);
				t = ((y2-y1+1)/3*2+y1);
				gdk_draw_line(blitter, grid, x1, t, x2, t);

			} break;
			
			case CROP_GRID_GOLDEN_TRIANGLES1:
			{
				gdouble goldenratio = ((1+sqrt(5))/2);
				gint t, golden;

				golden = ((x2-x1)/goldenratio);

				gdk_draw_line(blitter, grid, x1, y1, x2, y2);

				t = (x2-golden);
				gdk_draw_line(blitter, grid, x1, y2, t, y1);

				t = (x1+golden);
				gdk_draw_line(blitter, grid, x2, y1, t, y2);
			} break;
			

			case CROP_GRID_GOLDEN_TRIANGLES2:
			{
				gdouble goldenratio = ((1+sqrt(5))/2);
				gint t, golden;

				golden = ((x2-x1)/goldenratio);

				gdk_draw_line(blitter, grid, x2, y1, x1, y2);

				t = (x2-golden);
				gdk_draw_line(blitter, grid, x1, y1, t, y2);

				t = (x1+golden);
				gdk_draw_line(blitter, grid, x2, y2, t, y1);
			} break;

			case CROP_GRID_HARMONIOUS_TRIANGLES1:
			{
				gdouble goldenratio = ((1+sqrt(5))/2);
				gint t, golden;

				golden = ((x2-x1)/goldenratio);

				gdk_draw_line(blitter, grid, x1, y1, x2, y2);

				t = (x1+golden);
				gdk_draw_line(blitter, grid, x1, y2, t, y1);

				t = (x2-golden);
				gdk_draw_line(blitter, grid, x2, y1, t, y2);
			} break;

			case CROP_GRID_HARMONIOUS_TRIANGLES2:
			{
				gdouble goldenratio = ((1+sqrt(5))/2);
				gint t, golden;

				golden = ((x2-x1)/goldenratio);

				gdk_draw_line(blitter, grid, x1, y2, x2, y1);

				t = (x1+golden);
				gdk_draw_line(blitter, grid, x1, y1, t, y2);

				t = (x2-golden);
				gdk_draw_line(blitter, grid, x2, y2, t, y1);
			} break;
		}
	}

	/* blit to screen */
	gdk_draw_drawable(rs->preview_drawingarea->window, gc, blitter,
		target.x1, target.y1,
		target.x1, target.y1,
		target.x2-target.x1+1,
		target.y2-target.y1+1);
	return;
}

gboolean
drawingarea_expose (GtkWidget *widget, GdkEventExpose *event, RS_BLOB *rs)
{
	extern gint state;
	GtkAdjustment *vadj;
	GtkAdjustment *hadj;
	vadj = gtk_viewport_get_vadjustment((GtkViewport *) widget->parent->parent);
	hadj = gtk_viewport_get_hadjustment((GtkViewport *) widget->parent->parent);
	rs->preview_exposed->x1 = (gint) hadj->value;
	rs->preview_exposed->y1 = (gint) vadj->value;
	rs->preview_exposed->x2 = ((gint) hadj->page_size)+rs->preview_exposed->x1;
	rs->preview_exposed->y2 = ((gint) vadj->page_size)+rs->preview_exposed->y1;
	switch (state)
	{
		case STATE_NORMAL:
			if (rs->preview_done)
				gdk_draw_drawable(widget->window, widget->style->fg_gc[GTK_WIDGET_STATE (widget)],
					rs->preview_backing,
					event->area.x, event->area.y,
					event->area.x, event->area.y,
					event->area.width, event->area.height);
			else
				update_preview_region(rs, rs->preview_exposed);
			break;
		case STATE_CROP:
			if (rs->preview_done)
				draw_region_crop(rs, rs->preview_exposed);
			else
				update_preview_region(rs, rs->preview_exposed);
			break;
		default:
			update_preview_region(rs, rs->preview_exposed);
			break;
	}
	return(TRUE);
}

gboolean
drawingarea_configure (GtkWidget *widget, GdkEventExpose *event, RS_BLOB *rs)
{
	extern gint state;
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
	if (state==STATE_CROP) /* is this the only state where we can expect configure? */
	{
		g_object_unref(rs->preview_backing_crop);
		rs->preview_backing_crop = gdk_pixmap_new(widget->window,
			widget->allocation.width,
			widget->allocation.height, -1);
	}
	update_preview(rs, TRUE, FALSE); /* evil hack to catch bogus configure events */
	return(TRUE);
}

gboolean
gui_drawingarea_motion_callback(GtkWidget *widget, GdkEventMotion *event, RS_BLOB *rs)
{
	extern gint state;
	gint x = (gint) event->x;
	gint y = (gint) event->y;

	gui_set_values(rs, x, y);

	if (state == STATE_CROP)
	{
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
	}
	return(FALSE);
}

gboolean
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

gboolean
gui_drawingarea_crop_motion_callback(GtkWidget *widget, GdkEventMotion *event, RS_BLOB *rs)
{
	extern gint state;
	gint x,y;
	RS_RECT region;

	x = (gint) event->x;
	y = (gint) event->y;

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
	draw_region_crop(rs, &region);
	return(TRUE);
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
	extern gint state;
	gdk_window_set_cursor(rs->preview_drawingarea->window, cur_pencil);
	state = STATE_STRAIGHTEN;
	return;
}

static void
gui_drawingarea_popup_unstraighten(GtkMenuItem *menuitem, RS_BLOB *rs)
{
	rs->photo->angle = 0.0;
	update_preview(rs, FALSE, TRUE);
	return;
}

gboolean
gui_drawingarea_straighten_motion_callback(GtkWidget *widget, GdkEventMotion *event, RS_BLOB *rs)
{
	extern GdkGC *dashed;
	const gint x = event->x;
	const gint y = event->y;
	const gint vx = start_x - x;
	const gint vy = start_y - y;
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
		x, y);

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

gboolean
gui_drawingarea_button(GtkWidget *widget, GdkEventButton *event, RS_BLOB *rs)
{
	extern gint state;
	static gint operation = OP_NONE;
	static gint signal;
	gint x,y;

	x = (gint) event->x;
	y = (gint) event->y;

	if (event->type == GDK_BUTTON_PRESS)
	{
		if ((event->button==1) && (state == STATE_NORMAL))
			rs_set_wb_from_pixels(rs, x, y);
		else if ((event->button==2) && (state != STATE_STRAIGHTEN_DRAW))
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
		else if (((event->button==3) && (state == STATE_NORMAL)) && !gui_is_busy())
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
		else if (((event->button==1) && (state == STATE_CROP)) && rs->preview_done)
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
				last = *rs->preview_exposed;
				signal = g_signal_connect (G_OBJECT (rs->preview_drawingarea),
					"motion_notify_event",
					G_CALLBACK (gui_drawingarea_crop_motion_callback), rs);
			}
		}
		else if ((event->button==3) && (state == STATE_CROP))
		{
			if (((x>rs->roi_scaled.x1) && (x<rs->roi_scaled.x2)) && ((y>rs->roi_scaled.y1) && (y<rs->roi_scaled.y2)))
				rs_crop_end(rs, TRUE);
			else
				rs_crop_end(rs, FALSE);
			gdk_window_set_cursor(rs->preview_drawingarea->window, cur_normal);
		}
		else if (((event->button==1) && (state == STATE_STRAIGHTEN)) && rs->preview_done)
		{
			start_x = (gint) event->x;
			start_y = (gint) event->y;
			gdk_window_set_cursor(rs->preview_drawingarea->window, cur_pencil);
			state = STATE_STRAIGHTEN_DRAW;
			signal = g_signal_connect (G_OBJECT (rs->preview_drawingarea),
				"motion_notify_event",
				G_CALLBACK (gui_drawingarea_straighten_motion_callback), rs);
		}
		else if (((event->button==3) && (state == STATE_STRAIGHTEN)) && rs->preview_done)
		{
			gdk_window_set_cursor(rs->preview_drawingarea->window, cur_normal);
			state = STATE_NORMAL;
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
				break;
			case STATE_STRAIGHTEN_DRAW:
				gdk_window_set_cursor(rs->preview_drawingarea->window, cur_normal);
				g_signal_handler_disconnect(rs->preview_drawingarea, signal);
				state = STATE_NORMAL;
				rs->photo->angle += angle;
				update_preview(rs, FALSE, TRUE);
				break;
		}
	}
	return(TRUE);
}

/* hack to resize a bit nicer */
gboolean
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

void
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
	cur_n = gdk_cursor_new(GDK_TOP_SIDE);
	cur_e = gdk_cursor_new(GDK_RIGHT_SIDE);
	cur_s = gdk_cursor_new(GDK_BOTTOM_SIDE);
	cur_w = gdk_cursor_new(GDK_LEFT_SIDE);
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
	g_signal_connect (G_OBJECT (rs->preview_drawingarea), "button_press_event",
		G_CALLBACK (gui_drawingarea_button), rs);
	g_signal_connect (G_OBJECT (rs->preview_drawingarea), "button_release_event",
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
