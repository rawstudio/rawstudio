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

#include <gtk/gtk.h>
#include "rs-filter-request.h"

struct _RSFilterRequest {
	RSFilterParam parent;
	gboolean roi_set;
	GdkRectangle roi;
	gboolean quick;
};

G_DEFINE_TYPE(RSFilterRequest, rs_filter_request, RS_TYPE_FILTER_PARAM)

static void
rs_filter_request_finalize(GObject *object)
{
	G_OBJECT_CLASS (rs_filter_request_parent_class)->finalize (object);
}

static void
rs_filter_request_class_init(RSFilterRequestClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = rs_filter_request_finalize;
}

static void
rs_filter_request_init(RSFilterRequest *filter_request)
{
	filter_request->roi_set = FALSE;
	filter_request->quick = FALSE;
}

/**
 * Instantiate a new RSFilterRequest
 * @return A new RSFilterRequest with a refcount of 1
 */
RSFilterRequest *
rs_filter_request_new(void)
{
	return g_object_new(RS_TYPE_FILTER_REQUEST, NULL);
}

/**
 * Get a RSFilterRequest singleton with quick set to TRUE
 * @return A RSFilterRequest, this should not be unreffed
 */
const RSFilterRequest *rs_filter_request_get_quick_singleton(void)
{
	RSFilterRequest *request = NULL;
	GStaticMutex lock = G_STATIC_MUTEX_INIT;

	g_static_mutex_lock(&lock);
	if (!request)
	{
		request = rs_filter_request_new();
		rs_filter_request_set_quick(request, TRUE);
	}
	g_static_mutex_unlock(&lock);

	return request;
}

/**
 * Clone a RSFilterRequest
 * @param filter_request A RSFilterRequest
 * @return A new RSFilterRequest with a refcount of 1 with the same settings as
 *         filter_request
 */
RSFilterRequest *
rs_filter_request_clone(const RSFilterRequest *filter_request)
{
	RSFilterRequest *new_filter_request = rs_filter_request_new();

	if (RS_IS_FILTER_REQUEST(filter_request))
	{
		new_filter_request->roi_set = filter_request->roi_set;
		new_filter_request->roi = filter_request->roi;
		new_filter_request->quick = filter_request->quick;

		rs_filter_param_clone(RS_FILTER_PARAM(new_filter_request), RS_FILTER_PARAM(filter_request));
	}

	return new_filter_request;
}

/**
 * Set a region of interest
 * @param filter_request A RSFilterRequest
 * @param roi A GdkRectangle describing the ROI or NULL to disable
 */
void
rs_filter_request_set_roi(RSFilterRequest *filter_request, GdkRectangle *roi)
{
	g_assert(RS_IS_FILTER_REQUEST(filter_request));

	filter_request->roi_set = FALSE;

	if (roi)
	{
		filter_request->roi_set = TRUE;
		filter_request->roi = *roi;
	}
}

/**
 * Get the region of interest from a RSFilterRequest
 * @param filter_request A RSFilterRequest
 * @return A GdkRectangle describing the ROI or NULL if none is set, the
 *         GdkRectangle belongs to the filter_request and should not be freed
 */
GdkRectangle *
rs_filter_request_get_roi(const RSFilterRequest *filter_request)
{
	GdkRectangle *ret = NULL;

	if (RS_IS_FILTER_REQUEST(filter_request) && filter_request->roi_set)
		ret = &RS_FILTER_REQUEST(filter_request)->roi;

	return ret;
}

/**
 * Mark a request as "quick" allowing filters to priotize speed over quality
 * @param filter_request A RSFilterRequest
 * @param quick TRUE to mark a request as QUICK, FALSE to set normal (default)
 */
void rs_filter_request_set_quick(RSFilterRequest *filter_request, gboolean quick)
{
	g_assert(RS_IS_FILTER_REQUEST(filter_request));

	filter_request->quick = quick;
}

/**
 * Get quick status of a RSFilterRequest
 * @param filter_request A RSFilterRequest
 * @return TRUE if quality should be sacrified for speed, FALSE otherwise
 */
gboolean rs_filter_request_get_quick(const RSFilterRequest *filter_request)
{
	gboolean ret = FALSE;

	if (RS_IS_FILTER_REQUEST(filter_request))
		ret = filter_request->quick;

	return ret;
}
