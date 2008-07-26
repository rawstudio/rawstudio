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

#ifndef GTK_INTERFACE_H
#define GTK_INTERFACE_H
#include <gdk/gdkx.h>

enum {
	OP_NONE = 0,
	OP_BUSY,
	OP_MOVE
};

#define GUI_CATCHUP() do { \
  GdkDisplay *__gui_catchup_display = gdk_display_get_default (); \
  XFlush (GDK_DISPLAY_XDISPLAY (__gui_catchup_display)); } while (0)
#define GTK_CATCHUP() while (gtk_events_pending()) gtk_main_iteration()

extern void gui_set_busy(gboolean rawstudio_is_busy);
extern gboolean gui_is_busy();
extern void gui_status_notify(const char *text);
extern guint gui_status_push(const char *text);
extern void gui_status_pop(const guint msgid);
extern gboolean update_preview_callback(GtkAdjustment *caller, RS_BLOB *rs);
extern gboolean update_scale_callback(GtkAdjustment *do_not_use_this, RS_BLOB *rs);
extern void icon_set_flags(const gchar *filename, GtkTreeIter *iter, const guint *priority, const gboolean *exported);
extern void gui_dialog_simple(gchar *title, gchar *message);
extern GtkUIManager *gui_get_uimanager();
extern void gui_set_values(RS_BLOB *rs, gint x, gint y);
extern int gui_init(int argc, char **argv, RS_BLOB *rs);
extern void gui_setprio(RS_BLOB *rs, guint prio);
extern void gui_widget_show(GtkWidget *widget, gboolean show, const gchar *conf_fullscreen_key, const gchar *conf_windowed_key);
extern void gui_make_preference_window(RS_BLOB *rs);

extern GtkWindow *rawstudio_window;
extern GdkGC *dashed;
extern GdkGC *grid;

#endif /* GTK_INTERFACE_H */
