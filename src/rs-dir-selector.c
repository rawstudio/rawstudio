/*
 * Copyright (C) 2006-2008 Anders Brander <anders@brander.dk> and 
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

#include <gtk/gtk.h>
#include "rs-dir-selector.h"

static void add_element(GtkTreeStore *treestore, GtkTreeIter *iter, gchar *name, gchar *path);
static void onRowActivated (GtkTreeView *view, GtkTreePath *path, 
							GtkTreeViewColumn *col, gpointer user_data);
static void onRowExpanded (GtkTreeView *view, GtkTreeIter *iter,
						   GtkTreePath *path, gpointer user_data);
static void onRowCollapsed (GtkTreeView *view, GtkTreeIter *iter,
							GtkTreePath *path, gpointer user_data);
static GtkTreeModel *create_and_fill_model (gchar *root);

/**
 * Adds an element to treestore
 * @param treestore A GtkTreeStore
 * @param iter A GtkTreeIter
 * @param name A gchar
 * @param path A gchar
 */
void
add_element(GtkTreeStore *treestore, GtkTreeIter *iter, gchar *name, gchar *path)
{
	GtkTreeIter this, child;

	gtk_tree_store_append(treestore, &this, iter);
	gtk_tree_store_set(treestore, &this,
					   COL_NAME, name,
					   COL_PATH, path,
					   -1);
	gtk_tree_store_append(treestore, &child, &this);
	gtk_tree_store_set(treestore, &child,
	-1);
}

/**
 * Callback after activating a row
 * @param view A GtkTreeView
 * @param path A GtkTreePath
 * @param col A GtkTreeViewColumn
 * @param user_data A gpointer
 */
static void
onRowActivated (GtkTreeView *view,
				GtkTreePath *path,
				GtkTreeViewColumn *col,
				gpointer user_data)
{
	GtkTreeModel *model;
	GtkTreeIter iter;
	gchar *filepath;

	model = gtk_tree_view_get_model(view);
	gtk_tree_model_get_iter(model, &iter, path);
	gtk_tree_model_get(GTK_TREE_MODEL(model), &iter,
					   COL_PATH, &filepath,
					   -1);
	/* FIXME: Insert magic to open directory here */
}

/**
 * Callback after expanding a row
 * @param view A GtkTreeView
 * @param iter A GtkTreeIter
 * @param path A GtkTreePath
 * @param user_data A gpointer
 */
static void
onRowExpanded (GtkTreeView *view,
			   GtkTreeIter *iter,
			   GtkTreePath *path,
			   gpointer user_data)
{
	GtkTreeModel *model;
	GtkTreeIter empty;
	gchar *filepath;
	gchar *file;
	GDir *dir;
	GString *gs = NULL;
	model = gtk_tree_view_get_model(view);
	gtk_tree_model_iter_children(GTK_TREE_MODEL(model),
								 &empty, iter);
	gtk_tree_model_get(GTK_TREE_MODEL(model), iter,
					   COL_PATH, &filepath,
					   -1);

	dir = g_dir_open(filepath, 0, NULL);

	while ((file = g_dir_read_name(dir))) 
	{
		gs = g_string_new(filepath);
		g_string_append(gs, file);
		g_string_append(gs, "/");
		if (g_file_test(gs->str, G_FILE_TEST_IS_DIR)) 
		{
			if (file[0] == '.') 
			{
				/* Fixme: If hidden files should be shown too */
			} 
			else
			{
				add_element(GTK_TREE_STORE(model), iter, file, gs->str);
			}
		}
		g_string_free(gs, TRUE);	
	}
	g_dir_close(dir);
	g_free(filepath);
	g_free(file);

	gtk_tree_store_remove(GTK_TREE_STORE(model), &empty);
}

/**
 * Callback after collapsing a row
 * @param view A GtkTreeView
 * @param iter A GtkTreeIter
 * @param path A GtkTreePath
 * @param user_data A gpointer
 */
static void
onRowCollapsed (GtkTreeView *view,
				GtkTreeIter *iter,
				GtkTreePath *path,
				gpointer user_data)
{
	GtkTreeModel *model;
	GtkTreeIter child;

	model = gtk_tree_view_get_model(view);
	while (gtk_tree_model_iter_children(GTK_TREE_MODEL(model), &child, iter)) 
	{
		gtk_tree_store_remove(GTK_TREE_STORE(model), &child);
	}
	add_element(GTK_TREE_STORE(model), iter, NULL, NULL);
}

/**
 * Creates a GtkTreeStore and fills it with data
 * @param root A gchar
 * @return A GtkTreeModel
 */
static GtkTreeModel *
create_and_fill_model (gchar *root)
{
	GtkTreeStore *treestore;

	treestore = gtk_tree_store_new(NUM_COLS, G_TYPE_STRING, G_TYPE_STRING);
	add_element(treestore, NULL, root, root);
	return GTK_TREE_MODEL(treestore);
}

/**
 * Creates a GtkWidget
 * @param root A gchar
 * @return A GtkWidget
 */
GtkWidget *
rs_dir_selector(gchar *root)
{
	GtkTreeViewColumn *col;
	GtkCellRenderer *renderer;
	GtkWidget *view;
	GtkTreeModel *model;
	GtkTreeSortable *sortable;
	GtkWidget *scroller;

	view = gtk_tree_view_new();
	col = gtk_tree_view_column_new();
	gtk_tree_view_append_column(GTK_TREE_VIEW(view), col);
	renderer = gtk_cell_renderer_text_new();
	gtk_tree_view_column_pack_start(col, renderer, TRUE);
	gtk_tree_view_column_add_attribute (col, renderer, "text",
										COL_NAME);
	model = create_and_fill_model(root);

	sortable = GTK_TREE_SORTABLE(model);
	gtk_tree_sortable_set_sort_column_id(sortable,
										 COL_NAME,
										 GTK_SORT_ASCENDING);
	gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(view), FALSE);

	gtk_tree_view_set_model(GTK_TREE_VIEW(view), model);

	g_signal_connect(view, "row-activated", G_CALLBACK(onRowActivated), NULL);
	g_signal_connect(view, "row-expanded", G_CALLBACK(onRowExpanded), NULL);
	g_signal_connect(view, "row-collapsed", G_CALLBACK(onRowCollapsed), NULL);

	g_object_unref(model); /* destroy model automatically with view */

	gtk_tree_selection_set_mode(gtk_tree_view_get_selection(GTK_TREE_VIEW(view)),
								GTK_SELECTION_NONE);

	scroller = gtk_scrolled_window_new(NULL, NULL);
	gtk_container_add (GTK_CONTAINER (scroller), view);

	return scroller;
}
