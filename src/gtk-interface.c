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

struct rs_callback_data_t {
	RS_BLOB *rs;
	gpointer specific;
};

struct menu_item_t {
	GtkItemFactoryEntry item;
	gpointer specific_callback_data;
};

static gchar *filenames[] = {DEFAULT_CONF_EXPORT_FILENAME, "%f", "%f_%c", "%f_output_%4c", NULL};
static GtkStatusbar *statusbar;
static gboolean fullscreen;
GtkWindow *rawstudio_window;
static gint busycount = 0;
static GtkWidget *valuefield;
static GtkWidget *hbox;
GdkGC *dashed;
GdkGC *grid;

static struct rs_callback_data_t **callback_data_array;
static guint callback_data_array_size;

static void gui_menu_open_callback(gpointer callback_data, guint callback_action, GtkWidget *widget);
static void gui_menu_reload_callback(gpointer callback_data, guint callback_action, GtkWidget *widget);
static void gui_menu_purge_d_callback(gpointer callback_data, guint callback_action, GtkWidget *widget);
static void gui_preview_bg_color_changed(GtkColorButton *widget, RS_BLOB *rs);
static void gui_setprio(RS_BLOB *rs, guint prio);
static gboolean gui_accel_setprio_callback(GtkAccelGroup *group, GObject *obj, guint keyval,
	GdkModifierType mod, gpointer user_data);
static void gui_menu_setprio_callback(gpointer callback_data, guint callback_action, GtkWidget *widget);
#ifdef EXPERIMENTAL
static void gui_menu_group_photos_callback(gpointer callback_data, guint callback_action, GtkWidget *widget);
static void gui_menu_ungroup_photos_callback(gpointer callback_data, guint callback_action, GtkWidget *widget);
#endif
static void gui_widget_show(GtkWidget *widget, gboolean show, const gchar *conf_fullscreen_key, const gchar *conf_windowed_key);
static gboolean gui_fullscreen_iconbox_callback(GtkWidget *widget, GdkEventWindowState *event, GtkWidget *iconbox);
static gboolean gui_fullscreen_toolbox_callback(GtkWidget *widget, GdkEventWindowState *event, GtkWidget *toolbox);
static void gui_menu_iconbox_toggle_show_callback(gpointer callback_data, guint callback_action, GtkWidget *widget);
static void gui_menu_toolbox_toggle_show_callback(gpointer callback_data, guint callback_action, GtkWidget *widget);
static void gui_menu_fullscreen_callback(gpointer callback_data, guint callback_action, GtkWidget *widget);
static void gui_menu_prevnext_callback(gpointer callback_data, guint callback_action, GtkWidget *widget);
static void gui_preference_iconview_show_filenames_changed(GtkToggleButton *togglebutton, gpointer user_data);
static void gui_menu_preference_callback(gpointer callback_data, guint callback_action, GtkWidget *widget);
static void gui_menu_batch_run_queue_callback(gpointer callback_data, guint callback_action, GtkWidget *widget);
static void gui_menu_add_to_batch_queue_callback(gpointer callback_data, guint callback_action, GtkWidget *widget);
static void gui_menu_remove_from_batch_queue_callback(gpointer callback_data, guint callback_action, GtkWidget *widget);
static void gui_menu_add_view_to_batch_queue_callback(gpointer callback_data, guint callback_action, GtkWidget *widget);
static void gui_about();
static void gui_menu_auto_wb_callback(gpointer callback_data, guint callback_action, GtkWidget *widget);
static void gui_menu_cam_wb_callback(gpointer callback_data, guint callback_action, GtkWidget *widget);
static void gui_reset_current_settings_callback(gpointer callback_data, guint callback_action, GtkWidget *widget);
static void gui_menu_quit(gpointer callback_data, guint callback_action, GtkWidget *widget);
static void gui_menu_show_exposure_mask_callback(gpointer callback_data, guint callback_action, GtkWidget *widget);
static void gui_menu_paste_callback(gpointer callback_data, guint callback_action, GtkWidget *widget);
static void gui_menu_copy_callback(gpointer callback_data, guint callback_action, GtkWidget *widget);
static GtkWidget *gui_make_menubar(RS_BLOB *rs, GtkWidget *window, GtkWidget *iconbox, GtkWidget *toolbox);
static void drag_data_received(GtkWidget *widget, GdkDragContext *drag_context, gint x, gint y, GtkSelectionData *selection_data, guint info, guint t,	RS_BLOB *rs);
static GtkWidget *gui_window_make(RS_BLOB *rs);
static GtkWidget *gui_dialog_make_from_widget(const gchar *stock_id, gchar *primary_text, GtkWidget *widget);
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
		rs_settings_to_rs_settings_double(rs->settings[rs->current_setting], rs->photo->settings[rs->current_setting]);
		rs_update_preview(rs);
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
				rs->in_use = FALSE;
				rs_photo_close(rs->photo);
				rs_photo_free(rs->photo);
				rs->photo = NULL;
				rs_reset(rs);
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

			rs_settings_double_to_rs_settings(photo->settings[0], rs->settings[0]);
			rs_settings_double_to_rs_settings(photo->settings[1], rs->settings[1]);
			rs_settings_double_to_rs_settings(photo->settings[2], rs->settings[2]);
			rs->photo = photo;
			rs_preview_widget_set_photo(RS_PREVIEW_WIDGET(rs->preview), rs->photo);

			if (!cache_loaded)
			{
				if (rs->photo->metadata->cam_mul[R] == -1.0)
				{
					rs->current_setting = 2;
					rs_set_wb_auto(rs);
					rs->current_setting = 1;
					rs_set_wb_auto(rs);
					rs->current_setting = 0;
					rs_set_wb_auto(rs);
				}
				else
				{
					rs->current_setting = 2;
					rs_set_wb_from_mul(rs, rs->photo->metadata->cam_mul);
					rs->current_setting = 1;
					rs_set_wb_from_mul(rs, rs->photo->metadata->cam_mul);
					rs->current_setting = 0;
					rs_set_wb_from_mul(rs, rs->photo->metadata->cam_mul);
				}
				if (rs->photo->metadata->contrast != -1.0)
				{
					SETVAL(rs->settings[2]->contrast,rs->photo->metadata->contrast);
					SETVAL(rs->settings[1]->contrast,rs->photo->metadata->contrast);
					SETVAL(rs->settings[0]->contrast,rs->photo->metadata->contrast);
				}
				if (rs->photo->metadata->saturation != -1.0)
				{
					SETVAL(rs->settings[2]->saturation,rs->photo->metadata->saturation);
					SETVAL(rs->settings[1]->saturation,rs->photo->metadata->saturation);
					SETVAL(rs->settings[0]->saturation,rs->photo->metadata->saturation);
				}
			}

			if (rs->histogram_dataset)
				rs_image16_free(rs->histogram_dataset);

			rs->histogram_dataset = rs_image16_scale(rs->photo->input, NULL,
				(gdouble)HISTOGRAM_DATASET_WIDTH/(gdouble)rs->photo->input->w);
			rs_histogram_set_image(RS_HISTOGRAM_WIDGET(rs->histogram), rs->histogram_dataset);
		}
		rs->in_use = TRUE;
		rs_update_preview(rs);
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
gui_menu_open_callback(gpointer callback_data, guint callback_action, GtkWidget *widget)
{
	RS_BLOB *rs = (RS_BLOB *)((struct rs_callback_data_t*)callback_data)->rs;
	GtkWidget *fc;
	gchar *lwd = rs_conf_get_string(CONF_LWD);

	fc = gtk_file_chooser_dialog_new (_("Open directory"), NULL,
		GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
		GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
		GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT, NULL);
	gtk_dialog_set_default_response(GTK_DIALOG(fc), GTK_RESPONSE_ACCEPT);
	
	gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER (fc), lwd);

	if (gtk_dialog_run (GTK_DIALOG (fc)) == GTK_RESPONSE_ACCEPT)
	{
		char *filename;

		gui_set_busy(TRUE);
		filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (fc));
		gtk_widget_destroy (fc);
		rs_store_remove(rs->store, NULL, NULL);
		if (rs_store_load_directory(rs->store, filename) >= 0)
			rs_conf_set_string(CONF_LWD, filename);
		g_free (filename);
		gui_set_busy(FALSE);
	} else
		gtk_widget_destroy (fc);

	g_free(lwd);
	return;
}

static void
gui_menu_reload_callback(gpointer callback_data, guint callback_action, GtkWidget *widget)
{
	RS_BLOB *rs = (RS_BLOB *)((struct rs_callback_data_t*)callback_data)->rs;
	rs_store_remove(rs->store, NULL, NULL);
	rs_store_load_directory(rs->store, NULL);
	return;
}

static void
gui_menu_purge_d_callback(gpointer callback_data, guint callback_action, GtkWidget *widget)
{
	RS_BLOB *rs = (RS_BLOB *)((struct rs_callback_data_t*)callback_data)->rs;
	gchar *thumb, *cache;
	GtkWidget *dialog;
	GList *photos_d = NULL;
	gint items = 0, i;
	RS_PROGRESS *progress;

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

	photos_d = rs_store_get_iters_with_priority(rs->store, PRIO_D);
	items = g_list_length(photos_d);

	progress = gui_progress_new(NULL, items);

	for (i=0;i<items;i++)
	{
		gchar *fullname = rs_store_get_name(rs->store, g_list_nth_data(photos_d, i));

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
			/* Try to delete thm-files */
			{
				gchar *thm;
				gchar *ext;

				thm = g_strdup(fullname);
				ext = g_strrstr(thm, ".");
				ext++;
				g_strlcpy(ext, "thm", 4);
				if(g_unlink(thm))
				{
					g_strlcpy(ext, "THM", 4);
					g_unlink(thm);
				}
				g_free(thm);
			}
			rs_store_remove(rs->store, NULL, g_list_nth_data(photos_d, i));
			gui_progress_advance_one(progress);
			GUI_CATCHUP();
		}
	}
	g_list_free(photos_d);
	gui_progress_free(progress);

	return;
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

static void
gui_menu_zoom_callback(gpointer callback_data, guint callback_action, GtkWidget *widget)
{
	RS_BLOB *rs = (RS_BLOB *)((struct rs_callback_data_t*)callback_data)->rs;

	switch (callback_action)
	{
		case 0: /* zoom to fit */
			rs_preview_widget_set_zoom_to_fit(RS_PREVIEW_WIDGET(rs->preview));
			break;
		case 1: /* zoom in */
			rs_preview_widget_zoom_in(RS_PREVIEW_WIDGET(rs->preview));
			break;
		case 2: /* zoom out */
			rs_preview_widget_zoom_out(RS_PREVIEW_WIDGET(rs->preview));
			break;
		case 100: /* zoom 100% */
			rs_preview_widget_set_zoom(RS_PREVIEW_WIDGET(rs->preview), 1.0);
			break;
	}
	return;
}

static void
gui_menu_prevnext_callback(gpointer callback_data, guint callback_action, GtkWidget *widget)
{
	gchar *current_filename = NULL;
	RS_BLOB *rs = (RS_BLOB *)((struct rs_callback_data_t*)callback_data)->rs;
	
	/* Get current filename if a photo is loaded */
	if (rs->photo)
		current_filename = rs->photo->filename;

	rs_store_select_prevnext(rs->store, current_filename, callback_action);
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

static gboolean
gui_accel_setprio_callback(GtkAccelGroup *group, GObject *obj, guint keyval,
	GdkModifierType mod, gpointer user_data)
{
	RS_BLOB *rs = (RS_BLOB *)((struct rs_callback_data_t*)user_data)->rs;
	/* this cast is okay on 64-bit architectures, we cast from int in gui_make_menubar() */
	guint prio = GPOINTER_TO_INT(((struct rs_callback_data_t*)user_data)->specific);
	gui_setprio(rs, prio);
	return(TRUE);
}

static void
gui_menu_setprio_callback(gpointer callback_data, guint callback_action, GtkWidget *widget)
{
	RS_BLOB *rs = (RS_BLOB *)((struct rs_callback_data_t*)callback_data)->rs;
	gui_setprio(rs, callback_action);
	return;
}

static void
gui_menu_crop_callback(gpointer callback_data, guint callback_action, GtkWidget *widget)
{
	RS_BLOB *rs = (RS_BLOB *)((struct rs_callback_data_t*)callback_data)->rs;
	rs_preview_widget_crop_start(RS_PREVIEW_WIDGET(rs->preview));
	return;
}

static void
gui_menu_uncrop_callback(gpointer callback_data, guint callback_action, GtkWidget *widget)
{
	RS_BLOB *rs = (RS_BLOB *)((struct rs_callback_data_t*)callback_data)->rs;
	rs_preview_widget_uncrop(RS_PREVIEW_WIDGET(rs->preview));
	return;
}

static void
gui_menu_straighten_callback(gpointer callback_data, guint callback_action, GtkWidget *widget)
{
	RS_BLOB *rs = (RS_BLOB *)((struct rs_callback_data_t*)callback_data)->rs;

	if (callback_action == 1)
		rs_preview_widget_straighten(RS_PREVIEW_WIDGET(rs->preview));
	else
		rs_preview_widget_unstraighten(RS_PREVIEW_WIDGET(rs->preview));
	return;
}

#ifdef EXPERIMENTAL
static void
gui_menu_group_photos_callback(gpointer callback_data, guint callback_action, GtkWidget *widget)
{
	RS_BLOB *rs = (RS_BLOB *)((struct rs_callback_data_t*)callback_data)->rs;
	rs_store_group_photos(rs->store);
	return;
}

static void
gui_menu_ungroup_photos_callback(gpointer callback_data, guint callback_action, GtkWidget *widget)
{
	RS_BLOB *rs = (RS_BLOB *)((struct rs_callback_data_t*)callback_data)->rs;
	rs_store_ungroup_photos(rs->store);
	return;
}
#endif

static void
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


static void
gui_menu_iconbox_toggle_show_callback(gpointer callback_data, guint callback_action, GtkWidget *widget)
{
	GtkWidget *target = (GtkWidget *)((struct rs_callback_data_t*)callback_data)->specific;

	gui_widget_show(target, !GTK_WIDGET_VISIBLE(target), CONF_SHOW_ICONBOX_FULLSCREEN, CONF_SHOW_ICONBOX);
	return;
}

static void
gui_menu_toolbox_toggle_show_callback(gpointer callback_data, guint callback_action, GtkWidget *widget)
{
	GtkWidget *target = (GtkWidget *)((struct rs_callback_data_t*)callback_data)->specific;

	gui_widget_show(target, !GTK_WIDGET_VISIBLE(target), CONF_SHOW_TOOLBOX_FULLSCREEN, CONF_SHOW_TOOLBOX);
	return;
}


static void
gui_menu_fullscreen_callback(gpointer callback_data, guint callback_action, GtkWidget *widget)
{
	GtkWindow * window = (GtkWindow *)((struct rs_callback_data_t*)callback_data)->specific;
	if (fullscreen) {
		gtk_window_unfullscreen(window);
		rs_conf_set_boolean(CONF_FULLSCREEN, FALSE);
	}
	else
	{
		gtk_window_fullscreen(window);
		rs_conf_set_boolean(CONF_FULLSCREEN, TRUE);
	}
	return;
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
gui_preference_preload_changed(GtkToggleButton *togglebutton, gpointer user_data)
{
	if (togglebutton->active)
		rs_preload_set_maximum_memory(200*1024*1024);
	else
		rs_preload_set_maximum_memory(0);

	return;
}

static void
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
	GtkWidget *local_cache_check;
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

	RS_BLOB *rs = (RS_BLOB *)((struct rs_callback_data_t*)callback_data)->rs;

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

static void
gui_menu_batch_run_queue_callback(gpointer callback_data, guint callback_action, GtkWidget *widget)
{
	RS_BLOB *rs = (RS_BLOB *)((struct rs_callback_data_t*)callback_data)->rs;
	rs_batch_process(rs->queue);
	return;
}

static void
gui_menu_add_to_batch_queue_callback(gpointer callback_data, guint callback_action, GtkWidget *widget)
{
	RS_BLOB *rs = (RS_BLOB *)((struct rs_callback_data_t*)callback_data)->rs;
	GList *selected = NULL;
	gint num_selected, cur;

	selected = rs_store_get_selected_names(rs->store);
	num_selected = g_list_length(selected);

	if (rs->in_use && num_selected == 1)
	{
		rs_cache_save(rs->photo);

		if (rs_batch_add_to_queue(rs->queue, rs->photo->filename, rs->current_setting))
			gui_status_notify(_("Added to batch queue"));
		else
			gui_status_notify(_("Already added to batch queue"));
	}

	/* Deal with selected icons */
	for(cur=0;cur<num_selected;cur++)
		rs_batch_add_to_queue(rs->queue, g_list_nth_data(selected, cur), rs->current_setting);
	g_list_free(selected);
}

static void
gui_menu_remove_from_batch_queue_callback(gpointer callback_data, guint callback_action, GtkWidget *widget)
{
	RS_BLOB *rs = (RS_BLOB *)((struct rs_callback_data_t*)callback_data)->rs;
	if (rs->in_use)
	{
		if (rs_batch_remove_from_queue(rs->queue, rs->photo->filename, rs->current_setting))
			gui_status_notify(_("Removed from batch queue"));
		else
			gui_status_notify(_("Not in batch queue"));
	}
}

static void
gui_menu_add_view_to_batch_queue_callback(gpointer callback_data, guint callback_action, GtkWidget *widget)
{
	GtkWidget *dialog, *cb_box;
	GtkWidget *cb_a, *cb_b, *cb_c;
	RS_BLOB *rs = (RS_BLOB *)((struct rs_callback_data_t*)callback_data)->rs;

	cb_a = gtk_check_button_new_with_label (_("A"));
	cb_b = gtk_check_button_new_with_label (_("B"));
	cb_c = gtk_check_button_new_with_label (_("C"));

	switch (rs->current_setting)
	{
		case 0:
			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(cb_a), TRUE);
			break;
		case 1:
			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(cb_b), TRUE);
			break;
		case 2:
			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(cb_c), TRUE);
			break;
	}

	cb_box = gtk_vbox_new(FALSE, 4);

	gtk_box_pack_start (GTK_BOX (cb_box), cb_a, FALSE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (cb_box), cb_b, FALSE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (cb_box), cb_c, FALSE, TRUE, 0);

	dialog = gui_dialog_make_from_widget(GTK_STOCK_DIALOG_QUESTION, 
				_("Select which settings to\nadd to batch queue"), cb_box);

	gtk_dialog_add_buttons(GTK_DIALOG(dialog), GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL, GTK_STOCK_APPLY, GTK_RESPONSE_APPLY, NULL);
	gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_APPLY);
	gtk_widget_show_all(dialog);

	if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_APPLY)
	{
		GList *selected = NULL;
		gint num_selected, i;

		rs_store_get_names(rs->store, NULL, &selected, NULL);
		selected = rs_store_sort_selected(selected);
		num_selected = g_list_length(selected);

		for (i=0;i<num_selected;i++)
		{
			gchar *fullname = g_list_nth_data(selected, i);

			if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(cb_a)))
				rs_batch_add_to_queue(rs->queue, fullname, 0);

			if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(cb_b)))
				rs_batch_add_to_queue(rs->queue, fullname, 1);

			if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(cb_c)))
				rs_batch_add_to_queue(rs->queue, fullname, 2);
		}
		g_list_free(selected);

		/* Save settings of current photo just to be sure */
		if (rs->photo)
			rs_cache_save(rs->photo);

		gui_status_notify(_("Added view to batch queue"));
	}

	gtk_widget_destroy (dialog);

	return;
}

static void
gui_about(void)
{
	const gchar *authors[] = {
		"Anders Brander <anders@brander.dk>",
		"Anders Kvist <anders@kvistmail.dk>",
		NULL
	};
	const gchar *artists[] = {
		"Kristoffer Jørgensen <kristoffer@vektormusik.dk>",
		"Rune Stowasser <rune.stowasser@gmail.com>",
		NULL
	};
	gtk_show_about_dialog(GTK_WINDOW(rawstudio_window),
		"authors", authors,
		"artists", artists,
		"comments", _("A raw image converter for GTK+/GNOME"),
		"version", VERSION,
		"website", "http://rawstudio.org/",
		"name", "Rawstudio",
		NULL
	);
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

static void
gui_menu_auto_wb_callback(gpointer callback_data, guint callback_action, GtkWidget *widget)
{
	RS_BLOB *rs = (RS_BLOB *)((struct rs_callback_data_t*)callback_data)->rs;
	gui_set_busy(TRUE);
	GUI_CATCHUP();
	gui_status_notify(_("Adjusting to auto white balance"));
	rs_set_wb_auto(rs);
	gui_set_busy(FALSE);
}

static void
gui_menu_cam_wb_callback(gpointer callback_data, guint callback_action, GtkWidget *widget)
{
	RS_BLOB *rs = (RS_BLOB *)((struct rs_callback_data_t*)callback_data)->rs;
	if (!rs->photo || rs->photo->metadata->cam_mul[R] == -1.0)
		gui_status_notify(_("No white balance to set from"));
	else
	{
		gui_status_notify(_("Adjusting to camera white balance"));
		rs_set_wb_from_mul(rs, rs->photo->metadata->cam_mul);
	}
}

static void
gui_save_file_callback(gpointer callback_data, guint callback_action, GtkWidget *widget)
{
	RS_BLOB *rs = (RS_BLOB *)((struct rs_callback_data_t*)callback_data)->rs;
	gui_save_file_dialog(rs);
	return;
}

static void
gui_quick_save_file_callback(gpointer callback_data, guint callback_action, GtkWidget *widget)
{
	RS_BLOB *rs = (RS_BLOB *)((struct rs_callback_data_t*)callback_data)->rs;
	gchar *dirname;
	gchar *conf_export_directory;
	gchar *conf_export_filename;
	GString *export_path;
	GString *save;
	gchar *parsed_filename;
	RS_FILETYPE *filetype;

	if (!rs->photo) return;
	gui_set_busy(TRUE);
	GUI_CATCHUP();
	dirname = g_path_get_dirname(rs->photo->filename);
	
	conf_export_directory = rs_conf_get_string(CONF_EXPORT_DIRECTORY);
	if (!conf_export_directory)
		conf_export_directory = g_strdup(DEFAULT_CONF_EXPORT_DIRECTORY);

	conf_export_filename = rs_conf_get_string(CONF_EXPORT_FILENAME);
	if (!conf_export_filename)
		conf_export_filename = DEFAULT_CONF_EXPORT_FILENAME;

	rs_conf_get_filetype(CONF_EXPORT_FILETYPE, &filetype);

	if (conf_export_directory)
	{
		if (conf_export_directory[0]==G_DIR_SEPARATOR)
		{
			g_free(dirname);
			dirname = conf_export_directory;
		}
		else
		{
			export_path = g_string_new(dirname);
			g_string_append(export_path, G_DIR_SEPARATOR_S);
			g_string_append(export_path, conf_export_directory);
			g_free(dirname);
			dirname = export_path->str;
			g_string_free(export_path, FALSE);
			g_free(conf_export_directory);
		}
		g_mkdir_with_parents(dirname, 00755);
	}
	
	save = g_string_new(dirname);
	if (dirname[strlen(dirname)-1] != G_DIR_SEPARATOR)
		g_string_append(save, G_DIR_SEPARATOR_S);
	g_string_append(save, conf_export_filename);

	g_string_append(save, filetype->ext);

	parsed_filename = filename_parse(save->str, rs->photo->filename, rs->current_setting);
	g_string_free(save, TRUE);

	rs_photo_save(rs->photo, parsed_filename, filetype->filetype, -1, -1, FALSE, 1.0, rs->current_setting, rs->cms);
	gui_status_notify(_("File exported"));
	g_free(parsed_filename);

	gui_set_busy(FALSE);
	return;
}

static void
gui_reset_current_settings_callback(gpointer callback_data, guint callback_action, GtkWidget *widget)
{
	RS_BLOB *rs = (RS_BLOB *)((struct rs_callback_data_t*)callback_data)->rs;
	gboolean in_use = rs->in_use;

	rs->in_use = FALSE;
	rs_settings_reset(rs->settings[rs->current_setting], MASK_ALL);
	rs->in_use = in_use;
	rs_update_preview(rs);
	return;
}

static void
gui_menu_quit(gpointer callback_data, guint callback_action, GtkWidget *widget)
{
	RS_BLOB *rs = (RS_BLOB *)((struct rs_callback_data_t*)callback_data)->rs;
	int i;

	rs_shutdown(NULL, NULL, rs);
	for (i=0; i<callback_data_array_size; i++)
		g_free(callback_data_array[i]);
	g_free(callback_data_array);
	return;
}

static void
gui_menu_show_exposure_mask_callback(gpointer callback_data, guint callback_action, GtkWidget *widget)
{
	RS_BLOB *rs = (RS_BLOB *)((struct rs_callback_data_t*)callback_data)->rs;
	if (GTK_CHECK_MENU_ITEM(widget)->active)
	  gui_status_notify(_("Showing exposure mask"));
	else
	  gui_status_notify(_("Hiding exposure mask"));
	rs_preview_widget_set_show_exposure_mask(RS_PREVIEW_WIDGET(rs->preview), GTK_CHECK_MENU_ITEM(widget)->active);
	return;
}

static void
gui_menu_revert_callback(gpointer callback_data, guint callback_action, GtkWidget *widget)
{
	RS_BLOB *rs = (RS_BLOB *)((struct rs_callback_data_t*)callback_data)->rs;
	RS_PHOTO *photo; /* FIXME: evil evil evil hack, fix rs_cache_load() */

	if (!rs->photo) return;

	photo = rs_photo_new();
	photo->filename = rs->photo->filename;

	rs_cache_load(photo);
	rs_settings_double_to_rs_settings(photo->settings[rs->current_setting],
		rs->settings[rs->current_setting]);
	photo->filename = NULL;
	rs_photo_free(photo);
	rs_update_preview(rs);
	return;
}

static void
gui_menu_copy_callback(gpointer callback_data, guint callback_action, GtkWidget *widget)
{
	RS_BLOB *rs = (RS_BLOB *)((struct rs_callback_data_t*)callback_data)->rs;

	if (rs->in_use) 
	{
		if (!rs->settings_buffer)
			rs->settings_buffer = g_malloc(sizeof(RS_SETTINGS_DOUBLE));
		rs_settings_to_rs_settings_double(rs->settings[rs->current_setting], rs->settings_buffer);
		gui_status_notify(_("Copied settings"));
	}
	return;
}

static void
gui_menu_paste_callback(gpointer callback_data, guint callback_action, GtkWidget *widget)
{
	RS_BLOB *rs = (RS_BLOB *)((struct rs_callback_data_t*)callback_data)->rs;
	gint mask;
	
	GtkWidget *dialog, *cb_box;
	GtkWidget *cb_exposure, *cb_saturation, *cb_hue, *cb_contrast, *cb_whitebalance, *cb_curve;

	if (rs->settings_buffer)
	{
		/* Build GUI */
		cb_exposure = gtk_check_button_new_with_label (_("Exposure"));
		cb_saturation = gtk_check_button_new_with_label (_("Saturation"));
		cb_hue = gtk_check_button_new_with_label (_("Hue"));
		cb_contrast = gtk_check_button_new_with_label (_("Contrast"));
		cb_whitebalance = gtk_check_button_new_with_label (_("White balance"));
		cb_curve = gtk_check_button_new_with_label (_("Curve"));

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
		if (mask & MASK_CURVE)
			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(cb_curve), TRUE);

		cb_box = gtk_vbox_new(FALSE, 0);

		gtk_box_pack_start (GTK_BOX (cb_box), cb_exposure, FALSE, TRUE, 0);
		gtk_box_pack_start (GTK_BOX (cb_box), cb_saturation, FALSE, TRUE, 0);
		gtk_box_pack_start (GTK_BOX (cb_box), cb_hue, FALSE, TRUE, 0);
		gtk_box_pack_start (GTK_BOX (cb_box), cb_contrast, FALSE, TRUE, 0);
		gtk_box_pack_start (GTK_BOX (cb_box), cb_whitebalance, FALSE, TRUE, 0);
		gtk_box_pack_start (GTK_BOX (cb_box), cb_curve, FALSE, TRUE, 0);

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
			if (GTK_TOGGLE_BUTTON(cb_curve)->active)
				mask |= MASK_CURVE;
			rs_conf_set_integer(CONF_PASTE_MASK, mask);
   		}
		gtk_widget_destroy (dialog);

		if(mask > 0)
		{
			RS_PHOTO *photo;
			RS_FILETYPE *filetype;
			gint cur;
			GList *selected = NULL;
			gint num_selected;

			/* Apply to all selected photos */
			selected = rs_store_get_selected_names(rs->store);
			num_selected = g_list_length(selected);
			for(cur=0;cur<num_selected;cur++)
			{
				/* This is nothing but a hack around rs_cache_*() */
				photo = rs_photo_new();
				photo->filename = g_strdup(g_list_nth_data(selected, cur));
				if ((filetype = rs_filetype_get(photo->filename, TRUE)))
				{
					if (filetype->load_meta)
					{
						filetype->load_meta(photo->filename, photo->metadata);
						switch (photo->metadata->orientation)
						{
							case 90: ORIENTATION_90(photo->orientation);
								break;
							case 180: ORIENTATION_180(photo->orientation);
								break;
							case 270: ORIENTATION_270(photo->orientation);
								break;
						}
					}
					rs_cache_load(photo);
					rs_settings_double_copy(rs->settings_buffer, photo->settings[rs->current_setting], mask);
					rs_cache_save(photo);
				}
				rs_photo_free(photo);
			}
			g_list_free(selected);

			/* Apply to current photo */
			if (rs->in_use)
			{
				gboolean in_use = rs->in_use;
				rs->in_use = FALSE;
				rs_apply_settings_from_double(rs->settings[rs->current_setting], rs->settings_buffer, mask);
				rs->in_use = in_use;
				rs_update_preview(rs);
			}

			gui_status_notify(_("Pasted settings"));
		}
		else
			gui_status_notify(_("Nothing to paste"));
	}
	else 
		gui_status_notify(_("Buffer empty"));
	return;
}

static GtkWidget *
gui_make_menubar(RS_BLOB *rs, GtkWidget *window, GtkWidget *iconbox, GtkWidget *tools)
{
	struct menu_item_t menu_items[] = {
		{{ _("/_File"), NULL, NULL, 0, "<Branch>"}, NULL},
		{{ _("/File/_Open directory..."), "<CTRL>O", (gpointer)&gui_menu_open_callback, 1, "<StockItem>", GTK_STOCK_OPEN}, NULL},
		{{ _("/File/_Quick export"), "<CTRL>S", (gpointer)&gui_quick_save_file_callback, 1, "<StockItem>", GTK_STOCK_SAVE}, NULL},
		{{ _("/File/_Export as..."), "<CTRL><SHIFT>S", (gpointer)&gui_save_file_callback, 1, "<StockItem>", GTK_STOCK_SAVE_AS}, NULL},
		{{ _("/File/_Reload"), "<CTRL>R", (gpointer)&gui_menu_reload_callback, 1, "<StockItem>", GTK_STOCK_REFRESH}, NULL},
		{{ _("/File/_Delete flagged photos"), "<CTRL><SHIFT>D", (gpointer)&gui_menu_purge_d_callback, 0, "<StockItem>", GTK_STOCK_DELETE}, NULL},
		{{ _("/File/_Quit"), "<CTRL>Q", (gpointer)&gui_menu_quit, 0, "<StockItem>", GTK_STOCK_QUIT}, NULL},
		{{ _("/_Edit"), NULL, NULL, 0, "<Branch>"}, NULL},
		{{ _("/_Edit/_Revert settings"),  "<CTRL>Z", (gpointer)&gui_menu_revert_callback, 0, "<StockItem>", GTK_STOCK_UNDO}, NULL},
		{{ _("/_Edit/_Copy settings"),  "<CTRL>C", (gpointer)&gui_menu_copy_callback, 0, "<StockItem>", GTK_STOCK_COPY}, NULL},
		{{ _("/_Edit/_Paste settings"),  "<CTRL>V", (gpointer)&gui_menu_paste_callback, 0, "<StockItem>", GTK_STOCK_PASTE}, NULL},
		{{ _("/_Edit/_Reset current settings"), NULL , (gpointer)&gui_reset_current_settings_callback, 1}, NULL},
		{{ _("/_Edit/sep1"), NULL, NULL, 0, "<Separator>"}, NULL},
		{{ _("/_Edit/_Preferences"), NULL, (gpointer)&gui_menu_preference_callback, 0, "<StockItem>", GTK_STOCK_PREFERENCES}, NULL},
		{{ _("/_Photo/_Flag photo for deletion"),  "Delete", (gpointer)&gui_menu_setprio_callback, PRIO_D, "<StockItem>", GTK_STOCK_DELETE}, NULL},
		{{ _("/_Photo/_Set priority/_1"),  "1", (gpointer)&gui_menu_setprio_callback, PRIO_1}, NULL},
		{{ _("/_Photo/_Set priority/_2"),  "2", (gpointer)&gui_menu_setprio_callback, PRIO_2}, NULL},
		{{ _("/_Photo/_Set priority/_3"),  "3", (gpointer)&gui_menu_setprio_callback, PRIO_3}, NULL},
		{{ _("/_Photo/_Set priority/_Remove priority"),  "0", (gpointer)&gui_menu_setprio_callback, PRIO_U}, NULL},
		{{ _("/_Photo/_White balance/_Auto"), "A", (gpointer)&gui_menu_auto_wb_callback, 0 }, NULL},
		{{ _("/_Photo/_White balance/_Camera"), "C", (gpointer)&gui_menu_cam_wb_callback, 0 }, NULL},
		{{ _("/_Photo/_Crop"),  "<Shift>C", (gpointer)&gui_menu_crop_callback, PRIO_U}, NULL},
		{{ _("/_Photo/_Uncrop"),  "<Shift>V", (gpointer)&gui_menu_uncrop_callback, PRIO_U}, NULL},
		{{ _("/_Photo/_Straighten"),  NULL, (gpointer)&gui_menu_straighten_callback, 1}, NULL},
		{{ _("/_Photo/_Unstraighten"),  NULL, (gpointer)&gui_menu_straighten_callback, 0}, NULL},
#ifdef EXPERIMENTAL
		{{ _("/_Photo/_Group photos"),  "<Ctrl>G", (gpointer)&gui_menu_group_photos_callback, PRIO_1}, NULL},
		{{ _("/_Photo/_Ungroup photos"),  "<Ctrl><SHIFT>G", (gpointer)&gui_menu_ungroup_photos_callback, PRIO_1}, NULL},
#endif
		{{ _("/_View"), NULL, NULL, 0, "<Branch>"}, NULL},
		{{ _("/_View/_Previous photo"), "<CTRL>Left", (gpointer)&gui_menu_prevnext_callback, 1, "<StockItem>", GTK_STOCK_GO_BACK}, NULL},
		{{ _("/_View/_Next photo"), "<CTRL>Right", (gpointer)&gui_menu_prevnext_callback, 2, "<StockItem>", GTK_STOCK_GO_FORWARD}, NULL},
		{{ _("/_View/sep1"), NULL, NULL, 0, "<Separator>"}, NULL},
		{{ _("/_View/_Zoom in"), "plus", (gpointer)&gui_menu_zoom_callback, 1, "<StockItem>", GTK_STOCK_ZOOM_IN}, NULL},
		{{ _("/_View/_Zoom out"), "minus", (gpointer)&gui_menu_zoom_callback, 2, "<StockItem>", GTK_STOCK_ZOOM_OUT}, NULL},
		{{ _("/_View/_Zoom to fit"), "slash", (gpointer)&gui_menu_zoom_callback, 0, "<StockItem>", GTK_STOCK_ZOOM_FIT}, NULL},
		{{ _("/_View/_Zoom to 100%"), "asterisk", (gpointer)&gui_menu_zoom_callback, 100, "<StockItem>", GTK_STOCK_ZOOM_100}, NULL},
		{{ _("/_View/sep2"), NULL, NULL, 0, "<Separator>"}, NULL},
		{{ _("/_View/_Icon Box"), "<CTRL>I", (gpointer)&gui_menu_iconbox_toggle_show_callback, 1}, (gpointer)iconbox},
		{{ _("/_View/_Tool Box"), "<CTRL>T", (gpointer)&gui_menu_toolbox_toggle_show_callback, 1}, (gpointer)tools},
		{{ _("/_View/sep3"), NULL, NULL, 0, "<Separator>"}, NULL},
#if GTK_CHECK_VERSION(2,8,0)
		{{ _("/_View/_Fullscreen"), "F11", (gpointer)&gui_menu_fullscreen_callback, 1, "<StockItem>", GTK_STOCK_FULLSCREEN}, (gpointer)window},
#else
		{{ _("/_View/_Fullscreen"), "F11", (gpointer)&gui_menu_fullscreen_callback, 1}, (gpointer)window},
#endif
		{{ _("/_View/sep1"), NULL, NULL, 0, "<Separator>"}, NULL},
		{{ _("/_View/_Show exposure mask"), "<CTRL>E", (gpointer)&gui_menu_show_exposure_mask_callback, 0, "<ToggleItem>"}, NULL},
		{{ _("/_Batch"), NULL, NULL, 0, "<Branch>"}, NULL},
		{{ _("/_Batch/_Add to batch queue"),  "<CTRL>B", gui_menu_add_to_batch_queue_callback, 0 , "<StockItem>", GTK_STOCK_ADD}, NULL},
		{{ _("/_Batch/_Add current view to queue"), NULL, gui_menu_add_view_to_batch_queue_callback, 0 }, NULL},
		{{ _("/_Batch/_Remove from batch queue"),  "<CTRL><ALT>B", gui_menu_remove_from_batch_queue_callback, 0 , "<StockItem>", GTK_STOCK_REMOVE}, NULL},
		{{ _("/_Batch/_Start"), NULL, gui_menu_batch_run_queue_callback, 0 }, NULL},
		{{ _("/_Help"), NULL, NULL, 0, "<LastBranch>"}, NULL},
		{{ _("/_Help/About"), NULL, (gpointer)&gui_about, 0, "<StockItem>", GTK_STOCK_ABOUT}, NULL},
	};
	static gint nmenu_items = sizeof (menu_items) / sizeof (menu_items[0]);
	GtkItemFactory *item_factory;
	GtkAccelGroup *accel_group;
	GClosure *prio1, *prio2, *prio3, *priou;
	int i;

	accel_group = gtk_accel_group_new ();
	item_factory = gtk_item_factory_new (GTK_TYPE_MENU_BAR, "<main>", accel_group);
	callback_data_array = g_malloc(sizeof(struct rs_callback_data_t*)*(nmenu_items+4));
	callback_data_array_size = nmenu_items;
	for (i=0; i<nmenu_items; i++) {
		callback_data_array[i] = g_malloc(sizeof(struct rs_callback_data_t));
		callback_data_array[i]->rs = rs;
		callback_data_array[i]->specific = menu_items[i].specific_callback_data;
		gtk_item_factory_create_item (item_factory, &(menu_items[i].item), (gpointer)callback_data_array[i], 1);
	}

	/* this is stupid - but it makes numeric keypad work for setting priorities */
	callback_data_array[nmenu_items] = g_malloc(sizeof(struct rs_callback_data_t));
	callback_data_array[nmenu_items]->rs = rs;
	callback_data_array[nmenu_items]->specific = GINT_TO_POINTER(PRIO_1);
	callback_data_array[nmenu_items+1] = g_malloc(sizeof(struct rs_callback_data_t));
	callback_data_array[nmenu_items+1]->rs = rs;
	callback_data_array[nmenu_items+1]->specific = GINT_TO_POINTER(PRIO_2);
	callback_data_array[nmenu_items+2] = g_malloc(sizeof(struct rs_callback_data_t));
	callback_data_array[nmenu_items+2]->rs = rs;
	callback_data_array[nmenu_items+2]->specific = GINT_TO_POINTER(PRIO_3);
	callback_data_array[nmenu_items+3] = g_malloc(sizeof(struct rs_callback_data_t));
	callback_data_array[nmenu_items+3]->rs = rs;
	callback_data_array[nmenu_items+3]->specific = GINT_TO_POINTER(PRIO_U);

	prio1 = g_cclosure_new(G_CALLBACK(gui_accel_setprio_callback), (gpointer)callback_data_array[nmenu_items], NULL);
	prio2 = g_cclosure_new(G_CALLBACK(gui_accel_setprio_callback), callback_data_array[nmenu_items+1], NULL);
	prio3 = g_cclosure_new(G_CALLBACK(gui_accel_setprio_callback), callback_data_array[nmenu_items+2], NULL);
	priou = g_cclosure_new(G_CALLBACK(gui_accel_setprio_callback), callback_data_array[nmenu_items+3], NULL);

	gtk_accel_group_connect(accel_group, GDK_KP_1, 0, 0, prio1);
	gtk_accel_group_connect(accel_group, GDK_KP_2, 0, 0, prio2);
	gtk_accel_group_connect(accel_group, GDK_KP_3, 0, 0, prio3);
	gtk_accel_group_connect(accel_group, GDK_KP_0, 0, 0, priou);

	gtk_window_add_accel_group (GTK_WINDOW (window), accel_group);
	return(gtk_item_factory_get_widget (item_factory, "<main>"));
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

static GtkWidget *
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
	rs_set_wb_from_color(rs, cbdata->pixelfloat[R], cbdata->pixelfloat[G], cbdata->pixelfloat[B]);
}

void
preview_motion(RSPreviewWidget *preview, RS_PREVIEW_CALLBACK_DATA *cbdata, RS_BLOB *rs)
{
	gchar tmp[20];

	g_snprintf(tmp, 20, "%u %u %u", cbdata->pixel8[R], cbdata->pixel8[G], cbdata->pixel8[B]);
	gtk_label_set_text(GTK_LABEL(valuefield), tmp);
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

int
gui_init(int argc, char **argv, RS_BLOB *rs)
{
	GtkWidget *window;
	GtkWidget *vbox;
	GtkWidget *pane;
	GtkWidget *tools;
	GtkWidget *tools_label1, *tools_label2;
	GtkWidget *toolbox;
	GtkWidget *batchbox;
	GtkWidget *iconbox;
	GtkWidget *menubar;
	gint window_width = 0, toolbox_width = 0;
	GdkColor dashed_bg = {0, 0, 0, 0 };
	GdkColor dashed_fg = {0, 0, 65535, 0};
	GdkColor grid_bg = {0, 0, 0, 0 };
	GdkColor grid_fg = {0, 32767, 32767, 32767};
	GdkColor bgcolor = {0, 0, 0, 0 };

#ifdef PACKAGE_DATA_DIR
	gtk_window_set_default_icon_from_file(PACKAGE_DATA_DIR "/pixmaps/rawstudio.png", NULL);
#endif
	window = gui_window_make(rs);
	gtk_widget_show(window);

	/* initialize dashed gc */
	dashed = gdk_gc_new(window->window);
	gdk_gc_set_rgb_fg_color(dashed, &dashed_fg);
	gdk_gc_set_rgb_bg_color(dashed, &dashed_bg);
	gdk_gc_set_line_attributes(dashed, 1, GDK_LINE_DOUBLE_DASH, GDK_CAP_BUTT, GDK_JOIN_MITER);
	grid = gdk_gc_new(window->window);
	gdk_gc_set_rgb_fg_color(grid, &grid_fg);
	gdk_gc_set_rgb_bg_color(grid, &grid_bg);
	gdk_gc_set_line_attributes(grid, 1, GDK_LINE_SOLID, GDK_CAP_BUTT, GDK_JOIN_MITER);

	statusbar = (GtkStatusbar *) gtk_statusbar_new();
	valuefield = gtk_label_new(NULL);
	hbox = gtk_hbox_new (FALSE, 0);
	gtk_box_pack_start (GTK_BOX (hbox), valuefield, FALSE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (hbox), GTK_WIDGET (statusbar), TRUE, TRUE, 0);

	gui_set_busy(TRUE);
	tools_label1 = gtk_label_new(_("Tools"));
	tools_label2 = gtk_label_new(_("Batch"));

	tools = gtk_notebook_new();
	toolbox = make_toolbox(rs);
	batchbox = make_batchbox(rs->queue);

	gtk_notebook_append_page(GTK_NOTEBOOK(tools), toolbox, tools_label1);
	gtk_notebook_append_page(GTK_NOTEBOOK(tools), batchbox, tools_label2);

	rs->store = RS_STORE(iconbox = rs_store_new());
	g_signal_connect((gpointer) iconbox, "thumb-activated", G_CALLBACK(icon_activated), rs);

	g_signal_connect((gpointer) window, "window-state-event", G_CALLBACK(gui_fullscreen_iconbox_callback), iconbox);
	g_signal_connect((gpointer) window, "window-state-event", G_CALLBACK(gui_fullscreen_toolbox_callback), tools);

	menubar = gui_make_menubar(rs, window, iconbox, tools);

	rs->preview = rs_preview_widget_new();
	rs_preview_widget_set_cms(RS_PREVIEW_WIDGET(rs->preview), rs_cms_get_transform(rs->cms, PROFILE_DISPLAY));
	rs_conf_get_color(CONF_PREBGCOLOR, &bgcolor);
	rs_preview_widget_set_bgcolor(RS_PREVIEW_WIDGET(rs->preview), &bgcolor);
	g_signal_connect(G_OBJECT(rs->preview), "wb-picked", G_CALLBACK(preview_wb_picked), rs);
	g_signal_connect(G_OBJECT(rs->preview), "motion", G_CALLBACK(preview_motion), rs);

	pane = gtk_hpaned_new ();
	g_signal_connect_after(G_OBJECT(pane), "notify::position", G_CALLBACK(pane_position), NULL);

	gtk_paned_pack1 (GTK_PANED (pane), rs->preview, TRUE, TRUE);
	gtk_paned_pack2 (GTK_PANED (pane), tools, FALSE, TRUE);

	vbox = gtk_vbox_new (FALSE, 0);
	gtk_container_add (GTK_CONTAINER (window), vbox);

	gtk_box_pack_start (GTK_BOX (vbox), menubar, FALSE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (vbox), iconbox, FALSE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (vbox), pane, TRUE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, TRUE, 0);

	gui_status_push(_("Ready"));

	// arrange rawstudio as the user left it
	gboolean show_iconbox;
	gboolean show_toolbox;
	rs_conf_get_boolean_with_default(CONF_FULLSCREEN, &fullscreen, DEFAULT_CONF_FULLSCREEN);
	if (fullscreen)
	{       
		gtk_window_fullscreen(GTK_WINDOW(window));
		rs_conf_get_boolean_with_default(CONF_SHOW_ICONBOX_FULLSCREEN, &show_iconbox, DEFAULT_CONF_SHOW_ICONBOX_FULLSCREEN);
		rs_conf_get_boolean_with_default(CONF_SHOW_TOOLBOX_FULLSCREEN, &show_toolbox, DEFAULT_CONF_SHOW_TOOLBOX_FULLSCREEN);
		gui_widget_show(iconbox, show_iconbox, CONF_SHOW_ICONBOX_FULLSCREEN, CONF_SHOW_ICONBOX);
		gui_widget_show(tools, show_toolbox, CONF_SHOW_TOOLBOX_FULLSCREEN, CONF_SHOW_TOOLBOX);  } 
	else
	{
		gtk_window_get_size(rawstudio_window, &window_width, NULL);
		if (rs_conf_get_integer(CONF_TOOLBOX_WIDTH, &toolbox_width))
			gtk_paned_set_position(GTK_PANED(pane), window_width - toolbox_width);

		gtk_window_unfullscreen(GTK_WINDOW(window));
		rs_conf_get_boolean_with_default(CONF_SHOW_ICONBOX, &show_iconbox, DEFAULT_CONF_SHOW_TOOLBOX);
		rs_conf_get_boolean_with_default(CONF_SHOW_TOOLBOX, &show_toolbox, DEFAULT_CONF_SHOW_ICONBOX);
		gui_widget_show(iconbox, show_iconbox, CONF_SHOW_ICONBOX_FULLSCREEN, CONF_SHOW_ICONBOX);
		gui_widget_show(tools, show_toolbox, CONF_SHOW_TOOLBOX_FULLSCREEN, CONF_SHOW_TOOLBOX);
	}

	gtk_widget_show_all (window);

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
		rs_open_file(rs, argv[1]);
		rs_conf_set_integer(CONF_LAST_PRIORITY_PAGE, 0);
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
				gtk_notebook_set_current_page(GTK_NOTEBOOK(rs->store), last_priority_page);
			}
			else
				rs_conf_set_integer(CONF_LAST_PRIORITY_PAGE, 0);		
			g_free(lwd);
		}

	gui_set_busy(FALSE);
	gdk_threads_enter();
	gtk_main();
	gdk_threads_leave();
	return(0);
}
