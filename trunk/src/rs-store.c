/*
 * Copyright (C) 2006, 2007 Anders Brander <anders@brander.dk> and 
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
#include <glib/gprintf.h>
#include <config.h>
#include "rawstudio.h"
#include "conf_interface.h"
#include "gettext.h"
#include "rs-store.h"
#include "gtk-helper.h"
#include "gtk-progress.h"
#include "rs-cache.h"

/* How many different icon views do we have (tabs) */
#define NUM_VIEWS 6

enum {
	PIXBUF_COLUMN, /* The displayed pixbuf */
	PIXBUF_CLEAN_COLUMN, /* The clean thumbnail */
	TEXT_COLUMN, /* Icon text */
	FULLNAME_COLUMN, /* Full path to image */
	PRIORITY_COLUMN,
	EXPORTED_COLUMN,
	NUM_COLUMNS
};

struct _RSStore
{
	GtkNotebook parent;
	GtkWidget *iconview[NUM_VIEWS];
	GtkWidget *current_iconview;
	guint current_priority;
	GtkListStore *store;
	gulong counthandler;
	gchar *last_path;
};

/* Define the boiler plate stuff using the predefined macro */
G_DEFINE_TYPE (RSStore, rs_store, GTK_TYPE_NOTEBOOK);

enum {
	THUMB_ACTIVATED_SIGNAL,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

/* FIXME: Remember to remove stores from this too! */
static GList *all_stores = NULL;

/* Priorities to show */
const static guint priorities[NUM_VIEWS] = {PRIO_ALL, PRIO_1, PRIO_2, PRIO_3, PRIO_U, PRIO_D};
#if NUM_VIEWS != 6
 #error This must be updated
#endif

static void thumbnail_overlay(GdkPixbuf *pixbuf, GdkPixbuf *pixbuf_priority, GdkPixbuf *pixbuf_exported);
static void thumbnail_update(GdkPixbuf *pixbuf, GdkPixbuf *pixbuf_clean, gint priority, gboolean exported);
static void switch_page(GtkNotebook *notebook, GtkNotebookPage *page, guint page_num, gpointer data);
static void selection_changed(GtkIconView *iconview, gpointer data);
static GtkWidget *make_iconview(GtkWidget *iconview, RSStore *store, gint prio);
static gboolean model_filter_prio(GtkTreeModel *model, GtkTreeIter *iter, gpointer data);
static void count_priorities_del(GtkTreeModel *treemodel, GtkTreePath *path, gpointer data);
static void count_priorities(GtkTreeModel *treemodel, GtkTreePath *do_not_use1, GtkTreeIter *do_not_use2, gpointer data);
static void icon_get_selected_iters(GtkIconView *iconview, GtkTreePath *path, gpointer user_data);
static void icon_get_selected_names(GtkIconView *iconview, GtkTreePath *path, gpointer user_data);
static gboolean tree_foreach_names(GtkTreeModel *model, GtkTreePath *path, GtkTreeIter *iter, gpointer data);
static gboolean tree_find_filename(GtkTreeModel *store, const gchar *filename, GtkTreeIter *iter, GtkTreePath **path);

/**
 * Class initializer
 */
static void
rs_store_class_init(RSStoreClass *klass)
{
	GtkWidgetClass *widget_class;
	GtkObjectClass *object_class;
	widget_class = GTK_WIDGET_CLASS(klass);
	object_class = GTK_OBJECT_CLASS(klass);

	signals[THUMB_ACTIVATED_SIGNAL] = g_signal_new ("thumb-activated",
		G_TYPE_FROM_CLASS (klass),
		G_SIGNAL_RUN_FIRST,
		0,
		NULL, 
		NULL,                
		g_cclosure_marshal_VOID__STRING,
		G_TYPE_NONE,
		1,
		G_TYPE_STRING);
}

/**
 * Instance initialization
 */
static void
rs_store_init(RSStore *store)
{
	GtkNotebook *notebook = GTK_NOTEBOOK(store);
	gint n;
	gchar label_text[NUM_VIEWS][63];
	GtkWidget **label = g_new(GtkWidget *, NUM_VIEWS);
	GtkWidget *label_tt[NUM_VIEWS];

	store->store = gtk_list_store_new (NUM_COLUMNS,
		GDK_TYPE_PIXBUF,
		GDK_TYPE_PIXBUF,
		G_TYPE_STRING,
		G_TYPE_STRING,
		G_TYPE_INT,
		G_TYPE_BOOLEAN);

	for (n=0;n<NUM_VIEWS;n++)
	{
		GtkTreeModel *filter;

		/* New Icon view */
		store->iconview[n] = gtk_icon_view_new();

		/* New filter */
		filter = gtk_tree_model_filter_new(GTK_TREE_MODEL (store->store), NULL);

		/* Set the function used to determine "visibility" */
		gtk_tree_model_filter_set_visible_func(GTK_TREE_MODEL_FILTER (filter),
			model_filter_prio, GINT_TO_POINTER (priorities[n]), NULL);

		/* Attach the model to iconview */
		gtk_icon_view_set_model (GTK_ICON_VIEW (store->iconview[n]), filter);

		label[n] = gtk_label_new(NULL);

		switch (n)
		{
			case 0: /* All */
				g_sprintf(label_text[n], _("* <small>(%d)</small>"), 0);
				label_tt[n] = gui_tooltip_no_window(label[n], _("All photos (excluding deleted)"), NULL);
				break;
			case 1: /* 1 */
				g_sprintf(label_text[n], _("1 <small>(%d)</small>"), 0);
				label_tt[n] = gui_tooltip_no_window(label[n], _("Priority 1 photos"), NULL);
				break;
			case 2: /* 2 */
				g_sprintf(label_text[n], _("2 <small>(%d)</small>"), 0);
				label_tt[n] = gui_tooltip_no_window(label[n], _("Priority 2 photos"), NULL);
				break;
			case 3: /* 3 */
				g_sprintf(label_text[n], _("3 <small>(%d)</small>"), 0);
				label_tt[n] = gui_tooltip_no_window(label[n], _("Priority 3 photos"), NULL);
				break;
			case 4: /* Unsorted */
				g_sprintf(label_text[n], _("U <small>(%d)</small>"), 0);
				label_tt[n] = gui_tooltip_no_window(label[n], _("Unprioritized photos"), NULL);
				break;
			case 5: /* Deleted */
				g_sprintf(label_text[n], _("D <small>(%d)</small>"), 0);
				label_tt[n] = gui_tooltip_no_window(label[n], _("Deleted photos"), NULL);
				break;
#if NUM_VIEWS != 6
 #error You need to update this switch statement
#endif
		}

		gtk_label_set_markup(GTK_LABEL(label[n]), label_text[n]);
		gtk_misc_set_alignment(GTK_MISC(label[n]), 0.0, 0.5);

		/* Add everything to the notebook */
		gtk_notebook_append_page(notebook, make_iconview(store->iconview[n], store, priorities[n]), label_tt[n]);
	}

	/* Default to page 0 */
	store->current_iconview = store->iconview[0];
	store->current_priority = priorities[0];

	gtk_notebook_set_tab_pos(notebook, GTK_POS_LEFT);

	g_signal_connect(notebook, "switch-page", G_CALLBACK(switch_page), store);
	store->counthandler = g_signal_connect(store->store, "row-changed", G_CALLBACK(count_priorities), label);
	g_signal_connect(store->store, "row-deleted", G_CALLBACK(count_priorities_del), label);

	all_stores = g_list_append(all_stores, store);

	store->last_path = NULL;
}

static void
switch_page(GtkNotebook *notebook, GtkNotebookPage *page, guint page_num, gpointer data)
{
	RSStore *store = RS_STORE(data);

	store->current_iconview = store->iconview[page_num];
	store->current_priority = priorities[page_num];
	return;
}

static void
selection_changed(GtkIconView *iconview, gpointer data)
{
	GList *selected = NULL;
	gint num_selected;

	/* Get list of selected icons */
	gtk_icon_view_selected_foreach(iconview, icon_get_selected_names, &selected);

	num_selected = g_list_length(selected);

	/* Emit signal if only one thumbnail is selected */
	if (num_selected == 1)
		g_signal_emit (G_OBJECT (data), signals[THUMB_ACTIVATED_SIGNAL], 0, g_list_nth_data(selected, 0));

	g_list_free(selected);
}

static GtkWidget *
make_iconview(GtkWidget *iconview, RSStore *store, gint prio)
{
	GtkWidget *scroller;
	gboolean show_filenames;

	/* This must be before gtk_icon_view_set_text_column() if we're not to crash later (GTK+ <2.10) */
	gtk_icon_view_set_pixbuf_column (GTK_ICON_VIEW (iconview), PIXBUF_COLUMN);

	rs_conf_get_boolean_with_default(CONF_SHOW_FILENAMES, &show_filenames, TRUE);

	if (show_filenames)
		gtk_icon_view_set_text_column (GTK_ICON_VIEW (iconview), TEXT_COLUMN);
	else
		gtk_icon_view_set_text_column (GTK_ICON_VIEW (iconview), -1);

	/* We must be abletoselect multiple icons */
	gtk_icon_view_set_selection_mode(GTK_ICON_VIEW (iconview), GTK_SELECTION_MULTIPLE);

	/* pack them as close af possible */
	gtk_icon_view_set_column_spacing(GTK_ICON_VIEW (iconview), 0);

	/* 160 pixels should be enough */
	gtk_widget_set_size_request (iconview, -1, 160);

	/* Let us know if selection changes */
	g_signal_connect((gpointer) iconview, "selection_changed",
		G_CALLBACK (selection_changed), store);

	scroller = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scroller),
		GTK_POLICY_AUTOMATIC, GTK_POLICY_NEVER);
	gtk_container_add (GTK_CONTAINER (scroller), iconview);

	return(scroller);
}

static gboolean
model_filter_prio(GtkTreeModel *model, GtkTreeIter *iter, gpointer data)
{
	gint p;
	gint prio = GPOINTER_TO_INT (data);
	gtk_tree_model_get (model, iter, PRIORITY_COLUMN, &p, -1);
	switch(prio)
	{
		case PRIO_ALL:
			switch (p)
			{
				case PRIO_D:
					return(FALSE);
					break;
				default:
					return(TRUE);
					break;
			}
		case PRIO_U:
			switch (p)
			{
				case PRIO_1:
				case PRIO_2:
				case PRIO_3:
				case PRIO_D:
					return(FALSE);
					break;
				default:
					return(TRUE);
					break;
			}
		default:
			if (prio==p) return(TRUE);
			break;
	}
	return(FALSE);
}

static gint
model_sort_name(GtkTreeModel *model, GtkTreeIter *tia, GtkTreeIter *tib, gpointer userdata)
{
	gint ret;
	gchar *a, *b;

	gtk_tree_model_get(model, tia, TEXT_COLUMN, &a, -1);
	gtk_tree_model_get(model, tib, TEXT_COLUMN, &b, -1);
	ret = g_utf8_collate(a,b);
	g_free(a);
	g_free(b);
	return(ret);
}

static void
count_priorities_del(GtkTreeModel *treemodel, GtkTreePath *path, gpointer data)
{
	count_priorities(treemodel, path, NULL, data);
	return;
}

static void
count_priorities(GtkTreeModel *treemodel, GtkTreePath *do_not_use1, GtkTreeIter *do_not_use2, gpointer data)
{
	GtkWidget **label = (GtkWidget **) data;
	GtkTreeIter iter;
	GtkTreePath *path;
	gint priority;
	gint count_1 = 0;
	gint count_2 = 0;
	gint count_3 = 0;
	gint count_u = 0;
	gint count_d = 0;
	gint count_all;
	gchar label_text[NUM_VIEWS][63];
	gint i;

	path = gtk_tree_path_new_first();
	if (gtk_tree_model_get_iter(treemodel, &iter, path))
	{
		do {
			gtk_tree_model_get(treemodel, &iter, PRIORITY_COLUMN, &priority, -1);
			switch (priority)
			{
				case PRIO_1:
					count_1++;
					break;
				case PRIO_2:
					count_2++;
					break;
				case PRIO_3:
					count_3++;
					break;
				case PRIO_U:
					count_u++;
					break;
				case PRIO_D:
					count_d++;
					break;
			}
		} while(gtk_tree_model_iter_next (treemodel, &iter));
	}	

	gtk_tree_path_free(path);
	count_all = count_1+count_2+count_3+count_u;
	g_sprintf(label_text[0], _("* <small>(%d)</small>"), count_all);
	g_sprintf(label_text[1], _("1 <small>(%d)</small>"), count_1);
	g_sprintf(label_text[2], _("2 <small>(%d)</small>"), count_2);
	g_sprintf(label_text[3], _("3 <small>(%d)</small>"), count_3);
	g_sprintf(label_text[4], _("U <small>(%d)</small>"), count_u);
	g_sprintf(label_text[5], _("D <small>(%d)</small>"), count_d);
#if NUM_VIEWS != 6
 #error Update this accordingly
#endif

	for(i=0;i<NUM_VIEWS;i++)
		gtk_label_set_markup(GTK_LABEL(label[i]), label_text[i]);

	return;
}

static void
thumbnail_overlay(GdkPixbuf *pixbuf, GdkPixbuf *left, GdkPixbuf *right)
{
	gint thumb_width;
	gint thumb_height;
	gint icon_width;
	gint icon_height;

	thumb_width = gdk_pixbuf_get_width(pixbuf);
	thumb_height = gdk_pixbuf_get_height(pixbuf);

	/* Apply lower left icon */
	if (left)
	{
		icon_width = gdk_pixbuf_get_width(left);
		icon_height = gdk_pixbuf_get_height(left);

		gdk_pixbuf_composite(left, pixbuf,
				2, thumb_height-icon_height-2,
				icon_width, icon_height,
				2, thumb_height-icon_height-2,
				1.0, 1.0, GDK_INTERP_NEAREST, 255);
	}

	/* Apply lower right icon */
	if (right)
	{
		icon_width = gdk_pixbuf_get_width(right);
		icon_height = gdk_pixbuf_get_height(right);

		gdk_pixbuf_composite(right, pixbuf,
				thumb_width-icon_width-2, thumb_height-icon_height-2,
				icon_width, icon_height,
				thumb_width-icon_width-2, thumb_height-icon_height-2,
				1.0, 1.0, GDK_INTERP_NEAREST, 255);
	}

	return;
}

static void
thumbnail_update(GdkPixbuf *pixbuf, GdkPixbuf *pixbuf_clean, gint priority, gboolean exported)
{
	GdkPixbuf *icon_priority_temp;
	GdkPixbuf *icon_exported_temp;

	gdk_pixbuf_copy_area(pixbuf_clean,
			0,0,
			gdk_pixbuf_get_width(pixbuf_clean),
			gdk_pixbuf_get_height(pixbuf_clean),
			pixbuf,0,0);

	switch(priority)
	{
		case PRIO_1:
			icon_priority_temp = icon_priority_1;
			break;
		case PRIO_2:
			icon_priority_temp = icon_priority_2;
			break;
		case PRIO_3:
			icon_priority_temp = icon_priority_3;
			break;
		case PRIO_D:
			icon_priority_temp = icon_priority_D;
			break;
		default:
			icon_priority_temp = NULL;
	}
	if (exported)
		icon_exported_temp = icon_exported;
	else
		icon_exported_temp = NULL;

	thumbnail_overlay(pixbuf, icon_exported_temp, icon_priority_temp);
}

static void
icon_get_selected_iters(GtkIconView *iconview, GtkTreePath *path, gpointer user_data)
{
	GList **selected = user_data;
	GtkTreeModel *model = gtk_icon_view_get_model (iconview);
	GtkTreeIter iter;
	GtkTreeIter *tmp;

	if (gtk_tree_model_get_iter(model, &iter, path))
	{
		tmp = g_new(GtkTreeIter, 1);
		gtk_tree_model_filter_convert_iter_to_child_iter((GtkTreeModelFilter *)model, tmp, &iter);
		*selected = g_list_prepend(*selected, tmp);
	}
}

static void
icon_get_selected_names(GtkIconView *iconview, GtkTreePath *path, gpointer user_data)
{
	gchar *name;
	GList **selected = user_data;
	GtkTreeModel *model = gtk_icon_view_get_model (iconview);
	GtkTreeIter iter;

	if (gtk_tree_model_get_iter(model, &iter, path))
	{
		gtk_tree_model_get (model, &iter, FULLNAME_COLUMN, &name, -1);
		*selected = g_list_prepend(*selected, name);
	}
}

static gboolean
tree_foreach_names(GtkTreeModel *model, GtkTreePath *path, GtkTreeIter *iter, gpointer data)
{
	GList **names = data;
	gchar *fullname;
	gtk_tree_model_get(model, iter, FULLNAME_COLUMN, &fullname, -1);
	*names = g_list_prepend(*names, fullname);

	return FALSE; /* Continue */
}

static gboolean
tree_find_filename(GtkTreeModel *store, const gchar *filename, GtkTreeIter *iter, GtkTreePath **path)
{
	GtkTreeIter i;
	GtkTreePath *p;
	gchar *name;
	gboolean ret = FALSE;

	if (!store) return FALSE;
	if (!filename) return FALSE;

	p = gtk_tree_path_new_first();
	if (gtk_tree_model_get_iter(store, &i, p))
	{
		do {
			gtk_tree_model_get(store, &i, FULLNAME_COLUMN, &name, -1);
			if (g_utf8_collate(name, filename)==0)
			{
				if (iter)
					*iter = i;
				if (path)
					*path = gtk_tree_model_get_path(store, &i);
				ret = TRUE;
				break;
			}
		} while(gtk_tree_model_iter_next (store, &i));
	}
	gtk_tree_path_free(p);

	return ret;
}

/* Public functions */

/**
 * Creates a new RSStore
 * @return A new RSStore
 */
GtkWidget *
rs_store_new(void)
{
	return g_object_new (RS_STORE_TYPE_WIDGET, NULL);
}

/**
 * Remove thumbnail(s) from store
 * @param store A RSStore
 * @param filename The name of the thumbnail to remove or NULL
 * @param iter The iter of the thumbnail to remove or NULL (If both name and iter is NULL, ALL thumbnails will be removed)
 */
void
rs_store_remove(RSStore *store, const gchar *filename, GtkTreeIter *iter)
{
	GtkTreeIter i;

	/* If we got no store, iterate though all */
	if (!store)
	{
		gint i;
		for (i=0;i<g_list_length(all_stores);i++)
			rs_store_remove(g_list_nth_data(all_stores, i), filename, iter);
		return;
	}

	/* By now we should have a valid store */
	g_return_if_fail (RS_IS_STORE(store));

	/* If we got filename, but no iter, try to find correct iter */
	if (filename && (!iter))
		if (tree_find_filename(GTK_TREE_MODEL(store->store), filename, &i, NULL))
			iter = &i;

	/* We got iter, just remove it */
	if (iter)
		gtk_list_store_remove(GTK_LIST_STORE(GTK_TREE_MODEL(store->store)), iter);

	/* If both are NULL, remove everything */
	if ((filename == NULL) && (iter == NULL))
		gtk_list_store_clear(store->store);
}

/**
 * Load thumbnails from a directory into the store
 * @param store A RSStore
 * @param path The path to load
 * @return The number of files loaded or -1
 */
gint
rs_store_load_directory(RSStore *store, const gchar *path)
{
	gchar *name;
	GtkTreeIter iter;
	GdkPixbuf *pixbuf;
	GdkPixbuf *pixbuf_clean;
	GError *error;
	GDir *dir;
	GtkTreeSortable *sortable;
	gint priority;
	gboolean exported = FALSE;
	RS_FILETYPE *filetype;
	RS_PROGRESS *rsp;
	gboolean load_8bit = FALSE;
	gint items=0, n;
	GtkTreePath *treepath;
	GdkPixbuf *missing_thumb;

	g_return_val_if_fail(RS_IS_STORE(store), -1);
	if (!path)
	{
		if (store->last_path)
			path = store->last_path;
		else
			return -1;
	}
	else
	{
		if (store->last_path)
			g_free(store->last_path);
		store->last_path = g_strdup(path);
	}

	/* We will use this, if no thumbnail can be loaded */
	missing_thumb = gtk_widget_render_icon(GTK_WIDGET(store),
		GTK_STOCK_MISSING_IMAGE, GTK_ICON_SIZE_DIALOG, NULL);

	dir = g_dir_open(path, 0, &error);
	if (dir == NULL)
		return -1;

	rs_conf_get_boolean(CONF_LOAD_GDK, &load_8bit);

	/* Count loadable items */
	g_dir_rewind(dir);
	while ((name = (gchar *) g_dir_read_name(dir)))
	{
		filetype = rs_filetype_get(name, TRUE);
		if (filetype)
			if (filetype->load && ((filetype->filetype==FILETYPE_RAW)||load_8bit))
				items++;
	}

	/* unset model and make sure we have enough columns */
	for (n=0;n<NUM_VIEWS;n++)
	{
		gtk_icon_view_set_model(GTK_ICON_VIEW (store->iconview[n]), NULL);
		gtk_icon_view_set_columns(GTK_ICON_VIEW (store->iconview[n]), items);
	}

	/* Block the priority count */
	g_signal_handler_block(store->store, store->counthandler);
	rsp = gui_progress_new_with_delay(NULL, items, 200);

	/* Load all thumbnails */
	g_dir_rewind(dir);
	while ((name = (gchar *) g_dir_read_name(dir)))
	{
		filetype = rs_filetype_get(name, TRUE);
		if (filetype)
			if (filetype->load && ((filetype->filetype==FILETYPE_RAW)||load_8bit))
			{
				/* Generate full path to image */
				GString *fullname;
				fullname = g_string_new(path);
				fullname = g_string_append(fullname, G_DIR_SEPARATOR_S);
				fullname = g_string_append(fullname, name);

				pixbuf = NULL;
				if (filetype->thumb)
					pixbuf = filetype->thumb(fullname->str);
				if (pixbuf==NULL)
				{
					pixbuf = missing_thumb;
					g_object_ref (pixbuf);
				}

				/* Save a clean copy of the thumbnail for later use */
				pixbuf_clean = gdk_pixbuf_copy(pixbuf);

				/* Sane defaults */
				priority = PRIO_U;
				exported = FALSE;

				/* Load flags from XML cache */
				rs_cache_load_quick(fullname->str, &priority, &exported);

				/* Update thumbnail */
				thumbnail_update(pixbuf, pixbuf_clean, priority, exported);

				/* Add thumbnail to store */
				gtk_list_store_prepend (store->store, &iter);
				gtk_list_store_set (store->store, &iter,
					PIXBUF_COLUMN, pixbuf,
					PIXBUF_CLEAN_COLUMN, pixbuf_clean,
					TEXT_COLUMN, name,
					FULLNAME_COLUMN, fullname->str,
					PRIORITY_COLUMN, priority,
					EXPORTED_COLUMN, exported,
					-1);

				/* We can safely unref pixbuf by now, store holds a reference */
				g_object_unref (pixbuf);

				/* Don't free the actual data, they're needed in the store */
				g_string_free(fullname, FALSE);

				/* Move our progress bar */
				gui_progress_advance_one(rsp);
			}
	}

	g_dir_close(dir);

	/* Sort the store */
	sortable = GTK_TREE_SORTABLE(store->store);
	gtk_tree_sortable_set_sort_func(sortable,
		TEXT_COLUMN,
		model_sort_name,
		NULL,
		NULL);
	gtk_tree_sortable_set_sort_column_id(sortable, TEXT_COLUMN, GTK_SORT_ASCENDING);

	/* unblock the priority count */
	g_signal_handler_unblock(store->store, store->counthandler);

	/* count'em by sending a "row-changed"-signal */
	treepath = gtk_tree_path_new_first();
	if (gtk_tree_model_get_iter(GTK_TREE_MODEL(store->store), &iter, treepath))
		g_signal_emit_by_name(store->store, "row-changed", treepath, &iter);
	gtk_tree_path_free(treepath);

	/* set model for all 6 iconviews */
	for(n=0;n<NUM_VIEWS;n++)
	{
		GtkTreeModel *tree;

		tree = gtk_tree_model_filter_new(GTK_TREE_MODEL (store->store), NULL);
		gtk_tree_model_filter_set_visible_func(GTK_TREE_MODEL_FILTER (tree),
			model_filter_prio, GINT_TO_POINTER (priorities[n]), NULL);
		gtk_icon_view_set_model (GTK_ICON_VIEW (store->iconview[n]), tree);
	}

	/* Free the progress bar */
	gui_progress_free(rsp);

	/* Return the number of files successfully recognized */
	return items;
}

/**
 * Set priority and exported flags of a thumbnail
 * @param store A RSStore
 * @param filename The name of the thumbnail to remove or NULL
 * @param iter The iter of the thumbnail to remove or NULL
 * @param priority The priority or NULL to leave unchanged
 * @param exported The exported status or NULL to leave unchanged
 * @return TRUE if succeeded
 */
gboolean
rs_store_set_flags(RSStore *store, const gchar *filename, GtkTreeIter *iter,
	const guint *priority, const gboolean *exported)
{
	GtkTreeIter i;

	if (!store)
	{
		gint i;
		gboolean ret = FALSE;
		for (i=0;i<g_list_length(all_stores);i++)
		{
			if(rs_store_set_flags(g_list_nth_data(all_stores, i), filename, iter, priority, exported))
				ret = TRUE;
		}
		return ret;
	}

	g_return_val_if_fail(RS_IS_STORE(store), FALSE);
	g_return_val_if_fail((filename || iter), FALSE);

	/* If we got filename, but no iter, try to find correct iter */
	if (filename && (!iter))
		if (tree_find_filename(GTK_TREE_MODEL(store->store), filename, &i, NULL))
			iter = &i;

	if (iter)
	{
		guint prio;
		gboolean expo;
		GdkPixbuf *pixbuf;
		GdkPixbuf *pixbuf_clean;
		gchar *fullname;

		gtk_tree_model_get(GTK_TREE_MODEL(store->store), iter,
			PIXBUF_COLUMN, &pixbuf,
			PIXBUF_CLEAN_COLUMN, &pixbuf_clean,
			PRIORITY_COLUMN, &prio,
			EXPORTED_COLUMN, &expo,
			FULLNAME_COLUMN, &fullname,
			-1);

		if (priority)
			prio = *priority;
		if (exported)
			expo = *exported;

		thumbnail_update(pixbuf, pixbuf_clean, prio, expo);

		gtk_list_store_set (store->store, iter,
				PRIORITY_COLUMN, prio,
				EXPORTED_COLUMN, expo, -1);

		/* Update the cache */
		if (priority)
			rs_cache_save_flags(fullname, priority, exported);
		return TRUE;
	}

	return FALSE;
}

/**
 * Select a image
 * @param store A RSStore
 * @param name The filename to select
 */
gboolean
rs_store_set_selected_name(RSStore *store, const gchar *filename)
{
	gboolean ret = FALSE;
	GtkTreePath *path = NULL;

	g_return_val_if_fail(RS_IS_STORE(store), FALSE);
	g_return_val_if_fail(filename, FALSE);

	tree_find_filename(GTK_TREE_MODEL(store->store), filename, NULL, &path);

	if (path)
	{
		/* Get model for current icon-view */
		GtkTreeModel *model = gtk_icon_view_get_model (GTK_ICON_VIEW(store->current_iconview));

		/* Get the path in iconview and free path */
		GtkTreePath *iconpath = gtk_tree_model_filter_convert_child_path_to_path(GTK_TREE_MODEL_FILTER(model), path);
		gtk_tree_path_free(path);

		/* Select the icon */
		gtk_icon_view_select_path(GTK_ICON_VIEW(store->current_iconview), iconpath);

		/* Free the iconview path */
		gtk_tree_path_free(iconpath);
		ret = TRUE;
	}
	return ret;
}

/**
 * Get a list of currently selected thumbnail iters
 * @param store A RSStore
 * @return a GList or NULL
 */
GList *
rs_store_get_selected_iters(RSStore *store)
{
	GList *selected = NULL;

	g_return_val_if_fail(RS_IS_STORE(store), NULL);

	/* Get list of selected icons */
	gtk_icon_view_selected_foreach(GTK_ICON_VIEW(store->current_iconview),
		icon_get_selected_iters, &selected);

	return selected;
}

/**
 * Get a list of currently selected thumbnail names
 * @param store A RSStore
 * @return a GList or NULL
 */
GList *
rs_store_get_selected_names(RSStore *store)
{
	GList *selected = NULL;

	g_return_val_if_fail(RS_IS_STORE(store), NULL);

	/* Get list of selected icons */
	gtk_icon_view_selected_foreach(GTK_ICON_VIEW(store->current_iconview),
		icon_get_selected_names, &selected);

	return selected;
}

/**
 * Get a list of photo names
 * @param store A RSStore
 * @param selected GList for selected thumbs or NULL
 * @param visible GList for visible (in currently selected iconview) thumbs or NULL
 * @param all GList for all photos or NULL
 */
void
rs_store_get_names(RSStore *store, GList **selected, GList **visible, GList **all)
{
	GtkTreeModel *model;

	g_assert(RS_IS_STORE(store));

	model = gtk_icon_view_get_model (GTK_ICON_VIEW(store->current_iconview));

	if (selected)
		gtk_icon_view_selected_foreach(GTK_ICON_VIEW(store->current_iconview),
			icon_get_selected_names, selected);

	if (visible)
		gtk_tree_model_foreach(model, tree_foreach_names, visible);

	if (all)
		gtk_tree_model_foreach(GTK_TREE_MODEL(store->store), tree_foreach_names, all);

	return;
}

/**
 * Show filenames in the thumbnail browser
 * @param store A RSStore
 * @param show_filenames If TRUE filenames will be visible
 */
void
rs_store_set_show_filenames(RSStore *store, gboolean show_filenames)
{
	gint i;

	g_assert(RS_IS_STORE(store));

	for (i=0;i<NUM_VIEWS;i++)
	{
		if (show_filenames)
			gtk_icon_view_set_text_column (GTK_ICON_VIEW (store->iconview[i]), TEXT_COLUMN);
		else
			gtk_icon_view_set_text_column (GTK_ICON_VIEW (store->iconview[i]), -1);
	}

	return;
}

/**
 * Return a GList of iters with a specific priority
 * @param store A RSStore
 * @param priority The priority of interest
 * @return A GList of GtkTreeIters
 */
GList *
rs_store_get_iters_with_priority(RSStore *store, guint priority)
{
	GtkTreePath *path;
	guint prio;
	GList *list = NULL;
	GtkTreeIter iter;

	g_assert(RS_IS_STORE(store));

	path = gtk_tree_path_new_first();
	while(gtk_tree_model_get_iter(GTK_TREE_MODEL(store->store), &iter, path))
	{
		prio = PRIO_U;
		gtk_tree_model_get(GTK_TREE_MODEL(store->store), &iter, PRIORITY_COLUMN, &prio, -1);
		if (prio == priority)
		{
			GtkTreeIter *i = g_new(GtkTreeIter, 1);
			*i = iter;
			list = g_list_prepend(list, i);
		}
		gtk_tree_path_next(path);
	}

	return list;
}

/**
 * Get the filename of an image
 * @param store A RSStore
 * @param iter The iter of the thumbnail
 * @return a filename or NULL if failed
 */
gchar *
rs_store_get_name(RSStore *store, GtkTreeIter *iter)
{
	GtkTreeModel *model;
	gchar *fullname = NULL;

	g_assert(RS_IS_STORE(store));
	g_assert(iter != NULL);

	model = gtk_icon_view_get_model (GTK_ICON_VIEW(store->current_iconview));

	gtk_tree_model_get(GTK_TREE_MODEL(store->store), iter, FULLNAME_COLUMN, &fullname, -1);

	return(fullname);
}

/**
 * Selects the previous or next thumbnail
 * @param store A RSStore
 * @param direction 1: previous, 2: next
 */
gboolean
rs_store_select_prevnext(RSStore *store, guint direction)
{
	gboolean ret = FALSE;
	GList *selected;
	GtkIconView *iconview;
	GtkTreeIter iter;
	GtkTreePath *path, *newpath = NULL;

	g_assert(RS_IS_STORE(store));

	/* get the iconview */
	iconview = GTK_ICON_VIEW(store->current_iconview);

	/* Get a list of selected icons */
	selected = gtk_icon_view_get_selected_items(iconview);
	if (g_list_length(selected) == 1)
	{
		path = g_list_nth_data(selected, 0);
		newpath = gtk_tree_path_copy(path);
		if (direction == 1) /* Previous */
		{
			if (gtk_tree_path_prev(newpath))
			{
				gtk_icon_view_unselect_path(iconview, path);
				gtk_icon_view_select_path(iconview, newpath);
				ret = TRUE;
			}
		}
		else /* Next */
		{
			gtk_tree_path_next(newpath);
			if (gtk_tree_model_get_iter(gtk_icon_view_get_model (iconview), &iter, newpath))
			{
				gtk_icon_view_unselect_path(iconview, path);
				gtk_icon_view_select_path(iconview, newpath);
				ret = TRUE;
			}
		}
	}
	else if (g_list_length(selected) == 0)
	{
		/* If nothing is selected, select first thumbnail */
		newpath = gtk_tree_path_new_first();
		if (gtk_tree_model_get_iter(gtk_icon_view_get_model (iconview), &iter, newpath))
		{
			gtk_icon_view_select_path(iconview, newpath);
			ret = TRUE;
		}
	}

	/* Free everything */
	if (newpath)
		gtk_tree_path_free(newpath);
	g_list_foreach (selected, (GFunc) gtk_tree_path_free, NULL);
	g_list_free (selected);

	return ret;
}
