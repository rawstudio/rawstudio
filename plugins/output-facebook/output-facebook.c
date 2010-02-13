/*
 * * Copyright (C) 2006-2010 Anders Brander <anders@brander.dk>, 
 * * Anders Kvist <akv@lnxbx.dk> and Klaus Post <klauspost@gmail.com>
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
#include "rs-facebook-client.h"

/* Ugly HACK - conf_interface.c|h needs to be ported to librawstudio */
gchar *rs_conf_get_string (const gchar * name);
gboolean rs_conf_set_string (const gchar * name, const gchar * value);

/* FIXME: this should be moved to conf_interface.h when ported to librawstudio */
#define CONF_FACEBOOK_ALBUM_ID "facebook_album_id"

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
	gchar *caption;
	gchar *album_id;
};

struct _RSFacebookClass
{
	RSOutputClass parent_class;
};

typedef struct 
{
	RSFacebookClient *facebook_client;
	GtkEntry *entry;
	GtkComboBox *combobox;
} CreateAlbumData;

RS_DEFINE_OUTPUT (rs_facebook, RSFacebook)
enum
{
	PROP_0,
	PROP_LOGO,
	PROP_JPEG_QUALITY,
	PROP_CAPTION,
	PROP_ALBUM_SELECTOR
};

static void get_property (GObject * object, guint property_id, GValue * value, GParamSpec * pspec);
static void set_property (GObject * object, guint property_id, const GValue * value, GParamSpec * pspec);
static gboolean execute (RSOutput * output, RSFilter * filter);
static GtkWidget * get_album_selector_widget();
static GtkWidget * get_logo_widget(RSFacebook *facebook);

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

	g_object_class_install_property (object_class,
					 PROP_ALBUM_SELECTOR, g_param_spec_object ("album selector",
										   "album selector",
										   "Album selector",
										   GTK_TYPE_WIDGET,
										   G_PARAM_READABLE));

	g_object_class_install_property (object_class,
					 PROP_LOGO, g_param_spec_object ("Logo",
										   "logo",
										   "Logo",
										   GTK_TYPE_WIDGET,
										   G_PARAM_READABLE));

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
	case PROP_CAPTION:
		g_value_set_string (value, facebook->caption);
		break;
	case PROP_ALBUM_SELECTOR:
		g_value_set_object(value, get_album_selector_widget(facebook));
		break;
	case PROP_LOGO:
		g_value_set_object(value, get_logo_widget(facebook));
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
auth_popup(const gchar *text, const gchar *auth_url)
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
deal_with_error(GError **error)
{
	if (!*error)
		return FALSE;

	g_warning("Error from Facebook: '%s'", (*error)->message);

	gdk_threads_enter();
	GtkWidget *dialog = gtk_message_dialog_new(NULL, 0, GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE,
		"Error: '%s'", (*error)->message);

	gtk_window_set_title(GTK_WINDOW(dialog), _("Unhandled error from Facebook"));
	gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_CLOSE);

	g_signal_connect(dialog, "response", G_CALLBACK(gtk_widget_destroy), NULL);

	gtk_widget_show (dialog);

	gdk_threads_leave();

	g_clear_error(error);

	return TRUE;
}

gboolean
facebook_auth(RSFacebookClient *facebook_client)
{
	GError *error = NULL;
	gboolean ping = rs_facebook_client_ping(facebook_client, &error);
	deal_with_error(&error);

	if (!ping)
	{
		rs_facebook_client_set_session_key(facebook_client, NULL);
		const gchar *url = rs_facebook_client_get_auth_url(facebook_client, FACEBOOK_LOGIN, &error);
		deal_with_error(&error);
		if (!auth_popup(_("Rawstudio needs to be authenticated before it will be able to upload photos to your Facebook account."), url))
			return FALSE;

		const gchar *session = rs_facebook_client_get_session_key(facebook_client, &error);
		deal_with_error(&error);
		if (!session)
			return FALSE;

		rs_conf_set_string("facebook_session", session);
	}

	return ping;
}


static gboolean
execute (RSOutput * output, RSFilter * filter)
{
	GError *error = NULL;
	RSFacebook *facebook = RS_FACEBOOK (output);

	gchar *session = rs_conf_get_string("facebook_session");
	RSFacebookClient *facebook_client = rs_facebook_client_new(FACEBOOK_API_KEY, FACEBOOK_SECRET_KEY, session);
	g_free(session);

	facebook_auth(facebook_client);

	RSOutput *jpegsave = rs_output_new ("RSJpegfile");
	gchar *temp_file = g_strdup_printf ("%s%s.rawstudio-tmp-%d.jpg", g_get_tmp_dir (), G_DIR_SEPARATOR_S, (gint) (g_random_double () * 10000.0));

	g_object_set (jpegsave, "filename", temp_file, "quality", facebook->quality, NULL);
	rs_output_execute (jpegsave, filter);
	g_object_unref (jpegsave);

	gboolean ret = rs_facebook_client_upload_image(facebook_client, temp_file, facebook->caption, facebook->album_id, &error);
	deal_with_error(&error);

	unlink (temp_file);
	g_free (temp_file);
	g_object_unref(facebook_client);

	return ret;
}

void
combobox_cell_text(GtkComboBox *combo, gint col)
{
        GtkCellRenderer *rend = gtk_cell_renderer_text_new ();
        gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (combo), rend, TRUE);
        gtk_cell_layout_add_attribute (GTK_CELL_LAYOUT (combo), rend, "text", col);
}

static void
album_set_active(GtkComboBox *combo, gchar *aid)
{
	GtkTreeModel *model = gtk_combo_box_get_model(combo);
	GtkTreeIter iter;
	gchar *album_id;

	if (model && gtk_tree_model_get_iter_first(model, &iter))
		do
		{
			gtk_tree_model_get(model, &iter,
					   1, &album_id,
					   -1);

			if (g_strcmp0(aid, album_id) == 0)
			{
				gtk_combo_box_set_active_iter(combo, &iter);
				g_free(album_id);
				return;
			}
			g_free(album_id);
		}
		while (gtk_tree_model_iter_next(model, &iter));
}

static void
album_changed(GtkComboBox *combo, gpointer callback_data)
{
	RSFacebook *facebook = callback_data;
        GtkTreeIter iter;
        GtkTreeModel *model;
        gchar *album, *aid;

        gtk_combo_box_get_active_iter(GTK_COMBO_BOX(combo), &iter);
        model = gtk_combo_box_get_model(GTK_COMBO_BOX(combo));
        gtk_tree_model_get(model, &iter,
			   0, &album,
			   1, &aid,
			   -1);

	facebook->album_id = aid;
	rs_conf_set_string(CONF_FACEBOOK_ALBUM_ID, aid);

        return;
}

static void
create_album(GtkButton *button, gpointer callback_data)
{
	CreateAlbumData *create_album_data = callback_data;
	RSFacebookClient *facebook_client = create_album_data->facebook_client;
	GtkEntry *entry = create_album_data->entry;
	GtkComboBox *combobox = create_album_data->combobox;
	const gchar *album_name = gtk_entry_get_text(entry);

	gchar *aid = rs_facebook_client_create_album(facebook_client, album_name);

	if (aid)
	{
		GtkListStore *albums = rs_facebook_client_get_album_list(facebook_client, NULL);
		gtk_combo_box_set_model(combobox, GTK_TREE_MODEL(albums));
		album_set_active(combobox, aid);
		gtk_entry_set_text(entry, "");
	}
}

GtkWidget *
get_album_selector_widget(RSFacebook *facebook)
{
	GError *error = NULL;
	gchar *album_id = rs_conf_get_string(CONF_FACEBOOK_ALBUM_ID);

	CreateAlbumData *create_album_data = g_malloc(sizeof(CreateAlbumData));

	gchar *session = rs_conf_get_string("facebook_session");
	RSFacebookClient *facebook_client = rs_facebook_client_new(FACEBOOK_API_KEY, FACEBOOK_SECRET_KEY, session);
	g_free(session);

	facebook_auth(facebook_client);

	GtkListStore *albums = rs_facebook_client_get_album_list(facebook_client, &error);
	GtkWidget *combobox = gtk_combo_box_new();
	combobox_cell_text(GTK_COMBO_BOX(combobox), 0);
	gtk_combo_box_set_model(GTK_COMBO_BOX(combobox), GTK_TREE_MODEL(albums));
	album_set_active(GTK_COMBO_BOX(combobox), album_id);
	facebook->album_id = album_id;

	g_signal_connect ((gpointer) combobox, "changed", G_CALLBACK (album_changed), facebook);

	GtkWidget *box = gtk_hbox_new(FALSE, 2);
	GtkWidget *label = gtk_label_new(_("Albums"));
	GtkWidget *sep = gtk_vseparator_new();
	GtkWidget *entry = gtk_entry_new();
	GtkWidget *button = gtk_button_new_with_label(_("Create album"));
	gtk_box_pack_start (GTK_BOX (box), label, FALSE, FALSE, 2);
	gtk_box_pack_start (GTK_BOX (box), combobox, FALSE, FALSE, 2);
	gtk_box_pack_start (GTK_BOX (box), sep, FALSE, FALSE, 2);
	gtk_box_pack_start (GTK_BOX (box), entry, FALSE, FALSE, 2);
	gtk_box_pack_start (GTK_BOX (box), button, FALSE, FALSE, 2);

	create_album_data->facebook_client = facebook_client;
	create_album_data->entry = GTK_ENTRY(entry);
	create_album_data->combobox = GTK_COMBO_BOX(combobox);

	g_signal_connect ((gpointer) button, "clicked", G_CALLBACK (create_album), create_album_data);

	return box;
}

GtkWidget *
get_logo_widget(RSFacebook *facebook)
{
	gchar *filename = g_build_filename(PACKAGE_DATA_DIR, PACKAGE, "/plugins/facebook-logo.svg", NULL);
	GtkWidget *box = gtk_vbox_new(TRUE, 2);
	GtkWidget *logo = gtk_image_new_from_file(filename);
	g_free(filename);

	gtk_box_pack_start (GTK_BOX (box), logo, FALSE, FALSE, 2);
	return box;
}
