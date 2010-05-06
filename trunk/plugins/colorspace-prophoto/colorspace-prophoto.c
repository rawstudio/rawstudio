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

/* Plugin tmpl version 4 */

#include <rawstudio.h>
#include "config.h"
#include "gettext.h"

#define RS_TYPE_PROPHOTO (rs_prophoto_type)
#define RS_PROPHOTO(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), RS_TYPE_PROPHOTO, RSProphoto))
#define RS_PROPHOTO_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), RS_TYPE_PROPHOTO, RSProphotoClass))
#define RS_IS_PROPHOTO(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), RS_TYPE_PROPHOTO))
#define RS_PROPHOTO_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), RS_TYPE_PROPHOTO, RSProphotoClass))

typedef struct {
	RSColorSpace parent;
} RSProphoto;

typedef struct {
	RSColorSpaceClass parent_class;

	const RSIccProfile *icc_profile;
	const RSIccProfile *icc_profile_linear;
} RSProphotoClass;

RS_DEFINE_COLOR_SPACE(rs_prophoto, RSProphoto)

static const RSIccProfile *get_icc_profile(const RSColorSpace *color_space, gboolean linear_profile);

G_MODULE_EXPORT void
rs_plugin_load(RSPlugin *plugin)
{
	rs_prophoto_get_type(G_TYPE_MODULE(plugin));
}

static void
rs_prophoto_class_init(RSProphotoClass *klass)
{
	RSColorSpaceClass *colorclass = RS_COLOR_SPACE_CLASS(klass);

	colorclass->get_icc_profile = get_icc_profile;
	colorclass->name = "Linear ProPhoto";
	colorclass->description = _("Large gamut color space");

	klass->icc_profile = rs_icc_profile_new_from_file(PACKAGE_DATA_DIR "/" PACKAGE "/profiles/prophoto.icc");
	klass->icc_profile_linear = rs_icc_profile_new_from_file(PACKAGE_DATA_DIR "/" PACKAGE "/profiles/prophoto-linear.icc");
}

static void
rs_prophoto_init(RSProphoto *prophoto)
{
	/* Source: http://brucelindbloom.com/Eqn_RGB_XYZ_Matrix.html */
	const static RS_MATRIX3 to_pcs = {{
		{ 0.7976749, 0.1351917, 0.0313534 },
		{ 0.2880402, 0.7118741, 0.0000857 },
		{ 0.0000000, 0.0000000, 0.8252100 }
	}};

	rs_color_space_set_matrix_to_pcs(RS_COLOR_SPACE(prophoto), &to_pcs);
}

static const RSIccProfile *
get_icc_profile(const RSColorSpace *color_space, gboolean linear_profile)
{
	RSProphoto *prophoto = RS_PROPHOTO(color_space);

	if (linear_profile)
		return RS_PROPHOTO_GET_CLASS(prophoto)->icc_profile_linear;
	else
		return RS_PROPHOTO_GET_CLASS(prophoto)->icc_profile;
}
