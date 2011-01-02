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

#include <gtk/gtk.h>
#include <config.h>
#include "application.h"
#include "gettext.h"
#include "gtk-progress.h"
#include "gtk-interface.h"

struct _RS_PROGRESS {
	GtkWidget *window;
	GtkWidget *progressbar;
	GtkWidget *frame;
	GtkWidget *vbox;
	gint items;
	gint current;
	const gchar *title;
	gdouble delay;
	GTimer *lifetime;
};

static gboolean
gui_progress_destroy(GtkWidget *widget, GdkEvent *event, RS_PROGRESS *rsp)
{
	rsp->progressbar = NULL;
	return(TRUE);
}

/**
 * Initialize a new RS_PROGRESS
 * @return A new RS_PROGRESS
 */
static RS_PROGRESS *
gui_progress_init(const gchar *title)
{
	extern GtkWindow *rawstudio_window;
	GtkWidget *alignment;
	RS_PROGRESS *rsp;

	rsp = g_new(RS_PROGRESS, 1);
	rsp->progressbar = gtk_progress_bar_new();

	alignment = gtk_alignment_new (0.5, 0.5, 1, 1);
	gtk_alignment_set_padding (GTK_ALIGNMENT (alignment), 5, 5, 5, 5);

	rsp->frame = gtk_frame_new(_("Progress"));
	gtk_container_set_border_width (GTK_CONTAINER (rsp->frame), 5);

	rsp->vbox = gtk_vbox_new(FALSE, 2);

	rsp->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_resize (GTK_WINDOW(rsp->window), 400, 50);
	g_signal_connect(G_OBJECT(rsp->window), "delete_event", G_CALLBACK(gui_progress_destroy), rsp);
	gtk_widget_realize (rsp->window);
	gdk_window_set_type_hint(rsp->window->window, GDK_WINDOW_TYPE_HINT_UTILITY);
	gtk_window_set_position(GTK_WINDOW(rsp->window), GTK_WIN_POS_CENTER_ON_PARENT);
	gtk_window_set_title(GTK_WINDOW(rsp->window), title);
	gtk_window_set_transient_for(GTK_WINDOW (rsp->window), rawstudio_window);

	gtk_box_pack_start(GTK_BOX(rsp->vbox), rsp->frame, TRUE, TRUE, 0);

	gtk_container_add (GTK_CONTAINER (rsp->window), rsp->vbox);
	gtk_container_add (GTK_CONTAINER (rsp->frame), alignment);
	gtk_container_add (GTK_CONTAINER (alignment), rsp->progressbar);

	rsp->items = 1;
	rsp->current = 0;
	rsp->title = NULL;
	rsp->delay = -1.0f;
	rsp->lifetime = NULL;
	gui_progress_set_current(rsp, 0);
	return(rsp);
}

RS_PROGRESS *
gui_progress_new(const gchar *title, gint items)
{
	RS_PROGRESS *rsp = gui_progress_init(title);

	if (items==0) items = 1;

	if (title)
		gtk_frame_set_label(GTK_FRAME(rsp->frame), _("Progress"));
	rsp->items = items;
	rsp->current = 0;
	gui_progress_set_current(rsp, 0);
	gtk_widget_show_all(rsp->window);
	return(rsp);
}

/**
 * Shows a new progress bar with an initial delay, otherwise behaves like gui_progress_new()
 * @param title The title to use for the progress bar
 * @param items How many items must be processed
 * @param delay The delay in milliseconds
 * @return A new RS_PROGRESS
 */
RS_PROGRESS *
gui_progress_new_with_delay(const gchar *title, gint items, gint delay)
{
	RS_PROGRESS *rsp = gui_progress_init(title);

	if (items==0) items = 1;

	if (title)
		gtk_frame_set_label(GTK_FRAME(rsp->frame), _("Progress"));
	rsp->items = items;
	rsp->current = 0;
	rsp->delay = ((gdouble)delay)/1000.0f;
	rsp->lifetime = g_timer_new();
	gui_progress_set_current(rsp, 0);
	return(rsp);
}

void
gui_progress_free(RS_PROGRESS *rsp)
{
	extern GtkWindow *rawstudio_window;
	gtk_widget_destroy(rsp->window);
	/* Free the GTimer if needed */
	if (rsp->lifetime)
		g_timer_destroy(rsp->lifetime);
	g_free(rsp);
	gtk_window_present(rawstudio_window);
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

	/* Show the widget if we're past the initial delay */
	if ((rsp->delay>-1.0f) && (g_timer_elapsed(rsp->lifetime, NULL) > rsp->delay))
	{
		gtk_widget_show_all(rsp->window);
		gtk_window_present(GTK_WINDOW(rsp->window));
	}

	gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(rsp->progressbar),
		((gdouble)rsp->current)/((gdouble)rsp->items));

	gs = g_string_new(NULL);
	g_string_printf(gs, "%d/%d", rsp->current, rsp->items);
	gtk_progress_bar_set_text(GTK_PROGRESS_BAR(rsp->progressbar), gs->str);
	g_string_free(gs, TRUE);

	/* Lock if needed */
	if (g_main_depth() == 0)
	{
		gdk_threads_enter();
		GTK_CATCHUP();
		gdk_threads_leave();
	}
	else
		GTK_CATCHUP();
}

void
gui_progress_add_widget(RS_PROGRESS *rsp, GtkWidget *widget)
{
	GtkWidget *alignment;

	alignment = gtk_alignment_new (0.5, 0.5, 1, 1);
	gtk_alignment_set_padding (GTK_ALIGNMENT (alignment), 5, 5, 5, 5);
	gtk_container_add (GTK_CONTAINER (alignment), widget);

	gtk_box_pack_start(GTK_BOX(rsp->vbox), alignment, FALSE, FALSE, 0);
}
