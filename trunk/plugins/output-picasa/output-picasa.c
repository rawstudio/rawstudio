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

#include <rawstudio.h>
#include <gettext.h>
#include "config.h"
#include <unistd.h>
#include <string.h>
#include "rs-picasa-client.h"
#include <conf_interface.h>

#define RS_TYPE_PICASA (rs_picasa_type)
#define RS_PICASA(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), RS_TYPE_PICASA, RSPicasa))
#define RS_PICASA_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), RS_TYPE_PICASA, RSPicasaClass))
#define RS_IS_PICASA(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), RS_TYPE_PICASA))

typedef struct _RSPicasa RSPicasa;
typedef struct _RSPicasaClass RSPicasaClass;

struct _RSPicasa
{
	RSOutput parent;
        gchar *album_id;
	gint quality;
};

struct _RSPicasaClass
{
	RSOutputClass parent_class;
};

typedef struct 
{
        PicasaClient *picasa_client;
        GtkEntry *entry;
        GtkComboBox *combobox;
} CreateAlbumData;

RS_DEFINE_OUTPUT (rs_picasa, RSPicasa)
enum
{
	PROP_0,
	PROP_LOGO,
	PROP_JPEG_QUALITY,
        PROP_ALBUM_SELECTOR
};

static void get_property (GObject * object, guint property_id, GValue * value, GParamSpec * pspec);
static void set_property (GObject * object, guint property_id, const GValue * value, GParamSpec * pspec);
static gboolean execute (RSOutput * output, RSFilter * filter);
GtkWidget * get_album_selector_widget(RSPicasa *picasa);
GtkWidget * get_logo_widget(RSPicasa *picasa);

G_MODULE_EXPORT void rs_plugin_load (RSPlugin * plugin)
{
	rs_picasa_get_type (G_TYPE_MODULE (plugin));
}

static void
rs_picasa_class_init (RSPicasaClass * klass)
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
					 PROP_LOGO, g_param_spec_object ("Logo",
										   "logo",
										   "Logo",
										   GTK_TYPE_WIDGET,
										   G_PARAM_READABLE));

        g_object_class_install_property (object_class,
                                         PROP_ALBUM_SELECTOR, g_param_spec_object ("album selector",
                                                                                   "album selector",
                                                                                   "Album selector",
                                                                                   GTK_TYPE_WIDGET,
                                                                                   G_PARAM_READABLE));

	output_class->execute = execute;
	output_class->display_name = _("Upload photo to Picasa");
}

static void
rs_picasa_init (RSPicasa * picasa)
{
	picasa->quality = 90;
}

static void
get_property (GObject * object, guint property_id, GValue * value, GParamSpec * pspec)
{
	RSPicasa *picasa = RS_PICASA (object);

	switch (property_id)
	{
	case PROP_JPEG_QUALITY:
		g_value_set_int (value, picasa->quality);
		break;
	case PROP_LOGO:
		g_value_set_object(value, get_logo_widget(picasa));
		break;
        case PROP_ALBUM_SELECTOR:
                g_value_set_object(value, get_album_selector_widget(picasa));
                break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
	}
}

static void
set_property (GObject * object, guint property_id, const GValue * value, GParamSpec * pspec)
{
	RSPicasa *picasa = RS_PICASA (object);

	switch (property_id)
	{
	case PROP_JPEG_QUALITY:
		picasa->quality = g_value_get_int (value);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
	}
}

void
combobox_cell_text(GtkComboBox *combo, gint col)
{
        GtkCellRenderer *rend = gtk_cell_renderer_text_new ();
        gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (combo), rend, TRUE);
        gtk_cell_layout_add_attribute (GTK_CELL_LAYOUT (combo), rend, "text", col);
}

static gboolean
deal_with_error(GError **error)
{
	if (!*error)
		return FALSE;
	
	g_warning("Error from Picasa: '%s'", (*error)->message);

	gdk_threads_enter();
	GtkWidget *dialog = gtk_message_dialog_new(NULL, 0, GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE,
		"Error: '%s'", (*error)->message);

	gtk_window_set_title(GTK_WINDOW(dialog), _("Unhandled error from Picasa"));
	gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_CLOSE);

	g_signal_connect(dialog, "response", G_CALLBACK(gtk_widget_destroy), NULL);

	gtk_widget_show (dialog);

	gdk_threads_leave();

	g_clear_error(error);

	return TRUE;
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
        RSPicasa *picasa = callback_data;
        GtkTreeIter iter;
        GtkTreeModel *model;
        gchar *album, *aid;

        gtk_combo_box_get_active_iter(GTK_COMBO_BOX(combo), &iter);
        model = gtk_combo_box_get_model(GTK_COMBO_BOX(combo));
        gtk_tree_model_get(model, &iter,
                           0, &album,
                           1, &aid,
                           -1);

        picasa->album_id = aid;
        rs_conf_set_string(CONF_PICASA_CLIENT_ALBUM_ID, aid);

        return;
}

static void
create_album(GtkButton *button, gpointer callback_data)
{
        CreateAlbumData *create_album_data = callback_data;
        PicasaClient *picasa_client = create_album_data->picasa_client;
        GtkEntry *entry = create_album_data->entry;
        GtkComboBox *combobox = create_album_data->combobox;
        const gchar *album_name = gtk_entry_get_text(entry);

	gchar *aid = rs_picasa_client_create_album(picasa_client, album_name);

        if (aid)
        {
                GtkListStore *albums = rs_picasa_client_get_album_list(picasa_client);
                gtk_combo_box_set_model(combobox, GTK_TREE_MODEL(albums));
                album_set_active(combobox, aid);
                gtk_entry_set_text(entry, "");
        }
}

GtkWidget *
get_album_selector_widget(RSPicasa *picasa)
{
        GError *error = NULL;
        gchar *album_id = rs_conf_get_string(CONF_PICASA_CLIENT_ALBUM_ID);

        CreateAlbumData *create_album_data = g_malloc(sizeof(CreateAlbumData));

	PicasaClient *picasa_client = rs_picasa_client_init();

        GtkListStore *albums = rs_picasa_client_get_album_list(picasa_client);
        GtkWidget *combobox = gtk_combo_box_new();
        combobox_cell_text(GTK_COMBO_BOX(combobox), 0);
        gtk_combo_box_set_model(GTK_COMBO_BOX(combobox), GTK_TREE_MODEL(albums));
        album_set_active(GTK_COMBO_BOX(combobox), album_id);
	picasa->album_id = album_id;

        g_signal_connect ((gpointer) combobox, "changed", G_CALLBACK (album_changed), picasa);

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

        create_album_data->picasa_client = picasa_client;
        create_album_data->entry = GTK_ENTRY(entry);
        create_album_data->combobox = GTK_COMBO_BOX(combobox);

        g_signal_connect ((gpointer) button, "clicked", G_CALLBACK (create_album), create_album_data);

	return box;
}

static gboolean
execute (RSOutput * output, RSFilter * filter)
{
	GError *error = NULL;
	RSPicasa *picasa = RS_PICASA (output);
	RSOutput *jpegsave = rs_output_new ("RSJpegfile");

	PicasaClient *picasa_client = rs_picasa_client_init();

	gchar *temp_file = g_strdup_printf ("%s%s.rawstudio-tmp-%d.jpg", g_get_tmp_dir (), G_DIR_SEPARATOR_S, (gint) (g_random_double () * 10000.0));

	g_object_set (jpegsave, "filename", temp_file, "quality", picasa->quality, NULL);
	rs_output_execute (jpegsave, filter);
	g_object_unref (jpegsave);

	rs_picasa_client_upload_photo(picasa_client, temp_file, picasa->album_id, &error);

	unlink (temp_file);
	g_free (temp_file);

	return deal_with_error(&error);
}

GtkWidget *
get_logo_widget(RSPicasa *picasa)
{
	gchar *filename = g_build_filename(PACKAGE_DATA_DIR, PACKAGE, "/plugins/picasa-logo.svg", NULL);
	GtkWidget *box = gtk_vbox_new(TRUE, 2);
	GtkWidget *logo = gtk_image_new_from_file(filename);
	g_free(filename);

	gtk_box_pack_start (GTK_BOX (box), logo, FALSE, FALSE, 2);
	return box;
}
