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

#include "rawstudio.h"

G_DEFINE_TYPE (RSColorSpace, rs_color_space, G_TYPE_OBJECT)

static void
rs_color_space_class_init(RSColorSpaceClass *klass)
{
}

static void
rs_color_space_init(RSColorSpace *color_space)
{
	matrix3_identity(&color_space->matrix_to_pcs);
	matrix3_identity(&color_space->matrix_from_pcs);
}

/**
 * Get a color space definition
 * @param name The GType name for the colorspace (not the registered name)
 * @return A colorspace singleton if found, NULL otherwise. This should not be unreffed.
 */
RSColorSpace *
rs_color_space_new_singleton(const gchar *name)
{
	RSColorSpace *color_space = NULL;
	static GHashTable *singletons = NULL;
	static GStaticMutex lock = G_STATIC_MUTEX_INIT;

	g_assert(name != NULL);

	g_static_mutex_lock(&lock);

	if (!singletons)
		singletons = g_hash_table_new(g_str_hash, g_str_equal);

	color_space = g_hash_table_lookup(singletons, name);
	if (!color_space)
	{
		GType type = g_type_from_name(name);
		if (g_type_is_a (type, RS_TYPE_COLOR_SPACE))
			color_space = g_object_new(type, NULL);

		if (!RS_IS_COLOR_SPACE(color_space))
			g_warning("Could not instantiate color space of type \"%s\"", name);
		else
			g_hash_table_insert(singletons, (gpointer) name, color_space);
	}

	g_static_mutex_unlock(&lock);

	return color_space;
}

/**
 * Set (RGB) to PCS matrix
 * @note This is only interesting for color space implementations
 * @param color_space A RSColorSpace
 * @param matrix A matrix, xyz2rgb will be the inverse of this
 */
void
rs_color_space_set_matrix_to_pcs(RSColorSpace *color_space, const RS_MATRIX3 * const matrix)
{
	g_assert(RS_IS_COLOR_SPACE(color_space));

	/* Could this be replaced by bradford? */
	const RS_VECTOR3 identity = {1.0, 1.0, 1.0};
	const RS_VECTOR3 w1 = vector3_multiply_matrix(&identity, matrix);
	const RS_VECTOR3 w2 = XYZ_WP_D50;

	const RS_VECTOR3 scale_vector = { w2.x/w1.x, w2.y/w1.y, w2.z/w2.z };
	const RS_MATRIX3 scale = vector3_as_diagonal(&scale_vector);

	matrix3_multiply(&scale, matrix, &color_space->matrix_to_pcs);
	color_space->matrix_from_pcs = matrix3_invert(&color_space->matrix_to_pcs);
}

/**
 * Get a matrix that will transform this color space to PCS
 * @param color_space A RSColorSpace
 * @return from_pcs matrix
 */
RS_MATRIX3
rs_color_space_get_matrix_to_pcs(const RSColorSpace *color_space)
{
	g_assert(RS_IS_COLOR_SPACE(color_space));

	return color_space->matrix_from_pcs;
}

/**
 * Get a matrix that will transform PCS to this color space
 * @param color_space A RSColorSpace
 * @return to_pcs matrix
 */
RS_MATRIX3
rs_color_space_get_matrix_from_pcs(const RSColorSpace *color_space)
{
	g_assert(RS_IS_COLOR_SPACE(color_space));

	return color_space->matrix_to_pcs;
}

/**
 * Get the ICC profile for this colorspace if any
 * @param color_space A RSColorSpace
 * @return A RSIccProfile (or NULL) that should not be unreffed
 */
const RSIccProfile *
rs_color_space_get_icc_profile(const RSColorSpace *color_space)
{
	RSColorSpaceClass *klass = RS_COLOR_SPACE_GET_CLASS(color_space);

	if (klass->get_icc_profile)
		return klass->get_icc_profile(color_space);
	else
		return NULL;
}

/**
 * Get the gamma transfer function for this color space
 * @param color_space A RSColorSpace
 * @return A RS1dFunction that should not be unreffed
 */
const RS1dFunction *
rs_color_space_get_gamma_function(const RSColorSpace *color_space)
{
	RSColorSpaceClass *klass = RS_COLOR_SPACE_GET_CLASS(color_space);

	if (klass->get_gamma_function)
		return klass->get_gamma_function(color_space);
	else
		return rs_1d_function_new_singleton();
}
