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

typedef struct _rs_progress {
	GtkWidget *progresswindow;
	GtkWidget *progressbar;
	gint items;
	gint current;
	const gchar *title;
} RS_PROGRESS;

RS_PROGRESS *gui_progress_new(const gchar *title, gint items);
void gui_progress_free(RS_PROGRESS *rsp);
void gui_progress_advance_one(RS_PROGRESS *rsp);
void gui_progress_set_current(RS_PROGRESS *rsp, gint current);
