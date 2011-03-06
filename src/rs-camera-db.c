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

#include "config.h"
#include "gettext.h"
#include <libxml/encoding.h>
#include <libxml/xmlwriter.h>
#include "rs-camera-db.h"
#include "rs-photo.h"
#include "rs-toolbox.h"
#include "rs-cache.h"

/* FIXME: Make this thread safe! */

struct _RSCameraDb {
	GObject parent;

	gchar *path;
	GtkListStore *cameras;
};

enum {
	COLUMN_MAKE,
	COLUMN_MODEL,
	COLUMN_PROFILE,
	COLUMN_SETTINGS0,
	COLUMN_SETTINGS1,
	COLUMN_SETTINGS2,
	NUM_COLUMNS
};

G_DEFINE_TYPE(RSCameraDb, rs_camera_db, G_TYPE_OBJECT)

static void load_db(RSCameraDb *db);
static void save_db(RSCameraDb *db);

enum {
	PROP_0,
	PROP_PATH
};

static void
get_property(GObject *object, guint property_id, GValue *value, GParamSpec *pspec)
{
	RSCameraDb *camera_db = RS_CAMERA_DB(object);

	switch (property_id)
	{
		case PROP_PATH:
			g_value_set_string(value, camera_db->path);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
	}
}

static void
set_property(GObject *object, guint property_id, const GValue *value, GParamSpec *pspec)
{
	RSCameraDb *camera_db = RS_CAMERA_DB(object);

	switch (property_id)
	{
		case PROP_PATH:
			camera_db->path = g_value_dup_string(value);
			load_db(camera_db);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
	}
}

static void
dispose(GObject *object)
{
	G_OBJECT_CLASS(rs_camera_db_parent_class)->dispose(object);
}

static void
rs_camera_db_class_init(RSCameraDbClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS(klass);

	object_class->get_property = get_property;
	object_class->set_property = set_property;
	object_class->dispose = dispose;

	g_object_class_install_property(object_class,
		PROP_PATH, g_param_spec_string(
		"path", "Path", "Path to XML database",
		NULL, G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE));
}

static void
rs_camera_db_init(RSCameraDb *camera_db)
{
	camera_db->cameras = gtk_list_store_new(NUM_COLUMNS,
		G_TYPE_STRING, /* COLUMN_MAKE */
		G_TYPE_STRING, /* COLUMN_MODEL */
		G_TYPE_POINTER, /* COLUMN_PROFILE */
		RS_TYPE_SETTINGS, /* COLUMN_SETTINGS0 */
		RS_TYPE_SETTINGS, /* COLUMN_SETTINGS1 */
		RS_TYPE_SETTINGS /* COLUMN_SETTINGS2 */
	);
}

RSCameraDb *
rs_camera_db_new(const char *path)
{
	g_assert(path != NULL);
	g_assert(g_path_is_absolute(path));

	return g_object_new (RS_TYPE_CAMERA_DB, "path", path, NULL);
}

RSCameraDb *
rs_camera_db_get_singleton(void)
{
	static RSCameraDb *camera_db = NULL;
	static GStaticMutex lock = G_STATIC_MUTEX_INIT;

	g_static_mutex_lock(&lock);
	if (!camera_db)
	{
		gchar *path = g_build_filename(rs_confdir_get(), "camera-database.xml", NULL);
		camera_db = rs_camera_db_new(path);
		g_free(path);
	}
	g_static_mutex_unlock(&lock);

	return camera_db;
}

static void
camera_db_add_camera(RSCameraDb *camera_db, const gchar *make, const gchar *model)
{
	GtkTreeIter iter;

	gtk_list_store_append(camera_db->cameras, &iter);

	gtk_list_store_set(camera_db->cameras, &iter,
		COLUMN_MAKE, make,
		COLUMN_MODEL, model,
		-1);

	save_db(camera_db);
}

void
rs_camera_db_save_defaults(RSCameraDb *camera_db, RS_PHOTO *photo)
{
	g_return_if_fail(RS_IS_PHOTO(photo));
	g_return_if_fail(RS_IS_METADATA(photo->metadata));

	gboolean found = FALSE;
	gint snapshot;
	gchar *db_make, *db_model;
	const gchar *needle_make = photo->metadata->make_ascii;
	const gchar *needle_model = photo->metadata->model_ascii;

	GtkTreeIter iter;
	GtkTreeModel *model = GTK_TREE_MODEL(camera_db->cameras);

	if (needle_make && needle_model && gtk_tree_model_get_iter_first(model, &iter))
		do {
			gtk_tree_model_get(model, &iter,
				COLUMN_MAKE, &db_make,
				COLUMN_MODEL, &db_model,
				-1);
			if (db_make && db_model && g_str_equal(needle_make, db_make) && g_str_equal(needle_model, db_model))
			{
				gpointer profile = rs_photo_get_dcp_profile(photo);

				gtk_list_store_set(camera_db->cameras, &iter,
					COLUMN_PROFILE, profile,
					-1);

				RSSettings *settings[3];

				for(snapshot=0;snapshot<3;snapshot++)
				{
					settings[snapshot] = rs_settings_new();
					rs_settings_copy(photo->settings[snapshot], MASK_ALL, settings[snapshot]);
					gtk_list_store_set(camera_db->cameras, &iter,
						COLUMN_SETTINGS0 + snapshot, settings[snapshot],
						-1);
					g_object_unref(settings[snapshot]);
				}

				found = TRUE;
			}
			g_free(db_make);
			g_free(db_model);
		} while (!found && gtk_tree_model_iter_next(model, &iter));

	save_db(camera_db);
}

gboolean
rs_camera_db_photo_get_defaults(RSCameraDb *camera_db, RS_PHOTO *photo, RSSettings **dest_settings, gpointer *dest_profile)
{
	g_return_val_if_fail(RS_IS_PHOTO(photo), FALSE);
	g_return_val_if_fail(RS_IS_METADATA(photo->metadata), FALSE);

	gboolean found = FALSE;

	gchar *db_make, *db_model;
	const gchar *needle_make = photo->metadata->make_ascii;
	const gchar *needle_model = photo->metadata->model_ascii;

	GtkTreeIter iter;
	GtkTreeModel *model = GTK_TREE_MODEL(camera_db->cameras);

	if (needle_make && needle_model && gtk_tree_model_get_iter_first(model, &iter))
		do {
			gtk_tree_model_get(model, &iter,
				COLUMN_MAKE, &db_make,
				COLUMN_MODEL, &db_model,
				-1);
			if (db_make && db_model && g_str_equal(needle_make, db_make) && g_str_equal(needle_model, db_model))
			{

				gtk_tree_model_get(model, &iter,
					COLUMN_PROFILE, dest_profile,
					COLUMN_SETTINGS0, &dest_settings[0],
					COLUMN_SETTINGS1, &dest_settings[1],
					COLUMN_SETTINGS2, &dest_settings[2],
					-1);

				found = TRUE;
			}
			g_free(db_make);
			g_free(db_model);
		} while (!found && gtk_tree_model_iter_next(model, &iter));

	if (!found)
		camera_db_add_camera(camera_db, needle_make, needle_model);

	return found;
}

gboolean
rs_camera_db_photo_set_defaults(RSCameraDb *camera_db, RS_PHOTO *photo)
{
	g_return_val_if_fail(RS_IS_PHOTO(photo), FALSE);
	g_return_val_if_fail(RS_IS_METADATA(photo->metadata), FALSE);

	gpointer p;
	RSSettings *s[3];
	gboolean found = rs_camera_db_photo_get_defaults(camera_db, photo, s, &p);

	if (!found)
		return FALSE;

	if (RS_IS_DCP_FILE(p))
		rs_photo_set_dcp_profile(photo, p);

	gint i;
	for(i=0;i<3;i++)
		if (RS_IS_SETTINGS(s[i]))
		{
			rs_settings_copy(s[i], MASK_ALL, photo->settings[i]);
			g_object_unref(s[i]);
		}

	return found;
}

static void
load_db(RSCameraDb *camera_db)
{
	xmlDocPtr doc;
	xmlNodePtr cur;
	xmlNodePtr entry = NULL;
	xmlChar *val;
	RSProfileFactory *profile_factory = rs_profile_factory_new_default();

	doc = xmlParseFile(camera_db->path);
	if (!doc)
		return;

	cur = xmlDocGetRootElement(doc);
	if (cur && (xmlStrcmp(cur->name, BAD_CAST "rawstudio-camera-database") == 0))
	{
		cur = cur->xmlChildrenNode;
		while(cur)
		{
			if ((!xmlStrcmp(cur->name, BAD_CAST "camera")))
			{
				GtkTreeIter iter;

				gtk_list_store_append(camera_db->cameras, &iter);

				entry = cur->xmlChildrenNode;

				while (entry)
				{
					val = xmlNodeListGetString(doc, entry->xmlChildrenNode, 1);
					if ((!xmlStrcmp(entry->name, BAD_CAST "make")))
						gtk_list_store_set(camera_db->cameras, &iter, COLUMN_MAKE, val, -1);
					else if ((!xmlStrcmp(entry->name, BAD_CAST "model")))
						gtk_list_store_set(camera_db->cameras, &iter, COLUMN_MODEL, val, -1);
					else if ((!xmlStrcmp(entry->name, BAD_CAST "dcp-profile")))
						gtk_list_store_set(camera_db->cameras, &iter, COLUMN_PROFILE, rs_profile_factory_find_from_id(profile_factory, (gchar *) val), -1);
					xmlFree(val);
					
					if ((!xmlStrcmp(entry->name, BAD_CAST "settings")))
					{
						val = xmlGetProp(entry, BAD_CAST "id");
						gint id = (val) ? atoi((gchar *) val) : 0;
						xmlFree(val);
						id = CLAMP(id, 0, 2);
						RSSettings *settings = rs_settings_new();
						rs_cache_load_setting(settings, doc, entry->xmlChildrenNode, 100); /* FIXME: Correct version somehow! */
						gtk_list_store_set(camera_db->cameras, &iter, COLUMN_SETTINGS0 + id, settings, -1);
						g_object_unref(settings);
					}
					entry = entry->next;
				}
			}
			cur = cur->next;
		}
	}
	else
		g_warning(PACKAGE " did not understand the format in %s", camera_db->path);

	xmlFreeDoc(doc);
}

static void
save_db(RSCameraDb *camera_db)
{
	xmlTextWriterPtr writer;
	gint snapshot;
	gchar *db_make, *db_model;
	gpointer profile;
	RSSettings *settings[3];
	GtkTreeIter iter;
	GtkTreeModel *model = GTK_TREE_MODEL(camera_db->cameras);

	writer = xmlNewTextWriterFilename(camera_db->path, 0);
	if (!writer)
		return;

	xmlTextWriterSetIndent(writer, 1);
	xmlTextWriterStartDocument(writer, NULL, "ISO-8859-1", NULL);
	xmlTextWriterStartElement(writer, BAD_CAST "rawstudio-camera-database");

	if (gtk_tree_model_get_iter_first(model, &iter))
		do {
			gtk_tree_model_get(model, &iter,
				COLUMN_MAKE, &db_make,
				COLUMN_MODEL, &db_model,
				COLUMN_PROFILE, &profile,
				COLUMN_SETTINGS0, &settings[0],
				COLUMN_SETTINGS1, &settings[1],
				COLUMN_SETTINGS2, &settings[2],
				-1);

			xmlTextWriterStartElement(writer, BAD_CAST "camera");

			if (db_make)
				xmlTextWriterWriteFormatElement(writer, BAD_CAST "make", "%s", db_make);
			if (db_model)
				xmlTextWriterWriteFormatElement(writer, BAD_CAST "model", "%s", db_model);

			if (profile)
			{
				if (RS_IS_DCP_FILE(profile))
				{
					const gchar* dcp_id = rs_dcp_get_id(RS_DCP_FILE(profile));
					xmlTextWriterWriteFormatElement(writer, BAD_CAST "dcp-profile", "%s", dcp_id);
				}
				/* FIXME: Add support for ICC profiles */
			}

			for(snapshot=0;snapshot<3;snapshot++)
				if (RS_IS_SETTINGS(settings[snapshot]))
				    {
						xmlTextWriterStartElement(writer, BAD_CAST "settings");
						xmlTextWriterWriteFormatAttribute(writer, BAD_CAST "id", "%d", snapshot);
						rs_cache_save_settings(settings[snapshot], MASK_ALL-MASK_WB, writer);
						xmlTextWriterEndElement(writer);
						g_object_unref(settings[snapshot]);
					}

			xmlTextWriterEndElement(writer);

			g_free(db_make);
			g_free(db_model);
		} while (gtk_tree_model_iter_next(model, &iter));

	xmlTextWriterEndDocument(writer);
	xmlFreeTextWriter(writer);

	return;
}

static void
icon_func(GtkTreeViewColumn *tree_column, GtkCellRenderer *cell, GtkTreeModel *model, GtkTreeIter *iter, gpointer data)
{
	/* This will always be called from the GTK+ main thread, we should be safe */
	static GdkPixbuf *pixbuf = NULL;

	if (!pixbuf)
		pixbuf = gdk_pixbuf_new_from_file(PACKAGE_DATA_DIR "/pixmaps/" PACKAGE "/camera-photo.png", NULL);

	g_object_set(cell, "pixbuf", pixbuf, NULL);
}

GtkWidget *
rs_camera_db_editor_new(RSCameraDb *camera_db)
{
	GtkWidget *dialog = NULL;

	if (dialog)
		return dialog;

	dialog = gtk_dialog_new();

	gtk_window_set_title(GTK_WINDOW(dialog), _("Camera defaults editor"));
	gtk_window_set_type_hint(GTK_WINDOW(dialog), GDK_WINDOW_TYPE_HINT_DIALOG);
	gtk_window_set_destroy_with_parent(GTK_WINDOW(dialog), TRUE);

	gtk_dialog_set_has_separator (GTK_DIALOG(dialog), FALSE);

	/* Is this wise? */
	g_signal_connect_swapped(dialog, "delete_event", G_CALLBACK (gtk_widget_hide), dialog);
	g_signal_connect_swapped(dialog, "response", G_CALLBACK (gtk_widget_hide), dialog);

#if GTK_CHECK_VERSION(2,14,0)
	GtkWidget *vbox = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
#else
	GtkWidget *vbox = GTK_DIALOG(dialog)->vbox;
#endif

	GtkWidget *camera_selector = gtk_tree_view_new_with_model(GTK_TREE_MODEL(camera_db->cameras));

    GtkCellRenderer *renderer;
    GtkTreeViewColumn *column;
	
	column = gtk_tree_view_column_new();
	gtk_tree_view_column_set_title (column, _("Model"));

	/* Icon */
	renderer = gtk_cell_renderer_pixbuf_new();
	gtk_tree_view_column_pack_start (column, renderer, FALSE);
	gtk_tree_view_column_set_cell_data_func(column, renderer, icon_func, NULL, NULL);

	/* Model */
	renderer = gtk_cell_renderer_text_new();
	gtk_tree_view_column_pack_start(column, renderer, TRUE);
	gtk_tree_view_column_set_attributes(column, renderer, "text", COLUMN_MODEL, NULL);

	gtk_tree_view_append_column(GTK_TREE_VIEW(camera_selector), column);

	gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(camera_db->cameras), COLUMN_MODEL, GTK_SORT_ASCENDING);

	GtkWidget *hbox = gtk_hbox_new(FALSE, 4);

	gtk_box_pack_start(GTK_BOX(hbox), camera_selector, FALSE, FALSE, 3);

	GtkWidget *toolbox = rs_toolbox_new();
	gtk_box_pack_start(GTK_BOX(hbox), toolbox, TRUE, TRUE, 3);

	gtk_box_pack_start(GTK_BOX(vbox), hbox, TRUE, TRUE, 3);

	return dialog;
}
