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

#include <gconf/gconf-client.h>

typedef struct _RS_CONFBOX RS_CONFBOX;

#define gui_label_set_text_printf(label, format, ...) do { \
	gchar *__new_text = g_strdup_printf(format, __VA_ARGS__); \
	gtk_label_set_text(label, __new_text); \
	g_free(__new_text); \
} while (0)

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
extern void gui_batch_directory_entry_changed(GtkEntry *entry, gpointer user_data);
extern void gui_batch_filename_entry_changed(GtkEditable *editable, gpointer user_data);
extern void gui_batch_filetype_combobox_changed(gpointer active, gpointer user_data);
extern gboolean window_key_press_event(GtkWidget *widget, GdkEventKey *event);
extern void pos_menu_below_widget(GtkMenu *menu, gint *x, gint *y, gboolean *push_in, gpointer user_data);
extern GtkWidget *gui_framed(GtkWidget *widget, const gchar *title, GtkShadowType shadowtype);
extern GtkWidget *gui_aligned(GtkWidget *widget, const gfloat xalign, const gfloat yalign, const gfloat xscale, const gfloat yscale);
extern GdkPixbuf *cairo_convert_to_pixbuf (cairo_surface_t *surface);
extern void gui_set_block_keyboard(gboolean block_keyboard);

/**
 * Build and show a popup-menu
 * @param widget A widget to pop up below or NULL to pop upat mouse pointer
 * @param user_data Pointer to pass to callback
 * @param ... Pairs of gchar labels and callbaks, terminated by -1
 * @return The newly created menu
 */
GtkWidget *gui_menu_popup(GtkWidget *widget, gpointer user_data, ...);

/**
 * Creates a new GtkButton widget.
 * @param stock_id A stock id registered with GTK+
 * @param label The text to show besides the icon
 * @return a new GtkButton
 */
extern GtkWidget *gui_button_new_from_stock_with_label(const gchar *stock_id, const gchar *label);

/**
 * This will create a new GtkLabel that can alternate text when the pointer is
 * hovering above it.
 * @param normal_text The text to display when pointer is not hovering above
 * @param hover_text The text to display when pointer is hovering above the label
 * @return A new GtkLabel
 */
extern GtkWidget *gui_label_new_with_mouseover(const gchar *normal_text, const gchar *hover_text);

extern void gui_box_toggle_callback(GtkExpander *expander, gchar *key);

extern void gui_box_notify(GConfClient *client, guint cnxn_id, GConfEntry *entry, gpointer user_data);

extern GtkWidget * gui_box(const gchar *title, GtkWidget *in, gchar *key, gboolean default_expanded);

extern RSColorSpace* rs_get_display_profile(GtkWidget *widget);

extern GtkWidget * rs_combobox_new(const gchar *text, GtkListStore *store, const gchar *conf_key);
extern GtkWidget * rs_spinbox_new(const gchar *text, const gchar *conf_key, gint conf_key_default, gdouble lower, gdouble upper, gdouble step, gdouble page);
