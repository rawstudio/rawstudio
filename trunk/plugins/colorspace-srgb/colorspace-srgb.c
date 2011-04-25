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

/* Color space tmpl version 1 */

#include <math.h> /* pow() */
#include <rawstudio.h>
#include "config.h"
#include "gettext.h"

#define RS_TYPE_SRGB (rs_srgb_type)
#define RS_SRGB(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), RS_TYPE_SRGB, RSSrgb))
#define RS_SRGB_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), RS_TYPE_SRGB, RSSrgbClass))
#define RS_IS_SRGB(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), RS_TYPE_SRGB))
#define RS_SRGB_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), RS_TYPE_SRGB, RSSrgbClass))

typedef struct {
	RSColorSpace parent;
} RSSrgb;

typedef struct {
	RSColorSpaceClass parent_class;

	RSIccProfile *icc_profile;
	RSIccProfile *icc_profile_linear;
} RSSrgbClass;

RS_DEFINE_COLOR_SPACE(rs_srgb, RSSrgb)

static const RSIccProfile *get_icc_profile(const RSColorSpace *color_space, gboolean linear_profile);
static const RS1dFunction *get_gamma_function(const RSColorSpace *color_space);

G_MODULE_EXPORT void
rs_plugin_load(RSPlugin *plugin)
{
	rs_srgb_get_type(G_TYPE_MODULE(plugin));
}

static void
rs_srgb_class_init(RSSrgbClass *klass)
{
	RSColorSpaceClass *colorclass = RS_COLOR_SPACE_CLASS(klass);

	colorclass->name = "sRGB";
	colorclass->description = "";

	colorclass->get_icc_profile = get_icc_profile;
	colorclass->get_gamma_function = get_gamma_function;
	colorclass->is_internal = TRUE;

	klass->icc_profile = rs_icc_profile_new_from_file(PACKAGE_DATA_DIR G_DIR_SEPARATOR_S PACKAGE G_DIR_SEPARATOR_S "profiles" G_DIR_SEPARATOR_S "sRGB.icc");
	klass->icc_profile_linear = rs_icc_profile_new_from_file(PACKAGE_DATA_DIR G_DIR_SEPARATOR_S PACKAGE G_DIR_SEPARATOR_S "profiles" G_DIR_SEPARATOR_S "sRGB-linear.icc");
}

static void
rs_srgb_init(RSSrgb *srgb)
{
	/* Source: http://brucelindbloom.com/Eqn_RGB_XYZ_Matrix.html */
	const static RS_MATRIX3 to_pcs = {{
		{ 0.4360747, 0.3850649, 0.1430804 },
		{ 0.2225045, 0.7168786, 0.0606169 },
		{ 0.0139322, 0.0971045, 0.7141733 },
	}};

	rs_color_space_set_matrix_to_pcs(RS_COLOR_SPACE(srgb), &to_pcs);
}

static const RSIccProfile *
get_icc_profile(const RSColorSpace *color_space, gboolean linear_profile)
{
	RSSrgb *srgb = RS_SRGB(color_space);

	if (linear_profile)
		return RS_SRGB_GET_CLASS(srgb)->icc_profile_linear;
	else
		return RS_SRGB_GET_CLASS(srgb)->icc_profile;
}

/* Gamma */

static gdouble evaluate(const RS1dFunction *func, const gdouble x);
static gdouble evaluate_inverse(const RS1dFunction *func, const gdouble y);

#define RS_TYPE_SRGB_GAMMA rs_srgb_gamma_get_type()
#define RS_SRGB_GAMMA(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), RS_TYPE_SRGB_GAMMA, RSSrgbGamma))
#define RS_SRGB_GAMMA_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), RS_TYPE_SRGB_GAMMA, RSSrgbGammaClass))
#define RS_IS_SRGB_GAMMA(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), RS_TYPE_SRGB_GAMMA))

typedef struct {
	RS1dFunction parent;
} RSSrgbGamma;

typedef struct {
	RS1dFunctionClass parent_class;
} RSSrgbGammaClass;

GType rs_srgb_gamma_get_type(void);

RS1dFunction *rs_srgb_gamma_new(void);

G_DEFINE_TYPE (RSSrgbGamma, rs_srgb_gamma, RS_TYPE_1D_FUNCTION)

static void
rs_srgb_gamma_class_init(RSSrgbGammaClass *klass)
{
	RS1dFunctionClass *fclass = RS_1D_FUNCTION_CLASS(klass);

	fclass->evaluate = evaluate;
	fclass->evaluate_inverse = evaluate_inverse;
}

static void
rs_srgb_gamma_init(RSSrgbGamma *gamma)
{
}

RS1dFunction *
rs_srgb_gamma_new(void)
{
	return RS_1D_FUNCTION(g_object_new(RS_TYPE_SRGB_GAMMA, NULL));
}

static const RS1dFunction *
get_gamma_function(const RSColorSpace *color_space)
{
	static GStaticMutex lock = G_STATIC_MUTEX_INIT;
	static RS1dFunction *func = NULL;

	g_static_mutex_lock(&lock);
	if (!func)
		func = rs_srgb_gamma_new();
	g_static_mutex_unlock(&lock);

	return func;
}

static gdouble
evaluate(const RS1dFunction *func, const gdouble x)
{
	const gdouble junction = 0.0031308;

	if (x <= junction)
		return x * 12.92;
	else
		return 1.055 * pow(x, 1.0/2.4) - 0.055;
}

static gdouble
evaluate_inverse(const RS1dFunction *func, const gdouble y)
{
	const gdouble junction = 0.04045;

	if (y <= junction)
		return y / 12.92;
	else
		return pow((y+0.055)/1.055, 2.4);
}
