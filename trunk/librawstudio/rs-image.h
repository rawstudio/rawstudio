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

#ifndef RS_IMAGE_H
#define RS_IMAGE_H

#include <glib-object.h>

G_BEGIN_DECLS

#define RS_TYPE_IMAGE rs_image_get_type()
#define RS_IMAGE(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), RS_TYPE_IMAGE, RSImage))
#define RS_IMAGE_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), RS_TYPE_IMAGE, RSImageClass))
#define RS_IS_IMAGE(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), RS_TYPE_IMAGE))
#define RS_IS_IMAGE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), RS_TYPE_IMAGE))
#define RS_IMAGE_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), RS_TYPE_IMAGE, RSImageClass))

typedef struct {
	GObjectClass parent_class;
} RSImageClass;

GType rs_image_get_type(void);

extern RSImage *
rs_image_new(gint width, gint height, gint number_of_planes);

extern gint
rs_image_get_width(RSImage *image);

extern gint
rs_image_get_height(RSImage *image);

extern gint
rs_image_get_number_of_planes(RSImage *image);

extern gfloat *
rs_image_get_plane(RSImage *image, gint plane_num);

G_END_DECLS

#endif /* RS_IMAGE_H */
