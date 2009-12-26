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

/* Output plugin tmpl version 1 */

/* 
   TODO:
   - move rs_conf_* to librawstudio - this needs RS_FILETYPE from application.h. Should/can we move this to rs-filetypes.h?
   - decide if rawstudio can be dependent on libflickcurl. Will this plugin be build on dependency satisfiction or --with-output-flickr option.
*/

#include <rawstudio.h>
#include <gettext.h>
#include "config.h"
#include "output-facebook.h"
#include <unistd.h>
#include <string.h>
#include "facebook.h"

/* Ugly HACK - conf_interface.c|h needs to be ported to librawstudio */
gchar *rs_conf_get_string (const gchar * name);
gboolean rs_conf_set_string (const gchar * name, const gchar * value);

#define RS_TYPE_FACEBOOK (rs_facebook_type)
#define RS_FACEBOOK(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), RS_TYPE_FACEBOOK, RSFacebook))
#define RS_FACEBOOK_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), RS_TYPE_FACEBOOK, RSFacebookClass))
#define RS_IS_FACEBOOK(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), RS_TYPE_FACEBOOK))

typedef struct _RSFacebook RSFacebook;
typedef struct _RSFacebookClass RSFacebookClass;

struct _RSFacebook
{
	RSOutput parent;

	gint quality;
	gchar *filename; /* Required for a output plugin - not in use */
	gchar *caption;
};

struct _RSFacebookClass
{
	RSOutputClass parent_class;
};

RS_DEFINE_OUTPUT (rs_facebook, RSFacebook)
enum
{
	PROP_0,
	PROP_JPEG_QUALITY,
	PROP_FILENAME, /* Required for a output plugin - not in use */
	PROP_CAPTION
};

static void get_property (GObject * object, guint property_id, GValue * value, GParamSpec * pspec);
static void set_property (GObject * object, guint property_id, const GValue * value, GParamSpec * pspec);
static gboolean execute (RSOutput * output, RSFilter * filter);

G_MODULE_EXPORT void rs_plugin_load (RSPlugin * plugin)
{
	rs_facebook_get_type (G_TYPE_MODULE (plugin));
}

static void
rs_facebook_class_init (RSFacebookClass * klass)
{
	RSOutputClass *output_class = RS_OUTPUT_CLASS (klass);
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	
	object_class->get_property = get_property;
	object_class->set_property = set_property;

	g_object_class_install_property (object_class,
					 PROP_JPEG_QUALITY,
					 g_param_spec_int ("quality",
							   "JPEG Quality",
							   _("JPEG Quality"), 10,
							   100, 90,
							   G_PARAM_READWRITE));
	
	g_object_class_install_property (object_class,
					 PROP_CAPTION, g_param_spec_string ("caption",
									  "caption",
									  _("Caption"),
									  NULL,
									  G_PARAM_READWRITE));

	g_object_class_install_property (object_class, /* Required for a output plugin - not in use */
					 PROP_FILENAME, g_param_spec_string ("filename",
									  "filename",
									  "Filename",
									  NULL,
									  G_PARAM_READWRITE));

	output_class->execute = execute;
	output_class->display_name = _("Upload photo to Facebook");
}

static void
rs_facebook_init (RSFacebook * facebook)
{
	facebook->quality = 90;
}

static void
get_property (GObject * object, guint property_id, GValue * value, GParamSpec * pspec)
{
	RSFacebook *facebook = RS_FACEBOOK (object);

	switch (property_id)
	{
	case PROP_JPEG_QUALITY:
		g_value_set_int (value, facebook->quality);
		break;
	case PROP_FILENAME: /* Required for a output plugin - not in use */
		g_value_set_string (value, facebook->filename);
		break;
	case PROP_CAPTION:
		g_value_set_string (value, facebook->caption);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
	}
}

static void
set_property (GObject * object, guint property_id, const GValue * value, GParamSpec * pspec)
{
	RSFacebook *facebook = RS_FACEBOOK (object);

	switch (property_id)
	{
	case PROP_JPEG_QUALITY:
		facebook->quality = g_value_get_int (value);
		break;
	case PROP_FILENAME: /* Required for a output plugin - not in use */
		facebook->filename = g_value_dup_string (value);
		break;
	case PROP_CAPTION:
		facebook->caption = g_value_dup_string (value);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
	}
}

GtkWidget *
gui_dialog_make_from_widget (const gchar * stock_id, gchar * primary_text, GtkWidget * widget)
{
	GtkWidget *dialog, *image, *hhbox, *vvbox;
	GtkWidget *primary_label;
	gchar *str;

	image = gtk_image_new_from_stock (stock_id, GTK_ICON_SIZE_DIALOG);
	gtk_misc_set_alignment (GTK_MISC (image), 0.5, 0.0);
	dialog = gtk_dialog_new ();
	gtk_container_set_border_width (GTK_CONTAINER (dialog), 5);
	gtk_box_set_spacing (GTK_BOX (GTK_DIALOG (dialog)->vbox), 14);
	gtk_window_set_resizable (GTK_WINDOW (dialog), FALSE);
	gtk_dialog_set_has_separator (GTK_DIALOG (dialog), FALSE);
	gtk_window_set_title (GTK_WINDOW (dialog), "");
	gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);
	gtk_window_set_destroy_with_parent (GTK_WINDOW (dialog), TRUE);

	primary_label = gtk_label_new (NULL);
	gtk_label_set_line_wrap (GTK_LABEL (primary_label), TRUE);
	gtk_label_set_use_markup (GTK_LABEL (primary_label), TRUE);
	gtk_misc_set_alignment (GTK_MISC (primary_label), 0.0, 0.5);
	gtk_label_set_selectable (GTK_LABEL (primary_label), TRUE);
	str = g_strconcat ("<span weight=\"bold\" size=\"larger\">", primary_text, "</span>", NULL);
	gtk_label_set_markup (GTK_LABEL (primary_label), str);
	g_free (str);

	hhbox = gtk_hbox_new (FALSE, 12);
	gtk_container_set_border_width (GTK_CONTAINER (hhbox), 5);
	gtk_box_pack_start (GTK_BOX (hhbox), image, FALSE, FALSE, 0);
	vvbox = gtk_vbox_new (FALSE, 12);
	gtk_box_pack_start (GTK_BOX (hhbox), vvbox, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (vvbox), primary_label, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (vvbox), widget, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox), hhbox, FALSE, FALSE, 0);

	return (dialog);
}

GtkWidget *
gui_dialog_make_from_text (const gchar * stock_id, gchar * primary_text, gchar * secondary_text)
{
	GtkWidget *secondary_label;

	secondary_label = gtk_label_new (NULL);
	gtk_label_set_line_wrap (GTK_LABEL (secondary_label), TRUE);
	gtk_label_set_use_markup (GTK_LABEL (secondary_label), TRUE);
	gtk_misc_set_alignment (GTK_MISC (secondary_label), 0.0, 0.5);
	gtk_label_set_selectable (GTK_LABEL (secondary_label), TRUE);
	gtk_label_set_markup (GTK_LABEL (secondary_label), secondary_text);

	return (gui_dialog_make_from_widget(stock_id, primary_text, secondary_label));
}

gboolean
auth_popup(gchar *text, gchar *auth_url)
{
	/* FIXME: move this to librawstudio */

	gdk_threads_enter ();
	GtkWidget *auth_dialog = gtk_dialog_new ();
	gtk_window_set_title (GTK_WINDOW (auth_dialog), "Rawstudio");
	gtk_container_set_border_width (GTK_CONTAINER (auth_dialog), 4);
	gtk_dialog_set_has_separator (GTK_DIALOG (auth_dialog), FALSE);

	GtkWidget *vbox = GTK_DIALOG (auth_dialog)->vbox;

	GtkWidget *textlabel = gtk_label_new(text);
	gtk_label_set_line_wrap (GTK_LABEL (textlabel), TRUE);

	gtk_box_pack_start (GTK_BOX (vbox), textlabel, TRUE, TRUE, 4);

	GtkWidget *table = gtk_table_new (2, 2, FALSE);

	GtkWidget *step1label = gtk_label_new (_("Step 1:"));
	GtkWidget *step2label = gtk_label_new (_("Step 2:"));

	GtkWidget *link = gtk_link_button_new_with_label (auth_url, _("Authenticate Rawstudio"));

	GtkWidget *hbox = gtk_hbox_new (FALSE, 4);

	GtkWidget *cancelbutton = gtk_button_new_from_stock (GTK_STOCK_CANCEL);
	GtkWidget *acceptbutton = gtk_button_new_from_stock (GTK_STOCK_GO_FORWARD);

	gtk_box_pack_start (GTK_BOX (hbox), cancelbutton, TRUE, TRUE, 4);
	gtk_box_pack_start (GTK_BOX (hbox), acceptbutton, TRUE, TRUE, 4);

	gtk_dialog_add_action_widget (GTK_DIALOG (auth_dialog), cancelbutton, GTK_RESPONSE_CANCEL);
	gtk_dialog_add_action_widget (GTK_DIALOG (auth_dialog), acceptbutton, GTK_RESPONSE_ACCEPT);

	gtk_box_pack_start (GTK_BOX (vbox), table, FALSE, FALSE, 4);

	gtk_table_attach_defaults (GTK_TABLE (table), step1label, 0, 1, 0, 1);
	gtk_table_attach_defaults (GTK_TABLE (table), step2label, 0, 1, 1, 2);

	gtk_table_attach_defaults (GTK_TABLE (table), link, 1, 2, 0, 1);
	gtk_table_attach_defaults (GTK_TABLE (table), hbox, 1, 2, 1, 2);

	gtk_widget_show_all (auth_dialog);
	gint response = gtk_dialog_run (GTK_DIALOG (auth_dialog));

	gtk_widget_destroy (auth_dialog);

	gdk_threads_leave ();

	if (response == GTK_RESPONSE_ACCEPT)
		return TRUE;
	else
		return FALSE;
}

static gboolean
execute (RSOutput * output, RSFilter * filter)
{
	RSFacebook *facebook = RS_FACEBOOK (output);

	if (!facebook_init(FACEBOOK_API_KEY, FACEBOOK_SECRET_KEY, FACEBOOK_SERVER))
		return FALSE;

	gchar *session = rs_conf_get_string("facebook_session");

	if (session)
		facebook_set_session(session);

	if(!facebook_ping())
	{

		facebook_set_session(NULL);

		if (!facebook_get_token())
			return FALSE;

		gchar *url =  facebook_get_auth_url(FACEBOOK_LOGIN);
		if (!auth_popup(_("Rawstudio needs to be authenticated before it will be able to upload photos to your Facebook account."), url))
			return FALSE;

		gchar *session = facebook_get_session();
		if (!session)
			return FALSE;

		rs_conf_set_string("facebook_session", session);
	}

	RSOutput *jpegsave = rs_output_new ("RSJpegfile");
	gchar *temp_file = g_strdup_printf ("%s%s.rawstudio-tmp-%d.jpg", g_get_tmp_dir (), G_DIR_SEPARATOR_S, (gint) (g_random_double () * 10000.0));

	g_object_set (jpegsave, "filename", temp_file, "quality", facebook->quality, NULL);
	rs_output_execute (jpegsave, filter);
	g_object_unref (jpegsave);

	if(!facebook_upload_photo(temp_file, facebook->caption))
		return FALSE;

	unlink (temp_file);
	g_free (temp_file);
	facebook_close();

	return TRUE;
}
