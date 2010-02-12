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

#include <gtk/gtk.h>
#include <config.h>
#include <gettext.h>
#include "application.h"
#include "gtk-helper.h"
#include "rs-save-dialog.h"
#include "rs-photo.h"
#include "conf_interface.h"

G_DEFINE_TYPE (RSSaveDialog, rs_save_dialog, GTK_TYPE_WINDOW)

static void file_type_changed(gpointer active, gpointer user_data);
static void save_clicked(GtkButton *button, gpointer user_data);
static void cancel_clicked(GtkButton *button, gpointer user_data);
static GtkWidget *size_pref_new(RSSaveDialog *dialog);

static void
rs_save_dialog_dispose (GObject *object)
{
	RSSaveDialog *dialog = RS_SAVE_DIALOG(object);

	if (!dialog->dispose_has_run)
	{
		if (dialog->output)
		{
			g_assert(G_IS_OBJECT(dialog->output));
			g_object_unref(dialog->output);
		}

		gui_confbox_destroy(dialog->type_box);

		g_object_unref(dialog->finput);
		g_object_unref(dialog->fdemosaic);
		g_object_unref(dialog->flensfun);
		g_object_unref(dialog->ftransform_input);
		g_object_unref(dialog->frotate);
		g_object_unref(dialog->fcrop);
		g_object_unref(dialog->fresample);
		g_object_unref(dialog->fdcp);
		g_object_unref(dialog->fdenoise);
		g_object_unref(dialog->ftransform_display);

		if (dialog->photo)
			g_object_unref(dialog->photo);

		dialog->dispose_has_run = TRUE;

		/* Chain up */
		G_OBJECT_CLASS (rs_save_dialog_parent_class)->dispose (object);
	}
}

static void
rs_save_dialog_class_init (RSSaveDialogClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->dispose = rs_save_dialog_dispose;
}

static void
rs_save_dialog_init (RSSaveDialog *dialog)
{
	GtkWindow *window = GTK_WINDOW(dialog);
	GtkWidget *button_box;
	GType *savers;
	guint n_savers = 0, i;
	GtkWidget *button_save = gtk_button_new_from_stock(GTK_STOCK_SAVE);
	GtkWidget *button_cancel = gtk_button_new_from_stock(GTK_STOCK_CANCEL);
	const gchar *folder = rs_conf_get_string(CONF_EXPORT_AS_FOLDER);

	g_signal_connect(button_save, "clicked", G_CALLBACK(save_clicked), dialog);
	g_signal_connect(button_cancel, "clicked", G_CALLBACK(cancel_clicked), dialog);

	gtk_window_set_type_hint(window, GDK_WINDOW_TYPE_HINT_DIALOG);
	gtk_window_set_position(window, GTK_WIN_POS_CENTER_ON_PARENT);
	gtk_window_resize(window, 750, 550); /* FIXME: Calculate some sensible size - maybe even remember user resizes */
	gtk_window_set_title (window, _("Export File"));

	dialog->dispose_has_run = FALSE;
	dialog->output = NULL;
	dialog->file_pref = NULL;
	dialog->w_original = 666;
	dialog->h_original = 666;
	dialog->keep_aspect = TRUE;

	dialog->vbox = gtk_vbox_new(FALSE, 0);
	dialog->chooser = gtk_file_chooser_widget_new(GTK_FILE_CHOOSER_ACTION_SAVE);
	if (folder)
		gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(dialog->chooser), folder);
	dialog->type_box = gui_confbox_new((const gchar *) "save-as-filetype");
	dialog->pref_bin = gtk_alignment_new(0.0, 0.5, 1.0, 1.0);

	button_box = gtk_hbutton_box_new();
	gtk_button_box_set_layout (GTK_BUTTON_BOX(button_box), GTK_BUTTONBOX_END);
	gtk_container_add (GTK_CONTAINER(button_box), button_cancel);
	gtk_container_add (GTK_CONTAINER(button_box), button_save);

	/* Try to mimic a real GtkSaveDialog */
	gtk_container_set_border_width(GTK_CONTAINER(window), 5);
	gtk_container_set_border_width(GTK_CONTAINER(dialog->vbox), 2);
	gtk_box_set_spacing(GTK_BOX(dialog->vbox), 2);
	gtk_container_set_border_width(GTK_CONTAINER(dialog->chooser), 5);
	gtk_container_set_border_width(GTK_CONTAINER(button_box), 5);
	gtk_box_set_spacing(GTK_BOX(button_box), 6);
	gtk_container_set_border_width(GTK_CONTAINER(dialog->pref_bin), 5);

	/* Pack everything nicely */
	gtk_container_add(GTK_CONTAINER(window), dialog->vbox);
	gtk_box_pack_start(GTK_BOX(dialog->vbox), dialog->chooser, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(dialog->vbox), gui_confbox_get_widget(dialog->type_box), FALSE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(dialog->vbox), dialog->pref_bin, FALSE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(dialog->vbox), size_pref_new(dialog), FALSE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(dialog->vbox), button_box, FALSE, TRUE, 0);

	/* Set default action */
	GTK_WIDGET_SET_FLAGS(button_save, GTK_CAN_DEFAULT);
    gtk_window_set_default(window, button_save);

	gui_confbox_set_callback(dialog->type_box, dialog, file_type_changed);
	savers = g_type_children (RS_TYPE_OUTPUT, &n_savers);
	for (i = 0; i < n_savers; i++)
	{
		RSOutputClass *klass;
		gchar *name = g_strdup(g_type_name(savers[i]));
		klass = g_type_class_ref(savers[i]);
		gui_confbox_add_entry(dialog->type_box, name, klass->display_name, GINT_TO_POINTER(savers[i]));
		g_type_class_unref(klass);
	}
	g_free(savers);

	/* Load default fromconf, or use RSJpegfile */
	gui_confbox_load_conf(dialog->type_box, "RSJpegfile");

	/* Setup our filter chain for saving */
	dialog->finput = rs_filter_new("RSInputImage16", NULL);
	dialog->fdemosaic = rs_filter_new("RSDemosaic", dialog->finput);
	dialog->flensfun = rs_filter_new("RSLensfun", dialog->fdemosaic);
	dialog->ftransform_input = rs_filter_new("RSColorspaceTransform", dialog->flensfun);
	dialog->frotate = rs_filter_new("RSRotate",dialog->ftransform_input) ;
	dialog->fcrop = rs_filter_new("RSCrop", dialog->frotate);
	dialog->fresample= rs_filter_new("RSResample", dialog->fcrop);
	dialog->fdcp = rs_filter_new("RSDcp", dialog->fresample);
	dialog->fdenoise= rs_filter_new("RSDenoise", dialog->fdcp);
	dialog->ftransform_display = rs_filter_new("RSColorspaceTransform", dialog->fdenoise);
	dialog->fend = dialog->ftransform_display;

	RSIccProfile *profile;
	gchar *filename;
	/* Set input ICC profile */
	profile = NULL;
	filename = rs_conf_get_cms_profile(CMS_PROFILE_INPUT);
	if (filename)
	{
		profile = rs_icc_profile_new_from_file(filename);
		g_free(filename);
	}
	if (!profile)
		profile = rs_icc_profile_new_from_file(PACKAGE_DATA_DIR "/" PACKAGE "/profiles/generic_camera_profile.icc");
//	g_object_set(dialog->filter_input, "icc-profile", profile, NULL);
//	g_object_unref(profile);

	/* Set output ICC profile */
	profile = NULL;
	filename = rs_conf_get_cms_profile(CMS_PROFILE_EXPORT);
	if (filename)
	{
		profile = rs_icc_profile_new_from_file(filename);
		g_free(filename);
	}
	if (!profile)
		profile = rs_icc_profile_new_from_file(PACKAGE_DATA_DIR "/" PACKAGE "/profiles/sRGB.icc");
//	g_object_set(dialog->filter_basic_render, "icc-profile", profile, NULL);
	g_object_unref(profile);
}

RSSaveDialog *
rs_save_dialog_new (void)
{
	return g_object_new (RS_TYPE_SAVE_DIALOG, NULL);
}

void
rs_save_dialog_set_photo(RSSaveDialog *dialog, RS_PHOTO *photo, gint snapshot)
{
	g_assert(RS_IS_SAVE_DIALOG(dialog));
	g_assert(RS_IS_PHOTO(photo));

	/* This should be enough to calculate "original" size */
	rs_filter_set_recursive(dialog->fend, 
		"image", photo->input,
		"angle", photo->angle,
		"orientation", photo->orientation,
		"rectangle", photo->crop,
		"filename", photo->filename,
		NULL);

	if (dialog->photo)
		g_object_unref(dialog->photo);
	dialog->photo = g_object_ref(photo);

	dialog->w_original = rs_filter_get_width(dialog->fcrop);
	dialog->h_original = rs_filter_get_height(dialog->fcrop);

	gtk_spin_button_set_value(dialog->w_spin, dialog->w_original);
	gtk_spin_button_set_value(dialog->h_spin, dialog->h_original);
	gtk_spin_button_set_value(dialog->p_spin, 100.0);

	dialog->snapshot = snapshot;
}

static void
file_type_changed(gpointer active, gpointer user_data)
{
	RSSaveDialog *dialog = RS_SAVE_DIALOG(user_data);
	const gchar *identifier = g_type_name(GPOINTER_TO_INT(active));

	if (dialog->output)
		g_object_unref(dialog->output);
	dialog->output = rs_output_new(identifier);

	if (dialog->file_pref)
		gtk_widget_destroy(dialog->file_pref);
	dialog->file_pref = rs_output_get_parameter_widget(dialog->output, "save-as");

	gtk_container_add(GTK_CONTAINER(dialog->pref_bin), dialog->file_pref);
	gtk_widget_show_all(dialog->file_pref);
}

static gpointer 
job(RSJobQueueSlot *slot, gpointer data)
{
	gfloat actual_scale;
	RSSaveDialog *dialog = RS_SAVE_DIALOG(data);

	gchar *description = g_strdup_printf(_("Exporting to %s"), gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog->chooser)));
	rs_job_update_description(slot, description);
	g_free(description);

	actual_scale = ((gdouble) dialog->save_width / (gdouble) rs_filter_get_width(dialog->fcrop));

	/* Set DCP profile */
	RSDcpFile *dcp_profile  = rs_photo_get_dcp_profile(dialog->photo);
	if (dcp_profile != NULL)
	{
		g_object_set(dialog->fdcp, "profile", dcp_profile, NULL);
	}

	/* Look up lens */
	RSMetadata *meta = rs_photo_get_metadata(dialog->photo);
	RSLensDb *lens_db = rs_lens_db_get_default();
	RSLens *lens = rs_lens_db_lookup_from_metadata(lens_db, meta);

	/* Apply lens information to RSLensfun */
	if (lens)
	{
		rs_filter_set_recursive(dialog->fend,
			"make", meta->make_ascii,
			"model", meta->model_ascii,
			"lens", lens,
			"focal", (gfloat) meta->focallength,
			"aperture", meta->aperture,
			"tca_kr", dialog->photo->settings[dialog->snapshot]->tca_kr,
			"tca_kb", dialog->photo->settings[dialog->snapshot]->tca_kb,
			"vignetting", dialog->photo->settings[dialog->snapshot]->vignetting,
			NULL);
		g_object_unref(lens);
	}

	g_object_unref(meta);

	rs_filter_set_recursive(dialog->fend,
		"width", dialog->save_width,
		"height", dialog->save_height,
		"settings", dialog->photo->settings[dialog->snapshot],
		NULL);
	
	g_object_set(dialog->output, "filename", gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog->chooser)), NULL);
	rs_output_execute(dialog->output, dialog->fend);
	rs_job_update_progress(slot, 0.75);

	gdk_threads_enter();
	gtk_widget_destroy(GTK_WIDGET(dialog));
	gdk_threads_leave();

	return NULL;
}

static void
save_clicked(GtkButton *button, gpointer user_data)
{
	RSSaveDialog *dialog = RS_SAVE_DIALOG(user_data);
	rs_conf_set_string(CONF_EXPORT_AS_FOLDER, g_path_get_dirname(gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog->chooser))));

	/* Just hide it for now, we destroy it in job() */
	gtk_widget_hide_all(GTK_WIDGET(dialog));

	rs_job_queue_add_job(job, g_object_ref(dialog), FALSE);
}

static void
cancel_clicked(GtkButton *button, gpointer user_data)
{
	RSSaveDialog *dialog = RS_SAVE_DIALOG(user_data);

	gtk_widget_destroy(GTK_WIDGET(dialog));
}

static void
size_pref_aspect_changed(GtkToggleButton *togglebutton, gpointer user_data)
{
	RSSaveDialog *dialog = RS_SAVE_DIALOG(user_data);
	dialog->keep_aspect = togglebutton->active;
	if (dialog->keep_aspect)
	{
		gtk_spin_button_set_value(dialog->w_spin, dialog->w_original);
		gtk_spin_button_set_value(dialog->h_spin, dialog->h_original);
		gtk_spin_button_set_value(dialog->p_spin, 100.0);
	}
	return;
}

static void
size_pref_w_changed(GtkSpinButton *spinbutton, gpointer user_data)
{
	RSSaveDialog *dialog = RS_SAVE_DIALOG(user_data);
	gdouble ratio;
	if (dialog->keep_aspect)
	{
		g_signal_handler_block(dialog->h_spin, dialog->h_signal);
		ratio = gtk_spin_button_get_value(spinbutton)/dialog->w_original;
		gtk_spin_button_set_value(dialog->h_spin, dialog->h_original*ratio);
		g_signal_handler_unblock(dialog->h_spin, dialog->h_signal);
	}
	return;
}

static void
size_pref_h_changed(GtkSpinButton *spinbutton, gpointer user_data)
{
	RSSaveDialog *dialog = RS_SAVE_DIALOG(user_data);
	gdouble ratio;
	if (dialog->keep_aspect)
	{
		g_signal_handler_block(dialog->w_spin, dialog->w_signal);
		ratio = gtk_spin_button_get_value(spinbutton)/dialog->h_original;
		gtk_spin_button_set_value(dialog->w_spin, dialog->w_original*ratio);
		g_signal_handler_unblock(dialog->w_spin, dialog->w_signal);
	}
	return;
}

static void
size_pref_p_changed(GtkSpinButton *spinbutton, gpointer user_data)
{
	RSSaveDialog *dialog = RS_SAVE_DIALOG(user_data);
	gdouble ratio;
	g_signal_handler_block(dialog->w_spin, dialog->w_signal);
	g_signal_handler_block(dialog->h_spin, dialog->h_signal);
	ratio = gtk_spin_button_get_value(spinbutton)/100.0;
	gtk_spin_button_set_value(dialog->w_spin, dialog->w_original*ratio);
	gtk_spin_button_set_value(dialog->h_spin, dialog->h_original*ratio);
	g_signal_handler_unblock(dialog->w_spin, dialog->w_signal);
	g_signal_handler_unblock(dialog->h_spin, dialog->h_signal);
	return;
}

static void
spin_set_value(GtkSpinButton *spinbutton, gpointer user_data)
{
	gint *value = (gint *) user_data;
	*value = gtk_spin_button_get_value_as_int(spinbutton);
	return;
}

static GtkWidget *
size_pref_new(RSSaveDialog *dialog)
{
	GtkWidget *vbox, *hbox;
	GtkWidget *checkbox;

	checkbox = gtk_check_button_new_with_label(_("Keep aspect"));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(checkbox), dialog->keep_aspect);
	g_signal_connect ((gpointer) checkbox, "toggled", G_CALLBACK (size_pref_aspect_changed), dialog);

	dialog->w_spin = GTK_SPIN_BUTTON(gtk_spin_button_new_with_range(1.0, 65535.0, 1.0));
	dialog->h_spin = GTK_SPIN_BUTTON(gtk_spin_button_new_with_range(1.0, 65535.0, 1.0));
	dialog->p_spin = GTK_SPIN_BUTTON(gtk_spin_button_new_with_range(1.0, 200.0, 1.0));
	gtk_spin_button_set_value(dialog->w_spin, (gdouble) dialog->save_width);
	gtk_spin_button_set_value(dialog->h_spin, (gdouble) dialog->save_height);
	gtk_spin_button_set_value(dialog->p_spin, 100.0);
	dialog->w_signal = g_signal_connect(G_OBJECT(dialog->w_spin), "value_changed", G_CALLBACK(size_pref_w_changed), dialog);
	dialog->h_signal = g_signal_connect(G_OBJECT(dialog->h_spin), "value_changed", G_CALLBACK(size_pref_h_changed), dialog);
	g_signal_connect(G_OBJECT(dialog->p_spin), "value_changed", G_CALLBACK(size_pref_p_changed), dialog);

	g_signal_connect(G_OBJECT(dialog->w_spin), "value_changed", G_CALLBACK(spin_set_value), &dialog->save_width);
	g_signal_connect(G_OBJECT(dialog->h_spin), "value_changed", G_CALLBACK(spin_set_value), &dialog->save_height);

	hbox = gtk_hbox_new(FALSE, 3);
	gtk_box_pack_start (GTK_BOX (hbox), gtk_label_new_with_mnemonic(_("Width:")), FALSE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (hbox), GTK_WIDGET(dialog->w_spin), FALSE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (hbox), gtk_label_new_with_mnemonic(_("Height:")), FALSE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (hbox), GTK_WIDGET(dialog->h_spin), FALSE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (hbox), gtk_label_new_with_mnemonic(_("Percent:")), FALSE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (hbox), GTK_WIDGET(dialog->p_spin), FALSE, TRUE, 0);

	vbox = gtk_vbox_new(FALSE, 3);
	gtk_box_pack_start (GTK_BOX (vbox), GTK_WIDGET(checkbox), FALSE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (vbox), GTK_WIDGET(hbox), FALSE, TRUE, 0);
	return(vbox);
}
