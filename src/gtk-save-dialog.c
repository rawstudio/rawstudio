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

#include <gtk/gtk.h>
#include "rawstudio.h"
#include "gettext.h"
#include "gtk-interface.h"
#include "gtk-save-dialog.h"
#include "gtk-helper.h"
#include "conf_interface.h"
#include <config.h>

static RS_FILETYPE *filetype;
static GtkWidget *fc;
static GtkWidget *jpeg_pref;
static GtkWidget *tiff_pref;

static void
filetype_changed(GtkComboBox *filetype_combo, gpointer callback_data)
{
	gchar *filename, *newfilename;
	gint n, lastdot=0;

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

		newfilename = g_strconcat(filename, ".", gui_filetype_combobox_get_ext(filetype_combo), NULL);
		gtk_file_chooser_set_current_name (GTK_FILE_CHOOSER (fc), newfilename);

		g_free(filename);
	}

	filetype = gui_filetype_combobox_get_filetype(GTK_COMBO_BOX(filetype_combo));

	/* show relevant preferences */
	gtk_widget_hide(jpeg_pref);
	gtk_widget_hide(tiff_pref);
	switch (filetype->filetype)
	{
		case FILETYPE_JPEG:
			gtk_widget_show(jpeg_pref);
			break;
		case FILETYPE_TIFF8:
		case FILETYPE_TIFF16:
			gtk_widget_show(tiff_pref);
			break;
	}
	return;
}

void
jpeg_quality_changed(GtkAdjustment *adjustment, gpointer user_data)
{
	rs_conf_set_integer(CONF_EXPORT_JPEG_QUALITY, (gint) gtk_adjustment_get_value(adjustment));
	return;
}

GtkWidget *
jpeg_pref_new()
{
	GtkObject *jpeg_quality_adj;
	GtkWidget *jpeg_quality_label;
	GtkWidget *jpeg_quality_scale;
	GtkWidget *jpeg_quality_spin;
	GtkWidget *box;
	gint jpeg_quality;

	rs_conf_get_integer(CONF_EXPORT_JPEG_QUALITY, &jpeg_quality);

	jpeg_quality_adj = gtk_adjustment_new((gdouble) jpeg_quality, 10.0, 100.0, 1.0, 10.0, 0.0);
	g_signal_connect((gpointer) jpeg_quality_adj, "value-changed", G_CALLBACK(jpeg_quality_changed), NULL);
	jpeg_quality_label = gtk_label_new(_("JPEG Quality:"));
	jpeg_quality_scale = gtk_hscale_new(GTK_ADJUSTMENT(jpeg_quality_adj));
	gtk_scale_set_draw_value(GTK_SCALE(jpeg_quality_scale), FALSE);
	jpeg_quality_spin = gtk_spin_button_new(GTK_ADJUSTMENT(jpeg_quality_adj), 1.0, 0);
	
	box = gtk_hbox_new(FALSE, 2);
	gtk_box_pack_start (GTK_BOX (box), jpeg_quality_label, FALSE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (box), jpeg_quality_scale, TRUE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (box), jpeg_quality_spin, FALSE, TRUE, 0);

	return(box);
}

GtkWidget *
tiff_pref_new()
{
	GtkWidget *tiff_uncompressed_checkbox;
	tiff_uncompressed_checkbox = checkbox_from_conf(
		CONF_EXPORT_TIFF_UNCOMPRESSED, _("Save uncompressed TIFF"), FALSE);
	return(tiff_uncompressed_checkbox);
}

void
gui_save_file_dialog(RS_BLOB *rs)
{
	GString *name;
	gchar *dirname;
	gchar *basename;
	GString *export_path;
	gchar *conf_export;
	GtkWidget *filetype_combo;
	RS_FILETYPE *last = NULL;
	GtkWidget *prefbox;

	if (!rs->in_use) return;

	gui_set_busy(TRUE);
	GUI_CATCHUP();

	fc = gtk_file_chooser_dialog_new (_("Export File"), NULL,
		GTK_FILE_CHOOSER_ACTION_SAVE,
		GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
		GTK_STOCK_SAVE, GTK_RESPONSE_ACCEPT, NULL);

	/* make preference box */
	prefbox = gtk_vbox_new(FALSE, 4);
	jpeg_pref = jpeg_pref_new();
	tiff_pref = tiff_pref_new();
	filetype_combo = gui_filetype_combobox();
	filetype = gui_filetype_combobox_get_filetype(GTK_COMBO_BOX(filetype_combo));
	gtk_box_pack_start (GTK_BOX (prefbox), gtk_hseparator_new(), FALSE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (prefbox), filetype_combo, FALSE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (prefbox), jpeg_pref, FALSE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (prefbox), tiff_pref, FALSE, TRUE, 0);
	gtk_widget_show_all(prefbox);

	g_signal_connect ((gpointer) filetype_combo, "changed", G_CALLBACK (filetype_changed), NULL);
	filetype_changed(GTK_COMBO_BOX(filetype_combo), NULL);

	dirname = g_path_get_dirname(rs->photo->filename);
	basename = g_path_get_basename(rs->photo->filename);

	conf_export = rs_conf_get_string(CONF_DEFAULT_EXPORT_TEMPLATE);

	if (conf_export)
	{
		if (conf_export[0]==G_DIR_SEPARATOR)
		{
			g_free(dirname);
			dirname = conf_export;
		}
		else
		{
			export_path = g_string_new(dirname);
			g_string_append(export_path, G_DIR_SEPARATOR_S);
			g_string_append(export_path, conf_export);
			g_free(dirname);
			dirname = export_path->str;
			g_string_free(export_path, FALSE);
			g_free(conf_export);
		}
		g_mkdir_with_parents(dirname, 00755);
	}

	gui_status_push(_("Exporting file ..."));

	filetype_combo = gui_filetype_combobox();
	if (rs_conf_get_filetype(CONF_SAVE_FILETYPE, &last))
		gui_filetype_combobox_set_active(filetype_combo, last);
	else
		gtk_combo_box_set_active(GTK_COMBO_BOX(filetype_combo), 0);

	name = g_string_new(basename);
	g_string_append(name, ".");
	g_string_append(name, gui_filetype_combobox_get_ext(GTK_COMBO_BOX(filetype_combo)));

	gtk_dialog_set_default_response(GTK_DIALOG(fc), GTK_RESPONSE_ACCEPT);
#if GTK_CHECK_VERSION(2,8,0)
	gtk_file_chooser_set_do_overwrite_confirmation (GTK_FILE_CHOOSER (fc), TRUE);
#endif
	gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (fc), dirname);
	gtk_file_chooser_set_current_name (GTK_FILE_CHOOSER (fc), name->str);
	gtk_file_chooser_set_extra_widget(GTK_FILE_CHOOSER (fc), prefbox);
	if (gtk_dialog_run (GTK_DIALOG (fc)) == GTK_RESPONSE_ACCEPT)
	{
		char *filename;
		const RS_FILETYPE *filetype = gui_filetype_combobox_get_filetype(GTK_COMBO_BOX(filetype_combo));
		filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (fc));
		if (rs->cms_enabled)
			rs_photo_save(rs->photo, filename, filetype->filetype, rs->exportProfileFilename);
		else
			rs_photo_save(rs->photo, filename, filetype->filetype, NULL);

		rs_conf_set_filetype(CONF_SAVE_FILETYPE, filetype);

		gtk_widget_destroy(fc);
		g_free (filename);
		gui_status_push(_("File exported"));
	}
	else
	{
		gtk_widget_destroy(fc);
		gui_status_push(_("File export canceled"));
	}
	g_free(dirname);
	g_free(basename);
	g_string_free(name, TRUE);
	gui_set_busy(FALSE);
	return;
}
