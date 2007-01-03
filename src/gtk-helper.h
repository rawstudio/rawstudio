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

typedef struct _rs_confbox
{
	GtkWidget *widget;
	GtkListStore *model;
	const gchar *conf_key;
	gpointer user_data;
	void (*callback)(gpointer active, gpointer user_data);
} RS_CONFBOX;

extern gpointer gui_confbox_get_active(RS_CONFBOX *confbox);
extern void gui_confbox_add_entry(RS_CONFBOX *confbox, const gchar *conf_id, const gchar *text, gpointer *user_data);
extern void gui_confbox_load_conf(RS_CONFBOX *confbox, gchar *default_value);
extern void gui_confbox_set_callback(RS_CONFBOX *confbox, gpointer user_data, void (*callback)(gpointer active, gpointer user_data));
extern RS_CONFBOX *gui_confbox_new(const gchar *conf_key);
extern void gui_confbox_destroy(RS_CONFBOX *confbox);
extern GtkWidget *gui_confbox_get_widget(RS_CONFBOX *confbox);
extern RS_CONFBOX *gui_confbox_filetype_new(const gchar *conf_key);
extern void checkbox_set_conf(GtkToggleButton *togglebutton, gpointer user_data);
extern GtkWidget *checkbox_from_conf(const gchar *conf, gchar *label, gboolean default_value);
extern GtkWidget *gui_tooltip_no_window(GtkWidget *widget, gchar *tip_tip, gchar *tip_private);
extern void gui_tooltip_window(GtkWidget *widget, gchar *tip_tip, gchar *tip_private);
extern void gui_batch_directory_entry_changed(GtkEntry *entry, gpointer user_data);
extern void gui_batch_filename_entry_changed(GtkEntry *entry, gpointer user_data);
extern void gui_batch_filetype_entry_changed(GtkEntry *entry, gpointer user_data);
extern void gui_export_changed_helper(GtkLabel *label);
extern void gui_export_directory_entry_changed(GtkEntry *entry, gpointer user_data);
extern void gui_export_filename_entry_changed(GtkComboBox *combobox, gpointer user_data);
extern GtkWidget *gui_preferences_make_cms_page();
