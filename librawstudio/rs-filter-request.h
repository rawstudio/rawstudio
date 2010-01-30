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

#ifndef RS_FILTER_REQUEST_H
#define RS_FILTER_REQUEST_H

#include <glib-object.h>
#include "rs-filter-param.h"

G_BEGIN_DECLS

#define RS_TYPE_FILTER_REQUEST rs_filter_request_get_type()
#define RS_FILTER_REQUEST(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), RS_TYPE_FILTER_REQUEST, RSFilterRequest))
#define RS_FILTER_REQUEST_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), RS_TYPE_FILTER_REQUEST, RSFilterRequestClass))
#define RS_IS_FILTER_REQUEST(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), RS_TYPE_FILTER_REQUEST))
#define RS_IS_FILTER_REQUEST_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), RS_TYPE_FILTER_REQUEST))
#define RS_FILTER_REQUEST_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), RS_TYPE_FILTER_REQUEST, RSFilterRequestClass))

typedef struct _RSFilterRequest RSFilterRequest;

typedef struct {
	RSFilterParamClass parent_class;
} RSFilterRequestClass;

GType rs_filter_request_get_type(void);

/**
 * Instantiate a new RSFilterRequest
 * @return A new RSFilterRequest with a refcount of 1
 */
RSFilterRequest *rs_filter_request_new(void);

/**
 * Clone a RSFilterRequest
 * @param filter_request A RSFilterRequest
 * @return A new RSFilterRequest with a refcount of 1 with the same settings as
 *         filter_request
 */
RSFilterRequest *rs_filter_request_clone(const RSFilterRequest *filter_request);

/**
 * Set a region of interest
 * @param filter_request A RSFilterRequest
 * @param roi A GdkRectangle describing the ROI or NULL to disable
 */
void rs_filter_request_set_roi(RSFilterRequest *filter_request, GdkRectangle *roi);

/**
 * Get the region of interest from a RSFilterRequest
 * @param filter_request A RSFilterRequest
 * @return A GdkRectangle describing the ROI or NULL if none is set, the
 *         GdkRectangle belongs to the filter_request and should not be freed
 */
GdkRectangle *rs_filter_request_get_roi(const RSFilterRequest *filter_request);

/**
 * Mark a request as "quick" allowing filters to priotize speed over quality
 * @param filter_request A RSFilterRequest
 * @param quick TRUE to mark a request as QUICK, FALSE to set normal (default)
 */
void rs_filter_request_set_quick(RSFilterRequest *filter_request, gboolean quick);

/**
 * Get quick status of a RSFilterRequest
 * @param filter_request A RSFilterRequest
 * @return TRUE if quality should be sacrified for speed, FALSE otherwise
 */
gboolean rs_filter_request_get_quick(const RSFilterRequest *filter_request);

G_END_DECLS

#endif /* RS_FILTER_REQUEST_H */
