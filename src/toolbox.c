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
#include "matrix.h"
#include "rs-batch.h"
#include "rawstudio.h"
#include "gtk-helper.h"
#include "gtk-interface.h"
#include "color.h"
#include "toolbox.h"
#include "conf_interface.h"
#include "gettext.h"

/* used for gui_adj_reset_callback() */
struct reset_carrier {
	RS_BLOB *rs;
	gint mask;
};

GtkLabel *infolabel;

GtkWidget *gui_hist(RS_BLOB *rs, const gchar *label);
GtkWidget *gui_box(const gchar *title, GtkWidget *in);
void gui_transform_rot90_clicked(GtkWidget *w, RS_BLOB *rs);
void gui_transform_rot180_clicked(GtkWidget *w, RS_BLOB *rs);
void gui_transform_rot270_clicked(GtkWidget *w, RS_BLOB *rs);
void gui_transform_mirror_clicked(GtkWidget *w, RS_BLOB *rs);
void gui_transform_flip_clicked(GtkWidget *w, RS_BLOB *rs);
GtkWidget *gui_transform(RS_BLOB *rs);
GtkWidget *gui_tool_warmth(RS_BLOB *rs, gint n);
GtkWidget *gui_slider(GtkObject *adj, const gchar *label);
gboolean gui_adj_reset_callback(GtkWidget *widget, GdkEventButton *event, struct reset_carrier *rc);
GtkWidget *gui_make_scale_from_adj(RS_BLOB *rs, GCallback cb, GtkObject *adj, gint mask);
GtkWidget *gui_tool_exposure(RS_BLOB *rs, gint n);
GtkWidget *gui_tool_saturation(RS_BLOB *rs, gint n);
GtkWidget *gui_tool_hue(RS_BLOB *rs, gint n);
GtkWidget *gui_tool_contrast(RS_BLOB *rs, gint n);
GtkWidget *gui_make_tools(RS_BLOB *rs, gint n);
void gui_notebook_callback(GtkNotebook *notebook, GtkNotebookPage *page, guint page_num, RS_BLOB *rs);

void
settings_changed(RS_BLOB *rs)
{
}

GtkWidget *
gui_hist(RS_BLOB *rs, const gchar *label)
{
	GdkPixbuf *pixbuf;
	gint height;
	guint rowstride;
	guchar *pixels;

	if (!rs_conf_get_integer(CONF_HISTHEIGHT, &height))
		height = 128;

	/* creates the pixbuf containing the histogram */
	pixbuf = gdk_pixbuf_new(GDK_COLORSPACE_RGB, FALSE, 8, 256, height);
	
	rowstride = gdk_pixbuf_get_rowstride (pixbuf);
	pixels = gdk_pixbuf_get_pixels (pixbuf);

	/* sets all the pixels black */
	memset(pixels, 0x00, rowstride*height);

	/* creates an image from the histogram pixbuf */
	rs->histogram_image = (GtkImage *) gtk_image_new_from_pixbuf(pixbuf);

	return(gui_box(label, (GtkWidget *)rs->histogram_image));
}

GtkWidget *
gui_box(const gchar *title, GtkWidget *in)
{
	GtkWidget *expander, *label;

	expander = gtk_expander_new (NULL);
	gtk_widget_show (expander);
	gtk_expander_set_expanded (GTK_EXPANDER (expander), TRUE);

	label = gtk_label_new (title);
	gtk_widget_show (label);
	gtk_expander_set_label_widget (GTK_EXPANDER (expander), label);
	gtk_label_set_use_markup (GTK_LABEL (label), TRUE);
	gtk_container_add (GTK_CONTAINER (expander), in);
	return(expander);
}

void
gui_transform_rot90_clicked(GtkWidget *w, RS_BLOB *rs)
{
	ORIENTATION_90(rs->photo->orientation);
	update_preview(rs);
}

void
gui_transform_rot180_clicked(GtkWidget *w, RS_BLOB *rs)
{
	ORIENTATION_180(rs->photo->orientation);
	update_preview(rs);
}

void
gui_transform_rot270_clicked(GtkWidget *w, RS_BLOB *rs)
{
	ORIENTATION_270(rs->photo->orientation);
	update_preview(rs);
}

void
gui_transform_mirror_clicked(GtkWidget *w, RS_BLOB *rs)
{
	ORIENTATION_MIRROR(rs->photo->orientation);
	update_preview(rs);
}

void
gui_transform_flip_clicked(GtkWidget *w, RS_BLOB *rs)
{
	ORIENTATION_FLIP(rs->photo->orientation);
	update_preview(rs);
}

GtkWidget *
gui_transform(RS_BLOB *rs)
{
	GtkWidget *hbox;
	GtkWidget *flip;
	GtkWidget *mirror;
	GtkWidget *rot90;
	GtkWidget *rot180;
	GtkWidget *rot270;

	hbox = gtk_hbox_new(TRUE, 0);
	flip = gtk_button_new_with_mnemonic (_("Flip"));
	mirror = gtk_button_new_with_mnemonic (_("Mirror"));
	rot90 = gtk_button_new_with_mnemonic (_("CW"));
	rot180 = gtk_button_new_with_mnemonic (_("180"));
	rot270 = gtk_button_new_with_mnemonic (_("CCW"));
	gui_tooltip_window(flip, _("Flip the photo over the x-axis"), NULL);
	gui_tooltip_window(mirror, _("Mirror the photo over the y-axis"), NULL);
	gui_tooltip_window(rot90, _("Rotate the photo 90 degrees clockwise"), NULL);
	gui_tooltip_window(rot180, _("Rotate the photo 180 degrees"), NULL);
	gui_tooltip_window(rot270, _("Rotate the photo 90 degrees counter clockwise"), NULL);
	g_signal_connect ((gpointer) flip, "clicked", G_CALLBACK (gui_transform_flip_clicked), rs);
	g_signal_connect ((gpointer) mirror, "clicked", G_CALLBACK (gui_transform_mirror_clicked), rs);
	g_signal_connect ((gpointer) rot90, "clicked", G_CALLBACK (gui_transform_rot90_clicked), rs);
	g_signal_connect ((gpointer) rot180, "clicked", G_CALLBACK (gui_transform_rot180_clicked), rs);
	g_signal_connect ((gpointer) rot270, "clicked", G_CALLBACK (gui_transform_rot270_clicked), rs);
	gtk_widget_show (flip);
	gtk_widget_show (mirror);
	gtk_widget_show (rot90);
	gtk_widget_show (rot180);
	gtk_widget_show (rot270);
	gtk_box_pack_start(GTK_BOX (hbox), flip, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX (hbox), mirror, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX (hbox), rot270, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX (hbox), rot180, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX (hbox), rot90, FALSE, FALSE, 0);
	return(gui_box(_("Transforms"), hbox));
}

GtkWidget *
gui_tool_warmth(RS_BLOB *rs, gint n)
{
	GtkWidget *box;
	GtkWidget *wscale;
	GtkWidget *tscale;

	wscale = gui_make_scale_from_adj(rs, G_CALLBACK(update_preview_callback), rs->photo->settings[n]->warmth, MASK_WARMTH);
	tscale = gui_make_scale_from_adj(rs, G_CALLBACK(update_preview_callback), rs->photo->settings[n]->tint, MASK_TINT);

	box = gtk_vbox_new (FALSE, 0);
	gtk_box_pack_start (GTK_BOX (box), wscale, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (box), tscale, FALSE, FALSE, 0);
	return(gui_box(_("Warmth/tint"), box));
}

GtkWidget *
gui_slider(GtkObject *adj, const gchar *label)
{
	GtkWidget *hscale;
	hscale = gtk_hscale_new (GTK_ADJUSTMENT (adj));
	gtk_scale_set_value_pos( GTK_SCALE(hscale), GTK_POS_LEFT);
	gtk_scale_set_digits(GTK_SCALE(hscale), 2);
	return(gui_box(label, hscale));
}

gboolean
gui_adj_reset_callback(GtkWidget *widget, GdkEventButton *event, struct reset_carrier *rc)
{
	rs_settings_reset(rc->rs->photo->settings[rc->rs->photo->current_setting], rc->mask);
	return(TRUE);
}

GtkWidget *
gui_make_scale_from_adj(RS_BLOB *rs, GCallback cb, GtkObject *adj, gint mask)
{
	GtkWidget *hscale, *box, *rimage, *revent;
	struct reset_carrier *rc = malloc(sizeof(struct reset_carrier));
	rc->rs = rs;
	rc->mask = mask;

	box = gtk_hbox_new(FALSE, 0);

	hscale = gtk_hscale_new((GtkAdjustment *) adj);
	g_signal_connect(adj, "value_changed", cb, rs);
	gtk_scale_set_value_pos( GTK_SCALE(hscale), GTK_POS_LEFT);
	gtk_scale_set_digits(GTK_SCALE(hscale), 2);

	rimage = gtk_image_new_from_stock(GTK_STOCK_REFRESH, GTK_ICON_SIZE_MENU);

	revent = gtk_event_box_new();
	gui_tooltip_window(revent, _("Reset this setting"), NULL);
	gtk_container_add (GTK_CONTAINER (revent), rimage);

	gtk_widget_set_events(revent, GDK_BUTTON_PRESS_MASK);
	g_signal_connect ((gpointer) revent, "button_press_event",
		G_CALLBACK (gui_adj_reset_callback), rc);

	gtk_box_pack_start (GTK_BOX (box), hscale, TRUE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (box), revent, FALSE, TRUE, 0);

	return(box);
}

GtkWidget *
gui_tool_exposure(RS_BLOB *rs, gint n)
{
	GtkWidget *hscale;

	hscale = gui_make_scale_from_adj(rs, G_CALLBACK(update_preview_callback), rs->photo->settings[n]->exposure, MASK_EXPOSURE);
	return(gui_box(_("Exposure"), hscale));
}

GtkWidget *
gui_tool_saturation(RS_BLOB *rs, gint n)
{
	GtkWidget *hscale;

	hscale = gui_make_scale_from_adj(rs, G_CALLBACK(update_preview_callback), rs->photo->settings[n]->saturation, MASK_SATURATION);
	return(gui_box(_("Saturation"), hscale));
}

GtkWidget *
gui_tool_hue(RS_BLOB *rs, gint n)
{
	GtkWidget *hscale;

	hscale = gui_make_scale_from_adj(rs, G_CALLBACK(update_preview_callback), rs->photo->settings[n]->hue, MASK_HUE);
	return(gui_box(_("Hue"), hscale));
}

GtkWidget *
gui_tool_contrast(RS_BLOB *rs, gint n)
{
	GtkWidget *hscale;

	hscale = gui_make_scale_from_adj(rs, G_CALLBACK(update_preview_callback), rs->photo->settings[n]->contrast, MASK_CONTRAST);
	return(gui_box(_("Contrast"), hscale));
}

GtkWidget *
gui_make_tools(RS_BLOB *rs, gint n)
{
	GtkWidget *toolbox;

	toolbox = gtk_vbox_new (FALSE, 0);
	gtk_widget_show (toolbox);
	gtk_box_pack_start (GTK_BOX (toolbox), gui_tool_exposure(rs, n), FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (toolbox), gui_tool_saturation(rs, n), FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (toolbox), gui_tool_hue(rs, n), FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (toolbox), gui_tool_contrast(rs, n), FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (toolbox), gui_tool_warmth(rs, n), FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (toolbox), gui_transform(rs), FALSE, FALSE, 0);
	return(toolbox);
}

void
gui_notebook_callback(GtkNotebook *notebook, GtkNotebookPage *page, guint page_num, RS_BLOB *rs)
{
	rs->photo->current_setting = page_num;
	update_preview(rs);
}

GtkWidget *
make_toolbox(RS_BLOB *rs)
{
	GtkWidget *toolbox;
	GtkWidget *notebook;
	GtkWidget *label1;
	GtkWidget *label2;
	GtkWidget *label3;
	GtkWidget *toolboxscroller;
	GtkWidget *toolboxviewport;

	label1 = gtk_label_new(_(" A "));
	label2 = gtk_label_new(_(" B "));
	label3 = gtk_label_new(_(" C "));

	notebook = gtk_notebook_new();
	gtk_notebook_append_page(GTK_NOTEBOOK(notebook), gui_make_tools(rs, 0), label1);
	gtk_notebook_append_page(GTK_NOTEBOOK(notebook), gui_make_tools(rs, 1), label2);
	gtk_notebook_append_page(GTK_NOTEBOOK(notebook), gui_make_tools(rs, 2), label3);
	g_signal_connect(notebook, "switch-page", G_CALLBACK(gui_notebook_callback), rs);

	toolbox = gtk_vbox_new (FALSE, 0);
	gtk_box_pack_start (GTK_BOX (toolbox), notebook, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (toolbox), gui_slider(rs->scale, _("Scale")), FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (toolbox), gui_hist(rs, _("Histogram")), FALSE, FALSE, 0);

	infolabel = (GtkLabel *) gtk_label_new_with_mnemonic("");
	gtk_box_pack_start (GTK_BOX (toolbox), (GtkWidget *) infolabel, FALSE, FALSE, 0);

	toolboxscroller = gtk_scrolled_window_new (NULL, NULL);
	toolboxviewport = gtk_viewport_new (NULL, NULL);
	gtk_container_add (GTK_CONTAINER (toolboxscroller), toolboxviewport);
	gtk_container_add (GTK_CONTAINER (toolboxviewport), toolbox);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (toolboxscroller),
		GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);

	return(toolboxscroller);
}
