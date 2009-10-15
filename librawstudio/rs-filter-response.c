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

#include "rs-filter-response.h"

struct _RSFilterResponse {
	RSFilterParam parent;
	gboolean dispose_has_run;

	gboolean roi_set;
	GdkRectangle roi;
	gboolean quick;
	RS_IMAGE16 *image;
	GdkPixbuf *image8;

	GHashTable *properties;
};

G_DEFINE_TYPE(RSFilterResponse, rs_filter_response, RS_TYPE_FILTER_PARAM)

static void
rs_filter_response_dispose(GObject *object)
{
	RSFilterResponse *filter_response = RS_FILTER_RESPONSE(object);

	if (!filter_response->dispose_has_run)
	{
		filter_response->dispose_has_run = TRUE;

		if (filter_response->image)
			g_object_unref(filter_response->image);

		if (filter_response->image8)
			g_object_unref(filter_response->image8);

		g_hash_table_destroy(filter_response->properties);
	}

	G_OBJECT_CLASS (rs_filter_response_parent_class)->dispose (object);
}

static void
rs_filter_response_finalize(GObject *object)
{
	G_OBJECT_CLASS (rs_filter_response_parent_class)->finalize (object);
}

static void
rs_filter_response_class_init(RSFilterResponseClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->dispose = rs_filter_response_dispose;
	object_class->finalize = rs_filter_response_finalize;
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
rs_filter_response_init(RSFilterResponse *filter_response)
{
	filter_response->roi_set = FALSE;
	filter_response->quick = FALSE;
	filter_response->image = NULL;
	filter_response->image8 = NULL;
	filter_response->properties = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, free_value);
	filter_response->dispose_has_run = FALSE;
}

/**
 * Instantiate a new RSFilterResponse object
 * @return A new RSFilterResponse with a refcount of 1
 */
RSFilterResponse *
rs_filter_response_new(void)
{
	return g_object_new (RS_TYPE_FILTER_RESPONSE, NULL);
}

/**
 * Clone all flags of a RSFilterResponse EXCEPT images
 * @param filter_response A RSFilterResponse
 * @return A new RSFilterResponse with a refcount of 1
 */
RSFilterResponse *
rs_filter_response_clone(RSFilterResponse *filter_response)
{
	RSFilterResponse *new_filter_response = rs_filter_response_new();

	if (RS_IS_FILTER_RESPONSE(filter_response))
	{
		new_filter_response->roi_set = filter_response->roi_set;
		new_filter_response->roi = filter_response->roi;
		new_filter_response->quick = filter_response->quick;

		/* Clone the properties table */
		GHashTableIter iter;
		gpointer key, value;

		g_hash_table_iter_init (&iter, filter_response->properties);
		while (g_hash_table_iter_next (&iter, &key, &value))
			g_hash_table_insert(new_filter_response->properties, (gpointer) g_strdup(key), clone_value(value));
	}

	return new_filter_response;
}

/**
 * Set the ROI used in generating the response, if the whole image is
 * generated, this should NOT be set
 * @param filter_response A RSFilterResponse
 * @param roi A GdkRectangle describing the ROI or NULL to indicate complete
 *            image data
 */
void
rs_filter_response_set_roi(RSFilterResponse *filter_response, GdkRectangle *roi)
{
	g_assert(RS_IS_FILTER_RESPONSE(filter_response));

	filter_response->roi_set = FALSE;

	if (roi)
	{
		filter_response->roi_set = TRUE;
		filter_response->roi = *roi;
	}
}

/**
 * Get the ROI of the response
 * @param filter_response A RSFilterResponse
 * @return A GdkRectangle describing the ROI or NULL if the complete image is rendered
 */
GdkRectangle *
rs_filter_response_get_roi(const RSFilterResponse *filter_response)
{
	GdkRectangle *ret = NULL;

	g_assert(RS_IS_FILTER_RESPONSE(filter_response));

	if (filter_response->roi_set)
		ret = &RS_FILTER_RESPONSE(filter_response)->roi;

	return ret;
}

/**
 * Set quick flag on a response, this should be set if the image has been
 * rendered by any quick method and a better method is available
 * @note There is no boolean parameter, it would make no sense to remove a
 *       quick-flag
 * @param filter_response A RSFilterResponse
 */
void
rs_filter_response_set_quick(RSFilterResponse *filter_response)
{
	g_assert(RS_IS_FILTER_RESPONSE(filter_response));

	filter_response->quick = TRUE;
}

/**
 * Get the quick flag
 * @param filter_response A RSFilterResponse
 * @return TRUE if the image data was rendered using a "quick" algorithm and a
 *         faster is available, FALSE otherwise
 */
gboolean
rs_filter_response_get_quick(const RSFilterResponse *filter_response)
{
	g_assert(RS_IS_FILTER_RESPONSE(filter_response));

	return filter_response->quick;
}

/**
 * Set 16 bit image data
 * @param filter_response A RSFilterResponse
 * @param image A RS_IMAGE16
 */
void
rs_filter_response_set_image(RSFilterResponse *filter_response, RS_IMAGE16 *image)
{
	g_assert(RS_IS_FILTER_RESPONSE(filter_response));

	if (filter_response->image)
	{
		g_object_unref(filter_response->image);
		filter_response->image = NULL;
	}

	if (image)
		filter_response->image = g_object_ref(image);
}

/**
 * Is there a 16 bit image attached
 * @param filter_response A RSFilterResponse
 * @return A RS_IMAGE16 (must be unreffed after usage) or NULL if none is set
 */
 gboolean rs_filter_response_has_image(const RSFilterResponse *filter_response)
{
	g_assert(RS_IS_FILTER_RESPONSE(filter_response));

	return !!filter_response->image;
}

/**
 * Get 16 bit image data
 * @param filter_response A RSFilterResponse
 * @return A RS_IMAGE16 (must be unreffed after usage) or NULL if none is set
 */
RS_IMAGE16 *
rs_filter_response_get_image(const RSFilterResponse *filter_response)
{
	RS_IMAGE16 *ret = NULL;

	g_assert(RS_IS_FILTER_RESPONSE(filter_response));

	if (filter_response->image)
		ret = g_object_ref(filter_response->image);

	return ret;
}

/**
 * Set 8 bit image data
 * @param filter_response A RSFilterResponse
 * @param pixbuf A GdkPixbuf
 */
void
rs_filter_response_set_image8(RSFilterResponse *filter_response, GdkPixbuf *pixbuf)
{
	g_assert(RS_IS_FILTER_RESPONSE(filter_response));

	if (filter_response->image8)
	{
		g_object_unref(filter_response->image8);
		filter_response->image8 = NULL;
	}

	if (pixbuf)
		filter_response->image8 = g_object_ref(pixbuf);
}

/**
 * Does the response have an 8 bit image
 * @param filter_response A RSFilterResponse
 * @return A gboolean TRUE if an image8 is attached, FALSE otherwise
 */
gboolean rs_filter_response_has_image8(const RSFilterResponse *filter_response) 
{
	g_assert(RS_IS_FILTER_RESPONSE(filter_response));

	return !!filter_response->image8;
}

/**
 * Get 8 bit image data
 * @param filter_response A RSFilterResponse
 * @return A GdkPixbuf (must be unreffed after usage) or NULL if none is set
 */
GdkPixbuf *
rs_filter_response_get_image8(const RSFilterResponse *filter_response)
{
	GdkPixbuf *ret = NULL;

	g_assert(RS_IS_FILTER_RESPONSE(filter_response));

	if (filter_response->image8)
		ret = g_object_ref(filter_response->image8);

	return ret;
}

static void
rs_filter_response_set_gvalue(const RSFilterResponse *filter_response, const gchar *name, GValue * value)
{
	g_assert(RS_IS_FILTER_RESPONSE(filter_response));
	g_assert(name != NULL);
	g_assert(name[0] != '\0');

	g_hash_table_insert(filter_response->properties, (gpointer) g_strdup(name), value);
}

static GValue *
rs_filter_response_get_gvalue(const RSFilterResponse *filter_response, const gchar *name)
{
	g_assert(RS_IS_FILTER_RESPONSE(filter_response));

	GValue *value = g_hash_table_lookup(filter_response->properties, name);

	return value;
}

/**
 * Set a string property
 * @param filter_response A RSFilterResponse
 * @param name The name of the property
 * @param str NULL-terminated string to set (will be copied)
 */
void
rs_filter_response_set_string(const RSFilterResponse *filter_response, const gchar *name, const gchar *str)
{
	GValue *val = new_value(G_TYPE_STRING);
	g_value_set_string(val, str);

	rs_filter_response_set_gvalue(filter_response, name, val);
}

/**
 * Get a string property
 * @param filter_response A RSFilterResponse
 * @param name The name of the property
 * @param str A pointer to a string pointer where the value of the property can be saved. Should not be freed
 * @return TRUE if the property was found, FALSE otherwise
 */
gboolean
rs_filter_response_get_string(const RSFilterResponse *filter_response, const gchar *name, const gchar ** const str)
{
	GValue *val = rs_filter_response_get_gvalue(filter_response, name);

	if (val && G_VALUE_HOLDS_STRING(val))
		*str = g_value_get_string(val);

	return (val != NULL);
}

/**
 * Set a float property
 * @param filter_response A RSFilterResponse
 * @param name The name of the property
 * @param value A value to store
 */
void
rs_filter_response_set_float(const RSFilterResponse *filter_response, const gchar *name, const gfloat value)
{
	GValue *val = new_value(G_TYPE_FLOAT);
	g_value_set_float(val, value);

	rs_filter_response_set_gvalue(filter_response, name, val);
}

/**
 * Get a float property
 * @param filter_response A RSFilterResponse
 * @param name The name of the property
 * @param value A pointer to a gfloat where the value will be stored
 * @return TRUE if the property was found, FALSE otherwise
 */
gboolean
rs_filter_response_get_float(const RSFilterResponse *filter_response, const gchar *name, gfloat *value)
{
	GValue *val = rs_filter_response_get_gvalue(filter_response, name);

	if (val && G_VALUE_HOLDS_FLOAT(val))
		*value = g_value_get_float(val);

	return (val != NULL);
}
