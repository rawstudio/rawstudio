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

#include "rawstudio.h"

G_DEFINE_TYPE(RSColorSpaceIcc, rs_color_space_icc, RS_TYPE_COLOR_SPACE)

static const RSIccProfile *get_icc_profile(const RSColorSpace *color_space, gboolean linear_profile);

static void
rs_color_space_icc_dispose(GObject *object)
{
	RSColorSpaceIcc *color_space_icc = RS_COLOR_SPACE_ICC(object);

	if (!color_space_icc->dispose_has_run)
	{
		color_space_icc->dispose_has_run = TRUE;
		if (color_space_icc->icc_profile)
			g_object_unref(color_space_icc->icc_profile);
	}
	G_OBJECT_CLASS(rs_color_space_icc_parent_class)->dispose(object);
}

static void
rs_color_space_icc_class_init(RSColorSpaceIccClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS(klass);
	object_class->dispose = rs_color_space_icc_dispose;
	RSColorSpaceClass *colorclass = RS_COLOR_SPACE_CLASS(klass);

	colorclass->get_icc_profile = get_icc_profile;
	colorclass->name = "ICC derived color space";
	colorclass->description = "ICC derived color space";
}

static void
rs_color_space_icc_init(RSColorSpaceIcc *color_space_icc)
{
}

RSColorSpace *
rs_color_space_icc_new_from_icc(RSIccProfile *icc_profile)
{
	RSColorSpaceIcc *color_space_icc = g_object_new(RS_TYPE_COLOR_SPACE_ICC, NULL);

	if (RS_IS_ICC_PROFILE(icc_profile))
	{
		color_space_icc->icc_profile = g_object_ref(icc_profile);
		/* FIXME: Some profiles will be nothing more than a fancy container
		 * for a color spaces definition, we should recognize those cases and
		 * try to convert them to RSColorSpace without the need for a CMS */
		RS_COLOR_SPACE(color_space_icc)->flags |= RS_COLOR_SPACE_FLAG_REQUIRES_CMS;
	}

	return RS_COLOR_SPACE(color_space_icc);
}

RSColorSpace *
rs_color_space_icc_new_from_file(const gchar *path)
{
	RSIccProfile *icc_profile = rs_icc_profile_new_from_file(path);
	return rs_color_space_icc_new_from_icc(icc_profile);
}

static const
RSIccProfile *get_icc_profile(const RSColorSpace *color_space, gboolean linear_profile)
{
	RSColorSpaceIcc *color_space_icc = RS_COLOR_SPACE_ICC(color_space);

	return color_space_icc->icc_profile;
}
