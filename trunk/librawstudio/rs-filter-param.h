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

typedef struct {
	GObject parent;
	gboolean dispose_has_run;

	GHashTable *properties;
} RSFilterParam;

typedef struct {
	GObjectClass parent_class;
} RSFilterParamClass;

GType rs_filter_param_get_type(void);

RSFilterParam *rs_filter_param_new(void);

void
rs_filter_param_clone(RSFilterParam *destination, const RSFilterParam *source);

/**
 * Delete a property from a RSFilterParam
 * @param filter_param A RSFilterParam
 * @param name The name of the property
 * @return TRUE if the property was found, FALSE otherwise
 */
gboolean
rs_filter_param_delete(RSFilterParam *filter_param, const gchar *name);

/**
 * Set a string property
 * @param filter_param A RSFilterParam
 * @param name The name of the property
 * @param str NULL-terminated string to set (will be copied)
 */
void rs_filter_param_set_string(RSFilterParam *filter_param, const gchar *name, const gchar *str);

/**
 * Get a string property
 * @param filter_param A RSFilterParam
 * @param name The name of the property
 * @param str A pointer to a string pointer where the value of the property can be saved. Should not be freed
 * @return TRUE if the property was found, FALSE otherwise
 */
gboolean rs_filter_param_get_string(const RSFilterParam *filter_param, const gchar *name, const gchar ** const str);

/**
 * Set a boolean property
 * @param filter_param A RSFilterParam
 * @param name The name of the property
 * @param value A value to store
 */
void
rs_filter_param_set_boolean(RSFilterParam *filter_param, const gchar *name, const gboolean value);

/**
 * Get a gboolean property
 * @param filter_param A RSFilterParam
 * @param name The name of the property
 * @param value A pointer to a gboolean where the value will be stored
 * @return TRUE if the property was found, FALSE otherwise
 */
gboolean rs_filter_param_get_boolean(const RSFilterParam *filter_param, const gchar *name, gboolean *value);

/**
 * Set a integer property
 * @param filter_param A RSFilterParam
 * @param name The name of the property
 * @param value A value to store
 */
void
rs_filter_param_set_integer(RSFilterParam *filter_param, const gchar *name, const gint value);

/**
 * Get a integer property
 * @param filter_param A RSFilterParam
 * @param name The name of the property
 * @param value A pointer to a gint where the value will be stored
 * @return TRUE if the property was found, FALSE otherwise
 */
gboolean rs_filter_param_get_integer(const RSFilterParam *filter_param, const gchar *name, gint *value);

/**
 * Set a float property
 * @param filter_param A RSFilterParam
 * @param name The name of the property
 * @param value A value to store
 */
void
rs_filter_param_set_float(RSFilterParam *filter_param, const gchar *name, const gfloat value);

/**
 * Get a float property
 * @param filter_param A RSFilterParam
 * @param name The name of the property
 * @param value A pointer to a gfloat where the value will be stored
 * @return TRUE if the property was found, FALSE otherwise
 */
gboolean rs_filter_param_get_float(const RSFilterParam *filter_param, const gchar *name, gfloat *value);

/**
 * Set a float[4] property
 * @param filter_param A RSFilterParam
 * @param name The name of the property
 * @param value A value to store
 */
void
rs_filter_param_set_float4(RSFilterParam *filter_param, const gchar *name, const gfloat value[4]);

/**
 * Get a float property
 * @param filter_param A RSFilterParam
 * @param name The name of the property
 * @param value A pointer to a gfloat [4] where the values will be stored
 * @return TRUE if the property was found, FALSE otherwise
 */
gboolean rs_filter_param_get_float4(const RSFilterParam *filter_param, const gchar *name, gfloat value[4]);

/**
 * Set an object property
 * @param filter_param A RSFilterParam
 * @param name The name of the property
 * @param object An object to store. Refcount will be incremented by one
 */
void rs_filter_param_set_object(RSFilterParam *filter_param, const gchar *name, gpointer object);

/**
 * Get an object property
 * @param filter_param A RSFilterParam
 * @param name The name of the property
 * @return The object if found, NULL otherwise
 */
gpointer rs_filter_param_get_object(const RSFilterParam *filter_param, const gchar *name);

/**
 * Get an object property
 * @param filter_param A RSFilterParam
 * @param name The name of the property
 * @param type A desired GType, if the type doesn't match, the result is treated as non-existent
 * @return The object if found, NULL otherwise
 */
gpointer rs_filter_param_get_object_with_type(const RSFilterParam *filter_param, const gchar *name, GType type);

G_END_DECLS

#endif /* RS_FILTER_PARAM_H */
