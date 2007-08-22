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
#include <config.h>
#include "rawstudio.h"
#include "gtk-helper.h"
#include "gtk-interface.h"
#include "toolbox.h"
#include "conf_interface.h"
#include "gettext.h"
#include "color.h"
#include "rs-spline.h"
#include "rs-curve.h"

/* used for gui_adj_reset_callback() */
struct reset_carrier {
	RS_BLOB *rs;
	gint mask;
};

GtkLabel *infolabel;
static GtkWidget *toolbox;

static GtkWidget *gui_hist(RS_BLOB *rs, const gchar *label, gboolean show);
static GtkWidget *gui_box(const gchar *title, GtkWidget *in, gboolean expanded);
static void gui_transform_rot90_clicked(GtkWidget *w, RS_BLOB *rs);
static void gui_transform_rot180_clicked(GtkWidget *w, RS_BLOB *rs);
static void gui_transform_rot270_clicked(GtkWidget *w, RS_BLOB *rs);
static void gui_transform_mirror_clicked(GtkWidget *w, RS_BLOB *rs);
static void gui_transform_flip_clicked(GtkWidget *w, RS_BLOB *rs);
static GtkWidget *gui_transform(RS_BLOB *rs, gboolean show);
static GtkWidget *gui_tool_warmth(RS_BLOB *rs, gint n, gboolean show);
static gboolean gui_adj_reset_callback(GtkWidget *widget, GdkEventButton *event, struct reset_carrier *rc);
static GtkWidget *gui_make_scale_from_adj(RS_BLOB *rs, GCallback cb, GtkObject *adj, gint mask);
static void curve_context_callback_save(GtkMenuItem *menuitem, gpointer user_data);
static void curve_context_callback_open(GtkMenuItem *menuitem, gpointer user_data);
static void curve_context_callback_reset(GtkMenuItem *menuitem, gpointer user_data);
static void curve_context_callback(GtkWidget *widget, gpointer user_data);
static void gui_notebook_callback(GtkNotebook *notebook, GtkNotebookPage *page, guint page_num, RS_BLOB *rs);

static GtkWidget *
gui_hist(RS_BLOB *rs, const gchar *label, gboolean show)
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

	return(gui_box(label, (GtkWidget *)rs->histogram_image, show));
}

static GtkWidget *
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

static void
gui_transform_rot90_clicked(GtkWidget *w, RS_BLOB *rs)
{
	if (!rs->photo) return;
	rs_photo_rotate(rs->photo, 1, 0.0);
	rs_update_preview(rs);
}

static void
gui_transform_rot180_clicked(GtkWidget *w, RS_BLOB *rs)
{
	if (!rs->photo) return;
	rs_photo_rotate(rs->photo, 2, 0.0);
	rs_update_preview(rs);
}

static void
gui_transform_rot270_clicked(GtkWidget *w, RS_BLOB *rs)
{
	if (!rs->photo) return;
	rs_photo_rotate(rs->photo, 3, 0.0);
	rs_update_preview(rs);
}

static void
gui_transform_mirror_clicked(GtkWidget *w, RS_BLOB *rs)
{
	if (!rs->photo) return;
	rs_photo_mirror(rs->photo);
	rs_update_preview(rs);
}

static void
gui_transform_flip_clicked(GtkWidget *w, RS_BLOB *rs)
{
	if (!rs->photo) return;
	rs_photo_flip(rs->photo);
	rs_update_preview(rs);
}

static GtkWidget *
gui_transform(RS_BLOB *rs, gboolean show)
{
	GtkWidget *hbox;
	GtkWidget *flip;
	GtkWidget *mirror;
	GtkWidget *rot90;
	GtkWidget *rot180;
	GtkWidget *rot270;

	GtkWidget *flip_image = gtk_image_new_from_file(PACKAGE_DATA_DIR "/pixmaps/rawstudio/transform_flip.png");
	GtkWidget *mirror_image = gtk_image_new_from_file(PACKAGE_DATA_DIR "/pixmaps/rawstudio/transform_mirror.png");
	GtkWidget *rot90_image = gtk_image_new_from_file(PACKAGE_DATA_DIR "/pixmaps/rawstudio/transform_90.png");
	GtkWidget *rot180_image = gtk_image_new_from_file(PACKAGE_DATA_DIR "/pixmaps/rawstudio/transform_180.png");
	GtkWidget *rot270_image = gtk_image_new_from_file(PACKAGE_DATA_DIR "/pixmaps/rawstudio/transform_270.png");

	hbox = gtk_hbox_new(FALSE, 0);
	flip = gtk_button_new();
	mirror = gtk_button_new();
	rot90 = gtk_button_new();
	rot180 = gtk_button_new();
	rot270 = gtk_button_new();

	gtk_button_set_image(GTK_BUTTON(flip), flip_image);
	gtk_button_set_image(GTK_BUTTON(mirror), mirror_image);
	gtk_button_set_image(GTK_BUTTON(rot90), rot90_image);
	gtk_button_set_image(GTK_BUTTON(rot180), rot180_image);	
	gtk_button_set_image(GTK_BUTTON(rot270), rot270_image);

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
	return(gui_box(_("Transforms"), hbox, show));
}

static GtkWidget *
gui_tool_warmth(RS_BLOB *rs, gint n, gboolean show)
{
	GtkWidget *box;
	GtkWidget *wscale;
	GtkWidget *tscale;

	wscale = gui_make_scale_from_adj(rs, G_CALLBACK(update_preview_callback), rs->settings[n]->warmth, MASK_WARMTH);
	tscale = gui_make_scale_from_adj(rs, G_CALLBACK(update_preview_callback), rs->settings[n]->tint, MASK_TINT);

	box = gtk_vbox_new (FALSE, 0);
	gtk_box_pack_start (GTK_BOX (box), wscale, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (box), tscale, FALSE, FALSE, 0);
	return(gui_box(_("Warmth/tint"), box, show));
}

static gboolean
gui_adj_reset_callback(GtkWidget *widget, GdkEventButton *event, struct reset_carrier *rc)
{
	rs_settings_reset(rc->rs->settings[rc->rs->current_setting], rc->mask);
	return(TRUE);
}

static GtkWidget *
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

static void
curve_context_callback_save(GtkMenuItem *menuitem, gpointer user_data)
{
	RSCurveWidget *curve = RS_CURVE_WIDGET(user_data);
	GtkWidget *fc;
	GString *dir;

	fc = gtk_file_chooser_dialog_new (_("Export File"), NULL,
		GTK_FILE_CHOOSER_ACTION_SAVE,
		GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
		GTK_STOCK_SAVE, GTK_RESPONSE_ACCEPT, NULL);
	gtk_dialog_set_default_response(GTK_DIALOG(fc), GTK_RESPONSE_ACCEPT);
#if GTK_CHECK_VERSION(2,8,0)
	gtk_file_chooser_set_do_overwrite_confirmation (GTK_FILE_CHOOSER (fc), TRUE);
#endif

	/* Set default directory */
	dir = g_string_new(g_get_home_dir());
	g_string_append(dir, G_DIR_SEPARATOR_S);
	g_string_append(dir, DOTDIR);
	g_string_append(dir, G_DIR_SEPARATOR_S);
	g_string_append(dir, "curves");
	g_mkdir_with_parents(dir->str, 00755);
	gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER (fc), dir->str);
	g_string_free(dir, TRUE);

	if (gtk_dialog_run (GTK_DIALOG (fc)) == GTK_RESPONSE_ACCEPT)
	{
		char *filename;
		filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (fc));
		if (filename)
		{
			if (!g_str_has_suffix(filename, ".rscurve"))
			{
				GString *gs;
				gs = g_string_new(filename);
				g_string_append(gs, ".rscurve");
				g_free(filename);
				filename = gs->str;
				g_string_free(gs, FALSE);
			}
			rs_curve_widget_save(curve, filename);
			g_free(filename);
		}
	}
	gtk_widget_destroy(fc);
}

static void
curve_context_callback_open(GtkMenuItem *menuitem, gpointer user_data)
{
	RSCurveWidget *curve = RS_CURVE_WIDGET(user_data);
	GtkWidget *fc;
	GString *dir;

	fc = gtk_file_chooser_dialog_new (_("Open Curve ..."), NULL,
		GTK_FILE_CHOOSER_ACTION_OPEN,
		GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
		GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT, NULL);
	gtk_dialog_set_default_response(GTK_DIALOG(fc), GTK_RESPONSE_ACCEPT);

	/* Set default directory */
	dir = g_string_new(g_get_home_dir());
	g_string_append(dir, G_DIR_SEPARATOR_S);
	g_string_append(dir, DOTDIR);
	g_string_append(dir, G_DIR_SEPARATOR_S);
	g_string_append(dir, "curves");
	g_mkdir_with_parents(dir->str, 00755);
	gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER (fc), dir->str);
	g_string_free(dir, TRUE);

	if (gtk_dialog_run (GTK_DIALOG (fc)) == GTK_RESPONSE_ACCEPT)
	{
		char *filename;
		filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (fc));
		if (filename)
		{
			rs_curve_widget_load(curve, filename);
			g_free(filename);
		}
	}
	gtk_widget_destroy(fc);
}

static void
curve_context_callback_reset(GtkMenuItem *menuitem, gpointer user_data)
{
	RSCurveWidget *curve = RS_CURVE_WIDGET(user_data);

	rs_curve_widget_reset(curve);
	rs_curve_widget_add_knot(curve, 0.0,0.0);
	rs_curve_widget_add_knot(curve, 1.0,1.0);
}

static void
curve_context_callback_white_black_point(GtkMenuItem *menuitem, gpointer user_data)
{
	RS_BLOB *rs = user_data;
	rs_white_black_point(rs);
}

static void
curve_context_callback(GtkWidget *widget, gpointer user_data)
{
	GtkWidget *i, *menu = gtk_menu_new();
	gint n=0;
	RS_BLOB *rs = user_data;

	i = gtk_menu_item_new_with_label (_("Open curve ..."));
	gtk_widget_show (i);
	gtk_menu_attach (GTK_MENU (menu), i, 0, 1, n, n+1); n++;
	g_signal_connect (i, "activate", G_CALLBACK (curve_context_callback_open), widget);
	i = gtk_menu_item_new_with_label (_("Save curve as ..."));
	gtk_widget_show (i);
	gtk_menu_attach (GTK_MENU (menu), i, 0, 1, n, n+1); n++;
	g_signal_connect (i, "activate", G_CALLBACK (curve_context_callback_save), widget);
	i = gtk_menu_item_new_with_label (_("Reset curve"));
	gtk_widget_show (i);
	gtk_menu_attach (GTK_MENU (menu), i, 0, 1, n, n+1); n++;
	g_signal_connect (i, "activate", G_CALLBACK (curve_context_callback_reset), widget);
	i = gtk_menu_item_new_with_label (_("Auto adjust curve ends"));
	gtk_widget_show (i);
	gtk_menu_attach (GTK_MENU (menu), i, 0, 1, n, n+1); n++;
	g_signal_connect (i, "activate", G_CALLBACK (curve_context_callback_white_black_point), rs);
	gtk_menu_popup(GTK_MENU(menu), NULL, NULL, NULL, NULL, 0, GDK_CURRENT_TIME);
}

static void
gui_notebook_callback(GtkNotebook *notebook, GtkNotebookPage *page, guint page_num, RS_BLOB *rs)
{
	rs->current_setting = page_num;
	if (rs->photo)
		rs_update_preview(rs);
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

void 
gui_expander_toggle_callback(GtkExpander *expander, GtkWidget **expanders)
{
	gboolean expanded = gtk_expander_get_expanded(expander);

	/* Set expanders on all tabs to the same state */
	gtk_expander_set_expanded(GTK_EXPANDER(expanders[0]), expanded);
	gtk_expander_set_expanded(GTK_EXPANDER(expanders[1]), expanded);
	gtk_expander_set_expanded(GTK_EXPANDER(expanders[2]), expanded);
}

void
gui_expander_save_status_callback(GtkExpander *expander, gchar *name) {
	gboolean expanded = gtk_expander_get_expanded(expander);

	rs_conf_set_boolean(name, expanded);
}

GtkWidget *
make_toolbox(RS_BLOB *rs)
{
	GtkWidget *notebook;
	GtkWidget *tbox[3];
	GtkWidget *toolbox_label[3];
	GtkWidget **toolbox_exposure = g_new(GtkWidget *, 3); /* Please note that these allocations never get freed */
	GtkWidget **toolbox_saturation = g_new(GtkWidget *, 3);
	GtkWidget **toolbox_hue = g_new(GtkWidget *, 3);
	GtkWidget **toolbox_contrast = g_new(GtkWidget *, 3);
	GtkWidget **toolbox_warmth = g_new(GtkWidget *, 3);
	GtkWidget **toolbox_curve = g_new(GtkWidget *, 3);
	GtkWidget *toolbox_transform;
	GtkWidget *toolbox_hist;
	GtkWidget *toolboxscroller;
	GtkWidget *toolboxviewport;
	gint n;
	gboolean show;

	toolbox_label[0] = gtk_label_new(_(" A "));
	toolbox_label[1] = gtk_label_new(_(" B "));
	toolbox_label[2] = gtk_label_new(_(" C "));
	notebook = gtk_notebook_new();

	for(n = 0; n < 3; n++) {
		tbox[n] = gtk_vbox_new (FALSE, 0);
		gtk_widget_show (tbox[n]);

		rs_conf_get_boolean_with_default(CONF_SHOW_TOOLBOX_EXPOSURE, &show, DEFAULT_CONF_SHOW_TOOLBOX_EXPOSURE);
		toolbox_exposure[n] = gui_box(_("Exposure"), gui_make_scale_from_adj(rs, 
			G_CALLBACK(update_preview_callback), rs->settings[n]->exposure, MASK_EXPOSURE), show);
		gtk_box_pack_start (GTK_BOX (tbox[n]), toolbox_exposure[n], FALSE, FALSE, 0);
		g_signal_connect_after(toolbox_exposure[n], "activate", G_CALLBACK(gui_expander_toggle_callback), toolbox_exposure);
		g_signal_connect_after(toolbox_exposure[n], "activate", G_CALLBACK(gui_expander_save_status_callback), CONF_SHOW_TOOLBOX_EXPOSURE);

		rs_conf_get_boolean_with_default(CONF_SHOW_TOOLBOX_SATURATION, &show, DEFAULT_CONF_SHOW_TOOLBOX_SATURATION);
		toolbox_saturation[n] = gui_box(_("Saturation"), gui_make_scale_from_adj(rs, 
			G_CALLBACK(update_preview_callback), rs->settings[n]->saturation, MASK_SATURATION), show);
		gtk_box_pack_start (GTK_BOX (tbox[n]), toolbox_saturation[n], FALSE, FALSE, 0);
		g_signal_connect_after(toolbox_saturation[n], "activate", G_CALLBACK(gui_expander_toggle_callback), toolbox_saturation);
		g_signal_connect_after(toolbox_saturation[n], "activate", G_CALLBACK(gui_expander_save_status_callback), CONF_SHOW_TOOLBOX_SATURATION);

		rs_conf_get_boolean_with_default(CONF_SHOW_TOOLBOX_HUE, &show, DEFAULT_CONF_SHOW_TOOLBOX_HUE);
		toolbox_hue[n] = gui_box(_("Hue"), gui_make_scale_from_adj(rs, 
			G_CALLBACK(update_preview_callback), rs->settings[n]->hue, MASK_HUE), show);
		gtk_box_pack_start (GTK_BOX (tbox[n]), toolbox_hue[n], FALSE, FALSE, 0);
		g_signal_connect_after(toolbox_hue[n], "activate", G_CALLBACK(gui_expander_toggle_callback), toolbox_hue);
		g_signal_connect_after(toolbox_hue[n], "activate", G_CALLBACK(gui_expander_save_status_callback), CONF_SHOW_TOOLBOX_HUE);

		rs_conf_get_boolean_with_default(CONF_SHOW_TOOLBOX_CONTRAST, &show, DEFAULT_CONF_SHOW_TOOLBOX_CONTRAST);
		toolbox_contrast[n] = gui_box(_("Contrast"), gui_make_scale_from_adj(rs,
			G_CALLBACK(update_preview_callback), rs->settings[n]->contrast, MASK_CONTRAST), show);
		gtk_box_pack_start (GTK_BOX (tbox[n]), toolbox_contrast[n], FALSE, FALSE, 0);
		g_signal_connect_after(toolbox_contrast[n], "activate", G_CALLBACK(gui_expander_toggle_callback), toolbox_contrast);
		g_signal_connect_after(toolbox_contrast[n], "activate", G_CALLBACK(gui_expander_save_status_callback), CONF_SHOW_TOOLBOX_CONTRAST);

		rs_conf_get_boolean_with_default(CONF_SHOW_TOOLBOX_WARMTH, &show, DEFAULT_CONF_SHOW_TOOLBOX_WARMTH);
		toolbox_warmth[n] = gui_tool_warmth(rs, n, show);
		gtk_box_pack_start (GTK_BOX (tbox[n]), toolbox_warmth[n], FALSE, FALSE, 0);
		g_signal_connect_after(toolbox_warmth[n], "activate", G_CALLBACK(gui_expander_toggle_callback), toolbox_warmth);
		g_signal_connect_after(toolbox_warmth[n], "activate", G_CALLBACK(gui_expander_save_status_callback), CONF_SHOW_TOOLBOX_WARMTH);

		rs_conf_get_boolean_with_default(CONF_SHOW_TOOLBOX_CURVE, &show, DEFAULT_CONF_SHOW_TOOLBOX_CURVE);
		gtk_widget_set_size_request(rs->settings[n]->curve, 64, 64);
		g_signal_connect(rs->settings[n]->curve, "changed", G_CALLBACK(update_preview_callback), rs);
		g_signal_connect(rs->settings[n]->curve, "right-click", G_CALLBACK(curve_context_callback), rs);
		toolbox_curve[n] = gui_box(_("Curve"), rs->settings[n]->curve, show);
		gtk_box_pack_start (GTK_BOX (tbox[n]), toolbox_curve[n], TRUE, FALSE, 0);
		g_signal_connect_after(toolbox_curve[n], "activate", G_CALLBACK(gui_expander_toggle_callback), toolbox_curve);
		g_signal_connect_after(toolbox_curve[n], "activate", G_CALLBACK(gui_expander_save_status_callback), CONF_SHOW_TOOLBOX_CURVE);

		gtk_notebook_append_page(GTK_NOTEBOOK(notebook), tbox[n], toolbox_label[n]);
	}
	g_signal_connect(notebook, "switch-page", G_CALLBACK(gui_notebook_callback), rs);

	toolbox = gtk_vbox_new (FALSE, 0);
	gtk_box_pack_start (GTK_BOX (toolbox), notebook, FALSE, FALSE, 0);
	rs_conf_get_boolean_with_default(CONF_SHOW_TOOLBOX_TRANSFORM, &show, DEFAULT_CONF_SHOW_TOOLBOX_TRANSFORM);
	toolbox_transform = gui_transform(rs, show);
	gtk_box_pack_start (GTK_BOX (toolbox), toolbox_transform, FALSE, FALSE, 0);
	g_signal_connect_after(toolbox_transform, "activate", G_CALLBACK(gui_expander_save_status_callback), CONF_SHOW_TOOLBOX_TRANSFORM);
	rs_conf_get_boolean_with_default(CONF_SHOW_TOOLBOX_HIST, &show, DEFAULT_CONF_SHOW_TOOLBOX_HIST);
	toolbox_hist = gui_hist(rs, _("Histogram"), show);
	gtk_box_pack_start (GTK_BOX (toolbox), toolbox_hist, FALSE, FALSE, 0);
	g_signal_connect_after(toolbox_hist, "activate", G_CALLBACK(gui_expander_save_status_callback), CONF_SHOW_TOOLBOX_HIST);

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
