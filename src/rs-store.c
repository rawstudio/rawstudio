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

#include <rawstudio.h>
#include <gtk/gtk.h>
#include <glib/gprintf.h>
#include <config.h>
#include <libxml/encoding.h>
#include <libxml/xmlwriter.h>
#include <glib.h>
#include <math.h>
#include <memory.h>
#include "application.h"
#include "conf_interface.h"
#include "gettext.h"
#include "rs-store.h"
#include "gtk-helper.h"
#include "gtk-progress.h"
#include "rs-cache.h"
#include "rs-pixbuf.h"
#include "eog-pixbuf-cell-renderer.h"
#include "rs-photo.h"
#include "rs-library.h"

#ifdef WIN32
#undef near
#endif

/* How many different icon views do we have (tabs) */
#define NUM_VIEWS 6

#define GROUP_XML_FILE "groups.xml"

/* Overlay icons */
static GdkPixbuf *icon_priority_1 = NULL;
static GdkPixbuf *icon_priority_2 = NULL;
static GdkPixbuf *icon_priority_3 = NULL;
static GdkPixbuf *icon_priority_D = NULL;
static GdkPixbuf *icon_exported = NULL;

static GdkPixbuf *icon_default = NULL;

enum {
	PIXBUF_COLUMN, /* The displayed pixbuf */
	PIXBUF_CLEAN_COLUMN, /* The clean thumbnail */
	TEXT_COLUMN, /* Icon text */
	FULLNAME_COLUMN, /* Full path to image */
	PRIORITY_COLUMN,
	EXPORTED_COLUMN,
	METADATA_COLUMN, /* RSMetadata for image */
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
	GtkWidget *label[NUM_VIEWS];
	GtkWidget *current_iconview;
	guint current_priority;
	GtkListStore *store;
	gulong counthandler;
	gchar *last_path;
	gboolean cancelled;
	RS_STORE_SORT_METHOD sort_method;
	GString *tooltip_text;
	GtkTreePath *tooltip_last_path;
	volatile gint jobs_to_do;
	gboolean counter_blocked;		/* Only access when thread has gdk lock */
};

/* Classes to user for io-system */ 
#define PRELOAD_CLASS (82764283)
#define METADATA_CLASS (542344)

/* Define the boiler plate stuff using the predefined macro */
G_DEFINE_TYPE (RSStore, rs_store, GTK_TYPE_HBOX);

enum {
	THUMB_ACTIVATED_SIGNAL,
	GROUP_ACTIVATED_SIGNAL,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

typedef struct _worker_job {
	RSStore *store;
	gchar *filename;
	GtkTreeIter iter;
	gchar *name;
	GtkTreeModel *model;
} WORKER_JOB;

/* FIXME: Remember to remove stores from this too! */
static GList *all_stores = NULL;

/* Priorities to show */
const static guint priorities[NUM_VIEWS] = {PRIO_ALL, PRIO_1, PRIO_2, PRIO_3, PRIO_U, PRIO_D};
#if NUM_VIEWS != 6
 #error This must be updated
#endif

static void thumbnail_overlay(GdkPixbuf *pixbuf, GdkPixbuf *lowerleft, GdkPixbuf *lowerright, GdkPixbuf *topleft, GdkPixbuf *topright);
static void thumbnail_update(GdkPixbuf *pixbuf, GdkPixbuf *pixbuf_clean, gint priority, gboolean exported);
static void switch_page(GtkNotebook *notebook, GtkNotebookPage *page, guint page_num, gpointer data);
static void selection_changed(GtkIconView *iconview, gpointer data);
static GtkWidget *make_iconview(GtkWidget *iconview, RSStore *store, gint prio);
static gboolean model_filter_prio(GtkTreeModel *model, GtkTreeIter *iter, gpointer data);
static gint model_sort_name(GtkTreeModel *model, GtkTreeIter *tia, GtkTreeIter *tib, gpointer userdata);
static gint model_sort_timestamp(GtkTreeModel *model, GtkTreeIter *tia, GtkTreeIter *tib, gpointer userdata);
static gint model_sort_iso(GtkTreeModel *model, GtkTreeIter *tia, GtkTreeIter *tib, gpointer userdata);
static gint model_sort_aperture(GtkTreeModel *model, GtkTreeIter *tia, GtkTreeIter *tib, gpointer userdata);
static gint model_sort_focallength(GtkTreeModel *model, GtkTreeIter *tia, GtkTreeIter *tib, gpointer userdata);
static gint model_sort_shutterspeed(GtkTreeModel *model, GtkTreeIter *tia, GtkTreeIter *tib, gpointer userdata);
static void count_priorities_del(GtkTreeModel *treemodel, GtkTreePath *path, gpointer data);
static void count_priorities(GtkTreeModel *treemodel, GtkTreePath *do_not_use1, GtkTreeIter *do_not_use2, gpointer data);
static void icon_get_selected_iters(GtkIconView *iconview, GtkTreePath *path, gpointer user_data);
static void icon_get_selected_names(GtkIconView *iconview, GtkTreePath *path, gpointer user_data);
static gboolean tree_foreach_names(GtkTreeModel *model, GtkTreePath *path, GtkTreeIter *iter, gpointer data);
static gboolean tree_find_filename(GtkTreeModel *store, const gchar *filename, GtkTreeIter *iter, GtkTreePath **path);
void cairo_draw_thumbnail(cairo_t *cr, GdkPixbuf *pixbuf, gint x, gint y, gint width, gint height, gdouble alphas);
GdkPixbuf * store_group_update_pixbufs(GdkPixbuf *pixbuf, GdkPixbuf *pixbuf_clean);
void store_group_select_n(GtkListStore *store, GtkTreeIter iter, guint n);
gboolean store_iter_is_group(GtkListStore *store, GtkTreeIter *iter);
void store_save_groups(GtkListStore *store);
void store_load_groups(GtkListStore *store);
void store_group_photos_by_iters(GtkListStore *store, GList *members);
void store_group_photos_by_filenames(GtkListStore *store, GList *members);
static GList *store_iter_list_to_filename_list(GtkListStore *store, GList *iters);
void store_group_select_name(GtkListStore *store, const gchar *filename);
void store_group_find_name(GtkListStore *store, const gchar *name, GtkTreeIter *iter, gint *n);
void store_get_members(GtkListStore *store, GtkTreeIter *iter, GList **members);
void store_get_type(GtkListStore *store, GtkTreeIter *iter, gint *type);
void store_get_fullname(GtkListStore *store, GtkTreeIter *iter, gchar **fullname);
void store_set_members(GtkListStore *store, GtkTreeIter *iter, GList *members);
void got_metadata(RSMetadata *metadata, gpointer user_data);

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
	signals[GROUP_ACTIVATED_SIGNAL] = g_signal_new ("group-activated",
		G_TYPE_FROM_CLASS (klass),
		G_SIGNAL_RUN_FIRST,
		0,
		NULL, 
		NULL,                
		g_cclosure_marshal_VOID__POINTER,
		G_TYPE_NONE,
		1,
		G_TYPE_POINTER);
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
	GtkWidget *label_tt[NUM_VIEWS];
	GtkCellRenderer *cell_renderer;
	gboolean show_filenames;
	GtkWidget *label_priorities;

	store->counter_blocked = FALSE;
	store->notebook = GTK_NOTEBOOK(gtk_notebook_new());
	store->store = gtk_list_store_new (NUM_COLUMNS,
		GDK_TYPE_PIXBUF,
		GDK_TYPE_PIXBUF,
		G_TYPE_STRING,
		G_TYPE_STRING,
		G_TYPE_INT,
		G_TYPE_BOOLEAN,
		G_TYPE_OBJECT,
		G_TYPE_INT,
		G_TYPE_POINTER);

	for (n=0;n<NUM_VIEWS;n++)
	{
		GtkTreeModel *filter;

		/* New Icon view */
		store->iconview[n] = gtk_icon_view_new();

		/* Pack everything up nicely, we need the space for what matters */
		gtk_icon_view_set_margin(GTK_ICON_VIEW(store->iconview[n]), 1);
		gtk_icon_view_set_row_spacing(GTK_ICON_VIEW(store->iconview[n]), 0);

		/* New cell-renderer for thumbnails */
		cell_renderer = eog_pixbuf_cell_renderer_new();

		/* Use our own cell renderer */
		gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (store->iconview[n]),
			cell_renderer, FALSE);

		/* Set everything up nice */
		g_object_set (cell_renderer,
			"follow-state", TRUE,
			"height", -1,  
			"width", -1, 
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

		store->label[n] = gtk_label_new(NULL);

		switch (n)
		{
			case 0: /* All */
				g_sprintf(label_text[n], _("* <small>(%d)</small>"), 0);
				label_tt[n] = gui_tooltip_no_window(store->label[n], _("All photos (excluding deleted)"), NULL);
				break;
			case 1: /* 1 */
				g_sprintf(label_text[n], _("1 <small>(%d)</small>"), 0);
				label_tt[n] = gui_tooltip_no_window(store->label[n], _("Priority 1 photos"), NULL);
				break;
			case 2: /* 2 */
				g_sprintf(label_text[n], _("2 <small>(%d)</small>"), 0);
				label_tt[n] = gui_tooltip_no_window(store->label[n], _("Priority 2 photos"), NULL);
				break;
			case 3: /* 3 */
				g_sprintf(label_text[n], _("3 <small>(%d)</small>"), 0);
				label_tt[n] = gui_tooltip_no_window(store->label[n], _("Priority 3 photos"), NULL);
				break;
			case 4: /* Unsorted */
				g_sprintf(label_text[n], _("U <small>(%d)</small>"), 0);
				label_tt[n] = gui_tooltip_no_window(store->label[n], _("Unprioritized photos"), NULL);
				break;
			case 5: /* Deleted */
				g_sprintf(label_text[n], _("D <small>(%d)</small>"), 0);
				label_tt[n] = gui_tooltip_no_window(store->label[n], _("Deleted photos"), NULL);
				break;
#if NUM_VIEWS != 6
 #error You need to update this switch statement
#endif
		}

		gtk_label_set_markup(GTK_LABEL(store->label[n]), label_text[n]);
		gtk_misc_set_alignment(GTK_MISC(store->label[n]), 0.0, 0.5);

		/* Add everything to the notebook */
		gtk_notebook_append_page(store->notebook, make_iconview(store->iconview[n], store, priorities[n]), label_tt[n]);
	}

	/* Load show filenames state from config */
	rs_conf_get_boolean_with_default(CONF_SHOW_FILENAMES, &show_filenames, DEFAULT_CONF_SHOW_FILENAMES);
	rs_store_set_show_filenames(store, show_filenames);

	/* Default to page 0 */
	store->current_iconview = store->iconview[0];
	store->current_priority = priorities[0];

	gtk_notebook_set_tab_pos(store->notebook, GTK_POS_LEFT);

	g_signal_connect(store->notebook, "switch-page", G_CALLBACK(switch_page), store);
	store->counthandler = g_signal_connect(store->store, "row-changed", G_CALLBACK(count_priorities), store->label);
	g_signal_connect(store->store, "row-deleted", G_CALLBACK(count_priorities_del), store->label);

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
	gint sort_method = RS_STORE_SORT_BY_NAME;
	rs_conf_get_integer(CONF_STORE_SORT_METHOD, &sort_method);
	rs_store_set_sort_method(store, sort_method);
	store->tooltip_text = g_string_new("...");
	store->tooltip_last_path = NULL;
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

	rs_io_idle_prefetch_file(filename, PRELOAD_CLASS);
}

static void
predict_preload(RSStore *store, gboolean initial)
{
	GList *selected = NULL;
	gint n, near = 5;
	GtkTreeIter iter;
	GtkIconView *iconview = GTK_ICON_VIEW(store->current_iconview);
	GtkTreePath *path, *next, *prev;
	GtkTreeModel *model = gtk_icon_view_get_model (iconview);

	rs_io_idle_cancel_class(PRELOAD_CLASS);

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
	RSStore *store = RS_STORE(data);
	GtkTreeModel *model = GTK_TREE_MODEL(store->store);
	GtkTreeIter iter;
	gint type;
	gchar *name;
	GList *group_member_list;
	GList *filename_list;
	GList *selected = NULL;
	gint num_selected;

	/* Get list of selected icons */
	selected = rs_store_get_selected_iters(store);
	num_selected = g_list_length(selected);

	/* Emit signal if only one thumbnail is selected */
	if (num_selected == 1)
	{
		iter = * (GtkTreeIter *) g_list_nth_data(selected, 0);

		/* Get type of row */
		gtk_tree_model_get(model, &iter, TYPE_COLUMN, &type, -1);
		switch (type)
		{
			case RS_STORE_TYPE_GROUP:
				gtk_tree_model_get(model, &iter, GROUP_LIST_COLUMN, &group_member_list, -1);
				filename_list = store_iter_list_to_filename_list(store->store, group_member_list);
				g_signal_emit(G_OBJECT(data), signals[GROUP_ACTIVATED_SIGNAL], 0, filename_list);
				g_list_free(filename_list);
				break;
			default:
				gtk_tree_model_get(GTK_TREE_MODEL(store->store), &iter, FULLNAME_COLUMN, &name, -1);
				g_signal_emit(G_OBJECT(data), signals[THUMB_ACTIVATED_SIGNAL], 0, name);
				break;
		}
	}

	g_list_foreach(selected, (GFunc)g_free, NULL);
	g_list_free(selected);

	predict_preload(data, FALSE);
}

#if GTK_CHECK_VERSION(2,12,0)
static gboolean
query_tooltip(GtkWidget *widget, gint x, gint y, gboolean keyboard_mode, GtkTooltip *tooltip, gpointer user_data)
{
	gboolean ret = FALSE;
	RSStore *store = RS_STORE(user_data);
	GtkIconView *iconview = GTK_ICON_VIEW(widget);
	GtkTreeModel *model;
	GtkTreePath *path;
	GtkScrolledWindow *scrolled_window;
	GtkAdjustment *adj;
	GtkTreeIter iter;

	/* Remember the scrollbar - but we only need the horizontal (for now) */
	scrolled_window = GTK_SCROLLED_WINDOW(gtk_widget_get_parent(widget));
	adj = GTK_ADJUSTMENT(gtk_scrolled_window_get_hadjustment(scrolled_window));
	x += (gint) gtk_adjustment_get_value(adj);

	/* See if there's an icon at current position */
	path = gtk_icon_view_get_path_at_pos(GTK_ICON_VIEW(widget), x, y);

	if (path)
	{
		/* If we differ, render a new tooltip text */
		if (!store->tooltip_last_path || (gtk_tree_path_compare(path, store->tooltip_last_path)!=0))
		{
			if (store->tooltip_last_path)
				gtk_tree_path_free(store->tooltip_last_path);
			store->tooltip_last_path = path;
			model = gtk_icon_view_get_model (iconview);
			if (path && gtk_tree_model_get_iter(model, &iter, path))
			{
				RSMetadata *metadata;
				gint type;
				gchar *name;
				gchar *filename;

				gtk_tree_model_get (model, &iter,
					TYPE_COLUMN, &type,
					TEXT_COLUMN, &name,
					FULLNAME_COLUMN, &filename,
					METADATA_COLUMN, &metadata,
					-1);

				RSLibrary *library = rs_library_get_singleton();
				gboolean autotag;
				rs_conf_get_boolean_with_default(CONF_LIBRARY_AUTOTAG, &autotag, DEFAULT_CONF_LIBRARY_AUTOTAG);
				GList *tags = rs_library_photo_tags(library, filename, autotag);

				if (metadata) switch(type)
				{
					case RS_STORE_TYPE_GROUP:
						g_string_printf(store->tooltip_text, "<big>FIXME: group</big>");
						break;
					default:
						g_string_printf(store->tooltip_text, _("<big>%s</big>\n\n"), name);

						if (metadata->focallength > 0)
							g_string_append_printf(store->tooltip_text, _("<b>Focal length</b>: %dmm\n"), metadata->focallength);

						if (metadata->shutterspeed > 0.0 && metadata->shutterspeed < 4)
							g_string_append_printf(store->tooltip_text, _("<b>Shutter speed</b>: %.1fs\n"), 1.0/metadata->shutterspeed);
						else if (metadata->shutterspeed >= 4)
							g_string_append_printf(store->tooltip_text, _("<b>Shutter speed</b>: 1/%.0fs\n"), metadata->shutterspeed);

						if (metadata->aperture > 0.0)
							g_string_append_printf(store->tooltip_text, _("<b>Aperture</b>: F/%.01f\n"), metadata->aperture);

						if (metadata->iso != 0)
							g_string_append_printf(store->tooltip_text, _("<b>ISO</b>: %u\n"), metadata->iso);

						if (metadata->time_ascii != NULL)
							g_string_append_printf(store->tooltip_text, _("<b>Time</b>: %s"), metadata->time_ascii);


						if (g_list_length(tags) > 0)
						{
							gint num = g_list_length(tags);
							gint n;

							g_string_append_printf(store->tooltip_text, "\n<b>Tags:<small>");

							for(n = 0; n < num; n++)
							{
								g_string_append_printf(store->tooltip_text, "\n - %s", (gchar *) g_list_nth_data(tags, n));
							}

							g_string_append_printf(store->tooltip_text, "</small></b>");
							g_list_free(tags);
						}

						g_object_unref(metadata);
						g_free(name);
						g_free(filename);
						break;
				}
			}
		}

		/* If we're hovering over an icon, we would like to show the tooltip */
		ret = TRUE;
	}

	gtk_tooltip_set_markup(tooltip, store->tooltip_text->str);

	return ret;
}
#endif /* GTK_CHECK_VERSION(2,12,0) */

static GtkWidget *
make_iconview(GtkWidget *iconview, RSStore *store, gint prio)
{
	GtkWidget *scroller;

	/* We must be abletoselect multiple icons */
	gtk_icon_view_set_selection_mode(GTK_ICON_VIEW (iconview), GTK_SELECTION_MULTIPLE);

#if GTK_CHECK_VERSION(2,12,0)
	/* Enable tooltips */
	g_object_set (iconview, "has-tooltip", TRUE, NULL);
	g_signal_connect(iconview, "query-tooltip", G_CALLBACK(query_tooltip), store);
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

	gtk_tree_model_get (model, iter,
	    PRIORITY_COLUMN, &p,
	    TYPE_COLUMN, &t, -1);

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
	if (!a[0] && !b[0])
		ret = 0;
	else if (!a[0])
		ret = 1;
	else if (!b[0])
		ret = -1;
	else
		ret = g_utf8_collate(a,b);
	g_free(a);
	g_free(b);
	return(ret);
}

static gint
model_sort_timestamp(GtkTreeModel *model, GtkTreeIter *tia, GtkTreeIter *tib, gpointer userdata)
{
	gint ret;
	RSMetadata *a, *b;

	gtk_tree_model_get(model, tia, METADATA_COLUMN, &a, -1);
	gtk_tree_model_get(model, tib, METADATA_COLUMN, &b, -1);

	if ((a!=NULL) && (b!=NULL) && (a->timestamp != b->timestamp))
		ret = a->timestamp - b->timestamp;
	else
		ret = model_sort_name(model, tia, tib, userdata);

	if (a!=NULL)
		g_object_unref(a);
	if (b!=NULL)
		g_object_unref(b);

	return(ret);
}

static gint
model_sort_iso(GtkTreeModel *model, GtkTreeIter *tia, GtkTreeIter *tib, gpointer userdata)
{
	gint ret;
	RSMetadata *a, *b;

	gtk_tree_model_get(model, tia, METADATA_COLUMN, &a, -1);
	gtk_tree_model_get(model, tib, METADATA_COLUMN, &b, -1);

	if ((a!=NULL) && (b!=NULL) && (a->iso != b->iso))
		ret = a->iso - b->iso;
	else
		ret = model_sort_name(model, tia, tib, userdata);

	if (a!=NULL)
		g_object_unref(a);
	if (b!=NULL)
		g_object_unref(b);

	return(ret);
}

static gint
model_sort_aperture(GtkTreeModel *model, GtkTreeIter *tia, GtkTreeIter *tib, gpointer userdata)
{
	gint ret;
	RSMetadata *a, *b;

	gtk_tree_model_get(model, tia, METADATA_COLUMN, &a, -1);
	gtk_tree_model_get(model, tib, METADATA_COLUMN, &b, -1);

	if ((a!=NULL) && (b!=NULL) && (a->aperture != b->aperture))
		ret = a->aperture*10.0 - b->aperture*10.0;
	else
		ret = model_sort_name(model, tia, tib, userdata);

	if (a!=NULL)
		g_object_unref(a);
	if (b!=NULL)
		g_object_unref(b);

	return(ret);
}

static gint
model_sort_focallength(GtkTreeModel *model, GtkTreeIter *tia, GtkTreeIter *tib, gpointer userdata)
{
	gint ret;
	RSMetadata *a, *b;

	gtk_tree_model_get(model, tia, METADATA_COLUMN, &a, -1);
	gtk_tree_model_get(model, tib, METADATA_COLUMN, &b, -1);

	if ((a!=NULL) && (b!=NULL) && (a->focallength != b->focallength))
		ret = a->focallength*10.0 - b->focallength*10.0;
	else
		ret = model_sort_name(model, tia, tib, userdata);

	if (a!=NULL)
		g_object_unref(a);
	if (b!=NULL)
		g_object_unref(b);

	return(ret);
}

static gint
model_sort_shutterspeed(GtkTreeModel *model, GtkTreeIter *tia, GtkTreeIter *tib, gpointer userdata)
{
	gint ret;
	RSMetadata *a, *b;

	gtk_tree_model_get(model, tia, METADATA_COLUMN, &a, -1);
	gtk_tree_model_get(model, tib, METADATA_COLUMN, &b, -1);

	if ((a!=NULL) && (b!=NULL) && (a->shutterspeed != b->shutterspeed))
		ret = b->shutterspeed*10.0 - a->shutterspeed*10.0;
	else
		ret = model_sort_name(model, tia, tib, userdata);

	if (a!=NULL)
		g_object_unref(a);
	if (b!=NULL)
		g_object_unref(b);

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
thumbnail_overlay(GdkPixbuf *pixbuf, GdkPixbuf *lowerleft, GdkPixbuf *lowerright, GdkPixbuf *topleft, GdkPixbuf *topright)
{
	gint thumb_width;
	gint thumb_height;
	gint icon_width;
	gint icon_height;

	thumb_width = gdk_pixbuf_get_width(pixbuf);
	thumb_height = gdk_pixbuf_get_height(pixbuf);

	/* Apply lower left icon */
	if (lowerleft)
	{
		icon_width = gdk_pixbuf_get_width(lowerleft);
		icon_height = gdk_pixbuf_get_height(lowerleft);

		gdk_pixbuf_composite(lowerleft, pixbuf,
				2, thumb_height-icon_height-2,
				icon_width, icon_height,
				2, thumb_height-icon_height-2,
				1.0, 1.0, GDK_INTERP_NEAREST, 255);
	}

	/* Apply lower right icon */
	if (lowerright)
	{
		icon_width = gdk_pixbuf_get_width(lowerright);
		icon_height = gdk_pixbuf_get_height(lowerright);

		gdk_pixbuf_composite(lowerright, pixbuf,
				thumb_width-icon_width-2, thumb_height-icon_height-2,
				icon_width, icon_height,
				thumb_width-icon_width-2, thumb_height-icon_height-2,
				1.0, 1.0, GDK_INTERP_NEAREST, 255);
	}

	/* Apply top left icon */
	if (topleft)
	{
		icon_width = gdk_pixbuf_get_width(topleft);
		icon_height = gdk_pixbuf_get_height(topleft);

		gdk_pixbuf_composite(topleft, pixbuf,
				     2, 2,
				icon_width, icon_height,
				     2, 2,
				1.0, 1.0, GDK_INTERP_NEAREST, 255);
	}

	/* Apply top right icon */
	if (topright)
	{
		icon_width = gdk_pixbuf_get_width(topright);
		icon_height = gdk_pixbuf_get_height(topright);

		gdk_pixbuf_composite(topright, pixbuf,
				     thumb_width-icon_width-2, 2,
				icon_width, icon_height,
				     thumb_width-icon_width-2, 2,
				1.0, 1.0, GDK_INTERP_NEAREST, 255);
	}

	return;
}

static void
thumbnail_update(GdkPixbuf *pixbuf, GdkPixbuf *pixbuf_clean, gint priority, gboolean exported)
{
	GdkPixbuf *icon_priority_temp;
	GdkPixbuf *icon_exported_temp;

	if (!pixbuf_clean)
		return;
	if (!(gdk_pixbuf_get_width(pixbuf_clean) && gdk_pixbuf_get_height(pixbuf_clean)))
		return;

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

	thumbnail_overlay(pixbuf, icon_exported_temp, icon_priority_temp, NULL, NULL);
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

void
rs_store_load_file(RSStore *store, gchar *fullname)
{
	GtkTreeIter iter;
	WORKER_JOB *job;
	
	if (!fullname)
		return;

	gchar *name = g_path_get_basename(fullname);

	/* Global default icon */
	if (!icon_default)
		icon_default = gdk_pixbuf_new_from_file(PACKAGE_DATA_DIR "/icons/" PACKAGE ".png", NULL);

	/* Add file to store */
	gdk_threads_enter();
	gtk_list_store_append (store->store, &iter);
	gtk_list_store_set (store->store, &iter,
			    METADATA_COLUMN, NULL,
			    PIXBUF_COLUMN, icon_default,
			    PIXBUF_CLEAN_COLUMN, icon_default,
				TEXT_COLUMN, name,
			    FULLNAME_COLUMN, fullname,
			    -1);

	gdk_threads_leave();

	/* Push an asynchronous job for loading the thumbnail */
	job = g_new(WORKER_JOB, 1);
	job->store = g_object_ref(store);
	job->iter = iter;
	job->filename = g_strdup(fullname);
	job->name = g_strdup(name);
	job->model = g_object_ref(GTK_TREE_MODEL(store->store));

	rs_io_idle_read_metadata(job->filename, METADATA_CLASS, got_metadata, job);

	g_atomic_int_inc(&store->jobs_to_do);
}

static gint
load_directory(RSStore *store, const gchar *path, RSLibrary *library, const gboolean load_8bit, const gboolean load_recursive)
{
	const gchar *name;
	gchar *fullname;
	GDir *dir;
	gint count = 0;

	gchar *path_normalized = rs_normalize_path(path);

	rs_library_restore_tags(path_normalized);

	dir = g_dir_open(path_normalized, 0, NULL); /* FIXME: check errors */

	while((dir != NULL) && (name = g_dir_read_name(dir)))
	{
		/* Ignore "hidden" files and directories */
		if (name[0] == '.')
			continue;

		fullname = g_build_filename(path, name, NULL);

		if (rs_filetype_can_load(fullname))
		{
			rs_store_load_file(store, fullname);
			count++;
		}
		else if (load_recursive && g_file_test(fullname, G_FILE_TEST_IS_DIR))
			count += load_directory(store, fullname, library, load_8bit, load_recursive);

		g_free(fullname);
	}

	g_free(path_normalized);
	if (dir)
		g_dir_close(dir);

	return count;
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

	/* Empty the loader queue */
	rs_io_idle_cancel_class(METADATA_CLASS);
	rs_io_idle_cancel_class(PRELOAD_CLASS);

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

	gdk_threads_enter();
	/* We got iter, just remove it */
	if (iter)
		gtk_list_store_remove(GTK_LIST_STORE(GTK_TREE_MODEL(store->store)), iter);

	/* If both are NULL, remove everything */
	if ((filename == NULL) && (iter == NULL))
	{
		gint i;

		/* If we remove everything we have to block selection-changed signal */
		for(i=0;i<NUM_VIEWS;i++)
			g_signal_handlers_block_by_func(store->iconview[i], selection_changed, store);

		gtk_list_store_clear(store->store);

		for(i=0;i<NUM_VIEWS;i++)
			g_signal_handlers_unblock_by_func(store->iconview[i], selection_changed, store);
	}

	gdk_threads_leave();

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
	RSLibrary *library = rs_library_get_singleton();
	static gboolean running = FALSE;
	static GStaticMutex lock = G_STATIC_MUTEX_INIT;

	GtkTreeSortable *sortable;
	gboolean load_8bit = FALSE;
	gboolean load_recursive = DEFAULT_CONF_LOAD_RECURSIVE;
	gint items=0, n;

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
	if (!rs_conf_get_string(CONF_LWD))
		load_recursive = FALSE;

	/* Disable sort while loading - this greatly reduces the change of triggering a GTK crash bug */
	sortable = GTK_TREE_SORTABLE(store->store);
	gtk_tree_sortable_set_sort_column_id(sortable, GTK_TREE_SORTABLE_UNSORTED_SORT_COLUMN_ID, GTK_SORT_ASCENDING);

	g_atomic_int_set(&store->jobs_to_do, 0);
	gtk_label_set_markup(GTK_LABEL(store->label[0]), _("* <small>(-)</small>"));
	gtk_label_set_markup(GTK_LABEL(store->label[1]), _("1 <small>(-)</small>"));
	gtk_label_set_markup(GTK_LABEL(store->label[2]), _("2 <small>(-)</small>"));
	gtk_label_set_markup(GTK_LABEL(store->label[3]), _("3 <small>(-)</small>"));
	gtk_label_set_markup(GTK_LABEL(store->label[4]), _("U <small>(-)</small>"));
	gtk_label_set_markup(GTK_LABEL(store->label[5]), _("D <small>(-)</small>"));
	g_signal_handler_block(store->store, store->counthandler);
	store->counter_blocked = TRUE;

	/* While we're loading, we keep the IO lock to ourself. We need to read very basic meta and directory data */
	rs_io_lock();
	items = load_directory(store, path, library, load_8bit, load_recursive);
	rs_io_unlock();

	/* unset model and make sure we have enough columns */
	gdk_threads_enter();
	for (n=0;n<NUM_VIEWS;n++)
	{
		gtk_icon_view_set_model(GTK_ICON_VIEW (store->iconview[n]), NULL);
		gtk_icon_view_set_columns(GTK_ICON_VIEW (store->iconview[n]), items);
	}

	/* Sort the store */
	rs_store_set_sort_method(store, store->sort_method);

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
	gdk_threads_leave();

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
		if (priority || exported)
			rs_cache_save_flags(fullname, priority, exported);
		return TRUE;
	}

	return FALSE;
}

/**
 * Select a image
 * @param store A RSStore
 * @param name The filename to select
 * @param deselect_others Should other images be de-selected - this will also make the iconview scroll to the new icon.
 */
gboolean
rs_store_set_selected_name(RSStore *store, const gchar *filename, gboolean deselect_others)
{
	gboolean ret = FALSE;
	GtkTreePath *path = NULL;

	g_return_val_if_fail(RS_IS_STORE(store), FALSE);
	g_return_val_if_fail(filename, FALSE);

	if (deselect_others)
		gtk_icon_view_unselect_all(GTK_ICON_VIEW(store->current_iconview));

	tree_find_filename(GTK_TREE_MODEL(store->store), filename, NULL, &path);

	if (path)
	{
		/* Get model for current icon-view */
		GtkTreeModel *model = gtk_icon_view_get_model (GTK_ICON_VIEW(store->current_iconview));

		/* Get the path in iconview and free path */
		GtkTreePath *iconpath = gtk_tree_model_filter_convert_child_path_to_path(GTK_TREE_MODEL_FILTER(model), path);
		gtk_tree_path_free(path);

		/* Scroll to the icon */
		if (deselect_others)
			gtk_icon_view_scroll_to_path(GTK_ICON_VIEW(store->current_iconview), iconpath, FALSE, 0.0, 0.0);

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
		gtk_icon_view_set_cursor (iconview, newpath, NULL, FALSE);
		gtk_widget_grab_focus(GTK_WIDGET(iconview));
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

/**
 * Sets the sorting method in a RSStore
 * @param store A RSStore
 * @param sort A sort method from the RS_STORE_SORT_BY-family of enums
 */
void
rs_store_set_sort_method(RSStore *store, RS_STORE_SORT_METHOD sort_method)
{
	GtkTreeSortable *sortable;
	gint sort_column = TEXT_COLUMN;
	GtkTreeIterCompareFunc sort_func = model_sort_name;

	g_assert(RS_IS_STORE(store));

	store->sort_method = sort_method;
	rs_conf_set_integer(CONF_STORE_SORT_METHOD, sort_method);

	switch (sort_method)
	{
		case RS_STORE_SORT_BY_NAME:
			sort_column = TEXT_COLUMN;
			sort_func = model_sort_name;
			break;
		case RS_STORE_SORT_BY_TIMESTAMP:
			sort_column = METADATA_COLUMN;
			sort_func = model_sort_timestamp;
			break;
		case RS_STORE_SORT_BY_ISO:
			sort_column = METADATA_COLUMN;
			sort_func = model_sort_iso;
			break;
		case RS_STORE_SORT_BY_APERTURE:
			sort_column = METADATA_COLUMN;
			sort_func = model_sort_aperture;
			break;
		case RS_STORE_SORT_BY_FOCALLENGTH:
			sort_column = METADATA_COLUMN;
			sort_func = model_sort_focallength;
			break;
		case RS_STORE_SORT_BY_SHUTTERSPEED:
			sort_column = METADATA_COLUMN;
			sort_func = model_sort_shutterspeed;
			break;
	}

	sortable = GTK_TREE_SORTABLE(store->store);
	gtk_tree_sortable_set_sort_func(sortable,
		sort_column,
		sort_func,
		store,
		NULL);
	gtk_tree_sortable_set_sort_column_id(sortable, sort_column, GTK_SORT_ASCENDING);
}

/**
 * Get the sorting method for a RSStore
 * @param store A RSStore
 * @return A sort method from the RS_STORE_SORT_BY-family of enums
 */
extern RS_STORE_SORT_METHOD
rs_store_get_sort_method(RSStore *store)
{
	g_assert(RS_IS_STORE(store));

	return store->sort_method;
}

void
cairo_draw_thumbnail(cairo_t *cr, GdkPixbuf *pixbuf, gint x, gint y, gint width, gint height, gdouble alpha)
{
	gdouble greyvalue = 0.9;

	cairo_set_source_rgba(cr, greyvalue, greyvalue, greyvalue, 1.0);
	cairo_rectangle(cr, x, y, width, height);
	cairo_fill(cr);

	gdk_cairo_set_source_pixbuf(cr, pixbuf, (x+2), (y+2));
	cairo_paint(cr);

	cairo_set_source_rgba(cr, 0.5, 0.5, 0.5, alpha);
	cairo_rectangle(cr, x+1, y+1, width-2, height-2);
	cairo_fill(cr);

	return;
}

void
calc_rotated_coordinats(gdouble a, gdouble b, gdouble R, gdouble *a2, gdouble *b2)
{
	gdouble c, A;

	c = sqrt(a*a+b*b);
	A = atan(a/b)+R;
	*a2 = sin(A)*c;
	*b2 = cos(A)*c;
}

GdkPixbuf *
store_group_update_pixbufs(GdkPixbuf *pixbuf, GdkPixbuf *pixbuf_clean)
{
	gint width, height;
	GdkPixbuf *new_pixbuf;

	width = gdk_pixbuf_get_width(pixbuf_clean);
	height = gdk_pixbuf_get_height(pixbuf_clean);

	new_pixbuf = gdk_pixbuf_new(gdk_pixbuf_get_colorspace(pixbuf_clean),
								TRUE,
								gdk_pixbuf_get_bits_per_sample(pixbuf_clean),
								width,
								height);

#if GTK_CHECK_VERSION(2,8,0) && defined(EXPERIMENTAL)	

	gdouble a2, b2, scale, bb_x1, bb_x2, bb_y1, bb_y2, bb_height, bb_width, xoffset, yoffset, border = 1;

	/* We have a bit more room with landscape-mode photos than with portrait-mode*/
	if (height > width)
		scale = 0.9;
	else
		scale = 1.0;

	/* upper left of left rotation - we need a2 */
	calc_rotated_coordinats((0-(width/2)), 128, -0.1, &a2, &b2);
	bb_x1 = a2;

	/* upper left of right rotation - we need b2 */
	calc_rotated_coordinats((0-(width/2)), 128, 0.2, &a2, &b2);
	bb_y1 = b2;

	/* upper right of right rotation - we need b2 */
	calc_rotated_coordinats((width/2), 128, 0.2, &a2, &b2);
	bb_x2 = a2;

	/* lower right of right rotation - we need b2 */
	calc_rotated_coordinats((width/2),(128-height), 0.2, &a2, &b2);
	bb_y2 = b2;

	/* Calculate the magic numbers - it will work from scale 0.7 and up */
	bb_height = ((bb_y1-bb_y2)+border*2)*scale;
	bb_width = ((bb_x1*-1+bb_x2+10)+border*2)*scale;
	xoffset = (bb_x2+bb_x1)/2/scale*-1;
	yoffset = (128-(bb_y1-(128)))/scale*-1;

	cairo_surface_t *surface;
	cairo_t *cr;

	surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, bb_width, bb_height);

	cr = cairo_create(surface);

	cairo_translate(cr, (bb_width/2), 128);
	cairo_scale(cr, scale, scale);

	cairo_rotate(cr, -0.1);
	cairo_draw_thumbnail(cr, pixbuf_clean, (width/-2)+xoffset, yoffset, width, height, 0.7);
	cairo_rotate(cr, 0.2);
	cairo_draw_thumbnail(cr, pixbuf_clean, (width/-2)+xoffset, yoffset, width, height, 0.7);
	cairo_rotate(cr, 0.1);
	cairo_draw_thumbnail(cr, pixbuf_clean, (width/-2)+xoffset, yoffset, width, height, 0.7);
	cairo_rotate(cr, -0.2);
	cairo_draw_thumbnail(cr, pixbuf_clean, (width/-2)+xoffset, yoffset, width, height, 0.0);

	cairo_destroy(cr);
	new_pixbuf = cairo_convert_to_pixbuf(surface);
#else

	guint rowstride;
	guchar *pixels;
	gint channels;
	gint new_width, new_height;
	GdkPixbuf *pixbuf_scaled;

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

#endif

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

	store_get_members(store, &iter, &members);

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
				store_get_members(store->store, s, &members);
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
			store_get_members(store->store, iter, &members);

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
	gint t;

	store_get_type(store, iter, &t);
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
	store_get_fullname(store, &iter, &filename);

	dotdir = rs_dotdir_get(filename);
	if (!dotdir)
		return;
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
				gchar *selected = NULL;
				GList *members, *filenames;

				xmlTextWriterStartElement(writer, BAD_CAST "group");

				// Find selected member and place this first in XML
				store_get_fullname(store, &iter, &selected);

				xmlTextWriterWriteFormatElement(writer, BAD_CAST "member", "%s", selected);

				store_get_members(store, &iter, &members);

				gint m;
				filenames = store_iter_list_to_filename_list(store, members);

				for( m = 0; m < g_list_length(filenames); m++)
				{
					filename = g_list_nth_data(filenames, m);
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

	/* Don't try opening groups when there is no loaded photos in the store. */
	  if(!gtk_list_store_iter_is_valid(store, &iter))
		return;

	store_get_fullname(store, &iter, &filename);
	dotdir = rs_dotdir_get(filename);
	if (!dotdir)
		return;

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

void
rs_store_auto_group(RSStore *store)
{
	gchar *filename = NULL;
	gint timestamp = 0, timestamp_old = 0;
	gint exposure;
	RSMetadata *meta;
	GList *filenames = NULL;
	GtkTreeIter iter;

	// TODO: remove all existing groups in iconview.
	
	gtk_tree_model_get_iter_first(GTK_TREE_MODEL(store->store), &iter);
	do
	{
		store_get_fullname(GTK_LIST_STORE(store->store), &iter, &filename);
		meta = rs_metadata_new_from_file(filename);

		if (!meta->timestamp)
			return;

		timestamp = meta->timestamp;
		exposure = (1/meta->shutterspeed);

		if (timestamp > timestamp_old + 1)
		{
			if (g_list_length(filenames) > 1)
				store_group_photos_by_filenames(store->store, filenames);
			g_list_free(filenames);
			filenames = NULL;
		}
		timestamp_old = timestamp + exposure;
		g_free(meta);

		filenames = g_list_append(filenames, filename);
	} while (gtk_tree_model_iter_next(GTK_TREE_MODEL(store->store), &iter));

	store_group_photos_by_filenames(store->store, filenames);
	g_list_free(filenames);
	filenames = NULL;
}

static GList *
store_iter_list_to_filename_list(GtkListStore *store, GList *iters)
{
	gint n;
	gchar *filename = NULL;
	GList *filenames = NULL;

	for (n=0; n<g_list_length(iters); n++)
	{
		GtkTreeIter *iter = (GtkTreeIter *) g_list_nth_data(iters, n);
		store_get_fullname(store, iter, &filename);
		filenames = g_list_append(filenames, filename);
	}

	return filenames;
}

void
store_group_select_name(GtkListStore *store, const gchar *filename)
{
	GtkTreeIter iter;
	gint n = -1;

	store_group_find_name(store, filename, &iter, &n);
	store_group_select_n(store, iter, n);
}

void 
store_group_find_name(GtkListStore *store, const gchar *name, GtkTreeIter *iter, gint *n)
{
	gint type;
	gint i;
	GList *members = NULL;
	GList *filenames = NULL;

	gtk_tree_model_get_iter_first(GTK_TREE_MODEL(store), iter);
	do
	{
		store_get_type(store, iter, &type);

		if (type == RS_STORE_TYPE_GROUP)
		{
			store_get_members(store, iter, &members);
			filenames = store_iter_list_to_filename_list(store, members);

			for(i = 0; i < g_list_length(filenames); i++)
			{
				if (g_str_equal(g_list_nth_data(filenames, i), name))
				{
					*n = i;
					return;
				}
			}
			g_list_free(filenames);
		}
	} while (gtk_tree_model_iter_next(GTK_TREE_MODEL(store), iter));

	*n = -1;
	return;
}

void
rs_store_group_select_name(RSStore *store, const gchar *filename)
{
	store_group_select_name(store->store, filename);
}

void 
store_group_ungroup_name(GtkListStore *store, const gchar *name)
{
	GtkTreeIter iter, *child_iter;
	gint n = -1;
	GList *members;

	store_group_find_name(store, name, &iter, &n);
	
	gtk_tree_model_get (GTK_TREE_MODEL(store), &iter, 
						GROUP_LIST_COLUMN, &members,
						-1);
	
	child_iter = (GtkTreeIter *) g_list_nth_data(members, n);
	gtk_list_store_set(store, child_iter,
						TYPE_COLUMN, RS_STORE_TYPE_FILE,
					   -1);

	members = g_list_remove(members, g_list_nth_data(members,n));

	/* If group now only has one member, we destroy the group */
	if (g_list_length(members) == 1)
	{
		child_iter = (GtkTreeIter *) g_list_nth_data(members, 0);
		gtk_list_store_set(store, child_iter,
							TYPE_COLUMN, RS_STORE_TYPE_FILE,
						   -1);
		gtk_list_store_remove (store, &iter);
	}
	store_set_members(store, &iter, members);
	store_save_groups(store);
}

void
rs_store_group_ungroup_name(RSStore *store, const gchar *filename)
{
	store_group_ungroup_name(store->store, filename);
}

void
store_get_members(GtkListStore *store, GtkTreeIter *iter, GList **members)
{
	gtk_tree_model_get (GTK_TREE_MODEL(store), iter,
						GROUP_LIST_COLUMN, members,
						-1);
}

void
store_get_type(GtkListStore *store, GtkTreeIter *iter, gint *type)
{
	gtk_tree_model_get (GTK_TREE_MODEL(store), iter,
						TYPE_COLUMN, type,
						-1);
}

void
store_get_fullname(GtkListStore *store, GtkTreeIter *iter, gchar **fullname)
{
	gtk_tree_model_get (GTK_TREE_MODEL(store), iter, 
						FULLNAME_COLUMN, fullname,
						-1);
}

void
store_set_members(GtkListStore *store, GtkTreeIter *iter, GList *members)
{
	gtk_list_store_set (store, iter, 
						GROUP_LIST_COLUMN, members,
						-1);
}

/* This function is grabbed from http://stevehanov.ca/blog/index.php?id=53 */
void cairo_image_surface_blur( cairo_surface_t* surface, double radius )
{
	// Steve Hanov, 2009
	// Released into the public domain.

	// get width, height
	int width = cairo_image_surface_get_width( surface );
	int height = cairo_image_surface_get_height( surface );
	unsigned char* dst = (unsigned char*)malloc(width*height*4);
	unsigned* precalc = (unsigned*)malloc(width*height*sizeof(unsigned));
	unsigned char* src = cairo_image_surface_get_data( surface );
	double mul=1.f/((radius*2)*(radius*2));
	int channel;

	// The number of times to perform the averaging. According to wikipedia,
	// three iterations is good enough to pass for a gaussian.
	const int MAX_ITERATIONS = 3;
	int iteration;

	memcpy( dst, src, width*height*4 );

	for ( iteration = 0; iteration < MAX_ITERATIONS; iteration++ ) {
		for( channel = 0; channel < 4; channel++ ) {
			int x,y;

			// precomputation step.
			unsigned char* pix = src;
			unsigned* pre = precalc;

			pix += channel;
			for (y=0;y<height;y++) {
				for (x=0;x<width;x++) {
					int tot=pix[0];
					if (x>0) tot+=pre[-1];
					if (y>0) tot+=pre[-width];
					if (x>0 && y>0) tot-=pre[-width-1];
					*pre++=tot;
					pix += 4;
				}
			}

			// blur step.
			pix = dst + (int)radius * width * 4 + (int)radius * 4 + channel;
			for (y=radius;y<height-radius;y++) {
				for (x=radius;x<width-radius;x++) {
					int l = x < radius ? 0 : x - radius;
					int t = y < radius ? 0 : y - radius;
					int r = x + radius >= width ? width - 1 : x + radius;
					int b = y + radius >= height ? height - 1 : y + radius;
					int tot = precalc[r+b*width] + precalc[l+t*width] -
					precalc[l+b*width] - precalc[r+t*width];
					*pix=(unsigned char)(tot*mul);
					pix += 4;
				}
				pix += (int)radius * 2 * 4;
			}
		}
		memcpy( src, dst, width*height*4 );
	}

	free( dst );
	free( precalc );
}

cairo_surface_t *
cairo_surface_make_shadow(cairo_surface_t *surface)
{
	GdkPixbuf *pixbuf = cairo_convert_to_pixbuf(surface);
	guchar *pixels = gdk_pixbuf_get_pixels(pixbuf);
	int x, y;
	for (y = 0; gdk_pixbuf_get_height(pixbuf) > y; y++) {
		for (x = 0; gdk_pixbuf_get_rowstride(pixbuf) > x; x+=4) {
			pixels[y*gdk_pixbuf_get_rowstride(pixbuf)+x+0] = 50;
			pixels[y*gdk_pixbuf_get_rowstride(pixbuf)+x+1] = 50;
			pixels[y*gdk_pixbuf_get_rowstride(pixbuf)+x+2] = 50;
			if (pixels[y*gdk_pixbuf_get_rowstride(pixbuf)+x+3] > 0)
				pixels[y*gdk_pixbuf_get_rowstride(pixbuf)+x+3] = 255;
			else
				pixels[y*gdk_pixbuf_get_rowstride(pixbuf)+x+3] = 0;
		}
	}

	cairo_surface_t *surface_new = cairo_image_surface_create(cairo_image_surface_get_format(surface), cairo_image_surface_get_width(surface), cairo_image_surface_get_height(surface));
	cairo_t *cr = cairo_create(surface_new);

	gdk_cairo_set_source_pixbuf(cr, pixbuf, 0, 0);
	cairo_paint(cr);

	cairo_destroy(cr);

	return surface_new;
}

GdkPixbuf *
get_thumbnail_eyecandy(GdkPixbuf *thumbnail)
{
	gdouble scale = 1.0;

	cairo_surface_t *surface;
	cairo_t *cr;

	gdouble a2, b2, bb_x1, bb_x2, bb_y1, bb_y2, bb_height, bb_width;
	gint height_centering;

	gint frame_size = 4; /* MAGIC CONSTANT - see cairo_draw_thumbnail() */
	gint width = gdk_pixbuf_get_width(thumbnail)+frame_size;
	gint height = gdk_pixbuf_get_height(thumbnail)+frame_size;

	gint calc_width = width+8*3;
	gint calc_height = 128+8*3;

	gdouble random = 0.0; /* only for use when rotating */

#ifdef EXPERIMENTAL
	/* Overwrite random if we want rotation */
	random = g_random_double_range(-0.05, 0.05);
#endif

	if (random > 0.0) {	
		calc_rotated_coordinats((0-(calc_width/2)), 0, random, &a2, &b2);
		bb_x1 = a2;

		calc_rotated_coordinats((calc_width/2), calc_height, random, &a2, &b2);
		bb_x2 = a2;

		calc_rotated_coordinats((0-(calc_width/2)), calc_height, random, &a2, &b2);
		bb_y1 = b2;

		calc_rotated_coordinats((calc_width/2), 0, random, &a2, &b2);
		bb_y2 = b2;
	} else {
		calc_rotated_coordinats((0-(calc_width/2)), calc_height, random, &a2, &b2);
		bb_x1 = a2;

		calc_rotated_coordinats((calc_width/2), 0, random, &a2, &b2);
		bb_x2 = a2;

		calc_rotated_coordinats((calc_width/2), calc_height, random, &a2, &b2);
		bb_y1 = b2;

		calc_rotated_coordinats(0-(calc_width/2), 0, random, &a2, &b2);
		bb_y2 = b2;
	}

	/* Calculate the magic numbers */
	bb_height = (bb_y1-bb_y2);
	bb_width = (bb_x1*-1+bb_x2);

	surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, bb_width*scale, bb_height*scale);
	cr = cairo_create(surface);
	cairo_scale(cr, scale, scale);

	cairo_translate(cr, bb_width/2, bb_height/2);

#ifdef EXPERIMENTAL
	cairo_rotate(cr, random);
#endif

	/* Only adjust height when it's a landscape mode photo */
	if (width > height)
		height_centering = ((bb_height-height)/2)-8;
	else
		height_centering = 0;

	cairo_draw_thumbnail(cr, thumbnail, bb_width/2*-1+12, bb_height/2*-1+8+height_centering, width, height, 0.0);
	surface = cairo_surface_make_shadow(surface);
	cairo_destroy(cr);
	cr = cairo_create(surface);
	cairo_scale(cr, scale, scale);

	cairo_image_surface_blur(surface, round(4.0*scale));
	cairo_translate(cr, bb_width/2, bb_height/2);

#ifdef EXPERIMENTAL
	cairo_rotate(cr, random);
#endif

	cairo_draw_thumbnail(cr, thumbnail, bb_width/2*-1+4, bb_height/2*-1+0+height_centering, width, height, 0.0);

	cairo_destroy(cr);
	GdkPixbuf *pixbuf = cairo_convert_to_pixbuf(surface);

	return pixbuf;
}

void 
rs_store_update_thumbnail(RSStore *store, const gchar *filename, GdkPixbuf *pixbuf)
{
	GdkPixbuf *pixbuf_clean;
	GtkTreeIter i;
	guint prio;
	gboolean expo;

	if (!pixbuf || !filename || !store || !store->store)
		return;

	if (tree_find_filename(GTK_TREE_MODEL(store->store), filename, &i, NULL))
	{
#if GTK_CHECK_VERSION(2,8,0)
	  pixbuf = get_thumbnail_eyecandy(pixbuf);
#endif
		pixbuf_clean = gdk_pixbuf_copy(pixbuf);

		gtk_tree_model_get(GTK_TREE_MODEL(store->store), &i,
			PRIORITY_COLUMN, &prio,
			EXPORTED_COLUMN, &expo,
			-1);

		gdk_threads_enter();
		thumbnail_update(pixbuf, pixbuf_clean, prio, expo);

		gtk_list_store_set(GTK_LIST_STORE(store->store), &i,
			PIXBUF_COLUMN, pixbuf,
			PIXBUF_CLEAN_COLUMN, pixbuf_clean,
			-1);
		gdk_threads_leave();
	}
}

void
got_metadata(RSMetadata *metadata, gpointer user_data)
{
	WORKER_JOB *job = user_data;
	gboolean exported;
	gint priority;
	GdkPixbuf *pixbuf, *pixbuf_clean;

	pixbuf = rs_metadata_get_thumbnail(metadata);

	if (pixbuf==NULL)
		/* We will use this, if no thumbnail can be loaded */
		pixbuf = gtk_widget_render_icon(GTK_WIDGET(job->store),
			GTK_STOCK_MISSING_IMAGE, GTK_ICON_SIZE_DIALOG, NULL);
#if GTK_CHECK_VERSION(2,8,0)
	else
	  pixbuf = get_thumbnail_eyecandy(pixbuf);
#endif

	pixbuf_clean = gdk_pixbuf_copy(pixbuf);

	rs_cache_load_quick(job->filename, &priority, &exported);

	/* Update thumbnail */
	gdk_threads_enter();
	thumbnail_update(pixbuf, pixbuf_clean, priority, exported);

	g_assert(pixbuf != NULL);
	g_assert(pixbuf_clean != NULL);

	/* Add the new thumbnail to the store */
	gtk_list_store_set(GTK_LIST_STORE(job->model), &job->iter,
		METADATA_COLUMN, metadata,
		PIXBUF_COLUMN, pixbuf,
		PIXBUF_CLEAN_COLUMN, pixbuf_clean,
		PRIORITY_COLUMN, priority,
		EXPORTED_COLUMN, exported,
		-1);
	gdk_threads_leave();

	/* Add to library */
	rs_library_add_photo_with_metadata(rs_library_get_singleton(), job->filename, metadata);

	/* The GtkListStore should have ref'ed these */
	g_object_unref(pixbuf);
	g_object_unref(pixbuf_clean);

	if (g_atomic_int_dec_and_test(&job->store->jobs_to_do))
	{
		gdk_threads_enter();
		/* FIXME: Refilter as this point - not before */
		if (job->store->counter_blocked)
			g_signal_handler_unblock(job->store->store, job->store->counthandler);
		job->store->counter_blocked = FALSE;

		count_priorities(GTK_TREE_MODEL(job->store->store), NULL, NULL, job->store->label);
		RS_STORE_SORT_METHOD sort_method;
		if (rs_conf_get_integer(CONF_STORE_SORT_METHOD, (gint*)&sort_method))
			rs_store_set_sort_method(job->store, sort_method);
		else
			rs_store_set_sort_method(job->store, RS_STORE_SORT_BY_NAME);
		gdk_threads_leave();
	}

	/* Clean up the job */
	g_free(job->filename);
	g_object_unref(job->store);
	g_object_unref(job->model);
	g_free(job);
}

void rs_store_set_iconview_size(RSStore *store, gint size)
{
	gint n;

	for (n=0;n<NUM_VIEWS;n++)
		gtk_icon_view_set_columns(GTK_ICON_VIEW (store->iconview[n]), size);
}

gint
rs_store_get_iconview_size(RSStore *store)
{
	gint n;

	for (n=0;n<NUM_VIEWS;n++)
	 n += MAX(0, gtk_icon_view_get_columns(GTK_ICON_VIEW (store->iconview[n])));

	return n;
}
