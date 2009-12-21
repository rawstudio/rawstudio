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

/* Documentation:
 * http://www.sqlite.org/capi3ref.html
 */

/* Database layout:
 *
 * library 
 *   id
 *   filename
 *
 * tags
 *   id
 *   tagname
 *
 * phototags
 *   photo
 *   tag
 */
/*
#include <glib.h>
#include <stdio.h>
#include <stdlib.h>
#include <sqlite3.h>
#include <string.h>
#include "rawstudio.h"
#include "rs-metadata.h"
#include "rs-library.h"
#include "application.h"
*/

#include "rs-library.h"
#include "rs-store.h"
#include "gtk-interface.h"
#include "conf_interface.h"
#include "config.h"
#include "gettext.h"

struct _RSLibrary {
	GObject parent;
	gboolean dispose_has_run;

	sqlite3 *db;
};

G_DEFINE_TYPE(RSLibrary, rs_library, G_TYPE_OBJECT)

static void library_sqlite_error(sqlite3 *db, const gint result);
static gint library_create_tables(sqlite3 *db);
static gint library_find_tag_id(RSLibrary *library, const gchar *tagname);
static gint library_find_photo_id(RSLibrary *library, const gchar *photo);
static void library_photo_add_tag(RSLibrary *library, const gint photo_id, const gint tag_id, const gboolean autotag);
static gboolean library_is_photo_tagged(RSLibrary *library, const gint photo_id, const gint tag_id);
static void library_add_photo(RSLibrary *library, const gchar *filename);
static void library_add_tag(RSLibrary *library, const gchar *tagname);
static void library_delete_photo(RSLibrary *library, const gint photo_id);
static void library_delete_tag(RSLibrary *library, const gint tag_id);
static void library_photo_delete_tags(RSLibrary *library, const gint photo_id);
static void library_tag_delete_photos(RSLibrary *library, const gint tag_id);
static gboolean library_tag_is_used(RSLibrary *library, const gint tag_id);

static GtkWidget *tag_search_entry = NULL;

static void
rs_library_dispose(GObject *object)
{
	RSLibrary *library = RS_LIBRARY(object);

	if (!library->dispose_has_run)
	{
		library->dispose_has_run = TRUE;

		sqlite3_close(library->db);
	}

	G_OBJECT_CLASS(rs_library_parent_class)->dispose (object);
}

static void
rs_library_finalize(GObject *object)
{
	G_OBJECT_CLASS(rs_library_parent_class)->finalize (object);
}

static void
rs_library_class_init(RSLibraryClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS(klass);

	sqlite3_config(SQLITE_CONFIG_SERIALIZED);
	object_class->dispose = rs_library_dispose;
	object_class->finalize = rs_library_finalize;
}

static void
rs_library_init(RSLibrary *library)
{
	int rc;

	gchar *database = g_strdup_printf("%s/.rawstudio/library.db", g_get_home_dir());

	/* If unable to create database we exit */
	if(sqlite3_open(database, &(library->db)))
	{
		g_debug("sqlite3 debug: could not open database %s\n", database);
		sqlite3_close(library->db);
		exit(1);
	}
	g_free(database);

	rc = library_create_tables(library->db);
	library_sqlite_error(library->db, rc);
}

RSLibrary *
rs_library_get_singleton(void)
{
	static GStaticMutex singleton_lock = G_STATIC_MUTEX_INIT;
	static RSLibrary *singleton = NULL;

	g_static_mutex_lock(&singleton_lock);
	singleton = g_object_new(RS_TYPE_LIBRARY, NULL);
	g_static_mutex_unlock(&singleton_lock);

	return singleton;
}

static void
library_sqlite_error(sqlite3 *db, gint result)
{
	if (result != SQLITE_OK)
	{
		g_warning("sqlite3 warning: %s\n", sqlite3_errmsg(db));
	}
}

static gint
library_create_tables(sqlite3 *db)
{
	sqlite3_stmt *stmt;
	gint rc;
       
	/* Create table (library) to hold all known photos */
	sqlite3_prepare_v2(db, "create table library (id integer primary key, filename varchar(1024))", -1, &stmt, NULL);
	rc = sqlite3_step(stmt);
	sqlite3_finalize(stmt);

	/* Create table (tags) with all known tags */
	sqlite3_prepare_v2(db, "create table tags (id integer primary key, tagname varchar(128))", -1, &stmt, NULL);
	rc = sqlite3_step(stmt);
	sqlite3_finalize(stmt);

	/* Create table (phototags) to bind tags and photos together */
	sqlite3_prepare_v2(db, "create table phototags (photo integer, tag integer, autotag integer)", -1, &stmt, NULL);
	rc = sqlite3_step(stmt);
	sqlite3_finalize(stmt);

	return SQLITE_OK;
}

static gint
library_find_tag_id(RSLibrary *library, const gchar *tagname)
{
	sqlite3 *db = library->db;
	sqlite3_stmt *stmt;
	gint rc, tag_id = -1;

	rc = sqlite3_prepare_v2(db, "SELECT id FROM tags WHERE tagname = ?1;", -1, &stmt, NULL);
	rc = sqlite3_bind_text(stmt, 1, tagname, strlen(tagname), SQLITE_TRANSIENT);
	rc = sqlite3_step(stmt);
	if (rc == SQLITE_ROW)
		tag_id = sqlite3_column_int(stmt, 0);
	rc = sqlite3_finalize(stmt);
	return tag_id;
}

static gint
library_find_photo_id(RSLibrary *library, const gchar *photo)
{
	sqlite3 *db = library->db;
	sqlite3_stmt *stmt;
	gint rc, photo_id = -1;

	rc = sqlite3_prepare_v2(db, "SELECT id FROM library WHERE filename = ?1;", -1, &stmt, NULL);
	rc = sqlite3_bind_text(stmt, 1, photo, strlen(photo), SQLITE_TRANSIENT);
	library_sqlite_error(db, rc);
	rc = sqlite3_step(stmt);
	if (rc == SQLITE_ROW)
		photo_id = sqlite3_column_int(stmt, 0);
	rc = sqlite3_finalize(stmt);
	return photo_id;
}

static void
library_photo_add_tag(RSLibrary *library, const gint photo_id, const gint tag_id, const gboolean autotag)
{
	sqlite3 *db = library->db;
	gint rc;
	sqlite3_stmt *stmt;

	gint autotag_tag = 0;
	if (autotag)
		autotag_tag = 1;

	rc = sqlite3_prepare_v2(db, "INSERT INTO phototags (photo, tag, autotag) VALUES (?1, ?2, ?3);", -1, &stmt, NULL);
	rc = sqlite3_bind_int (stmt, 1, photo_id);
	rc = sqlite3_bind_int (stmt, 2, tag_id);
	rc = sqlite3_bind_int (stmt, 3, autotag_tag);
	rc = sqlite3_step(stmt);
	if (rc != SQLITE_DONE)
		library_sqlite_error(db, rc);
	sqlite3_finalize(stmt);
}

static gboolean
library_is_photo_tagged(RSLibrary *library, gint photo_id, gint tag_id)
{
	sqlite3 *db = library->db;
	gint rc;
	sqlite3_stmt *stmt;

	rc = sqlite3_prepare_v2(db, "SELECT * FROM phototags WHERE photo = ?1 AND tag = ?2;", -1, &stmt, NULL);
	rc = sqlite3_bind_int (stmt, 1, photo_id);
	rc = sqlite3_bind_int (stmt, 2, tag_id);
	rc = sqlite3_step(stmt);
	sqlite3_finalize(stmt);

	if (rc == SQLITE_ROW)
		return TRUE;
	else
		return FALSE;
}

static void
library_add_photo(RSLibrary *library, const gchar *filename)
{
	sqlite3 *db = library->db;
	gint rc;
	sqlite3_stmt *stmt;

	sqlite3_prepare_v2(db, "INSERT INTO library (filename) VALUES (?1);", -1, &stmt, NULL);
	rc = sqlite3_bind_text(stmt, 1, filename, strlen(filename), SQLITE_TRANSIENT);
	rc = sqlite3_step(stmt);
	if (rc != SQLITE_DONE)
		library_sqlite_error(db, rc);
	sqlite3_finalize(stmt);
}

static void
library_add_tag(RSLibrary *library, const gchar *tagname)
{
	sqlite3 *db = library->db;
	gint rc;
	sqlite3_stmt *stmt;

	sqlite3_prepare_v2(db, "INSERT INTO tags (tagname) VALUES (?1);", -1, &stmt, NULL);
	rc = sqlite3_bind_text(stmt, 1, tagname, strlen(tagname), SQLITE_TRANSIENT);
	rc = sqlite3_step(stmt);
	if (rc != SQLITE_DONE)
		library_sqlite_error(db, rc);
	sqlite3_finalize(stmt);
}

static void 
library_delete_photo(RSLibrary *library, gint photo_id)
{
	sqlite3 *db = library->db;
	sqlite3_stmt *stmt;
	gint rc;

	rc = sqlite3_prepare_v2(db, "DELETE FROM library WHERE id = ?1;", -1, &stmt, NULL);
	rc = sqlite3_bind_int(stmt, 1, photo_id);
	library_sqlite_error(db, rc);
	rc = sqlite3_step(stmt);
	if (rc != SQLITE_DONE)
		library_sqlite_error(db, rc);
	rc = sqlite3_finalize(stmt);
}

static void 
library_delete_tag(RSLibrary *library, gint tag_id)
{
	sqlite3 *db = library->db;
	sqlite3_stmt *stmt;
	gint rc;

	rc = sqlite3_prepare_v2(db, "DELETE FROM library WHERE filename = ?1;", -1, &stmt, NULL);
	rc = sqlite3_bind_int(stmt, 1, tag_id);
	library_sqlite_error(db, rc);
	rc = sqlite3_step(stmt);
	if (rc != SQLITE_DONE)
		library_sqlite_error(db, rc);
	rc = sqlite3_finalize(stmt);
}

static void 
library_photo_delete_tags(RSLibrary *library, gint photo_id)
{
	sqlite3 *db = library->db;
	sqlite3_stmt *stmt;
	gint rc;

	rc = sqlite3_prepare_v2(db, "DELETE FROM phototags WHERE photo = ?1;", -1, &stmt, NULL);
	rc = sqlite3_bind_int(stmt, 1, photo_id);
	library_sqlite_error(db, rc);
	rc = sqlite3_step(stmt);
	if (rc != SQLITE_DONE)
		library_sqlite_error(db, rc);
	rc = sqlite3_finalize(stmt);
}

static void
library_tag_delete_photos(RSLibrary *library, gint tag_id)
{
	sqlite3 *db = library->db;
	sqlite3_stmt *stmt;
	gint rc;

	rc = sqlite3_prepare_v2(db, "DELETE FROM phototags WHERE tag = ?1;", -1, &stmt, NULL);
	rc = sqlite3_bind_int(stmt, 1, tag_id);
	library_sqlite_error(db, rc);
	rc = sqlite3_step(stmt);
	if (rc != SQLITE_DONE)
		library_sqlite_error(db, rc);
	rc = sqlite3_finalize(stmt);
}

static gboolean
library_tag_is_used(RSLibrary *library, gint tag_id)
{
	sqlite3 *db = library->db;
	gint rc;
	sqlite3_stmt *stmt;

	rc = sqlite3_prepare_v2(db, "SELECT * FROM phototags WHERE tag = ?1;", -1, &stmt, NULL);
	rc = sqlite3_bind_int (stmt, 1, tag_id);
	rc = sqlite3_step(stmt);
	sqlite3_finalize(stmt);

	if (rc == SQLITE_ROW)
		return TRUE;
	else
		return FALSE;
}

void
rs_library_add_photo(RSLibrary *library, const gchar *filename)
{
	g_assert(RS_IS_LIBRARY(library));

	if (library_find_photo_id(library, filename) == -1)
	{
		g_debug("Adding photo to library: %s",filename);
		library_add_photo(library, filename);
	}
}

void
rs_library_add_tag(RSLibrary *library, const gchar *tagname)
{
	g_assert(RS_IS_LIBRARY(library));

	if (library_find_tag_id(library, tagname) == -1)
	{
		g_debug("Adding tag to tags: %s",tagname);
		library_add_tag(library, tagname);
	}

}

void
rs_library_photo_add_tag(RSLibrary *library, const gchar *filename, const gchar *tagname, const gboolean autotag)
{
	g_assert(RS_IS_LIBRARY(library));

	gint photo_id = 0, tag_id;

	photo_id = library_find_photo_id(library, filename);
	if (photo_id == -1)
	{
		g_warning("Photo not known...");
		return;
	}

	tag_id = library_find_tag_id(library, tagname);
	if (tag_id == -1)
	{
		g_warning("Tag not known...");
		return;
	}

	if (!library_is_photo_tagged(library, photo_id, tag_id))
		library_photo_add_tag(library, photo_id, tag_id, autotag);

	return;
}

void
rs_library_delete_photo(RSLibrary *library, const gchar *photo)
{
	g_assert(RS_IS_LIBRARY(library));

	gint photo_id = -1;

	photo_id = library_find_photo_id(library, photo);
	if (photo_id == -1)
	{
		g_warning("Photo not known...");
		return;
	}

	library_photo_delete_tags(library, photo_id);
	library_delete_photo(library, photo_id);
}

gboolean
rs_library_delete_tag(RSLibrary *library, const gchar *tag, const gboolean force)
{
	g_assert(RS_IS_LIBRARY(library));

	gint tag_id = -1;

	tag_id = library_find_tag_id(library, tag);
	if (tag_id == -1)
	{
		g_warning("Tag not known...");
		return FALSE;
	}

	if (library_tag_is_used(library, tag_id))
		if (force)
		{
			library_tag_delete_photos(library, tag_id);
			library_delete_tag(library, tag_id);
		}
		else
		{
			g_warning("Tag is in use...");
			return FALSE;
		}
	else
		library_delete_tag(library, tag_id);
	return TRUE;
}

GList *
rs_library_search(RSLibrary *library, GList *tags)
{
	g_assert(RS_IS_LIBRARY(library));

	sqlite3_stmt *stmt;
	gint rc;
	sqlite3 *db = library->db;
	gchar *tag;
	gint n, num_tags = g_list_length(tags);
	GList *photos = NULL;
	GTimer *gt = g_timer_new();
	
	sqlite3_prepare_v2(db, "create temp table filter (photo integer)", -1, &stmt, NULL);
	rc = sqlite3_step(stmt);
	sqlite3_finalize(stmt);
       
	for (n = 0; n < num_tags; n++)
	{
		tag = (gchar *) g_list_nth_data(tags, n);

		sqlite3_prepare_v2(db, "insert into filter select phototags.photo from phototags, tags where phototags.tag = tags.id and lower(tags.tagname) = lower(?1) ;", -1, &stmt, NULL);
		rc = sqlite3_bind_text(stmt, 1, tag, strlen(tag), SQLITE_TRANSIENT);
		rc = sqlite3_step(stmt);
		sqlite3_finalize(stmt);
	}

	sqlite3_prepare_v2(db, "create temp table result (photo integer, count integer)", -1, &stmt, NULL);
	rc = sqlite3_step(stmt);
	sqlite3_finalize(stmt);

	sqlite3_prepare_v2(db, "insert into result select photo, count(photo) from filter group by photo;", -1, &stmt, NULL);
	rc = sqlite3_step(stmt);
	sqlite3_finalize(stmt);

	sqlite3_prepare_v2(db, "select library.filename from library,result where library.id = result.photo and result.count = ?1 order by library.filename;", -1, &stmt, NULL);
        rc = sqlite3_bind_int(stmt, 1, num_tags);
	while (sqlite3_step(stmt) == SQLITE_ROW)
		photos = g_list_append(photos, g_strdup((gchar *) sqlite3_column_text(stmt, 0)));
	sqlite3_finalize(stmt);

	/* Empty filter */
	sqlite3_prepare_v2(db, "delete from filter;", -1, &stmt, NULL);
	rc = sqlite3_step(stmt);
	sqlite3_finalize(stmt);

	/* Empty result */
	sqlite3_prepare_v2(db, "delete from result;", -1, &stmt, NULL);
	rc = sqlite3_step(stmt);
	sqlite3_finalize(stmt);

	g_debug("Search in library took %.03f seconds", g_timer_elapsed(gt, NULL));
	g_timer_destroy(gt);

	return photos;
}

void
rs_library_photo_default_tags(RSLibrary *library, const gchar *photo, RSMetadata *metadata)
{
	g_assert(RS_IS_LIBRARY(library));

	/* Bail out if we already know the photo */
	if (library_find_photo_id(library, photo) > -1)
		return;

	GList *tags = NULL;

	rs_library_add_photo(library, photo);
	if (metadata->make_ascii)
	{
		GList *temp = rs_split_string(metadata->make_ascii, " ");
		tags = g_list_concat(tags, temp);
	}
	if (metadata->model_ascii)
	{
		GList *temp = rs_split_string(metadata->model_ascii, " ");
		tags = g_list_concat(tags, temp);
	}
	if (metadata->lens_min_focal != -1 && metadata->lens_max_focal != -1)
	{
		gchar *lens = NULL;
		if (metadata->lens_min_focal == metadata->lens_max_focal)
			lens = g_strdup_printf("%dmm",(gint) metadata->lens_min_focal);
		else
			lens = g_strdup_printf("%d-%dmm",(gint) metadata->lens_min_focal, (gint) metadata->lens_max_focal);
		tags = g_list_append(tags, g_strdup(lens));
		g_free(lens);
	}
	if (metadata->focallength > 0)
	{
		gchar *text = NULL;
		if (metadata->focallength < 50)
			text = g_strdup("wideangle");
		else
			text = g_strdup("telephoto");
		tags = g_list_append(tags, g_strdup(text));
		g_free(text);
	}
	if (metadata->timestamp != -1)
	{
		gchar *year = NULL;
		gchar *month = NULL;
		GDate *date = g_date_new();
		g_date_set_time_t(date, metadata->timestamp);
		year = g_strdup_printf("%d", g_date_get_year(date));
		gint m = g_date_get_month(date);

		switch (m)
		{
		case 1:
			month = g_strdup("January");
			break;
		case 2:
			month = g_strdup("February");
			break;
		case 3:
			month = g_strdup("March");
			break;
		case 4:
			month = g_strdup("April");
			break;
		case 5:
			month = g_strdup("May");
			break;
		case 6:
			month = g_strdup("June");
			break;
		case 7:
			month = g_strdup("July");
			break;
		case 8:
			month = g_strdup("August");
			break;
		case 9:
			month = g_strdup("September");
			break;
		case 10:
			month = g_strdup("Ocotober");
			break;
		case 11:
			month = g_strdup("November");
			break;
		case 12:
			month = g_strdup("December");
			break;
		}

		tags = g_list_append(tags, g_strdup(year));
		tags = g_list_append(tags, g_strdup(month));

		g_date_free(date);
		g_free(year);
		g_free(month);
	}

	gint i;
	for(i = 0; i < g_list_length(tags); i++)
	{
		gchar *tag = (gchar *) g_list_nth_data(tags, i);
		rs_library_add_tag(library, tag);
		rs_library_photo_add_tag(library, photo, tag, TRUE);
		g_free(tag);
	}
	g_list_free(tags);
}

GList *
rs_library_photo_tags(RSLibrary *library, const gchar *photo, const gboolean autotag)
{
	g_assert(RS_IS_LIBRARY(library));

	sqlite3_stmt *stmt;
	gint rc;
	sqlite3 *db = library->db;
	GList *tags = NULL;

	if (autotag)
	{
		sqlite3_prepare_v2(db, "select tags.tagname from library,phototags,tags WHERE library.id=phototags.photo and phototags.tag=tags.id and library.filename = ?1;", -1, &stmt, NULL);
		rc = sqlite3_bind_text(stmt, 1, photo, strlen(photo), NULL);
	}
	else
	{
		sqlite3_prepare_v2(db, "select tags.tagname from library,phototags,tags WHERE library.id=phototags.photo and phototags.tag=tags.id and library.filename = ?1 and phototags.autotag = 0;", -1, &stmt, NULL);
		rc = sqlite3_bind_text(stmt, 1, photo, strlen(photo), NULL);
	}
	while (sqlite3_step(stmt) == SQLITE_ROW)
		tags = g_list_append(tags, g_strdup((gchar *) sqlite3_column_text(stmt, 0)));
	sqlite3_finalize(stmt);

	return tags;
}

GList *
rs_library_find_tag(RSLibrary *library, const gchar *tag)
{
	g_assert(RS_IS_LIBRARY(library));

	sqlite3_stmt *stmt;
	gint rc;
	sqlite3 *db = library->db;
	GList *tags = NULL;

	rc = sqlite3_prepare_v2(db, "select tags.tagname from tags WHERE tags.tagname like ?1 order by tags.tagname;", -1, &stmt, NULL);
	gchar *like = g_strdup_printf("%%%s%%", tag);
        rc = sqlite3_bind_text(stmt, 1, like, strlen(like), NULL);
	library_sqlite_error(db, rc);
	
	while (sqlite3_step(stmt) == SQLITE_ROW)
		tags = g_list_append(tags, g_strdup((gchar *) sqlite3_column_text(stmt, 0)));
	sqlite3_finalize(stmt);

	g_free(like);

	return tags;
}

/* Carrier used for a few callbacks */
typedef struct {
	RSLibrary *library;
	RSStore *store;
} cb_carrier;

static void
load_photos(gpointer data, gpointer user_data) {
	RSStore *store = user_data;
	gchar *text = (gchar *) data;
	/* FIXME: Change this to be signal based at some point */
	rs_store_load_file(store, text);
	g_free(text);
}

static void 
search_changed(GtkEntry *entry, gpointer user_data)
{
	cb_carrier *carrier = user_data;
	const gchar *text = gtk_entry_get_text(entry);

	GList *tags = rs_split_string(text, " ");

	GList *photos = rs_library_search(carrier->library, tags);
/*
	printf("photos: %d\n",g_list_length(photos));
	g_list_foreach(photos, list_photos, NULL);
	g_list_foreach(tags, list_photos, NULL);
*/

	/* FIXME: deselect all photos in store */
	rs_store_remove(carrier->store, NULL, NULL);
	g_list_foreach(photos, load_photos, carrier->store);

	GString *window_title = g_string_new("");
	g_string_printf(window_title, _("Tag search [%s]"), text);
	rs_window_set_title(window_title->str);
	g_string_free(window_title, TRUE);
	
	rs_conf_set_string(CONF_LIBRARY_TAG_SEARCH, text);
	rs_conf_unset(CONF_LWD);

	g_list_free(photos);
	g_list_free(tags);
}

GtkWidget *
rs_library_toolbox_new(RSLibrary *library, RSStore *store)
{
	g_assert(RS_IS_LIBRARY(library));
	g_assert(RS_IS_STORE(store));

	cb_carrier *carrier = g_new(cb_carrier, 1);
	GtkWidget *box = gtk_vbox_new(FALSE, 0);
	tag_search_entry = gtk_entry_new();

	carrier->library = library;
	carrier->store = store;
	g_signal_connect (tag_search_entry, "changed",
			  G_CALLBACK (search_changed), carrier);
	gtk_box_pack_start (GTK_BOX(box), tag_search_entry, FALSE, TRUE, 0);
	/* FIXME: Make sure to free carrier at some point */

	return box;
}

gboolean
rs_library_set_tag_search(gchar *str)
{
	if (!str)
		return FALSE;
	gtk_entry_set_text(GTK_ENTRY(tag_search_entry), str);
	return TRUE;
}
