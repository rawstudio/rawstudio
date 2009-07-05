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

#ifndef RS_FILTER_PARAM_H
#define RS_FILTER_PARAM_H

#include <glib-object.h>

G_BEGIN_DECLS

#define RS_TYPE_FILTER_PARAM rs_filter_param_get_type()
#define RS_FILTER_PARAM(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), RS_TYPE_FILTER_PARAM, RSFilterParam))
#define RS_FILTER_PARAM_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), RS_TYPE_FILTER_PARAM, RSFilterParamClass))
#define RS_IS_FILTER_PARAM(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), RS_TYPE_FILTER_PARAM))
#define RS_IS_FILTER_PARAM_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), RS_TYPE_FILTER_PARAM))
#define RS_FILTER_PARAM_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), RS_TYPE_FILTER_PARAM, RSFilterParamClass))

typedef struct _RSFilterParam RSFilterParam;

typedef struct {
	GObjectClass parent_class;
} RSFilterParamClass;

GType rs_filter_param_get_type(void);

/**
 * Instantiate a new RSFilterParam
 * @return A new RSFilterParam with a refcount of 1
 */
RSFilterParam *rs_filter_param_new(void);

/**
 * Clone a RSFilterParam
 * @param filter_param A RSFilterParam
 * @return A new RSFilterParam with a refcount of 1 with the same settings as
 *         filter_param
 */
RSFilterParam *rs_filter_param_clone(const RSFilterParam *filter_param);

/**
 * Set a region of interest
 * @param filter_param A RSFilterParam
 * @param roi A GdkRectangle describing the ROI or NULL to disable
 */
void rs_filter_param_set_roi(RSFilterParam *filter_param, GdkRectangle *roi);

/**
 * Get the region of interest from a RSFilterParam
 * @param filter_param A RSFilterParam
 * @return A GdkRectangle describing the ROI or NULL if none is set, the
 *         GdkRectangle belongs to the filter_param and should not be freed
 */
GdkRectangle *rs_filter_param_get_roi(const RSFilterParam *filter_param);

G_END_DECLS

#endif /* RS_FILTER_PARAM_H */
