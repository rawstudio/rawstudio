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

#include <rawstudio.h>

struct _RSLens {
	GObject parent;
	gboolean dispose_has_run;

	gchar *description;
	gdouble min_focal;
	gdouble max_focal;
	gdouble min_aperture;
	gdouble max_aperture;
	gchar *identifier;
	gchar *lensfun_make;
	gchar *lensfun_model;
	gchar *camera_make;
	gchar *camera_model;
	gboolean enabled;
};

G_DEFINE_TYPE (RSLens, rs_lens, G_TYPE_OBJECT)

enum {
	PROP_0,
	PROP_DESCRIPTION,
	PROP_MIN_FOCAL,
	PROP_MAX_FOCAL,
	PROP_MIN_APERTURE,
	PROP_MAX_APERTURE,
	PROP_IDENTIFIER,
	PROP_LENSFUN_MAKE,
	PROP_LENSFUN_MODEL,
	PROP_CAMERA_MAKE,
	PROP_CAMERA_MODEL,
	PROP_ENABLED
};

static void
get_property(GObject *object, guint property_id, GValue *value, GParamSpec *pspec)
{
	RSLens *lens = RS_LENS(object);

	switch (property_id)
	{
		case PROP_DESCRIPTION:
			g_value_set_string(value, rs_lens_get_description(lens));
			break;
		case PROP_MIN_FOCAL:
			g_value_set_double(value, lens->min_focal);
			break;
		case PROP_MAX_FOCAL:
			g_value_set_double(value, lens->max_focal);
			break;
		case PROP_MIN_APERTURE:
			g_value_set_double(value, lens->min_aperture);
			break;
		case PROP_MAX_APERTURE:
			g_value_set_double(value, lens->max_aperture);
			break;
		case PROP_IDENTIFIER:
			g_value_set_string(value, lens->identifier);
			break;
		case PROP_LENSFUN_MAKE:
			g_value_set_string(value, lens->lensfun_make);
			break;
		case PROP_LENSFUN_MODEL:
			g_value_set_string(value, lens->lensfun_model);
			break;
		case PROP_CAMERA_MAKE:
			g_value_set_string(value, lens->camera_make);
			break;
		case PROP_CAMERA_MODEL:
			g_value_set_string(value, lens->camera_model);
			break;
		case PROP_ENABLED:
			g_value_set_boolean(value, lens->enabled);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
	}
}

static void
set_property(GObject *object, guint property_id, const GValue *value, GParamSpec *pspec)
{
	RSLens *lens = RS_LENS(object);

	switch (property_id)
	{
		case PROP_MIN_FOCAL:
			lens->min_focal = g_value_get_double(value);
			break;
		case PROP_MAX_FOCAL:
			lens->max_focal = g_value_get_double(value);
			break;
		case PROP_MIN_APERTURE:
			lens->min_aperture = g_value_get_double(value);
			break;
		case PROP_MAX_APERTURE:
			lens->max_aperture = g_value_get_double(value);
			break;
		case PROP_IDENTIFIER:
			g_free(lens->identifier);
			lens->identifier = g_value_dup_string(value);
			break;
		case PROP_LENSFUN_MAKE:
			g_free(lens->lensfun_make);
			lens->lensfun_make = g_value_dup_string(value);
			break;
		case PROP_LENSFUN_MODEL:
			g_free(lens->lensfun_model);
			lens->lensfun_model = g_value_dup_string(value);
			break;
		case PROP_CAMERA_MAKE:
			g_free(lens->camera_make);
			lens->camera_make = g_value_dup_string(value);
			break;
		case PROP_CAMERA_MODEL:
			g_free(lens->camera_model);
			lens->camera_model = g_value_dup_string(value);
			break;
		case PROP_ENABLED:
			lens->enabled = g_value_get_boolean(value);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
	}
}

static void
dispose(GObject *object)
{
	RSLens *lens = RS_LENS(object);

	if (!lens->dispose_has_run)
	{
		g_free(lens->lensfun_make);
		g_free(lens->lensfun_model);

		lens->dispose_has_run = TRUE;
	}
	G_OBJECT_CLASS (rs_lens_parent_class)->dispose (object);
}

static void
rs_lens_class_init(RSLensClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->get_property = get_property;
	object_class->set_property = set_property;
	object_class->dispose = dispose;

	g_object_class_install_property(object_class,
		PROP_IDENTIFIER, g_param_spec_string(
		"description", "Description", "Human readable description of lens",
		NULL, G_PARAM_READABLE));

	g_object_class_install_property(object_class,
		PROP_MIN_FOCAL, g_param_spec_double(
		"min-focal", "min-focal", "Minimum focal",
		-1.0, 20000.0, -1.0, G_PARAM_READWRITE));

	g_object_class_install_property(object_class,
		PROP_MAX_FOCAL, g_param_spec_double(
		"max-focal", "max-focal", "Maximum focal",
		-1.0, 20000.0, -1.0, G_PARAM_READWRITE));

	g_object_class_install_property(object_class,
		PROP_MIN_APERTURE, g_param_spec_double(
		"min-aperture", "min-aperture", "Minimum aperture",
		-1.0, 20000.0, -1.0, G_PARAM_READWRITE));

	g_object_class_install_property(object_class,
		PROP_MAX_APERTURE, g_param_spec_double(
		"max-aperture", "max-aperture", "Maximum aperture",
		-1.0, 20000.0, -1.0, G_PARAM_READWRITE));

	g_object_class_install_property(object_class,
		PROP_IDENTIFIER, g_param_spec_string(
		"identifier", "Identifier", "Rawstudio generated lens identifier",
		NULL, G_PARAM_READWRITE));

	g_object_class_install_property(object_class,
		PROP_LENSFUN_MAKE, g_param_spec_string(
		"lensfun-make", "lensfun-make", "String helping Lensfun to identify the lens make",
		"", G_PARAM_READWRITE));

	g_object_class_install_property(object_class,
		PROP_LENSFUN_MODEL, g_param_spec_string(
		"lensfun-model", "lensfun-model", "String helping Lensfun to identify the lens model",
		"", G_PARAM_READWRITE));

	g_object_class_install_property(object_class,
		PROP_CAMERA_MAKE, g_param_spec_string(
		"camera-make", "camera-make", "String helping Lensfun to identify the camera make",
		"", G_PARAM_READWRITE));

	g_object_class_install_property(object_class,
		PROP_CAMERA_MODEL, g_param_spec_string(
		"camera-model", "camera-model", "String helping Lensfun to identify the camera model",
		"", G_PARAM_READWRITE));

	g_object_class_install_property(object_class,
		PROP_ENABLED, g_param_spec_boolean(
		"enabled", "enabled", "Specify whether the lens should be corrected or not",
		FALSE, G_PARAM_READWRITE));
}

static void
rs_lens_init(RSLens *lens)
{
	lens->dispose_has_run = FALSE;

	lens->description = NULL;
	lens->min_focal = -1.0;
	lens->max_focal = -1.0;
	lens->min_aperture = -1.0;
	lens->max_aperture = -1.0;
	lens->lensfun_make = NULL;
	lens->lensfun_model = NULL;
	lens->camera_make = NULL;
	lens->camera_model = NULL;
	lens->enabled = FALSE;
}

/**
 * Instantiate a new RSLens
 * @return A new RSLens with a refcount of 1
 */
RSLens *
rs_lens_new(void)
{
	return g_object_new (RS_TYPE_LENS, NULL);
}

/**
 * Instantiate a new RSLens from a RSMetadata
 * @param metadata A RSMetadata type with lens information embedded
 * @return A new RSLens with a refcount of 1
 */
RSLens *
rs_lens_new_from_medadata(RSMetadata *metadata)
{
	g_assert(RS_IS_METADATA(metadata));

	return g_object_new(RS_TYPE_LENS,
		"identifier", metadata->lens_identifier,
		"min-focal", metadata->lens_min_focal,
		"max-focal", metadata->lens_max_focal,
		"min-aperture", metadata->lens_min_aperture,
		"max-aperture", metadata->lens_max_aperture,
		"camera-make", metadata->make_ascii,
		"camera-model", metadata->model_ascii,
		NULL);
}

/**
 * Get the Lensfun make from a RSLens
 * @param lens A RSLens
 * @return The make as used by Lensfun or NULL if unknown
 */
const gchar *
rs_lens_get_lensfun_make(RSLens *lens)
{
	g_assert(RS_IS_LENS(lens));

	return lens->lensfun_make;
}

/**
 * Get the Lensfun model from a RSLens
 * @param lens A RSLens
 * @return The model as used by Lensfun or NULL if unknown
 */
const gchar *
rs_lens_get_lensfun_model(RSLens *lens)
{
	g_assert(RS_IS_LENS(lens));

	return lens->lensfun_model;
}

/**
 * Get a human readable description of the lens
 * @param lens A RSLens
 * @return A human readble string describing the lens
 */
const gchar *
rs_lens_get_description(RSLens *lens)
{
	GString *ret;

	if (lens->description)
		return lens->description;

	/* We rely on the Lensfun description being human readble */
	if (rs_lens_get_lensfun_model(lens))
		return rs_lens_get_lensfun_model(lens);

	ret = g_string_new("");

	if (lens->min_focal > -1.0)
	{
		g_string_append_printf(ret, "%.0f", lens->min_focal);
		if ((lens->max_focal > -1.0) && (ABS(lens->max_focal-lens->min_focal) > 0.1))
			g_string_append_printf(ret, "-%.0f", lens->max_focal);
	}
	else if (lens->max_focal > -1.0)
		g_string_append_printf(ret, "%.0f", lens->max_focal);

	if (lens->max_aperture > -1.0)
		g_string_append_printf(ret, " f/%.1f", lens->max_aperture);

	lens->description = ret->str;
	g_string_free(ret, FALSE);

	return lens->description;
}

void
rs_lens_set_lensfun_make(RSLens *lens, gchar *make)
{
	g_assert(RS_IS_LENS(lens));

	lens->lensfun_make = make;
}

void
rs_lens_set_lensfun_model(RSLens *lens, gchar *model)
{
	g_assert(RS_IS_LENS(lens));

	lens->lensfun_model = model;
}

void
rs_lens_set_lensfun_enabled(RSLens *lens, gboolean enabled)
{
	g_assert(RS_IS_LENS(lens));

	lens->enabled = enabled;
}

gboolean
rs_lens_get_lensfun_enabled(RSLens *lens)
{
	g_assert(RS_IS_LENS(lens));

	return lens->enabled;
}
