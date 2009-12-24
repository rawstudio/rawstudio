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

#ifndef FACEBOOK_H
#define FACEBOOK_H

#include <glib-2.0/glib.h>
#include <curl/curl.h>

typedef struct {
        gchar *api_key;
	gchar *secret;
	gchar *token;
	gchar *server;
	gchar *session_key;

	/* curl */
	CURL *curl;
	CURLcode res;
	gint call_id;
} facebook;

gboolean facebook_upload_photo(const gchar *filename, const char *caption);
gboolean facebook_init(gchar *my_key, gchar *my_secret, gchar *my_server);
gboolean facebook_get_token();
gchar * facebook_get_auth_url(gchar *url);
gboolean facebook_get_session();
void facebook_close();

#endif /* FACEBOOK_H */
