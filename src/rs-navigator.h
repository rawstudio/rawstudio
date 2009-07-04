/*
 * Copyright (C) 2006-2009 Anders Brander <anders@brander.dk> and 
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

#ifndef RS_NAVIGATOR_H
#define RS_NAVIGATOR_H

#include <rawstudio.h>
#include <glib-object.h>

G_BEGIN_DECLS

#define RS_TYPE_NAVIGATOR rs_navigator_get_type()
#define RS_NAVIGATOR(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), RS_TYPE_NAVIGATOR, RSNavigator))
#define RS_NAVIGATOR_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), RS_TYPE_NAVIGATOR, RSNavigatorClass))
#define RS_IS_NAVIGATOR(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), RS_TYPE_NAVIGATOR))
#define RS_IS_NAVIGATOR_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), RS_TYPE_NAVIGATOR))
#define RS_NAVIGATOR_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), RS_TYPE_NAVIGATOR, RSNavigatorClass))

typedef struct {
	GtkDrawingArea parent;

	gint widget_width;
	gint widget_height;

	gint last_x;
	gint last_y;

	GtkAdjustment *vadjustment;
	GtkAdjustment *hadjustment;

	gulong vadjustment_signal1;
	gulong hadjustment_signal1;
	gulong vadjustment_signal2;
	gulong hadjustment_signal2;

	gdouble scale;

	RSFilter *cache;

	gint width;
	gint height;
	gint x;
	gint y;
	gint x_page;
	gint y_page;
} RSNavigator;

typedef struct {
	GtkDrawingAreaClass parent_class;
} RSNavigatorClass;

GType rs_navigator_get_type(void);

RSNavigator *rs_navigator_new(void);

void rs_navigator_set_adjustments(RSNavigator *navigator, GtkAdjustment *vadjustment, GtkAdjustment *hadjustment);
void rs_navigator_set_source_filter(RSNavigator *navigator, RSFilter *source_filter);

G_END_DECLS

#endif /* RS_NAVIGATOR_H */
