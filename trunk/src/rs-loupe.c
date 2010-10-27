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
#include "rs-loupe.h"

G_DEFINE_TYPE (RSLoupe, rs_loupe, GTK_TYPE_WINDOW)

static gboolean expose(GtkWidget *widget, GdkEventExpose *event, RSLoupe *loupe);
static void move(RSLoupe *loupe);
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

	/* We have to grab focus, otherwise window will not show up in fullscreen mode */
	g_object_set(GTK_WINDOW(loupe),
		"accept-focus", TRUE,
		"decorated", FALSE,
		"deletable", FALSE,
		"focus-on-map", FALSE,
		"skip-pager-hint", TRUE,
		"skip-taskbar-hint", TRUE,
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
	return g_object_new(RS_TYPE_LOUPE, "type", GTK_WINDOW_POPUP, NULL);
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

/**
 * Set display colorspace
 * @param loupe A RSLoupe
 * @param colorspace An RSColorSpace that should be used to display the content of the loupe
 */
void
rs_loupe_set_colorspace(RSLoupe *loupe, RSColorSpace *display_color_space)
{
	g_assert(RS_IS_LOUPE(loupe));
	loupe->display_color_space = display_color_space;
}

void 
rs_loupe_set_screen(RSLoupe* loupe, GdkScreen* screen, gint screen_num)
{
	g_assert(RS_IS_LOUPE(loupe));

	if (loupe->display_screen == screen && loupe->screen_num == screen_num)
		return;

	loupe->display_screen = screen;
	loupe->screen_num = screen_num;
	const gint screen_width = gdk_screen_get_width(screen);
	const gint screen_height = gdk_screen_get_height(screen);

	const gint loupe_size = MIN(screen_width/4, screen_height/3) & 0xfffffff0;

	/* Set screen and resize window */
	gtk_window_set_screen(GTK_WINDOW(loupe), screen);
	gtk_window_resize(GTK_WINDOW(loupe), loupe_size, loupe_size);
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
	gdk_display_get_pointer(gdk_display_get_default(), &loupe->display_screen, &cursor_x, &cursor_y, NULL);

	/* Get window size */
	gint window_width, window_height;
	gtk_window_get_size(GTK_WINDOW(loupe), &window_width, &window_height);

	/* Get screen size */
	GdkScreen *screen = loupe->display_screen;
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
redraw(RSLoupe *loupe)
{
	if (!loupe->filter)
		return;

	if (!GTK_WIDGET_DRAWABLE(loupe->canvas))
		return;

	GdkDrawable *drawable = GDK_DRAWABLE(loupe->canvas->window);
	GdkGC *gc = gdk_gc_new(drawable);

	gint width;
	gint height;
	rs_filter_get_size_simple(loupe->filter, RS_FILTER_REQUEST_QUICK, &width, &height);

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
	rs_filter_param_set_object(RS_FILTER_PARAM(request), "colorspace", loupe->display_color_space);

	RSFilterResponse *response = rs_filter_get_image8(loupe->filter, request);
	GdkPixbuf *buffer = rs_filter_response_get_image8(response);
	g_object_unref(response);

	g_object_unref(request);

	gdk_draw_pixbuf(drawable, gc, buffer, roi.x, roi.y, 0, 0, roi.width, roi.height, GDK_RGB_DITHER_NONE, 0, 0);

	/* Draw border */
	static const GdkColor black = {0,0,0,0};
	gdk_gc_set_foreground(gc, &black);
	gdk_draw_rectangle(drawable, gc, FALSE, 0, 0, roi.width-1, roi.height-1);

	g_object_unref(buffer);
	g_object_unref(gc);
}
