/*****************************************************************************
 * Curve widget
 * 
 * Copyright (C) 2007 Edouard Gomez <ed.gomez@free.fr>
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
 ****************************************************************************/

#ifndef _RS_CURVE_H_
#define _RS_CURVE_H_

#include <gtk/gtk.h>

/* Declared in rs-curve.c */
typedef struct _RSCurveWidget            RSCurveWidget;
typedef struct _RSCurveWidgetClass       RSCurveWidgetClass;

GType
rs_curve_widget_get_type (void);

/**
 * Creates a new RSCurveWidget
 * @return A new RSCurveWidget
 */
extern GtkWidget *
rs_curve_widget_new(void);

/**
 * Sets sample array for a RSCurveWidget, this array will be updates whenever the curve changes
 * @param curve A RSCurveWidget
 * @param array An array of gfloats to be updated or NULL to unset
 * @params array_length: Length of array or 0 to unset
 */
extern void
rs_curve_widget_set_array(RSCurveWidget *curve, gfloat *array, guint array_length);

/**
 * Draw a histogram in the background of the widget
 * @param curve A RSCurveWidget
 * @param image A image to sample from
 * @param setting Settings to use, curve and saturation will be ignored
 */
extern void
rs_curve_draw_histogram(RSCurveWidget *curve);

/**
 * Add a knot to a curve widget
 * @param widget A RSCurveWidget
 * @param x X coordinate
 * @param y Y coordinate
 */
extern void
rs_curve_widget_add_knot(RSCurveWidget *curve, gfloat x, gfloat y);

/**
 * Move a knot of a RSCurveWidget
 * @param curve A RSCurveWidget
 * @param knot Knot to move or -1 for last
 * @param x X coordinate
 * @param y Y coordinate
 */
extern void
rs_curve_widget_move_knot(RSCurveWidget *curve, gint knot, gfloat x, gfloat y);

/**
 * Get samples from curve
 * @param widget A RSCurveWidget
 * @param samples Pointer to output array or NULL
 * @param nbsamples number of samples
 * @return An array of floats, should be freed
 */
gfloat *
rs_curve_widget_sample(RSCurveWidget *curve, gfloat *samples, guint nbsamples);

/**
 * Set knots of a RSCurveWidget
 * @param curve A RSCurveWidget
 * @param knots An array of knots (two values/knot)
 * @param nknots Number of knots
 */
extern void
rs_curve_widget_set_knots(RSCurveWidget *curve, const gfloat *knots, const guint nknots);

/**
 * Get knots from a RSCurveWidget
 * @param curve A RSCurveWidget
 * @param knots An array of knots (two values/knot) (out)
 * @param nknots Number of knots written (out)
 */
extern void
rs_curve_widget_get_knots(RSCurveWidget *curve, gfloat **knots, guint *nknots);

/**
 * Resets a RSCurveWidget
 * @param curve A RSCurveWidget
 */
extern void
rs_curve_widget_reset(RSCurveWidget *curve);

/**
 * Saves a RSCurveWidgets knots to a XML-file.
 * @param curve A RSCurveWidget
 * @param filename The filename to save to
 * @return TRUE if succeded, FALSE otherwise
 */
extern gboolean
rs_curve_widget_save(RSCurveWidget *curve, const gchar *filename);

/**
 * Loads knots to a RSCurveWidgets from a XML-file.
 * @param curve A RSCurveWidget
 * @param filename The filename load from
 * @return TRUE if succeded, FALSE otherwise
 */
extern gboolean
rs_curve_widget_load(RSCurveWidget *curve, const gchar *filename);

/**
 * Set the input to base the histogram drawing from
 * @param curve A RSCurveWidget
 * @param image An image
 * @param display_color_space Colorspace to use to transform the input.
 */
extern void
rs_curve_set_input(RSCurveWidget *curve, RSFilter* input, RSColorSpace *display_color_space);

/**
 * Sets the current RGB data to be marked in histogram view based on currently set colorspace
 * @param curve A RSCurveWidget
 * @param rgb_values An array of length 3, that contain the current RGB value. Pass NULL to disable view.
 */
extern void 
rs_curve_set_highlight(RSCurveWidget *curve, const guchar* rgb_values);

/**
 * Sets the current histogram data
 * @param curve A RSCurveWidget
 * @param input An array of 256 entries containing the current histogram
 */
extern void
rs_curve_set_histogram_data(RSCurveWidget *curve, const gint *input);

extern void
rs_curve_auto_adjust_ends(GtkWidget *widget);

#define RS_CURVE_TYPE_WIDGET             (rs_curve_widget_get_type ())
#define RS_CURVE_WIDGET(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), RS_CURVE_TYPE_WIDGET, RSCurveWidget))
#define RS_CURVE_WIDGET_CLASS(obj)       (G_TYPE_CHECK_CLASS_CAST ((obj), RS_CURVE_WIDGET, RSCurveWidgetClass))
#define RS_IS_CURVE_WIDGET(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), RS_CURVE_TYPE_WIDGET))
#define RS_IS_CURVE_WIDGET_CLASS(obj)    (G_TYPE_CHECK_CLASS_TYPE ((obj), RS_CURVE_TYPE_WIDGET))
#define RS_CURVE_WIDGET_GET_CLASS        (G_TYPE_INSTANCE_GET_CLASS ((obj), RS_CURVE_TYPE_WIDGET, RSCurveWidgetClass))

#endif /* _RS_CURVE_H_ */
