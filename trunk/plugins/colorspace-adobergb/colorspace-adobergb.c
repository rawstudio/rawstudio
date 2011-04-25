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

/* Plugin tmpl version 4 */

#include <rawstudio.h>
#include "config.h"
#include "gettext.h"
#include <math.h> /* pow() */

#define RS_TYPE_ADOBERGB (rs_adobe_rgb_type)
#define RS_ADOBERGB(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), RS_TYPE_ADOBERGB, RSAdobeRGB))
#define RS_ADOBERGB_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), RS_TYPE_ADOBERGB, RSAdobeRGBClass))
#define RS_IS_ADOBERGB(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), RS_TYPE_ADOBERGB))
#define RS_ADOBERGB_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), RS_TYPE_ADOBERGB, RSAdobeRGBClass))

typedef struct {
	RSColorSpace parent;
} RSAdobeRGB;

typedef struct {
	RSColorSpaceClass parent_class;

	const RSIccProfile *icc_profile;
	const RSIccProfile *icc_profile_linear;
} RSAdobeRGBClass;

RS_DEFINE_COLOR_SPACE(rs_adobe_rgb, RSAdobeRGB)

static const RSIccProfile *get_icc_profile(const RSColorSpace *color_space, gboolean linear_profile);
static const RS1dFunction *get_gamma_function(const RSColorSpace *color_space);

G_MODULE_EXPORT void
rs_plugin_load(RSPlugin *plugin)
{
	rs_adobe_rgb_get_type(G_TYPE_MODULE(plugin));
}

static void
rs_adobe_rgb_class_init(RSAdobeRGBClass *klass)
{
	RSColorSpaceClass *colorclass = RS_COLOR_SPACE_CLASS(klass);

	colorclass->get_icc_profile = get_icc_profile;
	colorclass->name = "Adobe RGB (1998) Compatible";
	colorclass->description = _("Print friendly color space, compatible with Adobe RGB (1998)");
	colorclass->get_gamma_function = get_gamma_function;
	colorclass->is_internal = TRUE;

	klass->icc_profile = rs_icc_profile_new_from_file(PACKAGE_DATA_DIR G_DIR_SEPARATOR_S PACKAGE G_DIR_SEPARATOR_S "profiles" G_DIR_SEPARATOR_S "compatibleWithAdobeRGB1998.icc");
	klass->icc_profile_linear = rs_icc_profile_new_from_file(PACKAGE_DATA_DIR G_DIR_SEPARATOR_S PACKAGE G_DIR_SEPARATOR_S "profiles" G_DIR_SEPARATOR_S "compatibleWithAdobeRGB1998-linear.icc");
}

static void
rs_adobe_rgb_init(RSAdobeRGB *adobe_rgb)
{
	/* Source: http://brucelindbloom.com/Eqn_RGB_XYZ_Matrix.html */
	/* Internal XYZ transformations are using D50 reference white */
	const static RS_MATRIX3 to_pcs = {{
		{ 0.6097559, 0.2052401, 0.1492240 },
		{ 0.3111242, 0.6256560, 0.0632197 },
		{ 0.0194811, 0.0608902, 0.7448387 }
	}};

	rs_color_space_set_matrix_to_pcs(RS_COLOR_SPACE(adobe_rgb), &to_pcs);
}

static const RSIccProfile *
get_icc_profile(const RSColorSpace *color_space, gboolean linear_profile)
{
	RSAdobeRGB *adobe_rgb = RS_ADOBERGB(color_space);

	if (linear_profile)
		return RS_ADOBERGB_GET_CLASS(adobe_rgb)->icc_profile_linear;
	else
		return RS_ADOBERGB_GET_CLASS(adobe_rgb)->icc_profile;
}

/* Gamma */

static gdouble evaluate(const RS1dFunction *func, const gdouble x);
static gdouble evaluate_inverse(const RS1dFunction *func, const gdouble y);

#define RS_TYPE_ADOBE_GAMMA rs_adobe_gamma_get_type()
#define RS_ADOBE_GAMMA(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), RS_TYPE_ADOBE_GAMMA, RSAdobeGamma))
#define RS_ADOBE_GAMMA_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), RS_TYPE_ADOBE_GAMMA, RSAdobeGammaClass))
#define RS_IS_ADOBE_GAMMA(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), RS_TYPE_ADOBE_GAMMA))

typedef struct {
	RS1dFunction parent;
} RSAdobeGamma;

typedef struct {
	RS1dFunctionClass parent_class;
} RSAdobeGammaClass;

GType rs_adobe_gamma_get_type(void);

RS1dFunction *rs_adobe_gamma_new(void);

G_DEFINE_TYPE (RSAdobeGamma, rs_adobe_gamma, RS_TYPE_1D_FUNCTION)

static void
rs_adobe_gamma_class_init(RSAdobeGammaClass *klass)
{
	RS1dFunctionClass *fclass = RS_1D_FUNCTION_CLASS(klass);

	fclass->evaluate = evaluate;
	fclass->evaluate_inverse = evaluate_inverse;
}

static void
rs_adobe_gamma_init(RSAdobeGamma *gamma)
{
}

RS1dFunction *
rs_adobe_gamma_new(void)
{
	return RS_1D_FUNCTION(g_object_new(RS_TYPE_ADOBE_GAMMA, NULL));
}

static const RS1dFunction *
get_gamma_function(const RSColorSpace *color_space)
{
	static GStaticMutex lock = G_STATIC_MUTEX_INIT;
	static RS1dFunction *func = NULL;

	g_static_mutex_lock(&lock);
	if (!func)
		func = rs_adobe_gamma_new();
	g_static_mutex_unlock(&lock);

	return func;
}

static gdouble
evaluate(const RS1dFunction *func, const gdouble x)
{
	return pow(x, 1.0/2.2);
}

static gdouble
evaluate_inverse(const RS1dFunction *func, const gdouble y)
{
	return pow(y, 2.2);
}
