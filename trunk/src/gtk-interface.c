#include <gtk/gtk.h>
#include <glib/gstdio.h>
#include "dcraw_api.h"
#include "color.h"
#include "rawstudio.h"
#include "gtk-interface.h"
#include <string.h>
#include <unistd.h>

const gchar *rawsuffix[] = {"cr2", "crw", "nef", "tif" ,NULL};

GtkStatusbar *statusbar;

void
gui_status_push(const char *text)
{
	gtk_statusbar_push(statusbar, gtk_statusbar_get_context_id(statusbar, "generic"), text);
	return;
}

void
gui_status_pop()
{
	gtk_statusbar_pop(statusbar, gtk_statusbar_get_context_id(statusbar, "generic"));
	return;
}

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
gui_transform_rot90_clicked(GtkWidget *w, RS_BLOB *rs)
{
	rs_image16_rotate(rs->scaled, 1);
	update_preview(rs);
}

void
gui_transform_rot180_clicked(GtkWidget *w, RS_BLOB *rs)
{
	rs_image16_rotate(rs->scaled, 2);
	update_preview(rs);
}

void
gui_transform_rot270_clicked(GtkWidget *w, RS_BLOB *rs)
{
	rs_image16_rotate(rs->scaled, 3);
	update_preview(rs);
}

void
gui_transform_mirror_clicked(GtkWidget *w, RS_BLOB *rs)
{
	rs_image16_mirror(rs->scaled);
	update_preview(rs);
}

void
gui_transform_flip_clicked(GtkWidget *w, RS_BLOB *rs)
{
	rs_image16_flip(rs->scaled);
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

	gtk_file_chooser_set_do_overwrite_confirmation (GTK_FILE_CHOOSER (fc), TRUE);
	gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (fc), dirname);
	gtk_file_chooser_set_current_name (GTK_FILE_CHOOSER (fc), name->str);
	if (gtk_dialog_run (GTK_DIALOG (fc)) == GTK_RESPONSE_ACCEPT)
	{
		char *filename;
		GdkPixbuf *pixbuf;

		filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (fc));
		gtk_widget_destroy(fc);
		pixbuf = gdk_pixbuf_new_from_data(rs->preview->pixels, GDK_COLORSPACE_RGB, FALSE, 8,
			rs->preview->w, rs->preview->h, rs->preview->pitch*rs->preview->channels,
			NULL, NULL);
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
	gboolean canwrite=TRUE;
	GString *dotdir;
	gint tmpfd;
	gchar *tmp;
	dir = g_dir_open(path, 0, &error);
	if (dir == NULL) return;

	dotdir = g_string_new(path);
	dotdir = g_string_append(dotdir, "/");
	dotdir = g_string_append(dotdir, DOTDIR);
	gui_status_push("Opening directory ...");
	GUI_CATCHUP();
	if (!g_file_test(dotdir->str, (G_FILE_TEST_EXISTS | G_FILE_TEST_IS_DIR)))
	{
		if (g_mkdir(dotdir->str, 0700) != 0)
		{
			/* FIXME: Should warn the user somehow */
			canwrite = FALSE;
			tmpfd = g_file_open_tmp("XXXXXX", &tmp, NULL);
			close(tmpfd);
		}
	}

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
			GString *fullname;
			fullname = g_string_new(path);
			fullname = g_string_append(fullname, "/");
			fullname = g_string_append(fullname, name);
			tn = g_string_new(path);
			tn = g_string_append(tn, "/");
			tn = g_string_append(tn, DOTDIR);
			tn = g_string_append(tn, "/");
			tn = g_string_append(tn, name);
			tn = g_string_append(tn, ".thumb.png");
			gtk_list_store_append (store, &iter);
			pixbuf = NULL;
			if (g_file_test(tn->str, G_FILE_TEST_EXISTS))
			{
				pixbuf = gdk_pixbuf_new_from_file(tn->str, NULL);
			}
			else
			{
				char *in;
				char *argv[6];
				GString *status;

				in = g_filename_to_uri(fullname->str, NULL, NULL);

				status = g_string_new("Caching thumbnail for ");
				status = g_string_append(status, name);
				gui_status_push(status->str);
				GUI_CATCHUP();
				g_string_free(status, TRUE);

				argv[0] = "/usr/bin/gnome-raw-thumbnailer";
				argv[1] = "-s";
				argv[2] = "128";
				argv[3] = in;
				argv[5] = NULL;
				if (canwrite)
					argv[4] = tn->str;
				else
					argv[4] = tmp;
				g_spawn_sync(NULL, argv, NULL, 0, NULL, NULL, NULL, NULL, NULL, &error);
				pixbuf = gdk_pixbuf_new_from_file(argv[4], NULL);
				g_free(in);
				gui_status_pop();
			}
				
			g_string_free(tn, FALSE);
			if (pixbuf==NULL)
				pixbuf = gdk_pixbuf_new(GDK_COLORSPACE_RGB, FALSE, 8, 64, 64);
			gtk_list_store_set (store, &iter,
				PIXBUF_COLUMN, pixbuf,
				TEXT_COLUMN, name,
				FULLNAME_COLUMN, fullname->str,
				-1);
			g_object_unref (pixbuf);
			g_string_free(fullname, FALSE);
		}
	}
	if (!canwrite)
	{
		g_unlink(tmp);
		g_free(tmp);
	}
	g_string_free(dotdir, TRUE);
	gui_status_pop();
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
		gtk_tree_model_get(model, &iter, FULLNAME_COLUMN, &name, -1);
		gui_status_push("Opening image ...");
		GUI_CATCHUP();
		rs_load_raw_from_file(rs, strdup(name));
		rs_reset(rs);
		update_preview(rs);
		g_free(name);
		gui_status_pop();
	}
}

void
gui_cd_clicked(GtkWidget *button, GtkListStore *store)
{
	GtkWidget *fc;
	fc = gtk_file_chooser_dialog_new ("Open File", NULL,
		GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
		GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
		GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT, NULL);

	if (gtk_dialog_run (GTK_DIALOG (fc)) == GTK_RESPONSE_ACCEPT)
	{
		char *filename;

		filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (fc));
		gtk_widget_destroy (fc);
		fill_model(store, filename);
		g_free (filename);
	} else
		gtk_widget_destroy (fc);


	return;
}

GtkWidget *
make_iconbox(RS_BLOB *rs, GtkListStore *store)
{
	GtkWidget *scroller;
	GtkWidget *iconview;
	GtkWidget *hbox;
	GtkWidget *vbox;
	GtkWidget *button_cd;
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

//	button_cd = gtk_button_new_with_mnemonic("Open");
	button_cd = gtk_button_new_from_stock(GTK_STOCK_OPEN);
	g_signal_connect ((gpointer) button_cd, "clicked", G_CALLBACK (gui_cd_clicked), store);

	vbox = gtk_vbox_new(TRUE, 0);
	gtk_box_pack_start(GTK_BOX (vbox), button_cd, FALSE, TRUE, 0);
	

	hbox = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX (hbox), vbox, FALSE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX (hbox), scroller, TRUE, TRUE, 0);
	return(hbox);
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
	gchar *cwd;

	gtk_init(&argc, &argv);

	rs = rs_new();
	statusbar = (GtkStatusbar *) gtk_statusbar_new();

	toolbox = make_toolbox(rs);

	window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	gtk_window_resize((GtkWindow *) window, 800, 600);
	gtk_window_set_title (GTK_WINDOW (window), "Rawstudio");
	g_signal_connect((gpointer) window, "delete_event", G_CALLBACK(gtk_main_quit), NULL);

	vbox = gtk_vbox_new (FALSE, 0);
	gtk_widget_show (vbox);
	gtk_container_add (GTK_CONTAINER (window), vbox);
	
	store = gtk_list_store_new (NUM_COLUMNS, GDK_TYPE_PIXBUF, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_POINTER);
	iconbox = make_iconbox(rs, store);

	cwd = g_get_current_dir();
	fill_model(store, cwd);
	g_free(cwd);

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

	rs->preview_drawingarea = gtk_drawing_area_new();
	gtk_container_add (GTK_CONTAINER (viewport), (GtkWidget *) rs->preview_drawingarea);

	gtk_box_pack_start (GTK_BOX (vbox), GTK_WIDGET (statusbar), FALSE, TRUE, 0);

	gui_status_push("Ready");

	gtk_widget_show_all (window);
	gtk_main();
	return(0);
}
