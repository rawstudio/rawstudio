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

#include <glib.h>
#include <gtk/gtk.h>
#include <glib/gstdio.h>
#include <gdk/gdkkeysyms.h>
#include <config.h>
#include "color.h"
#include "rawstudio.h"
#include "gtk-helper.h"
#include "gtk-interface.h"
#include "gtk-save-dialog.h"
#include "gtk-progress.h"
#include "toolbox.h"
#include "conf_interface.h"
#include "rs-cache.h"
#include "rs-image.h"
#include "rs-batch.h"
#include "gettext.h"
#include "rs-batch.h"
#include "rs-cms.h"
#include <string.h>
#include <unistd.h>
#include "filename.h"
#include "rs-store.h"
#include "rs-preview-widget.h"
#include "rs-histogram.h"
#include "rs-preload.h"
#include "rs-photo.h"
#include "rs-external-editor.h"
#include "rs-actions.h"
#include "rs-dir-selector.h"

static gchar *filenames[] = {DEFAULT_CONF_EXPORT_FILENAME, "%f", "%f_%c", "%f_output_%4c", NULL};
static GtkStatusbar *statusbar;
static gboolean fullscreen;
GtkWindow *rawstudio_window;
static gint busycount = 0;
static GtkWidget *valuefield;
static GtkWidget *hbox;
GdkGC *dashed;
GdkGC *grid;

static void gui_preview_bg_color_changed(GtkColorButton *widget, RS_BLOB *rs);
static gboolean gui_fullscreen_iconbox_callback(GtkWidget *widget, GdkEventWindowState *event, GtkWidget *iconbox);
static gboolean gui_fullscreen_toolbox_callback(GtkWidget *widget, GdkEventWindowState *event, GtkWidget *toolbox);
static void gui_preference_iconview_show_filenames_changed(GtkToggleButton *togglebutton, gpointer user_data);
static GtkWidget *gui_make_menubar(RS_BLOB *rs);
static void drag_data_received(GtkWidget *widget, GdkDragContext *drag_context, gint x, gint y, GtkSelectionData *selection_data, guint info, guint t,	RS_BLOB *rs);
static GtkWidget *gui_window_make(RS_BLOB *rs);
static void rs_open_file_delayed(RS_BLOB *rs, const gchar *filename);
static void rs_open_file(RS_BLOB *rs, const gchar *filename);
static gboolean pane_position(GtkWidget* widget, gpointer dummy, gpointer user_data);

void
gui_set_busy(gboolean rawstudio_is_busy)
{
	static guint status = 0;

	if (rawstudio_is_busy)
		busycount++;
	else
		busycount--;

	if (busycount<0)
		busycount = 0;

	if (busycount)
	{
		if (status==0)
			status = gui_status_push(_("Background renderer active"));
	}
	else
	{
		if (status>0)
			gui_status_pop(status);
		status=0;
	}
	return;
}

gboolean
gui_is_busy(void)
{
	if (busycount)
		return(TRUE);
	else
		return(FALSE);
}

static gboolean
gui_statusbar_remove_helper(guint *msgid)
{
	gdk_threads_enter();
	gtk_statusbar_remove(statusbar, gtk_statusbar_get_context_id(statusbar, "generic"), *msgid);
	g_free(msgid);
	gdk_threads_leave();
	return(FALSE);
}

void
gui_status_notify(const char *text)
{
	guint *msgid;
	msgid = g_new(guint, 1);
	*msgid = gtk_statusbar_push(statusbar, gtk_statusbar_get_context_id(statusbar, "generic"), text);
	g_timeout_add(3000, (GSourceFunc) gui_statusbar_remove_helper, msgid);
	return;
}

guint
gui_status_push(const char *text)
{
	guint msgid;
	msgid = gtk_statusbar_push(statusbar, gtk_statusbar_get_context_id(statusbar, "generic"), text);
	return(msgid);
}

void
gui_status_pop(const guint msgid)
{
	gtk_statusbar_remove(statusbar, gtk_statusbar_get_context_id(statusbar, "generic"), msgid);
	return;
}

gboolean
update_preview_callback(GtkAdjustment *do_not_use_this, RS_BLOB *rs)
{
	if (rs->photo)
	{
		rs_photo_apply_settings(rs->photo, rs->current_setting, rs->settings[rs->current_setting], MASK_ALL);
	}
	return(FALSE);
}

static void
icon_activated(gpointer instance, const gchar *name, RS_BLOB *rs)
{
	RS_FILETYPE *filetype;
	RS_PHOTO *photo;
	extern GtkLabel *infolabel;
	GString *label;
	gboolean cache_loaded;
	guint msgid;

	if (name!=NULL)
	{
		GString *window_title;
		GList *selected = NULL;
		g_signal_handlers_block_by_func(instance, icon_activated, rs);
		gui_set_busy(TRUE);
		msgid = gui_status_push(_("Opening photo ..."));
		GTK_CATCHUP();
		g_signal_handlers_unblock_by_func(instance, icon_activated, rs);

		/* Read currently selected filename, it may or may not (!) be the same as served in name */
		selected = rs_store_get_selected_names(rs->store);
		if (g_list_length(selected)==1)
		{
			name = g_list_nth_data(selected, 0);
			g_list_free(selected);
		}

		if ((filetype = rs_filetype_get(name, TRUE)))
		{
			rs_preview_widget_set_photo(RS_PREVIEW_WIDGET(rs->preview), NULL);
			photo = rs_get_preloaded(name);
			if (!photo)
				photo = filetype->load(name, FALSE);
			if (photo)
			{
				rs_photo_close(rs->photo);
			}
			else
			{
				gui_status_pop(msgid);
				gui_status_notify(_("Couldn't open photo"));
				gui_set_busy(FALSE);
				return;
			}

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
				if (photo->metadata->focallength>0)
					g_string_append_printf(label, _("%dmm "), photo->metadata->focallength);
				if (photo->metadata->shutterspeed > 0.0 && photo->metadata->shutterspeed < 4) 
					g_string_append_printf(label, _("%.1fs "), 1/photo->metadata->shutterspeed);
				else if (photo->metadata->shutterspeed >= 4)
					g_string_append_printf(label, _("1/%.0fs "), photo->metadata->shutterspeed);
				if (photo->metadata->aperture!=0.0)
					g_string_append_printf(label, _("F/%.1f "), photo->metadata->aperture);
				if (photo->metadata->iso!=0)
					g_string_append_printf(label, _("ISO%d"), photo->metadata->iso);
				gtk_label_set_text(infolabel, label->str);
				g_string_free(label, TRUE);
			} else
				gtk_label_set_text(infolabel, _("No metadata"));

			cache_loaded = rs_cache_load(photo);

			rs_set_photo(rs, photo);

			if (!cache_loaded)
			{
				gint c;
				for (c=0;c<3;c++)
				{
					/* White balance */
					if (!rs_photo_set_wb_from_camera(rs->photo, c))
						rs_photo_set_wb_auto(rs->photo, c);

					/* Contrast */
					if (rs->photo->metadata->contrast != -1.0)
						rs_photo_set_contrast(rs->photo, c, rs->photo->metadata->contrast);

					/* Saturation */
					if (rs->photo->metadata->saturation != -1.0)
						rs_photo_set_saturation(rs->photo, c, rs->photo->metadata->saturation);
				}
			}

		}
		gui_status_pop(msgid);
		gui_status_notify(_("Image opened"));
		window_title = g_string_new(_("Rawstudio"));
		g_string_append(window_title, " - ");
		g_string_append(window_title, rs->photo->filename);
		gtk_window_set_title(GTK_WINDOW(rawstudio_window), window_title->str);
		g_string_free(window_title, TRUE);
	}
	gui_set_busy(FALSE);
}

static void
gui_preview_bg_color_changed(GtkColorButton *widget, RS_BLOB *rs)
{
	GdkColor color;
	gtk_color_button_get_color(GTK_COLOR_BUTTON(widget), &color);
	rs_preview_widget_set_bgcolor(RS_PREVIEW_WIDGET(rs->preview), &color);
	rs_conf_set_color(CONF_PREBGCOLOR, &color);
	return;
}

/**
 * Change priority on all selected and currently opened photos
 */
void
gui_setprio(RS_BLOB *rs, guint prio)
{
	GList *selected = NULL;
	gint i, num_selected;
	GString *gs;

	selected = rs_store_get_selected_iters(rs->store);
	num_selected = g_list_length(selected);

	/* Iterate throuh all selected thumbnails */
	for(i=0;i<num_selected;i++)
	{
		rs_store_set_flags(rs->store, NULL, g_list_nth_data(selected, i), &prio, NULL);
	}
	g_list_free(selected);

	/* Change priority for currently open photo */
	if (rs->photo)
	{
		rs->photo->priority = prio;
		rs_store_set_flags(rs->store, rs->photo->filename, NULL, &prio, NULL);
	}

	/* Generate text for statusbar notification */
	gs = g_string_new(NULL);

	if (prio == 0)
		g_string_printf(gs, _("Changed photo priority (*)"));
	else if (prio == 51)
		g_string_printf(gs, _("Changed photo priority (D)"));
	else
		g_string_printf(gs, _("Changed photo priority (%d)"),prio);
	gui_status_notify(gs->str);

	g_string_free(gs, TRUE);
}

void
gui_widget_show(GtkWidget *widget, gboolean show, const gchar *conf_fullscreen_key, const gchar *conf_windowed_key)
{
	if (show)
	{
		gtk_widget_show(widget);
		if (fullscreen)
			rs_conf_set_boolean(conf_fullscreen_key, TRUE);
		else
			rs_conf_set_boolean(conf_windowed_key, TRUE);
	}
	else
	{
		gtk_widget_hide(widget);
		if (fullscreen)
			rs_conf_set_boolean(conf_fullscreen_key, FALSE);
		else
			rs_conf_set_boolean(conf_windowed_key, FALSE);
	}
	return;
}

static gboolean
gui_fullscreen_iconbox_callback(GtkWidget *widget, GdkEventWindowState *event, GtkWidget *iconbox)
{
	gboolean show_iconbox;
	if (event->new_window_state & GDK_WINDOW_STATE_FULLSCREEN)
	{
		rs_conf_get_boolean_with_default(CONF_SHOW_ICONBOX_FULLSCREEN, &show_iconbox, DEFAULT_CONF_SHOW_ICONBOX_FULLSCREEN);
		fullscreen = TRUE;
		gui_widget_show(iconbox, show_iconbox, CONF_SHOW_ICONBOX_FULLSCREEN, CONF_SHOW_ICONBOX);
	}
	if (!(event->new_window_state & GDK_WINDOW_STATE_FULLSCREEN))
	{
		rs_conf_get_boolean_with_default(CONF_SHOW_ICONBOX, &show_iconbox, DEFAULT_CONF_SHOW_ICONBOX);
		fullscreen = FALSE;
		gui_widget_show(iconbox, show_iconbox, CONF_SHOW_ICONBOX_FULLSCREEN, CONF_SHOW_ICONBOX);
	}
	return(FALSE);
}

static gboolean
gui_fullscreen_toolbox_callback(GtkWidget *widget, GdkEventWindowState *event, GtkWidget *toolbox)
{
	gboolean show_toolbox;
	if (event->new_window_state & GDK_WINDOW_STATE_FULLSCREEN)
	{
		rs_conf_get_boolean_with_default(CONF_SHOW_TOOLBOX_FULLSCREEN, &show_toolbox, DEFAULT_CONF_SHOW_TOOLBOX_FULLSCREEN);
		fullscreen = TRUE;
		gui_widget_show(toolbox, show_toolbox, CONF_SHOW_TOOLBOX_FULLSCREEN, CONF_SHOW_TOOLBOX);
	}
	if (!(event->new_window_state & GDK_WINDOW_STATE_FULLSCREEN))
	{
		rs_conf_get_boolean_with_default(CONF_SHOW_TOOLBOX, &show_toolbox, DEFAULT_CONF_SHOW_TOOLBOX);
		fullscreen = FALSE;
		gui_widget_show(toolbox, show_toolbox, CONF_SHOW_TOOLBOX_FULLSCREEN, CONF_SHOW_TOOLBOX);
	}
	return(FALSE);
}

static gboolean
gui_histogram_height_changed(GtkAdjustment *caller, RS_BLOB *rs)
{
	const gint newheight = (gint) caller->value;
	gtk_widget_set_size_request(rs->histogram, 64, newheight);
	rs_conf_set_integer(CONF_HISTHEIGHT, newheight);
	return(FALSE);
}

static void
gui_export_filetype_combobox_changed(gpointer active, gpointer user_data)
{
	GtkLabel *label = GTK_LABEL(user_data);
	gui_export_changed_helper(label);

	return;
}

static void
gui_preference_iconview_show_filenames_changed(GtkToggleButton *togglebutton, gpointer user_data)
{
	RS_BLOB *rs = (RS_BLOB *)user_data;

	rs_store_set_show_filenames(rs->store, togglebutton->active);

	return;
}

static void
gui_preference_use_dark_theme(GtkToggleButton *togglebutton, gpointer user_data)
{
	if (togglebutton->active)
	{
		gui_select_theme(DARK_THEME);
	}
	else
	{
		gui_select_theme(STANDARD_GTK_THEME);
	}
}

static void
gui_preference_preload_changed(GtkToggleButton *togglebutton, gpointer user_data)
{
	if (togglebutton->active)
		rs_preload_set_maximum_memory(200*1024*1024);
	else
		rs_preload_set_maximum_memory(0);

	return;
}

void
gui_make_preference_window(RS_BLOB *rs)
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
	GtkWidget *local_cache_check;
	GtkWidget *dark_theme_check;
	GtkWidget *load_gdk_check;
	GtkWidget *preload_check;
	GtkWidget *show_filenames;

/*
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
*/

	GtkWidget *export_page;
	GtkWidget *export_directory_hbox;
	GtkWidget *export_directory_label;
	GtkWidget *export_directory_entry;
	GtkWidget *export_filename_hbox;
	GtkWidget *export_filename_label;
	GtkWidget *export_filename_entry;
	GtkWidget *export_filename_example_hbox;
	GtkWidget *export_filename_example_label1;
	GtkWidget *export_filename_example_label2;
	GtkWidget *export_filetype_hbox;
	GtkWidget *export_filetype_label;
	RS_CONFBOX *export_filetype_confbox;
	GtkWidget *export_hsep = gtk_hseparator_new();
	GtkWidget *export_tiff_uncompressed_check;
	
	RS_FILETYPE *filetype;

	GtkWidget *cms_page;

	gchar *conf_temp = NULL;
	gint n;

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

	show_filenames = checkbox_from_conf(CONF_SHOW_FILENAMES, _("Show filenames in iconview"), TRUE);
	gtk_box_pack_start (GTK_BOX (preview_page), show_filenames, FALSE, TRUE, 0);
	g_signal_connect ((gpointer) show_filenames, "toggled",
		G_CALLBACK (gui_preference_iconview_show_filenames_changed), rs);

	dark_theme_check = checkbox_from_conf(CONF_USE_DARK_THEME, _("Use dark theme"), TRUE);
	gtk_box_pack_start (GTK_BOX (preview_page), dark_theme_check, FALSE, TRUE, 0);
	g_signal_connect ((gpointer) dark_theme_check, "toggled",
		G_CALLBACK (gui_preference_use_dark_theme), rs);

	gtk_box_pack_start (GTK_BOX (preview_page), gtk_hseparator_new(), FALSE, TRUE, 0);

	local_cache_check = checkbox_from_conf(CONF_CACHEDIR_IS_LOCAL, _("Place cache in home directory"), FALSE);
	gtk_box_pack_start (GTK_BOX (preview_page), local_cache_check, FALSE, TRUE, 0);

	load_gdk_check = checkbox_from_conf(CONF_LOAD_GDK, _("Load 8 bit photos (jpeg, png, etc)"), FALSE);
	gtk_box_pack_start (GTK_BOX (preview_page), load_gdk_check, FALSE, TRUE, 0);

	preload_check = checkbox_from_conf(CONF_PRELOAD, _("Preload photos"), FALSE);
#ifdef EXPERIMENTAL
	gtk_box_pack_start (GTK_BOX (preview_page), preload_check, FALSE, TRUE, 0);
#endif
	g_signal_connect ((gpointer) preload_check, "toggled",
		G_CALLBACK (gui_preference_preload_changed), rs);

/*
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
*/
	
	export_page = gtk_vbox_new(FALSE, 4);
	gtk_container_set_border_width (GTK_CONTAINER (export_page), 6);

	export_directory_hbox = gtk_hbox_new(FALSE, 0);
	export_directory_label = gtk_label_new(_("Directory:"));
	gtk_misc_set_alignment(GTK_MISC(export_directory_label), 0.0, 0.5);
	export_directory_entry = gtk_entry_new();
	conf_temp = rs_conf_get_string(CONF_EXPORT_DIRECTORY);

	if (conf_temp)
	{
		gtk_entry_set_text(GTK_ENTRY(export_directory_entry), conf_temp);
		g_free(conf_temp);
	}
	gtk_box_pack_start (GTK_BOX (export_directory_hbox), export_directory_label, TRUE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (export_directory_hbox), export_directory_entry, FALSE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (export_page), export_directory_hbox, FALSE, TRUE, 0);


	export_filename_hbox = gtk_hbox_new(FALSE, 0);
	export_filename_label = gtk_label_new(_("Filename:"));
	gtk_misc_set_alignment(GTK_MISC(export_filename_label), 0.0, 0.5);
	export_filename_entry = gtk_combo_box_entry_new_text();
	conf_temp = rs_conf_get_string(CONF_EXPORT_FILENAME);

	if (!conf_temp)
	{
		rs_conf_set_string(CONF_EXPORT_FILENAME, DEFAULT_CONF_EXPORT_FILENAME);
		conf_temp = rs_conf_get_string(CONF_EXPORT_FILENAME);
	}

	gtk_combo_box_append_text(GTK_COMBO_BOX(export_filename_entry), conf_temp);

	n=0;
	while(filenames[n])
	{
		gtk_combo_box_append_text(GTK_COMBO_BOX(export_filename_entry), filenames[n]);	
		n++;
	}
	gtk_combo_box_set_active(GTK_COMBO_BOX(export_filename_entry), 0);
	g_free(conf_temp);
	gtk_box_pack_start (GTK_BOX (export_filename_hbox), export_filename_label, TRUE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (export_filename_hbox), export_filename_entry, FALSE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (export_page), export_filename_hbox, FALSE, TRUE, 0);

	export_filetype_hbox = gtk_hbox_new(FALSE, 0);
	export_filetype_label = gtk_label_new(_("File type:"));
	gtk_misc_set_alignment(GTK_MISC(export_filetype_label), 0.0, 0.5);

	if (!rs_conf_get_filetype(CONF_EXPORT_FILETYPE, &filetype))
		rs_conf_set_filetype(CONF_EXPORT_FILETYPE, filetype); /* set default */

	export_filename_example_label1 = gtk_label_new(_("Filename example:"));
	export_filename_example_label2 = gtk_label_new(NULL);
	export_filetype_confbox = gui_confbox_filetype_new(CONF_EXPORT_FILETYPE);

	export_tiff_uncompressed_check = checkbox_from_conf(CONF_EXPORT_TIFF_UNCOMPRESSED, _("Save uncompressed TIFF"), FALSE);

	gtk_box_pack_start (GTK_BOX (export_filetype_hbox), export_filetype_label, TRUE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (export_filetype_hbox), gui_confbox_get_widget(export_filetype_confbox), FALSE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (export_page), export_filetype_hbox, FALSE, TRUE, 0);

	export_filename_example_hbox = gtk_hbox_new(FALSE, 0);
	gui_export_changed_helper(GTK_LABEL(export_filename_example_label2));
	gtk_misc_set_alignment(GTK_MISC(export_filename_example_label1), 0.0, 0.5);
	gtk_box_pack_start (GTK_BOX (export_filename_example_hbox), export_filename_example_label1, TRUE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (export_filename_example_hbox), export_filename_example_label2, TRUE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (export_page), export_filename_example_hbox, FALSE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (export_page), export_hsep, FALSE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (export_page), export_tiff_uncompressed_check, FALSE, TRUE, 0);

	g_signal_connect ((gpointer) export_directory_entry, "changed", 
		G_CALLBACK(gui_export_directory_entry_changed), export_filename_example_label2);
	g_signal_connect ((gpointer) export_filename_entry, "changed", 
		G_CALLBACK(gui_export_filename_entry_changed), export_filename_example_label2);
	gui_confbox_set_callback(export_filetype_confbox, export_filename_example_label2, gui_export_filetype_combobox_changed);

	cms_page = gui_preferences_make_cms_page(rs);
	

	notebook = gtk_notebook_new();
	gtk_container_set_border_width (GTK_CONTAINER (notebook), 6);
	gtk_notebook_append_page(GTK_NOTEBOOK(notebook), preview_page, gtk_label_new(_("General")));
	//gtk_notebook_append_page(GTK_NOTEBOOK(notebook), batch_page, gtk_label_new(_("Batch")));
	gtk_notebook_append_page(GTK_NOTEBOOK(notebook), export_page, gtk_label_new(_("Quick export")));
	gtk_notebook_append_page(GTK_NOTEBOOK(notebook), cms_page, gtk_label_new(_("Colors")));
	gtk_box_pack_start (GTK_BOX (vbox), notebook, FALSE, FALSE, 0);

	button_close = gtk_button_new_from_stock(GTK_STOCK_CLOSE);
	gtk_dialog_add_action_widget (GTK_DIALOG (dialog), button_close, GTK_RESPONSE_CLOSE);

	gtk_widget_show_all(dialog);

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

GtkUIManager *
gui_get_uimanager()
{
	GStaticMutex lock = G_STATIC_MUTEX_INIT;
	static GtkUIManager *ui_manager = NULL;

	g_static_mutex_lock(&lock);
	if (!ui_manager)
	{
		GError *error = NULL;
		ui_manager = gtk_ui_manager_new ();
		gtk_ui_manager_add_ui_from_file (ui_manager, PACKAGE_DATA_DIR "/" PACKAGE "/ui.xml", &error);
		if (error)
		{
			g_message ("Building menus failed: %s", error->message);
			g_error_free (error);
		}
	}
	g_static_mutex_unlock(&lock);
	return ui_manager;
}

static GtkWidget *
gui_make_menubar(RS_BLOB *rs)
{
	GtkUIManager *menu_manager = gui_get_uimanager();
	gtk_ui_manager_insert_action_group (menu_manager, rs_get_core_action_group(rs), 0);
	/* FIXME: This gives trouble with auto-sensivity
	 * gtk_ui_manager_set_add_tearoffs(menu_manager, TRUE); */
	gtk_window_add_accel_group (GTK_WINDOW(rs->window), gtk_ui_manager_get_accel_group (menu_manager));
	return gtk_ui_manager_get_widget (menu_manager, "/MainMenu");
}

static void
drag_data_received(GtkWidget *widget, GdkDragContext *drag_context,
	gint x, gint y, GtkSelectionData *selection_data, guint info, guint t,
	RS_BLOB *rs)
{
	gchar *uris = (gchar *) selection_data->data;
	gchar *tmp;
	gchar *filename;

	if (!uris)
	{
		gtk_drag_finish (drag_context, FALSE, FALSE, t);
		return;
	}

	tmp = uris;
	while(tmp)
	{
		if ((*tmp == '\r') || (*tmp == '\n'))
		{
			*tmp = '\0';
			break;
		}
		tmp++;
	}

	filename = g_filename_from_uri(uris, NULL, NULL);

	rs_open_file(rs, filename);

	g_free(filename);

	gtk_drag_finish(drag_context, TRUE, FALSE, t);
	return;
}

static GtkWidget *
gui_window_make(RS_BLOB *rs)
{
	static const GtkTargetEntry targets[] = { { "text/uri-list", 0, 0 } };

	rawstudio_window = GTK_WINDOW(gtk_window_new (GTK_WINDOW_TOPLEVEL));
	gtk_window_resize((GtkWindow *) rawstudio_window, 800, 600);
	gtk_window_set_title (GTK_WINDOW (rawstudio_window), _("Rawstudio"));
	g_signal_connect((gpointer) rawstudio_window, "delete_event", G_CALLBACK(rs_shutdown), rs);
	g_signal_connect((gpointer) rawstudio_window, "key_press_event", G_CALLBACK(window_key_press_event), NULL);

	gtk_drag_dest_set(GTK_WIDGET(rawstudio_window), GTK_DEST_DEFAULT_ALL, targets, 1, GDK_ACTION_COPY);
	g_signal_connect((gpointer) rawstudio_window, "drag_data_received", G_CALLBACK(drag_data_received), rs);

	gtk_widget_set_name (GTK_WIDGET(rawstudio_window), "rawstudio-widget");

	return(GTK_WIDGET(rawstudio_window));
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
	GtkWidget *dialog, *image, *hhbox, *vvbox;
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

	hhbox = gtk_hbox_new (FALSE, 12);
	gtk_container_set_border_width (GTK_CONTAINER (hhbox), 5);
	gtk_box_pack_start (GTK_BOX (hhbox), image, FALSE, FALSE, 0);
	vvbox = gtk_vbox_new (FALSE, 12);
	gtk_box_pack_start (GTK_BOX (hhbox), vvbox, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (vvbox), primary_label, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (vvbox), widget, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox), hhbox, FALSE, FALSE, 0);

	return(dialog);
}

void
preview_wb_picked(RSPreviewWidget *preview, RS_PREVIEW_CALLBACK_DATA *cbdata, RS_BLOB *rs)
{
	rs_photo_set_wb_from_color(rs->photo, rs->current_setting,
		cbdata->pixelfloat[R],
		cbdata->pixelfloat[G],
		cbdata->pixelfloat[B]);
}

void
preview_motion(RSPreviewWidget *preview, RS_PREVIEW_CALLBACK_DATA *cbdata, RS_BLOB *rs)
{
	gchar tmp[20];

	g_snprintf(tmp, 20, "%u %u %u", cbdata->pixel8[R], cbdata->pixel8[G], cbdata->pixel8[B]);
	gtk_label_set_text(GTK_LABEL(valuefield), tmp);
}

static gboolean
open_file_in_mainloop(gpointer data)
{
	gpointer *foo = data;

	gdk_threads_enter();
	rs_open_file((RS_BLOB *) (foo[0]), (gchar *) foo[1]);
	g_free(foo[1]);
	gdk_threads_leave();

	return FALSE;
}

static void
rs_open_file_delayed(RS_BLOB *rs, const gchar *filename)
{
	gpointer *carrier = g_new(gpointer, 2);
	/* Load image in mainloop */
	carrier[0] = (gpointer) rs;
	carrier[1] = (gpointer) g_strdup(filename);
	g_idle_add(open_file_in_mainloop, carrier);
}

static void
rs_open_file(RS_BLOB *rs, const gchar *filename)
{
	if (filename)
	{	
		gchar *abspath;
		gchar *temppath = g_strdup(filename);
		gchar *lwd;

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
		{
			rs_store_remove(rs->store, NULL, NULL);
			if (rs_store_load_directory(rs->store, abspath) >= 0)
				rs_conf_set_string(CONF_LWD, abspath);
		}
		else if (g_file_test(abspath, G_FILE_TEST_IS_REGULAR))
		{
			lwd = g_path_get_dirname(abspath);
			filename = g_path_get_basename(abspath);
			rs_store_remove(rs->store, NULL, NULL);
			if (rs_store_load_directory(rs->store, lwd) >= 0)
				rs_conf_set_string(CONF_LWD, lwd);
			rs_store_set_selected_name(rs->store, abspath);
			g_free(lwd);
		}
		else
			rs_store_load_directory(rs->store, NULL);
		g_free(abspath);
	}
}

static gboolean
pane_position(GtkWidget* widget, gpointer dummy, gpointer user_data)
{
	GtkPaned *paned = GTK_PANED(widget);
	gint pos;
	gint window_width;
	gtk_window_get_size(rawstudio_window, &window_width, NULL);
	pos = gtk_paned_get_position(paned);
	rs_conf_set_integer(CONF_TOOLBOX_WIDTH, window_width - pos);
	return TRUE;
}

static void
directory_activated(gpointer instance, const gchar *path, RS_BLOB *rs)
{
	rs_store_remove(rs->store, NULL, NULL);
	if (rs_store_load_directory(rs->store, path) >= 0)
			rs_conf_set_string(CONF_LWD, path);
}

int
gui_init(int argc, char **argv, RS_BLOB *rs)
{
	GtkWidget *vbox;
	GtkWidget *pane;
	GtkWidget *tools;
	GtkWidget *batchbox;
	GtkWidget *menubar;
	GtkWidget *dir_selector_vbox;
	GtkWidget *checkbox_recursive;
	GtkWidget *dir_selector_separator;
	GtkWidget *dir_selector;
	gint window_width = 0, toolbox_width = 0;
	GdkColor dashed_bg = {0, 0, 0, 0 };
	GdkColor dashed_fg = {0, 0, 65535, 0};
	GdkColor grid_bg = {0, 0, 0, 0 };
	GdkColor grid_fg = {0, 32767, 32767, 32767};
	GdkColor bgcolor = {0, 0, 0, 0 };

	gtk_window_set_default_icon_from_file(PACKAGE_DATA_DIR "/pixmaps/" PACKAGE ".png", NULL);
	rs->window = gui_window_make(rs);
	gtk_widget_show(rs->window);

	/* initialize dashed gc */
	dashed = gdk_gc_new(rs->window->window);
	gdk_gc_set_rgb_fg_color(dashed, &dashed_fg);
	gdk_gc_set_rgb_bg_color(dashed, &dashed_bg);
	gdk_gc_set_line_attributes(dashed, 1, GDK_LINE_DOUBLE_DASH, GDK_CAP_BUTT, GDK_JOIN_MITER);
	grid = gdk_gc_new(rs->window->window);
	gdk_gc_set_rgb_fg_color(grid, &grid_fg);
	gdk_gc_set_rgb_bg_color(grid, &grid_bg);
	gdk_gc_set_line_attributes(grid, 1, GDK_LINE_SOLID, GDK_CAP_BUTT, GDK_JOIN_MITER);

	/* Build status bar */
	statusbar = GTK_STATUSBAR(gtk_statusbar_new());
	valuefield = gtk_label_new(NULL);
	hbox = gtk_hbox_new (FALSE, 0);
	gtk_box_pack_start (GTK_BOX (hbox), valuefield, FALSE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (hbox), GTK_WIDGET (statusbar), TRUE, TRUE, 0);

	/* Build toolbox */
	tools = make_toolbox(rs);
	batchbox = make_batchbox(rs->queue);

	dir_selector_vbox = gtk_vbox_new(FALSE, 0);
	checkbox_recursive = checkbox_from_conf(CONF_LOAD_RECURSIVE ,_("Open recursive"), DEFAULT_CONF_LOAD_RECURSIVE);
	dir_selector_separator = gtk_hseparator_new();
	dir_selector = rs_dir_selector_new();
	g_signal_connect(G_OBJECT(dir_selector), "directory-activated", G_CALLBACK(directory_activated), rs);
	gtk_box_pack_start (GTK_BOX(dir_selector_vbox), checkbox_recursive, FALSE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX(dir_selector_vbox), dir_selector_separator, FALSE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX(dir_selector_vbox), dir_selector, TRUE, TRUE, 0);

	rs->toolbox = gtk_notebook_new();
	gtk_notebook_append_page(GTK_NOTEBOOK(rs->toolbox), tools, gtk_label_new(_("Tools")));
	gtk_notebook_append_page(GTK_NOTEBOOK(rs->toolbox), batchbox, gtk_label_new(_("Batch")));
	gtk_notebook_append_page(GTK_NOTEBOOK(rs->toolbox), dir_selector_vbox, gtk_label_new(_("Open")));

	/* Build iconbox */
	rs->iconbox = rs_store_new();
	g_signal_connect((gpointer) rs->iconbox, "thumb-activated", G_CALLBACK(icon_activated), rs);

	/* Catch window state changes (un/fullscreen) */
	g_signal_connect((gpointer) rs->window, "window-state-event", G_CALLBACK(gui_fullscreen_iconbox_callback), rs->iconbox);
	g_signal_connect((gpointer) rs->window, "window-state-event", G_CALLBACK(gui_fullscreen_toolbox_callback), rs->toolbox);

	rs->store = RS_STORE(rs->iconbox);

	/* Build menubar */
	menubar = gui_make_menubar(rs);

	/* Preview area */
	rs->preview = rs_preview_widget_new();
	rs_preview_widget_set_cms(RS_PREVIEW_WIDGET(rs->preview), rs_cms_get_transform(rs->cms, PROFILE_DISPLAY));
	rs_conf_get_color(CONF_PREBGCOLOR, &bgcolor);
	rs_preview_widget_set_bgcolor(RS_PREVIEW_WIDGET(rs->preview), &bgcolor);
	g_signal_connect(G_OBJECT(rs->preview), "wb-picked", G_CALLBACK(preview_wb_picked), rs);
	g_signal_connect(G_OBJECT(rs->preview), "motion", G_CALLBACK(preview_motion), rs);

	/* Split pane below iconbox */
	pane = gtk_hpaned_new ();
	g_signal_connect_after(G_OBJECT(pane), "notify::position", G_CALLBACK(pane_position), NULL);

	gtk_paned_pack1 (GTK_PANED (pane), rs->preview, TRUE, TRUE);
	gtk_paned_pack2 (GTK_PANED (pane), rs->toolbox, FALSE, TRUE);

	/* Vertical packing box */
	vbox = gtk_vbox_new (FALSE, 0);
	gtk_container_add (GTK_CONTAINER (rs->window), vbox);

	gtk_box_pack_start (GTK_BOX (vbox), menubar, FALSE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (vbox), rs->iconbox, FALSE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (vbox), pane, TRUE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, TRUE, 0);

	gui_status_push(_("Ready"));

	// arrange rawstudio as the user left it
	gboolean show_iconbox;
	gboolean show_toolbox;
	rs_conf_get_boolean_with_default(CONF_FULLSCREEN, &fullscreen, DEFAULT_CONF_FULLSCREEN);
	if (fullscreen)
	{
		rs_core_action_group_activate("Fullscreen");
		rs_conf_get_boolean_with_default(CONF_SHOW_ICONBOX_FULLSCREEN, &show_iconbox, DEFAULT_CONF_SHOW_ICONBOX_FULLSCREEN);
		rs_conf_get_boolean_with_default(CONF_SHOW_TOOLBOX_FULLSCREEN, &show_toolbox, DEFAULT_CONF_SHOW_TOOLBOX_FULLSCREEN);
	}
	else
	{
		gtk_window_get_size(rawstudio_window, &window_width, NULL);
		if (rs_conf_get_integer(CONF_TOOLBOX_WIDTH, &toolbox_width))
			gtk_paned_set_position(GTK_PANED(pane), window_width - toolbox_width);

		gtk_window_unfullscreen(GTK_WINDOW(rs->window));
		rs_conf_get_boolean_with_default(CONF_SHOW_ICONBOX, &show_iconbox, DEFAULT_CONF_SHOW_TOOLBOX);
		rs_conf_get_boolean_with_default(CONF_SHOW_TOOLBOX, &show_toolbox, DEFAULT_CONF_SHOW_ICONBOX);
	}
	if (!show_iconbox)
		rs_core_action_group_activate("Iconbox");
	if (!show_toolbox)
		rs_core_action_group_activate("Toolbox");

	gtk_widget_show_all (rs->window);

	{
		gboolean preload = FALSE;
		rs_conf_get_boolean(CONF_PRELOAD, &preload);
		if (preload)
			rs_preload_set_maximum_memory(200*1024*1024);
		else
			rs_preload_set_maximum_memory(0);
	}

	if (argc > 1)
	{
		gchar *path;
		if (!g_path_is_absolute(argv[1]))
			path = g_build_filename(g_get_current_dir(), argv[1], NULL);
		else
			path = g_strdup(argv[1]);
		rs_open_file_delayed(rs, path);
		rs_conf_set_integer(CONF_LAST_PRIORITY_PAGE, 0);
		rs_dir_selector_expand_path(RS_DIR_SELECTOR(dir_selector), path);
		g_free(path);
	}
	else
	{
		gchar *lwd;
		lwd = rs_conf_get_string(CONF_LWD);
		if (!lwd)
			lwd = g_get_current_dir();
		if (rs_store_load_directory(rs->store, lwd))
		{
			gint last_priority_page = 0;
			rs_conf_get_integer(CONF_LAST_PRIORITY_PAGE, &last_priority_page);
			rs_store_set_current_page(rs->store, last_priority_page);
		}
		else
			rs_conf_set_integer(CONF_LAST_PRIORITY_PAGE, 0);
		rs_dir_selector_expand_path(RS_DIR_SELECTOR(dir_selector), lwd);
		g_free(lwd);
	}

	gui_set_busy(FALSE);
	gdk_threads_enter();
	gtk_main();
	gdk_threads_leave();
	return(0);
}
