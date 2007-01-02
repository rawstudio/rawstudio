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
#include <string.h> /* memset() */
#include "rawstudio.h"
#include "gtk-helper.h"
#include "gtk-interface.h"
#include "toolbox.h"
#include "conf_interface.h"
#include "gettext.h"

/* used for gui_adj_reset_callback() */
struct reset_carrier {
	RS_BLOB *rs;
	gint mask;
};

GtkLabel *infolabel;
static GtkWidget *scale;
static GtkWidget *toolbox;

static GtkWidget *gui_hist(RS_BLOB *rs, const gchar *label);
static GtkWidget *gui_box(const gchar *title, GtkWidget *in, gboolean expanded);
static void gui_transform_rot90_clicked(GtkWidget *w, RS_BLOB *rs);
static void gui_transform_rot180_clicked(GtkWidget *w, RS_BLOB *rs);
static void gui_transform_rot270_clicked(GtkWidget *w, RS_BLOB *rs);
static void gui_transform_mirror_clicked(GtkWidget *w, RS_BLOB *rs);
static void gui_transform_flip_clicked(GtkWidget *w, RS_BLOB *rs);
static GtkWidget *gui_transform(RS_BLOB *rs);
static GtkWidget *gui_tool_warmth(RS_BLOB *rs, gint n);
static GtkWidget *gui_slider(GtkObject *adj, const gchar *label, gboolean expanded);
static gboolean gui_adj_reset_callback(GtkWidget *widget, GdkEventButton *event, struct reset_carrier *rc);
static GtkWidget *gui_make_scale_from_adj(RS_BLOB *rs, GCallback cb, GtkObject *adj, gint mask);
static GtkWidget *gui_make_tools(RS_BLOB *rs, gint n);
static void gui_notebook_callback(GtkNotebook *notebook, GtkNotebookPage *page, guint page_num, RS_BLOB *rs);
static void scale_expand_callback(GObject *object, GParamSpec *param_spec, gpointer user_data);

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

	return(gui_box(label, (GtkWidget *)rs->histogram_image, TRUE));
}

GtkWidget *
gui_box(const gchar *title, GtkWidget *in, gboolean expanded)
{
	GtkWidget *expander, *label;

	expander = gtk_expander_new (NULL);
	gtk_widget_show (expander);
	gtk_expander_set_expanded (GTK_EXPANDER (expander), expanded);

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
	if (!rs->photo) return;
	rs_photo_rotate(rs->photo, 1, 0.0);
	update_preview(rs, FALSE, TRUE);
}

void
gui_transform_rot180_clicked(GtkWidget *w, RS_BLOB *rs)
{
	if (!rs->photo) return;
	rs_photo_rotate(rs->photo, 2, 0.0);
	update_preview(rs, FALSE, TRUE);
}

void
gui_transform_rot270_clicked(GtkWidget *w, RS_BLOB *rs)
{
	if (!rs->photo) return;
	rs_photo_rotate(rs->photo, 3, 0.0);
	update_preview(rs, FALSE, TRUE);
}

void
gui_transform_mirror_clicked(GtkWidget *w, RS_BLOB *rs)
{
	if (!rs->photo) return;
	rs_photo_mirror(rs->photo);
	update_preview(rs, FALSE, TRUE);
}

void
gui_transform_flip_clicked(GtkWidget *w, RS_BLOB *rs)
{
	if (!rs->photo) return;
	rs_photo_flip(rs->photo);
	update_preview(rs, FALSE, TRUE);
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
	return(gui_box(_("Transforms"), hbox, TRUE));
}

GtkWidget *
gui_tool_warmth(RS_BLOB *rs, gint n)
{
	GtkWidget *box;
	GtkWidget *wscale;
	GtkWidget *tscale;

	wscale = gui_make_scale_from_adj(rs, G_CALLBACK(update_preview_callback), rs->settings[n]->warmth, MASK_WARMTH);
	tscale = gui_make_scale_from_adj(rs, G_CALLBACK(update_preview_callback), rs->settings[n]->tint, MASK_TINT);

	box = gtk_vbox_new (FALSE, 0);
	gtk_box_pack_start (GTK_BOX (box), wscale, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (box), tscale, FALSE, FALSE, 0);
	return(gui_box(_("Warmth/tint"), box, TRUE));
}

GtkWidget *
gui_slider(GtkObject *adj, const gchar *label, gboolean expanded)
{
	GtkWidget *hscale;
	hscale = gtk_hscale_new (GTK_ADJUSTMENT (adj));
	gtk_scale_set_value_pos( GTK_SCALE(hscale), GTK_POS_LEFT);
	gtk_scale_set_digits(GTK_SCALE(hscale), 2);
	return(gui_box(label, hscale, expanded));
}

gboolean
gui_adj_reset_callback(GtkWidget *widget, GdkEventButton *event, struct reset_carrier *rc)
{
	rs_settings_reset(rc->rs->settings[rc->rs->current_setting], rc->mask);
	return(TRUE);
}

GtkWidget *
gui_make_scale_from_adj(RS_BLOB *rs, GCallback cb, GtkObject *adj, gint mask)
{
	GtkWidget *hscale, *box, *rimage, *revent;
	struct reset_carrier *rc = g_malloc(sizeof(struct reset_carrier));
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
gui_make_tools(RS_BLOB *rs, gint n)
{
	GtkWidget *toolbox;

	toolbox = gtk_vbox_new (FALSE, 0);
	gtk_widget_show (toolbox);
	gtk_box_pack_start (GTK_BOX (toolbox), gui_box(_("Exposure"),
		gui_make_scale_from_adj(rs, G_CALLBACK(update_preview_callback),
		rs->settings[n]->exposure, MASK_EXPOSURE), TRUE), FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (toolbox), gui_box(_("Saturation"),
		gui_make_scale_from_adj(rs, G_CALLBACK(update_preview_callback),
		rs->settings[n]->saturation, MASK_SATURATION), TRUE), FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (toolbox), gui_box(_("Hue"),
		gui_make_scale_from_adj(rs, G_CALLBACK(update_preview_callback),
		rs->settings[n]->hue, MASK_HUE), TRUE), FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (toolbox), gui_box(_("Contrast"),
		gui_make_scale_from_adj(rs, G_CALLBACK(update_previewtable_callback),
		rs->settings[n]->contrast, MASK_CONTRAST), TRUE), FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (toolbox), gui_tool_warmth(rs, n), FALSE, FALSE, 0);
	return(toolbox);
}

void
gui_notebook_callback(GtkNotebook *notebook, GtkNotebookPage *page, guint page_num, RS_BLOB *rs)
{
	rs->current_setting = page_num;
	if (rs->photo) {
		rs->photo->current_setting = rs->current_setting;
		update_previewtable_callback(NULL, rs);
	}
}

static void
scale_expand_callback(GObject *object, GParamSpec *param_spec, gpointer user_data)
{
	RS_BLOB *rs = (RS_BLOB *) user_data;

	if(gtk_expander_get_expanded (GTK_EXPANDER (object)))
	{
		rs->zoom_to_fit = FALSE;
		update_preview(rs, FALSE, TRUE);
	}
	else
		rs_zoom_to_fit(rs);
	return;
}

void
scale_expand_set(gboolean expanded)
{
	gtk_expander_set_expanded (GTK_EXPANDER (scale), expanded);
	return;
}

void
gui_toolbox_add_widget(GtkWidget *widget)
{
	gtk_box_pack_start (GTK_BOX (toolbox), widget, FALSE, FALSE, 0);
	gtk_widget_show_all(widget);
	return;
}

GtkWidget *
gui_toolbox_add_tool_frame(GtkWidget *widget, gchar *title)
{
	GtkWidget *frame;
	frame = gtk_frame_new(title);

	gtk_container_set_border_width(GTK_CONTAINER(frame), 4);
	gtk_container_add(GTK_CONTAINER(frame), widget);

	gui_toolbox_add_widget(frame);
	return frame;
}

GtkWidget *
make_toolbox(RS_BLOB *rs)
{
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

	scale = gui_slider(rs->scale, _("Scale"), FALSE);
	g_signal_connect(scale, "notify::expanded", G_CALLBACK (scale_expand_callback), rs);

	toolbox = gtk_vbox_new (FALSE, 0);
	gtk_box_pack_start (GTK_BOX (toolbox), notebook, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (toolbox), gui_transform(rs), FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (toolbox), scale, FALSE, FALSE, 0);
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
