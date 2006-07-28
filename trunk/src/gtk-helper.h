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

GtkWidget *gui_tooltip_no_window(GtkWidget *widget, gchar *tip_tip, gchar *tip_private);
void *gui_tooltip_window(GtkWidget *widget, gchar *tip_tip, gchar *tip_private);
gboolean gui_save_png(GdkPixbuf *pixbuf, gchar *filename);
gboolean gui_save_jpg(GdkPixbuf *pixbuf, gchar *filename);
void gui_batch_directory_entry_changed(GtkEntry *entry, gpointer user_data);
void gui_batch_filename_entry_changed(GtkEntry *entry, gpointer user_data);
void gui_batch_filetype_entry_changed(GtkEntry *entry, gpointer user_data);
void gui_export_directory_entry_changed(GtkEntry *entry, gpointer user_data);
void gui_export_filename_entry_changed(GtkEntry *entry, gpointer user_data);
