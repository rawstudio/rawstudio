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

#ifndef RS_LIBRARY_H
#define RS_LIBRARY_H

#include <glib-object.h>
#include <glib.h>
#include <gtk/gtk.h>
#include "application.h"
#include <rawstudio.h>

G_BEGIN_DECLS

#define RS_TYPE_LIBRARY rs_library_get_type()
#define RS_LIBRARY(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), RS_TYPE_LIBRARY, RSLibrary))
#define RS_LIBRARY_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), RS_TYPE_LIBRARY, RSLibraryClass))
#define RS_IS_LIBRARY(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), RS_TYPE_LIBRARY))
#define RS_IS_LIBRARY_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), RS_TYPE_LIBRARY))
#define RS_LIBRARY_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), RS_TYPE_LIBRARY, RSLibraryClass))

typedef struct _RSLibrary RSLibrary;

typedef struct {
	GObjectClass parent_class;
} RSLibraryClass;

GType rs_library_get_type(void);

RSLibrary *rs_library_get_singleton(void);
void rs_library_add_photo(RSLibrary *library, const gchar *filename);
void rs_library_add_tag(RSLibrary *library, const gchar *tagname);
void rs_library_photo_add_tag(RSLibrary *library, const gchar *filename, const gchar *tagname, const gboolean autotag);
void rs_library_delete_photo(RSLibrary *library, const gchar *photo);
gboolean rs_library_delete_tag(RSLibrary *library, const gchar *tag, const gboolean force);
GList *rs_library_search(RSLibrary *library, GList *tags);
void rs_library_photo_default_tags(RSLibrary *library, const gchar *photo, RSMetadata *metadata);
GList *rs_library_photo_tags(RSLibrary *library, const gchar *photo, const gboolean autotag);
GList *rs_library_find_tag(RSLibrary *library, const gchar *tag);
GtkWidget *rs_library_toolbox_new(RSLibrary *library, RSStore *store);
GtkWidget *rs_library_tag_entry_new(RSLibrary *library);
gboolean rs_library_set_tag_search(gchar *str);
void rs_library_add_photo_with_metadata(RSLibrary *library, const gchar *photo, RSMetadata *metadata);

G_END_DECLS

#endif /* RS_LIBRARY_H */
