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
#include <glib/gprintf.h>
#include <config.h>
#include <libxml/encoding.h>
#include <libxml/xmlwriter.h>
#include "rawstudio.h"
#include "conf_interface.h"
#include "gettext.h"
#include "rs-store.h"
#include "gtk-helper.h"
#include "gtk-progress.h"
#include "rs-cache.h"
#include "rs-pixbuf.h"
#include "eog-pixbuf-cell-renderer.h"
#include "rs-preload.h"
#include "rs-photo.h"

/* How many different icon views do we have (tabs) */
#define NUM_VIEWS 6

#define GROUP_XML_FILE "groups.xml"

/* Overlay icons */
static GdkPixbuf *icon_priority_1 = NULL;
static GdkPixbuf *icon_priority_2 = NULL;
static GdkPixbuf *icon_priority_3 = NULL;
static GdkPixbuf *icon_priority_D = NULL;
static GdkPixbuf *icon_exported = NULL;

enum {
	PIXBUF_COLUMN, /* The displayed pixbuf */
	PIXBUF_CLEAN_COLUMN, /* The clean thumbnail */
	TEXT_COLUMN, /* Icon text */
	FULLNAME_COLUMN, /* Full path to image */
	PRIORITY_COLUMN,
	EXPORTED_COLUMN,
	TYPE_COLUMN,
	GROUP_LIST_COLUMN,
	NUM_COLUMNS
};

enum {
	RS_STORE_TYPE_FILE,
	RS_STORE_TYPE_GROUP,
	RS_STORE_TYPE_GROUP_MEMBER
};

struct _RSStore
{
	GtkHBox parent;
	GtkNotebook *notebook;
	GtkWidget *iconview[NUM_VIEWS];
	GtkWidget *current_iconview;
	guint current_priority;
	GtkListStore *store;
	gulong counthandler;
	gchar *last_path;
	gboolean cancelled;
};

/* Define the boiler plate stuff using the predefined macro */
G_DEFINE_TYPE (RSStore, rs_store, GTK_TYPE_HBOX);

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
static void cancel_clicked(GtkButton *button, gpointer user_data);
GList *find_loadable(const gchar *path, gboolean load_8bit, gboolean load_recursive);
void load_loadable(RSStore *store, GList *loadable, RS_PROGRESS *rsp);
void store_group_select_n(GtkListStore *store, GtkTreeIter iter, guint n);
gboolean store_iter_is_group(GtkListStore *store, GtkTreeIter *iter);
void store_save_groups(GtkListStore *store);
void store_load_groups(GtkListStore *store);
void store_group_photos_by_iters(GtkListStore *store, GList *members);
void store_group_photos_by_filenames(GtkListStore *store, GList *members);

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
	if (!icon_priority_1)
	{
		icon_priority_1	= gdk_pixbuf_new_from_file(PACKAGE_DATA_DIR "/pixmaps/" PACKAGE "/overlay_priority1.png", NULL);
		icon_priority_2 = gdk_pixbuf_new_from_file(PACKAGE_DATA_DIR "/pixmaps/" PACKAGE "/overlay_priority2.png", NULL);
		icon_priority_3 = gdk_pixbuf_new_from_file(PACKAGE_DATA_DIR "/pixmaps/" PACKAGE "/overlay_priority3.png", NULL);
		icon_priority_D = gdk_pixbuf_new_from_file(PACKAGE_DATA_DIR "/pixmaps/" PACKAGE "/overlay_deleted.png", NULL);
		icon_exported = gdk_pixbuf_new_from_file(PACKAGE_DATA_DIR "/pixmaps/" PACKAGE "/overlay_exported.png", NULL);
	}
}

/**
 * Instance initialization
 */
static void
rs_store_init(RSStore *store)
{
	GtkHBox *hbox = GTK_HBOX(store);
	gint n;
	gchar label_text[NUM_VIEWS][63];
	GtkWidget **label = g_new(GtkWidget *, NUM_VIEWS);
	GtkWidget *label_tt[NUM_VIEWS];
	GtkCellRenderer *cell_renderer;
	gboolean show_filenames;
	GtkWidget *label_priorities;

	store->notebook = GTK_NOTEBOOK(gtk_notebook_new());
	store->store = gtk_list_store_new (NUM_COLUMNS,
		GDK_TYPE_PIXBUF,
		GDK_TYPE_PIXBUF,
		G_TYPE_STRING,
		G_TYPE_STRING,
		G_TYPE_INT,
		G_TYPE_BOOLEAN,
		G_TYPE_INT,
		G_TYPE_POINTER);

	for (n=0;n<NUM_VIEWS;n++)
	{
		GtkTreeModel *filter;

		/* New Icon view */
		store->iconview[n] = gtk_icon_view_new();

		/* New cell-renderer for thumbnails */
		cell_renderer = eog_pixbuf_cell_renderer_new();

		/* Use our own cell renderer */
		gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (store->iconview[n]),
			cell_renderer, FALSE);

		/* Set everything up nice */
		g_object_set (cell_renderer,
			"follow-state", TRUE,
			"height", 130,  
			"width", 130, 
			"yalign", 0.5,
			"xalign", 0.5,
			NULL);

		/* Set pixbuf column */
		gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (store->iconview[n]),
			cell_renderer,
			"pixbuf", PIXBUF_COLUMN,
			NULL);

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
		gtk_notebook_append_page(store->notebook, make_iconview(store->iconview[n], store, priorities[n]), label_tt[n]);
	}

	/* Load show filenames state from config */
	rs_conf_get_boolean_with_default(CONF_SHOW_FILENAMES, &show_filenames, FALSE);
	rs_store_set_show_filenames(store, show_filenames);

	/* Default to page 0 */
	store->current_iconview = store->iconview[0];
	store->current_priority = priorities[0];

	gtk_notebook_set_tab_pos(store->notebook, GTK_POS_LEFT);

	g_signal_connect(store->notebook, "switch-page", G_CALLBACK(switch_page), store);
	store->counthandler = g_signal_connect(store->store, "row-changed", G_CALLBACK(count_priorities), label);
	g_signal_connect(store->store, "row-deleted", G_CALLBACK(count_priorities_del), label);

	all_stores = g_list_append(all_stores, store);

	/* Due to popular demand, I will now add a very nice GTK+ label to the left
	   of the notebook. We hope this will give our users an even better
	   understanding of our interface. I was thinking about adding a button instead
	   that said "ROCK ON!" to instantly play "AC/DC - Highway to Hell", but I
	   believe this will be better for the end user */
	label_priorities = gtk_label_new(_("Priorities"));
	gtk_label_set_angle(GTK_LABEL(label_priorities), 90);
	gtk_box_pack_start(GTK_BOX (hbox), label_priorities, FALSE, FALSE, 0);

	gtk_box_pack_start(GTK_BOX (hbox), GTK_WIDGET(store->notebook), TRUE, TRUE, 0);

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
preload_iter(GtkTreeModel *model, GtkTreeIter *iter)
{
	gchar *filename;
	gtk_tree_model_get(model, iter, FULLNAME_COLUMN, &filename, -1);

	rs_preload_near_add(filename);
}

static void
predict_preload(RSStore *store, gboolean initial)
{
	GList *selected = NULL;
	gint n, near;
	GtkTreeIter iter;
	GtkIconView *iconview = GTK_ICON_VIEW(store->current_iconview);
	GtkTreePath *path, *next, *prev;
	GtkTreeModel *model = gtk_icon_view_get_model (iconview);

	near = rs_preload_get_near_count();
	if (near < 1) return;
	rs_preload_near_remove_all();

	/* Get a list of selected icons */
	selected = gtk_icon_view_get_selected_items(iconview);
	if (g_list_length(selected) == 1)
	{
		/* Preload current image - this is stupid thou! */
		path = g_list_nth_data(selected, 0);
		if (gtk_tree_model_get_iter(gtk_icon_view_get_model (iconview), &iter, path))
			preload_iter(model, &iter);

		/* Near */
		next = gtk_tree_path_copy(path);
		prev = gtk_tree_path_copy(path);
		for(n=0;n<near;n++)
		{
			/* Travel forward */
			gtk_tree_path_next(next);
			if (gtk_tree_model_get_iter(gtk_icon_view_get_model (iconview), &iter, next))
				preload_iter(model, &iter);
			/* Travel backward */
			if (gtk_tree_path_prev(prev))
				if (gtk_tree_model_get_iter(gtk_icon_view_get_model (iconview), &iter, prev))
					preload_iter(model, &iter);
		}
		gtk_tree_path_free(next);
		gtk_tree_path_free(prev);
	}
	else if (((g_list_length(selected) == 0)) && initial)
	{
		path = gtk_tree_path_new_first();
		if (gtk_tree_model_get_iter(gtk_icon_view_get_model (iconview), &iter, path))
			preload_iter(model, &iter);

		/* Next */
		for(n=0;n<near;n++)
		{
			gtk_tree_path_next(path);
			if (gtk_tree_model_get_iter(gtk_icon_view_get_model (iconview), &iter, path))
				preload_iter(model, &iter);
		}
		gtk_tree_path_free(path);
	}

	/* Free the list */
	if (g_list_length(selected) > 0)
	{
		g_list_foreach (selected, (GFunc) gtk_tree_path_free, NULL);
		g_list_free (selected);
	}
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

	predict_preload(data, FALSE);
}

static GtkWidget *
make_iconview(GtkWidget *iconview, RSStore *store, gint prio)
{
	GtkWidget *scroller;

	/* We must be abletoselect multiple icons */
	gtk_icon_view_set_selection_mode(GTK_ICON_VIEW (iconview), GTK_SELECTION_MULTIPLE);

#if GTK_CHECK_VERSION(2,12,0)
	/* Enable tooltips */
	gtk_icon_view_set_tooltip_column(GTK_ICON_VIEW (iconview), TEXT_COLUMN);
#endif

	/* pack them as close af possible */
	gtk_icon_view_set_column_spacing(GTK_ICON_VIEW (iconview), 0);

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
	gint p,t;
	gint prio = GPOINTER_TO_INT (data);
	gtk_tree_model_get (model, iter, PRIORITY_COLUMN, &p, -1);
	gtk_tree_model_get (model, iter, TYPE_COLUMN, &t, -1);

	if (t == RS_STORE_TYPE_GROUP_MEMBER)
		return FALSE;
	else 
	{
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
	gint priority, type;
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
			gtk_tree_model_get(treemodel, &iter, TYPE_COLUMN, &type, -1);
			if (type != RS_STORE_TYPE_GROUP_MEMBER)
			{
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


static void
cancel_clicked(GtkButton *button, gpointer user_data)
{
	RSStore *store = RS_STORE(user_data);
	store->cancelled = TRUE;
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
 * @param path The path to load
 * @param load_8bit Boolean
 * @param load_recursive Boolean
 * @return GList containing paths to loadable files
 */
GList *
find_loadable(const gchar *path, gboolean load_8bit, gboolean load_recursive)
{
	gchar *name;
	RS_FILETYPE *filetype;
	GString *fullname;
	GList *loadable = NULL;
	GDir *dir = g_dir_open(path, 0, NULL);

	if (dir == NULL)
		return loadable;

	while ((name = (gchar *) g_dir_read_name(dir)))
	{
		fullname = g_string_new(path);
		fullname = g_string_append(fullname, G_DIR_SEPARATOR_S);
		fullname = g_string_append(fullname, name);
		filetype = rs_filetype_get(name, TRUE);
		if (filetype)
		{
			if (filetype->load && ((filetype->filetype==FILETYPE_RAW)||load_8bit))
			{
				loadable = g_list_append(loadable, fullname->str);
			}
		}
		if (load_recursive)
		{
			if (g_file_test(fullname->str, G_FILE_TEST_IS_DIR))
			{
				/* We don't load hidden directories */
				if (name[0] != '.')
				{
					GList *temp_loadable;
					temp_loadable = find_loadable(fullname->str, load_8bit, load_recursive);
					loadable = g_list_concat(loadable, temp_loadable);
				}
			}
		}
		g_string_free(fullname, FALSE);
	}

	if (dir)
		g_dir_close(dir);

	return loadable;
}

/**
 * Load thumbnails from a directory into the store
 * @param store A RSStore
 * @param loadable A GList containing paths to loadable files
 * @param rsp A progress bar
 */
void
load_loadable(RSStore *store, GList *loadable, RS_PROGRESS *rsp)
{
	gchar *name;
	RS_FILETYPE *filetype;
	GdkPixbuf *pixbuf;
	GdkPixbuf *pixbuf_clean;
	GdkPixbuf *missing_thumb;
	gint priority;
	gboolean exported = FALSE;
	GtkTreeIter iter;
	gchar *fullname;
	gint n;

	/* We will use this, if no thumbnail can be loaded */
	missing_thumb = gtk_widget_render_icon(GTK_WIDGET(store),
		GTK_STOCK_MISSING_IMAGE, GTK_ICON_SIZE_DIALOG, NULL);

	for(n = 0; n < g_list_length(loadable); n++)
	{
		if (store->cancelled)
			break;
		fullname = g_list_nth_data(loadable, n);
		name = g_path_get_basename(fullname);

		filetype = rs_filetype_get(name, TRUE);
		if (filetype)
		{
			if (filetype->load && ((filetype->filetype==FILETYPE_RAW)))
			{
				pixbuf = NULL;
				if (filetype->thumb)
					pixbuf = filetype->thumb(fullname);
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
				rs_cache_load_quick(fullname, &priority, &exported);

				/* Update thumbnail */
				thumbnail_update(pixbuf, pixbuf_clean, priority, exported);

				/* Add thumbnail to store */
				gtk_list_store_prepend (store->store, &iter);
				gtk_list_store_set (store->store, &iter,
					PIXBUF_COLUMN, pixbuf,
					PIXBUF_CLEAN_COLUMN, pixbuf_clean,
					TEXT_COLUMN, name,
					FULLNAME_COLUMN, fullname,
					PRIORITY_COLUMN, priority,
					EXPORTED_COLUMN, exported,
					-1);

				/* We can safely unref pixbuf by now, store holds a reference */
				g_object_unref (pixbuf);

				/* Move our progress bar */
				gui_progress_advance_one(rsp);
			}
		}
		g_free(name);
	}

	return;
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
	static gboolean running = FALSE;
	GStaticMutex lock = G_STATIC_MUTEX_INIT;

	GtkTreeSortable *sortable;
	RS_PROGRESS *rsp;
	GtkWidget *cancel;
	gboolean load_8bit = FALSE;
	gboolean load_recursive = DEFAULT_CONF_LOAD_RECURSIVE;
	gint items=0, n;
	GtkTreePath *treepath;
	GtkTreeIter iter;
	GList *loadable = NULL;

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

	/* We should really only be running one instance at a time */
	g_static_mutex_lock(&lock);
	if (running)
		return -1;
	running = TRUE;
	g_static_mutex_unlock(&lock);

	rs_conf_get_boolean(CONF_LOAD_GDK, &load_8bit);
	rs_conf_get_boolean(CONF_LOAD_RECURSIVE, &load_recursive);

	/* find loadable files */
	loadable = find_loadable(path, load_8bit, load_recursive);
	loadable = g_list_first(loadable);
	items = g_list_length(loadable);

	/* unset model and make sure we have enough columns */
	for (n=0;n<NUM_VIEWS;n++)
	{
		gtk_icon_view_set_model(GTK_ICON_VIEW (store->iconview[n]), NULL);
		gtk_icon_view_set_columns(GTK_ICON_VIEW (store->iconview[n]), items);
	}

	/* Block the priority count */
	g_signal_handler_block(store->store, store->counthandler);

	/* Add a progress bar */
	store->cancelled = FALSE;
	cancel = gtk_button_new_from_stock(GTK_STOCK_CANCEL);
	g_signal_connect (G_OBJECT(cancel), "clicked", G_CALLBACK(cancel_clicked), store);
	rsp = gui_progress_new_with_delay(_("Opening directory..."), items, 200);
	gui_progress_add_widget(rsp, cancel);

	/* load all loadable items */
	load_loadable(store, loadable, rsp);
	g_list_free(loadable);

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

#ifdef EXPERIMENTAL
	/* load group file and group photos */
	store_load_groups(store->store);
#endif

	/* Free the progress bar */
	gui_progress_free(rsp);

	/* Start the preloader */
	predict_preload(store, TRUE);

	g_static_mutex_lock(&lock);
	running = FALSE;
	g_static_mutex_unlock(&lock);
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

	selected = rs_store_sort_selected(selected);

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
		{
			gtk_icon_view_set_text_column (GTK_ICON_VIEW (store->iconview[i]), TEXT_COLUMN);
			gtk_widget_set_size_request (store->iconview[i], -1, 160);
		}
		else
		{
			gtk_icon_view_set_text_column (GTK_ICON_VIEW (store->iconview[i]), -1);
			gtk_widget_set_size_request (store->iconview[i], -1, 130);
		}
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
 * @param current_filename Current filename or NULL if none
 * @param direction 1: previous, 2: next
 */
gboolean
rs_store_select_prevnext(RSStore *store, const gchar *current_filename, guint direction)
{
	gboolean ret = FALSE;
	GList *selected;
	GtkIconView *iconview;
	GtkTreeIter iter;
	GtkTreePath *path = NULL, *newpath = NULL;

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
				ret = TRUE;
			}
		}
		else /* Next */
		{
			gtk_tree_path_next(newpath);
			if (gtk_tree_model_get_iter(gtk_icon_view_get_model (iconview), &iter, newpath))
			{
				gtk_icon_view_unselect_path(iconview, path);
				ret = TRUE;
			}
		}
	}
	else if (g_list_length(selected) == 0)
	{
		/* Get current GtkTreeModelFilter */
		GtkTreeModel *model = gtk_icon_view_get_model (GTK_ICON_VIEW(store->current_iconview));

		/* If we got a filename, try to select prev/next from that */
		if (current_filename)
		{
			if (tree_find_filename(GTK_TREE_MODEL(store->store), current_filename, NULL, &newpath))
			{
				while (!(path = gtk_tree_model_filter_convert_child_path_to_path(GTK_TREE_MODEL_FILTER(model), newpath)))
				{
					ret = FALSE;
					if (direction == 1) /* Previous */
					{
						if (!gtk_tree_path_prev(newpath))
							break;
					}
					else
					{
						gtk_tree_path_next(newpath);
						if (!gtk_tree_model_get_iter(GTK_TREE_MODEL(store->store), &iter, newpath))
							break;
					}
					ret = TRUE;
				}
				if (newpath)
					gtk_tree_path_free(newpath);
				newpath = path;
			}
		}

		/* If we got no hit, fall back to this */
		if (ret == FALSE)
		{
			/* If nothing is selected, select first thumbnail */
			newpath = gtk_tree_path_new_first();
			if (gtk_tree_model_get_iter(gtk_icon_view_get_model (iconview), &iter, newpath))
				ret = TRUE;
		}
	}

	if (newpath && ret)
	{
#if GTK_CHECK_VERSION(2,8,0)
		/* Scroll to the new path */
		gtk_icon_view_scroll_to_path(iconview, newpath, FALSE, 0.5f, 0.5f);
#endif
		gtk_icon_view_select_path(iconview, newpath);
		/* Free the new path */
		gtk_tree_path_free(newpath);
	}

	/* Free list of selected */
	g_list_foreach (selected, (GFunc) gtk_tree_path_free, NULL);
	g_list_free (selected);

	return ret;
}

/**
 * Switches to the page number page_num
 * @note Should behave like gtk_notebook_set_current_page()
 * @param store A RSStore
 * @param page_num index of the page to switch to, starting from 0. If negative,
          the last page will be used. If greater than the number of pages in the notebook,
          nothing will be done.
 */
void
rs_store_set_current_page(RSStore *store, gint page_num)
{
	gtk_notebook_set_current_page(store->notebook, page_num);
}

/**
 * Returns the page number of the current page.
 * @note Should behave like gtk_notebook_get_current_page()
 * @param store A RSStore
 * @return the index (starting from 0) of the current page in the notebook. If the notebook
           has no pages, then -1 will be returned.
 */
gint
rs_store_get_current_page(RSStore *store)
{
	return gtk_notebook_get_current_page(store->notebook);
}

GdkPixbuf *
store_group_update_pixbufs(GdkPixbuf *pixbuf, GdkPixbuf *pixbuf_clean)
{
	gint width, height, new_width, new_height;
	guint rowstride;
	guchar *pixels;
	gint channels;
	GdkPixbuf *new_pixbuf, *pixbuf_scaled;

	width = gdk_pixbuf_get_width(pixbuf_clean);
	height = gdk_pixbuf_get_height(pixbuf_clean);
	
	new_pixbuf = gdk_pixbuf_new(gdk_pixbuf_get_colorspace(pixbuf_clean),
								TRUE,
								gdk_pixbuf_get_bits_per_sample(pixbuf_clean),
								width,
								height);

	width -= 6;
	height -= 6;
	
	rowstride = gdk_pixbuf_get_rowstride (new_pixbuf);
	pixels = gdk_pixbuf_get_pixels (new_pixbuf);
	new_width = gdk_pixbuf_get_width (new_pixbuf);
	new_height = gdk_pixbuf_get_height (new_pixbuf);
	channels = gdk_pixbuf_get_n_channels (new_pixbuf);

	// draw horizontal lines
	rs_pixbuf_draw_hline(new_pixbuf, 0, 0, width+2, 0x00, 0x00, 0x00, 0xFF);
	rs_pixbuf_draw_hline(new_pixbuf, width+2, 2, 2, 0x00, 0x00, 0x00, 0xFF);
	rs_pixbuf_draw_hline(new_pixbuf, width+4, 4, 2, 0x00, 0x00, 0x00, 0xFF);
	rs_pixbuf_draw_hline(new_pixbuf, 0, height+1, width+2, 0x00, 0x00, 0x00, 0xFF);
	rs_pixbuf_draw_hline(new_pixbuf, 2, height+3, width+2, 0x00, 0x00, 0x00, 0xFF);
	rs_pixbuf_draw_hline(new_pixbuf, 4, height+5, width+2, 0x00, 0x00, 0x00, 0xFF);
	// draw vertical lines
	rs_pixbuf_draw_vline(new_pixbuf, 0, 0, height+2, 0x00, 0x00, 0x00, 0xFF);
	rs_pixbuf_draw_vline(new_pixbuf, 2, height+2, 2, 0x00, 0x00, 0x00, 0xFF);
	rs_pixbuf_draw_vline(new_pixbuf, 4, height+4, 2, 0x00, 0x00, 0x00, 0xFF);
	rs_pixbuf_draw_vline(new_pixbuf, width+1, 0, height+2, 0x00, 0x00, 0x00, 0xFF);
	rs_pixbuf_draw_vline(new_pixbuf, width+3, 2, height+2, 0x00, 0x00, 0x00, 0xFF);
	rs_pixbuf_draw_vline(new_pixbuf, width+5, 4, height+2, 0x00, 0x00, 0x00, 0xFF);
	// fill spots with white
	rs_pixbuf_draw_hline(new_pixbuf, 3, height+2, width, 0xFF, 0xFF, 0xFF, 0xFF);
	rs_pixbuf_draw_hline(new_pixbuf, 5, height+4, width, 0xFF, 0xFF, 0xFF, 0xFF);
	rs_pixbuf_draw_vline(new_pixbuf, width+2, 3, height, 0xFF, 0xFF, 0xFF, 0xFF);
	rs_pixbuf_draw_vline(new_pixbuf, width+4, 5, height, 0xFF, 0xFF, 0xFF, 0xFF);

	pixbuf_scaled = gdk_pixbuf_scale_simple(pixbuf_clean,
							width,
							height,
							GDK_INTERP_BILINEAR);
	gdk_pixbuf_copy_area(pixbuf_scaled,
						 0, 0, width, height,
						 new_pixbuf, 1, 1);
	return new_pixbuf;
}

void
store_group_select_n(GtkListStore *store, GtkTreeIter iter, guint n)
{
	GList *members;
	GtkTreeIter *child_iter;
	GdkPixbuf *pixbuf = NULL;
	GdkPixbuf *pixbuf_clean = NULL;
	gchar *fullname = NULL;
	gchar *name = NULL;
	guint priority;
	gboolean exported;

	gtk_tree_model_get(GTK_TREE_MODEL(store), &iter,
					   GROUP_LIST_COLUMN, &members,
					   -1);
	child_iter = (GtkTreeIter *) g_list_nth_data(members, n);

	gtk_tree_model_get(GTK_TREE_MODEL(store), child_iter,
					   PIXBUF_COLUMN, &pixbuf,
					   PIXBUF_CLEAN_COLUMN, &pixbuf_clean,
					   TEXT_COLUMN, &name,
					   FULLNAME_COLUMN, &fullname,
					   PRIORITY_COLUMN, &priority,
					   EXPORTED_COLUMN, &exported,
					   -1);

	pixbuf_clean = store_group_update_pixbufs(pixbuf, pixbuf_clean);
	pixbuf = gdk_pixbuf_copy(pixbuf_clean);
	
	thumbnail_update(pixbuf, pixbuf_clean, priority, exported);

	gtk_list_store_set (store, &iter,
					PIXBUF_COLUMN, pixbuf,
					PIXBUF_CLEAN_COLUMN, pixbuf_clean,
					TEXT_COLUMN, name,
					FULLNAME_COLUMN, fullname,
					PRIORITY_COLUMN, priority,
					EXPORTED_COLUMN, exported,
					-1);

	store_save_groups(store);
	return;
}

/**
 * Marks a selection of thumbnails as a group
 * @param store A RSStore
 */
void
rs_store_group_photos(RSStore *store)
{
	GList *selected = NULL;
	gint selected_groups;

	selected = rs_store_get_selected_iters(store);
	selected_groups = rs_store_selection_n_groups(store, selected);
	gtk_icon_view_unselect_all(GTK_ICON_VIEW(store->current_iconview)); // Or automatic load of photo == wait time

	if (selected_groups == 0)
	{
		store_group_photos_by_iters(store->store, selected);
	}
	else
	{
		GtkTreeIter *group = NULL, *s = NULL;
		GList *members = NULL, *newmembers = NULL;

 		while (selected)
		{
			s = selected->data;
			if (store_iter_is_group(store->store, s))
			{
				gtk_tree_model_get(GTK_TREE_MODEL(store->store), s,
								   GROUP_LIST_COLUMN, &members,
								   -1);

				newmembers = g_list_concat(newmembers, members);

				if (group)
				{
					gtk_list_store_remove(store->store, s);
					selected = g_list_remove(selected, s);
					selected = g_list_previous(selected);
				}
				else
					group = s;
			}
			else
			{
				newmembers = g_list_append(newmembers, s);
				gtk_list_store_set(store->store, s,
								   TYPE_COLUMN, RS_STORE_TYPE_GROUP_MEMBER,
								   -1);
			}
			selected = g_list_next(selected);
		}

		gtk_list_store_set(store->store, group,
						   GROUP_LIST_COLUMN, newmembers,
						   -1);
	}
	store_save_groups(store->store);
}

/**
 * Ungroup a group or selection of groups
 * @param store A RSStore
 */
void
rs_store_ungroup_photos(RSStore *store)
{
	GList *members;
	GtkTreeIter *child_iter;
	GList *selected;
	gint n,m;
	GtkTreeIter *iter;

	selected = rs_store_get_selected_iters(store);

	for( n = 0; n < g_list_length(selected); n++)
	{
		if (store_iter_is_group(store->store, g_list_nth_data(selected, n)))
		{
			iter = (GtkTreeIter *) g_list_nth_data(selected, n);
			gtk_tree_model_get(GTK_TREE_MODEL(store->store), iter,
							   GROUP_LIST_COLUMN, &members,
							   -1);
			for( m = 0; m < g_list_length(members); m++)
			{
				child_iter = (GtkTreeIter *) g_list_nth_data(members, m);
				gtk_list_store_set(store->store, child_iter,
								   	TYPE_COLUMN, RS_STORE_TYPE_FILE,
								   -1);
			}
			gtk_list_store_remove(store->store, iter);
		}
	}
	store_save_groups(store->store);
}

gboolean
store_iter_is_group(GtkListStore *store, GtkTreeIter *iter)
{
	guint t;

	gtk_tree_model_get (GTK_TREE_MODEL(store), iter, TYPE_COLUMN, &t, -1);
	if (t == RS_STORE_TYPE_GROUP)
		return TRUE;
	else
		return FALSE;
}

gint
rs_store_selection_n_groups(RSStore *store, GList *selected)
{
	gint n, group = 0;

	for(n = 0; n < g_list_length(selected); n++)
	{
		if (store_iter_is_group(store->store, g_list_nth_data(selected, n)))
			group++;
	}
	return group;
}

void
store_save_groups(GtkListStore *store) {
	gchar *dotdir = NULL, *filename = NULL;
	GtkTreeIter iter;
	xmlTextWriterPtr writer;

	gtk_tree_model_get_iter_first(GTK_TREE_MODEL(store), &iter);
	gtk_tree_model_get (GTK_TREE_MODEL(store), &iter, FULLNAME_COLUMN, &filename, -1);

	dotdir = rs_dotdir_get(filename);
	GString *gs = g_string_new(dotdir);
	g_string_append(gs, G_DIR_SEPARATOR_S);
	g_string_append(gs, GROUP_XML_FILE);
	gchar *xmlfile = gs->str;
	g_string_free(gs, FALSE);

	writer = xmlNewTextWriterFilename(xmlfile, 0);
	if (!writer)
		return;
	xmlTextWriterSetIndent(writer, 1);
	xmlTextWriterStartDocument(writer, NULL, "UTF-8", NULL);
	xmlTextWriterStartElement(writer, BAD_CAST "rawstudio-groups");

	if (gtk_tree_model_get_iter_first(GTK_TREE_MODEL(store), &iter))
		do
		{
			if (store_iter_is_group(store, &iter))
			{
				gchar *selected;
				GList *members;

				xmlTextWriterStartElement(writer, BAD_CAST "group");

				// Find selected member and place this first in XML
				gtk_tree_model_get(GTK_TREE_MODEL(store), &iter,
					   FULLNAME_COLUMN, &selected,
					   -1);
				xmlTextWriterWriteFormatElement(writer, BAD_CAST "member", "%s", selected);

				gtk_tree_model_get(GTK_TREE_MODEL(store), &iter,
					   GROUP_LIST_COLUMN, &members,
					   -1);
				gint m;
				for( m = 0; m < g_list_length(members); m++)
				{
					GtkTreeIter *child_iter = (GtkTreeIter *) g_list_nth_data(members, m);
					gtk_tree_model_get (GTK_TREE_MODEL(store), child_iter, FULLNAME_COLUMN, &filename, -1);
					if (!g_str_equal(selected, filename))
						xmlTextWriterWriteFormatElement(writer, BAD_CAST "member", "%s", filename);
				}
				xmlTextWriterEndElement(writer);
			}
		} while(gtk_tree_model_iter_next(GTK_TREE_MODEL(store), &iter));

	xmlTextWriterEndDocument(writer);
	xmlFreeTextWriter(writer);

	return;
}

void
store_load_groups(GtkListStore *store) {
	gchar *dotdir = NULL, *filename = NULL;
	xmlDocPtr doc;
	xmlNodePtr cur;
	xmlNodePtr group = NULL;
	GtkTreeIter iter;
	GList *members = NULL;

	g_assert(store != NULL);

	gtk_tree_model_get_iter_first(GTK_TREE_MODEL(store), &iter);
	gtk_tree_model_get (GTK_TREE_MODEL(store), &iter, FULLNAME_COLUMN, &filename, -1);
	dotdir = rs_dotdir_get(filename);

	GString *gs = g_string_new(dotdir);
	g_string_append(gs, G_DIR_SEPARATOR_S);
	g_string_append(gs, GROUP_XML_FILE);
	gchar *xmlfile = gs->str;
	g_string_free(gs, FALSE);
	
	if (!g_file_test(xmlfile, G_FILE_TEST_IS_REGULAR))
		return;

	doc = xmlParseFile(xmlfile);
	if (!doc)
		return;
	
	cur = xmlDocGetRootElement(doc);
	cur = cur->xmlChildrenNode;

	while(cur)
	{
		if ((!xmlStrcmp(cur->name, BAD_CAST "group")))
		{
			xmlChar *member = NULL;

			group = cur->xmlChildrenNode;
			while (group)
			{
				if ((!xmlStrcmp(group->name, BAD_CAST "member")))
				{
					member = xmlNodeListGetString(doc, group->xmlChildrenNode, 1);
					if (member)
					{
						filename = (char *) member;
						members = g_list_append(members, filename);
					}
					member = NULL;
				}	
			group = group->next;
			}
		store_group_photos_by_filenames(store, members);
		members = NULL;
		}
		cur = cur->next;
	}

	xmlFreeDoc(doc);
	return;
}

void
store_group_photos_by_iters(GtkListStore *store, GList *members)
{
	GtkTreeIter iter;
	gint n;

	gtk_list_store_append (store, &iter);
	gtk_list_store_set (store, &iter,
						PIXBUF_COLUMN, NULL,
						PIXBUF_CLEAN_COLUMN, NULL,
						TEXT_COLUMN, "",
						FULLNAME_COLUMN, NULL,
						PRIORITY_COLUMN, 0,
						EXPORTED_COLUMN, 0,
						TYPE_COLUMN, RS_STORE_TYPE_GROUP,
						GROUP_LIST_COLUMN, members,
						-1);
	store_group_select_n(store, iter, 0);
	
	for(n = 0; n < g_list_length(members); n++)
	{
		gtk_list_store_set(store, g_list_nth_data(members,n),
						   TYPE_COLUMN, RS_STORE_TYPE_GROUP_MEMBER,
						   -1);
	}
}

void
store_group_photos_by_filenames(GtkListStore *store, GList *members)
{
	GList *members_iter = NULL;
	GtkTreeIter iter;
	GtkTreeIter *iter_copy;
	while (members)
	{
		tree_find_filename(GTK_TREE_MODEL(store), (gchar *) members->data, &iter, NULL);
		iter_copy = gtk_tree_iter_copy(&iter);
		members_iter = g_list_append(members_iter, iter_copy);
		members = g_list_next(members);
	}
	store_group_photos_by_iters(store, members_iter);
}

GList
* rs_store_sort_selected(GList *selected)
{
	return g_list_sort(selected, (GCompareFunc) g_utf8_collate);
}
