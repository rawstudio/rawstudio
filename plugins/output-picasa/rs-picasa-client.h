#ifndef RS_PICASA_CLIENT_H
#define RS_PICASA_CLIENT_H

#include <glib.h>
#include <curl/curl.h>

typedef struct {
	CURL *curl;
	gchar *username;
	gchar *password;
	gchar *auth_token;
	gchar *captcha_token;
	gchar *captcha_url;
} PicasaClient;

gboolean rs_picasa_client_auth_popup(PicasaClient *picasa_client);
void rs_picasa_client_auth(PicasaClient *picasa_client);
GtkListStore * rs_picasa_client_get_album_list(PicasaClient *picasa_client);
char * rs_picasa_client_create_album(PicasaClient *picasa_client, const gchar *name);
gboolean rs_picasa_client_upload_photo(PicasaClient *picasa_client, gchar *photo, gchar *albumid);
PicasaClient * rs_picasa_client_init();

#endif /* RS_PICASA_CLIENT_H */
