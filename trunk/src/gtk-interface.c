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

#include <glib.h>
#include <gtk/gtk.h>
#include <glib/gstdio.h>
#include <gdk/gdkkeysyms.h>
#include <config.h>
#include "application.h"
#include "gtk-helper.h"
#include "gtk-interface.h"
#include "gtk-progress.h"
#include "conf_interface.h"
#include "rs-cache.h"
#include "rs-batch.h"
#include "gettext.h"
#include "rs-batch.h"
#include <string.h>
#include <unistd.h>
#include "filename.h"
#include "rs-store.h"
#include "rs-preview-widget.h"
#include "rs-photo.h"
#include "rs-external-editor.h"
#include "rs-actions.h"
#include "rs-dir-selector.h"
#include "rs-toolbox.h"
#include "rs-library.h"
#include "rs-tag-gui.h"
#include "rs-geo-db.h"

static GtkStatusbar *statusbar;
static gboolean fullscreen;
GtkWindow *rawstudio_window;
static gint busycount = 0;
static GtkWidget *infobox = NULL;
static GtkWidget *frame_preview_toolbox = NULL;
static GtkWidget *preview_fullscreen_filler = NULL;
GdkGC *dashed;
GdkGC *grid;
static gboolean waiting_for_user_selects_screen = FALSE;

static gboolean open_photo(RS_BLOB *rs, const gchar *filename);
static void gui_preview_bg_color_changed(GtkColorButton *widget, RS_BLOB *rs);
gboolean gui_fullscreen_changed_callback(GtkWidget *widget, gboolean fullscreen, const gchar *conf_fullscreen_key, const gchar *conf_windowed_key);
//static void gui_preference_iconview_show_filenames_changed(GtkToggleButton *togglebutton, gpointer user_data);
static GtkWidget *gui_make_menubar(RS_BLOB *rs);
static void drag_data_received(GtkWidget *widget, GdkDragContext *drag_context, gint x, gint y, GtkSelectionData *selection_data, guint info, guint t,	RS_BLOB *rs);
static gboolean gui_window_delete(GtkWidget *widget, GdkEvent  *event, gpointer user_data);
static GtkWidget *gui_window_make(RS_BLOB *rs);
static void rs_open_file_delayed(RS_BLOB *rs, const gchar *filename);
static void rs_open_file(RS_BLOB *rs, const gchar *filename);
static gboolean pane_position(GtkWidget* widget, gpointer dummy, gpointer user_data);
static void directory_activated(gpointer instance, const gchar *path, RS_BLOB *rs);

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
		GdkCursor* cursor = gdk_cursor_new(GDK_WATCH);
		gdk_window_set_cursor(GTK_WIDGET(rawstudio_window)->window, cursor);
		gdk_cursor_unref(cursor);
	}
	else
	{
		if (status>0)
			gui_status_pop(status);
		status=0;
		gdk_window_set_cursor(GTK_WIDGET(rawstudio_window)->window, NULL);
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

/* This will ensure that notifications doesn't cover important error messages */
static gboolean blinking_error = FALSE;

static gboolean
gui_statusbar_blink_helper(guint *msgid)
{
	gdk_threads_enter();
	if (msgid[1] == 0)
	{
		gtk_statusbar_remove(statusbar, gtk_statusbar_get_context_id(statusbar, "generic"), *msgid);
		gtk_widget_modify_bg(GTK_WIDGET(statusbar), GTK_STATE_NORMAL, NULL);
		g_free(msgid);
		blinking_error = FALSE;
	}
	else
	{
#if GTK_CHECK_VERSION(2,20,0)
		const static GdkColor red = {0, 0xffff, 0x2222, 0x2222 };
		if ((msgid[1] & 1) == 0)
			gtk_widget_modify_bg(gtk_statusbar_get_message_area(statusbar)->parent, GTK_STATE_NORMAL, &red);
		else
			gtk_widget_modify_bg(gtk_statusbar_get_message_area(statusbar)->parent, GTK_STATE_NORMAL, NULL);
#else
		if (msgid[2] == 0)
		{
			msgid[2] = gui_status_push("");
		}
		else
		{
			gui_status_pop(msgid[2]);
			msgid[2] = 0;
		}
#endif
		g_timeout_add(500, (GSourceFunc) gui_statusbar_blink_helper, msgid);
		msgid[1] --;
	}
	gdk_threads_leave();
	return(FALSE);
}

void
gui_status_notify(const char *text)
{
	if (blinking_error)
		return;

	guint *msgid;
	msgid = g_new(guint, 1);
	*msgid = gtk_statusbar_push(statusbar, gtk_statusbar_get_context_id(statusbar, "generic"), text);
	g_timeout_add(5000, (GSourceFunc) gui_statusbar_remove_helper, msgid);
	return;
}

void
gui_status_error(const char *text)
{
	guint *msgid;
	blinking_error = TRUE;
	msgid = g_new(guint, 3);
	*msgid = gtk_statusbar_push(statusbar, gtk_statusbar_get_context_id(statusbar, "generic"), text);
	msgid[1] = 10;
	msgid[2] = 0;
	g_timeout_add(500, (GSourceFunc) gui_statusbar_blink_helper, msgid);
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

static void
set_photo_info_label(RS_PHOTO *photo)
{
	gchar *label;
	gchar label_temp[100];
	gint w,h;

	label = rs_metadata_get_short_description(photo->metadata);
	if (rs_photo_get_original_size(photo, TRUE, &w, &h))
	{
		g_snprintf(label_temp, 100, "%s\n%s: %d, %s: %d",label, _("Width"), w, _("Height"),h );
		gtk_label_set_text(GTK_LABEL(infobox), label_temp);
	}
	else
		gtk_label_set_text(GTK_LABEL(infobox), label);

	g_free(label);
}

static gboolean
open_photo(RS_BLOB *rs, const gchar *filename)
{
	RS_PHOTO *photo;

	gui_set_busy(TRUE);
	rs_preview_widget_set_photo(RS_PREVIEW_WIDGET(rs->preview), NULL);
	photo = rs_photo_load_from_file(filename);

	if (photo)
	{
		if (rs->photo)
		{
			rs_photo_close(rs->photo);
			if (rs->photo->metadata && rs->photo->metadata->thumbnail && rs->photo->thumbnail_filter)
				rs_store_update_thumbnail(rs->store, rs->photo->filename, rs->photo->metadata->thumbnail);
		}
	}
	else
	{
		rs_io_idle_unpause();
		gui_set_busy(FALSE);
		rs->post_open_event = NULL;
		return FALSE;
	}

	set_photo_info_label(photo);

	gdk_threads_leave();
	rs_preview_widget_lock_renderer(RS_PREVIEW_WIDGET(rs->preview));
	gdk_threads_enter();
	rs_set_photo(rs, photo);

	/* We need check if we should calculate and set auto wb here because the photo needs to be loaded for filterchain to work */
	gint i;
	g_object_ref(rs->filter_demosaic_cache);
	rs->photo->auto_wb_filter = rs->filter_demosaic_cache;
	for (i=0;i<3;i++)
	{
		if (photo && photo->settings[i] && photo->settings[i]->wb_ascii && g_strcmp0(photo->settings[i]->wb_ascii, PRESET_WB_AUTO) == 0)
			rs_photo_set_wb_auto(rs->photo, i);
	}
	/* Set photo in preview-widget */
	rs_preview_widget_set_photo(RS_PREVIEW_WIDGET(rs->preview), photo);
	rs->photo->proposed_crop = NULL;
	rs_preview_widget_unlock_renderer(RS_PREVIEW_WIDGET(rs->preview));
	rs_preview_widget_update(RS_PREVIEW_WIDGET(rs->preview), TRUE);
	GUI_CATCHUP();
	if (rs->photo && NULL==rs->photo->crop && rs->photo->proposed_crop)
		rs_photo_set_crop(rs->photo, rs->photo->proposed_crop);
	rs_core_actions_update_menu_items(rs);
	GUI_CATCHUP();
	if (NULL != rs->post_open_event)
	{
		rs_core_action_group_activate(rs->post_open_event);
		rs->post_open_event = NULL;
	}
	gui_set_busy(FALSE);
	return TRUE;
}

static void
icon_activated(gpointer instance, const gchar *name, RS_BLOB *rs)
{
	guint msgid;

	if (name!=NULL)
	{
		GList *selected = NULL;
		rs_io_idle_pause();
		gui_set_busy(TRUE);
		msgid = gui_status_push(_("Opening photo ..."));
		rs->signal = MAIN_SIGNAL_LOADING;
		GTK_CATCHUP();

		/* Read currently selected filename, it may or may not (!) be the same as served in name */
		selected = rs_store_get_selected_names(rs->store);
		if (g_list_length(selected)==1)
		{
			name = g_list_nth_data(selected, 0);
			g_list_free(selected);
		}

		if (rs->photo && rs->photo->filename)
			if (!g_strcmp0(rs->photo->filename, name))
			{
				gui_status_pop(msgid);
				gui_set_busy(FALSE);
				rs_io_idle_unpause();
				rs->signal = MAIN_SIGNAL_IDLE;
				return;
			}

		if (!open_photo(rs, name))
		{
			gui_status_pop(msgid);
			gui_status_notify(_("Couldn't open photo"));
		}
		else
		{
			gui_status_pop(msgid);
			gui_status_notify(_("Image opened"));
			rs_window_set_title(rs->photo->filename);
		}
	}
	GTK_CATCHUP();
	gui_set_busy(FALSE);
	rs_io_idle_unpause();
	rs->signal = MAIN_SIGNAL_IDLE;
	
}

static void
group_activated(gpointer instance, GList *names, RS_BLOB *rs)
{
	gchar *filename = (gchar *) g_list_nth_data(names, 0);
	icon_activated(instance, filename, rs);
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
	const gchar* next_name = NULL;
	gui_set_busy(TRUE);
	GTK_CATCHUP();

	selected = rs_store_get_selected_iters(rs->store);
	num_selected = g_list_length(selected);

	/* If moving to another iconview, select next */
	GList *selected_names = rs_store_get_selected_names(rs->store);
	if (g_list_length(selected_names))
		next_name = (const gchar*)(g_list_last(selected_names)->data);
	else if (rs->photo)
		next_name = rs->photo->filename;

	/* Select next image if moving */
	if (next_name)
		next_name = rs_store_get_prevnext(rs->store, next_name, 10 + prio);

	if (next_name)
		gui_set_block_keyboard(TRUE);

/* Iterate throuh all selected thumbnails */
	for(i=0;i<num_selected;i++)
	{
		rs_store_set_flags(rs->store, NULL, g_list_nth_data(selected, i), &prio, NULL, NULL);
	}
	g_list_free(selected);

	/* Change priority for currently open photo */
	if (rs->photo && rs_store_is_photo_selected(rs->store, rs->photo->filename))
	{
		rs->photo->priority = prio;
		rs_store_set_flags(rs->store, rs->photo->filename, NULL, &prio, NULL, NULL);
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

	/* If deleting, and no next image, we are most likely at last image */
	if (prio == 51 && NULL == next_name) 
		next_name = rs_store_get_first_last(rs->store, 2);

	/* Load next image if deleting */
	if (next_name)
		rs_store_set_selected_name(rs->store, next_name, TRUE);

	g_string_free(gs, TRUE);
	gui_set_busy(FALSE);
	GTK_CATCHUP();
	gui_set_block_keyboard(FALSE);
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

gboolean
gui_fullscreen_changed(GtkWidget *widget, gboolean is_fullscreen, const gchar *action, 
																gboolean default_fullscreen, gboolean default_windowed,
																const gchar *conf_fullscreen_key, const gchar *conf_windowed_key)
{
	gboolean show_widget;
	if (is_fullscreen)
	{
		gboolean show_widget_default;
		rs_conf_get_boolean_with_default(conf_windowed_key, &show_widget_default, default_fullscreen);
		rs_conf_get_boolean_with_default(conf_fullscreen_key, &show_widget, show_widget_default);
		fullscreen = TRUE;
		gui_widget_show(widget, show_widget, conf_fullscreen_key, conf_windowed_key);
	}
	else
	{
		rs_conf_get_boolean_with_default(conf_windowed_key, &show_widget, default_windowed);
		fullscreen = FALSE;
		gui_widget_show(widget, show_widget, conf_fullscreen_key, conf_windowed_key);
	}
	gtk_toggle_action_set_active(GTK_TOGGLE_ACTION(rs_core_action_group_get_action(action)) ,show_widget);
	return(FALSE);
}


static gboolean
gui_histogram_height_changed(GtkAdjustment *caller, RS_BLOB *rs)
{
	const gint newheight = (gint) caller->value;
	rs_conf_set_integer(CONF_HISTHEIGHT, newheight);
	return(FALSE);
}

typedef struct
{
	GdkScreen *screen;
	gint monitor_num;
} MonitorInfo;

static void
gui_enable_preview_screen(RS_BLOB *rs, const gchar *screen_name, int monitor_num)
{
	gboolean is_enabled;
	GdkRectangle rect;
	if (rs_conf_get_boolean_with_default("fullscreen-preview", &is_enabled, FALSE))
		if (is_enabled)
			return;

	if (waiting_for_user_selects_screen)
		return;

	GdkDisplay *open_display = gdk_display_open(screen_name);
	GdkScreen *open_screen = gdk_display_get_default_screen(open_display);
	if (NULL == open_screen)
	{
		gui_status_notify(_("Unable to locate screen for fullscreen preview"));
		return;
	}

	rs_preview_widget_lock_renderer(RS_PREVIEW_WIDGET(rs->preview));
	gdk_screen_get_monitor_geometry(open_screen, monitor_num, &rect);
	rs->window_preview_screen = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_screen(GTK_WINDOW(rs->window_preview_screen), open_screen);
	gtk_window_move(GTK_WINDOW(rs->window_preview_screen), rect.x, rect.y);
	gtk_window_maximize(GTK_WINDOW(rs->window_preview_screen));
	gtk_window_fullscreen(GTK_WINDOW(rs->window_preview_screen));

	/* Make sure we don't grab focus from main window */
	g_object_set(GTK_WINDOW(rs->window_preview_screen),
		"accept-focus", FALSE,
		NULL);

	/* Re-parent preview*/
	gtk_widget_reparent(rs->preview, rs->window_preview_screen);

	/* Add something to the preview area */
	preview_fullscreen_filler = gtk_label_new(_("Press F10 to return preview to this window"));
	gtk_container_add(GTK_CONTAINER(frame_preview_toolbox), preview_fullscreen_filler);

	gtk_widget_show_all(GTK_WIDGET(frame_preview_toolbox));
	gtk_widget_show_all(rs->window_preview_screen);
	rs_conf_set_boolean("fullscreen-preview", TRUE);
	GTK_CATCHUP();
	rs_preview_widget_unlock_renderer(RS_PREVIEW_WIDGET(rs->preview));
	rs_preview_widget_update(RS_PREVIEW_WIDGET(rs->preview), TRUE);
	GUI_CATCHUP_DISPLAY(open_display);
}

void
gui_disable_preview_screen(RS_BLOB *rs)
{
	if (waiting_for_user_selects_screen)
		return;
	gboolean is_enabled;
	if (rs_conf_get_boolean_with_default("fullscreen-preview", &is_enabled, FALSE))
		if (!is_enabled)
			return;
	gtk_container_remove(GTK_CONTAINER(frame_preview_toolbox), preview_fullscreen_filler);
	gtk_widget_reparent(rs->preview, frame_preview_toolbox);
	gtk_widget_destroy(rs->window_preview_screen);
	rs->window_preview_screen = NULL;
	rs_conf_set_boolean("fullscreen-preview", FALSE);
}

typedef struct
{
	GSList *all_window_widgets;
	guint status;
	RS_BLOB *rs;
	MonitorInfo *monitor;
	GSList *all_monitors;
} ScreenWindowInfo;

static void
screen_selected(GtkEntry *entry, gint response_id, gpointer user_data)
{
	waiting_for_user_selects_screen = FALSE;
	ScreenWindowInfo *info = (ScreenWindowInfo *)user_data;
	gui_status_pop(info->status);
	if (response_id == GTK_RESPONSE_OK)
	{
		GdkScreen *screen = GDK_SCREEN(info->monitor->screen);
		g_debug("screen: %p", screen);
		gchar *screen_name = gdk_screen_make_display_name(screen);
		gui_enable_preview_screen(info->rs, screen_name, info->monitor->monitor_num);
		g_free(screen_name);
	}
	else
	{
		/* We didn't activate after all */
		GtkAction *action = rs_core_action_group_get_action("FullscreenPreview");
		gtk_toggle_action_set_active(GTK_TOGGLE_ACTION(action), FALSE);
	}

	/* Delete widgets */
	GSList *iter = info->all_window_widgets;
	do {
		gtk_widget_destroy(GTK_WIDGET(iter->data));
		iter = iter->next;
	} while (iter);

	/* Free monitor info */
	iter = info->all_monitors;
	do {
		g_free(iter->data);
		iter = iter->next;
	} while (iter);
	
	g_slist_free(info->all_window_widgets);
	g_slist_free(info->all_monitors);
	g_free(info);
}

void
gui_select_preview_screen(RS_BLOB *rs)
{
	gboolean is_enabled;
	MonitorInfo *cmon;
	GdkRectangle rect;

	if (rs_conf_get_boolean_with_default("fullscreen-preview", &is_enabled, FALSE))
		if (is_enabled)
			return;

	if (waiting_for_user_selects_screen)
		return;

	/* Get information about current screen */
	GdkScreen *main_screen;
	main_screen = gtk_window_get_screen(GTK_WINDOW(rs->window));
	int main_screen_monitor = gdk_screen_get_monitor_at_window(main_screen, gtk_widget_get_window(rs->window));
	gchar *main_screen_name = gdk_screen_make_display_name(main_screen);

	/* For some obscure reason, if seems that gdk_display_manager_list_displays(..) */
	/* returns one display on first call, two on second and so on. */
	/* Therefore we have the display list as a static variable. */
	/* I guess you have to restart Rawstudio to detect new displays */
	static GSList *disps = NULL;
	GdkDisplayManager *disp_man = gdk_display_manager_get();
	if (!disps)
		disps = gdk_display_manager_list_displays(disp_man);

	GSList *cdisp = disps;
	GSList *all_monitors = NULL;
	do {
		gint num_screens = gdk_display_get_n_screens((GdkDisplay*)cdisp->data);
		gint i;
		for (i = 0; i < num_screens; i++)
		{
			GdkScreen* screen = gdk_display_get_screen(cdisp->data, i);
			gchar *screen_name = gdk_screen_make_display_name(screen);
			gint mons = gdk_screen_get_n_monitors(screen);
			gint j;
			for (j = 0; j < mons; j++)
			{
				/* We do not add the monitor with the current window */
				if (!(0 == g_strcmp0(screen_name, main_screen_name) && j == main_screen_monitor))
				{
					cmon = g_malloc(sizeof(MonitorInfo));
					cmon->screen = screen;
					cmon->monitor_num = j;
					all_monitors = g_slist_append(all_monitors, cmon);
				}
			}
		}
	} while ((cdisp = cdisp->next));

	gint total_monitors = g_slist_length(all_monitors);
	if (total_monitors <= 0)
	{
		g_free(main_screen_name);
		gui_status_notify(_("Unable to detect more than one monitor. Cannot open fullscreen preview"));
		return;
	}

	/* If two displays, just open on the other */
	if (total_monitors == 1)
	{
		cmon = all_monitors->data;
		gui_enable_preview_screen(rs, gdk_screen_make_display_name(cmon->screen), cmon->monitor_num);
		g_free(all_monitors->data);
		g_slist_free(all_monitors);
	}
	else
	{
		/* Pop up selection box */
		GSList *screen_widgets = NULL;
		guint status = gui_status_push(_("Select screen to open fullscreen preview"));
		waiting_for_user_selects_screen = TRUE;
		ScreenWindowInfo **info = g_malloc(sizeof(ScreenWindowInfo*) * total_monitors);
		gint i;
		for (i = 0; i < total_monitors; i++)
		{
			info[i] = g_malloc(sizeof(ScreenWindowInfo));
			cmon = g_slist_nth_data(all_monitors, i);
			info[i]->monitor = cmon;
			info[i]->status = status;
			info[i]->rs = rs;
			info[i]->all_monitors = all_monitors;
			gdk_screen_get_monitor_geometry(cmon->screen, cmon->monitor_num, &rect);

			/* Create dialog */
			GtkWidget *dialog = gtk_dialog_new();
			gtk_window_set_title(GTK_WINDOW(dialog), _("Select Screen for fullscreen preview"));
			gtk_window_set_screen(GTK_WINDOW(dialog), cmon->screen);
			gtk_window_move(GTK_WINDOW(dialog), rect.x+(rect.width/2), rect.y+(rect.height/2));
			gtk_dialog_set_has_separator(GTK_DIALOG(dialog), FALSE);

			gtk_dialog_add_button(GTK_DIALOG(dialog), GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL);
			gtk_dialog_add_button(GTK_DIALOG(dialog), GTK_STOCK_OK, GTK_RESPONSE_OK);
				
			GtkWidget *content = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
			GtkWidget *label = gtk_label_new(_("Select OK to use this screen for fullscreen preview"));
			gtk_box_pack_start(GTK_BOX(content), label, TRUE, TRUE, 10);

			/* Connect response and show it */
			g_signal_connect(dialog, "response", G_CALLBACK(screen_selected), info[i]);
			gtk_widget_show_all(dialog);
			screen_widgets = g_slist_append(screen_widgets, dialog);
		}

		/* Attach all widgets to info */
		for (i = 0; i < total_monitors; i++)
			info[i]->all_window_widgets = screen_widgets;
	}
	g_free(main_screen_name);
}

static void
gui_preference_use_system_theme(GtkToggleButton *togglebutton, gpointer user_data)
{
	if (togglebutton->active)
	{
		gui_select_theme(STANDARD_GTK_THEME);
	}
	else
	{
		gui_select_theme(RAWSTUDIO_THEME);
	}
}

typedef struct {
	GtkWidget *example_label;
	GtkWidget *event;
	const gchar *output_type;
	const gchar *filename;
} QUICK_EXPORT;

static void
update_example(QUICK_EXPORT *quick)
{
	gchar *parsed;
	gchar *final = "";
	RSOutput *output;
	GtkLabel *example = GTK_LABEL(quick->example_label);

	parsed = filename_parse(quick->filename, "filename", 0, TRUE);

	output = rs_output_new(quick->output_type);
	if (output)
	{
		final = g_strdup_printf("<small>%s.%s</small>", parsed, rs_output_get_extension(output));
		g_object_unref(output);
	}

	gtk_label_set_markup(example, final);

	g_free(parsed);
	g_free(final);
}

static void
directory_chooser_changed(GtkFileChooser *chooser, gpointer user_data)
{
	gchar *directory;

	directory = gtk_file_chooser_get_filename(chooser);
	if (directory)
		rs_conf_set_string("quick-export-directory", directory);
}

static void
filetype_changed(gpointer active, gpointer user_data)
{
	QUICK_EXPORT *quick = (QUICK_EXPORT *) user_data;
	GtkWidget *event = quick->event;
	GtkWidget *options;
	RSOutput *output;
	const gchar *identifier = g_type_name(GPOINTER_TO_INT(active));

	quick->output_type = identifier;

	options = gtk_bin_get_child(GTK_BIN(event));

	if (options)
		gtk_widget_destroy(options);

	/* Try to instantiate the output plugin to build options widget */
	output = rs_output_new(identifier);
	if (output)
	{
		options = rs_output_get_parameter_widget(output, "quick-export");

		gtk_container_add(GTK_CONTAINER(event), options);
		gtk_widget_show_all(event);

		g_object_unref(output);
	}
	update_example(quick);
}

static void
filename_entry_changed(GtkEntry *entry, gpointer user_data)
{
	QUICK_EXPORT *quick = (QUICK_EXPORT *) user_data;

	quick->filename = gtk_entry_get_text(entry);

	update_example(quick);
}

static void
closed_preferences(GtkEntry *entry, gint response_id, gpointer user_data)
{
	RS_BLOB *rs = (RS_BLOB*)user_data;
	rs_preview_widget_update(RS_PREVIEW_WIDGET(rs->preview), TRUE);
	gtk_widget_destroy(GTK_WIDGET(entry));
}

static void
colorspace_changed(RSColorSpaceSelector *selector, const gchar *color_space_name, gpointer user_data)
{
	rs_conf_set_string((const gchar*)user_data, color_space_name);
	/* OMG, gconf_client_set_string, is applied in the main loop, until then, old values are returned */
	GTK_CATCHUP();
	rs_preview_widget_update_display_colorspace(RS_PREVIEW_WIDGET(rs_get_blob()->preview), TRUE);
	rs_preview_widget_update(RS_PREVIEW_WIDGET(rs_get_blob()->preview), TRUE);
}

static GtkWidget *
gui_make_preference_quick_export(void)
{
	gpointer active;
	QUICK_EXPORT *quick;
	GtkWidget *page;
	GtkWidget *directory_hbox;
	GtkWidget *directory_label;
	GtkWidget *directory_chooser;
	gchar *directory;

	GtkWidget *filename_hbox;
	GtkWidget *filename_label;
	GtkWidget *filename_chooser;

	GtkWidget *filetype_hbox;
	GtkWidget *filetype_label;
	RS_CONFBOX *filetype_box;
	GtkWidget *filename_entry;
	GtkWidget *filetype_event;

	GtkWidget *example_hbox;
	GtkWidget *example_label1;
	GtkWidget *example_label2;

	page = gtk_vbox_new(FALSE, 4);
	gtk_container_set_border_width(GTK_CONTAINER (page), 6);

	/* Carrier */
	quick = g_new0(QUICK_EXPORT, 1);
	g_object_set_data_full(G_OBJECT(page), "quick", quick, g_free);

	/* Directory */
	directory_hbox = gtk_hbox_new(FALSE, 0);
	directory_label = gtk_label_new(_("Directory:"));

	directory_chooser = gtk_file_chooser_button_new(_("Choose Output Directory"),
		GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER);
	directory = rs_conf_get_string("quick-export-directory");

	if (directory && g_path_is_absolute(directory))
		gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(directory_chooser), directory);

	g_signal_connect (directory_chooser, "current_folder_changed", G_CALLBACK(directory_chooser_changed), NULL);

	gtk_misc_set_alignment(GTK_MISC(directory_label), 0.0, 0.5);
	gtk_box_pack_start(GTK_BOX(directory_hbox), directory_label, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(directory_hbox), directory_chooser, FALSE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(page), directory_hbox, FALSE, TRUE, 0);

	/* Filename */
	filename_hbox = gtk_hbox_new(FALSE, 0);
	filename_label = gtk_label_new(_("Filename Template:"));
	filename_chooser = rs_filename_chooser_button_new(NULL, "quick-export-filename");
	filename_entry = g_object_get_data(G_OBJECT(filename_chooser), "entry");
	g_signal_connect(filename_entry, "changed", G_CALLBACK(filename_entry_changed), quick);
	quick->filename = gtk_entry_get_text(GTK_ENTRY(filename_entry));

	gtk_misc_set_alignment(GTK_MISC(filename_label), 0.0, 0.5);
	gtk_box_pack_start(GTK_BOX(filename_hbox), filename_label, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(filename_hbox), filename_chooser, FALSE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(page), filename_hbox, FALSE, TRUE, 0);

	/* Example filename */
	example_hbox = gtk_hbox_new(FALSE, 0);
	example_label1 = gtk_label_new(_("Filename Example:"));
	example_label2 = gtk_label_new(NULL);
	quick->example_label = example_label2;

	gtk_misc_set_alignment(GTK_MISC(example_label1), 0.0, 0.5);
	gtk_box_pack_start(GTK_BOX(example_hbox), example_label1, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(example_hbox), example_label2, FALSE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(page), example_hbox, FALSE, TRUE, 0);

	/* Filetype */
	filetype_hbox = gtk_hbox_new(FALSE, 0);
	filetype_label = gtk_label_new(_("File Type:"));
	filetype_box = gui_confbox_filetype_new("quick-export-filetype");
	filetype_event = gtk_event_box_new();
	quick->event = filetype_event;
	gui_confbox_set_callback(filetype_box, quick, filetype_changed);
	active = gui_confbox_get_active(filetype_box);
	if (!active)
		active = GUINT_TO_POINTER(g_type_from_name("RSJpegfile"));
	quick->output_type = g_type_name(GPOINTER_TO_INT(active));

	/* Load default from conf, or use RSJpegfile */
	gui_confbox_load_conf(filetype_box, "RSJpegfile");

	gtk_misc_set_alignment(GTK_MISC(filetype_label), 0.0, 0.5);
	gtk_box_pack_start(GTK_BOX(filetype_hbox), filetype_label, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(filetype_hbox), gui_confbox_get_widget(filetype_box), FALSE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(page), filetype_hbox, FALSE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(page), filetype_event, FALSE, TRUE, 0);

	filetype_changed(active, quick);
	update_example(quick);

	return page;
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
	GtkWidget *cs_hbox;
	GtkWidget *cs_label;
	GtkWidget* cs_widget;
	GtkWidget *local_cache_check;
	GtkWidget *enfuse_cache_check;
	GtkWidget *system_theme_check;
	gchar *str;

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

	dialog = gtk_dialog_new();
	gtk_window_set_title(GTK_WINDOW(dialog), _("Preferences"));
	gtk_window_set_resizable(GTK_WINDOW(dialog), FALSE);
	gtk_window_set_type_hint(GTK_WINDOW(dialog), GDK_WINDOW_TYPE_HINT_DIALOG);
	gtk_window_set_destroy_with_parent(GTK_WINDOW(dialog), TRUE);
	gtk_dialog_set_has_separator (GTK_DIALOG(dialog), FALSE);
	g_signal_connect_swapped(dialog, "delete_event",
		G_CALLBACK (gtk_widget_destroy), dialog);

	vbox = GTK_DIALOG (dialog)->vbox;

	preview_page = gtk_vbox_new(FALSE, 4);
	gtk_container_set_border_width (GTK_CONTAINER (preview_page), 6);
	colorsel_hbox = gtk_hbox_new(FALSE, 0);
	colorsel_label = gtk_label_new(_("Preview Background Color:"));
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
	histsize_label = gtk_label_new(_("Histogram Height:"));
	gtk_misc_set_alignment(GTK_MISC(histsize_label), 0.0, 0.5);
	histsize_adj = gtk_adjustment_new(histogram_height, 15.0, 500.0, 1.0, 10.0, 0.0);
	g_signal_connect(histsize_adj, "value_changed",
		G_CALLBACK(gui_histogram_height_changed), rs);
	histsize = gtk_spin_button_new(GTK_ADJUSTMENT(histsize_adj), 1, 0);
	gtk_box_pack_start (GTK_BOX (histsize_hbox), histsize_label, TRUE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (histsize_hbox), histsize, FALSE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (preview_page), histsize_hbox, FALSE, TRUE, 0);

	system_theme_check = checkbox_from_conf(CONF_USE_SYSTEM_THEME, _("Use System Theme"), DEFAULT_CONF_USE_SYSTEM_THEME);
	gtk_box_pack_start (GTK_BOX (preview_page), system_theme_check, FALSE, TRUE, 0);
	g_signal_connect ((gpointer) system_theme_check, "toggled",
		G_CALLBACK (gui_preference_use_system_theme), rs);

	local_cache_check = checkbox_from_conf(CONF_CACHEDIR_IS_LOCAL, _("Place Cache in Home Directory"), FALSE);
	gtk_box_pack_start (GTK_BOX (preview_page), local_cache_check, FALSE, TRUE, 0);

	enfuse_cache_check = checkbox_from_conf(CONF_ENFUSE_CACHE, _("Cache images when enfusing (speed for memory)"), DEFAULT_CONF_ENFUSE_CACHE);
	gtk_box_pack_start (GTK_BOX (preview_page), enfuse_cache_check, FALSE, TRUE, 0);
	
	cs_hbox = gtk_hbox_new(FALSE, 0);
	cs_label = gtk_label_new(_("Display Colorspace:"));
	cs_widget = rs_color_space_selector_new();
	rs_color_space_selector_add_single(RS_COLOR_SPACE_SELECTOR(cs_widget), "_builtin_display", "System Display Profile", NULL);
	rs_color_space_selector_add_all(RS_COLOR_SPACE_SELECTOR(cs_widget));
	rs_color_space_selector_set_selected_by_name(RS_COLOR_SPACE_SELECTOR(cs_widget), "_builtin_display");
	if ((str = rs_conf_get_string("display-colorspace")))
		rs_color_space_selector_set_selected_by_name(RS_COLOR_SPACE_SELECTOR(cs_widget), str);
	g_signal_connect(cs_widget, "colorspace-selected", G_CALLBACK(colorspace_changed), "display-colorspace");
	gtk_box_pack_start (GTK_BOX (cs_hbox), cs_label, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (cs_hbox), cs_widget, TRUE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (preview_page), cs_hbox, FALSE, TRUE, 0);

	cs_hbox = gtk_hbox_new(FALSE, 0);
	cs_label = gtk_label_new(_("Histogram, Curve & Exp. Mask:"));
	cs_widget = rs_color_space_selector_new();
	rs_color_space_selector_add_all(RS_COLOR_SPACE_SELECTOR(cs_widget));
	rs_color_space_selector_set_selected_by_name(RS_COLOR_SPACE_SELECTOR(cs_widget), "RSSrgb");
	if ((str = rs_conf_get_string("exposure-mask-colorspace")))
		rs_color_space_selector_set_selected_by_name(RS_COLOR_SPACE_SELECTOR(cs_widget), str);
	g_signal_connect(cs_widget, "colorspace-selected", G_CALLBACK(colorspace_changed), "exposure-mask-colorspace");
	gtk_box_pack_start (GTK_BOX (cs_hbox), cs_label, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (cs_hbox), cs_widget, TRUE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (preview_page), cs_hbox, FALSE, TRUE, 0);

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
	

	notebook = gtk_notebook_new();
	gtk_container_set_border_width (GTK_CONTAINER (notebook), 6);
	gtk_notebook_append_page(GTK_NOTEBOOK(notebook), preview_page, gtk_label_new(_("General")));
	//gtk_notebook_append_page(GTK_NOTEBOOK(notebook), batch_page, gtk_label_new(_("Batch")));
	gtk_notebook_append_page(GTK_NOTEBOOK(notebook), gui_make_preference_quick_export(), gtk_label_new(_("Quick Export")));
	gtk_box_pack_start (GTK_BOX (vbox), notebook, FALSE, FALSE, 0);

	button_close = gtk_button_new_from_stock(GTK_STOCK_CLOSE);
	gtk_dialog_add_action_widget (GTK_DIALOG (dialog), button_close, GTK_RESPONSE_CLOSE);
	g_signal_connect(dialog, "response", G_CALLBACK(closed_preferences), rs);
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
	static GStaticMutex lock = G_STATIC_MUTEX_INIT;
	static GtkUIManager *ui_manager = NULL;

	g_static_mutex_lock(&lock);
	if (!ui_manager)
	{
		GError *error = NULL;
		ui_manager = gtk_ui_manager_new ();
		gboolean client_mode;
		rs_conf_get_boolean("client-mode", &client_mode);
		if (client_mode)
			gtk_ui_manager_add_ui_from_file (ui_manager, PACKAGE_DATA_DIR G_DIR_SEPARATOR_S PACKAGE G_DIR_SEPARATOR_S "ui-client.xml", &error);
		else
			gtk_ui_manager_add_ui_from_file (ui_manager, PACKAGE_DATA_DIR G_DIR_SEPARATOR_S PACKAGE G_DIR_SEPARATOR_S "ui.xml", &error);
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
window_state_event(GtkWidget *widget, GdkEventWindowState *event, gpointer user_data)
{
	GtkWindow *window = GTK_WINDOW(widget);
	gint width,height,pos_x,pos_y;
	gboolean maximized;

	switch (event->type)
	{
		case GDK_WINDOW_STATE:
		{
			maximized = !!(event->new_window_state & GDK_WINDOW_STATE_MAXIMIZED);
			rs_conf_set_boolean(CONF_MAIN_WINDOW_MAXIMIZED, maximized);
			if (!maximized)
			{ 
				gtk_window_get_size(window, &width, &height);
				gtk_window_get_position(window, &pos_x, &pos_y);
				rs_conf_set_integer(CONF_MAIN_WINDOW_WIDTH, width);
				rs_conf_set_integer(CONF_MAIN_WINDOW_HEIGHT, height);
				rs_conf_set_integer(CONF_MAIN_WINDOW_POS_X, pos_x);
				rs_conf_set_integer(CONF_MAIN_WINDOW_POS_Y, pos_y);
			}
			break;
		}
		default:
			break;
	}
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

static gboolean
gui_window_delete(GtkWidget *widget, GdkEvent  *event, gpointer user_data)
{
	gboolean client_mode;
	rs_conf_get_boolean("client-mode", &client_mode);
	if (client_mode)
		rs_core_action_group_activate("SaveAndQuit");
	else
		rs_core_action_group_activate("Quit");
	return TRUE;
}

static GtkWidget *
gui_window_make(RS_BLOB *rs)
{
	static const GtkTargetEntry targets[] = { { "text/uri-list", 0, 0 } };

	gint width = DEFAULT_CONF_MAIN_WINDOW_WIDTH;
	gint height = DEFAULT_CONF_MAIN_WINDOW_HEIGHT;
	gint pos_x = DEFAULT_CONF_MAIN_WINDOW_POS_X;
	gint pos_y = DEFAULT_CONF_MAIN_WINDOW_POS_Y;
	gboolean maximized;
	rs_conf_get_integer(CONF_MAIN_WINDOW_WIDTH, &width);
	rs_conf_get_integer(CONF_MAIN_WINDOW_HEIGHT, &height);
	rs_conf_get_integer(CONF_MAIN_WINDOW_POS_X, &pos_x);
	rs_conf_get_integer(CONF_MAIN_WINDOW_POS_Y, &pos_y);
	rs_conf_get_boolean_with_default(CONF_MAIN_WINDOW_MAXIMIZED, &maximized, DEFAULT_CONF_MAIN_WINDOW_MAXIMIZED);

	rawstudio_window = GTK_WINDOW(gtk_window_new (GTK_WINDOW_TOPLEVEL));
	if (!maximized)
	{
//		gtk_window_set_position((GtkWindow *) rawstudio_window, GTK_WIN_POS_NONE);
//		gtk_window_move((GtkWindow *) rawstudio_window, pos_x, pos_y);
		gtk_window_resize((GtkWindow *) rawstudio_window, width, height);
	}
	else
	{
		gtk_window_maximize((GtkWindow *) rawstudio_window);
	}

	rs_window_set_title(NULL);
	g_signal_connect((gpointer) rawstudio_window, "delete_event", G_CALLBACK(gui_window_delete), NULL);
	g_signal_connect((gpointer) rawstudio_window, "key_press_event", G_CALLBACK(window_key_press_event), NULL);

	gtk_drag_dest_set(GTK_WIDGET(rawstudio_window), GTK_DEST_DEFAULT_ALL, targets, 1, GDK_ACTION_COPY);
	g_signal_connect((gpointer) rawstudio_window, "drag_data_received", G_CALLBACK(drag_data_received), rs);
	g_signal_connect((gpointer) rawstudio_window, "window-state-event", G_CALLBACK(window_state_event), rs);
//	g_signal_connect((gpointer) rawstudio_window, "configure-event", G_CALLBACK(window_configure_event), rs);


	gtk_widget_set_name (GTK_WIDGET(rawstudio_window), "rawstudio-widget");

	return(GTK_WIDGET(rawstudio_window));
}

void
preview_wb_picked(RSPreviewWidget *preview, RS_PREVIEW_CALLBACK_DATA *cbdata, RS_BLOB *rs)
{
	if ((cbdata->pixelfloat[R]>0.0) && (cbdata->pixelfloat[G]>0.0) && (cbdata->pixelfloat[B]>0.0))
		rs_photo_set_wb_from_color(rs->photo, rs->current_setting,
			cbdata->pixelfloat[R],
			cbdata->pixelfloat[G],
			cbdata->pixelfloat[B]);
}

void
preview_motion(RSPreviewWidget *preview, RS_PREVIEW_CALLBACK_DATA *cbdata, GtkLabel **valuefield)
{
	gint c;
	gchar tmp[20];

	for(c=0;c<3;c++)
	{
		g_snprintf(tmp, 20, "%u", cbdata->pixel8[c]);
		gtk_label_set_markup (valuefield[c], tmp);
	}
}

void
preview_leave(RSPreviewWidget *preview, RS_PREVIEW_CALLBACK_DATA *cbdata, GtkLabel **valuefield)
{
	gint c;

	for(c=0;c<3;c++)
		gtk_label_set_text(valuefield[c], "-");
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

static gboolean
open_directory_in_mainloop(gpointer data)
{
	gpointer *foo = data;

	gdk_threads_enter();
	directory_activated(NULL, (gchar *) foo[1], (RS_BLOB *) (foo[0]));
	g_free(foo[1]);
	gdk_threads_leave();

	return FALSE;
}

static void
rs_open_directory_delayed(RS_BLOB *rs, const gchar *path)
{
	gpointer *carrier = g_new(gpointer, 2);
	/* Load image in mainloop */
	carrier[0] = (gpointer) rs;
	carrier[1] = (gpointer) g_strdup(path);
	g_idle_add(open_directory_in_mainloop, carrier);
}

static void
rs_open_file(RS_BLOB *rs, const gchar *filename)
{
	if (filename)
	{	
		gchar *abspath;
		gchar *temppath = g_strdup(filename);
		gchar *lwd;

		rs_io_idle_pause();
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
			rs_store_set_selected_name(rs->store, abspath, FALSE);
			g_free(lwd);
		}
		else
			rs_store_load_directory(rs->store, NULL);
		g_free(abspath);
		rs_io_idle_unpause();
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
	static gboolean opening = FALSE;
	if (opening)
		return;

	opening = TRUE;
	/* Set this, so directory is reset, if a crash occurs during load, */
	/* directory will be reset on next startup */
	rs_conf_set_string(CONF_LWD, g_get_home_dir());

	rs_io_idle_pause();
	rs_store_remove(rs->store, NULL, NULL);
	guint msgid = gui_status_push(_("Opening directory..."));
	gui_set_busy(TRUE);
	GTK_CATCHUP();
	if (rs_store_load_directory(rs->store, path) >= 0)
			rs_conf_set_string(CONF_LWD, path);
	rs_window_set_title(path);
	GTK_CATCHUP();
	gui_status_pop(msgid);
	gui_set_busy(FALSE);
	rs_io_idle_unpause();

	/* Restore directory */
	rs_conf_set_string(CONF_LWD, path);
	opening = FALSE;
}

static void
snapshot_changed(RSToolbox *toolbox, gint snapshot, RS_BLOB *rs)
{
	/* Switch preview widget to the correct snapshot */
	rs_preview_widget_set_snapshot(RS_PREVIEW_WIDGET(rs->preview), 0, snapshot);
	rs->current_setting = snapshot;
}

void
rs_window_set_title(const char *str)
{
	GString * window_title;
	gboolean client_mode;
	rs_conf_get_boolean("client-mode", &client_mode);
	if (client_mode)
		window_title = g_string_new(_("Rawstudio Client Mode"));
	else
		window_title = g_string_new(_("Rawstudio"));
	if (str)
	{
		window_title = g_string_append(window_title, " - ");
		window_title = g_string_append(window_title, str);
	}
	gtk_window_set_title(GTK_WINDOW(rawstudio_window), window_title->str);
	g_string_free(window_title, TRUE);	
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
	GtkWidget *library_vbox;
	gint window_width = 0, toolbox_width = 0;
	GdkColor dashed_bg = {0, 0, 0, 0 };
	GdkColor dashed_fg = {0, 0, 65535, 0};
	GdkColor grid_bg = {0, 0, 0, 0 };
	GdkColor grid_fg = {0, 32767, 32767, 32767};
	GdkColor bgcolor = {0, 0, 0, 0 };
	GdkColor tmpcolor;
	GtkWidget *hbox; /* for statusbar */
	GtkWidget *valuefield[3];

	
	gtk_window_set_default_icon_from_file(PACKAGE_DATA_DIR G_DIR_SEPARATOR_S "icons" G_DIR_SEPARATOR_S PACKAGE ".png", NULL);
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

	valuefield[R] = gtk_label_new(NULL);
	gtk_label_set_width_chars (GTK_LABEL(valuefield[R]), 3);
	gtk_misc_set_alignment (GTK_MISC(valuefield[R]), 1.0, 0.5);
	gdk_color_parse("#ef2929", &tmpcolor); /* Scarlet Red */
	gtk_widget_modify_fg (GTK_WIDGET(valuefield[R]), GTK_STATE_NORMAL, &tmpcolor);

	valuefield[G] = gtk_label_new(NULL);
	gtk_label_set_width_chars (GTK_LABEL(valuefield[G]), 3);
	gtk_misc_set_alignment (GTK_MISC(valuefield[G]), 1.0, 0.5);
	gdk_color_parse("#8ae234", &tmpcolor); /* Chameleon */
	gtk_widget_modify_fg (GTK_WIDGET(valuefield[G]), GTK_STATE_NORMAL, &tmpcolor);

	valuefield[B] = gtk_label_new(NULL);
	gtk_label_set_width_chars (GTK_LABEL(valuefield[B]), 3);
	gtk_misc_set_alignment (GTK_MISC(valuefield[B]), 1.0, 0.5);
	gdk_color_parse("#729fcf", &tmpcolor); /* Sky Blue */
	gtk_widget_modify_fg (GTK_WIDGET(valuefield[B]), GTK_STATE_NORMAL, &tmpcolor);

	hbox = gtk_hbox_new (FALSE, 0);
	gtk_box_set_spacing(GTK_BOX(hbox), 3);
	gtk_box_pack_start (GTK_BOX (hbox), valuefield[R], FALSE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (hbox), valuefield[G], FALSE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (hbox), valuefield[B], FALSE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (hbox), GTK_WIDGET (statusbar), TRUE, TRUE, 0);

	/* Build iconbox */
	rs->iconbox = rs_store_new();
	g_signal_connect((gpointer) rs->iconbox, "thumb-activated", G_CALLBACK(icon_activated), rs);
	g_signal_connect((gpointer) rs->iconbox, "group-activated", G_CALLBACK(group_activated), rs);

	rs->store = RS_STORE(rs->iconbox);
    
	rs_get_core_action_group(rs);

	/* Build toolbox */
	gboolean client_mode;
	rs_conf_get_boolean("client-mode", &client_mode);
    
	rs->tools = tools = rs_toolbox_new();
	g_signal_connect(tools, "snapshot-changed", G_CALLBACK(snapshot_changed), rs);
	rs_toolbox_register_actions(RS_TOOLBOX(tools));

	batchbox = make_batchbox(rs->queue);

	GtkWidget *open_box = gtk_vbox_new(FALSE, 0);
	GtkWidget *library_expander;
	GtkWidget *directory_expander;

	dir_selector_vbox = gtk_vbox_new(FALSE, 0);
	checkbox_recursive = checkbox_from_conf(CONF_LOAD_RECURSIVE ,_("Open Recursive"), DEFAULT_CONF_LOAD_RECURSIVE);
	dir_selector_separator = gtk_hseparator_new();
	dir_selector = rs_dir_selector_new();
	g_signal_connect(G_OBJECT(dir_selector), "directory-activated", G_CALLBACK(directory_activated), rs);
	gtk_box_pack_start (GTK_BOX(dir_selector_vbox), checkbox_recursive, FALSE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX(dir_selector_vbox), dir_selector_separator, FALSE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX(dir_selector_vbox), dir_selector, TRUE, TRUE, 0);

	directory_expander = gui_box(_("Directory"), dir_selector_vbox, "OPEN_DIRECTORY_EXPANDER", TRUE);

	library_vbox = rs_tag_gui_toolbox_new(rs_library_get_singleton(), rs->store);
	library_expander = gui_box(_("Tag Search"), library_vbox, "OPEN_LIBRARY_SEARCH_EXPANDER", TRUE);

	gtk_box_pack_start (GTK_BOX(open_box), library_expander, FALSE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX(open_box), directory_expander, TRUE, TRUE, 0);

	GtkWidget *geolocbox = gtk_vbox_new(FALSE, 0);
	RSGeoDb *geodb = rs_geo_db_get_singleton();
	GtkWidget *map = rs_geo_db_get_widget(geodb);
	gtk_box_pack_start (GTK_BOX(geolocbox), map, TRUE, TRUE, 0);
	
	if (client_mode)
		rs->toolbox = tools;
	else
	{
		rs->toolbox = gtk_notebook_new();
		gtk_notebook_append_page(GTK_NOTEBOOK(rs->toolbox), tools, gtk_label_new(_("Tools")));
		gtk_notebook_append_page(GTK_NOTEBOOK(rs->toolbox), geolocbox, gtk_label_new(_("Map")));
		gtk_notebook_append_page(GTK_NOTEBOOK(rs->toolbox), batchbox, gtk_label_new(_("Batch")));
		gtk_notebook_append_page(GTK_NOTEBOOK(rs->toolbox), open_box, gtk_label_new(_("Open")));
	}

	/* Metadata infobox */
	infobox = gtk_label_new("");
	gtk_misc_set_alignment(GTK_MISC(infobox), 0.0, 0.0);
	gtk_misc_set_padding (GTK_MISC(infobox), 7,3);
	rs_toolbox_add_widget(RS_TOOLBOX(rs->tools), infobox, NULL);

	/* Build menubar */
	menubar = gui_make_menubar(rs);

	/* Preview area */
	rs_conf_set_boolean("fullscreen-preview", FALSE);
	rs->preview = rs_preview_widget_new(tools);
	rs_preview_widget_set_filter(RS_PREVIEW_WIDGET(rs->preview), rs->filter_end, rs->filter_demosaic_cache);

	rs_conf_get_color(CONF_PREBGCOLOR, &bgcolor);
	rs_preview_widget_set_bgcolor(RS_PREVIEW_WIDGET(rs->preview), &bgcolor);
	g_signal_connect(G_OBJECT(rs->preview), "wb-picked", G_CALLBACK(preview_wb_picked), rs);
	g_signal_connect(G_OBJECT(rs->preview), "motion", G_CALLBACK(preview_motion), valuefield);
	g_signal_connect(G_OBJECT(rs->preview), "leave", G_CALLBACK(preview_leave), valuefield);

	/* Split pane below iconbox */
	pane = gtk_hpaned_new ();
	frame_preview_toolbox = gtk_frame_new(NULL);
	gtk_container_add(GTK_CONTAINER(frame_preview_toolbox), rs->preview);
	g_signal_connect_after(G_OBJECT(pane), "notify::position", G_CALLBACK(pane_position), NULL);
	gtk_paned_pack1 (GTK_PANED (pane), frame_preview_toolbox, TRUE, TRUE);
	gtk_paned_pack2 (GTK_PANED (pane), rs->toolbox, FALSE, TRUE);

	/* Vertical packing box */
	vbox = gtk_vbox_new (FALSE, 0);
	gtk_container_add (GTK_CONTAINER (rs->window), vbox);

	gtk_box_pack_start (GTK_BOX (vbox), menubar, FALSE, TRUE, 0);
    
    rs_conf_get_boolean("client-mode", &client_mode);
    if (!client_mode)
        gtk_box_pack_start (GTK_BOX (vbox), rs->iconbox, FALSE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (vbox), pane, TRUE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, TRUE, 0);

	if(gui_status_push(_("Ready"))); /* To put a "bottom" on the status stack, we ignore the return value */

	// arrange rawstudio as the user left it
	gboolean show_iconbox;
	gboolean show_toolbox;
	rs_conf_get_boolean_with_default(CONF_FULLSCREEN, &fullscreen, DEFAULT_CONF_FULLSCREEN);
	if (!show_iconbox)
		rs_core_action_group_activate("Iconbox");
	if (!show_toolbox)
		rs_core_action_group_activate("Toolbox");

	gtk_widget_show_all (rs->window);
	toolbox_width = 240;
	rs_conf_get_integer(CONF_TOOLBOX_WIDTH, &toolbox_width);
	gdk_threads_enter();
	GTK_CATCHUP();
	gdk_threads_leave();

	if (fullscreen)
	{
		rs_core_action_group_activate("Fullscreen");
		/* Retrieve defaults */
		gboolean show_iconbox_default;
		gboolean show_toolbox_default;
		rs_conf_get_boolean_with_default(CONF_SHOW_ICONBOX, &show_iconbox_default, DEFAULT_CONF_SHOW_ICONBOX_FULLSCREEN);
		rs_conf_get_boolean_with_default(CONF_SHOW_TOOLBOX, &show_toolbox_default, DEFAULT_CONF_SHOW_TOOLBOX_FULLSCREEN);
		/* Get actual state */
		rs_conf_get_boolean_with_default(CONF_SHOW_ICONBOX_FULLSCREEN, &show_iconbox, show_iconbox_default);
		rs_conf_get_boolean_with_default(CONF_SHOW_TOOLBOX_FULLSCREEN, &show_toolbox, show_toolbox_default);
		gtk_window_get_size(rawstudio_window, &window_width, NULL);
		gtk_paned_set_position(GTK_PANED(pane), window_width - toolbox_width);
	}
	else
	{
		rs_conf_get_boolean_with_default(CONF_SHOW_ICONBOX, &show_iconbox, DEFAULT_CONF_SHOW_TOOLBOX);
		rs_conf_get_boolean_with_default(CONF_SHOW_TOOLBOX, &show_toolbox, DEFAULT_CONF_SHOW_ICONBOX);
		gtk_window_unfullscreen(GTK_WINDOW(rs->window));
		gtk_window_get_size(rawstudio_window, &window_width, NULL);
		gtk_paned_set_position(GTK_PANED(pane), window_width - toolbox_width);
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
		
		if (!client_mode)
			rs_dir_selector_expand_path(RS_DIR_SELECTOR(dir_selector), path);

		rs_window_set_title(g_path_get_dirname(path));
		g_free(path);
	}
	else
	{
		gchar *lwd, *tags;
		lwd = rs_conf_get_string(CONF_LWD);
		tags = rs_conf_get_string(CONF_LIBRARY_TAG_SEARCH);

		if (tags && !lwd)
		{
			rs_library_set_tag_search(tags);
			g_free(tags);
		}
		else if (lwd)
		{
			rs_window_set_title(lwd);
			rs_open_directory_delayed(rs, lwd);
			rs_dir_selector_expand_path(RS_DIR_SELECTOR(dir_selector), lwd);
			g_free(lwd);
		}
		else
		{
			lwd = g_get_current_dir();
			rs_window_set_title(lwd);
			rs_open_directory_delayed(rs, lwd);
			rs_dir_selector_expand_path(RS_DIR_SELECTOR(dir_selector), lwd);
			g_free(lwd);
		}

		gint last_priority_page = 0;
		if (!rs_conf_get_integer(CONF_LAST_PRIORITY_PAGE, &last_priority_page))
			rs_conf_set_integer(CONF_LAST_PRIORITY_PAGE, 0);
		rs_store_set_current_page(rs->store, last_priority_page);

	}
	/* Construct this to load dcp profiles early */
	rs_window_set_title(_("Rawstudio: Loading Color Profiles"));
	GUI_CATCHUP();
	rs_profile_factory_new_default();

	RSLibrary *library = rs_library_get_singleton();
	if (!rs_library_has_database_connection(library))
	{
		GtkWidget *dialog = gui_dialog_make_from_text(GTK_STOCK_DIALOG_ERROR, "Could not initialize sqlite database:", rs_library_get_init_error_msg(library));
		gtk_dialog_add_button (GTK_DIALOG (dialog), GTK_STOCK_CLOSE, GTK_RESPONSE_ACCEPT);
		gtk_widget_show_all(dialog);
		gtk_dialog_run(GTK_DIALOG(dialog));
		gtk_widget_destroy(dialog);
	}

	gui_set_busy(FALSE);
	gdk_threads_enter();
	gtk_main();
	gdk_threads_leave();
	return(0);
}
