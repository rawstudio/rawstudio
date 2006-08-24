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

enum { 
	PIXBUF_COLUMN,
	TEXT_COLUMN,
	DATA_COLUMN,
	FULLNAME_COLUMN,
	PRIORITY_COLUMN,
	NUM_COLUMNS
};

enum {
	PRIO_U = 0,
	PRIO_D = 51,
	PRIO_1 = 1,
	PRIO_2 = 2,
	PRIO_3 = 3,
	PRIO_ALL = 255
};

enum {
	OP_NONE = 0,
	OP_MOVE
};

#define GUI_CATCHUP() while (gtk_events_pending()) gtk_main_iteration()

void update_histogram(RS_BLOB *rs);
gboolean update_preview_callback(GtkAdjustment *caller, RS_BLOB *rs);
gboolean update_previewtable_callback(GtkAdjustment *do_not_use_this, RS_BLOB *rs);
gboolean update_scale_callback(GtkAdjustment *do_not_use_this, RS_BLOB *rs);
void gui_dialog_simple(gchar *title, gchar *message);
GtkWidget *gui_dialog_make_from_text(const gchar *stock_id, gchar *primary_text, gchar *secondary_text);
int gui_init(int argc, char **argv, RS_BLOB *rs);
