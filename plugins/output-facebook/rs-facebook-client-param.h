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

#ifndef RS_FACEBOOK_CLIENT_PARAM_H
#define RS_FACEBOOK_CLIENT_PARAM_H

#include <glib-object.h>

G_BEGIN_DECLS

#define RS_TYPE_FACEBOOK_CLIENT_PARAM rs_facebook_client_param_get_type()
#define RS_FACEBOOK_CLIENT_PARAM(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), RS_TYPE_FACEBOOK_CLIENT_PARAM, RSFacebookClientParam))
#define RS_FACEBOOK_CLIENT_PARAM_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), RS_TYPE_FACEBOOK_CLIENT_PARAM, RSFacebookClientParamClass))
#define RS_IS_FACEBOOK_CLIENT_PARAM(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), RS_TYPE_FACEBOOK_CLIENT_PARAM))
#define RS_IS_FACEBOOK_CLIENT_PARAM_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), RS_TYPE_FACEBOOK_CLIENT_PARAM))
#define RS_FACEBOOK_CLIENT_PARAM_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), RS_TYPE_FACEBOOK_CLIENT_PARAM, RSFacebookClientParamClass))

typedef struct {
	GObject parent;

	GList *params;
} RSFacebookClientParam;

typedef struct {
	GObjectClass parent_class;
} RSFacebookClientParamClass;

GType rs_facebook_client_param_get_type(void);

/**
 * Initialize a new RSFacebookClientParam
 * @return Anew RSFacebookClientParam
 */
RSFacebookClientParam *
rs_facebook_client_param_new(void);

/**
 * Add a string argument to a RSFacebookClientParam
 * @param param A RSFacebookClientParam
 * @param name The name of the parameter
 * @param value The value of the parameter
 */
void
rs_facebook_client_param_add_string(RSFacebookClientParam *param, const gchar *name, const gchar *value);

/**
 * Add a string argument to a RSFacebookClientParam
 * @param param A RSFacebookClientParam
 * @param name The name of the parameter
 * @param value The value of the parameter
 */
void
rs_facebook_client_param_add_integer(RSFacebookClientParam *param, const gchar *name, const gint value);

/**
 * Get the complete POST string to use for a Facebook request including signature
 * @param param A RSFacebookClientParam
 * @param secret The secret provided by Facebook
 * @param boundary A string to use as HTTP boundary
 * @param length If non-NULL, will store the length of the post string returned
 * @return A newly allocated POST-string, this must be freed with g_free()
 */
gchar *
rs_facebook_client_param_get_post(RSFacebookClientParam *param, const gchar *secret, const gchar *boundary, gint *length);

G_END_DECLS

#endif /* RS_FACEBOOK_CLIENT_PARAM_H */
