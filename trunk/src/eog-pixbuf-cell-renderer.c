/* Eye Of Gnome - Pixbuf Cellrenderer 
 *
 * Lifted from eog-2.20.0 with minor changes, Thanks! - Anders Brander
 *
 * Copyright (C) 2007 The GNOME Foundation
 *
 * Author: Lucas Rocha <lucasr@gnome.org>
 *
 * Based on gnome-control-center code (capplets/appearance/wp-cellrenderer.c) by: 
 *      - Denis Washington <denisw@svn.gnome.org>
 *      - Jens Granseuer <jensgr@gmx.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "eog-pixbuf-cell-renderer.h"

#include <math.h>

G_DEFINE_TYPE (EogPixbufCellRenderer, eog_pixbuf_cell_renderer, GTK_TYPE_CELL_RENDERER_PIXBUF)

static void eog_pixbuf_cell_renderer_render (GtkCellRenderer *cell,
                                             GdkWindow *window,
                                             GtkWidget *widget,
                                             GdkRectangle *background_area,
                                             GdkRectangle *cell_area,
                                             GdkRectangle *expose_area,
                                             GtkCellRendererState flags);

#define PLACEMENT_TTL 120 /* Seconds to remember */
static GStaticMutex placement_lock = G_STATIC_MUTEX_INIT;
static GTree *placement = NULL;
static GTimer *placement_age = NULL;

static gint
placement_cmp(gconstpointer a, gconstpointer b, gpointer user_data)
{
	return GPOINTER_TO_INT(a)-GPOINTER_TO_INT(b);
}

static void
placement_free(gpointer data)
{
	g_slice_free(BangPosition, data);
}

/**
 * This is an evil evil evil hack that should never be used by any sane person.
 * eog_pixbuf_cell_renderer_render() will store the position and the drawable
 * a pixbuf has been drawn to - this function returns this information in a
 * BangPositon. This will allow an aggressive caller to bang the GdkDrawable
 * directly instead of using conventional methods of setting the icon.
 * Please understand: THE INFORMATION RETURNED CAN BE WRONG AND/OR OUTDATED!
 * The key used in the tree is simply two XOR'ed pointers, collisions can
 * easily exist.
 */
gboolean
eog_pixbuf_cell_renderer_get_bang_position(GtkIconView *iconview, GdkPixbuf *pixbuf, BangPosition *bp)
{
	BangPosition *bp_;
	gboolean result = FALSE;

	g_static_mutex_lock (&placement_lock);
	bp_ = g_tree_lookup(placement, GINT_TO_POINTER(GPOINTER_TO_INT(pixbuf)^GPOINTER_TO_INT(iconview)));
	if (bp_ && GDK_IS_DRAWABLE(bp_->drawable))
	{
		*bp = *bp_;
		result = TRUE;
	}
	g_static_mutex_unlock (&placement_lock);

	return result;
}

static void
eog_pixbuf_cell_renderer_class_init (EogPixbufCellRendererClass *klass)
{
	GtkCellRendererClass *renderer_class;
	
	renderer_class = (GtkCellRendererClass *) klass;
	renderer_class->render = eog_pixbuf_cell_renderer_render;
}

static void
eog_pixbuf_cell_renderer_init (EogPixbufCellRenderer *renderer)
{
}

GtkCellRenderer *
eog_pixbuf_cell_renderer_new (void)
{
	return g_object_new (eog_pixbuf_cell_renderer_get_type (), NULL);
}
#if GTK_CHECK_VERSION(2,8,0)
/* Copied almost verbatim from gtk+-2.12.0/gtk/gtkcellrendererpixbuf.c */
static void
gtk_cell_renderer_pixbuf_get_size (GtkCellRenderer *cell,
				   GtkWidget       *widget,
				   GdkRectangle    *cell_area,
				   gint            *x_offset,
				   gint            *y_offset,
				   gint            *width,
				   gint            *height)
{
  GtkCellRendererPixbuf *cellpixbuf = (GtkCellRendererPixbuf *) cell;
  gint pixbuf_width  = 0;
  gint pixbuf_height = 0;
  gint calc_width;
  gint calc_height;

  if (cellpixbuf->pixbuf)
    {
      pixbuf_width  = gdk_pixbuf_get_width (cellpixbuf->pixbuf);
      pixbuf_height = gdk_pixbuf_get_height (cellpixbuf->pixbuf);
    }
  if (cellpixbuf->pixbuf_expander_open)
    {
      pixbuf_width  = MAX (pixbuf_width, gdk_pixbuf_get_width (cellpixbuf->pixbuf_expander_open));
      pixbuf_height = MAX (pixbuf_height, gdk_pixbuf_get_height (cellpixbuf->pixbuf_expander_open));
    }
  if (cellpixbuf->pixbuf_expander_closed)
    {
      pixbuf_width  = MAX (pixbuf_width, gdk_pixbuf_get_width (cellpixbuf->pixbuf_expander_closed));
      pixbuf_height = MAX (pixbuf_height, gdk_pixbuf_get_height (cellpixbuf->pixbuf_expander_closed));
    }
  
  calc_width  = (gint) cell->xpad * 2 + pixbuf_width;
  calc_height = (gint) cell->ypad * 2 + pixbuf_height;
  
  if (cell_area && pixbuf_width > 0 && pixbuf_height > 0)
    {
      if (x_offset)
	{
	  *x_offset = (((gtk_widget_get_direction (widget) == GTK_TEXT_DIR_RTL) ?
                        (1.0 - cell->xalign) : cell->xalign) * 
                       (cell_area->width - calc_width));
	  *x_offset = MAX (*x_offset, 0);
	}
      if (y_offset)
	{
	  *y_offset = (cell->yalign *
                       (cell_area->height - calc_height));
          *y_offset = MAX (*y_offset, 0);
	}
    }
  else
    {
      if (x_offset) *x_offset = 0;
      if (y_offset) *y_offset = 0;
    }

  if (width)
    *width = calc_width;
  
  if (height)
    *height = calc_height;
}
#endif /* GTK_CHECK_VERSION(2,8,0) */

static void
eog_pixbuf_cell_renderer_render (GtkCellRenderer *cell,
                                 GdkWindow *window,
                                 GtkWidget *widget,
                                 GdkRectangle *background_area,
                                 GdkRectangle *cell_area,
                                 GdkRectangle *expose_area,
                                 GtkCellRendererState flags)
{
#if GTK_CHECK_VERSION(2,8,0)
	GtkCellRendererPixbuf *cellpixbuf = (GtkCellRendererPixbuf *) cell;
	GdkPixbuf *pixbuf;
	pixbuf = cellpixbuf->pixbuf;
	GdkRectangle pix_rect;
	cairo_t *cr;
	BangPosition *bp;
	gpointer key;

	gtk_cell_renderer_pixbuf_get_size (cell, widget, cell_area,
		&pix_rect.x,
		&pix_rect.y,
		&pix_rect.width,
		&pix_rect.height);

	pix_rect.x += cell_area->x + cell->xpad;
	pix_rect.y += cell_area->y + cell->ypad;
	pix_rect.width  -= cell->xpad * 2;
	pix_rect.height -= cell->ypad * 2;

	g_static_mutex_lock (&placement_lock);
	if (placement == NULL)
		placement = g_tree_new_full(placement_cmp, NULL, NULL, placement_free);
	if (placement_age == NULL)
		placement_age = g_timer_new();

	if (g_timer_elapsed(placement_age, NULL) > PLACEMENT_TTL)
	{
		g_tree_destroy(placement);
		placement = g_tree_new_full(placement_cmp, NULL, NULL, placement_free);
		g_timer_start(placement_age);
	}

	key = GINT_TO_POINTER(GPOINTER_TO_INT(pixbuf)^GPOINTER_TO_INT(widget));
	bp = g_tree_lookup(placement, key);
	if (bp)
	{
		bp->drawable = GDK_DRAWABLE (window);
		bp->x = pix_rect.x;
		bp->y = pix_rect.y;
	}
	else
	{
		bp = g_slice_new(BangPosition);
		bp->drawable = GDK_DRAWABLE (window);
		bp->x = pix_rect.x;
		bp->y = pix_rect.y;
		g_tree_insert(placement, key, bp);
	}
	g_static_mutex_unlock (&placement_lock);

	if ((flags & (GTK_CELL_RENDERER_SELECTED|GTK_CELL_RENDERER_PRELIT)) != 0) {
		gint radius = 5;
		gint x, y, w, h;
		GtkStateType state;

		x = background_area->x;
		y = background_area->y;
		w = background_area->width;
		h = background_area->height;
		
		/* Sometimes width is -1 - not sure what to do here */
		if (w == -1) return;
		
		if ((flags & GTK_CELL_RENDERER_SELECTED) != 0) {
			if (GTK_WIDGET_HAS_FOCUS (widget))
				state = GTK_STATE_SELECTED;
			else
				state = GTK_STATE_ACTIVE;
		} else
			state = GTK_STATE_PRELIGHT;

		/* draw the selection indicator */
		cr = gdk_cairo_create (GDK_DRAWABLE (window));
		gdk_cairo_set_source_color (cr, &widget->style->base[state]);
		
		cairo_arc (cr, x + radius, y + radius, radius, M_PI, M_PI * 1.5);
		cairo_arc (cr, x + w - radius, y + radius, radius, M_PI * 1.5, 0);
		cairo_arc (cr, x + w - radius, y + h - radius, radius, 0, M_PI * 0.5);
		cairo_arc (cr, x + radius, y + h - radius, radius, M_PI * 0.5, M_PI);
		cairo_close_path (cr);
		
		/* FIXME: this should not be hardcoded to 4 */
		cairo_rectangle (cr, x + 4, y + 4, w - 8, h - 8);

		/*cairo_set_fill_rule (cr, CAIRO_FILL_RULE_EVEN_ODD);*/
		cairo_fill (cr);
		cairo_destroy (cr);
	}

	/* Always draw the thumbnail */
	cr = gdk_cairo_create (window);
	gdk_cairo_set_source_pixbuf (cr, pixbuf,  pix_rect.x, pix_rect.y);
	gdk_cairo_rectangle (cr, &pix_rect);
	cairo_fill (cr);
	cairo_destroy (cr);
#else
	(* GTK_CELL_RENDERER_CLASS (eog_pixbuf_cell_renderer_parent_class)->render)
        (cell, window, widget, background_area, cell_area, expose_area, flags);
#endif /* GTK_CHECK_VERSION(2,8,0) */
}
