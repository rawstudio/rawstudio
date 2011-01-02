/*
 * * Copyright (C) 2006-2011 Anders Brander <anders@brander.dk>, 
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
#include "output-flickr.h"
#include <unistd.h>
#include <string.h>
#include <flickcurl.h>

/* Ugly HACK - conf_interface.c|h needs to be ported to librawstudio */
gchar *rs_conf_get_string (const gchar * name);
gboolean rs_conf_set_string (const gchar * name, const gchar * value);

#define RS_TYPE_FLICKR (rs_flickr_type)
#define RS_FLICKR(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), RS_TYPE_FLICKR, RSFlickr))
#define RS_FLICKR_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), RS_TYPE_FLICKR, RSFlickrClass))
#define RS_IS_FLICKR(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), RS_TYPE_FLICKR))

typedef struct _RSFlickr RSFlickr;
typedef struct _RSFlickrClass RSFlickrClass;

struct _RSFlickr
{
	RSOutput parent;

	gint quality;
	gchar *title;
	gchar *description;
	gchar *tags;
	gboolean is_public;
	gboolean is_friend;
	gboolean is_family;
	gint safety_level;
	gint content_type;
};

struct _RSFlickrClass
{
	RSOutputClass parent_class;
};

RS_DEFINE_OUTPUT (rs_flickr, RSFlickr)
enum
{
	PROP_0,
	PROP_LOGO,
	PROP_JPEG_QUALITY,
	PROP_FILENAME, /* Required for a output plugin - not in use */
	PROP_TITLE,
	PROP_DESCRIPTION,
	PROP_TAGS,
	PROP_IS_PUBLIC,
	PROP_IS_FRIEND,
	PROP_IS_FAMILY
};

static void get_property (GObject * object, guint property_id, GValue * value, GParamSpec * pspec);
static void set_property (GObject * object, guint property_id, const GValue * value, GParamSpec * pspec);
static gboolean execute (RSOutput * output, RSFilter * filter);
static GtkWidget * get_logo_widget(RSFlickr *flickr);

G_MODULE_EXPORT void rs_plugin_load (RSPlugin * plugin)
{
	rs_flickr_get_type (G_TYPE_MODULE (plugin));
}

static void
rs_flickr_class_init (RSFlickrClass * klass)
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
					 PROP_TITLE, g_param_spec_string ("title",
									  "title",
									  _("Title"),
									  NULL,
									  G_PARAM_READWRITE));

	g_object_class_install_property (object_class,
					 PROP_DESCRIPTION,
					 g_param_spec_string ("description",
							      "description",
							      _("Description"), NULL,
							      G_PARAM_READWRITE));

	g_object_class_install_property (object_class,
					 PROP_TAGS, g_param_spec_string ("tags",
									 "tags",
									 _("Tags"),
									 NULL,
									 G_PARAM_READWRITE));

	g_object_class_install_property (object_class,
					 PROP_IS_PUBLIC,
					 g_param_spec_boolean ("public", "public",
							       _("Public (everyone can see this)"), FALSE,
							       G_PARAM_READWRITE));

	g_object_class_install_property (object_class,
					 PROP_IS_FRIEND,
					 g_param_spec_boolean ("friend", "friend",
							       _("Visible to Friends"), FALSE,
							       G_PARAM_READWRITE));

	g_object_class_install_property (object_class, 
					 PROP_IS_PUBLIC,
					 g_param_spec_boolean ("family", "family",
							       _("Visible to Family"), FALSE,
							       G_PARAM_READWRITE));

	g_object_class_install_property (object_class,
					 PROP_LOGO, g_param_spec_object ("Logo",
										   "logo",
										   "Logo",
										   GTK_TYPE_WIDGET,
										   G_PARAM_READABLE));

	output_class->execute = execute;
	output_class->display_name = _("Upload photo to Flickr");
}

static void
rs_flickr_init (RSFlickr * flickr)
{
	flickr->quality = 90;
}

static void
get_property (GObject * object, guint property_id, GValue * value, GParamSpec * pspec)
{
	RSFlickr *flickr = RS_FLICKR (object);

	switch (property_id)
	{
	case PROP_JPEG_QUALITY:
		g_value_set_int (value, flickr->quality);
		break;
	case PROP_TITLE:
		g_value_set_string (value, flickr->title);
		break;
	case PROP_DESCRIPTION:
		g_value_set_string (value, flickr->description);
		break;
	case PROP_TAGS:
		g_value_set_string (value, flickr->tags);
		break;
	case PROP_IS_PUBLIC:
		g_value_set_boolean (value, flickr->is_public);
		break;
	case PROP_IS_FRIEND:
		g_value_set_boolean (value, flickr->is_friend);
		break;
	case PROP_IS_FAMILY:
		g_value_set_boolean (value, flickr->is_family);
		break;
	case PROP_LOGO:
		g_value_set_object(value, get_logo_widget(flickr));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
	}
}

static void
set_property (GObject * object, guint property_id, const GValue * value, GParamSpec * pspec)
{
	RSFlickr *flickr = RS_FLICKR (object);

	switch (property_id)
	{
	case PROP_JPEG_QUALITY:
		flickr->quality = g_value_get_int (value);
		break;
	case PROP_TITLE:
		flickr->title = g_value_dup_string (value);
		break;
	case PROP_DESCRIPTION:
		flickr->description = g_value_dup_string (value);
		break;
	case PROP_TAGS:
		flickr->tags = g_value_dup_string (value);
		break;
	case PROP_IS_PUBLIC:
		flickr->is_public = g_value_get_boolean (value);
		break;
	case PROP_IS_FRIEND:
		flickr->is_friend = g_value_get_boolean (value);
		break;
	case PROP_IS_FAMILY:
		flickr->is_family = g_value_get_boolean (value);
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

void
flickcurl_print_error (void *user_data, const char *temp)
{
	gchar *message = NULL;

	/* Errors not catched yet - probably due to lack of knowledge about what causes it
	   Call failed with error 99 - Insufficient permissions. Method requires write privileges; none granted.
	*/

	/* DEBUG */
	g_debug ("flickcurl: %s\n", temp);

	/* Catch errors and show our own and more userfriendly errors */
	if (g_ascii_strcasecmp(temp,"Method flickr.auth.getToken failed with error 108 - Invalid frob") == 0)
		message = g_strdup(_("We recieved an error during authentication. Please try again."));

	else if (g_ascii_strcasecmp(temp, "Call failed with error 98 - Invalid auth token") == 0)
		message = g_strdup(_("Rawstudio were not able to upload the photo cause the authentication has been revoked. Please re-authenticate Rawstudio to upload to Flickr."));

	else if (g_ascii_strcasecmp(temp,"Method flickr.test.login failed with error 98 - Invalid auth token"))
		message = g_strdup(_("It seems like rawstudio lost its authentication to upload to your account, please re-authenticate."));

	/* Everything else will be shown along with a note */
	else
		message = g_strdup_printf(_("%s\n\n<b>Note: This error isn't catched by Rawstudio. Please let us know that you found it and how to reproduce it so we can make a more useful errormessage. Thanks!</b>"), (gchar *) temp);

	GtkWidget *dialog = gui_dialog_make_from_text (GTK_STOCK_DIALOG_ERROR, g_strdup (_("Flickr error")), message);
	gtk_dialog_add_button (GTK_DIALOG (dialog), GTK_STOCK_OK, GTK_RESPONSE_OK);
	gdk_threads_enter ();
	gtk_widget_show_all (dialog);
	gtk_dialog_run (GTK_DIALOG (dialog));
	gdk_threads_leave ();
	gtk_widget_destroy (dialog);
}

static gboolean
execute (RSOutput * output, RSFilter * filter)
{
	RSFlickr *flickr = RS_FLICKR (output);

	flickcurl *fc;

	flickcurl_init ();		/* optional static initialising of resources */
	fc = flickcurl_new ();

	flickcurl_set_error_handler (fc, flickcurl_print_error, NULL);

	/* Set configuration, or more likely read from a config file */
	flickcurl_set_api_key (fc, FLICKR_API_KEY);
	flickcurl_set_shared_secret (fc, FLICKR_SECRET_KEY);
	
	gchar *flickr_user_token = rs_conf_get_string ("flickr_user_token");
	gchar *flickr_user_name = NULL;

	if (flickr_user_token)
	{
		flickcurl_set_auth_token (fc, flickr_user_token);
		flickr_user_name = flickcurl_test_login (fc);

		/* We need to reset all flickcurl */
		flickcurl_free (fc);
		fc = flickcurl_new ();
		flickcurl_set_error_handler (fc, flickcurl_print_error, NULL);

		/* Set configuration, or more likely read from a config file */
		flickcurl_set_api_key (fc, FLICKR_API_KEY);
		flickcurl_set_shared_secret (fc, FLICKR_SECRET_KEY);
	}

	if (!flickr_user_name)
	{
		char *frob = g_strdup (flickcurl_auth_getFrob (fc));	// FIXME: Returns NULL on failure
		char *sign = g_strdup_printf ("%sapi_key%sfrob%spermswrite", FLICKR_SECRET_KEY, FLICKR_API_KEY, frob);
		char *sign_md5 = g_compute_checksum_for_string (G_CHECKSUM_MD5, sign, strlen (sign));
		char *auth_url = g_strdup_printf("http://flickr.com/services/auth/?api_key=%s&perms=write&frob=%s&api_sig=%s", FLICKR_API_KEY, frob, sign_md5);

		gdk_threads_enter ();

		GtkWidget *flickr_auth_dialog = gtk_dialog_new ();
		gtk_window_set_title (GTK_WINDOW (flickr_auth_dialog), "Rawstudio");
		gtk_container_set_border_width (GTK_CONTAINER (flickr_auth_dialog), 4);
		gtk_dialog_set_has_separator (GTK_DIALOG (flickr_auth_dialog), FALSE);

		GtkWidget *vbox = GTK_DIALOG (flickr_auth_dialog)->vbox;

		GtkWidget *textlabel = gtk_label_new(_("Rawstudio needs to be authenticated before it will be able to upload photos to your Flickr account."));
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

		gtk_dialog_add_action_widget (GTK_DIALOG (flickr_auth_dialog), cancelbutton, GTK_RESPONSE_CANCEL);
		gtk_dialog_add_action_widget (GTK_DIALOG (flickr_auth_dialog), acceptbutton, GTK_RESPONSE_ACCEPT);

		gtk_box_pack_start (GTK_BOX (vbox), table, FALSE, FALSE, 4);

		gtk_table_attach_defaults (GTK_TABLE (table), step1label, 0, 1, 0, 1);
		gtk_table_attach_defaults (GTK_TABLE (table), step2label, 0, 1, 1, 2);

		gtk_table_attach_defaults (GTK_TABLE (table), link, 1, 2, 0, 1);
		gtk_table_attach_defaults (GTK_TABLE (table), hbox, 1, 2, 1, 2);

		gtk_widget_show_all (flickr_auth_dialog);
		gint response = gtk_dialog_run (GTK_DIALOG (flickr_auth_dialog));

		gtk_widget_destroy (flickr_auth_dialog);

		gdk_threads_leave ();

		if (response == GTK_RESPONSE_ACCEPT)
		{
			gchar *token = g_strdup (flickcurl_auth_getToken (fc, frob));
			
			if (token)
			{
				rs_conf_set_string ("flickr_user_token", token);
				flickr_user_token = token;
			}
			else
			{
				return FALSE;
			}
		}
		else
		{
			return FALSE;
		}
	}

	RSOutput *jpegsave = rs_output_new ("RSJpegfile");

	gchar *temp_file = g_strdup_printf ("%s%s.rawstudio-tmp-%d.jpg", g_get_tmp_dir (), G_DIR_SEPARATOR_S, (gint) (g_random_double () * 10000.0));

	g_object_set (jpegsave, "filename", temp_file, "quality", flickr->quality, NULL);
	rs_output_execute (jpegsave, filter);
	g_object_unref (jpegsave);

	flickcurl_upload_params *upload_params = malloc (sizeof (flickcurl_upload_params));

	flickcurl_set_auth_token (fc, flickr_user_token);

	upload_params->photo_file = temp_file;
	upload_params->title = flickr->title;
	upload_params->description = flickr->description;
	upload_params->tags = flickr->tags;
	upload_params->is_public = flickr->is_public;
	upload_params->is_friend = flickr->is_friend;
	upload_params->is_family = flickr->is_family;
	upload_params->safety_level = 0;	/* FIXME: we leave this hardcoded at the moment */
	upload_params->content_type = 0;	/* FIXME: same as above */

	/* Perform upload */
	flickcurl_photos_upload_params (fc, upload_params);

	unlink (temp_file);
	g_free (temp_file);

	flickcurl_free (fc);
	flickcurl_finish ();		/* optional static free of resources */

	return TRUE;
}

static GtkWidget *
get_logo_widget(RSFlickr *flickr)
{
	gchar *filename = g_build_filename(PACKAGE_DATA_DIR, PACKAGE, "/plugins/flickr-logo.svg", NULL);
	GtkWidget *box = gtk_vbox_new(TRUE, 2);
	GtkWidget *logo = gtk_image_new_from_file(filename);
	g_free(filename);

	gtk_box_pack_start (GTK_BOX (box), logo, FALSE, FALSE, 2);
	return box;
}
