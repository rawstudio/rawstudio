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
#include "rs-histogram.h"
#include "rs-color-transform.h"

struct _RSHistogramWidget
{
	GtkDrawingArea parent;
	gint width;
	gint height;
	guchar *buffer;
	RS_IMAGE16 *image;
	RS_COLOR_TRANSFORM *rct;
	guint input_samples[3][256];
	guint *output_samples[3];
};

struct _RSHistogramWidgetClass
{
	GtkDrawingAreaClass parent_class;
};

/* Define the boiler plate stuff using the predefined macro */
G_DEFINE_TYPE (RSHistogramWidget, rs_histogram_widget, GTK_TYPE_DRAWING_AREA);

static void size_allocate(GtkWidget *widget, GtkAllocation *allocation, gpointer user_data);
static gboolean expose(GtkWidget *widget, GdkEventExpose *event);

/**
 * Class initializer
 */
static void
rs_histogram_widget_class_init(RSHistogramWidgetClass *klass)
{
	GtkWidgetClass *widget_class;
	GtkObjectClass *object_class;
	widget_class = GTK_WIDGET_CLASS(klass);
	object_class = GTK_OBJECT_CLASS(klass);
	widget_class->expose_event = expose;
}

/**
 * Instance initialization
 */
static void
rs_histogram_widget_init(RSHistogramWidget *hist)
{
	hist->output_samples[0] = NULL;
	hist->output_samples[1] = NULL;
	hist->output_samples[2] = NULL;
	hist->image = NULL;
	hist->rct = NULL;
	hist->buffer = NULL;

	g_signal_connect(G_OBJECT(hist), "size-allocate", G_CALLBACK(size_allocate), NULL);
}

static void
size_allocate(GtkWidget *widget, GtkAllocation *allocation, gpointer user_data)
{
	gint c;

	RSHistogramWidget *histogram = RS_HISTOGRAM_WIDGET(widget);

	histogram->width = allocation->width;
	histogram->height = allocation->height;

	/* Free the samples array if needed */
	for (c=0;c<3;c++)
	{
		if (histogram->output_samples[c])
			g_free(histogram->output_samples[c]);
		histogram->output_samples[c] = NULL;
	}

	/* Free buffer if needed */
	if (histogram->buffer)
		g_free(histogram->buffer);
	histogram->buffer = NULL;
}

static gboolean
expose(GtkWidget *widget, GdkEventExpose *event)
{
	rs_histogram_redraw(RS_HISTOGRAM_WIDGET(widget));

	return FALSE;
}

/**
 * Creates a new RSHistogramWidget
 */
GtkWidget *
rs_histogram_new(void)
{
	return g_object_new (RS_HISTOGRAM_TYPE_WIDGET, NULL);
}

/**
 * Set an image to base the histogram from
 * @param histogram A RSHistogramWidget
 * @param image An image
 */
void
rs_histogram_set_image(RSHistogramWidget *histogram, RS_IMAGE16 *image)
{
	g_return_if_fail (RS_IS_HISTOGRAM_WIDGET(histogram));
	g_return_if_fail (image);

	histogram->image = image;

	rs_histogram_redraw(histogram);
}

/**
 * Set color transform to be used when rendering histogram
 * @param histogram A RSHistogramWidget
 * @param rct A RS_COLOR_TRANSFORM
 */
void
rs_histogram_set_color_transform(RSHistogramWidget *histogram, RS_COLOR_TRANSFORM *rct)
{
	g_return_if_fail (RS_IS_HISTOGRAM_WIDGET(histogram));
	g_return_if_fail (rct);

	histogram->rct = rct;

	rs_histogram_redraw(histogram);
}

/**
 * Redraw a RSHistogramWidget
 * @param histogram A RSHistogramWidget
 */
void
rs_histogram_redraw(RSHistogramWidget *histogram)
{
	gint c, x, y;
	guint max;
	GdkDrawable *window;
	GtkWidget *widget;
	GdkGC *gc;

	g_return_if_fail (RS_IS_HISTOGRAM_WIDGET(histogram));

	widget = GTK_WIDGET(histogram);
	window = GDK_DRAWABLE(widget->window);
	gc = gdk_gc_new(window);

	/* Allocate new buffer if needed */
	if (histogram->buffer == NULL)
		histogram->buffer = g_new(guchar, histogram->width * histogram->height * 3);

	/* Reset background to a nice grey */
	memset(histogram->buffer, 0x99, histogram->width*histogram->height*3);

	/* Draw vertical lines */
	gint dist = (gint) ((gfloat)histogram->width / 4.0f);
	for(y=0;y<histogram->height;y++)
	{
		for(x=dist;x<histogram->width;x+=dist)
		{
			histogram->buffer[(y*histogram->width+x)*3] = 0x77;
			histogram->buffer[(y*histogram->width+x)*3+1] = 0x77;
			histogram->buffer[(y*histogram->width+x)*3+2] = 0x77;
		}
	}
	/* Draw histogram if we got everything needed */
	if (histogram->rct && histogram->image && (GTK_WIDGET_VISIBLE(widget)))
	{
		/* Sample some data */
		rs_color_transform_make_histogram(histogram->rct, histogram->image, histogram->input_samples);

		/* Interpolate data for correct width and find maximum value */
		max = 0;
		for (c=0;c<3;c++)
			histogram->output_samples[c] = interpolate_dataset_int(
				&histogram->input_samples[c][1], 253,
				histogram->output_samples[c], histogram->width,
				&max);

		/* Find the scaling factor */
		gfloat factor = (gfloat)(max+histogram->height)/(gfloat)histogram->height;

		/* Draw everything */
		for (x = 0; x < histogram->width; x++)
			for (c = 0; c < 3; c++)
				for (y = 0; y < (histogram->output_samples[c][x]/factor); y++)
					(histogram->buffer + ((histogram->height-1)-y) * histogram->width*3 + x * 3)[c] =0xFF;

		/* draw under/over-exposed indicators */
		for (c = 0; c < 3; c++)
		{
			if (histogram->input_samples[c][0] > 100)
				for(y = 0; y < 10; y++)
					for(x = 0; x < (10-y); x++)
							histogram->buffer[y*histogram->width*3 + x*3+c] = 0xFF;

			if (histogram->input_samples[c][255] > 100)
				for(y = 0; y < 10; y++)
					for(x = (histogram->width-10+y); x < histogram->width; x++)
						histogram->buffer[y*histogram->width*3 + x*3+c] = 0xFF;
		}
	}

	/* Blit to screen */
	gdk_draw_rgb_image(window, gc,
		0, 0, histogram->width, histogram->height, GDK_RGB_DITHER_NONE,
		histogram->buffer, histogram->width*3);

	g_object_unref(gc);
}
