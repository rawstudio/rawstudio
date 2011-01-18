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

#ifdef WIN32 /* Win32 _aligned_malloc */
#include <malloc.h>
#include <stdio.h>
#endif

#include <rawstudio.h>
#include <string.h>
#include <stdlib.h>
#define _ISOC9X_SOURCE 1 /* lrint() */
#define _ISOC99_SOURCE 1
#define	__USE_ISOC9X 1
#define	__USE_ISOC99 1
#include <math.h> /* floor() */
#include "rs-image16.h"

#define PITCH(width) ((((width)+15)/16)*16)

G_DEFINE_TYPE (RS_IMAGE16, rs_image16, G_TYPE_OBJECT);

static GObjectClass *parent_class = NULL;

static void
rs_image16_dispose (GObject *obj)
{
	RS_IMAGE16 *self = (RS_IMAGE16 *)obj;

	if (self->dispose_has_run)
		return;
	self->dispose_has_run = TRUE;

	G_OBJECT_CLASS (parent_class)->dispose (obj);
}

static void
rs_image16_finalize (GObject *obj)
{
	RS_IMAGE16 *self = (RS_IMAGE16 *)obj;

	if (self->pixels && (self->pixels_refcount == 1))
		free(self->pixels);

	self->pixels_refcount--;

	/* Chain up to the parent class */
	G_OBJECT_CLASS (parent_class)->finalize (obj);
}

static void
rs_image16_class_init (RS_IMAGE16Class *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

	gobject_class->dispose = rs_image16_dispose;
	gobject_class->finalize = rs_image16_finalize;

	parent_class = g_type_class_peek_parent (klass);
}

static void
rs_image16_init (RS_IMAGE16 *self)
{
	self->filters = 0;
	self->pixels = NULL;
	self->pixels_refcount = 0;
}

void
rs_image16_transform_getwh(RS_IMAGE16 *in, RS_RECT *crop, gdouble angle, gint orientation, gint *w, gint *h)
{
	RS_MATRIX3 mat;
	gdouble minx, miny;
	gdouble maxx, maxy;

	matrix3_identity(&mat);

	/* rotate straighten-angle + orientation-angle */
	matrix3_affine_rotate(&mat, angle+(orientation&3)*90.0);

	/* flip if needed */
	if (orientation&4)
		matrix3_affine_scale(&mat, 1.0, -1.0);

	/* translate into positive x,y*/
	matrix3_affine_get_minmax(&mat, &minx, &miny, &maxx, &maxy, 0.0, 0.0, (gdouble) in->w, (gdouble) in->h);
	matrix3_affine_translate(&mat, -minx, -miny);

	/* get width and height used for calculating scale */
	*w = lrint(maxx - minx);
	*h = lrint(maxy - miny);

	/* apply crop if needed */
	if (crop)
	{
		/* calculate cropped width and height */
		*w = abs(crop->x2 - crop->x1 + 1);
		*h = abs(crop->y2 - crop->y1 + 1);
		/* translate non-cropped area into negative x,y*/
		matrix3_affine_translate(&mat, ((gdouble) -crop->x1), ((gdouble) -crop->y1));
	}

	return;
}

RS_IMAGE16 *
rs_image16_new(const guint width, const guint height, const guint channels, const guint pixelsize)
{
	gint ret;
	RS_IMAGE16 *rsi;

	g_assert(width < 65536);
	g_assert(height < 65536);

	g_assert(width > 0);
	g_assert(height > 0);

	g_assert(channels > 0);
	g_assert(pixelsize >= channels);

	rsi = g_object_new(RS_TYPE_IMAGE16, NULL);
	rsi->w = width;
	rsi->h = height;
	rsi->rowstride = PITCH(width * pixelsize);
	rsi->pitch = rsi->rowstride / pixelsize;
	rsi->channels = channels;
	rsi->pixelsize = pixelsize;
	rsi->filters = 0;

	/* Allocate actual pixels */
#ifdef WIN32
	rsi->pixels = _aligned_malloc(rsi->h*rsi->rowstride * sizeof(gushort), 16); 
	if (rsi->pixels == NULL)
#else
	ret = posix_memalign((void **) &rsi->pixels, 16, rsi->h*rsi->rowstride * sizeof(gushort));
	if (ret > 0)
#endif
	{
		rsi->pixels = NULL;
		g_object_unref(rsi);
		return NULL;
	}
	rsi->pixels_refcount = 1;

	/* Verify alignment */
	g_assert((GPOINTER_TO_INT(rsi->pixels) % 16) == 0);
	g_assert((rsi->rowstride % 16) == 0);

	return(rsi);
}

/**
 * Initializes a new RS_IMAGE16 with pixeldata from @input.
 * @note Pixeldata is NOT copied to new RS_IMAGE16.
 * @param input A RS_IMAGE16
 * @param rectangle A GdkRectangle describing the area to subframe
 * @return A new RS_IMAGE16 with a refcount of 1, the image can be bigger
 *         than rectangle to retain 16 byte alignment.
 */
RS_IMAGE16 *
rs_image16_new_subframe(RS_IMAGE16 *input, GdkRectangle *rectangle)
{
	RS_IMAGE16 *output;
	gint width, height;
	gint x, y;

	g_assert(RS_IS_IMAGE16(input));
	g_assert(rectangle->x >= 0);
	g_assert(rectangle->y >= 0);
	g_assert(rectangle->width > 0);
	g_assert(rectangle->height > 0);

	g_assert(rectangle->width <= input->w);
	g_assert(rectangle->height <= input->h);

	g_assert((rectangle->width + rectangle->x) <= input->w);
	g_assert((rectangle->height + rectangle->y) <= input->h);

	output = g_object_new(RS_TYPE_IMAGE16, NULL);

	/* Align x to 16 byte boundary on input with pixelsize 4 (8 byte/pixel) */
	x = rectangle->x;
	if (input->pixelsize == 4)
	{
		x = x - (x & 0x1);
		x = CLAMP(x, 0, input->w-1);
	}
	y = CLAMP(rectangle->y, 0, input->h-1);

	width = rectangle->width + (rectangle->x - x) + 1;
	width = CLAMP(width - (width&1), 1, input->w - x);
	height = CLAMP(rectangle->height, 1, input->h - y);

	output->w = width;
	output->h = height;
	output->rowstride = input->rowstride;
	output->pitch = input->pitch;
	output->channels = input->channels;
	output->pixelsize = input->pixelsize;
	output->filters = input->filters;

	output->pixels = GET_PIXEL(input, x, y);
	output->pixels_refcount = input->pixels_refcount + 1;

	/* Some sanity checks */
	g_assert(output->w <= input->w);
	g_assert(output->h <= input->h);

	g_assert(output->w > 0);
	g_assert(output->h > 0);

	g_assert(output->w >= rectangle->width);
	g_assert(output->h >= rectangle->height);

	g_assert((output->w - 4) <= rectangle->width);

	/* Verify alignment */
	g_assert((GPOINTER_TO_INT(output->pixels) % 16) == 0);
	g_assert((output->rowstride % 16) == 0);

	return output;
}

/* Bit blitter - works on byte-sized values */
static inline void 
bit_blt(char* dstp, int dst_pitch, const char* srcp, int src_pitch, int row_size, int height) 
{
	if (height == 1 || (dst_pitch == src_pitch && src_pitch == row_size)) 
	{
		memcpy(dstp, srcp, row_size*height);
		return;
	}

	int y;
	for (y = height; y > 0; --y)
	{
		memcpy(dstp, srcp, row_size);
		dstp += dst_pitch;
		srcp += src_pitch;
	}
}

RS_IMAGE16 *
rs_image16_copy(RS_IMAGE16 *in, gboolean copy_pixels)
{
	RS_IMAGE16 *out;
	out = rs_image16_new(in->w, in->h, in->channels, in->pixelsize);
	if (copy_pixels)
	{
		bit_blt((char*)GET_PIXEL(out,0,0), out->rowstride * 2, 
			(const char*)GET_PIXEL(in,0,0), in->rowstride * 2, out->rowstride * 2, in->h);
	}
	return(out);
}

/**
 * Returns a single pixel from a RS_IMAGE16
 * @param image A RS_IMAGE16
 * @param x X coordinate (column)
 * @param y Y coordinate (row)
 * @param extend_edges Tries to extend edges beyond image borders if TRUE
 */
inline gushort *
rs_image16_get_pixel(RS_IMAGE16 *image, gint x, gint y, gboolean extend_edges)
{
	gushort *pixel = NULL;

	if (image)
	{
		if (extend_edges)
		{
			if (x >= image->w)
				x = image->w-1;
			else if (x < 0)
				x = 0;
			if (y >= image->h)
				y = image->h-1;
			else if (y < 0)
				y = 0;
		}

		/* Return pixel if inside image */
		if ((x>=0) && (y>=0) && (x<image->w) && (y<image->h))
			pixel = &image->pixels[y*image->rowstride + x*image->pixelsize];
	}

	return pixel;
}

gchar *
rs_image16_get_checksum(RS_IMAGE16 *image)
{
	gint w = image->w;
	gint h = image->h;
	gint c = image->channels;

	gint x, y, z;

	gushort *pixels = g_new0(gushort, w*h*c);
	gushort *pixel = NULL;
	gushort *pixels_iter = pixels;

	for (x=0; x<w; x++)
	{
		for (y=0; y<h; y++)
		{
			pixel = rs_image16_get_pixel(image, x, y, FALSE);
			for (z=0; z<c; z++)
			{
				*pixels_iter = pixel[z];
				pixels_iter++;
			}
		}
	}
	return g_compute_checksum_for_data(G_CHECKSUM_SHA256, (guchar *) pixels, w*h*c);
}
