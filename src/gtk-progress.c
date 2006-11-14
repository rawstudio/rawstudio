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
#include <string.h> /* memset() */
#include "gtk-progress.h"

RS_PROGRESS *
gui_progress_new(const gchar *title, gint items)
{
	extern GtkWidget *hbox;
	RS_PROGRESS *rsp;
	if (items==0) items = 1;
	rsp = g_new(RS_PROGRESS, 1);
	rsp->progressbar = gtk_progress_bar_new();
	rsp->items = items;
	rsp->current = 0;
	rsp->title = title;
	if (rsp->title)
		gtk_progress_bar_set_text(GTK_PROGRESS_BAR(rsp->progressbar), title);
	gtk_box_pack_end(GTK_BOX (hbox), rsp->progressbar, FALSE, TRUE, 0);
	gtk_widget_show_all(rsp->progressbar);
	return(rsp);
}

void
gui_progress_free(RS_PROGRESS *rsp)
{
	gtk_widget_destroy(rsp->progressbar);
	g_free(rsp);
}

void
gui_progress_advance_one(RS_PROGRESS *rsp)
{
	rsp->current++;
	gui_progress_set_current(rsp, rsp->current);
}

void
gui_progress_set_current(RS_PROGRESS *rsp, gint current)
{
	GString *gs;
	rsp->current = current;
	gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(rsp->progressbar),
		((gdouble)rsp->current)/((gdouble)rsp->items));
	if (!rsp->title)
	{
		gs = g_string_new(NULL);
		g_string_printf(gs, "%d/%d", rsp->current, rsp->items);
		gtk_progress_bar_set_text(GTK_PROGRESS_BAR(rsp->progressbar), gs->str);
		g_string_free(gs, TRUE);
	}
	while (gtk_events_pending())
		gtk_main_iteration();
}
