#include <glib.h>
#include <gtk/gtk.h>
#include <glib/gstdio.h>
#include "dcraw_api.h"
#include "color.h"
#include "matrix.h"
#include "rawstudio.h"
#include "gtk-interface.h"
#include "drawingarea.h"
#include "toolbox.h"
#include "conf_interface.h"
#include "rs-cache.h"
#include <string.h>
#include <unistd.h>

static gchar *option_dir = NULL;
static GOptionEntry entries[] = 
  {
    { "dir", 'd', 0, G_OPTION_ARG_STRING, &option_dir, "Open this directory as cwd instead of current or the last used.", NULL },
    { NULL }
  };

GtkStatusbar *statusbar;
static gboolean fullscreen = FALSE;
static GtkWidget *iconview = NULL;

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

gint
fill_model_compare_func (GtkTreeModel *model, GtkTreeIter *tia,
	GtkTreeIter *tib, gpointer userdata)
{
	gint ret;
	gchar *a, *b;

	gtk_tree_model_get(model, tia, TEXT_COLUMN, &a, -1);
	gtk_tree_model_get(model, tib, TEXT_COLUMN, &b, -1);
	ret = g_utf8_collate(a,b);
	g_free(a);
	g_free(b);
	return(ret);
}

void
fill_model(GtkListStore *store, const gchar *inpath)
{
	static gchar *path=NULL;
	gchar *name;
	GtkTreeIter iter;
	GdkPixbuf *pixbuf;
	GError *error;
	GDir *dir;
	GtkTreeSortable *sortable;
	RS_FILETYPE *filetype;

	if (inpath)
	{
		if (path)
			g_free(path);
		path = g_strdup(inpath);
	}
	dir = g_dir_open(path, 0, &error);
	if (dir == NULL) return;

	rs_conf_set_string(CONF_LWD, path);

	gui_status_push("Opening directory ...");
	GUI_CATCHUP();

	g_dir_rewind(dir);

	gtk_list_store_clear(store);
	while((name = (gchar *) g_dir_read_name(dir)))
	{
		filetype = rs_filetype_get(name, TRUE);
		if (filetype)
			if (filetype->load)
			{
				GString *fullname;
				fullname = g_string_new(path);
				fullname = g_string_append(fullname, "/");
				fullname = g_string_append(fullname, name);
				pixbuf = NULL;
				if (filetype->thumb)
					pixbuf = filetype->thumb(fullname->str);
				gtk_list_store_prepend (store, &iter);
				if (pixbuf==NULL)
					pixbuf = gdk_pixbuf_new(GDK_COLORSPACE_RGB, FALSE, 8, 64, 64);
				gtk_list_store_set (store, &iter,
					PIXBUF_COLUMN, pixbuf,
					TEXT_COLUMN, name,
					FULLNAME_COLUMN, fullname->str,
					PRIORITY_COLUMN, -1,
					-1);
				g_object_unref (pixbuf);
				g_string_free(fullname, FALSE);
			}
	}
	sortable = GTK_TREE_SORTABLE(store);
	gtk_tree_sortable_set_sort_func(sortable,
		TEXT_COLUMN,
		fill_model_compare_func,
		NULL,
		NULL);
	gtk_tree_sortable_set_sort_column_id(sortable, TEXT_COLUMN, GTK_SORT_ASCENDING);
	gui_status_pop();
}

void
icon_activated_helper(GtkIconView *iconview, GtkTreePath *path, gpointer user_data)
{
	gchar *name;
	gchar **out = user_data;
	GtkTreeModel *model = gtk_icon_view_get_model (iconview);
	GtkTreeIter iter;

	if (gtk_tree_model_get_iter(model, &iter, path))
	{
		gtk_tree_model_get (model, &iter, FULLNAME_COLUMN, &name, -1);
		*out = name;
	}
}

void
icon_activated(GtkIconView *iconview, RS_BLOB *rs)
{
	GtkTreeModel *model;
	gchar *name = NULL;
	RS_FILETYPE *filetype;

	model = gtk_icon_view_get_model(iconview);
	gtk_icon_view_selected_foreach(iconview, icon_activated_helper, &name);
	if (name!=NULL)
	{
		gui_status_push("Opening image ...");
		GUI_CATCHUP();
		if ((filetype = rs_filetype_get(name, TRUE)))
		{
			rs_cache_save(rs);
			filetype->load(rs, name);
		}
		update_preview(rs);
		gui_status_pop();
	}
}

GtkWidget *
make_iconbox(RS_BLOB *rs, GtkListStore *store)
{
	GtkWidget *scroller;

	iconview = gtk_icon_view_new();

	gtk_icon_view_set_pixbuf_column (GTK_ICON_VIEW (iconview), PIXBUF_COLUMN);
	gtk_icon_view_set_text_column (GTK_ICON_VIEW (iconview), TEXT_COLUMN);
	gtk_icon_view_set_model (GTK_ICON_VIEW (iconview), GTK_TREE_MODEL (store));
	gtk_icon_view_set_columns(GTK_ICON_VIEW (iconview), 1000);
	gtk_icon_view_set_selection_mode(GTK_ICON_VIEW (iconview), GTK_SELECTION_BROWSE);
	gtk_widget_set_size_request (iconview, -1, 140);
	g_signal_connect((gpointer) iconview, "selection_changed",
		G_CALLBACK (icon_activated), rs);

	scroller = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scroller),
		GTK_POLICY_AUTOMATIC, GTK_POLICY_NEVER);
	gtk_container_add (GTK_CONTAINER (scroller), iconview);

	return(scroller);
}

void
gui_menu_open_callback(gpointer callback_data, guint callback_action, GtkWidget *widget)
{
	GtkWidget *fc;
	GtkListStore *store = (GtkListStore *) callback_action;
	gchar *lwd = rs_conf_get_string(CONF_LWD);

	fc = gtk_file_chooser_dialog_new ("Open File", NULL,
		GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
		GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
		GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT, NULL);

	gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER (fc), lwd);

	if (gtk_dialog_run (GTK_DIALOG (fc)) == GTK_RESPONSE_ACCEPT)
	{
		char *filename;

		filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (fc));
		gtk_widget_destroy (fc);
		fill_model(store, filename);
		g_free (filename);
	} else
		gtk_widget_destroy (fc);

	g_free(lwd);
	return;
}
void
gui_menu_reload_callback(gpointer callback_data, guint callback_action, GtkWidget *widget)
{
	GtkListStore *store = (GtkListStore *) callback_action;
	fill_model(store, NULL);
	return;
}
void
gui_preview_bg_color_changed(GtkColorButton *widget, RS_BLOB *rs)
{
	GdkColor color;
	gtk_color_button_get_color(GTK_COLOR_BUTTON(widget), &color);
	gtk_widget_modify_bg(rs->preview_drawingarea->parent->parent,
		GTK_STATE_NORMAL, &color);
	rs_conf_set_color("preview_background_color", &color);
	return;
}

gboolean
gui_fullscreen_callback(GtkWidget *widget, GdkEventWindowState *event, GtkWidget *iconbox)
{
	if (event->new_window_state & GDK_WINDOW_STATE_FULLSCREEN)
	{
		gtk_widget_hide(iconbox);
		fullscreen = TRUE;
	}
	if (!(event->new_window_state & GDK_WINDOW_STATE_FULLSCREEN))
	{
		gtk_widget_show(iconbox);
		fullscreen = FALSE;
	}
	return(FALSE);
}

void
gui_menu_iconbar_previous_callback(gpointer callback_data, guint callback_action, GtkWidget *widget)
{
	GtkTreePath *path;

	gtk_icon_view_get_cursor((GtkIconView *) iconview, &path, NULL);
	if(path!=NULL)
	{
		gtk_tree_path_prev(path);
		gtk_icon_view_set_cursor((GtkIconView *) iconview, path, NULL, FALSE);
		gtk_icon_view_select_path((GtkIconView *) iconview, path);
	}
	return;
}

void
gui_menu_iconbar_next_callback(gpointer callback_data, guint callback_action, GtkWidget *widget)
{
	GtkTreePath *path;

	gtk_icon_view_get_cursor((GtkIconView *) iconview, &path, NULL);
	if(path!=NULL)
	{
		gtk_tree_path_next(path);
		gtk_icon_view_set_cursor((GtkIconView *) iconview, path, NULL, FALSE);
		gtk_icon_view_select_path((GtkIconView *) iconview, path);
	}
	return;
}

void
gui_menu_setprio_callback(gpointer callback_data, guint callback_action, GtkWidget *widget)
{
	GtkTreePath *path;
	GtkTreeIter iter;
	GtkTreeModel *model;

	gtk_icon_view_get_cursor((GtkIconView *) iconview, &path, NULL);
	if(path!=NULL)
	{
		model = gtk_icon_view_get_model((GtkIconView *) iconview);
		gtk_tree_model_get_iter(model, &iter, path);
		gtk_list_store_set ((GtkListStore *)model, &iter,
			PRIORITY_COLUMN, callback_action,
			-1);
	}
	return;
}

void
gui_menu_widget_visible_callback(gpointer callback_data, guint callback_action, GtkWidget *widget)
{
	GtkWidget *target = (GtkWidget *) callback_action;
	if (GTK_WIDGET_VISIBLE(target))
		gtk_widget_hide(target);
	else
		gtk_widget_show(target);
	return;
}

void
gui_menu_fullscreen_callback(gpointer callback_data, guint callback_action, GtkWidget *widget)
{
	GtkWindow *window = (GtkWindow *) callback_action;
	if (fullscreen)
		gtk_window_unfullscreen(window);
	else
		gtk_window_fullscreen(window);
	return;
}

void
gui_menu_preference_callback(gpointer callback_data, guint callback_action, GtkWidget *widget)
{
	GtkWidget *dialog;
	GtkWidget *notebook;
	GtkWidget *vbox;
	GtkWidget *colorsel;
	GtkWidget *colorsel_label;
	GtkWidget *colorsel_hbox;
	GtkWidget *preview_page;
	GtkWidget *button_close;
	GdkColor color;
	RS_BLOB *rs = (RS_BLOB *) callback_action;

	dialog = gtk_dialog_new();
	gtk_window_set_title(GTK_WINDOW(dialog), "Preferences");
	gtk_window_set_resizable(GTK_WINDOW(dialog), FALSE);
	gtk_window_set_type_hint(GTK_WINDOW(dialog), GDK_WINDOW_TYPE_HINT_DIALOG);
	gtk_window_set_destroy_with_parent(GTK_WINDOW(dialog), TRUE);
	gtk_dialog_set_has_separator (GTK_DIALOG(dialog), FALSE);
	g_signal_connect_swapped(dialog, "delete_event",
		G_CALLBACK (gtk_widget_destroy), dialog);
	g_signal_connect_swapped(dialog, "response",
		G_CALLBACK (gtk_widget_destroy), dialog);

	vbox = GTK_DIALOG (dialog)->vbox;

	preview_page = gtk_vbox_new(FALSE, 0);
	colorsel_hbox = gtk_hbox_new(FALSE, 0);
	colorsel_label = gtk_label_new("Preview background color:");
	colorsel = gtk_color_button_new();
	COLOR_BLACK(color);
	if (rs_conf_get_color("preview_background_color", &color))
		gtk_color_button_set_color(GTK_COLOR_BUTTON(colorsel), &color);
	g_signal_connect(colorsel, "color-set", G_CALLBACK (gui_preview_bg_color_changed), rs);
	gtk_box_pack_start (GTK_BOX (colorsel_hbox), colorsel_label, FALSE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (colorsel_hbox), colorsel, FALSE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (preview_page), colorsel_hbox, FALSE, TRUE, 0);

	notebook = gtk_notebook_new();
	gtk_container_set_border_width (GTK_CONTAINER (notebook), 6);
	gtk_notebook_append_page(GTK_NOTEBOOK(notebook), preview_page, gtk_label_new("Preview"));
	gtk_box_pack_start (GTK_BOX (vbox), notebook, FALSE, FALSE, 0);

	button_close = gtk_button_new_from_stock(GTK_STOCK_CLOSE);
	gtk_dialog_add_action_widget (GTK_DIALOG (dialog), button_close, GTK_RESPONSE_CLOSE);

	gtk_widget_show_all(dialog);
	return;
}

void
gui_about()
{
	static GtkWidget *aboutdialog = NULL;
	const gchar *authors[] = {
		"Anders Brander <anders@brander.dk>",
		"Anders Kvist <anders@kvistmail.dk>",
	};
	if (!aboutdialog)
	{
		aboutdialog = gtk_about_dialog_new ();
		gtk_about_dialog_set_version (GTK_ABOUT_DIALOG (aboutdialog), "0.1rc");
		gtk_about_dialog_set_name (GTK_ABOUT_DIALOG (aboutdialog), "Rawstudio");
		gtk_about_dialog_set_comments(GTK_ABOUT_DIALOG (aboutdialog), "A raw image converter for GTK+/GNOME");
		gtk_about_dialog_set_website(GTK_ABOUT_DIALOG (aboutdialog), "http://rawstudio.org/");
		gtk_about_dialog_set_authors(GTK_ABOUT_DIALOG (aboutdialog), authors);
	}
	gtk_widget_show(aboutdialog);
	return;
}

void
gui_dialog_simple(gchar *title, gchar *message)
{
	GtkWidget *dialog, *label;

	dialog = gtk_dialog_new_with_buttons(title, NULL, GTK_DIALOG_NO_SEPARATOR,
		GTK_STOCK_OK, GTK_RESPONSE_NONE, NULL);
	label = gtk_label_new(message);
	g_signal_connect_swapped(dialog, "response",
		G_CALLBACK (gtk_widget_destroy), dialog);
	gtk_container_add(GTK_CONTAINER (GTK_DIALOG(dialog)->vbox), label);
	gtk_widget_show_all(dialog);
	return;
}

GtkWidget *
gui_make_menubar(RS_BLOB *rs, GtkWidget *window, GtkListStore *store, GtkWidget *iconbox, GtkWidget *toolbox)
{
	GtkItemFactoryEntry menu_items[] = {
		{ "/_File", NULL, NULL, 0, "<Branch>"},
		{ "/File/_Open", "<CTRL>O", gui_menu_open_callback, (gint) store, "<StockItem>", GTK_STOCK_OPEN},
		{ "/File/_Reload", "<CTRL>R", gui_menu_reload_callback, (gint) store, "<StockItem>", GTK_STOCK_REFRESH},
		{ "/File/_Quit", "<CTRL>Q", gtk_main_quit, 0, "<StockItem>", GTK_STOCK_QUIT},
		{ "/_Edit", NULL, NULL, 0, "<Branch>"},
		{ "/_Edit/_Preferences", NULL, gui_menu_preference_callback, (gint) rs, "<StockItem>", GTK_STOCK_PREFERENCES},
		{ "/_View", NULL, NULL, 0, "<Branch>"},
		{ "/_View/_Previous image", "<CTRL>Left", gui_menu_iconbar_previous_callback, 0, "<StockItem>", GTK_STOCK_GO_BACK},
		{ "/_View/_Next image", "<CTRL>Right", gui_menu_iconbar_next_callback, 0, "<StockItem>", GTK_STOCK_GO_FORWARD},
		{ "/_View/_Icon Box", "<CTRL>I", gui_menu_widget_visible_callback, (gint) iconbox},
		{ "/_View/_Tool Box", "<CTRL>T", gui_menu_widget_visible_callback, (gint) toolbox},
		{ "/_View/sep1", NULL, NULL, 0, "<Separator>"},
		{ "/_View/_Fullscreen", "F11", gui_menu_fullscreen_callback, (gint) window, "<StockItem>", GTK_STOCK_FULLSCREEN},
		{ "/_Help", NULL, NULL, 0, "<LastBranch>"},
		{ "/_Help/About", NULL, gui_about, 0, "<StockItem>", GTK_STOCK_ABOUT},
	};
	static gint nmenu_items = sizeof (menu_items) / sizeof (menu_items[0]);
	GtkItemFactory *item_factory;
	GtkAccelGroup *accel_group;

	accel_group = gtk_accel_group_new ();
	item_factory = gtk_item_factory_new (GTK_TYPE_MENU_BAR, "<main>", accel_group);
	gtk_item_factory_create_items (item_factory, nmenu_items, menu_items, NULL);
	gtk_window_add_accel_group (GTK_WINDOW (window), accel_group);
	return(gtk_item_factory_get_widget (item_factory, "<main>"));
}

int
gui_init(int argc, char **argv)
{
	GtkWidget *window;
	GtkWidget *scroller;
	GtkWidget *toolboxscroller;
	GtkWidget *viewport;
	GtkWidget *toolboxviewport;
	GtkWidget *align;
	GtkWidget *vbox;
	GtkWidget *pane;
	GtkWidget *toolbox;
	GtkWidget *iconbox;
	GtkListStore *store;
	GtkWidget *menubar;
	GdkColor color;
	RS_BLOB *rs;
	gchar *lwd;

	GError *error = NULL;
	GOptionContext* context;

	gtk_init(&argc, &argv);
	
	context = g_option_context_new ("");
	g_option_context_add_main_entries (context, entries, NULL);
	g_option_context_add_group (context, gtk_get_option_group (TRUE));
	g_option_context_parse (context, &argc, &argv, &error);
	g_option_context_free(context);

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

	store = gtk_list_store_new (NUM_COLUMNS, GDK_TYPE_PIXBUF, G_TYPE_STRING,
		G_TYPE_STRING, G_TYPE_POINTER, G_TYPE_INT);
	iconbox = make_iconbox(rs, store);
	g_signal_connect((gpointer) window, "window-state-event", G_CALLBACK(gui_fullscreen_callback), iconbox);


	// if -d og --dir is given, use that as path
	if (option_dir) {
	  lwd = option_dir;
	    } else {
	  lwd = rs_conf_get_string(CONF_LWD);
	  if (!lwd)
	    lwd = g_get_current_dir();
	}
	
	fill_model(store, lwd);
	g_free(lwd);

	menubar = gui_make_menubar(rs, window, store, iconbox, toolbox);
	gtk_box_pack_start (GTK_BOX (vbox), menubar, FALSE, TRUE, 0);

	gtk_box_pack_start (GTK_BOX (vbox), iconbox, FALSE, TRUE, 0);

	pane = gtk_hpaned_new ();
	gtk_box_pack_start (GTK_BOX (vbox), pane, TRUE, TRUE, 0);
	gtk_widget_show (pane);
	scroller = gtk_scrolled_window_new (NULL, NULL);
	gtk_paned_pack1 (GTK_PANED (pane), scroller, TRUE, TRUE);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scroller),
		GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_widget_show (scroller);

	toolboxscroller = gtk_scrolled_window_new (NULL, NULL);
	toolboxviewport = gtk_viewport_new (NULL, NULL);
	gtk_container_add (GTK_CONTAINER (toolboxscroller), toolboxviewport);
	gtk_container_add (GTK_CONTAINER (toolboxviewport), toolbox);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (toolboxscroller),
		GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
	gtk_paned_pack2 (GTK_PANED (pane), toolboxscroller, FALSE, TRUE);

	viewport = gtk_viewport_new (NULL, NULL);
	gtk_container_add (GTK_CONTAINER (scroller), viewport);

	align = gtk_alignment_new(0.5, 0.5, 0.0, 0.0);
	gtk_container_add (GTK_CONTAINER (viewport), align);

	rs->preview_drawingarea = gtk_drawing_area_new();
	
	g_signal_connect (GTK_OBJECT (rs->preview_drawingarea), "expose-event",
		GTK_SIGNAL_FUNC (drawingarea_expose), rs);
	g_signal_connect (GTK_OBJECT (rs->preview_drawingarea), "configure-event",
		GTK_SIGNAL_FUNC (drawingarea_configure), rs);
	g_signal_connect (G_OBJECT (rs->preview_drawingarea), "button_press_event",
		G_CALLBACK (gui_drawingarea_button), rs);
	g_signal_connect (G_OBJECT (rs->preview_drawingarea), "button_release_event",
		G_CALLBACK (gui_drawingarea_button), rs);
	gtk_widget_set_events(rs->preview_drawingarea, 0
		| GDK_BUTTON_PRESS_MASK
		| GDK_BUTTON_RELEASE_MASK
		| GDK_POINTER_MOTION_MASK);
	COLOR_BLACK(color);
	rs_conf_get_color("preview_background_color", &color);
	gtk_widget_modify_bg(viewport, GTK_STATE_NORMAL, &color);
	gtk_widget_modify_bg(rs->preview_drawingarea, GTK_STATE_NORMAL, &color);

	gtk_container_add (GTK_CONTAINER (align), (GtkWidget *) rs->preview_drawingarea);

	gtk_box_pack_start (GTK_BOX (vbox), GTK_WIDGET (statusbar), FALSE, TRUE, 0);

	gui_status_push("Ready");

	gtk_widget_show_all (window);
	gtk_main();
	return(0);
}
