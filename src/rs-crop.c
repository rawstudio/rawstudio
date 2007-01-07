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
static GtkWidget *frame;
static GtkWidget *roi_size_label_size;
static GString *roi_size_text;
static gdouble aspect_ratio = 0.0;

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

	const static gdouble aspect_freeform = 0.0;
	const static gdouble aspect_32 = 3.0/2.0;
	const static gdouble aspect_43 = 3.0/3.0;
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
		rs->roi.x2-rs->roi.x1, rs->roi.y2-rs->roi.y1);
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

#define DIST(x1,y1,x2,y2) sqrt(((x1)-(x2))*((x1)-(x2)) + ((y1)-(y2))*((y1)-(y2)))
void
calc_aspect(RS_RECT *in, gdouble aspect, gint *x, gint *y)
{
	const gdouble x1 = (gdouble) in->x1;
	const gdouble y1 = (gdouble) in->y1;
	const gdouble x2 = (gdouble) in->x2;
	const gdouble y2 = (gdouble) in->y2;
	gdouble w_lock_dist, h_lock_dist;

	w_lock_dist = DIST(*x, *y, x2, y1+((x2 - x1) / aspect));
	h_lock_dist = DIST(*x, *y, x1+((y2 - y1) / aspect), y2);

	if (h_lock_dist < w_lock_dist)
		*x = (gint) (x1+(y2 - y1) /aspect);
	else
		*y = (gint) (y1+(x2 - x1) / aspect);
	return;
}

#define CORNER_NW 1
#define CORNER_NE 2
#define CORNER_SE 3
#define CORNER_SW 4

void
find_aspect(RS_RECT *in, RS_RECT *out, gint *x, gint *y, gdouble aspect, gint corner)
{
	const gdouble original_w = (gdouble) abs(in->x2 - in->x1);
	const gdouble original_h = (gdouble) abs(in->y2 - in->y1);
	gdouble corrected_w, corrected_h;
	gdouble h_lock_dist=10000.0, w_lock_dist=10000.0;
	gint *target_x = &out->x1, *target_y = &out->y1;
	gint value_x=0, value_y=0;

	*out = *in; /* initialize out */

	if (aspect==0.0) /* freeform - and 0-division :) */
		return;

	corrected_w = original_h / aspect;
	corrected_h = original_w / aspect;
	switch(corner)
	{
		case CORNER_NW: /* x1,y1 */
			h_lock_dist = DIST(*x, *y, in->x2-corrected_w, in->y2-original_h);
			value_x = in->x2 - corrected_w;

			w_lock_dist = DIST(*x, *y, in->x2-original_w, in->y2-corrected_h);
			value_y = in->y2 - corrected_h;

			target_x = &out->x1;
			target_y = &out->y1;
			break;
		case CORNER_NE: /* x2,y1 */
			h_lock_dist = DIST(*x, *y, in->x1+corrected_w, in->y2-original_h);
			value_x = in->x1 + corrected_w;

			w_lock_dist = DIST(*x, *y, in->x1+original_w, in->y2-corrected_h);
			value_y = in->y2 - corrected_h;

			target_x = &out->x2;
			target_y = &out->y1;
			break;
		case CORNER_SE: /* x2,y2 */
			h_lock_dist = DIST(*x, *y, in->x1+corrected_w, in->y1+original_h);
			value_x = in->x1 + corrected_w;

			w_lock_dist = DIST(*x, *y, in->x1+original_w, in->y1+corrected_h);
			value_y = in->y1 + corrected_h;

			target_x = &out->x2;
			target_y = &out->y2;
			break;
		case CORNER_SW: /* x1,y2 */
			h_lock_dist = DIST(*x, *y, in->x2-corrected_w, in->y1+original_h);
			value_x = in->x2 - corrected_w;

			w_lock_dist = DIST(*x, *y, in->x2-original_w, in->y1+corrected_h);
			value_y = in->y1 + corrected_h;

			target_x = &out->x1;
			target_y = &out->y2;
			break;
	}

	if (h_lock_dist < w_lock_dist)
		*target_x = value_x;
	else
		*target_y = value_y;

	return;
}
#undef DIST

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
	gint corner = 0;

	switch (state)
	{
		case STATE_CROP_MOVE_NEW:
			crop_screen.x1 = start_x;
			crop_screen.x2 = start_x;
			crop_screen.y1 = start_y;
			crop_screen.y2 = start_y;
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
 			crop_screen.y1 = y;
			corner = CORNER_NE;
 			break;
 		case STATE_CROP_MOVE_E:
 			crop_screen.x2 = x;
			corner = CORNER_SE;
 			break;
 		case STATE_CROP_MOVE_S:
 			crop_screen.y2 = y;
			corner = CORNER_SW;
 			break;
 		case STATE_CROP_MOVE_W:
 			crop_screen.x1 = x;
			corner = CORNER_NW;
 			break;
 		case STATE_CROP_MOVE_NW:
 			crop_screen.x1 = x;
 			crop_screen.y1 = y;
			corner = CORNER_NW;
 			break;
 		case STATE_CROP_MOVE_NE:
 			crop_screen.x2 = x;
 			crop_screen.y1 = y;
			corner = CORNER_NE;
 			break;
 		case STATE_CROP_MOVE_SE:
 			crop_screen.x2 = x;
 			crop_screen.y2 = y;
			corner = CORNER_SE;
 			break;
 		case STATE_CROP_MOVE_SW:
 			crop_screen.x1 = x;
 			crop_screen.y2 = y;
			corner = CORNER_SW;
 			break;
	}
	if (crop_screen.x1 < 0)
		crop_screen.x1 = 0;
	if (crop_screen.x2 < 0)
		crop_screen.x2 = 0;
	if (crop_screen.y1 < 0)
		crop_screen.y1 = 0;
	if (crop_screen.y2 < 0)
		crop_screen.y2 = 0;

	if (crop_screen.x1 > rs->photo->preview->w)
		crop_screen.x1 = rs->photo->preview->w-1;
	if (crop_screen.x2 > rs->photo->preview->w)
		crop_screen.x2 = rs->photo->preview->w-1;
	if (crop_screen.y1 > rs->photo->preview->h)
		crop_screen.y1 = rs->photo->preview->h-1;
	if (crop_screen.y2 > rs->photo->preview->h)
		crop_screen.y2 = rs->photo->preview->h-1;

	if (crop_screen.x1 > crop_screen.x2)
	{
		SWAP(crop_screen.x1, crop_screen.x2);
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
	if (crop_screen.y1 > crop_screen.y2)
	{
		SWAP(crop_screen.y1, crop_screen.y2);
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

	matrix3_affine_transform_point_int(&rs->photo->inverse_affine,
		crop_screen.x1, crop_screen.y1,
		&rs->roi.x1, &rs->roi.y1);
	matrix3_affine_transform_point_int(&rs->photo->inverse_affine,
		crop_screen.x2, crop_screen.y2,
		&rs->roi.x2, &rs->roi.y2);

	matrix3_affine_transform_point_int(&rs->photo->inverse_affine,
		x, y, &x, &y);
	find_aspect(&rs->roi, &rs->roi, &x, &y, aspect_ratio, corner);

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

	return(TRUE);
}
#undef CORNER_NW
#undef CORNER_NE
#undef CORNER_SE
#undef CORNER_SW

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
				crop_screen = rs->roi_scaled;
				state = STATE_CROP;
				return(TRUE);
				break;
		}
	}
	return(FALSE);
}
