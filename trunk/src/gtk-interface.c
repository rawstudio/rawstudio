#include <gtk/gtk.h>
#include "dcraw_api.h"
#include "color.h"
#include "rawstudio.h"
#include "gtk-interface.h"
#include <string.h>

const gchar *rawsuffix[] = {"cr2", "crw", "nef", "tif" ,NULL};

gboolean
update_preview_callback(GtkAdjustment *caller, RS_BLOB *rs)
{
	update_preview(rs);
	return(FALSE);
}

void update_histogram(RS_BLOB *rs)
{
	guint c,i,x,y,rowstride;
	guint max = 0;
	guint factor = 0;
	guint hist[3][rs->histogram_w];
	GdkPixbuf *pixbuf;
	guchar *pixels, *p;

	pixbuf = gtk_image_get_pixbuf(rs->histogram_image);
	rowstride = gdk_pixbuf_get_rowstride (pixbuf);
  	pixels = gdk_pixbuf_get_pixels (pixbuf);
  	
	// sets all the pixels black
	memset(pixels, 0x00, rowstride*rs->histogram_h);
	
	// draw a grid with 7 bars with 32 pixels space
	p = pixels;
	for(y = 0; y < rs->histogram_h; y++)
	{
		for(x = 0; x < rs->histogram_w * 3; x +=93)
		{
			p[x++] = 100;
			p[x++] = 100;
			p[x++] = 100;
		}
		p+=rowstride;
	}
	
	// find the max value
	for (c = 0; c < 3; c++)
	{
		for (i = 0; i < rs->histogram_w; i++)
		{
			_MAX(rs->histogram_table[c][i], max);
		}
	}
	
	// find the factor to scale the histogram
	factor = (max+rs->histogram_h)/rs->histogram_h;

	// calculate the histogram values
	for (c = 0; c < 3; c++)
	{
		for (i = 0; i < rs->histogram_w; i++)
		{
			hist[c][i] = rs->histogram_table[c][i]/factor;
		}
	}
			
	// draw the histogram
	for (x = 0; x < rs->histogram_w; x++)
	{
		for (c = 0; c < 3; c++)
		{
			for (y = 0; y < hist[c][x]; y++)
			{				
				// address the pixel - the (rs->hist_h-1)-y is to draw it from the bottom
				p = pixels + ((rs->histogram_h-1)-y) * rowstride + x * 3;
				p[c] = 0xFF;
			}
		}
	}	
	gtk_image_set_from_pixbuf((GtkImage *) rs->histogram_image, pixbuf);

}

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
gui_transform_mirror_clicked(GtkWidget *w, RS_BLOB *rs)
{
	rs_image_mirror(rs->preview);
	update_preview(rs);
}

void
gui_transform_flip_clicked(GtkWidget *w, RS_BLOB *rs)
{
	rs_image_flip(rs->preview);
	update_preview(rs);
}

GtkWidget *
gui_transform(RS_BLOB *rs)
{
	GtkWidget *hbox;
	GtkWidget *flip;
	GtkWidget *mirror;
	
	hbox = gtk_hbox_new(TRUE, 0);
	flip = gtk_button_new_with_mnemonic ("Flip");
	mirror = gtk_button_new_with_mnemonic ("Mirror");
	g_signal_connect ((gpointer) flip, "clicked", G_CALLBACK (gui_transform_flip_clicked), rs);
	g_signal_connect ((gpointer) mirror, "clicked", G_CALLBACK (gui_transform_mirror_clicked), rs);
	gtk_widget_show (flip);
	gtk_widget_show (mirror);
	gtk_box_pack_start(GTK_BOX (hbox), flip, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX (hbox), mirror, FALSE, FALSE, 0);
	return(gui_box("Transforms", hbox));
}

GtkWidget *
gui_rgb_mixer(RS_BLOB *rs)
{
	GtkWidget *box;
	GtkWidget *rscale, *gscale, *bscale;

	rscale = gtk_hscale_new (GTK_ADJUSTMENT (rs->rgb_mixer[R]));
	gscale = gtk_hscale_new (GTK_ADJUSTMENT (rs->rgb_mixer[G]));
	bscale = gtk_hscale_new (GTK_ADJUSTMENT (rs->rgb_mixer[B]));

	gtk_scale_set_value_pos( GTK_SCALE(rscale), GTK_POS_LEFT );
	gtk_scale_set_value_pos( GTK_SCALE(gscale), GTK_POS_LEFT );
	gtk_scale_set_value_pos( GTK_SCALE(bscale), GTK_POS_LEFT );

	box = gtk_vbox_new (FALSE, 0);
	gtk_box_pack_start (GTK_BOX (box), rscale, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (box), gscale, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (box), bscale, FALSE, FALSE, 0);
	return(gui_box("RGB mixer", box));
}

GtkWidget *
gui_slider(GtkObject *adj, const gchar *label)
{
	GtkWidget *hscale;
	hscale = gtk_hscale_new (GTK_ADJUSTMENT (adj));
	gtk_scale_set_value_pos( GTK_SCALE(hscale), GTK_POS_LEFT);
	return(gui_box(label, hscale));
}

void
gui_reset_clicked(GtkWidget *w, RS_BLOB *rs)
{
	rs_reset(rs);
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
	GString *name;
	name = g_string_new(rs->filename);
	g_string_append(name, "_output.png");
	gdk_pixbuf_save(rs->preview_pixbuf, name->str, "png", NULL, NULL);
	g_string_free(name, TRUE);
	return;
}

GtkWidget *
save_file(RS_BLOB *rs)
{
	GtkWidget *button;
	button = gtk_button_new_with_mnemonic ("Save");
	g_signal_connect ((gpointer) button, "clicked", G_CALLBACK (save_file_clicked), rs);
	gtk_widget_show (button);
	return(button);
}

GtkWidget *
make_toolbox(RS_BLOB *rs)
{
	GtkWidget *toolbox;

	toolbox = gtk_vbox_new (FALSE, 0);
	gtk_widget_show (toolbox);
	gtk_box_pack_start (GTK_BOX (toolbox), gui_slider(rs->exposure, "Exposure"), FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (toolbox), gui_slider(rs->saturation, "Saturation"), FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (toolbox), gui_slider(rs->hue, "Hue rotation"), FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (toolbox), gui_slider(rs->contrast, "Contrast"), FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (toolbox), gui_rgb_mixer(rs), FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (toolbox), gui_slider(rs->scale, "Scale"), FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (toolbox), gui_slider(rs->gamma, "Gamma"), FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (toolbox), gui_hist(rs, "Histogram"), FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (toolbox), gui_transform(rs), FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (toolbox), gui_reset(rs), FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (toolbox), save_file(rs), FALSE, FALSE, 0);
	return(toolbox);
}

void
fill_model(GtkListStore *store, const char *path)
{
	gchar *name, *iname;
	guint n;
	GtkTreeIter iter;
	GdkPixbuf *pixbuf;
	GError *error;
	GDir *dir;
	gint filetype;
	
	dir = g_dir_open(path, 0, &error);
	if (dir == NULL) return;

	g_dir_rewind(dir);

	gtk_list_store_clear(store);
	while((name = (gchar *) g_dir_read_name(dir)))
	{
		iname = g_ascii_strdown(name,-1);
		filetype = FILE_UNKN;
		n=0;
		if (filetype==FILE_UNKN)
			while(rawsuffix[n])
				if (g_str_has_suffix(iname, rawsuffix[n++]))
				{
					filetype=FILE_RAW;
					break;
				}
		g_free(iname);
		if (filetype==FILE_RAW)
		{
			GString *tn;
			tn = g_string_new(name);
			tn = g_string_append(tn, ".png");
			gtk_list_store_append (store, &iter);

			pixbuf = gdk_pixbuf_new_from_file(tn->str, NULL);
			g_string_free(tn, TRUE);

			if (pixbuf==NULL)
				pixbuf = gdk_pixbuf_new(GDK_COLORSPACE_RGB, FALSE, 8, 64, 64);
			gtk_list_store_set (store, &iter, PIXBUF_COLUMN, pixbuf, TEXT_COLUMN, name, -1);
			g_object_unref (pixbuf);
		}
	}
}

void
icon_activated(GtkIconView *iconview, RS_BLOB *rs)
{
	GtkTreeModel *model;
	GtkTreeIter iter;
	GtkTreePath *path;
	gchar *name;

	model = gtk_icon_view_get_model(iconview);
	gtk_icon_view_get_cursor(iconview, &path, NULL);
	if (gtk_tree_model_get_iter(model, &iter, path))
	{
		gtk_tree_model_get(model, &iter, TEXT_COLUMN, &name, -1);
		rs_load_raw_from_file(rs, strdup(name));
		rs_reset(rs);
		update_preview(rs);
		g_free(name);
	}
}

GtkWidget *
make_iconbox(RS_BLOB *rs, GtkListStore *store)
{
	GtkWidget *scroller;
	GtkWidget *iconview;

	iconview = gtk_icon_view_new();

	gtk_icon_view_set_pixbuf_column (GTK_ICON_VIEW (iconview), PIXBUF_COLUMN);
	gtk_icon_view_set_text_column (GTK_ICON_VIEW (iconview), TEXT_COLUMN);
	gtk_icon_view_set_model (GTK_ICON_VIEW (iconview), GTK_TREE_MODEL (store));
	gtk_icon_view_set_columns(GTK_ICON_VIEW (iconview), 1000);
	gtk_widget_set_size_request (iconview, -1, 140);
	g_signal_connect((gpointer) iconview, "selection_changed",
		G_CALLBACK (icon_activated), rs);

	scroller = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scroller),
		GTK_POLICY_AUTOMATIC, GTK_POLICY_NEVER);
	gtk_container_add (GTK_CONTAINER (scroller), iconview);
	return(scroller);
}

int
gui_init(int argc, char **argv)
{
	GtkWidget *window;
	GtkWidget *scroller;
	GtkWidget *viewport;
	GtkWidget *vbox;
	GtkWidget *pane;
	GtkWidget *toolbox;
	GtkWidget *iconbox;
	GtkListStore *store;
	RS_BLOB *rs;

	gtk_init(&argc, &argv);

	rs = rs_new();

	toolbox = make_toolbox(rs);

	window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	gtk_window_resize((GtkWindow *) window, 800, 600);
	gtk_window_set_title (GTK_WINDOW (window), "Rawstudio");
	g_signal_connect((gpointer) window, "delete_event", G_CALLBACK(gtk_main_quit), NULL);

	vbox = gtk_vbox_new (FALSE, 0);
	gtk_widget_show (vbox);
	gtk_container_add (GTK_CONTAINER (window), vbox);
	
	store = gtk_list_store_new (3, GDK_TYPE_PIXBUF, G_TYPE_STRING, G_TYPE_POINTER);
	iconbox = make_iconbox(rs, store);
	fill_model(store, "./");
	gtk_box_pack_start (GTK_BOX (vbox), iconbox, FALSE, TRUE, 0);

	pane = gtk_hpaned_new ();
	gtk_box_pack_start (GTK_BOX (vbox), pane, TRUE, TRUE, 0);
	gtk_widget_show (pane);
	scroller = gtk_scrolled_window_new (NULL, NULL);
	gtk_paned_pack1 (GTK_PANED (pane), scroller, TRUE, TRUE);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scroller),
		GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_widget_show (scroller);

	gtk_paned_pack2 (GTK_PANED (pane), toolbox, FALSE, TRUE);

	viewport = gtk_viewport_new (NULL, NULL);
	gtk_container_add (GTK_CONTAINER (scroller), viewport);

	rs->preview_image = (GtkImage *) gtk_image_new();
	gtk_container_add (GTK_CONTAINER (viewport), (GtkWidget *) rs->preview_image);

	gtk_widget_show_all (window);
	gtk_main();
	return(0);
}
