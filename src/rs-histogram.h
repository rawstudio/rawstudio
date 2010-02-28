/*
 * * Copyright (C) 2006-2010 Anders Brander <anders@brander.dk>, 
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

#ifndef RS_HISTOGRAM_WIDGET_H
#define RS_HISTOGRAM_WIDGET_H

#include <gtk/gtk.h>
#include "application.h"

typedef struct _RSHistogramWidget            RSHistogramWidget;
typedef struct _RSHistogramWidgetClass       RSHistogramWidgetClass;

extern GType rs_histogram_widget_get_type (void);

/**
 * Creates a new RSHistogramWidget
 */
extern GtkWidget *rs_histogram_new();

/**
 * Set an image to base the histogram of
 * @param histogram A RSHistogramWidget
 * @param image An image
 * @param display_color_space Colorspace to use to transform the input.
 */
extern void rs_histogram_set_input(RSHistogramWidget *histogram, RSFilter* input, RSColorSpace *display_color_space);


/**
 * Redraw a RSHistogramWidget
 * @param histogram A RSHistogramWidget
 */
extern void rs_histogram_redraw(RSHistogramWidget *histogram);

#define RS_HISTOGRAM_TYPE_WIDGET             (rs_histogram_widget_get_type ())
#define RS_HISTOGRAM_WIDGET(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), RS_HISTOGRAM_TYPE_WIDGET, RSHistogramWidget))
#define RS_HISTOGRAM_WIDGET_CLASS(obj)       (G_TYPE_CHECK_CLASS_CAST ((obj), RS_HISTOGRAM_WIDGET, RSHistogramWidgetClass))
#define RS_IS_HISTOGRAM_WIDGET(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), RS_HISTOGRAM_TYPE_WIDGET))
#define RS_IS_HISTOGRAM_WIDGET_CLASS(obj)    (G_TYPE_CHECK_CLASS_TYPE ((obj), RS_HISTOGRAM_TYPE_WIDGET))
#define RS_HISTOGRAM_WIDGET_GET_CLASS        (G_TYPE_INSTANCE_GET_CLASS ((obj), RS_HISTOGRAM_TYPE_WIDGET, RSHistogramWidgetClass))

#endif /* RS_HISTOGRAM_WIDGET_H */
