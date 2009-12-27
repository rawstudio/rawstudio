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

#include <curl/curl.h>
#include <libxml/encoding.h>
#include "rs-facebook-client.h"

#define HTTP_BOUNDARY "4wncn84cq4ncto874ytnv90w43htn"

/**
 * Get a quark used to describe Facebook error domain
 * @return A quark touse in GErrors from Facebook code
 */
GQuark
rs_facebook_client_error_quark(void)
{
	static GStaticMutex lock = G_STATIC_MUTEX_INIT;
	static GQuark quark;

	g_static_mutex_lock(&lock);
	if (!quark)
		quark = g_quark_from_static_string("rawstudio_facebook_client_error");
	g_static_mutex_unlock(&lock);

	return quark;
}

struct _RSFacebookClient {
	GObject parent;

	const gchar *api_key;
	const gchar *secret;

	gchar *session_key;
	gchar *auth_token;
	gchar *auth_url;

	CURL *curl;
};

G_DEFINE_TYPE(RSFacebookClient, rs_facebook_client, G_TYPE_OBJECT)

static void
rs_facebook_client_finalize(GObject *object)
{
	RSFacebookClient *facebook = RS_FACEBOOK_CLIENT(object);

	g_free(facebook->session_key);
	g_free(facebook->auth_token);
	g_free(facebook->auth_url);

	curl_easy_cleanup(facebook->curl);

	G_OBJECT_CLASS(rs_facebook_client_parent_class)->finalize(object);
}

static void
rs_facebook_client_class_init(RSFacebookClientClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS(klass);

	object_class->finalize = rs_facebook_client_finalize;
}

static void
rs_facebook_client_init(RSFacebookClient *facebook)
{
	facebook->curl = curl_easy_init();
}

static gchar *
xml_simple_response(const GString *xml, const gchar *needle, const gboolean root)
{
	xmlDocPtr doc = xmlParseMemory(xml->str, xml->len);
	xmlNodePtr cur;

	cur = xmlDocGetRootElement(doc);

	if (!root)
		cur = cur->xmlChildrenNode;

	gchar *result = NULL;

	while (cur)
	{
		if ((!xmlStrcmp(cur->name, BAD_CAST(needle))))
			result = (gchar *) xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);

		cur = cur->next;
	}
	return result;
}

static gboolean
xml_error(const GString *xml, GError **error)
{
	gchar *error_code = xml_simple_response(xml, "error_code", FALSE);
	gchar *error_msg = xml_simple_response(xml, "error_msg", FALSE);

	if (error_code)
	{
		g_set_error(error, RS_FACEBOOK_CLIENT_ERROR_DOMAIN, 0, "%s", error_msg);
		g_free(error_code);
		g_free(error_msg);
		return TRUE;
	}
	g_free(error_code);
	g_free(error_msg);
	return FALSE;
}

static size_t
write_callback(void *ptr, size_t size, size_t nmemb, void *userp)
{
    GString *string = (GString *) userp;
    g_string_append_len(string, (char *) ptr, size * nmemb);
    return (size * nmemb);
}

static gboolean
facebook_client_request(RSFacebookClient *facebook, const gchar *method, RSFacebookClientParam *param, GString *content, GError **error)
{
	volatile static gint call_id = 0;
	CURLcode result;
	struct curl_slist *header = NULL;
	gint post_length = 0;
	gchar *post_str;

	/* We start by resetting all CURL parameters */
	curl_easy_reset(facebook->curl);

#ifdef fb_debug
	curl_easy_setopt(facebook->curl, CURLOPT_VERBOSE, TRUE);
#endif /* fb_debug */

	g_atomic_int_inc(&call_id);

	curl_easy_setopt(facebook->curl, CURLOPT_URL, "api.facebook.com/restserver.php");
	rs_facebook_client_param_add_string(param, "api_key", facebook->api_key);
	rs_facebook_client_param_add_string(param, "method", method);
	rs_facebook_client_param_add_string(param, "v", "1.0");
	rs_facebook_client_param_add_integer(param, "call_id", g_atomic_int_get(&call_id));

	/* If we have a session key, we will use it */
	if(facebook->session_key)
		rs_facebook_client_param_add_string(param, "session_key", facebook->session_key);

	header = curl_slist_append(header, "Content-Type: multipart/form-data; boundary=" HTTP_BOUNDARY);
	header = curl_slist_append(header, "MIME-version: 1.0;");

	post_str = rs_facebook_client_param_get_post(param, facebook->secret, HTTP_BOUNDARY, &post_length);

	curl_easy_setopt(facebook->curl, CURLOPT_POST, TRUE);
	curl_easy_setopt(facebook->curl, CURLOPT_POSTFIELDS, post_str);
	curl_easy_setopt(facebook->curl, CURLOPT_POSTFIELDSIZE, post_length);
	curl_easy_setopt(facebook->curl, CURLOPT_WRITEFUNCTION, write_callback);
	curl_easy_setopt(facebook->curl, CURLOPT_WRITEDATA, content);
	curl_easy_setopt(facebook->curl, CURLOPT_HTTPHEADER, header);
	result = curl_easy_perform(facebook->curl);

	curl_slist_free_all(header);
	g_free(post_str);
	g_object_unref(param);

	if (xml_error(content, error))
		return FALSE;

	return (result==0);
}

static const gchar *
facebook_client_get_auth_token(RSFacebookClient *facebook, GError **error)
{
	static GStaticMutex lock = G_STATIC_MUTEX_INIT;

	g_static_mutex_lock(&lock);
	if (!facebook->auth_token)
	{
		GString *content = g_string_new("");
		facebook_client_request(facebook, "facebook.auth.createToken", rs_facebook_client_param_new(), content, error);
		facebook->auth_token = xml_simple_response(content, "auth_createToken_response", TRUE);
		g_string_free(content, TRUE);
	}
	g_static_mutex_unlock(&lock);

	return facebook->auth_token;
}

/**
 * Initializes a new RSFacebookClient
 * @param api_key The API key from Facebook
 * @param secret The secret provided by Facebook
 * @param session_key The stored session key or NULL if you haven't got one yet
 * @return A new RSFacebookClient, this must be unreffed
 */
RSFacebookClient *
rs_facebook_client_new(const gchar *api_key, const gchar *secret, const gchar *session_key)
{
	RSFacebookClient *facebook = g_object_new(RS_TYPE_FACEBOOK_CLIENT, NULL);

	facebook->api_key = api_key;
	facebook->secret = secret;

	rs_facebook_client_set_session_key(facebook, session_key);

	return facebook;
}


/**
 * Get the url that the user must visit to authenticate this application (api_key)
 * @param facebook A RSFacebookClient
 * @param base_url A prefix URL, "http://api.facebook.com/login.php" would make sense
 * @param error NULL or a pointer to a GError * initialized to NULL
 * @return A URL that the user can visit to authenticate this application. Thisshould not be freed
 */
const gchar *
rs_facebook_client_get_auth_url(RSFacebookClient *facebook, const gchar *base_url, GError **error)
{
	static GStaticMutex lock = G_STATIC_MUTEX_INIT;

	g_assert(RS_IS_FACEBOOK_CLIENT(facebook));

	g_static_mutex_lock(&lock);
	if (!facebook->auth_url)
		facebook->auth_url = g_strdup_printf("%s?api_key=%s&auth_token=%s", base_url, facebook->api_key, facebook_client_get_auth_token(facebook, error));
	g_static_mutex_unlock(&lock);

	return facebook->auth_url;
}

/**
 * Get the session key as returned from Facebook
 * @param facebook A RSFacebookClient
 * @param error NULL or a pointer to a GError * initialized to NULL
 * @return The session key from Facebook or NULL on error
 */
const gchar *
rs_facebook_client_get_session_key(RSFacebookClient *facebook, GError **error)
{
	static GStaticMutex lock = G_STATIC_MUTEX_INIT;

	g_assert(RS_IS_FACEBOOK_CLIENT(facebook));

	g_static_mutex_lock(&lock);
	RSFacebookClientParam *param = rs_facebook_client_param_new();

	rs_facebook_client_param_add_string(param, "auth_token", facebook->auth_token);
	GString *content = g_string_new("");
	facebook_client_request(facebook, "facebook.auth.getSession", param, content, error);

	g_free(facebook->session_key);
	facebook->session_key = xml_simple_response(content, "session_key", FALSE);
	g_string_free(content, TRUE);
	g_static_mutex_unlock(&lock);

	return facebook->session_key;
}

/**
 * Set the session key, this can be used to cache the session_key
 * @param facebook A RSFacebookClient
 * @param session_key A new session key to use
 */
void
rs_facebook_client_set_session_key(RSFacebookClient *facebook, const gchar *session_key)
{
	g_assert(RS_IS_FACEBOOK_CLIENT(facebook));

	g_free(facebook->session_key);

	facebook->session_key = g_strdup(session_key);
}

/**
 * Check if we are authenticated to Facebook
 * @param facebook A RSFacebookClient
 * @param error NULL or a pointer to a GError * initialized to NULL
 * @return TRUE if we're authenticated both by the Facebook API and by the end-user, FALSE otherwise
 */
gboolean
rs_facebook_client_ping(RSFacebookClient *facebook, GError **error)
{
	gboolean ret = FALSE;
	g_assert(RS_IS_FACEBOOK_CLIENT(facebook));

	GString *content = g_string_new("");
	facebook_client_request(facebook, "facebook.users.isAppAdded", rs_facebook_client_param_new(), content, NULL);
	gchar *result = xml_simple_response(content, "users_isAppAdded_response", TRUE);
	g_string_free(content, TRUE);

	if (result && g_str_equal(result, "1"))
		ret = TRUE;

	g_free(result);

	return ret;
}

/**
 * Upload a photo to Facebook, will be placed in the registered applications default photo folder
 * @param facebook A RSFacebookClient
 * @param filename Full path to an image to upload. JPEG, PNG, TIFF accepted
 * @param caption The caption to use for the image
 * @param error NULL or a pointer to a GError * initialized to NULL
 * @return TRUE on success, FALSE otherwise
 */
gboolean
rs_facebook_client_upload_image(RSFacebookClient *facebook, const gchar *filename, const gchar *caption, const gchar *aid, GError **error)
{
	g_assert(RS_IS_FACEBOOK_CLIENT(facebook));
	g_return_val_if_fail(filename != NULL, FALSE);
	g_return_val_if_fail(g_path_is_absolute(filename), FALSE);

	RSFacebookClientParam *param = rs_facebook_client_param_new();

	rs_facebook_client_param_add_string(param, "filename", filename);
	if (caption)
		rs_facebook_client_param_add_string(param, "caption", caption);
	if (aid)
		rs_facebook_client_param_add_string(param, "aid", aid);

	GString *content = g_string_new("");
	facebook_client_request(facebook, "facebook.photos.upload", param, content, error);

	g_string_free(content, TRUE);

	return TRUE;
}
