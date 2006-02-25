#include <gtk/gtk.h>
#include <math.h> /* pow() */
#include <string.h> /* memset() */
#include "color.h"
#include "matrix.h"
#include "dcraw_api.h"

#define PITCH(width) ((((width)+31)/32)*32)
#define GETVAL(adjustment) \
	gtk_adjustment_get_value((GtkAdjustment *) adjustment)
#define SETVAL(adjustment, value) \
	gtk_adjustment_set_value((GtkAdjustment *) adjustment, value)

enum {
	FILE_UNKN,
	FILE_RAW
};

const gchar *rawsuffix[] = {"cr2", "crw", "nef", NULL};

enum { 
	PIXBUF_COLUMN,
	TEXT_COLUMN,
	DATA_COLUMN
};

typedef struct {
	gboolean in_use;
	guint w;
	guint h;
	gint pitch;
	guint channels;
	gushort *pixels[4];
	dcraw_data *raw;
	GtkObject *exposure;
	GtkObject *gamma;
	GtkObject *saturation;
	GtkObject *hue;
	GtkObject *rgb_mixer[3];
	GtkObject *contrast;
	GtkObject *scale;
	guint vis_scale;
	guint vis_w;
	guint vis_h;
	guint vis_pitch;
	gushort *vis_pixels[4];
	guint vis_histogram[4][256];
	GdkPixbuf *vis_pixbuf;
	GtkWidget *vis_image;
	GtkWidget *files; /* ugly hack */
} RS_IMAGE;

guchar gammatable[65536];
gdouble gammavalue = 0.0;
guchar previewtable[3][65536];

void
update_gammatable(const double g)
{
	gdouble res,nd;
	gint n;
	if (gammavalue!=g)
	for(n=0;n<65536;n++)
	{
		nd = ((double) n) / 65535.0;
		res = pow(nd, 1.0/g);
		gammatable[n]= (unsigned char) MIN(255,(res*255.0));
	}
#if 0
	for(n=0;n<100;n++)
		gammatable[n]= 255;
	for(n=65536;n<65536;n++)
		gammatable[n]= 0;
#endif
	gammavalue = g;
	return;
}

void
update_previewtable(RS_IMAGE *rs)
{
	gint n, c;
	gint multiply;
	const gint offset = (gint) 32767.5 * (1.0-GETVAL(rs->contrast));
	const guint ex = (guint) ((pow(2.0, GETVAL(rs->exposure)))*GETVAL(rs->contrast) * 128.0);

	for(c=0;c<3;c++)
	{
		multiply = (gint) (GETVAL(rs->rgb_mixer[c]) * 128.0) * ex;
		for(n=0;n<65536;n++)
			previewtable[c][n] = gammatable[CLAMP65535(((n*multiply)>>14)+offset)];
	}
}

void
rs_debug(RS_IMAGE *rs)
{
	guint c;
	printf("rs: %d\n", (guint) rs);
	printf("rs->w: %d\n", rs->w);
	printf("rs->h: %d\n", rs->h);
	printf("rs->pitch: %d\n", rs->pitch);
	printf("rs->channels: %d\n", rs->channels);
	for(c=0;c<rs->channels;c++)
		printf("rs->pixels[%d]: %d\n", c, (guint) rs->pixels[c]);
	printf("rs->vis_w: %d\n", rs->vis_w);
	printf("rs->vis_h: %d\n", rs->vis_h);
	printf("rs->vis_pitch: %d\n", rs->vis_pitch);
	printf("rs->vis_scale: %d\n", rs->vis_scale);
	for(c=0;c<3;c++)
		printf("rs->vis_pixels[%d]: %d\n", c, (guint) rs->vis_pixels[c]);
	printf("\n");
	return;
}

void
update_scaled(RS_IMAGE *rs)
{
	guint c,y,x;
	guint srcoffset, destoffset;

	if (!rs->in_use) return;

	/* 16 bit downscaled */
	rs->vis_scale = GETVAL(rs->scale);
	if (rs->vis_w != rs->w/rs->vis_scale) /* do we need to? */
	{
		if (rs->vis_w!=0) /* old allocs? */
		{
			for(c=0;c<rs->channels;c++)
				g_free(rs->vis_pixels[c]);
			rs->vis_w=0;
			rs->vis_h=0;
			rs->vis_pitch=0;
		}
		rs->vis_w = rs->w/rs->vis_scale;
		rs->vis_h = rs->h/rs->vis_scale;
		rs->vis_pitch = PITCH(rs->vis_w);
		for(c=0;c<rs->channels;c++)
		{
			rs->vis_pixels[c] = (gushort *) g_malloc(rs->vis_pitch*rs->vis_h*sizeof(gushort));

			for(y=0; y<rs->vis_h; y++)
			{
				destoffset = y*rs->vis_pitch;
				srcoffset = y*rs->vis_scale*rs->pitch;
				for(x=0; x<rs->vis_w; x++)
				{
					rs->vis_pixels[c][destoffset] = rs->pixels[c][srcoffset];
					destoffset++;
					srcoffset+=rs->vis_scale;
				}
			}
		}
		rs->vis_pixbuf = gdk_pixbuf_new(GDK_COLORSPACE_RGB, FALSE, 8, rs->vis_w, rs->vis_h);
		gtk_image_set_from_pixbuf((GtkImage *) rs->vis_image, rs->vis_pixbuf);
		g_object_unref(rs->vis_pixbuf);
	}
	return;
}

void
update_preview(RS_IMAGE *rs)
{
	RS_MATRIX4 mat;
	RS_MATRIX4Int mati;
	gint rowstride, x, y, offset, destoffset;
	register gint r,g,b;
	guchar *pixels;

	if(!rs->in_use) return;

	SETVAL(rs->scale, floor(GETVAL(rs->scale))); // we only do integer scaling
	update_scaled(rs);
	update_gammatable(GETVAL(rs->gamma));
	update_previewtable(rs);

	matrix4_identity(&mat);
	matrix4_color_saturate(&mat, GETVAL(rs->saturation));
	matrix4_color_hue(&mat, GETVAL(rs->hue));
	matrix4_to_matrix4int(&mat, &mati);

	pixels = gdk_pixbuf_get_pixels(rs->vis_pixbuf);
	rowstride = gdk_pixbuf_get_rowstride(rs->vis_pixbuf);
	memset(rs->vis_histogram, 0, sizeof(guint)*3*256); // reset histogram
	for(y=0 ; y<rs->vis_h ; y++)
	{
		offset = y*rs->vis_pitch;
		destoffset = rowstride*y;
		for(x=0 ; x<rs->vis_w ; x++)
		{
			r = (rs->vis_pixels[R][offset]*mati.coeff[0][0]
				+ rs->vis_pixels[G][offset]*mati.coeff[0][1]
				+ rs->vis_pixels[B][offset]*mati.coeff[0][2])>>MATRIX_RESOLUTION;
			g = (rs->vis_pixels[R][offset]*mati.coeff[1][0]
				+ rs->vis_pixels[G][offset]*mati.coeff[1][1]
				+ rs->vis_pixels[B][offset]*mati.coeff[1][2])>>MATRIX_RESOLUTION;
			b = (rs->vis_pixels[R][offset]*mati.coeff[2][0]
				+ rs->vis_pixels[G][offset]*mati.coeff[2][1]
				+ rs->vis_pixels[B][offset]*mati.coeff[2][2])>>MATRIX_RESOLUTION;

			pixels[destoffset] = previewtable[R][CLAMP65535(r)];
			rs->vis_histogram[R][pixels[destoffset++]]++;
			pixels[destoffset] = previewtable[G][CLAMP65535(g)];
			rs->vis_histogram[G][pixels[destoffset++]]++;
			pixels[destoffset] = previewtable[B][CLAMP65535(b)];
			rs->vis_histogram[B][pixels[destoffset++]]++;
			offset++; /* increment offset by one */
		}
	}
	gtk_image_set_from_pixbuf((GtkImage *) rs->vis_image, rs->vis_pixbuf);
	return;
}	

gboolean
update_preview_callback(GtkAdjustment *caller, RS_IMAGE *rs)
{
	update_preview(rs);
	return(FALSE);
}

void
rs_reset(RS_IMAGE *rs)
{
	guint c;
	gtk_adjustment_set_value((GtkAdjustment *) rs->exposure, 0.0);
	gtk_adjustment_set_value((GtkAdjustment *) rs->gamma, 2.2);
	gtk_adjustment_set_value((GtkAdjustment *) rs->saturation, 1.0);
	gtk_adjustment_set_value((GtkAdjustment *) rs->hue, 0.0);
	gtk_adjustment_set_value((GtkAdjustment *) rs->contrast, 1.0);
	for(c=0;c<3;c++)
		gtk_adjustment_set_value((GtkAdjustment *) rs->rgb_mixer[c], 1.0);
	rs->vis_scale = 2;
	rs->vis_w = 0;
	rs->vis_h = 0;
	rs->vis_pitch = 0;
	return;
}

void
rs_free_raw(RS_IMAGE *rs)
{
	dcraw_close(rs->raw);
	g_free(rs->raw);
	rs->raw = NULL;
}

void
rs_free(RS_IMAGE *rs)
{
	if (rs->in_use)
	{
		g_free(rs->pixels[R]);
		g_free(rs->pixels[G]);
		g_free(rs->pixels[G2]);
		if (rs->channels==4) g_free(rs->pixels[B]);
		rs->channels=0;
		rs->w=0;
		rs->h=0;
		if (rs->raw!=NULL)
			rs_free_raw(rs);
		rs->in_use=FALSE;
	}
}

void
rs_alloc(RS_IMAGE *rs, const guint width, const guint height, const guint channels)
{
	if(rs->in_use)
		rs_free(rs);
	rs->w = width;
	rs->pitch = PITCH(width);
	rs->h = height;
	rs->channels = channels;
	rs->pixels[R] = (gushort *) g_malloc(rs->pitch*rs->h*sizeof(unsigned short));
	rs->pixels[G] = (gushort *) g_malloc(rs->pitch*rs->h*sizeof(unsigned short));
	rs->pixels[G2] = (gushort *) g_malloc(rs->pitch*rs->h*sizeof(unsigned short));
	if (rs->channels==4) rs->pixels[B] = (gushort *) g_malloc(rs->pitch*rs->h*sizeof(unsigned short));
	rs->in_use = TRUE;
}

GtkObject *
make_adj(RS_IMAGE *rs, double value, double min, double max, double step, double page)
{
	GtkObject *adj;
	adj = gtk_adjustment_new(value, min, max, step, page, 0.0);
	gtk_signal_connect(GTK_OBJECT(adj), "value_changed",
		G_CALLBACK(update_preview_callback), rs);
	return(adj);
}

RS_IMAGE *
rs_new()
{
	RS_IMAGE *rs;
	guint c;
	rs = g_malloc(sizeof(RS_IMAGE));

	rs->exposure = make_adj(rs, 0.0, -2.0, 2.0, 0.1, 0.5);
	rs->gamma = make_adj(rs, 2.2, 0.0, 3.0, 0.1, 0.5);
	rs->saturation = make_adj(rs, 1.0, 0.0, 3.0, 0.1, 0.5);
	rs->hue = make_adj(rs, 0.0, 0.0, 360.0, 0.5, 30.0);
	rs->contrast = make_adj(rs, 1.0, 0.0, 3.0, 0.1, 0.1);
	rs->scale = make_adj(rs, 2.0, 1.0, 5.0, 1.0, 1.0);
	for(c=0;c<3;c++)
		rs->rgb_mixer[c] = make_adj(rs, 0.0, 0.0, 5.0, 0.1, 0.5);
	rs->raw = NULL;
	rs->in_use = FALSE;
	rs_reset(rs);
	return(rs);
}

void
rs_load_raw_from_memory(RS_IMAGE *rs)
{
	gushort *src = (gushort *) rs->raw->raw.image;
	gint mul[4];
	guint x,y;

	mul[R] = (int) (rs->raw->pre_mul[R] * 65536.0);
	mul[G] = (int) (rs->raw->pre_mul[G] * 65536.0);
	mul[B] = (int) (rs->raw->pre_mul[B] * 65536.0);
	mul[G2] = (int) (rs->raw->pre_mul[G] * 65536.0);

	for (y=0; y<rs->raw->raw.height; y++)
	{
		for (x=0; x<rs->raw->raw.width; x++)
		{
			rs->pixels[R][y*rs->pitch+x] = CLAMP65535(((src[(y*rs->w+x)*4+R]
				- rs->raw->black)*mul[R])>>12);
			rs->pixels[G][y*rs->pitch+x] = CLAMP65535(((src[(y*rs->w+x)*4+G]
				- rs->raw->black)*mul[G])>>12);
			rs->pixels[B][y*rs->pitch+x] = CLAMP65535(((src[(y*rs->w+x)*4+B]
				- rs->raw->black)*mul[B])>>12);
			if (rs->channels==4) rs->pixels[G2][y*rs->pitch+x] = CLAMP65535(((src[(y*rs->w+x)*4+G2]
				- rs->raw->black)*mul[G2])>>12);
		}
	}
	rs->in_use=TRUE;
	return;
}

void
rs_load_raw_from_file(RS_IMAGE *rs, const gchar *filename)
{
	dcraw_data *raw;

	if (rs->raw!=NULL) rs_free_raw(rs);
	raw = (dcraw_data *) g_malloc(sizeof(dcraw_data));
	dcraw_open(raw, (char *) filename);
	dcraw_load_raw(raw);
	rs_alloc(rs, raw->raw.width, raw->raw.height, 4);
	rs->raw = raw;
	rs_load_raw_from_memory(rs);
	update_preview(rs);
	return;
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

GtkWidget *
gui_rgb_mixer(RS_IMAGE *rs)
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

static void
gui_reset_clicked(GtkWidget *w, RS_IMAGE *rs)
{
	rs_reset(rs);
}

GtkWidget *
gui_reset(RS_IMAGE *rs)
{
	GtkWidget *button;
	button = gtk_button_new_with_mnemonic ("Reset");
	g_signal_connect ((gpointer) button, "clicked", G_CALLBACK (gui_reset_clicked), rs);
	gtk_widget_show (button);
	return(button);
}

static void
save_file_clicked(GtkWidget *w, RS_IMAGE *rs)
{
	gdk_pixbuf_save(rs->vis_pixbuf, "output.png", "png", NULL, NULL);
	return;
}

GtkWidget *
save_file(RS_IMAGE *rs)
{
	GtkWidget *button;
	button = gtk_button_new_with_mnemonic ("Save");
	g_signal_connect ((gpointer) button, "clicked", G_CALLBACK (save_file_clicked), rs);
	gtk_widget_show (button);
	return(button);
}

GtkWidget *
make_toolbox(RS_IMAGE *rs)
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
	gtk_box_pack_start (GTK_BOX (toolbox), gui_reset(rs), FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (toolbox), save_file(rs), FALSE, FALSE, 0);
	return(toolbox);
}

static void
open_file_ok(GtkWidget *w, RS_IMAGE *rs)
{
	rs_load_raw_from_file(rs, gtk_file_selection_get_filename (GTK_FILE_SELECTION (rs->files)));
	return;
}

gboolean
open_file(GtkWidget *caller, RS_IMAGE *rs)
{
	rs->files = gtk_file_selection_new ("Open file ...");
	g_signal_connect (G_OBJECT (GTK_FILE_SELECTION (rs->files)->ok_button),
		"clicked", G_CALLBACK (open_file_ok), rs);
	gtk_widget_show (rs->files);
	return(FALSE);
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

GdkPixbuf *
make_thumbnail(const gchar *filename)
{
	RS_IMAGE *rs;
	rs = rs_new();
	rs_load_raw_from_file(rs, filename);
	
	rs_free(rs);
}

void
icon_activated(GtkIconView *iconview, RS_IMAGE *rs)
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
		rs_load_raw_from_file(rs, name);
		rs_reset(rs);
		update_preview(rs);
		g_free(name);
	}
}

GtkWidget *
make_iconbox(RS_IMAGE *rs, GtkListStore *store)
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
main(int argc, char **argv)
{
	GtkWidget *window;
	GtkWidget *scroller;
	GtkWidget *viewport;
	GtkWidget *vbox;
	GtkWidget *pane;
	GtkWidget *toolbox;
	GtkWidget *button;
	GtkWidget *iconbox;
	GtkListStore *store;
	RS_IMAGE *rs;

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
	
	button = gtk_button_new_with_mnemonic ("Open ...");
	g_signal_connect ((gpointer) button, "clicked", G_CALLBACK (open_file), rs);
	gtk_widget_show (button);

	gtk_box_pack_start (GTK_BOX (vbox), button, FALSE, FALSE, 0);

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

	gtk_paned_pack2 (GTK_PANED (pane), toolbox, TRUE, TRUE);

	viewport = gtk_viewport_new (NULL, NULL);
	gtk_container_add (GTK_CONTAINER (scroller), viewport);

	rs->vis_image = gtk_image_new();
	gtk_container_add (GTK_CONTAINER (viewport), rs->vis_image);

	gtk_widget_show_all (window);
	gtk_main();
	return(0);
}
