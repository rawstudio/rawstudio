/*
 * Copyright (C) 2006, 2007 Anders Brander <anders@brander.dk> and 
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

RS_FILETYPE *gui_filetype_combobox_get_filetype(GtkComboBox *widget);
const gchar *gui_filetype_combobox_get_ext(GtkComboBox *widget);
GtkWidget *gui_filetype_combobox();
void gui_filetype_combobox_set_active(GtkWidget *combo, RS_FILETYPE *set);
GtkWidget *gui_filetype_preference(GtkWidget *filetype_combo);
void checkbox_set_conf(GtkToggleButton *togglebutton, gpointer user_data);
GtkWidget *checkbox_from_conf(const gchar *conf, gchar *label, gboolean default_value);
GtkWidget *gui_tooltip_no_window(GtkWidget *widget, gchar *tip_tip, gchar *tip_private);
void gui_tooltip_window(GtkWidget *widget, gchar *tip_tip, gchar *tip_private);
void gui_batch_directory_entry_changed(GtkEntry *entry, gpointer user_data);
void gui_batch_filename_entry_changed(GtkEntry *entry, gpointer user_data);
void gui_batch_filetype_entry_changed(GtkEntry *entry, gpointer user_data);
void gui_export_changed_helper(GtkLabel *label);
void gui_export_directory_entry_changed(GtkEntry *entry, gpointer user_data);
void gui_export_filename_entry_changed(GtkComboBox *combobox, gpointer user_data);
GtkWidget *gui_preferences_make_cms_page();
