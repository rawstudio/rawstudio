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
 * Set an image to base the histogram from
 * @param histogram A RSHistogramWidget
 * @param image An image
 */
extern void rs_histogram_set_image(RSHistogramWidget *histogram, RS_IMAGE16 *image);

/**
 * Set a RSSettings to use
 * @param histogram A RSHistogramWidget
 * @param settings A RSSettings object to use
 */
extern void rs_histogram_set_settings(RSHistogramWidget *histogram, RSSettings *settings);

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
