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

#ifndef RS_STORE_H
#define RS_STORE_H

#include <gtk/gtk.h>

typedef struct _RSStoreClass       RSStoreClass;

struct _RSStoreClass
{
	GtkHBoxClass parent_class;
};

typedef enum {
	RS_STORE_SORT_BY_NAME,
	RS_STORE_SORT_BY_TIMESTAMP,
	RS_STORE_SORT_BY_ISO,
	RS_STORE_SORT_BY_APERTURE,
	RS_STORE_SORT_BY_FOCALLENGTH,
	RS_STORE_SORT_BY_SHUTTERSPEED,
} RS_STORE_SORT_METHOD;

GType rs_store_get_type (void);

/**
 * Creates a new RSStore
 * @return A new GtkWidget
 */
extern GtkWidget *
rs_store_new(void);

#define RS_STORE_TYPE_WIDGET             (rs_store_get_type ())
#define RS_STORE(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), RS_STORE_TYPE_WIDGET, RSStore))
#define RS_STORE_CLASS(obj)       (G_TYPE_CHECK_CLASS_CAST ((obj), RS_STORE_WIDGET, RSStoreClass))
#define RS_IS_STORE(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), RS_STORE_TYPE_WIDGET))
#define RS_IS_STORE_CLASS(obj)    (G_TYPE_CHECK_CLASS_TYPE ((obj), RS_STORE_TYPE_WIDGET))
#define RS_STORE_GET_CLASS        (G_TYPE_INSTANCE_GET_CLASS ((obj), RS_STORE_TYPE_WIDGET, RSStoreClass))

/**
 * Load thumbnails from a directory into the store
 * @param store A RSStore
 * @param path The path to load
 * @return The number of files loaded or -1
 */
extern gint
rs_store_load_directory(RSStore *store, const gchar *path);

/**
 * Set priority and exported flags of a thumbnail
 * @param store A RSStore
 * @param filename The name of the thumbnail to remove or NULL
 * @param iter The iter of the thumbnail to remove or NULL
 * @param priority The priority or NULL to leave unchanged
 * @param exported The exported status or NULL to leave unchanged
 * @return TRUE if succeeded
 */
extern gboolean
rs_store_set_flags(RSStore *store, const gchar *filename, GtkTreeIter *iter,
	const guint *priority, const gboolean *exported);

/**
 * Select one image
 * @param store A RSStore
 * @param name The filename to select
 */
extern gboolean
rs_store_set_selected_name(RSStore *store, const gchar *filename);

/**
 * Remove thumbnail(s) from store
 * @param store A RSStore
 * @param filename The name of the thumbnail to remove or NULL
 * @param iter The iter of the thumbnail to remove or NULL (If both name and iter is NULL, ALL thumbnails will be removed)
 */
extern void
rs_store_remove(RSStore *store, const gchar *filename, GtkTreeIter *iter);

/**
 * Get a list of currently selected thumbnail iters
 * @param store A RSStore
 * @return a GList or NULL
 */
extern GList *
rs_store_get_selected_iters(RSStore *store);

/**
 * Get a list of currently selected thumbnail names
 * @param store A RSStore
 * @return a GList or NULL
 */
extern GList *
rs_store_get_selected_names(RSStore *store);

/**
 * Get a list of photo names
 * @param store A RSStore
 * @param selected GList for selected thumbs or NULL
 * @param visible GList for visible (in currently selected iconview) thumbs or NULL
 * @param all GList for all photos or NULL
 */
extern void
rs_store_get_names(RSStore *store, GList **selected, GList **visible, GList **all);

/**
 * Show filenames in the thumbnail browser
 * @param store A RSStore
 * @param show_filenames If TRUE filenames will be visible
 */
extern void
rs_store_set_show_filenames(RSStore *store, gboolean show_filenames);

/**
 * Return a GList of iters with a specific priority
 * @param store A RSStore
 * @param priority The priority of interest
 * @return A GList of GtkTreeIters
 */
extern GList *
rs_store_get_iters_with_priority(RSStore *store, guint priority);

/**
 * Get the filename of an image
 * @param store A RSStore
 * @param iter The iter of the thumbnail
 * @return a filename or NULL if failed
 */
extern gchar *
rs_store_get_name(RSStore *store, GtkTreeIter *iter);

/**
 * Selects the previous or next thumbnail
 * @param store A RSStore
 * @param current_filename Current filename or NULL if none
 * @param direction 1: previous, 2: next
 */
gboolean
rs_store_select_prevnext(RSStore *store, const gchar *current_filename, guint direction);

/**
 * Switches to the page number page_num
 * @note Should behave like gtk_notebook_set_current_page()
 * @param store A RSStore
 * @param page_num index of the page to switch to, starting from 0. If negative,
          the last page will be used. If greater than the number of pages in the notebook,
          nothing will be done.
 */
extern void
rs_store_set_current_page(RSStore *store, gint page_num);

/**
 * Returns the page number of the current page.
 * @note Should behave like gtk_notebook_get_current_page()
 * @param store A RSStore
 * @return the index (starting from 0) of the current page in the notebook. If the notebook
           has no pages, then -1 will be returned.
 */
extern gint
rs_store_get_current_page(RSStore *store);

/**
 * Set the sorting method for a RSStore
 * @param store A RSStore
 * @param sort_method A sort method from the RS_STORE_SORT_BY-family of enums
 */
extern void
rs_store_set_sort_method(RSStore *store, RS_STORE_SORT_METHOD sort_method);

/**
 * Get the sorting method for a RSStore
 * @param store A RSStore
 * @return A sort method from the RS_STORE_SORT_BY-family of enums
 */
extern RS_STORE_SORT_METHOD
rs_store_get_sort_method(RSStore *store);

/**
 * Marks a selection of thumbnails as a group
 * @param store A RSStore
 */
void
rs_store_group_photos(RSStore *store);

/**
 * Ungroup a group or selection of groups
 * @param store A RSStore
 */
void
rs_store_ungroup_photos(RSStore *store);

gint
rs_store_selection_n_groups(RSStore *store, GList *selected);

extern GList *
rs_store_sort_selected(GList *selected);

extern void
rs_store_auto_group(RSStore *store);

extern void
rs_store_group_select_name(RSStore *store, const gchar *filename);

extern void
rs_store_group_ungroup_name(RSStore *store, const gchar *filename);

extern void
rs_store_load_file(RSStore *store, gchar *fullname);

extern void
rs_store_set_iconview_size(RSStore *store, gint size);

#endif /* RS_STORE_H */
