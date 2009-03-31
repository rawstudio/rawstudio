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

	gdouble min_focal;
	gdouble max_focal;
	gdouble min_aperture;
	gdouble max_aperture;
	gchar *identifier;
	gchar *lensfun_identifier;
};

G_DEFINE_TYPE (RSLens, rs_lens, G_TYPE_OBJECT)

enum {
	PROP_0,
	PROP_MIN_FOCAL,
	PROP_MAX_FOCAL,
	PROP_MIN_APERTURE,
	PROP_MAX_APERTURE,
	PROP_IDENTIFIER,
	PROP_LENSFUN_IDENTIFIER
};

static void
get_property(GObject *object, guint property_id, GValue *value, GParamSpec *pspec)
{
	RSLens *lens = RS_LENS(object);

	switch (property_id)
	{
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
		case PROP_LENSFUN_IDENTIFIER:
			g_value_set_string(value, lens->lensfun_identifier);
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
		case PROP_LENSFUN_IDENTIFIER:
			g_free(lens->lensfun_identifier);
			lens->lensfun_identifier = g_value_dup_string(value);
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
		g_free(lens->lensfun_identifier);

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
		PROP_LENSFUN_IDENTIFIER, g_param_spec_string(
		"lensfun-identifier", "lensfun-identifier", "String helping Lensfun to identify the lens",
		"", G_PARAM_READWRITE));
}

static void
rs_lens_init(RSLens *lens)
{
	lens->dispose_has_run = FALSE;

	lens->min_focal = -1.0;
	lens->max_focal = -1.0;
	lens->min_aperture = -1.0;
	lens->max_aperture = -1.0;
	lens->lensfun_identifier = NULL;
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
 * Get the Lensfun idenfier from a RSLens
 * @param lens A RSLens
 * @return The identifier as used by Lensfun or NULL if unknown
 */
gchar *
rs_lens_get_lensfun_identifier(RSLens *lens)
{
	g_assert(RS_IS_LENS(lens));

	return g_strdup(lens->lensfun_identifier);
}
