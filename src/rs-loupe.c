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

#include <rawstudio.h>
#include "rs-loupe.h"

G_DEFINE_TYPE (RSLoupe, rs_loupe, GTK_TYPE_WINDOW)

static gboolean expose(GtkWidget *widget, GdkEventExpose *event, RSLoupe *loupe);
static void move(RSLoupe *loupe);
static void add_border(RSLoupe *loupe, GdkPixbuf *buffer, GdkRectangle *request);
static void redraw(RSLoupe *loupe);

static void
rs_loupe_finalize(GObject *object)
{
	RSLoupe *loupe = RS_LOUPE(object);

	if (loupe->filter)
		g_object_unref(loupe->filter);

	G_OBJECT_CLASS (rs_loupe_parent_class)->finalize (object);
}

static void
rs_loupe_class_init(RSLoupeClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = rs_loupe_finalize;
}

static void
rs_loupe_init(RSLoupe *loupe)
{
	/* Get screen size */
	GdkScreen *screen = gdk_screen_get_default();
	const gint screen_width = gdk_screen_get_width(screen);
	const gint screen_height = gdk_screen_get_height(screen);

	const gint loupe_size = MIN(screen_width/4, screen_height/3);

	/* Initialize window */
	gtk_window_resize(GTK_WINDOW(loupe), loupe_size, loupe_size);
	gtk_window_set_keep_above(GTK_WINDOW(loupe), TRUE);

	g_object_set(GTK_WINDOW(loupe),
		"accept-focus", FALSE,
		"decorated", FALSE,
		"deletable", FALSE,
		"focus-on-map", TRUE,
		"skip-pager-hint", TRUE,
		"skip-taskbar-hint", TRUE,
		"type", GTK_WINDOW_POPUP,
		NULL);

	loupe->canvas = gtk_drawing_area_new();
	gtk_container_add(GTK_CONTAINER(loupe), loupe->canvas);

	g_signal_connect(loupe->canvas, "expose-event", G_CALLBACK(expose), loupe);
}

/**
 * Instantiates a new RSLoupe
 * @return A new RSLoupe
 */
RSLoupe *
rs_loupe_new(void)
{
	return g_object_new(RS_TYPE_LOUPE, NULL);
}

/**
 * Set the RSFilter a RSLoupe will get its image data from
 * @param loupe A RSLoupe
 * @param filter A RSFilter
 */
void
rs_loupe_set_filter(RSLoupe *loupe, RSFilter *filter)
{
	g_assert(RS_IS_LOUPE(loupe));
	g_assert(RS_IS_FILTER(filter));

	if (loupe->filter)
		g_object_unref(loupe->filter);

	loupe->filter = g_object_ref(filter);
}

/**
 * Set center coordinate of the RSLoupe, this will be clamped to filter size
 * @param loupe A RSLoupe
 * @param center_x Center of loupe on the X-axis
 * @param center_y Center of loupe on the Y-axis
 */
void
rs_loupe_set_coord(RSLoupe *loupe, gint center_x, gint center_y)
{
	g_assert(RS_IS_LOUPE(loupe));

	loupe->center_x = center_x;
	loupe->center_y = center_y;

	move(loupe);
	redraw(loupe);
}

static gboolean
expose(GtkWidget *widget, GdkEventExpose *event, RSLoupe *loupe)
{
	/* We always draw the full frame */
	if (event->count > 0)
		return TRUE;

	redraw(loupe);

	return TRUE;
}

static void
move(RSLoupe *loupe)
{
	const gint distance_to_window = 50;
	const gint distance_to_border = 20;

	/* Get cursor position */
	gint cursor_x=0, cursor_y=0;
	gdk_display_get_pointer(gdk_display_get_default(), NULL, &cursor_x, &cursor_y, NULL);

	/* Get window size */
	gint window_width, window_height;
	gtk_window_get_size(GTK_WINDOW(loupe), &window_width, &window_height);

	/* Get screen size */
	GdkScreen *screen = gdk_screen_get_default();
	const gint screen_width = gdk_screen_get_width(screen);
	const gint screen_height = gdk_screen_get_height(screen);

	if (loupe->left)
	{
		if ((cursor_x - window_width - distance_to_window) < distance_to_border)
			loupe->left = !loupe->left;
	}
	else
	{
		if ((cursor_x + window_width + distance_to_window) > (screen_width - distance_to_border))
			loupe->left = !loupe->left;
	}

	if (loupe->atop)
	{
		if ((cursor_y - window_height - distance_to_window) < distance_to_border)
			loupe->atop = !loupe->atop;
	}
	else
	{
		if ((cursor_y + window_height + distance_to_window) > (screen_height - distance_to_border))
			loupe->atop = !loupe->atop;
	}
	gint place_x, place_y;

	if (loupe->left)
		place_x = cursor_x - window_width - distance_to_window;
	else
		place_x = cursor_x + distance_to_window;

	if (loupe->atop)
		place_y = cursor_y - window_height - distance_to_window;
	else
		place_y = cursor_y + distance_to_window;

	gtk_window_move(GTK_WINDOW(loupe), place_x, place_y);
}

static void
add_border(RSLoupe *loupe, GdkPixbuf *buffer, GdkRectangle *request)
{
	guchar *img = gdk_pixbuf_get_pixels(buffer);
	gint rs = gdk_pixbuf_get_rowstride (buffer);
	gint ch = gdk_pixbuf_get_n_channels (buffer);

	gint i;
	img = &img[request->x*ch+request->y*rs];
	guchar* img2 = &img[rs*(request->height-1)];
	for (i = 0; i < request->width*ch; i++) 
	{
		img[i] = 0;
		img2[i] = 0;
	}

	img2 = &img[(request->width-1)*ch];
	for (i = 1; i < request->height; i++) 
	{
		img[i*rs] =img[i*rs+1] = img[i*rs+2] = 0;
		img2[i*rs] = img2[i*rs+1] = img2[i*rs+2] = 0;
	}
}

static void
redraw(RSLoupe *loupe)
{
	if (!loupe->filter)
		return;

	if (!GTK_WIDGET_DRAWABLE(loupe->canvas))
		return;

	GdkDrawable *drawable = GDK_DRAWABLE(loupe->canvas->window);
	GdkGC *gc = gdk_gc_new(drawable);

	const gint width = rs_filter_get_width(loupe->filter);
	const gint height = rs_filter_get_height(loupe->filter);

	/* Get window size */
	gint window_width, window_height;
	gtk_window_get_size(GTK_WINDOW(loupe), &window_width, &window_height);

	/* Create request ROI */
	RSFilterRequest *request = rs_filter_request_new();
	GdkRectangle roi;
	roi.x = CLAMP(loupe->center_x - window_width/2, 0, width-window_width-1);
	roi.y = CLAMP(loupe->center_y - window_height/2, 0, height-window_height-1);
	roi.width = window_width;
	roi.height = window_height;
	rs_filter_request_set_roi(request, &roi);

	RSFilterResponse *response = rs_filter_get_image8(loupe->filter, request);
	GdkPixbuf *buffer = rs_filter_response_get_image8(response);
	g_object_unref(response);

	g_object_unref(request);

	add_border(loupe, buffer, &roi);

	gdk_draw_pixbuf(drawable, gc, buffer, roi.x, roi.y, 0, 0, roi.width, roi.height, GDK_RGB_DITHER_NONE, 0, 0);

	g_object_unref(buffer);
	g_object_unref(gc);
}
