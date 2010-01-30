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

typedef struct _RS_PROGRESS RS_PROGRESS; /* Defined in gtk-progress.c */

RS_PROGRESS *gui_progress_new(const gchar *title, gint items);

/**
 * Shows a new progress bar with an initial delay, otherwise behaves like gui_progress_new()
 * @param title The title to use for the progress bar
 * @param items How many items must be processed
 * @param delay The delay in milliseconds
 * @return A new RS_PROGRESS
 */
RS_PROGRESS *
gui_progress_new_with_delay(const gchar *title, gint items, gint delay);

void gui_progress_free(RS_PROGRESS *rsp);
void gui_progress_advance_one(RS_PROGRESS *rsp);
void gui_progress_set_current(RS_PROGRESS *rsp, gint current);
void gui_progress_add_widget(RS_PROGRESS *rsp, GtkWidget *widget);
