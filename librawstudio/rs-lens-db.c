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

#include <rawstudio.h>
#include <libxml/encoding.h>
#include <libxml/xmlwriter.h>
#include "config.h"
#include "rs-lens-db.h"

struct _RSLensDb {
	GObject parent;
	gboolean dispose_has_run;

	gchar *path;
	GList *lenses;
};

static void open_db(RSLensDb *lens_db);

G_DEFINE_TYPE (RSLensDb, rs_lens_db, G_TYPE_OBJECT)

enum {
	PROP_0,
	PROP_PATH
};

static void
get_property(GObject *object, guint property_id, GValue *value, GParamSpec *pspec)
{
	RSLensDb *lens_db = RS_LENS_DB(object);

	switch (property_id)
	{
		case PROP_PATH:
			g_value_set_string(value, lens_db->path);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
	}
}

static void
set_property(GObject *object, guint property_id, const GValue *value, GParamSpec *pspec)
{
	RSLensDb *lens_db = RS_LENS_DB(object);

	switch (property_id)
	{
		case PROP_PATH:
			lens_db->path = g_value_dup_string(value);
			open_db(lens_db);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
	}
}

static void
dispose(GObject *object)
{
	RSLensDb *lens_db = RS_LENS_DB(object);

	if (!lens_db->dispose_has_run)
	{
		g_free(lens_db->path);
		lens_db->dispose_has_run = TRUE;
	}

	G_OBJECT_CLASS (rs_lens_db_parent_class)->dispose (object);
}

static void
rs_lens_db_class_init(RSLensDbClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->get_property = get_property;
	object_class->set_property = set_property;
	object_class->dispose = dispose;

	g_object_class_install_property(object_class,
		PROP_PATH, g_param_spec_string(
		"path", "Path", "Path to XML database",
		NULL, G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE));
}

static void
rs_lens_db_init(RSLensDb *lens_db)
{
	lens_db->dispose_has_run = FALSE;
	lens_db->path = NULL;
	lens_db->lenses = NULL;
}

static void
save_db(RSLensDb *lens_db)
{
	xmlTextWriterPtr writer;
	GList *list;
	static GStaticMutex lock = G_STATIC_MUTEX_INIT;

	g_static_mutex_lock(&lock);
	writer = xmlNewTextWriterFilename(lens_db->path, 0);
	if (!writer)
	{
		g_static_mutex_unlock(&lock);
		return;
	}

	xmlTextWriterSetIndent(writer, 1);
	xmlTextWriterStartDocument(writer, NULL, "ISO-8859-1", NULL);
	xmlTextWriterStartElement(writer, BAD_CAST "rawstudio-lens-database");

	list = lens_db->lenses;
	while (list)
	{
		gchar *identifier;
		gchar *lensfun_make;
		gchar *lensfun_model;
		gdouble min_focal, max_focal, min_aperture, max_aperture;
		gchar *camera_make;
		gchar *camera_model;
		gboolean enabled;
		gboolean defish;

		RSLens *lens = list->data;

		g_assert(RS_IS_LENS(lens));
		g_object_get(lens,
			"identifier", &identifier,
			"lensfun-make", &lensfun_make,
			"lensfun-model", &lensfun_model,
			"min-focal", &min_focal,
			"max-focal", &max_focal,
			"min-aperture", &min_aperture,
			"max-aperture", &max_aperture,
			"camera-make", &camera_make,
			"camera-model", &camera_model,
			"enabled", &enabled,
			"defish", &defish,
			NULL);

		xmlTextWriterStartElement(writer, BAD_CAST "lens");
			if (identifier)
				xmlTextWriterWriteFormatElement(writer, BAD_CAST "identifier", "%s", identifier);
			if (lensfun_make)
				xmlTextWriterWriteFormatElement(writer, BAD_CAST "lensfun-make", "%s", lensfun_make);
			if (lensfun_model)
				xmlTextWriterWriteFormatElement(writer, BAD_CAST "lensfun-model", "%s", lensfun_model);
			if (min_focal > 0.0)
				xmlTextWriterWriteFormatElement(writer, BAD_CAST "min-focal", "%f", min_focal);
			if (max_focal > 0.0)
				xmlTextWriterWriteFormatElement(writer, BAD_CAST "max-focal", "%f", max_focal);
			if (min_aperture > 0.0)
				xmlTextWriterWriteFormatElement(writer, BAD_CAST "min-aperture", "%f", min_aperture);
			if (max_aperture > 0.0)
				xmlTextWriterWriteFormatElement(writer, BAD_CAST "max-aperture", "%f", max_aperture);
			if (camera_make)
				xmlTextWriterWriteFormatElement(writer, BAD_CAST "camera-make", "%s", camera_make);
			if (camera_model)
				xmlTextWriterWriteFormatElement(writer, BAD_CAST "camera-model", "%s", camera_model);
			if (enabled)
				xmlTextWriterWriteFormatElement(writer, BAD_CAST "enabled", "%s", "TRUE");
			if (!enabled)
				xmlTextWriterWriteFormatElement(writer, BAD_CAST "enabled", "%s", "FALSE");
			xmlTextWriterWriteFormatElement(writer, BAD_CAST "defish", "%s", defish ? "TRUE": "FALSE");
		xmlTextWriterEndElement(writer);

		g_free(identifier);
		g_free(lensfun_make);
		g_free(lensfun_model);
		g_free(camera_make);
		g_free(camera_model);

		list = g_list_next (list);
	}

	xmlTextWriterEndDocument(writer);
	xmlFreeTextWriter(writer);
	g_static_mutex_unlock(&lock);

	return;
}

/**
 * Force save of RSLensDb
 * @param lens_db the RSLensDb to save
 */
void
rs_lens_db_save(RSLensDb *lens_db)
{
	save_db(lens_db);
}

static void
open_db(RSLensDb *lens_db)
{
	xmlDocPtr doc;
	xmlNodePtr cur;
	xmlNodePtr entry = NULL;
	xmlChar *val;

	/* Some sanity checks */
	doc = xmlParseFile(lens_db->path);
	if (!doc)
		return;

	cur = xmlDocGetRootElement(doc);
	if (cur && (xmlStrcmp(cur->name, BAD_CAST "rawstudio-lens-database") == 0))
	{
		cur = cur->xmlChildrenNode;
		while(cur)
		{
			if ((!xmlStrcmp(cur->name, BAD_CAST "lens")))
			{
				RSLens *lens = rs_lens_new();

				entry = cur->xmlChildrenNode;

				while (entry)
				{
					val = xmlNodeListGetString(doc, entry->xmlChildrenNode, 1);
					if ((!xmlStrcmp(entry->name, BAD_CAST "identifier")))
						g_object_set(lens, "identifier", val, NULL);
					else if ((!xmlStrcmp(entry->name, BAD_CAST "lensfun-make")))
						g_object_set(lens, "lensfun-make", val, NULL);
					else if ((!xmlStrcmp(entry->name, BAD_CAST "lensfun-model")))
						g_object_set(lens, "lensfun-model", val, NULL);
					else if ((!xmlStrcmp(entry->name, BAD_CAST "min-focal")))
						g_object_set(lens, "min-focal", rs_atof((gchar *) val), NULL);
					else if ((!xmlStrcmp(entry->name, BAD_CAST "max-focal")))
						g_object_set(lens, "max-focal", rs_atof((gchar *) val), NULL);
					else if ((!xmlStrcmp(entry->name, BAD_CAST "min-aperture")))
						g_object_set(lens, "min-aperture", rs_atof((gchar *) val), NULL);
					else if ((!xmlStrcmp(entry->name, BAD_CAST "max-aperture")))
						g_object_set(lens, "max-aperture", rs_atof((gchar *) val), NULL);
					else if ((!xmlStrcmp(entry->name, BAD_CAST "camera-make")))
						g_object_set(lens, "camera-make", val, NULL);
					else if ((!xmlStrcmp(entry->name, BAD_CAST "camera-model")))
						g_object_set(lens, "camera-model", val, NULL);
					else if ((!xmlStrcmp(entry->name, BAD_CAST "enabled")))
					{
						gboolean enabled = FALSE;
						if (g_strcmp0((gchar *) val, "TRUE") == 0)
							enabled = TRUE;
						g_object_set(lens, "enabled", enabled, NULL);
					}
					else if ((!xmlStrcmp(entry->name, BAD_CAST "defish")))
					{
						gboolean defish = g_strcmp0((gchar *) val, "TRUE") == 0;
						g_object_set(lens, "defish", defish, NULL);
					}
					xmlFree(val);
					entry = entry->next;
				}

				lens_db->lenses = g_list_prepend(lens_db->lenses, lens);
			}
			cur = cur->next;
		}
	}
	else
		g_warning(PACKAGE " did not understand the format in %s", lens_db->path);

	xmlFreeDoc(doc);
}

/**
 * Instantiate a new RSLensDb
 * @param path An absolute path to a XML-file containing the database
 * @return A new RSLensDb with a refcount of 1
 */
RSLensDb *
rs_lens_db_new(const char *path)
{
	g_assert(path != NULL);
	g_assert(g_path_is_absolute(path));

	return g_object_new (RS_TYPE_LENS_DB, "path", path, NULL);
}

/**
 * Get the default RSLensDb as used globally by Rawstudio
 * @return A new RSLensDb, this should not be unref'ed after use!
 */
RSLensDb *
rs_lens_db_get_default(void)
{
	static GStaticMutex lock = G_STATIC_MUTEX_INIT;
	static RSLensDb *lens_db = NULL;

	g_static_mutex_lock(&lock);
	if (!lens_db)
	{
		gchar *path = g_build_filename(rs_confdir_get(), "lens-database.xml", NULL);
		lens_db = rs_lens_db_new(path);
		save_db(lens_db);
		g_free(path);
	}
	g_static_mutex_unlock(&lock);

	return lens_db;
}

/**
 * Look up identifer in database
 * @param lens_db A RSLensDb to search in
 * @param identifier A lens identifier as generated by metadata subsystem
 */
RSLens *
rs_lens_db_get_from_identifier(RSLensDb *lens_db, const gchar *identifier)
{
	GList *list;
	RSLens *lens, *ret = NULL;

	g_assert(RS_IS_LENS_DB(lens_db));
	g_assert(identifier != NULL);

	list = lens_db->lenses;
	while (list)
	{
		gchar *rs_identifier = NULL;
		lens = list->data;

		g_assert(RS_IS_LENS(lens));
		g_object_get(lens, "identifier", &rs_identifier, NULL);

		/* If we got a match, raise refcount by 1 and break out of the loop */
		if (rs_identifier && g_str_equal(rs_identifier, identifier))
		{
			ret = g_object_ref(lens);
			break;
		}

		list = g_list_next (list);
	}

	return ret;
}

/**
 * Add a lens to the database - will only be added if the lens appear unique
 * @param lens_db A RSLensDb
 * @param lens A RSLens to add
 */
void
rs_lens_db_add_lens(RSLensDb *lens_db, RSLens *lens)
{
	gchar *rs_identifier = NULL;

	g_assert(RS_IS_LENS_DB(lens_db));
	g_assert(RS_IS_LENS(lens));

	g_object_get(lens, "identifier", &rs_identifier, NULL);

	if (rs_identifier)
	{
		RSLens *locallens = rs_lens_db_get_from_identifier(lens_db, rs_identifier);

		/* If we got a hit, no need to do anymore - we do not wan't duplicates */
		if (locallens)
			g_object_unref(locallens);
		else
		{
			lens_db->lenses = g_list_prepend(lens_db->lenses, g_object_ref(lens));
			save_db(lens_db);
		}
	}
}

/**
 * Lookup a lens in the database based on information in a RSMetadata
 * @param lens_db A RSLensDb
 * @param metadata A RSMetadata
 * @return A RSLens or NULL if unsuccesful
 */
RSLens *rs_lens_db_lookup_from_metadata(RSLensDb *lens_db, RSMetadata *metadata)
{
	RSLens *lens = NULL;

	g_assert(RS_IS_LENS_DB(lens_db));
	g_assert(RS_IS_METADATA(metadata));

	/* Lookup lens based on generated identifier */
	if (metadata->lens_identifier)
		lens = rs_lens_db_get_from_identifier(lens_db, metadata->lens_identifier);

	/* If we didn't find any matches, we should try to add the lens to our
	   database */
	if (!lens)
	{
		lens = rs_lens_new_from_medadata(metadata);

		if (lens)
			rs_lens_db_add_lens(lens_db, lens);
	}

	return lens;
}

/**
 * Gets the lenses in RSLensDb
 * @param lens_db A RSLensDb
 * @return A GList of RSLens'es
 */
GList *
rs_lens_db_get_lenses(RSLensDb *lens_db)
{
	return lens_db->lenses;
}
