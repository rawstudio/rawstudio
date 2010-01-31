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

#ifndef RS_FACEBOOK_CLIENT_H
#define RS_FACEBOOK_CLIENT_H

#include <glib-object.h>
#include <gtk/gtk.h>
#include "rs-facebook-client-param.h"

G_BEGIN_DECLS

/**
 * Get a quark used to describe Facebook error domain
 * @return A quark touse in GErrors from Facebook code
 */
GQuark
rs_facebook_client_error_quark(void);

#define RS_FACEBOOK_CLIENT_ERROR_DOMAIN rs_facebook_client_error_quark()

#define RS_TYPE_FACEBOOK_CLIENT rs_facebook_client_get_type()
#define RS_FACEBOOK_CLIENT(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), RS_TYPE_FACEBOOK_CLIENT, RSFacebookClient))
#define RS_FACEBOOK_CLIENT_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), RS_TYPE_FACEBOOK_CLIENT, RSFacebookClientClass))
#define RS_IS_FACEBOOK_CLIENT(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), RS_TYPE_FACEBOOK_CLIENT))
#define RS_IS_FACEBOOK_CLIENT_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), RS_TYPE_FACEBOOK_CLIENT))
#define RS_FACEBOOK_CLIENT_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), RS_TYPE_FACEBOOK_CLIENT, RSFacebookClientClass))

typedef struct _RSFacebookClient RSFacebookClient;

typedef struct {
	GObjectClass parent_class;
} RSFacebookClientClass;

GType rs_facebook_client_get_type(void);

/**
 * Initializes a new RSFacebookClient
 * @param api_key The API key from Facebook
 * @param secret The secret provided by Facebook
 * @param session_key The stored session key or NULL if you haven't got one yet
 * @return A new RSFacebookClient, this must be unreffed
 */
RSFacebookClient *
rs_facebook_client_new(const gchar *api_key, const gchar *secret, const gchar *session_key);

/**
 * Get the url that the user must visit to authenticate this application (api_key)
 * @param facebook A RSFacebookClient
 * @param base_url A prefix URL, "http://api.facebook.com/login.php" would make sense
 * @param error NULL or a pointer to a GError * initialized to NULL
 * @return A URL that the user can visit to authenticate this application. Thisshould not be freed
 */
const gchar *
rs_facebook_client_get_auth_url(RSFacebookClient *facebook, const gchar *base_url, GError **error);

/**
 * Get the session key as returned from Facebook
 * @param facebook A RSFacebookClient
 * @param error NULL or a pointer to a GError * initialized to NULL
 * @return The session key from Facebook or NULL on error
 */
const gchar *
rs_facebook_client_get_session_key(RSFacebookClient *facebook, GError **error);

/**
 * Set the session key, this can be used to cache the session_key
 * @param facebook A RSFacebookClient
 * @param session_key A new session key to use
 */
void
rs_facebook_client_set_session_key(RSFacebookClient *facebook, const gchar *session_key);

/**
 * Check if we are authenticated to Facebook
 * @param facebook A RSFacebookClient
 * @param error NULL or a pointer to a GError * initialized to NULL
 * @return TRUE if we're authenticated both by the Facebook API and by the end-user, FALSE otherwise
 */
gboolean
rs_facebook_client_ping(RSFacebookClient *facebook, GError **error);

/**
 * Upload a photo to Facebook, will be placed in the registered applications default photo folder
 * @param facebook A RSFacebookClient
 * @param filename Full path to an image to upload. JPEG, PNG, TIFF accepted
 * @param caption The caption to use for the image
 * @param error NULL or a pointer to a GError * initialized to NULL
 * @return TRUE on success, FALSE otherwise
 */
gboolean
rs_facebook_client_upload_image(RSFacebookClient *facebook, const gchar *filename, const gchar *caption, const gchar *aid, GError **error);

/**
 * Get list of available albums on Facebook account (not profile, wall and so on)
 * @param facebook A RSFacebookClient
 * @param error NULL or a pointer to a GError * initialized to NULL
 * @return a GtkListStore with albums if any, NULL otherwise
 */
GtkListStore *
rs_facebook_client_get_album_list(RSFacebookClient *facebook, GError **error);

gchar *
rs_facebook_client_create_album(RSFacebookClient *facebook, const gchar *album_name);

G_END_DECLS

#endif /* RS_FACEBOOK_CLIENT_H */
