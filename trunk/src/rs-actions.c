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
#include <glib/gstdio.h> /* g_unlink */
#include <gtk/gtk.h>
#include <config.h>
#include "gettext.h"
#include "application.h"
#include "rs-actions.h"
#include "conf_interface.h"
#include "rs-store.h"
#include "rs-photo.h"
#include "filename.h"
#include "gtk-interface.h"
#include "gtk-progress.h"
#include "gtk-helper.h"
#include "rs-external-editor.h"
#include "rs-cache.h"
#include "rs-preview-widget.h"
#include "rs-batch.h"
#include "rs-save-dialog.h"
#include "rs-library.h"
#include "rs-tag-gui.h"
#include "rs-lens-db-editor.h"
#include "rs-camera-db.h"
#include "rs-toolbox.h"
#include "rs-tethered-shooting.h"
#include "rs-enfuse.h"
#include "rs-geo-db.h"

static GtkActionGroup *core_action_group = NULL;
static GStaticMutex rs_actions_spinlock = G_STATIC_MUTEX_INIT;

#define ACTION(Action) void rs_action_##Action(GtkAction *action, RS_BLOB *rs); \
	void rs_action_##Action(GtkAction *action, RS_BLOB *rs)
#define ACTION_CB(Action) G_CALLBACK(rs_action_##Action)

#define TOGGLEACTION(Action) void rs_action_##Action(GtkToggleAction *toggleaction, RS_BLOB *rs); \
	void rs_action_##Action(GtkToggleAction *toggleaction, RS_BLOB *rs)

#define RADIOACTION(Action) void rs_action_##Action(GtkRadioAction *radioaction, GtkRadioAction *current, RS_BLOB *rs); \
	void rs_action_##Action(GtkRadioAction *radioaction, GtkRadioAction *current, RS_BLOB *rs)

static gint copy_dialog_get_mask(void);
static void copy_dialog_set_mask(gint mask);

ACTION(todo)
{
	GString *gs = g_string_new("Action not implemented: ");
	g_string_append(gs, gtk_action_get_name(action));
	g_warning("%s", gs->str);
	gui_status_notify(gs->str);
	g_string_free(gs, TRUE);
}

void 
rs_core_actions_update_menu_items(RS_BLOB *rs)
{
	/* File Menu */
	rs_core_action_group_set_sensivity("QuickExport", RS_IS_PHOTO(rs->photo));
	rs_core_action_group_set_sensivity("ExportAs", RS_IS_PHOTO(rs->photo));
	rs_core_action_group_set_sensivity("ExportToGimp", RS_IS_PHOTO(rs->photo));

	/* Edit Menu */
	rs_core_action_group_set_sensivity("RevertSettings", RS_IS_PHOTO(rs->photo));
	rs_core_action_group_set_sensivity("CopySettings", RS_IS_PHOTO(rs->photo));
	rs_core_action_group_set_sensivity("CopyImage", RS_IS_PHOTO(rs->photo));
	rs_core_action_group_set_sensivity("PasteSettings", !!(rs->settings_buffer));
	rs_core_action_group_set_sensivity("SaveDefaultSettings", RS_IS_PHOTO(rs->photo));

	/* Photo Menu */
	GList *selected = NULL, *selected_iters = NULL;
	gint num_selected, selected_groups;
	gboolean photos_selected;

	selected = rs_store_get_selected_names(rs->store);
	num_selected = g_list_length(selected);

	photos_selected = (RS_IS_PHOTO(rs->photo) || (num_selected > 0));

	selected_iters = rs_store_get_selected_iters(rs->store);
	selected_groups = rs_store_selection_n_groups(rs->store, selected_iters);

	rs_core_action_group_set_sensivity("FlagPhoto", photos_selected);
	rs_core_action_group_set_sensivity("PriorityMenu", photos_selected);
	rs_core_action_group_set_sensivity("WhiteBalanceMenu", RS_IS_PHOTO(rs->photo));
	rs_core_action_group_set_sensivity("Crop", RS_IS_PHOTO(rs->photo));
	rs_core_action_group_set_sensivity("Uncrop", (RS_IS_PHOTO(rs->photo) && rs->photo->crop));
	rs_core_action_group_set_sensivity("Straighten", RS_IS_PHOTO(rs->photo));
	rs_core_action_group_set_sensivity("Unstraighten", (RS_IS_PHOTO(rs->photo) && (rs->photo->angle != 0.0)));
	rs_core_action_group_set_sensivity("TagPhoto", RS_IS_PHOTO(rs->photo));
	rs_core_action_group_set_sensivity("Group", (num_selected > 1));
	rs_core_action_group_set_sensivity("Ungroup", (selected_groups > 0));
	rs_core_action_group_set_sensivity("RotateClockwise", RS_IS_PHOTO(rs->photo));
	rs_core_action_group_set_sensivity("RotateCounterClockwise", RS_IS_PHOTO(rs->photo));
	rs_core_action_group_set_sensivity("Flip", RS_IS_PHOTO(rs->photo));
	rs_core_action_group_set_sensivity("Mirror", RS_IS_PHOTO(rs->photo));
	rs_core_action_group_set_sensivity("Enfuse", rs_has_enfuse(4,0) && num_selected >= 1);
#ifndef EXPERIMENTAL
	rs_core_action_group_set_visibility("Group", FALSE);
	rs_core_action_group_set_visibility("Ungroup", FALSE);
	rs_core_action_group_set_visibility("AutoGroup", FALSE);
#endif

	/* View Menu */
	rs_core_action_group_set_sensivity("Lightsout", !rs->window_fullscreen);

	/* Batch Menu */
	rs_core_action_group_set_sensivity("AddToBatch", (num_selected >= 1) || (rs->photo && !rs_batch_exists_in_queue(rs->queue, rs->photo->filename, rs->current_setting)));
	rs_core_action_group_set_sensivity("RemoveFromBatch", (num_selected >= 1) || (rs->photo && rs_batch_exists_in_queue(rs->queue, rs->photo->filename, rs->current_setting)));
	rs_core_action_group_set_sensivity("ProcessBatch", (rs_batch_num_entries(rs->queue)>0));

	/* Preview */
	rs_core_action_group_set_sensivity("Crop", RS_IS_PHOTO(rs->photo));
	rs_core_action_group_set_sensivity("Uncrop", (RS_IS_PHOTO(rs->photo) && rs->photo->crop));
	rs_core_action_group_set_sensivity("Straighten", RS_IS_PHOTO(rs->photo));
	rs_core_action_group_set_sensivity("Unstraighten", (RS_IS_PHOTO(rs->photo) && (rs->photo->angle != 0.0)));

	g_list_free(selected);

}

ACTION(file_menu)
{
	rs_core_actions_update_menu_items(rs);
}

ACTION(edit_menu)
{
	rs_core_actions_update_menu_items(rs);
}


ACTION(photo_menu)
{
	rs_core_actions_update_menu_items(rs);
}

ACTION(view_menu)
{
	rs_core_actions_update_menu_items(rs);
}

ACTION(batch_menu)
{
	rs_core_actions_update_menu_items(rs);
}

ACTION(preview_popup)
{
	rs_core_actions_update_menu_items(rs);
}

ACTION(open)
{
	GtkWidget *fc;
	gchar *lwd = rs_conf_get_string(CONF_LWD);

	fc = gtk_file_chooser_dialog_new (_("Open directory"), NULL,
		GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
		GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
		GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT, NULL);
	gtk_dialog_set_default_response(GTK_DIALOG(fc), GTK_RESPONSE_ACCEPT);
	
	if (g_file_test(lwd, G_FILE_TEST_IS_DIR))
		gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER (fc), lwd);

	if (gtk_dialog_run (GTK_DIALOG (fc)) == GTK_RESPONSE_ACCEPT)
	{
		char *filename;

		filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (fc));
		gtk_widget_destroy (fc);
		rs_store_remove(rs->store, NULL, NULL);
		if (rs_store_load_directory(rs->store, filename) >= 0)
			rs_conf_set_string(CONF_LWD, filename);
		g_free (filename);
	} else
		gtk_widget_destroy (fc);

	g_free(lwd);
	rs_core_actions_update_menu_items(rs);
}

ACTION(quick_export)
{
	gchar *directory;
	gchar *filename_template;
	gchar *parsed_filename = NULL;
	gchar *parsed_dir = NULL;
	gchar *output_identifier;

	directory = rs_conf_get_string("quick-export-directory");
	filename_template = rs_conf_get_string("quick-export-filename");
	output_identifier = rs_conf_get_string("quick-export-filetype");

	/* Initialize directory to home dir if nothing is saved in config */
	if (!directory)
	{
		const char *homedir = g_getenv("HOME");
		if (!homedir)
			homedir = g_get_home_dir();
		directory = g_strdup(homedir);
	}

	/* Initialize filename_template to default if nothing is saved in config */
	if (!filename_template)
		filename_template = g_strdup(DEFAULT_CONF_EXPORT_FILENAME);

	/* Output as Jpeg, if nothing is saved in config */
	if (!output_identifier)
		output_identifier = g_strdup("RSJpeg");

	RSOutput *output = rs_output_new(output_identifier);

	GString *filename = NULL;
	/* Build new filename */
	if (NULL == g_strrstr(filename_template, "%p"))
	{
		filename = g_string_new(directory);
		g_string_append(filename, G_DIR_SEPARATOR_S);
		g_string_append(filename, filename_template);
	} 
	else
		filename = g_string_new(filename_template);
	
	g_string_append(filename, ".");
	g_string_append(filename, rs_output_get_extension(output));

	parsed_filename = filename_parse(filename->str, rs->photo->filename, rs->current_setting, TRUE);

	if (parsed_filename && output)
	{
		/* Create directory, if it doesn't exist */
		parsed_dir = g_path_get_dirname(parsed_filename);
		if (FALSE == g_file_test(parsed_dir, G_FILE_TEST_EXISTS | G_FILE_TEST_IS_DIR))
			if (g_mkdir_with_parents(parsed_dir, 0x1ff))
			{
				gui_status_error(_("Could not create output directory."));
				return;
			}

		guint msg = gui_status_push(_("Exporting..."));
		gui_set_busy(TRUE);
		GTK_CATCHUP();
		g_object_set(output, "filename", parsed_filename, NULL);
		rs_output_set_from_conf(output, "quick-export");

		if (rs_photo_save(rs->photo, rs->filter_end, output, -1, -1, FALSE, 1.0, rs->current_setting))
		{
			gchar *status = g_strdup_printf("%s (%s)", _("File exported"), parsed_filename);
			gui_status_notify(status);
			g_free(status);
		}
		else
			gui_status_error(_("Export failed"));

		gui_status_pop(msg);
		gui_set_busy(FALSE);
	}

	g_object_unref(output);
	g_free(directory);
	g_free(parsed_filename);
	g_free(parsed_dir);
	g_free(output_identifier);
	g_string_free(filename, TRUE);
}

ACTION(export_as)
{
	if (RS_IS_PHOTO(rs->photo))
	{
		RSSaveDialog *dialog = rs_save_dialog_new();
		rs_save_dialog_set_photo(dialog, rs->photo, rs->current_setting);
		gtk_widget_show(GTK_WIDGET(dialog));
	}
	else
		gui_status_error(_("Export failed"));
}

ACTION(export_to_gimp)
{
	if (!RS_IS_PHOTO(rs->photo)) return;

	guint status = gui_status_push(_("Exporting to GIMP..."));
	GTK_CATCHUP();
	if (!rs_external_editor_gimp(rs->photo, rs->filter_end, rs->current_setting))
	{
		GtkWidget *dialog = gui_dialog_make_from_text(GTK_STOCK_DIALOG_WARNING, 
			_("Error exporting"),
			_("Error exporting photo to gimp."));
		gtk_dialog_add_button (GTK_DIALOG (dialog), GTK_STOCK_CLOSE, GTK_RESPONSE_ACCEPT);
		gtk_widget_show_all(dialog);
		gtk_dialog_run(GTK_DIALOG(dialog));
		gtk_widget_destroy(dialog);
	}
	gui_status_pop(status);
}

ACTION(reload)
{
	rs_store_remove(rs->store, NULL, NULL);
	rs_store_load_directory(rs->store, NULL);
	rs_core_actions_update_menu_items(rs);
}

ACTION(delete_flagged)
{
	gchar *cache;
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

	progress = gui_progress_new(_("Deleting photos"), items);

	for (i=0;i<items;i++)
	{
		gchar *fullname = rs_store_get_name(rs->store, g_list_nth_data(photos_d, i));

		if(0 == g_unlink(fullname))
		{
			rs_metadata_delete_cache(fullname);
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
}

ACTION(quit)
{
	if (rs->photo)
		rs_photo_close(rs->photo);
	rs_conf_set_integer(CONF_LAST_PRIORITY_PAGE, rs_store_get_current_page(rs->store));
	rs_io_idle_cancel_class(METADATA_CLASS);
	rs_io_idle_cancel_class(PRELOAD_CLASS);
	rs_io_idle_cancel_class(RESTORE_TAGS_CLASS);
	
	RS_PROGRESS *progress;
	gint total_items = rs_io_get_jobs_left();
	if (total_items)
	{
		progress = gui_progress_new(_("Waiting for jobs to finish"), total_items);
		gtk_widget_hide(rs->window);
		gint items = total_items;
		while (items > 0)
		{
			g_usleep(100*1000);
			items = rs_io_get_jobs_left();
			gui_progress_set_current(progress, total_items-items);
			GUI_CATCHUP();
		}
	}
	gtk_main_quit();
}

ACTION(client_mode_quit)
{

	gchar *directory;
    
	RSOutput *output = rs_output_new("RSJpegfile");
	gchar *filename = rs_conf_get_string("client-mode-destination");

	if (filename)
	{
		/* Create directory, if it doesn't exist */
		directory = g_path_get_dirname(filename);
        
		if (FALSE == g_file_test(directory, G_FILE_TEST_EXISTS | G_FILE_TEST_IS_DIR))
			if (g_mkdir_with_parents(directory, 0x1ff))
			{
				gui_status_notify(_("Could not create output directory."));
				return;
			}
		guint msg = gui_status_push(_("Auto exporting..."));
		gui_set_busy(TRUE);
		GTK_CATCHUP();
		g_object_set(output, "filename", filename, NULL);
        
		if (rs_photo_save(rs->photo, rs->filter_end, output, -1, -1, FALSE, 1.0, rs->current_setting))
		{
			gchar *status = g_strdup_printf("%s (%s)", _("File exported"), filename);
			gui_status_notify(status);
			g_free(status);
		}
		else
			gui_status_notify(_("Export failed"));
		gui_status_pop(msg);
		gui_set_busy(FALSE);
		g_free(directory);
	}

	g_object_unref(output);
	rs_action_quit(action, rs);
}

ACTION(copy_image)
{
	if (!rs->photo)
		return;
	guint msg = gui_status_push(_("Copying image to clipboard"));
	gui_set_busy(TRUE);
	GTK_CATCHUP();

	if (rs_photo_copy_to_clipboard(rs->photo, rs->filter_end, -1, -1, FALSE, 1.0, rs->current_setting))
		gui_status_notify(_("Image copied to clipboard"));
	else
		gui_status_notify(_("ERROR: Could not copy image to clipboard"));
	gui_set_busy(FALSE);
	gui_status_pop(msg);
}

ACTION(revert_settings)
{
	if (RS_IS_PHOTO(rs->photo))
		rs_cache_load(rs->photo);
}

static const gint COPY_MASK_ALL = MASK_PROFILE|MASK_EXPOSURE|MASK_SATURATION|MASK_HUE|
	MASK_CONTRAST|MASK_WB|MASK_SHARPEN|MASK_DENOISE_LUMA|MASK_DENOISE_CHROMA|
	MASK_CHANNELMIXER|MASK_TCA|MASK_VIGNETTING|MASK_CURVE|MASK_TIME_OFFSET;

/* Widgets for copy dialog */
static GtkWidget *cb_profile, *cb_exposure, *cb_saturation, *cb_hue, *cb_contrast, *cb_whitebalance, *cb_curve, *cb_sharpen, *cb_denoise_luma, *cb_denoise_chroma, *cb_channelmixer, *cb_tca, *cb_vignetting,*cb_transform, *cb_time_offset, *b_all_none;

static void
all_none_clicked(GtkButton *button, gpointer user_data)
{
	gint mask= copy_dialog_get_mask();
	if (mask == COPY_MASK_ALL)
		mask = 0;
	else
		mask = COPY_MASK_ALL;

	copy_dialog_set_mask(mask);
}

static GtkWidget *
create_copy_dialog(gint mask)
{
	GtkWidget *cb_box;
	GtkWidget *dialog;
	/* Build GUI */
	cb_profile = gtk_check_button_new_with_label (_("Profile"));
	cb_exposure = gtk_check_button_new_with_label (_("Exposure"));
	cb_saturation = gtk_check_button_new_with_label (_("Saturation"));
	cb_hue = gtk_check_button_new_with_label (_("Hue"));
	cb_contrast = gtk_check_button_new_with_label (_("Contrast"));
	cb_whitebalance = gtk_check_button_new_with_label (_("White Balance"));
	cb_sharpen = gtk_check_button_new_with_label (_("Sharpen"));
	cb_denoise_luma = gtk_check_button_new_with_label (_("Denoise"));
	cb_denoise_chroma = gtk_check_button_new_with_label (_("Color Denoise"));
	cb_channelmixer = gtk_check_button_new_with_label (_("Channel Mixer"));
	cb_tca = gtk_check_button_new_with_label (_("TCA"));
	cb_vignetting = gtk_check_button_new_with_label (_("Vignetting"));
	cb_curve = gtk_check_button_new_with_label (_("Curve"));
	cb_transform = gtk_check_button_new_with_label (_("Transform"));
	cb_time_offset = gtk_check_button_new_with_label (_("Time offset (GPS)"));
	b_all_none = gtk_button_new_with_label (_("Select All/None"));

	g_signal_connect(b_all_none, "clicked", G_CALLBACK(all_none_clicked), NULL);

	copy_dialog_set_mask(mask);
	cb_box = gtk_vbox_new(FALSE, 0);

	gtk_box_pack_start (GTK_BOX (cb_box), cb_profile, FALSE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (cb_box), cb_exposure, FALSE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (cb_box), cb_saturation, FALSE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (cb_box), cb_hue, FALSE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (cb_box), cb_contrast, FALSE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (cb_box), cb_whitebalance, FALSE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (cb_box), cb_sharpen, FALSE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (cb_box), cb_denoise_luma, FALSE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (cb_box), cb_denoise_chroma, FALSE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (cb_box), cb_channelmixer, FALSE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (cb_box), cb_tca, FALSE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (cb_box), cb_vignetting, FALSE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (cb_box), cb_curve, FALSE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (cb_box), cb_transform, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (cb_box), cb_time_offset, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (cb_box), b_all_none, FALSE, TRUE, 0);

	dialog = gui_dialog_make_from_widget(GTK_STOCK_DIALOG_QUESTION, _("Select Settings to Copy"), cb_box);

	gtk_dialog_add_buttons(GTK_DIALOG(dialog), GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL, GTK_STOCK_OK, GTK_RESPONSE_OK, NULL);
	gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_OK);
	return dialog;
}

static void
copy_dialog_set_mask(gint mask)
{
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(cb_profile), !!(mask & MASK_PROFILE));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(cb_exposure), !!(mask & MASK_EXPOSURE));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(cb_saturation), !!(mask & MASK_SATURATION));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(cb_hue), !!(mask & MASK_HUE));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(cb_contrast), !!(mask & MASK_CONTRAST));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(cb_whitebalance), !!(mask & MASK_WB));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(cb_sharpen), !!(mask & MASK_SHARPEN));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(cb_denoise_luma), !!(mask & MASK_DENOISE_LUMA));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(cb_denoise_chroma), !!(mask & MASK_DENOISE_CHROMA));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(cb_channelmixer), !!(mask & MASK_CHANNELMIXER));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(cb_tca), !!(mask & MASK_TCA));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(cb_vignetting), !!(mask & MASK_VIGNETTING));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(cb_curve), !!(mask & MASK_CURVE));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(cb_transform), !!(mask & MASK_TRANSFORM));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(cb_time_offset), !!(mask & MASK_TIME_OFFSET));
}

static gint
copy_dialog_get_mask(void)
{
	gint mask = 0;
	if (GTK_TOGGLE_BUTTON(cb_profile)->active)
		mask |= MASK_PROFILE;
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
	if (GTK_TOGGLE_BUTTON(cb_sharpen)->active)
		mask |= MASK_SHARPEN;
	if (GTK_TOGGLE_BUTTON(cb_denoise_luma)->active)
		mask |= MASK_DENOISE_LUMA;
	if (GTK_TOGGLE_BUTTON(cb_denoise_chroma)->active)
		mask |= MASK_DENOISE_CHROMA;
	if (GTK_TOGGLE_BUTTON(cb_channelmixer)->active)
		mask |= MASK_CHANNELMIXER;
	if (GTK_TOGGLE_BUTTON(cb_tca)->active)
		mask |= MASK_TCA;
	if (GTK_TOGGLE_BUTTON(cb_vignetting)->active)
		mask |= MASK_VIGNETTING;
	if (GTK_TOGGLE_BUTTON(cb_curve)->active)
		mask |= MASK_CURVE;
	if (GTK_TOGGLE_BUTTON(cb_transform)->active)
		mask |= MASK_TRANSFORM;
	if (GTK_TOGGLE_BUTTON(cb_time_offset)->active)
		mask |= MASK_TIME_OFFSET;
	return mask;
}

ACTION(copy_settings)
{
	gint mask = COPY_MASK_ALL; /* Should be RSSettingsMask, is gint to satisfy rs_conf_get_integer() */
	GtkWidget *dialog;

	if (!rs->settings_buffer)
		rs->settings_buffer = rs_settings_new();
	if (!rs->photo)
		return;

	rs_conf_get_integer(CONF_PASTE_MASK, &mask);
	dialog = create_copy_dialog(mask);
	gtk_window_set_transient_for(GTK_WINDOW(dialog), GTK_WINDOW(rs->window));
	gtk_widget_show_all(dialog);

	if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_OK)
	{
		mask = copy_dialog_get_mask();
		rs_conf_set_integer(CONF_PASTE_MASK, mask);
		rs_settings_copy(rs->photo->settings[rs->current_setting], MASK_ALL, rs->settings_buffer);
		rs->dcp_buffer = rs_photo_get_dcp_profile(rs->photo);
		rs->icc_buffer = rs_photo_get_icc_profile(rs->photo);
		RS_RECT *c = rs_photo_get_crop(rs->photo);
		if (c)
		{
			rs->crop_buffer.x1 = c->x1;rs->crop_buffer.x2 = c->x2;
			rs->crop_buffer.y1 = c->y1;rs->crop_buffer.y2 = c->y2;
		}
		else
			rs->crop_buffer.x1 = -1;
		rs->angle_buffer = rs->photo->angle;
		rs->orientation_buffer = rs->photo->orientation;
		rs->time_offset_buffer = rs->photo->time_offset;

		gui_status_notify(_("Copied settings"));
	}
	gtk_widget_destroy (dialog);
	rs_core_actions_update_menu_items(rs);
}

ACTION(paste_settings)
{
	gint mask = COPY_MASK_ALL; /* Should be RSSettingsMask, is gint to satisfy rs_conf_get_integer() */

	gui_set_busy(TRUE);
	guint msg = gui_status_push(_("Pasting settings to images"));
	GTK_CATCHUP();
	if (rs->settings_buffer)
	{
		rs_conf_get_integer(CONF_PASTE_MASK, &mask);
		if(mask != 0)
		{
			RSMetadata *metadata;
			RS_PHOTO *photo;
			gint cur;
			GList *selected = NULL;
			gint num_selected;
			guint new_mask;

			/* Apply to all selected photos */
			selected = rs_store_get_selected_names(rs->store);
			num_selected = g_list_length(selected);
			for(cur=0;cur<num_selected;cur++)
			{
				/* This is nothing but a hack around rs_cache_*() */
				photo = rs_photo_new();
				photo->filename = g_strdup(g_list_nth_data(selected, cur));

				if (mask & MASK_TRANSFORM)
				{
					photo->orientation = rs->orientation_buffer;
					rs_photo_set_angle(photo, rs->angle_buffer, FALSE);
					if (rs->crop_buffer.x1 == -1)
						rs_photo_set_crop(photo, NULL);
					else
						rs_photo_set_crop(photo, &rs->crop_buffer);
				} else
				{
					/* Make sure we rotate this right */
					metadata = rs_metadata_new_from_file(photo->filename);
					switch (metadata->orientation)
					{
						case 90: ORIENTATION_90(photo->orientation);
							break;
						case 180: ORIENTATION_180(photo->orientation);
							break;
						case 270: ORIENTATION_270(photo->orientation);
							break;
					}
					g_object_unref(metadata);
				}
				
				new_mask = rs_cache_load(photo);
				rs_settings_copy(rs->settings_buffer, mask, photo->settings[rs->current_setting]);
				if (mask & MASK_PROFILE)
				{
					if (rs->dcp_buffer)
						rs_photo_set_dcp_profile(photo, rs->dcp_buffer);
					else if (rs->icc_buffer)
						rs_photo_set_icc_profile(photo, rs->icc_buffer);
				}
				if (mask & MASK_TIME_OFFSET)
				{
					RSGeoDb *geodb = rs_geo_db_get_singleton();
					rs_geo_db_set_offset(geodb, photo, rs->time_offset_buffer);
				}	
				rs_cache_save(photo, (new_mask | mask) & MASK_ALL);
				g_object_unref(photo);
			}
			g_list_free(selected);

			/* Apply to current photo */
			if (rs->photo)
			{
				if (mask & MASK_PROFILE)
				{
					if (rs->dcp_buffer)
						rs_photo_set_dcp_profile(rs->photo, rs->dcp_buffer);
					else if (rs->icc_buffer)
						rs_photo_set_icc_profile(rs->photo, rs->icc_buffer);
				}
				if (mask & MASK_TRANSFORM)
				{
					rs->photo->orientation = rs->orientation_buffer;
					rs_photo_set_angle(rs->photo, rs->angle_buffer, FALSE);
					rs_photo_set_crop(rs->photo, &rs->crop_buffer);
				}
				if (mask & MASK_TIME_OFFSET)
				{
					RSGeoDb *geodb = rs_geo_db_get_singleton();
					rs_geo_db_set_offset(geodb, photo, rs->time_offset_buffer);
				}
			}

			if (rs->photo)
				rs_settings_copy(rs->settings_buffer, mask, rs->photo->settings[rs->current_setting]); 

			/* Update WB if wb_ascii is set */
			if (mask & MASK_WB && rs->photo->settings[rs->current_setting]->wb_ascii)
			{
				if (g_strcmp0(rs->photo->settings[rs->current_setting]->wb_ascii, PRESET_WB_AUTO) == 0)
					rs_photo_set_wb_auto(rs->photo, rs->current_setting);
				else if (g_strcmp0(rs->photo->settings[rs->current_setting]->wb_ascii, PRESET_WB_CAMERA) == 0)
					rs_photo_set_wb_from_camera(rs->photo, rs->current_setting);
			}
			gui_status_notify(_("Pasted settings"));
		}
		else
			gui_status_notify(_("Nothing to paste"));
	}
	else 
		gui_status_notify(_("Buffer empty"));
	gui_status_pop(msg);
	GTK_CATCHUP();
	gui_set_busy(FALSE);
}

ACTION(reset_settings)
{
	if (RS_IS_PHOTO(rs->photo))
	{
		rs_settings_commit_start(rs->photo->settings[rs->current_setting]);
		RSCameraDb *db = rs_camera_db_get_singleton();
		gpointer p;
		RSSettings *s[3];
		gint snapshot = rs->current_setting;

		if (rs_camera_db_photo_get_defaults(db, rs->photo, s, &p) && s[snapshot] && RS_IS_SETTINGS(s[snapshot]))
		{
			rs_settings_copy(s[snapshot], MASK_ALL, rs->photo->settings[snapshot]);
			if (rs->photo->dcp && p && RS_IS_DCP_FILE(p))
				rs_photo_set_dcp_profile(rs->photo, p);
		}
		else
			rs_settings_reset(rs->photo->settings[snapshot], MASK_ALL);

		rs_photo_set_wb_from_camera(rs->photo, snapshot);
		rs_settings_commit_stop(rs->photo->settings[snapshot]);
	}
}

ACTION(save_default_settings)
{
	if (RS_IS_PHOTO(rs->photo) && rs->photo->metadata)
	{
		rs_camera_db_save_defaults(rs_camera_db_get_singleton(), rs->photo);
		GString *gs = g_string_new("");
		g_string_printf(gs, _("Settings saved as default for %s %s"), rs->photo->metadata->make_ascii, rs->photo->metadata->model_ascii);
		gui_status_notify(gs->str);
		g_string_free(gs, TRUE);
	}
}

ACTION(preferences)
{
	gui_make_preference_window(rs);
}

ACTION(flag_for_deletion)
{
	gui_setprio(rs, PRIO_D);
}

ACTION(priority_1)
{
	gui_setprio(rs, PRIO_1);
}

ACTION(priority_2)
{
	gui_setprio(rs, PRIO_2);
}

ACTION(priority_3)
{
	gui_setprio(rs, PRIO_3);
}

ACTION(priority_0)
{
	gui_setprio(rs, PRIO_U);
}

void
set_wb_on_multiple_photos(GList *selected, gint current_setting, const gchar *wb_ascii)
{
	gint num_selected = g_list_length(selected);
	gui_set_busy(TRUE);
	GTK_CATCHUP();

	if (num_selected > 1)
	{
		RS_PHOTO *photo;
		gint cur, load_mask;

		for(cur=0;cur<num_selected;cur++)
		{
			/* This is nothing but a hack around rs_cache_*() */
			photo = rs_photo_new();
			photo->filename = g_strdup(g_list_nth_data(selected, cur));
			if (rs_metadata_load(photo->metadata, photo->filename))
			{
				/* Rotate photo inplace */
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
			/* Load defaults */
			rs_camera_db_photo_set_defaults(rs_camera_db_get_singleton(), photo);

			load_mask = rs_cache_load(photo);
			rs_settings_set_wb(photo->settings[current_setting], 0.0, 0.0, wb_ascii);
			rs_cache_save(photo, load_mask | MASK_WB);
			g_object_unref(photo);
		}
	}
	g_list_free(selected);
	gui_set_busy(FALSE);
}

ACTION(auto_wb)
{
	if (RS_IS_PHOTO(rs->photo) && rs_store_is_photo_selected(rs->store, rs->photo->filename))
	{
		gui_status_notify(_("Adjusting to auto white balance"));
		rs_photo_set_wb_auto(rs->photo, rs->current_setting);
	}

	/* Apply to all selected photos */
	set_wb_on_multiple_photos(rs_store_get_selected_names(rs->store), rs->current_setting, PRESET_WB_AUTO);
}

ACTION(camera_wb)
{
	if (RS_IS_PHOTO(rs->photo) && rs_store_is_photo_selected(rs->store, rs->photo->filename))
	{
		if (rs->photo && !rs->photo->dcp)
			rs_photo_set_wb_from_camera(rs->photo, rs->current_setting);
		else if (rs->photo && rs->photo->metadata && rs->photo->metadata->cam_mul[R] == -1.0)
			gui_status_notify(_("No white balance to set from"));
		else
		{
			gui_status_notify(_("Adjusting to camera white balance"));
			rs_photo_set_wb_from_camera(rs->photo, rs->current_setting);
		}
	}

	/* Apply to all selected photos */
	set_wb_on_multiple_photos(rs_store_get_selected_names(rs->store), rs->current_setting, PRESET_WB_CAMERA);
}

ACTION(crop)
{
	rs_preview_widget_crop_start(RS_PREVIEW_WIDGET(rs->preview));
	rs_core_actions_update_menu_items(rs);
}

ACTION(uncrop)
{
	rs_preview_widget_uncrop(RS_PREVIEW_WIDGET(rs->preview));
	rs_core_actions_update_menu_items(rs);
}

ACTION(straighten)
{
	rs_preview_widget_straighten(RS_PREVIEW_WIDGET(rs->preview));
}

ACTION(unstraighten)
{
	rs_preview_widget_unstraighten(RS_PREVIEW_WIDGET(rs->preview));
}

ACTION(rotate_clockwise)
{
	if (rs->photo)
		rs_photo_rotate(rs->photo, 1, 0.0);
}

ACTION(rotate_counter_clockwise)
{
	if (rs->photo)
		rs_photo_rotate(rs->photo, 3, 0.0);
}

ACTION(flip)
{
	if (rs->photo)
		rs_photo_flip(rs->photo);
}

ACTION(mirror)
{
	if (rs->photo)
		rs_photo_mirror(rs->photo);
}

ACTION(group_photos)
{
	rs_store_group_photos(rs->store);
}

ACTION(ungroup_photos)
{
	rs_store_ungroup_photos(rs->store);
}

ACTION(auto_group_photos)
{
	rs_store_auto_group(rs->store);
}

typedef struct TagPhotoData {
	RS_BLOB *rs;
	GtkEntry *entry;
} TagPhotoData;

static void 
tag_photo_input_changed(GtkWidget *button, gpointer user_data)
{
	GHashTableIter iter;
	gpointer key, value;
	RSLibrary *library = rs_library_get_singleton();
	TagPhotoData *info = (TagPhotoData*)(user_data);
	RS_BLOB *rs = info->rs;
	GtkEntry *entry = info->entry;

	GList * selected = rs_store_get_selected_names(rs->store);
	gint num_selected = g_list_length(selected);
	gint cur, i;
	GHashTable* directories = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);

	const char* entry_text = gtk_entry_get_text(entry);
	if (num_selected > 0 && entry_text)
	{
		GList *tags = rs_split_string(entry_text, " .,/;:~^*|&");
		for(i = 0; i < g_list_length(tags); i++)
		{
			gchar *tag = (gchar *) g_list_nth_data(tags, i);
			gint tag_id = rs_library_add_tag(library, tag);
			g_free(tag);

			for(cur=0;cur<num_selected;cur++)
			{
				gchar* filename = g_list_nth_data(selected, cur);
				rs_io_idle_add_tag(filename, tag_id, FALSE, -1);
				
				if (0 == i)
				{
					gchar* dir = g_path_get_dirname(filename);
					if (NULL == g_hash_table_lookup(directories, dir))
						g_hash_table_insert(directories, dir, filename);
					else
						g_free(dir);
				}
			}
		}

		g_hash_table_iter_init (&iter, directories);
		while (g_hash_table_iter_next (&iter, &key, &value)) 
		{
			rs_io_idle_add_tag(value, -2, FALSE, -1);
		}
		g_hash_table_remove_all(directories);
		g_list_free(tags);
	}
	GdkWindow *window = gtk_widget_get_parent_window(GTK_WIDGET(entry));
	gdk_window_destroy(window);
	g_list_free(selected);
	g_free(info);
}

ACTION(tag_photo)
{
	GtkWidget *popup = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_transient_for(GTK_WINDOW(popup), GTK_WINDOW(rs->window));
	gtk_window_set_type_hint(GTK_WINDOW(popup), GDK_WINDOW_TYPE_HINT_UTILITY);
	GtkWidget *label = gtk_label_new(_("Tags, separated by spaces:"));
	GtkWidget *box = gtk_hbox_new(FALSE, 10);
	GtkWidget *entry = rs_library_tag_entry_new(rs_library_get_singleton());
	GtkWidget *apply_button = gtk_button_new_from_stock(GTK_STOCK_APPLY);

	gtk_window_set_title(GTK_WINDOW(popup), _("Add Tags to Selected Photo(s)."));
	gtk_window_set_position(GTK_WINDOW(popup), GTK_WIN_POS_MOUSE);
	gtk_box_pack_start(GTK_BOX(box), label, FALSE, TRUE, 5);
	gtk_box_pack_start(GTK_BOX(box), entry, FALSE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(box), apply_button, FALSE, FALSE, 5);
	gtk_container_set_border_width(GTK_CONTAINER(box), 10);

	gtk_container_add(GTK_CONTAINER(popup), box);
	gtk_widget_show_all(popup);

	TagPhotoData *info = g_malloc(sizeof(TagPhotoData));
	info->rs = rs;
	info->entry = GTK_ENTRY(entry);

	g_signal_connect(entry, "activate", G_CALLBACK(tag_photo_input_changed), info);
	g_signal_connect(apply_button, "clicked", G_CALLBACK(tag_photo_input_changed), info);
}

ACTION(previous_photo)
{
	gchar *current_filename = NULL;

	/* Get current filename if a photo is loaded */
	if (RS_IS_PHOTO(rs->photo))
		current_filename = rs->photo->filename;

	rs_store_select_prevnext(rs->store, current_filename, 1);
}

ACTION(next_photo)
{
	gchar *current_filename = NULL;

	/* Get current filename if a photo is loaded */
	if (RS_IS_PHOTO(rs->photo))
		current_filename = rs->photo->filename;

	rs_store_select_prevnext(rs->store, current_filename, 2);
}

gboolean slideshow_play(RS_BLOB *rs)
{
	gchar *current_filename = NULL;
	
	if (rs->slideshow_running == FALSE)
		return FALSE;

	/* Get current filename if a photo is loaded */
	if (RS_IS_PHOTO(rs->photo))
		current_filename = rs->photo->filename;

	rs_store_select_prevnext(rs->store, current_filename, 2);

	if (rs_store_get_prevnext(rs->store, current_filename, 2))
	{
		/* FIXME: there must be a better way */
		gint retval = system("xset s reset");
		if (retval);

		return TRUE;
	}
	else
	  {
		rs->slideshow_running = FALSE;
		gui_status_notify(_("Slideshow stopped"));
		return FALSE;
	  }
}

ACTION(play)
{
	if (rs->slideshow_running == FALSE)
		rs->slideshow_running = TRUE;
	else
		rs->slideshow_running = FALSE;

	if (rs->slideshow_running)
	{
		g_timeout_add(5000, (GSourceFunc) slideshow_play, rs);
		gui_status_notify(_("Slideshow running"));
	}
	else
		gui_status_notify(_("Slideshow stopped"));

}

TOGGLEACTION(zoom_to_fit)
{
	rs_preview_widget_set_zoom_to_fit(RS_PREVIEW_WIDGET(rs->preview), gtk_toggle_action_get_active(toggleaction));
}

TOGGLEACTION(iconbox)
{  
	gboolean client_mode;
	rs_conf_get_boolean("client-mode", &client_mode);
	if (!client_mode)
		gui_widget_show(rs->iconbox, gtk_toggle_action_get_active(toggleaction), CONF_SHOW_ICONBOX_FULLSCREEN, CONF_SHOW_ICONBOX);
}

TOGGLEACTION(toolbox)
{
	gui_widget_show(rs->toolbox, gtk_toggle_action_get_active(toggleaction), CONF_SHOW_TOOLBOX_FULLSCREEN, CONF_SHOW_TOOLBOX);
}

TOGGLEACTION(show_filenames)
{
	rs_store_set_show_filenames(rs->store, gtk_toggle_action_get_active(toggleaction));
	rs_conf_set_boolean(CONF_SHOW_FILENAMES, gtk_toggle_action_get_active(toggleaction));
}

TOGGLEACTION(load_8bit)
{
	rs_conf_set_boolean(CONF_LOAD_GDK, gtk_toggle_action_get_active(toggleaction));
}

TOGGLEACTION(fullscreen)
{
	if (gtk_toggle_action_get_active(toggleaction))
	{
		rs->window_fullscreen = TRUE;
		gtk_window_fullscreen(GTK_WINDOW(rs->window));
	}
	else
	{
		rs->window_fullscreen = FALSE;
		gtk_window_unfullscreen(GTK_WINDOW(rs->window));
	}
	rs_conf_set_boolean(CONF_FULLSCREEN, rs->window_fullscreen);

	/* Update Toolox and Iconbox */
    
	gboolean client_mode;
	rs_conf_get_boolean("client-mode", &client_mode);
	if (!client_mode)
		gui_fullscreen_changed(rs->iconbox, rs->window_fullscreen, "Iconbox",
												 DEFAULT_CONF_SHOW_ICONBOX_FULLSCREEN, DEFAULT_CONF_SHOW_ICONBOX,
												 CONF_SHOW_ICONBOX_FULLSCREEN, CONF_SHOW_ICONBOX);
	gui_fullscreen_changed(rs->toolbox, rs->window_fullscreen, "Toolbox",
												 DEFAULT_CONF_SHOW_TOOLBOX_FULLSCREEN, DEFAULT_CONF_SHOW_TOOLBOX,
												 CONF_SHOW_TOOLBOX_FULLSCREEN, CONF_SHOW_TOOLBOX);
	rs_core_action_group_set_sensivity("Lightsout", !rs->window_fullscreen);
}

TOGGLEACTION(fullscreen_preview)
{
	if (gtk_toggle_action_get_active(toggleaction))
	{
		rs_core_action_group_set_sensivity("Lightsout", FALSE);
		gui_select_preview_screen(rs);
	}
	else
	{
		gui_disable_preview_screen(rs);
		rs_core_action_group_set_sensivity("Lightsout", !rs->window_fullscreen);
	}
}

TOGGLEACTION(exposure_mask)
{
	if (gtk_toggle_action_get_active(toggleaction))
		gui_status_notify(_("Showing exposure mask"));
	else
		gui_status_notify(_("Hiding exposure mask"));
	rs_preview_widget_set_show_exposure_mask(RS_PREVIEW_WIDGET(rs->preview), gtk_toggle_action_get_active(toggleaction));
}

TOGGLEACTION(load_selected)
{
	gtk_toggle_action_set_active(toggleaction, !rs_store_set_open_selected(rs->store, !gtk_toggle_action_get_active(toggleaction)));
}
TOGGLEACTION(split)
{
	rs_preview_widget_set_split(RS_PREVIEW_WIDGET(rs->preview), gtk_toggle_action_get_active(toggleaction));
}

#if GTK_CHECK_VERSION(2,12,0)
TOGGLEACTION(lightsout)
{
	rs_preview_widget_set_lightsout(RS_PREVIEW_WIDGET(rs->preview), gtk_toggle_action_get_active(toggleaction));
}
#endif

ACTION(add_to_batch)
{
	GString *gs = g_string_new("");
	GList *selected = NULL;
	gint num_selected, cur, num_added = 0;

	gui_set_busy(TRUE);
	guint msg = gui_status_push(_("Adding images to batch queue"));
	GTK_CATCHUP();

	selected = rs_store_get_selected_names(rs->store);
	num_selected = g_list_length(selected);

	if (RS_IS_PHOTO(rs->photo) && rs_store_is_photo_selected(rs->store, rs->photo->filename))
	{
		rs_cache_save(rs->photo, MASK_ALL);

		if (rs_batch_add_to_queue(rs->queue, rs->photo->filename, rs->current_setting))
			num_added++;
	}
	/* Deal with selected icons */
	for(cur=0;cur<num_selected;cur++)
		if (rs_batch_add_to_queue(rs->queue, g_list_nth_data(selected, cur), rs->current_setting))
			num_added++;
	g_string_printf(gs, _("%d photos added to batch queue"), num_added);

	g_list_free(selected);
	gui_status_notify(gs->str);
	g_string_free(gs, TRUE);
	rs_core_actions_update_menu_items(rs);

	gui_set_busy(FALSE);
	gui_status_pop(msg);
}

ACTION(add_view_to_batch)
{
	GString *gs = g_string_new("");
	GtkWidget *dialog, *cb_box;
	GtkWidget *cb_a, *cb_b, *cb_c;

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

		gui_set_busy(TRUE);
		rs_store_get_names(rs->store, &selected, NULL, NULL);
		selected = rs_store_sort_selected(selected);
		num_selected = g_list_length(selected);

		if (0 == num_selected && RS_IS_PHOTO(rs->photo))
		{
			selected = g_list_append(selected, g_strdup(rs->photo->filename));
			num_selected++;
		}

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
		g_list_foreach(selected, (GFunc) g_free, NULL);
		g_list_free(selected);

		/* Save settings of current photo just to be sure */
		if (rs->photo)
			rs_cache_save(rs->photo, MASK_ALL);

		g_string_printf(gs, _("%d photos added to batch queue"),
			((gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(cb_a))) ? num_selected : 0)
			+ ((gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(cb_b))) ? num_selected : 0)
			+ ((gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(cb_c))) ? num_selected : 0));

		gui_set_busy(FALSE);
		gui_status_notify(gs->str);
	}

	gtk_widget_destroy (dialog);
	g_string_free(gs, TRUE);
	rs_core_actions_update_menu_items(rs);
}

ACTION(remove_from_batch)
{
	GList *selected = NULL;
	GString *gs = g_string_new("");
	gint num_selected, i, num_removed = 0;

	gui_set_busy(TRUE);
	guint msg = gui_status_push(_("Removing images from batch queue"));
	GTK_CATCHUP();

	rs_store_get_names(rs->store, &selected, NULL, NULL);
	selected = rs_store_sort_selected(selected);
	num_selected = g_list_length(selected);

	for (i=0;i<num_selected;i++)
	{
		gchar *fullname = g_list_nth_data(selected, i);
		if (rs_batch_remove_from_queue(rs->queue, fullname, rs->current_setting))
			num_removed++;
	}
	g_list_foreach(selected, (GFunc) g_free, NULL);
	g_list_free(selected);
	g_string_printf(gs, _("%d photos removed from batch queue"), num_removed);
	gui_set_busy(FALSE);
	gui_status_pop(msg);
	gui_status_notify(gs->str);
	g_string_free(gs, TRUE);
}

/* This is protected by gdk_thread */
static GThread* process_thread = 0;

static gpointer
start_process_batch(gpointer data)
{
	RS_QUEUE *queue = data;

	rs_batch_process(queue);

	gdk_threads_enter();
	process_thread = NULL;
	gdk_threads_leave();
	return NULL;
}

ACTION(ProcessBatch)
{
	/* Save current photo just in case it's in the queue */
	if (RS_IS_PHOTO(rs->photo))
		rs_cache_save(rs->photo, MASK_ALL);

	if (NULL == process_thread)
			process_thread = g_thread_create(start_process_batch, rs->queue, FALSE, NULL);
}

ACTION(lens_db_editor)
{
	rs_lens_db_editor();
}

ACTION(tethered_shooting)
{
	rs_tethered_shooting_open(rs);
}

ACTION(filter_graph)
{
	rs_filter_graph(rs->filter_input);
}

ACTION(add_profile)
{
	GtkWidget *dialog = gtk_file_chooser_dialog_new(
		_("Add Profile ..."),
		rawstudio_window,
		GTK_FILE_CHOOSER_ACTION_OPEN,
		GTK_STOCK_CANCEL,
		GTK_RESPONSE_CANCEL,
		GTK_STOCK_OPEN,
		GTK_RESPONSE_ACCEPT,
		NULL);

	GtkFileFilter *filter_all = gtk_file_filter_new();

	GtkFileFilter *filter_profiles = gtk_file_filter_new();
	gtk_file_filter_set_name(filter_profiles, _("All Profiles"));
	gtk_file_filter_add_pattern(filter_profiles, "*.dcp");
	gtk_file_filter_add_pattern(filter_profiles, "*.DCP");
	gtk_file_filter_add_pattern(filter_profiles, "*.icc");
	gtk_file_filter_add_pattern(filter_profiles, "*.ICC");
	gtk_file_filter_add_pattern(filter_profiles, "*.icm");
	gtk_file_filter_add_pattern(filter_profiles, "*.ICM");
	gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), filter_profiles);

	GtkFileFilter *filter_dcp = gtk_file_filter_new();
	gtk_file_filter_set_name(filter_dcp, _("Camera Profiles (DCP)"));
	gtk_file_filter_add_pattern(filter_dcp, "*.dcp");
	gtk_file_filter_add_pattern(filter_dcp, "*.DCP");
	gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), filter_dcp);

	GtkFileFilter *filter_icc = gtk_file_filter_new();
	gtk_file_filter_set_name(filter_icc, _("Color Profiles (ICC and ICM)"));
	gtk_file_filter_add_pattern(filter_icc, "*.icc");
	gtk_file_filter_add_pattern(filter_icc, "*.ICC");
	gtk_file_filter_add_pattern(filter_icc, "*.icm");
	gtk_file_filter_add_pattern(filter_icc, "*.ICM");
	gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), filter_icc);

	gtk_file_filter_set_name(filter_all, _("All files"));
	gtk_file_filter_add_pattern(filter_all, "*");
	gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), filter_all);

	if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT)
	{
		RSProfileFactory *factory = rs_profile_factory_new_default();
		gchar *path = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
		gchar *basename = g_path_get_basename(path);
		const gchar *userdir = rs_profile_factory_get_user_profile_directory();
		g_mkdir_with_parents(userdir, 00755);
		gchar *new_path = g_build_filename(userdir, basename, NULL);

		if (rs_file_copy(path, new_path))
			rs_profile_factory_add_profile(factory, new_path);
		g_free(path);
		g_free(basename);
		g_free(new_path);
	}

	gtk_widget_destroy (dialog);
}

ACTION(online_documentation)
{
	const gchar *link = "http://rawstudio.org/documentation/";

#ifdef WIN32
#warning This is untested
	gchar* argv[]= {
		getenv("ComSpec"),
		"/c",
		"start",
		"uri click", /* start needs explicit title incase link has spaces or quotes */
		(gchar*) link,
		NULL
	};
#else
	gchar *argv[]= {
		"gnome-open", /* this feels like cheating! */
		(gchar *) link,
		NULL
		};
#endif
	GError *error = NULL;
	gint status = 0;
	gboolean res;

	res = g_spawn_sync(
		NULL /*PWD*/,
		argv,
		NULL /*envp*/,
		G_SPAWN_SEARCH_PATH, /*flags*/
		NULL, /*setup_func*/
		NULL, /*user data for setup*/
		NULL,
		NULL, /* stdin/out/error */
		&status,
		&error);

	if(!res)
	{
		gui_status_error(_("Could not open browser, please go to http://rawstudio.org/documentation"));
		return ;
	}
}

ACTION(about)
{
	const static gchar *authors[] = {
		"Anders Brander <anders@brander.dk>",
		"Anders Kvist <anders@kvistmail.dk>",
		"Klaus Post <klauspost@gmail.com>",
		NULL
	};
	const static gchar *artists[] = {
		"Kristoffer Jørgensen <kristoffer@vektormusik.dk>",
		"Rune Stowasser <rune.stowasser@gmail.com>",
		NULL
	};

	const static gchar *translators =
		"Simone Contini\n"
		"Paweł Gołaszewski\n"
		"Alexandre Prokoudine\n"
		"Jakub Friedl\n"
		"Carsten Mathaes\n"
		"Edouard Gomez\n"
		"Martin Egger\n"
		"Krzysztof Kościuszkiewicz\n"
		"Einar Ryeng\n"
		"Olli Hänninen\n"
		"Carlos Dávila\n"
		"Patrik Jarl\n"
		"Olav Lavell\n"
		"Rafael Sachetto Oliveira\n"
		"Paco Rivière\n"
		"Tuomas Haarala\n"
		"Martin Nilsson\n"
		"Ryo Fujita\n"
		"Roman Schulz\n"
		"Stian Grindvoll\n"
		"Dariusz Duma\n"
		"Ger Siemerink";

	gtk_show_about_dialog(GTK_WINDOW(rawstudio_window),
		"program-name", "Rawstudio",
		"authors", authors,
		"artists", artists,
		"translator-credits", translators, 
		"comments", _("A raw image converter for GTK+/GNOME"),
		"version", RAWSTUDIO_VERSION,
		"website", "http://rawstudio.org/",
		"name", "Rawstudio",
		NULL
	);
}

GList *
get_thumbnails_from_list(GList *files)
{
  if (files == NULL)
    return NULL;

  gint i;
  gint num_selected = g_list_length(files);
  gchar *name = NULL;
  GList *thumbnails = NULL;

  for(i=0; i<num_selected; i++) 
    {
      name = (gchar*) g_list_nth_data(files, i);
      thumbnails = g_list_append(thumbnails, rs_metadata_dotdir_helper(name, DOTDIR_THUMB));
    }

  return thumbnails;
}

void
enfuse_size_changed (GtkRange *range, gint *maxsize) {
  gint size = gtk_range_get_value(GTK_RANGE(range));
  /* to make it easier to adjust when using different size images next time */
  if (size == *maxsize)
    size = 0;
  rs_conf_set_integer(CONF_ENFUSE_SIZE, size);  
}

typedef struct _image_data {
  RS_BLOB *rs;
  GList *selected;
  GtkWidget *image;
} IMAGE_DATA;

static gboolean
update_image_callback (GtkWidget *event_box, GdkEventButton *event, IMAGE_DATA *image_data) 
{
  GdkCursor* cursor = gdk_cursor_new(GDK_WATCH);
  gdk_window_set_cursor(gtk_widget_get_window(event_box), cursor);
  gdk_cursor_unref(cursor);

  gchar *thumb = rs_enfuse(image_data->rs, image_data->selected, TRUE, 250);
  gtk_image_set_from_file(GTK_IMAGE(image_data->image), thumb);
  unlink(thumb);

  gdk_window_set_cursor(gtk_widget_get_window(event_box), NULL);

  return TRUE;
}

guint enfuse_cache_hash (gconstpointer key) {
  return g_str_hash(key);
}

gboolean enfuse_cache_hash_equal (gconstpointer a, gconstpointer b) {
  return g_str_equal(a, b);
}

gboolean cache_cleanup_free_data(gpointer key, gpointer value, gpointer user_data) {
  /* FIXME: is this enough? */
  g_object_unref(RS_PHOTO(value));
  g_free(key);
  return TRUE;
}

void
cache_cleanup(GHashTable *hashtable) {
  if (hashtable)
    g_hash_table_foreach_remove(hashtable, cache_cleanup_free_data, NULL);
}

ACTION(enfuse)
{
  rs_preview_widget_blank((RSPreviewWidget *) rs->preview);
  rs_photo_close(rs->photo);
  rs->photo = NULL;

  rs_filter_set_recursive(rs->filter_input, "color-space", rs_color_space_new_singleton("RSProphoto"), NULL);

  gboolean enfuse = TRUE;
  GList *selected_names = rs_store_get_selected_names(rs->store);
  gint num_selected = g_list_length(selected_names);
  guint priority = rs_store_get_current_priority(rs->store);

  g_assert(selected_names);

  gchar *temp = g_list_nth_data(selected_names, 0);
  RS_PHOTO *temp_photo = rs_photo_load_from_file(temp);
  gint maxsize = 0;
  if (temp_photo->input->w < temp_photo->input->h)
    maxsize = temp_photo->input->h;
  else
    maxsize = temp_photo->input->w;
  rs_photo_close(temp_photo);

  /* a bit of cleanup before we start enfusing */
  rs_photo_close(rs->photo);
  rs_preview_widget_set_photo((RSPreviewWidget *) rs->preview, NULL);
  rs_preview_widget_lock_renderer((RSPreviewWidget *) rs->preview);
  GUI_CATCHUP();

  /* initialize cache system */
  gboolean enfuse_cache = DEFAULT_CONF_ENFUSE_CACHE;
  rs_conf_get_boolean_with_default(CONF_ENFUSE_CACHE, &enfuse_cache, DEFAULT_CONF_ENFUSE_CACHE);
  if (!rs->enfuse_cache && enfuse_cache)
    rs->enfuse_cache = g_hash_table_new(enfuse_cache_hash, enfuse_cache_hash_equal);
  else
    printf("Enfuse cache disabled\n");

  gui_set_busy(TRUE);
  GList *thumbs = get_thumbnails_from_list(selected_names);
  gchar *thumb = rs_enfuse(rs, thumbs, TRUE, 250);
  gui_set_busy(FALSE);

  GtkWidget *dialog = NULL;
  dialog = gui_dialog_make_from_text(GTK_STOCK_DIALOG_QUESTION, _("Enfuse"), _("This might take quite some time and will lock up UI until finished..."));
  gtk_dialog_add_button (GTK_DIALOG (dialog), GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL);
  gtk_dialog_add_button (GTK_DIALOG (dialog), GTK_STOCK_EXECUTE, GTK_RESPONSE_ACCEPT);
  gtk_window_set_title(GTK_WINDOW(dialog), _("Rawstudio Enfuse"));

  GtkWidget *content = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
  GtkWidget *image = gtk_image_new_from_file(thumb);
  GtkWidget *image_container = gtk_event_box_new();
  gtk_container_add (GTK_CONTAINER (image_container), image);
  IMAGE_DATA *image_data = g_new0(IMAGE_DATA, 1);
  image_data->rs = rs;
  image_data->selected = selected_names;
  image_data->image = image;

  g_signal_connect (G_OBJECT (image_container), "button_press_event", G_CALLBACK (update_image_callback), image_data);

  GtkWidget *vbox = gtk_vbox_new(FALSE, 5);
  gtk_box_pack_start(GTK_BOX(vbox), image_container, TRUE, TRUE, 5);

  GtkListStore *enfuse_methods = gtk_list_store_new( 1, G_TYPE_STRING );
  GtkTreeIter iter;
  gtk_list_store_append(enfuse_methods, &iter);
  gtk_list_store_set(enfuse_methods, &iter, 0, ENFUSE_METHOD_EXPOSURE_BLENDING, -1 );
  gtk_list_store_append(enfuse_methods, &iter);
  gtk_list_store_set(enfuse_methods, &iter, 0, ENFUSE_METHOD_FOCUS_STACKING, -1 );

  GtkWidget *enfuse_method_combo = rs_combobox_new(_("Enfuse method"), enfuse_methods, CONF_ENFUSE_METHOD);
  gtk_box_pack_start(GTK_BOX(vbox), enfuse_method_combo, TRUE, TRUE, 5);

  GtkWidget *align_check = checkbox_from_conf(CONF_ENFUSE_ALIGN_IMAGES, _("Align images (may take a long time)"), DEFAULT_CONF_ENFUSE_ALIGN_IMAGES);
  gtk_box_pack_start(GTK_BOX(vbox), align_check, TRUE, TRUE, 5);

  GtkWidget *extend_check = checkbox_from_conf(CONF_ENFUSE_EXTEND, _("Extend exposure"), DEFAULT_CONF_ENFUSE_EXTEND);
  gtk_box_pack_start(GTK_BOX(vbox), extend_check, TRUE, TRUE, 5);

  if (num_selected == 1) 
    {
      gtk_widget_set_sensitive(align_check, FALSE);
      gtk_widget_set_sensitive(extend_check, FALSE);
    }

  GtkWidget *extend_negative = rs_spinbox_new(_("Extend negative steps"), 
					      (num_selected == 1) ? CONF_ENFUSE_EXTEND_NEGATIVE_SINGLE : CONF_ENFUSE_EXTEND_NEGATIVE_MULTI,
					      (num_selected == 1) ? DEFAULT_CONF_ENFUSE_EXTEND_NEGATIVE_SINGLE : DEFAULT_CONF_ENFUSE_EXTEND_NEGATIVE_MULTI,
					      0.0, 10.0, 1, 2);
  GtkWidget *extend_positive = rs_spinbox_new(_("Extend positive steps"),
					      (num_selected == 1) ? CONF_ENFUSE_EXTEND_POSITIVE_SINGLE : CONF_ENFUSE_EXTEND_POSITIVE_MULTI,
					      (num_selected == 1) ? DEFAULT_CONF_ENFUSE_EXTEND_POSITIVE_SINGLE : DEFAULT_CONF_ENFUSE_EXTEND_POSITIVE_MULTI,
0.0, 10.0, 1, 2);
  GtkWidget *extend_step = rs_spinbox_new(_("Extend exposure"), 
					  (num_selected == 1) ? CONF_ENFUSE_EXTEND_STEP_SINGLE : CONF_ENFUSE_EXTEND_STEP_MULTI,
					  (num_selected == 1) ? DEFAULT_CONF_ENFUSE_EXTEND_STEP_SINGLE : DEFAULT_CONF_ENFUSE_EXTEND_STEP_MULTI,
					  0.5, 10.0, 0.5, 1);
  gtk_box_pack_start(GTK_BOX(vbox), extend_negative, TRUE, TRUE, 5);
  gtk_box_pack_start(GTK_BOX(vbox), extend_positive, TRUE, TRUE, 5);
  gtk_box_pack_start(GTK_BOX(vbox), extend_step, TRUE, TRUE, 5);

  gint size_value = DEFAULT_CONF_ENFUSE_SIZE;
  if (!rs_conf_get_integer(CONF_ENFUSE_SIZE, &size_value))
    size_value = DEFAULT_CONF_ENFUSE_SIZE;
  if (size_value == 0)
    size_value = maxsize;

  GtkWidget *size_scale = gtk_hscale_new_with_range(300, maxsize, 1.0);
  gtk_range_set_value(GTK_RANGE(size_scale), size_value);
  g_signal_connect(size_scale, "value-changed", G_CALLBACK(enfuse_size_changed), &maxsize);
  GtkWidget *size_label = gtk_label_new("Size:");
  GtkWidget *size_box = gtk_hbox_new(FALSE, 5);
  gtk_box_pack_start(GTK_BOX(size_box), size_label, FALSE, TRUE, 5);
  gtk_box_pack_start(GTK_BOX(size_box), size_scale, TRUE, TRUE, 5);
  gtk_box_pack_start(GTK_BOX(vbox), size_box, TRUE, TRUE, 5);

  gtk_container_add(GTK_CONTAINER(content), vbox);
  gtk_widget_show_all(dialog);

#ifndef EXPERIMENTAL
  gtk_widget_hide(enfuse_method_combo);
#endif

  if (gtk_dialog_run(GTK_DIALOG(dialog)) != GTK_RESPONSE_ACCEPT)
    {
      gtk_widget_destroy(dialog);
      /* unlock render or we won't be able to do anything */
      rs_preview_widget_unlock_renderer((RSPreviewWidget *) rs->preview);
      cache_cleanup(rs->enfuse_cache);
      return;
    }
  gtk_widget_destroy(dialog);

  gui_set_busy(TRUE);
  gchar *filename = rs_enfuse(rs, selected_names, FALSE, -1);
  gui_set_busy(FALSE);

  cache_cleanup(rs->enfuse_cache);

  g_list_free(selected_names);
  rs_cache_save_flags(filename, &priority, NULL, &enfuse);

  /* reload store - grabbed from reload function */
  rs_store_remove(rs->store, NULL, NULL);
  rs_store_load_directory(rs->store, NULL);
  rs_core_actions_update_menu_items(rs);

  rs_store_set_selected_name(rs->store, filename, TRUE);

  /* unlocking render after enfusing */
  rs_preview_widget_unlock_renderer((RSPreviewWidget *) rs->preview);

  g_free(filename);
}

ACTION(auto_adjust_curve_ends)
{
  GtkWidget *curve = rs_toolbox_get_curve(RS_TOOLBOX(rs->tools), rs->current_setting);
  rs_curve_auto_adjust_ends(curve);
  GUI_CATCHUP();
}


RADIOACTION(right_popup)
{
	rs_preview_widget_set_snapshot(RS_PREVIEW_WIDGET(rs->preview), 1, gtk_radio_action_get_current_value(radioaction));
}

RADIOACTION(sort_by_popup)
{
	rs_store_set_sort_method(rs->store, gtk_radio_action_get_current_value(radioaction));
}

#ifndef GTK_STOCK_FULLSCREEN
 #define GTK_STOCK_FULLSCREEN NULL
#endif

/**
 * Get the core action group
 * @return A pointer to the core action group
 */
GtkActionGroup *
rs_get_core_action_group(RS_BLOB *rs)
{
	gboolean show_filenames;
	gboolean load_8bit = FALSE;

	rs_conf_get_boolean_with_default(CONF_SHOW_FILENAMES, &show_filenames, DEFAULT_CONF_SHOW_FILENAMES);
	rs_conf_get_boolean_with_default(CONF_LOAD_GDK, &load_8bit, FALSE);

	/* FIXME: This should be static */
	GtkActionEntry actionentries[] = {
	{ "FileMenu", NULL, _("_File"), NULL, NULL, ACTION_CB(file_menu) },
	{ "EditMenu", NULL, _("_Edit"), NULL, NULL, ACTION_CB(edit_menu) },
	{ "PhotoMenu", NULL, _("_Photo"), NULL, NULL, ACTION_CB(photo_menu) },
	{ "PriorityMenu", NULL, _("_Set Priority") },
	{ "WhiteBalanceMenu", "NULL", _("_White Balance") },
	{ "ViewMenu", NULL, _("_View"), NULL, NULL, ACTION_CB(view_menu) },
	{ "SortByMenu", NULL, _("_Sort by") },
	{ "BatchMenu", NULL, _("_Batch"), NULL, NULL, ACTION_CB(batch_menu) },
	{ "HelpMenu", NULL, _("_Help") },
	{ "PreviewPopup", NULL, NULL, NULL, NULL, ACTION_CB(preview_popup) },
	{ "SnapshotMenu", NULL, _("_Snapshot") },

	/* File menu */
	{ "Open", GTK_STOCK_OPEN, _("_Open Directory"), "<control>O", NULL, ACTION_CB(open) },
	{ "QuickExport", GTK_STOCK_SAVE, _("_Quick Export"), "<control>S", NULL, ACTION_CB(quick_export) },
	{ "ExportAs", GTK_STOCK_SAVE_AS, _("_Export As"), "<control><shift>S", NULL, ACTION_CB(export_as) },
	{ "ExportToGimp", GTK_STOCK_EXECUTE, _("_Export to Gimp"), "<control>G", NULL, ACTION_CB(export_to_gimp) },
	{ "CopyImage", GTK_STOCK_COPY, _("_Copy Image to Clipboard"), "<control><shift>C", NULL, ACTION_CB(copy_image) },
	{ "Reload", GTK_STOCK_REFRESH, _("_Reload directory"), "<control>R", NULL, ACTION_CB(reload) },
	{ "DeleteFlagged", GTK_STOCK_DELETE, _("_Delete Flagged Photos"), "<control><shift>D", NULL, ACTION_CB(delete_flagged) },
	{ "Quit", GTK_STOCK_QUIT, _("_Quit"), "<control>Q", NULL, ACTION_CB(quit) },
	{ "CancelAndQuit", NULL, _("_Just Quit"), NULL, NULL, ACTION_CB(quit) },
	{ "SaveAndQuit", GTK_STOCK_SAVE, _("Save & _Quit"), "<control>Q", NULL, ACTION_CB(client_mode_quit) },

	/* Edit menu */
	{ "RevertSettings", GTK_STOCK_UNDO, _("_Revert Settings"), "<control>Z", NULL, ACTION_CB(revert_settings) },
	{ "CopySettings", GTK_STOCK_COPY, _("_Copy Settings"), "<control>C", NULL, ACTION_CB(copy_settings) },
	{ "PasteSettings", GTK_STOCK_PASTE, _("_Paste Settings"), "<control>V", NULL, ACTION_CB(paste_settings) },
	{ "ResetSettings", GTK_STOCK_REFRESH, _("_Reset Settings"), NULL, NULL, ACTION_CB(reset_settings) },
	{ "SaveDefaultSettings", NULL, _("_Save Camera Default Settings"), NULL, NULL, ACTION_CB(save_default_settings) },
	{ "Preferences", GTK_STOCK_PREFERENCES, _("_Preferences"), NULL, NULL, ACTION_CB(preferences) },

	/* Photo menu */
	{ "FlagPhoto", GTK_STOCK_DELETE, _("_Flag Photo for Deletion"), "Delete", NULL, ACTION_CB(flag_for_deletion) },
	{ "Priority1", NULL, _("_1"), "1", NULL, ACTION_CB(priority_1) },
	{ "Priority2", NULL, _("_2"), "2", NULL, ACTION_CB(priority_2) },
	{ "Priority3", NULL, _("_3"), "3", NULL, ACTION_CB(priority_3) },
	{ "RemovePriority", NULL, _("_Remove Priority"), "0", NULL, ACTION_CB(priority_0) },
	{ "AutoWB", NULL, _("_Auto"), "A", NULL, ACTION_CB(auto_wb) },
	{ "CameraWB", NULL, _("_Camera"), "C", NULL, ACTION_CB(camera_wb) },
	{ "Crop", RS_STOCK_CROP, _("_Crop"), "<shift>C", NULL, ACTION_CB(crop) },
	{ "Uncrop", NULL, _("_Uncrop"), "<shift>V", NULL, ACTION_CB(uncrop) },
	{ "Straighten", RS_STOCK_ROTATE, _("_Straighten"), NULL, NULL, ACTION_CB(straighten) },
	{ "Unstraighten", NULL, _("_Unstraighten"), NULL, NULL, ACTION_CB(unstraighten) },
	{ "Group", NULL, _("_Group"), NULL, NULL, ACTION_CB(group_photos) },
	{ "Ungroup", NULL, _("_Ungroup"), NULL, NULL, ACTION_CB(ungroup_photos) },
	{ "AutoGroup", NULL, _("_Auto Group"), NULL, NULL, ACTION_CB(auto_group_photos) },
	{ "TagPhoto", NULL, _("_Tag Photo..."), "<alt>T", NULL, ACTION_CB(tag_photo) },
	{ "RotateClockwise", RS_STOCK_ROTATE_CLOCKWISE, _("Rotate Clockwise"), "<alt>Right", NULL, ACTION_CB(rotate_clockwise) },
	{ "RotateCounterClockwise", RS_STOCK_ROTATE_COUNTER_CLOCKWISE, _("Rotate Counterclockwise"), "<alt>Left", NULL, ACTION_CB(rotate_counter_clockwise) },
	{ "Flip", RS_STOCK_FLIP, _("Flip"), NULL, NULL, ACTION_CB(flip) },
	{ "Mirror", RS_STOCK_MIRROR, _("Mirror"), NULL, NULL, ACTION_CB(mirror) },
	{ "Enfuse", NULL, _("Enfuse"), "<ctrl><alt>E", NULL, ACTION_CB(enfuse) },
	{ "AutoAdjustCurveEnds", NULL, _("Auto adjust curve ends"), "<control><shift>L", NULL, ACTION_CB(auto_adjust_curve_ends) },

	/* View menu */
	{ "PreviousPhoto", GTK_STOCK_GO_BACK, _("_Previous Photo"), "<control>Left", NULL, ACTION_CB(previous_photo) },
	{ "NextPhoto", GTK_STOCK_GO_FORWARD, _("_Next Photo"), "<control>Right", NULL, ACTION_CB(next_photo) },
	{ "Play", GTK_STOCK_MEDIA_PLAY, _("_Play/Stop"), "<control>P", NULL, ACTION_CB(play) },
	{ "LensDbEditor", NULL, _("_Lens Library"), "<control>L", NULL, ACTION_CB(lens_db_editor) },
	{ "TetheredShooting", NULL, _("_Tethered Shooting"), "F9", NULL, ACTION_CB(tethered_shooting) },
	
	/* Batch menu */
	{ "AddToBatch", GTK_STOCK_ADD, _("_Add to Batch Queue"), "Insert", NULL, ACTION_CB(add_to_batch) },
	{ "AddViewToBatch", NULL, _("_Add View to Queue..."), "<control>Insert", NULL, ACTION_CB(add_view_to_batch) },
	{ "RemoveFromBatch", GTK_STOCK_REMOVE, _("_Remove from Batch Queue"), "<control>Delete", NULL, ACTION_CB(remove_from_batch) },
	{ "ProcessBatch", GTK_STOCK_EXECUTE, _("_Start"), NULL, NULL, ACTION_CB(ProcessBatch) },

	/* help menu */
	{ "OnlineDocumentation", GTK_STOCK_HELP, _("_Online Documentation"), NULL, NULL, ACTION_CB(online_documentation) },
	{ "About", GTK_STOCK_ABOUT, _("_About"), NULL, NULL, ACTION_CB(about) },
	{ "FilterGraph", NULL, "_Filter Graph", NULL, NULL, ACTION_CB(filter_graph) },

	/* Not in any menu (yet) */
	{ "AddProfile", NULL, _("Add Profile ..."), NULL, NULL, ACTION_CB(add_profile) },
	};
	static guint n_actionentries = G_N_ELEMENTS (actionentries);

	GtkToggleActionEntry toggleentries[] = {
	{ "ZommToFit", GTK_STOCK_ZOOM_FIT, _("_Zoom to Fit"), "asterisk", NULL, ACTION_CB(zoom_to_fit), TRUE },
	{ "Iconbox", NULL, _("_Iconbox"), "<control>I", NULL, ACTION_CB(iconbox), TRUE },
	{ "Toolbox", NULL, _("_Toolbox"), "<control>T", NULL, ACTION_CB(toolbox), TRUE },
	{ "Fullscreen", GTK_STOCK_FULLSCREEN, _("_Fullscreen"), "F11", NULL, ACTION_CB(fullscreen), FALSE },
	{ "FullscreenPreview", GTK_STOCK_FULLSCREEN, _("_Show Photo on Secondary Monitor"), "F10", NULL, ACTION_CB(fullscreen_preview), FALSE },
	{ "ShowFilenames", NULL, _("Show Filenames in Iconbox"), NULL, NULL, ACTION_CB(show_filenames), show_filenames },
	{ "Load8Bit", NULL, _("Load non-RAW images"), NULL, NULL, ACTION_CB(load_8bit), load_8bit },
	{ "LoadSelected", NULL, _("Do not Load Selected Images"), "Pause", NULL, ACTION_CB(load_selected), FALSE },
	{ "ExposureMask", NULL, _("_Exposure Mask"), "<control>E", NULL, ACTION_CB(exposure_mask), FALSE },
	{ "Split", NULL, _("_Split"), "<control>D", NULL, ACTION_CB(split), FALSE },
#if GTK_CHECK_VERSION(2,12,0)
	{ "Lightsout", NULL, _("_Lights Out"), "F12", NULL, ACTION_CB(lightsout), FALSE },
#endif
	};
	static guint n_toggleentries = G_N_ELEMENTS (toggleentries);

	GtkRadioActionEntry sort_by_popup[] = {
	{ "SortByName", NULL, _("Name"), NULL, NULL, RS_STORE_SORT_BY_NAME },
	{ "SortByTimestamp", NULL, _("Timestamp"), NULL, NULL, RS_STORE_SORT_BY_TIMESTAMP },
	{ "SortByISO", NULL, _("ISO"), NULL, NULL, RS_STORE_SORT_BY_ISO },
	{ "SortByAperture", NULL, _("Aperture"), NULL, NULL, RS_STORE_SORT_BY_APERTURE },
	{ "SortByFocallength", NULL, _("Focallength"), NULL, NULL, RS_STORE_SORT_BY_FOCALLENGTH },
	{ "SortByShutterspeed", NULL, _("Shutterspeed"), NULL, NULL, RS_STORE_SORT_BY_SHUTTERSPEED },
	};
	static guint n_sort_by_popup = G_N_ELEMENTS (sort_by_popup);

	GtkRadioActionEntry right_popup[] = {
	{ "RightA", NULL, _(" A "), NULL, NULL, 0 },
	{ "RightB", NULL, _(" B "), NULL, NULL, 1 },
	{ "RightC", NULL, _(" C "), NULL, NULL, 2 },
	};
	static guint n_right_popup = G_N_ELEMENTS (right_popup);

	g_static_mutex_lock(&rs_actions_spinlock);
	if (core_action_group == NULL)
	{
		core_action_group = gtk_action_group_new ("CoreActions");
		/* FIXME: gtk_action_group_set_translation_domain */
		gtk_action_group_add_actions (core_action_group, actionentries, n_actionentries, rs);
		gtk_action_group_add_toggle_actions(core_action_group, toggleentries, n_toggleentries, rs);
		gtk_action_group_add_radio_actions(core_action_group, sort_by_popup, n_sort_by_popup, rs_store_get_sort_method(rs->store), ACTION_CB(sort_by_popup), rs);
		gtk_action_group_add_radio_actions(core_action_group, right_popup, n_right_popup, 1, ACTION_CB(right_popup), rs);
	}
	g_static_mutex_unlock(&rs_actions_spinlock);

	return core_action_group;
}

/**
 * Set sensivity of an action
 * @param name The name of the action
 * @param sensitive The sensivity of the action
 */
void
rs_core_action_group_set_sensivity(const gchar *name, gboolean sensitive)
{
	if (core_action_group)
		gtk_action_set_sensitive(gtk_action_group_get_action(core_action_group, name), sensitive);
}

/**
 * Activate an action
 * @param name The action to activate
 */
void
rs_core_action_group_activate(const gchar *name)
{
	if (core_action_group)
		gtk_action_activate(gtk_action_group_get_action(core_action_group, name));
}

/**
 * Set visibility of an action
 * @param name The name of the action
 * @param visibility The visibility of the action
 */
void
rs_core_action_group_set_visibility(const gchar *name, gboolean visible)
{
	if (core_action_group)
		gtk_action_set_visible(gtk_action_group_get_action(core_action_group, name), visible);
}

/**
 * Add actions to global action group, see documentation for gtk_action_group_add_actions
 */
void
rs_core_action_group_add_actions(const GtkActionEntry *entries, guint n_entries, gpointer user_data)
{
	g_static_mutex_lock(&rs_actions_spinlock);
	
	if (core_action_group)
		gtk_action_group_add_actions(core_action_group, entries, n_entries, user_data);
	else
		g_warning("core_action_group is NULL");
	g_static_mutex_unlock(&rs_actions_spinlock);
}

/**
 * Add radio action to global action group, see documentation for gtk_action_group_add_radio_actions()
 */
void
rs_core_action_group_add_radio_actions(const GtkRadioActionEntry *entries, guint n_entries, gint value, GCallback on_change, gpointer user_data)
{
	g_static_mutex_lock(&rs_actions_spinlock);
	
	if (core_action_group)
		gtk_action_group_add_radio_actions(core_action_group, entries, n_entries, value, on_change, user_data);
	else
		g_warning("core_action_group is NULL");
	g_static_mutex_unlock(&rs_actions_spinlock);
}

/**
 * Get a GtkAction by name
 * @param name The name of the action
 * @return A GtkAction or NULL (should not be unreffed)
 */
extern GtkAction *
rs_core_action_group_get_action(const gchar *name)
{
	GtkAction *action = NULL;

	g_static_mutex_lock(&rs_actions_spinlock);

	if (core_action_group)
		action = gtk_action_group_get_action(core_action_group, name);
	else
		g_warning("core_action_group is NULL");

	g_static_mutex_unlock(&rs_actions_spinlock);

	return action;
}
