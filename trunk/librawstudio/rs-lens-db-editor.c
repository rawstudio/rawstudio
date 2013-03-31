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

/*
 * The following functions is more or less grabbed from UFraw:
 * lens_set(), lens_menu_select(), ptr_array_insert_sorted(),
 * ptr_array_find_sorted(), ptr_array_insert_index() and lens_menu_fill()
 */

#include <rawstudio.h>
#include <glib.h>
#include <glib/gstdio.h>
#include <gtk/gtk.h>
#include <string.h>
#include <config.h>
#include <lensfun.h>
#include <rs-lens-db.h>
#include <rs-lens.h>
#include <gettext.h>
#include <curl/curl.h>
#include <libxml/HTMLparser.h>
#include "rs-lens-db-editor.h"

static void fill_model(RSLensDb *lens_db, GtkTreeModel *tree_model);
static char * rs_lens_db_editor_update_lensfun(void);
GtkDialog *rs_lens_db_editor_single_lens(RSLens *lens);

typedef struct {
	GtkWidget *lensfun_make;
	GtkWidget *lensfun_model;
	GtkWidget *button;
	GtkWidget *checkbutton_enabled;
	GtkWidget *checkbutton_defish;
	RSLens *lens;
} SingleLensData;

typedef struct {
	/* The menu used to choose lens - either full or limited by search criteria */
	GtkWidget *LensMenu;
	/* The GtkTreeView */
	GtkTreeView *tree_view;
	SingleLensData *single_lens_data;
} lens_data;

static gint lf_lens_sort_by_model_func(gconstpointer *a, gconstpointer *b)
{
	lfLens *first = (lfLens *) ((GPtrArray *) a)->pdata;
	lfLens *second = (lfLens *) ((GPtrArray *) b)->pdata;

	return g_strcmp0(first->Model, second->Model);
}

const lfLens **lf_lens_sort_by_model(const lfLens *const *array)
{
	if (array == NULL)
		return NULL;

	gint x = 0;
	GPtrArray *temp = g_ptr_array_new();

	while (array[x])
		g_ptr_array_add(temp, (gpointer *) array[x++]);

	g_ptr_array_sort(temp, (GCompareFunc) lf_lens_sort_by_model_func);
	g_ptr_array_add (temp, NULL);
	return (const lfLens **) g_ptr_array_free (temp, FALSE);
}

static void lens_set (lens_data *data, const lfLens *lens)
{
	if (data->single_lens_data && lens)
	{
		/* Set Maker and Model to the selected RSLens */
		rs_lens_set_lensfun_make(data->single_lens_data->lens, lens->Maker);
		rs_lens_set_lensfun_model(data->single_lens_data->lens, lens->Model);
		rs_lens_set_lensfun_enabled(data->single_lens_data->lens, TRUE);

		gtk_label_set_text(GTK_LABEL(data->single_lens_data->lensfun_make), lens->Maker);
		gtk_label_set_text(GTK_LABEL(data->single_lens_data->lensfun_model), lens->Model);
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(data->single_lens_data->checkbutton_enabled), TRUE);

		gtk_widget_show(data->single_lens_data->lensfun_make);
		gtk_widget_show(data->single_lens_data->lensfun_model);
		gtk_widget_hide(data->single_lens_data->button);

		RSLensDb *lens_db = rs_lens_db_get_default();

		/* Force save of RSLensDb */
		rs_lens_db_save(lens_db);

		if (data)
			g_free(data);

		return;
	}

	GtkTreeSelection *selection = gtk_tree_view_get_selection(data->tree_view);
	GtkTreeModel *model = NULL;
	GtkTreeIter iter;

	gtk_tree_selection_get_selected(selection, &model, &iter);

	/* Set Maker and Model to the tree view */
	if (lens)
	{
		gtk_list_store_set (GTK_LIST_STORE(model), &iter,
					RS_LENS_DB_EDITOR_LENS_MAKE, lens->Maker,
					RS_LENS_DB_EDITOR_LENS_MODEL, lens->Model,
					RS_LENS_DB_EDITOR_ENABLED_ACTIVATABLE, TRUE,
					RS_LENS_DB_EDITOR_ENABLED, TRUE,
					RS_LENS_DB_EDITOR_DEFISH, FALSE,
					-1);
	}
	else
	{
		gtk_list_store_set (GTK_LIST_STORE(model), &iter,
					RS_LENS_DB_EDITOR_LENS_MAKE, "",
					RS_LENS_DB_EDITOR_LENS_MODEL, "",
					RS_LENS_DB_EDITOR_ENABLED_ACTIVATABLE, FALSE,
					RS_LENS_DB_EDITOR_ENABLED, FALSE,
					RS_LENS_DB_EDITOR_DEFISH, FALSE,
					-1);
	}

	RSLens *rs_lens = NULL;
	gtk_tree_model_get (model, &iter,
			    RS_LENS_DB_EDITOR_LENS, &rs_lens,
			    -1);

	/* Set Maker and Model to the selected RSLens */
	if (lens)
	{
		rs_lens_set_lensfun_make(rs_lens, lens->Maker);
		rs_lens_set_lensfun_model(rs_lens, lens->Model);
		rs_lens_set_lensfun_enabled(rs_lens, TRUE);
		rs_lens_set_lensfun_defish(rs_lens, FALSE);
	}
	else
	{
		rs_lens_set_lensfun_make(rs_lens, NULL);
		rs_lens_set_lensfun_model(rs_lens, NULL);
		rs_lens_set_lensfun_enabled(rs_lens, FALSE);
		rs_lens_set_lensfun_defish(rs_lens, FALSE);
	}

	RSLensDb *lens_db = rs_lens_db_get_default();

	/* Force save of RSLensDb */
	rs_lens_db_save(lens_db);
}


static void lens_menu_select (
	GtkMenuItem *menuitem, gpointer user_data)
{
	lens_data *data = (lens_data *)user_data;
	lens_set (data, (lfLens *)g_object_get_data(G_OBJECT(menuitem), "lfLens"));
}

static void lens_menu_deselect (
	GtkMenuItem *menuitem, gpointer user_data)
{
	lens_data *data = (lens_data *)user_data;
	lens_set (data, NULL);
}

int ptr_array_insert_sorted (
	GPtrArray *array, const void *item, GCompareFunc compare)
{
	int length = array->len;
	g_ptr_array_set_size (array, length + 1);
	const void **root = (const void **)array->pdata;

	int m = 0, l = 0, r = length - 1;

	// Skip trailing NULL, if any
	if (l <= r && !root [r])
		r--;
    
	while (l <= r)
	{
		m = (l + r) / 2;
		int cmp = compare (root [m], item);

		if (cmp == 0)
		{
			++m;
			goto done;
		}
		else if (cmp < 0)
			l = m + 1;
		else
			r = m - 1;
	}
	if (r == m)
		m++;

  done:
	memmove (root + m + 1, root + m, (length - m) * sizeof (void *));
	root [m] = item;
	return m;
}

int ptr_array_find_sorted (
	const GPtrArray *array, const void *item, GCompareFunc compare)
{
	int length = array->len;
	void **root = array->pdata;

	int l = 0, r = length - 1;
	int m = 0, cmp = 0;

	if (!length)
		return -1;

	// Skip trailing NULL, if any
	if (!root [r])
		r--;

	while (l <= r)
	{
		m = (l + r) / 2;
		cmp = compare (root [m], item);

		if (cmp == 0)
			return m;
		else if (cmp < 0)
			l = m + 1;
		else
			r = m - 1;
	}
    
	return -1;
}


void ptr_array_insert_index (
	GPtrArray *array, const void *item, int index)
{
	const void **root;
	int length = array->len;
	g_ptr_array_set_size (array, length + 1);
	root = (const void **)array->pdata;
	memmove (root + index + 1, root + index, (length - index) * sizeof (void *));
	root [index] = item;
}



static void lens_menu_fill (
	lens_data *data, const lfLens *const *lenslist_temp, const lfLens *const *full_lenslist_temp)
{
	unsigned i;
	GPtrArray *makers, *submenus, *allmakers, *allsubmenus;

	/* We want the two lists sorted by model */
	const lfLens **lenslist = lf_lens_sort_by_model(lenslist_temp);
	const lfLens **full_lenslist = lf_lens_sort_by_model(full_lenslist_temp);

	if (data->LensMenu)
	{
		/* This doesn't work, but will we be leaking GtkMenu's */
		//gtk_widget_destroy (data->LensMenu);
		data->LensMenu = NULL;
	}

	/* Count all existing lens makers and create a sorted list */
	makers = g_ptr_array_new ();
	submenus = g_ptr_array_new ();

	if (lenslist)
		for (i = 0; lenslist [i]; i++)
		{
			GtkWidget *submenu, *item;
			const char *m = lf_mlstr_get (lenslist [i]->Maker);
			int idx = ptr_array_find_sorted (makers, m, (GCompareFunc)g_utf8_collate);
			if (idx < 0)
			{
				/* No such maker yet, insert it into the array */
				idx = ptr_array_insert_sorted (makers, m, (GCompareFunc)g_utf8_collate);
				/* Create a submenu for lenses by this maker */
				submenu = gtk_menu_new ();
				ptr_array_insert_index (submenus, submenu, idx);
			}
			submenu = g_ptr_array_index (submenus, idx);
			/* Append current lens name to the submenu */
			item = gtk_menu_item_new_with_label (lf_mlstr_get (lenslist [i]->Model));
			gtk_widget_show (item);
			g_object_set_data(G_OBJECT(item), "lfLens", (void *)lenslist [i]);
			g_signal_connect(G_OBJECT(item), "activate",
					 G_CALLBACK(lens_menu_select), data);
			gtk_menu_shell_append (GTK_MENU_SHELL (submenu), item);
		}

	/* Count all existing lens makers and create a sorted list */
	allmakers = g_ptr_array_new ();
	allsubmenus = g_ptr_array_new ();

	for (i = 0; full_lenslist [i]; i++)
	{
		GtkWidget *allsubmenu, *allitem;
		const char *allm = lf_mlstr_get (full_lenslist [i]->Maker);
		int allidx = ptr_array_find_sorted (allmakers, allm, (GCompareFunc)g_utf8_collate);
		if (allidx < 0)
		{
			/* No such maker yet, insert it into the array */
			allidx = ptr_array_insert_sorted (allmakers, allm, (GCompareFunc)g_utf8_collate);
			/* Create a submenu for lenses by this maker */
			allsubmenu = gtk_menu_new ();
			ptr_array_insert_index (allsubmenus, allsubmenu, allidx);
		}
		allsubmenu = g_ptr_array_index (allsubmenus, allidx);
		/* Append current lens name to the submenu */
		allitem = gtk_menu_item_new_with_label (lf_mlstr_get (full_lenslist [i]->Model));
		gtk_widget_show (allitem);
		g_object_set_data(G_OBJECT(allitem), "lfLens", (void *)full_lenslist [i]);
		g_signal_connect(G_OBJECT(allitem), "activate",
				 G_CALLBACK(lens_menu_select), data);
		gtk_menu_shell_append (GTK_MENU_SHELL (allsubmenu), allitem);
	}

	data->LensMenu = gtk_menu_new ();
	for (i = 0; i < makers->len; i++)
	{
		GtkWidget *item = gtk_menu_item_new_with_label (g_ptr_array_index (makers, i));
		gtk_widget_show (item);
		gtk_menu_shell_append (GTK_MENU_SHELL (data->LensMenu), item);
		gtk_menu_item_set_submenu (
			GTK_MENU_ITEM (item), (GtkWidget *)g_ptr_array_index (submenus, i));
	}

	GtkWidget *allmenu = gtk_menu_new ();
	for (i = 0; i < allmakers->len; i++)
	{
		GtkWidget *allitem = gtk_menu_item_new_with_label (g_ptr_array_index (allmakers, i));
		gtk_widget_show (allitem);
		gtk_menu_shell_append (GTK_MENU_SHELL (allmenu), allitem);
		gtk_menu_item_set_submenu (
			GTK_MENU_ITEM (allitem), (GtkWidget *)g_ptr_array_index (allsubmenus, i));
	}

	GtkWidget *item = gtk_menu_item_new_with_label (_("All lenses"));
	gtk_widget_show (item);
	gtk_menu_shell_append (GTK_MENU_SHELL (data->LensMenu), item);
	gtk_menu_item_set_submenu (
		GTK_MENU_ITEM (item), allmenu);

	GtkWidget *deselect = gtk_menu_item_new_with_label (_("Deselect"));
	gtk_widget_show (deselect);
	gtk_menu_shell_append (GTK_MENU_SHELL (data->LensMenu), deselect);
	g_signal_connect(G_OBJECT(deselect), "activate",
			 G_CALLBACK(lens_menu_deselect), data);

	g_ptr_array_free (submenus, TRUE);
	g_ptr_array_free (makers, TRUE);

	g_ptr_array_free (allsubmenus, TRUE);
	g_ptr_array_free (allmakers, TRUE);
}

static void row_clicked (GtkTreeView *tree_view, GtkTreePath *path, GtkTreeViewColumn *column, gpointer user_data)
{
	struct lfDatabase *lensdb = NULL;
	const lfCamera *camera = NULL;
	const lfCamera **cameras = NULL;

	lens_data *data = g_malloc(sizeof(lens_data));
	data->tree_view = tree_view;
	data->single_lens_data = NULL;

	lensdb = lf_db_new ();
	lf_db_load (lensdb);

	GtkTreeSelection *selection = gtk_tree_view_get_selection(data->tree_view);
	GtkTreeModel *model = NULL;
	GtkTreeIter iter;

	gboolean ret = gtk_tree_selection_get_selected(selection, &model, &iter);
	if (ret == FALSE)
		return;

	RSLens *rs_lens = NULL;
	gtk_tree_model_get (model, &iter,
			    RS_LENS_DB_EDITOR_LENS, &rs_lens,
			    -1);

	gchar *camera_make;
	gchar *camera_model;
	gdouble min_focal;
	gdouble max_focal;

	g_assert(RS_IS_LENS(rs_lens));
	g_object_get(rs_lens,
		     "camera-make", &camera_make,
		     "camera-model", &camera_model,
		     "min-focal", &min_focal,
		     "max-focal", &max_focal,
		     NULL);

	gchar *lens_search;
	if (min_focal == max_focal)
	 lens_search = g_strdup_printf("%.0fmm", min_focal);
	else
	 lens_search = g_strdup_printf("%.0f-%.0f", min_focal, max_focal);

	cameras = lf_db_find_cameras(lensdb, camera_make, camera_model);
	if (cameras)
		camera = cameras[0];

	if (camera)
	{
		const lfLens **lenslist = lf_db_find_lenses_hd (
			lensdb, camera, NULL, lens_search, 0);
		const lfLens **full_lenslist = lf_db_find_lenses_hd (
			lensdb, camera, NULL, NULL, 0);

		if (!lenslist && !full_lenslist)
			return;

		lens_menu_fill (data, lenslist, full_lenslist);
		lf_free (lenslist);
	}
	else
	{
		const lfLens **lenslist = lf_db_find_lenses_hd (
			lensdb, NULL, NULL, lens_search, 0);
		const lfLens *const *full_lenslist = lf_db_get_lenses (lensdb);

		if (!lenslist)
			return;
		lens_menu_fill (data, lenslist, full_lenslist);
	}

	g_free(lens_search);

	gtk_menu_popup (GTK_MENU (data->LensMenu), NULL, NULL, NULL, NULL,
			0, gtk_get_current_event_time ());
}

static gboolean
view_on_button_pressed (GtkWidget *treeview, GdkEventButton *event, gpointer userdata)
{
	/* single click with the right mouse button? */
	if (event->type == GDK_BUTTON_PRESS  &&  event->button == 3)
	{
		GtkTreeSelection *selection;

		selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(treeview));

        /* Note: gtk_tree_selection_count_selected_rows() does not
		*   exist in gtk+-2.0, only in gtk+ >= v2.2 ! */
		GtkTreePath *path;

		/* Get tree path for row that was clicked */
		if (gtk_tree_view_get_path_at_pos(GTK_TREE_VIEW(treeview),
			(gint) event->x, 
			 (gint) event->y,
			  &path, NULL, NULL, NULL))
		{
			gtk_tree_selection_unselect_all(selection);
			gtk_tree_selection_select_path(selection, path);
			gtk_tree_path_free(path);
		}
		row_clicked(GTK_TREE_VIEW(treeview), path, NULL, userdata);
		return TRUE; /* we handled this */
	}
	return FALSE; /* we did not handle this */
}

static gboolean
view_popupmenu (GtkWidget *treeview, gpointer userdata)
{
	GtkTreeSelection *selection;
	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(treeview));
	GtkTreeModel *tree_model = gtk_tree_view_get_model(GTK_TREE_VIEW(treeview));
	GList* selected = gtk_tree_selection_get_selected_rows (selection, &tree_model);

	row_clicked(GTK_TREE_VIEW(treeview), selected->data, NULL, userdata);

	return TRUE; /* we handled this */
}

static void
toggle_clicked (GtkCellRendererToggle *cell_renderer_toggle, const gchar *path, gpointer user_data)
{
	GtkTreeIter iter;
	gboolean enabled;
	GtkTreeView *tree_view = GTK_TREE_VIEW(user_data);
	GtkTreeModel *tree_model = gtk_tree_view_get_model(tree_view);
	GtkTreePath* tree_path = gtk_tree_path_new_from_string(path);

	gtk_tree_model_get_iter(GTK_TREE_MODEL (tree_model), &iter, tree_path);
	gtk_tree_model_get(GTK_TREE_MODEL (tree_model), &iter, RS_LENS_DB_EDITOR_ENABLED, &enabled, -1);

	if (enabled)
		gtk_list_store_set(GTK_LIST_STORE (tree_model), &iter, RS_LENS_DB_EDITOR_ENABLED, FALSE, -1);
	else
		gtk_list_store_set(GTK_LIST_STORE (tree_model), &iter, RS_LENS_DB_EDITOR_ENABLED, TRUE, -1);

	RSLens *rs_lens = NULL;
	gtk_tree_model_get (tree_model, &iter,
			    RS_LENS_DB_EDITOR_LENS, &rs_lens,
			    -1);

	/* Set enabled/disabled to the selected RSLens */
	rs_lens_set_lensfun_enabled(rs_lens, !enabled);

	RSLensDb *lens_db = rs_lens_db_get_default();

	/* Force save of RSLensDb */
	rs_lens_db_save(lens_db);
}

static void
defish_clicked (GtkCellRendererToggle *cell_renderer_toggle, const gchar *path, gpointer user_data)
{
	GtkTreeIter iter;
	gboolean enabled;
	GtkTreeView *tree_view = GTK_TREE_VIEW(user_data);
	GtkTreeModel *tree_model = gtk_tree_view_get_model(tree_view);
	GtkTreePath* tree_path = gtk_tree_path_new_from_string(path);

	gtk_tree_model_get_iter(GTK_TREE_MODEL (tree_model), &iter, tree_path);
	gtk_tree_model_get(GTK_TREE_MODEL (tree_model), &iter, RS_LENS_DB_EDITOR_DEFISH, &enabled, -1);

	gtk_list_store_set(GTK_LIST_STORE (tree_model), &iter, RS_LENS_DB_EDITOR_DEFISH, !enabled, -1);

	RSLens *rs_lens = NULL;
	gtk_tree_model_get (tree_model, &iter,
			    RS_LENS_DB_EDITOR_LENS, &rs_lens,
			    -1);

	/* Set enabled/disabled to the selected RSLens */
	rs_lens_set_lensfun_defish(rs_lens, !enabled);

	RSLensDb *lens_db = rs_lens_db_get_default();

	/* Force save of RSLensDb */
	rs_lens_db_save(lens_db);
}

static void
update_lensfun(GtkButton *button, gpointer user_data)
{
	GtkWidget *window = GTK_WIDGET(user_data);
	GdkCursor* cursor = gdk_cursor_new(GDK_WATCH);
	gdk_window_set_cursor(window->window, cursor);
	GTK_CATCHUP();
	gchar *error = rs_lens_db_editor_update_lensfun();
	gdk_window_set_cursor(window->window, NULL);
	GtkWidget *dialog = NULL;

	if (error)
		dialog = gui_dialog_make_from_text(GTK_STOCK_DIALOG_ERROR, _("Error updating lensfun database"), error);
	else
		dialog = gui_dialog_make_from_text(GTK_STOCK_DIALOG_INFO, _("LensFun database updated"), error);

	gtk_dialog_add_button (GTK_DIALOG (dialog), GTK_STOCK_CLOSE, GTK_RESPONSE_ACCEPT);
	gtk_widget_show_all(dialog);
	gtk_dialog_run(GTK_DIALOG(dialog));
	gtk_widget_destroy(dialog);
	g_free(error);

	rs_lens_db_editor();
}

static gint
rs_lens_db_editor_sort(GtkTreeModel *model, GtkTreeIter *a, GtkTreeIter *b, gpointer user_data)
{
	gchar *a_lensfun_model;
	gchar *a_camera_model;
	gchar *a_identifier;

	gtk_tree_model_get(model, a, 
				RS_LENS_DB_EDITOR_IDENTIFIER, &a_identifier,
				RS_LENS_DB_EDITOR_LENS_MODEL, &a_lensfun_model,
				RS_LENS_DB_EDITOR_CAMERA_MODEL, &a_camera_model,
				-1);

	gchar *b_lensfun_model;
	gchar *b_camera_model;
	gchar *b_identifier;

	gtk_tree_model_get(model, b, 
				RS_LENS_DB_EDITOR_IDENTIFIER, &b_identifier,
				RS_LENS_DB_EDITOR_LENS_MODEL, &b_lensfun_model,
				RS_LENS_DB_EDITOR_CAMERA_MODEL, &b_camera_model,
				-1);

	gint ret = 0;

	ret = g_strcmp0(a_lensfun_model, b_lensfun_model);
	if (ret != 0)
		return ret;

	ret = g_strcmp0(a_camera_model, b_camera_model);
	if (ret != 0)
		return ret;

	ret = g_strcmp0(a_identifier, b_identifier);
	if (ret != 0)
		return ret;

	return ret;
}

void
rs_lens_db_editor(void) 
{
	GtkTreeModel *tree_model = GTK_TREE_MODEL(gtk_list_store_new(11, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_BOOLEAN, G_TYPE_BOOLEAN, G_TYPE_BOOLEAN, G_TYPE_OBJECT));

	RSLensDb *lens_db = rs_lens_db_get_default();
	fill_model(lens_db, tree_model);

	GtkWidget *editor = gtk_dialog_new();
	gtk_window_set_title(GTK_WINDOW(editor), _("Rawstudio Lens Library"));
	gtk_dialog_set_has_separator (GTK_DIALOG(editor), FALSE);
	g_signal_connect_swapped(editor, "delete_event",
				 G_CALLBACK (gtk_widget_destroy), editor);
	g_signal_connect_swapped(editor, "response",
				 G_CALLBACK (gtk_widget_destroy), editor);

	GtkWidget *frame = gtk_frame_new("");

        GtkWidget *scroller = gtk_scrolled_window_new (NULL, NULL);
        gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scroller),
					GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
        GtkWidget *view = gtk_tree_view_new_with_model(tree_model);

        gtk_tree_view_set_reorderable(GTK_TREE_VIEW(view), FALSE);
        gtk_container_add (GTK_CONTAINER (scroller), view);

        GtkCellRenderer *renderer_lens_make = gtk_cell_renderer_text_new();
        GtkCellRenderer *renderer_lens_model = gtk_cell_renderer_text_new();
        GtkCellRenderer *renderer_focal = gtk_cell_renderer_text_new();
        GtkCellRenderer *renderer_aperture = gtk_cell_renderer_text_new();
        GtkCellRenderer *renderer_camera_make = gtk_cell_renderer_text_new();
        GtkCellRenderer *renderer_camera_model = gtk_cell_renderer_text_new();
        GtkCellRenderer *renderer_enabled = gtk_cell_renderer_toggle_new();
        GtkCellRenderer *renderer_defish = gtk_cell_renderer_toggle_new();

        GtkTreeViewColumn *column_lens_make = gtk_tree_view_column_new_with_attributes (_("Lens make"),
								  renderer_lens_make,
								  "text", RS_LENS_DB_EDITOR_LENS_MAKE,
										   NULL);
        GtkTreeViewColumn *column_lens_model = gtk_tree_view_column_new_with_attributes (_("Lens model"),
								  renderer_lens_model,
								  "text", RS_LENS_DB_EDITOR_LENS_MODEL,
										   NULL);
        GtkTreeViewColumn *column_focal = gtk_tree_view_column_new_with_attributes (_("Focal"),
								  renderer_focal,
								  "text", RS_LENS_DB_EDITOR_HUMAN_FOCAL,
										   NULL);
        GtkTreeViewColumn *column_aperture = gtk_tree_view_column_new_with_attributes (_("Aperture"),
								  renderer_aperture,
								  "text", RS_LENS_DB_EDITOR_HUMAN_APERTURE,
										   NULL);
        GtkTreeViewColumn *column_camera_make = gtk_tree_view_column_new_with_attributes (_("Camera make"),
								  renderer_camera_make,
								  "text", RS_LENS_DB_EDITOR_CAMERA_MAKE,
										   NULL);
        GtkTreeViewColumn *column_camera_model = gtk_tree_view_column_new_with_attributes (_("Camera model"),
								  renderer_camera_model,
								  "text", RS_LENS_DB_EDITOR_CAMERA_MODEL,
										   NULL);
        GtkTreeViewColumn *column_enabled = gtk_tree_view_column_new_with_attributes (_("Enabled"),
								  renderer_enabled,
								  "active", RS_LENS_DB_EDITOR_ENABLED,
								  "activatable", RS_LENS_DB_EDITOR_ENABLED_ACTIVATABLE,
										   NULL);
        GtkTreeViewColumn *column_defish = gtk_tree_view_column_new_with_attributes (_("Defish"),
								  renderer_defish,
								  "active", RS_LENS_DB_EDITOR_DEFISH,
								  "activatable", RS_LENS_DB_EDITOR_ENABLED_ACTIVATABLE,
										   NULL);

	gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(tree_model), RS_LENS_DB_EDITOR_CAMERA_MODEL, GTK_SORT_ASCENDING);
	gtk_tree_sortable_set_sort_func(GTK_TREE_SORTABLE(tree_model), RS_LENS_DB_EDITOR_CAMERA_MODEL, rs_lens_db_editor_sort, NULL, NULL);

	g_signal_connect(G_OBJECT(view), "row-activated",
			 G_CALLBACK(row_clicked), NULL);

        g_signal_connect (renderer_enabled, "toggled",
			  G_CALLBACK (toggle_clicked), view);
			g_signal_connect (renderer_defish, "toggled", G_CALLBACK (defish_clicked), view);
		g_signal_connect(G_OBJECT(view), "button-press-event", G_CALLBACK(view_on_button_pressed), NULL);
		g_signal_connect(view, "popup-menu", (GCallback) view_popupmenu, NULL);

        gtk_tree_view_append_column (GTK_TREE_VIEW (view), column_lens_make);
        gtk_tree_view_append_column (GTK_TREE_VIEW (view), column_lens_model);
        gtk_tree_view_append_column (GTK_TREE_VIEW (view), column_focal);
        gtk_tree_view_append_column (GTK_TREE_VIEW (view), column_aperture);
        gtk_tree_view_append_column (GTK_TREE_VIEW (view), column_camera_make);
        gtk_tree_view_append_column (GTK_TREE_VIEW (view), column_camera_model);
        gtk_tree_view_append_column (GTK_TREE_VIEW (view), column_enabled);
        gtk_tree_view_append_column (GTK_TREE_VIEW (view), column_defish);

        gtk_tree_view_set_headers_visible(GTK_TREE_VIEW (view), TRUE);

        gtk_container_add (GTK_CONTAINER (frame), scroller);

	gtk_window_resize(GTK_WINDOW(editor), 400, 400);

        gtk_container_set_border_width (GTK_CONTAINER (frame), 6);
        gtk_container_set_border_width (GTK_CONTAINER (scroller), 6);

        gtk_box_pack_start (GTK_BOX (GTK_DIALOG(editor)->vbox), frame, TRUE, TRUE, 0);

	GtkWidget *button_update_lensfun = gtk_button_new_with_label(_("Update lensfun database"));
	g_signal_connect(button_update_lensfun, "clicked", G_CALLBACK(update_lensfun), editor);
	gtk_dialog_add_action_widget (GTK_DIALOG (editor), button_update_lensfun, GTK_RESPONSE_NONE);

        GtkWidget *button_close = gtk_button_new_from_stock(GTK_STOCK_CLOSE);
        gtk_dialog_add_action_widget (GTK_DIALOG (editor), button_close, GTK_RESPONSE_CLOSE);

        gtk_widget_show_all(GTK_WIDGET(editor));

}

static void
fill_model(RSLensDb *lens_db, GtkTreeModel *tree_model)
{
	GList *list = rs_lens_db_get_lenses(lens_db);

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

		const gchar *human_focal = rs_human_focal(min_focal, max_focal);
		const gchar *human_aperture = rs_human_aperture(max_aperture);

		GtkTreeIter iter;

		gboolean enabled_activatable = FALSE;
		if (lensfun_make && lensfun_model)
			enabled_activatable = TRUE;

		gtk_list_store_append (GTK_LIST_STORE(tree_model), &iter);
		gtk_list_store_set (GTK_LIST_STORE(tree_model), &iter,
				    RS_LENS_DB_EDITOR_IDENTIFIER, identifier,
				    RS_LENS_DB_EDITOR_HUMAN_FOCAL, human_focal,
				    RS_LENS_DB_EDITOR_HUMAN_APERTURE, human_aperture,
				    RS_LENS_DB_EDITOR_LENS_MAKE, lensfun_make,
				    RS_LENS_DB_EDITOR_LENS_MODEL, lensfun_model,
				    RS_LENS_DB_EDITOR_CAMERA_MAKE, camera_make,
				    RS_LENS_DB_EDITOR_CAMERA_MODEL, camera_model,
				    RS_LENS_DB_EDITOR_ENABLED, enabled,
				    RS_LENS_DB_EDITOR_DEFISH, defish,
				    RS_LENS_DB_EDITOR_ENABLED_ACTIVATABLE, enabled_activatable,
				    RS_LENS_DB_EDITOR_LENS, lens,
				    -1);
		list = g_list_next (list);
	}
}

static size_t
write_callback(void *ptr, size_t size, size_t nmemb, void *userp)
{
	GString *string = (GString *) userp;
	g_string_append_len(string, (char *) ptr, size * nmemb);
	return (size * nmemb);
}

static gchar *
rs_lens_db_editor_update_lensfun(void)
{
	const gchar *baseurl = "http://svn.berlios.de/svnroot/repos/lensfun/trunk/data/db/";
	const gchar *target = g_strdup_printf("%s/.%u-rawstudio_lensfun/", g_get_tmp_dir(), g_random_int());

	g_mkdir(target, 0700);
	if (!g_file_test(target, G_FILE_TEST_IS_DIR))
		return g_strdup(_("Could not create temporary directory."));

	CURL *curl = curl_easy_init();
	GString *xml = g_string_new(NULL);
	gchar *filename = NULL, *url = NULL, *file = NULL;
	FILE *fp = NULL;
	CURLcode result;

	curl_easy_setopt(curl, CURLOPT_URL, baseurl);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, xml);
	result = curl_easy_perform(curl);
	if (result != 0)
		return g_strdup_printf(_("Could not fetch list of files from %s."), baseurl);

	htmlDocPtr doc = htmlReadMemory(xml->str, xml->len, NULL, NULL, 0);
        htmlNodePtr cur, child;

	cur = xmlDocGetRootElement(doc);
	cur = cur->xmlChildrenNode;
	cur = cur->next;
	cur = cur->xmlChildrenNode;
	cur = cur->next;
	cur = cur->next;
	cur = cur->next;
	cur = cur->xmlChildrenNode;
	cur = cur->next;
	cur = cur->next;
	while (cur)
	{
		child = cur->xmlChildrenNode;
		filename =  (gchar *) xmlNodeListGetString(doc, child->xmlChildrenNode, 1);

		url = g_strdup_printf("%s%s", baseurl, filename);
		file = g_build_filename(target, filename, NULL);

		fp = fopen(file, "w");

		curl_easy_reset(curl);
		curl_easy_setopt(curl, CURLOPT_URL, url);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
		result = curl_easy_perform(curl);

		fclose(fp);

		g_free(filename);
		g_free(url);
		g_free(file);

		cur = cur->next;
		cur = cur->next;

		if (result != 0)
			return g_strdup_printf(_("Could not fetch file from %s or write it to %s."), url, file);
	}

	const gchar *datadir = g_build_filename(g_get_user_data_dir(), "lensfun", NULL);

	if (!g_file_test(datadir, G_FILE_TEST_IS_DIR))
	{
		g_mkdir(datadir, 0700);
		if (!g_file_test(datadir, G_FILE_TEST_IS_DIR))
			return g_strdup_printf(_("Could not create datadir for lensfun - %s"), datadir);
	}

	GDir *dir = g_dir_open(target, 0, NULL);
	const gchar *fn = NULL;

	while ((fn = g_dir_read_name (dir)))
	{
		GPatternSpec *ps = g_pattern_spec_new ("*.xml");
		if (g_pattern_match (ps, strlen(fn), fn, NULL))
		{
			gchar *ffn = g_build_filename (target, fn, NULL);
			GFile *source = g_file_new_for_path(ffn);
			GFile *destination = g_file_new_for_path(g_build_filename(datadir, fn, NULL));

			if (!g_file_copy(source, destination, G_FILE_COPY_OVERWRITE, NULL, NULL, NULL, NULL))
				return g_strdup_printf(_("Error copying file %s to %s\n"), g_file_get_parse_name(source), g_file_get_parse_name(destination));

			g_free(ffn);
		}
		g_free(ps);
	}

	/* FIXME: remove 'target' */

	g_dir_close(dir);

	return NULL;
}

static void set_lens (GtkButton *button, SingleLensData *single_lens_data)
{
	struct lfDatabase *lensdb = NULL;
	const lfCamera *camera = NULL;
	const lfCamera **cameras = NULL;

	lens_data *data = g_malloc(sizeof(lens_data));
	data->single_lens_data = single_lens_data;

	lensdb = lf_db_new ();
	lf_db_load (lensdb);

	RSLens *rs_lens = RS_LENS(single_lens_data->lens);

	gchar *camera_make;
	gchar *camera_model;
	gdouble min_focal;
	gdouble max_focal;

	g_assert(RS_IS_LENS(rs_lens));
	g_object_get(rs_lens,
		     "camera-make", &camera_make,
		     "camera-model", &camera_model,
		     "min-focal", &min_focal,
		     "max-focal", &max_focal,
		     NULL);

	gchar *lens_search;
	if (min_focal == max_focal)
	 lens_search = g_strdup_printf("%.0fmm", min_focal);
	else
	 lens_search = g_strdup_printf("%.0f-%.0f", min_focal, max_focal);

	cameras = lf_db_find_cameras(lensdb, camera_make, camera_model);
	if (cameras)
		camera = cameras[0];

	if (camera)
	{
		const lfLens **lenslist = lf_db_find_lenses_hd (
			lensdb, camera, NULL, lens_search, 0);
		const lfLens **full_lenslist = lf_db_find_lenses_hd (
			lensdb, camera, NULL, NULL, 0);

		if (!lenslist && !full_lenslist)
			return;

		lens_menu_fill (data, lenslist, full_lenslist);
		lf_free (lenslist);
	}
	else
	{
		const lfLens **lenslist = lf_db_find_lenses_hd (
			lensdb, NULL, NULL, lens_search, 0);
		const lfLens *const *full_lenslist = lf_db_get_lenses (lensdb);

		if (!lenslist)
			return;
		lens_menu_fill (data, lenslist, full_lenslist);
	}

	g_free(lens_search);

	gtk_menu_popup (GTK_MENU (data->LensMenu), NULL, NULL, NULL, NULL,
			0, gtk_get_current_event_time ());
}

static void
enable_lens(GtkCheckButton *checkbutton, gpointer user_data)
{
	RSLens *lens = user_data;
	rs_lens_set_lensfun_enabled(lens, gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(checkbutton)));
}

static void
defish_lens(GtkCheckButton *checkbutton, gpointer user_data)
{
	RSLens *lens = user_data;
	rs_lens_set_lensfun_defish(lens, gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(checkbutton)));
}

static void
open_full_lens_editor(GtkCheckButton *checkbutton, gpointer user_data)
{
	rs_lens_db_editor();
}

static gchar* 
boldify(const gchar* text)
{
	return g_strconcat("<b>", text, "</b>", NULL);
}

GtkDialog *
rs_lens_db_editor_single_lens(RSLens *lens)
{
	gchar *identifier;
	gchar *lensfun_make;
	gchar *lensfun_model;
	gdouble min_focal, max_focal, min_aperture, max_aperture;
	gchar *camera_make;
	gchar *camera_model;
	gboolean enabled;
	gboolean defish;

	g_return_val_if_fail(RS_IS_LENS(lens), NULL);

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
	
	GtkWidget *editor = gtk_dialog_new();
	gtk_window_set_title(GTK_WINDOW(editor), _("Rawstudio Lens Editor"));
	gtk_dialog_set_has_separator (GTK_DIALOG(editor), FALSE);
	g_signal_connect_swapped(editor, "delete_event",
				 G_CALLBACK (gtk_widget_destroy), editor);
	g_signal_connect_swapped(editor, "response",
				 G_CALLBACK (gtk_widget_destroy), editor);

	GtkWidget *frame = gtk_frame_new("");
	GtkWidget *table = gtk_table_new(2, 10, FALSE);

	GtkWidget *label1 = gtk_label_new("");
	gtk_label_set_markup(GTK_LABEL(label1), boldify(_("Lens Make")));
	gtk_misc_set_alignment(GTK_MISC(label1), 0, 0);

	GtkWidget *label2 = gtk_label_new("");
	gtk_label_set_markup(GTK_LABEL(label2), boldify(_("Lens Model")));
	gtk_misc_set_alignment(GTK_MISC(label2), 0, 0);

	GtkWidget *label3 = gtk_label_new("");
	gtk_label_set_markup(GTK_LABEL(label3), boldify(_("Focal Length")));
	gtk_misc_set_alignment(GTK_MISC(label3), 0, 0);

	GtkWidget *label4 = gtk_label_new("");
	gtk_label_set_markup(GTK_LABEL(label4), boldify(_("Aperture")));
	gtk_misc_set_alignment(GTK_MISC(label4), 0, 0);

	GtkWidget *label5 = gtk_label_new("");
	gtk_label_set_markup(GTK_LABEL(label5), boldify(_("Camera Make")));
	gtk_misc_set_alignment(GTK_MISC(label5), 0, 0);

	GtkWidget *label6 = gtk_label_new("");
	gtk_label_set_markup(GTK_LABEL(label6), boldify(_("Camera Model")));
	gtk_misc_set_alignment(GTK_MISC(label6), 0, 0);

//	GtkWidget *label7 = gtk_label_new("");
//	gtk_label_set_markup(GTK_LABEL(label7), "<b>Enabled</b>");
//	gtk_misc_set_alignment(GTK_MISC(label7), 0, 0);

	gtk_table_attach_defaults(GTK_TABLE(table), label5, 0,1,0,1);
	gtk_table_attach_defaults(GTK_TABLE(table), label6, 0,1,1,2);
	gtk_table_attach_defaults(GTK_TABLE(table), label3, 0,1,2,3);
	gtk_table_attach_defaults(GTK_TABLE(table), label4, 0,1,3,4);
//	gtk_table_attach_defaults(GTK_TABLE(table), label7, 0,1,4,5);
	gtk_table_attach_defaults(GTK_TABLE(table), label1, 0,1,6,7);
	gtk_table_attach_defaults(GTK_TABLE(table), label2, 0,1,7,8);

	GtkWidget *label_lensfun_make = gtk_label_new(lensfun_make);
	GtkWidget *label_lensfun_model = gtk_label_new(lensfun_model);
	GtkWidget *label_focal;
	if (min_focal == max_focal)
		label_focal = gtk_label_new(g_strdup_printf("%.0fmm", min_focal));
	else
		label_focal = gtk_label_new(g_strdup_printf("%.0f-%.0fmm", min_focal, max_focal));
	GtkWidget *label_aperture = gtk_label_new(g_strdup_printf("F/%.1f", max_aperture));
	GtkWidget *label_camera_make = gtk_label_new(camera_make);
	GtkWidget *label_camera_model = gtk_label_new(camera_model);
	GtkWidget *checkbutton_enabled = gtk_check_button_new_with_label(_("Enable this lens"));
	GtkWidget *checkbutton_defish = gtk_check_button_new_with_label(_("Enable Defish"));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(checkbutton_enabled), rs_lens_get_lensfun_enabled(lens));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(checkbutton_defish), rs_lens_get_lensfun_defish(lens));

	GtkWidget *button_set_lens = gtk_button_new_with_label(_("Set lens"));

	GtkWidget *sep1 = gtk_hseparator_new();
	GtkWidget *sep2 = gtk_hseparator_new();

	SingleLensData *single_lens_data = g_malloc(sizeof(SingleLensData));
	single_lens_data->lensfun_make = label_lensfun_make;
	single_lens_data->lensfun_model = label_lensfun_model;
	single_lens_data->lens = lens;
	single_lens_data->button = button_set_lens;
	single_lens_data->checkbutton_enabled = checkbutton_enabled;
	single_lens_data->checkbutton_defish = checkbutton_defish;

	g_signal_connect(button_set_lens, "clicked", G_CALLBACK(set_lens), single_lens_data);

	gtk_misc_set_alignment(GTK_MISC(label_lensfun_make), 1, 0);
	gtk_misc_set_alignment(GTK_MISC(label_lensfun_model), 1, 0);
	gtk_misc_set_alignment(GTK_MISC(label_focal), 1, 0);
	gtk_misc_set_alignment(GTK_MISC(label_aperture), 1, 0);
	gtk_misc_set_alignment(GTK_MISC(label_camera_make), 1, 0);
	gtk_misc_set_alignment(GTK_MISC(label_camera_model), 1, 0);
//	gtk_button_set_alignment(GTK_BUTTON(checkbutton_enabled), 1, 0);

	gtk_table_attach_defaults(GTK_TABLE(table), label_camera_make, 1,2,0,1);
	gtk_table_attach_defaults(GTK_TABLE(table), label_camera_model, 1,2,1,2);
	gtk_table_attach_defaults(GTK_TABLE(table), label_focal, 1,2,2,3);
	gtk_table_attach_defaults(GTK_TABLE(table), label_aperture, 1,2,3,4);
	gtk_table_attach_defaults(GTK_TABLE(table), sep1, 0,2,5,6);
	gtk_table_attach_defaults(GTK_TABLE(table), label_lensfun_make, 1,2,6,7);
	gtk_table_attach_defaults(GTK_TABLE(table), label_lensfun_model, 1,2,7,8);
	gtk_table_attach_defaults(GTK_TABLE(table), button_set_lens, 1,2,6,8);
	gtk_table_attach_defaults(GTK_TABLE(table), sep2, 0,2,8,9);
	gtk_table_attach_defaults(GTK_TABLE(table), checkbutton_enabled, 0,1,9,10);
	gtk_table_attach_defaults(GTK_TABLE(table), checkbutton_defish, 1,2,9,10);

	/* Set spacing around separator in table */
	gtk_table_set_row_spacing(GTK_TABLE(table), 4, 10);
	gtk_table_set_row_spacing(GTK_TABLE(table), 5, 10);
	gtk_table_set_row_spacing(GTK_TABLE(table), 7, 10);
	gtk_table_set_row_spacing(GTK_TABLE(table), 8, 10);

	gtk_window_resize(GTK_WINDOW(editor), 300, 1);

        gtk_container_set_border_width (GTK_CONTAINER (frame), 6);
        gtk_container_set_border_width (GTK_CONTAINER (table), 6);

        gtk_box_pack_start (GTK_BOX (GTK_DIALOG(editor)->vbox), frame, TRUE, TRUE, 0);
	gtk_container_add (GTK_CONTAINER (frame), table);

	g_signal_connect(checkbutton_enabled, "toggled", G_CALLBACK(enable_lens), lens);
	g_signal_connect(checkbutton_defish, "toggled", G_CALLBACK(defish_lens), lens);

	/* FIXME: Put lensfun update button in editor - for this to work, we cannot close the window when updating */
//	GtkWidget *button_update_lensfun = gtk_button_new_with_label(_("Update lensfun database"));
//	g_signal_connect(button_update_lensfun, "clicked", G_CALLBACK(update_lensfun), NULL);
//	gtk_dialog_add_action_widget (GTK_DIALOG (editor), button_update_lensfun, GTK_RESPONSE_NONE);

	GtkWidget *button_lens_library = gtk_button_new_with_label(_("Lens Library"));
	g_signal_connect(button_lens_library, "clicked", G_CALLBACK(open_full_lens_editor), lens);
	gtk_dialog_add_action_widget (GTK_DIALOG (editor), button_lens_library, GTK_RESPONSE_CLOSE);

        GtkWidget *button_close = gtk_button_new_from_stock(GTK_STOCK_CLOSE);
        gtk_dialog_add_action_widget (GTK_DIALOG (editor), button_close, GTK_RESPONSE_CLOSE);

        gtk_widget_show_all(GTK_WIDGET(editor));
	if (!rs_lens_get_lensfun_model(lens) || !rs_lens_get_lensfun_make(lens))
	{
		gtk_widget_hide(label_lensfun_make);
		gtk_widget_hide(label_lensfun_model);
		gtk_widget_show(button_set_lens);
	}
	else
	{
		gtk_widget_show(label_lensfun_make);
		gtk_widget_show(label_lensfun_model);
		gtk_widget_hide(button_set_lens);
	}
	return GTK_DIALOG(editor);
}
