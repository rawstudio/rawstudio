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

#include <gtk/gtk.h>
#include "rs-filter-param.h"

struct _RSFilterParam {
	GObject parent;
	gboolean roi_set;
	GdkRectangle roi;
};

G_DEFINE_TYPE(RSFilterParam, rs_filter_param, G_TYPE_OBJECT)

static void
rs_filter_param_finalize(GObject *object)
{
	G_OBJECT_CLASS (rs_filter_param_parent_class)->finalize (object);
}

static void
rs_filter_param_class_init(RSFilterParamClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = rs_filter_param_finalize;
}

static void
rs_filter_param_init(RSFilterParam *filter_param)
{
	filter_param->roi_set = FALSE;
}

/**
 * Instantiate a new RSFilterParam
 * @return A new RSFilterParam with a refcount of 1
 */
RSFilterParam *
rs_filter_param_new(void)
{
	return g_object_new(RS_TYPE_FILTER_PARAM, NULL);
}

/**
 * Clone a RSFilterParam
 * @param filter_param A RSFilterParam
 * @return A new RSFilterParam with a refcount of 1 with the same settings as
 *         filter_param
 */
RSFilterParam *
rs_filter_param_clone(const RSFilterParam *filter_param)
{
	RSFilterParam *new_filter_param = rs_filter_param_new();

	if (RS_IS_FILTER_PARAM(filter_param))
	{
		new_filter_param->roi_set = filter_param->roi_set;
		new_filter_param->roi = filter_param->roi;
	}

	return new_filter_param;
}

/**
 * Set a region of interest
 * @param filter_param A RSFilterParam
 * @param roi A GdkRectangle describing the ROI or NULL to disable
 */
void
rs_filter_param_set_roi(RSFilterParam *filter_param, GdkRectangle *roi)
{
	g_assert(RS_IS_FILTER_PARAM(filter_param));

	filter_param->roi_set = FALSE;

	if (roi)
	{
		filter_param->roi_set = TRUE;
		filter_param->roi = *roi;
	}
}

/**
 * Get the region of interest from a RSFilterParam
 * @param filter_param A RSFilterParam
 * @return A GdkRectangle describing the ROI or NULL if none is set, the
 *         GdkRectangle belongs to the filter_param and should not be freed
 */
GdkRectangle *
rs_filter_param_get_roi(const RSFilterParam *filter_param)
{
	GdkRectangle *ret = NULL;

	if (RS_IS_FILTER_PARAM(filter_param) && filter_param->roi_set)
		ret = &RS_FILTER_PARAM(filter_param)->roi;

	return ret;
}
