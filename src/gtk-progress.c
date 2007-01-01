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

#include <gtk/gtk.h>
#include <config.h>
#include "gettext.h"
#include "gtk-progress.h"

static gboolean
gui_progress_destroy(GtkWidget *widget, GdkEvent *event, RS_PROGRESS *rsp)
{
	rsp->progressbar = NULL;
	return(TRUE);
}

RS_PROGRESS *
gui_progress_new(const gchar *title, gint items)
{
	extern GtkWindow *rawstudio_window;
	GtkWidget *frame;
	GtkWidget *alignment;
	RS_PROGRESS *rsp;
	if (items==0) items = 1;
	rsp = g_new(RS_PROGRESS, 1);
	rsp->progressbar = gtk_progress_bar_new();

	alignment = gtk_alignment_new (0.5, 0.5, 1, 1);
	gtk_alignment_set_padding (GTK_ALIGNMENT (alignment), 5, 5, 5, 5);

	if (title)
		frame = gtk_frame_new(title);
	else
		frame = gtk_frame_new(_("Progress"));
	gtk_container_set_border_width (GTK_CONTAINER (frame), 5);

	rsp->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	g_signal_connect((gpointer) rsp->window, "delete_event", G_CALLBACK(gui_progress_destroy), rsp);
	gtk_window_set_resizable(GTK_WINDOW(rsp->window), FALSE);
	gtk_window_set_decorated(GTK_WINDOW(rsp->window), FALSE);
	gtk_window_set_position(GTK_WINDOW(rsp->window), GTK_WIN_POS_CENTER_ON_PARENT);
	gtk_window_set_title(GTK_WINDOW(rsp->window), _("Progress"));
	gtk_window_set_transient_for(GTK_WINDOW (rsp->window), rawstudio_window);

	gtk_container_add (GTK_CONTAINER (rsp->window), frame);
	gtk_container_add (GTK_CONTAINER (frame), alignment);
	gtk_container_add (GTK_CONTAINER (alignment), rsp->progressbar);

	rsp->items = items;
	rsp->current = 0;
	gui_progress_set_current(rsp, 0);
	gtk_widget_show_all(rsp->window);
	return(rsp);
}

void
gui_progress_free(RS_PROGRESS *rsp)
{
	gtk_widget_destroy(rsp->window);
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
	if (!rsp->progressbar) return;
	gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(rsp->progressbar),
		((gdouble)rsp->current)/((gdouble)rsp->items));

	gs = g_string_new(NULL);
	g_string_printf(gs, "%d/%d", rsp->current, rsp->items);
	gtk_progress_bar_set_text(GTK_PROGRESS_BAR(rsp->progressbar), gs->str);
	g_string_free(gs, TRUE);

	while (gtk_events_pending())
		gtk_main_iteration();
}
