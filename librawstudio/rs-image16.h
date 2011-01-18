/*
 * * Copyright (C) 2006-2011 Anders Brander <anders@brander.dk>, 
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

#ifndef RS_IMAGE16_H
#define RS_IMAGE16_H

#include <glib-object.h>

#define RS_TYPE_IMAGE16        (rs_image16_get_type ())
#define RS_IMAGE16(obj)        (G_TYPE_CHECK_INSTANCE_CAST ((obj), RS_TYPE_IMAGE16, RS_IMAGE16))
#define RS_IMAGE16_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), RS_TYPE_IMAGE16, RS_IMAGE16Class))
#define RS_IS_IMAGE16(obj)     (G_TYPE_CHECK_INSTANCE_TYPE ((obj), RS_TYPE_IMAGE16))
#define RS_IS_IMAGE16_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), RS_TYPE_IMAGE16))
#define RS_IMAGE16_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), RS_TYPE_IMAGE16, RS_IMAGE16Class))

struct _rs_image16 {
	GObject parent;
	gint w;
	gint h;
	gint pitch;
	gint rowstride;
	guint channels;
	guint pixelsize; /* the size of a pixel in SHORTS */
	gushort *pixels;
	gint pixels_refcount;
	guint filters;
	gboolean dispose_has_run;
};

typedef struct _RS_IMAGE16Class RS_IMAGE16Class;

struct _RS_IMAGE16Class {
	GObjectClass parent;
};

GType rs_image16_get_type (void);

/**
 * Convenience macro to get a pixel at specific position
 * @param image RS_IMAGE8 or RS_IMAGE16
 * @param x X coordinate (column)
 * @param y Y coordinate (row)
 */
#define GET_PIXEL(image, x, y) ((image)->pixels + (y)*(image)->rowstride + (x)*(image)->pixelsize)

#define GET_PIXBUF_PIXEL(pixbuf, x, y) (gdk_pixbuf_get_pixels((pixbuf)) + (y)*gdk_pixbuf_get_rowstride((pixbuf)) + (x)*gdk_pixbuf_get_n_channels((pixbuf)))

extern RS_IMAGE16 *rs_image16_new(const guint width, const guint height, const guint channels, const guint pixelsize);

/**
 * Initializes a new RS_IMAGE16 with pixeldata from @input.
 * @note Pixeldata is NOT copied to new RS_IMAGE16.
 * @param input A RS_IMAGE16
 * @param rectangle A GdkRectangle describing the area to subframe
 * @return A new RS_IMAGE16 with a refcount of 1, the image can be bigger
 *         than rectangle to retain 16 byte alignment.
 */
extern RS_IMAGE16 *
rs_image16_new_subframe(RS_IMAGE16 *input, GdkRectangle *rectangle);

extern void rs_image16_transform_getwh(RS_IMAGE16 *in, RS_RECT *crop, gdouble angle, gint orientation, gint *w, gint *h);

extern RS_IMAGE16 *rs_image16_copy(RS_IMAGE16 *rsi, gboolean copy_pixels);

/**
 * Returns a single pixel from a RS_IMAGE16
 * @param image A RS_IMAGE16
 * @param x X coordinate (column)
 * @param y Y coordinate (row)
 * @param extend_edges Tries to extend edges beyond image borders if TRUE
 */
extern inline gushort *rs_image16_get_pixel(RS_IMAGE16 *image, gint x, gint y, gboolean extend_edges);

extern gchar *rs_image16_get_checksum(RS_IMAGE16 *image);

#endif /* RS_IMAGE16_H */
