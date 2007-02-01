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
#include "gettext.h"
#include "rawstudio.h"
#include "gtk-interface.h"
#include "gtk-helper.h"
#include "toolbox.h"
#include "conf_interface.h"
#include "rs-crop.h"

gboolean rs_crop_motion_callback(GtkWidget *widget, GdkEventMotion *event, RS_BLOB *rs);
gboolean rs_crop_button_callback(GtkWidget *widget, GdkEventButton *event, RS_BLOB *rs);
gboolean rs_crop_resize_callback(GtkWidget *widget, GdkEventMotion *event, RS_BLOB *rs);
GtkWidget * rs_crop_tool_widget(RS_BLOB *rs);
static void gui_roi_grid_changed(gpointer active, gpointer user_data);
static void rs_crop_aspect_changed_callback(gpointer active, gpointer user_data);
void rs_crop_tool_widget_update(RS_BLOB *rs);

static gint state;
static gint motion, button_press, button_release;
static gint start_x, start_y;
static RS_RECT crop_screen;
static RS_RECT last = {0,0,0,0}; /* Initialize with more meaningfull values */
static RS_RECT start_roi;
static GtkWidget *frame;
static GtkWidget *roi_size_label_size;
static GString *roi_size_text;
static gdouble aspect_ratio = 0.0;

enum {
	STATE_CROP_MOVE,
	STATE_CROP_MOVE_NW,
	STATE_CROP_MOVE_NE,
	STATE_CROP_MOVE_SE,
	STATE_CROP_MOVE_SW,
	STATE_CROP_MOVE_NEW,
};

GtkWidget *
rs_crop_tool_widget(RS_BLOB *rs)
{
	GtkWidget *vbox;
	GtkWidget *roi_size_hbox;
	GtkWidget *roi_size_label;
	GtkWidget *roi_grid_hbox;
	GtkWidget *roi_grid_label;
	GtkWidget *roi_grid_combobox;
	GtkWidget *aspect_hbox;
	GtkWidget *aspect_label;
	RS_CONFBOX *grid_confbox;
	RS_CONFBOX *aspect_confbox;

	/* aspect MUST be => 1.0 */
	const static gdouble aspect_freeform = 0.0;
	const static gdouble aspect_32 = 3.0/2.0;
	const static gdouble aspect_43 = 4.0/3.0;
	const static gdouble aspect_1008 = 10.0/8.0;
	const static gdouble aspect_1610 = 16.0/10.0;
	const static gdouble aspect_83 = 8.0/3.0;
	const static gdouble aspect_11 = 1.0;
	static gdouble aspect_iso216;
	static gdouble aspect_golden;

	aspect_iso216 = sqrt(2.0);
	aspect_golden = (1.0+sqrt(5.0))/2.0;
	vbox = gtk_vbox_new(FALSE, 4);
	
	roi_size_label = gtk_label_new(_("Size"));
	gtk_misc_set_alignment(GTK_MISC(roi_size_label), 0.0, 0.5);
	roi_size_label_size = gtk_label_new("");
	roi_size_text = g_string_new("");
	roi_size_hbox = gtk_hbox_new(FALSE, 0);
	
	gtk_box_pack_start (GTK_BOX (roi_size_hbox), roi_size_label, TRUE, TRUE, 4);
	gtk_box_pack_start (GTK_BOX (roi_size_hbox), roi_size_label_size, FALSE, TRUE, 4);
	gtk_box_pack_start (GTK_BOX (vbox), roi_size_hbox, FALSE, TRUE, 0);
	
	roi_grid_hbox = gtk_hbox_new(FALSE, 0);
	roi_grid_label = gtk_label_new(_("Grid"));
	gtk_misc_set_alignment(GTK_MISC(roi_grid_label), 0.0, 0.5);

	grid_confbox = gui_confbox_new(CONF_ROI_GRID);
	gui_confbox_set_callback(grid_confbox, rs, gui_roi_grid_changed);
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
	gui_confbox_set_callback(aspect_confbox, rs, rs_crop_aspect_changed_callback);
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

	gtk_box_pack_start (GTK_BOX (vbox), roi_grid_hbox, FALSE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (vbox), aspect_hbox, FALSE, TRUE, 0);

	return vbox;
}

static void
rs_crop_aspect_changed_callback(gpointer active, gpointer user_data)
{
	RS_BLOB *rs = user_data;
	aspect_ratio = *((gdouble *)active);
	/* FIXME: Calculate new ROI */
	update_preview_region(rs, rs->preview_exposed, FALSE);
	return;
}

void
gui_roi_grid_changed(gpointer active, gpointer user_data)
{
	RS_BLOB *rs = user_data;
	rs->roi_grid = GPOINTER_TO_INT(active);
	update_preview_region(rs, rs->preview_exposed, FALSE);
	return;
}

void
rs_crop_tool_widget_update(RS_BLOB *rs)
{	
	g_string_printf(roi_size_text, _("%d x %d"),
		rs->roi.x2-rs->roi.x1+1, rs->roi.y2-rs->roi.y1+1);
	gtk_label_set_text(GTK_LABEL(roi_size_label_size), roi_size_text->str);
}

void
rs_crop_start(RS_BLOB *rs)
{
	GtkWidget *crop_tool_widget;

	if (!rs->photo) return;
	crop_screen.x1 = rs->roi_scaled.x1 = 0;
	crop_screen.y1 = rs->roi_scaled.y1 = 0;
	crop_screen.x2 = rs->roi_scaled.x2 = rs->photo->scaled->w-1;
	crop_screen.y2 = rs->roi_scaled.y2 = rs->photo->scaled->h-1;
	matrix3_affine_transform_point_int(&rs->photo->inverse_affine,
		crop_screen.x1, crop_screen.y1,
		&rs->roi.x1, &rs->roi.y1);
	matrix3_affine_transform_point_int(&rs->photo->inverse_affine,
		crop_screen.x2, crop_screen.y2,
		&rs->roi.x2, &rs->roi.y2);
	rs_mark_roi(rs, TRUE);

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

	crop_tool_widget = rs_crop_tool_widget(rs);
	frame = gui_toolbox_add_tool_frame(crop_tool_widget, _("Crop"));
	rs_crop_tool_widget_update(rs);

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

void
find_aspect(RS_RECT *in, RS_RECT *out, gdouble aspect, gint state)
{
	const gdouble original_w = (gdouble) abs(in->x2 - in->x1 + 1);
	const gdouble original_h = (gdouble) abs(in->y2 - in->y1 + 1);
	gdouble corrected_w, corrected_h;
	gdouble original_aspect = original_w/original_h;

	if (aspect==0.0)
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

	*out = *in; /* initialize out */

	switch(state)
	{
		case STATE_CROP_MOVE_NW: /* x1,y1 */
			out->x1 = out->x2 - ((gint)corrected_w) + 1;
			out->y1 = out->y2 - ((gint)corrected_h) + 1;
			break;
		case STATE_CROP_MOVE_NE: /* x2,y1 */
			out->x2 = out->x1 + ((gint)corrected_w) - 1;
			out->y1 = out->y2 - ((gint)corrected_h) + 1;
			break;
		case STATE_CROP_MOVE_SE: /* x2,y2 */
			out->x2 = out->x1 + ((gint)corrected_w) - 1;
			out->y2 = out->y1 + ((gint)corrected_h) - 1;
			break;
		case STATE_CROP_MOVE_SW: /* x1,y2 */
			out->x1 = out->x2 - ((gint)corrected_w) + 1;
			out->y2 = out->y1 + ((gint)corrected_h) - 1;
			break;
	}

	return;
}

#define NEAR 10 /* how far away (in pixels) to snap */
gboolean
rs_crop_motion_callback(GtkWidget *widget, GdkEventMotion *event, RS_BLOB *rs)
{
	gint x = (gint) event->x;
	gint y = (gint) event->y;
	extern GdkCursor *cur_normal;
	extern GdkCursor *cur_fleur;
	extern GdkCursor *cur_nw;
	extern GdkCursor *cur_ne;
	extern GdkCursor *cur_se;
	extern GdkCursor *cur_sw;

	if (abs(x-rs->roi_scaled.x1)<10) /* west block */
	{
		if (abs(y-rs->roi_scaled.y1)<10)
		{
			state = STATE_CROP_MOVE_NW;
			gdk_window_set_cursor(rs->preview_drawingarea->window, cur_nw);
		}
		else if (abs(y-rs->roi_scaled.y2)<10)
		{
			state = STATE_CROP_MOVE_SW;
			gdk_window_set_cursor(rs->preview_drawingarea->window, cur_sw);
		}
		else if ((y>rs->roi_scaled.y1) && (y<rs->roi_scaled.y2))
		{
			state = STATE_CROP_MOVE;
			gdk_window_set_cursor(rs->preview_drawingarea->window, cur_fleur);
		}
		else
		{
			state = STATE_CROP_MOVE_NEW;
			gdk_window_set_cursor(rs->preview_drawingarea->window, cur_normal);
		}
	}
	else if (abs(x-rs->roi_scaled.x2)<10) /* east block */
	{
		if (abs(y-rs->roi_scaled.y1)<10)
		{
			state = STATE_CROP_MOVE_NE;
			gdk_window_set_cursor(rs->preview_drawingarea->window, cur_ne);
		}
		else if (abs(y-rs->roi_scaled.y2)<10)
		{
			state = STATE_CROP_MOVE_SE;
			gdk_window_set_cursor(rs->preview_drawingarea->window, cur_se);
		}
		else if ((y>rs->roi_scaled.y1) && (y<rs->roi_scaled.y2))
		{
			state = STATE_CROP_MOVE;
			gdk_window_set_cursor(rs->preview_drawingarea->window, cur_fleur);
		}
		else
		{
			state = STATE_CROP_MOVE_NEW;
			gdk_window_set_cursor(rs->preview_drawingarea->window, cur_normal);
		}
	}
	else if ((x>rs->roi_scaled.x1) && (x<rs->roi_scaled.x2)) /* poles */
	{
		if (abs(y-rs->roi_scaled.y1)<10)
		{
			state = STATE_CROP_MOVE;
			gdk_window_set_cursor(rs->preview_drawingarea->window, cur_fleur);
		}
		else if (abs(y-rs->roi_scaled.y2)<10)
		{
			state = STATE_CROP_MOVE;
			gdk_window_set_cursor(rs->preview_drawingarea->window, cur_fleur);
		}
		else
		{
			state = STATE_CROP_MOVE_NEW;
			gdk_window_set_cursor(rs->preview_drawingarea->window, cur_normal);
		}
	}
	else
	{
		state = STATE_CROP_MOVE_NEW;
		gdk_window_set_cursor(rs->preview_drawingarea->window, cur_normal);
	}

	return(FALSE);
}
#undef NEAR

gboolean
rs_crop_resize_callback(GtkWidget *widget, GdkEventMotion *event, RS_BLOB *rs)
{
	static gint last_x=-1000000, last_y=-1000000;
	gint x = (gint) event->x;
	gint y = (gint) event->y;
	RS_RECT region;
	gint realx, realy;
	gint w,h;

	gdk_window_get_pointer(widget->window, &x, &y, NULL);
	if (last_x != -1000000)
		if ((x==last_x) && (y==last_y)) /* Have we actually changed? */
			return(TRUE);
	last_x = x;
	last_y = y;

	gtk_widget_get_size_request(widget, &w, &h);
	if ((x>w) || (y>h) || (x<0) || (y<0))
		return(TRUE);

	if (state == STATE_CROP_MOVE_NEW)
	{
		/* abort if crop is (still) too small */
		if ((abs(start_x-x)<5) || (abs(start_y-y)<5))
			return(TRUE);
		matrix3_affine_transform_point_int(&rs->photo->inverse_affine,
			start_x, start_y, &rs->roi.x1, &rs->roi.y1);
		matrix3_affine_transform_point_int(&rs->photo->inverse_affine,
			x, y, &rs->roi.x2, &rs->roi.y2);

		if(x>start_x)
		{
			if (y>start_y)
				state=STATE_CROP_MOVE_SE;
			else
				state=STATE_CROP_MOVE_NE;
		}
		else
		{
			if (y>start_y)
				state=STATE_CROP_MOVE_SW;
			else
				state=STATE_CROP_MOVE_NW;
		}
	}
	switch(state)
	{
		/* FIXME: Do something if crop "crosses over itself" */
		case STATE_CROP_MOVE_NW:
			matrix3_affine_transform_point_int(&rs->photo->inverse_affine,
				x, y, &rs->roi.x1, &rs->roi.y1);
			break;
		case STATE_CROP_MOVE_NE:
			matrix3_affine_transform_point_int(&rs->photo->inverse_affine,
				x, y, &rs->roi.x2, &rs->roi.y1);
			break;
		case STATE_CROP_MOVE_SE:
			matrix3_affine_transform_point_int(&rs->photo->inverse_affine,
				x, y, &rs->roi.x2, &rs->roi.y2);
			break;
		case STATE_CROP_MOVE_SW:
			matrix3_affine_transform_point_int(&rs->photo->inverse_affine,
				x, y, &rs->roi.x1, &rs->roi.y2);
			break;
		case STATE_CROP_MOVE:
			/* FIXME: This doesn't work when cropping an already cropped image */
			/* calculate delta */
			x -= start_x;
			y -= start_y;
			/* FIXME: it's a miracle if this keeps working */
			realx = (gint) (rs->photo->inverse_affine.coeff[0][0] * ((gdouble) x));
			realy = (gint) (rs->photo->inverse_affine.coeff[1][1] * ((gdouble) y));
			w = (gint) (rs->photo->inverse_affine.coeff[0][0] * ((gdouble) rs->photo->scaled->w));
			h = (gint) (rs->photo->inverse_affine.coeff[1][1] * ((gdouble) rs->photo->scaled->h));
			/* check borders */
			if ((start_roi.x1+realx) < 0)
				realx = 0-start_roi.x1;
			if ((start_roi.y1+realy) < 0)
				realy = 0-start_roi.y1;
			if (((start_roi.x2+realx) > w))
				realx = w-start_roi.x2;
			if (((start_roi.y2+realy) > h))
				realy = h-start_roi.y2;
			/* apply delta */
			rs->roi.x1 = start_roi.x1+realx;
			rs->roi.x2 = start_roi.x2+realx;
			rs->roi.y1 = start_roi.y1+realy;
			rs->roi.y2 = start_roi.y2+realy;
			break;
	}

	find_aspect(&rs->roi, &rs->roi, aspect_ratio, state);

	rs_rect_normalize(&rs->roi, &rs->roi);

	matrix3_affine_transform_point_int(&rs->photo->affine,
		rs->roi.x1, rs->roi.y1,
		&rs->roi_scaled.x1, &rs->roi_scaled.y1);
	matrix3_affine_transform_point_int(&rs->photo->affine,
		rs->roi.x2, rs->roi.y2,
		&rs->roi_scaled.x2, &rs->roi_scaled.y2);
	region.x1 = (rs->roi_scaled.x1 < last.x1) ? rs->roi_scaled.x1 : last.x1;
	region.x2 = (rs->roi_scaled.x2 > last.x2) ? rs->roi_scaled.x2 : last.x2;
	region.y1 = (rs->roi_scaled.y1 < last.y1) ? rs->roi_scaled.y1 : last.y1;
	region.y2 = (rs->roi_scaled.y2 > last.y2) ? rs->roi_scaled.y2 : last.y2;
	last.x1 = rs->roi_scaled.x1;
	last.x2 = rs->roi_scaled.x2;
	last.y1 = rs->roi_scaled.y1;
	last.y2 = rs->roi_scaled.y2;

	update_preview_region(rs, &region, FALSE);

	rs_crop_tool_widget_update(rs);

	return(FALSE);
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
		/* block calls to motion callback while button is pressed */
		g_signal_handlers_block_by_func(rs->preview_drawingarea, rs_crop_motion_callback, rs);
		if (((event->button==1)) /*&& (state == STATE_CROP))*/ && rs->preview_done)
		{
			start_x = x;
			start_y = y;
			start_roi = rs->roi;
			signal = g_signal_connect (G_OBJECT (rs->preview_drawingarea),
				"motion_notify_event",
				G_CALLBACK (rs_crop_resize_callback), rs);
			last = *rs->preview_exposed;
			return(TRUE);
		}
		else if (event->button==3)
		{
			if (((x>rs->roi_scaled.x1) && (x<rs->roi_scaled.x2)) && ((y>rs->roi_scaled.y1) && (y<rs->roi_scaled.y2)))
			{
				if (((rs->roi.x2-rs->roi.x1)>2) && ((rs->roi.y2-rs->roi.y1)>2))
				{
					if (!rs->photo->crop)
						rs->photo->crop = (RS_RECT *) g_malloc(sizeof(RS_RECT));
					rs->photo->crop->x1 = rs->roi.x1;
					rs->photo->crop->y1 = rs->roi.y1;
					rs->photo->crop->x2 = rs->roi.x2;
					rs->photo->crop->y2 = rs->roi.y2;
				}
			}
			rs_mark_roi(rs, FALSE);
			g_signal_handler_disconnect(rs->preview_drawingarea, motion);
			g_signal_handler_disconnect(rs->preview_drawingarea, button_press);
			g_signal_handler_disconnect(rs->preview_drawingarea, button_release);
			update_preview(rs, FALSE, TRUE);
			gdk_window_set_cursor(rs->preview_drawingarea->window, cur_normal);
			gtk_widget_destroy(frame);
			g_string_free(roi_size_text, TRUE);
			return(TRUE);
		}
	}
	else /* release */
	{
		g_signal_handlers_unblock_by_func(rs->preview_drawingarea, rs_crop_motion_callback, rs);
		switch(state)
		{
			case STATE_CROP_MOVE_NEW:
			case STATE_CROP_MOVE:
			case STATE_CROP_MOVE_NW:
			case STATE_CROP_MOVE_NE:
			case STATE_CROP_MOVE_SE:
			case STATE_CROP_MOVE_SW:
				g_signal_handler_disconnect(rs->preview_drawingarea, signal);
				update_preview_callback(NULL, rs);
				return(TRUE);
				break;
		}
	}
	return(FALSE);
}
