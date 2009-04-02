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
	gchar *lensfun_model;
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
	PROP_LENSFUN_MODEL
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
		case PROP_LENSFUN_MODEL:
			g_value_set_string(value, lens->lensfun_model);
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
		case PROP_LENSFUN_MODEL:
			g_free(lens->lensfun_model);
			lens->lensfun_model = g_value_dup_string(value);
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
		PROP_LENSFUN_MODEL, g_param_spec_string(
		"lensfun-model", "lensfun-model", "String helping Lensfun to identify the lens model",
		"", G_PARAM_READWRITE));
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
	lens->lensfun_model = NULL;
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
		NULL);
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
