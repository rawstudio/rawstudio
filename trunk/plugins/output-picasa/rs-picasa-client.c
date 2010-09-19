/**
 * Documentation for Picasa:
 * http://code.google.com/apis/picasaweb/docs/2.0/developers_guide_protocol.html
 * Login: http://code.google.com/apis/accounts/docs/AuthForInstalledApps.html
 *
 * Documentation for CURL:
 * http://curl.haxx.se/libcurl/c/curl_easy_setopt.html
 */

#include <glib.h>
#include <gtk/gtk.h>
#include <libxml/encoding.h>
#include <string.h>
#include <curl/curl.h>
#include "rs-picasa-client.h"
#include "conf_interface.h"

//#define CURL_DEBUG TRUE

#define PICASA_LOGIN_URL "https://www.google.com/accounts/ClientLogin"
#define PICASA_DATA_URL "http://picasaweb.google.com/data/feed/api"
#define HTTP_BOUNDARY "5d0ae7df9faf6ee0ae584d7676ca34d0" /* md5sum of "Rawstudio2PicasaWebAlbums" */

typedef enum {
	PICASA_ALBUM_NAME,
	PICASA_ALBUM_ID
} PicasaAlbum;

static gint
picasa_error(PicasaClient *picasa_client, gint code, const GString *data, GError **error)
{
	gchar *error_msg = NULL;

	switch(code)
	{
		case 200:
		case 201:
			break;
		case 404:
			error_msg = g_strdup(data->str);
			break;
		case 403:
		case 401:
		{
			picasa_client->auth_token = NULL;
			while (!rs_picasa_client_auth(picasa_client))
			{
				if (!rs_picasa_client_auth_popup(picasa_client))
				{
					/* Cancel pressed, or no info entered */
					g_set_error(error, g_quark_from_static_string("rawstudio_facebook_client_error"), code, "Cannot log in");
					return PICASA_CLIENT_ERROR;
				}
			}
			/* Save information */
			rs_conf_set_string(CONF_PICASA_CLIENT_AUTH_TOKEN, picasa_client->auth_token);
			rs_conf_set_string(CONF_PICASA_CLIENT_USERNAME, picasa_client->username);
			return PICASA_CLIENT_RETRY;
		}
		break;
	default:
		error_msg = g_strdup_printf("Error %d not caught, please submit this as a bugreport:\n%s", code, data->str);
		break;
	}

	if (error_msg)
	{
		g_set_error(error, g_quark_from_static_string("rawstudio_facebook_client_error"), code, "%s", error_msg);
		g_free(error_msg);
		return PICASA_CLIENT_ERROR;
	}
	else
	{
		return PICASA_CLIENT_OK;
	}
}

static GtkListStore *
xml_album_list_response(const GString *xml)
{
	xmlDocPtr doc = xmlParseMemory(xml->str, xml->len);
	xmlNodePtr cur, child;

	cur = xmlDocGetRootElement(doc);
	cur = cur->xmlChildrenNode;

	gchar *id = NULL;
	gchar *name = NULL;

	GtkListStore *albums = NULL;
	GtkTreeIter iter;

	while (cur)
	{
		if ((!xmlStrcmp(cur->name, BAD_CAST("entry"))))
		{
			child = cur->xmlChildrenNode;

			while (child)
			{
				if ((!xmlStrcmp(child->name, BAD_CAST("name"))) && g_strcmp0((char *) child->ns->prefix, "gphoto") == 0)
					name = (gchar *) xmlNodeListGetString(doc, child->xmlChildrenNode, 1);
				if ((!xmlStrcmp(child->name, BAD_CAST("id"))) && g_strcmp0((char *) child->ns->prefix, "gphoto") == 0)
					id = (gchar *) xmlNodeListGetString(doc, child->xmlChildrenNode, 1);
				child = child->next;
			}

			if (name && id)
			{
				if (!albums)
					albums = gtk_list_store_new(2, G_TYPE_STRING, G_TYPE_STRING);

				gtk_list_store_append(albums, &iter);
				gtk_list_store_set(albums, &iter,
						   PICASA_ALBUM_NAME, name,
						   PICASA_ALBUM_ID, id,
						   -1);
				id = NULL;
				name = NULL;
			}
		}
		cur = cur->next;
	}
	return albums;
}

gchar *
xml_album_create_response(const GString *xml)
{
	xmlDocPtr doc = xmlParseMemory(xml->str, xml->len);
	xmlNodePtr cur;

	cur = xmlDocGetRootElement(doc);
	cur = cur->xmlChildrenNode;

	gchar *id = NULL;

	while (cur)
	{
		if ((!xmlStrcmp(cur->name, BAD_CAST("id"))) && g_strcmp0((char *) cur->ns->prefix, "gphoto") == 0)
		{
			id = (gchar *) xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
			return id;
		}
		cur = cur->next;
	}
	return NULL;
}

static size_t
write_callback(void *ptr, size_t size, size_t nmemb, void *userp)
{
	GString *string = (GString *) userp;
	g_string_append_len(string, (char *) ptr, size * nmemb);
	return (size * nmemb);
}

gboolean
handle_curl_code(CURLcode result)
{
	if (result != CURLE_OK)
	{
		g_warning("Something has happened with the request, please try to recreate this error and tell the developers about it...");
		/* FIXME: g_warning() with some debug from CURL - request, returncode and such... */
		return FALSE;
	}
	else
	{
		return TRUE;
	}
}

gboolean 
rs_picasa_client_auth_popup(PicasaClient *picasa_client)
{
        gdk_threads_enter ();
        GtkWidget *auth_dialog = gtk_dialog_new ();
        gtk_window_set_title (GTK_WINDOW (auth_dialog), "Rawstudio");
        gtk_container_set_border_width (GTK_CONTAINER (auth_dialog), 4);
        gtk_dialog_set_has_separator (GTK_DIALOG (auth_dialog), FALSE);

        GtkWidget *vbox = GTK_DIALOG (auth_dialog)->vbox;

        GtkWidget *textlabel = gtk_label_new("Please type in your username and password for Picasa Web Albums.");
        gtk_label_set_line_wrap (GTK_LABEL (textlabel), TRUE);

        gtk_box_pack_start (GTK_BOX (vbox), textlabel, TRUE, TRUE, 4);

        GtkWidget *table = gtk_table_new (2, 2, FALSE);

        GtkWidget *username = gtk_label_new ("Username: ");
        GtkWidget *password = gtk_label_new ("Password: ");

	GtkWidget *input_username = gtk_entry_new();
	GtkWidget *input_password = gtk_entry_new();
	gtk_entry_set_visibility(GTK_ENTRY(input_password), FALSE);

        GtkWidget *cancelbutton = gtk_button_new_from_stock (GTK_STOCK_CANCEL);
        GtkWidget *acceptbutton = gtk_button_new_from_stock (GTK_STOCK_GO_FORWARD);

        gtk_dialog_add_action_widget (GTK_DIALOG (auth_dialog), cancelbutton, GTK_RESPONSE_CANCEL);
        gtk_dialog_add_action_widget (GTK_DIALOG (auth_dialog), acceptbutton, GTK_RESPONSE_OK);

        gtk_box_pack_start (GTK_BOX (vbox), table, FALSE, FALSE, 4);

        gtk_table_attach_defaults (GTK_TABLE (table), username, 0, 1, 0, 1);
        gtk_table_attach_defaults (GTK_TABLE (table), password, 0, 1, 1, 2);

        gtk_table_attach_defaults (GTK_TABLE (table), input_username, 1, 2, 0, 1);
        gtk_table_attach_defaults (GTK_TABLE (table), input_password, 1, 2, 1, 2);

        gtk_widget_show_all (auth_dialog);
        gint response = gtk_dialog_run (GTK_DIALOG (auth_dialog));

	if (0 == gtk_entry_get_text_length(GTK_ENTRY(input_username)) || 
			0 == gtk_entry_get_text_length(GTK_ENTRY(input_password)) ||
			response != GTK_RESPONSE_OK 
		)
	{
		gtk_widget_destroy (auth_dialog);
		gdk_threads_leave ();
		return FALSE;
	}

	picasa_client->auth_token = NULL;
	picasa_client->username = g_strdup(gtk_entry_get_text(GTK_ENTRY(input_username)));
	picasa_client->password = g_strdup(gtk_entry_get_text(GTK_ENTRY(input_password)));

    gtk_widget_destroy (auth_dialog);
    gdk_threads_leave ();
	return TRUE;
}

gboolean
rs_picasa_client_auth(PicasaClient *picasa_client)
{
	/* Already authenticated? */
	if (picasa_client->username && picasa_client->auth_token != NULL)
		return TRUE;

	/* do we have enough information? */
	if (picasa_client->username == NULL || picasa_client->password == NULL)
		return FALSE;

	GString *data = g_string_new(NULL);
        struct curl_slist *header = NULL;

	GString *post_str = g_string_new(NULL);
	g_string_printf(post_str, "accountType=GOOGLE&Email=%s&Passwd=%s&service=lh2&source=Rawstudio", picasa_client->username, picasa_client->password);
	g_free(picasa_client->password);

	header = curl_slist_append(header, "Content-Type: application/x-www-form-urlencoded");

        curl_easy_reset(picasa_client->curl);
        curl_easy_setopt(picasa_client->curl, CURLOPT_URL, PICASA_LOGIN_URL);
        curl_easy_setopt(picasa_client->curl, CURLOPT_POST, TRUE);
	curl_easy_setopt(picasa_client->curl, CURLOPT_POSTFIELDS, post_str->str);
        curl_easy_setopt(picasa_client->curl, CURLOPT_POSTFIELDSIZE, post_str->len);
        curl_easy_setopt(picasa_client->curl, CURLOPT_WRITEFUNCTION, write_callback);
	curl_easy_setopt(picasa_client->curl, CURLOPT_WRITEDATA, data);
        curl_easy_setopt(picasa_client->curl, CURLOPT_HTTPHEADER, header);

#ifdef CURL_DEBUG
        curl_easy_setopt(picasa_client->curl, CURLOPT_VERBOSE, TRUE);
#endif

        CURLcode result = curl_easy_perform(picasa_client->curl);
	handle_curl_code(result);
	
	/* To read values as GKeyFile we need a group */
	data = g_string_prepend(data, "[PICASA]\n");

	GKeyFile *kf = g_key_file_new();
	g_key_file_load_from_data(kf, data->str, data->len, G_KEY_FILE_NONE, NULL);

	picasa_client->captcha_token = g_key_file_get_value(kf, "PICASA", "CaptchaToken", NULL);
	picasa_client->captcha_url = g_key_file_get_value(kf, "PICASA", "CaptchaUrl", NULL);

	if (picasa_client->captcha_token && picasa_client->captcha_url)
	{
		g_warning("Capcha required and not implemented yet..sorry :(");
		// FIXME: fetch captcha and let user re-authenticate - call this function again.
		g_free(picasa_client->captcha_token);
		g_free(picasa_client->captcha_url);
		return FALSE;
	}
	else
	{
		picasa_client->auth_token = g_key_file_get_value(kf, "PICASA", "Auth", NULL);
	}

	g_string_free(data, TRUE);
	g_string_free(post_str, TRUE);
	curl_slist_free_all(header);
	if (NULL == picasa_client->auth_token)
		return FALSE;

	return TRUE;
}

GtkListStore *
rs_picasa_client_get_album_list(PicasaClient *picasa_client, GError **error)
{
	gint ret;
	g_assert(picasa_client->auth_token != NULL);
	g_assert(picasa_client->username != NULL);

	GString *data = g_string_new(NULL);
	struct curl_slist *header = NULL;

	GString *url = g_string_new(NULL);
	g_string_printf(url, "%s/user/%s", PICASA_DATA_URL, picasa_client->username);

	GString *auth_string = g_string_new("Authorization: GoogleLogin auth=");
	auth_string = g_string_append(auth_string, picasa_client->auth_token);
	header = curl_slist_append(header, auth_string->str);

        curl_easy_reset(picasa_client->curl);
        curl_easy_setopt(picasa_client->curl, CURLOPT_URL, url->str);
	curl_easy_setopt(picasa_client->curl, CURLOPT_WRITEFUNCTION, write_callback);
	curl_easy_setopt(picasa_client->curl, CURLOPT_WRITEDATA, data);
        curl_easy_setopt(picasa_client->curl, CURLOPT_HTTPHEADER, header);

#ifdef CURL_DEBUG
        curl_easy_setopt(picasa_client->curl, CURLOPT_VERBOSE, TRUE);
#endif

        CURLcode result = curl_easy_perform(picasa_client->curl);
	handle_curl_code(result);

	glong response_code;
	curl_easy_getinfo(picasa_client->curl, CURLINFO_RESPONSE_CODE, &response_code);
	ret = picasa_error(picasa_client, response_code, data, error);
	if (PICASA_CLIENT_OK == ret)
		return xml_album_list_response(data);
	else if (PICASA_CLIENT_RETRY == ret)
		return rs_picasa_client_get_album_list(picasa_client, error);
	return NULL;
}

gchar *
rs_picasa_client_create_album(PicasaClient *picasa_client, const gchar *name, GError **error)
{
	gint ret;
	gchar *body = g_strdup_printf("<entry xmlns='http://www.w3.org/2005/Atom' xmlns:media='http://search.yahoo.com/mrss/' xmlns:gphoto='http://schemas.google.com/photos/2007'> <title type='text'>%s</title><summary type='text'></summary><gphoto:location></gphoto:location><gphoto:access>private</gphoto:access><gphoto:commentingEnabled>true</gphoto:commentingEnabled><gphoto:timestamp>%d000</gphoto:timestamp><category scheme='http://schemas.google.com/g/2005#kind' term='http://schemas.google.com/photos/2007#album'></category></entry>", name, (int) time(NULL));

	g_assert(picasa_client->auth_token != NULL);
	g_assert(picasa_client->username != NULL);

	GString *data = g_string_new(NULL);
	struct curl_slist *header = NULL;

	GString *url = g_string_new(NULL);
	g_string_printf(url, "%s/user/%s", PICASA_DATA_URL, picasa_client->username);

	GString *auth_string = g_string_new("Authorization: GoogleLogin auth=");
	auth_string = g_string_append(auth_string, picasa_client->auth_token);
	header = curl_slist_append(header, auth_string->str);
	header = curl_slist_append(header, "Content-Type: application/atom+xml");

        curl_easy_reset(picasa_client->curl);
        curl_easy_setopt(picasa_client->curl, CURLOPT_URL, url->str);
	curl_easy_setopt(picasa_client->curl, CURLOPT_WRITEFUNCTION, write_callback);
	curl_easy_setopt(picasa_client->curl, CURLOPT_WRITEDATA, data);
        curl_easy_setopt(picasa_client->curl, CURLOPT_HTTPHEADER, header);
        curl_easy_setopt(picasa_client->curl, CURLOPT_POST, TRUE);
	curl_easy_setopt(picasa_client->curl, CURLOPT_POSTFIELDS, body);
        curl_easy_setopt(picasa_client->curl, CURLOPT_POSTFIELDSIZE, strlen(body));

#ifdef CURL_DEBUG
        curl_easy_setopt(picasa_client->curl, CURLOPT_VERBOSE, TRUE);
#endif

	CURLcode result = curl_easy_perform(picasa_client->curl);
	handle_curl_code(result);

	glong response_code;
	curl_easy_getinfo(picasa_client->curl, CURLINFO_RESPONSE_CODE, &response_code);
	ret = picasa_error(picasa_client, response_code, data, error);
	if (PICASA_CLIENT_OK == ret)
		return xml_album_create_response(data);
	else if (PICASA_CLIENT_RETRY == ret)
		return rs_picasa_client_create_album(picasa_client, name, error);
	return NULL;
}

gboolean
rs_picasa_client_upload_photo(PicasaClient *picasa_client, gchar *photo, gchar *albumid, GError **error)
{
	gint ret;
	g_assert(picasa_client->auth_token != NULL);
	g_assert(picasa_client->username != NULL);

	GString *data = g_string_new(NULL);
	struct curl_slist *header = NULL;

	GString *url = g_string_new(NULL);
	g_string_printf(url, "%s/user/%s/albumid/%s", PICASA_DATA_URL, picasa_client->username, albumid);

	GString *auth_string = g_string_new("Authorization: GoogleLogin auth=");
	auth_string = g_string_append(auth_string, picasa_client->auth_token);

	gchar *contents;
	gsize length;
	g_file_get_contents(photo, &contents, &length, NULL);

	header = curl_slist_append(header, auth_string->str);
	header = curl_slist_append(header, "Content-Type: image/jpeg");

        curl_easy_reset(picasa_client->curl);
        curl_easy_setopt(picasa_client->curl, CURLOPT_URL, url->str);
        curl_easy_setopt(picasa_client->curl, CURLOPT_HTTPHEADER, header);
        curl_easy_setopt(picasa_client->curl, CURLOPT_POST, TRUE);
	curl_easy_setopt(picasa_client->curl, CURLOPT_POSTFIELDS, contents);
        curl_easy_setopt(picasa_client->curl, CURLOPT_POSTFIELDSIZE, (gint) length);
	curl_easy_setopt(picasa_client->curl, CURLOPT_WRITEFUNCTION, write_callback);
	curl_easy_setopt(picasa_client->curl, CURLOPT_WRITEDATA, data);

#ifdef CURL_DEBUG
        curl_easy_setopt(picasa_client->curl, CURLOPT_VERBOSE, TRUE);
#endif

        CURLcode result = curl_easy_perform(picasa_client->curl);
	handle_curl_code(result);

	glong response_code;
	curl_easy_getinfo(picasa_client->curl, CURLINFO_RESPONSE_CODE, &response_code);

	ret = picasa_error(picasa_client, response_code, data, error);
	if (PICASA_CLIENT_OK == ret)
		return TRUE;
	else if (PICASA_CLIENT_RETRY == ret)
		return rs_picasa_client_upload_photo(picasa_client, photo, albumid, error);

	return FALSE;
}

PicasaClient *
rs_picasa_client_init()
{
	PicasaClient *picasa_client = g_malloc0(sizeof(PicasaClient));
	picasa_client->curl = curl_easy_init();

	picasa_client->auth_token = rs_conf_get_string(CONF_PICASA_CLIENT_AUTH_TOKEN);
	picasa_client->username = rs_conf_get_string(CONF_PICASA_CLIENT_USERNAME);

	while (!rs_picasa_client_auth(picasa_client))
	{
		if (!rs_picasa_client_auth_popup(picasa_client))
		{
			/* Cancel pressed, or no info entered */
			return NULL;
		}
	}
	/* Save information */
	rs_conf_set_string(CONF_PICASA_CLIENT_AUTH_TOKEN, picasa_client->auth_token);
	rs_conf_set_string(CONF_PICASA_CLIENT_USERNAME, picasa_client->username);

	return picasa_client;
}
