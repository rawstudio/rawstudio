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

#include <string.h>
#include <curl/curl.h>
#include <glib-2.0/glib.h>
#include <libxml/encoding.h>
#include "facebook.h"

static facebook *fb = NULL;

gboolean request(gchar *method, GList *params, GString *result);
static gint sort_alphabetical(gconstpointer a, gconstpointer b);
GString *get_param_string(GList *params, gboolean separate);
gchar *get_signature(GList *params);
size_t write_callback(void *ptr, size_t size, size_t nmemb, void *userp);
gboolean xml_error(gchar *xml, gint length);
gchar *parse_xml_response(gchar *xml, gint length, gchar *key, gboolean root);

gboolean
request(gchar *method, GList *params, GString *result)
{
	curl_easy_reset(fb->curl);

#ifdef fb_debug
	curl_easy_setopt(fb->curl, CURLOPT_VERBOSE, TRUE);
#endif

        curl_easy_setopt(fb->curl, CURLOPT_URL, fb->server);

	params = g_list_append(params, g_strdup_printf("api_key=%s", fb->api_key));
	params = g_list_append(params, g_strdup_printf("method=%s", method));
	params = g_list_append(params, g_strdup_printf("v=1.0"));

	if(fb->session_key)
		params = g_list_append(params, g_strdup_printf("session_key=%s", fb->session_key));

	params = g_list_sort(params, sort_alphabetical);	

	params = g_list_append(params, g_strdup_printf("sig=%s",get_signature(params)));

	GString *query = get_param_string(params, TRUE);

	struct curl_slist *header = NULL;
	header = curl_slist_append(header, "Content-Type: multipart/form-data; boundary=boundary");
	header = curl_slist_append(header, "MIME-version: 1.0;");

        curl_easy_setopt(fb->curl, CURLOPT_POST, TRUE);
	curl_easy_setopt(fb->curl, CURLOPT_POSTFIELDS, query->str);
	curl_easy_setopt(fb->curl, CURLOPT_POSTFIELDSIZE, query->len);
	curl_easy_setopt(fb->curl, CURLOPT_WRITEFUNCTION, write_callback);
	curl_easy_setopt(fb->curl, CURLOPT_WRITEDATA, result);
	curl_easy_setopt(fb->curl, CURLOPT_HTTPHEADER, header);
	fb->res = curl_easy_perform(fb->curl);
	fb->call_id++;
	return (fb->res == 0);
}

static gint
sort_alphabetical(gconstpointer a, gconstpointer b)
{
	gchar *str1 = (gchar *) a;
	gchar *str2 = (gchar *) b;

	return g_strcmp0(str1, str2);
}

GString *
get_param_string(GList *params, gboolean separate)
{
	GString *str = g_string_new("");
	GString *image = NULL;

	gint i;

	for (i = 0; i < g_list_length(params); i++)
	{
		if (separate) 
		{
			gchar **split = g_strsplit(g_list_nth_data(params, i), "=", 0);
			if(g_strcmp0(split[0], "filename") == 0)
			{
				gchar *contents;
				gsize length;
				if (g_file_get_contents(split[1], &contents, &length, NULL))
				{
					
					image = g_string_new("--boundary\r\n");
					g_string_append_printf(image, "Content-Disposition: form-data; filename=%s\r\n", split[1]);
					g_string_append_printf(image, "Content-Type: image/jpg\r\n\r\n");
					image = g_string_append_len(image, contents, length);
					g_string_append_printf(image, "\r\n--boundary\r\n");
				}
			}
			g_string_append_printf(str, "--%s\r\nContent-Disposition: form-data; name=\"%s\"\r\n\r\n%s\r\n", "boundary", split[0], split[1]);
		} else {
			str = g_string_append(str, g_list_nth_data(params, i));
		}
	}

	if (image)
		str = g_string_append_len(str, image->str, image->len);

	return str;
}

gchar *
get_signature(GList *params)
{
	GString *str = get_param_string(params, FALSE);
	str = g_string_append(str, fb->secret);
	gchar *signature = g_compute_checksum_for_string(G_CHECKSUM_MD5, str->str, strlen(str->str));
	g_string_free(str, TRUE);
	return signature;
}

size_t
write_callback(void *ptr, size_t size, size_t nmemb, void *userp)
{
	GString *string = (GString *) userp;
	string = g_string_append_len(string, (char *) ptr, size * nmemb);
	return (size * nmemb);
}

gboolean
xml_error(gchar *xml, gint length)
{
	gchar *error_code = parse_xml_response(xml, length, "error_code", FALSE);
	gchar *error_msg = parse_xml_response(xml, length, "error_msg", FALSE);

	if (error_code)
	{
		printf("error: %s\n", error_msg);
		g_free(error_code);
		g_free(error_msg);
		return TRUE;
	}
	g_free(error_code);
	g_free(error_msg);
	return FALSE;
}

gchar *
parse_xml_response(gchar *xml, gint length, gchar *key, gboolean root)
{
	xmlDocPtr doc = xmlParseMemory(xml, length);
        xmlNodePtr cur;

	cur = xmlDocGetRootElement(doc);

	if (!root)
		cur = cur->xmlChildrenNode;

	gchar *result = NULL;

	while (cur)
	{
		if ((!xmlStrcmp(cur->name, BAD_CAST(key))))
			result = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);

		cur = cur->next;
	}
	return result;
}

/* BEGIN PUBLIC FUNCTIONS */

gboolean
facebook_upload_photo(const gchar *filename, const char *caption)
{
	GList *params = NULL;
	GString *xml = g_string_new("");

	params = g_list_append(params, g_strdup_printf("filename=%s", filename));
	params = g_list_append(params, g_strdup_printf("caption=%s", caption));

	if (!request("facebook.Photos.upload", params, xml))
		return FALSE;

	if (g_utf8_strlen(xml->str, 1048576) == 0)
		return FALSE;

	gboolean error = xml_error(xml->str, strlen(xml->str));
	if (error)
		return FALSE;

	g_string_free(xml, TRUE);
	return TRUE;
}


gboolean
facebook_init(gchar *my_key, gchar *my_secret, gchar *my_server)
{
	fb = g_malloc(sizeof(facebook));
	fb->api_key = my_key;
	fb->secret = my_secret;
	fb->server = my_server;
	fb->call_id = 0;
	fb->token = NULL;
	fb->session_key = NULL;

	fb->curl = curl_easy_init();
	if(!fb->curl)
	{
		g_error("Could not initialize curl.");
		return FALSE;
	}
	return TRUE;
}

gboolean
facebook_get_token()
{
	GList *params = NULL;
	GString *xml = g_string_new("");

	if (!request("facebook.auth.createToken", params, xml))
		return FALSE;

	if (g_utf8_strlen(xml->str, 1048576) == 0)
		return FALSE;

	/* Check for errors */
	gboolean error = xml_error(xml->str, strlen(xml->str));
	if (error)
		return FALSE;

	/* Get auth token */
	fb->token = parse_xml_response(xml->str, strlen(xml->str), "auth_createToken_response", TRUE);

	if (!fb->token)
		return FALSE;

	return TRUE;
}

gchar *
facebook_get_auth_url(gchar *url)
{
	GString *str = g_string_new(url);
	str = g_string_append(str, "?api_key=");
	str = g_string_append(str, fb->api_key);
	str = g_string_append(str, "&auth_token=");
	str = g_string_append(str, fb->token);

	gchar *ret = str->str;
	g_string_free(str, FALSE);

	return ret;
}

void
facebook_set_session(gchar *session)
{
	fb->session_key = session;
}

gchar *
facebook_get_session()
{
	GList *params = NULL;
	GString *xml = g_string_new("");

	params = g_list_append(params, g_strdup_printf("auth_token=%s", fb->token));	

	if (!request("facebook.auth.getSession", params, xml))
		return NULL;

	if (g_utf8_strlen(xml->str, 1048576) == 0)
		return NULL;

	/* Check for errors */
	gboolean error = xml_error(xml->str, strlen(xml->str));
	if (error)
		return NULL;

	/* Get session_key */
	fb->session_key = parse_xml_response(xml->str, strlen(xml->str), "session_key", FALSE);
	g_string_free(xml, TRUE);

	if (!fb->session_key)
		return NULL;

	return fb->session_key;
}

void
facebook_close()
{
	curl_easy_cleanup(fb->curl);
	g_free(fb);
}

/* END PUBLIC FUNCTIONS */
