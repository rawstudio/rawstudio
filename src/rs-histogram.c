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

#include <rawstudio.h>
#include <gtk/gtk.h>
#include <string.h> /* memset() */
#include "rs-histogram.h"

/* FIXME: Do some cleanup in finalize! */

struct _RSHistogramWidget
{
	GtkDrawingArea parent;
	gint width;
	gint height;
	GdkPixmap *blitter;
	RSFilter *input;
	RSSettings *settings;
	guint input_samples[4][256];
	guint *output_samples[4];
	gfloat rgb_values[3];
	RSColorSpace *display_color_space;
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
	hist->output_samples[3] = NULL;
	hist->input = NULL;
	hist->settings = NULL;
	hist->blitter = NULL;
	hist->rgb_values[0] = -1;
	hist->rgb_values[1] = -1;
	hist->rgb_values[2] = -1;

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
	for (c=0;c<4;c++)
	{
		if (histogram->output_samples[c])
			g_free(histogram->output_samples[c]);
		histogram->output_samples[c] = NULL;
	}

	/* Free blitter if needed */
	if (histogram->blitter)
	{
		g_object_unref(histogram->blitter);
		histogram->blitter = NULL;
	}
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
 * Set an image to base the histogram of
 * @param histogram A RSHistogramWidget
 * @param input An input RSFilter
 */
void
rs_histogram_set_input(RSHistogramWidget* histogram, RSFilter* input, RSColorSpace* display_color_space)
{
	g_return_if_fail (RS_IS_HISTOGRAM_WIDGET(histogram));
	g_return_if_fail (RS_IS_FILTER(input));

	histogram->input = input;
	histogram->display_color_space = display_color_space;

	rs_histogram_redraw(histogram);
}

void
rs_histogram_set_highlight(RSHistogramWidget *histogram, const guchar* rgb_values)
{
	g_return_if_fail (RS_IS_HISTOGRAM_WIDGET(histogram));
	if (rgb_values)
	{
		histogram->rgb_values[0] = (float)rgb_values[0]/255.0f;
		histogram->rgb_values[1] = (float)rgb_values[1]/255.0f;
		histogram->rgb_values[2] = (float)rgb_values[2]/255.0f;
	} 
	else
	{
		histogram->rgb_values[0] = -1;
		histogram->rgb_values[1] = -1;
		histogram->rgb_values[2] = -1;
	}
	rs_histogram_redraw(histogram);
}

#define LUM_PRECISION 15
#define LUM_FIXED(a) ((guint)((a)*(1<<LUM_PRECISION)))
#define RLUMF LUM_FIXED(0.212671f)
#define GLUMF LUM_FIXED(0.715160f)
#define BLUMF LUM_FIXED(0.072169f)
#define HALFF LUM_FIXED(0.5f)

static void 
calculate_histogram(RSHistogramWidget *histogram)
{
	gint x, y;
	
	guint *hist = &histogram->input_samples[0][0];
	/* Reset table */
	memset(hist, 0x00, sizeof(guint)*4*256);

	if (!histogram->input)
		return;

	RSFilterRequest *request = rs_filter_request_new();
	rs_filter_request_set_quick(RS_FILTER_REQUEST(request), TRUE);
	rs_filter_param_set_object(RS_FILTER_PARAM(request), "colorspace", histogram->display_color_space);
		
	RSFilterResponse *response = rs_filter_get_image8(histogram->input, request);
	g_object_unref(request);

	GdkPixbuf *pixbuf = rs_filter_response_get_image8(response);
	if (!pixbuf)
		return;

	const gint pix_width = gdk_pixbuf_get_n_channels(pixbuf);
	const gint w = gdk_pixbuf_get_width(pixbuf);
	const gint h = gdk_pixbuf_get_height(pixbuf);
	for(y = 0; y < h; y++) 
	{
		guchar *i = GET_PIXBUF_PIXEL(pixbuf, 0, y);

		for(x = 0; x < w ; x++)
		{
			guchar r = i[R];
			guchar g = i[G];
			guchar b = i[B];
			hist[r]++;
			hist[g+256]++;
			hist[b+512]++;
			guchar luma = (guchar)((RLUMF * (int)r + GLUMF * (int)g + BLUMF * (int)b + HALFF) >> LUM_PRECISION);
			hist[luma+768]++;
			i += pix_width;
		}
	}
	g_object_unref(pixbuf);
	g_object_unref(response);
}

/**
 * Redraw a RSHistogramWidget
 * @param histogram A RSHistogramWidget
 */
void
rs_histogram_redraw(RSHistogramWidget *histogram)
{
	gint c, x;
	guint max;
	GdkDrawable *window;
	GtkWidget *widget;
	GdkGC *gc;
	gint current[4];

	current[0] = (int)(histogram->rgb_values[0] * histogram->width);
	current[1] = (int)(histogram->rgb_values[1] * histogram->width);
	current[2] = (int)(histogram->rgb_values[2] * histogram->width);
	gfloat lum = 0.212671f * histogram->rgb_values[0] + 0.715160f * histogram->rgb_values[1] + 0.072169f * histogram->rgb_values[2];
	current[3] = (int)(lum*histogram->width);
	g_return_if_fail (RS_IS_HISTOGRAM_WIDGET(histogram));

	widget = GTK_WIDGET(histogram);
	/* Draw histogram if we got everything needed */
	if (histogram->input && GTK_WIDGET_VISIBLE(widget) && GTK_WIDGET_REALIZED(widget))
	{
		const static GdkColor bg = {0, 0x9900, 0x9900, 0x9900};
		const static GdkColor lines = {0, 0x7700, 0x7700, 0x7700};

		window = GDK_DRAWABLE(widget->window);
		gc = gdk_gc_new(window);

		/* Allocate new buffer if needed */
		if (histogram->blitter == NULL)
			histogram->blitter = gdk_pixmap_new(window, histogram->width, histogram->height, -1);

		/* Reset background to a nice grey */
		gdk_gc_set_rgb_fg_color(gc, &bg);
		gdk_draw_rectangle(histogram->blitter, gc, TRUE, 0, 0, histogram->width, histogram->height);

		/* Draw vertical lines */
		gdk_gc_set_rgb_fg_color(gc, &lines);
		gdk_draw_line(histogram->blitter, gc, histogram->width*0.25, 0, histogram->width*0.25, histogram->height-1);
		gdk_draw_line(histogram->blitter, gc, histogram->width*0.5, 0, histogram->width*0.5, histogram->height-1);
		gdk_draw_line(histogram->blitter, gc, histogram->width*0.75, 0, histogram->width*0.75, histogram->height-1);

		/* Sample some data */
		calculate_histogram(histogram);

		/* Interpolate data for correct width and find maximum value */
		max = 0;
		for (c=0;c<4;c++)
			histogram->output_samples[c] = interpolate_dataset_int(
				&histogram->input_samples[c][1], 253,
				histogram->output_samples[c], histogram->width,
				&max);

		/* Find the scaling factor */
		gfloat factor = (gfloat)(max+histogram->height)/(gfloat)histogram->height;

#if GTK_CHECK_VERSION(2,8,0)
		cairo_t *cr;

		/* We will use Cairo for this if possible */
		cr = gdk_cairo_create (histogram->blitter);

		/* Line width */
		cairo_set_line_width (cr, 2.0);

		/* Red */
		cairo_set_source_rgba(cr, 1.0, 0.2, 0.2, 1.0);
		/* Start at first column */
		cairo_move_to (cr, 0, (histogram->height-1)-histogram->output_samples[0][0]/factor);
		/* Walk through columns */
		for (x = 1; x < histogram->width; x++)
			cairo_line_to(cr, x, (histogram->height-1)-histogram->output_samples[0][x]/factor);
		/* Draw the line */
		cairo_stroke (cr);

		/* Underexposed */
		cairo_set_source_rgba(cr, 1.0, 0.2, 0.2, histogram->input_samples[0][0]/100.0);
		cairo_arc(cr, 8.0, 8.0, 3.0, 0.0, 2*G_PI);
		cairo_fill(cr);

		/* Overexposed */
		cairo_set_source_rgba(cr, 1.0, 0.2, 0.2, histogram->input_samples[0][255]/100.0);
		cairo_arc(cr, histogram->width-8.0, 8.0, 3.0, 0.0, 2*G_PI);
		cairo_fill(cr);

		/* Green */
		cairo_set_source_rgba(cr, 0.2, 1.0, 0.2, 0.5);
		cairo_move_to (cr, 0, (histogram->height-1)-histogram->output_samples[1][0]/factor);
		for (x = 1; x < histogram->width; x++)
			cairo_line_to(cr, x, (histogram->height-1)-histogram->output_samples[1][x]/factor);
		cairo_stroke (cr);
		cairo_set_source_rgba(cr, 0.2, 1.0, 0.2, histogram->input_samples[1][0]/100.0);
		cairo_arc(cr, 8.0, 16.0, 3.0, 0.0, 2*G_PI);
		cairo_fill(cr);
		cairo_set_source_rgba(cr, 0.2, 1.0, 0.2, histogram->input_samples[1][255]/100.0);
		cairo_arc(cr, histogram->width-8.0, 16.0, 3.0, 0.0, 2*G_PI);
		cairo_fill(cr);

		/* Blue */
		cairo_set_source_rgba(cr, 0.2, 0.2, 1.0, 0.5);
		cairo_move_to (cr, 0, (histogram->height-1)-histogram->output_samples[2][0]/factor);
		for (x = 1; x < histogram->width; x++)
			cairo_line_to(cr, x, (histogram->height-1)-histogram->output_samples[2][x]/factor);
		cairo_stroke (cr);
		cairo_set_source_rgba(cr, 0.2, 0.2, 1.0, histogram->input_samples[2][0]/100.0);
		cairo_arc(cr, 8.0, 24.0, 3.0, 0.0, 2*G_PI);
		cairo_fill(cr);
		cairo_set_source_rgba(cr, 0.2, 0.2, 1.0, histogram->input_samples[2][255]/100.0);
		cairo_arc(cr, histogram->width-8.0, 24.0, 3.0, 0.0, 2*G_PI);
		cairo_fill(cr);

		/* Luma */
		cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.5);
		cairo_move_to (cr, 0, histogram->height);
		for (x = 0; x < histogram->width; x++)
			cairo_line_to(cr, x, (histogram->height)-histogram->output_samples[3][x]/factor);
		cairo_line_to(cr, x, histogram->height);
		cairo_fill (cr);

		for (c = 0; c < 4; c++)
		{
			if (current[c] >= 0 && current[c] < histogram->width)
			{
				switch (c)
				{
					case 0:
						cairo_set_source_rgba(cr, 1.0, 0.0, 0.0, 0.2);
						break;
					case 1:
						cairo_set_source_rgba(cr, 0.0, 1.0, 0.0, 0.3);
						break;
					case 2:
						cairo_set_source_rgba(cr, 0.0, 0.0, 1.0, 0.2);
						break;
					case 3:
						cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.4);
						break;
				}
				cairo_move_to (cr, current[c],(histogram->height-1)-histogram->output_samples[c][current[c]]/factor);
				cairo_line_to(cr, current[c],0);
				cairo_stroke (cr);
			}
		}

		/* We're done */
		cairo_destroy (cr);
#else /* GTK_CHECK_VERSION(2,8,0) */
		GdkPoint points[histogram->width];
		const static GdkColor red = {0, 0xffff, 0x0000, 0x0000 };
		const static GdkColor green = {0, 0x0000, 0xffff, 0x0000 };
		const static GdkColor blue = {0, 0x0000, 0x0000, 0xffff };
		const static GdkColor luma = {0, 0xeeee, 0xeeee, 0xeeee };

		/* Red */
		gdk_gc_set_rgb_fg_color(gc, &red);
		for (x = 0; x < histogram->width; x++)
		{
			points[x].x = x; /* Only update x the first time! */
			points[x].y = (histogram->height-1)-histogram->output_samples[0][x]/factor;
		}
		gdk_draw_lines(histogram->blitter, gc, points, histogram->width);
		/* Underexposed */
		if (histogram->input_samples[0][0]>99)
			gdk_draw_arc(histogram->blitter, gc, TRUE, 1, 0, 8, 8, 0, 360*64);
		/* Overexposed */
		if (histogram->input_samples[0][255]>99)
			gdk_draw_arc(histogram->blitter, gc, TRUE, histogram->width-10, 0, 8, 8, 0, 360*64);

		/* Green */
		gdk_gc_set_rgb_fg_color(gc, &green);
		for (x = 0; x < histogram->width; x++)
			points[x].y = (histogram->height-1)-histogram->output_samples[1][x]/factor;
		gdk_draw_lines(histogram->blitter, gc, points, histogram->width);
		if (histogram->input_samples[1][0]>99)
			gdk_draw_arc(histogram->blitter, gc, TRUE, 1, 10, 8, 8, 0, 360*64);
		if (histogram->input_samples[1][255]>99)
			gdk_draw_arc(histogram->blitter, gc, TRUE, histogram->width-10, 10, 8, 8, 0, 360*64);

		/* Blue */
		gdk_gc_set_rgb_fg_color(gc, &blue);
		for (x = 0; x < histogram->width; x++)
			points[x].y = (histogram->height-1)-histogram->output_samples[2][x]/factor;
		gdk_draw_lines(histogram->blitter, gc, points, histogram->width);
		if (histogram->input_samples[2][0]>99)
			gdk_draw_arc(histogram->blitter, gc, TRUE, 1, 20, 8, 8, 0, 360*64);
		if (histogram->input_samples[2][255]>99)
			gdk_draw_arc(histogram->blitter, gc, TRUE, histogram->width-10, 20, 8, 8, 0, 360*64);

		/* Luma */
		gdk_gc_set_rgb_fg_color(gc, &luma);
		for (x = 0; x < histogram->width; x++)
			points[x].y = (histogram->height-1)-histogram->output_samples[3][x]/factor;
		gdk_draw_lines(histogram->blitter, gc, points, histogram->width);

#endif /* GTK_CHECK_VERSION(2,8,0) */

		/* Blit to screen */
		gdk_draw_drawable(window, gc, histogram->blitter, 0, 0, 0, 0, histogram->width, histogram->height);

		g_object_unref(gc);
	}

}
