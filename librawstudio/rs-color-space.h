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

#ifndef RS_COLOR_SPACE_H
#define RS_COLOR_SPACE_H

#include <glib-object.h>

G_BEGIN_DECLS

#define RS_DEFINE_COLOR_SPACE(type_name, TypeName) \
static GType type_name##_get_type (GTypeModule *module); \
static void type_name##_class_init(TypeName##Class *klass); \
static void type_name##_init(TypeName *color_space); \
static GType type_name##_type = 0; \
static GType \
type_name##_get_type(GTypeModule *module) \
{ \
	if (!type_name##_type) \
	{ \
		static const GTypeInfo color_space_info = \
		{ \
			sizeof (TypeName##Class), \
			(GBaseInitFunc) NULL, \
			(GBaseFinalizeFunc) NULL, \
			(GClassInitFunc) type_name##_class_init, \
			NULL, \
			NULL, \
			sizeof (TypeName), \
			0, \
			(GInstanceInitFunc) type_name##_init \
		}; \
 \
		type_name##_type = g_type_module_register_type( \
			module, \
			RS_TYPE_COLOR_SPACE, \
			#TypeName, \
			&color_space_info, \
			0); \
	} \
	return type_name##_type; \
}

#define RS_TYPE_COLOR_SPACE rs_color_space_get_type()
#define RS_COLOR_SPACE(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), RS_TYPE_COLOR_SPACE, RSColorSpace))
#define RS_COLOR_SPACE_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), RS_TYPE_COLOR_SPACE, RSColorSpaceClass))
#define RS_IS_COLOR_SPACE(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), RS_TYPE_COLOR_SPACE))
#define RS_IS_COLOR_SPACE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), RS_TYPE_COLOR_SPACE))
#define RS_COLOR_SPACE_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), RS_TYPE_COLOR_SPACE, RSColorSpaceClass))

typedef enum {
	RS_COLOR_SPACE_FLAG_REQUIRES_CMS = 1
} RSColorSpaceFlag;

typedef struct {
	GObject parent;

	RSColorSpaceFlag flags;
	RS_MATRIX3 matrix_to_pcs;
	RS_MATRIX3 matrix_from_pcs;
} RSColorSpace;

typedef struct {
	GObjectClass parent_class;

	const gchar *name;
	const gchar *description;

	const RSIccProfile *(*get_icc_profile)(const RSColorSpace *color_space);
	const RS1dFunction *(*get_gamma_function)(const RSColorSpace *color_space);
} RSColorSpaceClass;

#define RS_COLOR_SPACE_REQUIRES_CMS(color_space) (!!((color_space)->flags & RS_COLOR_SPACE_FLAG_REQUIRES_CMS))

GType rs_color_space_get_type(void);

/**
 * Get a color space definition
 * @param name The GType name for the colorspace (not the registered name)
 * @return A colorspace singleton if found, NULL otherwise. This should not be unreffed.
 */
RSColorSpace *
rs_color_space_new_singleton(const gchar *name);

/**
 * Set (RGB) to PCS matrix
 * @note This is only interesting for color space implementations
 * @param color_space A RSColorSpace
 * @param matrix A matrix, xyz2rgb will be the inverse of this
 */
void
rs_color_space_set_matrix_to_pcs(RSColorSpace *color_space, const RS_MATRIX3 * const matrix);

/**
 * Get a matrix that will transform this color space to PCS
 * @param color_space A RSColorSpace
 * @return from_pcs matrix
 */
RS_MATRIX3
rs_color_space_get_matrix_to_pcs(const RSColorSpace *color_space);

/**
 * Get a matrix that will transform PCS to this color space
 * @param color_space A RSColorSpace
 * @return to_pcs matrix
 */
RS_MATRIX3
rs_color_space_get_matrix_from_pcs(const RSColorSpace *color_space);

/**
 * Get the ICC profile for this colorspace if any
 * @param color_space A RSColorSpace
 * @return A RSIccProfile (or NULL) that should not be unreffed
 */
const RSIccProfile *
rs_color_space_get_icc_profile(const RSColorSpace *color_space);

/**
 * Get the gamma transfer function for this color space
 * @param color_space A RSColorSpace
 * @return A RS1dFunction that should not be unreffed
 */
const RS1dFunction *
rs_color_space_get_gamma_function(const RSColorSpace *color_space);

G_END_DECLS

#endif /* RS_COLOR_SPACE_H */
