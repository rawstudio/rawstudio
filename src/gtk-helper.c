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
#include "color.h"
#include "matrix.h"
#include "rs-batch.h"
#include "rawstudio.h"
#include "conf_interface.h"
#include "filename.h"

GtkWidget *gui_tooltip_no_window(GtkWidget *widget, gchar *tip_tip, gchar *tip_private)
{
	GtkWidget *e;
	GtkTooltips *tip;

	tip = gtk_tooltips_new();
	e = gtk_event_box_new();
	gtk_tooltips_set_tip(tip, e, tip_tip, tip_private);
	gtk_widget_show(widget);
	gtk_container_add(GTK_CONTAINER(e), widget);

	return e;
}

void gui_tooltip_window(GtkWidget *widget, gchar *tip_tip, gchar *tip_private)
{
	GtkTooltips *tip;

	tip = gtk_tooltips_new();
	gtk_tooltips_set_tip(tip, widget, tip_tip, tip_private);
	gtk_widget_show(widget);

	return;
}

gboolean
gui_save_png(GdkPixbuf *pixbuf, gchar *filename)
{
	return gdk_pixbuf_save(pixbuf, filename, "png", NULL, NULL);
}

gboolean
gui_save_jpg(GdkPixbuf *pixbuf, gchar *filename)
{
	gboolean ret;
	gchar *quality = rs_conf_get_string(CONF_EXPORT_JPEG_QUALITY);
	if (!quality)
	{
		rs_conf_set_string(CONF_EXPORT_JPEG_QUALITY, DEFAULT_CONF_EXPORT_JPEG_QUALITY);
		quality = rs_conf_get_string(CONF_EXPORT_JPEG_QUALITY);
	}
	ret = gdk_pixbuf_save(pixbuf, filename, "jpeg", NULL, "quality", quality, NULL);
	g_free(quality);
	return ret;
}

void
gui_batch_directory_entry_changed(GtkEntry *entry, gpointer user_data)
{
	rs_conf_set_string(CONF_BATCH_DIRECTORY, gtk_entry_get_text(entry));
	return;
}

void
gui_batch_filename_entry_changed(GtkEntry *entry, gpointer user_data)
{
	rs_conf_set_string(CONF_BATCH_FILENAME, gtk_entry_get_text(entry));
	return;
}

void
gui_batch_filetype_entry_changed(GtkEntry *entry, gpointer user_data)
{
	rs_conf_set_string(CONF_BATCH_FILETYPE, gtk_entry_get_text(entry));
	return;
}

void
gui_export_changed_helper(GtkLabel *label)
{
	gchar *parsed = NULL;
	gchar *directory;
	gchar *filename;
	gchar *filetype;
	GString *final;

	directory = rs_conf_get_string(CONF_EXPORT_DIRECTORY);
	filename = rs_conf_get_string(CONF_EXPORT_FILENAME);
	filetype = rs_conf_get_string(CONF_EXPORT_FILETYPE);

	parsed = filename_parse(filename, NULL);

	final = g_string_new("<small>");
	g_string_append(final, directory);
	g_free(directory);
	g_string_append(final, parsed);
	g_free(parsed);
	g_string_append(final, ".");
	g_string_append(final, filetype);
	g_free(filetype);
	g_string_append(final, "</small>");

	gtk_label_set_markup(label, final->str);

	g_string_free(final, TRUE);

	return;
}

void
gui_export_directory_entry_changed(GtkEntry *entry, gpointer user_data)
{
	GtkLabel *label = GTK_LABEL(user_data);
	rs_conf_set_string(CONF_EXPORT_DIRECTORY, gtk_entry_get_text(entry));

	gui_export_changed_helper(label);

	return;
}

void
gui_export_filename_entry_changed(GtkEntry *entry, gpointer user_data)
{
	GtkLabel *label = GTK_LABEL(user_data);
	rs_conf_set_string(CONF_EXPORT_FILENAME, gtk_entry_get_text(entry));

	gui_export_changed_helper(label);

	return;
}
