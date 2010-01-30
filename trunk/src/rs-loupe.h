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

#ifndef RS_LOUPE_H
#define RS_LOUPE_H

#include <glib-object.h>

G_BEGIN_DECLS

#define RS_TYPE_LOUPE rs_loupe_get_type()
#define RS_LOUPE(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), RS_TYPE_LOUPE, RSLoupe))
#define RS_LOUPE_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), RS_TYPE_LOUPE, RSLoupeClass))
#define RS_IS_LOUPE(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), RS_TYPE_LOUPE))
#define RS_IS_LOUPE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), RS_TYPE_LOUPE))
#define RS_LOUPE_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), RS_TYPE_LOUPE, RSLoupeClass))

typedef struct {
	GtkWindow parent;
	RSFilter *filter;
	GtkWidget *canvas;
	gint center_x;
	gint center_y;
	gboolean left;
	gboolean atop;
	RSColorSpace *display_color_space;
} RSLoupe;

typedef struct {
	GtkWindowClass parent_class;
} RSLoupeClass;

GType rs_loupe_get_type(void);

/**
 * Instantiates a new RSLoupe
 * @return A new RSLoupe
 */
RSLoupe *rs_loupe_new(void);

/**
 * Set the RSFilter a RSLoupe will get its image data from
 * @param loupe A RSLoupe
 * @param filter A RSFilter
 */
void rs_loupe_set_filter(RSLoupe *loupe, RSFilter *filter);

/**
 * Set center coordinate of the RSLoupe, this will be clamped to filter size
 * @param loupe A RSLoupe
 * @param center_x Center of loupe on the X-axis
 * @param center_y Center of loupe on the Y-axis
 */
void rs_loupe_set_coord(RSLoupe *loupe, gint center_x, gint center_y);

/**
 * Set display colorspace
 * @param loupe A RSLoupe
 * @param display_color_space An RSColorSpace that should be used to display the content of the loupe
 */
void rs_loupe_set_colorspace(RSLoupe *loupe, RSColorSpace *display_color_space);

G_END_DECLS

#endif /* RS_LOUPE_H */
