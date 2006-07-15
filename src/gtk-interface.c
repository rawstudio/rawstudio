/*
 * Copyright (C) 2006 Anders Brander <anders@brander.dk> and 
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

#include <glib.h>
#include <gtk/gtk.h>
#include <glib/gstdio.h>
#include "color.h"
#include "matrix.h"
#include "rs-batch.h"
#include "rawstudio.h"
#include "gtk-helper.h"
#include "gtk-interface.h"
#include "drawingarea.h"
#include "toolbox.h"
#include "conf_interface.h"
#include "rs-cache.h"
#include "rs-image.h"
#include "gettext.h"
#include <config.h>
#include <string.h>
#include <unistd.h>

struct nextprev_helper {
	const gchar *filename;
	GtkTreePath *previous;
	GtkTreePath *next;
};

struct count_helper {
	GtkWidget *label1;
	GtkWidget *label2;
	GtkWidget *label3;
	GtkWidget *label4;
	GtkWidget *label5;
	GtkWidget *label6;
};

GtkStatusbar *statusbar;
static gboolean fullscreen = FALSE;
static GtkWidget *iconview[6];
static GtkWidget *current_iconview = NULL;
static guint priorities[6];
static guint current_priority = PRIO_ALL;
static GtkTreeIter current_iter;

void gui_status_push(const char *text);
gint fill_model_compare_func (GtkTreeModel *model, GtkTreeIter *tia,
	GtkTreeIter *tib, gpointer userdata);
void fill_model(GtkListStore *store, const char *path);
void icon_activated_helper(GtkIconView *iconview, GtkTreePath *path, gpointer user_data);
void icon_activated(GtkIconView *iconview, RS_BLOB *rs);
GtkWidget *make_iconbox(RS_BLOB *rs, GtkListStore *store);
void gui_menu_open_callback(gpointer callback_data, guint callback_action, GtkWidget *widget);
void gui_menu_reload_callback(gpointer callback_data, guint callback_action, GtkWidget *widget);
void gui_menu_purge_d_callback(gpointer callback_data, guint callback_action, GtkWidget *widget);
void gui_preview_bg_color_changed(GtkColorButton *widget, RS_BLOB *rs);
gboolean gui_fullscreen_callback(GtkWidget *widget, GdkEventWindowState *event, GtkWidget *iconbox);
void gui_menu_setprio_callback(gpointer callback_data, guint callback_action, GtkWidget *widget);
void gui_menu_widget_visible_callback(gpointer callback_data, guint callback_action, GtkWidget *widget);
void gui_menu_fullscreen_callback(gpointer callback_data, guint callback_action, GtkWidget *widget);
gboolean gui_menu_prevnext_helper(GtkTreeModel *model, GtkTreePath *path, GtkTreeIter *iter, gpointer user_data);
void gui_menu_prevnext_callback(gpointer callback_data, guint callback_action, GtkWidget *widget);
void local_cache_toggled(GtkToggleButton *togglebutton, gpointer user_data);
void gui_menu_preference_callback(gpointer callback_data, guint callback_action, GtkWidget *widget);
void gui_about();
void gui_menu_auto_wb_callback(gpointer callback_data, guint callback_action, GtkWidget *widget);
void gui_menu_cam_wb_callback(gpointer callback_data, guint callback_action, GtkWidget *widget);
void gui_save_file_callback(gpointer callback_data, guint callback_action, GtkWidget *widget);
void gui_reset_current_settings_callback(RS_BLOB *rs);
void gui_menu_quit(gpointer callback_data, guint callback_action, GtkWidget *widget);
void gui_menu_show_exposure_mask_callback(gpointer callback_data, guint callback_action, GtkWidget *widget);
void gui_menu_paste_callback(gpointer callback_data, guint callback_action, GtkWidget *widget);
void gui_menu_copy_callback(gpointer callback_data, guint callback_action, GtkWidget *widget);
GtkWidget *gui_make_menubar(RS_BLOB *rs, GtkWidget *window, GtkListStore *store, GtkWidget *iconbox, GtkWidget *toolbox);
GtkWidget *gui_window_make(RS_BLOB *rs);
GtkWidget *gui_dialog_make_from_text(const gchar *stock_id, gchar *primary_text, gchar *secondary_text);
GtkWidget *gui_dialog_make_from_widget(const gchar *stock_id, gchar *primary_text, GtkWidget *widget);


void
gui_status_push(const char *text)
{
	gtk_statusbar_pop(statusbar, gtk_statusbar_get_context_id(statusbar, "generic"));
	gtk_statusbar_push(statusbar, gtk_statusbar_get_context_id(statusbar, "generic"), text);
	return;
}

gboolean
update_preview_callback(GtkAdjustment *do_not_use_this, RS_BLOB *rs)
{
	if (rs->photo)
	{
		rs_settings_to_rs_settings_double(rs->settings[rs->current_setting], rs->photo->settings[rs->photo->current_setting]);
		update_preview(rs);
	}
	return(FALSE);
}

void update_histogram(RS_BLOB *rs)
{
	guint c,i,x,y,rowstride;
	guint max = 0;
	guint factor = 0;
	guint hist[3][256];
	gint height;
	GdkPixbuf *pixbuf;
	guchar *pixels, *p;

	pixbuf = gtk_image_get_pixbuf(rs->histogram_image);
	rowstride = gdk_pixbuf_get_rowstride (pixbuf);
	pixels = gdk_pixbuf_get_pixels (pixbuf);
	height = gdk_pixbuf_get_height (pixbuf);

	/* sets all the pixels black */
	memset(pixels, 0x00, rowstride*height);

	/* draw a grid with 7 bars with 32 pixels space */
	p = pixels;
	for(y = 0; y < height; y++)
	{
		for(x = 0; x < 256 * 3; x +=93)
		{
			p[x++] = 100;
			p[x++] = 100;
			p[x++] = 100;
		}
		p+=rowstride;
	}

	/* find the max value */
	for (c = 0; c < 3; c++)
	{
		for (i = 0; i < 256; i++)
		{
			_MAX(rs->histogram_table[c][i], max);
		}
	}

	/* find the factor to scale the histogram */
	factor = (max+height)/height;

	/* calculate the histogram values */
	for (c = 0; c < 3; c++)
	{
		for (i = 0; i < 256; i++)
		{
			hist[c][i] = rs->histogram_table[c][i]/factor;
		}
	}

	/* draw the histogram */
	for (x = 0; x < 256; x++)
	{
		for (c = 0; c < 3; c++)
		{
			for (y = 0; y < hist[c][x]; y++)
			{				
				/* address the pixel - the (rs->hist_h-1)-y is to draw it from the bottom */
				p = pixels + ((height-1)-y) * rowstride + x * 3;
				p[c] = 0xFF;
			}
		}
	}
	gtk_image_set_from_pixbuf((GtkImage *) rs->histogram_image, pixbuf);

}

gint
fill_model_compare_func (GtkTreeModel *model, GtkTreeIter *tia,
	GtkTreeIter *tib, gpointer userdata)
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

void
fill_model(GtkListStore *store, const gchar *inpath)
{
	static gchar *path=NULL;
	gchar *name;
	GtkTreeIter iter;
	GdkPixbuf *pixbuf;
	GError *error;
	GDir *dir;
	GtkTreeSortable *sortable;
	gint priority;
	RS_FILETYPE *filetype;

	if (inpath)
	{
		if (path)
			g_free(path);
		path = g_strdup(inpath);
	}
	if (!path)
		return;
	dir = g_dir_open(path, 0, &error);
	if (dir == NULL) return;

	rs_conf_set_string(CONF_LWD, path);

	gui_status_push(_("Opening directory ..."));
	GUI_CATCHUP();

	g_dir_rewind(dir);

	gtk_list_store_clear(store);
	while((name = (gchar *) g_dir_read_name(dir)))
	{
		filetype = rs_filetype_get(name, TRUE);
		if (filetype)
			if (filetype->load)
			{
				GString *fullname;
				fullname = g_string_new(path);
				fullname = g_string_append(fullname, "/");
				fullname = g_string_append(fullname, name);
				priority = PRIO_U;
				rs_cache_load_quick(fullname->str, &priority);
				pixbuf = NULL;
				if (filetype->thumb)
					pixbuf = filetype->thumb(fullname->str);
				gtk_list_store_prepend (store, &iter);
				if (pixbuf==NULL)
				{
					pixbuf = gdk_pixbuf_new(GDK_COLORSPACE_RGB, FALSE, 8, 64, 64);
					gdk_pixbuf_fill(pixbuf, 0x00000000);
				}
				gtk_list_store_set (store, &iter,
					PIXBUF_COLUMN, pixbuf,
					TEXT_COLUMN, name,
					FULLNAME_COLUMN, fullname->str,
					PRIORITY_COLUMN, priority,
					-1);
				g_object_unref (pixbuf);
				g_string_free(fullname, FALSE);
			}
	}
	sortable = GTK_TREE_SORTABLE(store);
	gtk_tree_sortable_set_sort_func(sortable,
		TEXT_COLUMN,
		fill_model_compare_func,
		NULL,
		NULL);
	gtk_tree_sortable_set_sort_column_id(sortable, TEXT_COLUMN, GTK_SORT_ASCENDING);
	gui_status_push(_("Directory opened"));
}

void
icon_activated_helper(GtkIconView *iconview, GtkTreePath *path, gpointer user_data)
{
	gchar *name;
	gchar **out = user_data;
	GtkTreeModel *model = gtk_icon_view_get_model (iconview);
	GtkTreeIter iter;

	if (gtk_tree_model_get_iter(model, &iter, path))
	{
		gtk_tree_model_get (model, &iter, FULLNAME_COLUMN, &name, -1);
		gtk_tree_model_filter_convert_iter_to_child_iter((GtkTreeModelFilter *)model, &current_iter, &iter);
		*out = name;
	}
}

void
icon_activated(GtkIconView *iconview, RS_BLOB *rs)
{
	GtkTreeModel *model;
	gchar *name = NULL;
	RS_FILETYPE *filetype;
	RS_PHOTO *photo;
	extern GtkLabel *infolabel;
	GString *label;

	model = gtk_icon_view_get_model(iconview);
	gtk_icon_view_selected_foreach(iconview, icon_activated_helper, &name);
	if (name!=NULL)
	{
		gui_status_push(_("Opening image ..."));
		GUI_CATCHUP();
		if ((filetype = rs_filetype_get(name, TRUE)))
		{
			rs->in_use = FALSE;
			rs_photo_close(rs->photo);
			rs_photo_free(rs->photo);
			rs->photo = NULL;
			rs_reset(rs);
			photo = filetype->load(name);
			rs_image16_free(rs->histogram_dataset); rs->histogram_dataset = NULL;
			if (filetype->load_meta)
			{
				filetype->load_meta(name, photo->metadata);
				switch (photo->metadata->orientation)
				{
					case 90: ORIENTATION_90(photo->orientation);
						break;
					case 180: ORIENTATION_180(photo->orientation);
						break;
					case 270: ORIENTATION_270(photo->orientation);
						break;
				}
				label = g_string_new("");
				if (photo->metadata->shutterspeed!=0.0)
					g_string_append_printf(label, _("1/%.0f "), photo->metadata->shutterspeed);
				if (photo->metadata->iso!=0)
					g_string_append_printf(label, _("ISO%d "), photo->metadata->iso);
				if (photo->metadata->aperture!=0.0)
					g_string_append_printf(label, _("F/%.1f"), photo->metadata->aperture);
				gtk_label_set_text(infolabel, label->str);
				g_string_free(label, TRUE);
			} else
				gtk_label_set_text(infolabel, _("No metadata"));
			rs_cache_load(photo);
			rs_settings_double_to_rs_settings(photo->settings[0], rs->settings[0]);
			rs_settings_double_to_rs_settings(photo->settings[1], rs->settings[1]);
			rs_settings_double_to_rs_settings(photo->settings[2], rs->settings[2]);
			rs->photo = photo;
			rs->histogram_dataset = rs_image16_scale(rs->photo->input, NULL,
				(gdouble)HISTOGRAM_DATASET_WIDTH/(gdouble)rs->photo->input->w);
		}
		rs->in_use = TRUE;
		update_preview_callback(NULL, rs);
		gui_status_push(_("Image opened"));
	}
}

gboolean
gui_tree_filter_helper(GtkTreeModel *model, GtkTreeIter *iter, gpointer data)
{
	gint p;
	gint prio = (gint) data;
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

GtkWidget *
make_iconview(RS_BLOB *rs, GtkWidget *iconview, GtkListStore *store, gint prio)
{
	GtkWidget *scroller;
	GtkTreeModel *tree;

	gtk_icon_view_set_pixbuf_column (GTK_ICON_VIEW (iconview), PIXBUF_COLUMN);
	gtk_icon_view_set_text_column (GTK_ICON_VIEW (iconview), TEXT_COLUMN);

	tree = gtk_tree_model_filter_new(GTK_TREE_MODEL (store), NULL);
	gtk_tree_model_filter_set_visible_func(GTK_TREE_MODEL_FILTER (tree),
		gui_tree_filter_helper, (gpointer) prio, NULL);
	gtk_icon_view_set_model (GTK_ICON_VIEW (iconview), tree);
	gtk_icon_view_set_columns(GTK_ICON_VIEW (iconview), 1000);
	gtk_icon_view_set_selection_mode(GTK_ICON_VIEW (iconview), GTK_SELECTION_BROWSE);
	gtk_widget_set_size_request (iconview, -1, 160);
	g_signal_connect((gpointer) iconview, "selection_changed",
		G_CALLBACK (icon_activated), rs);
	scroller = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scroller),
		GTK_POLICY_AUTOMATIC, GTK_POLICY_NEVER);
	gtk_container_add (GTK_CONTAINER (scroller), iconview);
	return(scroller);
}

void
gui_icon_notebook_callback(GtkNotebook *notebook, GtkNotebookPage *page,
	guint page_num, gpointer date)
{
	current_iconview = iconview[page_num];
	current_priority = priorities[page_num];
	return;
}

void
gui_icon_count_priorities_callback(GtkTreeModel *treemodel,
	GtkTreePath *treepath, GtkTreeIter *treeiter, gpointer data)
{
	struct count_helper *count = data;
	GtkTreeIter iter;
	GtkTreePath *path;
	gint priority;
	gint count_1 = 0;
	gint count_2 = 0;
	gint count_3 = 0;
	gint count_u = 0;
	gint count_d = 0;
	gint count_all;
	gchar label1[63];
	gchar label2[63];
	gchar label3[63];
	gchar label4[63];
	gchar label5[63];
	gchar label6[63];

	path = gtk_tree_path_new_first();
	gtk_tree_model_get_iter(treemodel, &iter, path);
	
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
	
	
	count_all = count_1+count_2+count_3+count_u;

	g_sprintf(label1, _("* <small>(%d)</small>"), count_all);
	g_sprintf(label2, _("1 <small>(%d)</small>"), count_1);
	g_sprintf(label3, _("2 <small>(%d)</small>"), count_2);
	g_sprintf(label4, _("3 <small>(%d)</small>"), count_3);
	g_sprintf(label5, _("U <small>(%d)</small>"), count_u);
	g_sprintf(label6, _("D <small>(%d)</small>"), count_d);

	gtk_label_set_markup(GTK_LABEL(count->label1), label1);
	gtk_label_set_markup(GTK_LABEL(count->label2), label2);
	gtk_label_set_markup(GTK_LABEL(count->label3), label3);
	gtk_label_set_markup(GTK_LABEL(count->label4), label4);
	gtk_label_set_markup(GTK_LABEL(count->label5), label5);
	gtk_label_set_markup(GTK_LABEL(count->label6), label6);

	return;
}

GtkWidget *
make_iconbox(RS_BLOB *rs, GtkListStore *store)
{
	GtkWidget *notebook;
	gint n;
	GtkWidget *e_label1;
	GtkWidget *e_label2;
	GtkWidget *e_label3;
	GtkWidget *e_label4;
	GtkWidget *e_label5;
	GtkWidget *e_label6;
	gchar label1[63];
	gchar label2[63];
	gchar label3[63];
	gchar label4[63];
	gchar label5[63];
	gchar label6[63];
	
	struct count_helper *count;
	count = g_malloc(sizeof(struct count_helper));

	for(n=0;n<6;n++)
		iconview[n] = gtk_icon_view_new();

	count->label1 = gtk_label_new(NULL);
	count->label2 = gtk_label_new(NULL);
	count->label3 = gtk_label_new(NULL);
	count->label4 = gtk_label_new(NULL);
	count->label5 = gtk_label_new(NULL);
	count->label6 = gtk_label_new(NULL);

	g_sprintf(label1, _("* <small>(%d)</small>"), 0);
	g_sprintf(label2, _("1 <small>(%d)</small>"), 0);
	g_sprintf(label3, _("2 <small>(%d)</small>"), 0);
	g_sprintf(label4, _("3 <small>(%d)</small>"), 0);
	g_sprintf(label5, _("U <small>(%d)</small>"), 0);
	g_sprintf(label6, _("D <small>(%d)</small>"), 0);

	gtk_label_set_markup(GTK_LABEL(count->label1), label1);
	gtk_label_set_markup(GTK_LABEL(count->label2), label2);
	gtk_label_set_markup(GTK_LABEL(count->label3), label3);
	gtk_label_set_markup(GTK_LABEL(count->label4), label4);
	gtk_label_set_markup(GTK_LABEL(count->label5), label5);
	gtk_label_set_markup(GTK_LABEL(count->label6), label6);
	
	e_label1 = gui_tooltip_no_window(count->label1, _("All photos (excluding deleted)"), NULL);
	e_label2 = gui_tooltip_no_window(count->label2, _("Priority 1 photos"), NULL);
	e_label3 = gui_tooltip_no_window(count->label3, _("Priority 2 photos"), NULL);
	e_label4 = gui_tooltip_no_window(count->label4, _("Priority 3 photos"), NULL);
	e_label5 = gui_tooltip_no_window(count->label5, _("Unprioritized photos"), NULL);
	e_label6 = gui_tooltip_no_window(count->label6, _("Deleded photos"), NULL);

	gtk_misc_set_alignment(GTK_MISC(count->label1), 0.0, 0.5);
	gtk_misc_set_alignment(GTK_MISC(count->label2), 0.0, 0.5);
	gtk_misc_set_alignment(GTK_MISC(count->label3), 0.0, 0.5);
	gtk_misc_set_alignment(GTK_MISC(count->label4), 0.0, 0.5);
	gtk_misc_set_alignment(GTK_MISC(count->label5), 0.0, 0.5);
	gtk_misc_set_alignment(GTK_MISC(count->label6), 0.0, 0.5);

	priorities[0] = PRIO_ALL;
	priorities[1] = PRIO_1;
	priorities[2] = PRIO_2;
	priorities[3] = PRIO_3;
	priorities[4] = PRIO_U;
	priorities[5] = PRIO_D;

	current_iconview = iconview[0];
	current_priority = priorities[0];

	notebook = gtk_notebook_new();

	gtk_notebook_set_tab_pos(GTK_NOTEBOOK(notebook), GTK_POS_LEFT);
	gtk_notebook_append_page(GTK_NOTEBOOK(notebook), make_iconview(rs, iconview[0], store, priorities[0]), e_label1);
	gtk_notebook_append_page(GTK_NOTEBOOK(notebook), make_iconview(rs, iconview[1], store, priorities[1]), e_label2);
	gtk_notebook_append_page(GTK_NOTEBOOK(notebook), make_iconview(rs, iconview[2], store, priorities[2]), e_label3);
	gtk_notebook_append_page(GTK_NOTEBOOK(notebook), make_iconview(rs, iconview[3], store, priorities[3]), e_label4);
	gtk_notebook_append_page(GTK_NOTEBOOK(notebook), make_iconview(rs, iconview[4], store, priorities[4]), e_label5);
	gtk_notebook_append_page(GTK_NOTEBOOK(notebook), make_iconview(rs, iconview[5], store, priorities[5]), e_label6);

	g_signal_connect(notebook, "switch-page", G_CALLBACK(gui_icon_notebook_callback), NULL);
	g_signal_connect(store, "row-changed", G_CALLBACK(gui_icon_count_priorities_callback), count);

	return(notebook);
}

void
gui_menu_open_callback(gpointer callback_data, guint callback_action, GtkWidget *widget)
{
	GtkWidget *fc;
	GtkListStore *store = (GtkListStore *) callback_action;
	gchar *lwd = rs_conf_get_string(CONF_LWD);

	fc = gtk_file_chooser_dialog_new (_("Open File"), NULL,
		GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
		GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
		GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT, NULL);
	gtk_dialog_set_default_response(GTK_DIALOG(fc), GTK_RESPONSE_ACCEPT);
	
	gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER (fc), lwd);

	if (gtk_dialog_run (GTK_DIALOG (fc)) == GTK_RESPONSE_ACCEPT)
	{
		char *filename;

		filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (fc));
		gtk_widget_destroy (fc);
		fill_model(store, filename);
		g_free (filename);
	} else
		gtk_widget_destroy (fc);

	g_free(lwd);
	return;
}

void
gui_menu_reload_callback(gpointer callback_data, guint callback_action, GtkWidget *widget)
{
	GtkListStore *store = (GtkListStore *) callback_action;
	fill_model(store, NULL);
	return;
}

void
gui_menu_purge_d_callback(gpointer callback_data, guint callback_action, GtkWidget *widget)
{
	GtkTreeModel *model;
	GtkTreeModel *child;
	GtkTreePath *path;
	GtkTreeIter iter;
	gchar *fullname, *thumb, *cache;
	gint priority;
	GtkWidget *dialog;

	dialog = gui_dialog_make_from_text(GTK_STOCK_DIALOG_WARNING,
		_("Deleting photos"),
		_("Your files will be <b>permanently</b> deleted!"));
	gtk_dialog_add_button (GTK_DIALOG (dialog), GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL);
	gtk_dialog_add_button (GTK_DIALOG (dialog), _("Delete photos"), GTK_RESPONSE_ACCEPT);
	gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_CANCEL);
	gtk_widget_show_all(dialog);

	if((gtk_dialog_run(GTK_DIALOG(dialog)) != GTK_RESPONSE_ACCEPT))
	{
		gtk_widget_destroy(dialog);
		return;
	}
	else
		gtk_widget_destroy(dialog);

	model = gtk_icon_view_get_model((GtkIconView *) current_iconview);
	child = gtk_tree_model_filter_get_model((GtkTreeModelFilter *) model);

	path = gtk_tree_path_new_first();
	while(gtk_tree_model_get_iter(child, &iter, path))
	{
		gtk_tree_model_get(GTK_TREE_MODEL(child), &iter, PRIORITY_COLUMN, &priority, -1);
		if (priority == PRIO_D)
		{
			gtk_tree_model_get(GTK_TREE_MODEL(child), &iter, FULLNAME_COLUMN, &fullname, -1);
			if(0 == g_unlink(fullname))
			{
				if ((thumb = rs_thumb_get_name(fullname)))
				{
					g_unlink(thumb);
					g_free(thumb);
				}
				if ((cache = rs_cache_get_name(fullname)))
				{
					g_unlink(cache);
					g_free(cache);
				}
				gtk_list_store_remove(GTK_LIST_STORE(child), &iter);
				GUI_CATCHUP();
			}
			else
				gtk_tree_path_next(path);
			g_free(fullname);
		}
		else
			gtk_tree_path_next(path);
	}
	return;
}

void
gui_preview_bg_color_changed(GtkColorButton *widget, RS_BLOB *rs)
{
	GdkColor color;
	gtk_color_button_get_color(GTK_COLOR_BUTTON(widget), &color);
	gtk_widget_modify_bg(rs->preview_drawingarea->parent->parent,
		GTK_STATE_NORMAL, &color);
	rs_conf_set_color(CONF_PREBGCOLOR, &color);
	return;
}

gboolean
gui_fullscreen_callback(GtkWidget *widget, GdkEventWindowState *event, GtkWidget *iconbox)
{
	if (event->new_window_state & GDK_WINDOW_STATE_FULLSCREEN)
	{
		gtk_widget_hide(iconbox);
		fullscreen = TRUE;
	}
	if (!(event->new_window_state & GDK_WINDOW_STATE_FULLSCREEN))
	{
		gtk_widget_show(iconbox);
		fullscreen = FALSE;
	}
	return(FALSE);
}

gboolean
gui_menu_prevnext_helper(GtkTreeModel *model, GtkTreePath *path, GtkTreeIter *iter, gpointer user_data)
{
	struct nextprev_helper *helper = user_data;
    gchar *name;
	guint priority;
	gchar *needle;

	gtk_tree_model_get(model, iter, PRIORITY_COLUMN, &priority, -1);

	if ((priority == current_priority) || ((current_priority==PRIO_ALL) && (priority != PRIO_D)))
	{
		needle = g_path_get_basename(helper->filename);
		gtk_tree_model_get(model, iter, TEXT_COLUMN, &name, -1);
		if(g_utf8_collate(needle, name) < 0) /* after */
		{
			helper->next = gtk_tree_path_copy(path);
			g_free(needle);
			g_free(name);
			return(TRUE);
		}
		else if (g_utf8_collate(needle, name) > 0) /* before */
		{
			if (helper->previous)
				gtk_tree_path_free(helper->previous);
			helper->previous = gtk_tree_path_copy(path);
			g_free(needle);
			g_free(name);
		}
		else
		{
			g_free(needle);
	    	g_free(name);
		}
	}
    return FALSE;
}

void
gui_menu_zoom_callback(gpointer callback_data, guint callback_action, GtkWidget *widget)
{
	gdouble scalex, scaley;
	RS_BLOB *rs = (RS_BLOB *) callback_data;

	switch (callback_action)
	{
		case 0: /* zoom to fit */
			if (!rs->photo->input)
				return;
			if (rs->photo->orientation & 1)
			{
				scalex = ((gdouble) rs->preview_width / (gdouble) rs->photo->input->h)*0.99;
				scaley = ((gdouble) rs->preview_height / (gdouble) rs->photo->input->w)*0.99;
			}
			else
			{
				scalex = ((gdouble) rs->preview_width / (gdouble) rs->photo->input->w)*0.99;
				scaley = ((gdouble) rs->preview_height / (gdouble) rs->photo->input->h)*0.99;
			}

			if (scalex < scaley)
				SETVAL(rs->scale, scalex);
			else
				SETVAL(rs->scale, scaley);
			break;
		case 1: /* zoom in */
			SETVAL(rs->scale, GETVAL(rs->scale)+0.1);
			break;
		case 2: /* zoom out */
			SETVAL(rs->scale, GETVAL(rs->scale)-0.1);
			break;
		case 100: /* zoom 100% */
			SETVAL(rs->scale, 1.0);
			break;
	}
	return;
}

void
gui_menu_prevnext_callback(gpointer callback_data, guint callback_action, GtkWidget *widget)
{
	GtkTreeModel *model;
	GtkTreeModel *child;
	GtkTreePath *path = NULL;
	struct nextprev_helper helper;
	RS_BLOB *rs = (RS_BLOB *) callback_data;

	if (!rs->in_use) return;

	helper.filename = rs->photo->filename;
	helper.previous = NULL;
	helper.next = NULL;

	model = gtk_icon_view_get_model((GtkIconView *) current_iconview);
	child = gtk_tree_model_filter_get_model((GtkTreeModelFilter *) model);
	gtk_tree_model_foreach(child, gui_menu_prevnext_helper, &helper);

	switch (callback_action)
	{
		case 1: /* previous */
			if (helper.previous)
				path = gtk_tree_model_filter_convert_child_path_to_path(
					(GtkTreeModelFilter *) model, helper.previous);
			break;
		case 2: /* next */
			if (helper.next)
				path = gtk_tree_model_filter_convert_child_path_to_path(
					(GtkTreeModelFilter *) model, helper.next);
			break;
	}

	if (path)
		gtk_icon_view_select_path((GtkIconView *) current_iconview, path);

	if (helper.next)
		gtk_tree_path_free(helper.next);
	if (helper.previous)
		gtk_tree_path_free(helper.previous);
	return;
}

void
gui_menu_setprio_callback(gpointer callback_data, guint callback_action, GtkWidget *widget)
{
	GtkTreeModel *model;
	RS_BLOB *rs = (RS_BLOB *) callback_data;

	model = gtk_icon_view_get_model((GtkIconView *) current_iconview);
	model = gtk_tree_model_filter_get_model ((GtkTreeModelFilter *) model);
	if (gtk_list_store_iter_is_valid((GtkListStore *)model, &current_iter))
	{
		gtk_list_store_set ((GtkListStore *)model, &current_iter,
			PRIORITY_COLUMN, callback_action,
			-1);
		rs->photo->priority = callback_action;
		gui_status_push(_("Changed image priority"));
	}
	return;
}

void
gui_menu_widget_visible_callback(gpointer callback_data, guint callback_action, GtkWidget *widget)
{
	GtkWidget *target = (GtkWidget *) callback_action;
	if (GTK_WIDGET_VISIBLE(target))
		gtk_widget_hide(target);
	else
		gtk_widget_show(target);
	return;
}

void
gui_menu_fullscreen_callback(gpointer callback_data, guint callback_action, GtkWidget *widget)
{
	GtkWindow *window = (GtkWindow *) callback_action;
	if (fullscreen)
		gtk_window_unfullscreen(window);
	else
		gtk_window_fullscreen(window);
	return;
}

gboolean
gui_histogram_height_changed(GtkAdjustment *caller, RS_BLOB *rs)
{
	GdkPixbuf *pixbuf;
	const gint newheight = (gint) caller->value;
	pixbuf = gdk_pixbuf_new(GDK_COLORSPACE_RGB, FALSE, 8, 256, newheight);
	gtk_image_set_from_pixbuf((GtkImage *) rs->histogram_image, pixbuf);
	update_histogram(rs);
	rs_conf_set_integer(CONF_HISTHEIGHT, newheight);
	return(FALSE);
}

gboolean
gui_gamma_value_changed(GtkAdjustment *caller, RS_BLOB *rs)
{
	rs_conf_set_double(CONF_GAMMAVALUE, caller->value);
	rs->gamma = caller->value;
	update_preview(rs);
	return(FALSE);
}

void
local_cache_toggled(GtkToggleButton *togglebutton, gpointer user_data)
{
	rs_local_cachedir(togglebutton->active);
	rs_conf_set_boolean(CONF_CACHEDIR_IS_LOCAL, togglebutton->active);
	return;
}

void
load_gdk_toggled(GtkToggleButton *togglebutton, gpointer user_data)
{
	rs_load_gdk(togglebutton->active);
	rs_conf_set_boolean(CONF_LOAD_GDK, togglebutton->active);
	return;
}

void
gui_menu_preference_callback(gpointer callback_data, guint callback_action, GtkWidget *widget)
{
	GtkWidget *dialog;
	GtkWidget *notebook;
	GtkWidget *vbox;
	GtkWidget *colorsel;
	GtkWidget *colorsel_label;
	GtkWidget *colorsel_hbox;
	GtkWidget *preview_page;
	GtkWidget *button_close;
	GdkColor color;
	GtkWidget *histsize;
	GtkWidget *histsize_label;
	GtkWidget *histsize_hbox;
	GtkObject *histsize_adj;
	gint histogram_height;
	GtkWidget *gammavalue;
	GtkWidget *gammavalue_label;
	GtkWidget *gammavalue_hbox;
	GtkObject *gammavalue_adj;
	gdouble gamma_value;
	GtkWidget *local_cache_check;
	gboolean local_cache;
	GtkWidget *load_gdk_check;
	gboolean load_gdk;

	GtkWidget *batch_page;
	GtkWidget *batch_directory_hbox;
	GtkWidget *batch_directory_label;
	GtkWidget *batch_directory_entry;
	GtkWidget *batch_filename_hbox;
	GtkWidget *batch_filename_label;
	GtkWidget *batch_filename_entry;
	GtkWidget *batch_filetype_hbox;
	GtkWidget *batch_filetype_label;
	GtkWidget *batch_filetype_entry;

	gchar *conf_temp = NULL;

	RS_BLOB *rs = (RS_BLOB *) callback_data;

	dialog = gtk_dialog_new();
	gtk_window_set_title(GTK_WINDOW(dialog), _("Preferences"));
	gtk_window_set_resizable(GTK_WINDOW(dialog), FALSE);
	gtk_window_set_type_hint(GTK_WINDOW(dialog), GDK_WINDOW_TYPE_HINT_DIALOG);
	gtk_window_set_destroy_with_parent(GTK_WINDOW(dialog), TRUE);
	gtk_dialog_set_has_separator (GTK_DIALOG(dialog), FALSE);
	g_signal_connect_swapped(dialog, "delete_event",
		G_CALLBACK (gtk_widget_destroy), dialog);
	g_signal_connect_swapped(dialog, "response",
		G_CALLBACK (gtk_widget_destroy), dialog);

	vbox = GTK_DIALOG (dialog)->vbox;

	preview_page = gtk_vbox_new(FALSE, 4);
	gtk_container_set_border_width (GTK_CONTAINER (preview_page), 6);
	colorsel_hbox = gtk_hbox_new(FALSE, 0);
	colorsel_label = gtk_label_new(_("Preview background color:"));
	gtk_misc_set_alignment(GTK_MISC(colorsel_label), 0.0, 0.5);

	colorsel = gtk_color_button_new();
	COLOR_BLACK(color);
	if (rs_conf_get_color(CONF_PREBGCOLOR, &color))
		gtk_color_button_set_color(GTK_COLOR_BUTTON(colorsel), &color);
	g_signal_connect(colorsel, "color-set", G_CALLBACK (gui_preview_bg_color_changed), rs);
	gtk_box_pack_start (GTK_BOX (colorsel_hbox), colorsel_label, TRUE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (colorsel_hbox), colorsel, FALSE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (preview_page), colorsel_hbox, FALSE, TRUE, 0);

	if (!rs_conf_get_integer(CONF_HISTHEIGHT, &histogram_height))
		histogram_height = 128;
	histsize_hbox = gtk_hbox_new(FALSE, 0);
	histsize_label = gtk_label_new(_("Histogram height:"));
	gtk_misc_set_alignment(GTK_MISC(histsize_label), 0.0, 0.5);
	histsize_adj = gtk_adjustment_new(histogram_height, 15.0, 500.0, 1.0, 10.0, 10.0);
	g_signal_connect(histsize_adj, "value_changed",
		G_CALLBACK(gui_histogram_height_changed), rs);
	histsize = gtk_spin_button_new(GTK_ADJUSTMENT(histsize_adj), 1, 0);
	gtk_box_pack_start (GTK_BOX (histsize_hbox), histsize_label, TRUE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (histsize_hbox), histsize, FALSE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (preview_page), histsize_hbox, FALSE, TRUE, 0);

	if (!rs_conf_get_double(CONF_GAMMAVALUE, &gamma_value))
		gamma_value = rs->gamma;
	gammavalue_hbox = gtk_hbox_new(FALSE, 0);
	gammavalue_label = gtk_label_new(_("Gamma value:"));
	gtk_misc_set_alignment(GTK_MISC(gammavalue_label), 0.0, 0.5);
	gammavalue_adj = gtk_adjustment_new(gamma_value, 0.0, 5.0, 0.1, 1.0, 1.0);
	g_signal_connect(gammavalue_adj, "value_changed",
		G_CALLBACK(gui_gamma_value_changed), rs);
	gammavalue = gtk_spin_button_new(GTK_ADJUSTMENT(gammavalue_adj), 1, 2);
	gtk_box_pack_start (GTK_BOX (gammavalue_hbox), gammavalue_label, TRUE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (gammavalue_hbox), gammavalue, FALSE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (preview_page), gammavalue_hbox, FALSE, TRUE, 0);

	local_cache_check = gtk_check_button_new_with_label(_("Place cache in home directory"));
	if(!rs_conf_get_boolean(CONF_CACHEDIR_IS_LOCAL, &local_cache))
		local_cache = FALSE;
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(local_cache_check), local_cache);
	g_signal_connect ((gpointer) local_cache_check, "toggled", G_CALLBACK (local_cache_toggled), NULL);
	gtk_box_pack_start (GTK_BOX (preview_page), local_cache_check, FALSE, TRUE, 0);

	load_gdk_check = gtk_check_button_new_with_label(_("Load 8 bit photos (jpeg, png, etc)"));
	if(!rs_conf_get_boolean(CONF_LOAD_GDK, &load_gdk))
		load_gdk = FALSE;
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(load_gdk_check), load_gdk);
	g_signal_connect ((gpointer) load_gdk_check, "toggled", G_CALLBACK (load_gdk_toggled), NULL);
	gtk_box_pack_start (GTK_BOX (preview_page), load_gdk_check, FALSE, TRUE, 0);


	batch_page = gtk_vbox_new(FALSE, 4);
	gtk_container_set_border_width (GTK_CONTAINER (batch_page), 6);

	batch_directory_hbox = gtk_hbox_new(FALSE, 0);
	batch_directory_label = gtk_label_new(_("Batch export directory:"));
	gtk_misc_set_alignment(GTK_MISC(batch_directory_label), 0.0, 0.5);
	batch_directory_entry = gtk_entry_new();
	conf_temp = rs_conf_get_string(CONF_BATCH_DIRECTORY);
	if (g_str_equal(conf_temp, ""))
	{
		rs_conf_set_string(CONF_BATCH_DIRECTORY, "exported/");
		g_free(conf_temp);
		conf_temp = rs_conf_get_string(CONF_BATCH_DIRECTORY);
	}
	gtk_entry_set_text(GTK_ENTRY(batch_directory_entry), conf_temp);
	g_free(conf_temp);
	g_signal_connect ((gpointer) batch_directory_entry, "changed", G_CALLBACK(gui_batch_directory_entry_changed), NULL);
	gtk_box_pack_start (GTK_BOX (batch_directory_hbox), batch_directory_label, TRUE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (batch_directory_hbox), batch_directory_entry, FALSE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (batch_page), batch_directory_hbox, FALSE, TRUE, 0);

	batch_filename_hbox = gtk_hbox_new(FALSE, 0);
	batch_filename_label = gtk_label_new(_("Batch export filename:"));
	gtk_misc_set_alignment(GTK_MISC(batch_filename_label), 0.0, 0.5);
	batch_filename_entry = gtk_entry_new();
	conf_temp = rs_conf_get_string(CONF_BATCH_FILENAME);
	if (g_str_equal(conf_temp, ""))
	{
		rs_conf_set_string(CONF_BATCH_FILENAME, "%f_%2c");
		g_free(conf_temp);
		conf_temp = rs_conf_get_string(CONF_BATCH_FILENAME);
	}
	gtk_entry_set_text(GTK_ENTRY(batch_filename_entry), conf_temp);
	g_free(conf_temp);
	g_signal_connect ((gpointer) batch_filename_entry, "changed", G_CALLBACK(gui_batch_filename_entry_changed), NULL);
	gtk_box_pack_start (GTK_BOX (batch_filename_hbox), batch_filename_label, TRUE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (batch_filename_hbox), batch_filename_entry, FALSE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (batch_page), batch_filename_hbox, FALSE, TRUE, 0);

	batch_filetype_hbox = gtk_hbox_new(FALSE, 0);
	batch_filetype_label = gtk_label_new(_("Batch export filetype:"));
	gtk_misc_set_alignment(GTK_MISC(batch_filetype_label), 0.0, 0.5);
	batch_filetype_entry = gtk_entry_new();
	conf_temp = rs_conf_get_string(CONF_BATCH_FILETYPE);
	if (g_str_equal(conf_temp, ""))
	{
		rs_conf_set_string(CONF_BATCH_FILETYPE, "jpg");
		g_free(conf_temp);
		conf_temp = rs_conf_get_string(CONF_BATCH_FILETYPE);
	}
	gtk_entry_set_text(GTK_ENTRY(batch_filetype_entry), conf_temp);
	g_free(conf_temp);
	g_signal_connect ((gpointer) batch_filetype_entry, "changed", G_CALLBACK(gui_batch_filetype_entry_changed), NULL);
	gtk_box_pack_start (GTK_BOX (batch_filetype_hbox), batch_filetype_label, TRUE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (batch_filetype_hbox), batch_filetype_entry, FALSE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (batch_page), batch_filetype_hbox, FALSE, TRUE, 0);


	notebook = gtk_notebook_new();
	gtk_container_set_border_width (GTK_CONTAINER (notebook), 6);
	gtk_notebook_append_page(GTK_NOTEBOOK(notebook), preview_page, gtk_label_new(_("Preview")));
	gtk_notebook_append_page(GTK_NOTEBOOK(notebook), batch_page, gtk_label_new(_("Batch")));
	gtk_box_pack_start (GTK_BOX (vbox), notebook, FALSE, FALSE, 0);

	button_close = gtk_button_new_from_stock(GTK_STOCK_CLOSE);
	gtk_dialog_add_action_widget (GTK_DIALOG (dialog), button_close, GTK_RESPONSE_CLOSE);

	gtk_widget_show_all(dialog);
	return;
}

void
gui_menu_batch_run_queue_callback(gpointer callback_data, guint callback_action, GtkWidget *widget)
{
	RS_BLOB *rs = (RS_BLOB *) callback_data;

	rs->queue->directory = rs_conf_get_string(CONF_BATCH_DIRECTORY);
	rs->queue->filename = rs_conf_get_string(CONF_BATCH_FILENAME);
	rs->queue->filetype = rs_conf_get_string(CONF_BATCH_FILETYPE);

	g_idle_add_full(G_PRIORITY_LOW, (GSourceFunc) rs_run_batch_idle, rs->queue, NULL);

	return;
}

void
gui_menu_add_to_batch_queue_callback(gpointer callback_data, guint callback_action, GtkWidget *widget)
{
	RS_BLOB *rs = (RS_BLOB *) callback_data;
	if (rs->in_use)
	{
		if (batch_add_to_queue(rs->queue, rs->photo->filename, rs->photo->current_setting, NULL))
			gui_status_push(_("Added to batch queue"));
		else
			gui_status_push(_("Already added to batch queue"));
	}
}

void
gui_menu_remove_from_batch_queue_callback(gpointer callback_data, guint callback_action, GtkWidget *widget)
{
	RS_BLOB *rs = (RS_BLOB *) callback_data;
	if (rs->in_use)
	{
		if (batch_remove_from_queue(rs->queue, rs->photo->filename, rs->photo->current_setting))
			gui_status_push(_("Removed from batch queue"));
		else
			gui_status_push(_("Not in batch queue"));
	}
}

void
gui_menu_add_view_to_batch_queue_callback(gpointer callback_data, guint callback_action, GtkWidget *widget)
{
	GtkTreeModel *model;
	GtkTreePath *path;
	GtkTreeIter iter;
	gchar *fullname;
	RS_BLOB *rs = (RS_BLOB *) callback_data;

	model = gtk_icon_view_get_model((GtkIconView *) current_iconview);

	path = gtk_tree_path_new_first();
	while(gtk_tree_model_get_iter(model, &iter, path))
	{
		gtk_tree_model_get(GTK_TREE_MODEL(model), &iter, FULLNAME_COLUMN, &fullname, -1);
		batch_add_to_queue(rs->queue, fullname, 0, NULL);
		gtk_tree_path_next(path);
	}
	gtk_tree_path_free(path);

	return;
}

void
gui_about()
{
	static GtkWidget *aboutdialog = NULL;
	const gchar *authors[] = {
		"Anders Brander <anders@brander.dk>",
		"Anders Kvist <anders@kvistmail.dk>",
		NULL
	};
	if (!aboutdialog)
	{
		aboutdialog = gtk_about_dialog_new ();
		gtk_about_dialog_set_version (GTK_ABOUT_DIALOG (aboutdialog), VERSION);
		gtk_about_dialog_set_name (GTK_ABOUT_DIALOG (aboutdialog), "Rawstudio");
		gtk_about_dialog_set_comments(GTK_ABOUT_DIALOG (aboutdialog), _("A raw image converter for GTK+/GNOME"));
		gtk_about_dialog_set_website(GTK_ABOUT_DIALOG (aboutdialog), "http://rawstudio.org/");
		gtk_about_dialog_set_authors(GTK_ABOUT_DIALOG (aboutdialog), authors);
	}
	gtk_widget_show(aboutdialog);
	return;
}

void
gui_dialog_simple(gchar *title, gchar *message)
{
	GtkWidget *dialog, *label;

	dialog = gtk_dialog_new_with_buttons(title, NULL, GTK_DIALOG_NO_SEPARATOR,
		GTK_STOCK_OK, GTK_RESPONSE_NONE, NULL);
	label = gtk_label_new(message);
	g_signal_connect_swapped(dialog, "response",
		G_CALLBACK (gtk_widget_destroy), dialog);
	gtk_container_add(GTK_CONTAINER (GTK_DIALOG(dialog)->vbox), label);
	gtk_widget_show_all(dialog);
	return;
}

void
gui_menu_auto_wb_callback(gpointer callback_data, guint callback_action, GtkWidget *widget)
{
	RS_BLOB *rs = (RS_BLOB *) callback_data;
	gui_status_push(_("Adjusting to auto white ballance"));
	rs_set_wb_auto(rs);
}

void
gui_menu_cam_wb_callback(gpointer callback_data, guint callback_action, GtkWidget *widget)
{
	RS_BLOB *rs = (RS_BLOB *) callback_data;
	if (rs->photo->metadata->cam_mul[R] == -1.0)
		gui_status_push(_("No white balance to set from"));
	else
	{
		gui_status_push(_("Adjusting to camera white balance"));
		rs_set_wb_from_mul(rs, rs->photo->metadata->cam_mul);
	}
}

struct {
	gchar *extension;
	gchar *label;
	gboolean (*func)(GdkPixbuf *, gchar *filename);
} savers[] = {
	{"jpg", _("JPEG (Joint Photographic Experts Group)"), gui_save_jpg},
	{"png", _("PNG (Portable Network Graphics)"), gui_save_png},
	{NULL, NULL, NULL},
};

void
gui_filetype_callback(GtkComboBox *filetype, gpointer callback_data)
{
	gchar *filename, *newfilename;
	gint n, lastdot=0;
	GtkWidget *fc = GTK_WIDGET(GTK_WIDGET(filetype)->parent)->parent; 
	/* GtkComboBox is a child to GtkAlignment type which has the fc as parent */

	filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(fc));

	if (filename)
	{
		newfilename = g_path_get_basename(filename);
		g_free(filename);
		filename = newfilename;

		/* find extension */
		n = 0;
		while (filename[n])
		{
			if (filename[n]=='.')
				lastdot = n;
			n++;
		}
		if (lastdot != 0)
			filename[lastdot] = '\0';

		n = gtk_combo_box_get_active(GTK_COMBO_BOX(filetype));
		newfilename = g_strconcat(filename, ".", savers[n].extension, NULL);
		gtk_file_chooser_set_current_name (GTK_FILE_CHOOSER (fc), newfilename);

		g_free(filename);
	}
	return;
}

void
gui_save_file_callback(gpointer callback_data, guint callback_action, GtkWidget *widget)
{
	RS_BLOB *rs = (RS_BLOB *) callback_data;
	GtkWidget *fc;
	GString *name;
	gchar *dirname;
	gchar *basename;
	GString *export_path;
	gchar *conf_export;
	GtkWidget *filetype;
	gint n;
	if (!rs->in_use) return;
	dirname = g_path_get_dirname(rs->photo->filename);
	basename = g_path_get_basename(rs->photo->filename);

	conf_export = rs_conf_get_string(CONF_DEFAULT_EXPORT_TEMPLATE);

	if (conf_export)
	{
		if (conf_export[0]=='/')
		{
			g_free(dirname);
			dirname = conf_export;
		}
		else
		{
			export_path = g_string_new(dirname);
			g_string_append(export_path, "/");
			g_string_append(export_path, conf_export);
			g_free(dirname);
			dirname = export_path->str;
			g_string_free(export_path, FALSE);
			g_free(conf_export);
		}
		g_mkdir_with_parents(dirname, 00755);
	}

	gui_status_push(_("Saving file ..."));

	filetype = gtk_combo_box_new_text();

	n=0;
	while(savers[n].extension)
	{
		gchar *filetype_str;
		gtk_combo_box_append_text(GTK_COMBO_BOX(filetype), savers[n].label);

		filetype_str = rs_conf_get_string(CONF_SAVE_FILETYPE);
		if(filetype_str)
			if (g_str_equal(savers[n].extension, filetype_str))
				gtk_combo_box_set_active(GTK_COMBO_BOX(filetype), n);
		g_free(filetype_str);
		n++;
	}
	if (gtk_combo_box_get_active(GTK_COMBO_BOX(filetype)) == -1)
		gtk_combo_box_set_active(GTK_COMBO_BOX(filetype), 0);

	name = g_string_new(basename);
	g_string_append(name, "_output.");
	g_string_append(name, savers[gtk_combo_box_get_active(GTK_COMBO_BOX(filetype))].extension);

	g_signal_connect ((gpointer) filetype, "changed", G_CALLBACK (gui_filetype_callback), name);

	fc = gtk_file_chooser_dialog_new (_("Save File"), NULL,
		GTK_FILE_CHOOSER_ACTION_SAVE,
		GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
		GTK_STOCK_SAVE, GTK_RESPONSE_ACCEPT, NULL);
	gtk_dialog_set_default_response(GTK_DIALOG(fc), GTK_RESPONSE_ACCEPT);

#if GTK_CHECK_VERSION(2,8,0)
	gtk_file_chooser_set_do_overwrite_confirmation (GTK_FILE_CHOOSER (fc), TRUE);
#endif
	gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (fc), dirname);
	gtk_file_chooser_set_current_name (GTK_FILE_CHOOSER (fc), name->str);
	gtk_file_chooser_set_extra_widget(GTK_FILE_CHOOSER (fc), filetype);
	if (gtk_dialog_run (GTK_DIALOG (fc)) == GTK_RESPONSE_ACCEPT)
	{
		char *filename;
		GdkPixbuf *pixbuf;
		RS_IMAGE16 *rsi;

		filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (fc));
		if (rs->photo->orientation)
		{
			rsi = rs_image16_copy(rs->photo->input);
			rs_image16_orientation(rsi, rs->photo->orientation);
		}
		else
			rsi = rs->photo->input;
		pixbuf = gdk_pixbuf_new(GDK_COLORSPACE_RGB, FALSE, 8, rsi->w, rsi->h);
		rs_render(rs->photo, rsi->w, rsi->h, rsi->pixels,
			rsi->rowstride, rsi->channels,
			gdk_pixbuf_get_pixels(pixbuf), gdk_pixbuf_get_rowstride(pixbuf));

		/* actually save */
		n = gtk_combo_box_get_active(GTK_COMBO_BOX(filetype));
		rs_conf_set_string(CONF_SAVE_FILETYPE, savers[n].extension);
		savers[n].func(pixbuf, filename);

		gtk_widget_destroy(fc);
		if (rs->photo->orientation)
			rs_image16_free(rsi);
		g_object_unref(pixbuf);
		g_free (filename);
		gui_status_push(_("File saved"));
	}
	else
	{
		gtk_widget_destroy(fc);
		gui_status_push(_("File save canceled"));
	}
	g_free(dirname);
	g_free(basename);
	g_string_free(name, TRUE);
	return;
}

void
gui_reset_current_settings_callback(RS_BLOB *rs)
{
	gboolean in_use = rs->in_use;
	rs->in_use = FALSE;
	rs_settings_reset(rs->settings[rs->current_setting], MASK_ALL);
	rs->in_use = in_use;
	update_preview(rs);
	return;
}

void
gui_menu_quit(gpointer callback_data, guint callback_action, GtkWidget *widget)
{
	RS_BLOB *rs = (RS_BLOB *) callback_data;
	rs_shutdown(NULL, NULL, rs);
	return;
}

void
gui_menu_show_exposure_mask_callback(gpointer callback_data, guint callback_action, GtkWidget *widget)
{
	RS_BLOB *rs = (RS_BLOB *) callback_data;
	if (GTK_CHECK_MENU_ITEM(widget)->active)
	  gui_status_push(_("Showing exposure mask"));
	else
	  gui_status_push(_("Hiding exposure mask"));
	rs->show_exposure_overlay = GTK_CHECK_MENU_ITEM(widget)->active;
	update_preview(rs);
	return;
}

void
gui_menu_copy_callback(gpointer callback_data, guint callback_action, GtkWidget *widget)
{
	RS_BLOB *rs = (RS_BLOB *) callback_data;

	if (rs->in_use) 
	{
		if (!rs->settings_buffer)
			rs->settings_buffer = g_malloc(sizeof(RS_SETTINGS_DOUBLE));

		rs->settings_buffer->exposure = GETVAL(rs->settings[rs->photo->current_setting]->exposure);
		rs->settings_buffer->saturation = GETVAL(rs->settings[rs->photo->current_setting]->saturation);
		rs->settings_buffer->hue = GETVAL(rs->settings[rs->photo->current_setting]->hue);
		rs->settings_buffer->contrast = GETVAL(rs->settings[rs->photo->current_setting]->contrast);
		rs->settings_buffer->warmth = GETVAL(rs->settings[rs->photo->current_setting]->warmth);
		rs->settings_buffer->tint = GETVAL(rs->settings[rs->photo->current_setting]->tint);

		gui_status_push(_("Copied settings"));
	}
	return;
}

void
gui_menu_paste_callback(gpointer callback_data, guint callback_action, GtkWidget *widget)
{
	RS_BLOB *rs = (RS_BLOB *) callback_data;
	gint mask;
	
	GtkWidget *dialog, *cb_box;
	GtkWidget *cb_exposure, *cb_saturation, *cb_hue, *cb_contrast, *cb_whitebalance;

	if (rs->in_use)
	{
		if (rs->settings_buffer)
		{
			cb_exposure = gtk_check_button_new_with_label (_("Exposure"));
			cb_saturation = gtk_check_button_new_with_label (_("Saturation"));
			cb_hue = gtk_check_button_new_with_label (_("Hue"));
			cb_contrast = gtk_check_button_new_with_label (_("Contrast"));
			cb_whitebalance = gtk_check_button_new_with_label (_("Whitebalance"));

			rs_conf_get_integer(CONF_PASTE_MASK, &mask);

			if (mask & MASK_EXPOSURE)
				gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(cb_exposure), TRUE);
			if (mask & MASK_SATURATION)
				gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(cb_saturation), TRUE);
			if (mask & MASK_HUE)
				gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(cb_hue), TRUE);
			if (mask & MASK_CONTRAST)
				gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(cb_contrast), TRUE);
			if (mask & MASK_WARMTH && mask & MASK_TINT)
				gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(cb_whitebalance), TRUE);

			cb_box = gtk_vbox_new(FALSE, 0);
			
			gtk_box_pack_start (GTK_BOX (cb_box), cb_exposure, FALSE, TRUE, 0);
			gtk_box_pack_start (GTK_BOX (cb_box), cb_saturation, FALSE, TRUE, 0);
			gtk_box_pack_start (GTK_BOX (cb_box), cb_hue, FALSE, TRUE, 0);
			gtk_box_pack_start (GTK_BOX (cb_box), cb_contrast, FALSE, TRUE, 0);
			gtk_box_pack_start (GTK_BOX (cb_box), cb_whitebalance, FALSE, TRUE, 0);
						
			dialog = gui_dialog_make_from_widget(GTK_STOCK_DIALOG_QUESTION, _("Select settings to paste"), cb_box);

			gtk_dialog_add_buttons(GTK_DIALOG(dialog), GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL, GTK_STOCK_APPLY, GTK_RESPONSE_APPLY, NULL);
			gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_APPLY);

			gtk_widget_show_all(dialog);
		
			mask=0;
		
			if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_APPLY)
			{
				if (GTK_TOGGLE_BUTTON(cb_exposure)->active)
					mask |= MASK_EXPOSURE;
				if (GTK_TOGGLE_BUTTON(cb_saturation)->active)
					mask |= MASK_SATURATION;
				if (GTK_TOGGLE_BUTTON(cb_hue)->active)
					mask |= MASK_HUE;
				if (GTK_TOGGLE_BUTTON(cb_contrast)->active)
					mask |= MASK_CONTRAST;
				if (GTK_TOGGLE_BUTTON(cb_whitebalance)->active)
					mask |= MASK_WB;
				rs_conf_set_integer(CONF_PASTE_MASK, mask);
    		}
  			gtk_widget_destroy (dialog);
			
			if(mask > 0)
			{
				gboolean in_use = rs->in_use;
				rs->in_use = FALSE;
				rs_apply_settings_from_double(rs->settings[rs->photo->current_setting], rs->settings_buffer, mask);
				rs->in_use = in_use;
				update_preview(rs);

				gui_status_push(_("Pasted settings"));
			}
			else
				gui_status_push(_("Nothing to paste!"));
		}
		else 
			gui_status_push(_("Buffer empty!"));
	}
	return;
}

GtkWidget *
gui_make_menubar(RS_BLOB *rs, GtkWidget *window, GtkListStore *store, GtkWidget *iconbox, GtkWidget *toolbox)
{
	GtkItemFactoryEntry menu_items[] = {
		{ _("/_File"), NULL, NULL, 0, "<Branch>"},
		{ _("/File/_Open..."), "<CTRL>O", gui_menu_open_callback, (gint) store, "<StockItem>", GTK_STOCK_OPEN},
		{ _("/File/_Save as..."), "<CTRL>S", gui_save_file_callback, (gint) store, "<StockItem>", GTK_STOCK_SAVE_AS},
		{ _("/File/_Reload"), "<CTRL>R", gui_menu_reload_callback, (gint) store, "<StockItem>", GTK_STOCK_REFRESH},
		{ _("/File/_Purge delete priority"), "<CTRL><SHIFT>D", gui_menu_purge_d_callback, 0, "<StockItem>", GTK_STOCK_DELETE},
		{ _("/File/_Quit"), "<CTRL>Q", gui_menu_quit, 0, "<StockItem>", GTK_STOCK_QUIT},
		{ _("/_Edit"), NULL, NULL, 0, "<Branch>"},
		{ _("/_Edit/_Copy settings"),  "<CTRL>C", gui_menu_copy_callback, 0, "<StockItem>", GTK_STOCK_COPY},
		{ _("/_Edit/_Paste settings"),  "<CTRL>V", gui_menu_paste_callback, 0, "<StockItem>", GTK_STOCK_PASTE},
		{ _("/_Edit/_Reset current settings"), NULL , gui_reset_current_settings_callback, (gint) store},
		{ _("/_Edit/_Delete photo"),  "Delete", gui_menu_setprio_callback, PRIO_D, "<StockItem>", GTK_STOCK_DELETE},
		{ _("/_Edit/_Set priority/_1"),  "1", gui_menu_setprio_callback, PRIO_1},
		{ _("/_Edit/_Set priority/_2"),  "2", gui_menu_setprio_callback, PRIO_2},
		{ _("/_Edit/_Set priority/_3"),  "3", gui_menu_setprio_callback, PRIO_3},
		{ _("/_Edit/_Set priority/_Remove priority"),  "0", gui_menu_setprio_callback, PRIO_U},
		{ _("/_Edit/_White balance/_Auto"), "A", gui_menu_auto_wb_callback, 0 },
		{ _("/_Edit/_White balance/_Camera"), "C", gui_menu_cam_wb_callback, 0 },
		{ _("/_Edit/_Preferences"), NULL, gui_menu_preference_callback, 0, "<StockItem>", GTK_STOCK_PREFERENCES},
		{ _("/_View"), NULL, NULL, 0, "<Branch>"},
		{ _("/_View/_Previous image"), "<CTRL>Left", gui_menu_prevnext_callback, 1, "<StockItem>", GTK_STOCK_GO_BACK},
		{ _("/_View/_Next image"), "<CTRL>Right", gui_menu_prevnext_callback, 2, "<StockItem>", GTK_STOCK_GO_FORWARD},
		{ _("/_View/sep1"), NULL, NULL, 0, "<Separator>"},
		{ _("/_View/_Zoom in"), NULL, gui_menu_zoom_callback, 1, "<StockItem>", GTK_STOCK_ZOOM_IN},
		{ _("/_View/_Zoom out"), NULL, gui_menu_zoom_callback, 2, "<StockItem>", GTK_STOCK_ZOOM_OUT},
		{ _("/_View/_Zoom to fit"), NULL, gui_menu_zoom_callback, 0, "<StockItem>", GTK_STOCK_ZOOM_FIT},
		{ _("/_View/_Zoom to 100%"), NULL, gui_menu_zoom_callback, 100, "<StockItem>", GTK_STOCK_ZOOM_100},
		{ _("/_View/sep2"), NULL, NULL, 0, "<Separator>"},
		{ _("/_View/_Icon Box"), "<CTRL>I", gui_menu_widget_visible_callback, (gint) iconbox},
		{ _("/_View/_Tool Box"), "<CTRL>T", gui_menu_widget_visible_callback, (gint) toolbox},
		{ _("/_View/sep3"), NULL, NULL, 0, "<Separator>"},
#if GTK_CHECK_VERSION(2,8,0)
		{ _("/_View/_Fullscreen"), "F11", gui_menu_fullscreen_callback, (gint) window, "<StockItem>", GTK_STOCK_FULLSCREEN},
#else
		{ _("/_View/_Fullscreen"), "F11", gui_menu_fullscreen_callback, (gint) window},
#endif
		{ _("/_View/sep1"), NULL, NULL, 0, "<Separator>"},
		{ _("/_View/_Show exposure mask"), "<CTRL>E", gui_menu_show_exposure_mask_callback, 0, "<ToggleItem>"},
//		{ _("/_Batch"), NULL, NULL, 0, "<Branch>"},
//		{ _("/_Batch/_Add to batch queue"),  "<CTRL>B", gui_menu_add_to_batch_queue_callback, 0 , "<StockItem>", GTK_STOCK_ADD},
//		{ _("/_Batch/_Add current view to queue"), NULL, gui_menu_add_view_to_batch_queue_callback, 0 },
//		{ _("/_Batch/_Remove from batch queue"),  "<CTRL><ALT>B", gui_menu_remove_from_batch_queue_callback, 0 , "<StockItem>", GTK_STOCK_REMOVE},
//		{ _("/_Batch/_Run!"), NULL, gui_menu_batch_run_queue_callback, 0 },
		{ _("/_Help"), NULL, NULL, 0, "<LastBranch>"},
		{ _("/_Help/About"), NULL, gui_about, 0, "<StockItem>", GTK_STOCK_ABOUT},
	};
	static gint nmenu_items = sizeof (menu_items) / sizeof (menu_items[0]);
	GtkItemFactory *item_factory;
	GtkAccelGroup *accel_group;

	accel_group = gtk_accel_group_new ();
	item_factory = gtk_item_factory_new (GTK_TYPE_MENU_BAR, "<main>", accel_group);
	gtk_item_factory_create_items (item_factory, nmenu_items, menu_items, (gpointer) rs);
	gtk_window_add_accel_group (GTK_WINDOW (window), accel_group);
	return(gtk_item_factory_get_widget (item_factory, "<main>"));
}

GtkWidget *
gui_window_make(RS_BLOB *rs)
{
	GtkWidget *window;
	window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	gtk_window_resize((GtkWindow *) window, 800, 600);
	gtk_window_set_title (GTK_WINDOW (window), _("Rawstudio"));
	g_signal_connect((gpointer) window, "delete_event", G_CALLBACK(rs_shutdown), rs);
	return(window);
}

GtkWidget *
gui_dialog_make_from_text(const gchar *stock_id, gchar *primary_text, gchar *secondary_text)
{
	GtkWidget *secondary_label;

	secondary_label = gtk_label_new (NULL);
	gtk_label_set_line_wrap (GTK_LABEL (secondary_label), TRUE);
	gtk_label_set_use_markup (GTK_LABEL (secondary_label), TRUE);
	gtk_misc_set_alignment (GTK_MISC (secondary_label), 0.0, 0.5);
	gtk_label_set_selectable (GTK_LABEL (secondary_label), TRUE);
	gtk_label_set_markup (GTK_LABEL (secondary_label), secondary_text);
	
	return(gui_dialog_make_from_widget(stock_id, primary_text, secondary_label));
}

GtkWidget *
gui_dialog_make_from_widget(const gchar *stock_id, gchar *primary_text, GtkWidget *widget)
{
	GtkWidget *dialog, *image, *hbox, *vbox;
	GtkWidget *primary_label;
	gchar *str;

	image = gtk_image_new_from_stock(stock_id, GTK_ICON_SIZE_DIALOG);
	gtk_misc_set_alignment (GTK_MISC (image), 0.5, 0.0);
	dialog = gtk_dialog_new();
	gtk_container_set_border_width (GTK_CONTAINER (dialog), 5);
	gtk_box_set_spacing (GTK_BOX (GTK_DIALOG (dialog)->vbox), 14);
	gtk_window_set_resizable (GTK_WINDOW (dialog), FALSE);
	gtk_dialog_set_has_separator (GTK_DIALOG (dialog), FALSE);
	gtk_window_set_title (GTK_WINDOW (dialog), "");
	gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);
	gtk_window_set_destroy_with_parent (GTK_WINDOW (dialog), TRUE);

	primary_label = gtk_label_new (NULL);
	gtk_label_set_line_wrap (GTK_LABEL (primary_label), TRUE);
	gtk_label_set_use_markup (GTK_LABEL (primary_label), TRUE);
	gtk_misc_set_alignment (GTK_MISC (primary_label), 0.0, 0.5);
	gtk_label_set_selectable (GTK_LABEL (primary_label), TRUE);
	str = g_strconcat("<span weight=\"bold\" size=\"larger\">", primary_text, "</span>", NULL);
	gtk_label_set_markup (GTK_LABEL (primary_label), str);
	g_free(str);

	hbox = gtk_hbox_new (FALSE, 12);
	gtk_container_set_border_width (GTK_CONTAINER (hbox), 5);
	gtk_box_pack_start (GTK_BOX (hbox), image, FALSE, FALSE, 0);
	vbox = gtk_vbox_new (FALSE, 12);
	gtk_box_pack_start (GTK_BOX (hbox), vbox, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (vbox), primary_label, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (vbox), widget, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox), hbox, FALSE, FALSE, 0);

	return(dialog);
}

int
gui_init(int argc, char **argv)
{
	GtkWidget *window;
	GtkWidget *vbox;
	GtkWidget *pane;
	GtkWidget *toolbox;
	GtkWidget *iconbox;
	GtkWidget *preview;
	GtkListStore *store;
	GtkWidget *menubar;
	RS_BLOB *rs;
	gchar *lwd = NULL;
	GtkTreePath *path;
	GtkTreePath *openpath = NULL;
	GtkTreeIter iter;
	gchar *name;
	gchar *filename;

	gtk_init(&argc, &argv);
	
	rs = rs_new();
	window = gui_window_make(rs);
	statusbar = (GtkStatusbar *) gtk_statusbar_new();
	toolbox = make_toolbox(rs);

	store = gtk_list_store_new (NUM_COLUMNS, GDK_TYPE_PIXBUF, G_TYPE_STRING,
		G_TYPE_STRING, G_TYPE_POINTER, G_TYPE_INT);
	iconbox = make_iconbox(rs, store);
	g_signal_connect((gpointer) window, "window-state-event", G_CALLBACK(gui_fullscreen_callback), iconbox);

	menubar = gui_make_menubar(rs, window, store, iconbox, toolbox);
	preview = gui_drawingarea_make(rs);

	pane = gtk_hpaned_new ();

	gtk_paned_pack1 (GTK_PANED (pane), preview, TRUE, TRUE);
	gtk_paned_pack2 (GTK_PANED (pane), toolbox, FALSE, TRUE);

	vbox = gtk_vbox_new (FALSE, 0);
	gtk_container_add (GTK_CONTAINER (window), vbox);

	gtk_box_pack_start (GTK_BOX (vbox), menubar, FALSE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (vbox), iconbox, FALSE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (vbox), pane, TRUE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (vbox), GTK_WIDGET (statusbar), FALSE, TRUE, 0);

	gui_status_push(_("Ready"));

	gtk_widget_show_all (window);

	if (argc > 1)
	{	
		gchar *abspath;
		gchar *temppath = g_strdup(argv[1]);

		if (g_path_is_absolute(temppath))
			abspath = g_strdup(temppath);
		else
		{
			gchar *tmpdir = g_get_current_dir ();
			abspath = g_build_filename (tmpdir, temppath, NULL);
			g_free (tmpdir);
		}
		g_free(temppath);

		if (g_file_test(abspath, G_FILE_TEST_IS_DIR))
			fill_model(store, abspath);
		else if (g_file_test(abspath, G_FILE_TEST_IS_REGULAR))
		{
			lwd = g_path_get_dirname(abspath);
			filename = g_path_get_basename(abspath);
			fill_model(store, lwd);
			{
				path = gtk_tree_path_new_first();
				while(gtk_tree_model_get_iter(GTK_TREE_MODEL(store), &iter, path))
				{
					gtk_tree_model_get(GTK_TREE_MODEL(store), &iter, TEXT_COLUMN, &name, -1);
					if(g_utf8_collate(filename, name) == 0)
					{
						openpath = gtk_tree_path_copy(path);
						g_free(name);
						break;
					}
					else
						g_free(name);
					gtk_tree_path_next(path);
				}
				if (openpath)
				{
					gtk_icon_view_select_path((GtkIconView *) current_iconview, openpath);
					gtk_tree_path_free(openpath);
				}
				gtk_tree_path_free(path);
			}
			g_free(lwd);
		}
		else
			fill_model(store, NULL);
		g_free(abspath);
	}
	else
	{
		lwd = rs_conf_get_string(CONF_LWD);
		if (!lwd)
			lwd = g_get_current_dir();
		fill_model(store, lwd);
		g_free(lwd);
	}
	gtk_main();
	return(0);
}
