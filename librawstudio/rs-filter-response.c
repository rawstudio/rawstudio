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

#include "rs-filter-response.h"
#include "rs-image16.h"

struct _RSFilterResponse {
	RSFilterParam parent;
	gboolean dispose_has_run;

	gboolean roi_set;
	GdkRectangle roi;
	gboolean quick;
	RS_IMAGE16 *image;
	GdkPixbuf *image8;
	gint width;
	gint height;
};

G_DEFINE_TYPE(RSFilterResponse, rs_filter_response, RS_TYPE_FILTER_PARAM)

static void
rs_filter_response_dispose(GObject *object)
{
	RSFilterResponse *filter_response = RS_FILTER_RESPONSE(object);

	if (!filter_response->dispose_has_run)
	{
		filter_response->dispose_has_run = TRUE;

		if (filter_response->image)
			g_object_unref(filter_response->image);

		if (filter_response->image8)
			g_object_unref(filter_response->image8);
	}

	G_OBJECT_CLASS (rs_filter_response_parent_class)->dispose (object);
}

static void
rs_filter_response_finalize(GObject *object)
{
	G_OBJECT_CLASS (rs_filter_response_parent_class)->finalize (object);
}

static void
rs_filter_response_class_init(RSFilterResponseClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->dispose = rs_filter_response_dispose;
	object_class->finalize = rs_filter_response_finalize;
}

static void
rs_filter_response_init(RSFilterResponse *filter_response)
{
	filter_response->roi_set = FALSE;
	filter_response->quick = FALSE;
	filter_response->image = NULL;
	filter_response->image8 = NULL;
	filter_response->width = -1;
	filter_response->height = -1;
	filter_response->dispose_has_run = FALSE;
}

/**
 * Instantiate a new RSFilterResponse object
 * @return A new RSFilterResponse with a refcount of 1
 */
RSFilterResponse *
rs_filter_response_new(void)
{
	return g_object_new (RS_TYPE_FILTER_RESPONSE, NULL);
}

/**
 * Clone all flags of a RSFilterResponse EXCEPT images
 * @param filter_response A RSFilterResponse
 * @return A new RSFilterResponse with a refcount of 1
 */
RSFilterResponse *
rs_filter_response_clone(RSFilterResponse *filter_response)
{
	RSFilterResponse *new_filter_response = rs_filter_response_new();

	if (RS_IS_FILTER_RESPONSE(filter_response))
	{
		new_filter_response->roi_set = filter_response->roi_set;
		new_filter_response->roi = filter_response->roi;
		new_filter_response->quick = filter_response->quick;
		new_filter_response->width = filter_response->width;
		new_filter_response->height = filter_response->height;

		rs_filter_param_clone(RS_FILTER_PARAM(new_filter_response), RS_FILTER_PARAM(filter_response));
	}

	return new_filter_response;
}

/**
 * Set the ROI used in generating the response, if the whole image is
 * generated, this should NOT be set
 * @param filter_response A RSFilterResponse
 * @param roi A GdkRectangle describing the ROI or NULL to indicate complete
 *            image data
 */
void
rs_filter_response_set_roi(RSFilterResponse *filter_response, GdkRectangle *roi)
{
	g_assert(RS_IS_FILTER_RESPONSE(filter_response));

	filter_response->roi_set = FALSE;

	if (roi)
	{
		filter_response->roi_set = TRUE;
		filter_response->roi = *roi;
	}
}

/**
 * Get the ROI of the response
 * @param filter_response A RSFilterResponse
 * @return A GdkRectangle describing the ROI or NULL if the complete image is rendered
 */
GdkRectangle *
rs_filter_response_get_roi(const RSFilterResponse *filter_response)
{
	GdkRectangle *ret = NULL;

	g_assert(RS_IS_FILTER_RESPONSE(filter_response));

	if (filter_response->roi_set)
		ret = &RS_FILTER_RESPONSE(filter_response)->roi;

	return ret;
}

/**
 * Set quick flag on a response, this should be set if the image has been
 * rendered by any quick method and a better method is available
 * @note There is no boolean parameter, it would make no sense to remove a
 *       quick-flag
 * @param filter_response A RSFilterResponse
 */
void
rs_filter_response_set_quick(RSFilterResponse *filter_response)
{
	g_assert(RS_IS_FILTER_RESPONSE(filter_response));

	filter_response->quick = TRUE;
}

/**
 * Get the quick flag
 * @param filter_response A RSFilterResponse
 * @return TRUE if the image data was rendered using a "quick" algorithm and a
 *         faster is available, FALSE otherwise
 */
gboolean
rs_filter_response_get_quick(const RSFilterResponse *filter_response)
{
	g_assert(RS_IS_FILTER_RESPONSE(filter_response));

	return filter_response->quick;
}

/**
 * Set 16 bit image data
 * @param filter_response A RSFilterResponse
 * @param image A RS_IMAGE16
 */
void
rs_filter_response_set_image(RSFilterResponse *filter_response, RS_IMAGE16 *image)
{
	g_assert(RS_IS_FILTER_RESPONSE(filter_response));

	if (filter_response->image)
	{
		g_object_unref(filter_response->image);
		filter_response->image = NULL;
	}

	if (image)
		filter_response->image = g_object_ref(image);
}

/**
 * Is there a 16 bit image attached?
 * @param filter_response A RSFilterResponse
 * @return A boolean indicating of there is an image attached
 */
 gboolean rs_filter_response_has_image(const RSFilterResponse *filter_response)
{
	g_assert(RS_IS_FILTER_RESPONSE(filter_response));

	return !!filter_response->image;
}

/**
 * Get 16 bit image data
 * @param filter_response A RSFilterResponse
 * @return A RS_IMAGE16 (must be unreffed after usage) or NULL if none is set
 */
RS_IMAGE16 *
rs_filter_response_get_image(const RSFilterResponse *filter_response)
{
	RS_IMAGE16 *ret = NULL;

	g_assert(RS_IS_FILTER_RESPONSE(filter_response));

	if (filter_response->image)
		ret = g_object_ref(filter_response->image);

	return ret;
}

/**
 * Set 8 bit image data
 * @param filter_response A RSFilterResponse
 * @param pixbuf A GdkPixbuf
 */
void
rs_filter_response_set_image8(RSFilterResponse *filter_response, GdkPixbuf *pixbuf)
{
	g_assert(RS_IS_FILTER_RESPONSE(filter_response));

	if (filter_response->image8)
	{
		g_object_unref(filter_response->image8);
		filter_response->image8 = NULL;
	}

	if (pixbuf)
		filter_response->image8 = g_object_ref(pixbuf);
}

/**
 * Does the response have an 8 bit image
 * @param filter_response A RSFilterResponse
 * @return A gboolean TRUE if an image8 is attached, FALSE otherwise
 */
gboolean rs_filter_response_has_image8(const RSFilterResponse *filter_response) 
{
	g_assert(RS_IS_FILTER_RESPONSE(filter_response));

	return !!filter_response->image8;
}

/**
 * Get 8 bit image data
 * @param filter_response A RSFilterResponse
 * @return A GdkPixbuf (must be unreffed after usage) or NULL if none is set
 */
GdkPixbuf *
rs_filter_response_get_image8(const RSFilterResponse *filter_response)
{
	GdkPixbuf *ret = NULL;

	g_assert(RS_IS_FILTER_RESPONSE(filter_response));

	if (filter_response->image8)
		ret = g_object_ref(filter_response->image8);

	return ret;
}

/**
 * Set predicted width
 * @param filter_response A RSFilterResponse
 * @param width Width in pixels
 * @note Do not set this if width is unknown
 */
void rs_filter_response_set_width(RSFilterResponse *filter_response, gint width)
{
	g_assert(RS_IS_FILTER_RESPONSE(filter_response));

	filter_response->width = width;
}

/**
 * Set predicted height
 * @param filter_response A RSFilterResponse
 * @param height Height in pixels
 * @note Do not set this if height is unknown
 */
void rs_filter_response_set_height(RSFilterResponse *filter_response, gint height)
{
	g_assert(RS_IS_FILTER_RESPONSE(filter_response));

	filter_response->height = height;
}

/**
 * Get width
 * @param filter_response A RSFilterResponse
 * @return Width in pixels or -1 if unknown
 */
gint
rs_filter_response_get_width(const RSFilterResponse *filter_response)
{
	g_assert(RS_IS_FILTER_RESPONSE(filter_response));

	if (filter_response->width > -1)
		return filter_response->width;
	else if (filter_response->image)
		return filter_response->image->w;
	else if (filter_response->image8)
		return gdk_pixbuf_get_width(filter_response->image8);
	else
		return -1;
}

/**
 * Get height
 * @param filter_response A RSFilterResponse
 * @return Height in pixels or -1 if unknown
 */
gint
rs_filter_response_get_height(const RSFilterResponse *filter_response)
{
	g_assert(RS_IS_FILTER_RESPONSE(filter_response));

	if (filter_response->height > -1)
		return filter_response->height;
	else if (filter_response->image)
		return filter_response->image->h;
	else if (filter_response->image8)
		return gdk_pixbuf_get_height(filter_response->image8);
	else
		return -1;
}
