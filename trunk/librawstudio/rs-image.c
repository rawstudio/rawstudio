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

#include <rawstudio.h>
#include "rs-image.h"

struct _RSImage {
	GObject parent;
	gint width;
	gint height;
	gint number_of_planes;
	gfloat **planes;
};

G_DEFINE_TYPE (RSImage, rs_image, G_TYPE_OBJECT)

typedef enum {
	RS_IMAGE_CHANGED,
	RS_IMAGE_LAST_SIGNAL
} RSImageSignals;

static guint signals[RS_IMAGE_LAST_SIGNAL] = { 0 };

static void
rs_image_finalize (GObject *object)
{
	RSImage *image = RS_IMAGE(object);
	gint plane;

	for (plane=0; plane<image->number_of_planes; plane++)
		g_free(image->planes[plane]);
	g_free(image->planes);

	if (G_OBJECT_CLASS (rs_image_parent_class)->finalize)
		G_OBJECT_CLASS (rs_image_parent_class)->finalize (object);
}

static void
rs_image_class_init (RSImageClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = rs_image_finalize;

	signals[RS_IMAGE_CHANGED] = g_signal_newv(
		"rs-image-changed",
		G_TYPE_FROM_CLASS (klass),
		G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
		NULL, NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0, NULL);
}

static void
rs_image_init(RSImage *self)
{
}

RSImage *
rs_image_new(gint width, gint height, gint number_of_planes)
{
	gint plane;
	RSImage *image;

	g_return_val_if_fail(width < 65535, NULL);
	g_return_val_if_fail(width < 65536, NULL);
	g_return_val_if_fail(height < 65536, NULL);
	g_return_val_if_fail(width > 0, NULL);
	g_return_val_if_fail(height > 0, NULL);
	g_return_val_if_fail(number_of_planes > 0, NULL);

	image = g_object_new(RS_TYPE_IMAGE, NULL);
	image->number_of_planes = number_of_planes;
	image->width = width;
	image->height = height;

	/* Allocate space for all planes and all pixels */
	image->planes = g_new(gfloat *, number_of_planes);
	for(plane=0; plane<image->number_of_planes; plane++)
		image->planes[plane] = g_new(gfloat, image->width*image->height);

	return image;
}

void
rs_image_changed(RSImage *image)
{
	g_return_if_fail(RS_IS_IMAGE(image));

	g_signal_emit(image, signals[RS_IMAGE_CHANGED], 0, NULL);
}

gint
rs_image_get_width(RSImage *image)
{
	g_return_val_if_fail(RS_IS_IMAGE(image), 0);

	return image->width;
}

gint
rs_image_get_height(RSImage *image)
{
	g_return_val_if_fail(RS_IS_IMAGE(image), 0);

	return image->height;
}

gint
rs_image_get_number_of_planes(RSImage *image)
{
	g_return_val_if_fail(RS_IS_IMAGE(image), 0);

	return image->number_of_planes;
}

gfloat *
rs_image_get_plane(RSImage *image, gint plane_num)
{
	g_return_val_if_fail(RS_IS_IMAGE(image), NULL);
	g_return_val_if_fail(plane_num > 0, NULL);
	g_return_val_if_fail(plane_num < image->number_of_planes, NULL);

	return image->planes[plane_num];
}
