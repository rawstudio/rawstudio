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

#include "rs-navigator.h"

G_DEFINE_TYPE (RSNavigator, rs_navigator, GTK_TYPE_DRAWING_AREA)

static gboolean button_press_event(GtkWidget *widget, GdkEventButton *event);
static gboolean button_release_event(GtkWidget *widget, GdkEventButton *event);
static gboolean motion_notify_event(GtkWidget *widget, GdkEventMotion *event);
static gboolean expose(GtkWidget *widget, GdkEventExpose *event);
static void size_allocate(GtkWidget *widget, GtkAllocation *allocation, gpointer user_data);
static void h_changed(GtkAdjustment *adjustment, RSNavigator *navigator);
static void v_changed(GtkAdjustment *adjustment, RSNavigator *navigator);
static void h_value_changed(GtkAdjustment *adjustment, RSNavigator *navigator);
static void v_value_changed(GtkAdjustment *adjustment, RSNavigator *navigator);
static void filter_changed(RSFilter *filter, RSFilterChangedMask mask, RSNavigator *navigator);
static void redraw(RSNavigator *navigator);

static void
rs_navigator_finalize(GObject *object)
{
	RSNavigator *navigator = RS_NAVIGATOR(object);

	g_object_unref(navigator->cache);

	g_signal_handler_disconnect(navigator->vadjustment, navigator->vadjustment_signal1);
	g_signal_handler_disconnect(navigator->vadjustment, navigator->vadjustment_signal2);
	g_signal_handler_disconnect(navigator->hadjustment, navigator->hadjustment_signal1);
	g_signal_handler_disconnect(navigator->hadjustment, navigator->hadjustment_signal2);

	g_object_unref(navigator->vadjustment);
	g_object_unref(navigator->hadjustment);
	if (navigator->display_color_space)
		g_object_unref(navigator->display_color_space);
	G_OBJECT_CLASS (rs_navigator_parent_class)->finalize (object);
}

static void
rs_navigator_class_init(RSNavigatorClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(klass);

	object_class->finalize = rs_navigator_finalize;
	widget_class->button_press_event = button_press_event;
	widget_class->button_release_event = button_release_event;
	widget_class->motion_notify_event = motion_notify_event;
	widget_class->expose_event = expose;
}

static void
rs_navigator_init(RSNavigator *navigator)
{
	navigator->cache = rs_filter_new("RSCache", NULL);
	g_signal_connect(navigator->cache, "changed", G_CALLBACK(filter_changed), navigator);

	g_signal_connect(navigator, "size-allocate", G_CALLBACK(size_allocate), NULL);

	gtk_widget_set_events(GTK_WIDGET(navigator), 0
		| GDK_BUTTON_PRESS_MASK
		| GDK_BUTTON_RELEASE_MASK
		| GDK_POINTER_MOTION_MASK);
	gtk_widget_set_app_paintable(GTK_WIDGET(navigator), TRUE);
	navigator->display_color_space = NULL;
}

RSNavigator *
rs_navigator_new(void)
{
	return g_object_new(RS_TYPE_NAVIGATOR, NULL);
}

void
rs_navigator_set_adjustments(RSNavigator *navigator, GtkAdjustment *vadjustment, GtkAdjustment *hadjustment)
{
	g_assert(RS_IS_NAVIGATOR(navigator));
	g_assert(GTK_IS_ADJUSTMENT(vadjustment));
	g_assert(GTK_IS_ADJUSTMENT(hadjustment));

	navigator->vadjustment = g_object_ref(vadjustment);
	navigator->hadjustment = g_object_ref(hadjustment);

	navigator->width = (gint) (gtk_adjustment_get_upper(hadjustment)+0.5);
	navigator->height = (gint) (gtk_adjustment_get_upper(vadjustment)+0.5);
	navigator->x = (gint) (gtk_adjustment_get_value(hadjustment)+0.5);
	navigator->y = (gint) (gtk_adjustment_get_value(vadjustment)+0.5);

	navigator->vadjustment_signal1 = g_signal_connect(vadjustment, "changed", G_CALLBACK(v_changed), navigator);
	navigator->vadjustment_signal2 = g_signal_connect(vadjustment, "value-changed", G_CALLBACK(v_value_changed), navigator);
	navigator->hadjustment_signal1 = g_signal_connect(hadjustment, "changed", G_CALLBACK(h_changed), navigator);
	navigator->hadjustment_signal2 = g_signal_connect(hadjustment, "value-changed", G_CALLBACK(h_value_changed), navigator);
}

void
rs_navigator_set_source_filter(RSNavigator *navigator, RSFilter *source_filter)
{
	g_assert(RS_IS_NAVIGATOR(navigator));
	g_assert(RS_IS_FILTER(source_filter));

	rs_filter_set_previous(navigator->cache, source_filter);
}

void
rs_navigator_set_preview_widget(RSNavigator *navigator, RSPreviewWidget *preview) {
	g_assert(RS_IS_NAVIGATOR(navigator));
	g_assert(RS_IS_PREVIEW_WIDGET(preview));

	navigator->preview = preview;
}

static void
get_placement(RSNavigator *navigator, GdkRectangle *placement)
{
	rs_filter_get_size_simple(navigator->cache, RS_FILTER_REQUEST_QUICK, &placement->width, &placement->height);
	placement->x = navigator->widget_width/2 - placement->width/2;
	placement->y = navigator->widget_height/2 - placement->height/2;
}

static void
move_to(RSNavigator *navigator, gdouble x, gdouble y)
{
	if (navigator->widget_width && navigator->width && navigator->vadjustment && navigator->hadjustment)
	{
		GdkRectangle placement;
		get_placement(navigator, &placement);
		gdouble dx, dy;

		x -= placement.x;
		y -= placement.y;
		const gdouble scale = ((gdouble) placement.width) / navigator->width;

		/* Scale back to original size */
		dx = ((gdouble) x) / scale;
		dy = ((gdouble) y) / scale;

		/* Center pointer */
		dx -= navigator->x_page/2;
		dy -= navigator->y_page/2;

		/* Clamp */
		dx = CLAMP(dx, 0, navigator->width - navigator->x_page - 1);
		dy = CLAMP(dy, 0, navigator->height - navigator->y_page - 1);

		/* Modify adjusters */
		g_object_set(navigator->hadjustment, "value", dx, NULL);
		g_object_set(navigator->vadjustment, "value", dy, NULL);
	}
}

static gboolean
button_press_event(GtkWidget *widget, GdkEventButton *event)
{
	RSNavigator *navigator = RS_NAVIGATOR(widget);

	rs_preview_widget_quick_start(navigator->preview, TRUE);

	move_to(navigator, event->x, event->y);

	return TRUE;
}

static gboolean
button_release_event(GtkWidget *widget, GdkEventButton *event)
{
	RSNavigator *navigator = RS_NAVIGATOR(widget);

	move_to(navigator, event->x, event->y);

	rs_preview_widget_quick_end(navigator->preview);

	return TRUE;
}

static gboolean
motion_notify_event(GtkWidget *widget, GdkEventMotion *event)
{
	RSNavigator *navigator = RS_NAVIGATOR(widget);
	GdkWindow *window = widget->window;
	GdkModifierType mask;
	gint x, y;

	gdk_window_get_pointer(window, &x, &y, &mask);

	if ((x == navigator->last_x) && (y == navigator->last_y))
		return TRUE;

	navigator->last_x = x;
	navigator->last_y = y;

	if (mask & (GDK_BUTTON1_MASK|GDK_BUTTON2_MASK|GDK_BUTTON3_MASK)) {
		move_to(navigator, (gdouble) x, (gdouble) y);

	}
	return TRUE;
}

static gboolean
expose(GtkWidget *widget, GdkEventExpose *event)
{
	redraw(RS_NAVIGATOR(widget));

	return FALSE;
}

static void
size_allocate(GtkWidget *widget, GtkAllocation *allocation, gpointer user_data)
{
	RSNavigator *navigator = RS_NAVIGATOR(widget);
	navigator->widget_width = allocation->width;
	navigator->widget_height = allocation->height;

	redraw(navigator);
}

static void
h_changed(GtkAdjustment *adjustment, RSNavigator *navigator)
{
	gboolean do_redraw = FALSE;

	const gint width = (gint) (gtk_adjustment_get_upper(adjustment)+0.5);
	if (width != navigator->width)
	{
		navigator->width = width;
		do_redraw = TRUE;
	}
	const gint x_page = (gint) (gtk_adjustment_get_page_size(adjustment)+0.5);
	if (x_page != navigator->x_page)
	{
		navigator->x_page = x_page;
		do_redraw = TRUE;
	}

	if (do_redraw)
		redraw(navigator);
}

static void
v_changed(GtkAdjustment *adjustment, RSNavigator *navigator)
{
	gboolean do_redraw = FALSE;

	const gint height = (gint) (gtk_adjustment_get_upper(adjustment)+0.5);
	if (height != navigator->height)
	{
		navigator->height = height;
		do_redraw = TRUE;
	}
	const gint y_page = (gint) (gtk_adjustment_get_page_size(adjustment)+0.5);
	if (y_page != navigator->y_page)
	{
		navigator->y_page = y_page;
		do_redraw = TRUE;
	}

	if (do_redraw)
		redraw(navigator);
}

static void
h_value_changed(GtkAdjustment *adjustment, RSNavigator *navigator)
{
	const gint x = (gint) (gtk_adjustment_get_value(adjustment)+0.5);
	if (x != navigator->x)
	{
		navigator->x = x;
		redraw(navigator);
	}
}

static void
v_value_changed(GtkAdjustment *adjustment, RSNavigator *navigator)
{
	const gint y = (gint) (gtk_adjustment_get_value(adjustment)+0.5);
	if (y != navigator->y)
	{
		navigator->y = y;
		redraw(navigator);
	}
}

/**
 * Set display colorspace
 * @param navigator A RSNavigator
 * @param colorspace An RSColorSpace that should be used to display the content of the navigator
 */
void
rs_navigator_set_colorspace(RSNavigator *navigator, RSColorSpace *display_color_space)
{
	g_assert(RS_IS_NAVIGATOR(navigator));

	g_object_ref(display_color_space);
	navigator->display_color_space = display_color_space;
}

static void
filter_changed(RSFilter *filter, RSFilterChangedMask mask, RSNavigator *navigator)
{
	redraw(navigator);
}

static void
redraw(RSNavigator *navigator)
{
	if ((navigator->widget_width==0) || (navigator->widget_height==0))
		return;

	if (!GTK_WIDGET_DRAWABLE(GTK_WIDGET(navigator)))
		return;

	GtkWidget *widget = GTK_WIDGET(navigator);
	GdkDrawable *drawable = GDK_DRAWABLE(widget->window);
	GdkPixmap *blitter = gdk_pixmap_new(drawable, navigator->widget_width, navigator->widget_height, -1);
	cairo_t *cr = gdk_cairo_create(GDK_DRAWABLE(blitter));
	GdkGC *gc = gdk_gc_new(GDK_DRAWABLE(blitter));

	if (navigator->cache->previous)
	{
		RSFilterRequest *request = rs_filter_request_new();
		rs_filter_request_set_quick(RS_FILTER_REQUEST(request), FALSE);
		rs_filter_param_set_object(RS_FILTER_PARAM(request), "colorspace", navigator->display_color_space);
		
		RSFilterResponse *response = rs_filter_get_image8(navigator->cache, request);
		g_object_unref(request);
		
		GdkPixbuf *pixbuf = rs_filter_response_get_image8(response);
		GdkRectangle placement, rect;

		rs_filter_get_size_simple(navigator->cache, RS_FILTER_REQUEST_QUICK, &placement.width, &placement.height);
		placement.x = navigator->widget_width/2 - placement.width/2;
		placement.y = navigator->widget_height/2 - placement.height/2;

		const gdouble scale = ((gdouble) placement.width) / navigator->width;

		cairo_set_source_rgba(cr, 0.5, 0.5, 0.5, 1.0);
		cairo_paint(cr);
		cairo_set_antialias(cr, CAIRO_ANTIALIAS_NONE);

		/* creates a rectangle that matches the photo */
		gdk_cairo_rectangle(cr, &placement);

		/* Translate to image placement */
		cairo_translate(cr, placement.x, placement.y);

		/* Paint the pixbuf */
		gdk_cairo_set_source_pixbuf(cr, pixbuf, 0.0, 0.0);
		cairo_fill_preserve(cr);

		/* creates a rectangle that matches ROI */
		rect.x = scale * navigator->x + 0.5;
		rect.y = scale * navigator->y + 0.5;
		rect.width = scale * navigator->x_page + 0.5;
		rect.height = scale * navigator->y_page + 0.5;
		gdk_cairo_rectangle(cr, &rect);

		cairo_set_fill_rule (cr, CAIRO_FILL_RULE_EVEN_ODD);
		cairo_set_source_rgba(cr, 0.0, 0.0, 0.0, 0.35);
		/* fill acording to rule */
		cairo_fill_preserve (cr);
		/* roi rectangle */
		cairo_set_source_rgba (cr, 0.0, 0.0, 0.0, 0.0);
		cairo_stroke (cr);

		/* Draw white rectangle */
		cairo_set_source_rgba (cr, 1.0, 1.0, 1.0, 0.5);
		gdk_cairo_rectangle(cr, &rect);
		cairo_stroke (cr);

		g_object_unref(pixbuf);
		g_object_unref(response);
	}

	gdk_draw_drawable(drawable, gc, blitter, 0, 0, 0, 0, navigator->widget_width, navigator->widget_height);
	g_object_unref(gc);
	cairo_destroy(cr);
}
