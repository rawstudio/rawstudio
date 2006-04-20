#include <gtk/gtk.h>
#include <string.h> /* memset() */
#include "dcraw_api.h"
#include "matrix.h"
#include "rawstudio.h"
#include "gtk-interface.h"
#include "color.h"
#include "toolbox.h"

GtkObject *
make_adj(RS_BLOB *rs, double value, double min, double max, double step, double page)
{
	GtkObject *adj;
	adj = gtk_adjustment_new(value, min, max, step, page, 0.0);
	gtk_signal_connect(GTK_OBJECT(adj), "value_changed",
		G_CALLBACK(update_preview_callback), rs);
	return(adj);
}

GtkWidget *
gui_hist(RS_BLOB *rs, const gchar *label)
{
	GdkPixbuf *pixbuf;

	rs->histogram_w = 256;
	rs->histogram_h = 128;

	guint rowstride;
	guchar *pixels;

	// creates the pixbuf containing the histogram 
	pixbuf = gdk_pixbuf_new(GDK_COLORSPACE_RGB, FALSE, 8, rs->histogram_w, rs->histogram_h);
	
	rowstride = gdk_pixbuf_get_rowstride (pixbuf);
	pixels = gdk_pixbuf_get_pixels (pixbuf);

	// sets all the pixels black
	memset(pixels, 0x00, rowstride*rs->histogram_h);

	// creates an image from the histogram pixbuf 
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
	DIRECTION_90(rs->direction);
	update_preview(rs);
}

void
gui_transform_rot180_clicked(GtkWidget *w, RS_BLOB *rs)
{
	DIRECTION_180(rs->direction);
	update_preview(rs);
}

void
gui_transform_rot270_clicked(GtkWidget *w, RS_BLOB *rs)
{
	DIRECTION_270(rs->direction);
	update_preview(rs);
}

void
gui_transform_mirror_clicked(GtkWidget *w, RS_BLOB *rs)
{
	DIRECTION_MIRROR(rs->direction);
	update_preview(rs);
}

void
gui_transform_flip_clicked(GtkWidget *w, RS_BLOB *rs)
{
	DIRECTION_FLIP(rs->direction);
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
	flip = gtk_button_new_with_mnemonic ("Flip");
	mirror = gtk_button_new_with_mnemonic ("Mirror");
	rot90 = gtk_button_new_with_mnemonic ("CW");
	rot180 = gtk_button_new_with_mnemonic ("180");
	rot270 = gtk_button_new_with_mnemonic ("CCW");
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
	return(gui_box("Transforms", hbox));
}

GtkWidget *
gui_tool_rgb_mixer(RS_BLOB *rs, gint n)
{
	GtkWidget *box;
	GtkWidget *rscale, *gscale, *bscale;

	rscale = gui_make_scale_from_adj(rs, G_CALLBACK(update_preview_callback), rs->settings[n]->rgb_mixer[R]);
	gscale = gui_make_scale_from_adj(rs, G_CALLBACK(update_preview_callback), rs->settings[n]->rgb_mixer[G]);
	bscale = gui_make_scale_from_adj(rs, G_CALLBACK(update_preview_callback), rs->settings[n]->rgb_mixer[B]);

	box = gtk_vbox_new (FALSE, 0);
	gtk_box_pack_start (GTK_BOX (box), rscale, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (box), gscale, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (box), bscale, FALSE, FALSE, 0);
	return(gui_box("RGB mixer", box));
}

GtkWidget *
gui_tool_warmth(RS_BLOB *rs, gint n)
{
	GtkWidget *box;
	GtkWidget *wscale;
	GtkWidget *tscale;

	wscale = gui_make_scale_from_adj(rs, G_CALLBACK(update_preview_callback), rs->settings[n]->warmth);
	tscale = gui_make_scale_from_adj(rs, G_CALLBACK(update_preview_callback), rs->settings[n]->tint);

	box = gtk_vbox_new (FALSE, 0);
	gtk_box_pack_start (GTK_BOX (box), wscale, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (box), tscale, FALSE, FALSE, 0);
	return(gui_box("Warmth/tint", box));
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

void
gui_reset_clicked(GtkWidget *w, RS_BLOB *rs)
{
	rs_settings_reset(rs->settings[rs->current_setting]);
	return;
}

GtkWidget *
gui_reset(RS_BLOB *rs)
{
	GtkWidget *button;
	button = gtk_button_new_with_mnemonic ("Reset");
	g_signal_connect ((gpointer) button, "clicked", G_CALLBACK (gui_reset_clicked), rs);
	gtk_widget_show (button);
	return(button);
}

void
save_file_clicked(GtkWidget *w, RS_BLOB *rs)
{
	GtkWidget *fc;
	GString *name;
	gchar *dirname;
	gchar *basename;
	if (!rs->in_use) return;
	dirname = g_path_get_dirname(rs->filename);
	basename = g_path_get_basename(rs->filename);
	gui_status_push("Saving file ...");
	name = g_string_new(basename);
	g_string_append(name, "_output.png");

	fc = gtk_file_chooser_dialog_new ("Save File", NULL,
		GTK_FILE_CHOOSER_ACTION_SAVE,
		GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
		GTK_STOCK_SAVE, GTK_RESPONSE_ACCEPT, NULL);
#if GTK_CHECK_VERSION(2,8,0)
	gtk_file_chooser_set_do_overwrite_confirmation (GTK_FILE_CHOOSER (fc), TRUE);
#endif
	gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (fc), dirname);
	gtk_file_chooser_set_current_name (GTK_FILE_CHOOSER (fc), name->str);
	if (gtk_dialog_run (GTK_DIALOG (fc)) == GTK_RESPONSE_ACCEPT)
	{
		char *filename;
		GdkPixbuf *pixbuf;

		filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (fc));
		gtk_widget_destroy(fc);
		pixbuf = gdk_pixbuf_new(GDK_COLORSPACE_RGB, FALSE, 8, rs->scaled->w, rs->scaled->h);
		rs_render(rs->mati, rs->scaled->w, rs->scaled->h, rs->scaled->pixels,
			rs->scaled->rowstride, rs->scaled->channels,
			gdk_pixbuf_get_pixels(pixbuf), gdk_pixbuf_get_rowstride(pixbuf));
		gdk_pixbuf_save(pixbuf, filename, "png", NULL, NULL);
		g_object_unref(pixbuf);
		g_free (filename);
	} else
		gtk_widget_destroy(fc);
	g_free(dirname);
	g_free(basename);
	g_string_free(name, TRUE);
	gui_status_pop();
	return;
}

GtkWidget *
save_file(RS_BLOB *rs)
{
	GtkWidget *button;
	button = gtk_button_new_with_mnemonic ("Save PNG");
	g_signal_connect ((gpointer) button, "clicked", G_CALLBACK (save_file_clicked), rs);
	gtk_widget_show (button);
	return(button);
}

GtkWidget *
gui_make_scale(RS_BLOB *rs, GCallback cb, double value, double min, double max, double step, double page)
{
	GtkWidget *hscale;
	GtkObject *adj;
	adj = gtk_adjustment_new(value, min, max, step, page, 0.0);
	g_signal_connect(GTK_OBJECT(adj), "value_changed",
		cb, rs);
	hscale = gtk_hscale_new (GTK_ADJUSTMENT (adj));
	gtk_scale_set_value_pos( GTK_SCALE(hscale), GTK_POS_LEFT);
	gtk_scale_set_digits(GTK_SCALE(hscale), 2);
	return(hscale);
}

GtkWidget *
gui_make_scale_from_adj(RS_BLOB *rs, GCallback cb, GtkObject *adj)
{
	GtkWidget *hscale;

	hscale = gtk_hscale_new((GtkAdjustment *) adj);
	g_signal_connect(adj, "value_changed", cb, rs);
	gtk_scale_set_value_pos( GTK_SCALE(hscale), GTK_POS_LEFT);
	gtk_scale_set_digits(GTK_SCALE(hscale), 2);
	return(hscale);
}

GtkWidget *
gui_tool_exposure(RS_BLOB *rs, gint n)
{
	GtkWidget *hscale;

	hscale = gui_make_scale_from_adj(rs, G_CALLBACK(update_preview_callback), rs->settings[n]->exposure);
	return(gui_box("Exposure", hscale));
}

GtkWidget *
gui_tool_saturation(RS_BLOB *rs, gint n)
{
	GtkWidget *hscale;

	hscale = gui_make_scale_from_adj(rs, G_CALLBACK(update_preview_callback), rs->settings[n]->saturation);
	return(gui_box("Saturation", hscale));
}

GtkWidget *
gui_tool_hue(RS_BLOB *rs, gint n)
{
	GtkWidget *hscale;

	hscale = gui_make_scale_from_adj(rs, G_CALLBACK(update_preview_callback), rs->settings[n]->hue);
	return(gui_box("Hue", hscale));
}

GtkWidget *
gui_tool_contrast(RS_BLOB *rs, gint n)
{
	GtkWidget *hscale;

	hscale = gui_make_scale_from_adj(rs, G_CALLBACK(update_preview_callback), rs->settings[n]->contrast);
	return(gui_box("Contrast", hscale));
}

GtkWidget *
gui_tool_gamma(RS_BLOB *rs, gint n)
{
	GtkWidget *hscale;

	hscale = gui_make_scale_from_adj(rs, G_CALLBACK(update_preview_callback), rs->settings[n]->gamma);
	return(gui_box("Gamma", hscale));
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
/*	gtk_box_pack_start (GTK_BOX (toolbox), gui_tool_rgb_mixer(rs, n), FALSE, FALSE, 0); */
	gtk_box_pack_start (GTK_BOX (toolbox), gui_tool_gamma(rs, n), FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (toolbox), gui_transform(rs), FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (toolbox), gui_reset(rs), FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (toolbox), save_file(rs), FALSE, FALSE, 0);
	return(toolbox);
}

void
gui_notebook_callback(GtkNotebook *notebook, GtkNotebookPage *page, guint page_num, RS_BLOB *rs)
{
	rs->current_setting = page_num;
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

	label1 = gtk_label_new(" A ");
	label2 = gtk_label_new(" B ");
	label3 = gtk_label_new(" C ");

	notebook = gtk_notebook_new();
	gtk_notebook_append_page(GTK_NOTEBOOK(notebook), gui_make_tools(rs, 0), label1);
	gtk_notebook_append_page(GTK_NOTEBOOK(notebook), gui_make_tools(rs, 1), label2);
	gtk_notebook_append_page(GTK_NOTEBOOK(notebook), gui_make_tools(rs, 2), label3);
	g_signal_connect(notebook, "switch-page", G_CALLBACK(gui_notebook_callback), rs);

	toolbox = gtk_vbox_new (FALSE, 0);
	gtk_box_pack_start (GTK_BOX (toolbox), notebook, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (toolbox), gui_slider(rs->scale, "Scale"), FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (toolbox), gui_hist(rs, "Histogram"), FALSE, FALSE, 0);
	return(toolbox);
}
