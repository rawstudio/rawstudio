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

#include "rs-filter-param.h"

#define RS_TYPE_FLOAT4 rs_float4_get_type()

gpointer
float4_copy(const gpointer boxed)
{
	return g_memdup(boxed, sizeof(gfloat)*4);
}

GType
rs_float4_get_type(void)
{
	static GType type = 0;
	if (!type)
		type = g_boxed_type_register_static(g_intern_static_string("RSFloat4"), float4_copy, g_free);
	return type;
}

G_DEFINE_TYPE(RSFilterParam, rs_filter_param, G_TYPE_OBJECT)

static void
rs_filter_param_dispose(GObject *object)
{
	RSFilterParam *filter_param = RS_FILTER_PARAM(object);

	if (!filter_param->dispose_has_run)
	{
		filter_param->dispose_has_run = TRUE;

		g_hash_table_destroy(filter_param->properties);
	}

	G_OBJECT_CLASS(rs_filter_param_parent_class)->dispose (object);
}

static void
rs_filter_param_finalize(GObject *object)
{
	G_OBJECT_CLASS(rs_filter_param_parent_class)->finalize (object);
}

static void
rs_filter_param_class_init(RSFilterParamClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->dispose = rs_filter_param_dispose;
	object_class->finalize = rs_filter_param_finalize;
}

static inline GValue *
new_value(GType type)
{
	GValue *value  = g_slice_new0(GValue);
	g_value_init(value, type);

	return value;
}

static void
free_value(gpointer data)
{
	GValue *value = (GValue *) data;

	g_value_unset(value);
	g_slice_free(GValue, value);
}

static inline GValue *
clone_value(const GValue *value)
{
	GType type = G_VALUE_TYPE(value);
	GValue *ret = new_value(type);
	g_value_copy(value, ret);

	return ret;
}

static void
rs_filter_param_init(RSFilterParam *param)
{
	param->properties = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, free_value);
}

RSFilterParam *
rs_filter_param_new(void)
{
	return g_object_new (RS_TYPE_FILTER_PARAM, NULL);
}

void
rs_filter_param_clone(RSFilterParam *destination, const RSFilterParam *source)
{
	g_assert(RS_IS_FILTER_PARAM(destination));
	g_assert(RS_IS_FILTER_PARAM(source));

	/* Clone the properties table */
	GHashTableIter iter;
	gpointer key, value;

	g_hash_table_iter_init (&iter, source->properties);
	while (g_hash_table_iter_next (&iter, &key, &value))
		g_hash_table_insert(destination->properties, (gpointer) g_strdup(key), clone_value(value));
}

static void
rs_filter_param_set_gvalue(RSFilterParam *filter_param, const gchar *name, GValue * value)
{
	g_assert(RS_IS_FILTER_PARAM(filter_param));
	g_assert(name != NULL);
	g_assert(name[0] != '\0');

	g_hash_table_insert(filter_param->properties, (gpointer) g_strdup(name), value);
}

static GValue *
rs_filter_param_get_gvalue(const RSFilterParam *filter_param, const gchar *name)
{
	g_assert(RS_IS_FILTER_PARAM(filter_param));

	GValue *value = g_hash_table_lookup(filter_param->properties, name);

	return value;
}

/**
 * Delete a property from a RSFilterParam
 * @param filter_param A RSFilterParam
 * @param name The name of the property
 * @return TRUE if the property was found, FALSE otherwise
 */
gboolean
rs_filter_param_delete(RSFilterParam *filter_param, const gchar *name)
{
	g_assert(RS_IS_FILTER_PARAM(filter_param));

	return g_hash_table_remove(filter_param->properties, name);
}

/**
 * Set a string property
 * @param filter_param A RSFilterParam
 * @param name The name of the property
 * @param str NULL-terminated string to set (will be copied)
 */
void
rs_filter_param_set_string(RSFilterParam *filter_param, const gchar *name, const gchar *str)
{
	GValue *val = new_value(G_TYPE_STRING);
	g_value_set_string(val, str);

	rs_filter_param_set_gvalue(filter_param, name, val);
}

/**
 * Get a string property
 * @param filter_param A RSFilterParam
 * @param name The name of the property
 * @param str A pointer to a string pointer where the value of the property can be saved. Should not be freed
 * @return TRUE if the property was found, FALSE otherwise
 */
gboolean
rs_filter_param_get_string(const RSFilterParam *filter_param, const gchar *name, const gchar ** const str)
{
	GValue *val = rs_filter_param_get_gvalue(filter_param, name);

	if (val && G_VALUE_HOLDS_STRING(val))
		*str = g_value_get_string(val);

	return (val != NULL);
}

/**
 * Set a boolean property
 * @param filter_param A RSFilterParam
 * @param name The name of the property
 * @param value A value to store
 */
void
rs_filter_param_set_boolean(RSFilterParam *filter_param, const gchar *name, const gboolean value)
{
	GValue *val = new_value(G_TYPE_BOOLEAN);
	g_value_set_boolean(val, value);

	rs_filter_param_set_gvalue(filter_param, name, val);
}

/**
 * Get a gboolean property
 * @param filter_param A RSFilterParam
 * @param name The name of the property
 * @param value A pointer to a gboolean where the value will be stored
 * @return TRUE if the property was found, FALSE otherwise
 */
gboolean
rs_filter_param_get_boolean(const RSFilterParam *filter_param, const gchar *name, gboolean *value)
{
	GValue *val = rs_filter_param_get_gvalue(filter_param, name);

	if (val && G_VALUE_HOLDS_BOOLEAN(val))
		*value = g_value_get_boolean(val);

	return (val != NULL);
}

/**
 * Set a integer property
 * @param filter_param A RSFilterParam
 * @param name The name of the property
 * @param value A value to store
 */
void
rs_filter_param_set_integer(RSFilterParam *filter_param, const gchar *name, const gint value)
{
	GValue *val = new_value(G_TYPE_INT);
	g_value_set_int(val, value);

	rs_filter_param_set_gvalue(filter_param, name, val);
}

/**
 * Get a integer property
 * @param filter_param A RSFilterParam
 * @param name The name of the property
 * @param value A pointer to a gint where the value will be stored
 * @return TRUE if the property was found, FALSE otherwise
 */
gboolean
rs_filter_param_get_integer(const RSFilterParam *filter_param, const gchar *name, gint *value)
{
	GValue *val = rs_filter_param_get_gvalue(filter_param, name);

	if (val && G_VALUE_HOLDS_INT(val))
		*value = g_value_get_int(val);

	return (val != NULL);
}

/**
 * Set a float property
 * @param filter_param A RSFilterParam
 * @param name The name of the property
 * @param value A value to store
 */
void
rs_filter_param_set_float(RSFilterParam *filter_param, const gchar *name, const gfloat value)
{
	GValue *val = new_value(G_TYPE_FLOAT);
	g_value_set_float(val, value);

	rs_filter_param_set_gvalue(filter_param, name, val);
}

/**
 * Get a float property
 * @param filter_param A RSFilterParam
 * @param name The name of the property
 * @param value A pointer to a gfloat where the value will be stored
 * @return TRUE if the property was found, FALSE otherwise
 */
gboolean
rs_filter_param_get_float(const RSFilterParam *filter_param, const gchar *name, gfloat *value)
{
	GValue *val = rs_filter_param_get_gvalue(filter_param, name);

	if (val && G_VALUE_HOLDS_FLOAT(val))
		*value = g_value_get_float(val);

	return (val != NULL);
}

/**
 * Set a float[4] property
 * @param filter_param A RSFilterParam
 * @param name The name of the property
 * @param value A value to store
 */
void
rs_filter_param_set_float4(RSFilterParam *filter_param, const gchar *name, const gfloat value[4])
{
	GValue *val = new_value(RS_TYPE_FLOAT4);
	g_value_set_boxed(val, value);

	rs_filter_param_set_gvalue(filter_param, name, val);
}

/**
 * Get a float property
 * @param filter_param A RSFilterParam
 * @param name The name of the property
 * @param value A pointer to a gfloat [4] where the values will be stored
 * @return TRUE if the property was found, FALSE otherwise
 */
gboolean
rs_filter_param_get_float4(const RSFilterParam *filter_param, const gchar *name, gfloat value[4])
{
	typedef gfloat buh[4];
	GValue *val = rs_filter_param_get_gvalue(filter_param, name);

	if (val && G_TYPE_CHECK_VALUE_TYPE(val, RS_TYPE_FLOAT4))
	{
		gfloat *boxed = g_value_get_boxed(val);

		value[0] = boxed[0];
		value[1] = boxed[1];
		value[2] = boxed[2];
		value[3] = boxed[3];
	}

	return (val != NULL);
}

/**
 * Set an object property
 * @param filter_param A RSFilterParam
 * @param name The name of the property
 * @param value An object to store. Refcount will be incremented by one
 */
void
rs_filter_param_set_object(RSFilterParam *filter_param, const gchar *name, gpointer object)
{
	g_return_if_fail(G_IS_OBJECT(object));

	GValue *val = new_value(G_OBJECT_TYPE(object));
	g_value_set_object(val, object);

	rs_filter_param_set_gvalue(filter_param, name, val);
}

/**
 * Get an object property
 * @param filter_param A RSFilterParam
 * @param name The name of the property
 * @return The object if found, NULL otherwise
 */
gpointer
rs_filter_param_get_object(const RSFilterParam *filter_param, const gchar *name)
{
	gpointer object = NULL;
	GValue *val = rs_filter_param_get_gvalue(filter_param, name);

	if (val && G_VALUE_HOLDS_OBJECT(val))
		object = g_value_dup_object(val);

	return object;
}

/**
 * Get an object property
 * @param filter_param A RSFilterParam
 * @param name The name of the property
 * @param type A desired GType, if the type doesn't match, the result is treated as non-existent
 * @return The object if found, NULL otherwise
 */
gpointer
rs_filter_param_get_object_with_type(const RSFilterParam *filter_param, const gchar *name, GType type)
{
	gpointer object = NULL;
	GValue *val = rs_filter_param_get_gvalue(filter_param, name);

	if (val && G_TYPE_CHECK_VALUE_TYPE(val, type))
		object = g_value_dup_object(val);

	return object;
}
