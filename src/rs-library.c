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

/* Documentation:
 * http://www.sqlite.org/capi3ref.html
 */

/* Database layout:
 *
 * library 
 *   id
 *   filename
 *   identifier
 *
 * tags
 *   id
 *   tagname
 *
 * phototags
 *   photo
 *   tag
 *   autotag
 *
 * version
 *   version
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
#include <libxml/encoding.h>
#include <libxml/xmlwriter.h>

#define LIBRARY_VERSION 2

struct _RSLibrary {
	GObject parent;
	gboolean dispose_has_run;

	sqlite3 *db;

	/* This mutex must be used when inserting data in a table with an
	   autocrementing column - which is ALWAYS for sqlite */
	GMutex *id_lock;
};

G_DEFINE_TYPE(RSLibrary, rs_library, G_TYPE_OBJECT)

static gint library_execute_sql(sqlite3 *db, const gchar *sql);
static void library_sqlite_error(sqlite3 *db, const gint result);
static gint library_create_tables(sqlite3 *db);
static gint library_find_tag_id(RSLibrary *library, const gchar *tagname);
static gint library_find_photo_id(RSLibrary *library, const gchar *photo);
static void library_photo_add_tag(RSLibrary *library, const gint photo_id, const gint tag_id, const gboolean autotag);
static gboolean library_is_photo_tagged(RSLibrary *library, const gint photo_id, const gint tag_id);
static gint library_add_photo(RSLibrary *library, const gchar *filename);
static gint library_add_tag(RSLibrary *library, const gchar *tagname);
static void library_delete_photo(RSLibrary *library, const gint photo_id);
static void library_delete_tag(RSLibrary *library, const gint tag_id);
static void library_photo_delete_tags(RSLibrary *library, const gint photo_id);
static void library_tag_delete_photos(RSLibrary *library, const gint tag_id);
static gboolean library_tag_is_used(RSLibrary *library, const gint tag_id);
static void library_photo_default_tags(RSLibrary *library, const gint photo_id, RSMetadata *metadata);
static void library_backup_tags(RSLibrary *library, const gchar *directory);

static GtkWidget *tag_search_entry = NULL;

static void
rs_library_dispose(GObject *object)
{
	RSLibrary *library = RS_LIBRARY(object);

	if (!library->dispose_has_run)
	{
		library->dispose_has_run = TRUE;

		sqlite3_close(library->db);

		g_mutex_free(library->id_lock);
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

static gint
library_set_version(sqlite3 *db, gint version)
{
	sqlite3_stmt *stmt;
	gint rc;

	rc = sqlite3_prepare_v2(db, "update version set version = ?1;", -1, &stmt, NULL);
	rc = sqlite3_bind_int(stmt, 1, version);
	rc = sqlite3_step(stmt);
	library_sqlite_error(db, rc);
	sqlite3_finalize(stmt);

	return SQLITE_OK;
}

static void
library_check_version(sqlite3 *db)
{
	sqlite3_stmt *stmt, *stmt_update;
	gint rc, version = 0, id;
	gchar *filename;

	rc = sqlite3_prepare_v2(db, "SELECT version FROM version", -1, &stmt, NULL);
	rc = sqlite3_step(stmt);
	if (rc == SQLITE_ROW)
		version = sqlite3_column_int(stmt, 0);
	rc = sqlite3_finalize(stmt);

	while (version < LIBRARY_VERSION)
	{
		switch (version)
		{
		case 0:
			/* Alter table library - add identifier column */
			sqlite3_prepare_v2(db, "alter table library add column identifier varchar(32)", -1, &stmt, NULL);
			rc = sqlite3_step(stmt);
			library_sqlite_error(db, rc);
			sqlite3_finalize(stmt);

			/* Run through all photos in library and insert unique identifier in library */
			gchar *identifier;
			sqlite3_prepare_v2(db, "select filename from library", -1, &stmt, NULL);
			while (sqlite3_step(stmt) == SQLITE_ROW)
			{
				filename = (gchar *) sqlite3_column_text(stmt, 0);
				if (g_file_test(filename, G_FILE_TEST_EXISTS))
				{
					identifier = rs_file_checksum(filename);
					rc = sqlite3_prepare_v2(db, "update library set identifier = ?1 WHERE filename = ?2;", -1, &stmt_update, NULL);
					rc = sqlite3_bind_text(stmt_update, 1, identifier, strlen(identifier), SQLITE_TRANSIENT);
					rc = sqlite3_bind_text(stmt_update, 2, filename, strlen(filename), SQLITE_TRANSIENT);
					rc = sqlite3_step(stmt_update);
					library_sqlite_error(db, rc);
					sqlite3_finalize(stmt_update);
					g_free(identifier);
				}
			}
			sqlite3_finalize(stmt);

			library_set_version(db, version+1);
			break;

		case 1:
			library_execute_sql(db, "BEGIN TRANSACTION;");
			sqlite3_prepare_v2(db, "select id,filename from library", -1, &stmt, NULL);
			while (sqlite3_step(stmt) == SQLITE_ROW)
			{
				id = (gint) sqlite3_column_int(stmt, 0);
				filename = rs_normalize_path((gchar *) sqlite3_column_text(stmt, 1));
				if (filename) /* FIXME: This will only work for paths that exists */
				{
					rc = sqlite3_prepare_v2(db, "update library set filename = ?1 WHERE id = ?2;", -1, &stmt_update, NULL);
					rc = sqlite3_bind_text(stmt_update, 1, filename, strlen(filename), SQLITE_TRANSIENT);
					rc = sqlite3_bind_int(stmt_update, 2, id);
					rc = sqlite3_step(stmt_update);
					library_sqlite_error(db, rc);
					sqlite3_finalize(stmt_update);
					g_free(filename);
				}
			}
			sqlite3_finalize(stmt);
			library_set_version(db, version+1);
			library_execute_sql(db, "COMMIT;");
			break;

		default:
			/* We should never hit this */
			g_debug("Some error occured in library_check_version() - please notify developers");
			break;
		}

		version++;
		g_debug("Updated library database to version %d", version);
	}
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

	/* This is not FULL synchronous mode as default, but almost as good. From
	   the sqlite3 manual:
	   "There is a very small (though non-zero) chance that a power failure at
	   just the wrong time could corrupt the database in NORMAL mode. But in
	   practice, you are more likely to suffer a catastrophic disk failure or
	   some other unrecoverable hardware fault." */
	library_execute_sql(library->db, "PRAGMA synchronous = normal;");

	/* Move our journal to memory, we're not doing banking for the Mafia */
	library_execute_sql(library->db, "PRAGMA journal_mode = memory;");

	/* Place temp tables in memory */
	library_execute_sql(library->db, "PRAGMA temp_store = memory;");

	rc = library_create_tables(library->db);
	library_sqlite_error(library->db, rc);

	library_check_version(library->db);

	library->id_lock = g_mutex_new();
}

RSLibrary *
rs_library_get_singleton(void)
{
	static GStaticMutex singleton_lock = G_STATIC_MUTEX_INIT;
	static RSLibrary *singleton = NULL;

	g_static_mutex_lock(&singleton_lock);
	if (!singleton)
		singleton = g_object_new(RS_TYPE_LIBRARY, NULL);
	g_static_mutex_unlock(&singleton_lock);

	return singleton;
}

static gint
library_execute_sql(sqlite3 *db, const gchar *sql)
{
	sqlite3_stmt *statement;

	if(SQLITE_OK != sqlite3_prepare(db, sql, -1, &statement, 0))
		return sqlite3_errcode(db);

	while (SQLITE_ROW == sqlite3_step(statement));

	return sqlite3_finalize(statement);
}

static void
library_sqlite_error(sqlite3 *db, gint result)
{
	if (result != SQLITE_OK && result != SQLITE_DONE)
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
	sqlite3_prepare_v2(db, "create table library (id integer primary key, filename varchar(1024), identifier varchar(32))", -1, &stmt, NULL);
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

	/* Create table (version) to help keeping track of database version */
	sqlite3_prepare_v2(db, "create table version (version integer)", -1, &stmt, NULL);
	rc = sqlite3_step(stmt);
	sqlite3_finalize(stmt);

	rc = sqlite3_prepare_v2(db, "select * from version", -1, &stmt, NULL);
	rc = sqlite3_step(stmt);
	sqlite3_finalize(stmt);
	if (rc != SQLITE_ROW)
	{
		/* Set current version */
		rc = sqlite3_prepare_v2(db, "insert into version (version) values (?1);", -1, &stmt, NULL);
		rc = sqlite3_bind_int(stmt, 1, LIBRARY_VERSION);
		rc = sqlite3_step(stmt);
		sqlite3_finalize(stmt);

		rc = sqlite3_prepare_v2(db, "select identifier from library", -1, &stmt, NULL);
		rc = sqlite3_step(stmt);
		sqlite3_finalize(stmt);

		/* Check if library.identifier exists */
		if (rc == SQLITE_MISUSE)
		{
			library_set_version(db, 0);
		}
	}

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

	g_mutex_lock(library->id_lock);
	rc = sqlite3_prepare_v2(db, "INSERT INTO phototags (photo, tag, autotag) VALUES (?1, ?2, ?3);", -1, &stmt, NULL);
	rc = sqlite3_bind_int (stmt, 1, photo_id);
	rc = sqlite3_bind_int (stmt, 2, tag_id);
	rc = sqlite3_bind_int (stmt, 3, autotag_tag);
	rc = sqlite3_step(stmt);
	g_mutex_unlock(library->id_lock);
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
got_checksum(const gchar *checksum, gpointer user_data)
{
	RSLibrary *library = rs_library_get_singleton();
	sqlite3 *db = library->db;
	sqlite3_stmt *stmt;

	sqlite3_prepare_v2(db, "UPDATE LIBRARY SET  identifier=?1 WHERE id=?2;", -1, &stmt, NULL);
	sqlite3_bind_text(stmt, 1, checksum, -1, SQLITE_TRANSIENT);
	sqlite3_bind_int(stmt, 2, GPOINTER_TO_INT(user_data));
	sqlite3_step(stmt);
	sqlite3_finalize(stmt);
}

static gint
library_add_photo(RSLibrary *library, const gchar *filename)
{
	gint id;
	sqlite3 *db = library->db;
	gint rc;
	sqlite3_stmt *stmt;

	g_mutex_lock(library->id_lock);
	sqlite3_prepare_v2(db, "INSERT INTO library (filename) VALUES (?1);", -1, &stmt, NULL);
	rc = sqlite3_bind_text(stmt, 1, filename, strlen(filename), SQLITE_TRANSIENT);
	rc = sqlite3_step(stmt);
	id = sqlite3_last_insert_rowid(db);
	g_mutex_unlock(library->id_lock);
	if (rc != SQLITE_DONE)
		library_sqlite_error(db, rc);
	sqlite3_finalize(stmt);

	rs_io_idle_read_checksum(filename, -1, got_checksum, GINT_TO_POINTER(id));

	library_backup_tags(library, g_path_get_dirname(filename));

	return id;
}

static gint
library_add_tag(RSLibrary *library, const gchar *tagname)
{
	gint id;
	sqlite3 *db = library->db;
	gint rc;
	sqlite3_stmt *stmt;

	g_mutex_lock(library->id_lock);
	sqlite3_prepare_v2(db, "INSERT INTO tags (tagname) VALUES (?1);", -1, &stmt, NULL);
	rc = sqlite3_bind_text(stmt, 1, tagname, strlen(tagname), SQLITE_TRANSIENT);
	rc = sqlite3_step(stmt);
	id = sqlite3_last_insert_rowid(db);
	g_mutex_unlock(library->id_lock);
	if (rc != SQLITE_DONE)
		library_sqlite_error(db, rc);
	sqlite3_finalize(stmt);

	return id;
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

gint
rs_library_add_photo(RSLibrary *library, const gchar *filename)
{
	gint photo_id;

	g_assert(RS_IS_LIBRARY(library));

	photo_id = library_find_photo_id(library, filename);
	if (photo_id == -1)
	{
		g_debug("Adding photo to library: %s",filename);
		photo_id = library_add_photo(library, filename);
	}

	return photo_id;
}

gint
rs_library_add_tag(RSLibrary *library, const gchar *tagname)
{
	gint tag_id;

	g_assert(RS_IS_LIBRARY(library));

	tag_id = library_find_tag_id(library, tagname);
	if (tag_id == -1)
	{
		g_debug("Adding tag to tags: %s",tagname);
		tag_id = library_add_tag(library, tagname);
	}

	return tag_id;
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

	library_backup_tags(library, g_path_get_dirname(filename));

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
	library_backup_tags(library, g_path_get_dirname(photo));
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
	gchar *filename;

	sqlite3_prepare_v2(db, "create temp table filter (photo integer)", -1, &stmt, NULL);
	rc = sqlite3_step(stmt);
	sqlite3_finalize(stmt);
       
	for (n = 0; n < num_tags; n++)
	{
		tag = (gchar *) g_list_nth_data(tags, n);

		g_mutex_lock(library->id_lock);
		sqlite3_prepare_v2(db, "insert into filter select phototags.photo from phototags, tags where phototags.tag = tags.id and lower(tags.tagname) = lower(?1) ;", -1, &stmt, NULL);
		rc = sqlite3_bind_text(stmt, 1, tag, strlen(tag), SQLITE_TRANSIENT);
		rc = sqlite3_step(stmt);
		sqlite3_finalize(stmt);
		g_mutex_unlock(library->id_lock);
	}

	sqlite3_prepare_v2(db, "create temp table result (photo integer, count integer)", -1, &stmt, NULL);
	rc = sqlite3_step(stmt);
	sqlite3_finalize(stmt);

	g_mutex_lock(library->id_lock);
	sqlite3_prepare_v2(db, "insert into result select photo, count(photo) from filter group by photo;", -1, &stmt, NULL);
	rc = sqlite3_step(stmt);
	sqlite3_finalize(stmt);
	g_mutex_unlock(library->id_lock);

	sqlite3_prepare_v2(db, "select library.filename from library,result where library.id = result.photo and result.count = ?1 order by library.filename;", -1, &stmt, NULL);
        rc = sqlite3_bind_int(stmt, 1, num_tags);
	while (sqlite3_step(stmt) == SQLITE_ROW)
	{
		filename = g_strdup((gchar *) sqlite3_column_text(stmt, 0));
		if (g_file_test(filename, G_FILE_TEST_EXISTS))
			photos = g_list_append(photos, filename);
	}				       
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

static void
library_photo_default_tags(RSLibrary *library, const gint photo_id, RSMetadata *metadata)
{
	g_assert(RS_IS_LIBRARY(library));

	GList *tags = NULL;

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
	library_execute_sql(library->db, "BEGIN TRANSACTION;");
	for(i = 0; i < g_list_length(tags); i++)
	{
		gchar *tag = (gchar *) g_list_nth_data(tags, i);
		gint tag_id = rs_library_add_tag(library, tag);
		library_photo_add_tag(library, photo_id, tag_id, TRUE);
		g_free(tag);
	}
	library_execute_sql(library->db, "COMMIT;");
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
	/* Fix size of iconview */
	rs_store_set_iconview_size(carrier->store, g_list_length(photos));

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
	tag_search_entry = rs_library_tag_entry_new(library);

	carrier->library = library;
	carrier->store = store;
	g_signal_connect (tag_search_entry, "changed",
			  G_CALLBACK (search_changed), carrier);
	gtk_box_pack_start (GTK_BOX(box), tag_search_entry, FALSE, TRUE, 0);
	/* FIXME: Make sure to free carrier at some point */

	return box;
}

GtkWidget *
rs_library_tag_entry_new(RSLibrary *library)
{
	g_assert(RS_IS_LIBRARY(library));

	gboolean
	selected(GtkEntryCompletion *completion, GtkTreeModel *model, GtkTreeIter *iter, gpointer user_data)
	{
		GtkEntry *entry = GTK_ENTRY(gtk_entry_completion_get_entry(completion));
		gchar *current_text, *new_text;
		gchar *tag;
		gchar *target;

		gtk_tree_model_get (model, iter, 0, &tag, -1);
		current_text = g_strdup(gtk_entry_get_text(entry));

		/* Try to find the last tag entered */
		target = strrchr(current_text, ' ');
		if (target)
			target++;
		else
			target = current_text;

		/* End the string just as the last tag starts */
		*target = '\0';

		/* Append selected tag */
		new_text = g_strconcat(current_text, tag, NULL);

		gtk_entry_set_text(entry, new_text);
		gtk_editable_set_position(GTK_EDITABLE(entry), -1);

		g_free(current_text);
		g_free(new_text);

		return TRUE;
	}

	gboolean
	match(GtkEntryCompletion *completion, const gchar *key, GtkTreeIter *iter, gpointer user_data)
	{
		gboolean found = FALSE;
		GtkTreeModel *model;
		const gchar *needle;
		gchar *needle_normalized;
		gchar *needle_case_normalized;
		gchar *tag;
		gchar *tag_normalized;
		gchar *tag_case_normalized;

		/* Look for last tag if found */
		needle = strrchr(key, ' ');
		if (needle)
			needle += 1;
		else
			needle = key;

		needle_normalized = g_utf8_normalize(needle, -1, G_NORMALIZE_ALL);
		needle_case_normalized = g_utf8_casefold(needle_normalized, -1);

		model = gtk_entry_completion_get_model (completion);
		gtk_tree_model_get (model, iter, 0, &tag, -1);
		tag_normalized = g_utf8_normalize(tag, -1, G_NORMALIZE_ALL);
		tag_case_normalized = g_utf8_casefold(tag_normalized, -1);

		if (g_str_has_prefix(tag_case_normalized, needle_case_normalized))
			found = TRUE;

		g_free(tag_normalized);

		return found;
	}

	GtkWidget *entry = gtk_entry_new();
	GtkEntryCompletion *completion = gtk_entry_completion_new();
	GtkListStore *store = gtk_list_store_new(1, G_TYPE_STRING);
	GList *all_tags = rs_library_find_tag(library, "");
	GtkTreeIter iter;

	GList *node;

	for (node = g_list_first(all_tags); node != NULL; node = g_list_next(node))
	{
		gchar *tag = node->data;
		gtk_list_store_append(store, &iter);
		gtk_list_store_set(store, &iter, 0, tag, -1);

		g_free(tag);
	}

	gtk_entry_completion_set_model(completion, GTK_TREE_MODEL(store));
	gtk_entry_completion_set_text_column(completion, 0);
	gtk_entry_completion_set_match_func(completion, match, NULL, NULL);
	g_signal_connect(completion, "match-selected", G_CALLBACK(selected), NULL);
	gtk_entry_set_completion (GTK_ENTRY(entry), completion);

	g_list_free(all_tags);

	return entry;
}

gboolean
rs_library_set_tag_search(gchar *str)
{
	if (!str)
		return FALSE;
	gtk_entry_set_text(GTK_ENTRY(tag_search_entry), str);
	return TRUE;
}

void
rs_library_add_photo_with_metadata(RSLibrary *library, const gchar *photo, RSMetadata *metadata)
{
	/* Bail out if we already know the photo */
	if (library_find_photo_id(library, photo) > -1)
		return;

	gint photo_id = library_add_photo(library, photo);
	library_photo_default_tags(library, photo_id, metadata);
	library_backup_tags(library, g_path_get_dirname(photo));
}

static void 
library_backup_tags(RSLibrary *library, const gchar *directory)
{
	sqlite3 *db = library->db;
	sqlite3_stmt *stmt;
	gint rc;
	gchar *filename = NULL, *checksum, *tag, *t_filename;
	gint autotag;

	const gchar *tagfile = g_build_filename(directory, "/.rawstudio/tags.xml", NULL);
	xmlTextWriterPtr writer;

	writer = xmlNewTextWriterFilename(tagfile, 0);
	xmlTextWriterSetIndent(writer, 1);
	xmlTextWriterStartDocument(writer, NULL, "ISO-8859-1", NULL);
	xmlTextWriterStartElement(writer, BAD_CAST "rawstudio-tags");
	xmlTextWriterWriteFormatAttribute(writer, BAD_CAST "version", "%d", LIBRARY_VERSION);

	const gchar *temp = g_strdup_printf("%s/%%", directory);
	rc = sqlite3_prepare_v2(db, "select library.filename,library.identifier,tags.tagname,phototags.autotag from library,phototags,tags where library.filename like ?1 and phototags.photo = library.id and tags.id = phototags.tag order by library.filename;", -1, &stmt, NULL);
	rc = sqlite3_bind_text(stmt, 1, temp, strlen(temp), SQLITE_TRANSIENT);
	library_sqlite_error(db, rc);
	while (sqlite3_step(stmt) == SQLITE_ROW)
	{
		t_filename = g_path_get_basename((gchar *) sqlite3_column_text(stmt, 0));
		if (g_strcmp0(t_filename, filename) != 0 || filename == NULL)
		{
			if (filename != NULL)
				xmlTextWriterEndElement(writer);
			filename = t_filename;
			xmlTextWriterStartElement(writer, BAD_CAST "file");
			xmlTextWriterWriteFormatAttribute(writer, BAD_CAST "name", "%s", filename);
			checksum = (gchar *) sqlite3_column_text(stmt, 1);
			xmlTextWriterWriteFormatAttribute(writer, BAD_CAST "checksum", "%s", checksum);
		}

		tag = (gchar *) sqlite3_column_text(stmt, 2);
		autotag = (gint) sqlite3_column_int(stmt, 3);
		xmlTextWriterStartElement(writer, BAD_CAST "tag");
		xmlTextWriterWriteFormatAttribute(writer, BAD_CAST "name", "%s", tag);
		xmlTextWriterWriteFormatAttribute(writer, BAD_CAST "auto", "%d", autotag);
		xmlTextWriterEndElement(writer);
	}
	xmlTextWriterEndElement(writer);

	rc = sqlite3_finalize(stmt);

	xmlTextWriterEndDocument(writer);
	xmlFreeTextWriter(writer);
	return;
}

void 
rs_library_restore_tags(const gchar *directory)
{
	RSLibrary *library = rs_library_get_singleton();
	const gchar *tagfile = g_build_filename(directory, "/.rawstudio/tags.xml", NULL);
	if (!g_file_test(tagfile, G_FILE_TEST_EXISTS))
	    return;

	xmlDocPtr doc;
	xmlNodePtr cur, cur2;
	xmlChar *val;
	gint version;

	gchar *filename, *identifier, *tagname;
	gint autotag, photoid, tagid;

	doc = xmlParseFile(tagfile);
	if (!doc)
		return;

	cur = xmlDocGetRootElement(doc);

	if ((!xmlStrcmp(cur->name, BAD_CAST "rawstudio-tags")))
	{
		val = xmlGetProp(cur, BAD_CAST "version");
		if (val)
			version = atoi((gchar *) val);
	}

	cur = cur->xmlChildrenNode;
	while(cur)
	{
		if ((!xmlStrcmp(cur->name, BAD_CAST "file")))
		{
			val = xmlGetProp(cur, BAD_CAST "name");
			filename = g_build_filename(directory, val, NULL);
			xmlFree(val);

			photoid = library_find_photo_id(library, filename);
			if ( photoid == -1)
			{
				photoid = rs_library_add_photo(library, filename);

				val = xmlGetProp(cur, BAD_CAST "checksum");
				identifier = (gchar *) val;

				cur2 = cur->xmlChildrenNode;
				while(cur2)
				{
					if ((!xmlStrcmp(cur2->name, BAD_CAST "tag")))
					{
						val = xmlGetProp(cur2, BAD_CAST "name");
						tagname =(gchar*) val;
						tagid = library_find_tag_id(library, tagname);
						if ( tagid == -1)
							tagid = rs_library_add_tag(library, tagname);

						val = xmlGetProp(cur2, BAD_CAST "auto");
						autotag = atoi((gchar *) val);
						xmlFree(val);

						library_photo_add_tag(library, photoid, tagid, (autotag == 1));

						xmlFree(tagname);
					}
					cur2 = cur2->next;
				}
				xmlFree(identifier);
			}
			g_free(filename);
		}
		cur = cur->next;
	}

	xmlFreeDoc(doc);
	return;
}
