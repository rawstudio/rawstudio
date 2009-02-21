/*
 * Copyright (C) 2006-2009 Anders Brander <anders@brander.dk> and 
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

#include <rawstudio.h>
#include <gtk/gtk.h>
#include <string.h> /* memset() */
#include <config.h>
#include "application.h"
#include "gtk-helper.h"
#include "gtk-interface.h"
#include "toolbox.h"
#include "conf_interface.h"
#include "gettext.h"
#include "rs-preview-widget.h"
#include "rs-histogram.h"
#include "rs-photo.h"

/* used for gui_adj_reset_callback() and others */
struct cb_carrier {
	RSSettings *settings;
	RSSettingsMask mask;
	gulong settings_signal_id;
	GObject *obj;
	gulong obj_signal_id; /* For signals FROM adj */
};

GtkLabel *infolabel;
static GtkWidget *toolbox;

static GtkWidget *gui_box(const gchar *title, GtkWidget *in, gboolean expanded);
static void gui_transform_rot90_clicked(GtkWidget *w, RS_BLOB *rs);
static void gui_transform_rot180_clicked(GtkWidget *w, RS_BLOB *rs);
static void gui_transform_rot270_clicked(GtkWidget *w, RS_BLOB *rs);
static void gui_transform_mirror_clicked(GtkWidget *w, RS_BLOB *rs);
static void gui_transform_flip_clicked(GtkWidget *w, RS_BLOB *rs);
static GtkWidget *gui_transform(RS_BLOB *rs, gboolean show);
static gboolean gui_adj_reset_callback(GtkWidget *widget, GdkEventButton *event, struct cb_carrier *rc);
static void gui_adj_value_callback(GtkAdjustment *adj, gpointer user_data);
static void curve_changed(GtkWidget *widget, gpointer user_data);
static GtkWidget *gui_make_scale_from_adj(RSSettings *settings, gulong settings_signal_id, GCallback cb, GtkAdjustment *adj, RSSettingsMask mask);
static void curve_context_callback_save(GtkMenuItem *menuitem, gpointer user_data);
static void curve_context_callback_open(GtkMenuItem *menuitem, gpointer user_data);
static void curve_context_callback_reset(GtkMenuItem *menuitem, gpointer user_data);
static void curve_context_callback(GtkWidget *widget, gpointer user_data);
static void gui_notebook_callback(GtkNotebook *notebook, GtkNotebookPage *page, guint page_num, RS_BLOB *rs);

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
	rs_photo_rotate(rs->photo, 1, 0.0);
}

static void
gui_transform_rot180_clicked(GtkWidget *w, RS_BLOB *rs)
{
	rs_photo_rotate(rs->photo, 2, 0.0);
}

static void
gui_transform_rot270_clicked(GtkWidget *w, RS_BLOB *rs)
{
	rs_photo_rotate(rs->photo, 3, 0.0);
}

static void
gui_transform_mirror_clicked(GtkWidget *w, RS_BLOB *rs)
{
	rs_photo_mirror(rs->photo);
}

static void
gui_transform_flip_clicked(GtkWidget *w, RS_BLOB *rs)
{
	rs_photo_flip(rs->photo);
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

	GtkWidget *flip_image = gtk_image_new_from_file(PACKAGE_DATA_DIR "/pixmaps/" PACKAGE "/transform_flip.png");
	GtkWidget *mirror_image = gtk_image_new_from_file(PACKAGE_DATA_DIR "/pixmaps/" PACKAGE "/transform_mirror.png");
	GtkWidget *rot90_image = gtk_image_new_from_file(PACKAGE_DATA_DIR "/pixmaps/" PACKAGE "/transform_90.png");
	GtkWidget *rot180_image = gtk_image_new_from_file(PACKAGE_DATA_DIR "/pixmaps/" PACKAGE "/transform_180.png");
	GtkWidget *rot270_image = gtk_image_new_from_file(PACKAGE_DATA_DIR "/pixmaps/" PACKAGE "/transform_270.png");

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

static gboolean
gui_adj_reset_callback(GtkWidget *widget, GdkEventButton *event, struct cb_carrier *rc)
{
	rs_settings_reset(rc->settings, rc->mask);
	return(TRUE);
}

static void
gui_adj_value_callback(GtkAdjustment *adj, gpointer user_data)
{
	struct cb_carrier *rc = (struct cb_carrier *) user_data;

	g_signal_handler_block(rc->settings, rc->settings_signal_id);
#define APPLY(lower, upper) \
do { \
	if (rc->mask & MASK_##upper) \
		rs_settings_set_##lower(rc->settings, gtk_adjustment_get_value(adj)); \
} while(0)
	APPLY(exposure, EXPOSURE);
	APPLY(saturation, SATURATION);
	APPLY(hue, HUE);
	APPLY(contrast, CONTRAST);
	APPLY(warmth, WARMTH);
	APPLY(tint, TINT);
	APPLY(sharpen, SHARPEN);
#undef APPLY
	g_signal_handler_unblock(rc->settings, rc->settings_signal_id);
}

static void
curve_changed(GtkWidget *widget, gpointer user_data)
{
	gfloat *knots;
	guint nknots;
	RSSettings *settings = RS_SETTINGS(user_data);

	rs_curve_widget_get_knots(RS_CURVE_WIDGET(widget), &knots, &nknots);

	rs_settings_set_curve_knots(settings, knots, nknots);

	g_free(knots);
}

static GtkWidget *
gui_make_scale_from_adj(RSSettings *settings, gulong settings_signal_id, GCallback cb, GtkAdjustment *adj, RSSettingsMask mask)
{
	GtkWidget *hscale, *box, *rimage, *revent;
	struct cb_carrier *rc = g_malloc(sizeof(struct cb_carrier));
	rc->obj = G_OBJECT(adj);
	rc->mask = mask;
	rc->settings = settings;
	rc->settings_signal_id = settings_signal_id;

	box = gtk_hbox_new(FALSE, 0);

	hscale = gtk_hscale_new((GtkAdjustment *) adj);
	rc->obj_signal_id = g_signal_connect(adj, "value_changed", cb, rc);
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
	gchar *dir;

	fc = gtk_file_chooser_dialog_new (_("Export File"), NULL,
		GTK_FILE_CHOOSER_ACTION_SAVE,
		GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
		GTK_STOCK_SAVE, GTK_RESPONSE_ACCEPT, NULL);
	gtk_dialog_set_default_response(GTK_DIALOG(fc), GTK_RESPONSE_ACCEPT);
#if GTK_CHECK_VERSION(2,8,0)
	gtk_file_chooser_set_do_overwrite_confirmation (GTK_FILE_CHOOSER (fc), TRUE);
#endif

	/* Set default directory */
	dir = g_build_filename(rs_confdir_get(), "curves", NULL);
	g_mkdir_with_parents(dir, 00755);
	gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER (fc), dir);
	g_free(dir);

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
	gchar *dir;

	fc = gtk_file_chooser_dialog_new (_("Open curve ..."), NULL,
		GTK_FILE_CHOOSER_ACTION_OPEN,
		GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
		GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT, NULL);
	gtk_dialog_set_default_response(GTK_DIALOG(fc), GTK_RESPONSE_ACCEPT);

	/* Set default directory */
	dir = g_build_filename(rs_confdir_get(), "curves", NULL);
	g_mkdir_with_parents(dir, 00755);
	gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER (fc), dir);
	g_free(dir);

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

	gulong handler = g_signal_handler_find(curve, G_SIGNAL_MATCH_FUNC, 0, 0, NULL, curve_changed, NULL);
	g_signal_handler_block(curve, handler);

	rs_curve_widget_reset(curve);
	rs_curve_widget_add_knot(curve, 0.0,0.0);
	g_signal_handler_unblock(curve, handler);
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
	rs_set_snapshot(rs, page_num);
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

typedef struct {
	GtkAdjustment *exposure;
	GtkAdjustment *saturation;
	GtkAdjustment *hue;
	GtkAdjustment *contrast;
	GtkAdjustment *warmth;
	GtkAdjustment *tint;
	GtkAdjustment *sharpen;
	RSCurveWidget *curve;
} ToolboxAdjusters;

static void
toolbox_settings_changed_cb(RSSettings *settings, RSSettingsMask mask, gpointer user_data)
{
	ToolboxAdjusters *adjusters = (ToolboxAdjusters *) user_data;

/* Programmers are lazy */
#define APPLY(lower, upper) do { \
	if (mask & MASK_##upper) \
	{\
		g_signal_handlers_block_matched(adjusters->lower, G_SIGNAL_MATCH_FUNC, 0, 0, NULL, gui_adj_value_callback, NULL); \
		gtk_adjustment_set_value(adjusters->lower, rs_settings_get_##lower(settings)); \
		g_signal_handlers_unblock_matched(adjusters->lower, G_SIGNAL_MATCH_FUNC, 0, 0, NULL, gui_adj_value_callback, NULL); \
	} \
} while(0)
	APPLY(exposure, EXPOSURE);
	APPLY(saturation, SATURATION);
	APPLY(hue, HUE);
	APPLY(contrast, CONTRAST);
	APPLY(warmth, WARMTH);
	APPLY(tint, TINT);
	APPLY(sharpen, SHARPEN);
#undef APPLY

	if (mask & MASK_CURVE)
	{
		gfloat *knots = rs_settings_get_curve_knots(settings);
		const gint nknots = rs_settings_get_curve_nknots(settings);

		/* Block handlers during update */
		g_signal_handlers_block_matched(adjusters->curve, G_SIGNAL_MATCH_FUNC, 0, 0, NULL, curve_changed, NULL); \
		rs_curve_widget_set_knots(adjusters->curve, knots, nknots);
		g_signal_handlers_unblock_matched(adjusters->curve, G_SIGNAL_MATCH_FUNC, 0, 0, NULL, curve_changed, NULL); \

		g_free(knots);
	}
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
	GtkWidget **toolbox_sharpen = g_new(GtkWidget *, 3);
	GtkWidget *toolbox_transform;
	GtkWidget *toolbox_hist;
	GtkWidget *toolboxscroller;
	GtkWidget *toolboxviewport;
	gint n;
	gboolean show;
	gint height;

	toolbox_label[0] = gtk_label_new(_(" A "));
	toolbox_label[1] = gtk_label_new(_(" B "));
	toolbox_label[2] = gtk_label_new(_(" C "));
	notebook = gtk_notebook_new();

	for(n = 0; n < 3; n++) {
		GtkWidget *embed;
		gulong settings_signal_id;
		tbox[n] = gtk_vbox_new (FALSE, 0);
		gtk_widget_show (tbox[n]);

		ToolboxAdjusters *adjusters = g_new0(ToolboxAdjusters, 1);

		settings_signal_id = g_signal_connect(rs->settings[n], "settings-changed", G_CALLBACK(toolbox_settings_changed_cb), adjusters);

#define SLIDER(lower, upper, label, floor, ceiling, step, page) \
	adjusters->lower = GTK_ADJUSTMENT(gtk_adjustment_new(rs_settings_get_##lower(rs->settings[n]), floor, ceiling, step, page, 0.0)); \
	rs_conf_get_boolean_with_default(CONF_SHOW_TOOLBOX_##upper, &show, DEFAULT_CONF_SHOW_TOOLBOX_##upper); \
	embed = gui_make_scale_from_adj(rs->settings[n], settings_signal_id, G_CALLBACK(gui_adj_value_callback), adjusters->lower, MASK_##upper); \
	toolbox_##lower[n] = gui_box(label, embed, show); \
	gtk_box_pack_start (GTK_BOX (tbox[n]), toolbox_##lower[n], FALSE, FALSE, 0); \
	g_signal_connect_after(toolbox_##lower[n], "activate", G_CALLBACK(gui_expander_toggle_callback), toolbox_##lower); \
	g_signal_connect_after(toolbox_##lower[n], "activate", G_CALLBACK(gui_expander_save_status_callback), CONF_SHOW_TOOLBOX_##upper) \

		SLIDER(exposure, EXPOSURE, _("Exposure"), -3.0, 3.0, 0.1, 0.5);
		SLIDER(saturation, SATURATION, _("Saturation"), 0.0, 3.0, 0.1, 0.5);
		SLIDER(hue, HUE, _("Hue"), -180.0, 180.0, 0.1, 30.0);
		SLIDER(contrast, CONTRAST, _("Contrast"), 0.0, 3.0, 0.1, 0.5);

		/* White balance */
		GtkWidget *box = gtk_vbox_new (FALSE, 0);
		rs_conf_get_boolean_with_default(CONF_SHOW_TOOLBOX_WARMTH, &show, DEFAULT_CONF_SHOW_TOOLBOX_WARMTH);

		/* Warmth slider */
		adjusters->warmth = GTK_ADJUSTMENT(gtk_adjustment_new(rs_settings_get_warmth(rs->settings[n]), -2.0, 2.0, 0.1, 0.5, 0.0));
		embed = gui_make_scale_from_adj(rs->settings[n], settings_signal_id, G_CALLBACK(gui_adj_value_callback), adjusters->warmth, MASK_WARMTH); \
		gtk_box_pack_start (GTK_BOX (box), embed, FALSE, FALSE, 0);

		/* Tint slider */
		adjusters->tint = GTK_ADJUSTMENT(gtk_adjustment_new(rs_settings_get_tint(rs->settings[n]), -2.0, 2.0, 0.1, 0.5, 0.0));
		embed = gui_make_scale_from_adj(rs->settings[n], settings_signal_id, G_CALLBACK(gui_adj_value_callback), adjusters->tint, MASK_TINT);
		gtk_box_pack_start (GTK_BOX (box), embed, FALSE, FALSE, 0);

		/* Box it! */
		toolbox_warmth[n] = gui_box(_("Warmth/tint"), box, show);

		gtk_box_pack_start (GTK_BOX (tbox[n]), toolbox_warmth[n], FALSE, FALSE, 0); \
		g_signal_connect_after(toolbox_warmth[n], "activate", G_CALLBACK(gui_expander_toggle_callback), toolbox_warmth);
		g_signal_connect_after(toolbox_warmth[n], "activate", G_CALLBACK(gui_expander_save_status_callback), CONF_SHOW_TOOLBOX_WARMTH);

		SLIDER(sharpen, SHARPEN, _("Sharpen"), 0.0, 10.0, 0.1, 0.5);

		/* Curve */
		gfloat *knots;
		gint nknots;
		rs->curve[n] = rs_curve_widget_new();
		adjusters->curve = RS_CURVE_WIDGET(rs->curve[n]);

		/* Initialize curve with knots from RSSettings */
		nknots = rs_settings_get_curve_nknots(rs->settings[n]);
		knots = rs_settings_get_curve_knots(rs->settings[n]);
		rs_curve_widget_set_knots(RS_CURVE_WIDGET(rs->curve[n]), knots, nknots);
		g_free(knots);

		rs_conf_get_boolean_with_default(CONF_SHOW_TOOLBOX_CURVE, &show, DEFAULT_CONF_SHOW_TOOLBOX_CURVE);
		gtk_widget_set_size_request(rs->curve[n], 64, 64);
		g_signal_connect(rs->curve[n], "changed", G_CALLBACK(curve_changed), rs->settings[n]);
		g_signal_connect(rs->curve[n], "right-click", G_CALLBACK(curve_context_callback), rs);
		toolbox_curve[n] = gui_box(_("Curve"), rs->curve[n], show);
		gtk_box_pack_start (GTK_BOX (tbox[n]), toolbox_curve[n], TRUE, FALSE, 0);
		g_signal_connect_after(toolbox_curve[n], "activate", G_CALLBACK(gui_expander_toggle_callback), toolbox_curve);
		g_signal_connect_after(toolbox_curve[n], "activate", G_CALLBACK(gui_expander_save_status_callback), CONF_SHOW_TOOLBOX_CURVE);

		/* Append tab */
		gtk_notebook_append_page(GTK_NOTEBOOK(notebook), tbox[n], toolbox_label[n]);
	}
	g_signal_connect(notebook, "switch-page", G_CALLBACK(gui_notebook_callback), rs);

	toolbox = gtk_vbox_new (FALSE, 0);
	gtk_box_pack_start (GTK_BOX (toolbox), notebook, FALSE, FALSE, 0);
	rs_conf_get_boolean_with_default(CONF_SHOW_TOOLBOX_TRANSFORM, &show, DEFAULT_CONF_SHOW_TOOLBOX_TRANSFORM);
	toolbox_transform = gui_transform(rs, show);
	gtk_box_pack_start (GTK_BOX (toolbox), toolbox_transform, FALSE, FALSE, 0);
	g_signal_connect_after(toolbox_transform, "activate", G_CALLBACK(gui_expander_save_status_callback), CONF_SHOW_TOOLBOX_TRANSFORM);

	if (!rs_conf_get_integer(CONF_HISTHEIGHT, &height))
		height = 70;
	rs->histogram = rs_histogram_new();
	gtk_widget_set_size_request(rs->histogram, 64, height);
	rs_conf_get_boolean_with_default(CONF_SHOW_TOOLBOX_HIST, &show, DEFAULT_CONF_SHOW_TOOLBOX_HIST);
	toolbox_hist = gui_box(_("Histogram"), rs->histogram, show);
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
